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

// Singleton instance
Flux_TerrainStreamingManager* Flux_TerrainStreamingManager::s_pxInstance = nullptr;

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

	// Start with one large free block
	FreeBlock initialBlock;
	initialBlock.m_uOffset = 0;
	initialBlock.m_uSize = uTotalSize;
	m_xFreeBlocks.push(initialBlock);

	Zenith_Log("Flux_TerrainBufferAllocator[%s] initialized: %u total units", szDebugName, uTotalSize);
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
	Zenith_Assert(s_pxInstance == nullptr, "Flux_TerrainStreamingManager already initialized");

	s_pxInstance = new Flux_TerrainStreamingManager();

	Zenith_Log("==========================================================");
	Zenith_Log("Flux_TerrainStreamingManager::Initialize()");
	Zenith_Log("Initializing terrain LOD streaming system");
	Zenith_Log("  Config: %u×%u grid (%u total chunks), %u LOD levels", 
		CHUNK_GRID_SIZE, CHUNK_GRID_SIZE, TOTAL_CHUNKS, LOD_COUNT);
	Zenith_Log("  Chunk world size: %.1f units", CHUNK_WORLD_SIZE);
	Zenith_Log("==========================================================");

	// ========== Calculate LOD3 Buffer Sizes ==========
	// LOD3 is always resident for all chunks
	// Calculate exact size needed for LOD3 across all chunks

	const float fLOD3Density = 0.125f;  // From Zenith_TerrainComponent

	uint32_t uLOD3TotalVerts = 0;
	uint32_t uLOD3TotalIndices = 0;

	for (uint32_t z = 0; z < CHUNK_GRID_SIZE; ++z)
	{
		for (uint32_t x = 0; x < CHUNK_GRID_SIZE; ++x)
		{
			bool hasRightEdge = (x < CHUNK_GRID_SIZE - 1);
			bool hasTopEdge = (z < CHUNK_GRID_SIZE - 1);

			// Base vertices and indices per chunk (using CHUNK_SIZE_WORLD, NOT total terrain size!)
			// Each chunk is 64x64 units, LOD3 density 0.125 = 8 verts per side = 9x9 grid = 81 verts base
			uint32_t verts = (uint32_t)((CHUNK_SIZE_WORLD * fLOD3Density + 1) * (CHUNK_SIZE_WORLD * fLOD3Density + 1));
			uint32_t indices = (uint32_t)((CHUNK_SIZE_WORLD * fLOD3Density) * (CHUNK_SIZE_WORLD * fLOD3Density) * 6);

			// Edge stitching
			if (hasRightEdge)
			{
				verts += (uint32_t)(CHUNK_SIZE_WORLD * fLOD3Density);
				indices += (uint32_t)((CHUNK_SIZE_WORLD * fLOD3Density - 1) * 6);
			}
			if (hasTopEdge)
			{
				verts += (uint32_t)(CHUNK_SIZE_WORLD * fLOD3Density);
				indices += (uint32_t)((CHUNK_SIZE_WORLD * fLOD3Density - 1) * 6);
			}
			if (hasRightEdge && hasTopEdge)
			{
				verts += 1;
				indices += 6;
			}

			uLOD3TotalVerts += verts;
			uLOD3TotalIndices += indices;
		}
	}

	Zenith_Log("LOD3 (always-resident) buffer requirements:");
	Zenith_Log("  Vertices: %u (%.2f MB)", uLOD3TotalVerts, (uLOD3TotalVerts * TERRAIN_VERTEX_STRIDE) / (1024.0f * 1024.0f));
	Zenith_Log("  Indices: %u (%.2f MB)", uLOD3TotalIndices, (uLOD3TotalIndices * 4.0f) / (1024.0f * 1024.0f));

	// ========== Load and Combine LOD3 Chunks ==========

	Zenith_Log("Loading LOD3 meshes for all %u chunks...", TOTAL_CHUNKS);

	// Load first chunk to get buffer layout (load ALL attributes so stride is correct for streaming allocator)
	Zenith_AssetHandler::AddMesh("Terrain_LOD3_Streaming_0_0",
		ASSETS_ROOT"Terrain/Render_LOD3_0_0.zmsh",
		0);  // 0 = load all attributes to get correct stride for allocator sizing

	Flux_MeshGeometry& xLOD3Geometry = Zenith_AssetHandler::GetMesh("Terrain_LOD3_Streaming_0_0");

	// Pre-allocate for all LOD3 chunks
	const uint64_t ulLOD3VertexDataSize = static_cast<uint64_t>(uLOD3TotalVerts) * xLOD3Geometry.m_xBufferLayout.GetStride();
	const uint64_t ulLOD3IndexDataSize = static_cast<uint64_t>(uLOD3TotalIndices) * sizeof(Flux_MeshGeometry::IndexType);
	const uint64_t ulLOD3PositionDataSize = static_cast<uint64_t>(uLOD3TotalVerts) * sizeof(Zenith_Maths::Vector3);

	xLOD3Geometry.m_pVertexData = static_cast<uint8_t*>(Zenith_MemoryManagement::Reallocate(xLOD3Geometry.m_pVertexData, ulLOD3VertexDataSize));
	xLOD3Geometry.m_ulReservedVertexDataSize = ulLOD3VertexDataSize;

	xLOD3Geometry.m_puIndices = static_cast<Flux_MeshGeometry::IndexType*>(Zenith_MemoryManagement::Reallocate(xLOD3Geometry.m_puIndices, ulLOD3IndexDataSize));
	xLOD3Geometry.m_ulReservedIndexDataSize = ulLOD3IndexDataSize;

	if (xLOD3Geometry.m_pxPositions)
	{
		xLOD3Geometry.m_pxPositions = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Reallocate(xLOD3Geometry.m_pxPositions, ulLOD3PositionDataSize));
		xLOD3Geometry.m_ulReservedPositionDataSize = ulLOD3PositionDataSize;
	}

	// Combine all LOD3 chunks
	for (uint32_t x = 0; x < CHUNK_GRID_SIZE; ++x)
	{
		for (uint32_t y = 0; y < CHUNK_GRID_SIZE; ++y)
		{
			if (x == 0 && y == 0)
				continue;  // Already loaded

			std::string strChunkName = "Terrain_LOD3_Streaming_" + std::to_string(x) + "_" + std::to_string(y);
			std::string strChunkPath = std::string(ASSETS_ROOT"Terrain/Render_LOD3_") + std::to_string(x) + "_" + std::to_string(y) + ".zmsh";

			// Check if LOD3 file exists, fallback to LOD0 if not
			std::ifstream lodFile(strChunkPath);
			if (!lodFile.good())
			{
				Zenith_Log("WARNING: LOD3 not found for chunk (%u,%u), using LOD0 as fallback", x, y);
				strChunkPath = std::string(ASSETS_ROOT"Terrain/Render_") + std::to_string(x) + "_" + std::to_string(y) + ".zmsh";
			}

			Zenith_AssetHandler::AddMesh(strChunkName, strChunkPath.c_str(),
				0);  // 0 = load all attributes for rendering

			Flux_MeshGeometry& xChunkMesh = Zenith_AssetHandler::GetMesh(strChunkName);
			Flux_MeshGeometry::Combine(xLOD3Geometry, xChunkMesh);

			Zenith_AssetHandler::DeleteMesh(strChunkName);

			if ((x * CHUNK_GRID_SIZE + y) % 512 == 0)
			{
				Zenith_Log("  Combined LOD3 chunk (%u,%u)", x, y);
			}
		}
	}

	Zenith_Log("LOD3 mesh combination complete: %u vertices, %u indices",
		xLOD3Geometry.m_uNumVerts, xLOD3Geometry.m_uNumIndices);

	// Store LOD3 counts for allocator initialization
	s_pxInstance->m_uLOD3VertexCount = xLOD3Geometry.m_uNumVerts;
	s_pxInstance->m_uLOD3IndexCount = xLOD3Geometry.m_uNumIndices;

	// ========== Initialize Unified Buffers (LOD3 + Streaming Space) ==========

	const uint64_t ulLOD3VertexSize = xLOD3Geometry.GetVertexDataSize();
	const uint64_t ulLOD3IndexSize = xLOD3Geometry.GetIndexDataSize();
	const uint64_t ulUnifiedVertexSize = ulLOD3VertexSize + STREAMING_VERTEX_BUFFER_SIZE;
	const uint64_t ulUnifiedIndexSize = ulLOD3IndexSize + STREAMING_INDEX_BUFFER_SIZE;

	Zenith_Log("Initializing unified terrain buffers:");
	Zenith_Log("  Vertex buffer: %.2f MB LOD3 + %llu MB streaming = %.2f MB total",
		ulLOD3VertexSize / (1024.0f * 1024.0f), STREAMING_VERTEX_BUFFER_SIZE_MB,
		ulUnifiedVertexSize / (1024.0f * 1024.0f));
	Zenith_Log("  Index buffer: %.2f MB LOD3 + %llu MB streaming = %.2f MB total",
		ulLOD3IndexSize / (1024.0f * 1024.0f), STREAMING_INDEX_BUFFER_SIZE_MB,
		ulUnifiedIndexSize / (1024.0f * 1024.0f));

	// Allocate unified buffers with LOD3 data at the beginning
	uint8_t* pUnifiedVertexData = new uint8_t[ulUnifiedVertexSize];
	uint32_t* pUnifiedIndexData = new uint32_t[ulUnifiedIndexSize / sizeof(uint32_t)];

	// Copy LOD3 data to beginning
	memcpy(pUnifiedVertexData, xLOD3Geometry.m_pVertexData, ulLOD3VertexSize);
	memcpy(pUnifiedIndexData, xLOD3Geometry.m_puIndices, ulLOD3IndexSize);

	// Zero out streaming region
	memset(pUnifiedVertexData + ulLOD3VertexSize, 0, STREAMING_VERTEX_BUFFER_SIZE);
	memset(pUnifiedIndexData + (ulLOD3IndexSize / sizeof(uint32_t)), 0, STREAMING_INDEX_BUFFER_SIZE);

	// Upload unified buffers to GPU
	Flux_MemoryManager::InitialiseVertexBuffer(pUnifiedVertexData, ulUnifiedVertexSize, s_pxInstance->m_xUnifiedVertexBuffer);
	Flux_MemoryManager::InitialiseIndexBuffer(pUnifiedIndexData, ulUnifiedIndexSize, s_pxInstance->m_xUnifiedIndexBuffer);

	// Store buffer sizes for bounds checking
	s_pxInstance->m_ulUnifiedVertexBufferSize = ulUnifiedVertexSize;
	s_pxInstance->m_ulUnifiedIndexBufferSize = ulUnifiedIndexSize;
	s_pxInstance->m_uVertexStride = xLOD3Geometry.m_xBufferLayout.GetStride();

	delete[] pUnifiedVertexData;
	delete[] pUnifiedIndexData;

	Zenith_Log("Unified terrain buffers uploaded to GPU");
	Zenith_Log("  LOD3 region: vertices [0, %u), indices [0, %u)", s_pxInstance->m_uLOD3VertexCount, s_pxInstance->m_uLOD3IndexCount);
	Zenith_Log("  Streaming region starts at: vertex %u, index %u", s_pxInstance->m_uLOD3VertexCount, s_pxInstance->m_uLOD3IndexCount);
	Zenith_Log("  Vertex stride: %u bytes", s_pxInstance->m_uVertexStride);
	Zenith_Log("  Total vertex buffer size: %llu bytes", ulUnifiedVertexSize);
	Zenith_Log("  Total index buffer size: %llu bytes", ulUnifiedIndexSize);

	// ========== Initialize Allocators ==========

	// Calculate allocator sizes for the streaming region (after LOD3)
	// Allocators manage the streaming region only; offsets are relative to the start of streaming region
	// When uploading, we add LOD3VertexCount/LOD3IndexCount to get absolute buffer offsets
	const uint32_t uVertexStride = xLOD3Geometry.m_xBufferLayout.GetStride();
	const uint32_t uMaxStreamingVertices = static_cast<uint32_t>(STREAMING_VERTEX_BUFFER_SIZE / uVertexStride);
	const uint32_t uMaxStreamingIndices = static_cast<uint32_t>(STREAMING_INDEX_BUFFER_SIZE / sizeof(uint32_t));

	s_pxInstance->m_xVertexAllocator.Initialize(uMaxStreamingVertices, "StreamingVertices");
	s_pxInstance->m_xIndexAllocator.Initialize(uMaxStreamingIndices, "StreamingIndices");

	Zenith_Log("Allocators initialized:");
	Zenith_Log("  Streaming vertex capacity: %u vertices", uMaxStreamingVertices);
	Zenith_Log("  Streaming index capacity: %u indices", uMaxStreamingIndices);

	// ========== Initialize Chunk Residency State ==========

	// Initialize all chunks: LOD3 is RESIDENT (in LOD3 buffer), LOD0-2 are NOT_LOADED
	uint32_t uCurrentLOD3IndexOffset = 0;
	uint32_t uCurrentLOD3VertexOffset = 0;

	for (uint32_t x = 0; x < CHUNK_GRID_SIZE; ++x)
	{
		for (uint32_t y = 0; y < CHUNK_GRID_SIZE; ++y)
		{
			uint32_t uChunkIndex = s_pxInstance->ChunkCoordsToIndex(x, y);
			Flux_TerrainChunkResidency& xResidency = s_pxInstance->m_axChunkResidency[uChunkIndex];

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
				xResidency.m_afPriorities[uLOD] = FLT_MAX;
			}

			xResidency.m_auLastRequestedFrame[LOWEST_DETAIL_LOD] = 0;
			xResidency.m_afPriorities[LOWEST_DETAIL_LOD] = FLT_MAX;
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
			uint32_t uChunkIndex = s_pxInstance->ChunkCoordsToIndex(x, y);
			
			std::string strChunkName = "Terrain_AABB_Init_" + std::to_string(x) + "_" + std::to_string(y);
			std::string strChunkPath = std::string(ASSETS_ROOT"Terrain/Render_") + std::to_string(x) + "_" + std::to_string(y) + ".zmsh";
			
			Zenith_AssetHandler::AddMesh(strChunkName, strChunkPath.c_str(), 1 << Flux_MeshGeometry::FLUX_VERTEX_ATTRIBUTE__POSITION);
			Flux_MeshGeometry& xChunkMesh = Zenith_AssetHandler::GetMesh(strChunkName);
			
			// Generate and cache AABB
			s_pxInstance->m_axChunkAABBs[uChunkIndex] = Zenith_FrustumCulling::GenerateAABBFromVertices(
				xChunkMesh.m_pxPositions,
				xChunkMesh.m_uNumVerts
			);
			
			Zenith_AssetHandler::DeleteMesh(strChunkName);
		}
	}
	s_pxInstance->m_bAABBsCached = true;
	
	Zenith_Log("Chunk AABBs cached for %u chunks", TOTAL_CHUNKS);

	// ========== Initialize Stats ==========

	s_pxInstance->m_xStats.m_uLOD3ChunksResident = TOTAL_CHUNKS;
	s_pxInstance->m_xStats.m_uHighLODChunksResident = 0;
	s_pxInstance->m_xStats.m_uStreamingRequestsThisFrame = 0;
	s_pxInstance->m_xStats.m_uEvictionsThisFrame = 0;
	s_pxInstance->m_xStats.m_uVertexBufferUsedMB = 0;
	s_pxInstance->m_xStats.m_uVertexBufferTotalMB = STREAMING_VERTEX_BUFFER_SIZE_MB;
	s_pxInstance->m_xStats.m_uIndexBufferUsedMB = 0;
	s_pxInstance->m_xStats.m_uIndexBufferTotalMB = STREAMING_INDEX_BUFFER_SIZE_MB;

	// ========== Initialize Performance Optimization State ==========
	// Initialize all chunks to LOD3 (lowest detail, always resident)
	memset(s_pxInstance->m_auDesiredLOD, LOWEST_DETAIL_LOD, sizeof(s_pxInstance->m_auDesiredLOD));
	
	// Pre-allocate chunk data buffer to avoid per-frame allocations
	s_pxInstance->m_pxCachedChunkData = new Zenith_TerrainChunkData[TOTAL_CHUNKS];
	
	// Reserve capacity for active chunk set (max ~1024 chunks in view)
	s_pxInstance->m_xActiveChunkIndices.reserve(1024);

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Render", "Terrain", "Log Streaming" }, dbg_bLogTerrainStreaming);
	Zenith_DebugVariables::AddBoolean({ "Render", "Terrain", "Log Evictions" }, dbg_bLogTerrainEvictions);
	Zenith_DebugVariables::AddBoolean({ "Render", "Terrain", "Log Allocations" }, dbg_bLogTerrainAllocations);
	Zenith_DebugVariables::AddBoolean({ "Render", "Terrain", "Log State Changes" }, dbg_bLogTerrainStateChanges);
