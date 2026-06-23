#pragma once
#include "AI/BehaviorTree/Zenith_BehaviorTree.h"
#include "AI/BehaviorTree/Zenith_BTNode.h"
#include "AI/BehaviorTree/Zenith_BTComposites.h"
#include "AI/BehaviorTree/Zenith_BTDecorators.h"
#include "AI/Navigation/Zenith_NavMeshAgent.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"

#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "Physics/Zenith_Physics.h"

#include "RenderTest/Components/RenderTest_TennisAgentComponent.h"
#include "RenderTest/Components/RenderTest_TennisPlayerComponent.h"
#include "RenderTest/Components/RenderTest_TennisDecision.h"

// Behaviour-tree leaves + tree builder for the autonomous tennis NPCs.
//
// Includes the brain header (to resolve + drive the brain at Execute time) but
// NOT the referee header — leaves read referee state (phase, epoch, role, ball
// entity/spin) from the blackboard the referee publishes each OnLateUpdate. That
// one-directional flow keeps the headers acyclic.
//
// INVARIANT: no leaf ever returns RUNNING. Every leaf reports SUCCESS or FAILURE,
// so the root selector re-evaluates top-down each tick (fully reactive). The arm
// guard lives on the brain and is reset ONLY by the referee's AdvanceBallEpoch ->
// ResetForNewBall (synchronous, both brains), never by a leaf.

// Shared resolution helpers for the tennis leaves.
class RenderTest_TennisLeaf : public Zenith_BTLeaf
{
protected:
	static RenderTest_TennisAgentComponent* Brain(Zenith_Entity& xA)
	{
		return xA.HasComponent<RenderTest_TennisAgentComponent>()
			? &xA.GetComponent<RenderTest_TennisAgentComponent>() : nullptr;
	}
	static RenderTest_TennisPlayerComponent* Body(Zenith_Entity& xA)
	{
		return xA.HasComponent<RenderTest_TennisPlayerComponent>()
			? &xA.GetComponent<RenderTest_TennisPlayerComponent>() : nullptr;
	}
	static Zenith_NavMeshAgent* Nav(Zenith_Entity& xA)
	{
		if (xA.HasComponent<Zenith_AIAgentComponent>())
			return xA.GetComponent<Zenith_AIAgentComponent>().GetNavMeshAgent();
		return nullptr;
	}
	static Zenith_Maths::Vector3 SelfPos(Zenith_Entity& xA)
	{
		Zenith_Maths::Vector3 xPos(0.0f);
		if (xA.HasComponent<Zenith_TransformComponent>())
			xA.GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
		return xPos;
	}
	static RenderTest_Tennis::TennisCourt Court() { return RenderTest_Tennis::DefaultCourt(); }

	// Resolve the live ball pose + velocity (spin comes from the blackboard the
	// referee publishes). Returns false if the ball can't be resolved.
	static bool BallState(Zenith_Blackboard& xBB, Zenith_Maths::Vector3& xPos,
		Zenith_Maths::Vector3& xVel, Zenith_Maths::Vector3& xSpin)
	{
		xSpin = xBB.GetVector3(RenderTest_TennisBB::k_szBallSpin, Zenith_Maths::Vector3(0.0f));
		const Zenith_EntityID xBall = xBB.GetEntityID(RenderTest_TennisBB::k_szBallEntity);
		if (xBall == INVALID_ENTITY_ID)
			return false;
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneDataForEntity(xBall);
		if (!pxSceneData)
			return false;
		Zenith_Entity xEnt = pxSceneData->GetEntity(xBall);
		if (!xEnt.IsValid() || !xEnt.HasComponent<Zenith_TransformComponent>())
			return false;
		xEnt.GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
		xVel = Zenith_Maths::Vector3(0.0f);
		if (xEnt.HasComponent<Zenith_ColliderComponent>())
		{
			Zenith_ColliderComponent& xCol = xEnt.GetComponent<Zenith_ColliderComponent>();
			if (xCol.HasValidBody())
				xVel = g_xEngine.Physics().GetLinearVelocity(xCol.GetBodyID());
		}
		return true;
	}

