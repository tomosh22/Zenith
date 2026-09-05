#include "UnitTests/Zenith_UnitTests.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"

// ============================================================================
// Zenith_SkinDeform unit tests -- pure, all-config, NO ASSET ON DISK.
//
// The subject is a synthetic humanoid built from the same ring shapes the two
// real generators use: a body whose foot block is WIDER than the ankle above it,
// whose leg has a shallow knee dip, whose neck is the narrowest thing above the
// shoulder, and whose arms hang at a constant X. Planting the landmarks is what
// makes "did the scan find them" answerable without shipping a mesh.
//
// ★ THE ONE TEST THAT MATTERS MOST is WarpPreservesHeightAndPinnedLandmarks. The
// first design of this change re-proportioned with linear-blend skinning, which
// TRANSLATES a rigidly-weighted vertex -- so it would have shrunk a 2.60-unit
// body to 2.36 and rendered one human 10% taller than the next, with every other
// test green.
// ============================================================================

namespace
{
	struct SkinDeformSynthBody
	{
		Zenith_Vector<Zenith_Maths::Vector3> m_xPositions;
		Zenith_Vector<Zenith_Maths::Vector3> m_xNormals;
		Zenith_Vector<glm::uvec4> m_xBoneIndices;
		Zenith_Vector<glm::vec4> m_xBoneWeights;

		Zenith_SkinDeformView View()
		{
			Zenith_SkinDeformView xView;
			xView.m_pxPositions = m_xPositions.GetDataPointer();
			xView.m_pxNormals = m_xNormals.GetDataPointer();
			xView.m_pxBoneIndices = m_xBoneIndices.GetDataPointer();
			xView.m_pxBoneWeights = m_xBoneWeights.GetDataPointer();
			xView.m_uNumVerts = m_xPositions.GetSize();
			return xView;
		}
	};

	// The sixteen core StickFigure bone indices, by the names the rig uses.
	enum : u_int
	{
		SD_ROOT = 0u, SD_SPINE, SD_NECK, SD_HEAD,
		SD_LUARM, SD_LLARM, SD_LHAND, SD_RUARM, SD_RLARM, SD_RHAND,
		SD_LULEG, SD_LLLEG, SD_LFOOT, SD_RULEG, SD_RLLEG, SD_RFOOT
	};

	struct SkinDeformSynthRing { float fY, fCx, fR; u_int uBoneA, uBoneB; float fBlend; };

	void SkinDeformAddRing(SkinDeformSynthBody& xBody, const SkinDeformSynthRing& xRing, float fSide)
	{
		constexpr u_int uSEGS = 24u;
		for (u_int s = 0u; s < uSEGS; ++s)
		{
			const float fA = (6.28318530718f * static_cast<float>(s)) / static_cast<float>(uSEGS);
			xBody.m_xPositions.PushBack(Zenith_Maths::Vector3(
				fSide * xRing.fCx + xRing.fR * std::cos(fA), xRing.fY, xRing.fR * std::sin(fA)));
			xBody.m_xNormals.PushBack(Zenith_Maths::Vector3(std::cos(fA), 0.0f, std::sin(fA)));
			xBody.m_xBoneIndices.PushBack(glm::uvec4(xRing.uBoneA, xRing.uBoneB, 0u, 0u));
			xBody.m_xBoneWeights.PushBack(glm::vec4(1.0f - xRing.fBlend, xRing.fBlend, 0.0f, 0.0f));
		}
	}

	void SkinDeformAddPart(SkinDeformSynthBody& xBody, const SkinDeformSynthRing* pxRings,
		u_int uNum, float fSide)
	{
		// Subdivide so a scan bin is never empty between two authored rings.
		for (u_int u = 0u; u + 1u < uNum; ++u)
		{
			constexpr u_int uSUB = 6u;
			// ★ WEIGHTS ONLY INTERPOLATE ACROSS A SPAN WHOSE TWO RINGS NAME THE SAME
			// PAIR OF BONES. Lerping the blend factor between rings that name
			// DIFFERENT pairs is meaningless -- it reads "1.0 of the lower arm"
			// and "0.45 of the hand" as a ramp between two numbers rather than
			// between two bones, which drove the arm weight to zero half-way down
			// the forearm and made the fixture, not the code, look broken.
			const bool bSamePair = (pxRings[u].uBoneA == pxRings[u + 1u].uBoneA) &&
				(pxRings[u].uBoneB == pxRings[u + 1u].uBoneB);
			for (u_int k = 0u; k < uSUB; ++k)
			{
				const float fT = static_cast<float>(k) / static_cast<float>(uSUB);
				SkinDeformSynthRing xR = pxRings[u];
				xR.fY = pxRings[u].fY + fT * (pxRings[u + 1u].fY - pxRings[u].fY);
				xR.fCx = pxRings[u].fCx + fT * (pxRings[u + 1u].fCx - pxRings[u].fCx);
				xR.fR = pxRings[u].fR + fT * (pxRings[u + 1u].fR - pxRings[u].fR);
				if (bSamePair)
				{
					xR.fBlend = pxRings[u].fBlend + fT * (pxRings[u + 1u].fBlend - pxRings[u].fBlend);
				}
				SkinDeformAddRing(xBody, xR, fSide);
			}
		}
		SkinDeformAddRing(xBody, pxRings[uNum - 1u], fSide);
	}

