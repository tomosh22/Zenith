//=============================================================================
// Zenith_Tools_FallenTreeAssetExport unit tests. Included from the bottom of
// Zenith_Tools_FallenTreeAssetExport.cpp (inside its ZENITH_TOOLS branch), so
// the anonymous-namespace helpers are in scope.
//
// Same discipline as the rock set's tests: rebuild geometry in memory and assert
// a PROPERTY, chosen so that a defect which is invisible in a render still fails
// here. The two that bit during authoring were an inverted smooth normal (reads
// as "the albedo is too dark") and a wrapped lattice of period 1 (reads as
// "the bark has no relief"), so both have a test.
//=============================================================================

#include "Core/Zenith_TestFramework.h"

#ifdef ZENITH_TESTING

namespace
{
	const int iFALLEN_TREE_SHAPES = static_cast<int>(FALLEN_TREE_VARIANT_COUNT);

	// Build one production variant exactly as the exporter does, settle included.
	void FallenTree_BuildVariant(Zenith_MeshAsset& xMesh, int iVariant)
	{
		xMesh.Reserve(8192u, 24576u);
		BuildFallenTreeVariantMesh(xMesh, FallenTreeVariantAt(iVariant));
	}
}

//-----------------------------------------------------------------------------
// The anisotropic tileable noise the bark is built on.
//-----------------------------------------------------------------------------

ZENITH_TEST(FallenTreeAssets, AnisotropicTileableNoiseWrapsOnBothAxes)
{
	// The cylindrical unwrap meets the texture's own edge on every log, and the
	// ANGULAR seam meets it once per ring. A lattice that does not wrap puts a
	// hard line down the length of every trunk.
	for (int i = 0; i < 48; i++)
	{
		const float fT = static_cast<float>(i) / 48.0f;
		ZENITH_ASSERT_EQ_FLOAT(Zenith_TerrainNoise::TileableValueNoise(0.0f, fT, 18, 3, 3301u),
			Zenith_TerrainNoise::TileableValueNoise(1.0f, fT, 18, 3, 3301u), 1e-6f,
			"anisotropic TileableValueNoise does not wrap in u");
		ZENITH_ASSERT_EQ_FLOAT(Zenith_TerrainNoise::TileableValueNoise(fT, 0.0f, 18, 3, 3301u),
			Zenith_TerrainNoise::TileableValueNoise(fT, 1.0f, 18, 3, 3301u), 1e-6f,
			"anisotropic TileableValueNoise does not wrap in v");
		ZENITH_ASSERT_EQ_FLOAT(Zenith_TerrainNoise::TileableFBM(0.0f, fT, 18, 3, 3u, 77u),
			Zenith_TerrainNoise::TileableFBM(1.0f, fT, 18, 3, 3u, 77u), 1e-6f,
			"anisotropic TileableFBM does not wrap in u");
	}

	// The isotropic convenience overload must agree with the two-period form --
	// the rock set calls it, so a divergence would silently re-texture the rocks.
	for (int i = 0; i < 32; i++)
	{
		const float fA = static_cast<float>(i) * 0.031f;
		const float fB = 1.0f - fA;
		ZENITH_ASSERT_EQ_FLOAT(Zenith_TerrainNoise::TileableValueNoise(fA, fB, 7, 991u),
			Zenith_TerrainNoise::TileableValueNoise(fA, fB, 7, 7, 991u), 0.0f,
			"the isotropic overload disagrees with the two-period form");
	}
}

ZENITH_TEST(FallenTreeAssets, ALatticePeriodOfOneWouldBeConstant)
{
	// ★ This is a REGRESSION GUARD ON A REAL DEFECT, not a curiosity. A wrapped
	// lattice of period 1 has exactly ONE distinct sample, so both interpolants
	// are the same value and the field is CONSTANT everywhere -- the bark relief
	// simply vanishes, which looks like a tuning problem rather than a bug. The
	// generator's first version passed period 1 along the trunk. SurfaceRadius
	// now clamps to >= 2, and this pins both halves of that.
	const float fA = Zenith_TerrainNoise::TileableValueNoise(0.13f, 0.20f, 8, 1, 5u);
	const float fB = Zenith_TerrainNoise::TileableValueNoise(0.13f, 0.81f, 8, 1, 5u);
	ZENITH_ASSERT_EQ_FLOAT(fA, fB, 0.0f,
		"a period-1 axis is expected to be degenerate -- if this ever varies, the clamp "
		"in SurfaceRadius is no longer needed and this test is stale");

	// ...and period 2 is not degenerate, which is what the clamp buys.
	float fMin = 2.0f;
	float fMax = -1.0f;
	for (int i = 0; i < 64; i++)
	{
		const float fV = static_cast<float>(i) / 64.0f;
		const float fN = Zenith_TerrainNoise::TileableValueNoise(0.13f, fV, 8, 2, 5u);
		fMin = std::min(fMin, fN);
		fMax = std::max(fMax, fN);
	}
	ZENITH_ASSERT_GT(fMax - fMin, 0.05f, "a period-2 axis is still degenerate");
}

