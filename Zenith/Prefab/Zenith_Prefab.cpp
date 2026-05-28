#include "Zenith.h"
#include "Prefab/Zenith_Prefab.h"
#include "EntityComponent/Zenith_ComponentMeta.h"
#include "EntityComponent/Zenith_SceneSystem.h"
namespace
{
	// Resolve a PrefabHandle to its concrete Zenith_Prefab*, handling both
	// procedural (cached) and file-based (registry-loaded) cases. Returns null
	// if the handle is unset or fails to load.
	// Triggers a registry load on miss — DO NOT call from cycle-detection
	// hot paths that should be side-effect-free.
	Zenith_Prefab* ResolvePrefabHandle(const PrefabHandle& xHandle)
	{
		if (xHandle.GetDirect() != nullptr) return xHandle.GetDirect();
		if (xHandle.IsSet())
		{
			return Zenith_AssetRegistry::Get<Zenith_Prefab>(xHandle.GetPath());
		}
		return nullptr;
	}

	// Side-effect-free variant: returns the prefab only if it's already cached
	// on the handle or already loaded in the registry. Never triggers a load.
	// Used by WouldFormVariantCycle so checking for cycles never has the side
	// effect of pulling assets off disk (which would assert on missing files).
	Zenith_Prefab* TryGetLoadedPrefab(const PrefabHandle& xHandle)
	{
		if (xHandle.GetDirect() != nullptr) return xHandle.GetDirect();
		if (xHandle.IsSet() && Zenith_AssetRegistry::IsLoaded(xHandle.GetPath()))
		{
			return Zenith_AssetRegistry::Get<Zenith_Prefab>(xHandle.GetPath());
		}
		return nullptr;
	}
}

//=============================================================================
// PropertyOverride Implementation
//=============================================================================

void Zenith_PropertyOverride::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << m_strComponentName;
	xStream << m_strPropertyPath;

	// Write the value stream size and data
	u_int uSize = static_cast<u_int>(m_xValue.GetSize());
	xStream << uSize;
	if (uSize > 0)
	{
		xStream.Write(m_xValue.GetData(), uSize);
	}
}

void Zenith_PropertyOverride::ReadFromDataStream(Zenith_DataStream& xStream)
{
	xStream >> m_strComponentName;
	xStream >> m_strPropertyPath;

	u_int uSize;
	xStream >> uSize;
	if (uSize > 0)
	{
		m_xValue = Zenith_DataStream();
		uint8_t* pBuffer = static_cast<uint8_t*>(Zenith_MemoryManagement::Allocate(uSize));
		xStream.Read(pBuffer, uSize);
		m_xValue.Write(pBuffer, uSize);
		m_xValue.SetCursor(0);
		Zenith_MemoryManagement::Deallocate(pBuffer);
	}
}

//=============================================================================
// Zenith_Prefab Implementation
//=============================================================================

Zenith_Prefab::Zenith_Prefab(Zenith_Prefab&& other)
	: m_strName(std::move(other.m_strName))
	, m_xComponentData(std::move(other.m_xComponentData))
	, m_bIsValid(other.m_bIsValid)
	, m_xBasePrefab(std::move(other.m_xBasePrefab))
	, m_xOverrides(std::move(other.m_xOverrides))
{
	other.m_bIsValid = false;
}

Zenith_Prefab& Zenith_Prefab::operator=(Zenith_Prefab&& other)
{
	if (this != &other)
	{
		m_strName = std::move(other.m_strName);
		m_xComponentData = std::move(other.m_xComponentData);
		m_bIsValid = other.m_bIsValid;
		m_xBasePrefab = std::move(other.m_xBasePrefab);
		m_xOverrides = std::move(other.m_xOverrides);
		other.m_bIsValid = false;
	}
	return *this;
}

bool Zenith_Prefab::CreateFromEntity(const Zenith_Entity& xEntity, const std::string& strPrefabName)
{
	m_strName = strPrefabName;
	m_bIsValid = false;
	m_xOverrides.Clear();
	m_xBasePrefab.Clear();

	m_xComponentData = Zenith_DataStream();

	m_xComponentData << PREFAB_MAGIC;
	m_xComponentData << PREFAB_VERSION;
	m_xComponentData << m_strName;

	Zenith_Entity& xMutableEntity = const_cast<Zenith_Entity&>(xEntity);
	SerializeComponents(xMutableEntity);

	m_bIsValid = true;
	return true;
}

