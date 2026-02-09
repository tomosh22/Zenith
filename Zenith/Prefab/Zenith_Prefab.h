#pragma once

#include "DataStream/Zenith_DataStream.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "AssetHandling/Zenith_Asset.h"
#include "AssetHandling/Zenith_AssetHandle.h"
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

class Zenith_Prefab : public Zenith_Asset
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
	bool CreateAsVariant(const PrefabHandle& xBasePrefab, const std::string& strVariantName);

	//--------------------------------------------------------------------------
	// Persistence
	//--------------------------------------------------------------------------

	bool SaveToFile(const std::string& strFilePath) const;

	//--------------------------------------------------------------------------
	// Instantiation
	//--------------------------------------------------------------------------

	Zenith_Entity Instantiate(Zenith_SceneData* pxSceneData, const std::string& strEntityName = "") const;
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
	const PrefabHandle& GetBasePrefab() const { return m_xBasePrefab; }

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
	// Accessors
	//--------------------------------------------------------------------------

	const std::string& GetName() const { return m_strName; }
	bool IsValid() const { return m_bIsValid; }

private:
	friend class Zenith_AssetRegistry;
	friend Zenith_Asset* LoadPrefabAsset(const std::string&);

	/**
	 * Load prefab from file (private - use Zenith_AssetRegistry::Get)
	 */
	bool LoadFromFile(const std::string& strFilePath);

	std::string m_strName;
	Zenith_DataStream m_xComponentData;
	bool m_bIsValid = false;

	// Variant support
	PrefabHandle m_xBasePrefab;
	Zenith_Vector<Zenith_PropertyOverride> m_xOverrides;

	// Prefab format version (bumped to 3 to break compatibility with GUID-based format)
	static constexpr u_int PREFAB_VERSION = 3;
	static constexpr u_int PREFAB_MAGIC = 0x5A505242; // "ZPRB"

	void SerializeComponents(Zenith_Entity& xEntity);
	void DeserializeComponents(Zenith_Entity& xEntity) const;
};
