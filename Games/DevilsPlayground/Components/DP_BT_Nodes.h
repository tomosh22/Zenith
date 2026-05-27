#pragma once
/**
 * Custom BT nodes used by Priest_Behaviour.
 *
 *  - DP_BTAction_FindPosInSuspicionSphere    (writes BB.PatrolTarget)
 *  - DP_BTCondition_HasInvestigatePos        (reads BB.HasInvestigatePos)
 *  - DP_BTAction_ClearInvestigatePos         (sets BB.HasInvestigatePos=false)
 *  - DP_BTDecorator_IsTargetValid            (gates child on BB.TargetWithDevil != INVALID)
 *  - DP_BTAction_Apprehend                   (channels on possessed villager,
 *                                             dispatches DP_OnRunLost{Apprehended})
 */

#include "AI/BehaviorTree/Zenith_BTNode.h"
#include "AI/BehaviorTree/Zenith_Blackboard.h"
#include "AI/Components/Zenith_AIAgentComponent.h"
#include "AI/Navigation/Zenith_NavMesh.h"
#include "AI/Navigation/Zenith_NavMeshAgent.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Zenith_EventSystem.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Source/DP_Tuning.h"

class DP_BTAction_FindPosInSuspicionSphere : public Zenith_BTLeaf
{
public:
	DP_BTAction_FindPosInSuspicionSphere() = default;

	BTNodeStatus Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBB, float /*fDt*/) override
	{
		if (m_pxNavMesh == nullptr) return BTNodeStatus::FAILURE;
		if (!xAgent.HasComponent<Zenith_TransformComponent>()) return BTNodeStatus::FAILURE;

		Zenith_Maths::Vector3 xAgentPos;
		xAgent.GetComponent<Zenith_TransformComponent>().GetPosition(xAgentPos);

		// 2026-05-21: bias patrol toward the high-scent villager when
		// one's been written to BB_KEY_HIGH_SCENT_TARGET AND that
		// villager's scent is above the hound-bark threshold. Centers
		// the random-reachable-point search on the scent target's
		// position rather than the priest's own. Means "demon's been
		// hopping bodies a lot -- priest is drawn to where the demon's
		// scent is strongest." Falls back to the agent's own position
		// when no qualifying scent target exists.
		Zenith_Maths::Vector3 xCenter = xAgentPos;
		float fRadius = xBB.GetFloat(DP_AI::BB_KEY_SUSPICION_RADIUS, 15.0f);
		bool bScentDiversion = false;
		const Zenith_EntityID xScentTarget =
			xBB.GetEntityID(DP_AI::BB_KEY_HIGH_SCENT_TARGET);
		if (xScentTarget.IsValid())
		{
			const float fScent = DP_Player::GetDemonScent(xScentTarget);
			const float fThreshold =
				DP_Tuning::Get<float>("possession.demon_scent_hound_bark_threshold");
			if (fScent >= fThreshold)
			{
				Zenith_SceneData* pxScene =
					g_xEngine.SceneRegistry().GetSceneDataForEntity(xScentTarget);
				if (pxScene != nullptr)
				{
					Zenith_Entity xTgt = pxScene->TryGetEntity(xScentTarget);
					if (xTgt.IsValid() && xTgt.HasComponent<Zenith_TransformComponent>())
					{
						xTgt.GetComponent<Zenith_TransformComponent>().GetPosition(xCenter);
						bScentDiversion = true;
					}
				}
			}
		}

		// 2026-05-25: tried wiring DP_AI::GetPatrolNodes() into the
		// FindPos cycle so the priest would roam across rooms instead
		// of staying within 15 m of its spawn. Both "every cycle" and
		// "every 4th cycle" patrol-node sampling dropped the matrix
		// win rates from 70%+ to 0% across the board (the priest
		// reliably reached every objective hand-off zone before the
		// bot could deliver). Reverted: patrol-node storage still
		// exists in DP_AI for future tuning, but the BT only uses the
		// local 15 m suspicion sphere + scent diversion. The "priest
		// stays in spawn room" complaint is partially addressed by
		// the priest's own door-opening (DP_AI::OpenNearbyDoorsFor)
		// -- once the player opens a route into the priest's room,
		// the priest can patrol outward without the navmesh BLOCKED
		// flag walling it in (DPDoor::SyncNavMeshBlock is a no-op).

		// 2026-05-26: every 5th FindPos call, try a procgen patrol
		// node as the target instead of the local suspicion sphere.
		// The patrol nodes are spread across the map (one per non-
		// pent room near the priest spawn) -- visiting them gives
		// the priest a route OUT of its spawn navmesh region so
		// OpenNearbyDoorsFor catches the room's exit doors as the
		// priest walks past them.
		//
		// 1-in-5 ratio chosen to balance: priest's threat curve
		// remains driven by perception (scent/sight) so it doesn't
		// reach every objective hand-off zone unprompted, but the
		// priest no longer parks in its spawn room for the whole
		// run when no contact triggers a pursue.
		//
		// History (matrix dropped from 70%+ to 0% on earlier attempts):
		// "every cycle" and "every 4th cycle" patrol-node sampling
		// dropped the matrix to 0% wins because the priest reached
		// the bot's hand-off zones reliably before the bot could
		// deliver. 1-in-5 sampling at this point in the bot+door
		// pipeline (with sensor toggle + all-doors rasterisation
		// fixes already landed) lets the priest occasionally roam
		// without dominating the player's escape windows.
		m_uFindPosTick++;
		const Zenith_Vector<Zenith_Maths::Vector3>& axPatrolNodes =
			DP_AI::GetPatrolNodes();
		const uint32_t uPatrolN = axPatrolNodes.GetSize();
		Zenith_Maths::Vector3 xResult;
		bool bGot = false;
		if (uPatrolN > 0 && (m_uFindPosTick % 5u) == 0u && !bScentDiversion)
		{
			const uint32_t uIdx = (m_uFindPosTick / 5u) % uPatrolN;
			const Zenith_Maths::Vector3& xNode = axPatrolNodes.Get(uIdx);
			// Try to find a reachable point near the patrol node.
			// Use a generous 30 m sphere so the request succeeds even
			// when the node sits in a building the priest hasn't
			// reached yet (the priest's planner will path as far as
			// the current navmesh permits, opening doors along the
			// way via OpenNearbyDoorsFor).
			if (m_pxNavMesh->GetRandomReachablePointInRadius(xNode, 30.0f, xResult))
			{
				bGot = true;
			}
		}
		if (!bGot)
		{
			if (!m_pxNavMesh->GetRandomReachablePointInRadius(xCenter, fRadius, xResult))
			{
				return BTNodeStatus::FAILURE;
			}
		}
		xBB.SetVector3(DP_AI::BB_KEY_PATROL_TARGET, xResult);
		return BTNodeStatus::SUCCESS;
	}

	const char* GetTypeName() const override { return "DP_FindPosInSuspicionSphere"; }

	void SetNavMesh(const Zenith_NavMesh* pxNavMesh) { m_pxNavMesh = pxNavMesh; }