ZENITH_TEST(FallenTreeAssets, BarkHeightFieldIsAnisotropicAlongTheTrunk)
{
	// The whole point of the two-period noise: ridges must run ALONG the trunk,
	// so the field has to change faster across U than along V. An isotropic bark
	// map reads as gravel wrapped round a cylinder.
	BarkTextureParams xBark;
	double dAcrossGrain = 0.0;
	double dAlongGrain = 0.0;
	const int iN = 96;
	for (int iY = 0; iY < iN; iY++)
	{
		for (int iX = 0; iX < iN; iX++)
		{
			const float fU = static_cast<float>(iX) / iN;
			const float fV = static_cast<float>(iY) / iN;
			const float fStep = 1.0f / iN;
			float fIgnored = 0.0f;
			const float fH = BarkSurfaceHeight(xBark, fU, fV, fIgnored);
			const float fHU = BarkSurfaceHeight(xBark, fU + fStep, fV, fIgnored);
			const float fHV = BarkSurfaceHeight(xBark, fU, fV + fStep, fIgnored);
			dAcrossGrain += std::abs(fHU - fH);
			dAlongGrain += std::abs(fHV - fH);
		}
	}
	ZENITH_ASSERT_GT(dAcrossGrain, dAlongGrain * 1.8,
		"the bark height field is not meaningfully anisotropic -- ridges will not read "
		"as running along the trunk");
}

ZENITH_TEST(FallenTreeAssets, BarkHeightAndFurrowStayInRange)
{
	BarkTextureParams xBark;
	BarkTextureParams xMossy;
	xMossy.m_fMossAmount = 0.78f;
	xMossy.m_uSeed = 6607u;

	for (int iY = 0; iY < 48; iY++)
	{
		for (int iX = 0; iX < 48; iX++)
		{
			const float fU = static_cast<float>(iX) / 48.0f;
			const float fV = static_cast<float>(iY) / 48.0f;
			float fFurrow = -1.0f;
			const float fH = BarkSurfaceHeight(xBark, fU, fV, fFurrow);
			ZENITH_ASSERT_GE(fH, 0.0f, "bark height below 0");
			ZENITH_ASSERT_LE(fH, 1.0f, "bark height above 1");
			ZENITH_ASSERT_GE(fFurrow, 0.0f, "bark furrow mask below 0");
			ZENITH_ASSERT_LE(fFurrow, 1.0f, "bark furrow mask above 1");

			float fFurrowM = -1.0f;
			const float fHM = BarkSurfaceHeight(xMossy, fU, fV, fFurrowM);
			ZENITH_ASSERT_GE(fHM, 0.0f, "mossy bark height below 0");
			ZENITH_ASSERT_LE(fHM, 1.0f, "mossy bark height above 1");
		}
	}
}

//-----------------------------------------------------------------------------
// The trunk profile.
//-----------------------------------------------------------------------------

