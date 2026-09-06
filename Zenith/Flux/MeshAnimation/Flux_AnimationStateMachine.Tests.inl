#include "Core/Zenith_TestFramework.h"
#include "Flux/MeshAnimation/Flux_AnimationController.h"
#include "Flux/MeshAnimation/Flux_AnimationLayer.h"
#include "Flux/MeshAnimation/Flux_SkeletonInstance.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"

// ============================================================================
// WU-6.1 — THE DEF / INSTANCE SPLIT, CONTROLLER-WIDE PARAMETERS (D42) AND
// NAMED BLEND-SPACE BINDINGS (D48)
//
// ★ WHAT THE BLEND-SPACE TESTS EXIST TO CATCH, and why they could not have been
// written before: Flux_BlendTreeNode::Evaluate takes no parameter set, and
// Flux_AnimationStateMachine::EvaluateState called it with (dt, pose, skeleton)
// only — so a 1D/2D blend space in a running game sat frozen at the literal it
// was deserialized with, forever. Nothing outside the unit suite ever called
// SetParameter. A test of the NODE alone would have passed the whole time
// (SetParameter has always worked); only a test that drives the STATE MACHINE
// and sets the value as a controller PARAMETER can see the missing wire.
//
// Everything here is headless: an in-memory Zenith_SkeletonAsset, in-memory
// clips, no assets on disk, no graphics.
// ============================================================================

namespace
{
	// A two-bone rig, matching the one the rest of the animation suite uses.
	struct WU61_Rig
	{
		Zenith_SkeletonAsset* m_pxSkeleton = nullptr;
		Flux_SkeletonInstance* m_pxInstance = nullptr;

		WU61_Rig()
		{
			m_pxSkeleton = new Zenith_SkeletonAsset();
			const Zenith_Maths::Quat xIdentity = glm::identity<Zenith_Maths::Quat>();
			const Zenith_Maths::Vector3 xUnitScale(1.0f);
			m_pxSkeleton->AddBone("Root", -1, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), xIdentity, xUnitScale);
			m_pxSkeleton->AddBone("Child", 0, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), xIdentity, xUnitScale);
			m_pxSkeleton->ComputeBindPoseMatrices();
			m_pxInstance = Flux_SkeletonInstance::CreateFromAsset(m_pxSkeleton);
		}

		~WU61_Rig()
		{
			delete m_pxInstance;
			delete m_pxSkeleton;
		}

		WU61_Rig(const WU61_Rig&) = delete;
		WU61_Rig& operator=(const WU61_Rig&) = delete;
	};

	// ONE constant position key on "Root", so the clip's contribution to a pose is
	// a single known vector at every time. That is what makes "the pose equals
	// clip A" and "the pose is halfway between A and B" exact statements rather
	// than statements about where two playheads happened to be.
	Flux_AnimationClip* WU61_MakeConstantClip(const char* szName, const Zenith_Maths::Vector3& xRootPosition)
	{
		Flux_AnimationClip* pxClip = new Flux_AnimationClip();
		pxClip->SetName(szName);
		pxClip->SetDuration(1.0f);
		pxClip->SetLooping(true);

		Flux_BoneChannel xChannel;
		xChannel.SetBoneName("Root");
		xChannel.AddPositionKeyframe(0.0f, xRootPosition);
		pxClip->AddBoneChannel("Root", std::move(xChannel));
		return pxClip;
	}

	bool WU61_PosesMatch(const Flux_SkeletonPose& xA, const Flux_SkeletonPose& xB, uint32_t uNumBones, float fTol)
	{
		for (uint32_t i = 0; i < uNumBones; ++i)
		{
			const Flux_BoneLocalPose& xPa = xA.GetLocalPose(i);
			const Flux_BoneLocalPose& xPb = xB.GetLocalPose(i);
			if (glm::length(xPa.m_xPosition - xPb.m_xPosition) > fTol) return false;
			if (glm::length(xPa.m_xScale - xPb.m_xScale) > fTol) return false;
			// A quaternion and its negation are the same rotation, so compare the
			// two possibilities rather than the raw components.
			const float fDot = glm::abs(glm::dot(xPa.m_xRotation, xPb.m_xRotation));
			if (fDot < 1.0f - fTol) return false;
		}
		return true;
	}

	bool WU61_RootPositionIs(const Flux_SkeletonPose& xPose, const Zenith_Maths::Vector3& xExpected, float fTol)
	{
		return glm::length(xPose.GetLocalPose(0).m_xPosition - xExpected) <= fTol;
	}

	// Idle/Walk over two constant clips, plus Idle -> Walk when Speed > 0.5.
	// Written against the DEF interface so the same routine can populate an
	// imperatively-built machine (which forwards into its own def) and a free
	// standing definition — that equivalence is what test (1) measures.
	void WU61_AuthorLocomotion(Flux_AnimationStateMachineDef& xDef,
		Flux_AnimationClip* pxIdleClip, Flux_AnimationClip* pxWalkClip)
	{
		xDef.GetParameterDeclarations().AddFloat("Speed", 0.0f);

		xDef.AddState("Idle")->SetBlendTree(new Flux_BlendTreeNode_Clip(pxIdleClip));
		xDef.AddState("Walk")->SetBlendTree(new Flux_BlendTreeNode_Clip(pxWalkClip));
		xDef.SetDefaultState("Idle");

		Flux_StateTransition xToWalk;
		xToWalk.m_strTargetStateName = "Walk";
		xToWalk.m_fTransitionDuration = 0.25f;

		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = "Speed";
		xCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Greater;
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Float;
		xCond.m_fThreshold = 0.5f;
		xToWalk.m_xConditions.PushBack(xCond);

		xDef.GetState("Idle")->AddTransition(xToWalk);
	}
}

