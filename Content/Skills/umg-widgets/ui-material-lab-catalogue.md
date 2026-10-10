---
name: umg-widgets/ui-material-lab-catalogue
description: Catalogue of Epic's free UIMaterialLab pack as measured in UE 5.8 (read-only editor probes) — the 40 reusable MF_UI_* material functions with inputs/outputs and which read Time, the UV/aspect mechanism (engine function GetUserInterfaceUV = TexCoord channels 0-4, MF_UI_AspectRatio from Pixel Size, SDF_Box Keep Aspect Ratio), what the 38 page masters really are (static-switch demos, not production masters), texture import settings, dead MPC paths, and which component masters a project should build on. Read before authoring a UI material from the pack in any project.
---

# Epic UIMaterialLab pack: what is reusable, measured

Pack root `/Game/UIMaterialLab` (Epic free sample). Measured in the UE 5.8 Hazelight fork with read-only editor
Python; counts corrected from earlier briefs: **43 material functions** (40 `MF_UI_*` + `MF_SnakeMask`,
`MF_SnakeyOnMap`, `MF_GridInventoryData`), **55 masters** (17 `Widgets/ComponentMaterials` + 38 under
`Materials/<page>`), 164 instances, 10 textures, 1 MPC. The 17 component masters, their 51 instances and the
Applications-page port are reviewed in the consuming project's `DesignDocs/UI/ui-material-usage-review.md`
(SpaceWasteCollector, WI-1548); this doc covers the rest. Engine behaviour behind every row:
`ui-material-engine-facts.md`.

Invariants that hold across the whole pack:

- Every master is `MD_UI` + `BLEND_Translucent`; Final Color is connected on all 55, Opacity on 54
  (`M_UI_PingPong` has none). **No master premultiplies**: the Final Color root is a Constant / Lerp /
  StaticSwitch / Saturate and Opacity is a separate mask chain (only `M_UI_Flip` shares one `TextureSample` for
  both). Copy this shape: colour chain -> Final Color, mask chain -> Opacity, never `Multiply(colour, mask)`.
- UV space is `TextureCoordinate` index 0 everywhere a master reads UVs directly (79 nodes, all index 0, tiling 1);
  no `ScreenPosition`, `VertexColor`, `DDX/DDY` or `Custom` nodes in any master. Size and aspect come ONLY through
  the engine function `GetUserInterfaceUV` (section 2): 12 functions and 44 of 55 masters reach it by nesting.
- 52 of 55 masters reach a `Time` node (directly in 9 masters, or through `MF_UI_LinearTime` / `SineTime` / `Wave`
  / `Random`); all 16 Time nodes have `Ignore Pause` OFF and no period override. In Slate that flag is a no-op and
  UI Time never pauses (`ui-material-engine-facts.md` §5).
- The 38 page masters are **teaching demos**: their only parameters are `Example 1/2/3` static switches plus a few
  demo scalars; sizes are constants. Do not instance them in production; instance a component master (section 3)
  or author a new master from the functions.

## 1. The 40 `MF_UI_*` functions (all `Expose to Library`, category `UIMaterialLab`)

Inputs `name:TYPE`; `S` scalar, `V2/V3/V4` vectors, `B` static bool. `T` = contains a Time node. Only
`MF_UI_Alternate` carries a description; every other function documents itself through its input names alone.
When you author a function, fill `Description` and each input's description: the pack's silence is the thing to
improve on, not copy.

**SDFs** (distance negative inside; `Fill` / `Outline` are already feathered 0..1 coverage, `SDF` / `SDF Outline`
are raw distances for booleans and effects):

