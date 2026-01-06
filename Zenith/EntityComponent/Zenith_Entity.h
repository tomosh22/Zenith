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

class Zenith_Entity
{
public:
	Zenith_Entity() = default;
	Zenith_Entity(Zenith_Scene* pxScene, const std::string& strName);
	Zenith_Entity(Zenith_Scene* pxScene, Zenith_EntityID uID, const std::string& strName);
	void Initialise(Zenith_Scene* pxScene, const std::string& strName);
	void Initialise(Zenith_Scene* pxScene, Zenith_EntityID uID, const std::string& strName);

	template<typename T, typename... Args>
	T& AddComponent(Args&&... args);

	template<typename T, typename... Args>
	T& AddOrReplaceComponent(Args&&... args);

	template<typename T>
	bool HasComponent() const;

	template<typename T>
	T& GetComponent() const;

	template<typename T>
	void RemoveComponent();

	Zenith_EntityID GetEntityID() const { return m_xEntityID; }
	class Zenith_Scene* m_pxParentScene;

	// Serialization methods for Zenith_DataStream
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

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

	// Name accessors
	const std::string& GetName() const { return m_strName; }
	void SetName(const std::string& strName);

	//--------------------------------------------------------------------------
	// Enabled State (Unity-like SetActive)
	//--------------------------------------------------------------------------

	/**
	 * Check if this entity is enabled. Disabled entities skip Update/FixedUpdate.
	 */
	bool IsEnabled() const { return m_bEnabled; }

	/**
	 * Enable or disable this entity. Calls OnEnable/OnDisable on all components.
	 * Disabled entities do not receive Update, FixedUpdate, or LateUpdate calls.
	 */
	void SetEnabled(bool bEnabled);

	//--------------------------------------------------------------------------
	// Transient Entity (not saved to scene)
	//--------------------------------------------------------------------------

	/**
	 * Mark entity as transient - it will NOT be saved when scene is serialized.
	 * Use this for procedurally generated entities at runtime.
	 * Runtime-instantiated objects that aren't saved to scene.
	 */
	void SetTransient(bool bTransient);

	/**
	 * Check if this entity is transient (not serialized)
	 */
	bool IsTransient() const { return m_bTransient; }

private:
	Zenith_EntityID m_xEntityID;
	std::string m_strName;
	bool m_bEnabled = true;    // Default: enabled. Disabled entities skip updates.
	bool m_bTransient = true;  // Default: transient (not saved). Scene loading and editor set this to false for persistent entities.
};
