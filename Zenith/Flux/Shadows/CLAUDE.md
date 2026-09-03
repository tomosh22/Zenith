# Flux Shadows — Cascaded Shadow Maps

Sun (directional) shadows via a 4-cascade CSM. Depth-only D32_SFLOAT maps at
`ZENITH_FLUX_CSM_RESOLUTION` (2048²) each, rendered by `SetupRenderGraph`'s four
"Shadow Cascade N" passes (recorded in parallel). The deferred lighting pass
(`Shaders/DeferredShading/Flux_DeferredShading.slang`) samples them.

**The once-per-frame CPU matrix update is NOT a cascade `Prepare`.** No cascade
pass registers one: `UpdateShadowMatrices` is called from `Zenith_Core.cpp` on the
main thread, *before* the render-task window opens, alongside the scene-graph
snapshot rebuild it is camera-derived like. That hoist is load-bearing — the
unified mesh cull's `Prepare` runs earlier in topological order (the cascades read
its cull output), so a cascade-owned `Prepare` would hand it last frame's frusta.

## Method (AAA baseline, jitter-free by construction)

The fit and sampling are deliberately deterministic frame-to-frame. **This is not
because the engine lacks TAA** — TAA is the default AA and the main view *is*
jittered. It is because the cascades are not: a cascade view stages
`m_xViewProjMatNoJitter == m_xViewProjMat`, so nothing in the shadow path is
allowed to move sub-pixel per frame, and anything that did would crawl/sparkle
straight through the temporal resolve rather than be smoothed by it.

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
  caster pipelines (UnifiedMesh/Terrain/Grass `*_ToShadowmap`) enable
  `m_bDepthBias` + `m_bDynamicDepthBias`, and `ExecuteShadowCascade` sets the
  slope/constant factors per cascade via `SetDepthBias()`
  (→ `vkCmdSetDepthBias`), then re-sets the slope for the terrain draw from
  Render/Shadows → Terrain Slope Bias (see the terrain bullet below). Slope-scaled
  bias carries the load (it works on D32 float where a constant bias is
  unreliable). Tunable: Render/Shadows → Depth Bias Constant / Slope.
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
- **Grass casts into cascades 0-1 only.** `Flux_GrassImpl::RenderToShadowMap` runs
  from inside `ExecuteShadowCascade` and issues one `DrawIndexedIndirect` of that
  cascade's LO blade partition (indirect slot `2 + cascade`); there is no partition
  for cascades 2-3 and the impl early-outs above index 1. Blades are **generated
  once and culled per view**: the placement CS tests every candidate against up to
  three plane sets (camera, cascade 0, cascade 1) and appends survivors into that
  view's partition, so the shadow blades are the same blades, not a second placement
  pass. All shadow blades use the **LO mesh** (15 indices at `firstIndex 33`), and
  the caster binds the **same `GrassDrawConstants`** the lit pass does — same wind
  block, same main-camera position — so a swaying blade casts the shadow it is
  actually standing in. Two switches gate it, both feeding
  `Flux_GrassImpl::IsShadowCastingEnabled()` alongside the engine-wide
  `m_bShadowsEnabled`: the `m_bGrassShadowsEnabled` graphics option and the
  `Flux/Grass/DisableShadowCasting` debug variable. Turning either off drops the
  cascade partitions out of the active-slot mask, so it removes the **placement
  work** as well as the two draws.
- **Every caster recorded into a cascade must call `UseBindlessTextures(2)` itself** —
  including depth-only ones that sample nothing. Set 2 is in every spine pipeline's
  layout because `Common/Bindings.slang` declares the block and Slang keeps declared
  `ParameterBlock`s; `BindPersistentSpineSets` auto-binds only GLOBAL/VIEW. Do NOT
  assume an earlier caster bound it: `Flux_UnifiedMeshImpl::RenderToShadowMap`
  early-outs on zero buckets *before* its own `UseBindlessTextures(2)`, so on a scene
  with no unified opaque casters the next caster in the cascade is the first user and
  inherits nothing. **The bind stays inside each caster rather than being hoisted to
  the top of `ExecuteShadowCascade`** — `UseBindlessTextures` binds against the
  CURRENT pipeline's layout, so there is no legal place to call it before the first
  `SetPipeline`, and hoisting would only re-create the dependency on which caster ran
  first. Enforced since 2026-08-07 by a debug pre-draw check
  (`Flux_PersistentSetLayouts::ShouldDemandBindlessBind`, asserted in
  `Zenith_Vulkan_CommandBuffer`): a pipeline that reads the table and whose own draw
  path did not bind it fails by name, whether or not an earlier caster would have
  covered it. Terrain's caster binds it too (`Flux_TerrainImpl::RenderToShadowMap`).
