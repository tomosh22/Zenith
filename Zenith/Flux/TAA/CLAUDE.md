# Flux TAA (Temporal Anti-Aliasing)

Neighborhood-clamped, disocclusion-rejecting temporal resolve over the composited HDR
scene, driven by per-pixel motion vectors and sub-pixel camera jitter. Registered as the
`Flux_TAAImpl` render feature **immediately before HDR**, so it resolves the fully-lit HDR
scene (after DeferredShading / Grass / Translucency / Fog / Particles) and **before** the
HDR bloom + tonemap, which read the resolved output through
`Flux_GraphicsImpl::GetSceneColourForPostFX`.

> **Status:** default **ON** — TAA is the shipping anti-aliaser (plan Stage 6 removed the inline FXAA
> stopgap from `Shaders/HDR/Flux_ToneMapping.slang` and flipped the `Enable` default to true). Temporal
> **upscaling** (render below output res, reconstruct at full) is **implemented** (plan Stage 5) — off
> by default; see [Temporal upscaling](#temporal-upscaling) below.

## Pipeline position

```
DeferredShading / Skybox / Fog / Particles / Translucency  ->  lit HDR scene (main view)
                                                                      |
                        TAA Resolve  (HDR + velocity + depth + history[prev]) -> resolved
                        TAA CopyToHistory (resolved -> persistent history, next frame)
                        TAA Sharpen  (resolved -> sharpened == GetSceneColourForPostFX)
                                                                      |
                                          HDR Bloom + Tonemap (reads the sharpened output)
```

**MAIN view only.** The material-preview and shadow-cascade views never jitter and never
write velocity, so they never get a TAA pass. With TAA on, the frame is exactly 3 passes
heavier (`Resolve` + `CopyToHistory` + `Sharpen`) — e.g. RenderTest smoke goes 111 -> 114.
If you ever see the pass count jump by more than 3, a TAA pass leaked onto a non-main view.

## Runtime control

- Debug vars under `Render/TAA/` (`Flux_TAA.cpp::Initialise`): `Enable` (the latch, see
  `Flux_Graphics.cpp`), `Blend Min/Max Alpha`, `Velocity Reject (px)`, `History Clamp Strength`,
  `Disocclusion Threshold`, `Sharpen Amount`. The tuning vars are **CB-only** (read into the
  pass constant buffers each frame, no graph rebuild); only `Enable` is structural.
- CLI `--taa=0` / `--taa=1` forces the latch at boot (scanned once in
  `Flux_Graphics.cpp::UpdateVelocityTargetSelection`) for capture/smoke harnesses. Pair with
  `--fixed-dt` for determinism and `--screenshot <path> --screenshot-frame N` to dump.

## The one latch

Everything TAA keys off **`FluxGraphics().IsVelocityMRTActive()`**. That single latched bool
(polled from the `Render/TAA/Enable` debug var + `--taa` override, one frame ahead in
`UpdateVelocityTargetSelection`, latched in `SetupTransients`) gates, in lockstep:
the velocity MRT transient, the jitter applied to the slot-0 GPU payload, the velocity /
prev-pose pipeline variants + their uploads/dispatches/graph edges, **and** the TAA passes
(via `Flux_TAAImpl::IsResolveActive()`, which mirrors the exact condition
`SetupRenderGraph` declares the passes under). Because the post-FX seam and the pass
declarations read the same latch, they can never disagree within a frame.

**Byte-identical-off invariant (load-bearing — do not break):** when the latch is off there
is **no** resolve pass, `GetSceneColourForPostFX` falls through to raw `GetHDRSceneTarget`,
no velocity transient is created, no velocity/prev buffers are grown/uploaded/read, no
jitter is applied, and no draw-constant bytes change. The off path (`--taa=0`) is the renderer
with **neither TAA nor FXAA** — as of Stage 6 the FXAA stopgap is gone, so `--taa=0` is the raw
no-AA baseline (it is byte-identical to the pre-Stage-5 renderer *minus* the removed FXAA, not to
the old FXAA'd default). Any new TAA code MUST stay behind this latch. (This is proven
**analytically**, not by pixel hash: the MaterialBattleTest TGA capture is non-deterministic
— SSAO/SSR/SSGI denoise noise — so two identical-build `--taa=0` runs differ. Gate on
pass-count + VK-clean + latch-gated buffer access, never a golden pixel compare.)

## Files

### This directory (`Flux/TAA/`)
- `Flux_TAA.cpp` / `Flux_TAAImpl.h` — the render feature. `Initialise` (pipelines + history
  target + res-change hook + debug vars), `SetupRenderGraph` (declares the 3 passes when
  `IsResolveActive`), `BuildPipelines`, `Shutdown`. Public pipeline/history members reached by
  the non-capturing graph callbacks via `g_xEngine.TAA()` (HiZ compute-feature convention).
- `Flux_TAA_Shaders.h` — the `Flux_ShaderDecl`s (`TAA_Resolve`, `TAA_CopyToHistory`,
  `TAA_Sharpen`) + `apxALL` for the catalog/registry/hot-reload.
- `Flux_TAA_ResolveCPU.h` (+ `.Tests.inl`) — **pure CPU mirror** of every resolve math step
  (YCoCg moments, variance clip, closest-depth velocity dilation, disocclusion, velocity-ramped
  Karis blend, RCAS sharpen, upscale kernel, `ComputeRenderDims`). The shader is a line-for-line
  transliteration of these, so ~headless tests pin the GPU path.
- `Flux_TAAJitter.h` (+ `.Tests.inl`) — Halton(2,3), the jitter-into-projection matrix, and the
  `Flux_ClipToUV` / velocity-encode CPU mirror (matches `Shaders/Common/Velocity.slang`).
- `Flux_VelocityHistory.h` (+ `.Tests.inl`) — `Flux_PrevTransformCache` (double-buffered
  per-entity previous model matrix, keyed on `m_ulEntityIDPacked`).

### Shaders (`Flux/Shaders/`)
- `TAA/Flux_TAA_Resolve.slang` / `_CopyToHistory.slang` / `_Sharpen.slang` — compute (8x8).
- `Common/Velocity.slang` — `GBufferVelocityOut` (the 5-MRT output = 4 core + velocity),
  `Flux_EncodeVelocityUV`, `MakeGBufferVelocity`.
- `UnifiedMesh/Flux_UnifiedMesh_ToGBufferVelocity.slang` — velocity variant of the unified G-buffer
  VS/FS (writes `MRT_INDEX_VELOCITY`; reprojects rigid/VAT through prev transform and skinned draws
  through the prev pose — see below).
- `UnifiedMesh/Flux_UnifiedMesh_SkinningPrev.slang` — positions-only second skinning dispatch
  (prev palette -> compact prev arena) for skeletal motion vectors.
- `Terrain/Flux_Terrain_ToGBufferVelocity.slang` — terrain velocity variant.

### Foundation living outside this dir (the motion-vector plumbing TAA consumes)
- `Flux_Enums.h` — `MRT_INDEX_VELOCITY` (5th MRT, `R16G16_SFLOAT`) + `uFLUX_MRT_CORE_COUNT` (4).
- `Flux_ViewConstants.h` — 624 B: adds `m_xViewProjMatNoJitter`, `m_xPrevViewProjMatNoJitter`,
  `m_xJitterUV_PrevJitterUV`. Jitter is added to the slot-0 GPU payload **only**; CPU
  `m_xFrameConstants` stays unjittered, so culling / CSM texel-snap / preview / terrain streaming
  are jitter-free by construction. **`Bindings.slang` must move in lockstep + FluxCompiler regen.**
- `Flux_Graphics.cpp` — the latch, the velocity transient (`SetupTransients`, main view), jitter
  application, and the `--taa` CLI scan. `GetSceneColourForPostFX(uViewSlot)` is the HDR seam.
- `UnifiedMesh/Flux_Skinning.h` — `Flux_BonePaletteHistory` (prev palette at the same bases as the
  current palette) + `Flux_SkinPrevPositionRaw` (CPU mirror of the prev-pose dispatch).

## Motion vectors (velocity MRT)

`velocity.xy = uvCurrent - uvPrev` in UV space, from the **unjittered** current/previous
view-proj matrices (so the vector is jitter-free at the source; history lookup = `uv - velocity`).
Written into the optional 5th MRT only inside the 5-attachment G-buffer pass, by the
`*_ToGBufferVelocity` pipeline variants selected at record time when the latch is on. The shared
4-target `GBufferOut` (Skybox / Primitives) is never touched.

Four deferred-motion sources, each captured exactly once the temporal resolve made them observable:
| Source | How prev world position is reconstructed |
|---|---|
| Camera + static | prev world == current world; motion is pure camera reprojection |
| Rigid / moving bodies, VAT foliage | `PrevTransforms[objectIndex] * currentLocalPos` (`Flux_PrevTransformCache`, index-locked to the GPU-scene objects) |
| **Skeletal pose (Stage 4.3b)** | prev pose from a **positions-only** second skinning dispatch using the **previous** bone palette, then `PrevTransforms * prevSkinnedLocalPos` |
| Terrain | two extra VS mat-muls on the world-space verts (`Flux_Terrain_ToGBufferVelocity`) |

### Skinned prev-pose indexing (⚠ read before touching it)

The prev arena is positions-only, **3 uint words / vertex**, index-locked to the main skinned
arena's out-vert bases. The velocity VS fetches a skinned vertex's prior pose at
**`(uOutVertBase + SV_VertexID) * 3`**, where `uOutVertBase` + a skinned flag arrive in the
per-draw constants (`g_uSkinnedPrevVertBase` / `g_uIsSkinned`, the repurposed pad words — only
written when the latch is on, so the off-path draw constants are unchanged).

`SV_VertexID` is the submesh-**LOCAL** vertex index, **not** a global arena index: the indirect
draw command's `vertexOffset` is hardcoded to 0 (`Flux_UnifiedMesh_Reset.slang` writes
`indirect[base+3]=0` for every command) and the arena slice base is applied via the
`SetVertexBuffer` **byte** offset (which does not affect `SV_VertexID`). So the global index must
be reconstructed as `uOutVertBase + SV_VertexID`. Do **not** fetch `PrevSkinnedPosArena[SV_VertexID]`
directly, and do **not** rely on an object "skinned" flag — there is no such GPUSceneObject flag bit
(only `OBJFLAG_VAT`). `Flux_BonePaletteHistory` runs **every frame** (CPU) so prev poses are ready
the instant the latch turns on; the GPU prev-pose dispatch + buffers engage only when the latch is on.

