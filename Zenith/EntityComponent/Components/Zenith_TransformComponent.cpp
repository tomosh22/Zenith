// This file uses Jolt Physics - disable memory tracking macro to avoid conflicts
#include "Zenith.h"
#include "Core/Zenith_Engine.h"
#define ZENITH_PLACEMENT_NEW_ZONE
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "ZenithECS/Zenith_ComponentMeta.h"
#include "Physics/Zenith_Physics.h"
#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/Body.h>

void Zenith_TransformComponent::RegisterProperties(Zenith_Vector<Zenith_PropertyDescriptor>& axProperties)
{
	// Use SETTER form — Transform's setters are STATEFUL:
	//   - SetPosition / SetRotation route through Jolt BodyInterface when a
	//     ColliderComponent is present, so the physics body stays in sync.
	//   - SetScale regenerates the physics mesh and rebuilds the collider
	//     when ModelComponent / ColliderComponent are present.
	// A raw field write would leave physics + cached collider geometry pointing
	// at the pre-override value, which is the bug code review caught.
	ZENITH_REGISTER_COMPONENT_PROPERTY_SETTER(
		Zenith_TransformComponent, &Zenith_TransformComponent::SetPosition,
		Zenith_Maths::Vector3, "Position", axProperties);
	ZENITH_REGISTER_COMPONENT_PROPERTY_SETTER(
		Zenith_TransformComponent, &Zenith_TransformComponent::SetRotation,
		Zenith_Maths::Quat, "Rotation", axProperties);
	ZENITH_REGISTER_COMPONENT_PROPERTY_SETTER(
		Zenith_TransformComponent, &Zenith_TransformComponent::SetScale,
		Zenith_Maths::Vector3, "Scale", axProperties);
}

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
	Zenith_SceneSystem& xScenes = g_xEngine.Scenes();
	Zenith_Scene xActiveScene = xScenes.GetActiveScene();
	Zenith_SceneData* pxActiveSceneData = xScenes.GetSceneData(xActiveScene);
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

	// Safe to perform hierarchy cleanup. Phase 5b: the Transform's own
	// DetachFromParent/DetachAllChildren shims were removed -- call the slot-backed
	// Zenith_Entity API directly (the shims were 1:1 forwards to exactly these, so
	// the cleanup is behaviour-identical: this entity is unparented and its children
	// are detached on the slot, which is what CollectResetHierarchy /
	// CollectHierarchyDepthFirst rely on when a Transform is destroyed individually).
	m_xOwningEntity.DetachFromParent();
	m_xOwningEntity.DetachAllChildren();
}

// Phase 5b: the Transform hierarchy navigation implementations (TryGetParent /
// GetParentEntity / TryGetChildAt / GetChildEntityAt / SetParent(Transform*), plus
// the former inline shims SetParentByID / GetParentEntityID / GetChildEntityIDs /
// GetChildCount / HasParent / IsRoot / DetachFromParent / DetachAllChildren /
// IsDescendantOf / pending-parent accessors / ForEachChild) were all REMOVED. The
// hierarchy is owned by Zenith_EntitySlot and queried through Zenith_Entity /
// Zenith_Scene. IsDescendantOfUnsafe was already gone in 5a. Only the
// position/rotation/scale pose, BuildModelMatrix (slot-walking), and the
// serialization parent bridge remain below.

void Zenith_TransformComponent::SetPosition(const Zenith_Maths::Vector3& xPos)
{
	// Always keep the cached transform in sync, even when a physics body is
	// authoritative. Otherwise a later body teardown (e.g. RebuildCollider,
	// which reads GetPosition AFTER destroying the body) falls back to a stale
	// m_xPosition and snaps the rebuilt body to the wrong place.
	m_xPosition = xPos;

	// Mirror the move onto the Jolt body when one exists.
	// Use BodyInterface with BodyID for thread-safe access.
	Zenith_Physics& xPhysics = g_xEngine.Physics();
	if (m_xOwningEntity.HasComponent<Zenith_ColliderComponent>() && xPhysics.GetJoltSystem() != nullptr)
	{
		Zenith_ColliderComponent& xCollider = m_xOwningEntity.GetComponent<Zenith_ColliderComponent>();
		if (xCollider.HasValidBody())
		{
			JPH::BodyInterface& xBodyInterface = xPhysics.GetJoltSystem()->GetBodyInterface();
			JPH::Vec3 xJoltPos(xPos.x, xPos.y, xPos.z);
			xBodyInterface.SetPosition(JPH::BodyID(xCollider.GetBodyID().m_uID), xJoltPos, JPH::EActivation::Activate);
		}
	}
}

