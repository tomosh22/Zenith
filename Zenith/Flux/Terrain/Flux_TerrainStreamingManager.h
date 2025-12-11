#pragma once

#include "Flux/Flux_Buffers.h"
#include "Flux/Terrain/Flux_TerrainConfig.h"
#include "Maths/Zenith_Maths.h"
#include "Maths/Zenith_FrustumCulling.h"
#include <vector>
#include <queue>
#include <cstdint>

// Use unified terrain configuration
using namespace Flux_TerrainConfig;

// Forward declarations
class Flux_MeshGeometry;
struct Zenith_TerrainChunkData;

// Residency state for a single LOD level of a terrain chunk
enum class Flux_TerrainLODResidencyState : uint8_t
{
	NOT_LOADED = 0,   // LOD mesh data not in GPU memory
	QUEUED,           // LOD request is in the streaming queue
	LOADING,          // LOD mesh data being uploaded to GPU (reserved in allocator)
	RESIDENT,         // LOD mesh data fully resident in GPU memory
	EVICTING          // LOD mesh data being removed from GPU memory
};

// Allocation info for a single LOD level when resident in GPU
struct Flux_TerrainLODAllocation
{
	uint32_t m_uVertexOffset;      // Offset in streaming vertex buffer (in vertices)
	uint32_t m_uVertexCount;       // Number of vertices
	uint32_t m_uIndexOffset;       // Offset in streaming index buffer (in indices)
	uint32_t m_uIndexCount;        // Number of indices
};

// Per-chunk per-LOD residency tracking
struct Flux_TerrainChunkResidency
{
	Flux_TerrainLODResidencyState m_aeStates[LOD_COUNT];
	Flux_TerrainLODAllocation m_axAllocations[LOD_COUNT];

	// Last frame this chunk was requested at each LOD
	// Used for eviction heuristics (LRU-style)
	uint32_t m_auLastRequestedFrame[LOD_COUNT];

	// Priority score for each LOD (lower distance = higher priority)
	float m_afPriorities[LOD_COUNT];
};

// Simple free-list allocator for buffer space
class Flux_TerrainBufferAllocator
{
public:
	Flux_TerrainBufferAllocator();

	void Initialize(uint32_t uTotalSize, const char* szDebugName);
	void Reset();

	// Attempt to allocate a block of the given size
	// Returns offset on success, UINT32_MAX on failure
	uint32_t Allocate(uint32_t uSize);

	// Free a previously allocated block
	void Free(uint32_t uOffset, uint32_t uSize);

	// Get current free space
	uint32_t GetUnusedSpace() const { return m_uUnusedSpace; }
	uint32_t GetTotalSpace() const { return m_uTotalSize; }
	uint32_t GetUsedSpace() const { return m_uTotalSize - m_uUnusedSpace; }

	// Debug info
	const char* GetDebugName() const { return m_szDebugName; }
	uint32_t GetFragmentationCount() const { return static_cast<uint32_t>(m_xFreeBlocks.size()); }

private:
	struct FreeBlock
	{
		uint32_t m_uOffset;
		uint32_t m_uSize;

		bool operator<(const FreeBlock& other) const
		{
			// Priority queue: largest blocks first
			return m_uSize < other.m_uSize;
		}
	};

	std::priority_queue<FreeBlock> m_xFreeBlocks;
	uint32_t m_uTotalSize;
	uint32_t m_uUnusedSpace;
	const char* m_szDebugName;

	// Merge adjacent free blocks to reduce fragmentation
	void Defragment();
};

// Central manager for terrain LOD streaming
// All methods are static - no instantiation needed
class Flux_TerrainStreamingManager
{
public:
	Flux_TerrainStreamingManager() = delete;
	~Flux_TerrainStreamingManager() = delete;

	static void Initialize();
	static void Shutdown();

	static bool IsInitialized() { return s_bInitialized; }

	// ========== Streaming API ==========

