#include "Zenith.h"
#include "Flux_TerrainStreamingManager.h"
#include "Flux_TerrainConfig.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "AssetHandling/Zenith_AssetHandler.h"
#include "Vulkan/Zenith_Vulkan_MemoryManager.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Profiling/Zenith_Profiling.h"
#include "Maths/Zenith_FrustumCulling.h"
#include <algorithm>
#include <fstream>

// ========== Configuration (from Flux_TerrainConfig.h) ==========
// Most configuration values are now in Flux_TerrainConfig.h for consistency
// across CPU streaming and GPU culling.

using namespace Flux_TerrainConfig;

// ========== Debug Logging Configuration ==========
// Logging categories for terrain streaming system
// Enable via debug menu: Render > Terrain > Log [Category]

DEBUGVAR bool dbg_bLogTerrainStreaming = false;    // Request/completion logs
DEBUGVAR bool dbg_bLogTerrainEvictions = false;    // Eviction decision logs
DEBUGVAR bool dbg_bLogTerrainAllocations = false;  // Buffer allocator logs
DEBUGVAR bool dbg_bLogTerrainStateChanges = false; // Residency state transition logs
DEBUGVAR bool dbg_bLogTerrainVertexData = false;   // Detailed vertex data forensics

// Debug: Track specific chunk for detailed forensic logging
static constexpr uint32_t DBG_TRACKED_CHUNK_X = UINT32_MAX;  // Set to valid coords to enable
static constexpr uint32_t DBG_TRACKED_CHUNK_Y = UINT32_MAX;
static constexpr uint32_t DBG_TRACKED_LOD = 0;

// ========== Static Member Definitions ==========
bool Flux_TerrainStreamingManager::s_bInitialized = false;
uint32_t Flux_TerrainStreamingManager::s_uCurrentFrame = 0;

Flux_VertexBuffer* Flux_TerrainStreamingManager::s_pxUnifiedVertexBuffer = nullptr;
Flux_IndexBuffer* Flux_TerrainStreamingManager::s_pxUnifiedIndexBuffer = nullptr;
uint64_t Flux_TerrainStreamingManager::s_ulUnifiedVertexBufferSize = 0;
uint64_t Flux_TerrainStreamingManager::s_ulUnifiedIndexBufferSize = 0;
uint32_t Flux_TerrainStreamingManager::s_uVertexStride = 0;
uint32_t Flux_TerrainStreamingManager::s_uLOD3VertexCount = 0;
uint32_t Flux_TerrainStreamingManager::s_uLOD3IndexCount = 0;

Flux_TerrainBufferAllocator Flux_TerrainStreamingManager::s_xVertexAllocator;
Flux_TerrainBufferAllocator Flux_TerrainStreamingManager::s_xIndexAllocator;
Flux_TerrainChunkResidency Flux_TerrainStreamingManager::s_axChunkResidency[TOTAL_CHUNKS];
Zenith_AABB Flux_TerrainStreamingManager::s_axChunkAABBs[TOTAL_CHUNKS];
bool Flux_TerrainStreamingManager::s_bAABBsCached = false;

std::priority_queue<Flux_TerrainStreamingManager::StreamingRequest> Flux_TerrainStreamingManager::s_xStreamingQueue;
Flux_TerrainStreamingManager::StreamingStats Flux_TerrainStreamingManager::s_xStats;

Zenith_Maths::Vector3 Flux_TerrainStreamingManager::s_xLastCameraPos = Zenith_Maths::Vector3(FLT_MAX, FLT_MAX, FLT_MAX);
uint8_t Flux_TerrainStreamingManager::s_auDesiredLOD[TOTAL_CHUNKS];
bool Flux_TerrainStreamingManager::s_bChunkDataDirty = true;
Zenith_TerrainChunkData* Flux_TerrainStreamingManager::s_pxCachedChunkData = nullptr;
uint32_t Flux_TerrainStreamingManager::s_uStreamingUpdateInterval = 2;
std::vector<uint32_t> Flux_TerrainStreamingManager::s_xActiveChunkIndices;
uint32_t Flux_TerrainStreamingManager::s_uActiveChunkRadius = 16;
int32_t Flux_TerrainStreamingManager::s_iLastCameraChunkX = INT32_MIN;
int32_t Flux_TerrainStreamingManager::s_iLastCameraChunkY = INT32_MIN;

// ========== Flux_TerrainBufferAllocator Implementation ==========

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

	// Clear any existing blocks
	while (!m_xFreeBlocks.empty()) m_xFreeBlocks.pop();

	// Start with one large free block
	FreeBlock initialBlock;
	initialBlock.m_uOffset = 0;
	initialBlock.m_uSize = uTotalSize;
	m_xFreeBlocks.push(initialBlock);

	Zenith_Log("Flux_TerrainBufferAllocator[%s] initialized: %u total units", szDebugName, uTotalSize);
}

void Flux_TerrainBufferAllocator::Reset()
{
	while (!m_xFreeBlocks.empty()) m_xFreeBlocks.pop();
	m_uTotalSize = 0;
	m_uUnusedSpace = 0;
}

uint32_t Flux_TerrainBufferAllocator::Allocate(uint32_t uSize)
{
	if (uSize == 0 || uSize > m_uUnusedSpace)
	{
		if (dbg_bLogTerrainAllocations)
		{
			Zenith_Log("Flux_TerrainBufferAllocator[%s] FAILED to allocate %u units (free: %u)",
				m_szDebugName, uSize, m_uUnusedSpace);
		}
		return UINT32_MAX;
	}

	// Find a free block large enough (best-fit from priority queue)
	std::vector<FreeBlock> rejectedBlocks;
	uint32_t uAllocatedOffset = UINT32_MAX;

	while (!m_xFreeBlocks.empty())
	{
		FreeBlock block = m_xFreeBlocks.top();
		m_xFreeBlocks.pop();

		if (block.m_uSize >= uSize)
		{
			// Found a suitable block
			uAllocatedOffset = block.m_uOffset;

			// Return unused portion to free list
			uint32_t uRemainder = block.m_uSize - uSize;
			if (uRemainder > 0)
			{
				FreeBlock remainderBlock;
				remainderBlock.m_uOffset = block.m_uOffset + uSize;
				remainderBlock.m_uSize = uRemainder;
				m_xFreeBlocks.push(remainderBlock);
			}

			m_uUnusedSpace -= uSize;
			break;
		}
		else
		{
			// Block too small, save for later
			rejectedBlocks.push_back(block);
		}
	}

	// Return rejected blocks to the queue
	for (const FreeBlock& block : rejectedBlocks)
	{
		m_xFreeBlocks.push(block);
	}

	if (uAllocatedOffset != UINT32_MAX && dbg_bLogTerrainAllocations)
	{
		Zenith_Log("Flux_TerrainBufferAllocator[%s] allocated %u units at offset %u (free: %u, fragments: %zu)",
			m_szDebugName, uSize, uAllocatedOffset, m_uUnusedSpace, m_xFreeBlocks.size());
	}

	return uAllocatedOffset;
}

void Flux_TerrainBufferAllocator::Free(uint32_t uOffset, uint32_t uSize)
{
	if (uSize == 0)
		return;

	// Validate the free doesn't exceed total capacity
	if (uOffset + uSize > m_uTotalSize)
	{
		Zenith_Log("ERROR: Flux_TerrainBufferAllocator[%s] Free out of bounds! offset=%u, size=%u, total=%u",
			m_szDebugName, uOffset, uSize, m_uTotalSize);
		return;
	}

	// Sanity check that we're not freeing more than was ever allocated
	if (m_uUnusedSpace + uSize > m_uTotalSize)
	{
		Zenith_Log("ERROR: Flux_TerrainBufferAllocator[%s] Free would exceed capacity! unused=%u, size=%u, total=%u",
			m_szDebugName, m_uUnusedSpace, uSize, m_uTotalSize);
		return;
	}

	FreeBlock newBlock;
	newBlock.m_uOffset = uOffset;
	newBlock.m_uSize = uSize;
	m_xFreeBlocks.push(newBlock);

	m_uUnusedSpace += uSize;

	if (dbg_bLogTerrainAllocations)
	{
		Zenith_Log("Flux_TerrainBufferAllocator[%s] freed %u units at offset %u (free: %u, fragments: %zu)",
			m_szDebugName, uSize, uOffset, m_uUnusedSpace, m_xFreeBlocks.size());
	}

	// TODO: Implement defragmentation if fragmentation becomes an issue
}

void Flux_TerrainBufferAllocator::Defragment()
{
	// TODO: Implement block merging for adjacent free blocks
	// For now, priority queue handles this reasonably well
}

