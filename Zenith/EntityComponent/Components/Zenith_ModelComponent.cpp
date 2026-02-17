#include "Zenith.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_ComponentMeta.h"
#include "DataStream/Zenith_DataStream.h"
#include "Flux/Flux_ModelInstance.h"
#include "Flux/MeshGeometry/Flux_MeshInstance.h"
#include "Flux/MeshAnimation/Flux_SkeletonInstance.h"
#include "Flux/MeshAnimation/Flux_MeshAnimation.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "AssetHandling/Zenith_MeshGeometryAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "Core/Zenith_Core.h"
#include <filesystem>

ZENITH_REGISTER_COMPONENT(Zenith_ModelComponent, "Model")


// Serialization version for ModelComponent
// Version 3: New model instance system with .zmodel path
// Version 4: GUID-based model references
// Version 5: Material overrides for model instance
static constexpr uint32_t MODEL_COMPONENT_SERIALIZE_VERSION = 5;
static constexpr uint32_t MODEL_COMPONENT_SERIALIZE_VERSION_MATERIALS = 5;
static constexpr uint32_t MODEL_COMPONENT_SERIALIZE_VERSION_GUID = 4;
static constexpr uint32_t MODEL_COMPONENT_SERIALIZE_VERSION_PATH = 3;

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
	, m_xMeshEntries(std::move(xOther.m_xMeshEntries))
	, m_pxPhysicsMeshAsset(xOther.m_pxPhysicsMeshAsset)
	, m_bDebugDrawPhysicsMesh(xOther.m_bDebugDrawPhysicsMesh)
	, m_xDebugDrawColor(xOther.m_xDebugDrawColor)
{
	// Nullify source pointers so its destructor doesn't delete our resources
	xOther.m_pxModelInstance = nullptr;
	xOther.m_pxPhysicsMeshAsset = nullptr;
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
		m_xMeshEntries = std::move(xOther.m_xMeshEntries);
		m_pxPhysicsMeshAsset = xOther.m_pxPhysicsMeshAsset;
		m_bDebugDrawPhysicsMesh = xOther.m_bDebugDrawPhysicsMesh;
		m_xDebugDrawColor = xOther.m_xDebugDrawColor;

		// Nullify source pointers
		xOther.m_pxModelInstance = nullptr;
		xOther.m_pxPhysicsMeshAsset = nullptr;
	}
	return *this;
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

	if (!std::filesystem::exists(strLocalPath))
	{
		Zenith_Error(LOG_CATEGORY_MESH, "LoadModel: File does not exist: %s", strLocalPath.c_str());
		return;
	}

	// Clear any existing model
	ClearModel();

	// Load model asset via registry
	Zenith_ModelAsset* pxAsset = Zenith_AssetRegistry::Get().Get<Zenith_ModelAsset>(strLocalPath);
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
	if (m_pxModelInstance)
	{
		return m_pxModelInstance->GetNumMeshes();
	}
	// Fall back to procedural mesh entries
	return m_xMeshEntries.GetSize();
}

Flux_MeshInstance* Zenith_ModelComponent::GetMeshInstance(uint32_t uIndex) const
{
	if (m_pxModelInstance)
	{
		return m_pxModelInstance->GetMeshInstance(uIndex);
	}
	// Legacy system doesn't use Flux_MeshInstance
	return nullptr;
}

