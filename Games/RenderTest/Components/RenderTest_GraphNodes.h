#pragma once
#include "Core/Zenith_Engine.h"

/**
 * RenderTest_GraphNodes - RenderTest's Behaviour Graph node library (W3).
 *
 * TENNIS: each RTTennis* node wraps EXACTLY the body of the retired BT leaf it
 * replaced (RenderTest_TennisBTNodes.h, deleted with the conversion) - same
 * decision-core calls, same guards, same early-SUCCESS paths - with blackboard
 * reads redirected at the GRAPH blackboard the referee now publishes into.
 * The BT-condition gates that only read blackboard state (phase / IsServer /
 * IsMyBall / ServeBallParked) decomposed into engine CompareBlackboardInt +
 * Gate nodes inside RenderTest_TennisBrain.bgraph; what remains here are the
 * verbs that touch the brain/body/nav/perception systems.
 *
 * RNG-DETERMINISM CONTRACT (pinned by RT_TennisDeterminismDigest): the decide
 * nodes draw from the brain's per-side TennisRng ONLY when un-armed, exactly
 * as the BT decide leaves did; the graph's tick chain reproduces the AIAgent
 * accumulator (accumulate-while-enabled, fire at >= 0.08, reset-to-zero) so
 * the draw cadence is bit-identical to the BT baseline.
 *
 * PLAYER: the discrete-press decisions (E interact, R reload, LMB fire,
 * T tennis-cam cycle) dispatched by RenderTest_PlayerActions.bgraph. The
 * continuous holds (WASD / Shift / Space / RMB) and every system (movement
 * integration, IK, anim layers, hitscan) stay C++ in the components.
 *
 * Registered from Project_RegisterGameComponents via
 * RenderTest_RegisterGraphNodes().
 */

#include "Scripting/Zenith_GraphNode.h"
#include "Scripting/Zenith_GraphNodeRegistry.h"
#include "Scripting/Zenith_GraphBlackboard.h"
#include "Scripting/Zenith_BehaviourGraph.h"
#include "ZenithECS/Zenith_Entity.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_AIAgentComponent.h"
#include "AI/Navigation/Zenith_NavMeshAgent.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"
#include "Physics/Zenith_Physics.h"

#include "RenderTest/Components/RenderTest_TennisAgentComponent.h"
#include "RenderTest/Components/RenderTest_TennisPlayerComponent.h"
#include "RenderTest/Components/RenderTest_TennisDecision.h"
#include "RenderTest/Components/RenderTest_PlayerComponent.h"
#include "RenderTest/Components/RenderTest_FollowCameraComponent.h"

//==============================================================================
// Shared tennis helpers (moved VERBATIM from the deleted
// RenderTest_TennisBTNodes.h leaf base; blackboard reads now target the graph
// blackboard in the node context).
//==============================================================================
namespace RenderTest_TennisNodes
{
	inline RenderTest_TennisAgentComponent* Brain(Zenith_GraphContext& xContext)
	{
		return xContext.m_xSelf.TryGetComponent<RenderTest_TennisAgentComponent>();
	}
	inline RenderTest_TennisPlayerComponent* Body(Zenith_GraphContext& xContext)
	{
		return xContext.m_xSelf.TryGetComponent<RenderTest_TennisPlayerComponent>();
	}
	inline Zenith_NavMeshAgent* Nav(Zenith_GraphContext& xContext)
	{
		if (Zenith_AIAgentComponent* pxAgent = xContext.m_xSelf.TryGetComponent<Zenith_AIAgentComponent>())
			return pxAgent->GetNavMeshAgent();
		return nullptr;
	}
	inline Zenith_Maths::Vector3 SelfPos(Zenith_GraphContext& xContext)
	{
		Zenith_Maths::Vector3 xPos(0.0f);
		if (Zenith_TransformComponent* pxTransform = xContext.m_xSelf.TryGetComponent<Zenith_TransformComponent>())
			pxTransform->GetPosition(xPos);
		return xPos;
	}
	inline RenderTest_Tennis::TennisCourt Court() { return RenderTest_Tennis::DefaultCourt(); }

