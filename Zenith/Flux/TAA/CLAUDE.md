# Flux TAA (Temporal Anti-Aliasing)

Neighborhood-clamped, disocclusion-rejecting temporal resolve over the composited HDR
scene, driven by per-pixel motion vectors and sub-pixel camera jitter. Registered as the
`Flux_TAAImpl` render feature **immediately before HDR**, so it resolves the fully-lit HDR
scene (after DeferredShading / Translucency / Fog / Particles) and **before** the
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
write velocity, so they never get a TAA pass. With TAA on, the frame is exactly **4 passes
heavier** — the 3 TAA passes (`Resolve` + `CopyToHistory` + `Sharpen`) plus **`Skybox
Velocity`**, the sky's motion-vector write (see *Sky velocity* below). The DELTA is the
invariant, not the absolute count — the RenderTest smoke scene measured 166 -> 170 on
2026-08-10 and its play scene 172 -> 176, and both numbers move with unrelated content.
If you ever see the pass count jump by more than 4, a TAA or velocity pass leaked
onto a non-main view — or someone added a pass where a pipeline VARIANT belonged. Every other
velocity writer (UnifiedMesh / Terrain / Grass / Primitives) is a 5-attachment pipeline variant
of a pass that exists either way, and adds nothing to the count. `TAAToggleStress` pins this as
`iTAA_LATCHED_PASS_DELTA`.

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
- `Flux_TAAJitter.h` (+ `.Tests.inl`) — Halton(2,3), the jitter-into-projection matrix, the
  `Flux_ClipToUV` / velocity-encode CPU mirror (matches `Shaders/Common/Velocity.slang`), and
  `Flux_SkyVelocityUV` — the w = 0 point-at-infinity mirror of `Shaders/Skybox/Flux_SkyboxVelocity.slang`.
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
- `Primitives/Flux_Primitives_ToGBufferVelocity.slang` — primitives velocity variant (camera-only).
- `Skybox/Flux_SkyboxVelocity.slang` — the sky's velocity-ONLY fullscreen pass (w = 0 view ray).
  The only motion-vector writer that is a pass rather than a pipeline variant.

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
Written into the optional 5th MRT by the `*_ToGBufferVelocity` pipeline variants selected at
record time when the latch is on, inside the 5-attachment G-buffer pass — plus one
velocity-ONLY pass for the sky (below). The shared 4-target `GBufferOut` is never touched.

Motion sources, each captured once the temporal resolve made it observable:
| Source | How prev world position is reconstructed |
|---|---|
| Camera + static | prev world == current world; motion is pure camera reprojection |
| Rigid / moving bodies, VAT foliage | `PrevTransforms[objectIndex] * currentLocalPos` (`Flux_PrevTransformCache`, index-locked to the GPU-scene objects) |
| **Skeletal pose (Stage 4.3b)** | prev pose from a **positions-only** second skinning dispatch using the **previous** bone palette, then `PrevTransforms * prevSkinnedLocalPos` |
| Terrain | two extra VS mat-muls on the world-space verts (`Flux_Terrain_ToGBufferVelocity`) |
| Grass blades | the blade pose **rebuilt** against the previous frame's wind block (`GrassPrevWindConstants`, the only prev-frame state the grass VS reads) — a blade has no cached transform, so `Flux_Grass_ToGBufferVelocity` re-runs the shared pose builder rather than reprojecting a static position. Correct only because a blade keeps its identity across frames: it is regenerated every frame as a pure function of its lattice node, so last frame's record described the same blade |
| **Sky** | no world position at all — the VIEW RAY through both unjittered view-projections with **w = 0** (`Flux_SkyboxVelocity.slang`). See *Sky velocity* below |
| **Primitives** | prev world == current world, i.e. camera-only, same as terrain (`Flux_Primitives_ToGBufferVelocity`). ⚠ A primitive that MOVES gets camera-only motion — see the limitation note below |

