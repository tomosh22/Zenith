#pragma once

#include "Collections/Zenith_Vector.h"
#include <cstdint>
#include <functional>  // Required for std::hash<Zenith_EntityID> specialization (NOT for std::function)

// Forward declarations
class Zenith_SceneData;
class Zenith_DataStream;
struct Zenith_Scene;

//--------------------------------------------------------------------------
// Zenith_EntityID - Unity-style entity identifier with generation counter
// The index identifies the slot in entity storage.
// The generation detects stale references (incremented on slot reuse).
//--------------------------------------------------------------------------
struct Zenith_EntityID
{
	uint32_t m_uIndex = INVALID_INDEX;
	uint32_t m_uGeneration = 0;

	// Pack into single 64-bit value for efficient hashing/comparison
	uint64_t GetPacked() const { return (static_cast<uint64_t>(m_uGeneration) << 32) | m_uIndex; }

	static Zenith_EntityID FromPacked(uint64_t ulPacked)
	{
		return { static_cast<uint32_t>(ulPacked), static_cast<uint32_t>(ulPacked >> 32) };
	}

	bool operator==(const Zenith_EntityID& xOther) const
	{
		return m_uIndex == xOther.m_uIndex && m_uGeneration == xOther.m_uGeneration;
	}

	bool operator!=(const Zenith_EntityID& xOther) const { return !(*this == xOther); }

	// Check if this ID could potentially be valid (not the invalid sentinel)
	bool IsValid() const { return m_uIndex != INVALID_INDEX; }

	static constexpr uint32_t INVALID_INDEX = 0xFFFFFFFF;
};

// Sentinel value for invalid entity references
static constexpr Zenith_EntityID INVALID_ENTITY_ID = { Zenith_EntityID::INVALID_INDEX, 0 };

// Hash specialization for use in unordered_map/unordered_set
namespace std
{
	template<>
	struct hash<Zenith_EntityID>
	{
		size_t operator()(const Zenith_EntityID& xID) const
		{
			return std::hash<uint64_t>{}(xID.GetPacked());
		}
	};
}

/**
 * Zenith_Entity - Lightweight handle to an entity in a scene
 *
 * This is a VALUE TYPE that can be freely copied. It only holds:
 * - A pointer to the scene data
 * - The entity ID (index + generation)
 *
 * All entity state (name, enabled, transient) is stored in the scene's
 * EntitySlot and accessed through this handle. This eliminates the
 * synchronization bugs that occurred when entity state was duplicated.
 *
 * THREADING: Zenith_Entity has no mutable state — GetSceneData() re-resolves
 * the SceneData pointer from the entity slot on every call. Safe to share
 * across threads as long as the underlying scene system isn't being mutated
 * concurrently (engine-wide invariant: scene mutation is main-thread only).
 *
 * Usage:
 *   Zenith_Entity xEntity = pxSceneData->GetEntity(entityID);
 *   xEntity.SetName("MyEntity");  // Modifies slot directly
 *   xEntity.SetEnabled(false);    // Modifies slot directly
 */
class Zenith_Entity
{
	// Friends granted access to the privatised serialization / hierarchy-internal
	// surface below. Zenith_SceneData drives scene (de)serialization and the
	// active-in-hierarchy dispatch checks. Keep this list minimal - see the private
	// section for what it gates.
	friend class Zenith_SceneData;

public:
	// Default constructor - creates an invalid entity handle
	Zenith_Entity() = default;

	// Construct a handle from scene data and ID (used internally by SceneData::GetEntity)
	Zenith_Entity(Zenith_SceneData* pxSceneData, Zenith_EntityID xID);

	// NOTE: the creating ctor Zenith_Entity(Zenith_SceneData*, const std::string&)
	// was removed in Phase 3 (ECS leaf-extraction). Create entities through
	// g_xEngine.Scenes().CreateEntity(...) / CreateEntityBare(...) instead.

	//--------------------------------------------------------------------------
	// Validity Check
	//--------------------------------------------------------------------------

	/**
	 * Check if this entity handle is valid (points to an existing entity)
	 * Returns false if: scene is null, ID is invalid, or entity was destroyed
	 */
	bool IsValid() const;

