#include "Zenith.h"

// ============================================================================
// ZM_Tests_BuildingGen -- S4 unit gate for ZM_BuildingGen (suite ZM_Gen).
//
// Buildings are STATIC models (NO skeleton, NO animation) split into FOUR
// SURFACE CLASSES (wall / roof / trim / glass), each carrying its own mesh and
// its own four-map PBR set. These author against the frozen public
// BuildingGen/Data seam and assert the roster / recipe / asset-path /
// determinism / static-mesh / surface contract. Pure and headless: no disk, no
// GPU, no ZENITH_TOOLS reach. Runs at boot before the scene loads.
//
//    1. BuildingGen_RosterTotality       -- every id yields a self-referencing row +
//                                           recipe + a buildable, ZM_ValidateBuilding-
//                                           passing bundle; gym theme-type contract.
//    2. BuildingGen_RecipePurity          -- resolve is pure f(id); distinct ids carry
//                                           distinct seeds; MESH != ALBEDO domain seed.
//    3. BuildingGen_SurfaceTableTotality  -- name/resolution/tile/response are total
//                                           and DISTINCT where the design says so.
//    4. BuildingGen_AssetKindAlgebra      -- compose/decompose round-trips over every
//                                           (surface, slot); the model kind is outside
//                                           the per-surface range; kind count is exact.
//    5. BuildingGen_AssetPathScheme       -- golden per-surface refs + truncation.
//    6. BuildingGen_BuildDeterminism      -- reflexive byte-identity + hash over all
//                                           four surfaces; two distinct ids differ.
//    7. BuildingGen_StaticMeshContract    -- per surface: zero bones, empty skin
//                                           buffers, tris > 0, outward winding.
//    8. BuildingGen_ShellMetricsDerivation-- metrics are pure f(recipe); the roof sits
//                                           on the walls; window helpers stay inside
//                                           the facade.
//    9. BuildingGen_SiteFixedSuppressesJitter -- a site-fixed row's footprint is EXACTLY
//                                           the roster's, a free row's is not, and the
//                                           RNG stream position is unchanged either way.
//   10. BuildingGen_SurfacesRegister      -- glass sits inside its frame, the trim
//                                           reaches the wall, and no surface is empty.
//   11. BuildingGen_WorldUVsAreUniformDensity -- the whole point of the tiling
//                                           projection: texels per metre is the same
//                                           on a 16 m wall and a 0.4 m quoin.
//   12. BuildingGen_MeshSensitivity       -- the MESH seed perturbs a free-standing
//                                           mesh; a non-MESH seed does not.
//   13. BuildingGen_TextureSetStructural  -- four maps per surface at the surface's
//                                           resolution; the normal is NOT flat; the
//                                           RM map carries the surface's response.
//   14. BuildingGen_TextureDomainIsolation-- the ALBEDO seed perturbs the maps; the
//                                           MESH seed does not; distinct palettes and
//                                           same-palette gyms differ.
//   15. BuildingGen_DawnmereBuildingsFitTheirBlockouts -- ★ THE CONTRACT THAT MATTERS:
//                                           the PlayerHome and Lab models fit inside
//                                           the Dawnmere shell blockouts that carry
//                                           their colliders.
//   16. StaticBoxEdgesAreChamfered      -- every ZM_StaticMesh box is bevelled, with
//                                           smoothed strip normals and IDENTICAL bounds.
//   17. BuildingGen_HeightIsBakedAndParallaxEnabled -- the height field is baked to its
//                                           own slot and POM is really on (map AND scale).
//   18. BuildingGen_WindowsAreRealOpenings -- a sight line through a window meets no
//                                           masonry in the outer reveal; solid wall does.
//   19. BuildingGen_DoorIsARecessedLeaf  -- leaf behind the wall plane, threshold, handle.
//   20. BuildingGen_RoofIsCoursed        -- overlapping slate courses with a proud lip,
//                                           a ridge run, barge boards and the eave fascia.
//   21. BuildingGen_WeatheringIsAnchoredToWorldHeight -- splash-back, eave runoff and
//                                           sill drips land on the rows they claim, in
//                                           METRES, and reach the albedo.
//   22. SynthEdgeWearLandsOnProudArrises -- the wear mask finds edges, only the proud
//                                           side of them, and polishes what it finds.
//   23. BuildingGen_MacroTintBreaksTheRepeat -- the tile is not two identical halves;
//                                           both grime colours exist; the per-face cast
//                                           is a vertex colour.
//   24. BuildingGen_OnlyTheHomeGlowsFromInside -- warm emissive glass, on exactly one row.
//   25. BuildingGen_CourseGaugeSurvivesTheTile -- the masonry lattice is derived from the
//                                           tile, so a 3.5 m storey does not stretch it.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/Gen/ZM_BuildingGen.h"
#include "Zenithmon/Source/Data/ZM_BuildingData.h"
#include "Zenithmon/Source/World/ZM_DawnmerePlacement.h"
#include "Zenithmon/Source/World/ZM_PlayerHomePlacement.h"
#include "Zenithmon/Source/World/ZM_ProfLabPlacement.h"
#include "Zenithmon/Source/Gen/ZM_InteriorGen.h"
#include "Zenithmon/Source/World/ZM_InteriorDressing.h"
#include "Zenithmon/Source/Gen/ZM_HumanAppearance.h"      // ZM_HumanPaletteSeparation + the shared floor
#include "Zenithmon/Source/Gen/ZM_PropGen.h"               // the prop bundle + its PBR response
#include "Zenithmon/Source/Data/ZM_PropData.h"
#include "Maths/Zenith_Maths.h"
#include "Collections/Zenith_Vector.h"

#include <cstring>   // strlen, strcmp
#include <cmath>     // std::isfinite, std::fabs

namespace
{
	// A building id is one of the 8 contiguous gyms.
	bool BuildingIsGym(ZM_BUILDING_ID eId)
	{
		return eId >= ZM_BUILDING_GYM_1 && eId <= ZM_BUILDING_GYM_8;
	}

	// "These two floats are the same authored number." Generous by float
	// standards, tiny against every metre quantity a building uses.
	constexpr float fBG_EXACT = 1.0e-4f;

	// A representative id per roof kind, so the roof-dependent clauses below name
	// what they are exercising rather than trusting index arithmetic.
	constexpr ZM_BUILDING_ID eBG_GABLE = ZM_BUILDING_HOUSE_COTTAGE_WARM;
	constexpr ZM_BUILDING_ID eBG_HIP   = ZM_BUILDING_HOUSE_TOWNHOUSE_WARM;
	constexpr ZM_BUILDING_ID eBG_FLAT  = ZM_BUILDING_HOUSE_SHOP_WARM;
}

// ############################################################################
// 1. Roster totality -- every building is resolvable + buildable + valid
// ############################################################################

// For EVERY ZM_BUILDING_ID: the roster row self-references (m_eId == index), the
// recipe resolves, and ZM_BuildBuilding produces a bundle that passes the whole
// ZM_ValidateBuilding contract on ALL FOUR surfaces -- static (zero-bone,
// empty-skin) geometry with outward winding, non-degenerate bounds, in-range
// indices, finite bounded UVs, and a complete four-map texture set. Plus the
// theme-type contract: gyms carry a real ZM_TYPE, every non-gym is ZM_TYPE_NONE.
ZENITH_TEST(ZM_Gen, BuildingGen_RosterTotality)
{
	ZENITH_ASSERT_EQ(ZM_GetBuildingCount(), (u_int)ZM_BUILDING_COUNT,
		"the roster accessor and the enum disagree on how many buildings exist");

	for (u_int u = 0u; u < (u_int)ZM_BUILDING_COUNT; ++u)
	{
		const ZM_BUILDING_ID eId = (ZM_BUILDING_ID)u;
		const ZM_BuildingData& xData = ZM_GetBuildingData(eId);

		ZENITH_ASSERT_EQ((u_int)xData.m_eId, u,
			"roster row %u does not self-reference (m_eId=%u)", u, (u_int)xData.m_eId);
		ZENITH_ASSERT_NOT_NULL(xData.m_szName, "roster row %u has no name", u);
		ZENITH_ASSERT_GT((u_int)strlen(xData.m_szName), 0u,
			"roster row %u has an empty name", u);
		ZENITH_ASSERT_GT(xData.m_fWidth, 0.0f, "%s has no width", xData.m_szName);
		ZENITH_ASSERT_GT(xData.m_fDepth, 0.0f, "%s has no depth", xData.m_szName);
		ZENITH_ASSERT_GT(xData.m_fStoreyHeight, 0.0f,
			"%s has no storey height -- the shell would be a flat sheet", xData.m_szName);
		ZENITH_ASSERT_GT(xData.m_fRoofPitch, 0.0f,
			"%s has no roof pitch", xData.m_szName);
		ZENITH_ASSERT_GT(xData.m_uStoreys, 0u, "%s has no storeys", xData.m_szName);

		// The theme contract.
		if (BuildingIsGym(eId))
		{
			ZENITH_ASSERT_NE((u_int)xData.m_eThemeType, (u_int)ZM_TYPE_NONE,
				"gym %s carries no element type", xData.m_szName);
		}
		else
		{
			ZENITH_ASSERT_EQ((u_int)xData.m_eThemeType, (u_int)ZM_TYPE_NONE,
				"non-gym %s carries an element type", xData.m_szName);
		}

		ZM_Building xBuilding;
		ZM_BuildBuilding(eId, xBuilding);
		ZENITH_ASSERT_EQ((u_int)xBuilding.m_eId, u,
			"the built bundle for %s does not carry its own id", xData.m_szName);

		const ZM_BuildingValidation xV = ZM_ValidateBuilding(xBuilding);
		for (u_int s = 0u; s < (u_int)ZM_BUILDING_SURFACE_COUNT; ++s)
		{
			const char* szS = ZM_BuildingSurfaceName((ZM_BUILDING_SURFACE)s);
			ZENITH_ASSERT_TRUE(xV.m_axSurface[s].m_bWindingOutward,
				"%s/%s has an inward-wound triangle (first bad %u)",
				xData.m_szName, szS, xV.m_axSurface[s].m_uFirstBadTriangle);
			ZENITH_ASSERT_TRUE(xV.m_axSurface[s].m_bBoundsNonDegen,
				"%s/%s is flat on at least one axis", xData.m_szName, szS);
			ZENITH_ASSERT_TRUE(xV.m_axSurface[s].m_bIndicesInRange,
				"%s/%s has an out-of-range index or a partial triangle",
				xData.m_szName, szS);
			ZENITH_ASSERT_TRUE(xV.m_axSurface[s].m_bUVsFiniteAndBounded,
				"%s/%s has a non-finite or runaway UV (max |uv| = %.3f)",
				xData.m_szName, szS, xV.m_axSurface[s].m_fMaxAbsUV);
			ZENITH_ASSERT_TRUE(xV.m_axSurface[s].m_bNoSkeleton,
				"%s/%s carries bones -- buildings are static", xData.m_szName, szS);
			ZENITH_ASSERT_TRUE(xV.m_axSurface[s].m_bNoSkinBuffers,
				"%s/%s carries skin buffers -- buildings are static", xData.m_szName, szS);
			ZENITH_ASSERT_TRUE(xV.m_abTexturesNonEmpty[s],
				"%s/%s is missing one of its four PBR maps", xData.m_szName, szS);
		}
		ZENITH_ASSERT_TRUE(xV.m_bAllValid,
			"%s failed the building validation contract", xData.m_szName);
	}
}

// ############################################################################
// 2. Recipe purity -- resolve is a pure function of the id
// ############################################################################
ZENITH_TEST(ZM_Gen, BuildingGen_RecipePurity)
{
	for (u_int u = 0u; u < (u_int)ZM_BUILDING_COUNT; ++u)
	{
		const ZM_BUILDING_ID eId = (ZM_BUILDING_ID)u;
		const ZM_BuildingRecipe xA = ZM_ResolveBuildingRecipe(eId);
		const ZM_BuildingRecipe xB = ZM_ResolveBuildingRecipe(eId);

		ZENITH_ASSERT_EQ(xA.m_uSyntheticSeed, xB.m_uSyntheticSeed,
			"resolving %s twice produced two different family seeds",
			ZM_GetBuildingName(eId));
		ZENITH_ASSERT_EQ_FLOAT(xA.m_fWidth, xB.m_fWidth, 0.0f,
			"resolving %s twice produced two different widths", ZM_GetBuildingName(eId));
		ZENITH_ASSERT_EQ(xA.m_bSiteFixed ? 1u : 0u, xB.m_bSiteFixed ? 1u : 0u,
			"resolving %s twice disagreed on site-fixedness", ZM_GetBuildingName(eId));

		// The recipe must carry the roster's values, not a re-derivation of them.
		const ZM_BuildingData& xData = ZM_GetBuildingData(eId);
		ZENITH_ASSERT_EQ_FLOAT(xA.m_fStoreyHeight, xData.m_fStoreyHeight, 0.0f,
			"%s's recipe dropped the roster storey height", xData.m_szName);
		ZENITH_ASSERT_EQ_FLOAT(xA.m_fRoofPitch, xData.m_fRoofPitch, 0.0f,
			"%s's recipe dropped the roster roof pitch", xData.m_szName);
		ZENITH_ASSERT_EQ(xA.m_bSiteFixed ? 1u : 0u, xData.m_bSiteFixed ? 1u : 0u,
			"%s's recipe dropped the roster site-fixed flag", xData.m_szName);

		// Domains must not share a seed, or a texture change would move a mesh.
		ZENITH_ASSERT_NE(xA.m_aulDomainSeed[ZM_GEN_DOMAIN_MESH],
			xA.m_aulDomainSeed[ZM_GEN_DOMAIN_ALBEDO],
			"%s's MESH and ALBEDO domains share a seed", xData.m_szName);
	}

	// Distinct ids carry distinct family seeds (the stems are distinct, and the
	// seed is the stem hash).
	for (u_int a = 0u; a < (u_int)ZM_BUILDING_COUNT; ++a)
	{
		for (u_int b = a + 1u; b < (u_int)ZM_BUILDING_COUNT; ++b)
		{
			const u_int uA = ZM_ResolveBuildingRecipe((ZM_BUILDING_ID)a).m_uSyntheticSeed;
			const u_int uB = ZM_ResolveBuildingRecipe((ZM_BUILDING_ID)b).m_uSyntheticSeed;
			ZENITH_ASSERT_NE(uA, uB, "%s and %s hash to the same family seed",
				ZM_GetBuildingName((ZM_BUILDING_ID)a), ZM_GetBuildingName((ZM_BUILDING_ID)b));
		}
	}
}

// ############################################################################
// 3. Surface tables are total AND actually differentiated
// ############################################################################

// Totality alone is a weak claim -- four surfaces that all answer "256, 2 m,
// roughness 0.8" would pass it and would defeat the entire reason the split
// exists. So this asserts DIFFERENCE where the design commits to one.
ZENITH_TEST(ZM_Gen, BuildingGen_SurfaceTableTotality)
{
	for (u_int s = 0u; s < (u_int)ZM_BUILDING_SURFACE_COUNT; ++s)
	{
		const ZM_BUILDING_SURFACE eS = (ZM_BUILDING_SURFACE)s;
		ZENITH_ASSERT_NOT_NULL(ZM_BuildingSurfaceName(eS), "surface %u has no name", s);
		ZENITH_ASSERT_GT((u_int)strlen(ZM_BuildingSurfaceName(eS)), 0u,
			"surface %u has an empty name", s);
		ZENITH_ASSERT_GT(ZM_BuildingSurfaceResolution(eS), 0u,
			"surface %s has a zero map resolution", ZM_BuildingSurfaceName(eS));
		ZENITH_ASSERT_GT(ZM_BuildingSurfaceTileMetres(eS), 0.0f,
			"surface %s has a zero tile size -- every UV would be infinite",
			ZM_BuildingSurfaceName(eS));

		const ZM_BuildingSurfaceResponse xR = ZM_BuildingSurfaceMaterialResponse(eS);
		ZENITH_ASSERT_GT(xR.m_fRoughness, 0.0f,
			"surface %s is perfectly smooth", ZM_BuildingSurfaceName(eS));
		ZENITH_ASSERT_LE(xR.m_fRoughness, 1.0f,
			"surface %s has a roughness above 1", ZM_BuildingSurfaceName(eS));
		// ★ NOTHING ON A BUILDING IS METALLIC, GLASS INCLUDED. Glass is a
		// dielectric; metallic=1 would tint its reflection by the base colour and
		// kill the diffuse outright.
		ZENITH_ASSERT_EQ_FLOAT(xR.m_fMetallic, 0.0f, 0.0f,
			"surface %s claims to be metal", ZM_BuildingSurfaceName(eS));
	}

	// Names are pairwise distinct -- they are FILENAME components, so a collision
	// would silently make two surfaces share one .zmesh.
	for (u_int a = 0u; a < (u_int)ZM_BUILDING_SURFACE_COUNT; ++a)
	{
		for (u_int b = a + 1u; b < (u_int)ZM_BUILDING_SURFACE_COUNT; ++b)
		{
			ZENITH_ASSERT_FALSE(
				strcmp(ZM_BuildingSurfaceName((ZM_BUILDING_SURFACE)a),
				       ZM_BuildingSurfaceName((ZM_BUILDING_SURFACE)b)) == 0,
				"surfaces %u and %u share the asset-stem name '%s'", a, b,
				ZM_BuildingSurfaceName((ZM_BUILDING_SURFACE)a));
		}
	}

	// The differentiation the split was made for.
	const ZM_BuildingSurfaceResponse xWall  = ZM_BuildingSurfaceMaterialResponse(ZM_BUILDING_SURFACE_WALL);
	const ZM_BuildingSurfaceResponse xTrim  = ZM_BuildingSurfaceMaterialResponse(ZM_BUILDING_SURFACE_TRIM);
	const ZM_BuildingSurfaceResponse xGlass = ZM_BuildingSurfaceMaterialResponse(ZM_BUILDING_SURFACE_GLASS);
	ZENITH_ASSERT_LT(xTrim.m_fRoughness, xWall.m_fRoughness,
		"painted trim is not smoother than the render it is set into (%.2f vs %.2f)",
		xTrim.m_fRoughness, xWall.m_fRoughness);
	ZENITH_ASSERT_LT(xGlass.m_fRoughness, xTrim.m_fRoughness,
		"glass is not smoother than the frame around it (%.2f vs %.2f)",
		xGlass.m_fRoughness, xTrim.m_fRoughness);
	ZENITH_ASSERT_LT(xGlass.m_fRoughness, 0.2f,
		"glass at roughness %.2f will read as matte plastic, which is the exact "
		"failure the surface split exists to prevent", xGlass.m_fRoughness);

	// Tiles differ, or the density argument is decoration.
	ZENITH_ASSERT_LT(ZM_BuildingSurfaceTileMetres(ZM_BUILDING_SURFACE_TRIM),
		ZM_BuildingSurfaceTileMetres(ZM_BUILDING_SURFACE_WALL),
		"trim tiles no finer than masonry, so a 0.1 m board smears one texel");

	// Out-of-range inputs are answered, not crashed (the TOTAL contract).
	const ZM_BUILDING_SURFACE eBad = (ZM_BUILDING_SURFACE)ZM_BUILDING_SURFACE_COUNT;
	ZENITH_ASSERT_NOT_NULL(ZM_BuildingSurfaceName(eBad),
		"an out-of-range surface returned no name");
	ZENITH_ASSERT_GT(ZM_BuildingSurfaceResolution(eBad), 0u,
		"an out-of-range surface returned no resolution");
	ZENITH_ASSERT_GT(ZM_BuildingSurfaceTileMetres(eBad), 0.0f,
		"an out-of-range surface returned no tile size");
}

// ############################################################################
// 4. The asset-kind algebra round-trips
// ############################################################################
ZENITH_TEST(ZM_Gen, BuildingGen_AssetKindAlgebra)
{
	ZENITH_ASSERT_EQ((u_int)ZM_BUILDING_ASSET_KIND_COUNT,
		(u_int)ZM_BUILDING_SURFACE_COUNT * (u_int)ZM_BUILDING_SLOT_COUNT + 1u,
		"the kind count is not 'six per surface plus the model' -- the bake manifest "
		"walks this range and would check the wrong files");

	for (u_int s = 0u; s < (u_int)ZM_BUILDING_SURFACE_COUNT; ++s)
	{
		for (u_int k = 0u; k < (u_int)ZM_BUILDING_SLOT_COUNT; ++k)
		{
			const ZM_BUILDING_SURFACE eS = (ZM_BUILDING_SURFACE)s;
			const ZM_BUILDING_ASSET_SLOT eK = (ZM_BUILDING_ASSET_SLOT)k;
			const ZM_BUILDING_ASSET_KIND eKind = ZM_BuildingSurfaceAssetKind(eS, eK);

			ZENITH_ASSERT_LT((u_int)eKind, (u_int)ZM_BUILDING_ASSET_MODEL,
				"(%u,%u) composed to kind %u, which is not per-surface", s, k, (u_int)eKind);
			ZENITH_ASSERT_EQ((u_int)ZM_BuildingAssetSurface(eKind), s,
				"kind %u did not decompose back to surface %u", (u_int)eKind, s);
			ZENITH_ASSERT_EQ((u_int)ZM_BuildingAssetSlot(eKind), k,
				"kind %u did not decompose back to slot %u", (u_int)eKind, k);
		}
	}

	// The model kind sits OUTSIDE the per-surface block, and the decomposers say so
	// rather than answering with a plausible-looking surface.
	ZENITH_ASSERT_EQ((u_int)ZM_BuildingAssetSurface(ZM_BUILDING_ASSET_MODEL),
		(u_int)ZM_BUILDING_SURFACE_WALL,
		"the model kind did not fall back to WALL as its total contract states");
	ZENITH_ASSERT_EQ((u_int)ZM_BuildingAssetSlot(ZM_BUILDING_ASSET_MODEL),
		(u_int)ZM_BUILDING_SLOT_MESH,
		"the model kind did not fall back to MESH as its total contract states");
}