### Sky velocity — one pass, w = 0, and why it is nearly unobservable

The sky has **three** G-buffer programs (`SkyboxSolidColour` / `SkyboxCubemap` /
`SkyboxAtmosphere`) and **one** motion vector, because sky velocity is a pure function of the
view ray: a point at infinity moves on screen for exactly one reason and by exactly one amount,
whatever painted it. So there is no velocity variant of the three shading programs — there is one
extra velocity-ONLY fullscreen pass, `Skybox Velocity`, declared in `Flux_SkyboxImpl::SetupRenderGraph`
right after the sky draw (and therefore after every geometry velocity writer, since Skybox is
registered after UnifiedMesh / Grass / Terrain / Primitives in `Flux_FeatureRegistry.cpp`).
Promoting the sky draw instead would have needed three variants, two of which would re-run work
(the atmosphere raymarch) the motion vector does not depend on.

- **`w = 0` is the physics, not a shortcut.** It annihilates the view matrix's translation
  column, so camera TRANSLATION contributes nothing — correct, because a point at infinity does
  not parallax. **Sky velocity is identically zero on a dollying camera and non-zero only under
  rotation**, which is why a translation-only capture can never reveal a missing one. Pinned by
  the `TAASkyVelocity` cases in `Flux_TAAJitter.Tests.inl` against the CPU mirror
  `Flux_SkyVelocityUV`.
- **Sky pixels are selected by the DEPTH TEST**, not a depth-texture read + `discard`: the
  fullscreen triangle rasterises at NDC z = 1.0 with LESSEQUAL and depth write off, so it survives
  only where the stored depth is still the far-cleared 1.0 — the same rule that decided which
  pixels the sky painted, rather than a second copy of it that could drift.
- A direction **behind the previous camera** (`clipPrev.w <= 0`) would fold onto a
  plausible-looking UV through the perspective divide. It writes a sentinel velocity of 2.0
  instead, so `uv - velocity` leaves [0,1] and the resolve keeps the current frame.

> **★ AND IT IS ALMOST IMPOSSIBLE TO SEE, FOR A STRUCTURAL REASON — DO NOT GO LOOKING FOR A
> BIGGER EFFECT.** The resolve clips history into the current 3x3 neighbourhood's colour range,
> so however wrong a motion vector is, the resulting error is bounded by that neighbourhood's
> sigma. Both of this engine's skies are smooth gradients — the cubemap band measures a local 3x3
> sigma of **~0.6 of an 8-bit level** — so the bound is under one LSB. Suppressing sky velocity
> entirely (`--taa-no-skyvelocity`) moves `TAATemporalStability`'s rotating-camera lag metric by
> **0.03%** on the atmosphere sky and **2.7%** on the cubemap, against ~0.05% run-to-run noise.
> The fix is correct and it is cheap; it is simply not a pixel-gateable effect on this content.
> It is gated where the evidence is unambiguous instead: `TAAToggleStress` asserts the pass exists
> iff the latch is on and renders the main view, and the unit tests pin the maths. **A sky with
> real high-frequency detail — clouds, a sun disk in frame — is where this would become visible.**

### Primitives: camera-only motion, and why moving primitives are not fixed here

`Flux_Primitives_ToGBufferVelocity` reprojects each primitive's CURRENT world position through
both unjittered view-projections — pure camera motion, exactly as terrain does. A primitive that
moves between frames therefore gets its background's motion rather than its own.

