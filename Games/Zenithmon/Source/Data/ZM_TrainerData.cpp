#include "Zenith.h"
#include "Zenithmon/Source/Data/ZM_TrainerData.h"

// A TRIPWIRE, not a live check: the header currently DEFINES uZM_TRAINER_MAX_PARTY
// as uZM_MAX_PARTY_SIZE, so today this is true by construction and cannot fire.
// It earns its keep only if the derivation is ever replaced by a literal -- at
// which point it is the one thing that catches the two drifting apart. It is NOT
// what bounds authored content: the per-array static_asserts below and the runtime
// ZENITH_ASSERT_LE in ZM_Tests_TrainerData are.
static_assert(uZM_TRAINER_MAX_PARTY == uZM_MAX_PARTY_SIZE,
	"the row party cap and the engine party cap have drifted apart");

// ============================================================================
// ZM_TrainerData -- the authored trainer roster (S7 item 3 SC2). Rows are in
// ZM_TRAINER_ID order; s_axTrainers[i].m_eId == i is asserted by the tests.
// Per-row party arrays are static and referenced by pointer + count, the same
// shape ZM_WorldSpec uses for its per-scene tables.
// Column legend:
//   id, "display name", party, partyCount, prize, defeatFlag, aiTier,
//   challengeLines, challengeLineCount
//
// The roster is deliberately TWO rows: the authored rival, and one generic route
// trainer that carries NO story flag. One row would let a "walk every row" unit
// pass while the accessors only ever saw index 0, and it would leave the
// ZM_STORY_FLAG_NONE arm of the defeat-flag column entirely unexercised.
//
// NOTHING IN THIS FILE MAY Zenith_Assert ON ITS ARGUMENTS. Zenith_Assert calls
// Zenith_DebugBreak() in EVERY configuration -- there is no build in which it
// degrades to a log. These functions are TOTAL by contract and the boot units
// pin that totality by feeding them the sentinel and out-of-range ids on
// purpose, so a defensive assert here does not catch a bug: it kills the process
// partway through the unit run at boot and takes the whole gate down with it.
// ============================================================================

namespace
{
#define ZM_ARRLEN(a) ((u_int)(sizeof(a) / sizeof((a)[0])))

	// -- fixed parties (species + level ONLY; ZM_BuildWildEnemySpec derives the rest) --

	// The rival mirrors the player: production grants FERNFAWN (Grass) at
	// uZM_STARTER_LEVEL -- the authority is the starter table in
	// ZM_StarterChoice.h, NOT ZM_GameState.cpp, which no longer names a species --
	// so Vesper brings the Fire counterpart at the same level. This row stays a
	// LITERAL rather than calling ZM_ResolveCounterStarterSpecies: the table is
	// compiled const data and a boot unit asserts the two agree, which is a real
	// tripwire, where a derived row would be a tautology.
	// One monster, because the player owns exactly one when this battle happens.
	const ZM_TrainerPartyMember s_axPartyVesper[] =
	{
		{ ZM_SPECIES_KINDLET, 5u },
	};

	// Two members, both Route 1 encounter species (ZM_WorldSpec.cpp), so the
	// generic row exercises a party count the rival's does not.
	const ZM_TrainerPartyMember s_axPartyRambler[] =
	{
		{ ZM_SPECIES_NIBBIN, 4u },
		{ ZM_SPECIES_PIPWIT, 4u },
	};

	// The data is entirely compile-time, so an author who pastes a seventh member
	// should find out at BUILD time rather than at boot. The runtime unit stays --
	// it still covers rows added later, and rows whose array is not named here.
	static_assert(ZM_ARRLEN(s_axPartyVesper)  <= uZM_TRAINER_MAX_PARTY, "Vesper's party outgrew the engine party cap");
	static_assert(ZM_ARRLEN(s_axPartyRambler) <= uZM_TRAINER_MAX_PARTY, "the rambler's party outgrew the engine party cap");

	// -- the pre-battle challenge barks (S7 item 3 SC7) --

	// TWO lines, each <= 40 characters, and that budget is not cosmetic. The
	// overworld typewriter rate is a hard `constexpr float fCHARS_PER_SEC = 45.0f`
	// (ZM_UI_BattleHUD.cpp:37) and ZM_UI_DialogueBox passes only its OWN
	// m_bRevealInstant -- it never consults ZM_InstantBattlesEnabled() the way the
	// battle log does (ZM_UI_BattleHUD.cpp:326-327). So ZM_SetInstantBattlesForTests
	// does NOTHING for a bark, and every authored character costs the walk-up test
	// real frames. Do NOT add a second reveal rate to work around this.
	const char* const s_aszChallengeVesper[] =
	{
		"Hey! You're not walking past me.",
		"Let's see what that starter can do.",
	};
	static_assert(ZM_ARRLEN(s_aszChallengeVesper) <= uZM_TRAINER_MAX_CHALLENGE_LINES,
		"Vesper's challenge outgrew the dialogue queue -- QueueLines is all-or-nothing, so he would go MUTE");

