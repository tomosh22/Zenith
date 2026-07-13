#include "Zenith.h"
#include "Profiling/Zenith_Profiling.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "ZenithECS/Zenith_ComponentMeta.h"
#include "Flux/Flux_ModelInstance.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/MeshGeometry/Flux_MeshInstance.h"
#include "Flux/MeshAnimation/Flux_SkeletonInstance.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"
#include "AssetHandling/Zenith_ModelAsset.h"
// Wave 3: EC-side model render-gather (so the Flux mesh consumers drop their
// Zenith_ModelComponent.h / Zenith_TransformComponent.h includes).
#include "Core/Zenith_Engine.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_Query.h"
#include <filesystem>

// File-local scene-system forwarder (defined near the snapshot fill below). Forward-
// declared here so the render-mutation bumps in ClearModel/LoadModel/AddMeshEntry — which
// appear before the definition — can route through it. The definition holds this TU's sole
// engine-singleton occurrence; this declaration adds none.
static Zenith_SceneSystem& Scenes();

void Zenith_ModelComponent::RegisterProperties(Zenith_Vector<Zenith_PropertyDescriptor>& axProperties)
{
	// "Model" is STATEFUL — overriding the asset handle alone would leave the
	// previously-loaded Flux_ModelInstance pointing at the base prefab's mesh,
	// because LoadModel is what creates the instance from a path. Use the
	// CUSTOM macro to deserialise a ModelHandle, extract its path, and call
	// LoadModel so the runtime model instance gets rebuilt for the new asset.
	//
	// (Material slot overrides would need per-mesh-section reflection that
	// doesn't yet exist — see ComponentMeta.h header docs.)
	ZENITH_REGISTER_COMPONENT_PROPERTY_CUSTOM(
		Zenith_ModelComponent, "Model", axProperties,
		{
			ModelHandle xHandle;
			xValue >> xHandle;
			if (!xHandle.GetPath().empty())
			{
				pxComp->LoadModel(xHandle.GetPath());
			}
		});
}


// Serialization version for ModelComponent
// Version 3: New model instance system with .zmodel path
// Version 4: GUID-based model references
// Version 5: Material overrides for model instance
// Version 6: Removed legacy m_xMeshEntries path - single model-instance serialization
// Version 7: Added model physics debug draw toggle
// Version 8: Physics collision mesh moved to Zenith_ColliderComponent — the v7 debug
//            toggle is no longer written; a v7 payload's trailing byte is harmlessly
//            absorbed by the per-component size framing (Zenith_ComponentMeta.cpp).
static constexpr uint32_t MODEL_COMPONENT_SERIALIZE_VERSION = 8;
static constexpr uint32_t MODEL_COMPONENT_SERIALIZE_VERSION_UNIFIED = 6;
static constexpr uint32_t MODEL_COMPONENT_SERIALIZE_VERSION_MATERIALS = 5;

//=============================================================================
// Destructor
//=============================================================================
Zenith_ModelComponent::~Zenith_ModelComponent()
{
	// Clean up model instance system
	ClearModel();
}

//=============================================================================
// Move Semantics - Required for component pool operations
//=============================================================================
Zenith_ModelComponent::Zenith_ModelComponent(Zenith_ModelComponent&& xOther) noexcept
	: m_xParentEntity(xOther.m_xParentEntity)
	, m_pxModelInstance(xOther.m_pxModelInstance)
	, m_xModel(std::move(xOther.m_xModel))
	, m_strModelPath(std::move(xOther.m_strModelPath))
	, m_uGeometryRevision(xOther.m_uGeometryRevision)
{
	// Nullify source pointers so its destructor doesn't delete our resources
	xOther.m_pxModelInstance = nullptr;
}

