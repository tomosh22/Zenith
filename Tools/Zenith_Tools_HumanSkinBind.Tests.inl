#include "UnitTests/Zenith_UnitTests.h"

// ============================================================================
// Zenith_Tools_HumanSkinBind tests -- synthetic, tools-only, no asset on disk.
//
// The subject is a capsule-limbed T-posed body with PLANTED landmarks, which is
// what makes "did the scan find them" and "did the solver gate the armpit"
// answerable without shipping art. The real .glb is covered separately, and
// skipped when it is absent.
// ============================================================================

namespace
{
	struct BindSynthMesh
	{
		Zenith_MeshAsset m_xMesh;
	};

	// A capsule along an arbitrary axis, tessellated coarsely. Enough geometry for
	// the scans to bin and for the solver to have neighbours.
	void BindAddCapsule(Zenith_MeshAsset& xMesh, const Zenith_Maths::Vector3& xA,
		const Zenith_Maths::Vector3& xB, float fRadius, u_int uRings = 10u, u_int uSegs = 12u)
	{
		const Zenith_Maths::Vector3 xAxis = xB - xA;
		const float fLen = glm::length(xAxis);
		if (fLen <= 1.0e-6f) { return; }
		const Zenith_Maths::Vector3 xDir = xAxis / fLen;
		Zenith_Maths::Vector3 xUp = (std::fabs(xDir.y) > 0.9f)
			? Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f) : Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);
		const Zenith_Maths::Vector3 xU = glm::normalize(glm::cross(xUp, xDir));
		const Zenith_Maths::Vector3 xV = glm::cross(xDir, xU);

		const uint32_t uBase = xMesh.GetNumVerts();
		for (u_int r = 0u; r <= uRings; ++r)
		{
			const float fT = static_cast<float>(r) / static_cast<float>(uRings);
			// Slight barrel so the ends are narrower than the middle: that is what
			// gives a limb an interior radius minimum where two of them meet.
			const float fR = fRadius * (0.55f + 0.45f * std::sin(3.14159265f * fT));
			const Zenith_Maths::Vector3 xC = xA + xAxis * fT;
			for (u_int s = 0u; s < uSegs; ++s)
			{
				const float fA = (6.28318530718f * static_cast<float>(s)) / static_cast<float>(uSegs);
				const Zenith_Maths::Vector3 xN = xU * std::cos(fA) + xV * std::sin(fA);
				xMesh.AddVertex(xC + xN * fR, xN, Zenith_Maths::Vector2(fT, static_cast<float>(s) / uSegs));
			}
		}
		for (u_int r = 0u; r < uRings; ++r)
		{
			for (u_int s = 0u; s < uSegs; ++s)
			{
				const uint32_t u0 = uBase + r * uSegs + s;
				const uint32_t u1 = uBase + r * uSegs + ((s + 1u) % uSegs);
				const uint32_t u2 = u0 + uSegs;
				const uint32_t u3 = u1 + uSegs;
				xMesh.AddTriangle(u0, u2, u1);
				xMesh.AddTriangle(u1, u2, u3);
			}
		}
	}

	// Planted at fractions of a 2.6012-unit body so the expected values are the
	// proportions table's own.
	struct BindPlanted
	{
		float fSole = -1.0404f, fCrown = 1.5608f;
		float fAnkle = -0.845f, fKnee = -0.4225f, fHip = 0.0f;
		float fShoulderY = 0.9716f;
		float fShoulderX = 0.3207f, fElbowX = 0.6628f, fWristX = 1.0049f, fTipX = 1.2705f;
		float fHipHalfX = 0.15f;
	};

	void BindBuildSynthTPoseBody(Zenith_MeshAsset& xMesh, const BindPlanted& xP)
	{
		xMesh.Reset();
		// ★ THE TORSO IS AS WIDE AS THE SHOULDER JOINT IS FAR OUT, and the arm
		// STARTS INSIDE IT. A fixture with a gap between the two is not a body:
		// the scan sweeping inward along the limb finds empty bins and then the
		// torso, so it reports the torso's own half-width and the planted shoulder
		// is unreachable. Real deltoids overlap the ribcage; so does this one.
		BindAddCapsule(xMesh, Zenith_Maths::Vector3(0.0f, xP.fHip - 0.12f, 0.0f),
			Zenith_Maths::Vector3(0.0f, 1.15f, 0.0f), 0.30f, 16u, 16u);
		BindAddCapsule(xMesh, Zenith_Maths::Vector3(0.0f, 1.19f, 0.0f),
			Zenith_Maths::Vector3(0.0f, xP.fCrown, 0.0f), 0.10f, 10u, 16u);
		for (int iSide = 0; iSide < 2; ++iSide)
		{
			const float fS = (iSide == 0) ? -1.0f : 1.0f;
			// Arms straight out along X, centred on the shoulder line.
			BindAddCapsule(xMesh, Zenith_Maths::Vector3(fS * 0.20f, xP.fShoulderY, 0.0f),
				Zenith_Maths::Vector3(fS * xP.fElbowX, xP.fShoulderY, 0.0f), 0.055f);
			BindAddCapsule(xMesh, Zenith_Maths::Vector3(fS * xP.fElbowX, xP.fShoulderY, 0.0f),
				Zenith_Maths::Vector3(fS * xP.fWristX, xP.fShoulderY, 0.0f), 0.042f);
			BindAddCapsule(xMesh, Zenith_Maths::Vector3(fS * xP.fWristX, xP.fShoulderY, 0.0f),
				Zenith_Maths::Vector3(fS * xP.fTipX, xP.fShoulderY, 0.0f), 0.050f);
			// Legs, with a foot block forward of the ankle.
			BindAddCapsule(xMesh, Zenith_Maths::Vector3(fS * xP.fHipHalfX, xP.fHip + 0.05f, 0.0f),
				Zenith_Maths::Vector3(fS * xP.fHipHalfX, xP.fKnee, 0.0f), 0.095f, 12u, 14u);
			BindAddCapsule(xMesh, Zenith_Maths::Vector3(fS * xP.fHipHalfX, xP.fKnee, 0.0f),
				Zenith_Maths::Vector3(fS * xP.fHipHalfX, xP.fAnkle, 0.0f), 0.070f, 12u, 14u);
			BindAddCapsule(xMesh, Zenith_Maths::Vector3(fS * xP.fHipHalfX, xP.fSole + 0.03f, -0.07f),
				Zenith_Maths::Vector3(fS * xP.fHipHalfX, xP.fSole + 0.03f, 0.16f), 0.055f, 8u, 12u);
		}
		xMesh.ComputeBounds();
	}

	bool BindMeasureSynth(Zenith_MeshAsset& xMesh, Zenith_HumanLandmarks& xOut)
	{
		Zenith_SkinDeformView xView = Zenith_MakeSkinDeformView(xMesh);
		return Zenith_MeasureHumanLandmarks(xView, ZENITH_HUMAN_POSE_T_POSE, xOut);
	}
}

