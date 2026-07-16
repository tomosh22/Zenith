#include "Zenith.h"

// ============================================================================
// ZM_Tests_BuildingGen -- S4 unit gate for ZM_BuildingGen (suite ZM_Gen).
//
// Buildings are STATIC models (NO skeleton, NO animation). These author against
// the frozen public BuildingGen/Data seam and assert the roster/recipe/asset-path
// /determinism/static-mesh contract. Pure / headless: no disk, no GPU, no
// ZENITH_TOOLS reach. Runs at boot before the scene loads.
//   1. BuildingGen_RosterTotality    -- every id yields a self-referencing row +
//                                        recipe + a buildable, ZM_ValidateBuilding-
//                                        passing bundle; gym theme-type contract.
//   2. BuildingGen_RecipePurity       -- resolve is pure f(id); distinct ids carry
//                                        distinct seeds; MESH != ALBEDO domain seed.
//   3. BuildingGen_AssetPathScheme    -- golden per-model refs + truncation.
//   4. BuildingGen_BuildDeterminism   -- reflexive byte-identity + hash; two
//                                        distinct ids differ.
//   5. BuildingGen_StaticMeshContract -- zero bones, empty skin buffers, tris > 0,
//                                        outward winding, finite in-range UVs.
//   6. BuildingGen_ShellStructural    -- exact per-roof-kind vert/tri counts, static
//                                        validity, grounded at y=0, roof above walls.
//   7. BuildingGen_MeshSensitivity    -- the MESH seed perturbs the mesh; a non-MESH
//                                        seed does not; distinct ids differ.
//   8. BuildingGen_FacadeStructural   -- every id's facade is 256^2, wall/roof/window/
//                                        door read as >=3 distinct colours in the right
//                                        V bands (window/door below the roof band), and
//                                        rebuilds byte-identically.
//   9. BuildingGen_FacadeDomainIsolation -- the ALBEDO seed perturbs the facade; a
//                                        non-ALBEDO (MESH) seed does not; distinct
//                                        palette/theme facades (incl. same-palette gyms
//                                        via theme tint) differ.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/Gen/ZM_BuildingGen.h"
#include "Zenithmon/Source/Data/ZM_BuildingData.h"
#include "Maths/Zenith_Maths.h"
#include "Collections/Zenith_Vector.h"

#include <cstring>   // strlen
#include <cmath>     // std::isfinite

namespace
{
	// A building id is one of the 8 contiguous gyms.
	bool BuildingIsGym(ZM_BUILDING_ID eId)
	{
		return eId >= ZM_BUILDING_GYM_1 && eId <= ZM_BUILDING_GYM_8;
	}
}

// ############################################################################
// 1. Roster totality -- every building is resolvable + buildable + valid
// ############################################################################

