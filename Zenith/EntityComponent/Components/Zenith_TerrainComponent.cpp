#include "Zenith.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Profiling/Zenith_Profiling.h"
#include "DataStream/Zenith_DataStream.h"
#include "AssetHandling/Zenith_AssetHandler.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Terrain/Flux_TerrainStreamingManager.h"
#include "Vulkan/Zenith_Vulkan_MemoryManager.h"
#include <fstream>

// LOD distance thresholds (distance squared)
static constexpr float LOD_DISTANCES_SQ[TERRAIN_LOD_COUNT] = {
	400000.0f,
	1000000.0f,
	2000000.0f,
	FLT_MAX
};

Zenith_TerrainComponent::Zenith_TerrainComponent(Flux_Material& xMaterial0, Flux_Material& xMaterial1, Zenith_Entity& xEntity)
	: m_pxMaterial0(&xMaterial0)
	, m_pxMaterial1(&xMaterial1)
	, m_bCullingResourcesInitialized(false)
	, m_xParentEntity(xEntity)
{
#pragma region Render
{
	// Define LOD suffixes
	const char* LOD_SUFFIXES[4] = { "", "_LOD1", "_LOD2", "_LOD3" };
	
	// Load all chunks for all LOD levels
	// IMPORTANT: Load with POSITION attribute retained for AABB calculation
	for (uint32_t uLOD = 0; uLOD < 4; ++uLOD)
	{
		for (uint32_t x = 0; x < TERRAIN_EXPORT_DIMS; x++)
		{
			for (uint32_t y = 0; y < TERRAIN_EXPORT_DIMS; y++)
			{
				std::string strSuffix = std::to_string(x) + "_" + std::to_string(y);
				std::string strLODMeshName = std::string("Terrain_Render") + LOD_SUFFIXES[uLOD] + strSuffix;
				std::string strLODPath = std::string(ASSETS_ROOT"Terrain/Render") + LOD_SUFFIXES[uLOD] + "_" + strSuffix + ".zmsh";
				
				// Check if LOD file exists, fallback to LOD0 if not
				std::ifstream lodFile(strLODPath);
				if (!lodFile.good() && uLOD > 0)
				{
					// Use LOD0 as fallback
					strLODPath = std::string(ASSETS_ROOT"Terrain/Render_") + strSuffix + ".zmsh";
					Zenith_Log("WARNING: LOD%u not found for chunk (%u,%u), using LOD0 as fallback", uLOD, x, y);
					Zenith_Assert(false, "");
				}
				
				// Load mesh with POSITION attribute retained for AABB calculation
				Zenith_AssetHandler::AddMesh(strLODMeshName, strLODPath.c_str(), 
					1 << Flux_MeshGeometry::FLUX_VERTEX_ATTRIBUTE__POSITION);
			}
		}
	}

	// Start with LOD0 chunk 0,0 as base
	Flux_MeshGeometry& xRenderGeometry = Zenith_AssetHandler::GetMesh("Terrain_Render0_0");

	// Calculate EXACT total size needed for ALL chunks � ALL LOD levels
	// Based on terrain export formulas from Zenith_Tools_TerrainExport.cpp
	//
	// Each chunk has:
	// - Base vertices: (TERRAIN_SIZE * density + 1)^2
	// - Right edge stitching (if not rightmost chunk): TERRAIN_SIZE * density verts
	// - Top edge stitching (if not topmost chunk): TERRAIN_SIZE * density verts
	// - Top-right corner (if has both edges): 1 vert
	//
	// Indices follow a similar pattern with extra triangles for stitching
	
	const uint32_t uNumChunks = TERRAIN_EXPORT_DIMS * TERRAIN_EXPORT_DIMS;
	const uint32_t uChunksX = TERRAIN_EXPORT_DIMS;
	const uint32_t uChunksZ = TERRAIN_EXPORT_DIMS;
	
	// Calculate total for all chunks at all LOD levels
	uint32_t uTotalVerts = 0;
	uint32_t uTotalIndices = 0;
	
	const float densities[4] = { 1.0f, 0.5f, 0.25f, 0.125f }; // LOD0, LOD1, LOD2, LOD3
	
	for (uint32_t lodLevel = 0; lodLevel < 4; ++lodLevel)
	{
		float density = densities[lodLevel];
		
		for (uint32_t z = 0; z < uChunksZ; ++z)
		{
			for (uint32_t x = 0; x < uChunksX; ++x)
			{
				bool hasRightEdge = (x < uChunksX - 1);
				bool hasTopEdge = (z < uChunksZ - 1);
				
				// Base vertices and indices
				uint32_t verts = (uint32_t)((TERRAIN_SIZE * density + 1) * (TERRAIN_SIZE * density + 1));
				uint32_t indices = (uint32_t)((TERRAIN_SIZE * density) * (TERRAIN_SIZE * density) * 6);
				
				// Add edge stitching vertices and indices
				if (hasRightEdge)
				{
					verts += (uint32_t)(TERRAIN_SIZE * density);
					indices += (uint32_t)((TERRAIN_SIZE * density - 1) * 6); // Right edge triangles
				}
				if (hasTopEdge)
				{
					verts += (uint32_t)(TERRAIN_SIZE * density);
					indices += (uint32_t)((TERRAIN_SIZE * density - 1) * 6); // Top edge triangles
				}
				if (hasRightEdge && hasTopEdge)
				{
					verts += 1; // Top-right corner vertex
					indices += 6; // Corner triangles
				}
				
				uTotalVerts += verts;
				uTotalIndices += indices;
			}
		}
	}
	
	// Convert to byte sizes
	const u_int64 ulTotalVertexDataSize = static_cast<u_int64>(uTotalVerts) * xRenderGeometry.m_xBufferLayout.GetStride();
	const u_int64 ulTotalIndexDataSize = static_cast<u_int64>(uTotalIndices) * sizeof(Flux_MeshGeometry::IndexType);
	const u_int64 ulTotalPositionDataSize = static_cast<u_int64>(uTotalVerts) * sizeof(Zenith_Maths::Vector3);

	Zenith_Log("Terrain EXACT pre-allocation (with edge stitching): %u total verts, %u total indices across all LODs", uTotalVerts, uTotalIndices);
	Zenith_Log("Terrain EXACT pre-allocation: Vertex=%llu MB, Index=%llu MB, Position=%llu MB",
		ulTotalVertexDataSize / (1024*1024), ulTotalIndexDataSize / (1024*1024), ulTotalPositionDataSize / (1024*1024));
	
	// Pre-allocate buffers and set reserved sizes BEFORE combining to avoid reallocations
	xRenderGeometry.m_pVertexData = static_cast<u_int8*>(Zenith_MemoryManagement::Reallocate(xRenderGeometry.m_pVertexData, ulTotalVertexDataSize));
	xRenderGeometry.m_ulReservedVertexDataSize = ulTotalVertexDataSize;

	xRenderGeometry.m_puIndices = static_cast<Flux_MeshGeometry::IndexType*>(Zenith_MemoryManagement::Reallocate(xRenderGeometry.m_puIndices, ulTotalIndexDataSize));
	xRenderGeometry.m_ulReservedIndexDataSize = ulTotalIndexDataSize;

	// Also pre-allocate position buffer (needed for AABB calculations later)
	if (xRenderGeometry.m_pxPositions)
	{
		xRenderGeometry.m_pxPositions = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Reallocate(xRenderGeometry.m_pxPositions, ulTotalPositionDataSize));
		xRenderGeometry.m_ulReservedPositionDataSize = ulTotalPositionDataSize;
	}
	
	// Combine all chunks for all LOD levels
	for (uint32_t x = 0; x < TERRAIN_EXPORT_DIMS; x++)
	{
		for (uint32_t y = 0; y < TERRAIN_EXPORT_DIMS; y++)
		{
			for (uint32_t uLOD = 0; uLOD < 4; ++uLOD)
			{
				// Skip the first chunk's LOD0 (already loaded as base)
				if (x == 0 && y == 0 && uLOD == 0) continue;

				std::string strSuffix = std::to_string(x) + "_" + std::to_string(y);
				std::string strLODMeshName = std::string("Terrain_Render") + LOD_SUFFIXES[uLOD] + strSuffix;
				Flux_MeshGeometry& xTerrainRenderMesh = Zenith_AssetHandler::GetMesh(strLODMeshName);

				Flux_MeshGeometry::Combine(xRenderGeometry, xTerrainRenderMesh);
				
				if ((x * TERRAIN_EXPORT_DIMS + y) % 256 == 0 || uLOD == 0)
				{
					Zenith_Log("Combined LOD%u chunk (%u,%u)", uLOD, x, y);
				}

				// Don't delete yet - we'll need these for BuildCullingData()
			}
		}
	}

	Zenith_Log("Terrain: Combined %u chunks � 4 LOD levels into unified vertex/index buffers", TERRAIN_EXPORT_DIMS * TERRAIN_EXPORT_DIMS);
	Zenith_Log("Terrain: Total vertices: %u, Total indices: %u", xRenderGeometry.m_uNumVerts, xRenderGeometry.m_uNumIndices);

	Flux_MemoryManager::InitialiseVertexBuffer(xRenderGeometry.GetVertexData(), xRenderGeometry.GetVertexDataSize(), xRenderGeometry.m_xVertexBuffer);
	Flux_MemoryManager::InitialiseIndexBuffer(xRenderGeometry.GetIndexData(), xRenderGeometry.GetIndexDataSize(), xRenderGeometry.m_xIndexBuffer);

	// DEPRECATED: Render geometry now managed by Flux_TerrainStreamingManager
	// Setup facade to reference streaming manager's unified buffers
	Flux_TerrainStreamingManager& xStreamMgr = Flux_TerrainStreamingManager::Get();
	m_xRenderGeometryFacade.m_xVertexBuffer = xStreamMgr.GetTerrainVertexBuffer();
	m_xRenderGeometryFacade.m_xIndexBuffer = xStreamMgr.GetTerrainIndexBuffer();

	Zenith_Log("Terrain render geometry facade setup complete (references streaming manager buffers)");

	// Initialize GPU culling resources for this terrain component
	// This allocates GPU buffers and builds chunk AABB + LOD metadata
	InitializeCullingResources();
}
#pragma endregion

