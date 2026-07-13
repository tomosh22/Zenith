#include "Zenith.h"

#include "Zenithmon/Components/ZM_WarpTrigger.h"

#include "DataStream/Zenith_DataStream.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "Zenithmon/Components/ZM_GameStateManager.h"
#include "Zenithmon/Components/ZM_PlayerController.h"

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

#include <cstring>

ZM_WarpTrigger::ZM_WarpTrigger(Zenith_Entity& xParentEntity)
	: m_xParentEntity(xParentEntity)
{
}

void ZM_WarpTrigger::OnStart()
{
	ResetOverlapLatch();
	if (!ZM_GameStateManager::IsWarpDestinationValid(
		m_uTargetBuildIndex, m_szSpawnTag))
	{
		ClearConfiguration();
	}
	ReassertSensor();
}

void ZM_WarpTrigger::OnCollisionEnter(Zenith_Entity& xOther)
{
	TryHandleOverlap(xOther);
}

void ZM_WarpTrigger::OnCollisionExit(Zenith_EntityID xOtherEntityID)
{
	if (m_xOverlappingPlayerEntityID == xOtherEntityID)
	{
		ResetOverlapLatch();
	}
}

bool ZM_WarpTrigger::Configure(
	u_int uTargetBuildIndex,
	const char* szSpawnTag)
{
	if (!ZM_GameStateManager::IsWarpDestinationValid(
		uTargetBuildIndex, szSpawnTag))
	{
		return false;
	}

	char szValidatedTag[uTAG_CAPACITY] = {};
	const size_t ulLength = std::strlen(szSpawnTag);
	std::memcpy(szValidatedTag, szSpawnTag, ulLength);

	m_uTargetBuildIndex = uTargetBuildIndex;
	std::memcpy(m_szSpawnTag, szValidatedTag, sizeof(m_szSpawnTag));
	ResetOverlapLatch();
	return true;
}

bool ZM_WarpTrigger::TryHandleOverlap(Zenith_Entity& xOther)
{
	if (m_bLatched || !xOther.IsValid()
		|| xOther.TryGetComponent<ZM_PlayerController>() == nullptr)
	{
		return false;
	}

	Zenith_EntityID xAuthoritativePlayerEntityID = INVALID_ENTITY_ID;
	if (!ZM_GameStateManager::TryGetUniqueActiveScenePlayerEntityID(
		xAuthoritativePlayerEntityID)
		|| xAuthoritativePlayerEntityID != xOther.GetEntityID())
	{
		return false;
	}
	if (!ZM_GameStateManager::RequestWarp(
		m_uTargetBuildIndex, m_szSpawnTag))
	{
		return false;
	}

	m_xOverlappingPlayerEntityID = xOther.GetEntityID();
	m_bLatched = true;
	return true;
}

void ZM_WarpTrigger::ResetOverlapLatch()
{
	m_xOverlappingPlayerEntityID = INVALID_ENTITY_ID;
	m_bLatched = false;
}

void ZM_WarpTrigger::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << uSERIALIZATION_VERSION;
	xStream << m_uTargetBuildIndex;
	xStream.WriteData(m_szSpawnTag, sizeof(m_szSpawnTag));
}

void ZM_WarpTrigger::ReadFromDataStream(Zenith_DataStream& xStream)
{
	u_int uVersion = 0u;
	xStream >> uVersion;
	ClearConfiguration();
	if (uVersion != uSERIALIZATION_VERSION)
	{
		return;
	}

	u_int uTargetBuildIndex = uINVALID_BUILD_INDEX;
	char szSpawnTag[uTAG_CAPACITY] = {};
	xStream >> uTargetBuildIndex;
	xStream.ReadData(szSpawnTag, sizeof(szSpawnTag));
	Configure(uTargetBuildIndex, szSpawnTag);
}

#ifdef ZENITH_TOOLS
void ZM_WarpTrigger::RenderPropertiesPanel()
{
	ImGui::Text("Target build index: %u", m_uTargetBuildIndex);
	ImGui::Text("Spawn tag: %s", m_szSpawnTag[0] != '\0'
		? m_szSpawnTag : "<invalid>");
	ImGui::Text("Overlap latched: %s", m_bLatched ? "true" : "false");
}
#endif

void ZM_WarpTrigger::ReassertSensor()
{
	Zenith_ColliderComponent* pxCollider =
		m_xParentEntity.TryGetComponent<Zenith_ColliderComponent>();
	if (pxCollider != nullptr)
	{
		pxCollider->SetIsSensor(true);
	}
}

void ZM_WarpTrigger::ClearConfiguration()
{
	m_uTargetBuildIndex = uINVALID_BUILD_INDEX;
	std::memset(m_szSpawnTag, 0, sizeof(m_szSpawnTag));
	ResetOverlapLatch();
}