//=============================================================================
// (1) A def-built machine and an imperatively-built one are the SAME machine.
//
// Sixty ticks, including a crossfade fired mid-run, compared bone-for-bone. The
// def half goes through CopyFrom — which is a serialize/deserialize round trip —
// so this is also what would catch a blend-tree field that the stream does not
// carry: the copy would diverge from the original within a frame of the value
// mattering.
//=============================================================================
ZENITH_TEST(Animation, WU61_DefBuiltMachineMatchesImperativeMachine)
{
	WU61_Rig xRig;

	Flux_AnimationClip* pxIdle = WU61_MakeConstantClip("Idle", Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
	Flux_AnimationClip* pxWalk = WU61_MakeConstantClip("Walk", Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f));

	Flux_AnimationClipCollection xClips;
	xClips.AddClip(pxIdle);   // the collection owns them from here
	xClips.AddClip(pxWalk);

	// A — the pre-WU-6.1 path: build the machine directly.
	Flux_AnimationStateMachine xImperative("Loco");
	WU61_AuthorLocomotion(xImperative.GetDef(), pxIdle, pxWalk);

	// B — author a standalone def, then build a machine from it.
	Flux_AnimationStateMachineDef xDef("Loco");
	WU61_AuthorLocomotion(xDef, pxIdle, pxWalk);

	Flux_AnimationStateMachine xFromDef;
	xFromDef.BuildFromDef(xDef, &xClips);

	ZENITH_ASSERT_EQ(xFromDef.GetName(), "Loco", "the def's name comes across");
	ZENITH_ASSERT_EQ(xFromDef.GetDefaultStateName(), "Idle", "and its default state");

	Flux_SkeletonPose xPoseA;
	Flux_SkeletonPose xPoseB;
	xPoseA.Initialize(2);
	xPoseB.Initialize(2);

	bool bAllMatched = true;
	for (u_int u = 0; u < 60; ++u)
	{
		if (u == 10)
		{
			// Fire the transition on both at the same tick.
			xImperative.GetParameters().SetFloat("Speed", 1.0f);
			xFromDef.GetParameters().SetFloat("Speed", 1.0f);
		}

		xImperative.Update(1.0f / 60.0f, xPoseA, *xRig.m_pxSkeleton);
		xFromDef.Update(1.0f / 60.0f, xPoseB, *xRig.m_pxSkeleton);

		if (!WU61_PosesMatch(xPoseA, xPoseB, 2, 1e-5f))
		{
			bAllMatched = false;
			break;
		}
	}

	ZENITH_ASSERT_TRUE(bAllMatched, "a def-built machine poses identically to an imperatively-built one, every tick");
	ZENITH_ASSERT_EQ(xImperative.GetCurrentStateName(), "Walk", "and the transition really did fire (otherwise both were idle the whole run)");
	ZENITH_ASSERT_EQ(xFromDef.GetCurrentStateName(), "Walk", "on both machines");
}