#pragma region Physics
{
	for (uint32_t x = 0; x < TERRAIN_EXPORT_DIMS; x++)
	{
		for (uint32_t y = 0; y < TERRAIN_EXPORT_DIMS; y++)
		{
			std::string strSuffix = std::to_string(x) + "_" + std::to_string(y);

			Zenith_AssetHandler::AddMesh("Terrain_Physics" + strSuffix, std::string(ASSETS_ROOT"Terrain/Physics_" + strSuffix + ".zmsh").c_str(), 1 << Flux_MeshGeometry::FLUX_VERTEX_ATTRIBUTE__POSITION | 1 << Flux_MeshGeometry::FLUX_VERTEX_ATTRIBUTE__NORMAL);
		}
	}

	Flux_MeshGeometry& xPhysicsGeometry = Zenith_AssetHandler::GetMesh("Terrain_Physics0_0");

	const u_int64 ulTotalVertexDataSize = xPhysicsGeometry.GetVertexDataSize() * TERRAIN_EXPORT_DIMS * TERRAIN_EXPORT_DIMS;
	const u_int64 ulTotalIndexDataSize = xPhysicsGeometry.GetIndexDataSize() * TERRAIN_EXPORT_DIMS * TERRAIN_EXPORT_DIMS;
	const u_int64 ulTotalPositionDataSize = xPhysicsGeometry.m_uNumVerts * sizeof(Zenith_Maths::Vector3) * TERRAIN_EXPORT_DIMS * TERRAIN_EXPORT_DIMS;

	xPhysicsGeometry.m_pVertexData = static_cast<u_int8*>(Zenith_MemoryManagement::Reallocate(xPhysicsGeometry.m_pVertexData, ulTotalVertexDataSize));
	xPhysicsGeometry.m_ulReservedVertexDataSize = ulTotalVertexDataSize;

	xPhysicsGeometry.m_puIndices = static_cast<Flux_MeshGeometry::IndexType*>(Zenith_MemoryManagement::Reallocate(xPhysicsGeometry.m_puIndices, ulTotalIndexDataSize));
xPhysicsGeometry.m_ulReservedIndexDataSize = ulTotalIndexDataSize;

	xPhysicsGeometry.m_pxPositions = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Reallocate(xPhysicsGeometry.m_pxPositions, ulTotalPositionDataSize));
	xPhysicsGeometry.m_ulReservedPositionDataSize = ulTotalPositionDataSize;

	for (uint32_t x = 0; x < TERRAIN_EXPORT_DIMS; x++)
	{
		for (uint32_t y = 0; y < TERRAIN_EXPORT_DIMS; y++)
		{
			if (x == 0 && y == 0) continue;

			std::string strPhysicsMeshName = "Terrain_Physics" + std::to_string(x) + "_" + std::to_string(y);
			Flux_MeshGeometry& xTerrainPhysicsMesh = Zenith_AssetHandler::GetMesh(strPhysicsMeshName);

			Flux_MeshGeometry::Combine(xPhysicsGeometry, xTerrainPhysicsMesh);
			Zenith_Log("Combined %u %u", x, y);

			Zenith_AssetHandler::DeleteMesh(strPhysicsMeshName);
		}
	}

	m_pxPhysicsGeometry = &xPhysicsGeometry;
}
#pragma endregion
}

