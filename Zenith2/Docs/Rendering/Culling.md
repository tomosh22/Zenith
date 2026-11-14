# Zenith2 rendering: hierarchical-Z occlusion culling

Section file of `Zenith2/Docs/Rendering.md`. It details the entry-file section "Hierarchical-Z occlusion culling". Normative statements share the `REND` tag family of the entry file. Code lives in `Zenith2/Engine/ZenithFlux/Internal/Culling/` and `Zenith2/Shaders/Passes/Culling/`, `Zenith2/Shaders/Passes/HiZ/`.

Two-phase HiZ occlusion culling is present in every shading path and runs for every main, editor and texture view (and for capture views, whose feature mask enables it). It sits inside the shared GPU-scene pipeline after frustum culling, so it serves all four paths (Z2-D-060).

<!-- informative -->
Zenith built a per-view HiZ chain with one pass per mip and used it for SSR and SSGI; occlusion culling was listed as a plausible future consumer. Zenith2 makes culling the primary consumer, stores linear depth so no depth mode changes the arithmetic, builds the whole chain in one dispatch, and keeps the min channel for SSR.
<!-- /informative -->

## Pyramid format

| Property | Value |
|---|---|
| Format | RG16F: R = max (farthest) linear view depth, G = min (nearest) linear view depth |
| Mip 0 extent | the view's render extent (the depth buffer's extent) |
| Mip n+1 extent | `max(1, floor(extent_n / 2))` per axis |
| Mip count | `floor(log2(max(width, height))) + 1`, at most 13 (render extents are at most 4,096 per axis) |
| Units | metres of view-space distance along the view's forward axis |
| Far-plane, cleared texels, and depths ≥ 65,504 m | +∞ (`0x7C00`) in both channels |
| Ownership | the final pyramid of each frame is the view's persistent HiZ (`Flux_ViewPersistentState::m_axHiZ`, ping-pong) |

- [Z2-N-REND-635] The HiZ pyramid stores linear view-space depth in RG16F, never raw device depth: the max channel drives occlusion culling and the min channel drives SSR's hierarchical tracing, and both stay within the 16-bit render-target rule.
- [Z2-N-REND-636] Linear depth increases away from the camera in every depth mode, so the pyramid and the occlusion test contain no min/max inversion and no branch on depth mode anywhere outside mip-0 conversion.
- [Z2-N-REND-637] Far-plane or cleared texels, texels whose device or linear depth is NaN, and any linear depth at or above 65,504 m store +∞ in both channels, so a NaN never enters the max/min reduction; +∞ culls nothing, which is conservative and covers infinite-far reversed-Z projections.

## Pyramid construction

### Mip 0 conversion

| Depth mode | Device depth z | Linear depth d |
|---|---|---|
| standard Z, finite far | `z = f(d − n) / (d(f − n))` | `d = n·f / (f − z(f − n))` |
| reversed Z, finite far | `z = n(f − d) / (d(f − n))` | `d = n·f / (n + z(f − n))` |
| reversed Z, infinite far | `z = n / d` | `d = n / z` |
| orthographic (every orthographic view is standard Z) | `z = (d − n) / (f − n)` | `d = n + z(f − n)` |
| cleared value (1.0 standard, 0.0 reversed), or NaN | — | +∞ |

### Conservative rounding

```
// f32 linear depth -> f16, conservative per channel (Zenith2/Shaders/Passes/HiZ/HiZ.slang)
uint RoundUpToHalf(float fDepth)      // max channel: towards farther
{
	if (isnan(fDepth) || fDepth >= 65504.0 || isinf(fDepth)) return 0x7C00;   // NaN and far depths: +inf
	uint uHalf = Flux_F32ToF16Bits(fDepth);                           // round to nearest even, integer-only codec
	if (Flux_F16BitsToF32(uHalf) < fDepth) uHalf += 1;               // next representable towards +inf (positive values)
	return uHalf;
}
uint RoundDownToHalf(float fDepth)    // min channel: towards nearer
{
	if (isnan(fDepth) || fDepth >= 65504.0 || isinf(fDepth)) return 0x7C00;   // the +inf rule applies to both channels
	uint uHalf = Flux_F32ToF16Bits(fDepth);
	if (Flux_F16BitsToF32(uHalf) > fDepth && uHalf > 0) uHalf -= 1;  // next representable towards 0
	return uHalf;
}
```

