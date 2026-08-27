#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#include "EntityComponent/Components/Zenith_InstancedMeshComponent.h"
#include "ZenithECS/Zenith_ComponentMeta.h"
#include "Flux/InstancedMeshes/Flux_InstancedMeshesImpl.h"
// The component header now forward-declares the Flux instance types (layering
// decoupling, wave13.A); pull in the concrete definitions here where the
// implementation dereferences them. Flux_InstanceGroup.h transitively provides
// Flux_MeshInstance, so the two includes below cover all three.
#include "Flux/InstancedMeshes/Flux_InstanceGroup.h"
#include "Flux/InstancedMeshes/Flux_AnimationTexture.h"
#include "AssetHandling/Zenith_MeshAsset.h"
// Per-instance colliders. Physics is reached through Zenith_Physics::Get() /
// TryGet() -- leaf statics, NOT g_xEngine: this file is capped at 3 g_xEngine
// reaches by Tools/engine_singleton_allowlist.txt and already has exactly 3.
#include "Physics/Zenith_Physics.h"
#include "Maths/Zenith_Maths.h"

//=============================================================================
// Constructor / Destructor
//=============================================================================

Zenith_InstancedMeshComponent::Zenith_InstancedMeshComponent(Zenith_Entity& xEntity)
	: m_xParentEntity(xEntity)
{
}

