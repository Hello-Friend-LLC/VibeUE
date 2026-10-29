---
name: umg-widgets/ui-material-examples-catalogue
description: Catalogue of five third-party UMG/UI-material example repos (ProceduralDrawingMaterialSamples, RTMSDF, TransitionFX_Dev, EffectBox, UMG3DTransform) as exercised on the UE 5.8.3 Hazelight fork (2026-10-28, WI-1641) — pins, MIT licences, where the clones live, fork build/open status, the reusable techniques with node-level recipes, traps (UV-stretch, opaque masters, ViewSize aspect, EffectBox renders nothing), and a copy-vs-reauthor verdict per technique. Read before borrowing any node graph from these repos.
---

# UI material example repos: what is reusable, measured on the fork

Surveyed 2026-10-28 (WI-1641) on UE 5.8.3 (engine HEAD bc09bb4e, Substrate on, Hazelight fork). Labels:
**RAN** = executed/captured on the fork (editor Python + `capture_preview_state` on a gray-bg scratch WBP + `img_check`;
one PIE run for EffectBox). **UPSTREAM** = README or source reading only, not exercised. Gaps are listed at the end
and stay gaps. Complements `ui-material-lab-catalogue.md` (Epic pack) and `ui-material-engine-facts.md`.

## 0. Where they live and fork status

Scratch clones under `E:/git/_external/uimat/<Repo>` (never in Perforce or HF repos; per-sample
`Content/ScratchSurvey` is scratch). Built with `rebuild-all.ps1 -SkipEngine` (never `-Force`); no engine module was
compiled, so no BuildId change. Opened with `UnrealEditor.exe <uproject> -log`.

| Repo | Pin | Licence | Layout | Fork build | Opens on fork |
|---|---|---|---|---|---|
| ProceduralDrawingMaterialSamples (PDMS) | ad2df765 | MIT | content-only project | none needed | yes (first run 290 s shader/DDC); only benign GameFeatureData asset-manager-rule errors |
| RTMSDF | 5429b5b1 | MIT (+ ChlumskyMSDFGen LICENSE.txt) | plugin (Runtime + 2 Editor modules) | Succeeded in blank host `UIMatPluginHost` | yes, no errors |
| TransitionFX_Dev | c2a245a8 | MIT | C++ project + plugins TransitionFX, DevMaterialTools (Git LFS: `git lfs pull`) | Succeeded (17 actions, 0 engine) | yes (first run ~620 s); uplugin says 5.5, log warns version string unparsable, harmless |
| EffectBox | bb056d5a | MIT | plugin (Runtime) | Succeeded in host | module loads; **renders nothing** (section 5) |
| UMG3DTransform | 3b036ba1 | MIT | content-only project (originally 5.1) | none needed | yes (30 s), asset upgrade needs no prompt |

Copying a node graph into a project: keep `Copyright (c) <year> <holder>; MIT License` from the source repo's
LICENSE in the copied asset's description or the nearest README. Sample editors need VibeUE enabled in the scratch
`.uproject` for `WidgetService`; `epic_capture_image` needs `bAllowDestructiveScope=True` in the sample's
`Config/DefaultUnrealMCPServer.ini`. One editor per machine.

## 1. Cross-cutting measurements (RAN)

- UI-domain (`MD_UI`) share of each repo: PDMS 11 of 174 materials, TransitionFX 14 of 44 (all `M_Widget_*`),
  RTMSDF + EffectBox 29 of 31, UMG3DTransform 2 of 2. Folder and name are no guide (PDMS `M_Warp_Effect_UI` lives
  under `SF/`): filter by `material_domain == MD_UI`.
- All 56 UI-domain masters loaded with 0 `LogMaterial` warnings or compile failures on the fork (Substrate on);
  35 were rendered, the rest load-only.
- First capture of a translucent UI master over black can come back uniform black (cold shaders, FND-01359 class);
  re-run over gray.
