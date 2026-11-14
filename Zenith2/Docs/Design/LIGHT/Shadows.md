# LIGHT design: shadows

Section file of `Zenith2/Docs/Design/LIGHT.md`. It specifies the shadow atlas, the shadowed-light budget, the shadow planner that view-list construction calls, atlas tile sizing and allocation, cascaded shadow maps with their split scheme and stabilisation, local-light shadow views (spot, point, rect and disc), the shadow-view record, filtering and bias, shadow sampling in shaders, and screen-space contact shadows. The shadow views themselves are Rendering's (`Zenith2/Docs/Rendering.md → "Render views"`, `VIEW_KIND_SHADOW`), as are their passes (`Zenith2/Docs/Rendering/PerPass/Common.md → "ShadowInstanceCull"`, `→ "ShadowDepth"`, `→ "ShadowHiZBuild"`, `→ "ContactShadows"`) and their culling (`Zenith2/Docs/Rendering/Culling.md → "Shadow view culling"`).

LIGHT is the single owner of the shadow-atlas allocator and of shadow planning: `Flux_AtlasTile` and `Flux_ShadowAtlasAllocator` (a buddy quadtree) are declared once, in the leaf header `Zenith2/Engine/ZenithFlux/Flux_ShadowAtlas.h` ("Flux_ShadowAtlas header"), and RPROD's view-list construction reaches the planning through `Flux_LightingSystem::PlanShadows` (`Zenith2/Docs/Design/LIGHT.md → "Flux_Lighting header"`; row L29 of the cross-area notes answers RPROD's rows RP7 and RP18).

## Scope of this file

| Topic | Heading |
|---|---|
| the atlas texture | "Shadow atlas" |
| which lights get shadows | "Shadowed-light budget" |
| the one entry view-list construction calls | "Shadow planning in view-list construction" |
| where each view renders in the atlas | "Atlas layout", "Atlas tile sizing", "Atlas allocation" |
| the sun | "Cascaded shadow maps" |
| local lights | "Spot shadow views", "Point shadow views", "Area light shadow views" |
| the GPU record | "Shadow view record" |
| depth rendering state | "Shadow depth rendering" |
| filtering and bias | "Filtering", "Bias" |
| shader sampling | "Shadow sampling in shaders" |
| screen-space contact shadows | "Contact shadows" |
| the headers | "Flux_ShadowAtlas header", "Flux_Shadows header" |

## Shadow atlas

The shadow atlas is one 2D depth texture per frame (`Flux_FrameShared::m_xShadowAtlas`, `Zenith2/Docs/Rendering.md → "Render views"`), shared by every shadow view of the frame (design choice LR2 of `Zenith2/Docs/Design/LIGHT.md → "Design choices this family makes"`).

| Property | Value |
|---|---|
| format | `D24_UNORM_S8_UINT`; `D32_FLOAT_S8X24_UINT` under the capability fallback (`m_bDepthD24S8` false, [Z2-N-REND-029]); never reversed Z ([Z2-N-REND-593]) |
| extent | `render.quality.shadowAtlasWidth` × `render.quality.shadowAtlasHeight` of the tier (`Zenith2/Docs/Design/LIGHT/QualityTiers.md → "Shadow tiers"`), read from `Flux_ShadowSettings::m_uAtlasWidth` × `m_uAtlasHeight` |
| lifetime | a graph transient created by `Flux_AddCascadedShadows` each frame (aliased like every transient, Z2-D-057); no shadow content survives a frame (design choice LR1) |
| first write | an `AddClearPass` clearing the whole atlas to depth 1.0 precedes the first shadow depth pass, which the graph folds into that pass's load op where the render areas match ([Z2-N-REND-915]); every tile is then cleared again by its own shadow depth pass (`CLEAR` over the tile's render area), so a tile never shows the previous occupant of aliased memory |
| sampling | `uFLUX_SAMPLER_SHADOW_LESS_EQUAL` (7) for comparison taps; `uFLUX_SAMPLER_POINT_CLAMP` (0) for the blocker search; `uFLUX_SAMPLER_LINEAR_CLAMP_BORDER_WHITE` (11) is never used on the depth aspect, because every lookup clamps its UV into the tile's interior rectangle |
| consumers | forward and transparent opaque shading, deferred lighting, the froxel injection (`Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Scattering injection"`) |

<!-- assumption: pending RPROD, X103 -->
With no shadowed light in the frame, or with the view's `FLUX_VIEW_FEATURE_SHADOW_RECEIVE` bit clear, `Flux_AddCascadedShadows` creates no atlas and every consumer's `m_xShadowAtlas` and `m_xShadowViews` fields are left unset; the fields are `[Optional]` in ForwardPlusOpaque, ForwardOpaquePerObject, DeferredLighting, TransparentForward and VolumetricFog for that reason (row L5; item (e) of X103 (applied by core-doc batch 4)), and every light record carries `m_uShadowIndex = 0xFFFFFFFF`.
<!-- /assumption -->

## Shadowed-light budget

Rendering selects the shadowed lights on the CPU during view-list construction ([Z2-N-REND-604]). The selection runs inside `Flux_LightingSystem::PlanShadows`, which RPROD's `Flux_BuildViewList` calls (`Zenith2/Docs/Design/RPROD/Views.md → "Shadowed-light selection"`), so the rule below is the only selection rule and RPROD declares no second one.

