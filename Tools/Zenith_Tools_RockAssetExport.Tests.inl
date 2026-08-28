//=============================================================================
// Zenith_Tools_RockAssetExport unit tests. Included from the bottom of
// Zenith_Tools_RockAssetExport.cpp (inside its ZENITH_TOOLS branch), so the
// anonymous-namespace shape helpers are in scope.
//
// Nothing here touches the filesystem or the GPU: every test rebuilds geometry
// in memory and asserts a PROPERTY of it. The properties are chosen to be the
// ones a rendering defect would violate silently — an inverted triangle shades
// correctly and only disappears under back-face culling, a non-tiling texture
// looks fine until it meets its own wrap seam, and a stretched UV only shows up
// as a smear at a grazing angle.
//=============================================================================

#include "Core/Zenith_TestFramework.h"

#ifdef ZENITH_TESTING

namespace
{
	// The exact centre AppendRockGeometry derives internally for an un-offset,
	// un-rotated, unit-scaled rock: the mean of the SHELL vertices after the full
	// shaping pipeline. Recomputed here rather than exported, so a test cannot
	// pass by agreeing with a bug in a shared accessor.
	Zenith_Maths::Vector3 RockTestShellCentre(const RockShapeParams& xParams, u_int uSeed)
	{
		Zenith_Vector<Zenith_Maths::Vector3> xVerts;
		Zenith_Vector<u_int> xIndices;
		Zenith_Vector<float> xShellRadius;
		Zenith_Vector<float> xRecess;
		BuildIcosphere(xParams.m_uSubdivisions, xVerts, xIndices);
		DisplaceRockShell(xParams, uSeed, xVerts, xShellRadius);
		CutRockFacets(xParams, uSeed, xVerts, xRecess);
		SettleRockOntoBase(xParams, xVerts);

		Zenith_Maths::Vector3 xCentre(0.0f, 0.0f, 0.0f);
		for (u_int u = 0; u < xVerts.GetSize(); u++)
		{
			xCentre = xCentre + xVerts.Get(u);
		}
		return xCentre / static_cast<float>(xVerts.GetSize());
	}

	// The production shapes, read from the ONE table the exporter reads. A test
	// that re-declared them would drift the first time a knob is tuned.
	RockShapeParams RockTestShape(int iIndex, u_int& uSeedOut)
	{
		const RockVariantSpec xSpec = RockVariantAt(iIndex);
		uSeedOut = xSpec.m_uSeed;
		return xSpec.m_xShape;
	}

	const int iROCK_TEST_SHAPES = static_cast<int>(ROCK_VARIANT_COUNT);
}

//-----------------------------------------------------------------------------
// The shared 3D noise the displacement is built on.
//-----------------------------------------------------------------------------

ZENITH_TEST(RockAssets, Noise3DIsBoundedAndDeterministic)
{
	for (int i = 0; i < 200; i++)
	{
		const float fX = static_cast<float>(i) * 0.37f - 18.0f;
		const float fY = static_cast<float>(i) * -0.21f + 4.5f;
		const float fZ = static_cast<float>(i) * 0.13f;
		const float fA = Zenith_TerrainNoise::ValueNoise3D(fX, fY, fZ, 1234u);
		const float fB = Zenith_TerrainNoise::ValueNoise3D(fX, fY, fZ, 1234u);
		ZENITH_ASSERT_EQ_FLOAT(fA, fB, 0.0f, "ValueNoise3D must be a pure function of (x,y,z,seed)");
		ZENITH_ASSERT_GE(fA, 0.0f, "ValueNoise3D below 0");
		ZENITH_ASSERT_LE(fA, 1.0f, "ValueNoise3D above 1");

		const float fFbm = Zenith_TerrainNoise::FBM3D(fX, fY, fZ, 77u, 4u, 2.0f, 0.5f);
		ZENITH_ASSERT_GE(fFbm, 0.0f, "FBM3D below 0");
		ZENITH_ASSERT_LE(fFbm, 1.0f, "FBM3D above 1");
	}
}