	// Resolve the live ball pose + velocity (spin comes from the blackboard the
	// referee publishes). Returns false if the ball can't be resolved. The
	// entity key defaults to the INVALID sentinel when unpublished, matching
	// the BT blackboard's GetEntityID miss semantics (packed 0 is never
	// written - seed and publish both skip invalid handles).
	inline bool BallState(Zenith_GraphContext& xContext, Zenith_Maths::Vector3& xPos,
		Zenith_Maths::Vector3& xVel, Zenith_Maths::Vector3& xSpin)
	{
		xSpin = xContext.m_pxBlackboard->GetVector3(RenderTest_TennisBB::k_szBallSpin, Zenith_Maths::Vector3(0.0f));
		const Zenith_EntityID xBall = Zenith_EntityID::FromPacked(
			xContext.m_pxBlackboard->GetPackedEntityID(RenderTest_TennisBB::k_szBallEntity,
				INVALID_ENTITY_ID.GetPacked()));
		if (xBall == INVALID_ENTITY_ID)
			return false;
		Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(xBall);
		Zenith_TransformComponent* pxTransform = xEnt.TryGetComponent<Zenith_TransformComponent>();
		if (pxTransform == nullptr)
			return false;
		pxTransform->GetPosition(xPos);
		xVel = Zenith_Maths::Vector3(0.0f);
		if (Zenith_ColliderComponent* pxCol = xEnt.TryGetComponent<Zenith_ColliderComponent>())
		{
			if (pxCol->HasValidBody())
				xVel = g_xEngine.Physics().GetLinearVelocity(pxCol->GetBodyID());
		}
		return true;
	}

	// The X-stance for a serve from the parity-selected court (deuce/ad).
	inline float ServeStanceX(const RenderTest_Tennis::TennisCourt& xCourt, int iSide, bool bDeuce)
	{
		const float fServerDeuceSign = (iSide == RenderTest_Tennis::TENNIS_SIDE_NEAR) ? 1.0f : -1.0f;
		const float fStanceSign = bDeuce ? fServerDeuceSign : -fServerDeuceSign;
		return xCourt.m_fCenterX + fStanceSign * (xCourt.m_fSinglesHalfWidth * 0.5f);
	}

	inline void SetNavDest(Zenith_GraphContext& xContext, const Zenith_Maths::Vector3& xDest)
	{
		if (Zenith_NavMeshAgent* pxNav = Nav(xContext))
			pxNav->SetDestination(xDest);
	}

	inline constexpr float k_fSeenThreshold = 0.25f;
	// Additive reachability slack in PredictIntercept (HorizDist <= runSpeed*t + this).
	// DEVIATES from the plan's k_fReachRadius=1.2: widened to 2.5 so agents commit to
	// chase more balls and rallies emerge (at 1.2 the receiver too often judged a ball
	// unreachable and let it pass). The contact proximity gate still rejects a swing that
	// doesn't reach the ball, so this only affects WHICH balls are chased.
	inline constexpr float k_fReachRadius   = 2.5f;
	inline constexpr float k_fRunSpeed      = 6.0f;    // matches the nav agent move speed
	// DEVIATES from the plan's k_fContactLead=0.40: 0.6 starts the swing slightly earlier
	// so the anim's contact frame aligns with the ball arrival given the stroke length.
	inline constexpr float k_fArmLeadWindow = 0.6f;    // start the swing this far before the strike
	// Erosion for nav destinations (plan's "eroded slab"): keep targets this far inside
	// the slab edge (~agent radius + clearance) so SetDestination doesn't fail-stop at
	// the very boundary of the generated navmesh.
	inline constexpr float k_fNavDestMargin = 1.0f;
}

//==============================================================================
// Tennis brain nodes (RenderTest_TennisBrain.bgraph)
//==============================================================================

// Reproduces the AIAgent enable gate (Zenith_AIAgentComponent::OnUpdate's
// `if (!m_bEnabled) return;`): the referee parks agents outside SERVING/LIVE
// via SetEnabled, which froze the BT tick accumulator without resetting it.
// FAILURE stops the tick chain BEFORE the accumulator adds this frame's dt,
// so the graph accumulator freezes identically. SetEnabled only mutates in
// the referee's OnLateUpdate, so the value read here (order 60) is the value
// the AIAgent itself would have seen at order 90.
class RTNode_TennisTickGate : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Zenith_AIAgentComponent* pxAgent = xContext.m_xSelf.TryGetComponent<Zenith_AIAgentComponent>();
		if (pxAgent == nullptr || !pxAgent->IsEnabled())
			return GRAPH_NODE_STATUS_FAILURE;
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "RTTennisTickGate"; }
};

