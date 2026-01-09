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
		// Initialize with skeleton instance for new model instance system
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
	else if (m_xMeshEntries.GetSize() > 0)
	{
		// Legacy: Initialize with first mesh that has bones
		for (uint32_t u = 0; u < m_xMeshEntries.GetSize(); u++)
		{
			Flux_MeshGeometry* pxGeometry = m_xMeshEntries.Get(u).m_pxGeometry;
			if (pxGeometry && pxGeometry->GetNumBones() > 0)
			{
				m_pxAnimController->Initialize(pxGeometry);
				Zenith_Log(LOG_CATEGORY_ANIMATION, "Created animation controller for legacy mesh (bones: %u)",
					pxGeometry->GetNumBones());
				break;
			}
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

//=============================================================================
// Editor UI
//=============================================================================

#ifdef ZENITH_TOOLS
void Zenith_ModelComponent::RenderTextureSlot(const char* szLabel, Flux_MaterialAsset& xMaterial, uint32_t uMeshIdx, TextureSlotType eSlot)
{
	ImGui::PushID(szLabel);

	const Flux_Texture* pxCurrentTexture = nullptr;
	std::string strCurrentPath;
	switch (eSlot)
	{
	case TEXTURE_SLOT_DIFFUSE:
		pxCurrentTexture = xMaterial.GetDiffuseTexture();
		strCurrentPath = xMaterial.GetDiffuseTextureRef().GetPath();
		break;
	case TEXTURE_SLOT_NORMAL:
		pxCurrentTexture = xMaterial.GetNormalTexture();
		strCurrentPath = xMaterial.GetNormalTextureRef().GetPath();
		break;
	case TEXTURE_SLOT_ROUGHNESS_METALLIC:
		pxCurrentTexture = xMaterial.GetRoughnessMetallicTexture();
		strCurrentPath = xMaterial.GetRoughnessMetallicTextureRef().GetPath();
		break;
	case TEXTURE_SLOT_OCCLUSION:
		pxCurrentTexture = xMaterial.GetOcclusionTexture();
		strCurrentPath = xMaterial.GetOcclusionTextureRef().GetPath();
		break;
	case TEXTURE_SLOT_EMISSIVE:
		pxCurrentTexture = xMaterial.GetEmissiveTexture();
		strCurrentPath = xMaterial.GetEmissiveTextureRef().GetPath();
		break;
	}

	std::string strTextureName = "(none)";
	if (pxCurrentTexture && pxCurrentTexture->m_xVRAMHandle.IsValid())
	{
		if (!strCurrentPath.empty())
		{
			std::filesystem::path xPath(strCurrentPath);
			strTextureName = xPath.filename().string();
		}
		else
		{
			strTextureName = "(loaded)";
		}
	}

	ImGui::Text("%s:", szLabel);
	ImGui::SameLine();

	ImVec2 xButtonSize(150, 20);
	ImGui::Button(strTextureName.c_str(), xButtonSize);

	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload(DRAGDROP_PAYLOAD_TEXTURE))
		{
			const DragDropFilePayload* pFilePayload =
				static_cast<const DragDropFilePayload*>(pPayload->Data);

			Zenith_Log(LOG_CATEGORY_MESH, "Texture dropped on %s: %s",
				szLabel, pFilePayload->m_szFilePath);

			AssignTextureToSlot(pFilePayload->m_szFilePath, uMeshIdx, eSlot);
		}

		ImGui::EndDragDropTarget();
	}

	if (ImGui::IsItemHovered())
	{
		if (!strCurrentPath.empty())
		{
			ImGui::SetTooltip("Drop a .ztxtr texture here\nPath: %s", strCurrentPath.c_str());
		}
		else
		{
			ImGui::SetTooltip("Drop a .ztxtr texture here\nCurrent: %s", strTextureName.c_str());
		}
	}

	ImGui::PopID();
}