ZENITH_TEST(RockAssets, Noise3DIsContinuousAndSeedSensitive)
{
	// Continuity: a lattice step apart may differ a lot, but a hundredth of one
	// may not — that is what stops the displacement pinning a vertex spike.
	float fMaxLocalDelta = 0.0f;
	for (int i = 0; i < 300; i++)
	{
		const float fX = static_cast<float>(i) * 0.011f;
		const float fA = Zenith_TerrainNoise::ValueNoise3D(fX, 2.5f, -1.25f, 909u);
		const float fB = Zenith_TerrainNoise::ValueNoise3D(fX + 0.01f, 2.5f, -1.25f, 909u);
		fMaxLocalDelta = std::max(fMaxLocalDelta, fabsf(fA - fB));
	}
	ZENITH_ASSERT_LT(fMaxLocalDelta, 0.10f, "ValueNoise3D is discontinuous at 0.01 lattice units");

	// Seed sensitivity: two seeds must not agree everywhere, or every rock in a
	// cluster would come out identical.
	int iDiffering = 0;
	for (int i = 0; i < 100; i++)
	{
		const float fP = static_cast<float>(i) * 0.73f;
		if (fabsf(Zenith_TerrainNoise::ValueNoise3D(fP, fP * 0.5f, fP * -0.25f, 11u) -
			Zenith_TerrainNoise::ValueNoise3D(fP, fP * 0.5f, fP * -0.25f, 12u)) > 1e-4f)
		{
			iDiffering++;
		}
	}
	ZENITH_ASSERT_GT(iDiffering, 90, "ValueNoise3D barely responds to the seed");
}

//-----------------------------------------------------------------------------
// Tileability — the property the box-projected UVs depend on. A texture that
// does not wrap shows a hard seam every m_fUVTileMetres, everywhere.
//-----------------------------------------------------------------------------

ZENITH_TEST(RockAssets, RockTexturesTileExactlyAcrossTheWrap)
{
	for (int i = 0; i < 64; i++)
	{
		const float fT = static_cast<float>(i) / 64.0f;

		ZENITH_ASSERT_EQ_FLOAT(TileableValueNoise(0.0f, fT, 8, 4242u),
			TileableValueNoise(1.0f, fT, 8, 4242u), 1e-6f, "TileableValueNoise does not wrap in u");
		ZENITH_ASSERT_EQ_FLOAT(TileableValueNoise(fT, 0.0f, 8, 4242u),
			TileableValueNoise(fT, 1.0f, 8, 4242u), 1e-6f, "TileableValueNoise does not wrap in v");

		ZENITH_ASSERT_EQ_FLOAT(TileableFBM(0.0f, fT, 5, 4u, 77u),
			TileableFBM(1.0f, fT, 5, 4u, 77u), 1e-6f, "TileableFBM does not wrap in u");
		ZENITH_ASSERT_EQ_FLOAT(TileableRidged(fT, 0.0f, 7, 4u, 91u),
			TileableRidged(fT, 1.0f, 7, 4u, 91u), 1e-6f, "TileableRidged does not wrap in v");
	}
}

ZENITH_TEST(RockAssets, SurfaceHeightAndCrackStayInRange)
{
	RockTextureParams xGranite;
	RockTextureParams xSandstone;
	xSandstone.m_fBedding = 0.65f;
	xSandstone.m_uSeed = 8837u;

	for (int iY = 0; iY < 48; iY++)
	{
		for (int iX = 0; iX < 48; iX++)
		{
			const float fU = static_cast<float>(iX) / 48.0f;
			const float fV = static_cast<float>(iY) / 48.0f;
			float fCrack = -1.0f;
			const float fH = RockSurfaceHeight(xGranite, fU, fV, fCrack);
			ZENITH_ASSERT_GE(fH, 0.0f, "granite height below 0");
			ZENITH_ASSERT_LE(fH, 1.0f, "granite height above 1");
			ZENITH_ASSERT_GE(fCrack, 0.0f, "granite crack mask below 0");
			ZENITH_ASSERT_LE(fCrack, 1.0f, "granite crack mask above 1");

			float fCrackS = -1.0f;
			const float fHS = RockSurfaceHeight(xSandstone, fU, fV, fCrackS);
			ZENITH_ASSERT_GE(fHS, 0.0f, "sandstone height below 0");
			ZENITH_ASSERT_LE(fHS, 1.0f, "sandstone height above 1");
		}
	}
}

//-----------------------------------------------------------------------------
// The shell.
//-----------------------------------------------------------------------------

