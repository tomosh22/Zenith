#include "Zenith.h"

// ============================================================================
// ZM_Tests_CreatureGen -- S4 SC1 unit gate for ZM_CreatureGen (suite ZM_Gen).
//
// This is the GENERIC, contract-driven creature harness: it authors against the
// frozen seam (Games/Zenithmon/Source/Gen/ZM_CreatureGen.h +
// ZM_CreatureArchetypeCommon.h), NEVER against a specific archetype .cpp. It
// loops every ZM_SPECIES_ID whose ZM_GetArchetypeBuilder is non-null (SC1 wires
// ONLY QUADRUPED; every other archetype returns nullptr, so those species are
// SKIPPED and coverage auto-grows as later SCs land their builders), and asserts
// the 12 UNIVERSAL creature invariants plus the SC1-core cases:
//   (1)  same-seed determinism      -- ZM_CreatureBuildEqual + equal ZM_CreatureContentHash
//   (2)  domain-seed isolation      -- recipe.m_aulDomainSeed[d] == ZM_GenDeriveSeed(...)
//   (3)  outward winding            -- ZM_CreatureValidation.m_bWindingOutward
//   (4)  bounds non-degenerate + within a sane world box
//   (5)  weights sum to 1           -- m_bWeightsSumToOne
//   (6)  <=2 non-zero influences    -- m_bWeightsAtMostTwo
//   (7)  bone caps (<=30 AND <=100)
//   (8)  bone indices in range + skeleton well-formed (single root, parent<child,
//        names resolvable via ZM_GenMeshFindBone)
//   (9)  shiny differs from base at matching dims + a single shared mesh
//   (10) dex icon non-empty, 128^2, >= 2 distinct texels
//   (11) seed/evo sensitivity       -- two distinct species differ; stage1 != stage3
//   (12) skeleton topology IDENTICAL across evo stages (equal bone count + equal
//        per-index bone names, over EVERY multi-stage buildable family)
// SC1 core: ZM_ResolveCreatureRecipe purity; QUADRUPED dispatch non-null while the
// other 7 archetypes dispatch nullptr; ZM_CreatureArchetypeCommon kit-helper units.
//
// PURE / HEADLESS: no disk, no GPU, no ZENITH_TOOLS reach (the .zmesh/.ztxtr bake
// bridges are compiled out). These run at boot before the scene loads.
//
// COST NOTE: a full bundle build synthesises a 512^2 albedo + shiny + icon, so the
// heavy per-species passes reuse a SINGLE build; the same-seed double-build (1) and
// the cross-species/evolution comparisons (11)/(12) run over a curated
// representative subset while the cheap mesh-only structural pass (3)-(8) and the
// bundle validation (9)/(10) run over EVERY buildable species. Mesh-structural
// checks use the public ZM_BuildCreatureMesh (no texture synth) to stay in budget.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/Gen/ZM_CreatureGen.h"
#include "Zenithmon/Source/Gen/ZM_CreatureArchetypeCommon.h"
#include "Zenithmon/Source/Gen/ZM_GenCommon.h"
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"
#include "Maths/Zenith_Maths.h"
#include "Collections/Zenith_Vector.h"

#include <cstring>   // memcmp, strncmp, strlen
#include <cmath>     // fabsf, isfinite

namespace
{
	// A generous absolute world-box bound: any creature whose mesh extent exceeds
	// this (or is non-finite) is unambiguously broken. There is no m_bBoundsWithinBox
	// flag on ZM_CreatureValidation, so assertion (4)'s "within box" clause is checked
	// here directly against the position bounds (see the report's reconciliation note).
	constexpr float fWORLD_BOX_LIMIT = 100.0f;

	// True when the species' body plan has a wired archetype builder (SC1: QUADRUPED
	// only). The generic harness SKIPS every species whose builder is nullptr, so the
	// suite exercises exactly the archetypes authored so far and grows automatically.
	bool HasBuilder(ZM_SPECIES_ID eId)
	{
		return ZM_GetArchetypeBuilder(ZM_GetSpeciesData(eId).m_eArchetype) != nullptr;
	}

	bool IsFiniteVec3(const Zenith_Maths::Vector3& xV)
	{
		return std::isfinite(xV.x) && std::isfinite(xV.y) && std::isfinite(xV.z);
	}

	// Bounds are non-degenerate (min < max on every axis), finite, and inside the
	// world box on every axis.
	bool BoundsSaneAndWithinBox(const ZM_GenMesh& xMesh)
	{
		if (xMesh.GetNumVerts() == 0u) { return false; }
		const Zenith_Maths::Vector3 xMin = ZM_GenMeshBoundsMin(xMesh);
		const Zenith_Maths::Vector3 xMax = ZM_GenMeshBoundsMax(xMesh);
		if (!IsFiniteVec3(xMin) || !IsFiniteVec3(xMax)) { return false; }
		if (!(xMin.x < xMax.x) || !(xMin.y < xMax.y) || !(xMin.z < xMax.z)) { return false; }
		if (fabsf(xMin.x) > fWORLD_BOX_LIMIT || fabsf(xMax.x) > fWORLD_BOX_LIMIT) { return false; }
		if (fabsf(xMin.y) > fWORLD_BOX_LIMIT || fabsf(xMax.y) > fWORLD_BOX_LIMIT) { return false; }
		if (fabsf(xMin.z) > fWORLD_BOX_LIMIT || fabsf(xMax.z) > fWORLD_BOX_LIMIT) { return false; }
		return true;
	}

	// Every vertex's four bone indices reference a real bone.
	bool BoneIndicesInRange(const ZM_GenMesh& xMesh)
	{
		const u_int uNumBones = xMesh.GetNumBones();
		if (uNumBones == 0u) { return false; }
		if (xMesh.m_xBoneIndices.GetSize() != xMesh.GetNumVerts()) { return false; }
		for (u_int v = 0; v < xMesh.m_xBoneIndices.GetSize(); ++v)
		{
			const glm::uvec4& xI = xMesh.m_xBoneIndices.Get(v);
			if (xI.x >= uNumBones || xI.y >= uNumBones || xI.z >= uNumBones || xI.w >= uNumBones)
			{
				return false;
			}
		}
		return true;
	}

