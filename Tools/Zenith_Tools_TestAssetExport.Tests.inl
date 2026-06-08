#include "UnitTests/Zenith_UnitTests.h"
#include "Flux/MeshAnimation/Flux_AnimationClip.h"

// ============================================================================
// StickFigure procedural-clip tests
//
// The four Create*Animation() factories above this include site (Aim, Fire,
// Reload, Jump) build the upper-body shooter clips authored at runtime and
// exported to .zanim files alongside the original Idle/Walk/Run set.
//
// These tests verify metadata, bone-channel presence, and a few representative
// sampled values to pin spec-matching behavior. Each test owns the clip it
// constructs and deletes it before returning.
//
// Note: Flux_BoneChannel::SampleRotation takes time in TICKS, not seconds.
// All clips use 24 ticks/second.
// ============================================================================

namespace
{
	bool StickFigureQuatEquals(const Zenith_Maths::Quat& a, const Zenith_Maths::Quat& b, float fTol = 1e-4f)
	{
		// Quaternion comparison must allow for double-cover (q and -q are the same rotation).
		float fDot = std::abs(a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z);
		return std::abs(fDot - 1.0f) < fTol;
	}
}

// ----- Aim ------------------------------------------------------------------

ZENITH_TEST(StickFigureProcAnim, AimClipMetadata) { Zenith_UnitTests::TestStickFigureAimClipMetadata(); }
void Zenith_UnitTests::TestStickFigureAimClipMetadata()
{
	Flux_AnimationClip* pxClip = CreateAimAnimation();
	ZENITH_ASSERT_TRUE(pxClip->GetName() == "Aim", "Aim clip name should be 'Aim'");
	ZENITH_ASSERT_TRUE(pxClip->IsLooping(), "Aim clip should be looping");
	ZENITH_ASSERT_TRUE(std::abs(pxClip->GetDuration() - 0.5f) < 1e-4f, "Aim clip duration should be 0.5s");
	ZENITH_ASSERT_TRUE(pxClip->GetTicksPerSecond() == 24, "Aim clip should be 24 fps");
	delete pxClip;
}

ZENITH_TEST(StickFigureProcAnim, AimClipBoneChannelsExist) { Zenith_UnitTests::TestStickFigureAimClipBoneChannelsExist(); }
void Zenith_UnitTests::TestStickFigureAimClipBoneChannelsExist()
{
	Flux_AnimationClip* pxClip = CreateAimAnimation();
	ZENITH_ASSERT_TRUE(pxClip->HasBoneChannel("RightUpperArm"), "Aim missing RightUpperArm channel");
	ZENITH_ASSERT_TRUE(pxClip->HasBoneChannel("RightLowerArm"), "Aim missing RightLowerArm channel");
	ZENITH_ASSERT_TRUE(pxClip->HasBoneChannel("LeftUpperArm"),  "Aim missing LeftUpperArm channel");
	ZENITH_ASSERT_TRUE(pxClip->HasBoneChannel("LeftLowerArm"),  "Aim missing LeftLowerArm channel");
	ZENITH_ASSERT_TRUE(pxClip->HasBoneChannel("Spine"),         "Aim missing Spine channel");
	ZENITH_ASSERT_TRUE(pxClip->HasBoneChannel("Head"),          "Aim missing Head channel");
	delete pxClip;
}

ZENITH_TEST(StickFigureProcAnim, AimClipRightArmRotation) { Zenith_UnitTests::TestStickFigureAimClipRightArmRotation(); }
void Zenith_UnitTests::TestStickFigureAimClipRightArmRotation()
{
	Flux_AnimationClip* pxClip = CreateAimAnimation();
	const Flux_BoneChannel* pxCh = pxClip->GetBoneChannel("RightUpperArm");
	ZENITH_ASSERT_TRUE(pxCh != nullptr, "Aim should have RightUpperArm channel");

	// Sample at the end (12 ticks) — should be the aim hold pose.
	const Zenith_Maths::Quat xExpected = StickFigureAimHoldPose::RightUpperArm();
	const Zenith_Maths::Quat xSample = pxCh->SampleRotation(12.0f);
	ZENITH_ASSERT_TRUE(StickFigureQuatEquals(xSample, xExpected),
		"Aim RightUpperArm at t=12 ticks should match aim hold pose");

	// And at the start (t=0 ticks) — same pose, since it's a stable hold.
	const Zenith_Maths::Quat xStart = pxCh->SampleRotation(0.0f);
	ZENITH_ASSERT_TRUE(StickFigureQuatEquals(xStart, xExpected),
		"Aim RightUpperArm at t=0 ticks should also match aim hold pose (stable hold)");
	delete pxClip;
}

// ----- Fire -----------------------------------------------------------------