That is a limitation, not an oversight: a primitive is submitted **by value** every frame (a
centre + extents, or a start/end pair) with no identity the renderer can key on, so there is no
`Flux_PrevTransformCache` entry to reproject through — nothing knows that this frame's sphere is
last frame's sphere moved. Camera-only is strictly better than the (0,0) it used to leave behind,
and exactly right for the overwhelming majority of primitive draws, which are static world
annotations. Fixing it properly means giving the submit API a caller-supplied identity, which is
a change to every `Add*` / `SubmitGameplay*` call site — do that deliberately, not as a side
effect of a TAA change.

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
would wrap the opposite edge in. History is **also** clamped: its in-bounds guard is centre-only
and inclusive, so a reprojected UV within half a texel of an edge would still let the bilinear footprint
wrap the opposite edge into the resolved colour. (Depth, velocity and the history's depth channel take
the **point** clamp sampler — same clamp behaviour, no filtering; see the sampler note under Resolve.)

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
verbatim) on Initialise / resize / the first build after the resolve was absent — **not** on every
graph rebuild, which is what it used to do. A rebuild neither destroys nor moves the persistent
history and the cyclic seed is re-derived from the rebuilt graph, so the content survives; blanking
it anyway cost one fully-aliased jittered frame per rebuild, and terrain streaming + instance-group
growth request rebuilds constantly *while the camera moves*. `m_bHistoryValid` is snapshotted on the main
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
(`mean +/- gamma*sigma`, gamma = `History Clamp Strength`) -> disocclusion rejection (below) ->
velocity-ramped blend `alpha = lerp(Blend Min, Blend Max, velPx / Velocity Reject)` + Karis
luminance weighting. Writes `rgb = resolved`, `a = current linear view depth`. `Sharpen` is
RCAS-style 5-tap (min/max-limited, `Sharpen Amount = 0` is identity).

### The disocclusion test — two things it is easy to get wrong

Both were wrong in the shipped resolve and both made the image flicker; between them they
forced `Disocclusion Threshold` to ~0.9 (which is the test switched OFF) before the picture
would sit still. `TAATemporalStability` (`Games/RenderTest/Tests/`) is the gate that measures it.

1. **The expected previous depth is a REPROJECTION, not the current depth.** Push the pixel's
   world position through `GetPrevViewProjMatNoJitter()` and take `.w` — for this projection
   (`perspectiveLH_ZO`, `M[2][3] == 1`) clip.w *is* view-space z. Comparing the history's
   stored depth against *this* frame's depth instead asserts that nothing ever changes
   distance from the camera, so any dolly rejects everything close to the near plane.
2. **Compare against the neighbourhood depth RANGE, not a point.** The depth buffer is
   rasterised through the jitter, so a pixel samples a different sub-pixel position every
   frame: its depth wanders a little on a slope and a lot at a silhouette, where it alternates
   between the near and far surface. A point comparison cannot tell that from a disocclusion,
   and rejecting it destroys history at exactly the edges TAA exists to anti-alias — an
   anti-aliased edge pixel's history *is* a blend of both surfaces. The 3x3 min/max range
   (which the dilation loop already computes) measures the local depth spread instead of
   estimating it, and subsumes any slope-tolerance heuristic.

Measured on the RenderTest campus, still camera, mean inter-frame delta vs the same scene with
TAA off: **4.96x** less stable than no-TAA as shipped, **2.97x** with only (1) fixed, **1.20x**
with both — against a **1.15x** floor with rejection disabled entirely.

**Sampler choice is load-bearing, not incidental.** Only colour may be filtered. HDR and the
history's rgb are LINEAR; depth, velocity and the history's *depth* are POINT. Interpolating a
depth or a motion vector across a silhouette yields a value belonging to neither surface — a
blended depth breaks the test above, and a blended motion vector defeats the closest-depth
dilation that just picked the foreground's velocity. This is why the history image is bound
**twice** (`g_xHistoryTex` linear, `g_xHistoryDepthTex` point): a combined image sampler bakes
the filter in. At render == output the taps land on texel centres, where point and linear agree,
so the non-upscaled path is unchanged.

## Testing seams

Per the extensive-unit-testing mandate, all shader math has a pure CPU mirror in a header, tested
headlessly (tests hosted in the always-linked `Flux_MaterialTable.cpp` per the dead-strip idiom):
`Flux_TAA_ResolveCPU.Tests.inl`, `Flux_TAAJitter.Tests.inl`, `Flux_VelocityHistory.Tests.inl`, and
the skinning prev-pose tests in `UnifiedMesh/Flux_Skinning.Tests.inl`. Graph-declaration logic is
static free functions taking a caller-supplied `Flux_RenderGraph&`, so headless tests can build a
stack graph and assert pass/edge structure without booting Flux.

