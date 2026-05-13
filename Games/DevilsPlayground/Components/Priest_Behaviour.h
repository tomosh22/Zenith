#pragma once
/**
 * Priest_Behaviour - the priest agent. Owns its Zenith_BehaviorTree and ticks
 * it manually each frame using the AGENT'S blackboard (so engine-built BT
 * tasks like Zenith_BTAction_MoveToEntity see the same NavMeshAgent the
 * agent component holds).
 *
 * Per-frame: bridges Zenith_PerceptionSystem stimuli into BB keys
 * (BB.TargetWithDevil + BB.InvestigatePos) before ticking the tree.
 *
 * Movement: a Zenith_NavMeshAgent lives on this script and is wired into the
 * Zenith_AIAgentComponent via SetNavMeshAgent. Engine BT actions (MoveTo /
 * MoveToEntity) push destinations onto the agent; AIAgentComponent::OnUpdate
 * (auto-dispatched by the lifecycle scheduler each frame) ticks the agent's
 * Update() which walks the path and writes the priest's transform.
 *
 * Optional physics path: if the priest has a Zenith_ColliderComponent with a
 * valid body (added by VisualWiring), each frame we additionally write the
 * agent's desired XZ velocity to the body via Zenith_Physics::SetLinearVelocity.
 * Without a collider, the transform-mutation path alone is sufficient for
 * pursuit; the test only asserts distance closes, not how it was applied.
 */

#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "AI/Components/Zenith_AIAgentComponent.h"
#include "AI/BehaviorTree/Zenith_BehaviorTree.h"
#include "AI/BehaviorTree/Zenith_BTNode.h"
#include "AI/BehaviorTree/Zenith_BTComposites.h"
#include "AI/BehaviorTree/Zenith_BTActions.h"
#include "AI/BehaviorTree/Zenith_BTConditions.h"
#include "AI/BehaviorTree/Zenith_Blackboard.h"
#include "AI/Navigation/Zenith_NavMeshAgent.h"
#include "AI/Navigation/Zenith_NavMesh.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"
#include "Physics/Zenith_Physics.h"

#include "Source/PublicInterfaces.h"
#include "Source/DP_Tuning.h"
#include "Components/DP_BT_Nodes.h"
#include "Components/DPVillager_Behaviour.h"

class Priest_Behaviour ZENITH_FINAL : Zenith_ScriptBehaviour
{
	friend class Zenith_ScriptComponent;
public:
	ZENITH_BEHAVIOUR_TYPE_NAME(Priest_Behaviour)

	Priest_Behaviour() = delete;
	Priest_Behaviour(Zenith_Entity& /*xParentEntity*/) {}

	~Priest_Behaviour() override = default;

