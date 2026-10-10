---
name: umg-widgets/workflows
description: Step-by-step UMG widget task workflows — create a WBP, build a menu, position on canvas, inspect, style fonts/brushes, animate, bind events, edit the hierarchy, capture previews, and run PIE checks.
---

# UMG Widget Workflows

## Contents
- Create Widget Blueprint (base class first: `script-base-first.md`)
- Build a Menu
- Canvas Positioning
- Inspect a Widget
- Font Styling
- Brush Styling
- Widget Animation
- Bind Event
- Edit the Hierarchy (rename / reparent / remove)
- Capture Preview
- PIE Runtime Check

Each workflow has a matching runnable example under `scripts/` where noted. Field names for the return
objects used below are in `reference.md`.

## Create Widget Blueprint

Decide the parent first. A widget with data, state or input gets a script/C++ base class **before**
the WBP exists (`script-base-first.md`); a plain `UserWidget` parent is for decorative widgets only.

```python
import unreal

factory = unreal.WidgetBlueprintFactory()
# Decorative widget:
factory.set_editor_property("parent_class", unreal.UserWidget)
# Logic-bearing widget: the compiled script/C++ base (AngelScript: "/Script/Angelscript.<Class>")
# factory.set_editor_property("parent_class", unreal.load_class(None, "/Script/Angelscript.MyPanelWidget"))
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
asset_tools.create_asset("MainMenu", "/Game/UI", unreal.WidgetBlueprint, factory)
```

Runnable: `scripts/create_widget.txt` (plain) / `scripts/create_widget_from_script_base.txt`
(script base + BindWidget tree + verification).

## Build a Menu

Root canvas → vertical box → buttons, each with a child text label.

```python
import unreal

path = "/Game/UI/WBP_Menu"

unreal.WidgetService.add_component(path, "CanvasPanel", "Root", "", True)
unreal.WidgetService.add_component(path, "VerticalBox", "ButtonList", "Root", False)

for btn_name, btn_text in [("PlayButton", "PLAY"), ("QuitButton", "QUIT")]:
    unreal.WidgetService.add_component(path, "Button", btn_name, "ButtonList", False)
    text_name = f"{btn_name}Text"
    unreal.WidgetService.add_component(path, "TextBlock", text_name, btn_name, False)
    unreal.WidgetService.set_property(path, text_name, "Text", btn_text)

unreal.EditorAssetLibrary.save_asset(path)
```

Runnable: `scripts/build_menu.txt`.

## Canvas Positioning

Position/size/anchor aliases are set via `set_property` (values are strings):

```python
unreal.WidgetService.set_property(path, "Button", "Position X", "100")
unreal.WidgetService.set_property(path, "Button", "Position Y", "200")
unreal.WidgetService.set_property(path, "Button", "Size X", "200")
unreal.WidgetService.set_property(path, "Button", "Size Y", "50")
unreal.WidgetService.set_property(path, "Button", "Anchor Min X", "0.5")
unreal.WidgetService.set_property(path, "Button", "Anchor Min Y", "0.5")
```

To stretch a widget to fill its canvas, set anchors `Min (0,0)` → `Max (1,1)`. To **center** it, set
anchors to `0.5` and `Alignment X/Y` to `0.5` (the alignment is the pivot — without it the widget's
top-left sits on the anchor point):

```python
for k, v in [("Anchor Min X","0.5"),("Anchor Min Y","0.5"),("Anchor Max X","0.5"),("Anchor Max Y","0.5"),
             ("Alignment X","0.5"),("Alignment Y","0.5")]:
    unreal.WidgetService.set_property(path, "MenuStack", k, v)
```

`set_property` also writes `ZOrder` on canvas children and `Horizontal Alignment` / `Vertical Alignment`
/ `Padding` / `Size Rule` / `Size Value` on box/overlay children (alignment accepts friendly values like
`Fill`, `Top`, `Center`). See SKILL.md → "Slot editing via set_property" for the full alias list.

## Inspect a Widget

Prefer `get_widget_snapshot` — it returns hierarchy order + slot layout + all properties in one call
(do not stitch `get_hierarchy` + per-widget `list_properties`).

```python
import unreal

path = "/Game/UI/WBP_MainMenu"
snapshots = unreal.WidgetService.get_widget_snapshot(path)

for s in snapshots:
    print(f"{s.widget_name} ({s.widget_class}) parent={s.parent_widget} root={s.is_root_widget}")
    slot = s.slot_info
    print(f"  slot={slot.slot_type}")
    for prop in s.properties:
        if prop.is_editable:
            print(f"  {prop.property_name} = {prop.current_value}")
```

For a single widget use `get_component_snapshot(path, "PlayButton")`. For names-only hierarchy use
`get_hierarchy(path)` (returns `FWidgetInfo`). Runnable: `scripts/inspect_hierarchy.txt`.

## Font Styling

Use the dedicated font API for full edits, then read it back to verify.

