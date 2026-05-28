#pragma once
/**
 * DPItemManager_Behaviour - one per scene; instantiates items at spawners
 * during OnStart.
 *
 * Spawn flow:
 *   1. Iterate every DPItemSpawn_Behaviour in the active scene.
 *   2. For each spawner, look up its tag from a fixed spawn-index mapping
 *      (the prefab system isn't wired in skeleton-grade yet).
 *   3. Create an item entity at the spawner's world position, attach a cube
 *      mesh + sphere collider + DPItemBase_Behaviour, stamp the tag.
 *
 * SourceBugFixed (FindItemByTag): the source AItemManager::FindItemByType
 * derefs on miss. The fix lives in DP_Items::FindItemByTag
 * (PublicInterfaces.cpp) which returns INVALID_ENTITY_ID on miss.
 */

#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Zenith_SceneSystem.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "Components/DPItemSpawn_Behaviour.h"
#include "Components/DPItemBase_Behaviour.h"
#include "Physics/Zenith_Physics.h"

#include "Source/PublicInterfaces.h"
#include "Source/DevilsPlayground_Tags.h"

#include "Collections/Zenith_HashMap.h"

#include <cstdio>

class DPItemManager_Behaviour ZENITH_FINAL : Zenith_ScriptBehaviour
{
	friend class Zenith_ScriptComponent;
public:
	ZENITH_BEHAVIOUR_TYPE_NAME(DPItemManager_Behaviour)

	DPItemManager_Behaviour() = delete;
	DPItemManager_Behaviour(Zenith_Entity& /*xParentEntity*/) {}

	void OnAwake() ZENITH_FINAL override
	{
		// Set the singleton ASAP — dependent OnAwake hooks may already query
		// it (engine fires Awake on all entities before any OnStart).
		s_pxInstance = this;
	}

	void OnDestroy() ZENITH_FINAL override
	{
		// m_xItemTagTable is auto-cleared by Zenith_HashMap's
		// destructor when this script instance is freed. Because the
		// script lives on a scene-owned entity, scene unload (which
		// destroys every entity + fires this OnDestroy) is the only
		// way the script ever dies -- so the tag table can never
		// outlive its scene. That guarantee replaces the previous
		// process-global g_xItemTagTable + manual cleanup in
		// DP_Player::ResetForNewRun, which was vulnerable to leaking
		// stale rows across batched tests when an item entity was
		// destroyed through a path that skipped its OnDestroy hook.
		if (s_pxInstance == this) s_pxInstance = nullptr;
	}

	//==========================================================================
	// Item registry. Moved from anonymous-namespace globals in
	// PublicInterfaces.cpp (g_xItemTagTable) so the side table is owned
	// by this scene-bound script -- the map's lifetime matches the
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

	void OnStart() ZENITH_FINAL override
	{
		// Active scene; if it's not loaded yet just bail (e.g. test harness
		// will retry on a later boot).
		Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneData(xScene);
		if (pxScene == nullptr) return;

		// Walk every DPItemSpawn_Behaviour and create an item entity for it.
		// Stable spawner ordering (the editor authoring loop creates ItemSpawn_0
		// .. ItemSpawn_14 in fixed order) lets a hand-written index→tag table
		// drive the assignment without needing prefab data.
		uint32_t uIndex = 0;
		DP_Query::ForEachScriptInActiveScene<DPItemSpawn_Behaviour>(
			[this, pxScene, &uIndex](Zenith_EntityID /*xId*/, DPItemSpawn_Behaviour& xSpawner)
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

	static DPItemManager_Behaviour* Instance() { return s_pxInstance; }

private:
	// Build a fully-formed item entity from scratch:
	//   Transform (auto on entity construction) - position set after.
	//   ModelComponent + LoadModel(cube) so the item renders.
	//   ColliderComponent (sphere, static) for click-to-pickup raycasts.
	//   ScriptComponent + DPItemBase_Behaviour with tag stamped via SetTag.
	// Returns the new entity's ID, or INVALID_ENTITY_ID on any failure.
	Zenith_EntityID SpawnItemEntity(
		Zenith_SceneData* pxScene,
		DP_ItemTag eTag,
		const Zenith_Maths::Vector3& xPos,
		uint32_t uIndex)
	{
		char szName[64];
		std::snprintf(szName, sizeof(szName), "Item_%u_%s", uIndex, DP_ItemTagToString(eTag));
		Zenith_Entity xEntity(pxScene, std::string(szName));
		if (!xEntity.IsValid()) return INVALID_ENTITY_ID;

		// Transform is auto-added by the entity constructor; just position it.
		if (xEntity.HasComponent<Zenith_TransformComponent>())
		{
			xEntity.GetComponent<Zenith_TransformComponent>().SetPosition(xPos);
		}

		// Model: prototype cube. The path matches the other authoring sites in
		// DevilsPlayground.cpp::MakeMeshPath which substitutes /Engine/* for
		// this same cube.
		Zenith_ModelComponent& xModel = xEntity.AddComponent<Zenith_ModelComponent>();
		xModel.LoadModel(std::string(GAME_ASSETS_DIR) + "Meshes/LevelPrototyping_Meshes_SM_Cube" + ZENITH_MODEL_EXT);

		// Collider: sphere, static. 0.3m radius matches a small-ish pickup.
		Zenith_ColliderComponent& xCollider = xEntity.AddComponent<Zenith_ColliderComponent>();
		xCollider.AddCollider(COLLISION_VOLUME_TYPE_SPHERE, RIGIDBODY_TYPE_STATIC);

		// Script: DPItemBase_Behaviour. AddScript<T> calls OnAwake immediately,
		// which registers the tag side-table entry and applies tag tinting.
		// We then SetTag for the actual tag (initial m_eTag is None).
		DPItemBase_Behaviour* pxItemBase = xEntity.AddComponent<Zenith_ScriptComponent>().AddScript<DPItemBase_Behaviour>();
		if (pxItemBase) pxItemBase->SetTag(eTag);

		return xEntity.GetEntityID();
	}

	static inline DPItemManager_Behaviour* s_pxInstance = nullptr;

	// Item registry -- one entry per live DPItemBase entity in this
	// scene. Cleared automatically when this script is destroyed (scene unload).
	Zenith_HashMap<Zenith_EntityID, DP_ItemTag> m_xItemTagTable;
};
