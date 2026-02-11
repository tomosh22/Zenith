#include "Zenith.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Zenith_ComponentMeta.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Profiling/Zenith_Profiling.h"
#include "DataStream/Zenith_DataStream.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/Terrain/Flux_TerrainStreamingManager.h"
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

Zenith_TerrainComponent::Zenith_TerrainComponent(Zenith_MaterialAsset& xMaterial0, Zenith_MaterialAsset& xMaterial1, Zenith_Entity& xEntity)
	: m_bCullingResourcesInitialized(false)
	, m_xParentEntity(xEntity)
	, m_ulUnifiedVertexBufferSize(0)
	, m_ulUnifiedIndexBufferSize(0)
	, m_uVertexStride(0)
	, m_uLowLODVertexCount(0)
	, m_uLowLODIndexCount(0)
{
	IncrementInstanceCount();

	// Store material handles (auto ref-counting)
	m_xMaterial0.Set(&xMaterial0);
	m_xMaterial1.Set(&xMaterial1);

	// Ensure streaming manager is initialized (may have been shut down after previous terrain was destroyed)
	if (!Flux_TerrainStreamingManager::IsInitialized())
	{
		Zenith_Log(LOG_CATEGORY_TERRAIN, "Zenith_TerrainComponent - Re-initializing streaming manager (was shut down)");
		Flux_TerrainStreamingManager::Initialize();
	}

#pragma region Render
{
	// Initialize render resources (LOW LOD meshes, unified buffers, culling)
	// This is the same initialization performed during deserialization
	InitializeRenderResources(xMaterial0, xMaterial1);

	// NOTE: The following old code has been extracted into InitializeRenderResources()
	// to allow reuse during deserialization
	/*
	const float fLowLODDensity = 0.125f;

	uint32_t uLowLODTotalVerts = 0;
	uint32_t uLowLODTotalIndices = 0;

	for (uint32_t z = 0; z < CHUNK_GRID_SIZE; ++z)
	{
		for (uint32_t x = 0; x < CHUNK_GRID_SIZE; ++x)
		{
			bool hasRightEdge = (x < CHUNK_GRID_SIZE - 1);
			bool hasTopEdge = (z < CHUNK_GRID_SIZE - 1);

			// Base vertices and indices per chunk (64x64 units)
			uint32_t verts = (uint32_t)((CHUNK_SIZE_WORLD * fLowLODDensity + 1) * (CHUNK_SIZE_WORLD * fLowLODDensity + 1));
			uint32_t indices = (uint32_t)((CHUNK_SIZE_WORLD * fLowLODDensity) * (CHUNK_SIZE_WORLD * fLowLODDensity) * 6);

			// Edge stitching
			if (hasRightEdge)
			{
				verts += (uint32_t)(CHUNK_SIZE_WORLD * fLowLODDensity);
				indices += (uint32_t)((CHUNK_SIZE_WORLD * fLowLODDensity - 1) * 6);
			}
			if (hasTopEdge)
			{
				verts += (uint32_t)(CHUNK_SIZE_WORLD * fLowLODDensity);
				indices += (uint32_t)((CHUNK_SIZE_WORLD * fLowLODDensity - 1) * 6);
			}
			if (hasRightEdge && hasTopEdge)
			{
				verts += 1;
				indices += 6;
			}

			uLowLODTotalVerts += verts;
			uLowLODTotalIndices += indices;
		}
	}

	Zenith_Log(LOG_CATEGORY_TERRAIN, "LOW LOD (always-resident) buffer requirements:");
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Vertices: %u (%.2f MB)", uLowLODTotalVerts, (uLowLODTotalVerts * TERRAIN_VERTEX_STRIDE) / (1024.0f * 1024.0f));
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Indices: %u (%.2f MB)", uLowLODTotalIndices, (uLowLODTotalIndices * 4.0f) / (1024.0f * 1024.0f));

	// ========== Load and Combine LOW LOD Chunks ==========
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Loading LOW LOD meshes for all %u chunks...", TOTAL_CHUNKS);

	// Load first chunk to get buffer layout (stored as owned pointer for later cleanup)
	Flux_MeshGeometry* pxLowLODGeometry = Zenith_AssetHandler::AddMeshFromFile(
		(GetGameAssetsDir() + "Terrain/Render_LOW_0_0" ZENITH_MESH_EXT).c_str(), 0);  // 0 = load all attributes
	Flux_MeshGeometry& xLowLODGeometry = *pxLowLODGeometry;

	// Store vertex stride for buffer calculations
	m_uVertexStride = xLowLODGeometry.GetBufferLayout().GetStride();

	// Pre-allocate for all LOW LOD chunks
	const uint64_t ulLowLODVertexDataSize = static_cast<uint64_t>(uLowLODTotalVerts) * m_uVertexStride;
	const uint64_t ulLowLODIndexDataSize = static_cast<uint64_t>(uLowLODTotalIndices) * sizeof(Flux_MeshGeometry::IndexType);
	const uint64_t ulLowLODPositionDataSize = static_cast<uint64_t>(uLowLODTotalVerts) * sizeof(Zenith_Maths::Vector3);

	xLowLODGeometry.m_pVertexData = static_cast<u_int8*>(Zenith_MemoryManagement::Reallocate(xLowLODGeometry.m_pVertexData, ulLowLODVertexDataSize));
	xLowLODGeometry.m_ulReservedVertexDataSize = ulLowLODVertexDataSize;

	xLowLODGeometry.m_puIndices = static_cast<Flux_MeshGeometry::IndexType*>(Zenith_MemoryManagement::Reallocate(xLowLODGeometry.m_puIndices, ulLowLODIndexDataSize));
	xLowLODGeometry.m_ulReservedIndexDataSize = ulLowLODIndexDataSize;

	if (xLowLODGeometry.m_pxPositions)
	{
		xLowLODGeometry.m_pxPositions = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Reallocate(xLowLODGeometry.m_pxPositions, ulLowLODPositionDataSize));
		xLowLODGeometry.m_ulReservedPositionDataSize = ulLowLODPositionDataSize;
	}

	// Combine all LOW LOD chunks
	for (uint32_t x = 0; x < CHUNK_GRID_SIZE; ++x)
	{
		for (uint32_t y = 0; y < CHUNK_GRID_SIZE; ++y)
		{
			if (x == 0 && y == 0)
				continue;  // Already loaded

			std::string strChunkPath = GetGameAssetsDir() + "Terrain/Render_LOW_" + std::to_string(x) + "_" + std::to_string(y) + ZENITH_MESH_EXT;

			// Check if LOW LOD file exists, fallback to LOD0 if not
			std::ifstream lodFile(strChunkPath);
			if (!lodFile.good())
			{
				Zenith_Log(LOG_CATEGORY_TERRAIN, "WARNING: LOW LOD not found for chunk (%u,%u), using HIGH as fallback", x, y);
				strChunkPath = GetGameAssetsDir() + "Terrain/Render_" + std::to_string(x) + "_" + std::to_string(y) + ZENITH_MESH_EXT;
			}

			Flux_MeshGeometry* pxChunkMesh = Zenith_AssetHandler::AddMeshFromFile(strChunkPath.c_str(), 0);  // 0 = all attributes
			Flux_MeshGeometry::Combine(xLowLODGeometry, *pxChunkMesh);
			Zenith_AssetHandler::DeleteMesh(pxChunkMesh);

			if ((x * CHUNK_GRID_SIZE + y) % 512 == 0)
			{
				Zenith_Log(LOG_CATEGORY_TERRAIN, "Combined LOW LOD chunk (%u,%u)", x, y);
			}
		}
	}

	Zenith_Log(LOG_CATEGORY_TERRAIN, "LOW LOD mesh combination complete: %u vertices, %u indices",
		xLowLODGeometry.GetNumVerts(), xLowLODGeometry.GetNumIndices());

	// Store LOW LOD counts
	m_uLowLODVertexCount = xLowLODGeometry.GetNumVerts();
	m_uLowLODIndexCount = xLowLODGeometry.GetNumIndices();

	// ========== Initialize Unified Buffers (LOW LOD + Streaming Space) ==========
	const uint64_t ulLowLODVertexSize = xLowLODGeometry.GetVertexDataSize();
	const uint64_t ulLowLODIndexSize = xLowLODGeometry.GetIndexDataSize();
	const uint64_t ulUnifiedVertexSize = ulLowLODVertexSize + STREAMING_VERTEX_BUFFER_SIZE;
	const uint64_t ulUnifiedIndexSize = ulLowLODIndexSize + STREAMING_INDEX_BUFFER_SIZE;

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Initializing unified terrain buffers (owned by TerrainComponent):");
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Vertex buffer: %.2f MB LOW LOD + %llu MB streaming = %.2f MB total",
		ulLowLODVertexSize / (1024.0f * 1024.0f), STREAMING_VERTEX_BUFFER_SIZE_MB,
		ulUnifiedVertexSize / (1024.0f * 1024.0f));
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Index buffer: %.2f MB LOW LOD + %llu MB streaming = %.2f MB total",
		ulLowLODIndexSize / (1024.0f * 1024.0f), STREAMING_INDEX_BUFFER_SIZE_MB,
		ulUnifiedIndexSize / (1024.0f * 1024.0f));

	// Allocate unified buffers with LOW LOD data at the beginning
	uint8_t* pUnifiedVertexData = new uint8_t[ulUnifiedVertexSize];
	uint32_t* pUnifiedIndexData = new uint32_t[ulUnifiedIndexSize / sizeof(uint32_t)];

	// Copy LOW LOD data to beginning
	memcpy(pUnifiedVertexData, xLowLODGeometry.m_pVertexData, ulLowLODVertexSize);
	memcpy(pUnifiedIndexData, xLowLODGeometry.m_puIndices, ulLowLODIndexSize);

	// Zero out streaming region
	memset(pUnifiedVertexData + ulLowLODVertexSize, 0, STREAMING_VERTEX_BUFFER_SIZE);
	memset(pUnifiedIndexData + (ulLowLODIndexSize / sizeof(uint32_t)), 0, STREAMING_INDEX_BUFFER_SIZE);

	// Upload unified buffers to GPU (component owns these)
	Flux_MemoryManager::InitialiseVertexBuffer(pUnifiedVertexData, ulUnifiedVertexSize, m_xUnifiedVertexBuffer);
	Flux_MemoryManager::InitialiseIndexBuffer(pUnifiedIndexData, ulUnifiedIndexSize, m_xUnifiedIndexBuffer);

	// Store buffer sizes
	m_ulUnifiedVertexBufferSize = ulUnifiedVertexSize;
	m_ulUnifiedIndexBufferSize = ulUnifiedIndexSize;

	delete[] pUnifiedVertexData;
	delete[] pUnifiedIndexData;

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Unified terrain buffers uploaded to GPU");
	Zenith_Log(LOG_CATEGORY_TERRAIN, "LOW LOD region: vertices [0, %u), indices [0, %u)", m_uLowLODVertexCount, m_uLowLODIndexCount);
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Streaming region starts at: vertex %u, index %u", m_uLowLODVertexCount, m_uLowLODIndexCount);

	// Clean up LOW LOD CPU data (no longer needed, data is in GPU buffer)
	delete pxLowLODGeometry;
	pxLowLODGeometry = nullptr;

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
	LoadCombinedPhysicsGeometry();
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

	delete m_pxPhysicsGeometry;
	m_pxPhysicsGeometry = nullptr;

	// MaterialHandle members (m_xMaterial0, m_xMaterial1) auto-release when destroyed

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
	Zenith_MaterialAsset* pxMat0 = m_xMaterial0.Get();
	if (pxMat0)
	{
		pxMat0->WriteToDataStream(xStream);
	}
	else
	{
		Zenith_MaterialAsset xEmptyMat;
		xEmptyMat.WriteToDataStream(xStream);
	}

	Zenith_MaterialAsset* pxMat1 = m_xMaterial1.Get();
	if (pxMat1)
	{
		pxMat1->WriteToDataStream(xStream);
	}
	else
	{
		Zenith_MaterialAsset xEmptyMat;
		xEmptyMat.WriteToDataStream(xStream);
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
	LoadCombinedPhysicsGeometry();

	// Version 2+: Read full materials with texture paths
	if (uVersion >= 2)
	{
		// Create fresh materials with descriptive names including entity name
		std::string strEntityName = m_xParentEntity.GetName().empty() ?
			("Entity_" + std::to_string(m_xParentEntity.GetEntityID().m_uIndex)) : m_xParentEntity.GetName();

		// Create materials via registry and store in handles (handles manage ref counting)
		Zenith_MaterialAsset* pxNewMat0 = Zenith_AssetRegistry::Get().Create<Zenith_MaterialAsset>();
		Zenith_MaterialAsset* pxNewMat1 = Zenith_AssetRegistry::Get().Create<Zenith_MaterialAsset>();

		if (pxNewMat0)
		{
			pxNewMat0->SetName(strEntityName + "_Terrain_Mat0");
			m_xMaterial0.Set(pxNewMat0);
		}
		if (pxNewMat1)
		{
			pxNewMat1->SetName(strEntityName + "_Terrain_Mat1");
			m_xMaterial1.Set(pxNewMat1);
		}

		// Read material data (this will also load textures from paths)
		if (m_xMaterial0.Get())
		{
			m_xMaterial0.Get()->ReadFromDataStream(xStream);
		}
		else
		{
			// Skip material data if we couldn't create material
			Zenith_MaterialAsset xTempMat;
			xTempMat.ReadFromDataStream(xStream);
		}

		if (m_xMaterial1.Get())
		{
			m_xMaterial1.Get()->ReadFromDataStream(xStream);
		}
		else
		{
			Zenith_MaterialAsset xTempMat;
			xTempMat.ReadFromDataStream(xStream);
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
		if (m_xMaterial0.Get())
		{
			m_xMaterial0.Get()->SetBaseColor(xMat0Color);
		}
		if (m_xMaterial1.Get())
		{
			m_xMaterial1.Get()->SetBaseColor(xMat1Color);
		}
	}

	// CRITICAL: Initialize render resources (LOW LOD meshes, buffers, culling)
	// This recreates the GPU resources that were destroyed when the old terrain component was deleted
	// Note: This takes several seconds to load and combine all LOW LOD meshes, which is expected
	// when loading a scene
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Terrain deserialization: Initializing render resources...");

	// Use blank material as fallback if materials not provided
	// This ensures terrain is visible (though with default textures) rather than invisible
	// The game code can set proper materials after scene load if needed
	Zenith_MaterialAsset* pxMat0 = m_xMaterial0.Get() ? m_xMaterial0.Get() : Flux_Graphics::s_pxBlankMaterial;
	Zenith_MaterialAsset* pxMat1 = m_xMaterial1.Get() ? m_xMaterial1.Get() : Flux_Graphics::s_pxBlankMaterial;

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

void Zenith_TerrainComponent::InitializeRenderResources(Zenith_MaterialAsset&, Zenith_MaterialAsset&)
{
	// Ensure streaming manager is initialized (may have been shut down after previous terrain was destroyed)
	if (!Flux_TerrainStreamingManager::IsInitialized())
	{
		Zenith_Log(LOG_CATEGORY_TERRAIN, "InitializeRenderResources - Re-initializing streaming manager (was shut down)");
		Flux_TerrainStreamingManager::Initialize();
	}

	// NOTE: Materials are stored in m_xMaterial0/m_xMaterial1 handles by the caller
	// (constructor or ReadFromDataStream) before this method is invoked

	// Calculate expected LOW LOD buffer requirements
	const float fLowLODDensity = 0.25f;  // 64x64 -> 16x16 vertices per chunk for LOW LOD
	uint32_t uLowLODTotalVerts = 0;
	uint32_t uLowLODTotalIndices = 0;

	for (uint32_t z = 0; z < CHUNK_GRID_SIZE; ++z)
	{
		for (uint32_t x = 0; x < CHUNK_GRID_SIZE; ++x)
		{
			bool hasRightEdge = (x < CHUNK_GRID_SIZE - 1);
			bool hasTopEdge = (z < CHUNK_GRID_SIZE - 1);

			// Base vertices and indices per chunk (64x64 units)
			uint32_t verts = (uint32_t)((CHUNK_SIZE_WORLD * fLowLODDensity + 1) * (CHUNK_SIZE_WORLD * fLowLODDensity + 1));
			uint32_t indices = (uint32_t)((CHUNK_SIZE_WORLD * fLowLODDensity) * (CHUNK_SIZE_WORLD * fLowLODDensity) * 6);

			// Edge stitching
			if (hasRightEdge)
			{
				verts += (uint32_t)(CHUNK_SIZE_WORLD * fLowLODDensity);
				indices += (uint32_t)((CHUNK_SIZE_WORLD * fLowLODDensity - 1) * 6);
			}
			if (hasTopEdge)
			{
				verts += (uint32_t)(CHUNK_SIZE_WORLD * fLowLODDensity);
				indices += (uint32_t)((CHUNK_SIZE_WORLD * fLowLODDensity - 1) * 6);
			}
			if (hasRightEdge && hasTopEdge)
			{
				verts += 1;
				indices += 6;
			}

			uLowLODTotalVerts += verts;
			uLowLODTotalIndices += indices;
		}
	}

	Zenith_Log(LOG_CATEGORY_TERRAIN, "LOW LOD (always-resident) buffer requirements:");
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Vertices: %u (%.2f MB)", uLowLODTotalVerts, (uLowLODTotalVerts * TERRAIN_VERTEX_STRIDE) / (1024.0f * 1024.0f));
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Indices: %u (%.2f MB)", uLowLODTotalIndices, (uLowLODTotalIndices * 4.0f) / (1024.0f * 1024.0f));

	// ========== Load and Combine LOW LOD Chunks ==========
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Loading LOW LOD meshes for all %u chunks...", TOTAL_CHUNKS);

	// Load first chunk to get buffer layout (stored as owned pointer for later cleanup)
	Flux_MeshGeometry* pxLowLODGeometry = new Flux_MeshGeometry();
	Flux_MeshGeometry::LoadFromFile(
		(std::string(Project_GetGameAssetsDirectory()) + "Terrain/Render_LOW_0_0" ZENITH_MESH_EXT).c_str(), *pxLowLODGeometry, 0);  // 0 = load all attributes
	Flux_MeshGeometry& xLowLODGeometry = *pxLowLODGeometry;

	// Store vertex stride for buffer calculations
	m_uVertexStride = xLowLODGeometry.GetBufferLayout().GetStride();

	// Pre-allocate for all LOW LOD chunks
	const uint64_t ulLowLODVertexDataSize = static_cast<uint64_t>(uLowLODTotalVerts) * m_uVertexStride;
	const uint64_t ulLowLODIndexDataSize = static_cast<uint64_t>(uLowLODTotalIndices) * sizeof(Flux_MeshGeometry::IndexType);
	const uint64_t ulLowLODPositionDataSize = static_cast<uint64_t>(uLowLODTotalVerts) * sizeof(Zenith_Maths::Vector3);

	xLowLODGeometry.m_pVertexData = static_cast<u_int8*>(Zenith_MemoryManagement::Reallocate(xLowLODGeometry.m_pVertexData, ulLowLODVertexDataSize));
	xLowLODGeometry.m_ulReservedVertexDataSize = ulLowLODVertexDataSize;

	xLowLODGeometry.m_puIndices = static_cast<Flux_MeshGeometry::IndexType*>(Zenith_MemoryManagement::Reallocate(xLowLODGeometry.m_puIndices, ulLowLODIndexDataSize));
	xLowLODGeometry.m_ulReservedIndexDataSize = ulLowLODIndexDataSize;

	if (xLowLODGeometry.m_pxPositions)
	{
		xLowLODGeometry.m_pxPositions = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Reallocate(xLowLODGeometry.m_pxPositions, ulLowLODPositionDataSize));
		xLowLODGeometry.m_ulReservedPositionDataSize = ulLowLODPositionDataSize;
	}

	// Combine all LOW LOD chunks
	for (uint32_t x = 0; x < CHUNK_GRID_SIZE; ++x)
	{
		for (uint32_t y = 0; y < CHUNK_GRID_SIZE; ++y)
		{
			if (x == 0 && y == 0)
				continue;  // Already loaded

			std::string strChunkPath = std::string(Project_GetGameAssetsDirectory()) + "Terrain/Render_LOW_" + std::to_string(x) + "_" + std::to_string(y) + ZENITH_MESH_EXT;

			// Check if LOW LOD file exists, fallback to LOD0 if not
			std::ifstream lodFile(strChunkPath);
			if (!lodFile.good())
			{
				Zenith_Log(LOG_CATEGORY_TERRAIN, "WARNING: LOW LOD not found for chunk (%u,%u), using HIGH as fallback", x, y);
				strChunkPath = std::string(Project_GetGameAssetsDirectory()) + "Terrain/Render_" + std::to_string(x) + "_" + std::to_string(y) + ZENITH_MESH_EXT;
			}

			Flux_MeshGeometry xChunkMesh;
			Flux_MeshGeometry::LoadFromFile(strChunkPath.c_str(), xChunkMesh, 0);  // 0 = all attributes
			Flux_MeshGeometry::Combine(xLowLODGeometry, xChunkMesh);
			// xChunkMesh automatically destroyed when going out of scope

			if ((x * CHUNK_GRID_SIZE + y) % 512 == 0)
			{
				Zenith_Log(LOG_CATEGORY_TERRAIN, "Combined LOW LOD chunk (%u,%u)", x, y);
			}
		}
	}

	Zenith_Log(LOG_CATEGORY_TERRAIN, "LOW LOD mesh combination complete: %u vertices, %u indices",
		xLowLODGeometry.GetNumVerts(), xLowLODGeometry.GetNumIndices());

	// Store LOW LOD counts
	m_uLowLODVertexCount = xLowLODGeometry.GetNumVerts();
	m_uLowLODIndexCount = xLowLODGeometry.GetNumIndices();

	// ========== Initialize Unified Buffers (LOW LOD + Streaming Space) ==========
	const uint64_t ulLowLODVertexSize = xLowLODGeometry.GetVertexDataSize();
	const uint64_t ulLowLODIndexSize = xLowLODGeometry.GetIndexDataSize();
	const uint64_t ulUnifiedVertexSize = ulLowLODVertexSize + STREAMING_VERTEX_BUFFER_SIZE;
	const uint64_t ulUnifiedIndexSize = ulLowLODIndexSize + STREAMING_INDEX_BUFFER_SIZE;

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Initializing unified terrain buffers (owned by TerrainComponent):");
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Vertex buffer: %.2f MB LOW LOD + %llu MB streaming = %.2f MB total",
		ulLowLODVertexSize / (1024.0f * 1024.0f), STREAMING_VERTEX_BUFFER_SIZE_MB,
		ulUnifiedVertexSize / (1024.0f * 1024.0f));
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Index buffer: %.2f MB LOW LOD + %llu MB streaming = %.2f MB total",
		ulLowLODIndexSize / (1024.0f * 1024.0f), STREAMING_INDEX_BUFFER_SIZE_MB,
		ulUnifiedIndexSize / (1024.0f * 1024.0f));

	// Allocate unified buffers with LOW LOD data at the beginning
	uint8_t* pUnifiedVertexData = new uint8_t[ulUnifiedVertexSize];
	uint32_t* pUnifiedIndexData = new uint32_t[ulUnifiedIndexSize / sizeof(uint32_t)];

	// Copy LOW LOD data to beginning
	memcpy(pUnifiedVertexData, xLowLODGeometry.m_pVertexData, ulLowLODVertexSize);
	memcpy(pUnifiedIndexData, xLowLODGeometry.m_puIndices, ulLowLODIndexSize);

	// Zero out streaming region
	memset(pUnifiedVertexData + ulLowLODVertexSize, 0, STREAMING_VERTEX_BUFFER_SIZE);
	memset(pUnifiedIndexData + (ulLowLODIndexSize / sizeof(uint32_t)), 0, STREAMING_INDEX_BUFFER_SIZE);

	// Upload unified buffers to GPU (component owns these)
	Flux_MemoryManager::InitialiseVertexBuffer(pUnifiedVertexData, ulUnifiedVertexSize, m_xUnifiedVertexBuffer);
	Flux_MemoryManager::InitialiseIndexBuffer(pUnifiedIndexData, ulUnifiedIndexSize, m_xUnifiedIndexBuffer);

	// Store buffer sizes
	m_ulUnifiedVertexBufferSize = ulUnifiedVertexSize;
	m_ulUnifiedIndexBufferSize = ulUnifiedIndexSize;

	delete[] pUnifiedVertexData;
	delete[] pUnifiedIndexData;

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Unified terrain buffers uploaded to GPU");
	Zenith_Log(LOG_CATEGORY_TERRAIN, "LOW LOD region: vertices [0, %u), indices [0, %u)", m_uLowLODVertexCount, m_uLowLODIndexCount);
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Streaming region starts at: vertex %u, index %u", m_uLowLODVertexCount, m_uLowLODIndexCount);

	// Clean up LOW LOD CPU data (no longer needed, data is in GPU buffer)
	delete pxLowLODGeometry;
	pxLowLODGeometry = nullptr;

	// ========== Register buffers with streaming manager ==========
	Flux_TerrainStreamingManager::RegisterTerrainBuffers(this);

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Terrain render geometry facade setup complete (references component-owned buffers)");

	// Initialize GPU culling resources for this terrain component
	// This allocates GPU buffers and builds chunk AABB + LOD metadata
	InitializeCullingResources();
}

// ========== Physics Geometry Loading ==========

void Zenith_TerrainComponent::LoadCombinedPhysicsGeometry()
{
	if (m_pxPhysicsGeometry != nullptr)
	{
		return;  // Already loaded
	}

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Loading and combining all physics chunks...");

	// Load first physics chunk
	m_pxPhysicsGeometry = new Flux_MeshGeometry();
	Flux_MeshGeometry::LoadFromFile(
		(std::string(Project_GetGameAssetsDirectory()) + "Terrain/Physics_0_0" ZENITH_MESH_EXT).c_str(),
		*m_pxPhysicsGeometry,
		1 << Flux_MeshGeometry::FLUX_VERTEX_ATTRIBUTE__POSITION | 1 << Flux_MeshGeometry::FLUX_VERTEX_ATTRIBUTE__NORMAL);

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
			if (x == 0 && y == 0) continue;  // Already loaded

			std::string strPhysicsPath = std::string(Project_GetGameAssetsDirectory()) + "Terrain/Physics_" + std::to_string(x) + "_" + std::to_string(y) + ZENITH_MESH_EXT;
			Flux_MeshGeometry xTerrainPhysicsMesh;
			Flux_MeshGeometry::LoadFromFile(
				strPhysicsPath.c_str(),
				xTerrainPhysicsMesh,
				1 << Flux_MeshGeometry::FLUX_VERTEX_ATTRIBUTE__POSITION | 1 << Flux_MeshGeometry::FLUX_VERTEX_ATTRIBUTE__NORMAL);

			Flux_MeshGeometry::Combine(xPhysicsGeometry, xTerrainPhysicsMesh);
			// xTerrainPhysicsMesh automatically destroyed when going out of scope
		}
	}

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Physics mesh combined: %u vertices, %u indices",
		xPhysicsGeometry.GetNumVerts(), xPhysicsGeometry.GetNumIndices());
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
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Zenith_TerrainComponent - LOD distances: HIGH<%.1f, LOW=always",
		sqrtf(LOD_MAX_DISTANCE_SQ[LOD_HIGH]));
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
// Editor code for RenderPropertiesPanel is in Zenith_TerrainComponent_Editor.cpp// Editor code for RenderPropertiesPanel is in Zenith_TerrainComponent_Editor.cpp