// For EVERY ZM_BUILDING_ID: the roster row self-references (m_eId == index), the
// recipe resolves, and ZM_BuildBuilding produces a bundle that passes the whole
// ZM_ValidateBuilding contract -- a static (zero-bone, empty-skin) box mesh with
// outward winding, non-degenerate bounds, in-range indices, finite UVs, and a
// non-empty facade. Plus the theme-type contract: gyms carry a real ZM_TYPE, every
// non-gym is ZM_TYPE_NONE.
ZENITH_TEST(ZM_Gen, BuildingGen_RosterTotality)
{
	ZENITH_ASSERT_EQ(ZM_GetBuildingCount(), (u_int)ZM_BUILDING_COUNT,
		"building count must equal ZM_BUILDING_COUNT");

	u_int uTested = 0u;
	for (u_int id = 0; id < (u_int)ZM_BUILDING_COUNT; ++id)
	{
		const ZM_BUILDING_ID eId = (ZM_BUILDING_ID)id;

		// Roster-table integrity: the row indexes itself.
		ZENITH_ASSERT_EQ((u_int)ZM_GetBuildingData(eId).m_eId, id,
			"building row %u does not self-reference (m_eId mismatch)", id);

		// Recipe resolves and carries this id.
		const ZM_BuildingRecipe xR = ZM_ResolveBuildingRecipe(eId);
		ZENITH_ASSERT_EQ((u_int)xR.m_eId, id, "recipe %u carries the wrong id", id);

		// Full bundle build + validation.
		ZM_Building xBuilding;
		ZM_BuildBuilding(eId, xBuilding);

		ZENITH_ASSERT_GT(xBuilding.m_xMesh.GetNumVerts(), 0u, "building %u mesh empty", id);
		ZENITH_ASSERT_GT(xBuilding.m_xMesh.GetNumTris(), 0u, "building %u has no triangles", id);
		ZENITH_ASSERT_EQ(xBuilding.m_xMesh.GetNumBones(), 0u,
			"building %u must be static (zero bones)", id);

		const ZM_BuildingValidation xV = ZM_ValidateBuilding(xBuilding);
		ZENITH_ASSERT_TRUE(xV.m_bAllValid, "building %u failed the ZM_ValidateBuilding rollup", id);
		ZENITH_ASSERT_TRUE(xV.m_xMesh.m_bWindingOutward,
			"building %u winding not outward (bad tri %u)", id, xV.m_xMesh.m_uFirstBadTriangle);
		ZENITH_ASSERT_TRUE(xV.m_xMesh.m_bBoundsNonDegen, "building %u has degenerate bounds", id);
		ZENITH_ASSERT_TRUE(xV.m_xMesh.m_bIndicesInRange, "building %u indices out of range", id);
		ZENITH_ASSERT_TRUE(xV.m_xMesh.m_bUVsFinite, "building %u UVs not finite/in [0,1]", id);
		ZENITH_ASSERT_TRUE(xV.m_xMesh.m_bNoSkeleton, "building %u must carry zero bones", id);
		ZENITH_ASSERT_TRUE(xV.m_xMesh.m_bNoSkinBuffers, "building %u must have empty skin buffers", id);
		ZENITH_ASSERT_TRUE(xV.m_bFacadeNonEmpty, "building %u placeholder facade is empty", id);

		// Theme-type contract: gyms carry a real type; every non-gym is NONE.
		const ZM_TYPE eTheme = ZM_GetBuildingData(eId).m_eThemeType;
		if (BuildingIsGym(eId))
		{
			ZENITH_ASSERT_NE((u_int)eTheme, (u_int)ZM_TYPE_NONE,
				"gym %u must carry a real theme type", id);
			ZENITH_ASSERT_NE((u_int)xR.m_eThemeType, (u_int)ZM_TYPE_NONE,
				"gym recipe %u theme type must be real", id);
		}
		else
		{
			ZENITH_ASSERT_EQ((u_int)eTheme, (u_int)ZM_TYPE_NONE,
				"non-gym %u must be ZM_TYPE_NONE", id);
		}

		++uTested;
	}
	ZENITH_ASSERT_GT(uTested, 0u, "no buildings exercised the roster-totality gate");
}

// ############################################################################
// 2. Recipe purity + distinct synthetic seeds
// ############################################################################

// ZM_ResolveBuildingRecipe is a pure function of the id (resolving twice yields a
// field-identical recipe), distinct ids carry distinct synthetic seeds (no
// name-hash collision), and each id's MESH domain seed differs from its ALBEDO
// domain seed (the domains never alias).
ZENITH_TEST(ZM_Gen, BuildingGen_RecipePurity)
{
	Zenith_Vector<u_int> xSeeds;
	for (u_int id = 0; id < (u_int)ZM_BUILDING_COUNT; ++id)
	{
		const ZM_BUILDING_ID eId = (ZM_BUILDING_ID)id;
		const ZM_BuildingRecipe xA = ZM_ResolveBuildingRecipe(eId);
		const ZM_BuildingRecipe xB = ZM_ResolveBuildingRecipe(eId);

		// Purity: id, synthetic seed, and every derived domain seed are identical.
		ZENITH_ASSERT_EQ((u_int)xA.m_eId, (u_int)xB.m_eId, "recipe id not pure for %u", id);
		ZENITH_ASSERT_EQ(xA.m_uSyntheticSeed, xB.m_uSyntheticSeed,
			"recipe synthetic seed not pure for %u", id);
		bool bDomainsEqual = true;
		for (u_int d = 0; d < (u_int)ZM_GEN_DOMAIN_COUNT; ++d)
		{
			if (xA.m_aulDomainSeed[d] != xB.m_aulDomainSeed[d]) { bDomainsEqual = false; }
		}
		ZENITH_ASSERT_TRUE(bDomainsEqual, "recipe domain seeds not pure for %u", id);

		// The MESH and ALBEDO streams must never share a seed.
		ZENITH_ASSERT_NE(xA.m_aulDomainSeed[ZM_GEN_DOMAIN_MESH],
			xA.m_aulDomainSeed[ZM_GEN_DOMAIN_ALBEDO],
			"building %u MESH/ALBEDO domain seeds must differ", id);

		xSeeds.PushBack(xA.m_uSyntheticSeed);
	}

	// Pairwise-distinct synthetic seeds across the whole roster.
	for (u_int i = 0; i < xSeeds.GetSize(); ++i)
	{
		for (u_int j = i + 1u; j < xSeeds.GetSize(); ++j)
		{
			ZENITH_ASSERT_NE(xSeeds.Get(i), xSeeds.Get(j),
				"buildings %u/%u share a synthetic seed (name-hash collision)", i, j);
		}
	}
}

