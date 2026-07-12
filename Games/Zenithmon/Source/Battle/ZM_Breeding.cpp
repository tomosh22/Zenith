#include "Zenith.h"

#include "Zenithmon/Source/Battle/ZM_Breeding.h"
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"   // ZM_GetSpeciesData, ZM_GetSpeciesCount
#include "Zenithmon/Source/Data/ZM_Learnsets.h"     // ZM_GetSpeciesLearnset
#include "Zenithmon/Source/Data/ZM_MoveData.h"      // ZM_MOVE_NONE

// ============================================================================
// ZM_Breeding implementation (S2 box 6 SC1 + SC-A gender + SC-B egg groups/roles).
// Pure, seeded, headless. Base-evo derivation, egg-group/gendered compatibility and
// parent-role selection are RNG-free; ZM_GenerateEgg draws from the one sanctioned
// ZM_BattleRNG in the EXACT order pinned by the spec (section 8): IVs (pick-K-distinct
// then fill six) -> nature -> gender (offspring ratio) -> ability/moves/EVs (no RNG).
// SC-B changes only WHICH parent each draw reads from (the FEMALE / non-universal side
// is the "mother"), never the draw ORDER, so every IV/nature draw is byte-identical.
// See the header banner for the reduced-mechanic rulings.
// ============================================================================

// The egg's fresh-IV cap is the [0,31] IV band (spec section 7 pins RandBelow(32)).
static const u_int uZM_BREED_IV_ROLL_BOUND = 32u;

ZM_SPECIES_ID ZM_GetBaseEvolution(ZM_SPECIES_ID eSpecies)
{
	Zenith_Assert(eSpecies < ZM_SPECIES_COUNT,
		"ZM_GetBaseEvolution: species %u out of range", (u_int)eSpecies);

	const u_int   uCount = ZM_GetSpeciesCount();
	ZM_SPECIES_ID eCur   = eSpecies;

	// Walk the (single, linear) evolution chain backward: repeatedly find the
	// species whose m_eEvolvesTo points at eCur. The guard bounds the walk to the
	// roster size so a malformed cyclic table can never hang.
	for (u_int uGuard = 0u; uGuard <= uCount; ++uGuard)
	{
		ZM_SPECIES_ID ePrev = ZM_SPECIES_NONE;
		for (u_int i = 0u; i < uCount; ++i)
		{
			const ZM_SPECIES_ID eCand = (ZM_SPECIES_ID)i;
			if (ZM_GetSpeciesData(eCand).m_eEvolvesTo == eCur)
			{
				ePrev = eCand;
				break;
			}
		}
		if (ePrev == ZM_SPECIES_NONE)
		{
			return eCur;   // fixed point: nothing evolves into eCur -> it is the base form
		}
		eCur = ePrev;
	}
	return eCur;   // defensive: cycle guard tripped (unreachable for linear chains)
}

namespace
{
	// True iff the two egg-group lists share at least one real breeding group.
	// NO_EGGS never matches (it marks the non-breeding legendaries) -- a defensive
	// guard, since legendaries are already rejected before this is reached.
	bool EggGroupsShare(const ZM_EggGroups& xA, const ZM_EggGroups& xB)
	{
		for (u_int a = 0u; a < xA.m_uCount; ++a)
		{
			if (xA.m_aeGroups[a] == ZM_EGG_GROUP_NO_EGGS) { continue; }
			for (u_int b = 0u; b < xB.m_uCount; ++b)
			{
				if (xB.m_aeGroups[b] == ZM_EGG_GROUP_NO_EGGS) { continue; }
				if (xA.m_aeGroups[a] == xB.m_aeGroups[b]) { return true; }
			}
		}
		return false;
	}
}