// ############################################################################
// 5. Asset-path scheme -- golden refs + truncation
// ############################################################################
ZENITH_TEST(ZM_Gen, BuildingGen_AssetPathScheme)
{
	char acRef[512];

	ZENITH_ASSERT_TRUE(ZM_BuildingAssetPath(ZM_BUILDING_PLAYER_HOME,
		ZM_BuildingSurfaceAssetKind(ZM_BUILDING_SURFACE_WALL, ZM_BUILDING_SLOT_MESH),
		acRef, sizeof(acRef)), "the PlayerHome wall mesh ref did not build");
	ZENITH_ASSERT_STREQ(acRef, "game:Buildings/PlayerHome/PlayerHome_wall.zmesh",
		"the per-surface mesh ref scheme moved");

	ZENITH_ASSERT_TRUE(ZM_BuildingAssetPath(ZM_BUILDING_LAB,
		ZM_BuildingSurfaceAssetKind(ZM_BUILDING_SURFACE_ROOF, ZM_BUILDING_SLOT_NORMAL),
		acRef, sizeof(acRef)), "the Lab roof normal ref did not build");
	ZENITH_ASSERT_STREQ(acRef, "game:Buildings/Lab/Lab_roof_normal.ztxtr",
		"the per-surface normal-map ref scheme moved");

	ZENITH_ASSERT_TRUE(ZM_BuildingAssetPath(ZM_BUILDING_LAB,
		ZM_BuildingSurfaceAssetKind(ZM_BUILDING_SURFACE_GLASS, ZM_BUILDING_SLOT_MATERIAL),
		acRef, sizeof(acRef)), "the Lab glass material ref did not build");
	ZENITH_ASSERT_STREQ(acRef, "game:Buildings/Lab/Lab_glass.zmtrl",
		"the per-surface material ref scheme moved");

	ZENITH_ASSERT_TRUE(ZM_BuildingAssetPath(ZM_BUILDING_PLAYER_HOME,
		ZM_BUILDING_ASSET_MODEL, acRef, sizeof(acRef)),
		"the PlayerHome model ref did not build");
	ZENITH_ASSERT_STREQ(acRef, "game:Buildings/PlayerHome/PlayerHome.zmodel",
		"the model ref scheme moved -- the scene authoring loads exactly this string");

	// TOTALITY: every id x every kind builds a non-empty ref.
	for (u_int u = 0u; u < (u_int)ZM_BUILDING_COUNT; ++u)
	{
		for (u_int k = 0u; k < (u_int)ZM_BUILDING_ASSET_KIND_COUNT; ++k)
		{
			ZENITH_ASSERT_TRUE(ZM_BuildingAssetPath((ZM_BUILDING_ID)u,
				(ZM_BUILDING_ASSET_KIND)k, acRef, sizeof(acRef)),
				"%s kind %u did not build a ref", ZM_GetBuildingName((ZM_BUILDING_ID)u), k);
			ZENITH_ASSERT_GT((u_int)strlen(acRef), 0u,
				"%s kind %u built an empty ref", ZM_GetBuildingName((ZM_BUILDING_ID)u), k);
		}
	}

	// Truncation is reported, and the buffer is still NUL-terminated.
	char acTiny[8];
	ZENITH_ASSERT_FALSE(ZM_BuildingAssetPath(ZM_BUILDING_PLAYER_HOME,
		ZM_BUILDING_ASSET_MODEL, acTiny, sizeof(acTiny)),
		"an 8-byte buffer did not report truncation");
	ZENITH_ASSERT_LT((u_int)strlen(acTiny), (u_int)sizeof(acTiny),
		"the truncated buffer is not NUL-terminated within its capacity");
}

// ############################################################################
// 6. Build determinism -- same id, byte-identical bundle
// ############################################################################
ZENITH_TEST(ZM_Gen, BuildingGen_BuildDeterminism)
{
	for (u_int u = 0u; u < (u_int)ZM_BUILDING_COUNT; ++u)
	{
		const ZM_BUILDING_ID eId = (ZM_BUILDING_ID)u;
		ZM_Building xA, xB;
		ZM_BuildBuilding(eId, xA);
		ZM_BuildBuilding(eId, xB);

		ZENITH_ASSERT_TRUE(ZM_BuildingBuildEqual(xA, xB),
			"%s did not rebuild byte-identically", ZM_GetBuildingName(eId));
		ZENITH_ASSERT_EQ(ZM_BuildingContentHash(xA), ZM_BuildingContentHash(xB),
			"%s's content hash is not reproducible", ZM_GetBuildingName(eId));

		// Every surface individually, so a failure names the surface.
		for (u_int s = 0u; s < (u_int)ZM_BUILDING_SURFACE_COUNT; ++s)
		{
			ZENITH_ASSERT_TRUE(ZM_BuildingMeshEqual(xA.m_axMesh[s], xB.m_axMesh[s]),
				"%s/%s did not rebuild byte-identically", ZM_GetBuildingName(eId),
				ZM_BuildingSurfaceName((ZM_BUILDING_SURFACE)s));
			ZENITH_ASSERT_TRUE(xA.m_axTextures[s].Equals(xB.m_axTextures[s]),
				"%s/%s's maps did not rebuild byte-identically", ZM_GetBuildingName(eId),
				ZM_BuildingSurfaceName((ZM_BUILDING_SURFACE)s));
		}
	}

	// ANTI-VACUITY: two distinct ids must NOT hash the same, or "deterministic"
	// would be satisfied by a generator that emits one building.
	ZM_Building xHome, xLab;
	ZM_BuildBuilding(ZM_BUILDING_PLAYER_HOME, xHome);
	ZM_BuildBuilding(ZM_BUILDING_LAB, xLab);
	ZENITH_ASSERT_FALSE(ZM_BuildingBuildEqual(xHome, xLab),
		"PlayerHome and Lab built the same bundle");
	ZENITH_ASSERT_NE(ZM_BuildingContentHash(xHome), ZM_BuildingContentHash(xLab),
		"PlayerHome and Lab share a content hash");
}

// ############################################################################
// 7. The static-mesh contract, per surface
// ############################################################################
ZENITH_TEST(ZM_Gen, BuildingGen_StaticMeshContract)
{
	for (u_int u = 0u; u < (u_int)ZM_BUILDING_COUNT; ++u)
	{
		const ZM_BUILDING_ID eId = (ZM_BUILDING_ID)u;
		const ZM_BuildingRecipe xR = ZM_ResolveBuildingRecipe(eId);

		for (u_int s = 0u; s < (u_int)ZM_BUILDING_SURFACE_COUNT; ++s)
		{
			const ZM_BUILDING_SURFACE eS = (ZM_BUILDING_SURFACE)s;
			ZM_GenMesh xMesh;
			ZM_BuildBuildingSurfaceMesh(xR, eS, xMesh);

			ZENITH_ASSERT_GT(xMesh.GetNumTris(), 0u,
				"%s/%s emitted no triangles -- a degenerate .zmesh fails the model load",
				ZM_GetBuildingName(eId), ZM_BuildingSurfaceName(eS));
			ZENITH_ASSERT_EQ(xMesh.GetNumBones(), 0u,
				"%s/%s carries bones", ZM_GetBuildingName(eId), ZM_BuildingSurfaceName(eS));
			ZENITH_ASSERT_EQ(xMesh.m_xBoneIndices.GetSize(), 0u,
				"%s/%s carries bone indices", ZM_GetBuildingName(eId), ZM_BuildingSurfaceName(eS));
			ZENITH_ASSERT_EQ(xMesh.m_xBoneWeights.GetSize(), 0u,
				"%s/%s carries bone weights", ZM_GetBuildingName(eId), ZM_BuildingSurfaceName(eS));
			// Tangents are finalised for every surface -- a normal map without them
			// is sampled in an undefined basis and reads as random lighting.
			ZENITH_ASSERT_EQ(xMesh.m_xTangents.GetSize(), xMesh.GetNumVerts(),
				"%s/%s has no per-vertex tangent, so its normal map has no basis",
				ZM_GetBuildingName(eId), ZM_BuildingSurfaceName(eS));
		}
	}
}

// ############################################################################
// 8. Shell metrics -- pure, and geometrically coherent
// ############################################################################
ZENITH_TEST(ZM_Gen, BuildingGen_ShellMetricsDerivation)
{
	for (u_int u = 0u; u < (u_int)ZM_BUILDING_COUNT; ++u)
	{
		const ZM_BUILDING_ID eId = (ZM_BUILDING_ID)u;
		const ZM_BuildingRecipe xR = ZM_ResolveBuildingRecipe(eId);
		const ZM_BuildingShellMetrics xA = ZM_ResolveBuildingShellMetrics(xR);
		const ZM_BuildingShellMetrics xB = ZM_ResolveBuildingShellMetrics(xR);

		ZENITH_ASSERT_EQ_FLOAT(xA.m_fWidth,   xB.m_fWidth,   0.0f,
			"%s's metrics are not a pure function of its recipe", ZM_GetBuildingName(eId));
		ZENITH_ASSERT_EQ_FLOAT(xA.m_fRidgeY,  xB.m_fRidgeY,  0.0f,
			"%s's ridge height is not reproducible", ZM_GetBuildingName(eId));

		ZENITH_ASSERT_GT(xA.m_fWallTop, 0.0f,
			"%s has no wall height", ZM_GetBuildingName(eId));
		ZENITH_ASSERT_GT(xA.m_fRidgeY, xA.m_fWallTop,
			"%s's roof does not rise above its eave (ridge %.2f, eave %.2f)",
			ZM_GetBuildingName(eId), xA.m_fRidgeY, xA.m_fWallTop);
		ZENITH_ASSERT_GT(xA.m_fExW, xA.m_fHalfW,
			"%s's eave does not overhang its wall", ZM_GetBuildingName(eId));
		ZENITH_ASSERT_GT(xA.m_fExD, xA.m_fHalfD,
			"%s's eave does not overhang its wall in Z", ZM_GetBuildingName(eId));

		// Window helpers stay on the facade they belong to.
		for (u_int c = 0u; c < xA.m_uWindowCols; ++c)
		{
			const float fX = xA.WindowCentreX(c);
			ZENITH_ASSERT_LT(std::fabs(fX), xA.m_fHalfW,
				"%s's window column %u is centred off the facade (x=%.2f, half-width %.2f)",
				ZM_GetBuildingName(eId), c, fX, xA.m_fHalfW);
		}
		for (u_int r = 0u; r < xA.m_uWindowRows; ++r)
		{
			ZENITH_ASSERT_GT(xA.WindowSillY(r), 0.0f,
				"%s's window row %u sits at or below the ground", ZM_GetBuildingName(eId), r);
		}

		// The accessors are TOTAL: an out-of-range index clamps rather than reads off
		// the end of the grid.
		ZENITH_ASSERT_EQ_FLOAT(xA.WindowCentreX(xA.m_uWindowCols + 5u),
			xA.WindowCentreX(xA.m_uWindowCols - 1u), 0.0f,
			"%s's WindowCentreX does not clamp", ZM_GetBuildingName(eId));
		ZENITH_ASSERT_EQ_FLOAT(xA.WindowSillY(xA.m_uWindowRows + 5u),
			xA.WindowSillY(xA.m_uWindowRows - 1u), 0.0f,
			"%s's WindowSillY does not clamp", ZM_GetBuildingName(eId));
	}

	// A FLAT roof rises by the parapet and nothing else; a pitched one rises more.
	const ZM_BuildingShellMetrics xFlat =
		ZM_ResolveBuildingShellMetrics(ZM_ResolveBuildingRecipe(eBG_FLAT));
	ZENITH_ASSERT_EQ_FLOAT(xFlat.m_fRise, 0.0f, 0.0f,
		"a FLAT roof was given a pitch rise");
	ZENITH_ASSERT_EQ_FLOAT(xFlat.m_fRidgeY - xFlat.m_fWallTop,
		fZM_BUILDING_PARAPET_HEIGHT, fBG_EXACT,
		"a FLAT roof's top is not its parapet");
	const ZM_BuildingShellMetrics xGable =
		ZM_ResolveBuildingShellMetrics(ZM_ResolveBuildingRecipe(eBG_GABLE));
	ZENITH_ASSERT_GT(xGable.m_fRise, 0.0f, "a GABLE roof was given no rise");
	const ZM_BuildingShellMetrics xHip =
		ZM_ResolveBuildingShellMetrics(ZM_ResolveBuildingRecipe(eBG_HIP));
	ZENITH_ASSERT_GT(xHip.m_fRise, 0.0f, "a HIP roof was given no rise");
}

// ############################################################################
// 9. Site-fixed rows are NOT jittered -- and the RNG stream is unchanged
// ############################################################################

// ★ THE SECOND HALF IS THE POINT. Suppressing the jitter by SKIPPING the draws
// would pass the first three clauses and silently advance the MESH stream
// differently for exactly two rows -- a divergence that only ever surfaces as a
// determinism failure years later. So this also asserts that a site-fixed
// recipe's MESH RNG is in the same position after the metrics are resolved as a
// free-standing one's, by drawing from a fresh RNG and comparing draw counts.
ZENITH_TEST(ZM_Gen, BuildingGen_SiteFixedSuppressesJitter)
{
	// A site-fixed row's footprint is EXACTLY the roster's.
	const ZM_BUILDING_ID aeFixed[2] = { ZM_BUILDING_PLAYER_HOME, ZM_BUILDING_LAB };
	for (u_int i = 0u; i < 2u; ++i)
	{
		const ZM_BuildingData& xData = ZM_GetBuildingData(aeFixed[i]);
		ZENITH_ASSERT_TRUE(xData.m_bSiteFixed,
			"%s is expected to be site-fixed and is not", xData.m_szName);

		const ZM_BuildingShellMetrics xM =
			ZM_ResolveBuildingShellMetrics(ZM_ResolveBuildingRecipe(aeFixed[i]));
		ZENITH_ASSERT_EQ_FLOAT(xM.m_fWidth, xData.m_fWidth, 0.0f,
			"%s's built width %.4f is not its roster width %.4f -- the jitter reached "
			"a footprint pinned to a hand-authored site",
			xData.m_szName, xM.m_fWidth, xData.m_fWidth);
		ZENITH_ASSERT_EQ_FLOAT(xM.m_fDepth, xData.m_fDepth, 0.0f,
			"%s's built depth %.4f is not its roster depth %.4f",
			xData.m_szName, xM.m_fDepth, xData.m_fDepth);
		ZENITH_ASSERT_EQ_FLOAT(xM.m_fStoreyHeight, xData.m_fStoreyHeight, 0.0f,
			"%s's built storey height %.4f is not its roster value %.4f",
			xData.m_szName, xM.m_fStoreyHeight, xData.m_fStoreyHeight);
	}

	// ANTI-VACUITY: a FREE-STANDING row must actually BE jittered, or the clause
	// above is satisfied by a generator that dropped the jitter entirely.
	u_int uJittered = 0u;
	for (u_int u = 0u; u < (u_int)ZM_BUILDING_COUNT; ++u)
	{
		const ZM_BuildingData& xData = ZM_GetBuildingData((ZM_BUILDING_ID)u);
		if (xData.m_bSiteFixed) { continue; }
		const ZM_BuildingShellMetrics xM =
			ZM_ResolveBuildingShellMetrics(ZM_ResolveBuildingRecipe((ZM_BUILDING_ID)u));
		if (std::fabs(xM.m_fWidth - xData.m_fWidth) > fBG_EXACT) { ++uJittered; }
	}
	ZENITH_ASSERT_GT(uJittered, 0u,
		"not one free-standing building's width was perturbed -- the shape jitter is "
		"dead, and every CottageWarm on a street is now the same stamped box");

	// The stream position is unchanged: four draws happen either way. Drawing four
	// values from a fresh MESH RNG and comparing against the SAME four values the
	// resolver must have consumed proves the count, because a fifth draw here would
	// otherwise return the value the resolver's fifth would have.
	for (u_int i = 0u; i < 2u; ++i)
	{
		const ZM_BuildingRecipe xR = ZM_ResolveBuildingRecipe(aeFixed[i]);
		ZM_GenRNG xRng = ZM_MakeGenRNG(xR, ZM_GEN_DOMAIN_MESH);
		(void)xRng.NextFloatRange(-0.03f, 0.03f);
		(void)xRng.NextFloatRange(-0.03f, 0.03f);
		(void)xRng.NextFloatRange(-0.03f, 0.03f);
		(void)xRng.NextFloatRange(-0.10f, 0.10f);
		const float fFifth = xRng.NextFloat01();

		ZM_GenRNG xRef = ZM_MakeGenRNG(xR, ZM_GEN_DOMAIN_MESH);
		for (u_int d = 0u; d < 4u; ++d) { (void)xRef.NextFloat01(); }
		ZENITH_ASSERT_EQ_FLOAT(fFifth, xRef.NextFloat01(), 0.0f,
			"%s's MESH domain does not consume exactly four draws before anything "
			"else reads it", ZM_GetBuildingName(aeFixed[i]));
	}
}

// ############################################################################
// 10. The four surfaces register with each other
// ############################################################################
ZENITH_TEST(ZM_Gen, BuildingGen_SurfacesRegister)
{
	for (u_int u = 0u; u < (u_int)ZM_BUILDING_COUNT; ++u)
	{
		const ZM_BUILDING_ID eId = (ZM_BUILDING_ID)u;
		ZM_Building xB;
		ZM_BuildBuilding(eId, xB);
		const ZM_BuildingShellMetrics xM =
			ZM_ResolveBuildingShellMetrics(ZM_ResolveBuildingRecipe(eId));

		// No surface is empty. A zero-triangle submesh would bake a degenerate
		// .zmesh and take the whole .zmodel down with it.
		for (u_int s = 0u; s < (u_int)ZM_BUILDING_SURFACE_COUNT; ++s)
		{
			ZENITH_ASSERT_GT(xB.m_axMesh[s].GetNumTris(), 0u,
				"%s/%s is empty", ZM_GetBuildingName(eId),
				ZM_BuildingSurfaceName((ZM_BUILDING_SURFACE)s));
		}

		const Zenith_Maths::Vector3 xWallMin = ZM_GenMeshBoundsMin(xB.m_axMesh[ZM_BUILDING_SURFACE_WALL]);
		const Zenith_Maths::Vector3 xWallMax = ZM_GenMeshBoundsMax(xB.m_axMesh[ZM_BUILDING_SURFACE_WALL]);
		const Zenith_Maths::Vector3 xRoofMin = ZM_GenMeshBoundsMin(xB.m_axMesh[ZM_BUILDING_SURFACE_ROOF]);
		const Zenith_Maths::Vector3 xRoofMax = ZM_GenMeshBoundsMax(xB.m_axMesh[ZM_BUILDING_SURFACE_ROOF]);
		const Zenith_Maths::Vector3 xGlassMin = ZM_GenMeshBoundsMin(xB.m_axMesh[ZM_BUILDING_SURFACE_GLASS]);
		const Zenith_Maths::Vector3 xGlassMax = ZM_GenMeshBoundsMax(xB.m_axMesh[ZM_BUILDING_SURFACE_GLASS]);

		// FEET ON THE FLOOR: the wall is grounded at y=0, matching every other
		// generated asset's bind convention. A building authored half a metre into
		// the terrain is the classic version of this bug.
		ZENITH_ASSERT_EQ_FLOAT(xWallMin.y, 0.0f, fBG_EXACT,
			"%s's wall is not grounded at y=0 (min y = %.4f)",
			ZM_GetBuildingName(eId), xWallMin.y);

		// The roof sits ON the walls, not through them.
		ZENITH_ASSERT_GE(xRoofMax.y, xWallMax.y,
			"%s's roof (top %.2f) does not reach its wall top (%.2f)",
			ZM_GetBuildingName(eId), xRoofMax.y, xWallMax.y);
		ZENITH_ASSERT_GE(xRoofMin.y, xM.m_fWallTop - fZM_BUILDING_FASCIA_HEIGHT - 0.5f,
			"%s's roof starts %.2f m below its eave -- it is inside the building",
			ZM_GetBuildingName(eId), xM.m_fWallTop - xRoofMin.y);

		// Glass never floats outside the building's own envelope.
		ZENITH_ASSERT_LE(xGlassMax.y, xM.m_fRidgeY,
			"%s's glass reaches above the ridge", ZM_GetBuildingName(eId));
		ZENITH_ASSERT_GE(xGlassMin.y, 0.0f,
			"%s's glass reaches below the ground", ZM_GetBuildingName(eId));
		ZENITH_ASSERT_LE(xGlassMax.x, xM.m_fHalfW + fBG_EXACT,
			"%s's glass reaches past the wall in +X", ZM_GetBuildingName(eId));
		ZENITH_ASSERT_GE(xGlassMin.x, -xM.m_fHalfW - fBG_EXACT,
			"%s's glass reaches past the wall in -X", ZM_GetBuildingName(eId));
	}
}

// ############################################################################
// 11. World UVs give UNIFORM texel density
// ############################################################################