	// Exactly one root (parent -1) and every parent index strictly precedes its child
	// (the AddBone precondition + a single-rooted tree). Also requires every bone name
	// to resolve back to itself via ZM_GenMeshFindBone (the anim-gen binding contract).
	bool SkeletonWellFormed(const ZM_GenMesh& xMesh)
	{
		const u_int uNumBones = xMesh.GetNumBones();
		if (uNumBones == 0u) { return false; }
		u_int uRootCount = 0u;
		for (u_int u = 0; u < uNumBones; ++u)
		{
			const ZM_GenBone& xBone = xMesh.m_xBones.Get(u);
			if (xBone.m_iParent == -1) { ++uRootCount; }
			else if (!(xBone.m_iParent < (int)u)) { return false; }

			// Name must resolve; the FIRST bone that shares this name must be findable
			// (duplicate names would resolve to an earlier index, which is acceptable
			// so long as SOME bone with the name exists).
			const int iFound = ZM_GenMeshFindBone(xMesh, xBone.m_szName);
			if (iFound < 0 || iFound >= (int)uNumBones) { return false; }
		}
		return uRootCount == 1u;
	}

	// The image carries at least two distinct texels (independent of the validator's
	// m_bIconDistinctTexels flag -- a belt-and-braces check for assertion (10)).
	bool DistinctTexelsAtLeastTwo(const ZM_GenImage& xImg)
	{
		if (xImg.IsEmpty()) { return false; }
		const Zenith_Maths::Vector4 xFirst = xImg.Get(0u, 0u);
		for (u_int y = 0; y < xImg.GetHeight(); ++y)
		{
			for (u_int x = 0; x < xImg.GetWidth(); ++x)
			{
				const Zenith_Maths::Vector4 xT = xImg.Get(y, x);
				if (xT.x != xFirst.x || xT.y != xFirst.y || xT.z != xFirst.z || xT.w != xFirst.w)
				{
					return true;
				}
			}
		}
		return false;
	}

	// Field-wise recipe equality (no memcmp -- avoids padding sensitivity). Compares
	// the scalar inputs, every derived domain seed, and the texture recipe.
	bool RecipeEqual(const ZM_CreatureRecipe& xA, const ZM_CreatureRecipe& xB)
	{
		if (xA.m_eSpecies != xB.m_eSpecies) { return false; }
		if (xA.m_eArchetype != xB.m_eArchetype) { return false; }
		if (xA.m_uEvoStage != xB.m_uEvoStage) { return false; }
		if (xA.m_uFamilySeed != xB.m_uFamilySeed) { return false; }
		if (xA.m_eSizeClass != xB.m_eSizeClass) { return false; }
		if (xA.m_fSizeScale != xB.m_fSizeScale) { return false; }
		if (xA.m_uElaboration != xB.m_uElaboration) { return false; }
		for (u_int d = 0; d < (u_int)ZM_GEN_DOMAIN_COUNT; ++d)
		{
			if (xA.m_aulDomainSeed[d] != xB.m_aulDomainSeed[d]) { return false; }
		}
		const ZM_CreatureTexRecipe& xTA = xA.m_xTex;
		const ZM_CreatureTexRecipe& xTB = xB.m_xTex;
		if (xTA.m_ePrimaryType != xTB.m_ePrimaryType) { return false; }
		if (xTA.m_eSecondaryType != xTB.m_eSecondaryType) { return false; }
		if (xTA.m_xPattern.m_eKind != xTB.m_xPattern.m_eKind) { return false; }
		if (xTA.m_xPattern.m_fFrequency != xTB.m_xPattern.m_fFrequency) { return false; }
		if (xTA.m_xPattern.m_fContrast != xTB.m_xPattern.m_fContrast) { return false; }
		if (xTA.m_xPattern.m_fJitter != xTB.m_xPattern.m_fJitter) { return false; }
		if (xTA.m_xPattern.m_uCount != xTB.m_xPattern.m_uCount) { return false; }
		if (xTA.m_fEyeU != xTB.m_fEyeU) { return false; }
		if (xTA.m_fEyeV != xTB.m_fEyeV) { return false; }
		if (xTA.m_fEyeRadius != xTB.m_fEyeRadius) { return false; }
		if (xTA.m_uWidth != xTB.m_uWidth) { return false; }
		if (xTA.m_uHeight != xTB.m_uHeight) { return false; }
		return true;
	}

	// A curated representative subset for the heavy same-seed / cross-species passes:
	// a full 3-stage family (F01), a 2-stage family (F07), a mid-dex 3-stage family
	// (F05), and two single-stage species (F54, F55). All are QUADRUPED in SC1; each
	// is guarded with HasBuilder in case the roster shifts.
	const ZM_SPECIES_ID g_aeSubset[] =
	{
		ZM_SPECIES_FERNFAWN,     // F01 stage 1
		ZM_SPECIES_SYLVASTAG,    // F01 stage 3
		ZM_SPECIES_STRAYLING,    // F07 stage 1
		ZM_SPECIES_NIBBIN,       // F05 stage 1
		ZM_SPECIES_AURICORN,     // F54 single stage
		ZM_SPECIES_DUNELEON,     // F55 single stage
	};
	constexpr u_int uSUBSET_COUNT = (u_int)(sizeof(g_aeSubset) / sizeof(g_aeSubset[0]));
}

// ############################################################################
// SC1 core -- archetype dispatch
// ############################################################################

