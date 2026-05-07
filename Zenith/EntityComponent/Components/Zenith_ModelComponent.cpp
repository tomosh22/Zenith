#include "Zenith.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Zenith_ComponentMeta.h"
#include "Flux/Flux_ModelInstance.h"
#include "Flux/MeshGeometry/Flux_MeshInstance.h"
#include "Flux/MeshAnimation/Flux_SkeletonInstance.h"
#include "Flux/MeshAnimation/Flux_MeshAnimation.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "AssetHandling/Zenith_MeshAsset.h"
#include "AssetHandling/Zenith_MeshGeometryAsset.h"
#include <filesystem>

ZENITH_REGISTER_COMPONENT(Zenith_ModelComponent, "Model")

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
static constexpr uint32_t MODEL_COMPONENT_SERIALIZE_VERSION = 7;
static constexpr uint32_t MODEL_COMPONENT_SERIALIZE_VERSION_UNIFIED = 6;
static constexpr uint32_t MODEL_COMPONENT_SERIALIZE_VERSION_MATERIALS = 5;
static constexpr uint32_t MODEL_COMPONENT_SERIALIZE_VERSION_DEBUG_DRAW = 7;

//=============================================================================
// Destructor
//=============================================================================
Zenith_ModelComponent::~Zenith_ModelComponent()
{
	// Clean up model instance system
	ClearModel();

	// Clean up physics mesh if it was generated
	ClearPhysicsMesh();
}

//=============================================================================
// Move Semantics - Required for component pool operations
//=============================================================================
Zenith_ModelComponent::Zenith_ModelComponent(Zenith_ModelComponent&& xOther) noexcept
	: m_xParentEntity(xOther.m_xParentEntity)
	, m_pxModelInstance(xOther.m_pxModelInstance)
	, m_xModel(std::move(xOther.m_xModel))
	, m_strModelPath(std::move(xOther.m_strModelPath))
	, m_xPhysicsMeshAsset(std::move(xOther.m_xPhysicsMeshAsset))
	, m_bDebugDrawPhysicsMesh(xOther.m_bDebugDrawPhysicsMesh)
{
	// Nullify source pointers so its destructor doesn't delete our resources
	xOther.m_pxModelInstance = nullptr;
	xOther.m_bDebugDrawPhysicsMesh = false;
}

