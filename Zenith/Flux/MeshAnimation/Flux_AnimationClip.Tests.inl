#include "UnitTests/Zenith_UnitTests.h"
#include "Flux/MeshAnimation/Flux_AnimationClip.h"

// ============================================================================
// Flux_RootMotion sample tests
//
// SamplePositionDelta and SampleRotationDelta share an internal templated
// helper (SampleRootMotionDeltas) which handles bracket-finding and a few
// edge cases (empty / single-keyframe / past-end / identical-timestamp).
// These tests pin the public-API behavior of all branches.
// ============================================================================

namespace
{
	bool RootMotionVec3Equals(const Zenith_Maths::Vector3& a, const Zenith_Maths::Vector3& b, float fTol = 1e-5f)
	{
		return std::abs(a.x - b.x) < fTol
			&& std::abs(a.y - b.y) < fTol
			&& std::abs(a.z - b.z) < fTol;
	}
	bool RootMotionQuatEquals(const Zenith_Maths::Quat& a, const Zenith_Maths::Quat& b, float fTol = 1e-5f)
	{
		// Quaternion comparison must allow for double-cover (q and -q are
		// the same rotation). Use absolute dot product close to 1.
		float fDot = std::abs(a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z);
		return std::abs(fDot - 1.0f) < fTol;
	}
}

ZENITH_TEST(Animation, RootMotionPositionEmpty) { Zenith_UnitTests::TestRootMotionPositionEmpty(); }
void Zenith_UnitTests::TestRootMotionPositionEmpty()
{
	Flux_RootMotion xRM;
	xRM.m_bEnabled = true;
	// Empty deltas → identity (Vector3 zero).
	Zenith_Maths::Vector3 xResult = xRM.SamplePositionDelta(0.5f);
	ZENITH_ASSERT_TRUE(RootMotionVec3Equals(xResult, Zenith_Maths::Vector3(0.0f)),
		"Empty deltas list must return zero vector");
}

ZENITH_TEST(Animation, RootMotionPositionDisabled) { Zenith_UnitTests::TestRootMotionPositionDisabled(); }
void Zenith_UnitTests::TestRootMotionPositionDisabled()
{
	Flux_RootMotion xRM;
	xRM.m_bEnabled = false; // disabled — even with data, returns identity
	xRM.m_xPositionDeltas.push_back({ Zenith_Maths::Vector3(10.0f, 20.0f, 30.0f), 0.0f });
	Zenith_Maths::Vector3 xResult = xRM.SamplePositionDelta(0.5f);
	ZENITH_ASSERT_TRUE(RootMotionVec3Equals(xResult, Zenith_Maths::Vector3(0.0f)),
		"Disabled root motion must return zero vector regardless of data");
}

ZENITH_TEST(Animation, RootMotionPositionSingleKeyframe) { Zenith_UnitTests::TestRootMotionPositionSingleKeyframe(); }
void Zenith_UnitTests::TestRootMotionPositionSingleKeyframe()
{
	Flux_RootMotion xRM;
	xRM.m_bEnabled = true;
	xRM.m_xPositionDeltas.push_back({ Zenith_Maths::Vector3(7.0f, 8.0f, 9.0f), 0.0f });
	// Single keyframe — fTime irrelevant, returns that keyframe's value.
	ZENITH_ASSERT_TRUE(RootMotionVec3Equals(xRM.SamplePositionDelta(0.0f), Zenith_Maths::Vector3(7.0f, 8.0f, 9.0f)),
		"Single keyframe must return that keyframe's value at t=0");
	ZENITH_ASSERT_TRUE(RootMotionVec3Equals(xRM.SamplePositionDelta(99.0f), Zenith_Maths::Vector3(7.0f, 8.0f, 9.0f)),
		"Single keyframe must return that keyframe's value at any time");
}