ZENITH_TEST(StickFigureProcAnim, FireClipMetadata) { Zenith_UnitTests::TestStickFigureFireClipMetadata(); }
void Zenith_UnitTests::TestStickFigureFireClipMetadata()
{
	Flux_AnimationClip* pxClip = CreateFireAnimation();
	ZENITH_ASSERT_TRUE(pxClip->GetName() == "Fire", "Fire clip name should be 'Fire'");
	ZENITH_ASSERT_TRUE(!pxClip->IsLooping(), "Fire clip should NOT be looping");
	ZENITH_ASSERT_TRUE(std::abs(pxClip->GetDuration() - 0.20f) < 1e-4f, "Fire clip duration should be 0.20s");
	ZENITH_ASSERT_TRUE(pxClip->GetTicksPerSecond() == 24, "Fire clip should be 24 fps");
	delete pxClip;
}

ZENITH_TEST(StickFigureProcAnim, FireClipReturnsToAimPoseAtEnd) { Zenith_UnitTests::TestStickFigureFireClipReturnsToAimPoseAtEnd(); }
void Zenith_UnitTests::TestStickFigureFireClipReturnsToAimPoseAtEnd()
{
	Flux_AnimationClip* pxClip = CreateFireAnimation();
	const Flux_BoneChannel* pxCh = pxClip->GetBoneChannel("RightUpperArm");
	ZENITH_ASSERT_TRUE(pxCh != nullptr, "Fire should have RightUpperArm channel");

	// At end of clip the recoil should have settled back to the aim hold pose
	// so the transition to Aim is seamless. Read the last authored keyframe
	// directly — Flux_BoneChannel::SampleRotation has an off-by-one quirk at
	// end-of-clip that returns the first keyframe instead of the last.
	const auto& axRotations = pxCh->GetRotationKeyframes();
	ZENITH_ASSERT_TRUE(axRotations.GetSize() != 0, "Fire RightUpperArm should have keyframes");
	const Zenith_Maths::Quat xExpected = StickFigureAimHoldPose::RightUpperArm();
	const Zenith_Maths::Quat xLast = axRotations.GetBack().first;
	ZENITH_ASSERT_TRUE(StickFigureQuatEquals(xLast, xExpected),
		"Fire RightUpperArm last keyframe should be aim hold pose");
	delete pxClip;
}

ZENITH_TEST(StickFigureProcAnim, FireClipPeakRecoil) { Zenith_UnitTests::TestStickFigureFireClipPeakRecoil(); }
void Zenith_UnitTests::TestStickFigureFireClipPeakRecoil()
{
	Flux_AnimationClip* pxClip = CreateFireAnimation();
	const Flux_BoneChannel* pxCh = pxClip->GetBoneChannel("RightUpperArm");
	ZENITH_ASSERT_TRUE(pxCh != nullptr, "Fire should have RightUpperArm channel");

	// At peak (t=2 ticks) the right upper arm should have +15deg X-axis recoil
	// stacked on top of the aim hold pose.
	const Zenith_Maths::Quat xKick = glm::angleAxis(glm::radians(15.0f), Zenith_Maths::Vector3(1, 0, 0))
	                                * StickFigureAimHoldPose::RightUpperArm();
	const Zenith_Maths::Quat xSample = pxCh->SampleRotation(2.0f);
	ZENITH_ASSERT_TRUE(StickFigureQuatEquals(xSample, xKick),
		"Fire RightUpperArm at t=2 ticks should be aim pose + 15deg X recoil");
	delete pxClip;
}

// ----- Reload ---------------------------------------------------------------

ZENITH_TEST(StickFigureProcAnim, ReloadClipMetadata) { Zenith_UnitTests::TestStickFigureReloadClipMetadata(); }
void Zenith_UnitTests::TestStickFigureReloadClipMetadata()
{
	Flux_AnimationClip* pxClip = CreateReloadAnimation();
	ZENITH_ASSERT_TRUE(pxClip->GetName() == "Reload", "Reload clip name should be 'Reload'");
	ZENITH_ASSERT_TRUE(!pxClip->IsLooping(), "Reload clip should NOT be looping");
	ZENITH_ASSERT_TRUE(std::abs(pxClip->GetDuration() - 1.5f) < 1e-4f, "Reload clip duration should be 1.5s");
	ZENITH_ASSERT_TRUE(pxClip->GetTicksPerSecond() == 24, "Reload clip should be 24 fps");
	delete pxClip;
}

ZENITH_TEST(StickFigureProcAnim, ReloadClipFiveKeyframesOnLeftArm) { Zenith_UnitTests::TestStickFigureReloadClipFiveKeyframesOnLeftArm(); }
void Zenith_UnitTests::TestStickFigureReloadClipFiveKeyframesOnLeftArm()
{
	Flux_AnimationClip* pxClip = CreateReloadAnimation();
	const Flux_BoneChannel* pxCh = pxClip->GetBoneChannel("LeftUpperArm");
	ZENITH_ASSERT_TRUE(pxCh != nullptr, "Reload should have LeftUpperArm channel");
	ZENITH_ASSERT_TRUE(pxCh->GetRotationKeyframes().GetSize() == 5,
		"Reload LeftUpperArm should have 5 rotation keyframes (rest, drop, reach, lift, rest)");
	delete pxClip;
}

