// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "UMaterialService.h"
#include "UMaterialNodeService.generated.h"

class UMaterial;
class UMaterialExpression;
class UMaterialFunction;
struct FExpressionInput;

/**
 * Information about a single input or output on a Material Function
 */
USTRUCT(BlueprintType)
struct FMaterialFunctionPinInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString Name;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString Description;

	/** Numeric input type (EFunctionInputType: 0=Scalar, 1=Vector2, 2=Vector3, 3=Vector4, 4=Texture2D, etc.) */
	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	int32 InputType = 0;

	/** Human-readable type name (e.g., "Scalar", "Vector3", "Texture2D") */
	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString InputTypeName;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	int32 SortPriority = 0;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString Id;
};

/**
 * Information about a Material Function's interface
 */
USTRUCT(BlueprintType)
struct FVibeUEMaterialFunctionInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString FunctionPath;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString Description;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	TArray<FMaterialFunctionPinInfo> Inputs;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	TArray<FMaterialFunctionPinInfo> Outputs;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	int32 ExpressionCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	bool bExposeToLibrary = false;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	TArray<FString> LibraryCategories;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString ErrorMessage;
};

/**
 * Material expression type information for discovery
 */
USTRUCT(BlueprintType)
struct FMaterialExpressionTypeInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString ClassName;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString DisplayName;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString Category;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString Description;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	bool bIsParameter = false;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	TArray<FString> Keywords;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	TArray<FString> InputNames;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	TArray<FString> OutputNames;
};

/**
 * Information about a material expression (node) in the material graph
 */
USTRUCT(BlueprintType)
struct FMaterialExpressionInfo
{
	GENERATED_BODY()

	/** Session-specific id ("<Class>_<pointer>"). Stable within one editor session but NOT across
	 *  restarts — persist ObjectPath if you need a durable reference. Accepted by the id-based lookups. */
	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString Id;

	/** Full object path of this expression (e.g. "/Game/M.M:MaterialExpressionConstant3Vector_0").
	 *  Session-independent; also accepted anywhere an ExpressionId is taken. */
	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString ObjectPath;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString ClassName;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString DisplayName;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString Category;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	int32 PosX = 0;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	int32 PosY = 0;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString Description;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	bool bIsParameter = false;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString ParameterName;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	TArray<FString> InputNames;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	TArray<FString> OutputNames;
};

/**
 * Information about an input or output pin on a material expression
 */
USTRUCT(BlueprintType)
struct FMaterialNodePinInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString Name;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	int32 Index = 0;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString Direction;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString ValueType;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	bool bIsConnected = false;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString ConnectedExpressionId;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	int32 ConnectedOutputIndex = -1;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString DefaultValue;
};

/**
 * Information about a connection between material expressions
 */
USTRUCT(BlueprintType)
struct FMaterialNodeConnectionInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString SourceExpressionId;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString SourceOutput;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString TargetExpressionId;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString TargetInput;
};

/**
 * Expression property info
 */
USTRUCT(BlueprintType)
struct FMaterialNodePropertyInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString Name;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString Value;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString PropertyType;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	bool bIsEditable = true;
};

/**
 * Material output connection info
 */
USTRUCT(BlueprintType)
struct FMaterialOutputConnectionInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString PropertyName;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString ConnectedExpressionId;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	bool bIsConnected = false;
};

/**
 * Descriptor for batch-creating specialized expressions (function calls, custom HLSL, collection params).
 * Used with BatchCreateSpecialized to create all node types in a single transaction.
 */
USTRUCT(BlueprintType)
struct FBatchCreateDescriptor
{
	GENERATED_BODY()

	/** Expression class name (e.g., "Multiply", "MaterialFunctionCall", "Custom", "CollectionParameter") */
	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString ClassName;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	int32 PosX = 0;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	int32 PosY = 0;

	/** For MaterialFunctionCall: path to the material function asset */
	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString FunctionPath;

	/** For Custom: HLSL code string */
	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString HLSLCode;

	/** For Custom: output type ("CMOT_Float1", "CMOT_Float2", "CMOT_Float3", "CMOT_Float4", "CMOT_MaterialAttributes") */
	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString HLSLOutputType;

	/** For Custom: display description */
	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString HLSLDescription;

	/** For Custom: comma-separated input names */
	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString HLSLInputNames;

	/** For Custom: semicolon-separated additional outputs, each as "Name:Type" (e.g., "WorldHeight:Float1;Mask:Float3") */
	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString HLSLAdditionalOutputs;

	/** For CollectionParameter: path to the MaterialParameterCollection asset */
	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString CollectionPath;

	/** For CollectionParameter: parameter name within the collection */
	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString CollectionParamName;
};

/**
 * One named-reroute declaration ("graph variable") in a Material or MaterialFunction, with the usages
 * linked to it. Durable keys are Name and VariableGuid; the ids are session-specific
 * ("<Class>_<pointer>") and must not be compared across an editor reload.
 */
USTRUCT(BlueprintType)
struct FVibeNamedRerouteInfo
{
	GENERATED_BODY()

	/** Final declaration name (after MakeNameUnique; may carry a _N suffix). */
	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString Name;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FLinearColor NodeColor = FLinearColor::White;

	/** The declaration's VariableGuid as a string (survives rename; Python cannot read the raw property). */
	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString VariableGuid;

	/** Session id of the declaration expression. */
	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString DeclarationId;

	/** Object path of the declaration expression (durable; also accepted as a node ref). */
	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString DeclarationObjectPath;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	int32 PosX = 0;

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	int32 PosY = 0;

	/** Session id of the expression wired into the declaration's input (empty when unwired). */
	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString SourceId;

	/** Name of the source expression's output pin feeding the declaration (empty when unwired). */
	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString SourceOutputName;

	/** Session ids of every usage linked to this declaration. */
	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	TArray<FString> UsageIds;
};

/**
 * Result of list_named_reroutes: every declaration of one Material / MaterialFunction plus the
 * usages whose declaration is missing or invalid.
 */
USTRUCT(BlueprintType)
struct FVibeNamedRerouteList
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	TArray<FVibeNamedRerouteInfo> Declarations;

	/** Session ids of usages whose Declaration is null, deleted or not in this asset. */
	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	TArray<FString> BrokenUsageIds;
};

/**
 * One result row of apply_reroute_readability_pass: a node OUTPUT that is a good named-reroute candidate
 * (high fanout or a long wire), and, when the pass was applied, the variable it became.
 */
USTRUCT(BlueprintType)
struct FVibeRerouteCandidate
{
	GENERATED_BODY()

	/** Session id of the source expression (not durable across a reload). */
	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString SourceId;

	/** Output pin of the source ("Output_0" style for an unnamed output). */
	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString SourceOutputName;

	/** Consumer inputs promote would rewire (node inputs plus material properties reading this output). */
	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	int32 Fanout = 0;

	/** Longest source-to-consumer distance in graph units (node editor positions). */
	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	float MaxWireLength = 0.f;

	/** Parameter name for a parameter source, else "<ClassShortName>_<OutputName>". */
	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString SuggestedName;