- **Terrain casts into ALL FOUR cascades** (since 2026-09-02; it was the largest
  structural shadow gap — mountains cast nothing into valleys). Design mirrors the
  unified path, with no new render-graph pass:
  - **Per-cascade cull slots.** Each terrain owns a second indirect/count pair
    (`m_xShadowIndirectDrawBuffer` = 4 cascade-major slots of `TOTAL_CHUNKS`
    20-byte records; `m_xShadowVisibleCountBuffer` = 4 uints), Flux-owned and
    created in `Flux_TerrainStreamingManagerImpl::RegisterTerrainBuffers`. The
    slot arithmetic is `Flux/Terrain/Flux_TerrainShadowCull.h` (pure, unit-tested).
  - **One dispatch culls five views.** "Terrain Culling Compute" dispatches
    `(64, 1 + 4, 1)`: row `y == 0` is the camera (unchanged), row `1 + c` tests
    every chunk AABB against cascade `c`'s snapped ortho box (planes from the view
    registry's `m_xViewProjMatNoJitter`, staged by `UpdateShadowMatrices`, uploaded
    per frame into `m_xShadowCullBuffer` = `Zenith_TerrainShadowCullGPU`) and
    appends survivors into slot `c`. "Terrain Reset Count and Indirect Arguments"
    clears every slot + count each frame, so the ZERO_PADDED_TO_MAX contract holds
    per slot and an inactive cascade (shadows off) is an EMPTY slot, not a stale one.
  - **The caster is the receiver's geometry.** Shadow rows resolve the SAME
    camera-distance LOD (hysteresis included) the camera row does, so the terrain
    self-shadows against its own surface; a coarser caster would put the receiver
    below it in concavities and read as acne no bias fixes. Cascades at/above
    `Render/Shadows/Terrain LOW LOD From Cascade` (default 3 = only the far
    cascade) cast the always-resident LOW mesh instead
    (`Flux_TerrainShadowCasterLOD`, `uFLUX_TERRAIN_SHADOW_FORCE_LOW_NEVER` = never).
  - **The draw.** `ExecuteShadowCascade` calls `Flux_TerrainImpl::RenderToShadowMap`
    LAST: `SetPipeline` (depth-only `Terrain_ToShadowmap`, empty FS) →
    `UseBindlessTextures(2)` → per terrain bind its own `TerrainConstants` CB (set 3
    binding 0, the dequant box) → `DrawIndexedIndirectCount` of slot `u` with two
    byte offsets (`Flux_TerrainShadowCullIndirectByteOffset` /
    `...CountByteOffset`). Every call is a `Flux_CommandBuffer` no-op on Null/D3D12.
  - **Own slope bias.** Terrain is a huge low-slope receiver AND caster of itself, so
    right before the terrain draw `ExecuteShadowCascade` re-sets the dynamic bias
    to `(shared constant, Render/Shadows/Terrain Slope Bias)` (default = the shared
    slope). Dynamic state persists in the command list, which is why terrain records
    last; a caster added after it must re-set the shared bias first.
  - **Graph edges.** Each cascade pass declares `READ_INDIRECT_ARG` of both slot
    buffers per terrain (gathered via `g_pfnZenithTerrainGather`, gated on
    `m_bCullingResourcesInitialized && m_bShadowCullResourcesInitialized`).
    Pinned by `FluxTerrain::ShadowCascadeReadsOfTheTerrainSlotsOrderAfterTheCullAndBarrier`.
- **`Terrain` MUST be registered before `Shadows` too**, for the same reason as the
  two above: the cascade reads of the terrain slot buffers link only to an
  EARLIER-declared writer. Pinned by `FluxTerrain::TerrainIsDeclaredBeforeShadows`.
- **`Grass` MUST be registered before `Shadows` too**, for the same reason
  `UnifiedMesh` is: each of cascades 0-1 declares a `ReadBuffer` on the grass blade
  pool / visible-index / indirect-args buffers, whose only writers are the grass
  Reset/Placement/IndirectFixup passes. Pinned by
  `FluxGrassImpl::GrassIsDeclaredBeforeShadows`.