	// bHasFoot false reproduces Zenithmon: a leg that tapers to a point, with no
	// shoe and therefore NO measurable ankle. That is not a broken mesh, and the
	// scan has to say so rather than inventing one.
	void SkinDeformBuildSynthBody(SkinDeformSynthBody& xBody, bool bHasFoot)
	{
		const SkinDeformSynthRing axTorso[] = {
			{  1.150f, 0.0f, 0.150f, SD_SPINE, SD_SPINE, 0.00f },
			{  1.060f, 0.0f, 0.200f, SD_SPINE, SD_SPINE, 0.00f },
			{  0.800f, 0.0f, 0.180f, SD_SPINE, SD_SPINE, 0.00f },
			{  0.450f, 0.0f, 0.162f, SD_ROOT,  SD_SPINE, 0.80f },
			{  0.180f, 0.0f, 0.160f, SD_ROOT,  SD_SPINE, 0.22f },
			{ -0.120f, 0.0f, 0.182f, SD_ROOT,  SD_ROOT,  0.00f },
		};
		const SkinDeformSynthRing axHead[] = {
			{  1.5608f, 0.0f, 0.030f, SD_HEAD, SD_HEAD, 0.00f },
			{  1.465f,  0.0f, 0.099f, SD_HEAD, SD_HEAD, 0.00f },
			{  1.300f,  0.0f, 0.072f, SD_HEAD, SD_HEAD, 0.00f },
			{  1.200f,  0.0f, 0.064f, SD_NECK, SD_NECK, 0.00f },
			{  1.130f,  0.0f, 0.069f, SD_SPINE, SD_NECK, 0.60f },
		};
		SkinDeformAddPart(xBody, axTorso, sizeof(axTorso) / sizeof(axTorso[0]), 1.0f);
		SkinDeformAddPart(xBody, axHead, sizeof(axHead) / sizeof(axHead[0]), 1.0f);

		for (int iSide = 0; iSide < 2; ++iSide)
		{
			const float fSide = (iSide == 0) ? -1.0f : 1.0f;
			const u_int uUA = (iSide == 0) ? SD_LUARM : SD_RUARM;
			const u_int uLA = (iSide == 0) ? SD_LLARM : SD_RLARM;
			const u_int uHA = (iSide == 0) ? SD_LHAND : SD_RHAND;
			const u_int uUL = (iSide == 0) ? SD_LULEG : SD_RULEG;
			const u_int uLL = (iSide == 0) ? SD_LLLEG : SD_RLLEG;
			const u_int uFT = (iSide == 0) ? SD_LFOOT : SD_RFOOT;

			// Each joint is PLANTED as the ring whose weights are 50/50 across it,
			// which is what a loft's author means by "the elbow is here" and what
			// the scan reads back.
			const SkinDeformSynthRing axArm[] = {
				{ 1.150f, 0.205f, 0.102f, SD_SPINE, uUA, 0.15f },   // deltoid cap
				{ 1.095f, 0.248f, 0.096f, SD_SPINE, uUA, 0.50f },   // SHOULDER
				{ 1.020f, 0.290f, 0.080f, SD_SPINE, uUA, 1.00f },
				{ 0.920f, 0.300f, 0.066f, uUA, uLA, 0.00f },
				{ 0.715f, 0.300f, 0.046f, uUA, uLA, 0.50f },        // ELBOW
				{ 0.520f, 0.300f, 0.044f, uUA, uLA, 1.00f },
				{ 0.500f, 0.300f, 0.040f, uLA, uHA, 0.00f },
				{ 0.435f, 0.300f, 0.031f, uLA, uHA, 0.50f },        // WRIST
				{ 0.380f, 0.300f, 0.048f, uLA, uHA, 1.00f },        // palm
				{ 0.205f, 0.300f, 0.014f, uHA, uHA, 0.00f },        // fingertip
			};
			SkinDeformAddPart(xBody, axArm, sizeof(axArm) / sizeof(axArm[0]), fSide);

			if (bHasFoot)
			{
				const SkinDeformSynthRing axLeg[] = {
					{  0.075f, 0.128f, 0.058f, SD_ROOT, uUL, 0.18f },
					{ -0.120f, 0.146f, 0.093f, SD_ROOT, uUL, 0.88f },
					{ -0.400f, 0.150f, 0.073f, uUL, uUL, 0.00f },
					{ -0.480f, 0.150f, 0.063f, uUL, uLL, 0.50f },   // knee dip
					{ -0.560f, 0.150f, 0.064f, uLL, uLL, 0.00f },
					{ -0.660f, 0.150f, 0.071f, uLL, uLL, 0.00f },
					{ -0.920f, 0.150f, 0.038f, uLL, uFT, 0.40f },   // ankle
					{ -0.984f, 0.150f, 0.050f, uFT, uFT, 0.00f },   // shoe
					{ -1.0404f, 0.150f, 0.044f, uFT, uFT, 0.00f },
				};
				SkinDeformAddPart(xBody, axLeg, sizeof(axLeg) / sizeof(axLeg[0]), fSide);
			}
			else
			{
				const SkinDeformSynthRing axLeg[] = {
					{  0.075f, 0.128f, 0.058f, SD_ROOT, uUL, 0.18f },
					{ -0.120f, 0.146f, 0.093f, SD_ROOT, uUL, 0.88f },
					{ -0.400f, 0.150f, 0.073f, uUL, uUL, 0.00f },
					{ -0.480f, 0.150f, 0.063f, uUL, uLL, 0.50f },
					{ -0.560f, 0.150f, 0.064f, uLL, uLL, 0.00f },
					{ -0.660f, 0.150f, 0.071f, uLL, uLL, 0.00f },
					{ -0.920f, 0.150f, 0.040f, uLL, uFT, 0.40f },
					{ -0.995145f, 0.150f, 0.020f, uFT, uFT, 0.00f },  // tapers away: no ankle, and a DIFFERENT sole
				};
				SkinDeformAddPart(xBody, axLeg, sizeof(axLeg) / sizeof(axLeg[0]), fSide);
			}
		}
	}

