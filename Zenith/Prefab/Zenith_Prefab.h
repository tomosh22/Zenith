#pragma once

#include "DataStream/Zenith_DataStream.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "AssetHandling/Zenith_Asset.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "Maths/Zenith_Maths.h"
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

	/**
	 * Instantiate the prefab into a scene at the given transform. Unity-style:
	 * the transform is applied to the entity BEFORE its lifecycle (OnAwake/
	 * OnEnable) runs, so a baked collider's physics body is built at the
	 * instance transform. Position/rotation/scale default to origin / identity
	 * / (1,1,1). For variants, per-property overrides apply on top of this
	 * transform (e.g. a Scale override replaces the scale; an unset property
	 * keeps the value passed here).
	 */
	Zenith_Entity Instantiate(Zenith_SceneData* pxSceneData,
		const std::string&           strEntityName = "",
		const Zenith_Maths::Vector3& xPosition     = Zenith_Maths::Vector3(0.0f),
		const Zenith_Maths::Quat&    xRotation     = Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f),
		const Zenith_Maths::Vector3& xScale        = Zenith_Maths::Vector3(1.0f)) const;

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

	// Recursive helper for Instantiate. Walks the base-prefab chain, applies the
	// caller transform at the non-variant leaf and overrides on top, and tracks
	// visited prefabs to abort on cycles. Lifecycle dispatch (OnAwake / OnEnable)
	// is performed by the public Instantiate() once after the recursion unwinds —
	// never by this helper.
	Zenith_Entity InstantiateInternal(
		Zenith_SceneData* pxSceneData,
		const std::string& strEntityName,
		const Zenith_Maths::Vector3& xPosition,
		const Zenith_Maths::Quat& xRotation,
		const Zenith_Maths::Vector3& xScale,
		Zenith_Vector<const Zenith_Prefab*>& axVisited) const;

	// Returns true if the proposed base prefab would form a cycle when set
	// as this prefab's base (i.e. the proposed base reaches `this` through
	// its own ancestor chain, or IS `this`).
	bool WouldFormVariantCycle(const PrefabHandle& xProposedBase) const;
};
