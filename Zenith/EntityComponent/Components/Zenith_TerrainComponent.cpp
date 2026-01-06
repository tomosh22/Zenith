#include "Zenith.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Zenith_ComponentMeta.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Profiling/Zenith_Profiling.h"
#include "DataStream/Zenith_DataStream.h"
#include "AssetHandling/Zenith_AssetHandler.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Terrain/Flux_TerrainStreamingManager.h"
#include "Vulkan/Zenith_Vulkan_MemoryManager.h"
#include <fstream>

ZENITH_REGISTER_COMPONENT(Zenith_TerrainComponent, "Terrain")

// LOD distance thresholds from unified config (distance squared)
// Used for debug visualization - actual thresholds are in Flux_TerrainConfig.h

// Static instance counter for terrain components - used to manage streaming manager lifecycle
uint32_t Zenith_TerrainComponent::s_uInstanceCount = 0;

void Zenith_TerrainComponent::IncrementInstanceCount()
{
	s_uInstanceCount++;
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Zenith_TerrainComponent instance count: %u (incremented)", s_uInstanceCount);
}

void Zenith_TerrainComponent::DecrementInstanceCount()
{
	if (s_uInstanceCount > 0)
	{
		s_uInstanceCount--;
		Zenith_Log(LOG_CATEGORY_TERRAIN, "Zenith_TerrainComponent instance count: %u (decremented)", s_uInstanceCount);

		// When the last terrain component is destroyed, shut down the streaming manager
		// to free the unified vertex/index buffers
		if (s_uInstanceCount == 0)
		{
			Zenith_Log(LOG_CATEGORY_TERRAIN, "Last Zenith_TerrainComponent destroyed - shutting down streaming manager");
			Flux_TerrainStreamingManager::Shutdown();
		}
	}
}