	// A minimal 16-bone rig at the given proportions, enough for the rebind tests.
	void SkinDeformBuildRig(Zenith_SkeletonAsset& xSkel, const Zenith_HumanProportions& xP,
		const Zenith_Maths::Quat& xUpperArmRotation)
	{
		const Zenith_Maths::Quat xI = glm::identity<Zenith_Maths::Quat>();
		const Zenith_Maths::Vector3 xS(1.0f);
		xSkel.AddBone("Root", -1, Zenith_Maths::Vector3(0, xP.HipY(), 0), xI, xS);
		xSkel.AddBone("Spine", 0, Zenith_Maths::Vector3(0, xP.SpineY() - xP.HipY(), 0), xI, xS);
		xSkel.AddBone("Neck", 1, Zenith_Maths::Vector3(0, xP.NeckY() - xP.SpineY(), 0), xI, xS);
		xSkel.AddBone("Head", 2, Zenith_Maths::Vector3(0, xP.HeadY() - xP.NeckY(), 0), xI, xS);
		for (int iSide = 0; iSide < 2; ++iSide)
		{
			const float fSide = (iSide == 0) ? -1.0f : 1.0f;
			const char* szU = (iSide == 0) ? "LeftUpperArm" : "RightUpperArm";
			const char* szL = (iSide == 0) ? "LeftLowerArm" : "RightLowerArm";
			const char* szH = (iSide == 0) ? "LeftHand" : "RightHand";
			const int32_t iU = static_cast<int32_t>(xSkel.AddBone(szU, 1,
				Zenith_Maths::Vector3(fSide * xP.ShoulderHalfX(), xP.ShoulderY() - xP.SpineY(), 0),
				xUpperArmRotation, xS));
			const int32_t iL = static_cast<int32_t>(xSkel.AddBone(szL, iU,
				Zenith_Maths::Vector3(0, xP.ElbowY() - xP.ShoulderY(), 0), xI, xS));
			xSkel.AddBone(szH, iL, Zenith_Maths::Vector3(0, xP.WristY() - xP.ElbowY(), 0), xI, xS);
		}
		for (int iSide = 0; iSide < 2; ++iSide)
		{
			const float fSide = (iSide == 0) ? -1.0f : 1.0f;
			const char* szU = (iSide == 0) ? "LeftUpperLeg" : "RightUpperLeg";
			const char* szL = (iSide == 0) ? "LeftLowerLeg" : "RightLowerLeg";
			const char* szF = (iSide == 0) ? "LeftFoot" : "RightFoot";
			const int32_t iU = static_cast<int32_t>(xSkel.AddBone(szU, 0,
				Zenith_Maths::Vector3(fSide * xP.HipHalfX(), 0, 0), glm::identity<Zenith_Maths::Quat>(), xS));
			const int32_t iL = static_cast<int32_t>(xSkel.AddBone(szL, iU,
				Zenith_Maths::Vector3(0, xP.KneeY() - xP.HipY(), 0), glm::identity<Zenith_Maths::Quat>(), xS));
			xSkel.AddBone(szF, iL, Zenith_Maths::Vector3(0, xP.AnkleY() - xP.KneeY(), 0),
				glm::identity<Zenith_Maths::Quat>(), xS);
		}
		xSkel.ComputeBindPoseMatrices();
	}

	bool SkinDeformMakeWarpForSynth(SkinDeformSynthBody& xBody, Zenith_HumanWarp& xWarpOut,
		Zenith_HumanLandmarks& xLandmarksOut)
	{
		Zenith_SkinDeformView xView = xBody.View();
		if (!Zenith_MeasureHumanLandmarks(xView, ZENITH_HUMAN_POSE_ARMS_DOWN, xLandmarksOut)) { return false; }
		return Zenith_MakeHumanWarp(xLandmarksOut, Zenith_HumanProportionsRealistic(), xWarpOut);
	}
}

