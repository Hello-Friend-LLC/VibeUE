---
name: materials/reference
description: Material reference — parameter types, material output names, enum formats, the export_material_graph JSON schema, recreate-from-export steps, and Material Function authoring (MaterialService / MaterialNodeService).
---

# Material Reference

## Contents
- Two services
- Parameter types
- Material output names
- Enum value format
- Export JSON schema
- Recreate a material from export
- Material Functions

## Two services
- **MaterialService** — create materials/instances, set properties, compile.
- **MaterialNodeService** — build graphs: expressions, parameters, connections, diagnostics, export.

## Parameter types

| Type | Use | Example value |
|------|-----|---------------|
| `Scalar` | single float | `0.5` |
| `Vector` | color/vector | `(R=1.0,G=0.0,B=0.0,A=1.0)` or `1,0,0,1` |
| `Texture` | texture parameter (with sampler) | `/Game/T_Diffuse.T_Diffuse` |
| `TextureObject` | texture object (no sampler) | `/Game/T_Diffuse.T_Diffuse` |
| `StaticSwitch` | compile-time bool branch | `true` / `false` |

## Material output names
`BaseColor`, `Metallic`, `Specular`, `Roughness`, `EmissiveColor`, `Normal`, `Opacity`,
`OpacityMask`, `WorldPositionOffset`, `AmbientOcclusion`.

## Enum value format
`MaterialService.set_property(path, name, value)` enum values are the full prefixed identifiers
(`BLEND_Masked`, `MSM_DefaultLit`, `MD_Surface`). `set_property` matching is case-insensitive and
ignores spaces on the property *name* (display or internal); read legal values from
`list_properties(...).allowed_values` rather than guessing.

## Export JSON schema

`export_material_graph(path)` returns JSON with top-level keys `material`, `expressions`,
`connections`, `output_connections`.

- **material**: `{blend_mode, shading_model, two_sided}`
- **expression**: `id`, `class` (e.g. `"Multiply"`), `class_full`, `pos_x`/`pos_y` (NOT x/y),
  `properties` (dict, excludes ParameterName/Group), `inputs`, `outputs`, `is_parameter`,
  `parameter_name`, `group`, `function_path`, `hlsl_code`, `custom_input_names`, `landscape_layers`.
  Named reroute declaration / usage nodes also carry `named_reroute`:
  `{role: "declaration"|"usage", name, guid, declaration_id}` (a usage whose declaration is gone adds
  `broken: true` with an empty `name`). `name` and `guid` are persistent; `declaration_id` is a session id.
  `export_function_graph` uses the same keys. `get_named_reroute_definition` emits nodes/connections in this shape.
- **connection**: `source_id`, `source_output_index`, `source_output_name` ("" = default),
  `target_id`, `target_input` (NOT `target_input_name`).
- **output_connection**: `property` (e.g. `"BaseColor"`), `expression_id`, `output_index`,
  `output_name` ("" = default).

Property gotchas: `MaterialExpressionTypeInfo` uses `display_name` (not `name`);
`MaterialOutputConnectionInfo` uses `connected_expression_id` (not `expression_id`).

## Recreate a material from export

1. `export_material_graph(src)` → parse JSON.
2. Engine `MaterialTools.create_material` (`call_tool`, args `folder_path`, `asset_name`) →
   reference the new asset by `folder_path + asset_name` (returns the Material object, no `.asset_path`).
3. `MaterialService.set_property(path, "BlendMode", ...)` / `set_property(path, "ShadingModel", ...)`
   from `graph['material']`.
4. `batch_create_expressions` (generic) / `batch_create_specialized` (function calls, custom) — keep an
   old-id → new-id map.
5. Set `ParameterName`/`Group` FIRST via `set_expression_property` (they're excluded from batch props).
6. `batch_set_properties` for the rest.
7. `batch_connect_expressions` using the `connections` array (mapped ids).
   **Named reroutes:** a node whose export has `named_reroute` is NOT created in step 4. Do instead, in this
   order, before step 7: (a) for each `role: "declaration"` node
   `create_named_reroute_declaration(path, named_reroute.name, color, pos_x, pos_y, source_ref, source_output_name)`
   (the final name is returned and can differ with a `_N` suffix on a clash: key the usages by that returned name);
   (b) for each `role: "usage"` node `create_named_reroute_usage(path, named_reroute.name, pos_x, pos_y, target_ref, target_input_name)`
   passing the already-recreated consumer (from the `connections` entry whose `source_id` is the usage) as
   target, and keep old-id -> new-id for the usage; (c) the declaration's own input wire (the `connections` entry whose
   `target_id` is the declaration) is `source_ref` in (a), so recreate its source node first. Skip those
   entries in `batch_connect_expressions`. A usage with `broken: true` has no declaration to name: recreate it
   as a plain node or drop it. `id` values (including `named_reroute.declaration_id`) are session ids and never survive
   a reload; the declaration `name` and `guid` are the persistent keys, and a recreated declaration gets a new guid.
8. `connect_expression_to_output(path, expr_id, output_name, property)` for each `output_connections`
   entry — `property` takes the friendly name (`"BaseColor"`) or the `MP_` spelling; `output_name=""`
   is output 0. (Interchangeable with the engine `MaterialTools.connect_to_output` `call_tool`.)
   Use `disconnect_output(path, property)` to clear one.
9. `compile_material`.

Runnable: `scripts/export_graph.txt` (export side).

## Material Functions

Reusable node subgraphs:
- `get_function_info(function_path)` — inspect inputs/outputs (bare input names).
- `export_function_graph(function_path)` — full function graph JSON.
- `create_material_function(name, dir)` — new function.
- `add_function_input(...)` / `add_function_output(...)` — I/O pins.
- `create_function_call(mat_path, func_path)` — reference a function in a material graph.

For landscape-specific function patterns, load `landscape-auto-material`.
