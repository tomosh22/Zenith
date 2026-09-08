#include "UnitTests/Zenith_UnitTests.h"
#include "Flux/MeshAnimation/Flux_BlendTree.h"

// WU-7.3's refusal case feeds SetBlendPointPosition a NaN on purpose: the guard
// is written as a POSITIVE range test precisely so a NaN falls out of it, and a
// test that never handed it one would not know.
#include <limits>

// ============================================================================
// Flux_BlendTreeNode_BlendSpace1D / 2D nearest-blend-point tests
//
// Both classes share an internal templated helper (FindNearestBlendPoint) for
// selecting the closest blend point to the current parameter value. These
// tests pin the public-API behavior (GetNormalizedTime → nearest point's
// child node's normalized time) for a few canonical configurations.
// ============================================================================

namespace
{
	// Mock blend-tree node that returns a caller-supplied normalized time.
	// Used so tests can verify which point was selected by the nearest-finder
	// without standing up a real animation clip + skeleton.
	class MockBlendNodeWithTime : public Flux_BlendTreeNode
	{
	public:
		explicit MockBlendNodeWithTime(float fNormalizedTime) : m_fNormalizedTime(fNormalizedTime) {}
		void Evaluate(float, Flux_SkeletonPose&, const Zenith_SkeletonAsset&) override {}
		float GetNormalizedTime() const override { return m_fNormalizedTime; }
		void Reset() override {}
		const char* GetNodeTypeName() const override { return "MockBlendNodeWithTime"; }
		void WriteToDataStream(Zenith_DataStream&) const override {}
		void ReadFromDataStream(Zenith_DataStream&) override {}
		float m_fNormalizedTime;
	};

	bool BlendSpaceFloatEquals(float a, float b, float fTol = 1e-5f) { return std::abs(a - b) < fTol; }

	// WU-5A: the smallest clip a leaf can advance through — a duration and a
	// looping flag are all Flux_BlendTreeNode_Clip reads to build a span.
	void WU5A_InitSpanClip(Flux_AnimationClip& xClip, const char* szName, float fDurationSeconds, bool bLooping)
	{
		xClip.SetName(szName);
		xClip.SetDuration(fDurationSeconds);
		xClip.SetLooping(bLooping);
	}
}

ZENITH_TEST(Animation, BlendSpace1DEmptyReturnsZero) { Zenith_UnitTests::TestBlendSpace1DEmptyReturnsZero(); }
void Zenith_UnitTests::TestBlendSpace1DEmptyReturnsZero()
{
	Flux_BlendTreeNode_BlendSpace1D xBS;
	xBS.SetParameter(0.5f);
	// No blend points added — must return 0.0f without indexing into nullptr.
	ZENITH_ASSERT_TRUE(BlendSpaceFloatEquals(xBS.GetNormalizedTime(), 0.0f),
		"Empty BlendSpace1D must return 0.0 normalized time");
}

ZENITH_TEST(Animation, BlendSpace1DSingleBlendPoint) { Zenith_UnitTests::TestBlendSpace1DSingleBlendPoint(); }
void Zenith_UnitTests::TestBlendSpace1DSingleBlendPoint()
{
	Flux_BlendTreeNode_BlendSpace1D xBS;
	xBS.AddBlendPoint(new MockBlendNodeWithTime(0.42f), 1.0f);
	xBS.SetParameter(99.0f); // far from the only point's position
	// Single point — always nearest, returns its normalized time regardless
	// of how far the parameter is.
	ZENITH_ASSERT_TRUE(BlendSpaceFloatEquals(xBS.GetNormalizedTime(), 0.42f),
		"BlendSpace1D with single point must return that point's normalized time");
}

ZENITH_TEST(Animation, BlendSpace1DSelectsNearestPoint) { Zenith_UnitTests::TestBlendSpace1DSelectsNearestPoint(); }
void Zenith_UnitTests::TestBlendSpace1DSelectsNearestPoint()
{
	Flux_BlendTreeNode_BlendSpace1D xBS;
	xBS.AddBlendPoint(new MockBlendNodeWithTime(0.10f), 0.0f);
	xBS.AddBlendPoint(new MockBlendNodeWithTime(0.50f), 0.5f);
	xBS.AddBlendPoint(new MockBlendNodeWithTime(0.90f), 1.0f);

	xBS.SetParameter(0.05f);
	ZENITH_ASSERT_TRUE(BlendSpaceFloatEquals(xBS.GetNormalizedTime(), 0.10f),
		"Parameter close to 0.0 must select the 0.0 point's normalized time");

	xBS.SetParameter(0.45f);
	ZENITH_ASSERT_TRUE(BlendSpaceFloatEquals(xBS.GetNormalizedTime(), 0.50f),
		"Parameter close to 0.5 must select the 0.5 point's normalized time");

	xBS.SetParameter(0.95f);
	ZENITH_ASSERT_TRUE(BlendSpaceFloatEquals(xBS.GetNormalizedTime(), 0.90f),
		"Parameter close to 1.0 must select the 1.0 point's normalized time");
}

