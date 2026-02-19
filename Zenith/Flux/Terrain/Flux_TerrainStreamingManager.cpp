#include "Zenith.h"
#include "Flux_TerrainStreamingManager.h"
#include "Flux_TerrainConfig.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "Vulkan/Zenith_Vulkan_MemoryManager.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Maths/Zenith_FrustumCulling.h"
#include <algorithm>
#include <fstream>

using namespace Flux_TerrainConfig;

// ========== Debug Logging ==========
DEBUGVAR bool dbg_bLogTerrainStreaming = false;

// ========== Static Member Definitions ==========
bool Flux_TerrainStreamingManager::s_bInitialized = false;
uint32_t Flux_TerrainStreamingManager::s_uCurrentFrame = 0;
Zenith_TerrainComponent* Flux_TerrainStreamingManager::s_pxTerrainComponent = nullptr;

Flux_TerrainBufferAllocator Flux_TerrainStreamingManager::s_xVertexAllocator;
Flux_TerrainBufferAllocator Flux_TerrainStreamingManager::s_xIndexAllocator;
Flux_TerrainChunkResidency Flux_TerrainStreamingManager::s_axChunkResidency[TOTAL_CHUNKS];
Zenith_AABB Flux_TerrainStreamingManager::s_axChunkAABBs[TOTAL_CHUNKS];
bool Flux_TerrainStreamingManager::s_bAABBsCached = false;
std::atomic<bool> Flux_TerrainStreamingManager::s_bChunkDataDirty{true};
Zenith_TerrainChunkData* Flux_TerrainStreamingManager::s_pxCachedChunkData = nullptr;

Zenith_Maths::Vector3 Flux_TerrainStreamingManager::s_xLastCameraPos = Zenith_Maths::Vector3(FLT_MAX, FLT_MAX, FLT_MAX);
int32_t Flux_TerrainStreamingManager::s_iLastCameraChunkX = INT32_MIN;
int32_t Flux_TerrainStreamingManager::s_iLastCameraChunkY = INT32_MIN;
std::vector<uint32_t> Flux_TerrainStreamingManager::s_xActiveChunkIndices;
uint32_t Flux_TerrainStreamingManager::s_uActiveChunkRadius = 16;

Flux_TerrainStreamingManager::StreamingStats Flux_TerrainStreamingManager::s_xStats;

// ========== HIGH LOD Chunk Size Cache ==========
// Cached size of a typical HIGH LOD chunk (set after first successful load)
// Used to pre-check available space before expensive disk I/O
static uint32_t s_uCachedHighLODVertexCount = 0;
static uint32_t s_uCachedHighLODIndexCount = 0;

// ========== Buffer Allocator Implementation ==========
// Best-fit allocator with coalescing. Free list sorted by offset.

Flux_TerrainBufferAllocator::Flux_TerrainBufferAllocator()
	: m_uTotalSize(0)
	, m_uUnusedSpace(0)
	, m_szDebugName("Unknown")
{
}

void Flux_TerrainBufferAllocator::Initialize(uint32_t uTotalSize, const char* szDebugName)
{
	m_uTotalSize = uTotalSize;
	m_uUnusedSpace = uTotalSize;
	m_szDebugName = szDebugName;

	m_axFreeBlocks.clear();

	FreeBlock xInitialBlock;
	xInitialBlock.m_uOffset = 0;
	xInitialBlock.m_uSize = uTotalSize;
	m_axFreeBlocks.push_back(xInitialBlock);
}

void Flux_TerrainBufferAllocator::Reset()
{
	m_axFreeBlocks.clear();
	m_uTotalSize = 0;
	m_uUnusedSpace = 0;
}

uint32_t Flux_TerrainBufferAllocator::Allocate(uint32_t uSize)
{
	Zenith_Profiling::Scope xProfileScope(ZENITH_PROFILE_INDEX__FLUX_TERRAIN_STREAMING_ALLOCATE);
	if (uSize == 0 || uSize > m_uUnusedSpace)
		return UINT32_MAX;

	// Best-fit: find smallest block that fits
	uint32_t uBestIdx = UINT32_MAX;
	uint32_t uBestSize = UINT32_MAX;

	for (uint32_t i = 0; i < static_cast<uint32_t>(m_axFreeBlocks.size()); ++i)
	{
		if (m_axFreeBlocks[i].m_uSize >= uSize && m_axFreeBlocks[i].m_uSize < uBestSize)
		{
			uBestIdx = i;
			uBestSize = m_axFreeBlocks[i].m_uSize;

			// Exact fit - can't do better
			if (uBestSize == uSize)
				break;
		}
	}

	if (uBestIdx == UINT32_MAX)
		return UINT32_MAX;

	uint32_t uAllocatedOffset = m_axFreeBlocks[uBestIdx].m_uOffset;
	uint32_t uRemainder = m_axFreeBlocks[uBestIdx].m_uSize - uSize;

	if (uRemainder > 0)
	{
		// Shrink the block in-place (keep sorted order since offset increases)
		m_axFreeBlocks[uBestIdx].m_uOffset += uSize;
		m_axFreeBlocks[uBestIdx].m_uSize = uRemainder;
	}
	else
	{
		// Exact fit - remove the block
		m_axFreeBlocks.erase(m_axFreeBlocks.begin() + uBestIdx);
	}

	m_uUnusedSpace -= uSize;
	return uAllocatedOffset;
}