Zenith_TerrainComponent::~Zenith_TerrainComponent()
{
	DestroyCullingResources();
	// NOTE: Render geometry now managed by Flux_TerrainStreamingManager, not deleted here
	Zenith_AssetHandler::DeleteMesh("Terrain_Physics0_0");
}

const bool Zenith_TerrainComponent::IsVisible(const float fVisibilityMultiplier, const Zenith_CameraComponent& xCam) const
{
	Zenith_Profiling::BeginProfile(ZENITH_PROFILE_INDEX__VISIBILITY_CHECK);
	//#TO_TODO: this should be a camera frustum check against the terrain's encapsulating AABB
	Zenith_Maths::Vector3 xCamPos;
	xCam.GetPosition(xCamPos);
	const Zenith_Maths::Vector2 xCamPos_2D(xCamPos.x, xCamPos.z);

	bool bRet = true;//(glm::length(xCamPos_2D - GetPosition_2D()) < xCam.GetFarPlane() * 2 * fVisibilityMultiplier);
	Zenith_Profiling::EndProfile(ZENITH_PROFILE_INDEX__VISIBILITY_CHECK);
	return bRet;
}

void Zenith_TerrainComponent::WriteToDataStream(Zenith_DataStream& xStream) const
{
	// Get asset names from pointers
	// NOTE: Render geometry now managed by Flux_TerrainStreamingManager, not serialized
	std::string strPhysicsGeometryName = Zenith_AssetHandler::GetMeshName(m_pxPhysicsGeometry);
	std::string strMaterial0Name = Zenith_AssetHandler::GetMaterialName(m_pxMaterial0);
	std::string strMaterial1Name = Zenith_AssetHandler::GetMaterialName(m_pxMaterial1);

	// Write asset names
	xStream << strPhysicsGeometryName;
	xStream << strMaterial0Name;
	xStream << strMaterial1Name;

	// m_xParentEntity reference is not serialized - will be restored during deserialization
}

