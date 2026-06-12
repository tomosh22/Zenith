#pragma once
#include "Core/Zenith_Engine.h"
/**
 * DPForge_Component - gym-map crafting station. Consumes the held item if
 * it matches m_eRecipeInputTag and spawns an output item next to the forge,
 * then auto-equips it on the villager that just interacted (skeleton-grade
 * crafting parity — UE source's recipe table system would replace this).
 *
 * Output spawn flow (mirrors DPItemManager_Component::SpawnItemEntity):
 *   1. Destroy the held input entity (RemoveHeldItem keeps the tag side-table
 *      consistent because the source entity is destroyed too).
 *   2. Instantiate the shared item prefab (cube mesh + sphere collider, baked)
 *      at the forge's world position.
 *   3. Attach DPItemBase_Component with m_eRecipeOutputTag stamped
 *      (post-instantiation).
 *   4. Snap the new item into the villager's hand via DP_Player::SetHeldItem.
 */

#include "Components/DPInteractable_Base.h"
// Contract exception (creation/lifecycle, not a cross-state read): the forge
// spawns an item entity and attaches DPItemBase_Component to it. See
// Components/CLAUDE.md.
#include "Components/DPItemBase_Component.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "Maths/Zenith_Maths.h"
#include "Core/Zenith_AudioBus.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"
#include "Prefab/Zenith_Prefab.h"
#include "Source/DPResources.h"

#include "Source/PublicInterfaces.h"
#include "Source/DevilsPlayground_Tags.h"
#include "Source/DP_Tuning.h"

#include <cstdio>

class DPForge_Component ZENITH_FINAL : public DPInteractable_Base
{
public:
	DPForge_Component() = delete;
	DPForge_Component(Zenith_Entity& xParentEntity)
		: DPInteractable_Base(xParentEntity)
	{}

	// Test + HUD accessors
	DP_ItemTag GetRecipeInputTag() const { return m_eRecipeInputTag; }
	DP_ItemTag GetRecipeOutputTag() const { return m_eRecipeOutputTag; }
	uint32_t GetCraftCount() const { return m_uCraftCount; }

	// MVP-2.3: configure the forge's recipe. Each forge instance holds
	// ONE recipe (input -> output); a level with multiple recipes
	// places multiple forge entities, each with its own recipe. This
	// matches the gym scene's design (Gym_Forge has four Iron spawners
	// around one forge) and keeps the surface narrow for MVP.
	//
	// Default is Iron -> Key (matches the original prototype's only
	// recipe). Authoring / test code calls SetRecipe after attaching
	// the DPForge_Component to override.
	void SetRecipe(DP_ItemTag eInput, DP_ItemTag eOutput)
	{
		m_eRecipeInputTag  = eInput;
		m_eRecipeOutputTag = eOutput;
	}

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
			Zenith_SceneData* pxInputScene = g_xEngine.Scenes().GetSceneDataForEntity(xInput);
			if (pxInputScene != nullptr)
			{
				Zenith_Entity xInputEnt = pxInputScene->TryGetEntity(xInput);
				if (xInputEnt.IsValid()) xInputEnt.Destroy();
			}
		}

		// 2. Spawn output item at the forge position and auto-equip it.
		Zenith_EntityID xOutput = SpawnOutputItem();
		if (xOutput.IsValid())
		{
			DP_Player::SetHeldItem(xVillager, xOutput);
		}
		++m_uCraftCount;

		// MVP-2.3.4: forge craft emits a hammer sound audible across
		// the village. Tuning.json:
		//   forge_audible_at_m   = 30.0  (per GDD: priest hears it
		//                                 from anywhere in the village
		//                                 if the craft happens during
		//                                 his patrol)
		//   forge_audible_loudness = 1.0
		// The radius is the audible-at distance; loudness is the
		// stimulus intensity. Future variant aelfrics could tune
		// their hearing threshold to attenuate.
		Zenith_Maths::Vector3 xForgePos(0.0f);
		if (m_xParentEntity.HasComponent<Zenith_TransformComponent>())
		{
			m_xParentEntity.GetComponent<Zenith_TransformComponent>().GetPosition(xForgePos);
		}
		const float fAudibleRadius =
			DP_Tuning::Get<float>("interactables.forge_audible_at_m");
		const float fAudibleLoudness =
			DP_Tuning::Get<float>("interactables.forge_audible_loudness");
		// AudioBus emit -- test instrumentation (recorded by
		// GetEmittedSoundsForTest for assertion harnesses) + future
		// shipping audio system hook.
		Zenith_AudioBus::EmitSound("DP.Forge.Hammer",
			xForgePos, fAudibleLoudness, fAudibleRadius);
		// PerceptionSystem emit -- this is the actual priest-hearing
		// path. Without this the AudioBus call records the event but
		// no AI agent reacts. Source entity is the forge so the
		// stimulus is attributed to the forge's position when the
		// priest's BridgePerceptionToBlackboard reads GetLastHeardSoundFor.
		Zenith_PerceptionSystem::EmitSoundStimulus(
			xForgePos, fAudibleLoudness, fAudibleRadius,
			m_xParentEntity.GetEntityID());

		// Phase-5-audit (2026-05-16): emit DP_OnForgeCrafted so the
		// analyzer can require "ForgeUsed" as a verified mechanic + the
		// visualiser can mark the craft moment with a distinct marker.
		// xOutput may be INVALID if SpawnOutputItem failed; downstream
		// consumers must guard on IsValid().
		Zenith_EventDispatcher::Get().Dispatch(
			DP_OnForgeCrafted{
				xVillager,
				m_xParentEntity.GetEntityID(),
				xOutput });
	}

