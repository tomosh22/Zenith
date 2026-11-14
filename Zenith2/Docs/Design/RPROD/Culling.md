# RPROD design: culling, HiZ and indirect draw compaction

This file is part of the RPROD design family; the entry document is `Zenith2/Docs/Design/RPROD.md`. It specifies how Flux realises two-phase HiZ occlusion culling at instance and meshlet level, the HiZ pyramid resources, GPU-driven indirect draw compaction with its bucket and draw-key tables, the transparent command path, shadow-view culling, the CPU reference, the culling counters and the HiZ debug overlay.

## Scope of this file

The specification is `Zenith2/Docs/Rendering/Culling.md` (every heading), `Zenith2/Docs/Rendering.md → "Hierarchical-Z occlusion culling"` and the culling and HiZ passes of `Zenith2/Docs/Rendering/PerPass/Common.md → "Culling and HiZ passes"` and "Shadow passes"; the pass parameter tables stay there and are extended only where this file says so, in the diff that implements the extension ([Z2-N-REND-800]). LOD selection inside the cull dispatch is GEOM's algorithm (`Zenith2/Docs/Design/GEOM/Meshes.md → "LOD selection"`), called here. Code lives in `Zenith2/Engine/ZenithFlux/Internal/Culling/`, `Zenith2/Shaders/Passes/Culling/`, `Zenith2/Shaders/Passes/HiZ/` and `Zenith2/Shaders/Common/HiZTest.slang`.

## Culling files

| File | Contents |
|---|---|
| `Internal/Culling/Flux_CullingSystem.h/.cpp` | per-frame preparation of every view's culling inputs; the bodies of `Flux_AddInstanceCullPhase1`, `Flux_AddInstanceCullPhase2`, `Flux_AddHiZBuild`, the shadow-view culling part of `Flux_AddShadowView`, the transparent emission part of `Flux_AddTransparentForward` |
| `Internal/Culling/Flux_BucketRegistry.h/.cpp` | "Bucket registry" |
| `Internal/Culling/Flux_DrawKeyTable.h/.cpp` | "Draw keys and command regions" |
| `Internal/Culling/Flux_TransparentCollector.h/.cpp` | "Transparent collection and emission" (the CPU half) |
| `Internal/Culling/Flux_HiZResources.h/.cpp` | "HiZ pyramid resources" |
| `Internal/Culling/Flux_CullingCounters.h/.cpp` | "Culling counters readback" |
| `Internal/Culling/Flux_HiZDebugOverlay.cpp` | "HiZ debug overlay" |
| `Internal/Culling/Flux_MeshletCulling.cpp` | the bodies of `Flux_AddMeshletCullPhase1` and `Flux_AddMeshletCullPhase2` (Stage 5) |
| `Zenith2/Shaders/Common/HiZTest.slang` | the one projection, nearest-depth and mip-selection module ([Z2-N-REND-642]) |
| `Zenith2/Shaders/Passes/Culling/InstanceCull.slang` | the `InstanceCull` program: steps cull, prefix, emit, transparent |
| `Zenith2/Shaders/Passes/Culling/CullSort.slang` | the per-command survivor sort of "Instance culling dispatch sequence" |
| `Zenith2/Shaders/Passes/Culling/BucketScatter.slang` | the scatter of slot command records ("Draw keys and command regions") |
| `Zenith2/Shaders/Passes/HiZ/HiZ.slang` | the single-pass downsampler |
| `Zenith2/Shaders/Passes/Debug/HiZOverlay.slang` | the debug overlay program |
| `Zenith2/Shaders/Passes/VisibilityBuffer/MeshletCull.slang` | the meshlet cull program (Stage 5) |
| `Zenith2/Engine/ZenithFlux/Tests/Culling/Flux_HiZReference.h/.cpp` | the bit-matching CPU reference |

Four passes of this file have no table in `Zenith2/Docs/Rendering/PerPass/Common.md` yet: `CullClear`, `CullSort`, `BucketScatter` and `HiZDebugOverlay`. Their tables are "Tables of the passes this file adds" below; the ticket that lands each pass adds its table to `Zenith2/Docs/Rendering/PerPass/Common.md → "Culling and HiZ passes"` (the overlay under "Epilogue passes") in its own diff ([Z2-N-REND-795], [Z2-N-REND-800]), and reconciliation row RP24 records the four additions.

[Z2-N-REND-642] places the shared mathematics in the module `Zenith2/Shaders/Common/HiZTest.slang`, because the instance cull, meshlet cull, transparent cull and debug overlay programs all import it and [Z2-N-REND-105] forbids a program importing another (reconciliation row RP5).

## HiZ pyramid resources

| Resource | Owner | Format and extent | Per frame |
|---|---|---|---|
| view pyramid, two halves `m_axHiZ[2]` | `Flux_ViewRegistry` (Zenith2/Docs/Design/RPROD/Views.md "View registry"), created by `Flux_HiZResources::CreateViewPyramid` | RG16F, the view's render extent, mip count `floor(log2(max(w, h))) + 1` (at most 13), usage sampled and storage, persistent-imported ([Z2-N-REND-635]) | the previous half is imported as `Flux_ViewContext::m_xPrevHiZ`; the current half is imported, written by the post-phase-1 build (full coverage of every mip), overwritten by the final build, and marked output ([Z2-N-REND-645]) |
| shadow-view pyramid | as above, per shadow view whose tier enables shadow HiZ | RG16F at the shadow view's atlas-tile extent | as above ([Z2-N-REND-655]) |
| SPD counter | transient, 4 bytes | cleared by `AddClearBuffer` before each build ([Z2-N-REND-640]) | two per view per frame |
| mip views | graph views, one `Zenith_RHIUAV` per mip, `[Optional]` beyond the mip count | — | 13 fields of `Flux_HiZParams::m_axMips` |