// ---- Serve branch ----------------------------------------------------------

class RTNode_TennisDecideServe : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		RenderTest_TennisAgentComponent* pxBrain = RenderTest_TennisNodes::Brain(xContext);
		if (!pxBrain)
			return GRAPH_NODE_STATUS_FAILURE;
		if (pxBrain->IsArmed())
			return GRAPH_NODE_STATUS_SUCCESS;   // keep the committed decision (don't unarm mid-swing)
		const bool bDeuce = xContext.m_pxBlackboard->GetBool(RenderTest_TennisBB::k_szServeFromDeuce, true);
		const bool bSecond = xContext.m_pxBlackboard->GetBool(RenderTest_TennisBB::k_szIsSecondServe, false);
		const RenderTest_Tennis::TennisShotDecision xDec = RenderTest_Tennis::SelectServe(
			RenderTest_TennisNodes::Court(), pxBrain->GetPlayerState(),
			RenderTest_Tennis::SERVE_RESULT_GOOD, bDeuce, bSecond, pxBrain->Rng());
		pxBrain->SetDecidedShot(xDec);   // stored UNARMED
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "RTTennisDecideServe"; }
};

class RTNode_TennisPositionForServe : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		const RenderTest_Tennis::TennisCourt xCourt = RenderTest_TennisNodes::Court();
		const int iSide = xContext.m_pxBlackboard->GetInt32(RenderTest_TennisBB::k_szMySide, 0);
		const bool bDeuce = xContext.m_pxBlackboard->GetBool(RenderTest_TennisBB::k_szServeFromDeuce, true);
		const float fStanceX = RenderTest_TennisNodes::ServeStanceX(xCourt, iSide, bDeuce);
		const Zenith_Maths::Vector3 xDest = RenderTest_Tennis::ProjectToSlab(xCourt,
			Zenith_Maths::Vector3(fStanceX, xCourt.m_fSurfaceY, xCourt.BaselineZ(
				static_cast<RenderTest_Tennis::TennisSide>(iSide))),
			RenderTest_TennisNodes::k_fNavDestMargin);
		RenderTest_TennisNodes::SetNavDest(xContext, xDest);
		if (RenderTest_TennisPlayerComponent* pxBody = RenderTest_TennisNodes::Body(xContext))
			pxBody->SetFootworkTargetX(xDest.x);
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "RTTennisPositionForServe"; }
};

class RTNode_TennisArmServe : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		RenderTest_TennisAgentComponent* pxBrain = RenderTest_TennisNodes::Brain(xContext);
		RenderTest_TennisPlayerComponent* pxBody = RenderTest_TennisNodes::Body(xContext);
		if (!pxBrain || !pxBody)
			return GRAPH_NODE_STATUS_FAILURE;
		if (pxBrain->IsArmed())
			return GRAPH_NODE_STATUS_SUCCESS;     // already committed this ball
		if (!pxBody->IsReady())
			return GRAPH_NODE_STATUS_SUCCESS;     // a stroke is in progress
		// Arm only if the body confirms the stroke actually started.
		if (pxBody->RequestServe(pxBrain->GetDecidedShot().m_xAim))
			pxBrain->ArmDecidedShot(static_cast<u_int>(
				xContext.m_pxBlackboard->GetInt32(RenderTest_TennisBB::k_szBallEpoch, 0)));
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "RTTennisArmServe"; }
};

// ---- Return / rally branch --------------------------------------------------

