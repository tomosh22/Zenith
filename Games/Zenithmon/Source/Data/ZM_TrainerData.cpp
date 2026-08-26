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
//   challengeLines, challengeLineCount, badgeReward, itemReward
//
// The roster opened at TWO rows -- the authored rival, and one generic route
// trainer that carries NO story flag. One row would let a "walk every row" unit
// pass while the accessors only ever saw index 0, and it would leave the
// ZM_STORY_FLAG_NONE arm of the defeat-flag column entirely unexercised. S8's Gym 1
// slice added a THIRD, Fenna, and that shape still holds: the silent / unflagged
// arms are still live content through the rambler, and the three rows now differ in
// party count (1 / 2 / 3) and in AI tier (GREEDY / RANDOM / SMART), so no column
// here is a constant a builder could fake.
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

	// Gym 1's leader (S8, slice G1-1). Two GDD rows fix the three columns that
	// matter here, and they are NOT interchangeable: GDD 194 is the badge table
	// (leader / type / town / badge / teach-move, and NO level), which is what makes
	// Fenna the GRASS leader of Thornacre; GDD 433 is the pacing table, and it is
	// the only row carrying a number -- her team at L13 against Routes 1-2 at L2-8.
	// That level is the first real step up, and the reason the room in front of her
	// is a maze rather than a corridor.
	//
	// THREE members, weakest-first with the ace last, all GRASS-primary and all
	// distinct species. Three is also a party count neither shipped row carries
	// (Vesper brings 1, the rambler 2), which keeps the roster exercising
	// m_uPartyCount as a genuine variable rather than a two-valued flag.
	//
	// ★ ALL THREE ARE AT THE SAME LEVEL 13, WHICH IS THE GDD FIGURE VERBATIM. Do not
	// "improve" this by giving the ace a level of its own: the GDD states one number
	// for Gym 1, ZM_Tests_TrainerData pins that number, and a leader whose ace is
	// off-spec is a balance decision nobody has made.
	const ZM_TrainerPartyMember s_axPartyFenna[] =
	{
		{ ZM_SPECIES_SPORELING,  13u },   // Grass/Umbral, stage 1 -- the opener
		{ ZM_SPECIES_DANDELIFT,  13u },   // Grass/Sky,    stage 2
		{ ZM_SPECIES_MANTISPRIG, 13u },   // Grass/Swarm,  stage 2 -- the ace
	};

	// The data is entirely compile-time, so an author who pastes a seventh member
	// should find out at BUILD time rather than at boot. The runtime unit stays --
	// it still covers rows added later, and rows whose array is not named here.
	static_assert(ZM_ARRLEN(s_axPartyVesper)  <= uZM_TRAINER_MAX_PARTY, "Vesper's party outgrew the engine party cap");
	static_assert(ZM_ARRLEN(s_axPartyRambler) <= uZM_TRAINER_MAX_PARTY, "the rambler's party outgrew the engine party cap");
	static_assert(ZM_ARRLEN(s_axPartyFenna)   <= uZM_TRAINER_MAX_PARTY, "Fenna's party outgrew the engine party cap");

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

	// Fenna's bark, under the SAME <= 40-character budget and for the same reason:
	// the typewriter rate is a hard 45 chars/sec that ZM_SetInstantBattlesForTests
	// cannot touch, so every character she speaks costs the walk-up test real frames.
	// Two lines, in the voice GDD 205 gives her -- Thornacre's youngest-ever leader,
	// who grew the maze the player has just solved to reach her.
	const char* const s_aszChallengeFenna[] =
	{
		"You found your way through my maze!",
		"Now let's see you through my team.",
	};
	static_assert(ZM_ARRLEN(s_aszChallengeFenna) <= uZM_TRAINER_MAX_CHALLENGE_LINES,
		"Fenna's challenge outgrew the dialogue queue -- QueueLines is all-or-nothing, so she would go MUTE");

	// The bound is DEDUCED, never spelled. With an explicit [ZM_TRAINER_COUNT] the
	// static_assert below would be a tautology -- true by construction -- and a
	// forgotten row would merely zero-initialise the tail into a nameless trainer
	// with a null party. Deduced, a missing or extra row is a COMPILE error at the
	// table itself.
	const ZM_TrainerData s_axTrainers[] =
	{
		// Neither Vesper nor the rambler awards a badge or an item -- their two
		// trailing columns default to ZM_BADGE_NONE / ZM_ITEM_NONE (ZM_TrainerData.h),
		// spelled out here rather than left to the default for the same reason every
		// other column in this table is spelled explicitly: a reader should not have
		// to open the header to know what a row does NOT do.
		{ ZM_TRAINER_RIVAL_VESPER,   "Vesper",         s_axPartyVesper,  ZM_ARRLEN(s_axPartyVesper),  500u, ZM_STORY_FLAG_RIVAL1_DEFEATED, ZM_AI_TIER_GREEDY, s_aszChallengeVesper, ZM_ARRLEN(s_aszChallengeVesper), ZM_BADGE_NONE, ZM_ITEM_NONE },
		// SILENT BY DESIGN. The generic route trainer is the production instance of
		// the "no lines -> straight to the battle" arm, so that arm is live content
		// rather than only a unit fixture.
		{ ZM_TRAINER_ROUTE1_RAMBLER, "Rambler Perrin", s_axPartyRambler, ZM_ARRLEN(s_axPartyRambler), 120u, ZM_STORY_FLAG_NONE,            ZM_AI_TIER_RANDOM, nullptr,              0u,                               ZM_BADGE_NONE, ZM_ITEM_NONE },
		// GYM 1'S LEADER (S8, slice G1-1).
		//
		// ★ THE DEFEAT FLAG IS THE ONE THAT ALREADY EXISTS. ZM_STORY_FLAG_GYM1_DEFEATED
		// has been registered since S6 (ZM_StoryFlags.h, wire bit 5, with its name row
		// and its ZM_IsMilestoneStoryFlag arm already in place) precisely so this row
		// could name it without touching ZM_StoryFlags at all. Appending a new flag id
		// would move ZM_STORY_FLAG_COUNT, which is save-affecting and separately pinned
		// by Tests/ZM_Tests_StoryFlags.cpp.
		//
		// ★ PRIZE 2600 = 200 x her team's level, which is the first prize in the game
		// that is neither a story beat's fixed purse (Vesper's 500) nor a route
		// trainer's pocket change (the rambler's 120). Comfortably under uZM_MONEY_CAP,
		// which matters because ZM_GameState::AddMoney is a NO-OP over cap, not a clamp.
		//
		// ★ AI TIER SMART, AND THAT IS A THIRD DISTINCT TIER ON PURPOSE. A leader with
		// three monsters is the first opponent in the game for whom "switch out of a
		// hopeless matchup" is a real decision, which is exactly what SMART adds over
		// GREEDY. It also keeps Setup_TierIsTheRowsTier's anti-vacuity clause honest:
		// a roster where every row shared a tier could not tell a row read from a
		// constant-returning builder.
		//
		// She BARKS, like the rival and unlike the rambler -- the silent arm stays live
		// content through ZM_TRAINER_ROUTE1_RAMBLER, which is what
		// TrainerData_ChallengeColumnsAreWellFormedAndBothArmsExist requires.
		//
		// ★ S8 G1-3 (ZM-70): THE FIRST ROW TO CARRY A REAL REWARD. Beating Fenna awards
		// ZM_BADGE_BLOOM (GDD 194) and grants ZM_ITEM_TM_VERDANTLASH -- the TM whose own
		// m_eEffect is ZM_ITEM_EFFECT_TEACH_MOVE (ZM_ItemData.cpp), i.e. the player
		// receives the teach-move item exactly as a gym leader hands one over in the
		// genre. ZM_ApplyTrainerResultToGameState (ZM_BattleWriteBack.cpp) reads both
		// columns straight off this row; there is no per-trainer branch anywhere else.
		{ ZM_TRAINER_GYM1_FENNA,     "Fenna",          s_axPartyFenna,   ZM_ARRLEN(s_axPartyFenna),   2600u, ZM_STORY_FLAG_GYM1_DEFEATED,  ZM_AI_TIER_SMART,  s_aszChallengeFenna,  ZM_ARRLEN(s_aszChallengeFenna),  ZM_BADGE_BLOOM, ZM_ITEM_TM_VERDANTLASH },
	};

	static_assert(sizeof(s_axTrainers) / sizeof(s_axTrainers[0]) == ZM_TRAINER_COUNT,
		"the roster must have exactly one row per ZM_TRAINER_ID");

	// Handed back for an unregistered id. The alternative -- indexing anyway after
	// an assert -- reads off the end of the table the moment the break is stepped
	// past, and a roster whose job is to make bad ids safe should not have that
	// hole. Every field is the INERT answer: no party, no prize, no flag written,
	// and ZM_AI_TIER_NONE, so a caller that ignores the id check still cannot start
	// a battle out of it -- and, since SC7, no challenge lines either, and since
	// S8 G1-3, no badge or item either, so the UNKNOWN row is silent, rewardless
	// and inert.
	const ZM_TrainerData s_xInvalidTrainer =
	{
		ZM_TRAINER_NONE, "UNKNOWN", nullptr, 0u, 0u, ZM_STORY_FLAG_NONE, ZM_AI_TIER_NONE, nullptr, 0u,
		ZM_BADGE_NONE, ZM_ITEM_NONE
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
