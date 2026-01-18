#include "Zenith.h"

#include "EntityComponent/Components/Zenith_InstancedMeshComponent.h"
#include "EntityComponent/Zenith_ComponentMeta.h"
#include "Flux/InstancedMeshes/Flux_InstancedMeshes.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_MeshAsset.h"

ZENITH_REGISTER_COMPONENT(Zenith_InstancedMeshComponent, "InstancedMesh")

//=============================================================================
// Constructor / Destructor
//=============================================================================

Zenith_InstancedMeshComponent::Zenith_InstancedMeshComponent(Zenith_Entity& xEntity)
	: m_xParentEntity(xEntity)
{
}

Zenith_InstancedMeshComponent::~Zenith_InstancedMeshComponent()
{
	// Unregister from renderer
	if (m_pxInstanceGroup != nullptr)
	{
		Flux_InstancedMeshes::UnregisterInstanceGroup(m_pxInstanceGroup);
		delete m_pxInstanceGroup;
		m_pxInstanceGroup = nullptr;
	}

	// Clean up owned resources (non-registry)
	delete m_pxOwnedAnimTexture;
	m_pxOwnedAnimTexture = nullptr;

	// Clean up mesh instance (we own this, created from the asset)
	if (m_pxOwnedMeshInstance != nullptr)
	{
		m_pxOwnedMeshInstance->Destroy();
		delete m_pxOwnedMeshInstance;
		m_pxOwnedMeshInstance = nullptr;
	}

	// m_xMeshAsset and m_xMaterial handles auto-release when destroyed
}

//=============================================================================
// Move Semantics
//=============================================================================

Zenith_InstancedMeshComponent::Zenith_InstancedMeshComponent(Zenith_InstancedMeshComponent&& xOther) noexcept
	: m_xParentEntity(xOther.m_xParentEntity)
	, m_pxInstanceGroup(xOther.m_pxInstanceGroup)
	, m_xMeshAsset(std::move(xOther.m_xMeshAsset))
	, m_xMaterial(std::move(xOther.m_xMaterial))
	, m_pxOwnedMeshInstance(xOther.m_pxOwnedMeshInstance)
	, m_pxOwnedAnimTexture(xOther.m_pxOwnedAnimTexture)
	, m_fAnimationDuration(xOther.m_fAnimationDuration)
	, m_fAnimationSpeed(xOther.m_fAnimationSpeed)
	, m_bAnimationsPaused(xOther.m_bAnimationsPaused)
{
	xOther.m_pxInstanceGroup = nullptr;
	xOther.m_pxOwnedMeshInstance = nullptr;
	xOther.m_pxOwnedAnimTexture = nullptr;
}

Zenith_InstancedMeshComponent& Zenith_InstancedMeshComponent::operator=(Zenith_InstancedMeshComponent&& xOther) noexcept
{
	if (this != &xOther)
	{
		// Clean up existing resources
		if (m_pxInstanceGroup != nullptr)
		{
			Flux_InstancedMeshes::UnregisterInstanceGroup(m_pxInstanceGroup);
			delete m_pxInstanceGroup;
		}
		delete m_pxOwnedAnimTexture;
		if (m_pxOwnedMeshInstance != nullptr)
		{
			m_pxOwnedMeshInstance->Destroy();
			delete m_pxOwnedMeshInstance;
		}

		// Move from other (handles auto-release old values)
		m_xParentEntity = xOther.m_xParentEntity;
		m_pxInstanceGroup = xOther.m_pxInstanceGroup;
		m_xMeshAsset = std::move(xOther.m_xMeshAsset);
		m_xMaterial = std::move(xOther.m_xMaterial);
		m_pxOwnedMeshInstance = xOther.m_pxOwnedMeshInstance;
		m_pxOwnedAnimTexture = xOther.m_pxOwnedAnimTexture;
		m_fAnimationDuration = xOther.m_fAnimationDuration;
		m_fAnimationSpeed = xOther.m_fAnimationSpeed;
		m_bAnimationsPaused = xOther.m_bAnimationsPaused;

		xOther.m_pxInstanceGroup = nullptr;
		xOther.m_pxOwnedMeshInstance = nullptr;
		xOther.m_pxOwnedAnimTexture = nullptr;
	}
	return *this;
}