**The two windowed gates** (`Games/RenderTest/Tests/`, both `requiresGraphics`):

| Gate | What it can see | What it CANNOT see |
|---|---|---|
| `TAATemporalStability` | **Phase 1 (still camera)** — flicker: TAA must not make a still image less stable than no TAA (~1.19x off, budget 1.5x). **Phase 2 (rotating camera)** — LAG: at MATCHED camera poses, TAA's output must not be displaced from the un-resolved image by more than one frame of camera motion (~0.47x, budget 1.0x). Phase 2 is the gross-regression gate on the motion vectors as a whole. | Phase 1 cannot see ANY missing motion vector (a still camera's correct vector is (0,0), which is what an absent one reads as). Phase 2 cannot isolate one smooth surface — see the variance-clip note under *Sky velocity*. Neither can see a stability regression via phase 2's ratio, which moves the WRONG WAY on a dropped vector (a dropped vector over-smooths). |
| `TAAToggleStress` | The graph SHAPE across 10 on/off flips: 3 TAA passes iff on, `Skybox Velocity` present iff on, all main-view, `IsResolveActive` in agreement, a stable per-state pass count with `on == off + 4`, and no VRAM leak. This is where sky velocity is actually gated — pass presence is exactly true or false where a pixel measurement is sub-LSB. | Anything about the CONTENT of a motion vector. That is the unit tests' job. |

## Gotchas

- **New program decl => rebuild `fluxcompiler.exe` first, THEN run it** (the shader catalog is
  compiled into the exe). A shader **body** edit (no new decl) => just re-run the existing exe.
  Editing `Bindings.slang` alone => the existing exe recompiles it at runtime (no rebuild).
- **`GetMRTAttachment` asserts `eIndex < uFLUX_MRT_CORE_COUNT`** — velocity access goes through the
  distinct `GetVelocityAttachment()`, never `GetMRTAttachment(MRT_INDEX_VELOCITY)`.
- **The producer-before-consumer check is CLEAN — it firing at all is a regression.** It used to log
  `pass 'Shadow Cascade N' reads '<buffer>' ...` and `'TAA Resolve' reads 'TAA History' ...` at every
  boot; both are fixed and must stay at zero:
  - The cascade lines were a **real dropped edge**, not a benign cross-frame deferral: `Shadows` was
    registered before `UnifiedMesh`, so the cascades were mutually UNORDERED with the passes producing
    their cull args and one cascade drew from just-zeroed indirect args. Fixed by declaring
    `UnifiedMesh` first (`Flux_FeatureRegistry.cpp`), gated by
    `Core::FeatureRegistryUnifiedMeshPrecedesShadows`.
  - The `TAA History` line was a genuine false positive — the resolve *wants* last frame's content.
    It now declares that with `.ReadsPrevFrame(...)` instead of `.Reads(...)`, which exempts that one
    usage. Reach for `ReadsPrevFrame`/`ReadBufferPrevFrame` ONLY for real temporal reads; using it to
    quiet a mis-ordered same-frame producer re-hides the shadow bug.
- RenderTest's default camera faces the sky/horizon, so **edge-AA / de-ghosting is not visually
  judgeable there** — use a geometry-facing camera for silhouette checks.
- **A/B capture toggles** (verification-only, default off, scanned once, byte-identical when unset):
  `--taa-no-prevpose` reverts skinned draws to camera+model-only velocity (isolates the prev-pose
  vector); `--taa-no-skyvelocity` suppresses the `Skybox Velocity` pass (restores the pre-fix
  "sky pinned to the screen" behaviour). Both exist because these vectors are proven by A/B, not
  by looking at one frame.