| Function | Inputs | Outputs |
|---|---|---|
| SDF_Box | UVs:V2, Size:V2 (container-relative, 0.5 = half), Stroke thickness:S, Glow Min:S, Glow Max:S, Corner Radiuses:V4, Keep Aspect Ratio:B (default ON), Is stroke thickness relative:B, Dimension to pick for stroke:S | SDF, SDF Outline, Fill, Outline |
| SDF_Circle | UV:V2, Size:S, Stroke thickness:S, GlowMin:S, GlowMax:S, Is stroke thickness relative:B | Fill, SDF, Outline, SDFOutline |
| SDF_Hexagon | UVs:V2, Radius:S, Outline Thickness:S, Corner roundness:S, Glow Min/Max:S, Is stroke thickness relative:B | SDF, Fill, SDF Outline, Outline |
| SDF_Triangle | UVs:V2, Radius:S, Stroke thickness:S, Corner roundness:S, Glow Min/Max:S, Is stroke thickness relative:B | SDF, Fill, Outline, SDF Outline |
| SDF_Union / SDF_Intersection / SDF_Subtraction | SDF A:S, SDF B:S, Blend Amount:S | Exact, Smooth |
| Wave (T) | SDF Input:S, Repetitions:S, Motion:S | Cosine, Frac, Raw Output, Ping pong |

**Masks:**

| Function | Inputs | Outputs |
|---|---|---|
| BlindMask | UVs:V2, Rotation:S, Progress:S, Tiling:S, Feather:S | One-sided, Symmetrical, Raw mask unclamped |
| WedgesMask | UVs:V2 | 1 / 2 / 3 subdivisions |
| CheckerMask | UVs:V2, Subdivisions:V2 | Result |
| Radial_Segments | UV:V2, Subdivisions:S, Segment Spacing:S, Feather:S | Segmented Triangles, Segmented Lines |
| RingMask | UVs:V2, Subdivisions:S, Glow Min Max:V2, Keep inside bounds:B | Result |
| Sectors | UVs:V2, Subdivisions:S, Radius:S, Thickness:S, Sharpness:S, Contrast:S, Motion:S, Is symmetrical:B | Sectors, Raw mask |
| SquareGradient | UVs:V2 | Result |
| Alternate | UVs:V2, Num Steps:S | Alternate Rows, Alternate Columns |

**Patterns:**

| Function | Inputs | Outputs |
|---|---|---|
| GridTiling | UV:V2, Tiling X:S, Tiling Y:S | PatternUVs, ScaledPatternUVs, Raw UVs |
| HexagonTiling | UVs:V2, Tiling:S, Hex Distance:S, Hex Thickness:S | Outline Border, Outline, Fill, Pattern UVs |
| Halftone | Input gradient:S, UVs:V2, Halftone pattern rotation:S, Halftone pan speed:V2, Dot density:S, Dot max/min size:S, Dot glow max/min:S, Container ratio:V2 | Dot halftone, Halftone UVs, Halftone size mask |
| Truchet | UVs:V2, Truchet tiling:V2, Line thickness:S, Feather:S, UVs flow speed:S, UV flow tiling:S, Random seed:S, Draw regions:B, Use line instead of circle:B | Truchet mask, Truchet UVs, Raw truchet SDF |

**Transforms and distortions** (UV in, UV out; chain them before an SDF):

| Function | Inputs | Outputs |
|---|---|---|
| Translate | Input UVs:V2, Translate:V2 | UVs raw, UVs repeat |
| Scale | UVs:V2, Scale:V2, Scale center:V2 | Result |
| Flip | UVs:V2 | Mirror X, Mirror Y, Mirror X and Y |
| Symmetry | UV:V2, Symmetry axis:S, Rotation:S, Scale:V2, Offset:V2, Flip other axis too:B | Result, Left Side Mask, Right Side Mask |
| Skew | UVs:V2, Skew amount:S, Skew rotation:S, Alpha threshold:S, Alpha feather:S | Skewed UVs, Alpha mask |
| Slant | UVs:V2, Slant amount:V2 | Slanted UVs |
| PolarCoordinates | Vector or UVs:V2, Swizzle Coordinate Output:B | Angle gradient, Radial Coordinates |
| WaveWarp (uses LinearTime) | UVs:V2, Amplitude X/Y:S, Frequency X/Y:S, Phase:S, Motion:S | Warped UVs, Raw Distortion |
| Pixelate | UVs:V2, NumPixels:V2 | Result |

**Time and easing:**