	/** True when the (non-dry-run) pass promoted this candidate. */
	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	bool bApplied = false;

	/** Final declaration name after MakeNameUnique (empty unless applied). */
	UPROPERTY(BlueprintReadWrite, Category = "MaterialNode")
	FString AppliedName;
};

/**
 * The graph a named-reroute call operates on: exactly one of Material / Function is set. Plain C++
 * (not reflected); resolved from an asset path by UMaterialNodeService::ResolveGraphOwner.
 */
struct FVibeMaterialGraphOwner
{
	UMaterial* Material = nullptr;
	UMaterialFunction* Function = nullptr;

	bool IsValid() const { return Material != nullptr || Function != nullptr; }
	UObject* Asset() const;
	TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions() const;
	bool Owns(const UMaterialExpression* Expression) const;
};

class UMaterialExpressionNamedRerouteDeclaration;
class FJsonObject;

/**
 * Material Node Service - Python API for material graph manipulation
 *
 * Provides 44 material node management actions. Single-node create / delete / move and auto layout
 * are NOT in this service (use batch_create_expressions / create_parameter here, or MaterialEditingLibrary):
 *
 * Discovery:
 * - discover_types: Find available material expression types
 * - get_categories: Get expression categories
 *
 * Specialized Creation:
 * - create_function_call: Create a MaterialFunctionCall node referencing a function asset
 * - create_custom_expression: Create a Custom HLSL expression node
 * - create_collection_parameter: Create a CollectionParameter referencing a parameter collection
 *
 * Information:
 * - list_expressions: List all expressions in material
 * - get_expression_details: Get detailed expression info
 * - get_expression_pins: Get expression pin information
 * - get_material_diagnostics: Compile status, errors and the textures the shader actually references
 *
 * Connections:
 * - list_connections: List all connections
 * - batch_connect_expressions: Connect multiple expression pairs (expression-to-expression)
 * - connect_expression_to_output: Connect an expression output to a material output (BaseColor, Normal, ...)
 * - disconnect_output: Clear a material output's input
 * - cleanup_unused_expressions: Delete expressions unreachable from any output (Material or MaterialFunction); returns the count
 *
 * Properties:
 * - get_expression_property: Get expression property value
 * - set_expression_property: Set expression property value
 * - list_expression_properties: List all expression properties
 *
 * Parameters:
 * - create_parameter: Create parameter expression (Scalar, Vector, Texture, TextureObject, StaticBool, StaticSwitch)
 * - promote_to_parameter: Promote constant to parameter
 * - set_parameter_metadata: Set parameter group/priority
 *
 * Batch Operations:
 * - batch_create_expressions: Create multiple expressions in one call
 * - batch_set_properties: Set properties on multiple expressions in one call
 * - batch_create_specialized: Create all expression types (including function calls, custom HLSL, collection params) in one call
 *
 * Export:
 * - export_material_graph: Export complete material graph as JSON for recreation
 * - export_material_graph_summary: Export lightweight summary (counts, types) without full property data
 * - export_function_graph: Export a material function's graph as JSON
 * - compare_material_graphs: Compare two material graphs and report differences
 *
 * Material Functions:
 * - create_material_function: Create a material function asset
 * - get_function_info: Inputs / outputs of a material function
 * - add_function_input: Add a FunctionInput to a material function
 * - add_function_output: Add a FunctionOutput to a material function
 * - create_function_expression: Create an expression inside a material function
 * - set_function_expression_property: Set a property on an expression inside a material function
 * - connect_function_expressions: Connect two expressions inside a material function
 *
 * Named reroutes (graph variables; Material or MaterialFunction, addressed by AssetPath):
 * - list_named_reroutes: Every declaration with its linked usages, plus broken usages
 * - create_named_reroute_declaration: Create a declaration (optionally wired from a source node)
 * - create_named_reroute_usage: Create a usage linked to a declaration by name (optionally wired into a node input)
 * - rename_named_reroute_declaration: Rename a declaration; usages stay linked and refresh
 * - link_named_reroute_usage: Link an existing usage node to an existing declaration node
 * - get_named_reroute_definition: One variable as JSON: its declaration, the subgraph that generates it, its inputs and consumers
 * - promote_to_named_reroute: Turn one node output with N consumers into 1 declaration + N usages (compiled result unchanged; knots convert)
 * - apply_reroute_readability_pass: Find high-fanout / long-wire outputs (dry run, default) or promote them all
 *
 * Material Outputs:
 * - get_output_properties: Get available material output pins
 * - get_output_connections: Get current output connections
 * 
 * Python Usage:
 *   import unreal
 * 
 *   # Discover expression types
 *   types = unreal.MaterialNodeService.discover_types("", "Constant", 20)
 * 
 *   # Create expressions (batch)
 *   nodes = unreal.MaterialNodeService.batch_create_expressions("/Game/M_Test", ["Constant3Vector"], [0], [0])
 *   expr = nodes[0]
 *
 *   # Connect an expression output to a material output.
 *   # expr.id is session-specific; expr.object_path is durable and also accepted here.
 *   unreal.MaterialNodeService.connect_expression_to_output("/Game/M_Test", expr.id, "", "BaseColor")
 *   # ... or clear it again:
 *   unreal.MaterialNodeService.disconnect_output("/Game/M_Test", "BaseColor")
 *
 *   # Create function call node
 *   func = unreal.MaterialNodeService.create_function_call("/Game/M_Test", "/Engine/Functions/MF_Noise", -500, 0)
 * 
 *   # Batch create expressions
 *   nodes = unreal.MaterialNodeService.batch_create_expressions("/Game/M_Test", ["Add","Multiply","Constant"], [0,0,0], [-300,-200,-100], [0,200,400])
 * 
 * note: This replaces the JSON-based manage_material_node MCP tool
 */

/**
 * Comprehensive material compile/sampler diagnostics. Use this to verify a material is
 * actually compiling and sampling textures — replaces the unreliable
 * `MaterialEditingLibrary.get_used_textures` signal which returns 0 for many graphs
 * that nonetheless render correctly.
 */
USTRUCT(BlueprintType)
struct FMaterialDiagnostics
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Materials")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Materials")
	bool bIsCompiledOk = false;

	/** Compile errors from the most recent compilation attempt. Empty when bIsCompiledOk. */
	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Materials")
	TArray<FString> CompileErrors;

	/** All texture asset paths the material actually references (after compile). Reliable. */
	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Materials")
	TArray<FString> ReferencedTexturePaths;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Materials")
	int32 ExpressionCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Materials")
	int32 TextureSampleCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Materials")
	int32 TextureObjectParameterCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Materials")
	int32 FunctionCallCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Materials")
	int32 ScalarParameterCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Materials")
	int32 VectorParameterCount = 0;
};

