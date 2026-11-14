# UI design: draw lists, the GPU side and output

This file is part of the UI design family; the entry document is `Zenith2/Docs/Design/UI.md`. It specifies how a laid-out context becomes a draw list, the 64-byte quad record, what the UI extractor writes into the render world, the UI renderer inside ZenithFlux, the screen-space composite pass `UIComposite`, the world-space pass `UIWorldSpace`, the shader programs, atlas pages as GPU textures, colour and HDR output, and the headless and device-loss paths. The per-pass table of `UIComposite` is Rendering's (`Zenith2/Docs/Rendering/PerPass/Common.md → "UIComposite"`); this file implements it and keeps its signature.

## Scope of this file

| Topic | Section |
|---|---|
| element tree → quads and batches | "Draw list construction" |
| the GPU record | "Flux_UIQuad layout" |
| the render-world hand-off | "Render-world records" |
| ZenithFlux side | "Flux UI renderer", "UIComposite implementation", "UIWorldSpace pass", "Shader programs", "Atlas pages on the GPU" |
| colour | "Colour and HDR output" |
| no renderer, device loss | "Headless and device loss" |

## Draw list construction

`Zenith_UIContext::BuildDrawList(xOut, xArena)` runs inside the extractor after `Update`; it reads the element table, the computed styles, the text layouts and the glyph placements and writes quads and batches into the caller's arena. It allocates nothing else. It runs identically with and without a renderer (§ "Headless and device loss"); without one, every glyph placement is EMPTY for drawing and glyph quads are not emitted.

### Paint order

1. Screens: screen-space screens in ascending (layer, open order); then each world-space panel's screen, as its own batch range.
2. Within a screen, a depth-first walk of elements in document order; the children of one parent are visited in ascending `zIndex` (style property 30, `Zenith2/Docs/Design/UI/Documents.md → "Style property catalogue"`), ties by document order (a stable sort of at most 4,096 children per parent, by the parser limits). Each element is painted as: shadow, background, border, content (image, glyphs, the type's parts: slider track and thumb, toggle box and check mark, scrollbar), children, then the focus ring when the element has `focusVisible` (the ring paints above the children, so a focused card's ring is never covered by its contents). Hit testing walks the same order in reverse (Interaction.md "Hit testing"), so `zIndex` reorders paint and hit testing among siblings and nothing else.
3. An element with `opacity` < 1 multiplies its alpha into every quad of its subtree (no offscreen group: overlapping children of a translucent element blend individually, a stated simplification the theme validator does not flag).
4. An element emits nothing when its element member `visible` is false or the element is inert (flag bit 9 of the element table, Layout.md "Element tree model"), when its computed opacity is 0, or when its clipped rectangle is empty.

<!-- elaboration: sibling-only zIndex -->
`zIndex` orders siblings only; it never lifts an element out of its parent's clip or above another screen, so a screen's paint order stays a property of its own tree and two screens never interleave.

### Quad generation per element

| Part | Quads | Mode |
|---|---|---|
| shadow (when `shadowColour` has alpha > 0) | 1: the border box translated by `shadowOffset` and inflated by 3 × `shadowBlur`, softness `shadowBlur` | `FLUX_UI_QUAD_SHADOW` |
| background colour, uniform radius | 1 | `SOLID`, with the border flag and colour when `borderWidth` > 0 |
| background gradient | 1 | `GRADIENT` |
| non-uniform corner radii | 4 corner quads, each covering a quarter of the box with that corner's radius, plus the shared middle as part of each | `SOLID` per quarter |
| image or sprite | 1; nine-slice sprites 9; an element with `mirrorInRtl` (property 84) in a right-to-left layout context swaps u0 and u1 | `IMAGE`, UVs from the sprite's rectangle in its atlas page |
| text | 1 per visible glyph, from the text layout's glyph records after truncation; selection highlights 1 per selected run per line, before the glyphs; the caret 1 when visible (blink 530 ms on, 530 ms off; steady under reduced motion) | `GLYPH`; `SOLID` |
| colour emoji glyph | 1 | `IMAGE` from a colour page |
| focus ring | 1 | `FOCUS_RING`, width `focusRingWidth`, offset `focusRingOffset` outside the border box |

