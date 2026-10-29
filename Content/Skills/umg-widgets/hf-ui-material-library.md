---
name: umg-widgets/hf-ui-material-library
description: How to use the HF_Common UI-material library (WI-1641, DEC-5685) in any HF project — the MF_HF_Ui* material functions and M_HF_UiSurface / M_HF_UiMeter masters with every parameter, the 7-MIC rest set and the python table that generates a style set, the three one-owner-per-brush modes and which HF_Common base uses which (UHF_StateSlotButtonBase + UHF_UiMaterialStateDriver, UHF_MaterialFillBase, anim-owned UHF_SlotButtonBase), driver lifecycle (OnInitialized, Snap on bind, Release on entry release, reduced motion), the list-entry recipe when a slot cannot inherit the base, when to use a plain brush instead, cost rules, a step-by-step decorate-a-slot recipe pointing at WBP_HFUIMaterialExample, and the PrjEfz UIMaterialLab as the worked inventory example.
---

# HF_Common UI-material library: how to use it

Library = `HF_Common` (engine fork `Engine/Plugins/HF_Plugins/HF_Common`, no project types, no `/Game` refs). Patterns and the
measured drive rule: `ui-materials.md` §3-4; engine facts (alpha, aspect, Time, batching): `ui-material-engine-facts.md`.
Every fact below is tagged **[WI-1641 measured]** (ran on the UE 5.8.3 fork: compile, editor Python readback or
`capture_preview_state` pixels) or **[pending p4-pie]** (needs a real PIE run, not yet measured). Nothing here was
measured in a shipped PIE build.

## 1. Material functions `/HF_Common/Materials/Functions/UI` (category "HF UI")

Contract as built (read from the assets and the builder scripts; matches DEC-5685 except where "drift" is noted).

| Function | Inputs | Outputs | Notes |
|---|---|---|---|
| `MF_HF_UiPixelSpace` | none | `PixelPos` (v2), `HalfSizePx` (v2) | reads `GetUserInterfaceUV` PixelSize (TexCoord 3): aspect-correct at any widget size, device pixels (DPI scaled). Never use TextureCoordinate/ViewSize for this [WI-1641 measured] |
| `MF_HF_UiBoxSdf` | `PixelPos`, `HalfSizePx`, `RadiusPx` | `DistPx` | rounded-box SDF in px, negative inside; radius clamped to the smaller half-size |
| `MF_HF_UiFWidth` | RTMSDF DefaultFWidth copy | | helper of the two below |
| `MF_HF_UiEdgeMask` | `SDF`, `PixelRange`, `Offset`, `Width` | `EdgeMask` | copy of RTMSDF `EdgeMask_Absolute`; feed SDF in px, `PixelRange` 1, `Width` 1 for a 1 px antialiased edge. Drift: DEC-5685 listed no pins; these are the built ones |
| `MF_HF_UiStroke` | `SDF`, `PixelRange`, `Offset`, `Width` | `StrokeMask` | copy of RTMSDF `Stroke_Absolute`; the band is CENTRED on SDF = 0, so for an inner stroke of width w feed `SDF + w/2` (this is what M_HF_UiSurface does) |
| `MF_HF_UiGradient` | `UV`, `ColorA`, `ColorB`, `AngleRad` | `Color` | linear gradient, corners hit exactly A/B; angle 0 = left to right, pi/2 = top to bottom; linear colours |
| `MF_HF_UiSegmentMask` | `U`, `Fill`, `Segments`, `GapPx` | `Mask` | lit segments = `floor(Fill*N + 1e-4)`, identical to `HF_UiFillRules::QuantizeSegments`; `Segments` 0 = continuous; reads the widget pixel width itself; gaps centred on boundaries, none at the outer ends |
| `MF_HF_UiStateLerp` | `From`, `To`, `Start`, `Duration` | `Value` | `lerp(From, To, saturate((Time - Start) / max(Duration, 1e-4)))`, Time = UI clock. `Start` default -1e6 so a MIC shows `To` |

