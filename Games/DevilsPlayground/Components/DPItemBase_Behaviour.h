#pragma once
/**
 * DPItemBase_Behaviour - tagged pickup item.
 *
 * Each item entity carries a DP_ItemTag. On OnAwake the tag is registered
 * with DP_Items::Internal_RegisterItemTag so DP_Items::GetItemTag /
 * FindItemByTag work without per-frame component scanning. Pickup is
 * distance-based (no trigger semantics in Zenith): each frame, if the
 * possessed villager is within m_fPickupRadius, the item is picked up.
 */

#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "Maths/Zenith_Maths.h"
#include "Flux/Flux_ModelInstance.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"

#include "Source/PublicInterfaces.h"
#include "Source/DPMaterials.h"
#include "Source/DP_Reagents.h"
#include "Components/DPVillager_Behaviour.h"

class DPItemBase_Behaviour ZENITH_FINAL : Zenith_ScriptBehaviour
{
	friend class Zenith_ScriptComponent;
public:
	ZENITH_BEHAVIOUR_TYPE_NAME(DPItemBase_Behaviour)

	DPItemBase_Behaviour() = delete;
	DPItemBase_Behaviour(Zenith_Entity& /*xParentEntity*/) {}

	void OnAwake() ZENITH_FINAL override
	{
		DP_Items::Internal_RegisterItemTag(m_xParentEntity.GetEntityID(), m_eTag);
		ApplyTagTint();
		ResolveReagentChannelFromRegistry();
	}

	void OnStart() ZENITH_FINAL override
	{
		// Re-apply on OnStart so runtime-spawned items (where the model component
		// is added immediately before the script's OnAwake runs) catch up if
		// model materials weren't ready at OnAwake.
		ApplyTagTint();
	}

	void OnDestroy() ZENITH_FINAL override
	{
		DP_Items::Internal_UnregisterItemTag(m_xParentEntity.GetEntityID());
	}

