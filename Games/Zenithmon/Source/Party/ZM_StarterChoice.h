#pragma once

#include "Zenithmon/Source/Data/ZM_SpeciesData.h"   // ZM_SPECIES_ID / ZM_SPECIES_NONE
#include "Zenithmon/Source/Party/ZM_GameState.h"    // ZM_GameState -- the grant target + ZM_CanEnterBattle's input

// ============================================================================
// ZM_StarterChoice (S8 item 1 SC3) -- WHICH starter the player owns, and the one
// predicate that asks whether they own anything at all.
//
// CONTENT ONLY, in the ZM-D-009 compiled-const-table idiom ZM_TrainerData /
// ZM_NpcData / ZM_WorldSpec already use: an enum, a compiled `const` C array of
// rows, and free-function accessors. NO ECS, NO scene, NO UI, NO RNG, NO
// allocation, NO I/O.
//
// EVERY function below is TOTAL and NEVER calls Zenith_Assert. Zenith_Assert
// calls Zenith_DebugBreak() in EVERY configuration (Zenith.h), so an assert on a
// unit-supplied value does not report a bug -- it ENDS the whole boot unit run.
// A bad value that indicates MIS-AUTHORED DATA is diagnosed with a non-fatal
// Zenith_Error and answered with the shared INVALID row; that is exactly the
// ruling ZM_TrainerData.h already states for the roster.
//
// * ZM_ApplyStarterChoice MUST validate BEFORE it reaches ZM_BuildMonsterRecord,
// which Zenith_Asserts on both its arguments (ZM_Monster.cpp:55-58) and then
// FALLS THROUGH into ZM_GetSpeciesAbilities / ZM_GetSpeciesGrowthRate /
// ZM_GetSpeciesLearnset, each of which bounds-asserts and then reads out of
// range. Validation is a genuine early return, never "the assert will catch it".
// ============================================================================

// Every starter is granted at this level. ONE constant, not a per-row column: a
// column would invite the three rows to drift apart, and the level is a code
// fact (it appears nowhere in Docs/GameDesignDocument.md section 4), inherited
// verbatim from the deleted ZM_MakeStarterGameState.
inline constexpr u_int uZM_STARTER_LEVEL = 5u;

// The authored trio (GameDesignDocument.md section 4: F01/F02/F03). APPEND ONLY
// before ZM_STARTER_CHOICE_COUNT. Nothing persists these ordinals in SC3 -- but
// the enums next door are append-only and there is no reason for two doctrines.
enum ZM_STARTER_CHOICE : u_int
{
	ZM_STARTER_CHOICE_FERNFAWN = 0u,   // F01, Grass
	ZM_STARTER_CHOICE_KINDLET,         // F02, Fire
	ZM_STARTER_CHOICE_FINLET,          // F03, Water

	ZM_STARTER_CHOICE_COUNT,
	ZM_STARTER_CHOICE_NONE = ZM_STARTER_CHOICE_COUNT   // "no choice" sentinel
};

// One authored starter row.
//
// COLUMN ORDER IS LOAD-BEARING -- every row is a POSITIONAL aggregate
// initializer, so a field inserted mid-struct silently shifts each trailing
// value one column left with no compile error. New columns go AT THE END.
//
// m_eCounteredBy is AUTHORED, not derived. Deriving it by querying
// ZM_TypeChart would make Counter_ColumnAgreesWithTheTypeChart a tautology that
// can never fail; authored, that unit is a real tripwire over both the column
// and the chart (GameDesignDocument.md section 3.3: "Vesper takes the family
// strong against the player's pick").
struct ZM_StarterChoiceData
{
	ZM_STARTER_CHOICE	m_eChoice;
	ZM_SPECIES_ID		m_eSpecies;
	ZM_STARTER_CHOICE	m_eCounteredBy;   // the choice whose species BEATS m_eSpecies
};

// TOTAL. True only for 0..ZM_STARTER_CHOICE_COUNT-1; ZM_STARTER_CHOICE_NONE
// aliases the count, so one comparison rejects the sentinel and every garbage
// value together. Mirrors ZM_IsRegisteredTrainer exactly.
bool ZM_IsRegisteredStarterChoice(ZM_STARTER_CHOICE eChoice);