ZENITH_TEST(Animation, BlendSpace1DSelectsNearestUnsorted) { Zenith_UnitTests::TestBlendSpace1DSelectsNearestUnsorted(); }
void Zenith_UnitTests::TestBlendSpace1DSelectsNearestUnsorted()
{
	// Ensures nearest-finder doesn't depend on insertion / sorted order.
	Flux_BlendTreeNode_BlendSpace1D xBS;
	xBS.AddBlendPoint(new MockBlendNodeWithTime(0.50f), 0.5f); // middle first
	xBS.AddBlendPoint(new MockBlendNodeWithTime(0.10f), 0.0f);
	xBS.AddBlendPoint(new MockBlendNodeWithTime(0.90f), 1.0f);
	xBS.SetParameter(0.05f);
	ZENITH_ASSERT_TRUE(BlendSpaceFloatEquals(xBS.GetNormalizedTime(), 0.10f),
		"Nearest-finder must work regardless of insertion order");
}

ZENITH_TEST(Animation, BlendSpace2DEmptyReturnsZero) { Zenith_UnitTests::TestBlendSpace2DEmptyReturnsZero(); }
void Zenith_UnitTests::TestBlendSpace2DEmptyReturnsZero()
{
	Flux_BlendTreeNode_BlendSpace2D xBS;
	xBS.SetParameter(Zenith_Maths::Vector2(0.5f, 0.5f));
	ZENITH_ASSERT_TRUE(BlendSpaceFloatEquals(xBS.GetNormalizedTime(), 0.0f),
		"Empty BlendSpace2D must return 0.0 normalized time");
}

ZENITH_TEST(Animation, BlendSpace2DSingleBlendPoint) { Zenith_UnitTests::TestBlendSpace2DSingleBlendPoint(); }
void Zenith_UnitTests::TestBlendSpace2DSingleBlendPoint()
{
	Flux_BlendTreeNode_BlendSpace2D xBS;
	xBS.AddBlendPoint(new MockBlendNodeWithTime(0.33f), Zenith_Maths::Vector2(2.0f, 3.0f));
	xBS.SetParameter(Zenith_Maths::Vector2(99.0f, 99.0f));
	ZENITH_ASSERT_TRUE(BlendSpaceFloatEquals(xBS.GetNormalizedTime(), 0.33f),
		"BlendSpace2D with single point must return that point's normalized time");
}

ZENITH_TEST(Animation, BlendSpace2DSelectsNearestByEuclideanDistance) { Zenith_UnitTests::TestBlendSpace2DSelectsNearestByEuclideanDistance(); }
void Zenith_UnitTests::TestBlendSpace2DSelectsNearestByEuclideanDistance()
{
	Flux_BlendTreeNode_BlendSpace2D xBS;
	xBS.AddBlendPoint(new MockBlendNodeWithTime(0.10f), Zenith_Maths::Vector2(0.0f, 0.0f));
	xBS.AddBlendPoint(new MockBlendNodeWithTime(0.50f), Zenith_Maths::Vector2(10.0f, 0.0f));
	xBS.AddBlendPoint(new MockBlendNodeWithTime(0.90f), Zenith_Maths::Vector2(0.0f, 10.0f));

	// Parameter (1, 1): closest to (0, 0) by Euclidean distance.
	xBS.SetParameter(Zenith_Maths::Vector2(1.0f, 1.0f));
	ZENITH_ASSERT_TRUE(BlendSpaceFloatEquals(xBS.GetNormalizedTime(), 0.10f),
		"Parameter (1,1) must select (0,0) point — closest by Euclidean distance");

	// Parameter (9, 1): closest to (10, 0).
	xBS.SetParameter(Zenith_Maths::Vector2(9.0f, 1.0f));
	ZENITH_ASSERT_TRUE(BlendSpaceFloatEquals(xBS.GetNormalizedTime(), 0.50f),
		"Parameter (9,1) must select (10,0) point — closest by Euclidean distance");

	// Parameter (1, 9): closest to (0, 10).
	xBS.SetParameter(Zenith_Maths::Vector2(1.0f, 9.0f));
	ZENITH_ASSERT_TRUE(BlendSpaceFloatEquals(xBS.GetNormalizedTime(), 0.90f),
		"Parameter (1,9) must select (0,10) point — closest by Euclidean distance");
}

// ============================================================================
// WU-5A — per-leaf event spans (D34/D35/D38)
//
// These pin the LEAF side of event delivery: what a clip node records about the
// step it just took, and how a composite splits its own contribution between
// children. Who is allowed to FIRE those spans is the controller's, and is
// pinned in Flux_AnimationController.Tests.inl.
// ============================================================================

