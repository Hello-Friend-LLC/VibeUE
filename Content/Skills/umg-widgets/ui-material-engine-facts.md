---
name: umg-widgets/ui-material-engine-facts
description: Engine-verified UMG/Slate UI-material facts for UE 5.8 (Hazelight fork) — which rendering mechanism to pick (brush vs material vs retainer vs blur), how Slate really composites an MD_UI material (alpha convention, tint, Time clock, batching), UV channels and aspect-correct shapes (TexCoord 3 = widget pixel size), MID lifecycle, retainer/invalidation refresh rules, MPCs in Slate, texture import, font materials, measurement, validation checklist and a symptom table. Companion to ui-materials.md (patterns) and ui-material-lab-catalogue.md (the Epic pack's functions).
---

# UI materials: what the engine actually does (UE 5.8)

Every statement is tagged **[verified]** (read in the 5.8.3 Hazelight fork source or measured in the editor; the
file:line evidence is registry doc DOC-0057, `hf-wi doc show DOC-0057`) or **[rule]** (a project rule, not an
engine fact). The pattern rules (one master per family, state = one animation-keyed scalar, base class first) live
in `ui-materials.md` and `script-base-first.md` and are not repeated here.

## 1. Pick the simplest rendering mechanism first  [rule]

| Visual requirement | First choice | A UI material only when |
|---|---|---|
| Flat panel, rounded corners, outline | brush `DrawAs=RoundedBox` | gradient, shaped mask, animated or unusual outline |
| Decorative frame that stretches | texture brush `DrawAs=Box` + margins (nine-slice) | procedural variation the texture cannot give |
| Icon / illustration | texture brush + tint | recolouring regions, reveal, dissolve, SDF outline |
| Horizontal meter | `UProgressBar` with Box-brush fill | custom fill shape, segments, delayed-loss layer |
| Radial cooldown / ring | UI material (`MF_UI_RingMask`, `MF_UI_Sectors`) | always: angular reveal belongs in the shader |
| Shimmer, pulse, scrolling pattern | UI material driven by `Time` | always: pixels change with no layout work |
| Whole card fade / slide | widget animation on `RenderOpacity` / `RenderTransform` | wipe, dissolve or distortion only |
| One mask or effect over text AND images | `URetainerBox` effect material | the operation needs the composited child image |
| Blur behind a popup | `UBackgroundBlur` | custom scene distortion (Slate post buffers, experimental) |
| Text | `UCommonTextBlock` + style asset | one specific text effect (gradient, sweep) via a font material |

`Box` draws nine regions (stretched centre), `Border` leaves the centre EMPTY and tiles the sides, `Image` ignores
margins, `RoundedBox` draws a filled rectangle with optional outline and corner radius. Choosing `Border` does not
make a panel. [verified: `ESlateBrushDrawType`]

## 2. How Slate composites an MD_UI material  [verified]

- The Slate pixel shader emits `half4(Color, Opacity)` for Translucent and AlphaComposite materials; it does NOT
  premultiply. Only Additive is emitted as `Color * Opacity`.
- Blend state follows the MATERIAL blend mode, never the brush's PreMultipliedAlpha draw flag:
  Translucent = `SrcAlpha / InvSrcAlpha` (straight colour in); AlphaComposite = `One / InvSrcAlpha` (the material
  graph must output premultiplied colour); Additive = `One / One`.
- Brush `TintColor` x `ColorAndOpacity` x every ancestor `RenderOpacity` arrive as the VERTEX colour: the material
  can read it through the `VertexColor` node, and after the material runs Slate multiplies
  `FinalColor = MaterialResult * VertexColor` (RGB and A). A material therefore always honours tint and parent
  fades for its own output; it only "ignores" them if it adds colour the vertex colour cannot scale (it cannot).

Rules that follow [rule]:
- Author Translucent UI materials as **Final Color = straight colour, Opacity = coverage**; never multiply colour by
  the mask into Final Color (the whole Epic pack follows this, see the catalogue). Use AlphaComposite only when the
  source data is already premultiplied (a retainer RT, a premultiplied texture) and say so in the master's
  description.
- One owner per fade: the widget's `RenderOpacity` (vertex colour) OR a material scalar, never both (two owners
  make the fade quadratic).