void Zenith_ModelComponent::AssignTextureToSlot(const char* szFilePath, uint32_t uMeshIdx, TextureSlotType eSlot)
{
	Zenith_AssetHandler::TextureData xTexData =
		Zenith_AssetHandler::LoadTexture2DFromFile(szFilePath);
	Flux_Texture* pxTexture = Zenith_AssetHandler::AddTexture(xTexData);
	xTexData.FreeAllocatedData();

	if (!pxTexture)
	{
		Zenith_Error(LOG_CATEGORY_MESH, "Failed to load texture: %s", szFilePath);
		return;
	}

	pxTexture->m_strSourcePath = szFilePath;
	Zenith_Log(LOG_CATEGORY_MESH, "Loaded texture from: %s", szFilePath);

	Flux_MaterialAsset* pxOldMaterial = m_xMeshEntries.Get(uMeshIdx).m_pxMaterial;

	Flux_MaterialAsset* pxNewMaterial = Flux_MaterialAsset::Create("Material_" + std::to_string(uMeshIdx));
	if (!pxNewMaterial)
	{
		Zenith_Error(LOG_CATEGORY_MATERIAL, "Failed to create new material instance");
		return;
	}

	Zenith_Log(LOG_CATEGORY_MATERIAL, "Created new material instance");

	if (pxOldMaterial)
	{
		if (!pxOldMaterial->GetDiffuseTextureRef().GetPath().empty())
			pxNewMaterial->SetDiffuseTextureRef(pxOldMaterial->GetDiffuseTextureRef());
		if (!pxOldMaterial->GetNormalTextureRef().GetPath().empty())
			pxNewMaterial->SetNormalTextureRef(pxOldMaterial->GetNormalTextureRef());
		if (!pxOldMaterial->GetRoughnessMetallicTextureRef().GetPath().empty())
			pxNewMaterial->SetRoughnessMetallicTextureRef(pxOldMaterial->GetRoughnessMetallicTextureRef());
		if (!pxOldMaterial->GetOcclusionTextureRef().GetPath().empty())
			pxNewMaterial->SetOcclusionTextureRef(pxOldMaterial->GetOcclusionTextureRef());
		if (!pxOldMaterial->GetEmissiveTextureRef().GetPath().empty())
			pxNewMaterial->SetEmissiveTextureRef(pxOldMaterial->GetEmissiveTextureRef());

		pxNewMaterial->SetBaseColor(pxOldMaterial->GetBaseColor());
	}

	TextureRef xRef;
	xRef.SetFromPath(szFilePath);
	switch (eSlot)
	{
	case TEXTURE_SLOT_DIFFUSE:
		pxNewMaterial->SetDiffuseTextureRef(xRef);
		Zenith_Log(LOG_CATEGORY_MATERIAL, "Set diffuse texture");
		break;
	case TEXTURE_SLOT_NORMAL:
		pxNewMaterial->SetNormalTextureRef(xRef);
		Zenith_Log(LOG_CATEGORY_MATERIAL, "Set normal texture");
		break;
	case TEXTURE_SLOT_ROUGHNESS_METALLIC:
		pxNewMaterial->SetRoughnessMetallicTextureRef(xRef);
		Zenith_Log(LOG_CATEGORY_MATERIAL, "Set roughness/metallic texture");
		break;
	case TEXTURE_SLOT_OCCLUSION:
		pxNewMaterial->SetOcclusionTextureRef(xRef);
		Zenith_Log(LOG_CATEGORY_MATERIAL, "Set occlusion texture");
		break;
	case TEXTURE_SLOT_EMISSIVE:
		pxNewMaterial->SetEmissiveTextureRef(xRef);
		Zenith_Log(LOG_CATEGORY_MATERIAL, "Set emissive texture");
		break;
	}

	m_xMeshEntries.Get(uMeshIdx).m_pxMaterial = pxNewMaterial;
}