void Flux_TerrainBufferAllocator::Free(uint32_t uOffset, uint32_t uSize)
{
	if (uSize == 0)
		return;

	m_uUnusedSpace += uSize;

	// Binary search for insertion point (list is sorted by offset)
	uint32_t uInsertIdx = 0;
	{
		uint32_t uLow = 0;
		uint32_t uHigh = static_cast<uint32_t>(m_axFreeBlocks.size());
		while (uLow < uHigh)
		{
			uint32_t uMid = (uLow + uHigh) / 2;
			if (m_axFreeBlocks[uMid].m_uOffset < uOffset)
				uLow = uMid + 1;
			else
				uHigh = uMid;
		}
		uInsertIdx = uLow;
	}

	// Insert the new free block
	FreeBlock xNewBlock;
	xNewBlock.m_uOffset = uOffset;
	xNewBlock.m_uSize = uSize;
	m_axFreeBlocks.insert(m_axFreeBlocks.begin() + uInsertIdx, xNewBlock);

	// Coalesce with right neighbor
	if (uInsertIdx + 1 < static_cast<uint32_t>(m_axFreeBlocks.size()))
	{
		FreeBlock& xCurrent = m_axFreeBlocks[uInsertIdx];
		FreeBlock& xRight = m_axFreeBlocks[uInsertIdx + 1];
		if (xCurrent.m_uOffset + xCurrent.m_uSize == xRight.m_uOffset)
		{
			xCurrent.m_uSize += xRight.m_uSize;
			m_axFreeBlocks.erase(m_axFreeBlocks.begin() + uInsertIdx + 1);
		}
	}

	// Coalesce with left neighbor
	if (uInsertIdx > 0)
	{
		FreeBlock& xLeft = m_axFreeBlocks[uInsertIdx - 1];
		FreeBlock& xCurrent = m_axFreeBlocks[uInsertIdx];
		if (xLeft.m_uOffset + xLeft.m_uSize == xCurrent.m_uOffset)
		{
			xLeft.m_uSize += xCurrent.m_uSize;
			m_axFreeBlocks.erase(m_axFreeBlocks.begin() + uInsertIdx);
		}
	}
}

// ========== Streaming Manager Implementation ==========

void Flux_TerrainStreamingManager::Initialize()
{
	if (s_bInitialized)
		return;

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Flux_TerrainStreamingManager::Initialize()");

	// Initialize stats
	s_xStats = StreamingStats();
	s_xStats.m_uVertexBufferTotalMB = STREAMING_VERTEX_BUFFER_SIZE_MB;
	s_xStats.m_uIndexBufferTotalMB = STREAMING_INDEX_BUFFER_SIZE_MB;

	// Pre-allocate chunk data buffer
	s_pxCachedChunkData = new Zenith_TerrainChunkData[TOTAL_CHUNKS];
	s_xActiveChunkIndices.reserve(1024);

	// Reset state
	s_xLastCameraPos = Zenith_Maths::Vector3(FLT_MAX, FLT_MAX, FLT_MAX);
	s_iLastCameraChunkX = INT32_MIN;
	s_iLastCameraChunkY = INT32_MIN;
	s_uCurrentFrame = 0;
	s_bChunkDataDirty.store(true, std::memory_order_release);
	s_pxTerrainComponent = nullptr;

	// Reset cached chunk size (will be recalculated on first successful stream)
	s_uCachedHighLODVertexCount = 0;
	s_uCachedHighLODIndexCount = 0;

	// Clear chunk residency - all LODs start as NOT_LOADED
	for (uint32_t i = 0; i < TOTAL_CHUNKS; ++i)
	{
		for (uint32_t uLOD = 0; uLOD < LOD_COUNT; ++uLOD)
		{
			s_axChunkResidency[i].m_aeStates[uLOD] = Flux_TerrainLODResidencyState::NOT_LOADED;
		}
	}

	s_bAABBsCached = false;

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Render", "Terrain", "Log Streaming" }, dbg_bLogTerrainStreaming);
#endif

	s_bInitialized = true;
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Flux_TerrainStreamingManager initialized");
}