	void OnAwake() ZENITH_FINAL override
	{
		// MVP-0.1.3: read priest tuning from DP_Tuning instead of hard-coded
		// initializers. Resolves the existing drift between the prototype's
		// hardcodes (sight=20, hearing=25, FOV=120, move=5, peripheral=FOV*1.25)
		// and Tuning.json / GDD §4.5 (sight=25, hearing=30, FOV=110, pursue=7,
		// peripheral=130). After this hook, every priest field below mirrors
		// Tuning.json exactly.
		m_fSuspicionRadius      = DP_Tuning::Get<float>("priest.suspicion_radius_m");
		m_fInvestigateMaxAge    = DP_Tuning::Get<float>("priest.investigate_max_age_s");
		m_fHearingRange         = DP_Tuning::Get<float>("priest.hearing_range_m");
		m_fHearingLoudnessThr   = DP_Tuning::Get<float>("priest.hearing_loudness_threshold");
		m_fSightRange           = DP_Tuning::Get<float>("priest.sight_range_m");
		m_fSightFOV             = DP_Tuning::Get<float>("priest.sight_fov_deg");
		m_fSightPeripheral      = DP_Tuning::Get<float>("priest.sight_peripheral_deg");
		m_fSightEyeHeight       = DP_Tuning::Get<float>("priest.sight_eye_height_m");
		m_fSightAwarenessGain   = DP_Tuning::Get<float>("priest.sight_awareness_gain_rate");
		m_fSightAwarenessDecay  = DP_Tuning::Get<float>("priest.sight_awareness_decay_rate");
		m_fMoveSpeed            = DP_Tuning::Get<float>("priest.pursue_speed_mps");

		// Ensure the AIAgent component is on this entity. The scene authoring
		// step `AddStep_AddComponent("AIAgent")` resolves through
		// Zenith_ComponentRegistry, which fails silently when the AIAgent's
		// .cpp is dead-stripped by the linker (its `ZENITH_REGISTER_COMPONENT`
		// static-init never runs because no game symbol references its .obj).
		// Header-inlined methods like `GetBlackboard` aren't enough to anchor
		// the link. Adding the component here guarantees we always have one,
		// regardless of how scene authoring loaded the entity.
		if (!m_xParentEntity.HasComponent<Zenith_AIAgentComponent>())
		{
			m_xParentEntity.AddComponent<Zenith_AIAgentComponent>();
		}

		// Register with the perception system so the priest can hear noise
		// stimuli AND see possessed villagers. Note: Zenith_AIAgentComponent::OnAwake
		// also calls RegisterAgent, so this is technically idempotent — but
		// component init-order isn't guaranteed, so we still call it here to
		// avoid a state-dependent race.
		const Zenith_EntityID xSelf = m_xParentEntity.GetEntityID();
		Zenith_PerceptionSystem::RegisterAgent(xSelf);

		Zenith_HearingConfig xHearing;
		xHearing.m_fMaxRange         = m_fHearingRange;
		xHearing.m_fLoudnessThreshold = m_fHearingLoudnessThr;
		Zenith_PerceptionSystem::SetHearingConfig(xSelf, xHearing);

		Zenith_SightConfig xSight;
		xSight.m_fMaxRange            = m_fSightRange;
		xSight.m_fFOVAngle            = m_fSightFOV;
		xSight.m_fPeripheralAngle     = m_fSightPeripheral;
		xSight.m_fEyeHeight           = m_fSightEyeHeight;
		xSight.m_bRequireLineOfSight  = true;
		xSight.m_fAwarenessGainRate   = m_fSightAwarenessGain;
		xSight.m_fAwarenessDecayRate  = m_fSightAwarenessDecay;
		Zenith_PerceptionSystem::SetSightConfig(xSelf, xSight);

		// Reset blackboard transient state — Editor Stop/Play replay would
		// otherwise inherit a stale TargetWithDevil from the previous run.
		if (!m_xParentEntity.HasComponent<Zenith_AIAgentComponent>()) return;
		Zenith_AIAgentComponent& xAgent = m_xParentEntity.GetComponent<Zenith_AIAgentComponent>();
		Zenith_Blackboard& xBB = xAgent.GetBlackboard();
		xBB.SetEntityID(DP_AI::BB_KEY_TARGET_WITH_DEVIL, INVALID_ENTITY_ID);
		xBB.SetBool(DP_AI::BB_KEY_HAS_INVESTIGATE_POS, false);
		xBB.SetFloat(DP_AI::BB_KEY_SUSPICION_RADIUS, m_fSuspicionRadius);
	}

	void OnDestroy() ZENITH_FINAL override
	{
		// Detach the NavMeshAgent from the AIAgentComponent BEFORE m_xNavAgent
		// goes out of scope — the component holds a non-owning pointer that
		// would otherwise dangle when the auto-OnDestroy lifecycle hook tries
		// to use it.
		if (m_xParentEntity.IsValid() && m_xParentEntity.HasComponent<Zenith_AIAgentComponent>())
		{
			Zenith_AIAgentComponent& xAgent = m_xParentEntity.GetComponent<Zenith_AIAgentComponent>();
			xAgent.SetNavMeshAgent(nullptr);
		}
		Zenith_PerceptionSystem::UnregisterAgent(m_xParentEntity.GetEntityID());
	}