ZENITH_TEST(Animation, RootMotionPositionInterpolatesBetween) { Zenith_UnitTests::TestRootMotionPositionInterpolatesBetween(); }
void Zenith_UnitTests::TestRootMotionPositionInterpolatesBetween()
{
	Flux_RootMotion xRM;
	xRM.m_bEnabled = true;
	xRM.m_xPositionDeltas.push_back({ Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), 0.0f });
	xRM.m_xPositionDeltas.push_back({ Zenith_Maths::Vector3(10.0f, 20.0f, 30.0f), 1.0f });
	// fTime=0.5 → midpoint between (0,0,0) at t=0 and (10,20,30) at t=1.
	Zenith_Maths::Vector3 xResult = xRM.SamplePositionDelta(0.5f);
	ZENITH_ASSERT_TRUE(RootMotionVec3Equals(xResult, Zenith_Maths::Vector3(5.0f, 10.0f, 15.0f)),
		"Linear lerp between keyframes must produce midpoint at t=0.5");
}

ZENITH_TEST(Animation, RootMotionPositionPastEnd) { Zenith_UnitTests::TestRootMotionPositionPastEnd(); }
void Zenith_UnitTests::TestRootMotionPositionPastEnd()
{
	Flux_RootMotion xRM;
	xRM.m_bEnabled = true;
	xRM.m_xPositionDeltas.push_back({ Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f), 0.0f });
	xRM.m_xPositionDeltas.push_back({ Zenith_Maths::Vector3(4.0f, 5.0f, 6.0f), 1.0f });
	// fTime past last keyframe — clamps to back value.
	Zenith_Maths::Vector3 xResult = xRM.SamplePositionDelta(2.0f);
	ZENITH_ASSERT_TRUE(RootMotionVec3Equals(xResult, Zenith_Maths::Vector3(4.0f, 5.0f, 6.0f)),
		"fTime past last keyframe must clamp to back keyframe value");
}

ZENITH_TEST(Animation, RootMotionPositionIdenticalTimestamps) { Zenith_UnitTests::TestRootMotionPositionIdenticalTimestamps(); }
void Zenith_UnitTests::TestRootMotionPositionIdenticalTimestamps()
{
	Flux_RootMotion xRM;
	xRM.m_bEnabled = true;
	// Two keyframes at the same timestamp would normally trigger div-by-zero
	// in the lerp parameter; the helper must guard against this and return
	// the lower keyframe's value.
	xRM.m_xPositionDeltas.push_back({ Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f), 0.0f });
	xRM.m_xPositionDeltas.push_back({ Zenith_Maths::Vector3(99.0f, 99.0f, 99.0f), 0.0f });
	xRM.m_xPositionDeltas.push_back({ Zenith_Maths::Vector3(2.0f, 0.0f, 0.0f), 1.0f });
	// At fTime < second keyframe timestamp (which is 0.0, same as first),
	// the bracket-finder uses i=0 with i+1 having same time — guard kicks
	// in and returns xKeys[0].first.
	Zenith_Maths::Vector3 xResult = xRM.SamplePositionDelta(-1.0f);
	ZENITH_ASSERT_TRUE(RootMotionVec3Equals(xResult, Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f)),
		"Identical-timestamp guard must return lower keyframe's value, not divide by zero");
}

ZENITH_TEST(Animation, RootMotionPositionExactKeyframeMatch) { Zenith_UnitTests::TestRootMotionPositionExactKeyframeMatch(); }
void Zenith_UnitTests::TestRootMotionPositionExactKeyframeMatch()
{
	Flux_RootMotion xRM;
	xRM.m_bEnabled = true;
	xRM.m_xPositionDeltas.push_back({ Zenith_Maths::Vector3(0.0f), 0.0f });
	xRM.m_xPositionDeltas.push_back({ Zenith_Maths::Vector3(10.0f, 0.0f, 0.0f), 1.0f });
	// fTime exactly at the first keyframe — bracket [0,1] with t=0,
	// expect (0,0,0) (or extremely close).
	Zenith_Maths::Vector3 xResult = xRM.SamplePositionDelta(0.0f);
	ZENITH_ASSERT_TRUE(RootMotionVec3Equals(xResult, Zenith_Maths::Vector3(0.0f)),
		"fTime exactly at first keyframe must return that keyframe's value");
}

