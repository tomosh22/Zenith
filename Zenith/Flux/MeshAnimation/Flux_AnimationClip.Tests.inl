#include "UnitTests/Zenith_UnitTests.h"
#include "UnitTests/Zenith_AssertCapture.h"   // the refused-envelope / refused-schema reads assert on purpose
#include "Flux/MeshAnimation/Flux_AnimationClip.h"
#include "Flux/MeshAnimation/Flux_BonePose.h"  // WU-1.2: the SAMPLER, which is where the tick multiply lived
#include "AssetHandling/Zenith_AssetTypeIds.h"
#include "DataStream/Zenith_StreamEnvelope.h"

#include <cstring>   // std::memcmp — the byte-identity determinism check
#include <limits>    // WU-1.3: quiet_NaN / infinity — the D10 rejected key times

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

// ============================================================================
// WU-1.1 — .zanim stream envelope (D1/D2), the new metadata fields (D6/D7/D8),
// the reserved per-key tangent block (D17) and deterministic channel order (D5).
//
// All pure CPU: a clip is built in memory, serialized to a Zenith_DataStream and
// read back. No device, no asset registry entry, no file — so every one of these
// runs unchanged under the Null backend and none is requiresGraphics.
// ============================================================================

namespace
{
	// Byte offsets into a Zenith_StreamHeader, which is four u_ints written in
	// declaration order by Zenith_WriteStreamHeader.
	constexpr uint64_t ulCLIP_HEADER_MAGIC_OFFSET  = 0;
	constexpr uint64_t ulCLIP_HEADER_SCHEMA_OFFSET = 3 * sizeof(u_int);

	void ClipPokeU32(Zenith_DataStream& xStream, uint64_t ulByteOffset, u_int uValue)
	{
		std::memcpy(static_cast<uint8_t*>(xStream.GetData()) + ulByteOffset, &uValue, sizeof(u_int));
	}

	// A small but non-degenerate clip: two bones, all three channel types, an event
	// and root motion, so every serialized block is exercised by a round-trip.
	void ClipBuildTwoBoneClip(Flux_AnimationClip& xClip)
	{
		xClip.SetName("WU11_Probe");
		xClip.SetDuration(2.0f);
		xClip.SetTicksPerSecond(24);

		// Key times are SECONDS inside the 2 s duration (D3). They used to be 0/10/20
		// — tick counts beside a 2-second clip, which is the two-clocks shape D3
		// removed; the byte layout is identical either way.
		Flux_BoneChannel xHip;
		xHip.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
		xHip.AddPositionKeyframe(1.0f, Zenith_Maths::Vector3(0.0f, 1.5f, 0.0f));
		xHip.AddRotationKeyframe(0.0f, Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f));
		xHip.AddScaleKeyframe   (0.0f, Zenith_Maths::Vector3(1.0f, 1.0f, 1.0f));
		xClip.AddBoneChannel("Hip", std::move(xHip));

		Flux_BoneChannel xKnee;
		xKnee.AddRotationKeyframe(0.0f, glm::angleAxis(glm::radians(10.0f), Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f)));
		xKnee.AddRotationKeyframe(2.0f, glm::angleAxis(glm::radians(40.0f), Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f)));
		xClip.AddBoneChannel("Knee", std::move(xKnee));

		Flux_AnimationEvent xEvent;
		xEvent.m_fNormalizedTime = 0.5f;
		xEvent.m_strEventName = "FootstepLeft";
		xEvent.m_xData = Zenith_Maths::Vector4(1.0f, 2.0f, 3.0f, 4.0f);
		xClip.AddEvent(xEvent);

		xClip.GetRootMotion().m_bEnabled = true;
		xClip.GetRootMotion().m_xPositionDeltas.EmplaceBack(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), 0.0f);
		xClip.GetRootMotion().m_xPositionDeltas.EmplaceBack(Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f), 1.0f);
	}
}

ZENITH_TEST(AnimationSerialization, ClipStreamEnvelopeRoundtrip)
{
	Flux_AnimationClip xClip;
	ClipBuildTwoBoneClip(xClip);

	Zenith_DataStream xStream;
	xClip.WriteToDataStream(xStream);

	// The envelope must be the FIRST thing on the wire, carrying this asset's id and
	// the current schema — not the skeleton's id, not a bare version word.
	xStream.SetCursor(0);
	Zenith_Result<Zenith_StreamHeader> xHdr = Zenith_ReadStreamHeader(xStream, uZENITH_ANIMATION_ASSET_TYPE_ID);
	ZENITH_ASSERT_TRUE(xHdr.IsOk(), "clip write must emit the shared stream envelope");
	if (xHdr.IsOk())
	{
		ZENITH_ASSERT_EQ(xHdr.Value().m_uAssetTypeId, uZENITH_ANIMATION_ASSET_TYPE_ID, "animation envelope type id");
		ZENITH_ASSERT_EQ(xHdr.Value().m_uSchemaVersion, uZENITH_ANIMATION_SCHEMA_CURRENT, "animation envelope schema");
	}

	// A wrong expected type-id is a wrong-type file, and must be refused.
	xStream.SetCursor(0);
	ZENITH_ASSERT_FALSE(Zenith_ReadStreamHeader(xStream, uZENITH_SKELETON_ASSET_TYPE_ID).IsOk(),
		"a .zanim envelope must not validate as a .zskel");

	xStream.SetCursor(0);
	Flux_AnimationClip xLoaded;
	xLoaded.ReadFromDataStream(xStream);
	ZENITH_ASSERT_TRUE(xLoaded.GetName() == "WU11_Probe", "clip name round-trips");
	ZENITH_ASSERT_EQ_FLOAT(xLoaded.GetDuration(), 2.0f, 1e-6f, "duration round-trips");
	ZENITH_ASSERT_EQ(xLoaded.GetBoneChannels().GetSize(), 2u, "both bone channels round-trip");
	ZENITH_ASSERT_TRUE(xLoaded.HasBoneChannel("Hip") && xLoaded.HasBoneChannel("Knee"), "bone names round-trip");
	ZENITH_ASSERT_EQ(xLoaded.GetEvents().GetSize(), 1u, "the event round-trips");
	ZENITH_ASSERT_TRUE(xLoaded.GetRootMotion().m_bEnabled, "root motion enable flag round-trips");
	ZENITH_ASSERT_EQ(xLoaded.GetRootMotion().m_xPositionDeltas.GetSize(), 2u, "root motion deltas round-trip");
	const Flux_BoneChannel* pxHip = xLoaded.GetBoneChannel("Hip");
	ZENITH_ASSERT_TRUE(pxHip != nullptr, "Hip channel resolves after the round-trip");
	if (pxHip != nullptr)
	{
		ZENITH_ASSERT_EQ(pxHip->GetPositionKeyframes().GetSize(), 2u, "Hip position keys round-trip");
	}
}

ZENITH_TEST(AnimationSerialization, ClipRefusesAFutureSchemaVersion)
{
	Flux_AnimationClip xClip;
	ClipBuildTwoBoneClip(xClip);

	Zenith_DataStream xStream;
	xClip.WriteToDataStream(xStream);

	// Poke the schema word one past current. The envelope itself is still well formed,
	// so this is exactly the "a newer tool wrote this file" case — it must be REFUSED,
	// not parsed as if it were the current layout (which would read one field order
	// with another's).
	ClipPokeU32(xStream, ulCLIP_HEADER_SCHEMA_OFFSET, uZENITH_ANIMATION_SCHEMA_CURRENT + 1u);
	xStream.SetCursor(0);

	Flux_AnimationClip xLoaded;
	{
		Zenith_AssertCaptureScope xCapture;
		xLoaded.ReadFromDataStream(xStream);
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "a future schema must assert exactly once");
	}
	ZENITH_ASSERT_EQ(xLoaded.GetBoneChannels().GetSize(), 0u, "a refused load leaves an EMPTY clip");
	ZENITH_ASSERT_EQ(xLoaded.GetEvents().GetSize(), 0u, "a refused load leaves an EMPTY clip");
	ZENITH_ASSERT_TRUE(xLoaded.GetName().empty(), "a refused load leaves an EMPTY clip");
}

ZENITH_TEST(AnimationSerialization, ClipRefusesAHeaderlessStream)
{
	Flux_AnimationClip xClip;
	ClipBuildTwoBoneClip(xClip);

	Zenith_DataStream xStream;
	xClip.WriteToDataStream(xStream);

	// Break the magic. There is deliberately NO legacy branch (D2), so a stream that
	// does not open with the envelope is not a .zanim at all.
	ClipPokeU32(xStream, ulCLIP_HEADER_MAGIC_OFFSET, uSTREAM_ENVELOPE_MAGIC ^ 0xFFu);
	xStream.SetCursor(0);

	Flux_AnimationClip xLoaded;
	{
		Zenith_AssertCaptureScope xCapture;
		xLoaded.ReadFromDataStream(xStream);
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "a missing envelope must assert exactly once");
	}
	ZENITH_ASSERT_EQ(xLoaded.GetBoneChannels().GetSize(), 0u, "a refused load leaves an EMPTY clip");
}

ZENITH_TEST(AnimationSerialization, ClipMetadataNewFieldsRoundTrip)
{
	Flux_AnimationClip xClip;
	ClipBuildTwoBoneClip(xClip);

	// Defaults first — D6 says 30, D8 says false, D7's paths start empty.
	Flux_AnimationClipMetadata xDefaults;
	ZENITH_ASSERT_EQ(xDefaults.m_uAuthoredFrameRate, 30u, "authored frame rate defaults to 30");
	ZENITH_ASSERT_FALSE(xDefaults.m_bGenerated, "a clip is not 'generated' unless it says so");
	ZENITH_ASSERT_TRUE(xDefaults.m_strSkeletonPath.empty(), "skeleton path defaults empty");
	ZENITH_ASSERT_TRUE(xDefaults.m_strPreviewModelPath.empty(), "preview model path defaults empty");

	xClip.GetMetadata().m_uAuthoredFrameRate = 60;
	xClip.GetMetadata().m_strSkeletonPath = "engine:Meshes/StickFigure/StickFigure.zskel";
	xClip.GetMetadata().m_strPreviewModelPath = "engine:Meshes/StickFigure/StickFigure.zmodel";
	xClip.GetMetadata().m_bGenerated = true;
	// The source path is the IMPORT source, and stays distinct from the rig.
	xClip.SetSourcePath("game:Meshes/Humans/Male.glb");

	Zenith_DataStream xStream;
	xClip.WriteToDataStream(xStream);
	xStream.SetCursor(0);

	Flux_AnimationClip xLoaded;
	xLoaded.ReadFromDataStream(xStream);
	ZENITH_ASSERT_EQ(xLoaded.GetMetadata().m_uAuthoredFrameRate, 60u, "authored frame rate round-trips");
	ZENITH_ASSERT_TRUE(xLoaded.GetMetadata().m_strSkeletonPath == "engine:Meshes/StickFigure/StickFigure.zskel",
		"skeleton path round-trips");
	ZENITH_ASSERT_TRUE(xLoaded.GetMetadata().m_strPreviewModelPath == "engine:Meshes/StickFigure/StickFigure.zmodel",
		"preview model path round-trips");
	ZENITH_ASSERT_TRUE(xLoaded.GetMetadata().m_bGenerated, "generated flag round-trips");
	ZENITH_ASSERT_TRUE(xLoaded.GetSourcePath() == "game:Meshes/Humans/Male.glb",
		"source path is the import source and survives independently of the rig reference");
	// The pre-existing fields must still round-trip beside the new ones — an appended
	// block that shifted an existing read would show up here.
	ZENITH_ASSERT_EQ(xLoaded.GetTicksPerSecond(), 24u, "ticks-per-second is unchanged by the authored frame rate");
	ZENITH_ASSERT_TRUE(xLoaded.IsLooping(), "looping round-trips");
}

