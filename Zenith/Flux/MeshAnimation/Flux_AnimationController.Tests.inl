#include "Core/Zenith_TestFramework.h"
#include "Flux/MeshAnimation/Flux_AnimationController.h"
#include "Flux/MeshAnimation/Flux_SkeletonInstance.h"

// ============================================================================
// WU-5A — RUNTIME EVENT DELIVERY (D34 - D40)
//
// ★ WHAT THESE EXIST TO CATCH. Before this unit, ProcessEvents had ONE call
// site: inside #ifdef ZENITH_TOOLS, gated on the tools-only direct-play node,
// and the function itself sourced the clip only from that same node. So a
// shipping build fired nothing at all, and a tools build fired nothing that a
// state machine or a layer played. Every one of these tests therefore drives a
// STATE MACHINE or a LAYER — a test that reached for PlayClip would pass
// against the broken code it replaces.
//
// Everything here runs headless on the Null backend: an in-memory
// Zenith_SkeletonAsset, in-memory clips, and a Flux_AnimationController
// Initialize'd on a Flux_SkeletonInstance. No graphics, no assets on disk.
// ============================================================================

namespace
{
	// A two-bone rig, matching the one the Animation suite already uses. The
	// controller needs a skeleton instance before Update will do anything at all.
	struct WU5A_Rig
	{
		Zenith_SkeletonAsset* m_pxSkeleton = nullptr;
		Flux_SkeletonInstance* m_pxInstance = nullptr;

		WU5A_Rig()
		{
			m_pxSkeleton = new Zenith_SkeletonAsset();
			const Zenith_Maths::Quat xIdentity = glm::identity<Zenith_Maths::Quat>();
			const Zenith_Maths::Vector3 xUnitScale(1.0f);
			m_pxSkeleton->AddBone("Root", -1, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), xIdentity, xUnitScale);
			m_pxSkeleton->AddBone("Child", 0, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), xIdentity, xUnitScale);
			m_pxSkeleton->ComputeBindPoseMatrices();
			m_pxInstance = Flux_SkeletonInstance::CreateFromAsset(m_pxSkeleton);
		}

		~WU5A_Rig()
		{
			delete m_pxInstance;
			delete m_pxSkeleton;
		}

		WU5A_Rig(const WU5A_Rig&) = delete;
		WU5A_Rig& operator=(const WU5A_Rig&) = delete;
	};

	// One keyed bone so SampleFromClip has something to do; the events are what
	// the tests are actually about.
	Flux_AnimationClip* WU5A_MakeClip(const char* szName, float fDurationSeconds, bool bLooping)
	{
		Flux_AnimationClip* pxClip = new Flux_AnimationClip();
		pxClip->SetName(szName);
		pxClip->SetDuration(fDurationSeconds);
		pxClip->SetLooping(bLooping);

		Flux_BoneChannel xChannel;
		xChannel.SetBoneName("Root");
		xChannel.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
		pxClip->AddBoneChannel("Root", std::move(xChannel));
		return pxClip;
	}

	// The event's own normalized time is stashed in m_xData.x so the sink can
	// record (name, time) — the callback signature carries no time of its own.
	void WU5A_AddEvent(Flux_AnimationClip& xClip, const char* szName, float fNormalizedTime)
	{
		Flux_AnimationEvent xEvent;
		xEvent.m_strEventName = szName;
		xEvent.m_fNormalizedTime = fNormalizedTime;
		xEvent.m_xData = Zenith_Maths::Vector4(fNormalizedTime, 0.0f, 0.0f, 0.0f);
		xClip.AddEvent(xEvent);
	}

	struct WU5A_EventSink
	{
		Zenith_Vector<std::string> m_xNames;
		Zenith_Vector<float> m_xTimes;

		u_int Count() const { return m_xNames.GetSize(); }

		u_int CountOf(const char* szName) const
		{
			u_int uCount = 0;
			for (u_int u = 0; u < m_xNames.GetSize(); ++u)
			{
				if (m_xNames.Get(u) == szName)
					uCount++;
			}
			return uCount;
		}

		void Reset()
		{
			m_xNames.Clear();
			m_xTimes.Clear();
		}
	};

	void WU5A_OnEvent(void* pUserData, const std::string& strEventName, const Zenith_Maths::Vector4& xData)
	{
		WU5A_EventSink* pxSink = static_cast<WU5A_EventSink*>(pUserData);
		pxSink->m_xNames.PushBack(strEventName);
		pxSink->m_xTimes.PushBack(xData.x);
	}

	// A state whose blend tree is one clip leaf. The state owns the node; the
	// node does NOT own the clip.
	Flux_AnimationState* WU5A_AddClipState(Flux_AnimationStateMachine& xSM,
		const char* szStateName, Flux_AnimationClip* pxClip)
	{
		Flux_AnimationState* pxState = xSM.AddState(szStateName);
		pxState->SetBlendTree(new Flux_BlendTreeNode_Clip(pxClip));
		return pxState;
	}

	void WU5A_Tick(Flux_AnimationController& xController, float fDt, u_int uCount)
	{
		for (u_int u = 0; u < uCount; ++u)
			xController.Update(fDt);
	}
}