Zenith_MaterialAsset* Zenith_ModelComponent::GetMaterial(uint32_t uIndex) const
{
	if (m_pxModelInstance)
	{
		return m_pxModelInstance->GetMaterial(uIndex);
	}
	// Fall back to procedural mesh entries
	if (uIndex < m_xMeshEntries.GetSize())
	{
		return m_xMeshEntries.Get(uIndex).m_xMaterial.Get();
	}
	return nullptr;
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
	// Write serialization version
	xStream << MODEL_COMPONENT_SERIALIZE_VERSION;

	// Check if using new model instance system
	bool bUsingModelInstance = (m_pxModelInstance != nullptr || m_xModel.IsSet());
	xStream << bUsingModelInstance;

	if (bUsingModelInstance)
	{
		// Version 4+: Write model GUID
		m_xModel.WriteToDataStream(xStream);

		// Version 5+: Write material overrides
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
				// Write empty material placeholder - create temporary material for serialization
				Zenith_MaterialAsset xEmptyMat;
				xEmptyMat.SetName("Empty");
				xEmptyMat.WriteToDataStream(xStream);
			}
		}
	}
	else
	{
		// Legacy system: Write mesh entries
		u_int uNumEntries = m_xMeshEntries.GetSize();
		xStream << uNumEntries;

		for (u_int u = 0; u < uNumEntries; u++)
		{
			const MeshEntry& xEntry = m_xMeshEntries.Get(u);

			// Get mesh source path
			std::string strMeshPath = xEntry.m_pxGeometry ? xEntry.m_pxGeometry->m_strSourcePath : "";
			xStream << strMeshPath;

			// Serialize the entire material
			Zenith_MaterialAsset* pxMaterial = xEntry.m_xMaterial.Get();
			if (pxMaterial)
			{
				pxMaterial->WriteToDataStream(xStream);
			}
			else
			{
				// Write empty material placeholder - create temporary material for serialization
				Zenith_MaterialAsset xEmptyMat;
				xEmptyMat.SetName("Empty");
				xEmptyMat.WriteToDataStream(xStream);
			}

			// Serialize animation path if animation exists
			std::string strAnimPath = "";
			if (xEntry.m_pxGeometry && xEntry.m_pxGeometry->m_pxAnimation)
			{
				strAnimPath = xEntry.m_pxGeometry->m_pxAnimation->GetSourcePath();
			}
			xStream << strAnimPath;
		}
	}
}

void Zenith_ModelComponent::ReadFromDataStream(Zenith_DataStream& xStream)
{
	// Clear existing data
	ClearModel();
	m_xMeshEntries.Clear();
	m_xModel.Clear();

	// Read serialization version
	uint32_t uVersion;
	xStream >> uVersion;

	if (uVersion < MODEL_COMPONENT_SERIALIZE_VERSION_GUID)
	{
		Zenith_Error(LOG_CATEGORY_MESH, "Unsupported legacy format version %u. Please re-save the scene.", uVersion);
		return;
	}

	// Version 4+: GUID-based model references
	bool bUsingModelInstance;
	xStream >> bUsingModelInstance;

	if (bUsingModelInstance)
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

		// Version 5+: Read and apply material overrides
		if (uVersion >= MODEL_COMPONENT_SERIALIZE_VERSION_MATERIALS)
		{
			uint32_t uNumMaterials = 0;
			xStream >> uNumMaterials;

			for (uint32_t u = 0; u < uNumMaterials; u++)
			{
				// Create material through registry for proper lifetime management
				Zenith_MaterialAsset* pxMaterial = Zenith_AssetRegistry::Get().Create<Zenith_MaterialAsset>();
				if (pxMaterial)
				{
					pxMaterial->SetName("LoadedMaterial");
					pxMaterial->ReadFromDataStream(xStream);
					pxMaterial->AddRef();  // Add reference for this component's usage

					// Apply material to model instance if it was loaded successfully
					if (m_pxModelInstance && u < m_pxModelInstance->GetNumMaterials())
					{
						m_pxModelInstance->SetMaterial(u, pxMaterial);
					}
				}
			}
		}
	}
	else
	{
		// Legacy system: Read mesh entries with file paths
		// Meshes are loaded from their source paths if available
		u_int uNumEntries;
		xStream >> uNumEntries;

		for (u_int u = 0; u < uNumEntries; u++)
		{
			// Read mesh path
			std::string strMeshPath;
			xStream >> strMeshPath;

			// Read material data through registry for proper lifetime management
			Zenith_MaterialAsset* pxMaterial = Zenith_AssetRegistry::Get().Create<Zenith_MaterialAsset>();
			if (pxMaterial)
			{
				pxMaterial->SetName("Material");
				pxMaterial->ReadFromDataStream(xStream);

				// Read animation path (for future use)
				std::string strAnimPath;
				xStream >> strAnimPath;

				// If mesh path is set, load the mesh from file
				if (!strMeshPath.empty() && std::filesystem::exists(strMeshPath))
				{
					Flux_MeshGeometry* pxGeometry = new Flux_MeshGeometry();
					Flux_MeshGeometry::LoadFromFile(strMeshPath.c_str(), *pxGeometry);
					pxGeometry->m_strSourcePath = strMeshPath;  // Preserve path for future serialization

					// Create mesh entry with handle (handles ref counting automatically)
					MeshEntry xEntry;
					xEntry.m_pxGeometry = pxGeometry;
					xEntry.m_xMaterial.Set(pxMaterial);
					m_xMeshEntries.PushBack(std::move(xEntry));
				}
				// else: material stays in registry with refcount 0, will be cleaned up later
			}
			else
			{
				// Skip animation path read
				std::string strAnimPath;
				xStream >> strAnimPath;
			}
		}
	}
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