| Input | Definition |
|---|---|
| candidates | every CPU light-mirror entry carrying `FLUX_LIGHT_FLAG_CASTS_SHADOW` whose intensity is above 0 and whose bounding volume (the light test of `Zenith2/Docs/Design/LIGHT/Lighting.md → "Light and decal list build"`) intersects the union of the frusta of `xNonShadowViews`' main, editor and texture views; the sun and moon slots and every other directional light always intersect |
| luminous power | `Flux_LightLuminousPower`: point `4π I`, spot `2π(1 − cos θ_outer) I`, rect `π w h L` (two-sided × 2), disc `π · π r² · L` (two-sided × 2), directional `+∞` |
| projected solid angle | `2π (1 − √(1 − min(1, R² / d²)))` of the light's range sphere R seen from the nearest such view position at distance d (4π when inside it) |
| priority | `Flux_ComputeShadowPriority(xLight, xNearestViewPositionWS)`: luminous power × projected solid angle × `Flux_RenderLightDelta::m_fShadowPriorityBias` |
| directional lights | always shadowed when casting and never counted against the local cap; at most `uFLUX_MAX_DIRECTIONAL_LIGHTS` = 2, in the directional order of `Zenith2/Docs/Design/LIGHT/Lighting.md → "Light and decal list build"` (the sun slot, the moon slot, then scene directional lights by luminance) |
| local cap | `render.quality.maxShadowedLights` (`Flux_ShadowSettings::m_uMaxShadowedLocalLights`): local lights only; a point light counts once against the cap and renders six faces |
| ordering and tie-break | priority descending, then `Flux_RenderLightKey::m_ulValue` ascending; this key is the one tie-break of shadow selection and allocation (row L29) |

The shadow views of visible shadowed lights are never deferred by the per-tier view cap `Z2-B-RPROD-022` ([Z2-N-BUDGET-262]); the local cap is the mechanism that bounds them.

## Shadow planning in view-list construction

`Flux_LightingSystem::PlanShadows(const Flux_ViewList& xNonShadowViews, Zenith_Arena& xFrameArena)` is the one shadow-planning entry. RPROD's `Flux_BuildViewList` calls it after it has built the non-shadow views (step 1 of `Zenith2/Docs/Design/RPROD/Views.md → "Shadowed-light selection"`, through the `Flux_LightingSystem&` its view registry borrows) and turns the returned plans into shadow views; RPROD declares no planner, no tile type, no packer and no shadow counter of its own (RPROD row RP18, which withdraws RP7).

| Step | Owner | Action |
|---|---|---|
| 1 | LIGHT | select the shadowed lights by the rule of "Shadowed-light budget" over the CPU light mirror |
| 2 | LIGHT | per shadowed directional light, the cascade setups of "Cascaded shadow maps", fitted to the sizing view (the main view of `xNonShadowViews`, else its first editor view; with neither, no cascade is planned); per shadowed local light, the tile size of "Atlas tile sizing" with its hysteresis |
| 3 | LIGHT | `Flux_ShadowAtlasAllocator::BeginFrame`, the cascade tiles, then "Atlas allocation" over the local requests |
| 4 | LIGHT | one `Flux_ShadowViewPlan` and one `Flux_ShadowViewRecord` per shadow view in allocation order (cascades first, then local faces), written into the frame arena and published as `Flux_LightingFrame::m_axShadowViews` and `m_axShadowRecords`; a light whose request was dropped keeps no view. The records reach the upload ring only through the ticket the shadow feature function declares and `WriteFrameData` fills at order 5 |
| 5 | LIGHT | for every light that received a delta this frame or whose shadow index differs from the previous frame's, `Flux_GPUScene::StageLightRecord(slot, Flux_EncodeLightRecord(delta, uShadowIndex))` (row L30), with `uShadowIndex` the record index of the light's first view, or `0xFFFFFFFF` for a light unshadowed this frame (step 3 of `Zenith2/Docs/Design/LIGHT/Lighting.md → "Division of the delta work with the GPU scene"`); this is the scatter that reaches the light records before any shadow view renders ([Z2-N-REND-604]) |
| 6 | RPROD | one `VIEW_KIND_SHADOW` `Flux_View` per plan, in plan order: identity `Flux_MakeViewID(VIEW_KIND_SHADOW, the light's key, plan.m_uSubIndex)`, camera from the plan's matrices, output `FLUX_VIEW_OUTPUT_SHADOW_ATLAS_TILE` with the plan's tile; `Flux_ViewList::m_axShadowPlans` holds the plans in the same order |

`PlanShadows` returns `FLUX_LIGHTING_ERROR_INVALID_SETTINGS` when the atlas settings violate "Atlas layout" (no shadow view is planned; every light is unshadowed); per-request atlas exhaustion is counted in `Flux.Shadows.Dropped` (the one counter of this event), never returned. Editor and texture views receive the sizing view's cascades; a point beyond the last cascade of the sizing view is unshadowed by the sun in every view. `PlanShadows` is a pure function of its inputs and the per-light hysteresis state, so the plans, the view list and the atlas contents are deterministic.

## Atlas layout