ZENITH_TEST(AnimationSerialization, ClipKeyTangentBlockRoundTrips)
{
	Flux_AnimationClip xClip;

	Flux_BoneChannel xChannel;
	xChannel.AddPositionKeyframe(0.0f,  Zenith_Maths::Vector3(0.0f));
	xChannel.AddPositionKeyframe(10.0f, Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
	xChannel.AddRotationKeyframe(0.0f,  Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f));
	xChannel.AddScaleKeyframe   (0.0f,  Zenith_Maths::Vector3(1.0f, 1.0f, 1.0f));

	// A tangent array is created in lockstep with its keys, at the zero default.
	ZENITH_ASSERT_EQ(xChannel.GetPositionTangents().GetSize(), 2u, "one tangent entry per position key");
	ZENITH_ASSERT_EQ(xChannel.GetRotationTangents().GetSize(), 1u, "one tangent entry per rotation key");
	ZENITH_ASSERT_EQ(xChannel.GetScaleTangents().GetSize(), 1u, "one tangent entry per scale key");
	ZENITH_ASSERT_EQ_FLOAT(xChannel.GetPositionTangents().Get(0).m_xInTangent.x, 0.0f, 1e-6f, "tangents default to zero");

	Flux_KeyTangents xPosTangent;
	xPosTangent.m_xInTangent  = Zenith_Maths::Vector3(0.25f, 0.5f, 0.75f);
	xPosTangent.m_xOutTangent = Zenith_Maths::Vector3(-1.0f, -2.0f, -3.0f);
	xChannel.SetPositionTangent(1u, xPosTangent);

	// A ROTATION tangent is an angular velocity (axis * rad/s), not a quaternion
	// control point — three components, and it lives in the same Flux_KeyTangents.
	Flux_KeyTangents xRotTangent;
	xRotTangent.m_xInTangent  = Zenith_Maths::Vector3(0.0f, 1.5f, 0.0f);
	xRotTangent.m_xOutTangent = Zenith_Maths::Vector3(0.0f, -1.5f, 0.0f);
	xChannel.SetRotationTangent(0u, xRotTangent);

	xClip.AddBoneChannel("Spine", std::move(xChannel));

	Zenith_DataStream xStream;
	xClip.WriteToDataStream(xStream);
	xStream.SetCursor(0);

	Flux_AnimationClip xLoaded;
	xLoaded.ReadFromDataStream(xStream);
	const Flux_BoneChannel* pxLoaded = xLoaded.GetBoneChannel("Spine");
	ZENITH_ASSERT_TRUE(pxLoaded != nullptr, "Spine channel resolves after the round-trip");
	if (pxLoaded == nullptr)
	{
		return;
	}
	ZENITH_ASSERT_EQ(pxLoaded->GetPositionTangents().GetSize(), 2u, "position tangent block round-trips its length");
	ZENITH_ASSERT_EQ_FLOAT(pxLoaded->GetPositionTangents().Get(1).m_xInTangent.y,  0.5f,  1e-6f, "position in-tangent round-trips");
	ZENITH_ASSERT_EQ_FLOAT(pxLoaded->GetPositionTangents().Get(1).m_xOutTangent.z, -3.0f, 1e-6f, "position out-tangent round-trips");
	ZENITH_ASSERT_EQ_FLOAT(pxLoaded->GetPositionTangents().Get(0).m_xInTangent.x,  0.0f,  1e-6f, "an unset tangent round-trips as zero");
	ZENITH_ASSERT_EQ_FLOAT(pxLoaded->GetRotationTangents().Get(0).m_xInTangent.y,  1.5f,  1e-6f, "rotation angular-velocity in-tangent round-trips");
	ZENITH_ASSERT_EQ_FLOAT(pxLoaded->GetRotationTangents().Get(0).m_xOutTangent.y, -1.5f, 1e-6f, "rotation angular-velocity out-tangent round-trips");
	ZENITH_ASSERT_EQ(pxLoaded->GetScaleTangents().GetSize(), 1u, "scale tangent block round-trips its length");

	// Reserved means reserved: sampling is untouched by the tangents above.
	ZENITH_ASSERT_TRUE(RootMotionVec3Equals(pxLoaded->SamplePosition(5.0f), Zenith_Maths::Vector3(0.5f, 0.0f, 0.0f)),
		"sampling is still linear — the tangent block is reserved, not consumed");
}

// ★ The determinism pair. Channels used to be written by walking the
// Zenith_HashMap, so the on-disk order was bucket order and a clip assembled by a
// different insertion sequence could serialize to different bytes for identical
// animation data. Two checks, because either alone is weak: the byte comparison
// alone can pass by luck when no two names happen to collide, and the ordering
// check alone does not prove the whole payload is stable.
ZENITH_TEST(AnimationSerialization, ClipChannelsAreWrittenInBoneNameOrder)
{
	Flux_AnimationClip xClip;
	// Deliberately inserted in reverse-alphabetical order.
	const char* aszInsertionOrder[4] = { "Zeta", "Mid", "Beta", "Alpha" };
	for (u_int u = 0; u < 4u; ++u)
	{
		Flux_BoneChannel xChannel;
		xChannel.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(static_cast<float>(u), 0.0f, 0.0f));
		xClip.AddBoneChannel(aszInsertionOrder[u], std::move(xChannel));
	}

	Zenith_DataStream xStream;
	xClip.WriteToDataStream(xStream);
	xStream.SetCursor(0);

	// Walk the payload by hand so the ON-DISK order is what is inspected — reading it
	// back into a clip would put the channels straight into a hash map again and lose
	// exactly the property under test.
	Zenith_Result<Zenith_StreamHeader> xHdr = Zenith_ReadStreamHeader(xStream, uZENITH_ANIMATION_ASSET_TYPE_ID);
	ZENITH_ASSERT_TRUE(xHdr.IsOk(), "envelope reads back");
	Flux_AnimationClipMetadata xMeta;
	xMeta.ReadFromDataStream(xStream);
	std::string strSourcePath;
	xStream >> strSourcePath;
	uint32_t uNumChannels = 0;
	xStream >> uNumChannels;
	ZENITH_ASSERT_EQ(uNumChannels, 4u, "all four channels are written");

	const char* aszExpectedOnDisk[4] = { "Alpha", "Beta", "Mid", "Zeta" };
	for (u_int u = 0; u < uNumChannels && u < 4u; ++u)
	{
		Flux_BoneChannel xChannel;
		xChannel.ReadFromDataStream(xStream);
		ZENITH_ASSERT_TRUE(xChannel.GetBoneName() == aszExpectedOnDisk[u],
			"channel %u on disk must be '%s', got '%s'", u, aszExpectedOnDisk[u], xChannel.GetBoneName().c_str());
	}
}

ZENITH_TEST(AnimationSerialization, ClipBytesAreIndependentOfInsertionOrder)
{
	const char* aszForward[4] = { "Alpha", "Beta", "Mid", "Zeta" };
	const char* aszReverse[4] = { "Zeta", "Mid", "Beta", "Alpha" };

	Zenith_DataStream axStreams[2];
	for (u_int uPass = 0; uPass < 2u; ++uPass)
	{
		const char* const* aszOrder = (uPass == 0) ? aszForward : aszReverse;

		Flux_AnimationClip xClip;
		xClip.SetName("OrderProbe");
		xClip.SetDuration(1.0f);
		xClip.GetMetadata().m_uAuthoredFrameRate = 30;
		for (u_int u = 0; u < 4u; ++u)
		{
			const char* szName = aszOrder[u];
			// ★ Every keyframe value is derived from the bone NAME, never from the
			// insertion index — otherwise the two passes would carry genuinely
			// different per-bone data and the byte comparison would be testing that
			// instead of the ordering.
			const float fSeed = static_cast<float>(szName[0]);

			Flux_BoneChannel xChannel;
			xChannel.AddPositionKeyframe(0.0f,  Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
			xChannel.AddPositionKeyframe(10.0f, Zenith_Maths::Vector3(fSeed, fSeed * 0.5f, 0.0f));
			xChannel.AddRotationKeyframe(0.0f,  Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f));
			xClip.AddBoneChannel(szName, std::move(xChannel));
		}

		xClip.WriteToDataStream(axStreams[uPass]);
	}

	ZENITH_ASSERT_EQ(axStreams[0].GetCursor(), axStreams[1].GetCursor(),
		"two insertion orders of the same clip must serialize to the same LENGTH");
	if (axStreams[0].GetCursor() == axStreams[1].GetCursor())
	{
		const int iDiff = std::memcmp(axStreams[0].GetData(), axStreams[1].GetData(),
			static_cast<size_t>(axStreams[0].GetCursor()));
		ZENITH_ASSERT_EQ(iDiff, 0, "two insertion orders of the same clip must serialize BYTE-IDENTICALLY");
	}
}

// ============================================================================
// WU-1.2 — KEY TIMES ARE SECONDS (D3).
//
// The change these pin is invisible to every structural check: a clip whose key
// times are still on a tick grid has the same channel count, the same bone names,
// the same quaternions, the same byte length and the same serialized order. What
// moves is WHERE IN TIME each pose lands — by a factor of the clip's
// ticks-per-second, which for the whole StickFigure set is 24x.
//
// All pure CPU: an in-memory clip, an in-memory 1-bone skeleton, no device, no
// registry, no file. None is requiresGraphics, so all of them actually run under
// the Null backend rather than being skipped-as-passed.
// ============================================================================

namespace
{
	// A one-bone skeleton at the origin — the least a Flux_SkeletonPose needs in
	// order to resolve a channel by bone name. Caller owns it.
	Zenith_SkeletonAsset* ClipMakeOneBoneSkeleton(const char* szBoneName)
	{
		Zenith_SkeletonAsset* pxSkeleton = new Zenith_SkeletonAsset();
		pxSkeleton->AddBone(szBoneName, -1,
			Zenith_Maths::Vector3(0.0f),
			Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f),
			Zenith_Maths::Vector3(1.0f));
		pxSkeleton->ComputeBindPoseMatrices();
		return pxSkeleton;
	}

	// A single-channel clip whose position track runs (0,0,0) -> (10,0,0) over
	// fDurationSeconds, with one interior key at the midpoint. uTicksPerSecond is
	// stamped as provenance and must not affect anything.
	void ClipBuildRampClip(Flux_AnimationClip& xClip, const char* szBoneName,
		float fDurationSeconds, uint32_t uTicksPerSecond)
	{
		xClip.SetName("Ramp");
		xClip.SetDuration(fDurationSeconds);
		xClip.SetTicksPerSecond(uTicksPerSecond);

		Flux_BoneChannel xChannel;
		xChannel.AddPositionKeyframe(0.0f,                    Zenith_Maths::Vector3(0.0f,  0.0f, 0.0f));
		xChannel.AddPositionKeyframe(fDurationSeconds * 0.5f, Zenith_Maths::Vector3(5.0f,  0.0f, 0.0f));
		xChannel.AddPositionKeyframe(fDurationSeconds,        Zenith_Maths::Vector3(10.0f, 0.0f, 0.0f));
		xChannel.SortKeyframes();
		xClip.AddBoneChannel(szBoneName, std::move(xChannel));
	}
}