ZENITH_TEST(Animation, ClipLeafReportsItsCrossingSpan)
{
	Flux_AnimationClip xClip;
	WU5A_InitSpanClip(xClip, "Span", 2.0f, true);

	Flux_BlendTreeNode_Clip xNode(&xClip, 1.0f);
	Flux_SkeletonPose xPose;
	Zenith_SkeletonAsset xSkeleton;

	xNode.SetEvalWeight(1.0f);
	xNode.Evaluate(0.5f, xPose, xSkeleton);

	Zenith_Vector<Flux_ClipEventSpan> xSpans;
	xNode.CollectEventSpans(&xSpans);

	ZENITH_ASSERT_EQ(xSpans.GetSize(), 1u, "one evaluated leaf reports one span");
	if (xSpans.GetSize() == 1)
	{
		ZENITH_ASSERT_TRUE(xSpans.Get(0).m_pxClip == &xClip, "and it names its own clip");
		ZENITH_ASSERT_EQ_FLOAT(xSpans.Get(0).m_fPrevNormalizedTime, 0.0f, 1e-5f, "the span starts where the playhead was");
		ZENITH_ASSERT_EQ_FLOAT(xSpans.Get(0).m_fCurrNormalizedTime, 0.25f, 1e-5f, "0.5s of a 2s clip is normalized 0.25");
		ZENITH_ASSERT_EQ_FLOAT(xSpans.Get(0).m_fWeight, 1.0f, 1e-5f, "a root leaf carries the whole weight");
		ZENITH_ASSERT_TRUE(xSpans.Get(0).m_bForward, "a positive step is forward");
		ZENITH_ASSERT_FALSE(xSpans.Get(0).m_bWrapped, "a quarter of the clip does not reach the loop point");
	}
	ZENITH_ASSERT_EQ_FLOAT(xNode.GetPreviousTimestamp(), 0.0f, 1e-5f, "the leaf's own mark is where the span started");

	// ★ COLLECTING IS WHAT CLEARS IT. A span handed out twice is an event fired
	// twice, and then every frame after that.
	xSpans.Clear();
	xNode.CollectEventSpans(&xSpans);
	ZENITH_ASSERT_EQ(xSpans.GetSize(), 0u, "a collected span is not handed out a second time");
}

ZENITH_TEST(Animation, ClipLeafWrapIsReadFromTheRawAdvancedTime)
{
	// ★ THE HAZARD THIS PINS: a step LONGER than the clip wraps and still lands
	// ABOVE where it started, so `curr < prev` reads it as no wrap and the whole
	// loop's worth of events is dropped. The flag comes off prev + step >= duration.
	Flux_AnimationClip xClip;
	WU5A_InitSpanClip(xClip, "Wrap", 1.0f, true);

	Flux_BlendTreeNode_Clip xNode(&xClip, 1.0f);
	Flux_SkeletonPose xPose;
	Zenith_SkeletonAsset xSkeleton;

	xNode.SetEvalWeight(1.0f);
	xNode.Evaluate(1.5f, xPose, xSkeleton);

	Zenith_Vector<Flux_ClipEventSpan> xSpans;
	xNode.CollectEventSpans(&xSpans);

	ZENITH_ASSERT_EQ(xSpans.GetSize(), 1u, "one span");
	if (xSpans.GetSize() == 1)
	{
		ZENITH_ASSERT_EQ_FLOAT(xSpans.Get(0).m_fCurrNormalizedTime, 0.5f, 1e-5f, "1.5s of a 1s looping clip lands at 0.5");
		ZENITH_ASSERT_GT(xSpans.Get(0).m_fCurrNormalizedTime, xSpans.Get(0).m_fPrevNormalizedTime,
			"and it lands ABOVE where it started, which is exactly why curr < prev cannot be the test");
		ZENITH_ASSERT_TRUE(xSpans.Get(0).m_bWrapped, "the step crossed the loop point and says so");
	}
}

ZENITH_TEST(Animation, BlendNodeSplitsWeightBetweenItsChildren)
{
	Flux_AnimationClip xClipA;
	Flux_AnimationClip xClipB;
	WU5A_InitSpanClip(xClipA, "A", 1.0f, true);
	WU5A_InitSpanClip(xClipB, "B", 1.0f, true);

	Flux_SkeletonPose xPose;
	Zenith_SkeletonAsset xSkeleton;
	Zenith_Vector<Flux_ClipEventSpan> xSpans;

	{
		Flux_BlendTreeNode_Blend xBlend(new Flux_BlendTreeNode_Clip(&xClipA),
			new Flux_BlendTreeNode_Clip(&xClipB), 0.25f);
		xBlend.SetEvalWeight(1.0f);
		xBlend.Evaluate(0.1f, xPose, xSkeleton);
		xBlend.CollectEventSpans(&xSpans);

		ZENITH_ASSERT_EQ(xSpans.GetSize(), 2u, "a Blend evaluates both children, so both report");
		if (xSpans.GetSize() == 2)
		{
			ZENITH_ASSERT_EQ_FLOAT(xSpans.Get(0).m_fWeight, 0.75f, 1e-5f, "child A carries 1 - blend weight");
			ZENITH_ASSERT_EQ_FLOAT(xSpans.Get(1).m_fWeight, 0.25f, 1e-5f, "child B carries the blend weight");
			ZENITH_ASSERT_TRUE(xSpans.Get(0).m_pxClip == &xClipA, "and A is reported FIRST — collection order is leaf index");
		}
	}
}

