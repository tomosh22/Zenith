#pragma once

// Placement-new scope guard: Zenith_Vector and other containers use placement
// new internally, which conflicts with the engine's global operator new/delete
// overrides. Save the zone state, define it for this header, and restore on

#include "Collections/Zenith_Vector.h"
#include "Collections/Zenith_HashMap.h"
#include "ZenithECS/Zenith_Entity.h"
#include <string>

// Phase 5a: the three "global entity storage" arrays were class statics on
// Zenith_SceneData (s_axEntitySlots / s_axFreeEntityIndices /
// s_axEntityComponents). They are not per-scene -- they index across every
// loaded scene by Zenith_EntityID. Zenith_SceneSystem owns this storage now
// (Phase 2.1 relocated it off Zenith_Engine; reached via the leaf accessor
// Zenith_ECS_EntityStore()); Zenith_SceneData keeps using-aliases for back-compat
// so the qualified type names (Zenith_SceneData::Zenith_EntitySlot etc.) keep
// resolving.

// Entity lifecycle state. Was Zenith_SceneData::EntityLifecycleState.
enum class Zenith_EntityLifecycleState : uint8_t
{
	FREE = 0,            // Slot is unoccupied
	OCCUPIED = 1,        // Slot occupied but not yet awoken
	AWOKEN = 2,          // Awake() called
	PENDING_START = 3,   // Queued for Start() next frame
	STARTED = 4,         // Start() called, receives Update/LateUpdate
};

// Per-entity slot. Indexed by Zenith_EntityID::m_uIndex. Was
// Zenith_SceneData::Zenith_EntitySlot.
struct Zenith_EntitySlot
{
	std::string m_strName;
	bool m_bEnabled = true;
	bool m_bTransient = true;

	uint32_t m_uGeneration = 0;
	Zenith_EntityLifecycleState m_eLifecycle = Zenith_EntityLifecycleState::FREE;
	bool m_bMarkedForDestruction = false;  // Orthogonal: can overlay any active lifecycle state
	int m_iSceneHandle = -1;        // Which scene owns this entity

	// Supplementary flags (orthogonal to lifecycle progression)
	bool m_bCreatedDuringUpdate = false;
	bool m_bOnEnableDispatched = false;  // Tracks whether OnEnable has been dispatched (Unity parity: prevents double-dispatch)

	// Cached activeInHierarchy state (Unity parity: avoids O(depth) parent chain walk per call)
	// Rebuilt lazily in Zenith_Entity::IsActiveInHierarchy() on first access.
	// Invalidated at 4 sites via InvalidateActiveInHierarchyCache():
	//   1. Zenith_Entity::SetEnabled()    - own enabled flag changed
	//   2. Zenith_Entity::SetParent()     - parent hierarchy changed
	//   3. ReleaseSlot()                  - slot freed (reset to defaults)
	//   4. OccupySlot()                   - new entity (reset to defaults)
	mutable bool m_bActiveInHierarchy = true;
	mutable bool m_bActiveInHierarchyDirty = true;

	// Phase 5a (KEYSTONE): scene-graph hierarchy STORAGE. The slot is the single
	// source of truth for parent/child links (the hierarchy fields — parent id,
	// child list, pending-parent file index — were relocated here, formerly held by
	// the transform/hierarchy component). Storing the hierarchy on the slot — which
	// already survives cross-scene MoveEntityToScene and component-pool relocation,
	// and is indexed by the stable Zenith_EntityID — keeps every cached parent/child
	// EntityID valid across those operations exactly as the former component members did.
	//   * m_xParentEntityID      — the HIERARCHY parent (NOT the owning entity).
	//   * m_xChildEntityIDs      — authoritative, PushBack-ordered child list.
	//                              NOT serialized; rebuilt from parent file-indices
	//                              on load (Zenith_SceneData_Serialization.cpp).
	//   * m_uPendingParentFileIndex — load-time only: maps an old file index to a
	//                              new EntityID after all entities are deserialized.
	// All three are reset in ReleaseSlot()/OccupySlot() alongside the active cache.
	Zenith_EntityID m_xParentEntityID = INVALID_ENTITY_ID;
	Zenith_Vector<Zenith_EntityID> m_xChildEntityIDs;
	uint32_t m_uPendingParentFileIndex = Zenith_EntityID::INVALID_INDEX;

	// Scene-graph transform-cache revision. Bumped whenever this entity's world
	// transform could have changed — its own pose edited, a structural re-parent,
	// a physics pose sync, or (propagated by Zenith_SceneData::BumpHierarchyRevision)
	// any ancestor's edit. Zenith_TransformComponent caches a world matrix stamped
	// with the revision it was built against; a mismatch forces a recompute. The
	// counter is monotonic across slot recycle (ReleaseSlot/OccupySlot increment it
	// rather than reset) so a reused slot can never alias a previous occupant's
	// cached stamp. Written only on the main thread (pose/structural/physics-sync
	// writers all run before SetRenderTasksActive(true)); read render-task-safe via
	// Zenith_SceneData::GetHierRevisionUnchecked. Starts at 1 so a fresh transform
	// cache (stamped 0 = "never built") always misses on first use.
	uint64_t m_uHierRevision = 1;

