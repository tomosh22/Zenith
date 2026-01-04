#include "Zenith.h"
#include "Prefab/Zenith_Prefab.h"
#include "EntityComponent/Zenith_ComponentMeta.h"

//=============================================================================
// PropertyOverride Implementation
//=============================================================================

void Zenith_PropertyOverride::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << m_strComponentName;
	xStream << m_strPropertyPath;

	// Write the value stream size and data
	u_int uSize = m_xValue.GetSize();
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
// NestedPrefabInstance Implementation
//=============================================================================

void Zenith_NestedPrefabInstance::WriteToDataStream(Zenith_DataStream& xStream) const
{
	m_xPrefab.WriteToDataStream(xStream);
	xStream << m_strLocalName;

	u_int uNumOverrides = m_xOverrides.GetSize();
	xStream << uNumOverrides;
	for (u_int i = 0; i < uNumOverrides; i++)
	{
		m_xOverrides.Get(i).WriteToDataStream(xStream);
	}
}

void Zenith_NestedPrefabInstance::ReadFromDataStream(Zenith_DataStream& xStream)
{
	m_xPrefab.ReadFromDataStream(xStream);
	xStream >> m_strLocalName;

	u_int uNumOverrides;
	xStream >> uNumOverrides;
	m_xOverrides.Clear();
	for (u_int i = 0; i < uNumOverrides; i++)
	{
		Zenith_PropertyOverride xOverride;
		xOverride.ReadFromDataStream(xStream);
		m_xOverrides.PushBack(std::move(xOverride));
	}
}

//=============================================================================
// Zenith_Prefab Implementation
//=============================================================================

Zenith_Prefab::Zenith_Prefab(Zenith_Prefab&& other)
	: m_strName(std::move(other.m_strName))
	, m_xGUID(other.m_xGUID)
	, m_xComponentData(std::move(other.m_xComponentData))
	, m_bIsValid(other.m_bIsValid)
	, m_xBasePrefab(std::move(other.m_xBasePrefab))
	, m_xOverrides(std::move(other.m_xOverrides))
	, m_xNestedPrefabs(std::move(other.m_xNestedPrefabs))
{
	other.m_bIsValid = false;
	other.m_xGUID = Zenith_AssetGUID::INVALID;
}

Zenith_Prefab& Zenith_Prefab::operator=(Zenith_Prefab&& other)
{
	if (this != &other)
	{
		m_strName = std::move(other.m_strName);
		m_xGUID = other.m_xGUID;
		m_xComponentData = std::move(other.m_xComponentData);
		m_bIsValid = other.m_bIsValid;
		m_xBasePrefab = std::move(other.m_xBasePrefab);
		m_xOverrides = std::move(other.m_xOverrides);
		m_xNestedPrefabs = std::move(other.m_xNestedPrefabs);
		other.m_bIsValid = false;
		other.m_xGUID = Zenith_AssetGUID::INVALID;
	}
	return *this;
}

bool Zenith_Prefab::CreateFromEntity(const Zenith_Entity& xEntity, const std::string& strPrefabName)
{
	m_strName = strPrefabName;
	m_xGUID = Zenith_AssetGUID::Generate();
	m_bIsValid = false;
	m_xOverrides.Clear();
	m_xNestedPrefabs.Clear();
	m_xBasePrefab.Clear();

	m_xComponentData = Zenith_DataStream();

	m_xComponentData << PREFAB_MAGIC;
	m_xComponentData << PREFAB_VERSION;
	m_xComponentData << m_strName;
	m_xGUID.WriteToDataStream(m_xComponentData);

	Zenith_Entity& xMutableEntity = const_cast<Zenith_Entity&>(xEntity);
	SerializeComponents(xMutableEntity);

	m_bIsValid = true;
	return true;
}

