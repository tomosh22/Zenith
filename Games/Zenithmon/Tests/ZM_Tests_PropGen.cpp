#include "Zenith.h"

// ============================================================================
// ZM_Tests_PropGen -- S4 unit gate for ZM_PropGen (suite ZM_Gen).
//
// Props are STATIC models (NO skeleton, NO animation). These author against the
// frozen public PropGen/Data seam and assert the roster/recipe/asset-path/
// determinism/static-mesh + biome-coverage contract. Pure / headless: no disk, no
// GPU, no ZENITH_TOOLS reach. Runs at boot before the scene loads.
//   1. PropGen_RosterTotality      -- every id yields a self-referencing row +
//                                      recipe + a buildable, ZM_ValidateProp-
//                                      passing bundle; the biome/kind contract.
//   2. PropGen_RecipePurity        -- resolve is pure f(id); distinct ids carry
//                                      distinct seeds; MESH != ALBEDO domain seed.
//   3. PropGen_AssetPathScheme     -- golden per-model refs + truncation.
//   4. PropGen_BuildDeterminism    -- reflexive byte-identity + hash; two distinct
//                                      ids differ.
//   5. PropGen_StaticMeshContract  -- zero bones, empty skin buffers, tris > 0,
//                                      outward winding, finite in-range UVs.
//   6. PropGen_BiomeDressingCoverage -- every real battle-dome biome has a DRESSING
//                                      set; the full dressing roster is present.
//   7. PropGen_MeshSensitivity     -- the MESH seed perturbs the mesh; a non-MESH
//                                      (ALBEDO) seed does not; distinct ids differ.
//   8. PropGen_TextureDomainIsolation -- the ALBEDO seed perturbs the texture; a
//                                      non-ALBEDO (MESH) seed does not; distinct
//                                      palette/biome props differ.
//   9. PropGen_GroundItemPickupRowsAreCentreAnchoredAndDistinct -- the ZM-67
//                                      ground-item presentations: the two ITEM
//                                      kinds are anchored at fZM_PROP_ITEM_BASE_Y
//                                      and every other kind is still grounded at
//                                      0; that anchor lands on Route 1's MEASURED
//                                      ground once the authored transform is
//                                      applied; the ITEM meshes fit the unit volume
//                                      that transform scales ACROSS A SWEPT MESH
//                                      SEED, not at one draw; the three are
//                                      pairwise distinct in mesh AND texture.
//  10. PropGen_RockKindsResolveToTheSharedEngineStones -- the three ROCK rows
//                                      present engine:Meshes/Rocks stones (their
//                                      own composition is one box); every other
//                                      row presents its own baked bundle, and a
//                                      prop's model and mesh refs never come
//                                      from different families.
//  11. PropGen_UVIslandsAreDisjointAndEveryVertexLandsInOne -- the 2x2 role
//                                      atlas: disjoint islands with real
//                                      gutters, painter and mesher agreeing on
//                                      the quadrants, and no vertex straddling
//                                      a seam.
//  12. PropGen_OnlyFixturesEmitAndOnlyOnTheirShade /
//      PropGen_LightFixturesAreBuiltAtExactlyTheirRosterSize -- the four
//                                      LIGHT_FIXTURE rows glow on the GLOW
//                                      island and nowhere else, nothing else
//                                      glows at all, and a fixture builds at
//                                      exactly its roster size so the fit is
//                                      the identity.
//
// ★ UNIT 9 ASSERTS STRUCTURE AND **MEASURES** APPEARANCE, and the split is
// deliberate. ZM-67 is a `human-gate` ticket: whether two props are far enough
// apart to tell at seven metres is a judgement a person signs, and a unit that
// asserted "these two colours are >= X apart" would be checking a threshold chosen
// by whoever picked the colours -- the self-referential guard this repo has been
// bitten by three times (reference_self_referential_guards_cannot_see_drift). So
// the hard clauses are INEQUALITIES (distinct meshes, distinct textures, distinct
// palettes) plus the geometry the frozen anchors demand, and the separation itself
// is LOGGED for the reviewer to read beside the capture.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/Gen/ZM_PropGen.h"
#include "Zenithmon/Source/Data/ZM_PropData.h"
// The AUTHORED transform the ITEM meshes were built around. It is here for ONE
// clause -- (a2) below -- and that clause is the only INDEPENDENT oracle this file
// has on fZM_PROP_ITEM_BASE_Y: every other statement about the anchor compares the
// mesh with the constant the mesh was built from. Pure, header-only, no ECS and no
// ZENITH_TOOLS, so it costs this leaf TU nothing but the compiled world table it
// already links.
#include "Zenithmon/Source/World/ZM_Route1Placement.h"
// ZM_ComputePropFit -- the transform the AUTHORING applies. Unit 12 asserts the
// four light fixtures come out of it as the IDENTITY, which is what lets a
// fixture placement row state a model BASE rather than a guess.
#include "Zenithmon/Source/World/ZM_PropFit.h"
#include "Maths/Zenith_Maths.h"
#include "Collections/Zenith_Vector.h"

#include <cstdio>    // snprintf -- the engine rock ref scheme
#include <cstring>   // strlen / strncmp
#include <cmath>     // std::isfinite

namespace
{
	// A prop id is one of the DRESSING (battle-dome biome set) rows.
	bool PropIsDressing(ZM_PROP_ID eId)
	{
		return ZM_GetPropData(eId).m_eKind == ZM_PROP_KIND_DRESSING;
	}
}

// ############################################################################
// 1. Roster totality -- every prop is resolvable + buildable + valid
// ############################################################################

// For EVERY ZM_PROP_ID: the roster row self-references (m_eId == index), the recipe
// resolves, and ZM_BuildProp produces a bundle that passes the whole ZM_ValidateProp
// contract -- a static (zero-bone, empty-skin) box mesh with outward winding,
// non-degenerate bounds, in-range indices, finite UVs, and a non-empty texture.
// Plus the biome/kind contract: DRESSING rows carry a real biome, every other row
// is ZM_PROP_BIOME_NONE.
ZENITH_TEST(ZM_Gen, PropGen_RosterTotality)
{
	ZENITH_ASSERT_EQ(ZM_GetPropCount(), (u_int)ZM_PROP_COUNT,
		"prop count must equal ZM_PROP_COUNT");

	u_int uTested = 0u;
	for (u_int id = 0; id < (u_int)ZM_PROP_COUNT; ++id)
	{
		const ZM_PROP_ID eId = (ZM_PROP_ID)id;

		// Roster-table integrity: the row indexes itself.
		ZENITH_ASSERT_EQ((u_int)ZM_GetPropData(eId).m_eId, id,
			"prop row %u does not self-reference (m_eId mismatch)", id);

		// Recipe resolves and carries this id.
		const ZM_PropRecipe xR = ZM_ResolvePropRecipe(eId);
		ZENITH_ASSERT_EQ((u_int)xR.m_eId, id, "recipe %u carries the wrong id", id);

		// Full bundle build + validation.
		ZM_Prop xProp;
		ZM_BuildProp(eId, xProp);

		ZENITH_ASSERT_GT(xProp.m_xMesh.GetNumVerts(), 0u, "prop %u mesh empty", id);
		ZENITH_ASSERT_GT(xProp.m_xMesh.GetNumTris(), 0u, "prop %u has no triangles", id);
		ZENITH_ASSERT_EQ(xProp.m_xMesh.GetNumBones(), 0u,
			"prop %u must be static (zero bones)", id);

		const ZM_PropValidation xV = ZM_ValidateProp(xProp);
		ZENITH_ASSERT_TRUE(xV.m_bAllValid, "prop %u failed the ZM_ValidateProp rollup", id);
		ZENITH_ASSERT_TRUE(xV.m_xMesh.m_bWindingOutward,
			"prop %u winding not outward (bad tri %u)", id, xV.m_xMesh.m_uFirstBadTriangle);
		ZENITH_ASSERT_TRUE(xV.m_xMesh.m_bBoundsNonDegen, "prop %u has degenerate bounds", id);
		ZENITH_ASSERT_TRUE(xV.m_xMesh.m_bIndicesInRange, "prop %u indices out of range", id);
		ZENITH_ASSERT_TRUE(xV.m_xMesh.m_bUVsFinite, "prop %u UVs not finite/in [0,1]", id);
		ZENITH_ASSERT_TRUE(xV.m_xMesh.m_bNoSkeleton, "prop %u must carry zero bones", id);
		ZENITH_ASSERT_TRUE(xV.m_xMesh.m_bNoSkinBuffers, "prop %u must have empty skin buffers", id);
		ZENITH_ASSERT_TRUE(xV.m_bTextureNonEmpty, "prop %u placeholder texture is empty", id);

		// Biome/kind contract: DRESSING rows carry a real biome; every other row is NONE.
		const ZM_PROP_BIOME eBiome = ZM_GetPropData(eId).m_eBiome;
		if (PropIsDressing(eId))
		{
			ZENITH_ASSERT_NE((u_int)eBiome, (u_int)ZM_PROP_BIOME_NONE,
				"dressing %u must carry a real biome", id);
			ZENITH_ASSERT_NE((u_int)xR.m_eBiome, (u_int)ZM_PROP_BIOME_NONE,
				"dressing recipe %u biome must be real", id);
		}
		else
		{
			ZENITH_ASSERT_EQ((u_int)eBiome, (u_int)ZM_PROP_BIOME_NONE,
				"non-dressing %u must be ZM_PROP_BIOME_NONE", id);
		}

		++uTested;
	}
	ZENITH_ASSERT_GT(uTested, 0u, "no props exercised the roster-totality gate");
}

