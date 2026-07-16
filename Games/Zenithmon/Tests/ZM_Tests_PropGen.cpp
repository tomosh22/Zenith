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
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/Gen/ZM_PropGen.h"
#include "Zenithmon/Source/Data/ZM_PropData.h"
#include "Maths/Zenith_Maths.h"
#include "Collections/Zenith_Vector.h"

#include <cstring>   // strlen
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
