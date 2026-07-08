#include "Zenith.h"
#include "Flux/Flux_RendererImpl.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "ZenithECS/Zenith_ComponentMeta.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Flux/Terrain/Flux_TerrainStreamingManagerImpl.h"
#include "Maths/Zenith_FrustumCulling.h"
// Wave 3 PART B: terrain render-record gather (so Flux_Terrain drops Zenith_TerrainComponent.h).
#include "Core/Zenith_Engine.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_Query.h"
#include <fstream>

// The Flux GPU state these methods operate on now lives on the owning
// Flux_TerrainStreamingState (Wave-18 relocation). Accessing
// m_pxStreamingState->m_x... throughout is the O(1) pointer hop the component
// already paid for the streaming state; no map lookup was added.
//
// NOTE: the full Flux_MeshGeometry type (used by LoadCombinedPhysicsGeometry /
// LoadAndCombineLowLODChunks / InitializeUnifiedBuffers) is pulled in
// transitively by Flux/Flux_GraphicsImpl.h above (it owns Flux_MeshGeometry
// members by value). Deliberately NOT adding a direct
// #include "Flux/MeshGeometry/Flux_MeshGeometry.h" here: that would introduce a
// new Zenith_TerrainComponent.cpp => Flux/MeshGeometry edge the layering gate
// would (correctly) flag. The .cpp's existing allow-listed Flux dependencies
// already carry the type.

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
{
	IncrementInstanceCount();

	// Each terrain owns its own streaming state. Initialise() pre-allocates
	// the cached chunk-data scratch buffer and resets per-frame counters;
	// the allocators get sized later in RegisterTerrainBuffers (it has the
	// vertex stride needed to derive vertex-count budgets). The relocated GPU
	// state (buffers + scalars + m_bCullingResourcesInitialized) default-inits
	// inside Flux_TerrainStreamingState.
	m_pxStreamingState = new Flux_TerrainStreamingState();
	m_pxStreamingState->Initialize();
}