// ★ THE ACCEPTANCE CHECK FOR "THE SAMPLER NO LONGER MULTIPLIES BY TICKS-PER-SECOND".
// A clip declaring 1000 ticks per second with a key at t=1.0 s must return THAT key
// at fTime=1.0. Under the old sampler this asked the channel for t=1000, which
// clamped to the last keyframe — so the pose was wrong by the whole clip.
//
// 1000 is chosen deliberately over 24: a 24x error is a plausible-looking pose on a
// looping clip, while 1000x can only be the clamp.
ZENITH_TEST(AnimationTime, SamplerIgnoresTicksPerSecond)
{
	Zenith_SkeletonAsset* pxSkeleton = ClipMakeOneBoneSkeleton("Root");

	Flux_AnimationClip xClip;
	ClipBuildRampClip(xClip, "Root", 2.0f, 1000u);

	Flux_SkeletonPose xPose;
	xPose.Initialize(1u);
	xPose.SampleFromClip(xClip, 1.0f, *pxSkeleton);

	// t=1.0 s is the clip's midpoint key: exactly (5,0,0).
	ZENITH_ASSERT_TRUE(RootMotionVec3Equals(xPose.GetLocalPose(0u).m_xPosition, Zenith_Maths::Vector3(5.0f, 0.0f, 0.0f), 1e-4f),
		"a clip with 1000 ticks-per-second must still sample its t=1.0s key at fTime=1.0 — got (%f, %f, %f)",
		xPose.GetLocalPose(0u).m_xPosition.x, xPose.GetLocalPose(0u).m_xPosition.y, xPose.GetLocalPose(0u).m_xPosition.z);

	// And the field is INERT, not merely harmless at one value: the same clip with
	// the default 24 must produce the identical pose at the identical wall-clock time.
	Flux_AnimationClip xClip24;
	ClipBuildRampClip(xClip24, "Root", 2.0f, 24u);
	Flux_SkeletonPose xPose24;
	xPose24.Initialize(1u);
	xPose24.SampleFromClip(xClip24, 1.0f, *pxSkeleton);
	ZENITH_ASSERT_TRUE(RootMotionVec3Equals(xPose.GetLocalPose(0u).m_xPosition, xPose24.GetLocalPose(0u).m_xPosition, 1e-6f),
		"ticks-per-second must be provenance only — two clips differing ONLY in it must pose identically");

	delete pxSkeleton;
}

// ★ THE IMPORT-vs-AUTHORED EQUIVALENCE. One clip built the way the Assimp import
// now builds one (source key times are ticks on a 24-per-second grid, DIVIDED by
// that grid on the way in) and one authored directly in seconds must be
// indistinguishable at every matched wall-clock time.
//
// This is written as a SWEEP rather than a spot check on purpose: a single sample
// at a keyframe passes even when the two clips disagree everywhere between keys,
// which is exactly the shape a half-applied conversion has.
ZENITH_TEST(AnimationTime, TickImportedClipMatchesSecondsAuthoredClip)
{
	constexpr float fSOURCE_TICKS_PER_SECOND = 24.0f;

	// The source file's key times, in TICKS, and the values at them.
	const float afSourceTicks[4]              = { 0.0f, 6.0f, 18.0f, 24.0f };
	const Zenith_Maths::Vector3 axValues[4] =
	{
		Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f),
		Zenith_Maths::Vector3(1.0f, 2.0f, 0.0f),
		Zenith_Maths::Vector3(-3.0f, 0.5f, 4.0f),
		Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f),
	};

	// (a) The import path: divide each source tick by the source's tick rate. This is
	// literally what Flux_BoneChannel(const aiNodeAnim*, double) now does per key.
	Flux_BoneChannel xImported;
	for (u_int u = 0; u < 4u; ++u)
	{
		xImported.AddPositionKeyframe(afSourceTicks[u] / fSOURCE_TICKS_PER_SECOND, axValues[u]);
	}
	xImported.SortKeyframes();

	// (b) The generator path: the same instants stated in seconds.
	const float afSeconds[4] = { 0.0f, 0.25f, 0.75f, 1.0f };
	Flux_BoneChannel xAuthored;
	for (u_int u = 0; u < 4u; ++u)
	{
		xAuthored.AddPositionKeyframe(afSeconds[u], axValues[u]);
	}
	xAuthored.SortKeyframes();

	// Sweep the whole 1-second span plus a little past the end (the clamp branch).
	constexpr u_int uSAMPLES = 41u;
	for (u_int u = 0; u < uSAMPLES; ++u)
	{
		const float fTimeSeconds = 1.2f * static_cast<float>(u) / static_cast<float>(uSAMPLES - 1u);
		const Zenith_Maths::Vector3 xA = xImported.SamplePosition(fTimeSeconds);
		const Zenith_Maths::Vector3 xB = xAuthored.SamplePosition(fTimeSeconds);
		ZENITH_ASSERT_TRUE(RootMotionVec3Equals(xA, xB, 1e-5f),
			"import-converted and seconds-authored clips diverge at t=%f s: (%f,%f,%f) vs (%f,%f,%f)",
			fTimeSeconds, xA.x, xA.y, xA.z, xB.x, xB.y, xB.z);
	}
}

// The key-time/duration agreement helper itself. Worth its own test because it is
// the check ZM_ValidateCreatureClip (and any future generator) leans on, and a
// vacuously-true predicate would make every one of those green for free.
ZENITH_TEST(AnimationTime, ClipKeyTimesFitDurationCatchesATickGrid)
{
	// (a) A well-formed seconds clip: last key exactly on the duration.
	{
		Flux_AnimationClip xClip;
		ClipBuildRampClip(xClip, "Root", 2.0f, 24u);
		ZENITH_ASSERT_TRUE(Flux_ClipKeyTimesFitDuration(xClip), "a seconds-authored clip must fit its duration");
		ZENITH_ASSERT_EQ_FLOAT(Flux_ClipLastKeyTimeSeconds(xClip), 2.0f, 1e-6f, "last key time is the duration");
	}

	// (b) The SAME clip with its key times left on the 24-per-second tick grid —
	// the exact relapse this WU is about. Every other property is unchanged.
	{
		Flux_AnimationClip xClip;
		xClip.SetDuration(2.0f);
		xClip.SetTicksPerSecond(24);
		Flux_BoneChannel xChannel;
		xChannel.AddPositionKeyframe(0.0f,  Zenith_Maths::Vector3(0.0f));
		xChannel.AddPositionKeyframe(24.0f, Zenith_Maths::Vector3(5.0f, 0.0f, 0.0f));
		xChannel.AddPositionKeyframe(48.0f, Zenith_Maths::Vector3(10.0f, 0.0f, 0.0f));
		xClip.AddBoneChannel("Root", std::move(xChannel));
		ZENITH_ASSERT_FALSE(Flux_ClipKeyTimesFitDuration(xClip),
			"a clip whose keys are still ticks must NOT fit a 2-second duration");
		ZENITH_ASSERT_EQ_FLOAT(Flux_ClipLastKeyTimeSeconds(xClip), 48.0f, 1e-6f,
			"the helper reports the offending time so the failure names itself");
	}

	// (c) A NEGATIVE key time — what a sign slip in a conversion produces, and what a
	// last-key-only check would wave through.
	{
		Flux_AnimationClip xClip;
		xClip.SetDuration(1.0f);
		Flux_BoneChannel xChannel;
		xChannel.AddPositionKeyframe(-0.5f, Zenith_Maths::Vector3(0.0f));
		xChannel.AddPositionKeyframe(1.0f,  Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
		xClip.AddBoneChannel("Root", std::move(xChannel));
		ZENITH_ASSERT_FALSE(Flux_ClipKeyTimesFitDuration(xClip), "a negative key time must be refused");
	}

	// (d) An empty clip is vacuously fine, and reports zero rather than garbage.
	{
		Flux_AnimationClip xClip;
		xClip.SetDuration(1.0f);
		ZENITH_ASSERT_TRUE(Flux_ClipKeyTimesFitDuration(xClip), "a clip with no channels is vacuously in range");
		ZENITH_ASSERT_EQ_FLOAT(Flux_ClipLastKeyTimeSeconds(xClip), 0.0f, 1e-6f, "no keys means no last key time");
	}
}

// GetLastKeyTimeSeconds takes the MAXIMUM across the three arrays and does not
// assume sorted input — a generator calls it before SortKeyframes, and a
// back-of-array read would then report an interior key and hide an overrun.
ZENITH_TEST(AnimationTime, ChannelLastKeyTimeIsAMaximumNotTheBack)
{
	Flux_BoneChannel xChannel;
	xChannel.AddPositionKeyframe(0.9f, Zenith_Maths::Vector3(0.0f));
	xChannel.AddPositionKeyframe(0.1f, Zenith_Maths::Vector3(0.0f));   // deliberately out of order
	xChannel.AddRotationKeyframe(0.4f, Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f));
	xChannel.AddScaleKeyframe   (1.7f, Zenith_Maths::Vector3(1.0f));   // the real latest, on a THIRD array

	ZENITH_ASSERT_EQ_FLOAT(xChannel.GetLastKeyTimeSeconds(), 1.7f, 1e-6f,
		"the last key time must be the max across all three arrays, unsorted input included");
}

// ============================================================================
// WU-1.3 — CHANNEL AND ROOT-MOTION MUTATORS (D9..D16).
//
// The clip used to be APPEND-ONLY: Add*Keyframe + SortKeyframes, three private
// vectors, const-only getters, and `friend class Flux_AnimationClip` as the sole
// escape hatch. Nothing could remove, retime or revalue a key. These pin the
// verbs that changed that, and — as much as the happy paths — the REFUSALS,
// because every one of the policy decisions here is a decision NOT to silently
// repair something:
//
//   • not to clamp a bad time to zero (a clamped key is indistinguishable from an
//     authored one),
//   • not to merge two keys on a retime (the merge happens mid-drag, when a user
//     is least able to see a key vanish),
//   • not to normalize a zero quaternion (that is how a NaN reaches every pose),
//   • not to leave an emptied channel in a clip (the two samplers disagree about
//     what it means — see ClipRemovingTheLastKeyRemovesTheChannel).
//
// ★ EVERY TEST BELOW RE-CHECKS THE TANGENT LOCKSTEP after every mutation. The
// reserved tangent arrays (D17) are parallel to the key arrays by index, so a
// mutator that moves a key without its tangent does not lose data — it silently
// RE-PAIRS every later key with the wrong tangent. Nothing that counts keys, and
// nothing that samples (the tangents are not sampled yet), can see that.
//
// All pure CPU — in-memory channels and clips, no device, no registry, no file —
// so every one runs under the Null backend and none is requiresGraphics.
// ============================================================================

namespace
{
	void MutAssertTangentsParallel(const Flux_BoneChannel& xChannel, const char* szWhere)
	{
		ZENITH_ASSERT_EQ(xChannel.GetPositionTangents().GetSize(), xChannel.GetPositionKeyframes().GetSize(),
			"%s: position tangents must stay parallel to the position keys", szWhere);
		ZENITH_ASSERT_EQ(xChannel.GetRotationTangents().GetSize(), xChannel.GetRotationKeyframes().GetSize(),
			"%s: rotation tangents must stay parallel to the rotation keys", szWhere);
		ZENITH_ASSERT_EQ(xChannel.GetScaleTangents().GetSize(), xChannel.GetScaleKeyframes().GetSize(),
			"%s: scale tangents must stay parallel to the scale keys", szWhere);
	}

	// Position keys at 0 / 1 / 2 s (value.x == the time), one rotation key and one
	// scale key, already sorted. Each POSITION tangent is stamped with a marker
	// derived from its ORIGINAL key index — 100+i in, 200+i out — so a test can say
	// WHICH tangent ended up where, rather than only that the arrays are the right
	// length. A length check alone passes a mutator that shuffles tangents.
	void MutBuildProbeChannel(Flux_BoneChannel& xChannel)
	{
		xChannel.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
		xChannel.AddPositionKeyframe(1.0f, Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
		xChannel.AddPositionKeyframe(2.0f, Zenith_Maths::Vector3(2.0f, 0.0f, 0.0f));
		xChannel.AddRotationKeyframe(0.0f, Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f));
		xChannel.AddScaleKeyframe   (0.0f, Zenith_Maths::Vector3(1.0f, 1.0f, 1.0f));
		xChannel.SortKeyframes();

		for (u_int u = 0; u < 3u; ++u)
		{
			Flux_KeyTangents xTangent;
			xTangent.m_xInTangent  = Zenith_Maths::Vector3(100.0f + static_cast<float>(u), 0.0f, 0.0f);
			xTangent.m_xOutTangent = Zenith_Maths::Vector3(200.0f + static_cast<float>(u), 0.0f, 0.0f);
			xChannel.SetPositionTangent(u, xTangent);
		}
	}

	// The stamp MutBuildProbeChannel wrote, read back from whichever slot it now
	// occupies. 100 + original index, or 0 for a tangent the mutator created fresh.
	float MutTangentMarker(const Flux_BoneChannel& xChannel, u_int uKeyIndex)
	{
		return xChannel.GetPositionTangents().Get(uKeyIndex).m_xInTangent.x;
	}