// == ZM_STARTER_CHOICE_COUNT. The walk bound every totality unit iterates to.
u_int ZM_GetStarterChoiceCount();

// TOTAL. An unregistered choice yields the shared INVALID row --
// { ZM_STARTER_CHOICE_NONE, ZM_SPECIES_NONE, ZM_STARTER_CHOICE_NONE } -- rather
// than a table read, and logs a NON-FATAL Zenith_Error because such a lookup
// means mis-authored data or a mis-typed caller. NEVER asserts.
const ZM_StarterChoiceData& ZM_GetStarterChoice(ZM_STARTER_CHOICE eChoice);

// TOTAL. The species the PLAYER receives for this choice; ZM_SPECIES_NONE for an
// unregistered choice (the INVALID row's species). Logs through ZM_GetStarterChoice.
ZM_SPECIES_ID ZM_ResolvePlayerStarterSpecies(ZM_STARTER_CHOICE eChoice);

// TOTAL. The species the RIVAL brings against this choice -- the species of the
// row named by m_eCounteredBy. ZM_SPECIES_NONE for an unregistered choice, and
// also for a row whose m_eCounteredBy is itself unregistered (fail closed twice,
// never index twice). NOTHING CONSUMES THIS IN SC3: the shipped Vesper row
// (ZM_TrainerData.cpp, { ZM_SPECIES_KINDLET, 5u }) stays a literal so behaviour
// does not move; a unit asserts the two agree.
ZM_SPECIES_ID ZM_ResolveCounterStarterSpecies(ZM_STARTER_CHOICE eChoice);

// TOTAL. "Does the player own anything to send out?"
//
// * IT KEYS ON EMPTINESS ALONE, AND THAT IS A RULING, NOT AN OVERSIGHT. The
// fainted-aware form (!IsEmpty() && !AllFainted()) is NOT inert:
// ZM_BattleWriteBack.cpp latches m_bPendingWhiteout on a loss and the heal only
// runs later, in ZM_GameStateManager::OnUpdate once the transition is IDLE -- so
// an entirely fainted party is a REAL window that ZM_Interactable::TickTrainerSight
// ticks through every frame. Keying on fainting would change m_bMayEngage during
// those frames, which SC3 forbids. This form matches, byte for byte, the test
// ZM_BattleDirector already makes in RunSetup / RunTrainerSetup.
// (Note ZM_Party::AllFainted() answers TRUE for an empty party -- "vacuously
// all-fainted" -- which makes the wrong form look tempting and correct.)
bool ZM_CanEnterBattle(const ZM_GameState& xState);

// TOTAL. Grants the chosen starter: exactly one ZM_BuildMonsterRecord(species,
// uZM_STARTER_LEVEL) appended to the party, and MarkCaught(species) -- which
// marks SEEN AND CAUGHT together (ZM_GameState.h). Marking only m_xCaught would
// trip ZM_SaveSchema's caughtImpliesSeen validation (ZM_SaveSchema.cpp:449-457)
// and make every save write fail.
//
// Returns TRUE when the grant happened. FALSE, with NO mutation whatsoever, for
// an unregistered choice (the ZM_BuildMonsterRecord assert-avoidance path) or a
// full party. On the full-party path it returns BEFORE MarkCaught, so the dex can
// never record a species the party does not hold.
//
// It touches PARTY AND DEX ONLY. Money, bag, story flags, badges, boxes, daycare,
// tower, world position, options and m_bPendingWhiteout belong to
// ZM_MakeNewGameState and are not this function's business -- in particular it
// sets NO story flag. ZM_STORY_FLAG_STARTER_RECEIVED exists (ZM_StoryFlags.h) and
// is deliberately LEFT CLEAR: setting it would move the save bytes and break
// SC3's behaviour-preservation contract. A later sub-commit owns that beat.
//
// PRECONDITION (documented, not enforced): a state fresh from ZM_MakeNewGameState.
bool ZM_ApplyStarterChoice(ZM_GameState& xState, ZM_STARTER_CHOICE eChoice);
