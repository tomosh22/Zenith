#pragma once
#include "Core/Zenith_Engine.h"
/**
 * DPItemBase_Component - tagged pickup item.
 *
 * Each item entity carries a DP_ItemTag. On OnAwake the tag is registered
 * with DP_Items::Internal_RegisterItemTag so DP_Items::GetItemTag /
 * FindItemByTag work without per-frame component scanning. Pickup is
 * distance-based (no trigger semantics in Zenith): each frame, if the
 * possessed villager is within m_fPickupRadius, the item is picked up.
 */

#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_GraphComponent.h"
#include "Scripting/Zenith_BehaviourGraph.h"
#include "Scripting/Zenith_GraphBlackboard.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "DataStream/Zenith_DataStream.h"
#include "Maths/Zenith_Maths.h"
#include "Flux/Flux_ModelInstance.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"

#include "Source/PublicInterfaces.h"
#include "Source/DPMaterials.h"
#include "Source/DP_Reagents.h"

#include <cstring>

class DPItemBase_Component ZENITH_FINAL
{
public:
	// W3 conversion: the pickup DECISIONS (evaporate countdown, post-drop
	// cooldown gate, possessed/held/range/child gates, the reagent channel
	// state machine, the commit + bell sequence) live on this graph; the
	// component keeps the SYSTEMS (side-table registration, tint, transform
	// reads, the reagent-registry resolve) and stages per-frame facts.
	// Mutable decision state (cooldown, channel, evaporate countdown) lives
	// on the blackboard; reagent CONFIG (channel duration, special
	// behaviour, evaporate duration) is resolved C++-side and mirrored.
	static constexpr const char* kszGraphAsset = "game:Graphs/DP_Item.bgraph";

	DPItemBase_Component() = delete;
	DPItemBase_Component(Zenith_Entity& xParentEntity)
		: m_xParentEntity(xParentEntity)
	{}

	void OnAwake()
	{
		DP_Items::Internal_RegisterItemTag(m_xParentEntity.GetEntityID(), m_eTag);
		ApplyTagTint();
		// Self-attach the decisions graph BEFORE the reagent resolve so the
		// config mirror lands on the blackboard (idempotent on re-entry).
		EnsureGraphAttached();
		ResolveReagentChannelFromRegistry();
	}

	// Idempotent self-attach; also invoked from SetTag so a test's
	// AddComponent + SetTag in the same pre-awake frame lands the config
	// mirror immediately (the pre-W3 component worked immediately too).
	void EnsureGraphAttached()
	{
		Zenith_GraphComponent* pxGraphs = m_xParentEntity.TryGetComponent<Zenith_GraphComponent>();
		if (pxGraphs == nullptr)
		{
			pxGraphs = &m_xParentEntity.AddComponent<Zenith_GraphComponent>();
		}
		for (u_int u = 0; u < pxGraphs->GetGraphCount(); ++u)
		{
			if (std::strcmp(pxGraphs->GetGraphAssetPathAt(u), kszGraphAsset) == 0)
			{
				return;
			}
		}
		pxGraphs->AddGraphByAssetPath(kszGraphAsset);
	}

	void OnStart()
	{
		// Re-apply on OnStart so runtime-spawned items (where the model component
		// is added immediately before the item component's OnAwake runs) catch up
		// if model materials weren't ready at OnAwake.
		ApplyTagTint();
	}

	void OnDestroy()
	{
		DP_Items::Internal_UnregisterItemTag(m_xParentEntity.GetEntityID());
	}

	// Component contract: version-only payload. Item identity (tag) is stamped
	// at runtime by the spawner via SetTag; nothing persists.
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