//------------------------------------------------------------------------------
// The proportions table
//------------------------------------------------------------------------------

ZENITH_TEST(SkinDeform, LegacyProportionsReproduceShippedRig)
{
	// The twenty literals CreateStickFigureSkeleton used to carry. Extraction is
	// only safe if the table gives them back.
	const Zenith_HumanProportions& x = Zenith_HumanProportionsLegacy();
	ZENITH_ASSERT_EQ_FLOAT(x.AnkleY(), -1.0f, 1.0e-4f, "legacy ankle");
	ZENITH_ASSERT_EQ_FLOAT(x.KneeY(), -0.5f, 1.0e-4f, "legacy knee");
	ZENITH_ASSERT_EQ_FLOAT(x.HipY(), 0.0f, 1.0e-4f, "legacy hip (Root)");
	ZENITH_ASSERT_EQ_FLOAT(x.SpineY(), 0.5f, 1.0e-4f, "legacy spine");
	ZENITH_ASSERT_EQ_FLOAT(x.ShoulderY(), 1.1f, 1.0e-4f, "legacy shoulder");
	ZENITH_ASSERT_EQ_FLOAT(x.NeckY(), 1.2f, 1.0e-4f, "legacy neck");
	ZENITH_ASSERT_EQ_FLOAT(x.HeadY(), 1.4f, 1.0e-4f, "legacy head");
	ZENITH_ASSERT_EQ_FLOAT(x.ElbowY(), 0.7f, 1.0e-4f, "legacy elbow");
	ZENITH_ASSERT_EQ_FLOAT(x.WristY(), 0.4f, 1.0e-4f, "legacy wrist");
	ZENITH_ASSERT_EQ_FLOAT(x.ShoulderHalfX(), 0.3f, 1.0e-4f, "legacy shoulder half-X");
	ZENITH_ASSERT_EQ_FLOAT(x.HipHalfX(), 0.15f, 1.0e-4f, "legacy hip half-X");
	ZENITH_ASSERT_TRUE(x.IsOrdered(), "the legacy table must itself be a valid proportion set");
}

ZENITH_TEST(SkinDeform, RealisticProportionsAreOrderedAndPinned)
{
	const Zenith_HumanProportions& xR = Zenith_HumanProportionsRealistic();
	const Zenith_HumanProportions& xL = Zenith_HumanProportionsLegacy();
	ZENITH_ASSERT_TRUE(xR.IsOrdered(), "the realistic table must be a valid proportion set");

	// The four pins, each of which something downstream depends on absolutely.
	ZENITH_ASSERT_EQ_FLOAT(xR.HipY(), xL.HipY(), 1.0e-6f, "Root is pinned: the clips write ABSOLUTE Root keys");
	ZENITH_ASSERT_EQ_FLOAT(xR.SpineY(), xL.SpineY(), 1.0e-6f, "Spine is pinned: Idle writes an ABSOLUTE Spine key");
	ZENITH_ASSERT_EQ_FLOAT(xR.HeadY(), xL.HeadY(), 1.0e-6f, "Head is pinned: it keeps the skull rigid under the warp");
	ZENITH_ASSERT_EQ_FLOAT(xR.HipHalfX(), xL.HipHalfX(), 1.0e-6f, "hip half-X is deliberately unchanged");

	// The three that MOVE, in the direction the measurement says.
	ZENITH_ASSERT_TRUE(xR.AnkleY() > xL.AnkleY(), "the ankle rises: a foot is 7.5% of a body, not 1.6%");
	ZENITH_ASSERT_TRUE(xR.ShoulderY() < xL.ShoulderY(), "the shoulder joint sits below the old bone");
	ZENITH_ASSERT_TRUE(xR.FingertipY() < xL.FingertipY(), "the arm is longer, so the hand hangs lower");
}

//------------------------------------------------------------------------------
// Measurement
//------------------------------------------------------------------------------