private:
	const Zenith_NavMesh* m_pxNavMesh = nullptr;
	// 2026-05-26: incremented on every Execute. Used for the 1-in-5
	// patrol-node sampling pattern in the body (see comment there).
	uint32_t              m_uFindPosTick = 0u;
};

class DP_BTCondition_HasInvestigatePos : public Zenith_BTLeaf
{
public:
	BTNodeStatus Execute(Zenith_Entity& /*xAgent*/, Zenith_Blackboard& xBB, float /*fDt*/) override
	{
		return xBB.GetBool(DP_AI::BB_KEY_HAS_INVESTIGATE_POS, false)
			? BTNodeStatus::SUCCESS
			: BTNodeStatus::FAILURE;
	}
	const char* GetTypeName() const override { return "DP_HasInvestigatePos"; }
};

class DP_BTAction_ClearInvestigatePos : public Zenith_BTLeaf
{
public:
	BTNodeStatus Execute(Zenith_Entity& /*xAgent*/, Zenith_Blackboard& xBB, float /*fDt*/) override
	{
		xBB.SetBool(DP_AI::BB_KEY_HAS_INVESTIGATE_POS, false);
		return BTNodeStatus::SUCCESS;
	}
	const char* GetTypeName() const override { return "DP_ClearInvestigatePos"; }
};

class DP_BTDecorator_IsTargetValid : public Zenith_BTDecorator
{
public:
	BTNodeStatus Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBB, float fDt) override
	{
		const Zenith_EntityID xTgt = xBB.GetEntityID(DP_AI::BB_KEY_TARGET_WITH_DEVIL);
		if (!xTgt.IsValid()) return BTNodeStatus::FAILURE;
		if (m_pxChild == nullptr) return BTNodeStatus::FAILURE;
		return m_pxChild->Execute(xAgent, xBB, fDt);
	}
	const char* GetTypeName() const override { return "DP_IsTargetValid"; }
};

