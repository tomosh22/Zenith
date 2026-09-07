#include "Core/Zenith_TestFramework.h"
#include "Flux/MeshAnimation/Flux_AnimationController.h"
#include "Flux/MeshAnimation/Flux_AnimatorControllerDef.h"   // WU-6.3: the def-rebuild reorder
#include "Flux/MeshAnimation/Flux_SkeletonInstance.h"
#include "AssetHandling/Zenith_AnimationAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"
#include "DataStream/Zenith_DataStream.h"                    // WU-6.3: the .zscen-shaped round trip

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

//=============================================================================
// (8) ASSET REFERENCES — Initialize(nullptr) detaches, and
//     ReleaseAssetReferences drops the lot.
//
// ★ WHAT THIS EXISTS TO CATCH, AND WHY IT WAS INVISIBLE. Initialize() used to
// Set() the skeleton handle INSIDE its `if (pxSkeleton)` branch, so
// Initialize(nullptr) left an AddRef'd cached pointer behind with nothing
// anywhere to clear it — the handle only died in ~Flux_AnimationController.
// For the editor's preview session, owned by a panel that reached ATEXIT, that
// destructor ran after Zenith_AssetRegistry::Shutdown had force-deleted the
// asset, so Release() wrote into freed memory: an assert when the freed word
// happened to read zero, silent corruption when it did not.
//
// Every assertion below is RELATIVE to a measured baseline rather than to an
// absolute count — the rig's own Flux_SkeletonInstance holds a reference of its
// own, and pinning the absolute number would be pinning that instead.
//=============================================================================
ZENITH_TEST(Animation, ControllerReleasesEveryAssetReferenceItHolds)
{
	WU5A_Rig xRig;
	Zenith_SkeletonAsset* pxSkeletonAsset = xRig.m_pxSkeleton;
	const uint32_t uSkeletonBaseline = pxSkeletonAsset->GetRefCount();

	// A registry-resident clip asset, so AddClipFromFile has something to resolve
	// and to pin. Procedural rather than a temp .zanim on disk: the reference
	// counting is the subject, and a file would add a parse and an I/O failure mode
	// that say nothing about it.
	const std::string strClipAssetPath = "procedural://anim_controller_release_probe";
	{
		AnimationHandle xClipAsset = Zenith_AssetRegistry::Create<Zenith_AnimationAsset>(strClipAssetPath);
		Zenith_AnimationAsset* pxClipAsset = xClipAsset.GetDirect();
		ZENITH_ASSERT_TRUE(pxClipAsset != nullptr, "the registry created the procedural animation asset");
		pxClipAsset->SetClip(WU5A_MakeClip("Held", 1.0f, true));   // the asset takes ownership
		const uint32_t uClipBaseline = pxClipAsset->GetRefCount();

		Flux_AnimationController xController;

		//---------------------------------------------------------------------
		// Initialize(nullptr) is a DETACH, not a forget.
		//---------------------------------------------------------------------
		xController.Initialize(xRig.m_pxInstance);
		ZENITH_ASSERT_EQ(pxSkeletonAsset->GetRefCount(), uSkeletonBaseline + 1u,
			"attaching pins the skeleton asset so UnloadUnused cannot free the bone data mid-frame");

		xController.Initialize(nullptr);
		ZENITH_ASSERT_FALSE(xController.IsInitialized(), "Initialize(nullptr) detached the instance");
		ZENITH_ASSERT_EQ(pxSkeletonAsset->GetRefCount(), uSkeletonBaseline,
			"★ and gave the skeleton reference back — this is the leak that reached atexit");

		//---------------------------------------------------------------------
		// ReleaseAssetReferences drops the animation handles AND the borrowed
		// pointers they were pinning, as ONE invariant.
		//---------------------------------------------------------------------
		xController.Initialize(xRig.m_pxInstance);
		Flux_AnimationClip* pxBorrowed = xController.AddClipFromFile(strClipAssetPath);
		ZENITH_ASSERT_TRUE(pxBorrowed != nullptr, "the clip resolved out of the registry");
		ZENITH_ASSERT_EQ(pxClipAsset->GetRefCount(), uClipBaseline + 1u,
			"AddClipFromFile pinned the ASSET behind the clip pointer it borrowed");
		ZENITH_ASSERT_EQ(xController.GetClipCollection().GetClipCount(), 1u,
			"and the collection borrowed it");

		xController.ReleaseAssetReferences();
		ZENITH_ASSERT_EQ(pxClipAsset->GetRefCount(), uClipBaseline,
			"ReleaseAssetReferences handed the animation reference back");
		ZENITH_ASSERT_EQ(xController.GetClipCollection().GetClipCount(), 0u,
			"★ and emptied the collection WITH it — a borrowed pointer must never outlive its pin");
		ZENITH_ASSERT_EQ(pxSkeletonAsset->GetRefCount(), uSkeletonBaseline,
			"the skeleton reference went in the same call");
	}

	// The handle above is out of scope, so the asset is unreferenced; remove it by
	// name rather than through UnloadUnused, which would also sweep every other
	// zero-ref asset the boot happens to have left in the registry.
	Zenith_AssetRegistry::ForceUnload(strClipAssetPath);
}

