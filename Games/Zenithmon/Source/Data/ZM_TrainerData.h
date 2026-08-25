#pragma once

#include "Zenithmon/Source/Battle/ZM_BattleAI.h"      // ZM_AI_TIER -- Begin()'s 7th argument, not a config field
#include "Zenithmon/Source/Battle/ZM_BattleTypes.h"   // uZM_MAX_PARTY_SIZE -- the cap this row DERIVES from
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"     // ZM_SPECIES_ID
#include "Zenithmon/Source/Data/ZM_StoryFlags.h"      // ZM_STORY_FLAG_ID / ZM_STORY_FLAG_NONE
#include "Zenithmon/Source/UI/ZM_UI_DialogueBox.h"    // uMAX_QUEUED_LINES -- the challenge cap is DERIVED from it

// ============================================================================
// ZM_TrainerData (S7 item 3 SC2) -- the authored trainer roster: WHO the player
// can be made to battle, WHAT team they bring, WHAT it pays, and WHICH story
// flag their defeat writes.
//
// This is CONTENT ONLY, in the ZM-D-009 compiled-const-table idiom that
// ZM_NpcData / ZM_ItemData / ZM_WorldSpec already use: a save-stable enum, a
// compiled `const` C array of rows, and free-function accessors. There is NO
// ECS, NO scene, NO UI, NO RNG and NO allocation here -- per-row party arrays
// are static tables referenced by pointer + count, exactly as ZM_WorldSpec
// references its encounter arrays.
//
// A row stores ONLY the two primitives ZM_BuildWildEnemySpec(species, level)
// consumes (ZM_BattleDirectorCore.h): that function derives IVs 31 / EVs 0 /
// nature FERAL / ability NONE / the learnset moveset with ZERO randomness, so a
// party expressed as {species, level} pairs is already a FIXED, reproducible
// team. Per-monster overrides remain expressible on ZM_BattleMonsterSpec if
// content ever needs them; duplicating them here would be dead columns.
//
// Deliberately NOT here: a ZM_BattleConfig copy (SC3's helper is the single source
// of the battle rules), a battle seed (SC5 derives one from m_eId), and an
// appearance id -- SC8 stood Vesper in Dawnmere WITHOUT one, because the NPC row
// that carries his scene identity (ZM_NpcData::m_eHuman) already names it and
// nothing renders it yet. Do not add a second appearance column here.
//
// EVERY function below is TOTAL: no argument value, however garbage, is UB and
// none of them asserts. Zenith_Assert breaks the process in EVERY configuration
// in this engine, and the boot units feed these functions the ZM_TRAINER_NONE
// sentinel and out-of-range ids on purpose to pin their fail-closed answers -- an
// assert on a unit-pinned input does not report a bug, it ENDS the boot unit run.
// A bad id that indicates MIS-AUTHORED DATA is diagnosed with a non-fatal
// Zenith_Error instead; a legitimate sentinel is not an error and logs nothing.
// ============================================================================

// Every authored trainer, in roster order. IDs are contiguous
// 0..ZM_TRAINER_COUNT-1 and the row index equals the id (asserted by
// ZM_Tests_TrainerData). APPEND ONLY before ZM_TRAINER_COUNT, never reorder: a
// later "trainers defeated" bitset persists these ordinals.
enum ZM_TRAINER_ID : u_int
{
	ZM_TRAINER_RIVAL_VESPER,     // the rival's first battle (Dawnmere); sets RIVAL1_DEFEATED
	ZM_TRAINER_ROUTE1_RAMBLER,   // a generic route trainer: no story flag, no fixed place in the plot
	ZM_TRAINER_GYM1_FENNA,       // Gym 1's leader (Thornacre); sets GYM1_DEFEATED

	ZM_TRAINER_COUNT,
	ZM_TRAINER_NONE = ZM_TRAINER_COUNT   // "no trainer" sentinel
};

// ---- Row caps (spelled once, HERE) ------------------------------------------

// `inline` so this has ONE definition across every TU: the ZENITH_ASSERT_*
// macros bind their operands by const reference, so the units odr-use it.
//
// The cap is DERIVED from the engine's party size rather than re-spelled,
// because the two must never drift: ZM_BattleEngine::Begin loops uEnemyCount
// without an upper bound of its own, so an over-cap row is not clamped anywhere
// -- it is simply a party the rest of the game's party-sized buffers cannot hold.
inline constexpr u_int uZM_TRAINER_MAX_PARTY = uZM_MAX_PARTY_SIZE;