RTMSDF copies are MIT (Richard Meredith AB 2022-2025): notice in each asset Description and in `Content/Materials/Functions/UI/NOTICES.md`
[WI-1641 measured: grep shows no `/RTMSDF` or `/Game` dependency].

## 2. Masters `/HF_Common/UI/Materials`

Both are MD_UI, Translucent, straight alpha: colour chain -> Final Color (`MP_EMISSIVE_COLOR`), coverage -> Opacity. Neither has a VertexColor
node: Slate multiplies `Image.ColorAndOpacity` once after the material, so a static tint works on a shared MIC (drift from DEC-5685,
which asked for a VertexColor multiply) [WI-1641 measured: tint applied once].

**`M_HF_UiSurface`** (rounded box + inner stroke):
- Shape: `CornerRadiusPx` 8, `StrokePx` 2.
- State: `HoverAmount/HoverFrom/HoverStart`, `SelectedAmount/SelectedFrom/SelectedStart` (Start default -1e6), `DragSourceAmount`, `EligibleAmount`,
  `InvalidAmount`, `StateDuration` 0.12, plus `StateFillMix` 0.35 and `HoverMix` 0.3 (added in P1; not in DEC-5685).
- Colours (LINEAR; alpha = opacity of the fill/stroke): `FillColor`, `StrokeColor`, `HoverColor`, `SelectedColor`, `DragSourceColor`, `EligibleColor`, `InvalidColor`.
- Composite order: base -> Selected -> DragSource -> Eligible -> Invalid by Amount, Hover overlay on top, suppressed while a drag state is on. DragSource also dims
  opacity to 55 %. Invalid reads by SHAPE too: a diagonal hatch (8 px period) over the fill, so it is not colour only.

**`M_HF_UiMeter`** (track + fill along width): `FillAmount` 1, `SegmentCount` 0 (0 = continuous), `SegmentGapPx` 2, `CornerRadiusPx` 4, `TrackColor`, `FillColor`.

Regenerate both: `Content/Python/hf_ui_masters.py` via `ue_driver.py py` (idempotent, deletes and recreates; `hf_ui_matlib.py` is the shared graph builder).
Authoring traps [WI-1641 measured]: `set_*_static_switch` always returns False (read pixels, not getters); never pass LAYER_PARAMETER for a non-layered
material (crashes the editor); verify a parameter by capture + `img_check`.

## 3. The 7-MIC rest set and generating a style

Seven rest MICs per style: `<Style>_{Idle,IdleHover,Selected,SelectedHover,DragSource,Eligible,Invalid}`. These names are the `StateMics` map keys
(`HF_UiStateRules::Key*`). Base axis precedence Invalid > Eligible > DragSource > Selected > Idle; Hover is an overlay suppressed in drag states. Only
Hover and Selected animate; drag states snap.

Generate (editor Python, `ue_driver.py py`), one table to seven instances:

```python
import hf_ui_styles   # HF_Common/Content/Python
paths = hf_ui_styles.make_surface_mics("/Game/UI/Materials/MyStyle", "MI_My",
        vectors={"FillColor": (r, g, b, a), "StrokeColor": (...), "SelectedColor": (...)})   # LINEAR values
meters = hf_ui_styles.make_meter_mics("/Game/UI/Materials/MyStyle", "MI_MyMeter", vectors={"FillColor": (...)})
```

- Colours are LINEAR: convert sRGB tokens first (PrjEfz: `EfzInventoryStyle::FromSRGB`; the PrjEfz worked script is `Content/Python/efz_ui_style_mics.py`).
  Hex written as linear renders wrong [WI-1641 measured: PrjEfz idle fill matched its token within 0 levels after conversion].