ZENITH_TEST(Animation, SelectNodeReportsOnlyTheBranchItEvaluated)
{
	Flux_AnimationClip xClipA;
	Flux_AnimationClip xClipB;
	WU5A_InitSpanClip(xClipA, "A", 1.0f, true);
	WU5A_InitSpanClip(xClipB, "B", 1.0f, true);

	Flux_SkeletonPose xPose;
	Zenith_SkeletonAsset xSkeleton;
	Zenith_Vector<Flux_ClipEventSpan> xSpans;

	{
		Flux_BlendTreeNode_Select xSelect;
		xSelect.AddChild(new Flux_BlendTreeNode_Clip(&xClipA));
		xSelect.AddChild(new Flux_BlendTreeNode_Clip(&xClipB));

		xSelect.SetEvalWeight(1.0f);
		xSelect.Evaluate(0.25f, xPose, xSkeleton);
		xSelect.CollectEventSpans(&xSpans);
		ZENITH_ASSERT_EQ(xSpans.GetSize(), 1u, "only the selected branch was evaluated, so only it reports");
		if (xSpans.GetSize() == 1)
		{
			ZENITH_ASSERT_TRUE(xSpans.Get(0).m_pxClip == &xClipA, "and it is branch 0");
		}

		// ★ THE BRANCH THAT STOPS BEING EVALUATED MUST STOP REPORTING. It still
		// holds the timestamps from its last evaluate; if collection did not walk
		// (and clear) it, it would hand out that same span once per frame forever.
		xSelect.SetSelectedIndex(1);
		for (u_int u = 0; u < 2; ++u)
		{
			xSpans.Clear();
			xSelect.Evaluate(0.25f, xPose, xSkeleton);
			xSelect.CollectEventSpans(&xSpans);
			ZENITH_ASSERT_EQ(xSpans.GetSize(), 1u, "exactly one branch reports after the switch");
			if (xSpans.GetSize() == 1)
			{
				ZENITH_ASSERT_TRUE(xSpans.Get(0).m_pxClip == &xClipB, "and it is the newly selected one");
			}
		}
	}
}

// ============================================================================
// WU-7.3: the editing accessors.
//
// Both cases pin the thing the accessor does BESIDES writing the field, because
// that is the half an editor cannot see and the runtime silently depends on: the
// 1D list must stay sorted (Evaluate scans for a bracketing pair) and the 2D
// triangulation must be re-derived (FindContainingTriangle reads it).
// ============================================================================

ZENITH_TEST(Animation, BlendSpace1DAPositionEditReSortsAndReportsWhereThePointWent)
{
	Flux_BlendTreeNode_BlendSpace1D xBS;
	xBS.AddBlendPoint(new MockBlendNodeWithTime(0.10f), 0.0f);
	xBS.AddBlendPoint(new MockBlendNodeWithTime(0.90f), 1.0f);
	xBS.SortBlendPoints();

	ZENITH_ASSERT_EQ(xBS.GetBlendPointCount(), 2u, "two points");
	ZENITH_ASSERT_NOT_NULL(xBS.GetBlendPointNode(0), "point 0 carries its child");
	ZENITH_ASSERT_NULL(xBS.GetBlendPointNode(2), "and an index past the end answers null rather than reading past it");

	float fPosition = -1.0f;
	ZENITH_ASSERT_TRUE(xBS.GetBlendPointPosition(1, fPosition), "point 1 has a position");
	ZENITH_ASSERT_EQ_FLOAT(fPosition, 1.0f, 1e-6f, "which is where it was added");
	ZENITH_ASSERT_FALSE(xBS.GetBlendPointPosition(2, fPosition), "and an index past the end refuses");

	// ★ MOVING POINT 1 BELOW POINT 0 RENUMBERS BOTH. Evaluate brackets the
	// parameter between two ADJACENT entries, so a list left unsorted would blend
	// the wrong pair — and a caller holding index 1 would now be holding the other
	// point.
	Flux_BlendTreeNode* pxMoved = xBS.GetBlendPointNode(1);
	u_int uNewIndex = 0xFFFFFFFFu;
	ZENITH_ASSERT_TRUE(xBS.SetBlendPointPosition(1, -2.0f, &uNewIndex), "the move is accepted");
	ZENITH_ASSERT_EQ(uNewIndex, 0u, "★ and it reports that the point is now index 0");
	ZENITH_ASSERT_TRUE(xBS.GetBlendPointNode(0) == pxMoved, "which is where the child actually is");

	float fFirst = 0.0f;
	float fSecond = 0.0f;
	ZENITH_ASSERT_TRUE(xBS.GetBlendPointPosition(0, fFirst), "point 0 reads");
	ZENITH_ASSERT_TRUE(xBS.GetBlendPointPosition(1, fSecond), "point 1 reads");
	ZENITH_ASSERT_TRUE(fFirst <= fSecond, "★ and the list is still ASCENDING, which is what Evaluate assumes");

	// A refusal changes nothing at all — no clamp, no partial write.
	ZENITH_ASSERT_FALSE(xBS.SetBlendPointPosition(7, 0.5f), "an index past the end is refused");
	ZENITH_ASSERT_FALSE(xBS.SetBlendPointPosition(0, std::numeric_limits<float>::quiet_NaN()),
		"and so is a non-finite position");
	ZENITH_ASSERT_TRUE(xBS.GetBlendPointPosition(0, fPosition), "point 0 still reads");
	ZENITH_ASSERT_EQ_FLOAT(fPosition, -2.0f, 1e-6f, "★ carrying the value the accepted move gave it");
}