// ★ THIS IS THE CLAUSE THE WHOLE TILING DESIGN EXISTS FOR, and it is written as
// a comparison between a LARGE surface and a SMALL one because that is the exact
// axis the old shared-atlas scheme failed on: a 16.5 m wall and a 0.4 m quoin
// each received the full [0,1] island, so their densities differed by 40x and the
// wall -- the thing most on screen -- got the worst of it.
//
// Density here is (UV span / world span), which under a world-scaled projection
// must equal 1/tile for EVERY primitive regardless of size.
ZENITH_TEST(ZM_Gen, BuildingGen_WorldUVsAreUniformDensity)
{
	for (u_int u = 0u; u < (u_int)ZM_BUILDING_COUNT; ++u)
	{
		const ZM_BUILDING_ID eId = (ZM_BUILDING_ID)u;
		const ZM_BuildingRecipe xR = ZM_ResolveBuildingRecipe(eId);

		for (u_int s = 0u; s < (u_int)ZM_BUILDING_SURFACE_COUNT; ++s)
		{
			const ZM_BUILDING_SURFACE eS = (ZM_BUILDING_SURFACE)s;
			const float fTile = ZM_BuildingSurfaceTileMetresFor(xR, eS);
			ZM_GenMesh xMesh;
			ZM_BuildBuildingSurfaceMesh(xR, eS, xMesh);

			// For every triangle, the ratio of UV edge length to world edge length
			// must be 1/tile on the projected axes. Checking it per TRIANGLE rather
			// than over the whole mesh is what makes this a density claim rather
			// than a bounds claim.
			const u_int uTris = xMesh.GetNumTris();
			for (u_int t = 0u; t < uTris; ++t)
			{
				const u_int uA = xMesh.m_xIndices.Get(t * 3u + 0u);
				const u_int uB = xMesh.m_xIndices.Get(t * 3u + 1u);
				const Zenith_Maths::Vector3 xDp =
					xMesh.m_xPositions.Get(uB) - xMesh.m_xPositions.Get(uA);
				const Zenith_Maths::Vector2 xDuv =
					xMesh.m_xUVs.Get(uB) - xMesh.m_xUVs.Get(uA);

				const float fWorld = glm::length(xDp);
				const float fUV    = glm::length(xDuv);
				if (fWorld < 1.0e-3f) { continue; }   // a degenerate edge says nothing

				// The projection drops one world axis, so |duv| <= |dp|/tile, with
				// equality on an edge lying in the projection plane. The BOUND is
				// what a stretched atlas would violate: it would produce edges whose
				// UV span is wildly larger than their world span allows.
				ZENITH_ASSERT_LE(fUV, fWorld / fTile + fBG_EXACT,
					"%s/%s triangle %u has a UV edge of %.4f over a %.4f m world edge "
					"at a %.2f m tile -- density is not world-uniform",
					ZM_GetBuildingName(eId), ZM_BuildingSurfaceName(eS), t, fUV, fWorld, fTile);
			}
		}
	}

	// ANTI-VACUITY: the bound above is one-sided, so prove that a LARGE surface
	// really does receive proportionally more UV span than a small one -- i.e.
	// that the projection is active rather than everything collapsing to zero.
	ZM_GenMesh xWall;
	ZM_BuildBuildingSurfaceMesh(ZM_ResolveBuildingRecipe(ZM_BUILDING_PLAYER_HOME),
		ZM_BUILDING_SURFACE_WALL, xWall);
	float fMaxU = 0.0f;
	for (u_int v = 0u; v < xWall.GetNumVerts(); ++v)
	{
		const float fA = std::fabs(xWall.m_xUVs.Get(v).x);
		if (fA > fMaxU) { fMaxU = fA; }
	}
	// PlayerHome is 16.5 m wide, so a world projection must reach past
	// |u| = halfWidth / tile. A [0,1] atlas mapping could not. Derived from the
	// live tile rather than a literal, because the wall's tile is its own storey
	// height and moves with the roster.
	const float fHomeTile = ZM_BuildingSurfaceTileMetresFor(
		ZM_ResolveBuildingRecipe(ZM_BUILDING_PLAYER_HOME), ZM_BUILDING_SURFACE_WALL);
	const float fExpectU = (ZM_GetBuildingData(ZM_BUILDING_PLAYER_HOME).m_fWidth * 0.5f) / fHomeTile;
	ZENITH_ASSERT_GT(fMaxU, fExpectU - fBG_EXACT,
		"the PlayerHome wall's largest |u| is %.2f but a 16.5 m wall on a %.2f m tile "
		"must reach %.2f -- it cannot be world-projected and stay inside an atlas "
		"island", fMaxU, fHomeTile, fExpectU);
}

// ############################################################################
// ★★ 11b. NOTHING IS COPLANAR WITH THE WALL IT IS APPLIED TO
// ############################################################################

// Z-FIGHTING IS THE ONE DEFECT CLASS THIS SUITE COULD NOT SEE, and it shipped:
// the door leaf was emitted spanning from the wall plane INWARD, on the reasoning
// that a door set back into its opening reads as an opening. There is no opening
// -- the wall body is a solid box -- so the leaf's OUTWARD face landed exactly on
// the wall's -Z face, and both Dawnmere buildings' front doors shimmered.
//
// Every clause in this file passed. The bounds were right, the winding was right,
// the UVs were right, the door was exactly the aperture width, and the geometry
// was in exactly the right place -- it was in the SAME place as something else.
//
// ★ THE RULE IS ASYMMETRIC, AND THAT ASYMMETRY IS WHY ONLY THE DOOR FOUGHT. A
// BACK-facing coplanar face is culled and costs nothing: the window frames, the
// sills and the glass all have their inner face on the wall plane and are fine.
// It is a FRONT-facing surface sharing a plane with the wall that fights. So this
// checks exactly that, over every surface, on both facades.
ZENITH_TEST(ZM_Gen, BuildingGen_NoFrontFaceIsCoplanarWithTheWall)
{
	// Tight: a real ledge is centimetres, a z-fight is micrometres. 1 mm sits far
	// below the smallest deliberate offset (fZM_BUILDING_GLAZING_BAR_OUT, 0.02)
	// and far above float noise on a 20 m building.
	constexpr float fMIN_SEPARATION = 1.0e-3f;

	for (u_int u = 0u; u < (u_int)ZM_BUILDING_COUNT; ++u)
	{
		const ZM_BUILDING_ID eId = (ZM_BUILDING_ID)u;
		const ZM_BuildingRecipe xR = ZM_ResolveBuildingRecipe(eId);
		const ZM_BuildingShellMetrics xM = ZM_ResolveBuildingShellMetrics(xR);

		// The two planes a facade feature can collide with.
		const float afWallPlaneZ[2] = { -xM.m_fHalfD, xM.m_fHalfD };
		const float afOutwardZ[2]   = { -1.0f, 1.0f };

		u_int uFrontFacingChecked = 0u;
		for (u_int s = 0u; s < (u_int)ZM_BUILDING_SURFACE_COUNT; ++s)
		{
			const ZM_BUILDING_SURFACE eS = (ZM_BUILDING_SURFACE)s;
			// The WALL is the reference; it may of course lie on its own plane.
			if (eS == ZM_BUILDING_SURFACE_WALL)
			{
				continue;
			}
			ZM_GenMesh xMesh;
			ZM_BuildBuildingSurfaceMesh(xR, eS, xMesh);

			for (u_int v = 0u; v < xMesh.GetNumVerts(); ++v)
			{
				const Zenith_Maths::Vector3& xN = xMesh.m_xNormals.Get(v);
				const Zenith_Maths::Vector3& xP = xMesh.m_xPositions.Get(v);

				for (u_int f = 0u; f < 2u; ++f)
				{
					// Front-facing on THIS facade: the normal points outward along it.
					if (xN.z * afOutwardZ[f] < 0.9f)
					{
						continue;
					}
					++uFrontFacingChecked;
					ZENITH_ASSERT_GE(std::fabs(xP.z - afWallPlaneZ[f]), fMIN_SEPARATION,
						"%s/%s has an OUTWARD-facing vertex at z=%.5f, only %.6f m from "
						"the wall's own facade plane at z=%.5f. Two front faces sharing a "
						"plane z-fight, and nothing else in this suite can see it -- the "
						"bounds, winding, UVs and placement are all still correct",
						ZM_GetBuildingName(eId), ZM_BuildingSurfaceName(eS),
						xP.z, std::fabs(xP.z - afWallPlaneZ[f]), afWallPlaneZ[f]);
				}
			}
		}

		// ANTI-VACUITY: a building whose trim and glass emitted nothing outward --
		// or a normal convention that flipped -- would satisfy the loop above
		// having examined nothing at all.
		ZENITH_ASSERT_GT(uFrontFacingChecked, 0u,
			"%s presented no outward-facing non-wall vertices on either facade, so "
			"this clause checked nothing", ZM_GetBuildingName(eId));
	}
}

// ############################################################################
// 12. Mesh sensitivity -- the MESH domain, and only it, shapes the mesh
// ############################################################################
ZENITH_TEST(ZM_Gen, BuildingGen_MeshSensitivity)
{
	// A FREE-STANDING building, because a site-fixed one is jitter-immune BY
	// DESIGN and would make this test pass for the wrong reason.
	const ZM_BuildingRecipe xBase = ZM_ResolveBuildingRecipe(eBG_GABLE);
	ZENITH_ASSERT_FALSE(xBase.m_bSiteFixed,
		"the mesh-sensitivity probe picked a site-fixed row, which cannot be jittered");

	ZM_GenMesh xA;
	ZM_BuildBuildingSurfaceMesh(xBase, ZM_BUILDING_SURFACE_WALL, xA);

	ZM_BuildingRecipe xMeshMoved = xBase;
	xMeshMoved.m_aulDomainSeed[ZM_GEN_DOMAIN_MESH] ^= 0x9E3779B97F4A7C15ull;
	ZM_GenMesh xB;
	ZM_BuildBuildingSurfaceMesh(xMeshMoved, ZM_BUILDING_SURFACE_WALL, xB);
	ZENITH_ASSERT_FALSE(ZM_BuildingMeshEqual(xA, xB),
		"moving the MESH seed did not change the mesh -- the shape jitter is dead");

	ZM_BuildingRecipe xAlbedoMoved = xBase;
	xAlbedoMoved.m_aulDomainSeed[ZM_GEN_DOMAIN_ALBEDO] ^= 0x9E3779B97F4A7C15ull;
	ZM_GenMesh xC;
	ZM_BuildBuildingSurfaceMesh(xAlbedoMoved, ZM_BUILDING_SURFACE_WALL, xC);
	ZENITH_ASSERT_TRUE(ZM_BuildingMeshEqual(xA, xC),
		"moving the ALBEDO seed moved the MESH -- the domains are not isolated");
}

// ############################################################################
// 13. The PBR map set is structurally real
// ############################################################################
ZENITH_TEST(ZM_Gen, BuildingGen_TextureSetStructural)
{
	for (u_int u = 0u; u < (u_int)ZM_BUILDING_COUNT; ++u)
	{
		const ZM_BUILDING_ID eId = (ZM_BUILDING_ID)u;
		const ZM_BuildingRecipe xR = ZM_ResolveBuildingRecipe(eId);

		for (u_int s = 0u; s < (u_int)ZM_BUILDING_SURFACE_COUNT; ++s)
		{
			const ZM_BUILDING_SURFACE eS = (ZM_BUILDING_SURFACE)s;
			const char* szS = ZM_BuildingSurfaceName(eS);
			const u_int uRes = ZM_BuildingSurfaceResolution(eS);
			const ZM_BuildingTextureSet xT = ZM_BuildBuildingSurfaceTextures(xR, eS);

			ZENITH_ASSERT_TRUE(xT.NonEmpty(), "%s/%s is missing a map",
				ZM_GetBuildingName(eId), szS);
			ZENITH_ASSERT_EQ(xT.m_xAlbedo.GetWidth(), uRes,
				"%s/%s's albedo is not the surface resolution", ZM_GetBuildingName(eId), szS);
			ZENITH_ASSERT_EQ(xT.m_xNormal.GetWidth(), uRes,
				"%s/%s's normal is not the surface resolution", ZM_GetBuildingName(eId), szS);
			ZENITH_ASSERT_EQ(xT.m_xRoughnessMetallic.GetWidth(), uRes,
				"%s/%s's RM map is not the surface resolution", ZM_GetBuildingName(eId), szS);
			ZENITH_ASSERT_EQ(xT.m_xOcclusion.GetWidth(), uRes,
				"%s/%s's AO map is not the surface resolution", ZM_GetBuildingName(eId), szS);

			// The RM map carries the surface's response in the glTF channels, and
			// METALLIC IS ZERO EVERYWHERE (glass included -- it is a dielectric).
			const Zenith_Maths::Vector4 xRM = xT.m_xRoughnessMetallic.Get(uRes / 2u, uRes / 2u);
			ZENITH_ASSERT_GT(xRM.y, 0.0f,
				"%s/%s's RM map has no roughness in G", ZM_GetBuildingName(eId), szS);
			ZENITH_ASSERT_LE(xRM.y, 1.0f,
				"%s/%s's roughness left [0,1]", ZM_GetBuildingName(eId), szS);
			ZENITH_ASSERT_EQ_FLOAT(xRM.z, 0.0f, fBG_EXACT,
				"%s/%s's RM map claims metal in B", ZM_GetBuildingName(eId), szS);
		}
	}

	// ★ THE NORMAL MAP IS NOT FLAT. A height field that came out constant would
	// produce a uniform (0.5, 0.5, 1) normal -- which loads, binds, samples and
	// renders exactly like no normal map at all. Nothing downstream can tell the
	// difference, so it has to be asserted here.
	const ZM_BuildingRecipe xR = ZM_ResolveBuildingRecipe(ZM_BUILDING_PLAYER_HOME);
	const ZM_GenImage xHeight = ZM_BuildBuildingSurfaceHeight(xR, ZM_BUILDING_SURFACE_WALL);
	float fMinH = 2.0f, fMaxH = -1.0f;
	for (u_int y = 0u; y < xHeight.GetHeight(); ++y)
	{
		for (u_int x = 0u; x < xHeight.GetWidth(); ++x)
		{
			const float fH = xHeight.Get(y, x).x;
			if (fH < fMinH) { fMinH = fH; }
			if (fH > fMaxH) { fMaxH = fH; }
		}
	}
	ZENITH_ASSERT_GT(fMaxH - fMinH, 0.15f,
		"the PlayerHome wall height field spans only %.4f -- the relief is flat and "
		"its normal map is an expensive way to store (0.5, 0.5, 1)", fMaxH - fMinH);

	const ZM_BuildingTextureSet xT =
		ZM_BuildBuildingSurfaceTextures(xR, ZM_BUILDING_SURFACE_WALL);
	u_int uNonFlat = 0u;
	for (u_int y = 0u; y < xT.m_xNormal.GetHeight(); ++y)
	{
		for (u_int x = 0u; x < xT.m_xNormal.GetWidth(); ++x)
		{
			const Zenith_Maths::Vector4 xN = xT.m_xNormal.Get(y, x);
			if (std::fabs(xN.x - 0.5f) > 0.02f || std::fabs(xN.y - 0.5f) > 0.02f)
			{
				++uNonFlat;
			}
		}
	}
	const u_int uTexels = xT.m_xNormal.GetWidth() * xT.m_xNormal.GetHeight();
	ZENITH_ASSERT_GT(uNonFlat, uTexels / 10u,
		"only %u of %u wall normal texels differ from flat", uNonFlat, uTexels);

	// AO likewise: a constant-white occlusion map is the same as none.
	float fMinAO = 2.0f, fMaxAO = -1.0f;
	for (u_int y = 0u; y < xT.m_xOcclusion.GetHeight(); ++y)
	{
		for (u_int x = 0u; x < xT.m_xOcclusion.GetWidth(); ++x)
		{
			const float fA = xT.m_xOcclusion.Get(y, x).x;
			if (fA < fMinAO) { fMinAO = fA; }
			if (fA > fMaxAO) { fMaxAO = fA; }
		}
	}
	ZENITH_ASSERT_GT(fMaxAO - fMinAO, 0.05f,
		"the wall AO map spans only %.4f -- it is uniform, and a uniform AO map is "
		"a constant multiply the engine's SSAO already applies better", fMaxAO - fMinAO);
}

// ############################################################################
// 14. Texture domain isolation
// ############################################################################
ZENITH_TEST(ZM_Gen, BuildingGen_TextureDomainIsolation)
{
	const ZM_BuildingRecipe xBase = ZM_ResolveBuildingRecipe(ZM_BUILDING_PLAYER_HOME);
	const ZM_BuildingTextureSet xA =
		ZM_BuildBuildingSurfaceTextures(xBase, ZM_BUILDING_SURFACE_WALL);

	ZM_BuildingRecipe xAlbedoMoved = xBase;
	xAlbedoMoved.m_aulDomainSeed[ZM_GEN_DOMAIN_ALBEDO] ^= 0x9E3779B97F4A7C15ull;
	const ZM_BuildingTextureSet xB =
		ZM_BuildBuildingSurfaceTextures(xAlbedoMoved, ZM_BUILDING_SURFACE_WALL);
	ZENITH_ASSERT_FALSE(xA.Equals(xB),
		"moving the ALBEDO seed did not change a single texel");

	ZM_BuildingRecipe xMeshMoved = xBase;
	xMeshMoved.m_aulDomainSeed[ZM_GEN_DOMAIN_MESH] ^= 0x9E3779B97F4A7C15ull;
	const ZM_BuildingTextureSet xC =
		ZM_BuildBuildingSurfaceTextures(xMeshMoved, ZM_BUILDING_SURFACE_WALL);
	ZENITH_ASSERT_TRUE(xA.Equals(xC),
		"moving the MESH seed moved a texel -- the domains are not isolated");

	// Distinct palettes read apart.
	const ZM_BuildingTextureSet xWarm = ZM_BuildBuildingSurfaceTextures(
		ZM_ResolveBuildingRecipe(ZM_BUILDING_HOUSE_COTTAGE_WARM), ZM_BUILDING_SURFACE_WALL);
	const ZM_BuildingTextureSet xCool = ZM_BuildBuildingSurfaceTextures(
		ZM_ResolveBuildingRecipe(ZM_BUILDING_HOUSE_COTTAGE_COOL), ZM_BUILDING_SURFACE_WALL);
	ZENITH_ASSERT_FALSE(xWarm.Equals(xCool),
		"the WARM and COOL cottages share a wall texture");

	// Two gyms on the SAME palette must still differ, via the theme tint -- the
	// case a palette-only difference check would miss.
	const ZM_BuildingTextureSet xG1 = ZM_BuildBuildingSurfaceTextures(
		ZM_ResolveBuildingRecipe(ZM_BUILDING_GYM_1), ZM_BUILDING_SURFACE_WALL);
	const ZM_BuildingTextureSet xG2 = ZM_BuildBuildingSurfaceTextures(
		ZM_ResolveBuildingRecipe(ZM_BUILDING_GYM_2), ZM_BUILDING_SURFACE_WALL);
	ZENITH_ASSERT_FALSE(xG1.Equals(xG2), "two gyms share a wall texture");
}

// ############################################################################
// 15. ★ The Dawnmere buildings fit the blockouts that carry their colliders
// ############################################################################