#endif

	Zenith_Log("==========================================================");
	Zenith_Log("Flux_TerrainStreamingManager initialization complete");
	Zenith_Log("  LOD3 always-resident: %u chunks", TOTAL_CHUNKS);
	Zenith_Log("  Streaming budget: %llu MB vertices, %llu MB indices",
		STREAMING_VERTEX_BUFFER_SIZE_MB, STREAMING_INDEX_BUFFER_SIZE_MB);
	Zenith_Log("  LOD thresholds (dist²): %.0f, %.0f, %.0f, %.0f",
		LOD_DISTANCE_SQ[0], LOD_DISTANCE_SQ[1], LOD_DISTANCE_SQ[2], LOD_DISTANCE_SQ[3]);
	Zenith_Log("==========================================================");
}

void Flux_TerrainStreamingManager::Shutdown()
{
	if (s_pxInstance)
	{
		Zenith_Log("Flux_TerrainStreamingManager::Shutdown()");
		
		// Clean up pre-allocated chunk data buffer
		if (s_pxInstance->m_pxCachedChunkData)
		{
			delete[] s_pxInstance->m_pxCachedChunkData;
			s_pxInstance->m_pxCachedChunkData = nullptr;
		}
		
		delete s_pxInstance;
		s_pxInstance = nullptr;
	}
}