Zenith_InstancedMeshComponent::~Zenith_InstancedMeshComponent()
{
	// FIRST: the instance bodies, while the group (and the Jolt world, if it is
	// still up) are both still reachable. Goes through Zenith_Physics::TryGet(),
	// so a component pool destroyed after Physics::Shutdown is a quiet no-op
	// rather than the assert Get() would raise.
	DestroyAllInstanceBodies();

	// Unregister from renderer
	if (m_pxInstanceGroup != nullptr)
	{
		g_xEngine.InstancedMeshes().UnregisterInstanceGroup(m_pxInstanceGroup);
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
	, m_xInstanceColliderConfig(xOther.m_xInstanceColliderConfig)
	, m_axInstanceBodyIDs(std::move(xOther.m_axInstanceBodyIDs))
{
	xOther.m_pxInstanceGroup = nullptr;
	xOther.m_pxOwnedMeshInstance = nullptr;
	xOther.m_pxOwnedAnimTexture = nullptr;
	// The source MUST end with an EMPTY ledger: its destructor still runs, and a
	// ledger it shares with us would destroy bodies this component now owns
	// (the move-op regression class -- see commit 7fd3ccdc).
	xOther.m_axInstanceBodyIDs.Clear();
	xOther.m_xInstanceColliderConfig = Zenith_InstanceColliderConfig();
}

Zenith_InstancedMeshComponent& Zenith_InstancedMeshComponent::operator=(Zenith_InstancedMeshComponent&& xOther) noexcept
{
	if (this != &xOther)
	{
		// Clean up existing resources. The bodies go FIRST, for the same reason
		// the destructor destroys them first: they are keyed to OUR slots, and the
		// ledger is about to be overwritten with the source's.
		DestroyAllInstanceBodies();
		if (m_pxInstanceGroup != nullptr)
		{
			g_xEngine.InstancedMeshes().UnregisterInstanceGroup(m_pxInstanceGroup);
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
		m_xInstanceColliderConfig = xOther.m_xInstanceColliderConfig;
		m_axInstanceBodyIDs = std::move(xOther.m_axInstanceBodyIDs);

		xOther.m_pxInstanceGroup = nullptr;
		xOther.m_pxOwnedMeshInstance = nullptr;
		xOther.m_pxOwnedAnimTexture = nullptr;
		xOther.m_axInstanceBodyIDs.Clear();
		xOther.m_xInstanceColliderConfig = Zenith_InstanceColliderConfig();
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
	Zenith_MaterialAsset* pxMaterial = Zenith_AssetRegistry::GetView<Zenith_MaterialAsset>(strPath);
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
	Zenith_MeshAsset* pxMeshAsset = Zenith_AssetRegistry::GetView<Zenith_MeshAsset>(strPath);
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

	// Load animation texture (resolve prefixed path for direct file access)
	std::string strResolved = Zenith_AssetRegistry::ResolvePath(strPath);
	m_pxOwnedAnimTexture = Flux_AnimationTexture::LoadFromFile(strResolved);
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
	// After the transform, never before: the body pose is decomposed from it.
	CreateInstanceBody(uID);

	return uID;
}

uint32_t Zenith_InstancedMeshComponent::SpawnInstanceWithMatrix(const Zenith_Maths::Matrix4& xMatrix)
{
	EnsureInstanceGroupCreated();

	uint32_t uID = m_pxInstanceGroup->AddInstance();
	m_pxInstanceGroup->SetInstanceTransform(uID, xMatrix);
	CreateInstanceBody(uID);

	return uID;
}

void Zenith_InstancedMeshComponent::DespawnInstance(uint32_t uInstanceID)
{
	if (m_pxInstanceGroup != nullptr)
	{
		// BEFORE RemoveInstance: the slot is about to go on the free list, and a
		// recycled slot must not inherit the previous occupant's body.
		DestroyInstanceBody(uInstanceID);
		m_pxInstanceGroup->RemoveInstance(uInstanceID);
	}
}

void Zenith_InstancedMeshComponent::ClearInstances()
{
	if (m_pxInstanceGroup != nullptr)
	{
		DestroyAllInstanceBodies();
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
		RefreshInstanceBody(uInstanceID);
	}
}

void Zenith_InstancedMeshComponent::SetInstanceMatrix(uint32_t uInstanceID, const Zenith_Maths::Matrix4& xMatrix)
{
	if (m_pxInstanceGroup != nullptr)
	{
		m_pxInstanceGroup->SetInstanceTransform(uInstanceID, xMatrix);
		RefreshInstanceBody(uInstanceID);
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
		// A disabled instance is not rendered and must not collide either.
		// Re-enabling an already-enabled slot is legal here, which is why
		// CreateInstanceBody early-returns on an occupied ledger slot rather
		// than asserting.
		if (bEnabled)
		{
			CreateInstanceBody(uInstanceID);
		}
		else
		{
			DestroyInstanceBody(uInstanceID);
		}
	}
}

//=============================================================================
// Instance Colliders
//
// One static Jolt capsule per LIVE instance, owned by this component. The
// alternative -- one entity + ColliderComponent per instance -- would pay the
// per-frame Zenith_SyncPhysicsTransforms sweep (it has no static filter) and
// turn every tree into a navmesh obstruction box, for a group that is 2520
// instances in RenderTest alone.
//
// Deliberate v1 exclusions, all of them consequences of "no component per
// body": instance bodies are invisible to Zenith_SyncPhysicsTransforms,
// Zenith_AINavGeometry and Zenith_PhysicsDebugDraw, and a contact or raycast
// attributes to the GROUP entity rather than an individual instance. A group
// approaching Zenith_Physics::s_uMaxBodies (65536) must not enable a config.
//=============================================================================

void Zenith_InstancedMeshComponent::SetInstanceColliderCapsule(float fRadius,
	float fCylinderHalfHeight, float fLocalYOffset)
{
	// Idempotent for an identical config that already has bodies -- the terrain
	// editor's tree authoring calls this on a component it may have just
	// adopted, and a rebuild there would destroy and recreate thousands of Jolt
	// bodies for nothing.
	if (m_xInstanceColliderConfig.m_eType == INSTANCE_COLLIDER_TYPE_CAPSULE &&
		m_xInstanceColliderConfig.m_fRadius == fRadius &&
		m_xInstanceColliderConfig.m_fCylinderHalfHeight == fCylinderHalfHeight &&
		m_xInstanceColliderConfig.m_fLocalYOffset == fLocalYOffset &&
		HasInstanceColliders())
	{
		return;
	}

	m_xInstanceColliderConfig.m_eType = INSTANCE_COLLIDER_TYPE_CAPSULE;
	m_xInstanceColliderConfig.m_fRadius = fRadius;
	m_xInstanceColliderConfig.m_fCylinderHalfHeight = fCylinderHalfHeight;
	m_xInstanceColliderConfig.m_fLocalYOffset = fLocalYOffset;
	RebuildInstanceBodies();
}

void Zenith_InstancedMeshComponent::ClearInstanceColliderConfig()
{
	DestroyAllInstanceBodies();
	m_xInstanceColliderConfig = Zenith_InstanceColliderConfig();
}

bool Zenith_InstancedMeshComponent::HasInstanceColliders() const
{
	for (uint32_t u = 0; u < m_axInstanceBodyIDs.GetSize(); ++u)
	{
		if (m_axInstanceBodyIDs.Get(u).IsValid())
		{
			return true;
		}
	}
	return false;
}

uint32_t Zenith_InstancedMeshComponent::GetInstanceBodyCount() const
{
	uint32_t uCount = 0;
	for (uint32_t u = 0; u < m_axInstanceBodyIDs.GetSize(); ++u)
	{
		if (m_axInstanceBodyIDs.Get(u).IsValid())
		{
			++uCount;
		}
	}
	return uCount;
}

Zenith_PhysicsBodyID Zenith_InstancedMeshComponent::GetInstanceBodyID(uint32_t uSlot) const
{
	if (uSlot >= m_axInstanceBodyIDs.GetSize())
	{
		return Zenith_PhysicsBodyID();
	}
	return m_axInstanceBodyIDs.Get(uSlot);
}

void Zenith_InstancedMeshComponent::CreateInstanceBody(uint32_t uSlot)
{
	if (m_xInstanceColliderConfig.m_eType != INSTANCE_COLLIDER_TYPE_CAPSULE)
	{
		return;
	}
	if (m_pxInstanceGroup == nullptr)
	{
		return;
	}

	// A DEAD slot never gets a body, and this check is what makes the two creation
	// paths agree: CreateAllInstanceBodies filters through the enabled-slot list,
	// so the per-slot path has to apply the same rule or they disagree about what
	// "live instance" means. Without it, SetInstanceTransform on a slot that
	// SetInstanceEnabled(false) just disabled RESURRECTS its collider -- an
	// invisible instance that still blocks the player, and one WriteToDataStream
	// does not serialize (it writes visible slots only), so it would exist in the
	// authoring session and vanish on reload.
	const Zenith_Vector<Flux_InstanceAnimData>& axAnimData = m_pxInstanceGroup->GetAnimData();
	if (uSlot >= axAnimData.GetSize() || axAnimData.Get(uSlot).m_uFlags == 0)
	{
		return;
	}

	// The ledger is slot-indexed, so it grows to cover the highest slot ever
	// given a body. The group allocates from its free list before extending, so
	// this stays dense in practice.
	while (m_axInstanceBodyIDs.GetSize() <= uSlot)
	{
		m_axInstanceBodyIDs.PushBack(Zenith_PhysicsBodyID());
	}
	if (m_axInstanceBodyIDs.Get(uSlot).IsValid())
	{
		// Already has one: SetInstanceEnabled(true) on an enabled slot, or a
		// config sweep over a slot a spawn just gave a body. A second body here
		// would leak the first.
		return;
	}

	Zenith_Maths::Vector3 xPosition;
	Zenith_Maths::Quat xRotation;
	Zenith_Maths::Vector3 xScale;
	Zenith_Maths::DecomposeTRS(m_pxInstanceGroup->GetTransforms().Get(uSlot), xPosition, xRotation, xScale);

	const Zenith_InstanceColliderConfig& xConfig = m_xInstanceColliderConfig;
	// A capsule has ONE radius, so a non-uniform XZ scale has to collapse to a
	// scalar; the larger of the two keeps the collider from being narrower than
	// the mesh on its wider axis.
	const float fRadius = xConfig.m_fRadius * std::max(std::abs(xScale.x), std::abs(xScale.z));
	const float fCylinderHalfHeight = xConfig.m_fCylinderHalfHeight * std::abs(xScale.y);
	// The offset is a LOCAL displacement: scaled, then rotated into world space
	// by the instance's own rotation.
	const Zenith_Maths::Vector3 xBodyPosition = xPosition +
		xRotation * Zenith_Maths::Vector3(0.0f, xConfig.m_fLocalYOffset * xScale.y, 0.0f);

	m_axInstanceBodyIDs.Get(uSlot) = Zenith_Physics::Get().CreateStaticCapsuleBody(
		xBodyPosition, xRotation, fRadius, fCylinderHalfHeight, m_xParentEntity.GetEntityID());
}

void Zenith_InstancedMeshComponent::DestroyInstanceBody(uint32_t uSlot)
{
	if (uSlot >= m_axInstanceBodyIDs.GetSize())
	{
		return;
	}
	Zenith_PhysicsBodyID& xBodyID = m_axInstanceBodyIDs.Get(uSlot);
	if (xBodyID.IsInvalid())
	{
		return;
	}
	// TryGet, NOT Get: component-pool teardown can run after Physics::Shutdown,
	// and Get() asserts. Every IsAdded / null guard lives once, in DestroyBody.
	if (Zenith_Physics* pxPhysics = Zenith_Physics::TryGet())
	{
		pxPhysics->DestroyBody(xBodyID);
	}
	// Invalidated even when there was no live simulation to destroy it in -- the
	// id names nothing either way, and a stale ledger entry would block a later
	// CreateInstanceBody on this slot.
	xBodyID = Zenith_PhysicsBodyID();
}

void Zenith_InstancedMeshComponent::CreateAllInstanceBodies()
{
	if (m_xInstanceColliderConfig.m_eType != INSTANCE_COLLIDER_TYPE_CAPSULE)
	{
		return;
	}
	if (m_pxInstanceGroup == nullptr)
	{
		return;
	}
	Zenith_Vector<uint32_t> xEnabledSlots;
	m_pxInstanceGroup->ComputeVisibleIndices(xEnabledSlots);

	// The create sweep has no choice but to trust the GROUP's enabled-slot list --
	// after a ClearInstances the ledger is empty, so the group is the only source
	// of truth left. That trust is now warranted (Flux_InstanceGroup::Clear() zeroes
	// per-slot flags, so the list cannot outlive the instances) and this assert is
	// what keeps it warranted: an enabled list LONGER than the live count is the
	// stale-flag state and nothing else. It fired for real before that fix -- a
	// config set after a bare Clear() built one invisible wall per stale flag, and
	// the occupied-slot early-return in CreateInstanceBody then denied the real
	// respawned instance its collider.
	Zenith_Assert(xEnabledSlots.GetSize() <= m_pxInstanceGroup->GetInstanceCount(),
		"InstancedMesh collider sweep: %u enabled slots but only %u live instances -- "
		"Flux_InstanceGroup::Clear() left stale per-slot flags behind",
		xEnabledSlots.GetSize(), m_pxInstanceGroup->GetInstanceCount());

	for (uint32_t u = 0; u < xEnabledSlots.GetSize(); ++u)
	{
		CreateInstanceBody(xEnabledSlots.Get(u));
	}
}

void Zenith_InstancedMeshComponent::DestroyAllInstanceBodies()
{
	// Ledger-driven, NOT ComputeVisibleIndices-driven -- and still so now that
	// Flux_InstanceGroup::Clear() zeroes per-slot flags. The ledger is the record of
	// which slots WE gave a body to, which is the question this function is asking;
	// the group's enabled list answers a different one and would strand any body
	// whose slot the group has since forgotten.
	for (uint32_t u = 0; u < m_axInstanceBodyIDs.GetSize(); ++u)
	{
		DestroyInstanceBody(u);
	}
}

void Zenith_InstancedMeshComponent::RebuildInstanceBodies()
{
	DestroyAllInstanceBodies();
	CreateAllInstanceBodies();
	// Same reason ReadFromDataStream does this: a bulk one-at-a-time re-add leaves
	// Jolt's broadphase quadtree unoptimised, and the reconfigure path is the OTHER
	// bulk-add site. Skipping it here was an asymmetry, not a decision.
	if (GetInstanceBodyCount() > 64)
	{
		if (Zenith_Physics* pxPhysics = Zenith_Physics::TryGet())
		{
			pxPhysics->OptimizeBroadPhase();
		}
	}
}

void Zenith_InstancedMeshComponent::RefreshInstanceBody(uint32_t uSlot)
{
	// The capsule's dimensions are scale-derived, so a transform change can move
	// the shape as well as the pose: destroy + recreate rather than SetPosition.
	DestroyInstanceBody(uSlot);
	CreateInstanceBody(uSlot);
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
	uint32_t uVersion = 5;  // Version 5: per-instance collider config
	xStream << uVersion;

	// Asset paths (get from handles for registry assets)
	std::string strMeshPath = Zenith_AssetRegistry::NormalizeAssetPath(m_xMeshAsset.GetPath());
	std::string strMaterialPath = Zenith_AssetRegistry::NormalizeAssetPath(m_xMaterial.GetPath());
	xStream << strMeshPath;
	xStream << Zenith_AssetRegistry::NormalizeAssetPath(m_strAnimTexturePath);
	xStream << strMaterialPath;

	// For procedural materials (no path), serialize the material data directly
	bool bHasProceduralMaterial = strMaterialPath.empty() && m_xMaterial.IsResolved();
	xStream << bHasProceduralMaterial;
	if (bHasProceduralMaterial)
	{
		Zenith_MaterialAsset* pxMaterial = m_xMaterial.GetDirect();
		if (pxMaterial)
		{
			pxMaterial->WriteToDataStream(xStream);
		}
	}

	// Animation settings
	xStream << m_fAnimationDuration;
	xStream << m_fAnimationSpeed;
	xStream << m_bAnimationsPaused;

	// Version 5+: instance collider config. Written BEFORE the instance data so
	// the read path has it in hand by the time SpawnInstanceWithMatrix starts
	// creating bodies inline.
	xStream << static_cast<uint32_t>(m_xInstanceColliderConfig.m_eType);
	xStream << m_xInstanceColliderConfig.m_fRadius;
	xStream << m_xInstanceColliderConfig.m_fCylinderHalfHeight;
	xStream << m_xInstanceColliderConfig.m_fLocalYOffset;

	// Instance data (version 4+). Serialize the ENABLED slots, not the first
	// N slots: RemoveInstance disables a slot in place and free-lists its ID,
	// so after any removal the live instances are NOT contiguous — the old
	// first-N loop wrote removed slots' transforms and dropped live ones.
	Zenith_Vector<uint32_t> xEnabledSlots;
	if (m_pxInstanceGroup != nullptr)
	{
		m_pxInstanceGroup->ComputeVisibleIndices(xEnabledSlots);
	}
	uint32_t uInstanceCount = xEnabledSlots.GetSize();
	xStream << uInstanceCount;

	if (uInstanceCount > 0)
	{
		const Zenith_Vector<Zenith_Maths::Matrix4>& axTransforms = m_pxInstanceGroup->GetTransforms();
		for (uint32_t i = 0; i < uInstanceCount; ++i)
		{
			const Zenith_Maths::Matrix4& xTransform = axTransforms.Get(xEnabledSlots.Get(i));
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
	strMeshPath = Zenith_AssetRegistry::NormalizeAssetPath(strMeshPath);
	xStream >> m_strAnimTexturePath;
	m_strAnimTexturePath = Zenith_AssetRegistry::NormalizeAssetPath(m_strAnimTexturePath);
	xStream >> strMaterialPath;
	strMaterialPath = Zenith_AssetRegistry::NormalizeAssetPath(strMaterialPath);

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
			auto xhMaterial = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
			Zenith_MaterialAsset* pxMaterial = xhMaterial.GetDirect();
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

	// Version 5+: instance collider config. Written directly into the members
	// rather than through SetInstanceColliderCapsule -- the ledger is empty at
	// this point so there is nothing to sweep, and every instance the v4+ loop
	// below spawns creates its body inline. A v4 stream skips the block entirely,
	// leaving the config NONE: byte-for-byte today's behaviour.
	if (uVersion >= 5)
	{
		uint32_t uColliderType = 0;
		xStream >> uColliderType;
		m_xInstanceColliderConfig.m_eType = static_cast<InstanceColliderType>(uColliderType);
		xStream >> m_xInstanceColliderConfig.m_fRadius;
		xStream >> m_xInstanceColliderConfig.m_fCylinderHalfHeight;
		xStream >> m_xInstanceColliderConfig.m_fLocalYOffset;
	}

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
			uint32_t uInstanceID = SpawnInstanceWithMatrix(xTransform);

			// Per-instance animation state is not serialized (the VAT-enable
			// flag, index and phase live in transient anim data) — re-derive
			// it: when an animation texture is loaded, restart every instance
			// on clip 0 with a deterministic per-instance phase so wind-swayed
			// foliage doesn't reload as a frozen, synchronised block.
			if (GetAnimationTexture() != nullptr)
			{
				const float fPhase = fmodf(static_cast<float>(uInstanceID) * 0.618034f, 1.0f);
				SetInstanceAnimationByIndex(uInstanceID, 0, fPhase);
			}
		}
	}

	// Bulk one-at-a-time static adds leave Jolt's broadphase quadtree
	// unoptimised until simulation steps have run. RenderTest's trunk group is
	// 2520 of them in one deserialize; the threshold keeps a handful of
	// instances from paying for a rebuild.
	if (GetInstanceBodyCount() > 64)
	{
		if (Zenith_Physics* pxPhysics = Zenith_Physics::TryGet())
		{
			pxPhysics->OptimizeBroadPhase();
		}
	}
}

//=============================================================================
// Helper Functions
//=============================================================================

// Deterministic-FP: instance transforms are SERIALIZED verbatim (16 floats each) by
// WriteToDataStream, so a tools boot that authors instances into a tracked scene must
// build the same matrix at every optimization level.
ZENITH_AUTHORING_DETERMINISM_BEGIN

Zenith_Maths::Matrix4 Zenith_InstancedMeshComponent::BuildMatrix(
	const Zenith_Maths::Vector3& xPosition,
	const Zenith_Maths::Quat& xRotation,
	const Zenith_Maths::Vector3& xScale) const
{
	return Zenith_Maths::AuthoringTRS(xPosition, xRotation, xScale);
}

ZENITH_AUTHORING_DETERMINISM_END

void Zenith_InstancedMeshComponent::EnsureInstanceGroupCreated()
{
	if (m_pxInstanceGroup == nullptr)
	{
		m_pxInstanceGroup = new Flux_InstanceGroup();
		g_xEngine.InstancedMeshes().RegisterInstanceGroup(m_pxInstanceGroup);
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

	// Instance collider. Edits go through the public setters (never straight
	// into m_xInstanceColliderConfig) so the body sweep runs -- a direct write
	// would leave the authored numbers and the live bodies disagreeing.
	ImGui::Separator();
	ImGui::Text("Instance Collider");
	const char* aszTypes[] = { "None", "Capsule" };
	int iType = static_cast<int>(m_xInstanceColliderConfig.m_eType);
	if (ImGui::Combo("Type", &iType, aszTypes, IM_ARRAYSIZE(aszTypes)))
	{
		if (iType == static_cast<int>(INSTANCE_COLLIDER_TYPE_CAPSULE))
		{
			SetInstanceColliderCapsule(m_xInstanceColliderConfig.m_fRadius,
				m_xInstanceColliderConfig.m_fCylinderHalfHeight,
				m_xInstanceColliderConfig.m_fLocalYOffset);
		}
		else
		{
			ClearInstanceColliderConfig();
		}
	}
	if (m_xInstanceColliderConfig.m_eType == INSTANCE_COLLIDER_TYPE_CAPSULE)
	{
		// ★ APPLY ON RELEASE, NOT PER DRAG FRAME. The dimensions are edited in
		// place (so the widget tracks the drag) and the BODIES are rebuilt only
		// when the drag ends. Rebuilding every frame would destroy and recreate
		// one Jolt body per instance per frame -- 2520 of them for RenderTest's
		// trunk group -- and the editor's Stopped mode never calls
		// PhysicsSystem::Update (Zenith_Core.cpp gates it on Playing), so nothing
		// reclaims the broadphase nodes that churn allocates. The comparable
		// destructive rebuild on Zenith_ColliderComponent's panel is behind a
		// BUTTON for the same reason.
		bool bCommitted = false;
		ImGui::DragFloat("Radius", &m_xInstanceColliderConfig.m_fRadius, 0.01f, 0.01f, 20.0f);
		bCommitted |= ImGui::IsItemDeactivatedAfterEdit();
		ImGui::DragFloat("Cyl Half-Height", &m_xInstanceColliderConfig.m_fCylinderHalfHeight, 0.05f, 0.01f, 100.0f);
		bCommitted |= ImGui::IsItemDeactivatedAfterEdit();
		ImGui::DragFloat("Local Y Offset", &m_xInstanceColliderConfig.m_fLocalYOffset, 0.05f, -100.0f, 100.0f);
		bCommitted |= ImGui::IsItemDeactivatedAfterEdit();
		if (bCommitted)
		{
			// The members already carry the edited values, so routing through
			// SetInstanceColliderCapsule would hit its idempotence guard and skip
			// the rebuild entirely. Rebuild explicitly.
			RebuildInstanceBodies();
		}
	}
	ImGui::Text("Instance bodies: %u", GetInstanceBodyCount());
}
#endif

// Instance-collider unit tests live aggregate-side (they exercise the concrete
// component + Zenith_Physics, neither of which the Physics leaf may name).
// Hosted in THIS TU because it is ALWAYS linked -- the component registrar
// references this component -- so the ZENITH_TEST registrars survive /OPT:REF in
// every config. Same reasoning as Zenith_ColliderComponent.cpp's host block.
#ifdef ZENITH_TESTING
#include "EntityComponent/Zenith_InstancedMeshComponent.Tests.inl"
#endif