Zenith_TerrainComponent::Zenith_TerrainComponent(Flux_MaterialAsset& xMaterial0, Flux_MaterialAsset& xMaterial1, Zenith_Entity& xEntity)
	: m_pxMaterial0(&xMaterial0)
	, m_pxMaterial1(&xMaterial1)
	, m_bCullingResourcesInitialized(false)
	, m_xParentEntity(xEntity)
	, m_ulUnifiedVertexBufferSize(0)
	, m_ulUnifiedIndexBufferSize(0)
	, m_uVertexStride(0)
	, m_uLOD3VertexCount(0)
	, m_uLOD3IndexCount(0)
{
	IncrementInstanceCount();

	// Ensure streaming manager is initialized (may have been shut down after previous terrain was destroyed)
	if (!Flux_TerrainStreamingManager::IsInitialized())
	{
		Zenith_Log(LOG_CATEGORY_TERRAIN, "Zenith_TerrainComponent - Re-initializing streaming manager (was shut down)");
		Flux_TerrainStreamingManager::Initialize();
	}

#pragma region Render
{
	// Initialize render resources (LOD3 meshes, unified buffers, culling)
	// This is the same initialization performed during deserialization
	InitializeRenderResources(xMaterial0, xMaterial1);

	// NOTE: The following old code has been extracted into InitializeRenderResources()
	// to allow reuse during deserialization
	/*
	const float fLOD3Density = 0.125f;

	uint32_t uLOD3TotalVerts = 0;
	uint32_t uLOD3TotalIndices = 0;

	for (uint32_t z = 0; z < CHUNK_GRID_SIZE; ++z)
	{
		for (uint32_t x = 0; x < CHUNK_GRID_SIZE; ++x)
		{
			bool hasRightEdge = (x < CHUNK_GRID_SIZE - 1);
			bool hasTopEdge = (z < CHUNK_GRID_SIZE - 1);

			// Base vertices and indices per chunk (64x64 units)
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

	Zenith_Log(LOG_CATEGORY_TERRAIN, "LOD3 (always-resident) buffer requirements:");
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Vertices: %u (%.2f MB)", uLOD3TotalVerts, (uLOD3TotalVerts * TERRAIN_VERTEX_STRIDE) / (1024.0f * 1024.0f));
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Indices: %u (%.2f MB)", uLOD3TotalIndices, (uLOD3TotalIndices * 4.0f) / (1024.0f * 1024.0f));

	// ========== Load and Combine LOD3 Chunks ==========
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Loading LOD3 meshes for all %u chunks...", TOTAL_CHUNKS);

	// Load first chunk to get buffer layout (stored as owned pointer for later cleanup)
	Flux_MeshGeometry* pxLOD3Geometry = Zenith_AssetHandler::AddMeshFromFile(
		(GetGameAssetsDir() + "Terrain/Render_LOD3_0_0" ZENITH_MESH_EXT).c_str(), 0);  // 0 = load all attributes
	Flux_MeshGeometry& xLOD3Geometry = *pxLOD3Geometry;

	// Store vertex stride for buffer calculations
	m_uVertexStride = xLOD3Geometry.GetBufferLayout().GetStride();

	// Pre-allocate for all LOD3 chunks
	const uint64_t ulLOD3VertexDataSize = static_cast<uint64_t>(uLOD3TotalVerts) * m_uVertexStride;
	const uint64_t ulLOD3IndexDataSize = static_cast<uint64_t>(uLOD3TotalIndices) * sizeof(Flux_MeshGeometry::IndexType);
	const uint64_t ulLOD3PositionDataSize = static_cast<uint64_t>(uLOD3TotalVerts) * sizeof(Zenith_Maths::Vector3);

	xLOD3Geometry.m_pVertexData = static_cast<u_int8*>(Zenith_MemoryManagement::Reallocate(xLOD3Geometry.m_pVertexData, ulLOD3VertexDataSize));
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

			std::string strChunkPath = GetGameAssetsDir() + "Terrain/Render_LOD3_" + std::to_string(x) + "_" + std::to_string(y) + ZENITH_MESH_EXT;

			// Check if LOD3 file exists, fallback to LOD0 if not
			std::ifstream lodFile(strChunkPath);
			if (!lodFile.good())
			{
				Zenith_Log(LOG_CATEGORY_TERRAIN, "WARNING: LOD3 not found for chunk (%u,%u), using LOD0 as fallback", x, y);
				strChunkPath = GetGameAssetsDir() + "Terrain/Render_" + std::to_string(x) + "_" + std::to_string(y) + ZENITH_MESH_EXT;
			}

			Flux_MeshGeometry* pxChunkMesh = Zenith_AssetHandler::AddMeshFromFile(strChunkPath.c_str(), 0);  // 0 = all attributes
			Flux_MeshGeometry::Combine(xLOD3Geometry, *pxChunkMesh);
			Zenith_AssetHandler::DeleteMesh(pxChunkMesh);

			if ((x * CHUNK_GRID_SIZE + y) % 512 == 0)
			{
				Zenith_Log(LOG_CATEGORY_TERRAIN, "Combined LOD3 chunk (%u,%u)", x, y);
			}
		}
	}

	Zenith_Log(LOG_CATEGORY_TERRAIN, "LOD3 mesh combination complete: %u vertices, %u indices",
		xLOD3Geometry.GetNumVerts(), xLOD3Geometry.GetNumIndices());

	// Store LOD3 counts
	m_uLOD3VertexCount = xLOD3Geometry.GetNumVerts();
	m_uLOD3IndexCount = xLOD3Geometry.GetNumIndices();

	// ========== Initialize Unified Buffers (LOD3 + Streaming Space) ==========
	const uint64_t ulLOD3VertexSize = xLOD3Geometry.GetVertexDataSize();
	const uint64_t ulLOD3IndexSize = xLOD3Geometry.GetIndexDataSize();
	const uint64_t ulUnifiedVertexSize = ulLOD3VertexSize + STREAMING_VERTEX_BUFFER_SIZE;
	const uint64_t ulUnifiedIndexSize = ulLOD3IndexSize + STREAMING_INDEX_BUFFER_SIZE;

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Initializing unified terrain buffers (owned by TerrainComponent):");
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Vertex buffer: %.2f MB LOD3 + %llu MB streaming = %.2f MB total",
		ulLOD3VertexSize / (1024.0f * 1024.0f), STREAMING_VERTEX_BUFFER_SIZE_MB,
		ulUnifiedVertexSize / (1024.0f * 1024.0f));
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Index buffer: %.2f MB LOD3 + %llu MB streaming = %.2f MB total",
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

	// Upload unified buffers to GPU (component owns these)
	Flux_MemoryManager::InitialiseVertexBuffer(pUnifiedVertexData, ulUnifiedVertexSize, m_xUnifiedVertexBuffer);
	Flux_MemoryManager::InitialiseIndexBuffer(pUnifiedIndexData, ulUnifiedIndexSize, m_xUnifiedIndexBuffer);

	// Store buffer sizes
	m_ulUnifiedVertexBufferSize = ulUnifiedVertexSize;
	m_ulUnifiedIndexBufferSize = ulUnifiedIndexSize;

	delete[] pUnifiedVertexData;
	delete[] pUnifiedIndexData;

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Unified terrain buffers uploaded to GPU");
	Zenith_Log(LOG_CATEGORY_TERRAIN, "LOD3 region: vertices [0, %u), indices [0, %u)", m_uLOD3VertexCount, m_uLOD3IndexCount);
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Streaming region starts at: vertex %u, index %u", m_uLOD3VertexCount, m_uLOD3IndexCount);

	// Clean up LOD3 CPU data (no longer needed, data is in GPU buffer)
	Zenith_AssetHandler::DeleteMesh(pxLOD3Geometry);

	// ========== Register buffers with streaming manager ==========
	Flux_TerrainStreamingManager::RegisterTerrainBuffers(this);

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Terrain render geometry facade setup complete (references component-owned buffers)");

	// Initialize GPU culling resources for this terrain component
	// This allocates GPU buffers and builds chunk AABB + LOD metadata
	InitializeCullingResources();
	*/
}
#pragma endregion

#pragma region Physics
{
	// Load first physics chunk
	m_pxPhysicsGeometry = Zenith_AssetHandler::AddMeshFromFile(
		(std::string(Project_GetGameAssetsDirectory()) + "Terrain/Physics_0_0" ZENITH_MESH_EXT).c_str(),
		1 << Flux_MeshGeometry::FLUX_VERTEX_ATTRIBUTE__POSITION | 1 << Flux_MeshGeometry::FLUX_VERTEX_ATTRIBUTE__NORMAL);

	Flux_MeshGeometry& xPhysicsGeometry = *m_pxPhysicsGeometry;

	const u_int64 ulTotalVertexDataSize = xPhysicsGeometry.GetVertexDataSize() * TOTAL_CHUNKS;
	const u_int64 ulTotalIndexDataSize = xPhysicsGeometry.GetIndexDataSize() * TOTAL_CHUNKS;
	const u_int64 ulTotalPositionDataSize = xPhysicsGeometry.GetNumVerts() * sizeof(Zenith_Maths::Vector3) * TOTAL_CHUNKS;

	xPhysicsGeometry.m_pVertexData = static_cast<u_int8*>(Zenith_MemoryManagement::Reallocate(xPhysicsGeometry.m_pVertexData, ulTotalVertexDataSize));
	xPhysicsGeometry.m_ulReservedVertexDataSize = ulTotalVertexDataSize;

	xPhysicsGeometry.m_puIndices = static_cast<Flux_MeshGeometry::IndexType*>(Zenith_MemoryManagement::Reallocate(xPhysicsGeometry.m_puIndices, ulTotalIndexDataSize));
	xPhysicsGeometry.m_ulReservedIndexDataSize = ulTotalIndexDataSize;

	xPhysicsGeometry.m_pxPositions = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Reallocate(xPhysicsGeometry.m_pxPositions, ulTotalPositionDataSize));
	xPhysicsGeometry.m_ulReservedPositionDataSize = ulTotalPositionDataSize;

	// Combine remaining physics chunks
	for (uint32_t x = 0; x < CHUNK_GRID_SIZE; x++)
	{
		for (uint32_t y = 0; y < CHUNK_GRID_SIZE; y++)
		{
			if (x == 0 && y == 0) continue;

			std::string strPhysicsPath = std::string(Project_GetGameAssetsDirectory()) + "Terrain/Physics_" + std::to_string(x) + "_" + std::to_string(y) + ZENITH_MESH_EXT;
			Flux_MeshGeometry* pxTerrainPhysicsMesh = Zenith_AssetHandler::AddMeshFromFile(
				strPhysicsPath.c_str(),
				1 << Flux_MeshGeometry::FLUX_VERTEX_ATTRIBUTE__POSITION | 1 << Flux_MeshGeometry::FLUX_VERTEX_ATTRIBUTE__NORMAL);

			Flux_MeshGeometry::Combine(xPhysicsGeometry, *pxTerrainPhysicsMesh);

			Zenith_AssetHandler::DeleteMesh(pxTerrainPhysicsMesh);
		}
	}
}
#pragma endregion
}