void Flux_TerrainStreamingManager::UpdateStreaming(const Zenith_Maths::Vector3& xCameraPos)
{
	m_uCurrentFrame++;

	// Reset per-frame stats
	m_xStats.m_uStreamingRequestsThisFrame = 0;
	m_xStats.m_uEvictionsThisFrame = 0;

	// ========== Check if camera moved significantly ==========
	const float fCameraMoveSq = glm::distance2(xCameraPos, m_xLastCameraPos);
	const bool bCameraMovedSignificantly = (fCameraMoveSq > CAMERA_MOVE_THRESHOLD_SQ);
	
	// Get current camera chunk position
	int32_t iCameraChunkX, iCameraChunkY;
	WorldPosToChunkCoords(xCameraPos, iCameraChunkX, iCameraChunkY);
	
	// ========== Rebuild active chunk set only when camera changes chunks ==========
	if (iCameraChunkX != m_iLastCameraChunkX || iCameraChunkY != m_iLastCameraChunkY)
	{
		RebuildActiveChunkSet(iCameraChunkX, iCameraChunkY);
		m_iLastCameraChunkX = iCameraChunkX;
		m_iLastCameraChunkY = iCameraChunkY;
		m_bChunkDataDirty = true;
		
		if (dbg_bLogTerrainStreaming)
		{
			Zenith_Log("[Terrain] Camera moved to chunk (%d,%d), active set: %zu chunks",
				iCameraChunkX, iCameraChunkY, m_xActiveChunkIndices.size());
		}
	}

	// ========== Process streaming on interval or significant camera move ==========
	const bool bProcessStreaming = bCameraMovedSignificantly || 
		(m_uCurrentFrame % STREAMING_UPDATE_INTERVAL == 0);
	
	if (bProcessStreaming)
	{
		m_xLastCameraPos = xCameraPos;
		
		// ========== CPU-side LOD Selection and Streaming Requests ==========
		// Use thresholds from Flux_TerrainConfig.h (single source of truth)
		
		for (uint32_t uActiveIdx = 0; uActiveIdx < m_xActiveChunkIndices.size(); ++uActiveIdx)
		{
			const uint32_t uChunkIndex = m_xActiveChunkIndices[uActiveIdx];
			uint32_t x, y;
			ChunkIndexToCoords(uChunkIndex, x, y);

			// Calculate distance from camera to chunk center
			Zenith_Maths::Vector3 xChunkCenter = GetChunkCenter(x, y);
			float fDistanceSq = glm::distance2(xCameraPos, xChunkCenter);

			// Determine desired LOD with hysteresis to prevent thrashing
			uint8_t uCurrentLOD = m_auDesiredLOD[uChunkIndex];
			uint8_t uNewLOD = static_cast<uint8_t>(LOD_ALWAYS_RESIDENT);
			
			for (uint32_t iLOD = 0; iLOD < LOD_ALWAYS_RESIDENT; ++iLOD)
			{
				float fThreshold = LOD_MAX_DISTANCE_SQ[iLOD];
				// Apply hysteresis when downgrading (moving to lower detail)
				if (uCurrentLOD <= iLOD)
				{
					fThreshold *= LOD_HYSTERESIS_FACTOR;
				}
				
				if (fDistanceSq < fThreshold)
				{
					uNewLOD = static_cast<uint8_t>(iLOD);
					break;
				}
			}
			
			// Update cached LOD if changed
			if (uNewLOD != uCurrentLOD)
			{
				m_auDesiredLOD[uChunkIndex] = uNewLOD;
				m_bChunkDataDirty = true;
				
				if (dbg_bLogTerrainStateChanges)
				{
					Zenith_Log("[Terrain] Chunk (%u,%u) LOD desire: %s -> %s (dist=%.0fm)",
						x, y, GetLODName(uCurrentLOD), GetLODName(uNewLOD), sqrtf(fDistanceSq));
				}
			}

			// Request streaming for non-always-resident LODs
			if (uNewLOD < LOD_ALWAYS_RESIDENT)
			{
				RequestLOD(x, y, uNewLOD, fDistanceSq);
			}
		}
	}

	// Always process streaming queue to complete pending uploads
	ProcessStreamingQueue();

	// ========== Update stats periodically ==========
	if (m_uCurrentFrame % 30 == 0)
	{
		uint32_t uHighLODResident = 0;
		for (uint32_t i = 0; i < TOTAL_CHUNKS; ++i)
		{
			for (uint32_t uLOD = 0; uLOD < LOD_ALWAYS_RESIDENT; ++uLOD)
			{
				if (m_axChunkResidency[i].m_aeStates[uLOD] == Flux_TerrainLODResidencyState::RESIDENT)
				{
					uHighLODResident++;
				}
			}
		}
		m_xStats.m_uHighLODChunksResident = uHighLODResident;

		const uint32_t uVertexBytesUsed = (m_xVertexAllocator.GetTotalSpace() - m_xVertexAllocator.GetUnusedSpace()) * VERTEX_STRIDE_BYTES;
		const uint32_t uIndexBytesUsed = (m_xIndexAllocator.GetTotalSpace() - m_xIndexAllocator.GetUnusedSpace()) * 4;

		m_xStats.m_uVertexBufferUsedMB = uVertexBytesUsed / (1024 * 1024);
		m_xStats.m_uIndexBufferUsedMB = uIndexBytesUsed / (1024 * 1024);
		m_xStats.m_uVertexFragments = m_xVertexAllocator.GetFragmentationCount();
		m_xStats.m_uIndexFragments = m_xIndexAllocator.GetFragmentationCount();

		if (m_uCurrentFrame % 60 == 0 && dbg_bLogTerrainStreaming)
		{
			LogStats();
		}
	}
}