// The systems half of the retired BallIsMine condition (its pure blackboard
// gates - phase LIVE + IsMyBall - are engine Compare/Gate nodes in the graph):
// aware enough of the ball (specific-ID query, never GetPrimaryTarget) AND the
// ball descends to my strike plane on my side within reach.
class RTNode_TennisBallReachable : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		const Zenith_EntityID xBall = Zenith_EntityID::FromPacked(
			xContext.m_pxBlackboard->GetPackedEntityID(RenderTest_TennisBB::k_szBallEntity,
				INVALID_ENTITY_ID.GetPacked()));
		const float fAware = Zenith_PerceptionSystem::GetAwarenessOf(
			xContext.m_xSelf.GetEntityID(), xBall);

		Zenith_Maths::Vector3 xBallPos, xBallVel, xBallSpin;
		if (!RenderTest_TennisNodes::BallState(xContext, xBallPos, xBallVel, xBallSpin))
			return GRAPH_NODE_STATUS_FAILURE;
		const RenderTest_Tennis::TennisCourt xCourt = RenderTest_TennisNodes::Court();
		const RenderTest_Tennis::TennisSide eSide = static_cast<RenderTest_Tennis::TennisSide>(
			xContext.m_pxBlackboard->GetInt32(RenderTest_TennisBB::k_szMySide, 0));
		const RenderTest_Tennis::TennisInterceptResult xHit = RenderTest_Tennis::PredictIntercept(
			xCourt, xBallPos, xBallVel, xBallSpin, eSide, RenderTest_Tennis::StrikeHeight(xCourt),
			RenderTest_TennisNodes::k_fReachRadius, RenderTest_TennisNodes::SelfPos(xContext),
			RenderTest_TennisNodes::k_fRunSpeed);
		if (fAware < RenderTest_TennisNodes::k_fSeenThreshold)
			return GRAPH_NODE_STATUS_FAILURE;
		return xHit.m_bReachable ? GRAPH_NODE_STATUS_SUCCESS : GRAPH_NODE_STATUS_FAILURE;
	}
	const char* GetTypeName() const override { return "RTTennisBallReachable"; }
};

class RTNode_TennisMoveToIntercept : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Zenith_Maths::Vector3 xBallPos, xBallVel, xBallSpin;
		if (!RenderTest_TennisNodes::BallState(xContext, xBallPos, xBallVel, xBallSpin))
			return GRAPH_NODE_STATUS_SUCCESS;   // quirk preserved: no ball -> no-op SUCCESS
		const RenderTest_Tennis::TennisCourt xCourt = RenderTest_TennisNodes::Court();
		const RenderTest_Tennis::TennisSide eSide = static_cast<RenderTest_Tennis::TennisSide>(
			xContext.m_pxBlackboard->GetInt32(RenderTest_TennisBB::k_szMySide, 0));
		const RenderTest_Tennis::TennisInterceptResult xHit = RenderTest_Tennis::PredictIntercept(
			xCourt, xBallPos, xBallVel, xBallSpin, eSide, RenderTest_Tennis::StrikeHeight(xCourt),
			RenderTest_TennisNodes::k_fReachRadius, RenderTest_TennisNodes::SelfPos(xContext),
			RenderTest_TennisNodes::k_fRunSpeed);

		Zenith_Maths::Vector3 xTarget = xHit.m_bReachable ? xHit.m_xStrikePoint : xBallPos;
		xTarget.y = xCourt.m_fSurfaceY;
		const Zenith_Maths::Vector3 xDest = RenderTest_Tennis::ProjectToSlab(xCourt, xTarget,
			RenderTest_TennisNodes::k_fNavDestMargin);
		RenderTest_TennisNodes::SetNavDest(xContext, xDest);
		if (RenderTest_TennisPlayerComponent* pxBody = RenderTest_TennisNodes::Body(xContext))
			pxBody->SetFootworkTargetX(xDest.x);   // footwork fallback when nav is invalid
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "RTTennisMoveToIntercept"; }
};

class RTNode_TennisDecideShot : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		RenderTest_TennisAgentComponent* pxBrain = RenderTest_TennisNodes::Brain(xContext);
		if (!pxBrain)
			return GRAPH_NODE_STATUS_FAILURE;
		if (pxBrain->IsArmed())
			return GRAPH_NODE_STATUS_SUCCESS;   // keep the committed decision (don't unarm mid-swing)
		pxBrain->SetDecidedShot(RenderTest_Tennis::SelectShot(
			RenderTest_TennisNodes::Court(), pxBrain->GetPlayerState(), pxBrain->Rng()));
		return GRAPH_NODE_STATUS_SUCCESS;   // stored UNARMED
	}
	const char* GetTypeName() const override { return "RTTennisDecideShot"; }
};