- Transforms: the element's computed `translate`, `scale` and `rotate` (about `transformOrigin`) compose with its ancestors' and become each quad's origin and axes; an untransformed quad has axis vectors (width, 0) and (0, height).
- Pixel snapping: untransformed quads have their edges snapped to whole pixels in the extractor (Layout.md "Rounding and pixel snapping"); glyph origins snap horizontally only when the text is not animating, so animated text moves smoothly.
- A glyph whose placement is pending (Text.md "Dynamic glyph state machine") is skipped this frame; the paragraph's line metrics do not change, so the text fills in without reflowing.

### Clipping

- Every element whose `clip` is true (style property 77; true by default on `scroll` and `list`) pushes a clip rectangle: its padding box intersected with the parent clip.
- Rectangular clips become the batch scissor (axis-aligned, in pixels, after pixel snapping). Under a rotated or scaled ancestor the clip is not axis-aligned: the clip's four planes are not representable by a scissor, so a rotated clip uses the axis-aligned bounding rectangle of the rotated clip as the scissor and the quads outside it are culled on the CPU when wholly outside; partial overlap draws to the bounding rectangle. The validator warns on a clipping element under a rotation (Documents.md "Validation rules", rule 18).
- A quad wholly outside its clip is not emitted. Rounded clip corners are not supported: `clip: true` on an element with a corner radius clips to its rectangle, and the validator warns (rule 19).

### Batching

A new batch starts when the scissor, the texture (atlas page or image), or the SDF permutation changes; consecutive quads otherwise append to the current batch. Within one screen the paint order is never reordered to merge batches, because UI relies on painter's ordering. Expected counts are tens of batches for a menu screen: the theme guideline groups an element's sprites on one page. `ui.draw.batches` and `ui.draw.quads` are counted per frame. Cost of the whole construction: O(visible elements + visible glyphs).

## Flux_UIQuad layout

The 64-byte record shared by C++ (`Flux_UIQuad`, `Zenith2/Docs/Design/UI/ServicesAPI.md → "Flux_UI header"`) and Slang (`Flux_UIQuadGPU` in `Zenith2/Shaders/Common/UIShapes.slang`, scalar layout):

| Offset | Size | Field | Encoding |
|---|---|---|---|
| 0 | 8 | origin | 2 × f32 pixels |
| 8 | 8 | axis X | 2 × f32 pixels per local unit |
| 16 | 8 | axis Y | 2 × f32 |
| 24 | 8 | UV rectangle | 4 × UNORM16 (u0, v0, u1, v1) |
| 32 | 4 | colour | sRGB8 RGBA, straight alpha, R in bits 0–7 |
| 36 | 4 | colour 2 | gradient end or border colour |
| 40 | 4 | mode and flags | bits 0–7 `Flux_UIQuadMode`; bit 8 gradient along y; bit 9 border; bit 10 outline; bits 11–31 zero |
| 44 | 2 | corner radius | f16 pixels |
| 46 | 2 | border width | f16 pixels |
| 48 | 2 | softness | f16 pixels |
| 50 | 2 | screen pixel range | f16 (glyphs) |
| 52 | 4 | local size | 2 × f16 pixels |
| 56 | 2 | outline width | f16 distance units |
| 58 | 2 | reserved | 0 |
| 60 | 4 | outline colour | sRGB8 RGBA |

The Slang struct is generated from the same field list by the shared-struct mechanism (`Zenith2/Docs/Rendering/ShaderSystem.md`) and `Test_UIQuad_LayoutMatchesShader` compares the reflected offsets with `offsetof`. The f16 fields are written by the UI extractor with ZenithVertexFormat's `Zenith_F32ToF16` (`Zenith2/Docs/Design/GEOM/VertexFormat.md → "Zenith_VertexFormat header"`; the integer-only codec of `Zenith2/Docs/Rendering/VertexData.md → "Half-float codec"`), which is why `ZenithUI` declares `ZenithVertexFormat` in its `DEPENDS` (`Zenith2/Docs/Design/UI.md → "Libraries UI code depends on"`); a size above 65,504 pixels clamps to 65,504 before the conversion (no UI element is that large), and `Test_UIQuad_HalfFieldsUseVertexCodec` checks the clamp and three round trips.

## Render-world records