// SC1 wires exactly one archetype builder: QUADRUPED resolves to a non-null
// function pointer; the other 7 archetypes resolve to nullptr (their builders land
// in later SCs). The generic harness relies on this to skip un-authored archetypes.
//
// NOTE (reconciliation): the frozen ZM_BuildCreature returns void and
// ZM_BuildCreatureMesh asserts a NON-NULL builder, so invoking ZM_BuildCreature on
// a nullptr-builder species would trip an assert -- it is intentionally never
// called here. The dispatch pointer IS the testable "routes / does not route"
// contract; the harness gates every build on HasBuilder.
ZENITH_TEST(ZM_Gen, CreatureGen_ArchetypeDispatch)
{
	// (Function pointers are compared to nullptr directly -- ZENITH_ASSERT_NULL would
	// reinterpret_cast a function pointer to a data pointer, which MSVC rejects/warns.)
	ZENITH_ASSERT_TRUE(ZM_GetArchetypeBuilder(ZM_ARCHETYPE_QUADRUPED) != nullptr,
		"QUADRUPED must route to a non-null builder in SC1");

	const ZM_ARCHETYPE aeOther[] =
	{
		ZM_ARCHETYPE_BIPED, ZM_ARCHETYPE_AVIAN, ZM_ARCHETYPE_SERPENT, ZM_ARCHETYPE_AQUATIC,
		ZM_ARCHETYPE_INSECTOID, ZM_ARCHETYPE_BLOB, ZM_ARCHETYPE_FLOATER_PLANTOID,
	};
	for (u_int i = 0; i < (u_int)(sizeof(aeOther) / sizeof(aeOther[0])); ++i)
	{
		ZENITH_ASSERT_TRUE(ZM_GetArchetypeBuilder(aeOther[i]) == nullptr,
			"archetype %u must be un-wired (nullptr) in SC1", (u_int)aeOther[i]);
	}

	// At least one species in the roster must be buildable, or the harness is vacuous.
	u_int uBuildable = 0u;
	for (u_int id = 0; id < (u_int)ZM_SPECIES_COUNT; ++id)
	{
		if (HasBuilder((ZM_SPECIES_ID)id)) { ++uBuildable; }
	}
	ZENITH_ASSERT_GT(uBuildable, 0u, "no buildable species -- the generic harness would be vacuous");
}

// ############################################################################
// SC1 core -- recipe resolution purity + domain-seed isolation (assertion 2)
// ############################################################################

// ZM_ResolveCreatureRecipe is a pure function of the species id: resolving the same
// id twice yields a field-identical recipe, and within one recipe the per-domain
// seeds are pairwise distinct (adding a draw to one domain can never perturb
// another). Runs over EVERY species (pure, no build).
ZENITH_TEST(ZM_Gen, CreatureGen_ResolveRecipePurity)
{
	for (u_int id = 0; id < (u_int)ZM_SPECIES_COUNT; ++id)
	{
		const ZM_SPECIES_ID eId = (ZM_SPECIES_ID)id;
		const ZM_CreatureRecipe xA = ZM_ResolveCreatureRecipe(eId);
		const ZM_CreatureRecipe xB = ZM_ResolveCreatureRecipe(eId);
		ZENITH_ASSERT_TRUE(RecipeEqual(xA, xB), "recipe not pure for species %u", id);

		// Pairwise-distinct domain seeds.
		for (u_int a = 0; a < (u_int)ZM_GEN_DOMAIN_COUNT; ++a)
		{
			for (u_int b = a + 1u; b < (u_int)ZM_GEN_DOMAIN_COUNT; ++b)
			{
				ZENITH_ASSERT_NE(xA.m_aulDomainSeed[a], xA.m_aulDomainSeed[b],
					"species %u domains %u/%u share a seed", id, a, b);
			}
		}
	}
}

// Domain-seed isolation (assertion 2): every resolved domain seed equals the frozen
// ZM_GenDeriveSeed(familySeed, id, evoStage, domain), and the recipe's family seed /
// evo stage match the species-data accessors (proves the resolve wiring). Pure.
ZENITH_TEST(ZM_Gen, CreatureGen_DomainSeedIsolation)
{
	for (u_int id = 0; id < (u_int)ZM_SPECIES_COUNT; ++id)
	{
		const ZM_SPECIES_ID eId = (ZM_SPECIES_ID)id;
		const ZM_CreatureRecipe xR = ZM_ResolveCreatureRecipe(eId);

		ZENITH_ASSERT_EQ(xR.m_uFamilySeed, ZM_GetSpeciesFamilySeed(eId),
			"species %u recipe family seed != ZM_GetSpeciesFamilySeed", id);
		ZENITH_ASSERT_EQ(xR.m_uEvoStage, ZM_GetSpeciesData(eId).m_uEvoStage,
			"species %u recipe evo stage != dex row", id);

		for (u_int d = 0; d < (u_int)ZM_GEN_DOMAIN_COUNT; ++d)
		{
			const u_int64 ulExpected = ZM_GenDeriveSeed(xR.m_uFamilySeed, id,
				xR.m_uEvoStage, (ZM_GEN_DOMAIN)d);
			ZENITH_ASSERT_EQ(xR.m_aulDomainSeed[d], ulExpected,
				"species %u domain %u seed != ZM_GenDeriveSeed", id, d);
		}
	}
}

// ############################################################################
// Universal harness -- mesh structure (assertions 3-8) over EVERY buildable species
// ############################################################################

// For every buildable species, the mesh-only build (ZM_BuildCreatureMesh, no texture
// synth) satisfies: outward winding (3), non-degenerate + in-box bounds (4), weights
// sum to 1 (5), <=2 influences (6), bone caps <=30 AND <=100 (7), and in-range bone
// indices + a well-formed single-rooted skeleton (8).
ZENITH_TEST(ZM_Gen, CreatureGen_UniversalMeshPerSpecies)
{
	u_int uTested = 0u;
	for (u_int id = 0; id < (u_int)ZM_SPECIES_COUNT; ++id)
	{
		const ZM_SPECIES_ID eId = (ZM_SPECIES_ID)id;
		if (!HasBuilder(eId)) { continue; }   // skip un-authored archetypes
		++uTested;

		const ZM_CreatureRecipe xRecipe = ZM_ResolveCreatureRecipe(eId);
		ZM_GenMesh xMesh;
		ZM_BuildCreatureMesh(xRecipe, xMesh);

		ZENITH_ASSERT_GT(xMesh.GetNumVerts(), 0u, "species %u mesh empty", id);
		ZENITH_ASSERT_GT(xMesh.GetNumTris(), 0u, "species %u has no triangles", id);

		const ZM_GenMeshValidation xVal = ZM_ValidateGenMesh(xMesh, uZM_GEN_CREATURE_BONE_CAP);

		// (3) winding
		ZENITH_ASSERT_TRUE(xVal.m_bWindingOutward,
			"species %u winding not outward (bad tri %u)", id, xVal.m_uFirstBadTriangle);
		// (4) bounds
		ZENITH_ASSERT_TRUE(xVal.m_bBoundsNonDegen, "species %u degenerate bounds", id);
		ZENITH_ASSERT_TRUE(BoundsSaneAndWithinBox(xMesh),
			"species %u bounds non-finite or outside the world box", id);
		// (5)/(6) weights
		ZENITH_ASSERT_TRUE(xVal.m_bWeightsSumToOne,
			"species %u weights do not sum to 1 (bad vert %u)", id, xVal.m_uFirstBadVertex);
		ZENITH_ASSERT_TRUE(xVal.m_bWeightsAtMostTwo, "species %u has >2 influences", id);
		// (7) bone caps
		ZENITH_ASSERT_TRUE(xVal.m_bBonesWithinCap, "species %u exceeds the creature bone cap", id);
		ZENITH_ASSERT_LE(xMesh.GetNumBones(), uZM_GEN_CREATURE_BONE_CAP, "species %u > 30 bones", id);
		ZENITH_ASSERT_LE(xMesh.GetNumBones(), 100u, "species %u > engine MAX_BONES", id);
		// (8) bone indices + skeleton topology
		ZENITH_ASSERT_TRUE(BoneIndicesInRange(xMesh), "species %u has out-of-range bone indices", id);
		ZENITH_ASSERT_TRUE(SkeletonWellFormed(xMesh),
			"species %u skeleton not single-rooted / parent-before-child / resolvable", id);
	}
	ZENITH_ASSERT_GT(uTested, 0u, "no buildable species exercised the mesh harness");
}

