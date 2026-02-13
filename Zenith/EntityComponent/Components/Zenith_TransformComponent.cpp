// This file uses Jolt Physics - disable memory tracking macro to avoid conflicts
#include "Zenith.h"
#define ZENITH_PLACEMENT_NEW_ZONE
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Zenith_ComponentMeta.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "Physics/Zenith_Physics.h"
#include "DataStream/Zenith_DataStream.h"
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/Body.h>

ZENITH_REGISTER_COMPONENT(Zenith_TransformComponent, "Transform")

Zenith_TransformComponent::Zenith_TransformComponent(Zenith_Entity& xEntity)
	: m_xOwningEntity(xEntity)
{
}

Zenith_TransformComponent::~Zenith_TransformComponent()
{
	// Skip hierarchy cleanup if entity's scene is not the current scene
	// This happens when:
	// 1. A local test scene is being destroyed (not the active scene)
	// 2. The scene is null (shouldn't happen but defensive check)
	//
	// During normal entity removal via SceneData::RemoveEntity or ProcessPendingDestructions,
	// hierarchy cleanup is handled explicitly before component destruction.
	// The destructor cleanup is only needed for edge cases where a TransformComponent
	// is removed individually without going through the scene's removal path.

	Zenith_SceneData* pxOwningSceneData = m_xOwningEntity.GetSceneData();
	if (pxOwningSceneData == nullptr)
	{
		// No scene - can't do hierarchy operations, just let member destructors run
		return;
	}

	// Check if the scene is being destroyed/reset - skip all cleanup to avoid
	// acquiring mutexes and accessing scene data during destruction.
	// This prevents crashes during static destruction when profiling data may be gone.
	if (pxOwningSceneData->IsBeingDestroyed())
	{
		return;
	}

	// Check if this entity's scene is the current active scene
	// If not, we're likely in a test scenario with a local scene being destroyed
	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxActiveSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
	if (pxOwningSceneData != pxActiveSceneData)
	{
		// Different scene - skip hierarchy cleanup to avoid accessing wrong scene data
		return;
	}

	// Check if the entity still exists in its scene
	// During scene destruction, entity slots may be cleared before component pools
	Zenith_EntityID xMyID = m_xOwningEntity.GetEntityID();
	if (!pxOwningSceneData->EntityExists(xMyID))
	{
		// Entity no longer valid - skip hierarchy cleanup
		return;
	}

	// Safe to perform hierarchy cleanup
	DetachFromParent();
	DetachAllChildren();
}

Zenith_TransformComponent* Zenith_TransformComponent::TryGetParent() const
{
	if (m_xParentEntityID == INVALID_ENTITY_ID)
	{
		return nullptr;
	}

	Zenith_SceneData* pxSceneData = m_xOwningEntity.GetSceneData();
	if (!pxSceneData)
	{
		return nullptr;
	}

	if (!pxSceneData->EntityExists(m_xParentEntityID))
	{
		return nullptr;
	}

	// Access component pool directly - safer than via temporary entity
	return &pxSceneData->GetComponentFromEntity<Zenith_TransformComponent>(m_xParentEntityID);
}

Zenith_Entity Zenith_TransformComponent::GetParentEntity() const
{
	if (m_xParentEntityID == INVALID_ENTITY_ID)
	{
		return Zenith_Entity();
	}

	Zenith_SceneData* pxSceneData = m_xOwningEntity.GetSceneData();
	if (!pxSceneData)
	{
		return Zenith_Entity();
	}

	if (!pxSceneData->EntityExists(m_xParentEntityID))
	{
		return Zenith_Entity();
	}

	return pxSceneData->GetEntity(m_xParentEntityID);
}

Zenith_TransformComponent* Zenith_TransformComponent::TryGetChildAt(uint32_t uIndex) const
{
	if (uIndex >= m_xChildEntityIDs.GetSize())
	{
		return nullptr;
	}

	Zenith_SceneData* pxSceneData = m_xOwningEntity.GetSceneData();
	if (!pxSceneData)
	{
		return nullptr;
	}

	Zenith_EntityID uChildID = m_xChildEntityIDs.Get(uIndex);
	if (!pxSceneData->EntityExists(uChildID))
	{
		return nullptr;
	}

	// Access component pool directly - safer than via temporary entity
	return &pxSceneData->GetComponentFromEntity<Zenith_TransformComponent>(uChildID);
}