	void OnUpdate(const float fDt)
	{
		// W3: the early-return decision chain (evaporate -> cooldown ->
		// possessed/held/range/child gates -> channel state machine ->
		// commit + bell) lives on DP_Item.bgraph. Stage this frame's facts,
		// then fire "ItemTick" with dt as the payload. The graph's chains
		// reproduce the returns via chain-reuse events (ItemGate2/3,
		// ItemCommit) - see BuildGraph_DPItem.
		Zenith_GraphComponent* pxGraphs = m_xParentEntity.TryGetComponent<Zenith_GraphComponent>();
		Zenith_BehaviourGraph* pxGraph = FindItemGraph();
		if (pxGraphs == nullptr || pxGraph == nullptr) return;

		const Zenith_EntityID xVillager = DP_Player::GetPossessedVillager();
		const bool bPossessedValid = xVillager.IsValid();
		const bool bHandsEmpty = bPossessedValid
			&& DP_Player::GetHeldItemTag(xVillager) == DP_ItemTag::None;

		// XZ-only squared-distance range fact (strict >, radius 1.5
		// hardcoded - the retired step-4 semantics).
		bool bInRange = false;
		if (bPossessedValid)
		{
			Zenith_Entity xV = g_xEngine.Scenes().ResolveEntity(xVillager);
			if (xV.IsValid())
			{
				if (Zenith_TransformComponent* pxVTransform = xV.TryGetComponent<Zenith_TransformComponent>())
				{
					Zenith_Maths::Vector3 xVPos;
					pxVTransform->GetPosition(xVPos);
					const Zenith_Maths::Vector3 xMyPos =
						DP_Items::GetItemWorldPos(m_xParentEntity.GetEntityID());
					const float fDx = xMyPos.x - xVPos.x;
					const float fDz = xMyPos.z - xVPos.z;
					bInRange = !(fDx * fDx + fDz * fDz > m_fPickupRadius * m_fPickupRadius);
				}
			}
		}

		Zenith_GraphBlackboard& xBB = pxGraph->GetBlackboard();
		Zenith_PropertyValue xValue;
		xValue.SetBool(bPossessedValid);
		xBB.SetValue("possessedValid", xValue);
		xValue.SetBool(bHandsEmpty);
		xBB.SetValue("handsEmpty", xValue);
		xValue.SetBool(bInRange);
		xBB.SetValue("inRange", xValue);
		xValue.SetPackedEntityID(xVillager.GetPacked());
		xBB.SetValue("possessedVillager", xValue);

		Zenith_PropertyValue xDtPayload;
		xDtPayload.SetFloat(fDt);
		pxGraphs->FireCustomEvent("ItemTick", &xDtPayload);
	}