//=============================================================================
// WU-6.3 (D43/D44) — STABLE LAYER IDS, AND THE POINTER GUARANTEE THEY REPLACE
//
// ★ WHAT THESE EXIST TO CATCH. `Flux/MeshAnimation/CLAUDE.md` and
// `EntityComponent/Components/CLAUDE.md` both used to promise that a game could
// cache a `Flux_AnimationLayer*` and keep it, on the reasoning that
// Flux_AnimationControllerStore heap-allocates the CONTROLLER so nothing inside
// it ever moves. The controller half is true and is still pinned by the
// `Animator` suite in Core/Zenith_UnitTests.Tests.inl. The layer half never
// was: BuildFromControllerDef deletes every layer and rebuilds the list from a
// def, and ReadFromDataStream deletes every layer and re-reads it — so a cached
// pointer is dangling from the first controller-asset load or scene deserialize
// onward, on a path where nothing asserts and the freed memory usually still
// looks like a layer.
//
// The replacement guarantee, which is what these four pin: an id is unique
// within a controller, monotonic for its whole lifetime, survives a rebuild
// that changes the layer ORDER, is never re-issued to a different layer, and
// resolves to nullptr rather than to a stranger once its layer is gone.
//=============================================================================

ZENITH_TEST(Animation, WU6_3_ImperativeLayerIdsAreDistinctAndMonotonic)
{
	Flux_AnimationController xController;

	// ★ IMPERATIVE AUTHORING MINTS TOO, and that is the whole of this clause.
	// The id was introduced by WU-6.2 for the DEF, and BuildFromControllerDef
	// was the only thing that ever wrote one onto a runtime layer — so every
	// controller built the way every game in the tree builds one (AddLayer, by
	// hand) had a list of layers all carrying the same value.
	Flux_AnimationLayer* pxBase = xController.AddLayer("Base");
	Flux_AnimationLayer* pxAim = xController.AddLayer("Aim");
	ZENITH_ASSERT_NOT_NULL(pxBase, "AddLayer returned the base layer");
	ZENITH_ASSERT_NOT_NULL(pxAim, "AddLayer returned the aim layer");
	if (pxBase == nullptr || pxAim == nullptr) { return; }

	const u_int uBaseId = pxBase->GetLayerId();
	const u_int uAimId = pxAim->GetLayerId();

	ZENITH_ASSERT_NE(uBaseId, uFLUX_INVALID_LAYER_ID, "an owned layer always carries a minted id");
	ZENITH_ASSERT_NE(uAimId, uFLUX_INVALID_LAYER_ID, "an owned layer always carries a minted id");
	ZENITH_ASSERT_NE(uBaseId, uAimId, "two layers of one controller must not share an id");
	ZENITH_ASSERT_GT(uAimId, uBaseId, "ids are monotonic in creation order");
	ZENITH_ASSERT_EQ(xController.GetNextLayerId(), uAimId + 1u, "the counter sits past the last id handed out");

	ZENITH_ASSERT_EQ(xController.GetLayerById(uBaseId), pxBase, "the base id resolves the base layer");
	ZENITH_ASSERT_EQ(xController.GetLayerById(uAimId), pxAim, "the aim id resolves the aim layer");
	ZENITH_ASSERT_NULL(xController.GetLayerById(uFLUX_INVALID_LAYER_ID),
		"the sentinel is not an address — an unresolved caller must get nullptr, not layer 0");
	ZENITH_ASSERT_NULL(xController.GetLayerById(uAimId + 1000u), "an id nothing carries resolves to nullptr");

	//-------------------------------------------------------------------------
	// A rebuild DESTROYS every layer, and the counter does not rewind.
	//
	// ★ THIS IS THE CLAUSE THAT MAKES A STALE ID SAFE. If the counter restarted,
	// the next layer created would inherit a number some caller is still holding
	// and GetLayerById would hand it a DIFFERENT layer — the exact failure the
	// index-based addressing had, reintroduced with extra steps.
	//-------------------------------------------------------------------------
	Flux_AnimatorControllerDef xEmptyDef;
	const bool bCleared = xController.BuildFromControllerDef(xEmptyDef, nullptr);
	ZENITH_ASSERT_TRUE(bCleared, "an empty def describes an empty controller completely");
	ZENITH_ASSERT_EQ(xController.GetLayerCount(), 0u, "the rebuild dropped every layer");
	ZENITH_ASSERT_NULL(xController.GetLayerById(uBaseId), "a destroyed layer's id resolves to nothing");

	Flux_AnimationLayer* pxFresh = xController.AddLayer("Fresh");
	ZENITH_ASSERT_NOT_NULL(pxFresh, "AddLayer returned the new layer");
	if (pxFresh == nullptr) { return; }
	ZENITH_ASSERT_NE(pxFresh->GetLayerId(), uBaseId, "a destroyed layer's id is never re-issued");
	ZENITH_ASSERT_NE(pxFresh->GetLayerId(), uAimId, "a destroyed layer's id is never re-issued");
	ZENITH_ASSERT_GT(pxFresh->GetLayerId(), uAimId, "the counter only ever moves forward");
}