// ############################################################################
// 3. Asset-path scheme (golden per-model refs + truncation)
// ############################################################################

// Golden-locks the four per-model refs for a known id (CareCenter) and the too-
// small-buffer -> false (truncation) contract. Pure; compiled in ALL configs.
ZENITH_TEST(ZM_Gen, BuildingGen_AssetPathScheme)
{
	char acRef[256];

	ZENITH_ASSERT_TRUE(
		ZM_BuildingAssetPath(ZM_BUILDING_CARE_CENTER, ZM_BUILDING_ASSET_MESH, acRef, sizeof(acRef)),
		"mesh ref must fit");
	ZENITH_ASSERT_STREQ(acRef, "game:Buildings/CareCenter/CareCenter.zmesh", "mesh ref scheme drifted");

	ZENITH_ASSERT_TRUE(
		ZM_BuildingAssetPath(ZM_BUILDING_CARE_CENTER, ZM_BUILDING_ASSET_FACADE, acRef, sizeof(acRef)),
		"facade ref must fit");
	ZENITH_ASSERT_STREQ(acRef, "game:Buildings/CareCenter/CareCenter_facade.ztxtr",
		"facade ref scheme drifted");

	ZENITH_ASSERT_TRUE(
		ZM_BuildingAssetPath(ZM_BUILDING_CARE_CENTER, ZM_BUILDING_ASSET_MATERIAL, acRef, sizeof(acRef)),
		"material ref must fit");
	ZENITH_ASSERT_STREQ(acRef, "game:Buildings/CareCenter/CareCenter.zmtrl", "material ref scheme drifted");

	ZENITH_ASSERT_TRUE(
		ZM_BuildingAssetPath(ZM_BUILDING_CARE_CENTER, ZM_BUILDING_ASSET_MODEL, acRef, sizeof(acRef)),
		"model ref must fit");
	ZENITH_ASSERT_STREQ(acRef, "game:Buildings/CareCenter/CareCenter.zmodel", "model ref scheme drifted");

	// Truncation: a cap far too small returns false + stays NUL-terminated.
	char acTiny[8];
	const bool bFits = ZM_BuildingAssetPath(ZM_BUILDING_CARE_CENTER, ZM_BUILDING_ASSET_MESH, acTiny, sizeof(acTiny));
	ZENITH_ASSERT_FALSE(bFits, "an 8-byte cap cannot hold the mesh ref -- must report truncation");
	ZENITH_ASSERT_LT((u_int)strlen(acTiny), (u_int)sizeof(acTiny),
		"a truncated ref must stay NUL-terminated within the cap");
}

// ############################################################################
// 4. Build determinism (reflexive) -- byte-identity + hash machinery
// ############################################################################