<!-- elaboration -->
The two builds of a frame write the same current half: the post-phase-1 build writes version v + 1 (every mip full coverage), phase 2 and prepass phase 2 read it, and the final build writes version v + 2 from the complete depth; the half is therefore never a transient and costs no aliasing memory. An output-extent change recreates both halves at the new extent and invalidates the view's histories ([Z2-N-REND-599]); the new halves' first frame follows [Z2-N-REND-646].

A render-extent change alone (a dynamic-resolution step, LIGHT row L18) keeps the histories valid ([Z2-N-REND-599]): it recreates only the half that becomes current, at the new extent, before the post-phase-1 build writes it; the previous half keeps its old extent and phase 1 reads it with its own extent (`Flux_InstanceCullParams::m_xHiZExtent` and `m_uHiZMipCount` describe the previous half, not the current one), so HiZ culling stays valid across the step.

## HiZ build dispatch

`Flux_AddHiZBuild(xGraph, xView, xDepth)` adds exactly one compute pass `HiZBuild` ([Z2-N-REND-640]):

| Parameter | Value |
|---|---|
| permutation | `uPERM_DEPTH_MODE` from the view's `Flux_DepthMode` (standard; reversed; reversed with infinite far), `bPERM_ORTHOGRAPHIC` from the camera; 4 compiled keys |
| dispatch | `(ceil(w / 64), ceil(h / 64), 1)` groups of 256 threads; `m_uGroupCount` is their product |
| `m_fNear`, `m_fFar` | the camera's planes (`+∞` far for the infinite mode) |
| mip 0 conversion | the conversion table of `Zenith2/Docs/Rendering/Culling.md → "Pyramid construction"`; cleared depth (1.0 standard, 0.0 reversed) and NaN give `+∞` ([Z2-N-REND-637]) |
| rounding | `RoundUpToHalf` for the max channel, `RoundDownToHalf` for the min channel, integer-only half codec of `Zenith2/Shaders/Common/Half.slang` ([Z2-N-REND-638]) |
| reduction | 2 × 2 max/min, with the odd-edge fold into 3 × 2, 2 × 3 or 3 × 3 footprints ([Z2-N-REND-639]); mips 1–6 per group in groupshared memory; the last group (by the atomic counter) reduces mips 7 onward |

The shadow form passes the atlas tile rectangle as `m_xTileOffset` and `bPERM_ORTHOGRAPHIC` from the shadow projection, standard depth mode alone.

## Culling records

<!-- elaboration -->
The records below are declared `[FluxShared]` in `Zenith2/Shaders/Common/Culling.slang` (their C++ form is generated into `Flux_SharedTypes.generated.h`), except `Flux_CullOutput` and `Flux_MeshletCullOutput`, which are CPU-side values of `Zenith2/Engine/ZenithFlux/Flux_Features.h` (`Zenith2/Docs/Design/RPROD/Passes.md → "Shared feature input types"`). Each landing ticket adds its rows to the table of `Zenith2/Docs/Rendering/ShaderSystem.md → "Shared GPU structs"` in its own diff ([Z2-N-REND-136]).