ZENITH_TEST(SkinDeform, LandmarkScanFindsPlantedLandmarks)
{
	SkinDeformSynthBody xBody;
	SkinDeformBuildSynthBody(xBody, /*bHasFoot=*/true);
	Zenith_SkinDeformView xView = xBody.View();

	Zenith_HumanLandmarks xL;
	ZENITH_ASSERT_TRUE(Zenith_MeasureHumanLandmarks(xView, ZENITH_HUMAN_POSE_ARMS_DOWN, xL),
		"a well-formed synthetic body must measure");
	Zenith_LogHumanLandmarks("synthetic (with feet)", xL);

	const float fTol = 0.02f * xL.Height();   // 2% of height -- what makes "measured at import" trustworthy
	ZENITH_ASSERT_EQ_FLOAT(xL.SoleY(), -1.0404f, 1.0e-4f, "sole is the mesh minimum");
	ZENITH_ASSERT_EQ_FLOAT(xL.CrownY(), 1.5608f, 1.0e-4f, "crown is the mesh maximum");

	ZENITH_ASSERT_TRUE(xL.m_abBodyFound[ZENITH_HUMAN_BODY_ANKLE], "a body with a shoe has an ankle seam");
	ZENITH_ASSERT_EQ_FLOAT(xL.m_afBodyY[ZENITH_HUMAN_BODY_ANKLE], -0.920f, fTol, "planted ankle");
	ZENITH_ASSERT_TRUE(xL.m_abBodyFound[ZENITH_HUMAN_BODY_KNEE], "knee");
	ZENITH_ASSERT_EQ_FLOAT(xL.m_afBodyY[ZENITH_HUMAN_BODY_KNEE], -0.480f, fTol, "planted knee dip");
	ZENITH_ASSERT_TRUE(xL.m_abBodyFound[ZENITH_HUMAN_BODY_NECK], "neck");
	ZENITH_ASSERT_EQ_FLOAT(xL.m_afBodyY[ZENITH_HUMAN_BODY_NECK], 1.200f, fTol, "planted neck");

	ZENITH_ASSERT_TRUE(xL.m_bArmChainFound, "arm chain");
	// ★ THE BOUNDED WRIST SEARCH. An unbounded minimum over the outer arm returns
	// the FINGERTIP at 0.205, which is thinner than the wrist at 0.435.
	ZENITH_ASSERT_EQ_FLOAT(xL.m_afArmChain[ZENITH_HUMAN_ARM_WRIST], 0.435f, fTol, "planted wrist");
	ZENITH_ASSERT_EQ_FLOAT(xL.m_afArmChain[ZENITH_HUMAN_ARM_FINGERTIP], 0.205f, 1.0e-3f, "fingertip");
	ZENITH_ASSERT_TRUE(xL.m_afArmChain[ZENITH_HUMAN_ARM_SHOULDER] > 0.92f &&
		xL.m_afArmChain[ZENITH_HUMAN_ARM_SHOULDER] < 1.16f, "shoulder lands in the deltoid, not on the wrist");
	ZENITH_ASSERT_TRUE(xL.m_fShoulderHalfX > 0.20f && xL.m_fShoulderHalfX < 0.36f,
		"shoulder half-X is the arm's CENTRELINE, not the deltoid's outer surface");

	// ★ THERE IS NO CROTCH SCAN, and its absence is a finding rather than a gap. A
	// two-cluster test needs a genuinely empty band at x = 0, and a LOFT is a
	// hollow shell: a 48-segment torso ring puts its nearest vertices 2.6 cm
	// either side of centre and never one ON it, so every slice reads as "two
	// legs" and the scan reported a crotch at +0.075 -- ABOVE the navel. Widening
	// the gap threshold enough to see through a loft makes it blind to an artist
	// mesh, whose thighs part by a hundredth of a height. The plane both meshes
	// genuinely agree on is Root, and Root is pinned, so that is what the mid-body
	// anchor uses.
	ZENITH_ASSERT_TRUE(xL.m_fHipHalfX > 0.10f && xL.m_fHipHalfX < 0.20f,
		"the thigh centreline is measurable even though the crotch is not");
}

ZENITH_TEST(SkinDeform, LandmarkScanReportsAbsentAnkleRatherThanInventingOne)
{
	// Zenithmon's shape: legs that taper to a point. There is no ankle seam,
	// there is no interior radius minimum, and the honest answer is "not found".
	// Inventing one would stretch a foot that does not exist.
	SkinDeformSynthBody xBody;
	SkinDeformBuildSynthBody(xBody, /*bHasFoot=*/false);
	Zenith_SkinDeformView xView = xBody.View();

	Zenith_HumanLandmarks xL;
	ZENITH_ASSERT_TRUE(Zenith_MeasureHumanLandmarks(xView, ZENITH_HUMAN_POSE_ARMS_DOWN, xL),
		"a footless body is still measurable");
	ZENITH_ASSERT_TRUE(!xL.m_abBodyFound[ZENITH_HUMAN_BODY_ANKLE],
		"a monotonically tapering leg has no ankle to find");

	Zenith_HumanWarp xWarp;
	ZENITH_ASSERT_TRUE(Zenith_MakeHumanWarp(xL, Zenith_HumanProportionsRealistic(), xWarp),
		"a missing anchor must be dropped, not fatal");
	ZENITH_ASSERT_TRUE(xWarp.IsMonotonic(), "the compacted chain is still monotonic");
}

