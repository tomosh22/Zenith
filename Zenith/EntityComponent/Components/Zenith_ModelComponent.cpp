#include "Zenith.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_ComponentMeta.h"
#include "DataStream/Zenith_DataStream.h"
#include "Flux/MeshAnimation/Flux_AnimationController.h"
#include "Flux/Flux_ModelInstance.h"
#include "Flux/MeshGeometry/Flux_MeshInstance.h"
#include "Flux/MeshAnimation/Flux_SkeletonInstance.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "Core/Zenith_Core.h"

ZENITH_REGISTER_COMPONENT(Zenith_ModelComponent, "Model")


// Serialization version for ModelComponent
// Version 3: New model instance system with .zmodel path
// Version 4: GUID-based model references
static constexpr uint32_t MODEL_COMPONENT_SERIALIZE_VERSION = 4;
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
	, m_pxAnimController(xOther.m_pxAnimController)
	, m_xModel(std::move(xOther.m_xModel))
	, m_strModelPath(std::move(xOther.m_strModelPath))
	, m_xMeshEntries(std::move(xOther.m_xMeshEntries))
	, m_pxPhysicsMesh(xOther.m_pxPhysicsMesh)
	, m_bDebugDrawPhysicsMesh(xOther.m_bDebugDrawPhysicsMesh)
	, m_xDebugDrawColor(xOther.m_xDebugDrawColor)
{
	// Nullify source pointers so its destructor doesn't delete our resources
	xOther.m_pxModelInstance = nullptr;
	xOther.m_pxAnimController = nullptr;
	xOther.m_pxPhysicsMesh = nullptr;
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
		m_pxAnimController = xOther.m_pxAnimController;
		m_xModel = std::move(xOther.m_xModel);
		m_strModelPath = std::move(xOther.m_strModelPath);
		m_xMeshEntries = std::move(xOther.m_xMeshEntries);
		m_pxPhysicsMesh = xOther.m_pxPhysicsMesh;
		m_bDebugDrawPhysicsMesh = xOther.m_bDebugDrawPhysicsMesh;
		m_xDebugDrawColor = xOther.m_xDebugDrawColor;

		// Nullify source pointers
		xOther.m_pxModelInstance = nullptr;
		xOther.m_pxAnimController = nullptr;
		xOther.m_pxPhysicsMesh = nullptr;
	}
	return *this;
}

//=============================================================================
// New Model Instance API
//=============================================================================