ZENITH_TEST(RockAssets, IcosphereSubdivisionDedupesItsMidpoints)
{
	// V = 10*4^n + 2 and F = 20*4^n hold ONLY if every shared edge midpoint is
	// found in the cache rather than re-created; a broken dedup inflates V and
	// leaves the shell cracked open along every edge.
	u_int uExpectedFaces = 20u;
	u_int uExpectedVerts = 12u;
	for (u_int uSubdiv = 0; uSubdiv <= 3u; uSubdiv++)
	{
		Zenith_Vector<Zenith_Maths::Vector3> xVerts;
		Zenith_Vector<u_int> xIndices;
		BuildIcosphere(uSubdiv, xVerts, xIndices);

		ZENITH_ASSERT_EQ(xIndices.GetSize(), uExpectedFaces * 3u, "icosphere face count");
		ZENITH_ASSERT_EQ(xVerts.GetSize(), uExpectedVerts, "icosphere vertex count (midpoint dedup)");
		for (u_int u = 0; u < xVerts.GetSize(); u++)
		{
			ZENITH_ASSERT_EQ_FLOAT(glm::length(xVerts.Get(u)), 1.0f, 1e-5f,
				"icosphere vertex is not on the unit sphere");
		}
		uExpectedVerts = uExpectedVerts + uExpectedFaces * 3u / 2u;   // one new vertex per edge
		uExpectedFaces *= 4u;
	}
}

ZENITH_TEST(RockAssets, RockSitsOnItsBaseAtTheRequestedWidth)
{
	// Origin-at-base is a contract the scatter relies on: it places an instance
	// AT the sampled terrain height with no per-mesh offset table.
	for (int iShape = 0; iShape < iROCK_TEST_SHAPES; iShape++)
	{
		u_int uSeed = 0u;
		const RockShapeParams xParams = RockTestShape(iShape, uSeed);
		Zenith_Vector<Zenith_Maths::Vector3> xVerts;
		Zenith_Vector<u_int> xIndices;
		Zenith_Vector<float> xShellRadius;
		Zenith_Vector<float> xRecess;
		BuildIcosphere(xParams.m_uSubdivisions, xVerts, xIndices);
		DisplaceRockShell(xParams, uSeed, xVerts, xShellRadius);
		CutRockFacets(xParams, uSeed, xVerts, xRecess);
		SettleRockOntoBase(xParams, xVerts);

		float fMinY = 1.0e30f;
		float fMaxY = -1.0e30f;
		float fMaxHorizontal = 0.0f;
		for (u_int u = 0; u < xVerts.GetSize(); u++)
		{
			const Zenith_Maths::Vector3& xP = xVerts.Get(u);
			fMinY = std::min(fMinY, xP.y);
			fMaxY = std::max(fMaxY, xP.y);
			fMaxHorizontal = std::max(fMaxHorizontal, sqrtf(xP.x * xP.x + xP.z * xP.z));
		}
		ZENITH_ASSERT_EQ_FLOAT(fMinY, 0.0f, 1e-4f, "rock does not rest on y=0");
		ZENITH_ASSERT_GT(fMaxY, 0.05f, "rock has no height at all");
		ZENITH_ASSERT_EQ_FLOAT(fMaxHorizontal * 2.0f, xParams.m_fWidthMetres, 1e-3f,
			"rock width does not match m_fWidthMetres");
	}
}

//-----------------------------------------------------------------------------
// The emitted mesh.
//-----------------------------------------------------------------------------