ZENITH_TEST(Animation, WU6_3_LayerIdSurvivesADefRebuildThatReordersLayers)
{
	Flux_AnimationController xController;

	Flux_AnimationLayer* pxBase = xController.AddLayer("Base");
	Flux_AnimationLayer* pxAim = xController.AddLayer("Aim");
	ZENITH_ASSERT_NOT_NULL(pxBase, "AddLayer returned the base layer");
	ZENITH_ASSERT_NOT_NULL(pxAim, "AddLayer returned the aim layer");
	if (pxBase == nullptr || pxAim == nullptr) { return; }
	pxBase->CreateStateMachine("BaseSM");
	pxAim->CreateStateMachine("AimSM");

	const u_int uBaseId = pxBase->GetLayerId();
	const u_int uAimId = pxAim->GetLayerId();

	// Export re-states the RUNTIME ids rather than renumbering (through
	// Flux_AnimatorControllerDef::AssignLayerId, so the def's counter follows).
	Flux_AnimatorControllerDef xDef;
	const bool bExported = xController.ExportControllerDef(xDef);
	ZENITH_ASSERT_TRUE(bExported, "a controller with no clips and no hand-set mask exports completely");
	ZENITH_ASSERT_EQ(xDef.GetLayerCount(), 2u, "both layers reached the def");
	ZENITH_ASSERT_NOT_NULL(xDef.FindLayerById(uBaseId), "the export re-stated the base layer's id");
	ZENITH_ASSERT_NOT_NULL(xDef.FindLayerById(uAimId), "the export re-stated the aim layer's id");

	// Reorder the def: drop Base off the front and re-state it at the BACK,
	// keeping its id — what an editor drag, or an authored layer inserted below
	// an existing one, produces.
	xDef.RemoveLayer(0u);
	Flux_AnimatorControllerLayerDef* pxMovedBase = xDef.AddLayer("Base");
	ZENITH_ASSERT_NOT_NULL(pxMovedBase, "the def accepted the re-added base layer");
	if (pxMovedBase == nullptr) { return; }
	xDef.AssignLayerId(*pxMovedBase, uBaseId);

	const bool bRebuilt = xController.BuildFromControllerDef(xDef, nullptr);
	ZENITH_ASSERT_TRUE(bRebuilt, "the reordered def rebuilt completely");
	ZENITH_ASSERT_EQ(xController.GetLayerCount(), 2u, "the rebuild produced both layers");

	// ★ THE INDEX MOVED. Every pointer the AddLayer calls above returned is
	// freed by now — this is the guarantee WU-6.3 withdraws, and the reason the
	// assertions below go through the id instead of through a cached pointer.
	const Flux_AnimationLayer* pxIndexZero = xController.GetLayer(0u);
	ZENITH_ASSERT_NOT_NULL(pxIndexZero, "layer 0 exists after the rebuild");
	if (pxIndexZero == nullptr) { return; }
	ZENITH_ASSERT_STREQ(pxIndexZero->GetName().c_str(), "Aim", "the reorder put the aim layer at index 0");

	// ★ THE ID DID NOT.
	const Flux_AnimationLayer* pxBaseAfter = xController.GetLayerById(uBaseId);
	ZENITH_ASSERT_NOT_NULL(pxBaseAfter, "the base layer's id still resolves after the reorder");
	if (pxBaseAfter == nullptr) { return; }
	ZENITH_ASSERT_STREQ(pxBaseAfter->GetName().c_str(), "Base",
		"the id addressed the SAME layer, not whatever now sits at its old index");

	const Flux_AnimationLayer* pxAimAfter = xController.GetLayerById(uAimId);
	ZENITH_ASSERT_NOT_NULL(pxAimAfter, "the aim layer's id still resolves after the reorder");
	if (pxAimAfter == nullptr) { return; }
	ZENITH_ASSERT_STREQ(pxAimAfter->GetName().c_str(), "Aim", "and it is still the aim layer");

	// The rebuild adopted the def's ids rather than the ones its own AddLayer
	// minted on the way through, so the counter must sit past BOTH — otherwise
	// the next AddLayer duplicates one of them.
	ZENITH_ASSERT_GT(xController.GetNextLayerId(), uBaseId, "the counter moved past the adopted base id");
	ZENITH_ASSERT_GT(xController.GetNextLayerId(), uAimId, "the counter moved past the adopted aim id");
	Flux_AnimationLayer* pxThird = xController.AddLayer("Face");
	ZENITH_ASSERT_NOT_NULL(pxThird, "AddLayer returned the third layer");
	if (pxThird == nullptr) { return; }
	ZENITH_ASSERT_NE(pxThird->GetLayerId(), uBaseId, "a layer added after a rebuild cannot collide with an adopted id");
	ZENITH_ASSERT_NE(pxThird->GetLayerId(), uAimId, "a layer added after a rebuild cannot collide with an adopted id");
}