// This is the clause that makes the exterior swap safe. The Dawnmere Home and Lab
// entities keep the AABB blockout colliders they have always had, and the
// generated model is a SEPARATE visual entity placed inside that box. If the
// model's walls reach outside the collider's XZ footprint, a player walks into a
// visible wall and stops half a metre short of it -- or worse, walks THROUGH one.
//
// ★ THE ROOF IS DELIBERATELY EXEMPT IN Y, and that is not a loophole. The shell
// blockout is 4.0 m tall for a building with 3 m walls and a pitched roof; roofs
// are not walked on and the collider is not a bounding box, it is the volume the
// player must not enter. What must be inside it is everything reachable at
// standing height.
ZENITH_TEST(ZM_Gen, BuildingGen_DawnmereBuildingsFitTheirBlockouts)
{
	struct Case
	{
		ZM_BUILDING_ID       m_eId;
		ZM_DawnmereBlockout  m_xShell;
		const char*          m_szWhat;
	};
	const Case axCases[2] = {
		{ ZM_BUILDING_PLAYER_HOME, ZM_GetDawnmereHomeShell(), "Home" },
		{ ZM_BUILDING_LAB,         ZM_GetDawnmereLabShell(),  "Lab"  },
	};

	for (u_int i = 0u; i < 2u; ++i)
	{
		const Case& xC = axCases[i];
		ZM_Building xB;
		ZM_BuildBuilding(xC.m_eId, xB);

		const float fShellHalfX = xC.m_xShell.m_xScale.x * 0.5f;
		const float fShellHalfZ = xC.m_xShell.m_xScale.z * 0.5f;

		// The WALL surface is the one a player can touch. Its footprint (plinth and
		// quoins included -- they are the proudest thing on the building at ground
		// level) must sit inside the collider's XZ.
		const Zenith_Maths::Vector3 xMin = ZM_GenMeshBoundsMin(xB.m_axMesh[ZM_BUILDING_SURFACE_WALL]);
		const Zenith_Maths::Vector3 xMax = ZM_GenMeshBoundsMax(xB.m_axMesh[ZM_BUILDING_SURFACE_WALL]);

		ZENITH_ASSERT_LE(xMax.x, fShellHalfX,
			"the %s model reaches x=+%.3f but its blockout collider stops at +%.3f -- "
			"a player would walk into a visible wall %.3f m before touching it",
			xC.m_szWhat, xMax.x, fShellHalfX, xMax.x - fShellHalfX);
		ZENITH_ASSERT_GE(xMin.x, -fShellHalfX,
			"the %s model reaches x=%.3f, outside its blockout's -%.3f",
			xC.m_szWhat, xMin.x, fShellHalfX);
		ZENITH_ASSERT_LE(xMax.z, fShellHalfZ,
			"the %s model reaches z=+%.3f, outside its blockout's +%.3f",
			xC.m_szWhat, xMax.z, fShellHalfZ);
		ZENITH_ASSERT_GE(xMin.z, -fShellHalfZ,
			"the %s model reaches z=%.3f, outside its blockout's -%.3f",
			xC.m_szWhat, xMin.z, fShellHalfZ);

		// ANTI-VACUITY: the facade must also very nearly FILL the blockout. A model
		// half the size of its collider would pass every clause above and leave the
		// player stopping in mid-air a metre from the wall.
		ZENITH_ASSERT_GT(xMax.x - xMin.x, xC.m_xShell.m_xScale.x - 1.0f,
			"the %s model is %.2f m wide inside a %.2f m collider -- the player would "
			"stop short of the wall", xC.m_szWhat, xMax.x - xMin.x, xC.m_xShell.m_xScale.x);
		ZENITH_ASSERT_GT(xMax.z - xMin.z, xC.m_xShell.m_xScale.z - 1.0f,
			"the %s model is %.2f m deep inside a %.2f m collider",
			xC.m_szWhat, xMax.z - xMin.z, xC.m_xShell.m_xScale.z);
	}

	// The exterior and the interior share ONE real-world scale: the building's
	// walls ARE the interior scene's outer envelope. Read from both headers so a
	// change to either reds here.
	const ZM_BuildingData& xHome = ZM_GetBuildingData(ZM_BUILDING_PLAYER_HOME);
	ZENITH_ASSERT_EQ_FLOAT(xHome.m_fWidth,
		fZM_PLAYERHOME_HALF_WIDTH * 2.0f + fZM_PLAYERHOME_WALL_THICKNESS, fBG_EXACT,
		"the PlayerHome facade is %.2f m wide but the interior's outer wall envelope "
		"is %.2f m -- they no longer describe one building", xHome.m_fWidth,
		fZM_PLAYERHOME_HALF_WIDTH * 2.0f + fZM_PLAYERHOME_WALL_THICKNESS);
	ZENITH_ASSERT_EQ_FLOAT(xHome.m_fStoreyHeight, fZM_PLAYERHOME_WALL_HEIGHT, fBG_EXACT,
		"the PlayerHome facade's storey height %.2f does not match the interior's "
		"%.2f m walls", xHome.m_fStoreyHeight, fZM_PLAYERHOME_WALL_HEIGHT);

	const ZM_BuildingData& xLab = ZM_GetBuildingData(ZM_BUILDING_LAB);
	ZENITH_ASSERT_EQ_FLOAT(xLab.m_fWidth,
		fZM_PROFLAB_HALF_WIDTH * 2.0f + fZM_PROFLAB_WALL_THICKNESS, fBG_EXACT,
		"the Lab facade and the ProfLab interior no longer describe one building");
	ZENITH_ASSERT_EQ_FLOAT(xLab.m_fStoreyHeight, fZM_PROFLAB_WALL_HEIGHT, fBG_EXACT,
		"the Lab facade's storey height does not match the ProfLab interior's walls");

	// ★ AND THE VISIBLE DOOR IS THE APERTURE THE WARP SENSOR IS SIZED AGAINST.
	// This is the clause that would otherwise be invisible: the generator's generic
	// leaf is 1.30 x 2.20, the Home's blockout aperture is 4.0 x 2.5, and a
	// mismatch shows up only as a player walking through a doorway twice the width
	// of the door drawn in it. Nothing else in the suite reads both numbers.
	ZENITH_ASSERT_EQ_FLOAT(xHome.m_fDoorWidth,
		fZM_PLAYERHOME_APERTURE_HALF_WIDTH * 2.0f, fBG_EXACT,
		"the Home facade's %.2f m door does not fill the %.2f m entrance aperture the "
		"jamb blockouts and the door trigger both describe", xHome.m_fDoorWidth,
		fZM_PLAYERHOME_APERTURE_HALF_WIDTH * 2.0f);
	ZENITH_ASSERT_EQ_FLOAT(xHome.m_fDoorHeight, fZM_PLAYERHOME_APERTURE_HEIGHT, fBG_EXACT,
		"the Home facade's door height does not match the entrance aperture");
	ZENITH_ASSERT_EQ_FLOAT(xLab.m_fDoorWidth,
		fZM_PROFLAB_APERTURE_HALF_WIDTH * 2.0f, fBG_EXACT,
		"the Lab facade's door does not fill the ProfLab entrance aperture");
	ZENITH_ASSERT_EQ_FLOAT(xLab.m_fDoorHeight, fZM_PROFLAB_APERTURE_HEIGHT, fBG_EXACT,
		"the Lab facade's door height does not match the ProfLab entrance aperture");

	// ...and the door is actually EMITTED at that size. The roster agreeing with
	// the aperture proves transcription; only the mesh proves the trim builder read
	// it (a guard comparing a value against a re-computation of itself is blind).
	for (u_int i = 0u; i < 2u; ++i)
	{
		const Case& xC = axCases[i];
		const ZM_BuildingShellMetrics xM =
			ZM_ResolveBuildingShellMetrics(ZM_ResolveBuildingRecipe(xC.m_eId));
		ZENITH_ASSERT_EQ_FLOAT(xM.m_fDoorWidth,
			ZM_GetBuildingData(xC.m_eId).m_fDoorWidth, 0.0f,
			"the %s shell metrics dropped the roster door width", xC.m_szWhat);

		ZM_GenMesh xTrim;
		ZM_BuildBuildingSurfaceMesh(ZM_ResolveBuildingRecipe(xC.m_eId),
			ZM_BUILDING_SURFACE_TRIM, xTrim);
		// The door surround reaches to +/-(doorWidth/2 + surround) in X on the -Z
		// face, which is the widest thing the trim emits down at ground level.
		float fMaxXAtGround = 0.0f;
		for (u_int v = 0u; v < xTrim.GetNumVerts(); ++v)
		{
			const Zenith_Maths::Vector3& xP = xTrim.m_xPositions.Get(v);
			if (xP.y < 0.05f && xP.z < 0.0f)
			{
				const float fA = std::fabs(xP.x);
				if (fA > fMaxXAtGround) { fMaxXAtGround = fA; }
			}
		}
		ZENITH_ASSERT_EQ_FLOAT(fMaxXAtGround,
			xM.m_fDoorWidth * 0.5f + fZM_BUILDING_DOOR_SURROUND, fBG_EXACT,
			"the %s's emitted door surround spans +/-%.3f but its door is %.3f m wide "
			"-- the trim builder is not reading the roster door",
			xC.m_szWhat, fMaxXAtGround, xM.m_fDoorWidth);

		// ★ AND NOTHING STANDS IN THE DOORWAY. The window grid is laid out across
		// the whole facade and knows nothing about the door; for a 4 m entrance on a
		// four-column facade the inner pair land at x = +/-1.96, INSIDE it. The
		// first render put a window frame, a sill and a pane through the Home's
		// front door and every clause in this suite stayed green, because none of
		// them compared the two grids.
		//
		// Measured on the GLASS surface: a pane is the unambiguous witness (the trim
		// legitimately has geometry across the doorway -- the surround itself).
		ZM_GenMesh xGlass;
		ZM_BuildBuildingSurfaceMesh(ZM_ResolveBuildingRecipe(xC.m_eId),
			ZM_BUILDING_SURFACE_GLASS, xGlass);
		const float fDoorHalf = xM.m_fDoorWidth * 0.5f + fZM_BUILDING_DOOR_SURROUND;
		for (u_int v = 0u; v < xGlass.GetNumVerts(); ++v)
		{
			const Zenith_Maths::Vector3& xP = xGlass.m_xPositions.Get(v);
			const bool bOnEntranceFace = xP.z < 0.0f;
			const bool bBelowDoorHead   = xP.y < xM.m_fDoorHeight;
			if (bOnEntranceFace && bBelowDoorHead)
			{
				ZENITH_ASSERT_GE(std::fabs(xP.x), fDoorHalf - fBG_EXACT,
					"the %s has glass at x=%.3f, y=%.3f on the entrance facade -- "
					"inside the +/-%.3f doorway. There is a window pane in the front "
					"door", xC.m_szWhat, xP.x, xP.y, fDoorHalf);
			}
		}
	}
}


// ############################################################################
// ★ 16. EVERY BOX EDGE IS CHAMFERED (the shared ZM_StaticMesh appender)
// ############################################################################

// A mathematically sharp arris reflects nothing -- two faces meet at a line of
// zero width, so no highlight ever sits on the edge and the box reads as CG.
// Every static box the architectural families emit is bevelled now, which is a
// change to a SHARED emitter, so it is asserted here on the emitter itself
// rather than only through a building.
//
// ★ THE CLAUSE THAT MATTERS IS THE BOUNDS ONE. A bevel that shrank the box
// would move every envelope contract in the game by 8 mm at once -- the blockout
// fit, the interior envelope, the aperture width. The chamfer INSETS the faces
// and leaves the outer planes populated, so the bounds are identical to the
// un-chamfered box's, and that is what makes it safe to switch on underneath
// five generator families at once.
ZENITH_TEST(ZM_Gen, StaticBoxEdgesAreChamfered)
{
	const ZM_GenUVIsland xIsland = { 0.0f, 0.0f, 1.0f, 1.0f };
	const Zenith_Maths::Vector3 xMin(-0.60f, 0.00f, -0.40f);
	const Zenith_Maths::Vector3 xMax( 0.60f, 2.00f,  0.40f);

	ZM_GenMesh xPlain;
	(void)ZM_StaticMesh::AppendBox(xPlain, xMin, xMax, xIsland, 0.0f);
	ZENITH_ASSERT_EQ(xPlain.GetNumVerts(), 24u,
		"the un-chamfered box is no longer six hard quads");
	ZENITH_ASSERT_EQ(xPlain.GetNumTris(), 12u, "the un-chamfered box is not 12 tris");

	ZM_GenMesh xBevel;
	(void)ZM_StaticMesh::AppendBox(xBevel, xMin, xMax, xIsland);
	// Six inset faces (24) + twelve strips (48) + eight corner tris (24).
	ZENITH_ASSERT_EQ(xBevel.GetNumVerts(), 96u,
		"the chamfered box is not 6 faces + 12 strips + 8 corners");
	ZENITH_ASSERT_EQ(xBevel.GetNumTris(), 44u,
		"the chamfered box is not 12 + 24 + 8 triangles");

	// ★ BOUNDS ARE IDENTICAL. The load-bearing clause: the six outer planes are
	// still touched, so nothing that measures a generated box against a collider,
	// an envelope or an aperture moved.
	const Zenith_Maths::Vector3 xPMin = ZM_GenMeshBoundsMin(xPlain);
	const Zenith_Maths::Vector3 xPMax = ZM_GenMeshBoundsMax(xPlain);
	const Zenith_Maths::Vector3 xBMin = ZM_GenMeshBoundsMin(xBevel);
	const Zenith_Maths::Vector3 xBMax = ZM_GenMeshBoundsMax(xBevel);
	ZENITH_ASSERT_NEAR_VEC3(xBMin, xPMin, 1.0e-5f,
		"chamfering moved the box's minimum corner");
	ZENITH_ASSERT_NEAR_VEC3(xBMax, xPMax, 1.0e-5f,
		"chamfering moved the box's maximum corner");

	// Still a valid static mesh: outward winding, in-range indices, UVs inside
	// the island, no bones. A bevel strip wound the wrong way renders as a black
	// notch and nothing else in the suite could see it.
	const ZM_GenStaticMeshValidation xV = ZM_ValidateGenMeshStatic(xBevel);
	ZENITH_ASSERT_TRUE(xV.m_bWindingOutward,
		"a chamfered box has an inward-wound triangle (first bad %u)", xV.m_uFirstBadTriangle);
	ZENITH_ASSERT_TRUE(xV.m_bIndicesInRange, "a chamfered box has an out-of-range index");
	ZENITH_ASSERT_TRUE(xV.m_bUVsFinite, "a chamfered box left the [0,1] island");
	ZENITH_ASSERT_TRUE(xV.m_bBoundsNonDegen, "a chamfered box is flat on an axis");
	ZENITH_ASSERT_TRUE(xV.m_bNoSkeleton && xV.m_bNoSkinBuffers,
		"a chamfered box is no longer static");

	// ★ THE STRIP NORMALS ARE SMOOTHED, NOT A THIRD FACET. A bevel carrying its
	// own hard normal reads as a visible extra plane rather than as a worn edge
	// catching the light, so every stored normal must still be one of the six
	// axis normals -- each strip vertex inheriting the face it borders.
	u_int uOnBevel = 0u;
	for (u_int v = 0u; v < xBevel.GetNumVerts(); ++v)
	{
		const Zenith_Maths::Vector3& xP = xBevel.m_xPositions.Get(v);
		const float afP[3]  = { xP.x, xP.y, xP.z };
		const float afLo[3] = { xMin.x, xMin.y, xMin.z };
		const float afHi[3] = { xMax.x, xMax.y, xMax.z };
		u_int uInset = 0u;
		for (u_int a = 0u; a < 3u; ++a)
		{
			const float fLo = std::fabs(afP[a] - afLo[a]);
			const float fHi = std::fabs(afP[a] - afHi[a]);
			const float fD  = fLo < fHi ? fLo : fHi;
			if (std::fabs(fD - fZM_STATIC_BOX_CHAMFER) < 1.0e-5f) { ++uInset; }
		}
		if (uInset >= 2u) { ++uOnBevel; }

		const Zenith_Maths::Vector3& xN = xBevel.m_xNormals.Get(v);
		ZENITH_ASSERT_EQ_FLOAT(glm::length(xN), 1.0f, 1.0e-4f,
			"chamfered-box vertex %u carries a non-unit normal", v);
		const float fAx = std::fabs(xN.x), fAy = std::fabs(xN.y), fAz = std::fabs(xN.z);
		const float fMaxComp = fAx > fAy ? (fAx > fAz ? fAx : fAz) : (fAy > fAz ? fAy : fAz);
		ZENITH_ASSERT_EQ_FLOAT(fMaxComp, 1.0f, 1.0e-4f,
			"chamfered-box vertex %u carries a facet normal rather than one of the two "
			"faces its bevel bridges -- the chamfer will read as an extra plane", v);
	}
	ZENITH_ASSERT_GT(uOnBevel, 0u,
		"no vertex sits on a bevel -- the chamfer emitted nothing and the vertex "
		"count above is being satisfied some other way");

	// ★ A BOX TOO THIN TO CARRY THE BEVEL GETS A SMALLER ONE, NOT AN INVERTED
	// ONE. A 6 mm pane at an 8 mm chamfer would otherwise fold through itself.
	ZM_GenMesh xThin;
	(void)ZM_StaticMesh::AppendBox(xThin,
		Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f),
		Zenith_Maths::Vector3(1.0f, 1.0f, 0.006f), xIsland);
	const ZM_GenStaticMeshValidation xTV = ZM_ValidateGenMeshStatic(xThin);
	ZENITH_ASSERT_TRUE(xTV.m_bWindingOutward,
		"a 6 mm-thick box folded through itself when chamfered (first bad tri %u)",
		xTV.m_uFirstBadTriangle);
	ZENITH_ASSERT_EQ_FLOAT(ZM_GenMeshBoundsMax(xThin).z, 0.006f, 1.0e-6f,
		"a thin box's clamped chamfer moved its far face");

	// A world-UV'd chamfered box keeps UNIFORM density: each strip is projected
	// on the plane of the face it borders, never smeared across the switch --
	// which is exactly what a per-VERTEX plane choice would do, because a strip's
	// four vertices carry two different normals.
	ZM_GenMesh xWorld;
	(void)ZM_StaticMesh::AppendWorldBox(xWorld, xMin, xMax, 2.0f);
	for (u_int t = 0u; t < xWorld.GetNumTris(); ++t)
	{
		const u_int uA = xWorld.m_xIndices.Get(t * 3u + 0u);
		const u_int uB = xWorld.m_xIndices.Get(t * 3u + 1u);
		const Zenith_Maths::Vector3 xDp = xWorld.m_xPositions.Get(uB) - xWorld.m_xPositions.Get(uA);
		const Zenith_Maths::Vector2 xDuv = xWorld.m_xUVs.Get(uB) - xWorld.m_xUVs.Get(uA);
		if (glm::length(xDp) < 1.0e-4f) { continue; }
		ZENITH_ASSERT_LE(glm::length(xDuv), glm::length(xDp) / 2.0f + 1.0e-4f,
			"chamfered-box triangle %u has a UV edge of %.5f over a %.5f m world edge "
			"-- a bevel strip was projected across two different planes",
			t, glm::length(xDuv), glm::length(xDp));
	}
}

// ############################################################################
// ★ 17. THE HEIGHT MAP IS BAKED, AND PARALLAX IS ACTUALLY ENABLED
// ############################################################################

// The generator computed a height field, differentiated it into a normal map and
// THREW IT AWAY. A normal map cannot move a silhouette: mortar beds stayed
// perfectly flat under every grazing camera, which is the strongest "this is a
// texture, not a surface" cue a facade gives off.
//
// ★ POM IS TWO THINGS AND EITHER ALONE IS INERT. Flux enables it iff a height
// texture is bound AND the height scale is > 0 (Flux_MaterialGPU's draw flags).
// A bound map with scale 0 is a texture nobody samples; a scale with no map is a
// number nobody reads. Both are silent, so both are asserted.
ZENITH_TEST(ZM_Gen, BuildingGen_HeightIsBakedAndParallaxEnabled)
{
	char acRef[512];
	ZENITH_ASSERT_TRUE(ZM_BuildingAssetPath(ZM_BUILDING_PLAYER_HOME,
		ZM_BuildingSurfaceAssetKind(ZM_BUILDING_SURFACE_WALL, ZM_BUILDING_SLOT_HEIGHT),
		acRef, sizeof(acRef)), "the PlayerHome wall height ref did not build");
	ZENITH_ASSERT_STREQ(acRef, "game:Buildings/PlayerHome/PlayerHome_wall_height.ztxtr",
		"the per-surface height-map ref scheme moved");

	// Three representative rows rather than all thirty: a 512^2 four-map set per
	// surface is ~1.6 M texels a building, and the roster-wide clauses that DO
	// need every row (RosterTotality, BuildDeterminism) already pay that once.
	const ZM_BUILDING_ID aeProbe[3] = {
		ZM_BUILDING_PLAYER_HOME, ZM_BUILDING_LAB, ZM_BUILDING_HOUSE_COTTAGE_WARM };
	for (u_int u = 0u; u < 3u; ++u)
	{
		const ZM_BUILDING_ID eId = aeProbe[u];
		const ZM_BuildingRecipe xR = ZM_ResolveBuildingRecipe(eId);
		for (u_int s = 0u; s < (u_int)ZM_BUILDING_SURFACE_COUNT; ++s)
		{
			const ZM_BUILDING_SURFACE eS = (ZM_BUILDING_SURFACE)s;
			const ZM_BuildingTextureSet xT = ZM_BuildBuildingSurfaceTextures(xR, eS);
			ZENITH_ASSERT_EQ(xT.m_xHeight.GetWidth(), ZM_BuildingSurfaceResolution(eS),
				"%s/%s's height map is not the surface resolution",
				ZM_GetBuildingName(eId), ZM_BuildingSurfaceName(eS));
			// ★ AND IT IS THE SAME FIELD THE NORMAL MAP CAME FROM. Two fields that
			// disagree about where the surface is is exactly the "dirt that does not
			// line up with the relief" failure -- and with POM on it becomes
			// geometry that does not line up with its own shading.
			ZENITH_ASSERT_TRUE(xT.m_xHeight.Equals(ZM_BuildBuildingSurfaceHeight(xR, eS)),
				"%s/%s bakes a height map that is not its own height field",
				ZM_GetBuildingName(eId), ZM_BuildingSurfaceName(eS));
		}
	}

	const ZM_BuildingSurfaceResponse xWall  = ZM_BuildingSurfaceMaterialResponse(ZM_BUILDING_SURFACE_WALL);
	const ZM_BuildingSurfaceResponse xRoof  = ZM_BuildingSurfaceMaterialResponse(ZM_BUILDING_SURFACE_ROOF);
	const ZM_BuildingSurfaceResponse xTrim  = ZM_BuildingSurfaceMaterialResponse(ZM_BUILDING_SURFACE_TRIM);
	const ZM_BuildingSurfaceResponse xGlass = ZM_BuildingSurfaceMaterialResponse(ZM_BUILDING_SURFACE_GLASS);
	ZENITH_ASSERT_GT(xWall.m_fHeightScale, 0.0f, "the wall's parallax is switched off");
	ZENITH_ASSERT_GT(xRoof.m_fHeightScale, 0.0f, "the roof's parallax is switched off");
	ZENITH_ASSERT_GT(xTrim.m_fHeightScale, 0.0f, "the trim's parallax is switched off");
	ZENITH_ASSERT_EQ_FLOAT(xGlass.m_fHeightScale, 0.0f, 0.0f,
		"glass is parallax-mapped -- a pane has no relief and the march would swim");
	ZENITH_ASSERT_LE(xWall.m_fHeightScale, 0.02f, "the wall's height scale will swim");
	ZENITH_ASSERT_LE(xRoof.m_fHeightScale, 0.02f, "the roof's height scale will swim");

	ZENITH_ASSERT_GE(xWall.m_fDetailTiling, 8.0f,
		"the wall's micro-detail repeats only %.1f times per tile -- at 170 px/m the "
		"surface goes soft at arm's length without it", xWall.m_fDetailTiling);
	ZENITH_ASSERT_LE(xWall.m_fDetailTiling, 12.0f, "the wall's micro-detail will alias");
	ZENITH_ASSERT_GE(xRoof.m_fDetailTiling, 8.0f, "the roof carries no micro-detail");
	ZENITH_ASSERT_EQ_FLOAT(xGlass.m_fDetailTiling, 0.0f, 0.0f,
		"glass carries a grain overlay, which frosts it");

	// The shared pair: ONE file for the whole family, reproducible, mean-neutral.
	char acDetAlb[512], acDetNrm[512];
	ZENITH_ASSERT_TRUE(ZM_BuildingSharedDetailPath(ZM_BUILDING_DETAIL_ALBEDO, acDetAlb, sizeof(acDetAlb)),
		"the shared detail-albedo ref did not build");
	ZENITH_ASSERT_STREQ(acDetAlb, "game:Buildings/Shared/MicroDetail_albedo.ztxtr",
		"the shared detail-albedo ref scheme moved");
	ZENITH_ASSERT_TRUE(ZM_BuildingSharedDetailPath(ZM_BUILDING_DETAIL_NORMAL, acDetNrm, sizeof(acDetNrm)),
		"the shared detail-normal ref did not build");
	ZENITH_ASSERT_FALSE(strcmp(acDetAlb, acDetNrm) == 0,
		"the two detail maps resolve to one file");

	const ZM_SynthDetailPair xPair = ZM_BuildBuildingMicroDetail();
	ZENITH_ASSERT_TRUE(xPair.NonEmpty(), "the shared micro-detail pair is missing a map");
	ZENITH_ASSERT_TRUE(xPair.Equals(ZM_BuildBuildingMicroDetail()),
		"the shared micro-detail pair is not reproducible -- every building would "
		"reference bytes that depend on which one happened to bake it");
	double dSum = 0.0;
	const u_int uDetRes = xPair.m_xAlbedo.GetWidth();
	for (u_int y = 0u; y < uDetRes; ++y)
	{
		for (u_int x = 0u; x < uDetRes; ++x)
		{
			dSum += xPair.m_xAlbedo.Get(y, x).x;
		}
	}
	const float fMean = (float)(dSum / (double)(uDetRes * uDetRes));
	// x2 around mid-grey (the Unity convention the shader follows): a mean away
	// from 0.5 tints every wall and roof in the game.
	ZENITH_ASSERT_EQ_FLOAT(fMean, 0.5f, 0.02f,
		"the detail albedo's mean is %.4f, not 0.5 -- the x2 overlay is not identity "
		"and every surface it touches shifts in value", fMean);
}

