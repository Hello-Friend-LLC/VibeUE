---
name: umg-widgets/screen-process
description: The one end-to-end process for implementing a UI screen — design ref to element list, material/instance plan, AngelScript base per logic-bearing widget, WBP creation, the verification ladder (compile, lineage, preview, PIE captures at 3 resolutions with 5 states, parity diff), staging, review checklist — with the WI-1548 Applications-page port as the worked example.
---

# Implement a UI screen: design ref -> material plan -> AS base -> WBP -> verify

Follow in order. Details live in sibling docs: `script-base-first.md` (base template),
`ui-materials.md` (materials, animations, Python recipes, copy/remap route), `ui-patterns-reference.md` (copyable
AS base code). This process applies to ANY screen in ANY project; no Epic pack is needed. The Epic UIMaterialLab
page below is only the worked example (its design ref and copy/remap route apply when porting an existing WBP
family; for a new screen skip the copy route). Do not restate the sibling docs here.

## 1. Inputs -> element list

- Inputs: the mockup / design reference and the project's `Docs/claude/hf-ui-design-brief.md` (type scale
  14/18/22/32/48, 8 px grid, the 5 control states, palette, 1920x1080 authoring size).
- Output: a flat element list (element, kind: static art / label / clickable / value display / list, states it needs).
  Mark every element that has data, state or input: those get an AS base (step 3).

## 2. Material and instance plan

Table, one row per styled element (reuse existing masters first; check the project's review doc, e.g.
`DesignDocs/UI/ui-material-usage-review.md` section 6, before adding a master):

| Element | Master | Instance (new or existing) | State params | Writer (anim / script) |
|---|---|---|---|---|

- New master only for a different node graph; otherwise an instance of the family master (`ui-materials.md` sec 2).
- Every state parameter has exactly one writer: `anim` (hover/press/equip, keyed on the WBP) or `script`
  (fill/progress). Name state params per `ui-materials.md` sec 2.
- Create instances with `EditorAssetLibrary.duplicate_asset(src_instance, "/Game/UI/<Screen>/MI_<X>")`; set
  overrides via `MaterialEditingLibrary` and read them back.

## 3. AS base per logic-bearing widget

- Template: `script-base-first.md` sec 1; `UCLASS(Abstract)`, `BindWidget` members, all behaviour in script.
- Anything clickable: `UCommonButtonBase` (`script-base-first.md` sec 1b): `Anim_*` `BindWidgetAnim` members,
  `OnHovered/OnUnhovered/OnSelected/OnDeselected` play them, `Construct` settles the rest pose (`Anim_Hover`
  frame 0). Assign a transparent `CommonButtonStyle` (NoDrawType in every state) on the CDO AND on every WBP
  instance (CDO Style does not reach instances):
  - create: `BlueprintService.set_property(style_bp, prop, "(DrawAs=NoDrawType)")` for NormalBase/Hovered/Pressed,
    SelectedBase/Hovered/Pressed, Disabled;
  - CDO: `BlueprintService.set_property(wbp, "Style", "/Game/UI/<Screen>/CBS_X_Transparent.CBS_X_Transparent_C")`;
  - each instance: `WidgetService.set_property(host_wbp, instance_name, "Style", "<same class path>")`.
- One-scalar looks (liquid, objective, segmented bar): the Fill-widget pattern (`UImage FillImage`,
  `FName FillParamName`, `UMaterialInterface FillMaterial`, `SetFill()` through an uncached
  `Image.GetDynamicMaterial()` write).
- Selection exclusivity: a Transient `UCommonButtonGroupBase` on the grid base (`AddWidget`, `SelectButtonAtIndex`).
- Pure rules (fill clamp, segment quantise) in a namespace with a unit test file `<Name>_Test.as`; keep tests
  cheap and world-free (every hot reload runs every unit test of the recompiled module).
- Compile gate: `build_watch.py --mark` before the save, `--since-mark` after. A `UPROPERTY`/`UFUNCTION`/
  `BindWidget` change is a Full Reload: stop PIE first. Run the tests through the live editor console:
  `ue_driver.py console "Automation RunTests Angelscript.UnitTests.UI.<Screen>"`, then
  `ue_driver.py log grep "Test Completed. Result=" --offset <mark>` (editor log `Saved/Logs/swc-driver-editor.log`).

## 4. WBP creation

- New widget: `WidgetBlueprintFactory.set_editor_property("parent_class", load_class(None, "/Script/Angelscript.<Class>"))`
  then `AssetTools.create_asset(...)` (runnable `scripts/create_widget_from_script_base.txt`). Tree: names equal the
  BindWidget members, `is_variable=True`; brushes are the planned `MI_*` instances; animations keyed on the state
  scalar of the brush (`ui-materials.md` sec 3); per-instance looks/text via `EditAnywhere` UPROPERTYs set with
  `WidgetService.set_property`. No EventGraph logic, no Blueprint variables.
- Porting an existing WBP family: the Route C copy/remap (unique names, `replace_widget_class`, delete graph nodes,
  `rename_widget`, animation rename, reparent): `ui-materials.md` sec 6.
- Compile each WBP (`BlueprintService.compile_blueprint`), `EditorAssetLibrary.save_asset(path, False)`.

## 5. Verification ladder (each rung before the next)

1. Compile: errors `[]`, warnings `[]`; `list_graphs` shows an empty EventGraph; `list_member_variable_names` has no user
   variables.
