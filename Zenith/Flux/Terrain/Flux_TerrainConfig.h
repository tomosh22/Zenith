#pragma once

/**
 * Flux_TerrainConfig.h - Unified Terrain LOD Configuration
 * 
 * This header provides a single source of truth for all terrain LOD-related constants.
 * These values MUST be kept in sync between:
 *   - CPU streaming (Flux_TerrainStreamingManager)
 *   - GPU culling (Flux_TerrainCulling.comp)
 *   - GPU rendering (Flux_Terrain_VertCommon.fxh)
 * 
 * CRITICAL: If you change LOD thresholds here, also update:
 *   - Zenith/Flux/Shaders/Terrain/Flux_TerrainCulling.comp (selectLOD function comments)
 * 
 * Bug History:
 *   - CPU/GPU threshold mismatch caused LOD colors correct but mesh density wrong
 *   - TERRAIN_SCALE mismatch caused streaming to load wrong chunks
 *   - See Flux/Terrain/CLAUDE.md for full bug documentation
 */

#include <cstdint>
#include <cfloat>

namespace Flux_TerrainConfig {

// ========== Grid Configuration ==========
// Number of chunks in each dimension (64x64 = 4096 total chunks)
static constexpr uint32_t CHUNK_GRID_SIZE = 64;
static constexpr uint32_t TOTAL_CHUNKS = CHUNK_GRID_SIZE * CHUNK_GRID_SIZE;

// Size of each chunk in world units (actual exported mesh positions)
// CRITICAL: Export tool uses TERRAIN_SCALE=1, so chunks are 64 units wide
// Do NOT multiply by TERRAIN_SCALE here - that was a major bug source
static constexpr float CHUNK_SIZE_WORLD = 64.0f;

// Total terrain size in world units
static constexpr float TERRAIN_SIZE = CHUNK_GRID_SIZE * CHUNK_SIZE_WORLD;

// Number of LOD levels (LOD0 = highest detail, LOD3 = lowest/always-resident)
static constexpr uint32_t LOD_COUNT = 4;
static constexpr uint32_t LOD_HIGHEST_DETAIL = 0;
static constexpr uint32_t LOD_LOWEST_DETAIL = 3;
static constexpr uint32_t LOD_ALWAYS_RESIDENT = 3;

// ========== LOD Distance Thresholds (Distance Squared) ==========
// These thresholds determine which LOD level is selected based on
// the squared distance from camera to chunk center.
//
// CRITICAL: These values MUST match the GPU culling shader!
// If they don't match, CPU will stream one LOD but GPU will select another,
// causing fallback to LOD3.
//
// Distance to chunk center (meters):
//   LOD0: 0 - 632m     (very close, highest detail)
//   LOD1: 632 - 1000m  (close)
//   LOD2: 1000 - 1414m (medium distance)
//   LOD3: 1414m+       (far, always-resident fallback)

static constexpr float LOD0_MAX_DISTANCE_SQ = 400000.0f;   // sqrt = ~632m
static constexpr float LOD1_MAX_DISTANCE_SQ = 1000000.0f;  // sqrt = 1000m
static constexpr float LOD2_MAX_DISTANCE_SQ = 2000000.0f;  // sqrt = ~1414m
static constexpr float LOD3_MAX_DISTANCE_SQ = FLT_MAX;     // Always used beyond LOD2

// Array form for easy iteration
static constexpr float LOD_MAX_DISTANCE_SQ[LOD_COUNT] = {
    LOD0_MAX_DISTANCE_SQ,
    LOD1_MAX_DISTANCE_SQ,
    LOD2_MAX_DISTANCE_SQ,
    LOD3_MAX_DISTANCE_SQ
};

// Alias for backward compatibility (some code uses LOD_DISTANCE_SQ)
static constexpr const float* LOD_DISTANCE_SQ = LOD_MAX_DISTANCE_SQ;

// ========== Streaming Configuration ==========
// Buffer budgets for streaming region (LOD0-2 meshes)
static constexpr uint64_t STREAMING_VERTEX_BUFFER_MB = 256;
static constexpr uint64_t STREAMING_INDEX_BUFFER_MB = 64;
static constexpr uint64_t STREAMING_VERTEX_BUFFER_BYTES = STREAMING_VERTEX_BUFFER_MB * 1024 * 1024;
static constexpr uint64_t STREAMING_INDEX_BUFFER_BYTES = STREAMING_INDEX_BUFFER_MB * 1024 * 1024;

// Alias for backward compatibility
static constexpr uint64_t STREAMING_VERTEX_BUFFER_SIZE = STREAMING_VERTEX_BUFFER_BYTES;
static constexpr uint64_t STREAMING_INDEX_BUFFER_SIZE = STREAMING_INDEX_BUFFER_BYTES;

// Per-frame processing limits to avoid stalls
// Note: Higher upload count = more responsive streaming but potentially more frame stutter
// 8 uploads/frame is a good balance for terrain chunks (~50-100KB each)
static constexpr uint32_t MAX_UPLOADS_PER_FRAME = 8;
static constexpr uint32_t MAX_EVICTIONS_PER_FRAME = 16;
static constexpr uint32_t MAX_QUEUE_SIZE = 256;

// ========== Optimization Tuning ==========
// Camera movement threshold before re-evaluating LODs (squared distance)
static constexpr float CAMERA_MOVE_THRESHOLD_SQ = 100.0f;  // ~10m movement

// LOD hysteresis factor - prevents LOD thrashing at boundaries
// Chunks must move past threshold * hysteresis to change LOD
static constexpr float LOD_HYSTERESIS_FACTOR = 1.1f;  // 10% hysteresis band

// Active chunk radius - only consider chunks within this many chunks of camera
// Reduces streaming updates from 4096 to ~1024 chunks
static constexpr uint32_t ACTIVE_CHUNK_RADIUS = 16;

// Frame interval for streaming updates (not every frame needs full update)
static constexpr uint32_t STREAMING_UPDATE_INTERVAL = 2;

// ========== Vertex Format ==========
// Terrain vertex stride (Position + UV + Normal + Tangent + Bitangent + MaterialLerp)
// = 12 + 8 + 12 + 12 + 12 + 4 = 60 bytes
static constexpr uint32_t VERTEX_STRIDE_BYTES = 60;

// ========== Helper Functions ==========

/**
 * Select appropriate LOD level for a given distance squared
 * @param fDistanceSq Squared distance from camera to chunk center
 * @return LOD level (0-3)
 */
inline uint32_t SelectLOD(float fDistanceSq)
{
    // Check LOD0-2 thresholds
    for (uint32_t i = 0; i < LOD_ALWAYS_RESIDENT; ++i)
    {
        if (fDistanceSq < LOD_MAX_DISTANCE_SQ[i])
        {
            return i;
        }
    }
    // Default to always-resident LOD3
    return LOD_ALWAYS_RESIDENT;
}

/**
 * Convert 2D chunk coordinates to flat index
 * Uses x * GRID_SIZE + y to match standard iteration order
 */
inline uint32_t ChunkCoordsToIndex(uint32_t uChunkX, uint32_t uChunkY)
{
    return uChunkX * CHUNK_GRID_SIZE + uChunkY;
}

/**
 * Convert flat index to 2D chunk coordinates
 */
inline void ChunkIndexToCoords(uint32_t uChunkIndex, uint32_t& uChunkX, uint32_t& uChunkY)
{
    uChunkX = uChunkIndex / CHUNK_GRID_SIZE;
    uChunkY = uChunkIndex % CHUNK_GRID_SIZE;
}

/**
 * Get human-readable LOD name for logging
 */
inline const char* GetLODName(uint32_t uLOD)
{
    static const char* LOD_NAMES[] = { "LOD0 (Highest)", "LOD1", "LOD2", "LOD3 (Lowest)" };
    return (uLOD < LOD_COUNT) ? LOD_NAMES[uLOD] : "Invalid";
}

// ========== Aliases for Legacy/Alternate Naming ==========
// These aliases maintain compatibility with code using different naming conventions

// Grid aliases
static constexpr float CHUNK_WORLD_SIZE = CHUNK_SIZE_WORLD;

// LOD aliases
static constexpr uint32_t LOWEST_DETAIL_LOD = LOD_LOWEST_DETAIL;
static constexpr uint32_t HIGHEST_DETAIL_LOD = LOD_HIGHEST_DETAIL;

// Buffer aliases (MB form for logging)
static constexpr uint64_t STREAMING_VERTEX_BUFFER_SIZE_MB = STREAMING_VERTEX_BUFFER_MB;
static constexpr uint64_t STREAMING_INDEX_BUFFER_SIZE_MB = STREAMING_INDEX_BUFFER_MB;

// Vertex stride alias
static constexpr uint32_t TERRAIN_VERTEX_STRIDE = VERTEX_STRIDE_BYTES;

// Terrain height (approximate - used for chunk center calculation)
static constexpr float MAX_TERRAIN_HEIGHT = 512.0f;

} // namespace Flux_TerrainConfig