```python
import unreal

path = "/Game/UI/WBP_MainMenu"

font_info = unreal.WidgetFontInfo()
font_info.font_family = "/Engine/EngineFonts/Roboto"
font_info.typeface = "Bold"
font_info.size = 30
font_info.letter_spacing = 20
font_info.color = "(R=1.0,G=0.9,B=0.25,A=1.0)"
font_info.shadow_offset = "(X=2.0,Y=2.0)"
font_info.shadow_color = "(R=0.0,G=0.0,B=0.0,A=0.75)"
unreal.WidgetService.set_font(path, "HeaderTitle", font_info)

applied = unreal.WidgetService.get_font(path, "HeaderTitle")
print(applied.size, applied.typeface, applied.color)
```

Runnable: `scripts/apply_font.txt`. Field names: `reference.md` → FWidgetFontInfo.

## Brush Styling

`set_brush`/`get_brush` take a **`slot_name`** argument:
`set_brush(widget_path, component_name, slot_name, brush_info)`. For an Image the slot is `"Brush"`;
other widgets expose different brush slot names.

```python
import unreal

path = "/Game/UI/WBP_MainMenu"

brush_info = unreal.WidgetBrushInfo()
brush_info.resource_path = "/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture"
brush_info.tint_color = "(R=0.08,G=0.12,B=0.2,A=1.0)"
brush_info.draw_as = "RoundedBox"
brush_info.image_size = "(X=1920.0,Y=1080.0)"
brush_info.margin = "(Left=0.2,Top=0.2,Right=0.2,Bottom=0.2)"
brush_info.corner_radius = "(TopLeft=16.0,TopRight=16.0,BottomRight=16.0,BottomLeft=16.0)"
unreal.WidgetService.set_brush(path, "BackgroundImage", "Brush", brush_info)

applied = unreal.WidgetService.get_brush(path, "BackgroundImage", "Brush")
print(applied.draw_as, applied.tint_color, applied.corner_radius)
```

Runnable: `scripts/apply_brush.txt`. Field names: `reference.md` → FWidgetBrushInfo.

A brush may also be a **material instance** (UI-domain material): the Image's `Brush.ResourceObject` is the
`MI_*` (a duplicated pack widget already carries it; for a new one use `resource_path` with `get_brush`
readback, or have the AS base call `Image.SetBrushFromMaterial(UMaterialInterface)`). Parameters of
that material are state (animation- or script-driven), not brush fields; prefer an instance of an existing
master over a new master. Rules and recipes: `ui-materials.md`.

## Widget Animation

Create the animation, add a track on a real property, then add keyframes.

```python
import unreal

path = "/Game/UI/WBP_MainMenu"

unreal.WidgetService.create_animation(path, "IntroFade")
unreal.WidgetService.add_animation_track(path, "IntroFade", "HeaderTitle", "RenderOpacity")

for t, v in [(0.0, "0.0"), (0.35, "1.0")]:
    key = unreal.WidgetAnimKeyframe()
    key.time = t
    key.value = v
    key.interpolation = "Linear"
    unreal.WidgetService.add_keyframe(path, "IntroFade", "HeaderTitle", "RenderOpacity", key)

for anim in unreal.WidgetService.list_animations(path):
    print(anim.animation_name, anim.duration, anim.track_count)
```

Runnable: `scripts/create_animation.txt`.

## Bind Event

`bind_event(widget_path, widget_name, event_name, function_name)`. Create the function first.
The engine `BlueprintTools` toolset owns Blueprint function creation — call it via `call_tool`:

```python
# Step 1: create the function graph (engine BlueprintTools, via call_tool)
call_tool(
    tool_name="add_function_graph",
    toolset_name="editor_toolset.toolsets.blueprint.BlueprintTools",
    arguments={"blueprint": path, "graph_name": "OnPlayClicked"},
)

# Step 2: bind the event to that function (WidgetService keeps this)
import unreal
unreal.WidgetService.bind_event(path, "PlayButton", "OnClicked", "OnPlayClicked")
unreal.EditorAssetLibrary.save_asset(path)
```

Common events: `OnClicked`, `OnPressed`, `OnReleased`, `OnHovered`, `OnUnhovered`. Use
`get_available_events(path, widget_name)` to discover valid events for a widget.

## Edit the Hierarchy (rename / reparent / remove)

```python
import unreal

path = "/Game/UI/WBP_Menu"

# Rename (GUID preserved)
unreal.WidgetService.rename_widget(path, "ItemButton", "Item1_ActionButton")

# Remove a widget (optionally its children)
unreal.WidgetService.remove_component(path, "Item3")

unreal.EditorAssetLibrary.save_asset(path)
```