// ############################################################################
// 2. Recipe purity + distinct synthetic seeds
// ############################################################################

// ZM_ResolvePropRecipe is a pure function of the id (resolving twice yields a
// field-identical recipe), distinct ids carry distinct synthetic seeds (no
// name-hash collision), and each id's MESH domain seed differs from its ALBEDO
// domain seed (the domains never alias).
ZENITH_TEST(ZM_Gen, PropGen_RecipePurity)
{
	Zenith_Vector<u_int> xSeeds;
	for (u_int id = 0; id < (u_int)ZM_PROP_COUNT; ++id)
	{
		const ZM_PROP_ID eId = (ZM_PROP_ID)id;
		const ZM_PropRecipe xA = ZM_ResolvePropRecipe(eId);
		const ZM_PropRecipe xB = ZM_ResolvePropRecipe(eId);

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
			"prop %u MESH/ALBEDO domain seeds must differ", id);

		xSeeds.PushBack(xA.m_uSyntheticSeed);
	}

	// Pairwise-distinct synthetic seeds across the whole roster.
	for (u_int i = 0; i < xSeeds.GetSize(); ++i)
	{
		for (u_int j = i + 1u; j < xSeeds.GetSize(); ++j)
		{
			ZENITH_ASSERT_NE(xSeeds.Get(i), xSeeds.Get(j),
				"props %u/%u share a synthetic seed (name-hash collision)", i, j);
		}
	}
}

// ############################################################################
// 3. Asset-path scheme (golden per-model refs + truncation)
// ############################################################################

// Golden-locks the four per-model refs for a known id (LampPost) and the too-
// small-buffer -> false (truncation) contract. Pure; compiled in ALL configs.
ZENITH_TEST(ZM_Gen, PropGen_AssetPathScheme)
{
	char acRef[256];

	ZENITH_ASSERT_TRUE(
		ZM_PropAssetPath(ZM_PROP_LAMP_POST, ZM_PROP_ASSET_MESH, acRef, sizeof(acRef)),
		"mesh ref must fit");
	ZENITH_ASSERT_STREQ(acRef, "game:Props/LampPost/LampPost.zmesh", "mesh ref scheme drifted");

	ZENITH_ASSERT_TRUE(
		ZM_PropAssetPath(ZM_PROP_LAMP_POST, ZM_PROP_ASSET_ALBEDO, acRef, sizeof(acRef)),
		"albedo ref must fit");
	ZENITH_ASSERT_STREQ(acRef, "game:Props/LampPost/LampPost_albedo.ztxtr",
		"albedo ref scheme drifted");

	ZENITH_ASSERT_TRUE(
		ZM_PropAssetPath(ZM_PROP_LAMP_POST, ZM_PROP_ASSET_MATERIAL, acRef, sizeof(acRef)),
		"material ref must fit");
	ZENITH_ASSERT_STREQ(acRef, "game:Props/LampPost/LampPost.zmtrl", "material ref scheme drifted");

	ZENITH_ASSERT_TRUE(
		ZM_PropAssetPath(ZM_PROP_LAMP_POST, ZM_PROP_ASSET_MODEL, acRef, sizeof(acRef)),
		"model ref must fit");
	ZENITH_ASSERT_STREQ(acRef, "game:Props/LampPost/LampPost.zmodel", "model ref scheme drifted");

	// Truncation: a cap far too small returns false + stays NUL-terminated.
	char acTiny[8];
	const bool bFits = ZM_PropAssetPath(ZM_PROP_LAMP_POST, ZM_PROP_ASSET_MESH, acTiny, sizeof(acTiny));
	ZENITH_ASSERT_FALSE(bFits, "an 8-byte cap cannot hold the mesh ref -- must report truncation");
	ZENITH_ASSERT_LT((u_int)strlen(acTiny), (u_int)sizeof(acTiny),
		"a truncated ref must stay NUL-terminated within the cap");
}

// ############################################################################
// 4. Build determinism (reflexive) -- byte-identity + hash machinery
// ############################################################################

// Reflexive lock on the SC4 determinism helpers: building the SAME id twice yields
// a byte-identical mesh (ZM_PropMeshEqual), an equal build (ZM_PropBuildEqual), and
// an equal content hash (ZM_PropContentHash). Plus a non-degeneracy guard: two
// DISTINCT ids whose recipes genuinely differ (LampPost KIND_LAMP METAL vs
// DressingCanyon KIND_DRESSING STONE -> different box set AND texture) must NOT
// collapse to one bundle/hash.
ZENITH_TEST(ZM_Gen, PropGen_BuildDeterminism)
{
	ZM_Prop xA;
	ZM_Prop xB;
	ZM_BuildProp(ZM_PROP_LAMP_POST, xA);
	ZM_BuildProp(ZM_PROP_LAMP_POST, xB);

	ZENITH_ASSERT_TRUE(ZM_PropBuildEqual(xA, xB),
		"rebuilding LampPost must yield an equal bundle (ZM_PropBuildEqual)");
	ZENITH_ASSERT_TRUE(ZM_PropMeshEqual(xA.m_xMesh, xB.m_xMesh),
		"rebuilding LampPost must yield a byte-identical mesh (ZM_PropMeshEqual)");
	ZENITH_ASSERT_EQ(ZM_PropContentHash(xA), ZM_PropContentHash(xB),
		"rebuilding LampPost must yield an equal content hash (reflexive)");

	// Non-degeneracy: a distinct id with a genuinely different recipe must not be
	// build-equal, and must differ in its hash OR its mesh (a trivially-constant
	// hash would otherwise pass everything above).
	ZM_Prop xOther;
	ZM_BuildProp(ZM_PROP_DRESSING_CANYON, xOther);
	ZENITH_ASSERT_FALSE(ZM_PropBuildEqual(xA, xOther),
		"LampPost and DressingCanyon differ in kind + palette -- must not be build-equal");
	const bool bHashDiffers = (ZM_PropContentHash(xA) != ZM_PropContentHash(xOther));
	const bool bMeshDiffers = !ZM_PropMeshEqual(xA.m_xMesh, xOther.m_xMesh);
	ZENITH_ASSERT_TRUE(bHashDiffers || bMeshDiffers,
		"LampPost and DressingCanyon must differ in content hash or mesh");
}

// ############################################################################
// 5. Static-mesh contract
// ############################################################################