//------------------------------------------------------------------------------
// Normalisation
//------------------------------------------------------------------------------

ZENITH_TEST(HumanSkinBind, NormaliseLandsSoleAndHeightOnTheRig)
{
	Zenith_MeshAsset xMesh;
	BindBuildSynthTPoseBody(xMesh, BindPlanted());
	// Pretend the artist authored it a tenth of the size and facing +X.
	for (u_int v = 0u; v < xMesh.GetNumVerts(); ++v) { xMesh.m_xPositions.Get(v) *= 0.1f; }

	ZENITH_ASSERT_TRUE(Zenith_Tools_HumanSkinBind::NormaliseMeshIntoRigSpace(xMesh, -90.0f),
		"normalise must run");

	float fMin = xMesh.m_xPositions.Get(0u).y, fMax = fMin;
	for (u_int v = 1u; v < xMesh.GetNumVerts(); ++v)
	{
		fMin = std::min(fMin, xMesh.m_xPositions.Get(v).y);
		fMax = std::max(fMax, xMesh.m_xPositions.Get(v).y);
	}
	ZENITH_ASSERT_EQ_FLOAT(fMin, fZENITH_HUMAN_RIG_SOLE_Y, 1.0e-4f, "the sole lands on the rig's sole plane");
	ZENITH_ASSERT_EQ_FLOAT(fMax - fMin, fZENITH_HUMAN_RIG_HEIGHT, 1.0e-4f, "and the body is the rig's height");
}

