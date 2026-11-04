---
name: materials/workflows
description: Step-by-step material authoring workflows — create a material, material instance, texture/math/function-call/custom-HLSL/collection/static-switch nodes, set material properties, and batch create/connect/set node operations.
---

# Material Workflows

## Contents
- Create a basic material
- Create a material instance
- Add a texture parameter
- Create a math expression
- Set material properties
- Function call node (MaterialFunction)
- Custom HLSL expression
- Collection parameter
- Static switch / texture object parameter
- Batch create / connect / set
- Named reroutes (graph variables)

VibeUE is an extension on Unreal's native MCP endpoint. **Material/function BASICS** (create
material, add expression, connect, set instance parameters) go through the **engine** material
toolsets via `call_tool(...)` — `editor_toolset.toolsets.material.MaterialTools` for
materials/functions, `editor_toolset.toolsets.material_instance.MaterialInstanceTools` for
instances. The **graph/parameter authoring DELTA** (`create_parameter`, `create_function_call`,
`create_custom_expression`, `batch_*`, diagnostics, export) stays on
`unreal.MaterialNodeService` via `execute_python_code`.

Surviving create calls return result objects — read `.id` (MaterialExpressionInfo from
`create_parameter`/`create_function_call`), never the raw object. Compile after graph changes.
Runnable examples in `scripts/`.

## Create a basic material

```python
import unreal
ms, mns = unreal.MaterialService, unreal.MaterialNodeService

# Engine: create the material asset (returns the Material object, no .asset_path).
call_tool(
    tool_name="create_material",
    toolset_name="editor_toolset.toolsets.material.MaterialTools",
    arguments={"folder_path": "/Game/Materials/", "asset_name": "M_Character"},
)
path = "/Game/Materials/M_Character"

# VibeUE delta: parameter authoring stays on MaterialNodeService.
color = mns.create_parameter(path, "Vector", "BaseColor", "Surface", "0.8,0.2,0.2,1.0", -500, 0)
# Engine: connect an expression output to a material property.
call_tool(
    tool_name="connect_to_output",
    toolset_name="editor_toolset.toolsets.material.MaterialTools",
    arguments={"expression": color.id, "output_name": "", "material_property": "MP_BaseColor"},
)
rough = mns.create_parameter(path, "Scalar", "Roughness", "Surface", "0.5", -500, 100)
call_tool(
    tool_name="connect_to_output",
    toolset_name="editor_toolset.toolsets.material.MaterialTools",
    arguments={"expression": rough.id, "output_name": "", "material_property": "MP_Roughness"},
)
ms.compile_material(path)
unreal.EditorAssetLibrary.save_asset(path)
# NOTE: there is no layout_expressions API — position nodes at creation time via the
# editor_x/editor_y args (as above); MaterialNodeService has no graph-wide auto-arrange.
```
Runnable: `scripts/create_material.txt`.

## Create a material instance

```python
import unreal
ms = unreal.MaterialService

# Engine: create the instance, then set its overrides — all via MaterialInstanceTools.
call_tool(
    tool_name="create_material_instance",
    toolset_name="editor_toolset.toolsets.material_instance.MaterialInstanceTools",
    arguments={"parent_material": "/Game/Materials/M_Character",
               "asset_name": "MI_PlayerRed", "folder_path": "/Game/Materials/"},
)
ip = "/Game/Materials/MI_PlayerRed"
call_tool(
    tool_name="set_vector_parameter",
    toolset_name="editor_toolset.toolsets.material_instance.MaterialInstanceTools",
    arguments={"instance_path": ip, "parameter_name": "BaseColor", "r": 1.0, "g": 0.0, "b": 0.0, "a": 1.0},
)
call_tool(
    tool_name="set_scalar_parameter",
    toolset_name="editor_toolset.toolsets.material_instance.MaterialInstanceTools",
    arguments={"instance_path": ip, "parameter_name": "Roughness", "value": 0.3},
)
unreal.EditorAssetLibrary.save_asset(ip)
```
Runnable: `scripts/create_instance.txt`. (For per-actor tiling on Megascans surfaces, prefer a child MI
with the `Tiling` scalar overridden — see SKILL.md Critical Rules.) For setting many instance
parameters at once, VibeUE keeps `ms.set_instance_parameters_bulk(...)`.

