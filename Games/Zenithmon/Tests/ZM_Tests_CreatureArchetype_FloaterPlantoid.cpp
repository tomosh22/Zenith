#include "Zenith.h"

// ============================================================================
// ZM_Tests_CreatureArchetype_FloaterPlantoid -- S4 FLOATER-PLANTOID archetype
// gate (suite ZM_Gen). The generic contract harness lives in
// ZM_Tests_CreatureGen.cpp; this file is the per-archetype gate for the
// FLOATER-PLANTOID builder (ZM_BuildArchetype_FloaterPlantoid). It:
//   * runs the universal creature contract over the FLOATER_PLANTOID species set
//     (mesh structure + the ZM_ValidateCreature rollup + same-seed determinism), and
//   * adds the FLOATER-PLANTOID-specific STRUCTURAL assert: the built skeleton is a
//     floating radial plant/jellyfish -- a single spine ROOT (the bulb), a HEAD
//     crown, exactly SIX radial tendril bones ("Tendril..."), ZERO ground-leg
//     ("...Up") bones, and a mesh whose min-Y bound is ABOVE the ground plane
//     (the body FLOATS) -- all resolvable via the shared ZM_CreatureArchetypeCommon
//     kit naming convention (spine prefix "Spine", the head bone "Head") plus the
//     builder's local tendril prefix ("Tendril").
//
// PURE / HEADLESS: no disk, no GPU, no ZENITH_TOOLS. Authored against the frozen
// seam, NOT the builder .cpp. Every build guards on ZM_GetArchetypeBuilder != nullptr
// (via HasFloaterPlantoidBuilder), so this gate no-ops cleanly until the orchestrator
// wires FLOATER_PLANTOID into the dispatch switch; the species set is discovered
// dynamically.
//
// COST: the all-species passes here use the mesh-only ZM_BuildCreatureMesh (no
// texture synth); the full-bundle rollup + same-seed determinism run over a small
// curated subset, since ZM_Tests_CreatureGen already validates the full bundle over
// every buildable species.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/Gen/ZM_CreatureGen.h"
#include "Zenithmon/Source/Gen/ZM_CreatureArchetypeCommon.h"
#include "Zenithmon/Source/Gen/ZM_GenCommon.h"
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"
#include "Maths/Zenith_Maths.h"

#include <cstring>   // strlen, strncmp, strcmp

namespace
{
	// The FIXED radial tendril count the builder emits (golden-fixed across all evo
	// tiers). Mirrors uFLOATER_TENDRIL_COUNT in the builder .cpp (that constant lives
	// in the builder's anonymous namespace and is not visible here, so the contract
	// is restated as the gate's expectation).
	constexpr u_int uEXPECTED_TENDRILS = 6u;

	bool HasFloaterPlantoidBuilder()
	{
		return ZM_GetArchetypeBuilder(ZM_ARCHETYPE_FLOATER_PLANTOID) != nullptr;
	}

	bool IsFloaterPlantoid(ZM_SPECIES_ID eId)
	{
		const ZM_SpeciesData& xRow = ZM_GetSpeciesData(eId);
		return xRow.m_eArchetype == ZM_ARCHETYPE_FLOATER_PLANTOID
			&& ZM_GetArchetypeBuilder(xRow.m_eArchetype) != nullptr;
	}

	bool NameStartsWith(const char* szName, const char* szPrefix)
	{
		return strncmp(szName, szPrefix, strlen(szPrefix)) == 0;
	}

	bool NameEndsWith(const char* szName, const char* szSuffix)
	{
		const size_t uNameLen = strlen(szName);
		const size_t uSufLen  = strlen(szSuffix);
		if (uSufLen > uNameLen) { return false; }
		return strcmp(szName + (uNameLen - uSufLen), szSuffix) == 0;
	}

	u_int CountBonesEndingWith(const ZM_GenMesh& xMesh, const char* szSuffix)
	{
		u_int uCount = 0u;
		for (u_int u = 0; u < xMesh.GetNumBones(); ++u)
		{
			if (NameEndsWith(xMesh.m_xBones.Get(u).m_szName, szSuffix)) { ++uCount; }
		}
		return uCount;
	}

	u_int CountBonesStartingWith(const ZM_GenMesh& xMesh, const char* szPrefix)
	{
		u_int uCount = 0u;
		for (u_int u = 0; u < xMesh.GetNumBones(); ++u)
		{
			if (NameStartsWith(xMesh.m_xBones.Get(u).m_szName, szPrefix)) { ++uCount; }
		}
		return uCount;
	}

