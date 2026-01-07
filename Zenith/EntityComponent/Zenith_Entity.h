#pragma once

#include "Collections/Zenith_Vector.h"
#include <cstdint>
#include <functional>

class Zenith_Scene;
class Zenith_DataStream;

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
 * Zenith_Entity - Lightweight handle to an entity in the scene
 *
 * This is a VALUE TYPE that can be freely copied. It only holds:
 * - A pointer to the scene
 * - The entity ID (index + generation)
 *
 * All entity state (name, enabled, transient) is stored in the scene's
 * EntitySlot and accessed through this handle. This eliminates the
 * synchronization bugs that occurred when entity state was duplicated.
 *
 * Usage:
 *   Zenith_Entity xEntity = scene.GetEntity(entityID);
 *   xEntity.SetName("MyEntity");  // Modifies slot directly
 *   xEntity.SetEnabled(false);    // Modifies slot directly
 */
class Zenith_Entity
{
public:
	// Default constructor - creates an invalid entity handle
	Zenith_Entity()
		: m_pxParentScene(nullptr)
		, m_xEntityID(INVALID_ENTITY_ID)
	{
	}

	// Construct a handle from scene and ID (used internally by Scene::GetEntity)
	Zenith_Entity(Zenith_Scene* pxScene, Zenith_EntityID xID)
		: m_pxParentScene(pxScene)
		, m_xEntityID(xID)
	{
	}

	// Create a NEW entity in the scene with the given name
	Zenith_Entity(Zenith_Scene* pxScene, const std::string& strName);

	// Create a NEW entity with a specific ID (for scene loading)
	Zenith_Entity(Zenith_Scene* pxScene, Zenith_EntityID xID, const std::string& strName);

	// Initialize an invalid entity handle to create a new entity
	void Initialise(Zenith_Scene* pxScene, const std::string& strName);
	void Initialise(Zenith_Scene* pxScene, Zenith_EntityID xID, const std::string& strName);

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
	Zenith_Scene* GetScene() const { return m_pxParentScene; }

	// Name accessors - delegate to EntitySlot
	const std::string& GetName() const;
	void SetName(const std::string& strName);

	/**
	 * Check if this entity is enabled. Disabled entities skip Update/FixedUpdate.
	 */
	bool IsEnabled() const;

	/**
	 * Enable or disable this entity. Calls OnEnable/OnDisable on all components.
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
	// Parent/Child Hierarchy (delegates to TransformComponent)
	//--------------------------------------------------------------------------

	Zenith_EntityID GetParentEntityID() const;
	bool HasParent() const;
	void SetParent(Zenith_EntityID uParentID);
	Zenith_Vector<Zenith_EntityID> GetChildEntityIDs() const;
	bool HasChildren() const;
	uint32_t GetChildCount() const;
	bool IsRoot() const;
	Zenith_TransformComponent& GetTransform();

	// Serialization methods for Zenith_DataStream
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

	//--------------------------------------------------------------------------
	// Comparison Operators
	//--------------------------------------------------------------------------

	bool operator==(const Zenith_Entity& xOther) const
	{
		return m_xEntityID == xOther.m_xEntityID && m_pxParentScene == xOther.m_pxParentScene;
	}

	bool operator!=(const Zenith_Entity& xOther) const
	{
		return !(*this == xOther);
	}

	// Legacy: public access to scene pointer for compatibility
	// TODO: Make this private once all code uses GetScene()
	Zenith_Scene* m_pxParentScene = nullptr;

private:
	Zenith_EntityID m_xEntityID;
};
