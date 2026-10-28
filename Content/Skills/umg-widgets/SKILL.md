---
name: umg-widgets
display_name: UMG Widget Blueprints
description: Create and modify UMG Widget Blueprints (UI) — build the widget hierarchy, set properties, style fonts/brushes, author widget animations, bind events, capture previews, run PIE checks, and wire MVVM ViewModels. Use when the user asks to create a UI/HUD/menu, add or arrange widgets (Button, TextBlock, Image, panels), style or animate a widget, or set up data bindings in a Widget Blueprint (WBP).
vibeue_classes:
  - WidgetService
  - BlueprintService
unreal_classes:
  - EditorAssetLibrary
keywords:
  - umg
  - widget
  - ui
  - hud
  - menu
  - canvas
  - button
  - textblock
  - panel
  - viewmodel
  - mvvm
  - binding
  - animation
related_skills:
  - blueprints
  - blueprint-graphs
---

> 🧠 **Brains complement:** IF an `unreal-engine-skills-manager` tool (external MCP) exists in this session, call it with `{action: "load", skill: "umg-and-slate"}` for UE domain knowledge on this topic — correct APIs, architecture, best practices — and treat it as the rubric for any review / "best practices" question. If no such tool is available (e.g. running under Claude Code or Codex without that MCP), skip this line entirely and proceed with this skill alone — do NOT attempt the call.

# UMG Widget Blueprints Skill

> **No PIE while developing UI (HF rule, Scott 2026-10-10).** Verify with `capture_preview_state` (PIE-free) and the
> `EUW_HfWidgetTester`; do not start PIE, run PIE checks or captures, or add PIE todos/baselines to a work item. All PIE
> testing is saved for the very end of the item and done by Scott unless he says otherwise (PIE cycles are expensive);
> list PIE-only gaps as unverified instead. Any PIE steps in this pack are reference for that end-of-item pass only.

This skill covers authoring **Widget Blueprints (WBP)** with `WidgetService`. Read the Critical Rules
below first — they prevent the most common failures. Then pick a task from the Task Index and load the
matching workflow doc and/or run the matching sample script.

> Load the **`blueprints`** skill when you also need to add Blueprint variables, functions, or
> components, and **`blueprint-graphs`** when wiring event-graph nodes for bound events.

## Critical Rules

### 🚨 Base class FIRST, Widget Blueprint second (any widget with data, state or input)

Build the abstract script/C++ base class before the WBP, then create the WBP **with that class as
its parent** so the Blueprint holds only the tree, art and animations:

1. Write `UCLASS(Abstract) class UMyPanelWidget : UUserWidget` (AngelScript on the Hazelight fork,
   or a C++ `UCLASS(Abstract, Blueprintable)`) with one `UPROPERTY(BindWidget)` per tree element it
   needs (`BindWidgetOptional` for optional ones), all behaviour in the class, `BP_*`
   `BlueprintEvent` hooks for animations. Compile it.
2. Create the WBP with `parent_class = unreal.load_class(None, "/Script/Angelscript.MyPanelWidget")`
   (C++: `"/Script/<Module>.<Class>"`).
3. Add the tree: each bound widget has **exactly** the member's name, `is_variable=True`, and a
   compatible class. Compile; a "BindWidget … not found" error means the tree is incomplete —
   never add a Blueprint variable to silence it.
4. Verify: `get_widget_snapshot` shows every bound name as a variable, and
   `MathLibrary.class_is_child_of(load_class(None, path + "." + name + "_C"), parent)` is `True`.

Full template, reparent path for legacy WBPs, ListView-entry workaround and review checklist:
**`script-base-first.md`**. Runnable: `scripts/create_widget_from_script_base.txt`. Skip the base
class only for a purely decorative widget with no logic.

### 🚨 Materials: look = an instance, state = one animation-keyed scalar