	//--------------------------------------------------------------------------
	// Component Operations (unchanged API)
	//
	// Template bodies for the methods below live in Zenith_Entity.inl. The .inl
	// is included from Zenith_Scene.h after Zenith_SceneData is defined (the
	// bodies need access to component pools that Zenith_Entity.h alone cannot
	// see). Any TU that wants to call these templates must include
	// Zenith_Scene.h, not just Zenith_Entity.h.
	//--------------------------------------------------------------------------

	template<typename T, typename... Args>
	T& AddComponent(Args&&... args);

	template<typename T>
	bool HasComponent() const;

	template<typename T>
	T& GetComponent() const;

	// Safe component accessor - returns nullptr if entity invalid or component doesn't exist
	template<typename T>
	T* TryGetComponent() const;

	template<typename T>
	void RemoveComponent();

	//--------------------------------------------------------------------------
	// Entity State Accessors (delegate to EntitySlot)
	//--------------------------------------------------------------------------

	Zenith_EntityID GetEntityID() const { return m_xEntityID; }

	/**
	 * Get the scene data that owns this entity.
	 * Returns nullptr if the scene has been unloaded (stale entity reference).
	 * Uses scene handle + generation for safe validation during async unload.
	 */
	Zenith_SceneData* GetSceneData() const;

	/**
	 * Get the scene handle for this entity's scene
	 */
	Zenith_Scene GetScene() const;

	// Name accessors - delegate to EntitySlot
	const std::string& GetName() const;
	void SetName(const std::string& strName);

	/**
	 * Check if this entity's own enabled flag is set (Unity's activeSelf).
	 * Does NOT check parent hierarchy - use IsActiveInHierarchy() for that.
	 */
	bool IsEnabled() const;

	/**
	 * Enable or disable this entity. Calls OnEnable/OnDisable on all components.
	 * When disabling, also dispatches OnDisable to children whose activeSelf is true.
	 * When enabling, also dispatches OnEnable to children whose activeSelf is true.
	 */
	void SetEnabled(bool bEnabled);

	/**
	 * Check if this entity is transient (not serialized to scene file)
	 */
	bool IsTransient() const;

	/**
	 * Mark entity as transient - it will NOT be saved when scene is serialized.
	 */
	void SetTransient(bool bTransient);

	//--------------------------------------------------------------------------
	// Persistence Across Scene Loads
	//--------------------------------------------------------------------------

	/**
	 * Mark this entity to persist across scene loads.
	 * Equivalent to Unity's DontDestroyOnLoad.
	 * Entity is moved to the persistent scene.
	 */
	void DontDestroyOnLoad();

	//--------------------------------------------------------------------------
	// Destruction (Unity-style API)
	//--------------------------------------------------------------------------

	/**
	 * Mark this entity for destruction at end of frame.
	 * Equivalent to Unity's Destroy(gameObject).
	 * Children are also marked for destruction.
	 *
	 * @note Must be called from main thread.
	 */
	void Destroy();

	/**
	 * Mark this entity for destruction after a delay (seconds).
	 * Equivalent to Unity's Destroy(gameObject, t). A delay <= 0 destroys at
	 * end of frame (same as Destroy()).
	 *
	 * @note Must be called from main thread.
	 */
	void Destroy(float fDelay);

	/**
	 * Immediately destroy this entity (current-frame destruction).
	 * Equivalent to Unity's DestroyImmediate(gameObject).
	 * Use with caution - mainly for editor/test scenarios.
	 *
	 * @note Must be called from main thread.
	 */
	void DestroyImmediate();

	//--------------------------------------------------------------------------
	// Cross-scene movement
	//--------------------------------------------------------------------------

	/**
	 * Move this entity (and its children) to another scene. Only root entities
	 * can be moved (Unity parity). The handle stays valid — EntityID is unchanged.
	 *
	 * @note Must be called from main thread.
	 */
	void MoveToScene(Zenith_Scene xTarget);

	//--------------------------------------------------------------------------
	// Parent/Child Hierarchy (SLOT-BACKED — single source of truth)
	//
	// Phase 5a (KEYSTONE): the parent/child links live on Zenith_EntitySlot, not
	// on any owning component any more. These methods read/write the slot
	// directly. SetParent holds the full bidirectional-link logic (same-scene check,
	// descendant-cycle check, old/new child-list maintenance, active-cache invalidation).
	//--------------------------------------------------------------------------

