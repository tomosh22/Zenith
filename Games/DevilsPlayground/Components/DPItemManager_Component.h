#pragma once
#include "Core/Zenith_Engine.h"
/**
 * DPItemManager_Component - one per scene; instantiates items at spawners
 * during OnStart.
 *
 * Spawn flow:
 *   1. Iterate every DPItemSpawn_Component in the active scene.
 *   2. For each spawner, look up its tag from a fixed spawn-index mapping.
 *   3. Instantiate the shared item prefab (cube mesh + sphere collider, baked)
 *      at the spawner's world position, attach DPItemBase_Component, stamp the
 *      tag. The index->tag mapping stays because the tag is per-instance, not
 *      baked into the prefab.
 *
 * SourceBugFixed (FindItemByTag): the source AItemManager::FindItemByType
 * derefs on miss. The fix lives in DP_Items::FindItemByTag which returns
 * INVALID_ENTITY_ID on miss.
 */

#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "DataStream/Zenith_DataStream.h"
// Contract exception (creation/lifecycle, not a cross-state read): the manager
// iterates its own DPItemSpawn anchors and instantiates DPItemBase item
// entities at them. See Components/CLAUDE.md.
#include "Components/DPItemSpawn_Component.h"
#include "Components/DPItemBase_Component.h"
#include "Physics/Zenith_Physics.h"
#include "Prefab/Zenith_Prefab.h"
#include "Source/DPResources.h"

#include "Source/PublicInterfaces.h"
#include "Source/DevilsPlayground_Tags.h"

#include "Collections/Zenith_HashMap.h"

#include <cstdio>

class DPItemManager_Component ZENITH_FINAL
{
public:
	DPItemManager_Component() = delete;
	DPItemManager_Component(Zenith_Entity& xParentEntity)
		: m_xParentEntity(xParentEntity)
	{}

	// Heap-stability: s_pxInstance points at `this`, and component pools
	// relocate on resize / swap-and-pop / cross-scene transfer. Hand-written
	// moves repoint the singleton at the new address; copies deleted.
	DPItemManager_Component(const DPItemManager_Component&) = delete;
	DPItemManager_Component& operator=(const DPItemManager_Component&) = delete;

	DPItemManager_Component(DPItemManager_Component&& xOther) noexcept
		: m_xParentEntity(xOther.m_xParentEntity)
		, m_xItemTagTable(std::move(xOther.m_xItemTagTable))
	{
		if (s_pxInstance == &xOther) s_pxInstance = this;
	}

	DPItemManager_Component& operator=(DPItemManager_Component&& xOther) noexcept
	{
		if (this != &xOther)
		{
			m_xParentEntity = xOther.m_xParentEntity;
			m_xItemTagTable = std::move(xOther.m_xItemTagTable);
			if (s_pxInstance == &xOther) s_pxInstance = this;
		}
		return *this;
	}

	~DPItemManager_Component()
	{
		// Pool relocation destructs the moved-from source; only clear the
		// singleton when it still points at THIS instance.
		if (s_pxInstance == this) s_pxInstance = nullptr;
	}

	void OnAwake()
	{
		// Set the singleton ASAP — dependent OnAwake hooks may already query
		// it (engine fires Awake on all entities before any OnStart).
		Zenith_Assert(s_pxInstance == nullptr,
			"DPItemManager_Component singleton double-instantiated");
		s_pxInstance = this;
	}

	void OnDestroy()
	{
		// m_xItemTagTable is auto-cleared by Zenith_HashMap's
		// destructor when this component is freed. Because the
		// component lives on a scene-owned entity, scene unload (which
		// destroys every entity + fires this OnDestroy) is the only
		// way the component ever dies -- so the tag table can never
		// outlive its scene. That guarantee replaces the previous
		// process-global g_xItemTagTable + manual cleanup in
		// DP_Player::ResetForNewRun, which was vulnerable to leaking
		// stale rows across batched tests when an item entity was
		// destroyed through a path that skipped its OnDestroy hook.
		if (s_pxInstance == this) s_pxInstance = nullptr;
	}

	// Component contract: version-only payload (the tag table is rebuilt
	// at runtime by the item components).
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

	//==========================================================================
	// Item registry. Moved from anonymous-namespace globals in
	// PublicInterfaces.cpp (g_xItemTagTable) so the side table is owned
	// by this scene-bound component -- the map's lifetime matches the
	// scene's, and there's no opportunity for stale rows to leak
	// across scene transitions (Phase B fix for the bot-test batched
	// failure investigated 2026-05-17).
	//==========================================================================
	void RegisterItemTag(Zenith_EntityID xItem, DP_ItemTag eTag)
	{
		m_xItemTagTable.Insert(xItem, eTag);
	}

	void UnregisterItemTag(Zenith_EntityID xItem)
	{
		m_xItemTagTable.Remove(xItem);
	}

	DP_ItemTag GetItemTag(Zenith_EntityID xItem) const
	{
		const DP_ItemTag* pxTag = m_xItemTagTable.TryGet(xItem);
		if (pxTag == nullptr) return DP_ItemTag::None;
		return *pxTag;
	}

