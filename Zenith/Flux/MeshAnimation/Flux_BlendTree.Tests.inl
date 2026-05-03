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