ZENITH_TEST(RockAssets, EveryEmittedTriangleFacesOutward)
{
	// The engine's proven cull convention: a triangle (A,B,C) is front-facing iff
	// cross(C-A, B-A) points OUTWARD. An inverted rock still SHADES correctly
	// (the normals are authored outward regardless), so back-face culling is the
	// only thing that reveals it — which is exactly why this is a test and not an
	// eyeball check.
	for (int iShape = 0; iShape < iROCK_TEST_SHAPES; iShape++)
	{
		u_int uSeed = 0u;
		const RockShapeParams xParams = RockTestShape(iShape, uSeed);
		const Zenith_Maths::Vector3 xCentre = RockTestShellCentre(xParams, uSeed);

		Zenith_MeshAsset xMesh;
		xMesh.Reserve(4096u, 12288u);
		AppendRockGeometry(xMesh, xParams, uSeed, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), 0.0f, 1.0f);

		ZENITH_ASSERT_GT(xMesh.GetNumIndices(), 900u, "rock emitted almost no geometry");
		for (u_int uTri = 0; uTri < xMesh.GetNumIndices(); uTri += 3u)
		{
			const Zenith_Maths::Vector3 xA = xMesh.m_xPositions.Get(xMesh.m_xIndices.Get(uTri));
			const Zenith_Maths::Vector3 xB = xMesh.m_xPositions.Get(xMesh.m_xIndices.Get(uTri + 1u));
			const Zenith_Maths::Vector3 xC = xMesh.m_xPositions.Get(xMesh.m_xIndices.Get(uTri + 2u));
			const Zenith_Maths::Vector3 xFace = glm::cross(xC - xA, xB - xA);
			const Zenith_Maths::Vector3 xCentroid = (xA + xB + xC) / 3.0f;
			ZENITH_ASSERT_GT(glm::dot(xFace, xCentroid - xCentre), 0.0f,
				"rock triangle is wound inward (would be back-face culled)");
		}
	}
}

ZENITH_TEST(RockAssets, NoDegenerateTrianglesSurviveTheFacetCuts)
{
	// A half-space cut projects whole triangles onto its plane; those must be
	// DROPPED, not emitted with an undefined normal.
	for (int iShape = 0; iShape < iROCK_TEST_SHAPES; iShape++)
	{
		u_int uSeed = 0u;
		const RockShapeParams xParams = RockTestShape(iShape, uSeed);
		Zenith_MeshAsset xMesh;
		xMesh.Reserve(4096u, 12288u);
		AppendRockGeometry(xMesh, xParams, uSeed, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), 0.0f, 1.0f);

		for (u_int uTri = 0; uTri < xMesh.GetNumIndices(); uTri += 3u)
		{
			const Zenith_Maths::Vector3 xA = xMesh.m_xPositions.Get(xMesh.m_xIndices.Get(uTri));
			const Zenith_Maths::Vector3 xB = xMesh.m_xPositions.Get(xMesh.m_xIndices.Get(uTri + 1u));
			const Zenith_Maths::Vector3 xC = xMesh.m_xPositions.Get(xMesh.m_xIndices.Get(uTri + 2u));
			const Zenith_Maths::Vector3 xFace = glm::cross(xC - xA, xB - xA);
			ZENITH_ASSERT_GT(glm::dot(xFace, xFace), 1.0e-12f, "degenerate rock triangle was emitted");
		}
	}
}

ZENITH_TEST(RockAssets, BoxUVDensityIsBoundedInBothDirections)
{
	// Box projection scales UV area by cos(theta)/tile^2, where theta is the angle
	// between the face normal and the axis it was projected along. Choosing the
	// DOMINANT axis bounds cos(theta) below by 1/sqrt(3), so the texel density can
	// never vary by more than that factor — which is the whole reason this
	// unwrap was chosen over a sphere map, and therefore worth pinning.
	for (int iShape = 0; iShape < iROCK_TEST_SHAPES; iShape++)
	{
		u_int uSeed = 0u;
		const RockShapeParams xParams = RockTestShape(iShape, uSeed);
		Zenith_MeshAsset xMesh;
		xMesh.Reserve(4096u, 12288u);
		AppendRockGeometry(xMesh, xParams, uSeed, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), 0.0f, 1.0f);

		const float fIdeal = 1.0f / (xParams.m_fUVTileMetres * xParams.m_fUVTileMetres);
		const float fFloor = fIdeal * 0.5773f;   // 1/sqrt(3), the dominant-axis bound
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
				continue;   // too small to measure a ratio through
			}
			const Zenith_Maths::Vector2 xUA = xMesh.m_xUVs.Get(uIA);
			const Zenith_Maths::Vector2 xUB = xMesh.m_xUVs.Get(uIB);
			const Zenith_Maths::Vector2 xUC = xMesh.m_xUVs.Get(uIC);
			const float fUVArea = 0.5f * fabsf(
				(xUB.x - xUA.x) * (xUC.y - xUA.y) - (xUC.x - xUA.x) * (xUB.y - xUA.y));
			const float fRatio = fUVArea / fWorldArea;
			ZENITH_ASSERT_LE(fRatio, fIdeal * 1.001f, "rock UVs are denser than one tile per metre allows");
			ZENITH_ASSERT_GE(fRatio, fFloor * 0.999f, "rock UVs are stretched past the dominant-axis bound");
		}
	}
}