//=============================================================================
// (1) A state machine — no direct play anywhere near it — fires a looping
//     clip's event once per loop.
//=============================================================================
ZENITH_TEST(Animation, WU5A_StateMachineLoopingClipEmitsOncePerLoop)
{
	WU5A_Rig xRig;
	Flux_AnimationController xController;
	xController.Initialize(xRig.m_pxInstance);

	Flux_AnimationClip* pxClip = WU5A_MakeClip("Walk", 1.0f, true);
	WU5A_AddEvent(*pxClip, "Footstep", 0.5f);

	Flux_AnimationStateMachine& xSM = xController.GetStateMachine();
	WU5A_AddClipState(xSM, "Walk", pxClip);
	xSM.SetDefaultState("Walk");

	WU5A_EventSink xSink;
	xController.SetEventCallback(&WU5A_OnEvent, &xSink);

	ZENITH_ASSERT_FALSE(xController.HasDirectPlayClip(),
		"nothing here uses the editor's direct-play path — that is the point of this test");

	// 12 ticks of 0.25s over a 1s clip is exactly three loops.
	WU5A_Tick(xController, 0.25f, 12);

	ZENITH_ASSERT_EQ(xSink.Count(), 3u,
		"three loops, three footsteps — a state machine's events fire in a NON-tools build (D34)");
	ZENITH_ASSERT_EQ(xSink.CountOf("Footstep"), 3u, "and they are the authored event");
	if (xSink.Count() > 0)
	{
		ZENITH_ASSERT_EQ_FLOAT(xSink.m_xTimes.Get(0), 0.5f, 1e-5f, "carrying the event's own payload");
	}

	delete pxClip;
}

//=============================================================================
// (2) The same through a layer, and SetEmitEvents(false) silences exactly that
//     layer (D36) without banking the silenced frames.
//=============================================================================
ZENITH_TEST(Animation, WU5A_LayerEmitsAndSetEmitEventsSilences)
{
	WU5A_Rig xRig;
	Flux_AnimationController xController;
	xController.Initialize(xRig.m_pxInstance);

	Flux_AnimationClip* pxClip = WU5A_MakeClip("Walk", 1.0f, true);
	WU5A_AddEvent(*pxClip, "Footstep", 0.5f);

	Flux_AnimationLayer* pxLayer = xController.AddLayer("Base");
	Flux_AnimationStateMachine* pxLayerSM = pxLayer->CreateStateMachine("BaseSM");
	WU5A_AddClipState(*pxLayerSM, "Walk", pxClip);
	pxLayerSM->SetDefaultState("Walk");

	WU5A_EventSink xSink;
	xController.SetEventCallback(&WU5A_OnEvent, &xSink);

	ZENITH_ASSERT_TRUE(pxLayer->GetEmitEvents(), "a layer emits by default (D36)");

	WU5A_Tick(xController, 0.25f, 4);   // one loop
	ZENITH_ASSERT_EQ(xSink.Count(), 1u, "a LAYER's events fire too, not just a top-level state machine");

	pxLayer->SetEmitEvents(false);
	xSink.Reset();
	WU5A_Tick(xController, 0.25f, 4);   // a second loop, silenced
	ZENITH_ASSERT_EQ(xSink.Count(), 0u, "m_bEmitEvents = false silences the layer");

	// ★ AND SILENCING DOES NOT BANK. A silenced layer is still walked, so
	// re-enabling it fires the NEXT crossing and not the ones it sat out.
	pxLayer->SetEmitEvents(true);
	xSink.Reset();
	WU5A_Tick(xController, 0.25f, 4);   // a third loop
	ZENITH_ASSERT_EQ(xSink.Count(), 1u, "re-enabling fires the next crossing only — no banked burst");

	delete pxClip;
}