//=============================================================================
// (2) ★ A BOUND BLEND SPACE ACTUALLY MOVES — the test that could not exist
//     before D48, because nothing passed parameters into a blend tree.
//=============================================================================
ZENITH_TEST(Animation, WU61_BlendSpaceTracksItsNamedParameter)
{
	WU61_Rig xRig;

	Flux_AnimationClip* pxA = WU61_MakeConstantClip("A", Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
	Flux_AnimationClip* pxB = WU61_MakeConstantClip("B", Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f));

	Flux_AnimationClipCollection xClips;
	xClips.AddClip(pxA);
	xClips.AddClip(pxB);

	Flux_BlendTreeNode_BlendSpace1D* pxSpace = new Flux_BlendTreeNode_BlendSpace1D();
	pxSpace->AddBlendPoint(new Flux_BlendTreeNode_Clip(pxA), 0.0f);
	pxSpace->AddBlendPoint(new Flux_BlendTreeNode_Clip(pxB), 1.0f);
	pxSpace->SetParameterName("Speed");

	Flux_AnimationStateMachine xSM("Loco");
	xSM.GetParameters().AddFloat("Speed", 0.0f);
	xSM.AddState("Move")->SetBlendTree(pxSpace);
	xSM.SetDefaultState("Move");

	Flux_SkeletonPose xPose;
	xPose.Initialize(2);

	xSM.GetParameters().SetFloat("Speed", 0.0f);
	xSM.Update(1.0f / 60.0f, xPose, *xRig.m_pxSkeleton);
	ZENITH_ASSERT_TRUE(WU61_RootPositionIs(xPose, Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f), 1e-5f),
		"Speed 0 poses clip A exactly");

	xSM.GetParameters().SetFloat("Speed", 1.0f);
	xSM.Update(1.0f / 60.0f, xPose, *xRig.m_pxSkeleton);
	ZENITH_ASSERT_TRUE(WU61_RootPositionIs(xPose, Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f), 1e-5f),
		"Speed 1 poses clip B exactly");

	xSM.GetParameters().SetFloat("Speed", 0.5f);
	xSM.Update(1.0f / 60.0f, xPose, *xRig.m_pxSkeleton);
	ZENITH_ASSERT_TRUE(WU61_RootPositionIs(xPose, Zenith_Maths::Vector3(0.5f, 0.0f, 0.5f), 1e-5f),
		"Speed 0.5 poses halfway between them");

	ZENITH_ASSERT_EQ_FLOAT(pxSpace->GetParameter(), 0.5f, 1e-5f,
		"and the node's own position followed the named parameter, not a SetParameter call");
}