## Jitter

Halton(2,3), applied to the projection matrix of the **slot-0 GPU CB payload only** in the
per-view upload. The proj-jitter element indices are pinned by an NDC-shift unit test — do not
trust them blind (`Zenith_Maths::PerspectiveProjection` is `perspectiveLH_ZO`, `M[2][3]==1`; jitter
injects into `[2][0]`/`[2][1]`). Cascade / preview payloads get jitter = 0 (separate view slots).

## Temporal upscaling

Render the scene chain **below** output resolution and let the resolve reconstruct at full output
res. Off by default (`Render/TAA/Upscaling` = false, `Render/TAA/Render Scale` = 1.0; CLI
`--taa-upscaling=1 --taa-render-scale=<0.5..1.0>`). **Gated on the velocity latch** — upscaling can
never engage while TAA is off (there is no resolve pass to reconstruct output res, so the scene
must render at output res).

**Render vs output dims.** `Flux_GraphicsImpl::GetOutputDims()` is the swapchain resolution;
`GetRenderDims()` is `round_to_even(RenderScale × output)` (`Flux_TAAComputeRenderDims`), latched
per graph build in `SetupTransients` (`m_xRenderDimsThisBuild`) so every per-frame consumer agrees
with the live transient sizes. **`GetRenderDims() == GetOutputDims()` whenever upscaling is off**
(returned verbatim, never routed through the quantiser) — that is the byte-identical-off contract.
`Upscaling` / `RenderScale` are **structural**: a change resizes the slot-0 transients, so
`UpdateVelocityTargetSelection` requests a full graph rebuild (the render-dims comparison is gated
on the next upscaling state, so an off-path change or a sub-even-pixel wiggle never thrashes).