ZENITH_TEST(Animation, WU6_3_GetLayerByNameFindsALayerAndNullsOnAMiss)
{
	Flux_AnimationController xController;

	Flux_AnimationLayer* pxBase = xController.AddLayer("Base");
	Flux_AnimationLayer* pxAim = xController.AddLayer("Aim");
	ZENITH_ASSERT_NOT_NULL(pxBase, "AddLayer returned the base layer");
	ZENITH_ASSERT_NOT_NULL(pxAim, "AddLayer returned the aim layer");
	if (pxBase == nullptr || pxAim == nullptr) { return; }

	ZENITH_ASSERT_EQ(xController.GetLayerByName("Base"), pxBase, "the base layer is found by name");
	ZENITH_ASSERT_EQ(xController.GetLayerByName("Aim"), pxAim, "the aim layer is found by name");
	ZENITH_ASSERT_NULL(xController.GetLayerByName("NoSuchLayer"), "a name nothing carries resolves to nullptr");
	ZENITH_ASSERT_NULL(xController.GetLayerByName(""), "an empty name is a miss, not a wildcard");

	// Name -> id is how a game that has just built (or just loaded) its graph
	// gets the handle it will hold; it is deliberately a ONE-TIME lookup.
	const Flux_AnimationLayer* pxByName = xController.GetLayerByName("Aim");
	ZENITH_ASSERT_NOT_NULL(pxByName, "the aim layer is found by name");
	if (pxByName == nullptr) { return; }
	ZENITH_ASSERT_EQ(xController.GetLayerById(pxByName->GetLayerId()), pxAim,
		"the id read off a by-name lookup addresses the same layer");

	// ★ A NAME IS NOT AN IDENTITY — nothing rejects a duplicate, and the FIRST
	// in blend order wins. This is exactly why the id exists, and pinning the
	// tie-break stops a caller reading the ambiguity as a bug in the lookup.
	Flux_AnimationLayer* pxSecondAim = xController.AddLayer("Aim");
	ZENITH_ASSERT_NOT_NULL(pxSecondAim, "AddLayer returned the duplicate-named layer");
	if (pxSecondAim == nullptr) { return; }
	ZENITH_ASSERT_EQ(xController.GetLayerByName("Aim"), pxAim, "the FIRST match in blend order wins");
	ZENITH_ASSERT_NE(pxSecondAim->GetLayerId(), pxAim->GetLayerId(), "...and the two are still told apart by id");
}