Zenith_TerrainComponent::~Zenith_TerrainComponent()
{
	DestroyCullingResources();

	// Unregister buffers from streaming manager before destroying them
	Flux_TerrainStreamingManager::UnregisterTerrainBuffers();

	// Destroy owned unified buffers
	Flux_MemoryManager::DestroyVertexBuffer(m_xUnifiedVertexBuffer);
	Flux_MemoryManager::DestroyIndexBuffer(m_xUnifiedIndexBuffer);

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Zenith_TerrainComponent - Unified terrain buffers destroyed");

	Zenith_AssetHandler::DeleteMesh(m_pxPhysicsGeometry);
	
	// Clean up materials if we own them (created during deserialization)
	if (m_bOwnsMaterials)
	{
		Zenith_Log(LOG_CATEGORY_TERRAIN, "Zenith_TerrainComponent - Cleaning up owned materials");

		// Flux_MaterialAsset manages its own lifecycle
		// We just need to delete the instances
		if (m_pxMaterial0)
		{
			delete m_pxMaterial0;
			m_pxMaterial0 = nullptr;
		}
		if (m_pxMaterial1)
		{
			delete m_pxMaterial1;
			m_pxMaterial1 = nullptr;
		}
	}

	// Decrement instance count - this may trigger streaming manager shutdown if last instance
	DecrementInstanceCount();
}

void Zenith_TerrainComponent::WriteToDataStream(Zenith_DataStream& xStream) const
{
	// Serialization version
	uint32_t uVersion = 2;
	xStream << uVersion;
	
	// NOTE: Terrain component uses hardcoded paths for physics geometry during construction
	// We serialize the source paths for reference, but reconstruction is handled by the constructor
	std::string strPhysicsGeometryPath = m_pxPhysicsGeometry ? m_pxPhysicsGeometry->m_strSourcePath : "";
	xStream << strPhysicsGeometryPath;

	// Version 2: Serialize full materials with texture paths
	if (m_pxMaterial0)
	{
		m_pxMaterial0->WriteToDataStream(xStream);
	}
	else
	{
		Flux_MaterialAsset* pxEmptyMat = Flux_MaterialAsset::Create("Empty");
		pxEmptyMat->WriteToDataStream(xStream);
		delete pxEmptyMat;
	}

	if (m_pxMaterial1)
	{
		m_pxMaterial1->WriteToDataStream(xStream);
	}
	else
	{
		Flux_MaterialAsset* pxEmptyMat = Flux_MaterialAsset::Create("Empty");
		pxEmptyMat->WriteToDataStream(xStream);
		delete pxEmptyMat;
	}
}

void Zenith_TerrainComponent::ReadFromDataStream(Zenith_DataStream& xStream)
{
	// Ensure streaming manager is initialized (may have been shut down after previous terrain was destroyed)
	if (!Flux_TerrainStreamingManager::IsInitialized())
	{
		Zenith_Log(LOG_CATEGORY_TERRAIN, "ReadFromDataStream - Re-initializing streaming manager (was shut down)");
		Flux_TerrainStreamingManager::Initialize();
	}

	// Read serialization version
	uint32_t uVersion;
	xStream >> uVersion;
	
	// Read physics geometry path (for reference, but we load all chunks below)
	std::string strPhysicsGeometryPath;
	xStream >> strPhysicsGeometryPath;

	// Load and combine ALL physics chunks, same as the full constructor
	// This is necessary for terrain colliders to cover the entire terrain, not just the first chunk
	if (m_pxPhysicsGeometry == nullptr)
	{
		Zenith_Log(LOG_CATEGORY_TERRAIN, "Terrain deserialization: Loading and combining all physics chunks...");

		// Load first physics chunk
		m_pxPhysicsGeometry = Zenith_AssetHandler::AddMeshFromFile(
			(std::string(Project_GetGameAssetsDirectory()) + "Terrain/Physics_0_0" ZENITH_MESH_EXT).c_str(),
			1 << Flux_MeshGeometry::FLUX_VERTEX_ATTRIBUTE__POSITION | 1 << Flux_MeshGeometry::FLUX_VERTEX_ATTRIBUTE__NORMAL);

		Flux_MeshGeometry& xPhysicsGeometry = *m_pxPhysicsGeometry;

		// Pre-allocate for all chunks (same calculation as constructor)
		const u_int64 ulTotalVertexDataSize = xPhysicsGeometry.GetVertexDataSize() * TOTAL_CHUNKS;
		const u_int64 ulTotalIndexDataSize = xPhysicsGeometry.GetIndexDataSize() * TOTAL_CHUNKS;
		const u_int64 ulTotalPositionDataSize = xPhysicsGeometry.GetNumVerts() * sizeof(Zenith_Maths::Vector3) * TOTAL_CHUNKS;

		xPhysicsGeometry.m_pVertexData = static_cast<u_int8*>(Zenith_MemoryManagement::Reallocate(xPhysicsGeometry.m_pVertexData, ulTotalVertexDataSize));
		xPhysicsGeometry.m_ulReservedVertexDataSize = ulTotalVertexDataSize;

		xPhysicsGeometry.m_puIndices = static_cast<Flux_MeshGeometry::IndexType*>(Zenith_MemoryManagement::Reallocate(xPhysicsGeometry.m_puIndices, ulTotalIndexDataSize));
		xPhysicsGeometry.m_ulReservedIndexDataSize = ulTotalIndexDataSize;

		xPhysicsGeometry.m_pxPositions = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Reallocate(xPhysicsGeometry.m_pxPositions, ulTotalPositionDataSize));
		xPhysicsGeometry.m_ulReservedPositionDataSize = ulTotalPositionDataSize;

		// Combine remaining physics chunks
		for (uint32_t x = 0; x < CHUNK_GRID_SIZE; x++)
		{
			for (uint32_t y = 0; y < CHUNK_GRID_SIZE; y++)
			{
				if (x == 0 && y == 0) continue;  // Already loaded

				std::string strPhysicsPath = std::string(Project_GetGameAssetsDirectory()) + "Terrain/Physics_" + std::to_string(x) + "_" + std::to_string(y) + ZENITH_MESH_EXT;
				Flux_MeshGeometry* pxTerrainPhysicsMesh = Zenith_AssetHandler::AddMeshFromFile(
					strPhysicsPath.c_str(),
					1 << Flux_MeshGeometry::FLUX_VERTEX_ATTRIBUTE__POSITION | 1 << Flux_MeshGeometry::FLUX_VERTEX_ATTRIBUTE__NORMAL);

				Flux_MeshGeometry::Combine(xPhysicsGeometry, *pxTerrainPhysicsMesh);

				Zenith_AssetHandler::DeleteMesh(pxTerrainPhysicsMesh);
			}
		}

		Zenith_Log(LOG_CATEGORY_TERRAIN, "Terrain deserialization: Physics mesh combined: %u vertices, %u indices",
			xPhysicsGeometry.GetNumVerts(), xPhysicsGeometry.GetNumIndices());
	}

	// Version 2+: Read full materials with texture paths
	if (uVersion >= 2)
	{
		// Create fresh materials with descriptive names including entity name
		std::string strEntityName = m_xParentEntity.GetName().empty() ?
			("Entity_" + std::to_string(m_xParentEntity.GetEntityID().m_uIndex)) : m_xParentEntity.GetName();
		m_pxMaterial0 = Flux_MaterialAsset::Create(strEntityName + "_Terrain_Mat0");
		m_pxMaterial1 = Flux_MaterialAsset::Create(strEntityName + "_Terrain_Mat1");

		// Mark that we own these materials and their textures need cleanup
		m_bOwnsMaterials = true;

		// Read material data (this will also load textures from paths)
		if (m_pxMaterial0)
		{
			m_pxMaterial0->ReadFromDataStream(xStream);
		}
		else
		{
			// Skip material data if we couldn't create material
			Flux_MaterialAsset* pxTempMat = Flux_MaterialAsset::Create("Temp");
			pxTempMat->ReadFromDataStream(xStream);
			delete pxTempMat;
		}

		if (m_pxMaterial1)
		{
			m_pxMaterial1->ReadFromDataStream(xStream);
		}
		else
		{
			Flux_MaterialAsset* pxTempMat = Flux_MaterialAsset::Create("Temp");
			pxTempMat->ReadFromDataStream(xStream);
			delete pxTempMat;
		}
	}
	else
	{
		// Version 1: Legacy format with only base colors
		Zenith_Maths::Vector4 xMat0Color, xMat1Color;
		xStream >> xMat0Color.x;
		xStream >> xMat0Color.y;
		xStream >> xMat0Color.z;
		xStream >> xMat0Color.w;
		xStream >> xMat1Color.x;
		xStream >> xMat1Color.y;
		xStream >> xMat1Color.z;
		xStream >> xMat1Color.w;

		// Apply loaded colors to materials if they exist
		if (m_pxMaterial0)
		{
			m_pxMaterial0->SetBaseColor(xMat0Color);
		}
		if (m_pxMaterial1)
		{
			m_pxMaterial1->SetBaseColor(xMat1Color);
		}
	}

	// CRITICAL: Initialize render resources (LOD3 meshes, buffers, culling)
	// This recreates the GPU resources that were destroyed when the old terrain component was deleted
	// Note: This takes several seconds to load and combine all LOD3 meshes, which is expected
	// when loading a scene
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Terrain deserialization: Initializing render resources...");

	// Use blank material as fallback if materials not provided
	// This ensures terrain is visible (though with default textures) rather than invisible
	// The game code can set proper materials after scene load if needed
	Flux_MaterialAsset* pxMat0 = m_pxMaterial0 ? m_pxMaterial0 : Flux_Graphics::s_pxBlankMaterial;
	Flux_MaterialAsset* pxMat1 = m_pxMaterial1 ? m_pxMaterial1 : Flux_Graphics::s_pxBlankMaterial;

	if (pxMat0 && pxMat1)
	{
		InitializeRenderResources(*pxMat0, *pxMat1);
		Zenith_Log(LOG_CATEGORY_TERRAIN, "Terrain deserialization: Render resources initialized successfully");
	}
	else
	{
		Zenith_Log(LOG_CATEGORY_TERRAIN, "ERROR: Terrain deserialization: Blank material not available!");
	}
}

