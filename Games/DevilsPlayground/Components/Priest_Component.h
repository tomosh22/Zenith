#pragma once
#include "Core/Zenith_Engine.h"
/**
 * Priest_Component - the priest agent, a SYSTEMS SHIM (W3). Decisions (the
 * reactive Selector apprehend > pursue > investigate > patrol) live in
 * DP_Priest.bgraph, driven by the "PriestTick" event this shim fires from
 * OnUpdate. The graph's NavMoveTo / SetNavDestination nodes push destinations
 * onto the same NavMeshAgent this component holds.
 *
 * Per-frame: bridges Zenith_PerceptionSystem stimuli into the GRAPH blackboard
 * keys (BB.TargetWithDevil + BB.InvestigatePos) before firing "PriestTick".
 *
 * Movement: a Zenith_NavMeshAgent lives on this component and is wired into
 * the Zenith_AIAgentComponent via SetNavMeshAgent. The graph's nav nodes push
 * destinations onto the agent; AIAgentComponent::OnUpdate (auto-dispatched by
 * the lifecycle scheduler each frame) ticks the agent's Update() which walks
 * the path and writes the priest's transform.
 *
 * Optional physics path: if the priest has a Zenith_ColliderComponent with a
 * valid body, each frame we additionally write the agent's desired XZ velocity
 * to the body via g_xEngine.Physics().SetLinearVelocity. Without a collider,
 * the transform-mutation path alone is sufficient for pursuit; the test only
 * asserts distance closes, not how it was applied.
 *
 * Heap-stability: the AIAgentComponent stores a raw pointer to m_xNavAgent
 * (a by-value member), so a pool relocation would dangle it. The hand-written
 * move operations re-wire SetNavMeshAgent(&m_xNavAgent) at the new address.
 */

#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "DataStream/Zenith_DataStream.h"
#include "EntityComponent/Components/Zenith_AIAgentComponent.h"
#include "EntityComponent/Components/Zenith_GraphComponent.h"
#include "Scripting/Zenith_BehaviourGraph.h"
#include "Scripting/Zenith_GraphBlackboard.h"
#include "AI/Navigation/Zenith_NavMeshAgent.h"
#include "AI/Navigation/Zenith_NavMesh.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"
#include "Physics/Zenith_Physics.h"

#include "Source/PublicInterfaces.h"
#include "Source/DP_Tuning.h"

#include <cstring>

class Priest_Component ZENITH_FINAL
{
public:
	// W3 conversion (risk R3): the BT decision body is DP_Priest.bgraph's
	// reactive Selector (apprehend > pursue > investigate > patrol); this
	// component is the SYSTEMS shim - nav agent ownership/wiring, perception
	// registration/config, the perception->blackboard BRIDGE (writes the
	// GRAPH blackboard under the same DP_AI::BB_KEY_* names), door opening,
	// the alert telegraph edges, and the body-velocity guard. It fires
	// "PriestTick" (dt payload) where m_xTree.Tick sat, and
	// "PriestTargetChanged" where m_xTree.Reset sat (the reactive Selector
	// re-scans priorities every tick, so the event is a parity hook - wired
	// to nothing today, kept for a non-reactive fallback).
	static constexpr const char* kszGraphAsset = "game:Graphs/DP_Priest.bgraph";

	Priest_Component() = delete;
	Priest_Component(Zenith_Entity& xParentEntity)
		: m_xParentEntity(xParentEntity)
	{}