The extractor `UI.Extract@W<tag>` (Before `Render.Seal@W0`, mandatory, [Z2-N-REND-008] ordering by world tag) writes one `Flux_RenderUIList` per world per frame through `Flux_RenderWorldWriter::AddUIList`, after `BuildDrawList`:

| Field | Value |
|---|---|
| `m_axQuads` | every quad of the world's context, screen-space first |
| `m_axScreenBatches` | the screen-space batches in paint order, quad indices relative to `m_axQuads` |
| `m_axWorldBatches`, `m_axWorldPanelMatrices`, `m_auWorldPanelFirstBatch` | world-space panels in ascending screen open order |
| `m_uSurfaceWidth`, `m_uSurfaceHeight` | the context surface's pixel size this frame |
| `m_uOutputViewKindMask` | `1 << VIEW_KIND_MAIN` for a game world; `1 << VIEW_KIND_EDITOR` for the editor's game view of a world it shows (EDIT pending, row UI15) |
| `m_uWorldTag` | the world's tag, which UIComposite matches against the view's source world (row UI8 (d)) |
| `m_fWorldLuminanceNits` | `ui.world.luminanceNits` (default 100) |

A world with no visible screen writes nothing. A full-state extraction ([Z2-N-REND-015]) needs nothing extra: a UI list is whole-frame state, never a delta. The UI list is a presentation-only record; its contents never feed simulation. When `AddUIList` returns `FLUX_RENDER_WORLD_ERROR_SLOT_FULL`, the world draws no UI that frame and `ui.draw.slot_full` counts it (`Test_UIExtract_SlotFullDrawsNothing`).

## Flux UI renderer

<!-- elaboration: the two-step frame data path -->
`Flux_UIRenderer` (ZenithFlux, UI-owned files under `Engine/ZenithFlux/Internal/UI/`) follows the build node's upload order exactly (`Zenith2/Docs/Design/RHI/Memory.md → "Build-node upload order"`, `Zenith2/Docs/Rendering/GpuResources.md → "Upload ring"`): nothing is written into the frame's ring memory before the frame's `ExtendReservation`.