Zenith_TerrainComponent::Zenith_TerrainComponent(Zenith_MaterialAsset& xMaterial0, Zenith_MaterialAsset& xMaterial1, Zenith_Entity& xEntity)
	: m_xParentEntity(xEntity)
{
	IncrementInstanceCount();

	// Allocate this terrain's own streaming state — same pattern as the
	// default (deserialization) constructor. The unified-buffer scalars and
	// the culling-init flag now live on (and default-init inside) the state.
	m_pxStreamingState = new Flux_TerrainStreamingState();
	m_pxStreamingState->Initialize();

	// Store material handles (auto ref-counting)
	m_axMaterials[0].Set(&xMaterial0);
	m_axMaterials[1].Set(&xMaterial1);

	// Ensure all 4 material slots have valid assets (blank fallback for 2-3)
	for (u_int u = 2; u < TERRAIN_MATERIAL_COUNT; u++)
	{
		if (!m_axMaterials[u].GetDirect())
		{
			auto xhBlank = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
			Zenith_MaterialAsset* pxBlank = xhBlank.GetDirect();
			if (pxBlank)
			{
				pxBlank->SetName("Terrain_Mat" + std::to_string(u));
				m_axMaterials[u].Set(pxBlank);
			}
		}
	}

	// Ensure streaming manager is initialized — defensive, normally
	// g_xEngine.Terrain().Initialise() does this once at engine startup.
	auto& xTerrainStreaming = g_xEngine.TerrainStreaming();
	if (!xTerrainStreaming.IsInitialized())
	{
		xTerrainStreaming.Initialize();
	}

#pragma region Render
{
	// Initialize render resources (LOW LOD meshes, unified buffers, culling)
	// This is the same initialization performed during deserialization
	InitializeRenderResources();
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
	// A moved-from component owns nothing — its state pointer was stolen and
	// nulled, and DecrementInstanceCount must NOT run (the moved-to component
	// is the live one, and IncrementInstanceCount fired once at construction).
	if (m_pxStreamingState == nullptr)
	{
		// Physics geometry was also stolen (nulled) by the move; delete is a
		// no-op on null but kept for symmetry / clarity.
		delete m_pxPhysicsGeometry;
		m_pxPhysicsGeometry = nullptr;
		return;
	}

	DestroyCullingResources();

	// Take this terrain out of the manager's registry FIRST so no concurrent
	// PreRenderUpdate iteration can pick up a state that's about to be freed.
	g_xEngine.TerrainStreaming().UnregisterTerrainBuffers(m_pxStreamingState);

	// Destroy the owned unified buffers (they now live ON the streaming state)
	// BEFORE freeing the state — preserves the documented destroy order:
	// DestroyCullingResources -> unregister -> destroy unified buffers ->
	// delete state.
	auto& xVulkanMemory = g_xEngine.FluxMemory();
	xVulkanMemory.DestroyVertexBuffer(m_pxStreamingState->m_xUnifiedVertexBuffer);
	xVulkanMemory.DestroyIndexBuffer(m_pxStreamingState->m_xUnifiedIndexBuffer);

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Zenith_TerrainComponent - Unified terrain buffers destroyed");

	m_pxStreamingState->Shutdown();
	delete m_pxStreamingState;
	m_pxStreamingState = nullptr;

	delete m_pxPhysicsGeometry;
	m_pxPhysicsGeometry = nullptr;

	// MaterialHandle members (m_axMaterials[]) auto-release when destroyed

	DecrementInstanceCount();
}

// ========== Move semantics (Wave-18) ==========
// STEAL the owned state + physics geometry + material/splat handles from the
// source, null the source's owning pointers, and REPOINT the streaming state's
// owner back-pointer at *this so the manager registry / per-frame resolver keep
// dereferencing the live component. See the header for why the implicit move
// (a shallow pointer copy → double-free on pool relocation) is wrong.
Zenith_TerrainComponent::Zenith_TerrainComponent(Zenith_TerrainComponent&& xOther) noexcept
	: m_xParentEntity(xOther.m_xParentEntity)
	, m_pxPhysicsGeometry(xOther.m_pxPhysicsGeometry)
	, m_bTerrainGeometryUnusable(xOther.m_bTerrainGeometryUnusable)
	, m_pxStreamingState(xOther.m_pxStreamingState)
{
	for (u_int u = 0; u < TERRAIN_MATERIAL_COUNT; u++)
		m_axMaterials[u] = std::move(xOther.m_axMaterials[u]);
	m_xSplatmap = std::move(xOther.m_xSplatmap);

	// Wave 3: the old "repoint m_pxStreamingState->m_pxOwner = this" is gone — the
	// Flux state no longer stores a Zenith_TerrainComponent back-pointer (it was never
	// dereferenced; only an identity/validation the streaming manager no longer needs).
	// The double-free fix is purely the state-steal + null-source below.

	// Null the source so its destructor frees nothing (no double-free).
	xOther.m_pxStreamingState = nullptr;
	xOther.m_pxPhysicsGeometry = nullptr;
}

Zenith_TerrainComponent& Zenith_TerrainComponent::operator=(Zenith_TerrainComponent&& xOther) noexcept
{
	if (this == &xOther)
		return *this;

	// Release anything this component currently owns before stealing. Mirrors
	// the destructor's order (cull -> unregister -> unified buffers -> state).
	if (m_pxStreamingState)
	{
		DestroyCullingResources();
		g_xEngine.TerrainStreaming().UnregisterTerrainBuffers(m_pxStreamingState);
		auto& xVulkanMemory = g_xEngine.FluxMemory();
		xVulkanMemory.DestroyVertexBuffer(m_pxStreamingState->m_xUnifiedVertexBuffer);
		xVulkanMemory.DestroyIndexBuffer(m_pxStreamingState->m_xUnifiedIndexBuffer);
		m_pxStreamingState->Shutdown();
		delete m_pxStreamingState;
		m_pxStreamingState = nullptr;
		// This component had its own instance-count slot; the source keeps its
		// own (transferred below). Balance the count for the state we just
		// destroyed so the net count is unchanged by the assignment.
		DecrementInstanceCount();
	}
	delete m_pxPhysicsGeometry;
	m_pxPhysicsGeometry = nullptr;

	// Steal from the source.
	m_xParentEntity            = xOther.m_xParentEntity;
	m_pxPhysicsGeometry        = xOther.m_pxPhysicsGeometry;
	m_bTerrainGeometryUnusable = xOther.m_bTerrainGeometryUnusable;
	m_pxStreamingState         = xOther.m_pxStreamingState;
	for (u_int u = 0; u < TERRAIN_MATERIAL_COUNT; u++)
		m_axMaterials[u] = std::move(xOther.m_axMaterials[u]);
	m_xSplatmap = std::move(xOther.m_xSplatmap);

	// Wave 3: no m_pxOwner back-pointer to repoint (see the move-ctor note).

	xOther.m_pxStreamingState  = nullptr;
	xOther.m_pxPhysicsGeometry = nullptr;

	return *this;
}

// ========== Out-of-line buffer / stride / draw-count forwarders ==========
// Each forwards into the owning Flux_TerrainStreamingState. The full state +
// buffer-wrapper types are visible here (Flux_TerrainStreamingManagerImpl.h);
// the header only forward-declares them.

const Flux_VertexBuffer& Zenith_TerrainComponent::GetUnifiedVertexBuffer() const
{
	return m_pxStreamingState->m_xUnifiedVertexBuffer;
}

const Flux_IndexBuffer& Zenith_TerrainComponent::GetUnifiedIndexBuffer() const
{
	return m_pxStreamingState->m_xUnifiedIndexBuffer;
}

uint32_t Zenith_TerrainComponent::GetVertexStride() const
{
	return m_pxStreamingState->m_uVertexStride;
}

const Flux_MeshGeometry& Zenith_TerrainComponent::GetPhysicsMeshGeometry() const
{
	return *m_pxPhysicsGeometry;
}

const Flux_IndirectBuffer& Zenith_TerrainComponent::GetIndirectDrawBuffer() const
{
	return m_pxStreamingState->m_xIndirectDrawBuffer;
}

const Flux_IndirectBuffer& Zenith_TerrainComponent::GetVisibleCountBuffer() const
{
	return m_pxStreamingState->m_xVisibleCountBuffer;
}

uint32_t Zenith_TerrainComponent::GetMaxDrawCount() const
{
	return Flux_TerrainConfig::TOTAL_CHUNKS;
}

Flux_ReadWriteBuffer& Zenith_TerrainComponent::GetLODLevelBuffer()
{
	return m_pxStreamingState->m_xLODLevelBuffer;
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
	auto xhNewMat = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
	if (Zenith_MaterialAsset* pxNewMat = xhNewMat.GetDirect())
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
			auto xhBlank = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
			if (Zenith_MaterialAsset* pxBlank = xhBlank.GetDirect())
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
	auto& xTerrainStreaming = g_xEngine.TerrainStreaming();
	if (!xTerrainStreaming.IsInitialized())
	{
		Zenith_Log(LOG_CATEGORY_TERRAIN, "ReadFromDataStream - Re-initializing streaming manager (was shut down)");
		xTerrainStreaming.Initialize();
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
	auto& xTerrainStreaming = g_xEngine.TerrainStreaming();
	if (!xTerrainStreaming.IsInitialized())
	{
		Zenith_Log(LOG_CATEGORY_TERRAIN, "InitializeRenderResources - Re-initializing streaming manager (was shut down)");
		xTerrainStreaming.Initialize();
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

	xTerrainStreaming.RegisterTerrainBuffers(m_pxStreamingState, pxChunkInitData);
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

	m_pxStreamingState->m_uVertexStride = xLowLODGeometry.GetBufferLayout().GetStride();

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

	const uint64_t ulLowLODVertexDataSize = static_cast<uint64_t>(uTotalVerts) * m_pxStreamingState->m_uVertexStride;
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

	m_pxStreamingState->m_uLowLODVertexCount = xLowLODGeometry.GetNumVerts();
	m_pxStreamingState->m_uLowLODIndexCount = xLowLODGeometry.GetNumIndices();
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

	auto& xVulkanMemory = g_xEngine.FluxMemory();
	xVulkanMemory.InitialiseVertexBuffer(pUnifiedVertexData, ulUnifiedVertexSize, m_pxStreamingState->m_xUnifiedVertexBuffer);
	xVulkanMemory.InitialiseIndexBuffer(pUnifiedIndexData, ulUnifiedIndexSize, m_pxStreamingState->m_xUnifiedIndexBuffer);

	m_pxStreamingState->m_ulUnifiedVertexBufferSize = ulUnifiedVertexSize;
	m_pxStreamingState->m_ulUnifiedIndexBufferSize = ulUnifiedIndexSize;

	delete[] pUnifiedVertexData;
	delete[] pUnifiedIndexData;

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Unified terrain buffers uploaded to GPU");
	Zenith_Log(LOG_CATEGORY_TERRAIN, "LOW LOD region: vertices [0, %u), indices [0, %u)", m_pxStreamingState->m_uLowLODVertexCount, m_pxStreamingState->m_uLowLODIndexCount);
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Streaming region starts at: vertex %u, index %u", m_pxStreamingState->m_uLowLODVertexCount, m_pxStreamingState->m_uLowLODIndexCount);
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
	if (m_pxStreamingState->m_bCullingResourcesInitialized)
	{
		Zenith_Assert(false, "Zenith_TerrainComponent::InitializeCullingResources() called when already initialized");
		return;
	}

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Zenith_TerrainComponent::InitializeCullingResources() - Setting up GPU-driven terrain culling with LOD support");

	// ========== CREATE GPU BUFFERS ==========

	auto& xVulkanMemory = g_xEngine.FluxMemory();

	// Frustum planes buffer (6 planes, updated per frame)
	xVulkanMemory.InitialiseDynamicConstantBuffer(
		nullptr,
		sizeof(Zenith_CameraDataGPU),
		m_pxStreamingState->m_xFrustumPlanesBuffer
	);

	// Indirect draw command buffer (one command per chunk, max)
	// Structure: VkDrawIndexedIndirectCommand (5 uint32_t values)
	// Initialize to zeros so all commands start with indexCount=0
	const size_t indirectBufferSize = sizeof(uint32_t) * 5 * TOTAL_CHUNKS;
	uint32_t* pZeroBuffer = new uint32_t[5 * TOTAL_CHUNKS];
	memset(pZeroBuffer, 0, indirectBufferSize);

	xVulkanMemory.InitialiseIndirectBuffer(
		indirectBufferSize,
		m_pxStreamingState->m_xIndirectDrawBuffer
	);

	// Upload the zero-initialized data
	xVulkanMemory.UploadBufferData(m_pxStreamingState->m_xIndirectDrawBuffer.GetBuffer().m_xVRAMHandle, pZeroBuffer, indirectBufferSize);
	delete[] pZeroBuffer;

	// Visible chunk counter (single atomic uint32_t)
	xVulkanMemory.InitialiseIndirectBuffer(
		sizeof(uint32_t),
		m_pxStreamingState->m_xVisibleCountBuffer
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
		xVulkanMemory.InitialiseReadWriteBuffer(
			pZero,
			sizeof(uint32_t) * TOTAL_CHUNKS,
			m_pxStreamingState->m_xLODLevelBuffer
		);
		delete[] pZero;
	}

	// Build chunk data (AABBs + LOD metadata) and upload to GPU
	BuildChunkData();

	m_pxStreamingState->m_bCullingResourcesInitialized = true;

	// New per-terrain GPU buffers (chunk data, indirect, count, LOD level)
	// just appeared. g_xEngine.Terrain().SetupRenderGraph reads
	// m_bCullingResourcesInitialized when declaring per-component buffer
	// dependencies, so the next graph compile must rebuild to pick them up.
	g_xEngine.FluxRenderer().RequestGraphRebuild();

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
	if (!m_pxStreamingState || !m_pxStreamingState->m_bCullingResourcesInitialized)
	{
		return;
	}

	// Drop the rebuild flag BEFORE queueing buffer destruction so the next
	// graph compile rebuilds SetupRenderGraph against the new (smaller) set
	// of live terrain components. The deferred-destroy path
	// (QueueVRAMDeletion + MAX_FRAMES_IN_FLIGHT+1 grace) keeps the buffers
	// alive long enough for any in-flight command lists to finish reading
	// them before the GPU side actually frees the memory.
	g_xEngine.FluxRenderer().RequestGraphRebuild();

	// Cleanup GPU resources - queue for deferred deletion to avoid destroying in-use resources.
	// Chunk data buffer is frame-indexed; DestroyDynamicReadWriteBuffer queues
	// every frame slot for deferred deletion.
	auto& xVulkanMemory = g_xEngine.FluxMemory();
	xVulkanMemory.DestroyDynamicReadWriteBuffer(m_pxStreamingState->m_xChunkDataBuffer);
	xVulkanMemory.DestroyDynamicConstantBuffer(m_pxStreamingState->m_xFrustumPlanesBuffer);
	xVulkanMemory.DestroyIndirectBuffer(m_pxStreamingState->m_xIndirectDrawBuffer);
	xVulkanMemory.DestroyIndirectBuffer(m_pxStreamingState->m_xVisibleCountBuffer);
	xVulkanMemory.DestroyReadWriteBuffer(m_pxStreamingState->m_xLODLevelBuffer);

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Zenith_TerrainComponent - Culling resources destroyed");

	m_pxStreamingState->m_bCullingResourcesInitialized = false;
}

void Zenith_TerrainComponent::BuildChunkData()
{
	Zenith_Log(LOG_CATEGORY_TERRAIN, "Zenith_TerrainComponent::BuildChunkData() - Building chunk data using streaming manager");

	// Get chunk data from streaming manager (includes AABBs and current LOD
	// allocations) — component-aware overload routes through this terrain's
	// own state, no cross-terrain pollution.
	Zenith_TerrainChunkData* pxChunkData = new Zenith_TerrainChunkData[TOTAL_CHUNKS];
	g_xEngine.TerrainStreaming().BuildChunkDataForGPU(m_pxStreamingState, pxChunkData);

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Zenith_TerrainComponent - Chunk data retrieved from streaming manager for %u chunks", TOTAL_CHUNKS);

	// Allocate the per-frame buffers. InitialiseDynamicReadWriteBuffer also
	// performs the initial upload into every frame's slot, so the GPU has
	// valid chunk metadata in slot 0 by the time the first frame's compute
	// runs (the orphan-read validator in the render graph would otherwise
	// trip on a Read declaration with no preceding writer).
	g_xEngine.FluxMemory().InitialiseDynamicReadWriteBuffer(
		pxChunkData,
		sizeof(Zenith_TerrainChunkData) * TOTAL_CHUNKS,
		m_pxStreamingState->m_xChunkDataBuffer
	);

	// Cleanup CPU data
	delete[] pxChunkData;

	Zenith_Log(LOG_CATEGORY_TERRAIN, "Zenith_TerrainComponent - Chunk data with %u LOD levels uploaded to GPU", LOD_COUNT);
}

void Zenith_TerrainComponent::UpdateChunkLODAllocations()
{
	// Wave 3: GPU chunk-data upload relocated to Flux_TerrainStreamingManagerImpl (operates on
	// the Flux state). Thin forwarder kept for API compatibility.
	if (m_pxStreamingState) g_xEngine.TerrainStreaming().UpdateChunkLODAllocations(*m_pxStreamingState);
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
	// Wave 3: relocated to Flux_TerrainStreamingManagerImpl. Thin forwarder.
	if (m_pxStreamingState) g_xEngine.TerrainStreaming().UploadFrustumPlanesForFrame(*m_pxStreamingState, xViewProjMatrix);
}

void Zenith_TerrainComponent::UpdateCullingAndLod(Flux_CommandBuffer& xCmdList)
{
	// Wave 3: relocated to Flux_TerrainStreamingManagerImpl. Thin forwarder.
	if (m_pxStreamingState) g_xEngine.TerrainStreaming().UpdateCullingAndLod(*m_pxStreamingState, xCmdList);
}
// Editor code for RenderPropertiesPanel is in Zenith_TerrainComponent_Editor.cpp// Editor code for RenderPropertiesPanel is in Zenith_TerrainComponent_Editor.cpp
// ============================================================================
// Wave 3 PART B: terrain render-record gather. Flux_Terrain consumes these neutral
// records (Flux state + asset handles) instead of querying/holding Zenith_TerrainComponent.
// ============================================================================
static void Zenith_GatherTerrainRecordsImpl(Zenith_Vector<Flux_TerrainRenderRecord>& xOut)
{
	g_xEngine.Scenes().QueryAllScenes<Zenith_TerrainComponent>().ForEach(
		[&xOut](Zenith_EntityID, Zenith_TerrainComponent& xTerrain)
	{
		Flux_TerrainRenderRecord xRec;
		xRec.m_pxState = xTerrain.m_pxStreamingState;
		for (u_int m = 0; m < 4; ++m)
			xRec.m_apxMaterials[m] = xTerrain.GetMaterial(m);
		xRec.m_pxSplatmap = xTerrain.GetSplatmapTexture();
		xOut.PushBack(xRec);
	});
}

Zenith_TerrainGatherFn g_pfnZenithTerrainGather = &Zenith_GatherTerrainRecordsImpl;