	// ---- Graph blackboard plumbing (public: DP_AI's forwarders + the scent
	// ---- fanout read/write the priest's decision inputs through these) ----
	Zenith_BehaviourGraph* FindPriestGraph() const
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
	Zenith_EntityID ReadBBEntity(const char* szVar) const
	{
		Zenith_BehaviourGraph* pxGraph = FindPriestGraph();
		if (pxGraph == nullptr) return INVALID_ENTITY_ID;
		return Zenith_EntityID::FromPacked(pxGraph->GetBlackboard().GetPackedEntityID(szVar,
			INVALID_ENTITY_ID.GetPacked()));
	}
	bool ReadBBBool(const char* szVar, bool bDefault) const
	{
		Zenith_BehaviourGraph* pxGraph = FindPriestGraph();
		return pxGraph ? pxGraph->GetBlackboard().GetBool(szVar, bDefault) : bDefault;
	}
	void WriteBBEntity(const char* szVar, Zenith_EntityID xId)
	{
		if (Zenith_BehaviourGraph* pxGraph = FindPriestGraph())
		{
			Zenith_PropertyValue xValue;
			xValue.SetPackedEntityID(xId.GetPacked());
			pxGraph->GetBlackboard().SetValue(szVar, xValue);
		}
	}
	void WriteBBBool(const char* szVar, bool bValue)
	{
		if (Zenith_BehaviourGraph* pxGraph = FindPriestGraph())
		{
			Zenith_PropertyValue xValue;
			xValue.SetBool(bValue);
			pxGraph->GetBlackboard().SetValue(szVar, xValue);
		}
	}
	void WriteBBFloat(const char* szVar, float fValue)
	{
		if (Zenith_BehaviourGraph* pxGraph = FindPriestGraph())
		{
			Zenith_PropertyValue xValue;
			xValue.SetFloat(fValue);
			pxGraph->GetBlackboard().SetValue(szVar, xValue);
		}
	}
	void WriteBBVector3(const char* szVar, const Zenith_Maths::Vector3& xValue3)
	{
		if (Zenith_BehaviourGraph* pxGraph = FindPriestGraph())
		{
			Zenith_PropertyValue xValue;
			xValue.SetVector3(xValue3);
			pxGraph->GetBlackboard().SetValue(szVar, xValue);
		}
	}

	// Heap-stability: the parent's AIAgentComponent holds a raw pointer to
	// m_xNavAgent — moves must re-wire it at the new address. The BT is
	// move-only anyway; copies deleted.
	Priest_Component(const Priest_Component&) = delete;
	Priest_Component& operator=(const Priest_Component&) = delete;

	Priest_Component(Priest_Component&& xOther) noexcept
		: m_xParentEntity(xOther.m_xParentEntity)
		, m_xNavAgent(std::move(xOther.m_xNavAgent))
		, m_uDebugFrameCounter(xOther.m_uDebugFrameCounter)
		, m_xLastSeenTarget(xOther.m_xLastSeenTarget)
		, m_bLastHadInvestigate(xOther.m_bLastHadInvestigate)
		, m_bLastWithinApprehendRange(xOther.m_bLastWithinApprehendRange)
		, m_fSuspicionRadius(xOther.m_fSuspicionRadius)
		, m_fInvestigateMaxAge(xOther.m_fInvestigateMaxAge)
		, m_fHearingRange(xOther.m_fHearingRange)
		, m_fHearingLoudnessThr(xOther.m_fHearingLoudnessThr)
		, m_fSightRange(xOther.m_fSightRange)
		, m_fSightFOV(xOther.m_fSightFOV)
		, m_fSightPeripheral(xOther.m_fSightPeripheral)
		, m_fSightEyeHeight(xOther.m_fSightEyeHeight)
		, m_fSightAwarenessGain(xOther.m_fSightAwarenessGain)
		, m_fSightAwarenessDecay(xOther.m_fSightAwarenessDecay)
		, m_fMoveSpeed(xOther.m_fMoveSpeed)
		, m_fApprehendRange(xOther.m_fApprehendRange)
	{
		RewireNavAgentPointer(xOther);
	}

	Priest_Component& operator=(Priest_Component&& xOther) noexcept
	{
		if (this != &xOther)
		{
			m_xParentEntity             = xOther.m_xParentEntity;
			m_xNavAgent                 = std::move(xOther.m_xNavAgent);
			m_uDebugFrameCounter        = xOther.m_uDebugFrameCounter;
			m_xLastSeenTarget           = xOther.m_xLastSeenTarget;
			m_bLastHadInvestigate       = xOther.m_bLastHadInvestigate;
			m_bLastWithinApprehendRange = xOther.m_bLastWithinApprehendRange;
			m_fSuspicionRadius          = xOther.m_fSuspicionRadius;
			m_fInvestigateMaxAge        = xOther.m_fInvestigateMaxAge;
			m_fHearingRange             = xOther.m_fHearingRange;
			m_fHearingLoudnessThr       = xOther.m_fHearingLoudnessThr;
			m_fSightRange               = xOther.m_fSightRange;
			m_fSightFOV                 = xOther.m_fSightFOV;
			m_fSightPeripheral          = xOther.m_fSightPeripheral;
			m_fSightEyeHeight           = xOther.m_fSightEyeHeight;
			m_fSightAwarenessGain       = xOther.m_fSightAwarenessGain;
			m_fSightAwarenessDecay      = xOther.m_fSightAwarenessDecay;
			m_fMoveSpeed                = xOther.m_fMoveSpeed;
			m_fApprehendRange           = xOther.m_fApprehendRange;
			RewireNavAgentPointer(xOther);
		}
		return *this;
	}