// ========== Flux_TerrainStreamingManager Implementation ==========

void Flux_TerrainStreamingManager::Initialize()
{
	// Allow re-initialization after shutdown (happens when terrain is recreated after state change)
	if (s_bInitialized)
	{
		Zenith_Log("Flux_TerrainStreamingManager::Initialize() - Already initialized, skipping");
		return;
	}

	Zenith_Log("==========================================================");
	Zenith_Log("Flux_TerrainStreamingManager::Initialize()");
	Zenith_Log("Initializing terrain LOD streaming system (allocators and residency only)");
	Zenith_Log("  Config: %u×%u grid (%u total chunks), %u LOD levels",
		CHUNK_GRID_SIZE, CHUNK_GRID_SIZE, TOTAL_CHUNKS, LOD_COUNT);
	Zenith_Log("  Chunk world size: %.1f units", CHUNK_WORLD_SIZE);
	Zenith_Log("  NOTE: Unified buffers are owned by Zenith_TerrainComponent");
	Zenith_Log("==========================================================");

	// ========== Initialize Stats ==========

	s_xStats.m_uLOD3ChunksResident = TOTAL_CHUNKS;
	s_xStats.m_uHighLODChunksResident = 0;
	s_xStats.m_uStreamingRequestsThisFrame = 0;
	s_xStats.m_uEvictionsThisFrame = 0;
	s_xStats.m_uVertexBufferUsedMB = 0;
	s_xStats.m_uVertexBufferTotalMB = STREAMING_VERTEX_BUFFER_SIZE_MB;
	s_xStats.m_uIndexBufferUsedMB = 0;
	s_xStats.m_uIndexBufferTotalMB = STREAMING_INDEX_BUFFER_SIZE_MB;

	// ========== Initialize Performance Optimization State ==========
	// Initialize all chunks to LOD3 (lowest detail, always resident)
	memset(s_auDesiredLOD, LOWEST_DETAIL_LOD, sizeof(s_auDesiredLOD));

	// Pre-allocate chunk data buffer to avoid per-frame allocations
	s_pxCachedChunkData = new Zenith_TerrainChunkData[TOTAL_CHUNKS];

	// Reserve capacity for active chunk set (max ~1024 chunks in view)
	s_xActiveChunkIndices.reserve(1024);

	// Reset camera tracking
	s_xLastCameraPos = Zenith_Maths::Vector3(FLT_MAX, FLT_MAX, FLT_MAX);
	s_iLastCameraChunkX = INT32_MIN;
	s_iLastCameraChunkY = INT32_MIN;
	s_uCurrentFrame = 0;
	s_bChunkDataDirty = true;

	// Reset buffer pointers (will be set by RegisterTerrainBuffers)
	s_pxUnifiedVertexBuffer = nullptr;
	s_pxUnifiedIndexBuffer = nullptr;
	s_ulUnifiedVertexBufferSize = 0;
	s_ulUnifiedIndexBufferSize = 0;
	s_uVertexStride = 0;
	s_uLOD3VertexCount = 0;
	s_uLOD3IndexCount = 0;

	// Clear chunk residency state (will be initialized when buffers are registered)
	for (uint32_t i = 0; i < TOTAL_CHUNKS; ++i)
	{
		for (uint32_t uLOD = 0; uLOD < LOD_COUNT; ++uLOD)
		{
			s_axChunkResidency[i].m_aeStates[uLOD] = Flux_TerrainLODResidencyState::NOT_LOADED;
			s_axChunkResidency[i].m_auLastRequestedFrame[uLOD] = 0;
		}
	}

	s_bAABBsCached = false;

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Render", "Terrain", "Log Streaming" }, dbg_bLogTerrainStreaming);
	Zenith_DebugVariables::AddBoolean({ "Render", "Terrain", "Log Evictions" }, dbg_bLogTerrainEvictions);
	Zenith_DebugVariables::AddBoolean({ "Render", "Terrain", "Log Allocations" }, dbg_bLogTerrainAllocations);
	Zenith_DebugVariables::AddBoolean({ "Render", "Terrain", "Log State Changes" }, dbg_bLogTerrainStateChanges);
#endif

	// Mark as initialized
	// Note: Allocators are NOT initialized yet - that happens in RegisterTerrainBuffers
	// when we know the LOD3 vertex count and can calculate streaming region size
	s_bInitialized = true;

	Zenith_Log("==========================================================");
	Zenith_Log("Flux_TerrainStreamingManager initialization complete");
	Zenith_Log("  Waiting for Zenith_TerrainComponent to register buffers");
	Zenith_Log("  Streaming budget: %llu MB vertices, %llu MB indices",
		STREAMING_VERTEX_BUFFER_SIZE_MB, STREAMING_INDEX_BUFFER_SIZE_MB);
	Zenith_Log("  LOD thresholds (dist²): %.0f, %.0f, %.0f, %.0f",
		LOD_DISTANCE_SQ[0], LOD_DISTANCE_SQ[1], LOD_DISTANCE_SQ[2], LOD_DISTANCE_SQ[3]);
	Zenith_Log("==========================================================");
}

void Flux_TerrainStreamingManager::Shutdown()
{
	if (!s_bInitialized)
	{
		return;
	}

	Zenith_Log("Flux_TerrainStreamingManager::Shutdown()");

	// NOTE: Unified buffers are owned by Zenith_TerrainComponent, NOT destroyed here
	// Just clear our pointers to them
	s_pxUnifiedVertexBuffer = nullptr;
	s_pxUnifiedIndexBuffer = nullptr;

	Zenith_Log("Flux_TerrainStreamingManager - Buffer references cleared (buffers owned by terrain component)");

	// Clean up pre-allocated chunk data buffer (CPU memory)
	if (s_pxCachedChunkData)
	{
		delete[] s_pxCachedChunkData;
		s_pxCachedChunkData = nullptr;
	}

	// Clear streaming queue
	while (!s_xStreamingQueue.empty()) s_xStreamingQueue.pop();

	// Clear active chunk indices
	s_xActiveChunkIndices.clear();

	// Reset allocators
	s_xVertexAllocator.Reset();
	s_xIndexAllocator.Reset();

	// Reset state
	s_bAABBsCached = false;
	s_bInitialized = false;

	Zenith_Log("Flux_TerrainStreamingManager shutdown complete");
}

const Flux_VertexBuffer* Flux_TerrainStreamingManager::GetTerrainVertexBuffer()
{
	return s_pxUnifiedVertexBuffer;
}

const Flux_IndexBuffer* Flux_TerrainStreamingManager::GetTerrainIndexBuffer()
{
	return s_pxUnifiedIndexBuffer;
}