void Flux_TerrainStreamingManager::Shutdown()
{
	if (!s_bInitialized)
		return;

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Flux_TerrainStreamingManager::Shutdown()");

	s_pxTerrainComponent = nullptr;

	if (s_pxCachedChunkData)
	{
		delete[] s_pxCachedChunkData;
		s_pxCachedChunkData = nullptr;
	}

	s_xActiveChunkIndices.clear();
	s_xVertexAllocator.Reset();
	s_xIndexAllocator.Reset();
	s_bAABBsCached = false;
	s_bInitialized = false;
}

void Flux_TerrainStreamingManager::RegisterTerrainBuffers(Zenith_TerrainComponent* pxTerrainComponent, const Flux_TerrainChunkInitData* pxChunkInitData)
{
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Flux_TerrainStreamingManager::RegisterTerrainBuffers()");

	s_pxTerrainComponent = pxTerrainComponent;

	// Initialize allocators for the streaming region (after LOW LOD)
	const uint32_t uMaxStreamingVertices = static_cast<uint32_t>(STREAMING_VERTEX_BUFFER_SIZE / s_pxTerrainComponent->m_uVertexStride);
	const uint32_t uMaxStreamingIndices = static_cast<uint32_t>(STREAMING_INDEX_BUFFER_SIZE / sizeof(uint32_t));

	s_xVertexAllocator.Initialize(uMaxStreamingVertices, "StreamingVertices");
	s_xIndexAllocator.Initialize(uMaxStreamingIndices, "StreamingIndices");

	// Initialize LOW LOD as RESIDENT for all chunks using pre-computed data
	// (vertex/index counts and AABBs were collected during LOW LOD loading in InitializeRenderResources)
	uint32_t uCurrentLowLODVertexOffset = 0;
	uint32_t uCurrentLowLODIndexOffset = 0;

	for (uint32_t x = 0; x < CHUNK_GRID_SIZE; ++x)
	{
		for (uint32_t y = 0; y < CHUNK_GRID_SIZE; ++y)
		{
			uint32_t uChunkIndex = ChunkCoordsToIndex(x, y);
			Flux_TerrainChunkResidency& xResidency = s_axChunkResidency[uChunkIndex];
			const Flux_TerrainChunkInitData& xInitData = pxChunkInitData[uChunkIndex];

			// Mark LOW LOD as resident with its allocation info
			xResidency.m_aeStates[LOD_LOW] = Flux_TerrainLODResidencyState::RESIDENT;
			xResidency.m_axAllocations[LOD_LOW].m_uVertexOffset = uCurrentLowLODVertexOffset;
			xResidency.m_axAllocations[LOD_LOW].m_uVertexCount = xInitData.m_uVertexCount;
			xResidency.m_axAllocations[LOD_LOW].m_uIndexOffset = uCurrentLowLODIndexOffset;
			xResidency.m_axAllocations[LOD_LOW].m_uIndexCount = xInitData.m_uIndexCount;

			uCurrentLowLODVertexOffset += xInitData.m_uVertexCount;
			uCurrentLowLODIndexOffset += xInitData.m_uIndexCount;

			// HIGH LOD starts as NOT_LOADED
			xResidency.m_aeStates[LOD_HIGH] = Flux_TerrainLODResidencyState::NOT_LOADED;

			// Cache AABB from pre-computed data
			s_axChunkAABBs[uChunkIndex] = xInitData.m_xAABB;
		}
	}
	s_bAABBsCached = true;

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Terrain buffers registered: LOW LOD resident for all %u chunks (zero redundant file reads)", TOTAL_CHUNKS);
}

void Flux_TerrainStreamingManager::UnregisterTerrainBuffers()
{
	s_pxTerrainComponent = nullptr;
	s_xVertexAllocator.Reset();
	s_xIndexAllocator.Reset();

	for (uint32_t i = 0; i < TOTAL_CHUNKS; ++i)
		for (uint32_t uLOD = 0; uLOD < LOD_COUNT; ++uLOD)
			s_axChunkResidency[i].m_aeStates[uLOD] = Flux_TerrainLODResidencyState::NOT_LOADED;

	s_bAABBsCached = false;
}

