#include "Zenith.h"
#include "Flux_TerrainStreamingManager.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "AssetHandling/Zenith_AssetHandler.h"
#include "Vulkan/Zenith_Vulkan_MemoryManager.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Profiling/Zenith_Profiling.h"
#include "Maths/Zenith_FrustumCulling.h"
#include <algorithm>
#include <fstream>

// ========== Configuration ==========

// Streaming buffer budget: 256 MB for vertices, 64 MB for indices
// This allows roughly 1024 high-LOD chunks resident at once
constexpr uint64_t STREAMING_VERTEX_BUFFER_SIZE_MB = 256;
constexpr uint64_t STREAMING_INDEX_BUFFER_SIZE_MB = 64;

constexpr uint64_t STREAMING_VERTEX_BUFFER_SIZE = STREAMING_VERTEX_BUFFER_SIZE_MB * 1024 * 1024;
constexpr uint64_t STREAMING_INDEX_BUFFER_SIZE = STREAMING_INDEX_BUFFER_SIZE_MB * 1024 * 1024;

// Streaming processing limits per frame to avoid stalls
constexpr uint32_t MAX_STREAMING_UPLOADS_PER_FRAME = 16;
constexpr uint32_t MAX_EVICTIONS_PER_FRAME = 32;

// Debug logging control
DEBUGVAR bool dbg_bLogTerrainStreaming = true;   // ENABLED for debugging
DEBUGVAR bool dbg_bLogTerrainEvictions = true;   // ENABLED for debugging
DEBUGVAR bool dbg_bLogTerrainAllocations = true; // ENABLED for debugging
DEBUGVAR bool dbg_bLogTerrainVertexData = true;  // ENABLED for debugging - Detailed vertex data tracing
DEBUGVAR float dbg_fStreamingAggressiveness = 1.0f;  // Multiplier for streaming distance thresholds