- `make_neutral()` makes the HF grey/blue set `MI_HF_UiSurface_*`, `MI_HF_UiMeter_Linear`, `MI_HF_UiMeter_Segmented`.
- Regenerating deletes and recreates the instances (same paths); keep paths stable. Create the MICs in your project, not in HF_Common.
- Editor asset creation lands in the Perforce DEFAULT changelist: reopen the new files into your item CL.

## 4. Three ownership modes (one owner per brush)

| Mode | Owner of the brush | HF_Common base | Use when |
|---|---|---|---|
| anim-owned | a WBP animation track (the only writer of the state scalar; script only plays it) | `UHF_SlotButtonBase` / `UHF_MaterialButtonBase` (`Anim_Hover`, `Anim_Equip`) | few instances (a button, a panel) |
| script value re-fetched | script writes one value through an uncached `Image.GetDynamicMaterial()` each time | `UHF_MaterialFillBase` (`FillParamName`, `SetFill`), `HF_UiMaterial::Set*` | per-instance data (fill amount, item texture) on few instances |
| driver-owned | `UHF_UiMaterialStateDriver`: rest = shared MIC, a transient MID only while tweening | `UHF_StateSlotButtonBase` (derives `UHF_SlotButtonBase`) | many instances (grids, tile views) |

Rules: a driver brush never gets per-instance data (put the item texture on a SEPARATE image, `ItemIcon`; static tint via `Image.ColorAndOpacity`);
never call `HF_UiMaterial::Set*` on a driver brush (on a MIC brush it silently creates a MID); `UHF_StateSlotButtonBase` logs a warning if any `Anim_*` is
bound (a second writer) [WI-1641 measured: no warning on the example slots].

## 5. The driver API and lifecycle

`UHF_UiMaterialStateDriver : UObject`, in `Script/UI/Material/HF_UiMaterialStateDriver.as`; pure rules in `HF_UiStateRules.as` (unit-tested).

```angelscript
// FHF_UiStateFlags { bHovered, bSelected, bDragSource, bDragEligible, bDragInvalid }  -- callers pass flags, never channel names
Driver = Cast<UHF_UiMaterialStateDriver>(NewObject(this, UHF_UiMaterialStateDriver));   // OnInitialized, Outer = the widget
Driver.Setup(StateImage, StateMics, 0.12f);          // TMap<FName, UMaterialInterface>; missing Idle key logs a warning
Driver.Snap(Flags);                                  // show now: MIC only, no MID (Construct / list-entry bind)
Driver.SetFlags(Flags, true);                        // hover / select tween; drag-state changes always snap
Driver.SetReducedMotion(true);                       // animation off = always snap
Driver.Release();                                    // clear timer, drop MID, show Idle (list-entry release)
```

- First `SetFlags` after `Setup`/`Release` snaps. An unknown `FromPreviewName` name -> idle flags.
- Tween = a transient MID (parent = target-state MIC) gets `<X>From/Amount/Start`; Start = `Gameplay::GetAccurateRealTime` (the UI Time clock); the
  master lerps; a one-shot `System::SetTimer` (duration + 0.02 s) swaps back to the MIC. No Tick, no looping timer, no world-timer tween.
- Retarget mid-tween starts from the in-flight value (`HF_UiStateRules::LerpAt`), so nothing jumps.
- Measured [WI-1641 measured, editor preview world, not PIE]: with `Log HF_Ui Verbose`, a forced tween logs `tween start IdleHover hover 0.0->1.0 ...` and
  `tween finish IdleHover` 72 ms later; idle 4 s after: 0 `HF_Ui` lines. A resting slot's `Background.Brush.ResourceObject` class is `MaterialInstanceConstant`
  (31 of 31 readbacks), never `MaterialInstanceDynamic`. Pool of 64 cells after `Release()+Snap()`: "MID brushes 0".
- **Pending p4-pie**: the real hover tween with a real pointer, the swap-back timer in a PIE world (world resolves through the widget Outer: source-read only),
  pause behaviour, drag states via `SetDragState`, the hybrid as one measured unit, GPU cost, real DPI.