// A built prop mesh is STATIC: exactly zero bones, byte-empty skin buffers, at
// least one triangle, ZM_ValidateGenMeshStatic-valid (outward winding), and every
// UV finite within [0,1]. The complete per-vertex buffers (normal/UV/tangent)
// confirm the finalise pass ran.
ZENITH_TEST(ZM_Gen, PropGen_StaticMeshContract)
{
	ZM_Prop xProp;
	ZM_BuildProp(ZM_PROP_TABLE, xProp);
	const ZM_GenMesh& xMesh = xProp.m_xMesh;

	ZENITH_ASSERT_EQ(xMesh.GetNumBones(), 0u, "static prop mesh must have zero bones");
	ZENITH_ASSERT_EQ(xMesh.m_xBoneIndices.GetSize(), 0u,
		"static prop mesh must have an empty bone-index buffer");
	ZENITH_ASSERT_EQ(xMesh.m_xBoneWeights.GetSize(), 0u,
		"static prop mesh must have an empty bone-weight buffer");
	ZENITH_ASSERT_GT(xMesh.GetNumTris(), 0u, "static prop mesh must have triangles");

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
// 6. Biome dressing coverage -- every battle-dome biome has a set
// ############################################################################

// Each REAL biome b in [uZM_PROP_BIOME_FIRST_REAL, ZM_PROP_BIOME_COUNT) has at
// least one DRESSING roster row whose m_eBiome == b (the battle-dome dressing sets
// are complete), and the total DRESSING count is at least one-per-real-biome
// (ZM_PROP_BIOME_COUNT - 1, since NONE is not a real biome).
ZENITH_TEST(ZM_Gen, PropGen_BiomeDressingCoverage)
{
	u_int uDressingTotal = 0u;
	for (u_int id = 0; id < (u_int)ZM_PROP_COUNT; ++id)
	{
		if (ZM_GetPropData((ZM_PROP_ID)id).m_eKind == ZM_PROP_KIND_DRESSING) { ++uDressingTotal; }
	}

	for (u_int b = uZM_PROP_BIOME_FIRST_REAL; b < (u_int)ZM_PROP_BIOME_COUNT; ++b)
	{
		u_int uMatches = 0u;
		for (u_int id = 0; id < (u_int)ZM_PROP_COUNT; ++id)
		{
			const ZM_PropData& xRow = ZM_GetPropData((ZM_PROP_ID)id);
			if (xRow.m_eKind == ZM_PROP_KIND_DRESSING && (u_int)xRow.m_eBiome == b) { ++uMatches; }
		}
		ZENITH_ASSERT_GE(uMatches, 1u, "biome %u has no DRESSING set", b);
	}

	ZENITH_ASSERT_GE(uDressingTotal, (u_int)ZM_PROP_BIOME_COUNT - 1u,
		"the dressing roster must cover every real biome (>= ZM_PROP_BIOME_COUNT - 1 sets)");
}

// ############################################################################
// 7. Mesh sensitivity -- the MESH seed (and only it) drives the geometry
// ############################################################################

// Mutating the MESH domain seed perturbs the box mesh (the jitter reads it),
// mutating a NON-mesh (ALBEDO) domain seed leaves the mesh byte-identical (the
// builder never draws it), and two distinct ids yield distinct meshes. TABLE is a
// FURNITURE prop whose mesh consumes all three (fW/fD/fH) MESH dim draws, so the
// MESH-seed mutation genuinely perturbs the geometry.
ZENITH_TEST(ZM_Gen, PropGen_MeshSensitivity)
{
	const ZM_PropRecipe xBase = ZM_ResolvePropRecipe(ZM_PROP_TABLE);
	ZM_GenMesh x0; ZM_BuildPropMesh(xBase, x0);

	// Mutating the MESH domain seed perturbs the mesh.
	ZM_PropRecipe xMeshMut = xBase;
	xMeshMut.m_aulDomainSeed[ZM_GEN_DOMAIN_MESH] ^= 0x9E3779B97F4A7C15ull;
	ZM_GenMesh xM; ZM_BuildPropMesh(xMeshMut, xM);
	ZENITH_ASSERT_FALSE(ZM_PropMeshEqual(x0, xM), "mutating the MESH seed must perturb the prop mesh");

	// Mutating a NON-MESH (ALBEDO) seed leaves the mesh byte-identical.
	ZM_PropRecipe xAlbMut = xBase;
	xAlbMut.m_aulDomainSeed[ZM_GEN_DOMAIN_ALBEDO] ^= 0x9E3779B97F4A7C15ull;
	ZM_GenMesh xA; ZM_BuildPropMesh(xAlbMut, xA);
	ZENITH_ASSERT_TRUE(ZM_PropMeshEqual(x0, xA), "mutating a non-MESH seed must NOT change the prop mesh");

	// Two distinct ids yield distinct meshes.
	ZM_GenMesh xOther; ZM_BuildPropMesh(ZM_ResolvePropRecipe(ZM_PROP_DRESSING_CANYON), xOther);
	ZENITH_ASSERT_FALSE(ZM_PropMeshEqual(x0, xOther), "distinct prop ids must yield distinct meshes");
}

// ############################################################################
// 8. Texture domain isolation -- ALBEDO (and only it) drives the texture
// ############################################################################

// Mutating the ALBEDO domain seed perturbs the placeholder albedo (the colour
// jitter reads it), mutating a NON-albedo (MESH) domain seed leaves the texture
// byte-identical (the builder never constructs a MESH RNG), and a distinct
// palette/biome prop yields a distinct texture. DressingCanyon (STONE palette +
// CANYON biome tint) vs Table (WOOD palette, no biome) diverge.
ZENITH_TEST(ZM_Gen, PropGen_TextureDomainIsolation)
{
	const ZM_PropRecipe xBase = ZM_ResolvePropRecipe(ZM_PROP_DRESSING_CANYON);
	const ZM_GenImage x0 = ZM_BuildPropTexture(xBase);

	// Mutating the ALBEDO seed changes the texture.
	ZM_PropRecipe xAlb = xBase;
	xAlb.m_aulDomainSeed[ZM_GEN_DOMAIN_ALBEDO] ^= 0x9E3779B97F4A7C15ull;
	const ZM_GenImage xA = ZM_BuildPropTexture(xAlb);
	ZENITH_ASSERT_FALSE(x0.Equals(xA), "mutating the ALBEDO seed must change the prop texture");
	ZENITH_ASSERT_NE(x0.ContentHash(), xA.ContentHash(), "ALBEDO mutation must change the prop texture hash");

	// Mutating a non-ALBEDO (MESH) seed leaves the texture byte-identical.
	ZM_PropRecipe xMesh = xBase;
	xMesh.m_aulDomainSeed[ZM_GEN_DOMAIN_MESH] ^= 0x9E3779B97F4A7C15ull;
	const ZM_GenImage xM = ZM_BuildPropTexture(xMesh);
	ZENITH_ASSERT_TRUE(x0.Equals(xM), "mutating a non-ALBEDO seed must NOT change the prop texture");
	ZENITH_ASSERT_EQ(x0.ContentHash(), xM.ContentHash(), "non-ALBEDO mutation must not change the prop texture hash");

	// A distinct-palette/biome prop yields a distinct texture.
	const ZM_GenImage xWood = ZM_BuildPropTexture(ZM_ResolvePropRecipe(ZM_PROP_TABLE));
	ZENITH_ASSERT_FALSE(x0.Equals(xWood), "distinct palette/biome props must yield distinct textures");
}

// ############################################################################
// 9. The ZM-67 ground-item pickup presentations
// ############################################################################

namespace
{
	// Every ITEM row lives inside the volume the authored ground-item transform
	// scales -- a unit cube centred on the entity origin. Nothing here spells 0.6 m:
	// the entity's scale supplies the metres, so the LOCAL bound is +/- 0.5 whatever
	// fZM_ROUTE1_PROP_CUBE_EDGE is re-frozen to.
	constexpr float fPICKUP_HALF_VOLUME = 0.5f;

	// Positions come out of a fixed set of axis-aligned box appends, so an anchor is
	// exact rather than approximate; this only absorbs the float round trip.
	constexpr float fPICKUP_EPSILON = 1.0e-4f;

	bool PropIsItemKind(ZM_PROP_ID eId)
	{
		const ZM_PROP_KIND eKind = ZM_GetPropData(eId).m_eKind;
		return eKind == ZM_PROP_KIND_ITEM_PICKUP || eKind == ZM_PROP_KIND_ITEM_SPENT;
	}

	struct PropMeshBounds
	{
		float m_fMinX = 0.0f, m_fMaxX = 0.0f;
		float m_fMinY = 0.0f, m_fMaxY = 0.0f;
		float m_fMinZ = 0.0f, m_fMaxZ = 0.0f;
		bool  m_bValid = false;
	};

	PropMeshBounds PropBoundsOf(const ZM_GenMesh& xMesh)
	{
		PropMeshBounds xB;
		const u_int uVerts = xMesh.GetNumVerts();
		for (u_int v = 0; v < uVerts; ++v)
		{
			const Zenith_Maths::Vector3& xP = xMesh.m_xPositions.Get(v);
			if (!xB.m_bValid)
			{
				xB.m_fMinX = xB.m_fMaxX = xP.x;
				xB.m_fMinY = xB.m_fMaxY = xP.y;
				xB.m_fMinZ = xB.m_fMaxZ = xP.z;
				xB.m_bValid = true;
				continue;
			}
			if (xP.x < xB.m_fMinX) { xB.m_fMinX = xP.x; }
			if (xP.x > xB.m_fMaxX) { xB.m_fMaxX = xP.x; }
			if (xP.y < xB.m_fMinY) { xB.m_fMinY = xP.y; }
			if (xP.y > xB.m_fMaxY) { xB.m_fMaxY = xP.y; }
			if (xP.z < xB.m_fMinZ) { xB.m_fMinZ = xP.z; }
			if (xP.z > xB.m_fMaxZ) { xB.m_fMaxZ = xP.z; }
		}
		return xB;
	}

	// LOGGED CONTEXT ONLY -- see the header block. Nothing asserts on this value.
	Zenith_Maths::Vector3 PropTextureMeanRGB(const ZM_GenImage& xImage)
	{
		if (xImage.IsEmpty()) { return Zenith_Maths::Vector3(0.0f); }
		double dR = 0.0, dG = 0.0, dB = 0.0;
		for (u_int uY = 0; uY < xImage.GetHeight(); ++uY)
		{
			for (u_int uX = 0; uX < xImage.GetWidth(); ++uX)
			{
				const Zenith_Maths::Vector4 xTexel = xImage.Get(uY, uX);
				dR += (double)xTexel.x; dG += (double)xTexel.y; dB += (double)xTexel.z;
			}
		}
		const double dN = (double)xImage.GetWidth() * (double)xImage.GetHeight();
		return Zenith_Maths::Vector3(
			(float)(dR / dN), (float)(dG / dN), (float)(dB / dN));
	}

	float PropColourDistance(const Zenith_Maths::Vector3& xA, const Zenith_Maths::Vector3& xB)
	{
		const float fR = xA.x - xB.x, fG = xA.y - xB.y, fB = xA.z - xB.z;
		return std::sqrt(fR * fR + fG * fG + fB * fB);
	}

	// The three ZM-67 presentations, in the order a prop moves through them.
	const ZM_PROP_ID aePICKUP_PRESENTATIONS[] =
	{
		ZM_PROP_ITEM_PHIAL, ZM_PROP_ITEM_ORB, ZM_PROP_ITEM_TAKEN,
	};
	constexpr u_int uPICKUP_PRESENTATION_COUNT =
		(u_int)(sizeof(aePICKUP_PRESENTATIONS) / sizeof(aePICKUP_PRESENTATIONS[0]));

	// The three Route 1 columns a ground-item prop actually stands on. Spelled as
	// its own list rather than walked from ZM_ROUTE1_GROUND_SAMPLE_COUNT because six
	// of that enum's nine rows are gates, arrivals and trainers, which wear no prop
	// mesh and are anchored at their FEET rather than at a volume centre.
	const ZM_ROUTE1_GROUND_SAMPLE aePICKUP_PROP_COLUMNS[] =
	{
		ZM_ROUTE1_GROUND_SAMPLE_PROP_SOUTH_SALVE,
		ZM_ROUTE1_GROUND_SAMPLE_PROP_LANE_CATCHORB,
		ZM_ROUTE1_GROUND_SAMPLE_PROP_NORTH_SALVE,
	};
	constexpr u_int uPICKUP_PROP_COLUMN_COUNT =
		(u_int)(sizeof(aePICKUP_PROP_COLUMNS) / sizeof(aePICKUP_PROP_COLUMNS[0]));

	// ---- The jitter sweep (clause (b2)) -------------------------------------
	//
	// ★★ WHY A SWEEP AND NOT AN ARITHMETIC BOUND. ZM_BuildPropMesh draws ONE jitter
	// triple per row, off that row's own MESH-domain PCG (ZM_PropGen.cpp:170-179).
	// Building a row and measuring it therefore samples ONE point of the band, and a
	// row sized with no headroom would pass or fail on which point its name hash
	// happened to land on. The obvious alternative -- asserting `row dimension x 1.04
	// <= 1.0` -- re-spells the generator's own jitter constant here, so widening the
	// band in ZM_PropGen.cpp would move the generator and leave this file asserting
	// the OLD band: the self-referential shape this repo has been bitten by three
	// times (reference_self_referential_guards_cannot_see_drift).
	//
	// So the sweep STEERS the generator instead, exactly as PropGen_MeshSensitivity
	// does: it walks the MESH domain seed and measures what comes out. The factor it
	// records is the produced extent over the ROSTER dimension, which for both ITEM
	// compositions is (1 + that draw's jitter) exactly -- read back from the mesh,
	// never recomputed. If the band widens, the observed factors widen with it and
	// the volume clauses red on their own.
	//
	// ★ AND IT IS BOUNDED, SO STATE WHAT IT BUYS HONESTLY: sampling cannot reach the
	// band's open edge, only approach it. uPICKUP_JITTER_SWEEP_SAMPLES draws per row
	// is what makes "measured near the extreme" an earned claim, and the widest draw
	// actually reached is LOGGED rather than assumed.
	constexpr u_int uPICKUP_JITTER_SWEEP_SAMPLES = 512u;

	// The stride the sweep walks the MESH seed by -- the 64-bit golden-ratio odd
	// constant, the same one PropGen_MeshSensitivity perturbs with. Odd, so the walk
	// visits uPICKUP_JITTER_SWEEP_SAMPLES distinct seeds, and the PCG decorrelates
	// them.
	constexpr u_int64 ulPICKUP_SEED_STRIDE = 0x9E3779B97F4A7C15ull;

	// ANTI-VACUITY FLOOR, and it is NOT a claim about how wide the jitter band is.
	// It is a floor on how far into the GROWTH half of whatever band exists this
	// sweep actually got: a sweep that only ever drew shrinking rows would satisfy
	// every volume clause below while testing nothing, because a prop can only breach
	// the unit volume by growing. If the generator's jitter is ever narrowed past
	// this, THIS clause reds and names the reason, which is the right conversation to
	// have -- the volume clauses would otherwise start passing vacuously.
	constexpr float fPICKUP_MIN_SWEPT_GROWTH = 1.03f;

	// What one row's sweep observed. Every field is a MEASUREMENT off a built mesh.
	struct PropSweptExtremes
	{
		float m_fMaxBaseDeviation = 0.0f;   // |lowest vertex - fZM_PROP_ITEM_BASE_Y|
		float m_fMaxTop           = 0.0f;   // the highest vertex any draw reached
		float m_fMaxAbsX          = 0.0f;   // the widest |x| any draw reached
		float m_fMaxAbsZ          = 0.0f;   // the deepest |z| any draw reached
		float m_fMaxSizeFactor    = 0.0f;   // widest produced extent / roster dimension
		float m_fMinSizeFactor    = 0.0f;   // ...and the narrowest
		u_int m_uSamples          = 0u;
	};

	float PropMaxf(float fA, float fB) { return fA > fB ? fA : fB; }
	float PropAbsf(float f)            { return f < 0.0f ? -f : f; }
}

// The ZM-67 ground-item presentations, in five clauses.
//
// (a) THE ANCHOR, BOTH WAYS ROUND. The two ITEM kinds start at
//     fZM_PROP_ITEM_BASE_Y because their entity's origin is the CENTRE of a unit
//     volume (ZM_Route1PropCentreY authors "measured surface + half the cube
//     edge"), and EVERY other kind still starts at 0 because it is dropped onto an
//     identity transform. The second half is the regression guard that matters: a
//     centre anchor leaking into the scenery kinds would sink every fence, sign and
//     lamp in the game half a model into the ground, and no ground-item test would
//     have noticed.
//     ★ THE SCENERY HALF IS A REAL ORACLE AND THE ITEM HALF IS NOT, and the file
//     is honest about the difference. `0.0f` is a fact about identity transforms
//     that the generator does not supply; fZM_PROP_ITEM_BASE_Y is the very constant
//     ZM_PropGen.cpp:231 built the mesh from, so a drift in it moves both sides of
//     that comparison together and the clause cannot fail. (a2) is the half that
//     can.
// (a2) THE ANCHOR AGAINST THE **GROUND**. The independent relation, and the only
//     statement in this file that can see fZM_PROP_ITEM_BASE_Y move:
//
//         ZM_Route1PropCentreY(column)
//             + fZM_PROP_ITEM_BASE_Y * fZM_ROUTE1_PROP_CUBE_EDGE
//         == ZM_Route1GroundFeetY(column)
//
//     i.e. "the local plane the ITEM compositions stand on, carried through the
//     AUTHORED transform, lands exactly on the surface the Route 1 ground-truth
//     oracle MEASURED under that prop". The right-hand side is a raycast result
//     frozen from a real bake (ZM_Route1Placement.h's measured table) -- nothing
//     ZM_PropGen computes -- so the two sides cannot drift together. It closes the
//     hazard ZM_Route1Placement.h names in its own words at ZM_Route1PropCentreY:
//     "Change the halving here and the props hover or sink, with every other check
//     in the suite green." Either constant moving reds this, naming the column.
// (b) THE VOLUME, AT THE SHIPPED DRAW. An ITEM mesh that reached outside
//     [-0.5, +0.5] on any axis would, once the authored scale is applied, stand
//     further into the walked lane than the blockout cube it replaces -- and
//     ZM-D-207's reach budget holds these props inside 2.5 m of that lane with no
//     slack to lend.
// (b2) THE VOLUME, ACROSS THE JITTER BAND. (b) measures the ONE jitter triple the
//     row's own name hash happens to draw, which is a single point of a band the
//     generator can reach anywhere in. So the MESH-domain seed is SWEPT and the
//     volume re-checked on the widest draw found -- see the sweep note in the
//     anonymous namespace for why steering the generator beats multiplying the
//     roster row by its jitter constant. The widest draw reached is logged, and an
//     anti-vacuity floor asserts the sweep explored the growth half of the band at
//     all. A row sized without headroom -- one whose largest dimension times the
//     widest observed jitter exceeds the unit volume -- reds here even if its own
//     shipped draw happens to fit.
// (c) THE SEPARATION. The three presentations must be pairwise distinct in BOTH
//     mesh and texture, or the "collected" state and the two item kinds would be
//     the same picture. Distinctness is asserted; HOW distinct is logged.
ZENITH_TEST(ZM_Gen, PropGen_GroundItemPickupRowsAreCentreAnchoredAndDistinct)
{
	// (a) + (b), over the WHOLE roster so the exception is provably confined.
	// ★ THE SHIPPED DRAW ONLY -- ZM_BuildProp takes the row's own MESH seed. (b2)
	// below is what covers the rest of the jitter band.
	u_int uItemRows = 0u;
	for (u_int id = 0; id < (u_int)ZM_PROP_COUNT; ++id)
	{
		const ZM_PROP_ID eId = (ZM_PROP_ID)id;

		ZM_Prop xProp;
		ZM_BuildProp(eId, xProp);
		const PropMeshBounds xB = PropBoundsOf(xProp.m_xMesh);
		ZENITH_ASSERT_TRUE(xB.m_bValid, "prop %u ('%s') built an empty mesh, so its "
			"anchor cannot be measured", id, ZM_GetPropName(eId));
		if (!xB.m_bValid) { continue; }

		const bool bItem = PropIsItemKind(eId);
		const float fExpectedBase = bItem ? fZM_PROP_ITEM_BASE_Y : 0.0f;
		ZENITH_ASSERT_EQ_FLOAT(xB.m_fMinY, fExpectedBase, fPICKUP_EPSILON,
			"prop %u ('%s', kind %u) has its lowest vertex at %.5f; a %s prop must be "
			"anchored at %.5f. A grounded ITEM prop hovers half a model above its "
			"measured column, and a centre-anchored SCENERY prop sinks into it",
			id, ZM_GetPropName(eId), (u_int)ZM_GetPropData(eId).m_eKind,
			(double)xB.m_fMinY, bItem ? "centre-anchored ITEM" : "grounded scenery",
			(double)fExpectedBase);

		if (!bItem) { continue; }
		++uItemRows;

		ZENITH_ASSERT_LE(xB.m_fMaxY, fPICKUP_HALF_VOLUME + fPICKUP_EPSILON,
			"ITEM prop '%s' reaches %.5f, past the top of the unit volume its "
			"authored transform scales", ZM_GetPropName(eId), (double)xB.m_fMaxY);
		ZENITH_ASSERT_GE(xB.m_fMinX, -fPICKUP_HALF_VOLUME - fPICKUP_EPSILON,
			"ITEM prop '%s' reaches %.5f on -X, wider than the cube it replaces",
			ZM_GetPropName(eId), (double)xB.m_fMinX);
		ZENITH_ASSERT_LE(xB.m_fMaxX, fPICKUP_HALF_VOLUME + fPICKUP_EPSILON,
			"ITEM prop '%s' reaches %.5f on +X, wider than the cube it replaces",
			ZM_GetPropName(eId), (double)xB.m_fMaxX);
		ZENITH_ASSERT_GE(xB.m_fMinZ, -fPICKUP_HALF_VOLUME - fPICKUP_EPSILON,
			"ITEM prop '%s' reaches %.5f on -Z, deeper than the cube it replaces",
			ZM_GetPropName(eId), (double)xB.m_fMinZ);
		ZENITH_ASSERT_LE(xB.m_fMaxZ, fPICKUP_HALF_VOLUME + fPICKUP_EPSILON,
			"ITEM prop '%s' reaches %.5f on +Z, deeper than the cube it replaces",
			ZM_GetPropName(eId), (double)xB.m_fMaxZ);

		// METHODOLOGY, not a threshold: the aspect ratio a reviewer is being asked to
		// tell apart in the AFTER captures, printed from the mesh that was actually
		// built rather than from the roster row it came from.
		const float fWidth  = xB.m_fMaxX - xB.m_fMinX;
		const float fHeight = xB.m_fMaxY - xB.m_fMinY;
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[PropGen] OBSERVED presentation '%s': local bounds x[%.4f, %.4f] "
			"y[%.4f, %.4f] z[%.4f, %.4f], aspect (h/w) %.3f",
			ZM_GetPropName(eId),
			(double)xB.m_fMinX, (double)xB.m_fMaxX,
			(double)xB.m_fMinY, (double)xB.m_fMaxY,
			(double)xB.m_fMinZ, (double)xB.m_fMaxZ,
			(double)(fWidth > 1.0e-4f ? fHeight / fWidth : 0.0f));
	}

	ZENITH_ASSERT_EQ(uItemRows, uPICKUP_PRESENTATION_COUNT,
		"the roster carries %u ITEM-kind rows but ZM-67 presents %u -- a ground-item "
		"prop can only ever wear one of the presentations this table lists",
		uItemRows, uPICKUP_PRESENTATION_COUNT);

	// (a2) The anchor against the MEASURED ground, per authored prop column. See the
	// clause note above the test: this is the half of the anchor claim that is not a
	// constant compared with itself.
	for (u_int i = 0; i < uPICKUP_PROP_COLUMN_COUNT; ++i)
	{
		const ZM_ROUTE1_GROUND_SAMPLE eColumn = aePICKUP_PROP_COLUMNS[i];
		const float fMeasuredGroundY = ZM_Route1GroundFeetY(eColumn);

		// The local ITEM ground plane, carried through the AUTHORED transform: the
		// entity centre the scene places, plus the composition's own base scaled by
		// the uniform edge that entity wears.
		const float fPropFootInWorld =
			ZM_Route1PropCentreY(eColumn)
			+ fZM_PROP_ITEM_BASE_Y * fZM_ROUTE1_PROP_CUBE_EDGE;

		ZENITH_ASSERT_EQ_FLOAT(fPropFootInWorld, fMeasuredGroundY, fPICKUP_EPSILON,
			"column '%s': an ITEM composition's base lands at world Y %.5f but the "
			"MEASURED terrain surface under that prop is %.5f, so the prop %s. Either "
			"fZM_PROP_ITEM_BASE_Y or the halving in ZM_Route1PropCentreY has moved "
			"without the other -- every other check in both suites stays green when "
			"that happens, which is why this clause exists",
			ZM_GetRoute1GroundSample(eColumn).m_szEntityName,
			(double)fPropFootInWorld, (double)fMeasuredGroundY,
			fPropFootInWorld > fMeasuredGroundY ? "hovers" : "sinks into the ground");
	}

	// (b2) The volume across the JITTER BAND, by steering the generator.
	float fSweptMaxFactor = 0.0f;
	const char* szSweptWidestRow = "none";
	for (u_int i = 0; i < uPICKUP_PRESENTATION_COUNT; ++i)
	{
		const ZM_PROP_ID eId = aePICKUP_PRESENTATIONS[i];
		const ZM_PropData& xRow = ZM_GetPropData(eId);
		const ZM_PropRecipe xBase = ZM_ResolvePropRecipe(eId);

		PropSweptExtremes xSwept;
		ZM_GenMesh xMesh;   // hoisted: ZM_BuildPropMesh Reset()s it on entry
		for (u_int uDraw = 0; uDraw < uPICKUP_JITTER_SWEEP_SAMPLES; ++uDraw)
		{
			ZM_PropRecipe xSteered = xBase;
			xSteered.m_aulDomainSeed[ZM_GEN_DOMAIN_MESH] =
				xBase.m_aulDomainSeed[ZM_GEN_DOMAIN_MESH]
					+ (u_int64)uDraw * ulPICKUP_SEED_STRIDE;

			ZM_BuildPropMesh(xSteered, xMesh);
			const PropMeshBounds xB = PropBoundsOf(xMesh);
			if (!xB.m_bValid) { continue; }

			// The three produced extents over the three ROSTER dimensions. For both
			// ITEM compositions the box set spans exactly (fW, fH, fD), so each ratio
			// IS that draw's jitter factor -- read back off the mesh, never recomputed
			// from ZM_PropGen's constants.
			const float afFactor[3] =
			{
				(xB.m_fMaxX - xB.m_fMinX) / xRow.m_fWidth,
				(xB.m_fMaxY - xB.m_fMinY) / xRow.m_fHeight,
				(xB.m_fMaxZ - xB.m_fMinZ) / xRow.m_fDepth,
			};

			if (xSwept.m_uSamples == 0u)
			{
				xSwept.m_fMaxTop = xB.m_fMaxY;
				xSwept.m_fMinSizeFactor = afFactor[0];
			}
			for (u_int a = 0; a < 3u; ++a)
			{
				xSwept.m_fMaxSizeFactor = PropMaxf(xSwept.m_fMaxSizeFactor, afFactor[a]);
				if (afFactor[a] < xSwept.m_fMinSizeFactor)
				{
					xSwept.m_fMinSizeFactor = afFactor[a];
				}
			}
			xSwept.m_fMaxBaseDeviation = PropMaxf(xSwept.m_fMaxBaseDeviation,
				PropAbsf(xB.m_fMinY - fZM_PROP_ITEM_BASE_Y));
			xSwept.m_fMaxTop  = PropMaxf(xSwept.m_fMaxTop,  xB.m_fMaxY);
			xSwept.m_fMaxAbsX = PropMaxf(xSwept.m_fMaxAbsX,
				PropMaxf(PropAbsf(xB.m_fMinX), PropAbsf(xB.m_fMaxX)));
			xSwept.m_fMaxAbsZ = PropMaxf(xSwept.m_fMaxAbsZ,
				PropMaxf(PropAbsf(xB.m_fMinZ), PropAbsf(xB.m_fMaxZ)));
			++xSwept.m_uSamples;
		}

		// EVERY draw built a mesh. A sweep that silently dropped its samples would
		// leave every extreme below at its zero-initialised value and pass.
		ZENITH_ASSERT_EQ(xSwept.m_uSamples, uPICKUP_JITTER_SWEEP_SAMPLES,
			"ITEM prop '%s': only %u of %u swept MESH seeds produced a mesh, so the "
			"extremes measured below are not the band they claim to be",
			ZM_GetPropName(eId), xSwept.m_uSamples, uPICKUP_JITTER_SWEEP_SAMPLES);

		// ANTI-VACUITY FIRST, so the clauses after it mean something. A prop can only
		// breach the unit volume by GROWING, so a sweep that never drew a growing row
		// would be checking nothing.
		ZENITH_ASSERT_GE(xSwept.m_fMaxSizeFactor, fPICKUP_MIN_SWEPT_GROWTH,
			"ITEM prop '%s': the widest of %u swept MESH draws inflated the row by only "
			"x%.5f. The volume clauses below are then vacuous -- they would pass for a "
			"row with no headroom at all. Either the mesh jitter has been narrowed (say "
			"so here) or the sweep has stopped reaching the generator",
			ZM_GetPropName(eId), xSwept.m_uSamples, (double)xSwept.m_fMaxSizeFactor);

		// The anchor is JITTER-INDEPENDENT: the base is spelled, not scaled. A
		// generator change that made it ride fH would put every prop underground.
		ZENITH_ASSERT_LE(xSwept.m_fMaxBaseDeviation, fPICKUP_EPSILON,
			"ITEM prop '%s': across %u swept MESH draws its lowest vertex wandered "
			"%.5f from fZM_PROP_ITEM_BASE_Y -- the anchor has started depending on the "
			"jitter, so the prop's depth in the ground would vary with its name hash",
			ZM_GetPropName(eId), xSwept.m_uSamples,
			(double)xSwept.m_fMaxBaseDeviation);

		// THE GUARD. The widest draw the sweep found still fits the unit volume the
		// authored transform scales.
		ZENITH_ASSERT_LE(xSwept.m_fMaxTop, fPICKUP_HALF_VOLUME + fPICKUP_EPSILON,
			"ITEM prop '%s': its tallest swept draw reaches %.5f, past the top of the "
			"unit volume (roster height %.3f; the widest inflation this sweep drew on "
			"ANY axis of this row was x%.5f). Give the row headroom -- the transform, "
			"the measured ground columns and ZM-D-207's 2.5 m reach budget are all "
			"frozen, so the picture is what has to move",
			ZM_GetPropName(eId), (double)xSwept.m_fMaxTop,
			(double)xRow.m_fHeight, (double)xSwept.m_fMaxSizeFactor);
		ZENITH_ASSERT_LE(xSwept.m_fMaxAbsX, fPICKUP_HALF_VOLUME + fPICKUP_EPSILON,
			"ITEM prop '%s': its widest swept draw reaches %.5f on X, wider than the "
			"cube it replaces", ZM_GetPropName(eId), (double)xSwept.m_fMaxAbsX);
		ZENITH_ASSERT_LE(xSwept.m_fMaxAbsZ, fPICKUP_HALF_VOLUME + fPICKUP_EPSILON,
			"ITEM prop '%s': its deepest swept draw reaches %.5f on Z, deeper than the "
			"cube it replaces", ZM_GetPropName(eId), (double)xSwept.m_fMaxAbsZ);

		if (xSwept.m_fMaxSizeFactor > fSweptMaxFactor)
		{
			fSweptMaxFactor  = xSwept.m_fMaxSizeFactor;
			szSweptWidestRow = ZM_GetPropName(eId);
		}

		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[PropGen] OBSERVED jitter sweep '%s' over %u MESH seeds: size factor "
			"[%.5f, %.5f], extremes top %.5f |x| %.5f |z| %.5f (unit volume half "
			"%.3f)",
			ZM_GetPropName(eId), xSwept.m_uSamples,
			(double)xSwept.m_fMinSizeFactor, (double)xSwept.m_fMaxSizeFactor,
			(double)xSwept.m_fMaxTop, (double)xSwept.m_fMaxAbsX,
			(double)xSwept.m_fMaxAbsZ, (double)fPICKUP_HALF_VOLUME);
	}

	// ...and the widest factor found ANYWHERE in the sweep applied to EVERY row. The
	// jitter stream belongs to the GENERATOR, not to a row -- any row can draw any
	// factor, and which one it draws is decided by a name hash -- so a row that merely
	// got lucky with its own seeds is caught here.
	//
	// ★ THIS IS THE CLAUSE THAT MAKES THE HEADROOM CLAIM FALSIFIABLE. Raise any ITEM
	// row's largest dimension until (that dimension x the widest factor this sweep
	// observed) passes the unit volume and this reds, whether or not that row's own
	// shipped draw happens to fit. Try it before trusting it.
	for (u_int i = 0; i < uPICKUP_PRESENTATION_COUNT; ++i)
	{
		const ZM_PROP_ID eId = aePICKUP_PRESENTATIONS[i];
		const ZM_PropData& xRow = ZM_GetPropData(eId);
		const float fLargest = PropMaxf(xRow.m_fWidth,
			PropMaxf(xRow.m_fDepth, xRow.m_fHeight));

		ZENITH_ASSERT_LE(fLargest * fSweptMaxFactor,
			2.0f * fPICKUP_HALF_VOLUME + fPICKUP_EPSILON,
			"ITEM prop '%s': its largest roster dimension %.3f inflated by x%.5f -- the "
			"widest draw this sweep found anywhere (on '%s') -- spans %.5f, past the "
			"whole unit volume. The row survives its OWN seeds by luck; the jitter "
			"stream belongs to the generator and any row can draw any factor",
			ZM_GetPropName(eId), (double)fLargest, (double)fSweptMaxFactor,
			szSweptWidestRow, (double)(fLargest * fSweptMaxFactor));
	}

	// (c) Pairwise distinctness, asserted; separation, logged.
	for (u_int i = 0; i < uPICKUP_PRESENTATION_COUNT; ++i)
	{
		ZM_Prop xA;
		ZM_BuildProp(aePICKUP_PRESENTATIONS[i], xA);
		const Zenith_Maths::Vector3 xMeanA = PropTextureMeanRGB(xA.m_xTexture);

		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[PropGen] OBSERVED presentation '%s': palette %u, mean albedo "
			"(%.4f, %.4f, %.4f)",
			ZM_GetPropName(aePICKUP_PRESENTATIONS[i]),
			(u_int)ZM_GetPropData(aePICKUP_PRESENTATIONS[i]).m_ePalette,
			(double)xMeanA.x, (double)xMeanA.y, (double)xMeanA.z);

		for (u_int j = i + 1u; j < uPICKUP_PRESENTATION_COUNT; ++j)
		{
			ZM_Prop xB2;
			ZM_BuildProp(aePICKUP_PRESENTATIONS[j], xB2);

			// The PALETTE choice is what makes the albedos differ, and it is a roster
			// fact rather than a distance: two presentations sharing a palette would
			// separate on shape alone, which a re-tuned sky can flatten.
			ZENITH_ASSERT_NE(
				(u_int)ZM_GetPropData(aePICKUP_PRESENTATIONS[i]).m_ePalette,
				(u_int)ZM_GetPropData(aePICKUP_PRESENTATIONS[j]).m_ePalette,
				"presentations '%s' and '%s' share a palette",
				ZM_GetPropName(aePICKUP_PRESENTATIONS[i]),
				ZM_GetPropName(aePICKUP_PRESENTATIONS[j]));

			ZENITH_ASSERT_FALSE(ZM_PropMeshEqual(xA.m_xMesh, xB2.m_xMesh),
				"presentations '%s' and '%s' build the SAME mesh, so a prop would not "
				"change shape when it is taken or when the item kind differs",
				ZM_GetPropName(aePICKUP_PRESENTATIONS[i]),
				ZM_GetPropName(aePICKUP_PRESENTATIONS[j]));
			ZENITH_ASSERT_FALSE(xA.m_xTexture.Equals(xB2.m_xTexture),
				"presentations '%s' and '%s' build the SAME albedo",
				ZM_GetPropName(aePICKUP_PRESENTATIONS[i]),
				ZM_GetPropName(aePICKUP_PRESENTATIONS[j]));

			const Zenith_Maths::Vector3 xMeanB = PropTextureMeanRGB(xB2.m_xTexture);
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[PropGen] OBSERVED separation '%s' vs '%s': mean-albedo distance %.4f "
				"(CONTEXT for the ZM-67 human sign-off; no threshold is asserted)",
				ZM_GetPropName(aePICKUP_PRESENTATIONS[i]),
				ZM_GetPropName(aePICKUP_PRESENTATIONS[j]),
				(double)PropColourDistance(xMeanA, xMeanB));
		}
	}
}

