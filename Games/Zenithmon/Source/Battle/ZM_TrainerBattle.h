#pragma once

#include "Zenithmon/Source/Battle/ZM_BattleMonster.h"   // ZM_BattleMonsterSpec
#include "Zenithmon/Source/Data/ZM_TrainerData.h"       // ZM_TRAINER_ID, uZM_TRAINER_MAX_PARTY, ZM_AI_TIER

// ============================================================================
// ZM_TrainerBattle (S7 item 3 SC5) -- the PURE half of a forced trainer battle:
// everything ZM_BattleDirector needs to call ZM_BattleDirectorCore::Begin,
// derived from one compiled ZM_TrainerData row and NOTHING else. No ECS, no
// scene, no physics, no UI, no g_xEngine, no RNG, no allocation, no I/O, no
// statics -- so a boot unit proves the enemy party, the AI tier and the battle
// seed WITHOUT starting a battle or loading a scene.
//
// EVERY function here is TOTAL and none of them calls Zenith_Assert:
// Zenith_Assert breaks the process in EVERY configuration and the whole unit
// suite runs at boot, so an assert on a unit-supplied id would end the boot run
// rather than fail one case. An unregistered id yields the inert
// default-constructed setup (m_bValid == false) and is SILENT -- the registered
// check runs BEFORE ZM_GetTrainerData, which logs a non-fatal Zenith_Error.
// ============================================================================

// ResolveTurn now completes every non-terminal faint replacement before it
// returns, so trainers may field their complete authored row up to the shared
// engine party cap. Keep this named bound at the builder boundary.
inline constexpr u_int uZM_TRAINER_BATTLEABLE_PARTY = uZM_TRAINER_MAX_PARTY;
static_assert(uZM_TRAINER_BATTLEABLE_PARTY >= 1u
	&& uZM_TRAINER_BATTLEABLE_PARTY <= uZM_TRAINER_MAX_PARTY,
	"the battleable count must be at least one monster and must fit the row cap");

// The complete Begin() input for one trainer. Fixed-size by construction: the
// party array is uZM_TRAINER_MAX_PARTY wide (== uZM_MAX_PARTY_SIZE), so nothing
// heap-allocates and the caller passes m_axEnemyParty + m_uEnemyCount straight
// through. The array stays the FULL row cap wide; every authored member up to
// uZM_TRAINER_BATTLEABLE_PARTY can be filled.
struct ZM_TrainerBattleSetup
{
	ZM_BattleMonsterSpec	m_axEnemyParty[uZM_TRAINER_MAX_PARTY];
	u_int					m_uEnemyCount  = 0u;                 // 1..uZM_TRAINER_BATTLEABLE_PARTY when valid
	ZM_AI_TIER				m_eEnemyTier   = ZM_AI_TIER_NONE;    // -> Begin()'s 7th argument
	u_int64					m_ulBattleSeed = 0u;                 // -> Begin()'s 6th argument
	ZM_SPECIES_ID			m_eLeadSpecies = ZM_SPECIES_NONE;    // -> PlaceCreatureModels
	bool					m_bValid       = false;              // false == do NOT Begin
};

// Build the whole trainer-battle input from the roster row. Each authored
// {species, level} pair goes through the SHIPPED ZM_BuildWildEnemySpec, which
// derives IVs 31 / EVs 0 / nature FERAL / ability NONE / the learnset moveset
// with ZERO randomness -- so the returned party is a fixed, reproducible team.
//
// TOTAL: an unregistered id (including the ZM_TRAINER_NONE sentinel), a null row
// party pointer, or a zero party count all return the default setup
// (m_bValid == false, m_uEnemyCount == 0, tier NONE, seed 0, lead NONE) with no
// log and no assert. A malformed row whose count exceeds the shared storage cap
// is clamped defensively; every valid authored member is otherwise fielded in
// row order, with member zero as the lead.
ZM_TrainerBattleSetup	ZM_BuildTrainerBattleSetup(ZM_TRAINER_ID eTrainer);

// The trainer battle seed: an FNV-1a fold that MIRRORS the shape of
// ZM_BattleDirector::DeriveBattleSeed(species, level) but folds a trainer DOMAIN
// SALT first, so the two seed spaces are provably disjoint (a naive id-only fold
// would collide with the wild fold of species 0). Deterministic and TOTAL for
// every id, including unregistered ones.
u_int64					ZM_DeriveTrainerBattleSeed(ZM_TRAINER_ID eTrainer);