//=============================================================================
// (3) Two leaves crossing the same instant in one layer: the heavier one emits
//     and the lighter one does not (D35).
//=============================================================================
ZENITH_TEST(Animation, WU5A_HighestWeightLeafInALayerEmits)
{
	WU5A_Rig xRig;
	Flux_AnimationController xController;
	xController.Initialize(xRig.m_pxInstance);

	// Same duration and rate, so both leaves cross their 0.5 event on the SAME
	// tick — there is no ordering-in-time escape hatch, the weight has to decide.
	Flux_AnimationClip* pxClipA = WU5A_MakeClip("A", 1.0f, true);
	Flux_AnimationClip* pxClipB = WU5A_MakeClip("B", 1.0f, true);
	WU5A_AddEvent(*pxClipA, "BeatA", 0.5f);
	WU5A_AddEvent(*pxClipB, "BeatB", 0.5f);

	Flux_BlendTreeNode_BlendSpace1D* pxBlendSpace = new Flux_BlendTreeNode_BlendSpace1D();
	pxBlendSpace->AddBlendPoint(new Flux_BlendTreeNode_Clip(pxClipA), 0.0f);
	pxBlendSpace->AddBlendPoint(new Flux_BlendTreeNode_Clip(pxClipB), 1.0f);
	pxBlendSpace->SetParameter(0.75f);   // A = 0.25, B = 0.75

	Flux_AnimationStateMachine& xSM = xController.GetStateMachine();
	Flux_AnimationState* pxState = xSM.AddState("Locomotion");
	pxState->SetBlendTree(pxBlendSpace);
	xSM.SetDefaultState("Locomotion");

	WU5A_EventSink xSink;
	xController.SetEventCallback(&WU5A_OnEvent, &xSink);

	WU5A_Tick(xController, 0.25f, 4);   // one loop

	ZENITH_ASSERT_EQ(xSink.Count(), 1u, "one layer emits ONCE per crossing instant, whatever is playing");
	ZENITH_ASSERT_EQ(xSink.CountOf("BeatB"), 1u, "the heavier leaf (0.75) is the one that emits");
	ZENITH_ASSERT_EQ(xSink.CountOf("BeatA"), 0u, "the lighter leaf (0.25) stays quiet");

	delete pxClipA;
	delete pxClipB;
}

//=============================================================================
// (3b) A dead-even weight goes to the LOWEST leaf index (D35).
//=============================================================================
ZENITH_TEST(Animation, WU5A_WeightTieGoesToTheLowestLeafIndex)
{
	WU5A_Rig xRig;
	Flux_AnimationController xController;
	xController.Initialize(xRig.m_pxInstance);

	Flux_AnimationClip* pxClipA = WU5A_MakeClip("A", 1.0f, true);
	Flux_AnimationClip* pxClipB = WU5A_MakeClip("B", 1.0f, true);
	WU5A_AddEvent(*pxClipA, "BeatA", 0.5f);
	WU5A_AddEvent(*pxClipB, "BeatB", 0.5f);

	Flux_BlendTreeNode_BlendSpace1D* pxBlendSpace = new Flux_BlendTreeNode_BlendSpace1D();
	pxBlendSpace->AddBlendPoint(new Flux_BlendTreeNode_Clip(pxClipA), 0.0f);   // leaf index 0
	pxBlendSpace->AddBlendPoint(new Flux_BlendTreeNode_Clip(pxClipB), 1.0f);   // leaf index 1
	pxBlendSpace->SetParameter(0.5f);   // exactly 0.5 / 0.5

	Flux_AnimationStateMachine& xSM = xController.GetStateMachine();
	Flux_AnimationState* pxState = xSM.AddState("Locomotion");
	pxState->SetBlendTree(pxBlendSpace);
	xSM.SetDefaultState("Locomotion");

	WU5A_EventSink xSink;
	xController.SetEventCallback(&WU5A_OnEvent, &xSink);

	WU5A_Tick(xController, 0.25f, 4);

	ZENITH_ASSERT_EQ(xSink.Count(), 1u, "a tie still emits exactly once");
	ZENITH_ASSERT_EQ(xSink.CountOf("BeatA"), 1u, "and the lower leaf index takes it (D35)");

	delete pxClipA;
	delete pxClipB;
}