- `WidgetPreviewStateOptions.dpi_scale` 1.0 vs 1.5 produced byte-identical captures (FND-02055): the option is a
  no-op at a fixed `desired_size`, so 1.5x DPI behaviour of these samples is NOT measured (gap).
- No sample uses shared state MICs. PDMS and RTMSDF are Time or instance-parameter driven; TransitionFX writes one
  cached MID every tick (drive-verdict case B, +64 batches over 64 widgets). Adopt their naming (`Progress`,
  `Invert`) but drive per `ui-materials.md` section 3-4 (state MICs + Time lerp).

## 2. ProceduralDrawingMaterialSamples (PDMS)

- 11 UI masters (6 Opaque, 5 Translucent), 91 material functions. Drive: none; the generated loading WBP animates
  purely on material `Time` (RAN, source + hierarchy).
- Box SDF: `MF_DrawRoundedRectangle` is analytic with AA by `DDX/DDY` on `TextureCoordinate`. `MF_SDBox` /
  `MF_SdRoundBox2D` return the raw distance `d = length(max(abs(P)-B+R,0)) + min(max(d.x,d.y),0) - R`; the caller
  does AA (`M_LoadingBoxes`: DDX/DDY + SmoothStep, params `BoxSize/BoxGap/BoxRound/FadeSpeed`).
- `M_UI_Battery1` uses hard `Step` (no AA; `WidthLoad/HalfHeight/StepOffset`, switch `DrainFromLeft`).
- `MF_DrawBoxLine` stroke is Step-based (hard). `MF_SmootherStep` gradient. `MF_Seg4Cov`: four capsule/hex segment
  coverages per call, `1-smoothstep(0,AA,d)`, 53 nodes. Radar masters take data as `spoke1..6` scalars (26 Custom
  nodes, 234 expressions, 295 PS instructions: heavy).
- Traps (RAN): every PDMS shape reads `TextureCoordinate` 0-1 UV, none read `GetUserInterfaceUV` Pixel Size, so all
  shapes stretch at 256x64. Opaque masters draw an opaque black quad inside a UI brush. `M_Loading_Icon_UI` shows a
  dark fringe (row min luminance 179 < bg 188) = colour multiplied by mask, the `Multiply(colour,mask)` trap.
- Verdict: copy `MF_Seg4Cov` (meters) with the MIT header; re-author box/rounded-box/gradient/segment mask
  aspect-correct; do not copy the opaque or TexCoord masters.

## 3. RTMSDF (plugin, 92 material functions, 29 UI masters)

- Analytic shapes: `MF_RTMSDF_Shape_RectangleRounded` (per-corner radii, If-based), `Shape_Pie/Capsule/Line/...`.
- AA: `MF_RTMSDF_DefaultFWidth` (DDX/DDY, Euclidean + Manhattan, 18-32 nodes) feeds `EdgeMask_Absolute` (pixel
  width) or `EdgeMask_Relative` (smoothstep); `Stroke_Absolute`, `Glow_Outer`, `Union/Difference_Soft` compose.
- RAN: Outline, Glow, Shadow, MSDF-corner, Stroked, IrregularPolygon masters render. Texture SDFs UV-stretch at
  256x64, but an outline built on the `Absolute` mask keeps ~constant pixel width. Params: `OutlineWidth`,
  `PixelWidth`, `Zoom`, `GlowWidth`, `UVRange`.
- UPSTREAM: `MF_RTMSDF_WidgetScaler(_SDF)` expands the quad beyond layout via `WorldPosition` (the capture clips to
  the slot, so drawing outside the layout is unverified); drive is MIC/MID params.
- Verdict: copy `DefaultFWidth` + `EdgeMask_Absolute` + `Stroke_Absolute` (best pixel-width AA/stroke; holder
  Richard Meredith AB 2022-2025).

## 4. TransitionFX_Dev, UMG3DTransform