//=============================================================================
// Configuration
//=============================================================================

void Zenith_InstancedMeshComponent::SetMesh(Flux_MeshInstance* pxMesh)
{
	EnsureInstanceGroupCreated();
	m_pxInstanceGroup->SetMesh(pxMesh);
}

void Zenith_InstancedMeshComponent::SetMaterial(Zenith_MaterialAsset* pxMaterial)
{
	EnsureInstanceGroupCreated();
	m_xMaterial.Set(pxMaterial);  // Store in handle for ref counting (clears path for procedural)
	m_pxInstanceGroup->SetMaterial(pxMaterial);
}

void Zenith_InstancedMeshComponent::LoadMaterial(const std::string& strPath)
{
	m_xMaterial.SetPath(strPath);  // Store path for serialization
	Zenith_MaterialAsset* pxMaterial = m_xMaterial.Get();
	if (pxMaterial == nullptr)
	{
		Zenith_Error(LOG_CATEGORY_MESH, "[InstancedMeshComponent] Failed to load material: %s", strPath.c_str());
		return;
	}

	EnsureInstanceGroupCreated();
	m_pxInstanceGroup->SetMaterial(pxMaterial);

	Zenith_Log(LOG_CATEGORY_MESH, "[InstancedMeshComponent] Loaded material: %s", strPath.c_str());
}

void Zenith_InstancedMeshComponent::SetAnimationTexture(Flux_AnimationTexture* pxAnimTex)
{
	EnsureInstanceGroupCreated();
	m_pxInstanceGroup->SetAnimationTexture(pxAnimTex);

	// Update animation duration from texture if available
	if (pxAnimTex != nullptr && pxAnimTex->GetNumAnimations() > 0)
	{
		const Flux_AnimationTexture::AnimationInfo* pxInfo = pxAnimTex->GetAnimationInfo(0);
		if (pxInfo != nullptr)
		{
			m_fAnimationDuration = pxInfo->m_fDuration;
		}
	}
}

void Zenith_InstancedMeshComponent::SetBounds(const Zenith_Maths::Vector3& xCenter, float fRadius)
{
	EnsureInstanceGroupCreated();
	Flux_InstanceBounds xBounds;
	xBounds.m_xCenter = xCenter;
	xBounds.m_fRadius = fRadius;
	m_pxInstanceGroup->SetBounds(xBounds);
}

void Zenith_InstancedMeshComponent::LoadMesh(const std::string& strPath)
{
	// Clean up existing mesh instance (we own this)
	if (m_pxOwnedMeshInstance != nullptr)
	{
		m_pxOwnedMeshInstance->Destroy();
		delete m_pxOwnedMeshInstance;
		m_pxOwnedMeshInstance = nullptr;
	}

	// Load mesh asset via handle (handles ref counting automatically)
	m_xMeshAsset.SetPath(strPath);
	Zenith_MeshAsset* pxMeshAsset = m_xMeshAsset.Get();
	if (pxMeshAsset == nullptr)
	{
		Zenith_Error(LOG_CATEGORY_MESH, "[InstancedMeshComponent] Failed to load mesh asset: %s", strPath.c_str());
		return;
	}

	// Create mesh instance for GPU rendering
	m_pxOwnedMeshInstance = Flux_MeshInstance::CreateFromAsset(pxMeshAsset);
	if (m_pxOwnedMeshInstance == nullptr)
	{
		Zenith_Error(LOG_CATEGORY_MESH, "[InstancedMeshComponent] Failed to create mesh instance from asset: %s", strPath.c_str());
		m_xMeshAsset.Clear();
		return;
	}

	// Set on instance group
	SetMesh(m_pxOwnedMeshInstance);

	// Set bounds from mesh asset
	Zenith_Maths::Vector3 xMin = pxMeshAsset->GetBoundsMin();
	Zenith_Maths::Vector3 xMax = pxMeshAsset->GetBoundsMax();
	Zenith_Maths::Vector3 xCenter = (xMin + xMax) * 0.5f;
	float fRadius = glm::length(xMax - xCenter);
	SetBounds(xCenter, fRadius);

	Zenith_Log(LOG_CATEGORY_MESH, "[InstancedMeshComponent] Loaded mesh: %s (%u verts, %u indices)",
		strPath.c_str(), pxMeshAsset->GetNumVerts(), pxMeshAsset->GetNumIndices());
}