For any widget whose look or hover/press/fill state is a UI-domain material: one master per family, a
`MaterialInstanceConstant` per style variant (never a duplicate master for a style change), and runtime
parameters for STATE only (`Is Hovered`, `Is Active`, `Fill amount`). The WBP animation (a
`Brush.Brush Material` scalar track) is the ONLY writer of the state scalar it keys; the AS base plays it
through `UPROPERTY(Transient, meta = (BindWidgetAnimOptional)) UWidgetAnimation Anim_X;` (the animation's FName
must equal the property name) and never also calls `SetScalarParameterValue` on that name. Script-owned values
(fill) write `Image.GetDynamicMaterial().SetScalarParameterValue(...)` uncached on every update; per-instance
looks are `SetBrushFromMaterial` from an `EditAnywhere UMaterialInterface` on the base, never a Blueprint
variable. Buttons/slots derive `UCommonButtonBase` (no `UButton` hitbox); the base settles the rest pose
(`Anim_*` frame 0) in `Construct`. Porting an existing WBP family: copy with UNIQUE names and
`replace_widget_class` (never `begin_advanced_copy_packages`/`consolidate_assets`, FND-01275). Details,
Python recipes and the review checklist: **`ui-materials.md`**. Engine truth behind the rules (Slate does not
premultiply Translucent output; tint/RenderOpacity arrive as VertexColor; widget pixel size is TexCoord 3 via the
engine function `GetUserInterfaceUV`; UI `Time` never pauses; distinct MIDs never batch; RetainerBox repaints every
frame by default; MPCs resolve per viewport world) with the mechanism-choice table, validation checklist and
symptom table: **`ui-material-engine-facts.md`**. Epic UIMaterialLab function/master catalogue:
**`ui-material-lab-catalogue.md`**. Third-party example repos (RTMSDF, TransitionFX, PDMS, EffectBox, UMG3DTransform)
measured on the fork, with copy-vs-reauthor verdicts: **`ui-material-examples-catalogue.md`**. Many-instance state
drive (grids/lists): shared state MICs + UI-Time lerp, `ui-materials.md` section 3.

### 🚨 Creating Widget Blueprints — only one pattern works

`WidgetService` does **not** create the asset. Create it with the factory, then use `WidgetService`
to populate it:

```python
import unreal
factory = unreal.WidgetBlueprintFactory()
factory.set_editor_property("parent_class", unreal.UserWidget)   # or load_class(None, "/Script/Angelscript.MyPanelWidget")
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
asset_tools.create_asset("MainMenu", "/Game/UI", unreal.WidgetBlueprint, factory)
```

Do **not** call `BlueprintEditorLibrary.get_blueprint_class()`, `unreal.create_widget_blueprint()`, or
`WidgetService.create_widget()` — none exist. (Runnable: `scripts/create_widget.txt`.)
`get_editor_property("parent_class")` is not readable on a `WidgetBlueprint`; check lineage with
`class_is_child_of` on the generated `_C` class instead.

### 🚨 Hierarchy: set the root first, parents before children

```python
unreal.WidgetService.add_component(path, "CanvasPanel", "RootCanvas", "", True)   # empty parent -> becomes root
unreal.WidgetService.add_component(path, "Button", "PlayButton", "RootCanvas", False)
```

That 5th argument is `is_variable`, **not** `set_as_root` — a widget becomes the root because its parent
name is empty, not because of that flag. A 6th argument, `child_index`, inserts at a position instead of
appending; `reorder_component` moves an existing widget within its parent. See `reference.md` ▸ Child order.

> ⚠️ **Widget names are unique per-blueprint, NOT per-parent.** UMG enforces one name across the
> whole Widget Blueprint, so you can't add an `ItemLabel`/`ItemButton` under each of `Item1`,
> `Item2`, `Item3`. Suffix per-parent (`Item1Label`, `Item2Label`, …) — reusing a name silently
> lands the component on the wrong parent or no-ops.

### 🚨 Property values are ALWAYS strings

```python
unreal.WidgetService.set_property(path, "PlayText", "Font.Size", "24")  # "24", not 24
```

