#pragma once
#include "Core/Zenith_Engine.h"

#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_AnimatorComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "ZenithECS/Zenith_Entity.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "DataStream/Zenith_DataStream.h"
#include "Physics/Zenith_Physics.h"
#include "Maths/Zenith_Maths.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "Flux/MeshAnimation/Flux_AnimationController.h"
#include "Flux/MeshAnimation/Flux_AnimationStateMachine.h"
#include "Flux/MeshAnimation/Flux_AnimationClip.h"
#include "Flux/MeshAnimation/Flux_BlendTree.h"
#include "Flux/MeshAnimation/Flux_InverseKinematics.h"
#include "Flux/MeshAnimation/Flux_SkeletonInstance.h"
#include "RenderTest/RenderTest_Tennis.h"
#include "RenderTest/Components/RenderTest_TennisDecision.h"

#include <cmath>
#include <string>

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

// Autonomous tennis-playing NPC. Same StickFigure model/skeleton/animator as the
// RenderTest player, driven by the match orchestrator.
//
// Animation: a SINGLE full-body state machine (Ready / Serve / Forehand /
// Backhand) — deliberately NOT layered, so GetCurrentAnimatorStateInfo() works
// for contact-frame detection (layers bypass the controller SM).
//
// IK testbed: a RightArm chain (shoulder->elbow->hand) reaches the hand to the
// live ball during the swing window, and the end-effector orientation extension
// squares the (hand-attached) racket toward the shot target — so the racket
// genuinely meets the ball rather than swinging at empty air.
class RenderTest_TennisPlayerComponent
{
public:
	enum class Stroke { None, Serve, Forehand, Backhand };

	RenderTest_TennisPlayerComponent(Zenith_Entity& xEntity)
		: m_xParentEntity(xEntity)
	{
	}

	// ---- Match-facing API ------------------------------------------------
	bool IsReady() const { return m_eStroke == Stroke::None; }

	// Which baseline this NPC plays (serialized v2; read back by tests/tools).
	bool IsNearSide() const { return m_bNearSide; }

	// Net-facing direction (true = faces +Z). Derived from the side at Init().
	bool IsFacingPositiveZ() const { return m_bFacingPositiveZ; }

	// When the nav agent owns the XZ velocity (autonomous AI), the body's footwork
	// must NOT also write velocity (the two fight); it keeps EnforceUpright and
	// re-asserts net-facing instead. The referee toggles this per phase.
	void SetExternalMovementDriven(bool bDriven) { m_bExternalMovementDriven = bDriven; }
	bool IsExternalMovementDriven() const { return m_bExternalMovementDriven; }

	// Begin a serve aimed at xAimTarget (world space). Returns true only if a
	// stroke actually started — i.e. the NPC was ready AND the animator/state
	// machine are available. The BT arms its decided shot only on a true return,
	// so a not-ready / setup-missing call can never leave a phantom armed shot.
	bool RequestServe(const Zenith_Maths::Vector3& xAimTarget)
	{
		return IsReady() && BeginStroke(Stroke::Serve, xAimTarget);
	}

	// Begin a groundstroke; forehand/backhand picked from the ball side relative
	// to this NPC's facing. Returns true only if the stroke actually started.
	bool RequestSwing(const Zenith_Maths::Vector3& xAimTarget, float fBallX)
	{
		if (!IsReady())
			return false;
		const float fSideX = fBallX - PlayerX();
		// Facing +Z (near side): ball to the player's right (+X) is a forehand for
		// a right-hander. Facing -Z (far side): mirror.
		const bool bForehand = m_bFacingPositiveZ ? (fSideX >= 0.0f) : (fSideX < 0.0f);
		return BeginStroke(bForehand ? Stroke::Forehand : Stroke::Backhand, xAimTarget);
	}

	// World position of the racket-head sweet spot, derived from the POSED
	// RightHand bone (so it tracks serve/forehand/backhand poses). The referee
	// proximity-gates contact against this, so a mistimed/mispositioned swing is a
	// genuine miss. Falls back to the body position if no skeleton is posed yet.
	Zenith_Maths::Vector3 GetRacketSweetSpotPos() const
	{
		Zenith_Maths::Vector3 xPos(0.0f);
		Zenith_TransformComponent* pxTransform = m_xParentEntity.TryGetComponent<Zenith_TransformComponent>();
		if (pxTransform == nullptr)
			return xPos;
		pxTransform->GetPosition(xPos);
		Zenith_ModelComponent* pxModel = m_xParentEntity.TryGetComponent<Zenith_ModelComponent>();
		if (pxModel == nullptr)
			return xPos;
		Zenith_ModelComponent& xModel = *pxModel;
		Zenith_Maths::Matrix4 xBoneModel;
		if (!xModel.HasSkeleton() || !xModel.GetBoneModelMatrix("RightHand", xBoneModel))
			return xPos;
		Zenith_Maths::Matrix4 xWorld;
		pxTransform->BuildModelMatrix(xWorld);
		const Zenith_Maths::Matrix4 xHandWorld = xWorld * xBoneModel;
		return RenderTest_Tennis::ComputeRacketSweetSpot(xHandWorld, k_fRacketReach);
	}