void Zenith_InstancedMeshComponent::LoadAnimationTexture(const std::string& strPath)
{
	m_strAnimTexturePath = strPath;

	// Clean up existing
	delete m_pxOwnedAnimTexture;
	m_pxOwnedAnimTexture = nullptr;

	// Load animation texture
	m_pxOwnedAnimTexture = Flux_AnimationTexture::LoadFromFile(strPath);
	if (m_pxOwnedAnimTexture == nullptr)
	{
		Zenith_Error(LOG_CATEGORY_MESH, "[InstancedMeshComponent] Failed to load animation texture: %s", strPath.c_str());
		return;
	}

	// Create GPU resources
	m_pxOwnedAnimTexture->CreateGPUResources();

	// Set on instance group
	SetAnimationTexture(m_pxOwnedAnimTexture);

	Zenith_Log(LOG_CATEGORY_MESH, "[InstancedMeshComponent] Loaded animation texture: %s (%u anims, %u frames)",
		strPath.c_str(), m_pxOwnedAnimTexture->GetNumAnimations(), m_pxOwnedAnimTexture->GetFramesPerAnimation());
}

//=============================================================================
// Instance Spawning
//=============================================================================

uint32_t Zenith_InstancedMeshComponent::SpawnInstance(
	const Zenith_Maths::Vector3& xPosition,
	const Zenith_Maths::Quat& xRotation,
	const Zenith_Maths::Vector3& xScale)
{
	EnsureInstanceGroupCreated();

	uint32_t uID = m_pxInstanceGroup->AddInstance();
	Zenith_Maths::Matrix4 xMatrix = BuildMatrix(xPosition, xRotation, xScale);
	m_pxInstanceGroup->SetInstanceTransform(uID, xMatrix);

	return uID;
}

uint32_t Zenith_InstancedMeshComponent::SpawnInstanceWithMatrix(const Zenith_Maths::Matrix4& xMatrix)
{
	EnsureInstanceGroupCreated();

	uint32_t uID = m_pxInstanceGroup->AddInstance();
	m_pxInstanceGroup->SetInstanceTransform(uID, xMatrix);

	return uID;
}

void Zenith_InstancedMeshComponent::DespawnInstance(uint32_t uInstanceID)
{
	if (m_pxInstanceGroup != nullptr)
	{
		m_pxInstanceGroup->RemoveInstance(uInstanceID);
	}
}

void Zenith_InstancedMeshComponent::ClearInstances()
{
	if (m_pxInstanceGroup != nullptr)
	{
		m_pxInstanceGroup->Clear();
	}
}

void Zenith_InstancedMeshComponent::Reserve(uint32_t uCapacity)
{
	EnsureInstanceGroupCreated();
	m_pxInstanceGroup->Reserve(uCapacity);
}

//=============================================================================
// Per-Instance Control
//=============================================================================

void Zenith_InstancedMeshComponent::SetInstanceTransform(
	uint32_t uInstanceID,
	const Zenith_Maths::Vector3& xPosition,
	const Zenith_Maths::Quat& xRotation,
	const Zenith_Maths::Vector3& xScale)
{
	if (m_pxInstanceGroup != nullptr)
	{
		Zenith_Maths::Matrix4 xMatrix = BuildMatrix(xPosition, xRotation, xScale);
		m_pxInstanceGroup->SetInstanceTransform(uInstanceID, xMatrix);
	}
}