	// The X-stance for a serve from the parity-selected court (deuce/ad).
	static float ServeStanceX(const RenderTest_Tennis::TennisCourt& xCourt, int iSide, bool bDeuce)
	{
		const float fServerDeuceSign = (iSide == RenderTest_Tennis::TENNIS_SIDE_NEAR) ? 1.0f : -1.0f;
		const float fStanceSign = bDeuce ? fServerDeuceSign : -fServerDeuceSign;
		return xCourt.m_fCenterX + fStanceSign * (xCourt.m_fSinglesHalfWidth * 0.5f);
	}

	static void SetNavDest(Zenith_Entity& xA, const Zenith_Maths::Vector3& xDest)
	{
		if (Zenith_NavMeshAgent* pxNav = Nav(xA))
			pxNav->SetDestination(xDest);
	}

	static constexpr float k_fSeenThreshold = 0.25f;
	// Additive reachability slack in PredictIntercept (HorizDist <= runSpeed*t + this).
	// DEVIATES from the plan's k_fReachRadius=1.2: widened to 2.5 so agents commit to
	// chase more balls and rallies emerge (at 1.2 the receiver too often judged a ball
	// unreachable and let it pass). The contact proximity gate still rejects a swing that
	// doesn't reach the ball, so this only affects WHICH balls are chased.
	static constexpr float k_fReachRadius   = 2.5f;
	static constexpr float k_fRunSpeed      = 6.0f;    // matches the nav agent move speed
	// DEVIATES from the plan's k_fContactLead=0.40: 0.6 starts the swing slightly earlier
	// so the anim's contact frame aligns with the ball arrival given the stroke length.
	static constexpr float k_fArmLeadWindow = 0.6f;    // start the swing this far before the strike
	// Erosion for nav destinations (plan's "eroded slab"): keep targets this far inside
	// the slab edge (~agent radius + clearance) so SetDestination doesn't fail-stop at
	// the very boundary of the generated navmesh.
	static constexpr float k_fNavDestMargin = 1.0f;
};

// ---- A: Serve branch -------------------------------------------------------

class RenderTest_BTCond_IsMyServeAndBallParked : public RenderTest_TennisLeaf
{
public:
	BTNodeStatus Execute(Zenith_Entity&, Zenith_Blackboard& xBB, float) override
	{
		if (xBB.GetInt(RenderTest_TennisBB::k_szPhase, 0) != static_cast<int>(RenderTest_Tennis::POINT_PHASE_SERVING))
			return BTNodeStatus::FAILURE;
		if (!xBB.GetBool(RenderTest_TennisBB::k_szIsServer, false))
			return BTNodeStatus::FAILURE;
		// The ball must still be PARKED (serve not yet struck) — otherwise the serve
		// is in flight and re-firing would flail a phantom swing into empty air.
		if (!xBB.GetBool(RenderTest_TennisBB::k_szServeBallParked, false))
			return BTNodeStatus::FAILURE;
		return BTNodeStatus::SUCCESS;
	}
	const char* GetTypeName() const override { return "IsMyServeAndBallParked"; }
};

class RenderTest_BTAction_DecideServe : public RenderTest_TennisLeaf
{
public:
	BTNodeStatus Execute(Zenith_Entity& xA, Zenith_Blackboard& xBB, float) override
	{
		RenderTest_TennisAgentComponent* pxBrain = Brain(xA);
		if (!pxBrain)
			return BTNodeStatus::FAILURE;
		if (pxBrain->IsArmed())
			return BTNodeStatus::SUCCESS;   // keep the committed decision (don't unarm mid-swing)
		const bool bDeuce = xBB.GetBool(RenderTest_TennisBB::k_szServeFromDeuce, true);
		const bool bSecond = xBB.GetBool(RenderTest_TennisBB::k_szIsSecondServe, false);
		const RenderTest_Tennis::TennisShotDecision xDec = RenderTest_Tennis::SelectServe(
			Court(), pxBrain->GetPlayerState(), RenderTest_Tennis::SERVE_RESULT_GOOD, bDeuce, bSecond, pxBrain->Rng());
		pxBrain->SetDecidedShot(xDec);   // stored UNARMED
		return BTNodeStatus::SUCCESS;
	}
	const char* GetTypeName() const override { return "DecideServe"; }
};