	// Zero the body's horizontal velocity (keeps Y so gravity/landing survive).
	// The referee calls this when parking a disabled agent so it doesn't coast on
	// the nav agent's last velocity.
	void ParkBody()
	{
		Zenith_ColliderComponent* pxCol = m_xParentEntity.TryGetComponent<Zenith_ColliderComponent>();
		if (pxCol == nullptr)
			return;
		Zenith_ColliderComponent& xCol = *pxCol;
		if (!xCol.HasValidBody())
			return;
		Zenith_Maths::Vector3 xVel = g_xEngine.Physics().GetLinearVelocity(xCol.GetBodyID());
		xVel.x = 0.0f;
		xVel.z = 0.0f;
		g_xEngine.Physics().SetLinearVelocity(xCol.GetBodyID(), xVel);
	}

	// True exactly once, on the frame the active stroke reaches its contact point.
	bool ConsumeContact()
	{
		if (m_bContactPending)
		{
			m_bContactPending = false;
			return true;
		}
		return false;
	}

	// Footwork target along the court width (X); the NPC slides toward it.
	void SetFootworkTargetX(float fX) { m_fFootworkTargetX = fX; }

	// IK-showcase hold: pin the arm-IK weight to 1 every frame so the racket head
	// stays locked on the (frozen) ball while the body plays the stroke — makes the
	// contact pose continuously visible for capture. No effect on the live match.
	void SetIKShowcaseHold(bool bHold) { m_bIKShowcaseHold = bHold; }

	void Init(bool bNearSide)
	{
		m_bNearSide = bNearSide;
		m_bFacingPositiveZ = bNearSide;   // near baseline faces the net (+Z); far faces -Z
	}

	// ---- Lifecycle -------------------------------------------------------
	void OnStart()
	{
		// Side/facing are set by the spawn via Init() before OnStart runs. Resolve
		// the shared ball entity here.
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneDataForEntity(m_xParentEntity.GetEntityID());
		if (pxSceneData)
			m_xBall = pxSceneData->FindEntityByName("Tennis_Ball");

		// Dynamic capsule for footwork (proper physics movement, like the player).
		Zenith_ColliderComponent* pxExistingCollider = m_xParentEntity.TryGetComponent<Zenith_ColliderComponent>();
		Zenith_ColliderComponent& xCollider = pxExistingCollider != nullptr
			? *pxExistingCollider
			: m_xParentEntity.AddComponent<Zenith_ColliderComponent>();
		if (!xCollider.HasValidBody())
			xCollider.AddCapsuleCollider(0.10f, 0.95f, RIGIDBODY_TYPE_DYNAMIC);
		if (xCollider.HasValidBody())
			g_xEngine.Physics().LockRotation(xCollider.GetBodyID(), true, false, true);

		// Face the net.
		if (Zenith_TransformComponent* pxTransform = m_xParentEntity.TryGetComponent<Zenith_TransformComponent>())
		{
			Zenith_TransformComponent& xT = *pxTransform;
			const float fYaw = m_bFacingPositiveZ ? 0.0f : static_cast<float>(Zenith_Maths::Pi);
			xT.SetRotation(glm::angleAxis(fYaw, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f)));
		}