	/**
	 * Update streaming decisions based on current camera position
	 * Called once per frame before terrain rendering
	 * Early-outs if not initialized (no terrain components)
	 * @param xCameraPos Current camera position for distance calculations
	 */
	static void UpdateStreaming(const Zenith_Maths::Vector3& xCameraPos);

	/**
	 * Request a specific LOD for a chunk to be resident
	 * If not already resident, will queue for streaming
	 * @param uChunkX Chunk X coordinate (0-63)
	 * @param uChunkY Chunk Y coordinate (0-63)
	 * @param uLODLevel LOD level (0=highest detail, 3=lowest)
	 * @param fPriority Priority score (lower = more important)
	 * @return true if LOD is resident and ready to use
	 */
	static bool RequestLOD(uint32_t uChunkX, uint32_t uChunkY, uint32_t uLODLevel, float fPriority);

	/**
	 * Get the buffer offsets for a chunk's LOD if it's resident
	 * @param uChunkX Chunk X coordinate
	 * @param uChunkY Chunk Y coordinate
	 * @param uLODLevel LOD level
	 * @param xAllocOut [out] Allocation info if resident
	 * @return true if LOD is resident, false if need to fall back to LOD3
	 */
	static bool GetLODAllocation(uint32_t uChunkX, uint32_t uChunkY, uint32_t uLODLevel, Flux_TerrainLODAllocation& xAllocOut);

	/**
	 * Get residency state for a chunk's LOD
	 */
	static Flux_TerrainLODResidencyState GetResidencyState(uint32_t uChunkX, uint32_t uChunkY, uint32_t uLODLevel);

	/**
	 * Build chunk data array for GPU upload
	 * This is called by Zenith_TerrainComponent::BuildChunkData() to get the current
	 * allocation info for all chunks and LODs
	 * @param pxChunkDataOut [out] Array of TOTAL_CHUNKS chunk data structs
	 */
	static void BuildChunkDataForGPU(Zenith_TerrainChunkData* pxChunkDataOut);

	/**
	 * Check if chunk data needs to be re-uploaded to GPU
	 * Returns true if any LOD allocations have changed since last upload
	 */
	static bool IsChunkDataDirty() { return s_bInitialized && s_bChunkDataDirty; }

	/**
	 * Mark chunk data as uploaded (clear dirty flag)
	 */
	static void ClearChunkDataDirty() { s_bChunkDataDirty = false; }

	/**
	 * Get pre-allocated chunk data buffer for zero-allocation updates
	 * Returns nullptr if not initialized
	 */
	static Zenith_TerrainChunkData* GetCachedChunkDataBuffer() { return s_pxCachedChunkData; }

	// ========== Buffer Access ==========

	/**
	 * Get the unified terrain vertex/index buffers from the registered component
	 * These buffers contain both LOD3 (always-resident, at the beginning)
	 * and streaming LOD0-2 data (dynamically allocated in the rest of the buffer)
	 * Returns null references if no component is registered
	 */
	static const Flux_VertexBuffer* GetTerrainVertexBuffer();
	static const Flux_IndexBuffer* GetTerrainIndexBuffer();

	/**
	 * Register a terrain component's unified buffers with the streaming manager
	 * The component owns the buffers; the streaming manager uses them for LOD streaming
	 * Only one component can be registered at a time
	 * @param pxVertexBuffer Pointer to component's unified vertex buffer
	 * @param pxIndexBuffer Pointer to component's unified index buffer
	 * @param ulVertexBufferSize Total size of vertex buffer in bytes
	 * @param ulIndexBufferSize Total size of index buffer in bytes
	 * @param uVertexStride Bytes per vertex
	 * @param uLOD3VertexCount Number of vertices in LOD3 region (at buffer start)
	 * @param uLOD3IndexCount Number of indices in LOD3 region (at buffer start)
	 */
	static void RegisterTerrainBuffers(
		Flux_VertexBuffer* pxVertexBuffer,
		Flux_IndexBuffer* pxIndexBuffer,
		uint64_t ulVertexBufferSize,
		uint64_t ulIndexBufferSize,
		uint32_t uVertexStride,
		uint32_t uLOD3VertexCount,
		uint32_t uLOD3IndexCount
	);