| Function | Inputs | Outputs |
|---|---|---|
| LinearTime (T) | Speed:S, FloatValue:S | 0-1 Loop, 0-1 Ping pong, 0-1 Switch |
| SineTime (T) | Speed:S | 0-1 sine time, Raw sine time |
| TimeDisplace | Mask:S, Value:S, Offset:S | Result |
| EaseCurves | Value (0-1):S | EaseIn / EaseOut / EaseInOut x Sine / Cubic / Quint (9 outputs) |
| PingPong | Value:V2, Subdivisions:S, Height:S | Result, Inverse Result |
| Random (T) | Float 2:V2, Seed:S, Single value:S | 2D, 1D, Single value |

**Utilities:**

| Function | Inputs | Outputs |
|---|---|---|
| AspectRatio | (none; reads GetUserInterfaceUV Pixel Size) | Ratio, Horizontal, Vertical |
| RemappableGradient | Value 1..5:V3, Pos value 1..5:S, In:S | Gradient 3 / 4 / 5 Colors |
| RemappableGradient_Float | Value 1..5:S, Pos value 1..5:S, Gradient mask:S | Gradient 3 / 4 / 5 Values |
| SteppedGradient | Gradient:S, Num steps:S | keep initial range, keep steps different |
| WeightedCorners | UVs:V2, Corners values:V4 | Corners |

Non-`MF_UI_` functions: `MF_SnakeMask` / `MF_SnakeyOnMap` (a consuming project's map effect; reads a project MPC,
unreferenced) and `MF_GridInventoryData` (reads `MPC_GridInventoryUiParamCollection`, section 4). Not Epic library.

## 2. UV and aspect mechanism (how the pack stays resolution-independent)