## Add a texture parameter

```python
tex = mns.create_parameter(path, "Texture", "DiffuseMap", "Textures", "", -500, 0)
# Engine: connect the parameter output to a material property.
call_tool(
    tool_name="connect_to_output",
    toolset_name="editor_toolset.toolsets.material.MaterialTools",
    arguments={"expression": tex.id, "output_name": "", "material_property": "MP_BaseColor"},
)
ms.compile_material(path)
```

## Create a math expression

```python
color = mns.create_parameter(path, "Vector", "TintColor", "Surface", "", -600, 0)
inten = mns.create_parameter(path, "Scalar", "Intensity", "Surface", "1.0", -600, 100)
MT = "editor_toolset.toolsets.material.MaterialTools"
# Engine: add a generic expression node, then wire it.
mult = call_tool(tool_name="add_expression", toolset_name=MT,
                 arguments={"material_or_function": path, "expression_class": "Multiply",
                            "x": -300, "y": 0})
mult_id = mult.id
call_tool(tool_name="connect_expressions", toolset_name=MT,
          arguments={"from_expression": color.id, "from_output_name": "",
                     "to_expression": mult_id, "to_input_name": "A"})
call_tool(tool_name="connect_expressions", toolset_name=MT,
          arguments={"from_expression": inten.id, "from_output_name": "",
                     "to_expression": mult_id, "to_input_name": "B"})
call_tool(tool_name="connect_to_output", toolset_name=MT,
          arguments={"expression": mult_id, "output_name": "", "material_property": "MP_BaseColor"})
ms.compile_material(path)
```

## Set material properties

```python
ms.set_property(path, "BlendMode", "BLEND_Translucent")  # display "Blend Mode" also works
ms.set_property(path, "ShadingModel", "MSM_DefaultLit")
ms.set_property(path, "TwoSided", "true")
ms.compile_material(path)
```

## Function call node (MaterialFunction)

```python
fn = mns.create_function_call(path,
    "/Engine/Functions/Engine_MaterialFunctions02/Utility/BlendAngleCorrectedNormals", -600, 0)
call_tool(
    tool_name="connect_to_output",
    toolset_name="editor_toolset.toolsets.material.MaterialTools",
    arguments={"expression": fn.id, "output_name": "", "material_property": "MP_Normal"},
)
ms.compile_material(path)
```
Input names for function calls are the **bare** names from `get_function_info(func_path).inputs`
(never the display labels with type suffixes like `"TextureObject (T2d)"`).

## Custom HLSL expression

```python
custom = mns.create_custom_expression(path, "return sin(Time * Speed);", "SineWave",
    "Float1", "Time,Speed", -500, 0)   # OutputType: Float1..Float4, MaterialAttributes
call_tool(
    tool_name="connect_to_output",
    toolset_name="editor_toolset.toolsets.material.MaterialTools",
    arguments={"expression": custom.id, "output_name": "", "material_property": "MP_EmissiveColor"},
)
ms.compile_material(path)
```

## Collection parameter

```python
coll = mns.create_collection_parameter(path, "/Game/Materials/MPC_GlobalParams", "WindStrength", -500, 0)
# Engine: wire the collection-parameter output into another node's input.
call_tool(
    tool_name="connect_expressions",
    toolset_name="editor_toolset.toolsets.material.MaterialTools",
    arguments={"from_expression": coll.id, "from_output_name": "",
               "to_expression": some_mult_id, "to_input_name": "B"},
)
```

## Static switch / texture object parameter

```python
sw = mns.create_parameter(path, "StaticSwitch", "UseDetailTexture", "Features", "true", -600, 0)
to = mns.create_parameter(path, "TextureObject", "DetailMap", "Textures", "/Game/Textures/T_Detail.T_Detail", -500, 0)
```

