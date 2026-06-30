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
	xRM.m_xPositionDeltas.EmplaceBack(Zenith_Maths::Vector3(10.0f, 20.0f, 30.0f), 0.0f);
	Zenith_Maths::Vector3 xResult = xRM.SamplePositionDelta(0.5f);
	ZENITH_ASSERT_TRUE(RootMotionVec3Equals(xResult, Zenith_Maths::Vector3(0.0f)),
		"Disabled root motion must return zero vector regardless of data");
}

ZENITH_TEST(Animation, RootMotionPositionSingleKeyframe) { Zenith_UnitTests::TestRootMotionPositionSingleKeyframe(); }
void Zenith_UnitTests::TestRootMotionPositionSingleKeyframe()
{
	Flux_RootMotion xRM;
	xRM.m_bEnabled = true;
	xRM.m_xPositionDeltas.EmplaceBack(Zenith_Maths::Vector3(7.0f, 8.0f, 9.0f), 0.0f);
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
	xRM.m_xPositionDeltas.EmplaceBack(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), 0.0f);
	xRM.m_xPositionDeltas.EmplaceBack(Zenith_Maths::Vector3(10.0f, 20.0f, 30.0f), 1.0f);
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
	xRM.m_xPositionDeltas.EmplaceBack(Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f), 0.0f);
	xRM.m_xPositionDeltas.EmplaceBack(Zenith_Maths::Vector3(4.0f, 5.0f, 6.0f), 1.0f);
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
	xRM.m_xPositionDeltas.EmplaceBack(Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f), 0.0f);
	xRM.m_xPositionDeltas.EmplaceBack(Zenith_Maths::Vector3(99.0f, 99.0f, 99.0f), 0.0f);
	xRM.m_xPositionDeltas.EmplaceBack(Zenith_Maths::Vector3(2.0f, 0.0f, 0.0f), 1.0f);
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
	xRM.m_xPositionDeltas.EmplaceBack(Zenith_Maths::Vector3(0.0f), 0.0f);
	xRM.m_xPositionDeltas.EmplaceBack(Zenith_Maths::Vector3(10.0f, 0.0f, 0.0f), 1.0f);
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
	xRM.m_xRotationDeltas.EmplaceBack(xQuat, 0.0f);
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
	xRM.m_xRotationDeltas.EmplaceBack(xQ0, 0.0f);
	xRM.m_xRotationDeltas.EmplaceBack(xQ1, 1.0f);
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
	xRM.m_xRotationDeltas.EmplaceBack(xQ0, 0.0f);
	xRM.m_xRotationDeltas.EmplaceBack(xQ1, 1.0f);
	Zenith_Maths::Quat xResult = xRM.SampleRotationDelta(99.0f);
	ZENITH_ASSERT_TRUE(RootMotionQuatEquals(xResult, xQ1),
		"fTime past last rotation keyframe must clamp to back value");
}

// ============================================================================
// Flux_BoneChannel end-of-clip sampling (regression for the instanced-tree leaf
// "teleport").
//
// GetRotationIndex/Position/Scale used to `return 0` (the FIRST segment) when
// fTime was at/past the last keyframe. Sample*() then computed
// scaleFactor = (fTime - firstKeyTime) / firstSegLen — a huge value — and
// EXTRAPOLATED the first segment instead of holding the last keyframe. A VAT bake
// samples its final frame at EXACTLY t=duration (the last keyframe time), so that
// corrupted the last baked frame; the GPU two-tap lerp reached into it at every
// loop wrap, making each instanced tree lurch for one frame (phase-staggered ->
// scattered across the forest, only while animating, with a correct anim time).
// The fix returns the last keyframe index so Sample*() clamps to it.
// ============================================================================