void Zenith_TransformComponent::SetRotation(const Zenith_Maths::Quat& xRot)
{
	// Always keep the cached transform in sync (see SetPosition) so a later
	// body teardown/rebuild reads the correct rotation rather than a stale one.
	m_xRotation = xRot;

	// Mirror the move onto the Jolt body when one exists.
	// Use BodyInterface with BodyID for thread-safe access.
	Zenith_Physics& xPhysics = g_xEngine.Physics();
	if (m_xOwningEntity.HasComponent<Zenith_ColliderComponent>() && xPhysics.GetJoltSystem() != nullptr)
	{
		Zenith_ColliderComponent& xCollider = m_xOwningEntity.GetComponent<Zenith_ColliderComponent>();
		if (xCollider.HasValidBody())
		{
			JPH::BodyInterface& xBodyInterface = xPhysics.GetJoltSystem()->GetBodyInterface();
			JPH::Quat xJoltRot(xRot.x, xRot.y, xRot.z, xRot.w);
			xBodyInterface.SetRotation(JPH::BodyID(xCollider.GetBodyID().m_uID), xJoltRot, JPH::EActivation::Activate);
		}
	}
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
	Zenith_Physics& xPhysics = g_xEngine.Physics();
	if (m_xOwningEntity.HasComponent<Zenith_ColliderComponent>() && xPhysics.GetJoltSystem() != nullptr)
	{
		Zenith_ColliderComponent& xCollider = m_xOwningEntity.GetComponent<Zenith_ColliderComponent>();
		if (xCollider.HasValidBody())
		{
			// Use BodyInterface for safe access - never access Body pointer directly
			JPH::BodyInterface& xBodyInterface = xPhysics.GetJoltSystem()->GetBodyInterfaceNoLock();
			JPH::Vec3 xJoltPos = xBodyInterface.GetPosition(JPH::BodyID(xCollider.GetBodyID().m_uID));
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
	Zenith_Physics& xPhysics = g_xEngine.Physics();
	if (m_xOwningEntity.HasComponent<Zenith_ColliderComponent>() && xPhysics.GetJoltSystem() != nullptr)
	{
		Zenith_ColliderComponent& xCollider = m_xOwningEntity.GetComponent<Zenith_ColliderComponent>();
		if (xCollider.HasValidBody())
		{
			// Use BodyInterface for safe access - never access Body pointer directly
			JPH::BodyInterface& xBodyInterface = xPhysics.GetJoltSystem()->GetBodyInterfaceNoLock();
			JPH::Quat xJoltRot = xBodyInterface.GetRotation(JPH::BodyID(xCollider.GetBodyID().m_uID));
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

void Zenith_TransformComponent::CommitPhysicsTransformToCache()
{
	// GetPosition/GetRotation read the live body when one is valid; copy those
	// values straight into the cache so a subsequent body destroy + rebuild
	// (which reads the cache once the body is gone) lands at the current world
	// transform. No-op (writes back identical values) when there is no body.
	Zenith_Maths::Vector3 xPos;
	Zenith_Maths::Quat xRot;
	GetPosition(xPos);
	GetRotation(xRot);
	m_xPosition = xPos;
	m_xRotation = xRot;
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

	// Walk parent chain via EntityIDs (safe against pool relocations).
	// Phase 5a: the parent links are read from the SLOT (single source of truth)
	// via Zenith_SceneData::GetParentEntityIDUnchecked — the per-parent transform
	// math (position/rotation/scale) is unchanged. That non-asserting leaf accessor
	// preserves the prior thread-affinity of this hot path (the OLD code read the
	// slot directly with no main-thread assert, and the parent Transform via
	// GetComponentFromEntity, which permits render-task reads); its doc comment
	// carries the render-task rationale.
	Zenith_SceneData* pxSceneData = m_xOwningEntity.GetSceneData();
	if (!pxSceneData)
	{
		return;
	}
	Zenith_EntityID uMyID = m_xOwningEntity.GetEntityID();
	Zenith_EntityID uParentID = pxSceneData->GetParentEntityIDUnchecked(uMyID);

	// Depth limit to catch any circular references that slip through
	// (should never happen with SetParent checks, but safety first)
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
		// Next parent: the current parent's SLOT link via the non-asserting leaf
		// accessor (no main-thread assert — render-task safe).
		uParentID = pxSceneData->GetParentEntityIDUnchecked(uParentID);
		++uDepth;
	}
}

void Zenith_TransformComponent::WriteToDataStream(Zenith_DataStream& xStream)
{
	// Scene v7 (Phase 7a): the Transform blob is pos/rot/scale ONLY. The hierarchy
	// parent file-index moved OUT to the entity record (written by
	// Zenith_Entity::WriteToDataStream). The per-component size prefix still wraps this
	// blob, so dropping the parent simply shrinks the payload by one u32; the bounded
	// component deserializer keeps every reader aligned (see Zenith_ComponentMeta).
	// Note: We get current values from physics if a rigid body exists.
	Zenith_Maths::Vector3 xPos;
	Zenith_Maths::Quat xRot;
	GetPosition(xPos);
	GetRotation(xRot);

	xStream << xPos;
	xStream << xRot;
	xStream << m_xScale;
}

void Zenith_TransformComponent::ReadFromDataStream(Zenith_DataStream& xStream)
{
	// Current-format (v7) reader: pos/rot/scale only -- the parent lives in the entity
	// record now and is sunk to the slot pending-parent by the entity-record reader.
	xStream >> m_xPosition;
	xStream >> m_xRotation;
	xStream >> m_xScale;
}

void Zenith_TransformComponent::ReadFromDataStream(Zenith_DataStream& xStream, u_int uReadSchemaVersion)
{
	// Version-aware reader (preferred by the meta registry -- see ComponentMeta
	// HasVersionedReadFromDataStream). The schema version comes from the file: for a
	// scene v7 / current prefab blob it is uSchemaVersion (7); for a legacy scene
	// v3/4/5/6 / pre-v7 prefab blob it is < 7 (v3/4/5 default to 1 because those scene
	// versions wrote no per-component schemaVersion field; v6 wrote 1 because the
	// Transform had not yet opted in).
	//
	// pos/rot/scale are common to every version. The LEGACY-FORMAT KNOWLEDGE -- that a
	// pre-v7 blob ALSO carries the hierarchy parent file-index -- lives ONLY here, in
	// this engine-side reader; the ECS core just runs ResolvePendingParents afterwards.
	xStream >> m_xPosition;
	xStream >> m_xRotation;
	xStream >> m_xScale;

	if (uReadSchemaVersion < 7u)
	{
		// Legacy migration: consume the in-blob parent file-index and SINK it into the
		// slot pending-parent (Phase 5a). The scene's post-load ResolvePendingParents
		// maps the file-index to a real EntityID and calls Zenith_Entity::SetParent.
		// Guard against a truncated tail: the bounded component deserializer forces the
		// cursor to (payloadStart + declaredSize) regardless, so if the declared size
		// does not actually include the parent u32 (e.g. a v7-format blob mislabelled
		// as schema < 7 in a hand-crafted stream), we simply skip the read rather than
		// over-read past end-of-stream.
		if (xStream.GetCursor() + sizeof(uint32_t) <= xStream.GetCapacity())
		{
			uint32_t uParentFileIndex;
			xStream >> uParentFileIndex;
			m_xOwningEntity.SetPendingParentFileIndex(uParentFileIndex);
		}
		// Children are NOT serialized -- they are rebuilt from parent references during
		// the post-load resolve pass.
	}
	// uReadSchemaVersion >= 7: nothing more to read; the parent came from the entity record.
}