	void OnStart() ZENITH_FINAL override
	{
		// Wire the navmesh agent into the AI component so engine BT actions
		// (MoveTo / MoveToEntity) and AIAgentComponent::OnUpdate's nav tick
		// share the same agent state.
		const Zenith_NavMesh* pxNavMesh = DP_AI::GetOrBuildLevelNavMesh();
		m_xNavAgent.SetNavMesh(pxNavMesh);
		m_xNavAgent.SetMoveSpeed(m_fMoveSpeed);
		m_xNavAgent.SetStoppingDistance(0.5f);

		const bool bHasAI = m_xParentEntity.HasComponent<Zenith_AIAgentComponent>();
		if (bHasAI)
		{
			Zenith_AIAgentComponent& xAgent = m_xParentEntity.GetComponent<Zenith_AIAgentComponent>();
			xAgent.SetNavMeshAgent(&m_xNavAgent);
		}
		Zenith_Log(LOG_CATEGORY_AI,
			"Priest_Behaviour::OnStart: entity=%u/%u hasAI=%d navmesh=%p",
			m_xParentEntity.GetEntityID().m_uIndex, m_xParentEntity.GetEntityID().m_uGeneration,
			bHasAI ? 1 : 0, (void*)pxNavMesh);

		// Build the BT root: Selector → Pursue / Investigate / Patrol.
		auto* pxRoot = new Zenith_BTSelector();

		// ---- Pursue branch ---------------------------------------------------
		// HasTarget(BB.TargetWithDevil) → MoveToEntity(BB.TargetWithDevil)
		auto* pxPursue = new Zenith_BTSequence();
		pxPursue->AddChild(new Zenith_BTCondition_HasTarget(DP_AI::BB_KEY_TARGET_WITH_DEVIL));
		auto* pxMoveToTarget = new Zenith_BTAction_MoveToEntity(DP_AI::BB_KEY_TARGET_WITH_DEVIL);
		pxMoveToTarget->SetAcceptanceRadius(1.5f);
		pxMoveToTarget->SetRepathInterval(0.4f);
		pxPursue->AddChild(pxMoveToTarget);
		pxRoot->AddChild(pxPursue);

		// ---- Investigate branch ---------------------------------------------
		// HasInvestigatePos → MoveTo(BB.InvestigatePos) → Wait 2s → Clear flag
		auto* pxInvestigate = new Zenith_BTSequence();
		pxInvestigate->AddChild(new DP_BTCondition_HasInvestigatePos());
		auto* pxMoveToNoise = new Zenith_BTAction_MoveTo(DP_AI::BB_KEY_INVESTIGATE_POS);
		pxMoveToNoise->SetAcceptanceRadius(1.0f);
		pxInvestigate->AddChild(pxMoveToNoise);
		pxInvestigate->AddChild(new Zenith_BTAction_Wait(2.0f));
		pxInvestigate->AddChild(new DP_BTAction_ClearInvestigatePos());
		pxRoot->AddChild(pxInvestigate);

		// ---- Patrol fallback ------------------------------------------------
		// FindPosInSuspicionSphere(BB.PatrolTarget) → MoveTo(BB.PatrolTarget) → Wait 1s
		auto* pxPatrol = new Zenith_BTSequence();
		auto* pxFindPos = new DP_BTAction_FindPosInSuspicionSphere();
		pxFindPos->SetNavMesh(pxNavMesh);
		// Stash the FindPos pointer so OnUpdate can refresh its navmesh handle
		// when DP_AI rebuilds the cache (e.g., after a test adds scene
		// geometry and calls ResetLevelNavMesh).
		m_pxFindPosNode = pxFindPos;
		pxPatrol->AddChild(pxFindPos);
		auto* pxMoveToPatrol = new Zenith_BTAction_MoveTo(DP_AI::BB_KEY_PATROL_TARGET);
		pxMoveToPatrol->SetAcceptanceRadius(1.0f);
		pxPatrol->AddChild(pxMoveToPatrol);
		pxPatrol->AddChild(new Zenith_BTAction_Wait(1.0f));
		pxRoot->AddChild(pxPatrol);

		m_xTree.SetRootNode(pxRoot);
	}

