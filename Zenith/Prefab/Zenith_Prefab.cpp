#include "Zenith.h"
#include "Prefab/Zenith_Prefab.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_TextComponent.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"

Zenith_Prefab::Zenith_Prefab(Zenith_Prefab&& other) noexcept
	: m_strName(std::move(other.m_strName))
	, m_xComponentData(std::move(other.m_xComponentData))
	, m_bIsValid(other.m_bIsValid)
{
	other.m_bIsValid = false;
}

Zenith_Prefab& Zenith_Prefab::operator=(Zenith_Prefab&& other) noexcept
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
	std::vector<std::string> xComponentTypes;

	if (xEntity.HasComponent<Zenith_TransformComponent>())
		xComponentTypes.push_back("TransformComponent");
	if (xEntity.HasComponent<Zenith_ModelComponent>())
		xComponentTypes.push_back("ModelComponent");
	if (xEntity.HasComponent<Zenith_CameraComponent>())
		xComponentTypes.push_back("CameraComponent");
	if (xEntity.HasComponent<Zenith_TextComponent>())
		xComponentTypes.push_back("TextComponent");
	if (xEntity.HasComponent<Zenith_TerrainComponent>())
		xComponentTypes.push_back("TerrainComponent");
	if (xEntity.HasComponent<Zenith_ColliderComponent>())
		xComponentTypes.push_back("ColliderComponent");
	if (xEntity.HasComponent<Zenith_ScriptComponent>())
		xComponentTypes.push_back("ScriptComponent");

	u_int uNumComponents = static_cast<u_int>(xComponentTypes.size());
	m_xComponentData << uNumComponents;

	for (const std::string& strTypeName : xComponentTypes)
	{
		m_xComponentData << strTypeName;

		if (strTypeName == "TransformComponent")
		{
			xEntity.GetComponent<Zenith_TransformComponent>().WriteToDataStream(m_xComponentData);
		}
		else if (strTypeName == "ModelComponent")
		{
			xEntity.GetComponent<Zenith_ModelComponent>().WriteToDataStream(m_xComponentData);
		}
		else if (strTypeName == "CameraComponent")
		{
			xEntity.GetComponent<Zenith_CameraComponent>().WriteToDataStream(m_xComponentData);
		}
		else if (strTypeName == "TextComponent")
		{
			xEntity.GetComponent<Zenith_TextComponent>().WriteToDataStream(m_xComponentData);
		}
		else if (strTypeName == "TerrainComponent")
		{
			xEntity.GetComponent<Zenith_TerrainComponent>().WriteToDataStream(m_xComponentData);
		}
		else if (strTypeName == "ColliderComponent")
		{
			xEntity.GetComponent<Zenith_ColliderComponent>().WriteToDataStream(m_xComponentData);
		}
		else if (strTypeName == "ScriptComponent")
		{
			xEntity.GetComponent<Zenith_ScriptComponent>().WriteToDataStream(m_xComponentData);
		}
	}
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
	Zenith_DataStream xReadStream;
	xReadStream.ReadFromFile("");

	Zenith_DataStream& xStream = const_cast<Zenith_DataStream&>(m_xComponentData);
	xStream.SetCursor(0);

	u_int uMagic, uVersion;
	std::string strName;
	xStream >> uMagic;
	xStream >> uVersion;
	xStream >> strName;

	u_int uNumComponents;
	xStream >> uNumComponents;

	for (u_int c = 0; c < uNumComponents; c++)
	{
		std::string strComponentType;
		xStream >> strComponentType;

		if (strComponentType == "TransformComponent")
		{
			if (xEntity.HasComponent<Zenith_TransformComponent>())
			{
				xEntity.GetComponent<Zenith_TransformComponent>().ReadFromDataStream(xStream);
			}
		}
		else if (strComponentType == "ModelComponent")
		{
			if (!xEntity.HasComponent<Zenith_ModelComponent>())
			{
				xEntity.AddComponent<Zenith_ModelComponent>();
			}
			xEntity.GetComponent<Zenith_ModelComponent>().ReadFromDataStream(xStream);
		}
		else if (strComponentType == "CameraComponent")
		{
			if (!xEntity.HasComponent<Zenith_CameraComponent>())
			{
				xEntity.AddComponent<Zenith_CameraComponent>();
			}
			xEntity.GetComponent<Zenith_CameraComponent>().ReadFromDataStream(xStream);
		}
		else if (strComponentType == "TextComponent")
		{
			if (!xEntity.HasComponent<Zenith_TextComponent>())
			{
				xEntity.AddComponent<Zenith_TextComponent>();
			}
			xEntity.GetComponent<Zenith_TextComponent>().ReadFromDataStream(xStream);
		}
		else if (strComponentType == "TerrainComponent")
		{
			if (!xEntity.HasComponent<Zenith_TerrainComponent>())
			{
				xEntity.AddComponent<Zenith_TerrainComponent>();
			}
			xEntity.GetComponent<Zenith_TerrainComponent>().ReadFromDataStream(xStream);
		}
		else if (strComponentType == "ColliderComponent")
		{
			if (!xEntity.HasComponent<Zenith_ColliderComponent>())
			{
				xEntity.AddComponent<Zenith_ColliderComponent>();
			}
			xEntity.GetComponent<Zenith_ColliderComponent>().ReadFromDataStream(xStream);
		}
		else if (strComponentType == "ScriptComponent")
		{
			if (!xEntity.HasComponent<Zenith_ScriptComponent>())
			{
				xEntity.AddComponent<Zenith_ScriptComponent>();
			}
			xEntity.GetComponent<Zenith_ScriptComponent>().ReadFromDataStream(xStream);
		}
	}
}