// MVP-1.3.2: Apprehend BT action. Placed BEFORE pursue in the priest's
// Selector so it intercepts the moment the priest gets within
// priest.apprehend_range_m of the possessed villager. The priest stands
// still and channels for priest.apprehend_channel_s seconds; if the
// channel completes without interruption, it dispatches
// DP_OnRunLost{Apprehended} and clears the player's possession.
// Interruption sources:
//   * Possessed-villager handle changes (player switched to another
//     vessel) -- Priest_Behaviour::OnUpdate's m_xTree.Reset() on rising-
//     edge of BB target tears the node down before this Execute runs.
//   * Target leaves apprehend range (player ran) -- Execute returns
//     FAILURE so the Selector falls back to pursue.
//   * Target despawns (e.g., villager died) -- HasTarget check earlier
//     in the same Selector turn would have failed; in this node the
//     scene-data null check handles the same case defensively.
class DP_BTAction_Apprehend : public Zenith_BTLeaf
{
public:
	DP_BTAction_Apprehend() = default;

	void OnEnter(Zenith_Entity& /*xAgent*/, Zenith_Blackboard& /*xBB*/) override
	{
		// Cache tuning per-run so DP_Tuning hot-reload during a play
		// session (if ever wired) takes effect on the next apprehend
		// window. m_fAppliedChannelSeconds is the actual duration this
		// channel will run for; m_fChannelElapsed is the per-channel
		// timer.
		m_fAppliedChannelSeconds = DP_Tuning::Get<float>("priest.apprehend_channel_s");
		m_fAppliedRangeMetres    = DP_Tuning::Get<float>("priest.apprehend_range_m");
		m_fChannelElapsed        = 0.0f;
		m_xChannelTarget         = INVALID_ENTITY_ID;
		m_bChannelStartEmitted   = false;

		// Note: we no longer call pxNav->Stop() here. Originally the
		// priest planted-and-channelled, which (per the seed-matrix
		// analysis 2026-05-19) made the catch 0% reliable -- any
		// possessed villager could simply walk out of the 2 m range
		// during the 3 s channel and bypass the catch forever. We now
		// keep pursuing during Execute so the channel timer only
		// completes if the villager can be held in range -- walkers
		// get caught (priest pursue 7 m/s > walk 4 m/s); sprinters
		// (12 m/s) still escape.
	}

	// Helper: emit a DP_OnApprehendChannelInterrupted event iff a channel
	// had been started but not completed. Called from every FAILURE
	// return so the visualiser sees the interrupt regardless of which
	// guard path tripped.
	void EmitInterruptedIfRunning(const Zenith_EntityID& xPriestID,
	                              DP_ApprehendInterruptReason eReason)
	{
		if (!m_bChannelStartEmitted) return;
		Zenith_EventDispatcher::Get().Dispatch(
			DP_OnApprehendChannelInterrupted{
				xPriestID, m_xChannelTarget, eReason });
		m_bChannelStartEmitted = false;
	}