	// A track is sorted iff every adjacent pair is non-decreasing. Checked directly
	// rather than by spot-testing two indices, because an insert that lands in the
	// wrong place can still leave any single pair looking right.
	bool MutPositionTrackIsSorted(const Flux_BoneChannel& xChannel)
	{
		for (u_int u = 1; u < xChannel.GetPositionKeyframes().GetSize(); ++u)
		{
			if (xChannel.GetPositionKeyframes().Get(u - 1u).second > xChannel.GetPositionKeyframes().Get(u).second)
			{
				return false;
			}
		}
		return true;
	}
}

// ★ REPLACE, NOT A SECOND KEY. The incoming time is deliberately NOT bit-identical
// to the stored 1.0 — it is one quarter of an epsilon away, which is the shape a
// snap-to-frame, a UI round-trip or a serialization produces. An `==` compare (D9's
// forbidden shape) would call the slot free and insert a second key a microsecond
// from the first, which samples as a near-vertical step in the curve.
ZENITH_TEST(AnimationMutation, ChannelInsertOnOccupiedTimeReplacesInPlace)
{
	Flux_BoneChannel xChannel;
	MutBuildProbeChannel(xChannel);

	const float fNearlyOne = 1.0f + (fANIM_TIME_EPSILON * 0.25f);
	u_int uIndex = 0xFFFFFFFFu;
	const bool bOk = xChannel.InsertKeyframeAt(FLUX_ANIM_TRACK_POSITION, fNearlyOne,
		Zenith_Maths::Vector3(9.0f, 9.0f, 9.0f), &uIndex);

	ZENITH_ASSERT_TRUE(bOk, "an insert onto an occupied time must succeed, as a replace");
	ZENITH_ASSERT_EQ(uIndex, 1u, "the replaced key keeps its index SLOT — an undo record naming (bone, track, 1) still names it");
	ZENITH_ASSERT_EQ(xChannel.GetKeyframeCount(FLUX_ANIM_TRACK_POSITION), 3u, "a replace must not add a key");
	ZENITH_ASSERT_TRUE(RootMotionVec3Equals(xChannel.GetPositionKeyframes().Get(1).first, Zenith_Maths::Vector3(9.0f, 9.0f, 9.0f)),
		"the occupied key's VALUE is replaced");

	// The stored time is left exactly as authored. Rewriting it with fNearlyOne would
	// nudge a snapped key by up to an epsilon on every re-drop — invisible per edit,
	// cumulative across a session.
	ZENITH_ASSERT_EQ_FLOAT(xChannel.GetPositionKeyframes().Get(1).second, 1.0f, 1e-9f,
		"a replace must not nudge the key's authored time toward the incoming one");
	ZENITH_ASSERT_EQ_FLOAT(MutTangentMarker(xChannel, 1u), 101.0f, 1e-6f,
		"a replace keeps the EXISTING key's tangent — re-dropping a value must not discard curve authoring");
	ZENITH_ASSERT_TRUE(MutPositionTrackIsSorted(xChannel), "the track stays sorted");
	MutAssertTangentsParallel(xChannel, "insert-on-occupied");
}

ZENITH_TEST(AnimationMutation, ChannelInsertOnFreeTimeLandsSorted)
{
	Flux_BoneChannel xChannel;
	MutBuildProbeChannel(xChannel);

	// Interior: between the keys at 0 and 1.
	u_int uIndex = 0xFFFFFFFFu;
	ZENITH_ASSERT_TRUE(xChannel.InsertKeyframeAt(FLUX_ANIM_TRACK_POSITION, 0.5f, Zenith_Maths::Vector3(5.0f, 0.0f, 0.0f), &uIndex),
		"an insert on a free time must succeed");
	ZENITH_ASSERT_EQ(uIndex, 1u, "the new key lands at its SORTED position, not at the back");
	ZENITH_ASSERT_EQ(xChannel.GetKeyframeCount(FLUX_ANIM_TRACK_POSITION), 4u, "the key count grows by one");
	ZENITH_ASSERT_TRUE(MutPositionTrackIsSorted(xChannel), "the track is sorted with no SortKeyframes() call");
	ZENITH_ASSERT_EQ_FLOAT(xChannel.GetPositionKeyframes().Get(1).second, 0.5f, 1e-6f, "the new key is at the requested time");

	// The three original tangents shifted right with their own keys, and the new key
	// got a FRESH zero tangent rather than inheriting its neighbour's.
	ZENITH_ASSERT_EQ_FLOAT(MutTangentMarker(xChannel, 0u), 100.0f, 1e-6f, "the t=0 key keeps its tangent");
	ZENITH_ASSERT_EQ_FLOAT(MutTangentMarker(xChannel, 1u), 0.0f,   1e-6f, "the inserted key gets a zero tangent");
	ZENITH_ASSERT_EQ_FLOAT(MutTangentMarker(xChannel, 2u), 101.0f, 1e-6f, "the t=1 key's tangent moved WITH it");
	ZENITH_ASSERT_EQ_FLOAT(MutTangentMarker(xChannel, 3u), 102.0f, 1e-6f, "the t=2 key's tangent moved WITH it");
	MutAssertTangentsParallel(xChannel, "insert-interior");

	// Past the end: the other boundary of the sorted-position search.
	ZENITH_ASSERT_TRUE(xChannel.InsertKeyframeAt(FLUX_ANIM_TRACK_POSITION, 7.0f, Zenith_Maths::Vector3(7.0f, 0.0f, 0.0f), &uIndex),
		"an insert past the last key must succeed");
	ZENITH_ASSERT_EQ(uIndex, 4u, "a key later than every other lands at the back");
	ZENITH_ASSERT_TRUE(MutPositionTrackIsSorted(xChannel), "the track stays sorted");

	// And at the very front.
	ZENITH_ASSERT_TRUE(xChannel.InsertKeyframeAt(FLUX_ANIM_TRACK_POSITION, 0.0f + fANIM_TIME_EPSILON * 4.0f, Zenith_Maths::Vector3(-1.0f, 0.0f, 0.0f), &uIndex),
		"an insert just clear of the first key must succeed");
	ZENITH_ASSERT_EQ(uIndex, 1u, "a time just past t=0 — outside the epsilon — is a NEW key, not a replace");
	ZENITH_ASSERT_EQ(xChannel.GetKeyframeCount(FLUX_ANIM_TRACK_POSITION), 6u, "the key count grew again");
	ZENITH_ASSERT_TRUE(MutPositionTrackIsSorted(xChannel), "the track stays sorted");
	MutAssertTangentsParallel(xChannel, "insert-front-and-back");
}

// ★ NO SILENT MERGE (D11/D25). A drag that lands on an occupied frame is ordinary
// user input, so this is a plain `false` and NOT an assert — the caller's job is to
// reject the drag, not to have been prevented from attempting it. What must not
// happen is either key changing.
ZENITH_TEST(AnimationMutation, ChannelSetKeyframeTimeOntoOccupiedIsRefused)
{
	Flux_BoneChannel xChannel;
	MutBuildProbeChannel(xChannel);

	u_int uIndex = 0xFFFFFFFFu;
	const bool bOk = xChannel.SetKeyframeTime(FLUX_ANIM_TRACK_POSITION, 0u,
		2.0f - (fANIM_TIME_EPSILON * 0.5f), &uIndex);

	ZENITH_ASSERT_FALSE(bOk, "retiming onto an occupied time must FAIL, not merge and not overwrite");
	ZENITH_ASSERT_EQ(uIndex, 0xFFFFFFFFu, "a refused retime writes no out-index");
	ZENITH_ASSERT_EQ(xChannel.GetKeyframeCount(FLUX_ANIM_TRACK_POSITION), 3u, "a refused retime destroys no key");

	// BOTH keys — the dragged one and the one it landed on — are untouched, in time
	// and in value. A merge would have left two keys and the right count.
	ZENITH_ASSERT_EQ_FLOAT(xChannel.GetPositionKeyframes().Get(0).second, 0.0f, 1e-9f, "the dragged key keeps its time");
	ZENITH_ASSERT_EQ_FLOAT(xChannel.GetPositionKeyframes().Get(2).second, 2.0f, 1e-9f, "the target key keeps its time");
	ZENITH_ASSERT_TRUE(RootMotionVec3Equals(xChannel.GetPositionKeyframes().Get(0).first, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f)),
		"the dragged key keeps its value");
	ZENITH_ASSERT_TRUE(RootMotionVec3Equals(xChannel.GetPositionKeyframes().Get(2).first, Zenith_Maths::Vector3(2.0f, 0.0f, 0.0f)),
		"the target key keeps its value");
	ZENITH_ASSERT_EQ_FLOAT(MutTangentMarker(xChannel, 0u), 100.0f, 1e-6f, "tangents are untouched by a refusal");
	ZENITH_ASSERT_EQ_FLOAT(MutTangentMarker(xChannel, 2u), 102.0f, 1e-6f, "tangents are untouched by a refusal");
	MutAssertTangentsParallel(xChannel, "set-time-refused");
}

ZENITH_TEST(AnimationMutation, ChannelSetKeyframeTimeResortsAndCarriesTangent)
{
	Flux_BoneChannel xChannel;
	MutBuildProbeChannel(xChannel);

	// Drag the FIRST key (t=0, value (0,0,0), tangent marker 100) past the second.
	u_int uIndex = 0xFFFFFFFFu;
	ZENITH_ASSERT_TRUE(xChannel.SetKeyframeTime(FLUX_ANIM_TRACK_POSITION, 0u, 1.5f, &uIndex),
		"retiming to a free time must succeed");
	ZENITH_ASSERT_EQ(uIndex, 1u, "the retimed key's NEW index comes back — the undo record has to be able to find it again");
	ZENITH_ASSERT_EQ(xChannel.GetKeyframeCount(FLUX_ANIM_TRACK_POSITION), 3u, "a retime moves a key, it does not add or drop one");
	ZENITH_ASSERT_TRUE(MutPositionTrackIsSorted(xChannel), "the track is re-sorted with no SortKeyframes() call");

	ZENITH_ASSERT_EQ_FLOAT(xChannel.GetPositionKeyframes().Get(0).second, 1.0f, 1e-6f, "the untouched key is now first");
	ZENITH_ASSERT_EQ_FLOAT(xChannel.GetPositionKeyframes().Get(1).second, 1.5f, 1e-6f, "the moved key sits at its new time");
	ZENITH_ASSERT_TRUE(RootMotionVec3Equals(xChannel.GetPositionKeyframes().Get(1).first, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f)),
		"a retime moves the key's VALUE with it — only the time changes");

	// ★ The payload of this test. The tangent must travel with its key, not stay at
	// the index. Marker 100 belongs to the moved key and must now be at index 1.
	ZENITH_ASSERT_EQ_FLOAT(MutTangentMarker(xChannel, 0u), 101.0f, 1e-6f, "the t=1 key's tangent is now first");
	ZENITH_ASSERT_EQ_FLOAT(MutTangentMarker(xChannel, 1u), 100.0f, 1e-6f, "the MOVED key's tangent moved with it");
	ZENITH_ASSERT_EQ_FLOAT(MutTangentMarker(xChannel, 2u), 102.0f, 1e-6f, "the t=2 key's tangent is unchanged");
	MutAssertTangentsParallel(xChannel, "set-time-resort");
}

