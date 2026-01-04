#pragma once

#include "DataStream/Zenith_DataStream.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Zenith_Scene.h"
#include "Core/Zenith_GUID.h"
#include "AssetHandling/Zenith_AssetRef.h"
#include "Collections/Zenith_Vector.h"
#include <string>

/**
 * PropertyOverride - Tracks a property that differs from the base prefab
 * Used by prefab variants and prefab instances to track local changes
 */
struct Zenith_PropertyOverride
{
	std::string m_strComponentName;   // Which component (e.g., "Transform", "Model")
	std::string m_strPropertyPath;    // Path to property (e.g., "Position.x", "Scale")
	Zenith_DataStream m_xValue;       // Serialized override value

	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);
};

/**
 * NestedPrefabInstance - A prefab contained within another prefab
 */
struct Zenith_NestedPrefabInstance
{
	PrefabRef m_xPrefab;                                    // Reference to nested prefab
	std::string m_strLocalName;                             // Name within parent prefab
	Zenith_Vector<Zenith_PropertyOverride> m_xOverrides;    // Local overrides

	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);
};

class Zenith_Prefab
{
public:
	Zenith_Prefab() = default;
	~Zenith_Prefab() = default;

	Zenith_Prefab(const Zenith_Prefab&) = delete;
	Zenith_Prefab& operator=(const Zenith_Prefab&) = delete;

	Zenith_Prefab(Zenith_Prefab&& other);
	Zenith_Prefab& operator=(Zenith_Prefab&& other);

	//--------------------------------------------------------------------------
	// Creation
	//--------------------------------------------------------------------------

	bool CreateFromEntity(const Zenith_Entity& xEntity, const std::string& strPrefabName);

	/**
	 * Create a variant of an existing prefab
	 * Variants inherit from a base prefab and only store overrides
	 */
	bool CreateAsVariant(const PrefabRef& xBasePrefab, const std::string& strVariantName);

	//--------------------------------------------------------------------------
	// Persistence
	//--------------------------------------------------------------------------

	bool SaveToFile(const std::string& strFilePath) const;
	bool LoadFromFile(const std::string& strFilePath);

	//--------------------------------------------------------------------------
	// Instantiation
	//--------------------------------------------------------------------------

	Zenith_Entity Instantiate(Zenith_Scene* pxScene, const std::string& strEntityName = "") const;
	bool ApplyToEntity(Zenith_Entity& xEntity) const;

	//--------------------------------------------------------------------------
	// Variant/Override Support
	//--------------------------------------------------------------------------

	/**
	 * Check if this is a variant (has a base prefab)
	 */
	bool IsVariant() const { return m_xBasePrefab.IsSet(); }

	/**
	 * Get the base prefab (for variants)
	 */
	const PrefabRef& GetBasePrefab() const { return m_xBasePrefab; }

	/**
	 * Add a property override (takes ownership via move)
	 */
	void AddOverride(Zenith_PropertyOverride xOverride);

	/**
	 * Get all overrides
	 */
	const Zenith_Vector<Zenith_PropertyOverride>& GetOverrides() const { return m_xOverrides; }

	/**
	 * Clear all overrides (revert to base prefab)
	 */
	void ClearOverrides() { m_xOverrides.Clear(); }

	//--------------------------------------------------------------------------
	// Nested Prefabs
	//--------------------------------------------------------------------------

	/**
	 * Add a nested prefab instance (takes ownership via move)
	 */
	void AddNestedPrefab(Zenith_NestedPrefabInstance xNested);

	/**
	 * Get all nested prefab instances
	 */
	const Zenith_Vector<Zenith_NestedPrefabInstance>& GetNestedPrefabs() const { return m_xNestedPrefabs; }

	//--------------------------------------------------------------------------
	// Accessors
	//--------------------------------------------------------------------------

	const std::string& GetName() const { return m_strName; }
	const Zenith_AssetGUID& GetGUID() const { return m_xGUID; }
	bool IsValid() const { return m_bIsValid; }

	void SetGUID(const Zenith_AssetGUID& xGUID) { m_xGUID = xGUID; }

private:
	std::string m_strName;
	Zenith_AssetGUID m_xGUID;
	Zenith_DataStream m_xComponentData;
	bool m_bIsValid = false;

	// Variant support
	PrefabRef m_xBasePrefab;
	Zenith_Vector<Zenith_PropertyOverride> m_xOverrides;

	// Nested prefab support
	Zenith_Vector<Zenith_NestedPrefabInstance> m_xNestedPrefabs;

	// Prefab format version
	static constexpr u_int PREFAB_VERSION = 2;
	static constexpr u_int PREFAB_MAGIC = 0x5A505242; // "ZPRB"

	void SerializeComponents(Zenith_Entity& xEntity);
	void DeserializeComponents(Zenith_Entity& xEntity) const;
};