## Batch create / connect / set

Each batch call applies in one transaction (much faster than per-node). Arrays must be equal length.

```python
types = ["Multiply", "Add", "Lerp", "OneMinus"]
names = ["Mult1", "Add1", "Lerp1", "Invert1"]   # "" to skip
xs, ys = [-400, -400, -200, -600], [0, 200, 100, 0]
created = mns.batch_create_expressions(path, types, names, xs, ys)

mns.batch_connect_expressions(path, source_ids, output_names, target_ids, input_names)
mns.batch_set_properties(path, node_ids, property_names, property_values)

# Wire a node into a material output with the VibeUE service (output_name="" = output 0;
# property accepts "BaseColor" or "MP_BaseColor"). Returns True only after a read-back.
mns.connect_expression_to_output(path, created[0].id, "", "BaseColor")
# mns.disconnect_output(path, "BaseColor")   # clear it again

# created[0].id is session-specific; created[0].object_path is durable and also accepted.
# (The engine MaterialTools.connect_to_output call_tool path is interchangeable.)
```
Runnable: `scripts/material_graph_batch.txt`.

## Named reroutes (graph variables)

Treat a Named Reroute Declaration as a graph variable: any value that feeds more than one node, or
would be wired across the graph, is declared once with a meaningful name and read through one Usage
per consumer (rule in `SKILL.md` → Critical Rules). Start with "Name-keyed API" below; the object-based
recipe here is for when you already hold expression objects. Python creates and names the declaration;
`MaterialNodeService.link_named_reroute_usage` links each usage, since the link fields are not
Blueprint-visible.

```python
import unreal
MEL, MNS = unreal.MaterialEditingLibrary, unreal.MaterialNodeService

def declare(owner, name, src, src_out="", x=-800, y=0, is_function=False):
    create = MEL.create_material_expression_in_function if is_function else MEL.create_material_expression
    decl = create(owner, unreal.MaterialExpressionNamedRerouteDeclaration, x, y)
    decl.set_editor_property("name", name)
    MEL.connect_material_expressions(src, src_out, decl, "")
    return decl

def use(owner, decl, dst, dst_in, is_function=False):
    create = MEL.create_material_expression_in_function if is_function else MEL.create_material_expression
    x = dst.get_editor_property("material_expression_editor_x") - 200
    y = dst.get_editor_property("material_expression_editor_y")
    u = create(owner, unreal.MaterialExpressionNamedRerouteUsage, x, y)   # sits next to its consumer
    if not MNS.link_named_reroute_usage(u, decl):
        raise RuntimeError("link failed for %s" % decl.get_editor_property("name"))
    MEL.connect_material_expressions(u, "", dst, dst_in)
    return u

mat = unreal.load_asset(path)
accent = declare(mat, "Accent", accent_param)
for node, pin in [(mul_a, "A"), (lerp_b, "B"), (add_c, "A")]:
    use(mat, accent, node, pin)
MEL.recompile_material(mat)
diag = MNS.get_material_diagnostics(path)   # expect no new compile errors
```

- A usage with no linked declaration compiles to "Invalid named reroute variable"; if you see that error,
  a usage was created but not linked.
- A name clash is renamed to `Name_N` on assignment; read the name back if later code looks it up.
- In HF projects, HF_Common `Content/Python/hf_ui_matlib.py` wraps this as `G.named(name, ref)`: every
  consumer of the returned value gets its own usage (used by `M_HF_UiTile`).
- To rebuild a graph in place, delete node by node and finish with `MNS.cleanup_unused_expressions(path)`
  (`delete_all_material_expressions` leaves declarations behind, so new ones get `_1` suffixes).
- `export_material_graph` / `export_function_graph` record each reroute node's link in `named_reroute`
  (`role`, `name`, `guid`, `declaration_id`); see `reference.md` → Recreate from export.

### Name-keyed API (Material or MaterialFunction, by asset path)