Zenith_ModelComponent& Zenith_ModelComponent::operator=(Zenith_ModelComponent&& xOther) noexcept
{
	if (this != &xOther)
	{
		// Release our existing resources
		ClearModel();

		// Take ownership from source. m_uGeometryRevision is carried over so a paired
		// Zenith_ColliderComponent's cached collision-mesh revision stays valid across a
		// component-pool relocation (a relocation does not change the geometry).
		m_xParentEntity = xOther.m_xParentEntity;
		m_pxModelInstance = xOther.m_pxModelInstance;
		m_xModel = std::move(xOther.m_xModel);
		m_strModelPath = std::move(xOther.m_strModelPath);
		m_uGeometryRevision = xOther.m_uGeometryRevision;

		// Nullify source pointers
		xOther.m_pxModelInstance = nullptr;
	}
	return *this;
}

//=============================================================================
// Procedural Mesh API
//=============================================================================

void Zenith_ModelComponent::AddMeshEntry(Flux_MeshGeometry& xGeometry, Zenith_MaterialAsset& xMaterial)
{
	if (!m_pxModelInstance)
	{
		m_pxModelInstance = Flux_ModelInstance::CreateProcedural(xGeometry, xMaterial);
	}
	else
	{
		m_pxModelInstance->AppendProceduralMesh(xGeometry, xMaterial);
	}

	// Geometry changed — bump the revision so a paired ColliderComponent regenerates
	// its derived collision mesh on the next rebuild.
	++m_uGeometryRevision;

	// Phase 2: a renderable was created or its mesh set grew — bump the render-mutation
	// epoch so the snapshot's copied diagnostics (mesh count) aren't read as current-but-stale.
	Scenes().NotifyRenderMutation();
}

//=============================================================================
// New Model Instance API
//=============================================================================

void Zenith_ModelComponent::LoadModel(const std::string& strPath)
{
	// Make a local copy of the path before any operations that might invalidate it.
	// This is necessary because callers may pass m_strModelPath, which gets cleared
	// by ClearModel() below.
	std::string strLocalPath = strPath;

	Zenith_Log(LOG_CATEGORY_MESH, "LoadModel called with path: %s", strLocalPath.c_str());

	// Early-out if path is empty or invalid
	if (strLocalPath.empty())
	{
		Zenith_Error(LOG_CATEGORY_MESH, "LoadModel called with empty path");
		return;
	}

	// strLocalPath may be prefixed (e.g. "game:Meshes/Foo.zmodel" or
	// "engine:Meshes/Foo.zmodel") when this is reached via scene deserialization
	// — the saved ModelHandle stores the normalised prefixed form. Resolve to an
	// absolute filesystem path before the exists() check so reload doesn't
	// silently bail with "File does not exist". Zenith_AssetRegistry::Get below
	// already handles prefixed paths, but it doesn't surface "missing file" the
	// same way, so we keep the explicit existence check on the resolved path.
	const std::string strResolvedPath = Zenith_AssetRegistry::ResolvePath(strLocalPath);
	if (!std::filesystem::exists(strResolvedPath))
	{
		Zenith_Error(LOG_CATEGORY_MESH, "LoadModel: File does not exist: %s (resolved: %s)",
			strLocalPath.c_str(), strResolvedPath.c_str());
		return;
	}

	// Clear any existing model
	ClearModel();

	// Load model asset via registry
	Zenith_ModelAsset* pxAsset = Zenith_AssetRegistry::GetView<Zenith_ModelAsset>(strLocalPath);
	if (!pxAsset)
	{
		Zenith_Error(LOG_CATEGORY_MESH, "Failed to load model asset from: %s", strLocalPath.c_str());
		return;
	}

	Zenith_Log(LOG_CATEGORY_MESH, "Model asset loaded: %s (meshes: %u, has skeleton: %s)",
		pxAsset->GetName().c_str(), pxAsset->GetNumMeshes(),
		pxAsset->HasSkeleton() ? "yes" : "no");

	// Create model instance from asset
	m_pxModelInstance = Flux_ModelInstance::CreateFromAsset(pxAsset);
	if (!m_pxModelInstance)
	{
		Zenith_Error(LOG_CATEGORY_MESH, "Failed to create model instance from asset: %s", strLocalPath.c_str());
		// Note: pxAsset is owned by the registry, don't delete it here
		// It will be cleaned up by UnloadUnused() if nothing references it
		return;
	}

	// Store path for serialization
	m_strModelPath = strLocalPath;

	// Also populate handle if not already set
	if (!m_xModel.HasPath())
	{
		m_xModel.SetPath(strLocalPath);
	}

	// Detailed logging for debugging
	Zenith_Log(LOG_CATEGORY_MESH, "SUCCESS: Loaded model from: %s", strLocalPath.c_str());
	Zenith_Log(LOG_CATEGORY_MESH, "  Meshes: %u", m_pxModelInstance->GetNumMeshes());
	Zenith_Log(LOG_CATEGORY_MESH, "  Materials: %u", m_pxModelInstance->GetNumMaterials());
	Zenith_Log(LOG_CATEGORY_MESH, "  Has Skeleton: %s", m_pxModelInstance->HasSkeleton() ? "yes (animated mesh renderer)" : "no (static mesh renderer)");

	for (uint32_t u = 0; u < m_pxModelInstance->GetNumMeshes(); u++)
	{
		Flux_MeshInstance* pxMesh = m_pxModelInstance->GetMeshInstance(u);
		if (pxMesh)
		{
			Zenith_Log(LOG_CATEGORY_MESH, "  Mesh %u: %u verts, %u indices", u, pxMesh->GetNumVerts(), pxMesh->GetNumIndices());
		}
		else
		{
			Zenith_Log(LOG_CATEGORY_MESH, "  Mesh %u: NULL", u);
		}
	}

	// Geometry changed — bump the revision so a paired ColliderComponent regenerates
	// its derived collision mesh on the next rebuild.
	++m_uGeometryRevision;

	// Phase 2: a new renderable now exists — bump the render-mutation epoch so the tools
	// panel doesn't read a snapshot that predates it as "current".
	Scenes().NotifyRenderMutation();
}

