---
name: umg-widgets/ui-patterns-reference
description: The compiled HF_Common UI-material AngelScript bases (HF_UiMaterial write helper, HF_UiFillRules + test, UHF_MaterialButtonBase with rest-pose settle, UHF_SlotButtonBase, UHF_MaterialFillBase, UHF_SlotGridBase with CommonButtonGroup, UHF_TileBase) and the contract they follow — derive or reparent onto them; needs no Epic UIMaterialLab pack. Read before writing any AS widget base that drives a UI material.
---

# UI pattern reference (the compiled HF_Common bases)

The classes live in `HF_Common/Script/UI/Material/` (engine fork `Engine/Plugins/HF_Plugins/HF_Common`; HF_Common.uplugin
depends on CommonUI). Promoted from the SpaceWasteCollector Applications-page port (WI-1548) in WI-1550 and proven on
UE 5.8 Hazelight. The source pack (Epic UIMaterialLab) is NOT required. Process: `screen-process.md`; materials and
animations: `ui-materials.md`; base-class rules: `script-base-first.md`.

**Use the compiled classes: parent a WBP on one of them (`/Script/Angelscript.HF_MaterialButtonBase`, no `U` in the
`load_class` path) instead of writing a parallel base. The source is the reference: read the `.as` file, and copy it
only to adapt a variant.** Verify any native signature against the generated AS API dump / compiler (a dump miss proves
nothing).

| Class (file `HF_<name>.as`) | Derives | Role |
|---|---|---|
| `HF_UiMaterial` namespace (`HF_UiMaterial.as`) | none | `SetScalar/SetVector/SetTexture(UImage, FName, value)` uncached MID writes |
| `HF_UiFillRules` namespace (+ `HF_UiFillRules_Test.as`) | none | pure `ClampFill`, `QuantizeSegments`; world-free tests `Test_HF_UiFillRules*` |
| `UHF_MaterialButtonBase` (`HF_MaterialButtonBase.as`) | `UCommonButtonBase` | `Background`, optional `Label`, `ButtonText`, optional `Anim_Hover/Pressed/Focused/Selected/Disabled`; rest-pose settle in `Construct` |
| `UHF_SlotButtonBase` (`HF_SlotButtonBase.as`) | `UHF_MaterialButtonBase` | selectable/toggleable slot, `SlotMaterial`, optional `Anim_Equip`, `SetItemTexture` |
| `UHF_SlotGridBase` (`HF_SlotGridBase.as`) | `UUserWidget` | `Grid` (`UUniformGridPanel`) + transient `UCommonButtonGroupBase` for exclusive selection, `OnSlotSelected`, `GetSelectedIndex` |
| `UHF_MaterialFillBase` (`HF_MaterialFillBase.as`) | `UUserWidget` | `FillImage`, `FillParamName` (default `Fill amount`), `FillMaterial`, `InitialFill`, `SetFill/GetFill` |
| `UHF_TileBase` (`HF_TileBase.as`) | `UUserWidget` | sized container + title + content NamedSlot; binds `SizeBox_Container`, `Text_Name`, `NamedSlot_203`, `Image_Shadow`; no material writes |

All log through `HF_Log` category `n"HF_Ui"`; designer properties use category `"HF UI"`. The bases are final in the
project: a project WBP is parented directly to them (no per-project shell class).

## Contract every base follows

1. **Animation is the only writer of a state parameter.** The WBP animation keys the material scalar
   (`Hovered`, `Is Active`); script only plays it. Script writes only script-owned values (fill amount, item
   texture), never a parameter an animation also keys.
2. **One scalar per state**; look = a UI-domain material instance, not a new master.
3. **Uncached MID writes**: re-fetch `Image.GetDynamicMaterial()` on every write (`HF_UiMaterial::SetScalar`); the MID
   goes stale after `SetBrushFromMaterial` or an animation restore.
4. **Pure rules in a namespace** (no UObjects), unit-tested world-free (`HF_UiFillRules`); everything else is thin glue.
5. **UCLASS(Abstract) base, WBP holds tree/art/animations only**: zero EventGraph nodes, zero Blueprint variables.
   Animations bind through `UPROPERTY(BindWidgetAnim, Transient)`; the optional form is
   `UPROPERTY(Transient, meta = (BindWidgetAnimOptional))` (the bare keyword is rejected). The animation FName must
   equal the property name or it never binds.
6. **Buttons derive `UCommonButtonBase`** (CommonUI owns hover/press/selected; no separate `UButton` hitbox). A
   `UCommonButtonBase` child with `Style` None draws a default grey box: assign a `CommonButtonStyle` with
   NoDrawType on the class default AND on every existing WBP instance (CDO value does not reach instances).