// ############################################################################
// ★★ 18. THE WINDOWS ARE REAL OPENINGS
// ############################################################################

// Version 3's wall was ONE SOLID BOX with the pane glued to its face -- which
// reads as a decal from any angle but head-on: no jamb, no head, no shadow, no
// depth. The facade slab is cut into panels around every opening now, the trim
// lines the cut with a reveal, and the glass sits at the back of it.
//
// ★ THE POCKET IS DELIBERATELY CLOSED AT THE BACK. There is no interior behind
// these walls (the interiors are separate scenes), so an opening is a POCKET a
// reveal deep, backed by the core slab and a dark room card. The claim is
// therefore not "a ray passes through the building" but "the outer part of the
// facade carries no masonry across the opening" -- which is what makes the jamb
// visible and the reveal foreshorten as a player walks past.
ZENITH_TEST(ZM_Gen, BuildingGen_WindowsAreRealOpenings)
{
	// Segment/triangle intersection (Moller-Trumbore): what does a sight line
	// through a window centre actually meet, and how far in?
	struct Ray
	{
		static bool Hits(const Zenith_Maths::Vector3& xOrigin, const Zenith_Maths::Vector3& xDir,
			const Zenith_Maths::Vector3& xA, const Zenith_Maths::Vector3& xB,
			const Zenith_Maths::Vector3& xC, float& fTOut)
		{
			const Zenith_Maths::Vector3 xE1 = xB - xA;
			const Zenith_Maths::Vector3 xE2 = xC - xA;
			const Zenith_Maths::Vector3 xP = glm::cross(xDir, xE2);
			const float fDet = glm::dot(xE1, xP);
			if (std::fabs(fDet) < 1.0e-9f) { return false; }
			const float fInv = 1.0f / fDet;
			const Zenith_Maths::Vector3 xT = xOrigin - xA;
			const float fU = glm::dot(xT, xP) * fInv;
			if (fU < 0.0f || fU > 1.0f) { return false; }
			const Zenith_Maths::Vector3 xQ = glm::cross(xT, xE1);
			const float fV = glm::dot(xDir, xQ) * fInv;
			if (fV < 0.0f || fU + fV > 1.0f) { return false; }
			fTOut = glm::dot(xE2, xQ) * fInv;
			return fTOut > 0.0f;
		}
	};

	// How deep the opening must be clear: the outer part of the pocket, stopping
	// short of the room card at the back of it.
	const float fClearDepth = fZM_BUILDING_REVEAL_DEPTH
		- fZM_BUILDING_ROOM_CARD_GAP - fZM_BUILDING_ROOM_CARD_THICK - 1.0e-3f;
	ZENITH_ASSERT_GT(fClearDepth, 0.10f,
		"the reveal is only %.3f m deep -- too shallow to read as an opening", fClearDepth);

	u_int uOpeningsProbed = 0u;
	for (u_int u = 0u; u < (u_int)ZM_BUILDING_COUNT; ++u)
	{
		const ZM_BUILDING_ID eId = (ZM_BUILDING_ID)u;
		const ZM_BuildingRecipe xR = ZM_ResolveBuildingRecipe(eId);
		const ZM_BuildingShellMetrics xM = ZM_ResolveBuildingShellMetrics(xR);

		ZM_GenMesh xWall, xGlass;
		ZM_BuildBuildingSurfaceMesh(xR, ZM_BUILDING_SURFACE_WALL, xWall);
		ZM_BuildBuildingSurfaceMesh(xR, ZM_BUILDING_SURFACE_GLASS, xGlass);

		// Probe the +Z facade -- the one with no door, so a column is missing only
		// when the window grid itself did not fit.
		for (u_int r = 0u; r < xM.m_uWindowRows; ++r)
		{
			for (u_int c = 0u; c < xM.m_uWindowCols; ++c)
			{
				const float fCx = xM.WindowCentreX(c);
				const float fCy = xM.WindowSillY(r) + fZM_BUILDING_WINDOW_HEIGHT * 0.5f;

				// Only probe where a pane was actually emitted, so this asks about
				// openings that EXIST rather than about a grid that was refused.
				bool bHasPane = false;
				for (u_int v = 0u; v < xGlass.GetNumVerts() && !bHasPane; ++v)
				{
					const Zenith_Maths::Vector3& xP = xGlass.m_xPositions.Get(v);
					bHasPane = xP.z > 0.0f
						&& std::fabs(xP.x - fCx) < fZM_BUILDING_WINDOW_WIDTH
						&& std::fabs(xP.y - fCy) < fZM_BUILDING_WINDOW_HEIGHT;
				}
				if (!bHasPane) { continue; }
				++uOpeningsProbed;

				const Zenith_Maths::Vector3 xOrigin(fCx, fCy, xM.m_fHalfD + 1.0f);
				const Zenith_Maths::Vector3 xDir(0.0f, 0.0f, -1.0f);
				for (u_int t = 0u; t < xWall.GetNumTris(); ++t)
				{
					const Zenith_Maths::Vector3& xA = xWall.m_xPositions.Get(xWall.m_xIndices.Get(t * 3u + 0u));
					const Zenith_Maths::Vector3& xB = xWall.m_xPositions.Get(xWall.m_xIndices.Get(t * 3u + 1u));
					const Zenith_Maths::Vector3& xC = xWall.m_xPositions.Get(xWall.m_xIndices.Get(t * 3u + 2u));
					float fT = 0.0f;
					if (!Ray::Hits(xOrigin, xDir, xA, xB, xC, fT)) { continue; }
					const float fDepth = fT - 1.0f;   // behind the facade plane
					ZENITH_ASSERT_FALSE(fDepth > -1.0e-3f && fDepth < fClearDepth,
						"%s: a sight line through the window at (%.2f, %.2f) meets wall "
						"triangle %u only %.4f m behind the facade plane. The opening is "
						"not a hole -- the pane is glued to a solid box and the reveal "
						"does not exist", ZM_GetBuildingName(eId), fCx, fCy, t, fDepth);
				}
			}
		}

		// ANTI-VACUITY: the SAME ray fired at solid masonry (on the axis, just
		// under the eave, above every opening) must meet the facade AT the plane.
		// Without this the loop above passes for a building with no wall at all.
		{
			const Zenith_Maths::Vector3 xOrigin(0.0f, xM.m_fWallTop - 0.10f, xM.m_fHalfD + 1.0f);
			const Zenith_Maths::Vector3 xDir(0.0f, 0.0f, -1.0f);
			bool bHitFacade = false;
			for (u_int t = 0u; t < xWall.GetNumTris() && !bHitFacade; ++t)
			{
				const Zenith_Maths::Vector3& xA = xWall.m_xPositions.Get(xWall.m_xIndices.Get(t * 3u + 0u));
				const Zenith_Maths::Vector3& xB = xWall.m_xPositions.Get(xWall.m_xIndices.Get(t * 3u + 1u));
				const Zenith_Maths::Vector3& xC = xWall.m_xPositions.Get(xWall.m_xIndices.Get(t * 3u + 2u));
				float fT = 0.0f;
				if (Ray::Hits(xOrigin, xDir, xA, xB, xC, fT) && (fT - 1.0f) < 0.05f)
				{
					bHitFacade = true;
				}
			}
			ZENITH_ASSERT_TRUE(bHitFacade,
				"%s: a ray at solid masonry under the eave met no wall at the facade "
				"plane, so the opening clause above proves nothing",
				ZM_GetBuildingName(eId));
		}
	}
	ZENITH_ASSERT_GT(uOpeningsProbed, 20u,
		"only %u window openings were probed across the whole roster -- the grid is "
		"being refused everywhere and this clause is nearly empty", uOpeningsProbed);

	// ★ AND THE PANE IS SET BACK. A pane on the wall plane is the decal this whole
	// change exists to remove. Measured on a building known to carry the full
	// window treatment rather than the windowless single-pane fallback (which
	// stands proud by design -- it has no pocket to sit in).
	{
		const ZM_BuildingRecipe xR = ZM_ResolveBuildingRecipe(ZM_BUILDING_PLAYER_HOME);
		const ZM_BuildingShellMetrics xM = ZM_ResolveBuildingShellMetrics(xR);
		ZM_GenMesh xGlass;
		ZM_BuildBuildingSurfaceMesh(xR, ZM_BUILDING_SURFACE_GLASS, xGlass);
		ZENITH_ASSERT_GT(xGlass.GetNumTris(), 12u,
			"the PlayerHome fell back to the single windowless pane, so the setback "
			"clause below would measure the wrong thing");
		for (u_int v = 0u; v < xGlass.GetNumVerts(); ++v)
		{
			const Zenith_Maths::Vector3& xP = xGlass.m_xPositions.Get(v);
			const float fDepth = (xP.z > 0.0f) ? (xM.m_fHalfD - xP.z) : (xP.z + xM.m_fHalfD);
			ZENITH_ASSERT_GT(fDepth, 0.04f,
				"the PlayerHome has a pane only %.4f m behind the facade plane -- it is "
				"applied to the wall rather than set into a reveal", fDepth);
		}
	}
}

// ############################################################################
// ★ 19. THE DOOR IS A RECESS, WITH A THRESHOLD AND A HANDLE
// ############################################################################

// Version 3 stood the leaf PROUD of a solid wall, because there was no opening
// to recess into -- and a leaf spanning inward from the wall plane put its
// outward face exactly coplanar with the masonry, which z-fought on both
// Dawnmere buildings. The doorway is a pocket now, so the leaf can sit where a
// door actually sits: at the back of the reveal, with the surround's ledge and
// the pocket's jambs casting onto it.
ZENITH_TEST(ZM_Gen, BuildingGen_DoorIsARecessedLeaf)
{
	const ZM_BUILDING_ID aeCases[3] = {
		ZM_BUILDING_PLAYER_HOME, ZM_BUILDING_LAB, ZM_BUILDING_HOUSE_COTTAGE_WARM };
	for (u_int i = 0u; i < 3u; ++i)
	{
		const ZM_BUILDING_ID eId = aeCases[i];
		const ZM_BuildingRecipe xR = ZM_ResolveBuildingRecipe(eId);
		const ZM_BuildingShellMetrics xM = ZM_ResolveBuildingShellMetrics(xR);
		ZM_GenMesh xTrim;
		ZM_BuildBuildingSurfaceMesh(xR, ZM_BUILDING_SURFACE_TRIM, xTrim);

		const float fWallPlane = -xM.m_fHalfD;
		const float fDoorHalf  = xM.m_fDoorWidth * 0.5f;

		bool bLeaf      = false;
		bool bThreshold = false;
		bool bHandle    = false;
		for (u_int v = 0u; v < xTrim.GetNumVerts(); ++v)
		{
			const Zenith_Maths::Vector3& xP = xTrim.m_xPositions.Get(v);
			// Inside the doorway's width, and inside the pocket's depth band.
			if (std::fabs(xP.x) > fDoorHalf + 1.0e-3f) { continue; }
			if (xP.z <= fWallPlane || xP.z >= fWallPlane + fZM_BUILDING_REVEAL_DEPTH + 1.0e-3f)
			{
				continue;
			}
			if (xP.y > fZM_BUILDING_THRESHOLD_H + 0.20f && xP.y < xM.m_fDoorHeight - 0.10f)
			{
				bLeaf = true;
			}
			if (xP.y > 1.0e-4f && xP.y <= fZM_BUILDING_THRESHOLD_H + 1.0e-3f) { bThreshold = true; }
			if (std::fabs(xP.y - fZM_BUILDING_HANDLE_Y) < fZM_BUILDING_HANDLE_LEN * 0.75f)
			{
				bHandle = true;
			}
		}
		ZENITH_ASSERT_TRUE(bLeaf,
			"%s emitted no door leaf inside its own doorway pocket", ZM_GetBuildingName(eId));
		ZENITH_ASSERT_TRUE(bThreshold,
			"%s has no threshold step -- the leaf stands on nothing and the pocket "
			"floor is open to the plinth", ZM_GetBuildingName(eId));
		ZENITH_ASSERT_TRUE(bHandle,
			"%s has no door handle at hand height", ZM_GetBuildingName(eId));

		// ★ RECESSED, NOT PROUD. Every leaf vertex sits BEHIND the masonry plane
		// (which is also what keeps it out of the coplanar test's way). Measured as
		// "no trim vertex within the door's width, above the threshold and below
		// the head, is outboard of the wall plane by more than the surround".
		for (u_int v = 0u; v < xTrim.GetNumVerts(); ++v)
		{
			const Zenith_Maths::Vector3& xP = xTrim.m_xPositions.Get(v);
			if (std::fabs(xP.x) > fDoorHalf - fZM_BUILDING_REVEAL_LINING - 1.0e-3f) { continue; }
			if (xP.y < fZM_BUILDING_THRESHOLD_H + 0.20f) { continue; }
			if (xP.y > xM.m_fDoorHeight - 0.10f) { continue; }
			// Inside the opening at hand height there is only the leaf and its
			// handle; the handle stands proud of the leaf but must still be behind
			// the wall plane, or a passer-by would clip it.
			ZENITH_ASSERT_GT(xP.z, fWallPlane + 1.0e-3f,
				"%s has door geometry at z=%.4f, on or outboard of the wall plane "
				"%.4f -- the leaf is applied to the facade rather than set into an "
				"opening, which is the z-fight this pocket exists to end",
				ZM_GetBuildingName(eId), xP.z, fWallPlane);
		}
	}
}

// ############################################################################
// ★ 20. THE ROOF IS COURSED SLATE, NOT TWO PLANES
// ############################################################################

// Two ruled planes have a straight silhouette and no shadow lines. Real slate is
// rows, each lying on the one below with its lower edge proud, so a grazing sun
// draws a line per course and the edge against the sky is stepped.
ZENITH_TEST(ZM_Gen, BuildingGen_RoofIsCoursed)
{
	const ZM_BUILDING_ID aePitched[2] = { eBG_GABLE, eBG_HIP };
	for (u_int i = 0u; i < 2u; ++i)
	{
		const ZM_BUILDING_ID eId = aePitched[i];
		const ZM_BuildingRecipe xR = ZM_ResolveBuildingRecipe(eId);
		const ZM_BuildingShellMetrics xM = ZM_ResolveBuildingShellMetrics(xR);
		ZM_GenMesh xRoof;
		ZM_BuildBuildingSurfaceMesh(xR, ZM_BUILDING_SURFACE_ROOF, xRoof);

		// Two pitches plus two gable ends is 8 triangles. A coursed roof is many
		// times that. The bound is the CLAIM ("this is not ruled planes") rather
		// than a golden count, which would red on any proportion change.
		ZENITH_ASSERT_GT(xRoof.GetNumTris(), 40u,
			"%s's roof is only %u triangles -- it is still ruled planes, not courses",
			ZM_GetBuildingName(eId), xRoof.GetNumTris());

		// A ridge run sits ABOVE the apex the pitches meet at.
		ZENITH_ASSERT_GT(ZM_GenMeshBoundsMax(xRoof).y, xM.m_fRidgeY + 1.0e-3f,
			"%s has no ridge run above its apex (roof top %.4f, ridge %.4f)",
			ZM_GetBuildingName(eId), ZM_GenMeshBoundsMax(xRoof).y, xM.m_fRidgeY);

		// ★ THE COURSES ARE PROUD AND JITTERED. Every course is offset along its
		// facet normal by a lip plus its own hash-derived jitter, so the eave row
		// is not a single ruled height -- which is what stops the rows reading as
		// machine-parallel.
		float fMinEave = 1.0e9f, fMaxEave = -1.0e9f;
		for (u_int v = 0u; v < xRoof.GetNumVerts(); ++v)
		{
			const Zenith_Maths::Vector3& xP = xRoof.m_xPositions.Get(v);
			if (xP.y > xM.m_fWallTop - 0.05f && xP.y < xM.m_fWallTop + 0.05f)
			{
				if (xP.y < fMinEave) { fMinEave = xP.y; }
				if (xP.y > fMaxEave) { fMaxEave = xP.y; }
			}
		}
		ZENITH_ASSERT_GT(fMaxEave - fMinEave, fZM_BUILDING_ROOF_LIP * 0.5f,
			"%s's eave course has no proud lip (height spread %.5f) -- there is no "
			"step for a grazing sun to draw", ZM_GetBuildingName(eId), fMaxEave - fMinEave);

		// Still a valid surface after all that (a coursed strip wound the wrong way
		// is a black band across the roof).
		const ZM_BuildingSurfaceValidation xV = ZM_ValidateBuildingSurfaceMesh(xRoof, 1.0e6f);
		ZENITH_ASSERT_TRUE(xV.m_bWindingOutward,
			"%s's coursed roof has an inward-wound triangle (first bad %u)",
			ZM_GetBuildingName(eId), xV.m_uFirstBadTriangle);
		ZENITH_ASSERT_TRUE(xV.m_bIndicesInRange,
			"%s's coursed roof has an out-of-range index", ZM_GetBuildingName(eId));
		ZENITH_ASSERT_TRUE(xV.m_bUVsFiniteAndBounded,
			"%s's coursed roof has a non-finite UV", ZM_GetBuildingName(eId));
	}

	// Barge boards close the verge of a gable: trim out at the gable end, above
	// the eave, which nothing else on the building emits there (the chimney sits
	// inboard at 0.45 x half-width).
	{
		const ZM_BuildingRecipe xR = ZM_ResolveBuildingRecipe(eBG_GABLE);
		const ZM_BuildingShellMetrics xM = ZM_ResolveBuildingShellMetrics(xR);
		ZM_GenMesh xTrim;
		ZM_BuildBuildingSurfaceMesh(xR, ZM_BUILDING_SURFACE_TRIM, xTrim);
		u_int uVerge = 0u;
		for (u_int v = 0u; v < xTrim.GetNumVerts(); ++v)
		{
			const Zenith_Maths::Vector3& xP = xTrim.m_xPositions.Get(v);
			if (xP.y > xM.m_fWallTop + 0.05f && std::fabs(xP.x) > xM.m_fExW - 0.10f)
			{
				++uVerge;
			}
		}
		ZENITH_ASSERT_GT(uVerge, 0u,
			"a gabled roof has no barge board along its verge -- the slate course "
			"ends are open to the sky");
	}

	// And the fascia still runs the whole eave, so the wall keeps its shadow line.
	{
		const ZM_BuildingRecipe xR = ZM_ResolveBuildingRecipe(eBG_HIP);
		const ZM_BuildingShellMetrics xM = ZM_ResolveBuildingShellMetrics(xR);
		ZM_GenMesh xTrim;
		ZM_BuildBuildingSurfaceMesh(xR, ZM_BUILDING_SURFACE_TRIM, xTrim);
		ZENITH_ASSERT_GE(ZM_GenMeshBoundsMax(xTrim).x, xM.m_fExW - 1.0e-3f,
			"the eave fascia no longer reaches the overhang, so the wall gets no eave "
			"shadow (%.3f vs %.3f)", ZM_GenMeshBoundsMax(xTrim).x, xM.m_fExW);
	}
}

// ############################################################################
// ★★ 21. THE WEATHERING READS AS GRAVITY AND TIME
// ############################################################################