	BTNodeStatus Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBB, float fDt) override
	{
		const Zenith_EntityID xPriestID = xAgent.GetEntityID();
		const Zenith_EntityID xTarget = xBB.GetEntityID(DP_AI::BB_KEY_TARGET_WITH_DEVIL);
		if (!xTarget.IsValid())
		{
			EmitInterruptedIfRunning(xPriestID, DP_ApprehendInterruptReason::TargetLost);
			m_eLastStatus = BTNodeStatus::FAILURE; return m_eLastStatus;
		}

		// Resolve target's scene + transform.
		Zenith_SceneData* pxScene =
			g_xEngine.SceneRegistry().GetSceneDataForEntity(xTarget);
		if (pxScene == nullptr)
		{
			EmitInterruptedIfRunning(xPriestID, DP_ApprehendInterruptReason::TargetLost);
			m_eLastStatus = BTNodeStatus::FAILURE; return m_eLastStatus;
		}
		Zenith_Entity xTargetEnt = pxScene->TryGetEntity(xTarget);
		if (!xTargetEnt.IsValid()
		 || !xTargetEnt.HasComponent<Zenith_TransformComponent>()
		 || !xAgent.HasComponent<Zenith_TransformComponent>())
		{
			EmitInterruptedIfRunning(xPriestID, DP_ApprehendInterruptReason::TargetLost);
			m_eLastStatus = BTNodeStatus::FAILURE; return m_eLastStatus;
		}

		Zenith_Maths::Vector3 xTargetPos, xAgentPos;
		xTargetEnt.GetComponent<Zenith_TransformComponent>().GetPosition(xTargetPos);
		xAgent.GetComponent<Zenith_TransformComponent>().GetPosition(xAgentPos);

		// Horizontal distance only -- DP's gameplay plane is flat-ish and
		// the priest/villager Y values are body-capsule-driven (settle at
		// half-height above the floor), so a 3D distance would erroneously
		// reject apprehend the moment one rests slightly lower than the
		// other.
		const float fDx = xTargetPos.x - xAgentPos.x;
		const float fDz = xTargetPos.z - xAgentPos.z;
		const float fDist = std::sqrt(fDx * fDx + fDz * fDz);

		if (fDist > m_fAppliedRangeMetres)
		{
			// Out of range -- bail so the Selector falls back to pursue.
			// OnEnter resets state when this branch is re-entered after
			// the priest re-closes the gap.
			EmitInterruptedIfRunning(xPriestID, DP_ApprehendInterruptReason::OutOfRange);
			m_eLastStatus = BTNodeStatus::FAILURE;
			return m_eLastStatus;
		}

		// Lock onto target on first valid tick. If the BB target swaps
		// mid-channel (player switched villager), m_xTree.Reset() in
		// Priest_Behaviour::OnUpdate aborts this node -- but as a defence
		// in depth, also bail here on mismatch.
		if (!m_xChannelTarget.IsValid())
		{
			m_xChannelTarget = xTarget;
			// Channel start: emit once on lock-on so the visualiser
			// timeline shows the channel window even when the channel
			// gets interrupted before SUCCESS.
			Zenith_EventDispatcher::Get().Dispatch(
				DP_OnApprehendChannelStart{ xPriestID, xTarget });
			m_bChannelStartEmitted = true;
		}
		else if (m_xChannelTarget.m_uIndex != xTarget.m_uIndex
		      || m_xChannelTarget.m_uGeneration != xTarget.m_uGeneration)
		{
			EmitInterruptedIfRunning(xPriestID, DP_ApprehendInterruptReason::TargetSwitched);
			m_eLastStatus = BTNodeStatus::FAILURE;
			return m_eLastStatus;
		}

		// Continue pursuing the villager while channelling. Without this
		// the priest would plant in place and any walking villager would
		// simply outpace the 2 m range during the 3 s channel (verified
		// 0 / 15 catches in the seed-matrix analysis 2026-05-19). With
		// pursuit the priest closes at pursue_speed_mps (~7 m/s in
		// Tuning.json) -- walk speed is 4 m/s so the priest gains 3 m/s
		// on a walking target, catching them inside ~1 channel window;
		// sprint speed is 12 m/s so sprinters still outrun and the
		// channel times out on OutOfRange. Throttle SetDestination to
		// once per BT tick (default 10 Hz) so we don't thrash the
		// pathfinder.
		if (xAgent.HasComponent<Zenith_AIAgentComponent>())
		{
			Zenith_AIAgentComponent& xAI = xAgent.GetComponent<Zenith_AIAgentComponent>();
			if (Zenith_NavMeshAgent* pxNav = xAI.GetNavMeshAgent())
			{
				pxNav->SetDestination(xTargetPos);
			}
		}

		m_fChannelElapsed += fDt;
		if (m_fChannelElapsed >= m_fAppliedChannelSeconds)
		{
			// Channel complete. Dispatch the run-loss event and clear
			// the player's possession (the player gets bounced to the
			// "run over" screen by whichever subscriber owns that).
			// Payload carries the apprehended villager (xTarget) + the
			// priest that caught them (xAgent.GetEntityID()) so the
			// telemetry visualiser can plot the catch site.
			Zenith_EventDispatcher::Get().Dispatch(
				DP_OnApprehendChannelComplete{ xPriestID, xTarget });
			m_bChannelStartEmitted = false;
			Zenith_EventDispatcher::Get().Dispatch(
				DP_OnRunLost{
					DP_RunLostCause::Apprehended,
					xTarget,
					xPriestID });
			DP_Player::SetPossessedVillager(INVALID_ENTITY_ID);
			m_eLastStatus = BTNodeStatus::SUCCESS;
			return m_eLastStatus;
		}

		m_eLastStatus = BTNodeStatus::RUNNING;
		return m_eLastStatus;
	}

	const char* GetTypeName() const override { return "DP_Apprehend"; }

#ifdef ZENITH_INPUT_SIMULATOR
	// Test-only accessors for inspecting channel progress without
	// reaching into private state. Not used by production gameplay.
	float           GetChannelElapsed() const  { return m_fChannelElapsed; }
	float           GetAppliedChannelSeconds() const { return m_fAppliedChannelSeconds; }
	float           GetAppliedRangeMetres() const    { return m_fAppliedRangeMetres; }
	Zenith_EntityID GetChannelTarget() const    { return m_xChannelTarget; }
#endif

private:
	float           m_fChannelElapsed        = 0.0f;
	float           m_fAppliedChannelSeconds = 3.0f;
	float           m_fAppliedRangeMetres    = 2.0f;
	Zenith_EntityID m_xChannelTarget         = INVALID_ENTITY_ID;
	bool            m_bChannelStartEmitted   = false; // emit Interrupted iff true
};