// D10: a key time must be FINITE and NON-NEGATIVE, and a bad one is REFUSED rather
// than clamped. NaN is checked separately from +inf because `t < 0` catches neither
// and `!(t >= 0)` catches only NaN — a validator written either of those two ways
// alone lets one of them through.
ZENITH_TEST(AnimationMutation, ChannelMutatorsRejectNonFiniteAndNegativeTimes)
{
	Flux_BoneChannel xChannel;
	MutBuildProbeChannel(xChannel);

	const float fNaN = std::numeric_limits<float>::quiet_NaN();
	const float fInf = std::numeric_limits<float>::infinity();

	{
		Zenith_AssertCaptureScope xCapture;

		ZENITH_ASSERT_FALSE(xChannel.InsertKeyframeAt(FLUX_ANIM_TRACK_POSITION, fNaN, Zenith_Maths::Vector3(1.0f)),
			"a NaN insert time must be refused");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "a NaN key time asserts exactly once");
		xCapture.ResetHitCount();

		ZENITH_ASSERT_FALSE(xChannel.InsertKeyframeAt(FLUX_ANIM_TRACK_POSITION, fInf, Zenith_Maths::Vector3(1.0f)),
			"a +inf insert time must be refused");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "an infinite key time asserts exactly once");
		xCapture.ResetHitCount();

		ZENITH_ASSERT_FALSE(xChannel.InsertKeyframeAt(FLUX_ANIM_TRACK_POSITION, -0.25f, Zenith_Maths::Vector3(1.0f)),
			"a negative insert time must be refused, NOT clamped to zero");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "a negative key time asserts exactly once");
		xCapture.ResetHitCount();

		ZENITH_ASSERT_FALSE(xChannel.SetKeyframeTime(FLUX_ANIM_TRACK_POSITION, 1u, fNaN), "a NaN retime must be refused");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "a NaN retime asserts exactly once");
		xCapture.ResetHitCount();

		ZENITH_ASSERT_FALSE(xChannel.SetKeyframeTime(FLUX_ANIM_TRACK_POSITION, 1u, -fInf), "a -inf retime must be refused");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "a -inf retime asserts exactly once");
		xCapture.ResetHitCount();

		ZENITH_ASSERT_FALSE(xChannel.SetKeyframeTime(FLUX_ANIM_TRACK_POSITION, 1u, -3.0f), "a negative retime must be refused");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "a negative retime asserts exactly once");
	}

	// Nothing moved. Every refusal above is total — no partial edit, no clamped key.
	ZENITH_ASSERT_EQ(xChannel.GetKeyframeCount(FLUX_ANIM_TRACK_POSITION), 3u, "a refused time changes no key count");
	ZENITH_ASSERT_EQ_FLOAT(xChannel.GetPositionKeyframes().Get(0).second, 0.0f, 1e-9f, "no key was clamped to t=0");
	ZENITH_ASSERT_EQ_FLOAT(xChannel.GetPositionKeyframes().Get(1).second, 1.0f, 1e-9f, "the targeted key keeps its time");
	ZENITH_ASSERT_EQ_FLOAT(xChannel.GetPositionKeyframes().Get(2).second, 2.0f, 1e-9f, "no key moved");
	MutAssertTangentsParallel(xChannel, "rejected-times");
}

ZENITH_TEST(AnimationMutation, ChannelRemoveKeyframeKeepsTangentsParallel)
{
	Flux_BoneChannel xChannel;
	MutBuildProbeChannel(xChannel);

	// Remove the MIDDLE key: the case that re-pairs every later key with the wrong
	// tangent if the tangent array is not cut at the same index.
	ZENITH_ASSERT_TRUE(xChannel.RemoveKeyframe(FLUX_ANIM_TRACK_POSITION, 1u), "removing an in-range key must succeed");
	ZENITH_ASSERT_EQ(xChannel.GetKeyframeCount(FLUX_ANIM_TRACK_POSITION), 2u, "the key count drops by one");
	ZENITH_ASSERT_EQ_FLOAT(xChannel.GetPositionKeyframes().Get(0).second, 0.0f, 1e-9f, "the earlier key is untouched");
	ZENITH_ASSERT_EQ_FLOAT(xChannel.GetPositionKeyframes().Get(1).second, 2.0f, 1e-9f, "the later key closed up");
	ZENITH_ASSERT_EQ_FLOAT(MutTangentMarker(xChannel, 0u), 100.0f, 1e-6f, "the earlier key keeps its own tangent");
	ZENITH_ASSERT_EQ_FLOAT(MutTangentMarker(xChannel, 1u), 102.0f, 1e-6f,
		"the later key keeps ITS OWN tangent — not the removed key's, which is what a key-only removal would leave");
	MutAssertTangentsParallel(xChannel, "remove-middle");

	{
		Zenith_AssertCaptureScope xCapture;
		ZENITH_ASSERT_FALSE(xChannel.RemoveKeyframe(FLUX_ANIM_TRACK_POSITION, 99u), "an out-of-range removal must be refused");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "an out-of-range key index asserts exactly once");
		xCapture.ResetHitCount();
		ZENITH_ASSERT_FALSE(xChannel.RemoveKeyframe(FLUX_ANIM_TRACK_ROTATION, 1u), "one-past-the-end is out of range too");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "one-past-the-end asserts exactly once");
	}
	ZENITH_ASSERT_EQ(xChannel.GetKeyframeCount(FLUX_ANIM_TRACK_POSITION), 2u, "a refused removal drops nothing");
	MutAssertTangentsParallel(xChannel, "remove-refused");

	// Emptying all three tracks is legal at CHANNEL level — the channel simply has no
	// keys. It is only forbidden INSIDE A CLIP (D14), which the clip tests cover.
	ZENITH_ASSERT_TRUE(xChannel.RemoveKeyframe(FLUX_ANIM_TRACK_POSITION, 0u), "removal succeeds");
	ZENITH_ASSERT_TRUE(xChannel.RemoveKeyframe(FLUX_ANIM_TRACK_POSITION, 0u), "removal succeeds");
	ZENITH_ASSERT_TRUE(xChannel.RemoveKeyframe(FLUX_ANIM_TRACK_ROTATION, 0u), "removal succeeds");
	ZENITH_ASSERT_TRUE(xChannel.RemoveKeyframe(FLUX_ANIM_TRACK_SCALE,    0u), "removal succeeds");
	ZENITH_ASSERT_TRUE(xChannel.IsEmpty(), "all three tracks are now empty");
	MutAssertTangentsParallel(xChannel, "remove-all");
}

// D15, the accepted half: a rotation value is NORMALIZED on write, on both the
// revalue and the insert path. A denormalized quaternion reaching the track would
// scale every pose the slerp produces.
ZENITH_TEST(AnimationMutation, ChannelRotationValueIsNormalizedOnWrite)
{
	Flux_BoneChannel xChannel;
	MutBuildProbeChannel(xChannel);

	const Zenith_Maths::Quat xUnit = glm::angleAxis(glm::radians(45.0f), Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
	const Zenith_Maths::Quat xLong(xUnit.w * 3.0f, xUnit.x * 3.0f, xUnit.y * 3.0f, xUnit.z * 3.0f);

	ZENITH_ASSERT_TRUE(xChannel.SetKeyframeValue(FLUX_ANIM_TRACK_ROTATION, 0u, xLong),
		"a denormalized (but non-degenerate) quaternion is accepted");
	ZENITH_ASSERT_EQ_FLOAT(glm::length(xChannel.GetRotationKeyframes().Get(0).first), 1.0f, 1e-5f,
		"the stored rotation key is unit length");
	ZENITH_ASSERT_TRUE(RootMotionQuatEquals(xChannel.GetRotationKeyframes().Get(0).first, xUnit, 1e-4f),
		"normalizing must not change the ROTATION, only the length");

	u_int uIndex = 0xFFFFFFFFu;
	ZENITH_ASSERT_TRUE(xChannel.InsertKeyframeAt(FLUX_ANIM_TRACK_ROTATION, 1.0f, xLong, &uIndex),
		"the insert path accepts it too");
	ZENITH_ASSERT_EQ(uIndex, 1u, "the new rotation key lands after the t=0 one");
	ZENITH_ASSERT_EQ_FLOAT(glm::length(xChannel.GetRotationKeyframes().Get(1).first), 1.0f, 1e-5f,
		"the INSERTED rotation key is normalized as well — both write paths, not just one");
	MutAssertTangentsParallel(xChannel, "rotation-normalize");
}

// D15, the refused half. glm::normalize of a zero-length quaternion is NaN, and one
// NaN rotation key poisons every pose the clip can produce at every time, through
// the slerp — so it is refused outright rather than "normalized".
ZENITH_TEST(AnimationMutation, ChannelRotationRejectsAZeroQuaternion)
{
	Flux_BoneChannel xChannel;
	MutBuildProbeChannel(xChannel);

	const Zenith_Maths::Quat xZero(0.0f, 0.0f, 0.0f, 0.0f);
	{
		Zenith_AssertCaptureScope xCapture;
		ZENITH_ASSERT_FALSE(xChannel.SetKeyframeValue(FLUX_ANIM_TRACK_ROTATION, 0u, xZero),
			"a zero quaternion must be refused, not normalized into NaN");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "a zero quaternion asserts exactly once");
		xCapture.ResetHitCount();

		ZENITH_ASSERT_FALSE(xChannel.InsertKeyframeAt(FLUX_ANIM_TRACK_ROTATION, 1.0f, xZero),
			"the insert path refuses it too");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "a zero quaternion asserts exactly once on insert");
	}

	ZENITH_ASSERT_EQ(xChannel.GetKeyframeCount(FLUX_ANIM_TRACK_ROTATION), 1u, "the refused insert added no key");
	// The existing key is untouched, and is still a real rotation.
	ZENITH_ASSERT_EQ_FLOAT(glm::length(xChannel.GetRotationKeyframes().Get(0).first), 1.0f, 1e-5f,
		"the existing rotation key survives the refusal intact");
	MutAssertTangentsParallel(xChannel, "zero-quaternion");
}

// The selector enum and the value overload have to AGREE. A Vector3 aimed at the
// rotation track (or a Quat at position/scale) is a caller bug that no type check
// can catch — the enum is a runtime value — so it is refused at runtime.
ZENITH_TEST(AnimationMutation, ChannelTrackSelectorRefusesAMismatchedValueType)
{
	Flux_BoneChannel xChannel;
	MutBuildProbeChannel(xChannel);

	{
		Zenith_AssertCaptureScope xCapture;
		ZENITH_ASSERT_FALSE(xChannel.SetKeyframeValue(FLUX_ANIM_TRACK_ROTATION, 0u, Zenith_Maths::Vector3(1.0f)),
			"a Vector3 aimed at the rotation track is refused");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "the mismatch asserts exactly once");
		xCapture.ResetHitCount();

		ZENITH_ASSERT_FALSE(xChannel.SetKeyframeValue(FLUX_ANIM_TRACK_POSITION, 0u, Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f)),
			"a Quat aimed at the position track is refused");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "the mismatch asserts exactly once");
		xCapture.ResetHitCount();

		ZENITH_ASSERT_FALSE(xChannel.InsertKeyframeAt(FLUX_ANIM_TRACK_ROTATION, 0.5f, Zenith_Maths::Vector3(1.0f)),
			"insert refuses a Vector3 on the rotation track");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "the mismatch asserts exactly once");
		xCapture.ResetHitCount();

		ZENITH_ASSERT_FALSE(xChannel.InsertKeyframeAt(FLUX_ANIM_TRACK_SCALE, 0.5f, Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f)),
			"insert refuses a Quat on the scale track");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "the mismatch asserts exactly once");
	}

	ZENITH_ASSERT_EQ(xChannel.GetKeyframeCount(FLUX_ANIM_TRACK_POSITION), 3u, "no key was added or dropped");
	ZENITH_ASSERT_EQ(xChannel.GetKeyframeCount(FLUX_ANIM_TRACK_ROTATION), 1u, "no key was added or dropped");
	ZENITH_ASSERT_EQ(xChannel.GetKeyframeCount(FLUX_ANIM_TRACK_SCALE),    1u, "no key was added or dropped");
	MutAssertTangentsParallel(xChannel, "track-mismatch");
}

