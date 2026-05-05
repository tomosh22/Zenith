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
#include "Flux/RenderGraph/Flux_RenderGraph.h"
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
	}
	// Note: the previous "shut down the streaming manager when last terrain
	// is destroyed" side effect was removed when state ownership moved onto
	// the component. The manager no longer owns any per-terrain state, so
	// there is nothing to free here — each component frees its own state in
	// the destructor. The instance counter stays as a debug aid.
}

// Default constructor for deserialization. Out-of-line so the per-terrain
// Flux_TerrainStreamingState (forward-declared in the component header) can
// be allocated here with the full type visible from the manager header.
Zenith_TerrainComponent::Zenith_TerrainComponent(Zenith_Entity& xEntity)
	: m_xParentEntity(xEntity)
	, m_pxPhysicsGeometry(nullptr)
	, m_bCullingResourcesInitialized(false)
{
	IncrementInstanceCount();

	// Each terrain owns its own streaming state. Initialise() pre-allocates
	// the cached chunk-data scratch buffer and resets per-frame counters;
	// the allocators get sized later in RegisterTerrainBuffers (it has the
	// vertex stride needed to derive vertex-count budgets).
	m_pxStreamingState = new Flux_TerrainStreamingState();
	m_pxStreamingState->Initialize(this);
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

	// Allocate this terrain's own streaming state — same pattern as the
	// default (deserialization) constructor.
	m_pxStreamingState = new Flux_TerrainStreamingState();
	m_pxStreamingState->Initialize(this);

	// Store material handles (auto ref-counting)
	m_axMaterials[0].Set(&xMaterial0);
	m_axMaterials[1].Set(&xMaterial1);

	// Ensure all 4 material slots have valid assets (blank fallback for 2-3)
	for (u_int u = 2; u < TERRAIN_MATERIAL_COUNT; u++)
	{
		if (!m_axMaterials[u].GetDirect())
		{
			Zenith_MaterialAsset* pxBlank = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
			if (pxBlank)
			{
				pxBlank->SetName("Terrain_Mat" + std::to_string(u));
				m_axMaterials[u].Set(pxBlank);
			}
		}
	}

	// Ensure streaming manager is initialized — defensive, normally
	// Flux_Terrain::Initialise() does this once at engine startup.
	if (!Flux_TerrainStreamingManager::IsInitialized())
	{
		Flux_TerrainStreamingManager::Initialize();
	}

#pragma region Render
{
	// Initialize render resources (LOW LOD meshes, unified buffers, culling)
	// This is the same initialization performed during deserialization
	InitializeRenderResources();

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

	// Take this terrain out of the manager's registry FIRST so no concurrent
	// PreRenderUpdate iteration can pick up a state that's about to be freed.
	// Then tear down and free the per-terrain state.
	Flux_TerrainStreamingManager::UnregisterTerrainBuffers(this);

	if (m_pxStreamingState)
	{
		m_pxStreamingState->Shutdown();
		delete m_pxStreamingState;
		m_pxStreamingState = nullptr;
	}

	// Destroy owned unified buffers
	Flux_MemoryManager::DestroyVertexBuffer(m_xUnifiedVertexBuffer);
	Flux_MemoryManager::DestroyIndexBuffer(m_xUnifiedIndexBuffer);

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Zenith_TerrainComponent - Unified terrain buffers destroyed");

	delete m_pxPhysicsGeometry;
	m_pxPhysicsGeometry = nullptr;

	// MaterialHandle members (m_axMaterials[]) auto-release when destroyed

	DecrementInstanceCount();
}

void Zenith_TerrainComponent::WriteToDataStream(Zenith_DataStream& xStream) const
{
	// Serialization version
	uint32_t uVersion = 3;
	xStream << uVersion;

	// NOTE: Terrain component uses hardcoded paths for physics geometry during construction
	// We serialize the source paths for reference, but reconstruction is handled by the constructor
	std::string strPhysicsGeometryPath = m_pxPhysicsGeometry ? Zenith_AssetRegistry::NormalizeAssetPath(m_pxPhysicsGeometry->m_strSourcePath) : "";
	xStream << strPhysicsGeometryPath;

	// Version 3: Serialize 4 materials + splatmap path
	for (u_int u = 0; u < TERRAIN_MATERIAL_COUNT; u++)
	{
		Zenith_MaterialAsset* pxMat = m_axMaterials[u].GetDirect();
		if (pxMat)
		{
			pxMat->WriteToDataStream(xStream);
		}
		else
		{
			Zenith_MaterialAsset xEmptyMat;
			xEmptyMat.WriteToDataStream(xStream);
		}
	}

	// Splatmap path
	std::string strSplatmapPath = Zenith_AssetRegistry::NormalizeAssetPath(m_xSplatmap.GetPath());
	xStream << strSplatmapPath;
}

// ReadFromDataStream helpers — keep the top-level function focused on the
// version dispatch by factoring the per-version material-read passes out.

void Zenith_TerrainComponent::AssignTerrainMaterialSlot(u_int uSlot, const std::string& strEntityName, Zenith_DataStream& xStream)
{
	Zenith_Assert(uSlot < TERRAIN_MATERIAL_COUNT, "Terrain material slot %u out of range", uSlot);
	if (Zenith_MaterialAsset* pxNewMat = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>())
	{
		pxNewMat->SetName(strEntityName + "_Terrain_Mat" + std::to_string(uSlot));
		m_axMaterials[uSlot].Set(pxNewMat);
		pxNewMat->ReadFromDataStream(xStream);
	}
	else
	{
		// Registry couldn't allocate — still drain the stream so later data is aligned.
		Zenith_MaterialAsset xTempMat;
		xTempMat.ReadFromDataStream(xStream);
	}
}

void Zenith_TerrainComponent::ReadMaterialsV3(const std::string& strEntityName, Zenith_DataStream& xStream)
{
	for (u_int u = 0; u < TERRAIN_MATERIAL_COUNT; u++)
	{
		AssignTerrainMaterialSlot(u, strEntityName, xStream);
	}

	std::string strSplatmapPath;
	xStream >> strSplatmapPath;
	if (!strSplatmapPath.empty())
	{
		m_xSplatmap.SetPath(Zenith_AssetRegistry::NormalizeAssetPath(strSplatmapPath));
	}
}

void Zenith_TerrainComponent::ReadMaterialsV2(const std::string& strEntityName, Zenith_DataStream& xStream)
{
	for (u_int u = 0; u < 2; u++)
	{
		AssignTerrainMaterialSlot(u, strEntityName, xStream);
	}
}

void Zenith_TerrainComponent::ReadMaterialsV1Legacy(Zenith_DataStream& xStream)
{
	// Legacy format stored two base colors and no material assets.
	// operator>> returns void, so each component gets its own statement.
	Zenith_Maths::Vector4 xMat0Color, xMat1Color;
	xStream >> xMat0Color.x;
	xStream >> xMat0Color.y;
	xStream >> xMat0Color.z;
	xStream >> xMat0Color.w;
	xStream >> xMat1Color.x;
	xStream >> xMat1Color.y;
	xStream >> xMat1Color.z;
	xStream >> xMat1Color.w;

	if (m_axMaterials[0].GetDirect()) m_axMaterials[0].GetDirect()->SetBaseColor(xMat0Color);
	if (m_axMaterials[1].GetDirect()) m_axMaterials[1].GetDirect()->SetBaseColor(xMat1Color);
}

void Zenith_TerrainComponent::BackfillMissingMaterialSlots(const std::string& strEntityName)
{
	for (u_int u = 0; u < TERRAIN_MATERIAL_COUNT; u++)
	{
		if (!m_axMaterials[u].GetDirect())
		{
			if (Zenith_MaterialAsset* pxBlank = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>())
			{
				pxBlank->SetName(strEntityName + "_Terrain_Mat" + std::to_string(u));
				m_axMaterials[u].Set(pxBlank);
			}
		}
	}
}

void Zenith_TerrainComponent::ReadFromDataStream(Zenith_DataStream& xStream)
{
	// Streaming manager may have been shut down after the previous terrain
	// was destroyed — bring it back up before we touch chunks.
	if (!Flux_TerrainStreamingManager::IsInitialized())
	{
		Zenith_Log(LOG_CATEGORY_TERRAIN, "ReadFromDataStream - Re-initializing streaming manager (was shut down)");
		Flux_TerrainStreamingManager::Initialize();
	}

	uint32_t uVersion;
	xStream >> uVersion;

	std::string strPhysicsGeometryPath;
	xStream >> strPhysicsGeometryPath;  // Read-through; actual chunks loaded below.

	LoadCombinedPhysicsGeometry();

	const std::string strEntityName = m_xParentEntity.GetName().empty()
		? ("Entity_" + std::to_string(m_xParentEntity.GetEntityID().m_uIndex))
		: m_xParentEntity.GetName();

	if      (uVersion >= 3) ReadMaterialsV3(strEntityName, xStream);
	else if (uVersion >= 2) ReadMaterialsV2(strEntityName, xStream);
	else                    ReadMaterialsV1Legacy(xStream);

	BackfillMissingMaterialSlots(strEntityName);

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Terrain deserialization: Initializing render resources...");
	InitializeRenderResources();
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Terrain deserialization: Render resources initialized successfully");
}

// ========== Render Resources Initialization ==========

void Zenith_TerrainComponent::InitializeRenderResources()
{
	// Reset the unusable flag at the start of each load attempt so the
	// editor's regenerate/retry path can recover after the missing chunk
	// files are produced. Without this, a stale "unusable" set by a
	// previous failed load would short-circuit the retry even when the
	// new files load fine.
	m_bTerrainGeometryUnusable = false;

	// Ensure streaming manager is initialized (may have been shut down after previous terrain was destroyed)
	if (!Flux_TerrainStreamingManager::IsInitialized())
	{
		Zenith_Log(LOG_CATEGORY_TERRAIN, "InitializeRenderResources - Re-initializing streaming manager (was shut down)");
		Flux_TerrainStreamingManager::Initialize();
	}

	// NOTE: Materials are stored in m_axMaterials[] handles by the caller
	// (constructor or ReadFromDataStream) before this method is invoked

	uint32_t uLowLODTotalVerts = 0;
	uint32_t uLowLODTotalIndices = 0;
	CalculateLowLODBufferSizes(uLowLODTotalVerts, uLowLODTotalIndices);

	Zenith_Log(LOG_CATEGORY_TERRAIN, "LOW LOD (always-resident) buffer requirements:");
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Vertices: %u (%.2f MB)", uLowLODTotalVerts, (uLowLODTotalVerts * TERRAIN_VERTEX_STRIDE) / (1024.0f * 1024.0f));
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Indices: %u (%.2f MB)", uLowLODTotalIndices, (uLowLODTotalIndices * 4.0f) / (1024.0f * 1024.0f));

	// Pre-allocate per-chunk init data so the streaming manager doesn't re-read
	// every chunk file just to learn its vertex/index counts and AABB.
	Flux_TerrainChunkInitData* pxChunkInitData = new Flux_TerrainChunkInitData[TOTAL_CHUNKS];
	Flux_MeshGeometry* pxLowLODGeometry = nullptr;
	LoadAndCombineLowLODChunks(uLowLODTotalVerts, uLowLODTotalIndices, pxChunkInitData, pxLowLODGeometry);

	// LoadAndCombineLowLODChunks flips m_bTerrainGeometryUnusable when
	// chunk (0,0) is missing — without it we have no canonical vertex
	// layout to size the unified buffer with. Bail out cleanly so the
	// component lands in the "renders nothing, no physics body" state
	// instead of crashing partway through buffer creation.
	if (m_bTerrainGeometryUnusable)
	{
		delete pxLowLODGeometry;
		delete[] pxChunkInitData;
		Zenith_Error(LOG_CATEGORY_TERRAIN,
			"Zenith_TerrainComponent::InitializeRenderResources - skipping unified-buffer / streaming / culling init because terrain geometry is unusable");
		return;
	}

	InitializeUnifiedBuffers(*pxLowLODGeometry);

	delete pxLowLODGeometry;
	pxLowLODGeometry = nullptr;

	Flux_TerrainStreamingManager::RegisterTerrainBuffers(this, pxChunkInitData);
	delete[] pxChunkInitData;

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Terrain render geometry facade setup complete (references component-owned buffers)");

	InitializeCullingResources();
}

void Zenith_TerrainComponent::CalculateLowLODBufferSizes(uint32_t& uTotalVertsOut, uint32_t& uTotalIndicesOut) const
{
	const float fLowLODDensity = 0.25f;  // 64x64 -> 16x16 vertices per chunk for LOW LOD
	uTotalVertsOut = 0;
	uTotalIndicesOut = 0;

	for (uint32_t z = 0; z < CHUNK_GRID_SIZE; ++z)
	{
		for (uint32_t x = 0; x < CHUNK_GRID_SIZE; ++x)
		{
			const bool hasRightEdge = (x < CHUNK_GRID_SIZE - 1);
			const bool hasTopEdge = (z < CHUNK_GRID_SIZE - 1);

			uint32_t verts = (uint32_t)((CHUNK_SIZE_WORLD * fLowLODDensity + 1) * (CHUNK_SIZE_WORLD * fLowLODDensity + 1));
			uint32_t indices = (uint32_t)((CHUNK_SIZE_WORLD * fLowLODDensity) * (CHUNK_SIZE_WORLD * fLowLODDensity) * 6);

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

			uTotalVertsOut += verts;
			uTotalIndicesOut += indices;
		}
	}
}

void Zenith_TerrainComponent::LoadAndCombineLowLODChunks(uint32_t uTotalVerts, uint32_t uTotalIndices, Flux_TerrainChunkInitData* pxChunkInitData, Flux_MeshGeometry*& pxLowLODGeometryOut)
{
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Loading LOW LOD meshes for all %u chunks...", TOTAL_CHUNKS);

	// Load chunk (0,0) first — its buffer layout sets the global vertex
	// stride, so without it the rest of the geometry pipeline can't size
	// itself. Any failure here flips the unusable flag and the caller
	// (InitializeRenderResources) bails out before allocating the unified
	// buffer or registering with the streaming manager.
	pxLowLODGeometryOut = new Flux_MeshGeometry();
	Flux_MeshGeometry::LoadFromFile(
		(std::string(Project_GetGameAssetsDirectory()) + "Terrain/Render_LOW_0_0" ZENITH_MESH_EXT).c_str(), *pxLowLODGeometryOut,
		1 << Flux_MeshGeometry::FLUX_VERTEX_ATTRIBUTE__POSITION);
	Flux_MeshGeometry& xLowLODGeometry = *pxLowLODGeometryOut;

	if (xLowLODGeometry.GetNumVerts() == 0)
	{
		Zenith_Error(LOG_CATEGORY_TERRAIN,
			"Terrain LOW LOD chunk (0,0) failed to load (Render_LOW_0_0%s missing or empty). Marking terrain geometry unusable; this terrain will not render and will not produce a physics body.",
			ZENITH_MESH_EXT);
		m_bTerrainGeometryUnusable = true;
		// pxChunkInitData entries stay at their default-initialised zero
		// counts / default AABB. No further work to do — caller will bail.
		return;
	}

	m_uVertexStride = xLowLODGeometry.GetBufferLayout().GetStride();

	// Collect init data for chunk (0,0)
	{
		const uint32_t uChunkIndex = Flux_TerrainConfig::ChunkCoordsToIndex(0, 0);
		pxChunkInitData[uChunkIndex].m_uVertexCount = xLowLODGeometry.GetNumVerts();
		pxChunkInitData[uChunkIndex].m_uIndexCount = xLowLODGeometry.GetNumIndices();
		if (xLowLODGeometry.m_pxPositions)
		{
			pxChunkInitData[uChunkIndex].m_xAABB = Zenith_FrustumCulling::GenerateAABBFromVertices(
				xLowLODGeometry.m_pxPositions, xLowLODGeometry.GetNumVerts());
		}
	}

	const uint64_t ulLowLODVertexDataSize = static_cast<uint64_t>(uTotalVerts) * m_uVertexStride;
	const uint64_t ulLowLODIndexDataSize = static_cast<uint64_t>(uTotalIndices) * sizeof(Flux_MeshGeometry::IndexType);
	const uint64_t ulLowLODPositionDataSize = static_cast<uint64_t>(uTotalVerts) * sizeof(Zenith_Maths::Vector3);

	xLowLODGeometry.m_pVertexData = static_cast<u_int8*>(Zenith_MemoryManagement::Reallocate(xLowLODGeometry.m_pVertexData, ulLowLODVertexDataSize));
	xLowLODGeometry.m_ulReservedVertexDataSize = ulLowLODVertexDataSize;

	xLowLODGeometry.m_puIndices = static_cast<Flux_MeshGeometry::IndexType*>(Zenith_MemoryManagement::Reallocate(xLowLODGeometry.m_puIndices, ulLowLODIndexDataSize));
	xLowLODGeometry.m_ulReservedIndexDataSize = ulLowLODIndexDataSize;

	if (xLowLODGeometry.m_pxPositions)
	{
		xLowLODGeometry.m_pxPositions = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Reallocate(xLowLODGeometry.m_pxPositions, ulLowLODPositionDataSize));
		xLowLODGeometry.m_ulReservedPositionDataSize = ulLowLODPositionDataSize;
	}

	for (uint32_t x = 0; x < CHUNK_GRID_SIZE; ++x)
	{
		for (uint32_t y = 0; y < CHUNK_GRID_SIZE; ++y)
		{
			if (x == 0 && y == 0)
				continue;

			std::string strChunkPath = std::string(Project_GetGameAssetsDirectory()) + "Terrain/Render_LOW_" + std::to_string(x) + "_" + std::to_string(y) + ZENITH_MESH_EXT;

			std::ifstream lodFile(strChunkPath);
			if (!lodFile.good())
			{
				Zenith_Log(LOG_CATEGORY_TERRAIN, "WARNING: LOW LOD not found for chunk (%u,%u), using HIGH as fallback", x, y);
				strChunkPath = std::string(Project_GetGameAssetsDirectory()) + "Terrain/Render_" + std::to_string(x) + "_" + std::to_string(y) + ZENITH_MESH_EXT;
			}

			Flux_MeshGeometry xChunkMesh;
			Flux_MeshGeometry::LoadFromFile(strChunkPath.c_str(), xChunkMesh,
				1 << Flux_MeshGeometry::FLUX_VERTEX_ATTRIBUTE__POSITION);

			const uint32_t uChunkIndex = Flux_TerrainConfig::ChunkCoordsToIndex(x, y);

			// A failed load (file missing, parse error, empty mesh) leaves
			// xChunkMesh at zero verts. Log the warning and skip Combine —
			// the init data entry stays at its default-initialised zero
			// counts + default AABB so the GPU compute shader's
			// "indexCount == 0 → return" early-out renders the chunk as
			// nothing, and the streaming offset bookkeeping stays consistent
			// (this chunk simply contributes no vertices/indices to the
			// combined LOW LOD buffer).
			if (xChunkMesh.GetNumVerts() == 0)
			{
				Zenith_Warning(LOG_CATEGORY_TERRAIN,
					"Terrain LOW LOD chunk (%u,%u) source mesh missing or empty — chunk will render empty.", x, y);
				continue;
			}

			pxChunkInitData[uChunkIndex].m_uVertexCount = xChunkMesh.GetNumVerts();
			pxChunkInitData[uChunkIndex].m_uIndexCount = xChunkMesh.GetNumIndices();
			if (xChunkMesh.m_pxPositions)
			{
				pxChunkInitData[uChunkIndex].m_xAABB = Zenith_FrustumCulling::GenerateAABBFromVertices(
					xChunkMesh.m_pxPositions, xChunkMesh.GetNumVerts());
			}

			Flux_MeshGeometry::Combine(xLowLODGeometry, xChunkMesh);

			if ((x * CHUNK_GRID_SIZE + y) % 512 == 0)
			{
				Zenith_Log(LOG_CATEGORY_TERRAIN, "Combined LOW LOD chunk (%u,%u)", x, y);
			}
		}
	}

	Zenith_Log(LOG_CATEGORY_TERRAIN, "LOW LOD mesh combination complete: %u vertices, %u indices",
		xLowLODGeometry.GetNumVerts(), xLowLODGeometry.GetNumIndices());

	m_uLowLODVertexCount = xLowLODGeometry.GetNumVerts();
	m_uLowLODIndexCount = xLowLODGeometry.GetNumIndices();
}

