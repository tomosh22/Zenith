#include "UnitTests/Zenith_UnitTests.h"
#include "Flux/MeshAnimation/Flux_BlendTree.h"

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