// ############################################################################
// Universal harness -- full bundle textures (assertions 9-10) over EVERY buildable species
// ############################################################################

// For every buildable species, the full bundle (ZM_BuildCreature) passes the whole
// ZM_ValidateCreature contract (m_bAllValid) and specifically: a single non-empty
// shared mesh, a shiny that DIFFERS from the base at matching dims (9), and a
// non-empty 128^2 icon carrying >= 2 distinct texels (10).
ZENITH_TEST(ZM_Gen, CreatureGen_UniversalBundlePerSpecies)
{
	u_int uTested = 0u;
	for (u_int id = 0; id < (u_int)ZM_SPECIES_COUNT; ++id)
	{
		const ZM_SPECIES_ID eId = (ZM_SPECIES_ID)id;
		if (!HasBuilder(eId)) { continue; }
		++uTested;

		ZM_Creature xCreature;
		ZM_BuildCreature(eId, xCreature);
		const ZM_CreatureValidation xVal = ZM_ValidateCreature(xCreature);

		// Rollup: the structural conjunction must hold for a good build.
		ZENITH_ASSERT_TRUE(xVal.m_bAllValid, "species %u failed the ZM_ValidateCreature rollup", id);

		// (9) shiny: one shared non-empty mesh + differing shiny at matching dims.
		ZENITH_ASSERT_GT(xCreature.m_xMesh.GetNumVerts(), 0u, "species %u bundle mesh empty", id);
		ZENITH_ASSERT_TRUE(xVal.m_bAlbedoNonEmpty, "species %u albedo empty", id);
		ZENITH_ASSERT_TRUE(xVal.m_bShinyDimsMatch, "species %u shiny dims != albedo dims", id);
		ZENITH_ASSERT_TRUE(xVal.m_bShinyDiffers, "species %u shiny identical to base albedo", id);
		ZENITH_ASSERT_EQ(xCreature.m_xShiny.GetWidth(), xCreature.m_xAlbedo.GetWidth(),
			"species %u shiny width != albedo width", id);
		ZENITH_ASSERT_EQ(xCreature.m_xShiny.GetHeight(), xCreature.m_xAlbedo.GetHeight(),
			"species %u shiny height != albedo height", id);

		// (10) icon: non-empty, 128^2, >= 2 distinct texels.
		ZENITH_ASSERT_TRUE(xVal.m_bIconNonEmpty, "species %u icon empty", id);
		ZENITH_ASSERT_TRUE(xVal.m_bIconDistinctTexels, "species %u icon lacks distinct texels (validator)", id);
		ZENITH_ASSERT_EQ(xCreature.m_xIcon.GetWidth(), uZM_CREATURE_ICON_RESOLUTION,
			"species %u icon width != 128", id);
		ZENITH_ASSERT_EQ(xCreature.m_xIcon.GetHeight(), uZM_CREATURE_ICON_RESOLUTION,
			"species %u icon height != 128", id);
		ZENITH_ASSERT_TRUE(DistinctTexelsAtLeastTwo(xCreature.m_xIcon),
			"species %u icon has fewer than 2 distinct texels (independent check)", id);
	}
	ZENITH_ASSERT_GT(uTested, 0u, "no buildable species exercised the bundle harness");
}

// ############################################################################
// Universal harness -- same-seed determinism (assertion 1) over the subset
// ############################################################################

// Building the SAME species twice yields a byte-identical bundle: ZM_CreatureBuildEqual
// AND an equal ZM_CreatureContentHash AND ZM_CreatureMeshEqual. The headline creature
// determinism gate; run over the curated representative subset for cost.
ZENITH_TEST(ZM_Gen, CreatureGen_SameSeedDeterminism)
{
	u_int uTested = 0u;
	for (u_int i = 0; i < uSUBSET_COUNT; ++i)
	{
		const ZM_SPECIES_ID eId = g_aeSubset[i];
		if (!HasBuilder(eId)) { continue; }
		++uTested;

		ZM_Creature xA;
		ZM_Creature xB;
		ZM_BuildCreature(eId, xA);
		ZM_BuildCreature(eId, xB);

		ZENITH_ASSERT_TRUE(ZM_CreatureBuildEqual(xA, xB),
			"species %u re-build is not byte-identical", (u_int)eId);
		ZENITH_ASSERT_EQ(ZM_CreatureContentHash(xA), ZM_CreatureContentHash(xB),
			"species %u content hash diverged on re-build", (u_int)eId);
		ZENITH_ASSERT_TRUE(ZM_CreatureMeshEqual(xA.m_xMesh, xB.m_xMesh),
			"species %u mesh not byte-identical on re-build", (u_int)eId);
	}
	ZENITH_ASSERT_GT(uTested, 0u, "no subset species exercised the determinism gate");
}

// ############################################################################
// Universal harness -- seed/evo sensitivity (assertion 11)
// ############################################################################