	void OnAwake()
	{
		// W3: self-attach the decisions graph FIRST so the blackboard seeds
		// below land (idempotent on re-entry).
		{
			Zenith_GraphComponent* pxGraphs = m_xParentEntity.TryGetComponent<Zenith_GraphComponent>();
			if (pxGraphs == nullptr)
			{
				pxGraphs = &m_xParentEntity.AddComponent<Zenith_GraphComponent>();
			}
			bool bAttached = false;
			for (u_int u = 0; u < pxGraphs->GetGraphCount(); ++u)
			{
				if (std::strcmp(pxGraphs->GetGraphAssetPathAt(u), kszGraphAsset) == 0)
				{
					bAttached = true;
					break;
				}
			}
			if (!bAttached)
			{
				pxGraphs->AddGraphByAssetPath(kszGraphAsset);
			}
		}

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
		m_fApprehendRange       = DP_Tuning::Get<float>("priest.apprehend_range_m");

		// Ensure the AIAgent component is on this entity. Scene authoring's
		// `AddStep_AddComponent("AIAgent")` resolves through
		// Zenith_ComponentEditorRegistry, which fails silently when the AIAgent's
		// .cpp is dead-stripped by the linker (its `ZENITH_REGISTER_COMPONENT`
		// static-init never runs because no game symbol references its .obj).
		// Header-inlined methods like `GetBlackboard` aren't enough to anchor
		// the link. Adding the component here guarantees we always have one,
		// regardless of how scene authoring loaded the entity.
		if (!m_xParentEntity.HasComponent<Zenith_AIAgentComponent>())
		{
			m_xParentEntity.AddComponent<Zenith_AIAgentComponent>();
		}

		// MVP-1.2.2: the priest is authored with a DYNAMIC rigid body so
		// NavMeshAgent's SetPosition writes actually land in Jolt (a STATIC
		// body in the NON_MOVING broadphase layer ignores SetPosition's
		// Activate call and stays pinned at its initial pose, which made the
		// priest look frozen even though the BT was correctly emitting
		// transform writes every frame).
		//
		// Two follow-ups required for a DYNAMIC capsule on a top-down map,
		// mirroring the villager pattern:
		//   - Gravity off: the level is a flush slab; we don't want the
		//     priest's body integrating downward into it between SetPosition
		//     writes.
		//   - Lock pitch + roll: the capsule may yaw to face its movement
		//     direction, but a glancing wall hit shouldn't be able to tip it
		//     over. LockRotation zeroes the inverse inertia on the locked
		//     axes so Jolt won't try to integrate them.
		//
		// The collider also opts out of navmesh generation -- the priest's
		// own footprint would otherwise carve a hole in the floor mesh at
		// authoring time, trapping it on a sub-polygon island disconnected
		// from every villager. (SetIncludeInNavMesh is a one-time hint
		// consumed by the generator; it has no per-frame cost.)
		if (Zenith_ColliderComponent* pxCollider = m_xParentEntity.TryGetComponent<Zenith_ColliderComponent>())
		{
			pxCollider->SetIncludeInNavMesh(false);
			if (pxCollider->HasValidBody())
			{
				const Zenith_PhysicsBodyID xBodyID = pxCollider->GetBodyID();
				g_xEngine.Physics().SetGravityEnabled(xBodyID, false);
				g_xEngine.Physics().LockRotation(xBodyID, /*X=*/true, /*Y=*/false, /*Z=*/true);
			}
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
		// W3: the decision inputs live on the GRAPH blackboard now (same
		// DP_AI::BB_KEY_* names, read by DP_Priest.bgraph's Selector pins).
		WriteBBEntity(DP_AI::BB_KEY_TARGET_WITH_DEVIL, INVALID_ENTITY_ID);
		WriteBBBool(DP_AI::BB_KEY_HAS_INVESTIGATE_POS, false);
		WriteBBFloat(DP_AI::BB_KEY_SUSPICION_RADIUS, m_fSuspicionRadius);
	}

	void OnDestroy()
	{
		// Detach the NavMeshAgent from the AIAgentComponent BEFORE m_xNavAgent
		// goes out of scope — the component holds a non-owning pointer that
		// would otherwise dangle when the auto-OnDestroy lifecycle hook tries
		// to use it.
		if (m_xParentEntity.IsValid())
		{
			if (Zenith_AIAgentComponent* pxAgent = m_xParentEntity.TryGetComponent<Zenith_AIAgentComponent>())
			{
				pxAgent->SetNavMeshAgent(nullptr);
			}
		}
		Zenith_PerceptionSystem::UnregisterAgent(m_xParentEntity.GetEntityID());
	}

	// Component contract: version-only payload (the BT + nav agent are
	// rebuilt every OnStart).
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

	void OnStart()
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
			"Priest_Component::OnStart: entity=%u/%u hasAI=%d navmesh=%p",
			m_xParentEntity.GetEntityID().m_uIndex, m_xParentEntity.GetEntityID().m_uGeneration,
			bHasAI ? 1 : 0, (void*)pxNavMesh);