ZENITH_TEST(Animation, BlendSpace2DAPositionEditKeepsTheIndexAndReTriangulates)
{
	Flux_BlendTreeNode_BlendSpace2D xBS;
	xBS.AddBlendPoint(new MockBlendNodeWithTime(0.10f), Zenith_Maths::Vector2(0.0f, 0.0f));
	xBS.AddBlendPoint(new MockBlendNodeWithTime(0.50f), Zenith_Maths::Vector2(1.0f, 0.0f));
	xBS.AddBlendPoint(new MockBlendNodeWithTime(0.90f), Zenith_Maths::Vector2(0.0f, 1.0f));
	xBS.ComputeTriangulation();

	ZENITH_ASSERT_EQ(xBS.GetBlendPointCount(), 3u, "three points");
	Flux_BlendTreeNode* pxSecond = xBS.GetBlendPointNode(1);
	ZENITH_ASSERT_NOT_NULL(pxSecond, "point 1 carries its child");

	// The nearest-point read is the cheapest observable that depends on the
	// stored positions, so it is what tells us the write landed.
	xBS.SetParameter(Zenith_Maths::Vector2(0.95f, 0.0f));
	ZENITH_ASSERT_TRUE(BlendSpaceFloatEquals(xBS.GetNormalizedTime(), 0.50f),
		"the parameter sits on top of point 1");

	ZENITH_ASSERT_TRUE(xBS.SetBlendPointPosition(1, Zenith_Maths::Vector2(-4.0f, -4.0f)), "point 1 moves");
	ZENITH_ASSERT_TRUE(xBS.GetBlendPointNode(1) == pxSecond,
		"★ and it is STILL index 1 — a 2D space has no order for a move to renumber");

	Zenith_Maths::Vector2 xRead(0.0f);
	ZENITH_ASSERT_TRUE(xBS.GetBlendPointPosition(1, xRead), "its position reads back");
	ZENITH_ASSERT_EQ_FLOAT(xRead.x, -4.0f, 1e-6f, "as the value written");
	ZENITH_ASSERT_EQ_FLOAT(xRead.y, -4.0f, 1e-6f, "on both axes");
	ZENITH_ASSERT_TRUE(BlendSpaceFloatEquals(xBS.GetNormalizedTime(), 0.10f),
		"★ and the same parameter now finds point 0 — the sampler sees the NEW position");

	ZENITH_ASSERT_FALSE(xBS.SetBlendPointPosition(9, Zenith_Maths::Vector2(0.0f, 0.0f)),
		"an index past the end is refused");
	ZENITH_ASSERT_FALSE(xBS.GetBlendPointPosition(9, xRead), "and reading past the end refuses too");
}

// ============================================================================
// SetNormalizedTime — the inverse of GetNormalizedTime, one virtual per node.
//
// These pin the RECURSION SHAPE, which is the half a strcmp walk got wrong by
// omission: a node type missing from its case list was silently left at zero.
// Each case therefore asserts on the child the READ side would never look at —
// an unselected Select branch, an Additive node's additive layer, a blend point
// the parameter is nowhere near — because that is the assertion a walk written
// from GetNormalizedTime would fail.
// ============================================================================