ZENITH_TEST(Animation, WU6_3_LayersRestoredFromAStreamAreAddressableById)
{
	// ★ THE ID IS NOT IN THE SCENE BYTES, AND MAY NOT BE — Flux_AnimationLayer's
	// payload is written INLINE into a .zscen and committed scene files carry it
	// with no version word. So the deserializing controller MINTS: the numbers
	// differ from the save's, and what must hold is that they are present,
	// distinct and resolvable. A game therefore re-reads its ids after a load
	// (by name) and never persists one.
	Flux_AnimationController xSource;
	Flux_AnimationLayer* pxSourceBase = xSource.AddLayer("Base");
	Flux_AnimationLayer* pxSourceAim = xSource.AddLayer("Aim");
	ZENITH_ASSERT_NOT_NULL(pxSourceBase, "AddLayer returned the base layer");
	ZENITH_ASSERT_NOT_NULL(pxSourceAim, "AddLayer returned the aim layer");
	if (pxSourceBase == nullptr || pxSourceAim == nullptr) { return; }
	pxSourceAim->SetWeight(0.25f);

	Zenith_DataStream xStream(1);
	xSource.WriteToDataStream(xStream);

	xStream.SetCursor(0);
	Flux_AnimationController xLoaded;
	xLoaded.ReadFromDataStream(xStream);

	ZENITH_ASSERT_EQ(xLoaded.GetLayerCount(), 2u, "both layers came back");

	const Flux_AnimationLayer* pxLoadedBase = xLoaded.GetLayerByName("Base");
	const Flux_AnimationLayer* pxLoadedAim = xLoaded.GetLayerByName("Aim");
	ZENITH_ASSERT_NOT_NULL(pxLoadedBase, "the base layer came back");
	ZENITH_ASSERT_NOT_NULL(pxLoadedAim, "the aim layer came back");
	if (pxLoadedBase == nullptr || pxLoadedAim == nullptr) { return; }

	const u_int uLoadedBaseId = pxLoadedBase->GetLayerId();
	const u_int uLoadedAimId = pxLoadedAim->GetLayerId();
	ZENITH_ASSERT_NE(uLoadedBaseId, uFLUX_INVALID_LAYER_ID, "a restored layer is minted an id, not left unowned");
	ZENITH_ASSERT_NE(uLoadedAimId, uFLUX_INVALID_LAYER_ID, "a restored layer is minted an id, not left unowned");
	ZENITH_ASSERT_NE(uLoadedBaseId, uLoadedAimId, "two restored layers must not share an id");
	ZENITH_ASSERT_EQ(xLoaded.GetLayerById(uLoadedAimId), pxLoadedAim, "the restored aim id addresses the aim layer");
	ZENITH_ASSERT_EQ_FLOAT(pxLoadedAim->GetWeight(), 0.25f, 0.001f,
		"and it is the layer that carried the weight, not merely one with the right name");
}

//=============================================================================
// WU-6.4 (D45) — HOT RELOAD AT CONTROLLER LEVEL
//
// ★ WHAT THESE EXIST TO CATCH. BuildFromControllerDef is a LOAD, and a load is
// a demolition: every layer deleted and rebuilt at the file's weight, the
// top-level machine replaced, every graph's playhead at zero. Running it against
// an edited .zanimctrl while the game is playing therefore snaps the character
// to its default state and throws away every live parameter value — which is
// exactly the behaviour a cold load is supposed to have, so nothing anywhere
// asserts and no gate could see it.
//
// ReloadFromControllerDef is the same rebuild with the playback carried across.
// The two rows the CONTROLLER owns (the machine-level rows live in
// Flux_AnimationStateMachine.Tests.inl) are the parameter VALUES — one live set
// per controller since D42 — and the layer WEIGHTS, keyed by the stable layer id
// WU-6.3 introduced. Each appears here in both directions.
//
// ★ EVERY Flux_AnimationLayer* TAKEN BEFORE A RELOAD IS DANGLING AFTER IT
// (D43/D44), exactly as it is after a build. Every assertion below therefore
// re-resolves through GetLayerById — the ids are read off the layers BEFORE the
// reload and the pointers are never touched again.
//=============================================================================

namespace
{
	// A one-layer controller whose machine declares szParamName as a float. The
	// smallest thing a .zanimctrl can round-trip that still carries a live value.
	Flux_AnimationLayer* WU64_AddFloatParamLayer(Flux_AnimationController& xController,
		const char* szLayerName, const char* szParamName)
	{
		Flux_AnimationLayer* pxLayer = xController.AddLayer(szLayerName);
		Flux_AnimationStateMachine* pxSM = pxLayer->CreateStateMachine("SM");
		pxSM->GetDef().GetParameterDeclarations().AddFloat(szParamName, 0.0f);
		pxSM->AddState("Idle");
		pxSM->SetDefaultState("Idle");
		return pxLayer;
	}
}

