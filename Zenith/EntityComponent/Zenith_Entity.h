#pragma once

#include "Collections/Zenith_Vector.h"

class Zenith_Scene;
class Zenith_DataStream;
using Zenith_EntityID = unsigned int;

class Zenith_Entity
{
public:
	Zenith_Entity() = default;
	Zenith_Entity(Zenith_Scene* pxScene, const std::string& strName);
	Zenith_Entity(Zenith_Scene* pxScene, Zenith_EntityID xGUID, Zenith_EntityID uParentID, const std::string& strName);
	void Initialise(Zenith_Scene* pxScene, const std::string& strName);
	void Initialise(Zenith_Scene* pxScene, Zenith_EntityID xGUID, Zenith_EntityID uParentID, const std::string& strName);

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

	Zenith_EntityID GetEntityID() { return m_uEntityID; }
	class Zenith_Scene* m_pxParentScene;

	// Serialization methods for Zenith_DataStream
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

	//--------------------------------------------------------------------------
	// Parent/Child Hierarchy
	//--------------------------------------------------------------------------

	Zenith_EntityID m_uParentEntityID = static_cast<Zenith_EntityID>(-1);

	/**
	 * Get the parent entity ID
	 * @return Parent entity ID, or -1 if no parent (root entity)
	 */
	Zenith_EntityID GetParentEntityID() const { return m_uParentEntityID; }

	/**
	 * Check if this entity has a parent
	 */
	bool HasParent() const { return m_uParentEntityID != static_cast<Zenith_EntityID>(-1); }

	/**
	 * Set the parent entity
	 * Updates both this entity's parent and the parent's child list
	 * @param uParentID Parent entity ID, or -1 to unparent
	 */
	void SetParent(Zenith_EntityID uParentID);

	/**
	 * Add a child entity to this entity's child list
	 * Called automatically by SetParent - usually don't call directly
	 * @param uChildID Child entity ID to add
	 */
	void AddChild(Zenith_EntityID uChildID);

	/**
	 * Remove a child entity from this entity's child list
	 * Called automatically by SetParent - usually don't call directly
	 * @param uChildID Child entity ID to remove
	 */
	void RemoveChild(Zenith_EntityID uChildID);

	/**
	 * Get all child entity IDs
	 */
	const Zenith_Vector<Zenith_EntityID>& GetChildren() const { return m_xChildEntityIDs; }

	/**
	 * Check if this entity has any children
	 */
	bool HasChildren() const { return m_xChildEntityIDs.GetSize() > 0; }

	/**
	 * Get the number of children
	 */
	uint32_t GetChildCount() const { return static_cast<uint32_t>(m_xChildEntityIDs.GetSize()); }

	/**
	 * Check if this is a root entity (no parent)
	 */
	bool IsRoot() const { return !HasParent(); }

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
	void SetTransient(bool bTransient) { m_bTransient = bTransient; }

	/**
	 * Check if this entity is transient (not serialized)
	 */
	bool IsTransient() const { return m_bTransient; }

private:
	Zenith_EntityID m_uEntityID;
	Zenith_Vector<Zenith_EntityID> m_xChildEntityIDs;
	std::string m_strName;
	bool m_bEnabled = true;    // Default: enabled. Disabled entities skip updates.
	bool m_bTransient = true;  // Default: transient (not saved). Scene loading and editor set this to false for persistent entities.
};