Prefer these to the object-based recipe above: they take the asset path and the variable NAME, work the same
for a `Material` and a `MaterialFunction`, and cannot reach nodes inside functions the asset calls.

```python
mns = unreal.MaterialNodeService
info = mns.create_named_reroute_declaration(path, "Accent", unreal.LinearColor(1, 0.5, 0, 1), -800, 0,
                                            accent_param.get_path_name(), "")   # wire a source node (optional)
name = info.name                       # final name: a clash became "Accent_1"
use_id = mns.create_named_reroute_usage(path, name, -400, 0, mul.get_path_name(), "A")   # linked + wired
lst = mns.list_named_reroutes(path)    # declarations, their usage_ids, broken_usage_ids
final = mns.rename_named_reroute_declaration(path, name, "Highlight")   # usages stay linked and refresh
defn = json.loads(mns.get_named_reroute_definition(path, final))        # see below
```

Node refs (`source_ref`, `target_ref`) are a session id, an object path or a bare subobject name of a node in
THAT asset; a number or a node of a called function is refused. Failed calls change nothing and return an empty
name / id. Python `set_editor_property("name")` does NOT refresh usage nodes: rename through
`rename_named_reroute_declaration`.

### Promote a fan-out, then run the readability pass

```python
# one output with several consumers -> 1 declaration + N usages; compiled result unchanged
info = mns.promote_to_named_reroute(path, mul.get_path_name(), "", "Tint")   # source_ref, output "" = first, requested name
print(info.name, len(info.usage_ids))      # empty name = nothing changed (unknown source, no consumer to rewire)

# whole asset: DRY RUN first (default) - it only lists candidates
rows = mns.apply_reroute_readability_pass(path)                  # min_fanout=3, min_wire_length=1500
for r in rows: print(r.suggested_name, r.fanout, round(r.max_wire_length))
rows = mns.apply_reroute_readability_pass(path, False)           # apply; r.applied / r.applied_name per row
unreal.MaterialEditingLibrary.recompile_material(unreal.load_asset(path))   # then get_material_diagnostics(path)
```

A consumer whose component mask differs from the output's is skipped (stays wired to the source); a Reroute
knot source is converted (declaration takes the knot's input, the knot is deleted). Both calls are one undo
transaction and never save. Read the dry-run rows before applying: long single-consumer wires qualify via
`min_wire_length`, so raise it if you only want fan-outs.

### Extract one variable and how it is generated

`get_named_reroute_definition(path, name)` returns JSON: `declaration`, `nodes` + `connections` (the subgraph
that generates the value, same node/connection shape as the full export), `inputs` (the boundary: parameters,
constants, texture objects, function inputs, and `{usage_id, depends_on}` for OTHER variables, which are
dependencies and never inlined) and `consumers` (each usage with its `feeds`: `{expression_id, input_name}`,
`{property}` or `function_output`). To rebuild it elsewhere, recreate `inputs` first, then `nodes`, then wire
`connections`, then create the declaration with the `connections` entry whose `target_id` is `declaration.id`
as its source, then one usage per consumer.

### Slice one variable out of a full export

With only an export file and no live editor (prefer `get_named_reroute_definition` when you have one):
1. Parse the `export_material_graph` / `export_function_graph` JSON; find the node with
   `named_reroute.role == "declaration"` and the wanted `named_reroute.name`.
2. Back-walk `connections` from that node: for each connection with `target_id` == the current node, add its
   `source_id` node; repeat.
3. Stop (list as inputs, do not walk past) at parameters (`is_parameter`), constants (`class` Constant,
   Constant2Vector/3Vector/4Vector), texture objects, function inputs (`is_function_input`), and any node with
   `named_reroute.role == "usage"` and a DIFFERENT `name` (record `depends_on: name`).
4. Consumers: nodes with `named_reroute.role == "usage"` and the same `name`, plus every `connections` entry
   whose `source_id` is one of them (and `output_connections` entries for materials).
Ids in the file are session ids from the export run: only compare ids inside the one file.