	Zenith_EntityID GetParentEntityID() const;
	bool HasParent() const;
	void SetParent(Zenith_EntityID uParentID);
	const Zenith_Vector<Zenith_EntityID>& GetChildEntityIDs() const;
	bool HasChildren() const;
	uint32_t GetChildCount() const;
	bool IsRoot() const;
	// Phase 7a: the removed transform-accessor (0 callers) was deleted. It named a
	// concrete engine component; callers fetch that component via GetComponent<T>()
	// directly. Removing it lets the sibling ECS TU Zenith_Scene.cpp drop its include
	// of that concrete component.

	// Hierarchy safety: true if this entity is a descendant of uAncestorID.
	// Walks the SLOT parent chain (used to reject cycles in SetParent). Mirrors
	// the former component-side IsDescendantOf semantics exactly.
	bool IsDescendantOf(Zenith_EntityID uAncestorID) const;

	//--------------------------------------------------------------------------
	// Pending-parent file index (scene/prefab load only)
	//
	// Phase 5a: relocated to the slot. On load, the parent-owning component's
	// ReadFromDataStream sinks the parent file-index here (via the now-public
	// SetPendingParentFileIndex below); the post-load resolve pass (SceneData
	// serialization) maps it to a real EntityID, calls SetParent, then clears it.
	//
	// GetPendingParentFileIndex stays public: a unit test in
	// Zenith/Core/Zenith_UnitTests.Tests.inl reads it directly and is not a friend.
	//--------------------------------------------------------------------------
	uint32_t GetPendingParentFileIndex() const;

	//--------------------------------------------------------------------------
	// Slot-backed hierarchy / load helpers (public)
	//
	// These are public, component-agnostic slot operations. Engine-side code that
	// owns a hierarchy component (e.g. its destructor-time detach, or its v6->v7
	// load migration) calls them; they name no concrete component.
	//--------------------------------------------------------------------------

	// Detach helpers (slot-backed). DetachAllChildren clears each child's slot
	// parent (preserving the per-child order) then bulk-clears this slot's list.
	void DetachFromParent();
	void DetachAllChildren();

	// Pending-parent file index writer (scene/prefab load only). The matching reader
	// GetPendingParentFileIndex() is public above; the clearer
	// ClearPendingParentFileIndex() stays private (resolve-pass / friend-only).
	void SetPendingParentFileIndex(uint32_t uIndex);

	// Serialization methods for Zenith_DataStream
	//
	// WriteToDataStream stays public: unit tests in
	// Zenith/Core/Zenith_UnitTests.Tests.inl call it directly on an entity and are
	// not friends. ReadFromDataStream is part of the public load API.
	void WriteToDataStream(Zenith_DataStream& xStream);
	void ReadFromDataStream(Zenith_DataStream& xStream);

	//--------------------------------------------------------------------------
	// Comparison Operators
	//--------------------------------------------------------------------------

	bool operator==(const Zenith_Entity& xOther) const
	{
		// EntityIDs are globally unique (not per-scene), so comparing only IDs is sufficient
		return m_xEntityID == xOther.m_xEntityID;
	}

	bool operator!=(const Zenith_Entity& xOther) const
	{
		return !(*this == xOther);
	}

private:
	// Lifecycle helpers for SetEnabled - separated for readability
	void DispatchEnableLifecycle(Zenith_SceneData* pxSceneData);
	void DispatchDisableLifecycle(Zenith_SceneData* pxSceneData);

	// Iterative hierarchy propagation (avoids unbounded recursion)
	static void PropagateHierarchyEnabled(Zenith_SceneData* pxSceneData, Zenith_EntityID xParentID, bool bBecomingActive);

	//--------------------------------------------------------------------------
	// Privatised surface (gated by the friends declared at the top of the class)
	//--------------------------------------------------------------------------

	/**
	 * Check if this entity is active in the scene hierarchy (Unity's activeInHierarchy).
	 * Returns true only if this entity AND all ancestors are enabled.
	 * Update/FixedUpdate/LateUpdate only run on entities where this returns true.
	 * Friend: Zenith_SceneData (lifecycle dispatch); also called internally.
	 */
	bool IsActiveInHierarchy() const;

	// Pending-parent file index clearer (scene/prefab load only). The writer
	// SetPendingParentFileIndex() and reader GetPendingParentFileIndex() are public
	// above; this clearer is invoked only by the SceneData resolve pass (friend).
	void ClearPendingParentFileIndex();

	Zenith_EntityID m_xEntityID;
};