void Zenith_TerrainComponent::InitializeUnifiedBuffers(const Flux_MeshGeometry& xLowLODGeometry)
{
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

	uint8_t* pUnifiedVertexData = new uint8_t[ulUnifiedVertexSize];
	uint32_t* pUnifiedIndexData = new uint32_t[ulUnifiedIndexSize / sizeof(uint32_t)];

	memcpy(pUnifiedVertexData, xLowLODGeometry.m_pVertexData, ulLowLODVertexSize);
	memcpy(pUnifiedIndexData, xLowLODGeometry.m_puIndices, ulLowLODIndexSize);

	memset(pUnifiedVertexData + ulLowLODVertexSize, 0, STREAMING_VERTEX_BUFFER_SIZE);
	memset(pUnifiedIndexData + (ulLowLODIndexSize / sizeof(uint32_t)), 0, STREAMING_INDEX_BUFFER_SIZE);

	Flux_MemoryManager::InitialiseVertexBuffer(pUnifiedVertexData, ulUnifiedVertexSize, m_xUnifiedVertexBuffer);
	Flux_MemoryManager::InitialiseIndexBuffer(pUnifiedIndexData, ulUnifiedIndexSize, m_xUnifiedIndexBuffer);

	m_ulUnifiedVertexBufferSize = ulUnifiedVertexSize;
	m_ulUnifiedIndexBufferSize = ulUnifiedIndexSize;

	delete[] pUnifiedVertexData;
	delete[] pUnifiedIndexData;

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Unified terrain buffers uploaded to GPU");
	Zenith_Log(LOG_CATEGORY_TERRAIN, "LOW LOD region: vertices [0, %u), indices [0, %u)", m_uLowLODVertexCount, m_uLowLODIndexCount);
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Streaming region starts at: vertex %u, index %u", m_uLowLODVertexCount, m_uLowLODIndexCount);
}