UCLASS()
class VIBEUE_API UMaterialNodeService : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * Comprehensive material diagnostic — compile status, errors, and the textures the
	 * shader actually references. Reliable replacement for `get_used_textures`.
	 *
	 * Use this after building a material graph to verify it compiles cleanly and samples
	 * the expected textures. Particularly useful for materials with `MaterialFunctionCall`
	 * nodes (triplanar / world-aligned), where `MaterialEditingLibrary.get_used_textures`
	 * silently returns 0.
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Materials", meta = (AICallable, DisplayName = "Get Material Diagnostics"))
	static FMaterialDiagnostics GetMaterialDiagnostics(const FString& MaterialPath);


	// =================================================================
	// Discovery Actions
	// =================================================================

	/**
	 * Discover available material expression types.
	 * Maps to action="discover_types"
	 * @param Category Optional category filter
	 * @param SearchTerm Optional search term (e.g., "Constant", "Parameter", "Texture")
	 * @param MaxResults Maximum results to return
	 * @return Array of expression type information
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "MaterialNode")
	static TArray<FMaterialExpressionTypeInfo> DiscoverTypes(
		const FString& Category = TEXT(""),
		const FString& SearchTerm = TEXT(""),
		int32 MaxResults = 100);

	/**
	 * Get all material expression categories.
	 * Maps to action="get_categories"
	 * @return Array of category names
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "MaterialNode")
	static TArray<FString> GetCategories();

	// =================================================================
	// Specialized Creation Actions
	// =================================================================

	/**
	 * Create a MaterialFunctionCall expression referencing a material function asset.
	 * Maps to action="create_function_call"
	 *
	 * The function reference is loaded and set automatically, which creates the
	 * correct input/output pins matching the function's interface.
	 *
	 * @param MaterialPath Full path to the material
	 * @param FunctionPath Full path to the material function asset (e.g., "/Engine/Functions/Engine_MaterialFunctions03/Procedurals/NoiseFunctions")
	 * @param PosX X position in graph
	 * @param PosY Y position in graph
	 * @return Created expression info with function's inputs/outputs
	 *
	 * Example:
	 *   func = unreal.MaterialNodeService.create_function_call("/Game/M_Test", "/Game/MF_TerrainLayer", -500, 0)
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "MaterialNode")
	static FMaterialExpressionInfo CreateFunctionCall(
		const FString& MaterialPath,
		const FString& FunctionPath,
		int32 PosX = 0,
		int32 PosY = 0);

	/**
	 * Create a Custom HLSL expression node with code, output type, and optional inputs.
	 * Maps to action="create_custom_expression"
	 *
	 * @param MaterialPath Full path to the material
	 * @param Code HLSL code string (e.g., "return Input0 * 2.0;")
	 * @param OutputType Output type: "CMOT_Float1", "CMOT_Float2", "CMOT_Float3", "CMOT_Float4", "CMOT_MaterialAttributes"
	 * @param Description Display name for the node
	 * @param InputNames Comma-separated custom input names (e.g., "Albedo,Mask,UV") or empty for no custom inputs
	 * @param PosX X position in graph
	 * @param PosY Y position in graph
	 * @return Created expression info
	 *
	 * Example:
	 *   node = unreal.MaterialNodeService.create_custom_expression("/Game/M_Test", "return Input0 * 2;", "CMOT_Float3", "DoubleIt", "Color", -500, 0)
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "MaterialNode")
	static FMaterialExpressionInfo CreateCustomExpression(
		const FString& MaterialPath,
		const FString& Code,
		const FString& OutputType = TEXT("CMOT_Float3"),
		const FString& Description = TEXT("Custom"),
		const FString& InputNames = TEXT(""),
		int32 PosX = 0,
		int32 PosY = 0);

	/**
	 * Create a CollectionParameter expression referencing a MaterialParameterCollection.
	 * Maps to action="create_collection_parameter"
	 *
	 * @param MaterialPath Full path to the material
	 * @param CollectionPath Full path to the MaterialParameterCollection asset
	 * @param ParameterName Name of the parameter within the collection to use
	 * @param PosX X position in graph
	 * @param PosY Y position in graph
	 * @return Created expression info
	 *
	 * Example:
	 *   node = unreal.MaterialNodeService.create_collection_parameter("/Game/M_Test", "/Game/MPC_Weather", "WindStrength", -500, 0)
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "MaterialNode")
	static FMaterialExpressionInfo CreateCollectionParameter(
		const FString& MaterialPath,
		const FString& CollectionPath,
		const FString& ParameterName,
		int32 PosX = 0,
		int32 PosY = 0);

	// =================================================================
	// Information Actions
	// =================================================================

	/**
	 * List all expressions in a material.
	 * Maps to action="list"
	 * @param MaterialPath Full path to the material
	 * @return Array of expression info
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "MaterialNode")
	static TArray<FMaterialExpressionInfo> ListExpressions(const FString& MaterialPath);

	/**
	 * Get detailed information about an expression.
	 * Maps to action="get_details"
	 * @param MaterialPath Full path to the material
	 * @param ExpressionId ID of expression
	 * @param OutInfo Output expression info
	 * @return True if found
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "MaterialNode")
	static bool GetExpressionDetails(
		const FString& MaterialPath,
		const FString& ExpressionId,
		FMaterialExpressionInfo& OutInfo);

	/**
	 * Get all pins (inputs and outputs) for an expression.
	 * Maps to action="get_pins"
	 * @param MaterialPath Full path to the material
	 * @param ExpressionId ID of expression
	 * @return Array of pin info
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "MaterialNode")
	static TArray<FMaterialNodePinInfo> GetExpressionPins(
		const FString& MaterialPath,
		const FString& ExpressionId);

	// =================================================================
	// Connection Actions
	// =================================================================

	/**
	 * List all connections in a material.
	 * Maps to action="list_connections"
	 * @param MaterialPath Full path to the material
	 * @return Array of connection info
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "MaterialNode")
	static TArray<FMaterialNodeConnectionInfo> ListConnections(const FString& MaterialPath);

	// =================================================================
	// Property Actions
	// =================================================================

	/**
	 * Get a property value from an expression.
	 * Maps to action="get_property"
	 * @param MaterialPath Full path to the material
	 * @param ExpressionId ID of expression
	 * @param PropertyName Property name
	 * @return Property value as string
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "MaterialNode")
	static FString GetExpressionProperty(
		const FString& MaterialPath,
		const FString& ExpressionId,
		const FString& PropertyName);

	/**
	 * Set a property value on an expression.
	 * Maps to action="set_property"
	 * @param MaterialPath Full path to the material
	 * @param ExpressionId ID of expression
	 * @param PropertyName Property name
	 * @param PropertyValue Value as string
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "MaterialNode")
	static bool SetExpressionProperty(
		const FString& MaterialPath,
		const FString& ExpressionId,
		const FString& PropertyName,
		const FString& PropertyValue);

	/**
	 * List all editable properties on an expression.
	 * Maps to action="list_properties"
	 * @param MaterialPath Full path to the material
	 * @param ExpressionId ID of expression
	 * @return Array of property info
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "MaterialNode")
	static TArray<FMaterialNodePropertyInfo> ListExpressionProperties(
		const FString& MaterialPath,
		const FString& ExpressionId);

	// =================================================================
	// Parameter Actions
	// =================================================================

	/**
	 * Create a parameter expression directly.
	 * Maps to action="create_parameter"
	 * @param MaterialPath Full path to the material
	 * @param ParameterType Type: "Scalar", "Vector", "Texture", "TextureObject", "StaticBool", "StaticSwitch"
	 * @param ParameterName Name for the parameter
	 * @param GroupName Optional parameter group
	 * @param DefaultValue Default value as string. For Scalar: "0.5". For Vector/Color:
	 *        "(R=1.0,G=0.0,B=0.0,A=1.0)" or "1.0,0.0,0.0,1.0" or "1.0,0.0,0.0" (alpha defaults to 1.0)
	 *        or "1.0/0.0/0.0". For StaticBool/StaticSwitch: "true"/"false".
	 *        For Texture/TextureObject: asset path "/Game/Textures/T_Tex"
	 * @param PosX X position in graph
	 * @param PosY Y position in graph
	 * @return Created parameter info
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "MaterialNode")
	static FMaterialExpressionInfo CreateParameter(
		const FString& MaterialPath,
		const FString& ParameterType,
		const FString& ParameterName,
		const FString& GroupName = TEXT(""),
		const FString& DefaultValue = TEXT(""),
		int32 PosX = 0,
		int32 PosY = 0);

	/**
	 * Promote a constant expression to a parameter.
	 * Maps to action="promote_to_parameter"
	 * @param MaterialPath Full path to the material
	 * @param ExpressionId Expression to promote (must be Constant, Constant3Vector, etc.)
	 * @param ParameterName Name for the new parameter
	 * @param GroupName Optional parameter group
	 * @return New parameter expression info
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "MaterialNode")
	static FMaterialExpressionInfo PromoteToParameter(
		const FString& MaterialPath,
		const FString& ExpressionId,
		const FString& ParameterName,
		const FString& GroupName = TEXT(""));

	/**
	 * Set parameter metadata (group, sort priority).
	 * Maps to action="set_parameter_metadata"
	 * @param MaterialPath Full path to the material
	 * @param ExpressionId Parameter expression ID
	 * @param GroupName New group name
	 * @param SortPriority Sort priority within group
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "MaterialNode")
	static bool SetParameterMetadata(
		const FString& MaterialPath,
		const FString& ExpressionId,
		const FString& GroupName,
		int32 SortPriority = 0);

	// =================================================================
	// Material Output Actions
	// =================================================================

	/**
	 * Get available material output properties.
	 * Maps to action="get_output_properties"
	 * @param MaterialPath Full path to the material
	 * @return Array of property names (BaseColor, Metallic, Roughness, etc.)
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "MaterialNode")
	static TArray<FString> GetOutputProperties(const FString& MaterialPath);

	/**
	 * Get current connections to material outputs.
	 * Maps to action="get_output_connections"
	 * @param MaterialPath Full path to the material
	 * @return Array of output connection info
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "MaterialNode")
	static TArray<FMaterialOutputConnectionInfo> GetOutputConnections(const FString& MaterialPath);

	/**
	 * Connect an expression's output to one of the material's output properties
	 * (BaseColor, Normal, Roughness, ...) — the final step of "create nodes -> connect
	 * nodes -> connect to material output -> compile/save".
	 * Maps to action="connect_expression_to_output"
	 *
	 * This is the writer that closes the gap left by the expression-to-expression
	 * BatchConnectExpressions: previously a material output could only be wired with
	 * unreal.MaterialEditingLibrary.
	 *
	 * @param MaterialPath Full path to the material
	 * @param ExpressionId Source expression id (session id from create/list) OR its object_path
	 * @param OutputName Source output pin name; "" selects output 0. Unknown names are rejected.
	 * @param PropertyName Material output property. Accepts friendly ("BaseColor") and enum
	 *        ("MP_BaseColor") spellings, case-insensitive. Unknown names are rejected.
	 * @return True only after the connection is verified via a read-back. False (with a logged
	 *         reason and NO graph mutation) when the material, expression, output or property is invalid.
	 *
	 * Example:
	 *   nodes = unreal.MaterialNodeService.batch_create_expressions("/Game/M_Test", ["Constant3Vector"], [0], [0])
	 *   unreal.MaterialNodeService.connect_expression_to_output("/Game/M_Test", nodes[0].id, "", "BaseColor")
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "MaterialNode")
	static bool ConnectExpressionToOutput(
		const FString& MaterialPath,
		const FString& ExpressionId,
		const FString& OutputName,
		const FString& PropertyName);

	/**
	 * Clear the expression wired to a material output property.
	 * Maps to action="disconnect_output"
	 *
	 * Idempotent: returns true if the property was already empty. Returns false (with a logged
	 * reason and no graph mutation) only when the property name is unknown.
	 *
	 * @param MaterialPath Full path to the material
	 * @param PropertyName Material output property. Accepts "BaseColor" and "MP_BaseColor" spellings.
	 * @return True if the property's input is empty afterward.
	 *
	 * Example:
	 *   unreal.MaterialNodeService.disconnect_output("/Game/M_Test", "BaseColor")
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "MaterialNode")
	static bool DisconnectOutput(
		const FString& MaterialPath,
		const FString& PropertyName);

	// =================================================================
	// Batch Operations
	// =================================================================

	/**
	 * Batch-create multiple expressions in one call for performance.
	 * Maps to action="batch_create"
	 *
	 * All nodes are created in a single transaction with one graph refresh,
	 * yielding 10-100x speedup over individual create calls for large materials.
	 *
	 * @param MaterialPath Full path to the material
	 * @param ExpressionClasses Array of class names (e.g., ["Add", "Multiply", "Constant"])
	 * @param PosXArray Array of X positions (same length as ExpressionClasses)
	 * @param PosYArray Array of Y positions (same length as ExpressionClasses)
	 * @return Array of created expression info in same order
	 *
	 * Example:
	 *   nodes = unreal.MaterialNodeService.batch_create_expressions("/Game/M_Test",
	 *       ["Add","Multiply","Constant"], [-300,-200,-100], [0,200,400])
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "MaterialNode")
	static TArray<FMaterialExpressionInfo> BatchCreateExpressions(
		const FString& MaterialPath,
		const TArray<FString>& ExpressionClasses,
		const TArray<int32>& PosXArray,
		const TArray<int32>& PosYArray);

	/**
	 * Batch-connect multiple expression pairs in one call.
	 * Maps to action="batch_connect"
	 *
	 * All connections are made in a single transaction with one graph refresh.
	 *
	 * @param MaterialPath Full path to the material
	 * @param SourceIds Array of source expression IDs
	 * @param SourceOutputs Array of source output names (empty string for first output)
	 * @param TargetIds Array of target expression IDs
	 * @param TargetInputs Array of target input names
	 * @return Number of successful connections
	 *
	 * Example:
	 *   count = unreal.MaterialNodeService.batch_connect_expressions("/Game/M_Test",
	 *       [n1.id, n2.id], ["", ""], [n3.id, n3.id], ["A", "B"])
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "MaterialNode")
	static int32 BatchConnectExpressions(
		const FString& MaterialPath,
		const TArray<FString>& SourceIds,
		const TArray<FString>& SourceOutputs,
		const TArray<FString>& TargetIds,
		const TArray<FString>& TargetInputs);

	/**
	 * Batch-set properties on multiple expressions in one call.
	 * Maps to action="batch_set_properties"
	 *
	 * All property changes are made in a single transaction with one graph refresh.
	 * The arrays must all be the same length (each index is one set operation).
	 *
	 * @param MaterialPath Full path to the material
	 * @param ExpressionIds Array of expression IDs
	 * @param PropertyNames Array of property names
	 * @param PropertyValues Array of property values as strings
	 * @return Number of successful property sets
	 *
	 * Example:
	 *   count = unreal.MaterialNodeService.batch_set_properties("/Game/M_Test",
	 *       [n1.id, n1.id, n2.id], ["R", "G", "Bias"], ["True", "False", "0.5"])
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "MaterialNode")
	static int32 BatchSetProperties(
		const FString& MaterialPath,
		const TArray<FString>& ExpressionIds,
		const TArray<FString>& PropertyNames,
		const TArray<FString>& PropertyValues);

	/**
	 * Batch-create expressions of all types (including specialized) in one transaction.
	 * Maps to action="batch_create_specialized"
	 *
	 * Supports generic expressions, MaterialFunctionCall (with function path),
	 * Custom HLSL (with code, output type, inputs), and CollectionParameter
	 * (with collection path and parameter name). All created in a single transaction
	 * with one graph refresh for 10-100x speedup.
	 *
	 * For generic expressions, only ClassName/PosX/PosY are needed.
	 * For MaterialFunctionCall, also set FunctionPath.
	 * For Custom, also set HLSLCode, HLSLOutputType, HLSLDescription, HLSLInputNames.
	 * For CollectionParameter, also set CollectionPath, CollectionParamName.
	 *
	 * @param MaterialPath Full path to the material
	 * @param Descriptors Array of creation descriptors
	 * @return Array of created expression info in same order (empty entries on failure)
	 *
	 * Example:
	 *   descs = [FBatchCreateDescriptor(ClassName="Multiply", PosX=-400, PosY=0),
	 *            FBatchCreateDescriptor(ClassName="MaterialFunctionCall", PosX=-600, PosY=0, FunctionPath="/Game/MF_Test")]
	 *   nodes = unreal.MaterialNodeService.batch_create_specialized("/Game/M_Test", descs)
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "MaterialNode")
	static TArray<FMaterialExpressionInfo> BatchCreateSpecialized(
		const FString& MaterialPath,
		const TArray<FBatchCreateDescriptor>& Descriptors);

	// =================================================================
	// Export Actions
	// =================================================================

	/**
	 * Export the complete material graph as a JSON string for analysis or recreation.
	 * Maps to action="export_graph"
	 *
	 * Returns material settings, all expressions (type, position, properties),
	 * all connections, and material output connections — everything needed to
	 * recreate the material from scratch.
	 *
	 * @param MaterialPath Full path to the material
	 * @return JSON string of complete graph, or empty string on failure
	 *
	 * Example:
	 *   json_str = unreal.MaterialNodeService.export_material_graph("/Game/M_Landscape")
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "MaterialNode")
	static FString ExportMaterialGraph(const FString& MaterialPath);

	/**
	 * Export a lightweight summary of a material graph (counts, types, parameters).
	 * Maps to action="export_material_graph_summary"
	 *
	 * Returns a JSON object with:
	 * - expression_count: total expressions
	 * - class_counts: {class_name: count} frequency map
	 * - connection_count: total connections
	 * - material_output_count: connected material outputs
	 * - parameter_count: number of scalar/vector/texture/static parameters
	 * - parameters: [{name, type}] list
	 *
	 * Much lighter than ExportMaterialGraph — useful for verification and comparison.
	 *
	 * @param MaterialPath Full path to the material
	 * @return JSON string summary, or empty string on failure
	 *
	 * Example:
	 *   summary = unreal.MaterialNodeService.export_material_graph_summary("/Game/M_Mat")
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "MaterialNode")
	static FString ExportMaterialGraphSummary(const FString& MaterialPath);

	/**
	 * Compare two material graphs and return a JSON diff report.
	 * Maps to action="compare_material_graphs"
	 *
	 * Compares expression counts by class, connection counts, parameter lists,
	 * and material output assignments. Returns JSON with:
	 * - match: true/false overall
	 * - expression_count_match, connection_count_match, parameter_match, output_match
	 * - expression_count_a, expression_count_b
	 * - class_diff: [{class_name, count_a, count_b}] for mismatches
	 * - missing_parameters_in_b, extra_parameters_in_b: [{name, type}]
	 *
	 * @param MaterialPathA Full path to first material (reference)
	 * @param MaterialPathB Full path to second material (recreation)
	 * @return JSON diff string, or empty string on failure
	 *
	 * Example:
	 *   diff = unreal.MaterialNodeService.compare_material_graphs("/Game/M_Original", "/Game/M_Copy")
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "MaterialNode")
	static FString CompareMaterialGraphs(
		const FString& MaterialPathA,
		const FString& MaterialPathB);

	// =================================================================
	// Material Function Actions
	// =================================================================

	/**
	 * Export a Material Function's node graph as JSON.
	 * Maps to action="export_function_graph"
	 *
	 * Works like export_graph but for UMaterialFunction assets.
	 * Returns all expressions, connections, function inputs/outputs,
	 * and properties — everything needed to recreate the function.
	 *
	 * @param FunctionPath Full asset path to the UMaterialFunction
	 * @return JSON string of the complete function graph, or empty string on failure
	 *
	 * Example:
	 *   json_str = unreal.MaterialNodeService.export_function_graph("/Game/Real_Landscape/Core/Materials/Functions/MF_AutoLayer_01")
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "MaterialNode")
	static FString ExportFunctionGraph(const FString& FunctionPath);

	/**
	 * Get information about a Material Function's interface (inputs/outputs).
	 * Maps to action="get_function_info"
	 *
	 * Inspects a UMaterialFunction asset and returns its description,
	 * input pins, output pins, expression count, and library exposure settings.
	 *
	 * @param FunctionPath Full asset path to the UMaterialFunction
	 * @return Function info struct (empty with error if not found)
	 *
	 * Example:
	 *   info = unreal.MaterialNodeService.get_function_info("/Game/Real_Landscape/Core/Materials/Functions/MF_Slope_Blend_01")
	 *   for inp in info.inputs:
	 *       print(f"Input: {inp.name} ({inp.input_type_name})")
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "MaterialNode")
	static FVibeUEMaterialFunctionInfo GetFunctionInfo(const FString& FunctionPath);

	/**
	 * Create a new Material Function asset.
	 * Maps to action="create_material_function"
	 *
	 * Creates an empty UMaterialFunction that can be populated with
	 * expressions, inputs, and outputs. Use add_function_input/output
	 * to define the function's interface.
	 *
	 * @param FunctionName Name for the new function asset
	 * @param DirectoryPath Content directory to create it in
	 * @param Description Optional description text
	 * @param bExposeToLibrary If true, function appears in the material function library
	 * @param LibraryCategories Optional category strings for library organization
	 * @return Create result with asset path
	 *
	 * Example:
	 *   result = unreal.MaterialNodeService.create_material_function("MF_MyLayer", "/Game/Materials/Functions", "Custom layer blending function", True)
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "MaterialNode")
	static FMaterialCreateResult CreateMaterialFunction(
		const FString& FunctionName,
		const FString& DirectoryPath,
		const FString& Description = TEXT(""),
		bool bExposeToLibrary = false);

	/**
	 * Add an input to a Material Function.
	 * Maps to action="add_function_input"
	 *
	 * Creates a UMaterialExpressionFunctionInput inside the function.
	 * The input will appear as a pin when the function is used in a material.
	 *
	 * @param FunctionPath Full asset path to the UMaterialFunction
	 * @param InputName Display name for the input
	 * @param InputType Type of the input: "Scalar", "Vector2", "Vector3", "Vector4",
	 *        "Texture2D", "TextureCube", "VolumeTexture", "StaticBool", "MaterialAttributes"
	 * @param SortPriority Sort order (lower = higher in list)
	 * @param Description Optional tooltip description
	 * @param PosX X position in graph
	 * @param PosY Y position in graph
	 * @return Expression ID of the created FunctionInput node
	 *
	 * Example:
	 *   inp_id = unreal.MaterialNodeService.add_function_input("/Game/MF_MyLayer", "BaseColor", "Vector3", 0, "Albedo texture input")
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "MaterialNode")
	static FString AddFunctionInput(
		const FString& FunctionPath,
		const FString& InputName,
		const FString& InputType = TEXT("Vector3"),
		int32 SortPriority = 0,
		const FString& Description = TEXT(""),
		int32 PosX = -600,
		int32 PosY = 0);

	/**
	 * Add an output to a Material Function.
	 * Maps to action="add_function_output"
	 *
	 * Creates a UMaterialExpressionFunctionOutput inside the function.
	 * The output will appear as a pin when the function is used in a material.
	 *
	 * @param FunctionPath Full asset path to the UMaterialFunction
	 * @param OutputName Display name for the output
	 * @param SortPriority Sort order (lower = higher in list)
	 * @param Description Optional tooltip description
	 * @param PosX X position in graph
	 * @param PosY Y position in graph
	 * @return Expression ID of the created FunctionOutput node
	 *
	 * Example:
	 *   out_id = unreal.MaterialNodeService.add_function_output("/Game/MF_MyLayer", "BlendedColor", 0, "Final blended output")
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "MaterialNode")
	static FString AddFunctionOutput(
		const FString& FunctionPath,
		const FString& OutputName,
		int32 SortPriority = 0,
		const FString& Description = TEXT(""),
		int32 PosX = 200,
		int32 PosY = 0);

	/**
	 * Create a material expression inside a Material Function's graph.
	 * Maps to action="create_function_expression"
	 *
	 * Like CreateExpression but operates on a UMaterialFunction instead of UMaterial.
	 *
	 * @param FunctionPath Full asset path to the UMaterialFunction
	 * @param ExpressionClass Class name (e.g., "Add", "Multiply", "TextureSample")
	 * @param PosX X position in graph
	 * @param PosY Y position in graph
	 * @return Created expression info
	 *
	 * Example:
	 *   node = unreal.MaterialNodeService.create_function_expression("/Game/MF_MyLayer", "Multiply", -200, 0)
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "MaterialNode")
	static FMaterialExpressionInfo CreateFunctionExpression(
		const FString& FunctionPath,
		const FString& ExpressionClass,
		int32 PosX = 0,
		int32 PosY = 0);

	/**
	 * Connect two expressions inside a Material Function's graph.
	 * Maps to action="connect_function_expressions"
	 *
	 * Like ConnectExpressions but operates on a UMaterialFunction.
	 *
	 * @param FunctionPath Full asset path to the UMaterialFunction
	 * @param SourceExpressionId Source expression ID
	 * @param SourceOutput Output name (empty for first output)
	 * @param TargetExpressionId Target expression ID
	 * @param TargetInput Input name on target
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "MaterialNode")
	static bool ConnectFunctionExpressions(
		const FString& FunctionPath,
		const FString& SourceExpressionId,
		const FString& SourceOutput,
		const FString& TargetExpressionId,
		const FString& TargetInput);

	/**
	 * Set a property on an expression inside a Material Function's graph.
	 * Maps to action="set_function_expression_property"
	 *
	 * Like SetExpressionProperty but operates on a UMaterialFunction.
	 *
	 * @param FunctionPath Full asset path to the UMaterialFunction
	 * @param ExpressionId Expression ID
	 * @param PropertyName Property name
	 * @param PropertyValue Value as string
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "MaterialNode")
	static bool SetFunctionExpressionProperty(
		const FString& FunctionPath,
		const FString& ExpressionId,
		const FString& PropertyName,
		const FString& PropertyValue);

	/**
	 * Remove every expression not reachable from the asset's outputs — the scriptable
	 * equivalent of the Material Editor's "Clean Graph -> Clean Up".
	 * Maps to action="cleanup_unused_expressions"
	 *
	 * Fills the gap Epic leaves: its native MaterialTools.delete_unused_expressions only
	 * accepts a UMaterial, so this is the only way to clean unused nodes out of a
	 * UMaterialFunction. (Single-node deletion is left to Epic's delete_expression, which
	 * already handles both Material and MaterialFunction graphs.)
	 *
	 * Works on both UMaterial and UMaterialFunction assets. Roots match the engine's own
	 * UMaterialGraph::GetUnusedExpressions: for a function, every FunctionOutput; for a
	 * material, every expression wired to a material property input plus all CustomOutput
	 * nodes. A depth-first walk back through every connected input marks the reachable set
	 * (following NamedRerouteUsage declarations, which have no input pins), and everything
	 * else is removed. Unreachable nodes only reference each other, so no reachable node is
	 * ever orphaned.
	 *
	 * @param AssetPath Full asset path to a UMaterial or UMaterialFunction
	 * @return Number of expressions removed
	 *
	 * Example:
	 *   removed = unreal.MaterialNodeService.cleanup_unused_expressions("/Game/MF_MyLayer")
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "MaterialNode")
	static int32 CleanupUnusedExpressions(const FString& AssetPath);

	/**
	 * Link a Named Reroute Usage node to its Declaration (the step the material editor does when you
	 * place a usage). Python cannot do this itself: UMaterialExpressionNamedRerouteUsage::Declaration and
	 * DeclarationGuid are plain UPROPERTY() fields, not editable, so set_editor_property refuses them.
	 *
	 * Both expressions must already exist in the same material or material function (create them with
	 * MaterialEditingLibrary.create_material_expression[_in_function]; wire the value into the declaration's
	 * input and the usage's output onward with connect_material_expressions).
	 *
	 * @param Usage       A MaterialExpressionNamedRerouteUsage
	 * @param Declaration A MaterialExpressionNamedRerouteDeclaration in the same material / function
	 * @return True when linked; false (with a warning) for a wrong class or a different owner
	 *
	 * Example:
	 *   decl = mel.create_material_expression(mat, unreal.MaterialExpressionNamedRerouteDeclaration, 0, 0)
	 *   decl.set_editor_property("name", "Accent")
	 *   use = mel.create_material_expression(mat, unreal.MaterialExpressionNamedRerouteUsage, 400, 0)
	 *   unreal.MaterialNodeService.link_named_reroute_usage(use, decl)
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "MaterialNode")
	static bool LinkNamedRerouteUsage(UMaterialExpression* Usage, UMaterialExpression* Declaration);

	/**
	 * List every named-reroute declaration ("graph variable") of a Material or MaterialFunction with the
	 * usages linked to it, and the usages whose declaration is missing. Reads only this asset's own
	 * expressions (never the nested expressions of functions it calls).
	 *
	 * @param AssetPath Full asset path to a UMaterial or UMaterialFunction
	 * @return Declarations (name, color, VariableGuid, position, wired source, usage ids) and broken usage ids;
	 *         empty when the asset does not resolve
	 *
	 * Example:
	 *   lst = unreal.MaterialNodeService.list_named_reroutes("/Game/MF_MyLayer")
	 *   for d in lst.declarations: print(d.name, len(d.usage_ids))
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "MaterialNode")
	static FVibeNamedRerouteList ListNamedReroutes(const FString& AssetPath);

	/**
	 * Create a named-reroute declaration in a Material or MaterialFunction and optionally wire a node
	 * into its input. A name that is already taken gets a _N suffix (engine MakeNameUnique); the returned
	 * info carries the final name.
	 *
	 * @param AssetPath        Full asset path to a UMaterial or UMaterialFunction
	 * @param Name             Requested variable name (must not be empty)
	 * @param NodeColor        Graph node color (shared by all linked usages)
	 * @param PosX             Node X position
	 * @param PosY             Node Y position
	 * @param SourceRef        Optional node to wire into the declaration: session id, object path or subobject
	 *                         name of an expression in this same asset. Empty = leave unwired
	 * @param SourceOutputName Output pin of SourceRef ("" = first output)
	 * @return The new declaration (empty Name on failure; nothing is changed on failure)
	 *
	 * Example:
	 *   info = unreal.MaterialNodeService.create_named_reroute_declaration(
	 *       "/Game/M_Test", "Accent", unreal.LinearColor(1, 0.5, 0, 1), 0, 0, tint.get_name(), "")
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "MaterialNode")
	static FVibeNamedRerouteInfo CreateNamedRerouteDeclaration(
		const FString& AssetPath,
		const FString& Name,
		const FLinearColor& NodeColor,
		int32 PosX,
		int32 PosY,
		const FString& SourceRef,
		const FString& SourceOutputName);

	/**
	 * Create a named-reroute usage linked to the declaration called DeclarationName and optionally wire its
	 * output into a node input. Declarations are addressed by exact name; a name that matches no declaration
	 * (or, impossibly, two) fails without changing anything.
	 *
	 * @param AssetPath       Full asset path to a UMaterial or UMaterialFunction
	 * @param DeclarationName Exact declaration name (including any _N suffix)
	 * @param PosX            Node X position
	 * @param PosY            Node Y position
	 * @param TargetRef       Optional consumer node: session id, object path or subobject name of an expression
	 *                        in this same asset (an expression of a referenced function is refused). Empty = unwired
	 * @param TargetInputName Input pin of TargetRef to wire ("" = first input)
	 * @return Session id of the new usage; empty on failure
	 *
	 * Example:
	 *   use_id = unreal.MaterialNodeService.create_named_reroute_usage("/Game/M_Test", "Accent", 400, 0, mul.get_name(), "A")
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "MaterialNode")
	static FString CreateNamedRerouteUsage(
		const FString& AssetPath,
		const FString& DeclarationName,
		int32 PosX,
		int32 PosY,
		const FString& TargetRef,
		const FString& TargetInputName);

	/**
	 * Rename a named-reroute declaration through the engine's own rename path: the name goes through
	 * MakeNameUnique and every linked usage node is reconstructed so it shows the new name. (Python
	 * set_editor_property("name") changes the name but does not refresh usages, which is why this call
	 * exists.) Usages stay linked by pointer and VariableGuid is unchanged.
	 *
	 * @param AssetPath       Full asset path to a UMaterial or UMaterialFunction
	 * @param DeclarationName Exact current declaration name
	 * @param NewName         Requested new name (must not be empty)
	 * @return Final name after MakeNameUnique; empty on failure
	 *
	 * Example:
	 *   final = unreal.MaterialNodeService.rename_named_reroute_declaration("/Game/M_Test", "Accent", "Highlight")
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "MaterialNode")
	static FString RenameNamedRerouteDeclaration(
		const FString& AssetPath,
		const FString& DeclarationName,
		const FString& NewName);

	/**
	 * Extract one named-reroute variable and how it is generated, as JSON (read-only).
	 *
	 * Walks back from the declaration's input through every connected input, inside this asset only, and
	 * returns: "declaration" {id, name, node_color, variable_guid, pos}; "nodes" + "connections" (the
	 * generating subgraph, in the same node/connection shape as export_material_graph / export_function_graph,
	 * plus the wire into the declaration); "inputs" (the walk boundary, listed and not walked past:
	 * parameters, constants, texture objects, function inputs, and usages of OTHER named reroutes as
	 * {usage_id, depends_on} - nested variables are dependencies, never inlined); and "consumers" (each
	 * usage of this variable with the node inputs / material properties / function outputs it feeds).
	 * Ids are session ids; names and variable_guid are the durable keys.
	 *
	 * @param AssetPath       Full asset path to a UMaterial or UMaterialFunction
	 * @param DeclarationName Exact declaration name
	 * @return JSON string; empty (with a warning) for an unknown or ambiguous name
	 *
	 * Example:
	 *   import json
	 *   d = json.loads(unreal.MaterialNodeService.get_named_reroute_definition("/Game/M_Test", "Tint"))
	 *   print(len(d["nodes"]), [i.get("name") or i.get("depends_on") for i in d["inputs"]])
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "MaterialNode")
	static FString GetNamedRerouteDefinition(const FString& AssetPath, const FString& DeclarationName);

	/**
	 * Promote one output of a node into a named-reroute variable: create a declaration wired from that output
	 * and replace every wire from it with a usage of the new declaration, one usage per consumer input, placed
	 * next to its consumer. Consumers are the node inputs of this asset (function-call inputs, function outputs
	 * and custom outputs included) and, in a Material, the material properties that read exactly this output;
	 * the node's other outputs stay wired. The compiled result is unchanged: a usage forwards to the
	 * declaration's input, which carries the output's component mask. A consumer whose own component mask
	 * differs from the output's is skipped (left wired to the source, logged, not in UsageIds). A Reroute (knot)
	 * source is converted: the declaration takes the knot's input, the knot's consumers get usages and the knot
	 * is deleted (kept when a skipped consumer still reads it). One undo transaction; never saves.
	 *
	 * @param AssetPath        Full asset path to a UMaterial or UMaterialFunction
	 * @param SourceRef        Node to promote: session id, object path or subobject name of an expression in this
	 *                         asset (a named reroute declaration or usage is refused)
	 * @param SourceOutputName Output pin of SourceRef ("" = first output)
	 * @param DeclarationName  Requested variable name; "" = the suggested name (the parameter name for a
	 *                         parameter, else "<ClassShortName>_<OutputName>"). A taken name gets a _N suffix
	 * @return The new declaration; UsageIds = the usages that replaced consumer wires. Empty Name on failure
	 *         (unknown asset / source / output, a named reroute source, or no consumer to rewire), nothing changed
	 *
	 * Example:
	 *   info = unreal.MaterialNodeService.promote_to_named_reroute("/Game/M_Test", mul.get_name(), "", "Tint")
	 *   print(info.name, len(info.usage_ids))
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "MaterialNode")
	static FVibeNamedRerouteInfo PromoteToNamedReroute(
		const FString& AssetPath,
		const FString& SourceRef,
		const FString& SourceOutputName = TEXT(""),
		const FString& DeclarationName = TEXT(""));

	/**
	 * Readability pass: find every node output worth turning into a named-reroute variable and, unless
	 * bDryRun, promote each one with promote_to_named_reroute. A candidate is one source OUTPUT whose
	 * rewirable consumers number at least MinFanout, or whose longest wire (distance between the source's and
	 * a consumer's editor position; a material property counts from the material's root node) is at least
	 * MinWireLength. Skipped: named reroute declarations / usages, outputs that already feed a declaration,
	 * constants feeding one node, and knots feeding one node. Rows are ordered by source (PosY, PosX, name),
	 * then output index. The dry run (default) changes nothing and is the finder; an applied pass is one undo
	 * transaction and never saves.
	 *
	 * @param AssetPath     Full asset path to a UMaterial or UMaterialFunction
	 * @param bDryRun       True (default): only report candidates. False: promote each and fill bApplied/AppliedName
	 * @param MinFanout     Fanout threshold (default 3)
	 * @param MinWireLength Wire-length threshold in graph units (default 1500)
	 * @return Candidates in deterministic order; empty when the asset does not resolve or nothing qualifies
	 *
	 * Example:
	 *   rows = unreal.MaterialNodeService.apply_reroute_readability_pass("/Game/M_Test")   # dry run
	 *   for r in rows: print(r.suggested_name, r.fanout, round(r.max_wire_length))
	 *   rows = unreal.MaterialNodeService.apply_reroute_readability_pass("/Game/M_Test", False)
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "MaterialNode")
	static TArray<FVibeRerouteCandidate> ApplyRerouteReadabilityPass(
		const FString& AssetPath,
		bool bDryRun = true,
		int32 MinFanout = 3,
		float MinWireLength = 1500.f);

	/**
	 * The single source of truth for the material outputs this service understands, as ordered
	 * (friendly name -> EMaterialProperty) pairs. Both the writers (StringToMaterialProperty, behind
	 * connect_expression_to_output / disconnect_output) and the reader (GetOutputConnections) are
	 * driven from it, so a property can never be writable but invisible again (issue #611).
	 * Not a UFUNCTION (TPair does not marshal); public so a regression test can assert that parity.
	 */
	static const TArray<TPair<FString, EMaterialProperty>>& GetMaterialOutputProperties();

	/** Strict material-property mapper, driven by GetMaterialOutputProperties(). Accepts friendly
	 *  ("BaseColor") and enum ("MP_BaseColor") spellings, case-insensitive. Returns false and leaves
	 *  OutProperty untouched for unknown names — a bad name must never silently rewire another output.
	 *  Not a UFUNCTION (EMaterialProperty out-param); public so the parity test can assert against it. */
	static bool StringToMaterialProperty(const FString& PropertyName, EMaterialProperty& OutProperty);