void Flux_TerrainStreamingManager::UpdateStreaming(const Zenith_Maths::Vector3& xCameraPos)
{
	if (!s_bInitialized || !s_pxTerrainComponent)
		return;

	s_uCurrentFrame++;
	s_xStats.m_uStreamsThisFrame = 0;
	s_xStats.m_uEvictionsThisFrame = 0;
	s_xLastCameraPos = xCameraPos;

	// Check if camera moved to a new chunk
	int32_t iCameraChunkX, iCameraChunkY;
	WorldPosToChunkCoords(xCameraPos, iCameraChunkX, iCameraChunkY);

	if (iCameraChunkX != s_iLastCameraChunkX || iCameraChunkY != s_iLastCameraChunkY)
	{
		RebuildActiveChunkSet(iCameraChunkX, iCameraChunkY);
		s_iLastCameraChunkX = iCameraChunkX;
		s_iLastCameraChunkY = iCameraChunkY;
		s_bChunkDataDirty.store(true, std::memory_order_release);

		if (dbg_bLogTerrainStreaming)
			Zenith_Log(LOG_CATEGORY_TERRAIN, "[Terrain] Camera moved to chunk (%d,%d), active set: %zu chunks",
				iCameraChunkX, iCameraChunkY, s_xActiveChunkIndices.size());
	}

	// Process each active chunk: determine desired LOD and stream if needed
	uint32_t uStreamsThisFrame = 0;
	uint32_t uStreamAttemptsThisFrame = 0;
	bool bAllocationFailed = false;  // Stop trying after first allocation failure (likely out of space)

	for (uint32_t uActiveIdx = 0; uActiveIdx < s_xActiveChunkIndices.size(); ++uActiveIdx)
	{
		// Stop if we've done enough successful streams or hit allocation failure
		if (uStreamsThisFrame >= MAX_UPLOADS_PER_FRAME || bAllocationFailed)
			break;

		// Also limit total attempts to prevent excessive disk I/O
		if (uStreamAttemptsThisFrame >= MAX_UPLOADS_PER_FRAME * 2)
			break;

		const uint32_t uChunkIndex = s_xActiveChunkIndices[uActiveIdx];
		uint32_t uChunkX, uChunkY;
		ChunkIndexToCoords(uChunkIndex, uChunkX, uChunkY);

		// Calculate distance to chunk center
		Zenith_Maths::Vector3 xChunkCenter = GetChunkCenter(uChunkX, uChunkY);
		float fDistanceSq = glm::distance2(xCameraPos, xChunkCenter);

		// Determine desired LOD based on distance
		uint32_t uDesiredLOD = CalculateDesiredLOD(fDistanceSq);
		Flux_TerrainChunkResidency& xResidency = s_axChunkResidency[uChunkIndex];

		// If desired LOD is HIGH and it's not resident, stream it in
		if (uDesiredLOD == LOD_HIGH && xResidency.m_aeStates[LOD_HIGH] != Flux_TerrainLODResidencyState::RESIDENT)
		{
			uStreamAttemptsThisFrame++;

			if (StreamInLOD(uChunkIndex, LOD_HIGH))
			{
				uStreamsThisFrame++;
				s_xStats.m_uStreamsThisFrame++;
				s_bChunkDataDirty.store(true, std::memory_order_release);

				if (dbg_bLogTerrainStreaming)
					Zenith_Log(LOG_CATEGORY_TERRAIN, "[Terrain] Streamed in chunk (%u,%u) HIGH", uChunkX, uChunkY);
			}
			else
			{
				// Allocation failed - stop trying this frame (buffer likely full/fragmented)
				bAllocationFailed = true;

				if (dbg_bLogTerrainStreaming)
					Zenith_Log(LOG_CATEGORY_TERRAIN, "[Terrain] Stream failed for chunk (%u,%u), stopping this frame", uChunkX, uChunkY);
			}
		}
	}

	// Evict distant HIGH LODs that are no longer needed (LOW is always resident)
	// Limit evictions per frame to avoid mass pop-in when camera teleports
	uint32_t uEvictionsThisFrame = 0;
	for (uint32_t i = 0; i < TOTAL_CHUNKS && uEvictionsThisFrame < MAX_EVICTIONS_PER_FRAME; ++i)
	{
		Flux_TerrainChunkResidency& xResidency = s_axChunkResidency[i];

		// Only HIGH LOD can be evicted
		if (xResidency.m_aeStates[LOD_HIGH] != Flux_TerrainLODResidencyState::RESIDENT)
			continue;

		uint32_t uChunkX, uChunkY;
		ChunkIndexToCoords(i, uChunkX, uChunkY);
		Zenith_Maths::Vector3 xChunkCenter = GetChunkCenter(uChunkX, uChunkY);
		float fDistanceSq = glm::distance2(xCameraPos, xChunkCenter);

		// Evict if chunk is well beyond HIGH LOD's range (with hysteresis)
		float fEvictionThreshold = LOD_HIGH_MAX_DISTANCE_SQ * LOD_EVICTION_HYSTERESIS;
		if (fDistanceSq > fEvictionThreshold)
		{
			EvictLOD(i, LOD_HIGH);
			uEvictionsThisFrame++;
			s_xStats.m_uEvictionsThisFrame++;
			s_bChunkDataDirty.store(true, std::memory_order_release);

			if (dbg_bLogTerrainStreaming)
				Zenith_Log(LOG_CATEGORY_TERRAIN, "[Terrain] Evicted chunk (%u,%u) HIGH (dist=%.0f, threshold=%.0f)",
					uChunkX, uChunkY, sqrtf(fDistanceSq), sqrtf(fEvictionThreshold));
		}
	}

	// Update stats periodically
	if (s_uCurrentFrame % 30 == 0)
	{
		uint32_t uHighLODResident = 0;
		for (uint32_t i = 0; i < TOTAL_CHUNKS; ++i)
			if (s_axChunkResidency[i].m_aeStates[LOD_HIGH] == Flux_TerrainLODResidencyState::RESIDENT)
				uHighLODResident++;

		s_xStats.m_uHighLODChunksResident = uHighLODResident;

		const uint32_t uVertexBytesUsed = (s_xVertexAllocator.GetTotalSpace() - s_xVertexAllocator.GetUnusedSpace()) * VERTEX_STRIDE_BYTES;
		const uint32_t uIndexBytesUsed = (s_xIndexAllocator.GetTotalSpace() - s_xIndexAllocator.GetUnusedSpace()) * 4;

		s_xStats.m_uVertexBufferUsedMB = uVertexBytesUsed / (1024 * 1024);
		s_xStats.m_uIndexBufferUsedMB = uIndexBytesUsed / (1024 * 1024);
		s_xStats.m_uVertexFragments = s_xVertexAllocator.GetFragmentationCount();
		s_xStats.m_uIndexFragments = s_xIndexAllocator.GetFragmentationCount();
	}
}