`UHF_StateSlotButtonBase` surface (callers pass state): `StateMics`, `TransitionDuration`, optional `ItemIcon`, `SetDragState(bSource, bEligible, bInvalid)`,
`SetReducedMotion`, `ForceState(FName)` (a forced slot ignores pointer events until `ReleaseEntry`), `ApplyStyle(Mics, Duration)` (palette swap; pass the same
map contents to every slot so brushes still batch), `Snap(Flags)`, `ReleaseEntry()`, `IsBackgroundBrushDynamic()` (diagnostic). `PreConstruct` does not write
`SlotMaterial` (it would fight the Idle MIC).

HF_Log: one category `n"HF_Ui"`. A category registers lazily on first use, so a console `Log HF_Ui Verbose` typed before any HF_Log call is lost:
set `[Core.Log] HF_Ui=Verbose` or `-LogCmds="HF_Ui Verbose"`, or register the category at module load with a global initializer (PrjEfz does:
`GEfzUiMatLabLogCategoryRegistered` in `EfzUiMatLabTypes.as`) [WI-1641 measured].

## 6. List entries: recycle correctly

- `Snap` on bind (`OnListItemObjectSet` / `SetItemStack`), `Release` on `OnEntryReleased` / `ClearSlot`. Skipping the reset leaks hover/selected state onto
  reused entries for the anim, cached-MID and Time-lerp drives (measured in the P0 spike; the state-MIC drive did not reproduce it, cause not investigated).
- AngelScript cannot implement `UserObjectListEntry`: the tile-view entry WBP stays a Blueprint that forwards the two events to AS. Entry slot that CAN
  inherit `UHF_StateSlotButtonBase`: call `Snap(flags)` from the forwarded bind and `ReleaseEntry()` from the release.
- Slot that CANNOT inherit the HF base (existing project class): COMPOSE the driver. Keep `UPROPERTY(Transient) UHF_UiMaterialStateDriver Driver` on your class,
  `NewObject(this, ...)` + `Setup(Background, StateMics, duration)` in `OnInitialized`; call `Driver.Snap(...)` at the end of your bind function and
  `Driver.Release()` in your clear function. PrjEfz hook points (not yet wired in shipping code; WI-1635 owns those widgets): `Script/UI/EfzInventoryWidgets.as`
  `SetItemStack` (BP `OnListItemObjectSet` forwards to it) and `ClearSlot` (BP `OnEntryReleased` forwards to it) [source read, not run].

## 7. When to use a plain brush instead

- A single static panel or border with no state (one `Border`/RoundedBox brush is cheaper and designer-editable).
- Anything that needs a texture-backed look the SDF cannot give (artwork, 9-slice textures), or a retained/blurred effect (use RetainerBox, `ui-material-engine-facts.md` §6).
- Gradients and rounded rectangles that never change at runtime and appear once or twice.
- Rule of thumb (WI-1641): use the material when it earns its cost by (a) crisp aspect-correct rounded corners/strokes at any size and DPI, (b) a state tween
  across many instances on shared MICs, or (c) a meter with exact segment quantization. Otherwise brush. The material-vs-brush GPU cost is **pending p4-pie**.

## 8. Cost rules

- Distinct MIDs never batch in Slate; a shared MIC can [WI-1641 measured, P0 spike: 64 slots, baseline 12 batches; state MICs +4 for 4 MICs, per-slot MIDs +64].
- Rest every instance on a shared MIC; own a MID only while tweening. Do not set per-slot scalars; use the 7 MICs plus `Image.ColorAndOpacity` for static tints.
- A style tweak (stroke, radius, intensity) for a whole list: hand EVERY cell the SAME shared set of MIDs (the PrjEfz lab does this), never one per slot.
- Idle cost is 0 per frame (no Tick, no timer at rest) [WI-1641 measured]. CPU Slate numbers in the editor were noise; GPU not measured: **pending p4-pie**.

## 9. Decorate a slot in a new project (step by step)