Zenith_Entity Zenith_TransformComponent::GetChildEntityAt(uint32_t uIndex) const
{
	if (uIndex >= m_xChildEntityIDs.GetSize())
	{
		return Zenith_Entity();
	}

	Zenith_SceneData* pxSceneData = m_xOwningEntity.GetSceneData();
	if (!pxSceneData)
	{
		return Zenith_Entity();
	}

	Zenith_EntityID uChildID = m_xChildEntityIDs.Get(uIndex);
	if (!pxSceneData->EntityExists(uChildID))
	{
		return Zenith_Entity();
	}

	return pxSceneData->GetEntity(uChildID);
}

void Zenith_TransformComponent::SetParent(Zenith_TransformComponent* pxParent)
{
	Zenith_EntityID uNewParentID = (pxParent != nullptr) ?
		pxParent->GetEntity().GetEntityID() : INVALID_ENTITY_ID;
	SetParentByID(uNewParentID);
}

bool Zenith_TransformComponent::IsDescendantOf(Zenith_EntityID uAncestorID) const
{
	if (uAncestorID == INVALID_ENTITY_ID)
	{
		return false;
	}

	// Use the owning entity's scene, not GetActiveScene()
	// This allows hierarchy operations to work correctly on local/test scenes
	Zenith_SceneData* pxSceneData = m_xOwningEntity.GetSceneData();
	if (pxSceneData == nullptr)
	{
		return false;
	}
	Zenith_EntityID uCurrentID = m_xParentEntityID;

	// Walk up the parent chain looking for the ancestor
	// Also includes depth limit as safety against corrupted data
	constexpr u_int MAX_HIERARCHY_DEPTH = 1000;
	u_int uDepth = 0;

	while (uCurrentID != INVALID_ENTITY_ID && uDepth < MAX_HIERARCHY_DEPTH)
	{
		if (uCurrentID == uAncestorID)
		{
			return true;
		}

		if (!pxSceneData->EntityExists(uCurrentID))
		{
			return false;
		}

		uCurrentID = pxSceneData->GetEntity(uCurrentID).GetComponent<Zenith_TransformComponent>().m_xParentEntityID;
		++uDepth;
	}

	// If we hit MAX_DEPTH, likely circular reference or corrupted hierarchy
	if (uDepth >= MAX_HIERARCHY_DEPTH)
	{
		Zenith_Error(LOG_CATEGORY_ECS, "IsDescendantOf: Max hierarchy depth %u exceeded for entity %u - possible circular reference",
			MAX_HIERARCHY_DEPTH, m_xOwningEntity.GetEntityID().m_uIndex);
	}

	return false;
}

bool Zenith_TransformComponent::IsDescendantOfUnsafe(Zenith_EntityID uAncestorID, Zenith_SceneData& xSceneData) const
{
	// Unsafe version - assumes caller holds xSceneData.m_xMutex
	// Used by SetParentByID to avoid recursive locking

	if (uAncestorID == INVALID_ENTITY_ID)
	{
		return false;
	}

	Zenith_EntityID uCurrentID = m_xParentEntityID;

	// Walk up the parent chain looking for the ancestor
	constexpr u_int MAX_HIERARCHY_DEPTH = 1000;
	u_int uDepth = 0;

	while (uCurrentID != INVALID_ENTITY_ID && uDepth < MAX_HIERARCHY_DEPTH)
	{
		if (uCurrentID == uAncestorID)
		{
			return true;
		}

		if (!xSceneData.EntityExists(uCurrentID))
		{
			return false;
		}

		uCurrentID = xSceneData.GetComponentFromEntity<Zenith_TransformComponent>(uCurrentID).m_xParentEntityID;
		++uDepth;
	}

	Zenith_Assert(uDepth < MAX_HIERARCHY_DEPTH,
		"IsDescendantOfUnsafe: Max depth exceeded - possible circular reference");

	return false;
}