void Zenith_InstancedMeshComponent::SetInstanceMatrix(uint32_t uInstanceID, const Zenith_Maths::Matrix4& xMatrix)
{
	if (m_pxInstanceGroup != nullptr)
	{
		m_pxInstanceGroup->SetInstanceTransform(uInstanceID, xMatrix);
	}
}

void Zenith_InstancedMeshComponent::SetInstanceAnimation(uint32_t uInstanceID, const std::string& strAnimName, float fNormalizedTime)
{
	if (m_pxInstanceGroup == nullptr)
		return;

	Flux_AnimationTexture* pxAnimTex = m_pxInstanceGroup->GetAnimationTexture();
	if (pxAnimTex == nullptr)
		return;

	const Flux_AnimationTexture::AnimationInfo* pxInfo = pxAnimTex->FindAnimation(strAnimName);
	if (pxInfo == nullptr)
	{
		Zenith_Error(LOG_CATEGORY_MESH, "[InstancedMeshComponent] Animation not found: %s", strAnimName.c_str());
		return;
	}

	// Find animation index
	for (uint32_t i = 0; i < pxAnimTex->GetNumAnimations(); ++i)
	{
		if (pxAnimTex->GetAnimationInfo(i) == pxInfo)
		{
			m_pxInstanceGroup->SetInstanceAnimation(uInstanceID, i, fNormalizedTime, pxInfo->m_uFrameCount);
			break;
		}
	}
}

void Zenith_InstancedMeshComponent::SetInstanceAnimationByIndex(uint32_t uInstanceID, uint32_t uAnimIndex, float fNormalizedTime)
{
	if (m_pxInstanceGroup == nullptr)
		return;

	Flux_AnimationTexture* pxAnimTex = m_pxInstanceGroup->GetAnimationTexture();
	if (pxAnimTex == nullptr)
		return;

	const Flux_AnimationTexture::AnimationInfo* pxInfo = pxAnimTex->GetAnimationInfo(uAnimIndex);
	if (pxInfo == nullptr)
		return;

	m_pxInstanceGroup->SetInstanceAnimation(uInstanceID, uAnimIndex, fNormalizedTime, pxInfo->m_uFrameCount);
}

void Zenith_InstancedMeshComponent::SetInstanceAnimationTime(uint32_t uInstanceID, float fNormalizedTime)
{
	if (m_pxInstanceGroup == nullptr)
		return;

	Flux_AnimationTexture* pxAnimTex = m_pxInstanceGroup->GetAnimationTexture();
	if (pxAnimTex == nullptr)
		return;

	// Get current animation info - we need the frame count
	// For simplicity, use animation 0 frame count
	const Flux_AnimationTexture::AnimationInfo* pxInfo = pxAnimTex->GetAnimationInfo(0);
	if (pxInfo == nullptr)
		return;

	m_pxInstanceGroup->SetInstanceAnimation(uInstanceID, 0, fNormalizedTime, pxInfo->m_uFrameCount);
}

void Zenith_InstancedMeshComponent::SetInstanceColor(uint32_t uInstanceID, const Zenith_Maths::Vector4& xColor)
{
	if (m_pxInstanceGroup != nullptr)
	{
		m_pxInstanceGroup->SetInstanceColor(uInstanceID, xColor);
	}
}

void Zenith_InstancedMeshComponent::SetInstanceEnabled(uint32_t uInstanceID, bool bEnabled)
{
	if (m_pxInstanceGroup != nullptr)
	{
		m_pxInstanceGroup->SetInstanceEnabled(uInstanceID, bEnabled);
	}
}

//=============================================================================
// Per-Frame Update
//=============================================================================

void Zenith_InstancedMeshComponent::Update(float fDt)
{
	if (m_pxInstanceGroup == nullptr || m_bAnimationsPaused)
		return;

	float fScaledDt = fDt * m_fAnimationSpeed;
	m_pxInstanceGroup->AdvanceAllAnimations(fScaledDt, m_fAnimationDuration);
}