uint32_t Flux_TerrainStreamingManager::CalculateDesiredLOD(float fDistanceSq)
{
	return (fDistanceSq < LOD_HIGH_MAX_DISTANCE_SQ) ? LOD_HIGH : LOD_LOW;
}

bool Flux_TerrainStreamingManager::StreamInLOD(uint32_t uChunkIndex, uint32_t uLODLevel)
{
	Zenith_Profiling::Scope xProfileScope(ZENITH_PROFILE_INDEX__FLUX_TERRAIN_STREAMING_STREAM_IN_LOD);

	if (!s_bInitialized || !s_pxTerrainComponent)
		return false;

	// Only HIGH LOD can be streamed (LOW is always resident)
	if (uLODLevel != LOD_HIGH)
		return false;

	// Pre-check: If we know the typical chunk size, check if there's enough space
	// before doing expensive disk I/O. All HIGH LOD chunks are roughly the same size.
	if (s_uCachedHighLODVertexCount > 0 && s_uCachedHighLODIndexCount > 0)
	{
		if (s_xVertexAllocator.GetUnusedSpace() < s_uCachedHighLODVertexCount ||
			s_xIndexAllocator.GetUnusedSpace() < s_uCachedHighLODIndexCount)
		{
			// Not enough space - skip disk I/O entirely
			return false;
		}
	}

	uint32_t uChunkX, uChunkY;
	ChunkIndexToCoords(uChunkIndex, uChunkX, uChunkY);

	// Build mesh file path - HIGH LOD uses Render_X_Y.zmesh
	std::string strChunkPath = std::string(Project_GetGameAssetsDirectory()) + "Terrain/Render_" + std::to_string(uChunkX) + "_" + std::to_string(uChunkY) + ZENITH_MESH_EXT;

	// Check if file exists
	std::ifstream lodFile(strChunkPath);
	if (!lodFile.good())
		return false;

	// Load mesh to get size requirements, false so we don't upload to GPU
	Flux_MeshGeometry xChunkMesh;
	Flux_MeshGeometry::LoadFromFile(strChunkPath.c_str(), xChunkMesh, 0, false);
	if (xChunkMesh.GetNumVerts() == 0)
		return false;

	const uint32_t uNumVerts = xChunkMesh.GetNumVerts();
	const uint32_t uNumIndices = xChunkMesh.GetNumIndices();

	// Cache the chunk size for future pre-checks (all chunks are roughly the same size)
	if (s_uCachedHighLODVertexCount == 0)
	{
		s_uCachedHighLODVertexCount = uNumVerts;
		s_uCachedHighLODIndexCount = uNumIndices;
	}

	// Try to allocate space
	uint32_t uVertexOffset = s_xVertexAllocator.Allocate(uNumVerts);
	uint32_t uIndexOffset = s_xIndexAllocator.Allocate(uNumIndices);

	// If allocation failed, try evicting distant chunks to make space
	if (uVertexOffset == UINT32_MAX || uIndexOffset == UINT32_MAX)
	{
		// Clean up any partial allocation before eviction attempt
		if (uVertexOffset != UINT32_MAX)
			s_xVertexAllocator.Free(uVertexOffset, uNumVerts);
		if (uIndexOffset != UINT32_MAX)
			s_xIndexAllocator.Free(uIndexOffset, uNumIndices);

		// Try to evict distant chunks to free space
		if (!EvictToMakeSpace(uNumVerts, uNumIndices, s_xLastCameraPos))
			return false;

		// Retry allocation after eviction
		uVertexOffset = s_xVertexAllocator.Allocate(uNumVerts);
		uIndexOffset = s_xIndexAllocator.Allocate(uNumIndices);

		if (uVertexOffset == UINT32_MAX || uIndexOffset == UINT32_MAX)
		{
			if (uVertexOffset != UINT32_MAX)
				s_xVertexAllocator.Free(uVertexOffset, uNumVerts);
			if (uIndexOffset != UINT32_MAX)
				s_xIndexAllocator.Free(uIndexOffset, uNumIndices);

			return false;
		}
	}

	// Calculate absolute offsets in unified buffer (streaming region starts after LOW LOD)
	const uint32_t uAbsoluteVertexOffset = s_pxTerrainComponent->m_uLowLODVertexCount + uVertexOffset;
	const uint32_t uAbsoluteIndexOffset = s_pxTerrainComponent->m_uLowLODIndexCount + uIndexOffset;

	// Calculate byte offsets
	const uint32_t uVertexStride = s_pxTerrainComponent->m_uVertexStride;
	const uint64_t ulVertexDataSize = static_cast<uint64_t>(uNumVerts) * uVertexStride;
	const uint64_t ulVertexOffsetBytes = static_cast<uint64_t>(uAbsoluteVertexOffset) * uVertexStride;
	const uint64_t ulIndexDataSize = static_cast<uint64_t>(uNumIndices) * sizeof(uint32_t);
	const uint64_t ulIndexOffsetBytes = static_cast<uint64_t>(uAbsoluteIndexOffset) * sizeof(uint32_t);

	// Upload to GPU
	Flux_MemoryManager::UploadBufferDataAtOffset(
		s_pxTerrainComponent->GetUnifiedVertexBuffer().GetBuffer().m_xVRAMHandle,
		xChunkMesh.m_pVertexData,
		ulVertexDataSize,
		ulVertexOffsetBytes
	);

	Flux_MemoryManager::UploadBufferDataAtOffset(
		s_pxTerrainComponent->GetUnifiedIndexBuffer().GetBuffer().m_xVRAMHandle,
		xChunkMesh.m_puIndices,
		ulIndexDataSize,
		ulIndexOffsetBytes
	);

	// Update residency
	Flux_TerrainChunkResidency& xResidency = s_axChunkResidency[uChunkIndex];
	xResidency.m_axAllocations[uLODLevel].m_uVertexOffset = uAbsoluteVertexOffset;
	xResidency.m_axAllocations[uLODLevel].m_uVertexCount = uNumVerts;
	xResidency.m_axAllocations[uLODLevel].m_uIndexOffset = uAbsoluteIndexOffset;
	xResidency.m_axAllocations[uLODLevel].m_uIndexCount = uNumIndices;
	xResidency.m_aeStates[uLODLevel] = Flux_TerrainLODResidencyState::RESIDENT;

	// xChunkMesh automatically destroyed when going out of scope
	return true;
}