// ★ D14, and the reason is that THE TWO SAMPLERS DISAGREE about an empty channel.
// Flux_SkeletonPose::SampleFromClip guards each track with Has*Keyframes() and so
// leaves the bind pose alone (Flux_BonePose.cpp:190-195, :259-264) — while the
// direct channel API, Flux_BoneChannel::Sample*(), has no guard and hands back the
// origin, the identity rotation and unit scale. One empty channel therefore means
// "not animated" through one door and "authored at the origin" through the other,
// with a legitimate key count of zero either way. The fix is to have only one
// representation of "this bone is not animated": no channel.
//
// The tail of this test also pins the guard branch it relies on — that a channel is
// only empty when ALL THREE tracks are, not when one is.
ZENITH_TEST(AnimationMutation, ClipRemovingTheLastKeyRemovesTheChannel)
{
	Flux_AnimationClip xClip;
	xClip.SetDuration(2.0f);

	{
		Flux_BoneChannel xSolo;
		xSolo.AddPositionKeyframe(0.5f, Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f));
		xSolo.SortKeyframes();
		xClip.AddBoneChannel("Solo", std::move(xSolo));
	}
	{
		Flux_BoneChannel xPair;
		xPair.AddPositionKeyframe(0.5f, Zenith_Maths::Vector3(0.0f));
		xPair.AddRotationKeyframe(0.5f, Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f));
		xPair.SortKeyframes();
		xClip.AddBoneChannel("Pair", std::move(xPair));
	}
	ZENITH_ASSERT_EQ(xClip.GetBoneChannels().GetSize(), 2u, "both channels start present");

	// A channel with keys on ANOTHER track survives — "empty" means all three.
	ZENITH_ASSERT_TRUE(xClip.RemoveKeyframe("Pair", FLUX_ANIM_TRACK_POSITION, 0u), "the removal succeeds");
	ZENITH_ASSERT_TRUE(xClip.HasBoneChannel("Pair"), "a channel still holding a rotation key must NOT be pruned");

	ZENITH_ASSERT_TRUE(xClip.RemoveKeyframe("Pair", FLUX_ANIM_TRACK_ROTATION, 0u), "the removal succeeds");
	ZENITH_ASSERT_FALSE(xClip.HasBoneChannel("Pair"), "the removal that empties a channel removes the CHANNEL (D14)");

	ZENITH_ASSERT_TRUE(xClip.RemoveKeyframe("Solo", FLUX_ANIM_TRACK_POSITION, 0u), "the removal succeeds");
	ZENITH_ASSERT_FALSE(xClip.HasBoneChannel("Solo"), "a single-key channel goes in one step");
	ZENITH_ASSERT_EQ(xClip.GetBoneChannels().GetSize(), 0u, "the clip carries no empty channels");

	// An unknown bone is a no-op refusal, not a crash and not a created channel.
	ZENITH_ASSERT_FALSE(xClip.RemoveKeyframe("Ghost", FLUX_ANIM_TRACK_POSITION, 0u), "removing from an absent bone is refused");
	ZENITH_ASSERT_FALSE(xClip.RemoveBoneChannel("Ghost"), "removing an absent channel is refused");
	ZENITH_ASSERT_EQ(xClip.GetBoneChannels().GetSize(), 0u, "a refusal creates nothing");

	// D12: none of that touched the authored duration.
	ZENITH_ASSERT_EQ_FLOAT(xClip.GetDuration(), 2.0f, 1e-6f, "removals never recompute the duration");
}

// The other half of D14: the CHANNEL-level mutator cannot prune, because a channel
// cannot reach the hash map that owns it. That is not an oversight — it is why
// PruneEmptyChannel exists as a separate verb for the document layer to call after
// an edit made through GetBoneChannelMutable.
ZENITH_TEST(AnimationMutation, ClipPruneEmptyChannelIsTheDocumentsRoute)
{
	Flux_AnimationClip xClip;
	xClip.SetDuration(1.0f);

	Flux_BoneChannel& xNew = xClip.GetOrAddBoneChannel("Elbow");
	ZENITH_ASSERT_TRUE(xNew.GetBoneName() == "Elbow", "GetOrAddBoneChannel stamps the bone name onto the channel");
	ZENITH_ASSERT_TRUE(xClip.HasBoneChannel("Elbow"), "the created channel is in the clip");
	ZENITH_ASSERT_TRUE(xNew.IsEmpty(), "a freshly created channel is empty — the state the caller must resolve");
	xNew.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(0.0f));
	xNew.SortKeyframes();

	// A second call must return the SAME channel, not clobber it with a fresh one.
	ZENITH_ASSERT_EQ(xClip.GetOrAddBoneChannel("Elbow").GetKeyframeCount(FLUX_ANIM_TRACK_POSITION), 1u,
		"GetOrAddBoneChannel returns the EXISTING channel when the bone already has one");

	ZENITH_ASSERT_FALSE(xClip.PruneEmptyChannel("Elbow"), "a non-empty channel is never pruned");
	ZENITH_ASSERT_TRUE(xClip.HasBoneChannel("Elbow"), "and it is still there");

	Flux_BoneChannel* pxMutable = xClip.GetBoneChannelMutable("Elbow");
	ZENITH_ASSERT_NOT_NULL(pxMutable, "the mutable accessor resolves a present bone");
	ZENITH_ASSERT_TRUE(pxMutable->RemoveKeyframe(FLUX_ANIM_TRACK_POSITION, 0u), "the channel-level removal succeeds");
	ZENITH_ASSERT_TRUE(xClip.HasBoneChannel("Elbow"),
		"the CHANNEL-level mutator does not prune — this is the bypass PruneEmptyChannel exists to close");

	ZENITH_ASSERT_TRUE(xClip.PruneEmptyChannel("Elbow"), "the document's prune removes the now-empty channel");
	ZENITH_ASSERT_FALSE(xClip.HasBoneChannel("Elbow"), "and it is gone");
	ZENITH_ASSERT_FALSE(xClip.PruneEmptyChannel("Elbow"), "pruning an absent channel is an idempotent false");
	ZENITH_ASSERT_NULL(xClip.GetBoneChannelMutable("Elbow"), "the mutable accessor returns nullptr for an absent bone");

	// RemoveBoneChannel drops a NON-empty channel outright — the other verb.
	Flux_BoneChannel& xWrist = xClip.GetOrAddBoneChannel("Wrist");
	xWrist.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(0.0f));
	xWrist.SortKeyframes();
	ZENITH_ASSERT_TRUE(xClip.RemoveBoneChannel("Wrist"), "RemoveBoneChannel drops a channel that still has keys");
	ZENITH_ASSERT_FALSE(xClip.HasBoneChannel("Wrist"), "and it is gone");

	ZENITH_ASSERT_EQ_FLOAT(xClip.GetDuration(), 1.0f, 1e-6f, "channel mutation never recomputes the duration");
}

// D12 + D13 together: the duration is AUTHORED and no mutator recomputes it, and a
// key past the duration is PERMITTED rather than vetoed or clamped. The overrun
// stays REPORTABLE — Flux_ClipKeyTimesFitDuration still says so — which is what
// lets the panel warn about it later instead of the mutator silently deciding.
ZENITH_TEST(AnimationMutation, ClipMutatorsLeaveDurationAuthored)
{
	Flux_AnimationClip xClip;
	xClip.SetDuration(2.0f);

	Flux_BoneChannel& xChannel = xClip.GetOrAddBoneChannel("Root");
	xChannel.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(0.0f));
	xChannel.AddPositionKeyframe(1.0f, Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
	xChannel.SortKeyframes();
	ZENITH_ASSERT_TRUE(Flux_ClipKeyTimesFitDuration(xClip), "the clip starts inside its duration");

	u_int uIndex = 0xFFFFFFFFu;
	ZENITH_ASSERT_TRUE(xChannel.InsertKeyframeAt(FLUX_ANIM_TRACK_POSITION, 10.0f, Zenith_Maths::Vector3(10.0f, 0.0f, 0.0f), &uIndex),
		"a key PAST the duration must be permitted (D13)");
	ZENITH_ASSERT_EQ(uIndex, 2u, "it still lands at its sorted position");
	ZENITH_ASSERT_EQ_FLOAT(xClip.GetDuration(), 2.0f, 1e-6f, "an insert must not stretch the duration (D12)");
	ZENITH_ASSERT_FALSE(Flux_ClipKeyTimesFitDuration(xClip),
		"the overrun stays visible to the key-times/duration helper — permitted is not hidden");
	ZENITH_ASSERT_EQ_FLOAT(Flux_ClipLastKeyTimeSeconds(xClip), 10.0f, 1e-6f, "the helper names the offending time");

	// The rest of the verbs, same expectation.
	ZENITH_ASSERT_TRUE(xChannel.SetKeyframeTime(FLUX_ANIM_TRACK_POSITION, 2u, 20.0f), "retiming further past the end is permitted");
	ZENITH_ASSERT_EQ_FLOAT(xClip.GetDuration(), 2.0f, 1e-6f, "a retime must not stretch the duration");
	ZENITH_ASSERT_TRUE(xChannel.SetKeyframeValue(FLUX_ANIM_TRACK_POSITION, 0u, Zenith_Maths::Vector3(7.0f, 7.0f, 7.0f)), "revalue succeeds");
	ZENITH_ASSERT_EQ_FLOAT(xClip.GetDuration(), 2.0f, 1e-6f, "a revalue must not touch the duration");
	ZENITH_ASSERT_TRUE(xChannel.RemoveKeyframe(FLUX_ANIM_TRACK_POSITION, 0u), "removal succeeds");
	ZENITH_ASSERT_EQ_FLOAT(xClip.GetDuration(), 2.0f, 1e-6f, "a removal must not SHRINK the duration either");
	ZENITH_ASSERT_TRUE(xClip.RemoveKeyframe("Root", FLUX_ANIM_TRACK_POSITION, 0u), "the clip-level removal succeeds");
	ZENITH_ASSERT_EQ_FLOAT(xClip.GetDuration(), 2.0f, 1e-6f, "and the clip-level entry point does not either");
	MutAssertTangentsParallel(*xClip.GetBoneChannelMutable("Root"), "duration-battery");
}

// ============================================================================
// Root motion (D16) — the SAME matrix, on Flux_RootMotion's two delta tracks.
//
// ★ ROOT MOTION HAS NO TANGENT ARRAYS, and this unit adds none: doing so would
// move the .zanim layout, which is out of scope here. So the lockstep assertions
// above have no counterpart here — the mutators share the channel's implementation
// and pass no tangent array, which is why the policy cannot drift between the two.
// ============================================================================

ZENITH_TEST(AnimationMutation, RootMotionInsertAndRemoveMatchChannelPolicy)
{
	Flux_RootMotion xRM;
	xRM.m_bEnabled = true;
	xRM.m_xPositionDeltas.EmplaceBack(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), 0.0f);
	xRM.m_xPositionDeltas.EmplaceBack(Zenith_Maths::Vector3(2.0f, 0.0f, 0.0f), 2.0f);

	u_int uIndex = 0xFFFFFFFFu;
	ZENITH_ASSERT_TRUE(xRM.InsertKeyframeAt(FLUX_ANIM_TRACK_POSITION, 1.0f, Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f), &uIndex),
		"an insert on a free time succeeds");
	ZENITH_ASSERT_EQ(uIndex, 1u, "the new delta lands at its sorted position");
	ZENITH_ASSERT_EQ(xRM.GetKeyframeCount(FLUX_ANIM_TRACK_POSITION), 3u, "the key count grew by one");
	ZENITH_ASSERT_EQ_FLOAT(xRM.m_xPositionDeltas.Get(1).second, 1.0f, 1e-6f, "sorted between the two originals");

	// Replace-on-occupied, through the epsilon rather than an exact compare.
	ZENITH_ASSERT_TRUE(xRM.InsertKeyframeAt(FLUX_ANIM_TRACK_POSITION, 1.0f + (fANIM_TIME_EPSILON * 0.5f),
		Zenith_Maths::Vector3(5.0f, 0.0f, 0.0f), &uIndex), "an insert on an occupied time replaces");
	ZENITH_ASSERT_EQ(uIndex, 1u, "the replaced delta keeps its slot");
	ZENITH_ASSERT_EQ(xRM.GetKeyframeCount(FLUX_ANIM_TRACK_POSITION), 3u, "a replace must not add a key");
	ZENITH_ASSERT_TRUE(RootMotionVec3Equals(xRM.m_xPositionDeltas.Get(1).first, Zenith_Maths::Vector3(5.0f, 0.0f, 0.0f)),
		"the value was replaced");

	// The SAMPLER — untouched by this unit — still reads the mutated track correctly,
	// which is the property a mutator that left the track unsorted would break.
	ZENITH_ASSERT_TRUE(RootMotionVec3Equals(xRM.SamplePositionDelta(1.0f), Zenith_Maths::Vector3(5.0f, 0.0f, 0.0f), 1e-4f),
		"sampling at the mutated key's time returns the mutated value");

	ZENITH_ASSERT_TRUE(xRM.RemoveKeyframe(FLUX_ANIM_TRACK_POSITION, 1u), "removal succeeds");
	ZENITH_ASSERT_EQ(xRM.GetKeyframeCount(FLUX_ANIM_TRACK_POSITION), 2u, "the key count dropped by one");
	ZENITH_ASSERT_EQ_FLOAT(xRM.m_xPositionDeltas.Get(1).second, 2.0f, 1e-6f, "the later delta closed up");

	// The rotation delta track normalizes on write, exactly like a bone channel's.
	const Zenith_Maths::Quat xUnit = glm::angleAxis(glm::radians(30.0f), Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
	const Zenith_Maths::Quat xLong(xUnit.w * 4.0f, xUnit.x * 4.0f, xUnit.y * 4.0f, xUnit.z * 4.0f);
	ZENITH_ASSERT_TRUE(xRM.InsertKeyframeAt(FLUX_ANIM_TRACK_ROTATION, 0.0f, xLong, &uIndex), "a rotation delta inserts");
	ZENITH_ASSERT_EQ(uIndex, 0u, "the first rotation delta lands at index 0");
	ZENITH_ASSERT_EQ_FLOAT(glm::length(xRM.m_xRotationDeltas.Get(0).first), 1.0f, 1e-5f,
		"a root-motion rotation delta is normalized on write");
}