ZENITH_TEST(FallenTreeAssets, TrunkTapersAndTheStumpFlares)
{
	// A trunk that does not narrow is a pipe; a stump without a root flare is a
	// bollard. Both are the silhouette, so both are worth pinning.
	const LogShapeParams xLog = FallenTreeVariantAt(FALLEN_TREE_LOG).m_xShape;
	float fPrev = LogRadiusAt(xLog, 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(fPrev, xLog.m_fButtRadius, 1e-4f, "the butt radius is not m_fButtRadius");
	for (int i = 1; i <= 20; i++)
	{
		const float fT = static_cast<float>(i) / 20.0f;
		const float fR = LogRadiusAt(xLog, fT);
		ZENITH_ASSERT_LE(fR, fPrev + 1e-5f, "the trunk radius must decrease monotonically");
		fPrev = fR;
	}
	ZENITH_ASSERT_EQ_FLOAT(fPrev, xLog.m_fTipRadius, 1e-4f, "the tip radius is not m_fTipRadius");

	const LogShapeParams xStump = FallenTreeVariantAt(FALLEN_TREE_STUMP).m_xShape;
	ZENITH_ASSERT_GT(LogRadiusAt(xStump, 0.0f), LogRadiusAt(xStump, 0.30f) * 1.15f,
		"the stump has no root flare at its base");
}

ZENITH_TEST(FallenTreeAssets, TheAxisActuallyBends)
{
	// A straight axis would make LogAxisFrame's derivative pointless and the
	// piece read as an extrusion.
	const LogShapeParams xLog = FallenTreeVariantAt(FALLEN_TREE_LOG).m_xShape;
	const Zenith_Maths::Vector3 xStart = LogAxisPoint(xLog, 0.0f);
	const Zenith_Maths::Vector3 xEnd = LogAxisPoint(xLog, 1.0f);
	ZENITH_ASSERT_EQ_FLOAT(xStart.x, 0.0f, 1e-5f, "the axis must start on the origin");
	ZENITH_ASSERT_EQ_FLOAT(xStart.y, 0.0f, 1e-5f, "the axis must start on the origin");
	ZENITH_ASSERT_EQ_FLOAT(xEnd.y, xLog.m_fLengthMetres, 1e-4f,
		"the axis must span m_fLengthMetres in Y");

	float fMaxLateral = 0.0f;
	for (int i = 0; i <= 20; i++)
	{
		const Zenith_Maths::Vector3 xP = LogAxisPoint(xLog, static_cast<float>(i) / 20.0f);
		fMaxLateral = std::max(fMaxLateral, std::sqrt(xP.x * xP.x + xP.z * xP.z));
	}
	ZENITH_ASSERT_GT(fMaxLateral, 0.10f, "the trunk axis is effectively straight");
}

//-----------------------------------------------------------------------------
// The emitted mesh.
//-----------------------------------------------------------------------------

ZENITH_TEST(FallenTreeAssets, EveryPieceRestsOnTheOriginPlane)
{
	// The scatter places an instance AT the sampled terrain height with no
	// per-mesh offset table, so the lowest vertex has to be exactly y=0. A broken
	// butt cap's splinter offsets push geometry below the axis origin, which is
	// why SettleMeshOntoGround exists and why this is not free.
	for (int iVariant = 0; iVariant < iFALLEN_TREE_SHAPES; iVariant++)
	{
		Zenith_MeshAsset xMesh;
		FallenTree_BuildVariant(xMesh, iVariant);
		ZENITH_ASSERT_GT(xMesh.GetNumVerts(), 0u, "variant emitted no geometry");

		float fMinY = 1.0e30f;
		float fMaxY = -1.0e30f;
		for (u_int u = 0; u < xMesh.GetNumVerts(); u++)
		{
			fMinY = std::min(fMinY, xMesh.m_xPositions.Get(u).y);
			fMaxY = std::max(fMaxY, xMesh.m_xPositions.Get(u).y);
		}
		ZENITH_ASSERT_EQ_FLOAT(fMinY, 0.0f, 1e-4f, "the piece does not rest on y=0");
		ZENITH_ASSERT_GT(fMaxY, 0.20f, "the piece has no height at all");
	}
}

ZENITH_TEST(FallenTreeAssets, SinglePiecesSpanTheirAuthoredLength)
{
	// m_fLengthMetres has to MEAN metres: the scatter's collider half-height and
	// bounds radius are both derived from it by hand.
	for (int iVariant = 0; iVariant < iFALLEN_TREE_SHAPES; iVariant++)
	{
		const FallenTreeVariantSpec xSpec = FallenTreeVariantAt(iVariant);
		if (xSpec.m_uClusterPieces > 1u)
		{
			continue;   // a tangle has no single length
		}
		Zenith_MeshAsset xMesh;
		FallenTree_BuildVariant(xMesh, iVariant);

		float fMaxY = -1.0e30f;
		for (u_int u = 0; u < xMesh.GetNumVerts(); u++)
		{
			fMaxY = std::max(fMaxY, xMesh.m_xPositions.Get(u).y);
		}
		// A BAND, not a computed tolerance. The extent legitimately overshoots the
		// axis length by the splinter depth at each end, by the root flare, and by
		// the rim of an end cap that a bent axis has tilted out of horizontal --
		// and a test that re-derived all three from the shape parameters would be
		// asserting against its own copy of the generator rather than against the
		// property that matters. What matters is that m_fLengthMetres MEANS
		// metres: the scatter's collider half-height and bounds radius are both
		// worked out from it by hand, so an error of tens of percent is a real
		// defect and single percent is not.
		// The band is wide because the overshoot is REAL and varies by variant: the
		// stump's root flare plus its 0.40 splinter depth put ~28% on top of the
		// axis length. That is worth knowing rather than hiding -- its capsule is
		// sized from the mesh height for exactly this reason. The EXACT contract
		// (LogAxisPoint(1).y == m_fLengthMetres) is pinned by TheAxisActuallyBends;
		// what this adds is that the mesh does not wander off from its axis.
		ZENITH_ASSERT_GE(fMaxY, xSpec.m_xShape.m_fLengthMetres * 0.95f,
			"the piece is shorter than its own axis");
		ZENITH_ASSERT_LE(fMaxY, xSpec.m_xShape.m_fLengthMetres * 1.35f,
			"the piece overshoots its axis length by more than the ends can account for");
	}
}

ZENITH_TEST(FallenTreeAssets, EveryEmittedTriangleFacesOutward)
{
	// Same engine convention and the same silent failure as the rock set: an
	// inverted triangle still SHADES correctly and only vanishes under back-face
	// culling. A bent, capped tube is not star-shaped about any point, so the
	// check is against the triangle's own vertex normals rather than a centre.
	for (int iVariant = 0; iVariant < iFALLEN_TREE_SHAPES; iVariant++)
	{
		Zenith_MeshAsset xMesh;
		FallenTree_BuildVariant(xMesh, iVariant);
		ZENITH_ASSERT_GT(xMesh.GetNumIndices(), 300u, "variant emitted almost no geometry");

		for (u_int uTri = 0; uTri < xMesh.GetNumIndices(); uTri += 3u)
		{
			const u_int auIdx[3] = { xMesh.m_xIndices.Get(uTri),
				xMesh.m_xIndices.Get(uTri + 1u), xMesh.m_xIndices.Get(uTri + 2u) };
			const Zenith_Maths::Vector3 xA = xMesh.m_xPositions.Get(auIdx[0]);
			const Zenith_Maths::Vector3 xB = xMesh.m_xPositions.Get(auIdx[1]);
			const Zenith_Maths::Vector3 xC = xMesh.m_xPositions.Get(auIdx[2]);
			const Zenith_Maths::Vector3 xFace = glm::cross(xC - xA, xB - xA);
			ZENITH_ASSERT_GT(glm::dot(xFace, xFace), 1.0e-12f,
				"a degenerate triangle was emitted");

			const Zenith_Maths::Vector3 xAvgNormal = xMesh.m_xNormals.Get(auIdx[0])
				+ xMesh.m_xNormals.Get(auIdx[1]) + xMesh.m_xNormals.Get(auIdx[2]);
			ZENITH_ASSERT_GT(glm::dot(glm::normalize(xFace), glm::normalize(xAvgNormal)), 0.20f,
				"the winding disagrees with the surface normals (would be back-face culled)");
		}
	}
}

ZENITH_TEST(FallenTreeAssets, NormalsAndTangentsAreOrthonormal)
{
	for (int iVariant = 0; iVariant < iFALLEN_TREE_SHAPES; iVariant++)
	{
		Zenith_MeshAsset xMesh;
		FallenTree_BuildVariant(xMesh, iVariant);
		for (u_int u = 0; u < xMesh.GetNumVerts(); u++)
		{
			const Zenith_Maths::Vector3 xN = xMesh.m_xNormals.Get(u);
			const Zenith_Maths::Vector3 xT = xMesh.m_xTangents.Get(u);
			ZENITH_ASSERT_EQ_FLOAT(glm::length(xN), 1.0f, 1e-3f, "normal is not unit length");
			ZENITH_ASSERT_EQ_FLOAT(glm::length(xT), 1.0f, 1e-3f, "tangent is not unit length");
			ZENITH_ASSERT_LE(std::abs(glm::dot(xN, xT)), 1e-3f,
				"tangent is not perpendicular to its shading normal");
		}
	}
}

ZENITH_TEST(FallenTreeAssets, CylindricalUVsAreNotStretched)
{
	// A cylindrical unwrap of a tube is very nearly isometric, so UV area over
	// world area should sit close to 1/tile^2. The end caps map V radially and
	// the stubs use their own frame, so this is a BAND rather than a point --
	// but a smeared flank (the failure a box projection would give here) lands
	// far outside it.
	for (int iVariant = 0; iVariant < iFALLEN_TREE_SHAPES; iVariant++)
	{
		const FallenTreeVariantSpec xSpec = FallenTreeVariantAt(iVariant);
		Zenith_MeshAsset xMesh;
		FallenTree_BuildVariant(xMesh, iVariant);

		const float fIdeal = 1.0f / (xSpec.m_xShape.m_fUVTileMetres * xSpec.m_xShape.m_fUVTileMetres);

		// Worst case only, and REPORTED: "some triangle is stretched" is not
		// actionable, and this test spent three rounds being guessed at from a bare
		// ratio. The log names the offender so the next reader does not have to.
		float fWorstLow = 1.0e30f;
		float fWorstHigh = -1.0f;
		u_int uWorstLowTri = 0u;
		u_int uWorstHighTri = 0u;
		for (u_int uTri = 0; uTri < xMesh.GetNumIndices(); uTri += 3u)
		{
			const u_int uIA = xMesh.m_xIndices.Get(uTri);
			const u_int uIB = xMesh.m_xIndices.Get(uTri + 1u);
			const u_int uIC = xMesh.m_xIndices.Get(uTri + 2u);
			const Zenith_Maths::Vector3 xA = xMesh.m_xPositions.Get(uIA);
			const Zenith_Maths::Vector3 xB = xMesh.m_xPositions.Get(uIB);
			const Zenith_Maths::Vector3 xC = xMesh.m_xPositions.Get(uIC);
			const float fWorldArea = 0.5f * glm::length(glm::cross(xB - xA, xC - xA));
			if (fWorldArea < 1.0e-5f)
			{
				continue;
			}
			const Zenith_Maths::Vector2 xUA = xMesh.m_xUVs.Get(uIA);
			const Zenith_Maths::Vector2 xUB = xMesh.m_xUVs.Get(uIB);
			const Zenith_Maths::Vector2 xUC = xMesh.m_xUVs.Get(uIC);
			const float fUVArea = 0.5f * std::abs(
				(xUB.x - xUA.x) * (xUC.y - xUA.y) - (xUC.x - xUA.x) * (xUB.y - xUA.y));
			const float fRatio = fUVArea / fWorldArea;
			if (fRatio < fWorstLow) { fWorstLow = fRatio; uWorstLowTri = uTri; }
			if (fRatio > fWorstHigh) { fWorstHigh = fRatio; uWorstHighTri = uTri; }
		}

		if (fWorstLow < fIdeal * 0.10f || fWorstHigh > fIdeal * 3.0f)
		{
			const u_int uTri = (fWorstLow < fIdeal * 0.10f) ? uWorstLowTri : uWorstHighTri;
			const u_int uIA = xMesh.m_xIndices.Get(uTri);
			const u_int uIB = xMesh.m_xIndices.Get(uTri + 1u);
			const u_int uIC = xMesh.m_xIndices.Get(uTri + 2u);
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[FallenTreeUV] %s worst low %.4f (tri %u) high %.4f, ideal %.4f -- "
				"offender P0(%.3f,%.3f,%.3f) P1(%.3f,%.3f,%.3f) P2(%.3f,%.3f,%.3f) "
				"UV0(%.3f,%.3f) UV1(%.3f,%.3f) UV2(%.3f,%.3f)",
				xSpec.m_szName, fWorstLow, uWorstLowTri, fWorstHigh, fIdeal,
				xMesh.m_xPositions.Get(uIA).x, xMesh.m_xPositions.Get(uIA).y, xMesh.m_xPositions.Get(uIA).z,
				xMesh.m_xPositions.Get(uIB).x, xMesh.m_xPositions.Get(uIB).y, xMesh.m_xPositions.Get(uIB).z,
				xMesh.m_xPositions.Get(uIC).x, xMesh.m_xPositions.Get(uIC).y, xMesh.m_xPositions.Get(uIC).z,
				xMesh.m_xUVs.Get(uIA).x, xMesh.m_xUVs.Get(uIA).y,
				xMesh.m_xUVs.Get(uIB).x, xMesh.m_xUVs.Get(uIB).y,
				xMesh.m_xUVs.Get(uIC).x, xMesh.m_xUVs.Get(uIC).y);
		}

		ZENITH_ASSERT_LE(fWorstHigh, fIdeal * 3.0f, "UVs are compressed far past one tile per metre");
		ZENITH_ASSERT_GE(fWorstLow, fIdeal * 0.10f, "UVs are stretched into a smear");
	}
}