| Region | Placement |
|---|---|
| cascades | a strip along the top edge: cascade c at `(c × T_c, 0)` with size `T_c = Flux_ShadowSettings::m_uCascadeTileTexels` (`render.quality.shadowCascadeTileTexels`), for `c < m_uCascadeCount` (`render.quality.shadowCascades`); a second shadowed directional light's cascades take the next strip `(c × T_c, T_c)` in the frames it is shadowed |
| local lights | the remaining area below the frame's cascade strips (one strip per shadowed directional light), managed by the buddy quadtree of "Atlas allocation" in the blocks of `m_uLocalTileMaxTexels` that fit entirely in it at positions aligned to their size; the validation rules below guarantee at least one such block with one strip, and a frame with two strips whose region holds none drops every local request (counted in `Flux.Shadows.Dropped`) |

- Four configuration validation rules, with `L = m_uLocalTileMaxTexels` and `roundUp(x, L)` the smallest multiple of L not below x: `m_uAtlasWidth ≥ m_uCascadeCount × T_c` (the strip fits), `m_uAtlasWidth ≥ L` (a local block fits across), `m_uAtlasHeight ≥ roundUp(T_c, L) + L` (one size-aligned local block fits below one strip: a block starts at a multiple of its own size, so a strip height that is not a multiple of L wastes the rows up to the next multiple), and `L ≥ m_uLocalTileMinTexels`. A tier file violating any of them is rejected by `Flux_ReadLightQualitySettings` with `FLUX_LIGHTING_ERROR_INVALID_SETTINGS` naming the keys, and the tier's shadow settings keep their previous values; `Flux_ShadowAtlasAllocator::BeginFrame` applies the same rules to its layout. Every tier column of `Zenith2/Docs/Design/LIGHT/QualityTiers.md → "Shadow tiers"` satisfies them (T_c is a multiple of L in each, so `roundUp(T_c, L) = T_c`); a layout such as T_c = 512, L = 1,024, height 1,536 passed the old two rules and held no aligned block, and is now refused. A frame with two shadowed directional lights (a sun and a moon above 1 lux at the same time) has one strip less of local region; the ENV sun below the horizon has intensity 0 and is not a candidate, so the case is rare.
- Tile coordinates and sizes are texels and powers of two; every tile is aligned to its own size.

## Atlas tile sizing

A local light's desired per-face tile size follows its projected size in the sizing view (the main view, or, with no main view, the first editor view; with neither, the minimum size), in texels:

`desired = clamp(nextPow2(s × k × H), m_uLocalTileMinTexels, min(m_uLocalTileMaxTexels, uMaxTileClamp))`

| Symbol | Meaning |
|---|---|
| s | the range sphere's projected radius as a fraction of the view height: `R / (d × tan(fovY / 2))`, 1 inside the sphere |
| k | `Flux_ShadowSettings::m_fLocalResolutionScale` (`render.quality.shadowLocalResolutionScale`, default 1.0) |
| H | the sizing view's output height in pixels |
| point lights | the per-face size is `desired / 2`, clamped again to the minimum |
| `uMaxTileClamp` | `Flux_RenderLightDelta::m_uShadowMaxTileTexels` when non-zero |

Hysteresis, per light (state in `Flux_CPULight`): the current size changes to the desired size on the first frame the light is shadowed, and afterwards only when the desired size has been at least twice or at most half the current size for 8 consecutive frames. A light that loses its shadow keeps its state for 60 frames, then resets. Frames are counted in submitted frames: `PlanShadows` computes the new hysteresis values into frame-arena copies and `EndFrame` writes them into `Flux_CPULight` only when the frame submitted (`Zenith2/Docs/Design/LIGHT.md → "Deferred and failed frames"`). `Flux_DesiredShadowTileTexels` is the pure function of the formula; the hysteresis lives in `PlanShadows`.

## Atlas allocation

`Flux_ShadowAtlasAllocator` runs once per frame inside `Flux_LightingSystem::PlanShadows`.

| Step | Rule |
|---|---|
| 1 | reset: the cascade strips are reserved; the local region is split into free blocks of `m_uLocalTileMaxTexels`, scanned row-major from the top-left |
| 2 | order the local requests by current tile size descending, then priority descending, then `Flux_RenderLightKey` ascending |
| 3 | for each request, allocate its face count (1, or 6 for a point light) of equal square tiles from the buddy quadtree (a free block is split into four children until the requested size; the first free block in Morton order within the row-major block order is taken) |
| 4 | if any face fails, free the request's faces and retry at half the size, down to `m_uLocalTileMinTexels` |
| 5 | a request that fails at the minimum size is unshadowed this frame: `AllocateLocal` returns `FLUX_LIGHTING_ERROR_ATLAS_FULL`, `PlanShadows` writes `m_uShadowIndex = 0xFFFFFFFF` for it, and `Flux.Shadows.Dropped` counts it |
| 6 | `PlanShadows` writes one `Flux_ShadowViewRecord` per face in allocation order (directional cascades first) into the frame's shadow-view record array |

Descending-size allocation into a power-of-two quadtree packs without fragmentation until the region is full, so step 4 triggers only when the region is exhausted. The allocation is a pure function of the ordered request list, so it is deterministic. Complexity: O(requests × log(region / minTile)).

## Cascaded shadow maps

### Cascade splits

For the sizing view's near plane n, the shadow distance `D = min(Flux_ShadowSettings::m_fShadowDistance, viewFar)`, cascade count N and blend λ (`m_fSplitLambda`, default 0.8), the practical split scheme (Zhang et al. 2006) gives split i:

`c_i = λ · n · (D / n)^(i / N) + (1 − λ) · (n + (D − n) · i / N)`, i = 0 … N, so `c_0 = n` and `c_N = D`.

`Flux_ComputeCascadeSplits` evaluates it in `double` and stores `float`; N is in [1, 4].

### Stabilisation

| Step | Rule |
|---|---|
| 1 | bounding sphere of the view-frustum slice `[c_i, c_{i+1}]`: with `k = √(tan²(fovX/2) + tan²(fovY/2))`, the centre lies on the view axis at `z = min(c_{i+1}, (c_i + c_{i+1})(1 + k²)/2)` and the radius is `r = √((z − c_{i+1})² + k² c_{i+1}²)`; an orthographic sizing view uses the slice box's bounding sphere |
| 2 | quantise the radius up: `r' = ceil(r × 16) / 16` metres, so small FOV or near-plane changes do not change the cascade's size |
| 3 | light basis: forward `L` = the light's direction; up `U = (0, 1, 0)`, or `(1, 0, 0)` when `|L · U| > 0.99`; `X = normalize(cross(U, −L))`, `Y = cross(−L, X)`; the basis depends only on the light, never on the camera, so the projection never rotates with the view |
| 4 | texel size `t = 2 r' / T_c`; project the sphere centre into the light basis and snap its X and Y coordinates down to multiples of t |
| 5 | orthographic projection (standard Z, `Zenith_MakeOrthographic`, `Zenith2/Docs/Design/CORE/Maths.md → "Matrix types"`) with left, right, bottom, top at the snapped centre ± r', near at `−(r' + m_fCasterExtensionMetres)` and far at `+r'` along −L from the centre |
| 6 | depth clamp is enabled in the cascade's shadow depth PSO (pancaking), so casters in front of the near plane still write the nearest depth |

- A camera translation smaller than one texel leaves every cascade's projection bit-identical except for the snapped translation, and a camera rotation leaves the cascade size unchanged (`Test_CSM_StableUnderRotation`, `Test_CSM_TexelSnapping`).
- The orthographic projection is linear in depth, so D24 gives a uniform step of `(far − near) / 2²⁴` (about 0.05 mm over 900 m), and no precision check applies ([Z2-N-REND-593]).

### Cascade selection and blending

| Rule | Value |
|---|---|
| selection | the first cascade whose `m_fSplitFar` exceeds the pixel's linear view depth |
| blend band | the last `m_fCascadeBlendFraction` (default 0.1) of each cascade's depth range blends linearly into the next cascade's result (two lookups inside the band only) |
| fade-out | the last 5% of D fades the sun shadow to 1 (unshadowed); beyond D the sun is unshadowed |
| cascade HiZ | `ShadowHiZBuild` runs per cascade where the tier enables shadow HiZ (`render.quality.shadowHiZ`, `Zenith2/Docs/Rendering/PerPass/Common.md → "ShadowHiZBuild"`) |

## Spot shadow views

| Parameter | Value |
|---|---|
| projection | perspective, standard Z, D24S8 (or the capability fallback): `Zenith_MakePerspectiveStandardZ(fov, 1, near, far)` |
| field of view | `fov = 2 θ_outer + 2 atan(g / (T/2))`, with the guard band g in texels ("Filtering") and T the tile size, clamped to 170° |
| near and far | `near = max(0.05, 0.01 × range)`, `far = range` |
| view orientation | looking along the light's axis; up as in step 3 of "Stabilisation" with L the axis |

## Point shadow views

| Parameter | Value |
|---|---|
| faces | six views in the order +X, −X, +Y, −Y, +Z, −Z (sub-index 0–5 of `Flux_MakeViewID`), each a square tile of the same size |
| field of view | `2 atan((T/2 + g) / (T/2))`, slightly above 90° so a filter footprint near a face edge stays inside its own face |
| near and far | as spot lights |
| face selection in shaders | the axis of the largest absolute component of `p − centre`, ties resolved in the face order above |

## Area light shadow views

Rect and disc lights (Stage 5) render one perspective shadow view from the emitter's centre along its normal with a 120° field of view and the near and far planes of spot lights. Surfaces outside the 120° cone receive the light unshadowed; the path-tracer area-light scene keeps its occluders inside the cone, and the limitation is design choice LR6 of `Zenith2/Docs/Design/LIGHT.md → "Design choices this family makes"`. Two-sided emitters shadow their front face only.

## Shadow view record

`Flux_ShadowViewRecord` is the `[FluxShared]` struct Rendering assigns to `Zenith2/Shaders/Common/GPUScene.slang` (`Zenith2/Docs/Rendering/ShaderSystem.md → "Shared GPU structs"`); LIGHT owns its layout. One record exists per shadow view per frame, in the upload ring, bound as `m_xShadowViews` (`Zenith2/Docs/Rendering/PerPass/ForwardPlus.md → "ForwardPlusOpaque"`).