// Debug: Track specific chunks for forensic vertex data verification
static constexpr uint32_t DBG_TRACKED_CHUNK_X = 0;
static constexpr uint32_t DBG_TRACKED_CHUNK_Y = 0;
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
	Zenith_Log("==========================================================");

	// ========== Calculate LOD3 Buffer Sizes ==========
	// LOD3 is always resident for all 4096 chunks
	// Calculate exact size needed for LOD3 across all chunks

	const uint32_t uNumChunks = TERRAIN_EXPORT_DIMS * TERRAIN_EXPORT_DIMS;
	const float fLOD3Density = 0.125f;  // From Zenith_TerrainComponent

	uint32_t uLOD3TotalVerts = 0;
	uint32_t uLOD3TotalIndices = 0;

	for (uint32_t z = 0; z < TERRAIN_EXPORT_DIMS; ++z)
	{
		for (uint32_t x = 0; x < TERRAIN_EXPORT_DIMS; ++x)
		{
			bool hasRightEdge = (x < TERRAIN_EXPORT_DIMS - 1);
			bool hasTopEdge = (z < TERRAIN_EXPORT_DIMS - 1);

			// Base vertices and indices
			uint32_t verts = (uint32_t)((TERRAIN_SIZE * fLOD3Density + 1) * (TERRAIN_SIZE * fLOD3Density + 1));
			uint32_t indices = (uint32_t)((TERRAIN_SIZE * fLOD3Density) * (TERRAIN_SIZE * fLOD3Density) * 6);

			// Edge stitching
			if (hasRightEdge)
			{
				verts += (uint32_t)(TERRAIN_SIZE * fLOD3Density);
				indices += (uint32_t)((TERRAIN_SIZE * fLOD3Density - 1) * 6);
			}
			if (hasTopEdge)
			{
				verts += (uint32_t)(TERRAIN_SIZE * fLOD3Density);
				indices += (uint32_t)((TERRAIN_SIZE * fLOD3Density - 1) * 6);
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
	Zenith_Log("  Vertices: %u (%.2f MB)", uLOD3TotalVerts, (uLOD3TotalVerts * 60.0f) / (1024 * 1024));  // 60 bytes per vertex
	Zenith_Log("  Indices: %u (%.2f MB)", uLOD3TotalIndices, (uLOD3TotalIndices * 4.0f) / (1024 * 1024));  // 4 bytes per index

	// ========== Load and Combine LOD3 Chunks ==========

	Zenith_Log("Loading LOD3 meshes for all %u chunks...", uNumChunks);

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
	for (uint32_t x = 0; x < TERRAIN_EXPORT_DIMS; ++x)
	{
		for (uint32_t y = 0; y < TERRAIN_EXPORT_DIMS; ++y)
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

			if ((x * TERRAIN_EXPORT_DIMS + y) % 512 == 0)
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

	for (uint32_t x = 0; x < TERRAIN_EXPORT_DIMS; ++x)
	{
		for (uint32_t y = 0; y < TERRAIN_EXPORT_DIMS; ++y)
		{
			uint32_t uChunkIndex = s_pxInstance->ChunkCoordsToIndex(x, y);
			Flux_TerrainChunkResidency& xResidency = s_pxInstance->m_axChunkResidency[uChunkIndex];

			// Mark LOD3 as always resident (stored in LOD3 buffer)
			xResidency.m_aeStates[3] = Flux_TerrainLODResidencyState::RESIDENT;

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

			xResidency.m_axAllocations[3].m_uVertexOffset = uCurrentLOD3VertexOffset;
			xResidency.m_axAllocations[3].m_uVertexCount = xChunkMesh.m_uNumVerts;
			xResidency.m_axAllocations[3].m_uIndexOffset = uCurrentLOD3IndexOffset;
			xResidency.m_axAllocations[3].m_uIndexCount = xChunkMesh.m_uNumIndices;

			uCurrentLOD3VertexOffset += xChunkMesh.m_uNumVerts;
			uCurrentLOD3IndexOffset += xChunkMesh.m_uNumIndices;

			Zenith_AssetHandler::DeleteMesh(strChunkName);

			// Mark LOD0-2 as not loaded
			for (uint32_t uLOD = 0; uLOD < 3; ++uLOD)
			{
				xResidency.m_aeStates[uLOD] = Flux_TerrainLODResidencyState::NOT_LOADED;
				xResidency.m_auLastRequestedFrame[uLOD] = 0;
				xResidency.m_afPriorities[uLOD] = FLT_MAX;
			}

			xResidency.m_auLastRequestedFrame[3] = 0;
			xResidency.m_afPriorities[3] = FLT_MAX;
		}
	}

	Zenith_Log("Chunk residency state initialized: LOD3 resident for all %u chunks", uNumChunks);

	// ========== Cache Chunk AABBs ==========
	// Load LOD0 meshes to get accurate world-space AABBs for each chunk
	// This is needed for accurate streaming distance calculations
	
	Zenith_Log("Caching chunk AABBs from LOD0 meshes...");
	
	for (uint32_t x = 0; x < TERRAIN_EXPORT_DIMS; ++x)
	{
		for (uint32_t y = 0; y < TERRAIN_EXPORT_DIMS; ++y)
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
	
	Zenith_Log("Chunk AABBs cached for %u chunks", uNumChunks);

	// ========== Initialize Stats ==========

	s_pxInstance->m_xStats.m_uLOD3ChunksResident = uNumChunks;
	s_pxInstance->m_xStats.m_uHighLODChunksResident = 0;
	s_pxInstance->m_xStats.m_uStreamingRequestsThisFrame = 0;
	s_pxInstance->m_xStats.m_uEvictionsThisFrame = 0;
	s_pxInstance->m_xStats.m_uVertexBufferUsedMB = 0;
	s_pxInstance->m_xStats.m_uVertexBufferTotalMB = STREAMING_VERTEX_BUFFER_SIZE_MB;
	s_pxInstance->m_xStats.m_uIndexBufferUsedMB = 0;
	s_pxInstance->m_xStats.m_uIndexBufferTotalMB = STREAMING_INDEX_BUFFER_SIZE_MB;

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Render", "Terrain", "Log Streaming" }, dbg_bLogTerrainStreaming);
	Zenith_DebugVariables::AddBoolean({ "Render", "Terrain", "Log Evictions" }, dbg_bLogTerrainEvictions);
	Zenith_DebugVariables::AddBoolean({ "Render", "Terrain", "Log Allocations" }, dbg_bLogTerrainAllocations);
	Zenith_DebugVariables::AddBoolean({ "Render", "Terrain", "Log Vertex Data" }, dbg_bLogTerrainVertexData);
	Zenith_DebugVariables::AddFloat({ "Render", "Terrain", "Streaming Aggressiveness" }, dbg_fStreamingAggressiveness, 0.1f, 3.0f);
#endif

	Zenith_Log("==========================================================");
	Zenith_Log("Flux_TerrainStreamingManager initialization complete");
	Zenith_Log("  LOD3 always-resident: %u chunks", uNumChunks);
	Zenith_Log("  Streaming budget: %llu MB vertices, %llu MB indices",
		STREAMING_VERTEX_BUFFER_SIZE_MB, STREAMING_INDEX_BUFFER_SIZE_MB);
	Zenith_Log("==========================================================");
}

void Flux_TerrainStreamingManager::Shutdown()
{
	if (s_pxInstance)
	{
		Zenith_Log("Flux_TerrainStreamingManager::Shutdown()");
		delete s_pxInstance;
		s_pxInstance = nullptr;
	}
}

void Flux_TerrainStreamingManager::UpdateStreaming(const Zenith_Maths::Vector3& xCameraPos)
{
	//ZENITH_PROFILING_FUNCTION(Flux_TerrainStreamingManager::UpdateStreaming, ZENITH_PROFILE_INDEX__FLUX_TERRAIN);

	m_uCurrentFrame++;

	// Reset per-frame stats
	m_xStats.m_uStreamingRequestsThisFrame = 0;
	m_xStats.m_uEvictionsThisFrame = 0;

	// ========== CPU-side LOD Selection and Streaming Requests ==========
	// Iterate through all chunks and request appropriate LODs based on camera distance
	// LOD distance thresholds (distance squared):
	// LOD0: 0-400000 (0-632m)
	// LOD1: 400000-1000000 (632-1000m)
	// LOD2: 1000000-2000000 (1000-1414m)
	// LOD3: 2000000+ (1414m+, always resident)

	for (uint32_t x = 0; x < TERRAIN_EXPORT_DIMS; ++x)
	{
		for (uint32_t y = 0; y < TERRAIN_EXPORT_DIMS; ++y)
		{
			// Calculate distance from camera to chunk center
			Zenith_Maths::Vector3 xChunkCenter = GetChunkCenter(x, y);
			float fDistanceSq = glm::distance2(xCameraPos, xChunkCenter);

			// Determine desired LOD based on distance
			uint32_t uDesiredLOD = 3;  // Default to LOD3 (always resident)
			if (fDistanceSq < 4000000.0f)
				uDesiredLOD = 0;  // Highest detail
			else if (fDistanceSq < 10000000.0f)
				uDesiredLOD = 1;
			else if (fDistanceSq < 20000000.0f)
				uDesiredLOD = 2;

			// Request the desired LOD (will queue for streaming if not resident)
			// LOD3 is always resident, so no need to request
			if (uDesiredLOD < 3)
			{
				RequestLOD(x, y, uDesiredLOD, fDistanceSq);
			}
		}
	}

	// Process streaming queue (upload requested LODs)
	ZENITH_PROFILING_FUNCTION_WRAPPER(ProcessStreamingQueue, ZENITH_PROFILE_INDEX__FLUX_TERRAIN);

	// Update stats
	uint32_t uHighLODResident = 0;
	for (uint32_t i = 0; i < TERRAIN_EXPORT_DIMS * TERRAIN_EXPORT_DIMS; ++i)
	{
		for (uint32_t uLOD = 0; uLOD < 3; ++uLOD)
		{
			if (m_axChunkResidency[i].m_aeStates[uLOD] == Flux_TerrainLODResidencyState::RESIDENT)
			{
				uHighLODResident++;
			}
		}
	}
	m_xStats.m_uHighLODChunksResident = uHighLODResident;

	const uint32_t uVertexBytesUsed = (m_xVertexAllocator.GetTotalSpace() - m_xVertexAllocator.GetUnusedSpace()) * 60;  // 60 bytes per vertex (approx)
	const uint32_t uIndexBytesUsed = (m_xIndexAllocator.GetTotalSpace() - m_xIndexAllocator.GetUnusedSpace()) * 4;  // 4 bytes per index

	m_xStats.m_uVertexBufferUsedMB = uVertexBytesUsed / (1024 * 1024);
	m_xStats.m_uIndexBufferUsedMB = uIndexBytesUsed / (1024 * 1024);
	m_xStats.m_uVertexFragments = m_xVertexAllocator.GetFragmentationCount();
	m_xStats.m_uIndexFragments = m_xIndexAllocator.GetFragmentationCount();

	// Log stats periodically (every 60 frames = ~1 second)
	if (m_uCurrentFrame % 60 == 0 && dbg_bLogTerrainStreaming)
	{
		LogStats();
	}
}

bool Flux_TerrainStreamingManager::RequestLOD(uint32_t uChunkX, uint32_t uChunkY, uint32_t uLODLevel, float fPriority)
{
	Zenith_Assert(uChunkX < TERRAIN_EXPORT_DIMS && uChunkY < TERRAIN_EXPORT_DIMS, "Invalid chunk coordinates");
	Zenith_Assert(uLODLevel < TERRAIN_LOD_COUNT, "Invalid LOD level");

	uint32_t uChunkIndex = ChunkCoordsToIndex(uChunkX, uChunkY);
	Flux_TerrainChunkResidency& xResidency = m_axChunkResidency[uChunkIndex];

	// Update last requested frame and priority
	xResidency.m_auLastRequestedFrame[uLODLevel] = m_uCurrentFrame;
	xResidency.m_afPriorities[uLODLevel] = fPriority;

	Flux_TerrainLODResidencyState eState = xResidency.m_aeStates[uLODLevel];

	if (eState == Flux_TerrainLODResidencyState::RESIDENT)
	{
		// Already resident, ready to use
		return true;
	}
	else if (eState == Flux_TerrainLODResidencyState::LOADING || eState == Flux_TerrainLODResidencyState::QUEUED)
	{
		// Already queued or being loaded, don't re-queue
		return false;
	}
	else if (eState == Flux_TerrainLODResidencyState::NOT_LOADED)
	{
		// Limit queue size to prevent unbounded growth
		constexpr size_t MAX_QUEUE_SIZE = 256;
		if (m_xStreamingQueue.size() >= MAX_QUEUE_SIZE)
		{
			// Queue is full, skip this request (will be retried next frame)
			return false;
		}

		// Not loaded, add to streaming queue
		StreamingRequest request;
		request.m_uChunkIndex = uChunkIndex;
		request.m_uLODLevel = uLODLevel;
		request.m_fPriority = fPriority;
		m_xStreamingQueue.push(request);

		// Mark as queued to prevent duplicate requests
		xResidency.m_aeStates[uLODLevel] = Flux_TerrainLODResidencyState::QUEUED;

		m_xStats.m_uStreamingRequestsThisFrame++;

		if (dbg_bLogTerrainStreaming)
		{
			Zenith_Log("[TerrainStreaming] Chunk (%u,%u) LOD%u requested (priority=%.1f, queue size=%zu)",
				uChunkX, uChunkY, uLODLevel, fPriority, m_xStreamingQueue.size());
		}

		return false;
	}

	return false;
}

bool Flux_TerrainStreamingManager::GetLODAllocation(uint32_t uChunkX, uint32_t uChunkY, uint32_t uLODLevel, Flux_TerrainLODAllocation& xAllocOut) const
{
	Zenith_Assert(uChunkX < TERRAIN_EXPORT_DIMS && uChunkY < TERRAIN_EXPORT_DIMS, "Invalid chunk coordinates");
	Zenith_Assert(uLODLevel < TERRAIN_LOD_COUNT, "Invalid LOD level");

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
	Zenith_Assert(uChunkX < TERRAIN_EXPORT_DIMS && uChunkY < TERRAIN_EXPORT_DIMS, "Invalid chunk coordinates");
	Zenith_Assert(uLODLevel < TERRAIN_LOD_COUNT, "Invalid LOD level");

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

	while (!m_xStreamingQueue.empty() && uUploadsThisFrame < MAX_STREAMING_UPLOADS_PER_FRAME)
	{
		StreamingRequest request = m_xStreamingQueue.top();
		m_xStreamingQueue.pop();

		uint32_t uChunkIndex = request.m_uChunkIndex;
		uint32_t uLODLevel = request.m_uLODLevel;

		// Check if already resident or loading (may have been processed by another request)
		Flux_TerrainChunkResidency& xResidency = m_axChunkResidency[uChunkIndex];
		if (xResidency.m_aeStates[uLODLevel] == Flux_TerrainLODResidencyState::RESIDENT ||
			xResidency.m_aeStates[uLODLevel] == Flux_TerrainLODResidencyState::LOADING)
		{
			continue;  // Already being handled or complete
		}

		// Reset state from QUEUED - will be set to LOADING once we start, or back to NOT_LOADED on failure
		xResidency.m_aeStates[uLODLevel] = Flux_TerrainLODResidencyState::LOADING;

		// Load mesh to get size requirements
		uint32_t uChunkX, uChunkY;
		ChunkIndexToCoords(uChunkIndex, uChunkX, uChunkY);

		const char* LOD_SUFFIXES[3] = { "", "_LOD1", "_LOD2" };  // Only for LOD0-2
		std::string strChunkName = "Terrain_Streaming_LOD" + std::to_string(uLODLevel) + "_" + std::to_string(uChunkX) + "_" + std::to_string(uChunkY);
		std::string strChunkPath = std::string(ASSETS_ROOT"Terrain/Render") + LOD_SUFFIXES[uLODLevel] + "_" + std::to_string(uChunkX) + "_" + std::to_string(uChunkY) + ".zmsh";

		// Check if file exists
		std::ifstream lodFile(strChunkPath);
		if (!lodFile.good())
		{
			if (dbg_bLogTerrainStreaming)
			{
				Zenith_Log("[TerrainStreaming] WARNING: LOD%u file not found for chunk (%u,%u), skipping", uLODLevel, uChunkX, uChunkY);
			}
			xResidency.m_aeStates[uLODLevel] = Flux_TerrainLODResidencyState::NOT_LOADED;
			continue;
		}

		Zenith_AssetHandler::AddMesh(strChunkName, strChunkPath.c_str(),
			1 << Flux_MeshGeometry::FLUX_VERTEX_ATTRIBUTE__POSITION);
		Flux_MeshGeometry& xChunkMesh = Zenith_AssetHandler::GetMesh(strChunkName);

		// Try to allocate space
		uint32_t uVertexOffset = m_xVertexAllocator.Allocate(xChunkMesh.m_uNumVerts);
		uint32_t uIndexOffset = m_xIndexAllocator.Allocate(xChunkMesh.m_uNumIndices);

		if (uVertexOffset == UINT32_MAX || uIndexOffset == UINT32_MAX)
		{
			// Not enough space, try eviction
			if (dbg_bLogTerrainStreaming)
			{
				Zenith_Log("[TerrainStreaming] Insufficient space for Chunk (%u,%u) LOD%u (%u verts, %u indices), attempting eviction...",
					uChunkX, uChunkY, uLODLevel, xChunkMesh.m_uNumVerts, xChunkMesh.m_uNumIndices);
			}

			// Free partial allocation if one succeeded
			if (uVertexOffset != UINT32_MAX)
			{
				m_xVertexAllocator.Free(uVertexOffset, xChunkMesh.m_uNumVerts);
			}
			if (uIndexOffset != UINT32_MAX)
			{
				m_xIndexAllocator.Free(uIndexOffset, xChunkMesh.m_uNumIndices);
			}

			// Attempt eviction
			bool bEvictionSuccess = EvictToMakeSpace(xChunkMesh.m_uNumVerts, xChunkMesh.m_uNumIndices);
			if (!bEvictionSuccess)
			{
				if (dbg_bLogTerrainStreaming)
				{
					Zenith_Log("[TerrainStreaming] FAILED to evict enough space for Chunk (%u,%u) LOD%u, deferring",
						uChunkX, uChunkY, uLODLevel);
				}
				xResidency.m_aeStates[uLODLevel] = Flux_TerrainLODResidencyState::NOT_LOADED;
				Zenith_AssetHandler::DeleteMesh(strChunkName);
				continue;  // Can't make space, defer this request
			}

			// Retry allocation after eviction
			uVertexOffset = m_xVertexAllocator.Allocate(xChunkMesh.m_uNumVerts);
			uIndexOffset = m_xIndexAllocator.Allocate(xChunkMesh.m_uNumIndices);

			if (uVertexOffset == UINT32_MAX || uIndexOffset == UINT32_MAX)
			{
				if (dbg_bLogTerrainStreaming)
				{
					Zenith_Log("[TerrainStreaming] FAILED to allocate after eviction for Chunk (%u,%u) LOD%u",
						uChunkX, uChunkY, uLODLevel);
				}

				// Free partial allocation if one succeeded
				if (uVertexOffset != UINT32_MAX)
				{
					m_xVertexAllocator.Free(uVertexOffset, xChunkMesh.m_uNumVerts);
				}
				if (uIndexOffset != UINT32_MAX)
				{
					m_xIndexAllocator.Free(uIndexOffset, xChunkMesh.m_uNumIndices);
				}

				xResidency.m_aeStates[uLODLevel] = Flux_TerrainLODResidencyState::NOT_LOADED;
				Zenith_AssetHandler::DeleteMesh(strChunkName);
				continue;
			}
		}

		// Validate allocation is within streaming region bounds
		const uint32_t uMaxStreamingVertices = m_xVertexAllocator.GetTotalSpace();
		const uint32_t uMaxStreamingIndices = m_xIndexAllocator.GetTotalSpace();
		
		if (uVertexOffset + xChunkMesh.m_uNumVerts > uMaxStreamingVertices)
		{
			Zenith_Log("[TerrainStreaming] ERROR: Vertex allocation out of bounds! offset=%u, size=%u, max=%u",
				uVertexOffset, xChunkMesh.m_uNumVerts, uMaxStreamingVertices);
			m_xVertexAllocator.Free(uVertexOffset, xChunkMesh.m_uNumVerts);
			m_xIndexAllocator.Free(uIndexOffset, xChunkMesh.m_uNumIndices);
			xResidency.m_aeStates[uLODLevel] = Flux_TerrainLODResidencyState::NOT_LOADED;
			Zenith_AssetHandler::DeleteMesh(strChunkName);
			continue;
		}
		
		if (uIndexOffset + xChunkMesh.m_uNumIndices > uMaxStreamingIndices)
		{
			Zenith_Log("[TerrainStreaming] ERROR: Index allocation out of bounds! offset=%u, size=%u, max=%u",
				uIndexOffset, xChunkMesh.m_uNumIndices, uMaxStreamingIndices);
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
			if (dbg_bLogTerrainStreaming)
			{
				Zenith_Log("[TerrainStreaming] FAILED to stream in Chunk (%u,%u) LOD%u", uChunkX, uChunkY, uLODLevel);
			}

			// Free allocations on failure
			m_xVertexAllocator.Free(uVertexOffset, xChunkMesh.m_uNumVerts);
			m_xIndexAllocator.Free(uIndexOffset, xChunkMesh.m_uNumIndices);
			xResidency.m_aeStates[uLODLevel] = Flux_TerrainLODResidencyState::NOT_LOADED;
			Zenith_AssetHandler::DeleteMesh(strChunkName);
			continue;
		}

		Zenith_AssetHandler::DeleteMesh(strChunkName);
		uUploadsThisFrame++;

		if (dbg_bLogTerrainStreaming)
		{
			Zenith_Log("[TerrainStreaming] Chunk (%u,%u) LOD%u streamed in (%u verts @ %u, %u indices @ %u)",
				uChunkX, uChunkY, uLODLevel,
				xResidency.m_axAllocations[uLODLevel].m_uVertexCount, uVertexOffset,
				xResidency.m_axAllocations[uLODLevel].m_uIndexCount, uIndexOffset);
		}
	}
}

bool Flux_TerrainStreamingManager::EvictToMakeSpace(uint32_t uVertexSpaceNeeded, uint32_t uIndexSpaceNeeded)
{
	//ZENITH_PROFILING_FUNCTION(EvictToMakeSpace, ZENITH_PROFILE_INDEX__FLUX_TERRAIN);

	// Build eviction candidate list (all resident high LODs sorted by priority)
	// Note: This is a simplified approach; a real engine might use a smarter heuristic

	std::vector<EvictionCandidate> candidates;
	for (uint32_t i = 0; i < TERRAIN_EXPORT_DIMS * TERRAIN_EXPORT_DIMS; ++i)
	{
		Flux_TerrainChunkResidency& xResidency = m_axChunkResidency[i];
		for (uint32_t uLOD = 0; uLOD < 3; ++uLOD)  // Only LOD0-2 can be evicted
		{
			if (xResidency.m_aeStates[uLOD] == Flux_TerrainLODResidencyState::RESIDENT)
			{
				EvictionCandidate candidate;
				candidate.m_uChunkIndex = i;
				candidate.m_uLODLevel = uLOD;

				// Priority: higher value = more likely to evict
				// Use last requested frame (older = higher priority to evict)
				uint32_t uFramesSinceRequested = m_uCurrentFrame - xResidency.m_auLastRequestedFrame[uLOD];
				candidate.m_fPriority = static_cast<float>(uFramesSinceRequested) + xResidency.m_afPriorities[uLOD];

				candidates.push_back(candidate);
			}
		}
	}

	if (candidates.empty())
	{
		// Nothing to evict
		return false;
	}

	// Sort candidates by priority (highest first = most likely to evict)
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
		{
			break;  // Freed enough space
		}

		if (uEvictionsThisCall >= MAX_EVICTIONS_PER_FRAME)
		{
			break;  // Hit per-frame eviction limit
		}

		// Evict this LOD
		Flux_TerrainChunkResidency& xResidency = m_axChunkResidency[candidate.m_uChunkIndex];
		Flux_TerrainLODAllocation& xAlloc = xResidency.m_axAllocations[candidate.m_uLODLevel];

		uVertexSpaceFreed += xAlloc.m_uVertexCount;
		uIndexSpaceFreed += xAlloc.m_uIndexCount;

		EvictLOD(candidate.m_uChunkIndex, candidate.m_uLODLevel);
		uEvictionsThisCall++;
		m_xStats.m_uEvictionsThisFrame++;
	}

	// Check if we freed enough
	bool bSuccess = (uVertexSpaceFreed >= uVertexSpaceNeeded && uIndexSpaceFreed >= uIndexSpaceNeeded);

	if (dbg_bLogTerrainEvictions)
	{
		Zenith_Log("[TerrainEviction] Evicted %u LODs, freed %u verts, %u indices (needed %u verts, %u indices) - %s",
			uEvictionsThisCall, uVertexSpaceFreed, uIndexSpaceFreed, uVertexSpaceNeeded, uIndexSpaceNeeded,
			bSuccess ? "SUCCESS" : "INSUFFICIENT");
	}

	return bSuccess;
}

bool Flux_TerrainStreamingManager::StreamInLOD(uint32_t uChunkIndex, uint32_t uLODLevel, uint32_t uVertexOffset, uint32_t uIndexOffset)
{
	Zenith_Assert(uLODLevel < 3, "StreamInLOD only for LOD0-2");

	uint32_t uChunkX, uChunkY;
	ChunkIndexToCoords(uChunkIndex, uChunkX, uChunkY);

	Flux_TerrainChunkResidency& xResidency = m_axChunkResidency[uChunkIndex];

	// Mark as loading
	xResidency.m_aeStates[uLODLevel] = Flux_TerrainLODResidencyState::LOADING;

	// Load mesh (should already be loaded by caller, but reload to be safe)
	const char* LOD_SUFFIXES[3] = { "", "_LOD1", "_LOD2" };
	std::string strChunkName = "Terrain_Upload_LOD" + std::to_string(uLODLevel) + "_" + std::to_string(uChunkX) + "_" + std::to_string(uChunkY);
	std::string strChunkPath = std::string(ASSETS_ROOT"Terrain/Render") + LOD_SUFFIXES[uLODLevel] + "_" + std::to_string(uChunkX) + "_" + std::to_string(uChunkY) + ".zmsh";

	// Load with all vertex attributes (not just position)
	Zenith_AssetHandler::AddMesh(strChunkName, strChunkPath.c_str(), 0);  // 0 = load all attributes
	Flux_MeshGeometry& xChunkMesh = Zenith_AssetHandler::GetMesh(strChunkName);

	// Calculate absolute offsets in unified buffer (streaming region starts after LOD3)
	const uint32_t uAbsoluteVertexOffset = m_uLOD3VertexCount + uVertexOffset;
	const uint32_t uAbsoluteIndexOffset = m_uLOD3IndexCount + uIndexOffset;

	// Upload vertex data to unified buffer at absolute offset
	const uint32_t uVertexStride = xChunkMesh.m_xBufferLayout.GetStride();
	const uint64_t ulVertexDataSize = static_cast<uint64_t>(xChunkMesh.m_uNumVerts) * uVertexStride;
	const uint64_t ulVertexOffsetBytes = static_cast<uint64_t>(uAbsoluteVertexOffset) * uVertexStride;

	// Upload index data to unified buffer at absolute offset
	const uint64_t ulIndexDataSize = static_cast<uint64_t>(xChunkMesh.m_uNumIndices) * sizeof(uint32_t);
	const uint64_t ulIndexOffsetBytes = static_cast<uint64_t>(uAbsoluteIndexOffset) * sizeof(uint32_t);

	// ========== BOUNDS CHECK: Ensure we don't exceed buffer size ==========
	if (ulVertexOffsetBytes + ulVertexDataSize > m_ulUnifiedVertexBufferSize)
	{
		Zenith_Log("[TerrainStreaming] ERROR: Vertex upload would exceed buffer! Chunk (%u,%u) LOD%u", uChunkX, uChunkY, uLODLevel);
		Zenith_Log("  Offset: %llu bytes, Size: %llu bytes, Buffer: %llu bytes", 
			ulVertexOffsetBytes, ulVertexDataSize, m_ulUnifiedVertexBufferSize);
		Zenith_Log("  uVertexOffset (relative): %u, uAbsoluteVertexOffset: %u, stride: %u",
			uVertexOffset, uAbsoluteVertexOffset, uVertexStride);
		Zenith_AssetHandler::DeleteMesh(strChunkName);
		return false;
	}

	if (ulIndexOffsetBytes + ulIndexDataSize > m_ulUnifiedIndexBufferSize)
	{
		Zenith_Log("[TerrainStreaming] ERROR: Index upload would exceed buffer! Chunk (%u,%u) LOD%u", uChunkX, uChunkY, uLODLevel);
		Zenith_Log("  Offset: %llu bytes, Size: %llu bytes, Buffer: %llu bytes",
			ulIndexOffsetBytes, ulIndexDataSize, m_ulUnifiedIndexBufferSize);
		Zenith_AssetHandler::DeleteMesh(strChunkName);
		return false;
	}

	// ========== DEBUG: Track specific chunk vertex data upload ==========
	if (dbg_bLogTerrainVertexData && uChunkX == DBG_TRACKED_CHUNK_X && uChunkY == DBG_TRACKED_CHUNK_Y && uLODLevel == DBG_TRACKED_LOD)
	{
		Zenith_Log("=== VERTEX DATA UPLOAD: Chunk (%u,%u) LOD%u ===", uChunkX, uChunkY, uLODLevel);
		Zenith_Log("  Absolute vertex offset: %u vertices = %llu bytes", uAbsoluteVertexOffset, ulVertexOffsetBytes);
		Zenith_Log("  Vertex count: %u vertices", xChunkMesh.m_uNumVerts);
		Zenith_Log("  Vertex stride: %u bytes", uVertexStride);

		// Sample first, middle, and last vertex positions from CPU data
		if (xChunkMesh.m_pxPositions)
		{
			Zenith_Log("  CPU Vertex Samples:");
			Zenith_Log("    Vertex [0]: pos=(%.2f, %.2f, %.2f)",
				xChunkMesh.m_pxPositions[0].x, xChunkMesh.m_pxPositions[0].y, xChunkMesh.m_pxPositions[0].z);

			uint32_t uMid = xChunkMesh.m_uNumVerts / 2;
			Zenith_Log("    Vertex [%u] (mid): pos=(%.2f, %.2f, %.2f)", uMid,
				xChunkMesh.m_pxPositions[uMid].x, xChunkMesh.m_pxPositions[uMid].y, xChunkMesh.m_pxPositions[uMid].z);

			uint32_t uLast = xChunkMesh.m_uNumVerts - 1;
			Zenith_Log("    Vertex [%u] (last): pos=(%.2f, %.2f, %.2f)", uLast,
				xChunkMesh.m_pxPositions[uLast].x, xChunkMesh.m_pxPositions[uLast].y, xChunkMesh.m_pxPositions[uLast].z);
		}

		// Sample index data (should be relative, starting from 0)
		if (xChunkMesh.m_puIndices)
		{
			Zenith_Log("  CPU Index Samples (should be 0-based, relative to chunk):");
			Zenith_Log("    Index [0-2]: %u, %u, %u",
				xChunkMesh.m_puIndices[0], xChunkMesh.m_puIndices[1], xChunkMesh.m_puIndices[2]);
			Zenith_Log("    Index [%u-%u] (last tri): %u, %u, %u",
				xChunkMesh.m_uNumIndices - 3, xChunkMesh.m_uNumIndices - 1,
				xChunkMesh.m_puIndices[xChunkMesh.m_uNumIndices - 3],
				xChunkMesh.m_puIndices[xChunkMesh.m_uNumIndices - 2],
				xChunkMesh.m_puIndices[xChunkMesh.m_uNumIndices - 1]);
		}
	}

	Flux_MemoryManager::UploadBufferDataAtOffset(
		m_xUnifiedVertexBuffer.GetBuffer().m_xVRAMHandle,
		xChunkMesh.m_pVertexData,
		ulVertexDataSize,
		ulVertexOffsetBytes
	);

	// Upload index data to unified buffer at absolute offset (size/offset already computed above for bounds check)
	Flux_MemoryManager::UploadBufferDataAtOffset(
		m_xUnifiedIndexBuffer.GetBuffer().m_xVRAMHandle,
		xChunkMesh.m_puIndices,
		ulIndexDataSize,
		ulIndexOffsetBytes
	);

	// Update residency state with ABSOLUTE buffer offsets
	xResidency.m_axAllocations[uLODLevel].m_uVertexOffset = uAbsoluteVertexOffset;
	xResidency.m_axAllocations[uLODLevel].m_uVertexCount = xChunkMesh.m_uNumVerts;
	xResidency.m_axAllocations[uLODLevel].m_uIndexOffset = uAbsoluteIndexOffset;
	xResidency.m_axAllocations[uLODLevel].m_uIndexCount = xChunkMesh.m_uNumIndices;
	xResidency.m_aeStates[uLODLevel] = Flux_TerrainLODResidencyState::RESIDENT;

	// ========== DEBUG: Verify allocation stored correctly ==========
	if (dbg_bLogTerrainVertexData && uChunkX == DBG_TRACKED_CHUNK_X && uChunkY == DBG_TRACKED_CHUNK_Y && uLODLevel == DBG_TRACKED_LOD)
	{
		Zenith_Log("  Allocation stored:");
		Zenith_Log("    m_uVertexOffset = %u (ABSOLUTE in unified buffer)", uAbsoluteVertexOffset);
		Zenith_Log("    m_uVertexCount = %u", xChunkMesh.m_uNumVerts);
		Zenith_Log("    m_uIndexOffset = %u (ABSOLUTE in unified buffer)", uAbsoluteIndexOffset);
		Zenith_Log("    m_uIndexCount = %u", xChunkMesh.m_uNumIndices);
	}

	Zenith_AssetHandler::DeleteMesh(strChunkName);

	return true;
}

void Flux_TerrainStreamingManager::EvictLOD(uint32_t uChunkIndex, uint32_t uLODLevel)
{
	Zenith_Assert(uLODLevel < 3, "EvictLOD only for LOD0-2");

	uint32_t uChunkX, uChunkY;
	ChunkIndexToCoords(uChunkIndex, uChunkX, uChunkY);

	Flux_TerrainChunkResidency& xResidency = m_axChunkResidency[uChunkIndex];

	if (xResidency.m_aeStates[uLODLevel] != Flux_TerrainLODResidencyState::RESIDENT)
	{
		// Not resident, nothing to evict
		return;
	}

	// Mark as evicting
	xResidency.m_aeStates[uLODLevel] = Flux_TerrainLODResidencyState::EVICTING;

	// Free allocations
	Flux_TerrainLODAllocation& xAlloc = xResidency.m_axAllocations[uLODLevel];
	m_xVertexAllocator.Free(xAlloc.m_uVertexOffset, xAlloc.m_uVertexCount);
	m_xIndexAllocator.Free(xAlloc.m_uIndexOffset, xAlloc.m_uIndexCount);

	// Mark as not loaded
	xResidency.m_aeStates[uLODLevel] = Flux_TerrainLODResidencyState::NOT_LOADED;

	if (dbg_bLogTerrainEvictions)
	{
		Zenith_Log("[TerrainEviction] Chunk (%u,%u) LOD%u evicted (freed %u verts, %u indices)",
			uChunkX, uChunkY, uLODLevel, xAlloc.m_uVertexCount, xAlloc.m_uIndexCount);
	}
}

std::vector<Flux_TerrainStreamingManager::EvictionCandidate> Flux_TerrainStreamingManager::BuildEvictionCandidates(const Zenith_Maths::Vector3& xCameraPos) const
{
	std::vector<EvictionCandidate> candidates;

	for (uint32_t i = 0; i < TERRAIN_EXPORT_DIMS * TERRAIN_EXPORT_DIMS; ++i)
	{
		const Flux_TerrainChunkResidency& xResidency = m_axChunkResidency[i];
		for (uint32_t uLOD = 0; uLOD < 3; ++uLOD)
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
	// NOTE: This uses TERRAIN_SIZE * TERRAIN_SCALE which may not match actual exported mesh positions
	// This fallback should only be used before AABBs are cached
	const float fChunkSizeWorld = TERRAIN_SIZE * TERRAIN_SCALE;
	const float fX = (static_cast<float>(uChunkX) + 0.5f) * fChunkSizeWorld;
	const float fZ = (static_cast<float>(uChunkY) + 0.5f) * fChunkSizeWorld;
	const float fY = MAX_TERRAIN_HEIGHT * 0.5f;  // Approximate center height

	return Zenith_Maths::Vector3(fX, fY, fZ);
}

void Flux_TerrainStreamingManager::BuildChunkDataForGPU(Zenith_TerrainChunkData* pxChunkDataOut) const
{
	// LOD distance thresholds (distance squared)
	static constexpr float LOD_DISTANCES_SQ[TERRAIN_LOD_COUNT] = {
		400000.0f,
		1000000.0f,
		2000000.0f,
		FLT_MAX
	};

	// Cache AABBs on first call (expensive - loads all meshes)
	// Subsequent calls reuse cached AABBs (cheap - just updates LOD data)
	if (!m_bAABBsCached)
	{
		for (uint32_t x = 0; x < TERRAIN_EXPORT_DIMS; ++x)
		{
			for (uint32_t y = 0; y < TERRAIN_EXPORT_DIMS; ++y)
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
	for (uint32_t x = 0; x < TERRAIN_EXPORT_DIMS; ++x)
	{
		for (uint32_t y = 0; y < TERRAIN_EXPORT_DIMS; ++y)
		{
			uint32_t uChunkIndex = ChunkCoordsToIndex(x, y);
			const Flux_TerrainChunkResidency& xResidency = m_axChunkResidency[uChunkIndex];
			Zenith_TerrainChunkData& xChunkData = pxChunkDataOut[uChunkIndex];

			// Use cached AABB
			const Zenith_AABB& xChunkAABB = m_axChunkAABBs[uChunkIndex];
			xChunkData.m_xAABBMin = Zenith_Maths::Vector4(xChunkAABB.m_xMin, 0.0f);
			xChunkData.m_xAABBMax = Zenith_Maths::Vector4(xChunkAABB.m_xMax, 0.0f);

			// Fill in LOD data with current allocations
			for (uint32_t uLOD = 0; uLOD < TERRAIN_LOD_COUNT; ++uLOD)
			{
				xChunkData.m_axLODs[uLOD].m_fMaxDistance = LOD_DISTANCES_SQ[uLOD];

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
					if (uLOD == 3)
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
					const Flux_TerrainLODAllocation& xLOD3Alloc = xResidency.m_axAllocations[3];
					xChunkData.m_axLODs[uLOD].m_uFirstIndex = xLOD3Alloc.m_uIndexOffset;
					xChunkData.m_axLODs[uLOD].m_uIndexCount = xLOD3Alloc.m_uIndexCount;
					// LOD3 uses combined buffer with rebased indices, so vertexOffset = 0
					xChunkData.m_axLODs[uLOD].m_uVertexOffset = 0;
				}
			}
		}
	}
}