void Zenith_TerrainComponent::ReadFromDataStream(Zenith_DataStream& xStream)
{
	// Read asset names
	// NOTE: Render geometry now managed by Flux_TerrainStreamingManager, not deserialized
	std::string strPhysicsGeometryName;
	std::string strMaterial0Name;
	std::string strMaterial1Name;

	xStream >> strPhysicsGeometryName;
	xStream >> strMaterial0Name;
	xStream >> strMaterial1Name;

	// Look up assets by name (they must already be loaded)
	if (!strPhysicsGeometryName.empty() && !strMaterial0Name.empty() && !strMaterial1Name.empty())
	{
		if (Zenith_AssetHandler::MeshExists(strPhysicsGeometryName) &&
			Zenith_AssetHandler::MaterialExists(strMaterial0Name) &&
			Zenith_AssetHandler::MaterialExists(strMaterial1Name))
		{
			m_pxPhysicsGeometry = &Zenith_AssetHandler::GetMesh(strPhysicsGeometryName);
			m_pxMaterial0 = &Zenith_AssetHandler::GetMaterial(strMaterial0Name);
			m_pxMaterial1 = &Zenith_AssetHandler::GetMaterial(strMaterial1Name);

			// Setup render geometry facade to reference streaming manager buffers
			Flux_TerrainStreamingManager& xStreamMgr = Flux_TerrainStreamingManager::Get();
			m_xRenderGeometryFacade.m_xVertexBuffer = xStreamMgr.GetTerrainVertexBuffer();
			m_xRenderGeometryFacade.m_xIndexBuffer = xStreamMgr.GetTerrainIndexBuffer();
		}
		else
		{
			// Asset not loaded - this is expected if assets haven't been loaded yet
			Zenith_Assert(false, "Referenced assets not found during TerrainComponent deserialization");
		}
	}

	// m_xParentEntity will be set by the entity deserialization system
}