bool Flux_TerrainStreamingManager::RequestLOD(uint32_t uChunkX, uint32_t uChunkY, uint32_t uLODLevel, float fPriority)
{
	Zenith_Assert(uChunkX < CHUNK_GRID_SIZE && uChunkY < CHUNK_GRID_SIZE, "Invalid chunk coordinates");
	Zenith_Assert(uLODLevel < LOD_COUNT, "Invalid LOD level");

	uint32_t uChunkIndex = ChunkCoordsToIndex(uChunkX, uChunkY);
	Flux_TerrainChunkResidency& xResidency = m_axChunkResidency[uChunkIndex];

	// Update last requested frame and priority
	xResidency.m_auLastRequestedFrame[uLODLevel] = m_uCurrentFrame;
	xResidency.m_afPriorities[uLODLevel] = fPriority;

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
		if (m_xStreamingQueue.size() >= MAX_QUEUE_SIZE)
		{
			return false;  // Queue full, retry next frame
		}

		// Queue for streaming
		StreamingRequest request;
		request.m_uChunkIndex = uChunkIndex;
		request.m_uLODLevel = uLODLevel;
		request.m_fPriority = fPriority;
		m_xStreamingQueue.push(request);

		xResidency.m_aeStates[uLODLevel] = Flux_TerrainLODResidencyState::QUEUED;
		m_xStats.m_uStreamingRequestsThisFrame++;

		if (dbg_bLogTerrainStateChanges)
		{
			Zenith_Log("[Terrain] Chunk (%u,%u) %s: NOT_LOADED -> QUEUED (priority=%.0f)",
				uChunkX, uChunkY, GetLODName(uLODLevel), fPriority);
		}

		return false;
	}

	return false;
}