void Flux_TerrainStreamingManager::EvictLOD(uint32_t uChunkIndex, uint32_t uLODLevel)
{
	if (!s_bInitialized || !s_pxTerrainComponent)
		return;

	Flux_TerrainChunkResidency& xResidency = s_axChunkResidency[uChunkIndex];

	if (xResidency.m_aeStates[uLODLevel] != Flux_TerrainLODResidencyState::RESIDENT)
		return;

	// Free allocations (convert absolute to relative offsets)
	Flux_TerrainLODAllocation& xAlloc = xResidency.m_axAllocations[uLODLevel];
	const uint32_t uRelativeVertexOffset = xAlloc.m_uVertexOffset - s_pxTerrainComponent->m_uLowLODVertexCount;
	const uint32_t uRelativeIndexOffset = xAlloc.m_uIndexOffset - s_pxTerrainComponent->m_uLowLODIndexCount;

	s_xVertexAllocator.Free(uRelativeVertexOffset, xAlloc.m_uVertexCount);
	s_xIndexAllocator.Free(uRelativeIndexOffset, xAlloc.m_uIndexCount);

	xResidency.m_aeStates[uLODLevel] = Flux_TerrainLODResidencyState::NOT_LOADED;
}

bool Flux_TerrainStreamingManager::EvictToMakeSpace(uint32_t uVertexSpaceNeeded, uint32_t uIndexSpaceNeeded, const Zenith_Maths::Vector3& xCameraPos)
{
	Zenith_Profiling::Scope xProfileScope(ZENITH_PROFILE_INDEX__FLUX_TERRAIN_STREAMING_EVICT);

	// Build list of eviction candidates (resident HIGH LODs that are far from camera)
	struct EvictionCandidate
	{
		uint32_t m_uChunkIndex;
		float m_fDistanceSq;
	};

	std::vector<EvictionCandidate> candidates;

	for (uint32_t i = 0; i < TOTAL_CHUNKS; ++i)
	{
		Flux_TerrainChunkResidency& xResidency = s_axChunkResidency[i];

		// Only HIGH LOD can be evicted
		if (xResidency.m_aeStates[LOD_HIGH] != Flux_TerrainLODResidencyState::RESIDENT)
			continue;

		uint32_t uChunkX, uChunkY;
		ChunkIndexToCoords(i, uChunkX, uChunkY);
		Zenith_Maths::Vector3 xChunkCenter = GetChunkCenter(uChunkX, uChunkY);
		float fDistanceSq = glm::distance2(xCameraPos, xChunkCenter);

		// Only consider for eviction if beyond HIGH LOD's range
		if (fDistanceSq > LOD_HIGH_MAX_DISTANCE_SQ * LOD_FORCED_EVICTION_HYSTERESIS)
		{
			EvictionCandidate candidate;
			candidate.m_uChunkIndex = i;
			candidate.m_fDistanceSq = fDistanceSq;
			candidates.push_back(candidate);
		}
	}

	if (candidates.empty())
		return false;

	// Sort by distance (farthest first)
	std::sort(candidates.begin(), candidates.end(), [](const EvictionCandidate& a, const EvictionCandidate& b) {
		return a.m_fDistanceSq > b.m_fDistanceSq;
	});

	// Evict until we have enough space
	uint32_t uVertexSpaceFreed = 0;
	uint32_t uIndexSpaceFreed = 0;
	uint32_t uEvictionsThisCall = 0;

	for (const EvictionCandidate& candidate : candidates)
	{
		if (uVertexSpaceFreed >= uVertexSpaceNeeded && uIndexSpaceFreed >= uIndexSpaceNeeded)
			break;

		if (uEvictionsThisCall >= MAX_EVICTIONS_PER_FRAME)
			break;

		Flux_TerrainChunkResidency& xResidency = s_axChunkResidency[candidate.m_uChunkIndex];
		Flux_TerrainLODAllocation& xAlloc = xResidency.m_axAllocations[LOD_HIGH];

		uVertexSpaceFreed += xAlloc.m_uVertexCount;
		uIndexSpaceFreed += xAlloc.m_uIndexCount;

		EvictLOD(candidate.m_uChunkIndex, LOD_HIGH);
		uEvictionsThisCall++;
	}

	return (uVertexSpaceFreed >= uVertexSpaceNeeded && uIndexSpaceFreed >= uIndexSpaceNeeded);
}