- `/Engine/Functions/UserInterface/GetUserInterfaceUV` (engine content, "Provides access to various built in UV
  sets for UI materials rendered with Slate or UMG") is five `TextureCoordinate` nodes wired to five outputs:

| Output (call index) | Reads | Meaning |
|---|---|---|
| 9-Slice UV (0) | TexCoord 0 | tiled UV; for a plain un-tiled `Image` identical to 0..1 |
| Pixel Size (1) | TexCoord 3 | **the element's size in device pixels** (`RoundToInt(LocalSize * DPI)`) |
| Normalized UV (2) | TexCoord 4 | 0..1 across the whole element, ignores 9-slice and tiling |
| Tiling (3) | TexCoord 2 | horizontal / vertical tiling factors |
| 9-Slice UV No Tiling (4) | TexCoord 1 | 9-slice UVs without tiling |

- `MF_UI_AspectRatio` masks `Pixel Size` into W and H: `Horizontal = (W/H, 1)`, `Vertical = (1, H/W)`,
  `Ratio = max(Vertical, Horizontal)` component-wise, so one component is 1 and the other is long/short. No size
  parameter and no script write are needed: resizing the widget changes `Pixel Size` and the ratio follows.
- `MF_UI_SDF_Box`: `Keep Aspect Ratio` (default ON) switches `Ratio` vs `1.0`; the signed offset
  `|UV*2-1| - halfExtent(Size, stroke)` is multiplied by `Ratio` BEFORE the corner radius is added, so corners
  stay circular on a non-square widget while the box still spans a fraction of each side. With the switch off the
  corners stretch with the element.
- In the page masters `Size` is a constant (e.g. `M_UI_Translate`: `(0.5, 0.5)`) and `Keep Aspect Ratio` is left
  at the default; a production master that sizes the box per instance wires its own `Size` parameter.
- Caveat: `Pixel Size` is the pre-render-transform device size; a scaled `RenderTransform` does not update it.

## 3. Component masters worth building on

From the consuming-project review (one master per family; verdicts measured):

| Family | Master | State scalar(s) | Notes |
|---|---|---|---|
| Button | `M_UI_Button` (54 expr) | `Hovered` | `M_UI_ButtonEfz` is a duplicate (reroutes only) |
| Button, wave outline | `M_UI_Button2` | `IsHovered`, `Vignette Size` | direct `GetUserInterfaceUV` caller |
| Button, gradient | `M_UI_Button3` (87) | `Hovered` | instance rests at `Hovered=1`; rest pose is the animation's frame 0 |
| Inventory slot | `M_UI_Slot` (104) | `Is Hovered`, `Is Active`, `ItemTexture` | |
| Liquid fill | `M_UI_Liquid_Fill` | `Fill amount` | `_Fill1` is a duplicate (+1 Min) |
| Objective fill | `M_UI_ObjectiveFill` | `Fill progress` | `M_UI_ContextActionOverlay` is a different effect |
| Segmented bar | `M_UI_ProgressBar_Segmented` | `ProgressValue` (switch `bSetProgressValueManually` ON) | switch off = self-animating demo |
| LED display | `M_UI_LedBackground` | none | `_1` is an orphan; `M_UI_EfzLedBackground` depends on a dead MPC |
| Profile card / tile / halftone | `M_UI_ProfileCard`, `M_UI_TileBackground`, `M_UI_Halftone` | none | |

Avoid: `M_UI_SlotFrontOverlay` (357 expressions, 60 scalars, 5 switches), `M_UI_WIP_Tile`, the Efz variants.

## 4. Dead and project-coupled paths

- `MPC_GridInventoryUiParamCollection` (4 scalars `Hover_GridInventory_*`, default -1) has NO runtime writer in
  the measured project (0 hits in Script/Plugins/Source, no Blueprint referencer). Only `MF_GridInventoryData` and
  `M_UI_EfzLedBackground` read it. Treat the "MPC-driven hovered cell" idea as unproven; a per-widget state scalar
  is the pattern (MPC semantics in Slate: `ui-material-engine-facts.md` §7).
- `MF_SnakeyOnMap` reads `/Game/Materials/MPC_VacuumLung` (project collection): the pack folder in a project is
  not clean of project edits. Diff a pack copy against a fresh Fab download before trusting it as Epic's.

## 5. Textures

| Texture | sRGB | Compression | Group | Address | Size | Note |
|---|---|---|---|---|---|---|
| T_UI_Axe / Sword / Shield / Potion | on | Default | UI | Clamp | 256 / 32 | icon art: correct |
| T_UI_Oxo | on | Default | UI | Wrap | 256 | tiling art |
| T_UI_Cards, T_UI_Numbers | on | VectorDisplacementmap (uncompressed RGBA8) | UI | Wrap | 512 / 256 | banding-free flat art; sRGB on is right for colour |
| T_UI_Noise, WIP_texture | off | Alpha | UI | Wrap | 256 | data textures: sRGB off is right |
| vacuum-icon | on | Default | **World** | Wrap | 256 | project addition in the wrong LOD group |

No texture sets `max_texture_size` or `never_stream`; mips follow the UI group. Rule confirmed by the pack: colour
art sRGB on, data masks sRGB off, icons Clamp, patterns Wrap.

## 6. Instance lineage in a consuming project (sanity probe)

Measured in SpaceWasteCollector: every `MaterialInstanceConstant` under `/Game/UI` roots either in a vendor set
(DGGUI, CurvedHUD, map) or in a pack component master; the texture-brush kit (`/Game/UI/Toadalled/Kit`) references
zero materials. Before any pack reorganisation walk `parent` to the `UMaterial` for every instance under
`/Game/UI` and print its folder: anything rooted in `/Game/UIMaterialLab/Widgets/ComponentMaterials/` breaks if the
pack moves.

## 7. Probe traps (measured while building this catalogue)

- `MaterialEditingLibrary.get_input_node_output_name_for_material_expression` with no material editor open
  CRASHES the editor (access violation). Read wiring from a text export instead: `unreal.AssetExportTask` with
  `exporter = unreal.MaterialFunctionExporterT3D()` / `MaterialExporterT3D()` into a scratch `.t3d`, then parse
  `Input=(Expression=..., OutputIndex=N)`.
- `get_material_function_expressions` lists classes and parameter names but not wiring; `imported_size` on a
  texture is not Python-readable (`blueprint_get_size_x/y` is); `bUsedWithUI` is reflection-protected and moot for
  `MD_UI`.