bool Flux_TerrainStreamingManager::GetLODAllocation(uint32_t uChunkX, uint32_t uChunkY, uint32_t uLODLevel, Flux_TerrainLODAllocation& xAllocOut) const
{
	Zenith_Assert(uChunkX < CHUNK_GRID_SIZE && uChunkY < CHUNK_GRID_SIZE, "Invalid chunk coordinates");
	Zenith_Assert(uLODLevel < LOD_COUNT, "Invalid LOD level");

	uint32_t uChunkIndex = ChunkCoordsToIndex(uChunkX, uChunkY);
	const Flux_TerrainChunkResidency& xResidency = m_axChunkResidency[uChunkIndex];

	if (xResidency.m_aeStates[uLODLevel] == Flux_TerrainLODResidencyState::RESIDENT)
	{
		xAllocOut = xResidency.m_axAllocations[uLODLevel];
		return true;
	}

	return false;
}

Flux_TerrainLODResidencyState Flux_TerrainStreamingManager::GetResidencyState(uint32_t uChunkX, uint32_t uChunkY, uint32_t uLODLevel) const
{
	Zenith_Assert(uChunkX < CHUNK_GRID_SIZE && uChunkY < CHUNK_GRID_SIZE, "Invalid chunk coordinates");
	Zenith_Assert(uLODLevel < LOD_COUNT, "Invalid LOD level");

	uint32_t uChunkIndex = ChunkCoordsToIndex(uChunkX, uChunkY);
	return m_axChunkResidency[uChunkIndex].m_aeStates[uLODLevel];
}

void Flux_TerrainStreamingManager::LogStats() const
{
	Zenith_Log("=== Terrain Streaming Stats (Frame %u) ===", m_uCurrentFrame);
	Zenith_Log("  LOD3 resident: %u chunks (always)", m_xStats.m_uLOD3ChunksResident);
	Zenith_Log("  High LOD (0-2) resident: %u chunks", m_xStats.m_uHighLODChunksResident);
	Zenith_Log("  Streaming requests this frame: %u", m_xStats.m_uStreamingRequestsThisFrame);
	Zenith_Log("  Evictions this frame: %u", m_xStats.m_uEvictionsThisFrame);
	Zenith_Log("  Vertex buffer: %u / %u MB (%.1f%%)",
		m_xStats.m_uVertexBufferUsedMB, m_xStats.m_uVertexBufferTotalMB,
		(m_xStats.m_uVertexBufferUsedMB * 100.0f) / m_xStats.m_uVertexBufferTotalMB);
	Zenith_Log("  Index buffer: %u / %u MB (%.1f%%)",
		m_xStats.m_uIndexBufferUsedMB, m_xStats.m_uIndexBufferTotalMB,
		(m_xStats.m_uIndexBufferUsedMB * 100.0f) / m_xStats.m_uIndexBufferTotalMB);
	Zenith_Log("  Vertex fragments: %u, Index fragments: %u",
		m_xStats.m_uVertexFragments, m_xStats.m_uIndexFragments);
}