Move a widget to a new parent with `reparent_widget(path, widget_name, new_parent_name)` — it preserves
the widget object/GUID (rejects moving a panel into itself/a descendant; root can't be reparented).

After any structural edit, re-read with `get_widget_snapshot(path)` to confirm before saving.

## Capture Preview

`capture_preview(widget_path, width=1920, height=1080)` takes **no output path** — it writes to
`<project>/Saved/WidgetPreviews/<WidgetName>.png` and returns the path in `result.output_path`.
The success field is `success` (not `b_success`).

```python
import unreal

result = unreal.WidgetService.capture_preview("/Game/UI/WBP_MainMenu", 1280, 720)
print(result.success, result.output_path, result.error_message)
```

Runnable: `scripts/capture_preview.txt`.

### Capture Preview State (PIE-free, interaction states)

`capture_preview_state(widget_path, width, height, options)` renders the widget off-screen in a requested state with the
project DPI curve applied (`UUserInterfaceSettings::GetDPIScaleBasedOnSize`), no PIE, no OS input. Same result struct as
`capture_preview` (`success`, `output_path`, `error_message`, plus `dpi_scale_used` and `state_log`, the ordered trail of
mechanics applied). Mechanism: DOC-0058. Legacy `capture_preview` is unchanged (static, no state, `Saved/WidgetPreviews/<WBP>.png`).

```python
import unreal
o = unreal.WidgetPreviewStateOptions()
o.set_editor_property("state", "hovered")            # normal|hovered|pressed|focused|disabled|selected
o.set_editor_property("desired_size", unreal.Vector2D(240, 72))   # (0,0) = fill the target; set it for a button
r = unreal.WidgetService.capture_preview_state("/Game/UI/Toadalled/Kit/WBP_Kit_Button", 1920, 1080, o)
print(r.success, r.output_path, r.dpi_scale_used, r.state_log, r.error_message)
```

Options (`WidgetPreviewStateOptions`, defaults): `state` "normal"; `hook_function` "HF_PreviewState" (a UFUNCTION on the
widget taking one FName; empty = skip); `animation_name` "" (explicit UWidgetAnimation to scrub) with `animation_time`
-1 (= the animation end); `dpi_scale` 0 (= resolve from W x H); `frames` 1 (DrawWidget calls per capture); `delta_time`
1/60; `output_name` "" (= `<WBP>_<state>_<W>x<H>`); `apply_generic_setters` True (CommonUI enabled/selected/focus
setters, skipped when the hook ran); `desired_size` (0,0). State semantics live in the widget's AS hook
(`UHF_MaterialButtonBase.HF_PreviewState` maps a state to its `Anim_*` end pose); C++ stays a generic primitive.
Project rig: `Tools/ui_kit/rig/run_states_offline.py <job.json>` (see `screen-process.md` section 5 for measured limits).

Gotchas:
- `frames` does not advance material `Time` (or UMG animations): N draws in one editor call see one
  `FApp::GetCurrentTime()`. Sample Time across separate editor calls (`Tools/ui_kit/rig/time_samples.py`), never sleep
  inside the editor snippet (FND-00627).
- Pressed is an animation pose (`Anim_Pressed` end pose), not a synthetic press; hover on kit buttons is also the
  `Anim_Hover` pose (`OnMouseEnter` alone renders no change, FND-01357). Disabled renders the dim pose without Slate's
  DisabledEffect desaturation (FND-01363): judge that in PIE or the tester tab.
- Focus / selected visuals need the AS hook: the editor world has no LocalPlayer, so CommonUI focus routing is not
  exercised. The first capture of a cold texture can be blank (FND-01359): warm with a throwaway capture.

Live tester (complement, real top-level window: real hover, animations and material `Time` tick, AS body hot reload
re-picks): Editor Utility Widget `/HF_Common/UI/Editor/EUW_HfWidgetTester`.

```python
import unreal
sub = unreal.get_editor_subsystem(unreal.EditorUtilitySubsystem)
euw = unreal.load_asset("/HF_Common/UI/Editor/EUW_HfWidgetTester")
inst = sub.spawn_and_register_tab(euw)
# AS UFUNCTIONs have NO exported Python name: reach them with call_method
# (SetWidgetToTest(class), GetTestWidget(), SetDesiredSize(FVector2D), PreviewState(FName)).
# Find an existing instance via unreal.ObjectIterator(unreal.EditorUtilityWidget), skipping Default__ CDOs.
# Close: sub.close_tab_by_id(sub.get_tab_id_from_blueprint(euw))
```

Capture the tab with `ue_driver.py call epic_capture_image {"source":"window","max_width":0}` and crop host-side.
`SetDesiredSize` does not pin the size (the ScaleBox stretches, FND-01364), so the tab is a visual-state cross-check, not
a sizing reference. Measured against headless: hovered/focused/selected agree; disabled differs and the tab is the
reference (full desaturation).

## PIE Runtime Check

`start_pie()` is **asynchronous** — PIE is not ready in the same call. Start PIE in one step, then
spawn/read in a later step. `get_live_property` and `remove_widget_from_pie` take the **handle object**
(not `handle.instance_id`), and the handle's validity field is `valid` (not `b_valid`).

```python
import unreal
ws = unreal.WidgetService
path = "/Game/UI/WBP_MainMenu"

# Step 1 (its own call):
if not ws.is_pie_running():
    ws.start_pie()

# Step 2 (after PIE is live):
handle = ws.spawn_widget_in_pie(path)
print(handle.valid, handle.instance_id, handle.error_message)
if handle.valid:
    live = ws.get_live_property(handle, "HeaderTitle", "RenderOpacity")
    print(live)
    ws.remove_widget_from_pie(handle)
ws.stop_pie()
```

Runnable: `scripts/pie_inspect.txt`.