| Record | Bytes | Fields | Where |
|---|---|---|---|
| `Flux_TransparentEntry` | 16 | exactly `Zenith2/Docs/Rendering/Culling.md → "Transparent commands"` | upload ring, per view |
| `Flux_FrustumPlanes` | 96 | `Zenith_RHIFloat4 m_axPlanes[6]` (inward normals, world space), for the light list and the cull passes | upload ring |
| `Flux_SlotCommandRecord` | 16 | `m_uDepthCommandBase`, `m_uColourCommandBase`, `m_uShadowCommandBase` (command index of the slot's LOD 0 in each bucket kind's region, `0xFFFFFFFF` when the slot is in no bucket of that kind), `m_uLODCount` | persistent buffer, one per material slot ("Draw keys and command regions") |
| `Flux_SlotCommandScatter` | 20 | `m_uMaterialSlot`, then the slot's `Flux_SlotCommandRecord` | upload ring, the `BucketScatter` input |
| `Flux_CullSurvivor` | 8 | `m_uInstance`, `m_uCommand` (bits 0–27 the command index, bits 28–31 the section's bucket kind) | transient, per view per phase |
| `Flux_CullCounters` | 32 | `m_uTested`, `m_uFrustumCulled`, `m_uOcclusionCulled`, `m_uDrawn`, `m_uMeshletsTested`, `m_uMeshletsConeCulled`, `m_uMeshletsOcclusionCulled`, `m_uMeshletsDrawn` | transient, one per (view, phase), read back ("Culling counters readback") |
| `Flux_LODView` | GEOM's (`Zenith2/Docs/Design/GEOM/Meshes.md → "LOD selection"`) | the LOD reference view | a plain-data field of `Flux_InstanceCullParams` |

`Flux_InstanceCullParams` gains, in the diffs that implement them, three plain-data members beside those `Zenith2/Docs/Rendering/PerPass/Common.md → "InstanceCullPhase1"` lists: `Flux_LODView m_xLODView` (GEOM's row GM6), `uint32 m_uBucketKind` (0 depth, 1 colour, 2 shadow), `uint32 m_uDebugFlags` (bit 0 writes the culled rectangles of "HiZ debug overlay"), and the resource fields `m_xSlotCommands` (`Zenith_RHISRV<Zenith_RHIStructBuffer<Flux_SlotCommandRecord>>`, read), `m_xBucketCommandBase` (`Zenith_RHISRV<Zenith_RHIByteBuffer>`, read), `m_xSortDispatchArgs` (`Zenith_RHIUAV<Zenith_RHIByteBuffer>`, written by the prefix step: the 12-byte dispatch arguments `(n, 1, 1)` with n the view's non-zero command count of the bucket kind, read indirectly by `CullSort`) and `m_xDebugBounds` (`Zenith_RHIUAV<Zenith_RHIByteBuffer>`, `[Optional]`, write). The permutation set stays the 26 keys of the specification.

The specification's field `m_xPrimitiveCounters` (one counter per primitive) becomes `m_xCommandCounters` (`Zenith_RHIUAV<Zenith_RHIByteBuffer>`, cleared transient, one `u32` per command slot of "Draw keys and command regions", read-write, compute) in the `InstanceCullPhase1` table; the rename and the unit it carries are part of cross-doc item X99 and land with it.

## Bucket registry

<!-- elaboration -->
```cpp
// Internal/Culling/Flux_BucketRegistry.h
enum Flux_BucketKind : uint8 { FLUX_BUCKET_KIND_DEPTH = 0, FLUX_BUCKET_KIND_COLOUR = 1, FLUX_BUCKET_KIND_SHADOW = 2, FLUX_BUCKET_KIND_COUNT = 3 };

struct Flux_BucketKey                    // 48 bytes; equality is bytewise
{
	Zenith_RHIShaderProgramID m_xProgram;
	Zenith_RHIPermutationKey m_xPermutation;     // program key, stream-set code, material variant
	Flux_RasterState m_xRaster;                  // the bucket's cull, depth and blend state (PSO.md "Flux_PSO header")
	Flux_BucketKind m_eKind;
	uint8 m_uOrderClass;                        // 0 opaque, 1 alpha-tested: the early-Z order of [Z2-N-REND-722]
	uint16 m_uPad;
	uint32 m_uPad2;
};

class Flux_BucketRegistry          // owned by Flux_CullingSystem; one per renderer
{
public:
	uint32 Acquire(const Flux_BucketKey& xKey);     // dense index; reference counted
	void Release(uint32 uBucket);                                 // index recycled after the retirement of every frame that drew it
	const Flux_BucketKey& GetKey(uint32 uBucket) const;
	Zenith_Span<const uint32> GetBucketsInOrder(Flux_BucketKind eKind) const;   // (order class, index) ascending
	uint32 GetBucketCount() const;
	bool HasChangedSince(uint64 ulFrameIndex) const;
};
```

| Rule | Specification |
|---|---|
| who acquires | GEOM's material-slot cache (`Zenith2/Docs/Design/GEOM/Materials.md → "Material slots and constants"`) acquires the three buckets of each material slot when the slot is created and writes their indices into the slot's `m_uDepthBucket`, `m_uColourBucket` and `m_uShadowBucket` ([Z2-N-REND-649]); transparent domains acquire a colour bucket alone |
| key derivation | depth bucket: the depth program of the domain (the material-independent prepass program for opaque, the `[AlphaTest]` variant with the material for alpha-tested), the mesh's raster stream-set code; colour bucket: the path's opaque or G-buffer program with the material variant and the raster stream-set code; shadow bucket: the shadow-depth program's position-only specialisation of the mesh's raster stream-set code for opaque (the program declares POSITION alone, so its specialised table binds stream 0 of that code's pool, `Zenith2/Docs/Design/GEOM/VertexFormat.md → "Program stream-set specialisation"`), and its `[AlphaTest]` variant with the material and the same raster code otherwise; two raster codes never share a shadow bucket, because each is its own stream-0 pool and one bucket binds one pool set ([Z2-N-REND-887]; GEOM row GM21). The stream-set component is always the raster code (`POOL16`/`POOL32` for a skinned mesh), which is also the key of the fixed-function vertex pool every draw of the bucket binds; a skinned mesh's bind-position and skin streams live in a compute-only skin-source pool keyed by the cooked code, read by the skinning dispatch and never bound to a raster draw (`Zenith2/Docs/Design/GEOM/Meshes.md → "Geometry pools"`) |
| capacity | at most 65,535 buckets; `Acquire` beyond it returns `uFLUX_RG_INVALID`, the slot is not drawn and `Flux.Culling.BucketsFull` increments |
| threading | `Acquire`/`Release` on the build node (material loads are finalised there, `Zenith2/Docs/Design/GEOM.md → "Frame-level flow of GEOM work"`); readers on the build node alone |
| path change | a shading-path switch acquires the new path's colour buckets for every live slot at the next build ([Z2-N-REND-692]) |

[Z2-N-REND-886] and [Z2-N-REND-887] key a bucket by "(pipeline, stream-set)" with the raster code (X94 applied): buckets by the raster code, the fixed-function pool by the raster code, the skin-source pool by the cooked code, as GEOM's design conforms.

## Draw keys and command regions

`Zenith2/Docs/Rendering/Culling.md → "Indirect draw compaction"` compacts per draw key ([Z2-N-REND-650], X99 applied): RHI's GPU scene allocates primitives per mesh and material slots per (mesh, material assignment) (`Zenith2/Docs/Design/RHI/Memory.md → "Slot allocators"`), so two instances of one primitive can carry different material slots in one bucket, and the draw key (material slot, LOD) is the unit of compaction, which fixes both the primitive (`m_uPrimitiveBase + LOD × S + section`, the slot's section) and the material slot (reconciliation row RP6).

| Element | Specification |
|---|---|
| command index | each material slot s in a bucket of kind k owns `m_uLODCount` consecutive command slots starting at `m_u<Kind>CommandBase`; the command of (s, LOD l) is `base + l` |
| region | bucket b's region is the concatenation, in ascending material-slot order, of the command slots of every slot in b; regions are concatenated in ascending bucket index; `m_xBucketCommandBase[b]` is b's first command and the region length is `Flux_DrawKeyTable::GetRegionLength(b)` |
| rebuild | on a frame where `Flux_BucketRegistry::HasChangedSince(last)` or a slot entered or left a bucket, `Flux_DrawKeyTable::Rebuild` recomputes every base on the CPU in O(slots), writes the changed `Flux_SlotCommandRecord`s as scatter records into the upload ring and the `BucketCommandBase` array (u32 per bucket) as a full upload, and adds the `BucketScatter` compute pass before the first cull pass of the frame; no pass reads a stale base because the scatter is ordered before every reader by the graph |
| persistent buffers | `SlotCommands` (16 bytes × material-slot capacity) and `BucketCommandBase` (4 bytes × bucket capacity) are imported every frame; they grow by resize and copy like the GPU scene tables ([Z2-N-REND-544]) |
| sizes per view | commands per kind = the sum of region lengths of that kind; visible-instance capacity = the frame's candidate instance-section count; counts = bucket count; these size the transient draw-argument, draw-record, count, counter, survivor and visible-instance buffers of each view, so the GPU never writes past a region ([Z2-N-REND-649]) |

## Instance culling dispatch sequence

`Flux_AddInstanceCullPhase1` and `Flux_AddInstanceCullPhase2` each add, for one view and one bucket kind, these passes of the `InstanceCull` program ([Z2-N-REND-648], [Z2-N-REND-650]):

| # | Pass | Dispatch | Action |
|---|---|---|---|
| 0 | `CullClear` | `AddClearBuffer` ×4 | per-command counters, bucket counts and the survivor cursor to zero, the sort dispatch arguments to `(0, 1, 1)` ("CullClear" below) |
| 1 | `InstanceCull[cull]` | one thread per candidate instance-section (64 per group) | phase 1: candidates whose instance bit is set in the previous visibility bits; phase 2: candidates whose instance bit is clear in this frame's drawn bits; select the LOD with `Flux_SelectLOD` and `m_xLODView`; frustum test (and distance for shadow views); the per-instance cull distance of "Per-instance cull distance" when its primitive carries the flag; the HiZ test of `Zenith2/Docs/Rendering/Culling.md → "Occlusion test"` through `HiZTest.slang` when `bPERM_HIZ` (phase 1: current bounds, previous view-projection, nearest depth linearised in the previous camera space, the previous pyramid; phase 2: the current view-projection and this frame's pyramid); a survivor increments `counter[command]` atomically and appends `{instance, command}` to the survivor list through the cursor; counters of `Flux_CullCounters` accumulate by atomics |
| 2 | `InstanceCull[prefix]` | one group of 256 per bucket | walk the bucket's region in command order: a command with a non-zero counter gets the compacted index `n` (the count of non-zero commands before it in the region) and the visible-instance base (the exclusive prefix sum of the counters); it writes `Flux_IndirectDrawIndexed` at region base + n (`m_uDrawIndex` = n, index count, first index and vertex offset from the command's primitive record, `m_uInstanceCount` = the counter, `m_uFirstInstance` = 0, [Z2-N-REND-070]) and `Flux_DrawRecord` at the same index (base, primitive, material slot, flags); the bucket's count is n at the end, and n is added atomically to the x component of `m_xSortDispatchArgs` (cleared by `CullClear` to `(0, 1, 1)`) |
| 3 | `InstanceCull[emit]` | one thread per survivor | write the instance into the visible-instance list at its command's base plus an atomically reserved position, and set its instance bit in the drawn bits (phase 1) or in the next frame's visibility bits (both phases, [Z2-N-REND-644]) |
| 4 | `CullSort` | one group of 256 per visible command, `DispatchIndirect` over the command count | sort each command's range of the visible-instance list by instance index: a groupshared bitonic sort for ranges of at most 2,048 entries, otherwise a global merge-path sort in 2,048-entry tiles; the range is bounded by the instance count, so the output is byte-identical whatever order step 3 wrote in ([Z2-N-REND-650]) |