- [Z2-N-REND-638] Mip 0 converts device depth to linear depth once, per the view's depth mode, with the formulas of the conversion table, and rounds the max channel up and the min channel down to half precision with the functions above; every coarser mip is a max (and min) of already-rounded values, which is exact, so nothing ever rounds the max channel towards nearer.
- [Z2-N-REND-639] Each texel of mip n+1 stores the max of the max channel and the min of the min channel over its footprint in mip n; the footprint is 2×2, except that when mip n has an odd width (height) the last column (row) of mip n+1 also folds in the extra column (row) of mip n, giving 3×2, 2×3 or 3×3 footprints at the edges, so the pyramid stays conservative.

<!-- informative -->
Half-float relative precision is about 0.1% at every distance, roughly 0.25 m steps at 500 m. Raw D24 values quantised to 16 bits would give steps of tens of metres at the same distance. Rounding the max channel up costs a little culling efficiency and never correctness.
<!-- /informative -->

### Single-pass downsampler

| Step | Action |
|---|---|
| 1 | One compute dispatch; 256 threads per group; each group owns a 64×64 tile of mip 0. |
| 2 | Each thread reads its depth texels, converts them (mip 0 conversion), writes mip 0, and reduces in registers and groupshared memory to mips 1–6 of its tile, writing each. |
| 3 | Each group increments a global atomic counter after writing mip 6; the last group to finish reads every tile's mip-6 texel and reduces mips 7 to the last mip. |
| 4 | The counter buffer (4 bytes) is a transient cleared by the graph before the dispatch, satisfying the first-write rule. |

- [Z2-N-REND-640] The pyramid is built by one compute dispatch of a single-pass downsampler (SPD style) as the table above specifies, declared with a `[FullCoverage]` UAV per mip and a cleared counter, and no per-mip pass exists.

## Occlusion test

| Step | Action |
|---|---|
| 1 | Project the candidate's bounds (the world bounding box from the primitive's local AABB and the instance transform, or the bounding sphere with `m_fMaxScale`) to a screen-space rectangle in mip-0 pixels, and compute its nearest linear depth (the minimum view-space depth over the box corners, or centre depth minus radius for a sphere). |
| 2 | If the bounds cross the near plane (nearest depth ≤ n) or the projection is degenerate (any corner with w ≤ 0, a NaN, or an empty rectangle after clamping), keep the candidate. |
| 3 | Choose the mip `L = max(0, ceil(log2(max(width_px, height_px))))` so that the rectangle covers at most 2×2 texels of mip L. |
| 4 | Sample the max channel of the up to four mip-L texels `(floor(min_x / 2^L) … floor(max_x / 2^L)) × (floor(min_y / 2^L) … floor(max_y / 2^L))`, each coordinate clamped to the mip's last texel (which, by folding, covers any extra edge row or column). |
| 5 | Cull the candidate if its nearest depth is farther than the largest of the sampled max values; otherwise keep it. |

- [Z2-N-REND-641] The occlusion test is exactly the five steps above: a candidate is culled when its nearest linear depth is farther than the pyramid's farthest depth over its footprint, which guarantees it is hidden behind geometry already drawn, and candidates crossing the near plane or with a degenerate projection are always kept.
- [Z2-N-REND-642] The projection and mip-selection mathematics exist once, in the module `Zenith2/Shaders/Common/HiZTest.slang` (a module, because the instance-cull, meshlet-cull, transparent-cull and HiZ debug overlay programs all import it and [Z2-N-REND-105] forbids a program importing another), and a bit-matching CPU reference in `Zenith2/Engine/ZenithFlux/Tests/Culling/` drives the analytic unit tests.

