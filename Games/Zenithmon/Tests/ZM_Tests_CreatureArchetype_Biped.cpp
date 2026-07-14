#include "Zenith.h"

// ============================================================================
// ZM_Tests_CreatureArchetype_Biped -- S4 SC2 BIPED archetype gate (suite ZM_Gen).
// The generic contract harness lives in ZM_Tests_CreatureGen.cpp; this file is the
// per-archetype gate for the BIPED builder (ZM_BuildArchetype_Biped). It:
//   * runs the universal creature contract over the BIPED species set (mesh
//     structure + the ZM_ValidateCreature rollup + same-seed determinism), and
//   * adds the BIPED-specific STRUCTURAL assert: the built skeleton carries the
//     expected upright bones -- a single spine ROOT, TWO arm roots and TWO leg
//     roots (limb "...Up" bones from ZM_AppendLimb), and a HEAD -- located via
//     ZM_GenMeshFindBone on the shared ZM_CreatureArchetypeCommon kit naming
//     convention (arms "ArmL"/"ArmR", legs "LegL"/"LegR", spine prefix "Spine",
//     head bone "Head").
//
// PURE / HEADLESS: no disk, no GPU, no ZENITH_TOOLS. Authored against the frozen
// seam, NOT the builder .cpp. The BIPED species set is discovered dynamically
// (ZM_GetSpeciesData(id).m_eArchetype == BIPED with a non-null builder), so the
// gate tracks whatever bipeds ship -- and NO-OPs cleanly (guarded on a non-null
// builder) until the orchestrator wires ZM_GetArchetypeBuilder for BIPED.
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
	bool HasBipedBuilder()
	{
		return ZM_GetArchetypeBuilder(ZM_ARCHETYPE_BIPED) != nullptr;
	}

	bool IsBiped(ZM_SPECIES_ID eId)
	{
		const ZM_SpeciesData& xRow = ZM_GetSpeciesData(eId);
		return xRow.m_eArchetype == ZM_ARCHETYPE_BIPED
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

	// Count bones whose name both starts with szPrefix AND ends with szSuffix
	// (e.g. an arm root "ArmL".."ArmR" ending "Up").
	u_int CountBonesMatching(const ZM_GenMesh& xMesh, const char* szPrefix, const char* szSuffix)
	{
		u_int uCount = 0u;
		for (u_int u = 0; u < xMesh.GetNumBones(); ++u)
		{
			const char* szName = xMesh.m_xBones.Get(u).m_szName;
			if (NameStartsWith(szName, szPrefix) && NameEndsWith(szName, szSuffix)) { ++uCount; }
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

	// A curated subset for the full-bundle + determinism pass: two 3-stage BIPED
	// families (F02 kiln salamander, F15 blindfolded seer), one member of a 2-stage
	// family (F50 clockwork oracle) and a mid-dex 3-stage family (F37 quarry golem).
	const ZM_SPECIES_ID g_aeSubset[] =
	{
		ZM_SPECIES_KINDLET,   ZM_SPECIES_PYROCLAST, ZM_SPECIES_TRANCET,
		ZM_SPECIES_ORACLYNE,  ZM_SPECIES_PEBBLEFIST, ZM_SPECIES_COGLING,
	};
	constexpr u_int uSUBSET_COUNT = (u_int)(sizeof(g_aeSubset) / sizeof(g_aeSubset[0]));
}

// ############################################################################
// BIPED structural assert -- the expected bone set
// ############################################################################

// Every BIPED build carries the canonical upright skeleton: exactly one root which
// is a SPINE bone, TWO arm roots (ArmL/ArmR "...Up" bones from ZM_AppendLimb), TWO
// leg roots (LegL/LegR "...Up" bones), and a HEAD -- all resolvable via the kit
// naming convention. Runs mesh-only (ZM_BuildCreatureMesh) over every biped.
ZENITH_TEST(ZM_Gen, Biped_ExpectedBoneSet)
{
	if (!HasBipedBuilder()) { return; }   // no-op cleanly until BIPED is wired
	u_int uTested = 0u;
	for (u_int id = 0; id < (u_int)ZM_SPECIES_COUNT; ++id)
	{
		const ZM_SPECIES_ID eId = (ZM_SPECIES_ID)id;
		if (!IsBiped(eId)) { continue; }
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

		// Two arm roots + two leg roots (limb uppers).
		ZENITH_ASSERT_EQ(CountBonesMatching(xMesh, "Arm", "Up"), 2u,
			"species %u does not have exactly 2 arm-root ('Arm...Up') bones", id);
		ZENITH_ASSERT_EQ(CountBonesMatching(xMesh, "Leg", "Up"), 2u,
			"species %u does not have exactly 2 leg-root ('Leg...Up') bones", id);

		// Four limb roots total (the universal "...Up" count).
		ZENITH_ASSERT_GE(CountBonesEndingWith(xMesh, "Up"), 4u,
			"species %u has fewer than 4 limb-root ('...Up') bones", id);

		// Head.
		ZENITH_ASSERT_GE(ZM_GenMeshFindBone(xMesh, "Head"), 0,
			"species %u has no 'Head' bone", id);
	}
	ZENITH_ASSERT_GT(uTested, 0u, "no BIPED species found -- structural gate vacuous");
}

// ############################################################################
// Universal harness over the BIPED species set -- mesh structure
// ############################################################################

// The universal mesh contract (winding / bounds / weights / <=2 influences / bone
// caps / single-rooted parent-before-child skeleton) holds for every biped.
ZENITH_TEST(ZM_Gen, Biped_UniversalMeshStructure)
{
	if (!HasBipedBuilder()) { return; }   // no-op cleanly until BIPED is wired
	u_int uTested = 0u;
	for (u_int id = 0; id < (u_int)ZM_SPECIES_COUNT; ++id)
	{
		const ZM_SPECIES_ID eId = (ZM_SPECIES_ID)id;
		if (!IsBiped(eId)) { continue; }
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
	ZENITH_ASSERT_GT(uTested, 0u, "no BIPED species found");
}

// ############################################################################
// Universal harness over the BIPED subset -- full bundle rollup + determinism
// ############################################################################

// Over the curated biped subset, the full bundle passes the ZM_ValidateCreature
// rollup AND a same-seed re-build is byte-identical (ZM_CreatureBuildEqual + equal
// ZM_CreatureContentHash) -- the universal texture + determinism contract, confirmed
// for the BIPED archetype specifically.
ZENITH_TEST(ZM_Gen, Biped_UniversalBundleAndDeterminism)
{
	if (!HasBipedBuilder()) { return; }   // no-op cleanly until BIPED is wired
	u_int uTested = 0u;
	for (u_int i = 0; i < uSUBSET_COUNT; ++i)
	{
		const ZM_SPECIES_ID eId = g_aeSubset[i];
		if (!IsBiped(eId)) { continue; }
		++uTested;

		ZM_Creature xA;
		ZM_Creature xB;
		ZM_BuildCreature(eId, xA);
		ZM_BuildCreature(eId, xB);

		const ZM_CreatureValidation xVal = ZM_ValidateCreature(xA);
		ZENITH_ASSERT_TRUE(xVal.m_bAllValid, "biped species %u failed the ZM_ValidateCreature rollup", (u_int)eId);
		ZENITH_ASSERT_TRUE(xVal.m_bShinyDiffers, "biped species %u shiny == base", (u_int)eId);
		ZENITH_ASSERT_TRUE(xVal.m_bIconDistinctTexels, "biped species %u icon lacks distinct texels", (u_int)eId);

		ZENITH_ASSERT_TRUE(ZM_CreatureBuildEqual(xA, xB),
			"biped species %u re-build not byte-identical", (u_int)eId);
		ZENITH_ASSERT_EQ(ZM_CreatureContentHash(xA), ZM_CreatureContentHash(xB),
			"biped species %u content hash diverged on re-build", (u_int)eId);
	}
	ZENITH_ASSERT_GT(uTested, 0u, "no BIPED subset species were buildable");
}