	// Index of the single root bone (parent -1), or -1 if not exactly one.
	int SingleRootIndex(const ZM_GenMesh& xMesh)
	{
		int iRoot = -1;
		u_int uRootCount = 0u;
		for (u_int u = 0; u < xMesh.GetNumBones(); ++u)
		{
			if (xMesh.m_xBones.Get(u).m_iParent == -1) { iRoot = (int)u; ++uRootCount; }
		}
		return (uRootCount == 1u) ? iRoot : -1;
	}

	// A curated FLOATER_PLANTOID subset: two 3-stage families spanning every evo
	// stage (F08 dandelion drifter + F30 updraft jellyfish). Filtered through
	// IsFloaterPlantoid, so it no-ops until FLOATER_PLANTOID is wired.
	const ZM_SPECIES_ID g_aeSubset[] =
	{
		ZM_SPECIES_PUFFSEED, ZM_SPECIES_DANDELIFT, ZM_SPECIES_ZEPHYRBLOOM,
		ZM_SPECIES_PUFFJEL,  ZM_SPECIES_NIMBJEL,   ZM_SPECIES_STRATOJEL,
	};
	constexpr u_int uSUBSET_COUNT = (u_int)(sizeof(g_aeSubset) / sizeof(g_aeSubset[0]));
}

// ############################################################################
// FLOATER-PLANTOID structural assert -- the expected bone set + the float
// ############################################################################

// Every FLOATER-PLANTOID build carries the canonical floating-plant skeleton:
// exactly one root which is a SPINE bone (the bulb), a HEAD crown, exactly SIX
// radial tendril roots ("Tendril..." bones), and NO ground legs (zero "...Up"
// bones). Additionally the whole mesh FLOATS -- its min-Y bound is strictly above
// the ground plane (y == 0). Runs mesh-only (ZM_BuildCreatureMesh) over every
// floater-plantoid.
ZENITH_TEST(ZM_Gen, FloaterPlantoid_ExpectedBoneSet)
{
	if (!HasFloaterPlantoidBuilder())
	{
		// FLOATER_PLANTOID not yet wired into the dispatch switch -- nothing to assert.
		return;
	}

	u_int uTested = 0u;
	for (u_int id = 0; id < (u_int)ZM_SPECIES_COUNT; ++id)
	{
		const ZM_SPECIES_ID eId = (ZM_SPECIES_ID)id;
		if (!IsFloaterPlantoid(eId)) { continue; }
		++uTested;

		const ZM_CreatureRecipe xRecipe = ZM_ResolveCreatureRecipe(eId);
		ZM_GenMesh xMesh;
		ZM_BuildCreatureMesh(xRecipe, xMesh);

		// Single spine root (the bulb).
		const int iRoot = SingleRootIndex(xMesh);
		ZENITH_ASSERT_GE(iRoot, 0, "species %u has no single root bone", id);
		if (iRoot >= 0)
		{
			ZENITH_ASSERT_TRUE(NameStartsWith(xMesh.m_xBones.Get((u_int)iRoot).m_szName, "Spine"),
				"species %u root bone '%s' is not a Spine bone", id,
				xMesh.m_xBones.Get((u_int)iRoot).m_szName);
		}

		// Spine trunk present.
		ZENITH_ASSERT_GE(CountBonesStartingWith(xMesh, "Spine"), 1u,
			"species %u has no spine bones", id);

		// Head crown present.
		ZENITH_ASSERT_GE(ZM_GenMeshFindBone(xMesh, "Head"), 0,
			"species %u has no 'Head' crown bone", id);

		// EXACTLY six radial tendril roots -- the fixed radial appendage count.
		ZENITH_ASSERT_EQ(CountBonesStartingWith(xMesh, "Tendril"), uEXPECTED_TENDRILS,
			"species %u does not have exactly %u radial ('Tendril...') bones", id, uEXPECTED_TENDRILS);

		// NO ground legs -- zero limb-upper ("...Up") bones (a floater has no legs).
		ZENITH_ASSERT_EQ(CountBonesEndingWith(xMesh, "Up"), 0u,
			"species %u has leg-root ('...Up') bones -- a floater must have none", id);

		// The body FLOATS: the mesh's min-Y bound is above the ground plane (y == 0),
		// i.e. no vertex touches or dips below the ground (off-origin, no-foot layout).
		const Zenith_Maths::Vector3 xMin = ZM_GenMeshBoundsMin(xMesh);
		ZENITH_ASSERT_TRUE(xMin.y > 0.0f,
			"species %u is not floating -- min-Y bound %f is not above the ground plane", id, xMin.y);
	}
	ZENITH_ASSERT_GT(uTested, 0u, "no FLOATER_PLANTOID species found -- structural gate vacuous");
}