The prefix step writes each command, because after the per-command sort there is no first survivor to elect deterministically; the arguments are identical.

<!-- elaboration -->
Phase 1 and phase 2 of one view use separate draw-argument, draw-record, count and visible-instance buffers, returned in `Flux_CullOutput::m_axPhase[2]`, and every raster consumer draws the union by issuing each bucket's indirect call once per phase ([Z2-N-REND-740]).

The visibility bits are the view's persistent ping-pong buffer (`Flux_ViewPersistentState::m_axVisibilityBits`): the current half is cleared by an `AddClearBuffer` pass before phase 1 and set by both emits, so it holds the union ([Z2-N-REND-644]); a view with an invalid history imports no previous half and phase 1 has no candidates ([Z2-N-REND-646]).

### Per-instance cull distance

<!-- assumption: pending GEOM (RP21) -->
ENV's placed foliage (row EN10) needs a per-type cull distance without CPU culling. For a primitive whose record carries `FLUX_PRIMITIVE_FLAG_CUSTOM_CULL_DISTANCE` (primitive flag bit 5, which is bit 29 of the primitive record's `m_uStreamSetAndLOD`: flag bit n is bit 24 + n, `Zenith2/Docs/Rendering/GpuResources.md → "Primitive record (128 bytes)"`; the record has no separate flags word), step 1 culls an instance when the distance from the LOD reference view's position (`m_xLODView`) to its world bounds centre exceeds `m_uCustomData >> 16` metres (bits 16–31 of the instance record's `m_uCustomData`; 65,535 means no limit). The test runs before the frustum test and counts in `m_uFrustumCulled`; shadow views apply it with the same LOD reference view, so a foliage instance culled in the main view casts no shadow. The flag bit is in Rendering's primitive record (cross-doc item X127 (2), applied); setting it is a GEOM primitive-writer input (the mesh cook sets it for a mesh whose document requests it), recorded as reconciliation row RP21; until GEOM's writer sets it, no primitive carries the flag and the step is inert.
<!-- /assumption -->

## Two-phase sequence per view