class RenderTest_BTAction_PositionForServe : public RenderTest_TennisLeaf
{
public:
	BTNodeStatus Execute(Zenith_Entity& xA, Zenith_Blackboard& xBB, float) override
	{
		const RenderTest_Tennis::TennisCourt xCourt = Court();
		const int iSide = xBB.GetInt(RenderTest_TennisBB::k_szMySide, 0);
		const bool bDeuce = xBB.GetBool(RenderTest_TennisBB::k_szServeFromDeuce, true);
		const float fStanceX = ServeStanceX(xCourt, iSide, bDeuce);
		const Zenith_Maths::Vector3 xDest = RenderTest_Tennis::ProjectToSlab(xCourt,
			Zenith_Maths::Vector3(fStanceX, xCourt.m_fSurfaceY, xCourt.BaselineZ(iSide)), k_fNavDestMargin);
		SetNavDest(xA, xDest);
		if (RenderTest_TennisPlayerComponent* pxBody = Body(xA))
			pxBody->SetFootworkTargetX(xDest.x);
		return BTNodeStatus::SUCCESS;
	}
	const char* GetTypeName() const override { return "PositionForServe"; }
};

class RenderTest_BTAction_ArmServe : public RenderTest_TennisLeaf
{
public:
	BTNodeStatus Execute(Zenith_Entity& xA, Zenith_Blackboard& xBB, float) override
	{
		RenderTest_TennisAgentComponent* pxBrain = Brain(xA);
		RenderTest_TennisPlayerComponent* pxBody = Body(xA);
		if (!pxBrain || !pxBody)
			return BTNodeStatus::FAILURE;
		if (pxBrain->IsArmed())
			return BTNodeStatus::SUCCESS;     // already committed this ball
		if (!pxBody->IsReady())
			return BTNodeStatus::SUCCESS;     // a stroke is in progress
		// Arm only if the body confirms the stroke actually started.
		if (pxBody->RequestServe(pxBrain->GetDecidedShot().m_xAim))
			pxBrain->ArmDecidedShot(static_cast<u_int>(xBB.GetInt(RenderTest_TennisBB::k_szBallEpoch, 0)));
		return BTNodeStatus::SUCCESS;
	}
	const char* GetTypeName() const override { return "ArmServe"; }
};

// ---- B: Return / rally branch ---------------------------------------------

class RenderTest_BTCond_BallIsMine : public RenderTest_TennisLeaf
{
public:
	BTNodeStatus Execute(Zenith_Entity& xA, Zenith_Blackboard& xBB, float) override
	{
		// Receiver must NOT arm during SERVING (before the serve's required bounce).
		if (xBB.GetInt(RenderTest_TennisBB::k_szPhase, 0) != static_cast<int>(RenderTest_Tennis::POINT_PHASE_LIVE))
			return BTNodeStatus::FAILURE;
		if (!xBB.GetBool(RenderTest_TennisBB::k_szIsMyBall, false))
			return BTNodeStatus::FAILURE;

		// Aware enough of the ball (specific-ID query, never GetPrimaryTarget).
		const Zenith_EntityID xBall = xBB.GetEntityID(RenderTest_TennisBB::k_szBallEntity);
		const float fAware = Zenith_PerceptionSystem::GetAwarenessOf(xA.GetEntityID(), xBall);

		// Reachable: the ball descends to my strike plane on my side within reach.
		Zenith_Maths::Vector3 xBallPos, xBallVel, xBallSpin;
		if (!BallState(xBB, xBallPos, xBallVel, xBallSpin))
			return BTNodeStatus::FAILURE;
		const RenderTest_Tennis::TennisCourt xCourt = Court();
		const RenderTest_Tennis::TennisSide eSide =
			static_cast<RenderTest_Tennis::TennisSide>(xBB.GetInt(RenderTest_TennisBB::k_szMySide, 0));
		const RenderTest_Tennis::TennisInterceptResult xHit = RenderTest_Tennis::PredictIntercept(
			xCourt, xBallPos, xBallVel, xBallSpin, eSide, RenderTest_Tennis::StrikeHeight(xCourt),
			k_fReachRadius, SelfPos(xA), k_fRunSpeed);
		if (fAware < k_fSeenThreshold)
			return BTNodeStatus::FAILURE;
		return xHit.m_bReachable ? BTNodeStatus::SUCCESS : BTNodeStatus::FAILURE;
	}
	const char* GetTypeName() const override { return "BallIsMine"; }
};