// ############################################################################
// 10. The ROCK kinds present the shared ENGINE stones
// ############################################################################

// ★★ A ROCK WAS ONE BOX, AND NO TEXTURE MAKES A BOX A ROCK. The engine already
// ships four sculpted stones under engine:Meshes/Rocks
// (Tools/Zenith_Tools_RockAssetExport.cpp -- the same set RenderTest and
// Dawnmere's scatter dress with), so the three ROCK roster rows present one of
// those and ZM_PropFit scales it to the roster size like any other delivery.
//
// ★ THE CLAUSES ARE ABOUT THE RESOLVER, NOT ABOUT DISK. Whether the .zasset is
// present is a bake question, and a clone with no engine bake would legitimately
// have none; what must hold in EVERY configuration is that a rock row resolves
// to an engine ref and every other row resolves to its own game: bundle. A rock
// that silently fell back to its own bake would be a cube again, in a scene that
// still loads, with every other clause in this file green.
ZENITH_TEST(ZM_Gen, PropGen_RockKindsResolveToTheSharedEngineStones)
{
	u_int uRocks = 0u, uOwnBundle = 0u;
	for (u_int u = 0u; u < (u_int)ZM_PROP_COUNT; ++u)
	{
		const ZM_PROP_ID eId = (ZM_PROP_ID)u;
		const bool bRockKind = ZM_GetPropData(eId).m_eKind == ZM_PROP_KIND_ROCK;

		char acModel[512], acMesh[512];
		ZENITH_ASSERT_TRUE(ZM_PropModelRef(eId, acModel, sizeof(acModel)),
			"'%s' model ref must fit", ZM_GetPropName(eId));
		ZENITH_ASSERT_TRUE(ZM_PropMeshRef(eId, acMesh, sizeof(acMesh)),
			"'%s' mesh ref must fit", ZM_GetPropName(eId));

		// The two answers agree about WHICH family the prop comes from. A model
		// from the engine set with a mesh from the game set would fit an engine
		// stone to a cube's measurements.
		const bool bModelIsEngine = strncmp(acModel, "engine:", 7) == 0;
		const bool bMeshIsEngine  = strncmp(acMesh,  "engine:", 7) == 0;
		ZENITH_ASSERT_EQ(bModelIsEngine ? 1u : 0u, bMeshIsEngine ? 1u : 0u,
			"'%s' resolves its MODEL to '%s' and its MESH to '%s' -- one is the "
			"engine set and the other is not, so ZM_ResolvePropFit would measure a "
			"different asset from the one that renders",
			ZM_GetPropName(eId), acModel, acMesh);

		if (bRockKind)
		{
			++uRocks;
			const char* szStem = ZM_PropEngineRockStem(eId);
			ZENITH_ASSERT_NOT_NULL(szStem,
				"'%s' is ZM_PROP_KIND_ROCK but names no engine stone, so it falls "
				"back to its own generated bundle -- which is a single box",
				ZM_GetPropName(eId));
			ZENITH_ASSERT_TRUE(ZM_PropUsesEngineRock(eId),
				"'%s' is a rock kind that does not report using an engine rock",
				ZM_GetPropName(eId));
			ZENITH_ASSERT_TRUE(bModelIsEngine,
				"'%s' resolves to '%s', not an engine: ref -- it would render the "
				"generator's cube", ZM_GetPropName(eId), acModel);
			// The exact scheme the rock exporter writes, so a rename on either side
			// reds here rather than at load time.
			char acExpected[512];
			snprintf(acExpected, sizeof(acExpected),
				"engine:Meshes/Rocks/Rock_%s.zmodel", szStem);
			ZENITH_ASSERT_STREQ(acModel, acExpected,
				"'%s' resolves to a ref the rock exporter does not write",
				ZM_GetPropName(eId));
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_Gen] OBSERVED '%s' presents the shared engine stone '%s'",
				ZM_GetPropName(eId), acModel);
		}
		else
		{
			++uOwnBundle;
			ZENITH_ASSERT_FALSE(ZM_PropUsesEngineRock(eId),
				"'%s' is not a rock kind but claims an engine rock",
				ZM_GetPropName(eId));
			ZENITH_ASSERT_FALSE(bModelIsEngine,
				"'%s' resolves to the engine set ('%s') -- only the rock kinds "
				"substitute", ZM_GetPropName(eId), acModel);
			// It is its OWN bundle, at the documented scheme.
			char acOwn[512];
			ZENITH_ASSERT_TRUE(
				ZM_PropAssetPath(eId, ZM_PROP_ASSET_MODEL, acOwn, sizeof(acOwn)),
				"'%s' own model ref must fit", ZM_GetPropName(eId));
			ZENITH_ASSERT_STREQ(acModel, acOwn,
				"'%s' presents '%s' rather than its own baked bundle '%s'",
				ZM_GetPropName(eId), acModel, acOwn);
		}
	}

	// ANTI-VACUITY, both ways.
	ZENITH_ASSERT_GE(uRocks, 3u,
		"only %u rock-kind rows were found; the roster carries RockSmall, RockLarge "
		"and Boulder, and this clause is what keeps them off the cube", uRocks);
	ZENITH_ASSERT_GT(uOwnBundle, 0u,
		"every roster row resolved to the engine set -- the substitution has "
		"widened past the rock kinds");
	// TOTAL on a garbage id.
	ZENITH_ASSERT_NULL(ZM_PropEngineRockStem(ZM_PROP_COUNT),
		"an out-of-range id must name no engine stone");
}