ZENITH_TEST(Animation, SetNormalizedTime_ClipDelegatesToSetCurrentTimestamp)
{
	Flux_AnimationClip xClip;
	WU5A_InitSpanClip(xClip, "Scrub", 2.0f, true);

	Flux_BlendTreeNode_Clip xNode(&xClip, 1.0f);
	Flux_SkeletonPose xPose;
	Zenith_SkeletonAsset xSkeleton;

	// Advance first, so there IS a mark behind the playhead and a span pending —
	// the two things a raw write to m_fCurrentTimestamp would leave stale.
	xNode.SetEvalWeight(1.0f);
	xNode.Evaluate(0.5f, xPose, xSkeleton);
	ZENITH_ASSERT_EQ_FLOAT(xNode.GetCurrentTimestamp(), 0.5f, 1e-5f,
		"the playhead moved, otherwise the scrub below proves nothing");

	xNode.SetNormalizedTime(0.75f);

	ZENITH_ASSERT_EQ_FLOAT(xNode.GetCurrentTimestamp(), 1.5f, 1e-5f,
		"0.75 of a 2s clip is 1.5 SECONDS — the leaf is where the fraction is converted");
	ZENITH_ASSERT_EQ_FLOAT(xNode.GetNormalizedTime(), 0.75f, 1e-5f,
		"and it reads back as the fraction that was asked for");
	ZENITH_ASSERT_EQ_FLOAT(xNode.GetPreviousTimestamp(), 1.5f, 1e-5f,
		"★ the MARK came with it (D40) — this is a scrub, not a step");

	Zenith_Vector<Flux_ClipEventSpan> xSpans;
	xNode.CollectEventSpans(&xSpans);
	ZENITH_ASSERT_EQ(xSpans.GetSize(), 0u,
		"★ and the pending span was dropped — leaving it would replay every event the playhead was dropped past");

	// ★ AN UNRESOLVED LEAF GOES TO 0, not to fNormalizedTime * 0. Same number,
	// different reason, and the reason is what stops the guard being simplified
	// away once a reload starts resolving clip names later than it does today.
	Flux_BlendTreeNode_Clip xUnresolved;
	xUnresolved.SetNormalizedTime(0.75f);
	ZENITH_ASSERT_EQ_FLOAT(xUnresolved.GetCurrentTimestamp(), 0.0f, 1e-5f,
		"a leaf with no clip has no duration to be a fraction of");
}

ZENITH_TEST(Animation, SetNormalizedTime_BlendWritesBothChildren)
{
	Flux_AnimationClip xClipA;
	Flux_AnimationClip xClipB;
	WU5A_InitSpanClip(xClipA, "A", 2.0f, true);
	WU5A_InitSpanClip(xClipB, "B", 4.0f, true);

	// Deliberately different durations: one fraction, two conversions.
	Flux_BlendTreeNode_Clip* pxA = new Flux_BlendTreeNode_Clip(&xClipA);
	Flux_BlendTreeNode_Clip* pxB = new Flux_BlendTreeNode_Clip(&xClipB);
	Flux_BlendTreeNode_Blend xBlend(pxA, pxB, 0.25f);

	xBlend.SetNormalizedTime(0.5f);

	ZENITH_ASSERT_EQ_FLOAT(pxA->GetCurrentTimestamp(), 1.0f, 1e-5f, "0.5 of the 2s child");
	ZENITH_ASSERT_EQ_FLOAT(pxB->GetCurrentTimestamp(), 2.0f, 1e-5f, "0.5 of the 4s child");
	ZENITH_ASSERT_EQ_FLOAT(xBlend.GetNormalizedTime(), 0.5f, 1e-5f,
		"★ and the composite reads back what was written: mix(0.5, 0.5, w) is 0.5 for ANY blend weight");
}

ZENITH_TEST(Animation, SetNormalizedTime_BlendSpace1DWritesEveryPoint)
{
	Flux_AnimationClip xWalk;
	Flux_AnimationClip xRun;
	WU5A_InitSpanClip(xWalk, "Walk", 2.0f, true);
	WU5A_InitSpanClip(xRun, "Run", 5.0f, true);

	Flux_BlendTreeNode_Clip* pxWalk = new Flux_BlendTreeNode_Clip(&xWalk);
	Flux_BlendTreeNode_Clip* pxRun = new Flux_BlendTreeNode_Clip(&xRun);

	Flux_BlendTreeNode_BlendSpace1D xBS;
	xBS.AddBlendPoint(pxWalk, 0.0f);
	xBS.AddBlendPoint(pxRun, 1.0f);
	xBS.SortBlendPoints();

	// The parameter sits ON point 0, so GetNormalizedTime can only ever see that
	// one — which is exactly why the interesting assertion below is on point 1.
	xBS.SetParameter(0.0f);
	xBS.SetNormalizedTime(0.4f);

	ZENITH_ASSERT_EQ_FLOAT(pxWalk->GetCurrentTimestamp(), 0.8f, 1e-5f, "0.4 of the 2s point");
	ZENITH_ASSERT_EQ_FLOAT(pxRun->GetCurrentTimestamp(), 2.0f, 1e-5f,
		"★ and the point the parameter is nowhere near moved too — 0.4 of 5s");
	ZENITH_ASSERT_EQ_FLOAT(xBS.GetNormalizedTime(), 0.4f, 1e-5f,
		"the nearest-point read gives back the fraction that was written");
}