void Flux_TerrainStreamingManager::RegisterTerrainBuffers(
	Flux_VertexBuffer* pxVertexBuffer,
	Flux_IndexBuffer* pxIndexBuffer,
	uint64_t ulVertexBufferSize,
	uint64_t ulIndexBufferSize,
	uint32_t uVertexStride,
	uint32_t uLOD3VertexCount,
	uint32_t uLOD3IndexCount)
{
	Zenith_Log("==========================================================");
	Zenith_Log("Flux_TerrainStreamingManager::RegisterTerrainBuffers()");
	Zenith_Log("  Registering terrain component's unified buffers");
	Zenith_Log("==========================================================");

	// Store buffer pointers
	s_pxUnifiedVertexBuffer = pxVertexBuffer;
	s_pxUnifiedIndexBuffer = pxIndexBuffer;
	s_ulUnifiedVertexBufferSize = ulVertexBufferSize;
	s_ulUnifiedIndexBufferSize = ulIndexBufferSize;
	s_uVertexStride = uVertexStride;
	s_uLOD3VertexCount = uLOD3VertexCount;
	s_uLOD3IndexCount = uLOD3IndexCount;

	Zenith_Log("  LOD3 region: vertices [0, %u), indices [0, %u)", uLOD3VertexCount, uLOD3IndexCount);
	Zenith_Log("  Vertex stride: %u bytes", uVertexStride);
	Zenith_Log("  Total vertex buffer size: %llu bytes", ulVertexBufferSize);
	Zenith_Log("  Total index buffer size: %llu bytes", ulIndexBufferSize);

	// ========== Initialize Allocators ==========
	// Calculate allocator sizes for the streaming region (after LOD3)
	// Allocators manage the streaming region only; offsets are relative to the start of streaming region
	// When uploading, we add LOD3VertexCount/LOD3IndexCount to get absolute buffer offsets
	const uint32_t uMaxStreamingVertices = static_cast<uint32_t>(STREAMING_VERTEX_BUFFER_SIZE / uVertexStride);
	const uint32_t uMaxStreamingIndices = static_cast<uint32_t>(STREAMING_INDEX_BUFFER_SIZE / sizeof(uint32_t));

	s_xVertexAllocator.Initialize(uMaxStreamingVertices, "StreamingVertices");
	s_xIndexAllocator.Initialize(uMaxStreamingIndices, "StreamingIndices");

	Zenith_Log("Allocators initialized:");
	Zenith_Log("  Streaming vertex capacity: %u vertices", uMaxStreamingVertices);
	Zenith_Log("  Streaming index capacity: %u indices", uMaxStreamingIndices);
	Zenith_Log("  Streaming region starts at: vertex %u, index %u", uLOD3VertexCount, uLOD3IndexCount);

	// ========== Initialize Chunk Residency State ==========
	// Initialize all chunks: LOD3 is RESIDENT (in LOD3 buffer), LOD0-2 are NOT_LOADED
	uint32_t uCurrentLOD3IndexOffset = 0;
	uint32_t uCurrentLOD3VertexOffset = 0;

	for (uint32_t x = 0; x < CHUNK_GRID_SIZE; ++x)
	{
		for (uint32_t y = 0; y < CHUNK_GRID_SIZE; ++y)
		{
			uint32_t uChunkIndex = ChunkCoordsToIndex(x, y);
			Flux_TerrainChunkResidency& xResidency = s_axChunkResidency[uChunkIndex];

			// Mark LOD3 as always resident (stored in LOD3 buffer)
			xResidency.m_aeStates[LOWEST_DETAIL_LOD] = Flux_TerrainLODResidencyState::RESIDENT;

			// Calculate LOD3 allocation (need to reload mesh to get counts)
			std::string strChunkName = "Terrain_LOD3_Calc_" + std::to_string(x) + "_" + std::to_string(y);
			std::string strChunkPath = std::string(ASSETS_ROOT"Terrain/Render_LOD3_") + std::to_string(x) + "_" + std::to_string(y) + ".zmsh";

			std::ifstream lodFile(strChunkPath);
			if (!lodFile.good())
			{
				strChunkPath = std::string(ASSETS_ROOT"Terrain/Render_") + std::to_string(x) + "_" + std::to_string(y) + ".zmsh";
			}

			Zenith_AssetHandler::AddMesh(strChunkName, strChunkPath.c_str(),
				1 << Flux_MeshGeometry::FLUX_VERTEX_ATTRIBUTE__POSITION);
			Flux_MeshGeometry& xChunkMesh = Zenith_AssetHandler::GetMesh(strChunkName);

			xResidency.m_axAllocations[LOWEST_DETAIL_LOD].m_uVertexOffset = uCurrentLOD3VertexOffset;
			xResidency.m_axAllocations[LOWEST_DETAIL_LOD].m_uVertexCount = xChunkMesh.m_uNumVerts;
			xResidency.m_axAllocations[LOWEST_DETAIL_LOD].m_uIndexOffset = uCurrentLOD3IndexOffset;
			xResidency.m_axAllocations[LOWEST_DETAIL_LOD].m_uIndexCount = xChunkMesh.m_uNumIndices;

			uCurrentLOD3VertexOffset += xChunkMesh.m_uNumVerts;
			uCurrentLOD3IndexOffset += xChunkMesh.m_uNumIndices;

			Zenith_AssetHandler::DeleteMesh(strChunkName);

			// Mark LOD0-2 as not loaded (streamable LODs)
			for (uint32_t uLOD = 0; uLOD < LOWEST_DETAIL_LOD; ++uLOD)
			{
				xResidency.m_aeStates[uLOD] = Flux_TerrainLODResidencyState::NOT_LOADED;
				xResidency.m_auLastRequestedFrame[uLOD] = 0;
			}

			xResidency.m_auLastRequestedFrame[LOWEST_DETAIL_LOD] = 0;
		}
	}

	Zenith_Log("Chunk residency state initialized: LOD3 resident for all %u chunks", TOTAL_CHUNKS);

	// ========== Cache Chunk AABBs ==========
	// Load LOD0 meshes to get accurate world-space AABBs for each chunk
	// This is needed for accurate streaming distance calculations

	Zenith_Log("Caching chunk AABBs from LOD0 meshes...");

	for (uint32_t x = 0; x < CHUNK_GRID_SIZE; ++x)
	{
		for (uint32_t y = 0; y < CHUNK_GRID_SIZE; ++y)
		{
			uint32_t uChunkIndex = ChunkCoordsToIndex(x, y);

			std::string strChunkName = "Terrain_AABB_Init_" + std::to_string(x) + "_" + std::to_string(y);
			std::string strChunkPath = std::string(ASSETS_ROOT"Terrain/Render_") + std::to_string(x) + "_" + std::to_string(y) + ".zmsh";

			Zenith_AssetHandler::AddMesh(strChunkName, strChunkPath.c_str(), 1 << Flux_MeshGeometry::FLUX_VERTEX_ATTRIBUTE__POSITION);
			Flux_MeshGeometry& xChunkMesh = Zenith_AssetHandler::GetMesh(strChunkName);

			// Generate and cache AABB
			s_axChunkAABBs[uChunkIndex] = Zenith_FrustumCulling::GenerateAABBFromVertices(
				xChunkMesh.m_pxPositions,
				xChunkMesh.m_uNumVerts
			);

			Zenith_AssetHandler::DeleteMesh(strChunkName);
		}
	}
	s_bAABBsCached = true;

	Zenith_Log("Chunk AABBs cached for %u chunks", TOTAL_CHUNKS);
	Zenith_Log("==========================================================");
	Zenith_Log("Terrain buffers registered successfully");
	Zenith_Log("==========================================================");
}

void Flux_TerrainStreamingManager::UnregisterTerrainBuffers()
{
	Zenith_Log("Flux_TerrainStreamingManager::UnregisterTerrainBuffers()");

	// Clear buffer pointers (component owns the actual buffers)
	s_pxUnifiedVertexBuffer = nullptr;
	s_pxUnifiedIndexBuffer = nullptr;
	s_ulUnifiedVertexBufferSize = 0;
	s_ulUnifiedIndexBufferSize = 0;
	s_uVertexStride = 0;
	s_uLOD3VertexCount = 0;
	s_uLOD3IndexCount = 0;

	// Reset allocators (streaming region no longer valid)
	s_xVertexAllocator.Reset();
	s_xIndexAllocator.Reset();

	// Reset residency state
	for (uint32_t i = 0; i < TOTAL_CHUNKS; ++i)
	{
		for (uint32_t uLOD = 0; uLOD < LOD_COUNT; ++uLOD)
		{
			s_axChunkResidency[i].m_aeStates[uLOD] = Flux_TerrainLODResidencyState::NOT_LOADED;
		}
	}

	s_bAABBsCached = false;

	Zenith_Log("Terrain buffers unregistered");
}

