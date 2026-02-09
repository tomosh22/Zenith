#pragma once

#include "Collections/Zenith_Vector.h"
#include <cstdint>
#include <functional>  // Required for std::hash<Zenith_EntityID> specialization (NOT for std::function)

// Forward declarations
class Zenith_SceneData;
class Zenith_DataStream;
struct Zenith_Scene;
class Zenith_SceneManager;

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

class Zenith_TransformComponent;

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
 * Usage:
 *   Zenith_Entity xEntity = pxSceneData->GetEntity(entityID);
 *   xEntity.SetName("MyEntity");  // Modifies slot directly
 *   xEntity.SetEnabled(false);    // Modifies slot directly
 */
class Zenith_Entity
{
public:
	// Default constructor - creates an invalid entity handle
	Zenith_Entity() = default;

	// Construct a handle from scene data and ID (used internally by SceneData::GetEntity)
	Zenith_Entity(Zenith_SceneData* pxSceneData, Zenith_EntityID xID);


	// Create a NEW entity in the scene with the given name
	Zenith_Entity(Zenith_SceneData* pxSceneData, const std::string& strName);

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
	//--------------------------------------------------------------------------

	template<typename T, typename... Args>
	T& AddComponent(Args&&... args);

	template<typename T, typename... Args>
	T& AddOrReplaceComponent(Args&&... args);

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
	 * Check if this entity is active in the scene hierarchy (Unity's activeInHierarchy).
	 * Returns true only if this entity AND all ancestors are enabled.
	 * Update/FixedUpdate/LateUpdate only run on entities where this returns true.
	 */
	bool IsActiveInHierarchy() const;

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
	 * Immediately destroy this entity (current-frame destruction).
	 * Equivalent to Unity's DestroyImmediate(gameObject).
	 * Use with caution - mainly for editor/test scenarios.
	 *
	 * @note Must be called from main thread.
	 */
	void DestroyImmediate();

	//--------------------------------------------------------------------------
	// Parent/Child Hierarchy (delegates to TransformComponent)
	//--------------------------------------------------------------------------

	Zenith_EntityID GetParentEntityID() const;
	bool HasParent() const;
	void SetParent(Zenith_EntityID uParentID);
	const Zenith_Vector<Zenith_EntityID>& GetChildEntityIDs() const;
	bool HasChildren() const;
	uint32_t GetChildCount() const;
	bool IsRoot() const;
	Zenith_TransformComponent& GetTransform();

	// Serialization methods for Zenith_DataStream
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
	static void PropagateHierarchyEnabled(Zenith_SceneData* pxSceneData, Zenith_EntityID xParentID, bool bBecomingActive);

	Zenith_EntityID m_xEntityID;
	mutable Zenith_SceneData* m_pxCachedSceneData = nullptr;  // Cached for fast path in GetSceneData()
	mutable int m_iCachedSceneHandle = -1;  // Cached handle for safe validation (avoids dereferencing stale pointer)
};