**RENDER res** (the slot-0 scene chain): the main-view G-buffer / velocity / depth / HDR-scene
transients, HiZ, SSAO, SSR, SSGI, Decals, the HDR **histogram** (it reads the raw render-res HDR),
and the main VIEW CB `g_xScreenDims` / `g_xRcpScreenDims`. Everything else in the scene chain
(DeferredShading, Fog, Skybox, Translucency, Particles, Primitives, DynamicLights clustering, IBL)
auto-tracks — it rasterises a fullscreen pass over the render-res RTV or reads the VIEW CB screen
dims — **zero edits**.

**OUTPUT res:** the TAA history / resolved / sharpened outputs, the HDR **bloom + tonemap** (they
read the resolved output, which is output res → bloom after upscale), the FinalRT, **and the
UI/text overlay** (Quads/Text) which draw onto the output-res FinalRT after the upscale. Because the
main VIEW CB screen dims are now render res, quads/text read a separate `g_xView.g_xRcpOutputDims`
(a repurposed spine pad @472 — **zero sizeof change**, stays 624 B) for their pixel→NDC mapping. For
a non-upscaled view (preview/cascade) `g_xRcpOutputDims == g_xRcpScreenDims`.

**Resolve reconstruction.** The resolve dispatches + writes at output res but SAMPLES the render-res
HDR/velocity/depth (`g_uRenderWidth`/`g_uRenderHeight` in the resolve CB — the former pad slots, so
the CB stays 48 B). The 3×3 neighbourhood steps by `1/renderDim`; velocity-reject stays **output**
pixels (the knob is output-relative). The current-frame colour is a Gaussian scatter-as-gather over
the render texels (`Flux_TAAUpscaleReconstruct` CPU mirror), **guarded** so at `renderDim ==
outputDim` it collapses to today's single centre tap → the scale-1 path is byte-identical. History
accumulates full-res detail across jittered frames (the jitter is ±0.5 of a **render** pixel). The
resolve binds the render-res sources (HDR/velocity/depth) with the **clamp** sampler, because the
gather and the 3×3 neighbourhood step outside `[0,1]` at screen edges — the default (repeat) sampler
would wrap the opposite edge in. History is **also** clamp-sampled: its in-bounds guard is centre-only
and inclusive, so a reprojected UV within half a texel of an edge would still let the bilinear footprint
wrap the opposite edge into both the resolved colour and the disocclusion depth.