ZENITH_TEST(AnimationMutation, RootMotionSetKeyframeTimeMatchesChannelPolicy)
{
	Flux_RootMotion xRM;
	xRM.m_bEnabled = true;
	xRM.m_xPositionDeltas.EmplaceBack(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), 0.0f);
	xRM.m_xPositionDeltas.EmplaceBack(Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f), 1.0f);
	xRM.m_xPositionDeltas.EmplaceBack(Zenith_Maths::Vector3(2.0f, 0.0f, 0.0f), 2.0f);

	// Onto an occupied time: refused, nothing changed. No merge here either.
	u_int uIndex = 0xFFFFFFFFu;
	ZENITH_ASSERT_FALSE(xRM.SetKeyframeTime(FLUX_ANIM_TRACK_POSITION, 0u, 1.0f, &uIndex),
		"retiming a delta onto an occupied time is refused");
	ZENITH_ASSERT_EQ(uIndex, 0xFFFFFFFFu, "a refused retime writes no out-index");
	ZENITH_ASSERT_EQ(xRM.GetKeyframeCount(FLUX_ANIM_TRACK_POSITION), 3u, "nothing was merged away");
	ZENITH_ASSERT_EQ_FLOAT(xRM.m_xPositionDeltas.Get(0).second, 0.0f, 1e-9f, "the dragged delta keeps its time");
	ZENITH_ASSERT_EQ_FLOAT(xRM.m_xPositionDeltas.Get(1).second, 1.0f, 1e-9f, "the target delta keeps its time");

	// Onto a free time: re-sorted, value carried.
	ZENITH_ASSERT_TRUE(xRM.SetKeyframeTime(FLUX_ANIM_TRACK_POSITION, 0u, 1.5f, &uIndex), "retiming to a free time succeeds");
	ZENITH_ASSERT_EQ(uIndex, 1u, "the moved delta's new index comes back");
	ZENITH_ASSERT_EQ_FLOAT(xRM.m_xPositionDeltas.Get(0).second, 1.0f, 1e-6f, "the untouched delta is now first");
	ZENITH_ASSERT_EQ_FLOAT(xRM.m_xPositionDeltas.Get(1).second, 1.5f, 1e-6f, "the moved delta sits at its new time");
	ZENITH_ASSERT_TRUE(RootMotionVec3Equals(xRM.m_xPositionDeltas.Get(1).first, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f)),
		"the moved delta carried its VALUE with it");

	ZENITH_ASSERT_TRUE(xRM.SetKeyframeValue(FLUX_ANIM_TRACK_POSITION, 1u, Zenith_Maths::Vector3(9.0f, 9.0f, 9.0f)),
		"revaluing a delta succeeds");
	ZENITH_ASSERT_TRUE(RootMotionVec3Equals(xRM.m_xPositionDeltas.Get(1).first, Zenith_Maths::Vector3(9.0f, 9.0f, 9.0f)),
		"the value was written");
	ZENITH_ASSERT_EQ_FLOAT(xRM.m_xPositionDeltas.Get(1).second, 1.5f, 1e-9f, "a revalue leaves the TIME alone");

	float fTime = 0.0f;
	ZENITH_ASSERT_TRUE(xRM.GetKeyframeTime(FLUX_ANIM_TRACK_POSITION, 1u, fTime), "GetKeyframeTime resolves an in-range index");
	ZENITH_ASSERT_EQ_FLOAT(fTime, 1.5f, 1e-6f, "and reports the moved time");
	ZENITH_ASSERT_FALSE(xRM.GetKeyframeTime(FLUX_ANIM_TRACK_POSITION, 99u, fTime), "and refuses an out-of-range one");
	ZENITH_ASSERT_EQ(xRM.FindKeyframeAtTime(FLUX_ANIM_TRACK_POSITION, 1.5f + (fANIM_TIME_EPSILON * 0.5f)), 1u,
		"FindKeyframeAtTime resolves through the epsilon");
	ZENITH_ASSERT_EQ(xRM.FindKeyframeAtTime(FLUX_ANIM_TRACK_POSITION, 1.7f), 3u,
		"and reports the key COUNT when the time is free");
}

ZENITH_TEST(AnimationMutation, RootMotionRejectsScaleTrackAndBadValues)
{
	Flux_RootMotion xRM;
	xRM.m_bEnabled = true;
	xRM.m_xPositionDeltas.EmplaceBack(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), 0.0f);
	xRM.m_xRotationDeltas.EmplaceBack(Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f), 0.0f);

	const float fNaN = std::numeric_limits<float>::quiet_NaN();
	const float fInf = std::numeric_limits<float>::infinity();

	{
		Zenith_AssertCaptureScope xCapture;

		// There is no third track, and asking for one is a caller bug rather than
		// something to quietly map onto the position deltas.
		ZENITH_ASSERT_EQ(xRM.GetKeyframeCount(FLUX_ANIM_TRACK_SCALE), 0u, "root motion has no scale track");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "asking for a scale track asserts exactly once");
		xCapture.ResetHitCount();

		ZENITH_ASSERT_FALSE(xRM.RemoveKeyframe(FLUX_ANIM_TRACK_SCALE, 0u), "removing from the scale track is refused");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "and asserts exactly once");
		xCapture.ResetHitCount();

		ZENITH_ASSERT_FALSE(xRM.InsertKeyframeAt(FLUX_ANIM_TRACK_SCALE, 1.0f, Zenith_Maths::Vector3(1.0f)),
			"inserting into the scale track is refused");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "and asserts exactly once");
		xCapture.ResetHitCount();

		ZENITH_ASSERT_FALSE(xRM.InsertKeyframeAt(FLUX_ANIM_TRACK_POSITION, fNaN, Zenith_Maths::Vector3(1.0f)),
			"a NaN delta time is refused");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "and asserts exactly once");
		xCapture.ResetHitCount();

		ZENITH_ASSERT_FALSE(xRM.InsertKeyframeAt(FLUX_ANIM_TRACK_POSITION, -fInf, Zenith_Maths::Vector3(1.0f)),
			"a -inf delta time is refused");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "and asserts exactly once");
		xCapture.ResetHitCount();

		ZENITH_ASSERT_FALSE(xRM.InsertKeyframeAt(FLUX_ANIM_TRACK_POSITION, -1.0f, Zenith_Maths::Vector3(1.0f)),
			"a negative delta time is refused, NOT clamped");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "and asserts exactly once");
		xCapture.ResetHitCount();

		const Zenith_Maths::Quat xZero(0.0f, 0.0f, 0.0f, 0.0f);
		ZENITH_ASSERT_FALSE(xRM.InsertKeyframeAt(FLUX_ANIM_TRACK_ROTATION, 1.0f, xZero),
			"a zero rotation delta is refused, not normalized into NaN");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "and asserts exactly once");
		xCapture.ResetHitCount();

		ZENITH_ASSERT_FALSE(xRM.SetKeyframeValue(FLUX_ANIM_TRACK_ROTATION, 0u, xZero),
			"revaluing a rotation delta to zero is refused too");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "and asserts exactly once");
	}

	ZENITH_ASSERT_EQ(xRM.GetKeyframeCount(FLUX_ANIM_TRACK_POSITION), 1u, "no refusal added or dropped a position delta");
	ZENITH_ASSERT_EQ(xRM.GetKeyframeCount(FLUX_ANIM_TRACK_ROTATION), 1u, "no refusal added or dropped a rotation delta");
	ZENITH_ASSERT_EQ_FLOAT(glm::length(xRM.m_xRotationDeltas.Get(0).first), 1.0f, 1e-5f,
		"the surviving rotation delta is still a real rotation");
}

// ============================================================================
// WU-2.1 — in-place content replacement (D26), the name-immutability rule (D28)
// and the status-returning parse.
//
// Pure CPU: everything below builds clips in memory and serializes to a
// Zenith_DataStream. No device, no file, no registry — so none of these is
// requiresGraphics and all of them run under the Null backend.
// ============================================================================

namespace
{
	// The one value every replace test watches. Sampling a channel is what a
	// controller actually does with a clip, so a test that only compared key COUNTS
	// could pass on a clip whose keys had not moved at all.
	float ClipSampleHipHeight(const Flux_AnimationClip& xClip, float fTimeSeconds)
	{
		const Flux_BoneChannel* pxHip = xClip.GetBoneChannel("Hip");
		if (pxHip == nullptr)
		{
			return -1.0f;
		}
		return pxHip->SamplePosition(fTimeSeconds).y;
	}

	// The same shape as ClipBuildTwoBoneClip, but with the Hip's height and a few
	// metadata fields under the caller's control so a "before" and an "after" clip
	// are distinguishable by SAMPLING, not just by counting.
	void ClipBuildProbe(Flux_AnimationClip& xClip, const char* szName, float fHipHeight)
	{
		xClip.SetName(szName);
		xClip.SetDuration(2.0f);

		Flux_BoneChannel xHip;
		xHip.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(0.0f, fHipHeight, 0.0f));
		xHip.AddPositionKeyframe(2.0f, Zenith_Maths::Vector3(0.0f, fHipHeight, 0.0f));
		xClip.AddBoneChannel("Hip", std::move(xHip));
	}
}

