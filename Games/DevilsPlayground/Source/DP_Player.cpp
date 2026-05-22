#include "Zenith.h"

#include "DP_Player.h"
#include "DP_Items.h"
#include "DP_AI.h"
#include "DP_Query.h"
#include "DPCommonTypes.h"
#include "DP_Tuning.h"

#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Zenith_EventSystem.h"
#include "AI/Components/Zenith_AIAgentComponent.h"
#include "AI/BehaviorTree/Zenith_Blackboard.h"

#include "../Components/DPVillager_Behaviour.h"
#include "../Components/Priest_Behaviour.h"
#include "../Components/DPPlayerController_Behaviour.h"

namespace
{
	// ---- DP_Player state (B2 fills in) ----
	Zenith_EntityID g_xPossessedVillager = INVALID_ENTITY_ID;

	// MVP-1.5: possession-cooldown timer. Decrements per frame via
	// DP_Player::TickPossessionCooldown (called from
	// DPPlayerController_Behaviour::OnUpdate). Set by
	// TryVoluntaryPossessSwitch on a successful voluntary switch /
	// release. Death + apprehend paths bypass this entirely (they
	// call SetPossessedVillager directly), matching the Tuning.json
	// canon: "cooldown_after_burnout_s = 0.0".
	float g_fPossessionCooldownRemaining = 0.0f;

	// MVP-1.8: anchor position. Set by SetPossessedVillager and
	// TryVoluntaryPossessSwitch from the new villager's transform
	// position on any successful possession.
	Zenith_Maths::Vector3 g_xPossessionAnchor = Zenith_Maths::Vector3(0.0f);
	bool                  g_bHasPossessionAnchor = false;

	// MVP-2.1.1 Devout channel state.
	Zenith_EntityID g_xChannelTarget   = INVALID_ENTITY_ID;
	float           g_fChannelRemaining = 0.0f;
	bool            g_bChannelActive    = false;

	// Resolves a villager handle to its world position. Returns false
	// if the entity has no transform / no scene-data binding (e.g.,
	// passed a stale handle after the villager was destroyed).
	bool TryGetVillagerPos(Zenith_EntityID xId, Zenith_Maths::Vector3& xOut)
	{
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneDataForEntity(xId);
		if (pxScene == nullptr) return false;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xId);
		if (!xEnt.IsValid()) return false;
		if (!xEnt.HasComponent<Zenith_TransformComponent>()) return false;
		xEnt.GetComponent<Zenith_TransformComponent>().GetPosition(xOut);
		return true;
	}
}

namespace DP_Player
{
	Zenith_EntityID GetPossessedVillager()
	{
		return g_xPossessedVillager;
	}