void Zenith_ModelComponent::GeneratePhysicsMeshWithConfig(const PhysicsMeshConfig& xConfig)
{
	// Clean up existing physics mesh
	ClearPhysicsMesh();

	// Collect mesh geometries from either model instance or procedural mesh entries
	Zenith_Vector<Flux_MeshGeometry*> xMeshGeometries;

	if (m_pxModelInstance)
	{
		// New system: Get geometries from mesh instances
		// TODO: Flux_MeshInstance needs to provide access to geometry or position data
		// For now, physics mesh generation is not supported with new system
		Zenith_Log(LOG_CATEGORY_PHYSICS, "Physics mesh generation not yet implemented for new model instance system");
		return;
	}
	else
	{
		// Legacy system
		if (m_xMeshEntries.GetSize() == 0)
		{
			Zenith_Error(LOG_CATEGORY_PHYSICS, "Cannot generate physics mesh: no mesh entries");
			return;
		}

		for (uint32_t i = 0; i < m_xMeshEntries.GetSize(); i++)
		{
			if (m_xMeshEntries.Get(i).m_pxGeometry)
			{
				xMeshGeometries.PushBack(m_xMeshEntries.Get(i).m_pxGeometry);
			}
		}

		if (xMeshGeometries.GetSize() == 0)
		{
			Zenith_Error(LOG_CATEGORY_PHYSICS, "Cannot generate physics mesh: no valid geometries");
			return;
		}
	}

	// Log current entity scale
	if (m_xParentEntity.HasComponent<Zenith_TransformComponent>())
	{
		Zenith_TransformComponent& xTransform = m_xParentEntity.GetComponent<Zenith_TransformComponent>();
		Zenith_Maths::Vector3 xScale;
		xTransform.GetScale(xScale);
		Zenith_Log(LOG_CATEGORY_PHYSICS, "Generating physics mesh with entity scale (%.3f, %.3f, %.3f)",
			xScale.x, xScale.y, xScale.z);
	}

	// Generate the physics mesh (returns registry-managed asset)
	m_pxPhysicsMeshAsset = Zenith_PhysicsMeshGenerator::GeneratePhysicsMeshWithConfig(xMeshGeometries, xConfig);

	if (m_pxPhysicsMeshAsset)
	{
		Flux_MeshGeometry* pxGeometry = m_pxPhysicsMeshAsset->GetGeometry();
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
}

void Zenith_ModelComponent::ClearPhysicsMesh()
{
	// Just clear the pointer - registry manages asset deletion
	m_pxPhysicsMeshAsset = nullptr;
}

Flux_MeshGeometry* Zenith_ModelComponent::GetPhysicsMesh() const
{
	return m_pxPhysicsMeshAsset ? m_pxPhysicsMeshAsset->GetGeometry() : nullptr;
}

void Zenith_ModelComponent::DebugDrawPhysicsMesh()
{
	Flux_MeshGeometry* pxPhysicsMesh = GetPhysicsMesh();
	if (!m_bDebugDrawPhysicsMesh || !pxPhysicsMesh)
	{
		return;
	}

	if (!m_xParentEntity.HasComponent<Zenith_TransformComponent>())
	{
		return;
	}

	Zenith_TransformComponent& xTransform = m_xParentEntity.GetComponent<Zenith_TransformComponent>();
	Zenith_Maths::Vector3 xScale;
	xTransform.GetScale(xScale);

	Zenith_Maths::Matrix4 xModelMatrix;
	xTransform.BuildModelMatrix(xModelMatrix);

	Zenith_Log(LOG_CATEGORY_PHYSICS, "DebugDraw: Entity scale (%.3f, %.3f, %.3f), verts=%u",
		xScale.x, xScale.y, xScale.z, pxPhysicsMesh->GetNumVerts());

	Zenith_PhysicsMeshGenerator::DebugDrawPhysicsMesh(pxPhysicsMesh, xModelMatrix, m_xDebugDrawColor);
}

// Editor code for RenderPropertiesPanel and AssignTextureToSlot
// is in Zenith_ModelComponent_Editor.cpp