void Zenith_ModelComponent::LoadModel(const std::string& strPath)
{
	Zenith_Log(LOG_CATEGORY_MESH, "LoadModel called with path: %s", strPath.c_str());

	// Clear any existing model
	ClearModel();

	// Load model asset from file
	Zenith_ModelAsset* pxAsset = Zenith_ModelAsset::LoadFromFile(strPath.c_str());
	if (!pxAsset)
	{
		Zenith_Error(LOG_CATEGORY_MESH, "Failed to load model asset from: %s", strPath.c_str());
		return;
	}

	Zenith_Log(LOG_CATEGORY_MESH, "Model asset loaded: %s (meshes: %u, has skeleton: %s)",
		pxAsset->GetName().c_str(), pxAsset->GetNumMeshes(),
		pxAsset->HasSkeleton() ? "yes" : "no");

	// Create model instance from asset
	m_pxModelInstance = Flux_ModelInstance::CreateFromAsset(pxAsset);
	if (!m_pxModelInstance)
	{
		Zenith_Error(LOG_CATEGORY_MESH, "Failed to create model instance from asset: %s", strPath.c_str());
		delete pxAsset;
		return;
	}

	// Store path for serialization
	m_strModelPath = strPath;

	// Also populate GUID reference if not already set
	if (!m_xModel.IsSet())
	{
		m_xModel.SetFromPath(strPath);
	}

	// Detailed logging for debugging
	Zenith_Log(LOG_CATEGORY_MESH, "SUCCESS: Loaded model from: %s", strPath.c_str());
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
	// Delete animation controller (owned by component)
	if (m_pxAnimController)
	{
		delete m_pxAnimController;
		m_pxAnimController = nullptr;
	}

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

Flux_MaterialAsset* Zenith_ModelComponent::GetMaterial(uint32_t uIndex) const
{
	if (m_pxModelInstance)
	{
		return m_pxModelInstance->GetMaterial(uIndex);
	}
	// Fall back to procedural mesh entries
	if (uIndex < m_xMeshEntries.GetSize())
	{
		return m_xMeshEntries.Get(uIndex).m_pxMaterial;
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
// Animation System Integration
//=============================================================================

Flux_AnimationController* Zenith_ModelComponent::GetOrCreateAnimationController()
{
	if (m_pxAnimController)
	{
		return m_pxAnimController;
	}

	// Create new controller at component level
	m_pxAnimController = new Flux_AnimationController();

	if (m_pxModelInstance && m_pxModelInstance->HasSkeleton())
	{
		// Initialize with skeleton instance
		Flux_SkeletonInstance* pxSkeleton = m_pxModelInstance->GetSkeletonInstance();
		if (pxSkeleton)
		{
			m_pxAnimController->Initialize(pxSkeleton);
			Zenith_Log(LOG_CATEGORY_ANIMATION, "Created animation controller for model instance (bones: %u)",
				pxSkeleton->GetNumBones());
		}
		else
		{
			Zenith_Log(LOG_CATEGORY_ANIMATION, "Model has skeleton but GetSkeletonInstance() returned null");
		}
	}

	return m_pxAnimController;
}

void Zenith_ModelComponent::Update(float fDt)
{
	// Update world matrices for animation controller
	UpdateAnimationWorldMatrix();

	// Update component-level animation controller
	if (m_pxAnimController)
	{
		m_pxAnimController->Update(fDt);
	}

	// If using new model instance with skeleton, update animation
	if (m_pxModelInstance && m_pxModelInstance->HasSkeleton())
	{
		m_pxModelInstance->UpdateAnimation();
	}
}

void Zenith_ModelComponent::PlayAnimation(const std::string& strClipName, float fBlendTime)
{
	if (m_pxAnimController)
	{
		m_pxAnimController->PlayClip(strClipName, fBlendTime);
	}
}

void Zenith_ModelComponent::StopAnimations()
{
	if (m_pxAnimController)
	{
		m_pxAnimController->Stop();
	}
}

void Zenith_ModelComponent::SetAnimationsPaused(bool bPaused)
{
	if (m_pxAnimController)
	{
		m_pxAnimController->SetPaused(bPaused);
	}
}

bool Zenith_ModelComponent::AreAnimationsPaused() const
{
	if (m_pxAnimController)
	{
		return m_pxAnimController->IsPaused();
	}
	return false;
}

void Zenith_ModelComponent::SetAnimationPlaybackSpeed(float fSpeed)
{
	if (m_pxAnimController)
	{
		m_pxAnimController->SetPlaybackSpeed(fSpeed);
	}
}

float Zenith_ModelComponent::GetAnimationPlaybackSpeed() const
{
	if (m_pxAnimController)
	{
		return m_pxAnimController->GetPlaybackSpeed();
	}
	return 1.0f;
}

void Zenith_ModelComponent::SetAnimationFloat(const std::string& strName, float fValue)
{
	if (m_pxAnimController)
	{
		m_pxAnimController->SetFloat(strName, fValue);
	}
}

void Zenith_ModelComponent::SetAnimationInt(const std::string& strName, int32_t iValue)
{
	if (m_pxAnimController)
	{
		m_pxAnimController->SetInt(strName, iValue);
	}
}

void Zenith_ModelComponent::SetAnimationBool(const std::string& strName, bool bValue)
{
	if (m_pxAnimController)
	{
		m_pxAnimController->SetBool(strName, bValue);
	}
}

void Zenith_ModelComponent::SetAnimationTrigger(const std::string& strName)
{
	if (m_pxAnimController)
	{
		m_pxAnimController->SetTrigger(strName);
	}
}

void Zenith_ModelComponent::SetIKTarget(const std::string& strChainName, const Zenith_Maths::Vector3& xPosition, float fWeight)
{
	if (m_pxAnimController)
	{
		m_pxAnimController->SetIKTarget(strChainName, xPosition, fWeight);
	}
}

void Zenith_ModelComponent::ClearIKTarget(const std::string& strChainName)
{
	if (m_pxAnimController)
	{
		m_pxAnimController->ClearIKTarget(strChainName);
	}
}

void Zenith_ModelComponent::UpdateAnimationWorldMatrix()
{
	if (!m_xParentEntity.HasComponent<Zenith_TransformComponent>())
	{
		return;
	}

	Zenith_TransformComponent& xTransform = m_xParentEntity.GetComponent<Zenith_TransformComponent>();
	Zenith_Maths::Matrix4 xWorldMatrix;
	xTransform.BuildModelMatrix(xWorldMatrix);

	// Update component-level controller
	if (m_pxAnimController)
	{
		m_pxAnimController->SetWorldMatrix(xWorldMatrix);
	}
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

		// TODO: Serialize animation controller state if needed
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
			if (xEntry.m_pxMaterial)
			{
				xEntry.m_pxMaterial->WriteToDataStream(xStream);
			}
			else
			{
				Flux_MaterialAsset* pxEmptyMat = Flux_MaterialAsset::Create("Empty");
				pxEmptyMat->WriteToDataStream(xStream);
				delete pxEmptyMat;
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
	}
	else
	{
		// Legacy system: Read mesh entry data to keep stream aligned
		// Note: Procedural meshes can't be reconstructed from serialized data -
		// they must be regenerated at runtime (e.g., by behaviour scripts)
		u_int uNumEntries;
		xStream >> uNumEntries;

		for (u_int u = 0; u < uNumEntries; u++)
		{
			// Read and discard mesh path
			std::string strMeshPath;
			xStream >> strMeshPath;

			// Read and discard material data
			Flux_MaterialAsset* pxTempMat = Flux_MaterialAsset::Create("Temp");
			pxTempMat->ReadFromDataStream(xStream);
			delete pxTempMat;

			// Read and discard animation path
			std::string strAnimPath;
			xStream >> strAnimPath;
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

	// Generate the physics mesh
	m_pxPhysicsMesh = Zenith_PhysicsMeshGenerator::GeneratePhysicsMeshWithConfig(xMeshGeometries, xConfig);

	if (m_pxPhysicsMesh)
	{
		Zenith_Log(LOG_CATEGORY_PHYSICS, "Generated physics mesh for model: %u verts, %u tris",
			m_pxPhysicsMesh->GetNumVerts(),
			m_pxPhysicsMesh->GetNumIndices() / 3);

		if (m_pxPhysicsMesh->GetNumVerts() > 0)
		{
			Zenith_Maths::Vector3& v0 = m_pxPhysicsMesh->m_pxPositions[0];
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
	if (m_pxPhysicsMesh)
	{
		delete m_pxPhysicsMesh;
		m_pxPhysicsMesh = nullptr;
	}
}

void Zenith_ModelComponent::DebugDrawPhysicsMesh()
{
	if (!m_bDebugDrawPhysicsMesh || !m_pxPhysicsMesh)
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
		xScale.x, xScale.y, xScale.z, m_pxPhysicsMesh->GetNumVerts());

	Zenith_PhysicsMeshGenerator::DebugDrawPhysicsMesh(m_pxPhysicsMesh, xModelMatrix, m_xDebugDrawColor);
}

// Editor code for RenderPropertiesPanel, RenderTextureSlot, and AssignTextureToSlot
// is in Zenith_ModelComponent_Editor.cpp