1. `Zenith_Result<Flux_UIRenderer*, Flux_RendererError> Flux_Renderer::EnableUI(const Flux_UIRendererDesc& xDesc)` is RPROD's declaration (`Zenith2/Docs/Design/RPROD/Renderer.md → "Flux_Renderer header"`, ticket Z2T-RPROD-014); UI implements its body in RPROD's renderer source under RPROD's "add your own Create step" pattern (UI's renderer ticket is blocked by Z2T-RPROD-014 and Z2T-RPROD-045), creating the `Flux_UIRenderer` from the UI-owned `Flux_UIRendererDesc` (`Zenith2/Docs/Design/UI/ServicesAPI.md → "Flux_UI header"`) at module start step 6, at most once (`FLUX_RENDERER_ERROR_ALREADY_ENABLED` on a second call). `Flux.BuildGraph` step 7 sets `Flux_FrameShared::m_pxUIFrame` (`Zenith2/Docs/Rendering.md → "Shared producers"`, [Z2-N-REND-995]; Z2T-RPROD-046) to `GetFrame()` before any feature function runs, and UI's renderer ticket writes that assignment in RPROD's `Zenith2/Engine/ZenithFlux/Internal/Renderer/Flux_Renderer.cpp` together with the `EnableUI` body, because `GetFrame` is UI's (`Zenith2/Docs/Design/RPROD/Renderer.md → "Flux_Renderer header"`: "the UI half of that assignment lands with UI's `EnableUI` body"); until UI is enabled, and on a build whose game uses no UI module, it is null and `UIComposite` and `UIWorldSpace` declare nothing.
2. `PrepareFrame(xWorld, xRing)` runs at step 4 of `Zenith2/Docs/Design/RPROD/Renderer.md → "Flux.BuildGraph"` (render-world consumption, after GEOM's load finalisation of step 3 and before the view list of step 5; row UI7 (b)). RPROD's step 4 is order 1 of "Build-node upload order" (render-world consumption), not order 2 (graph setup, where `DeclareFrameData`'s own comment places producers); declaring there is sound because `DeclareFrameData` "records a per-frame data need; allocates and writes nothing", so a declaration at order 1 enters `GetDeclaredFrameDataBytes()` before compile reads it at order 3 exactly as an order-2 declaration does, and the invariant the deferral depends on (no allocation or ring write before `ExtendReservation`) is untouched: for each section in ascending world tag, for each UI list, it accumulates the list's quad count and records the list's quad base (the running sum); lists that would take the total above `m_uMaxQuadsPerFrame` are dropped whole from the highest world tag down and counted `flux.ui.quads_dropped`; it then calls `xRing.DeclareFrameData(Σquads × 64, 64, "UIComposite")` and keeps the returned `Flux_FrameDataRange` in the frame's `Flux_UIFrame`. With Σquads = 0 it declares nothing and the frame's `Flux_UIFrame` has no lists. No ring byte is written here.
3. `ImportQuadChunk(xGraph)` is called by `Flux_AddUIComposite` and `Flux_AddUIWorldSpace` when they declare their pass (steps 8 and 9): it returns `xRing.ImportFrameChunk(xGraph)`, the read-only import of the frame reservation's chunk, which is the same handle on every call of a frame, so both passes read one import and no barrier depends on which chunk the reservation binds.
4. `WriteFrame(xRing)` runs at step 11 of "Flux.BuildGraph" (order 5 of "Build-node upload order"), called by RPROD after `ExtendReservation` returned `FITS` or `EXTENDED`, beside LIGHT's `WriteFrameData` (row UI7 (b)): `xRing.GetFrameData(range)` returns the `Flux_UploadAllocation`; the renderer copies each kept list's quads to `m_pCPU + listBase × 64` and stores `m_ulOffset / 64` as the frame's `m_uQuadBaseIndex`. A deferred or failed extension never calls it, so such a frame writes no UI byte (`Zenith2/Docs/Design/RHI/Memory.md → "Frame deferral"`: UI lists are per-frame state, replaced by the next frame's). `GetFrame()` returns the frame's `Flux_UIFrame` for the passes; the record jobs read `m_uQuadBaseIndex`, which step 11 wrote before any record job starts.
5. Page bookkeeping: `CreatePage`, `ReleasePage`, `IsPageResident` and `GetPageBytes` forward to `Flux_PersistentResources` (§ "Atlas pages on the GPU").

<!-- assumption: pending RPROD (UI7 (b), UI8 (d)) -->
RPROD's `Flux.BuildGraph` calls `Flux_UIRenderer::PrepareFrame(xWorld, xRing)` in its step 4 when the UI renderer is enabled and `Flux_UIRenderer::WriteFrame(xRing)` in its step 11 after a `FITS` or `EXTENDED` extension (the shape of LIGHT's `WriteFrameData` call), and `Flux_ViewContext` exposes the view's source world tag (`uint16 m_uSourceWorldTag`, the world whose render-world section requested the view), which UIComposite and UIWorldSpace match against `Flux_RenderUIList::m_uWorldTag`.
<!-- /assumption -->

At the defaults (`m_uMaxQuadsPerFrame` = 262,144) the UI's frame data is at most 16 MiB of the frame's reservation, measured against `Z2-B-RHI-006` (upload ring hard cap; `Zenith2/Docs/Design/UI.md → "Rows measured against"`). `Test_UIRenderer_WritesAfterReservation` runs a frame through the real `Flux_UploadRing` on the software device and asserts that the quads land at the allocation's offset only after `ExtendReservation`, and that a deferred frame leaves the ring range unchanged.

## UIComposite implementation

`Flux_AddUIComposite(xGraph, xView, xOutput)` is called by the path's composition after `TonemapAndGrade` and before `FLUX_EXTENSION_POINT_OVERLAY`, which RPROD places after UIComposite:

1. Return without declaring a pass when `m_pxUIFrame` is null or holds no list whose `m_uOutputViewKindMask` contains the view's kind and whose `m_uWorldTag` equals the view's source world (row UI8 (d)).
2. Declare one raster pass `UIComposite` on queue G, render target `xOutput` (`LOAD`, `STORE`), no depth; the pass reads the frame chunk returned by `ImportQuadChunk(xGraph)` as an SRV.
3. Projection: an orthographic matrix mapping surface pixels (x right, y down, origin top-left) to clip space for the view's output extent; when the surface size differs from the output extent (the editor game view at a different size), the context was laid out at the output extent, so they match by construction (the editor sets the context surface to its game view size).
4. For each matching list, for each screen batch: bind `UIDraw` with permutation `bPERM_SDF_TEXT = m_bSDFText`; set `Flux_UIDrawParams` {`m_xVertices` = the chunk SRV, `m_xAtlas` = the batch texture (or the renderer's 1×1 white texture for untextured batches), `m_xProjection`, `m_uBatchFirstVertex` = `m_uQuadBaseIndex` + list quad base + batch first quad (a quad index: the program pulls six vertices per quad record), `m_fSDFSharpness`, `m_uOutputMode` and `m_fNitsForOne` (row UI8 (b))}; set the scissor (the batch scissor, or the full extent); `Draw(6 × quadCount, 1, 0, 0)`.
5. Blending: premultiplied alpha (`ONE`, `ONE_MINUS_SRC_ALPHA` for colour and alpha); the pixel shader outputs premultiplied colour (§ "Colour and HDR output" says in which space).

Cost: one draw per batch; vertex work 6 per quad; pixel work bounded by covered area (`Z2-B-UI-003`).

## UIWorldSpace pass

The UI module registers one `Flux_ViewExtension` at module start (`GetPoint` = `FLUX_EXTENSION_POINT_TRANSPARENT`, view kinds MAIN and EDITOR; `Zenith2/Docs/Design/RPROD/Views.md → "View extensions"`), whose `AddPasses` calls `Flux_AddUIWorldSpace(xGraph, xView, xTargets)`:

1. Nothing is declared when `m_pxUIFrame` is null or the view's world list has no world batches.
2. One raster pass `UIWorldSpace`, queue G, target the view's HDR scene colour (`LOAD`, `STORE`) with the scene depth attached read-only and no depth write. The depth comparison is the view's: `Flux_DepthModeOps::GetOpaqueCompare(xView`'s `Flux_DepthMode)` (`Zenith2/Docs/Design/RPROD/Views.md → "Flux_DepthMode header"`), which is `LESS_OR_EQUAL` on the default standard-Z D24S8 views and `GREATER_OR_EQUAL` on a reversed-Z view (Z2-D-061), so world panels are occluded by opaque geometry on either kind of view but not by each other's depth. The comparison is part of the pass's raster state, so the two depth modes are two pipeline states of one program, never a shader permutation.
3. Panels are drawn back to front by the view-space distance of their centre (computed on the CPU per view); within a panel, batches in paint order.
4. Program `UIWorldSpace.slang`, identical to `UIDraw` except: the vertex stage transforms canvas pixels by the panel's canvas-to-world matrix and the view's view-projection with TAA jitter (`Flux_ViewContext`); the pixel stage outputs scene-referred linear radiance `linear(colour) × m_fWorldLuminanceNits × fPreExposure` (`Zenith2/Docs/Design/LIGHT/Lighting.md → "Light units"`), premultiplied, as an unlit emissive surface that receives no light and casts no shadow; MTSDF glyphs use the screen-space derivative of the UVs for the pixel range, so text stays sharp at any distance until it is smaller than the minimum legible size (`ui.world.minGlyphPixels`, 6), below which glyphs fade out over 2 pixels to avoid shimmering.
5. Panels write no motion vectors, as transparent surfaces at this extension point write none; TAA's neighbourhood clamp bounds the ghosting of a moving panel, and `Test_UIWorldSpace_MovingPanelGhostingBounded` measures it on the software device.

The per-pass table for `UIWorldSpace` (program, fields, targets, the depth comparison of the view's `Flux_DepthMode`, budget row `Z2-B-UI-003`) is `Zenith2/Docs/Rendering/PerPass/Common.md → "UIWorldSpace"` (`Zenith2/Docs/Design/UI.md → "Cross-area reconciliation"`, row UI8 (c); cross-doc item X236 (4)).

## Shader programs

| File | Entry points | Permutations |
|---|---|---|
| `Zenith2/Shaders/Passes/UI/UIDraw.slang` | `UIDrawVS`, `UIDrawPS` | `bPERM_SDF_TEXT` |
| `Zenith2/Shaders/Passes/UI/UIWorldSpace.slang` | `UIWorldSpaceVS`, `UIWorldSpacePS` | `bPERM_SDF_TEXT` |
| `Zenith2/Shaders/Common/UIShapes.slang` | `Flux_UIQuadGPU`, `Flux_UIEvalShape`, `Flux_UIEvalGlyph`, `Flux_UIDecodeColour` | — |

Vertex stage: `quad = m_xVertices.Load<Flux_UIQuadGPU>((m_uBatchFirstVertex + SV_VertexID / 6) × 64)`; corner `c` of (0,0), (1,0), (0,1), (1,0), (1,1), (0,1) by `SV_VertexID % 6`; position `origin + c.x × axisX + c.y × axisY`; shadows inflate the local rectangle by 3 × softness; local coordinates in pixels and UVs interpolate.

Pixel stage per mode:

| Mode | Coverage and colour |
|---|---|
| SOLID | signed distance to the rounded rectangle of local size and corner radius; coverage `saturate(0.5 − d)` (one-pixel antialiasing in local pixels scaled by the axis lengths); with a border, the border colour where `d > −borderWidth` |
| GRADIENT | as SOLID, colour `lerp(colour, colour2, t)` in linear space, t along local x (or y) |
| IMAGE | texture sample × tint, multiplied by the SOLID coverage of the rounded rectangle |
| GLYPH (`bPERM_SDF_TEXT`) | `sd = median(r, g, b)`; `screenPxDistance = screenPxRange × (sd − 0.5) × m_fSDFSharpness`; coverage `saturate(screenPxDistance + 0.5)`; outline: the alpha channel's true distance field gives `saturate(screenPxRange × (a − 0.5 + outlineWidth) + 0.5)` for the outline coverage under the fill |
| SHADOW | the closed-form Gaussian-blurred rounded rectangle (separable erf approximation of a box convolved with a Gaussian of σ = softness / 2), multiplied by colour alpha |
| FOCUS_RING | coverage between distance 0 and `borderWidth` outside the rounded rectangle |

`Flux_UIDecodeColour` converts sRGB8 to linear with the exact sRGB curve; the output stage is § "Colour and HDR output". Each program's reflection is checked against its per-pass table by the shader gate step `ShaderChecks`.

## Atlas pages on the GPU

- Pages are `Flux_PersistentReadOnly` textures created by `Flux_UIRenderer::CreatePage` through `Flux_PersistentResources::CreateTexture` with format `RGBA8_UNORM` (distance pages) or `RGBA8_SRGB` (colour pages), one mip, usage SRV, registered in the bindless heap (one descriptor per page, counted against `Z2-B-RHI-009`).
- Static pages come from the cooked font (`.zfont` section 6, Documents.md "MSDF atlas page payload") and image sprite sheets (textures loaded through ASSET, already GPU-resident; UI references their SRV and creates no page for them).
- Dynamic pages (glyphs generated at run time) are rebuilt CPU-side; when a page gained glyphs during a frame, `Zenith_UISystem::EndFrame` uploads it by creating a replacement texture with the page's full contents and releasing the previous one after its last use (deferred deletion through the persistent-resource retirement), the "reallocate and swap" policy; the glyph cache swaps the page handle in its placements in the same call, and quads written before the swap still name the old descriptor, which stays valid until retirement.
- A placement's quad is emitted only when `IsPageResident` is true for its page (the upload completed); otherwise the glyph is skipped as pending.
- At most `ui.atlas.pageUploadsPerFrame` (default 2) pages are created per frame, static and dynamic together, so UI's page upload traffic is at most `pageUploadsPerFrame × page bytes` per frame: 8 MiB at the defaults with 1,024² pages, 32 MiB with the largest permitted 2,048² pages; further dirty pages wait for the next frame. That traffic goes through the persistent-resource upload path (`Zenith2/Docs/Rendering/GpuResources.md → "Persistent resources"`) and is measured against `Z2-B-RHI-006` together with the quad data.

## Colour and HDR output

<!-- elaboration: the per-mode blending table -->
1. Authoring colours are sRGB8 with straight alpha; the shaders decode to linear Rec.709, compute coverage, then call LIGHT's `Flux_EncodeOutput(half3 linear, fNitsForOne, uOutputMode)` (`Zenith2/Docs/Design/LIGHT/ColourManagement.md → "UI and HDR"`) with `fNitsForOne` = `r.hdr.uiNits`, and output the result premultiplied by coverage × alpha. What `Flux_EncodeOutput` returns, and therefore in which space the fixed-function blend happens, follows the output mode (`Zenith2/Docs/Design/LIGHT/ColourManagement.md → "Output transforms"`):

| Output mode | Output view | `Flux_EncodeOutput` returns | Blending |
|---|---|---|---|
| `FLUX_OUTPUT_MODE_SDR` | `B8G8R8A8_SRGB` (sRGB view) | the plain linear value (`fNitsForOne` is ignored) | linear: the hardware decodes the destination, blends the linear source and encodes once on write (IEC 61966-2-1), so the shader never encodes |
| `FLUX_OUTPUT_MODE_HDR10` | `A2B10G10R10_UNORM_PACK32` | PQ of the Rec.2020 value at `fNitsForOne` nits for 1.0 | in PQ code values (not linear), LIGHT's stated behaviour for HDR10 UI |
| `FLUX_OUTPUT_MODE_SCRGB` | `R16G16B16A16_SFLOAT` | `value × fNitsForOne / 80` | linear |

2. SDR blending is therefore linear-light blending, not the sRGB-space blending of authoring tools: a 50 % white quad over black stores `round(255 × encode_sRGB(0.5))` = 188 (`Test_UIRender_PremultipliedBlending`, ±1 for the hardware encoder), and gradients interpolate in linear space. HDR10's non-linear blending is LIGHT's acceptance item for the Game2D slice (`Zenith2/Docs/Design/LIGHT/ColourManagement.md → "UI and HDR"`), and UI adds no second encode on any path.
3. World-space panels are scene-referred (§ "UIWorldSpace pass") and go through tonemapping like any surface.
4. Colour-vision modes and contrast settings are applied CPU-side to theme colours at style resolution (Accessibility.md "Colour-vision modes and contrast"), never as a full-screen filter over the game image.

## Headless and device loss

<!-- elaboration: one definition of headless for UI -->
- **Headless** is the runtime configuration without Flux ([Z2-N-ARCH-785]; the UI row of `Zenith2/Docs/Architecture.md`'s module table: "layout and logic run; no UI passes"): `Zenith_ModuleStartContext::GetRenderer()` is null, so module start step 6 creates no `Flux_UIRenderer`, registers no view extension and enables nothing in Flux; no atlas page is created, no MSDF is generated and no pass is declared. Everything else runs: documents load, layout, styles, bindings, input, focus and navigation, events and requests, localization, the text pipeline in measure-and-layout mode, and the accessibility tree. `BuildDrawList` still builds draw lists (glyph quads omitted, every glyph EMPTY for drawing) and the extractor builds them for tests through `Zenith_UIContext::BuildDrawList`, but no render world exists to receive them, so the extractor writes nothing. Tests of the CPU side therefore compare draw lists and trees, never pixels.
- **GPU tests are not headless runs**: UI's pixel tests (UI's files of `ZenithFlux_Tests` and the GPU cases of `Zenith_Integration_UI` under `Zenith2/Tests/UI/Render/`, Tests.md "Render tests") create an RHI device directly on WARP (D3D12) and lavapipe (Vulkan) ([Z2-N-ARCH-793]), use the real `Flux_UploadRing`, `Flux_PersistentResources` and `Flux_UIRenderer`, and need no physical GPU, so they are not `needs-gpu`; their expected values are analytic (coverage at sampled pixels, encoded values computed in the test), never committed images.
- Device loss: Flux's recovery recreates persistent resources and requests a full extraction; the UI renderer's pages are lost with the device, so `Zenith_UISystem` observes `Flux_UIRenderer::IsPageResident` turning false for every page and re-creates every static page from the font assets' CPU copies (fonts keep page payloads resident CPU-side while loaded, part of `Z2-B-UI-001`) and every dynamic page from its CPU image, at most `ui.atlas.pageUploadsPerFrame` pages per frame; glyphs are skipped as pending until their page is resident again (`Test_UIRender_DeviceLossRecreatesPages`).