//=============================================================================
// Accessors
//=============================================================================

uint32_t Zenith_InstancedMeshComponent::GetInstanceCount() const
{
	return m_pxInstanceGroup != nullptr ? m_pxInstanceGroup->GetInstanceCount() : 0;
}

uint32_t Zenith_InstancedMeshComponent::GetVisibleCount() const
{
	return m_pxInstanceGroup != nullptr ? m_pxInstanceGroup->GetVisibleCount() : 0;
}

bool Zenith_InstancedMeshComponent::IsEmpty() const
{
	return m_pxInstanceGroup == nullptr || m_pxInstanceGroup->IsEmpty();
}

Flux_MeshInstance* Zenith_InstancedMeshComponent::GetMesh() const
{
	return m_pxInstanceGroup != nullptr ? m_pxInstanceGroup->GetMesh() : nullptr;
}

Zenith_MaterialAsset* Zenith_InstancedMeshComponent::GetMaterial() const
{
	return m_pxInstanceGroup != nullptr ? m_pxInstanceGroup->GetMaterial() : nullptr;
}

Flux_AnimationTexture* Zenith_InstancedMeshComponent::GetAnimationTexture() const
{
	return m_pxInstanceGroup != nullptr ? m_pxInstanceGroup->GetAnimationTexture() : nullptr;
}

//=============================================================================
// Serialization
//=============================================================================

void Zenith_InstancedMeshComponent::WriteToDataStream(Zenith_DataStream& xStream) const
{
	// Version
	uint32_t uVersion = 4;  // Version 4: serialize instance data (transforms)
	xStream << uVersion;

	// Asset paths (get from handles for registry assets)
	std::string strMeshPath = m_xMeshAsset.GetPath();
	std::string strMaterialPath = m_xMaterial.GetPath();
	xStream << strMeshPath;
	xStream << m_strAnimTexturePath;
	xStream << strMaterialPath;

	// For procedural materials (no path), serialize the material data directly
	bool bHasProceduralMaterial = strMaterialPath.empty() && m_xMaterial.IsLoaded();
	xStream << bHasProceduralMaterial;
	if (bHasProceduralMaterial)
	{
		Zenith_MaterialAsset* pxMaterial = m_xMaterial.Get();
		if (pxMaterial)
		{
			pxMaterial->WriteToDataStream(xStream);
		}
	}

	// Animation settings
	xStream << m_fAnimationDuration;
	xStream << m_fAnimationSpeed;
	xStream << m_bAnimationsPaused;

	// Instance data (version 4+)
	uint32_t uInstanceCount = GetInstanceCount();
	xStream << uInstanceCount;

	// Serialize instance transforms
	if (uInstanceCount > 0 && m_pxInstanceGroup != nullptr)
	{
		const std::vector<Zenith_Maths::Matrix4>& axTransforms = m_pxInstanceGroup->GetTransforms();
		// Write all transform matrices
		for (uint32_t i = 0; i < uInstanceCount; ++i)
		{
			const Zenith_Maths::Matrix4& xTransform = axTransforms[i];
			// Write matrix as 16 floats
			for (int col = 0; col < 4; ++col)
			{
				for (int row = 0; row < 4; ++row)
				{
					xStream << xTransform[col][row];
				}
			}
		}
	}
}