## Translucency and Particles deliberately write NO velocity

This looks like the obvious next gap and it is not one. Read this before "finishing the job".
The same decision is recorded as a `TODO(taa-translucent-velocity)` at both pass declarations —
`Flux/Translucency/Flux_Translucency.cpp::SetupRenderGraph` (the shared argument, plus how to
build it if the conditions below are ever met) and `Flux/Particles/Flux_Particles.cpp::SetupRenderGraph`
(the two particle-specific findings).

**The premise is wrong for these two passes.** Unlike the sky and primitives, Translucency and
Particles do *not* leave the velocity target at its (0,0) clear. They composite into the HDR scene
AFTER lighting and neither writes depth, so the velocity at their pixels already holds the OPAQUE
surface behind them — a real, coherent motion vector. TAA is already reprojecting them by
something correct; it just isn't *theirs*.

Three reasons giving them their own vector is not an improvement:

1. **It swaps which half is wrong.** A composited pixel is `a*layer + (1-a)*background`: two
   motions, one `R16G16` slot. Using the background's vector is right for the `(1-a)` fraction;
   using the layer's is right for the `a` fraction. An alpha threshold ("write velocity only where
   the layer owns the pixel") is the standard answer — but every translucent material in the tree
   sits at alpha **0.4–0.6** (`RenderTest_MaterialShowcase.cpp`: `Showcase_TransCyan` 0.4,
   `Showcase_TransOrange` 0.55, `Showcase_Additive` 0.6), i.e. precisely the band such a threshold
   must EXCLUDE. The change would fire on nothing that ships.
2. **It breaks velocity↔depth coherence, which the resolve depends on.** The disocclusion test
   compares the history's stored linear depth — fetched at `uv - velocity` — against a range
   derived from the DEPTH buffer at the current pixel. Because neither pass writes depth, displacing
   that fetch by a non-depth-writing layer's motion makes the two describe *different surfaces*, so
   history gets REJECTED rather than reprojected. Writing particle velocity therefore amounts to
   "reject history at particle pixels" by a much more expensive route.
3. **An alpha-blended sprite cloud has no well-defined per-pixel motion vector at all.** N
   overlapping semi-transparent billboards plus a background contribute N+1 motions to one colour,
   and the engine's sprite (`particleSwirl`) is a soft radial falloff whose above-threshold alpha is
   a small core with many low-alpha layers around it.

**The prerequisite the design worried about is NOT a blocker** — `Flux_PipelineSpecification`
already supports **per-attachment** blend state: `m_axBlendStates[FLUX_MAX_TARGETS]` is indexed per
attachment by `Zenith_Vulkan_Pipeline.cpp::BuildColorBlendState`, including `m_uColorWriteMask`, and
`Flux/Decals/Flux_Decals.cpp` already ships a per-attachment write mask. (Null and D3D12 consume no
blend state at all.) So an opaque velocity attachment beside an alpha-blended colour attachment is
buildable today; it is the *semantics*, not the plumbing, that says no.

If this is ever revisited: the GPU particle path can produce `prevPos = pos - vel*dt` in one line of
`Flux_ParticleUpdate.slang` (the record already carries velocity), but carrying it to the draw costs
a **20 B → 32 B** per-instance stream, which moves `uFLUX_PARTICLE_INSTANCE_WORDS`, the generated
`Particles` vertex layout, the compute writer's `uINSTANCE_WORDS` literal and six `static_assert`s in
`Flux_ParticleData.h`. The cheaper honest option for particle ghosting is to reject history where
particles drew, not to invent a motion vector for them.

## Not yet implemented (plan Stages 5-6)

- **Vendor upscaler seam** (`Flux_TemporalResolverDesc` with DLSS/FSR2/XeSS stubs) — designed for;
  the resolve is isolated behind `IsResolveActive()` + the post-FX seam so a vendor backend can slot
  in later, but only the **Custom** compute resolve exists today.
