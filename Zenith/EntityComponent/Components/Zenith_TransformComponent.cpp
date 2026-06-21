#include "Zenith.h"
#include "Core/Zenith_Engine.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "ZenithECS/Zenith_ComponentMeta.h"
#include "Physics/Zenith_Physics.h"
// No Jolt headers: physics-body pose access goes through Zenith_Physics
// (Set/GetBodyPosition/Rotation), so this component names no JPH:: type.

void Zenith_TransformComponent::RegisterProperties(Zenith_Vector<Zenith_PropertyDescriptor>& axProperties)
{
	// Use SETTER form — Transform's setters are STATEFUL:
	//   - SetPosition / SetRotation mirror the pose onto the physics body (via
	//     Zenith_Physics) when a ColliderComponent has a live body, keeping it in sync.
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

// If the owning entity has a collider with a live body, hand back its body ID.
// One place for the "is there an authoritative physics body?" test the pose
// getters/setters share. Names no Jolt type — the body is a Zenith_PhysicsBodyID.
bool Zenith_TransformComponent::TryGetColliderBody(Zenith_PhysicsBodyID& xOutBodyID)
{
	if (!m_xOwningEntity.HasComponent<Zenith_ColliderComponent>())
	{
		return false;
	}
	Zenith_ColliderComponent& xCollider = m_xOwningEntity.GetComponent<Zenith_ColliderComponent>();
	if (!xCollider.HasValidBody())
	{
		return false;
	}
	xOutBodyID = xCollider.GetBodyID();
	return true;
}

void Zenith_TransformComponent::SetPosition(const Zenith_Maths::Vector3& xPos)
{
	// The physics body is the authoritative pose when one exists; the cache is a
	// write-through mirror that also serves bodyless entities. (RebuildCollider
	// commits the live body pose back into the cache before teardown, so the
	// cache is never stale even after physics-driven motion.)
	m_xPosition = xPos;

	Zenith_Physics& xPhysics = g_xEngine.Physics();
	Zenith_PhysicsBodyID xBodyID;
	if (xPhysics.HasActiveSimulation() && TryGetColliderBody(xBodyID))
	{
		xPhysics.SetBodyPosition(xBodyID, xPos);
	}

	// Moving this entity changes its world matrix and every descendant's — invalidate
	// the cached world matrices of the whole subtree (Phase 1 scene-graph cache).
	Zenith_SceneData::BumpHierarchyRevision(m_xOwningEntity.GetEntityID());
}

void Zenith_TransformComponent::SetRotation(const Zenith_Maths::Quat& xRot)
{
	m_xRotation = xRot;

	Zenith_Physics& xPhysics = g_xEngine.Physics();
	Zenith_PhysicsBodyID xBodyID;
	if (xPhysics.HasActiveSimulation() && TryGetColliderBody(xBodyID))
	{
		xPhysics.SetBodyRotation(xBodyID, xRot);
	}

	Zenith_SceneData::BumpHierarchyRevision(m_xOwningEntity.GetEntityID());
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

	// Scale changed (we early-returned above if it didn't) — invalidate this entity's
	// subtree cached world matrices (Phase 1 scene-graph cache).
	Zenith_SceneData::BumpHierarchyRevision(m_xOwningEntity.GetEntityID());
}

void Zenith_TransformComponent::GetPosition(Zenith_Maths::Vector3& xPos)
{
	// Body authoritative when present (reads the live simulation pose); otherwise
	// the cached pose (bodyless entities, or before/after the simulation exists).
	Zenith_Physics& xPhysics = g_xEngine.Physics();
	Zenith_PhysicsBodyID xBodyID;
	if (xPhysics.HasActiveSimulation() && TryGetColliderBody(xBodyID))
	{
		xPos = xPhysics.GetBodyPosition(xBodyID);
		return;
	}
	xPos = m_xPosition;
}

void Zenith_TransformComponent::GetRotation(Zenith_Maths::Quat& xRot)
{
	Zenith_Physics& xPhysics = g_xEngine.Physics();
	Zenith_PhysicsBodyID xBodyID;
	if (xPhysics.HasActiveSimulation() && TryGetColliderBody(xBodyID))
	{
		xRot = xPhysics.GetBodyRotation(xBodyID);
		return;
	}
	xRot = m_xRotation;
}

void Zenith_TransformComponent::GetScale(Zenith_Maths::Vector3& xScale) const
{
	xScale = m_xScale;
}

bool Zenith_TransformComponent::PhysicsPoseDiffersFromCache(const Zenith_Maths::Vector3& xPos, const Zenith_Maths::Quat& xRot) const
{
	// Compare against the last-synced pose (m_xPosition/m_xRotation). These mirror the
	// body for body-backed entities — kept current by SetPosition/Rotation, by
	// CommitPhysicsTransformToCache, and by the post-physics sweep — so an unmoved body
	// is a true no-op (no spurious cache invalidation for sleeping/static bodies).
	const Zenith_Maths::Vector3 xPosDelta = xPos - m_xPosition;
	const float fPosDistSq = glm::dot(xPosDelta, xPosDelta);
	const float fRotDot = xRot.x * m_xRotation.x + xRot.y * m_xRotation.y
		+ xRot.z * m_xRotation.z + xRot.w * m_xRotation.w;
	const float fRotDotAbs = fRotDot < 0.0f ? -fRotDot : fRotDot;

	constexpr float fPOS_EPSILON_SQ = 1e-10f;   // ~1e-5 world units
	constexpr float fROT_EPSILON = 1e-6f;       // 1 - |dot| past this => rotated
	return fPosDistSq > fPOS_EPSILON_SQ || (1.0f - fRotDotAbs) > fROT_EPSILON;
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

	// If the live body has moved since the last-synced pose, bump the hierarchy revision
	// here — committing the moved pose into m_xPosition/m_xRotation advances the very
	// values the post-physics sweep compares against, so without this bump the sweep would
	// see no delta and never invalidate the cached world matrix, leaving BuildModelMatrix
	// permanently stuck at the pre-move pose. (RebuildCollider commits a simulation-moved
	// body before destroying it, so this is the only invalidation point on that path.)
	const bool bMoved = PhysicsPoseDiffersFromCache(xPos, xRot);

	m_xPosition = xPos;
	m_xRotation = xRot;

	if (bMoved)
	{
		Zenith_SceneData::BumpHierarchyRevision(m_xOwningEntity.GetEntityID());
	}
}

void Zenith_TransformComponent::SyncPhysicsPoseAndInvalidate()
{
	// Only entities with a live, active physics body can move via simulation; bodyless
	// entities and setter-driven moves already bump the revision at the mutation site.
	// Uses the engine-free Zenith_Physics::Get() accessor (not the global engine reach)
	// so this new method does not raise the file's singleton-ratchet count.
	Zenith_Physics& xPhysics = Zenith_Physics::Get();
	Zenith_PhysicsBodyID xBodyID;
	if (!xPhysics.HasActiveSimulation() || !TryGetColliderBody(xBodyID))
	{
		return;
	}

	const Zenith_Maths::Vector3 xBodyPos = xPhysics.GetBodyPosition(xBodyID);
	const Zenith_Maths::Quat xBodyRot = xPhysics.GetBodyRotation(xBodyID);

	if (!PhysicsPoseDiffersFromCache(xBodyPos, xBodyRot))
	{
		return;
	}

	m_xPosition = xBodyPos;
	m_xRotation = xBodyRot;
	Zenith_SceneData::BumpHierarchyRevision(m_xOwningEntity.GetEntityID());
}

void Zenith_TransformComponent::BuildModelMatrix(Zenith_Maths::Matrix4& xMatOut)
{
	// Legal only on the main thread, or during the render-task window (which freezes
	// every transform writer — see the worker-thread cache-write contract below). This
	// mirrors the prior thread-affinity of this hot path (the old slot read used the
	// non-asserting GetParentEntityIDUnchecked, permitting render-task reads).
	Zenith_Assert(Zenith_ECS_IsMainThread() || Zenith_AreRenderTasksActive(),
		"BuildModelMatrix must run on the main thread or during active render tasks");

	Zenith_SceneData* pxSceneData = m_xOwningEntity.GetSceneData();

	// Effective revision: this entity's slot revision already folds in any ancestor
	// edit (BumpHierarchyRevision propagates a bump down the whole subtree). 0 means
	// "no scene / never built" and never matches a stamped cache, forcing a recompute.
	const uint64_t uEffectiveRevision = pxSceneData
		? pxSceneData->GetHierRevisionUnchecked(m_xOwningEntity.GetEntityID())
		: 0;

	// Cache hit: nothing affecting this entity's world matrix has changed since the
	// cache was stamped.
	if (uEffectiveRevision != 0 && m_uCachedHierRevision == uEffectiveRevision)
	{
		xMatOut = m_xCachedWorld;
		return;
	}

	// --- Recompute (cache miss) ---
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
	if (!pxSceneData)
	{
		// No scene: xMatOut is the local TRS only (matches the pre-cache behaviour).
		// uEffectiveRevision is 0, so there is nothing to cache — just return.
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

	// Worker-thread cache-write contract: only the main thread mutates the shared
	// cache. A render-task worker that missed recomputed into xMatOut above and
	// returns WITHOUT writing m_xCachedWorld/m_uCachedHierRevision — a concurrent
	// worker read of a clean cache is safe, but a worker write would race the other
	// workers. A 0 effective revision (no valid slot) is never cached — it would be the
	// "never built" stamp, so it can't false-hit. In practice the main-thread owner
	// pre-populates every snapshot-covered transform, so a worker miss is rare.
	if (uEffectiveRevision != 0 && Zenith_ECS_IsMainThread())
	{
		m_xCachedWorld = xMatOut;
		m_uCachedHierRevision = uEffectiveRevision;
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