	/**
	 * Unregister terrain buffers when component is destroyed
	 */
	static void UnregisterTerrainBuffers();

	// ========== Debug / Stats ==========

	struct StreamingStats
	{
		uint32_t m_uLOD3ChunksResident;          // Always 4096 (all chunks)
		uint32_t m_uHighLODChunksResident;       // Number of LOD0-2 chunks currently resident
		uint32_t m_uStreamingRequestsThisFrame;  // Number of streaming requests this frame
		uint32_t m_uEvictionsThisFrame;          // Number of evictions this frame
		uint32_t m_uVertexBufferUsedMB;          // MB used in streaming vertex buffer
		uint32_t m_uVertexBufferTotalMB;         // MB total in streaming vertex buffer
		uint32_t m_uIndexBufferUsedMB;           // MB used in streaming index buffer
		uint32_t m_uIndexBufferTotalMB;          // MB total in streaming index buffer
		uint32_t m_uVertexFragments;             // Number of free blocks in vertex allocator
		uint32_t m_uIndexFragments;              // Number of free blocks in index allocator
	};

	static const StreamingStats& GetStats() { return s_xStats; }
	static void LogStats();

private:
	// Initialization state
	static bool s_bInitialized;

	// Frame counter for LRU tracking
	static uint32_t s_uCurrentFrame;

	// ========== GPU Buffers (owned by Zenith_TerrainComponent, referenced here) ==========

	// Pointers to the terrain component's unified buffers
	// Contains LOD3 (reserved at start) and streaming LOD0-2 data
	static Flux_VertexBuffer* s_pxUnifiedVertexBuffer;
	static Flux_IndexBuffer* s_pxUnifiedIndexBuffer;

	// Buffer sizes for bounds checking (cached from registration)
	static uint64_t s_ulUnifiedVertexBufferSize;  // Total size in bytes
	static uint64_t s_ulUnifiedIndexBufferSize;   // Total size in bytes
	static uint32_t s_uVertexStride;              // Bytes per vertex

	// Reserved space tracking for LOD3 (never evicted)
	static uint32_t s_uLOD3VertexCount;   // Number of vertices reserved for LOD3 at buffer start
	static uint32_t s_uLOD3IndexCount;     // Number of indices reserved for LOD3 at buffer start

	// ========== Allocation Tracking ==========

	static Flux_TerrainBufferAllocator s_xVertexAllocator;
	static Flux_TerrainBufferAllocator s_xIndexAllocator;

	// Per-chunk residency state (64x64 = 4096 chunks)
	static Flux_TerrainChunkResidency s_axChunkResidency[TOTAL_CHUNKS];

	// Cached AABBs for all chunks (computed once, reused for all chunk data updates)
	static Zenith_AABB s_axChunkAABBs[TOTAL_CHUNKS];
	static bool s_bAABBsCached;

	// ========== Streaming Queue ==========

	struct StreamingRequest
	{
		uint32_t m_uChunkIndex;  // Flat index (x * 64 + y)
		uint32_t m_uLODLevel;
		float m_fPriority;       // Lower = more important

		bool operator<(const StreamingRequest& other) const
		{
			// Priority queue: lower priority value = higher priority
			return m_fPriority > other.m_fPriority;
		}
	};

	static std::priority_queue<StreamingRequest> s_xStreamingQueue;

	// ========== Eviction Candidates ==========

	struct EvictionCandidate
	{
		uint32_t m_uChunkIndex;
		uint32_t m_uLODLevel;
		float m_fPriority;  // Higher = more likely to evict

		bool operator<(const EvictionCandidate& other) const
		{
			// Priority queue: higher priority value = evict first
			return m_fPriority < other.m_fPriority;
		}
	};

	// ========== Stats ==========

	static StreamingStats s_xStats;

	// ========== Performance Optimization State ==========

	// Camera position tracking for incremental updates
	static Zenith_Maths::Vector3 s_xLastCameraPos;