For every main, editor and texture view, and every capture view whose feature mask carries `FLUX_VIEW_FEATURE_HIZ_OCCLUSION` ([Z2-N-REND-630]; `Zenith2/Docs/Rendering/Culling.md` opening paragraph), the path function (`Zenith2/Docs/Design/RPROD/Passes.md → "Path frame functions"`) declares, with the data flow shown ([Z2-N-REND-643], [Z2-N-REND-727]):

```
xCull1 = Flux_AddInstanceCullPhase1(xGraph, xView, xTargets)      // prev bits, prev HiZ, prev view-projection
Flux_Add<Path>Prepass(xGraph, xView, xCull1, xTargets)             // depth (+ velocity, normals, roughness): CLEAR, STORE
xTargets.m_xHiZ = Flux_AddHiZBuild(xGraph, xView, xTargets.m_xDepth)                // current half, version v+1
xCull2 = Flux_AddInstanceCullPhase2(xGraph, xView, xCull1, xTargets)               // drawn bits of phase 1, new HiZ
Flux_Add<Path>Prepass(xGraph, xView, xCull2, xTargets)             // LOAD, STORE
xTargets.m_xHiZ = Flux_AddHiZBuild(xGraph, xView, xTargets.m_xDepth)                // final: current half, version v+2, MarkOutput
```

A view whose feature mask lacks the bit (a preview view, a capture view without it) runs one culling phase with `bPERM_HIZ = false` and one prepass, and builds no pyramid. The visibility-buffer path substitutes `Flux_AddMeshletCullPhase1/2` and `Flux_AddVisibilityRaster` ("Meshlet culling realisation"). On `FLUX_CAPABILITY_TIER_MOBILE`, the depth store after prepass phase 1 and its reload in phase 2 are the bandwidth of `Z2-B-RPROD-026` ([Z2-N-REND-647]), measured by the RPROD budget harness and the D-TBDR (open, Stage 5) harness.

## Shadow view culling realisation

`Flux_AddShadowView` is LIGHT's feature function (`Zenith2/Docs/Design/LIGHT/Shadows.md → "Flux_Shadows header"`, implemented in `Internal/Passes/Shadows/`), called by LIGHT's `Flux_AddCascadedShadows` and `Flux_AddLocalShadows` per shadow view in view-list order ([Z2-N-REND-725]). Its culling half is RPROD's: it calls `Flux_CullOutput Flux_AddShadowViewCulling(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_FrameShared& xShared)` (declared in `Internal/Culling/Flux_CullingSystem.h`), which adds the culling and HiZ passes of steps 1, 2 and 4 below; LIGHT's body adds the `ShadowDepth` passes of step 3 (after each culling phase when step 2 runs two phases):

