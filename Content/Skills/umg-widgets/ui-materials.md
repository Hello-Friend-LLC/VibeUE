---
name: umg-widgets/ui-materials
description: UI-domain material patterns for widgets (Epic UIMaterialLab, proven on the SpaceWasteCollector Applications-page port, WI-1548) — when a material beats a brush, master vs instance rule, WBP animations that key one state scalar, script-driven fill values, Python recipes for inspecting animations and materials, the copy/remap route for porting an existing WBP family, and the review checklist.
---

# UI materials: look = instance, state = one scalar

Measured on UE 5.8 Hazelight (2026-10-09). Pack = Epic UIMaterialLab (`/Game/UIMaterialLab`): guidance, not
gospel. It ships duplicate masters and Blueprint-variable/graph logic that an instance and an AS base replace.
Base-class rules: `script-base-first.md`. The patterns here apply to any project's UI materials, with or without
the pack (it was only the source of the example screen); reference AS code: `ui-patterns-reference.md`.
What the engine does underneath (alpha convention, tint, UV channels and widget size, Time clock, MID/invalidation,
retainer refresh, batching, MPCs, textures, fonts, checklist, symptom table): `ui-material-engine-facts.md`.
The pack's 40 reusable functions, aspect mechanism and which masters to build on: `ui-material-lab-catalogue.md`.

## 1. When a UI material beats a brush/texture

- Use a UI-domain material (`MD_UI`, `BLEND_Translucent`) when the look is procedural (rounded SDF box, outline,
  gradient, liquid fill, segmented bar, halftone), must stay crisp at any size/DPI, or must tween between states.
  Full mechanism table (RoundedBox brush, Box nine-slice, ProgressBar, widget animation, RetainerBox,
  BackgroundBlur vs material): `ui-material-engine-facts.md` §1.
- Use a plain texture brush for a flat art asset with no state (icon, photo).
- Author Translucent masters as Final Color = straight colour, Opacity = coverage (Slate does not premultiply;
  the whole pack follows this). Aspect-correct shapes read the widget size through `GetUserInterfaceUV` ->
  `Pixel Size` (`MF_UI_AspectRatio`), never a script-written size (`ui-material-lab-catalogue.md` §2).
- UI `Time` never pauses or dilates (app clock, `Ignore Pause` is a no-op): anything that must freeze on pause is
  a script-written scalar.
- State lives in ONE scalar per state (`Hovered`, `Is Active`, `Fill amount`) that the master lerps between its
  style endpoints (`Button Color - default` / `- hovered`). A brush swap per state is the thing to avoid for a FEW
  widgets; for many instances (grids, tile views) the measured rule is shared state MICs plus a transient tween
  MID (section 3, measured-on-fork 2026-10-28 WI-1641).

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

## 3. State tweens: WBP animation keys the scalar (few-instance widgets) / state MICs + UI-Time lerp (many)

**Measured drive rule (measured-on-fork 2026-10-28 WI-1641; PIE, 8x8 grid + CommonTileView, 256 recycled items; CPU/GPU cost not
resolved).** Four drive models were compared: A = WBP animation track, B = cached MID + timer, C = material `Time`
lerp, D = shared state MICs with a MID only while tweening.

| | A anim track | B cached MID+timer | C material Time | D state MICs |
|---|---|---|---|---|
| MIDs per 64 slots | lazy per touched slot, KEPT; rest-settle in Construct makes all 64 | 64 | 64 | 0 idle |
| Slate batches (baseline 12) | +1 untouched, +64 touched | +64 | +64 | +4 (4 MICs) |
| Idle work | 0 (+1 ticked while playing) | 0 (a Tick variant: +64 ticks/frame) | 0 | 0 |
| While paused | animates | freezes | animates | freezes |
| Recycle with snap-on-bind | clean (StopAllAnimations + settle) | clean | clean | clean |

- The anim track creates a per-instance MID that is kept, so N touched slots = N batches (distinct MIDs never
  batch; a shared MIC can). The Construct rest-settle below makes MIDs for ALL instances. Animations run while paused.
  Recycle = `StopAllAnimations` + settle. Scope this section's animation recipe to few-instance widgets.
- Recommended for lists/grids: **D+C hybrid.** Rest = one shared state MIC per state combination. On a change: a
  transient MID (MIC parent) gets `<X>From/<X>Amount/<X>Start`; the master lerps on UI Time (zero CPU, keeps running
  while paused); a one-shot timer at Start+Duration swaps back to the target MIC and drops the MID. `StartTime` comes
  from `Gameplay::GetAccurateRealTime` (the UI `Time` clock, 20 s ramp measured h .02/.26/.50 at +0.5/5.4/10.2 s;
  float32 Time gets coarse after days of uptime: reasoned, not measured). No world timers for the tween itself: they
  freeze on pause. The hybrid was measured as parts (D batching, C clock), not built as one unit.