private:
	Zenith_EntityID SpawnOutputItem()
	{
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(
			m_xParentEntity.GetEntityID());
		if (pxScene == nullptr) return INVALID_ENTITY_ID;

		Zenith_Maths::Vector3 xForgePos(0.0f);
		if (m_xParentEntity.HasComponent<Zenith_TransformComponent>())
		{
			m_xParentEntity.GetComponent<Zenith_TransformComponent>().GetPosition(xForgePos);
		}
		// Slight offset so the new item doesn't perfectly overlap the forge mesh.
		xForgePos.x += 0.5f;

		Zenith_Prefab* pxItemPrefab = DevilsPlayground::Resources().m_xItemPrefab.GetDirect();
		if (pxItemPrefab == nullptr) return INVALID_ENTITY_ID;

		char szName[64];
		std::snprintf(szName, sizeof(szName), "ForgeOut_%u_%s",
			m_uCraftCount, DP_ItemTagToString(m_eRecipeOutputTag));

		// Reuse the shared item prefab (cube mesh + sphere collider, baked);
		// Instantiate places it at the forge offset and finalizes the collider.
		Zenith_Entity xEntity = pxItemPrefab->Instantiate(pxScene, std::string(szName), xForgePos);
		if (!xEntity.IsValid()) return INVALID_ENTITY_ID;

		// Component + tag stay post-instantiation. Prefab::Instantiate already
		// dispatched the entity's OnAwake wave, so a component added after it
		// never receives the hook from the lifecycle scheduler -- invoke it
		// explicitly (mirrors the old script system's immediate-OnAwake-on-
		// attach semantics: registers tag=None, then SetTag re-registers).
		DPItemBase_Component& xItemBase = xEntity.AddComponent<DPItemBase_Component>();
		xItemBase.OnAwake();
		xItemBase.SetTag(m_eRecipeOutputTag);

		return xEntity.GetEntityID();
	}

	DP_ItemTag m_eRecipeInputTag  = DP_ItemTag::Iron;
	DP_ItemTag m_eRecipeOutputTag = DP_ItemTag::Key;
	uint32_t   m_uCraftCount     = 0;
};