// ========== Render Resources Initialization ==========

void Zenith_TerrainComponent::InitializeRenderResources(Flux_MaterialAsset& xMaterial0, Flux_MaterialAsset& xMaterial1)
{
	// Ensure streaming manager is initialized (may have been shut down after previous terrain was destroyed)
	if (!Flux_TerrainStreamingManager::IsInitialized())
	{
		Zenith_Log(LOG_CATEGORY_TERRAIN, "InitializeRenderResources - Re-initializing streaming manager (was shut down)");
		Flux_TerrainStreamingManager::Initialize();
	}

	// Store material references
	m_pxMaterial0 = &xMaterial0;
	m_pxMaterial1 = &xMaterial1;

	// Calculate expected LOD3 buffer requirements
	const float fLOD3Density = 0.25f;  // 64x64 -> 16x16 vertices per chunk for LOD3
	uint32_t uLOD3TotalVerts = 0;
	uint32_t uLOD3TotalIndices = 0;

	for (uint32_t z = 0; z < CHUNK_GRID_SIZE; ++z)
	{
		for (uint32_t x = 0; x < CHUNK_GRID_SIZE; ++x)
		{
			bool hasRightEdge = (x < CHUNK_GRID_SIZE - 1);
			bool hasTopEdge = (z < CHUNK_GRID_SIZE - 1);

			// Base vertices and indices per chunk (64x64 units)
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

	Zenith_Log(LOG_CATEGORY_TERRAIN, "LOD3 (always-resident) buffer requirements:");
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Vertices: %u (%.2f MB)", uLOD3TotalVerts, (uLOD3TotalVerts * TERRAIN_VERTEX_STRIDE) / (1024.0f * 1024.0f));
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Indices: %u (%.2f MB)", uLOD3TotalIndices, (uLOD3TotalIndices * 4.0f) / (1024.0f * 1024.0f));

	// ========== Load and Combine LOD3 Chunks ==========
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Loading LOD3 meshes for all %u chunks...", TOTAL_CHUNKS);

	// Load first chunk to get buffer layout (stored as owned pointer for later cleanup)
	Flux_MeshGeometry* pxLOD3Geometry = Zenith_AssetHandler::AddMeshFromFile(
		(std::string(Project_GetGameAssetsDirectory()) + "Terrain/Render_LOD3_0_0" ZENITH_MESH_EXT).c_str(), 0);  // 0 = load all attributes
	Flux_MeshGeometry& xLOD3Geometry = *pxLOD3Geometry;

	// Store vertex stride for buffer calculations
	m_uVertexStride = xLOD3Geometry.GetBufferLayout().GetStride();

	// Pre-allocate for all LOD3 chunks
	const uint64_t ulLOD3VertexDataSize = static_cast<uint64_t>(uLOD3TotalVerts) * m_uVertexStride;
	const uint64_t ulLOD3IndexDataSize = static_cast<uint64_t>(uLOD3TotalIndices) * sizeof(Flux_MeshGeometry::IndexType);
	const uint64_t ulLOD3PositionDataSize = static_cast<uint64_t>(uLOD3TotalVerts) * sizeof(Zenith_Maths::Vector3);

	xLOD3Geometry.m_pVertexData = static_cast<u_int8*>(Zenith_MemoryManagement::Reallocate(xLOD3Geometry.m_pVertexData, ulLOD3VertexDataSize));
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

			std::string strChunkPath = std::string(Project_GetGameAssetsDirectory()) + "Terrain/Render_LOD3_" + std::to_string(x) + "_" + std::to_string(y) + ZENITH_MESH_EXT;

			// Check if LOD3 file exists, fallback to LOD0 if not
			std::ifstream lodFile(strChunkPath);
			if (!lodFile.good())
			{
				Zenith_Log(LOG_CATEGORY_TERRAIN, "WARNING: LOD3 not found for chunk (%u,%u), using LOD0 as fallback", x, y);
				strChunkPath = std::string(Project_GetGameAssetsDirectory()) + "Terrain/Render_" + std::to_string(x) + "_" + std::to_string(y) + ZENITH_MESH_EXT;
			}

			Flux_MeshGeometry* pxChunkMesh = Zenith_AssetHandler::AddMeshFromFile(strChunkPath.c_str(), 0);  // 0 = all attributes
			Flux_MeshGeometry::Combine(xLOD3Geometry, *pxChunkMesh);
			Zenith_AssetHandler::DeleteMesh(pxChunkMesh);

			if ((x * CHUNK_GRID_SIZE + y) % 512 == 0)
			{
				Zenith_Log(LOG_CATEGORY_TERRAIN, "Combined LOD3 chunk (%u,%u)", x, y);
			}
		}
	}

	Zenith_Log(LOG_CATEGORY_TERRAIN, "LOD3 mesh combination complete: %u vertices, %u indices",
		xLOD3Geometry.GetNumVerts(), xLOD3Geometry.GetNumIndices());

	// Store LOD3 counts
	m_uLOD3VertexCount = xLOD3Geometry.GetNumVerts();
	m_uLOD3IndexCount = xLOD3Geometry.GetNumIndices();

	// ========== Initialize Unified Buffers (LOD3 + Streaming Space) ==========
	const uint64_t ulLOD3VertexSize = xLOD3Geometry.GetVertexDataSize();
	const uint64_t ulLOD3IndexSize = xLOD3Geometry.GetIndexDataSize();
	const uint64_t ulUnifiedVertexSize = ulLOD3VertexSize + STREAMING_VERTEX_BUFFER_SIZE;
	const uint64_t ulUnifiedIndexSize = ulLOD3IndexSize + STREAMING_INDEX_BUFFER_SIZE;

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Initializing unified terrain buffers (owned by TerrainComponent):");
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Vertex buffer: %.2f MB LOD3 + %llu MB streaming = %.2f MB total",
		ulLOD3VertexSize / (1024.0f * 1024.0f), STREAMING_VERTEX_BUFFER_SIZE_MB,
		ulUnifiedVertexSize / (1024.0f * 1024.0f));
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Index buffer: %.2f MB LOD3 + %llu MB streaming = %.2f MB total",
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

	// Upload unified buffers to GPU (component owns these)
	Flux_MemoryManager::InitialiseVertexBuffer(pUnifiedVertexData, ulUnifiedVertexSize, m_xUnifiedVertexBuffer);
	Flux_MemoryManager::InitialiseIndexBuffer(pUnifiedIndexData, ulUnifiedIndexSize, m_xUnifiedIndexBuffer);

	// Store buffer sizes
	m_ulUnifiedVertexBufferSize = ulUnifiedVertexSize;
	m_ulUnifiedIndexBufferSize = ulUnifiedIndexSize;

	delete[] pUnifiedVertexData;
	delete[] pUnifiedIndexData;

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Unified terrain buffers uploaded to GPU");
	Zenith_Log(LOG_CATEGORY_TERRAIN, "LOD3 region: vertices [0, %u), indices [0, %u)", m_uLOD3VertexCount, m_uLOD3IndexCount);
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Streaming region starts at: vertex %u, index %u", m_uLOD3VertexCount, m_uLOD3IndexCount);

	// Clean up LOD3 CPU data (no longer needed, data is in GPU buffer)
	Zenith_AssetHandler::DeleteMesh(pxLOD3Geometry);

	// ========== Register buffers with streaming manager ==========
	Flux_TerrainStreamingManager::RegisterTerrainBuffers(this);

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Terrain render geometry facade setup complete (references component-owned buffers)");

	// Initialize GPU culling resources for this terrain component
	// This allocates GPU buffers and builds chunk AABB + LOD metadata
	InitializeCullingResources();
}