- Library: HF_Common `MF_HF_UiStateLerp(From, To, Start, Duration)` (Start default -1e6 so MICs show Amount) and
  `UHF_UiMaterialStateDriver` (`Setup(UImage, StateMics, Channels, Duration)`, `SetChannel(Name, Target, bAnimate)`,
  `Snap(model)` from Construct / `OnListItemObjectSet`, `SetReducedMotion`, `Release()` from `OnEntryReleased`).
- Negative control: skipping the reset on recycle leaks state for A, B and C (stale hover/select on reused entries);
  D did not reproduce a leak (cause not investigated).

The animation recipe (few-instance widgets, one animation moving more than one scalar):

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
- Animations also key text colour / RenderTransform tracks; keep them as authored. To author a NEW material-track
  animation use the section 5 route (`create_animation` + `add_animation_track`, then `copy_tracks`/`paste_tracks`
  with `BrushPropertyNamePath` injected; measured-on-fork 2026-10-28 WI-1641, scalar params only, played in PIE; FND-01257 was not
  re-tested), otherwise duplicate a WBP that already has one.

## 4. Script-driven values

- One owner per brush (measured-on-fork 2026-10-28 WI-1641): a brush whose material an ANIMATION drives is re-fetched on every write and
  never cached (it goes stale after `SetBrushFromMaterial` or when an animation restore re-points the brush); a
  brush the SCRIPT owns (state-MIC driver) may cache its own MID or swap MICs, because nothing else re-points it.
  `HF_UiMaterial::SetScalar` on a brush that currently holds a MIC silently creates a MID (a new batch). Helper for
  the animation-owned / one-off case:

```angelscript
namespace HF_UiMaterial
{
	void SetScalar(UImage Image, FName Param, float Value)
	{
		if (!IsValid(Image)) return;
		UMaterialInstanceDynamic MID = Image.GetDynamicMaterial();
		if (IsValid(MID)) MID.SetScalarParameterValue(Param, Value);
	}
}
```

  Compiled in HF_Common: `HF_Common/Script/UI/Material/HF_UiMaterial.as` (+ `SetVector`, `SetTexture`); call it,
  do not re-copy it (`ui-patterns-reference.md`).
- Per-instance looks: `UPROPERTY(EditAnywhere) UMaterialInterface FillMaterial;` on the base and
  `FillImage.SetBrushFromMaterial(FillMaterial)` in PreConstruct (`UHF_MaterialFillBase`, `UHF_SlotButtonBase`).
  The WBP instance then picks the `MI_*` in the details panel; no Blueprint variable.
- Fill clamp (0..1) and segment quantise are the only pure rules worth unit-testing (`HF_UiFillRules` + `HF_UiFillRules_Test.as`).
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
                                           unreal.load_class(None, "/Script/Angelscript.HF_MaterialButtonBase")))  # class name without the U prefix
# graph leftovers / BP variables
print(unreal.BlueprintService.list_graphs(path), unreal.BlueprintEditorLibrary.list_member_variable_names(bp))
```

- `brush_property_name_path` on a `MovieSceneWidgetMaterialTrack` CANNOT be read or set from Python (FND-02052; the
  print in the loop above is not reliable, measured-on-fork 2026-10-28 WI-1641); the `create_animation` + `add_animation_track` result has an empty path and
  does not drive the brush. Working authoring route: create the animation and track with VibeUE, then
  `LevelSequenceEditorSubsystem.copy_tracks` / `paste_tracks` with `BrushPropertyNamePath(0)="Brush"` injected in the
  copied text (spike scripts `spike_track_t3d.py` in PrjEfz `Saved/claude_tmp/WI-1641/`; played in PIE, scalar tracks only).

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
BEL.reparent_blueprint(bp, unreal.load_class(None, "/Script/Angelscript.HF_MaterialButtonBase"))
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
| Cached `UMaterialInstanceDynamic` member on an animation-driven brush | Re-fetch `Image.GetDynamicMaterial()` per write (a script-owned brush may cache its own MID, section 4) |
| N per-slot MIDs (or a per-slot animation track) across a grid | Shared state MICs + transient tween MID (section 3) |
| CommonButtonBase child draws a grey box | Style None draws a default box: assign a `CommonButtonStyle` with NoDrawType (`CBS_MatLab_Transparent`) on the CDO AND on every WBP instance (CDO Style does not reach instances; FND-01277) |
| Port deleted a Construct pose chain | Rest pose lost: settle the animation at frame 0 in the base `Construct` (section 3) |
| Segmented bar animating by itself | Pack off state is time-driven: switch `bSetProgressValueManually` on and drive `ProgressValue` |
| Copy page still references pack child classes | `replace_widget_class` per child instance (section 6) |