		// W3: the decision body (Selector -> Apprehend / Pursue / Investigate /
		// Patrol) lives on DP_Priest.bgraph, attached in OnAwake and driven by
		// the "PriestTick" event below. The Zenith/AI/BehaviorTree module is
		// gone (deleted at the adoption-program teardown); the graph's reactive
		// Selector re-scans priorities every tick, which subsumes the old
		// m_xTree.Reset() hack.
	}

	void OnUpdate(const float fDt)
	{
		Zenith_AIAgentComponent* pxAgent = m_xParentEntity.TryGetComponent<Zenith_AIAgentComponent>();
		if (pxAgent == nullptr) return;

		// Refresh the cached navmesh pointer. DP_AI's navmesh can be rebuilt
		// at runtime (e.g., a test setup that adds scene geometry then calls
		// DP_AI::ResetLevelNavMesh), invalidating the pointer the agent
		// captured during OnStart. (The graph's patrol picker resolves the
		// live cache per Execute, so only the agent needs the re-pull.)
		const Zenith_NavMesh* pxLiveNavMesh = DP_AI::GetOrBuildLevelNavMesh();
		if (pxLiveNavMesh != m_xNavAgent.GetNavMesh())
		{
			m_xNavAgent.SetNavMesh(pxLiveNavMesh);
		}

		BridgePerceptionToBlackboard();

		// 2026-05-25 v4: priest opens any closed unlocked door it's
		// adjacent to, regardless of BT branch (Idle / Patrol /
		// Investigate / Pursue / Apprehend). User-confirmed via
		// telemetry that pursuit-gating left the priest stuck in
		// its spawn room (intent=Patrol for 200 s, position
		// unchanged after the first 0.8 s) because patrol never
		// crosses a closed door and pursuit only fires on LOS
		// contact -- which can't happen if the priest can't leave
		// the spawn room.
		//
		// Locked doors still reject the priest (it doesn't hold a
		// Key), so the player's pent-side locks remain a hard
		// gate; the priest cannot follow the player into the pent
		// room without the same Key economy the player navigates.
		// Threat curve: priest now roams the non-pent map freely,
		// which is the same reach it had pre-PR (when doors lived
		// at corridor midpoints and the wall gaps were always
		// open -- the priest's pre-PR navmesh was the union of
		// every room and corridor).
		if (Zenith_TransformComponent* pxDoorTransform = m_xParentEntity.TryGetComponent<Zenith_TransformComponent>())
		{
			Zenith_Maths::Vector3 xPriestPosForDoors;
			pxDoorTransform->GetPosition(xPriestPosForDoors);
			DP_AI::OpenNearbyDoorsFor(m_xParentEntity.GetEntityID(), xPriestPosForDoors);
		}

		// W3: the graph's Selector is REACTIVE - it re-scans from pin 0 every
		// tick, so the retired memory-selector Reset() hack is structurally
		// unnecessary. "PriestTargetChanged" still fires on the same edge
		// (INVALID->A and direct A->B) as a parity hook: telemetry/debug can
		// observe it, and a future non-reactive fallback wires it to an
		// abort chain. The graph currently has no consumer for it.
		const Zenith_EntityID xCurrentTarget = ReadBBEntity(DP_AI::BB_KEY_TARGET_WITH_DEVIL);
		if (xCurrentTarget.IsValid() && xCurrentTarget != m_xLastSeenTarget)
		{
			if (Zenith_GraphComponent* pxGraphs = m_xParentEntity.TryGetComponent<Zenith_GraphComponent>())
			{
				pxGraphs->FireCustomEvent("PriestTargetChanged");
			}
		}

		// 2026-05-21: in-world alert telegraph. The priest's BT can shift
		// into Investigate / Pursue / Apprehend mid-frame; previously the
		// only feedback to the player was the HUD's text indicator (and
		// sometimes that indicator was placeholder text). DP_OnPriestAlerted
		// fires on rising-edge of each transition so the particles system
		// can paint a "!" above the priest's head and the HUD can raise
		// the awareness icon to the right state.
		//
		// Falling edge (target lost, priest returns to patrol) is NOT
		// emitted; the particles + HUD auto-clear on their own timers.
		Zenith_Maths::Vector3 xPriestPos(0.0f);
		if (Zenith_TransformComponent* pxPriestTransform = m_xParentEntity.TryGetComponent<Zenith_TransformComponent>())
		{
			pxPriestTransform->GetPosition(xPriestPos);
		}

		// Priority order: Apprehend > Pursue > Investigate. We only fire
		// the highest-priority NEW alert this frame, even if multiple
		// stimuli rose at once -- otherwise three particle bursts stack
		// onto each other and read as noise rather than the cleaner
		// "the priest just noticed you" signal.
		const bool bHasInvestigate = ReadBBBool(DP_AI::BB_KEY_HAS_INVESTIGATE_POS, false);
		bool bWithinApprehendRange = false;
		if (xCurrentTarget.IsValid() && m_xParentEntity.HasComponent<Zenith_TransformComponent>())
		{
			Zenith_Entity xTgt = g_xEngine.Scenes().ResolveEntity(xCurrentTarget);
			if (xTgt.IsValid())
			{
				if (Zenith_TransformComponent* pxTgtTransform = xTgt.TryGetComponent<Zenith_TransformComponent>())
				{
					Zenith_Maths::Vector3 xTgtPos;
					pxTgtTransform->GetPosition(xTgtPos);
					const float fDx = xTgtPos.x - xPriestPos.x;
					const float fDz = xTgtPos.z - xPriestPos.z;
					bWithinApprehendRange = (fDx*fDx + fDz*fDz)
						<= (m_fApprehendRange * m_fApprehendRange);
				}
			}
		}

		const Zenith_EntityID xSelfId = m_xParentEntity.GetEntityID();
		if (bWithinApprehendRange && !m_bLastWithinApprehendRange)
		{
			Zenith_EventDispatcher::Get().Dispatch(
				DP_OnPriestAlerted{
					xSelfId, DP_PriestAlertKind::WithinRange, xPriestPos });
		}
		else if (xCurrentTarget.IsValid() && !m_xLastSeenTarget.IsValid())
		{
			Zenith_EventDispatcher::Get().Dispatch(
				DP_OnPriestAlerted{
					xSelfId, DP_PriestAlertKind::SawTarget, xPriestPos });
		}
		else if (bHasInvestigate && !m_bLastHadInvestigate)
		{
			Zenith_EventDispatcher::Get().Dispatch(
				DP_OnPriestAlerted{
					xSelfId, DP_PriestAlertKind::HeardNoise, xPriestPos });
		}

		m_xLastSeenTarget          = xCurrentTarget;
		m_bLastHadInvestigate      = bHasInvestigate;
		m_bLastWithinApprehendRange = bWithinApprehendRange;

		// W3: drive the decisions graph where m_xTree.Tick sat - AFTER the
		// bridge wrote this frame's perception facts, so decisions see fresh
		// data. NOTE the graph's suspended Selector chain is ALSO re-driven
		// by the engine's ON_UPDATE dispatch (GraphComponent order 60, real
		// dt) each frame; this custom-event drive carries dt 0 in its
		// context, so timers accumulate exactly once per frame (on the
		// ON_UPDATE drive) while the decisions settle on the freshest
		// bridge data (this drive). The dt payload is stashed to "dt" for
		// any chain that needs it explicitly.
		{
			if (Zenith_GraphComponent* pxGraphs = m_xParentEntity.TryGetComponent<Zenith_GraphComponent>())
			{
				Zenith_PropertyValue xDtPayload;
				xDtPayload.SetFloat(fDt);
				pxGraphs->FireCustomEvent("PriestTick", &xDtPayload);
			}
		}

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
			const Zenith_EntityID xTgt = ReadBBEntity(DP_AI::BB_KEY_TARGET_WITH_DEVIL);
			const Zenith_NavMeshAgent* pxNav = pxAgent->GetNavMeshAgent();
			Zenith_Maths::Vector3 xPos(0.0f);
			if (Zenith_TransformComponent* pxTransform = m_xParentEntity.TryGetComponent<Zenith_TransformComponent>())
			{
				pxTransform->GetPosition(xPos);
			}
			Zenith_Log(LOG_CATEGORY_AI,
				"Priest_Component::OnUpdate frame=%u tgt=(%u/%u) navAgent=%p hasPath=%d pos=(%.1f,%.1f,%.1f)",
				m_uDebugFrameCounter,
				xTgt.m_uIndex, xTgt.m_uGeneration,
				(void*)pxNav, pxNav ? (pxNav->HasPath() ? 1 : 0) : -1,
				xPos.x, xPos.y, xPos.z);
		}
	}