bool Zenith_Prefab::CreateAsVariant(const PrefabRef& xBasePrefab, const std::string& strVariantName)
{
	if (!xBasePrefab.IsSet())
	{
		Zenith_Error(LOG_CATEGORY_PREFAB, "Cannot create variant without base prefab");
		return false;
	}

	m_strName = strVariantName;
	m_xGUID = Zenith_AssetGUID::Generate();
	m_xBasePrefab = xBasePrefab;
	m_xOverrides.Clear();
	m_xNestedPrefabs.Clear();
	m_xComponentData = Zenith_DataStream();

	// Variants don't store component data - they inherit from base
	m_bIsValid = true;
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
	m_xGUID.WriteToDataStream(xOutput);

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

	// Write nested prefabs
	u_int uNumNested = m_xNestedPrefabs.GetSize();
	xOutput << uNumNested;
	for (u_int i = 0; i < uNumNested; i++)
	{
		m_xNestedPrefabs.Get(i).WriteToDataStream(xOutput);
	}

	// Write component data (only for non-variants)
	if (!bIsVariant)
	{
		u_int uDataSize = m_xComponentData.GetSize();
		xOutput << uDataSize;
		if (uDataSize > 0)
		{
			xOutput.Write(m_xComponentData.GetData(), uDataSize);
		}
	}

	xOutput.WriteToFile(strFilePath.c_str());
	Zenith_Log(LOG_CATEGORY_PREFAB, "Saved prefab '%s' to %s (GUID: %s, variant: %s)",
		m_strName.c_str(), strFilePath.c_str(), m_xGUID.ToString().c_str(),
		bIsVariant ? "yes" : "no");
	return true;
}

bool Zenith_Prefab::LoadFromFile(const std::string& strFilePath)
{
	m_bIsValid = false;
	m_xOverrides.Clear();
	m_xNestedPrefabs.Clear();
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

	// Read GUID
	m_xGUID.ReadFromDataStream(xInput);

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

	// Read nested prefabs
	u_int uNumNested;
	xInput >> uNumNested;
	for (u_int i = 0; i < uNumNested; i++)
	{
		Zenith_NestedPrefabInstance xNested;
		xNested.ReadFromDataStream(xInput);
		m_xNestedPrefabs.PushBack(std::move(xNested));
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
	Zenith_Log(LOG_CATEGORY_PREFAB, "Loaded prefab '%s' from %s (GUID: %s)",
		m_strName.c_str(), strFilePath.c_str(), m_xGUID.ToString().c_str());
	return true;
}

Zenith_Entity Zenith_Prefab::Instantiate(Zenith_Scene* pxScene, const std::string& strEntityName) const
{
	if (!m_bIsValid || !pxScene)
	{
		Zenith_Error(LOG_CATEGORY_PREFAB, "Cannot instantiate invalid prefab or null scene");
		return Zenith_Entity();
	}

	std::string strName = strEntityName.empty() ? m_strName : strEntityName;
	Zenith_Entity xEntity(pxScene, strName);

	DeserializeComponents(xEntity);

	return xEntity;
}

bool Zenith_Prefab::ApplyToEntity(Zenith_Entity& xEntity) const
{
	if (!m_bIsValid)
	{
		Zenith_Error(LOG_CATEGORY_PREFAB, "Cannot apply invalid prefab");
		return false;
	}

	DeserializeComponents(xEntity);

	return true;
}

void Zenith_Prefab::DeserializeComponents(Zenith_Entity& xEntity) const
{
	Zenith_DataStream& xStream = const_cast<Zenith_DataStream&>(m_xComponentData);
	xStream.SetCursor(0);

	// Skip the header (magic, version, name, GUID)
	u_int uMagic, uVersion;
	std::string strName;
	xStream >> uMagic;
	xStream >> uVersion;
	xStream >> strName;

	// Skip the GUID (it was written after the name in CreateFromEntity)
	Zenith_AssetGUID xSkipGUID;
	xSkipGUID.ReadFromDataStream(xStream);

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

void Zenith_Prefab::AddNestedPrefab(Zenith_NestedPrefabInstance xNested)
{
	m_xNestedPrefabs.PushBack(std::move(xNested));
}