// ========== GPU-Driven Culling Implementation ==========

void Zenith_TerrainComponent::InitializeCullingResources()
{
	if (m_bCullingResourcesInitialized)
	{
		Zenith_Assert(false, "Zenith_TerrainComponent::InitializeCullingResources() called when already initialized");
		return;
	}

	Zenith_Log("Zenith_TerrainComponent::InitializeCullingResources() - Setting up GPU-driven terrain culling with LOD support");

	// ========== CREATE GPU BUFFERS ==========

	// Frustum planes buffer (6 planes, updated per frame)
	Flux_MemoryManager::InitialiseDynamicConstantBuffer(
		nullptr,
		sizeof(Zenith_CameraDataGPU),
		m_xFrustumPlanesBuffer
	);

	constexpr uint32_t TERRAIN_CHUNK_COUNT = TERRAIN_EXPORT_DIMS * TERRAIN_EXPORT_DIMS;

	// Indirect draw command buffer (one command per chunk, max)
	// Structure: VkDrawIndexedIndirectCommand (5 uint32_t values)
	// Initialize to zeros so all commands start with indexCount=0
	const size_t indirectBufferSize = sizeof(uint32_t) * 5 * TERRAIN_CHUNK_COUNT;
	uint32_t* pZeroBuffer = new uint32_t[5 * TERRAIN_CHUNK_COUNT];
	memset(pZeroBuffer, 0, indirectBufferSize);

	Flux_MemoryManager::InitialiseIndirectBuffer(
		indirectBufferSize,
		m_xIndirectDrawBuffer
	);

	// Upload the zero-initialized data
	Flux_MemoryManager::UploadBufferData(m_xIndirectDrawBuffer.GetBuffer().m_xVRAMHandle, pZeroBuffer, indirectBufferSize);
	delete[] pZeroBuffer;

	// Visible chunk counter (single atomic uint32_t)
	Flux_MemoryManager::InitialiseIndirectBuffer(
		sizeof(uint32_t),
		m_xVisibleCountBuffer
	);

	// LOD level buffer (one uint32_t per potential draw call)
	Flux_MemoryManager::InitialiseReadWriteBuffer(
		nullptr,
		sizeof(uint32_t) * TERRAIN_CHUNK_COUNT,
		m_xLODLevelBuffer
	);

	// Build chunk data (AABBs + LOD metadata) and upload to GPU
	BuildChunkData();

	m_bCullingResourcesInitialized = true;

	Zenith_Log("Zenith_TerrainComponent - Culling resources initialized with %u terrain chunks, %u LOD levels", TERRAIN_CHUNK_COUNT, TERRAIN_LOD_COUNT);
	Zenith_Log("Zenith_TerrainComponent - LOD distances: LOD0<%.1f, LOD1<%.1f, LOD2<%.1f, LOD3<inf",
		sqrtf(LOD_DISTANCES_SQ[0]), sqrtf(LOD_DISTANCES_SQ[1]), sqrtf(LOD_DISTANCES_SQ[2]));
}