class RenderTest_BTAction_MoveToIntercept : public RenderTest_TennisLeaf
{
public:
	BTNodeStatus Execute(Zenith_Entity& xA, Zenith_Blackboard& xBB, float) override
	{
		Zenith_Maths::Vector3 xBallPos, xBallVel, xBallSpin;
		if (!BallState(xBB, xBallPos, xBallVel, xBallSpin))
			return BTNodeStatus::SUCCESS;
		const RenderTest_Tennis::TennisCourt xCourt = Court();
		const RenderTest_Tennis::TennisSide eSide =
			static_cast<RenderTest_Tennis::TennisSide>(xBB.GetInt(RenderTest_TennisBB::k_szMySide, 0));
		const RenderTest_Tennis::TennisInterceptResult xHit = RenderTest_Tennis::PredictIntercept(
			xCourt, xBallPos, xBallVel, xBallSpin, eSide, RenderTest_Tennis::StrikeHeight(xCourt),
			k_fReachRadius, SelfPos(xA), k_fRunSpeed);

		Zenith_Maths::Vector3 xTarget = xHit.m_bReachable ? xHit.m_xStrikePoint : xBallPos;
		xTarget.y = xCourt.m_fSurfaceY;
		const Zenith_Maths::Vector3 xDest = RenderTest_Tennis::ProjectToSlab(xCourt, xTarget, k_fNavDestMargin);
		SetNavDest(xA, xDest);
		if (RenderTest_TennisPlayerComponent* pxBody = Body(xA))
			pxBody->SetFootworkTargetX(xDest.x);   // footwork fallback when nav is invalid
		return BTNodeStatus::SUCCESS;
	}
	const char* GetTypeName() const override { return "MoveToIntercept"; }
};

class RenderTest_BTAction_DecideShot : public RenderTest_TennisLeaf
{
public:
	BTNodeStatus Execute(Zenith_Entity& xA, Zenith_Blackboard&, float) override
	{
		RenderTest_TennisAgentComponent* pxBrain = Brain(xA);
		if (!pxBrain)
			return BTNodeStatus::FAILURE;
		if (pxBrain->IsArmed())
			return BTNodeStatus::SUCCESS;   // keep the committed decision (don't unarm mid-swing)
		pxBrain->SetDecidedShot(RenderTest_Tennis::SelectShot(Court(), pxBrain->GetPlayerState(), pxBrain->Rng()));
		return BTNodeStatus::SUCCESS;   // stored UNARMED
	}
	const char* GetTypeName() const override { return "DecideShot"; }
};

class RenderTest_BTAction_ArmSwing : public RenderTest_TennisLeaf
{
public:
	BTNodeStatus Execute(Zenith_Entity& xA, Zenith_Blackboard& xBB, float) override
	{
		RenderTest_TennisAgentComponent* pxBrain = Brain(xA);
		RenderTest_TennisPlayerComponent* pxBody = Body(xA);
		if (!pxBrain || !pxBody)
			return BTNodeStatus::FAILURE;
		if (pxBrain->IsArmed())
			return BTNodeStatus::SUCCESS;
		if (!pxBody->IsReady())
			return BTNodeStatus::SUCCESS;

		Zenith_Maths::Vector3 xBallPos, xBallVel, xBallSpin;
		if (!BallState(xBB, xBallPos, xBallVel, xBallSpin))
			return BTNodeStatus::SUCCESS;
		const RenderTest_Tennis::TennisCourt xCourt = Court();
		const RenderTest_Tennis::TennisSide eSide =
			static_cast<RenderTest_Tennis::TennisSide>(xBB.GetInt(RenderTest_TennisBB::k_szMySide, 0));
		const RenderTest_Tennis::TennisInterceptResult xHit = RenderTest_Tennis::PredictIntercept(
			xCourt, xBallPos, xBallVel, xBallSpin, eSide, RenderTest_Tennis::StrikeHeight(xCourt),
			k_fReachRadius, SelfPos(xA), k_fRunSpeed);

		// Start the swing only inside the contact-lead window so the anim contact
		// frame lines up with the ball arriving. Arm only if the body confirms it.
		if (xHit.m_bReachable && xHit.m_fTimeToStrike <= k_fArmLeadWindow)
		{
			if (pxBody->RequestSwing(pxBrain->GetDecidedShot().m_xAim, xBallPos.x))
				pxBrain->ArmDecidedShot(static_cast<u_int>(xBB.GetInt(RenderTest_TennisBB::k_szBallEpoch, 0)));
		}
		return BTNodeStatus::SUCCESS;
	}
	const char* GetTypeName() const override { return "ArmSwing"; }
};