void Flux_TerrainStreamingManager::UpdateStreaming(const Zenith_Maths::Vector3& xCameraPos)
{
	// Early-out if not initialized (no terrain in scene)
	if (!s_bInitialized)
	{
		return;
	}

	s_uCurrentFrame++;

	// Reset per-frame stats
	s_xStats.m_uStreamingRequestsThisFrame = 0;
	s_xStats.m_uEvictionsThisFrame = 0;

	// Store camera position for eviction distance calculations
	s_xLastCameraPos = xCameraPos;

	// Get current camera chunk position
	int32_t iCameraChunkX, iCameraChunkY;
	WorldPosToChunkCoords(xCameraPos, iCameraChunkX, iCameraChunkY);

	// ========== Handle camera chunk change ==========
	if (iCameraChunkX != s_iLastCameraChunkX || iCameraChunkY != s_iLastCameraChunkY)
	{
		RebuildActiveChunkSet(iCameraChunkX, iCameraChunkY);
		s_iLastCameraChunkX = iCameraChunkX;
		s_iLastCameraChunkY = iCameraChunkY;
		s_bChunkDataDirty = true;

		// Filter the streaming queue - only keep requests for chunks that are still nearby
		// This fixes the issue where clearing the entire queue would prevent nearby chunks
		// from ever streaming in if the player keeps moving
		std::priority_queue<StreamingRequest> xFilteredQueue;
		uint32_t uRemovedCount = 0;

		while (!s_xStreamingQueue.empty())
		{
			StreamingRequest request = s_xStreamingQueue.top();
			s_xStreamingQueue.pop();

			// Calculate current distance to this chunk
			uint32_t uChunkX, uChunkY;
			ChunkIndexToCoords(request.m_uChunkIndex, uChunkX, uChunkY);
			Zenith_Maths::Vector3 xChunkCenter = GetChunkCenter(uChunkX, uChunkY);
			float fCurrentDistanceSq = glm::distance2(xCameraPos, xChunkCenter);

			// Keep request if chunk is still within streaming range for its LOD level
			// Use the actual LOD threshold (not eviction threshold) to be more aggressive about keeping
			if (fCurrentDistanceSq < LOD_MAX_DISTANCE_SQ[request.m_uLODLevel])
			{
				// Update priority with current distance and re-add to queue
				request.m_fPriority = fCurrentDistanceSq;
				xFilteredQueue.push(request);
			}
			else
			{
				// Request is now irrelevant - chunk is too far for this LOD
				s_axChunkResidency[request.m_uChunkIndex].m_aeStates[request.m_uLODLevel] = Flux_TerrainLODResidencyState::NOT_LOADED;
				uRemovedCount++;
			}
		}

		s_xStreamingQueue = std::move(xFilteredQueue);

		if (dbg_bLogTerrainStreaming)
		{
			Zenith_Log("[Terrain] Camera moved to chunk (%d,%d), filtered queue (removed %u, kept %zu), active set: %zu chunks",
				iCameraChunkX, iCameraChunkY, uRemovedCount, s_xStreamingQueue.size(), s_xActiveChunkIndices.size());
		}

		// Proactively evict chunks that are now far from camera
		// This frees up space BEFORE we need it, preventing the "fighting for residency" bug
		ProactivelyEvictDistantChunks(xCameraPos);
	}

	// ========== CPU-side LOD Selection and Streaming Requests ==========
	// Process every frame for responsive streaming
	for (uint32_t uActiveIdx = 0; uActiveIdx < s_xActiveChunkIndices.size(); ++uActiveIdx)
	{
		const uint32_t uChunkIndex = s_xActiveChunkIndices[uActiveIdx];
		uint32_t x, y;
		ChunkIndexToCoords(uChunkIndex, x, y);

		// Calculate distance from camera to chunk center
		Zenith_Maths::Vector3 xChunkCenter = GetChunkCenter(x, y);
		float fDistanceSq = glm::distance2(xCameraPos, xChunkCenter);

		// Determine desired LOD (simple threshold check, no hysteresis complexity)
		uint8_t uNewLOD = static_cast<uint8_t>(LOD_ALWAYS_RESIDENT);
		for (uint32_t iLOD = 0; iLOD < LOD_ALWAYS_RESIDENT; ++iLOD)
		{
			if (fDistanceSq < LOD_MAX_DISTANCE_SQ[iLOD])
			{
				uNewLOD = static_cast<uint8_t>(iLOD);
				break;
			}
		}

		// Update cached LOD
		uint8_t uOldLOD = s_auDesiredLOD[uChunkIndex];
		if (uNewLOD != uOldLOD)
		{
			s_auDesiredLOD[uChunkIndex] = uNewLOD;
			s_bChunkDataDirty = true;

			if (dbg_bLogTerrainStateChanges)
			{
				Zenith_Log("[Terrain] Chunk (%u,%u) LOD: %s -> %s (dist=%.0fm)",
					x, y, GetLODName(uOldLOD), GetLODName(uNewLOD), sqrtf(fDistanceSq));
			}
		}

		// Request streaming for non-always-resident LODs
		if (uNewLOD < LOD_ALWAYS_RESIDENT)
		{
			RequestLOD(x, y, uNewLOD, fDistanceSq);
		}
	}

	// Process streaming queue
	ProcessStreamingQueue();

	// ========== Update stats periodically ==========
	if (s_uCurrentFrame % 30 == 0)
	{
		uint32_t uHighLODResident = 0;
		for (uint32_t i = 0; i < TOTAL_CHUNKS; ++i)
		{
			for (uint32_t uLOD = 0; uLOD < LOD_ALWAYS_RESIDENT; ++uLOD)
			{
				if (s_axChunkResidency[i].m_aeStates[uLOD] == Flux_TerrainLODResidencyState::RESIDENT)
				{
					uHighLODResident++;
				}
			}
		}
		s_xStats.m_uHighLODChunksResident = uHighLODResident;

		const uint32_t uVertexBytesUsed = (s_xVertexAllocator.GetTotalSpace() - s_xVertexAllocator.GetUnusedSpace()) * VERTEX_STRIDE_BYTES;
		const uint32_t uIndexBytesUsed = (s_xIndexAllocator.GetTotalSpace() - s_xIndexAllocator.GetUnusedSpace()) * 4;

		s_xStats.m_uVertexBufferUsedMB = uVertexBytesUsed / (1024 * 1024);
		s_xStats.m_uIndexBufferUsedMB = uIndexBytesUsed / (1024 * 1024);
		s_xStats.m_uVertexFragments = s_xVertexAllocator.GetFragmentationCount();
		s_xStats.m_uIndexFragments = s_xIndexAllocator.GetFragmentationCount();

		if (s_uCurrentFrame % 60 == 0 && dbg_bLogTerrainStreaming)
		{
			LogStats();
		}
	}
}

bool Flux_TerrainStreamingManager::RequestLOD(uint32_t uChunkX, uint32_t uChunkY, uint32_t uLODLevel, float fPriority)
{
	if (!s_bInitialized) return false;

	Zenith_Assert(uChunkX < CHUNK_GRID_SIZE && uChunkY < CHUNK_GRID_SIZE, "Invalid chunk coordinates");
	Zenith_Assert(uLODLevel < LOD_COUNT, "Invalid LOD level");

	uint32_t uChunkIndex = ChunkCoordsToIndex(uChunkX, uChunkY);
	Flux_TerrainChunkResidency& xResidency = s_axChunkResidency[uChunkIndex];

	// Update last requested frame
	xResidency.m_auLastRequestedFrame[uLODLevel] = s_uCurrentFrame;

	Flux_TerrainLODResidencyState eState = xResidency.m_aeStates[uLODLevel];

	if (eState == Flux_TerrainLODResidencyState::RESIDENT)
	{
		return true;  // Already resident
	}
	else if (eState == Flux_TerrainLODResidencyState::LOADING || eState == Flux_TerrainLODResidencyState::QUEUED)
	{
		return false;  // Already in progress
	}
	else if (eState == Flux_TerrainLODResidencyState::NOT_LOADED)
	{
		// Limit queue size to prevent unbounded growth
		if (s_xStreamingQueue.size() >= MAX_QUEUE_SIZE)
		{
			return false;  // Queue full, retry next frame
		}

		// Queue for streaming
		StreamingRequest request;
		request.m_uChunkIndex = uChunkIndex;
		request.m_uLODLevel = uLODLevel;
		request.m_fPriority = fPriority;
		s_xStreamingQueue.push(request);

		xResidency.m_aeStates[uLODLevel] = Flux_TerrainLODResidencyState::QUEUED;
		s_xStats.m_uStreamingRequestsThisFrame++;

		if (dbg_bLogTerrainStateChanges)
		{
			Zenith_Log("[Terrain] Chunk (%u,%u) %s: NOT_LOADED -> QUEUED (priority=%.0f)",
				uChunkX, uChunkY, GetLODName(uLODLevel), fPriority);
		}

		return false;
	}

	return false;
}

bool Flux_TerrainStreamingManager::GetLODAllocation(uint32_t uChunkX, uint32_t uChunkY, uint32_t uLODLevel, Flux_TerrainLODAllocation& xAllocOut)
{
	if (!s_bInitialized) return false;

	Zenith_Assert(uChunkX < CHUNK_GRID_SIZE && uChunkY < CHUNK_GRID_SIZE, "Invalid chunk coordinates");
	Zenith_Assert(uLODLevel < LOD_COUNT, "Invalid LOD level");

	uint32_t uChunkIndex = ChunkCoordsToIndex(uChunkX, uChunkY);
	const Flux_TerrainChunkResidency& xResidency = s_axChunkResidency[uChunkIndex];

	if (xResidency.m_aeStates[uLODLevel] == Flux_TerrainLODResidencyState::RESIDENT)
	{
		xAllocOut = xResidency.m_axAllocations[uLODLevel];
		return true;
	}

	return false;
}