Zenith_ModelComponent& Zenith_ModelComponent::operator=(Zenith_ModelComponent&& xOther) noexcept
{
	if (this != &xOther)
	{
		// Release our existing resources
		ClearModel();
		ClearPhysicsMesh();

		// Take ownership from source
		m_xParentEntity = xOther.m_xParentEntity;
		m_pxModelInstance = xOther.m_pxModelInstance;
		m_xModel = std::move(xOther.m_xModel);
		m_strModelPath = std::move(xOther.m_strModelPath);
		m_xPhysicsMeshAsset = std::move(xOther.m_xPhysicsMeshAsset);
		m_bDebugDrawPhysicsMesh = xOther.m_bDebugDrawPhysicsMesh;

		// Nullify source pointers
		xOther.m_pxModelInstance = nullptr;
		xOther.m_bDebugDrawPhysicsMesh = false;
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
	Zenith_ModelAsset* pxAsset = Zenith_AssetRegistry::Get<Zenith_ModelAsset>(strLocalPath);
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
	if (!m_xModel.IsSet())
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

	// Generate physics mesh if auto-generation is enabled
	if (g_xPhysicsMeshConfig.m_bAutoGenerate && m_pxModelInstance->GetNumMeshes() > 0)
	{
		GeneratePhysicsMesh();
	}
}

void Zenith_ModelComponent::ClearModel()
{
	// Delete model instance (handles cleanup of mesh instances, skeleton instance, etc.)
	if (m_pxModelInstance)
	{
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

	xStream << m_bDebugDrawPhysicsMesh;
}

void Zenith_ModelComponent::ReadModelInstanceWithMaterials(Zenith_DataStream& xStream, uint32_t uVersion)
{
	// Read model GUID
	m_xModel.ReadFromDataStream(xStream);

	// Resolve GUID to path and load the model
	if (m_xModel.IsSet())
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
		Zenith_MaterialAsset* pxMaterial = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
		if (!pxMaterial)
			continue;

		pxMaterial->SetName("LoadedMaterial");
		pxMaterial->ReadFromDataStream(xStream);
		pxMaterial->AddRef();  // Add reference for this component's usage

		if (m_pxModelInstance && u < m_pxModelInstance->GetNumMaterials())
		{
			m_pxModelInstance->SetMaterial(u, pxMaterial);
		}
	}

	if (uVersion >= MODEL_COMPONENT_SERIALIZE_VERSION_DEBUG_DRAW)
	{
		xStream >> m_bDebugDrawPhysicsMesh;
	}
}

void Zenith_ModelComponent::ReadFromDataStream(Zenith_DataStream& xStream)
{
	ClearModel();
	m_xModel.Clear();
	m_bDebugDrawPhysicsMesh = false;

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

//=============================================================================
// Physics Mesh
//=============================================================================

void Zenith_ModelComponent::GeneratePhysicsMesh(PhysicsMeshQuality eQuality)
{
	PhysicsMeshConfig xConfig = g_xPhysicsMeshConfig;
	xConfig.m_eQuality = eQuality;
	GeneratePhysicsMeshWithConfig(xConfig);
}

namespace
{
	// Builds a throwaway Flux_MeshGeometry holding positions + indices copied
	// from a Zenith_MeshAsset, for feeding the physics generator. The physics
	// generator only reads m_pxPositions / m_puIndices / counts, so no other
	// attributes need to be populated.
	// Caller owns the returned pointer and must delete it.
	Flux_MeshGeometry* BuildTempGeometryFromAsset(const Zenith_MeshAsset* pxAsset)
	{
		if (!pxAsset)
		{
			return nullptr;
		}

		const uint32_t uNumVerts = pxAsset->GetNumVerts();
		const uint32_t uNumIndices = pxAsset->GetNumIndices();
		if (uNumVerts == 0 || uNumIndices == 0)
		{
			return nullptr;
		}

		Flux_MeshGeometry* pxGeom = new Flux_MeshGeometry();
		pxGeom->m_uNumVerts = uNumVerts;
		pxGeom->m_uNumIndices = uNumIndices;

		pxGeom->m_pxPositions = static_cast<Zenith_Maths::Vector3*>(
			Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
		pxGeom->m_puIndices = static_cast<Flux_MeshGeometry::IndexType*>(
			Zenith_MemoryManagement::Allocate(uNumIndices * sizeof(Flux_MeshGeometry::IndexType)));

		for (uint32_t v = 0; v < uNumVerts; v++)
		{
			pxGeom->m_pxPositions[v] = pxAsset->m_xPositions.Get(v);
		}
		for (uint32_t i = 0; i < uNumIndices; i++)
		{
			pxGeom->m_puIndices[i] = pxAsset->m_xIndices.Get(i);
		}

		return pxGeom;
	}
}

void Zenith_ModelComponent::GeneratePhysicsMeshWithConfig(const PhysicsMeshConfig& xConfig)
{
	ClearPhysicsMesh();

	if (!m_pxModelInstance || m_pxModelInstance->GetNumMeshes() == 0)
	{
		Zenith_Error(LOG_CATEGORY_PHYSICS, "Cannot generate physics mesh: no model instance");
		return;
	}

	// Collect geometries from the model instance. Procedural meshes expose their
	// source geometry directly; asset-backed meshes require a throwaway geometry
	// populated from the mesh asset's positions/indices.
	Zenith_Vector<const Flux_MeshGeometry*> xMeshGeometries;
	Zenith_Vector<Flux_MeshGeometry*> xTempGeometries;

	const uint32_t uNumMeshes = m_pxModelInstance->GetNumMeshes();
	for (uint32_t uMesh = 0; uMesh < uNumMeshes; uMesh++)
	{
		Flux_MeshInstance* pxMeshInstance = m_pxModelInstance->GetMeshInstance(uMesh);
		if (!pxMeshInstance)
		{
			continue;
		}

		if (const Flux_MeshGeometry* pxProcedural = pxMeshInstance->GetProceduralGeometry())
		{
			xMeshGeometries.PushBack(pxProcedural);
			continue;
		}

		if (Flux_MeshGeometry* pxTemp = BuildTempGeometryFromAsset(pxMeshInstance->GetSourceAsset()))
		{
			xMeshGeometries.PushBack(pxTemp);
			xTempGeometries.PushBack(pxTemp);
		}
	}

	if (xMeshGeometries.GetSize() == 0)
	{
		Zenith_Error(LOG_CATEGORY_PHYSICS, "Cannot generate physics mesh: no valid geometries");
		// Nothing to clean up - xTempGeometries is empty in this branch.
		return;
	}

	if (m_xParentEntity.HasComponent<Zenith_TransformComponent>())
	{
		Zenith_TransformComponent& xTransform = m_xParentEntity.GetComponent<Zenith_TransformComponent>();
		Zenith_Maths::Vector3 xScale;
		xTransform.GetScale(xScale);
		Zenith_Log(LOG_CATEGORY_PHYSICS, "Generating physics mesh with entity scale (%.3f, %.3f, %.3f)",
			xScale.x, xScale.y, xScale.z);
	}

	if (Zenith_MeshGeometryAsset* pxPhysicsAsset = Zenith_PhysicsMeshGenerator::GeneratePhysicsMeshWithConfig(xMeshGeometries, xConfig))
	{
		m_xPhysicsMeshAsset.Set(pxPhysicsAsset);
		Flux_MeshGeometry* pxGeometry = pxPhysicsAsset->GetGeometry();
		Zenith_Log(LOG_CATEGORY_PHYSICS, "Generated physics mesh for model: %u verts, %u tris",
			pxGeometry->GetNumVerts(),
			pxGeometry->GetNumIndices() / 3);

		if (pxGeometry->GetNumVerts() > 0)
		{
			Zenith_Maths::Vector3& v0 = pxGeometry->m_pxPositions[0];
			Zenith_Log(LOG_CATEGORY_PHYSICS, "First vertex in model space: (%.3f, %.3f, %.3f)",
				v0.x, v0.y, v0.z);
		}
	}
	else
	{
		Zenith_Error(LOG_CATEGORY_PHYSICS, "Failed to generate physics mesh for model");
	}

	// Release throwaway geometries now that the generator has produced its output.
	for (uint32_t u = 0; u < xTempGeometries.GetSize(); u++)
	{
		delete xTempGeometries.Get(u);
	}
}

void Zenith_ModelComponent::ClearPhysicsMesh()
{
	// Drop the handle ref; registry frees the asset when its refcount hits zero.
	m_xPhysicsMeshAsset.Clear();
}

Flux_MeshGeometry* Zenith_ModelComponent::GetPhysicsMesh() const
{
	Zenith_MeshGeometryAsset* pxPhysics = m_xPhysicsMeshAsset.GetDirect();
	return pxPhysics ? pxPhysics->GetGeometry() : nullptr;
}

void Zenith_ModelComponent::QueueDebugDrawPhysicsMesh(const Zenith_Maths::Vector3& xColor) const
{
	const Flux_MeshGeometry* pxPhysicsMesh = GetPhysicsMesh();
	if (!pxPhysicsMesh)
	{
		return;
	}

	Zenith_TransformComponent& xTransform = m_xParentEntity.GetComponent<Zenith_TransformComponent>();
	Zenith_Maths::Matrix4 xModelMatrix;
	xTransform.BuildModelMatrix(xModelMatrix);
	Zenith_PhysicsMeshGenerator::DebugDrawPhysicsMesh(pxPhysicsMesh, xModelMatrix, xColor);
}

// Editor code for RenderPropertiesPanel and AssignTextureToSlot
// is in Zenith_ModelComponent_Editor.cpp