// Reflexive lock on the SC1 determinism helpers: building the SAME id twice yields
// a byte-identical mesh (ZM_BuildingMeshEqual), an equal build (ZM_BuildingBuild-
// Equal), and an equal content hash (ZM_BuildingContentHash). Plus a non-
// degeneracy guard: two DISTINCT ids whose recipes genuinely differ (CareCenter
// 10x8x1-storey WARM vs GymGrass 12x14x2-storey EARTH -> different box AND facade)
// must NOT collapse to one bundle/hash.
ZENITH_TEST(ZM_Gen, BuildingGen_BuildDeterminism)
{
	ZM_Building xA;
	ZM_Building xB;
	ZM_BuildBuilding(ZM_BUILDING_CARE_CENTER, xA);
	ZM_BuildBuilding(ZM_BUILDING_CARE_CENTER, xB);

	ZENITH_ASSERT_TRUE(ZM_BuildingBuildEqual(xA, xB),
		"rebuilding CareCenter must yield an equal bundle (ZM_BuildingBuildEqual)");
	ZENITH_ASSERT_TRUE(ZM_BuildingMeshEqual(xA.m_xMesh, xB.m_xMesh),
		"rebuilding CareCenter must yield a byte-identical mesh (ZM_BuildingMeshEqual)");
	ZENITH_ASSERT_EQ(ZM_BuildingContentHash(xA), ZM_BuildingContentHash(xB),
		"rebuilding CareCenter must yield an equal content hash (reflexive)");

	// Non-degeneracy: a distinct id with a genuinely different recipe must not be
	// build-equal, and must differ in its hash OR its mesh (a trivially-constant
	// hash would otherwise pass everything above).
	ZM_Building xOther;
	ZM_BuildBuilding(ZM_BUILDING_GYM_1, xOther);
	ZENITH_ASSERT_FALSE(ZM_BuildingBuildEqual(xA, xOther),
		"CareCenter and GymGrass differ in dims + palette -- must not be build-equal");
	const bool bHashDiffers = (ZM_BuildingContentHash(xA) != ZM_BuildingContentHash(xOther));
	const bool bMeshDiffers = !ZM_BuildingMeshEqual(xA.m_xMesh, xOther.m_xMesh);
	ZENITH_ASSERT_TRUE(bHashDiffers || bMeshDiffers,
		"CareCenter and GymGrass must differ in content hash or mesh");
}

// ############################################################################
// 5. Static-mesh contract
// ############################################################################

// A built building mesh is STATIC: exactly zero bones, byte-empty skin buffers,
// at least one triangle, ZM_ValidateGenMeshStatic-valid (outward winding), and
// every UV finite within [0,1]. The complete per-vertex buffers (normal/UV/
// tangent) confirm the finalise pass ran.
ZENITH_TEST(ZM_Gen, BuildingGen_StaticMeshContract)
{
	ZM_Building xBuilding;
	ZM_BuildBuilding(ZM_BUILDING_PLAYER_HOME, xBuilding);
	const ZM_GenMesh& xMesh = xBuilding.m_xMesh;

	ZENITH_ASSERT_EQ(xMesh.GetNumBones(), 0u, "static building mesh must have zero bones");
	ZENITH_ASSERT_EQ(xMesh.m_xBoneIndices.GetSize(), 0u,
		"static building mesh must have an empty bone-index buffer");
	ZENITH_ASSERT_EQ(xMesh.m_xBoneWeights.GetSize(), 0u,
		"static building mesh must have an empty bone-weight buffer");
	ZENITH_ASSERT_GT(xMesh.GetNumTris(), 0u, "static building mesh must have triangles");

	const ZM_GenStaticMeshValidation xV = ZM_ValidateGenMeshStatic(xMesh);
	ZENITH_ASSERT_TRUE(xV.m_bAllValid, "static mesh validation rollup failed");
	ZENITH_ASSERT_TRUE(xV.m_bWindingOutward,
		"static mesh winding not outward (bad tri %u)", xV.m_uFirstBadTriangle);
	ZENITH_ASSERT_TRUE(xV.m_bBoundsNonDegen, "static mesh bounds degenerate");
	ZENITH_ASSERT_TRUE(xV.m_bIndicesInRange, "static mesh indices out of range");
	ZENITH_ASSERT_TRUE(xV.m_bUVsFinite, "static mesh UVs not finite/in [0,1]");
	ZENITH_ASSERT_TRUE(xV.m_bNoSkeleton, "static mesh must report zero skeleton");
	ZENITH_ASSERT_TRUE(xV.m_bNoSkinBuffers, "static mesh must report empty skin buffers");

	// Per-vertex buffers complete (the finalise pass ran) + UVs finite in [0,1].
	const u_int uNumVerts = xMesh.GetNumVerts();
	ZENITH_ASSERT_EQ(xMesh.m_xNormals.GetSize(), uNumVerts, "one normal per vertex");
	ZENITH_ASSERT_EQ(xMesh.m_xUVs.GetSize(), uNumVerts, "one UV per vertex");
	ZENITH_ASSERT_EQ(xMesh.m_xTangents.GetSize(), uNumVerts, "one tangent per vertex (finalised)");
	for (u_int v = 0; v < uNumVerts; ++v)
	{
		const Zenith_Maths::Vector2& xUV = xMesh.m_xUVs.Get(v);
		ZENITH_ASSERT_TRUE(std::isfinite(xUV.x) && std::isfinite(xUV.y),
			"vertex %u UV is non-finite", v);
		ZENITH_ASSERT_TRUE(xUV.x >= -1.0e-4f && xUV.x <= 1.0f + 1.0e-4f,
			"vertex %u U outside [0,1]", v);
		ZENITH_ASSERT_TRUE(xUV.y >= -1.0e-4f && xUV.y <= 1.0f + 1.0e-4f,
			"vertex %u V outside [0,1]", v);
	}
}