	// SourceBugFixed (carried over from the old DP_Items::FindItemByTag):
	// miss returns INVALID_ENTITY_ID rather than dereferencing.
	Zenith_EntityID FindItemByTag(DP_ItemTag eTag) const
	{
		Zenith_HashMap<Zenith_EntityID, DP_ItemTag>::Iterator it(m_xItemTagTable);
		while (!it.Done())
		{
			if (it.GetValue() == eTag) return it.GetKey();
			it.Next();
		}
		return INVALID_ENTITY_ID;
	}

	void OnStart()
	{
		// Active scene; if it's not loaded yet just bail (e.g. test harness
		// will retry on a later boot).
		Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneData(xScene);
		if (pxScene == nullptr) return;

		// Walk every DPItemSpawn_Component and create an item entity for it.
		// Stable spawner ordering (the editor authoring loop creates ItemSpawn_0
		// .. ItemSpawn_14 in fixed order) lets a hand-written index→tag table
		// drive the assignment without needing prefab data.
		uint32_t uIndex = 0;
		DP_Query::ForEachComponentInActiveScene<DPItemSpawn_Component>(
			[this, pxScene, &uIndex](Zenith_EntityID /*xId*/, DPItemSpawn_Component& xSpawner)
			{
				if (xSpawner.HasSpawned()) { ++uIndex; return; }
				const DP_ItemTag eTag = TagForSpawnerIndex(uIndex);
				const Zenith_Maths::Vector3 xPos = xSpawner.GetSpawnWorldPos();
				Zenith_EntityID xSpawned = SpawnItemEntity(pxScene, eTag, xPos, uIndex);
				if (xSpawned.IsValid()) xSpawner.MarkSpawned(xSpawned);
				++uIndex;
			});
	}

	// Distribute 15 spawners across:
	//   0..4   → Objective1..Objective5
	//   5..8   → Iron
	//   9..12  → Key
	//   13..14 → SkeletonKey
	// Public/static so tests can verify the mapping without needing the
	// manager instance.
	static DP_ItemTag TagForSpawnerIndex(uint32_t uIndex)
	{
		switch (uIndex)
		{
			case 0: return DP_ItemTag::Objective1;
			case 1: return DP_ItemTag::Objective2;
			case 2: return DP_ItemTag::Objective3;
			case 3: return DP_ItemTag::Objective4;
			case 4: return DP_ItemTag::Objective5;
			case 5: case 6: case 7: case 8:    return DP_ItemTag::Iron;
			case 9: case 10: case 11: case 12: return DP_ItemTag::Key;
			case 13: case 14:                  return DP_ItemTag::SkeletonKey;
			default: return DP_ItemTag::Iron;
		}
	}

	static DPItemManager_Component* Instance() { return s_pxInstance; }

private:
	// Build a fully-formed item entity from scratch:
	//   Transform (auto on entity construction) - position set after.
	//   ModelComponent + LoadModel(cube) so the item renders.
	//   ColliderComponent (sphere, static) for click-to-pickup raycasts.
	//   DPItemBase_Component with tag stamped via SetTag.
	// Returns the new entity's ID, or INVALID_ENTITY_ID on any failure.
	Zenith_EntityID SpawnItemEntity(
		Zenith_SceneData* pxScene,
		DP_ItemTag eTag,
		const Zenith_Maths::Vector3& xPos,
		uint32_t uIndex)
	{
		Zenith_Prefab* pxItemPrefab = DevilsPlayground::Resources().m_xItemPrefab.GetDirect();
		if (pxItemPrefab == nullptr) return INVALID_ENTITY_ID;

		char szName[64];
		std::snprintf(szName, sizeof(szName), "Item_%u_%s", uIndex, DP_ItemTagToString(eTag));

		// Model (cube) + sphere/static collider are baked into the item prefab;
		// Instantiate places it at xPos and finalizes the collider body.
		Zenith_Entity xEntity = pxItemPrefab->Instantiate(pxScene, std::string(szName), xPos);
		if (!xEntity.IsValid()) return INVALID_ENTITY_ID;

		// Component + tag stay post-instantiation. Prefab::Instantiate already
		// dispatched the entity's OnAwake wave, so a component added after it
		// never receives the hook from the lifecycle scheduler -- invoke it
		// explicitly (mirrors the old script system's immediate-OnAwake-on-
		// attach semantics: OnAwake registers the tag side-table entry with
		// m_eTag=None; SetTag then re-registers with the real tag + tints).
		DPItemBase_Component& xItemBase = xEntity.AddComponent<DPItemBase_Component>();
		xItemBase.OnAwake();
		xItemBase.SetTag(eTag);

		return xEntity.GetEntityID();
	}

	static inline DPItemManager_Component* s_pxInstance = nullptr;

	Zenith_Entity m_xParentEntity;

	// Item registry -- one entry per live DPItemBase entity in this
	// scene. Cleared automatically when this component is destroyed (scene unload).
	Zenith_HashMap<Zenith_EntityID, DP_ItemTag> m_xItemTagTable;
};