void Zenith_ModelComponent::ClearModel()
{
	// Delete model instance (handles cleanup of mesh instances, skeleton instance, etc.)
	if (m_pxModelInstance)
	{
		// Geometry is going away — bump the revision so a paired ColliderComponent's
		// cached collision mesh is treated as stale.
		++m_uGeometryRevision;

		// Phase 2: bump the render-mutation epoch BEFORE the free so the previous
		// frame's snapshot (which may still reference this instance, and which the tools
		// panel reads pre-rebuild) is marked stale before the pointer dangles. This is the
		// single Flux_ModelInstance free site — reached by the dtor, move-assignment,
		// LoadModel's reload, scene unload + entity destroy — so one bump here covers them all.
		Scenes().NotifyRenderMutation();

		m_pxModelInstance->Destroy();
		delete m_pxModelInstance;
		m_pxModelInstance = nullptr;
	}

	// Clear path and GUID reference
	m_strModelPath.clear();
	m_xModel.Clear();
}

//=============================================================================
// Rendering Helpers
//=============================================================================

uint32_t Zenith_ModelComponent::GetNumMeshes() const
{
	return m_pxModelInstance ? m_pxModelInstance->GetNumMeshes() : 0;
}

Flux_MeshInstance* Zenith_ModelComponent::GetMeshInstance(uint32_t uIndex) const
{
	return m_pxModelInstance ? m_pxModelInstance->GetMeshInstance(uIndex) : nullptr;
}

Zenith_MaterialAsset* Zenith_ModelComponent::GetMaterial(uint32_t uIndex) const
{
	return m_pxModelInstance ? m_pxModelInstance->GetMaterial(uIndex) : nullptr;
}