| Step | Action |
|---|---|
| 1 | `InstanceCull` with `m_uBucketKind` = shadow, candidates = every instance with `FLUX_INSTANCE_FLAG_CASTS_SHADOW`; frustum test against the shadow projection plus distance culling against the light's range or the cascade's far split ([Z2-N-REND-655]); LOD from the LOD reference view (GEOM's rule) |
| 2 | with shadow HiZ enabled by the tier (`Flux_QualitySettings::m_xLight.m_xShadows.m_bShadowHiZ`, LIGHT's member) and a valid shadow history whose projection moved by at most its texel-snapping step: the two-phase scheme keyed by the shadow view's ID, orthographic or perspective linear depth along the light view; otherwise one phase of frustum and distance culling alone ([Z2-N-REND-656]) |
| 3 | `ShadowDepth` draws the shadow buckets in order, opaque with the position-only index range when `m_uPositionOnlyFirstIndex` is valid ([Z2-N-REND-651]), then alpha-tested |
| 4 | `ShadowHiZBuild` over the tile when shadow HiZ is enabled |

## Meshlet culling realisation

Stage 5, visibility-buffer path ([Z2-N-REND-652] to [Z2-N-REND-654]):

| Step | Action |
|---|---|
| 1 | the instance test of "Instance culling dispatch sequence" step 1 with the meshlet program's instance stage; each surviving instance's meshlets (`m_uMeshletFirst`, `m_uMeshletCount` of its primitive) are appended to a meshlet candidate list |
| 2 | one thread per candidate meshlet: frustum test of the meshlet bounding sphere, the normal-cone backface test (cull when `dot(normalize(centre − eye), axis) ≥ cutoff`), and the HiZ test (phase 1 previous, phase 2 current) |
| 3 | survivors are appended to the visible-meshlet list, then sorted by (instance, meshlet) with the `CullSort` program so the visibility IDs are deterministic |
| 4 | the raster arguments: one `DrawMeshTasksIndirectCount` argument set per (opaque, alpha-tested) range when `m_bMeshShaders`, else one indirect indexed draw over a compute-expanded per-meshlet index list; both produce identical visibility IDs ([Z2-N-REND-653]) |
| 5 | overflow: survivors beyond 2²⁵ entries (the ID's 25-bit field) keep the first 2²⁵ in sorted order, the rest are drawn by the instance-level fallback draw, and `Flux.Culling.MeshletOverflow` increments ([Z2-N-REND-654]) |

## Transparent collection and emission

`Flux_TransparentCollector` builds each view's entries during view-list construction on the build node, as `Zenith2/Docs/Rendering/Culling.md → "Transparent commands"` specifies ([Z2-N-REND-660], [Z2-N-REND-661]), with GEOM's two-sided rule (row GM6; `Zenith2/Docs/Design/GEOM/Materials.md → "Transparency"`); the table's sort key carries `sortBias`, the face-order tie-break and the cull-mode run break of steps 2 to 4 below (X100 (d) applied).

| Step | Action |
|---|---|
| 1 | collect every section of every instance carrying `FLUX_INSTANCE_FLAG_TRANSPARENT` whose world bounds lie within the view's far distance; LOD from `Flux_SelectLODCPU` (`Zenith2/Docs/Design/GEOM/Meshes.md → "LOD selection"`) |
| 2 | a section whose material is two-sided with `twoSidedOrder = backThenFront` (`Zenith2/Docs/Design/GEOM/Materials.md → "Transparency"`) yields two entries with the same sort key, back faces (cull front) first, front faces (cull back) second; `single` yields one cull-none entry |
| 3 | sort by view-space depth of the bounds centre plus the material's `sortBias`, farthest first; ties by instance index, then section, then face order |
| 4 | form runs: consecutive entries with the same colour bucket and the same cull mode form one run (a cull-mode change breaks a run, because the cull mode is part of the pipeline); `m_uRunStart` names the run's first entry |
| 5 | at setup, declare `16 × n` bytes for the n entries with `Flux_UploadRing::DeclareFrameData(16 × n, 16, "Flux.Transparent")` and keep the ticket; publish `Flux.Culling.TransparentEntries[<view>]` = n; after the frame's `ExtendReservation` succeeded, write them through `GetFrameData` ("Build-node upload data" below); `TransparentCull` (the `InstanceCull` program, step `transparent`) writes slot i's command, draw record and visible-instance entry, instance count 1 visible, 0 culled |
| 6 | `TransparentForward` walks the runs: `BindPipeline` for the run's bucket and cull mode, `BeginDrawParams` with `m_uBucketRecordBase = m_uRunStart`, `DrawIndexedIndirect(m_uRunStart, run length)` |

The CPU never learns which transparent entries were culled ([Z2-N-REND-539]).

<!-- elaboration -->
The entry count has no fixed capacity of its own, which is [Z2-N-REND-660] and [Z2-N-REND-661] as written (every transparent instance-section of the view gets a slot): n is the view's collected count, bounded by its inputs (transparent instances × their sections, × 2 for `backThenFront` pairs), and every per-view buffer `TransparentCull` writes (commands, draw records, visible-instance entries) is sized from n on the CPU. The one bound on the entries is the frame's upload ring, whose hard cap is `Z2-B-RHI-006` Upload ring hard cap (pending): the entries' bytes are part of the frame's exact need, and the need's two outcomes are RHI's (`Zenith2/Docs/Design/RHI/Memory.md → "Frame deferral"`, "Frame failures"):

| Exact need of the frame | Outcome |
|---|---|
| within the cap, but the ring cannot serve it without a wait | the frame is deferred (`Z2-B-RHI-007` Upload ring stall frames): nothing of it is written, the visibility-bit history does not advance, and the next frame reserves the recorded need |
| above the cap | the frame is cancelled with `FLUX_FRAME_FAILURE_UPLOAD_OVER_CAP`: nothing is recorded, submitted or presented, `RHI.Upload.OverCap` increments, and RHI's `flux.upload` error line (`upload need <n> bytes above the cap <cap> bytes; largest contributor ...`) names the owner string of the largest declaration, which is `Flux.Transparent` when the transparent entries dominate. An over-cap need does not shrink by itself, so after 30 consecutive failed frames (`uFLUX_MAX_CONSECUTIVE_FAILED_FRAMES`) the next prologue exits the process with `EXIT_CODE_DEVICE_LOST` (6) and RHI's over-cap message ("The renderer could not fit a frame's uploads into memory. Your progress has been saved.") |

A view whose transparent entries alone exceed the cap is therefore a content defect that ends the run about half a second after it appears. To make it attributable before the exit, the collector publishes the gauge `Flux.Culling.TransparentEntries[<view>]` every frame and logs one `flux.culling` warning per view per run, `culling: view <view suffix> transparent entries <n> (<bytes> bytes) above its share <s> bytes of the upload cap`, when `16 × n` exceeds the per-view soft share `s = floor(C / V)`, C being the cap the ring enforces and V the frame's view count.

<!-- assumption: pending RHI (RP27) -->
C is read through `uint64 Flux_UploadRing::GetCapBytes() const` (the `Z2-B-RHI-006` value once adopted), which RHI does not yet declare; reconciliation row RP27 asks for it, and until it lands the transparent-collection ticket is blocked by it for the warning alone (the over-cap failure itself is RHI's and needs no RPROD read).
<!-- /assumption -->

The share is a warning threshold, never a limit: the entries are still declared whole. The transparent bytes per view of W-Indoor and W-Outdoor are an input of the RHI budget ticket for `Z2-B-RHI-006` (entry document "Measurement harness"). The collector never draws a partial list, because a partial write before `ExtendReservation` is exactly what RHI's build-node order forbids.

### Build-node upload data

Every build-node producer of per-frame upload data in this file follows `Zenith2/Docs/Design/RHI/Memory.md → "Build-node upload order"` (RHI row R17 (a)): it calls `Flux_UploadRing::DeclareFrameData(bytes, align, owner)` during graph setup, keeps the ticket, writes through `GetFrameData(ticket)` only after the frame's `ExtendReservation` returned `FITS` or `EXTENDED`, and its passes read the bytes through `ImportFrameChunk`. The owner is a string literal, as `DeclareFrameData` requires, and it is the name RHI's over-cap log prints ("Frame failures"):

| Producer | Bytes | Alignment | Owner string |
|---|---|---|---|
| the transparent entries of one view | 16 per entry | 16 | `"Flux.Transparent"` |
| the frustum planes of one culled view | 96 | 16 | `"Flux.FrustumPlanes"` |
| the `BucketScatter` records, in rebuild frames | 20 per changed slot | 16 | `"Flux.BucketScatter"` |
| the `BucketCommandBase` array, in rebuild frames | 4 per bucket | 16 | `"Flux.BucketCommandBase"` |

The view is not part of the owner string (a per-view string would need storage outliving the frame's declaration list, and `DeclareFrameData` takes a literal); `Flux.Culling.TransparentEntries[<view>]` and the `flux.culling` warning of "Transparent collection and emission" attribute the transparent bytes to their view. The per-view counts `InstanceCull` reads (`m_uInstanceCount`, `m_uBucketCount`, `m_uTransparentCount`) travel in the parameter structs, not as frame data. A frame RHI defers writes none of them and advances no culling history (`Flux_FrameLifetime::GetFrameOutcome`, RHI row R17 (d)); a frame RHI fails writes none of them either, and records and presents nothing.

## CPU reference implementation

`Zenith2/Engine/ZenithFlux/Tests/Culling/Flux_HiZReference.h/.cpp` is a test-only C++ port of `HiZTest.slang` and the mip-0 conversion and rounding of `HiZ.slang`, written in GEOM's kernel operations (`Zenith2/Docs/Design/GEOM/VertexFormat.md → "The exact arithmetic kernel"`) so it matches the GPU bit for bit: box and sphere projection to a mip-0 rectangle, nearest linear depth per depth mode, mip selection, the conservative half rounding and the fold rule. The analytic tests of `Zenith2/Docs/Design/RPROD/Tests.md → "Culling tests"` drive it, and `Test_HiZ_ShaderMatchesReference` compares a GPU readback of the same 4,096 random cases with it bit for bit ([Z2-N-REND-642]).

## Culling counters readback

<!-- elaboration -->
| Step | Action |
|---|---|
| 1 | each cull, prefix and meshlet pass accumulates into the `Flux_CullCounters` of its (view, phase), a transient buffer cleared before phase 1 |
| 2 | after the view's last culling pass, one `AddReadback` copies the view's counters (2 × 32 bytes) into the readback pool (`Zenith2/Docs/Design/RHI/Memory.md → "Readback pool"`); a full pool defers the readback (BR-DEFER) and the profiler keeps the previous values |
| 3 | `Flux_CullingCounters::Poll` on the build node publishes every resolved readback as the counters of `Zenith2/Docs/Design/RPROD/Diagnostics.md → "Counters"` (per view, per phase), tagged with the frame index they describe; the on-screen profiler shows the newest resolved frame, which is at most `r.framesInFlight` frames old ([Z2-N-REND-657] as cross-doc item X100 (c) rewords it) |

[Z2-N-REND-657] bounds the counters' latency at `r.framesInFlight` frames (X100 (c) applied, reconciliation row RP13): a readback resolves when its frame slot is reused, so step 3 resolves each frame's counters at most `r.framesInFlight` frames after the frame they describe.

## HiZ debug overlay

`r.debug.hiz <mip|auto> <max|min>` (development builds) enables the `HiZDebugOverlay` fullscreen pass on the main or selected editor view, after `DebugOverlay`: it samples the chosen mip and channel of the view's final pyramid mapped to a false-colour ramp over the view's near–far range, and draws the screen rectangles of culled candidates from `m_xDebugBounds` (set by the cull passes when `m_uDebugFlags` bit 0 is set) coloured by the phase that culled them (phase 1 red, phase 2 orange, frustum grey) ([Z2-N-REND-658]). The overlay adds no pass and no field write when off.

<!-- elaboration -->
`m_xDebugBounds` is a transient of `16 + 12 × uFLUX_HIZ_OVERLAY_MAX_RECTS` bytes (`uFLUX_HIZ_OVERLAY_MAX_RECTS = 65536`, 786,448 bytes): a `u32` count and three `u32` of padding, then per culled candidate `u16` x0, y0, x1, y1 in output pixels and a `u32` reason (1 phase 1, 2 phase 2, 3 frustum), appended through an atomic cursor that stops at the cap and sets bit 31 of the count, which the overlay's legend reports as "truncated". The cap is a development-only bound on a debug view, so it carries no budget row; the transient exists only in frames where `r.debug.hiz` is on.

## Tables of the passes this file adds

<!-- assumption: pending REND (RP24) -->
Each table below is added to `Zenith2/Docs/Rendering/PerPass/Common.md` by the ticket that lands the pass, in the form of that file's tables ([Z2-N-REND-795], [Z2-N-REND-800]): `CullClear`, `CullSort` and `BucketScatter` under "Culling and HiZ passes", `HiZDebugOverlay` under "Epilogue passes". Until then these tables are this design's and the per-pass file is incomplete by exactly these four passes (reconciliation row RP24).
<!-- /assumption -->

### CullClear

| Property | Value |
|---|---|
| Program | none: four graph clear-buffer passes (`AddClearBuffer`, `FLUX_PASS_KIND_CLEAR_BUFFER`) |
| Declared by | `Flux_AddInstanceCullPhase1`, `Flux_AddInstanceCullPhase2`, `Flux_AddShadowViewCulling` and, Stage 5, `Flux_AddMeshletCullPhase1/2`, before the phase's first `InstanceCull` or `MeshletCull` dispatch |
| Kind | clear buffer (to zero unless the table below names a pattern) |
| Queue | G |
| Capability | all tiers |
| Budget row | within the phase's culling row (`Z2-B-RPROD-005`, `-008`, `-015`, or `Z2-B-LIGHT-002` for shadow views) |
| Permutations | none |

| Cleared buffer | Size |
|---|---|
| `m_xCommandCounters` (X99's name for the per-command counters) | 4 bytes × the view's command count of the bucket kind |
| `m_xDrawCounts` | 4 bytes × the bucket count |
| the survivor cursor | 4 bytes |
| `m_xSortDispatchArgs` | 12 bytes, cleared to `(0, 1, 1)` (a clear-buffer pass with a 12-byte pattern) |

### CullSort

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/Culling/CullSort.slang` → `Flux_CullSortParams` |
| Declared by | the culling feature functions, after `InstanceCull[emit]` (step 4 of "Instance culling dispatch sequence") and after meshlet survivor emission ("Meshlet culling realisation" step 3) |
| Kind | compute indirect: one group of 256 threads per visible command (per meshlet range in the meshlet form), `DispatchIndirect` over the dispatch arguments `InstanceCull[prefix]` wrote |
| Queue | G |
| Capability | all tiers |
| Budget row | within the phase's culling row (`Z2-B-RPROD-005`, `-008`, `-015`) |
| Permutations | `uPERM_KEY` {instance, instanceMeshlet}: 2 keys (the 32-bit instance key of instance culling, the 64-bit (instance, meshlet) key of meshlet culling, Stage 5) |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xDrawArgs` | `Zenith_RHISRV<Zenith_RHIStructBuffer<Flux_IndirectDrawIndexed>>` | read | compute |
| `m_xDrawCounts` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` | read | compute |
| `m_xVisibleInstances` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` | read-write | compute |
| `m_xScratch` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` (transient, the merge-path tiles of ranges above 2,048 entries, 8 bytes × the view's visible-instance capacity) | read-write | compute |
| the indirect-argument buffer | not a field: `m_xSortDispatchArgs` of `InstanceCull`, passed to `AddComputePassIndirect` and declared by it as an `INDIRECT_ARGS` read | indirect read | — |
| data | `uint32 m_uBucketCount`, `uint32 m_uCommandCount`, `uint32 m_uRegionBase` | — | — |

### BucketScatter

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/Culling/BucketScatter.slang` → `Flux_BucketScatterParams` |
| Declared by | `Flux_DrawKeyTable::AddScatterPass(Flux_RenderGraph& xGraph, Flux_FrameShared& xShared)`, called by `Flux_Renderer::BuildGraph` right after `Flux_AddGPUSceneUpdate`, in frames where "Draw keys and command regions" rebuilt the bases; no pass otherwise |
| Kind | compute: one thread per scatter record, 64 threads per group, `(ceil(max(records, buckets) / 64), 1, 1)` groups |
| Queue | G |
| Capability | all tiers |
| Budget row | `Z2-B-RPROD-004` GPU-scene scatter upload GPU (the same kind of work, measured together) |
| Permutations | none: 1 key |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xRecords` | `Zenith_RHISRV<Zenith_RHIStructBuffer<Flux_SlotCommandScatter>>` (upload ring: `u32` material slot, then its 16-byte `Flux_SlotCommandRecord`, 20 bytes) | read | compute |
| `m_xBaseUpload` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` (upload ring, `u32` per bucket) | read | compute |
| `m_xSlotCommands` | `Zenith_RHIUAV<Zenith_RHIStructBuffer<Flux_SlotCommandRecord>>` (persistent import) | write | compute |
| `m_xBucketCommandBase` | `Zenith_RHIUAV<Zenith_RHIByteBuffer>` (persistent import) | write, `[FullCoverage]` of the bucket range | compute |
| data | `uint32 m_uRecordCount`, `uint32 m_uBucketCount` | — | — |

### HiZDebugOverlay

| Property | Value |
|---|---|
| Program | `Zenith2/Shaders/Passes/Debug/HiZOverlay.slang` → `Flux_HiZOverlayParams` |
| Declared by | `void Flux_AddHiZDebugOverlay(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_RGTexture& xOutput)` (`Internal/Culling/Flux_HiZDebugOverlay.cpp`), called by `Flux_BuildCommonEpilogue` after `Flux_AddDebugOverlay` for the view `r.debug.hiz` selects |
| Kind | raster, output extent: a fullscreen triangle (step `pyramid`), then one indirect instanced quad draw over the rectangle list (step `rectangles`, instance count = the list's count) |
| Queue | G |
| Capability | all tiers; development builds |
| Budget row | `Z2-B-DEVT-003` Debug overlays and on-screen profiler GPU |
| Permutations | `uPERM_STEP` {pyramid, rectangles}: 2 keys |

| Field | C++ type | Access | Stages |
|---|---|---|---|
| `m_xHiZ` | `Zenith_RHISRV<Zenith_RHITex2D>` (the view's final pyramid) | read | pixel |
| `m_xDebugBounds` | `Zenith_RHISRV<Zenith_RHIByteBuffer>` | read | vertex |
| colour 0 | `m_xOutput`, `LOAD`, `STORE`, alpha blend | write | — |
| data | `uint32 m_uMip` (`0xFFFFFFFF` = auto), `uint32 m_uChannel` (0 max, 1 min), `float m_fNear`, `float m_fFar`, `Zenith_RHIFloat2 m_xOutputExtent` | — | — |

## Culling failure behaviour

| Failure | Behaviour |
|---|---|
| a view's candidate count exceeds its transient sizing | cannot happen: sizes come from the counts the CPU uploaded for the frame |
| the bucket registry is full | the slot is not drawn; `Flux.Culling.BucketsFull` |
| a material slot's command base is stale during a rebuild frame | cannot happen: `BucketScatter` precedes every cull pass through a graph edge |
| NaN or infinite bounds | the candidate is kept (a degenerate projection, [Z2-N-REND-641]); the instance renders, and the validation of GEOM's reader rejects NaN bounds at load |
| an invalid history | phase 1 has no candidates; phase 2 tests everything against a pyramid of `+∞` ([Z2-N-REND-646]) |
| a meshlet overflow | the fallback of "Meshlet culling realisation" step 5 |
| a frame whose upload need the ring cannot serve within the cap | RHI's frame deferral; no culling data of the frame is written and the visibility-bit history does not advance |
| a frame whose upload need exceeds the cap | RHI's frame failure `FLUX_FRAME_FAILURE_UPLOAD_OVER_CAP`: nothing recorded, submitted or presented, the history unchanged, the owner named in RHI's log and the view in the `flux.culling` warning; 30 consecutive failures exit with code 6 ("Transparent collection and emission") |
| the counter readback pool is full | the counters are one frame older; nothing else changes |
