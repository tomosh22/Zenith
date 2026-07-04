# Flux Shadows — Cascaded Shadow Maps

Sun (directional) shadows via a 4-cascade CSM. Depth-only D32_SFLOAT maps at
`ZENITH_FLUX_CSM_RESOLUTION` (2048²) each, rendered by `SetupRenderGraph`'s four
"Shadow Cascade N" passes (recorded in parallel; cascade 0 owns the once-per-frame
CPU matrix update via its `Prepare` callback). The deferred lighting pass
(`Shaders/DeferredShading/Flux_DeferredShading.slang`) samples them.

## Method (AAA baseline, no-TAA-safe)

The fit and sampling are deliberately deterministic frame-to-frame because the
engine has **no TAA** — anything that jitters per-frame would crawl/sparkle.

**Cascade fit (`Flux_ShadowsImpl::UpdateShadowMatrices`):**
- **PSSM split scheme** — log/uniform blend (`dbg_fSplitLambda`, ~0.85) over
  `[camera near, dbg_fShadowDistance]` (capped by the camera far plane). Packing
  resolution into a near shadow distance is what keeps 2048² maps looking sharp.
- **Bounding-sphere fit** — each cascade is fit to the bounding *sphere* of its
  frustum slice. The sphere radius depends only on FOV/aspect/split, not camera
  orientation, so the cascade extent (and texel footprint) is rotation-invariant
  → no edge shimmer. (The old AABB-in-light-space fit was orientation-dependent.)
- **Texel snapping** — the projection origin is quantised to whole shadow texels
  so the sampling grid is locked to world space → no crawl as the camera moves.
- **Bounded caster extension** (`dbg_fCasterExtendRadii`, ~1 radius) pushes the
  light origin back past the sphere so off-frustum occluders still rasterise in,
  *without* the old 17× depth-range inflation that destroyed depth precision.

**Sampling (deferred shader):**
- The PCF/PCSS **filter** lives in `Shaders/Common/ShadowSampling.slang` as
  `ShadowFilterPCSS` (an `IShadowFilter` — the shared shadow-filter seam). The
  deferred composite fills one from its `ShadowSampling` CB and drives it; the
  **cascade walk stays in the shader** (each consumer's differs — the translucent
  pass has its own cheaper 4-tap `ShadowFilterSimple4Tap` conformed via
  `extension`). See `Shaders/SHADER_STYLE.md` → *Interface / Extension Seams*.
- Cascade selected by **view-space depth** vs the CPU-computed split distances
  (stable), with a **cross-fade band** into the next cascade to hide the seam.
- **Depth bias is fixed-function only** — never in the sampling shader. The
  caster pipelines (Static/Animated/Instanced/Terrain `*_ToShadowmap`) enable
  `m_bDepthBias` + `m_bDynamicDepthBias`, and `ExecuteShadowCascade` sets the
  slope/constant factors per cascade via `SetDepthBias()`
  (→ `vkCmdSetDepthBias`). Slope-scaled bias carries the load (it works on D32
  float where a constant bias is unreliable). Tunable: Render/Shadows → Depth
  Bias Constant / Slope.
- **Normal-offset** — receiver pushed along its normal by `N` texels scaled by
  `sin(angle-to-light)`, self-scaling per cascade via world-units-per-texel.
  This is NOT depth bias (no fixed-function equivalent); it handles grazing-angle
  acne without peter-panning and lets the hardware bias stay small. The shadow
  comparison itself uses the receiver's true depth (no shader-side bias).
- **Optimised PCF** over a Vogel disk (raw-depth `Gather` → per-texel compare →
  average; never pre-filter depth). Disk rotated per-pixel by interleaved-gradient
  noise (stable per pixel/frame).
- **PCSS** (`dbg_bPCSSEnabled`) — a blocker search estimates penumbra width from
  the blocker→receiver gap and the sun's angular size (`dbg_fSunAngularRadius`),
  so shadows are crisp at contact and soften with distance.

## Data flow / contract

- Each cascade is a first-class RENDER VIEW (fixed slots 1..4 in
  `Flux/RenderViews/Flux_RenderViews.h`): its pass declares `.View(1+cascade)`,
  which binds a per-cascade VIEW descriptor set whose `g_xView` holds the sun
  ortho matrices (staged by `UpdateShadowMatrices` into the view registry). The
  CASTER shaders (`UnifiedMesh`/`Terrain` `*_ToShadowmap.slang`) read the sun
  view-proj via the spine accessor `GetViewProjMat()` like any other pass — no
  per-draw cascade index remains.
- All-cascade `sun view×proj` matrices → a single `StructuredBuffer<float4x4>`
  in the persistent VIEW set (set 1, binding 2; declared as
  `g_xViewSet.g_xShadowMatrices` in `Common/Bindings.slang`, read by RECEIVERS via
  the `GetShadowMatrix(iCascade)` accessor) — RECEIVERS ONLY: the lit/fog consumers
  project receiver positions with it. A frame-indexed
  host-coherent dynamic buffer (graph-invisible), written once per frame by
  `Zenith_Vulkan::WritePersistentViewBuffer` (replicated into every view slot's
  set). Per-cascade GPU frustum culling of the unified scene populates each
  cascade view's slice of the shared cull-output buffers, so each shadow view
  draws only the objects inside its own frustum.
- `ShadowSampling` CB (binding 24, set 0) carries per-cascade split view-depths /
  world-per-texel / depth-range + global filter params. GPU mirror is
  `Flux_ShadowSamplingGPU` (`Flux_ShadowsImpl.h`); it MUST match
  `ShadowSamplingLayout` in `Shaders/DeferredShading/Flux_DeferredShading.slang` byte-for-byte (6× float4,
  no scalar straddling). Seeded with sane defaults at Initialise so a
  shadows-disabled boot can't feed garbage tap counts to the PCF loop.

Tunables are exposed under **Render/Shadows** in the debug-variable panel.

## Caveats / TODO

- **Terrain does not cast** (`Flux_TerrainImpl::RenderToShadowMap` is stubbed and
  the call in `ExecuteShadowCascade` is commented out). Terrain *receives* shadows
  (it's in the G-buffer); it just doesn't occlude. Enabling it means driving the
  GPU-driven indirect terrain draw into the cascade passes and declaring the
  indirect/visible-count buffer reads on each shadow pass for correct barriers.
- Shadows are sun-only; dynamic (point/spot) lights are unshadowed.
- `GetFOV()` returns radians for the game camera but degrees for the editor camera
  while Stopped/Paused — `UpdateShadowMatrices` normalises defensively (any value
  > π is treated as degrees).