		if (Zenith_AnimatorComponent* pxAnimator = m_xParentEntity.TryGetComponent<Zenith_AnimatorComponent>())
		{
			m_pxAnimator = pxAnimator;
			SetupAnimator();
		}
	}

	void OnUpdate(float fDt)
	{
		if (!m_pxAnimator)
			return;

		Footwork(fDt);
		// When the nav agent drives movement it may also rotate the body toward
		// its travel direction; re-assert the fixed net-facing so strokes/IK stay
		// oriented across the net.
		if (m_bExternalMovementDriven)
			ReassertFacing();
		UpdateStroke(fDt);
		UpdateArmIK();
	}

	void WriteToDataStream(Zenith_DataStream& xStream) const
	{
		// v2: persist which baseline this NPC plays so an authored player reloads
		// facing the right way (the runtime spawn that called Init(side) is gone).
		// v1 (version tag only) loads back as the near-side default.
		const u_int uVersion = 2;
		xStream << uVersion;
		xStream << m_bNearSide;
	}
	void ReadFromDataStream(Zenith_DataStream& xStream)
	{
		u_int uVersion = 0;
		xStream >> uVersion;
		if (uVersion < 2)
		{
			return;   // v1 carried no side — keep the near-side default.
		}
		bool bNearSide = true;
		xStream >> bNearSide;
		Init(bNearSide);   // derives m_bFacingPositiveZ; OnStart reads both later.
	}
#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel()
	{
		ImGui::Text("Side: %s", m_bNearSide ? "Near" : "Far");
		ImGui::Text("Stroke: %d  norm: %.2f", static_cast<int>(m_eStroke), m_fStrokeNorm);
		ImGui::Text("IK weight: %.2f", m_fIKWeight);
	}
#endif

