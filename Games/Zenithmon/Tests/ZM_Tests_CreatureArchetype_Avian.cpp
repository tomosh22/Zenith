#include "Zenith.h"

// ============================================================================
// ZM_Tests_CreatureArchetype_Avian -- S4 AVIAN archetype gate (suite ZM_Gen).
// The generic contract harness lives in ZM_Tests_CreatureGen.cpp; this file is
// the per-archetype gate for the AVIAN builder (ZM_BuildArchetype_Avian). It:
//   * runs the universal creature contract over the AVIAN species set
//     (mesh structure + the ZM_ValidateCreature rollup + same-seed determinism), and
//   * adds the AVIAN-specific STRUCTURAL assert: the built skeleton carries the
//     expected bird bones -- a single spine ROOT, two WING roots, two LEG roots
//     (limb "...Up" bones from ZM_AppendLimb), and a HEAD -- located via the shared
//     ZM_CreatureArchetypeCommon kit naming convention (spine prefix "Spine", wing
//     bones prefix "Wing", the head bone "Head").
//
// PURE / HEADLESS: no disk, no GPU, no ZENITH_TOOLS. Authored against the frozen
// seam, NOT the builder .cpp. Every build guards on ZM_GetArchetypeBuilder != nullptr
// (via HasAvianBuilder), so this gate no-ops cleanly until the orchestrator wires
// AVIAN into the dispatch switch; the AVIAN species set is discovered dynamically.
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
	bool HasAvianBuilder()
	{
		return ZM_GetArchetypeBuilder(ZM_ARCHETYPE_AVIAN) != nullptr;
	}

	bool IsAvian(ZM_SPECIES_ID eId)
	{
		const ZM_SpeciesData& xRow = ZM_GetSpeciesData(eId);
		return xRow.m_eArchetype == ZM_ARCHETYPE_AVIAN
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

	// A curated AVIAN subset: two 3-stage families (spanning stages) and a 2-stage
	// family. Filtered through IsAvian, so it no-ops until AVIAN is wired.
	const ZM_SPECIES_ID g_aeSubset[] =
	{
		ZM_SPECIES_PIPWIT,   ZM_SPECIES_STRATAVIS, ZM_SPECIES_SQUALLET,
		ZM_SPECIES_THUNDEROC, ZM_SPECIES_SKIMMET,  ZM_SPECIES_PELAGAIR,
	};
	constexpr u_int uSUBSET_COUNT = (u_int)(sizeof(g_aeSubset) / sizeof(g_aeSubset[0]));
}

// ############################################################################
// AVIAN structural assert -- the expected bone set
// ############################################################################

// Every AVIAN build carries the canonical bird skeleton: exactly one root which is
// a SPINE bone, two WING roots ("Wing..." bones), at least two LEG roots (limb
// "...Up" bones from ZM_AppendLimb), and a HEAD -- all resolvable via the kit naming
// convention. Runs mesh-only (ZM_BuildCreatureMesh) over every avian.
ZENITH_TEST(ZM_Gen, Avian_ExpectedBoneSet)
{
	if (!HasAvianBuilder())
	{
		// AVIAN not yet wired into the dispatch switch -- nothing to assert.
		return;
	}

	u_int uTested = 0u;
	for (u_int id = 0; id < (u_int)ZM_SPECIES_COUNT; ++id)
	{
		const ZM_SPECIES_ID eId = (ZM_SPECIES_ID)id;
		if (!IsAvian(eId)) { continue; }
		++uTested;

		const ZM_CreatureRecipe xRecipe = ZM_ResolveCreatureRecipe(eId);
		ZM_GenMesh xMesh;
		ZM_BuildCreatureMesh(xRecipe, xMesh);

		// Single spine root.
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

		// Two wing roots.
		ZENITH_ASSERT_GE(CountBonesStartingWith(xMesh, "Wing"), 2u,
			"species %u has fewer than 2 wing ('Wing...') bones", id);

		// Two leg roots (limb uppers).
		ZENITH_ASSERT_GE(CountBonesEndingWith(xMesh, "Up"), 2u,
			"species %u has fewer than 2 leg-root ('...Up') bones", id);

		// Head.
		ZENITH_ASSERT_GE(ZM_GenMeshFindBone(xMesh, "Head"), 0,
			"species %u has no 'Head' bone", id);
	}
	ZENITH_ASSERT_GT(uTested, 0u, "no AVIAN species found -- structural gate vacuous");
}

// ############################################################################
// Universal harness over the AVIAN species set -- mesh structure
// ############################################################################

// The universal mesh contract (winding / bounds / weights / <=2 influences / bone
// caps / single-rooted parent-before-child skeleton) holds for every avian.
ZENITH_TEST(ZM_Gen, Avian_UniversalMeshStructure)
{
	if (!HasAvianBuilder())
	{
		return;
	}

	u_int uTested = 0u;
	for (u_int id = 0; id < (u_int)ZM_SPECIES_COUNT; ++id)
	{
		const ZM_SPECIES_ID eId = (ZM_SPECIES_ID)id;
		if (!IsAvian(eId)) { continue; }
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
	ZENITH_ASSERT_GT(uTested, 0u, "no AVIAN species found");
}

// ############################################################################
// Universal harness over the AVIAN subset -- full bundle rollup + determinism
// ############################################################################

// Over the curated avian subset, the full bundle passes the ZM_ValidateCreature
// rollup AND a same-seed re-build is byte-identical (ZM_CreatureBuildEqual + equal
// ZM_CreatureContentHash) -- the universal texture + determinism contract, confirmed
// for the AVIAN archetype specifically.
ZENITH_TEST(ZM_Gen, Avian_UniversalBundleAndDeterminism)
{
	if (!HasAvianBuilder())
	{
		return;
	}

	u_int uTested = 0u;
	for (u_int i = 0; i < uSUBSET_COUNT; ++i)
	{
		const ZM_SPECIES_ID eId = g_aeSubset[i];
		if (!IsAvian(eId)) { continue; }
		++uTested;

		ZM_Creature xA;
		ZM_Creature xB;
		ZM_BuildCreature(eId, xA);
		ZM_BuildCreature(eId, xB);

		const ZM_CreatureValidation xVal = ZM_ValidateCreature(xA);
		ZENITH_ASSERT_TRUE(xVal.m_bAllValid, "avian species %u failed the ZM_ValidateCreature rollup", (u_int)eId);
		ZENITH_ASSERT_TRUE(xVal.m_bShinyDiffers, "avian species %u shiny == base", (u_int)eId);
		ZENITH_ASSERT_TRUE(xVal.m_bIconDistinctTexels, "avian species %u icon lacks distinct texels", (u_int)eId);

		ZENITH_ASSERT_TRUE(ZM_CreatureBuildEqual(xA, xB),
			"avian species %u re-build not byte-identical", (u_int)eId);
		ZENITH_ASSERT_EQ(ZM_CreatureContentHash(xA), ZM_CreatureContentHash(xB),
			"avian species %u content hash diverged on re-build", (u_int)eId);
	}
	ZENITH_ASSERT_GT(uTested, 0u, "no AVIAN subset species were buildable");
}