//=============================================================================
// (2b) The other half of the same rule: an UNBOUND space keeps its literal, so
//      a parameter that happens to share a name cannot move it by accident.
//=============================================================================
ZENITH_TEST(Animation, WU61_UnboundBlendSpaceKeepsItsLiteral)
{
	WU61_Rig xRig;

	Flux_AnimationClip* pxA = WU61_MakeConstantClip("A", Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
	Flux_AnimationClip* pxB = WU61_MakeConstantClip("B", Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f));

	Flux_AnimationClipCollection xClips;
	xClips.AddClip(pxA);
	xClips.AddClip(pxB);

	Flux_BlendTreeNode_BlendSpace1D* pxSpace = new Flux_BlendTreeNode_BlendSpace1D();
	pxSpace->AddBlendPoint(new Flux_BlendTreeNode_Clip(pxA), 0.0f);
	pxSpace->AddBlendPoint(new Flux_BlendTreeNode_Clip(pxB), 1.0f);
	pxSpace->SetParameter(0.0f);   // authored literal; NO name bound

	ZENITH_ASSERT_FALSE(pxSpace->HasParameterBinding(), "this space is deliberately unbound");

	Flux_AnimationStateMachine xSM("Loco");
	xSM.GetParameters().AddFloat("Speed", 0.0f);
	xSM.AddState("Move")->SetBlendTree(pxSpace);
	xSM.SetDefaultState("Move");

	Flux_SkeletonPose xPose;
	xPose.Initialize(2);

	xSM.GetParameters().SetFloat("Speed", 1.0f);
	xSM.Update(1.0f / 60.0f, xPose, *xRig.m_pxSkeleton);

	ZENITH_ASSERT_TRUE(WU61_RootPositionIs(xPose, Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f), 1e-5f),
		"an unbound space ignores the parameter set and stays on clip A");
	ZENITH_ASSERT_EQ_FLOAT(pxSpace->GetParameter(), 0.0f, 1e-5f, "its literal is untouched");
}

//=============================================================================
// (3) D42 — a value set on the CONTROLLER reaches a condition inside a
//     sub-state machine, whose declaration the controller never saw directly.
//=============================================================================
ZENITH_TEST(Animation, WU61_ControllerParameterReachesASubStateMachineCondition)
{
	WU61_Rig xRig;
	Flux_AnimationController xController;
	xController.Initialize(xRig.m_pxInstance);

	Flux_AnimationClip* pxWalk = WU61_MakeConstantClip("Walk", Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
	Flux_AnimationClip* pxRun = WU61_MakeConstantClip("Run", Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f));
	xController.GetClipCollection().AddClip(pxWalk);
	xController.GetClipCollection().AddClip(pxRun);

	Flux_AnimationStateMachine* pxRoot = xController.CreateStateMachine("Root");
	Flux_AnimationState* pxLoco = pxRoot->AddState("Loco");
	pxRoot->SetDefaultState("Loco");

	Flux_AnimationStateMachine* pxSub = pxLoco->CreateSubStateMachine("LocoSM");
	// ★ DECLARED ONLY ON THE SUB-MACHINE. The controller has to reach in and find
	// it, which is what SeedParametersInto's recursion is for.
	pxSub->GetParameters().AddFloat("Speed", 0.0f);
	pxSub->AddState("Walk")->SetBlendTree(new Flux_BlendTreeNode_Clip(pxWalk));
	pxSub->AddState("Run")->SetBlendTree(new Flux_BlendTreeNode_Clip(pxRun));
	pxSub->SetDefaultState("Walk");

	Flux_StateTransition xToRun;
	xToRun.m_strTargetStateName = "Run";
	xToRun.m_fTransitionDuration = 0.1f;
	Flux_TransitionCondition xCond;
	xCond.m_strParameterName = "Speed";
	xCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Greater;
	xCond.m_eParamType = Flux_AnimationParameters::ParamType::Float;
	xCond.m_fThreshold = 3.0f;
	xToRun.m_xConditions.PushBack(xCond);
	pxSub->GetState("Walk")->AddTransition(xToRun);

	xController.Update(1.0f / 60.0f);
	ZENITH_ASSERT_EQ(pxSub->GetCurrentStateName(), "Walk", "the sub-machine enters its default state");

	// The value is set on the CONTROLLER and named nowhere else.
	xController.SetFloat("Speed", 5.0f);
	ZENITH_ASSERT_TRUE(xController.GetParameters().HasParameter("Speed"),
		"the sub-machine's declaration was seeded into the controller's live set");

	for (u_int u = 0; u < 30; ++u)
		xController.Update(1.0f / 60.0f);

	ZENITH_ASSERT_EQ(pxSub->GetCurrentStateName(), "Run",
		"a controller-level parameter drives a condition two levels down");
}

