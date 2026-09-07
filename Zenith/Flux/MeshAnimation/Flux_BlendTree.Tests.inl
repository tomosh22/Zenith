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