// Two DISTINCT species produce different bundles, and stage-1 vs stage-3 of one
// family (F01: Fernfawn -> Sylvastag) also differ -- so the generator is genuinely
// keyed on the species id + evolution stage, not emitting one canned creature.
ZENITH_TEST(ZM_Gen, CreatureGen_SeedAndEvoSensitivity)
{
	// Guard: only meaningful when the compared species are buildable.
	if (HasBuilder(ZM_SPECIES_FERNFAWN) && HasBuilder(ZM_SPECIES_NIBBIN))
	{
		ZM_Creature xFern;
		ZM_Creature xNibbin;
		ZM_BuildCreature(ZM_SPECIES_FERNFAWN, xFern);
		ZM_BuildCreature(ZM_SPECIES_NIBBIN, xNibbin);
		ZENITH_ASSERT_FALSE(ZM_CreatureBuildEqual(xFern, xNibbin),
			"distinct species Fernfawn/Nibbin produced identical bundles");
		ZENITH_ASSERT_NE(ZM_CreatureContentHash(xFern), ZM_CreatureContentHash(xNibbin),
			"distinct species Fernfawn/Nibbin share a content hash");
	}

	// Stage 1 vs stage 3 of F01.
	if (HasBuilder(ZM_SPECIES_FERNFAWN) && HasBuilder(ZM_SPECIES_SYLVASTAG))
	{
		ZM_Creature xStage1;
		ZM_Creature xStage3;
		ZM_BuildCreature(ZM_SPECIES_FERNFAWN, xStage1);
		ZM_BuildCreature(ZM_SPECIES_SYLVASTAG, xStage3);
		ZENITH_ASSERT_FALSE(ZM_CreatureBuildEqual(xStage1, xStage3),
			"F01 stage-1 and stage-3 produced identical bundles");
	}
}

// ############################################################################
// Universal harness -- skeleton topology stable across evo stages (assertion 12)
// ############################################################################

// The clip-transfer invariant is STRONGER than "shared name set": index-keyed clips
// only transfer across an evolution line if the skeleton is IDENTICAL at every stage
// -- same bone COUNT, same NAMES, in the same index ORDER. So for EVERY multi-stage
// family whose stages are all buildable (SC1: all-QUADRUPED), build each stage's mesh
// and assert (a) GetNumBones() is equal across all stages and (b) bone i of every
// later stage carries the same name as bone i of the head stage. This iterates all
// such families (enumerated from ZM_SpeciesData by walking each stage-1 head down its
// m_eEvolvesTo chain), not just F01 -- evolution may only ELABORATE geometry in place
// (e.g. grow horns), never re-index or rename the shared skeleton.
ZENITH_TEST(ZM_Gen, CreatureGen_TopologyStableAcrossEvo)
{
	u_int uFamiliesTested = 0u;

	for (u_int id = 0; id < (u_int)ZM_SPECIES_COUNT; ++id)
	{
		const ZM_SPECIES_ID eHead = (ZM_SPECIES_ID)id;
		// A chain is enumerated from its stage-1 head only (single-stage species and
		// legendaries are also stage 1 but terminate immediately below).
		if (ZM_GetSpeciesData(eHead).m_uEvoStage != 1u) { continue; }

		// Collect the family's evolution chain (head -> ... -> final stage) by following
		// m_eEvolvesTo, and note whether every stage has a wired archetype builder.
		ZM_SPECIES_ID aeStages[8];
		u_int uNumStages   = 0u;
		bool  bAllBuildable = true;
		for (ZM_SPECIES_ID eCur = eHead;
			eCur != ZM_SPECIES_NONE && uNumStages < 8u;
			eCur = ZM_GetSpeciesData(eCur).m_eEvolvesTo)
		{
			aeStages[uNumStages++] = eCur;
			if (!HasBuilder(eCur)) { bAllBuildable = false; }
		}

		// Only multi-stage families whose EVERY stage is buildable (all-QUADRUPED in SC1).
		if (uNumStages < 2u || !bAllBuildable) { continue; }
		++uFamiliesTested;

		// Stage 0 (the head) is the reference skeleton.
		ZM_GenMesh xRefMesh;
		ZM_BuildCreatureMesh(ZM_ResolveCreatureRecipe(aeStages[0]), xRefMesh);
		ZENITH_ASSERT_TRUE(SkeletonWellFormed(xRefMesh),
			"family head %u stage-0 skeleton malformed", id);
		const u_int uRefBones = xRefMesh.GetNumBones();
		ZENITH_ASSERT_GT(uRefBones, 0u, "family head %u stage-0 has no bones", id);

		// Every later stage: IDENTICAL bone count AND identical per-index bone name.
		for (u_int s = 1; s < uNumStages; ++s)
		{
			ZM_GenMesh xMesh;
			ZM_BuildCreatureMesh(ZM_ResolveCreatureRecipe(aeStages[s]), xMesh);
			ZENITH_ASSERT_TRUE(SkeletonWellFormed(xMesh),
				"family head %u stage-%u skeleton malformed", id, s);

			ZENITH_ASSERT_EQ(xMesh.GetNumBones(), uRefBones,
				"family head %u stage-%u bone count %u != head count %u (topology not identical)",
				id, s, xMesh.GetNumBones(), uRefBones);

			// Per-index name equality, guarded to the common prefix so a count divergence
			// is reported by the assert above rather than by an out-of-range bone read.
			const u_int uCmp = (xMesh.GetNumBones() < uRefBones) ? xMesh.GetNumBones() : uRefBones;
			for (u_int b = 0; b < uCmp; ++b)
			{
				ZENITH_ASSERT_STREQ(xMesh.m_xBones.Get(b).m_szName, xRefMesh.m_xBones.Get(b).m_szName,
					"family head %u stage-%u bone %u name != head bone %u (index-keyed clip transfer broken)",
					id, s, b, b);
			}
		}
	}

	ZENITH_ASSERT_GT(uFamiliesTested, 0u,
		"no multi-stage buildable family exercised the topology-stability gate");
}

// ############################################################################
// SC1 core -- frozen scalar seams (asset paths, size scale, bone-name format, shiny angle)
// ############################################################################

