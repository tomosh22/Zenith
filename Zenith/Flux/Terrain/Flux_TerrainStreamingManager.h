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
class Zenith_TerrainComponent;

// ========== Residency State ==========
// Simplified to just two states - either data is loaded or it isn't
enum class Flux_TerrainLODResidencyState : uint8_t
{
	NOT_LOADED = 0,   // LOD mesh data not in GPU memory
	RESIDENT          // LOD mesh data fully resident in GPU memory
};

// ========== LOD Allocation Info ==========
// Tracks where a LOD's data lives in the unified buffer
struct Flux_TerrainLODAllocation
{
	uint32_t m_uVertexOffset = 0;   // Offset in unified vertex buffer (in vertices)
	uint32_t m_uVertexCount = 0;    // Number of vertices
	uint32_t m_uIndexOffset = 0;    // Offset in unified index buffer (in indices)
	uint32_t m_uIndexCount = 0;     // Number of indices
};

// ========== Per-Chunk Residency Tracking ==========
struct Flux_TerrainChunkResidency
{
	Flux_TerrainLODResidencyState m_aeStates[LOD_COUNT];
	Flux_TerrainLODAllocation m_axAllocations[LOD_COUNT];
};

// ========== Simple Buffer Allocator ==========
// Best-fit allocator for managing streaming buffer space
class Flux_TerrainBufferAllocator
{
public:
	Flux_TerrainBufferAllocator();

	void Initialize(uint32_t uTotalSize, const char* szDebugName);
	void Reset();

	// Allocate a block of the given size. Returns offset, or UINT32_MAX on failure.
	uint32_t Allocate(uint32_t uSize);

	// Free a previously allocated block
	void Free(uint32_t uOffset, uint32_t uSize);

	uint32_t GetUnusedSpace() const { return m_uUnusedSpace; }
	uint32_t GetTotalSpace() const { return m_uTotalSize; }
	uint32_t GetFragmentationCount() const { return static_cast<uint32_t>(m_xFreeBlocks.size()); }

private:
	struct FreeBlock
	{
		uint32_t m_uOffset;
		uint32_t m_uSize;
		bool operator<(const FreeBlock& other) const { return m_uSize < other.m_uSize; }
	};

	std::priority_queue<FreeBlock> m_xFreeBlocks;
	uint32_t m_uTotalSize;
	uint32_t m_uUnusedSpace;
	const char* m_szDebugName;
};

// ========== Terrain Streaming Manager ==========
// Manages LOD streaming for terrain chunks.
// LOD3 is always resident (loaded at startup). LOD0-2 are streamed based on distance.
class Flux_TerrainStreamingManager
{
public:
	Flux_TerrainStreamingManager() = delete;
	~Flux_TerrainStreamingManager() = delete;

	// ========== Lifecycle ==========
	static void Initialize();
	static void Shutdown();
	static bool IsInitialized() { return s_bInitialized; }

	// ========== Buffer Registration ==========
	// Called by Zenith_TerrainComponent to register its unified buffers
	static void RegisterTerrainBuffers(Zenith_TerrainComponent* pxTerrainComponent);
	static void UnregisterTerrainBuffers();

	// ========== Main Update ==========
	// Call once per frame. Streams in/out LODs based on camera distance.
	static void UpdateStreaming(const Zenith_Maths::Vector3& xCameraPos);

	// ========== Query API ==========
	static Flux_TerrainLODResidencyState GetResidencyState(uint32_t uChunkX, uint32_t uChunkY, uint32_t uLODLevel);
	static bool GetLODAllocation(uint32_t uChunkX, uint32_t uChunkY, uint32_t uLODLevel, Flux_TerrainLODAllocation& xAllocOut);

	// ========== GPU Data Building ==========
	// Builds chunk data array for GPU upload (AABBs + LOD allocations)
	static void BuildChunkDataForGPU(Zenith_TerrainChunkData* pxChunkDataOut);
	static bool IsChunkDataDirty() { return s_bInitialized && s_bChunkDataDirty; }
	static void ClearChunkDataDirty() { s_bChunkDataDirty = false; }
	static Zenith_TerrainChunkData* GetCachedChunkDataBuffer() { return s_pxCachedChunkData; }