	void SetPossessedVillager(Zenith_EntityID xId)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_Player::SetPossessedVillager must be called from main thread");
		const Zenith_EntityID xPrior = g_xPossessedVillager;
		g_xPossessedVillager = xId;
		if (xId.IsValid() && TryGetVillagerPos(xId, g_xPossessionAnchor))
		{
			g_bHasPossessionAnchor = true;
		}
		else
		{
			g_bHasPossessionAnchor = false;
		}
		if (xPrior != xId)
		{
			Zenith_EventDispatcher::Get().Dispatch(
				DP_OnPossessionChanged{ xPrior, xId });
		}
	}

	// Internal helper used by both the immediate-possession path and
	// the channel-completion path. Updates possessed handle, sets
	// anchor, arms cooldown, bumps scent. Caller is responsible for
	// having already passed the gates (cooldown / state / range).
	static void CommitVoluntaryPossession(
		Zenith_EntityID xId,
		const Zenith_Maths::Vector3& xNewPos,
		bool bGotNewPos)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_Player::CommitVoluntaryPossession must be called from main thread");
		g_xPossessedVillager = xId;
		g_fPossessionCooldownRemaining =
			DP_Tuning::Get<float>("possession.cooldown_after_voluntary_switch_s");

		if (bGotNewPos)
		{
			g_xPossessionAnchor = xNewPos;
			g_bHasPossessionAnchor = true;
		}
		else
		{
			g_bHasPossessionAnchor = false;
		}

		// MVP-1.6 scent: only successful possession bumps. Tuning.json's
		// scent comment explicitly says channel-interrupted switches
		// produce no scent.
		if (xId.IsValid())
		{
			DPPlayerController_Behaviour* pxCtrl =
				DPPlayerController_Behaviour::Instance();
			if (pxCtrl != nullptr)
			{
				const float fPerPossession =
					DP_Tuning::Get<float>("possession.demon_scent_per_possession");
				const float fMaxScent =
					DP_Tuning::Get<float>("possession.demon_scent_max");
				pxCtrl->BumpDemonScent(xId, fPerPossession, fMaxScent);
			}
		}
	}

	bool TryVoluntaryPossessSwitch(Zenith_EntityID xId)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_Player::TryVoluntaryPossessSwitch must be called from main thread");
		// Idempotent re-click.
		if (xId.IsValid()
			&& g_xPossessedVillager.IsValid()
			&& xId.m_uIndex == g_xPossessedVillager.m_uIndex
			&& xId.m_uGeneration == g_xPossessedVillager.m_uGeneration)
		{
			return true;
		}

		// MVP-2.1.1: refuse a new switch attempt while a Devout channel
		// is in progress.
		if (g_bChannelActive)
		{
			if (xId.IsValid()
				&& g_xChannelTarget.IsValid()
				&& xId.m_uIndex == g_xChannelTarget.m_uIndex
				&& xId.m_uGeneration == g_xChannelTarget.m_uGeneration)
			{
				return true;
			}
			return false;
		}

		// Cooldown gate.
		if (g_fPossessionCooldownRemaining > 0.0f)
		{
			return false;
		}

		// MVP-1.4.1-3: state gate. Refuse fainted (still recovering) or
		// dead villagers. SetPossessedVillager (system path) bypasses
		// this -- only the player-driven voluntary switch enforces it.
		if (xId.IsValid())
		{
			Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneDataForEntity(xId);
			if (pxScene != nullptr)
			{
				Zenith_Entity xEnt = pxScene->TryGetEntity(xId);
				if (xEnt.IsValid() && xEnt.HasComponent<Zenith_ScriptComponent>())
				{
					DPVillager_Behaviour* pxV =
						xEnt.GetComponent<Zenith_ScriptComponent>()
							.GetScript<DPVillager_Behaviour>();
					if (pxV != nullptr && !pxV->IsPossessable())
					{
						return false;
					}
				}
			}
		}

		// MVP-1.8: range gate.
		Zenith_Maths::Vector3 xNewPos(0.0f);
		const bool bGotNewPos = xId.IsValid() && TryGetVillagerPos(xId, xNewPos);
		if (g_bHasPossessionAnchor && bGotNewPos)
		{
			const float fDx = xNewPos.x - g_xPossessionAnchor.x;
			const float fDz = xNewPos.z - g_xPossessionAnchor.z;
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
		if (xId.IsValid())
		{
			Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneDataForEntity(xId);
			if (pxScene != nullptr)
			{
				Zenith_Entity xEnt = pxScene->TryGetEntity(xId);
				if (xEnt.IsValid() && xEnt.HasComponent<Zenith_ScriptComponent>())
				{
					DPVillager_Behaviour* pxV =
						xEnt.GetComponent<Zenith_ScriptComponent>()
							.GetScript<DPVillager_Behaviour>();
					if (pxV != nullptr && pxV->GetArchetypeId() == "Devout")
					{
						fChannelSec = DP_Tuning::Get<float>("possession.channel_devout_s");
					}
				}
			}
		}

		if (fChannelSec > 0.0f)
		{
			g_xChannelTarget    = xId;
			g_fChannelRemaining = fChannelSec;
			g_bChannelActive    = true;
			return true;
		}

		CommitVoluntaryPossession(xId, xNewPos, bGotNewPos);
		return true;
	}

	bool IsChanneling()
	{
		return g_bChannelActive;
	}

	Zenith_EntityID GetChannelTarget()
	{
		return g_xChannelTarget;
	}

	float GetChannelRemaining()
	{
		return g_fChannelRemaining;
	}

	void InterruptChannel()
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_Player::InterruptChannel must be called from main thread");
		if (!g_bChannelActive) return;
		g_bChannelActive    = false;
		g_xChannelTarget    = INVALID_ENTITY_ID;
		g_fChannelRemaining = 0.0f;
	}

	void TickChannel(float fDt)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_Player::TickChannel must be called from main thread");
		if (!g_bChannelActive) return;

		// MVP-2.1.2 interrupt check: any priest within
		// channel_interrupt_distance_m of the channel target cancels
		// the channel before the timer runs down.
		Zenith_Maths::Vector3 xTargetPos(0.0f);
		const bool bGotTargetPos =
			g_xChannelTarget.IsValid()
			&& TryGetVillagerPos(g_xChannelTarget, xTargetPos);
		if (bGotTargetPos)
		{
			const float fInterruptDist =
				DP_Tuning::Get<float>("possession.channel_interrupt_distance_m");
			bool bInterrupted = false;
			DP_Query::ForEachScriptInActiveScene<Priest_Behaviour>(
				[&bInterrupted, &xTargetPos, fInterruptDist]
				(Zenith_EntityID xPriestId, Priest_Behaviour&)
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

		g_fChannelRemaining -= fDt;
		if (g_fChannelRemaining <= 0.0f)
		{
			Zenith_EntityID xTarget = g_xChannelTarget;
			g_bChannelActive    = false;
			g_xChannelTarget    = INVALID_ENTITY_ID;
			g_fChannelRemaining = 0.0f;
			Zenith_Maths::Vector3 xCommitPos(0.0f);
			const bool bGotPos =
				xTarget.IsValid() && TryGetVillagerPos(xTarget, xCommitPos);
			CommitVoluntaryPossession(xTarget, xCommitPos, bGotPos);
		}
	}

	void TickPossessionCooldown(float fDt)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_Player::TickPossessionCooldown must be called from main thread");
		if (g_fPossessionCooldownRemaining <= 0.0f) return;
		g_fPossessionCooldownRemaining -= fDt;
		if (g_fPossessionCooldownRemaining < 0.0f)
		{
			g_fPossessionCooldownRemaining = 0.0f;
		}
	}

	float GetPossessionCooldownRemaining()
	{
		return g_fPossessionCooldownRemaining;
	}

	// 2026-05-17 scene-ownership refactor: all per-villager side
	// tables (held item, demon scent) live on
	// DPPlayerController_Behaviour::m_xHeldItems / m_xDemonScent so
	// they get destroyed automatically when the scene that owns the
	// controller unloads. Forwarders below return safe defaults when
	// no controller is loaded (between-scenes / non-DP scenes).

	float GetDemonScent(Zenith_EntityID xVillager)
	{
		if (!xVillager.IsValid()) return 0.0f;
		DPPlayerController_Behaviour* pxCtrl = DPPlayerController_Behaviour::Instance();
		if (pxCtrl == nullptr) return 0.0f;
		return pxCtrl->GetDemonScent(xVillager);
	}

	void TickDemonScent(float fDt)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_Player::TickDemonScent must be called from main thread");
		DPPlayerController_Behaviour* pxCtrl = DPPlayerController_Behaviour::Instance();
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
		DPPlayerController_Behaviour* pxCtrl = DPPlayerController_Behaviour::Instance();
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

		const uint32_t uSlotCount = Zenith_SceneManager::GetSceneSlotCount();
		for (uint32_t uSlot = 0; uSlot < uSlotCount; ++uSlot)
		{
			Zenith_SceneData* pxScene =
				Zenith_SceneManager::GetLoadedSceneDataAtSlot(uSlot);
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
		DPPlayerController_Behaviour* pxCtrl = DPPlayerController_Behaviour::Instance();
		if (pxCtrl == nullptr) return DP_ItemTag::None;
		return pxCtrl->GetHeldItemTag(xVillager);
	}

	Zenith_EntityID GetHeldItemEntity(Zenith_EntityID xVillager)
	{
		DPPlayerController_Behaviour* pxCtrl = DPPlayerController_Behaviour::Instance();
		if (pxCtrl == nullptr) return INVALID_ENTITY_ID;
		return pxCtrl->GetHeldItemEntity(xVillager);
	}

	void SetHeldItem(Zenith_EntityID xVillager, Zenith_EntityID xItem)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_Player::SetHeldItem must be called from main thread");
		DPPlayerController_Behaviour* pxCtrl = DPPlayerController_Behaviour::Instance();
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
		DPPlayerController_Behaviour* pxCtrl = DPPlayerController_Behaviour::Instance();
		if (pxCtrl == nullptr) return;
		pxCtrl->RemoveHeldItem(xVillager);
	}

	void ResetForNewRun()
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_Player::ResetForNewRun must be called from main thread");
		g_xPossessedVillager = INVALID_ENTITY_ID;
		// Note: held-items + demon-scent tables no longer need explicit
		// reset here -- they're owned by DPPlayerController_Behaviour
		// which is destroyed on scene unload. 2026-05-17.
		g_fPossessionCooldownRemaining = 0.0f;
		g_xPossessionAnchor = Zenith_Maths::Vector3(0.0f);
		g_bHasPossessionAnchor = false;
		g_xChannelTarget    = INVALID_ENTITY_ID;
		g_fChannelRemaining = 0.0f;
		g_bChannelActive    = false;
	}
}
