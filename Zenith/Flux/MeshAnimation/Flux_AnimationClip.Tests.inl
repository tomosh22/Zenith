#include "UnitTests/Zenith_UnitTests.h"
#include "UnitTests/Zenith_AssertCapture.h"   // the refused-envelope / refused-schema reads assert on purpose
#include "Flux/MeshAnimation/Flux_AnimationClip.h"
#include "Flux/MeshAnimation/Flux_BonePose.h"  // WU-1.2: the SAMPLER, which is where the tick multiply lived
#include "AssetHandling/Zenith_AssetTypeIds.h"
#include "DataStream/Zenith_StreamEnvelope.h"

#include <cstring>   // std::memcmp — the byte-identity determinism check

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
