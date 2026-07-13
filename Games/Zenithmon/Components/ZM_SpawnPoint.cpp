#include "Zenith.h"

#include "Zenithmon/Components/ZM_SpawnPoint.h"

#include "Core/Zenith_Engine.h"
#include "DataStream/Zenith_DataStream.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

#include <cstring>

ZM_SpawnPoint::ZM_SpawnPoint(Zenith_Entity& xParentEntity)
	: m_xParentEntity(xParentEntity)
{
}

void ZM_SpawnPoint::OnStart()
{
	if (!IsTagValid(m_szTag))
	{
		ClearTag();
	}
}

bool ZM_SpawnPoint::SetTag(const char* szTag)
{
	if (!IsTagValid(szTag))
	{
		return false;
	}

	char szValidatedTag[uTAG_CAPACITY] = {};
	const size_t ulLength = std::strlen(szTag);
	std::memcpy(szValidatedTag, szTag, ulLength);
	std::memcpy(m_szTag, szValidatedTag, sizeof(m_szTag));
	return true;
}

void ZM_SpawnPoint::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << uSERIALIZATION_VERSION;
	xStream.WriteData(m_szTag, sizeof(m_szTag));
}

void ZM_SpawnPoint::ReadFromDataStream(Zenith_DataStream& xStream)
{
	u_int uVersion = 0u;
	xStream >> uVersion;
	ClearTag();
	if (uVersion != uSERIALIZATION_VERSION)
	{
		return;
	}

	char szTag[uTAG_CAPACITY] = {};
	xStream.ReadData(szTag, sizeof(szTag));
	SetTag(szTag);
}

#ifdef ZENITH_TOOLS
void ZM_SpawnPoint::RenderPropertiesPanel()
{
	ImGui::Text("Tag: %s", IsTagValid(m_szTag) ? m_szTag : "<invalid>");
}
#endif

bool ZM_SpawnPoint::IsTagValid(const char* szTag)
{
	if (szTag == nullptr)
	{
		return false;
	}

	for (u_int uIndex = 0u; uIndex < uTAG_CAPACITY; ++uIndex)
	{
		const unsigned char uCharacter =
			static_cast<unsigned char>(szTag[uIndex]);
		if (uCharacter == '\0')
		{
			return uIndex > 0u;
		}
		if (uCharacter < 0x20u || uCharacter > 0x7eu)
		{
			return false;
		}
	}

	return false;
}

ZM_SPAWN_POINT_LOOKUP_RESULT ZM_SpawnPoint::FindUniqueInScene(
	Zenith_Scene xScene,
	const char* szTag,
	Zenith_EntityID& xEntityIDOut)
{
	xEntityIDOut = INVALID_ENTITY_ID;
	if (!IsTagValid(szTag))
	{
		return ZM_SPAWN_POINT_LOOKUP_INVALID_TAG;
	}
	if (!xScene.IsValid())
	{
		return ZM_SPAWN_POINT_LOOKUP_INVALID_SCENE;
	}

	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);
	if (pxSceneData == nullptr || !pxSceneData->IsLoaded())
	{
		return ZM_SPAWN_POINT_LOOKUP_INVALID_SCENE;
	}

	u_int uMatchCount = 0u;
	pxSceneData->Query<ZM_SpawnPoint>().ForEach(
		[&](Zenith_EntityID xEntityID, ZM_SpawnPoint& xSpawnPoint)
		{
			if (std::strcmp(xSpawnPoint.GetTag(), szTag) != 0)
			{
				return;
			}

			++uMatchCount;
			if (uMatchCount == 1u)
			{
				xEntityIDOut = xEntityID;
			}
		});

	if (uMatchCount == 0u)
	{
		return ZM_SPAWN_POINT_LOOKUP_MISSING;
	}
	if (uMatchCount > 1u)
	{
		xEntityIDOut = INVALID_ENTITY_ID;
		return ZM_SPAWN_POINT_LOOKUP_DUPLICATE;
	}
	return ZM_SPAWN_POINT_LOOKUP_FOUND;
}

void ZM_SpawnPoint::ClearTag()
{
	std::memset(m_szTag, 0, sizeof(m_szTag));
}