// ############################################################################
// 11. The per-KIND UV islands
// ############################################################################

// ★★ EVERY FACE USED TO SAMPLE THE WHOLE [0,1] SQUARE -- a post, a rail, a sign
// board and a lamp head all wore the same stretched square of grain, and the one
// accent band landed across all of them at once. The texture is a 2x2 atlas of
// ROLES now. These clauses pin the atlas itself (disjoint islands, real gutters,
// painter and mesher agreeing on which quadrant is whose) and then that every
// built mesh's UVs actually land inside a role island rather than straddling a
// seam -- which is what catches a composition arm that forgot its role.
ZENITH_TEST(ZM_Gen, PropGen_UVIslandsAreDisjointAndEveryVertexLandsInOne)
{
	// (a) THE ATLAS.
	for (u_int a = 0u; a < (u_int)ZM_PROP_UV_ROLE_COUNT; ++a)
	{
		const ZM_PROP_UV_ROLE eA = (ZM_PROP_UV_ROLE)a;
		const ZM_GenUVIsland xIsland = ZM_PropUVIsland(eA);
		const ZM_GenUVIsland xPaint  = ZM_PropUVPaintRect(eA);

		ZENITH_ASSERT_GT(xIsland.m_fU1, xIsland.m_fU0, "role %u island is inverted in U", a);
		ZENITH_ASSERT_GT(xIsland.m_fV1, xIsland.m_fV0, "role %u island is inverted in V", a);
		ZENITH_ASSERT_GE(xIsland.m_fU0, -1.0e-6f, "role %u island leaves [0,1] in U", a);
		ZENITH_ASSERT_LE(xIsland.m_fU1, 1.0f + 1.0e-6f, "role %u island leaves [0,1] in U", a);
		ZENITH_ASSERT_GE(xIsland.m_fV0, -1.0e-6f, "role %u island leaves [0,1] in V", a);
		ZENITH_ASSERT_LE(xIsland.m_fV1, 1.0f + 1.0e-6f, "role %u island leaves [0,1] in V", a);

		// The mesh island is strictly INSIDE its paint rect: that inset is the
		// gutter, and without it a bilinear tap at an island edge reads the
		// neighbouring role -- a lamp's shade sampling the housing's paint.
		ZENITH_ASSERT_GT(xIsland.m_fU0, xPaint.m_fU0, "role %u has no gutter on its -U edge", a);
		ZENITH_ASSERT_LT(xIsland.m_fU1, xPaint.m_fU1, "role %u has no gutter on its +U edge", a);
		ZENITH_ASSERT_GT(xIsland.m_fV0, xPaint.m_fV0, "role %u has no gutter on its -V edge", a);
		ZENITH_ASSERT_LT(xIsland.m_fV1, xPaint.m_fV1, "role %u has no gutter on its +V edge", a);

		// The painter and the mesher agree about which quadrant is whose.
		const float fMidU = 0.5f * (xIsland.m_fU0 + xIsland.m_fU1);
		const float fMidV = 0.5f * (xIsland.m_fV0 + xIsland.m_fV1);
		ZENITH_ASSERT_EQ((u_int)ZM_PropUVRoleAt(fMidU, fMidV), a,
			"the centre of role %u's island (%.3f, %.3f) resolves to role %u -- the "
			"mesh islands and the painter's quadrants disagree, so every part would "
			"wear the wrong material", a, (double)fMidU, (double)fMidV,
			(u_int)ZM_PropUVRoleAt(fMidU, fMidV));

		for (u_int b = a + 1u; b < (u_int)ZM_PROP_UV_ROLE_COUNT; ++b)
		{
			const ZM_GenUVIsland xB = ZM_PropUVIsland((ZM_PROP_UV_ROLE)b);
			const bool bDisjoint = xIsland.m_fU1 <= xB.m_fU0 || xB.m_fU1 <= xIsland.m_fU0
				|| xIsland.m_fV1 <= xB.m_fV0 || xB.m_fV1 <= xIsland.m_fV0;
			ZENITH_ASSERT_TRUE(bDisjoint,
				"roles %u and %u overlap on the atlas -- one part's texture would "
				"bleed into another's", a, b);
		}
	}

	// (b) EVERY BUILT MESH lands inside SOME island.
	//
	// ★ THE MESH ALONE, not ZM_BuildProp. This clause is about UVs, and building
	// the full bundle would paint a 512^2 albedo plus a height field and three
	// derived maps for all 32 rows to look at a vertex attribute -- 16x the texel
	// work the old 128^2 maps cost, on a unit that runs at every boot.
	for (u_int u = 0u; u < (u_int)ZM_PROP_COUNT; ++u)
	{
		const ZM_PROP_ID eId = (ZM_PROP_ID)u;
		ZM_GenMesh xMesh;
		ZM_BuildPropMesh(ZM_ResolvePropRecipe(eId), xMesh);

		for (u_int v = 0u; v < xMesh.GetNumVerts(); ++v)
		{
			const Zenith_Maths::Vector2& xUV = xMesh.m_xUVs.Get(v);
			bool bInSome = false;
			for (u_int a = 0u; a < (u_int)ZM_PROP_UV_ROLE_COUNT && !bInSome; ++a)
			{
				const ZM_GenUVIsland xI = ZM_PropUVIsland((ZM_PROP_UV_ROLE)a);
				bInSome = xUV.x >= xI.m_fU0 - 1.0e-4f && xUV.x <= xI.m_fU1 + 1.0e-4f
					&& xUV.y >= xI.m_fV0 - 1.0e-4f && xUV.y <= xI.m_fV1 + 1.0e-4f;
			}
			ZENITH_ASSERT_TRUE(bInSome,
				"'%s' vertex %u has UV (%.4f, %.4f), which is in no role island -- it "
				"samples across a seam or over the whole atlas, which is exactly the "
				"every-face-wears-everything failure the islands replaced",
				ZM_GetPropName(eId), v, (double)xUV.x, (double)xUV.y);
		}
	}

	// (c) THE RESOLUTION. 128 could not carry four islands: each part would get a
	//     64-texel square.
	ZENITH_ASSERT_GE(uZM_PROP_ALBEDO_RESOLUTION, 512u,
		"the prop albedo is %u^2. Split into four role islands that is %u texels "
		"per part, and a fence post standing at arm's length goes soft",
		uZM_PROP_ALBEDO_RESOLUTION, uZM_PROP_ALBEDO_RESOLUTION / 2u);
}

