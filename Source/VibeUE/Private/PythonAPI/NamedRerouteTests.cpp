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
#include "Materials/MaterialExpressionReroute.h"
#include "MaterialShared.h"
#include "RHIShaderPlatform.h"
#include "MaterialGraph/MaterialGraph.h"
#include "MaterialGraph/MaterialGraphSchema.h"
#include "Kismet2/BlueprintEditorUtils.h"
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

// =============================================================================================
// WI-1672 P2: promote_to_named_reroute + apply_reroute_readability_pass. The acceptance bar is "compiled
// result unchanged": every case compares the GetMaterialDiagnostics error list AND the generated HLSL
// (/Engine/Generated/Material.ush, FMaterial::GetMaterialExpressionSource) before and after; a wrong rewire
// (dropped mask, wrong output, consumer left on a stale pin) changes the HLSL text.
// =============================================================================================

namespace
{
	FString TranslatedHlsl(UMaterial* Material)
	{
		FString Source;
		if (Material)
		{
			if (FMaterialResource* Resource = Material->GetMaterialResource(GMaxRHIShaderPlatform))
			{
				Resource->GetMaterialExpressionSource(Source);
			}
		}
		return Source;
	}

	struct FCompileSnapshot
	{
		bool bCompiledOk = false;
		TArray<FString> Errors;
		FString Hlsl;
	};

	FCompileSnapshot TakeCompileSnapshot(UMaterial* Material, const FString& MaterialPath)
	{
		FCompileSnapshot Snapshot;
		const FMaterialDiagnostics Diag = UMaterialNodeService::GetMaterialDiagnostics(MaterialPath);
		Snapshot.bCompiledOk = Diag.bSuccess && Diag.bIsCompiledOk;
		Snapshot.Errors = Diag.CompileErrors;
		Snapshot.Hlsl = TranslatedHlsl(Material);
		return Snapshot;
	}

	// The Material whose compile stands for a scratch graph: the graph itself, or for a MaterialFunction a scratch
	// Material whose function call feeds BaseColor from output "Result" and EmissiveColor from "Sum" (when the
	// function has one). GetMaterialDiagnostics is Material-only (P0 G2).
	struct FCompileHost
	{
		TUniquePtr<FScratchGraph> Caller;
		UMaterial* Material = nullptr;
		FString Path;
	};

	bool MakeCompileHost(const FScratchGraph& Graph, const TCHAR* CallerName, FCompileHost& Out)
	{
		if (Graph.Material)
		{
			Out.Material = Graph.Material;
			Out.Path = Graph.Path;
			return true;
		}
		Graph.Function->PostEditChange();
		Out.Caller = MakeUnique<FScratchGraph>(CallerName, /*bFunction*/ false);
		if (!Out.Caller->IsValid())
		{
			return false;
		}
		const FMaterialExpressionInfo Call = UMaterialNodeService::CreateFunctionCall(Out.Caller->Path, Graph.Path, 0, 0);
		if (Call.Id.IsEmpty())
		{
			return false;
		}
		bool bOk = UMaterialNodeService::ConnectExpressionToOutput(Out.Caller->Path, Call.Id, TEXT("Result"), TEXT("BaseColor"));
		for (const UMaterialExpressionFunctionOutput* Output : Graph.All<UMaterialExpressionFunctionOutput>())
		{
			if (Output->OutputName == FName(TEXT("Sum")))
			{
				bOk &= UMaterialNodeService::ConnectExpressionToOutput(Out.Caller->Path, Call.Id, TEXT("Sum"), TEXT("EmissiveColor"));
			}
		}
		Out.Material = Out.Caller->Material;
		Out.Path = Out.Caller->Path;
		return bOk;
	}

	UMaterialExpressionFunctionOutput* NewFunctionOutput(const FScratchGraph& Graph, const TCHAR* Name, int32 X, int32 Y, UMaterialExpression* From)
	{
		UMaterialExpressionFunctionOutput* Output = Cast<UMaterialExpressionFunctionOutput>(
			Graph.NewExpression(UMaterialExpressionFunctionOutput::StaticClass(), X, Y));
		if (Output)
		{
			Output->OutputName = FName(Name);
			Output->A.Connect(0, From);
		}
		return Output;
	}

	// The declaration an input reads through a linked usage; null when the input is not wired to one.
	const UMaterialExpressionNamedRerouteDeclaration* DeclarationBehind(const FExpressionInput* Input)
	{
		const UMaterialExpressionNamedRerouteUsage* Usage = Input ? Cast<UMaterialExpressionNamedRerouteUsage>(Input->Expression) : nullptr;
		return Usage ? Usage->Declaration.Get() : nullptr;
	}

	// Compares two snapshots, records the receipt line, returns whether the compiled result is identical.
	bool CompareSnapshots(FAutomationTestBase& Test, const FString& Label, const FCompileSnapshot& Before, const FCompileSnapshot& After)
	{
		Test.TestTrue(FString::Printf(TEXT("%s: compiles clean after (%s)"), *Label, *FString::Join(After.Errors, TEXT(" | "))),
			After.bCompiledOk && After.Errors.Num() == 0);
		Test.TestTrue(FString::Printf(TEXT("%s: compile error list unchanged"), *Label), After.Errors == Before.Errors);
		const bool bIdentical = !Before.Hlsl.IsEmpty() && Before.Hlsl == After.Hlsl && Before.Errors == After.Errors;
		Test.TestTrue(FString::Printf(TEXT("%s: generated HLSL identical (%d vs %d chars)"), *Label, Before.Hlsl.Len(), After.Hlsl.Len()), bIdentical);
		return bIdentical;
	}
}