- **`UnifiedMesh` MUST be registered before `Shadows`** in `RegisterDefaultFeatures`.
  The `ReadBuffer` declarations in `SetupRenderGraph` are what order the cascades
  after the cull — but a reader only links to an EARLIER-declared writer, so with
  `Shadows` first they produced no edges at all and the cascades ran unordered
  against their own producers (one landed between the cull-args reset and the cull,
  drawing zero casters). `Flux_RenderGraph::ValidateProducerBeforeConsumer` guards
  this; `Core::FeatureRegistryUnifiedMeshPrecedesShadows` pins the registration order.
- `ShadowSampling` CB (binding 24, set 0) carries per-cascade split view-depths /
  world-per-texel / depth-range + global filter params. GPU mirror is
  `Flux_ShadowSamplingGPU` (`Flux_ShadowsImpl.h`); it MUST match
  `ShadowSamplingLayout` in `Shaders/DeferredShading/Flux_DeferredShading.slang` byte-for-byte (6× float4,
  no scalar straddling). Seeded with sane defaults at Initialise so a
  shadows-disabled boot can't feed garbage tap counts to the PCF loop.

Tunables are exposed under **Render/Shadows** in the debug-variable panel.
`Render/Shadows/Terrain Casts Shadows` gates the terrain caster specifically
(off resolves the terrain cull to zero cascade slots AND skips the draw — it
removes the work, not just the result; see `Terrain/Flux_TerrainShadowCull.h`),
and `Graphics/Shadows/Enabled` (`Zenith_GraphicsOptions::m_bShadowsEnabled`)
gates the whole system.

## Diagnosing "X stopped casting a shadow"

**That sentence is two questions, and they fail independently.** Answer the
receiver one first — it is the one that has actually been wrong.

`--ds-debug=N` needs no rebuild (`Flux_DeferredShading.cpp`; the debug-variable
twin is `Render/DeferredShading/DebugMode`). The three that matter here:

| Mode | Shows | Reads as |
|---|---|---|
| `9` | the resolved sun shadow factor | white = lit, black = shadowed. **Start here.** |
| `8` | `NdotL` | black means the surface believes it faces away from the sun — no sun term AND no shadow lookup |
| `7` | the G-buffer world normal | `n * 0.5 + 0.5`, so flat ground should be ~`(128, 255, 128)` |

The deferred pass only resolves a shadow when `fNdotL > 0.0` (or the receiver is
`GBUFFER_SHADING_SUBSURFACE`). **A broken G-buffer normal therefore silently
disables shadow RECEPTION**, and mode 9 comes back uniformly white with nothing
else about the frame obviously wrong.

That is not hypothetical: BC5 terrain normal maps decoded as three channels put
the terrain's shading normal underground, and every terrain pixel in the game
stopped receiving shadows. Mode 8 read `0.9/255` on ground that should read
`~191`. Meanwhile the CASTERS were all fine — the buildings' chimneys still shadowed
their own roofs, and meshes, foliage and grass all shadowed correctly, so every
caster-side check said yes. See `Docs/design/Photorealism.md` §1.10.

> **★ AND DO NOT DIAGNOSE THIS FROM SCREENSHOTS.** A shadow tucked behind its own
> caster looks identical to no shadow, so a pose where the sun is behind the camera
> "proves" an absence that is not there. Four separate crops read as "no shadow"
> before mode 8 settled it in one run. If you do need an A/B, the photo tours take
> `--phototour-shadows=ab` (whole system) and `--phototour-terrain-shadows=ab`
> (terrain caster only), which capture both arms **in one run** — two runs differ by
> wind phase alone at 0.9–1.3x the effect being measured. And read the sun's
> elevation from `Zenith_GetDefaultSunDirection()`, never from where the disc sits
> in frame; under camera pitch it reads far higher than it is.

## Caveats / TODO

- **Terrain casters are bounded by the cascade's caster extend** like everything
  else: a ridge further behind the cascade sphere than `dbg_fCasterExtendRadii`
  radii (toward the sun) is outside the ortho near plane and casts nothing into
  it. At a very low sun a distant mountain's shadow can therefore stop short —
  raise the extend, not the terrain code.
- Shadows are sun-only; dynamic (point/spot) lights are unshadowed.
- `GetFOV()` returns radians for the game camera but degrees for the editor camera
  while Stopped/Paused — `UpdateShadowMatrices` normalises defensively (any value
  > π is treated as degrees).