	void OnUpdate(const float fDt) ZENITH_FINAL override
	{
		// MVP-2.2.4: evaporate countdown. Set by BeginPostDropCooldown
		// for items with special_behaviour="evaporates_after_drop".
		// On zero-crossing the entity is destroyed. We check this
		// BEFORE the post-drop cooldown block so a dropped reagent's
		// evaporate timer keeps ticking through the cooldown window
		// (the cooldown only gates re-pickup, not the destroy timer).
		if (m_fEvaporateRemaining > 0.0f)
		{
			m_fEvaporateRemaining -= fDt;
			if (m_fEvaporateRemaining <= 0.0f)
			{
				m_fEvaporateRemaining = 0.0f;
				Zenith_SceneData* pxScene =
					Zenith_SceneManager::GetSceneDataForEntity(m_xParentEntity.GetEntityID());
				if (pxScene != nullptr)
				{
					Zenith_Entity xEnt = pxScene->TryGetEntity(m_xParentEntity.GetEntityID());
					if (xEnt.IsValid()) Zenith_SceneManager::Destroy(xEnt);
				}
				return;
			}
		}

		// MVP-1.4.5: after a drop, the item sits AT the villager's
		// foot position -- which is well inside m_fPickupRadius, so the
		// next-frame OnUpdate would immediately re-pick-up. Hold a
		// short cooldown so the player has to step away (or another
		// villager has to walk over) before pickup re-engages.
		if (m_fPostDropCooldownSec > 0.0f)
		{
			m_fPostDropCooldownSec -= fDt;
			if (m_fPostDropCooldownSec < 0.0f) m_fPostDropCooldownSec = 0.0f;
			return;
		}

		// Distance-based pickup: when the possessed villager wanders within
		// m_fPickupRadius of an item that has no holder, the villager picks
		// it up. Source-game semantics: an item already in the villager's
		// hand is dropped (swap), but for skeleton-grade we just refuse the
		// new pickup if held — the swap behaviour can be added in Wave 4.
		const Zenith_EntityID xVillager = DP_Player::GetPossessedVillager();
		if (!xVillager.IsValid()) return;
		if (DP_Player::GetHeldItemTag(xVillager) != DP_ItemTag::None) return;

		Zenith_Maths::Vector3 xMyPos = DP_Items::GetItemWorldPos(m_xParentEntity.GetEntityID());

		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneDataForEntity(xVillager);
		if (pxScene == nullptr) return;
		Zenith_Entity xV = pxScene->TryGetEntity(xVillager);
		if (!xV.IsValid()) return;
		if (!xV.HasComponent<Zenith_TransformComponent>()) return;
		Zenith_Maths::Vector3 xVPos;
		xV.GetComponent<Zenith_TransformComponent>().GetPosition(xVPos);

		const float fDx = xMyPos.x - xVPos.x;
		const float fDz = xMyPos.z - xVPos.z;
		if (fDx * fDx + fDz * fDz > m_fPickupRadius * m_fPickupRadius) return;

		// MVP-2.1.4: Child archetype can't carry tools. The GDD framing
		// is "small hands"; mechanically it means the Child has to
		// route through other villagers for any door / forge work. We
		// look up the possessed villager's archetype id and refuse
		// pickup if it's "Child" AND the item is in the tool set
		// (DP_IsToolTag -- Iron, Key). Objectives + SkeletonKey are
		// exempt so a Child can still complete the run.
		if (xV.HasComponent<Zenith_ScriptComponent>())
		{
			DPVillager_Behaviour* pxV =
				xV.GetComponent<Zenith_ScriptComponent>().GetScript<DPVillager_Behaviour>();
			if (pxV != nullptr
				&& pxV->GetArchetypeId() == "Child"
				&& DP_IsToolTag(m_eTag))
			{
				return;
			}
		}

		// MVP-2.2.2: per-tag pickup channel. For reagents,
		// m_fPickupChannelDuration > 0 (loaded from DP_Reagents at
		// OnAwake); pickup only fires after the villager stays in
		// range for the full channel duration. For tools / objectives
		// (channel duration 0), pickup fires immediately.
		//
		// State machine on the ITEM:
		//   * No channel yet: start one with the current villager.
		//   * Same villager in range: tick down. On 0, fire pickup.
		//   * Same villager OUT of range: reset (player can walk away
		//     to cancel mid-channel).
		//   * Different villager: not a real concern in MVP (only one
		//     possessed villager at a time); treated as restart.
		if (m_fPickupChannelDuration > 0.0f)
		{
			const bool bSameVillager = m_xChannelingVillager.IsValid()
				&& m_xChannelingVillager.m_uIndex == xVillager.m_uIndex
				&& m_xChannelingVillager.m_uGeneration == xVillager.m_uGeneration;
			if (!bSameVillager)
			{
				// Fresh channel.
				m_xChannelingVillager = xVillager;
				m_fChannelRemaining = m_fPickupChannelDuration;
				return;
			}
			m_fChannelRemaining -= fDt;
			if (m_fChannelRemaining > 0.0f)
			{
				return; // still channeling
			}
			// Channel complete: fall through to the SetHeldItem block
			// below. Reset state so a future drop+re-channel restarts
			// from full.
			m_xChannelingVillager = INVALID_ENTITY_ID;
			m_fChannelRemaining = 0.0f;
		}

		DP_Player::SetHeldItem(xVillager, m_xParentEntity.GetEntityID());
		Zenith_EventDispatcher::Get().Dispatch(
			DP_OnItemPickedUp{ xVillager, m_xParentEntity.GetEntityID() });

		// MVP-2.2.6: BellSoul rings the bell on pickup -- audible to
		// every priest on the map. Dispatches DP_OnBellRing (HUD +
		// state-machine subscribers) AND triggers the map-wide priest
		// fanout.
		//
		// Two propagation paths run in parallel:
		//   1. Zenith_PerceptionSystem::EmitSoundStimulus -- the normal
		//      hearing path. Priests within their own hearing_range_m
		//      (30 m default) pick it up here. Going through perception
		//      gives the priest's BridgePerceptionToBlackboard the
		//      stale-source-entity scrub it needs (the BellSoul entity
		//      is destroyed on pickup so the perception system would
		//      otherwise drop the stimulus on its next tick).
		//   2. DP_AI::NotifyAllPriestsOfInvestigatePos -- direct BB
		//      write to every priest in the scene, regardless of
		//      distance. This delivers the GDD's "audible from the
		//      entire map" intent; the perception system's
		//      min(emit_radius, agent_max_range) clamp would otherwise
		//      cap audibility at the 30 m hearing range no matter how
		//      loud the bell.
		if (m_strSpecialBehaviour == "rings_bell_on_pickup")
		{
			DP_OnBellRing xEvt;
			xEvt.m_xVillager = xVillager;
			xEvt.m_xBellSoul = m_xParentEntity.GetEntityID();
			xEvt.m_xPosition = xMyPos;
			Zenith_EventDispatcher::Get().Dispatch(xEvt);
			// Perception stimulus: serves priests within hearing range.
			Zenith_PerceptionSystem::EmitSoundStimulus(
				xMyPos, 1.0f, 200.0f, m_xParentEntity.GetEntityID());
			// Direct BB fanout: serves every priest beyond hearing
			// range. Both paths together = truly map-wide.
			DP_AI::NotifyAllPriestsOfInvestigatePos(xMyPos);
		}
	}