void Zenith_TerrainComponent::DestroyCullingResources()
{
	if (!m_bCullingResourcesInitialized)
	{
		return;
	}

	// Cleanup GPU resources
	// Note: Flux_MemoryManager cleanup methods are currently commented out in the original code
	// so we follow the same pattern here for consistency
	//Flux_MemoryManager::DestroyBuffer(m_xChunkDataBuffer);
	//Flux_MemoryManager::DestroyBuffer(m_xFrustumPlanesBuffer);
	//Flux_MemoryManager::DestroyBuffer(m_xIndirectDrawBuffer);
	//Flux_MemoryManager::DestroyBuffer(m_xVisibleCountBuffer);
	//Flux_MemoryManager::DestroyBuffer(m_xLODLevelBuffer);

	m_bCullingResourcesInitialized = false;
}

void Zenith_TerrainComponent::BuildChunkData()
{
	constexpr uint32_t TERRAIN_CHUNK_COUNT = TERRAIN_EXPORT_DIMS * TERRAIN_EXPORT_DIMS;

	Zenith_Log("Zenith_TerrainComponent::BuildChunkData() - Building chunk data using streaming manager");

	// Get chunk data from streaming manager (includes AABBs and current LOD allocations)
	Zenith_TerrainChunkData* pxChunkData = new Zenith_TerrainChunkData[TERRAIN_CHUNK_COUNT];
	Flux_TerrainStreamingManager::Get().BuildChunkDataForGPU(pxChunkData);

	Zenith_Log("Zenith_TerrainComponent - Chunk data retrieved from streaming manager for %u chunks", TERRAIN_CHUNK_COUNT);

	// Upload chunk data to GPU
	Flux_MemoryManager::InitialiseReadWriteBuffer(
		pxChunkData,
		sizeof(Zenith_TerrainChunkData) * TERRAIN_CHUNK_COUNT,
		m_xChunkDataBuffer
	);

	// Cleanup CPU data
	delete[] pxChunkData;

	Zenith_Log("Zenith_TerrainComponent - Chunk data with %u LOD levels uploaded to GPU", TERRAIN_LOD_COUNT);
}

