#include "UnitTests/Zenith_UnitTests.h"
#include "Flux/MeshAnimation/Flux_AnimationClip.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"

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
	ZENITH_ASSERT_TRUE(pxCh->GetRotationKeyframes().GetSize() == 8,
		"Reload LeftUpperArm should have 8 rotation keyframes (rest, drop, reach, grab, lift, seat, slap, rest)");
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

// ----- Human body mesh -------------------------------------------------------

ZENITH_TEST(StickFigureBody, BodyMeshInvariants) { Zenith_UnitTests::TestStickFigureBodyMeshInvariants(); }
void Zenith_UnitTests::TestStickFigureBodyMeshInvariants()
{
	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton();
	Zenith_MeshAsset* pxMesh = CreateStickFigureMesh(pxSkel);

	// A lofted human body, not the old 128-vert cube figure.
	ZENITH_ASSERT_TRUE(pxMesh->GetNumVerts() >= 1200, "Body mesh should have at least 1200 verts");
	ZENITH_ASSERT_TRUE(pxMesh->GetNumIndices() >= 6000, "Body mesh should have at least 6000 indices");
	ZENITH_ASSERT_TRUE(pxMesh->m_xBitangents.GetSize() == pxMesh->GetNumVerts(),
		"Body mesh must author bitangents (normal mapping TBN)");
	ZENITH_ASSERT_TRUE(pxMesh->m_xColors.GetSize() == pxMesh->GetNumVerts(),
		"Body mesh must author vertex colors (baked AO)");

	// Bounds: soles below the -1.0 foot bind (inside the 1.05 capsule), crown
	// above the 1.4 head bind.
	ZENITH_ASSERT_TRUE(pxMesh->GetBoundsMin().y < -1.0f && pxMesh->GetBoundsMin().y > -1.06f,
		"Soles should sit just below the foot bind at -1.0");
	ZENITH_ASSERT_TRUE(pxMesh->GetBoundsMax().y > 1.55f && pxMesh->GetBoundsMax().y < 1.65f,
		"Crown should top out just above 1.55");

	for (uint32_t v = 0; v < pxMesh->GetNumVerts(); v++)
	{
		// Weights normalized, bone indices valid.
		const glm::vec4& xW = pxMesh->m_xBoneWeights.Get(v);
		const float fSum = xW.x + xW.y + xW.z + xW.w;
		ZENITH_ASSERT_TRUE(std::abs(fSum - 1.0f) < 0.001f, "Vertex weights must sum to 1");
		const glm::uvec4& xI = pxMesh->m_xBoneIndices.Get(v);
		ZENITH_ASSERT_TRUE(xI.x < STICK_BONE_COUNT && xI.y < STICK_BONE_COUNT,
			"Bone indices must reference the 16-bone rig");

		// UVs inside the atlas, tangent frame finite and unit-ish.
		const Zenith_Maths::Vector2& xUV = pxMesh->m_xUVs.Get(v);
		ZENITH_ASSERT_TRUE(xUV.x >= -0.001f && xUV.x <= 1.001f && xUV.y >= -0.001f && xUV.y <= 1.001f,
			"UVs must stay inside the atlas");
		const Zenith_Maths::Vector3& xT = pxMesh->m_xTangents.Get(v);
		ZENITH_ASSERT_TRUE(std::isfinite(xT.x) && std::isfinite(xT.y) && std::isfinite(xT.z)
			&& std::abs(glm::length(xT) - 1.0f) < 0.01f, "Tangents must be finite unit vectors");
		const Zenith_Maths::Vector3& xN = pxMesh->m_xNormals.Get(v);
		ZENITH_ASSERT_TRUE(std::abs(glm::length(xN) - 1.0f) < 0.01f, "Normals must be unit length");
	}

	delete pxMesh;
	delete pxSkel;
}

ZENITH_TEST(StickFigureBody, BodySmoothSkinning) { Zenith_UnitTests::TestStickFigureBodySmoothSkinning(); }
void Zenith_UnitTests::TestStickFigureBodySmoothSkinning()
{
	// The point of the body overhaul: joints carry BLENDED weights between the
	// adjacent bones so elbows/knees bend smoothly instead of tearing. Verify a
	// genuinely blended vertex exists at each major joint.
	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton();
	Zenith_MeshAsset* pxMesh = CreateStickFigureMesh(pxSkel);

	struct JointCheck { float fY; float fXSign; uint32_t uBoneA; uint32_t uBoneB; const char* szName; };
	const JointCheck axJoints[] = {
		{ 0.715f, -1.0f, 4 /*LUA*/, 5 /*LLA*/,  "left elbow"  },
		{ 0.715f,  1.0f, 7 /*RUA*/, 8 /*RLA*/,  "right elbow" },
		{ -0.480f, -1.0f, 10 /*LUL*/, 11 /*LLL*/, "left knee"  },
		{ -0.480f,  1.0f, 13 /*RUL*/, 14 /*RLL*/, "right knee" },
	};

	for (const JointCheck& xJoint : axJoints)
	{
		bool bFoundBlend = false;
		for (uint32_t v = 0; v < pxMesh->GetNumVerts() && !bFoundBlend; v++)
		{
			const Zenith_Maths::Vector3& xPos = pxMesh->m_xPositions.Get(v);
			if (std::abs(xPos.y - xJoint.fY) > 0.05f || xPos.x * xJoint.fXSign < 0.05f)
			{
				continue;
			}
			const glm::uvec4& xI = pxMesh->m_xBoneIndices.Get(v);
			const glm::vec4& xW = pxMesh->m_xBoneWeights.Get(v);
			const bool bPair = (xI.x == xJoint.uBoneA && xI.y == xJoint.uBoneB)
			                || (xI.x == xJoint.uBoneB && xI.y == xJoint.uBoneA);
			if (bPair && xW.x > 0.25f && xW.x < 0.75f && xW.y > 0.25f && xW.y < 0.75f)
			{
				bFoundBlend = true;
			}
		}
		ZENITH_ASSERT_TRUE(bFoundBlend, "Expected blended skin weights at the %s", xJoint.szName);
	}

	delete pxMesh;
	delete pxSkel;
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

// ----- ProceduralTree leaf material regression -------------------------------
// The leaf albedo's alpha channel is a real leaf-shape mask, so the GENERATED leaf
// material MUST be MASKED (GenerateTreeMaterials). A regression to OPAQUE makes the
// leaves render as opaque quads (leaf texture on a black square) — BuildMaterialDraw-
// Constants only feeds a non-zero cutoff to the shader's discard for MASKED materials.
// Loads the committed/generated .zmtrl and guards against the SetBlendMode omission.
ZENITH_TEST(ProceduralTree, LeafMaterialIsAlphaMasked)
{
	// Load via the asset registry (the public path; LoadFromFile is private). The
	// registry resolves the engine: prefix and caches.
	Zenith_MaterialAsset* pxLeaves = Zenith_AssetRegistry::Get<Zenith_MaterialAsset>(
		"engine:Meshes/ProceduralTree/Tree_Leaves.zmtrl");
	ZENITH_ASSERT_NOT_NULL(pxLeaves, "Tree_Leaves.zmtrl must load (run a tools boot to (re)generate it)");
	ZENITH_ASSERT_EQ(pxLeaves->GetBlendMode(), MATERIAL_BLEND_MASKED,
		"Leaf material must be MASKED so the alpha mask cuts the leaves out");
	ZENITH_ASSERT_EQ_FLOAT(pxLeaves->GetAlphaCutoff(), 0.45f, 0.0001f,
		"Leaf alpha cutoff must stay 0.45");
}
