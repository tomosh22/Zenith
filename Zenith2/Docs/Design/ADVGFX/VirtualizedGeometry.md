# ADVGFX design: virtualized geometry (Stage 7)

Section file of `Zenith2/Docs/Design/ADVGFX.md`. Virtualized geometry (VG) is deferred to Stage 7 (Z2-D-043, Z2-I-ADVGFX-02): a mesh is cooked into a hierarchy of 128-triangle clusters whose level of detail is chosen per cluster group by projected error, streamed in fixed-size pages, culled two-phase against HiZ and rasterised into the visibility buffer, in the family of UE5's Nanite. This file specifies the tree to the depth possible before Stage 7: the cooked format, the cluster DAG cook, streaming, LOD selection, culling, rasterisation, path integration, validation, budgets and the open decision on software rasterisation. No Stage 0 to 6 ticket depends on it.

## Scope of this file

| Topic | Heading |
|---|---|
| when | "VG classification and gating" |
| the asset | "VG cooked format", "Cluster DAG cook" |
| residency | "Cluster page streaming" |
| per frame | "LOD selection", "VG culling", "VG rasterisation", "VG path integration" |
| proof and limits | "VG validation", "VG budgets", "Decision D-VGEO-RASTER" |

## VG classification and gating

| Element | Rule |
|---|---|
| classification | deferred, Stage 7 (Z2-D-043) |
| paths | the visibility-buffer path (desktop) alone draws VG clusters; Forward+, Forward and Deferred, and the S24, draw the mesh's fallback LOD chain, so a VG asset renders on every path |
| ID format | a VG cluster is a meshlet of at most 128 triangles, so the visibility ID's (visible-meshlet index : 25, triangle : 7) layout of [Z2-N-REND-771] holds unchanged |
| opt-in | a mesh is cooked as VG when its import preset sets `mesh.virtualized` (boolean, default false; CONTENT's preset, row AV6); the importer then declares a VG product beside the ordinary mesh product, which becomes the fallback |
| decision | D-VGEO-RASTER (open, Stage 7) rules whether a compute software rasteriser joins the hardware path for small triangles |

## VG cooked format

<!-- assumption: pending ASSET -->
A new kind, requested from ASSET (row AV5 (b)): `ASSET_KIND_VIRTUAL_GEOMETRY`, stable name `Flux.VirtualGeometry`, script name `VirtualGeometry`, runtime type `Flux_VirtualGeometryResource`, owner ADVGFX, extension `.zvgeo`, magic `ZVGE`, memory subsystem RENDERING, default fallback policy SUBSTITUTE, cacheable no, GPU resident yes. Under ruling KIND-CODE-ORDER the code is the next dense code when the Stage 7 kind-adding ticket lands, and that ticket blocks on the last kind-adding ticket before it.
<!-- /assumption -->

`.zvgeo` is ASSET's container with format version 1:

| Section kind | Value | Content |
|---|---|---|
| `ADVGFX_SECTION_VG_INFO` | 60 | 64 bytes: cluster count, group count, page count, root page count, the quantisation box, the UV scale and bias, the stream-set ID, the fallback mesh GUID (16 bytes) |
| `ADVGFX_SECTION_VG_CLUSTERS` | 61 | one 64-byte record per cluster: bounding sphere (16), normal cone (axis as octahedral SNORM16x2, cos cutoff as f16; 8), LOD error (f32), parent error (f32), group index (u32), page index (u32), the cluster's offset and counts inside its page (vertex offset u16, vertex count u8, triangle count u8), material section (u8), flags (u8), reserved |
| `ADVGFX_SECTION_VG_GROUPS` | 62 | one 48-byte record per group: bounds, LOD error, max parent error, first child cluster, child count, the group's page range |
| `ADVGFX_SECTION_VG_HIERARCHY` | 63 | a BVH over groups (32-byte nodes, at most 8 children per node through child lists), the culling traversal's structure |
| `ADVGFX_SECTION_VG_PAGES` | 64 | the pages: each 131,072 raw bytes holding the clusters' vertices in GEOM's default stream formats ([Z2-N-REND-866]) and their 8-bit local indices, compressed alone with a per-page `Zenith_CompressedSectionDesc` in a page table at the section's head (as `.zvt` tiles) |

## Cluster DAG cook

`Cook_VirtualGeometry` (ZenithCook, `Zenith2/Tools/ZenithCook/Internal/VirtualGeometry/`), per product:

| Step | Action |
|---|---|
| 1 | read GEOM's intermediate mesh (`Zenith2/Docs/Design/ASSET/Import.md → "Intermediates"`) and quantise positions and UVs exactly as the mesh cook does, so the VG asset and its fallback decode identically |
| 2 | split each material section into clusters of at most 128 triangles and 128 vertices with meshoptimizer's `meshopt_buildMeshlets` (cone weight 0.25), in a deterministic triangle order |
| 3 | level 0 = the clusters; repeat while the level has more than one group's worth of clusters: (a) partition the level's clusters into groups of 8 … 32 by greedy region growing over the cluster adjacency graph (shared-edge counts; seed = the lowest-index ungrouped cluster; ties by index), (b) merge each group's triangles and simplify them to half the triangle count with `meshopt_simplify` with every vertex on the group's boundary locked, (c) split the simplified triangles into clusters of the next level, (d) set each new cluster's LOD error to `max(simplification error, max child cluster error)` so the error is monotonic up the DAG, and each child's parent error to the new clusters' error |
| 4 | stop when a level holds at most one group; its clusters are the roots |
| 5 | pack clusters into 128 KiB pages, a group's clusters in one page where they fit; root clusters into the root pages |
| 6 | build the group hierarchy BVH (binned SAH over group bounds, 8-wide) |
| 7 | compress pages, write the `.zvgeo` and the cook report entry (`virtualGeometry`: clusters, groups, levels, pages, root pages, triangles per level, bytes) |

The cook is deterministic: meshoptimizer runs single-threaded with fixed parameters and every tie is broken by index (`Test_CookVirtualGeometry_Deterministic`). Locked group boundaries make the LOD selection crack-free ("LOD selection").

## Cluster page streaming

| Element | Rule |
|---|---|
| pool | one persistent-imported page pool buffer, `render.vg.pagePoolBytes` within the pending row "Virtualized geometry page pool memory"; root pages pinned |
| feedback | `VGCulling` appends the page index of every group it wanted to refine but whose child pages are not resident to a request buffer (deduplicated by a per-page bit in a frame bitmap), read back like VT feedback |
| processing | sort, unique, priority by projected error excess then page index; reads through `Zenith_AssetReader::ReadRange` of single pages; decode in sub-jobs; upload through GEOM's uploader in a VG class (row AV3 (f)) |
| eviction | LRU by last-requested frame with an 8-frame grace window; a page is evicted only when no resident page depends on it (child pages before parents) |
| residency rule | a group is drawn at a finer level only when all its child pages are resident, so a missing page only coarsens, never holes |
| cancellation | unload cancels reads and uploads and releases pages after retirement; `Test_VirtualGeometry_LoadUnload1000Cycles` |

## LOD selection

A cluster is drawn when its own projected error is at most τ = `render.vg.errorPixels` (1.0) and its parent error projects above τ. Both errors come from the group records, so every cluster of a group, and the clusters on either side of a locked boundary, decide alike: neighbours never pick incompatible levels, which is the crack-freedom argument of the DAG. Projected error of an error e at bounds sphere (c, r) for a view with vertical projection scale g and render height H: `p = e × g × H / (2 × max(distance(c) − r, near))`.

## VG culling

| Phase | Work |
|---|---|
| instance | the VG instances pass RPROD's instance culling (frustum, two-phase HiZ, [Z2-N-REND-630]) like every instance |
| hierarchy | `VGCulling` traverses the group BVH of each surviving instance with a persistent-thread work queue (a bounded GPU queue of node indices, 64 threads per group), testing node bounds against the frustum and the HiZ pyramid of the phase |
| clusters | each selected cluster is frustum-, cone- and HiZ-tested ([Z2-N-REND-771]'s meshlet culling); survivors are appended to the view's visible-meshlet list with their raster route |
| two phases | phase 1 against last frame's pyramid for clusters visible last frame, phase 2 against this frame's for the rest, as RPROD's meshlet culling (`Zenith2/Docs/Rendering/Culling.md → "Meshlet culling"`) |

## VG rasterisation

| Route | Rule |
|---|---|
| hardware | clusters whose projected triangle edge exceeds `render.vg.softwareEdgePixels` (2), or every cluster when D-VGEO-RASTER (open, Stage 7) rejects the software route: mesh shaders when `m_bMeshShaders`, else an indirect draw per cluster batch from the page pool's index data; writes the 32-bit visibility ID and depth as the visibility raster does |
| software (candidate) | clusters of smaller triangles: a compute rasteriser writing 64-bit `(depth : 32, visibility ID : 32)` with 64-bit image or buffer atomics (`shaderImageInt64Atomics` / SM 6.6 64-bit typed atomics), then a resolve into the visibility and depth targets; available only where the device reports the atomics |
| order | opaque clusters first, then alpha-tested ones, as every depth-writing pass ([Z2-N-REND-722]) |

## VG path integration

| Element | Rule |
|---|---|
| instance flag | the instance record gains `FLUX_INSTANCE_FLAG_VIRTUAL_GEOMETRY` (bit 6, row AV1 (h)); RPROD's buckets skip such instances on the visibility-buffer path, and every other path draws the fallback mesh for them |
| material resolve | unchanged: VG clusters appear in the visible-meshlet list and the resolve reconstructs attributes from the page pool |
| velocity | from the previous transform, as for meshlets |
| shadows | shadow views draw the fallback mesh's coarsest LOD that meets the cascade's texel error (detail at Stage 7) |
| ray tracing | RT bottom-level structures are built from the fallback mesh, never from clusters |

## VG validation

| Evidence | Rule |
|---|---|
| cook tests | `Test_CookVirtualGeometry_ErrorMonotonic` (every parent error ≥ every child error), `Test_CookVirtualGeometry_BoundariesLocked` (no boundary vertex moves), `Test_CookVirtualGeometry_Deterministic`, `Test_CookVirtualGeometry_RootFitsRootPages` |
| crack test | `Test_VirtualGeometry_NoCracksAcrossLevels`: a sphere mesh at a camera distance sweep renders with no background pixel inside its silhouette (the visibility ID never `0xFFFFFFFF` there) on WARP and lavapipe |
| parity | a VG mesh against its LOD-0 fallback at the error threshold, final image within the per-feature tolerance, in the visibility-buffer path parity |
| streaming | cancellation and 1,000 cycles; a camera move that needs pages coarsens until they arrive and never shows holes |
| device runs | W-Indoor and W-OpenWorld with VG content on the RTX 3070 (`needs-human`, `gpu-reason:visual-measure`) |

## VG budgets

Pending rows, named descriptively until the Budgets writer assigns IDs (cross-doc item X422), produced by the Stage 7 budget ticket for ADVGFX:

| Row | Unit | Procedure | Platform |
|---|---|---|---|
| Virtualized geometry page pool memory | MB | MP-SUBMEM | Win |
| Virtualized geometry page uploads per frame | pages | MP-COUNTER | Win |
| Virtualized geometry culling and rasterisation GPU | ms | MP-GPUPASS | Win |

## Decision D-VGEO-RASTER

| Element | Content |
|---|---|
| question | does a compute software rasteriser join the hardware route for clusters of small triangles |
| candidates | (1) hardware route alone (mesh shaders or indirect draws); (2) hardware plus software for clusters below `render.vg.softwareEdgePixels`, on devices with 64-bit atomics |
| measurements | an ordinary harness ticket (`Zenith2/Benchmarks/VirtualGeometry/`, CTest smoke) measures visibility-raster GPU time on a dense VG scene at three triangle densities on the RTX 3070, with each candidate |
| acceptance rule | candidate 2 is adopted when it is at least 20% faster on the densest scene with non-overlapping 95% intervals and is image-space equivalent to candidate 1: on the crack test scene and the three density scenes the final image is within FLIP mean 0.01 of candidate 1's, and no interior hole appears (zero pixels inside a mesh's silhouette that candidate 1 covers and candidate 2 leaves uncovered); the fraction of pixels whose visibility IDs agree is reported, not required, because the two rasterisers differ in sub-pixel precision and fill-rule corners; otherwise candidate 1 |
| consumers blocked | Z2-R-ADVGFX-141 to Z2-R-ADVGFX-150 (the VG tree) |
| nouns | `Flux_VirtualGeometrySystem`, `Flux_VirtualGeometryResource`, `Flux_AddVGRaster`, `Flux_AddVGCulling` |
| ruling | a `needs-human` ruling ticket blocked by the harness ticket (Z2-D-084, Z2-D-097); the name is registered through cross-doc item X423 |
