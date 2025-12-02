#pragma once

#include "Flux/Flux_Buffers.h"
#include "Maths/Zenith_Maths.h"
#include "Maths/Zenith_FrustumCulling.h"
#include <vector>
#include <queue>
#include <cstdint>

// Terrain constants (must match Zenith_TerrainComponent.h)
#define TERRAIN_EXPORT_DIMS 64
#define TERRAIN_LOD_COUNT 4
#define TERRAIN_SIZE 64
#define TERRAIN_SCALE 8
#define MAX_TERRAIN_HEIGHT 2048

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
	Flux_TerrainLODResidencyState m_aeStates[TERRAIN_LOD_COUNT];
	Flux_TerrainLODAllocation m_axAllocations[TERRAIN_LOD_COUNT];

	// Last frame this chunk was requested at each LOD
	// Used for eviction heuristics (LRU-style)
	uint32_t m_auLastRequestedFrame[TERRAIN_LOD_COUNT];

	// Priority score for each LOD (lower distance = higher priority)
	float m_afPriorities[TERRAIN_LOD_COUNT];
};

// Simple free-list allocator for buffer space
class Flux_TerrainBufferAllocator
{
public:
	Flux_TerrainBufferAllocator();

	void Initialize(uint32_t uTotalSize, const char* szDebugName);

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
class Flux_TerrainStreamingManager
{
public:
	static void Initialize();
	static void Shutdown();

	static Flux_TerrainStreamingManager& Get() { return *s_pxInstance; }

	// ========== Streaming API ==========

	/**
	 * Update streaming decisions based on current camera position
	 * Called once per frame before terrain rendering
	 * @param xCameraPos Current camera position for distance calculations
	 */
	void UpdateStreaming(const Zenith_Maths::Vector3& xCameraPos);

	/**
	 * Request a specific LOD for a chunk to be resident
	 * If not already resident, will queue for streaming
	 * @param uChunkX Chunk X coordinate (0-63)
	 * @param uChunkY Chunk Y coordinate (0-63)
	 * @param uLODLevel LOD level (0=highest detail, 3=lowest)
	 * @param fPriority Priority score (lower = more important)
	 * @return true if LOD is resident and ready to use
	 */
	bool RequestLOD(uint32_t uChunkX, uint32_t uChunkY, uint32_t uLODLevel, float fPriority);

	/**
	 * Get the buffer offsets for a chunk's LOD if it's resident
	 * @param uChunkX Chunk X coordinate
	 * @param uChunkY Chunk Y coordinate
	 * @param uLODLevel LOD level
	 * @param xAllocOut [out] Allocation info if resident
	 * @return true if LOD is resident, false if need to fall back to LOD3
	 */
	bool GetLODAllocation(uint32_t uChunkX, uint32_t uChunkY, uint32_t uLODLevel, Flux_TerrainLODAllocation& xAllocOut) const;

	/**
	 * Get residency state for a chunk's LOD
	 */
	Flux_TerrainLODResidencyState GetResidencyState(uint32_t uChunkX, uint32_t uChunkY, uint32_t uLODLevel) const;

	/**
	 * Build chunk data array for GPU upload
	 * This is called by Zenith_TerrainComponent::BuildChunkData() to get the current
	 * allocation info for all chunks and LODs
	 * @param pxChunkDataOut [out] Array of TERRAIN_EXPORT_DIMS * TERRAIN_EXPORT_DIMS chunk data structs
	 */
	void BuildChunkDataForGPU(Zenith_TerrainChunkData* pxChunkDataOut) const;

	// ========== Buffer Access ==========

	/**
	 * Get the unified terrain vertex/index buffers
	 * These buffers contain both LOD3 (always-resident, at the beginning)
	 * and streaming LOD0-2 data (dynamically allocated in the rest of the buffer)
	 */
	const Flux_VertexBuffer& GetTerrainVertexBuffer() const { return m_xUnifiedVertexBuffer; }
	const Flux_IndexBuffer& GetTerrainIndexBuffer() const { return m_xUnifiedIndexBuffer; }

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

	const StreamingStats& GetStats() const { return m_xStats; }
	void LogStats() const;

private:
	Flux_TerrainStreamingManager() = default;
	~Flux_TerrainStreamingManager() = default;

	static Flux_TerrainStreamingManager* s_pxInstance;

	// Frame counter for LRU tracking
	uint32_t m_uCurrentFrame = 0;