void Flux_TerrainStreamingManager::ProcessStreamingQueue()
{
	uint32_t uUploadsThisFrame = 0;

	while (!m_xStreamingQueue.empty() && uUploadsThisFrame < MAX_UPLOADS_PER_FRAME)
	{
		StreamingRequest request = m_xStreamingQueue.top();
		m_xStreamingQueue.pop();

		uint32_t uChunkIndex = request.m_uChunkIndex;
		uint32_t uLODLevel = request.m_uLODLevel;

		Flux_TerrainChunkResidency& xResidency = m_axChunkResidency[uChunkIndex];
		
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
		uint32_t uVertexOffset = m_xVertexAllocator.Allocate(xChunkMesh.m_uNumVerts);
		uint32_t uIndexOffset = m_xIndexAllocator.Allocate(xChunkMesh.m_uNumIndices);

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
				m_xVertexAllocator.Free(uVertexOffset, xChunkMesh.m_uNumVerts);
			if (uIndexOffset != UINT32_MAX)
				m_xIndexAllocator.Free(uIndexOffset, xChunkMesh.m_uNumIndices);

			bool bEvictionSuccess = EvictToMakeSpace(xChunkMesh.m_uNumVerts, xChunkMesh.m_uNumIndices);
			if (!bEvictionSuccess)
			{
				xResidency.m_aeStates[uLODLevel] = Flux_TerrainLODResidencyState::NOT_LOADED;
				Zenith_AssetHandler::DeleteMesh(strChunkName);
				continue;
			}

			// Retry allocation after eviction
			uVertexOffset = m_xVertexAllocator.Allocate(xChunkMesh.m_uNumVerts);
			uIndexOffset = m_xIndexAllocator.Allocate(xChunkMesh.m_uNumIndices);

			if (uVertexOffset == UINT32_MAX || uIndexOffset == UINT32_MAX)
			{
				if (uVertexOffset != UINT32_MAX)
					m_xVertexAllocator.Free(uVertexOffset, xChunkMesh.m_uNumVerts);
				if (uIndexOffset != UINT32_MAX)
					m_xIndexAllocator.Free(uIndexOffset, xChunkMesh.m_uNumIndices);

				xResidency.m_aeStates[uLODLevel] = Flux_TerrainLODResidencyState::NOT_LOADED;
				Zenith_AssetHandler::DeleteMesh(strChunkName);
				continue;
			}
		}

		// Validate allocation bounds
		const uint32_t uMaxStreamingVertices = m_xVertexAllocator.GetTotalSpace();
		const uint32_t uMaxStreamingIndices = m_xIndexAllocator.GetTotalSpace();
		
		if (uVertexOffset + xChunkMesh.m_uNumVerts > uMaxStreamingVertices ||
			uIndexOffset + xChunkMesh.m_uNumIndices > uMaxStreamingIndices)
		{
			Zenith_Log("[Terrain] ERROR: Allocation out of bounds for chunk (%u,%u) %s",
				uChunkX, uChunkY, GetLODName(uLODLevel));
			m_xVertexAllocator.Free(uVertexOffset, xChunkMesh.m_uNumVerts);
			m_xIndexAllocator.Free(uIndexOffset, xChunkMesh.m_uNumIndices);
			xResidency.m_aeStates[uLODLevel] = Flux_TerrainLODResidencyState::NOT_LOADED;
			Zenith_AssetHandler::DeleteMesh(strChunkName);
			continue;
		}

		// Stream in the LOD mesh data
		bool bStreamSuccess = StreamInLOD(uChunkIndex, uLODLevel, uVertexOffset, uIndexOffset);
		if (!bStreamSuccess)
		{
			m_xVertexAllocator.Free(uVertexOffset, xChunkMesh.m_uNumVerts);
			m_xIndexAllocator.Free(uIndexOffset, xChunkMesh.m_uNumIndices);
			xResidency.m_aeStates[uLODLevel] = Flux_TerrainLODResidencyState::NOT_LOADED;
			Zenith_AssetHandler::DeleteMesh(strChunkName);
			continue;
		}

		Zenith_AssetHandler::DeleteMesh(strChunkName);
		uUploadsThisFrame++;
		m_bChunkDataDirty = true;

		if (dbg_bLogTerrainStateChanges)
		{
			Zenith_Log("[Terrain] Chunk (%u,%u) %s: LOADING -> RESIDENT (%u verts, %u indices)",
				uChunkX, uChunkY, GetLODName(uLODLevel),
				xResidency.m_axAllocations[uLODLevel].m_uVertexCount,
				xResidency.m_axAllocations[uLODLevel].m_uIndexCount);
		}
	}
}