**Toggle coherence.** `Upscaling`/`RenderScale` are polled *after* `UploadFrameConstants` in the frame,
and consumed (via the deferred rebuild) at the *top* of the next frame's `ExecuteRenderGraph` — before
that frame's `SetupTransients` latch. So on the toggle frame the slot-0 VIEW CB would lag the freshly-
resized transients by one build. `UploadFrameConstants` therefore stages its screen dims + jitter from
`GetPendingRenderDims()` (the *requested* state = what `SetupTransients` is about to latch), while every
in-`Execute` consumer (the resolve CB, HiZ/SSAO/SSR setup) uses the latched `GetRenderDims()`. The two
are equal on every steady frame; they diverge for exactly the one rebuild frame, and using pending for
the CB keeps it dims-coherent with the transients that frame.

## History model (why a copy pass, not a ping-pong)

The render graph's Reads/Writes are fixed per build, so it **cannot** ping-pong two history images
by frame parity. Instead: one **persistent, feature-owned** `m_xHistory` (RGBA16F, `alpha = linear
depth` for disocclusion), the resolve writes a **transient** output, and `TAA_CopyToHistory` copies
that into the persistent history for next frame. History is invalidated (`blend = 1`, current-frame
verbatim) on Initialise / graph rebuild / resize; `m_bHistoryValid` is snapshotted on the main
thread in the resolve's `Prepare` so the worker record reads an immutable bool.

