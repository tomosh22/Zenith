#include "Zenith.h"
#include "Prefab/Zenith_Prefab.h"
#include "EntityComponent/Zenith_ComponentMeta.h"

Zenith_Prefab::Zenith_Prefab(Zenith_Prefab&& other)
	: m_strName(std::move(other.m_strName))
	, m_xComponentData(std::move(other.m_xComponentData))
	, m_bIsValid(other.m_bIsValid)
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
		other.m_bIsValid = false;
	}
	return *this;
}

bool Zenith_Prefab::CreateFromEntity(const Zenith_Entity& xEntity, const std::string& strPrefabName)
{
	m_strName = strPrefabName;
	m_bIsValid = false;

	m_xComponentData = Zenith_DataStream();

	m_xComponentData << PREFAB_MAGIC;
	m_xComponentData << PREFAB_VERSION;
	m_xComponentData << m_strName;

	Zenith_Entity& xMutableEntity = const_cast<Zenith_Entity&>(xEntity);
	SerializeComponents(xMutableEntity);

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
		Zenith_Log("[Prefab] Cannot save invalid prefab");
		return false;
	}

	const_cast<Zenith_DataStream&>(m_xComponentData).WriteToFile(strFilePath.c_str());
	Zenith_Log("[Prefab] Saved prefab '%s' to %s", m_strName.c_str(), strFilePath.c_str());
	return true;
}

bool Zenith_Prefab::LoadFromFile(const std::string& strFilePath)
{
	m_bIsValid = false;

	m_xComponentData = Zenith_DataStream();
	m_xComponentData.ReadFromFile(strFilePath.c_str());

	u_int uMagic;
	u_int uVersion;
	m_xComponentData >> uMagic;
	m_xComponentData >> uVersion;

	if (uMagic != PREFAB_MAGIC)
	{
		Zenith_Log("[Prefab] Invalid prefab file format: %s", strFilePath.c_str());
		return false;
	}

	if (uVersion != PREFAB_VERSION)
	{
		Zenith_Log("[Prefab] Unsupported prefab version: %u", uVersion);
		return false;
	}

	m_xComponentData >> m_strName;

	m_bIsValid = true;
	Zenith_Log("[Prefab] Loaded prefab '%s' from %s", m_strName.c_str(), strFilePath.c_str());
	return true;
}

Zenith_Entity Zenith_Prefab::Instantiate(Zenith_Scene* pxScene, const std::string& strEntityName) const
{
	if (!m_bIsValid || !pxScene)
	{
		Zenith_Log("[Prefab] Cannot instantiate invalid prefab or null scene");
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
		Zenith_Log("[Prefab] Cannot apply invalid prefab");
		return false;
	}

	DeserializeComponents(xEntity);

	return true;
}

void Zenith_Prefab::DeserializeComponents(Zenith_Entity& xEntity) const
{
	Zenith_DataStream& xStream = const_cast<Zenith_DataStream&>(m_xComponentData);
	xStream.SetCursor(0);

	// Skip the header (magic, version, name)
	u_int uMagic, uVersion;
	std::string strName;
	xStream >> uMagic;
	xStream >> uVersion;
	xStream >> strName;

	// Use the ComponentMeta registry to deserialize all components
	Zenith_ComponentMetaRegistry::Get().DeserializeEntityComponents(xEntity, xStream);
}