class RTNode_TennisArmSwing : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		RenderTest_TennisAgentComponent* pxBrain = RenderTest_TennisNodes::Brain(xContext);
		RenderTest_TennisPlayerComponent* pxBody = RenderTest_TennisNodes::Body(xContext);
		if (!pxBrain || !pxBody)
			return GRAPH_NODE_STATUS_FAILURE;
		if (pxBrain->IsArmed())
			return GRAPH_NODE_STATUS_SUCCESS;
		if (!pxBody->IsReady())
			return GRAPH_NODE_STATUS_SUCCESS;

		Zenith_Maths::Vector3 xBallPos, xBallVel, xBallSpin;
		if (!RenderTest_TennisNodes::BallState(xContext, xBallPos, xBallVel, xBallSpin))
			return GRAPH_NODE_STATUS_SUCCESS;
		const RenderTest_Tennis::TennisCourt xCourt = RenderTest_TennisNodes::Court();
		const RenderTest_Tennis::TennisSide eSide = static_cast<RenderTest_Tennis::TennisSide>(
			xContext.m_pxBlackboard->GetInt32(RenderTest_TennisBB::k_szMySide, 0));
		const RenderTest_Tennis::TennisInterceptResult xHit = RenderTest_Tennis::PredictIntercept(
			xCourt, xBallPos, xBallVel, xBallSpin, eSide, RenderTest_Tennis::StrikeHeight(xCourt),
			RenderTest_TennisNodes::k_fReachRadius, RenderTest_TennisNodes::SelfPos(xContext),
			RenderTest_TennisNodes::k_fRunSpeed);

		// Start the swing only inside the contact-lead window so the anim contact
		// frame lines up with the ball arriving. Arm only if the body confirms it.
		if (xHit.m_bReachable && xHit.m_fTimeToStrike <= RenderTest_TennisNodes::k_fArmLeadWindow)
		{
			if (pxBody->RequestSwing(pxBrain->GetDecidedShot().m_xAim, xBallPos.x))
				pxBrain->ArmDecidedShot(static_cast<u_int>(
					xContext.m_pxBlackboard->GetInt32(RenderTest_TennisBB::k_szBallEpoch, 0)));
		}
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "RTTennisArmSwing"; }
};

// ---- Recover fallback --------------------------------------------------------

class RTNode_TennisRecoverToReady : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		const RenderTest_Tennis::TennisCourt xCourt = RenderTest_TennisNodes::Court();
		const int iSide = xContext.m_pxBlackboard->GetInt32(RenderTest_TennisBB::k_szMySide, 0);
		// A receiver awaiting a serve stands up near the service line; everyone else holds
		// the baseline (see ComputeReadyZ - keeps the gently-paced serve returnable).
		const float fReadyZ = RenderTest_Tennis::ComputeReadyZ(xCourt,
			static_cast<RenderTest_Tennis::TennisSide>(iSide),
			static_cast<RenderTest_Tennis::PointPhase>(
				xContext.m_pxBlackboard->GetInt32(RenderTest_TennisBB::k_szPhase, 0)),
			xContext.m_pxBlackboard->GetBool(RenderTest_TennisBB::k_szIsServer, false));
		const Zenith_Maths::Vector3 xDest = RenderTest_Tennis::ProjectToSlab(xCourt,
			Zenith_Maths::Vector3(xCourt.m_fCenterX, xCourt.m_fSurfaceY, fReadyZ),
			RenderTest_TennisNodes::k_fNavDestMargin);
		RenderTest_TennisNodes::SetNavDest(xContext, xDest);
		if (RenderTest_TennisPlayerComponent* pxBody = RenderTest_TennisNodes::Body(xContext))
			pxBody->SetFootworkTargetX(xDest.x);
		return GRAPH_NODE_STATUS_SUCCESS;   // always succeeds (fallback)
	}
	const char* GetTypeName() const override { return "RTTennisRecoverToReady"; }
};

//==============================================================================
// Player action nodes (RenderTest_PlayerActions.bgraph)
//==============================================================================

