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
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
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

namespace
{
	TSharedPtr<FJsonObject> ParseJsonObject(const FString& Json)
	{
		TSharedPtr<FJsonObject> Object;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		return FJsonSerializer::Deserialize(Reader, Object) ? Object : nullptr;
	}

	// The object in a JSON array field whose string field Key equals Value.
	TSharedPtr<FJsonObject> FindByField(const TSharedPtr<FJsonObject>& Root, const TCHAR* ArrayField, const TCHAR* Key, const FString& Value)
	{
		const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
		if (Root.IsValid() && Root->TryGetArrayField(ArrayField, Array))
		{
			for (const TSharedPtr<FJsonValue>& Item : *Array)
			{
				const TSharedPtr<FJsonObject>* Object = nullptr;
				FString Found;
				if (Item->TryGetObject(Object) && (*Object)->TryGetStringField(Key, Found) && Found == Value)
				{
					return *Object;
				}
			}
		}
		return nullptr;
	}

	int32 ArrayNum(const TSharedPtr<FJsonObject>& Root, const TCHAR* ArrayField)
	{
		const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
		return (Root.IsValid() && Root->TryGetArrayField(ArrayField, Array)) ? Array->Num() : -1;
	}

	FString Serialize(const TSharedPtr<FJsonObject>& Object)
	{
		FString Out;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Object.ToSharedRef(), Writer);
		return Out;
	}
}