void Zenith_ModelComponent::RenderPropertiesPanel()
{
	if (ImGui::CollapsingHeader("Model", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Checkbox("Draw Physics Mesh", &m_bDebugDrawPhysicsMesh);

		ImGui::Separator();

		// Show which system is being used
		if (m_pxModelInstance)
		{
			ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Using: New Model Instance System");
			ImGui::Text("Model Path: %s", m_strModelPath.c_str());
			ImGui::Text("Meshes: %u", m_pxModelInstance->GetNumMeshes());
			ImGui::Text("Has Skeleton: %s", m_pxModelInstance->HasSkeleton() ? "Yes" : "No");
		}
		else if (m_xMeshEntries.GetSize() > 0)
		{
			ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "Using: Procedural Mesh Entries");
		}
		else
		{
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No model loaded");
		}

		ImGui::Separator();

		// Model drop target - drag .zmodel files here
		{
			ImGui::Text("Model:");
			ImGui::SameLine();

			std::string strModelName = "(none)";
			if (m_pxModelInstance && !m_strModelPath.empty())
			{
				std::filesystem::path xPath(m_strModelPath);
				strModelName = xPath.filename().string();
			}

			ImVec2 xButtonSize(200, 20);
			ImGui::Button(strModelName.c_str(), xButtonSize);

			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload(DRAGDROP_PAYLOAD_MODEL))
				{
					const DragDropFilePayload* pFilePayload =
						static_cast<const DragDropFilePayload*>(pPayload->Data);

					Zenith_Log(LOG_CATEGORY_MESH, "Model dropped: %s", pFilePayload->m_szFilePath);
					LoadModel(pFilePayload->m_szFilePath);
				}
				ImGui::EndDragDropTarget();
			}

			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Drop a .zmodel file here to load it");
			}

			ImGui::SameLine();
			if (ImGui::Button("Clear##ClearModel"))
			{
				ClearModel();
			}
		}

		// Load model from .zmodel file (new system) - manual path entry
		if (ImGui::TreeNode("Load Model (Manual Path)"))
		{
			static char s_szModelPath[512] = "";
			ImGui::InputText("Model Path", s_szModelPath, sizeof(s_szModelPath));

			if (ImGui::Button("Load Model"))
			{
				if (strlen(s_szModelPath) > 0)
				{
					LoadModel(s_szModelPath);
				}
			}

			ImGui::TreePop();
		}

		// Animation section for new model instance system
		if (m_pxModelInstance && m_pxModelInstance->HasSkeleton() && ImGui::TreeNode("Animations (.zanim)"))
		{
			// Animation drop target - drag .zanim files here
			{
				ImVec2 xDropZoneSize(ImGui::GetContentRegionAvail().x, 30);
				ImGui::Button("Drop .zanim file here to add animation", xDropZoneSize);

				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload(DRAGDROP_PAYLOAD_ANIMATION))
					{
						const DragDropFilePayload* pFilePayload =
							static_cast<const DragDropFilePayload*>(pPayload->Data);

						Zenith_Log(LOG_CATEGORY_ANIMATION, "Animation dropped: %s", pFilePayload->m_szFilePath);

						Flux_AnimationController* pxController = GetOrCreateAnimationController();
						if (pxController)
						{
							Flux_AnimationClip* pxClip = Flux_AnimationClip::LoadFromZanimFile(pFilePayload->m_szFilePath);
							if (pxClip)
							{
								pxController->GetClipCollection().AddClip(pxClip);
								Zenith_Log(LOG_CATEGORY_ANIMATION, "Loaded animation: %s", pxClip->GetName().c_str());
							}
							else
							{
								Zenith_Error(LOG_CATEGORY_ANIMATION, "Failed to load animation from: %s", pFilePayload->m_szFilePath);
							}
						}
					}
					ImGui::EndDragDropTarget();
				}
			}

			ImGui::Separator();

			// Manual path entry section
			if (ImGui::TreeNode("Load Animation (Manual Path)"))
			{
				static char s_szAnimPath[512] = "";
				ImGui::InputText("Animation Path", s_szAnimPath, sizeof(s_szAnimPath));

				if (ImGui::Button("Load .zanim"))
				{
					if (strlen(s_szAnimPath) > 0)
					{
						Flux_AnimationController* pxController = GetOrCreateAnimationController();
						if (pxController)
						{
							// Load from .zanim file using our new loader
							Flux_AnimationClip* pxClip = Flux_AnimationClip::LoadFromZanimFile(s_szAnimPath);
							if (pxClip)
							{
								pxController->GetClipCollection().AddClip(pxClip);
								Zenith_Log(LOG_CATEGORY_ANIMATION, "Loaded animation from: %s", s_szAnimPath);
							}
							else
							{
								Zenith_Error(LOG_CATEGORY_ANIMATION, "Failed to load animation from: %s", s_szAnimPath);
							}
						}
					}
				}

				ImGui::SameLine();
				if (ImGui::Button("Browse...##AnimBrowse"))
				{
#ifdef _WIN32
					OPENFILENAMEA ofn = { 0 };
					char szFile[512] = "";
					ofn.lStructSize = sizeof(ofn);
					ofn.lpstrFilter = "Animation Files (*.zanim)\0*.zanim\0All Files (*.*)\0*.*\0";
					ofn.lpstrFile = szFile;
					ofn.nMaxFile = sizeof(szFile);
					ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
					if (GetOpenFileNameA(&ofn))
					{
						strncpy(s_szAnimPath, szFile, sizeof(s_szAnimPath) - 1);
					}
#endif
				}

				ImGui::TreePop();
			}

			// Show animation controller info
			Flux_AnimationController* pxController = GetAnimationController();
			if (pxController)
			{
				ImGui::Separator();

				const Flux_AnimationClipCollection& xClips = pxController->GetClipCollection();
				ImGui::Text("Loaded Clips: %u", xClips.GetClipCount());

				// List clips and allow playing them
				if (xClips.GetClipCount() > 0)
				{
					for (const Flux_AnimationClip* pxClip : xClips.GetClips())
					{
						if (pxClip)
						{
							ImGui::PushID(pxClip);
							if (ImGui::Button(pxClip->GetName().c_str()))
							{
								pxController->PlayClip(pxClip->GetName());
								Zenith_Log(LOG_CATEGORY_ANIMATION, "Playing animation: %s", pxClip->GetName().c_str());
							}
							ImGui::SameLine();
							ImGui::Text("(%.2fs)", pxClip->GetDuration());
							ImGui::PopID();
						}
					}
				}

				ImGui::Separator();

				// Playback controls
				bool bPaused = pxController->IsPaused();
				if (ImGui::Checkbox("Paused##NewModel", &bPaused))
				{
					pxController->SetPaused(bPaused);
				}

				float fSpeed = pxController->GetPlaybackSpeed();
				if (ImGui::SliderFloat("Speed##NewModel", &fSpeed, 0.0f, 2.0f))
				{
					pxController->SetPlaybackSpeed(fSpeed);
				}

				if (ImGui::Button("Stop"))
				{
					pxController->Stop();
				}

				// Editor preview: Update animation even when not in Play mode
				// This allows previewing animations in the editor
				if (pxController->HasAnimationContent())
				{
					float fPreviewDt = Zenith_Core::GetDt();
					pxController->Update(fPreviewDt);

					// Also update the model instance skeleton if using new system
					if (m_pxModelInstance && m_pxModelInstance->HasSkeleton())
					{
						m_pxModelInstance->UpdateAnimation();
					}
				}
			}
			else
			{
				ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Load an animation to create controller");
			}

			ImGui::TreePop();
		}

		// Animation loading section (only for procedural mesh entries)
		if (m_xMeshEntries.GetSize() > 0 && ImGui::TreeNode("Animations"))
		{
			static char s_szAnimFilePath[512] = "";
			ImGui::InputText("Animation File (.fbx/.gltf)", s_szAnimFilePath, sizeof(s_szAnimFilePath));

			static int s_iTargetMeshIndex = 0;
			int iMaxIndex = static_cast<int>(m_xMeshEntries.GetSize()) - 1;
			ImGui::SliderInt("Target Mesh Index", &s_iTargetMeshIndex, 0, iMaxIndex);

			if (ImGui::Button("Load Animation"))
			{
				if (strlen(s_szAnimFilePath) > 0 && s_iTargetMeshIndex >= 0 && s_iTargetMeshIndex < static_cast<int>(m_xMeshEntries.GetSize()))
				{
					Flux_MeshGeometry& xMesh = GetMeshGeometryAtIndex(s_iTargetMeshIndex);
					if (xMesh.GetNumBones() > 0)
					{
						if (xMesh.m_pxAnimation)
						{
							delete xMesh.m_pxAnimation;
						}
						xMesh.m_pxAnimation = new Flux_MeshAnimation(s_szAnimFilePath, xMesh);
						Zenith_Log(LOG_CATEGORY_ANIMATION, "Loaded animation from: %s for mesh %d", s_szAnimFilePath, s_iTargetMeshIndex);
					}
					else
					{
						Zenith_Error(LOG_CATEGORY_ANIMATION, "Cannot load animation: mesh %d has no bones", s_iTargetMeshIndex);
					}
				}
			}

			if (ImGui::Button("Load Animation for All Meshes"))
			{
				if (strlen(s_szAnimFilePath) > 0)
				{
					for (uint32_t u = 0; u < m_xMeshEntries.GetSize(); u++)
					{
						Flux_MeshGeometry& xMesh = GetMeshGeometryAtIndex(u);
						if (xMesh.GetNumBones() > 0)
						{
							if (xMesh.m_pxAnimation)
							{
								delete xMesh.m_pxAnimation;
							}
							xMesh.m_pxAnimation = new Flux_MeshAnimation(s_szAnimFilePath, xMesh);
							Zenith_Log(LOG_CATEGORY_ANIMATION, "Loaded animation for mesh %u", u);
						}
					}
				}
			}

			ImGui::Separator();
			for (uint32_t u = 0; u < m_xMeshEntries.GetSize(); u++)
			{
				Flux_MeshGeometry& xMesh = GetMeshGeometryAtIndex(u);
				if (xMesh.m_pxAnimation)
				{
					ImGui::Text("Mesh %u: Animation loaded (%s)", u, xMesh.m_pxAnimation->GetSourcePath().c_str());
				}
				else if (xMesh.GetNumBones() > 0)
				{
					ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Mesh %u: Has %u bones, no animation", u, xMesh.GetNumBones());
				}
			}

			// Animation Controller section
			Flux_AnimationController* pxController = GetAnimationController();
			if (pxController)
			{
				ImGui::Separator();
				ImGui::Text("Animation Controller");

				bool bPaused = pxController->IsPaused();
				if (ImGui::Checkbox("Paused", &bPaused))
				{
					pxController->SetPaused(bPaused);
				}

				float fSpeed = pxController->GetPlaybackSpeed();
				if (ImGui::SliderFloat("Playback Speed", &fSpeed, 0.0f, 2.0f))
				{
					pxController->SetPlaybackSpeed(fSpeed);
				}

				const Flux_AnimationClipCollection& xClips = pxController->GetClipCollection();
				ImGui::Text("Clips loaded: %u", static_cast<uint32_t>(xClips.GetClips().size()));

				if (!xClips.GetClips().empty())
				{
					if (ImGui::TreeNode("Clip List"))
					{
						for (const Flux_AnimationClip* pxClip : xClips.GetClips())
						{
							if (pxClip && ImGui::Selectable(pxClip->GetName().c_str()))
							{
								pxController->PlayClip(pxClip->GetName());
							}
						}
						ImGui::TreePop();
					}
				}
			}

			ImGui::TreePop();
		}

		ImGui::Separator();
		ImGui::Text("Mesh Entries: %u", GetNumMeshes());

		// Display each procedural mesh entry with its material
		for (uint32_t uMeshIdx = 0; uMeshIdx < m_xMeshEntries.GetSize(); ++uMeshIdx)
		{
			ImGui::PushID(uMeshIdx);

			Flux_MaterialAsset& xMaterial = GetMaterialAtIndex(uMeshIdx);

			if (ImGui::TreeNode("MeshEntry", "Mesh Entry %u", uMeshIdx))
			{
				Flux_MeshGeometry& xGeom = GetMeshGeometryAtIndex(uMeshIdx);
				if (!xGeom.m_strSourcePath.empty())
				{
					ImGui::TextWrapped("Source: %s", xGeom.m_strSourcePath.c_str());
				}

				RenderTextureSlot("Diffuse", xMaterial, uMeshIdx, TEXTURE_SLOT_DIFFUSE);
				RenderTextureSlot("Normal", xMaterial, uMeshIdx, TEXTURE_SLOT_NORMAL);
				RenderTextureSlot("Roughness/Metallic", xMaterial, uMeshIdx, TEXTURE_SLOT_ROUGHNESS_METALLIC);
				RenderTextureSlot("Occlusion", xMaterial, uMeshIdx, TEXTURE_SLOT_OCCLUSION);
				RenderTextureSlot("Emissive", xMaterial, uMeshIdx, TEXTURE_SLOT_EMISSIVE);

				ImGui::TreePop();
			}

			ImGui::PopID();
		}
	}
}
#endif