ZENITH_TEST(Animation, SetNormalizedTime_BlendSpace2DWritesEveryPoint)
{
	Flux_AnimationClip xIdle;
	Flux_AnimationClip xStrafe;
	Flux_AnimationClip xSprint;
	WU5A_InitSpanClip(xIdle, "Idle", 1.0f, true);
	WU5A_InitSpanClip(xStrafe, "Strafe", 2.0f, true);
	WU5A_InitSpanClip(xSprint, "Sprint", 8.0f, true);

	Flux_BlendTreeNode_Clip* pxIdle = new Flux_BlendTreeNode_Clip(&xIdle);
	Flux_BlendTreeNode_Clip* pxStrafe = new Flux_BlendTreeNode_Clip(&xStrafe);
	Flux_BlendTreeNode_Clip* pxSprint = new Flux_BlendTreeNode_Clip(&xSprint);

	Flux_BlendTreeNode_BlendSpace2D xBS;
	xBS.AddBlendPoint(pxIdle, Zenith_Maths::Vector2(0.0f, 0.0f));
	xBS.AddBlendPoint(pxStrafe, Zenith_Maths::Vector2(10.0f, 0.0f));
	xBS.AddBlendPoint(pxSprint, Zenith_Maths::Vector2(0.0f, 10.0f));
	xBS.ComputeTriangulation();

	xBS.SetParameter(Zenith_Maths::Vector2(0.0f, 0.0f));  // sitting on point 0
	xBS.SetNormalizedTime(0.25f);

	ZENITH_ASSERT_EQ_FLOAT(pxIdle->GetCurrentTimestamp(), 0.25f, 1e-5f, "0.25 of the 1s point");
	ZENITH_ASSERT_EQ_FLOAT(pxStrafe->GetCurrentTimestamp(), 0.5f, 1e-5f, "★ 0.25 of the 2s point, ten units away");
	ZENITH_ASSERT_EQ_FLOAT(pxSprint->GetCurrentTimestamp(), 2.0f, 1e-5f, "★ and 0.25 of the 8s point");
	ZENITH_ASSERT_EQ_FLOAT(xBS.GetNormalizedTime(), 0.25f, 1e-5f, "the space reads back the fraction");
}

ZENITH_TEST(Animation, SetNormalizedTime_AdditiveWritesBaseAndAdditive)
{
	Flux_AnimationClip xBase;
	Flux_AnimationClip xAdd;
	WU5A_InitSpanClip(xBase, "Base", 2.0f, true);
	WU5A_InitSpanClip(xAdd, "Aim", 4.0f, true);

	Flux_BlendTreeNode_Clip* pxBase = new Flux_BlendTreeNode_Clip(&xBase);
	Flux_BlendTreeNode_Clip* pxAdd = new Flux_BlendTreeNode_Clip(&xAdd);

	Flux_BlendTreeNode_Additive xAdditive;
	xAdditive.SetBaseNode(pxBase);
	xAdditive.SetAdditiveNode(pxAdd);

	xAdditive.SetNormalizedTime(0.5f);

	ZENITH_ASSERT_EQ_FLOAT(pxBase->GetCurrentTimestamp(), 1.0f, 1e-5f, "the base moved");
	ZENITH_ASSERT_EQ_FLOAT(pxAdd->GetCurrentTimestamp(), 2.0f, 1e-5f,
		"★ and so did the ADDITIVE layer, which GetNormalizedTime never reads — "
		"a walk mirroring the read would leave the two a playthrough apart");
	ZENITH_ASSERT_EQ_FLOAT(xAdditive.GetNormalizedTime(), 0.5f, 1e-5f, "the base's time is the node's time");
}

ZENITH_TEST(Animation, SetNormalizedTime_MaskedWritesBaseAndOverride)
{
	Flux_AnimationClip xBase;
	Flux_AnimationClip xOverride;
	WU5A_InitSpanClip(xBase, "Base", 2.0f, true);
	WU5A_InitSpanClip(xOverride, "UpperBody", 4.0f, true);

	Flux_BlendTreeNode_Clip* pxBase = new Flux_BlendTreeNode_Clip(&xBase);
	Flux_BlendTreeNode_Clip* pxOverride = new Flux_BlendTreeNode_Clip(&xOverride);

	Flux_BlendTreeNode_Masked xMasked;
	xMasked.SetBaseNode(pxBase);
	xMasked.SetOverrideNode(pxOverride);

	// The mask is left EMPTY on purpose: the override branch contributes nothing
	// to the pose, and its playhead must still be put back.
	xMasked.SetNormalizedTime(0.5f);

	ZENITH_ASSERT_EQ_FLOAT(pxBase->GetCurrentTimestamp(), 1.0f, 1e-5f, "the base moved");
	ZENITH_ASSERT_EQ_FLOAT(pxOverride->GetCurrentTimestamp(), 2.0f, 1e-5f,
		"★ and so did the OVERRIDE branch, which GetNormalizedTime never reads");
	ZENITH_ASSERT_EQ_FLOAT(xMasked.GetNormalizedTime(), 0.5f, 1e-5f, "the base's time is the node's time");
}