// Version 3 weathered a wall with ONE term: cavity x a per-building constant.
// That is dirt in the mortar and nothing else -- no splash-back off the ground,
// no streaks under the sills, no runoff under the eave. And it was computed
// against the TILE's own v, which repeated one and a half times up a storey, so
// even a gradient would have anchored to nothing.
//
// ★ THE FIX IS THAT THE WALL'S TILE IS THE STOREY HEIGHT, so v IS
// height-within-the-storey. These clauses assert the three terms land on the
// rows they claim to, IN METRES -- the property that stops being true the moment
// somebody "simplifies" the tile back to a round number.
ZENITH_TEST(ZM_Gen, BuildingGen_WeatheringIsAnchoredToWorldHeight)
{
	const ZM_BuildingRecipe xR = ZM_ResolveBuildingRecipe(ZM_BUILDING_PLAYER_HOME);
	const float fTile = ZM_BuildingSurfaceTileMetresFor(xR, ZM_BUILDING_SURFACE_WALL);
	ZENITH_ASSERT_EQ_FLOAT(fTile, ZM_GetBuildingData(ZM_BUILDING_PLAYER_HOME).m_fStoreyHeight,
		fBG_EXACT,
		"the wall tile is %.3f m but the storey is %.3f m -- v is no longer "
		"height-within-the-storey and every row below anchors to nothing",
		fTile, ZM_GetBuildingData(ZM_BUILDING_PLAYER_HOME).m_fStoreyHeight);

	const u_int uSalt = xR.m_uSyntheticSeed;
	constexpr float fDRIPS = 4.0f;

	// (a) SPLASH-BACK is monotone DECREASING with height above the plinth, and is
	// gone one splash-height above it.
	{
		float fPrev = 2.0f;
		constexpr u_int uSTEPS = 24u;
		for (u_int i = 0u; i <= uSTEPS; ++i)
		{
			const float fY = fZM_BUILDING_PLINTH_HEIGHT
				+ fZM_BUILDING_SPLASH_HEIGHT * (float)i / (float)uSTEPS;
			const float fS = ZM_BuildingWallWeatherAt(0.5f, fY / fTile, fTile, uSalt, fDRIPS).m_fSplash;
			ZENITH_ASSERT_LE(fS, fPrev + 1.0e-4f,
				"splash-back rose with height at y=%.3f (%.4f after %.4f) -- rain does "
				"not bounce upward", fY, fS, fPrev);
			fPrev = fS;
		}
		ZENITH_ASSERT_GT(ZM_BuildingWallWeatherAt(0.5f,
			fZM_BUILDING_PLINTH_HEIGHT / fTile, fTile, uSalt, fDRIPS).m_fSplash, 0.9f,
			"there is no splash-back at the plinth top");
		const float fAbove = ZM_BuildingWallWeatherAt(0.5f,
			(fZM_BUILDING_PLINTH_HEIGHT + fZM_BUILDING_SPLASH_HEIGHT + 0.05f) / fTile,
			fTile, uSalt, fDRIPS).m_fSplash;
		ZENITH_ASSERT_LT(fAbove, 1.0e-3f,
			"splash-back is still %.4f a full %.2f m above the plinth -- that is a "
			"wash, not a splash line", fAbove, fZM_BUILDING_SPLASH_HEIGHT);
	}

	// (b) RUNOFF is the mirror image: nothing mid-wall, everything at the eave.
	{
		const float fMid = ZM_BuildingWallWeatherAt(0.5f, 0.5f, fTile, uSalt, fDRIPS).m_fRunoff;
		const float fTop = ZM_BuildingWallWeatherAt(0.5f, 1.0f - 1.0e-4f, fTile, uSalt, fDRIPS).m_fRunoff;
		ZENITH_ASSERT_LT(fMid, 1.0e-3f,
			"there is eave runoff halfway down the wall (%.4f)", fMid);
		ZENITH_ASSERT_GT(fTop, 0.9f,
			"there is no runoff staining under the eave (%.4f)", fTop);
	}

	// (c) DRIPS hang UNDER the sill row and nowhere above it -- and there really
	// are some, because a streak generator that produced nothing would satisfy
	// "none above the sill" perfectly.
	{
		const float fSillUnder = fZM_BUILDING_WINDOW_SILL_Y - fZM_BUILDING_FRAME_THICK
			- fZM_BUILDING_SILL_HEIGHT;
		float fMaxBelow = 0.0f;
		// ★ 256 SAMPLES ACROSS U, NOT 64. A drip stain is 2-4 cm wide, which on a
		// 3 m tile is 0.007-0.013 in u -- a 64-sample sweep steps 0.016 at a time
		// and can walk straight past every streak, measuring the gaps between them
		// and calling the term dead. The bake itself samples u 512 times.
		constexpr u_int uU = 256u;
		for (u_int i = 0u; i < uU; ++i)
		{
			const float fU = ((float)i + 0.5f) / (float)uU;
			for (u_int j = 1u; j < 8u; ++j)
			{
				const float fY = fSillUnder - fZM_BUILDING_DRIP_LENGTH * (float)j / 8.0f;
				const float fD = ZM_BuildingWallWeatherAt(fU, fY / fTile, fTile, uSalt, fDRIPS).m_fDrip;
				if (fD > fMaxBelow) { fMaxBelow = fD; }
			}
			ZENITH_ASSERT_EQ_FLOAT(ZM_BuildingWallWeatherAt(fU,
				(fSillUnder + 0.10f) / fTile, fTile, uSalt, fDRIPS).m_fDrip, 0.0f, 0.0f,
				"there is a drip streak 0.10 m ABOVE the sill underside at u=%.3f -- "
				"water does not run up a wall", fU);
		}
		ZENITH_ASSERT_GT(fMaxBelow, 0.2f,
			"the strongest drip streak under the whole sill row is %.4f -- the streaks "
			"are invisible and the term is decoration", fMaxBelow);
	}

	// ★ AND THE TERMS REACH THE ALBEDO. Everything above is a pure function
	// nobody is obliged to call; this is the clause that fails if the texture
	// builder stops reading it.
	{
		const ZM_BuildingTextureSet xT = ZM_BuildBuildingSurfaceTextures(xR, ZM_BUILDING_SURFACE_WALL);
		const u_int uRes = xT.m_xAlbedo.GetWidth();
		struct Band
		{
			static float Mean(const ZM_GenImage& xImg, float fV0, float fV1)
			{
				const u_int uRes2 = xImg.GetHeight();
				u_int uY0 = (u_int)(fV0 * (float)uRes2);
				u_int uY1 = (u_int)(fV1 * (float)uRes2);
				if (uY1 > uRes2) { uY1 = uRes2; }
				double dSum = 0.0; u_int uN = 0u;
				for (u_int y = uY0; y < uY1; ++y)
				{
					for (u_int x = 0u; x < xImg.GetWidth(); ++x)
					{
						const Zenith_Maths::Vector4 xC = xImg.Get(y, x);
						dSum += 0.2126 * xC.x + 0.7152 * xC.y + 0.0722 * xC.z;
						++uN;
					}
				}
				return uN > 0u ? (float)(dSum / (double)uN) : 0.0f;
			}
		};
		const float fSplashBand = Band::Mean(xT.m_xAlbedo, 0.0f,
			(fZM_BUILDING_PLINTH_HEIGHT + 0.15f) / fTile);
		const float fMidBand  = Band::Mean(xT.m_xAlbedo, 0.45f, 0.60f);
		const float fEaveBand = Band::Mean(xT.m_xAlbedo,
			1.0f - (fZM_BUILDING_RUNOFF_HEIGHT * 0.5f) / fTile, 1.0f);
		ZENITH_ASSERT_LT(fSplashBand, fMidBand - 0.01f,
			"the bottom of the wall (%.4f) is not darker than the middle (%.4f) -- the "
			"splash-back never reached the albedo", fSplashBand, fMidBand);
		ZENITH_ASSERT_LT(fEaveBand, fMidBand - 0.005f,
			"the top of the wall (%.4f) is not darker than the middle (%.4f) -- the "
			"eave runoff never reached the albedo", fEaveBand, fMidBand);
		ZENITH_ASSERT_EQ(uRes, ZM_BuildingSurfaceResolution(ZM_BUILDING_SURFACE_WALL),
			"the wall albedo is not the surface resolution");
	}
}

// ############################################################################
// ★ 22. EDGE WEAR LANDS ON PROUD ARRISES, AND POLISHES THEM
// ############################################################################

// The second thing weather does after filling cavities with dirt, and the one
// that reads as AGE rather than as grime: anything proud and sharp gets rubbed,
// loses its patina and polishes. The mask is derived from the height field's own
// gradient, so it lands exactly where the normal map says the edge is -- which
// is the whole reason it is not a second, independent noise field.
ZENITH_TEST(ZM_Gen, SynthEdgeWearLandsOnProudArrises)
{
	constexpr u_int uRES = 64u;
	// A single proud plateau: flat low, a sharp step up, flat high. The arris is
	// the step; both flats must come out clean.
	ZM_GenImage xHeight(uRES, uRES);
	for (u_int y = 0u; y < uRES; ++y)
	{
		for (u_int x = 0u; x < uRES; ++x)
		{
			const float fH = (x >= uRES / 4u && x < uRES / 2u) ? 0.85f : 0.15f;
			xHeight.Set(y, x, Zenith_Maths::Vector4(fH, fH, fH, 1.0f));
		}
	}
	const ZM_GenImage xWear = ZM_SynthEdgeWearFromHeight(xHeight);
	ZENITH_ASSERT_EQ(xWear.GetWidth(), uRES, "the wear mask is not the height field's size");
	ZENITH_ASSERT_TRUE(ZM_SynthEdgeWearFromHeight(xHeight).Equals(xWear),
		"ZM_SynthEdgeWearFromHeight is not a pure function of the height field");

	const u_int uRow      = uRES / 2u;
	const u_int uEdge     = uRES / 2u - 1u;   // the proud side of the upper step
	const u_int uPlateau  = uRES / 4u + 6u;   // middle of the plateau
	const u_int uGround   = 2u;               // middle of the low ground
	const float fAtEdge    = xWear.Get(uRow, uEdge).x;
	const float fOnPlateau = xWear.Get(uRow, uPlateau).x;
	const float fOnGround  = xWear.Get(uRow, uGround).x;
	ZENITH_ASSERT_GT(fAtEdge, 0.5f,
		"the arris of a 0.7-high step wears only %.4f -- the mask is not finding "
		"edges at all", fAtEdge);
	ZENITH_ASSERT_LT(fOnPlateau, 0.05f,
		"the middle of a flat plateau wears %.4f -- this is a height map, not an "
		"edge mask", fOnPlateau);
	ZENITH_ASSERT_LT(fOnGround, 0.05f, "flat low ground wears %.4f", fOnGround);

	// ★ THE PROUD SIDE, NOT THE CAVITY SIDE. A cavity edge is exactly as sharp;
	// what makes wear read as wear is that it only ever appears on the part that
	// sticks out. The recessed side collects dirt instead.
	const float fCavitySide = xWear.Get(uRow, uRES / 4u - 1u).x;
	ZENITH_ASSERT_LT(fCavitySide, fAtEdge * 0.6f,
		"the recessed side of the step wears %.4f against the proud side's %.4f -- "
		"dirt collects there, it does not polish", fCavitySide, fAtEdge);

	// It reaches the PBR set: a worn arris is SMOOTHER, a flat texel is untouched,
	// and zero strength is byte-identical to no wear at all (or every family that
	// does not ask for wear would silently re-bake).
	ZM_SynthPbrResponse xResp;
	xResp.m_fRoughness       = 0.85f;
	xResp.m_fCavityRoughness = 0.0f;   // isolate the wear term
	xResp.m_fCavityOcclusion = 0.5f;
	const ZM_SynthPbrSet xNoWear = ZM_SynthBuildPbrSet(xHeight, xResp);
	xResp.m_fEdgeWearStrength = 1.0f;
	const ZM_SynthPbrSet xWorn = ZM_SynthBuildPbrSet(xHeight, xResp);
	ZENITH_ASSERT_TRUE(xNoWear.NonEmpty() && xWorn.NonEmpty(), "a PBR set is incomplete");
	ZENITH_ASSERT_LT(xWorn.m_xRoughnessMetallic.Get(uRow, uEdge).y,
		xNoWear.m_xRoughnessMetallic.Get(uRow, uEdge).y - 0.05f,
		"a fully worn arris is no smoother than an unworn one");
	ZENITH_ASSERT_EQ_FLOAT(xWorn.m_xRoughnessMetallic.Get(uRow, uPlateau).y,
		xNoWear.m_xRoughnessMetallic.Get(uRow, uPlateau).y, 1.0e-3f,
		"switching edge wear on changed a FLAT texel, so it is not an edge term");
	xResp.m_fEdgeWearStrength = 0.0f;
	ZENITH_ASSERT_TRUE(ZM_SynthBuildPbrSet(xHeight, xResp).Equals(xNoWear),
		"edge-wear strength 0 is not identical to no edge wear");
}

// ############################################################################
// ★ 23. TWO WALLS OF ONE BUILDING ARE NOT THE SAME SQUARE
// ############################################################################

// A tiling texture repeats several times up a storey and across a facade.
// Without a low-frequency term the eye finds the grid instantly -- it is the
// most recognisable "this is a game texture" cue there is. The macro tint is a
// period-2/3 drift in value and hue: too slow to read as a pattern, fast enough
// that no two halves of the tile match.
ZENITH_TEST(ZM_Gen, BuildingGen_MacroTintBreaksTheRepeat)
{
	// ★ MEASURED AS VERTICAL STRIPS, ACROSS A MIDDLE BAND. Strips in U cancel
	// every height-anchored term exactly (splash-back, eave runoff and the sill
	// drips are all functions of V alone at a given U, and the band excludes the
	// drip rows outright), so what is left is the macro tint and the course
	// lattice's own residue. Quarters rather than halves because a wrapping
	// lattice's halves can be mirror images of each other.
	const ZM_BUILDING_ID aeProbe[4] = {
		ZM_BUILDING_HOUSE_COTTAGE_WARM, ZM_BUILDING_HOUSE_TOWNHOUSE_COOL,
		ZM_BUILDING_PLAYER_HOME, ZM_BUILDING_GYM_3 };
	u_int uVarying = 0u;
	float fBestSpread = 0.0f;
	// The first probe is also the subject of the per-face cast clause at the end.
	const ZM_BuildingRecipe xR = ZM_ResolveBuildingRecipe(aeProbe[0]);
	for (u_int b = 0u; b < 4u; ++b)
	{
		const ZM_BuildingRecipe xRb = ZM_ResolveBuildingRecipe(aeProbe[b]);
		const ZM_BuildingTextureSet xTb =
			ZM_BuildBuildingSurfaceTextures(xRb, ZM_BUILDING_SURFACE_WALL);
		const u_int uRes2 = xTb.m_xAlbedo.GetWidth();
		const u_int uQ = uRes2 / 4u;
		float fLo = 2.0f, fHi = -1.0f;
		for (u_int q = 0u; q < 4u; ++q)
		{
			double dSum = 0.0; u_int uN = 0u;
			for (u_int y = uRes2 * 4u / 10u; y < uRes2 * 6u / 10u; ++y)
			{
				for (u_int x = q * uQ; x < (q + 1u) * uQ; ++x)
				{
					const Zenith_Maths::Vector4 xC = xTb.m_xAlbedo.Get(y, x);
					dSum += 0.2126 * xC.x + 0.7152 * xC.y + 0.0722 * xC.z;
					++uN;
				}
			}
			const float fMeanQ = (float)(dSum / (double)uN);
			if (fMeanQ < fLo) { fLo = fMeanQ; }
			if (fMeanQ > fHi) { fHi = fMeanQ; }
		}
		const float fSpread = fHi - fLo;
		if (fSpread > 0.003f) { ++uVarying; }
		if (fSpread > fBestSpread) { fBestSpread = fSpread; }
		// ...and never SO much that one tile reads as two materials.
		ZENITH_ASSERT_LT(fSpread, 0.12f,
			"%s's wall tile varies by %.4f across its width -- that is a patch, not a "
			"drift", ZM_GetBuildingName(aeProbe[b]), fSpread);
	}
	ZENITH_ASSERT_GE(uVarying, 3u,
		"only %u of 4 walls show any low-frequency variation across the tile (best "
		"spread %.5f) -- the macro tint is not reaching the albedo and the repeat "
		"will read as a grid", uVarying, fBestSpread);

	// ★★ THE TWO GRIME COLOURS, AND WHY THE FIRST VERSION OF THIS CLAUSE WAS
	// DEAD. It counted texels with luma <= 0.35 that were warm-biased or
	// green-biased, and it found ZERO of each -- not because the grime was
	// missing, but because BOTH constants were unreachable by construction:
	//
	//   * MEASURED luma over a whole CottageWarm wall tile: min 0.403, p1 0.414,
	//     p10 0.452, median 0.570. Nothing is anywhere near 0.35, and nothing can
	//     be: the base is luma 0.605, the darkest grime is luma 0.14, the mix is
	//     at most (grime 0.80) x 0.55 = 0.44, so the floor is ~0.40. A threshold
	//     calibrated against the NOMINAL [0,1] range measures nothing.
	//   * "green > red" can NEVER hold on a warm wall. The base is (0.82, 0.56),
	//     and even fully grimed it lands at (0.44, 0.35) -- red still leads.
	//     Biological grime shows up as a RELATIVE shift toward green, never as an
	//     absolute one.
	//
	// So the claim is made three ways, none of them an absolute colour threshold:
	// the PALETTE must actually differ, the MIX FIELD must reach both ends, and
	// the ALBEDO must carry the difference where the mix says it should.

	// (1) The palette. Both numbers are measured off the shipped constants.
	const Zenith_Maths::Vector3 xDust = ZM_BuildingGrimeColour(false);
	const Zenith_Maths::Vector3 xBio  = ZM_BuildingGrimeColour(true);
	const float fDustGR = xDust.y - xDust.x;
	const float fBioGR  = xBio.y  - xBio.x;
	const float fDustLuma = 0.2126f * xDust.x + 0.7152f * xDust.y + 0.0722f * xDust.z;
	const float fBioLuma  = 0.2126f * xBio.x  + 0.7152f * xBio.y  + 0.0722f * xBio.z;
	ZENITH_ASSERT_GT(fBioGR - fDustGR, 0.10f,
		"the two grime colours differ by only %.4f in (G-R). They are mixed into a "
		"base colour at 55%% of a grime factor that peaks near 0.8, so anything "
		"under ~0.1 arrives as rounding: the shipped pair before this measured "
		"0.070 here and came out 0.006 apart on the wall", fBioGR - fDustGR);
	ZENITH_ASSERT_GT(fDustLuma - fBioLuma, 0.10f,
		"biological grime (luma %.4f) is not meaningfully darker than dust (%.4f) "
		"-- algae in a damp corner is nearly black", fBioLuma, fDustLuma);

	// (2) The mix field SPANS enough of [0,1] on every roster building for both
	// colours to be seen. This is the clause that would have caught the real
	// defect: at the first version's +/-0.4 swing three rows -- CottageWarm,
	// PlayerHome and GymIce, two of them the most-looked-at buildings in the game
	// -- were confined to 0.40-0.92, 0.05-0.61 and 0.28-0.80, so one of their two
	// grime colours was never seen. At +/-0.8 all thirty clear the bounds below.
	//
	// ★ THE BOUNDS ARE MEASURED, NOT ROUND. Full saturation is NOT asserted,
	// because it is not achievable: the mix is clamp(share + noise), the share is
	// drawn in [0.30, 0.70] and the period-4 lattice has only 16 nodes, so a row
	// drawn at 0.58 needs a node below 0.075 to reach pure dust and three rows do
	// not have one. MEASURED worst cases across the roster: lowest minimum 0.208
	// (CottageWarm), highest maximum 0.910 (PlayerHome), narrowest span 0.792.
	for (u_int u = 0u; u < (u_int)ZM_BUILDING_COUNT; ++u)
	{
		const ZM_BuildingRecipe xRb = ZM_ResolveBuildingRecipe((ZM_BUILDING_ID)u);
		const float fShare = ZM_BuildingAlbedoDrawsFor(xRb).m_fBioShare;
		float fLo = 2.0f, fHi = -1.0f;
		for (u_int y = 0u; y < 64u; ++y)
		{
			for (u_int x = 0u; x < 64u; ++x)
			{
				const float fM = ZM_BuildingWallGrimeBioMixAt(
					((float)x + 0.5f) / 64.0f, ((float)y + 0.5f) / 64.0f,
					xRb.m_uSyntheticSeed, fShare);
				ZENITH_ASSERT_TRUE(fM >= 0.0f && fM <= 1.0f,
					"%s's grime mix left [0,1] (%.4f)", ZM_GetBuildingName((ZM_BUILDING_ID)u), fM);
				if (fM < fLo) { fLo = fM; }
				if (fM > fHi) { fHi = fM; }
			}
		}
		ZENITH_ASSERT_LT(fLo, 0.35f,
			"%s's wall never gets near warm dust (lowest mix %.4f on a share of %.3f) "
			"-- one of its two grime colours is a sliver",
			ZM_GetBuildingName((ZM_BUILDING_ID)u), fLo, fShare);
		ZENITH_ASSERT_GT(fHi, 0.65f,
			"%s's wall never gets near biological grime (highest mix %.4f on a share "
			"of %.3f)", ZM_GetBuildingName((ZM_BUILDING_ID)u), fHi, fShare);
		ZENITH_ASSERT_GT(fHi - fLo, 0.60f,
			"%s's grime mix spans only %.4f of the range, so its wall carries one "
			"blended colour rather than two",
			ZM_GetBuildingName((ZM_BUILDING_ID)u), fHi - fLo);
	}

	// (3) The albedo CARRIES it. Measured on the roster row whose drawn share is
	// nearest 0.5 -- found by scanning rather than hardcoded, because which row
	// that is depends on the seed and would rot the first time the draw order
	// moved. A balanced share is what makes both sample sets large enough for the
	// mean to mean anything: on a row drawn at 0.32 the high-mix set is ~100
	// texels and the confounds (mortar joints, the macro tint) swamp it.
	ZM_BUILDING_ID eBalanced = ZM_BUILDING_HOUSE_COTTAGE_WARM;
	float fBestShare = 0.0f, fBestDist = 2.0f;
	for (u_int u = 0u; u < (u_int)ZM_BUILDING_COUNT; ++u)
	{
		const float fShare = ZM_BuildingAlbedoDrawsFor(
			ZM_ResolveBuildingRecipe((ZM_BUILDING_ID)u)).m_fBioShare;
		const float fDist = std::fabs(fShare - 0.5f);
		if (fDist < fBestDist) { fBestDist = fDist; fBestShare = fShare; eBalanced = (ZM_BUILDING_ID)u; }
	}
	{
		const ZM_BuildingRecipe xRb = ZM_ResolveBuildingRecipe(eBalanced);
		const ZM_BuildingTextureSet xTb =
			ZM_BuildBuildingSurfaceTextures(xRb, ZM_BUILDING_SURFACE_WALL);
		const ZM_GenImage xHeightB = ZM_BuildBuildingSurfaceHeight(xRb, ZM_BUILDING_SURFACE_WALL);
		const u_int uResB = xTb.m_xAlbedo.GetWidth();

		// TWO filters, both necessary. The MEAN height splits unit faces from
		// mortar joints: joints swing (G-R) by ~0.25 on a warm wall, three times
		// what the grime does, so including them measures the lattice. And the
		// SPLASH BAND (the plinth plus one splash height) is where the grime
		// factor is near 1, so the colour is applied at full strength -- measured
		// over the whole tile instead, most of the sample is clean wall and the
		// signal drops from +0.034 to +0.014.
		float fHeightSum = 0.0f;
		for (u_int y = 0u; y < uResB; ++y)
		{
			for (u_int x = 0u; x < uResB; ++x) { fHeightSum += xHeightB.Get(y, x).x; }
		}
		const float fHeightMean = fHeightSum / (float)(uResB * uResB);

		const float fTileB = ZM_BuildingSurfaceTileMetresFor(xRb, ZM_BUILDING_SURFACE_WALL);
		const float fBandV = (fZM_BUILDING_PLINTH_HEIGHT + fZM_BUILDING_SPLASH_HEIGHT) / fTileB;

		double dHi = 0.0, dLo = 0.0; u_int uHi = 0u, uLo = 0u;
		for (u_int y = 0u; y < uResB; ++y)
		{
			const float fV = ((float)y + 0.5f) / (float)uResB;
			if (fV > fBandV) { continue; }
			for (u_int x = 0u; x < uResB; ++x)
			{
				const float fU = ((float)x + 0.5f) / (float)uResB;
				if (xHeightB.Get(y, x).x <= fHeightMean) { continue; }   // a joint, not a face
				const Zenith_Maths::Vector4 xC = xTb.m_xAlbedo.Get(y, x);
				const float fGR = xC.y - xC.x;
				const float fMix = ZM_BuildingWallGrimeBioMixAt(fU, fV,
					xRb.m_uSyntheticSeed, fBestShare);
				if (fMix > 0.66f)      { dHi += fGR; ++uHi; }
				else if (fMix < 0.34f) { dLo += fGR; ++uLo; }
			}
		}
		ZENITH_ASSERT_GT(uHi, 200u,
			"%s's splash band has only %u biological-grime texels to average over",
			ZM_GetBuildingName(eBalanced), uHi);
		ZENITH_ASSERT_GT(uLo, 200u,
			"%s's splash band has only %u dust-grime texels to average over",
			ZM_GetBuildingName(eBalanced), uLo);
		const float fDelta = (float)(dHi / (double)uHi) - (float)(dLo / (double)uLo);
		// MEASURED +0.034 on the balanced row, +0.030..+0.042 across three
		// different rows and two resolutions, so this floor carries a 2x margin.
		// It was +0.006 before the palette was widened -- which is exactly what
		// the dead clause this replaced could not see.
		ZENITH_ASSERT_GT(fDelta, 0.015f,
			"on %s (share %.3f) the biologically-grimed texels are only %.4f greener "
			"than the dust-grimed ones -- the second grime colour is not reaching "
			"the albedo, whatever the palette says",
			ZM_GetBuildingName(eBalanced), fBestShare, fDelta);
	}

	// ★ AND THE PER-FACE CAST IS A VERTEX COLOUR, because ONE material serves
	// every face: "cool green-black on the sun-averted faces, warm dust on the lit
	// ones" cannot be a texel choice. The sun tracks the -Z/+X quarter in every
	// Zenithmon outdoor scene, so +Z and -X are the averted pair.
	ZM_GenMesh xWall;
	ZM_BuildBuildingSurfaceMesh(xR, ZM_BUILDING_SURFACE_WALL, xWall);
	ZENITH_ASSERT_EQ(xWall.m_xColors.GetSize(), xWall.GetNumVerts(),
		"the wall mesh has no per-vertex colour buffer, so the per-face grime cast "
		"cannot exist");
	bool bNeutral = false, bCast = false;
	for (u_int v = 0u; v < xWall.GetNumVerts(); ++v)
	{
		const Zenith_Maths::Vector3& xN = xWall.m_xNormals.Get(v);
		const Zenith_Maths::Vector4& xC = xWall.m_xColors.Get(v);
		if (xN.z < -0.5f && xC.x > 0.99f && xC.y > 0.99f && xC.z > 0.99f) { bNeutral = true; }
		if (xN.z >  0.5f && xC.y > xC.x + 0.01f) { bCast = true; }
	}
	ZENITH_ASSERT_TRUE(bNeutral,
		"the sun-facing (-Z) wall face carries a grime cast rather than the neutral "
		"one the texture already contains");
	ZENITH_ASSERT_TRUE(bCast,
		"the sun-averted (+Z) wall face carries no cool cast, so both faces read as "
		"having had the same weather");
}

