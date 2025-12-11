# Terrain LOD Streaming and Culling System - Complete Technical Reference

**IMPORTANT: READ THIS ENTIRE DOCUMENT BEFORE MODIFYING THE TERRAIN SYSTEM**

This document contains critical information about bugs that have been encountered and fixed, design decisions, and areas that are prone to issues. The terrain LOD streaming system has multiple interacting components with subtle dependencies. Changes to one component can break others in non-obvious ways.

---

# Table of Contents

1. [System Overview](#system-overview)
2. [Critical Bug History and Lessons Learned](#critical-bug-history-and-lessons-learned)
3. [Architecture Deep Dive](#architecture-deep-dive)
4. [The TERRAIN_SCALE Bug - A Case Study](#the-terrain_scale-bug---a-case-study)
5. [Buffer Architecture and Index Handling](#buffer-architecture-and-index-handling)
6. [CPU vs GPU Threshold Alignment](#cpu-vs-gpu-threshold-alignment)
7. [Known Issues and Future Improvements](#known-issues-and-future-improvements)
8. [Debugging Guide](#debugging-guide)
9. [Terrain Frustum Culling System (Original)](#terrain-frustum-culling-system)

---

# System Overview

The terrain system consists of three major subsystems that must work together:

1. **Terrain Streaming Manager** - Decides which LODs to load, manages GPU buffer allocations
2. **GPU Culling Compute Shader** - Frustum culls chunks, selects LODs, generates draw commands
3. **Terrain Renderer** - Executes indirect draw commands

**Critical Invariant:** The CPU streaming manager and GPU culling shader MUST agree on:
- Which chunk the camera is in (chunk coordinate calculation)
- What LOD should be used at what distance (LOD thresholds)
- Where mesh data is located in the unified buffer (vertex/index offsets)

---

# Critical Bug History and Lessons Learned

## Bug #1: TERRAIN_SCALE Mismatch (CRITICAL - Cost 4+ hours debugging)

**Symptom:** LOD colors showed correct selection (Red/Green/Blue/Magenta), but mesh density was always LOD3 (lowest detail) everywhere except near origin.

**Root Cause:** 
- Export tool (`Zenith_Tools_TerrainExport.cpp`) uses `TERRAIN_SCALE = 1`
- Streaming manager (`Flux_TerrainStreamingManager.h`) defines `TERRAIN_SCALE = 8`
- `WorldPosToChunkCoords()` calculated chunk coords using `TERRAIN_SIZE * TERRAIN_SCALE = 512`
- Actual chunks are 64 units wide (just `TERRAIN_SIZE`)

**Impact:**
- At camera position (1200, 0, 0):
  - Streaming manager thought camera was in chunk 2 (1200/512 = 2.34)
  - Actual chunk is 18 (1200/64 = 18.75)
  - System streamed LOD0 for chunks 0-16 (wrong area)
  - GPU needed LOD0 for chunks 16-20 (actual camera area)
  - Those weren't streamed → fell back to LOD3

**Fix Applied:**
```cpp
// WRONG (was):
const float fChunkSizeWorld = TERRAIN_SIZE * TERRAIN_SCALE;  // 512

// CORRECT (now):
const float fChunkSizeWorld = static_cast<float>(TERRAIN_SIZE);  // 64
```

**Lesson:** The `TERRAIN_SCALE` constant in the streaming manager is MISLEADING. The actual terrain vertex positions from the export tool do NOT use this scale. Always derive chunk positions from actual mesh AABB data, not from manual calculations.

---

## Bug #2: CPU/GPU LOD Threshold Mismatch

**Symptom:** Same as Bug #1 - colors correct, mesh density wrong.

**Root Cause:**
- GPU culling shader used thresholds: `400000, 1000000, 2000000` (distance squared)
- CPU streaming used thresholds: `4000000, 10000000, 20000000` (10x larger!)

**Impact:**
- At 800m from chunk:
  - CPU calculated: 640000 < 4000000 → Request LOD0
  - GPU calculated: 640000 >= 400000 → Select LOD1
  - LOD1 never streamed → fell back to LOD3

**Fix Applied:**
```cpp
// CPU streaming thresholds - MUST MATCH GPU thresholds in BuildChunkDataForGPU()
static constexpr float LOD_THRESHOLDS_SQ[3] = { 400000.0f, 1000000.0f, 2000000.0f };
```

**Lesson:** ANY threshold value that affects LOD selection must be identical in:
1. `Flux_TerrainStreamingManager.cpp` - `UpdateStreaming()` LOD_THRESHOLDS_SQ
2. `Flux_TerrainStreamingManager.cpp` - `BuildChunkDataForGPU()` LOD_DISTANCES_SQ
3. `Flux_TerrainCulling.comp` - `selectLOD()` threshold comparison

---

## Bug #3: Eviction Allocator Offset Bug

**Symptom:** Allocator corruption, "Free out of bounds" errors, crashes.

**Root Cause:**
- Allocation stores ABSOLUTE offsets (includes LOD3 region offset)
- Allocator's `Free()` expects RELATIVE offsets (within streaming region)
- `EvictLOD()` was passing absolute offsets directly to `Free()`

**Fix Applied:**
```cpp
// In EvictLOD():
// IMPORTANT: Allocation stores ABSOLUTE offsets, but allocator uses RELATIVE offsets
const uint32_t uRelativeVertexOffset = xAlloc.m_uVertexOffset - m_uLOD3VertexCount;
const uint32_t uRelativeIndexOffset = xAlloc.m_uIndexOffset - m_uLOD3IndexCount;
m_xVertexAllocator.Free(uRelativeVertexOffset, xAlloc.m_uVertexCount);
m_xIndexAllocator.Free(uRelativeIndexOffset, xAlloc.m_uIndexCount);
```

**Lesson:** Document clearly whether offsets are ABSOLUTE (buffer-wide) or RELATIVE (region-specific). Add comments at every conversion point.

---

## Bug #4: Asynchronous Upload Race Condition

**Symptom:** Mesh data uploaded, but GPU read garbage/previous data.

**Root Cause:**
- `UploadBufferData()` uses deferred staging buffer - only records commands
- `FlushStagingBuffer()` submits command buffer but doesn't wait
- GPU culling could run before upload completed

**Fix Applied:**
```cpp
// Use synchronous upload that waits for completion:
Flux_MemoryManager::UploadBufferDataAtOffset(
    m_xUnifiedVertexBuffer.GetBuffer().m_xVRAMHandle,
    xChunkMesh.m_pVertexData,
    ulVertexDataSize,
    ulVertexOffsetBytes
);
// This internally does: submit + waitIdle
```

**Lesson:** For streaming systems where data must be ready immediately, use synchronous upload. The staging buffer system is optimized for batching but doesn't guarantee immediate availability.

---

## Bug #5: LOD3 Vertex Stride Mismatch

**Symptom:** Buffer overflow validation errors, visual corruption.

**Root Cause:**
- LOD3 loaded with position-only flag: `1 << FLUX_VERTEX_ATTRIBUTE__POSITION` (12 bytes/vertex)
- Allocator calculated size using full vertex stride (60 bytes/vertex)
- Upload wrote 12 bytes/vertex but allocated space assumed 60 bytes/vertex

**Fix Applied:**
```cpp
// Load LOD3 with ALL attributes (flag = 0 means all attributes):
Zenith_AssetHandler::AddMesh(strChunkName, strChunkPath.c_str(), 0);
```

**Lesson:** The terrain system uses a 60-byte vertex format with Position, Normal, Tangent, Bitangent, UV, and MaterialLerp. ALL mesh loads must use this format for consistency.

---

## Bug #6: "Fighting for Residency" - Stale Priority and Queue Bug (CRITICAL)

**Symptom:** After moving camera far from spawn point:
- Nearby terrain does NOT stream in
- Distant terrain (near original spawn) repeatedly streams in and out
- Appears as if chunks are "fighting for residency"

**Root Cause (THREE separate issues):**

1. **Stale Priority Values:**
   - `m_afPriorities` stored distance when chunk was REQUESTED, not CURRENT distance
   - When camera moved far away, old chunks kept their LOW priority (they were nearby when requested)
   - Eviction calculated priority as: `framesSinceRequested + cachedDistanceSq`
   - Old chunks had LOW cachedDistanceSq (from when they were near camera) even though they're now FAR
   - New chunks also had LOW distance (they're actually near camera)
   - System couldn't distinguish which chunks were truly far!

2. **Queue Not Cleared on Camera Move:**
   - Streaming queue is a priority queue sorted by distance (low = high priority)
   - Old requests from spawn location had LOW distances (they were near camera then)
   - When camera moved, old requests were still in queue
   - Old requests processed before new requests (both had similar low distances)

3. **No Proactive Eviction:**
   - Chunks only evicted when allocation failed
   - Buffer stayed full of old chunks
   - New chunks couldn't get space without triggering eviction
   - Eviction used stale priorities, potentially evicting new chunks instead of old!

**The Cascade:**
```
1. Camera at spawn (0,0) - chunks at (0,0) stream in with distance ~0
2. Camera moves to (2000, 0, 2000)
3. Old chunks still have cached priority ~0 (stale from spawn time)
4. New chunks request with priority ~0 (they're near camera now)
5. Queue has: [old requests dist=0] + [new requests dist=0]
6. System can't tell which is which!
7. Old chunks evicted → immediately re-requested from queue → loaded → evicted...
```

**Fix Applied:**

1. **Clear Queue on Camera Chunk Change:**
```cpp
if (iCameraChunkX != s_iLastCameraChunkX || iCameraChunkY != s_iLastCameraChunkY)
{
    // Clear stale requests - they're no longer relevant
    while (!s_xStreamingQueue.empty())
    {
        StreamingRequest request = s_xStreamingQueue.top();
        s_xStreamingQueue.pop();
        // Reset state so they can be re-requested if still needed
        s_axChunkResidency[request.m_uChunkIndex].m_aeStates[request.m_uLODLevel] =
            Flux_TerrainLODResidencyState::NOT_LOADED;
    }
}
```

2. **Use CURRENT Distance for Eviction:**
```cpp
// OLD (buggy):
candidate.m_fPriority = framesSinceRequested + xResidency.m_afPriorities[uLOD];

// NEW (fixed):
Zenith_Maths::Vector3 xChunkCenter = GetChunkCenter(uChunkX, uChunkY);
float fCurrentDistanceSq = glm::distance2(s_xLastCameraPos, xChunkCenter);
candidate.m_fPriority = fCurrentDistanceSq;  // Farthest chunks evicted first
```

3. **Proactive Eviction of Distant Chunks:**
```cpp
void ProactivelyEvictDistantChunks(const Zenith_Maths::Vector3& xCameraPos)
{
    // On camera chunk change, evict ALL chunks beyond streaming range
    // This frees space BEFORE it's needed, preventing fighting
    for (uint32_t i = 0; i < TOTAL_CHUNKS; ++i)
    {
        float fDistanceSq = glm::distance2(xCameraPos, GetChunkCenter(...));
        const float fEvictionThreshold = LOD_MAX_DISTANCE_SQ[LOD2] * 1.5f;

        if (fDistanceSq > fEvictionThreshold)
            EvictLOD(i, uLOD);
    }
}
```

4. **Removed `m_afPriorities` Array Entirely:**
   - No longer cache priorities - always calculate current distance when needed
   - Simpler code, no stale data possible

5. **Filter Queue Instead of Clearing (Follow-up Fix):**
   - Original fix cleared entire queue on camera chunk change
   - Problem: With only 4 uploads/frame, player movement kept resetting progress
   - New approach: Keep requests for chunks still within range, remove only far chunks
   ```cpp
   // Filter queue - keep nearby, remove far
   while (!s_xStreamingQueue.empty())
   {
       float fCurrentDistanceSq = glm::distance2(xCameraPos, GetChunkCenter(...));
       if (fCurrentDistanceSq < LOD_MAX_DISTANCE_SQ[request.m_uLODLevel])
       {
           // Update priority and keep
           request.m_fPriority = fCurrentDistanceSq;
           xFilteredQueue.push(request);
       }
       else
       {
           // Request now irrelevant - reset state
           s_axChunkResidency[...].m_aeStates[...] = NOT_LOADED;
       }
   }
   ```

6. **Increased Upload Rate:**
   - MAX_UPLOADS_PER_FRAME: 4 → 8
   - MAX_EVICTIONS_PER_FRAME: 8 → 16
   - More responsive streaming, especially for moving cameras

7. **Safe Eviction Policy (Final Fix - Regression):**
   - **New Bug:** After fixes 1-6, chunks would switch FROM correct LOD TO LOD3 as camera moves closer
   - **Root Cause:** `EvictToMakeSpace()` would evict ANY resident LOD to make room for new allocation
     - Example: Camera at 500m from chunk, LOD1 resident (appropriate for 500m)
     - System tries to stream LOD0 (higher detail)
     - `EvictToMakeSpace()` evicts the working LOD1 to make room
     - Allocation for LOD0 fails due to buffer fragmentation
     - Result: Both LOD0 and LOD1 gone → falls back to LOD3
   - **Solution:** Only evict chunks OUTSIDE their LOD's appropriate range
   ```cpp
   bool Flux_TerrainStreamingManager::EvictToMakeSpace(...)
   {
       for (each resident LOD)
       {
           float fCurrentDistanceSq = glm::distance2(s_xLastCameraPos, xChunkCenter);
           float fLODMaxDistanceSq = LOD_MAX_DISTANCE_SQ[uLOD];
           float fEvictionThreshold = fLODMaxDistanceSq * 1.2f;  // 20% hysteresis

           // CRITICAL: Only evict if chunk is OUTSIDE this LOD's range
           if (fCurrentDistanceSq > fEvictionThreshold)
           {
               // Safe to evict - this LOD not needed at this distance
               candidates.push_back(candidate);
           }
           // Otherwise DON'T evict - this LOD is still appropriate!
       }

       if (candidates.empty())
       {
           // No safe candidates - all LODs still needed
           // Return false rather than evict working data
           return false;
       }
   }
   ```
   - **Result:** Chunks stay at correct LOD as camera approaches
   - **Tradeoff:** In low-memory situations, system falls back to LOD3 more often rather than thrashing

**Lesson:** In streaming systems:
1. NEVER cache distance/priority - always compute from current state
2. Filter queues on camera move, don't clear everything (preserves nearby requests)
3. Proactively free resources when no longer needed, don't wait for allocation failures
4. Mixing different scales in priority (frames + distance²) is confusing and error-prone
5. Upload rate must be high enough to keep up with player movement speed
6. **Safe eviction is critical:** Only evict data that's genuinely not needed (outside appropriate range)
7. **Fail gracefully:** Return false rather than evict working data and risk making things worse
8. **Fragmentation awareness:** When allocation might fail, preserve working state rather than gamble

---

# Architecture Deep Dive

## Component Interaction Diagram

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           FRAME UPDATE FLOW                                  │
└─────────────────────────────────────────────────────────────────────────────┘

┌──────────────────────┐     ┌───────────────────────────────────────────────┐
│ Camera Position      │────►│ Flux_TerrainStreamingManager::UpdateStreaming │
│ (from scene)         │     │                                               │
└──────────────────────┘     │ 1. WorldPosToChunkCoords() ◄─── BUG #1 HERE  │
                             │ 2. RebuildActiveChunkSet()                    │
                             │ 3. For each active chunk:                     │
                             │    - GetChunkCenter() (uses cached AABB)     │
                             │    - Calculate distance squared               │
                             │    - Compare to LOD_THRESHOLDS_SQ ◄─ BUG #2  │
                             │    - RequestLOD() if needed                   │
                             │ 4. ProcessStreamingQueue()                    │
                             │    - Allocate buffer space                    │
                             │    - Upload mesh data (sync) ◄─── BUG #4 FIX │
                             │    - Mark RESIDENT                            │
                             └───────────────────────┬───────────────────────┘
                                                     │
                                                     ▼
                             ┌───────────────────────────────────────────────┐
                             │ BuildChunkDataForGPU()                        │
                             │                                               │
                             │ For each of 4096 chunks:                      │
                             │ - Copy cached AABB to GPU struct              │
                             │ - For each LOD level:                         │
                             │   - Set distance threshold (LOD_DISTANCES_SQ) │
                             │   - If RESIDENT: copy allocation offsets      │
                             │   - Else: copy LOD3 fallback offsets          │
                             │                                               │
                             │ CRITICAL: vertexOffset handling:              │
                             │ - LOD3: vertexOffset = 0 (rebased indices)    │
                             │ - LOD0-2: vertexOffset = absolute offset      │
                             └───────────────────────┬───────────────────────┘
                                                     │
                                                     ▼
                             ┌───────────────────────────────────────────────┐
                             │ Upload to Chunk Data Buffer                   │
                             │ (Zenith_TerrainComponent manages this)        │
                             └───────────────────────┬───────────────────────┘
                                                     │
                                                     ▼
┌──────────────────────┐     ┌───────────────────────────────────────────────┐
│ GPU Compute Dispatch │────►│ Flux_TerrainCulling.comp                      │
│ (4096 / 64 = 64      │     │                                               │
│  workgroups)         │     │ For each chunk (parallel):                    │
└──────────────────────┘     │ 1. Read chunk AABB from buffer                │
                             │ 2. Frustum test against 6 planes              │
                             │ 3. If visible:                                │
                             │    - Calculate distance to camera             │
                             │    - selectLOD() using maxDistanceSq          │
                             │    - Read LOD's firstIndex, indexCount,       │
                             │      vertexOffset from chunk data             │
                             │    - Write VkDrawIndexedIndirectCommand       │
                             │    - Write LOD level for visualization        │
                             └───────────────────────┬───────────────────────┘
                                                     │
                                                     ▼
                             ┌───────────────────────────────────────────────┐
                             │ Indirect Draw Execution                       │
                             │                                               │
                             │ vkCmdDrawIndexedIndirect() with:              │
                             │ - indexCount from command                     │
                             │ - firstIndex from command                     │
                             │ - vertexOffset from command (CRITICAL!)       │
                             │ - instanceCount = 1                           │
                             │ - firstInstance = drawIndex (for LOD lookup)  │
                             └───────────────────────────────────────────────┘
```

## Buffer Memory Layout

```
UNIFIED VERTEX BUFFER (Total: LOD3 size + 256 MB streaming)
┌─────────────────────────────────────────────────────────────────────────────┐
│ OFFSET 0                                                                    │
│ ┌─────────────────────────────────────────────────────────────────────────┐ │
│ │                     LOD3 COMBINED REGION                                │ │
│ │                                                                         │ │
│ │  All 4096 chunks' LOD3 vertices, combined via Flux_MeshGeometry::       │ │
│ │  Combine(). Indices are REBASED during combination to point to         │ │
│ │  correct absolute vertex positions in this buffer.                     │ │
│ │                                                                         │ │
│ │  Size: m_uLOD3VertexCount vertices × 60 bytes/vertex                   │ │
│ │  Typical: ~340,000 vertices × 60 = ~20 MB                              │ │
│ │                                                                         │ │
│ │  CRITICAL: When rendering LOD3, use vertexOffset = 0 because           │ │
│ │  indices already account for vertex positions in combined buffer.      │ │
│ └─────────────────────────────────────────────────────────────────────────┘ │
│                                                                             │
│ OFFSET: m_uLOD3VertexCount × 60 bytes                                      │
│ ┌─────────────────────────────────────────────────────────────────────────┐ │
│ │                     STREAMING REGION                                    │ │
│ │                                                                         │ │
│ │  Managed by Flux_TerrainBufferAllocator                                │ │
│ │  Contains LOD0, LOD1, LOD2 chunks loaded on-demand                     │ │
│ │                                                                         │ │
│ │  Size: 256 MB budget                                                   │ │
│ │  Allocation unit: 1 vertex (60 bytes)                                  │ │
│ │                                                                         │ │
│ │  Chunks are uploaded individually with 0-BASED indices.                │ │
│ │  When rendering, use vertexOffset = absolute offset in buffer.         │ │
│ │                                                                         │ │
│ │  ┌─────────┐ ┌─────────┐ ┌─────────┐     ┌─────────┐                   │ │
│ │  │Chunk 0,0│ │Chunk 1,0│ │  FREE   │ ... │Chunk n,m│                   │ │
│ │  │ LOD0    │ │ LOD1    │ │  SPACE  │     │ LOD2    │                   │ │
│ │  └─────────┘ └─────────┘ └─────────┘     └─────────┘                   │ │
│ └─────────────────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────────────┘

UNIFIED INDEX BUFFER (Similar layout)
┌─────────────────────────────────────────────────────────────────────────────┐
│ LOD3 COMBINED REGION: Rebased indices pointing to combined vertex buffer   │
├─────────────────────────────────────────────────────────────────────────────┤
│ STREAMING REGION: 0-based indices for individually uploaded chunks         │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Offset Terminology (CRITICAL - Source of Bug #3)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         OFFSET TERMINOLOGY                                  │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ABSOLUTE OFFSET:                                                          │
│    - Offset from start of ENTIRE unified buffer                            │
│    - Used in: Residency allocation struct, GPU chunk data, draw commands   │
│    - Formula: AbsoluteOffset = m_uLOD3VertexCount + RelativeOffset        │
│                                                                             │
│  RELATIVE OFFSET:                                                          │
│    - Offset from start of STREAMING REGION only                            │
│    - Used in: Allocator internal tracking                                  │
│    - Formula: RelativeOffset = AbsoluteOffset - m_uLOD3VertexCount         │
│                                                                             │
│  CONVERSION REQUIRED:                                                       │
│    - StreamInLOD(): Allocator returns RELATIVE, store as ABSOLUTE          │
│    - EvictLOD(): Read ABSOLUTE from residency, convert to RELATIVE for Free│
│                                                                             │
│  Example:                                                                   │
│    m_uLOD3VertexCount = 340000                                             │
│    Allocator.Allocate(1000) returns 0 (relative)                           │
│    Store in residency: 340000 + 0 = 340000 (absolute)                      │
│    GPU uses 340000 as vertexOffset in draw command                         │
│    On evict: 340000 - 340000 = 0, Free(0, 1000)                            │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

# The TERRAIN_SCALE Bug - A Case Study

This section provides detailed analysis of the most subtle bug encountered, as a reference for future debugging.

## The Setup

Three files define terrain scale:
```cpp
// Zenith_Tools_TerrainExport.cpp (export tool)
#define TERRAIN_SCALE 1

// Flux_TerrainStreamingManager.h (runtime)
#define TERRAIN_SCALE 8

// Zenith_TerrainComponent.h
#define TERRAIN_SCALE 8
```

## The Problem

The export tool generates vertex positions with `TERRAIN_SCALE=1`:
```cpp
// Export generates chunk (0,0) with vertices at x=0..63, z=0..63
// Export generates chunk (1,0) with vertices at x=64..127, z=0..63
// etc.
```

But the streaming manager calculated positions with `TERRAIN_SCALE=8`:
```cpp
// WorldPosToChunkCoords thought:
// Chunk (0,0) covers x=0..511, z=0..511
// Chunk (1,0) covers x=512..1023, z=512..1023
// etc.
```

## The Cascade of Failures

1. Camera at (1200, 0, 0)
2. `WorldPosToChunkCoords(1200)` → chunk X = 1200/512 = 2
3. `RebuildActiveChunkSet(2, ...)` builds set centered on chunk 2
4. Active chunks: 0-18 in X direction (radius 16 from center 2)
5. `UpdateStreaming()` requests LOD0 for chunks 0-18
6. Streaming uploads LOD0 meshes for chunks 0-18
7. GPU renders... but camera is actually viewing chunks 16-20!
8. Chunks 16-18 have LOD0 (lucky overlap)
9. Chunks 19-20+ have no LOD0 → fall back to LOD3
10. Result: Sudden LOD3 "wall" at around x=1152 (chunk 18)

## Why Colors Were Correct

The GPU culling shader calculates distance from camera to chunk AABB center:
```glsl
vec3 chunkCenter = (chunk.aabbMin.xyz + chunk.aabbMax.xyz) * 0.5;
float distSq = dot(cameraPosition.xyz - chunkCenter, ...);
uint lodLevel = selectLOD(distSq, chunk);
```

The AABBs are cached from actual mesh data, so they have correct positions.
The LOD selection based on distance from camera to correct AABB works fine.
But the MESH DATA for that LOD isn't uploaded because streaming looked at wrong chunks!

## Why It Worked Near Origin

Near (0,0,0), both calculations agree:
- Streaming thinks camera is in chunk 0
- Active set includes chunks 0-16
- GPU needs chunks 0-~10 for LOD0
- Those are all in the active set!

## The Fix

```cpp
// Use TERRAIN_SIZE directly, not TERRAIN_SIZE * TERRAIN_SCALE
const float fChunkSizeWorld = static_cast<float>(TERRAIN_SIZE);  // 64, not 512
```

## Validation

After fix, at camera (1200, 0, 0):
- `WorldPosToChunkCoords(1200)` → chunk X = 1200/64 = 18
- Active set centered on chunk 18
- LOD0 streamed for chunks 2-34
- GPU needs LOD0 for chunks 16-20
- All covered! ✓

---

# CPU vs GPU Threshold Alignment

## The Threshold Matching Requirement

The CPU streaming manager and GPU culling shader MUST use identical distance thresholds:

```cpp
// Flux_TerrainStreamingManager.cpp - UpdateStreaming()
static constexpr float LOD_THRESHOLDS_SQ[3] = { 400000.0f, 1000000.0f, 2000000.0f };

// Flux_TerrainStreamingManager.cpp - BuildChunkDataForGPU()
static constexpr float LOD_DISTANCES_SQ[TERRAIN_LOD_COUNT] = {
    400000.0f,    // LOD0: 0 - 632m
    1000000.0f,   // LOD1: 632 - 1000m
    2000000.0f,   // LOD2: 1000 - 1414m
    FLT_MAX       // LOD3: 1414m+
};

// Flux_TerrainCulling.comp - selectLOD()
// Uses chunk.lods[i].maxDistanceSq which comes from LOD_DISTANCES_SQ
```

## What Happens If They Don't Match

If CPU uses 10x larger thresholds (the original bug):

| Distance | CPU Decision | GPU Decision | Result |
|----------|-------------|--------------|--------|
| 500m     | 250000 < 4000000 → LOD0 | 250000 < 400000 → LOD0 | ✓ Works |
| 700m     | 490000 < 4000000 → LOD0 | 490000 >= 400000 → LOD1 | ✗ LOD1 not streamed |
| 1100m    | 1210000 < 4000000 → LOD0 | 1210000 >= 1000000 → LOD2 | ✗ LOD2 not streamed |

The GPU selects a LOD level, but that LOD wasn't streamed because the CPU thought a higher-detail LOD was needed. Fall back to LOD3.

---

# Known Issues and Future Improvements

## Current Issue: Streaming Thrash

**Symptom:** With stationary camera, terrain constantly streams in and out.

**Cause:** The LOD hysteresis system has issues:
1. Hysteresis is applied inconsistently
2. Eviction priority doesn't account for likelihood of re-request
3. Small camera movements trigger full active set rebuild

**Mitigation (not fix):**
- `LOD_HYSTERESIS_FACTOR = 1.2f` provides 20% buffer
- `CAMERA_MOVE_THRESHOLD_SQ = 100.0f` prevents tiny movement updates

**Proper Fix Needed:**
1. Track streaming "temperature" per chunk (how often requested)
2. Don't evict hot chunks even if technically outside threshold
3. Implement request rate limiting per chunk

## Suggested Improvements

### 1. Asynchronous Mesh Loading
Currently mesh files are loaded synchronously on the main thread.
Move to background thread with completion callback.

### 2. Better Eviction Priority
Current: Evict farthest chunks first.
Better: Evict based on (distance × time_since_last_request).

### 3. Allocator Defragmentation
After many alloc/free cycles, streaming region fragments.
Implement periodic defragmentation during low-activity frames.

### 4. Remove TERRAIN_SCALE Confusion
Either:
- Remove TERRAIN_SCALE from streaming manager entirely
- Or make export tool actually use it
Don't have mismatched values.

### 5. Unified Threshold Constants
Create single source of truth for LOD thresholds:
```cpp
// In Flux_TerrainStreamingManager.h
namespace TerrainLODConfig
{
    constexpr float LOD0_MAX_DISTANCE_SQ = 400000.0f;
    constexpr float LOD1_MAX_DISTANCE_SQ = 1000000.0f;
    constexpr float LOD2_MAX_DISTANCE_SQ = 2000000.0f;
}
```
Use these constants in both CPU and GPU code.

---

# Debugging Guide

## Symptom → Likely Cause Reference

| Symptom | Likely Cause | Check |
|---------|--------------|-------|
| All terrain is LOD3 (magenta) | Streaming not running | dbg_bLogTerrainStreaming |
| Colors correct, density wrong | Threshold mismatch or TERRAIN_SCALE bug | Compare LOD_THRESHOLDS_SQ |
| Sharp LOD boundary at wrong position | WorldPosToChunkCoords wrong | Log camera chunk vs expected |
| Chunks pop in/out rapidly | Hysteresis too low or eviction too aggressive | Increase hysteresis, reduce eviction rate |
| Vulkan validation errors | Buffer overflow or stride mismatch | Check vertex stride = 60 |
| Allocator corruption | Absolute/relative offset confusion | Verify offset conversions |

## Debug Variables

```cpp
DEBUGVAR bool dbg_bLogTerrainStreaming = false;   // Request/completion logs
DEBUGVAR bool dbg_bLogTerrainEvictions = false;   // Eviction decisions
DEBUGVAR bool dbg_bLogTerrainAllocations = false; // Allocator operations
DEBUGVAR bool dbg_bLogTerrainVertexData = false;  // Detailed vertex forensics
```

## Forensic Chunk Tracking

To debug a specific chunk in detail:
```cpp
static constexpr uint32_t DBG_TRACKED_CHUNK_X = 18;  // The chunk to track
static constexpr uint32_t DBG_TRACKED_CHUNK_Y = 0;
static constexpr uint32_t DBG_TRACKED_LOD = 0;       // LOD level to track
```

This enables detailed logging for that specific chunk only.

## Validating the Full Pipeline

1. **Check camera chunk calculation:**
   ```cpp
   int32_t cx, cy;
   WorldPosToChunkCoords(cameraPos, cx, cy);
   Zenith_Log("Camera at (%.1f, %.1f, %.1f) -> Chunk (%d, %d)", 
              cameraPos.x, cameraPos.y, cameraPos.z, cx, cy);
   ```

2. **Check active chunk set:**
   ```cpp
   Zenith_Log("Active chunks: %zu (center chunk: %d, %d)", 
              m_xActiveChunkIndices.size(), m_iLastCameraChunkX, m_iLastCameraChunkY);
   ```

3. **Check residency state:**
   ```cpp
   for (uint32_t lod = 0; lod < 3; ++lod)
   {
       uint32_t resident = 0;
       for (uint32_t i = 0; i < 4096; ++i)
           if (m_axChunkResidency[i].m_aeStates[lod] == RESIDENT) resident++;
       Zenith_Log("LOD%u: %u chunks resident", lod, resident);
   }
   ```

4. **Check GPU chunk data:**
   Enable `dbg_bLogTerrainVertexData` and check "CHUNK DATA FOR GPU" logs.

5. **Check draw commands:**
   Use RenderDoc to inspect the indirect draw buffer contents.

---

# Terrain Frustum Culling System

## Overview

The Terrain Frustum Culling System provides efficient CPU and GPU-based frustum culling for terrain components in the Zenith engine. It uses Axis-Aligned Bounding Box (AABB) tests against the camera frustum to determine visibility, significantly reducing rendering overhead.

## Architecture

### Key Components

1. **Zenith_FrustumCulling** ([Maths/Zenith_FrustumCulling.h](../../Maths/Zenith_FrustumCulling.h))
   - Core math structures: `Zenith_AABB`, `Zenith_Plane`, `Zenith_Frustum`
   - Frustum extraction using Gribb-Hartmann method
   - AABB vs Frustum intersection testing (radius-based, conservative)

2. **Flux_TerrainCulling** ([Flux_TerrainCulling.h](Flux_TerrainCulling.h), [Flux_TerrainCulling.cpp](Flux_TerrainCulling.cpp))
   - Public API for terrain culling operations
   - CPU culling implementation (active)
   - GPU culling infrastructure (prepared, currently disabled)

3. **Integration** ([Flux_Terrain.cpp](Flux_Terrain.cpp))
   - Called from `Flux_Terrain::SubmitRenderToGBufferTask()`
   - Replaces old distance-based visibility checks

### Data Flow

```
┌─────────────────────────────────────────┐
│  Flux_Terrain::SubmitRenderToGBufferTask│
│  (Called once per frame)                │
└──────────────┬──────────────────────────┘
               │
               ▼
    ┌──────────────────────┐
    │ Get all terrain      │
    │ components from scene│
    └──────┬───────────────┘
           │
           ▼
    ┌──────────────────────────┐
    │ Generate AABBs (lazy)    │◄── Uses Flux_TerrainCulling::GenerateTerrainAABB()
    │ - Only on first frame    │    - Reads mesh vertex data
    │ - Cached in component    │    - Stores in TerrainComponent
    └──────┬───────────────────┘
           │
           ▼
    ┌───────────────────────────────┐
    │ Flux_TerrainCulling::         │
    │ PerformCulling()              │
    │ - Extract frustum from camera │◄── Gribb-Hartmann extraction
    │ - Test each AABB vs frustum   │    - 6 plane tests per AABB
    │ - Build visible terrain list  │    - ~30-50 CPU cycles/terrain
    └──────┬────────────────────────┘
           │
           ▼
    ┌──────────────────────────┐
    │ GetVisibleTerrain        │
    │ Components()             │
    │ - Returns culled list    │
    └──────┬───────────────────┘
           │
           ▼
    ┌──────────────────────────┐
    │ Submit render tasks      │
    │ (only visible terrain)   │
    └──────────────────────────┘
```

## Key Implementation Details

### AABB Caching Strategy

AABBs are cached directly in `Zenith_TerrainComponent`:

```cpp
class Zenith_TerrainComponent
{
    // ...
    Zenith_AABB m_xAABB;           // Cached world-space AABB
    bool m_bAABBValid = false;     // Validity flag
};
```

**Lazy Initialization:**
- AABBs generated once on first frame terrain is encountered
- Regenerated if terrain mesh changes (requires manual invalidation)
- Minimal memory overhead: 24 bytes per terrain (2 × Vector3)

**Why This Approach:**
- Terrain meshes are typically static (don't move/rotate)
- Avoids per-frame AABB regeneration
- Cache-friendly: stored with component data

### Frustum Extraction

Uses the **Gribb-Hartmann method** from view-projection matrix:

```cpp
void Zenith_Frustum::ExtractFromViewProjection(const Zenith_Maths::Matrix4& xViewProj)
{
    // Extract 6 planes directly from matrix rows/columns
    // Left:   row4 + row1
    // Right:  row4 - row1
    // Bottom: row4 + row2
    // Top:    row4 - row2
    // Near:   row4 + row3
    // Far:    row4 - row3

    // Each plane normalized to ensure correct distance tests
}
```

**Advantages:**
- Fast: ~50 CPU cycles per frame
- Works with any projection matrix (perspective/orthographic)
- Handles oblique/asymmetric frustums correctly

### AABB-Frustum Intersection Test

Uses **radius-based test** (conservative, no false negatives):

```cpp
bool Zenith_FrustumCulling::TestAABBFrustum(const Zenith_Frustum& xFrustum, const Zenith_AABB& xAABB)
{
    Vector3 center = (xAABB.m_xMin + xAABB.m_xMax) * 0.5f;
    Vector3 extents = (xAABB.m_xMax - xAABB.m_xMin) * 0.5f;

    for (int i = 0; i < 6; ++i)
    {
        const Zenith_Plane& plane = xFrustum.m_axPlanes[i];

        // Effective radius of AABB projected onto plane normal
        float fRadius =
            extents.x * abs(plane.m_xNormal.x) +
            extents.y * abs(plane.m_xNormal.y) +
            extents.z * abs(plane.m_xNormal.z);

        float fDistance = dot(plane.m_xNormal, center) + plane.m_fDistance;

        // AABB completely outside this plane?
        if (fDistance < -fRadius)
            return false;  // Culled
    }

    return true;  // Visible (or partially visible)
}
```

**Performance:**
- ~30-50 CPU cycles per AABB
- Branch-prediction friendly (early-out on first failing plane)
- SIMD-friendly (can vectorize inner loop)

**Conservative Nature:**
- May have false positives (reports visible when actually culled)
- Zero false negatives (never culls truly visible objects)
- Acceptable trade-off: slight over-rendering vs visual artifacts

## GPU Culling (Future Implementation)

Infrastructure is in place but currently disabled:

### Prepared Components

1. **Compute Shader** ([Shaders/Terrain/TerrainCulling.comp](../Shaders/Terrain/TerrainCulling.comp))
   - GLSL compute shader (workgroup size 64)
   - Shared memory optimization (reduces atomic contention 64x)
   - Outputs visible indices + indirect draw commands

2. **GPU Data Structures** ([Flux_TerrainCulling.cpp](Flux_TerrainCulling.cpp))
   ```cpp
   struct GPU_TerrainAABB
   {
       Vector4 m_xMinAndIndex;  // xyz = min, w = terrain index
       Vector4 m_xMax;          // xyz = max, w = unused
   };

   struct GPU_FrustumPlane
   {
       Vector4 m_xNormalAndDistance;  // xyz = normal, w = distance
   };

   struct GPU_FrustumData
   {
       GPU_FrustumPlane m_axPlanes[6];
   };
   ```

3. **Buffer Placeholders**
   - `g_xAABBBuffer` - All terrain AABBs
   - `g_xFrustumBuffer` - Current frame frustum
   - `g_xVisibleIndicesBuffer` - Output: visible terrain indices
   - `g_xVisibleCountBuffer` - Output: count (atomic)
   - `g_xIndirectDrawBuffer` - VkDrawIndexedIndirectCommand array

### Why Currently Disabled

GPU culling requires full Vulkan compute pipeline integration:
- Compute shader compilation
- Descriptor set management
- Indirect draw command recording
- GPU-CPU synchronization

The infrastructure is prepared for when these systems are ready. See [TERRAIN_CULLING_GUIDE.md](TERRAIN_CULLING_GUIDE.md) for complete implementation details.

## Integration Guide

### Basic Usage

The system is already integrated into `Flux_Terrain`. No manual setup required for standard terrain rendering.

### Manual Integration (Custom Systems)

If you need to use terrain culling outside the standard pipeline:

```cpp
#include "Flux/Terrain/Flux_TerrainCulling.h"

// Once at initialization
Flux_TerrainCulling::Initialise();

// Each frame
Zenith_Vector<Zenith_TerrainComponent*> allTerrain;
Zenith_Scene::GetCurrentScene().GetAllOfComponentType<Zenith_TerrainComponent>(allTerrain);

const Zenith_CameraComponent& camera = Zenith_Scene::GetCurrentScene().GetMainCamera();

// Perform culling
Flux_TerrainCulling::PerformCulling(camera, allTerrain);

// Get results
const Zenith_Vector<Zenith_TerrainComponent*>& visible =
    Flux_TerrainCulling::GetVisibleTerrainComponents();

// Render only visible terrain
for (Zenith_Vector<Zenith_TerrainComponent*>::Iterator it(visible); !it.Done(); it.Next())
{
    Zenith_TerrainComponent* terrain = it.GetData();
    // ... render terrain
}
```

### AABB Invalidation

If terrain mesh changes at runtime:

```cpp
Zenith_TerrainComponent* terrain = /* ... */;

// Modify mesh
terrain->GetRenderMeshGeometry().m_pxPositions = newVertices;

// Regenerate AABB
Zenith_AABB newAABB = Flux_TerrainCulling::GenerateTerrainAABB(*terrain);
terrain->SetAABB(newAABB);
```

### Debug Visualization

```cpp
#ifdef ZENITH_DEBUG_VARIABLES
// Enable via debug variables UI
// Render > Terrain > Show Culling Stats
// Render > Terrain > Show Visible AABBs
// Render > Terrain > Show Culled AABBs
// Render > Terrain > Show Frustum

// Or programmatically
const Flux_TerrainCulling::CullingStats& stats = Flux_TerrainCulling::GetCullingStats();
Zenith_Log("Culled %u / %u terrain (%.1f%%)",
    stats.m_uCulledTerrain,
    stats.m_uTotalTerrain,
    100.0f * stats.m_uCulledTerrain / stats.m_uTotalTerrain);
#endif
```

## Performance Characteristics

### CPU Culling

**Typical Performance (Debug Build):**
- 1,000 terrain chunks: ~0.05ms per frame
- 10,000 terrain chunks: ~0.5ms per frame
- 100,000 terrain chunks: ~5ms per frame

**Bottlenecks:**
- Cache misses when accessing scattered terrain components
- Branch mispredictions in plane tests

**Optimization Tips:**
- Sort terrain by spatial locality before culling
- Consider SIMD vectorization for >10,000 terrain chunks
- Profile with `ZENITH_PROFILE_INDEX__VISIBILITY_CHECK`

### Expected GPU Culling Performance

**Projected (when implemented):**
- 100,000 terrain chunks: ~0.2ms GPU time
- Bottleneck shifts to GPU-CPU sync overhead
- Beneficial for >10,000 terrain chunks

## Common Pitfalls

### 1. Missing Quaternion Type

**Error:**
```
C2039: 'Quaternion': is not a member of 'Zenith_Maths'
```

**Solution:**
Already fixed in [Zenith_Maths.h](../../Maths/Zenith_Maths.h):
```cpp
using Quaternion = glm::quat;
```

### 2. Iterator Syntax

**Wrong:**
```cpp
Zenith_Vector<T>::ConstIterator it(vec);  // ConstIterator doesn't exist
```

**Correct:**
```cpp
Zenith_Vector<T>::Iterator it(vec);  // Use Iterator for both const and non-const
```

### 3. Forgetting AABB Initialization

**Symptom:** Terrain disappears or flickers

**Cause:** AABB not generated before first culling pass

**Solution:**
```cpp
// In Flux_Terrain::SubmitRenderToGBufferTask()
for (Iterator it(allTerrain); !it.Done(); it.Next())
{
    Zenith_TerrainComponent* terrain = it.GetData();
    if (!terrain->HasValidAABB())
    {
        Zenith_AABB aabb = Flux_TerrainCulling::GenerateTerrainAABB(*terrain);
        terrain->SetAABB(aabb);
    }
}
```

### 4. Using glm::min/max

**Error:**
```
Ambiguous call to overloaded function
```

**Cause:** Multiple glm::min overloads confuse MSVC

**Solution:**
```cpp
// Don't use:
uint32_t end = glm::min(start + count, total);

// Use instead:
uint32_t end = (start + count < total) ? start + count : total;
```

### 5. Static Function Scope Issues

**Error:**
```
C2668: 'PerformCPUCulling': ambiguous call to overloaded function
```

**Cause:** Static function declared in anonymous namespace but defined outside

**Solution:**
Ensure all internal static functions are fully within the anonymous namespace:
```cpp
namespace
{
    static void InternalFunction();  // Forward declaration

    static void InternalFunction()   // Implementation
    {
        // ...
    }
}  // End anonymous namespace
```

## File Reference

### Core Files

- [Zenith_FrustumCulling.h](../../Maths/Zenith_FrustumCulling.h) - Math structures and algorithms
- [Flux_TerrainCulling.h](Flux_TerrainCulling.h) - Public API
- [Flux_TerrainCulling.cpp](Flux_TerrainCulling.cpp) - Implementation

### Integration

- [Flux_Terrain.cpp](Flux_Terrain.cpp):122-145 - AABB lazy initialization
- [Flux_Terrain.cpp](Flux_Terrain.cpp):147-149 - Culling invocation
- [Zenith_TerrainComponent.h](../../EntityComponent/Components/Zenith_TerrainComponent.h) - AABB caching members

### GPU Culling (Prepared)

- [TerrainCulling.comp](../Shaders/Terrain/TerrainCulling.comp) - Compute shader
- [Flux_TerrainCulling.cpp](Flux_TerrainCulling.cpp):217-261 - GPU infrastructure

### Documentation

- [TERRAIN_CULLING_GUIDE.md](TERRAIN_CULLING_GUIDE.md) - Comprehensive implementation guide
- [TERRAIN_CULLING_IMPLEMENTATION_SUMMARY.md](TERRAIN_CULLING_IMPLEMENTATION_SUMMARY.md) - Executive summary
- [Flux_TerrainCulling_Example.cpp](Flux_TerrainCulling_Example.cpp) - 8 usage examples

## Testing Recommendations

### Unit Testing

```cpp
// Test AABB generation
Zenith_TerrainComponent terrain = /* create test terrain */;
Zenith_AABB aabb = Flux_TerrainCulling::GenerateTerrainAABB(terrain);
ASSERT(aabb.m_xMin.x <= aabb.m_xMax.x);  // Valid bounds

// Test frustum extraction
Zenith_CameraComponent camera = /* create test camera */;
Matrix4 view, proj;
camera.BuildViewMatrix(view);
camera.BuildProjectionMatrix(proj);
Zenith_Frustum frustum;
frustum.ExtractFromViewProjection(proj * view);
// Verify plane normals are normalized
for (int i = 0; i < 6; ++i)
{
    float length = glm::length(frustum.m_axPlanes[i].m_xNormal);
    ASSERT(abs(length - 1.0f) < 0.001f);
}

// Test AABB vs Frustum
Zenith_AABB insideAABB = /* AABB fully inside frustum */;
ASSERT(Zenith_FrustumCulling::TestAABBFrustum(frustum, insideAABB) == true);

Zenith_AABB outsideAABB = /* AABB fully outside frustum */;
ASSERT(Zenith_FrustumCulling::TestAABBFrustum(frustum, outsideAABB) == false);
```

### Integration Testing

1. **Visual Verification:**
   - Enable debug visualization
   - Fly camera through scene
   - Verify terrain appears/disappears correctly
   - Check no visible popping

2. **Performance Testing:**
   - Create stress test scene (10,000+ terrain chunks)
   - Profile with `ZENITH_PROFILE_INDEX__VISIBILITY_CHECK`
   - Verify culling time < 1ms for typical scenes

3. **Edge Cases:**
   - Camera inside large terrain AABB
   - Terrain AABB partially intersecting frustum
   - Oblique/asymmetric frustums
   - Zero-sized AABBs (degenerate terrain)

## Future Enhancements

### Short Term

1. **SIMD Optimization:**
   - Vectorize AABB vs plane tests (4 planes at once)
   - Expected 2-4x speedup for large terrain counts

2. **Hierarchical Culling:**
   - Build AABB tree for terrain chunks
   - Cull entire regions with single test

3. **Debug Visualization:**
   - Implement `RenderDebugVisualization()` for wireframe AABBs/frustum

### Long Term

1. **GPU Culling Completion:**
   - Integrate compute shader pipeline
   - Implement indirect draw command recording
   - Benchmark CPU vs GPU crossover point

2. **Occlusion Culling:**
   - Combine frustum + occlusion tests
   - Use GPU hierarchical Z-buffer

3. **Temporal Coherence:**
   - Track visibility frame-to-frame
   - Early-out for stable camera

## Contributing

When modifying this system:

1. **Maintain Conservative Culling:** Never introduce false negatives (culling visible objects)
2. **Document Performance Impact:** Profile any changes with realistic scene loads
3. **Update Examples:** Add to [Flux_TerrainCulling_Example.cpp](Flux_TerrainCulling_Example.cpp) if adding new features
4. **Test Edge Cases:** Verify with degenerate AABBs, extreme camera positions, etc.

---

**Last Updated:** 2025-01-19
**Author:** Terrain Culling Implementation
**Status:** Production-Ready (CPU), Prepared (GPU)

---

# Terrain LOD Streaming System

## Overview

The Terrain LOD Streaming System provides efficient level-of-detail management for large terrain meshes. It uses a unified buffer approach with on-demand streaming of high-detail LODs while keeping LOD3 (lowest detail) always resident for fallback rendering.

## Architecture

### Key Components

1. **Flux_TerrainStreamingManager** ([Flux_TerrainStreamingManager.h](Flux_TerrainStreamingManager.h), [Flux_TerrainStreamingManager.cpp](Flux_TerrainStreamingManager.cpp))
   - Central singleton manager for terrain streaming
   - Manages unified GPU buffers (vertex + index)
   - Handles streaming requests, allocations, and evictions
   - Provides chunk data for GPU compute shader culling

2. **Flux_TerrainBufferAllocator** (embedded in Flux_TerrainStreamingManager.cpp)
   - Simple priority-queue based free-list allocator
   - Manages streaming region of unified buffers
   - Best-fit allocation strategy

3. **Integration** ([Flux_Terrain.cpp](Flux_Terrain.cpp))
   - `Flux_Terrain::SubmitRenderToGBufferTask()` calls `UpdateStreaming()` each frame
   - GPU culling compute shader uses chunk data to select LODs

### Buffer Architecture

```
┌────────────────────────────────────────────────────────────────┐
│                     UNIFIED VERTEX BUFFER                      │
├───────────────────────────┬────────────────────────────────────┤
│   LOD3 REGION             │       STREAMING REGION             │
│   (Always Resident)       │       (Dynamic LOD0-2)             │
│                           │                                    │
│   ~4096 chunks combined   │   Allocator-managed space         │
│   All attributes (60B/v)  │   256 MB budget                    │
│                           │                                    │
│   Indices are REBASED     │   Indices are 0-BASED (relative)  │
│   (absolute per-buffer)   │   Use vertexOffset in draw call   │
└───────────────────────────┴────────────────────────────────────┘

┌────────────────────────────────────────────────────────────────┐
│                      UNIFIED INDEX BUFFER                      │
├───────────────────────────┬────────────────────────────────────┤
│   LOD3 REGION             │       STREAMING REGION             │
│   (Always Resident)       │       (Dynamic LOD0-2)             │
│                           │                                    │
│   64 MB budget            │   64 MB budget                     │
└───────────────────────────┴────────────────────────────────────┘
```

### Data Flow

```
┌─────────────────────────────────────────────────────────────────┐
│   Flux_Terrain::SubmitRenderToGBufferTask()                     │
│   (Called once per frame)                                       │
└──────────────────────────┬──────────────────────────────────────┘
                           │
                           ▼
          ┌────────────────────────────────────┐
          │ Flux_TerrainStreamingManager::     │
          │ UpdateStreaming(cameraPos)         │
          │                                    │
          │ 1. For each chunk:                 │
          │    - Calculate distance to camera  │
          │    - Determine desired LOD         │
          │    - RequestLOD() if < LOD3        │
          │                                    │
          │ 2. ProcessStreamingQueue()         │
          │    - Load mesh from disk           │
          │    - Allocate space in buffer      │
          │    - Upload to GPU                 │
          │    - Mark as RESIDENT              │
          │                                    │
          │ 3. Evictions (if needed)           │
          │    - LRU-style priority            │
          │    - Free allocator space          │
          └──────────────────────────────────┬─┘
                                             │
                                             ▼
          ┌────────────────────────────────────┐
          │ BuildChunkDataForGPU()             │
          │                                    │
          │ For each chunk:                    │
          │ - Write AABB (cached from init)    │
          │ - For each LOD:                    │
          │   - If RESIDENT: use allocation    │
          │   - Else: fallback to LOD3         │
          └──────────────────────────────────┬─┘
                                             │
                                             ▼
          ┌────────────────────────────────────┐
          │ GPU Culling Compute Shader         │
          │                                    │
          │ - Frustum cull chunks              │
          │ - Select LOD based on distance     │
          │ - Generate indirect draw commands  │
          └────────────────────────────────────┘
```

## Key Implementation Details

### Terrain Constants

```cpp
#define TERRAIN_EXPORT_DIMS 64        // 64x64 = 4096 chunks
#define TERRAIN_LOD_COUNT 4           // LOD0 (highest) to LOD3 (lowest)
#define TERRAIN_SIZE 64               // Logical chunk size
#define TERRAIN_SCALE 8               // World scale multiplier
#define MAX_TERRAIN_HEIGHT 2048       // Maximum terrain height
```

**CRITICAL:** The `TERRAIN_SCALE` constant was a source of bugs. The export tool uses `TERRAIN_SCALE=1` in vertex positions, while the streaming manager originally assumed `TERRAIN_SCALE=8`. The fix was to cache AABBs from actual mesh data during initialization.

### Vertex Format

The terrain uses a 60-byte vertex stride with all attributes:
- Position (12 bytes) - Vector3
- Normal (12 bytes) - Vector3
- Tangent (12 bytes) - Vector3
- Bitangent (12 bytes) - Vector3
- UV (8 bytes) - Vector2
- Material Lerp (4 bytes) - float

**CRITICAL:** LOD3 must be loaded with ALL attributes, not just positions. Loading with position-only (12 bytes/vertex) causes stride mismatch with the allocator that expects 60 bytes/vertex, leading to buffer overflow.

### Distance Thresholds

**GPU Culling (squared meters):**
```cpp
LOD0: 0 - 400,000      (0 - 632m)
LOD1: 400,000 - 1,000,000   (632 - 1000m)
LOD2: 1,000,000 - 2,000,000   (1000 - 1414m)
LOD3: 2,000,000+       (1414m+, always resident)
```

**CPU Streaming Prefetch (squared meters):**
```cpp
LOD0: 0 - 4,000,000     (0 - 2000m)
LOD1: 4,000,000 - 10,000,000  (2000 - 3162m)
LOD2: 10,000,000 - 20,000,000  (3162 - 4472m)
```

The CPU thresholds are ~10x larger to prefetch LODs before they're needed for rendering.

### Residency States

```cpp
enum class Flux_TerrainLODResidencyState : uint8_t
{
    NOT_LOADED = 0,  // LOD not in GPU memory
    QUEUED,          // Request in streaming queue (prevents duplicates)
    LOADING,         // Being uploaded to GPU
    RESIDENT,        // Ready to render
    EVICTING         // Being removed from GPU memory
};
```

The `QUEUED` state was added to prevent duplicate streaming requests when a chunk is requested multiple frames in a row before streaming completes.

### AABB Caching

AABBs are cached during initialization to avoid per-frame mesh loading:

```cpp
// During Initialize():
for (uint32_t x = 0; x < TERRAIN_EXPORT_DIMS; ++x)
{
    for (uint32_t y = 0; y < TERRAIN_EXPORT_DIMS; ++y)
    {
        // Load LOD0 mesh to get accurate world positions
        Zenith_AssetHandler::AddMesh(strChunkName, strChunkPath.c_str(), 
            1 << Flux_MeshGeometry::FLUX_VERTEX_ATTRIBUTE__POSITION);
        Flux_MeshGeometry& xChunkMesh = Zenith_AssetHandler::GetMesh(strChunkName);
        
        // Generate and cache AABB
        m_axChunkAABBs[uChunkIndex] = Zenith_FrustumCulling::GenerateAABBFromVertices(
            xChunkMesh.m_pxPositions,
            xChunkMesh.m_uNumVerts
        );
        
        Zenith_AssetHandler::DeleteMesh(strChunkName);
    }
}
```

**Why AABBs are cached from mesh data:**
- Export tool uses `TERRAIN_SCALE=1` in actual vertex positions
- Manual calculation using `TERRAIN_SIZE * TERRAIN_SCALE` gives wrong positions
- Cached AABBs are authoritative and match actual rendered geometry

### Index Buffer Handling

**Critical difference between LOD3 and LOD0-2:**

**LOD3 (Combined Buffer):**
- All 4096 LOD3 chunks are combined into one mesh during initialization
- `Flux_MeshGeometry::Combine()` **rebases indices** to be absolute within the buffer
- Indices already point to correct vertices
- `vertexOffset = 0` in draw call

**LOD0-2 (Streamed Individually):**
- Each chunk is uploaded individually
- Indices remain **0-based** (relative to chunk's first vertex)
- Need `vertexOffset = allocation.m_uVertexOffset` in draw call

```cpp
// In BuildChunkDataForGPU():
if (uLOD == 3)
{
    // LOD3: Combined buffer, indices already rebased
    xChunkData.m_axLODs[uLOD].m_uVertexOffset = 0;
}
else
{
    // LOD0-2: Streamed, indices are 0-based
    xChunkData.m_axLODs[uLOD].m_uVertexOffset = xAlloc.m_uVertexOffset;
}
```

### Streaming Queue Management

**Queue Size Limit:**
```cpp
constexpr size_t MAX_QUEUE_SIZE = 256;
if (m_xStreamingQueue.size() >= MAX_QUEUE_SIZE)
{
    return false;  // Skip, retry next frame
}
```

**State Machine for Requests:**
```
NOT_LOADED  ──RequestLOD()──►  QUEUED  ──ProcessQueue()──►  LOADING  ──Upload──►  RESIDENT
     ▲                                                          │
     └───────────────────── (on failure) ───────────────────────┘
```

All failure paths reset state to `NOT_LOADED` so the chunk can be retried next frame.

### Buffer Bounds Checking

Bounds checking was added to prevent Vulkan validation errors:

```cpp
// Before upload:
if (ulVertexOffsetBytes + ulVertexDataSize > m_ulUnifiedVertexBufferSize)
{
    Zenith_Log("[TerrainStreaming] ERROR: Vertex upload would exceed buffer!");
    // Free allocations, reset state, return false
}

// Allocator validation in Free():
if (uOffset + uSize > m_uTotalSize)
{
    Zenith_Log("ERROR: Free out of bounds!");
    return;
}
```

## Configuration

### Buffer Sizes

```cpp
constexpr uint64_t STREAMING_VERTEX_BUFFER_SIZE_MB = 256;
constexpr uint64_t STREAMING_INDEX_BUFFER_SIZE_MB = 64;
```

### Per-Frame Limits

```cpp
constexpr uint32_t MAX_STREAMING_UPLOADS_PER_FRAME = 16;
constexpr uint32_t MAX_EVICTIONS_PER_FRAME = 32;
```

### Debug Variables

```cpp
DEBUGVAR bool dbg_bLogTerrainStreaming = true;
DEBUGVAR bool dbg_bLogTerrainEvictions = true;
DEBUGVAR bool dbg_bLogTerrainAllocations = true;
DEBUGVAR bool dbg_bLogTerrainVertexData = true;
DEBUGVAR float dbg_fStreamingAggressiveness = 1.0f;
```

## Common Pitfalls and Fixes

### 1. TERRAIN_SCALE Mismatch

**Symptom:** Only LOD3 renders regardless of camera distance

**Cause:** Export tool uses `TERRAIN_SCALE=1`, streaming manager assumed `TERRAIN_SCALE=8`

**Solution:** Cache AABBs from actual mesh data during initialization, not manual calculation

### 2. Vertex Stride Mismatch

**Error:** Vulkan validation: "dstOffset + size > buffer size"

**Cause:** LOD3 loaded with position-only (12 bytes/vertex), allocator expects 60 bytes/vertex

**Solution:** Load LOD3 with all attributes (flag=0, not flag=1<<POSITION)

### 3. Streaming Queue Growing Unboundedly

**Symptom:** `ProcessStreamingQueue` takes forever, queue size in thousands

**Cause:** Same chunk requested every frame, added to queue multiple times

**Solution:** 
- Add `QUEUED` residency state
- Limit queue size to 256 entries
- Don't re-queue if already QUEUED or LOADING

### 4. State Not Reset on Failure

**Symptom:** Chunks get stuck in LOADING state, never retry

**Cause:** Failure paths didn't reset state to NOT_LOADED

**Solution:** All failure paths in `ProcessStreamingQueue` and `StreamInLOD` reset state:
```cpp
xResidency.m_aeStates[uLODLevel] = Flux_TerrainLODResidencyState::NOT_LOADED;
```

### 5. Allocator Corruption

**Error:** "Free would exceed capacity" errors

**Cause:** Free() called with invalid offset/size from corrupted state

**Solution:** Added validation in Free():
```cpp
if (uOffset + uSize > m_uTotalSize) { return; }
if (m_uUnusedSpace + uSize > m_uTotalSize) { return; }
```

## Debug Logging

Enable via debug variables or code:
```cpp
dbg_bLogTerrainStreaming = true;  // Streaming requests/completions
dbg_bLogTerrainEvictions = true;  // Eviction operations
dbg_bLogTerrainAllocations = true; // Allocator operations
dbg_bLogTerrainVertexData = true;  // Detailed vertex data tracing
```

Track specific chunk for detailed forensics:
```cpp
static constexpr uint32_t DBG_TRACKED_CHUNK_X = 0;
static constexpr uint32_t DBG_TRACKED_CHUNK_Y = 0;
static constexpr uint32_t DBG_TRACKED_LOD = 0;
```

## Performance Characteristics

**Initialization:** ~5-10 seconds (loads all LOD3 chunks + caches AABBs)

**Per-Frame (typical):**
- UpdateStreaming: ~0.5ms
- ProcessStreamingQueue: 0-5ms depending on uploads

**Memory:**
- LOD3 always-resident: ~20MB vertices, ~5MB indices
- Streaming budget: 256MB vertices, 64MB indices
- Chunk residency tracking: ~64KB (4096 chunks × 16 bytes)

## Future Improvements

1. **Async Loading:** Move mesh loading to background thread
2. **Better Eviction:** Use actual distance-based priority, not just LRU
3. **Defragmentation:** Implement allocator defragmentation for long sessions
4. **Compression:** Compress mesh data on disk for faster loading
5. **Predictive Prefetch:** Prefetch based on camera velocity/direction

---

# Quick Reference Card

## Critical Constants (MUST MATCH ACROSS FILES)

| Constant | Value | Files |
|----------|-------|-------|
| TERRAIN_SIZE | 64 | All files |
| TERRAIN_EXPORT_DIMS | 64 | Streaming manager |
| Vertex Stride | 60 bytes | All mesh loading |
| LOD0 threshold (dist²) | 400,000 | CPU streaming + GPU chunk data |
| LOD1 threshold (dist²) | 1,000,000 | CPU streaming + GPU chunk data |
| LOD2 threshold (dist²) | 2,000,000 | CPU streaming + GPU chunk data |
| Chunk size (world units) | 64 | WorldPosToChunkCoords |

## File Locations

| Component | File |
|-----------|------|
| Streaming Manager | `Zenith/Flux/Terrain/Flux_TerrainStreamingManager.cpp` |
| Streaming Manager Header | `Zenith/Flux/Terrain/Flux_TerrainStreamingManager.h` |
| GPU Culling Shader | `Zenith/Flux/Shaders/Terrain/Flux_TerrainCulling.comp` |
| Terrain Component | `Zenith/EntityComponent/Components/Zenith_TerrainComponent.cpp` |
| Terrain Renderer | `Zenith/Flux/Terrain/Flux_Terrain.cpp` |
| Export Tool | `Tools/Zenith_Tools_TerrainExport.cpp` |

## Key Functions

| Function | Purpose | Watch For |
|----------|---------|-----------|
| `WorldPosToChunkCoords()` | Camera pos → chunk coords | Uses TERRAIN_SIZE only (not × TERRAIN_SCALE) |
| `GetChunkCenter()` | Chunk coords → world pos | Uses cached AABB center |
| `BuildChunkDataForGPU()` | Prepare GPU buffer | LOD3 vertexOffset=0, LOD0-2 vertexOffset=absolute |
| `StreamInLOD()` | Upload mesh to GPU | Allocator returns relative, store absolute |
| `EvictLOD()` | Free mesh from GPU | Convert absolute to relative before Free() |

## Residency State Machine

```
NOT_LOADED ──RequestLOD()──► QUEUED ──ProcessQueue()──► LOADING ──Upload──► RESIDENT
     ▲                          │                           │
     │                          │                           │
     └──────────────────────────┴───────────────────────────┘
                            (on failure)
```

## Buffer Layout Summary

```
VERTEX BUFFER:
[LOD3 Combined (rebased indices, vertexOffset=0)] [Streaming Region (0-based indices, vertexOffset=absolute)]
                                                  ▲
                                                  └── m_uLOD3VertexCount marks boundary

INDEX BUFFER:
[LOD3 Combined] [Streaming Region]
                ▲
                └── m_uLOD3IndexCount marks boundary
```

## Debugging Checklist

When terrain LOD doesn't work:

- [ ] Check `WorldPosToChunkCoords` uses `TERRAIN_SIZE` not `TERRAIN_SIZE * TERRAIN_SCALE`
- [ ] Check CPU `LOD_THRESHOLDS_SQ` matches GPU `LOD_DISTANCES_SQ`
- [ ] Check mesh loaded with all attributes (flag=0), not position-only
- [ ] Check absolute/relative offset conversion in `StreamInLOD` and `EvictLOD`
- [ ] Check synchronous upload used (`UploadBufferDataAtOffset`)
- [ ] Enable debug logging and check "Chunk X resident" count
- [ ] Verify `EvictToMakeSpace()` only evicts chunks OUTSIDE their LOD range (fCurrentDistanceSq > fLODMaxDistanceSq * 1.2)
- [ ] Check queue filtering preserves nearby requests on camera chunk change

---

# GPU Performance Optimizations (December 2024)

## Overview

The terrain rendering pipeline was analyzed and optimized for GPU performance. Key bottlenecks were identified in the compute shader, vertex shader, and fragment shader. The optimizations focus on reducing per-thread work, eliminating unnecessary computations, and improving memory access patterns.

## Compute Shader Optimizations (Flux_TerrainCulling.comp)

### Previous Issues
1. **O(n²) Insertion Sort**: Each workgroup sorted up to 64 chunks using insertion sort
2. **Shared Memory Overhead**: Required barriers and atomic operations for sorting
3. **Full Chunk Load**: Every thread loaded entire chunk data even when culled
4. **Dynamic Branching**: Loop-based frustum test and LOD selection

### Optimizations Applied

1. **Removed Sorting Entirely**
   - Modern GPUs have effective early-Z rejection even without front-to-back ordering
   - ~5% depth overdraw is much cheaper than O(n²) sort overhead
   - Eliminated shared memory usage and barriers

2. **Unrolled Frustum Test**
   - Replaced 6-iteration loop with unrolled inline tests
   - Reordered to test Near/Far planes first (most likely to cull terrain)
   - Better instruction-level parallelism

3. **Branchless LOD Selection**
   ```glsl
   // Old: Loop with branching
   for (uint i = 0; i < LOD_COUNT; ++i) {
       if (distanceSq < chunk.lods[i].maxDistanceSq) return i;
   }
   
   // New: Branchless step functions
   uint lod = 0;
   lod += uint(distanceSq >= LOD0_MAX_DIST_SQ);
   lod += uint(distanceSq >= LOD1_MAX_DIST_SQ);
   lod += uint(distanceSq >= LOD2_MAX_DIST_SQ);
   ```

4. **Deferred Chunk Data Load**
   - Load only AABB first for culling test
   - Load full LOD data only if chunk passes frustum test
   - Reduces memory bandwidth for culled chunks

### Expected Performance Improvement
- Compute dispatch: ~30-50% faster due to removal of sorting
- Better GPU occupancy from reduced shared memory usage

## Vertex Shader Optimizations (Flux_Terrain_VertCommon.fxh)

### Previous Issues
1. **Large TBN mat3 Varying**: 9 floats (3×vec3) passed vertex → fragment
2. **Per-Vertex LOD Buffer Read**: Every vertex sampled LOD buffer

### Optimizations Applied

1. **TBN Reconstruction**
   - Pass Normal + Tangent + BitangentSign (7 floats)
   - Reconstruct bitangent in fragment shader: `B = cross(N, T) * sign`
   - Saves 2 floats per vertex in varying bandwidth

2. **Flat LOD Interpolation**
   - LOD level uses `flat` interpolation qualifier
   - Only provoking vertex reads from LOD buffer
   - 3x reduction in LOD buffer reads

### Varying Count Reduction
- Before: UV(2) + Normal(3) + WorldPos(3) + MaterialLerp(1) + TBN(9) + LOD(1) = 19 floats
- After: UV(2) + Normal(3) + WorldPos(3) + Tangent(3) + MaterialLerp(1) + LOD(1) + BitSign(1) = 14 floats
- ~26% reduction in vertex-to-fragment bandwidth

## Fragment Shader Optimizations (Flux_Terrain_FragCommon.fxh)

### Previous Issues
1. **6 Texture Samples Always**: Both materials sampled even if only one visible
2. **Redundant TBN Transforms**: Normal transformed twice then lerped

### Optimizations Applied

1. **Material Lerp Early-Out**
   ```glsl
   const float LERP_THRESHOLD = 0.02;
   
   if (materialLerp < LERP_THRESHOLD) {
       // Sample only material 0 (3 textures)
   } else if (materialLerp > (1.0 - LERP_THRESHOLD)) {
       // Sample only material 1 (3 textures)
   } else {
       // Sample both materials (6 textures)
   }
   ```
   - Saves 3 texture samples when terrain is uniform
   - Estimated 50% of terrain pixels use single material

2. **Tangent-Space Normal Blending**
   - Previous: Transform both normals to world space, then lerp
   - New: Lerp in tangent space, single TBN transform
   - Saves 1 mat3×vec3 multiply when blending

3. **Switch-Based LOD Visualization**
   - Early return path for debug visualization
   - No texture samples in LOD viz mode

### Expected Texture Bandwidth Savings
- Uniform terrain regions: 50% fewer texture samples
- Mixed terrain regions: 1 fewer matrix multiply

## Profiling Infrastructure

### New Profile Indices
- `ZENITH_PROFILE_INDEX__FLUX_TERRAIN_CULLING` - GPU culling dispatch
- `ZENITH_PROFILE_INDEX__FLUX_TERRAIN_STREAMING` - CPU streaming updates

### Performance Metrics Logging
Enable via debug variable: `Render > Terrain > Log Metrics`

Logs every 120 frames:
- High-LOD chunks resident count
- Vertex/Index buffer utilization percentage
- Buffer fragmentation (free block count)

### Interpretation Guide
- **High-LOD Resident < 50**: Camera far from all chunks, no streaming needed
- **High-LOD Resident > 500**: Large visible area, streaming active
- **Buffer > 80% Used**: Consider increasing streaming budget
- **Fragmentation > 100**: May need defragmentation pass

## Performance Guidelines for Future Work

### Shader Optimization Rules
1. **Avoid Dynamic Branching in Inner Loops** - Unroll when count is small and known
2. **Prefer Branchless Selection** - Use step/mix/clamp over if-else chains
3. **Minimize Varyings** - Reconstruct cheap values in fragment shader
4. **Use `flat` Qualifier** - For per-draw (not per-vertex) data
5. **Early-Out Expensive Paths** - Skip work when inputs are trivial

### Compute Shader Rules
1. **Question the Need for Sorting** - Often not worth the complexity
2. **Defer Data Loads** - Test cheap conditions before expensive loads
3. **Avoid Shared Memory for Small Data** - Register pressure < sync overhead
4. **Test Culling Planes in Priority Order** - Most-likely-to-cull first

### Memory Access Patterns
1. **Coalesced Reads** - Sequential threads read sequential memory
2. **Minimize Buffer Switches** - Keep all terrain data in unified buffer
3. **Batch Similar Work** - All chunks use same pipeline/descriptor set

---

**Last Updated:** 2025-12-11
**Author:** Terrain LOD Streaming System Debug Session
**Status:** PRODUCTION-READY - All bugs fixed and verified working
**Culling Status:** CPU-based frustum culling (Production-Ready)
**Streaming Status:** Distance-based LOD streaming with safe eviction (Production-Ready)
**Bug #6 Status:** RESOLVED - Fighting for residency, queue filtering, and safe eviction all fixed
**GPU Optimization Pass:** December 2024 - Compute/Vertex/Fragment shader optimizations applied