bool Zenith_Prefab::CreateAsVariant(const PrefabHandle& xBasePrefab, const std::string& strVariantName)
{
	if (!xBasePrefab.IsSet())
	{
		Zenith_Error(LOG_CATEGORY_PREFAB, "Cannot create variant without base prefab");
		return false;
	}

	if (WouldFormVariantCycle(xBasePrefab))
	{
		Zenith_Error(LOG_CATEGORY_PREFAB,
			"Cannot create variant '%s': base prefab would form a cycle (variant chain reaches back to this prefab).",
			strVariantName.c_str());
		return false;
	}

	m_strName = strVariantName;
	m_xBasePrefab = xBasePrefab;
	m_xOverrides.Clear();
	m_xComponentData = Zenith_DataStream();

	// Variants don't store component data - they inherit from base
	m_bIsValid = true;
	return true;
}

bool Zenith_Prefab::WouldFormVariantCycle(const PrefabHandle& xProposedBase) const
{
	// Walk the proposed base's ancestor chain using ONLY the cached pointer
	// (GetDirect) — never trigger a registry load here. Cycle detection at
	// CreateAsVariant time should not have a side effect of loading prefabs
	// off disk, and an unloaded ancestor cannot be the start of a cycle that
	// reaches `this` without being loaded first. The runtime path in
	// InstantiateInternal has its own visited-set guard that catches anything
	// that slips through (e.g. a corrupted on-disk variant chain).
	constexpr u_int uMAX_VARIANT_CHAIN_DEPTH = 64;

	PrefabHandle xCursor = xProposedBase;
	for (u_int u = 0; u < uMAX_VARIANT_CHAIN_DEPTH; ++u)
	{
		Zenith_Prefab* pxAncestor = TryGetLoadedPrefab(xCursor);
		if (pxAncestor == nullptr) return false;
		if (pxAncestor == this) return true;
		xCursor = pxAncestor->m_xBasePrefab;
	}
	Zenith_Warning(LOG_CATEGORY_PREFAB,
		"Variant chain exceeded depth %u during cycle check — assuming cycle.", uMAX_VARIANT_CHAIN_DEPTH);
	return true;
}

void Zenith_Prefab::SerializeComponents(Zenith_Entity& xEntity)
{
	// Use the ComponentMeta registry to serialize all components
	Zenith_ComponentMetaRegistry::Get().SerializeEntityComponents(xEntity, m_xComponentData);
}

bool Zenith_Prefab::SaveToFile(const std::string& strFilePath) const
{
	if (!m_bIsValid)
	{
		Zenith_Error(LOG_CATEGORY_PREFAB, "Cannot save invalid prefab");
		return false;
	}

	Zenith_DataStream xOutput;

	// Write header
	xOutput << PREFAB_MAGIC;
	xOutput << PREFAB_VERSION;
	xOutput << m_strName;

	// Write base prefab reference (for variants)
	bool bIsVariant = m_xBasePrefab.IsSet();
	xOutput << bIsVariant;
	if (bIsVariant)
	{
		m_xBasePrefab.WriteToDataStream(xOutput);
	}

	// Write overrides
	u_int uNumOverrides = m_xOverrides.GetSize();
	xOutput << uNumOverrides;
	for (u_int i = 0; i < uNumOverrides; i++)
	{
		m_xOverrides.Get(i).WriteToDataStream(xOutput);
	}

	// Write component data (only for non-variants)
	if (!bIsVariant)
	{
		u_int uDataSize = static_cast<u_int>(m_xComponentData.GetSize());
		xOutput << uDataSize;
		if (uDataSize > 0)
		{
			xOutput.Write(m_xComponentData.GetData(), uDataSize);
		}
	}

	xOutput.WriteToFile(strFilePath.c_str());
	Zenith_Log(LOG_CATEGORY_PREFAB, "Saved prefab '%s' to %s (variant: %s)",
		m_strName.c_str(), strFilePath.c_str(), bIsVariant ? "yes" : "no");
	return true;
}