//=============================================================================
// (4) A crossfade emits from ONE side — the one at weight >= 0.5 (D37).
//=============================================================================
ZENITH_TEST(Animation, WU5A_CrossFadeEmitsOnlyFromTheDominantSide)
{
	WU5A_Rig xRig;
	Flux_AnimationController xController;
	xController.Initialize(xRig.m_pxInstance);

	Flux_AnimationClip* pxClipA = WU5A_MakeClip("A", 1.0f, true);
	Flux_AnimationClip* pxClipB = WU5A_MakeClip("B", 1.0f, true);
	WU5A_AddEvent(*pxClipA, "BeatA", 0.5f);
	WU5A_AddEvent(*pxClipB, "BeatB", 0.5f);

	Flux_AnimationStateMachine& xSM = xController.GetStateMachine();
	WU5A_AddClipState(xSM, "A", pxClipA);
	WU5A_AddClipState(xSM, "B", pxClipB);
	xSM.SetDefaultState("A");

	WU5A_EventSink xSink;
	xController.SetEventCallback(&WU5A_OnEvent, &xSink);

	xController.Update(0.1f);   // enter A; nothing crossed yet
	ZENITH_ASSERT_EQ(xSM.GetCurrentStateName(), "A", "the state machine entered its default state");
	ZENITH_ASSERT_EQ(xSink.Count(), 0u, "and crossed nothing on the way in");

	// A four-second crossfade at 0.25s per tick. B's tree is Reset on entry, so
	// B crosses its 0.5 beat on transition ticks 3, 7 and 11. The transition's
	// eased weight passes 0.5 at tick 8, so the first two crossings are BELOW
	// the threshold and the third is above it.
	xController.CrossFade("B", 4.0f);
	xSink.Reset();
	WU5A_Tick(xController, 0.25f, 12);

	ZENITH_ASSERT_EQ(xSink.CountOf("BeatB"), 1u,
		"B's beat fires only once the transition has handed B the majority weight");
	ZENITH_ASSERT_EQ(xSink.CountOf("BeatA"), 0u,
		"and the outgoing state contributes nothing — UpdateTransition freezes it to a pose snapshot");

	delete pxClipA;
	delete pxClipB;
}

//=============================================================================
// (4b) EXACTLY 0.5 goes to the TARGET (D37). The threshold is >=, not >.
//=============================================================================
ZENITH_TEST(Animation, WU5A_CrossFadeExactHalfWeightGoesToTheTarget)
{
	WU5A_Rig xRig;
	Flux_AnimationController xController;
	xController.Initialize(xRig.m_pxInstance);

	Flux_AnimationClip* pxClipA = WU5A_MakeClip("A", 1.0f, true);
	Flux_AnimationClip* pxClipB = WU5A_MakeClip("B", 1.0f, true);
	WU5A_AddEvent(*pxClipB, "BeatB", 0.5f);

	Flux_AnimationStateMachine& xSM = xController.GetStateMachine();
	WU5A_AddClipState(xSM, "A", pxClipA);
	WU5A_AddClipState(xSM, "B", pxClipB);
	xSM.SetDefaultState("A");

	WU5A_EventSink xSink;
	xController.SetEventCallback(&WU5A_OnEvent, &xSink);

	xController.Update(0.1f);

	// ★ THE ARITHMETIC IS EXACT, ON PURPOSE. A 1.5s crossfade ticked at 0.25s
	// puts the third tick at elapsed 0.75 == half the duration, and the default
	// smoothstep easing maps 0.5 to exactly 0.5. B crosses its 0.5 beat on that
	// same third tick (its tree was Reset on entry), so the crossing lands on
	// the tie. A `> 0.5` threshold would emit nothing here.
	xController.CrossFade("B", 1.5f);
	xSink.Reset();
	WU5A_Tick(xController, 0.25f, 3);

	ZENITH_ASSERT_EQ(xSink.CountOf("BeatB"), 1u, "a dead-even crossfade lets the TARGET emit (D37)");

	delete pxClipA;
	delete pxClipB;
}

