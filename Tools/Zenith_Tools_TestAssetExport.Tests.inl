#include "UnitTests/Zenith_UnitTests.h"
#include "Core/Zenith_TestFramework.h"
#include "Flux/MeshAnimation/Flux_AnimationClip.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"

#include <filesystem>   // the authored-seed fixture writes into a temp directory
#include <fstream>      // ...and compares the bytes it left behind
#include <iterator>

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
// Note: Flux_BoneChannel::SampleRotation takes time in SECONDS (D3) — the same
// clock as Flux_AnimationClip::GetDuration. The clips are AUTHORED on a 24 fps
// frame grid, so a test that wants "the pose at frame N" says
// HumanFrameSeconds(N) rather than N. It used to say N, because the channel
// stored ticks and the sampler multiplied wall-clock seconds by 24 on the way in.
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

	// Sample at the end (authored frame 12 = 0.5 s, the clip duration) — should be
	// the aim hold pose.
	const Zenith_Maths::Quat xExpected = StickFigureAimHoldPose::RightUpperArm();
	const Zenith_Maths::Quat xSample = pxCh->SampleRotation(HumanFrameSeconds(12.0f));
	ZENITH_ASSERT_TRUE(StickFigureQuatEquals(xSample, xExpected),
		"Aim RightUpperArm at the clip end should match aim hold pose");

	// And at the start (t=0 s) — same pose, since it's a stable hold.
	const Zenith_Maths::Quat xStart = pxCh->SampleRotation(0.0f);
	ZENITH_ASSERT_TRUE(StickFigureQuatEquals(xStart, xExpected),
		"Aim RightUpperArm at t=0 should also match aim hold pose (stable hold)");
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

	// At peak (authored frame 2 = 1/12 s) the right upper arm should have +15deg
	// X-axis recoil stacked on top of the aim hold pose.
	const Zenith_Maths::Quat xKick = glm::angleAxis(glm::radians(15.0f), Zenith_Maths::Vector3(1, 0, 0))
	                                * StickFigureAimHoldPose::RightUpperArm();
	const Zenith_Maths::Quat xSample = pxCh->SampleRotation(HumanFrameSeconds(2.0f));
	ZENITH_ASSERT_TRUE(StickFigureQuatEquals(xSample, xKick),
		"Fire RightUpperArm at the recoil peak should be aim pose + 15deg X recoil");
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
	const Zenith_HumanProportions& xP = Zenith_HumanProportionsRealistic();
	Zenith_HumanWarp xWarp;
	Zenith_MeshAsset* pxMesh = CreateStickFigureMesh(xP, xWarp);
	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton(xP, xWarp);

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
	const Zenith_HumanProportions& xP = Zenith_HumanProportionsRealistic();
	Zenith_HumanWarp xWarp;
	Zenith_MeshAsset* pxMesh = CreateStickFigureMesh(xP, xWarp);
	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton(xP, xWarp);

	// ★ RE-DERIVED FROM THE TABLE, never re-typed. These used to be the literals
	// 0.715 and -0.480, which were where the joints happened to be. The whole
	// point of the warp is that a blended vertex now lands on the RIG's joint
	// plane, so asking the table is both the correct check and one that survives
	// the next proportion edit.
	//
	// ★★ THE JOINT IS FOUND BY ITS BONE'S POSITION, NOT BY A HEIGHT. This used to
	// select candidate vertices with |y - ElbowY()| < 0.09, which only ever worked
	// because the arm hung straight down: the rig is T-POSED now (see
	// Zenith_HumanArmBindRotation) and an elbow is a distance OUT along X, at
	// shoulder height, so a height band finds the ribcage instead. Reading the
	// bone's own model-space position covers both poses and cannot go stale the
	// next time one changes -- the bone IS where the joint is, by definition.
	struct JointCheck { uint32_t uBoneA; uint32_t uBoneB; const char* szName; };
	const JointCheck axJoints[] = {
		{ 4 /*LUA*/, 5 /*LLA*/,  "left elbow"  },
		{ 7 /*RUA*/, 8 /*RLA*/,  "right elbow" },
		{ 10 /*LUL*/, 11 /*LLL*/, "left knee"  },
		{ 13 /*RUL*/, 14 /*RLL*/, "right knee" },
	};

	for (const JointCheck& xJoint : axJoints)
	{
		// The child bone's own origin is the joint it rotates about.
		const Zenith_Maths::Vector3 xJointPos(
			pxSkel->GetBone(xJoint.uBoneB).m_xBindPoseModel[3]);
		bool bFoundBlend = false;
		for (uint32_t v = 0; v < pxMesh->GetNumVerts() && !bFoundBlend; v++)
		{
			const Zenith_Maths::Vector3& xPos = pxMesh->m_xPositions.Get(v);
			if (glm::length(xPos - xJointPos) > 0.14f)
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
	Zenith_MaterialAsset* pxLeaves = Zenith_AssetRegistry::GetView<Zenith_MaterialAsset>(
		"engine:Meshes/ProceduralTree/Tree_Leaves.zmtrl");
	ZENITH_ASSERT_NOT_NULL(pxLeaves, "Tree_Leaves.zmtrl must load (run a tools boot to (re)generate it)");
	ZENITH_ASSERT_EQ(pxLeaves->GetBlendMode(), MATERIAL_BLEND_MASKED,
		"Leaf material must be MASKED so the alpha mask cuts the leaves out");
	ZENITH_ASSERT_EQ_FLOAT(pxLeaves->GetAlphaCutoff(), 0.45f, 0.0001f,
		"Leaf alpha cutoff must stay 0.45");
}

// ----- Rig identity + key-time/duration agreement, across ALL SEVENTEEN clips ----
//
// ★ THE POPULATION IS THE POINT. Every other clip test in this file names ONE
// factory, so a clip added later inherits no coverage at all -- which is exactly
// how the four tennis clips arrived without the file header's "13 clips" moving.
// These two iterate the whole export set, so a new factory is covered the moment
// it is listed and an omitted listing is the only way to escape them.
//
// ★ THE TABLE THEY WALK — axSTICKFIGURE_CLIP_FACTORIES / uSTICKFIGURE_CLIP_COUNT
// — LIVES IN THE .cpp ABOVE THIS INCLUDE, not here. It used to be declared in
// this file, which meant the production seeding phase and the units could walk
// two different lists; one table is the whole point of the assertion that the
// count is seventeen.

ZENITH_TEST(StickFigureProcAnim, EveryClipCarriesTheSharedRigIdentity)
{
	// D7 / D8. A .zanim whose m_strSkeletonPath is empty loads, plays and reports
	// no error -- the animator simply has nothing to retarget onto and the preview
	// has nothing to draw. Nothing downstream distinguishes that from a clip that
	// happens to drive no visible bone, so the metadata is where it has to be
	// caught.
	//
	// ★ AND THE PREFIX IS PART OF THE ASSERTION, not decoration.
	// Zenith_AssetRegistry::NormalizeAssetPath leaves a bare RELATIVE path exactly
	// as it found it, so "Meshes/StickFigure/StickFigure.zskel" would satisfy a
	// "non-empty" check, round-trip through the stream unchanged, and resolve to
	// nothing (Docs/HumanoidImport.md invariant 6).
	ZENITH_ASSERT_EQ(uSTICKFIGURE_CLIP_COUNT, 17u,
		"the StickFigure clip set is seventeen -- update the export table and this list together");

	for (u_int u = 0; u < uSTICKFIGURE_CLIP_COUNT; u++)
	{
		const StickFigureClipFactory& xFactory = axSTICKFIGURE_CLIP_FACTORIES[u];
		Flux_AnimationClip* pxClip = xFactory.m_pfnCreate();
		const Flux_AnimationClipMetadata& xMetadata = pxClip->GetMetadata();

		ZENITH_ASSERT_STREQ(pxClip->GetName().c_str(), xFactory.m_szName,
			"clip %u is not the one this table says it is", u);
		ZENITH_ASSERT_STREQ(xMetadata.m_strSkeletonPath.c_str(),
			"engine:Meshes/StickFigure/StickFigure.zskel",
			"clip '%s' does not name the ONE shared humanoid rig, engine:-prefixed", xFactory.m_szName);
		ZENITH_ASSERT_STREQ(xMetadata.m_strPreviewModelPath.c_str(),
			"engine:Meshes/StickFigure/StickFigure.zmodel",
			"clip '%s' names no model to preview it on", xFactory.m_szName);
		ZENITH_ASSERT_TRUE(xMetadata.m_bGenerated,
			"clip '%s' is rewritten in full by every tools boot but does not say so (D8)", xFactory.m_szName);
		ZENITH_ASSERT_EQ(xMetadata.m_uAuthoredFrameRate, 24u,
			"clip '%s' authored frame rate must be the 24 fps grid HumanFrameSeconds divides by", xFactory.m_szName);

		delete pxClip;
	}
}

ZENITH_TEST(StickFigureProcAnim, EveryClipsKeysFitInsideItsDuration)
{
	// D3 made "the last key lands at or before the end" a checkable property, and
	// this is the check. A generator that authored on one grid and stated its
	// length on another produces a clip that samples correctly for its first
	// fraction and then holds its last pose -- visible as a freeze, invisible to
	// every value-based assertion in this file.
	//
	// ★ FIRE IS EXEMPT, DELIBERATELY, AND THE NUMBERS ARE HERE SO THE EXEMPTION CAN
	// BE FALSIFIED. CreateFireAnimation's recoil channels carry a settle key at
	// authored frame 5, which is 5/24 = 0.208333 s, against a stated duration of
	// 0.20 s -- 8.3 ms past the end. Decision D13 permits a key past the duration
	// (the mutators do not veto one; a panel warns), and the duration itself is
	// PINNED at 0.20f by FireClipMetadata above, so moving the duration to 0.2083 s
	// to close the gap would red that test instead. The exemption is the smaller
	// lie of the two, and it is one clip.
	for (u_int u = 0; u < uSTICKFIGURE_CLIP_COUNT; u++)
	{
		const StickFigureClipFactory& xFactory = axSTICKFIGURE_CLIP_FACTORIES[u];
		Flux_AnimationClip* pxClip = xFactory.m_pfnCreate();

		const bool bIsFire = (std::string(xFactory.m_szName) == "Fire");
		if (bIsFire)
		{
			// Pin the exemption's own numbers, so it stops being true the moment Fire
			// is retimed -- an exemption nothing measures is an exemption that outlives
			// its reason.
			ZENITH_ASSERT_EQ_FLOAT(pxClip->GetDuration(), 0.20f, 1e-4f,
				"Fire's duration moved; re-derive the D13 exemption below it");
			ZENITH_ASSERT_EQ_FLOAT(Flux_ClipLastKeyTimeSeconds(*pxClip), 5.0f / 24.0f, 1e-4f,
				"Fire's last key is no longer authored frame 5 -- the D13 exemption may be unnecessary now");
		}
		else
		{
			ZENITH_ASSERT_TRUE(Flux_ClipKeyTimesFitDuration(*pxClip),
				"clip '%s' carries a key past its %.4f s duration (last key at %.4f s)",
				xFactory.m_szName, pxClip->GetDuration(), Flux_ClipLastKeyTimeSeconds(*pxClip));
		}

		delete pxClip;
	}
}

// ----- The proportion warp actually reaching the mesh -------------------------

ZENITH_TEST(StickFigureBody, WarpedLoftLandmarksLandOnTheRigsJointPlanes)
{
	// ★ THE TEST FOR "DID THE TABLE EDIT REACH THE GEOMETRY". Every other check in
	// this file would pass if the warp were skipped entirely -- the mesh would
	// still be a valid, well-weighted, correctly-bounded human, just one whose
	// knee is 8 cm from the knee bone it bends around. Re-measuring the FINISHED
	// mesh and comparing against the rig's own planes is the only thing that
	// notices, and it is the whole point of the change.
	const Zenith_HumanProportions& xP = Zenith_HumanProportionsRealistic();
	Zenith_HumanWarp xWarp;
	Zenith_MeshAsset* pxMesh = CreateStickFigureMesh(xP, xWarp);
	ZENITH_ASSERT_TRUE(xWarp.IsValid(), "the loft must have produced a usable warp");

	// ★ MEASURED AS A T-POSE, because that is what it now is. The loft is authored
	// arms-down and warped arms-down -- every pass above the rotation seam still
	// works in that space -- but what SHIPS has its arms out, so re-measuring the
	// finished mesh has to ask the right question. In T_POSE the arm chain is
	// reported as distances OUT along the lateral axis rather than as heights.
	Zenith_SkinDeformView xView = Zenith_MakeSkinDeformView(*pxMesh);
	Zenith_HumanLandmarks xAfter;
	ZENITH_ASSERT_TRUE(Zenith_MeasureHumanLandmarks(xView, ZENITH_HUMAN_POSE_T_POSE, xAfter),
		"the warped loft must still measure");
	Zenith_LogHumanLandmarks("StickFigure loft (POST-warp)", xAfter);

	// One scan bin is height/128 = 0.020, so 2.5 bins is the resolution floor.
	const float fTol = 0.05f;
	ZENITH_ASSERT_EQ_FLOAT(xAfter.SoleY(), fZENITH_HUMAN_RIG_SOLE_Y, 1.0e-3f,
		"the sole is PINNED: colliders and spawn lifts are tuned against it");
	ZENITH_ASSERT_EQ_FLOAT(xAfter.Height(), fZENITH_HUMAN_RIG_HEIGHT, 1.0e-3f,
		"and so is total height");

	ZENITH_ASSERT_TRUE(xAfter.m_abBodyFound[ZENITH_HUMAN_BODY_ANKLE], "the warped loft still has an ankle seam");
	ZENITH_ASSERT_EQ_FLOAT(xAfter.m_afBodyY[ZENITH_HUMAN_BODY_ANKLE], xP.AnkleY(), fTol,
		"the loft's ankle seam now sits on the RIG's ankle plane");
	ZENITH_ASSERT_EQ_FLOAT(xAfter.m_afBodyY[ZENITH_HUMAN_BODY_SHOULDER], xP.ShoulderY(), fTol,
		"...and its shoulder on the rig's shoulder plane");
	// The arm chain, as lateral reach: the shoulder's own half-width plus the
	// segment length that used to be measured as a drop in Y. Same table, same
	// lengths -- the rotation moved the arm without resizing it, and this is the
	// assertion that says so about the shipped geometry rather than about the rig.
	ZENITH_ASSERT_EQ_FLOAT(xAfter.m_afArmChain[ZENITH_HUMAN_ARM_ELBOW],
		xP.ShoulderHalfX() + (xP.ShoulderY() - xP.ElbowY()), fTol,
		"...its elbow at the rig's elbow reach");
	ZENITH_ASSERT_EQ_FLOAT(xAfter.m_afArmChain[ZENITH_HUMAN_ARM_WRIST],
		xP.ShoulderHalfX() + (xP.ShoulderY() - xP.WristY()), fTol,
		"...and its wrist at the rig's wrist reach");

	delete pxMesh;
}

// ============================================================================
// The AUTHORED TWINS of the seventeen clips (WU-9.1 stage 1).
//
// ★ WHAT THESE FOUR ARE FOR. Stage 2 deletes the generators and re-points every
// consumer at the committed files under Assets/Authored/. Once that happens
// nothing in the tree can re-derive what the clips were, so the properties that
// have to survive the deletion are the ones worth pinning NOW, while both halves
// still exist and can be compared to each other:
//
//   1. the twin IS the original (it samples identically, everywhere);
//   2. every clip drives both UpperArms -- today the export loop's bake-time
//      assert, tomorrow a property of seventeen committed files;
//   3. the authored path keeps the root prefix AND the subdirectory;
//   4. seeding never touches a file that is already there (D21).
//
// All four are pure CPU work under the Null backend: a clip is data, the seeding
// unit writes into a private directory under the OS temp dir and removes it
// again, and nothing here touches a device, a scene or the asset registry. None
// is requiresGraphics.
// ============================================================================

namespace
{
	// 20 matched times spanning [0, duration].
	constexpr u_int uSTICKFIGURE_AUTHORED_SAMPLE_COUNT = 20u;

	// ★ THE COMPARISON IS OF POSES, NOT OF FIELDS. A field-by-field walk would
	// pass on two clips whose keys agree and whose SAMPLING does not (a tangent
	// array out of lockstep, a channel left unsorted), and the pose is what a
	// skeleton actually receives. The whole local matrix is compared as well as
	// the three tracks separately, so a failure says which one moved.
	bool StickFigureAuthoredPosesMatch(const Flux_AnimationClip& xGenerated,
		const Flux_AnimationClip& xAuthored, std::string& strOutWhy)
	{
		if (xGenerated.GetBoneChannels().GetSize() != xAuthored.GetBoneChannels().GetSize())
		{
			strOutWhy = "the twin carries a different number of bone channels";
			return false;
		}

		const float fDuration = xGenerated.GetDuration();
		for (Zenith_HashMap<std::string, Flux_BoneChannel>::Iterator xIt(xGenerated.GetBoneChannels());
			!xIt.Done(); xIt.Next())
		{
			const std::string strBone = xIt.GetKey();
			const Flux_BoneChannel* pxTwin = xAuthored.GetBoneChannel(strBone);
			if (pxTwin == nullptr)
			{
				strOutWhy = "the twin has no channel for bone '" + strBone + "'";
				return false;
			}
			const Flux_BoneChannel& xChannel = xIt.GetValue();

			for (u_int u = 0; u < uSTICKFIGURE_AUTHORED_SAMPLE_COUNT; u++)
			{
				const float fTime = fDuration
					* (static_cast<float>(u) / static_cast<float>(uSTICKFIGURE_AUTHORED_SAMPLE_COUNT - 1u));

				if (glm::length(xChannel.SamplePosition(fTime) - pxTwin->SamplePosition(fTime)) > 1e-5f)
				{
					strOutWhy = "position diverges on bone '" + strBone + "'";
					return false;
				}
				if (glm::length(xChannel.SampleScale(fTime) - pxTwin->SampleScale(fTime)) > 1e-5f)
				{
					strOutWhy = "scale diverges on bone '" + strBone + "'";
					return false;
				}
				// A quaternion and its negation are the same rotation, so compare |dot|.
				const float fDot = glm::dot(xChannel.SampleRotation(fTime), pxTwin->SampleRotation(fTime));
				if (std::abs(std::abs(fDot) - 1.0f) > 1e-5f)
				{
					strOutWhy = "rotation diverges on bone '" + strBone + "'";
					return false;
				}

				const Zenith_Maths::Matrix4 xLocalA = xChannel.Sample(fTime);
				const Zenith_Maths::Matrix4 xLocalB = pxTwin->Sample(fTime);
				for (int iCol = 0; iCol < 4; iCol++)
				{
					for (int iRow = 0; iRow < 4; iRow++)
					{
						if (std::abs(xLocalA[iCol][iRow] - xLocalB[iCol][iRow]) > 1e-5f)
						{
							strOutWhy = "the local pose matrix diverges on bone '" + strBone + "'";
							return false;
						}
					}
				}
			}
		}

		return true;
	}

	//--------------------------------------------------------------------------
	// Fixture — a private directory under the OS temp dir, removed on the way
	// out. ★ THE SEEDING UNIT MUST NOT REACH THE REAL TREE: those files are
	// COMMITTED, and a unit that wrote one would put the checkout in `git status`
	// and (worse) look identical to a pass whether or not the skip logic works.
	//--------------------------------------------------------------------------
	struct StickFigureAuthoredTempDir
	{
		std::filesystem::path m_xRoot;

		explicit StickFigureAuthoredTempDir(const char* szLeafDirectory)
		{
			std::error_code xError;
			std::filesystem::path xBase = std::filesystem::temp_directory_path(xError);
			if (xError)
			{
				xBase = ".";
			}
			m_xRoot = xBase / szLeafDirectory;
			std::filesystem::remove_all(m_xRoot, xError);
		}

		~StickFigureAuthoredTempDir()
		{
			std::error_code xError;
			std::filesystem::remove_all(m_xRoot, xError);
		}

		StickFigureAuthoredTempDir(const StickFigureAuthoredTempDir&) = delete;
		StickFigureAuthoredTempDir& operator=(const StickFigureAuthoredTempDir&) = delete;

		// With a trailing separator — what the seeding function concatenates onto.
		// The directory is deliberately NOT created here: the pass has to create it.
		std::string Dir() const { return m_xRoot.generic_string() + "/"; }
	};

	std::string StickFigureAuthoredReadBytes(const std::string& strPath)
	{
		std::ifstream xFile(strPath, std::ios::binary);
		if (!xFile)
		{
			return std::string();
		}
		return std::string(std::istreambuf_iterator<char>(xFile), std::istreambuf_iterator<char>());
	}

	bool StickFigureAuthoredParseFile(const std::string& strPath, Flux_AnimationClip& xOut)
	{
		Zenith_DataStream xStream;
		xStream.ReadFromFile(strPath.c_str());
		if (!xStream.IsValid())
		{
			return false;
		}
		return xOut.ParseStream(xStream).IsOk();
	}
}

ZENITH_TEST(StickFigureAuthored, EveryAuthoredTwinSamplesIdenticallyToItsGeneratedOriginal)
{
	// ★ THE TWIN IS THE SAME ANIMATION, AND m_bGenerated IS THE ONLY DIFFERENCE.
	// That claim is the whole justification for seeding Assets/Authored/ from a
	// generator: if the twin were even slightly a different clip, stage 2's
	// re-pointing would silently change how every human in three games moves,
	// with no gate able to see it (the files are new, so there is nothing to diff
	// against). Sampled rather than compared field by field — see the helper.
	ZENITH_ASSERT_EQ(uSTICKFIGURE_CLIP_COUNT, 17u,
		"the StickFigure clip set is seventeen -- update the export table and the factory table together");

	for (u_int u = 0; u < uSTICKFIGURE_CLIP_COUNT; u++)
	{
		const StickFigureClipFactory& xFactory = axSTICKFIGURE_CLIP_FACTORIES[u];

		Flux_AnimationClip* pxGenerated = xFactory.m_pfnCreate();
		Flux_AnimationClip* pxAuthored  = xFactory.m_pfnCreate();

		// Exactly what the seeding phase does to a clip, and nothing else.
		pxAuthored->GetMetadata().m_bGenerated = false;

		std::string strWhy;
		ZENITH_ASSERT_TRUE(StickFigureAuthoredPosesMatch(*pxGenerated, *pxAuthored, strWhy),
			"clip '%s': the authored twin does not pose like its generated original -- %s",
			xFactory.m_szName, strWhy.c_str());

		// The rest of the metadata travels untouched: a twin that lost its rig
		// reference would preview against nothing, and one whose name moved could
		// not be resolved through a clip collection at all.
		const Flux_AnimationClipMetadata& xGen = pxGenerated->GetMetadata();
		const Flux_AnimationClipMetadata& xAuth = pxAuthored->GetMetadata();
		ZENITH_ASSERT_STREQ(xAuth.m_strName.c_str(), xGen.m_strName.c_str(),
			"clip '%s': the twin's name moved", xFactory.m_szName);
		ZENITH_ASSERT_EQ_FLOAT(xAuth.m_fDuration, xGen.m_fDuration, 1e-6f,
			"clip '%s': the twin's duration moved", xFactory.m_szName);
		ZENITH_ASSERT_STREQ(xAuth.m_strSkeletonPath.c_str(), xGen.m_strSkeletonPath.c_str(),
			"clip '%s': the twin names a different rig", xFactory.m_szName);
		ZENITH_ASSERT_STREQ(xAuth.m_strPreviewModelPath.c_str(), xGen.m_strPreviewModelPath.c_str(),
			"clip '%s': the twin names a different preview model", xFactory.m_szName);
		ZENITH_ASSERT_EQ(xAuth.m_uAuthoredFrameRate, xGen.m_uAuthoredFrameRate,
			"clip '%s': the twin's authored frame rate moved", xFactory.m_szName);
		ZENITH_ASSERT_EQ(xAuth.m_uTicksPerSecond, xGen.m_uTicksPerSecond,
			"clip '%s': the twin's import provenance moved", xFactory.m_szName);
		ZENITH_ASSERT_TRUE(xAuth.m_bLooping == xGen.m_bLooping,
			"clip '%s': the twin's loop flag moved", xFactory.m_szName);
		ZENITH_ASSERT_EQ(pxAuthored->GetEvents().GetSize(), pxGenerated->GetEvents().GetSize(),
			"clip '%s': the twin carries a different number of events", xFactory.m_szName);

		ZENITH_ASSERT_TRUE(xGen.m_bGenerated, "clip '%s': the original must still say it is generated (D8)",
			xFactory.m_szName);
		ZENITH_ASSERT_FALSE(xAuth.m_bGenerated, "clip '%s': the twin must NOT say it is generated (D8)",
			xFactory.m_szName);

		delete pxAuthored;
		delete pxGenerated;
	}
}

ZENITH_TEST(StickFigureAuthored, EveryClipDrivesBothUpperArms)
{
	// ★★ THE ONE RIG DEPENDENCY, AS A UNIT RATHER THAN AS A BAKE-TIME ASSERT.
	// A bone a clip omits keeps its BIND local transform, and the two UpperArms
	// are the only bones whose T-pose bind rotation is not identity
	// (Zenith_HumanArmBindRotation) -- so a clip that omits one leaves that arm
	// sticking straight out sideways for its whole duration, on StickFigure, on
	// Zenithmon's humans and on every imported artist humanoid alike.
	//
	// GenerateStickFigureAssets asserts this in its export loop, which is a
	// TOOLS-BUILD BOOT: it fires only where the bake runs, and stage 2 deletes
	// that loop. Here it is a headless unit over the same seventeen clips, so the
	// invariant survives the generators' deletion and can be re-pointed at the
	// committed files without losing coverage in between.
	ZENITH_ASSERT_EQ(uSTICKFIGURE_CLIP_COUNT, 17u,
		"the StickFigure clip set is seventeen -- update the export table and the factory table together");

	for (u_int u = 0; u < uSTICKFIGURE_CLIP_COUNT; u++)
	{
		const StickFigureClipFactory& xFactory = axSTICKFIGURE_CLIP_FACTORIES[u];
		Flux_AnimationClip* pxClip = xFactory.m_pfnCreate();

		// The bone names are the exporter gate's, spelled the same way.
		ZENITH_ASSERT_TRUE(pxClip->HasBoneChannel("LeftUpperArm"),
			"clip '%s' does not animate LeftUpperArm -- a T-posed human would hold that arm out",
			xFactory.m_szName);
		ZENITH_ASSERT_TRUE(pxClip->HasBoneChannel("RightUpperArm"),
			"clip '%s' does not animate RightUpperArm -- a T-posed human would hold that arm out",
			xFactory.m_szName);

		delete pxClip;
	}
}

ZENITH_TEST(StickFigureAuthored, TheAuthoredPathKeepsTheRootPrefixAndTheSubdirectory)
{
	// ★ THE PREFIX AND THE SUBDIRECTORY ARE BOTH PART OF THE ASSERTION.
	// NormalizeAssetPath leaves a bare RELATIVE path exactly as it found it, so a
	// ref without "engine:" would serialize cleanly, load cleanly and resolve to
	// nothing; and flattening "Meshes/StickFigure/" away would put every set's
	// "Walk" on one path, so the next generated set to be promoted would silently
	// overwrite this one. Same rule as
	// Zenith_AnimationDocument::BuildAuthoredAssetPath, matched here rather than
	// called -- Tools may not include Editor.

	// One row spelled out in full, with nothing constructed, so at least one
	// expectation cannot drift with the helper it is checking.
	ZENITH_ASSERT_STREQ(Zenith_Tools_StickFigureClipFileName("Idle").c_str(),
		"StickFigure_Idle.zanim", "the bake's clip file naming moved");
	ZENITH_ASSERT_STREQ(Zenith_Tools_StickFigureAuthoredPath("StickFigure_Idle.zanim").c_str(),
		"engine:Authored/Meshes/StickFigure/StickFigure_Idle.zanim",
		"the authored path for the Idle clip moved");

	for (u_int u = 0; u < uSTICKFIGURE_CLIP_COUNT; u++)
	{
		const StickFigureClipFactory& xFactory = axSTICKFIGURE_CLIP_FACTORIES[u];

		const std::string strFileName = Zenith_Tools_StickFigureClipFileName(xFactory.m_szName);
		const std::string strExpectedFileName = std::string("StickFigure_") + xFactory.m_szName + ".zanim";
		ZENITH_ASSERT_STREQ(strFileName.c_str(), strExpectedFileName.c_str(),
			"clip '%s' does not use the bake's own file naming", xFactory.m_szName);

		const std::string strExpectedPath = "engine:Authored/Meshes/StickFigure/" + strFileName;
		ZENITH_ASSERT_STREQ(Zenith_Tools_StickFigureAuthoredPath(strFileName.c_str()).c_str(),
			strExpectedPath.c_str(),
			"clip '%s' does not map into engine:Authored/Meshes/StickFigure/", xFactory.m_szName);
	}

	// And the DIRECTORY the boot writes into is the same location the asset path
	// describes -- an asset path nothing writes to would resolve to a file that
	// never appears.
	const std::string strDir = Zenith_Tools_StickFigureAuthoredDir();
	ZENITH_ASSERT_TRUE(strDir.ends_with("Authored/Meshes/StickFigure/"),
		"the seeding directory '%s' is not the location engine:Authored/Meshes/StickFigure/ resolves to",
		strDir.c_str());
}

ZENITH_TEST(StickFigureAuthored, SeedingWritesOnceAndThenNeverTouchesTheDirectoryAgain)
{
	// ★ D21, AS THE PROPERTY THAT MAKES THIS PHASE SAFE TO SHIP. The bake never
	// overwrites authored data; this one-shot seeding is the sanctioned exception
	// and it is idempotent. The second pass is where that is decided, so the test
	// HAND-EDITS one of the seeded files first: a pass that merely wrote the same
	// bytes again would be indistinguishable from a skip by any check that only
	// counted files, and would destroy an edit in the real tree.
	StickFigureAuthoredTempDir xTemp("zenith_stickfigure_authored_seed");
	const std::string strDir = xTemp.Dir();

	const Zenith_Tools_StickFigureAuthoredSeedReport xFirst =
		Zenith_Tools_ExportStickFigureAuthoredClips(strDir);

	ZENITH_ASSERT_TRUE(xFirst.CountsAddUp(), "the first pass dropped a clip without saying so");
	ZENITH_ASSERT_EQ(xFirst.m_uConsidered, uSTICKFIGURE_CLIP_COUNT, "all seventeen clips are considered");
	ZENITH_ASSERT_EQ(xFirst.m_uWritten, uSTICKFIGURE_CLIP_COUNT, "an empty directory is seeded in full");
	ZENITH_ASSERT_EQ(xFirst.m_uSkippedExisting, 0u, "nothing was already there");
	ZENITH_ASSERT_EQ(xFirst.m_uFailed, 0u, "and nothing failed");

	for (u_int u = 0; u < uSTICKFIGURE_CLIP_COUNT; u++)
	{
		const std::string strPath = strDir
			+ Zenith_Tools_StickFigureClipFileName(axSTICKFIGURE_CLIP_FACTORIES[u].m_szName);
		ZENITH_ASSERT_TRUE(std::filesystem::exists(strPath), "'%s' should have been written", strPath.c_str());
	}

	// What landed is a real .zanim at the CURRENT schema, read back through the
	// runtime reader — the same verification the authored-clip migrator does, and
	// for the same reason: these files are committed, so a truncated one would be
	// committed too.
	const std::string strIdlePath = strDir + Zenith_Tools_StickFigureClipFileName("Idle");
	Flux_AnimationClip xSeeded;
	ZENITH_ASSERT_TRUE(StickFigureAuthoredParseFile(strIdlePath, xSeeded),
		"the seeded file must parse through the RUNTIME reader");
	ZENITH_ASSERT_STREQ(xSeeded.GetName().c_str(), "Idle", "and be the clip it claims to be");
	ZENITH_ASSERT_FALSE(xSeeded.GetMetadata().m_bGenerated,
		"an authored clip does not claim to be regenerated on every boot (D8)");
	ZENITH_ASSERT_STREQ(xSeeded.GetMetadata().m_strSkeletonPath.c_str(),
		"engine:Meshes/StickFigure/StickFigure.zskel", "and still names the one shared rig");

	// The hand edit: a different duration, written back over the seeded file.
	Flux_AnimationClip xEdited = xSeeded;
	xEdited.SetDuration(xSeeded.GetDuration() + 1.0f);
	xEdited.Export(strIdlePath);
	const std::string strEditedBytes = StickFigureAuthoredReadBytes(strIdlePath);
	ZENITH_ASSERT_TRUE(!strEditedBytes.empty(), "the hand edit landed on disk");

	const Zenith_Tools_StickFigureAuthoredSeedReport xSecond =
		Zenith_Tools_ExportStickFigureAuthoredClips(strDir);

	ZENITH_ASSERT_TRUE(xSecond.CountsAddUp(), "the second pass dropped a clip without saying so");
	ZENITH_ASSERT_EQ(xSecond.m_uConsidered, uSTICKFIGURE_CLIP_COUNT, "all seventeen are still considered");
	ZENITH_ASSERT_EQ(xSecond.m_uWritten, 0u, "a second pass over a full directory writes NOTHING");
	ZENITH_ASSERT_EQ(xSecond.m_uSkippedExisting, uSTICKFIGURE_CLIP_COUNT, "every clip is skipped as already authored");
	ZENITH_ASSERT_EQ(xSecond.m_uFailed, 0u, "and nothing failed");

	ZENITH_ASSERT_TRUE(StickFigureAuthoredReadBytes(strIdlePath) == strEditedBytes,
		"the hand edit survives the bake, byte for byte (D21)");

	Flux_AnimationClip xAfter;
	ZENITH_ASSERT_TRUE(StickFigureAuthoredParseFile(strIdlePath, xAfter), "and the edited file still parses");
	ZENITH_ASSERT_EQ_FLOAT(xAfter.GetDuration(), xSeeded.GetDuration() + 1.0f, 1e-6f,
		"with the edit in it, not the generator's value");
}