### 🚨 Color properties: FSlateColor vs FLinearColor — verify with a readback

`ColorAndOpacity` on text/image widgets is **FSlateColor**, whose export text is
`"(SpecifiedColor=(R=1.0,G=0.5,B=0.0,A=1.0))"` — a bare LinearColor string is auto-wrapped
into that form by `set_property`. `ShadowColorAndOpacity` is a plain **FLinearColor**:
`"(R=0.0,G=0.0,B=0.0,A=1.0)"`. Named color strings ("red", "orange") are NOT supported on
either — pass numeric RGBA.

**Always `get_property` after `set_property` and compare.** A struct set can return `True`
while writing nothing (UE struct import ignores unrecognized member names). If the readback
still shows the old value, your value string didn't match the struct's member layout — check
`list_properties` for the property's real type and use its export-text form.

### 🚨 Use dedicated style APIs for full font/brush edits

Use `set_font` / `set_brush` (with `WidgetFontInfo` / `WidgetBrushInfo`) when changing a complete font
or brush — they keep related fields together. Use `set_property` only for single leaf values or slot
aliases like `Position X`, `Size Y`, `Anchor Min X`. See `reference.md` for every field name; runnable
examples in `scripts/apply_font.txt` and `scripts/apply_brush.txt`.

`set_brush` requires a **`slot_name`**: `set_brush(widget_path, component_name, slot_name, brush_info)`
(for an Image the slot is `"Brush"`). `set_font` takes an optional `property_name` that defaults to `"Font"`.

> ⚠️ **`set_property("Font.Typeface", ...)` silently returns False** (size and color leaf-sets work,
> but typeface/family do not go through the struct sub-property path). Always change typeface/family
> via `set_font` with a `WidgetFontInfo`. Don't trust the `True/False` return of `set_property` on
> nested struct sub-fields — read it back to confirm.

