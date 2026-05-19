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

		Zenith_Maths::Vector3 xCenter;
		xAgent.GetComponent<Zenith_TransformComponent>().GetPosition(xCenter);

		const float fRadius = xBB.GetFloat(DP_AI::BB_KEY_SUSPICION_RADIUS, 15.0f);
		Zenith_Maths::Vector3 xResult;
		if (!m_pxNavMesh->GetRandomReachablePointInRadius(xCenter, fRadius, xResult))
		{
			return BTNodeStatus::FAILURE;
		}
		xBB.SetVector3(DP_AI::BB_KEY_PATROL_TARGET, xResult);
		return BTNodeStatus::SUCCESS;
	}

	const char* GetTypeName() const override { return "DP_FindPosInSuspicionSphere"; }

	void SetNavMesh(const Zenith_NavMesh* pxNavMesh) { m_pxNavMesh = pxNavMesh; }

private:
	const Zenith_NavMesh* m_pxNavMesh = nullptr;
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

	void OnEnter(Zenith_Entity& xAgent, Zenith_Blackboard& /*xBB*/) override
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

		// Stop the navmesh agent so the priest plants and channels
		// instead of continuing to drift along the path Pursue had
		// requested. Pursue will re-request a path when Apprehend
		// FAILS (target leaves range) and the Selector falls through.
		if (xAgent.HasComponent<Zenith_AIAgentComponent>())
		{
			Zenith_AIAgentComponent& xAI = xAgent.GetComponent<Zenith_AIAgentComponent>();
			if (Zenith_NavMeshAgent* pxNav = xAI.GetNavMeshAgent())
			{
				pxNav->Stop();
			}
		}
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
			Zenith_SceneManager::GetSceneDataForEntity(xTarget);
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