**TransitionFX.** 14 widget masters `M_Widget_*` share one convention: `Progress`, `Invert` (`MF_ApplyInvert` =
`|Invert - mask|`), `Smoothness`, `FadeColor`, `UVScale/Offset`. Aspect via `MF_CorrectAspectRatio` = TextureCoordinate
+ `ViewProperty` VIEW size (screen, not slot size). RAN: Iris, Diamond, Linear, Radial, SlidingDoor, Fade at
Progress 0.3/0.6 render aspect-correct at 128x128 and 256x64, but the preview viewport equals the capture size, so
this does not prove a slot smaller than the viewport (trap, UPSTREAM-risk). Widget path (source read,
`WidgetTransitionEffect.cpp`): one SImage over the viewport, brush = one cached MID, `SetScalarParameterValue(Progress)`
per tick, `HitTestInvisible`; PostProcess path = MID blendable (source read).
Verdict: copy `M_Widget_LinearWipe/RadialWipe/Iris` + `MF_ApplyInvert` as transition recipes, replacing
`MF_CorrectAspectRatio` with `GetUserInterfaceUV` Pixel Size (Kurorekishi/EmbarrassingMoment 2026).

**UMG3DTransform.** `M_UI_3DTransform` (Masked; `xAngle/yAngle/zAngle/perspective`, `lightVector`, `Texture`; one
Custom node with rotation matrices; reads `GetUserInterfaceUV` + `WorldPosition`) and `M_WidgetBG` (two-colour lerp).
RAN: renders on the fork at 256x256 and 256x128; angles are RADIANS (40/50 = garbage, 0.6 = clean tilt with
lighting). Drive = MID params from WBP sliders (UPSTREAM). Hit-test mismatch (Slate geometry stays flat) is
UPSTREAM/logic, not measured (needs PIE). Verdict: copy the Custom-node tilt for card/inspector tilt (Alex Engelmann
2022).

## 5. EffectBox: broken on this fork (FND-02054)

RAN in PIE: an `EffectBox` with a red-square child rendered NOTHING (0 red px) with Alpha-Composite and Translucent
masters, a passthrough master, `bAllowFastUpdate` true and false; the same subtree under UMG `RetainerBox` with the
same master rendered 11449 red px (control). Design-time also draws no content. Cause unresolved; hypothesis only:
`SEffectBox::OnPaint` skips `VirtualWindow->SlatePrepass` for engine >= 5.4 (`#if ENGINE_MINOR_VERSION < 4`).
README requires UI domain + Alpha Composite (premultiplied); its invalidation claim (RetainerBox disables FastPath)
is UPSTREAM. Verdict: do not copy, use `RetainerBox` (`ui-material-engine-facts.md` section 6).

## 6. Copy vs re-author summary

| Copy (small, working, MIT header) | Re-author (tiny, aspect-correct, our param names) | Do not copy |
|---|---|---|
| RTMSDF `DefaultFWidth` + `EdgeMask_Absolute` + `Stroke_Absolute`; TransitionFX LinearWipe/RadialWipe/Iris + `MF_ApplyInvert` (swap aspect fn); UMG3DTransform tilt; PDMS `MF_Seg4Cov` | box / rounded-box SDF + edge coverage (standard IQ box via Pixel Size), linear gradient, segment mask (Battery1 bars are hard-edged) | PDMS opaque/TexCoord masters, `MF_CorrectAspectRatio`, EffectBox |

## 7. Gaps (not measured)

G1 1.5x DPI behaviour (`dpi_scale` no-op, FND-02055). G2 Epic UIMaterialLab pack not reopened for the survey (state
mechanisms come from the prior WI-1548 measurement). G3 upstream-only: UMG3DTransform hit-test mismatch, TransitionFX
PostProcess path, RTMSDF WidgetScaler outside layout, EffectBox invalidation claim. G4 TransitionFX aspect in a slot
smaller than the viewport. G5 EffectBox failure cause. G6 21 of 56 UI masters load-only. GPU cost of any master not
measured.

Using the HF_Common material library in a project (functions, masters, driver, recipe): `hf-ui-material-library.md`. The HF_Common functions here are the reauthored/copied result of this survey (RTMSDF edge/stroke copies, notice in HF_Common NOTICES.md).