ZENITH_TEST(Animation, BoneChannelRotationClampsAtClipEnd) { Zenith_UnitTests::TestBoneChannelRotationClampsAtClipEnd(); }
void Zenith_UnitTests::TestBoneChannelRotationClampsAtClipEnd()
{
	Flux_BoneChannel xChannel;
	const Zenith_Maths::Quat xQ0 = glm::angleAxis(glm::radians(0.0f),  Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f));
	const Zenith_Maths::Quat xQ1 = glm::angleAxis(glm::radians(30.0f), Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f));
	const Zenith_Maths::Quat xQ2 = glm::angleAxis(glm::radians(3.0f),  Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f));
	xChannel.AddRotationKeyframe(0.0f,  xQ0);
	xChannel.AddRotationKeyframe(10.0f, xQ1);
	xChannel.AddRotationKeyframe(20.0f, xQ2);  // last keyframe (a near-rest "loop close")
	xChannel.SortKeyframes();

	// At EXACTLY the last keyframe time — the VAT-bake case. Must be the last keyframe,
	// NOT a wild extrapolation of the first segment (the old return-0 bug gave
	// slerp(xQ0, xQ1, 2.0) ~= 60deg here).
	ZENITH_ASSERT_TRUE(RootMotionQuatEquals(xChannel.SampleRotation(20.0f), xQ2),
		"SampleRotation at the last keyframe time must return the last keyframe");
	// Past the end clamps to the last keyframe (no extrapolation).
	ZENITH_ASSERT_TRUE(RootMotionQuatEquals(xChannel.SampleRotation(50.0f), xQ2),
		"SampleRotation past the clip end must clamp to the last keyframe");
	// The fix must not disturb interior sampling.
	ZENITH_ASSERT_TRUE(RootMotionQuatEquals(xChannel.SampleRotation(0.0f), xQ0),
		"SampleRotation at t=0 must return the first keyframe");
	ZENITH_ASSERT_TRUE(RootMotionQuatEquals(xChannel.SampleRotation(10.0f), xQ1),
		"SampleRotation at an interior keyframe time must return that keyframe");
}

ZENITH_TEST(Animation, BoneChannelPositionClampsAtClipEnd) { Zenith_UnitTests::TestBoneChannelPositionClampsAtClipEnd(); }
void Zenith_UnitTests::TestBoneChannelPositionClampsAtClipEnd()
{
	Flux_BoneChannel xChannel;
	xChannel.AddPositionKeyframe(0.0f,  Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xChannel.AddPositionKeyframe(10.0f, Zenith_Maths::Vector3(5.0f, 0.0f, 0.0f));
	xChannel.AddPositionKeyframe(20.0f, Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f));
	xChannel.SortKeyframes();

	// Old bug: GetPositionIndex returned 0 -> mix(p0,p1, fTime/firstSegLen) extrapolated.
	ZENITH_ASSERT_TRUE(RootMotionVec3Equals(xChannel.SamplePosition(20.0f), Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f)),
		"SamplePosition at the last keyframe time must return the last keyframe");
	ZENITH_ASSERT_TRUE(RootMotionVec3Equals(xChannel.SamplePosition(50.0f), Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f)),
		"SamplePosition past the clip end must clamp to the last keyframe");
	ZENITH_ASSERT_TRUE(RootMotionVec3Equals(xChannel.SamplePosition(5.0f), Zenith_Maths::Vector3(2.5f, 0.0f, 0.0f)),
		"SamplePosition mid-first-segment must interpolate (t=5 -> halfway to (5,0,0))");
}