// ---------------------------------------------------------------------------------------------
// 1 output, 4 consumers (3 node inputs + BaseColor / a FunctionOutput) -> 1 declaration + 4 usages,
// compiled result unchanged. Material and MaterialFunction.
// ---------------------------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeNamedReroutePromoteFanoutTest,
	"VibeUE.MaterialNodeService.NamedReroute.PromoteFanout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibeNamedReroutePromoteFanoutTest::RunTest(const FString&)
{
	for (const bool bFunction : {false, true})
	{
		const TCHAR* Kind = bFunction ? TEXT("MaterialFunction") : TEXT("Material");
		FScratchGraph Graph(bFunction ? TEXT("MF_NamedReroutePromote") : TEXT("M_NamedReroutePromote"), bFunction);
		if (!TestTrue(FString::Printf(TEXT("%s: scratch asset created"), Kind), Graph.IsValid()))
		{
			return false;
		}

		UMaterialExpressionConstant3Vector* Color = Cast<UMaterialExpressionConstant3Vector>(Graph.NewExpression(UMaterialExpressionConstant3Vector::StaticClass(), -1200, 0));
		UMaterialExpressionConstant* Scale = Cast<UMaterialExpressionConstant>(Graph.NewExpression(UMaterialExpressionConstant::StaticClass(), -1200, 200));
		UMaterialExpressionConstant* Bias = Cast<UMaterialExpressionConstant>(Graph.NewExpression(UMaterialExpressionConstant::StaticClass(), -900, -300));
		UMaterialExpressionMultiply* Mul = Cast<UMaterialExpressionMultiply>(Graph.NewExpression(UMaterialExpressionMultiply::StaticClass(), -900, 0));
		UMaterialExpressionAdd* Add1 = Cast<UMaterialExpressionAdd>(Graph.NewExpression(UMaterialExpressionAdd::StaticClass(), -500, -300));
		UMaterialExpressionAdd* Add2 = Cast<UMaterialExpressionAdd>(Graph.NewExpression(UMaterialExpressionAdd::StaticClass(), -500, 0));
		UMaterialExpressionAdd* Add3 = Cast<UMaterialExpressionAdd>(Graph.NewExpression(UMaterialExpressionAdd::StaticClass(), -500, 300));
		if (!TestTrue(FString::Printf(TEXT("%s: nodes created"), Kind), Color && Scale && Bias && Mul && Add1 && Add2 && Add3))
		{
			return false;
		}
		Color->Constant = FLinearColor(0.2f, 0.4f, 0.6f);
		Scale->R = 2.f;
		Bias->R = 0.1f;
		Mul->A.Connect(0, Color);
		Mul->B.Connect(0, Scale);
		Add1->A.Connect(0, Mul);
		Add1->B.Connect(0, Bias);
		Add2->A.Connect(0, Add1);
		Add2->B.Connect(0, Mul);
		Add3->A.Connect(0, Add2);
		Add3->B.Connect(0, Mul);

		FExpressionInput* FourthConsumer = nullptr;
		if (!bFunction)
		{
			TestTrue(TEXT("Material: Mul -> BaseColor"), UMaterialNodeService::ConnectExpressionToOutput(Graph.Path, Mul->GetPathName(), TEXT(""), TEXT("BaseColor")));
			TestTrue(TEXT("Material: Add3 -> EmissiveColor"), UMaterialNodeService::ConnectExpressionToOutput(Graph.Path, Add3->GetPathName(), TEXT(""), TEXT("EmissiveColor")));
			FourthConsumer = Graph.Material->GetExpressionInputForProperty(MP_BaseColor);
		}
		else
		{
			UMaterialExpressionFunctionOutput* Result = NewFunctionOutput(Graph, TEXT("Result"), -100, 0, Mul);
			UMaterialExpressionFunctionOutput* Sum = NewFunctionOutput(Graph, TEXT("Sum"), -100, 300, Add3);
			if (!TestTrue(TEXT("MaterialFunction: outputs created"), Result && Sum))
			{
				return false;
			}
			FourthConsumer = &Result->A;
		}

		FCompileHost Host;
		if (!TestTrue(FString::Printf(TEXT("%s: compile host"), Kind), MakeCompileHost(Graph, TEXT("M_NamedReroutePromoteCaller"), Host)))
		{
			return false;
		}
		const FCompileSnapshot Before = TakeCompileSnapshot(Host.Material, Host.Path);
		TestTrue(FString::Printf(TEXT("%s: compiles clean before promote (control)"), Kind), Before.bCompiledOk && Before.Errors.Num() == 0 && !Before.Hlsl.IsEmpty());
		const int32 CountBefore = Graph.Expressions().Num();

		const FVibeNamedRerouteInfo Info = UMaterialNodeService::PromoteToNamedReroute(Graph.Path, Mul->GetName(), TEXT(""), TEXT("Tint"));
		TestEqual(FString::Printf(TEXT("%s: declaration name"), Kind), Info.Name, FString(TEXT("Tint")));
		TestEqual(FString::Printf(TEXT("%s: one usage per consumer input"), Kind), Info.UsageIds.Num(), 4);
		TestEqual(FString::Printf(TEXT("%s: 1 declaration + 4 usages added"), Kind), Graph.Expressions().Num(), CountBefore + 5);

		const FVibeNamedRerouteList List = UMaterialNodeService::ListNamedReroutes(Graph.Path);
		TestTrue(FString::Printf(TEXT("%s: list shows Tint with 4 usages"), Kind),
			List.Declarations.Num() == 1 && List.Declarations[0].UsageIds.Num() == 4 && List.BrokenUsageIds.Num() == 0);

		const TArray<UMaterialExpressionNamedRerouteDeclaration*> Decls = Graph.All<UMaterialExpressionNamedRerouteDeclaration>();
		if (!TestEqual(FString::Printf(TEXT("%s: one declaration in the graph"), Kind), Decls.Num(), 1))
		{
			return false;
		}
		const UMaterialExpressionNamedRerouteDeclaration* Decl = Decls[0];
		TestTrue(FString::Printf(TEXT("%s: declaration wired from Mul output 0"), Kind), Decl->Input.Expression == Mul && Decl->Input.OutputIndex == 0);
		TestTrue(FString::Printf(TEXT("%s: Add1.A reads the variable"), Kind), DeclarationBehind(&Add1->A) == Decl);
		TestTrue(FString::Printf(TEXT("%s: Add2.B reads the variable"), Kind), DeclarationBehind(&Add2->B) == Decl);
		TestTrue(FString::Printf(TEXT("%s: Add3.B reads the variable"), Kind), DeclarationBehind(&Add3->B) == Decl);
		TestTrue(FString::Printf(TEXT("%s: %s reads the variable"), Kind, bFunction ? TEXT("FunctionOutput Result") : TEXT("BaseColor")),
			DeclarationBehind(FourthConsumer) == Decl);
		TestTrue(FString::Printf(TEXT("%s: non-consumer wires untouched"), Kind),
			Add2->A.Expression == Add1 && Add3->A.Expression == Add2 && Mul->A.Expression == Color && Add1->B.Expression == Bias);

		const FCompileSnapshot After = TakeCompileSnapshot(Host.Material, Host.Path);
		const bool bIdentical = CompareSnapshots(*this, Kind, Before, After);
		const FString Receipt = FString::Printf(TEXT("PROMOTE kind=%s usages=%d diag_before=%d diag_after=%d identical=%s"),
			bFunction ? TEXT("MF") : TEXT("M"), Info.UsageIds.Num(), Before.Errors.Num(), After.Errors.Num(), bIdentical ? TEXT("true") : TEXT("false"));
		AddInfo(Receipt);
		UE_LOG(LogTemp, Display, TEXT("%s"), *Receipt);
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// Output selection, masks and refusals: of a Constant3Vector only output 1 (R) is promoted, output 0 stays
// wired; a consumer of output 1 with its own mask is skipped and stays on the source; refusals change nothing.
// ---------------------------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeNamedReroutePromoteMasksTest,
	"VibeUE.MaterialNodeService.NamedReroute.PromoteMasks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibeNamedReroutePromoteMasksTest::RunTest(const FString&)
{
	FScratchGraph Graph(TEXT("M_NamedReroutePromoteMasks"), /*bFunction*/ false);
	FScratchGraph Other(TEXT("M_NamedReroutePromoteMasksOther"), /*bFunction*/ false);
	if (!TestTrue(TEXT("scratch assets created"), Graph.IsValid() && Other.IsValid()))
	{
		return false;
	}

	UMaterialExpressionConstant3Vector* Color = Cast<UMaterialExpressionConstant3Vector>(Graph.NewExpression(UMaterialExpressionConstant3Vector::StaticClass(), -1000, 0));
	UMaterialExpressionConstant* K = Cast<UMaterialExpressionConstant>(Graph.NewExpression(UMaterialExpressionConstant::StaticClass(), -1000, 300));
	UMaterialExpressionAdd* AddFull = Cast<UMaterialExpressionAdd>(Graph.NewExpression(UMaterialExpressionAdd::StaticClass(), -500, -200));
	UMaterialExpressionMultiply* MulR1 = Cast<UMaterialExpressionMultiply>(Graph.NewExpression(UMaterialExpressionMultiply::StaticClass(), -500, 0));
	UMaterialExpressionMultiply* MulR2 = Cast<UMaterialExpressionMultiply>(Graph.NewExpression(UMaterialExpressionMultiply::StaticClass(), -500, 200));
	UMaterialExpressionAdd* AddMasked = Cast<UMaterialExpressionAdd>(Graph.NewExpression(UMaterialExpressionAdd::StaticClass(), -500, 400));
	UMaterialExpressionAdd* Final1 = Cast<UMaterialExpressionAdd>(Graph.NewExpression(UMaterialExpressionAdd::StaticClass(), -250, 0));
	UMaterialExpressionAdd* Final2 = Cast<UMaterialExpressionAdd>(Graph.NewExpression(UMaterialExpressionAdd::StaticClass(), -100, 200));
	UMaterialExpressionConstant* Unwired = Cast<UMaterialExpressionConstant>(Graph.NewExpression(UMaterialExpressionConstant::StaticClass(), -1000, 600));
	UMaterialExpression* Foreign = Other.NewExpression(UMaterialExpressionConstant::StaticClass(), 0, 0);
	if (!TestTrue(TEXT("nodes created"), Color && K && AddFull && MulR1 && MulR2 && AddMasked && Final1 && Final2 && Unwired && Foreign))
	{
		return false;
	}
	Color->Constant = FLinearColor(0.2f, 0.4f, 0.6f);
	K->R = 0.5f;
	AddFull->A.Connect(0, Color);          // output 0 (RGB): must stay wired
	AddFull->B.Connect(0, K);
	MulR1->A.Connect(1, Color);            // output 1 (R)
	MulR1->B.Connect(0, K);
	MulR2->A.Connect(1, Color);            // output 1 (R)
	MulR2->B.Connect(0, MulR1);
	AddMasked->A.Connect(1, Color);        // output 1, but with its own mask (G): must be skipped
	AddMasked->A.SetMask(1, 0, 1, 0, 0);
	AddMasked->B.Connect(0, K);
	Final1->A.Connect(0, AddFull);
	Final1->B.Connect(0, MulR2);
	Final2->A.Connect(0, Final1);
	Final2->B.Connect(0, AddMasked);
	TestTrue(TEXT("Final2 -> BaseColor"), UMaterialNodeService::ConnectExpressionToOutput(Graph.Path, Final2->GetPathName(), TEXT(""), TEXT("BaseColor")));

	const FCompileSnapshot Before = TakeCompileSnapshot(Graph.Material, Graph.Path);
	TestTrue(TEXT("compiles clean before promote (control)"), Before.bCompiledOk && Before.Errors.Num() == 0 && !Before.Hlsl.IsEmpty());

	// --- refusals: nothing changes ---
	const int32 CountBefore = Graph.Expressions().Num();
	TestTrue(TEXT("unknown output refused"), UMaterialNodeService::PromoteToNamedReroute(Graph.Path, Color->GetName(), TEXT("Nope"), TEXT("X")).Name.IsEmpty());
	TestTrue(TEXT("source without consumers refused"), UMaterialNodeService::PromoteToNamedReroute(Graph.Path, Unwired->GetName(), TEXT(""), TEXT("X")).Name.IsEmpty());
	TestTrue(TEXT("source of another asset refused"), UMaterialNodeService::PromoteToNamedReroute(Graph.Path, Foreign->GetPathName(), TEXT(""), TEXT("X")).Name.IsEmpty());
	TestEqual(TEXT("refusals leave the graph unchanged"), Graph.Expressions().Num(), CountBefore);

	// --- promote output 1 only ---
	const FVibeNamedRerouteInfo Info = UMaterialNodeService::PromoteToNamedReroute(Graph.Path, Color->GetName(), TEXT("Output_1"), TEXT("Red"));
	TestEqual(TEXT("two R consumers rewired; the masked one is not"), Info.UsageIds.Num(), 2);
	const TArray<UMaterialExpressionNamedRerouteDeclaration*> Decls = Graph.All<UMaterialExpressionNamedRerouteDeclaration>();
	if (!TestEqual(TEXT("one declaration"), Decls.Num(), 1))
	{
		return false;
	}
	const UMaterialExpressionNamedRerouteDeclaration* Decl = Decls[0];
	TestTrue(TEXT("declaration reads output 1 with the output's mask"),
		Decl->Input.Expression == Color && Decl->Input.OutputIndex == 1 && Decl->Input.Mask == 1 && Decl->Input.MaskR == 1 && Decl->Input.MaskG == 0);
	TestTrue(TEXT("R consumers read the variable with their mask reset"),
		DeclarationBehind(&MulR1->A) == Decl && DeclarationBehind(&MulR2->A) == Decl && MulR1->A.Mask == 0 && MulR2->A.Mask == 0);
	TestTrue(TEXT("output 0 consumer stays on the source"), AddFull->A.Expression == Color && AddFull->A.OutputIndex == 0);
	TestTrue(TEXT("masked consumer skipped: still on output 1 with its own G mask"),
		AddMasked->A.Expression == Color && AddMasked->A.OutputIndex == 1 && AddMasked->A.Mask == 1 && AddMasked->A.MaskG == 1 && AddMasked->A.MaskR == 0);

	// --- refusals on named reroutes themselves ---
	const int32 CountAfterPromote = Graph.Expressions().Num();
	TestTrue(TEXT("a declaration source is refused"), UMaterialNodeService::PromoteToNamedReroute(Graph.Path, Decl->GetName(), TEXT(""), TEXT("X")).Name.IsEmpty());
	const TArray<UMaterialExpressionNamedRerouteUsage*> Usages = Graph.All<UMaterialExpressionNamedRerouteUsage>();
	TestTrue(TEXT("a usage source is refused"), Usages.Num() > 0 && UMaterialNodeService::PromoteToNamedReroute(Graph.Path, Usages[0]->GetName(), TEXT(""), TEXT("X")).Name.IsEmpty());
	TestEqual(TEXT("reroute refusals leave the graph unchanged"), Graph.Expressions().Num(), CountAfterPromote);

	const FCompileSnapshot After = TakeCompileSnapshot(Graph.Material, Graph.Path);
	const bool bIdentical = CompareSnapshots(*this, TEXT("masks"), Before, After);
	const FString Receipt = FString::Printf(TEXT("PROMOTE kind=M-masks usages=%d diag_before=%d diag_after=%d identical=%s"),
		Info.UsageIds.Num(), Before.Errors.Num(), After.Errors.Num(), bIdentical ? TEXT("true") : TEXT("false"));
	AddInfo(Receipt);
	UE_LOG(LogTemp, Display, TEXT("%s"), *Receipt);
	return true;
}

// ---------------------------------------------------------------------------------------------
// A knot (Reroute) source converts: the declaration takes the knot's input and place, every knot consumer
// (two inputs of one node included) gets a usage, the knot is deleted. Material and MaterialFunction.
// ---------------------------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeNamedReroutePromoteKnotTest,
	"VibeUE.MaterialNodeService.NamedReroute.PromoteKnot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibeNamedReroutePromoteKnotTest::RunTest(const FString&)
{
	for (const bool bFunction : {false, true})
	{
		const TCHAR* Kind = bFunction ? TEXT("MaterialFunction") : TEXT("Material");
		FScratchGraph Graph(bFunction ? TEXT("MF_NamedReroutePromoteKnot") : TEXT("M_NamedReroutePromoteKnot"), bFunction);
		if (!TestTrue(FString::Printf(TEXT("%s: scratch asset created"), Kind), Graph.IsValid()))
		{
			return false;
		}
		UMaterialExpressionConstant3Vector* Color = Cast<UMaterialExpressionConstant3Vector>(Graph.NewExpression(UMaterialExpressionConstant3Vector::StaticClass(), -1000, 0));
		UMaterialExpressionReroute* Knot = Cast<UMaterialExpressionReroute>(Graph.NewExpression(UMaterialExpressionReroute::StaticClass(), -700, 40));
		UMaterialExpressionAdd* Add1 = Cast<UMaterialExpressionAdd>(Graph.NewExpression(UMaterialExpressionAdd::StaticClass(), -400, -100));
		UMaterialExpressionAdd* Add2 = Cast<UMaterialExpressionAdd>(Graph.NewExpression(UMaterialExpressionAdd::StaticClass(), -400, 150));
		if (!TestTrue(FString::Printf(TEXT("%s: nodes created"), Kind), Color && Knot && Add1 && Add2))
		{
			return false;
		}
		Color->Constant = FLinearColor(0.3f, 0.1f, 0.7f);
		Knot->Input.Connect(0, Color);
		Add1->A.Connect(0, Knot);
		Add1->B.Connect(0, Knot);
		Add2->A.Connect(0, Add1);
		Add2->B.Connect(0, Knot);
		FExpressionInput* FourthConsumer = nullptr;
		if (!bFunction)
		{
			TestTrue(TEXT("Material: Add2 -> BaseColor"), UMaterialNodeService::ConnectExpressionToOutput(Graph.Path, Add2->GetPathName(), TEXT(""), TEXT("BaseColor")));
			TestTrue(TEXT("Material: knot -> EmissiveColor"), UMaterialNodeService::ConnectExpressionToOutput(Graph.Path, Knot->GetPathName(), TEXT(""), TEXT("EmissiveColor")));
			FourthConsumer = Graph.Material->GetExpressionInputForProperty(MP_EmissiveColor);
		}
		else
		{
			UMaterialExpressionFunctionOutput* Result = NewFunctionOutput(Graph, TEXT("Result"), -100, 0, Add2);
			UMaterialExpressionFunctionOutput* Sum = NewFunctionOutput(Graph, TEXT("Sum"), -100, 300, Knot);
			if (!TestTrue(TEXT("MaterialFunction: outputs created"), Result && Sum))
			{
				return false;
			}
			FourthConsumer = &Sum->A;
		}
		FCompileHost Host;
		if (!TestTrue(FString::Printf(TEXT("%s: compile host"), Kind), MakeCompileHost(Graph, TEXT("M_NamedReroutePromoteKnotCaller"), Host)))
		{
			return false;
		}
		const FCompileSnapshot Before = TakeCompileSnapshot(Host.Material, Host.Path);
		TestTrue(FString::Printf(TEXT("%s: compiles clean before promote (control)"), Kind), Before.bCompiledOk && Before.Errors.Num() == 0 && !Before.Hlsl.IsEmpty());
		const int32 CountBefore = Graph.Expressions().Num();

		const FVibeNamedRerouteInfo Info = UMaterialNodeService::PromoteToNamedReroute(Graph.Path, Knot->GetName(), TEXT(""), TEXT("Base"));
		TestEqual(FString::Printf(TEXT("%s: four knot consumers become usages"), Kind), Info.UsageIds.Num(), 4);
		TestEqual(FString::Printf(TEXT("%s: knot removed"), Kind), Graph.All<UMaterialExpressionReroute>().Num(), 0);
		TestEqual(FString::Printf(TEXT("%s: -1 knot +1 declaration +4 usages"), Kind), Graph.Expressions().Num(), CountBefore + 4);
		const TArray<UMaterialExpressionNamedRerouteDeclaration*> Decls = Graph.All<UMaterialExpressionNamedRerouteDeclaration>();
		if (!TestEqual(FString::Printf(TEXT("%s: one declaration"), Kind), Decls.Num(), 1))
		{
			return false;
		}
		const UMaterialExpressionNamedRerouteDeclaration* Decl = Decls[0];
		TestTrue(FString::Printf(TEXT("%s: declaration takes the knot's input and position"), Kind),
			Decl->Input.Expression == Color && Decl->MaterialExpressionEditorX == -700 && Decl->MaterialExpressionEditorY == 40);
		TestTrue(FString::Printf(TEXT("%s: every former knot consumer reads the variable"), Kind),
			DeclarationBehind(&Add1->A) == Decl && DeclarationBehind(&Add1->B) == Decl && DeclarationBehind(&Add2->B) == Decl && DeclarationBehind(FourthConsumer) == Decl);
		TestTrue(FString::Printf(TEXT("%s: Add1.A and Add1.B got distinct usages"), Kind), Add1->A.Expression != Add1->B.Expression);

		const FCompileSnapshot After = TakeCompileSnapshot(Host.Material, Host.Path);
		const bool bIdentical = CompareSnapshots(*this, Kind, Before, After);
		const FString Receipt = FString::Printf(TEXT("PROMOTE kind=%s-knot usages=%d diag_before=%d diag_after=%d identical=%s"),
			bFunction ? TEXT("MF") : TEXT("M"), Info.UsageIds.Num(), Before.Errors.Num(), After.Errors.Num(), bIdentical ? TEXT("true") : TEXT("false"));
		AddInfo(Receipt);
		UE_LOG(LogTemp, Display, TEXT("%s"), *Receipt);
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// Stale-pin risk: with a live UMaterialGraph on the Material (built from the pre-promote wiring), the refresh
// must not copy the old pin links back over the expression-level rewire.
// ---------------------------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeNamedReroutePromoteMaterialGraphTest,
	"VibeUE.MaterialNodeService.NamedReroute.PromoteWithMaterialGraph",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibeNamedReroutePromoteMaterialGraphTest::RunTest(const FString&)
{
	FScratchGraph Graph(TEXT("M_NamedReroutePromoteGraph"), /*bFunction*/ false);
	if (!TestTrue(TEXT("scratch asset created"), Graph.IsValid()))
	{
		return false;
	}
	UMaterialExpressionConstant3Vector* Color = Cast<UMaterialExpressionConstant3Vector>(Graph.NewExpression(UMaterialExpressionConstant3Vector::StaticClass(), -900, 0));
	UMaterialExpressionMultiply* Mul = Cast<UMaterialExpressionMultiply>(Graph.NewExpression(UMaterialExpressionMultiply::StaticClass(), -700, 0));
	UMaterialExpressionAdd* Add1 = Cast<UMaterialExpressionAdd>(Graph.NewExpression(UMaterialExpressionAdd::StaticClass(), -400, -100));
	UMaterialExpressionAdd* Add2 = Cast<UMaterialExpressionAdd>(Graph.NewExpression(UMaterialExpressionAdd::StaticClass(), -400, 150));
	if (!TestTrue(TEXT("nodes created"), Color && Mul && Add1 && Add2))
	{
		return false;
	}
	Color->Constant = FLinearColor(0.5f, 0.25f, 0.125f);
	Mul->A.Connect(0, Color);
	Mul->B.Connect(0, Color);
	Add1->A.Connect(0, Mul);
	Add1->B.Connect(0, Color);
	Add2->A.Connect(0, Add1);
	Add2->B.Connect(0, Mul);
	TestTrue(TEXT("Add2 -> BaseColor"), UMaterialNodeService::ConnectExpressionToOutput(Graph.Path, Add2->GetPathName(), TEXT(""), TEXT("BaseColor")));
	TestTrue(TEXT("Mul -> EmissiveColor"), UMaterialNodeService::ConnectExpressionToOutput(Graph.Path, Mul->GetPathName(), TEXT(""), TEXT("EmissiveColor")));

	// The graph the material editor would hold, built from the current (pre-promote) expression links.
	Graph.Material->MaterialGraph = CastChecked<UMaterialGraph>(FBlueprintEditorUtils::CreateNewGraph(
		Graph.Material, NAME_None, UMaterialGraph::StaticClass(), UMaterialGraphSchema::StaticClass()));
	Graph.Material->MaterialGraph->Material = Graph.Material;
	Graph.Material->MaterialGraph->RebuildGraph();
	ON_SCOPE_EXIT { if (Graph.Material) { Graph.Material->MaterialGraph = nullptr; } };
	if (!TestTrue(TEXT("graph nodes exist for the source and consumers (control)"), Mul->GraphNode && Add1->GraphNode && Add2->GraphNode))
	{
		return false;
	}

	const FCompileSnapshot Before = TakeCompileSnapshot(Graph.Material, Graph.Path);
	const FVibeNamedRerouteInfo Info = UMaterialNodeService::PromoteToNamedReroute(Graph.Path, Mul->GetName(), TEXT(""), TEXT("Product"));
	TestEqual(TEXT("three consumers rewired"), Info.UsageIds.Num(), 3);
	const TArray<UMaterialExpressionNamedRerouteDeclaration*> Decls = Graph.All<UMaterialExpressionNamedRerouteDeclaration>();
	const UMaterialExpressionNamedRerouteDeclaration* Decl = Decls.Num() == 1 ? Decls[0] : nullptr;
	int32 Rewired = 0;
	Rewired += (Decl && DeclarationBehind(&Add1->A) == Decl) ? 1 : 0;
	Rewired += (Decl && DeclarationBehind(&Add2->B) == Decl) ? 1 : 0;
	Rewired += (Decl && DeclarationBehind(Graph.Material->GetExpressionInputForProperty(MP_EmissiveColor)) == Decl) ? 1 : 0;
	TestEqual(TEXT("rewires survive the graph refresh (no stale pins copied back)"), Rewired, 3);
	bool bUsagesHaveNodes = true;
	for (const UMaterialExpressionNamedRerouteUsage* Usage : Graph.All<UMaterialExpressionNamedRerouteUsage>())
	{
		bUsagesHaveNodes &= Usage->GraphNode != nullptr;
	}
	TestTrue(TEXT("the graph was rebuilt from the expressions (usages have graph nodes)"), Decl && Decl->GraphNode && bUsagesHaveNodes);

	const FCompileSnapshot After = TakeCompileSnapshot(Graph.Material, Graph.Path);
	const bool bIdentical = CompareSnapshots(*this, TEXT("graph"), Before, After);
	const FString Receipt = FString::Printf(TEXT("PROMOTE-GRAPH material_graph=non-null rewired=%d/3 usage_nodes=%s identical=%s"),
		Rewired, bUsagesHaveNodes ? TEXT("true") : TEXT("false"), bIdentical ? TEXT("true") : TEXT("false"));
	AddInfo(Receipt);
	UE_LOG(LogTemp, Display, TEXT("%s"), *Receipt);
	return true;
}

// ---------------------------------------------------------------------------------------------
// Readability pass: a fanout-4 node and a 2000-unit parameter wire are the only 2 candidates (a 2000-unit
// constant wire and every fanout-1 short wire are not); the dry run changes nothing; applying promotes both
// with the compiled result unchanged. Material and MaterialFunction.
// ---------------------------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeNamedRerouteReadabilityTest,
	"VibeUE.MaterialNodeService.NamedReroute.Readability",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibeNamedRerouteReadabilityTest::RunTest(const FString&)
{
	for (const bool bFunction : {false, true})
	{
		const TCHAR* Kind = bFunction ? TEXT("MaterialFunction") : TEXT("Material");
		FScratchGraph Graph(bFunction ? TEXT("MF_NamedRerouteReadability") : TEXT("M_NamedRerouteReadability"), bFunction);
		if (!TestTrue(FString::Printf(TEXT("%s: scratch asset created"), Kind), Graph.IsValid()))
		{
			return false;
		}
		UMaterialExpressionConstant3Vector* Color = Cast<UMaterialExpressionConstant3Vector>(Graph.NewExpression(UMaterialExpressionConstant3Vector::StaticClass(), -1300, 0));
		UMaterialExpressionConstant* Scale = Cast<UMaterialExpressionConstant>(Graph.NewExpression(UMaterialExpressionConstant::StaticClass(), -1300, 150));
		UMaterialExpressionMultiply* Hub = Cast<UMaterialExpressionMultiply>(Graph.NewExpression(UMaterialExpressionMultiply::StaticClass(), -1000, 0));
		UMaterialExpressionConstant* Bias = Cast<UMaterialExpressionConstant>(Graph.NewExpression(UMaterialExpressionConstant::StaticClass(), -900, -300));
		UMaterialExpressionAdd* Add1 = Cast<UMaterialExpressionAdd>(Graph.NewExpression(UMaterialExpressionAdd::StaticClass(), -600, -300));
		UMaterialExpressionAdd* Add2 = Cast<UMaterialExpressionAdd>(Graph.NewExpression(UMaterialExpressionAdd::StaticClass(), -600, -100));
		UMaterialExpressionAdd* Add3 = Cast<UMaterialExpressionAdd>(Graph.NewExpression(UMaterialExpressionAdd::StaticClass(), -600, 100));
		UMaterialExpressionAdd* Add4 = Cast<UMaterialExpressionAdd>(Graph.NewExpression(UMaterialExpressionAdd::StaticClass(), -600, 300));
		UMaterialExpressionScalarParameter* Far = Cast<UMaterialExpressionScalarParameter>(Graph.NewExpression(UMaterialExpressionScalarParameter::StaticClass(), -2600, 600));
		UMaterialExpressionMultiply* Mul2 = Cast<UMaterialExpressionMultiply>(Graph.NewExpression(UMaterialExpressionMultiply::StaticClass(), -600, 600));
		UMaterialExpressionConstant* FarConst = Cast<UMaterialExpressionConstant>(Graph.NewExpression(UMaterialExpressionConstant::StaticClass(), -2600, 800));
		UMaterialExpressionMultiply* Mul3 = Cast<UMaterialExpressionMultiply>(Graph.NewExpression(UMaterialExpressionMultiply::StaticClass(), -300, 800));
		if (!TestTrue(FString::Printf(TEXT("%s: nodes created"), Kind), Color && Scale && Hub && Bias && Add1 && Add2 && Add3 && Add4 && Far && Mul2 && FarConst && Mul3))
		{
			return false;
		}
		Color->Constant = FLinearColor(0.2f, 0.4f, 0.6f);
		Scale->R = 2.f;
		Bias->R = 0.1f;
		Far->ParameterName = TEXT("FarScale");
		Far->DefaultValue = 0.5f;
		FarConst->R = 0.75f;
		Hub->A.Connect(0, Color);
		Hub->B.Connect(0, Scale);
		Add1->A.Connect(0, Hub);
		Add1->B.Connect(0, Bias);
		Add2->A.Connect(0, Add1);
		Add2->B.Connect(0, Hub);
		Add3->A.Connect(0, Add2);
		Add3->B.Connect(0, Hub);
		Add4->A.Connect(0, Add3);
		Add4->B.Connect(0, Hub);
		Mul2->A.Connect(0, Add4);
		Mul2->B.Connect(0, Far);      // 2000-unit wire from a parameter: candidate
		Mul3->A.Connect(0, Mul2);
		Mul3->B.Connect(0, FarConst); // 2000-unit wire from a constant feeding one node: skipped
		if (!bFunction)
		{
			TestTrue(TEXT("Material: Mul3 -> BaseColor"), UMaterialNodeService::ConnectExpressionToOutput(Graph.Path, Mul3->GetPathName(), TEXT(""), TEXT("BaseColor")));
		}
		else if (!TestNotNull(TEXT("MaterialFunction: output created"), NewFunctionOutput(Graph, TEXT("Result"), -100, 800, Mul3)))
		{
			return false;
		}
		FCompileHost Host;
		if (!TestTrue(FString::Printf(TEXT("%s: compile host"), Kind), MakeCompileHost(Graph, TEXT("M_NamedRerouteReadabilityCaller"), Host)))
		{
			return false;
		}
		const FCompileSnapshot Before = TakeCompileSnapshot(Host.Material, Host.Path);
		TestTrue(FString::Printf(TEXT("%s: compiles clean before the pass (control)"), Kind), Before.bCompiledOk && Before.Errors.Num() == 0 && !Before.Hlsl.IsEmpty());
		const int32 CountBefore = Graph.Expressions().Num();

		// --- dry run: the finder ---
		const TArray<FVibeRerouteCandidate> Dry = UMaterialNodeService::ApplyRerouteReadabilityPass(Graph.Path);
		TestEqual(FString::Printf(TEXT("%s: dry run changes nothing"), Kind), Graph.Expressions().Num(), CountBefore);
		if (!TestEqual(FString::Printf(TEXT("%s: exactly two candidates"), Kind), Dry.Num(), 2))
		{
			for (const FVibeRerouteCandidate& Row : Dry)
			{
				AddInfo(FString::Printf(TEXT("candidate %s fanout=%d wire=%.0f"), *Row.SuggestedName, Row.Fanout, Row.MaxWireLength));
			}
			return false;
		}
		// Ordered by source PosY: Hub (y=0) before FarScale (y=600).
		TestTrue(FString::Printf(TEXT("%s: row 0 = the fanout-4 Multiply"), Kind),
			Dry[0].SourceId == SessionId(Hub) && Dry[0].Fanout == 4 && Dry[0].SuggestedName == TEXT("Multiply_Output_0") && !Dry[0].bApplied);
		TestTrue(FString::Printf(TEXT("%s: row 1 = the long FarScale wire, named after the parameter"), Kind),
			Dry[1].SourceId == SessionId(Far) && Dry[1].Fanout == 1 && FMath::IsNearlyEqual(Dry[1].MaxWireLength, 2000.f, 1.f) && Dry[1].SuggestedName == TEXT("FarScale"));

		// --- apply ---
		const TArray<FVibeRerouteCandidate> Applied = UMaterialNodeService::ApplyRerouteReadabilityPass(Graph.Path, /*bDryRun*/ false);
		int32 NumApplied = 0;
		for (const FVibeRerouteCandidate& Row : Applied)
		{
			NumApplied += Row.bApplied ? 1 : 0;
		}
		TestEqual(FString::Printf(TEXT("%s: both candidates applied"), Kind), NumApplied, 2);
		const FVibeNamedRerouteList List = UMaterialNodeService::ListNamedReroutes(Graph.Path);
		TestEqual(FString::Printf(TEXT("%s: two declarations"), Kind), List.Declarations.Num(), 2);
		TestTrue(FString::Printf(TEXT("%s: applied names are the final declaration names"), Kind),
			Applied.Num() == 2 && Applied[0].AppliedName == TEXT("Multiply_Output_0") && Applied[1].AppliedName == TEXT("FarScale"));
		TestEqual(FString::Printf(TEXT("%s: +2 declarations +5 usages"), Kind), Graph.Expressions().Num(), CountBefore + 7);
		TestEqual(FString::Printf(TEXT("%s: a second dry run finds nothing left"), Kind), UMaterialNodeService::ApplyRerouteReadabilityPass(Graph.Path).Num(), 0);

		const FCompileSnapshot After = TakeCompileSnapshot(Host.Material, Host.Path);
		const bool bIdentical = CompareSnapshots(*this, Kind, Before, After);
		const FString Receipt = FString::Printf(TEXT("READABILITY kind=%s candidates=%d applied=%d diag_identical=%s"),
			bFunction ? TEXT("MF") : TEXT("M"), Dry.Num(), NumApplied, bIdentical ? TEXT("true") : TEXT("false"));
		AddInfo(Receipt);
		UE_LOG(LogTemp, Display, TEXT("%s"), *Receipt);
	}
	return true;
}

#endif // WITH_AUTOMATION_TESTS