// ---- C: Recover fallback ---------------------------------------------------

class RenderTest_BTAction_RecoverToReady : public RenderTest_TennisLeaf
{
public:
	BTNodeStatus Execute(Zenith_Entity& xA, Zenith_Blackboard& xBB, float) override
	{
		const RenderTest_Tennis::TennisCourt xCourt = Court();
		const int iSide = xBB.GetInt(RenderTest_TennisBB::k_szMySide, 0);
		// A receiver awaiting a serve stands up near the service line; everyone else holds
		// the baseline (see ComputeReadyZ — keeps the gently-paced serve returnable).
		const float fReadyZ = RenderTest_Tennis::ComputeReadyZ(xCourt,
			static_cast<RenderTest_Tennis::TennisSide>(iSide),
			static_cast<RenderTest_Tennis::PointPhase>(xBB.GetInt(RenderTest_TennisBB::k_szPhase, 0)),
			xBB.GetBool(RenderTest_TennisBB::k_szIsServer, false));
		const Zenith_Maths::Vector3 xDest = RenderTest_Tennis::ProjectToSlab(xCourt,
			Zenith_Maths::Vector3(xCourt.m_fCenterX, xCourt.m_fSurfaceY, fReadyZ), k_fNavDestMargin);
		SetNavDest(xA, xDest);
		if (RenderTest_TennisPlayerComponent* pxBody = Body(xA))
			pxBody->SetFootworkTargetX(xDest.x);
		return BTNodeStatus::SUCCESS;   // always succeeds (fallback)
	}
	const char* GetTypeName() const override { return "RecoverToReady"; }
};

// ---- Tree builder ----------------------------------------------------------

inline Zenith_BehaviorTree* RenderTest_BuildTennisTree()
{
	Zenith_BTSequence* pxServe = new Zenith_BTSequence();
	pxServe->AddChild(new RenderTest_BTCond_IsMyServeAndBallParked());
	pxServe->AddChild(new RenderTest_BTAction_DecideServe());
	pxServe->AddChild(new RenderTest_BTAction_PositionForServe());
	pxServe->AddChild(new RenderTest_BTAction_ArmServe());

	Zenith_BTSequence* pxRally = new Zenith_BTSequence();
	pxRally->AddChild(new RenderTest_BTCond_BallIsMine());
	pxRally->AddChild(new RenderTest_BTAction_MoveToIntercept());
	pxRally->AddChild(new RenderTest_BTAction_DecideShot());
	pxRally->AddChild(new RenderTest_BTAction_ArmSwing());

	Zenith_BTSucceeder* pxRecover = new Zenith_BTSucceeder();
	pxRecover->SetChild(new RenderTest_BTAction_RecoverToReady());

	Zenith_BTSelector* pxRoot = new Zenith_BTSelector();
	pxRoot->AddChild(pxServe);
	pxRoot->AddChild(pxRally);
	pxRoot->AddChild(pxRecover);

	Zenith_BehaviorTree* pxTree = new Zenith_BehaviorTree();
	pxTree->SetRootNode(pxRoot);
	pxTree->SetName("TennisBrain");
	return pxTree;
}