> **Cross-frame preservation now works on tiled GPUs too.** The render graph's cyclic-barrier seed
> was extended from buffers to persistent images (`SeedCyclicImageState` in
> `Flux_RenderGraph_Compilation.cpp`): a **write-last** persistent image (the history ends each frame
> written via UAV) is seeded so its first touch next frame carries the real prior-frame access as the
> barrier source (not a `UNDEFINED` first-touch that discards on tiled GPUs), and it is **primed** once
> per (re)creation via `Flux_MemoryManager::TransitionImageInitialLayout` to that resident layout so the
> seeded first-touch barrier is layout-valid from frame 0 (a fresh target is created in `SHADER_READ`,
> only mip 0 transitioned). **Read-last** persistent images (the IBL LUTs, Preview-LDR) are deliberately
> **un-seeded** back to the per-frame `UNDEFINED` first touch — their mip>0 subresources are `UNDEFINED`
> at creation, so collapsing that first touch would sample an untransitioned subresource. Their own
> cross-frame preservation therefore remains the desktop-only `UNDEFINED -> SHADER_READ` no-op discard
> (unchanged; baked once, so it is not new UB).

## Resolve (`Flux_TAA_Resolve.slang`)

3x3 closest-depth velocity dilation -> reproject history at `uv - velocity` -> YCoCg variance clip
(`mean +/- gamma*sigma`, gamma = `History Clamp Strength`) -> disocclusion rejection (history-alpha
prev depth vs reprojected current depth, threshold = `Disocclusion Threshold`) -> velocity-ramped
blend `alpha = lerp(Blend Min, Blend Max, velPx / Velocity Reject)` + Karis luminance weighting.
Writes `rgb = resolved`, `a = current linear view depth`. `Sharpen` is RCAS-style 5-tap
(min/max-limited, `Sharpen Amount = 0` is identity).

## Testing seams

Per the extensive-unit-testing mandate, all shader math has a pure CPU mirror in a header, tested
headlessly (tests hosted in the always-linked `Flux_MaterialTable.cpp` per the dead-strip idiom):
`Flux_TAA_ResolveCPU.Tests.inl`, `Flux_TAAJitter.Tests.inl`, `Flux_VelocityHistory.Tests.inl`, and
the skinning prev-pose tests in `UnifiedMesh/Flux_Skinning.Tests.inl`. Graph-declaration logic is
static free functions taking a caller-supplied `Flux_RenderGraph&`, so headless tests can build a
stack graph and assert pass/edge structure without booting Flux.

## Gotchas

- **New program decl => rebuild `fluxcompiler.exe` first, THEN run it** (the shader catalog is
  compiled into the exe). A shader **body** edit (no new decl) => just re-run the existing exe.
  Editing `Bindings.slang` alone => the existing exe recompiles it at runtime (no rebuild).
- **`GetMRTAttachment` asserts `eIndex < uFLUX_MRT_CORE_COUNT`** — velocity access goes through the
  distinct `GetVelocityAttachment()`, never `GetMRTAttachment(MRT_INDEX_VELOCITY)`.
- The `[Core] Check failed: pass 'Shadow Cascade N' reads '<buffer>' ...` and `'TAA Resolve' reads
  'TAA History' ...` lines are **pre-existing** (the cull-buffer cross-frame cyclic-barrier deferral
  and the persistent-history read-after-write). They scale with the number of graph compiles per run
  (velocity-on triggers extra rebuilds), so counts differ between `--taa=0` and `--taa=1` runs — not
  a regression.
- RenderTest's default camera faces the sky/horizon, so **edge-AA / de-ghosting is not visually
  judgeable there** — use a geometry-facing camera for silhouette checks.

## Not yet implemented (plan Stages 5-6)

- **Vendor upscaler seam** (`Flux_TemporalResolverDesc` with DLSS/FSR2/XeSS stubs) — designed for;
  the resolve is isolated behind `IsResolveActive()` + the post-FX seam so a vendor backend can slot
  in later, but only the **Custom** compute resolve exists today.
