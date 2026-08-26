#pragma once

/**
 * Flux_TerrainConfig.h - Unified Terrain LOD Configuration
 *
 * This header provides a single source of truth for all terrain LOD-related constants.
 * These values MUST be kept in sync between:
 *   - CPU streaming (Flux_TerrainStreamingManager)
 *   - GPU culling (Shaders/Terrain/Flux_TerrainCulling.slang)
 *   - GPU rendering (Shaders/Terrain/Flux_Terrain_ToGBuffer.slang + the terrain vertex format)
 *
 * LOD System (2 levels):
 *   - HIGH (LOD 0): Highest detail, density divisor 1, streamed dynamically
 *   - LOW (LOD 1): Lower detail, density divisor 4, always-resident (never evicted)
 *
 * CRITICAL: If you change LOD thresholds here, also update:
 *   - Zenith/Flux/Shaders/Terrain/Flux_TerrainCulling.slang (selectLOD function)
 */

#include <cstdint>
#include <cfloat>

namespace Flux_TerrainConfig {

// ========== Grid Configuration ==========
//
// ★ THESE ARE CAPACITY AND DEFAULTS, NOT A TERRAIN'S ACTUAL SHAPE.
// A terrain's chunk world size, vertex density and grid extent live on its
// Zenith_TerrainDimensions (Core/Zenith_TerrainDimensions.h), reached through
// Zenith_TerrainComponent::GetTerrainDimensions() or the owning
// Flux_TerrainStreamingState::m_xDims. Read those for anything per-terrain.
//
// CHUNK_GRID_SIZE / TOTAL_CHUNKS are the FIXED slot capacity every GPU-side
// array is sized for -- the indirect argument buffer, the residency and AABB
// tables, the reset dispatch, the culling shader's slot count -- and the flat
// chunk index keeps its stride-64 spelling so a chunk's slot never moves when a
// grid shrinks. An active grid may be smaller in either axis and need not be
// square; slots outside it are zero-count no-op records.
//
// CHUNK_SIZE_WORLD and TERRAIN_SIZE describe the DEFAULT terrain --
// Zenith_TerrainDimensions::Default() -- which is what every bake on disk was
// authored at before the knobs existed. The static_asserts in
// Flux_TerrainStreamingManager.cpp pin the two descriptions together.
//
// (The exporter's TERRAIN_SCALE define is GONE. It was a multiply by exactly
// 1.0f applied to every position, and the sample step IS the world step now, so
// there is nothing left to rescale.)
static constexpr uint32_t CHUNK_GRID_SIZE = 64;
static constexpr uint32_t TOTAL_CHUNKS = CHUNK_GRID_SIZE * CHUNK_GRID_SIZE;

// Size of a DEFAULT terrain's chunk in world units. A terrain's own is
// m_xDims.m_fChunkWorldSize.
static constexpr float CHUNK_SIZE_WORLD = 64.0f;

// Total size of a DEFAULT terrain in world units. A terrain's own extent is
// m_xDims.WorldSizeX() / WorldSizeZ(), and the square authoring domain every
// per-set image spans is m_xDims.MaxWorldSize().
static constexpr float TERRAIN_SIZE = CHUNK_GRID_SIZE * CHUNK_SIZE_WORLD;

// Number of LOD levels (HIGH = highest detail, LOW = always-resident)
static constexpr uint32_t LOD_COUNT = 2;
static constexpr uint32_t LOD_HIGH = 0;            // Streamed dynamically
static constexpr uint32_t LOD_LOW = 1;             // Always resident (never evicted)
static constexpr uint32_t LOD_ALWAYS_RESIDENT = LOD_LOW;

// ========== LOD Distance Thresholds (Distance Squared) ==========
// These thresholds determine which LOD level is selected based on
// the squared distance from camera to chunk center.
//
// CRITICAL: These values MUST match the GPU culling shader!
// If they don't match, CPU will stream one LOD but GPU will select another,
// causing fallback to LOW LOD.
//
// Distance to chunk center (meters):
//   HIGH: 0 - 1000m    (close, highest detail, streamed)
//   LOW: 1000m+        (far, always-resident fallback)

static constexpr float LOD_HIGH_MAX_DISTANCE_SQ = 1000000.0f;  // sqrt = 1000m
static constexpr float LOD_LOW_MAX_DISTANCE_SQ = FLT_MAX;      // Always used beyond HIGH

// Array form for easy iteration
static constexpr float LOD_MAX_DISTANCE_SQ[LOD_COUNT] = {
    LOD_HIGH_MAX_DISTANCE_SQ,
    LOD_LOW_MAX_DISTANCE_SQ
};

// ========== Streaming Configuration ==========
// Buffer budgets for streaming region (HIGH LOD meshes only - LOW is always resident)
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

// LOD hysteresis factors - prevent thrashing at LOD boundaries.
// These constants are LINEAR distance ratios (e.g. 1.5x means evict beyond
// 1.5 × LOD-range linear distance). Distance comparisons in the streaming
// code work in squared-distance space, so multiply LOD_*_DISTANCE_SQ by
// SquaredHysteresis(linear) — never by the linear constant directly. The
// previous code applied 1.5f to a squared threshold, which gave √1.5 ≈
// 1.225× (and 1.2f → ≈1.095×), narrower than intended.
static constexpr float LOD_EVICTION_HYSTERESIS = 1.5f;        // 50% beyond LOD threshold for main eviction
static constexpr float LOD_FORCED_EVICTION_HYSTERESIS = 1.2f;  // 20% beyond LOD threshold for forced eviction

// Convert a linear-distance hysteresis ratio to its squared counterpart so
// it can be applied to a squared-distance threshold without distorting the
// effective radius.
inline constexpr float SquaredHysteresis(float fLinear) { return fLinear * fLinear; }

// Active chunk radius - only consider chunks within this many chunks of camera
// Reduces streaming updates from 4096 to ~1024 chunks.
//
// DELIBERATELY IN CHUNKS, NOT METRES, and therefore deliberately GLOBAL: it is a
// budget on how many chunks the streamer examines per rebuild, which is what it
// costs whatever a chunk measures. The same is true of the LOD distance
// thresholds above, which stay in metres. A terrain with smaller chunks
// therefore keeps a smaller world-space active set (and a larger one a bigger
// set); that is a behaviour change, not a defect, and it is bounded by the
// active grid, which is smaller too.
static constexpr uint32_t ACTIVE_CHUNK_RADIUS = 16;

// Frame interval for streaming updates (not every frame needs full update)
static constexpr uint32_t STREAMING_UPDATE_INTERVAL = 2;

// ========== Vertex Format ==========
// Terrain vertex stride (Position + UV + Normal + Tangent+Sign)
// = SNORM16x4(8) + UNORM16x2(4) + SNORM10:10:10:2(4) + SNORM10:10:10:2(4) = 20 bytes
//
// The position is quantised against the AUTHORED terrain box (not a per-chunk
// AABB, which would crack every chunk seam); the box and the byte offsets live
// with the on-disk contract in Core/Zenith_TerrainChunkLayout.h, which the
// static_asserts in Flux_TerrainStreamingManager.cpp pin against the reflected
// shader layout.
//
// UV is UNORM16, not HALF2: terrain UVs are AUTHORED WORLD XZ IN METRES over the
// terrain's square authoring domain (up to 4096 for a default terrain), and
// HALF's 10-bit mantissa loses sub-integer precision above 1024 / 2-unit
// precision above 2048. With HALF the far half of any large terrain shows a
// stretched/compressed strip artefact at vertex spacing in the diffuse channel.
// Unorm16 normalised by the terrain's own extent is uniform across the whole
// range -- 1/16 metre on a default terrain, and FINER on a smaller one, so
// shrinking a terrain can never cost UV precision.
//
// (Metres and heightmap pixels were the same number while every terrain was
// 4096m wide baked from a 4096px image, which is why the two readings were
// indistinguishable until terrains could differ in size.)
static constexpr uint32_t VERTEX_STRIDE_BYTES = 20;

// ========== Helper Functions ==========

/**
 * Select appropriate LOD level for a given distance squared
 * @param fDistanceSq Squared distance from camera to chunk center
 * @return LOD level (LOD_HIGH or LOD_LOW)
 */
inline uint32_t SelectLOD(float fDistanceSq)
{
    return (fDistanceSq < LOD_HIGH_MAX_DISTANCE_SQ) ? LOD_HIGH : LOD_LOW;
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
    static const char* LOD_NAMES[] = { "HIGH", "LOW" };
    return (uLOD < LOD_COUNT) ? LOD_NAMES[uLOD] : "Invalid";
}

// ========== Aliases for Legacy/Alternate Naming ==========
// These aliases maintain compatibility with code using different naming conventions

// Grid aliases
static constexpr float CHUNK_WORLD_SIZE = CHUNK_SIZE_WORLD;

// LOD aliases for backward compatibility
static constexpr uint32_t LOD_HIGHEST_DETAIL = LOD_HIGH;
static constexpr uint32_t LOD_LOWEST_DETAIL = LOD_LOW;

// Buffer aliases (MB form for logging)
static constexpr uint64_t STREAMING_VERTEX_BUFFER_SIZE_MB = STREAMING_VERTEX_BUFFER_MB;
static constexpr uint64_t STREAMING_INDEX_BUFFER_SIZE_MB = STREAMING_INDEX_BUFFER_MB;

// Vertex stride alias
static constexpr uint32_t TERRAIN_VERTEX_STRIDE = VERTEX_STRIDE_BYTES;

// Terrain height (approximate - used for chunk center calculation)
static constexpr float MAX_TERRAIN_HEIGHT = 512.0f;

} // namespace Flux_TerrainConfig

