#pragma once

#include "Flux/Flux_Buffers.h"
#include "Flux/Terrain/Flux_TerrainConfig.h"
#include "Maths/Zenith_Maths.h"
#include "Maths/Zenith_FrustumCulling.h"
#include "Collections/Zenith_Vector.h"
#include "Core/Multithreading/Zenith_Multithreading.h"
#include <vector>
#include <cstdint>
#include <atomic>

// Use unified terrain configuration
using namespace Flux_TerrainConfig;

// Forward declarations
class Flux_MeshGeometry;
struct Zenith_TerrainChunkData;
class Zenith_TerrainComponent;

// ========== Residency State ==========
enum class Flux_TerrainLODResidencyState : uint8_t
{
	NOT_LOADED = 0,
	RESIDENT
};

// ========== LOD Allocation Info ==========
struct Flux_TerrainLODAllocation
{
	uint32_t m_uVertexOffset = 0;
	uint32_t m_uVertexCount = 0;
	uint32_t m_uIndexOffset = 0;
	uint32_t m_uIndexCount = 0;
};

// ========== Per-Chunk Residency Tracking ==========
struct Flux_TerrainChunkResidency
{
	Flux_TerrainLODResidencyState m_aeStates[LOD_COUNT]      = {};
	Flux_TerrainLODAllocation     m_axAllocations[LOD_COUNT] = {};
};

// ========== Buffer Allocator ==========
class Flux_TerrainBufferAllocator
{
public:
	Flux_TerrainBufferAllocator();

	void Initialize(uint32_t uTotalSize, const char* szDebugName);
	void Reset();

	uint32_t Allocate(uint32_t uSize);
	void Free(uint32_t uOffset, uint32_t uSize);

	uint32_t GetUnusedSpace() const { return m_uUnusedSpace; }
	uint32_t GetTotalSpace() const { return m_uTotalSize; }
	uint32_t GetFragmentationCount() const { return static_cast<uint32_t>(m_axFreeBlocks.size()); }

private:
	struct FreeBlock
	{
		uint32_t m_uOffset;
		uint32_t m_uSize;
	};

	std::vector<FreeBlock> m_axFreeBlocks;
	uint32_t m_uTotalSize;
	uint32_t m_uUnusedSpace;
	const char* m_szDebugName;
};

// ========== Per-Chunk Init Data ==========
struct Flux_TerrainChunkInitData
{
	uint32_t m_uVertexCount = 0;
	uint32_t m_uIndexCount  = 0;
	Zenith_AABB m_xAABB;
};

// ========== Stream-In Result ==========
enum class Flux_TerrainStreamInResult : uint8_t
{
	Success,
	MissingOrInvalidSource,
	AllocationFailure,
};

// ========== Streaming Stats ==========
struct Flux_TerrainStreamingStats
{
	uint32_t m_uHighLODChunksResident = 0;
	uint32_t m_uStreamsThisFrame = 0;
	uint32_t m_uEvictionsThisFrame = 0;
	uint32_t m_uVertexBufferUsedMB = 0;
	uint32_t m_uVertexBufferTotalMB = 0;
	uint32_t m_uIndexBufferUsedMB = 0;
	uint32_t m_uIndexBufferTotalMB = 0;
	uint32_t m_uVertexBufferFragments = 0;
	uint32_t m_uIndexBufferFragments = 0;
	uint32_t m_uVertexFragments = 0;
	uint32_t m_uIndexFragments = 0;
};

// ========== Per-Terrain Streaming State ==========
struct Flux_TerrainStreamingState
{
	Flux_TerrainBufferAllocator m_xVertexAllocator;
	Flux_TerrainBufferAllocator m_xIndexAllocator;
	Flux_TerrainChunkResidency  m_axChunkResidency[TOTAL_CHUNKS];
	Zenith_AABB                 m_axChunkAABBs[TOTAL_CHUNKS];
	bool                        m_bAABBsCached      = false;

	uint32_t                    m_uCachedHighLODVertexCount = 0;
	uint32_t                    m_uCachedHighLODIndexCount  = 0;

	std::atomic<bool>           m_bChunkDataDirty   { true };
	Zenith_TerrainChunkData*    m_pxCachedChunkData = nullptr;

	Zenith_Maths::Vector3       m_xLastCameraPos    { FLT_MAX, FLT_MAX, FLT_MAX };
	int32_t                     m_iLastCameraChunkX = INT32_MIN;
	int32_t                     m_iLastCameraChunkY = INT32_MIN;

	std::vector<uint32_t>       m_xActiveChunkIndices;
	uint32_t                    m_uActiveChunkRadius = 16;

	Flux_TerrainStreamingStats  m_xStats;
	uint32_t                    m_uCurrentFrame      = 0;

	Zenith_TerrainComponent*    m_pxOwner = nullptr;

	void Initialize(Zenith_TerrainComponent* pxOwner);
	void Shutdown();
};

// Phase 9: state + behaviour for the terrain streaming manager.
//
// Methods that touch the registry are non-static instance members
// (Initialize / Shutdown / Register* / Unregister* / UpdateStreaming* /
// BuildChunkData* / IsChunkDataDirty / ClearChunkDataDirty /
// GetCachedChunkDataBuffer / GetStateFor / IsInitialized).
//
// Methods that take a Flux_TerrainStreamingState& and operate purely on
// that state are kept as `static` member functions -- they're stateless
// utilities, not engine-state mutators.
class Flux_TerrainStreamingManagerImpl
{
public:
	Flux_TerrainStreamingManagerImpl() = default;
	~Flux_TerrainStreamingManagerImpl() = default;

	Flux_TerrainStreamingManagerImpl(const Flux_TerrainStreamingManagerImpl&) = delete;
	Flux_TerrainStreamingManagerImpl& operator=(const Flux_TerrainStreamingManagerImpl&) = delete;