bool Flux_TerrainStreamingManager::EvictToMakeSpace(uint32_t uVertexSpaceNeeded, uint32_t uIndexSpaceNeeded)
{
	// Build eviction candidate list (all resident high LODs sorted by priority)
	std::vector<EvictionCandidate> candidates;
	for (uint32_t i = 0; i < TOTAL_CHUNKS; ++i)
	{
		Flux_TerrainChunkResidency& xResidency = m_axChunkResidency[i];
		for (uint32_t uLOD = 0; uLOD < LOD_ALWAYS_RESIDENT; ++uLOD)
		{
			if (xResidency.m_aeStates[uLOD] == Flux_TerrainLODResidencyState::RESIDENT)
			{
				EvictionCandidate candidate;
				candidate.m_uChunkIndex = i;
				candidate.m_uLODLevel = uLOD;

				// Higher value = more likely to evict (older and farther = evict first)
				uint32_t uFramesSinceRequested = m_uCurrentFrame - xResidency.m_auLastRequestedFrame[uLOD];
				candidate.m_fPriority = static_cast<float>(uFramesSinceRequested) + xResidency.m_afPriorities[uLOD];

				candidates.push_back(candidate);
			}
		}
	}

	if (candidates.empty())
	{
		return false;
	}

	// Sort by priority (highest = most likely to evict)
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

		Flux_TerrainChunkResidency& xResidency = m_axChunkResidency[candidate.m_uChunkIndex];
		Flux_TerrainLODAllocation& xAlloc = xResidency.m_axAllocations[candidate.m_uLODLevel];

		uVertexSpaceFreed += xAlloc.m_uVertexCount;
		uIndexSpaceFreed += xAlloc.m_uIndexCount;

		EvictLOD(candidate.m_uChunkIndex, candidate.m_uLODLevel);
		uEvictionsThisCall++;
		m_xStats.m_uEvictionsThisFrame++;
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
	Zenith_Assert(uLODLevel < LOD_ALWAYS_RESIDENT, "StreamInLOD only for LOD0-2");

	uint32_t uChunkX, uChunkY;
	ChunkIndexToCoords(uChunkIndex, uChunkX, uChunkY);

	Flux_TerrainChunkResidency& xResidency = m_axChunkResidency[uChunkIndex];
	xResidency.m_aeStates[uLODLevel] = Flux_TerrainLODResidencyState::LOADING;

	// Load mesh with all vertex attributes
	const char* LOD_SUFFIXES[3] = { "", "_LOD1", "_LOD2" };
	std::string strChunkName = "Terrain_Upload_LOD" + std::to_string(uLODLevel) + "_" + std::to_string(uChunkX) + "_" + std::to_string(uChunkY);
	std::string strChunkPath = std::string(ASSETS_ROOT"Terrain/Render") + LOD_SUFFIXES[uLODLevel] + "_" + std::to_string(uChunkX) + "_" + std::to_string(uChunkY) + ".zmsh";

	Zenith_AssetHandler::AddMesh(strChunkName, strChunkPath.c_str(), 0);  // 0 = all attributes
	Flux_MeshGeometry& xChunkMesh = Zenith_AssetHandler::GetMesh(strChunkName);

	// Calculate absolute offsets in unified buffer
	const uint32_t uAbsoluteVertexOffset = m_uLOD3VertexCount + uVertexOffset;
	const uint32_t uAbsoluteIndexOffset = m_uLOD3IndexCount + uIndexOffset;

	// Calculate byte offsets and sizes
	const uint32_t uVertexStride = xChunkMesh.m_xBufferLayout.GetStride();
	const uint64_t ulVertexDataSize = static_cast<uint64_t>(xChunkMesh.m_uNumVerts) * uVertexStride;
	const uint64_t ulVertexOffsetBytes = static_cast<uint64_t>(uAbsoluteVertexOffset) * uVertexStride;
	const uint64_t ulIndexDataSize = static_cast<uint64_t>(xChunkMesh.m_uNumIndices) * sizeof(uint32_t);
	const uint64_t ulIndexOffsetBytes = static_cast<uint64_t>(uAbsoluteIndexOffset) * sizeof(uint32_t);

	// Bounds check
	if (ulVertexOffsetBytes + ulVertexDataSize > m_ulUnifiedVertexBufferSize)
	{
		Zenith_Log("[Terrain] ERROR: Vertex upload exceeds buffer for chunk (%u,%u) %s",
			uChunkX, uChunkY, GetLODName(uLODLevel));
		Zenith_AssetHandler::DeleteMesh(strChunkName);
		return false;
	}

	if (ulIndexOffsetBytes + ulIndexDataSize > m_ulUnifiedIndexBufferSize)
	{
		Zenith_Log("[Terrain] ERROR: Index upload exceeds buffer for chunk (%u,%u) %s",
			uChunkX, uChunkY, GetLODName(uLODLevel));
		Zenith_AssetHandler::DeleteMesh(strChunkName);
		return false;
	}

	// Upload vertex and index data (synchronous)
	Flux_MemoryManager::UploadBufferDataAtOffset(
		m_xUnifiedVertexBuffer.GetBuffer().m_xVRAMHandle,
		xChunkMesh.m_pVertexData,
		ulVertexDataSize,
		ulVertexOffsetBytes
	);

	Flux_MemoryManager::UploadBufferDataAtOffset(
		m_xUnifiedIndexBuffer.GetBuffer().m_xVRAMHandle,
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
	Zenith_Assert(uLODLevel < LOD_ALWAYS_RESIDENT, "EvictLOD only for LOD0-2");

	uint32_t uChunkX, uChunkY;
	ChunkIndexToCoords(uChunkIndex, uChunkX, uChunkY);

	Flux_TerrainChunkResidency& xResidency = m_axChunkResidency[uChunkIndex];

	if (xResidency.m_aeStates[uLODLevel] != Flux_TerrainLODResidencyState::RESIDENT)
	{
		return;  // Not resident
	}

	// Transition: RESIDENT -> EVICTING -> NOT_LOADED
	xResidency.m_aeStates[uLODLevel] = Flux_TerrainLODResidencyState::EVICTING;

	// Free allocations (convert absolute to relative offsets)
	Flux_TerrainLODAllocation& xAlloc = xResidency.m_axAllocations[uLODLevel];
	const uint32_t uRelativeVertexOffset = xAlloc.m_uVertexOffset - m_uLOD3VertexCount;
	const uint32_t uRelativeIndexOffset = xAlloc.m_uIndexOffset - m_uLOD3IndexCount;
	m_xVertexAllocator.Free(uRelativeVertexOffset, xAlloc.m_uVertexCount);
	m_xIndexAllocator.Free(uRelativeIndexOffset, xAlloc.m_uIndexCount);

	xResidency.m_aeStates[uLODLevel] = Flux_TerrainLODResidencyState::NOT_LOADED;
	m_bChunkDataDirty = true;

	if (dbg_bLogTerrainStateChanges)
	{
		Zenith_Log("[Terrain] Chunk (%u,%u) %s: RESIDENT -> NOT_LOADED (freed %u verts, %u indices)",
			uChunkX, uChunkY, GetLODName(uLODLevel), xAlloc.m_uVertexCount, xAlloc.m_uIndexCount);
	}
}

std::vector<Flux_TerrainStreamingManager::EvictionCandidate> Flux_TerrainStreamingManager::BuildEvictionCandidates(const Zenith_Maths::Vector3& xCameraPos) const
{
	std::vector<EvictionCandidate> candidates;

	for (uint32_t i = 0; i < TOTAL_CHUNKS; ++i)
	{
		const Flux_TerrainChunkResidency& xResidency = m_axChunkResidency[i];
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

Zenith_Maths::Vector3 Flux_TerrainStreamingManager::GetChunkCenter(uint32_t uChunkX, uint32_t uChunkY) const
{
	uint32_t uChunkIndex = ChunkCoordsToIndex(uChunkX, uChunkY);
	
	// Use cached AABB if available (accurate world positions from actual mesh data)
	if (m_bAABBsCached)
	{
		const Zenith_AABB& xAABB = m_axChunkAABBs[uChunkIndex];
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

void Flux_TerrainStreamingManager::WorldPosToChunkCoords(const Zenith_Maths::Vector3& xWorldPos, int32_t& iChunkX, int32_t& iChunkY) const
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
	
	m_xActiveChunkIndices.clear();
	
	// Reserve approximate capacity to avoid reallocations
	const int32_t iDiameter = static_cast<int32_t>(m_uActiveChunkRadius * 2 + 1);
	m_xActiveChunkIndices.reserve(iDiameter * iDiameter);
	
	const int32_t iMinX = std::max(0, iCameraChunkX - static_cast<int32_t>(m_uActiveChunkRadius));
	const int32_t iMaxX = std::min(static_cast<int32_t>(CHUNK_GRID_SIZE - 1), iCameraChunkX + static_cast<int32_t>(m_uActiveChunkRadius));
	const int32_t iMinY = std::max(0, iCameraChunkY - static_cast<int32_t>(m_uActiveChunkRadius));
	const int32_t iMaxY = std::min(static_cast<int32_t>(CHUNK_GRID_SIZE - 1), iCameraChunkY + static_cast<int32_t>(m_uActiveChunkRadius));
	
	for (int32_t x = iMinX; x <= iMaxX; ++x)
	{
		for (int32_t y = iMinY; y <= iMaxY; ++y)
		{
			m_xActiveChunkIndices.push_back(ChunkCoordsToIndex(static_cast<uint32_t>(x), static_cast<uint32_t>(y)));
		}
	}
}

void Flux_TerrainStreamingManager::BuildChunkDataForGPU(Zenith_TerrainChunkData* pxChunkDataOut) const
{
	// One-time debug log to verify chunk data
	static bool s_bLoggedOnce = false;

	// Cache AABBs on first call (expensive - loads all meshes)
	// Subsequent calls reuse cached AABBs (cheap - just updates LOD data)
	if (!m_bAABBsCached)
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
				m_axChunkAABBs[uChunkIndex] = Zenith_FrustumCulling::GenerateAABBFromVertices(
					xChunkMesh.m_pxPositions,
					xChunkMesh.m_uNumVerts
				);

				Zenith_AssetHandler::DeleteMesh(strChunkName);
			}
		}
		m_bAABBsCached = true;
	}

	// Build chunk data using cached AABBs and current LOD allocations
	for (uint32_t x = 0; x < CHUNK_GRID_SIZE; ++x)
	{
		for (uint32_t y = 0; y < CHUNK_GRID_SIZE; ++y)
		{
			uint32_t uChunkIndex = ChunkCoordsToIndex(x, y);
			const Flux_TerrainChunkResidency& xResidency = m_axChunkResidency[uChunkIndex];
			Zenith_TerrainChunkData& xChunkData = pxChunkDataOut[uChunkIndex];

			// Use cached AABB
			const Zenith_AABB& xChunkAABB = m_axChunkAABBs[uChunkIndex];
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
			const Flux_TerrainChunkResidency& xRes = m_axChunkResidency[i];
			if (xRes.m_aeStates[0] == Flux_TerrainLODResidencyState::RESIDENT) uLOD0Resident++;
			if (xRes.m_aeStates[1] == Flux_TerrainLODResidencyState::RESIDENT) uLOD1Resident++;
			if (xRes.m_aeStates[2] == Flux_TerrainLODResidencyState::RESIDENT) uLOD2Resident++;
			if (xRes.m_aeStates[3] == Flux_TerrainLODResidencyState::RESIDENT) uLOD3Resident++;
		}
		Zenith_Log("=== LOD Residency Summary ===");
		Zenith_Log("  LOD0 resident: %u chunks", uLOD0Resident);
		Zenith_Log("  LOD1 resident: %u chunks", uLOD1Resident);
		Zenith_Log("  LOD2 resident: %u chunks", uLOD2Resident);
		Zenith_Log("  LOD3 resident: %u chunks (always)", uLOD3Resident);
		
		// Log a few sample chunks at different positions to verify mesh data differs
		uint32_t sampleChunks[] = {0, 32, 63, 2048, 4095};
		for (uint32_t i = 0; i < 5; ++i)
		{
			uint32_t idx = sampleChunks[i];
			if (idx >= TOTAL_CHUNKS) continue;
			
			uint32_t cx = idx / CHUNK_GRID_SIZE;
			uint32_t cy = idx % CHUNK_GRID_SIZE;
			const Zenith_TerrainChunkData& chunk = pxChunkDataOut[idx];
			const Flux_TerrainChunkResidency& xRes = m_axChunkResidency[idx];
			
			Zenith_Log("Chunk[%u] (%u,%u):", idx, cx, cy);
			Zenith_Log("  AABB: min=(%.1f, %.1f, %.1f) max=(%.1f, %.1f, %.1f)",
				chunk.m_xAABBMin.x, chunk.m_xAABBMin.y, chunk.m_xAABBMin.z,
				chunk.m_xAABBMax.x, chunk.m_xAABBMax.y, chunk.m_xAABBMax.z);
			
			// Log mesh data for each LOD level
			for (uint32_t lod = 0; lod < LOD_COUNT; ++lod)
			{
				const char* stateStr = "NOT_LOADED";
				if (xRes.m_aeStates[lod] == Flux_TerrainLODResidencyState::RESIDENT) stateStr = "RESIDENT";
				else if (xRes.m_aeStates[lod] == Flux_TerrainLODResidencyState::LOADING) stateStr = "LOADING";
				else if (xRes.m_aeStates[lod] == Flux_TerrainLODResidencyState::QUEUED) stateStr = "QUEUED";
				
				Zenith_Log("  LOD%u [%s]: firstIndex=%u, indexCount=%u, vertexOffset=%u",
					lod, stateStr,
					chunk.m_axLODs[lod].m_uFirstIndex,
					chunk.m_axLODs[lod].m_uIndexCount,
					chunk.m_axLODs[lod].m_uVertexOffset);
			}
		}
		Zenith_Log("=================================================");
	}
}