//=============================================================================
// (5) The loop boundary (D38): 0.0 and 1.0 on a looping clip are one instant,
//     and it comes round once per loop.
//=============================================================================
ZENITH_TEST(Animation, WU5A_LoopBoundaryEventsFireOncePerLoop)
{
	WU5A_Rig xRig;
	Flux_AnimationController xController;
	xController.Initialize(xRig.m_pxInstance);

	Flux_AnimationClip* pxClip = WU5A_MakeClip("Walk", 1.0f, true);
	WU5A_AddEvent(*pxClip, "AtZero", 0.0f);
	WU5A_AddEvent(*pxClip, "AtOne", 1.0f);

	Flux_AnimationStateMachine& xSM = xController.GetStateMachine();
	WU5A_AddClipState(xSM, "Walk", pxClip);
	xSM.SetDefaultState("Walk");

	WU5A_EventSink xSink;
	xController.SetEventCallback(&WU5A_OnEvent, &xSink);

	WU5A_Tick(xController, 0.25f, 12);   // three loops

	ZENITH_ASSERT_EQ(xSink.CountOf("AtZero"), 3u,
		"an event at 0.0 comes round once per loop — not twice at the boundary, not never");
	// ★ 1.0 IS 0.0 OF THE NEXT LOOP (D38). Half-open [prev, curr) can never
	// contain 1.0 on a clip that wraps to 0, so without the fold this is zero.
	ZENITH_ASSERT_EQ(xSink.CountOf("AtOne"), 3u,
		"and an event authored at 1.0 is the same instant, folded to 0.0");

	delete pxClip;
}

//=============================================================================
// (5b) A NON-looping clip stops AT 1.0, and an event there fires exactly once.
//=============================================================================
ZENITH_TEST(Animation, WU5A_NonLoopingEndEventFiresExactlyOnce)
{
	WU5A_Rig xRig;
	Flux_AnimationController xController;
	xController.Initialize(xRig.m_pxInstance);

	Flux_AnimationClip* pxClip = WU5A_MakeClip("Attack", 1.0f, false);
	WU5A_AddEvent(*pxClip, "Recover", 1.0f);

	Flux_AnimationStateMachine& xSM = xController.GetStateMachine();
	WU5A_AddClipState(xSM, "Attack", pxClip);
	xSM.SetDefaultState("Attack");

	WU5A_EventSink xSink;
	xController.SetEventCallback(&WU5A_OnEvent, &xSink);

	// Four ticks reach the end; the next four sit on it. The clamp means every
	// later frame's span is [1, 1) — empty — so the event must not repeat.
	WU5A_Tick(xController, 0.25f, 8);

	ZENITH_ASSERT_EQ(xSink.CountOf("Recover"), 1u,
		"a clip that STOPS at 1.0 closes the top of its last span, once (D38)");

	delete pxClip;
}