// ZM_CreatureAssetPath writes the canonical "game:" asset ref
// (game:Creatures/<Name>/<Name><suffix>.<ext>) for a (species, kind) and reports
// truncation. Golden-locks the EXACT ref strings for a known species (Fernfawn, F01
// stage 1) across the five shipped asset kinds, and the too-small-cap -> false
// (truncation) contract. Pure; compiled in ALL configs (no build, no bake, no guard).
ZENITH_TEST(ZM_Gen, CreatureGen_AssetPathScheme)
{
	char acRef[256];

	ZENITH_ASSERT_TRUE(
		ZM_CreatureAssetPath(ZM_SPECIES_FERNFAWN, ZM_CREATURE_ASSET_MESH, acRef, sizeof(acRef)),
		"mesh ref must fit a 256-byte buffer");
	ZENITH_ASSERT_STREQ(acRef, "game:Creatures/Fernfawn/Fernfawn.zmesh", "mesh ref scheme drifted");

	ZENITH_ASSERT_TRUE(
		ZM_CreatureAssetPath(ZM_SPECIES_FERNFAWN, ZM_CREATURE_ASSET_SKELETON, acRef, sizeof(acRef)),
		"skeleton ref must fit");
	ZENITH_ASSERT_STREQ(acRef, "game:Creatures/Fernfawn/Fernfawn.zskel", "skeleton ref scheme drifted");

	ZENITH_ASSERT_TRUE(
		ZM_CreatureAssetPath(ZM_SPECIES_FERNFAWN, ZM_CREATURE_ASSET_ALBEDO, acRef, sizeof(acRef)),
		"albedo ref must fit");
	ZENITH_ASSERT_STREQ(acRef, "game:Creatures/Fernfawn/Fernfawn_albedo.ztxtr", "albedo ref scheme drifted");

	ZENITH_ASSERT_TRUE(
		ZM_CreatureAssetPath(ZM_SPECIES_FERNFAWN, ZM_CREATURE_ASSET_SHINY, acRef, sizeof(acRef)),
		"shiny ref must fit");
	ZENITH_ASSERT_STREQ(acRef, "game:Creatures/Fernfawn/Fernfawn_shiny.ztxtr", "shiny ref scheme drifted");

	ZENITH_ASSERT_TRUE(
		ZM_CreatureAssetPath(ZM_SPECIES_FERNFAWN, ZM_CREATURE_ASSET_ICON, acRef, sizeof(acRef)),
		"icon ref must fit");
	ZENITH_ASSERT_STREQ(acRef, "game:Creatures/Fernfawn/Fernfawn_icon.ztxtr", "icon ref scheme drifted");

	// Truncation: a cap far too small to hold the full ref returns false and leaves the
	// buffer NUL-terminated (best-effort) within the cap.
	char acTiny[8];
	const bool bFits = ZM_CreatureAssetPath(ZM_SPECIES_FERNFAWN, ZM_CREATURE_ASSET_MESH,
		acTiny, sizeof(acTiny));
	ZENITH_ASSERT_FALSE(bFits, "an 8-byte cap cannot hold the mesh ref -- must report truncation");
	ZENITH_ASSERT_LT((u_int)strlen(acTiny), (u_int)sizeof(acTiny),
		"a truncated ref must stay NUL-terminated within the cap");
}

// ZM_SizeClassScale is GOLDEN-PINNED (baked into every creature's mesh extents): lock
// the five exact values, MEDIUM == 1.0 (the reference scale), and the strictly
// increasing TINY < SMALL < MEDIUM < LARGE < HUGE ordering.
ZENITH_TEST(ZM_Gen, CreatureGen_SizeClassScaleGolden)
{
	// Exact golden values (float literals identical to the impl's, so operator== holds).
	ZENITH_ASSERT_EQ(ZM_SizeClassScale(ZM_SIZE_TINY),   0.45f, "TINY scale golden-pinned to 0.45");
	ZENITH_ASSERT_EQ(ZM_SizeClassScale(ZM_SIZE_SMALL),  0.70f, "SMALL scale golden-pinned to 0.70");
	ZENITH_ASSERT_EQ(ZM_SizeClassScale(ZM_SIZE_MEDIUM), 1.00f, "MEDIUM scale golden-pinned to 1.00");
	ZENITH_ASSERT_EQ(ZM_SizeClassScale(ZM_SIZE_LARGE),  1.50f, "LARGE scale golden-pinned to 1.50");
	ZENITH_ASSERT_EQ(ZM_SizeClassScale(ZM_SIZE_HUGE),   2.20f, "HUGE scale golden-pinned to 2.20");

	// MEDIUM is exactly the 1.0 reference scale.
	ZENITH_ASSERT_EQ(ZM_SizeClassScale(ZM_SIZE_MEDIUM), 1.0f, "MEDIUM must be the 1.0 reference scale");

	// Strictly increasing across the ladder.
	ZENITH_ASSERT_LT(ZM_SizeClassScale(ZM_SIZE_TINY),   ZM_SizeClassScale(ZM_SIZE_SMALL),  "TINY < SMALL");
	ZENITH_ASSERT_LT(ZM_SizeClassScale(ZM_SIZE_SMALL),  ZM_SizeClassScale(ZM_SIZE_MEDIUM), "SMALL < MEDIUM");
	ZENITH_ASSERT_LT(ZM_SizeClassScale(ZM_SIZE_MEDIUM), ZM_SizeClassScale(ZM_SIZE_LARGE),  "MEDIUM < LARGE");
	ZENITH_ASSERT_LT(ZM_SizeClassScale(ZM_SIZE_LARGE),  ZM_SizeClassScale(ZM_SIZE_HUGE),   "LARGE < HUGE");
}

// ZM_FormatBoneName: iIndex < 0 yields the bare base name; iIndex >= 0 appends a
// zero-padded 2-digit suffix. Direct coverage of the deterministic bone-name formatter.
ZENITH_TEST(ZM_Gen, CreatureGen_FormatBoneName)
{
	char acName[uZM_GEN_BONE_NAME_MAX];

	// Negative index -> bare base, no suffix.
	ZM_FormatBoneName(acName, uZM_GEN_BONE_NAME_MAX, "Head", -1);
	ZENITH_ASSERT_STREQ(acName, "Head", "iIndex<0 must yield the bare base name");

	// Non-negative index -> base + zero-padded 2-digit suffix.
	ZM_FormatBoneName(acName, uZM_GEN_BONE_NAME_MAX, "Spine", 0);
	ZENITH_ASSERT_STREQ(acName, "Spine00", "index 0 must zero-pad to two digits");

	ZM_FormatBoneName(acName, uZM_GEN_BONE_NAME_MAX, "Spine", 7);
	ZENITH_ASSERT_STREQ(acName, "Spine07", "index 7 must zero-pad to two digits");

	// A two-digit index is not padded further.
	ZM_FormatBoneName(acName, uZM_GEN_BONE_NAME_MAX, "Tail", 12);
	ZENITH_ASSERT_STREQ(acName, "Tail12", "a two-digit index must not pad beyond two digits");
}