ZENITH_TEST(AnimationReload, ReplaceContentsFromCarriesEverythingInPlace)
{
	// The destination starts life with DIFFERENT content under the same name, and a
	// bone channel the source does not have. A replace that merged rather than
	// replaced would leave the stale channel behind.
	Flux_AnimationClip xLive;
	ClipBuildProbe(xLive, "Walk", 1.0f);
	Flux_BoneChannel xStale;
	xStale.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(9.0f, 9.0f, 9.0f));
	xLive.AddBoneChannel("StaleBone", std::move(xStale));
	xLive.GetMetadata().m_uAuthoredFrameRate = 24;
	xLive.SetSourcePath("game:Meshes/Old.glb");

	Flux_AnimationClip xSource;
	ClipBuildProbe(xSource, "Walk", 4.0f);
	xSource.SetDuration(3.5f);
	xSource.SetLooping(false);
	xSource.SetTicksPerSecond(60);
	xSource.GetMetadata().m_uAuthoredFrameRate = 60;
	xSource.GetMetadata().m_strSkeletonPath = "engine:Meshes/StickFigure/StickFigure.zskel";
	xSource.GetMetadata().m_strPreviewModelPath = "engine:Meshes/StickFigure/StickFigure.zmodel";
	xSource.GetMetadata().m_bGenerated = true;
	xSource.SetSourcePath("game:Meshes/New.glb");
	Flux_AnimationEvent xEvent;
	xEvent.m_fNormalizedTime = 0.25f;
	xEvent.m_strEventName = "FootstepRight";
	xSource.AddEvent(xEvent);
	xSource.GetRootMotion().m_bEnabled = true;
	xSource.GetRootMotion().m_xPositionDeltas.EmplaceBack(Zenith_Maths::Vector3(0.0f, 0.0f, 2.0f), 1.0f);

	// The address a borrower would be holding.
	const Flux_AnimationClip* pxAddressBefore = &xLive;

	ZENITH_ASSERT_TRUE(xLive.ReplaceContentsFrom(xSource), "a same-named replace is accepted");
	ZENITH_ASSERT_TRUE(&xLive == pxAddressBefore, "the clip OBJECT does not move — that is the whole point");

	ZENITH_ASSERT_EQ_FLOAT(ClipSampleHipHeight(xLive, 1.0f), 4.0f, 1e-5f, "the live clip samples the SOURCE's keys");
	ZENITH_ASSERT_FALSE(xLive.HasBoneChannel("StaleBone"), "a channel the source lacks is REMOVED, not merged");
	ZENITH_ASSERT_EQ(xLive.GetBoneChannels().GetSize(), 1u, "channel map is the source's, exactly");
	ZENITH_ASSERT_EQ_FLOAT(xLive.GetDuration(), 3.5f, 1e-6f, "duration carries");
	ZENITH_ASSERT_FALSE(xLive.IsLooping(), "looping carries");
	ZENITH_ASSERT_EQ(xLive.GetTicksPerSecond(), 60u, "ticks-per-second carries");
	ZENITH_ASSERT_EQ(xLive.GetMetadata().m_uAuthoredFrameRate, 60u, "authored frame rate carries");
	ZENITH_ASSERT_TRUE(xLive.GetMetadata().m_strSkeletonPath == "engine:Meshes/StickFigure/StickFigure.zskel", "skeleton path carries");
	ZENITH_ASSERT_TRUE(xLive.GetMetadata().m_strPreviewModelPath == "engine:Meshes/StickFigure/StickFigure.zmodel", "preview model path carries");
	ZENITH_ASSERT_TRUE(xLive.GetMetadata().m_bGenerated, "generated flag carries");
	ZENITH_ASSERT_TRUE(xLive.GetSourcePath() == "game:Meshes/New.glb", "source path carries");
	ZENITH_ASSERT_EQ(xLive.GetEvents().GetSize(), 1u, "events carry");
	ZENITH_ASSERT_TRUE(xLive.GetRootMotion().m_bEnabled, "root motion enable carries");
	ZENITH_ASSERT_EQ(xLive.GetRootMotion().m_xPositionDeltas.GetSize(), 1u, "root motion deltas carry");

	// The source is untouched — this is a copy, and a caller may keep using its
	// staging clip afterwards (Zenith_AnimationAsset's does not, but nothing here
	// should silently gut it either).
	ZENITH_ASSERT_TRUE(xSource.GetName() == "Walk", "the SOURCE is left intact");
	ZENITH_ASSERT_EQ(xSource.GetBoneChannels().GetSize(), 1u, "the SOURCE keeps its channels");
}

ZENITH_TEST(AnimationReload, ReplaceContentsFromRefusesARename)
{
	// D28. The collection is name-keyed and AddClip on a collision DELETES the clip
	// already registered under that name, so a rename underneath a live clip breaks
	// both the collection lookup and every state-machine reference resolved through
	// it — silently, because neither side re-checks.
	Flux_AnimationClip xLive;
	ClipBuildProbe(xLive, "Walk", 1.0f);

	Flux_AnimationClip xRenamed;
	ClipBuildProbe(xRenamed, "Run", 7.0f);
	xRenamed.SetDuration(9.0f);

	{
		Zenith_AssertCaptureScope xCapture;
		ZENITH_ASSERT_FALSE(xLive.ReplaceContentsFrom(xRenamed), "a rename is refused");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "and asserts exactly once");
	}

	// A refusal changes NOTHING — not the name, not the keys, not the duration.
	ZENITH_ASSERT_TRUE(xLive.GetName() == "Walk", "the live clip keeps its name");
	ZENITH_ASSERT_EQ_FLOAT(ClipSampleHipHeight(xLive, 1.0f), 1.0f, 1e-5f, "the live clip keeps its keys");
	ZENITH_ASSERT_EQ_FLOAT(xLive.GetDuration(), 2.0f, 1e-6f, "the live clip keeps its duration");
}

ZENITH_TEST(AnimationReload, ReplaceContentsFromPopulatesAnUnnamedClip)
{
	// The one exception to D28: a freshly constructed clip has no name to protect, so
	// the FIRST populate is not a rename. This is the path Zenith_AnimationAsset takes
	// on an initial load, and it must not assert.
	Flux_AnimationClip xFresh;
	ZENITH_ASSERT_TRUE(xFresh.GetName().empty(), "a default-constructed clip is unnamed");

	Flux_AnimationClip xSource;
	ClipBuildProbe(xSource, "Idle", 2.0f);

	{
		Zenith_AssertCaptureScope xCapture;
		ZENITH_ASSERT_TRUE(xFresh.ReplaceContentsFrom(xSource), "populating an unnamed clip is accepted");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 0u, "and asserts NOT AT ALL — this is not a rename");
	}
	ZENITH_ASSERT_TRUE(xFresh.GetName() == "Idle", "the fresh clip adopts the source's name");
	ZENITH_ASSERT_EQ_FLOAT(ClipSampleHipHeight(xFresh, 1.0f), 2.0f, 1e-5f, "and the source's keys");

	// Self-replace is a no-op success rather than self-assignment through every
	// owned container.
	ZENITH_ASSERT_TRUE(xSource.ReplaceContentsFrom(xSource), "self-replace succeeds");
	ZENITH_ASSERT_EQ(xSource.GetBoneChannels().GetSize(), 1u, "and does not destroy the clip");
}

ZENITH_TEST(AnimationReload, ClipParseStreamReportsRefusalsAsAStatus)
{
	// ★ THE READER REPORTS FAILURE. Before WU-2.1 the only signals a refused .zanim
	// produced were an assert and an empty clip; the return type was void, so every
	// caller carried on as if the load had worked.
	Flux_AnimationClip xClip;
	ClipBuildTwoBoneClip(xClip);

	// A good stream parses OK.
	{
		Zenith_DataStream xGood;
		xClip.WriteToDataStream(xGood);
		xGood.SetCursor(0);
		Flux_AnimationClip xLoaded;
		ZENITH_ASSERT_TRUE(xLoaded.ParseStream(xGood).IsOk(), "ParseStream accepts its own output");
		ZENITH_ASSERT_EQ(xLoaded.GetBoneChannels().GetSize(), 2u, "and populates the clip");
	}

	// A future schema is VERSION_MISMATCH, not a plausible clip.
	{
		Zenith_DataStream xFuture;
		xClip.WriteToDataStream(xFuture);
		ClipPokeU32(xFuture, ulCLIP_HEADER_SCHEMA_OFFSET, uZENITH_ANIMATION_SCHEMA_CURRENT + 1u);
		xFuture.SetCursor(0);

		Flux_AnimationClip xLoaded;
		Zenith_AssertCaptureScope xCapture;
		const Zenith_Status xStatus = xLoaded.ParseStream(xFuture);
		ZENITH_ASSERT_FALSE(xStatus.IsOk(), "a future schema is refused with a STATUS");
		ZENITH_ASSERT_EQ(xStatus.Error(), Zenith_ErrorCode::VERSION_MISMATCH, "and the status names the reason");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "and still asserts exactly once");
	}

	// A headerless stream is BAD_MAGIC (D2: there is no legacy branch).
	{
		Zenith_DataStream xHeaderless;
		xClip.WriteToDataStream(xHeaderless);
		ClipPokeU32(xHeaderless, ulCLIP_HEADER_MAGIC_OFFSET, uSTREAM_ENVELOPE_MAGIC ^ 0xFFu);
		xHeaderless.SetCursor(0);

		Flux_AnimationClip xLoaded;
		Zenith_AssertCaptureScope xCapture;
		const Zenith_Status xStatus = xLoaded.ParseStream(xHeaderless);
		ZENITH_ASSERT_FALSE(xStatus.IsOk(), "a missing envelope is refused with a STATUS");
		ZENITH_ASSERT_EQ(xStatus.Error(), Zenith_ErrorCode::BAD_MAGIC, "and the status names the reason");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "and still asserts exactly once");
		ZENITH_ASSERT_EQ(xLoaded.GetBoneChannels().GetSize(), 0u, "a refused parse leaves an EMPTY clip");
	}

	// A stream too short to hold an envelope at all — the truncation case.
	//
	// ★ It has to WRAP a fixed 4-byte buffer, not be an owned stream with four bytes
	// written into it. Zenith_ReadStreamHeader measures against GetCapacity(), and an
	// owned write stream's capacity is its 1024-byte allocation rather than the bytes
	// written — so an owned "truncated" stream would sail past the size check and read
	// three words of uninitialised heap, giving a different error code per build tier.
	{
		u_int uMagicOnly = uSTREAM_ENVELOPE_MAGIC;
		Zenith_DataStream xTruncated(&uMagicOnly, sizeof(uMagicOnly));

		Flux_AnimationClip xLoaded;
		Zenith_AssertCaptureScope xCapture;
		const Zenith_Status xStatus = xLoaded.ParseStream(xTruncated);
		ZENITH_ASSERT_FALSE(xStatus.IsOk(), "a truncated stream is refused with a STATUS");
		ZENITH_ASSERT_EQ(xStatus.Error(), Zenith_ErrorCode::BAD_MAGIC, "too short to hold an envelope is 'not a .zanim'");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "and asserts exactly once");
	}

	// Another asset type's envelope is INVALID_ARGUMENT — a .zskel is not a .zanim.
	{
		Zenith_DataStream xWrongType;
		Zenith_WriteStreamHeader(xWrongType, uZENITH_SKELETON_ASSET_TYPE_ID, uZENITH_SKELETON_SCHEMA_CURRENT);
		const uint32_t uZeroBones = 0;
		xWrongType << uZeroBones;
		xWrongType.SetCursor(0);

		Flux_AnimationClip xLoaded;
		Zenith_AssertCaptureScope xCapture;
		const Zenith_Status xStatus = xLoaded.ParseStream(xWrongType);
		ZENITH_ASSERT_FALSE(xStatus.IsOk(), "another asset's envelope is refused with a STATUS");
		ZENITH_ASSERT_EQ(xStatus.Error(), Zenith_ErrorCode::INVALID_ARGUMENT, "and the status names the reason");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "and asserts exactly once");
	}
}

ZENITH_TEST(AnimationReload, CollectionRefusesAnUnnamedClip)
{
	// An unnamed clip keys on "": two of them evict each other from m_xClipsByName
	// while both stay in m_xClips, so the map and the ordered list stop agreeing and
	// one of them ends up holding a freed pointer. The collection now says so.
	//
	// Declared BEFORE the collection so the borrowed clip outlives it.
	Flux_AnimationClip xBorrowedUnnamed;
	Flux_AnimationClipCollection xCollection;

	Flux_AnimationClip* pxUnnamed = new Flux_AnimationClip();
	{
		Zenith_AssertCaptureScope xCapture;
		xCollection.AddClip(pxUnnamed);
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "AddClip asserts on an unnamed clip");
	}

	{
		Zenith_AssertCaptureScope xCapture;
		xCollection.AddClipReference(&xBorrowedUnnamed);
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "AddClipReference asserts on an unnamed clip too");
	}

	// A NAMED clip is added without complaint — the assert is about the empty key,
	// not about adding clips.
	Flux_AnimationClip* pxNamed = new Flux_AnimationClip();
	pxNamed->SetName("Idle");
	{
		Zenith_AssertCaptureScope xCapture;
		xCollection.AddClip(pxNamed);
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 0u, "a named clip is added silently");
	}
	ZENITH_ASSERT_TRUE(xCollection.GetClip("Idle") == pxNamed, "and resolves by name");
}