	// Current desired LOD for each chunk (cached to avoid recomputation)
	static uint8_t s_auDesiredLOD[TOTAL_CHUNKS];

	// Dirty flag - true when chunk data needs GPU re-upload
	static bool s_bChunkDataDirty;

	// Pre-allocated chunk data buffer to avoid per-frame allocations
	static Zenith_TerrainChunkData* s_pxCachedChunkData;

	// Frame skip for streaming updates (not every frame needs full update)
	static uint32_t s_uStreamingUpdateInterval;  // Process streaming every N frames

	// Active chunk set - chunks within streaming consideration distance
	// Avoids scanning all 4096 chunks every frame
	static std::vector<uint32_t> s_xActiveChunkIndices;
	static uint32_t s_uActiveChunkRadius;  // Chunks in each direction from camera
	static int32_t s_iLastCameraChunkX;
	static int32_t s_iLastCameraChunkY;

	// ========== Internal Helpers ==========

	/**
	 * Process streaming queue and upload LOD mesh data to GPU
	 * Called during UpdateStreaming()
	 */
	static void ProcessStreamingQueue();

	/**
	 * Evict one or more low-priority LODs to make space
	 * @param uVertexSpaceNeeded Vertices needed
	 * @param uIndexSpaceNeeded Indices needed
	 * @return true if enough space was freed
	 */
	static bool EvictToMakeSpace(uint32_t uVertexSpaceNeeded, uint32_t uIndexSpaceNeeded);

	/**
	 * Stream in a specific chunk's LOD mesh data
	 * Assumes space has already been allocated
	 * @param uChunkIndex Flat chunk index
	 * @param uLODLevel LOD level to load
	 * @param uVertexOffset Offset in streaming vertex buffer
	 * @param uIndexOffset Offset in streaming index buffer
	 * @return true on success
	 */
	static bool StreamInLOD(uint32_t uChunkIndex, uint32_t uLODLevel, uint32_t uVertexOffset, uint32_t uIndexOffset);

	/**
	 * Evict a specific chunk's LOD from GPU memory
	 * @param uChunkIndex Flat chunk index
	 * @param uLODLevel LOD level to evict
	 */
	static void EvictLOD(uint32_t uChunkIndex, uint32_t uLODLevel);

	/**
	 * Build eviction candidate list from all resident high LODs
	 * @param xCameraPos Current camera position for priority calculation
	 */
	static std::vector<EvictionCandidate> BuildEvictionCandidates(const Zenith_Maths::Vector3& xCameraPos);

	/**
	 * Calculate chunk center position for distance calculations
	 */
	static Zenith_Maths::Vector3 GetChunkCenter(uint32_t uChunkX, uint32_t uChunkY);

	/**
	 * Rebuild the active chunk set based on camera position
	 * Only chunks in this set are considered for LOD updates each frame
	 */
	static void RebuildActiveChunkSet(int32_t iCameraChunkX, int32_t iCameraChunkY);

	/**
	 * Get chunk coords from world position
	 */
	static void WorldPosToChunkCoords(const Zenith_Maths::Vector3& xWorldPos, int32_t& iChunkX, int32_t& iChunkY);

	/**
	 * Convert 2D chunk coords to flat index
	 * NOTE: Uses x * DIMS + y to match the loop order (x outer, y inner) used throughout
	 * the terrain streaming and combine code.
	 */
	static inline uint32_t ChunkCoordsToIndex(uint32_t uChunkX, uint32_t uChunkY)
	{
		return uChunkX * CHUNK_GRID_SIZE + uChunkY;
	}

	/**
	 * Convert flat index to 2D chunk coords
	 * Must be the inverse of ChunkCoordsToIndex: index = x * DIMS + y
	 * So: x = index / DIMS, y = index % DIMS
	 */
	static inline void ChunkIndexToCoords(uint32_t uChunkIndex, uint32_t& uChunkX, uint32_t& uChunkY)
	{
		uChunkX = uChunkIndex / CHUNK_GRID_SIZE;
		uChunkY = uChunkIndex % CHUNK_GRID_SIZE;
	}
};