Flux_TerrainLODResidencyState Flux_TerrainStreamingManager::GetResidencyState(uint32_t uChunkX, uint32_t uChunkY, uint32_t uLODLevel)
{
	if (!s_bInitialized)
		return Flux_TerrainLODResidencyState::NOT_LOADED;

	uint32_t uChunkIndex = ChunkCoordsToIndex(uChunkX, uChunkY);
	return s_axChunkResidency[uChunkIndex].m_aeStates[uLODLevel];
}

bool Flux_TerrainStreamingManager::GetLODAllocation(uint32_t uChunkX, uint32_t uChunkY, uint32_t uLODLevel, Flux_TerrainLODAllocation& xAllocOut)
{
	if (!s_bInitialized)
		return false;

	uint32_t uChunkIndex = ChunkCoordsToIndex(uChunkX, uChunkY);
	const Flux_TerrainChunkResidency& xResidency = s_axChunkResidency[uChunkIndex];

	if (xResidency.m_aeStates[uLODLevel] == Flux_TerrainLODResidencyState::RESIDENT)
	{
		xAllocOut = xResidency.m_axAllocations[uLODLevel];
		return true;
	}

	return false;
}

Zenith_Maths::Vector3 Flux_TerrainStreamingManager::GetChunkCenter(uint32_t uChunkX, uint32_t uChunkY)
{
	uint32_t uChunkIndex = ChunkCoordsToIndex(uChunkX, uChunkY);

	if (s_bAABBsCached)
	{
		const Zenith_AABB& xAABB = s_axChunkAABBs[uChunkIndex];
		return (xAABB.m_xMin + xAABB.m_xMax) * 0.5f;
	}

	// Fallback (shouldn't happen after initialization)
	const float fX = (static_cast<float>(uChunkX) + 0.5f) * CHUNK_WORLD_SIZE;
	const float fZ = (static_cast<float>(uChunkY) + 0.5f) * CHUNK_WORLD_SIZE;
	const float fY = MAX_TERRAIN_HEIGHT * 0.5f;

	return Zenith_Maths::Vector3(fX, fY, fZ);
}