// ############################################################################
// ★ 24. THE HOME'S WINDOWS ARE LIT, AND ONLY THE HOME'S
// ############################################################################
ZENITH_TEST(ZM_Gen, BuildingGen_OnlyTheHomeGlowsFromInside)
{
	const ZM_BuildingGlassGlow xHome =
		ZM_BuildingGlassGlowFor(ZM_ResolveBuildingRecipe(ZM_BUILDING_PLAYER_HOME));
	ZENITH_ASSERT_GT(xHome.m_fIntensity, 0.0f,
		"the player's own house is dark from outside -- nothing in the town reads as "
		"lived in");
	ZENITH_ASSERT_LT(xHome.m_fIntensity, 2.0f,
		"the home's windows glow at %.2f, which is a shop front, not a lamp behind a "
		"curtain", xHome.m_fIntensity);
	ZENITH_ASSERT_GT(xHome.m_xColour.x, xHome.m_xColour.z + 0.2f,
		"the home's window glow is not warm (%.2f, %.2f, %.2f) -- a domestic lamp is "
		"not daylight", xHome.m_xColour.x, xHome.m_xColour.y, xHome.m_xColour.z);

	u_int uGlowing = 0u;
	for (u_int u = 0u; u < (u_int)ZM_BUILDING_COUNT; ++u)
	{
		if (ZM_BuildingGlassGlowFor(ZM_ResolveBuildingRecipe((ZM_BUILDING_ID)u)).m_fIntensity > 0.0f)
		{
			++uGlowing;
		}
	}
	ZENITH_ASSERT_EQ(uGlowing, 1u,
		"%u buildings glow from inside -- emissive glass is a story beat for one "
		"house, not a material default", uGlowing);
}

// ############################################################################
// ★ 25. THE MASONRY LATTICE IS SIZED FROM THE TILE, NOT PINNED TO A COUNT
// ############################################################################

// The wall tile is the storey height now, and storeys differ (PlayerHome 3.0,
// ProfLab 3.5, and every free-standing row is jittered +/-3%). A FIXED course
// count would therefore stretch: the Lab's "250 mm" courses would be 292 mm and
// the two buildings standing 56 m apart would visibly disagree about what a
// block is. The lattice is derived from the tile instead.
ZENITH_TEST(ZM_Gen, BuildingGen_CourseGaugeSurvivesTheTile)
{
	// The gauge, in metres per course, across every building in the roster.
	for (u_int u = 0u; u < (u_int)ZM_BUILDING_COUNT; ++u)
	{
		const ZM_BUILDING_ID eId = (ZM_BUILDING_ID)u;
		const ZM_BuildingRecipe xR = ZM_ResolveBuildingRecipe(eId);
		const float fTile = ZM_BuildingSurfaceTileMetresFor(xR, ZM_BUILDING_SURFACE_WALL);
		const u_int uRows = ZM_BuildingWallCourseRows(fTile);
		const u_int uCols = ZM_BuildingWallCourseCols(fTile);
		ZENITH_ASSERT_GT(uRows, 1u, "%s's wall lattice has %u course rows -- a period-1 "
			"lattice is CONSTANT and renders as a flat surface", ZM_GetBuildingName(eId), uRows);
		ZENITH_ASSERT_GT(uCols, 1u, "%s's wall lattice has %u columns", ZM_GetBuildingName(eId), uCols);

		const float fCourse = fTile / (float)uRows;
		const float fUnit   = fTile / (float)uCols;
		ZENITH_ASSERT_GT(fCourse, 0.20f,
			"%s's courses are %.3f m -- too fine to read as masonry at 170 px/m",
			ZM_GetBuildingName(eId), fCourse);
		ZENITH_ASSERT_LT(fCourse, 0.32f,
			"%s's courses are %.3f m -- the block gauge stretched with the storey",
			ZM_GetBuildingName(eId), fCourse);
		ZENITH_ASSERT_GT(fUnit, 0.40f, "%s's units are %.3f m wide",
			ZM_GetBuildingName(eId), fUnit);
		ZENITH_ASSERT_LT(fUnit, 0.62f, "%s's units are %.3f m wide",
			ZM_GetBuildingName(eId), fUnit);
	}

	// ANTI-VACUITY: two buildings with DIFFERENT storeys must get different
	// lattices, or the derivation is a constant wearing a formula.
	const float fHome = ZM_BuildingSurfaceTileMetresFor(
		ZM_ResolveBuildingRecipe(ZM_BUILDING_PLAYER_HOME), ZM_BUILDING_SURFACE_WALL);
	const float fLab = ZM_BuildingSurfaceTileMetresFor(
		ZM_ResolveBuildingRecipe(ZM_BUILDING_LAB), ZM_BUILDING_SURFACE_WALL);
	ZENITH_ASSERT_NE(ZM_BuildingWallCourseRows(fHome), ZM_BuildingWallCourseRows(fLab),
		"the 3.0 m Home and the 3.5 m Lab were given the same number of courses, so "
		"the Lab's blocks are 17%% taller than the Home's");
}

// ############################################################################
// ★★ ZM-D-176: THE TWO INTERIOR ROOMS DO NOT READ AS THE SAME ROOM
// ############################################################################

// This is the MAGNITUDE half of the ZM-D-176 ruling ("the player's bedroom must
// stop reading as the same greybox room as ProfLab"), and it lives in a unit
// rather than in ZM_InteriorTint_Test because it is a property of compiled
// constants. The automated test kept the half only a live scene can see -- that
// each scene loaded ITS OWN room model -- and gave this half up, because the
// rooms' colour now comes from textures rather than from a material base-colour
// factor it could sample.
//
// ★ IT CHECKS EVERY SURFACE, NOT ONE SAMPLED BLOCK. The old scan measured the
// first blockout it happened to resolve. Here all four surface classes are
// asserted, which is what actually reaches the framebuffer: a floor, walls, a
// ceiling and trim all moving together is why the pixel gap can clear its floor
// where a single hue nudge on shared grey blocks reached only 0.121.
ZENITH_TEST(ZM_Gen, InteriorRoomsAreVisuallySeparated)
{
	u_int uSurfacesChecked = 0u;
	for (u_int s = 0u; s < (u_int)ZM_INTERIOR_SURFACE_COUNT; ++s)
	{
		const ZM_INTERIOR_SURFACE eS = (ZM_INTERIOR_SURFACE)s;
		const ZM_InteriorSurfaceLook xHome =
			ZM_GetInteriorSurfaceLook(ZM_INTERIOR_ROOM_PLAYER_HOME, eS);
		const ZM_InteriorSurfaceLook xLab =
			ZM_GetInteriorSurfaceLook(ZM_INTERIOR_ROOM_PROF_LAB, eS);

		// ★ THE DIRECTION, WHICH IS WHAT THE PIXEL TEST ACTUALLY MEASURES. It
		// computes a red/blue RATIO per room and asserts a GAP between them. A
		// separation clause alone would be satisfied by two rooms that differ
		// strongly but in the same hue direction, and that gap would stay at zero.
		ZENITH_ASSERT_GT(xHome.m_xBaseColour.x, xHome.m_xBaseColour.z,
			"the PlayerHome %s is not WARM (R %.3f <= B %.3f) -- the bedroom half of "
			"the ZM-D-176 ruling is that it reads warm against the lab's cool",
			ZM_InteriorSurfaceName(eS), xHome.m_xBaseColour.x, xHome.m_xBaseColour.z);
		ZENITH_ASSERT_GT(xLab.m_xBaseColour.z, xLab.m_xBaseColour.x,
			"the ProfLab %s is not COOL (B %.3f <= R %.3f)",
			ZM_InteriorSurfaceName(eS), xLab.m_xBaseColour.z, xLab.m_xBaseColour.x);

		// ...and the magnitude, per surface, measured with the SAME function the
		// human palette uses so the floor means there what it means here.
		const float fSep = ZM_HumanPaletteSeparation(
			Zenith_Maths::Vector4(xHome.m_xBaseColour, 1.0f),
			Zenith_Maths::Vector4(xLab.m_xBaseColour, 1.0f));
		ZENITH_ASSERT_GE(fSep, fZM_HUMAN_PALETTE_MIN_SEPARATION,
			"the two rooms' %s colours sit only %.5f apart, under the %.5f margin -- "
			"PlayerHome and ProfLab still read as the same room on that surface",
			ZM_InteriorSurfaceName(eS), fSep, fZM_HUMAN_PALETTE_MIN_SEPARATION);
		++uSurfacesChecked;
	}

	ZENITH_ASSERT_EQ(uSurfacesChecked, (u_int)ZM_INTERIOR_SURFACE_COUNT,
		"only %u of %u surface classes were compared", uSurfacesChecked,
		(u_int)ZM_INTERIOR_SURFACE_COUNT);

	// The shell spec must come FROM the placement headers, so a change to either
	// interior reds here rather than leaving the drawn room describing one that no
	// longer exists. A visible wall the player walks through is the failure.
	const ZM_InteriorRoomSpec xHomeSpec =
		ZM_GetInteriorRoomSpec(ZM_INTERIOR_ROOM_PLAYER_HOME);
	const ZM_InteriorRoomSpec xLabSpec =
		ZM_GetInteriorRoomSpec(ZM_INTERIOR_ROOM_PROF_LAB);
	ZENITH_ASSERT_EQ_FLOAT(xHomeSpec.m_fHalfWidth, fZM_PLAYERHOME_HALF_WIDTH, 0.0f,
		"the interior shell spec no longer reads PlayerHome's authored half-width");
	ZENITH_ASSERT_EQ_FLOAT(xHomeSpec.m_fWallHeight, fZM_PLAYERHOME_WALL_HEIGHT, 0.0f,
		"the interior shell spec no longer reads PlayerHome's authored wall height");
	ZENITH_ASSERT_EQ_FLOAT(xLabSpec.m_fHalfWidth, fZM_PROFLAB_HALF_WIDTH, 0.0f,
		"the interior shell spec no longer reads ProfLab's authored half-width");
	ZENITH_ASSERT_EQ_FLOAT(xLabSpec.m_fWallHeight, fZM_PROFLAB_WALL_HEIGHT, 0.0f,
		"the interior shell spec no longer reads ProfLab's authored wall height");
	ZENITH_ASSERT_NE((u_int)(xHomeSpec.m_fHalfWidth * 100.0f),
		(u_int)(xLabSpec.m_fHalfWidth * 100.0f),
		"the two rooms are the same size, so a registration pointed at the wrong "
		"scene would agree on geometry as well as on entity names");
}

// ############################################################################
// The interior shell is structurally sound and fits its blockouts
// ############################################################################
ZENITH_TEST(ZM_Gen, InteriorShellsBuildAndFitTheirBlockouts)
{
	for (u_int r = 0u; r < (u_int)ZM_INTERIOR_ROOM_COUNT; ++r)
	{
		const ZM_INTERIOR_ROOM eRoom = (ZM_INTERIOR_ROOM)r;
		const ZM_InteriorRoomSpec xSpec = ZM_GetInteriorRoomSpec(eRoom);

		ZM_Interior xA, xB;
		ZM_BuildInterior(eRoom, xA);
		ZM_BuildInterior(eRoom, xB);
		ZENITH_ASSERT_TRUE(ZM_InteriorBuildEqual(xA, xB),
			"%s did not rebuild byte-identically", ZM_InteriorRoomName(eRoom));
		ZENITH_ASSERT_EQ(ZM_InteriorContentHash(xA), ZM_InteriorContentHash(xB),
			"%s's content hash is not reproducible", ZM_InteriorRoomName(eRoom));

		const ZM_InteriorValidation xV = ZM_ValidateInterior(xA);
		for (u_int s = 0u; s < (u_int)ZM_INTERIOR_SURFACE_COUNT; ++s)
		{
			const char* szS = ZM_InteriorSurfaceName((ZM_INTERIOR_SURFACE)s);
			ZENITH_ASSERT_TRUE(xV.m_axSurface[s].m_bWindingOutward,
				"%s/%s has an inward-wound triangle (first bad %u)",
				ZM_InteriorRoomName(eRoom), szS, xV.m_axSurface[s].m_uFirstBadTriangle);
			ZENITH_ASSERT_TRUE(xV.m_axSurface[s].m_bUVsFiniteAndBounded,
				"%s/%s has a non-finite or runaway UV (max |uv| = %.3f)",
				ZM_InteriorRoomName(eRoom), szS, xV.m_axSurface[s].m_fMaxAbsUV);
			ZENITH_ASSERT_TRUE(xV.m_abTexturesNonEmpty[s],
				"%s/%s is missing one of its four PBR maps",
				ZM_InteriorRoomName(eRoom), szS);
			ZENITH_ASSERT_GT(xA.m_axMesh[s].GetNumTris(), 0u,
				"%s/%s is empty -- a degenerate .zmesh fails the whole model load",
				ZM_InteriorRoomName(eRoom), szS);
		}
		ZENITH_ASSERT_TRUE(xV.m_bAllValid, "%s failed the interior validation contract",
			ZM_InteriorRoomName(eRoom));

		// ★★ THE SHELL MUST NOT REACH PAST THE WALLS THE PLAYER STOPS AGAINST.
		// The seven blockouts are centred on the wall centrelines and are what
		// collide; the shell is drawn against their INNER faces. A surface past a
		// centreline means the player stops short of a wall they can see, or walks
		// into one they cannot.
		for (u_int s = 0u; s < (u_int)ZM_INTERIOR_SURFACE_COUNT; ++s)
		{
			const char* szS = ZM_InteriorSurfaceName((ZM_INTERIOR_SURFACE)s);
			const Zenith_Maths::Vector3 xMin = ZM_GenMeshBoundsMin(xA.m_axMesh[s]);
			const Zenith_Maths::Vector3 xMax = ZM_GenMeshBoundsMax(xA.m_axMesh[s]);
			ZENITH_ASSERT_LE(xMax.x, xSpec.m_fHalfWidth + 1.0e-4f,
				"%s/%s reaches x=%.4f, past the wall centreline at %.4f",
				ZM_InteriorRoomName(eRoom), szS, xMax.x, xSpec.m_fHalfWidth);
			ZENITH_ASSERT_GE(xMin.x, -xSpec.m_fHalfWidth - 1.0e-4f,
				"%s/%s reaches x=%.4f, past the wall centreline at -%.4f",
				ZM_InteriorRoomName(eRoom), szS, xMin.x, xSpec.m_fHalfWidth);
			ZENITH_ASSERT_LE(xMax.z, xSpec.m_fHalfDepth + 1.0e-4f,
				"%s/%s reaches z=%.4f, past the wall centreline at %.4f",
				ZM_InteriorRoomName(eRoom), szS, xMax.z, xSpec.m_fHalfDepth);
		}

		// The floor's walking surface is exactly y=0 -- the plane every spawn marker
		// and authored body in these scenes stands on.
		const Zenith_Maths::Vector3 xFloorMax =
			ZM_GenMeshBoundsMax(xA.m_axMesh[ZM_INTERIOR_SURFACE_FLOOR]);
		ZENITH_ASSERT_EQ_FLOAT(xFloorMax.y, 0.0f, 1.0e-4f,
			"%s's floor top is at y=%.4f, not the y=0 plane every spawn in this scene "
			"stands on", ZM_InteriorRoomName(eRoom), xFloorMax.y);

		// The entrance is a REAL GAP, not a painted door: nothing may be emitted
		// across the aperture below its head, or the player walks into an invisible
		// wall on the way in.
		const float fInnerZ = xSpec.InnerHalfDepth();
		for (u_int v = 0u; v < xA.m_axMesh[ZM_INTERIOR_SURFACE_WALL].GetNumVerts(); ++v)
		{
			const Zenith_Maths::Vector3& xP =
				xA.m_axMesh[ZM_INTERIOR_SURFACE_WALL].m_xPositions.Get(v);
			const bool bOnEntranceWall = xP.z > fInnerZ - 1.0e-3f;
			const bool bBelowHead      = xP.y < xSpec.m_fApertureHeight - 1.0e-3f;
			if (bOnEntranceWall && bBelowHead)
			{
				// ★ ONE CHAMFER OF SLACK, AND NO MORE. Every static box is bevelled
				// now (fZM_STATIC_BOX_CHAMFER), so the panel flanking the aperture
				// has its inset face exactly one chamfer inside the opening. That is
				// 8 mm of geometry a 0.4 m-radius capsule cannot notice; anything
				// beyond it is a panel genuinely across the doorway.
				ZENITH_ASSERT_GE(std::fabs(xP.x),
					xSpec.m_fApertureHalfW - fZM_STATIC_BOX_CHAMFER - 1.0e-3f,
					"%s has wall geometry at x=%.3f, y=%.3f on the entrance wall -- "
					"inside the +/-%.2f doorway the player walks through",
					ZM_InteriorRoomName(eRoom), xP.x, xP.y, xSpec.m_fApertureHalfW);
			}
		}
	}
}