Flux_TerrainLODResidencyState Flux_TerrainStreamingManager::GetResidencyState(uint32_t uChunkX, uint32_t uChunkY, uint32_t uLODLevel)
{
	if (!s_bInitialized) return Flux_TerrainLODResidencyState::NOT_LOADED;

	Zenith_Assert(uChunkX < CHUNK_GRID_SIZE && uChunkY < CHUNK_GRID_SIZE, "Invalid chunk coordinates");
	Zenith_Assert(uLODLevel < LOD_COUNT, "Invalid LOD level");

	uint32_t uChunkIndex = ChunkCoordsToIndex(uChunkX, uChunkY);
	return s_axChunkResidency[uChunkIndex].m_aeStates[uLODLevel];
}

void Flux_TerrainStreamingManager::LogStats()
{
	if (!s_bInitialized) return;

	Zenith_Log("=== Terrain Streaming Stats (Frame %u) ===", s_uCurrentFrame);
	Zenith_Log("  LOD3 resident: %u chunks (always)", s_xStats.m_uLOD3ChunksResident);
	Zenith_Log("  High LOD (0-2) resident: %u chunks", s_xStats.m_uHighLODChunksResident);
	Zenith_Log("  Streaming requests this frame: %u", s_xStats.m_uStreamingRequestsThisFrame);
	Zenith_Log("  Evictions this frame: %u", s_xStats.m_uEvictionsThisFrame);
	Zenith_Log("  Vertex buffer: %u / %u MB (%.1f%%)",
		s_xStats.m_uVertexBufferUsedMB, s_xStats.m_uVertexBufferTotalMB,
		(s_xStats.m_uVertexBufferUsedMB * 100.0f) / s_xStats.m_uVertexBufferTotalMB);
	Zenith_Log("  Index buffer: %u / %u MB (%.1f%%)",
		s_xStats.m_uIndexBufferUsedMB, s_xStats.m_uIndexBufferTotalMB,
		(s_xStats.m_uIndexBufferUsedMB * 100.0f) / s_xStats.m_uIndexBufferTotalMB);
	Zenith_Log("  Vertex fragments: %u, Index fragments: %u",
		s_xStats.m_uVertexFragments, s_xStats.m_uIndexFragments);
}

void Flux_TerrainStreamingManager::ProcessStreamingQueue()
{
	if (!s_bInitialized) return;

	uint32_t uUploadsThisFrame = 0;

	while (!s_xStreamingQueue.empty() && uUploadsThisFrame < MAX_UPLOADS_PER_FRAME)
	{
		StreamingRequest request = s_xStreamingQueue.top();
		s_xStreamingQueue.pop();

		uint32_t uChunkIndex = request.m_uChunkIndex;
		uint32_t uLODLevel = request.m_uLODLevel;

		Flux_TerrainChunkResidency& xResidency = s_axChunkResidency[uChunkIndex];

		// Skip if already resident or loading
		if (xResidency.m_aeStates[uLODLevel] == Flux_TerrainLODResidencyState::RESIDENT ||
			xResidency.m_aeStates[uLODLevel] == Flux_TerrainLODResidencyState::LOADING)
		{
			continue;
		}

		// Transition: QUEUED -> LOADING
		xResidency.m_aeStates[uLODLevel] = Flux_TerrainLODResidencyState::LOADING;

		uint32_t uChunkX, uChunkY;
		ChunkIndexToCoords(uChunkIndex, uChunkX, uChunkY);

		// Build mesh file path
		const char* LOD_SUFFIXES[3] = { "", "_LOD1", "_LOD2" };
		std::string strChunkName = "Terrain_Streaming_LOD" + std::to_string(uLODLevel) + "_" + std::to_string(uChunkX) + "_" + std::to_string(uChunkY);
		std::string strChunkPath = std::string(ASSETS_ROOT"Terrain/Render") + LOD_SUFFIXES[uLODLevel] + "_" + std::to_string(uChunkX) + "_" + std::to_string(uChunkY) + ".zmsh";

		// Check if file exists
		std::ifstream lodFile(strChunkPath);
		if (!lodFile.good())
		{
			if (dbg_bLogTerrainStreaming)
			{
				Zenith_Log("[Terrain] WARNING: %s file not found for chunk (%u,%u)",
					GetLODName(uLODLevel), uChunkX, uChunkY);
			}
			xResidency.m_aeStates[uLODLevel] = Flux_TerrainLODResidencyState::NOT_LOADED;
			continue;
		}

		// Load mesh to get size requirements
		Zenith_AssetHandler::AddMesh(strChunkName, strChunkPath.c_str(),
			1 << Flux_MeshGeometry::FLUX_VERTEX_ATTRIBUTE__POSITION);
		Flux_MeshGeometry& xChunkMesh = Zenith_AssetHandler::GetMesh(strChunkName);

		// Try to allocate space
		uint32_t uVertexOffset = s_xVertexAllocator.Allocate(xChunkMesh.m_uNumVerts);
		uint32_t uIndexOffset = s_xIndexAllocator.Allocate(xChunkMesh.m_uNumIndices);

		if (uVertexOffset == UINT32_MAX || uIndexOffset == UINT32_MAX)
		{
			// Not enough space - attempt eviction
			if (dbg_bLogTerrainEvictions)
			{
				Zenith_Log("[Terrain] Insufficient space for chunk (%u,%u) %s, attempting eviction...",
					uChunkX, uChunkY, GetLODName(uLODLevel));
			}

			// Free partial allocation
			if (uVertexOffset != UINT32_MAX)
				s_xVertexAllocator.Free(uVertexOffset, xChunkMesh.m_uNumVerts);
			if (uIndexOffset != UINT32_MAX)
				s_xIndexAllocator.Free(uIndexOffset, xChunkMesh.m_uNumIndices);

			bool bEvictionSuccess = EvictToMakeSpace(xChunkMesh.m_uNumVerts, xChunkMesh.m_uNumIndices);
			if (!bEvictionSuccess)
			{
				xResidency.m_aeStates[uLODLevel] = Flux_TerrainLODResidencyState::NOT_LOADED;
				Zenith_AssetHandler::DeleteMesh(strChunkName);
				continue;
			}

			// Retry allocation after eviction
			uVertexOffset = s_xVertexAllocator.Allocate(xChunkMesh.m_uNumVerts);
			uIndexOffset = s_xIndexAllocator.Allocate(xChunkMesh.m_uNumIndices);

			if (uVertexOffset == UINT32_MAX || uIndexOffset == UINT32_MAX)
			{
				if (uVertexOffset != UINT32_MAX)
					s_xVertexAllocator.Free(uVertexOffset, xChunkMesh.m_uNumVerts);
				if (uIndexOffset != UINT32_MAX)
					s_xIndexAllocator.Free(uIndexOffset, xChunkMesh.m_uNumIndices);

				xResidency.m_aeStates[uLODLevel] = Flux_TerrainLODResidencyState::NOT_LOADED;
				Zenith_AssetHandler::DeleteMesh(strChunkName);
				continue;
			}
		}

		// Validate allocation bounds
		const uint32_t uMaxStreamingVertices = s_xVertexAllocator.GetTotalSpace();
		const uint32_t uMaxStreamingIndices = s_xIndexAllocator.GetTotalSpace();

		if (uVertexOffset + xChunkMesh.m_uNumVerts > uMaxStreamingVertices ||
			uIndexOffset + xChunkMesh.m_uNumIndices > uMaxStreamingIndices)
		{
			Zenith_Log("[Terrain] ERROR: Allocation out of bounds for chunk (%u,%u) %s",
				uChunkX, uChunkY, GetLODName(uLODLevel));
			s_xVertexAllocator.Free(uVertexOffset, xChunkMesh.m_uNumVerts);
			s_xIndexAllocator.Free(uIndexOffset, xChunkMesh.m_uNumIndices);
			xResidency.m_aeStates[uLODLevel] = Flux_TerrainLODResidencyState::NOT_LOADED;
			Zenith_AssetHandler::DeleteMesh(strChunkName);
			continue;
		}

		// Stream in the LOD mesh data
		bool bStreamSuccess = StreamInLOD(uChunkIndex, uLODLevel, uVertexOffset, uIndexOffset);
		if (!bStreamSuccess)
		{
			s_xVertexAllocator.Free(uVertexOffset, xChunkMesh.m_uNumVerts);
			s_xIndexAllocator.Free(uIndexOffset, xChunkMesh.m_uNumIndices);
			xResidency.m_aeStates[uLODLevel] = Flux_TerrainLODResidencyState::NOT_LOADED;
			Zenith_AssetHandler::DeleteMesh(strChunkName);
			continue;
		}

		Zenith_AssetHandler::DeleteMesh(strChunkName);
		uUploadsThisFrame++;
		s_bChunkDataDirty = true;

		if (dbg_bLogTerrainStateChanges)
		{
			Zenith_Log("[Terrain] Chunk (%u,%u) %s: LOADING -> RESIDENT (%u verts, %u indices)",
				uChunkX, uChunkY, GetLODName(uLODLevel),
				xResidency.m_axAllocations[uLODLevel].m_uVertexCount,
				xResidency.m_axAllocations[uLODLevel].m_uIndexCount);
		}
	}
}