ZENITH_TEST(SkinDeform, LandmarkScanRejectsDegenerateInput)
{
	Zenith_HumanLandmarks xL;
	Zenith_SkinDeformView xEmpty;
	ZENITH_ASSERT_TRUE(!Zenith_MeasureHumanLandmarks(xEmpty, ZENITH_HUMAN_POSE_ARMS_DOWN, xL),
		"an empty view has no landmarks");

	Zenith_Vector<Zenith_Maths::Vector3> xFlat;
	for (u_int u = 0u; u < 64u; ++u)
	{
		xFlat.PushBack(Zenith_Maths::Vector3(static_cast<float>(u) * 0.01f, 0.0f, 0.0f));
	}
	Zenith_SkinDeformView xFlatView;
	xFlatView.m_pxPositions = xFlat.GetDataPointer();
	xFlatView.m_uNumVerts = xFlat.GetSize();
	ZENITH_ASSERT_TRUE(!Zenith_MeasureHumanLandmarks(xFlatView, ZENITH_HUMAN_POSE_ARMS_DOWN, xL),
		"a body with no height is not a body");
}

//------------------------------------------------------------------------------
// The warp
//------------------------------------------------------------------------------

ZENITH_TEST(SkinDeform, WarpAnchorsAreMonotonic)
{
	// ★ THE INVERSION THIS CATCHES. A anchor array taken from BONE positions is
	// already out of order before anything moves: Zenithmon's lowest vertex is
	// -0.995145 while the legacy Foot PIVOT is -1.0, so the sole sits ABOVE the
	// bone. Anchors come from geometry, which is why this holds.
	for (int iFoot = 0; iFoot < 2; ++iFoot)
	{
		SkinDeformSynthBody xBody;
		SkinDeformBuildSynthBody(xBody, iFoot != 0);
		Zenith_HumanWarp xWarp;
		Zenith_HumanLandmarks xL;
		ZENITH_ASSERT_TRUE(SkinDeformMakeWarpForSynth(xBody, xWarp, xL), "warp must build");
		ZENITH_ASSERT_TRUE(xWarp.IsMonotonic(), "anchors must be strictly ascending on BOTH sides");
		ZENITH_ASSERT_TRUE(xWarp.m_uNumBodyAnchors >= 4u, "at least sole/hip/shoulder/crown survive");
	}
}

ZENITH_TEST(SkinDeform, WarpPreservesHeightAndPinnedLandmarks)
{
	SkinDeformSynthBody xBody;
	SkinDeformBuildSynthBody(xBody, true);
	Zenith_HumanWarp xWarp;
	Zenith_HumanLandmarks xBefore;
	ZENITH_ASSERT_TRUE(SkinDeformMakeWarpForSynth(xBody, xWarp, xBefore), "warp must build");

	Zenith_SkinDeformView xView = xBody.View();
	ZENITH_ASSERT_TRUE(Zenith_SkinWarpVertices(xView, xWarp), "warp must apply");

	float fMin = xView.m_pxPositions[0].y;
	float fMax = fMin;
	for (u_int v = 1u; v < xView.m_uNumVerts; ++v)
	{
		fMin = std::min(fMin, xView.m_pxPositions[v].y);
		fMax = std::max(fMax, xView.m_pxPositions[v].y);
	}
	// ★ THIS is the test the first design would have failed. A linear-blend
	// "re-proportion" translates rigidly-weighted vertices, so it drags the crown
	// down with the Head bone and lifts the sole with the Foot bone.
	ZENITH_ASSERT_EQ_FLOAT(fMin, xBefore.SoleY(), 1.0e-5f, "the sole plane is PINNED");
	ZENITH_ASSERT_EQ_FLOAT(fMax, xBefore.CrownY(), 1.0e-5f, "the crown plane is PINNED");
	ZENITH_ASSERT_EQ_FLOAT(fMax - fMin, xBefore.Height(), 1.0e-5f, "total height is unchanged");

	// The Root and Head planes, which the clips and the skull depend on.
	const Zenith_HumanProportions& xP = Zenith_HumanProportionsRealistic();
	ZENITH_ASSERT_EQ_FLOAT(xWarp.MapY(xP.HipY(), 0.0f), xP.HipY(), 1.0e-5f, "the Root plane does not move");
	ZENITH_ASSERT_EQ_FLOAT(xWarp.MapY(xP.HeadY(), 0.0f), xP.HeadY(), 1.0e-5f, "the Head plane does not move");
	// ...and the skull ABOVE it is rigid, which is what "the head keeps its size" means.
	const float fSkull = 0.5f * (xP.HeadY() + xBefore.CrownY());
	ZENITH_ASSERT_EQ_FLOAT(xWarp.MapY(fSkull, 0.0f), fSkull, 1.0e-5f, "the skull above the head plane is rigid");
}