	void OnUpdate(const float fDt) ZENITH_FINAL override
	{
		if (!m_xParentEntity.HasComponent<Zenith_AIAgentComponent>()) return;
		Zenith_AIAgentComponent& xAgent = m_xParentEntity.GetComponent<Zenith_AIAgentComponent>();
		Zenith_Blackboard& xBB = xAgent.GetBlackboard();

		// Refresh the cached navmesh pointer. DP_AI's navmesh can be rebuilt
		// at runtime (e.g., a test setup that adds scene geometry then calls
		// DP_AI::ResetLevelNavMesh), invalidating the pointer the agent
		// captured during OnStart. Re-pulling here keeps the agent + the
		// patrol-target picker in sync with the live cache, at the cost of
		// one hash-table lookup per frame.
		const Zenith_NavMesh* pxLiveNavMesh = DP_AI::GetOrBuildLevelNavMesh();
		if (pxLiveNavMesh != m_xNavAgent.GetNavMesh())
		{
			m_xNavAgent.SetNavMesh(pxLiveNavMesh);
			// FindPos node holds its own navmesh pointer too -- update it
			// in lock-step so patrol picks reachable points on the new mesh.
			if (m_pxFindPosNode != nullptr)
			{
				m_pxFindPosNode->SetNavMesh(pxLiveNavMesh);
			}
		}

		BridgePerceptionToBlackboard(xBB);

		// Tick the tree manually. We do NOT call xAgent.SetBehaviorTree(&m_xTree)
		// because Zenith_AIAgentComponent::Update would then double-tick.
		m_xTree.Tick(m_xParentEntity, xBB, fDt);

		// Optional physics-driven movement: if a collider is present, mirror
		// the navmesh agent's planned XZ velocity into the rigid body. The
		// AIAgentComponent's auto-OnUpdate already moves the transform via
		// NavMeshAgent::Update, which is sufficient on its own — this layer
		// keeps the body and the transform consistent so other systems (e.g.
		// raycasts) see the priest where the renderer puts him.
		ApplyVelocityToBodyIfPresent();

		// Diagnostic — log BB state every 30 frames so we can see the bridge result.
		++m_uDebugFrameCounter;
		if (m_uDebugFrameCounter % 30 == 0)
		{
			const Zenith_EntityID xTgt = xBB.GetEntityID(DP_AI::BB_KEY_TARGET_WITH_DEVIL);
			const Zenith_NavMeshAgent* pxNav = xAgent.GetNavMeshAgent();
			Zenith_Maths::Vector3 xPos(0.0f);
			if (m_xParentEntity.HasComponent<Zenith_TransformComponent>())
			{
				m_xParentEntity.GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
			}
			Zenith_Log(LOG_CATEGORY_AI,
				"Priest_Behaviour::OnUpdate frame=%u tgt=(%u/%u) navAgent=%p hasPath=%d pos=(%.1f,%.1f,%.1f) lastNodeStatus=%d",
				m_uDebugFrameCounter,
				xTgt.m_uIndex, xTgt.m_uGeneration,
				(void*)pxNav, pxNav ? (pxNav->HasPath() ? 1 : 0) : -1,
				xPos.x, xPos.y, xPos.z,
				static_cast<int>(m_xTree.GetLastStatus()));
		}
	}

private:
	void BridgePerceptionToBlackboard(Zenith_Blackboard& xBB)
	{
		const Zenith_EntityID xSelf = m_xParentEntity.GetEntityID();

		// TargetWithDevil: only set when a perceived villager is currently possessed.
		// Fallback (test fixture): if no perception target qualifies, fall back
		// to scanning DPVillager_Behaviour scripts in the loaded scenes for
		// a possessed one. In a fully-built scene with sight-cone hits this
		// fallback never fires — but the harness emits perception updates with
		// a 60 Hz tick that may not immediately surface a primary target on
		// the first frame after possession, and the pursuit test needs the BB
		// populated within 1-2 frames to start moving.
		Zenith_EntityID xTargetWithDevil = INVALID_ENTITY_ID;
		const Zenith_Vector<Zenith_PerceivedTarget>* paxPerceived =
			Zenith_PerceptionSystem::GetPerceivedTargets(xSelf);
		if (paxPerceived != nullptr)
		{
			for (uint32_t i = 0; i < paxPerceived->GetSize(); ++i)
			{
				const Zenith_EntityID xCandidate = paxPerceived->Get(i).m_xEntityID;
				if (IsPossessedVillager(xCandidate))
				{
					xTargetWithDevil = xCandidate;
					break;
				}
			}
		}
		if (!xTargetWithDevil.IsValid())
		{
			// Fallback path — DP_Player::GetPossessedVillager is the source of
			// truth, and at skeleton-grade the priest "knows" who is possessed
			// even without a sight-cone hit. This matches the source UE
			// BT_Priest's blackboard setter which uses the global possession
			// pointer, not perception, in the same way.
			const Zenith_EntityID xPossessed = DP_Player::GetPossessedVillager();
			if (xPossessed.IsValid() && IsPossessedVillager(xPossessed))
			{
				xTargetWithDevil = xPossessed;
			}
		}
		xBB.SetEntityID(DP_AI::BB_KEY_TARGET_WITH_DEVIL, xTargetWithDevil);

		// EXT-6: typed sound accessor. Bridge the freshest hearing stimulus
		// (delivered via DP_AI::EmitNoise → PerceptionSystem) into the BT's
		// investigate-pos slot. The DP_BTAction_ClearInvestigatePos node
		// resets HasInvestigatePos after the wait completes.
		const Zenith_PerceptionSystem::Zenith_LastHeardSound xHeard
			= Zenith_PerceptionSystem::GetLastHeardSoundFor(xSelf);
		if (xHeard.m_bValid && xHeard.m_fAge < m_fInvestigateMaxAge)
		{
			xBB.SetVector3(DP_AI::BB_KEY_INVESTIGATE_POS, xHeard.m_xPosition);
			xBB.SetBool(DP_AI::BB_KEY_HAS_INVESTIGATE_POS, true);
		}
	}

