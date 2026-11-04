// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "PythonAPI/UMaterialNodeService.h"
#include "Materials/Material.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionNamedReroute.h"
#include "MaterialEditingLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

#if WITH_AUTOMATION_TESTS

// WI-1672 P1: named-reroute ("graph variable") authoring API on UMaterialNodeService. Every case runs
// against BOTH a UMaterial and a UMaterialFunction, because the service resolves either through the
// asset's own expression list (FVibeMaterialGraphOwner) and the legacy id lookups are Material-only.
//
// EditorContext: in-memory assets registered with FAssetRegistryModule::AssetCreated, never saved.
// Requires a full editor.

namespace
{
	UMaterial* NewScratchMaterial(const FString& PackageName, const FName AssetName)
	{
		UPackage* Package = CreatePackage(*PackageName);
		UMaterial* Material = Package ? NewObject<UMaterial>(Package, AssetName, RF_Public | RF_Standalone | RF_Transactional) : nullptr;
		if (Material)
		{
			FAssetRegistryModule::AssetCreated(Material);
		}
		return Material;
	}

	UMaterialFunction* NewScratchFunction(const FString& PackageName, const FName AssetName)
	{
		UPackage* Package = CreatePackage(*PackageName);
		UMaterialFunction* Function = Package ? NewObject<UMaterialFunction>(Package, AssetName, RF_Public | RF_Standalone | RF_Transactional) : nullptr;
		if (Function)
		{
			FAssetRegistryModule::AssetCreated(Function);
		}
		return Function;
	}

	void DropScratch(UObject* Asset)
	{
		if (Asset)
		{
			FAssetRegistryModule::AssetDeleted(Asset);
			Asset->ClearFlags(RF_Standalone | RF_Public);
		}
	}

	// A scratch Material or MaterialFunction under /Game/__VibeUETest, dropped on destruction.
	struct FScratchGraph
	{
		UMaterial* Material = nullptr;
		UMaterialFunction* Function = nullptr;
		FString Path;

		FScratchGraph(const FString& Name, bool bFunction)
		{
			Path = FString::Printf(TEXT("/Game/__VibeUETest/%s"), *Name);
			if (bFunction)
			{
				Function = NewScratchFunction(Path, FName(*Name));
			}
			else
			{
				Material = NewScratchMaterial(Path, FName(*Name));
			}
		}
		~FScratchGraph() { DropScratch(Material); DropScratch(Function); }

		bool IsValid() const { return Material || Function; }

		// Created with the plain engine library (what editor Python does), NOT through the service:
		// for a function this leaves UMaterialExpression::Function null, which the service must cope with.
		UMaterialExpression* NewExpression(UClass* Class, int32 X = 0, int32 Y = 0) const
		{
			return UMaterialEditingLibrary::CreateMaterialExpressionEx(Material, Function, Class, nullptr, X, Y);
		}

		TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions() const
		{
			return Material ? Material->GetExpressions() : Function->GetExpressions();
		}

		template <typename T>
		TArray<T*> All() const
		{
			TArray<T*> Out;
			for (UMaterialExpression* Expression : Expressions())
			{
				if (T* Typed = Cast<T>(Expression))
				{
					Out.Add(Typed);
				}
			}
			return Out;
		}
	};

	// The session id format GetExpressionId produces ("<Class>_<pointer>").
	FString SessionId(const UMaterialExpression* Expression)
	{
		return FString::Printf(TEXT("%s_%p"), *Expression->GetClass()->GetName(), Expression);
	}
}