ZENITH_TEST(FallenTreeAssets, VertexTintDarkensOnlyTheFurrows)
{
	// Same contract as the rock set's cavity tint: it MULTIPLIES albedo, so it
	// must darken from an unmodulated baseline rather than dimming the whole
	// piece. (The rock generator shipped that bug once.)
	for (int iVariant = 0; iVariant < iFALLEN_TREE_SHAPES; iVariant++)
	{
		Zenith_MeshAsset xMesh;
		FallenTree_BuildVariant(xMesh, iVariant);

		float fMinTint = 2.0f;
		float fMaxTint = -1.0f;
		for (u_int u = 0; u < xMesh.GetNumVerts(); u++)
		{
			const Zenith_Maths::Vector4 xColour = xMesh.m_xColors.Get(u);
			ZENITH_ASSERT_GT(xColour.x, 0.0f, "vertex tint is not positive");
			ZENITH_ASSERT_LE(xColour.x, 1.0f, "vertex tint brightens past the authored albedo");
			ZENITH_ASSERT_EQ_FLOAT(xColour.w, 1.0f, 1e-5f,
				"vertex colour alpha must be 1 or the shader ignores the tint entirely");
			fMinTint = std::min(fMinTint, xColour.x);
			fMaxTint = std::max(fMaxTint, xColour.x);
		}
		ZENITH_ASSERT_GT(fMaxTint, 0.95f,
			"nothing is left unmodulated -- the tint is dimming the whole piece");
		ZENITH_ASSERT_LT(fMinTint, 0.92f, "the tint has no depth at all");
	}
}