	// ========== Stats ==========
	struct StreamingStats
	{
		uint32_t m_uHighLODChunksResident = 0;
		uint32_t m_uStreamsThisFrame = 0;
		uint32_t m_uEvictionsThisFrame = 0;
		uint32_t m_uVertexBufferUsedMB = 0;
		uint32_t m_uVertexBufferTotalMB = 0;
		uint32_t m_uIndexBufferUsedMB = 0;
		uint32_t m_uIndexBufferTotalMB = 0;
		uint32_t m_uVertexFragments = 0;
		uint32_t m_uIndexFragments = 0;
	};
	static const StreamingStats& GetStats() { return s_xStats; }

private:
	// ========== State ==========
	static bool s_bInitialized;
	static uint32_t s_uCurrentFrame;
	static Zenith_TerrainComponent* s_pxTerrainComponent;

	// ========== Allocators ==========
	static Flux_TerrainBufferAllocator s_xVertexAllocator;
	static Flux_TerrainBufferAllocator s_xIndexAllocator;

	// ========== Chunk Data ==========
	static Flux_TerrainChunkResidency s_axChunkResidency[TOTAL_CHUNKS];
	static Zenith_AABB s_axChunkAABBs[TOTAL_CHUNKS];
	static bool s_bAABBsCached;
	static bool s_bChunkDataDirty;
	static Zenith_TerrainChunkData* s_pxCachedChunkData;

	// ========== Camera Tracking ==========
	static Zenith_Maths::Vector3 s_xLastCameraPos;
	static int32_t s_iLastCameraChunkX;
	static int32_t s_iLastCameraChunkY;

	// ========== Active Chunk Set ==========
	// Only chunks near camera are considered for streaming (optimization)
	static std::vector<uint32_t> s_xActiveChunkIndices;
	static uint32_t s_uActiveChunkRadius;

	// ========== Stats ==========
	static StreamingStats s_xStats;

	// ========== Internal Helpers ==========
	
	// Stream in a LOD for a chunk. Returns true on success.
	static bool StreamInLOD(uint32_t uChunkIndex, uint32_t uLODLevel);
	
	// Evict a LOD from a chunk
	static void EvictLOD(uint32_t uChunkIndex, uint32_t uLODLevel);
	
	// Try to free space by evicting distant LODs. Returns true if enough space was freed.
	static bool EvictToMakeSpace(uint32_t uVertexSpaceNeeded, uint32_t uIndexSpaceNeeded, const Zenith_Maths::Vector3& xCameraPos);
	
	// Calculate desired LOD for a chunk based on distance
	static uint32_t CalculateDesiredLOD(float fDistanceSq);
	
	// Rebuild the active chunk set when camera moves to a new chunk
	static void RebuildActiveChunkSet(int32_t iCameraChunkX, int32_t iCameraChunkY);
	
	// Get chunk center position
	static Zenith_Maths::Vector3 GetChunkCenter(uint32_t uChunkX, uint32_t uChunkY);
	
	// Coordinate conversion helpers
	static void WorldPosToChunkCoords(const Zenith_Maths::Vector3& xWorldPos, int32_t& iChunkX, int32_t& iChunkY);
	
	static inline uint32_t ChunkCoordsToIndex(uint32_t uChunkX, uint32_t uChunkY)
	{
		return uChunkX * CHUNK_GRID_SIZE + uChunkY;
	}
	
	static inline void ChunkIndexToCoords(uint32_t uChunkIndex, uint32_t& uChunkX, uint32_t& uChunkY)
	{
		uChunkX = uChunkIndex / CHUNK_GRID_SIZE;
		uChunkY = uChunkIndex % CHUNK_GRID_SIZE;
	}
	
	// Debug LOD name helper
	static const char* GetLODName(uint32_t uLOD)
	{
		static const char* names[] = { "LOD0", "LOD1", "LOD2", "LOD3" };
		return (uLOD < LOD_COUNT) ? names[uLOD] : "LOD?";
	}
};