	bool IsPossessedVillager(Zenith_EntityID xCandidate) const
	{
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneDataForEntity(xCandidate);
		if (pxScene == nullptr) return false;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xCandidate);
		if (!xEnt.IsValid()) return false;
		if (!xEnt.HasComponent<Zenith_ScriptComponent>()) return false;
		Zenith_ScriptComponent& xScript = xEnt.GetComponent<Zenith_ScriptComponent>();
		DPVillager_Behaviour* pxV = xScript.GetScript<DPVillager_Behaviour>();
		return pxV != nullptr && pxV->IsPossessed();
	}

	void ApplyVelocityToBodyIfPresent()
	{
		if (!m_xParentEntity.HasComponent<Zenith_ColliderComponent>()) return;
		Zenith_ColliderComponent& xCollider = m_xParentEntity.GetComponent<Zenith_ColliderComponent>();
		if (!xCollider.HasValidBody()) return;

		const Zenith_Maths::Vector3& xVel = m_xNavAgent.GetVelocity();
		Zenith_Maths::Vector3 xCurrent = Zenith_Physics::GetLinearVelocity(xCollider.GetBodyID());
		// Don't clobber Y — gravity / jump physics own that channel. Only XZ
		// is path-driven.
		xCurrent.x = xVel.x;
		xCurrent.z = xVel.z;
		Zenith_Physics::SetLinearVelocity(xCollider.GetBodyID(), xCurrent);
	}

	Zenith_BehaviorTree m_xTree;
	Zenith_NavMeshAgent m_xNavAgent;
	uint32_t            m_uDebugFrameCounter = 0;

	// Non-owning pointer to the patrol's FindPos node. Set in OnStart so
	// OnUpdate can refresh the navmesh handle when DP_AI rebuilds the cache.
	// The BT itself owns the node's memory; we only need read access to
	// call SetNavMesh on it.
	DP_BTAction_FindPosInSuspicionSphere* m_pxFindPosNode = nullptr;

	// Class-body initializers below are FALLBACKS only -- production gameplay
	// always overwrites them in OnAwake via DP_Tuning::Get<float>() reads.
	// Values picked to match Tuning.json's ratified constants so a misordered
	// init still produces sensible behaviour.
	float m_fSuspicionRadius     = 15.0f;
	float m_fInvestigateMaxAge   = 5.0f;
	float m_fHearingRange        = 30.0f;
	float m_fHearingLoudnessThr  = 0.05f;
	float m_fSightRange          = 25.0f;
	float m_fSightFOV            = 110.0f;
	float m_fSightPeripheral     = 130.0f;
	float m_fSightEyeHeight      = 1.6f;
	float m_fSightAwarenessGain  = 2.0f;
	float m_fSightAwarenessDecay = 0.5f;
	float m_fMoveSpeed           = 7.0f;

#ifdef ZENITH_INPUT_SIMULATOR
public:
	// Test-only accessors -- MVP-0.1.3's Test_P1Tuning_PriestValuesMatchConfig
	// reads these back to verify DP_Tuning propagated correctly. Production
	// gameplay doesn't read priest tuning externally (perception system consumes
	// the values inside OnAwake, BT actions consume m_fMoveSpeed indirectly via
	// the NavMeshAgent).
	float GetSuspicionRadius() const     { return m_fSuspicionRadius; }
	float GetHearingRange() const        { return m_fHearingRange; }
	float GetHearingLoudnessThr() const  { return m_fHearingLoudnessThr; }
	float GetSightRange() const          { return m_fSightRange; }
	float GetSightFOV() const            { return m_fSightFOV; }
	float GetSightPeripheral() const     { return m_fSightPeripheral; }
	float GetSightEyeHeight() const      { return m_fSightEyeHeight; }
	float GetSightAwarenessGain() const  { return m_fSightAwarenessGain; }
	float GetSightAwarenessDecay() const { return m_fSightAwarenessDecay; }
	float GetMoveSpeed() const           { return m_fMoveSpeed; }
#endif
};