void Flux_TerrainStreamingManager::ProactivelyEvictDistantChunks(const Zenith_Maths::Vector3& xCameraPos)
{
	if (!s_bInitialized) return;

	// Evict all resident high-LOD chunks that are now beyond their LOD's max distance
	// This is called when camera changes chunk, so we clear out old data proactively
	// rather than waiting for allocation failures

	uint32_t uEvicted = 0;

	for (uint32_t i = 0; i < TOTAL_CHUNKS; ++i)
	{
		Flux_TerrainChunkResidency& xResidency = s_axChunkResidency[i];

		for (uint32_t uLOD = 0; uLOD < LOD_ALWAYS_RESIDENT; ++uLOD)
		{
			if (xResidency.m_aeStates[uLOD] != Flux_TerrainLODResidencyState::RESIDENT)
				continue;

			// Calculate CURRENT distance to this chunk
			uint32_t uChunkX, uChunkY;
			ChunkIndexToCoords(i, uChunkX, uChunkY);
			Zenith_Maths::Vector3 xChunkCenter = GetChunkCenter(uChunkX, uChunkY);
			float fDistanceSq = glm::distance2(xCameraPos, xChunkCenter);

			// If this chunk is now beyond the streaming range for ANY high LOD,
			// evict it. The threshold is the max distance for LOD2 (the farthest streaming LOD)
			// with some margin to prevent thrashing
			const float fEvictionThreshold = LOD_MAX_DISTANCE_SQ[LOD_ALWAYS_RESIDENT - 1] * 1.5f;

			if (fDistanceSq > fEvictionThreshold)
			{
				EvictLOD(i, uLOD);
				uEvicted++;
				s_xStats.m_uEvictionsThisFrame++;
			}
		}
	}

	if (uEvicted > 0 && dbg_bLogTerrainEvictions)
	{
		Zenith_Log("[Terrain] Proactively evicted %u distant LODs", uEvicted);
	}
}

