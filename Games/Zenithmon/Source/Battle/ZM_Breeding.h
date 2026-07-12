#pragma once

#include "Zenithmon/Source/Battle/ZM_BattleMonster.h"   // ZM_BattleMonsterSpec, ZM_STAT_COUNT
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"        // ZM_SPECIES_ID, ZM_ARCHETYPE
#include "Zenithmon/Source/Data/ZM_NatureData.h"         // ZM_NATURE, ZM_NATURE_COUNT
#include "Zenithmon/Source/Data/ZM_BattleRNG.h"          // ZM_BattleRNG

// ============================================================================
// ZM_Breeding -- S2 box 6 SC1. The pure, seeded logic that turns two parent
// monster specs into an egg (a level-1 ZM_BattleMonsterSpec). Deterministic:
// a fixed RNG stream reproduces every roll bit-for-bit. No globals, no UI, no
// overworld. RNG is the ONLY randomness source (never rand()/std::random).
//
// Reduced/derived rulings (see spec sections 5-7 + Documented Cuts): egg groups
// are proxied by ZM_ARCHETYPE; gender is NOT modelled ("mother" = the first
// parameter by convention); ability = copied from the mother; egg moves = the
// base-evo level-1 learnset; hidden abilities / Ditto / Masuda / shiny = cut.
// ============================================================================

// --- tunable constants (spec sections 7-8) ---
static const u_int uZM_BREED_IV_INHERIT_BASE = 3u;   // IVs inherited from parents (default)
static const u_int uZM_BREED_IV_INHERIT_KNOT = 5u;   // ... with the Heirloom Knot
static const u_int uZM_EGG_HATCH_LEVEL       = 1u;   // eggs are produced at level 1

// Optional per-breed modifiers. The two GDD breeding items are modelled as
// explicit pure-function inputs (ZM_BattleMonster carries no held-item slot).
struct ZM_BreedingParams
{
	// Heirloom Knot (ZM_ITEM_HEIRLOOMKNOT): inherit 5 IVs instead of 3.
	bool      m_bHeirloomKnot    = false;
	// Stasis Stone (ZM_ITEM_STASISSTONE): lock the egg's nature to this value.
	// ZM_NATURE_COUNT == "no lock" (nature is rolled randomly instead).
	ZM_NATURE m_eEverstoneNature = ZM_NATURE_COUNT;
};

// The lowest evolutionary form of eSpecies (walks m_eEvolvesTo predecessors to a
// fixed point). Returns eSpecies itself for a stage-1 / single-stage / legendary.
// Pure, no RNG, bounds-asserted.
ZM_SPECIES_ID ZM_GetBaseEvolution(ZM_SPECIES_ID eSpecies);

// The breeding-group proxy for a species (its ZM_ARCHETYPE). Same value == same
// group. Documented reduced stand-in for a real egg-group column.
ZM_ARCHETYPE  ZM_GetBreedingGroup(ZM_SPECIES_ID eSpecies);

// True iff the two species can produce an egg: BOTH breedable (rarity != LEGENDARY)
// AND sharing a breeding group. Symmetric. Pure, no RNG. (Same-slot identity is a
// daycare concern, not checked here.)
bool          ZM_AreSpeciesCompatible(ZM_SPECIES_ID eA, ZM_SPECIES_ID eB);

// Convenience overload on the parent specs (reads each spec's m_eSpecies).
bool          ZM_AreCompatible(const ZM_BattleMonsterSpec& xParentA,
                               const ZM_BattleMonsterSpec& xParentB);

// Produce the egg. PRECONDITION: ZM_AreCompatible(xMother, xFather) is true
// (asserted). xMother (the FIRST parameter) is the species source. Deterministic
// function of the two parents + xRng + xParams; draw order pinned in spec section 7.
// Returns a level-1 ZM_BattleMonsterSpec ready for ZM_BuildBattleMonster.
ZM_BattleMonsterSpec ZM_GenerateEgg(const ZM_BattleMonsterSpec& xMother,
                                    const ZM_BattleMonsterSpec& xFather,
                                    ZM_BattleRNG& xRng,
                                    const ZM_BreedingParams& xParams = ZM_BreedingParams{});