7. **Rest pose**: a hover animation's frame 0 is the rest state, not the material instance default. Settle it at
   Construct (below), or an instance whose default is the hovered value rests hovered.

## Kit bases (HF_Common/Script/UI/Kit)

All `UCLASS(Abstract)`; Binds are required `BindWidget` unless `opt` (`...Optional`).

| Class | Derives | Binds | Role |
|---|---|---|---|
| `UHF_KitBarBase` | `UUserWidget` | `Bar` (UProgressBar); opt `ValueText`, `Icon`, `Anim_Low`, `Anim_Critical` | `SetValue(Cur, Max)`, `SetIconTexture` |
| `UHF_LampBase` | `UUserWidget` | `Lens`; opt `Glyph`, `StatusText` | `SetState(EHF_LampState, FText)` |
| `UHF_GaugeBase` | `UUserWidget` | `Needle`; opt `ValueText` | `SetNormalized`, `SetValue(Cur, Max)` |
| `UHF_PipRowBase` | `UUserWidget` | `Pips` (UHorizontalBox) | `SetLevel(Level, Max)` |
| `UHF_CounterBase` | `UUserWidget` | `Digits`; opt `Anim_Tick` | `SetValue(int)` |
| `UHF_TooltipBase` | `UUserWidget` | `Body`; opt `Title`, `Arrow` | `SetText`, `SetArrowSide` (rotates only) |
| `UHF_TimingTrackBase` | `UUserWidget` | `Track`, `Needle`, `WindowBand`, `PerfectBand`; opt `WarningBand`, `Anim_Success/Perfect/Fail/Cancelled` | `SetWindow`, `SetNeedle`, `ShowResult` |
| `UHF_ActionDetailsWidget_KeycapKey` | `UHF_ActionDetailsWidget_DisplayInputKey` | opt `Keycap` | key label on keycap art |

Pure rules: `HF_UiKitRules`, `HF_UiTimingRules` (+ `_Test.as`). `UHF_MaterialButtonBase` anim slots are optional, played by
the matching event; the WBP binds them by animation NAME.

## Texture-brush variant of the contract

- State overlays are plain Image layers (HitTestInvisible, slot Fill), each keyed by ONE animation (opacity); never
  `BindWidget` members; a tint above 1 is clamped, so a lighter state is a light overlay.
- Box brush margins come from the project's brush table (see the project's `kit_brushes.json`), never the pack's
  candidates. Slate draws Box corners at margin x TEXEL size after `max_texture_size`, not the brush ImageSize:
  `button_base` 512, `header_strip` 1024, `frame_panel` 512, `frame_slot` 256 are intentional caps; never re-import
  them with another max.

## Adapting a variant

- Another family's fill parameter: set `FillParamName` per WBP instance (`Fill progress`, `ProgressValue`); do not
  fork `UHF_MaterialFillBase`. A per-instance brush needs `FillMaterial` as a property: widget tooling cannot address a
  nested child image per instance, so do not wrap the fill in a one-off child WBP to vary its brush.
- Another animated state: add a `BindWidgetAnimOptional` member on a derived project class and play it from
  `OnSelected/OnDeselected` the way `UHF_SlotButtonBase` does. The hook names are
  `OnHovered/OnUnhovered/OnSelected/OnDeselected` (the `BP_`-prefixed names do not exist).
- Another tile: the tile base keeps the source pack's auto-generated widget names so a duplicated WBP binds without
  renames. A new family renames the widgets in its own base AND via `WidgetService.rename_widget` in the WBP.
- Rest-pose settle (the one non-obvious body): play `Anim_Hover` in reverse from its end at speed 100 so it lands on
  frame 0 in the first tick, with no visible hover-out tween at spawn:
  `PlayAnimation(Anim_Hover, Anim_Hover.GetEndTime(), 1, EUMGSequencePlayMode::Reverse, 100.0);`
- Grid selection: `Group.SetSelectionRequired(false)`, add each `UHF_SlotButtonBase` child of `Grid`, bind
  `Group.OnSelectedButtonBaseChanged` to a `UFUNCTION()` taking `(UCommonButtonBase, int)`. Rename the tree widget to
  `Grid` and mark it IsVariable.
- Keep tests cheap and world-free: hot reload runs every unit test of the recompiled module.

## Verify what compiling cannot

Compile and lineage checks passed while three real defects (grey button box, wrong rest pose, oversized fill)
only showed in the PIE parity ladder (`screen-process.md` step 5). Capture the pair in the same game-thread tick
(animated regions differ otherwise), and treat an exact 0.00 diff as proof only after confirming the two images
come from two different assets.