## Two-phase culling

| Phase | Candidates | Frustum | HiZ test | Output |
|---|---|---|---|---|
| 1 | instances visible last frame (the view's previous visibility bits) | against the current camera | current bounds projected with the previous frame's view-projection, nearest depth linearised in the previous camera's view space, against last frame's pyramid, unmodified | indirect draws of the survivors, drawn to depth (or depth plus visibility) |
| build | — | — | — | this frame's pyramid from that partial depth |
| 2 | every instance not drawn in phase 1: phase 1's rejects plus every instance invisible last frame | against the current camera | current bounds with the current view-projection against the new pyramid | indirect draws of the survivors, drawn into the same targets |
| final | — | — | — | the final pyramid, from the complete depth, kept as the view's persistent HiZ |

- [Z2-N-REND-643] Culling runs in two phases so occlusion never lags a frame: phase 1 tests last frame's visible instances with current bounds and the previous view-projection against last frame's unmodified pyramid (the conservative formulation of Nanite and Haar/Aaltonen; the pyramid is never reprojected), and phase 2 tests every instance phase 1 did not draw against this frame's pyramid with the current view-projection.
- [Z2-N-REND-644] The next frame's visibility bits are the current half of the view's ping-pong visibility-bits buffer, cleared to zero by an `AddClearBuffer` pass before phase 1; the emit steps of both phases set the bit of every instance they draw, so after phase 2 the buffer holds the union of the phase-1 and phase-2 draws, and no emit field claims full coverage of it.
- [Z2-N-REND-645] The final pyramid of the frame is kept as the view's persistent HiZ: its max channel serves next frame's phase 1, and its min channel serves SSR's hierarchical tracing this frame.
- [Z2-N-REND-646] When a view's history is invalid (first frame, camera cut, output-extent change, shading-path change), phase 1 has no candidates and phase 2 tests every instance against the pyramid built from an empty depth (all +∞), so every frustum-visible instance is drawn that frame.
- [Z2-N-REND-647] On tile-based GPUs, two-phase culling forces a mid-frame depth store and reload between the prepass phases; that bandwidth is budgeted by `Z2-B-RPROD-026` HiZ mid-frame depth store and reload bandwidth (pending) and is part of the D-TBDR measurement (open, Stage 5).

## Indirect draw compaction

The unit of compaction is the draw key (material slot, LOD): one material slot fixes both the primitive (`m_uPrimitiveBase + LOD × S + section`, the slot's section) and the material, so two instances of one primitive with different material slots in one bucket get different commands (`Zenith2/Docs/Design/RPROD/Culling.md → "Draw keys and command regions"`).

<!-- elaboration -->
| Element | Specification |
|---|---|
| command index | each material slot s in a bucket of kind k (depth, colour or shadow) owns `m_uLODCount` consecutive command slots starting at its `Flux_SlotCommandRecord`'s base for k; the command of (s, LOD l) is base + l |
| region | bucket b's region is the concatenation, in ascending material-slot order, of the command slots of every slot in b; regions are concatenated in ascending bucket index, and `BucketCommandBase[b]` is b's first command |
| sizing | on the CPU from the registered slots: commands per kind = the sum of that kind's region lengths, visible-instance capacity = the frame's candidate instance-section count, counts = the bucket count; a frame whose slot set or bucket set changed rebuilds every base in O(slots) and uploads them through the `BucketScatter` pass before the first cull pass |

| Dispatch | Per | Action |
|---|---|---|
| clear (`CullClear`) | phase | the per-command counters, the bucket counts and the survivor cursor to zero, the sort dispatch arguments to `(0, 1, 1)` |
| cull | candidate instance-section (instance × section of the selected LOD) | LOD selection with `Flux_SelectLOD` and the LOD reference view, the per-instance cull distance, frustum test, HiZ test; for each survivor, atomically increment its command's counter (`m_xCommandCounters`) and append (instance, command) to the survivor list |
| prefix | bucket | walk the bucket's region in command order: a command with a non-zero counter gets the compacted index n (the count of non-zero commands before it in the region) and the visible-instance base (the exclusive prefix sum of the counters), and writes its `Flux_IndirectDrawIndexed` at region base + n (`m_uDrawIndex` = n, instance count = the counter) and its `Flux_DrawRecord` at the same index; the bucket's count is n at the end, and n is added to the sort dispatch arguments |
| emit | survivor | write the instance index into the visible-instance list at its command's base plus an atomically reserved position, and set its instance bit in the drawn bits or the next frame's visibility bits |
| sort (`CullSort`) | visible command | sort each command's range of the visible-instance list by instance index (a groupshared bitonic sort up to 2,048 entries, a global merge-path sort beyond), dispatched indirectly over the visible command count |

- [Z2-N-REND-648] Culling runs as compute over the GPU-scene instance buffer and writes compacted indirect draw arguments (`Flux_IndirectDrawIndexed`, 24 bytes) plus one count per bucket, per material bucket and pipeline, consumed by `vkCmdDrawIndexedIndirectCount` or `ExecuteIndirect` with a count buffer, so a culled object never reaches the vertex or pixel stage and the CPU never sees per-object visibility.
- [Z2-N-REND-649] Indirect buckets are keyed by (pipeline, raster stream-set code), taken from the material-slot record's bucket fields for the pass kind, and compaction emits one command slot per draw key (material slot, LOD) per bucket kind, each bucket's command region being sized on the CPU from its registered slots as the tables above specify, so the GPU never writes past a region.
- [Z2-N-REND-650] Compaction is deterministic: survivors are counted per command, the prefix step writes each non-zero command, and the separate `CullSort` dispatch orders each command's visible-instance range by instance index (a range bounded by the instance count), so the same scene and camera produce byte-identical indirect arguments and visible-instance lists on every run and on both APIs.

### Per-instance cull distance

<!-- elaboration -->
For a primitive whose flags carry `FLUX_PRIMITIVE_FLAG_CUSTOM_CULL_DISTANCE` (`Zenith2/Docs/Rendering/GpuResources.md` "Primitive record (128 bytes)"), the cull step culls an instance when the distance from the LOD reference view's position (`Flux_InstanceCullParams::m_xLODView`) to its world bounds centre exceeds `m_uCustomData >> 16` metres; the value 65,535 means no limit. The test runs before the frustum test and counts as frustum-culled. ENV's placed foliage sets the distance per foliage type (`Zenith2/Docs/Design/RPROD/Culling.md → "Per-instance cull distance"`).

- [Z2-N-REND-662] The per-instance cull distance applies in every view that culls instances, shadow views included, with the frame's LOD reference view, so an instance culled by distance in the main view casts no shadow, and a primitive without the flag is never distance-culled by it.
- [Z2-N-REND-651] Shadow views, depth-only opaque buckets and every position-only draw use the primitive's position-only index range when `m_uPositionOnlyFirstIndex` is valid.

## Meshlet culling

- [Z2-N-REND-652] The visibility-buffer path runs the same two-phase test per meshlet after the instance test: each surviving instance's meshlets are tested for frustum, for backfacing with the meshlet's normal cone, and against the HiZ pyramid (phase 1 with the previous view-projection and last frame's pyramid, phase 2 with the current ones), and survivors are appended to the visible-meshlet list.
- [Z2-N-REND-653] The visible-meshlet list feeds `DrawMeshTasksIndirectCount` when `m_bMeshShaders` is set and otherwise a compute-expanded indirect index draw (the fallback), both producing identical visibility IDs.
- [Z2-N-REND-654] The visible-meshlet list holds at most 2²⁵ entries (the visibility ID's 25-bit meshlet field); a frame whose survivors would exceed it keeps the first 2²⁵ in deterministic order, draws the rest as ordinary instance-level draws of the visibility raster fallback, and increments `Flux.Culling.MeshletOverflow`.

## Shadow view culling

- [Z2-N-REND-655] Shadow views use frustum and distance culling (the light's range, the cascade's far split), plus HiZ against the shadow view's own cascade-depth pyramid wherever the capability tier enables shadow HiZ; the shadow pyramid uses orthographic or perspective linear depth along the light view and the same two-phase scheme keyed by the shadow view's ID.
- [Z2-N-REND-656] A shadow view whose projection moved by more than its texel-snapping step since last frame, or whose history is invalid, skips the phase-1 HiZ test (frustum and distance only) for that frame.

## Culling diagnostics

| Counter (per view, per phase) | Meaning |
|---|---|
| `Flux.Culling.Tested` | candidate instance-sections tested |
| `Flux.Culling.FrustumCulled` | rejected by the frustum or distance test |
| `Flux.Culling.OcclusionCulled` | rejected by the HiZ test |
| `Flux.Culling.Drawn` | survivors emitted as draws |
| `Flux.Culling.MeshletsTested`, `…MeshletsConeCulled`, `…MeshletsOcclusionCulled`, `…MeshletsDrawn` | visibility-buffer path |

- [Z2-N-REND-657] The culling passes write the counters of the table above into a small readback buffer every frame, and the on-screen profiler shows them per view and per phase with at most `r.framesInFlight` frames of latency (the counters travel through a readback resolved when the frame slot is reused), each tagged with the frame index it describes.
- [Z2-N-REND-658] A debug view (`r.debug.hiz`) overlays the chosen pyramid mip and channel, and the screen rectangles of culled bounds coloured by the phase that culled them.

## Culling tests

| Test | Kind | Asserts |
|---|---|---|
| `Test_HiZ_ProjectionAnalytic` | unit (CPU reference) | box and sphere rectangles and nearest depths equal closed-form values for fixed cameras in every depth mode |
| `Test_HiZ_MipSelection` | unit | the selected mip covers the rectangle with at most 2×2 texels for a sweep of rectangle sizes and positions |
| `Test_HiZ_RoundingConservative` | unit | the max channel never rounds below, and the min channel never above, the f32 value, over every half interval up to 65,504 |
| `Test_HiZ_InfinityRule` | unit | cleared texels, NaN depths and depths ≥ 65,504 m give +∞ in both channels and never cull |
| `Test_HiZ_OddDimensionFold` | render | an occluder in the last odd column of mip 0 is represented in every coarser mip |
| `Test_HiZ_DepthModeParity` | render | the same scene in standard-D24, standard-D32F and reversed-D32F modes gives the same culled set |
| `Test_HiZ_FullyOccludedZeroDraws` | render | a fully occluded object records zero draws in both phases |
| `Test_HiZ_DisocclusionSameFrame` | render | an object revealed this frame by a moving occluder is drawn in phase 2 of this frame |
| `Test_HiZ_CompactionDeterministic` | render | indirect arguments are byte-identical over 10 runs and across Vulkan and D3D12 |
| `Test_HiZ_TransparentOrderPreserved` | render | transparent draws execute in the CPU sort order with culled entries drawing nothing, identically on Vulkan and D3D12 |
| `Test_HiZ_MeshletFallbackMatchesMeshShader` | render | the compute fallback and mesh-shader visibility IDs are bit-identical where mesh shaders exist |

- [Z2-N-REND-659] The culling tests of the table above exist in `Zenith2/Engine/ZenithFlux/Tests/Culling/`, run on WARP and lavapipe with validation clean, and pass.

## Transparent commands

Transparent geometry is drawn back to front, an order that a compacted, bucket-grouped command list loses, so the transparent pass uses fixed, non-compacted command slots in CPU sort order. The CPU learns no visibility: the GPU writes each slot's instance count.

<!-- elaboration -->
```cpp
struct Flux_TransparentEntry      // 16 bytes, upload ring, one per transparent instance-section per view, in sort order;
                                  // declared [FluxShared] in Common/Culling.slang, generated
{
	uint32 m_uInstance;          // GPU scene instance index
	uint32 m_uPrimitive;         // primitive record of the CPU-selected LOD and section
	uint32 m_uMaterialSlot;      // its m_uColourBucket names the transparent pipeline
	uint32 m_uRunStart;          // index of the first entry of this entry's bucket run
};
static_assert(sizeof(Flux_TransparentEntry) == 16);
```

<!-- elaboration -->
| Element | Specification |
|---|---|
| Collection | during view-list construction the CPU collects, per view, every section of every instance carrying `FLUX_INSTANCE_FLAG_TRANSPARENT` whose world bounds lie within the view's far distance, selecting the LOD with GEOM's `Flux_SelectLODCPU`; it performs no frustum or occlusion test |
| Two-sided sections | a section whose material is two-sided with `twoSidedOrder = backThenFront` yields two entries with the same sort key: back faces (cull front) first, front faces (cull back) second; `twoSidedOrder = single` yields one cull-none entry (`Zenith2/Docs/Design/GEOM/Materials.md → "Transparency"`) |
| Sort key | view-space depth of the world bounds centre plus the material's `sortBias`, farthest first; ties by instance index, then section, then face order, ascending |
| Runs | consecutive entries whose material slots share `m_uColourBucket` and whose cull mode is the same form one run (a cull-mode change breaks a run, because the cull mode is part of the pipeline); `m_uRunStart` names the run's first entry |
| Upload | the entries are build-node frame data: `16 × n` bytes declared at setup with owner `"Flux.Transparent"` and written after the frame's `ExtendReservation` (`Zenith2/Docs/Rendering/GpuResources.md` "Build-node upload order"); the count has no cap of its own, so a view whose entries push the frame above `Z2-B-RHI-006` fails the frame ("Frame failures" there) |
| Emit | the `TransparentCull` pass (`Zenith2/Docs/Rendering/PerPass/Common.md` "TransparentCull"): the `InstanceCull` program with `uPERM_STEP` = transparent, one thread per entry i: frustum test against the current camera, then the HiZ test against the view's final pyramid with the current view-projection; it writes command slot i (`m_uDrawIndex` = i − `m_uRunStart`, index count, first index and vertex offset from the primitive, `m_uInstanceCount` = 1 when visible and 0 when culled, `m_uFirstInstance` = 0), draw record i (`m_uVisibleInstanceBase` = i, the primitive, the material slot, the flags) and visible-instance list entry i = `m_uInstance` |
| Count | none: slot i always exists, so there is no count buffer and no compaction |
| Draw | the `TransparentForward` lambda walks the runs in order: `BindPipeline` for the run's bucket and cull mode, `BeginDrawParams` with `m_uBucketRecordBase` = `m_uRunStart`, then `DrawIndexedIndirect(uFirstCommand = m_uRunStart, uCommandCount = run length)` |

- [Z2-N-REND-660] The transparent pass departs from compacted indirect arguments on purpose: its commands are fixed, non-compacted slots in CPU sort order, one per transparent entry (one per instance-section, two for a `backThenFront` section), whose instance count the GPU culling writes (1 visible, 0 culled), and no count buffer exists for them.
- [Z2-N-REND-661] Transparent commands are collected, sorted, grouped into runs, emitted and drawn exactly as the table above specifies, so draw order equals CPU sort order on both APIs and every culled entry draws nothing.
