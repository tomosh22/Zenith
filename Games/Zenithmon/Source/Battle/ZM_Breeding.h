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
// Box-6 SC-B upgrades: egg groups are a REAL derived taxonomy (ZM_GetSpeciesEggGroups,
// no longer the ZM_ARCHETYPE proxy); compatibility is GENDERED (one MALE + one FEMALE)
// with a Ditto-analog UNIVERSAL breeder (GLOOPET) that ignores gender + egg group;
// and ZM_GenerateEgg derives parent ROLES internally -- the "mother" (egg species +
// inherited line) is the FEMALE parent, or the NON-universal parent when the universal
// breeder is involved. The egg's gender is rolled from the OFFSPRING species' ratio
// (SC-A, ZM_RollGender).
//
// Box-6 SC-C upgrades (this file): ABILITY inheritance defaults to the mother's ability,
// but if the mother CARRIES her species' hidden ability (ZM_GetSpeciesAbilities), the
// offspring inherits its own species' hidden ability with a Chance(60,100) draw, else
// the regular ability -- a CONDITIONAL draw appended LAST in the pinned order. EGG-MOVE
// inheritance (RNG-free) fills any empty move slot after the base-evo L1 learnset with
// the OFFSPRING species' derived egg moves (ZM_GetSpeciesEggMoves) that a parent already
// knows, up to the 4-move cap. Masuda / shiny = cut.
// ============================================================================

// --- tunable constants (spec sections 7-8) ---
static const u_int uZM_BREED_IV_INHERIT_BASE  = 3u;   // IVs inherited from parents (default)
static const u_int uZM_BREED_IV_INHERIT_KNOT  = 5u;   // ... with the Heirloom Knot
static const u_int uZM_EGG_HATCH_LEVEL        = 1u;   // eggs are produced at level 1
// Hidden-ability inheritance chance (box-6 SC-C): when the mother carries her hidden
// ability, the offspring inherits the hidden (else regular) with this percent (Gen VI+).
static const u_int uZM_BREED_HIDDEN_INHERIT_PCT = 60u;

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

// True iff the two species CAN form a breeding pair ignoring gender (box-6 SC-B;
// used where the parents' genders are unknown). BOTH must be non-legendary AND either
// share >= 1 egg group (ZM_GetSpeciesEggGroups) OR exactly one side is the universal
// breeder. Two universal breeders (or universal + legendary) are incompatible.
// Symmetric. Pure, no RNG. (Same-slot identity is a daycare concern, not checked here.)
bool          ZM_AreSpeciesCompatible(ZM_SPECIES_ID eA, ZM_SPECIES_ID eB);

// The real, GENDER-AWARE breeding gate on two parent specs (box-6 SC-B): the species
// are ZM_AreSpeciesCompatible AND either one side is the universal breeder (which
// ignores gender) OR the two carry OPPOSITE binary genders (one MALE + one FEMALE). A
// GENDERLESS non-universal parent can breed ONLY with the universal breeder. Symmetric,
// no RNG.
bool          ZM_AreCompatible(const ZM_BattleMonsterSpec& xParentA,
                               const ZM_BattleMonsterSpec& xParentB);

// Produce the egg. PRECONDITION: ZM_AreCompatible(xParentA, xParentB) is true
// (asserted). Parent ROLES are derived internally (box-6 SC-B): the "mother" -- the
// egg's species (base evolution) plus its inherited ability -- is the FEMALE parent,
// or the NON-universal parent when the universal breeder is involved; the other parent
// is the "father". Callers may pass the two parents in EITHER order. Deterministic
// function of the parents + xRng + xParams; the RNG draw order is pinned (IV -> nature
// -> gender -> conditional hidden-ability) and unchanged by role selection. Returns a
// level-1 ZM_BattleMonsterSpec.
ZM_BattleMonsterSpec ZM_GenerateEgg(const ZM_BattleMonsterSpec& xParentA,
                                    const ZM_BattleMonsterSpec& xParentB,
                                    ZM_BattleRNG& xRng,
                                    const ZM_BreedingParams& xParams = ZM_BreedingParams{});