- Diagnostic: place the widget over black, white, saturated magenta and a moving scene at RenderOpacity 1 / 0.5 / 0.
  Dark fringe = alpha applied twice or dark RGB under the alpha edge; bright edge = straight colour entering an
  AlphaComposite blend.

## 3. UV channels, widget size, aspect-correct shapes  [verified]

- There is NO `GetUserInterfaceUV` material EXPRESSION in 5.8. The `GetUserInterfaceUV` node the pack uses is an
  engine CONTENT material function (`/Engine/Functions/UserInterface/GetUserInterfaceUV`): five `TextureCoordinate`
  nodes wired straight to five outputs, no math. Output -> channel: `9-Slice UV` = TexCoord 0, `Pixel Size` =
  TexCoord 3, `Normalized UV` = TexCoord 4, `Tiling` = TexCoord 2, `9-Slice UV No Tiling` = TexCoord 1.
- Slate vertex texcoord channels as a UI material sees them: `TexCoord[0]` = brush UV x tiling (the normal 0..1
  UV for an `Image` with no tiling), `[1]` = UV, `[2]` = tiling, **`[3]` = widget size in device pixels**
  (`RoundToInt(LocalSize * DPI scale)`, ignores render transforms), `[4]` = 0..1 position across the quad.
  Channel 3 is the only in-shader size source; aspect ratio = `PixelSize.x / PixelSize.y`, which is exactly what
  `MF_UI_AspectRatio` computes (catalogue §2). (A compile-and-capture confirmation of channel 3 values is still
  owed: DOC-0057 OPEN.)
- `ddx` / `ddy` compile in the UI domain (pixel stage), so derivative-width antialiasing works.
- The `RoundedBox` brush's local-size shader params are brush-only; a material cannot read them.

Rules [rule]:
- A circle in normalized UV is an ellipse on a non-square widget. Correct it with `MF_UI_AspectRatio` (reads the
  size channel) or keep the brush in a fixed-ratio `SizeBox`. A script-written `WidgetSize` scalar is the fallback
  when the material cannot use channel 3; write it from `GetCachedGeometry().GetLocalSize()` only when it CHANGES
  (geometry is zero before the first layout pass: defer until non-zero), never every tick.
- Soft edges from a signed distance `d` (negative inside): `w = max(0.5*(abs(ddx(d))+abs(ddy(d))), eps)`,
  `coverage = 1 - smoothstep(-w, w, d)`. The pack's SDF functions output `SDF` raw plus pre-feathered `Fill`/`Outline`.
  Test at fractional DPI and at the smallest on-screen size in use.
- A Slate unit is not a device pixel: DPI scale changes the raster footprint of a 2 px stroke. Decide whether
  strokes scale with UI (constant in UV / Slate units) or stay crisp (derive from channel 3 or ddx).

## 4. MID lifecycle  [verified + rule]

- `UImage::GetDynamicMaterial` / `UBorder::GetDynamicMaterial` create the MID ONCE (outer = the widget), set it as
  the brush resource and invalidate layout on creation only; later calls return the same MID; they return null if
  the brush resource is not a material. `SetScalarParameterValue` pushes a render-proxy update only when the value
  changed and triggers NO widget invalidation; Slate re-submits cached elements with the live proxy, so parameter
  and Time changes show without any `Invalidate`/Volatile.
- HF rule stands [rule]: re-fetch `Image.GetDynamicMaterial()` on every write (`HF_UiMaterial::SetScalar`), never
  hold the MID in a member: `SetBrushFromMaterial` and a widget-animation material-track restore replace the brush
  resource, after which a cached pointer writes into an object nobody draws. The re-fetch is a pointer return.
- Initialization order in the base [rule]: `SetBrushFromMaterial(Preset)` in `PreConstruct` -> apply the complete
  initial state in `Construct` (including values equal to defaults) -> subscribe to events. Construction is
  reentrant (designer, rebuilds): never duplicate subscriptions.
- Recycled `ListView` / `TileView` entries [rule]: reset every state scalar, texture and transition timestamp in
  `OnListItemObjectSet`, release subscriptions in `OnEntryReleased`. A stale cooldown or selection glow on a
  recycled row is this.

## 5. Time, state and transitions