void Zenith_TerrainComponent::UpdateChunkLODAllocations()
{
	// OPTIMIZATION: Skip GPU upload if chunk data hasn't changed
	Flux_TerrainStreamingManager& xStreamMgr = Flux_TerrainStreamingManager::Get();
	if (!xStreamMgr.IsChunkDataDirty())
	{
		return;  // No changes since last upload
	}
	
	// DEBUG: Log when we're updating chunk allocations
	static uint32_t s_uUpdateCount = 0;
	s_uUpdateCount++;
	
	constexpr uint32_t TERRAIN_CHUNK_COUNT = TERRAIN_EXPORT_DIMS * TERRAIN_EXPORT_DIMS;

	// OPTIMIZATION: Use pre-allocated buffer from streaming manager to avoid heap allocation
	Zenith_TerrainChunkData* pxChunkData = xStreamMgr.GetCachedChunkDataBuffer();
	bool bUsedCachedBuffer = (pxChunkData != nullptr);
	
	if (pxChunkData == nullptr)
	{
		// Fallback to allocation if cached buffer not available (shouldn't happen)
		pxChunkData = new Zenith_TerrainChunkData[TERRAIN_CHUNK_COUNT];
	}
	
	xStreamMgr.BuildChunkDataForGPU(pxChunkData);
	
	// DEBUG: Log sample chunk data after streaming updates (first 5 updates, then every 100th)
	if (s_uUpdateCount <= 5 || s_uUpdateCount % 100 == 0)
	{
		Zenith_Log("=== Chunk Data After Update #%u (cached=%d) ===", s_uUpdateCount, bUsedCachedBuffer ? 1 : 0);
		
		// Log chunk 0 (0,0) - far from camera at (2100, y, 1475)
		const Zenith_TerrainChunkData& chunk0 = pxChunkData[0];
		Zenith_Log("Chunk[0] (0,0) - FAR from camera:");
		Zenith_Log("  LOD0: firstIndex=%u, indexCount=%u, vertexOffset=%u",
			chunk0.m_axLODs[0].m_uFirstIndex, chunk0.m_axLODs[0].m_uIndexCount, chunk0.m_axLODs[0].m_uVertexOffset);
		Zenith_Log("  LOD1: firstIndex=%u, indexCount=%u, vertexOffset=%u",
			chunk0.m_axLODs[1].m_uFirstIndex, chunk0.m_axLODs[1].m_uIndexCount, chunk0.m_axLODs[1].m_uVertexOffset);
		Zenith_Log("  LOD3: firstIndex=%u, indexCount=%u, vertexOffset=%u",
			chunk0.m_axLODs[3].m_uFirstIndex, chunk0.m_axLODs[3].m_uIndexCount, chunk0.m_axLODs[3].m_uVertexOffset);
		
		// Log chunk near camera: (32, 23) = index 32*64+23 = 2071
		uint32_t nearCameraIdx = 32 * 64 + 23;
		const Zenith_TerrainChunkData& chunkNear = pxChunkData[nearCameraIdx];
		Zenith_Log("Chunk[%u] (32,23) - NEAR camera:", nearCameraIdx);
		Zenith_Log("  LOD0: firstIndex=%u, indexCount=%u, vertexOffset=%u",
			chunkNear.m_axLODs[0].m_uFirstIndex, chunkNear.m_axLODs[0].m_uIndexCount, chunkNear.m_axLODs[0].m_uVertexOffset);
		Zenith_Log("  LOD1: firstIndex=%u, indexCount=%u, vertexOffset=%u",
			chunkNear.m_axLODs[1].m_uFirstIndex, chunkNear.m_axLODs[1].m_uIndexCount, chunkNear.m_axLODs[1].m_uVertexOffset);
		Zenith_Log("  LOD3: firstIndex=%u, indexCount=%u, vertexOffset=%u",
			chunkNear.m_axLODs[3].m_uFirstIndex, chunkNear.m_axLODs[3].m_uIndexCount, chunkNear.m_axLODs[3].m_uVertexOffset);
	}
	
	// CRITICAL: Use UploadBufferDataAtOffset which is synchronous (waits for GPU copy completion)
	// The regular UploadBufferData is deferred and would not be visible to the compute shader
	// that runs in the same frame
	Flux_MemoryManager::UploadBufferDataAtOffset(
		m_xChunkDataBuffer.GetBuffer().m_xVRAMHandle,
		pxChunkData,
		sizeof(Zenith_TerrainChunkData) * TERRAIN_CHUNK_COUNT,
		0  // Offset 0 - replace entire buffer
	);
	
	if (!bUsedCachedBuffer)
	{
		delete[] pxChunkData;
	}
	
	// Clear dirty flag after successful upload
	xStreamMgr.ClearChunkDataDirty();
}

void Zenith_TerrainComponent::ExtractFrustumPlanes(const Zenith_Maths::Matrix4& xViewProjMatrix, Zenith_FrustumPlaneGPU* pxOutPlanes)
{
	// Use existing frustum extraction code
	Zenith_Frustum xFrustum;
	xFrustum.ExtractFromViewProjection(xViewProjMatrix);

	// Convert to GPU format
	for (int i = 0; i < 6; ++i)
	{
		pxOutPlanes[i].m_xNormalAndDistance = Zenith_Maths::Vector4(
			xFrustum.m_axPlanes[i].m_xNormal,
			xFrustum.m_axPlanes[i].m_fDistance
		);
	}
}