void Zenith_TransformComponent::SetParentByID(Zenith_EntityID uNewParentID)
{
	if (m_xParentEntityID == uNewParentID) return;

	// Use the owning entity's scene, not GetActiveScene()
	// This allows hierarchy operations to work correctly on local/test scenes
	Zenith_SceneData* pxSceneData = m_xOwningEntity.GetSceneData();
	if (pxSceneData == nullptr)
	{
		Zenith_Warning(LOG_CATEGORY_ECS, "SetParentByID: Entity has no scene");
		return;
	}

	Zenith_EntityID uMyEntityID = m_xOwningEntity.GetEntityID();

	// CIRCULAR HIERARCHY CHECKS (Unity-style safety)
	if (uNewParentID != INVALID_ENTITY_ID)
	{
		// Unity parity: Cannot parent to an entity in a different scene
		// EntityExists checks global slots and would succeed for cross-scene entities,
		// but GetComponentFromEntity uses per-scene pools and would access the wrong pool
		int iParentScene = Zenith_SceneData::GetEntitySceneHandle(uNewParentID);
		int iMyScene = Zenith_SceneData::GetEntitySceneHandle(uMyEntityID);
		Zenith_Assert(iParentScene != -1, "SetParentByID: Parent entity index out of range");
		Zenith_Assert(iParentScene == iMyScene,
			"SetParentByID: Cannot parent entity to an entity in a different scene (child scene=%d, parent scene=%d)",
			iMyScene, iParentScene);

		// Cannot parent to self
		if (uNewParentID == uMyEntityID)
		{
			Zenith_Warning(LOG_CATEGORY_ECS, "Cannot parent entity %u to itself", uMyEntityID.m_uIndex);
			return;
		}

		// Cannot parent to a descendant (would create cycle)
		if (pxSceneData->EntityExists(uNewParentID))
		{
			Zenith_TransformComponent& xProposedParent = pxSceneData->GetComponentFromEntity<Zenith_TransformComponent>(uNewParentID);
			if (xProposedParent.IsDescendantOfUnsafe(uMyEntityID, *pxSceneData))
			{
				Zenith_Warning(LOG_CATEGORY_ECS, "Cannot parent entity %u to %u - would create circular hierarchy",
					uMyEntityID.m_uIndex, uNewParentID.m_uIndex);
				return;
			}
		}
		else
		{
			// Parent entity doesn't exist
			Zenith_Warning(LOG_CATEGORY_ECS, "Cannot parent entity %u to non-existent entity %u",
				uMyEntityID.m_uIndex, uNewParentID.m_uIndex);
			return;
		}
	}

	// Remove from old parent's children (use unsafe methods since we hold lock)
	if (m_xParentEntityID != INVALID_ENTITY_ID && pxSceneData->EntityExists(m_xParentEntityID))
	{
		Zenith_TransformComponent& xOldParent = pxSceneData->GetComponentFromEntity<Zenith_TransformComponent>(m_xParentEntityID);
		xOldParent.m_xChildEntityIDs.EraseValue(uMyEntityID);
	}

	m_xParentEntityID = uNewParentID;

	// Add to new parent's children
	if (m_xParentEntityID != INVALID_ENTITY_ID && pxSceneData->EntityExists(m_xParentEntityID))
	{
		Zenith_TransformComponent& xNewParent = pxSceneData->GetComponentFromEntity<Zenith_TransformComponent>(m_xParentEntityID);
		xNewParent.m_xChildEntityIDs.PushBack(uMyEntityID);
	}

	// Invalidate root entity cache since parent changed (entity may have become/stopped being a root)
	pxSceneData->InvalidateRootEntityCache();
}

void Zenith_TransformComponent::DetachFromParent()
{
	SetParentByID(INVALID_ENTITY_ID);
}

void Zenith_TransformComponent::DetachAllChildren()
{
	// Use the owning entity's scene, not GetActiveScene()
	// This allows hierarchy operations to work correctly on local/test scenes
	Zenith_SceneData* pxSceneData = m_xOwningEntity.GetSceneData();
	if (pxSceneData == nullptr)
	{
		// No scene - just clear our list directly
		m_xChildEntityIDs.Clear();
		return;
	}

	// Clear each child's parent reference directly, then bulk-clear our list
	// This avoids O(n^2) from per-child EraseValue on our child list
	for (u_int i = 0; i < m_xChildEntityIDs.GetSize(); ++i)
	{
		Zenith_EntityID uChildID = m_xChildEntityIDs.Get(i);
		if (pxSceneData->EntityExists(uChildID))
		{
			Zenith_TransformComponent& xChild = pxSceneData->GetComponentFromEntity<Zenith_TransformComponent>(uChildID);
			xChild.m_xParentEntityID = INVALID_ENTITY_ID;
		}
	}
	m_xChildEntityIDs.Clear();
	pxSceneData->InvalidateRootEntityCache();
}

void Zenith_TransformComponent::SetPosition(const Zenith_Maths::Vector3& xPos)
{
	// Check if entity has a physics body via ColliderComponent
	// Use BodyInterface with BodyID for thread-safe access
	if (m_xOwningEntity.HasComponent<Zenith_ColliderComponent>() && Zenith_Physics::s_pxPhysicsSystem != nullptr)
	{
		Zenith_ColliderComponent& xCollider = m_xOwningEntity.GetComponent<Zenith_ColliderComponent>();
		if (xCollider.HasValidBody())
		{
			JPH::BodyInterface& xBodyInterface = Zenith_Physics::s_pxPhysicsSystem->GetBodyInterface();
			JPH::Vec3 xJoltPos(xPos.x, xPos.y, xPos.z);
			xBodyInterface.SetPosition(xCollider.GetBodyID(), xJoltPos, JPH::EActivation::Activate);
			return;
		}
	}
	m_xPosition = xPos;
}