//=============================================================================
// (6) Reverse emits nothing, still moves every mark, and does not bank a burst
//     for the next forward frame (D39).
//=============================================================================
ZENITH_TEST(Animation, WU5A_ReversePlaybackEmitsNothingAndStillAdvancesTheMark)
{
	WU5A_Rig xRig;
	Flux_AnimationController xController;
	xController.Initialize(xRig.m_pxInstance);

	Flux_AnimationClip* pxClip = WU5A_MakeClip("Walk", 1.0f, true);
	WU5A_AddEvent(*pxClip, "Early", 0.1f);
	WU5A_AddEvent(*pxClip, "Beat", 0.5f);

	Flux_AnimationStateMachine& xSM = xController.GetStateMachine();
	Flux_AnimationState* pxState = xSM.AddState("Walk");
	Flux_BlendTreeNode_Clip* pxLeaf = new Flux_BlendTreeNode_Clip(pxClip);
	pxState->SetBlendTree(pxLeaf);
	xSM.SetDefaultState("Walk");

	WU5A_EventSink xSink;
	xController.SetEventCallback(&WU5A_OnEvent, &xSink);

	// --- Forward pass over one whole loop: each event exactly once.
	WU5A_Tick(xController, 0.25f, 4);
	ZENITH_ASSERT_EQ(xSink.CountOf("Early"), 1u, "the forward pass crosses Early once");
	ZENITH_ASSERT_EQ(xSink.CountOf("Beat"), 1u, "and Beat once");
	ZENITH_ASSERT_EQ(xSink.Count(), 2u, "and nothing else");

	// --- Reverse back across BOTH of them.
	xSink.Reset();
	xController.SetPlaybackSpeed(-1.0f);
	WU5A_Tick(xController, 0.25f, 2);   // 0.0 -> 0.75 -> 0.5

	ZENITH_ASSERT_EQ(xSink.Count(), 0u,
		"reverse emits NOTHING — and it is an early return, not an assert: SetPlaybackSpeed takes negatives");
	// ★ THE MARK MOVED ANYWAY. This is the half that is easy to forget and
	// impossible to see from the callback: the leaf's own previous time tracked
	// the reverse steps instead of staying at the last forward crossing.
	ZENITH_ASSERT_EQ_FLOAT(pxLeaf->GetPreviousTimestamp(), 0.75f, 1e-4f,
		"the leaf's previous time followed the reverse step (D39)");
	ZENITH_ASSERT_EQ_FLOAT(pxLeaf->GetCurrentTimestamp(), 0.5f, 1e-4f, "and the playhead is where reverse left it");

	// --- Forward again: the span the reverse walked back over is NOT replayed.
	xSink.Reset();
	xController.SetPlaybackSpeed(1.0f);
	WU5A_Tick(xController, 0.25f, 1);   // 0.5 -> 0.75

	ZENITH_ASSERT_EQ(xSink.Count(), 1u, "exactly the one crossing the forward step made");
	ZENITH_ASSERT_EQ(xSink.CountOf("Beat"), 1u, "which is Beat at 0.5");
	ZENITH_ASSERT_EQ(xSink.CountOf("Early"), 0u,
		"Early is NOT replayed — a mark left behind by the reverse would have fired it again");

	delete pxClip;
}

//=============================================================================
// The span predicate itself (D38). Pure, so it can be pinned directly rather
// than inferred from four frames of playback.
//=============================================================================
ZENITH_TEST(Animation, WU5A_SpanContainsEventTimeRules)
{
	Flux_ClipEventSpan xSpan;
	xSpan.m_bForward = true;
	xSpan.m_bLooping = true;
	xSpan.m_fPrevNormalizedTime = 0.25f;
	xSpan.m_fCurrNormalizedTime = 0.75f;

	ZENITH_ASSERT_TRUE(Flux_AnimationController::SpanContainsEventTime(xSpan, 0.25f), "the bottom end is CLOSED");
	ZENITH_ASSERT_FALSE(Flux_AnimationController::SpanContainsEventTime(xSpan, 0.75f), "the top end is OPEN");
	ZENITH_ASSERT_TRUE(Flux_AnimationController::SpanContainsEventTime(xSpan, 0.5f), "and the inside is inside");

	// Reverse: nothing, whatever the times say (D39).
	xSpan.m_bForward = false;
	ZENITH_ASSERT_FALSE(Flux_AnimationController::SpanContainsEventTime(xSpan, 0.5f), "a reverse span contains nothing");

	// Wrapped: [prev, 1) U [0, curr), as ONE test — an event inside both ranges
	// (which a step longer than the clip produces) must not count twice, and
	// since this returns a bool it structurally cannot.
	xSpan.m_bForward = true;
	xSpan.m_bWrapped = true;
	xSpan.m_fPrevNormalizedTime = 0.9f;
	xSpan.m_fCurrNormalizedTime = 0.1f;
	ZENITH_ASSERT_TRUE(Flux_AnimationController::SpanContainsEventTime(xSpan, 0.95f), "above prev is in");
	ZENITH_ASSERT_TRUE(Flux_AnimationController::SpanContainsEventTime(xSpan, 0.05f), "below curr is in");
	ZENITH_ASSERT_FALSE(Flux_AnimationController::SpanContainsEventTime(xSpan, 0.5f), "the middle is out");
	ZENITH_ASSERT_TRUE(Flux_AnimationController::SpanContainsEventTime(xSpan, 1.0f),
		"and 1.0 folds to 0.0 on a looping clip, landing in [0, curr) (D38)");

	// Non-looping, stopped at the end: the one CLOSED top end.
	xSpan.m_bWrapped = false;
	xSpan.m_bLooping = false;
	xSpan.m_bReachedEnd = true;
	xSpan.m_fPrevNormalizedTime = 0.75f;
	xSpan.m_fCurrNormalizedTime = 1.0f;
	ZENITH_ASSERT_TRUE(Flux_AnimationController::SpanContainsEventTime(xSpan, 1.0f),
		"a clip that stops AT 1.0 must be able to fire an event authored there");

	// ...and only once: the frame after, prev has caught up and the span is empty.
	xSpan.m_bReachedEnd = false;
	xSpan.m_fPrevNormalizedTime = 1.0f;
	ZENITH_ASSERT_FALSE(Flux_AnimationController::SpanContainsEventTime(xSpan, 1.0f),
		"and the frames it spends parked there fire nothing");
}