private:
	// Helper methods
	static UMaterial* LoadMaterialAsset(const FString& MaterialPath);
	static UMaterialFunction* LoadMaterialFunctionAsset(const FString& FunctionPath);
	static UMaterialExpression* FindExpressionById(UMaterial* Material, const FString& ExpressionId);
	static UMaterialExpression* FindExpressionInFunctionById(UMaterialFunction* Function, const FString& ExpressionId);
	static FString GetExpressionId(UMaterialExpression* Expression);
	static FExpressionInput* FindInputByName(UMaterialExpression* Expression, const FString& InputName);
	static int32 FindOutputIndexByName(UMaterialExpression* Expression, const FString& OutputName);
	static TArray<FString> GetExpressionInputNames(UMaterialExpression* Expression);
	static TArray<FString> GetExpressionOutputNames(UMaterialExpression* Expression);
	static UClass* ResolveExpressionClass(const FString& ClassName);
	static FMaterialExpressionInfo BuildExpressionInfo(UMaterialExpression* Expression);
	static void RefreshMaterialGraph(UMaterial* Material);
	static FString FunctionInputTypeToString(int32 InputType);
	static int32 StringToFunctionInputType(const FString& TypeName);

	// Graph-owner helpers shared by the named-reroute calls (DEC-6297). Plain C++, not reflected.
	/** Load a Material, else a MaterialFunction, from AssetPath; logs a warning naming Caller on failure. */
	static bool ResolveGraphOwner(const FString& AssetPath, FVibeMaterialGraphOwner& OutOwner, const TCHAR* Caller);
	/** Session id / object path / subobject name -> expression, only if the Owner holds it. No numeric-index form. */
	static UMaterialExpression* FindOwnedExpression(const FVibeMaterialGraphOwner& Owner, const FString& Ref);
	/** Exact-name lookup; zero or two-plus matches set OutError and return null. */
	static UMaterialExpressionNamedRerouteDeclaration* FindDeclarationByName(const FVibeMaterialGraphOwner& Owner, const FString& Name, FString& OutError);
	static UMaterialExpression* CreateOwnedExpression(const FVibeMaterialGraphOwner& Owner, UClass* ExpressionClass, int32 PosX, int32 PosY);
	/** Recompile / refresh the graph after a mutation. Never saves. */
	static void RefreshOwner(const FVibeMaterialGraphOwner& Owner);
	static FVibeNamedRerouteInfo BuildNamedRerouteInfo(const FVibeMaterialGraphOwner& Owner, UMaterialExpressionNamedRerouteDeclaration* Declaration);
	/** Body of promote_to_named_reroute without the transaction / refresh (the readability pass batches them).
	 *  Returns the new declaration, or null with OutError set and nothing changed. */
	static UMaterialExpressionNamedRerouteDeclaration* PromoteOwnedOutput(const FVibeMaterialGraphOwner& Owner, UMaterialExpression* Source,
		int32 OutputIndex, const FString& DeclarationName, FString& OutError);
	/** Default variable name for a promoted output: the parameter name, else "<ClassShortName>_<OutputName>". */
	static FString SuggestNamedRerouteName(UMaterialExpression* Source, int32 OutputIndex);

	// Shared JSON builders behind export_material_graph, export_function_graph and get_named_reroute_definition.
	/** One node object (adds the "named_reroute" key on reroute declaration / usage nodes). */
	static TSharedPtr<FJsonObject> ExportExpressionNode(UMaterialExpression* Expression, bool bFunctionGraph);
	/** One connection object for Expression's input InputIndex; null when that input is unconnected. */
	static TSharedPtr<FJsonObject> ExportConnection(UMaterialExpression* Expression, int32 InputIndex, bool bFunctionGraph);
};