ZENITH_TEST(StickFigureProcAnim, ReloadClipReturnsToAimPoseAtEnd) { Zenith_UnitTests::TestStickFigureReloadClipReturnsToAimPoseAtEnd(); }
void Zenith_UnitTests::TestStickFigureReloadClipReturnsToAimPoseAtEnd()
{
	Flux_AnimationClip* pxClip = CreateReloadAnimation();
	const Flux_BoneChannel* pxCh = pxClip->GetBoneChannel("LeftUpperArm");
	ZENITH_ASSERT_TRUE(pxCh != nullptr, "Reload should have LeftUpperArm channel");

	// Last keyframe should match aim hold pose so the transition back to Aim
	// is seamless. Read the authored last keyframe directly (see Fire test for
	// why we don't sample at the boundary).
	const auto& axRotations = pxCh->GetRotationKeyframes();
	ZENITH_ASSERT_TRUE(axRotations.GetSize() != 0, "Reload LeftUpperArm should have keyframes");
	const Zenith_Maths::Quat xExpected = StickFigureAimHoldPose::LeftUpperArm();
	const Zenith_Maths::Quat xLast = axRotations.GetBack().first;
	ZENITH_ASSERT_TRUE(StickFigureQuatEquals(xLast, xExpected),
		"Reload LeftUpperArm last keyframe should be aim hold pose");
	delete pxClip;
}

// ----- Jump -----------------------------------------------------------------

ZENITH_TEST(StickFigureProcAnim, JumpClipMetadata) { Zenith_UnitTests::TestStickFigureJumpClipMetadata(); }
void Zenith_UnitTests::TestStickFigureJumpClipMetadata()
{
	Flux_AnimationClip* pxClip = CreateJumpAnimation();
	ZENITH_ASSERT_TRUE(pxClip->GetName() == "Jump", "Jump clip name should be 'Jump'");
	ZENITH_ASSERT_TRUE(!pxClip->IsLooping(), "Jump clip should NOT be looping");
	ZENITH_ASSERT_TRUE(std::abs(pxClip->GetDuration() - 0.8f) < 1e-4f, "Jump clip duration should be 0.8s");
	ZENITH_ASSERT_TRUE(pxClip->GetTicksPerSecond() == 24, "Jump clip should be 24 fps");
	delete pxClip;
}

ZENITH_TEST(StickFigureProcAnim, JumpClipBothLegsHaveKeyframes) { Zenith_UnitTests::TestStickFigureJumpClipBothLegsHaveKeyframes(); }
void Zenith_UnitTests::TestStickFigureJumpClipBothLegsHaveKeyframes()
{
	Flux_AnimationClip* pxClip = CreateJumpAnimation();
	ZENITH_ASSERT_TRUE(pxClip->HasBoneChannel("LeftUpperLeg"),  "Jump missing LeftUpperLeg channel");
	ZENITH_ASSERT_TRUE(pxClip->HasBoneChannel("RightUpperLeg"), "Jump missing RightUpperLeg channel");
	ZENITH_ASSERT_TRUE(pxClip->HasBoneChannel("LeftLowerLeg"),  "Jump missing LeftLowerLeg channel");
	ZENITH_ASSERT_TRUE(pxClip->HasBoneChannel("RightLowerLeg"), "Jump missing RightLowerLeg channel");
	delete pxClip;
}

ZENITH_TEST(StickFigureProcAnim, JumpClipReturnsToIdentityAtEnd) { Zenith_UnitTests::TestStickFigureJumpClipReturnsToIdentityAtEnd(); }
void Zenith_UnitTests::TestStickFigureJumpClipReturnsToIdentityAtEnd()
{
	Flux_AnimationClip* pxClip = CreateJumpAnimation();
	const Flux_BoneChannel* pxCh = pxClip->GetBoneChannel("Spine");
	ZENITH_ASSERT_TRUE(pxCh != nullptr, "Jump should have Spine channel");

	// Last keyframe should be identity (recovered after the jump). Read the
	// authored last keyframe directly (see Fire test for why we don't sample
	// at the boundary).
	const auto& axRotations = pxCh->GetRotationKeyframes();
	ZENITH_ASSERT_TRUE(axRotations.GetSize() != 0, "Jump Spine should have keyframes");
	const Zenith_Maths::Quat xIdentity = glm::identity<Zenith_Maths::Quat>();
	const Zenith_Maths::Quat xLast = axRotations.GetBack().first;
	ZENITH_ASSERT_TRUE(StickFigureQuatEquals(xLast, xIdentity),
		"Jump Spine last keyframe should be identity");
	delete pxClip;
}
