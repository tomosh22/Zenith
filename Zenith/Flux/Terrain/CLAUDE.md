# Terrain System

## Overview

GPU-driven terrain rendering with LOD streaming and frustum culling. Supports 4,096 chunks (64x64 grid) with 2 LOD levels. Key features:
- GPU compute shader culling with indirect rendering
- Priority-based LOD streaming with 256MB vertex budget
- Always-resident LOD_LOW fallback for guaranteed visibility
- Unified buffer architecture for minimal state changes

## Files

- `Flux_TerrainConfig.h` - Central configuration (grid size, LOD distances, buffer sizes)
- `Flux_Terrain.cpp` / `Flux_TerrainImpl.h` - Rendering coordination, compute culling dispatch (`Flux_TerrainImpl` class)
- `Flux_TerrainStreamingManager.cpp` / `Flux_TerrainStreamingManagerImpl.h` - LOD streaming and buffer management (public API reached via `Zenith_TerrainComponent`; the impl header holds the implementation)
- `Flux_TerrainGPUStructs.h` - GPU-side struct definitions (chunk data, per-LOD offsets/counts)
- `Flux_TerrainPipelineSelect.h` - Pure `(velocity latch, wireframe) -> G-buffer pipeline variant` selector plus the attachment-count contract. `<cstdint>`-only (no Flux headers) so it is unit-testable in every configuration — same shape as `Flux_TerrainExportRect.h`. See *Debug wireframe* below.
- `Flux_TerrainExportRect.h` - Inclusive chunk rectangle for authoring exports; transactional `TryCreate` (invalid bounds leave the caller's rect untouched). Consumed by the terrain editor bake, `Zenith_EditorAutomation`, `Tools/Zenith_Tools_TerrainExport.*` and `ZM_TerrainAuthoring.cpp`.
- `Flux_TerrainVertexQuant.h` - The ONE place a baked terrain position/UV is quantised, and the only thing that knows where they sit in the packed vertex. Joins the on-disk contract (`Core/Zenith_TerrainChunkLayout.h`: which bytes, which box) to the bit layouts (`Flux/Flux_VertexCodec.h`). Used by the exporter, the editor sculpt hook, CityBuilder's carve + stream-in hook and the runtime chunk validator — five producers that would otherwise each carry their own copy of the box.
- `Flux_TerrainSourceGrid.h` - The exporter's source-sample grid as pure arithmetic (`SampleCountForCells` / `SampleCountPerEdge` / `SampleIndex` / `ChunkVertexCount` / `ChunkIndexCount`). `<cstdint>`-only, same shape as the two headers above. See *Every chunk closes on its neighbour* below.
- `Flux_Terrain.Tests.inl` - Unit tests for the pipeline-variant selection, included at the bottom of `Flux_Terrain.cpp` (the module-owns-its-tests idiom)
- `Flux_Terrain_Shaders.h` - Shader program declarations (`Flux_ShaderDecl` + `apxALL`)
- Shaders in `Zenith/Flux/Shaders/Terrain/` (all `.slang`):
  - `Flux_TerrainCulling.slang` - GPU compute shader for frustum culling
  - `Flux_TerrainResetCounters.slang` - GPU compute shader that zeroes the visible-count atomics
  - `Flux_Terrain_ToGBufferVelocity.slang` - the 5-attachment TAA variant of the below: identical shading plus `SV_Target4` (pure camera-reprojection motion vector, since terrain verts are static world-space). A hand-maintained copy, not a shared include.
  - `Flux_Terrain_ToGBuffer.slang` - G-buffer (splatmap 4-material blend). The
    splat blend is expressed as `TerrainSplatSurface`, conformed to the shared
    `ISurfaceModel` seam (`Common/MaterialSurface.slang`) via `extension` — it
    produces a `MaterialSurface` and the G-buffer packing stays outside. See
    `Shaders/SHADER_STYLE.md` → *Interface / Extension Seams*.
  - `Flux_Terrain_ToShadowmap.slang` - shadow-cascade depth

> **The Water shader/pipeline the Terrain feature used to own was DELETED (2026-08-09,
> compressed-vertex Phase 6).** `xWater`, `Flux_Water.slang`, `Generated/Water.h`, the
> `m_xWaterShader`/`m_xWaterPipeline` pair and the engine's pinned water-normal texture
> are all gone. It was never drawn: `m_xWaterPipeline` was built at boot and then
> referenced by nothing — no render-graph pass, no `SetPipeline`, and no code anywhere
> in the tree wrote a water vertex buffer, so the pipeline had no producer and no
> consumer. Its `.slang` had not had a content change since the original Slang port;
> every commit touching it since was a mechanical sweep. Deleting it migrated nothing
> (there were zero callers) and took the shader catalog 70 → 69 programs. **Do not
> reintroduce it as a placeholder** — a water feature starts with a mesh producer.

## Core Architecture

### Grid Layout
Terrain divided into 64x64 = 4,096 chunks. Each chunk is 64 world units square, giving 4,096x4,096 unit total terrain. Chunks indexed as `(x, y)` where both range 0-63.

### LOD System
Two detail levels with distance-based selection:
- **LOD_HIGH (0):** High detail (0-1000m from camera), streamed dynamically
- **LOD_LOW (1):** Low detail (1000m+), always-resident fallback (never evicted)

Aliases: `LOD_HIGHEST_DETAIL = LOD_HIGH`, `LOD_LOWEST_DETAIL = LOD_LOW`, `LOD_ALWAYS_RESIDENT = LOD_LOW`.

Distance thresholds stored squared to avoid sqrt calculations. CPU and GPU use identical thresholds from `Flux_TerrainConfig.h`.

### Unified Buffer Architecture
Single vertex and index buffer per terrain component containing all chunks:

```
[LOD_LOW Always-Resident Region][Streaming Region for LOD_HIGH]
```

**LOD_LOW Region:** Pre-loaded at initialization, fixed position at buffer start. Contains all 4,096 chunks at lowest detail. Never evicted or moved.

**Streaming Region:** 256MB vertex + 64MB index budget. LOD_HIGH chunks allocated/deallocated dynamically as camera moves. Uses best-fit allocator with priority-based eviction.

**Critical Design:** LOD_LOW at start means its absolute offsets never change. Streaming allocator returns relative offsets within streaming region, converted to absolute by adding LOD_LOW size.

## Rendering Pipeline

### Per-frame Indirect-Count Compatibility (Phase 1-8 of the Indirect-Count plan)

**Terrain renders on devices missing `vkCmdDrawIndexedIndirectCount`** via
the backend's zero-padded fallback, NOT via a CPU readback or a silent skip.
The contract is the explicit `Flux_IndirectCountFallback::ZERO_PADDED_TO_MAX`
policy the terrain call site passes to `DrawIndexedIndirectCount` (see
`Flux/Backend/Flux_IndirectDraw.h`). The backend selects an effective mode
per request — `NATIVE_COUNT` on capable devices within `maxDrawIndirectCount`,
`PADDED_MULTI` on no-count-but-multi-draw devices (batches no larger than
the device's legal per-call limit), or `PADDED_SINGLE` when multi-draw is
also unavailable (one `vkCmdDrawIndexedIndirect` call per record).

The frame sequence (the plan's zero-padded invariant):

1. **Reset pass** (`Flux_TerrainResetCounters.slang`): a separate GPU compute
   dispatch that (a) writes `visibleCount[0] = 0` and (b) clears **every
   one of the 4,096 indirect-command records** to the legal no-op (all five
   words zero). The pass is `[numthreads(64,1,1)]`; `ceil(TOTAL_CHUNKS/64)`
   = 64 groups cover the shipping grid, and the shader bounds-checks via
   `GetDimensions` on the bound argument buffer (no embedded `4096`).
2. **Culling pass** (`Flux_TerrainCulling.slang`): unchanged — atomic-append
   compaction of live records into `[0, visibleCount)`. The cleared tail
   `[visibleCount, TOTAL_CHUNKS)` stays all-zero no-ops, so a fixed indexed-
   indirect draw over the entire range is valid even when visibility falls
   to zero. The order is intentionally NOT sorted (atomic append order); the
   shader source comment continues to call this out.
3. **G-buffer draw** (`ExecuteGBuffer`): one `DrawIndexedIndirectCount` call
   passes `TOTAL_CHUNKS`, the named 20-byte ABI stride
   (`uFLUX_INDIRECT_DRAW_INDEXED_BYTE_STRIDE`), named zero offsets, and
   `Flux_IndirectCountFallback::ZERO_PADDED_TO_MAX`. The backend selects
   the effective execution mode; the recorder never splits one global
   counted draw incorrectly across batches against the same count buffer
   (each batch would re-read the full count and overdraw).

> **★ THE ZERO-TAIL INVARIANT IS NOT A HINT.** The reset pass clears all
> 4,096 records every frame before culling writes the live prefix; the
> tail `[visibleCount, TOTAL_CHUNKS)` is therefore always all-zero no-ops.
> A many → few → zero visibility transition cannot replay stale chunks
> because the previous frame's compacted prefix was overwritten with
> zero-records. Rolling back ONLY the reset clear while keeping fixed-max
> drawing re-introduces stale-tail replay. The invariant is pinned by:
>   * the GPU reset shader's full-record zero write;
>   * barrier-graph tests (`RenderGraphResetCountWriteCullCountReadWriteIsWarRaw`,
>     `RenderGraphResetArgumentWriteCullArgumentWriteIsWaw`, and
>     `RenderGraphResetArgumentWriteGBufferIndirectReadCyclic`);
>   * the test-only full-buffer `DownloadBufferData` readback in
>     `TerrainIndirectCompatibility` (Phase 7), which asserts every tail
>     record's five words are zero across many → few → cull-all → many
>     transitions;
>   * the shared 20-byte ABI POD (`Flux_IndirectDrawIndexedCommand`) and
>     its `Flux_ZeroIndirectDrawIndexedCommand` helper used by allocation
>     seeding in `Zenith_TerrainComponent.cpp`.

> **★ INDIRECT-FIRST-INSTANCE + SHADER-DRAW-PARAMETERS ARE A HARD TERRAIN
> MINIMUM.** Terrain's `firstInstance` carries the stable chunk index that
> the vertex shader reads via `SV_StartInstanceLocation` to index
> `LODLevelBuffer[chunkIndex]`. `drawIndirectFirstInstance` and shader draw
> parameters are advertised / enabled / usable state kept DISTINCT in
> `Zenith_Vulkan::CreateDevice` — the device's hard-suitability check in
> `CreatePhysicalDevice` rejects any adapter missing either before
> `vkCreateDevice`, rather than silently forcing an unsupported feature bit
> to `VK_TRUE`. Native count and `multiDrawIndirect` are optional: fixed
> multi-draw batches are used without count, and one-record indirect calls
> are used when multi-draw is also absent. See `Zenith/Vulkan/CLAUDE.md`.

### Frame Update Sequence

**1. CPU Streaming Phase** (`UpdateStreamingForTerrain()`)
- Runs each frame with camera position
- Only considers "active set" (16 chunk radius around camera)
- For each chunk in active set:
  - Calculate distance to camera
  - Select desired LOD based on distance thresholds
  - If LOD_HIGH not resident: stream in from file (max 8 per frame)
- Scan all chunks for eviction candidates:
  - If LOD_HIGH resident but distance > threshold x 1.5: evict (max 16 per frame)
  - Hysteresis prevents thrashing at LOD boundary

**2. GPU Data Upload** (`BuildChunkDataForGPU()`)
- For each chunk, build GPU-side struct:
  - AABB min/max for frustum testing
  - Buffer offsets and counts for each LOD
- Upload to GPU buffer if residency changed (marked dirty during streaming)

**3. GPU Compute Culling** (`ExecuteCulling()`)
- Bind frustum planes (extracted from view-projection matrix)
- Bind chunk data buffer (AABBs + LOD info)
- Bind output buffer for indirect draw commands
- Dispatch compute shader: 64 threads per workgroup, processing all 4,096 chunks

**4. GPU Compute Execution** (Shader: `Flux_TerrainCulling.slang`)
- Each thread processes one chunk
- Test AABB against 6 frustum planes (early-out on near/far)
- If visible: calculate distance to camera, select LOD
- Write `DrawIndexedIndirectCommand` to output buffer
- Atomically increment visible count
- The culling shader writes the shared Slang include's `Flux_TerrainIndexedIndirectCommand` (see
  `Terrain/Flux_TerrainIndirectCommon.slang`), pinned to the C++ twin `Flux_IndirectDrawIndexedCommand`
  in `Flux/Backend/Flux_IndirectDraw.h` by `static_assert`s in `Flux_Terrain.cpp`.

**5. GPU Rendering** (`ExecuteGBuffer()`)
- Select the G-buffer pipeline variant (see *Debug wireframe* below)
- Bind unified vertex/index buffers
- Bind material textures
- Execute `DrawIndexedIndirectCount()` with `TOTAL_CHUNKS`,
  `Flux_IndirectCountFallback::ZERO_PADDED_TO_MAX`, named zero offsets, and
  the named 20-byte stride `uFLUX_INDIRECT_DRAW_INDEXED_BYTE_STRIDE`.
- GPU reads visible count on capable devices (NATIVE_COUNT); otherwise
  the recorder's batch planner emits padded fixed `vkCmdDrawIndexedIndirect`
  calls over the range the caller's zero-tail invariant keeps valid.

### Debug wireframe — and why there are FOUR G-buffer pipelines

The toggle is the `Render/Terrain/Wireframe` debug variable (`Flux_Terrain.cpp`), surfaced
by the terrain component's *Debug Visualization* panel checkbox and by RenderTest's
`--rendertest-wireframe`. `ExecuteGBuffer` resolves it through the pure
`Flux_TerrainSelectGBufferVariant(bVelocity, bWireframe)` in
`Flux_TerrainPipelineSelect.h`, a **2x2** over two INDEPENDENT axes:

| | solid | wireframe |
|---|---|---|
| **latch off** (4 MRTs) | `m_xTerrainGBufferPipeline` | `m_xTerrainWireframePipeline` |
| **latch on** (5 MRTs) | `m_xTerrainGBufferVelocityPipeline` | `m_xTerrainWireframeVelocityPipeline` |

The TAA velocity latch (`IsVelocityMRTActive()`) decides the **attachment count** —
`SetupRenderGraph` reads the same latch when declaring the pass's `.Writes`, and
`SetupTransients` freezes it for the whole graph build. Wireframe decides only the
**rasterizer polygon mode** (`vk::PolygonMode::eLine`). Polygon mode is static pipeline
state in Flux — there is no dynamic-state path for it — which is why each combination
needs its own prebuilt object rather than a runtime flag.

> **★ DO NOT RE-COLLAPSE THIS INTO A NESTED TERNARY.** It used to be
> `bVelocity ? velocity : (bWireframe ? wireframe : solid)`, on the belief that a wireframe
> pipeline had to be 4-attachment and so "couldn't coexist" with the 5-attachment pass. It
> can: wireframe is orthogonal to the attachment set. Because **TAA ships ON**
> (`Flux_Graphics.cpp`, `bRequested = true`), the velocity arm was always taken and the
> wireframe flag was never evaluated — the checkbox did nothing in every default run, in
> every game. `Flux_Terrain.Tests.inl` pins the 2x2, its surjectivity, and the invariant
> that makes the velocity twin legal: **the wireframe axis must never move the attachment
> count**, only the velocity latch may.

Two behaviours that get reported as bugs but are not:

- **With TAA on, wireframe lines shimmer / ghost.** They are 1px high-contrast features
  going through temporal resolve. Motion vectors are still correct (the velocity shader is
  shared), so it will not smear — but launch with `--taa=0` for crisp lines.
- **Lighting looks wrong while wireframe is on.** The wireframe pipelines keep depth
  test/write enabled, so terrain contributes line-only depth and only line pixels fill the
  MRTs; SSAO/SSR/SSGI/deferred lighting degrade accordingly. Pre-existing on the 4-MRT
  path, just visible far more often now that the toggle works.

### Every chunk closes on its neighbour — and the grid must have a closing sample

A baked chunk is `uCells x uCells` quads but `(uCells+1)^2` VERTICES: the exporter
writes the `uCells x uCells` interior, then **stitches** a closing `+X` vertex column,
a closing `+Z` vertex row, and one corner vertex, taken from the FIRST column/row of
the neighbouring chunk. That shared edge is what makes adjacent chunks seamless.

A quad grid therefore needs `chunks * cells + 1` source samples per edge — one more
than the heightmap has columns at that density. `GenerateFullTerrain` emits that
closing row/column (heightmap coordinate CLAMPED to the last texel, the same clamp its
bilinear tap already applies), which also lands the terrain's outer boundary exactly on
`CHUNK_SIZE_WORLD * CHUNK_GRID_SIZE` (4096) rather than one sample short of it.

> **★ DO NOT REINTRODUCE A BORDER SPECIAL CASE IN `ExportChunkBatch`.** The source grid
> used to be exactly `chunks * cells` samples, so the last chunk column (`x == 63`) and
> row (`z == 63`) had no neighbour to stitch from and their stitch was skipped behind
> `if (x < uNumSplitsX - 1)` / `if (z < uNumSplitsZ - 1)`. Those **127 of 4096** chunks
> baked with **unwritten stitch vertices** (raw `Allocate()` memory — `0xCDCDCDCD`
> serialized straight into the asset, which then poisoned the chunk AABB through
> `GenerateAABBFromVertices`) and **`(0,0,0)` index triples**.
> `Zenith_TerrainComponent`'s chunk-topology validator rejects exactly that, so all 127
> were dropped from the always-resident LOW LOD **and** from the combined physics mesh:
> **no geometry and no collision on the outer +X/+Z strip of every full-grid terrain**,
> in RenderTest and CityBuilder alike. (Zenithmon was spared only because its recipes
> export INTERIOR rects.) It presented as RenderTest's smoke
> `RENDERTEST_SMOKE_FAIL: terrain[0] has 127 LOW zero-count chunks`, and a cold re-bake
> did NOT fix it — the exporter reproduced it byte-for-byte every time.
>
> The fix is the closing sample above, not a relaxed validator: relaxing the validator
> re-admits the uninitialised positions into the culling AABBs. `ExportChunkBatch` now
> asserts its **completeness invariant** (`indexIndex == m_uNumIndices` and every vertex
> slot written) at bake time, and `Flux_Terrain.Tests.inl`'s `FluxTerrainSourceGrid`
> suite pins the arithmetic headlessly — including that a chunk's closing edge IS its
> neighbour's first edge, and that the `(63,63)` corner's closing sample is the last
> sample in the grid.

## Streaming System

### State Management
Each chunk-LOD pair tracks residency state: `NOT_LOADED` or `RESIDENT`. No intermediate states (CPU streaming is synchronous).

### Active Set Optimization
Only chunks within 16-chunk radius of camera considered for streaming updates. Reduces checks from 4,096 to ~1,024 chunks. Active set rebuilt when camera crosses chunk boundaries.

### Stream In Process
When chunk needs LOD_HIGH:
1. Load mesh from file (`Render_X_Y.zmesh`)
2. Allocate space in streaming region via allocator
3. If allocation fails: evict distant chunks until space available
4. Upload vertex/index data to GPU at calculated absolute offset
5. Mark LOD as resident
6. Set "chunk data dirty" flag for next GPU upload

### Eviction Strategy
Priority-based: chunks farthest from camera evicted first. Uses distance x 1.5 hysteresis to prevent thrashing. Eviction frees allocator space immediately for reuse.

### Buffer Allocator
Best-fit allocation on free block list. Maintains priority queue of available blocks sorted by size. Splits blocks on allocation, coalesces on free. Separate allocators for vertices and indices.

## Frustum Culling

### Data Structures

**Chunk Data (GPU):**
- AABB min/max (vec4 each, 6 planes test against this)
- Per-LOD data: buffer offsets and element counts
- Updated when streaming changes residency

**Frustum Planes (GPU):**
- 6 planes: left, right, bottom, top, near, far
- Extracted CPU-side using Gribb-Hartmann method
- Uploaded to GPU as uniform buffer each frame

**Indirect Command Buffer (GPU Output):**
- Array of `Flux_TerrainIndexedIndirectCommand` (Slang) = `Flux_IndirectDrawIndexedCommand` (C++),
  five 32-bit words / 20 bytes — the ABI pinned by `Flux/Backend/Flux_IndirectDraw.h` and
  the `Terrain/Flux_TerrainIndirectCommon.slang` shared include.
- Max 4,096 records (one per chunk), seeded to zero at allocation time by
  `Zenith_TerrainComponent::InitializeCullingResources` (sized via the named
  `uFLUX_INDIRECT_DRAW_INDEXED_BYTE_STRIDE` constant, not a literal `20`).
- Contains: indexCount, instanceCount, firstIndex, vertexOffset (signed int),
  firstInstance (the stable chunk index the vertex shader reads through
  `SV_StartInstanceLocation`).
- The GPU reset pass clears every record every frame (the zero-tail contract).
- GPU reads this at the G-buffer command-processor stage via either the
  native counted call or padded fixed batches.

### Culling Algorithm
For each chunk:
1. Load AABB from buffer
2. Test against near plane (early reject)
3. Test against far plane (early reject)
4. Test against 4 side planes
5. If any plane rejects: mark invisible, exit
6. If visible: calculate camera distance (distance squared)
7. Select LOD based on distance thresholds
8. Write draw command with selected LOD's buffer offsets
9. Atomically increment visible count

### LOD Selection (GPU)
Uses same distance thresholds as CPU (from config header). Critical that these match exactly, or CPU streams wrong LOD for what GPU tries to render.

## Integration Points

### In-Editor Terrain Editing (ZENITH_TOOLS)

`Zenith_TerrainEditor` (`Editor/TerrainEditor/`) sculpts and paints this terrain
live. Its CPU/GPU sync contract rides this system's streaming path exclusively:
it registers `Flux_TerrainStreamingState::m_pfnChunkVertexHook` (re-shapes
chunk verts from its live heightfield on stream-in, before the GPU upload) and
force-evicts edited resident chunks via `EvictLOD` so they re-stream through
the hook. It NEVER writes a resident chunk of the unified vertex buffer in
place. The always-resident LOW LOD stays stale during live editing; the
explicit bake (full re-export + render re-init) refreshes it.

### Scene Component
`Zenith_TerrainComponent` in EntityComponent system:
- Owns unified vertex/index buffers
- Owns separate physics mesh (no LOD, all chunks combined)
- Stores four materials for texture blending (`TERRAIN_MATERIAL_COUNT = 4`; an RGBA8 splatmap selects between the 4 materials)
- Registers buffers with streaming manager on creation
- Calls `UpdateCullingAndLod()` each frame during scene update

### Physics System
Terrain collision uses separate mesh, not render LODs:
- Single combined mesh for all 4,096 chunks (no subdivision) — `LoadCombinedPhysicsGeometry()` loads each `Physics_X_Y.zmesh` and `Flux_MeshGeometry::Combine()`s them into one unified Jolt mesh
- Always resident, never streamed
- Generated at component initialization
- Physics chunks use a density divisor of 4 (17 x 17 vertices / 4 m quads in a 64 m chunk), intentionally lower density than HIGH render chunks (65 x 65 / 1 m). Do not raise it to render density merely to correct a visual issue; choose collision density explicitly and rebake affected terrain assets.

> **★ CHANGING THE DIVISOR IS A BREAKING ASSET CHANGE — BUMP EVERY GAME'S BAKE STAMP IN THE SAME COMMIT.**
> `TryReadTerrainChunkSnapshot` REJECTS any chunk whose vertex/index counts differ from
> `Zenith_TerrainChunkLayout` (`Core/Zenith_TerrainChunkLayout.h` -- the baked-chunk
> file-format contract, moved out of Flux so the EntityComponent chunk loader can read it
> without an edge into Flux), so a stale bake does not degrade — chunk (0,0) fails validation and
> `LoadCombinedPhysicsGeometry` returns with **no physics body at all**. It logs a `Zenith_Error` and
> the game keeps running, so it presents as characters falling through the world rather than as a
> stale asset. Each game gates its bake on its own stamp, and **none of them hash the chunk bytes** —
> Zenithmon's manifest is `(version, file COUNT)` and a density change moves neither, so a stale tree
> reports warm forever. The stamps that must move together:
>
> | Game | Stamp |
> |---|---|
> | Zenithmon | `uZM_TERRAIN_MANIFEST_VERSION` (`Games/Zenithmon/Source/World/ZM_TerrainAuthoring.h`) |
> | CityBuilder | `terrain_hills_vN.marker` (`Games/CityBuilder/CityBuilder.cpp`) |
> | RenderTest | `sk_szTerrainProcMarkerRel` → `terrain_proc_vN.marker` (`Games/RenderTest/RenderTest.cpp`) |
>
> CI cannot catch this: `**/Assets/` is gitignored, so every CI run bakes cold and passes while every
> developer tree with an existing bake silently loses collision. The same applies to any future change
> that rewrites baked chunk BYTES without changing the file count.
>
> **★ IT IS NO LONGER SILENT AT RUNTIME.** `Zenith_ValidateTerrainPhysicsBodies`
> (`Zenith/EntityComponent/Zenith_TerrainPhysicsValidate.h`) runs at every runtime scene load
> (the `m_pfnSceneLoaded` hook) and on editor Stopped->Playing, i.e. before the first physics
> step that could drop a body, and logs one line per terrain:
> `[TerrainPhysics] context='...' terrain='...' physicsGeometry=yes/NO collider=... terrainVolume=... body=yes/NO`.
> **When a fall-through is reported, grep the log for `TerrainPhysics` first** -- `body=NO` names
> this bug outright, and the error line also counts the dynamic bodies that are about to fall.
> A missing/stale bake is a non-fatal `Zenith_Error` (a cold tree is legitimate and
> `Zenith_Assert` breaks in every configuration); "geometry loaded but no body" -- which no
> asset can cause -- asserts.

### Rendering System
Terrain submits separate task each frame:
- Runs on worker thread (parallel with other render tasks)
- Executes streaming, GPU data upload, compute dispatch
- Synchronizes with main render pass via task dependencies

## Key Configuration

All constants in `Flux_TerrainConfig.h`:

**Grid:**
- `CHUNK_GRID_SIZE = 64` (64x64 chunks)
- `CHUNK_SIZE_WORLD = 64.0f` (units per chunk)
- `TERRAIN_SIZE = 4096.0f` (total world size)

**LOD Thresholds (squared):**
- `LOD_HIGH_MAX_DISTANCE_SQ = 1000000.0` (1000m)
- `LOD_LOW_MAX_DISTANCE_SQ = FLT_MAX` (always fallback)

**Streaming Budget:**
- `STREAMING_VERTEX_BUFFER_MB = 256`
- `STREAMING_INDEX_BUFFER_MB = 64`
- `MAX_UPLOADS_PER_FRAME = 8`
- `MAX_EVICTIONS_PER_FRAME = 16`

**Vertex Format (20 bytes, packed):**
- `VERTEX_STRIDE_BYTES = 20` (SNORM16x4 Position + UNORM16x2 UV + SNORM10:10:10:2 Normal + SNORM10:10:10:2 Tangent+BitangentSign). UV holds GLOBAL heightmap pixel coordinates [0, 4096] normalised by that same extent — UNORM16 and not HALF2, because a half mantissa loses sub-integer precision above 1024. There is no per-vertex material lerp; material blending comes from the RGBA8 splatmap.
- **The position is quantised against an AUTHORED box, not a per-chunk AABB.** The box (XZ `[0, 4096]`, Y `[0, MAX_TERRAIN_HEIGHT]`), the byte offsets and the quantisation steps all live with the on-disk contract in `Core/Zenith_TerrainChunkLayout.h`; `Flux/Terrain/Flux_TerrainVertexQuant.h` joins them to `Flux_VertexCodec` and is the ONE place a terrain position or UV is packed (exporter, editor sculpt hook, CityBuilder carve + stream-in hook, chunk validator). A per-chunk box would be tighter but would crack every seam: a chunk's closing edge IS its neighbour's first edge, and only a shared box makes those duplicated world positions quantise to identical words in both chunks.
- Steps: **6.25 cm** in XZ, **7.8 mm** in Y, **1/16 pixel** in UV. Anything comparing a decoded position against its authored value (the chunk-topology validator's cross-stream check) must use those, not an equality epsilon. The `m_pxPositions` attribute stream stays uncompressed `float3`, so physics, the chunk AABBs and the validator's grid-slot test are unaffected by the quantisation.
- The shader half is `Flux_DequantPosition` (`Shaders/Common/VertexFormats.slang`), fed scale/bias from terrain's own constant buffer (`TerrainConstants`, filled once in `Flux_TerrainImpl::Initialise` from `Flux_MakeTerrainPosQuant`). Its agreement with the CPU codec has no runtime tripwire, so it is pinned by a frozen transcription in `Flux_VertexCodec.Tests.inl` (`VertexCodec, DequantPositionMatchesSlangTranscription`).

## Important Constraints

### Buffer Offsets
Allocator returns relative offsets within streaming region. Convert to absolute for GPU by adding LOD_LOW size. Mixing up relative/absolute was source of past bugs.

### CPU/GPU Threshold Sync
Distance thresholds for LOD selection MUST match between CPU (streaming) and GPU (culling). Mismatch causes CPU to stream wrong LOD for what GPU tries to render, falling back to LOD_LOW.

### LOD_LOW Guarantee
LOD_LOW must always be available. If streaming fails or LOD_HIGH not loaded, GPU falls back to LOD_LOW. This prevents holes in terrain but looks low-detail.

### Hysteresis
Eviction uses distance x 1.5 threshold. Prevents thrashing when camera oscillates near LOD boundary. Without hysteresis, same chunks stream in/out repeatedly.

### Active Set Boundary
When camera crosses chunk boundaries, active set rebuilds. Causes spike in streaming activity. Can be smoothed by increasing `ACTIVE_CHUNK_RADIUS` at cost of more per-frame checks.

## Design Rationale

**Why Unified Buffers?** Reduces GPU state changes. All chunks rendered with single pipeline bind, just indirect draw count varies.

**Why LOD_LOW First?** Guarantees fallback available even if streaming fails or budget exhausted. Prevents terrain holes.

**Why GPU Culling?** 4,096 frustum tests parallel on GPU faster than sequential on CPU. Enables indirect rendering.

**Why Distance Squared?** Avoids sqrt() in hot path. Distance comparisons work same with squared values.

**Why Active Set?** Camera rarely sees all 4,096 chunks. Checking 1,024 near camera sufficient, reduces wasted work.

**Why Separate Physics Mesh?** Physics needs watertight mesh, rendering prioritizes visual quality. Different requirements warrant separate representations.