//=============================================================================
// (3b) D42 — THE REGRESSION THAT WAS LIVE IN A SHIPPING GAME. A layered
//      controller has a NULL m_pxStateMachine, and every parameter shortcut used
//      to be `if (m_pxStateMachine) …`, so Zenith_AnimatorComponent::SetFloat was
//      a silent no-op for Zenithmon's humans: ZM_PlayerController set "Speed"
//      every frame and the Idle<->Walk transition never saw a thing.
//=============================================================================
ZENITH_TEST(Animation, WU61_ControllerParameterReachesALayerStateMachine)
{
	WU61_Rig xRig;
	Flux_AnimationController xController;
	xController.Initialize(xRig.m_pxInstance);

	ZENITH_ASSERT_FALSE(xController.HasStateMachine(),
		"this controller is LAYER-only — the shape the old shortcuts could not reach");

	Flux_AnimationClip* pxIdle = WU61_MakeConstantClip("Idle", Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
	Flux_AnimationClip* pxWalk = WU61_MakeConstantClip("Walk", Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f));
	xController.GetClipCollection().AddClip(pxIdle);
	xController.GetClipCollection().AddClip(pxWalk);

	Flux_AnimationLayer* pxLayer = xController.AddLayer("Base");
	Flux_AnimationStateMachine* pxSM = pxLayer->CreateStateMachine("BaseSM");
	WU61_AuthorLocomotion(pxSM->GetDef(), pxIdle, pxWalk);

	xController.Update(1.0f / 60.0f);
	ZENITH_ASSERT_EQ(pxSM->GetCurrentStateName(), "Idle", "starts idle");

	xController.SetFloat("Speed", 1.0f);
	for (u_int u = 0; u < 40; ++u)
		xController.Update(1.0f / 60.0f);

	ZENITH_ASSERT_EQ(pxSM->GetCurrentStateName(), "Walk",
		"a controller-level parameter reaches a LAYER's state machine");
}