// ############################################################################
// 12. The light fixtures and their emissive mask
// ############################################################################

// A fixture GLOWS on its shade and nowhere else, and nothing that is not a
// fixture glows at all. The mask is what enforces it: the engine's default
// emissive texture is WHITE, so a colour and an intensity with no mask would
// light the whole fixture, body and flex included.
ZENITH_TEST(ZM_Gen, PropGen_OnlyFixturesEmitAndOnlyOnTheirShade)
{
	u_int uFixtures = 0u;
	for (u_int u = 0u; u < (u_int)ZM_PROP_COUNT; ++u)
	{
		const ZM_PROP_ID eId = (ZM_PROP_ID)u;
		const bool bFixture = ZM_GetPropData(eId).m_eKind == ZM_PROP_KIND_LIGHT_FIXTURE;
		const ZM_PropEmissive xEmissive = ZM_GetPropEmissive(eId);

		// The mask and the mesh, not the whole bundle -- see unit 11(b) for why
		// this file stopped calling ZM_BuildProp in a roster-wide loop.
		const ZM_PropRecipe xRecipe = ZM_ResolvePropRecipe(eId);
		const ZM_GenImage xMask = ZM_BuildPropEmissive(xRecipe);
		ZENITH_ASSERT_FALSE(xMask.IsEmpty(),
			"'%s' has no emissive map at all -- the bundle shape must be ONE shape "
			"so the bake and the family manifest can enumerate it",
			ZM_GetPropName(eId));

		// Where the mask is bright, in role terms.
		const u_int uRes = xMask.GetWidth();
		u_int uLitInGlow = 0u, uLitOutsideGlow = 0u;
		for (u_int uY = 0u; uY < uRes; ++uY)
		{
			for (u_int uX = 0u; uX < uRes; ++uX)
			{
				if (xMask.Get(uY, uX).x <= 0.5f) { continue; }
				const float fU = ((float)uX + 0.5f) / (float)uRes;
				const float fV = ((float)uY + 0.5f) / (float)uRes;
				if (ZM_PropUVRoleAt(fU, fV) == ZM_PROP_UV_GLOW) { ++uLitInGlow; }
				else                                            { ++uLitOutsideGlow; }
			}
		}

		ZENITH_ASSERT_EQ(uLitOutsideGlow, 0u,
			"'%s' has %u lit texels OUTSIDE the GLOW island -- whatever is meshed "
			"into those roles (a lamp's body, a batten's housing) would glow as "
			"brightly as its own shade", ZM_GetPropName(eId), uLitOutsideGlow);

		if (bFixture)
		{
			++uFixtures;
			ZENITH_ASSERT_GT(xEmissive.m_fIntensity, 1.0f,
				"'%s' is a light fixture emitting at %.2f -- a lit shade is brighter "
				"than any diffuse surface in the room, and below about 1 it just "
				"reads as pale paint",
				ZM_GetPropName(eId), (double)xEmissive.m_fIntensity);
			ZENITH_ASSERT_GT(uLitInGlow, 0u,
				"'%s' is a light fixture whose emissive mask is entirely black -- it "
				"declares a glow and paints none, so the fixture is a dark object "
				"hanging under a floating light", ZM_GetPropName(eId));

			// The shade is really meshed into the GLOW island.
			bool bAnyGlowVert = false;
			const ZM_GenUVIsland xGlow = ZM_PropUVIsland(ZM_PROP_UV_GLOW);
			ZM_GenMesh xFixtureMesh;
			ZM_BuildPropMesh(xRecipe, xFixtureMesh);
			for (u_int v = 0u; v < xFixtureMesh.GetNumVerts() && !bAnyGlowVert; ++v)
			{
				const Zenith_Maths::Vector2& xUV = xFixtureMesh.m_xUVs.Get(v);
				bAnyGlowVert = xUV.x >= xGlow.m_fU0 - 1.0e-4f && xUV.x <= xGlow.m_fU1 + 1.0e-4f
					&& xUV.y >= xGlow.m_fV0 - 1.0e-4f && xUV.y <= xGlow.m_fV1 + 1.0e-4f;
			}
			ZENITH_ASSERT_TRUE(bAnyGlowVert,
				"'%s' paints a glow but no part of its mesh is in the GLOW island -- "
				"the emissive lands on nothing", ZM_GetPropName(eId));

			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_Gen] OBSERVED '%s': %u lit texels, emissive (%.2f, %.2f, %.2f) x %.1f",
				ZM_GetPropName(eId), uLitInGlow, (double)xEmissive.m_xColour.x,
				(double)xEmissive.m_xColour.y, (double)xEmissive.m_xColour.z,
				(double)xEmissive.m_fIntensity);
		}
		else
		{
			ZENITH_ASSERT_EQ_FLOAT(xEmissive.m_fIntensity, 0.0f, 1.0e-6f,
				"'%s' is not a light fixture but emits at %.3f",
				ZM_GetPropName(eId), (double)xEmissive.m_fIntensity);
			ZENITH_ASSERT_EQ(uLitInGlow, 0u,
				"'%s' is not a light fixture but its emissive mask has %u lit texels",
				ZM_GetPropName(eId), uLitInGlow);
		}
	}

	ZENITH_ASSERT_EQ(uFixtures, 4u,
		"%u light-fixture rows were found; the roster carries a pendant, a bedside "
		"lamp, a standard lamp and a lab batten -- one per DISTINCT light in the "
		"two rooms", uFixtures);
	// TOTAL out of range.
	ZENITH_ASSERT_EQ_FLOAT(ZM_GetPropEmissive(ZM_PROP_COUNT).m_fIntensity, 0.0f, 1.0e-6f,
		"an out-of-range id must answer an inert emissive row");
}