//=============================================================================
// (9) A parameter matched on name AND type keeps the value gameplay put in it.
//=============================================================================
ZENITH_TEST(Animation, WU64_ReloadKeepsAParameterMatchedByNameAndType)
{
	Flux_AnimationController xController;
	WU64_AddFloatParamLayer(xController, "Base", "Speed");

	// Set through the CONTROLLER, which is the shape D42 exists for: a layered
	// controller has a null m_pxStateMachine and one shared live set.
	xController.SetFloat("Speed", 4.25f);
	ZENITH_ASSERT_EQ_FLOAT(xController.GetFloat("Speed"), 4.25f, 1e-5f, "gameplay is running on 4.25");

	Flux_AnimatorControllerDef xDef;
	ZENITH_ASSERT_TRUE(xController.ExportControllerDef(xDef),
		"a controller with no clips and no hand-set mask exports completely");

	Flux_AnimatorControllerLayerDef* pxLayerDef = xDef.GetLayer(0u);
	ZENITH_ASSERT_NOT_NULL(pxLayerDef, "the layer reached the def");
	if (pxLayerDef == nullptr) { return; }

	// ★ THE FILE CARRIES THE DECLARATION, NOT THE LIVE VALUE (D42), which is what
	// makes this test able to tell "restored" from "re-seeded": the def says 0.
	ZENITH_ASSERT_EQ_FLOAT(pxLayerDef->GetStateMachineDef().GetParameterDeclarations().GetFloat("Speed"),
		0.0f, 1e-5f, "the exported DECLARATION carries the authored default, not the running value");

	ZENITH_ASSERT_TRUE(xController.ReloadFromControllerDef(xDef, nullptr),
		"the def names no clip and no mask, so the reload is complete");

	ZENITH_ASSERT_EQ_FLOAT(xController.GetFloat("Speed"), 4.25f, 1e-5f,
		"★ the live value survived a whole-controller rebuild — the def's 0.0 did not overwrite it");
}

//=============================================================================
// (9b) THE FIRST NEGATIVE. A RENAMED parameter gets the new default, and the
//      old name does not linger in the live set.
//=============================================================================
ZENITH_TEST(Animation, WU64_ReloadGivesARenamedParameterTheNewDefault)
{
	Flux_AnimationController xController;
	WU64_AddFloatParamLayer(xController, "Base", "Speed");

	xController.SetFloat("Speed", 4.25f);

	Flux_AnimatorControllerDef xDef;
	ZENITH_ASSERT_TRUE(xController.ExportControllerDef(xDef), "exported");
	Flux_AnimatorControllerLayerDef* pxLayerDef = xDef.GetLayer(0u);
	ZENITH_ASSERT_NOT_NULL(pxLayerDef, "the layer reached the def");
	if (pxLayerDef == nullptr) { return; }

	// The edit: "Speed" is renamed to "Velocity", with a different default.
	Flux_AnimationParameters& xDecls = pxLayerDef->GetStateMachineDef().GetParameterDeclarations();
	xDecls.RemoveParameter("Speed");
	xDecls.AddFloat("Velocity", 9.5f);

	ZENITH_ASSERT_TRUE(xController.ReloadFromControllerDef(xDef, nullptr), "the reload is complete");

	// ★ THIS IS THE ONE A RE-SEED CANNOT DO. SeedInto never overwrites (D42), so
	// seeding the new declarations on top of the old live set would leave "Speed"
	// in it at 4.25 forever — under a name no condition reads and nothing removes.
	ZENITH_ASSERT_FALSE(xController.GetParameters().HasParameter("Speed"),
		"the old name is gone from the live set, not merely shadowed");
	ZENITH_ASSERT_TRUE(xController.GetParameters().HasParameter("Velocity"), "and the new one is declared");
	ZENITH_ASSERT_EQ_FLOAT(xController.GetFloat("Velocity"), 9.5f, 1e-5f,
		"a rename is an EDIT: the new parameter starts at its authored default, not at 4.25");
}