- `Time` in a Slate-drawn material compiles to `View.GameTime` (or `View.RealTime` with Ignore Pause), and Slate
  fills BOTH from the application clock, never from a `UWorld`: **UI material time never pauses, never dilates,
  and `Ignore Pause` is a no-op**. [verified] A pause menu's shimmer keeps running; a cooldown sweep that must
  freeze on pause needs a script-written progress scalar, not `Time`. [rule]
- Shader transitions `FromValue/ToValue/StartTime/Duration` need `StartTime` from the same clock the shader reads:
  `FPlatformTime::Seconds() - GStartTime` (app real time), not world time. Prefer the WBP animation
  (`ui-materials.md` §3). [rule]
- Button precedence Disabled > Pressed > Hovered/Focused > Normal, Selected orthogonal; resolve centrally in the
  base so mouse hover, gamepad focus, selection and disable do not fight over one scalar. [rule]
- Decorative material `Image`s are `HitTestInvisible`; the `UCommonButtonBase` owns input. Material opacity never
  changes the hit area. [rule]
- Reduced motion: a static selected/hovered treatment, and the base skips decorative loops. A cooldown that must
  match gameplay derives from authoritative start/end data; a decorative loop is never the gameplay clock. [rule]

## 6. Retainer boxes, invalidation, clipping  [verified]

- `URetainerBox`: `SetEffectMaterial(UMaterialInterface)`, `GetEffectMaterial()` (returns the MID),
  `SetTextureParameter(FName)` (default name `Texture`), `RequestRender()`, `SetRetainRendering(bool)`,
  `SetRenderingPhase(int Phase, int PhaseCount)`. Defaults: `RenderOnPhase` true, Phase 0 of 1,
  `RenderOnInvalidation` false; both `Render*` flags are construction-only.
- Default phase 0 of 1 means the child is repainted EVERY frame, so a Time-driven child animates. Only when
  `PhaseCount > 1` or the retainer is switched to invalidation-driven does a Time-driven child freeze until the
  child invalidates or `RequestRender` is called. The retained RT is composited as a premultiplied box
  (`PreMultipliedAlpha|NoGamma`), and the EFFECT material's own blend mode still decides the final blend: an effect
  material sampling the RT should pass the sampled alpha through and treat the colour as premultiplied.
- Invalidation caches widget geometry/paint; a Retainer caches a rendered TEXTURE (RT memory, extra pass). Use
  ordinary invalidation first; use a Retainer for a composite effect or a measured win. [rule]
- Masking a panel's background material masks only that background; sibling text/icons are untouched. A subtree
  mask is a retainer effect or `Clipping = ClipToBounds`, never a bigger background material. [rule]
- Batching key: layer, shader params, resource, primitive, shader type, draw effects/flags, scene index, clipping
  state; the material resource key is (material OBJECT, brush ImageSize, mask). **Two different MIDs never batch**,
  even from one master; a shared `MaterialInstanceConstant` does. Different clip scopes never batch.

## 7. MPCs, blur, post buffers  [verified]

- A `MaterialParameterCollection` in a UI material resolves to the collection INSTANCE of the world owning the
  enclosing `SViewport` (`FSceneViewportPaintScope`): per-PIE-client values work in the game viewport; widgets in
  editor panels and the Designer see the collection DEFAULTS. Writers use `Material::SetScalarParameterValue(
  Collection, Name, Value)` (bound in AngelScript). Use an MPC only for genuinely global UI data (theme, time of
  day); per-widget state stays a MID scalar. [rule]
- `UBackgroundBlur`: `BlurStrength` 0..100, `BlurRadius` 0..255 (needs `bOverrideAutoRadiusCalculation`),
  `bApplyAlphaToBlur`, `CornerRadius` (FVector4), `LowQualityFallbackBrush`; scalability kill switch
  `Slate.ForceBackgroundBlurLowQualityOverride` forces the fallback brush. Author the fallback.
- Slate post buffers: `Slate.CopyBackbufferToSlatePostRenderTargets` (help text still says Experimental), five RTs
  `ESlatePostRT_0..4` (only 0 enabled by default), `USlatePostBufferProcessor` (the 5.8 class; the RHI-named one
  is deprecated), `UPostBufferUpdate` widget decides what gets captured. Prototype only when `UBackgroundBlur`
  cannot do it; test packaged, resized and HDR output.

## 8. Fonts  [verified]