	// Lifecycle state queries
	bool IsOccupied() const { return m_eLifecycle >= Zenith_EntityLifecycleState::OCCUPIED; }
	bool IsAwoken() const { return m_eLifecycle >= Zenith_EntityLifecycleState::AWOKEN; }
	bool IsPendingStart() const { return m_eLifecycle == Zenith_EntityLifecycleState::PENDING_START; }
	bool IsStarted() const { return m_eLifecycle >= Zenith_EntityLifecycleState::STARTED; }

#ifdef ZENITH_ASSERT
	void ValidateSlotState() const
	{
		if (m_eLifecycle == Zenith_EntityLifecycleState::FREE)
		{
			return; // Free slot, nothing to validate
		}
		Zenith_Assert(!m_bMarkedForDestruction || IsOccupied(), "Destroyed entity not occupied");
	}
#endif

	// Explicit lifecycle transition methods with invariant assertions
	void TransitionToAwoken()
	{
		Zenith_Assert(m_eLifecycle == Zenith_EntityLifecycleState::OCCUPIED, "Invalid state for Awoken transition (current=%u)", static_cast<uint8_t>(m_eLifecycle));
		m_eLifecycle = Zenith_EntityLifecycleState::AWOKEN;
	}
	void TransitionToPendingStart()
	{
		Zenith_Assert(m_eLifecycle == Zenith_EntityLifecycleState::AWOKEN, "Invalid state for PendingStart transition (current=%u)", static_cast<uint8_t>(m_eLifecycle));
		m_eLifecycle = Zenith_EntityLifecycleState::PENDING_START;
	}
	void TransitionToStarted()
	{
		Zenith_Assert(m_eLifecycle == Zenith_EntityLifecycleState::AWOKEN || m_eLifecycle == Zenith_EntityLifecycleState::PENDING_START,
			"Invalid state for Start transition (current=%u)", static_cast<uint8_t>(m_eLifecycle));
		m_eLifecycle = Zenith_EntityLifecycleState::STARTED;
	}
	// Revert from PENDING_START back to AWOKEN (used when entity is destroyed before Start fires)
	void RevertFromPendingStart()
	{
		Zenith_Assert(m_eLifecycle == Zenith_EntityLifecycleState::PENDING_START, "RevertFromPendingStart: not in PENDING_START state (current=%u)", static_cast<uint8_t>(m_eLifecycle));
		m_eLifecycle = Zenith_EntityLifecycleState::AWOKEN;
	}

	void TransitionToDestroying()
	{
		Zenith_Assert(IsOccupied() && !m_bMarkedForDestruction, "Invalid state for Destroy transition");
		m_bMarkedForDestruction = true;
	}

	// Reset all lifecycle flags and release the slot for reuse
	void ReleaseSlot()
	{
		m_eLifecycle = Zenith_EntityLifecycleState::FREE;
		m_bMarkedForDestruction = false;
		m_bCreatedDuringUpdate = false;
		m_bOnEnableDispatched = false;
		m_bActiveInHierarchy = true;
		m_bActiveInHierarchyDirty = true;
		m_iSceneHandle = -1;
		// Phase 5a: clear relocated hierarchy storage so a reused slot starts as a
		// detached root with no children (matches a freshly-constructed owning component's defaults).
		m_xParentEntityID = INVALID_ENTITY_ID;
		m_xChildEntityIDs.Clear();
		m_uPendingParentFileIndex = Zenith_EntityID::INVALID_INDEX;
		// Monotonic bump (NOT reset) so a recycled slot's revision can never equal a
		// previous occupant's cached transform stamp — forces a recompute on reuse.
		++m_uHierRevision;
	}

	// Initialize all lifecycle flags and occupy the slot for a new entity
	void OccupySlot(int iSceneHandle)
	{
		m_eLifecycle = Zenith_EntityLifecycleState::OCCUPIED;
		m_bMarkedForDestruction = false;
		m_bCreatedDuringUpdate = false;
		m_bOnEnableDispatched = false;
		m_bActiveInHierarchy = true;
		m_bActiveInHierarchyDirty = true;
		m_iSceneHandle = iSceneHandle;
		// Phase 5a: a freshly occupied slot is a detached root with no children.
		m_xParentEntityID = INVALID_ENTITY_ID;
		m_xChildEntityIDs.Clear();
		m_uPendingParentFileIndex = Zenith_EntityID::INVALID_INDEX;
		// Monotonic bump (NOT reset) so a recycled slot's revision can never equal a
		// previous occupant's cached transform stamp — forces a recompute on reuse.
		++m_uHierRevision;
	}
};

// Engine-owned holder for the three global entity arrays. Public members --
// every former Zenith_SceneData::s_axXxx call site now reads
// g_xEngine.EntityStore().m_axXxx directly. Reset() replaces the former
// Zenith_SceneData::ResetGlobalEntityStorage helper.
class Zenith_EntityStore
{
public:
	Zenith_EntityStore() = default;
	~Zenith_EntityStore() = default;

	Zenith_EntityStore(const Zenith_EntityStore&) = delete;
	Zenith_EntityStore& operator=(const Zenith_EntityStore&) = delete;

	// TypeID type used by per-entity component-index maps. Mirrors
	// Zenith_SceneData::TypeID, kept as a free typedef so this header
	// doesn't have to include Zenith_SceneData.h (which would cycle).
	using TypeID = u_int;

	Zenith_Vector<Zenith_EntitySlot>                               m_axEntitySlots;
	Zenith_Vector<uint32_t>                                        m_axFreeEntityIndices;
	Zenith_Vector<Zenith_HashMap<TypeID, u_int>>                   m_axEntityComponents;

	void Reset()
	{
		m_axEntitySlots.Clear();
		m_axFreeEntityIndices.Clear();
		m_axEntityComponents.Clear();
	}
};