bool Zenith_ModelComponent::HasSkeleton() const
{
	if (m_pxModelInstance)
	{
		return m_pxModelInstance->HasSkeleton();
	}
	return false;
}

Flux_SkeletonInstance* Zenith_ModelComponent::GetSkeletonInstance() const
{
	if (m_pxModelInstance)
	{
		return m_pxModelInstance->GetSkeletonInstance();
	}
	return nullptr;
}

bool Zenith_ModelComponent::GetBoneModelMatrix(const std::string& strBoneName, Zenith_Maths::Matrix4& xOut) const
{
	Flux_SkeletonInstance* pxSkel = GetSkeletonInstance();
	if (!pxSkel)
		return false;
	Zenith_SkeletonAsset* pxAsset = pxSkel->GetSourceSkeleton();
	if (!pxAsset)
		return false;
	const int32_t iBone = pxAsset->GetBoneIndex(strBoneName);
	if (iBone < 0)
		return false;
	xOut = pxSkel->GetBoneModelTransform(static_cast<uint32_t>(iBone));
	return true;
}

//=============================================================================
// Serialization
//=============================================================================

void Zenith_ModelComponent::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << MODEL_COMPONENT_SERIALIZE_VERSION;

	// v6+: single model-instance serialization path.
	m_xModel.WriteToDataStream(xStream);

	uint32_t uNumMaterials = m_pxModelInstance ? m_pxModelInstance->GetNumMaterials() : 0;
	xStream << uNumMaterials;

	for (uint32_t u = 0; u < uNumMaterials; u++)
	{
		Zenith_MaterialAsset* pxMaterial = m_pxModelInstance->GetMaterial(u);
		if (pxMaterial)
		{
			pxMaterial->WriteToDataStream(xStream);
		}
		else
		{
			Zenith_MaterialAsset xEmptyMat;
			xEmptyMat.SetName("Empty");
			xEmptyMat.WriteToDataStream(xStream);
		}
	}
}

void Zenith_ModelComponent::ReadModelInstanceWithMaterials(Zenith_DataStream& xStream, uint32_t uVersion)
{
	// Read model GUID
	m_xModel.ReadFromDataStream(xStream);

	// Resolve GUID to path and load the model
	if (m_xModel.HasPath())
	{
		m_strModelPath = m_xModel.GetPath();
		if (!m_strModelPath.empty())
		{
			LoadModel(m_strModelPath);
		}
		else
		{
			Zenith_Error(LOG_CATEGORY_MESH, "Failed to resolve model GUID to path");
		}
	}

	if (uVersion < MODEL_COMPONENT_SERIALIZE_VERSION_MATERIALS)
		return;

	uint32_t uNumMaterials = 0;
	xStream >> uNumMaterials;

	for (uint32_t u = 0; u < uNumMaterials; u++)
	{
		auto xhMaterial = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
		Zenith_MaterialAsset* pxMaterial = xhMaterial.GetDirect();
		if (!pxMaterial)
			continue;

		pxMaterial->SetName("LoadedMaterial");
		pxMaterial->ReadFromDataStream(xStream);

		if (m_pxModelInstance && u < m_pxModelInstance->GetNumMaterials())
		{
			// SetMaterial stores an owning handle. The temporary registry handle
			// keeps the material alive through deserialization, then the model
			// instance takes the sole retained reference when this scope exits.
			m_pxModelInstance->SetMaterial(u, pxMaterial);
		}
	}

	// v7 wrote a trailing physics-debug-draw bool here; v8 no longer reads it (the
	// collision mesh + its debug toggle moved to Zenith_ColliderComponent). An old v7
	// payload's extra byte is harmlessly absorbed by the per-component size framing
	// (Zenith_ComponentMeta.cpp), so there is nothing to consume.
}