bool Zenith_Prefab::LoadFromFile(const std::string& strFilePath)
{
	m_bIsValid = false;
	m_xOverrides.Clear();
	m_xBasePrefab.Clear();

	Zenith_DataStream xInput;
	xInput.ReadFromFile(strFilePath.c_str());

	u_int uMagic;
	u_int uVersion;
	xInput >> uMagic;
	xInput >> uVersion;

	if (uMagic != PREFAB_MAGIC)
	{
		Zenith_Error(LOG_CATEGORY_PREFAB, "Invalid prefab file format: %s", strFilePath.c_str());
		return false;
	}

	if (uVersion != PREFAB_VERSION)
	{
		Zenith_Error(LOG_CATEGORY_PREFAB, "Unsupported prefab version %u (expected %u). Please re-export the prefab: %s", uVersion, PREFAB_VERSION, strFilePath.c_str());
		return false;
	}

	xInput >> m_strName;

	// Read base prefab reference
	bool bIsVariant;
	xInput >> bIsVariant;
	if (bIsVariant)
	{
		m_xBasePrefab.ReadFromDataStream(xInput);
	}

	// Read overrides
	u_int uNumOverrides;
	xInput >> uNumOverrides;
	for (u_int i = 0; i < uNumOverrides; i++)
	{
		Zenith_PropertyOverride xOverride;
		xOverride.ReadFromDataStream(xInput);
		m_xOverrides.PushBack(std::move(xOverride));
	}

	// Read component data (only for non-variants)
	if (!bIsVariant)
	{
		u_int uDataSize;
		xInput >> uDataSize;
		if (uDataSize > 0)
		{
			m_xComponentData = Zenith_DataStream();
			uint8_t* pBuffer = static_cast<uint8_t*>(Zenith_MemoryManagement::Allocate(uDataSize));
			xInput.Read(pBuffer, uDataSize);
			m_xComponentData.Write(pBuffer, uDataSize);
			m_xComponentData.SetCursor(0);
			Zenith_MemoryManagement::Deallocate(pBuffer);
		}
	}

	m_bIsValid = true;
	Zenith_Log(LOG_CATEGORY_PREFAB, "Loaded prefab '%s' from %s", m_strName.c_str(), strFilePath.c_str());
	return true;
}

Zenith_Entity Zenith_Prefab::Instantiate(Zenith_SceneData* pxSceneData, const std::string& strEntityName) const
{
	if (!m_bIsValid || !pxSceneData)
	{
		Zenith_Error(LOG_CATEGORY_PREFAB, "Cannot instantiate invalid prefab or null scene");
		return Zenith_Entity();
	}

	// Suppress immediate lifecycle dispatch in Entity constructor across the entire
	// recursive chain. PrefabInstantiationGuard restores the prior value when it
	// goes out of scope; the manual dispatch below fires once after recursion completes.
	Zenith_Entity xEntity;
	{
		Zenith_PrefabInstantiationGuard xPrefabGuard;
		Zenith_Vector<const Zenith_Prefab*> axVisited;
		xEntity = InstantiateInternal(pxSceneData, strEntityName, axVisited);
	}

	if (!xEntity.IsValid())
	{
		return xEntity;  // Inner call already logged the failure reason.
	}

	// Dispatch lifecycle hooks with all components present (Unity-style: per-entity,
	// immediately after creation). Done once at the top level, not per recursion step.
	Zenith_ComponentMetaRegistry& xRegistry = Zenith_ComponentMetaRegistry::Get();
	xRegistry.DispatchOnAwake(xEntity);
	if (xEntity.IsEnabled())
	{
		xRegistry.DispatchOnEnable(xEntity);
	}

	// Mark as awoken so Update() doesn't dispatch again
	pxSceneData->MarkEntityAwoken(xEntity.GetEntityID());

	return xEntity;
}

Zenith_Entity Zenith_Prefab::InstantiateInternal(
	Zenith_SceneData* pxSceneData,
	const std::string& strEntityName,
	Zenith_Vector<const Zenith_Prefab*>& axVisited) const
{
	// Cycle guard. CreateAsVariant rejects cycles at construction time, but a
	// hand-crafted or corrupted .zprfb on disk could still load with one.
	for (u_int u = 0; u < axVisited.GetSize(); ++u)
	{
		if (axVisited.Get(u) == this)
		{
			Zenith_Error(LOG_CATEGORY_PREFAB,
				"Variant cycle detected while instantiating '%s'. Aborting.", m_strName.c_str());
			return Zenith_Entity();
		}
	}
	axVisited.PushBack(this);

	if (m_xBasePrefab.IsSet())
	{
		// Variant path: instantiate the base, then apply our overrides on top.
		Zenith_Prefab* pxBase = ResolvePrefabHandle(m_xBasePrefab);
		if (pxBase == nullptr)
		{
			Zenith_Error(LOG_CATEGORY_PREFAB,
				"Variant '%s' references a base prefab that could not be loaded.",
				m_strName.c_str());
			return Zenith_Entity();
		}

		Zenith_Entity xEntity = pxBase->InstantiateInternal(pxSceneData, strEntityName, axVisited);
		if (!xEntity.IsValid())
		{
			return xEntity;
		}

		// Apply overrides on top of the base entity. Skip nested paths — flat
		// names only in this iteration (see Zenith_ComponentMeta.h header docs).
		Zenith_ComponentMetaRegistry& xRegistry = Zenith_ComponentMetaRegistry::Get();
		for (u_int u = 0; u < m_xOverrides.GetSize(); ++u)
		{
			const Zenith_PropertyOverride& xOv = m_xOverrides.Get(u);
			if (xOv.m_strPropertyPath.find('.') != std::string::npos)
			{
				Zenith_Warning(LOG_CATEGORY_PREFAB,
					"Variant '%s': override path '%s' uses nested fields, which are not yet supported. Skipping.",
					m_strName.c_str(), xOv.m_strPropertyPath.c_str());
				continue;
			}
			xRegistry.SetComponentProperty(
				xEntity,
				xOv.m_strComponentName,
				xOv.m_strPropertyPath,
				const_cast<Zenith_DataStream&>(xOv.m_xValue));
		}
		return xEntity;
	}

	// Non-variant path: create entity from this prefab's component data.
	std::string strName = strEntityName.empty() ? m_strName : strEntityName;
	Zenith_Entity xEntity(pxSceneData, strName);
	DeserializeComponents(xEntity);
	return xEntity;
}