void Zenith_InstancedMeshComponent::ReadFromDataStream(Zenith_DataStream& xStream)
{
	// Version
	uint32_t uVersion;
	xStream >> uVersion;

	// Asset paths
	std::string strMeshPath;
	std::string strMaterialPath;
	xStream >> strMeshPath;
	xStream >> m_strAnimTexturePath;
	xStream >> strMaterialPath;

	// Load assets
	if (!strMeshPath.empty())
	{
		LoadMesh(strMeshPath);
	}
	if (!m_strAnimTexturePath.empty())
	{
		LoadAnimationTexture(m_strAnimTexturePath);
	}

	// Handle material - either from path or from serialized data
	if (!strMaterialPath.empty())
	{
		// File-based material - load from path
		LoadMaterial(strMaterialPath);
	}

	// Version 3+: read procedural material flag (always present)
	if (uVersion >= 3)
	{
		bool bHasProceduralMaterial;
		xStream >> bHasProceduralMaterial;
		if (bHasProceduralMaterial && strMaterialPath.empty())
		{
			// Create a new procedural material and deserialize its data
			Zenith_MaterialAsset* pxMaterial = Zenith_AssetRegistry::Get().Create<Zenith_MaterialAsset>();
			if (pxMaterial)
			{
				pxMaterial->ReadFromDataStream(xStream);
				SetMaterial(pxMaterial);
			}
		}
	}

	// Animation settings
	xStream >> m_fAnimationDuration;
	xStream >> m_fAnimationSpeed;
	xStream >> m_bAnimationsPaused;

	// Instance count
	uint32_t uInstanceCount;
	xStream >> uInstanceCount;

	// Version 4+: read and recreate instances from serialized transforms
	if (uVersion >= 4 && uInstanceCount > 0)
	{
		// Reserve capacity
		Reserve(uInstanceCount);

		// Read and spawn instances from serialized transforms
		for (uint32_t i = 0; i < uInstanceCount; ++i)
		{
			Zenith_Maths::Matrix4 xTransform;
			// Read matrix as 16 floats
			for (int col = 0; col < 4; ++col)
			{
				for (int row = 0; row < 4; ++row)
				{
					xStream >> xTransform[col][row];
				}
			}
			SpawnInstanceWithMatrix(xTransform);
		}
	}
}

//=============================================================================
// Helper Functions
//=============================================================================

Zenith_Maths::Matrix4 Zenith_InstancedMeshComponent::BuildMatrix(
	const Zenith_Maths::Vector3& xPosition,
	const Zenith_Maths::Quat& xRotation,
	const Zenith_Maths::Vector3& xScale) const
{
	Zenith_Maths::Matrix4 xMatrix = glm::identity<Zenith_Maths::Matrix4>();
	xMatrix = glm::translate(xMatrix, xPosition);
	xMatrix = xMatrix * glm::mat4_cast(xRotation);
	xMatrix = glm::scale(xMatrix, xScale);
	return xMatrix;
}

void Zenith_InstancedMeshComponent::EnsureInstanceGroupCreated()
{
	if (m_pxInstanceGroup == nullptr)
	{
		m_pxInstanceGroup = new Flux_InstanceGroup();
		Flux_InstancedMeshes::RegisterInstanceGroup(m_pxInstanceGroup);
	}
}

//=============================================================================
// Editor UI
//=============================================================================

#ifdef ZENITH_TOOLS
void Zenith_InstancedMeshComponent::RenderPropertiesPanel()
{
	ImGui::Text("Instanced Mesh Component");
	ImGui::Separator();

	// Mesh path (from handle)
	const std::string& strMeshPath = m_xMeshAsset.GetPath();
	ImGui::Text("Mesh: %s", strMeshPath.empty() ? "(none)" : strMeshPath.c_str());

	// Animation texture path
	ImGui::Text("Animation: %s", m_strAnimTexturePath.empty() ? "(none)" : m_strAnimTexturePath.c_str());

	// Stats
	ImGui::Separator();
	ImGui::Text("Instances: %u", GetInstanceCount());
	ImGui::Text("Visible: %u", GetVisibleCount());

	// Animation settings
	ImGui::Separator();
	ImGui::Text("Animation Settings");
	ImGui::DragFloat("Duration", &m_fAnimationDuration, 0.1f, 0.1f, 60.0f);
	ImGui::DragFloat("Speed", &m_fAnimationSpeed, 0.1f, 0.0f, 10.0f);
	ImGui::Checkbox("Paused", &m_bAnimationsPaused);
}
#endif