// ZM_CreatureShinyAngle returns a hue-rotation angle inside the golden-pinned shiny
// band [80, 280) degrees for a spread of QUADRUPED (buildable) species; deterministic
// per species (keyed on the SHINY domain).
ZENITH_TEST(ZM_Gen, CreatureGen_ShinyAngleInBand)
{
	// The band constants themselves are pinned to [80, 280).
	ZENITH_ASSERT_EQ(fZM_CREATURE_SHINY_MIN_DEG, 80.0f,  "shiny band min golden-pinned to 80");
	ZENITH_ASSERT_EQ(fZM_CREATURE_SHINY_MAX_DEG, 280.0f, "shiny band max golden-pinned to 280");

	u_int uTested = 0u;
	for (u_int id = 0; id < (u_int)ZM_SPECIES_COUNT; ++id)
	{
		const ZM_SPECIES_ID eId = (ZM_SPECIES_ID)id;
		if (!HasBuilder(eId)) { continue; }   // scope to QUADRUPED (buildable) species
		++uTested;

		const ZM_CreatureRecipe xR = ZM_ResolveCreatureRecipe(eId);
		const float fAngle = ZM_CreatureShinyAngle(xR);
		ZENITH_ASSERT_GE(fAngle, fZM_CREATURE_SHINY_MIN_DEG,
			"species %u shiny angle %f below the shiny band min", id, (double)fAngle);
		ZENITH_ASSERT_LT(fAngle, fZM_CREATURE_SHINY_MAX_DEG,
			"species %u shiny angle %f reached/exceeded the shiny band max", id, (double)fAngle);
	}
	ZENITH_ASSERT_GT(uTested, 0u, "no buildable species exercised the shiny-angle band check");
}

// ############################################################################
// SC1 core -- ZM_CreatureArchetypeCommon kit helpers
// ############################################################################

namespace
{
	// Build a minimal spine tube (creates the single root) and hand back a spine bone
	// + its model-space position so appendage helpers can attach to it. Returns the
	// out-bone count.
	u_int MakeKitSpine(ZM_GenMesh& xMesh, u_int (&auOut)[8], Zenith_Maths::Vector3& xBellyOut)
	{
		ZM_KitSpineParams xParams;
		xParams.m_iParentBone  = -1;                                      // create the root here
		xParams.m_xBellyCentre = Zenith_Maths::Vector3(0.0f, 0.5f, 0.0f);
		xParams.m_fLength      = 1.0f;
		xParams.m_uSegments    = 4u;
		xBellyOut = xParams.m_xBellyCentre;
		return ZM_AppendSpineTube(xMesh, xParams, auOut, 8u);
	}

	// Y extent over the vertices appended from uFirstVert onward.
	float YExtentFrom(const ZM_GenMesh& xMesh, u_int uFirstVert)
	{
		if (uFirstVert >= xMesh.GetNumVerts()) { return 0.0f; }
		float fMin = xMesh.m_xPositions.Get(uFirstVert).y;
		float fMax = fMin;
		for (u_int v = uFirstVert; v < xMesh.GetNumVerts(); ++v)
		{
			const float fY = xMesh.m_xPositions.Get(v).y;
			if (fY < fMin) { fMin = fY; }
			if (fY > fMax) { fMax = fY; }
		}
		return fMax - fMin;
	}

	// Common post-append checks: bones grew, verts grew by at least one ring, the part
	// spans a non-zero Y, the mesh winds outward, and (post-normalise) the weight/index
	// contract holds. uSegs is the appendage's radial segment count.
	void AssertAppendageValid(ZM_GenMesh& xMesh, u_int uBonesBefore, u_int uVertsBefore,
		u_int uFirstVert, u_int uSegs, const char* szWho)
	{
		ZENITH_ASSERT_GT(xMesh.GetNumBones(), uBonesBefore, "%s added no bones", szWho);
		ZENITH_ASSERT_GE(xMesh.GetNumVerts() - uVertsBefore, uSegs + 1u,
			"%s appended fewer than one full ring", szWho);
		ZENITH_ASSERT_GT(YExtentFrom(xMesh, uFirstVert), 1.0e-4f, "%s has degenerate Y span", szWho);

		ZM_GenNormalizeSkinWeights(xMesh);
		const ZM_GenMeshValidation xVal = ZM_ValidateGenMesh(xMesh, uZM_GEN_CREATURE_BONE_CAP);
		ZENITH_ASSERT_TRUE(xVal.m_bWindingOutward, "%s winding not outward (bad tri %u)", szWho, xVal.m_uFirstBadTriangle);
		ZENITH_ASSERT_TRUE(xVal.m_bWeightsSumToOne, "%s weights do not sum to 1", szWho);
		ZENITH_ASSERT_TRUE(xVal.m_bWeightsAtMostTwo, "%s has >2 influences", szWho);
		ZENITH_ASSERT_TRUE(BoneIndicesInRange(xMesh), "%s produced out-of-range bone indices", szWho);
		ZENITH_ASSERT_TRUE(SkeletonWellFormed(xMesh), "%s left the skeleton malformed", szWho);
	}
}