	DP_ItemTag GetTag() const { return m_eTag; }
	void SetTag(DP_ItemTag eTag)
	{
		// Re-register if the tag changed after OnAwake (e.g. editor setter).
		if (m_eTag == eTag) return;
		DP_Items::Internal_UnregisterItemTag(m_xParentEntity.GetEntityID());
		m_eTag = eTag;
		DP_Items::Internal_RegisterItemTag(m_xParentEntity.GetEntityID(), m_eTag);
		// Re-tint with the new tag's colour. Reset the guard so the materials
		// are re-set even if a previous tag's tint was already applied.
		m_bTintApplied = false;
		ApplyTagTint();
		// MVP-2.2: re-resolve the pickup-channel duration from the
		// reagent registry. The OnAwake call resolved against
		// m_eTag=None (default); SetTag is the canonical "I'm actually
		// a BogWater now" moment.
		ResolveReagentChannelFromRegistry();
	}

private:
	// Map the item's tag to a tint colour and overwrite every material slot on
	// the model with the tinted variant. No-op if the entity has no model
	// component, no model instance, or the tag is None. Idempotent — once the
	// tint is applied, m_bTintApplied prevents OnStart from re-tinting (and
	// thus re-tinting an already-tinted material into a doubly-tinted variant).
	void ApplyTagTint()
	{
		if (m_bTintApplied) return;
		if (m_eTag == DP_ItemTag::None) return;
		if (!m_xParentEntity.HasComponent<Zenith_ModelComponent>()) return;
		Zenith_ModelComponent& xModel = m_xParentEntity.GetComponent<Zenith_ModelComponent>();
		Flux_ModelInstance* pxModelInstance = xModel.GetModelInstance();
		if (!pxModelInstance) return;
		const uint32_t uNumMaterials = pxModelInstance->GetNumMaterials();
		if (uNumMaterials == 0) return;

		Zenith_Maths::Vector3 xRgb(1.0f, 1.0f, 1.0f);
		const char* szLabel = "Tint";
		switch (m_eTag)
		{
			case DP_ItemTag::Iron:        xRgb = Zenith_Maths::Vector3(0.5f, 0.5f, 0.55f); szLabel = "TintIron";        break;
			case DP_ItemTag::Key:         xRgb = Zenith_Maths::Vector3(1.0f, 0.85f, 0.2f); szLabel = "TintKey";         break;
			case DP_ItemTag::SkeletonKey: xRgb = Zenith_Maths::Vector3(0.7f, 0.3f, 0.9f);  szLabel = "TintSkeletonKey"; break;
			// MVP-2.3 forge additions
			case DP_ItemTag::Wood:        xRgb = Zenith_Maths::Vector3(0.55f, 0.35f, 0.15f); szLabel = "TintWood";        break;
			case DP_ItemTag::Spike:       xRgb = Zenith_Maths::Vector3(0.85f, 0.85f, 0.9f);  szLabel = "TintSpike";       break;
			// MVP-2.2 reagent tints. Read tint_rgb from DP_Reagents at
			// OnAwake instead of hardcoding; but compile-time enum case
			// labels need a static colour fallback. We keep the
			// per-tag tints here as a backup AND let
			// ResolveReagentChannelFromRegistry use the live tint_rgb
			// from the JSON if it diverges. The JSON values match
			// these constants today.
			case DP_ItemTag::Caul:        xRgb = Zenith_Maths::Vector3(0.95f, 0.92f, 0.85f); szLabel = "TintCaul";        break;
			case DP_ItemTag::HareTongue:  xRgb = Zenith_Maths::Vector3(0.65f, 0.20f, 0.18f); szLabel = "TintHareTongue";  break;
			case DP_ItemTag::BogWater:    xRgb = Zenith_Maths::Vector3(0.20f, 0.30f, 0.25f); szLabel = "TintBogWater";    break;
			case DP_ItemTag::BurialCoin:  xRgb = Zenith_Maths::Vector3(0.70f, 0.60f, 0.30f); szLabel = "TintBurialCoin";  break;
			case DP_ItemTag::BellSoul:    xRgb = Zenith_Maths::Vector3(0.85f, 0.75f, 0.45f); szLabel = "TintBellSoul";    break;
			case DP_ItemTag::Objective1:
			case DP_ItemTag::Objective2:
			case DP_ItemTag::Objective3:
			case DP_ItemTag::Objective4:
			case DP_ItemTag::Objective5:  xRgb = Zenith_Maths::Vector3(0.95f, 0.15f, 0.15f); szLabel = "TintObjective"; break;
			default: return;
		}

		for (uint32_t u = 0; u < uNumMaterials; ++u)
		{
			Zenith_MaterialAsset* pxBase = pxModelInstance->GetMaterial(u);
			Zenith_MaterialAsset* pxTint = DPMaterials::GetOrCreateColouredVariant(pxBase, xRgb, szLabel);
			if (pxTint) pxModelInstance->SetMaterial(u, pxTint);
		}
		m_bTintApplied = true;
	}

public:
	// MVP-1.4.5: external setter for the post-drop pickup cooldown.
	// Called by DPPlayerController_Behaviour::HandleDropItem so the
	// dropped item doesn't immediately re-pick-up from the villager's
	// foot position. 0.5 s is long enough to step out of the 1.5 m
	// pickup radius at 4 m/s walk speed (which covers 2 m in 0.5 s).
	void BeginPostDropCooldown()
	{
		m_fPostDropCooldownSec = 0.5f;
		// MVP-2.2.4: BogWater + post-MVP reagents with
		// special_behaviour="evaporates_after_drop" start a destroy
		// timer when dropped. The duration is loaded from
		// Reagents.json (8.0s for BogWater). Once expired the entity
		// destroys itself in OnUpdate.
		if (m_strSpecialBehaviour == "evaporates_after_drop"
			&& m_fEvaporateDuration > 0.0f)
		{
			m_fEvaporateRemaining = m_fEvaporateDuration;
		}
	}

#ifdef ZENITH_INPUT_SIMULATOR
	float GetPostDropCooldownForTest() const { return m_fPostDropCooldownSec; }

