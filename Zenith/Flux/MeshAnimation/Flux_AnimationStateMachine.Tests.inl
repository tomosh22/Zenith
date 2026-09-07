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

// ============================================================================
// WU-6.4 — HOT RELOAD AT STATE-MACHINE LEVEL (D45)
//
// ★ WHAT THESE EXIST TO CATCH. BuildFromDef is a DEMOLITION: ResetRuntime drops
// the current state and any transition in flight, and CopyFrom rebuilds every
// blend tree with its playhead at zero. So an author who changed one
// transition's duration while the game was running got the character snapped to
// the default state at frame 0 — and NOTHING asserted, because that is exactly
// what a cold load is supposed to do.
//
// Every test below therefore drives the machine FIRST (a state entered, a
// playhead advanced, a transition put in flight) and only then reloads. A test
// that reloaded a machine which had never ticked would pass against the
// demolition it replaces.
//
// Each of D45's rows appears twice, once in each direction: a state that
// survives and one that does not, a time that survives and one that resets, a
// transition that lands on its target and one whose target is gone.
// ============================================================================

namespace
{
	// The locomotion graph WU61_AuthorLocomotion builds, plus a THIRD state so a
	// fallback to the default can be told apart from staying where we were. That
	// distinction is the whole of test (6b): "Crouch" is the new default and
	// "Idle" is the state the machine was transitioning OUT of, so landing on
	// Idle would look like success under a two-state graph.
	void WU64_AuthorLocomotionWithCrouch(Flux_AnimationStateMachineDef& xDef,
		Flux_AnimationClip* pxIdleClip, Flux_AnimationClip* pxWalkClip,
		bool bIncludeWalk, const char* szDefaultState)
	{
		xDef.GetParameterDeclarations().AddFloat("Speed", 0.0f);

		xDef.AddState("Idle")->SetBlendTree(new Flux_BlendTreeNode_Clip(pxIdleClip));
		xDef.AddState("Crouch")->SetBlendTree(new Flux_BlendTreeNode_Clip(pxIdleClip));
		if (bIncludeWalk)
		{
			xDef.AddState("Walk")->SetBlendTree(new Flux_BlendTreeNode_Clip(pxWalkClip));

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

		xDef.SetDefaultState(szDefaultState);
	}
}

//=============================================================================
// (5) The current state and its playhead both survive an unrelated edit.
//=============================================================================
ZENITH_TEST(Animation, WU64_ReloadKeepsTheCurrentStateAndItsNormalizedTime)
{
	WU61_Rig xRig;

	Flux_AnimationClip* pxIdle = WU61_MakeConstantClip("Idle", Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
	Flux_AnimationClip* pxWalk = WU61_MakeConstantClip("Walk", Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f));
	Flux_AnimationClipCollection xClips;
	xClips.AddClip(pxIdle);
	xClips.AddClip(pxWalk);

	Flux_AnimationStateMachineDef xDef("Loco");
	WU64_AuthorLocomotionWithCrouch(xDef, pxIdle, pxWalk, true, "Idle");

	Flux_AnimationStateMachine xSM;
	xSM.BuildFromDef(xDef, &xClips);

	Flux_SkeletonPose xPose;
	xPose.Initialize(2);

	// Four ticks of 0.1s into a 1s looping clip: the playhead is at 0.4, which is
	// a value neither a Reset (0.0) nor a wrap could produce by accident.
	for (u_int u = 0; u < 4; ++u)
		xSM.Update(0.1f, xPose, *xRig.m_pxSkeleton);

	ZENITH_ASSERT_EQ(xSM.GetCurrentStateName(), "Idle", "the machine is in its default state");
	ZENITH_ASSERT_EQ_FLOAT(xSM.GetCurrentStateInfo().m_fNormalizedTime, 0.4f, 1e-4f,
		"and 0.4 of the way through it");

	// The edit: a state added somewhere else in the graph. Nothing about Idle
	// moved, and nothing about Idle should move.
	Flux_AnimationStateMachineDef xEdited("Loco");
	WU64_AuthorLocomotionWithCrouch(xEdited, pxIdle, pxWalk, true, "Idle");
	xEdited.AddState("Sprint")->SetBlendTree(new Flux_BlendTreeNode_Clip(pxWalk));

	const bool bSurvived = xSM.ReloadFromDef(xEdited, &xClips);

	ZENITH_ASSERT_TRUE(bSurvived, "the state the machine was in still exists, so the playback survived intact");
	ZENITH_ASSERT_EQ(xSM.GetCurrentStateName(), "Idle",
		"★ the reload did NOT snap the character back to the default state at frame 0");
	ZENITH_ASSERT_EQ_FLOAT(xSM.GetCurrentStateInfo().m_fNormalizedTime, 0.4f, 1e-4f,
		"★ and the playhead came with it — there is no SetNormalizedTime anywhere in the system, "
		"so this had to be put back through the leaf's SECONDS timestamp");
	ZENITH_ASSERT_TRUE(xEdited.HasState("Sprint"), "and the edit really was applied");
	ZENITH_ASSERT_TRUE(xSM.HasState("Sprint"), "to the machine, not only to the def on the stack");
}

//=============================================================================
// (5b) THE NEGATIVE. A current state the edit deleted falls to the new default,
//      and its normalized time does NOT come with it.
//=============================================================================
ZENITH_TEST(Animation, WU64_ReloadDropsADeletedCurrentStateToTheNewDefaultAtTimeZero)
{
	WU61_Rig xRig;

	Flux_AnimationClip* pxIdle = WU61_MakeConstantClip("Idle", Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
	Flux_AnimationClip* pxWalk = WU61_MakeConstantClip("Walk", Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f));
	Flux_AnimationClipCollection xClips;
	xClips.AddClip(pxIdle);
	xClips.AddClip(pxWalk);

	Flux_AnimationStateMachineDef xDef("Loco");
	WU64_AuthorLocomotionWithCrouch(xDef, pxIdle, pxWalk, true, "Idle");

	Flux_AnimationStateMachine xSM;
	xSM.BuildFromDef(xDef, &xClips);

	Flux_SkeletonPose xPose;
	xPose.Initialize(2);

	xSM.Update(0.1f, xPose, *xRig.m_pxSkeleton);       // enter Idle
	xSM.GetParameters().SetFloat("Speed", 1.0f);
	for (u_int u = 0; u < 8; ++u)                      // 0.8s: past the 0.25s crossfade
		xSM.Update(0.1f, xPose, *xRig.m_pxSkeleton);

	ZENITH_ASSERT_EQ(xSM.GetCurrentStateName(), "Walk", "the machine settled in Walk");
	ZENITH_ASSERT_FALSE(xSM.IsTransitioning(), "with the transition complete, so this is the CURRENT-state row");
	ZENITH_ASSERT_GT(xSM.GetCurrentStateInfo().m_fNormalizedTime, 0.0f,
		"and its playhead has moved off zero — otherwise the assertion below proves nothing");

	// The edit DELETES Walk. Crouch is the new default, and it is deliberately
	// NOT the state we came from.
	Flux_AnimationStateMachineDef xEdited("Loco");
	WU64_AuthorLocomotionWithCrouch(xEdited, pxIdle, pxWalk, false, "Crouch");

	const bool bSurvived = xSM.ReloadFromDef(xEdited, &xClips);

	ZENITH_ASSERT_FALSE(bSurvived, "the state the machine was in is gone, and the return value says so");
	ZENITH_ASSERT_EQ(xSM.GetCurrentStateName(), "Crouch",
		"a deleted current state falls to the new def's DEFAULT state (D45)");
	ZENITH_ASSERT_EQ_FLOAT(xSM.GetCurrentStateInfo().m_fNormalizedTime, 0.0f, 1e-5f,
		"★ and the time goes with it. A normalized time is a fraction of a PARTICULAR clip; "
		"carrying 0.8 onto an unrelated animation is a glitch nobody can trace to the edit");
}

//=============================================================================
// (6) An in-flight transition is CANCELLED onto its target, carrying the
//     target's own playhead (the target is what UpdateTransition advances).
//=============================================================================
ZENITH_TEST(Animation, WU64_ReloadCancelsAnInFlightTransitionOntoItsTarget)
{
	WU61_Rig xRig;

	Flux_AnimationClip* pxIdle = WU61_MakeConstantClip("Idle", Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
	Flux_AnimationClip* pxWalk = WU61_MakeConstantClip("Walk", Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f));
	Flux_AnimationClipCollection xClips;
	xClips.AddClip(pxIdle);
	xClips.AddClip(pxWalk);

	Flux_AnimationStateMachineDef xDef("Loco");
	WU64_AuthorLocomotionWithCrouch(xDef, pxIdle, pxWalk, true, "Idle");

	Flux_AnimationStateMachine xSM;
	xSM.BuildFromDef(xDef, &xClips);

	Flux_SkeletonPose xPose;
	xPose.Initialize(2);

	xSM.Update(0.05f, xPose, *xRig.m_pxSkeleton);      // enter Idle
	xSM.GetParameters().SetFloat("Speed", 1.0f);
	xSM.Update(0.05f, xPose, *xRig.m_pxSkeleton);      // start the 0.25s crossfade, tick it once

	ZENITH_ASSERT_TRUE(xSM.IsTransitioning(), "a transition is genuinely in flight");
	ZENITH_ASSERT_EQ(xSM.GetCurrentStateName(), "Idle",
		"and the CURRENT state is still the source — which is why 'snap to the target' is a rule at all");

	Flux_AnimationStateMachineDef xEdited("Loco");
	WU64_AuthorLocomotionWithCrouch(xEdited, pxIdle, pxWalk, true, "Idle");

	const bool bSurvived = xSM.ReloadFromDef(xEdited, &xClips);

	ZENITH_ASSERT_TRUE(bSurvived, "the transition's target still exists");
	ZENITH_ASSERT_FALSE(xSM.IsTransitioning(),
		"★ the transition is CANCELLED, not resumed — its source pose snapshot describes a graph that no longer exists");
	ZENITH_ASSERT_EQ(xSM.GetCurrentStateName(), "Walk",
		"★ and it is cancelled onto its TARGET. Landing back on the source would re-run the "
		"conditions from scratch and re-spend a one-shot the player already paid for");
	// StartTransition Resets the target's tree, then UpdateTransition advanced it
	// by one 0.05s tick of a 1s clip.
	ZENITH_ASSERT_EQ_FLOAT(xSM.GetCurrentStateInfo().m_fNormalizedTime, 0.05f, 1e-4f,
		"carrying the TARGET's playhead, which the crossfade had already been advancing");
}

//=============================================================================
// (6b) THE NEGATIVE. The target was deleted too, so it falls to the DEFAULT —
//      not back to the source, which is still there and would look like a pass.
//=============================================================================
ZENITH_TEST(Animation, WU64_ReloadFallsToTheDefaultWhenTheTransitionTargetIsGone)
{
	WU61_Rig xRig;

	Flux_AnimationClip* pxIdle = WU61_MakeConstantClip("Idle", Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
	Flux_AnimationClip* pxWalk = WU61_MakeConstantClip("Walk", Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f));
	Flux_AnimationClipCollection xClips;
	xClips.AddClip(pxIdle);
	xClips.AddClip(pxWalk);

	Flux_AnimationStateMachineDef xDef("Loco");
	WU64_AuthorLocomotionWithCrouch(xDef, pxIdle, pxWalk, true, "Idle");

	Flux_AnimationStateMachine xSM;
	xSM.BuildFromDef(xDef, &xClips);

	Flux_SkeletonPose xPose;
	xPose.Initialize(2);

	xSM.Update(0.05f, xPose, *xRig.m_pxSkeleton);
	xSM.GetParameters().SetFloat("Speed", 1.0f);
	xSM.Update(0.05f, xPose, *xRig.m_pxSkeleton);
	ZENITH_ASSERT_TRUE(xSM.IsTransitioning(), "a transition is in flight");

	// Walk is deleted; the SOURCE (Idle) survives and the DEFAULT is Crouch.
	Flux_AnimationStateMachineDef xEdited("Loco");
	WU64_AuthorLocomotionWithCrouch(xEdited, pxIdle, pxWalk, false, "Crouch");

	const bool bSurvived = xSM.ReloadFromDef(xEdited, &xClips);

	ZENITH_ASSERT_FALSE(bSurvived, "neither the target nor anything else the snapshot named came back");
	ZENITH_ASSERT_FALSE(xSM.IsTransitioning(), "the transition is still cancelled");
	ZENITH_ASSERT_EQ(xSM.GetCurrentStateName(), "Crouch",
		"★ the fallback is the new DEFAULT state, not the surviving source. The machine had already "
		"left Idle — putting it back there would resurrect a state the graph had exited");
	ZENITH_ASSERT_EQ_FLOAT(xSM.GetCurrentStateInfo().m_fNormalizedTime, 0.0f, 1e-5f, "at time 0");
}

//=============================================================================
// (7) A STANDALONE machine owns its parameter declarations (no controller has
//     published a shared set), so the reload restores those: matched on name
//     AND type, and the negative of both in the same run.
//=============================================================================
ZENITH_TEST(Animation, WU64_ReloadKeepsAStandaloneMachinesParameterValuesByNameAndType)
{
	Flux_AnimationStateMachine xSM("Params");
	ZENITH_ASSERT_NULL(xSM.GetSharedParameters(),
		"no controller has published to this machine, so GetParameters() IS its own declaration table");

	xSM.GetDef().GetParameterDeclarations().AddFloat("Speed", 0.0f);
	xSM.GetDef().GetParameterDeclarations().AddInt("Combo", 0);
	xSM.GetDef().GetParameterDeclarations().AddFloat("Stamina", 0.0f);
	xSM.AddState("Idle");
	xSM.SetDefaultState("Idle");

	xSM.GetParameters().SetFloat("Speed", 3.5f);
	xSM.GetParameters().SetInt("Combo", 2);
	xSM.GetParameters().SetFloat("Stamina", 7.25f);

	Flux_AnimationStateMachineDef xEdited("Params");
	xEdited.GetParameterDeclarations().AddFloat("Speed", 0.0f);        // unchanged: name AND type match
	xEdited.GetParameterDeclarations().AddFloat("Combo", 9.0f);        // RETYPED int -> float
	xEdited.GetParameterDeclarations().AddFloat("Endurance", 1.5f);    // RENAMED from "Stamina"
	xEdited.AddState("Idle");
	xEdited.SetDefaultState("Idle");

	xSM.ReloadFromDef(xEdited, nullptr);

	ZENITH_ASSERT_EQ_FLOAT(xSM.GetParameters().GetFloat("Speed"), 3.5f, 1e-5f,
		"a parameter matched on name AND type keeps the value gameplay put in it");

	ZENITH_ASSERT_EQ_FLOAT(xSM.GetParameters().GetFloat("Combo"), 9.0f, 1e-5f,
		"★ a RETYPED parameter gets the new default. Flux_AnimationParameters::Parameter holds its "
		"value in a UNION, so a name-only match would have handed the float 2 as a bit pattern");

	ZENITH_ASSERT_FALSE(xSM.GetParameters().HasParameter("Stamina"),
		"★ the old name is GONE — a reload that merely re-SEEDED would leave it in the set forever, "
		"since SeedInto never overwrites and nothing else could ever remove it");
	ZENITH_ASSERT_TRUE(xSM.GetParameters().HasParameter("Endurance"), "and the renamed one is declared");
	ZENITH_ASSERT_EQ_FLOAT(xSM.GetParameters().GetFloat("Endurance"), 1.5f, 1e-5f,
		"★ carrying the new DEFAULT, not the 7.25 the old name was holding — a rename is an edit");
}