private:
	float PlayerX() const
	{
		Zenith_Maths::Vector3 xPos(0.0f);
		if (Zenith_TransformComponent* pxTransform = m_xParentEntity.TryGetComponent<Zenith_TransformComponent>())
			pxTransform->GetPosition(xPos);
		return xPos.x;
	}

	Zenith_Maths::Vector3 BallPos() const
	{
		Zenith_Maths::Vector3 xPos(0.0f);
		if (Zenith_TransformComponent* pxTransform = m_xBall.TryGetComponent<Zenith_TransformComponent>())
			pxTransform->GetPosition(xPos);
		return xPos;
	}

	// Returns false (no-op) when the animator/state-machine aren't available — so
	// RequestServe/RequestSwing can report whether a stroke truly started.
	bool BeginStroke(Stroke eStroke, const Zenith_Maths::Vector3& xAimTarget)
	{
		if (!m_pxAnimator || !m_pxSM)
			return false;
		m_eStroke = eStroke;
		m_xAimTarget = xAimTarget;
		m_fStrokeNorm = 0.0f;
		m_bContactFired = false;
		m_bContactPending = false;
		const char* szTrigger =
			eStroke == Stroke::Serve ? "ServeTrigger" :
			eStroke == Stroke::Forehand ? "ForehandTrigger" : "BackhandTrigger";
		m_pxSM->GetParameters().SetTrigger(szTrigger);
		return true;
	}

	// Re-assert the fixed net-facing rotation (used when the nav agent might have
	// rotated the body toward its travel direction).
	void ReassertFacing()
	{
		Zenith_TransformComponent* pxTransform = m_xParentEntity.TryGetComponent<Zenith_TransformComponent>();
		if (pxTransform == nullptr)
			return;
		Zenith_TransformComponent& xT = *pxTransform;
		const float fYaw = m_bFacingPositiveZ ? 0.0f : static_cast<float>(Zenith_Maths::Pi);
		xT.SetRotation(glm::angleAxis(fYaw, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f)));
	}

	// Slide along X toward the footwork target; hold the baseline Z. Routed through
	// the pure ComputeFootworkVelocityX/ShouldDriveFootwork seam. When the nav
	// agent owns movement (m_bExternalMovementDriven) the body keeps EnforceUpright
	// but skips its velocity write so the two don't fight.
	void Footwork(float)
	{
		Zenith_ColliderComponent* pxCol = m_xParentEntity.TryGetComponent<Zenith_ColliderComponent>();
		if (pxCol == nullptr)
			return;
		Zenith_ColliderComponent& xCol = *pxCol;
		if (!xCol.HasValidBody())
			return;
		g_xEngine.Physics().EnforceUpright(xCol.GetBodyID());

		if (!RenderTest_Tennis::ShouldDriveFootwork(m_bExternalMovementDriven))
			return;   // nav agent owns the XZ velocity this frame

		const float fX = PlayerX();
		Zenith_Maths::Vector3 xVel = g_xEngine.Physics().GetLinearVelocity(xCol.GetBodyID());
		xVel.x = RenderTest_Tennis::ComputeFootworkVelocityX(fX, m_fFootworkTargetX, k_fRunSpeed);
		xVel.z = 0.0f;   // pinned to the baseline
		g_xEngine.Physics().SetLinearVelocity(xCol.GetBodyID(), xVel);
	}

	// Advance the active stroke, raising the contact event at its contact frame.
	void UpdateStroke(float)
	{
		if (m_eStroke == Stroke::None)
			return;

		Zenith_AnimatorStateInfo xInfo = m_pxAnimator->GetCurrentAnimatorStateInfo();

		const char* szState =
			m_eStroke == Stroke::Serve ? "Serve" :
			m_eStroke == Stroke::Forehand ? "Forehand" : "Backhand";

		// Only adopt the normalized time once the SM is actually in the stroke
		// state. Before the trigger transition completes, GetCurrentAnimatorStateInfo
		// reports the (looping) Ready clock, which could sit near the contact
		// fraction and spuriously drive the arm-IK weight before the swing starts.
		// Pinning m_fStrokeNorm at 0 until the stroke plays keeps the IK weight 0.
		if (xInfo.IsName(szState))
		{
			m_fStrokeNorm = xInfo.m_fNormalizedTime;
			const float fContactNorm = (m_eStroke == Stroke::Serve) ? k_fServeContactNorm : k_fSwingContactNorm;
			if (!m_bContactFired && m_fStrokeNorm >= fContactNorm)
			{
				m_bContactFired = true;
				m_bContactPending = true;   // consumed by the match to launch the ball
			}
			// Stroke complete -> back to ready.
			if (m_fStrokeNorm >= 0.98f)
			{
				m_eStroke = Stroke::None;
				m_fStrokeNorm = 0.0f;
			}
		}
	}

	// Arm IK: ramp weight up around the contact window so the hand (and the
	// rigidly-attached racket) meets the live ball, with the racket face squared
	// toward the shot target. Implemented in Phase 6; harmless no-op before the
	// chain exists.
	void UpdateArmIK()
	{
		if (!m_pxAnimator)
			return;

		// Weight envelope: 0 outside a stroke, ramping to 1 across the contact
		// window. Bell-shaped around the contact normalized time.
		float fTargetWeight = 0.0f;
		if (m_eStroke != Stroke::None)
		{
			const float fContactNorm = (m_eStroke == Stroke::Serve) ? k_fServeContactNorm : k_fSwingContactNorm;
			const float fHalf = 0.22f;
			const float fDist = std::fabs(m_fStrokeNorm - fContactNorm);
			fTargetWeight = (fDist < fHalf) ? (1.0f - fDist / fHalf) : 0.0f;
		}
		m_fIKWeight = fTargetWeight;
		if (m_bIKShowcaseHold)
			m_fIKWeight = 1.0f;   // showcase: keep the racket head pinned on the ball

		if (m_fIKWeight <= 0.001f)
		{
			m_pxAnimator->ClearIKTarget("RightArm");
			return;
		}

		// Need the posed skeleton + the world matrix to convert the live ball
		// position into model space (the same model-space-pin trick the foot IK
		// uses to avoid a one-physics-step drag).
		if (!m_xParentEntity.HasComponent<Zenith_ModelComponent>()
			|| !m_xParentEntity.HasComponent<Zenith_TransformComponent>())
			return;
		Zenith_ModelComponent& xModel = m_xParentEntity.GetComponent<Zenith_ModelComponent>();
		if (!xModel.HasSkeleton())
			return;
		Flux_SkeletonInstance* pxSkel = xModel.GetSkeletonInstance();
		if (!pxSkel)
			return;

		Zenith_Maths::Matrix4 xWorld;
		m_xParentEntity.GetComponent<Zenith_TransformComponent>().BuildModelMatrix(xWorld);
		const Zenith_Maths::Matrix4 xInvWorld = glm::inverse(xWorld);

		const Zenith_Maths::Vector3 xBall = BallPos();
		const Zenith_Maths::Vector4 xBallModel4 = xInvWorld * Zenith_Maths::Vector4(xBall, 1.0f);
		const Zenith_Maths::Vector3 xBallModel(xBallModel4);

		// Orient the racket so its SHAFT (the hand bone's -Y, along which the head
		// extends via the 180deg-about-X mount) points from the shoulder toward the
		// ball, then place the HAND so the racket HEAD — not the grip — lands on the
		// ball. The head sits ~k_fRacketReach up the shaft, so with the shaft aimed
		// at the ball the head reaches it when the hand is pulled back along the
		// shaft. (A fixed forward-facing orientation pointed the racket past the
		// ball and put the contact on the hand/handle.)
		Zenith_Maths::Vector3 xShaftDir(0.0f, -1.0f, 0.0f);
		float fBallFromShoulder = k_fRacketReach + 0.5f;   // safe default if no shoulder bone
		Zenith_Maths::Matrix4 xShoulderMat;
		if (xModel.GetBoneModelMatrix("RightUpperArm", xShoulderMat))
		{
			const Zenith_Maths::Vector3 xShoulderModel(xShoulderMat[3]);
			const Zenith_Maths::Vector3 xToBall = xBallModel - xShoulderModel;
			fBallFromShoulder = glm::length(xToBall);
			if (fBallFromShoulder > 1e-4f)
				xShaftDir = xToBall / fBallFromShoulder;
		}
		const Zenith_Maths::Quat xRacketRot =
			RotationToDirection(Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f), xShaftDir);

		// Degenerate-guard: if the ball is so close to the shoulder that pulling the
		// hand back a full racket-reach would collapse it onto (or behind) the
		// shoulder — which FABRIK can't fold to, leaving the arm extended and the
		// racket missing — clamp the pull-back so the hand stays at least
		// k_fMinHandFromShoulder out along the arm. Contact points are placed so this
		// never binds in practice; it just keeps a bad ball from breaking the solve.
		float fReach = k_fRacketReach;
		if (fBallFromShoulder - fReach < k_fMinHandFromShoulder)
			fReach = glm::max(0.0f, fBallFromShoulder - k_fMinHandFromShoulder);
		const Zenith_Maths::Vector3 xHandTargetModel = xBallModel - xShaftDir * fReach;

		m_pxAnimator->SetIKTargetModelSpace("RightArm", xHandTargetModel, xRacketRot, m_fIKWeight);
	}

	// Shortest-arc rotation taking unit vector xFrom to unit vector xTo.
	static Zenith_Maths::Quat RotationToDirection(const Zenith_Maths::Vector3& xFrom, const Zenith_Maths::Vector3& xTo)
	{
		const float fD = glm::dot(xFrom, xTo);
		if (fD > 0.99999f)
			return Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f);
		if (fD < -0.99999f)
		{
			Zenith_Maths::Vector3 xPerp = (std::fabs(xFrom.x) < 0.9f)
				? Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f) : Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
			xPerp = glm::normalize(glm::cross(xFrom, xPerp));
			return glm::angleAxis(3.14159265f, xPerp);
		}
		const Zenith_Maths::Vector3 xAxis = glm::normalize(glm::cross(xFrom, xTo));
		return glm::angleAxis(acosf(glm::clamp(fD, -1.0f, 1.0f)), xAxis);
	}

	// ---- Animator construction ------------------------------------------
	void SetupAnimator()
	{
		Flux_AnimationController& xCtl = m_pxAnimator->GetController();

		static const std::string s_strDir = std::string(ENGINE_ASSETS_DIR) + "Meshes/StickFigure/";
		xCtl.AddClipFromFile(s_strDir + "StickFigure_ReadyStance" ZENITH_ANIMATION_EXT);
		xCtl.AddClipFromFile(s_strDir + "StickFigure_Serve"       ZENITH_ANIMATION_EXT);
		xCtl.AddClipFromFile(s_strDir + "StickFigure_Forehand"    ZENITH_ANIMATION_EXT);
		xCtl.AddClipFromFile(s_strDir + "StickFigure_Backhand"    ZENITH_ANIMATION_EXT);

		m_pxSM = xCtl.CreateStateMachine("Tennis");
		Flux_AnimationClipCollection& xClips = xCtl.GetClipCollection();

		AddClipState(m_pxSM, xClips, "Ready",    "ReadyStance");
		AddClipState(m_pxSM, xClips, "Serve",    "Serve");
		AddClipState(m_pxSM, xClips, "Forehand", "Forehand");
		AddClipState(m_pxSM, xClips, "Backhand", "Backhand");

		m_pxSM->GetParameters().AddTrigger("ServeTrigger");
		m_pxSM->GetParameters().AddTrigger("ForehandTrigger");
		m_pxSM->GetParameters().AddTrigger("BackhandTrigger");

		// Ready -> stroke on trigger; stroke -> Ready when the clip finishes. The
		// match only commands a swing while IsReady(), so a trigger is never lost
		// mid-stroke.
		AddTriggerTransition(m_pxSM, "Ready", "Serve",    "ServeTrigger",    0.06f, 50);
		AddTriggerTransition(m_pxSM, "Ready", "Forehand", "ForehandTrigger", 0.06f, 50);
		AddTriggerTransition(m_pxSM, "Ready", "Backhand", "BackhandTrigger", 0.06f, 50);
		AddExitTimeTransition(m_pxSM, "Serve",    "Ready", 0.98f, 0.12f);
		AddExitTimeTransition(m_pxSM, "Forehand", "Ready", 0.98f, 0.12f);
		AddExitTimeTransition(m_pxSM, "Backhand", "Ready", 0.98f, 0.12f);

		m_pxSM->SetDefaultState("Ready");
		m_pxSM->ResolveClipReferences(&xClips);

		// Right-arm IK chain (shoulder -> elbow -> hand). Tuned like the foot
		// chains for clean convergence near full extension.
		Flux_IKSolver& xIK = xCtl.GetIKSolver();
		if (!xIK.HasChain("RightArm"))
		{
			Flux_IKChain xArm = Flux_IKSolver::CreateArmChain("RightArm",
				"RightUpperArm", "RightLowerArm", "RightHand");
			xArm.m_uMaxIterations = 30;
			xArm.m_fTolerance = 0.0005f;
			xIK.AddChain(xArm);
		}
	}

	static void AddClipState(Flux_AnimationStateMachine* pxSM, Flux_AnimationClipCollection& xClips,
		const char* szState, const char* szClip)
	{
		Flux_AnimationState* pxState = pxSM->AddState(szState);
		Flux_AnimationClip* pxClip = xClips.GetClip(szClip);
		if (pxClip)
			pxState->SetBlendTree(new Flux_BlendTreeNode_Clip(pxClip, 1.0f));
	}

	static void AddTriggerTransition(Flux_AnimationStateMachine* pxSM, const char* szFrom, const char* szTo,
		const char* szTrigger, float fDuration, int32_t iPriority)
	{
		Flux_StateTransition xTrans;
		xTrans.m_strTargetStateName = szTo;
		xTrans.m_fTransitionDuration = fDuration;
		xTrans.m_iPriority = iPriority;
		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = szTrigger;
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Trigger;
		xTrans.m_xConditions.PushBack(xCond);
		pxSM->GetState(szFrom)->AddTransition(xTrans);
	}

	static void AddExitTimeTransition(Flux_AnimationStateMachine* pxSM, const char* szFrom, const char* szTo,
		float fExitTime, float fDuration)
	{
		Flux_StateTransition xTrans;
		xTrans.m_strTargetStateName = szTo;
		xTrans.m_fTransitionDuration = fDuration;
		xTrans.m_bHasExitTime = true;
		xTrans.m_fExitTime = fExitTime;
		pxSM->GetState(szFrom)->AddTransition(xTrans);
	}

	// Contact normalized times match the clip authoring (Serve contact ~tick
	// 16/30; groundstroke contact ~tick 10/18).
	static constexpr float k_fServeContactNorm = 0.53f;
	static constexpr float k_fSwingContactNorm = 0.55f;
	// Footwork X-slide speed. MUST match the nav agent / PredictIntercept run speed
	// (6.0): on the P2 nav-fallback path the body becomes the sole mover, and the brain
	// committed reachability at 6.0 — a slower body would undershoot a ball it judged
	// reachable, exceeding the contact radius and losing the point.
	static constexpr float k_fRunSpeed = 6.0f;
	// Distance from the hand/grip to the racket head's sweet spot (the racket is
	// attached at the grip; the head extends ~0.42 m up the shaft).
	static constexpr float k_fRacketReach = 0.42f;
	// Minimum distance the IK hand target is kept from the shoulder, so a ball very
	// close to the shoulder can't collapse the target onto it (FABRIK degeneracy).
	static constexpr float k_fMinHandFromShoulder = 0.20f;

	Zenith_Entity m_xParentEntity;
	Zenith_Entity m_xBall;
	Zenith_AnimatorComponent*    m_pxAnimator = nullptr;
	Flux_AnimationStateMachine*  m_pxSM = nullptr;

	bool  m_bNearSide = true;
	bool  m_bFacingPositiveZ = true;
	bool  m_bExternalMovementDriven = false;   // nav agent owns XZ velocity (transient)
	float m_fFootworkTargetX = RenderTest_Tennis::fCOURT_CX;

	Stroke m_eStroke = Stroke::None;
	float  m_fStrokeNorm = 0.0f;
	bool   m_bContactFired = false;
	bool   m_bContactPending = false;
	float  m_fIKWeight = 0.0f;
	bool   m_bIKShowcaseHold = false;
	Zenith_Maths::Vector3 m_xAimTarget = Zenith_Maths::Vector3(0.0f);
};
