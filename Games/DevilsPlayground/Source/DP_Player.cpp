#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#include "DP_Player.h"
#include "DP_Items.h"
#include "DP_AI.h"
#include "DP_Query.h"
#include "DPCommonTypes.h"
#include "DP_Tuning.h"

#include "ZenithECS/Zenith_Entity.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_EventSystem.h"
#include "EntityComponent/Components/Zenith_AIAgentComponent.h"
#include "AI/BehaviorTree/Zenith_Blackboard.h"

#include "../Components/DPVillager_Component.h"
#include "../Components/Priest_Component.h"
#include "../Components/DPPlayerController_Component.h"

#include <cmath>

namespace
{
	// Resolves a villager handle to its world position. Returns false
	// if the entity has no transform / no scene-data binding (e.g.,
	// passed a stale handle after the villager was destroyed).
	bool TryGetVillagerPos(Zenith_EntityID xId, Zenith_Maths::Vector3& xOut)
	{
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xId);
		if (pxScene == nullptr) return false;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xId);
		if (!xEnt.IsValid()) return false;
		if (!xEnt.HasComponent<Zenith_TransformComponent>()) return false;
		xEnt.GetComponent<Zenith_TransformComponent>().GetPosition(xOut);
		return true;
	}
}

// Internal commit helper used by both the immediate-possession path and
// the channel-completion path in TryVoluntaryPossessSwitch. Promoted from
// an anon-namespace free function to a private static member of the
// controller so it can write the now-private possession/anchor/scent
// state (an anon-namespace free function can't be befriended). Its sole
// callers, DP_Player::TryVoluntaryPossessSwitch + TickChannel, are in the
// controller's friend list and so can call this private static member.
void DPPlayerController_Component::CommitVoluntaryPossession(
	DPPlayerController_Component& xCtrl,
	Zenith_EntityID xId,
	const Zenith_Maths::Vector3& xNewPos,
	bool bGotNewPos)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(),
		"CommitVoluntaryPossession must be called from main thread");
	xCtrl.m_xPossessedVillager = xId;
	xCtrl.m_fPossessionCooldownSec =
		DP_Tuning::Get<float>("possession.cooldown_after_voluntary_switch_s");

	if (bGotNewPos)
	{
		xCtrl.m_xPossessionAnchor = xNewPos;
		xCtrl.m_bHasPossessionAnchor = true;
	}
	else
	{
		xCtrl.m_bHasPossessionAnchor = false;
	}

	// MVP-1.6 scent: only successful possession bumps. Tuning.json's
	// scent comment explicitly says channel-interrupted switches
	// produce no scent.
	if (xId.IsValid())
	{
		const float fPerPossession =
			DP_Tuning::Get<float>("possession.demon_scent_per_possession");
		const float fMaxScent =
			DP_Tuning::Get<float>("possession.demon_scent_max");
		xCtrl.BumpDemonScent(xId, fPerPossession, fMaxScent);
	}
}

namespace DP_Player
{
	Zenith_EntityID GetPossessedVillager()
	{
		const DPPlayerController_Component* pxCtrl = DPPlayerController_Component::Instance();
		if (pxCtrl == nullptr) return INVALID_ENTITY_ID;
		return pxCtrl->m_xPossessedVillager;
	}