class RTNode_PlayerInteractGun : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		RenderTest_PlayerComponent* pxPlayer = xContext.m_xSelf.TryGetComponent<RenderTest_PlayerComponent>();
		if (pxPlayer == nullptr)
			return GRAPH_NODE_STATUS_FAILURE;
		pxPlayer->TryInteractGun();
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "RTPlayerInteractGun"; }
};

class RTNode_PlayerTryReload : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		RenderTest_PlayerComponent* pxPlayer = xContext.m_xSelf.TryGetComponent<RenderTest_PlayerComponent>();
		if (pxPlayer == nullptr)
			return GRAPH_NODE_STATUS_FAILURE;
		pxPlayer->TryStartReload();
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "RTPlayerTryReload"; }
};

class RTNode_PlayerTryFire : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		RenderTest_PlayerComponent* pxPlayer = xContext.m_xSelf.TryGetComponent<RenderTest_PlayerComponent>();
		if (pxPlayer == nullptr)
			return GRAPH_NODE_STATUS_FAILURE;
		pxPlayer->TryFire();
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "RTPlayerTryFire"; }
};

class RTNode_PlayerCycleTennisCam : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& /*xContext*/) override
	{
		// The follow camera lives on the GameManager entity, not the player -
		// find it in the active scene (single instance by construction).
		RenderTest_FollowCameraComponent* pxCamera = nullptr;
		Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
		if (xScene.IsValid())
		{
			if (Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene))
			{
				pxSceneData->Query<RenderTest_FollowCameraComponent>().ForEach(
					[&](Zenith_EntityID, RenderTest_FollowCameraComponent& xCam)
					{
						if (pxCamera == nullptr) pxCamera = &xCam;
					});
			}
		}
		if (pxCamera == nullptr)
			return GRAPH_NODE_STATUS_FAILURE;
		pxCamera->CycleTennisCameraMode();
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "RTPlayerCycleTennisCam"; }
};

//==============================================================================
// Registration
//==============================================================================
inline void RenderTest_RegisterGraphNodes()
{
	Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
	xRegistry.RegisterNodeType<RTNode_TennisTickGate>("RTTennisTickGate", GRAPH_EVENT_NONE, 1, false, "RenderTest");
	xRegistry.RegisterNodeType<RTNode_TennisDecideServe>("RTTennisDecideServe", GRAPH_EVENT_NONE, 1, false, "RenderTest");
	xRegistry.RegisterNodeType<RTNode_TennisPositionForServe>("RTTennisPositionForServe", GRAPH_EVENT_NONE, 1, false, "RenderTest");
	xRegistry.RegisterNodeType<RTNode_TennisArmServe>("RTTennisArmServe", GRAPH_EVENT_NONE, 1, false, "RenderTest");
	xRegistry.RegisterNodeType<RTNode_TennisBallReachable>("RTTennisBallReachable", GRAPH_EVENT_NONE, 1, false, "RenderTest");
	xRegistry.RegisterNodeType<RTNode_TennisMoveToIntercept>("RTTennisMoveToIntercept", GRAPH_EVENT_NONE, 1, false, "RenderTest");
	xRegistry.RegisterNodeType<RTNode_TennisDecideShot>("RTTennisDecideShot", GRAPH_EVENT_NONE, 1, false, "RenderTest");
	xRegistry.RegisterNodeType<RTNode_TennisArmSwing>("RTTennisArmSwing", GRAPH_EVENT_NONE, 1, false, "RenderTest");
	xRegistry.RegisterNodeType<RTNode_TennisRecoverToReady>("RTTennisRecoverToReady", GRAPH_EVENT_NONE, 1, false, "RenderTest");
	xRegistry.RegisterNodeType<RTNode_PlayerInteractGun>("RTPlayerInteractGun", GRAPH_EVENT_NONE, 1, false, "RenderTest");
	xRegistry.RegisterNodeType<RTNode_PlayerTryReload>("RTPlayerTryReload", GRAPH_EVENT_NONE, 1, false, "RenderTest");
	xRegistry.RegisterNodeType<RTNode_PlayerTryFire>("RTPlayerTryFire", GRAPH_EVENT_NONE, 1, false, "RenderTest");
	xRegistry.RegisterNodeType<RTNode_PlayerCycleTennisCam>("RTPlayerCycleTennisCam", GRAPH_EVENT_NONE, 1, false, "RenderTest");
}