// ========== Physics Geometry Loading ==========

void Zenith_TerrainComponent::LoadCombinedPhysicsGeometry()
{
	if (m_pxPhysicsGeometry != nullptr)
	{
		return;  // Already loaded
	}

	// If render geometry is already known to be unusable, there's no
	// physics body to build either — without chunk (0,0) we don't know
	// the canonical mesh layout. Leave m_pxPhysicsGeometry null and let
	// callers gate on HasPhysicsGeometry().
	if (m_bTerrainGeometryUnusable)
	{
		Zenith_Warning(LOG_CATEGORY_TERRAIN,
			"Skipping physics geometry load — terrain geometry already marked unusable.");
		return;
	}

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Loading and combining all physics chunks...");

	// Load chunk (0,0) — its layout sizes the pre-allocated combined buffer.
	// If this load fails we can't usefully size or combine anything else;
	// drop the half-built geometry and bail.
	m_pxPhysicsGeometry = new Flux_MeshGeometry();
	Flux_MeshGeometry::LoadFromFile(
		(std::string(Project_GetGameAssetsDirectory()) + "Terrain/Physics_0_0" ZENITH_MESH_EXT).c_str(),
		*m_pxPhysicsGeometry,
		1 << Flux_MeshGeometry::FLUX_VERTEX_ATTRIBUTE__POSITION | 1 << Flux_MeshGeometry::FLUX_VERTEX_ATTRIBUTE__NORMAL);

	if (m_pxPhysicsGeometry->GetNumVerts() == 0)
	{
		Zenith_Error(LOG_CATEGORY_TERRAIN,
			"Terrain physics chunk (0,0) failed to load (Physics_0_0%s missing or empty). Terrain will have no physics body.",
			ZENITH_MESH_EXT);
		delete m_pxPhysicsGeometry;
		m_pxPhysicsGeometry = nullptr;
		return;
	}

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

	// Combine remaining physics chunks. A failed per-chunk load is non-fatal:
	// we skip it (don't Combine) so the resulting mesh is missing that
	// region but stays watertight elsewhere. Substituting degenerate
	// triangles for missing regions risks tripping Jolt's mesh validation.
	uint32_t uSkippedChunks = 0;
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

			if (xTerrainPhysicsMesh.GetNumVerts() == 0)
			{
				Zenith_Warning(LOG_CATEGORY_TERRAIN,
					"Terrain physics chunk (%u,%u) source mesh missing or empty — region skipped from combined physics body.", x, y);
				uSkippedChunks++;
				continue;
			}

			Flux_MeshGeometry::Combine(xPhysicsGeometry, xTerrainPhysicsMesh);
			// xTerrainPhysicsMesh automatically destroyed when going out of scope
		}
	}

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Physics mesh combined: %u vertices, %u indices (%u chunk(s) skipped)",
		xPhysicsGeometry.GetNumVerts(), xPhysicsGeometry.GetNumIndices(), uSkippedChunks);
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

	// LOD level buffer (one uint32_t per potential draw call). Zero-initialise
	// so the GPU LOD hysteresis (Flux_TerrainCulling.slang reads
	// LODLevelBuffer[chunkIndex] as priorLOD before writing) sees a
	// deterministic 0 (== HIGH) on the first frame instead of whatever
	// VRAM the allocator handed us. Garbage values can't trigger the
	// hysteresis branches (they require priorLOD == 0 or 1) but a zero-
	// init buffer makes the first-frame behaviour predictable.
	{
		uint32_t* pZero = new uint32_t[TOTAL_CHUNKS];
		memset(pZero, 0, sizeof(uint32_t) * TOTAL_CHUNKS);
		Flux_MemoryManager::InitialiseReadWriteBuffer(
			pZero,
			sizeof(uint32_t) * TOTAL_CHUNKS,
			m_xLODLevelBuffer
		);
		delete[] pZero;
	}

	// Build chunk data (AABBs + LOD metadata) and upload to GPU
	BuildChunkData();

	m_bCullingResourcesInitialized = true;

	// New per-terrain GPU buffers (chunk data, indirect, count, LOD level)
	// just appeared. Flux_Terrain::SetupRenderGraph reads
	// m_bCullingResourcesInitialized when declaring per-component buffer
	// dependencies, so the next graph compile must rebuild to pick them up.
	Flux::RequestGraphRebuild();

	// Note: m_xChunkDataBuffer no longer needs MarkBufferHostWritten here.
	// It's now a Flux_DynamicReadWriteBuffer (frame-indexed, host-visible),
	// and is intentionally not declared in the render graph for the same
	// reason m_xFrustumPlanesBuffer isn't — declaring a frame-indexed buffer
	// via GetBuffer() at compile time locks the graph to frame 0's instance.
	// vkSubmit's implicit host-write-available barrier covers visibility for
	// host-coherent buffers without a manual TransferWrite→ShaderRead
	// barrier, which only the staged-upload path required.

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

	// Drop the rebuild flag BEFORE queueing buffer destruction so the next
	// graph compile rebuilds SetupRenderGraph against the new (smaller) set
	// of live terrain components. The deferred-destroy path
	// (QueueVRAMDeletion + MAX_FRAMES_IN_FLIGHT+1 grace) keeps the buffers
	// alive long enough for any in-flight command lists to finish reading
	// them before the GPU side actually frees the memory.
	Flux::RequestGraphRebuild();

	// Cleanup GPU resources - queue for deferred deletion to avoid destroying in-use resources.
	// Chunk data buffer is frame-indexed; DestroyDynamicReadWriteBuffer queues
	// every frame slot for deferred deletion.
	Flux_MemoryManager::DestroyDynamicReadWriteBuffer(m_xChunkDataBuffer);
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

	// Get chunk data from streaming manager (includes AABBs and current LOD
	// allocations) — component-aware overload routes through this terrain's
	// own state, no cross-terrain pollution.
	Zenith_TerrainChunkData* pxChunkData = new Zenith_TerrainChunkData[TOTAL_CHUNKS];
	Flux_TerrainStreamingManager::BuildChunkDataForGPU(this, pxChunkData);

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Zenith_TerrainComponent - Chunk data retrieved from streaming manager for %u chunks", TOTAL_CHUNKS);

	// Allocate the per-frame buffers. InitialiseDynamicReadWriteBuffer also
	// performs the initial upload into every frame's slot, so the GPU has
	// valid chunk metadata in slot 0 by the time the first frame's compute
	// runs (the orphan-read validator in the render graph would otherwise
	// trip on a Read declaration with no preceding writer).
	Flux_MemoryManager::InitialiseDynamicReadWriteBuffer(
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

	// The per-component dirty-flag short-circuit used to live here:
	//
	//   if (!Flux_TerrainStreamingManager::IsChunkDataDirty(this)) return;
	//
	// It produced a steady-state "stretched-triangle to a previously-resident
	// chunk's slot" spike. Disabling the short-circuit and re-uploading every
	// frame eliminates the spike; a full audit of every m_axChunkResidency
	// mutation site failed to identify which state-change path was leaking
	// (StreamInLOD, EvictLOD, RegisterTerrainBuffers, RequestNearbyHighLOD,
	// EvictDistantHighLOD, RebuildActiveChunkSet, EvictToMakeSpace all set
	// m_bChunkDataDirty by code inspection). Rather than ship a flaky cache,
	// we always rebuild + upload — the chunk buffer is one TerrainChunkData
	// struct (64 bytes) per chunk × TOTAL_CHUNKS = 256 KB per terrain per
	// frame, which at 60 fps is ~15 MB/s of memory-queue bandwidth. That's
	// negligible against the streaming-region budget (256 MB vertex / 64 MB
	// index) and well below the staging buffer chunk size, so the upload
	// stays in a single staging slot.
	//
	// The dirty-flag setters in the streaming manager are kept (harmless
	// dead state) so that revisiting this optimisation later doesn't have
	// to re-thread the flag through every call site.

	// DEBUG: Log when we're updating chunk allocations
	static uint32_t s_uUpdateCount = 0;
	s_uUpdateCount++;

	// OPTIMIZATION: Use this terrain's pre-allocated cached buffer to avoid
	// per-frame heap allocation.
	Zenith_TerrainChunkData* pxChunkData = Flux_TerrainStreamingManager::GetCachedChunkDataBuffer(this);
	bool bUsedCachedBuffer = (pxChunkData != nullptr);

	if (pxChunkData == nullptr)
	{
		// Fallback to allocation if cached buffer not available (shouldn't happen)
		pxChunkData = new Zenith_TerrainChunkData[TOTAL_CHUNKS];
	}

	Flux_TerrainStreamingManager::BuildChunkDataForGPU(this, pxChunkData);

	// Upload to the CURRENT FRAME's chunk-data buffer slot. m_xChunkDataBuffer
	// is frame-indexed, so GetBuffer() resolves to the current ring-slot's
	// buffer; the previous frame slot's buffer is owned by the GPU work still
	// in flight on the other slot and must not be touched. The host-visible
	// memory residency means the write goes direct (no staging) and vkSubmit's
	// implicit host-write-available barrier carries the visibility into the
	// compute read on the render queue.
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

	// Clear THIS terrain's dirty flag after successful upload.
	Flux_TerrainStreamingManager::ClearChunkDataDirty(this);
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

void Zenith_TerrainComponent::UploadFrustumPlanesForFrame(const Zenith_Maths::Matrix4& xViewProjMatrix)
{
	if (!m_bCullingResourcesInitialized)
	{
		return;
	}

	Zenith_CameraDataGPU xCameraData;
	Zenith_Frustum xFrustum;
	xFrustum.ExtractFromViewProjection(xViewProjMatrix);

	for (int i = 0; i < 6; ++i)
	{
		xCameraData.m_axFrustumPlanes[i].m_xNormalAndDistance = Zenith_Maths::Vector4(
			xFrustum.m_axPlanes[i].m_xNormal,
			xFrustum.m_axPlanes[i].m_fDistance
		);
	}

	const Zenith_Maths::Vector3 xCameraPos = Flux_Graphics::GetCameraPosition();
	xCameraData.m_xCameraPosition = Zenith_Maths::Vector4(xCameraPos, 0.0f);

	Flux_MemoryManager::UploadBufferDataAtOffset(m_xFrustumPlanesBuffer.GetBuffer().m_xVRAMHandle, &xCameraData, sizeof(Zenith_CameraDataGPU), 0);
}

void Zenith_TerrainComponent::UpdateCullingAndLod(Flux_CommandList& xCmdList)
{
	if (!m_bCullingResourcesInitialized)
	{
		Zenith_Error(LOG_CATEGORY_TERRAIN, "Zenith_TerrainComponent::UpdateCullingAndLod() called before InitializeCullingResources()");
		return;
	}

	// IMPORTANT: Assumes the terrain culling compute pipeline is already bound by Flux_Terrain.
	// Frustum planes + camera position were uploaded in PreRenderUpdate; visible-count was
	// zeroed by the dedicated reset pass that this pass DependsOn. We only record bindings
	// and the dispatch here.

	// Bind descriptor set 0 with all buffers. ChunkBuffer is reflected as
	// StructuredBuffer<TerrainChunkData> (read-only) so it goes through the
	// SRV-buffer path. m_xChunkDataBuffer is frame-indexed, so GetSRV()
	// resolves to the current frame's SRV — the same slot CPU just wrote to
	// in PreRenderUpdate via UpdateChunkLODAllocations.
	xCmdList.AddCommand<Flux_CommandBeginBind>(0);
	xCmdList.AddCommand<Flux_CommandBindSRV_Buffer>(m_xChunkDataBuffer.GetSRV(), 0);            // Chunk data (read-only StructuredBuffer)
	xCmdList.AddCommand<Flux_CommandBindCBV>(&m_xFrustumPlanesBuffer.GetCBV(), 1);              // Frustum planes (read)
	xCmdList.AddCommand<Flux_CommandBindUAV_Buffer>(&m_xIndirectDrawBuffer.GetUAV(), 2);        // Indirect commands (write)
	xCmdList.AddCommand<Flux_CommandBindUAV_Buffer>(&m_xVisibleCountBuffer.GetUAV(), 3);        // Visible count (read/write atomic)
	xCmdList.AddCommand<Flux_CommandBindUAV_Buffer>(&m_xLODLevelBuffer.GetUAV(), 4);            // LOD levels (read-modify-write for hysteresis)

	// Dispatch compute shader
	// We have TOTAL_CHUNKS chunks, with local_size_x=64 we need (TOTAL_CHUNKS + 63) / 64 workgroups
	uint32_t uNumWorkgroups = (TOTAL_CHUNKS + 63) / 64;
	xCmdList.AddCommand<Flux_CommandDispatch>(uNumWorkgroups, 1, 1);
}
// Editor code for RenderPropertiesPanel is in Zenith_TerrainComponent_Editor.cpp// Editor code for RenderPropertiesPanel is in Zenith_TerrainComponent_Editor.cpp