//=============================================================================
// (4) The def round-trips: states, transitions, any-state transitions,
//     parameter declarations WITH their defaults, the default state, and the
//     blend-space parameter BINDING (which is authored data, unlike the value
//     it reads).
//=============================================================================
ZENITH_TEST(Animation, WU61_DefRoundTripsThroughTheStream)
{
	Flux_AnimationStateMachineDef xOriginal("RoundTrip");
	xOriginal.GetParameterDeclarations().AddFloat("Speed", 2.5f);
	xOriginal.GetParameterDeclarations().AddTrigger("Hit");

	Flux_BlendTreeNode_BlendSpace1D* pxSpace = new Flux_BlendTreeNode_BlendSpace1D();
	pxSpace->AddBlendPoint(new Flux_BlendTreeNode_Clip(), 0.0f);
	pxSpace->AddBlendPoint(new Flux_BlendTreeNode_Clip(), 1.0f);
	pxSpace->SetParameterName("Speed");

	xOriginal.AddState("Move")->SetBlendTree(pxSpace);
	xOriginal.AddState("Hurt");
	xOriginal.SetDefaultState("Move");

	Flux_StateTransition xToHurt;
	xToHurt.m_strTargetStateName = "Hurt";
	xToHurt.m_fTransitionDuration = 0.42f;
	xToHurt.m_iPriority = 7;
	Flux_TransitionCondition xCond;
	xCond.m_strParameterName = "Hit";
	xCond.m_eParamType = Flux_AnimationParameters::ParamType::Trigger;
	xToHurt.m_xConditions.PushBack(xCond);
	xOriginal.AddAnyStateTransition(xToHurt);

	Flux_StateTransition xMoveToHurt;
	xMoveToHurt.m_strTargetStateName = "Hurt";
	xMoveToHurt.m_fTransitionDuration = 0.11f;
	xOriginal.GetState("Move")->AddTransition(xMoveToHurt);

	Zenith_DataStream xStream(1);
	xOriginal.WriteToDataStream(xStream);
	xStream.SetCursor(0);

	Flux_AnimationStateMachineDef xLoaded;
	xLoaded.ReadFromDataStream(xStream);

	ZENITH_ASSERT_EQ(xLoaded.GetName(), "RoundTrip", "name round-trips");
	ZENITH_ASSERT_EQ(xLoaded.GetDefaultStateName(), "Move", "default state round-trips");
	ZENITH_ASSERT_EQ(static_cast<uint32_t>(xLoaded.GetStates().GetSize()), 2u, "both states round-trip");
	ZENITH_ASSERT_TRUE(xLoaded.HasState("Move") && xLoaded.HasState("Hurt"), "by name");

	ZENITH_ASSERT_TRUE(xLoaded.GetParameterDeclarations().HasParameter("Speed"), "the float declaration survives");
	ZENITH_ASSERT_EQ_FLOAT(xLoaded.GetParameterDeclarations().GetFloat("Speed"), 2.5f, 1e-5f,
		"and so does its DEFAULT — a declaration without one seeds a controller with the wrong value");
	ZENITH_ASSERT_TRUE(xLoaded.GetParameterDeclarations().HasParameter("Hit"), "the trigger declaration survives");

	ZENITH_ASSERT_EQ(xLoaded.GetAnyStateTransitions().GetSize(), 1u, "the any-state transition survives");
	if (xLoaded.GetAnyStateTransitions().GetSize() == 1)
	{
		const Flux_StateTransition& xTrans = xLoaded.GetAnyStateTransitions().Get(0);
		ZENITH_ASSERT_EQ(xTrans.m_strTargetStateName, "Hurt", "with its target");
		ZENITH_ASSERT_EQ(xTrans.m_iPriority, 7, "its priority");
		ZENITH_ASSERT_EQ(xTrans.m_xConditions.GetSize(), 1u, "and its condition");
	}

	const Flux_AnimationState* pxLoadedMove = xLoaded.GetState("Move");
	ZENITH_ASSERT_NOT_NULL(pxLoadedMove, "Move survives");
	if (pxLoadedMove)
	{
		ZENITH_ASSERT_EQ(pxLoadedMove->GetTransitions().GetSize(), 1u, "with its own transition");
		ZENITH_ASSERT_EQ_FLOAT(pxLoadedMove->GetTransitions().Get(0).m_fTransitionDuration, 0.11f, 1e-5f,
			"and that transition's duration");

		// ★ THE BINDING IS THE NEW BYTES (D48). Without it a saved controller comes
		// back with a blend space that no longer tracks anything — which is exactly
		// the frozen-literal failure this work exists to remove, reintroduced by
		// the round trip instead of by the missing call.
		Flux_BlendTreeNode* pxTree = pxLoadedMove->GetBlendTree();
		ZENITH_ASSERT_NOT_NULL(pxTree, "the blend tree survives");
		if (pxTree)
		{
			ZENITH_ASSERT_STREQ(pxTree->GetNodeTypeName(), "BlendSpace1D", "as the right node type");
			Flux_BlendTreeNode_BlendSpace1D* pxLoadedSpace = static_cast<Flux_BlendTreeNode_BlendSpace1D*>(pxTree);
			ZENITH_ASSERT_EQ(pxLoadedSpace->GetParameterName(), "Speed", "and it is still bound to Speed");
			ZENITH_ASSERT_EQ(pxLoadedSpace->GetBlendPoints().GetSize(), 2u, "with both blend points");
		}
	}
}