ZENITH_TEST(RockAssets, NormalsAndTangentsAreOrthonormalAndOutward)
{
	// The shading normal is lerp(faceNormal, smoothNormal): both inputs are supposed
	// to point outward, so the result must agree with the face it belongs to. It is
	// the SMOOTH one that can silently invert -- ComputeShellNormals accumulates
	// cross(C-A, B-A) and therefore inherits whatever winding BuildIcosphere left,
	// while the emitter fixes its own winding per triangle and so cannot notice.
	// That combination shipped once: the boulder (m_fNormalSmooth 0.62) ended up
	// shading with a mostly-INWARD normal, which looks like an albedo problem
	// rather than a geometry one. Comparing against the face normal is what names
	// it; the radial check alone passes on the shard, which blends only 0.22.
	for (int iShape = 0; iShape < iROCK_TEST_SHAPES; iShape++)
	{
		u_int uSeed = 0u;
		const RockShapeParams xParams = RockTestShape(iShape, uSeed);
		const Zenith_Maths::Vector3 xCentre = RockTestShellCentre(xParams, uSeed);

		Zenith_MeshAsset xMesh;
		xMesh.Reserve(4096u, 12288u);
		AppendRockGeometry(xMesh, xParams, uSeed, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), 0.0f, 1.0f);

		for (u_int uTri = 0; uTri < xMesh.GetNumIndices(); uTri += 3u)
		{
			const u_int auIdx[3] = {
				xMesh.m_xIndices.Get(uTri), xMesh.m_xIndices.Get(uTri + 1u), xMesh.m_xIndices.Get(uTri + 2u) };
			const Zenith_Maths::Vector3 xA = xMesh.m_xPositions.Get(auIdx[0]);
			const Zenith_Maths::Vector3 xB = xMesh.m_xPositions.Get(auIdx[1]);
			const Zenith_Maths::Vector3 xC = xMesh.m_xPositions.Get(auIdx[2]);
			const Zenith_Maths::Vector3 xFace = glm::normalize(glm::cross(xC - xA, xB - xA));

			for (u_int uCorner = 0; uCorner < 3u; uCorner++)
			{
				const u_int u = auIdx[uCorner];
				const Zenith_Maths::Vector3 xN = xMesh.m_xNormals.Get(u);
				const Zenith_Maths::Vector3 xT = xMesh.m_xTangents.Get(u);
				ZENITH_ASSERT_EQ_FLOAT(glm::length(xN), 1.0f, 1e-3f, "rock normal is not unit length");
				ZENITH_ASSERT_EQ_FLOAT(glm::length(xT), 1.0f, 1e-3f, "rock tangent is not unit length");
				ZENITH_ASSERT_LE(fabsf(glm::dot(xN, xT)), 1e-3f,
					"rock tangent is not perpendicular to its shading normal");
				ZENITH_ASSERT_GT(glm::dot(xN, xFace), 0.25f,
					"shading normal disagrees with the face it belongs to (smooth normals inverted?)");
				ZENITH_ASSERT_GT(glm::dot(xN, xMesh.m_xPositions.Get(u) - xCentre), -0.35f,
					"rock normal points back into the body");
			}
		}
	}
}