<!-- elaboration -->
| Offset | Size | Field | Meaning |
|---|---|---|---|
| 0 | 64 | `m_xWorldToShadow` | row-major float4x4 from world space to atlas UV (x, y) and depth (z) with w for the perspective divide; the tile's scale and offset are folded in |
| 64 | 16 | `m_xAtlasRectUV` | u0, v0, u1, v1 of the tile's interior shrunk by the filter radius; every tap clamps into it |
| 80 | 4 | `m_fTexelWorldSize` | cascades: one texel's world size; perspective views: one texel's world size at 1 m from the light |
| 84 | 4 | `m_fNormalOffsetTexels` | the light's `m_fShadowNormalOffsetTexels` |
| 88 | 4 | `m_fSplitFar` | cascades: the cascade's far view depth; others: 0 |
| 92 | 4 | `m_uFlags` | bits 0–1 kind (0 cascade, 1 spot or area, 2 point face); bits 2–4 face index; bits 5–6 cascade index; bits 7–8 cascade count − 1; bit 9 orthographic; bits 10–11 filter quality; bits 12–15 reserved; bits 16–31 the light's depth bias in 1/1,024 m units |

`static_assert(sizeof(Flux_ShadowViewRecord) == 96)`; adding the layout to Rendering's shared-struct documentation is row L6 and item (e) of X103, which core-doc batch 4 applied.

## Shadow depth rendering

The pass tables are Rendering's ("ShadowDepth"); LIGHT fixes the values the table leaves to "the fixed shadow bias entry".

| State | Value |
|---|---|
| depth test | `LESS_OR_EQUAL`, write on (standard Z) |
| depth bias | constant 0, slope-scaled 2.0, clamp 0: the shadow entry `FLUX_DEPTH_BIAS_SHADOW` of the depth-bias table of `Zenith2/Docs/Design/RPROD/PSO.md → "Fixed state tables"` carries these values (RPROD row RP19); the core-doc record of "the fixed shadow bias entry" is item (e) of X103 |
| depth clamp | on for cascades (pancaking), off for perspective views |
| culling | back faces, unless the material is two-sided ([Z2-N-REND-923]) |
| LOD | shadow views select LOD from the world's main view (GEOM cross-area row GM6, `Zenith2/Docs/Design/GEOM/Meshes.md → "LOD selection"`) |
| buckets | opaque buckets first through the position-only index buffer, then alpha-tested buckets (`[AlphaTest]`) |

## Filtering

`uPERM_SHADOW_QUALITY` of the lighting and fog programs selects the filter; the tier chooses the value (`render.quality.shadowQuality`, "Shadow tiers").

| Quality | Filter | Guard band g | Taps |
|---|---|---|---|
| 0 | one bilinear comparison tap (2 × 2 hardware PCF) | 1 texel | 1 |
| 1 | 3 × 3 bilinear comparison taps with the weights of Castaño 2013 (the "optimised 5 × 5 PCF" kernel) | 2 texels | 9 |
| 2 | contact-hardening: a 16-tap blocker search (a Poisson disc rotated per pixel by interleaved gradient noise `IGN(pixel + 5.588238 × frameIndex)`), then a 16-tap comparison filter over the estimated penumbra | 8 texels | 32 |

- Penumbra estimate (quality 2): `w = (d_receiver − d_blocker) × S / d_blocker` texels, clamped to [1, 8], with S the light's size in texels at the receiver: the sun's angular radius × the cascade's texels per radian, a sphere light's `m_fSourceRadius`, a rect's half diagonal, a disc's radius; a punctual light (radius 0) uses quality 1.
- No blocker found in the search: the result is 1 (lit) without the filter.
- Every tap's UV is clamped into `m_xAtlasRectUV`, so no filter reads a neighbouring tile.

## Bias

| Term | Rule |
|---|---|
| normal offset | the receiver position moves along the geometric normal by `m_fNormalOffsetTexels × texelSize(p) × √(1 − saturate(n·l)²)` before projection, where `texelSize(p)` is `m_fTexelWorldSize` for cascades and `m_fTexelWorldSize × distance(p, light)` for perspective views |
| receiver depth bias | the comparison depth is reduced by the record's depth bias (bits 16–31 of `m_uFlags`), converted into the view's depth units |
| slope bias | the raster state of "Shadow depth rendering" |
| peter-panning bound | with the defaults, the light leak at a contact edge is at most 2 texels, which `Test_Shadows_ContactEdgeLeakBound` asserts on the analytic plane-on-plane scene |

## Shadow sampling in shaders

`Zenith2/Shaders/Lighting/Shadows.slang` exports one entry per light type; each returns a `half` visibility in [0, 1].

| Function | Rule |
|---|---|
| `Flux_SampleDirectionalShadow(Flux_LightRecord, float3 xPositionWS, float3 xNormalWS, float fLinearDepth, uint2 xPixel, uint uFrameIndex)` | cascade selection, blend band and fade of "Cascade selection and blending"; the cascade records follow `m_uShadowIndex` consecutively |
| `Flux_SampleSpotShadow(…)` | the one record at `m_uShadowIndex`, with the perspective divide |
| `Flux_SamplePointShadow(…)` | face selection of "Point shadow views"; the face record at `m_uShadowIndex + face` |
| `Flux_SampleAreaShadow(…)` | the spot form with the area penumbra; 1 outside the 120° cone |
| `Flux_SampleShadow(Flux_LightRecord, …)` | dispatches on `m_uType`; returns 1 when `m_uShadowIndex == 0xFFFFFFFF` |

A shadow lookup of a light whose atlas tile was dropped this frame returns 1, so an unshadowed light leaks for that frame, the same accepted leak as a missing shadow PSO ([Z2-N-REND-192]).

## Contact shadows