// ---------------------------------------------------------------------------------------------
// Declaration + two usages by name; list reports them linked; the graph compiles.
// ---------------------------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeNamedRerouteCreateListTest,
	"VibeUE.MaterialNodeService.NamedReroute.CreateList",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibeNamedRerouteCreateListTest::RunTest(const FString&)
{
	for (const bool bFunction : {false, true})
	{
		const TCHAR* Kind = bFunction ? TEXT("MaterialFunction") : TEXT("Material");
		FScratchGraph Graph(bFunction ? TEXT("MF_NamedRerouteCreate") : TEXT("M_NamedRerouteCreate"), bFunction);
		if (!TestTrue(FString::Printf(TEXT("%s: scratch asset created"), Kind), Graph.IsValid()))
		{
			return false;
		}

		UMaterialExpression* Source = Graph.NewExpression(UMaterialExpressionConstant3Vector::StaticClass(), -600, 0);
		UMaterialExpressionMultiply* Multiply = Cast<UMaterialExpressionMultiply>(Graph.NewExpression(UMaterialExpressionMultiply::StaticClass(), 300, 100));
		if (!TestTrue(FString::Printf(TEXT("%s: source + consumer nodes created"), Kind), Source && Multiply))
		{
			return false;
		}

		const FVibeNamedRerouteInfo Decl = UMaterialNodeService::CreateNamedRerouteDeclaration(
			Graph.Path, TEXT("Accent"), FLinearColor(1.f, 0.5f, 0.f, 1.f), -300, 0, Source->GetPathName(), TEXT(""));
		TestEqual(FString::Printf(TEXT("%s: declaration keeps the requested name"), Kind), Decl.Name, FString(TEXT("Accent")));
		TestTrue(FString::Printf(TEXT("%s: declaration reports the wired source"), Kind), Decl.SourceId == SessionId(Source));
		FGuid ParsedGuid;
		TestTrue(FString::Printf(TEXT("%s: declaration has a VariableGuid"), Kind),
			FGuid::Parse(Decl.VariableGuid, ParsedGuid) && ParsedGuid.IsValid());

		const FString UsageA = UMaterialNodeService::CreateNamedRerouteUsage(Graph.Path, TEXT("Accent"), 0, 0, TEXT(""), TEXT(""));
		const FString UsageB = UMaterialNodeService::CreateNamedRerouteUsage(
			Graph.Path, TEXT("Accent"), 0, 120, Multiply->GetPathName(), TEXT("A"));
		if (!TestTrue(FString::Printf(TEXT("%s: two usages created by declaration name"), Kind), !UsageA.IsEmpty() && !UsageB.IsEmpty() && UsageA != UsageB))
		{
			return false;
		}

		// List: one declaration, both usages linked, nothing broken.
		const FVibeNamedRerouteList List = UMaterialNodeService::ListNamedReroutes(Graph.Path);
		if (!TestEqual(FString::Printf(TEXT("%s: one declaration listed"), Kind), List.Declarations.Num(), 1))
		{
			return false;
		}
		TestEqual(FString::Printf(TEXT("%s: listed name"), Kind), List.Declarations[0].Name, FString(TEXT("Accent")));
		TestEqual(FString::Printf(TEXT("%s: listed VariableGuid matches create result"), Kind), List.Declarations[0].VariableGuid, Decl.VariableGuid);
		TestEqual(FString::Printf(TEXT("%s: both usages linked"), Kind), List.Declarations[0].UsageIds.Num(), 2);
		TestTrue(FString::Printf(TEXT("%s: usage ids returned by create are the listed ones"), Kind),
			List.Declarations[0].UsageIds.Contains(UsageA) && List.Declarations[0].UsageIds.Contains(UsageB));
		TestEqual(FString::Printf(TEXT("%s: no broken usages"), Kind), List.BrokenUsageIds.Num(), 0);

		// Graph level: the links the compiler reads (Declaration pointer) and DeclarationGuid are both set,
		// and the optional wiring into the consumer happened.
		const TArray<UMaterialExpressionNamedRerouteDeclaration*> Decls = Graph.All<UMaterialExpressionNamedRerouteDeclaration>();
		const TArray<UMaterialExpressionNamedRerouteUsage*> Usages = Graph.All<UMaterialExpressionNamedRerouteUsage>();
		if (!TestTrue(FString::Printf(TEXT("%s: 1 declaration + 2 usages in the graph"), Kind), Decls.Num() == 1 && Usages.Num() == 2))
		{
			return false;
		}
		TestTrue(FString::Printf(TEXT("%s: declaration input wired from the source"), Kind), Decls[0]->Input.Expression == Source);
		UMaterialExpressionNamedRerouteUsage* WiredUsage = nullptr;
		for (UMaterialExpressionNamedRerouteUsage* Usage : Usages)
		{
			TestTrue(FString::Printf(TEXT("%s: usage points at the declaration"), Kind), Usage->Declaration == Decls[0]);
			TestTrue(FString::Printf(TEXT("%s: usage DeclarationGuid matches"), Kind), Usage->DeclarationGuid == Decls[0]->VariableGuid);
			if (SessionId(Usage) == UsageB)
			{
				WiredUsage = Usage;
			}
		}
		TestTrue(FString::Printf(TEXT("%s: usage B wired into Multiply.A"), Kind), WiredUsage && Multiply->A.Expression == WiredUsage);

		// Compile. A Material is diagnosed directly; GetMaterialDiagnostics is Material-only, so a function
		// is diagnosed through a scratch Material that calls it.
		UMaterialExpressionNamedRerouteUsage* UsageForOutput = nullptr;
		for (UMaterialExpressionNamedRerouteUsage* Usage : Usages)
		{
			if (SessionId(Usage) == UsageA)
			{
				UsageForOutput = Usage;
			}
		}
		if (!TestNotNull(FString::Printf(TEXT("%s: usage A resolved"), Kind), UsageForOutput))
		{
			return false;
		}

		FString DiagnosedMaterialPath = Graph.Path;
		TUniquePtr<FScratchGraph> Caller;
		if (!bFunction)
		{
			TestTrue(TEXT("Material: usage A -> BaseColor"),
				UMaterialNodeService::ConnectExpressionToOutput(Graph.Path, UsageA, TEXT(""), TEXT("BaseColor")));
			TestTrue(TEXT("Material: Multiply -> EmissiveColor"),
				UMaterialNodeService::ConnectExpressionToOutput(Graph.Path, Multiply->GetPathName(), TEXT(""), TEXT("EmissiveColor")));
		}
		else
		{
			UMaterialExpressionFunctionOutput* Output = Cast<UMaterialExpressionFunctionOutput>(
				Graph.NewExpression(UMaterialExpressionFunctionOutput::StaticClass(), 600, 0));
			if (!TestNotNull(TEXT("MaterialFunction: output node"), Output))
			{
				return false;
			}
			Output->A.Connect(0, UsageForOutput);
			Graph.Function->PostEditChange();

			Caller = MakeUnique<FScratchGraph>(TEXT("M_NamedRerouteCaller"), /*bFunction*/ false);
			if (!TestTrue(TEXT("MaterialFunction: caller material created"), Caller->IsValid()))
			{
				return false;
			}
			const FMaterialExpressionInfo Call = UMaterialNodeService::CreateFunctionCall(Caller->Path, Graph.Path, 0, 0);
			if (!TestFalse(TEXT("MaterialFunction: function call node created"), Call.Id.IsEmpty()))
			{
				return false;
			}
			TestTrue(TEXT("MaterialFunction: call -> BaseColor"),
				UMaterialNodeService::ConnectExpressionToOutput(Caller->Path, Call.Id, TEXT(""), TEXT("BaseColor")));
			DiagnosedMaterialPath = Caller->Path;
		}

		const FMaterialDiagnostics Diag = UMaterialNodeService::GetMaterialDiagnostics(DiagnosedMaterialPath);
		TestTrue(FString::Printf(TEXT("%s: graph with named reroutes compiles clean (%s)"), Kind, *FString::Join(Diag.CompileErrors, TEXT(" | "))),
			Diag.bSuccess && Diag.bIsCompiledOk && Diag.CompileErrors.Num() == 0);

		// Control for the compile check: a usage whose declaration is gone must be reported broken AND must
		// fail to compile, otherwise the assertion above proves nothing.
		if (!bFunction)
		{
			UsageForOutput->Declaration = nullptr;
			Graph.Material->PreEditChange(nullptr); // invalidate the cached compile, as an editor change would
			Graph.Material->PostEditChange();
			const FVibeNamedRerouteList Broken = UMaterialNodeService::ListNamedReroutes(Graph.Path);
			TestEqual(TEXT("Material: usage without declaration is listed as broken"), Broken.BrokenUsageIds.Num(), 1);
			TestEqual(TEXT("Material: the declaration then lists one usage"), Broken.Declarations.Num() == 1 ? Broken.Declarations[0].UsageIds.Num() : -1, 1);
			const FMaterialDiagnostics BrokenDiag = UMaterialNodeService::GetMaterialDiagnostics(Graph.Path);
			TestFalse(TEXT("Material: a broken usage fails the compile check"), BrokenDiag.bIsCompiledOk && BrokenDiag.CompileErrors.Num() == 0);
		}
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// Name clash, unknown names, and the owner check (no leakage between a Material and the function it calls).
// ---------------------------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeNamedRerouteNamesAndOwnerTest,
	"VibeUE.MaterialNodeService.NamedReroute.NamesAndOwner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibeNamedRerouteNamesAndOwnerTest::RunTest(const FString&)
{
	for (const bool bFunction : {false, true})
	{
		const TCHAR* Kind = bFunction ? TEXT("MaterialFunction") : TEXT("Material");
		FScratchGraph Graph(bFunction ? TEXT("MF_NamedRerouteNames") : TEXT("M_NamedRerouteNames"), bFunction);
		if (!TestTrue(FString::Printf(TEXT("%s: scratch asset created"), Kind), Graph.IsValid()))
		{
			return false;
		}

		const FVibeNamedRerouteInfo First = UMaterialNodeService::CreateNamedRerouteDeclaration(
			Graph.Path, TEXT("Tint"), FLinearColor::White, 0, 0, TEXT(""), TEXT(""));
		const FVibeNamedRerouteInfo Second = UMaterialNodeService::CreateNamedRerouteDeclaration(
			Graph.Path, TEXT("Tint"), FLinearColor::White, 0, 100, TEXT(""), TEXT(""));
		TestEqual(FString::Printf(TEXT("%s: first declaration keeps its name"), Kind), First.Name, FString(TEXT("Tint")));
		TestTrue(FString::Printf(TEXT("%s: clashing name is suffixed (got '%s')"), Kind, *Second.Name),
			!Second.Name.IsEmpty() && Second.Name != TEXT("Tint") && Second.Name.StartsWith(TEXT("Tint")));
		TestEqual(FString::Printf(TEXT("%s: two declarations listed"), Kind), UMaterialNodeService::ListNamedReroutes(Graph.Path).Declarations.Num(), 2);

		// The suffixed name is the declaration's key: a usage by that exact name links to the second one.
		const FString UsageId = UMaterialNodeService::CreateNamedRerouteUsage(Graph.Path, Second.Name, 300, 100, TEXT(""), TEXT(""));
		TestFalse(FString::Printf(TEXT("%s: usage by suffixed name created"), Kind), UsageId.IsEmpty());
		for (const FVibeNamedRerouteInfo& Listed : UMaterialNodeService::ListNamedReroutes(Graph.Path).Declarations)
		{
			TestEqual(FString::Printf(TEXT("%s: usage landed on '%s' only"), Kind, *Listed.Name),
				Listed.UsageIds.Num(), Listed.Name == Second.Name ? 1 : 0);
		}

		// Unknown / empty declaration name and empty create name fail and leave the graph alone.
		// Each failure logs a warning, which the automation framework counts as an error unless expected.
		AddExpectedError(TEXT("no named reroute declaration called"), EAutomationExpectedErrorFlags::Contains, 0);
		AddExpectedError(TEXT("declaration name is empty"), EAutomationExpectedErrorFlags::Contains, 0);
		AddExpectedError(TEXT("Name is empty"), EAutomationExpectedErrorFlags::Contains, 0);
		AddExpectedError(TEXT("is not an expression of"), EAutomationExpectedErrorFlags::Contains, 0);
		AddExpectedError(TEXT("is not a material or material function"), EAutomationExpectedErrorFlags::Contains, 0);
		AddExpectedError(TEXT("LoadAsset failed"), EAutomationExpectedErrorFlags::Contains, 0);
		const int32 CountBefore = Graph.Expressions().Num();
		TestTrue(FString::Printf(TEXT("%s: usage by unknown name fails"), Kind),
			UMaterialNodeService::CreateNamedRerouteUsage(Graph.Path, TEXT("NoSuchVariable"), 0, 0, TEXT(""), TEXT("")).IsEmpty());
		TestTrue(FString::Printf(TEXT("%s: usage with empty name fails"), Kind),
			UMaterialNodeService::CreateNamedRerouteUsage(Graph.Path, TEXT(""), 0, 0, TEXT(""), TEXT("")).IsEmpty());
		TestTrue(FString::Printf(TEXT("%s: declaration with empty name fails"), Kind),
			UMaterialNodeService::CreateNamedRerouteDeclaration(Graph.Path, TEXT(""), FLinearColor::White, 0, 0, TEXT(""), TEXT("")).Name.IsEmpty());
		// A numeric index is not a node ref (DEC-6297: no index fallback).
		TestTrue(FString::Printf(TEXT("%s: numeric source ref is refused"), Kind),
			UMaterialNodeService::CreateNamedRerouteDeclaration(Graph.Path, TEXT("ByIndex"), FLinearColor::White, 0, 0, TEXT("0"), TEXT("")).Name.IsEmpty());
		TestTrue(FString::Printf(TEXT("%s: unknown path is refused"), Kind),
			UMaterialNodeService::ListNamedReroutes(TEXT("/Game/__VibeUETest/DoesNotExist")).Declarations.IsEmpty());
		TestEqual(FString::Printf(TEXT("%s: failed calls changed nothing"), Kind), Graph.Expressions().Num(), CountBefore);
	}

	// Owner check: a Material that calls a function must not reach into that function's expressions,
	// by session id or by object path.
	{
		FScratchGraph Function(TEXT("MF_NamedRerouteOwner"), /*bFunction*/ true);
		FScratchGraph Caller(TEXT("M_NamedRerouteOwner"), /*bFunction*/ false);
		if (!TestTrue(TEXT("owner check: scratch assets created"), Function.IsValid() && Caller.IsValid()))
		{
			return false;
		}
		UMaterialExpression* Inner = Function.NewExpression(UMaterialExpressionConstant3Vector::StaticClass());
		UMaterialExpressionFunctionOutput* Output = Cast<UMaterialExpressionFunctionOutput>(Function.NewExpression(UMaterialExpressionFunctionOutput::StaticClass(), 300, 0));
		if (!TestTrue(TEXT("owner check: inner nodes created"), Inner && Output))
		{
			return false;
		}
		Output->A.Connect(0, Inner);
		Function.Function->PostEditChange();
		const FMaterialExpressionInfo Call = UMaterialNodeService::CreateFunctionCall(Caller.Path, Function.Path, 0, 0);
		if (!TestFalse(TEXT("owner check: function call created"), Call.Id.IsEmpty()))
		{
			return false;
		}

		const int32 CallerCount = Caller.Expressions().Num();
		const int32 FunctionCount = Function.Expressions().Num();
		for (const FString& Ref : {SessionId(Inner), Inner->GetPathName(), Inner->GetName()})
		{
			TestTrue(FString::Printf(TEXT("owner check: Material refuses the function-internal ref '%s'"), *Ref),
				UMaterialNodeService::CreateNamedRerouteDeclaration(Caller.Path, TEXT("Leak"), FLinearColor::White, 0, 0, Ref, TEXT("")).Name.IsEmpty());
		}
		// ...while the function resolves its own node by the same refs.
		TestFalse(TEXT("owner check: the function accepts its own node"),
			UMaterialNodeService::CreateNamedRerouteDeclaration(Function.Path, TEXT("Own"), FLinearColor::White, 0, 0, Inner->GetPathName(), TEXT("")).Name.IsEmpty());
		TestEqual(TEXT("owner check: Material graph untouched by the refused calls"), Caller.Expressions().Num(), CallerCount);
		TestEqual(TEXT("owner check: only the accepted call added a node to the function"), Function.Expressions().Num(), FunctionCount + 1);
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// Rename keeps usages linked, keeps the VariableGuid, and resolves a clash with a suffix.
// ---------------------------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeNamedRerouteRenameTest,
	"VibeUE.MaterialNodeService.NamedReroute.Rename",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibeNamedRerouteRenameTest::RunTest(const FString&)
{
	// The refused calls below log warnings, which the framework counts as errors unless expected.
	AddExpectedError(TEXT("no named reroute declaration called"), EAutomationExpectedErrorFlags::Contains, 0);
	AddExpectedError(TEXT("Name is empty"), EAutomationExpectedErrorFlags::Contains, 0);

	for (const bool bFunction : {false, true})
	{
		const TCHAR* Kind = bFunction ? TEXT("MaterialFunction") : TEXT("Material");
		FScratchGraph Graph(bFunction ? TEXT("MF_NamedRerouteRename") : TEXT("M_NamedRerouteRename"), bFunction);
		if (!TestTrue(FString::Printf(TEXT("%s: scratch asset created"), Kind), Graph.IsValid()))
		{
			return false;
		}

		const FVibeNamedRerouteInfo Decl = UMaterialNodeService::CreateNamedRerouteDeclaration(
			Graph.Path, TEXT("Accent"), FLinearColor::White, 0, 0, TEXT(""), TEXT(""));
		UMaterialNodeService::CreateNamedRerouteDeclaration(Graph.Path, TEXT("Other"), FLinearColor::White, 0, 200, TEXT(""), TEXT(""));
		UMaterialNodeService::CreateNamedRerouteUsage(Graph.Path, TEXT("Accent"), 300, 0, TEXT(""), TEXT(""));
		UMaterialNodeService::CreateNamedRerouteUsage(Graph.Path, TEXT("Accent"), 300, 100, TEXT(""), TEXT(""));

		const FString NewName = UMaterialNodeService::RenameNamedRerouteDeclaration(Graph.Path, TEXT("Accent"), TEXT("Highlight"));
		TestEqual(FString::Printf(TEXT("%s: rename returns the final name"), Kind), NewName, FString(TEXT("Highlight")));

		FVibeNamedRerouteInfo Renamed;
		for (const FVibeNamedRerouteInfo& Listed : UMaterialNodeService::ListNamedReroutes(Graph.Path).Declarations)
		{
			if (Listed.Name == TEXT("Highlight"))
			{
				Renamed = Listed;
			}
			TestNotEqual(FString::Printf(TEXT("%s: the old name is gone"), Kind), Listed.Name, FString(TEXT("Accent")));
		}
		TestEqual(FString::Printf(TEXT("%s: renamed declaration keeps both usages"), Kind), Renamed.UsageIds.Num(), 2);
		TestEqual(FString::Printf(TEXT("%s: VariableGuid unchanged by rename"), Kind), Renamed.VariableGuid, Decl.VariableGuid);
		TestEqual(FString::Printf(TEXT("%s: no broken usages after rename"), Kind), UMaterialNodeService::ListNamedReroutes(Graph.Path).BrokenUsageIds.Num(), 0);

		// Usages still point at the declaration object itself (no relink happened or was needed).
		const TArray<UMaterialExpressionNamedRerouteUsage*> Usages = Graph.All<UMaterialExpressionNamedRerouteUsage>();
		for (UMaterialExpressionNamedRerouteUsage* Usage : Usages)
		{
			TestTrue(FString::Printf(TEXT("%s: usage still linked to the renamed declaration"), Kind),
				Usage->Declaration && Usage->Declaration->Name == FName(TEXT("Highlight")));
		}

		// The old name no longer resolves; the new one does.
		TestTrue(FString::Printf(TEXT("%s: rename of the old name fails"), Kind),
			UMaterialNodeService::RenameNamedRerouteDeclaration(Graph.Path, TEXT("Accent"), TEXT("Again")).IsEmpty());
		TestFalse(FString::Printf(TEXT("%s: usage by the new name works"), Kind),
			UMaterialNodeService::CreateNamedRerouteUsage(Graph.Path, TEXT("Highlight"), 300, 200, TEXT(""), TEXT("")).IsEmpty());

		// Renaming onto a taken name is resolved by suffix, never by merging or by failing.
		const FString Clash = UMaterialNodeService::RenameNamedRerouteDeclaration(Graph.Path, TEXT("Highlight"), TEXT("Other"));
		TestTrue(FString::Printf(TEXT("%s: rename onto a taken name is suffixed (got '%s')"), Kind, *Clash),
			Clash.StartsWith(TEXT("Other")) && Clash != TEXT("Other"));
		TestEqual(FString::Printf(TEXT("%s: still two declarations after the clash rename"), Kind),
			UMaterialNodeService::ListNamedReroutes(Graph.Path).Declarations.Num(), 2);

		TestTrue(FString::Printf(TEXT("%s: empty new name is refused"), Kind),
			UMaterialNodeService::RenameNamedRerouteDeclaration(Graph.Path, Clash, TEXT("")).IsEmpty());
	}
	return true;
}

#endif // WITH_AUTOMATION_TESTS