ZENITH_TEST(Animation, SetNormalizedTime_SelectWritesEveryChildNotOnlySelected)
{
	Flux_AnimationClip xAttackA;
	Flux_AnimationClip xAttackB;
	WU5A_InitSpanClip(xAttackA, "AttackA", 2.0f, false);
	WU5A_InitSpanClip(xAttackB, "AttackB", 4.0f, false);

	Flux_BlendTreeNode_Clip* pxA = new Flux_BlendTreeNode_Clip(&xAttackA);
	Flux_BlendTreeNode_Clip* pxB = new Flux_BlendTreeNode_Clip(&xAttackB);

	Flux_BlendTreeNode_Select xSelect;
	xSelect.AddChild(pxA);
	xSelect.AddChild(pxB);
	// The default index is 0; SetSelectedIndex RESETS the branch it moves to, so
	// switching after the write would prove nothing about the write.
	ZENITH_ASSERT_EQ(xSelect.GetSelectedIndex(), 0, "branch 0 is the selected one");

	xSelect.SetNormalizedTime(0.5f);

	ZENITH_ASSERT_EQ_FLOAT(pxA->GetCurrentTimestamp(), 1.0f, 1e-5f, "the selected branch moved");
	ZENITH_ASSERT_EQ_FLOAT(pxB->GetCurrentTimestamp(), 2.0f, 1e-5f,
		"★ and so did the UNSELECTED one — it is the pose the frame the index changes, "
		"and nothing else in the system moves it");
	ZENITH_ASSERT_EQ_FLOAT(xSelect.GetNormalizedTime(), 0.5f, 1e-5f,
		"the read still reports only the selected branch, which is the deliberate asymmetry");
}

ZENITH_TEST(Animation, SetNormalizedTime_DefaultIsNoOp)
{
	// ★ THE DEFAULTS ARE NON-PURE FOR THIS CASE. MockBlendNodeWithTime overrides
	// neither new virtual — a node type with no time and no clips of its own keeps
	// what it had rather than being handed a wrong value, which is exactly what the
	// old strcmp walk did by OMITTING it from the case list. Making either virtual
	// pure would also stop this TU compiling.
	MockBlendNodeWithTime xMock(0.42f);
	Flux_AnimationClipCollection xCollection;

	xMock.SetNormalizedTime(0.9f);
	ZENITH_ASSERT_EQ_FLOAT(xMock.GetNormalizedTime(), 0.42f, 1e-5f,
		"a node that does not override keeps its own time");

	xMock.ResolveClips(&xCollection);
	ZENITH_ASSERT_EQ_FLOAT(xMock.GetNormalizedTime(), 0.42f, 1e-5f,
		"and resolving clips it does not have changes nothing");
}

ZENITH_TEST(Animation, ResolveClips_SelectResolvesEveryLeafFromTheCollection)
{
	Flux_AnimationClipCollection xCollection;
	Flux_AnimationClip* pxIdleClip = new Flux_AnimationClip();
	pxIdleClip->SetName("Idle");
	Flux_AnimationClip* pxWalkClip = new Flux_AnimationClip();
	pxWalkClip->SetName("Walk");
	xCollection.AddClip(pxIdleClip);
	xCollection.AddClip(pxWalkClip);

	Flux_BlendTreeNode_Clip* pxLeafA = new Flux_BlendTreeNode_Clip();
	pxLeafA->SetClipName("Idle");
	Flux_BlendTreeNode_Clip* pxLeafB = new Flux_BlendTreeNode_Clip();
	pxLeafB->SetClipName("Walk");

	Flux_BlendTreeNode_Select xSelect;
	xSelect.AddChild(pxLeafA);
	xSelect.AddChild(pxLeafB);

	ZENITH_ASSERT_NULL(pxLeafA->GetClip(), "a deserialized leaf starts holding a NAME and no clip");
	ZENITH_ASSERT_NULL(pxLeafB->GetClip(), "including the branch the index does not select");

	xSelect.ResolveClips(&xCollection);

	ZENITH_ASSERT_TRUE(pxLeafA->GetClip() == pxIdleClip, "the selected leaf found its clip by name");
	ZENITH_ASSERT_TRUE(pxLeafB->GetClip() == pxWalkClip,
		"★ and so did the UNSELECTED one — an unresolved branch poses the bind pose, silently, "
		"the first frame it is selected");
}