The pass is `Zenith2/Docs/Rendering/PerPass/Common.md → "ContactShadows"` (budget `Z2-B-LIGHT-006`, queue A), for one directional entry c: the first directional entry in frame-list order (words 2–3 of `m_xLightList`, `Zenith2/Docs/Design/LIGHT/Lighting.md → "Light and decal list build"`) whose record carries `FLUX_LIGHT_FLAG_CONTACT_SHADOWS`. Whenever the sun slot is active it is the sun slot, because the sun is entry 0 and carries the bit by the slot flag contract; it is never the moon slot, whose bit `ApplyRenderWorld` clears (`Zenith2/Docs/Design/LIGHT/Lighting.md → "Sun and moon slots"`); in a frame without environment slots (a world without ENV's celestial state) it is the first flagged scene directional light, with or without a role. `m_xSunDirectionVS` is entry c's direction.

| Step | Rule |
|---|---|
| 1 | per render pixel: reconstruct the view-space position from depth; skip (mask 1) when the depth is the far clear value |
| 2 | ray: from the position toward the sun (`m_xSunDirectionVS`, the negated light direction in view space), length `m_fMaxDistance` (`Flux_ContactShadowSettings::m_fMaxDistanceMetres`, `render.quality.contactShadowMaxDistanceMetres`, default 0.3 m desktop, 0.2 m mobile) |
| 3 | march `uPERM_STEPS` (8 or 16, `render.quality.contactShadowSteps`) steps in screen space with the start offset jittered by `IGN(pixel + 5.588238 × frameIndex)` of one step, so TAA integrates the noise |
| 4 | at each step compare the ray's linear depth with the scene's linear depth at the step's pixel: occluded when `0 < rayDepth − sceneDepth < thickness`, `thickness = m_fThicknessMetres + 0.02 × sceneDepth` (default 0.05 m) |
| 5 | first occlusion at step s (1 ≤ s ≤ steps, counted from the surface): `mask = s / steps`, so an occluder found at the first step (nearest the surface) darkens most (mask `1 / steps`) and one found at the last step (farthest along the ray) darkens least (mask 1 at s = steps); the mask is then raised toward 1 by a screen-border fade over the outer 5% of the viewport, `mask = lerp(1, mask, saturate(min(u, 1 − u, v, 1 − v) / 0.05))`; no occlusion: 1 |
| 6 | write `m_xMask` (R8_UNORM, 1 = lit) |

The mask multiplies entry c's shadow term only (step 3 of `Zenith2/Docs/Design/LIGHT/Lighting.md → "Evaluation order"`, which finds c by the same rule), never another light's: a second directional entry carrying the bit takes 1, because the mask was traced along c's direction alone. `Test_ContactShadows_SingleEntry` asserts it.

## Flux_ShadowAtlas header

The one declaration of `Flux_AtlasTile`, of the shadow-view plan and of the allocator. It is a leaf header (ZenithCore and `Flux_LightingErrors.h` only), so RPROD's `Flux_View.h` includes it for `Flux_ViewOutputs::m_xAtlasTile` and its `Flux_ViewContext.h` for `Flux_ViewList::m_axShadowPlans` (a `Zenith_Span<const Flux_ShadowViewPlan>`) without an include cycle: the plan type used to live in `Flux_Shadows.h`, which includes `Flux_RenderGraph.h` and uses `Flux_ViewList`, the cycle RPROD row RP18 raised for the tile. `Flux_RenderLightKey`, a member of the plan, is declared here for the same reason and `Flux_Lights.h` includes this header (answers RP18's ask for both types; row L29).

