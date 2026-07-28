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

// ---- The count the ENGINE can actually resolve, which is NOT the row cap -----
//
// ZM_BattleEngine has NO forced replacement on faint. m_uActiveSlot is written in
// exactly two places -- ZM_BattleEngine::Begin (0) and DoSwitch, and DoSwitch is
// reached only from a VOLUNTARY ZM_ACTION_SWITCH or a move's forced-switch
// secondary -- while ResolveTurn ends the battle only when a whole side has NO
// unfainted member (HasUnfainted() scans the entire party). So with two or more
// enemy members: the player KOs the enemy active, the enemy side still
// HasUnfainted(), the battle does NOT end, the director returns to AWAIT_INPUT
// with m_uActiveSlot still pointing at the corpse, and the next
// SubmitPlayerAction's unconditional m_xEngine.SubmitAction(ZM_SIDE_ENEMY, ...)
// hits `Zenith_Assert(!xActive.IsFainted(), "SubmitAction: active monster is
// fainted")` -- a Zenith_DebugBreak() in EVERY configuration, above the
// action-kind dispatch, so not even a SWITCH reply dodges it.
//
// That gap is the already-documented deferral (Docs/Shortfalls.md item (c),
// "single-LEAD battles only -- multi-member parties + forced-switch-on-faint
// remain future work"), and SC5 is the first commit that could make a
// multi-member party REACHABLE at Begin. It does not: the built party is clamped
// to what the engine can finish, so an authored row may carry a bench the game
// simply does not field yet. Content keeps its authored rows; the engine is never
// handed a battle it cannot end.
//
// RAISE THIS TO uZM_TRAINER_MAX_PARTY in the commit that adds
// forced-switch-on-faint -- not before, and never independently of it.
inline constexpr u_int uZM_TRAINER_BATTLEABLE_PARTY = 1u;
static_assert(uZM_TRAINER_BATTLEABLE_PARTY >= 1u
	&& uZM_TRAINER_BATTLEABLE_PARTY <= uZM_TRAINER_MAX_PARTY,
	"the battleable count must be at least one monster and must fit the row cap");

// The complete Begin() input for one trainer. Fixed-size by construction: the
// party array is uZM_TRAINER_MAX_PARTY wide (== uZM_MAX_PARTY_SIZE), so nothing
// heap-allocates and the caller passes m_axEnemyParty + m_uEnemyCount straight
// through. The array stays the FULL row cap wide even though only
// uZM_TRAINER_BATTLEABLE_PARTY entries are filled today, so raising that constant
// is a one-line change here and nowhere else.
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
// log and no assert. A row whose count exceeds uZM_TRAINER_BATTLEABLE_PARTY is
// CLAMPED here, because nothing downstream clamps: ZM_BattleEngine::Begin loops
// uEnemyCount with no upper bound of its own AND cannot resolve a battle whose
// active faints with a bench behind it (see the constant's comment above).
// Members are taken from the FRONT of the row, so the authored lead is always the
// monster fielded.
ZM_TrainerBattleSetup	ZM_BuildTrainerBattleSetup(ZM_TRAINER_ID eTrainer);

// The trainer battle seed: an FNV-1a fold that MIRRORS the shape of
// ZM_BattleDirector::DeriveBattleSeed(species, level) but folds a trainer DOMAIN
// SALT first, so the two seed spaces are provably disjoint (a naive id-only fold
// would collide with the wild fold of species 0). Deterministic and TOTAL for
// every id, including unregistered ones.
u_int64					ZM_DeriveTrainerBattleSeed(ZM_TRAINER_ID eTrainer);