ZENITH_TEST(SkinDeform, WarpMovesInteriorLandmarksToTarget)
{
	SkinDeformSynthBody xBody;
	SkinDeformBuildSynthBody(xBody, true);
	Zenith_HumanWarp xWarp;
	Zenith_HumanLandmarks xBefore;
	ZENITH_ASSERT_TRUE(SkinDeformMakeWarpForSynth(xBody, xWarp, xBefore), "warp must build");

	const Zenith_HumanProportions& xP = Zenith_HumanProportionsRealistic();
	ZENITH_ASSERT_EQ_FLOAT(xWarp.MapY(xBefore.m_afBodyY[ZENITH_HUMAN_BODY_ANKLE], 0.0f),
		xP.AnkleY(), 1.0e-4f, "the measured ankle lands on the RIG's ankle plane");
	ZENITH_ASSERT_EQ_FLOAT(xWarp.MapY(xBefore.m_afBodyY[ZENITH_HUMAN_BODY_KNEE], 0.0f),
		xP.KneeY(), 1.0e-4f, "the measured knee lands on the RIG's knee plane");
	ZENITH_ASSERT_EQ_FLOAT(xWarp.MapY(xBefore.m_afBodyY[ZENITH_HUMAN_BODY_NECK], 0.0f),
		xP.NeckY(), 1.0e-4f, "the measured neck lands on the RIG's neck plane");

	// ★ TARGETS ARE THE RIG'S PLANES, NOT THIS MESH'S OWN FRACTIONS. Two meshes of
	// slightly different heights re-proportioned to their own fractions would put
	// their knees 4.6 cm apart on one shared skeleton.
	SkinDeformSynthBody xShort;
	SkinDeformBuildSynthBody(xShort, false);   // different sole, different height
	Zenith_HumanWarp xWarpShort;
	Zenith_HumanLandmarks xShortBefore;
	ZENITH_ASSERT_TRUE(SkinDeformMakeWarpForSynth(xShort, xWarpShort, xShortBefore), "warp must build");
	ZENITH_ASSERT_EQ_FLOAT(xWarpShort.MapY(xShortBefore.m_afBodyY[ZENITH_HUMAN_BODY_KNEE], 0.0f),
		xP.KneeY(), 1.0e-4f, "a shorter mesh's knee lands on the SAME rig plane");
}

ZENITH_TEST(SkinDeform, WarpArmChainMovesElbowAndWrist)
{
	SkinDeformSynthBody xBody;
	SkinDeformBuildSynthBody(xBody, true);
	Zenith_HumanWarp xWarp;
	Zenith_HumanLandmarks xL;
	ZENITH_ASSERT_TRUE(SkinDeformMakeWarpForSynth(xBody, xWarp, xL), "warp must build");

	const Zenith_HumanProportions& xP = Zenith_HumanProportionsRealistic();
	ZENITH_ASSERT_EQ_FLOAT(xWarp.MapY(xL.m_afArmChain[ZENITH_HUMAN_ARM_WRIST], 1.0f),
		xP.WristY(), 1.0e-4f, "at arm weight 1 the wrist follows the ARM chain");
	ZENITH_ASSERT_EQ_FLOAT(xWarp.MapY(xL.m_afArmChain[ZENITH_HUMAN_ARM_ELBOW], 1.0f),
		xP.ElbowY(), 1.0e-4f, "and so does the elbow");

	// ★ WHY THE ARM NEEDS ITS OWN CHAIN. At the same Y, a torso vertex and an arm
	// vertex must go to different places -- the body map is a chest/waist map and
	// would drag the wrist along it.
	const float fBodyPath = xWarp.MapY(xL.m_afArmChain[ZENITH_HUMAN_ARM_WRIST], 0.0f);
	ZENITH_ASSERT_TRUE(std::fabs(fBodyPath - xP.WristY()) > 0.02f,
		"the body map must NOT already agree, or the second chain would be pointless");
}

ZENITH_TEST(SkinDeform, WarpWidensShouldersByArmWeight)
{
	SkinDeformSynthBody xBody;
	SkinDeformBuildSynthBody(xBody, true);
	Zenith_HumanWarp xWarp;
	Zenith_HumanLandmarks xL;
	ZENITH_ASSERT_TRUE(SkinDeformMakeWarpForSynth(xBody, xWarp, xL), "warp must build");

	Zenith_Vector<Zenith_Maths::Vector3> xPts;
	xPts.PushBack(Zenith_Maths::Vector3(0.30f, 0.90f, 0.0f));    // arm
	xPts.PushBack(Zenith_Maths::Vector3(-0.30f, 0.90f, 0.0f));   // arm, mirrored
	xPts.PushBack(Zenith_Maths::Vector3(0.12f, 0.90f, 0.0f));    // torso
	Zenith_Vector<glm::uvec4> xIdx;
	xIdx.PushBack(glm::uvec4(SD_RUARM, 0u, 0u, 0u));
	xIdx.PushBack(glm::uvec4(SD_LUARM, 0u, 0u, 0u));
	xIdx.PushBack(glm::uvec4(SD_SPINE, 0u, 0u, 0u));
	Zenith_Vector<glm::vec4> xW;
	for (u_int u = 0u; u < 3u; ++u) { xW.PushBack(glm::vec4(1.0f, 0.0f, 0.0f, 0.0f)); }

	Zenith_SkinDeformView xView;
	xView.m_pxPositions = xPts.GetDataPointer();
	xView.m_pxBoneIndices = xIdx.GetDataPointer();
	xView.m_pxBoneWeights = xW.GetDataPointer();
	xView.m_uNumVerts = 3u;
	ZENITH_ASSERT_TRUE(Zenith_SkinWarpVertices(xView, xWarp), "warp must apply");

	ZENITH_ASSERT_EQ_FLOAT(xPts.Get(0u).x, 0.30f + xWarp.m_fLateralShiftX, 1.0e-5f,
		"a weight-1 arm vertex shifts by exactly the lateral shift");
	ZENITH_ASSERT_EQ_FLOAT(xPts.Get(1u).x, -(0.30f + xWarp.m_fLateralShiftX), 1.0e-5f,
		"and the mirror image shifts the other way by the same amount");
	ZENITH_ASSERT_EQ_FLOAT(xPts.Get(2u).x, 0.12f, 1.0e-6f,
		"a torso vertex does not move sideways at all");
}

