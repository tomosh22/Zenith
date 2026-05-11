#pragma once
/**
 * DPForge_Behaviour - gym-map crafting station. Consumes the held item if
 * it matches m_eRecipeInputTag and spawns an output item next to the forge,
 * then auto-equips it on the villager that just interacted (skeleton-grade
 * crafting parity — UE source's recipe table system would replace this).
 *
 * Output spawn flow (mirrors DPItemManager_Behaviour::SpawnItemEntity):
 *   1. Destroy the held input entity (RemoveHeldItem keeps the tag side-table
 *      consistent because the source entity is destroyed too).
 *   2. Construct a new item entity at the forge's world position.
 *   3. Attach Transform + ModelComponent + ColliderComponent + DPItemBase
 *      with m_eRecipeOutputTag stamped.
 *   4. Snap the new item into the villager's hand via DP_Player::SetHeldItem.
 */

#include "Components/DPInteractable_Behaviour.h"
#include "Components/DPItemBase_Behaviour.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Source/DevilsPlayground_Tags.h"

#include <cstdio>

class DPForge_Behaviour ZENITH_FINAL : public DPInteractable_Behaviour
{
	friend class Zenith_ScriptComponent;
public:
	ZENITH_BEHAVIOUR_TYPE_NAME(DPForge_Behaviour)

	DPForge_Behaviour() = delete;
	DPForge_Behaviour(Zenith_Entity& xParentEntity)
		: DPInteractable_Behaviour(xParentEntity)
	{}

	// Test + HUD accessors
	DP_ItemTag GetRecipeInputTag() const { return m_eRecipeInputTag; }
	DP_ItemTag GetRecipeOutputTag() const { return m_eRecipeOutputTag; }
	uint32_t GetCraftCount() const { return m_uCraftCount; }

	// Test-only: bypass DPInteractable's proximity / rising-edge dance.
	// Used by Forge_Test to drive the recipe-consume + output-spawn path
	// directly from a fixed frame budget.
	void CraftForTest(Zenith_EntityID xVillager) { HandleInteract(xVillager); }

protected:
	void HandleInteract(Zenith_EntityID xVillager) override
	{
		const DP_ItemTag eHeld = DP_Player::GetHeldItemTag(xVillager);
		if (eHeld != m_eRecipeInputTag) return;

		// 1. Consume input. Mirror DPDoor's key-consumption path: destroy the
		//    underlying entity AND remove from the held-item table so the
		//    side-table doesn't keep a stale tag-entity entry.
		Zenith_EntityID xInput = DP_Player::GetHeldItemEntity(xVillager);
		DP_Player::RemoveHeldItem(xVillager);
		if (xInput.IsValid())
		{
			Zenith_SceneData* pxInputScene = Zenith_SceneManager::GetSceneDataForEntity(xInput);
			if (pxInputScene != nullptr)
			{
				Zenith_Entity xInputEnt = pxInputScene->TryGetEntity(xInput);
				if (xInputEnt.IsValid()) Zenith_SceneManager::Destroy(xInputEnt);
			}
		}

		// 2. Spawn output item at the forge position and auto-equip it.
		Zenith_EntityID xOutput = SpawnOutputItem();
		if (xOutput.IsValid())
		{
			DP_Player::SetHeldItem(xVillager, xOutput);
		}
		++m_uCraftCount;
	}

private:
	Zenith_EntityID SpawnOutputItem()
	{
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneDataForEntity(
			m_xParentEntity.GetEntityID());
		if (pxScene == nullptr) return INVALID_ENTITY_ID;

		Zenith_Maths::Vector3 xForgePos(0.0f);
		if (m_xParentEntity.HasComponent<Zenith_TransformComponent>())
		{
			m_xParentEntity.GetComponent<Zenith_TransformComponent>().GetPosition(xForgePos);
		}
		// Slight offset so the new item doesn't perfectly overlap the forge mesh.
		xForgePos.x += 0.5f;

		char szName[64];
		std::snprintf(szName, sizeof(szName), "ForgeOut_%u_%s",
			m_uCraftCount, DP_ItemTagToString(m_eRecipeOutputTag));
		Zenith_Entity xEntity(pxScene, std::string(szName));
		if (!xEntity.IsValid()) return INVALID_ENTITY_ID;

		if (xEntity.HasComponent<Zenith_TransformComponent>())
		{
			xEntity.GetComponent<Zenith_TransformComponent>().SetPosition(xForgePos);
		}

		Zenith_ModelComponent& xModel = xEntity.AddComponent<Zenith_ModelComponent>();
		xModel.LoadModel(std::string(GAME_ASSETS_DIR) +
			"Meshes/LevelPrototyping_Meshes_SM_Cube" ZENITH_MODEL_EXT);

		Zenith_ColliderComponent& xCollider = xEntity.AddComponent<Zenith_ColliderComponent>();
		xCollider.AddCollider(COLLISION_VOLUME_TYPE_SPHERE, RIGIDBODY_TYPE_STATIC);

		DPItemBase_Behaviour* pxItemBase = xEntity
			.AddComponent<Zenith_ScriptComponent>()
			.AddScript<DPItemBase_Behaviour>();
		if (pxItemBase) pxItemBase->SetTag(m_eRecipeOutputTag);

		return xEntity.GetEntityID();
	}

	DP_ItemTag m_eRecipeInputTag  = DP_ItemTag::Iron;
	DP_ItemTag m_eRecipeOutputTag = DP_ItemTag::Key;
	uint32_t   m_uCraftCount     = 0;
};