> ✅ **`set_font` colour is now applied** (`ColorAndOpacity` is an `FSlateColor`; the old path
> silently no-op'd a bare `(R=,G=,B=,A=)` tuple). Pass a LINEAR tuple, e.g. `(R=0.035,G=0.002,B=0.002,A=1)`;
> `set_font` returns **False** and logs a Warning if a supplied colour did not land, so trust the return.

> ⚠️ **`get_font` readback quirk (issue #470):** the struct-typed string fields `color`,
> `shadow_color`, and `shadow_offset` come back as **two concatenated representations** glued
> together, e.g. `shadow_offset == "(X=0.0,Y=0.0)(X=1.000000,Y=1.000000)"`. The *trailing*
> parenthesized group is the real value. Scalar fields (`size`, `typeface`, `font_family`,
> `letter_spacing`, `outline_size`) are clean. When verifying a font round-trip, parse the last
> `(...)` group of these three fields (or compare scalars only) until #470 is fixed.

### 🚨 Override-gated properties (SizeBox width/height etc.)

`set_editor_property("width_override", 34.0)` on a `USizeBox` stores the value but leaves the paired
`bOverride_WidthOverride` edit-condition flag false — the override silently never applies (the value
even reads back as 34.0). Call the dedicated setters (`set_width_override()`, `set_height_override()`,
`set_min_desired_width()`, ...) which set flag + value together. Applies to any widget property gated
by a `bOverride_*` flag; after a raw property set, read the flag back before trusting the layout.

### 🚨 Animations require real property paths

`add_animation_track` / `add_keyframe` target actual widget properties or slot aliases
(`RenderOpacity`, `ColorAndOpacity`, `Position X/Y`, `Size X/Y`). Always create the animation first,
then the track, then keyframes. (Runnable: `scripts/create_animation.txt`.)

### 🚨 Preview vs PIE — different purposes

- `capture_preview` renders an editor-side PNG without starting gameplay — use for appearance checks.
  It now renders with a single gamma pass (an sRGB PNG matching the designer, no more double-gamma) and
  a layout prepass (Overlay-centred content is centred, not bottom-aligned).
- `start_pie` + `spawn_widget_in_pie` are for runtime state / live property reads — use only when you
  need a live instance.

### 🚨 Custom WBPs as components — discover first, pass the asset name

Use `list_widget_blueprints("")` to find existing WBPs, then pass just the asset name (e.g.
`"WBP_HealthBar"`, not the full package path) as `component_type`. The child WBP must already exist;
circular references are rejected; the parent recompiles automatically.

### 🚨 Field-name gotchas (return objects)

| WRONG | CORRECT |
|-------|---------|
| `p.name` | `p.property_name` |
| `p.type` | `p.property_type` |
| `p.property_value` | `p.current_value` |
| `w.component_name` / `w.name` | `w.widget_name` |
| `w.widget_type` | `w.widget_class` |

`WidgetInfo` complete fields: `widget_name`, `widget_class`, `parent_widget`, `is_root_widget`, `is_variable`, `children`.

**`children` is an `Array[str]` of widget NAMES, not nested objects.** Iterating it and reading
`.widget_name`/`.is_variable` raises `AttributeError: 'str' object has no attribute ...`. To walk
the tree, build a name→info map first:

```python
infos = {w.widget_name: w for w in unreal.WidgetService.get_hierarchy(path)}
for child_name in infos["RootCanvas"].children:
    child = infos[child_name]          # look the child up by name
    print(child.widget_name, child.widget_class, child.is_variable)
```

`get_widget_snapshot(path)` returns full data (hierarchy + slot + all properties) for a valid
path — if it comes back **empty, the path is wrong**; do not conclude the widget is empty.

Full field tables for every return type are in `reference.md`.

### 🚨 Auditing widgets (best-practices review) — use these exact APIs

For "do our widgets follow best practices?" style audits:

- **Property bindings** (the #1 anti-pattern): in a snapshot's `properties`, any `*Delegate`
  entry (e.g. `VisibilityDelegate`, `TextDelegate`) with `current_value` of `(null).None` is
  **unbound** — a function name there means a polling property binding exists.
- **Tick/graph checks** go through `BlueprintService` (a WBP is a Blueprint):
  `list_graphs(path)` → `get_nodes_in_graph(path, "EventGraph")` → find `Event Tick` →
  `get_node_pins(path, graph, node_id)` and check each pin's `connected` flag. A Tick node with
  no connected pins is just the default stub, not live logic.
- These methods do **NOT** exist — do not guess them: `BlueprintService.get_graphs`,
  `get_functions`, `get_connected_nodes`, `unreal.find_class`. Use `list_graphs`,
  `list_functions`, `get_nodes_in_graph`, `get_node_details` (includes pin connections), and
  `unreal.load_class(None, path)` instead.

### Slot editing via `set_property`

`set_property` edits slot layout through these aliases (string values):

- **Canvas children:** `Position X/Y`, `Size X/Y`, `Anchor Min X/Y`, `Anchor Max X/Y`, `Alignment X/Y`
  (pivot — set to `0.5` with anchors at `0.5` to truly center), `ZOrder` (or `Z Order`).
- **Box/Overlay children** (VerticalBox/HorizontalBox/Overlay): `Horizontal Alignment` / `Vertical Alignment`
  (values `Fill`/`Left`/`Center`/`Right`/`Top`/`Bottom`), `Padding` (one value or `(Left=..,Top=..,Right=..,Bottom=..)`),
  `Padding Left/Top/Right/Bottom`, and on box slots `Size Rule` (`Fill`/`Automatic`) + `Size Value`.

```python
unreal.WidgetService.set_property(path, "PlayButton", "ZOrder", "5")
unreal.WidgetService.set_property(path, "HeaderRow", "Vertical Alignment", "Top")
unreal.WidgetService.set_property(path, "HeaderRow", "Padding", "8")
```

> **Shadowed names prefer the WIDGET (fixed in issue #553).** A name that exists on both a widget
> and its slot (e.g. `HorizontalAlignment` on a Border sitting in an Overlay) now writes the
> widget's own property; a log warning notes the ambiguity. To target the slot explicitly, prefix
> with `Slot.` — e.g. `set_property(path, "BackgroundBorder", "Slot.HorizontalAlignment", "Center")`
> (value aliases like `Fill`/`Center` still work behind the prefix). Names that exist only as slot
> aliases (the lists above) keep resolving to the slot with no prefix needed.

### Reparenting — `reparent_widget`

Move an existing widget to a new parent panel (preserves the widget object/GUID):

```python
unreal.WidgetService.reparent_widget(path, "BackgroundImage", "MainContainer")
```

Rejects moving a panel into itself/a descendant; the root cannot be reparented.

### Panel types

| Type | Purpose |
|------|---------|
| `CanvasPanel` | Absolute positioning (X, Y coords) |
| `VerticalBox` | Stack children vertically |
| `HorizontalBox` | Stack children horizontally |
| `Overlay` | Stack children on top of each other |

---

## Task Index

Pick the task, load its workflow section, and/or run its sample script via `execute_python_code`.
Sample scripts under `scripts/` are **runnable examples** — edit the variables at the top, then execute.

| Task | Workflow | Sample script (run via `execute_python_code`) |
|------|----------|-----------------------------------------------|
| Create a Widget Blueprint | `workflows.md` → Create Widget Blueprint | `scripts/create_widget.txt` |
| Script/C++ base class first, then the WBP extends it (BindWidget tree, verify lineage) | `script-base-first.md` | `scripts/create_widget_from_script_base.txt` |
| Reparent a legacy WBP onto a script base / ListView entry rows | `script-base-first.md` → §5, §6 | — |
| Implement a whole UI screen end to end (design ref -> material plan -> AS base -> WBP -> verify) | `screen-process.md` | — |
| Write an AS widget base that drives a UI material (button/slot/fill/grid, pure rules + test, rest-pose settle) | `ui-patterns-reference.md` (copy code, rename prefix; no pack needed) | — |
| Drive a widget's look/state with a UI material (instance vs master, state animation, fill) | `ui-materials.md` → §1-§4 | — |
| Inspect widget animations / material params from Python | `ui-materials.md` → §5 | — |
| Port an existing WBP family into a new folder (copy + class remap + anim rename) | `ui-materials.md` → §6; `script-base-first.md` → §5 | — |
| Review a UI screen's materials and bases | `ui-materials.md` → §7 | — |
| Build a menu (root + panel + buttons) | `workflows.md` → Build a Menu | `scripts/build_menu.txt` |
| Position widgets on a CanvasPanel | `workflows.md` → Canvas Positioning | — |
| Inspect a widget (hierarchy + properties) | `workflows.md` → Inspect a Widget | `scripts/inspect_hierarchy.txt` |
| Apply full font styling | `workflows.md` → Font Styling | `scripts/apply_font.txt` |
| Apply full brush styling | `workflows.md` → Brush Styling | `scripts/apply_brush.txt` |
| Author a widget animation | `workflows.md` → Widget Animation | `scripts/create_animation.txt` |
| Bind a widget event to a function | `workflows.md` → Bind Event | — |
| Rename / remove a widget | `workflows.md` → Edit the Hierarchy | — |
| Edit slot layout (z-order, alignment, padding) / reparent | SKILL.md → Slot editing | `scripts/edit_slots.txt` |
| Capture a preview PNG | `workflows.md` → Capture Preview | `scripts/capture_preview.txt` |
| Inspect a widget live in PIE | `workflows.md` → PIE Runtime Check | `scripts/pie_inspect.txt` |
| Add MVVM ViewModel + bindings | `mvvm.md` | `scripts/mvvm_hud.txt` |

## Sub-docs

Load these only when the task needs them — read the sibling file directly under
`Plugins/VibeUE/Content/Skills/umg-widgets/` (the engine `AgentSkillToolset` `GetSkills` is the
loader that exposes this skill; there is no `vibeue-skills-manager` tool):

- **`workflows.md`** — step-by-step task workflows with copy-paste Python for every task above.
- **`reference.md`** — field-name tables for every return type (`FWidgetInfo`, `FWidgetComponentSnapshot`,
  `FWidgetSlotInfo`, `FWidgetFontInfo`, `FWidgetBrushInfo`, `FWidgetAnimInfo`, `FWidgetPreviewResult`,
  `FPIEWidgetHandle`), the WidgetService action list, common properties, and event names.
- **`mvvm.md`** — MVVM ViewModel support: rules, creation types, binding modes, field names, and full
  add/bind/list/remove workflows.
- **`script-base-first.md`** — base-class-first authoring: the abstract AngelScript/C++ `UUserWidget`
  template with BindWidget members, creating the WBP with that parent, BindWidget tree rules,
  compile/lineage verification, reparenting legacy WBPs, ListView entry rows, hot-reload traps,
  and a review checklist for existing script widgets.
- **`screen-process.md`** — the one end-to-end "implement a UI screen" process: element list, material/instance
  plan, AS base, WBP creation, verification ladder (compile, lineage, preview, PIE captures at 3 resolutions x 5
  states, parity diff), staging, review checklist, with the WI-1548 port as worked example.
- **`ui-patterns-reference.md`** — project-neutral reference AS code for the UI-material patterns (material-write
  helper, pure rules + unit test, animated button base with rest-pose settle, slot, fill, grid with
  `UCommonButtonGroupBase`); copy and rename the prefix, no Epic pack required.
- **`ui-materials.md`** — UI-domain material patterns: when a material beats a brush, master vs instance rule,
  WBP animations keying one state scalar (`BindWidgetAnim` members, one writer, rest pose), script-driven fill
  values, Python inspection recipes (animations, material params), the copy/remap route for porting a WBP
  family, and a review checklist.

## Verification (do this before claiming success)

1. After hierarchy edits: re-read with `get_widget_snapshot(path)` and confirm the expected widgets/parents.
2. After property/style edits: read the value back (`get_property` / `get_font` / `get_brush`).
3. Open/compile the WBP and confirm no errors, then `unreal.EditorAssetLibrary.save_asset(path)`.

Never report a widget as created/configured until it appears in a fresh `get_widget_snapshot` result.

## Additional gotchas

- `capture_preview` renders correct gamma/colour now, but still confirm slot values with `get_component_snapshot` and layout in PIE. UMG colours are LINEAR — a dark sRGB red is about (0.035, 0.002, 0.002).
- Slot values written through `ObjectIterator` are discarded on the next compile; set them with `set_property(path, widget, "Slot.<Prop>", value)` or in C++ `NativeConstruct`.
- A fresh WBP has no root: the first `add_component(path, type, name, parent_name="")` becomes it. `BindWidget` binds only when the tree widget is `is_variable=True`; reading a bound member from Python is blocked, so enumerate `ObjectIterator(unreal.TextBlock)` filtered by the instance path.
- `bind_event` is only provably bound on the `GameInstance_*`-outered PIE instance (asset/preview instances always read unbound); simulate a click with `on_clicked.broadcast()` on that instance. Hand-built bound-event nodes compile but never fire — use `create_component_bound_event`.
- A widget can be fully authored yet never compiled; compile a legacy widget and read the error first. UE 5.8 renamed `WidgetBlueprintLibrary` to `unreal.WidgetLibrary`, and `UserWidget` instances do not expose `get_widget_from_name`.
- Input modes: a UI-only mode drops the first WASD press after it closes, so use `SetInputMode_GameAndUIEx` in Construct; the input mode survives `ServerTravel`; `DefaultInput.ini` holds a full serialised InputSettings block mid-file, so a key appended at the section top is silently overridden.