ZENITH_TEST(RockAssets, VertexCavityTintDarkensOnlyTheHollows)
{
	// The tint MULTIPLIES albedo, so this pins BOTH failure directions. Above 1
	// would brighten the stone past its authored colour. Never reaching 1 is the
	// regression that actually shipped: the first pass derived cavity from
	// |p - centre| / maxLen, which on the elongated shard put almost every vertex
	// below the ramp's lower edge, applied a FLAT 0.55x to the whole stone, and
	// rendered it near-black. A tint with no baseline is an albedo cut in disguise.
	for (int iShape = 0; iShape < iROCK_TEST_SHAPES; iShape++)
	{
		u_int uSeed = 0u;
		const RockShapeParams xParams = RockTestShape(iShape, uSeed);
		Zenith_MeshAsset xMesh;
		xMesh.Reserve(4096u, 12288u);
		AppendRockGeometry(xMesh, xParams, uSeed, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), 0.0f, 1.0f);

		float fMinTint = 2.0f;
		float fMaxTint = -1.0f;
		for (u_int u = 0; u < xMesh.GetNumVerts(); u++)
		{
			const Zenith_Maths::Vector4 xColour = xMesh.m_xColors.Get(u);
			ZENITH_ASSERT_GT(xColour.x, 0.0f, "vertex cavity tint is not positive");
			ZENITH_ASSERT_LE(xColour.x, 1.0f, "vertex cavity tint brightens past the authored albedo");
			ZENITH_ASSERT_EQ_FLOAT(xColour.w, 1.0f, 1e-5f,
				"vertex colour alpha must be 1 or the shader ignores the tint entirely");
			fMinTint = std::min(fMinTint, xColour.x);
			fMaxTint = std::max(fMaxTint, xColour.x);
		}
		ZENITH_ASSERT_GT(fMaxTint, 0.95f,
			"no vertex is left unmodulated — the tint is darkening the whole stone, not its hollows");
		ZENITH_ASSERT_LT(fMinTint, 0.92f, "the tint has no depth at all — nothing is darkened");
	}
}

ZENITH_TEST(RockAssets, GenerationIsDeterministicForAFixedSeed)
{
	// Every boot regenerates these assets in place. A generator that drifts would
	// rewrite ~20 MB of engine assets on every run for no reason.
	u_int uSeed = 0u;
	const RockShapeParams xParams = RockTestShape(0, uSeed);

	Zenith_MeshAsset xFirst;
	Zenith_MeshAsset xSecond;
	xFirst.Reserve(4096u, 12288u);
	xSecond.Reserve(4096u, 12288u);
	AppendRockGeometry(xFirst, xParams, uSeed, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), 0.0f, 1.0f);
	AppendRockGeometry(xSecond, xParams, uSeed, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), 0.0f, 1.0f);

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

ZENITH_TEST(RockAssets, ClusterAppendsPlaceStonesWithoutSharingVertices)
{
	// The pebble cluster is N rocks in ONE mesh. Each append must index only its
	// own vertices — a base-offset slip would stitch two stones together with a
	// triangle spanning both.
	u_int uSeed = 0u;
	const RockShapeParams xParams = RockTestShape(3, uSeed);

	Zenith_MeshAsset xMesh;
	xMesh.Reserve(4096u, 12288u);
	AppendRockGeometry(xMesh, xParams, uSeed, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), 0.0f, 1.0f);
	const u_int uFirstVerts = xMesh.GetNumVerts();
	const u_int uFirstIndices = xMesh.GetNumIndices();
	ZENITH_ASSERT_GT(uFirstVerts, 0u, "first cluster stone emitted nothing");

	const Zenith_Maths::Vector3 xOffset(4.0f, 0.0f, -3.0f);
	AppendRockGeometry(xMesh, xParams, uSeed + 137u, xOffset, 1.1f, 0.6f);
	ZENITH_ASSERT_GT(xMesh.GetNumVerts(), uFirstVerts, "second cluster stone emitted nothing");

	for (u_int u = 0; u < uFirstIndices; u++)
	{
		ZENITH_ASSERT_LT(xMesh.m_xIndices.Get(u), uFirstVerts,
			"a first-stone triangle reaches into the second stone's vertices");
	}
	for (u_int u = uFirstIndices; u < xMesh.GetNumIndices(); u++)
	{
		ZENITH_ASSERT_GE(xMesh.m_xIndices.Get(u), uFirstVerts,
			"a second-stone triangle reaches back into the first stone's vertices");
		ZENITH_ASSERT_LT(xMesh.m_xIndices.Get(u), xMesh.GetNumVerts(), "index out of range");
	}

	// ...and the second stone actually landed where it was asked to.
	float fMinX = 1.0e30f;
	for (u_int u = uFirstVerts; u < xMesh.GetNumVerts(); u++)
	{
		fMinX = std::min(fMinX, xMesh.m_xPositions.Get(u).x);
	}
	ZENITH_ASSERT_GT(fMinX, 3.0f, "the offset stone was not translated");
}

#endif // ZENITH_TESTING