// ############################################################################
// 6. Parametric-shell structure -- per-roof-kind counts + geometry contract
// ############################################################################

// For EVERY building id: the SC2 shell (body box + roof-by-kind) emits the exact
// per-kind vertex/triangle counts (AppendBox 24/12 body + GABLE 14/6, HIP 12/4,
// FLAT 24/12 roof), passes ZM_ValidateGenMeshStatic (outward winding + finite
// in-range UVs), stays grounded at y=0 after the MESH jitter, and rises above the
// nominal wall height (the roof genuinely sits on top of the walls).
ZENITH_TEST(ZM_Gen, BuildingGen_ShellStructural)
{
	u_int uTested = 0u;
	for (u_int id = 0; id < (u_int)ZM_BUILDING_COUNT; ++id)
	{
		const ZM_BUILDING_ID eId = (ZM_BUILDING_ID)id;
		const ZM_BuildingRecipe xR = ZM_ResolveBuildingRecipe(eId);

		ZM_GenMesh xMesh;
		ZM_BuildBuildingMesh(xR, xMesh);

		// Exact per-roof-kind vertex/triangle counts.
		const ZM_ROOF_KIND eRoof = ZM_GetBuildingData(eId).m_eRoof;
		u_int uExpectVerts = 0u, uExpectTris = 0u;
		switch (eRoof)
		{
		case ZM_ROOF_GABLE: uExpectVerts = 38u; uExpectTris = 18u; break;
		case ZM_ROOF_HIP:   uExpectVerts = 36u; uExpectTris = 16u; break;
		case ZM_ROOF_FLAT:
		default:            uExpectVerts = 48u; uExpectTris = 24u; break;
		}
		ZENITH_ASSERT_EQ(xMesh.GetNumVerts(), uExpectVerts,
			"building %u (roof %u) vertex count drifted", id, (u_int)eRoof);
		ZENITH_ASSERT_EQ(xMesh.GetNumTris(), uExpectTris,
			"building %u (roof %u) triangle count drifted", id, (u_int)eRoof);

		// Static structural contract: outward winding, finite in-range UVs, valid rollup.
		const ZM_GenStaticMeshValidation xV = ZM_ValidateGenMeshStatic(xMesh);
		ZENITH_ASSERT_TRUE(xV.m_bAllValid, "building %u shell failed static validation", id);
		ZENITH_ASSERT_TRUE(xV.m_bWindingOutward,
			"building %u shell winding not outward (bad tri %u)", id, xV.m_uFirstBadTriangle);
		ZENITH_ASSERT_TRUE(xV.m_bUVsFinite, "building %u shell UVs not finite/in [0,1]", id);

		// Grounded at y=0 (the body-box base is literal 0.0f, even after jitter).
		ZENITH_ASSERT_TRUE(std::fabs(ZM_GenMeshBoundsMin(xMesh).y) <= 1.0e-3f,
			"building %u shell not grounded at y=0", id);

		// The roof apex rises above the nominal wall height.
		const float fNominalWall = xR.m_fStoreyHeight * static_cast<float>(xR.m_uStoreys);
		ZENITH_ASSERT_TRUE(ZM_GenMeshBoundsMax(xMesh).y > fNominalWall,
			"building %u roof apex must exceed nominal wall height", id);

		++uTested;
	}
	ZENITH_ASSERT_GT(uTested, 0u, "no buildings exercised the shell-structural gate");
}

// ############################################################################
// 7. Mesh sensitivity -- the MESH seed (and only it) drives the geometry
// ############################################################################