	// MVP-2.2 test accessors. ChannelDuration is the per-tag value
	// resolved at OnAwake from DP_Reagents; ChannelRemaining tracks
	// the in-flight countdown when a villager is in pickup range.
	float           GetPickupChannelDurationForTest() const { return m_fPickupChannelDuration; }
	float           GetChannelRemainingForTest() const { return m_fChannelRemaining; }
	Zenith_EntityID GetChannelingVillagerForTest() const { return m_xChannelingVillager; }
	// MVP-2.2.4 evaporate test accessors.
	float           GetEvaporateDurationForTest() const { return m_fEvaporateDuration; }
	float           GetEvaporateRemainingForTest() const { return m_fEvaporateRemaining; }
	const std::string& GetSpecialBehaviourForTest() const { return m_strSpecialBehaviour; }
#endif

private:
	// MVP-2.2.1/4: look up the item's reagent properties at OnAwake
	// and cache the pickup-channel duration + special-behaviour
	// metadata. Non-reagent tags (Iron, Key, Wood, Spike,
	// Objective1..5) silently default to no channel + no special
	// behaviour -- TryGet returns nullptr for them.
	void ResolveReagentChannelFromRegistry()
	{
		const char* szTagName = DP_ItemTagToString(m_eTag);
		const DP_Reagents::Reagent* pxR = DP_Reagents::TryGet(szTagName);
		if (pxR != nullptr)
		{
			m_fPickupChannelDuration = pxR->pickup_channel_s;
			m_strSpecialBehaviour    = pxR->special_behaviour;
			m_fEvaporateDuration     = pxR->evaporate_duration_s;
		}
		else
		{
			m_fPickupChannelDuration = 0.0f;
			m_strSpecialBehaviour.clear();
			m_fEvaporateDuration     = 0.0f;
		}
	}

	DP_ItemTag m_eTag = DP_ItemTag::None;
	float      m_fPickupRadius = 1.5f;
	bool       m_bTintApplied = false;
	float      m_fPostDropCooldownSec = 0.0f;
	// MVP-2.2.2 pickup-channel state. Duration is loaded from
	// DP_Reagents at OnAwake; >0 means the item is a reagent and
	// requires staying in range for that many seconds before pickup
	// fires. Remaining counts down per frame while a villager is in
	// range. Channeling villager identity is tracked so a different
	// villager wandering into range mid-channel restarts cleanly.
	float           m_fPickupChannelDuration = 0.0f;
	float           m_fChannelRemaining      = 0.0f;
	Zenith_EntityID m_xChannelingVillager;
	// MVP-2.2.4 evaporate state. Set when BeginPostDropCooldown is
	// called on an item with special_behaviour=evaporates_after_drop
	// (currently only BogWater). When > 0, the timer counts down in
	// OnUpdate; on 0, the entity is destroyed.
	std::string m_strSpecialBehaviour;
	float       m_fEvaporateDuration  = 0.0f;
	float       m_fEvaporateRemaining = 0.0f;
};