bool ZM_AreSpeciesCompatible(ZM_SPECIES_ID eA, ZM_SPECIES_ID eB)
{
	Zenith_Assert(eA < ZM_SPECIES_COUNT,
		"ZM_AreSpeciesCompatible: species A %u out of range", (u_int)eA);
	Zenith_Assert(eB < ZM_SPECIES_COUNT,
		"ZM_AreSpeciesCompatible: species B %u out of range", (u_int)eB);

	// Legendaries never breed (they carry the NO_EGGS group).
	if (ZM_GetSpeciesData(eA).m_eRarity == ZM_RARITY_LEGENDARY) { return false; }
	if (ZM_GetSpeciesData(eB).m_eRarity == ZM_RARITY_LEGENDARY) { return false; }

	// The universal breeder pairs with any breedable partner -- EXCEPT another universal
	// breeder (two shapeshifters cannot seed a line). This path ignores egg groups.
	const bool bAUniversal = ZM_IsUniversalBreeder(eA);
	const bool bBUniversal = ZM_IsUniversalBreeder(eB);
	if (bAUniversal && bBUniversal) { return false; }
	if (bAUniversal || bBUniversal) { return true; }

	// Otherwise: both non-universal -> a shared egg group is required.
	return EggGroupsShare(ZM_GetSpeciesEggGroups(eA), ZM_GetSpeciesEggGroups(eB));
}

bool ZM_AreCompatible(const ZM_BattleMonsterSpec& xParentA, const ZM_BattleMonsterSpec& xParentB)
{
	// Species-level gate first (non-legendary + shared group / universal breeder).
	if (!ZM_AreSpeciesCompatible(xParentA.m_eSpecies, xParentB.m_eSpecies))
	{
		return false;
	}

	// The universal breeder ignores gender: its partner is already a breedable, non-
	// legendary, non-universal species by the species-level check above.
	if (ZM_IsUniversalBreeder(xParentA.m_eSpecies) ||
		ZM_IsUniversalBreeder(xParentB.m_eSpecies))
	{
		return true;
	}

	// Otherwise a valid pair is exactly one MALE + one FEMALE. A GENDERLESS parent on
	// either side (non-universal) is never a valid pairing.
	const bool bAMale   = (xParentA.m_eGender == ZM_GENDER_MALE);
	const bool bAFemale = (xParentA.m_eGender == ZM_GENDER_FEMALE);
	const bool bBMale   = (xParentB.m_eGender == ZM_GENDER_MALE);
	const bool bBFemale = (xParentB.m_eGender == ZM_GENDER_FEMALE);
	return (bAMale && bBFemale) || (bAFemale && bBMale);
}