// ---------------------------------------------------------------------------------------------
// Export carries the named_reroute link, and the export recipe rebuilds the variable by name.
// ---------------------------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeNamedRerouteExportTest,
	"VibeUE.MaterialNodeService.NamedReroute.Export",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibeNamedRerouteExportTest::RunTest(const FString&)
{
	for (const bool bFunction : {false, true})
	{
		const TCHAR* Kind = bFunction ? TEXT("MaterialFunction") : TEXT("Material");
		FScratchGraph Graph(bFunction ? TEXT("MF_NamedRerouteExport") : TEXT("M_NamedRerouteExport"), bFunction);
		FScratchGraph Copy(bFunction ? TEXT("MF_NamedRerouteExportCopy") : TEXT("M_NamedRerouteExportCopy"), bFunction);
		if (!TestTrue(FString::Printf(TEXT("%s: scratch assets created"), Kind), Graph.IsValid() && Copy.IsValid()))
		{
			return false;
		}

		UMaterialExpression* Source = Graph.NewExpression(UMaterialExpressionConstant3Vector::StaticClass(), -600, 0);
		const FVibeNamedRerouteInfo Decl = UMaterialNodeService::CreateNamedRerouteDeclaration(
			Graph.Path, TEXT("Accent"), FLinearColor::White, -300, 40, Source->GetPathName(), TEXT(""));
		const FString UsageA = UMaterialNodeService::CreateNamedRerouteUsage(Graph.Path, TEXT("Accent"), 0, 0, TEXT(""), TEXT(""));
		const FString UsageB = UMaterialNodeService::CreateNamedRerouteUsage(Graph.Path, TEXT("Accent"), 0, 120, TEXT(""), TEXT(""));
		if (!TestTrue(FString::Printf(TEXT("%s: variable built"), Kind), !Decl.Name.IsEmpty() && !UsageA.IsEmpty() && !UsageB.IsEmpty()))
		{
			return false;
		}

		const TSharedPtr<FJsonObject> Export = ParseJsonObject(
			bFunction ? UMaterialNodeService::ExportFunctionGraph(Graph.Path) : UMaterialNodeService::ExportMaterialGraph(Graph.Path));
		if (!TestTrue(FString::Printf(TEXT("%s: export parses"), Kind), Export.IsValid()))
		{
			return false;
		}

		// Declaration and both usages carry named_reroute with the declaration's NAME and guid.
		const TSharedPtr<FJsonObject> DeclNode = FindByField(Export, TEXT("expressions"), TEXT("id"), Decl.DeclarationId);
		const TSharedPtr<FJsonObject>* DeclKey = nullptr;
		if (!TestTrue(FString::Printf(TEXT("%s: declaration node exported with named_reroute"), Kind),
				DeclNode.IsValid() && DeclNode->TryGetObjectField(TEXT("named_reroute"), DeclKey)))
		{
			return false;
		}
		TestEqual(FString::Printf(TEXT("%s: declaration role"), Kind), (*DeclKey)->GetStringField(TEXT("role")), FString(TEXT("declaration")));
		TestEqual(FString::Printf(TEXT("%s: declaration name"), Kind), (*DeclKey)->GetStringField(TEXT("name")), FString(TEXT("Accent")));
		TestEqual(FString::Printf(TEXT("%s: declaration guid"), Kind), (*DeclKey)->GetStringField(TEXT("guid")), Decl.VariableGuid);
		for (const FString& UsageId : {UsageA, UsageB})
		{
			const TSharedPtr<FJsonObject> UsageNode = FindByField(Export, TEXT("expressions"), TEXT("id"), UsageId);
			const TSharedPtr<FJsonObject>* UsageKey = nullptr;
			if (TestTrue(FString::Printf(TEXT("%s: usage node exported with named_reroute"), Kind),
					UsageNode.IsValid() && UsageNode->TryGetObjectField(TEXT("named_reroute"), UsageKey)))
			{
				TestEqual(FString::Printf(TEXT("%s: usage role"), Kind), (*UsageKey)->GetStringField(TEXT("role")), FString(TEXT("usage")));
				TestEqual(FString::Printf(TEXT("%s: usage names its declaration"), Kind), (*UsageKey)->GetStringField(TEXT("name")), FString(TEXT("Accent")));
				TestEqual(FString::Printf(TEXT("%s: usage declaration_id"), Kind), (*UsageKey)->GetStringField(TEXT("declaration_id")), Decl.DeclarationId);
				TestFalse(FString::Printf(TEXT("%s: healthy usage is not flagged broken"), Kind), (*UsageKey)->HasField(TEXT("broken")));
			}
		}
		// Ordinary nodes get no new key.
		const TSharedPtr<FJsonObject> SourceNode = FindByField(Export, TEXT("expressions"), TEXT("id"), SessionId(Source));
		TestTrue(FString::Printf(TEXT("%s: non-reroute node has no named_reroute key"), Kind), SourceNode.IsValid() && !SourceNode->HasField(TEXT("named_reroute")));

		// The recreate recipe (reference.md): declarations first by name, then usages by name.
		const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
		Export->TryGetArrayField(TEXT("expressions"), Nodes);
		for (const TSharedPtr<FJsonValue>& Node : *Nodes)
		{
			const TSharedPtr<FJsonObject>* NodeObject = nullptr;
			const TSharedPtr<FJsonObject>* Key = nullptr;
			if (Node->TryGetObject(NodeObject) && (*NodeObject)->TryGetObjectField(TEXT("named_reroute"), Key) &&
				(*Key)->GetStringField(TEXT("role")) == TEXT("declaration"))
			{
				const FVibeNamedRerouteInfo Made = UMaterialNodeService::CreateNamedRerouteDeclaration(
					Copy.Path, (*Key)->GetStringField(TEXT("name")), FLinearColor::White,
					(*NodeObject)->GetIntegerField(TEXT("pos_x")), (*NodeObject)->GetIntegerField(TEXT("pos_y")), TEXT(""), TEXT(""));
				TestEqual(FString::Printf(TEXT("%s: recreated declaration keeps its exported name"), Kind), Made.Name, (*Key)->GetStringField(TEXT("name")));
			}
		}
		for (const TSharedPtr<FJsonValue>& Node : *Nodes)
		{
			const TSharedPtr<FJsonObject>* NodeObject = nullptr;
			const TSharedPtr<FJsonObject>* Key = nullptr;
			if (Node->TryGetObject(NodeObject) && (*NodeObject)->TryGetObjectField(TEXT("named_reroute"), Key) &&
				(*Key)->GetStringField(TEXT("role")) == TEXT("usage"))
			{
				TestFalse(FString::Printf(TEXT("%s: recreated usage by name"), Kind),
					UMaterialNodeService::CreateNamedRerouteUsage(Copy.Path, (*Key)->GetStringField(TEXT("name")),
						(*NodeObject)->GetIntegerField(TEXT("pos_x")), (*NodeObject)->GetIntegerField(TEXT("pos_y")), TEXT(""), TEXT("")).IsEmpty());
			}
		}
		const FVibeNamedRerouteList Original = UMaterialNodeService::ListNamedReroutes(Graph.Path);
		const FVibeNamedRerouteList Rebuilt = UMaterialNodeService::ListNamedReroutes(Copy.Path);
		if (TestEqual(FString::Printf(TEXT("%s: rebuilt graph has the same declarations"), Kind), Rebuilt.Declarations.Num(), Original.Declarations.Num()) &&
			Rebuilt.Declarations.Num() == 1)
		{
			TestEqual(FString::Printf(TEXT("%s: rebuilt name matches"), Kind), Rebuilt.Declarations[0].Name, Original.Declarations[0].Name);
			TestEqual(FString::Printf(TEXT("%s: rebuilt usage count matches"), Kind), Rebuilt.Declarations[0].UsageIds.Num(), Original.Declarations[0].UsageIds.Num());
			TestNotEqual(FString::Printf(TEXT("%s: rebuilt guid is the new graph's own"), Kind), Rebuilt.Declarations[0].VariableGuid, Original.Declarations[0].VariableGuid);
		}

		// A usage whose declaration is gone is flagged, with an empty name.
		const TArray<UMaterialExpressionNamedRerouteUsage*> Usages = Graph.All<UMaterialExpressionNamedRerouteUsage>();
		if (TestTrue(FString::Printf(TEXT("%s: usages present"), Kind), Usages.Num() == 2))
		{
			Usages[0]->Declaration = nullptr;
			const TSharedPtr<FJsonObject> Broken = ParseJsonObject(
				bFunction ? UMaterialNodeService::ExportFunctionGraph(Graph.Path) : UMaterialNodeService::ExportMaterialGraph(Graph.Path));
			const TSharedPtr<FJsonObject> BrokenNode = FindByField(Broken, TEXT("expressions"), TEXT("id"), SessionId(Usages[0]));
			const TSharedPtr<FJsonObject>* BrokenKey = nullptr;
			if (TestTrue(FString::Printf(TEXT("%s: broken usage still exported"), Kind),
					BrokenNode.IsValid() && BrokenNode->TryGetObjectField(TEXT("named_reroute"), BrokenKey)))
			{
				TestTrue(FString::Printf(TEXT("%s: broken usage flagged"), Kind), (*BrokenKey)->GetBoolField(TEXT("broken")));
				TestEqual(FString::Printf(TEXT("%s: broken usage has an empty name"), Kind), (*BrokenKey)->GetStringField(TEXT("name")), FString());
			}
		}

		// The other readers of a material graph still work with reroute nodes present.
		if (!bFunction)
		{
			TestNotNull(TEXT("summary export parses"), ParseJsonObject(UMaterialNodeService::ExportMaterialGraphSummary(Graph.Path)).Get());
			TestNotNull(TEXT("compare parses"), ParseJsonObject(UMaterialNodeService::CompareMaterialGraphs(Graph.Path, Copy.Path)).Get());
		}
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// get_named_reroute_definition: the generating subgraph, its boundary and its consumers.
//   Base = Add(Constant, TexCoord);  Tint = Multiply(ScalarParam "Strength", Usage(Base))
//   Tint is consumed twice (output / BaseColor, and Multiply2.A).
// ---------------------------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeNamedRerouteDefinitionTest,
	"VibeUE.MaterialNodeService.NamedReroute.Definition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibeNamedRerouteDefinitionTest::RunTest(const FString&)
{
	AddExpectedError(TEXT("no named reroute declaration called"), EAutomationExpectedErrorFlags::Contains, 0);
	for (const bool bFunction : {false, true})
	{
		const TCHAR* Kind = bFunction ? TEXT("MaterialFunction") : TEXT("Material");
		FScratchGraph Graph(bFunction ? TEXT("MF_NamedRerouteDefinition") : TEXT("M_NamedRerouteDefinition"), bFunction);
		if (!TestTrue(FString::Printf(TEXT("%s: scratch asset created"), Kind), Graph.IsValid()))
		{
			return false;
		}

		UMaterialExpression* Constant = Graph.NewExpression(UMaterialExpressionConstant::StaticClass(), -900, 0);
		UMaterialExpression* TexCoord = Graph.NewExpression(UMaterialExpressionTextureCoordinate::StaticClass(), -900, 100);
		UMaterialExpressionAdd* Add = Cast<UMaterialExpressionAdd>(Graph.NewExpression(UMaterialExpressionAdd::StaticClass(), -700, 0));
		UMaterialExpressionScalarParameter* Param = Cast<UMaterialExpressionScalarParameter>(Graph.NewExpression(UMaterialExpressionScalarParameter::StaticClass(), -700, 200));
		UMaterialExpressionMultiply* Multiply = Cast<UMaterialExpressionMultiply>(Graph.NewExpression(UMaterialExpressionMultiply::StaticClass(), -300, 0));
		UMaterialExpressionMultiply* Multiply2 = Cast<UMaterialExpressionMultiply>(Graph.NewExpression(UMaterialExpressionMultiply::StaticClass(), 300, 200));
		if (!TestTrue(FString::Printf(TEXT("%s: nodes created"), Kind), Constant && TexCoord && Add && Param && Multiply && Multiply2))
		{
			return false;
		}
		Param->ParameterName = TEXT("Strength");
		Add->A.Connect(0, Constant);
		Add->B.Connect(0, TexCoord);
		Multiply->A.Connect(0, Param);

		UMaterialNodeService::CreateNamedRerouteDeclaration(Graph.Path, TEXT("Base"), FLinearColor::White, -500, 0, Add->GetPathName(), TEXT(""));
		const FString BaseUsage = UMaterialNodeService::CreateNamedRerouteUsage(Graph.Path, TEXT("Base"), -500, 100, Multiply->GetPathName(), TEXT("B"));
		const FVibeNamedRerouteInfo Tint = UMaterialNodeService::CreateNamedRerouteDeclaration(
			Graph.Path, TEXT("Tint"), FLinearColor::White, -100, 0, Multiply->GetPathName(), TEXT(""));
		const FString TintUsageA = UMaterialNodeService::CreateNamedRerouteUsage(Graph.Path, TEXT("Tint"), 100, 0, TEXT(""), TEXT(""));
		const FString TintUsageB = UMaterialNodeService::CreateNamedRerouteUsage(Graph.Path, TEXT("Tint"), 100, 200, Multiply2->GetPathName(), TEXT("A"));
		if (!TestTrue(FString::Printf(TEXT("%s: variables built"), Kind), !BaseUsage.IsEmpty() && !Tint.Name.IsEmpty() && !TintUsageA.IsEmpty() && !TintUsageB.IsEmpty()))
		{
			return false;
		}
		if (bFunction)
		{
			UMaterialExpressionFunctionOutput* Output = Cast<UMaterialExpressionFunctionOutput>(Graph.NewExpression(UMaterialExpressionFunctionOutput::StaticClass(), 500, 0));
			UMaterialExpressionNamedRerouteUsage* UsageNode = nullptr;
			for (UMaterialExpressionNamedRerouteUsage* Candidate : Graph.All<UMaterialExpressionNamedRerouteUsage>())
			{
				if (SessionId(Candidate) == TintUsageA)
				{
					UsageNode = Candidate;
				}
			}
			if (!TestTrue(TEXT("MaterialFunction: output wired from the first Tint usage"), Output && UsageNode))
			{
				return false;
			}
			Output->A.Connect(0, UsageNode);
		}
		else
		{
			TestTrue(TEXT("Material: Tint usage -> BaseColor"), UMaterialNodeService::ConnectExpressionToOutput(Graph.Path, TintUsageA, TEXT(""), TEXT("BaseColor")));
		}

		// --- Tint: Multiply + its parameter + the Base usage, nothing of Base's own generation. ---
		const TSharedPtr<FJsonObject> Def = ParseJsonObject(UMaterialNodeService::GetNamedRerouteDefinition(Graph.Path, TEXT("Tint")));
		if (!TestTrue(FString::Printf(TEXT("%s: Tint definition parses"), Kind), Def.IsValid()))
		{
			return false;
		}
		const TSharedPtr<FJsonObject>* DeclObj = nullptr;
		if (TestTrue(FString::Printf(TEXT("%s: declaration block"), Kind), Def->TryGetObjectField(TEXT("declaration"), DeclObj)))
		{
			TestEqual(FString::Printf(TEXT("%s: declaration name"), Kind), (*DeclObj)->GetStringField(TEXT("name")), FString(TEXT("Tint")));
			TestEqual(FString::Printf(TEXT("%s: declaration variable_guid"), Kind), (*DeclObj)->GetStringField(TEXT("variable_guid")), Tint.VariableGuid);
		}
		TestEqual(FString::Printf(TEXT("%s: Tint nodes = Multiply + Strength + Base usage"), Kind), ArrayNum(Def, TEXT("nodes")), 3);
		for (UMaterialExpression* Inlined : {static_cast<UMaterialExpression*>(Add), Constant, TexCoord})
		{
			TestFalse(FString::Printf(TEXT("%s: Base's own node %s is not inlined"), Kind, *Inlined->GetClass()->GetName()),
				FindByField(Def, TEXT("nodes"), TEXT("id"), SessionId(Inlined)).IsValid());
		}
		TestTrue(FString::Printf(TEXT("%s: Multiply is in the definition"), Kind), FindByField(Def, TEXT("nodes"), TEXT("id"), SessionId(Multiply)).IsValid());
		TestEqual(FString::Printf(TEXT("%s: two boundary inputs"), Kind), ArrayNum(Def, TEXT("inputs")), 2);
		const TSharedPtr<FJsonObject> ParamInput = FindByField(Def, TEXT("inputs"), TEXT("expression_id"), SessionId(Param));
		TestTrue(FString::Printf(TEXT("%s: the parameter is listed as an input by name"), Kind),
			ParamInput.IsValid() && ParamInput->GetStringField(TEXT("kind")) == TEXT("parameter") && ParamInput->GetStringField(TEXT("name")) == TEXT("Strength"));
		const TSharedPtr<FJsonObject> UsageInput = FindByField(Def, TEXT("inputs"), TEXT("usage_id"), BaseUsage);
		TestTrue(FString::Printf(TEXT("%s: the Base usage is a dependency, not inlined"), Kind),
			UsageInput.IsValid() && UsageInput->GetStringField(TEXT("depends_on")) == TEXT("Base"));
		// Param->Multiply, BaseUsage->Multiply, Multiply->declaration.
		TestEqual(FString::Printf(TEXT("%s: three connections"), Kind), ArrayNum(Def, TEXT("connections")), 3);
		TestTrue(FString::Printf(TEXT("%s: the wire into the declaration is included"), Kind),
			FindByField(Def, TEXT("connections"), TEXT("target_id"), Tint.DeclarationId).IsValid());

		// Same node shape as the full export, by construction.
		const TSharedPtr<FJsonObject> Export = ParseJsonObject(
			bFunction ? UMaterialNodeService::ExportFunctionGraph(Graph.Path) : UMaterialNodeService::ExportMaterialGraph(Graph.Path));
		const TSharedPtr<FJsonObject> InDef = FindByField(Def, TEXT("nodes"), TEXT("id"), SessionId(Multiply));
		const TSharedPtr<FJsonObject> InExport = FindByField(Export, TEXT("expressions"), TEXT("id"), SessionId(Multiply));
		TestTrue(FString::Printf(TEXT("%s: definition node is byte-identical to the export node"), Kind),
			InDef.IsValid() && InExport.IsValid() && Serialize(InDef) == Serialize(InExport));
		const TSharedPtr<FJsonObject> UsageInDef = FindByField(Def, TEXT("nodes"), TEXT("id"), BaseUsage);
		const TSharedPtr<FJsonObject>* UsageKey = nullptr;
		TestTrue(FString::Printf(TEXT("%s: usage node in the definition carries named_reroute"), Kind),
			UsageInDef.IsValid() && UsageInDef->TryGetObjectField(TEXT("named_reroute"), UsageKey) && (*UsageKey)->GetStringField(TEXT("name")) == TEXT("Base"));

		// --- consumers ---
		TestEqual(FString::Printf(TEXT("%s: two Tint consumers"), Kind), ArrayNum(Def, TEXT("consumers")), 2);
		const TSharedPtr<FJsonObject> ConsumerA = FindByField(Def, TEXT("consumers"), TEXT("usage_id"), TintUsageA);
		const TSharedPtr<FJsonObject> ConsumerB = FindByField(Def, TEXT("consumers"), TEXT("usage_id"), TintUsageB);
		if (TestTrue(FString::Printf(TEXT("%s: both usages listed as consumers"), Kind), ConsumerA.IsValid() && ConsumerB.IsValid()))
		{
			TestEqual(FString::Printf(TEXT("%s: usage A feeds one place"), Kind), ArrayNum(ConsumerA, TEXT("feeds")), 1);
			TSharedPtr<FJsonObject> Feed;
			const TArray<TSharedPtr<FJsonValue>>* FeedsB = nullptr;
			if (ConsumerB->TryGetArrayField(TEXT("feeds"), FeedsB) && FeedsB->Num() == 1)
			{
				const TSharedPtr<FJsonObject>* FeedObject = nullptr;
				if ((*FeedsB)[0]->TryGetObject(FeedObject))
				{
					TestEqual(FString::Printf(TEXT("%s: usage B feeds Multiply2"), Kind), (*FeedObject)->GetStringField(TEXT("expression_id")), SessionId(Multiply2));
					TestEqual(FString::Printf(TEXT("%s: usage B feeds input A"), Kind), (*FeedObject)->GetStringField(TEXT("input_name")), FString(TEXT("A")));
				}
			}
			else
			{
				AddError(FString::Printf(TEXT("%s: usage B should feed exactly one input"), Kind));
			}
			const TArray<TSharedPtr<FJsonValue>>* FeedsA = nullptr;
			const TSharedPtr<FJsonObject>* FeedA = nullptr;
			if (ConsumerA->TryGetArrayField(TEXT("feeds"), FeedsA) && FeedsA->Num() == 1 && (*FeedsA)[0]->TryGetObject(FeedA))
			{
				if (bFunction)
				{
					TestTrue(TEXT("MaterialFunction: usage A feeds a function output"), (*FeedA)->HasField(TEXT("function_output")));
				}
				else
				{
					TestEqual(TEXT("Material: usage A feeds BaseColor"), (*FeedA)->GetStringField(TEXT("property")), FString(TEXT("BaseColor")));
				}
			}
		}

		// --- Base: the other side of the boundary. Constant + TexCoord + Add, one constant input, one consumer. ---
		const TSharedPtr<FJsonObject> BaseDef = ParseJsonObject(UMaterialNodeService::GetNamedRerouteDefinition(Graph.Path, TEXT("Base")));
		TestEqual(FString::Printf(TEXT("%s: Base nodes = Add + Constant + TexCoord"), Kind), ArrayNum(BaseDef, TEXT("nodes")), 3);
		TestEqual(FString::Printf(TEXT("%s: Base has one boundary input (the constant)"), Kind), ArrayNum(BaseDef, TEXT("inputs")), 1);
		const TSharedPtr<FJsonObject> ConstantInput = FindByField(BaseDef, TEXT("inputs"), TEXT("expression_id"), SessionId(Constant));
		TestTrue(FString::Printf(TEXT("%s: the constant is the input"), Kind), ConstantInput.IsValid() && ConstantInput->GetStringField(TEXT("kind")) == TEXT("constant"));
		TestEqual(FString::Printf(TEXT("%s: Base has one consumer"), Kind), ArrayNum(BaseDef, TEXT("consumers")), 1);

		// --- failures ---
		TestTrue(FString::Printf(TEXT("%s: unknown name gives an empty definition"), Kind),
			UMaterialNodeService::GetNamedRerouteDefinition(Graph.Path, TEXT("NoSuchVariable")).IsEmpty());
	}
	return true;
}

#endif // WITH_AUTOMATION_TESTS
