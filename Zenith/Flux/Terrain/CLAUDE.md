# Terrain System

## Overview

GPU-driven terrain rendering with LOD streaming and frustum culling. Supports 4,096 chunks (64x64 grid) with 2 LOD levels. Key features:
- GPU compute shader culling with indirect rendering
- Priority-based LOD streaming with 256MB vertex budget
- Always-resident LOD_LOW fallback for guaranteed visibility
- Unified buffer architecture for minimal state changes

## Files

- `Flux_TerrainConfig.h` - Central configuration (grid size, LOD distances, buffer sizes)
- `Flux_Terrain.h/cpp` - Rendering coordination, compute culling dispatch
- `Flux_TerrainStreamingManager.h/cpp` - LOD streaming and buffer management
- `Flux_TerrainCulling.comp` - GPU compute shader for frustum culling

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

### Frame Update Sequence

**1. CPU Streaming Phase** (`UpdateStreaming()`)
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

**3. GPU Compute Culling** (`DispatchCullingCompute()`)
- Bind frustum planes (extracted from view-projection matrix)
- Bind chunk data buffer (AABBs + LOD info)
- Bind output buffer for indirect draw commands
- Dispatch compute shader: 64 threads per workgroup, processing all 4,096 chunks

**4. GPU Compute Execution** (Shader: `Flux_TerrainCulling.comp`)
- Each thread processes one chunk
- Test AABB against 6 frustum planes (early-out on near/far)
- If visible: calculate distance to camera, select LOD
- Write `DrawIndexedIndirectCommand` to output buffer
- Atomically increment visible count

**5. GPU Rendering** (`RenderToGBuffer()`)
- Bind terrain pipeline (shaders, render targets)
- Bind unified vertex/index buffers
- Bind material textures
- Execute `DrawIndexedIndirectCount()` using compute shader output
- GPU reads visible count, executes that many draw calls from indirect buffer

## Streaming System

### State Management
Each chunk-LOD pair tracks residency state: `NOT_LOADED` or `RESIDENT`. No intermediate states (CPU streaming is synchronous).

### Active Set Optimization
Only chunks within 16-chunk radius of camera considered for streaming updates. Reduces checks from 4,096 to ~1,024 chunks. Active set rebuilt when camera crosses chunk boundaries.

### Stream In Process
When chunk needs LOD_HIGH:
1. Load mesh from file (`Render_[LOD]_X_Y.zmesh`)
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
- Array of `VkDrawIndexedIndirectCommand`
- One per visible chunk (max 4,096)
- Contains: index count, vertex offset, first index
- GPU reads this for actual rendering

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

### Scene Component
`Zenith_TerrainComponent` in EntityComponent system:
- Owns unified vertex/index buffers
- Owns separate physics mesh (no LOD, all chunks combined)
- Stores two materials for texture blending
- Registers buffers with streaming manager on creation
- Calls `UpdateCullingAndLod()` each frame during scene update

### Physics System
Terrain collision uses separate mesh, not render LODs:
- Single combined mesh for all 4,096 chunks
- Subdivided into 64 regions for Jolt Physics contact handling
- Always resident, never streamed
- Generated at component initialization

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

**Vertex Format (28 bytes, packed):**
- `VERTEX_STRIDE_BYTES = 28` (FLOAT3 Position + HALF2 UV + SNORM10:10:10:2 Normal + SNORM10:10:10:2 Tangent+BitangentSign + FLOAT MaterialLerp)

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