ZENITH_TEST(FallenTreeAssets, GenerationIsDeterministicForAFixedSeed)
{
	Zenith_MeshAsset xFirst;
	Zenith_MeshAsset xSecond;
	FallenTree_BuildVariant(xFirst, FALLEN_TREE_LOG);
	FallenTree_BuildVariant(xSecond, FALLEN_TREE_LOG);

	ZENITH_ASSERT_EQ(xFirst.GetNumVerts(), xSecond.GetNumVerts(), "vertex count drifted between runs");
	ZENITH_ASSERT_EQ(xFirst.GetNumIndices(), xSecond.GetNumIndices(), "index count drifted between runs");
	for (u_int u = 0; u < xFirst.GetNumVerts(); u++)
	{
		ZENITH_ASSERT_NEAR_VEC3(xFirst.m_xPositions.Get(u), xSecond.m_xPositions.Get(u), 0.0f,
			"vertex position drifted between runs");
		ZENITH_ASSERT_NEAR_VEC3(xFirst.m_xNormals.Get(u), xSecond.m_xNormals.Get(u), 0.0f,
			"vertex normal drifted between runs");
	}
}

ZENITH_TEST(FallenTreeAssets, TheBranchClusterIsSeveralSeparatePieces)
{
	// The tangle is N pieces in ONE mesh. Each append must index only its own
	// vertices -- a base-offset slip would stitch two branches together with a
	// triangle spanning both -- and the pieces must actually be spread out.
	const FallenTreeVariantSpec xSpec = FallenTreeVariantAt(FALLEN_TREE_BRANCHES);
	ZENITH_ASSERT_GT(xSpec.m_uClusterPieces, 1u, "the branch variant is not a cluster");

	Zenith_MeshAsset xMesh;
	FallenTree_BuildVariant(xMesh, FALLEN_TREE_BRANCHES);
	for (u_int u = 0; u < xMesh.GetNumIndices(); u++)
	{
		ZENITH_ASSERT_LT(xMesh.m_xIndices.Get(u), xMesh.GetNumVerts(), "index out of range");
	}

	// Spread: the pieces are offset and independently yawed, so the footprint has
	// to be wider than a single branch's own girth.
	float fMinX = 1.0e30f, fMaxX = -1.0e30f, fMinZ = 1.0e30f, fMaxZ = -1.0e30f;
	for (u_int u = 0; u < xMesh.GetNumVerts(); u++)
	{
		const Zenith_Maths::Vector3& xP = xMesh.m_xPositions.Get(u);
		fMinX = std::min(fMinX, xP.x); fMaxX = std::max(fMaxX, xP.x);
		fMinZ = std::min(fMinZ, xP.z); fMaxZ = std::max(fMaxZ, xP.z);
	}
	ZENITH_ASSERT_GT(std::max(fMaxX - fMinX, fMaxZ - fMinZ), xSpec.m_xShape.m_fButtRadius * 6.0f,
		"the cluster's pieces are stacked on one spot instead of spread into a tangle");
}

#endif // ZENITH_TESTING