void Zenith_TransformComponent::SetRotation(const Zenith_Maths::Quat& xRot)
{
	// Check if entity has a physics body via ColliderComponent
	// Use BodyInterface with BodyID for thread-safe access
	if (m_xOwningEntity.HasComponent<Zenith_ColliderComponent>() && Zenith_Physics::s_pxPhysicsSystem != nullptr)
	{
		Zenith_ColliderComponent& xCollider = m_xOwningEntity.GetComponent<Zenith_ColliderComponent>();
		if (xCollider.HasValidBody())
		{
			JPH::BodyInterface& xBodyInterface = Zenith_Physics::s_pxPhysicsSystem->GetBodyInterface();
			JPH::Quat xJoltRot(xRot.x, xRot.y, xRot.z, xRot.w);
			xBodyInterface.SetRotation(xCollider.GetBodyID(), xJoltRot, JPH::EActivation::Activate);
			return;
		}
	}
	m_xRotation = xRot;
}

void Zenith_TransformComponent::SetScale(const Zenith_Maths::Vector3& xScale)
{
	// Check if scale actually changed
	if (m_xScale.x == xScale.x && m_xScale.y == xScale.y && m_xScale.z == xScale.z)
	{
		return;
	}

	m_xScale = xScale;

	// If entity has a model component, regenerate physics mesh with new baked scale
	if (m_xOwningEntity.HasComponent<Zenith_ModelComponent>())
	{
		Zenith_ModelComponent& xModel = m_xOwningEntity.GetComponent<Zenith_ModelComponent>();
		if (xModel.HasPhysicsMesh())
		{
			xModel.GeneratePhysicsMesh();
		}
	}

	// If entity has a collider component, rebuild it to reflect new scale
	if (m_xOwningEntity.HasComponent<Zenith_ColliderComponent>())
	{
		Zenith_ColliderComponent& xCollider = m_xOwningEntity.GetComponent<Zenith_ColliderComponent>();
		xCollider.RebuildCollider();
	}
}

void Zenith_TransformComponent::GetPosition(Zenith_Maths::Vector3& xPos)
{
	// Check if entity has a physics body via ColliderComponent
	// Use BodyInterface with BodyID for thread-safe access
	if (m_xOwningEntity.HasComponent<Zenith_ColliderComponent>() && Zenith_Physics::s_pxPhysicsSystem != nullptr)
	{
		Zenith_ColliderComponent& xCollider = m_xOwningEntity.GetComponent<Zenith_ColliderComponent>();
		if (xCollider.HasValidBody())
		{
			// Use BodyInterface for safe access - never access Body pointer directly
			JPH::BodyInterface& xBodyInterface = Zenith_Physics::s_pxPhysicsSystem->GetBodyInterfaceNoLock();
			JPH::Vec3 xJoltPos = xBodyInterface.GetPosition(xCollider.GetBodyID());
			xPos.x = xJoltPos.GetX();
			xPos.y = xJoltPos.GetY();
			xPos.z = xJoltPos.GetZ();
			return;
		}
		else
		{
			// Collider exists but body is invalid - fall through to m_xPosition
		}
	}
	xPos = m_xPosition;
}

void Zenith_TransformComponent::GetRotation(Zenith_Maths::Quat& xRot)
{
	// Check if entity has a physics body via ColliderComponent
	// Use BodyInterface with BodyID for thread-safe access
	if (m_xOwningEntity.HasComponent<Zenith_ColliderComponent>() && Zenith_Physics::s_pxPhysicsSystem != nullptr)
	{
		Zenith_ColliderComponent& xCollider = m_xOwningEntity.GetComponent<Zenith_ColliderComponent>();
		if (xCollider.HasValidBody())
		{
			// Use BodyInterface for safe access - never access Body pointer directly
			JPH::BodyInterface& xBodyInterface = Zenith_Physics::s_pxPhysicsSystem->GetBodyInterfaceNoLock();
			JPH::Quat xJoltRot = xBodyInterface.GetRotation(xCollider.GetBodyID());
			xRot.x = xJoltRot.GetX();
			xRot.y = xJoltRot.GetY();
			xRot.z = xJoltRot.GetZ();
			xRot.w = xJoltRot.GetW();
			return;
		}
	}
	xRot = m_xRotation;
}

