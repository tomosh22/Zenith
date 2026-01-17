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

	// Clean up owned resources
	delete m_pxOwnedAnimTexture;
	m_pxOwnedAnimTexture = nullptr;

	// Note: m_pxOwnedMeshInstance is deleted by m_pxOwnedMeshAsset's cleanup
	// or we own it directly - either way it needs deletion
	if (m_pxOwnedMeshInstance != nullptr)
	{
		m_pxOwnedMeshInstance->Destroy();
		delete m_pxOwnedMeshInstance;
		m_pxOwnedMeshInstance = nullptr;
	}

	delete m_pxOwnedMeshAsset;
	m_pxOwnedMeshAsset = nullptr;

	delete m_pxOwnedMaterial;
	m_pxOwnedMaterial = nullptr;
}

//=============================================================================
// Move Semantics
//=============================================================================

Zenith_InstancedMeshComponent::Zenith_InstancedMeshComponent(Zenith_InstancedMeshComponent&& xOther) noexcept
	: m_xParentEntity(xOther.m_xParentEntity)
	, m_pxInstanceGroup(xOther.m_pxInstanceGroup)
	, m_pxOwnedMeshAsset(xOther.m_pxOwnedMeshAsset)
	, m_pxOwnedMeshInstance(xOther.m_pxOwnedMeshInstance)
	, m_pxOwnedAnimTexture(xOther.m_pxOwnedAnimTexture)
	, m_pxOwnedMaterial(xOther.m_pxOwnedMaterial)
	, m_fAnimationDuration(xOther.m_fAnimationDuration)
	, m_fAnimationSpeed(xOther.m_fAnimationSpeed)
	, m_bAnimationsPaused(xOther.m_bAnimationsPaused)
	, m_strMeshPath(std::move(xOther.m_strMeshPath))
	, m_strAnimTexturePath(std::move(xOther.m_strAnimTexturePath))
	, m_strMaterialPath(std::move(xOther.m_strMaterialPath))
{
	xOther.m_pxInstanceGroup = nullptr;
	xOther.m_pxOwnedMeshAsset = nullptr;
	xOther.m_pxOwnedMeshInstance = nullptr;
	xOther.m_pxOwnedAnimTexture = nullptr;
	xOther.m_pxOwnedMaterial = nullptr;
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
		delete m_pxOwnedMeshAsset;
		delete m_pxOwnedMaterial;

		// Move from other
		m_xParentEntity = xOther.m_xParentEntity;
		m_pxInstanceGroup = xOther.m_pxInstanceGroup;
		m_pxOwnedMeshAsset = xOther.m_pxOwnedMeshAsset;
		m_pxOwnedMeshInstance = xOther.m_pxOwnedMeshInstance;
		m_pxOwnedAnimTexture = xOther.m_pxOwnedAnimTexture;
		m_pxOwnedMaterial = xOther.m_pxOwnedMaterial;
		m_fAnimationDuration = xOther.m_fAnimationDuration;
		m_fAnimationSpeed = xOther.m_fAnimationSpeed;
		m_bAnimationsPaused = xOther.m_bAnimationsPaused;
		m_strMeshPath = std::move(xOther.m_strMeshPath);
		m_strAnimTexturePath = std::move(xOther.m_strAnimTexturePath);
		m_strMaterialPath = std::move(xOther.m_strMaterialPath);

		xOther.m_pxInstanceGroup = nullptr;
		xOther.m_pxOwnedMeshAsset = nullptr;
		xOther.m_pxOwnedMeshInstance = nullptr;
		xOther.m_pxOwnedAnimTexture = nullptr;
		xOther.m_pxOwnedMaterial = nullptr;
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
	m_pxInstanceGroup->SetMaterial(pxMaterial);
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
	m_strMeshPath = strPath;

	// Clean up existing
	if (m_pxOwnedMeshInstance != nullptr)
	{
		m_pxOwnedMeshInstance->Destroy();
		delete m_pxOwnedMeshInstance;
		m_pxOwnedMeshInstance = nullptr;
	}
	delete m_pxOwnedMeshAsset;
	m_pxOwnedMeshAsset = nullptr;

	// Load mesh asset (.zasset format) via registry
	m_pxOwnedMeshAsset = Zenith_AssetRegistry::Get().Get<Zenith_MeshAsset>(strPath);
	if (m_pxOwnedMeshAsset == nullptr)
	{
		Zenith_Error(LOG_CATEGORY_MESH, "[InstancedMeshComponent] Failed to load mesh asset: %s", strPath.c_str());
		return;
	}

	// Create mesh instance for GPU rendering
	m_pxOwnedMeshInstance = Flux_MeshInstance::CreateFromAsset(m_pxOwnedMeshAsset);
	if (m_pxOwnedMeshInstance == nullptr)
	{
		Zenith_Error(LOG_CATEGORY_MESH, "[InstancedMeshComponent] Failed to create mesh instance from asset: %s", strPath.c_str());
		delete m_pxOwnedMeshAsset;
		m_pxOwnedMeshAsset = nullptr;
		return;
	}

	// Set on instance group
	SetMesh(m_pxOwnedMeshInstance);

	// Set bounds from mesh asset
	Zenith_Maths::Vector3 xMin = m_pxOwnedMeshAsset->GetBoundsMin();
	Zenith_Maths::Vector3 xMax = m_pxOwnedMeshAsset->GetBoundsMax();
	Zenith_Maths::Vector3 xCenter = (xMin + xMax) * 0.5f;
	float fRadius = glm::length(xMax - xCenter);
	SetBounds(xCenter, fRadius);

	Zenith_Log(LOG_CATEGORY_MESH, "[InstancedMeshComponent] Loaded mesh: %s (%u verts, %u indices)",
		strPath.c_str(), m_pxOwnedMeshAsset->GetNumVerts(), m_pxOwnedMeshAsset->GetNumIndices());
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
	uint32_t uVersion = 1;
	xStream << uVersion;

	// Asset paths
	xStream << m_strMeshPath;
	xStream << m_strAnimTexturePath;
	xStream << m_strMaterialPath;

	// Animation settings
	xStream << m_fAnimationDuration;
	xStream << m_fAnimationSpeed;
	xStream << m_bAnimationsPaused;

	// Instance count (we don't serialize individual instances - they're runtime only)
	uint32_t uInstanceCount = GetInstanceCount();
	xStream << uInstanceCount;
}

void Zenith_InstancedMeshComponent::ReadFromDataStream(Zenith_DataStream& xStream)
{
	// Version
	uint32_t uVersion;
	xStream >> uVersion;

	// Asset paths
	xStream >> m_strMeshPath;
	xStream >> m_strAnimTexturePath;
	xStream >> m_strMaterialPath;

	// Load assets
	if (!m_strMeshPath.empty())
	{
		LoadMesh(m_strMeshPath);
	}
	if (!m_strAnimTexturePath.empty())
	{
		LoadAnimationTexture(m_strAnimTexturePath);
	}

	// Animation settings
	xStream >> m_fAnimationDuration;
	xStream >> m_fAnimationSpeed;
	xStream >> m_bAnimationsPaused;

	// Instance count (read but not used - instances are runtime only)
	uint32_t uInstanceCount;
	xStream >> uInstanceCount;
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

	// Mesh path
	ImGui::Text("Mesh: %s", m_strMeshPath.empty() ? "(none)" : m_strMeshPath.c_str());

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