ZENITH_TEST(HumanSkinBind, OrientationIsRecoveredFromAnyCardinalYaw)
{
	// ★★ THE ONE ERROR A SCREENSHOT PASS DOES NOT CATCH. A model imported 180
	// degrees round looks like a model: at head-thumbnail size the back of a head
	// reads as a face, which is exactly how the first version of this asset shipped
	// backwards past a review. So the importer measures its own answer, and this
	// pins that the measurement is right from all four cardinal starting yaws --
	// including the two where the arm span starts on Z rather than X.
	for (int iYaw = 0; iYaw < 4; ++iYaw)
	{
		const float fSourceYaw = 90.0f * static_cast<float>(iYaw);
		Zenith_MeshAsset xMesh;
		BindBuildSynthTPoseBody(xMesh, BindPlanted());

		const Zenith_Maths::Quat xTurn = glm::angleAxis(
			glm::radians(fSourceYaw), Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
		for (u_int v = 0u; v < xMesh.GetNumVerts(); ++v)
		{
			xMesh.m_xPositions.Get(v) = xTurn * xMesh.m_xPositions.Get(v);
		}

		float fYawApplied = 0.0f;
		ZENITH_ASSERT_TRUE(Zenith_Tools_HumanSkinBind::DetectAndNormaliseIntoRigSpace(xMesh, fYawApplied),
			"the importer must orient a body presented at any cardinal yaw");

		Zenith_HumanLandmarks xL;
		ZENITH_ASSERT_TRUE(BindMeasureSynth(xMesh, xL), "measure the oriented body");
		ZENITH_ASSERT_TRUE(xL.m_bFootFacingMeasured, "the facing must be measurable after orienting");
		ZENITH_ASSERT_TRUE(xL.m_fFacingSign > 0.0f,
			"and every one of the four must end up facing the engine's +Z, not just the easy two");

		// The arm span has to end up on X, or the solver's left/right gates cut the
		// body front-to-back instead of side-to-side.
		Zenith_Maths::Vector3 xMin = xMesh.m_xPositions.Get(0u);
		Zenith_Maths::Vector3 xMax = xMin;
		for (u_int v = 1u; v < xMesh.GetNumVerts(); ++v)
		{
			xMin = glm::min(xMin, xMesh.m_xPositions.Get(v));
			xMax = glm::max(xMax, xMesh.m_xPositions.Get(v));
		}
		ZENITH_ASSERT_TRUE((xMax.x - xMin.x) > (xMax.z - xMin.z),
			"the arm span lands on X");
	}
}

ZENITH_TEST(HumanSkinBind, OrientingPreservesWinding)
{
	// ★ EVERY CORRECTION THE ORIENTER APPLIES IS A PROPER ROTATION, so the model
	// can never come out inside-out. That matters because an inverted mesh reads as
	// "the character is invisible from the front", which is diagnosed as a
	// completely different bug -- see the winding fix in LoadGlbMesh.
	auto SignedVolume = [](const Zenith_MeshAsset& xM)
	{
		double dSum = 0.0;
		for (u_int i = 0u; i + 2u < xM.m_xIndices.GetSize(); i += 3u)
		{
			const Zenith_Maths::Vector3& a = xM.m_xPositions.Get(xM.m_xIndices.Get(i));
			const Zenith_Maths::Vector3& b = xM.m_xPositions.Get(xM.m_xIndices.Get(i + 1u));
			const Zenith_Maths::Vector3& c = xM.m_xPositions.Get(xM.m_xIndices.Get(i + 2u));
			dSum += glm::dot(a, glm::cross(b, c));
		}
		return dSum;
	};

	Zenith_MeshAsset xRef;
	BindBuildSynthTPoseBody(xRef, BindPlanted());
	const double dRef = SignedVolume(xRef);

	for (int iYaw = 0; iYaw < 4; ++iYaw)
	{
		Zenith_MeshAsset xMesh;
		BindBuildSynthTPoseBody(xMesh, BindPlanted());
		const Zenith_Maths::Quat xTurn = glm::angleAxis(
			glm::radians(90.0f * static_cast<float>(iYaw)), Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
		for (u_int v = 0u; v < xMesh.GetNumVerts(); ++v)
		{
			xMesh.m_xPositions.Get(v) = xTurn * xMesh.m_xPositions.Get(v);
		}
		float fYawApplied = 0.0f;
		Zenith_Tools_HumanSkinBind::DetectAndNormaliseIntoRigSpace(xMesh, fYawApplied);
		ZENITH_ASSERT_TRUE(SignedVolume(xMesh) * dRef > 0.0,
			"orienting must never flip the winding handedness");
	}
}

//------------------------------------------------------------------------------
// Landmarks and sanity
//------------------------------------------------------------------------------

ZENITH_TEST(HumanSkinBind, LandmarkScanFindsPlantedTPoseLandmarks)
{
	const BindPlanted xP;
	Zenith_MeshAsset xMesh;
	BindBuildSynthTPoseBody(xMesh, xP);
	Zenith_HumanLandmarks xL;
	ZENITH_ASSERT_TRUE(BindMeasureSynth(xMesh, xL), "a T-posed body must measure");
	Zenith_LogHumanLandmarks("synthetic T-pose", xL);

	const float fTol = 0.02f * xL.Height();   // within 2% of height
	ZENITH_ASSERT_EQ_FLOAT(xL.m_afBodyY[ZENITH_HUMAN_BODY_SHOULDER], xP.fShoulderY, fTol,
		"the shoulder is the T-posed arm's own centreline");
	ZENITH_ASSERT_TRUE(xL.m_bArmChainFound, "arm chain");
	ZENITH_ASSERT_EQ_FLOAT(xL.m_afArmChain[ZENITH_HUMAN_ARM_FINGERTIP], xP.fTipX, fTol, "fingertip");
	// ★ THE BOUNDED WRIST SEARCH AGAIN, in the other pose: an unbounded minimum
	// over the outer arm finds the tapering hand tip, not the wrist.
	ZENITH_ASSERT_EQ_FLOAT(xL.m_afArmChain[ZENITH_HUMAN_ARM_WRIST], xP.fWristX, 2.0f * fTol, "planted wrist");
	// The scan reports where the arm TUBE meets the torso, which for a body whose
	// ribcage is 0.30 wide is 0.30-ish -- and for the real male is 0.315 against a
	// 0.313 torso half-width. That agreement is the check.
	ZENITH_ASSERT_TRUE(xL.m_fShoulderHalfX > 0.22f && xL.m_fShoulderHalfX < 0.42f,
		"the shoulder joint sits where the arm tube meets the torso");
	ZENITH_ASSERT_TRUE(xL.m_abBodyFound[ZENITH_HUMAN_BODY_ANKLE], "a body with a foot block has an ankle");
}

ZENITH_TEST(HumanSkinBind, ArmChainSanityBoundsRejectAMitten)
{
	// ★ THE FAILURE THIS EXISTS FOR. A landmark that catches a chunky mitten
	// instead of a wrist reports a hand half the length of the forearm -- and a
	// rig built from it would ship, because nothing else in the build measures a
	// hand. Refusing is the only way it becomes visible.
	Zenith_HumanLandmarks xL;
	xL.m_bValid = true;
	xL.m_bArmChainFound = true;
	xL.m_afBodyY[ZENITH_HUMAN_BODY_SOLE] = -1.0404f;
	xL.m_afBodyY[ZENITH_HUMAN_BODY_CROWN] = 1.5608f;
	xL.m_afArmChain[ZENITH_HUMAN_ARM_SHOULDER] = 0.32f;
	xL.m_afArmChain[ZENITH_HUMAN_ARM_ELBOW] = 0.66f;
	xL.m_afArmChain[ZENITH_HUMAN_ARM_WRIST] = 1.00f;
	xL.m_afArmChain[ZENITH_HUMAN_ARM_FINGERTIP] = 1.27f;
	ZENITH_ASSERT_TRUE(Zenith_Tools_HumanSkinBind::CheckArmChain(xL).m_bValid,
		"a plausible arm passes");

	xL.m_afArmChain[ZENITH_HUMAN_ARM_FINGERTIP] = 1.60f;   // a 0.23-of-height hand
	const Zenith_Tools_HumanSkinBind::ArmSanity xBad = Zenith_Tools_HumanSkinBind::CheckArmChain(xL);
	ZENITH_ASSERT_TRUE(!xBad.m_bValid, "a mitten-sized hand must be refused, not shipped");
	ZENITH_ASSERT_TRUE(!xBad.m_strReason.empty(), "and the refusal must say which bound it broke");
}

//------------------------------------------------------------------------------
// The shared rig and the solver
//------------------------------------------------------------------------------

namespace
{
	// The shipped rig's shape, built here so these tests need no asset on disk.
	void BindBuildShippedRig(Zenith_SkeletonAsset& xSkel)
	{
		const Zenith_HumanProportions& xP = Zenith_HumanProportionsRealistic();
		const Zenith_Maths::Quat xI = glm::identity<Zenith_Maths::Quat>();
		const Zenith_Maths::Vector3 xS(1.0f);
		xSkel.AddBone("Root", -1, Zenith_Maths::Vector3(0, xP.HipY(), 0), xI, xS);
		xSkel.AddBone("Spine", 0, Zenith_Maths::Vector3(0, xP.SpineY() - xP.HipY(), 0), xI, xS);
		xSkel.AddBone("Neck", 1, Zenith_Maths::Vector3(0, xP.NeckY() - xP.SpineY(), 0), xI, xS);
		xSkel.AddBone("Head", 2, Zenith_Maths::Vector3(0, xP.HeadY() - xP.NeckY(), 0), xI, xS);
		for (int iSide = 0; iSide < 2; ++iSide)
		{
			const float fS = (iSide == 0) ? -1.0f : 1.0f;
			const char* szU = (iSide == 0) ? "LeftUpperArm" : "RightUpperArm";
			const char* szL = (iSide == 0) ? "LeftLowerArm" : "RightLowerArm";
			const char* szH = (iSide == 0) ? "LeftHand" : "RightHand";
			// ★ T-POSED, like the rig it stands in for. The one non-identity bind
			// rotation in the whole skeleton, and the reason an artist's T-posed
			// mesh can share it without being deformed into it.
			const int32_t iU = static_cast<int32_t>(xSkel.AddBone(szU, 1,
				Zenith_Maths::Vector3(fS * xP.ShoulderHalfX(), xP.ShoulderY() - xP.SpineY(), 0),
				Zenith_HumanArmBindRotation(fS), xS));
			const int32_t iL = static_cast<int32_t>(xSkel.AddBone(szL, iU,
				Zenith_Maths::Vector3(0, xP.ElbowY() - xP.ShoulderY(), 0), xI, xS));
			xSkel.AddBone(szH, iL, Zenith_Maths::Vector3(0, xP.WristY() - xP.ElbowY(), 0), xI, xS);
		}
		for (int iSide = 0; iSide < 2; ++iSide)
		{
			const float fS = (iSide == 0) ? -1.0f : 1.0f;
			const char* szU = (iSide == 0) ? "LeftUpperLeg" : "RightUpperLeg";
			const char* szL = (iSide == 0) ? "LeftLowerLeg" : "RightLowerLeg";
			const char* szF = (iSide == 0) ? "LeftFoot" : "RightFoot";
			const char* szT = (iSide == 0) ? "LeftToe" : "RightToe";
			const int32_t iU = static_cast<int32_t>(xSkel.AddBone(szU, 0,
				Zenith_Maths::Vector3(fS * xP.HipHalfX(), 0, 0), xI, xS));
			const int32_t iL = static_cast<int32_t>(xSkel.AddBone(szL, iU,
				Zenith_Maths::Vector3(0, xP.KneeY() - xP.HipY(), 0), xI, xS));
			const int32_t iF = static_cast<int32_t>(xSkel.AddBone(szF, iL,
				Zenith_Maths::Vector3(0, xP.AnkleY() - xP.KneeY(), 0), xI, xS));
			xSkel.AddBone(szT, iF, Zenith_Maths::Vector3(0, -0.146f, 0.100f), xI, xS);
		}
		xSkel.ComputeBindPoseMatrices();
	}
}

ZENITH_TEST(HumanSkinBind, SharedRigArmPointsAlongTheLateralAxis)
{
	// ★★ THE SHARED RIG IS T-POSED, and this is the assertion that says so in the
	// only terms that matter: where the bones actually END UP. The bind rotation is
	// a sign that is easy to get backwards, and a backwards one crosses the arms
	// through the torso while every test that measures LENGTHS stays green -- the
	// upper arm is exactly as long either way.
	Zenith_SkeletonAsset xShipped;
	BindBuildShippedRig(xShipped);

	const Zenith_Maths::Vector3 xShoulder(
		xShipped.GetBone(static_cast<u_int>(xShipped.GetBoneIndex("RightUpperArm"))).m_xBindPoseModel[3]);
	const Zenith_Maths::Vector3 xHand(
		xShipped.GetBone(static_cast<u_int>(xShipped.GetBoneIndex("RightHand"))).m_xBindPoseModel[3]);
	ZENITH_ASSERT_TRUE(xHand.x > xShoulder.x + 0.3f, "the right hand runs out along +X from the shoulder");
	ZENITH_ASSERT_TRUE(std::fabs(xHand.y - xShoulder.y) < 0.05f, "and at the same height: the arm is horizontal");

	const Zenith_Maths::Vector3 xLeftHand(
		xShipped.GetBone(static_cast<u_int>(xShipped.GetBoneIndex("LeftHand"))).m_xBindPoseModel[3]);
	ZENITH_ASSERT_TRUE(xLeftHand.x < -0.3f, "and the left one runs out along -X: the arms do not cross");

	// ★ THE ARM CHAIN'S SEGMENT LENGTHS ARE UNTOUCHED BY THE POSE. Only the two
	// UpperArms carry a bind rotation; everything below them keeps the offsets the
	// arms-down rig had, which is what makes this a pose change and not a re-rig.
	const Zenith_HumanProportions& xP = Zenith_HumanProportionsRealistic();
	ZENITH_ASSERT_EQ_FLOAT(glm::length(xHand - xShoulder),
		xP.ShoulderY() - xP.WristY(), 1.0e-3f,
		"shoulder-to-wrist is the same LENGTH it was hanging down");

	// The legs are not rotated at all.
	const Zenith_Maths::Vector3 xFoot(
		xShipped.GetBone(static_cast<u_int>(xShipped.GetBoneIndex("RightFoot"))).m_xBindPoseModel[3]);
	ZENITH_ASSERT_TRUE(xFoot.y < -0.5f, "the foot still hangs below the hip");
}

ZENITH_TEST(HumanSkinBind, SolverWeightsAreNormalisedAndGated)
{
	const BindPlanted xPlanted;
	Zenith_MeshAsset xMesh;
	BindBuildSynthTPoseBody(xMesh, xPlanted);
	Zenith_HumanLandmarks xL;
	ZENITH_ASSERT_TRUE(BindMeasureSynth(xMesh, xL), "measure");

	Zenith_SkeletonAsset xShipped;
	BindBuildShippedRig(xShipped);

	Zenith_Tools_HumanSkinBind::SolveReport xReport;
	ZENITH_ASSERT_TRUE(Zenith_Tools_HumanSkinBind::SolveHumanSkinWeights(xMesh, xShipped, xL, xReport),
		"the solve must run");

	// ★ FALLBACK COUNT ZERO. A vertex no bone claimed is a hole in the gates or
	// the radii; pinning it to Root would hide that behind a patch of skin riding
	// the pelvis, and every other assertion here would still pass.
	ZENITH_ASSERT_EQ(xReport.m_uFallbackCount, 0u, "every vertex must be claimed by a real bone");
	ZENITH_ASSERT_TRUE(xReport.m_uUniquePositions > 0u && xReport.m_uUniquePositions <= xMesh.GetNumVerts(),
		"welding must produce a sane unique-position count");
	ZENITH_ASSERT_TRUE(xReport.m_uMaxBoneIndex < xShipped.GetNumBones(), "no bone index past the rig");

	const int32_t iLeftHand = xShipped.GetBoneIndex("LeftHand");
	const int32_t iRightHand = xShipped.GetBoneIndex("RightHand");
	const int32_t iHead = xShipped.GetBoneIndex("Head");
	for (u_int v = 0u; v < xMesh.GetNumVerts(); ++v)
	{
		const glm::vec4& xW = xMesh.m_xBoneWeights.Get(v);
		const glm::uvec4& xI = xMesh.m_xBoneIndices.Get(v);
		const float fSum = xW.x + xW.y + xW.z + xW.w;
		ZENITH_ASSERT_TRUE(std::fabs(fSum - 1.0f) < 1.0e-5f, "weights sum to 1");
		ZENITH_ASSERT_TRUE(xW.x >= xW.y && xW.y >= xW.z && xW.z >= xW.w, "weights are sorted descending");

		const Zenith_Maths::Vector3& xP = xMesh.m_xPositions.Get(v);
		for (int k = 0; k < 4; ++k)
		{
			if (xW[k] <= 0.0f) { continue; }
			// left/right bleed
			if (static_cast<int32_t>(xI[k]) == iLeftHand)
			{
				ZENITH_ASSERT_TRUE(xP.x < 0.2f, "a LEFT hand may not claim geometry on the right");
			}
			if (static_cast<int32_t>(xI[k]) == iRightHand)
			{
				ZENITH_ASSERT_TRUE(xP.x > -0.2f, "a RIGHT hand may not claim geometry on the left");
			}
			// A hand vertex keeping a trace of the head forever is what the hard
			// cut exists to prevent; 1/d^n never reaches zero.
			if (static_cast<int32_t>(xI[k]) == iHead)
			{
				ZENITH_ASSERT_TRUE(xP.y > 0.5f, "the head may not claim anything below the chest");
			}
		}
	}
}

ZENITH_TEST(HumanSkinBind, SeamDuplicatesGetIdenticalWeights)
{
	// ★ A UV ATLAS SPLITS VERTICES AT EVERY SEAM. Solving per-INDEX gives the two
	// sides of a seam different weights, and the mesh cracks open the first time
	// it animates -- a failure that looks like a modelling problem, not a
	// weighting one.
	const BindPlanted xPlanted;
	Zenith_MeshAsset xMesh;
	BindBuildSynthTPoseBody(xMesh, xPlanted);

	// Duplicate a run of vertices at exactly the same positions, as a seam does.
	const u_int uOriginal = xMesh.GetNumVerts();
	for (u_int v = 0u; v < 64u && v < uOriginal; ++v)
	{
		xMesh.AddVertex(xMesh.m_xPositions.Get(v), xMesh.m_xNormals.Get(v), Zenith_Maths::Vector2(0.5f, 0.5f));
	}

	Zenith_HumanLandmarks xL;
	ZENITH_ASSERT_TRUE(BindMeasureSynth(xMesh, xL), "measure");
	Zenith_SkeletonAsset xShipped;
	BindBuildShippedRig(xShipped);
	Zenith_Tools_HumanSkinBind::SolveReport xReport;
	ZENITH_ASSERT_TRUE(Zenith_Tools_HumanSkinBind::SolveHumanSkinWeights(xMesh, xShipped, xL, xReport), "solve");

	for (u_int v = 0u; v < 64u && v < uOriginal; ++v)
	{
		ZENITH_ASSERT_TRUE(xMesh.m_xBoneIndices.Get(v) == xMesh.m_xBoneIndices.Get(uOriginal + v),
			"a seam duplicate must get the SAME bone indices as its twin");
		ZENITH_ASSERT_TRUE(xMesh.m_xBoneWeights.Get(v) == xMesh.m_xBoneWeights.Get(uOriginal + v),
			"...and the same weights");
	}
}

ZENITH_TEST(HumanSkinBind, BindIsDeterministic)
{
	const BindPlanted xPlanted;
	Zenith_MeshAsset xA, xB;
	BindBuildSynthTPoseBody(xA, xPlanted);
	BindBuildSynthTPoseBody(xB, xPlanted);

	Zenith_SkeletonAsset xShipped;
	BindBuildShippedRig(xShipped);

	auto Run = [&](Zenith_MeshAsset& xMesh)
	{
		Zenith_HumanLandmarks xL;
		BindMeasureSynth(xMesh, xL);
		Zenith_Tools_HumanSkinBind::SolveReport xReport;
		Zenith_Tools_HumanSkinBind::SolveHumanSkinWeights(xMesh, xShipped, xL, xReport);
	};
	Run(xA);
	Run(xB);

	bool bIdentical = (xA.GetNumVerts() == xB.GetNumVerts());
	for (u_int v = 0u; v < xA.GetNumVerts() && bIdentical; ++v)
	{
		bIdentical = (xA.m_xPositions.Get(v) == xB.m_xPositions.Get(v))
			&& (xA.m_xBoneIndices.Get(v) == xB.m_xBoneIndices.Get(v))
			&& (xA.m_xBoneWeights.Get(v) == xB.m_xBoneWeights.Get(v));
	}
	ZENITH_ASSERT_TRUE(bIdentical, "the same input twice must bind bit-identically");
}