// ========== GPU-Driven Culling Implementation ==========

void Zenith_TerrainComponent::InitializeCullingResources()
{
	if (m_bCullingResourcesInitialized)
	{
		Zenith_Assert(false, "Zenith_TerrainComponent::InitializeCullingResources() called when already initialized");
		return;
	}

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Zenith_TerrainComponent::InitializeCullingResources() - Setting up GPU-driven terrain culling with LOD support");

	// ========== CREATE GPU BUFFERS ==========

	// Frustum planes buffer (6 planes, updated per frame)
	Flux_MemoryManager::InitialiseDynamicConstantBuffer(
		nullptr,
		sizeof(Zenith_CameraDataGPU),
		m_xFrustumPlanesBuffer
	);

	// Indirect draw command buffer (one command per chunk, max)
	// Structure: VkDrawIndexedIndirectCommand (5 uint32_t values)
	// Initialize to zeros so all commands start with indexCount=0
	const size_t indirectBufferSize = sizeof(uint32_t) * 5 * TOTAL_CHUNKS;
	uint32_t* pZeroBuffer = new uint32_t[5 * TOTAL_CHUNKS];
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
		sizeof(uint32_t) * TOTAL_CHUNKS,
		m_xLODLevelBuffer
	);

	// Build chunk data (AABBs + LOD metadata) and upload to GPU
	BuildChunkData();

	m_bCullingResourcesInitialized = true;

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Zenith_TerrainComponent - Culling resources initialized with %u terrain chunks, %u LOD levels", TOTAL_CHUNKS, LOD_COUNT);
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Zenith_TerrainComponent - LOD distances: LOD0<%.1f, LOD1<%.1f, LOD2<%.1f, LOD3<inf",
		sqrtf(LOD_DISTANCE_SQ[0]), sqrtf(LOD_DISTANCE_SQ[1]), sqrtf(LOD_DISTANCE_SQ[2]));
}

void Zenith_TerrainComponent::DestroyCullingResources()
{
	if (!m_bCullingResourcesInitialized)
	{
		return;
	}

	// Cleanup GPU resources - queue for deferred deletion to avoid destroying in-use resources
	Flux_MemoryManager::DestroyReadWriteBuffer(m_xChunkDataBuffer);
	Flux_MemoryManager::DestroyDynamicConstantBuffer(m_xFrustumPlanesBuffer);
	Flux_MemoryManager::DestroyIndirectBuffer(m_xIndirectDrawBuffer);
	Flux_MemoryManager::DestroyIndirectBuffer(m_xVisibleCountBuffer);
	Flux_MemoryManager::DestroyReadWriteBuffer(m_xLODLevelBuffer);

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Zenith_TerrainComponent - Culling resources destroyed");

	m_bCullingResourcesInitialized = false;
}

