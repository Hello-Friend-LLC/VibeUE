---
name: umg-widgets/ui-materials
description: UI-domain material patterns for widgets (Epic UIMaterialLab, proven on the SpaceWasteCollector Applications-page port, WI-1548) — when a material beats a brush, master vs instance rule, WBP animations that key one state scalar, script-driven fill values, Python recipes for inspecting animations and materials, the copy/remap route for porting an existing WBP family, and the review checklist.
---

# UI materials: look = instance, state = one scalar

Measured on UE 5.8 Hazelight (2026-10-09). Pack = Epic UIMaterialLab (`/Game/UIMaterialLab`): guidance, not
gospel. It ships duplicate masters and Blueprint-variable/graph logic that an instance and an AS base replace.
Base-class rules: `script-base-first.md`.

## 1. When a UI material beats a brush/texture

- Use a UI-domain material (`MD_UI`, `BLEND_Translucent`) when the look is procedural (rounded SDF box, outline,
  gradient, liquid fill, segmented bar, halftone), must stay crisp at any size/DPI, or must tween between states.
- Use a plain texture brush for a flat art asset with no state (icon, photo).
- State lives in ONE scalar per state (`Hovered`, `Is Active`, `Fill amount`) that the master lerps between its
  style endpoints (`Button Color - default` / `- hovered`). A brush swap per state is the thing to avoid.

## 2. Master vs instance rule

- One master per family. One `MaterialInstanceConstant` per style variant (colours, sizes, roundness). A new
  master only for a genuinely different node graph.
- Parameters you write at runtime are STATE only; everything else is instance style and is never touched by script.
- State names to keep consistent: `Is Hovered`, `Is Active`, `Fill amount`, `Fill progress`. Do not rename pack
  parameters (copied animations key them by name). A NEW state parameter in a project master: 0/1 flags `Is<State>`
  with no spaces, continuous 0..1 values `<Thing>Amount` / `<Thing>Progress`.
- Anti-pattern (pack, measured in `DesignDocs/UI/ui-material-usage-review.md` section 4): `M_UI_Button` vs
  `M_UI_ButtonEfz` (identical 22 scalar / 3 vector params, 54 vs 58 expressions, cosmetic reroutes);
  `M_UI_LedBackground` vs `M_UI_LedBackground1` (+1 texture param, ZERO instances use `_1`);
  `M_UI_Liquid_Fill` vs `M_UI_Liquid_Fill1` (+1 `Min` clamp). Verdict for each: an instance of the first master.
  Redundant instances: `MI_UI_Liquid_Fill_1 == _4`, `MI_UI_Slot_3 == _6 == _7`.
- A port makes NO new masters: duplicate the pack instance with `EditorAssetLibrary.duplicate_asset(src, dst)`
  (keeps parent and overrides) into the port folder under a new name. Never repoint/save the pack.

## 3. State tweens: WBP animation keys the scalar

- The animation holds a `MovieSceneWidgetMaterialTrack` named `Brush.Brush Material`, bound to the brush Image,
  with a scalar channel for the state parameter (2-4 frames). UMG writes the tween into the brush's dynamic
  material (the object `Image.GetDynamicMaterial()` returns).
- The AS base declares the animation member and plays it. Forms proven by compile spike:

```angelscript
UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
UWidgetAnimation Anim_Hover;          // optional animation
UPROPERTY(BindWidgetAnim, Transient)
UWidgetAnimation Anim_Equip;          // required animation
```

  `BindWidgetAnimOptional` as a plain specifier is REJECTED ("Unknown property specifier"); `meta=(OptionalWidget=true)`
  does not make an animation optional. A new WBP missing a required animation compiles with a WARNING naming the
  property. Binding is by NAME: the animation's FName must equal the property name exactly (rename the WBP
  animation, section 6), `WidgetBlueprintCompiler.cpp:587`.
- CommonButtonBase hooks (UFUNCTION(BlueprintOverride)): `OnHovered/OnUnhovered/OnSelected/OnDeselected/OnPressed/
  OnReleased/OnClicked/OnEnabled/OnDisabled`. The `BP_` names do not exist.

```angelscript
UFUNCTION(BlueprintOverride) void OnHovered()   { if (IsValid(Anim_Hover)) PlayAnimationForward(Anim_Hover); }
UFUNCTION(BlueprintOverride) void OnUnhovered() { if (IsValid(Anim_Hover)) PlayAnimationReverse(Anim_Hover); }
```

- ONE writer per parameter: the animation owns every scalar it keys. Script plays the animation and never also
  calls `SetScalarParameterValue` on that name (an animation keying a second scalar, e.g. Button_2 `Vignette Size`,
  counts: never set it from script either).