// ############################################################################
// The furniture and lights stand where they claim to
// ############################################################################

// ★ THE WALK DRIVER HAS NO OBSTACLE AVOIDANCE (map playbook 3.4). Both rooms are
// entered and left through the +Z aperture on the room's axis, and a prop on that
// line would wedge a traversal test into its frame cap with a failure naming a
// DISTANCE rather than the blocker -- a failure shape that has cost this repo
// whole diagnostic cycles. The furniture is visual-only today, so the corridor
// clause is currently about not walking THROUGH a bed; it becomes a hard safety
// property the moment anything here grows a collider, which is why it is asserted
// now rather than when that happens.
ZENITH_TEST(ZM_Interaction, InteriorPropsClearTheEntranceCorridor)
{
	u_int uPropsChecked = 0u;
	for (u_int r = 0u; r < (u_int)ZM_INTERIOR_ROOM_COUNT; ++r)
	{
		const ZM_INTERIOR_ROOM eRoom = (ZM_INTERIOR_ROOM)r;
		const ZM_InteriorRoomSpec xSpec = ZM_GetInteriorRoomSpec(eRoom);
		const u_int uCount = ZM_GetInteriorPropCount(eRoom);
		ZENITH_ASSERT_GT(uCount, 0u, "%s has no furniture at all",
			ZM_InteriorRoomName(eRoom));

		for (u_int p = 0u; p < uCount; ++p)
		{
			const ZM_InteriorProp& xProp = ZM_GetInteriorProp(eRoom, p);
			++uPropsChecked;

			ZENITH_ASSERT_GE(std::fabs(xProp.m_fX), fZM_INTERIOR_CORRIDOR_HALF_WIDTH,
				"'%s' stands at x=%.2f, inside the +/-%.2f entrance corridor of %s",
				xProp.m_szEntityName, xProp.m_fX, fZM_INTERIOR_CORRIDOR_HALF_WIDTH,
				ZM_InteriorRoomName(eRoom));

			// ...and inside the room at all, with its footprint clear of the walls.
			ZENITH_ASSERT_LE(std::fabs(xProp.m_fX),
				xSpec.InnerHalfWidth() - fZM_INTERIOR_PROP_RADIUS * 0.5f,
				"'%s' at x=%.2f is inside or through the wall of %s (inner half-width "
				"%.2f)", xProp.m_szEntityName, xProp.m_fX, ZM_InteriorRoomName(eRoom),
				xSpec.InnerHalfWidth());
			ZENITH_ASSERT_LE(std::fabs(xProp.m_fZ),
				xSpec.InnerHalfDepth() - fZM_INTERIOR_PROP_RADIUS * 0.5f,
				"'%s' at z=%.2f is inside or through the wall of %s (inner half-depth "
				"%.2f)", xProp.m_szEntityName, xProp.m_fZ, ZM_InteriorRoomName(eRoom),
				xSpec.InnerHalfDepth());

			// The rotation is a FROZEN UNIT quaternion, never computed. A non-unit one
			// would be normalised somewhere downstream and the committed scene bytes
			// would then depend on where -- the ZM-D-183 failure shape exactly.
			const float fLenSq = xProp.m_fQuatW * xProp.m_fQuatW
				+ xProp.m_fQuatY * xProp.m_fQuatY;
			ZENITH_ASSERT_EQ_FLOAT(fLenSq, 1.0f, 1.0e-6f,
				"'%s' carries a non-unit frozen quaternion (w=%.8f y=%.8f, |q|^2=%.8f)",
				xProp.m_szEntityName, xProp.m_fQuatW, xProp.m_fQuatY, fLenSq);

			// The name maps back to this prop, which is how the runtime component
			// resolves it. A typo here is a piece of furniture that shows nothing,
			// in a scene that still loads and still passes every other clause.
			ZENITH_ASSERT_EQ((u_int)ZM_PropForInteriorPropEntity(xProp.m_szEntityName),
				(u_int)xProp.m_eProp,
				"'%s' does not map back to its own prop id", xProp.m_szEntityName);
		}

		// Lights: inside the room, above the floor, below the ceiling, and lit.
		const u_int uLights = ZM_GetInteriorLightCount(eRoom);
		ZENITH_ASSERT_GT(uLights, 0u,
			"%s has no lights. Both interiors were lit by the global ambient term "
			"alone before this -- a constant, with no direction, no falloff, nothing "
			"for SSAO to darken and nothing for CSM to cast",
			ZM_InteriorRoomName(eRoom));
		for (u_int l = 0u; l < uLights; ++l)
		{
			const ZM_InteriorLight& xLight = ZM_GetInteriorLight(eRoom, l);
			ZENITH_ASSERT_GT(xLight.m_fLumens, 0.0f, "'%s' emits nothing",
				xLight.m_szEntityName);
			ZENITH_ASSERT_GT(xLight.m_fRange, 0.0f, "'%s' has no range",
				xLight.m_szEntityName);
			ZENITH_ASSERT_GT(xLight.m_fY, 0.0f, "'%s' is at or below the floor",
				xLight.m_szEntityName);
			ZENITH_ASSERT_LT(xLight.m_fY, xSpec.m_fWallHeight,
				"'%s' is at y=%.2f, at or above the %.2f m ceiling of %s",
				xLight.m_szEntityName, xLight.m_fY, xSpec.m_fWallHeight,
				ZM_InteriorRoomName(eRoom));
			ZENITH_ASSERT_LE(std::fabs(xLight.m_fX), xSpec.InnerHalfWidth(),
				"'%s' is outside the room in X", xLight.m_szEntityName);
			ZENITH_ASSERT_LE(std::fabs(xLight.m_fZ), xSpec.InnerHalfDepth(),
				"'%s' is outside the room in Z", xLight.m_szEntityName);
		}
	}

	ZENITH_ASSERT_GT(uPropsChecked, 4u,
		"only %u props were checked across both rooms -- the dressing tables are "
		"empty or unreachable and this clause is asserting nothing", uPropsChecked);

	// The two shells map to DISTINCT rooms, which is how the runtime component
	// picks a model. Both resolving to one room would render one interior twice.
	ZENITH_ASSERT_NE((u_int)ZM_RoomForShellEntity(szZM_PLAYERHOME_SHELL_ENTITY_NAME),
		(u_int)ZM_RoomForShellEntity(szZM_PROFLAB_SHELL_ENTITY_NAME),
		"both interior shell entities resolve to the same room");
	ZENITH_ASSERT_FALSE(ZM_IsInteriorShellEntity("PlayerHomeFloor"),
		"a blockout name was accepted as a shell entity");
	ZENITH_ASSERT_FALSE(ZM_IsInteriorShellEntity(nullptr),
		"a null name was accepted as a shell entity");
	ZENITH_ASSERT_EQ((u_int)ZM_PropForInteriorPropEntity("NotAProp"),
		(u_int)ZM_PROP_NONE, "an unknown name resolved to a prop");
}

// ############################################################################
// ★★ THE SHARED PBR MAP SET
// ############################################################################

// ZM_SynthBuildPbrSet is the one place normal / roughness-metallic / occlusion
// are derived, for buildings, interiors, props, humans and creatures alike. It
// therefore gets the clauses each of those would otherwise need five copies of --
// and above all the two that catch a map which LOADS, BINDS, SAMPLES AND RENDERS
// EXACTLY LIKE HAVING NO MAP AT ALL: a flat normal and a uniform AO.
ZENITH_TEST(ZM_Gen, SynthPbrSetIsDerivedFromOneHeightField)
{
	// ★ 128, NOT 64, AND THE REASON IS A PROPERTY OF THE FUNCTION UNDER TEST.
	// ZM_SynthNormalFromHeight scales its central differences by 2.2 * width / 1024,
	// so the SAME height field yields a weaker normal at a lower resolution -- at 64
	// the scale is 0.1375 and even a hard 0.12 step encodes only 0.008 away from
	// flat, which reads as no normal map at all. A probe below the resolutions the
	// real generators use (128-256) tests the encoding's noise floor rather than the
	// builder.
	constexpr u_int uRES = 128u;

	// A height field with BOTH kinds of variation, because the two derived families
	// need different things and a field with only one of them tests only half.
	//
	// ★ THE FINE CHECKER IS NOT DECORATION -- IT IS WHAT MAKES THE NORMAL CLAUSE
	// MEAN ANYTHING. The first version of this fixture was a smooth diagonal ramp
	// spanning 0.75 of the height range, and it produced a COMPLETELY FLAT normal
	// map. ZM_SynthNormalFromHeight takes central differences scaled by
	// 2.2 * width / 1024, so at 64px a ramp of 0.0078 per texel yields a gradient
	// of ~0.001 -- below the encoding's resolution. RANGE is not RELIEF: a normal
	// map answers to the LOCAL gradient, and a field can span the whole scale and
	// still be flat everywhere. The real generators' fields are noise at period 16
	// over 256 texels, which has exactly the local contrast this checker stands in
	// for.
	//
	// The ramp and step are still here because AO and roughness answer to the
	// LEVEL, not the gradient, and the directional clauses below need the corners
	// to differ. The checker's phase is identical at both corners (both even), so
	// it cannot flip that comparison.
	ZM_GenImage xHeight(uRES, uRES);
	for (u_int y = 0u; y < uRES; ++y)
	{
		for (u_int x = 0u; x < uRES; ++x)
		{
			const float fRamp    = (float)(x + y) / (float)(2u * uRES);
			const float fStep    = (x < uRES / 2u) ? 0.0f : 0.20f;
			const float fChecker = (((x / 2u) + (y / 2u)) % 2u) ? 0.25f : 0.0f;
			const float fH = fRamp * 0.35f + fStep + fChecker;
			xHeight.Set(y, x, Zenith_Maths::Vector4(fH, fH, fH, 1.0f));
		}
	}

	ZM_SynthPbrResponse xResponse;
	xResponse.m_fRoughness      = 0.60f;
	xResponse.m_fMetallic       = 0.0f;
	xResponse.m_fNormalStrength = 1.0f;

	const ZM_SynthPbrSet xA = ZM_SynthBuildPbrSet(xHeight, xResponse);
	const ZM_SynthPbrSet xB = ZM_SynthBuildPbrSet(xHeight, xResponse);

	ZENITH_ASSERT_TRUE(xA.NonEmpty(), "the PBR set is missing a map");
	ZENITH_ASSERT_TRUE(xA.Equals(xB),
		"ZM_SynthBuildPbrSet is not a pure function of (height, response)");
	ZENITH_ASSERT_EQ(xA.m_xNormal.GetWidth(), uRES,
		"the normal map is not the height field's size");
	ZENITH_ASSERT_EQ(xA.m_xRoughnessMetallic.GetWidth(), uRES,
		"the RM map is not the height field's size");
	ZENITH_ASSERT_EQ(xA.m_xOcclusion.GetWidth(), uRES,
		"the AO map is not the height field's size");

	// ★ THE NORMAL IS NOT FLAT. A constant height field yields a uniform
	// (0.5, 0.5, 1) normal, which binds and samples exactly like no normal map --
	// so nothing downstream can tell the difference and this is the only place it
	// can be caught.
	u_int uNonFlat = 0u;
	for (u_int y = 0u; y < uRES; ++y)
	{
		for (u_int x = 0u; x < uRES; ++x)
		{
			const Zenith_Maths::Vector4 xN = xA.m_xNormal.Get(y, x);
			if (std::fabs(xN.x - 0.5f) > 0.02f || std::fabs(xN.y - 0.5f) > 0.02f)
			{
				++uNonFlat;
			}
		}
	}
	ZENITH_ASSERT_GT(uNonFlat, (uRES * uRES) / 4u,
		"only %u of %u normal texels differ from flat on a ramped height field",
		uNonFlat, uRES * uRES);

	// ★ AND THE AO IS NOT UNIFORM, for the same reason: a constant AO map is a
	// constant multiply the engine's SSAO already applies better.
	float fMinAO = 2.0f, fMaxAO = -1.0f;
	float fMinRough = 2.0f, fMaxRough = -1.0f;
	for (u_int y = 0u; y < uRES; ++y)
	{
		for (u_int x = 0u; x < uRES; ++x)
		{
			const float fAO = xA.m_xOcclusion.Get(y, x).x;
			if (fAO < fMinAO) { fMinAO = fAO; }
			if (fAO > fMaxAO) { fMaxAO = fAO; }
			const Zenith_Maths::Vector4 xRM = xA.m_xRoughnessMetallic.Get(y, x);
			if (xRM.y < fMinRough) { fMinRough = xRM.y; }
			if (xRM.y > fMaxRough) { fMaxRough = xRM.y; }
			// glTF packing: metallic is B, and it is whatever the response said.
			ZENITH_ASSERT_EQ_FLOAT(xRM.z, xResponse.m_fMetallic, 1.0e-4f,
				"the RM map's B channel is not the response's metallic");
		}
	}
	ZENITH_ASSERT_GT(fMaxAO - fMinAO, 0.05f,
		"the AO map spans only %.4f on a ramped height field -- it is uniform",
		fMaxAO - fMinAO);
	ZENITH_ASSERT_GT(fMaxRough - fMinRough, 0.01f,
		"roughness does not vary with cavity (span %.4f)", fMaxRough - fMinRough);

	// A cavity is ROUGHER and DARKER than a proud face. Directionality, not just
	// variance -- inverting the sign would still pass a span check.
	const Zenith_Maths::Vector4 xLowRM  = xA.m_xRoughnessMetallic.Get(0u, 0u);
	const Zenith_Maths::Vector4 xHighRM = xA.m_xRoughnessMetallic.Get(uRES - 1u, uRES - 1u);
	ZENITH_ASSERT_GT(xLowRM.y, xHighRM.y,
		"the LOW end of the height field is not rougher than the high end (%.4f vs "
		"%.4f) -- the cavity term is inverted", xLowRM.y, xHighRM.y);
	ZENITH_ASSERT_LT(xA.m_xOcclusion.Get(0u, 0u).x,
		xA.m_xOcclusion.Get(uRES - 1u, uRES - 1u).x,
		"the LOW end of the height field is not more occluded than the high end -- "
		"the occlusion term is inverted");

	// TOTAL on an empty input: an empty set, not three unusable images.
	const ZM_SynthPbrSet xEmpty = ZM_SynthBuildPbrSet(ZM_GenImage(), xResponse);
	ZENITH_ASSERT_FALSE(xEmpty.NonEmpty(),
		"an empty height field produced a non-empty PBR set");
}

// ############################################################################
// The luma height heuristic
// ############################################################################

// ★ THE FLATTEN PARAMETER IS THE WHOLE POINT, so it is what gets asserted.
// Luminance is not height -- a dark marking on flat skin is a marking, not a dent
// -- and the families that use this (humans, creatures) pass a hard flatten so
// the derived relief stays subtle. A flatten of 1 must give a CONSTANT field, and
// anything less must not.
ZENITH_TEST(ZM_Gen, SynthHeightFromAlbedoLumaFlattensAsAsked)
{
	constexpr u_int uRES = 32u;
	ZM_GenImage xAlbedo(uRES, uRES);
	for (u_int y = 0u; y < uRES; ++y)
	{
		for (u_int x = 0u; x < uRES; ++x)
		{
			const float fV = (x < uRES / 2u) ? 0.15f : 0.85f;   // a hard light/dark split
			xAlbedo.Set(y, x, Zenith_Maths::Vector4(fV, fV, fV, 1.0f));
		}
	}

	const ZM_GenImage xSharp = ZM_SynthHeightFromAlbedoLuma(xAlbedo, 0.0f);
	const ZM_GenImage xSoft  = ZM_SynthHeightFromAlbedoLuma(xAlbedo, 0.75f);
	const ZM_GenImage xFlat  = ZM_SynthHeightFromAlbedoLuma(xAlbedo, 1.0f);

	auto Span = [](const ZM_GenImage& xImg) -> float
	{
		float fMin = 2.0f, fMax = -1.0f;
		for (u_int y = 0u; y < xImg.GetHeight(); ++y)
		{
			for (u_int x = 0u; x < xImg.GetWidth(); ++x)
			{
				const float fH = xImg.Get(y, x).x;
				if (fH < fMin) { fMin = fH; }
				if (fH > fMax) { fMax = fH; }
			}
		}
		return fMax - fMin;
	};

	ZENITH_ASSERT_GT(Span(xSharp), 0.5f,
		"an unflattened luma height field lost the albedo's contrast (span %.4f)",
		Span(xSharp));
	ZENITH_ASSERT_LT(Span(xSoft), Span(xSharp),
		"flattening by 0.75 did not reduce the relief (%.4f vs %.4f)",
		Span(xSoft), Span(xSharp));
	ZENITH_ASSERT_GT(Span(xSoft), 0.0f,
		"flattening by 0.75 removed ALL relief -- the heuristic buys nothing");
	ZENITH_ASSERT_LT(Span(xFlat), 1.0e-4f,
		"a full flatten did not produce a constant field (span %.6f)", Span(xFlat));

	// ★ THE PIVOT IS THE IMAGE'S OWN MEAN, not a fixed 0.5. A uniformly DARK asset
	// flattened toward 0.5 would come out as one big dent; toward its own mean it
	// comes out flat, which is correct.
	ZM_GenImage xDark(uRES, uRES);
	for (u_int y = 0u; y < uRES; ++y)
	{
		for (u_int x = 0u; x < uRES; ++x)
		{
			xDark.Set(y, x, Zenith_Maths::Vector4(0.08f, 0.08f, 0.08f, 1.0f));
		}
	}
	const ZM_GenImage xDarkH = ZM_SynthHeightFromAlbedoLuma(xDark, 0.75f);
	ZENITH_ASSERT_LT(Span(xDarkH), 1.0e-4f,
		"a uniformly dark albedo produced relief (span %.6f) -- the flatten pivot is "
		"a fixed constant rather than the image's own mean", Span(xDarkH));
	ZENITH_ASSERT_LT(std::fabs(xDarkH.Get(0u, 0u).x - 0.08f), 0.02f,
		"a uniformly dark albedo was pushed to %.4f rather than staying at its own "
		"level", xDarkH.Get(0u, 0u).x);

	ZENITH_ASSERT_TRUE(ZM_SynthHeightFromAlbedoLuma(ZM_GenImage(), 0.5f).IsEmpty(),
		"an empty albedo produced a non-empty height field");
}

// ############################################################################
// Props carry the four-map set, and only metal is metallic
// ############################################################################
ZENITH_TEST(ZM_Gen, PropsCarryAFullPbrSet)
{
	u_int uMetallic = 0u;
	for (u_int u = 0u; u < (u_int)ZM_PROP_COUNT; ++u)
	{
		const ZM_PROP_ID eId = (ZM_PROP_ID)u;
		ZM_Prop xA, xB;
		ZM_BuildProp(eId, xA);
		ZM_BuildProp(eId, xB);

		ZENITH_ASSERT_TRUE(xA.m_xPbr.NonEmpty(),
			"%s is missing one of its PBR maps", ZM_GetPropName(eId));
		ZENITH_ASSERT_TRUE(xA.m_xPbr.Equals(xB.m_xPbr),
			"%s's PBR maps did not rebuild byte-identically", ZM_GetPropName(eId));

		const ZM_SynthPbrResponse xResp =
			ZM_PropPbrResponse(ZM_GetPropData(eId).m_ePalette);
		ZENITH_ASSERT_GT(xResp.m_fRoughness, 0.0f,
			"%s is perfectly smooth", ZM_GetPropName(eId));
		ZENITH_ASSERT_LE(xResp.m_fRoughness, 1.0f,
			"%s has a roughness above 1", ZM_GetPropName(eId));

		// ★ METALLIC IS A CLAIM ABOUT PHYSICS, NOT A LOOK. Only the METAL palette
		// may set it: on a dielectric it kills the diffuse and tints the reflection
		// by a colour no conductor has.
		if (ZM_GetPropData(eId).m_ePalette == ZM_PROP_PALETTE_METAL)
		{
			ZENITH_ASSERT_EQ_FLOAT(xResp.m_fMetallic, 1.0f, 1.0e-4f,
				"%s is METAL but is not metallic", ZM_GetPropName(eId));
			++uMetallic;
		}
		else
		{
			ZENITH_ASSERT_EQ_FLOAT(xResp.m_fMetallic, 0.0f, 1.0e-4f,
				"%s is a dielectric (%u) but claims to be metal", ZM_GetPropName(eId),
				(u_int)ZM_GetPropData(eId).m_ePalette);
		}

		// The relief is real for this prop, not a flat map wearing a normal's name.
		const ZM_GenImage xHeight = ZM_BuildPropHeight(ZM_ResolvePropRecipe(eId));
		float fMin = 2.0f, fMax = -1.0f;
		for (u_int y = 0u; y < xHeight.GetHeight(); ++y)
		{
			for (u_int x = 0u; x < xHeight.GetWidth(); ++x)
			{
				const float fH = xHeight.Get(y, x).x;
				if (fH < fMin) { fMin = fH; }
				if (fH > fMax) { fMax = fH; }
			}
		}
		ZENITH_ASSERT_GT(fMax - fMin, 0.02f,
			"%s's height field spans only %.4f -- its normal map is an expensive way "
			"to store (0.5, 0.5, 1)", ZM_GetPropName(eId), fMax - fMin);
	}

	// ANTI-VACUITY on the metallic clause: the roster must actually CONTAIN metal,
	// or the "only metal is metallic" rule is checking an empty set.
	ZENITH_ASSERT_GT(uMetallic, 0u,
		"no prop in the roster uses the METAL palette, so the metallic clause above "
		"asserted nothing");
}