2. Lineage and tree: `MathLibrary.class_is_child_of(load_class(None, path + "." + name + "_C"), base)` is True;
   `get_hierarchy` (NOT `get_widget_snapshot`, which omits NamedSlot content) shows every BindWidget name as a
   variable; every child `widget_class_path` is under the new folder (no pack references).
3. `WidgetService.capture_preview` (design time, static: hover/press states are NOT exercised here).
4. PIE capture, `capture_image source=game` (Slate composited) at 1920x1080, 3440x1440, 1280x720. NEVER
   `pie_screenshot`/`pie capture` for UMG. Gate each PNG with `img_check.py --after <epoch>`. Rig forms that worked:
   - spawn: `WidgetService.spawn_widget_in_pie(path)` after `ue_driver.py pie start`;
   - hover: `PlayerController.set_mouse_location(x, y)` (viewport px) after `show_mouse_cursor = True` +
     `WidgetLibrary.set_input_mode_ui_only_ex(pc, None, DO_NOT_LOCK, False)`; read back with
     `UserWidget.is_hovered()`;
   - press / select: an OS click (ctypes `mouse_event`) AFTER the PIE window is foregrounded
     (`SetForegroundWindow`) because the Claude window covers it; `pie_send_input` does not reach Slate;
   - exercise all five states for each interactive control: Normal, Hovered, Pressed, Selected/Active (and Focused
     by gamepad/keyboard), Disabled; log each transition from the base (`HF_Log::Verbose(n"<Cat>", ...)`) and match
     it with `ue_driver.py wait --log "<regex>"` / `log grep --offset`;
   - bindings: `inst.get_editor_property("Anim_Hover")` is non-null on every live button/slot instance.
5. Parity (when porting): capture the design-time pack page and the port at the same size with the mouse parked off
   the page; compute `mean_abs_diff` per tile region (Pillow; numpy is absent). Target 0.00 for static tiles;
   time-animated materials (halftone background, SDF bars, glows) differ by phase, compare a pulse-free mask
   or self-diff the same tile at two times for the noise floor.
6. Unit tests: `Test Completed. Result={Success}` per test in the reload log; also grep the log for `Full Reload is
   required`, `Error:.*<Cat>`, `Accessed None` (all 0).
7. Dirty check: no pack/source package dirty after the session (`EditorAssetLibrary` dirty list empty).

## 6. Staging

- Source-controlled project (Perforce): stage exact paths into the item changelist (`p4_reconcile`), never submit.
  New `.uasset`s created by the editor (`duplicate_asset`, `save_asset`, factory create) land in the DEFAULT
  changelist through editor SCC: reopen them into the item CL. Verify with `p4_opened`.
- VibeUE skill docs live in the VibeUE git repo (branch hf-5-8): commit + push, then console `VibeUE.ReloadSkills`.

## 7. Review checklist

`ui-materials.md` sec 7 (master/instance, one writer, CommonButton Style, rest pose, NamedSlot replace) plus
`script-base-first.md` sec 7 (polling, tree lookups, logic in WBP graph, Abstract, shadowing BP variables).

## Worked example: WI-1548 Applications-page port (SpaceWasteCollector)

- Design ref: Epic UIMaterialLab page `9_MaterialLab_Applications` (pack, read-only: `/Game/UIMaterialLab`).
- Review + plan: `DesignDocs/UI/ui-material-usage-review.md` (17 masters, instance verdicts, section 6 plan table).
- AS bases `Script/UI/MaterialLab/`: `MatLab_MaterialButtonBase`, `MatLab_FillWidgetBase`, `MatLab_InventorySlotBase`,
  `MatLab_InventoryGridBase`, `MatLab_ModularTileBase`, `MatLab_HalftoneMenuBase`, `MatLab_ApplicationsPageBase`
  (+ `MatLabMaterial.as`, `MatLabRules.as`, `MatLabRules_Test.as`; 4 unit tests pass).
- WBPs `/Game/UI/MaterialLab/`: `WBP_MatLab_Applications` (page), `WBP_MatLab_Button`, `_Button2`, `_Button3`, `_Fill`,
  `_HalftoneMenu`, `_InventoryGrid`, `_InventorySlot`, `_ModularTile`, `_ObjectiveFill`; style `CBS_MatLab_Transparent`;
  instances `MI_ML_ProgressBar_1..3` (switch `bSetProgressValueManually` on, script drives `ProgressValue`).
  Zero EventGraph nodes, zero Blueprint variables, 0 pack widget references, pack never dirtied.
- Route C copy: 20 `replace_widget_class` calls on the page, NamedSlot content kept on all 10 tiles.
- Measured: all 10 tiles `mean_abs_diff` 0.00 vs the pack page after the fixes below (animated tiles drift by time
  phase only, FND-01278). Three defects were found ONLY by the ladder (step 5), none by compile or preview:
  1. Grey CommonButton box (FND-01277): ports derive `UCommonButtonBase` with Style None while the pack buttons are
     plain UserWidgets with an invisible UButton; fixed with `CBS_MatLab_Transparent` on CDOs AND per instance.
  2. Button3 rest pose: the pack's `Event Construct -> PlayAnimation(Hovered, StartAtTime=GetEndTime, Reverse)` settles
     frame 0; the port deleted the graph chain and rested at the instance default `Hovered=1.0`; fixed in
     `MatLab_MaterialButtonBase.Construct` (speed 100). Sci-fi menu tile 26.81 -> 0.00.
  3. ObjectiveFill sizing: the Fill root was larger than the pack's 100x100 images; fixed by 100x100 SizeBox wrappers.