// ★ A FIXTURE IS BUILT AT EXACTLY ITS ROSTER SIZE, which is what lets its
// placement row's Y be the model's base to the millimetre -- and that is what
// makes "the pendant's rose ends flush with the ceiling" a number rather than a
// hope. Every other kind is jittered +/-4% on purpose; these four are not.
ZENITH_TEST(ZM_Gen, PropGen_LightFixturesAreBuiltAtExactlyTheirRosterSize)
{
	u_int uChecked = 0u;
	for (u_int u = 0u; u < (u_int)ZM_PROP_COUNT; ++u)
	{
		const ZM_PROP_ID eId = (ZM_PROP_ID)u;
		const ZM_PropData& xData = ZM_GetPropData(eId);
		if (xData.m_eKind != ZM_PROP_KIND_LIGHT_FIXTURE) { continue; }
		++uChecked;

		// The mesh alone: this clause is about SIZE, and only four rows reach it.
		ZM_GenMesh xMesh;
		ZM_BuildPropMesh(ZM_ResolvePropRecipe(eId), xMesh);
		const Zenith_Maths::Vector3 xMin = ZM_GenMeshBoundsMin(xMesh);
		const Zenith_Maths::Vector3 xMax = ZM_GenMeshBoundsMax(xMesh);

		// Grounded at zero, and exactly as tall as the row says.
		ZENITH_ASSERT_EQ_FLOAT(xMin.y, 0.0f, 1.0e-4f,
			"'%s' is not grounded at y=0 (min.y=%.5f) -- its placement row's Y is "
			"the model BASE, so a lifted model hangs that far low",
			ZM_GetPropName(eId), (double)xMin.y);
		ZENITH_ASSERT_EQ_FLOAT(xMax.y - xMin.y, xData.m_fHeight, 1.0e-3f,
			"'%s' builds %.4f m tall against a %.4f m roster row. A fixture must NOT "
			"be jittered: the pendant is hung at (ceiling - height), and a 4 percent "
			"error puts its rose through the ceiling or leaves a gap under it",
			ZM_GetPropName(eId), (double)(xMax.y - xMin.y), (double)xData.m_fHeight);

		// ...so the fit is the identity, which is what the authoring relies on.
		const ZM_PropFit xFit = ZM_ComputePropFit(xMin, xMax,
			xData.m_fWidth, xData.m_fDepth, xData.m_fHeight);
		ZENITH_ASSERT_EQ_FLOAT(xFit.m_fScale, 1.0f, 1.0e-3f,
			"'%s' does not fit at scale 1 (got %.5f)", ZM_GetPropName(eId),
			(double)xFit.m_fScale);
		ZENITH_ASSERT_EQ_FLOAT(xFit.m_fGroundY, 0.0f, 1.0e-4f,
			"'%s' needs a ground lift of %.5f", ZM_GetPropName(eId),
			(double)xFit.m_fGroundY);
	}
	ZENITH_ASSERT_EQ(uChecked, 4u, "only %u fixtures were checked", uChecked);
}
