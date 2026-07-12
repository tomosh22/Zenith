#include "Zenith.h"

#include "Zenithmon/Source/Battle/ZM_Breeding.h"
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"   // ZM_GetSpeciesData, ZM_GetSpeciesCount
#include "Zenithmon/Source/Data/ZM_Learnsets.h"     // ZM_GetSpeciesLearnset
#include "Zenithmon/Source/Data/ZM_MoveData.h"      // ZM_MOVE_NONE

// ============================================================================
// ZM_Breeding implementation (S2 box 6 SC1). Pure, seeded, headless. Base-evo
// derivation and compatibility are RNG-free; ZM_GenerateEgg draws from the one
// sanctioned ZM_BattleRNG in the EXACT order pinned by the spec (section 7):
// IVs (pick-K-distinct then fill six) -> nature -> ability/moves/EVs (no RNG).
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

ZM_ARCHETYPE ZM_GetBreedingGroup(ZM_SPECIES_ID eSpecies)
{
	Zenith_Assert(eSpecies < ZM_SPECIES_COUNT,
		"ZM_GetBreedingGroup: species %u out of range", (u_int)eSpecies);
	return ZM_GetSpeciesData(eSpecies).m_eArchetype;
}

bool ZM_AreSpeciesCompatible(ZM_SPECIES_ID eA, ZM_SPECIES_ID eB)
{
	Zenith_Assert(eA < ZM_SPECIES_COUNT,
		"ZM_AreSpeciesCompatible: species A %u out of range", (u_int)eA);
	Zenith_Assert(eB < ZM_SPECIES_COUNT,
		"ZM_AreSpeciesCompatible: species B %u out of range", (u_int)eB);

	if (ZM_GetSpeciesData(eA).m_eRarity == ZM_RARITY_LEGENDARY) { return false; }
	if (ZM_GetSpeciesData(eB).m_eRarity == ZM_RARITY_LEGENDARY) { return false; }
	return ZM_GetBreedingGroup(eA) == ZM_GetBreedingGroup(eB);
}

bool ZM_AreCompatible(const ZM_BattleMonsterSpec& xParentA, const ZM_BattleMonsterSpec& xParentB)
{
	return ZM_AreSpeciesCompatible(xParentA.m_eSpecies, xParentB.m_eSpecies);
}

ZM_BattleMonsterSpec ZM_GenerateEgg(const ZM_BattleMonsterSpec& xMother,
	const ZM_BattleMonsterSpec& xFather, ZM_BattleRNG& xRng, const ZM_BreedingParams& xParams)
{
	Zenith_Assert(ZM_AreCompatible(xMother, xFather),
		"ZM_GenerateEgg: parents (species %u, %u) are not breeding-compatible",
		(u_int)xMother.m_eSpecies, (u_int)xFather.m_eSpecies);

	ZM_BattleMonsterSpec xEgg = {};

	// --- Step A: species = mother's base evolution (NO RNG) ---
	xEgg.m_eSpecies = ZM_GetBaseEvolution(xMother.m_eSpecies);

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
			xEgg.m_auIV[i] = (uParent == 0u) ? xMother.m_auIV[i] : xFather.m_auIV[i];
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

	// --- Step D: ability copied from the mother (NO RNG; hidden abilities cut) ---
	xEgg.m_eAbility = xMother.m_eAbility;

	// --- Step E: moves = base-evo level-1 learnset, first uZM_MAX_MOVES (NO RNG) ---
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

	// --- Step F: remaining fields (NO RNG). Level 1, zero EVs, exp -> L1 floor. ---
	xEgg.m_uLevel = uZM_EGG_HATCH_LEVEL;
	for (u_int i = 0u; i < ZM_STAT_COUNT; ++i)
	{
		xEgg.m_auEV[i] = 0u;
	}
	xEgg.m_bOverrideBaseStats = false;
	xEgg.m_uCurExp            = uZM_EXP_UNSPECIFIED;   // ZM_BuildBattleMonster bands to the L1 floor (0)
	return xEgg;
}