// ############################################################################
// Universal harness over the FLOATER_PLANTOID species set -- mesh structure
// ############################################################################

// The universal mesh contract (winding / bounds / weights / <=2 influences / bone
// caps / single-rooted parent-before-child skeleton) holds for every
// floater-plantoid.
ZENITH_TEST(ZM_Gen, FloaterPlantoid_UniversalMeshStructure)
{
	if (!HasFloaterPlantoidBuilder())
	{
		return;
	}

	u_int uTested = 0u;
	for (u_int id = 0; id < (u_int)ZM_SPECIES_COUNT; ++id)
	{
		const ZM_SPECIES_ID eId = (ZM_SPECIES_ID)id;
		if (!IsFloaterPlantoid(eId)) { continue; }
		++uTested;

		const ZM_CreatureRecipe xRecipe = ZM_ResolveCreatureRecipe(eId);
		ZM_GenMesh xMesh;
		ZM_BuildCreatureMesh(xRecipe, xMesh);

		ZENITH_ASSERT_GT(xMesh.GetNumVerts(), 0u, "species %u mesh empty", id);
		ZENITH_ASSERT_GT(xMesh.GetNumTris(), 0u, "species %u has no triangles", id);

		const ZM_GenMeshValidation xVal = ZM_ValidateGenMesh(xMesh, uZM_GEN_CREATURE_BONE_CAP);
		ZENITH_ASSERT_TRUE(xVal.m_bWindingOutward, "species %u winding not outward (bad tri %u)", id, xVal.m_uFirstBadTriangle);
		ZENITH_ASSERT_TRUE(xVal.m_bBoundsNonDegen, "species %u degenerate bounds", id);
		ZENITH_ASSERT_TRUE(xVal.m_bWeightsSumToOne, "species %u weights do not sum to 1 (bad vert %u)", id, xVal.m_uFirstBadVertex);
		ZENITH_ASSERT_TRUE(xVal.m_bWeightsAtMostTwo, "species %u has >2 influences", id);
		ZENITH_ASSERT_TRUE(xVal.m_bBonesWithinCap, "species %u exceeds the creature bone cap", id);
		ZENITH_ASSERT_LE(xMesh.GetNumBones(), uZM_GEN_CREATURE_BONE_CAP, "species %u > 30 bones", id);

		const int iRoot = SingleRootIndex(xMesh);
		ZENITH_ASSERT_GE(iRoot, 0, "species %u lacks a single root", id);
	}
	ZENITH_ASSERT_GT(uTested, 0u, "no FLOATER_PLANTOID species found");
}

// ############################################################################
// Universal harness over the FLOATER_PLANTOID subset -- full bundle rollup + determinism
// ############################################################################

// Over the curated floater-plantoid subset, the full bundle passes the
// ZM_ValidateCreature rollup AND a same-seed re-build is byte-identical
// (ZM_CreatureBuildEqual + equal ZM_CreatureContentHash) -- the universal texture +
// determinism contract, confirmed for the FLOATER_PLANTOID archetype specifically.
ZENITH_TEST(ZM_Gen, FloaterPlantoid_UniversalBundleAndDeterminism)
{
	if (!HasFloaterPlantoidBuilder())
	{
		return;
	}

	u_int uTested = 0u;
	for (u_int i = 0; i < uSUBSET_COUNT; ++i)
	{
		const ZM_SPECIES_ID eId = g_aeSubset[i];
		if (!IsFloaterPlantoid(eId)) { continue; }
		++uTested;

		ZM_Creature xA;
		ZM_Creature xB;
		ZM_BuildCreature(eId, xA);
		ZM_BuildCreature(eId, xB);

		const ZM_CreatureValidation xVal = ZM_ValidateCreature(xA);
		ZENITH_ASSERT_TRUE(xVal.m_bAllValid, "floater-plantoid species %u failed the ZM_ValidateCreature rollup", (u_int)eId);
		ZENITH_ASSERT_TRUE(xVal.m_bShinyDiffers, "floater-plantoid species %u shiny == base", (u_int)eId);
		ZENITH_ASSERT_TRUE(xVal.m_bIconDistinctTexels, "floater-plantoid species %u icon lacks distinct texels", (u_int)eId);

		ZENITH_ASSERT_TRUE(ZM_CreatureBuildEqual(xA, xB),
			"floater-plantoid species %u re-build not byte-identical", (u_int)eId);
		ZENITH_ASSERT_EQ(ZM_CreatureContentHash(xA), ZM_CreatureContentHash(xB),
			"floater-plantoid species %u content hash diverged on re-build", (u_int)eId);
	}
	ZENITH_ASSERT_GT(uTested, 0u, "no FLOATER_PLANTOID subset species were buildable");
}