void Zenith_TransformComponent::GetScale(Zenith_Maths::Vector3& xScale) const
{
	xScale = m_xScale;
}

void Zenith_TransformComponent::BuildModelMatrix(Zenith_Maths::Matrix4& xMatOut)
{
	Zenith_Maths::Vector3 xPos;
	Zenith_Maths::Quat xRot;
	GetPosition(xPos);
	GetRotation(xRot);

	glm::mat4 xTranslation = glm::translate(glm::identity<glm::mat4>(), xPos);
	glm::mat4 xRotation = glm::mat4_cast(xRot);
	glm::mat4 xScaleMat = glm::scale(glm::identity<glm::mat4>(), m_xScale);

	xMatOut = xTranslation * xRotation * xScaleMat;

	// Walk parent chain via EntityIDs (safe against pool relocations)
	Zenith_EntityID uParentID = m_xParentEntityID;
	Zenith_SceneData* pxSceneData = m_xOwningEntity.GetSceneData();
	if (!pxSceneData)
	{
		return;
	}

	// Depth limit to catch any circular references that slip through
	// (should never happen with SetParentByID checks, but safety first)
	constexpr u_int SOFT_HIERARCHY_DEPTH = 100;   // Warning threshold
	constexpr u_int MAX_HIERARCHY_DEPTH = 1000;   // Hard limit
	u_int uDepth = 0;

	while (uParentID != INVALID_ENTITY_ID && pxSceneData->EntityExists(uParentID))
	{
		// Soft warning at 100 levels - unusual but not necessarily broken
		if (uDepth == SOFT_HIERARCHY_DEPTH)
		{
			Zenith_Warning(LOG_CATEGORY_ECS, "BuildModelMatrix: Entity %u has deep hierarchy (%u levels) - consider flattening",
				m_xOwningEntity.GetEntityID().m_uIndex, uDepth);
		}

		Zenith_Assert(uDepth < MAX_HIERARCHY_DEPTH, "BuildModelMatrix: Exceeded max hierarchy depth %u - possible circular reference for entity %u", MAX_HIERARCHY_DEPTH, m_xOwningEntity.GetEntityID().m_uIndex);
		if (uDepth >= MAX_HIERARCHY_DEPTH)
		{
			break; // Safety break even in release builds
		}

		Zenith_TransformComponent& xParentTransform = pxSceneData->GetEntity(uParentID).GetComponent<Zenith_TransformComponent>();

		Zenith_Maths::Vector3 xParentPos;
		Zenith_Maths::Quat xParentRot;
		xParentTransform.GetPosition(xParentPos);
		xParentTransform.GetRotation(xParentRot);

		glm::mat4 xParentTranslation = glm::translate(glm::identity<glm::mat4>(), xParentPos);
		glm::mat4 xParentRotation = glm::mat4_cast(xParentRot);
		glm::mat4 xParentScale = glm::scale(glm::identity<glm::mat4>(), xParentTransform.m_xScale);
		glm::mat4 xParentMatrix = xParentTranslation * xParentRotation * xParentScale;

		xMatOut = xParentMatrix * xMatOut;
		uParentID = xParentTransform.m_xParentEntityID;
		++uDepth;
	}
}

void Zenith_TransformComponent::WriteToDataStream(Zenith_DataStream& xStream)
{
	// Write position, rotation, and scale
	// Note: We get current values from physics if rigid body exists
	Zenith_Maths::Vector3 xPos;
	Zenith_Maths::Quat xRot;
	GetPosition(xPos);
	GetRotation(xRot);

	xStream << xPos;
	xStream << xRot;
	xStream << m_xScale;

	// Write parent entity index for hierarchy reconstruction (generation is runtime only)
	uint32_t uParentIndex = m_xParentEntityID.IsValid() ? m_xParentEntityID.m_uIndex : Zenith_EntityID::INVALID_INDEX;
	xStream << uParentIndex;
}

void Zenith_TransformComponent::ReadFromDataStream(Zenith_DataStream& xStream)
{
	// Read position, rotation, and scale
	xStream >> m_xPosition;
	xStream >> m_xRotation;
	xStream >> m_xScale;

	// Read parent file index - stored in pending member for scene to resolve after all entities loaded
	uint32_t uParentFileIndex;
	xStream >> uParentFileIndex;
	m_uPendingParentFileIndex = uParentFileIndex;
	// Note: Children are NOT serialized - they're rebuilt from parent references
	// The scene will call SetParentByID after mapping file indices to new EntityIDs
}