`FSlateFontInfo::FontMaterial`, `FFontOutlineSettings::OutlineMaterial`, `bSeparateFillAlpha`, `bMaterialIsStencil`.
A font material's `TexCoord[0]` is a per-text-run 0..1 gradient (glyph i spans `[i/N, (i+1)/N]`), and
`VertexColor` is the font tint: a left-to-right gradient across a label works; a per-glyph effect does not.
Keep native colour/shadow/outline first; a font material is for one specific effect, tested on long labels,
small sizes, the disabled state and parent fades. [rule]

## 9. Textures for UI  [rule, confirmed by the pack's settings]

| Asset | Colour | Sampling |
|---|---|---|
| Coloured UI artwork | sRGB on; `UI` LOD group; `Default` or `UserInterface2D` compression (uncompressed `VectorDisplacementmap` is the pack's trick for banding-free flat art) | alpha preserved; `Clamp` unless it tiles |
| Packed masks / distance fields / noise | sRGB OFF (numeric data); `Alpha` or `Masks` compression | sampler type matches (`Linear Color`/`Masks`) |
| Crisp small icon | `UI` group, `NoMipmaps` | judge at the real on-screen size |
| Art that is minified or used in world UI | mips on | check shimmer at distance |

Colour and data textures need different gamma treatment; one preset for both is the usual halo source. Fix fringes
with edge padding in the art, not by disabling filtering globally.

## 10. Measure CPU and GPU separately  [rule]

- Count batches (`Slate.ShowBatching`, Slate Insights `-trace=slate`); sharing a master is not sharing a drawn
  resource (§6). Never promise one draw call per screen.
- Compare identical layouts at target resolution: static brush -> shared constant instance -> separate MIDs -> the
  same under a retainer. Record idle, hover, scrolling, progress updates, screen open, resize.
- A large translucent material costs more GPU than a static brush; stacked near-full-screen translucent layers
  need a measurement before they ship (shader complexity view, `ProfileGPU`).
- Do not invent limits (max MIDs, fixed UI ms budget); measure against the project's existing screens.

## 11. Validation checklist (before declaring a material widget done)

- Material compiles for the target platforms with no warnings; domain `User Interface`; blend mode and alpha
  convention stated in the master's description.
- Parameter names, units, defaults and endpoint behaviour (0 and 1) documented on the master.
- Several instances side by side change independently; theme swap and list recycling re-apply state.
- Black / white / saturated / moving backgrounds at parent RenderOpacity 1, 0.5, 0.
- Narrow, wide, small, large; DPI 0.67 / 1.0 / 1.5 (1280x720, 1920x1080, 3440x1440).
- Hover, gamepad focus, pressed, selected, disabled, loading, reduced motion.
- Pause (UI Time keeps running, §5), long session, resize, cooked standalone.
- Retainer refresh rules tested separately if used.
- Evidence captured with VibeUE `capture_image source=game` (Slate composited), never a scene-only capture.

## 12. Symptom table

| Symptom | First thing to check |
|---|---|
| Changing one meter changes every meter | a mutable MID is shared; one brush instance per widget |
| Parameter writes log fine, visual unchanged | cached MID after a brush swap (§4); parameter name/case; the widget sits in a phased or invalidation-driven retainer (§6) |
| Glow still visible at RenderOpacity 0 | the material adds colour outside its own coverage, or a second fade owner (§2) |
| Dark or bright fringes | alpha convention (§2), double premultiply, dark RGB under transparent texels, filtering |
| Round icon is an ellipse | no aspect correction (§3) |
| Stroke thickness changes with aspect | size taken from normalized UV; use channel 3 or a SizeBox (§3) |
| Shimmer keeps running on the pause menu | by design: UI Time is the app clock (§5); drive it from script if it must stop |
| Shine freezes only under a Retainer | phased / invalidation-driven retainer (§6) |
| Hover feels delayed | the interactive accent is inside a slowly phased retainer |
| Label escapes the rounded panel | background mask masks only the background (§6) |
| Recycled row shows old state | state not reset on item set (§4) |
| MPC-driven UI shows defaults in the Designer / editor panel | expected: MPC resolves per viewport world (§7) |
| Colours differ designer vs PIE | tint multiplication, sRGB on a data texture, RT convention, capture gamma |
| UI CPU down, GPU up | more pixel work: overdraw, blur, retained rendering |