```cpp
// Zenith2/Engine/ZenithFlux/Flux_ShadowAtlas.h
#pragma once
#include "ZenithCore/Zenith_Matrix.h"
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_Types.h"
#include "ZenithFlux/Flux_LightingErrors.h"

/// @contract value Flux_RenderLightKey
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: it is copy-assigned into descriptors, records and tables.
/// Invalidation: None.
/// Units: None; a 64-bit key of Zenith2/Docs/Design/LIGHT/Lighting.md "Light keys", bit 63 set for named keys and
///   clear for entity keys.
/// Errors: None.
/// Version: since Z2 Stage 3. Never serialized; recomputed by the extractors every frame.
/// @endcontract
struct Flux_RenderLightKey { uint64 m_ulValue = 0; };

/// @contract value Flux_AtlasTile
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: the atlas packer assigns tiles into the shadow-view plans.
/// Invalidation: None; valid for the frame whose allocation produced it.
/// Units: Texels of the shadow atlas.
/// Errors: None.
/// Version: since Z2 Stage 3. Never serialized. The one declaration of the type.
/// @endcontract
struct Flux_AtlasTile
{
	uint16 m_uX = 0;
	uint16 m_uY = 0;
	uint16 m_uSize = 0;       // power of two
	uint16 m_uReserved = 0;
};
static_assert(sizeof(Flux_AtlasTile) == 8);

/// @contract enum Flux_ShadowViewKind
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3 (AREA since Z2 Stage 5). Never serialized; the values match bits 0–1 of the shadow-view
///   record's m_uFlags (AREA shares value 1's sampling with SPOT and is encoded as 1 there).
/// @endcontract
enum Flux_ShadowViewKind : uint8 { FLUX_SHADOW_VIEW_KIND_CASCADE = 0, FLUX_SHADOW_VIEW_KIND_SPOT, FLUX_SHADOW_VIEW_KIND_POINT_FACE, FLUX_SHADOW_VIEW_KIND_AREA };

/// @contract value Flux_ShadowViewPlan
/// Ownership: Plain value in the frame arena, produced by Flux_LightingSystem::PlanShadows.
/// Thread affinity: IMMUTABLE after the view list is built.
///   Members non-const: the view-list build fills each plan field by field before it becomes immutable.
/// Invalidation: Invalid after the frame arena resets.
/// Units: Texels (tile), metres (matrices, splits).
/// Errors: None.
/// Version: since Z2 Stage 3. RPROD's Flux_ViewList::m_axShadowPlans holds a span of this type.
/// @endcontract
struct Flux_ShadowViewPlan
{
	uint32 m_uLightSlot = 0;            // light-table slot
	Flux_RenderLightKey m_xLightKey;     // the light's key (Flux_MakeViewID owner)
	Flux_ShadowViewKind m_eKind = FLUX_SHADOW_VIEW_KIND_CASCADE;
	uint8 m_uSubIndex = 0;              // cascade or face; the sub-index of Flux_MakeViewID
	uint16 m_uReserved = 0;
	Flux_AtlasTile m_xTile;
	Zenith_Matrix4 m_xView;
	Zenith_Matrix4 m_xProjection;
	float m_fSplitNear = 0.0f;           // cascades
	float m_fSplitFar = 0.0f;
	uint32 m_uRecordIndex = 0;          // index of its Flux_ShadowViewRecord in Flux_LightingFrame::m_axShadowRecords
};

/// @contract value Flux_ShadowAtlasLayout
/// Ownership: Plain value, filled by PlanShadows from Flux_ShadowSettings.
/// Thread affinity: IMMUTABLE.
///   Members non-const: the producer default-constructs it and fills its fields before returning it.
/// Invalidation: None.
/// Units: Texels; counts.
/// Errors: None; Flux_ShadowAtlasAllocator::BeginFrame validates it.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Flux_ShadowAtlasLayout
{
	uint32 m_uAtlasWidth = 4096;
	uint32 m_uAtlasHeight = 4096;
	uint32 m_uDirectionalCount = 0;     // 0 … 2 cascade strips reserved
	uint32 m_uCascadeCount = 4;
	uint32 m_uCascadeTileTexels = 1024;
	uint32 m_uLocalTileMinTexels = 128;
	uint32 m_uLocalTileMaxTexels = 1024;
};

/// @contract type Flux_ShadowAtlasAllocator
/// Ownership: Owned by Flux_LightingSystem; one per renderer.
/// Thread affinity: OWNER_THREAD. Called from Flux_LightingSystem::PlanShadows on the Flux.BuildGraph node.
/// Invalidation: Tiles returned by one frame are invalid after the next BeginFrame.
/// Units: Texels.
/// Errors: AllocateLocal returns FLUX_LIGHTING_ERROR_ATLAS_FULL when the request cannot be placed at the minimum
///   size; the caller marks the light unshadowed. BeginFrame returns FLUX_LIGHTING_ERROR_INVALID_SETTINGS for a layout
///   violating the rules of "Atlas layout".
/// Version: since Z2 Stage 3. The buddy quadtree of "Atlas allocation" is the only shadow-atlas packer.
/// @endcontract
class Flux_ShadowAtlasAllocator
{
public:
	Zenith_Result<void, Flux_LightingError> BeginFrame(const Flux_ShadowAtlasLayout& xLayout);
	Flux_AtlasTile GetCascadeTile(uint32 uDirectional, uint32 uCascade) const;   // ZENITH_ASSERT in range
	Zenith_Result<void, Flux_LightingError> AllocateLocal(uint32 uTexels, uint32 uFaceCount,
		Zenith_Span<Flux_AtlasTile> axOutTiles);                                            // uFaceCount 1 or 6
	uint32 GetUsedTexelArea() const;

private:
	// Fixed storage sized for the widest legal layout: an 8,192 × 8,192 atlas, quadtree cells of 4,096 texels (the largest
	// local tile) down to 32 (the smallest), so sum over s = 32 … 4,096 of (8,192 / s)²
	// = 65,536 + 16,384 + 4,096 + 1,024 + 256 + 64 + 16 + 4 = 87,380 nodes (uNODE_WORDS = 1,366). Never resized and
	// never heap-allocated: BeginFrame clears the two bit planes; the node index layout is private to the allocator's source.
	static constexpr uint32 uNODE_COUNT = 87380;
	static constexpr uint32 uNODE_WORDS = (uNODE_COUNT + 63) / 64;
	uint64 m_aulAllocated[uNODE_WORDS] = {};   // node holds a tile or lies inside a reserved cascade strip
	uint64 m_aulSplit[uNODE_WORDS] = {};       // node is split into four children
	Flux_ShadowAtlasLayout m_xLayout = {};
	uint32 m_uUsedTexelArea = 0;
	bool m_bLayoutValid = false;                // false until a BeginFrame accepted a layout
};
```

The private members are part of the declaration so that `Flux_LightingSystem` holds the allocator by value with no allocation of its own; the header still needs only `Zenith_Types.h` for them (no ZenithCore container).

## Flux_Shadows header