private:
	// Move-helper: the parent's AIAgentComponent may hold a pointer to the
	// SOURCE's m_xNavAgent (wired in OnStart). After the member moves into
	// this instance, re-point the agent at the new address. Only re-wires
	// when the agent was actually pointing at the source's member, so a
	// pre-OnStart move stays a no-op.
	void RewireNavAgentPointer(Priest_Component& xOther)
	{
		if (!m_xParentEntity.IsValid()) return;
		Zenith_AIAgentComponent* pxAgent = m_xParentEntity.TryGetComponent<Zenith_AIAgentComponent>();
		if (pxAgent == nullptr) return;
		if (pxAgent->GetNavMeshAgent() == &xOther.m_xNavAgent)
		{
			pxAgent->SetNavMeshAgent(&m_xNavAgent);
		}
	}

	void BridgePerceptionToBlackboard()
	{
		const Zenith_EntityID xSelf = m_xParentEntity.GetEntityID();

		// TargetWithDevil: only set when a perceived villager is currently possessed.
		Zenith_EntityID xTargetWithDevil = INVALID_ENTITY_ID;
		const Zenith_Vector<Zenith_PerceivedTarget>* paxPerceived =
			Zenith_PerceptionSystem::GetPerceivedTargets(xSelf);
		if (paxPerceived != nullptr)
		{
			for (uint32_t i = 0; i < paxPerceived->GetSize(); ++i)
			{
				const Zenith_EntityID xCandidate = paxPerceived->Get(i).m_xEntityID;
				if (!DP_Player::IsPossessedVillager(xCandidate)) continue;
				// MVP-2.1.6: Beggar archetype filter. Even when
				// possessed AND directly in sight, a Beggar is never
				// the priest's target. (Future Variant aelfrics can
				// override this -- their bridges will pick whichever
				// archetype filter they care about; the canonical
				// MVP Aelfric just skips Beggar.)
				if (DP_Player::IsBeggarVillager(xCandidate)) continue;
				xTargetWithDevil = xCandidate;
				break;
			}
		}
		// MVP-1.9 cleanup (2026-05-19): the priest is sight-driven
		// only. The historical "omniscient fallback" -- which wrote
		// the possessed villager into BB_KEY_TARGET_WITH_DEVIL when
		// perception found nothing -- was a test backdoor that leaked
		// into every build because ZENITH_INPUT_SIMULATOR is defined
		// unconditionally in this codebase. With it gone, tests that
		// need the priest to acquire a target call
		// Zenith_PerceptionSystem::RegisterTarget(villager, hostile)
		// and position the villager inside the priest's sight cone --
		// exercising the same code path production gameplay uses.
		WriteBBEntity(DP_AI::BB_KEY_TARGET_WITH_DEVIL, xTargetWithDevil);

		// EXT-6: typed sound accessor. Bridge the freshest hearing stimulus
		// (delivered via DP_AI::EmitNoise → PerceptionSystem) into the graph's
		// investigate-pos slot. The investigate chain's SetBlackboardBool
		// resets HasInvestigatePos after the wait completes.
		const Zenith_PerceptionSystem::Zenith_LastHeardSound xHeard
			= Zenith_PerceptionSystem::GetLastHeardSoundFor(xSelf);
		if (xHeard.m_bValid && xHeard.m_fAge < m_fInvestigateMaxAge)
		{
			WriteBBVector3(DP_AI::BB_KEY_INVESTIGATE_POS, xHeard.m_xPosition);
			WriteBBBool(DP_AI::BB_KEY_HAS_INVESTIGATE_POS, true);
		}
	}

	void ApplyVelocityToBodyIfPresent()
	{
		// MVP-1.2.2 originally zeroed the body's linearVelocity every frame so
		// the physics step couldn't drift the body past the position the nav
		// agent wrote. That was safe when this logic lived on the old script
		// host (dispatch order 60, BEFORE the AIAgent's nav tick at 90); as a
		// game component (order 109) it now runs AFTER the nav tick, and the
		// engine NavMeshAgent drives a dynamic body via SetLinearVelocity --
		// an unconditional zero here erases the velocity the nav tick just
		// wrote and freezes the priest. Only zero when the agent is NOT
		// path-following (kills residual slide after a path completes/aborts).
		Zenith_ColliderComponent* pxCollider = m_xParentEntity.TryGetComponent<Zenith_ColliderComponent>();
		if (pxCollider == nullptr) return;
		if (!pxCollider->HasValidBody()) return;
		if (Zenith_AIAgentComponent* pxAgent = m_xParentEntity.TryGetComponent<Zenith_AIAgentComponent>())
		{
			const Zenith_NavMeshAgent* pxNav = pxAgent->GetNavMeshAgent();
			if (pxNav != nullptr && pxNav->HasPath()) return;
		}
		g_xEngine.Physics().SetLinearVelocity(pxCollider->GetBodyID(),
			Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	}

	Zenith_Entity       m_xParentEntity;

	Zenith_NavMeshAgent m_xNavAgent;
	uint32_t            m_uDebugFrameCounter = 0;

	// Edge memory for the "PriestTargetChanged" parity event (fires on
	// INVALID->A and direct A->B target changes, where the retired
	// m_xTree.Reset() hack sat).
	Zenith_EntityID     m_xLastSeenTarget = INVALID_ENTITY_ID;

	// 2026-05-21: rising-edge tracking for DP_OnPriestAlerted dispatch.
	// The HUD + particles system both subscribe; firing on every frame
	// the priest is in Investigate / Pursue / Apprehend would spam the
	// burst counter and stack particle clouds. We fire ONCE per
	// transition into a higher-priority branch.
	bool                m_bLastHadInvestigate       = false;
	bool                m_bLastWithinApprehendRange = false;

	// Class-body initializers below are FALLBACKS only -- production gameplay
	// always overwrites them in OnAwake via DP_Tuning::Get<float>() reads.
	// Values picked to match Tuning.json's ratified constants so a misordered
	// init still produces sensible behaviour.
	float m_fSuspicionRadius     = 15.0f;
	float m_fInvestigateMaxAge   = 5.0f;
	float m_fHearingRange        = 25.0f;
	float m_fHearingLoudnessThr  = 0.05f;
	float m_fSightRange          = 17.0f;
	float m_fSightFOV            = 90.0f;
	float m_fSightPeripheral     = 130.0f;
	float m_fSightEyeHeight      = 1.6f;
	float m_fSightAwarenessGain  = 2.0f;
	float m_fSightAwarenessDecay = 0.5f;
	float m_fMoveSpeed           = 4.5f;
	float m_fApprehendRange      = 2.0f;

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
