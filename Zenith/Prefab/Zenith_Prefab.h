#pragma once

#include "DataStream/Zenith_DataStream.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Zenith_Scene.h"
#include <string>

class Zenith_Prefab
{
public:
	Zenith_Prefab() = default;
	~Zenith_Prefab() = default;

	Zenith_Prefab(const Zenith_Prefab&) = delete;
	Zenith_Prefab& operator=(const Zenith_Prefab&) = delete;

	Zenith_Prefab(Zenith_Prefab&& other) noexcept;
	Zenith_Prefab& operator=(Zenith_Prefab&& other) noexcept;

	bool CreateFromEntity(const Zenith_Entity& xEntity, const std::string& strPrefabName);

	bool SaveToFile(const std::string& strFilePath) const;
	bool LoadFromFile(const std::string& strFilePath);

	Zenith_Entity Instantiate(Zenith_Scene* pxScene, const std::string& strEntityName = "") const;
	bool ApplyToEntity(Zenith_Entity& xEntity) const;

	const std::string& GetName() const { return m_strName; }
	bool IsValid() const { return m_bIsValid; }

private:
	std::string m_strName;
	Zenith_DataStream m_xComponentData;
	bool m_bIsValid = false;

	static constexpr u_int PREFAB_VERSION = 1;
	static constexpr u_int PREFAB_MAGIC = 0x5A505242; // "ZPRB"

	void SerializeComponents(Zenith_Entity& xEntity);
	void DeserializeComponents(Zenith_Entity& xEntity) const;
};