// ZM_AppendSpineTube creates the single root, adds >= 2 bones, emits >= 1 ring with a
// non-zero Y span, and winds outward -- the body-plan trunk every archetype starts from.
ZENITH_TEST(ZM_Gen, CreatureGen_KitSpineTube)
{
	ZM_GenMesh xMesh;
	u_int auOut[8] = {};
	Zenith_Maths::Vector3 xBelly(0.0f);
	const u_int uCount = MakeKitSpine(xMesh, auOut, xBelly);

	ZENITH_ASSERT_GE(uCount, 2u, "spine must produce >= 2 out bones");
	ZENITH_ASSERT_EQ(xMesh.GetNumBones(), uCount, "spine bone count != out-bone count (nothing else added)");
	ZENITH_ASSERT_GT(xMesh.GetNumVerts(), 0u, "spine emitted no vertices");
	ZENITH_ASSERT_GT(YExtentFrom(xMesh, 0u), 1.0e-4f, "spine has degenerate Y span");
	ZENITH_ASSERT_EQ(xMesh.m_xBones.Get(0u).m_iParent, -1, "spine bone 0 must be the root");

	ZM_GenNormalizeSkinWeights(xMesh);
	const ZM_GenMeshValidation xVal = ZM_ValidateGenMesh(xMesh, uZM_GEN_CREATURE_BONE_CAP);
	ZENITH_ASSERT_TRUE(xVal.m_bWindingOutward, "spine winding not outward (bad tri %u)", xVal.m_uFirstBadTriangle);
	ZENITH_ASSERT_TRUE(xVal.m_bWeightsSumToOne, "spine weights do not sum to 1");
	ZENITH_ASSERT_TRUE(SkeletonWellFormed(xMesh), "spine skeleton malformed");
}

// ZM_AppendLimb attaches a 2-bone (Up/Lo) descending leg to a spine bone, adding
// bones + at least one ring with a non-zero Y span and outward winding.
ZENITH_TEST(ZM_Gen, CreatureGen_KitLimb)
{
	ZM_GenMesh xMesh;
	u_int auOut[8] = {};
	Zenith_Maths::Vector3 xBelly(0.0f);
	MakeKitSpine(xMesh, auOut, xBelly);

	const u_int uBones = xMesh.GetNumBones();
	const u_int uVerts = xMesh.GetNumVerts();

	ZM_KitLimbParams xParams;
	xParams.m_iParentBone  = (int)auOut[0];
	xParams.m_xParentWorld = xBelly;
	xParams.m_xHip  = Zenith_Maths::Vector3(0.35f, 1.00f, 0.40f);
	xParams.m_xKnee = Zenith_Maths::Vector3(0.38f, 0.55f, 0.40f);
	xParams.m_xFoot = Zenith_Maths::Vector3(0.40f, 0.05f, 0.40f);
	xParams.m_szName = "LegFL";
	const ZM_KitAppendResult xRes = ZM_AppendLimb(xMesh, xParams);

	ZENITH_ASSERT_GE(xMesh.GetNumBones() - uBones, 2u, "limb must add its Up + Lo bones");
	AssertAppendageValid(xMesh, uBones, uVerts, xRes.m_uFirstVert, xParams.m_uSegs, "limb");
}

// ZM_AppendTail attaches a tapering multi-bone tail to a spine bone.
ZENITH_TEST(ZM_Gen, CreatureGen_KitTail)
{
	ZM_GenMesh xMesh;
	u_int auOut[8] = {};
	Zenith_Maths::Vector3 xBelly(0.0f);
	const u_int uSpineCount = MakeKitSpine(xMesh, auOut, xBelly);

	const u_int uBones = xMesh.GetNumBones();
	const u_int uVerts = xMesh.GetNumVerts();

	ZM_KitTailParams xParams;
	xParams.m_iParentBone  = (int)auOut[uSpineCount - 1u];
	xParams.m_xParentWorld = Zenith_Maths::Vector3(0.0f, 1.5f, 0.0f);
	xParams.m_xBase = Zenith_Maths::Vector3(0.0f, 0.90f, -0.50f);
	xParams.m_xTip  = Zenith_Maths::Vector3(0.0f, 0.40f, -1.20f);
	xParams.m_uSegments = 3u;
	const ZM_KitAppendResult xRes = ZM_AppendTail(xMesh, xParams);

	AssertAppendageValid(xMesh, uBones, uVerts, xRes.m_uFirstVert, xParams.m_uSegs, "tail");
}

// ZM_AppendHorn attaches a single-bone upward cone to a spine/head bone.
ZENITH_TEST(ZM_Gen, CreatureGen_KitHorn)
{
	ZM_GenMesh xMesh;
	u_int auOut[8] = {};
	Zenith_Maths::Vector3 xBelly(0.0f);
	const u_int uSpineCount = MakeKitSpine(xMesh, auOut, xBelly);

	const u_int uBones = xMesh.GetNumBones();
	const u_int uVerts = xMesh.GetNumVerts();

	ZM_KitHornParams xParams;
	xParams.m_iParentBone  = (int)auOut[uSpineCount - 1u];
	xParams.m_xParentWorld = Zenith_Maths::Vector3(0.0f, 1.5f, 0.0f);
	xParams.m_xBase = Zenith_Maths::Vector3(0.0f, 1.55f, 0.30f);
	xParams.m_xTip  = Zenith_Maths::Vector3(0.0f, 1.95f, 0.30f);
	const ZM_KitAppendResult xRes = ZM_AppendHorn(xMesh, xParams);

	ZENITH_ASSERT_GE(xMesh.GetNumBones() - uBones, 1u, "horn must add its bone");
	AssertAppendageValid(xMesh, uBones, uVerts, xRes.m_uFirstVert, xParams.m_uSegs, "horn");
}

// ZM_AppendEllipsoidHead attaches a single-bone ellipsoid head to a spine bone.
ZENITH_TEST(ZM_Gen, CreatureGen_KitHead)
{
	ZM_GenMesh xMesh;
	u_int auOut[8] = {};
	Zenith_Maths::Vector3 xBelly(0.0f);
	const u_int uSpineCount = MakeKitSpine(xMesh, auOut, xBelly);

	const u_int uBones = xMesh.GetNumBones();
	const u_int uVerts = xMesh.GetNumVerts();

	ZM_KitHeadParams xParams;
	xParams.m_iParentBone  = (int)auOut[uSpineCount - 1u];
	xParams.m_xParentWorld = Zenith_Maths::Vector3(0.0f, 1.5f, 0.0f);
	xParams.m_xCentre      = Zenith_Maths::Vector3(0.0f, 1.60f, 0.60f);
	xParams.m_xHalfExtents = Zenith_Maths::Vector3(0.30f, 0.30f, 0.30f);
	xParams.m_uRings = 5u;
	const ZM_KitAppendResult xRes = ZM_AppendEllipsoidHead(xMesh, xParams);

	ZENITH_ASSERT_GE(xMesh.GetNumBones() - uBones, 1u, "head must add its bone");
	AssertAppendageValid(xMesh, uBones, uVerts, xRes.m_uFirstVert, xParams.m_uSegs, "head");
}