	// ========== Lifecycle ==========
	void Initialize();
	void Shutdown();
	bool IsInitialized() const { return m_bInitialized; }

	// ========== Buffer Registration ==========
	void RegisterTerrainBuffers(Zenith_TerrainComponent* pxTerrainComponent, const Flux_TerrainChunkInitData* pxChunkInitData);
	void UnregisterTerrainBuffers(Zenith_TerrainComponent* pxTerrainComponent);

	// ========== Main Update ==========
	void UpdateStreamingForTerrain(Zenith_TerrainComponent* pxTerrainComponent, const Zenith_Maths::Vector3& xCameraPos);

	// ========== GPU Data Building ==========
	void BuildChunkDataForGPU(const Zenith_TerrainComponent* pxTerrainComponent, Zenith_TerrainChunkData* pxChunkDataOut);
	bool IsChunkDataDirty(const Zenith_TerrainComponent* pxTerrainComponent);
	void ClearChunkDataDirty(const Zenith_TerrainComponent* pxTerrainComponent);
	Zenith_TerrainChunkData* GetCachedChunkDataBuffer(const Zenith_TerrainComponent* pxTerrainComponent);

	// ========== Stats type alias ==========
	using StreamingStats = Flux_TerrainStreamingStats;

	// Resolve a component to its streaming state.
	Flux_TerrainStreamingState* GetStateFor(const Zenith_TerrainComponent* pxComp);

	friend class Zenith_UnitTests;

	// ========== State ==========
	bool                                       m_bInitialized = false;
	Zenith_Vector<Flux_TerrainStreamingState*> m_xRegistry;
	Zenith_Mutex                               m_xRegistryMutex;

	// ========== Internal pure helpers (still static -- operate on State, not the manager) ==========
	struct StreamingAllocation
	{
		uint32_t m_uVertexOffset;
		uint32_t m_uIndexOffset;
	};

	struct StreamingFrameDiagnostics
	{
		uint32_t m_uNearestChunkIndex = 0;
		uint32_t m_uActiveCount = 0;
		uint32_t m_uDesiredHighCount = 0;
		uint32_t m_uAlreadyResidentHighCount = 0;
		uint32_t m_uStreamAttempts = 0;
		uint32_t m_uStreamSuccesses = 0;
		uint32_t m_uMissingSourceCount = 0;
		uint32_t m_uAllocationFailureCount = 0;
		uint32_t m_uLowZeroCount = 0;
		uint32_t m_uHighResidentCount = 0;
	};

	static float GetChunkDistanceSq(const Flux_TerrainStreamingState& xState, uint32_t uChunkIndex, const Zenith_Maths::Vector3& xWorldPos);
	static bool TryAllocateStreamingSpace(Flux_TerrainStreamingState& xState, uint32_t uNumVerts, uint32_t uNumIndices, const Zenith_Maths::Vector3& xCameraPos, StreamingAllocation& xAllocOut);
	static void UpdateStreamingStats(Flux_TerrainStreamingState& xState);
	static void RequestNearbyHighLOD(Flux_TerrainStreamingState& xState, const Zenith_Maths::Vector3& xCameraPos, StreamingFrameDiagnostics& xDiagnostics);
	static void EvictDistantHighLOD(Flux_TerrainStreamingState& xState, const Zenith_Maths::Vector3& xCameraPos);
	static Flux_TerrainStreamInResult StreamInLOD(Flux_TerrainStreamingState& xState, uint32_t uChunkIndex, uint32_t uLODLevel);
	static void EvictLOD(Flux_TerrainStreamingState& xState, uint32_t uChunkIndex, uint32_t uLODLevel);
	static bool EvictToMakeSpace(Flux_TerrainStreamingState& xState, uint32_t uVertexSpaceNeeded, uint32_t uIndexSpaceNeeded, const Zenith_Maths::Vector3& xCameraPos);
	static uint32_t CalculateDesiredLOD(float fDistanceSq);
	static void RebuildActiveChunkSet(Flux_TerrainStreamingState& xState, int32_t iCameraChunkX, int32_t iCameraChunkY, const Zenith_Maths::Vector3& xCameraPos);
	static uint32_t FindNearestChunkByAABB(const Flux_TerrainStreamingState& xState, const Zenith_Maths::Vector3& xWorldPos);
	static void ResolveCameraChunkCoords(const Flux_TerrainStreamingState& xState, const Zenith_Maths::Vector3& xWorldPos, int32_t& iChunkX, int32_t& iChunkY, uint32_t& uNearestChunkIndex);
	static uint32_t CountLowZeroChunks(const Flux_TerrainStreamingState& xState);
	static uint32_t CountHighResidentChunks(const Flux_TerrainStreamingState& xState);
	static void LogLowZeroChunkCoordinates(const Flux_TerrainStreamingState& xState, uint32_t uMaxToLog = 16);
	static void LogStreamingHeartbeat(const Flux_TerrainStreamingState& xState, const Zenith_Maths::Vector3& xCameraPos, int32_t iCameraChunkX, int32_t iCameraChunkY, const StreamingFrameDiagnostics& xDiagnostics);
	static void BuildChunkDataForGPU_Internal(const Flux_TerrainStreamingState& xState, Zenith_TerrainChunkData* pxChunkDataOut);

	static Zenith_Maths::Vector3 GetChunkCenter(const Flux_TerrainStreamingState& xState, uint32_t uChunkX, uint32_t uChunkY);
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

	static const char* GetLODName(uint32_t uLOD)
	{
		static const char* names[] = { "HIGH", "LOW" };
		return (uLOD < LOD_COUNT) ? names[uLOD] : "LOD?";
	}
};