bool Flux_TerrainStreamingManager::EvictToMakeSpace(uint32_t uVertexSpaceNeeded, uint32_t uIndexSpaceNeeded)
{
	if (!s_bInitialized) return false;

	// Build eviction candidate list using CURRENT distances (not stale cached values)
	// CRITICAL: Only evict chunks that are OUTSIDE their LOD's appropriate range
	// This prevents the bug where we evict a working LOD, fail to allocate the new one
	// due to fragmentation, and end up worse off than before (falling back to LOD3)
	std::vector<EvictionCandidate> candidates;

	for (uint32_t i = 0; i < TOTAL_CHUNKS; ++i)
	{
		Flux_TerrainChunkResidency& xResidency = s_axChunkResidency[i];
		for (uint32_t uLOD = 0; uLOD < LOD_ALWAYS_RESIDENT; ++uLOD)
		{
			if (xResidency.m_aeStates[uLOD] == Flux_TerrainLODResidencyState::RESIDENT)
			{
				// Calculate CURRENT distance to camera
				uint32_t uChunkX, uChunkY;
				ChunkIndexToCoords(i, uChunkX, uChunkY);
				Zenith_Maths::Vector3 xChunkCenter = GetChunkCenter(uChunkX, uChunkY);
				float fCurrentDistanceSq = glm::distance2(s_xLastCameraPos, xChunkCenter);

				// CRITICAL FIX: Only consider for eviction if this chunk is OUTSIDE
				// the range where this LOD is appropriate.
				// For example: If LOD0 is resident and chunk is at 500m (within LOD0 range of 632m),
				// do NOT evict it! We'd lose useful data.
				// Only evict LOD0 if chunk is beyond 632m (where LOD1 or lower should be used)
				float fLODMaxDistanceSq = LOD_MAX_DISTANCE_SQ[uLOD];

				// Add some hysteresis to prevent thrashing at boundaries
				float fEvictionThreshold = fLODMaxDistanceSq * 1.2f;

				if (fCurrentDistanceSq > fEvictionThreshold)
				{
					// Chunk is beyond where this LOD is needed - safe to evict
					EvictionCandidate candidate;
					candidate.m_uChunkIndex = i;
					candidate.m_uLODLevel = uLOD;
					// Higher distance = higher eviction priority (evict far chunks first)
					candidate.m_fPriority = fCurrentDistanceSq;

					candidates.push_back(candidate);
				}
			}
		}
	}

	if (candidates.empty())
	{
		// No safe eviction candidates - all resident LODs are still needed
		// Let the caller fall back to LOD3 rather than evict working data
		if (dbg_bLogTerrainEvictions)
		{
			Zenith_Log("[Terrain] EvictToMakeSpace: No safe candidates (all LODs still in range)");
		}
		return false;
	}

	// Sort by distance - farthest chunks first
	std::sort(candidates.begin(), candidates.end(), [](const EvictionCandidate& a, const EvictionCandidate& b) {
		return a.m_fPriority > b.m_fPriority;
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
		Flux_TerrainLODAllocation& xAlloc = xResidency.m_axAllocations[candidate.m_uLODLevel];

		uVertexSpaceFreed += xAlloc.m_uVertexCount;
		uIndexSpaceFreed += xAlloc.m_uIndexCount;

		EvictLOD(candidate.m_uChunkIndex, candidate.m_uLODLevel);
		uEvictionsThisCall++;
		s_xStats.m_uEvictionsThisFrame++;
	}

	bool bSuccess = (uVertexSpaceFreed >= uVertexSpaceNeeded && uIndexSpaceFreed >= uIndexSpaceNeeded);

	if (dbg_bLogTerrainEvictions)
	{
		Zenith_Log("[Terrain] Evicted %u LODs (freed %u verts, %u indices, needed %u/%u) - %s",
			uEvictionsThisCall, uVertexSpaceFreed, uIndexSpaceFreed,
			uVertexSpaceNeeded, uIndexSpaceNeeded, bSuccess ? "SUCCESS" : "INSUFFICIENT");
	}

	return bSuccess;
}

bool Flux_TerrainStreamingManager::StreamInLOD(uint32_t uChunkIndex, uint32_t uLODLevel, uint32_t uVertexOffset, uint32_t uIndexOffset)
{
	if (!s_bInitialized) return false;

	Zenith_Assert(uLODLevel < LOD_ALWAYS_RESIDENT, "StreamInLOD only for LOD0-2");

	uint32_t uChunkX, uChunkY;
	ChunkIndexToCoords(uChunkIndex, uChunkX, uChunkY);

	Flux_TerrainChunkResidency& xResidency = s_axChunkResidency[uChunkIndex];
	xResidency.m_aeStates[uLODLevel] = Flux_TerrainLODResidencyState::LOADING;

	// Load mesh with all vertex attributes
	const char* LOD_SUFFIXES[3] = { "", "_LOD1", "_LOD2" };
	std::string strChunkName = "Terrain_Upload_LOD" + std::to_string(uLODLevel) + "_" + std::to_string(uChunkX) + "_" + std::to_string(uChunkY);
	std::string strChunkPath = std::string(ASSETS_ROOT"Terrain/Render") + LOD_SUFFIXES[uLODLevel] + "_" + std::to_string(uChunkX) + "_" + std::to_string(uChunkY) + ".zmsh";

	Zenith_AssetHandler::AddMesh(strChunkName, strChunkPath.c_str(), 0);  // 0 = all attributes
	Flux_MeshGeometry& xChunkMesh = Zenith_AssetHandler::GetMesh(strChunkName);

	// Calculate absolute offsets in unified buffer
	const uint32_t uAbsoluteVertexOffset = s_uLOD3VertexCount + uVertexOffset;
	const uint32_t uAbsoluteIndexOffset = s_uLOD3IndexCount + uIndexOffset;

	// Calculate byte offsets and sizes
	const uint32_t uVertexStride = xChunkMesh.m_xBufferLayout.GetStride();
	const uint64_t ulVertexDataSize = static_cast<uint64_t>(xChunkMesh.m_uNumVerts) * uVertexStride;
	const uint64_t ulVertexOffsetBytes = static_cast<uint64_t>(uAbsoluteVertexOffset) * uVertexStride;
	const uint64_t ulIndexDataSize = static_cast<uint64_t>(xChunkMesh.m_uNumIndices) * sizeof(uint32_t);
	const uint64_t ulIndexOffsetBytes = static_cast<uint64_t>(uAbsoluteIndexOffset) * sizeof(uint32_t);

	// Bounds check
	if (ulVertexOffsetBytes + ulVertexDataSize > s_ulUnifiedVertexBufferSize)
	{
		Zenith_Log("[Terrain] ERROR: Vertex upload exceeds buffer for chunk (%u,%u) %s",
			uChunkX, uChunkY, GetLODName(uLODLevel));
		Zenith_AssetHandler::DeleteMesh(strChunkName);
		return false;
	}

	if (ulIndexOffsetBytes + ulIndexDataSize > s_ulUnifiedIndexBufferSize)
	{
		Zenith_Log("[Terrain] ERROR: Index upload exceeds buffer for chunk (%u,%u) %s",
			uChunkX, uChunkY, GetLODName(uLODLevel));
		Zenith_AssetHandler::DeleteMesh(strChunkName);
		return false;
	}

	// Ensure buffers are registered before uploading
	if (!s_pxUnifiedVertexBuffer || !s_pxUnifiedIndexBuffer)
	{
		Zenith_Log("[Terrain] ERROR: Cannot upload LOD data - buffers not registered");
		Zenith_AssetHandler::DeleteMesh(strChunkName);
		return false;
	}

	// Upload vertex and index data (synchronous)
	Flux_MemoryManager::UploadBufferDataAtOffset(
		s_pxUnifiedVertexBuffer->GetBuffer().m_xVRAMHandle,
		xChunkMesh.m_pVertexData,
		ulVertexDataSize,
		ulVertexOffsetBytes
	);

	Flux_MemoryManager::UploadBufferDataAtOffset(
		s_pxUnifiedIndexBuffer->GetBuffer().m_xVRAMHandle,
		xChunkMesh.m_puIndices,
		ulIndexDataSize,
		ulIndexOffsetBytes
	);

	// Update residency with absolute buffer offsets
	xResidency.m_axAllocations[uLODLevel].m_uVertexOffset = uAbsoluteVertexOffset;
	xResidency.m_axAllocations[uLODLevel].m_uVertexCount = xChunkMesh.m_uNumVerts;
	xResidency.m_axAllocations[uLODLevel].m_uIndexOffset = uAbsoluteIndexOffset;
	xResidency.m_axAllocations[uLODLevel].m_uIndexCount = xChunkMesh.m_uNumIndices;
	xResidency.m_aeStates[uLODLevel] = Flux_TerrainLODResidencyState::RESIDENT;

	Zenith_AssetHandler::DeleteMesh(strChunkName);
	return true;
}

void Flux_TerrainStreamingManager::EvictLOD(uint32_t uChunkIndex, uint32_t uLODLevel)
{
	if (!s_bInitialized) return;

	Zenith_Assert(uLODLevel < LOD_ALWAYS_RESIDENT, "EvictLOD only for LOD0-2");

	uint32_t uChunkX, uChunkY;
	ChunkIndexToCoords(uChunkIndex, uChunkX, uChunkY);

	Flux_TerrainChunkResidency& xResidency = s_axChunkResidency[uChunkIndex];

	if (xResidency.m_aeStates[uLODLevel] != Flux_TerrainLODResidencyState::RESIDENT)
	{
		return;  // Not resident
	}

	// Transition: RESIDENT -> EVICTING -> NOT_LOADED
	xResidency.m_aeStates[uLODLevel] = Flux_TerrainLODResidencyState::EVICTING;

	// Free allocations (convert absolute to relative offsets)
	Flux_TerrainLODAllocation& xAlloc = xResidency.m_axAllocations[uLODLevel];
	const uint32_t uRelativeVertexOffset = xAlloc.m_uVertexOffset - s_uLOD3VertexCount;
	const uint32_t uRelativeIndexOffset = xAlloc.m_uIndexOffset - s_uLOD3IndexCount;
	s_xVertexAllocator.Free(uRelativeVertexOffset, xAlloc.m_uVertexCount);
	s_xIndexAllocator.Free(uRelativeIndexOffset, xAlloc.m_uIndexCount);

	xResidency.m_aeStates[uLODLevel] = Flux_TerrainLODResidencyState::NOT_LOADED;
	s_bChunkDataDirty = true;

	if (dbg_bLogTerrainStateChanges)
	{
		Zenith_Log("[Terrain] Chunk (%u,%u) %s: RESIDENT -> NOT_LOADED (freed %u verts, %u indices)",
			uChunkX, uChunkY, GetLODName(uLODLevel), xAlloc.m_uVertexCount, xAlloc.m_uIndexCount);
	}
}

std::vector<Flux_TerrainStreamingManager::EvictionCandidate> Flux_TerrainStreamingManager::BuildEvictionCandidates(const Zenith_Maths::Vector3& xCameraPos)
{
	std::vector<EvictionCandidate> candidates;

	if (!s_bInitialized) return candidates;

	for (uint32_t i = 0; i < TOTAL_CHUNKS; ++i)
	{
		const Flux_TerrainChunkResidency& xResidency = s_axChunkResidency[i];
		// Only consider LOD0-2 for eviction (LOD3 is always resident)
		for (uint32_t uLOD = 0; uLOD < LOWEST_DETAIL_LOD; ++uLOD)
		{
			if (xResidency.m_aeStates[uLOD] == Flux_TerrainLODResidencyState::RESIDENT)
			{
				uint32_t uChunkX, uChunkY;
				ChunkIndexToCoords(i, uChunkX, uChunkY);

				Zenith_Maths::Vector3 xChunkCenter = GetChunkCenter(uChunkX, uChunkY);
				float fDistanceSq = glm::distance2(xCameraPos, xChunkCenter);

				EvictionCandidate candidate;
				candidate.m_uChunkIndex = i;
				candidate.m_uLODLevel = uLOD;
				candidate.m_fPriority = fDistanceSq;  // Farther = higher priority to evict

				candidates.push_back(candidate);
			}
		}
	}

	return candidates;
}

Zenith_Maths::Vector3 Flux_TerrainStreamingManager::GetChunkCenter(uint32_t uChunkX, uint32_t uChunkY)
{
	uint32_t uChunkIndex = ChunkCoordsToIndex(uChunkX, uChunkY);

	// Use cached AABB if available (accurate world positions from actual mesh data)
	if (s_bAABBsCached)
	{
		const Zenith_AABB& xAABB = s_axChunkAABBs[uChunkIndex];
		return (xAABB.m_xMin + xAABB.m_xMax) * 0.5f;
	}

	// Fallback: Calculate approximate chunk center in world space
	// NOTE: Uses CHUNK_WORLD_SIZE from config (should match actual exported mesh positions)
	// This fallback should only be used before AABBs are cached
	const float fX = (static_cast<float>(uChunkX) + 0.5f) * CHUNK_WORLD_SIZE;
	const float fZ = (static_cast<float>(uChunkY) + 0.5f) * CHUNK_WORLD_SIZE;
	const float fY = MAX_TERRAIN_HEIGHT * 0.5f;  // Approximate center height

	return Zenith_Maths::Vector3(fX, fY, fZ);
}

void Flux_TerrainStreamingManager::WorldPosToChunkCoords(const Zenith_Maths::Vector3& xWorldPos, int32_t& iChunkX, int32_t& iChunkY)
{
	// Convert world position to chunk coordinates
	// CRITICAL: The export tool uses TERRAIN_SCALE=1, so actual chunk size in world space
	// is CHUNK_WORLD_SIZE (64 units), NOT TERRAIN_SIZE * TERRAIN_SCALE (512).
	// Using TERRAIN_SCALE here would cause the streaming system to think the camera
	// is in a different chunk than where it actually is, breaking LOD streaming.
	iChunkX = static_cast<int32_t>(xWorldPos.x / CHUNK_WORLD_SIZE);
	iChunkY = static_cast<int32_t>(xWorldPos.z / CHUNK_WORLD_SIZE);

	// Clamp to valid range
	iChunkX = std::max(0, std::min(iChunkX, static_cast<int32_t>(CHUNK_GRID_SIZE - 1)));
	iChunkY = std::max(0, std::min(iChunkY, static_cast<int32_t>(CHUNK_GRID_SIZE - 1)));
}

void Flux_TerrainStreamingManager::RebuildActiveChunkSet(int32_t iCameraChunkX, int32_t iCameraChunkY)
{
	// OPTIMIZATION: Only track chunks within streaming radius of camera
	// This reduces the set from 4096 to typically 256-1024 chunks depending on radius

	s_xActiveChunkIndices.clear();

	// Reserve approximate capacity to avoid reallocations
	const int32_t iDiameter = static_cast<int32_t>(s_uActiveChunkRadius * 2 + 1);
	s_xActiveChunkIndices.reserve(iDiameter * iDiameter);

	const int32_t iMinX = std::max(0, iCameraChunkX - static_cast<int32_t>(s_uActiveChunkRadius));
	const int32_t iMaxX = std::min(static_cast<int32_t>(CHUNK_GRID_SIZE - 1), iCameraChunkX + static_cast<int32_t>(s_uActiveChunkRadius));
	const int32_t iMinY = std::max(0, iCameraChunkY - static_cast<int32_t>(s_uActiveChunkRadius));
	const int32_t iMaxY = std::min(static_cast<int32_t>(CHUNK_GRID_SIZE - 1), iCameraChunkY + static_cast<int32_t>(s_uActiveChunkRadius));

	for (int32_t x = iMinX; x <= iMaxX; ++x)
	{
		for (int32_t y = iMinY; y <= iMaxY; ++y)
		{
			s_xActiveChunkIndices.push_back(ChunkCoordsToIndex(static_cast<uint32_t>(x), static_cast<uint32_t>(y)));
		}
	}
}

void Flux_TerrainStreamingManager::BuildChunkDataForGPU(Zenith_TerrainChunkData* pxChunkDataOut)
{
	if (!s_bInitialized) return;

	// One-time debug log to verify chunk data
	static bool s_bLoggedOnce = false;

	// Cache AABBs on first call (expensive - loads all meshes)
	// Subsequent calls reuse cached AABBs (cheap - just updates LOD data)
	if (!s_bAABBsCached)
	{
		for (uint32_t x = 0; x < CHUNK_GRID_SIZE; ++x)
		{
			for (uint32_t y = 0; y < CHUNK_GRID_SIZE; ++y)
			{
				uint32_t uChunkIndex = ChunkCoordsToIndex(x, y);

				// Load chunk to get AABB (we need actual vertex positions)
				std::string strChunkName = "Terrain_AABB_" + std::to_string(x) + "_" + std::to_string(y);
				std::string strChunkPath = std::string(ASSETS_ROOT"Terrain/Render_") + std::to_string(x) + "_" + std::to_string(y) + ".zmsh";

				Zenith_AssetHandler::AddMesh(strChunkName, strChunkPath.c_str(), 1 << Flux_MeshGeometry::FLUX_VERTEX_ATTRIBUTE__POSITION);
				Flux_MeshGeometry& xChunkMesh = Zenith_AssetHandler::GetMesh(strChunkName);

				// Generate and cache AABB
				s_axChunkAABBs[uChunkIndex] = Zenith_FrustumCulling::GenerateAABBFromVertices(
					xChunkMesh.m_pxPositions,
					xChunkMesh.m_uNumVerts
				);

				Zenith_AssetHandler::DeleteMesh(strChunkName);
			}
		}
		s_bAABBsCached = true;
	}

	// Build chunk data using cached AABBs and current LOD allocations
	for (uint32_t x = 0; x < CHUNK_GRID_SIZE; ++x)
	{
		for (uint32_t y = 0; y < CHUNK_GRID_SIZE; ++y)
		{
			uint32_t uChunkIndex = ChunkCoordsToIndex(x, y);
			const Flux_TerrainChunkResidency& xResidency = s_axChunkResidency[uChunkIndex];
			Zenith_TerrainChunkData& xChunkData = pxChunkDataOut[uChunkIndex];

			// Use cached AABB
			const Zenith_AABB& xChunkAABB = s_axChunkAABBs[uChunkIndex];
			xChunkData.m_xAABBMin = Zenith_Maths::Vector4(xChunkAABB.m_xMin, 0.0f);
			xChunkData.m_xAABBMax = Zenith_Maths::Vector4(xChunkAABB.m_xMax, 0.0f);

			// Fill in LOD data with current allocations
			for (uint32_t uLOD = 0; uLOD < LOD_COUNT; ++uLOD)
			{
				// Use unified LOD thresholds from Flux_TerrainConfig.h
				xChunkData.m_axLODs[uLOD].m_fMaxDistance = LOD_DISTANCE_SQ[uLOD];

				if (xResidency.m_aeStates[uLOD] == Flux_TerrainLODResidencyState::RESIDENT)
				{
					// LOD is resident, use actual allocation
					const Flux_TerrainLODAllocation& xAlloc = xResidency.m_axAllocations[uLOD];
					xChunkData.m_axLODs[uLOD].m_uFirstIndex = xAlloc.m_uIndexOffset;
					xChunkData.m_axLODs[uLOD].m_uIndexCount = xAlloc.m_uIndexCount;

					// **CRITICAL:** LOD3 uses the combined buffer where indices are already rebased
					// (absolute within the buffer) during Flux_MeshGeometry::Combine.
					// LOD0-2 use individually uploaded chunks with relative indices (0-based).
					// Therefore:
					// - LOD3: vertexOffset = 0 (indices already point to correct vertices)
					// - LOD0-2: vertexOffset = absolute offset (indices are 0-based, need offset)
					if (uLOD == LOWEST_DETAIL_LOD)
					{
						xChunkData.m_axLODs[uLOD].m_uVertexOffset = 0;
					}
					else
					{
						xChunkData.m_axLODs[uLOD].m_uVertexOffset = xAlloc.m_uVertexOffset;
					}

					// ========== DEBUG: Verify chunk data for tracked chunk ==========
					if (dbg_bLogTerrainVertexData && x == DBG_TRACKED_CHUNK_X && y == DBG_TRACKED_CHUNK_Y && uLOD == DBG_TRACKED_LOD)
					{
						Zenith_Log("=== CHUNK DATA FOR GPU: Chunk (%u,%u) LOD%u ===", x, y, uLOD);
						Zenith_Log("  Chunk data written to GPU buffer:");
						Zenith_Log("    m_uFirstIndex = %u (absolute offset in index buffer)", xAlloc.m_uIndexOffset);
						Zenith_Log("    m_uIndexCount = %u", xAlloc.m_uIndexCount);
						Zenith_Log("    m_uVertexOffset = %u (0 for LOD3-combined, absolute for LOD0-2-streamed)", xChunkData.m_axLODs[uLOD].m_uVertexOffset);
						Zenith_Log("  This data will be used by compute shader to generate draw commands");
						Zenith_Log("  Draw command will be: DrawIndexed(indexCount=%u, firstIndex=%u, vertexOffset=%u)",
							xAlloc.m_uIndexCount, xAlloc.m_uIndexOffset, xChunkData.m_axLODs[uLOD].m_uVertexOffset);
					}
				}
				else
				{
					// LOD not resident, fall back to LOD3
					const Flux_TerrainLODAllocation& xLOD3Alloc = xResidency.m_axAllocations[LOWEST_DETAIL_LOD];
					xChunkData.m_axLODs[uLOD].m_uFirstIndex = xLOD3Alloc.m_uIndexOffset;
					xChunkData.m_axLODs[uLOD].m_uIndexCount = xLOD3Alloc.m_uIndexCount;
					// LOD3 uses combined buffer with rebased indices, so vertexOffset = 0
					xChunkData.m_axLODs[uLOD].m_uVertexOffset = 0;
				}
			}
		}
	}

	// One-time debug log to verify chunk data is correct
	if (!s_bLoggedOnce)
	{
		s_bLoggedOnce = true;
		Zenith_Log("=== BuildChunkDataForGPU DEBUG - Sample Chunks ===");
		Zenith_Log("sizeof(Zenith_TerrainChunkData) = %zu bytes", sizeof(Zenith_TerrainChunkData));
		Zenith_Log("sizeof(Zenith_TerrainLODData) = %zu bytes", sizeof(Zenith_TerrainLODData));

		// Count residency states
		uint32_t uLOD0Resident = 0, uLOD1Resident = 0, uLOD2Resident = 0, uLOD3Resident = 0;
		for (uint32_t i = 0; i < TOTAL_CHUNKS; ++i)
		{
			const Flux_TerrainChunkResidency& xRes = s_axChunkResidency[i];
			if (xRes.m_aeStates[0] == Flux_TerrainLODResidencyState::RESIDENT) uLOD0Resident++;
			if (xRes.m_aeStates[1] == Flux_TerrainLODResidencyState::RESIDENT) uLOD1Resident++;
			if (xRes.m_aeStates[2] == Flux_TerrainLODResidencyState::RESIDENT) uLOD2Resident++;
			if (xRes.m_aeStates[3] == Flux_TerrainLODResidencyState::RESIDENT) uLOD3Resident++;
		}
	}
}