ZENITH_TEST(Animation, BoneChannelScaleClampsAtClipEnd) { Zenith_UnitTests::TestBoneChannelScaleClampsAtClipEnd(); }
void Zenith_UnitTests::TestBoneChannelScaleClampsAtClipEnd()
{
	Flux_BoneChannel xChannel;
	xChannel.AddScaleKeyframe(0.0f,  Zenith_Maths::Vector3(1.0f, 1.0f, 1.0f));
	xChannel.AddScaleKeyframe(10.0f, Zenith_Maths::Vector3(2.0f, 2.0f, 2.0f));
	xChannel.AddScaleKeyframe(20.0f, Zenith_Maths::Vector3(1.5f, 1.5f, 1.5f));
	xChannel.SortKeyframes();

	ZENITH_ASSERT_TRUE(RootMotionVec3Equals(xChannel.SampleScale(20.0f), Zenith_Maths::Vector3(1.5f, 1.5f, 1.5f)),
		"SampleScale at the last keyframe time must return the last keyframe");
	ZENITH_ASSERT_TRUE(RootMotionVec3Equals(xChannel.SampleScale(50.0f), Zenith_Maths::Vector3(1.5f, 1.5f, 1.5f)),
		"SampleScale past the clip end must clamp to the last keyframe");
}

// ============================================================================
// Timestamped-keyframe serialization helpers (Flux_Write/ReadVec3Keys + ...QuatKeys).
// Pins the on-disk byte LENGTH (so a format change can't slip through internally-
// consistent round-trips) AND verifies a full round-trip. These back the shared
// helpers the MeshAnimation Read/WriteToDataStream paths were migrated onto.
// ============================================================================
ZENITH_TEST(AnimationSerialization, Vec3KeysRoundTripAndByteLength)
{
	Zenith_Vector<std::pair<Zenith_Maths::Vector3, float>> xKeys;
	xKeys.PushBack(std::make_pair(Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f), 0.5f));
	xKeys.PushBack(std::make_pair(Zenith_Maths::Vector3(4.0f, 5.0f, 6.0f), 1.5f));

	Zenith_DataStream xStream;
	Flux_WriteVec3Keys(xStream, xKeys);

	// Format pin: uint32 count + N * (3 position floats + 1 time float) = 4 + N*16.
	const uint64_t uExpected = sizeof(uint32_t) + 2ull * (4ull * sizeof(float));
	ZENITH_ASSERT_EQ(xStream.GetCursor(), uExpected, "Vec3 keys must serialize to count(4)+N*16 bytes; got %llu", xStream.GetCursor());

	xStream.SetCursor(0);
	Zenith_Vector<std::pair<Zenith_Maths::Vector3, float>> xOut;
	Flux_ReadVec3Keys(xStream, xOut);
	ZENITH_ASSERT_EQ(xOut.GetSize(), 2u, "round-trip restores 2 keys");
	ZENITH_ASSERT_TRUE(xOut.Get(0).first.x == 1.0f && xOut.Get(0).first.z == 3.0f && xOut.Get(0).second == 0.5f, "key 0 round-trips exactly");
	ZENITH_ASSERT_TRUE(xOut.Get(1).first.y == 5.0f && xOut.Get(1).second == 1.5f, "key 1 round-trips exactly");
}

ZENITH_TEST(AnimationSerialization, QuatKeysRoundTripAndByteLength)
{
	Zenith_Vector<std::pair<Zenith_Maths::Quat, float>> xKeys;
	xKeys.PushBack(std::make_pair(Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f), 0.25f)); // Quat(w,x,y,z) = identity

	Zenith_DataStream xStream;
	Flux_WriteQuatKeys(xStream, xKeys);

	// Format pin: uint32 count + N * (4 quat floats + 1 time float) = 4 + N*20.
	const uint64_t uExpected = sizeof(uint32_t) + 1ull * (5ull * sizeof(float));
	ZENITH_ASSERT_EQ(xStream.GetCursor(), uExpected, "Quat keys must serialize to count(4)+N*20 bytes; got %llu", xStream.GetCursor());

	xStream.SetCursor(0);
	Zenith_Vector<std::pair<Zenith_Maths::Quat, float>> xOut;
	Flux_ReadQuatKeys(xStream, xOut);
	ZENITH_ASSERT_EQ(xOut.GetSize(), 1u, "round-trip restores 1 key");
	ZENITH_ASSERT_TRUE(xOut.Get(0).first.w == 1.0f && xOut.Get(0).second == 0.25f, "quat key round-trips exactly");
}