ZENITH_TEST(Animation, RootMotionRotationEmpty) { Zenith_UnitTests::TestRootMotionRotationEmpty(); }
void Zenith_UnitTests::TestRootMotionRotationEmpty()
{
	Flux_RootMotion xRM;
	xRM.m_bEnabled = true;
	Zenith_Maths::Quat xResult = xRM.SampleRotationDelta(0.5f);
	// Empty rotation deltas → identity quaternion (1,0,0,0).
	Zenith_Maths::Quat xExpected(1.0f, 0.0f, 0.0f, 0.0f);
	ZENITH_ASSERT_TRUE(RootMotionQuatEquals(xResult, xExpected),
		"Empty rotation deltas must return identity quaternion");
}

ZENITH_TEST(Animation, RootMotionRotationSingleKeyframe) { Zenith_UnitTests::TestRootMotionRotationSingleKeyframe(); }
void Zenith_UnitTests::TestRootMotionRotationSingleKeyframe()
{
	Flux_RootMotion xRM;
	xRM.m_bEnabled = true;
	// 90 degrees rotation around Y axis as a quaternion.
	Zenith_Maths::Quat xQuat = glm::angleAxis(glm::radians(90.0f), Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
	xRM.m_xRotationDeltas.push_back({ xQuat, 0.0f });
	Zenith_Maths::Quat xResult = xRM.SampleRotationDelta(0.0f);
	ZENITH_ASSERT_TRUE(RootMotionQuatEquals(xResult, xQuat),
		"Single rotation keyframe must return that quaternion");
}

ZENITH_TEST(Animation, RootMotionRotationInterpolatesBetween) { Zenith_UnitTests::TestRootMotionRotationInterpolatesBetween(); }
void Zenith_UnitTests::TestRootMotionRotationInterpolatesBetween()
{
	Flux_RootMotion xRM;
	xRM.m_bEnabled = true;
	Zenith_Maths::Quat xQ0 = glm::angleAxis(glm::radians(0.0f),  Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
	Zenith_Maths::Quat xQ1 = glm::angleAxis(glm::radians(90.0f), Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
	xRM.m_xRotationDeltas.push_back({ xQ0, 0.0f });
	xRM.m_xRotationDeltas.push_back({ xQ1, 1.0f });
	// fTime=0.5 → slerp midpoint = 45 degrees Y-rotation.
	Zenith_Maths::Quat xExpected = glm::angleAxis(glm::radians(45.0f), Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
	Zenith_Maths::Quat xResult = xRM.SampleRotationDelta(0.5f);
	ZENITH_ASSERT_TRUE(RootMotionQuatEquals(xResult, xExpected, 1e-4f),
		"Slerp midpoint between 0deg and 90deg around Y must be 45deg around Y");
}

ZENITH_TEST(Animation, RootMotionRotationPastEnd) { Zenith_UnitTests::TestRootMotionRotationPastEnd(); }
void Zenith_UnitTests::TestRootMotionRotationPastEnd()
{
	Flux_RootMotion xRM;
	xRM.m_bEnabled = true;
	Zenith_Maths::Quat xQ0 = glm::angleAxis(glm::radians(0.0f),  Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
	Zenith_Maths::Quat xQ1 = glm::angleAxis(glm::radians(90.0f), Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
	xRM.m_xRotationDeltas.push_back({ xQ0, 0.0f });
	xRM.m_xRotationDeltas.push_back({ xQ1, 1.0f });
	Zenith_Maths::Quat xResult = xRM.SampleRotationDelta(99.0f);
	ZENITH_ASSERT_TRUE(RootMotionQuatEquals(xResult, xQ1),
		"fTime past last rotation keyframe must clamp to back value");
}