void Zenith_TerrainComponent::UpdateCullingAndLod(Flux_CommandList& xCmdList, const Zenith_Maths::Matrix4& xViewProjMatrix)
{
	if (!m_bCullingResourcesInitialized)
	{
		Zenith_Log("ERROR: Zenith_TerrainComponent::UpdateCullingAndLod() called before InitializeCullingResources()");
		return;
	}

	// Extract frustum planes and camera position, upload to GPU
	Zenith_CameraDataGPU xCameraData;
	Zenith_Frustum xFrustum;
	xFrustum.ExtractFromViewProjection(xViewProjMatrix);

	// Convert frustum to GPU format
	for (int i = 0; i < 6; ++i)
	{
		xCameraData.m_axFrustumPlanes[i].m_xNormalAndDistance = Zenith_Maths::Vector4(
			xFrustum.m_axPlanes[i].m_xNormal,
			xFrustum.m_axPlanes[i].m_fDistance
		);
	}

	// Add camera position for distance-based sorting and LOD selection
	Zenith_Maths::Vector3 xCameraPos = Flux_Graphics::GetCameraPosition();
	xCameraData.m_xCameraPosition = Zenith_Maths::Vector4(xCameraPos, 0.0f);
	
	// One-time debug log to verify camera position
	static bool s_bLoggedCameraOnce = false;
	static uint32_t s_uCameraLogFrames = 0;
	if (!s_bLoggedCameraOnce || s_uCameraLogFrames < 5)
	{
		s_bLoggedCameraOnce = true;
		s_uCameraLogFrames++;
		Zenith_Log("UpdateCullingAndLod: Camera pos = (%.1f, %.1f, %.1f)", xCameraPos.x, xCameraPos.y, xCameraPos.z);
	}

	Flux_MemoryManager::UploadBufferData(m_xFrustumPlanesBuffer.GetBuffer().m_xVRAMHandle, &xCameraData, sizeof(Zenith_CameraDataGPU));

	// Reset visible chunk counter to 0
	uint32_t uZero = 0;
	Flux_MemoryManager::UploadBufferData(m_xVisibleCountBuffer.GetBuffer().m_xVRAMHandle, &uZero, sizeof(uint32_t));

	// IMPORTANT: Assumes the terrain culling compute pipeline is already bound by Flux_Terrain
	// We only record buffer bindings and dispatch here

	// Bind descriptor set 0 with all buffers
	xCmdList.AddCommand<Flux_CommandBeginBind>(0);
	xCmdList.AddCommand<Flux_CommandBindUAV_Buffer>(&m_xChunkDataBuffer.GetUAV(), 0);          // Chunk data (read)
	xCmdList.AddCommand<Flux_CommandBindCBV>(&m_xFrustumPlanesBuffer.GetCBV(), 1);             // Frustum planes (read)
	xCmdList.AddCommand<Flux_CommandBindUAV_Buffer>(&m_xIndirectDrawBuffer.GetUAV(), 2);       // Indirect commands (write)
	xCmdList.AddCommand<Flux_CommandBindUAV_Buffer>(&m_xVisibleCountBuffer.GetUAV(), 3);       // Visible count (read/write atomic)
	xCmdList.AddCommand<Flux_CommandBindUAV_Buffer>(&m_xLODLevelBuffer.GetUAV(), 4);           // LOD levels (write)

	// Dispatch compute shader
	// We have 64x64 = 4096 chunks, with local_size_x=64 we need (4096 + 63) / 64 = 64 workgroups
	constexpr uint32_t TERRAIN_CHUNK_COUNT = TERRAIN_EXPORT_DIMS * TERRAIN_EXPORT_DIMS;
	uint32_t uNumWorkgroups = (TERRAIN_CHUNK_COUNT + 63) / 64;
	xCmdList.AddCommand<Flux_CommandDispatch>(uNumWorkgroups, 1, 1);
}