- REST POSE is a design decision, not the instance value. A state scalar's value on the instance is meaningless
  (the pack's `MI_UI_Button3` rests at `Hovered=1.0`). The base settles the animation at frame 0 in Construct:

```angelscript
UFUNCTION(BlueprintOverride) void Construct()
{
	if (IsValid(Anim_Hover))   // speed 100 lands on frame 0 in the first tick, no visible hover-out tween
		PlayAnimation(Anim_Hover, Anim_Hover.GetEndTime(), 1, EUMGSequencePlayMode::Reverse, 100.0);
}
```

  This is the same call the pack's `WBP_Applications_Button_3` Event Construct makes
  (`PlayAnimation(Hovered, StartAtTime=GetEndTime, Reverse)`); a port that deletes that graph chain loses the rest pose.
- Toggle states (equip): `SetIsToggleable(true)` / `OnSelected` plays `Anim_Equip` forward, `OnDeselected` reverse;
  slot exclusivity through a Transient `UCommonButtonGroupBase` on the grid (`AddWidget(slot)` works,
  `SetSelectionRequired`, `OnSelectedButtonBaseChanged`).
- Animations also key text colour / RenderTransform tracks; keep them as authored. Never create animations through
  VibeUE `create_animation` (compiler ensure, FND-01257): duplicate a WBP that already has one.

## 4. Script-driven values

- Write through a helper that re-fetches the MID on EVERY write; never cache it (it goes stale after
  `SetBrushFromMaterial` or when an animation restore re-points the brush):

```angelscript
namespace MatLabMaterial
{
	void SetScalar(UImage Image, FName Param, float Value)
	{
		if (!IsValid(Image)) return;
		UMaterialInstanceDynamic MID = Image.GetDynamicMaterial();
		if (IsValid(MID)) MID.SetScalarParameterValue(Param, Value);
	}
}
```

  Reference implementation: `Script/UI/MaterialLab/MatLabMaterial.as` (+ `SetVector`, `SetTexture`).
- Per-instance looks: `UPROPERTY(EditAnywhere) UMaterialInterface FillMaterial;` on the base and
  `FillImage.SetBrushFromMaterial(FillMaterial)` in PreConstruct (`MatLab_FillWidgetBase`, `MatLab_InventorySlotBase`).
  The WBP instance then picks the `MI_*` in the details panel; no Blueprint variable.
- Fill clamp (0..1) and segment quantise are the only pure rules worth unit-testing (`MatLabRules` + `_Test.as`).
- Segmented bar `M_UI_ProgressBar_Segmented`: the master animates on its own time-driven off state when the static
  switch `bSetProgressValueManually` is off (pack default, FND-01278). Port instances set the switch ON and script
  writes `ProgressValue` 0..1; `Num Steps` is a per-instance constant, not a per-frame value.
- Parameter names by family: button `Hovered` (`IsHovered` + `Vignette Size` on Button_2), slot `Is Hovered`/`Is Active`,
  liquid `Fill amount`, objective `Fill progress`, segmented `ProgressValue`.

## 5. Python inspection recipes

Works:

```python
import unreal
# animations and what they key (load the WBP first so its animations exist)
unreal.EditorAssetLibrary.load_asset("/Game/UI/MaterialLab/WBP_MatLab_Button")
for a in unreal.ObjectIterator(unreal.WidgetAnimation):
    if a.get_outer() and a.get_outer().get_name() != "WBP_MatLab_Button": continue
    for b in unreal.MovieSceneSequenceExtensions.get_bindings(a):          # b.get_name() = bound widget
        for t in b.get_tracks():
            print(a.get_name(), b.get_name(), t.get_class().get_name(), t.get_property_path(),
                  [str(n) for n in t.get_editor_property("brush_property_name_path")])
            for s in t.get_sections():
                print([(c.get_name(), [(k.get_time().frame_number.value, k.get_value()) for k in c.get_keys()])
                       for c in s.get_all_channels()])
# master parameter names / instance overrides
mel = unreal.MaterialEditingLibrary
print([str(n) for n in mel.get_scalar_parameter_names(unreal.load_asset(master_path))])
print({str(p.parameter_info.name): p.parameter_value for p in mi.get_editor_property("scalar_parameter_values")})
# lineage of a WBP generated class
print(unreal.MathLibrary.class_is_child_of(unreal.load_class(None, path + "." + name + "_C"),
                                           unreal.load_class(None, "/Script/Angelscript.MatLab_X")))
# graph leftovers / BP variables
print(unreal.BlueprintService.list_graphs(path), unreal.BlueprintEditorLibrary.list_member_variable_names(bp))
```

Does not work / use instead:

- `WidgetBlueprint.animations`, `.widget_tree`, `.parent_class` are NOT readable from Python (use the
  `ObjectIterator(WidgetAnimation)` filter on outer and `class_is_child_of` above).
- `get_widget_snapshot` OMITS NamedSlot content widgets; `get_hierarchy` includes them (rows carry
  `widget_class_path`; `widget_class` is the bare name only).
- `AnimationBindings` is protected (`is protected and cannot be read`).
- `Anim_*` members of a live PIE instance ARE readable: `inst.get_editor_property("Anim_Hover")` (exact declared name).
- `list_animations` DisplayLabel of a renamed animation shows the old label (cosmetic; the object name is bound).

## 6. Copy/remap route for porting an existing WBP family (ROUTE C)

The pack stays read-only. A duplicated page keeps child-widget CLASS references to the ORIGINAL components, and
`AddComponent` resolves WBP types by bare asset name with a static cache, so every copy needs a UNIQUE asset name
(`WBP_MatLab_*`). Per WBP:

```python
import unreal
WS, EAL, BEL = unreal.WidgetService, unreal.EditorAssetLibrary, unreal.BlueprintEditorLibrary
SRC = "/Game/UIMaterialLab/Widgets/Components/WBP_Applications_Button"
DST = "/Game/UI/MaterialLab/WBP_MatLab_Button"
EAL.duplicate_asset(SRC, DST)                                    # unique name; never advanced copy
# 1. child WBP instances -> the port copies (compile the target WBP first; class path needs _C)
r = WS.replace_widget_class(DST, "WBP_Applications_Button_3_0", "/Game/UI/MaterialLab/WBP_MatLab_Button3.WBP_MatLab_Button3_C")
print(r.success, r.error_message, r.named_slot_content_survived)  # 4th arg keep_named_slot_content=True (default)
# 2. delete EventGraph nodes and Blueprint variables that shadow AS UPROPERTYs (BEFORE reparent: same-name ICE)
# 3. rename tree widgets to the BindWidget names (animation bindings follow)
WS.rename_widget(DST, "ButtonBackground", "Background")
# 4. rename the animation object AND its MovieScene subobject to the AS property name
bp = EAL.load_asset(DST)
a = [x for x in unreal.ObjectIterator(unreal.WidgetAnimation) if x.get_outer() == bp and x.get_name() == "Hover"][0]
scenes = [m for m in unreal.ObjectIterator(unreal.MovieScene) if m.get_outer() == a]
a.modify(); a.rename("Anim_Hover")
for m in scenes: m.modify(); m.rename("Anim_Hover")
# 5. reparent, compile, verify, save
BEL.reparent_blueprint(bp, unreal.load_class(None, "/Script/Angelscript.MatLab_MaterialButtonBase"))
cr = unreal.BlueprintService.compile_blueprint(DST)               # errors / warnings lists
EAL.save_asset(DST, False)
```

- One `replace_widget_class` per child UserWidget instance; NamedSlot CONTENT children keep their pack class until
  replaced separately. Replace the tiles first, then their content.
- Remap check: iterate `ObjectIterator(unreal.UserWidget)` for instances under the WBP and print
  `get_class().get_path_name()`, or read `widget_class_path` from `get_hierarchy`. Assert no path starts with the pack folder.
- NEVER `begin_advanced_copy_packages` + `consolidate_assets` against the pack (FND-01275): consolidate leaves
  ObjectRedirectors, dirties/saves ~38 pack packages and `p4 edit`s pack WBPs; advanced copy adds copies to the
  DEFAULT changelist.
- `duplicate_asset`/`save_asset` make the editor SCC `p4 add` new `.uasset`s into the DEFAULT CL: reopen them into the
  item CL.
- `save_asset(path, False)`; `execute_python_code` `auto_save` defaults true and saves ALL dirty packages.

## 7. Review checklist

| Smell | Fix |
|---|---|
| New master whose parameter set equals an existing master | Instance of the existing master |
| Instance identical to a sibling | One instance; point both users at it (document, do not delete pack assets in use) |
| State scalar written by both an animation and script | Animation owns it; script plays the animation |
| `UButton` hitbox over an `Image` | `UCommonButtonBase` parent (owns hover/press/selected); no UButton |
| Blueprint variable holding a material ref (`SlotMaterial`) | `UPROPERTY(EditAnywhere) UMaterialInterface` on the AS base + `SetBrushFromMaterial` |
| Cached `UMaterialInstanceDynamic` member | Re-fetch `Image.GetDynamicMaterial()` per write |
| CommonButtonBase child draws a grey box | Style None draws a default box: assign a `CommonButtonStyle` with NoDrawType (`CBS_MatLab_Transparent`) on the CDO AND on every WBP instance (CDO Style does not reach instances; FND-01277) |
| Port deleted a Construct pose chain | Rest pose lost: settle the animation at frame 0 in the base `Construct` (section 3) |
| Segmented bar animating by itself | Pack off state is time-driven: switch `bSetProgressValueManually` on and drive `ProgressValue` |
| Copy page still references pack child classes | `replace_widget_class` per child instance (section 6) |