ZM_BattleMonsterSpec ZM_GenerateEgg(const ZM_BattleMonsterSpec& xParentA,
	const ZM_BattleMonsterSpec& xParentB, ZM_BattleRNG& xRng, const ZM_BreedingParams& xParams)
{
	Zenith_Assert(ZM_AreCompatible(xParentA, xParentB),
		"ZM_GenerateEgg: parents (species %u, %u) are not breeding-compatible",
		(u_int)xParentA.m_eSpecies, (u_int)xParentB.m_eSpecies);

	// --- Parent ROLES (box-6 SC-B, NO RNG). The "mother" is the egg's species +
	// inherited-line/ability source: the FEMALE parent, or the NON-universal parent
	// when the universal breeder is involved. Role selection happens BEFORE any draw,
	// so the pinned draw order (IV -> nature -> gender) is untouched -- only WHICH
	// parent each draw reads from can change. ---
	const ZM_BattleMonsterSpec* pxMother = &xParentA;
	const ZM_BattleMonsterSpec* pxFather = &xParentB;
	if (ZM_IsUniversalBreeder(xParentA.m_eSpecies))
	{
		// A is the shapeshifter -> B (the non-universal parent) seeds the line.
		pxMother = &xParentB;
		pxFather = &xParentA;
	}
	else if (ZM_IsUniversalBreeder(xParentB.m_eSpecies))
	{
		// B is the shapeshifter -> A seeds the line (the default, made explicit).
		pxMother = &xParentA;
		pxFather = &xParentB;
	}
	else if (xParentA.m_eGender != ZM_GENDER_FEMALE)
	{
		// Non-universal pair -> the FEMALE parent is the mother. The precondition
		// guarantees exactly one FEMALE, so A-not-FEMALE means B is that female mother.
		pxMother = &xParentB;
		pxFather = &xParentA;
	}

	ZM_BattleMonsterSpec xEgg = {};

	// --- Step A: species = mother's base evolution (NO RNG) ---
	xEgg.m_eSpecies = ZM_GetBaseEvolution(pxMother->m_eSpecies);

	// --- Step B: IV inheritance (RNG). K = 5 with the Heirloom Knot, else 3. ---
	const u_int uInheritCount = xParams.m_bHeirloomKnot
		? uZM_BREED_IV_INHERIT_KNOT : uZM_BREED_IV_INHERIT_BASE;

	// Phase B1: pick K distinct stat indices to inherit (rejection sampling; one
	// RandBelow(6) draw per loop iteration, re-rolls on a duplicate).
	bool  abInherit[ZM_STAT_COUNT] = {};
	u_int uPicked = 0u;
	while (uPicked < uInheritCount)
	{
		const u_int uIdx = xRng.RandBelow(ZM_STAT_COUNT);
		if (!abInherit[uIdx])
		{
			abInherit[uIdx] = true;
			++uPicked;
		}
	}

	// Phase B2: fill all six stats in canonical ZM_STAT order. An inherited stat
	// draws RandBelow(2) (0 = mother, 1 = father); a non-inherited stat draws a
	// fresh RandBelow(32) IV in [0,31]. Draws interleave in this fixed order.
	for (u_int i = 0u; i < ZM_STAT_COUNT; ++i)
	{
		if (abInherit[i])
		{
			const u_int uParent = xRng.RandBelow(2u);
			xEgg.m_auIV[i] = (uParent == 0u) ? pxMother->m_auIV[i] : pxFather->m_auIV[i];
		}
		else
		{
			xEgg.m_auIV[i] = xRng.RandBelow(uZM_BREED_IV_ROLL_BOUND);
		}
	}

	// --- Step C: nature (locked by the Stasis Stone, else one RandBelow(25) draw) ---
	if (xParams.m_eEverstoneNature != ZM_NATURE_COUNT)
	{
		xEgg.m_eNature = xParams.m_eEverstoneNature;   // locked -> NO draw
	}
	else
	{
		xEgg.m_eNature = (ZM_NATURE)xRng.RandBelow((u_int)ZM_NATURE_COUNT);   // 0..24
	}

	// --- Step D: gender rolled from the OFFSPRING species' ratio (box-6 SC-A). This
	// draw is APPENDED after the IV + nature draws, so every existing IV/nature golden
	// is byte-identical; a fixed-ratio (genderless / single-gender) offspring draws
	// nothing. This is the LAST RNG draw -- steps E/F/G below are RNG-free. ---
	xEgg.m_eGender = ZM_RollGender(xEgg.m_eSpecies, xRng);

	// --- Step E: ability copied from the mother (NO RNG; hidden abilities cut) ---
	xEgg.m_eAbility = pxMother->m_eAbility;

	// --- Step F: moves = base-evo level-1 learnset, first uZM_MAX_MOVES (NO RNG) ---
	for (u_int i = 0u; i < uZM_MAX_MOVES; ++i)
	{
		xEgg.m_aeMoves[i] = ZM_MOVE_NONE;
	}
	const ZM_Learnset xLearnset = ZM_GetSpeciesLearnset(xEgg.m_eSpecies);
	u_int uMoveSlot = 0u;
	for (u_int k = 0u; k < xLearnset.m_uCount && uMoveSlot < uZM_MAX_MOVES; ++k)
	{
		if (xLearnset.m_axMoves[k].m_uLevel <= uZM_EGG_HATCH_LEVEL)
		{
			xEgg.m_aeMoves[uMoveSlot++] = xLearnset.m_axMoves[k].m_eMove;
		}
	}

	// --- Step G: remaining fields (NO RNG). Level 1, zero EVs, exp -> L1 floor. ---
	xEgg.m_uLevel = uZM_EGG_HATCH_LEVEL;
	for (u_int i = 0u; i < ZM_STAT_COUNT; ++i)
	{
		xEgg.m_auEV[i] = 0u;
	}
	xEgg.m_bOverrideBaseStats = false;
	xEgg.m_uCurExp            = uZM_EXP_UNSPECIFIED;   // ZM_BuildBattleMonster bands to the L1 floor (0)
	return xEgg;
}