void Zenith_TerrainComponent::BuildChunkData()
{
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Zenith_TerrainComponent::BuildChunkData() - Building chunk data using streaming manager");

	// Get chunk data from streaming manager (includes AABBs and current LOD allocations)
	Zenith_TerrainChunkData* pxChunkData = new Zenith_TerrainChunkData[TOTAL_CHUNKS];
	Flux_TerrainStreamingManager::BuildChunkDataForGPU(pxChunkData);

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Zenith_TerrainComponent - Chunk data retrieved from streaming manager for %u chunks", TOTAL_CHUNKS);

	// Upload chunk data to GPU
	Flux_MemoryManager::InitialiseReadWriteBuffer(
		pxChunkData,
		sizeof(Zenith_TerrainChunkData) * TOTAL_CHUNKS,
		m_xChunkDataBuffer
	);

	// Cleanup CPU data
	delete[] pxChunkData;

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Zenith_TerrainComponent - Chunk data with %u LOD levels uploaded to GPU", LOD_COUNT);
}

void Zenith_TerrainComponent::UpdateChunkLODAllocations()
{
	// CRITICAL: Skip if culling resources not initialized (e.g., terrain component added via editor but not yet set up)
	if (!m_bCullingResourcesInitialized)
	{
		return;  // Terrain not ready for rendering yet
	}

	// OPTIMIZATION: Skip GPU upload if chunk data hasn't changed
	if (!Flux_TerrainStreamingManager::IsChunkDataDirty())
	{
		return;  // No changes since last upload
	}
	
	// DEBUG: Log when we're updating chunk allocations
	static uint32_t s_uUpdateCount = 0;
	s_uUpdateCount++;

	// OPTIMIZATION: Use pre-allocated buffer from streaming manager to avoid heap allocation
	Zenith_TerrainChunkData* pxChunkData = Flux_TerrainStreamingManager::GetCachedChunkDataBuffer();
	bool bUsedCachedBuffer = (pxChunkData != nullptr);
	
	if (pxChunkData == nullptr)
	{
		// Fallback to allocation if cached buffer not available (shouldn't happen)
		pxChunkData = new Zenith_TerrainChunkData[TOTAL_CHUNKS];
	}
	
	Flux_TerrainStreamingManager::BuildChunkDataForGPU(pxChunkData);
	
	// CRITICAL: Use UploadBufferDataAtOffset which is synchronous (waits for GPU copy completion)
	// The regular UploadBufferData is deferred and would not be visible to the compute shader
	// that runs in the same frame
	Flux_MemoryManager::UploadBufferDataAtOffset(
		m_xChunkDataBuffer.GetBuffer().m_xVRAMHandle,
		pxChunkData,
		sizeof(Zenith_TerrainChunkData) * TOTAL_CHUNKS,
		0  // Offset 0 - replace entire buffer
	);
	
	if (!bUsedCachedBuffer)
	{
		delete[] pxChunkData;
	}
	
	// Clear dirty flag after successful upload
	Flux_TerrainStreamingManager::ClearChunkDataDirty();
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
		Zenith_Log(LOG_CATEGORY_TERRAIN, "ERROR: Zenith_TerrainComponent::UpdateCullingAndLod() called before InitializeCullingResources()");
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
	// We have TOTAL_CHUNKS chunks, with local_size_x=64 we need (TOTAL_CHUNKS + 63) / 64 workgroups
	uint32_t uNumWorkgroups = (TOTAL_CHUNKS + 63) / 64;
	xCmdList.AddCommand<Flux_CommandDispatch>(uNumWorkgroups, 1, 1);
}

#ifdef ZENITH_TOOLS
#include "imgui.h"
#include "Editor/Zenith_Editor.h"
#include <filesystem>

// Windows file dialog support
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#pragma comment(lib, "Comdlg32.lib")

// Terrain export functionality (extern declaration to avoid include path issues)
extern void ExportHeightmapFromPaths(const std::string& strHeightmapPath, const std::string& strMaterialPath, const std::string& strOutputDir);

// Static state for terrain creation UI
static char s_szHeightmapPath[512] = "";
static char s_szMaterialPath[512] = "";
static bool s_bTerrainExportInProgress = false;
static std::string s_strTerrainExportStatus = "";

// Helper function to show Windows Open File dialog for .tif files
static std::string ShowTifOpenFileDialog()
{
	char szFilePath[MAX_PATH] = { 0 };

	OPENFILENAMEA ofn = {};
	ofn.lStructSize = sizeof(OPENFILENAMEA);
	ofn.hwndOwner = nullptr;
	ofn.lpstrFilter = "TIF Files (*.tif)\0*.tif\0All Files (*.*)\0*.*\0";
	ofn.lpstrFile = szFilePath;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrDefExt = "tif";
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

	if (GetOpenFileNameA(&ofn))
	{
		return std::string(szFilePath);
	}
	return "";
}

// Helper to render a terrain material texture slot with drag-drop support
static void RenderTerrainTextureSlot(const char* szLabel, Flux_MaterialAsset& xMaterial, int iSlotType)
{
	ImGui::PushID(szLabel);

	// Get current texture path based on slot type
	std::string strCurrentPath;
	switch (iSlotType)
	{
	case 0: strCurrentPath = xMaterial.GetDiffuseTexturePath(); break;
	case 1: strCurrentPath = xMaterial.GetNormalTexturePath(); break;
	case 2: strCurrentPath = xMaterial.GetRoughnessMetallicTexturePath(); break;
	case 3: strCurrentPath = xMaterial.GetOcclusionTexturePath(); break;
	case 4: strCurrentPath = xMaterial.GetEmissiveTexturePath(); break;
	}

	std::string strTextureName = "(none)";
	if (!strCurrentPath.empty())
	{
		std::filesystem::path xPath(strCurrentPath);
		strTextureName = xPath.filename().string();
	}

	ImGui::Text("%s:", szLabel);
	ImGui::SameLine();

	// Drop zone button
	ImVec2 xButtonSize(150, 20);
	ImGui::Button(strTextureName.c_str(), xButtonSize);

	// Drag-drop target for texture files
	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload(DRAGDROP_PAYLOAD_TEXTURE))
		{
			const DragDropFilePayload* pFilePayload =
				static_cast<const DragDropFilePayload*>(pPayload->Data);

			std::string strFilePath(pFilePayload->m_szFilePath);

			// Set the texture path on the material
			switch (iSlotType)
			{
			case 0: xMaterial.SetDiffuseTexturePath(strFilePath); break;
			case 1: xMaterial.SetNormalTexturePath(strFilePath); break;
			case 2: xMaterial.SetRoughnessMetallicTexturePath(strFilePath); break;
			case 3: xMaterial.SetOcclusionTexturePath(strFilePath); break;
			case 4: xMaterial.SetEmissiveTexturePath(strFilePath); break;
			}

			Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Set %s texture: %s", szLabel, strFilePath.c_str());
		}
		ImGui::EndDragDropTarget();
	}

	// Tooltip
	if (ImGui::IsItemHovered())
	{
		if (!strCurrentPath.empty())
		{
			ImGui::SetTooltip("Drop a .ztxtr texture here\nPath: %s", strCurrentPath.c_str());
		}
		else
		{
			ImGui::SetTooltip("Drop a .ztxtr texture here");
		}
	}

	ImGui::PopID();
}