```cpp
// Zenith2/Engine/ZenithFlux/Flux_Shadows.h
#pragma once
#include "ZenithCore/Zenith_Matrix.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithFlux/Flux_LightingErrors.h"
#include "ZenithFlux/Flux_LightSettings.h"
#include "ZenithFlux/Flux_Lights.h"
#include "ZenithFlux/Flux_RenderGraph.h"
#include "ZenithFlux/Flux_ShadowAtlas.h"   // Flux_ShadowViewKind, Flux_ShadowViewPlan, Flux_AtlasTile, Flux_RenderLightKey

/// @contract value Flux_CascadeSetup
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: the producer default-constructs it and fills its fields before returning it.
/// Invalidation: None.
/// Units: Metres; matrices world to light view and light view to clip.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Flux_CascadeSetup
{
	Zenith_Matrix4 m_xView;
	Zenith_Matrix4 m_xProjection;
	float m_fSplitNear = 0.0f;
	float m_fSplitFar = 0.0f;
	float m_fRadius = 0.0f;              // quantised bounding-sphere radius r'
	float m_fTexelWorldSize = 0.0f;
};

/// @contract functions Flux_ShadowFunctions
/// Ownership: Pure functions over values, and feature functions that add passes to the graph they receive.
/// Thread affinity: ANY_THREAD for the pure functions; the feature functions run on the Flux.BuildGraph node.
/// Invalidation: None.
/// Units: Metres, radians, texels.
/// Errors: Flux_ComputeCascadeSplits asserts uCascadeCount in [1, 4] and afOutSplits sized uCascadeCount + 1;
///   the feature functions add no pass when no shadow view exists.
/// Version: since Z2 Stage 3.
/// @endcontract
void Flux_ComputeCascadeSplits(float fNear, float fShadowDistance, uint32 uCascadeCount, float fLambda,
	Zenith_Span<float> afOutSplits);
Flux_CascadeSetup Flux_BuildStableCascade(const Flux_ViewCamera& xCamera, float fAspect, Zenith_Vector3 xLightDirection,
	float fSplitNear, float fSplitFar, uint32 uTileTexels, float fCasterExtensionMetres);
uint32 Flux_DesiredShadowTileTexels(const Flux_ViewCamera& xCamera, uint32 uOutputHeight, Zenith_Vector3 xLightPosition,
	float fRange, bool bPointLight, const Flux_ShadowSettings& xSettings, uint32 uMaxTileClamp);
Flux_ShadowViewRecord Flux_EncodeShadowViewRecord(const Flux_ShadowViewPlan& xPlan, uint32 uAtlasWidth, uint32 uAtlasHeight,
	uint32 uFilterQuality, float fNormalOffsetTexels, float fDepthBiasMetres);
float Flux_ComputeShadowPriority(const Flux_CPULight& xLight, Zenith_Vector3 xNearestViewPositionWS);   // "Shadowed-light budget"

void Flux_AddShadowView(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_FrameShared& xShared);
void Flux_AddCascadedShadows(Flux_RenderGraph& xGraph, const Flux_ViewList& xViews, Flux_FrameShared& xShared);
void Flux_AddLocalShadows(Flux_RenderGraph& xGraph, const Flux_ViewList& xViews, Flux_FrameShared& xShared);
Flux_RGTexture Flux_AddContactShadows(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, const Flux_ViewTargets& xTargets);
```

| Function | Contract |
|---|---|
| `Flux_AddShadowView` | Rendering's per-shadow-view function ([Z2-N-REND-725]): `ShadowInstanceCull`, `ShadowDepth` into the view's tile, and `ShadowHiZBuild` where the tier enables it; implemented in `Zenith2/Engine/ZenithFlux/Internal/Passes/Shadows/` |
| `Flux_AddCascadedShadows` | step 7 of the common prologue for every cascade view: creates the atlas transient and its clear, declares the frame's `Flux_ShadowViewRecord` array (96 bytes per record) through `Flux_LightingUploadList::Declare` (`Flux_UploadRing::DeclareFrameData`) and sets `xShared.m_xShadowViewRecords` (item (b) of X103) to the frame chunk `Flux_LightingFrame::m_xFrameChunk`, with the element base of its ticket as the consumers' plain-data `m_uShadowViewBase` (`Zenith2/Docs/Design/LIGHT.md → "Build-node upload data"`, row L42), writing nothing at setup: `Flux_LightingSystem::WriteFrameData` copies the records at order 5 (`Zenith2/Docs/Design/LIGHT.md → "Build-node upload data"`, row L35), then calls `Flux_AddShadowView` for each `VIEW_KIND_SHADOW` view whose plan kind is `CASCADE`, in view-list order, and sets `xShared.m_xShadowAtlas` |
| `Flux_AddLocalShadows` | the same for spot, point-face and area plans, called after `Flux_AddCascadedShadows`; creates the atlas and declares the records when no cascade did |
| `Flux_AddContactShadows` | Rendering's function; adds no pass when no directional light carries the flag or the view's `FLUX_VIEW_FEATURE_CONTACT_SHADOWS` bit is clear |

The implementation of the allocator and of `PlanShadows` is `Zenith2/Engine/ZenithFlux/Internal/Lighting/ShadowAtlasAllocator.cpp` and `Internal/Lighting/ShadowPlanning.cpp`; cascade setup is `Internal/Lighting/CascadeSetup.cpp`.