	DP_ItemTag GetTag() const { return m_eTag; }
	void SetTag(DP_ItemTag eTag)
	{
		// Re-register if the tag changed after OnAwake (e.g. editor setter).
		if (m_eTag == eTag) return;
		EnsureGraphAttached();
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
		Zenith_ModelComponent* pxModel = m_xParentEntity.TryGetComponent<Zenith_ModelComponent>();
		if (pxModel == nullptr) return;
		Flux_ModelInstance* pxModelInstance = pxModel->GetModelInstance();
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
	// Called by DPPlayerController_Component::HandleDropItem (via
	// DP_Items::BeginPostDropCooldownForItem) so the dropped item doesn't
	// immediately re-pick-up from the villager's foot position. 0.5 s is
	// long enough to step out of the 1.5 m pickup radius at 4 m/s walk
	// speed (which covers 2 m in 0.5 s).
	void BeginPostDropCooldown()
	{
		WriteBBFloat("postDropCooldown", 0.5f);
		// MVP-2.2.4: BogWater + post-MVP reagents with
		// special_behaviour="evaporates_after_drop" start a destroy
		// timer when dropped. The duration is loaded from
		// Reagents.json (8.0s for BogWater). Once expired the entity
		// destroys itself from the graph's evaporate chain.
		if (m_strSpecialBehaviour == "evaporates_after_drop"
			&& m_fEvaporateDuration > 0.0f)
		{
			WriteBBFloat("evaporateRemaining", m_fEvaporateDuration);
		}
	}

	// ---- W3 graph plumbing (the DPDoor shim pattern) ----
	Zenith_BehaviourGraph* FindItemGraph() const
	{
		Zenith_GraphComponent* pxGraphs = m_xParentEntity.TryGetComponent<Zenith_GraphComponent>();
		if (pxGraphs == nullptr) return nullptr;
		for (u_int u = 0; u < pxGraphs->GetGraphCount(); ++u)
		{
			if (std::strcmp(pxGraphs->GetGraphAssetPathAt(u), kszGraphAsset) == 0)
			{
				return pxGraphs->GetGraphAt(u);
			}
		}
		return nullptr;
	}
	float ReadBBFloat(const char* szVar, float fDefault) const
	{
		Zenith_BehaviourGraph* pxGraph = FindItemGraph();
		return pxGraph ? pxGraph->GetBlackboard().GetFloat(szVar, fDefault) : fDefault;
	}
	void WriteBBFloat(const char* szVar, float fValue)
	{
		if (Zenith_BehaviourGraph* pxGraph = FindItemGraph())
		{
			Zenith_PropertyValue xValue;
			xValue.SetFloat(fValue);
			pxGraph->GetBlackboard().SetValue(szVar, xValue);
		}
	}

#ifdef ZENITH_INPUT_SIMULATOR
	float GetPostDropCooldownForTest() const { return ReadBBFloat("postDropCooldown", 0.0f); }

	// MVP-2.2 test accessors. ChannelDuration is the per-tag value
	// resolved at OnAwake from DP_Reagents; the in-flight countdown +
	// channeling-villager identity live on the graph blackboard (W3).
	float           GetPickupChannelDurationForTest() const { return m_fPickupChannelDuration; }
	float           GetChannelRemainingForTest() const { return ReadBBFloat("channelRemaining", 0.0f); }
	Zenith_EntityID GetChannelingVillagerForTest() const
	{
		Zenith_BehaviourGraph* pxGraph = FindItemGraph();
		if (pxGraph == nullptr) return INVALID_ENTITY_ID;
		return Zenith_EntityID::FromPacked(
			pxGraph->GetBlackboard().GetPackedEntityID("channelVillager", 0));
	}
	// MVP-2.2.4 evaporate test accessors.
	float           GetEvaporateDurationForTest() const { return m_fEvaporateDuration; }
	float           GetEvaporateRemainingForTest() const { return ReadBBFloat("evaporateRemaining", 0.0f); }
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
		// Mirror the config the graph's decision chains read (the members
		// stay the systems-side source of truth for BeginPostDropCooldown +
		// the test accessors that report config, not state).
		if (Zenith_BehaviourGraph* pxGraph = FindItemGraph())
		{
			Zenith_GraphBlackboard& xBB = pxGraph->GetBlackboard();
			Zenith_PropertyValue xValue;
			xValue.SetFloat(m_fPickupChannelDuration);
			xBB.SetValue("channelDuration", xValue);
			xValue.SetInt32((int32_t)m_eTag);
			xBB.SetValue("tag", xValue);
			xValue.SetString(m_strSpecialBehaviour.c_str());
			xBB.SetValue("specialBehaviour", xValue);
		}
	}

	Zenith_Entity m_xParentEntity;

	DP_ItemTag m_eTag = DP_ItemTag::None;
	float      m_fPickupRadius = 1.5f;
	bool       m_bTintApplied = false;
	// MVP-2.2 reagent CONFIG (resolved from DP_Reagents at OnAwake/SetTag,
	// mirrored onto the graph blackboard). The MUTABLE countdown state
	// (postDropCooldown / channelRemaining / channelVillager /
	// evaporateRemaining) lives on DP_Item.bgraph's blackboard since W3.
	float       m_fPickupChannelDuration = 0.0f;
	std::string m_strSpecialBehaviour;
	float       m_fEvaporateDuration  = 0.0f;
};