	// The bound is DEDUCED, never spelled. With an explicit [ZM_TRAINER_COUNT] the
	// static_assert below would be a tautology -- true by construction -- and a
	// forgotten row would merely zero-initialise the tail into a nameless trainer
	// with a null party. Deduced, a missing or extra row is a COMPILE error at the
	// table itself.
	const ZM_TrainerData s_axTrainers[] =
	{
		{ ZM_TRAINER_RIVAL_VESPER,   "Vesper",         s_axPartyVesper,  ZM_ARRLEN(s_axPartyVesper),  500u, ZM_STORY_FLAG_RIVAL1_DEFEATED, ZM_AI_TIER_GREEDY, s_aszChallengeVesper, ZM_ARRLEN(s_aszChallengeVesper) },
		// SILENT BY DESIGN. The generic route trainer is the production instance of
		// the "no lines -> straight to the battle" arm, so that arm is live content
		// rather than only a unit fixture.
		{ ZM_TRAINER_ROUTE1_RAMBLER, "Rambler Perrin", s_axPartyRambler, ZM_ARRLEN(s_axPartyRambler), 120u, ZM_STORY_FLAG_NONE,            ZM_AI_TIER_RANDOM, nullptr,              0u                              },
	};

	static_assert(sizeof(s_axTrainers) / sizeof(s_axTrainers[0]) == ZM_TRAINER_COUNT,
		"the roster must have exactly one row per ZM_TRAINER_ID");

	// Handed back for an unregistered id. The alternative -- indexing anyway after
	// an assert -- reads off the end of the table the moment the break is stepped
	// past, and a roster whose job is to make bad ids safe should not have that
	// hole. Every field is the INERT answer: no party, no prize, no flag written,
	// and ZM_AI_TIER_NONE, so a caller that ignores the id check still cannot start
	// a battle out of it -- and, since SC7, no challenge lines either, so the
	// UNKNOWN row is silent as well as inert.
	const ZM_TrainerData s_xInvalidTrainer =
	{
		ZM_TRAINER_NONE, "UNKNOWN", nullptr, 0u, 0u, ZM_STORY_FLAG_NONE, ZM_AI_TIER_NONE, nullptr, 0u
	};

#undef ZM_ARRLEN
}

bool ZM_IsRegisteredTrainer(ZM_TRAINER_ID eId)
{
	// ZM_TRAINER_NONE aliases ZM_TRAINER_COUNT, so this single comparison rejects
	// the sentinel and every garbage value together.
	return (u_int)eId < (u_int)ZM_TRAINER_COUNT;
}

const ZM_TrainerData& ZM_GetTrainerData(ZM_TRAINER_ID eId)
{
	if (!ZM_IsRegisteredTrainer(eId))
	{
		// A row lookup for an id the roster does not name is mis-authored data or a
		// mis-typed caller, so it is SAID OUT LOUD -- but non-fatally, because the
		// function is total and returning the UNKNOWN row is the contract.
		Zenith_Error(LOG_CATEGORY_GAMEPLAY,
			"[ZM_TrainerData] ZM_GetTrainerData: id %u is not a registered trainer "
			"-- returning the UNKNOWN row", (u_int)eId);
		return s_xInvalidTrainer;
	}
	return s_axTrainers[(u_int)eId];
}

u_int ZM_GetTrainerCount()
{
	return (u_int)ZM_TRAINER_COUNT;
}

const char* ZM_GetTrainerName(ZM_TRAINER_ID eId)
{
	// NONE is a legal value to name (a component field carries it constantly), so
	// it is distinguished from garbage rather than folded into it.
	if (eId == ZM_TRAINER_NONE)
	{
		return "NONE";
	}
	if (!ZM_IsRegisteredTrainer(eId))
	{
		return "UNKNOWN";
	}
	return s_axTrainers[(u_int)eId].m_szDisplayName;
}

void ZM_SelectTrainerChallengeLines(const ZM_TrainerData& xRow,
	const char* const*& paszLinesOut, u_int& uCountOut)
{
	paszLinesOut = xRow.m_paszChallengeLines;
	uCountOut    = xRow.m_uChallengeLineCount;

	// NULL first, THEN clamp -- the ZM_SelectNpcLines order. Clamping first would
	// leave a validated count attached to a bogus pointer.
	if (paszLinesOut == nullptr)
	{
		uCountOut = 0u;
	}
	else if (uCountOut > uZM_TRAINER_MAX_CHALLENGE_LINES)
	{
		uCountOut = uZM_TRAINER_MAX_CHALLENGE_LINES;
	}
}
