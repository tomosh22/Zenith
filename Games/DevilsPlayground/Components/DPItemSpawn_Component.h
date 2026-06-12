#pragma once
/**
 * DPItemSpawn_Component - spawn anchor for one item type.
 *
 * Pure positional anchor — does not auto-spawn. DPItemManager_Component
 * enumerates spawners during OnStart and asks each to instantiate its
 * configured prefab. This avoids the OnAwake/OnStart race that would occur
 * if the spawner spawned itself (manager might not be ready yet).
 */

#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "DataStream/Zenith_DataStream.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "Prefab/Zenith_Prefab.h"

#include "Source/PublicInterfaces.h"

class DPItemSpawn_Component ZENITH_FINAL
{
public:
	DPItemSpawn_Component() = delete;
	DPItemSpawn_Component(Zenith_Entity& xParentEntity)
		: m_xParentEntity(xParentEntity)
	{}

	// Component contract: version-only payload (spawner config is stamped at
	// runtime by procgen / tests).
	void WriteToDataStream(Zenith_DataStream& xStream) const
	{
		const u_int uVersion = 1;
		xStream << uVersion;
	}
	void ReadFromDataStream(Zenith_DataStream& xStream)
	{
		u_int uVersion = 0;
		xStream >> uVersion;
	}
#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel() {}
#endif

	DP_ItemTag GetSpawnTag() const { return m_eTag; }
	void SetSpawnTag(DP_ItemTag eTag) { m_eTag = eTag; }

	const PrefabHandle& GetItemPrefab() const { return m_xItemPrefab; }
	void SetItemPrefab(const PrefabHandle& xPrefab) { m_xItemPrefab = xPrefab; }

	bool HasSpawned() const { return m_bSpawned; }
	void MarkSpawned(Zenith_EntityID xId) { m_bSpawned = true; m_xSpawnedItem = xId; }
	Zenith_EntityID GetSpawnedItem() const { return m_xSpawnedItem; }

	Zenith_Maths::Vector3 GetSpawnWorldPos() const
	{
		Zenith_Maths::Vector3 xPos(0.0f);
		if (m_xParentEntity.HasComponent<Zenith_TransformComponent>())
		{
			m_xParentEntity.GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
		}
		return xPos;
	}

private:
	Zenith_Entity   m_xParentEntity;
	DP_ItemTag      m_eTag = DP_ItemTag::None;
	PrefabHandle    m_xItemPrefab;
	bool            m_bSpawned = false;
	Zenith_EntityID m_xSpawnedItem;
};