//=============================================================================
// (9c) THE SECOND NEGATIVE. A RETYPED parameter (float -> int) gets the new
//      default. The value lives in a UNION, so a name-only match is not merely
//      wrong here — it is unrelated.
//=============================================================================
ZENITH_TEST(Animation, WU64_ReloadGivesARetypedParameterTheNewDefault)
{
	Flux_AnimationController xController;
	WU64_AddFloatParamLayer(xController, "Base", "Speed");

	xController.SetFloat("Speed", 4.25f);

	Flux_AnimatorControllerDef xDef;
	ZENITH_ASSERT_TRUE(xController.ExportControllerDef(xDef), "exported");
	Flux_AnimatorControllerLayerDef* pxLayerDef = xDef.GetLayer(0u);
	ZENITH_ASSERT_NOT_NULL(pxLayerDef, "the layer reached the def");
	if (pxLayerDef == nullptr) { return; }

	Flux_AnimationParameters& xDecls = pxLayerDef->GetStateMachineDef().GetParameterDeclarations();
	xDecls.RemoveParameter("Speed");
	xDecls.AddInt("Speed", 7);

	ZENITH_ASSERT_TRUE(xController.ReloadFromControllerDef(xDef, nullptr), "the reload is complete");

	ZENITH_ASSERT_TRUE(xController.GetParameters().HasParameter("Speed"), "the name is still declared");
	ZENITH_ASSERT_TRUE(xController.GetParameters().GetParameterType("Speed") == Flux_AnimationParameters::ParamType::Int,
		"as the type the edit gave it");
	ZENITH_ASSERT_EQ(xController.GetInt("Speed"), 7,
		"★ and holding the new DEFAULT. Matching on name alone would have written 4.25f's bit pattern "
		"into the int arm of the union and produced 1082130432");
	ZENITH_ASSERT_EQ_FLOAT(xController.GetFloat("Speed"), 0.0f, 1e-5f,
		"a float read of an int parameter is the type's default, which is how the getters already behave");
}

//=============================================================================
// (10) A layer weight survives matched by ID, across a reorder that moves every
//      index. This is what the stable id was introduced for (WU-6.3 / D43).
//=============================================================================
ZENITH_TEST(Animation, WU64_ReloadKeepsLayerWeightsByIdAcrossAReorder)
{
	Flux_AnimationController xController;
	Flux_AnimationLayer* pxBase = xController.AddLayer("Base");
	Flux_AnimationLayer* pxAim = xController.AddLayer("Aim");
	ZENITH_ASSERT_NOT_NULL(pxBase, "AddLayer returned the base layer");
	ZENITH_ASSERT_NOT_NULL(pxAim, "AddLayer returned the aim layer");
	if (pxBase == nullptr || pxAim == nullptr) { return; }
	pxBase->CreateStateMachine("BaseSM");
	pxAim->CreateStateMachine("AimSM");

	const u_int uBaseId = pxBase->GetLayerId();
	const u_int uAimId = pxAim->GetLayerId();

	Flux_AnimatorControllerDef xDef;
	ZENITH_ASSERT_TRUE(xController.ExportControllerDef(xDef), "exported");
	ZENITH_ASSERT_EQ(xDef.GetLayerCount(), 2u, "both layers reached the def");

	// ★ THE LIVE WEIGHTS ARE SET *AFTER* THE EXPORT, so the def says 1.0 for both
	// and the only way these numbers can come back is by being restored. A fade
	// in progress is precisely the state an author must not destroy by saving.
	pxBase->SetWeight(0.8f);
	pxAim->SetWeight(0.3f);

	// The edit reorders: Base is dropped off the front and re-stated at the back,
	// keeping its id — an editor drag, or a layer inserted below an existing one.
	xDef.RemoveLayer(0u);
	Flux_AnimatorControllerLayerDef* pxMovedBase = xDef.AddLayer("Base");
	ZENITH_ASSERT_NOT_NULL(pxMovedBase, "the def accepted the re-added base layer");
	if (pxMovedBase == nullptr) { return; }
	xDef.AssignLayerId(*pxMovedBase, uBaseId);

	ZENITH_ASSERT_TRUE(xController.ReloadFromControllerDef(xDef, nullptr), "the reordered def reloaded completely");
	ZENITH_ASSERT_EQ(xController.GetLayerCount(), 2u, "both layers came back");

	// pxBase / pxAim are freed by now — this is the D44 rule, and it applies to a
	// reload exactly as it applies to a build.
	const Flux_AnimationLayer* pxIndexZero = xController.GetLayer(0u);
	ZENITH_ASSERT_NOT_NULL(pxIndexZero, "layer 0 exists");
	if (pxIndexZero == nullptr) { return; }
	ZENITH_ASSERT_STREQ(pxIndexZero->GetName().c_str(), "Aim",
		"the reorder really did move the indices — otherwise 'by id' proves nothing here");

	const Flux_AnimationLayer* pxAimAfter = xController.GetLayerById(uAimId);
	ZENITH_ASSERT_NOT_NULL(pxAimAfter, "the aim layer's id still resolves");
	if (pxAimAfter == nullptr) { return; }
	ZENITH_ASSERT_EQ_FLOAT(pxAimAfter->GetWeight(), 0.3f, 1e-5f,
		"★ the live weight followed the ID. The def says 1.0, so a build would have snapped the fade shut");

	const Flux_AnimationLayer* pxBaseAfter = xController.GetLayerById(uBaseId);
	ZENITH_ASSERT_NOT_NULL(pxBaseAfter, "the base layer's id still resolves");
	if (pxBaseAfter == nullptr) { return; }
	ZENITH_ASSERT_STREQ(pxBaseAfter->GetName().c_str(), "Base", "and addresses the layer it named before");
	ZENITH_ASSERT_EQ_FLOAT(pxBaseAfter->GetWeight(), 0.8f, 1e-5f,
		"...even though that layer moved from index 0 to index 1");
}