#ifdef ZENITH_TOOLS
//=============================================================================
// (7) A scrub emits nothing unless asked, and moves the mark either way (D40).
//     Direct play is tools-only, so this test is too — it still runs headless
//     in the Null_*_True configuration every gate uses.
//=============================================================================
ZENITH_TEST(Animation, WU5A_SeekEmitsNothingUnlessAsked)
{
	WU5A_Rig xRig;
	Flux_AnimationClip* pxClip = WU5A_MakeClip("Walk", 2.0f, true);
	WU5A_AddEvent(*pxClip, "Mid", 0.5f);

	// The controller is scoped so its clip collection (which holds a BORROWED
	// reference to pxClip) is gone before the clip is.
	{
		Flux_AnimationController xController;
		xController.Initialize(xRig.m_pxInstance);
		xController.GetClipCollection().AddClipReference(pxClip);
		xController.PlayClip("Walk", 0.0f);
		ZENITH_ASSERT_TRUE(xController.HasDirectPlayClip(), "the direct-play preview is armed");

		WU5A_EventSink xSink;
		xController.SetEventCallback(&WU5A_OnEvent, &xSink);

		ZENITH_ASSERT_FALSE(xController.GetEmitEventsOnSeek(), "a scrub emits nothing by default (D40)");
		ZENITH_ASSERT_TRUE(xController.SeekDirectPlay(1.5f), "scrub straight past the event");
		ZENITH_ASSERT_EQ(xSink.Count(), 0u, "the scrub emitted nothing");
		ZENITH_ASSERT_EQ_FLOAT(xController.GetLastEventCheckTime(), 0.75f, 1e-4f,
			"but the mark moved anyway — 1.5s of a 2s clip (D40)");

		xController.SeekDirectPlay(0.0f);
		ZENITH_ASSERT_EQ(xSink.Count(), 0u, "still nothing");
		ZENITH_ASSERT_EQ_FLOAT(xController.GetLastEventCheckTime(), 0.0f, 1e-4f, "and the mark followed it back");

		xController.SetEmitEventsOnSeek(true);
		ZENITH_ASSERT_TRUE(xController.SeekDirectPlay(1.5f), "scrub forward across the event again");
		ZENITH_ASSERT_EQ(xSink.Count(), 1u, "with the flag on, the crossed event fires");
		if (xSink.Count() > 0)
		{
			ZENITH_ASSERT_TRUE(xSink.m_xNames.Get(0) == "Mid", "and it is the one the scrub crossed");
		}

		// A BACKWARD scrub is still silent, flag or no flag — same rule as reverse
		// playback (D39); nothing downstream is written to receive a clip's events
		// in reverse order.
		xSink.Reset();
		xController.SeekDirectPlay(0.0f);
		ZENITH_ASSERT_EQ(xSink.Count(), 0u, "a backward scrub emits nothing even with the flag on");
	}

	delete pxClip;
}
#endif