// Mutating the MESH domain seed perturbs the shell (the jitter reads it), mutating a
// NON-mesh (ALBEDO) domain seed leaves the mesh byte-identical (the builder never
// draws it), and two distinct ids yield distinct meshes.
ZENITH_TEST(ZM_Gen, BuildingGen_MeshSensitivity)
{
	const ZM_BuildingRecipe xBase = ZM_ResolveBuildingRecipe(ZM_BUILDING_PLAYER_HOME);
	ZM_GenMesh x0; ZM_BuildBuildingMesh(xBase, x0);

	ZM_BuildingRecipe xMeshMut = xBase;
	xMeshMut.m_aulDomainSeed[ZM_GEN_DOMAIN_MESH] ^= 0x9E3779B97F4A7C15ull;
	ZM_GenMesh xM; ZM_BuildBuildingMesh(xMeshMut, xM);
	ZENITH_ASSERT_FALSE(ZM_BuildingMeshEqual(x0, xM), "mutating the MESH seed must perturb the mesh");

	ZM_BuildingRecipe xAlbMut = xBase;
	xAlbMut.m_aulDomainSeed[ZM_GEN_DOMAIN_ALBEDO] ^= 0x9E3779B97F4A7C15ull;
	ZM_GenMesh xA; ZM_BuildBuildingMesh(xAlbMut, xA);
	ZENITH_ASSERT_TRUE(ZM_BuildingMeshEqual(x0, xA), "mutating a non-MESH seed must NOT change the mesh");

	ZM_GenMesh xOther; ZM_BuildBuildingMesh(ZM_ResolveBuildingRecipe(ZM_BUILDING_LAB), xOther);
	ZENITH_ASSERT_FALSE(ZM_BuildingMeshEqual(x0, xOther), "distinct ids must yield distinct meshes");
}

// ############################################################################
// 8. Facade structure -- wall/roof/window/door bands + determinism
// ############################################################################

// For EVERY building id: the SC3 facade is the expected square resolution, the
// wall base / roof band / window grid / door read as at least three distinct
// colours sampled in the right V bands (windows + door strictly below the roof
// band, so a flipped-V regression collapses them), and rebuilding the same recipe
// yields a byte-identical image.
ZENITH_TEST(ZM_Gen, BuildingGen_FacadeStructural)
{
	auto Differ = [](const Zenith_Maths::Vector4& a, const Zenith_Maths::Vector4& b) {
		return std::fabs(a.x - b.x) > 2.0f / 255.0f
			|| std::fabs(a.y - b.y) > 2.0f / 255.0f
			|| std::fabs(a.z - b.z) > 2.0f / 255.0f;
	};
	u_int uTested = 0u;
	for (u_int id = 0; id < (u_int)ZM_BUILDING_COUNT; ++id)
	{
		const ZM_BUILDING_ID eId = (ZM_BUILDING_ID)id;
		const ZM_BuildingRecipe xR = ZM_ResolveBuildingRecipe(eId);
		const ZM_GenImage xF = ZM_BuildBuildingFacade(xR);
		ZENITH_ASSERT_FALSE(xF.IsEmpty(), "building %u facade empty", id);
		ZENITH_ASSERT_EQ(xF.GetWidth(),  uZM_BUILDING_FACADE_RESOLUTION, "facade width drifted (%u)", id);
		ZENITH_ASSERT_EQ(xF.GetHeight(), uZM_BUILDING_FACADE_RESOLUTION, "facade height drifted (%u)", id);

		const u_int uCols = xR.m_uWindowCols > 0u ? xR.m_uWindowCols : 1u;
		const u_int uRows = xR.m_uWindowRows > 0u ? xR.m_uWindowRows : 1u;
		const float fCellW = (fZM_FACADE_GRID_U1 - fZM_FACADE_GRID_U0) / (float)uCols;
		const float fCellH = (fZM_FACADE_GRID_V1 - fZM_FACADE_GRID_V0) / (float)uRows;
		const float fCU = fZM_FACADE_GRID_U0 + 0.5f * fCellW;
		const float fCV = fZM_FACADE_GRID_V0 + 0.5f * fCellH;
		const u_int uYwin = (u_int)(fCV * 255.0f + 0.5f);
		const u_int uXwin = (u_int)(fCU * 255.0f + 0.5f);

		const Zenith_Maths::Vector4 xWall = xF.Get(150u, 8u);
		const Zenith_Maths::Vector4 xRoof = xF.Get(230u, 128u);
		const Zenith_Maths::Vector4 xWin  = xF.Get(uYwin, uXwin);
		const Zenith_Maths::Vector4 xDoor = xF.Get(28u, 128u);

		ZENITH_ASSERT_TRUE(fCV < 0.72f, "building %u window row V not in wall band", id);
		ZENITH_ASSERT_TRUE(Differ(xWin,  xRoof), "building %u window colour == roof (flipped V?)", id);
		ZENITH_ASSERT_TRUE(Differ(xDoor, xRoof), "building %u door colour == roof (flipped V?)", id);
		ZENITH_ASSERT_TRUE(Differ(xWin,  xWall), "building %u window not painted over wall", id);

		const Zenith_Maths::Vector4 axS[4] = { xWall, xRoof, xWin, xDoor };
		u_int uDistinct = 0u;
		for (u_int i = 0; i < 4u; ++i)
		{
			bool bNew = true;
			for (u_int j = 0; j < i; ++j) { if (!Differ(axS[i], axS[j])) { bNew = false; } }
			if (bNew) { ++uDistinct; }
		}
		ZENITH_ASSERT_TRUE(uDistinct >= 3u, "building %u facade has < 3 distinct colours", id);

		const ZM_GenImage xF2 = ZM_BuildBuildingFacade(xR);
		ZENITH_ASSERT_TRUE(xF.Equals(xF2), "building %u facade not deterministic", id);
		++uTested;
	}
	ZENITH_ASSERT_GT(uTested, 0u, "no buildings exercised the facade-structural gate");
}

