#pragma once

#include "ZenithECS/Zenith_Entity.h"
#include "ZenithECS/Zenith_Scene.h"

class Zenith_DataStream;

enum ZM_SPAWN_POINT_LOOKUP_RESULT : u_int
{
	ZM_SPAWN_POINT_LOOKUP_FOUND,
	ZM_SPAWN_POINT_LOOKUP_MISSING,
	ZM_SPAWN_POINT_LOOKUP_DUPLICATE,
	ZM_SPAWN_POINT_LOOKUP_INVALID_TAG,
	ZM_SPAWN_POINT_LOOKUP_INVALID_SCENE
};

class ZM_SpawnPoint
{
public:
	static constexpr u_int uSERIALIZATION_VERSION = 1u;
	static constexpr u_int uTAG_CAPACITY = 32u;

	ZM_SpawnPoint() = delete;
	explicit ZM_SpawnPoint(Zenith_Entity& xParentEntity);

	ZM_SpawnPoint(const ZM_SpawnPoint&) = delete;
	ZM_SpawnPoint& operator=(const ZM_SpawnPoint&) = delete;
	ZM_SpawnPoint(ZM_SpawnPoint&&) noexcept = default;
	ZM_SpawnPoint& operator=(ZM_SpawnPoint&&) noexcept = default;

	void OnStart();

	bool SetTag(const char* szTag);
	const char* GetTag() const { return m_szTag; }

	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel();
#endif

	static bool IsTagValid(const char* szTag);
	static ZM_SPAWN_POINT_LOOKUP_RESULT FindUniqueInScene(
		Zenith_Scene xScene,
		const char* szTag,
		Zenith_EntityID& xEntityIDOut);

private:
	void ClearTag();

	Zenith_Entity m_xParentEntity;
	char m_szTag[uTAG_CAPACITY] = {};
};