void Flux_TerrainStreamingManager::WorldPosToChunkCoords(const Zenith_Maths::Vector3& xWorldPos, int32_t& iChunkX, int32_t& iChunkY)
{
	iChunkX = static_cast<int32_t>(xWorldPos.x / CHUNK_WORLD_SIZE);
	iChunkY = static_cast<int32_t>(xWorldPos.z / CHUNK_WORLD_SIZE);

	// Clamp to valid range (use std:: prefix to avoid Windows macro conflicts)
	if (iChunkX < 0) iChunkX = 0;
	if (iChunkX >= static_cast<int32_t>(CHUNK_GRID_SIZE)) iChunkX = static_cast<int32_t>(CHUNK_GRID_SIZE - 1);
	if (iChunkY < 0) iChunkY = 0;
	if (iChunkY >= static_cast<int32_t>(CHUNK_GRID_SIZE)) iChunkY = static_cast<int32_t>(CHUNK_GRID_SIZE - 1);
}

void Flux_TerrainStreamingManager::RebuildActiveChunkSet(int32_t iCameraChunkX, int32_t iCameraChunkY)
{
	s_xActiveChunkIndices.clear();

	const int32_t iRadius = static_cast<int32_t>(s_uActiveChunkRadius);
	const int32_t iGridMax = static_cast<int32_t>(CHUNK_GRID_SIZE - 1);
	
	// Calculate bounds (avoid Windows min/max macro conflicts)
	int32_t iMinX = iCameraChunkX - iRadius;
	int32_t iMaxX = iCameraChunkX + iRadius;
	int32_t iMinY = iCameraChunkY - iRadius;
	int32_t iMaxY = iCameraChunkY + iRadius;
	
	if (iMinX < 0) iMinX = 0;
	if (iMaxX > iGridMax) iMaxX = iGridMax;
	if (iMinY < 0) iMinY = 0;
	if (iMaxY > iGridMax) iMaxY = iGridMax;

	for (int32_t x = iMinX; x <= iMaxX; ++x)
		for (int32_t y = iMinY; y <= iMaxY; ++y)
			s_xActiveChunkIndices.push_back(ChunkCoordsToIndex(static_cast<uint32_t>(x), static_cast<uint32_t>(y)));
}

void Flux_TerrainStreamingManager::BuildChunkDataForGPU(Zenith_TerrainChunkData* pxChunkDataOut)
{
	if (!s_bInitialized)
		return;

	for (uint32_t x = 0; x < CHUNK_GRID_SIZE; ++x)
	{
		for (uint32_t y = 0; y < CHUNK_GRID_SIZE; ++y)
		{
			uint32_t uChunkIndex = ChunkCoordsToIndex(x, y);
			const Flux_TerrainChunkResidency& xResidency = s_axChunkResidency[uChunkIndex];
			Zenith_TerrainChunkData& xChunkData = pxChunkDataOut[uChunkIndex];

			// Set AABB
			if (s_bAABBsCached)
			{
				const Zenith_AABB& xAABB = s_axChunkAABBs[uChunkIndex];
				xChunkData.m_xAABBMin = Zenith_Maths::Vector4(xAABB.m_xMin, 0.0f);
				xChunkData.m_xAABBMax = Zenith_Maths::Vector4(xAABB.m_xMax, 0.0f);
			}

			// Set LOD data (HIGH and LOW)
			for (uint32_t uLOD = 0; uLOD < LOD_COUNT; ++uLOD)
			{
				xChunkData.m_axLODs[uLOD].m_fMaxDistance = LOD_MAX_DISTANCE_SQ[uLOD];

				if (xResidency.m_aeStates[uLOD] == Flux_TerrainLODResidencyState::RESIDENT)
				{
					const Flux_TerrainLODAllocation& xAlloc = xResidency.m_axAllocations[uLOD];
					xChunkData.m_axLODs[uLOD].m_uFirstIndex = xAlloc.m_uIndexOffset;
					xChunkData.m_axLODs[uLOD].m_uIndexCount = xAlloc.m_uIndexCount;

					// LOW LOD uses combined buffer with rebased indices (vertexOffset = 0)
					// HIGH LOD uses individually uploaded chunks with 0-based indices (need offset)
					if (uLOD == LOD_LOW)
						xChunkData.m_axLODs[uLOD].m_uVertexOffset = 0;
					else
						xChunkData.m_axLODs[uLOD].m_uVertexOffset = xAlloc.m_uVertexOffset;
				}
				else
				{
					// Fall back to LOW LOD (always resident)
					const Flux_TerrainLODAllocation& xLowAlloc = xResidency.m_axAllocations[LOD_LOW];
					xChunkData.m_axLODs[uLOD].m_uFirstIndex = xLowAlloc.m_uIndexOffset;
					xChunkData.m_axLODs[uLOD].m_uIndexCount = xLowAlloc.m_uIndexCount;
					xChunkData.m_axLODs[uLOD].m_uVertexOffset = 0;
				}
			}
		}
	}
}