void Zenith_ModelComponent::ReadFromDataStream(Zenith_DataStream& xStream)
{
	ClearModel();
	m_xModel.Clear();

	uint32_t uVersion;
	xStream >> uVersion;

	if (uVersion < MODEL_COMPONENT_SERIALIZE_VERSION_UNIFIED)
	{
		Zenith_Error(LOG_CATEGORY_MESH,
			"ModelComponent: unsupported pre-v%u scene format (version %u). "
			"Re-save the scene in the editor to migrate to v%u.",
			MODEL_COMPONENT_SERIALIZE_VERSION_UNIFIED, uVersion, MODEL_COMPONENT_SERIALIZE_VERSION_UNIFIED);
		return;
	}

	ReadModelInstanceWithMaterials(xStream, uVersion);
}

// Editor code for RenderPropertiesPanel and AssignTextureToSlot
// is in Zenith_ModelComponent_Editor.cpp

// ---------------------------------------------------------------------------
// Wave 3: model render-gather. Queries every Zenith_ModelComponent with a built
// model instance and produces parallel (instance, world-matrix) lists for the
// mesh renderers. The unified mesh path consumes the full list and filters it
// itself (the static walk skips skinned-animated models; the skinned walk keeps
// them) -- identical to the per-renderer queries this replaces.
// ---------------------------------------------------------------------------
// File-local scene-system accessor. This forwarder holds the SOLE engine-singleton
// occurrence in this TU (baseline count 1): both the snapshot fill below AND every
// NotifyRenderMutation() call route through it, so adding the epoch notifications does
// not raise the singleton-ratchet count. NOT a getter on the component — a TU-local
// forwarder, like the per-feature re-entry pattern in the Flux subsystems.
static Zenith_SceneSystem& Scenes() { return g_xEngine.Scenes(); }

// Phase 2: EC->Flux snapshot fill (replaces the former two-vector Zenith_GatherModelInstancesImpl).
// Queries every Zenith_ModelComponent with a built model instance once per frame and pushes
// one Flux_RenderSceneItem per renderable (instance + cached world matrix + pointer-free
// diagnostics) into the snapshot's item vector. Flux_RenderSceneItem + this fn-ptr type live
// in the already-included Flux_ModelInstance.h, so this TU never includes the snapshot header
// (no new EntityComponent->Flux edge). The mesh renderers each derive their own filtered
// packet from the snapshot — identical selection to the per-renderer filters this feeds.
static void Zenith_FillSceneSnapshotImpl(Zenith_Vector<Flux_RenderSceneItem>& xOutItems)
{
	ZENITH_PROFILE_SCOPE("Snapshot::Fill");
	Scenes().QueryAllScenes<Zenith_ModelComponent>()
		.ForEach([&xOutItems](Zenith_EntityID xEntityID, Zenith_ModelComponent& xModel)
	{
		Flux_ModelInstance* pxModelInstance = xModel.GetModelInstance();
		if (!pxModelInstance) return;

		Zenith_Maths::Matrix4 xMatrix;
		xModel.GetParentEntity().GetComponent<Zenith_TransformComponent>().BuildModelMatrix(xMatrix);

		Flux_RenderSceneItem xItem;
		xItem.m_pxModelInstance  = pxModelInstance;
		xItem.m_xWorldMatrix     = xMatrix;
		// Phase 3: world-space AABB = the model's local union bounds transformed by the
		// world matrix. Consumers frustum-cull against this; overlays draw it.
		xItem.m_xWorldAABB       = Zenith_FrustumCulling::TransformAABB(pxModelInstance->GetLocalBounds(), xMatrix);
		xItem.m_ulEntityIDPacked = xEntityID.GetPacked();
		xItem.m_uMeshCount       = pxModelInstance->GetNumMeshes();
		xItem.m_bAnimatedSkinned = pxModelInstance->IsAnimatedSkinned();
		xOutItems.PushBack(xItem);
	});
}

Zenith_SceneSnapshotFillFn g_pfnZenithSceneSnapshotFill = &Zenith_FillSceneSnapshotImpl;