	// ========== GPU Buffers ==========

	// Unified buffers containing both LOD3 (reserved at start) and streaming LOD0-2 data
	Flux_VertexBuffer m_xUnifiedVertexBuffer;
	Flux_IndexBuffer m_xUnifiedIndexBuffer;

	// Buffer sizes for bounds checking
	uint64_t m_ulUnifiedVertexBufferSize;  // Total size in bytes
	uint64_t m_ulUnifiedIndexBufferSize;   // Total size in bytes
	uint32_t m_uVertexStride;              // Bytes per vertex

	// Reserved space tracking for LOD3 (never evicted)
	uint32_t m_uLOD3VertexCount;   // Number of vertices reserved for LOD3 at buffer start
	uint32_t m_uLOD3IndexCount;     // Number of indices reserved for LOD3 at buffer start

	// ========== Allocation Tracking ==========

	Flux_TerrainBufferAllocator m_xVertexAllocator;
	Flux_TerrainBufferAllocator m_xIndexAllocator;

	// Per-chunk residency state (4096 chunks = 64x64)
	Flux_TerrainChunkResidency m_axChunkResidency[TERRAIN_EXPORT_DIMS * TERRAIN_EXPORT_DIMS];

	// Cached AABBs for all chunks (computed once, reused for all chunk data updates)
	// Mutable because caching is an implementation detail that doesn't affect logical const-ness
	mutable Zenith_AABB m_axChunkAABBs[TERRAIN_EXPORT_DIMS * TERRAIN_EXPORT_DIMS];
	mutable bool m_bAABBsCached = false;

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

	std::priority_queue<StreamingRequest> m_xStreamingQueue;

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

	StreamingStats m_xStats;

	// ========== Internal Helpers ==========

	/**
	 * Process streaming queue and upload LOD mesh data to GPU
	 * Called during UpdateStreaming()
	 */
	void ProcessStreamingQueue();

	/**
	 * Evict one or more low-priority LODs to make space
	 * @param uVertexSpaceNeeded Vertices needed
	 * @param uIndexSpaceNeeded Indices needed
	 * @return true if enough space was freed
	 */
	bool EvictToMakeSpace(uint32_t uVertexSpaceNeeded, uint32_t uIndexSpaceNeeded);

	/**
	 * Stream in a specific chunk's LOD mesh data
	 * Assumes space has already been allocated
	 * @param uChunkIndex Flat chunk index
	 * @param uLODLevel LOD level to load
	 * @param uVertexOffset Offset in streaming vertex buffer
	 * @param uIndexOffset Offset in streaming index buffer
	 * @return true on success
	 */
	bool StreamInLOD(uint32_t uChunkIndex, uint32_t uLODLevel, uint32_t uVertexOffset, uint32_t uIndexOffset);

	/**
	 * Evict a specific chunk's LOD from GPU memory
	 * @param uChunkIndex Flat chunk index
	 * @param uLODLevel LOD level to evict
	 */
	void EvictLOD(uint32_t uChunkIndex, uint32_t uLODLevel);

	/**
	 * Build eviction candidate list from all resident high LODs
	 * @param xCameraPos Current camera position for priority calculation
	 */
	std::vector<EvictionCandidate> BuildEvictionCandidates(const Zenith_Maths::Vector3& xCameraPos) const;

	/**
	 * Calculate chunk center position for distance calculations
	 */
	Zenith_Maths::Vector3 GetChunkCenter(uint32_t uChunkX, uint32_t uChunkY) const;

	/**
	 * Convert 2D chunk coords to flat index
	 * NOTE: Uses x * DIMS + y to match the loop order (x outer, y inner) used throughout
	 * the terrain streaming and combine code.
	 */
	inline uint32_t ChunkCoordsToIndex(uint32_t uChunkX, uint32_t uChunkY) const
	{
		return uChunkX * TERRAIN_EXPORT_DIMS + uChunkY;
	}

	/**
	 * Convert flat index to 2D chunk coords
	 * Must be the inverse of ChunkCoordsToIndex: index = x * DIMS + y
	 * So: x = index / DIMS, y = index % DIMS
	 */
	inline void ChunkIndexToCoords(uint32_t uChunkIndex, uint32_t& uChunkX, uint32_t& uChunkY) const
	{
		uChunkX = uChunkIndex / TERRAIN_EXPORT_DIMS;
		uChunkY = uChunkIndex % TERRAIN_EXPORT_DIMS;
	}
};