Zenith_Entity Zenith_Prefab::Instantiate(const std::string& strEntityName) const
{
	const Zenith_Scene xTarget = g_xEngine.Scenes().GetDefaultCreationScene();
	if (!xTarget.IsValid())
	{
		Zenith_Error(LOG_CATEGORY_PREFAB,
			"Prefab::Instantiate('%s'): no creation target available "
			"(no active scene and no SceneCreationTargetScope)",
			m_strName.c_str());
		return Zenith_Entity();
	}

	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTarget);
	if (!pxSceneData)
	{
		Zenith_Error(LOG_CATEGORY_PREFAB,
			"Prefab::Instantiate('%s'): default creation scene (handle=%d gen=%u) is not loaded",
			m_strName.c_str(), xTarget.m_iHandle, xTarget.m_uGeneration);
		return Zenith_Entity();
	}

	return Instantiate(pxSceneData, strEntityName);
}

bool Zenith_Prefab::ApplyToEntity(Zenith_Entity& xEntity) const
{
	if (!m_bIsValid)
	{
		Zenith_Error(LOG_CATEGORY_PREFAB, "Cannot apply invalid prefab");
		return false;
	}

	// For variants, apply the base prefab first then layer our overrides on top.
	// Cycle protection is bounded by chain depth (see WouldFormVariantCycle).
	if (m_xBasePrefab.IsSet())
	{
		Zenith_Prefab* pxBase = ResolvePrefabHandle(m_xBasePrefab);
		if (pxBase == nullptr)
		{
			Zenith_Error(LOG_CATEGORY_PREFAB,
				"Variant '%s' references a base prefab that could not be loaded.", m_strName.c_str());
			return false;
		}
		if (!pxBase->ApplyToEntity(xEntity))
		{
			return false;
		}

		Zenith_ComponentMetaRegistry& xRegistry = Zenith_ComponentMetaRegistry::Get();
		for (u_int u = 0; u < m_xOverrides.GetSize(); ++u)
		{
			const Zenith_PropertyOverride& xOv = m_xOverrides.Get(u);
			if (xOv.m_strPropertyPath.find('.') != std::string::npos)
			{
				Zenith_Warning(LOG_CATEGORY_PREFAB,
					"Variant '%s': override path '%s' uses nested fields, which are not yet supported. Skipping.",
					m_strName.c_str(), xOv.m_strPropertyPath.c_str());
				continue;
			}
			xRegistry.SetComponentProperty(
				xEntity,
				xOv.m_strComponentName,
				xOv.m_strPropertyPath,
				const_cast<Zenith_DataStream&>(xOv.m_xValue));
		}
		return true;
	}

	DeserializeComponents(xEntity);
	return true;
}

void Zenith_Prefab::DeserializeComponents(Zenith_Entity& xEntity) const
{
	Zenith_DataStream& xStream = const_cast<Zenith_DataStream&>(m_xComponentData);
	xStream.SetCursor(0);

	// Consume the header (magic, version, name) to advance the stream cursor
	// past it before the component reader runs. The values aren't validated
	// here — SaveToFile/LoadFromFile own the integrity checks.
	u_int uMagic, uVersion;
	std::string strName;
	xStream >> uMagic;
	xStream >> uVersion;
	xStream >> strName;

	// Use the ComponentMeta registry to deserialize all components
	Zenith_ComponentMetaRegistry::Get().DeserializeEntityComponents(xEntity, xStream);
}

void Zenith_Prefab::AddOverride(Zenith_PropertyOverride xOverride)
{
	// Check if we already have an override for this component/property
	for (u_int i = 0; i < m_xOverrides.GetSize(); i++)
	{
		Zenith_PropertyOverride& xExisting = m_xOverrides.Get(i);
		if (xExisting.m_strComponentName == xOverride.m_strComponentName &&
			xExisting.m_strPropertyPath == xOverride.m_strPropertyPath)
		{
			// Replace existing override by moving data
			xExisting.m_xValue = std::move(xOverride.m_xValue);
			return;
		}
	}

	// Add new override
	m_xOverrides.PushBack(std::move(xOverride));
}