// ############################################################################
// 9. Facade domain isolation -- ALBEDO (and only it) drives the facade
// ############################################################################

// Mutating the ALBEDO domain seed perturbs the facade (the wall/roof jitter reads
// it), mutating a NON-albedo (MESH) domain seed leaves the facade byte-identical
// (the builder never constructs a MESH RNG), and distinct palette/theme facades --
// including two same-palette gyms separated only by their theme tint -- differ.
ZENITH_TEST(ZM_Gen, BuildingGen_FacadeDomainIsolation)
{
	const ZM_BuildingRecipe xBase = ZM_ResolveBuildingRecipe(ZM_BUILDING_CARE_CENTER);
	const ZM_GenImage x0 = ZM_BuildBuildingFacade(xBase);

	ZM_BuildingRecipe xAlb = xBase;
	xAlb.m_aulDomainSeed[ZM_GEN_DOMAIN_ALBEDO] ^= 0x9E3779B97F4A7C15ull;
	const ZM_GenImage xA = ZM_BuildBuildingFacade(xAlb);
	ZENITH_ASSERT_FALSE(x0.Equals(xA), "mutating the ALBEDO seed must change the facade");
	ZENITH_ASSERT_NE(x0.ContentHash(), xA.ContentHash(), "ALBEDO mutation must change the facade hash");

	ZM_BuildingRecipe xMesh = xBase;
	xMesh.m_aulDomainSeed[ZM_GEN_DOMAIN_MESH] ^= 0x9E3779B97F4A7C15ull;
	const ZM_GenImage xM = ZM_BuildBuildingFacade(xMesh);
	ZENITH_ASSERT_TRUE(x0.Equals(xM), "mutating a non-ALBEDO seed must NOT change the facade");
	ZENITH_ASSERT_EQ(x0.ContentHash(), xM.ContentHash(), "non-ALBEDO mutation must not change the facade hash");

	const ZM_GenImage xEarth = ZM_BuildBuildingFacade(ZM_ResolveBuildingRecipe(ZM_BUILDING_GYM_1));
	ZENITH_ASSERT_FALSE(x0.Equals(xEarth), "distinct palette/theme facades must differ");

	const ZM_GenImage xFire = ZM_BuildBuildingFacade(ZM_ResolveBuildingRecipe(ZM_BUILDING_GYM_2));
	const ZM_GenImage xElec = ZM_BuildBuildingFacade(ZM_ResolveBuildingRecipe(ZM_BUILDING_GYM_4));
	ZENITH_ASSERT_FALSE(xFire.Equals(xElec), "same-palette gyms must diverge via theme tint");
}