void Zenith_TerrainComponent::RenderPropertiesPanel()
{
	if (ImGui::CollapsingHeader("Terrain Component", ImGuiTreeNodeFlags_DefaultOpen))
	{
		// ========== Terrain Creation Section ==========
		// Only show if terrain is not yet initialized (no vertex buffer)
		bool bTerrainInitialized = m_ulUnifiedVertexBufferSize > 0;
		
		if (!bTerrainInitialized)
		{
			if (ImGui::TreeNode("Create Terrain From Heightmap"))
			{
				ImGui::TextWrapped("Specify heightmap and material interpolation textures to generate terrain geometry. Both textures should be 4096x4096 16-bit grayscale .tif files.");
				ImGui::Separator();

				// Heightmap path input
				ImGui::Text("Heightmap Texture:");
				ImGui::PushItemWidth(300);
				ImGui::InputText("##HeightmapPath", s_szHeightmapPath, sizeof(s_szHeightmapPath), ImGuiInputTextFlags_ReadOnly);
				ImGui::PopItemWidth();
				ImGui::SameLine();
				if (ImGui::Button("Browse...##Heightmap"))
				{
					std::string strPath = ShowTifOpenFileDialog();
					if (!strPath.empty())
					{
						strncpy(s_szHeightmapPath, strPath.c_str(), sizeof(s_szHeightmapPath) - 1);
						s_szHeightmapPath[sizeof(s_szHeightmapPath) - 1] = '\0';
						Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Selected heightmap: %s", s_szHeightmapPath);
					}
				}

				// Material path input
				ImGui::Text("Material Interpolation Texture:");
				ImGui::PushItemWidth(300);
				ImGui::InputText("##MaterialPath", s_szMaterialPath, sizeof(s_szMaterialPath), ImGuiInputTextFlags_ReadOnly);
				ImGui::PopItemWidth();
				ImGui::SameLine();
				if (ImGui::Button("Browse...##Material"))
				{
					std::string strPath = ShowTifOpenFileDialog();
					if (!strPath.empty())
					{
						strncpy(s_szMaterialPath, strPath.c_str(), sizeof(s_szMaterialPath) - 1);
						s_szMaterialPath[sizeof(s_szMaterialPath) - 1] = '\0';
						Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Selected material texture: %s", s_szMaterialPath);
					}
				}

				ImGui::Separator();

				// Output directory info
				std::string strOutputDir = std::string(Project_GetGameAssetsDirectory()) + "Terrain/";
				ImGui::Text("Output Directory: %s", strOutputDir.c_str());

				// Create terrain button
				bool bCanCreate = strlen(s_szHeightmapPath) > 0 && strlen(s_szMaterialPath) > 0 && !s_bTerrainExportInProgress;
				
				if (!bCanCreate)
					ImGui::BeginDisabled();
				
				if (ImGui::Button("Create Terrain", ImVec2(200, 30)))
				{
					s_bTerrainExportInProgress = true;
					s_strTerrainExportStatus = "Exporting terrain meshes...";
					
					Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Starting terrain export...");
					Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent]   Heightmap: %s", s_szHeightmapPath);
					Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent]   Material: %s", s_szMaterialPath);
					Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent]   Output: %s", strOutputDir.c_str());
					
					// Perform the terrain export
					ExportHeightmapFromPaths(s_szHeightmapPath, s_szMaterialPath, strOutputDir);
					
					s_strTerrainExportStatus = "Export complete. Initializing terrain...";
					Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Export complete. Initializing terrain...");
					
					// Create blank materials for initial rendering
					std::string strEntityName = m_xParentEntity.GetName().empty() ?
						("Entity_" + std::to_string(m_xParentEntity.GetEntityID().m_uIndex)) : m_xParentEntity.GetName();
					
					m_pxMaterial0 = Flux_MaterialAsset::Create(strEntityName + "_Terrain_Mat0");
					m_pxMaterial1 = Flux_MaterialAsset::Create(strEntityName + "_Terrain_Mat1");
					m_bOwnsMaterials = true;
					
					// Load physics geometry (same as constructor/deserialization)
					if (m_pxPhysicsGeometry == nullptr)
					{
						Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Loading and combining all physics chunks...");
						
						// Load first physics chunk
						m_pxPhysicsGeometry = Zenith_AssetHandler::AddMeshFromFile(
							(strOutputDir + "Physics_0_0" ZENITH_MESH_EXT).c_str(),
							1 << Flux_MeshGeometry::FLUX_VERTEX_ATTRIBUTE__POSITION | 1 << Flux_MeshGeometry::FLUX_VERTEX_ATTRIBUTE__NORMAL);

						if (m_pxPhysicsGeometry)
						{
							Flux_MeshGeometry& xPhysicsGeometry = *m_pxPhysicsGeometry;

							// Pre-allocate for all chunks
							const u_int64 ulTotalVertexDataSize = xPhysicsGeometry.GetVertexDataSize() * TOTAL_CHUNKS;
							const u_int64 ulTotalIndexDataSize = xPhysicsGeometry.GetIndexDataSize() * TOTAL_CHUNKS;
							const u_int64 ulTotalPositionDataSize = xPhysicsGeometry.GetNumVerts() * sizeof(Zenith_Maths::Vector3) * TOTAL_CHUNKS;

							xPhysicsGeometry.m_pVertexData = static_cast<u_int8*>(Zenith_MemoryManagement::Reallocate(xPhysicsGeometry.m_pVertexData, ulTotalVertexDataSize));
							xPhysicsGeometry.m_ulReservedVertexDataSize = ulTotalVertexDataSize;

							xPhysicsGeometry.m_puIndices = static_cast<Flux_MeshGeometry::IndexType*>(Zenith_MemoryManagement::Reallocate(xPhysicsGeometry.m_puIndices, ulTotalIndexDataSize));
							xPhysicsGeometry.m_ulReservedIndexDataSize = ulTotalIndexDataSize;

							xPhysicsGeometry.m_pxPositions = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Reallocate(xPhysicsGeometry.m_pxPositions, ulTotalPositionDataSize));
							xPhysicsGeometry.m_ulReservedPositionDataSize = ulTotalPositionDataSize;

							// Combine remaining physics chunks
							for (uint32_t x = 0; x < CHUNK_GRID_SIZE; x++)
							{
								for (uint32_t y = 0; y < CHUNK_GRID_SIZE; y++)
								{
									if (x == 0 && y == 0) continue;

									std::string strPhysicsPath = strOutputDir + "Physics_" + std::to_string(x) + "_" + std::to_string(y) + ZENITH_MESH_EXT;
									Flux_MeshGeometry* pxTerrainPhysicsMesh = Zenith_AssetHandler::AddMeshFromFile(
										strPhysicsPath.c_str(),
										1 << Flux_MeshGeometry::FLUX_VERTEX_ATTRIBUTE__POSITION | 1 << Flux_MeshGeometry::FLUX_VERTEX_ATTRIBUTE__NORMAL);

									if (pxTerrainPhysicsMesh)
									{
										Flux_MeshGeometry::Combine(xPhysicsGeometry, *pxTerrainPhysicsMesh);
										Zenith_AssetHandler::DeleteMesh(pxTerrainPhysicsMesh);
									}
								}
							}

							Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Physics mesh combined: %u vertices, %u indices",
								xPhysicsGeometry.GetNumVerts(), xPhysicsGeometry.GetNumIndices());
						}
					}
					
					// Initialize render resources (LOD3 meshes, buffers, culling)
					InitializeRenderResources(*m_pxMaterial0, *m_pxMaterial1);
					
					s_bTerrainExportInProgress = false;
					s_strTerrainExportStatus = "Terrain created successfully!";
					Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Terrain creation complete!");
				}
				
				if (!bCanCreate)
					ImGui::EndDisabled();

				// Status display
				if (!s_strTerrainExportStatus.empty())
				{
					ImGui::Separator();
					if (s_bTerrainExportInProgress)
					{
						ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%s", s_strTerrainExportStatus.c_str());
					}
					else if (s_strTerrainExportStatus.find("success") != std::string::npos || 
					         s_strTerrainExportStatus.find("complete") != std::string::npos)
					{
						ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%s", s_strTerrainExportStatus.c_str());
					}
					else
					{
						ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%s", s_strTerrainExportStatus.c_str());
					}
				}

				ImGui::TreePop();
			}
			
			ImGui::Separator();
		}

		// Terrain statistics
		if (ImGui::TreeNode("Statistics"))
		{
			ImGui::Text("Chunks: %d x %d", CHUNK_GRID_SIZE, CHUNK_GRID_SIZE);
			ImGui::Text("Total Chunks: %d", TOTAL_CHUNKS);
			ImGui::Text("LOD Count: %d", LOD_COUNT);
			ImGui::Text("Vertex Buffer Size: %.2f MB", m_ulUnifiedVertexBufferSize / (1024.0f * 1024.0f));
			ImGui::Text("Index Buffer Size: %.2f MB", m_ulUnifiedIndexBufferSize / (1024.0f * 1024.0f));
			ImGui::Text("LOD3 Vertices: %u", m_uLOD3VertexCount);
			ImGui::Text("LOD3 Indices: %u", m_uLOD3IndexCount);
			bool bTemp = m_bCullingResourcesInitialized;
			ImGui::Checkbox("Culling Resources Initialized", &bTemp);
			ImGui::TreePop();
		}

		ImGui::Separator();

		// Material 0 editing
		if (m_pxMaterial0)
		{
			if (ImGui::TreeNode("Material 0 (Base)"))
			{
				ImGui::Text("Name: %s", m_pxMaterial0->GetName().c_str());

				// Base color
				Zenith_Maths::Vector4 xBaseColor = m_pxMaterial0->GetBaseColor();
				float fColor[4] = { xBaseColor.x, xBaseColor.y, xBaseColor.z, xBaseColor.w };
				if (ImGui::ColorEdit4("Base Color##Mat0", fColor))
				{
					m_pxMaterial0->SetBaseColor({ fColor[0], fColor[1], fColor[2], fColor[3] });
				}

				// Material properties
				float fMetallic = m_pxMaterial0->GetMetallic();
				if (ImGui::SliderFloat("Metallic##Mat0", &fMetallic, 0.0f, 1.0f))
				{
					m_pxMaterial0->SetMetallic(fMetallic);
				}

				float fRoughness = m_pxMaterial0->GetRoughness();
				if (ImGui::SliderFloat("Roughness##Mat0", &fRoughness, 0.0f, 1.0f))
				{
					m_pxMaterial0->SetRoughness(fRoughness);
				}

				// Texture slots
				ImGui::Text("Textures:");
				ImGui::PushID("Mat0Textures");
				RenderTerrainTextureSlot("Diffuse", *m_pxMaterial0, 0);
				RenderTerrainTextureSlot("Normal", *m_pxMaterial0, 1);
				RenderTerrainTextureSlot("Roughness/Metallic", *m_pxMaterial0, 2);
				RenderTerrainTextureSlot("Occlusion", *m_pxMaterial0, 3);
				RenderTerrainTextureSlot("Emissive", *m_pxMaterial0, 4);
				ImGui::PopID();

				ImGui::TreePop();
			}
		}
		else
		{
			ImGui::TextDisabled("Material 0: (not set)");
		}

		// Material 1 editing
		if (m_pxMaterial1)
		{
			if (ImGui::TreeNode("Material 1 (Blend)"))
			{
				ImGui::Text("Name: %s", m_pxMaterial1->GetName().c_str());

				// Base color
				Zenith_Maths::Vector4 xBaseColor = m_pxMaterial1->GetBaseColor();
				float fColor[4] = { xBaseColor.x, xBaseColor.y, xBaseColor.z, xBaseColor.w };
				if (ImGui::ColorEdit4("Base Color##Mat1", fColor))
				{
					m_pxMaterial1->SetBaseColor({ fColor[0], fColor[1], fColor[2], fColor[3] });
				}

				// Material properties
				float fMetallic = m_pxMaterial1->GetMetallic();
				if (ImGui::SliderFloat("Metallic##Mat1", &fMetallic, 0.0f, 1.0f))
				{
					m_pxMaterial1->SetMetallic(fMetallic);
				}

				float fRoughness = m_pxMaterial1->GetRoughness();
				if (ImGui::SliderFloat("Roughness##Mat1", &fRoughness, 0.0f, 1.0f))
				{
					m_pxMaterial1->SetRoughness(fRoughness);
				}

				// Texture slots
				ImGui::Text("Textures:");
				ImGui::PushID("Mat1Textures");
				RenderTerrainTextureSlot("Diffuse", *m_pxMaterial1, 0);
				RenderTerrainTextureSlot("Normal", *m_pxMaterial1, 1);
				RenderTerrainTextureSlot("Roughness/Metallic", *m_pxMaterial1, 2);
				RenderTerrainTextureSlot("Occlusion", *m_pxMaterial1, 3);
				RenderTerrainTextureSlot("Emissive", *m_pxMaterial1, 4);
				ImGui::PopID();

				ImGui::TreePop();
			}
		}
		else
		{
			ImGui::TextDisabled("Material 1: (not set)");
		}
	}
}
#endif