Open the worked example first: `/HF_Common/UI/MaterialExamples/WBP_HFUIMaterialExample` (base `UHF_UIMaterialExampleWidget`), with `WBP_HFUIStateSlot`
(base `UHF_StateSlotButtonBase`) and `WBP_HFUIMeter` (base `UHF_MaterialFillBase`, `FillParamName = FillAmount`).

1. Enable HF_Common. Generate your MIC set into your project (§3) from your style tokens (linear).
2. Make a slot WBP parented to `UHF_StateSlotButtonBase` (no Blueprint variables; the AS base owns every variable). Tree: Overlay root (not a CanvasPanel), an `Image` named
   `Background` (Slot alignment Fill/Fill, so a larger size scales the material), an optional `Image` named `ItemIcon`, optional `Label`.
3. On the CDO set `StateMics` (the 7 keys) and `TransitionDuration`. Set a `CommonButtonStyle` with NoDrawType on the CDO AND on every instance (CDO Style does not
   reach instances). Bind no `Anim_*`.
4. Decorative images `HitTestInvisible`; the slot itself is the clickable (a `UCommonButtonBase`).
5. List entry: bind -> `Snap`, release -> `ReleaseEntry` (§6). Drag verdict: `SetDragState(source, eligible, invalid)`.
6. Verify with `capture_preview_state` + `img_check` (no PIE): the slot WBP accepts normal/hovered/pressed/focused/disabled/selected only, so drag/eligible/invalid
   are checked as forced slots on a panel (as the example does with `ForceState`). First capture after an editor start can be blank (shader warm-up): repeat once.
7. Reduced-motion/animation-off: `SetReducedMotion(true)`.

Meter: WBP parented to `UHF_MaterialFillBase`, `FillImage` bound to an Image with a Fill/Fill slot alignment (a Left/Top image ignores a SizeBox width), a
`MI_*Meter_*` MIC as `FillMaterial`, `FillParamName = FillAmount`; call `SetFill(0..1)`. A meter owns a MID by design (script value re-fetched mode).
Note the base's default `FillParamName` is `Fill amount` (the Epic-pack name): set it to `FillAmount` for `M_HF_UiMeter`.

## 10. Worked inventory example: the PrjEfz UIMaterialLab

PrjEfz keeps an isolated lab (never cooked: `/Game/Editor/UIMaterialLab`, `Config/DefaultGame.ini`; no shipping referencer [WI-1641 measured]): map `L_EfzUiMatLab`,
root `WBP_EfzUiMatLab` (`UEfzUiMatLabWidget`, `Script/UI/Editor/UIMaterialLab/`), stations State (candidate material slot vs plain-brush baseline, 6 states, 8x8 grid,
recycle), Card/Socket (loadout card, socket row), Meter (linear and 8-/5-segment bars at 120 and 320 px) and Inspector (gradient backdrop, header accent tween,
item well). Efz style MICs (31) in `/Game/UI/Materials/EfzUI` from `efz_ui_style_mics.py`. Measured there [WI-1641 measured, capture/editor only]: Efz idle fill
within 0 levels of the token; inner stroke 1.00 px at all measured sizes; `0.6 * 8 = 4` lit segments, `0.6 * 5 = 3`; recycle readback "MID brushes 0".
Lab traps worth copying: changing a BindWidget UPROPERTY type then recompiling the WBP with the editor open crashed the editor (restart first); `set_font` resets the
colour to transparent (always pass a colour); Overlay children default to Left/Top (set Fill/Fill). The lab is the worked inventory example; production adoption
is a one-slot pilot owned by a separate item.

## Pending p4-pie (not measured)

GPU cost (material vs brush), real DPI (1.5x; `capture_preview_state dpi_scale` is a no-op, FND-02055), the hybrid driver as one unit in a pooled tile view,
real hover/click/drag, pause behaviour, the swap-back timer in a PIE world, float32 Time coarseness after long uptime (reasoned only).