	void SetPossessedVillager(Zenith_EntityID xId)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_Player::SetPossessedVillager must be called from main thread");
		DPPlayerController_Component* pxCtrl = DPPlayerController_Component::Instance();
		if (pxCtrl == nullptr) return;
		const Zenith_EntityID xPrior = pxCtrl->m_xPossessedVillager;
		pxCtrl->m_xPossessedVillager = xId;
		if (xId.IsValid() && TryGetVillagerPos(xId, pxCtrl->m_xPossessionAnchor))
		{
			pxCtrl->m_bHasPossessionAnchor = true;
		}
		else
		{
			pxCtrl->m_bHasPossessionAnchor = false;
		}
		if (xPrior != xId)
		{
			Zenith_EventDispatcher::Get().Dispatch(
				DP_OnPossessionChanged{ xPrior, xId });
		}
	}

	bool TryVoluntaryPossessSwitch(Zenith_EntityID xId)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_Player::TryVoluntaryPossessSwitch must be called from main thread");
		DPPlayerController_Component* pxCtrl = DPPlayerController_Component::Instance();
		if (pxCtrl == nullptr) return false;

		// Idempotent re-click.
		if (xId.IsValid()
			&& pxCtrl->m_xPossessedVillager.IsValid()
			&& xId.m_uIndex == pxCtrl->m_xPossessedVillager.m_uIndex
			&& xId.m_uGeneration == pxCtrl->m_xPossessedVillager.m_uGeneration)
		{
			return true;
		}

		// MVP-2.1.1: refuse a new switch attempt while a Devout channel
		// is in progress.
		if (pxCtrl->m_bChannelActive)
		{
			if (xId.IsValid()
				&& pxCtrl->m_xChannelTarget.IsValid()
				&& xId.m_uIndex == pxCtrl->m_xChannelTarget.m_uIndex
				&& xId.m_uGeneration == pxCtrl->m_xChannelTarget.m_uGeneration)
			{
				return true;
			}
			return false;
		}

		// Cooldown gate.
		if (pxCtrl->m_fPossessionCooldownSec > 0.0f)
		{
			return false;
		}

		// Resolve the candidate villager component once and share it across
		// the possessability check + Devout-channel branch below.
		DPVillager_Component* pxCandidateVillager = nullptr;
		if (xId.IsValid())
		{
			Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xId);
			if (pxScene != nullptr)
			{
				Zenith_Entity xEnt = pxScene->TryGetEntity(xId);
				if (xEnt.IsValid())
				{
					pxCandidateVillager = xEnt.TryGetComponent<DPVillager_Component>();
				}
			}
		}

		// MVP-1.4.1-3: state gate. Refuse fainted (still recovering) or
		// dead villagers.
		if (pxCandidateVillager != nullptr && !pxCandidateVillager->IsPossessable())
		{
			return false;
		}

		// MVP-1.8: range gate.
		Zenith_Maths::Vector3 xNewPos(0.0f);
		const bool bGotNewPos = xId.IsValid() && TryGetVillagerPos(xId, xNewPos);
		if (pxCtrl->m_bHasPossessionAnchor && bGotNewPos)
		{
			const float fDx = xNewPos.x - pxCtrl->m_xPossessionAnchor.x;
			const float fDz = xNewPos.z - pxCtrl->m_xPossessionAnchor.z;
			const float fDist = std::sqrt(fDx * fDx + fDz * fDz);
			const float fMaxRange =
				DP_Tuning::Get<float>("possession.range_from_anchor_m");
			if (fDist > fMaxRange)
			{
				return false;
			}
		}

		// MVP-2.1.1: Devout channel dispatch.
		float fChannelSec = DP_Tuning::Get<float>("possession.channel_default_s");
		if (pxCandidateVillager != nullptr && pxCandidateVillager->GetArchetypeId() == "Devout")
		{
			fChannelSec = DP_Tuning::Get<float>("possession.channel_devout_s");
		}

		if (fChannelSec > 0.0f)
		{
			pxCtrl->m_xChannelTarget    = xId;
			pxCtrl->m_fChannelRemaining = fChannelSec;
			pxCtrl->m_bChannelActive    = true;
			return true;
		}

		DPPlayerController_Component::CommitVoluntaryPossession(
			*pxCtrl, xId, xNewPos, bGotNewPos);
		return true;
	}

	bool IsChanneling()
	{
		const DPPlayerController_Component* pxCtrl = DPPlayerController_Component::Instance();
		if (pxCtrl == nullptr) return false;
		return pxCtrl->m_bChannelActive;
	}

	Zenith_EntityID GetChannelTarget()
	{
		const DPPlayerController_Component* pxCtrl = DPPlayerController_Component::Instance();
		if (pxCtrl == nullptr) return INVALID_ENTITY_ID;
		return pxCtrl->m_xChannelTarget;
	}

	float GetChannelRemaining()
	{
		const DPPlayerController_Component* pxCtrl = DPPlayerController_Component::Instance();
		if (pxCtrl == nullptr) return 0.0f;
		return pxCtrl->m_fChannelRemaining;
	}

	void InterruptChannel()
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_Player::InterruptChannel must be called from main thread");
		DPPlayerController_Component* pxCtrl = DPPlayerController_Component::Instance();
		if (pxCtrl == nullptr || !pxCtrl->m_bChannelActive) return;
		pxCtrl->m_bChannelActive    = false;
		pxCtrl->m_xChannelTarget    = INVALID_ENTITY_ID;
		pxCtrl->m_fChannelRemaining = 0.0f;
	}

	void TickChannel(float fDt)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_Player::TickChannel must be called from main thread");
		DPPlayerController_Component* pxCtrl = DPPlayerController_Component::Instance();
		if (pxCtrl == nullptr || !pxCtrl->m_bChannelActive) return;

		// MVP-2.1.2 interrupt check: any priest within
		// channel_interrupt_distance_m of the channel target cancels
		// the channel before the timer runs down.
		Zenith_Maths::Vector3 xTargetPos(0.0f);
		const bool bGotTargetPos =
			pxCtrl->m_xChannelTarget.IsValid()
			&& TryGetVillagerPos(pxCtrl->m_xChannelTarget, xTargetPos);
		if (bGotTargetPos)
		{
			const float fInterruptDist =
				DP_Tuning::Get<float>("possession.channel_interrupt_distance_m");
			bool bInterrupted = false;
			DP_Query::ForEachComponentInActiveScene<Priest_Component>(
				[&bInterrupted, &xTargetPos, fInterruptDist]
				(Zenith_EntityID xPriestId, Priest_Component&)
				{
					if (bInterrupted) return;
					Zenith_Maths::Vector3 xPriestPos;
					if (!TryGetVillagerPos(xPriestId, xPriestPos)) return;
					const float fDx = xPriestPos.x - xTargetPos.x;
					const float fDz = xPriestPos.z - xTargetPos.z;
					const float fDistSq = fDx * fDx + fDz * fDz;
					if (fDistSq < fInterruptDist * fInterruptDist)
					{
						bInterrupted = true;
					}
				});
			if (bInterrupted)
			{
				InterruptChannel();
				return;
			}
		}

		pxCtrl->m_fChannelRemaining -= fDt;
		if (pxCtrl->m_fChannelRemaining <= 0.0f)
		{
			Zenith_EntityID xTarget = pxCtrl->m_xChannelTarget;
			pxCtrl->m_bChannelActive    = false;
			pxCtrl->m_xChannelTarget    = INVALID_ENTITY_ID;
			pxCtrl->m_fChannelRemaining = 0.0f;
			Zenith_Maths::Vector3 xCommitPos(0.0f);
			const bool bGotPos =
				xTarget.IsValid() && TryGetVillagerPos(xTarget, xCommitPos);
			DPPlayerController_Component::CommitVoluntaryPossession(
				*pxCtrl, xTarget, xCommitPos, bGotPos);
		}
	}

	void TickPossessionCooldown(float fDt)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_Player::TickPossessionCooldown must be called from main thread");
		DPPlayerController_Component* pxCtrl = DPPlayerController_Component::Instance();
		if (pxCtrl == nullptr || pxCtrl->m_fPossessionCooldownSec <= 0.0f) return;
		pxCtrl->m_fPossessionCooldownSec -= fDt;
		if (pxCtrl->m_fPossessionCooldownSec < 0.0f)
		{
			pxCtrl->m_fPossessionCooldownSec = 0.0f;
		}
	}

	float GetPossessionCooldownRemaining()
	{
		const DPPlayerController_Component* pxCtrl = DPPlayerController_Component::Instance();
		if (pxCtrl == nullptr) return 0.0f;
		return pxCtrl->m_fPossessionCooldownSec;
	}

	float GetDemonScent(Zenith_EntityID xVillager)
	{
		if (!xVillager.IsValid()) return 0.0f;
		DPPlayerController_Component* pxCtrl = DPPlayerController_Component::Instance();
		if (pxCtrl == nullptr) return 0.0f;
		return pxCtrl->GetDemonScent(xVillager);
	}

	void TickDemonScent(float fDt)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_Player::TickDemonScent must be called from main thread");
		DPPlayerController_Component* pxCtrl = DPPlayerController_Component::Instance();
		if (pxCtrl == nullptr || !pxCtrl->HasDemonScentEntries()) return;
		const float fDecayPerSec =
			DP_Tuning::Get<float>("possession.demon_scent_decay_per_s");
		pxCtrl->DecayDemonScent(fDecayPerSec, fDt);
	}

	void WriteHighestScentToBlackboard()
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_Player::WriteHighestScentToBlackboard must be called from main thread");
		Zenith_EntityID xHighestId = INVALID_ENTITY_ID;
		float fHighestScent = 0.0f;
		DPPlayerController_Component* pxCtrl = DPPlayerController_Component::Instance();
		if (pxCtrl != nullptr)
		{
			pxCtrl->ForEachDemonScentEntry(
				[&xHighestId, &fHighestScent](Zenith_EntityID xId, float fScent)
				{
					if (fScent > fHighestScent)
					{
						fHighestScent = fScent;
						xHighestId = xId;
					}
				});
		}

		// B2: skip the expensive all-scenes x all-AI-agents blackboard write
		// unless the highest-scent target changed, or a reset / scene-load
		// flagged the cache dirty. INVALID is a legitimate value, so an
		// explicit dirty flag (not an INVALID sentinel) is required to force
		// the first write after a reset. Cache is updated up-front -- the
		// write below uses the same xHighestId regardless of order.
		if (pxCtrl != nullptr)
		{
			if (!pxCtrl->m_bHighScentBlackboardDirty &&
				xHighestId == pxCtrl->m_xLastWrittenHighScent)
			{
				return;
			}
			pxCtrl->m_xLastWrittenHighScent     = xHighestId;
			pxCtrl->m_bHighScentBlackboardDirty = false;
		}

		const uint32_t uSlotCount = g_xEngine.Scenes().GetSceneSlotCount();
		for (uint32_t uSlot = 0; uSlot < uSlotCount; ++uSlot)
		{
			Zenith_SceneData* pxScene =
				g_xEngine.Scenes().GetLoadedSceneDataAtSlot(uSlot);
			if (pxScene == nullptr) continue;
			pxScene->Query<Zenith_AIAgentComponent>().ForEach(
				[xHighestId]
				(Zenith_EntityID, Zenith_AIAgentComponent& xAgent)
				{
					xAgent.GetBlackboard().SetEntityID(
						DP_AI::BB_KEY_HIGH_SCENT_TARGET, xHighestId);
				});
		}
	}

	DP_ItemTag GetHeldItemTag(Zenith_EntityID xVillager)
	{
		DPPlayerController_Component* pxCtrl = DPPlayerController_Component::Instance();
		if (pxCtrl == nullptr) return DP_ItemTag::None;
		return pxCtrl->GetHeldItemTag(xVillager);
	}

	Zenith_EntityID GetHeldItemEntity(Zenith_EntityID xVillager)
	{
		DPPlayerController_Component* pxCtrl = DPPlayerController_Component::Instance();
		if (pxCtrl == nullptr) return INVALID_ENTITY_ID;
		return pxCtrl->GetHeldItemEntity(xVillager);
	}

	void SetHeldItem(Zenith_EntityID xVillager, Zenith_EntityID xItem)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_Player::SetHeldItem must be called from main thread");
		DPPlayerController_Component* pxCtrl = DPPlayerController_Component::Instance();
		if (pxCtrl == nullptr) return;
		DPVillagerHeldRecord xRec;
		xRec.m_xItem = xItem;
		xRec.m_eTag  = DP_Items::GetItemTag(xItem);
		pxCtrl->SetHeldItemRecord(xVillager, xRec);
	}

	// SourceBugFixed: GameJam0 ADPVillager::RemoveHeldItem null-derefs when no
	// held item. This port guards with early-return inside the controller.
	void RemoveHeldItem(Zenith_EntityID xVillager)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_Player::RemoveHeldItem must be called from main thread");
		DPPlayerController_Component* pxCtrl = DPPlayerController_Component::Instance();
		if (pxCtrl == nullptr) return;
		pxCtrl->RemoveHeldItem(xVillager);
	}

	void ResetForNewRun()
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_Player::ResetForNewRun must be called from main thread");
		DPPlayerController_Component* pxCtrl = DPPlayerController_Component::Instance();
		if (pxCtrl == nullptr) return;
		// Scene-bound state lifetime: held-items + demon-scent tables are
		// owned by the controller and cleared by Clear() below. Possession,
		// cooldown, anchor + channel state are also controller-owned now
		// (2026-05-22 Phase 5.2 migration).
		pxCtrl->m_xHeldItems.Clear();
		pxCtrl->m_xDemonScent.Clear();
		// B2: force a fresh high-scent blackboard write next frame.
		pxCtrl->m_xLastWrittenHighScent     = INVALID_ENTITY_ID;
		pxCtrl->m_bHighScentBlackboardDirty = true;
		pxCtrl->m_xPossessedVillager     = INVALID_ENTITY_ID;
		pxCtrl->m_fPossessionCooldownSec = 0.0f;
		pxCtrl->m_xPossessionAnchor      = Zenith_Maths::Vector3(0.0f);
		pxCtrl->m_bHasPossessionAnchor   = false;
		pxCtrl->m_xChannelTarget         = INVALID_ENTITY_ID;
		pxCtrl->m_fChannelRemaining      = 0.0f;
		pxCtrl->m_bChannelActive         = false;
	}

	// ========================================================================
	// Cross-component villager forwarders. Resolve the villager component and
	// read the requested field. Moved here from Priest_Component /
	// DPItemBase / DPHUDController so those headers no
	// longer include DPVillager_Component.h (cross-component rule).
	// ========================================================================

	bool IsPossessedVillager(Zenith_EntityID xCandidate)
	{
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xCandidate);
		if (pxScene == nullptr) return false;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xCandidate);
		if (!xEnt.IsValid()) return false;
		DPVillager_Component* pxV = xEnt.TryGetComponent<DPVillager_Component>();
		return pxV != nullptr && pxV->IsPossessed();
	}

	bool IsBeggarVillager(Zenith_EntityID xCandidate)
	{
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xCandidate);
		if (pxScene == nullptr) return false;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xCandidate);
		if (!xEnt.IsValid()) return false;
		DPVillager_Component* pxV = xEnt.TryGetComponent<DPVillager_Component>();
		return pxV != nullptr && pxV->GetArchetypeId() == "Beggar";
	}

	bool IsChildVillagerWithToolTag(Zenith_EntityID xVillager, DP_ItemTag eTag)
	{
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xVillager);
		if (pxScene == nullptr) return false;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xVillager);
		if (!xEnt.IsValid()) return false;
		DPVillager_Component* pxV = xEnt.TryGetComponent<DPVillager_Component>();
		return pxV != nullptr && pxV->GetArchetypeId() == "Child" && DP_IsToolTag(eTag);
	}

	float GetVillagerRemainingLife(Zenith_EntityID xVillager)
	{
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xVillager);
		if (pxScene == nullptr) return 0.0f;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xVillager);
		if (!xEnt.IsValid()) return 0.0f;
		DPVillager_Component* pxV = xEnt.TryGetComponent<DPVillager_Component>();
		return pxV ? pxV->GetRemainingLife() : 0.0f;
	}

	float GetVillagerMaxLife(Zenith_EntityID xVillager)
	{
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xVillager);
		if (pxScene == nullptr) return 0.0f;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xVillager);
		if (!xEnt.IsValid()) return 0.0f;
		DPVillager_Component* pxV = xEnt.TryGetComponent<DPVillager_Component>();
		return pxV ? pxV->GetMaxLife() : 0.0f;
	}

	const char* GetVillagerArchetypeId(Zenith_EntityID xVillager)
	{
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xVillager);
		if (pxScene == nullptr) return "";
		Zenith_Entity xEnt = pxScene->TryGetEntity(xVillager);
		if (!xEnt.IsValid()) return "";
		DPVillager_Component* pxV = xEnt.TryGetComponent<DPVillager_Component>();
		return pxV ? pxV->GetArchetypeId().c_str() : "";
	}

	bool IsVillagerSprintingNow(Zenith_EntityID xVillager)
	{
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xVillager);
		if (pxScene == nullptr) return false;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xVillager);
		if (!xEnt.IsValid()) return false;
		DPVillager_Component* pxV = xEnt.TryGetComponent<DPVillager_Component>();
		return pxV != nullptr && pxV->IsSprintingNow();
	}

	bool IsVillagerWalkQuietNow(Zenith_EntityID xVillager)
	{
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xVillager);
		if (pxScene == nullptr) return false;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xVillager);
		if (!xEnt.IsValid()) return false;
		DPVillager_Component* pxV = xEnt.TryGetComponent<DPVillager_Component>();
		return pxV != nullptr && pxV->IsWalkQuietNow();
	}

	void CountVillagers(int& iOutAlive, int& iOutTotal)
	{
		iOutAlive = 0;
		iOutTotal = 0;
		DP_Query::ForEachComponentInActiveScene<DPVillager_Component>(
			[&iOutAlive, &iOutTotal](Zenith_EntityID, DPVillager_Component& xV)
			{
				++iOutTotal;
				if (xV.GetRemainingLife() > 0.0f) ++iOutAlive;
			});
	}

	void ForEachVillagerInActiveScene(void (*pfnCallback)(Zenith_EntityID, void*),
	                                  void* pUserData)
	{
		if (pfnCallback == nullptr) return;
		DP_Query::ForEachComponentInActiveScene<DPVillager_Component>(
			[pfnCallback, pUserData](Zenith_EntityID xId, DPVillager_Component&)
			{
				pfnCallback(xId, pUserData);
			});
	}
}