// DERIVED, never re-spelled, for the same reason uZM_NPC_MAX_LINES is
// (ZM_NpcData.h:67): ZM_UI_DialogueBox::QueueLines is ALL-OR-NOTHING, so a row
// that outgrew the queue would not lose its last line -- it would leave that
// trainer completely MUTE, and a mute trainer under a fail-open window is a
// silent half-second of dead air rather than a visible bug.
inline constexpr u_int uZM_TRAINER_MAX_CHALLENGE_LINES = ZM_UI_DialogueBox::uMAX_QUEUED_LINES;

// One member of a fixed enemy party. Species + level is the WHOLE authoring
// surface: ZM_BuildWildEnemySpec turns this pair into a complete
// ZM_BattleMonsterSpec deterministically.
struct ZM_TrainerPartyMember
{
	ZM_SPECIES_ID	m_eSpecies;
	u_int			m_uLevel;      // [uZM_MIN_LEVEL, uZM_MAX_LEVEL], boot-tested
};

// One authored trainer row. m_paxParty points at a per-row static table and is
// never null for a registered row (a trainer with no team cannot battle);
// nothing here allocates or owns.
//
// COLUMN ORDER IS LOAD-BEARING. Every row is a POSITIONAL aggregate initializer,
// so a field inserted mid-struct silently shifts each trailing value one column
// left -- the prize would become the flag and the flag the tier, with no compile
// error to say why. New columns go AT THE END.
struct ZM_TrainerData
{
	ZM_TRAINER_ID					m_eId;
	const char*						m_szDisplayName;   // shown in dialogue / HUD / debug
	const ZM_TrainerPartyMember*	m_paxParty;        // fixed team, row-owned (never null when registered)
	u_int							m_uPartyCount;     // 1..uZM_TRAINER_MAX_PARTY
	u_int							m_uPrizeMoney;     // EXPLICIT per-row u32 (Q-E: no formula); > 0
	ZM_STORY_FLAG_ID				m_eDefeatFlag;     // set on defeat; ZM_STORY_FLAG_NONE == writes no flag
	ZM_AI_TIER						m_eAITier;         // straight into ZM_BattleDirectorCore::Begin's 7th arg

	// ---- S7 item 3 SC7: the pre-battle challenge bark --------------------------
	// APPENDED AT THE END. The struct's own warning above is binding: every row is a
	// POSITIONAL aggregate initializer, so a column inserted mid-struct silently
	// shifts the prize into the flag and the flag into the tier with no compile
	// error. Null array <=> zero count; a row with (nullptr, 0u) battles in SILENCE
	// and skips the whole beat, which is a supported authoring choice, not an error
	// (ZM_TRAINER_ROUTE1_RAMBLER ships exactly that).
	const char* const*				m_paszChallengeLines;  // null when the row barks nothing
	u_int							m_uChallengeLineCount; // 0 == silent; <= uZM_TRAINER_MAX_CHALLENGE_LINES
};

// TOTAL: an unregistered id (including the sentinel) yields the shared UNKNOWN
// row -- { ZM_TRAINER_NONE, "UNKNOWN", nullptr, 0u, 0u, ZM_STORY_FLAG_NONE,
// ZM_AI_TIER_NONE, nullptr, 0u } -- rather than a table read, and logs a non-fatal
// Zenith_Error because such a lookup means mis-authored data or a mis-typed
// caller.
const ZM_TrainerData&	ZM_GetTrainerData(ZM_TRAINER_ID eId);
u_int					ZM_GetTrainerCount();                        // == ZM_TRAINER_COUNT
// TOTAL: "NONE" for the sentinel, "UNKNOWN" out of range, and SILENT for both --
// naming a trainer is what callers do WITH a bad id, not a second place to
// complain about it. Never returns null, because every caller is a log argument.
const char*				ZM_GetTrainerName(ZM_TRAINER_ID eId);
// TOTAL. True only for 0..ZM_TRAINER_COUNT-1; ZM_TRAINER_NONE aliases
// ZM_TRAINER_COUNT, so the sentinel and every garbage value are rejected by one
// comparison. Callers that hold an id from data or a component use this before
// building a battle.
bool					ZM_IsRegisteredTrainer(ZM_TRAINER_ID eId);

// The row's challenge lines, with the SAME two guarantees ZM_SelectNpcLines gives
// (ZM_NpcData.h:115-127) and for the same reasons:
//   * COUNT is clamped to uZM_TRAINER_MAX_CHALLENGE_LINES, because QueueLines is
//     all-or-nothing and rejects an over-cap push WHOLE.
//   * a NULL array yields count 0, so the pair can never contradict itself -- a
//     count that passes every check attached to a pointer that does not.
// TOTAL, and NEVER asserts: the boot units feed it hand-built rows with null
// arrays and absurd counts on purpose.
void ZM_SelectTrainerChallengeLines(const ZM_TrainerData& xRow,
	const char* const*& paszLinesOut, u_int& uCountOut);