//=============================================================================
// (10b) THE NEGATIVE, both halves of it. A layer the edit DELETED is gone and
//       its snapshot is dropped; a layer the edit ADDED carries the def's
//       weight, because no snapshot names its id.
//=============================================================================
ZENITH_TEST(Animation, WU64_ReloadDropsARemovedLayerAndGivesANewOneTheDefsWeight)
{
	Flux_AnimationController xController;
	Flux_AnimationLayer* pxBase = xController.AddLayer("Base");
	Flux_AnimationLayer* pxAim = xController.AddLayer("Aim");
	ZENITH_ASSERT_NOT_NULL(pxBase, "AddLayer returned the base layer");
	ZENITH_ASSERT_NOT_NULL(pxAim, "AddLayer returned the aim layer");
	if (pxBase == nullptr || pxAim == nullptr) { return; }
	pxBase->CreateStateMachine("BaseSM");
	pxAim->CreateStateMachine("AimSM");

	const u_int uBaseId = pxBase->GetLayerId();
	const u_int uAimId = pxAim->GetLayerId();

	Flux_AnimatorControllerDef xDef;
	ZENITH_ASSERT_TRUE(xController.ExportControllerDef(xDef), "exported");

	pxBase->SetWeight(0.8f);
	pxAim->SetWeight(0.3f);

	// The edit deletes Aim (def index 1) and adds Face at a weight of its own.
	xDef.RemoveLayer(1u);
	Flux_AnimatorControllerLayerDef* pxFaceDef = xDef.AddLayer("Face");
	ZENITH_ASSERT_NOT_NULL(pxFaceDef, "the def accepted the new layer");
	if (pxFaceDef == nullptr) { return; }
	pxFaceDef->SetWeight(0.6f);
	const u_int uFaceId = pxFaceDef->GetLayerId();
	ZENITH_ASSERT_NE(uFaceId, uAimId,
		"the def's counter never re-issues a removed layer's id, which is what makes the drop below unambiguous");

	ZENITH_ASSERT_TRUE(xController.ReloadFromControllerDef(xDef, nullptr), "the reload is complete");
	ZENITH_ASSERT_EQ(xController.GetLayerCount(), 2u, "Base and Face — Aim is gone");

	ZENITH_ASSERT_NULL(xController.GetLayerById(uAimId),
		"★ a layer the edit deleted is GONE, and its snapshot is dropped rather than applied to a stranger");

	const Flux_AnimationLayer* pxFaceAfter = xController.GetLayerById(uFaceId);
	ZENITH_ASSERT_NOT_NULL(pxFaceAfter, "the new layer resolves by the id the def minted");
	if (pxFaceAfter == nullptr) { return; }
	ZENITH_ASSERT_STREQ(pxFaceAfter->GetName().c_str(), "Face", "and it is the new layer");
	ZENITH_ASSERT_EQ_FLOAT(pxFaceAfter->GetWeight(), 0.6f, 1e-5f,
		"★ a NEW id carries the DEF's weight — no snapshot names it, so nothing restores over it");

	const Flux_AnimationLayer* pxBaseAfter = xController.GetLayerById(uBaseId);
	ZENITH_ASSERT_NOT_NULL(pxBaseAfter, "the surviving layer still resolves");
	if (pxBaseAfter == nullptr) { return; }
	ZENITH_ASSERT_EQ_FLOAT(pxBaseAfter->GetWeight(), 0.8f, 1e-5f,
		"and it kept its live weight while its neighbour was deleted out from under it");
}