ZENITH_TEST(SkinDeform, WarpRingAndVertexFormsAgree)
{
	// One map, two call shapes. Zenithmon warps RINGS because it emits analytic
	// normals it must never regenerate; StickFigure warps VERTICES because it
	// rebuilds them. If the two ever disagreed, the two games' humans would stop
	// sharing a skeleton's proportions.
	SkinDeformSynthBody xBody;
	SkinDeformBuildSynthBody(xBody, true);
	Zenith_HumanWarp xWarp;
	Zenith_HumanLandmarks xL;
	ZENITH_ASSERT_TRUE(SkinDeformMakeWarpForSynth(xBody, xWarp, xL), "warp must build");

	const float afY[7] = { -1.0404f, -0.92f, -0.48f, 0.0f, 0.435f, 1.02f, 1.5f };
	const float afArmW[3] = { 0.0f, 0.5f, 1.0f };
	for (u_int y = 0u; y < 7u; ++y)
	{
		for (u_int w = 0u; w < 3u; ++w)
		{
			float fRingY = afY[y];
			float fRingCx = 0.30f;
			ZENITH_ASSERT_TRUE(Zenith_SkinWarpRing(fRingY, fRingCx, afArmW[w], xWarp), "ring form must apply");

			Zenith_Maths::Vector3 xP(0.30f, afY[y], 0.0f);
			glm::uvec4 xIdx(SD_RUARM, SD_SPINE, 0u, 0u);
			glm::vec4 xWeights(afArmW[w], 1.0f - afArmW[w], 0.0f, 0.0f);
			Zenith_SkinDeformView xView;
			xView.m_pxPositions = &xP;
			xView.m_pxBoneIndices = &xIdx;
			xView.m_pxBoneWeights = &xWeights;
			xView.m_uNumVerts = 1u;
			ZENITH_ASSERT_TRUE(Zenith_SkinWarpVertices(xView, xWarp), "vertex form must apply");

			ZENITH_ASSERT_EQ_FLOAT(fRingY, xP.y, 1.0e-5f, "ring and vertex forms must map Y identically");
			ZENITH_ASSERT_EQ_FLOAT(fRingCx, xP.x, 1.0e-5f, "...and X identically");
		}
	}
}

ZENITH_TEST(SkinDeform, WarpIsDeterministic)
{
	SkinDeformSynthBody xA, xB;
	SkinDeformBuildSynthBody(xA, true);
	SkinDeformBuildSynthBody(xB, true);
	Zenith_HumanWarp xWarpA, xWarpB;
	Zenith_HumanLandmarks xLA, xLB;
	ZENITH_ASSERT_TRUE(SkinDeformMakeWarpForSynth(xA, xWarpA, xLA), "warp A");
	ZENITH_ASSERT_TRUE(SkinDeformMakeWarpForSynth(xB, xWarpB, xLB), "warp B");

	Zenith_SkinDeformView xVA = xA.View();
	Zenith_SkinDeformView xVB = xB.View();
	Zenith_SkinWarpVertices(xVA, xWarpA);
	Zenith_SkinWarpVertices(xVB, xWarpB);

	ZENITH_ASSERT_EQ(xVA.m_uNumVerts, xVB.m_uNumVerts, "same input, same vertex count");
	bool bIdentical = true;
	for (u_int v = 0u; v < xVA.m_uNumVerts && bIdentical; ++v)
	{
		bIdentical = (xVA.m_pxPositions[v] == xVB.m_pxPositions[v]);
	}
	ZENITH_ASSERT_TRUE(bIdentical, "the same input twice must produce bit-identical output");
}

//------------------------------------------------------------------------------
// The rebind
ZENITH_TEST(SkinDeform, SkinDeformCompilesAndLinksWithoutTools)
{
	// The point of this file living in AssetHandling rather than Tools. If the
	// deformation math ever drifts behind a ZENITH_TOOLS guard, Zenithmon's
	// headless ZM_Gen gate stops exercising it and this test stops existing.
	Zenith_HumanWarp xWarp;
	ZENITH_ASSERT_TRUE(!xWarp.IsValid(), "a default-constructed warp is not usable");
	ZENITH_ASSERT_EQ_FLOAT(xWarp.MapY(1.234f, 0.5f), 1.234f, 1.0e-6f, "and maps Y to itself");
	ZENITH_ASSERT_TRUE(Zenith_HumanProportionsRealistic().IsOrdered(), "the table links in every config");
}
