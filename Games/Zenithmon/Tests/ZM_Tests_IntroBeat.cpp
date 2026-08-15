#include "Zenith.h"

// ============================================================================
// ZM_Tests_IntroBeat -- S8 item 1's PLAYABLE INTRO BEAT (ZM-D-188), unit half.
//
// The beat: a new game starts with an EMPTY PARTY, the player walks out of
// PlayerHome into Dawnmere and on into Aster's lab, talks to the professor, and
// CHOOSES their first partner. Three story flags record it (INTRO_LEFT_HOME,
// MET_PROFESSOR, STARTER_RECEIVED) -- and this slice is the first thing in
// production that has ever set any of them.
//
// Everything in this file is PURE and headless: compiled const tables, free
// functions over a by-value ZM_GameState, and two pure statics. No ECS, no
// scene, no UI, no singleton, no disk, no baked asset -- so every fixture is
// hermetic and no RequestSkip is needed. NOTHING HERE CREATES AN ENTITY (the
// ZM_Tests_Interaction rule: the boot unit suite runs before scene authoring, and
// an entity-creating unit shifts the indices a committed .zscen is baked with).
//
// ★★ THREE THINGS THIS FILE EXISTS TO SAY, BECAUSE EACH IS EASY TO BELIEVE IS
// SOMEBODY ELSE'S PROPERTY AND NONE OF THEM IS:
//
// 1. **ZM_ApplyStarterChoice DOES NOT REFUSE A SECOND GRANT.** It refuses exactly
//    two things -- an unregistered choice and a FULL party (six slots). A state
//    that already received a starter is not refused; it silently receives a
//    SECOND monster. So the intro's one-shot property is NOT free, and
//    Intro_ApplyStarterChoiceItselfWouldHappilyGrantTwice below asserts that
//    directly so nobody re-derives the wrong belief from a green suite.
//
// 2. **THE ONE-SHOT GUARD IS ZM_NpcRaisesStarterChoice, IN THE RAISE PATH.** Not
//    the row's ZM_StoryGate: a story gate selects LINES only and cannot stop a
//    screen from being raised beside them. The units that pin the property are
//    therefore named for the ROUTING layer, which is where the behaviour lives.
//
// 3. **THE PARTYLESS STRETCH IS A REAL PLAYTHROUGH STATE NOW.** Between the front
//    door and the lab the player owns nothing, so ZM_CanEnterBattle is false and
//    ZM_MayTrainerEngage's bPlayerCanBattle arm -- which had units but had never
//    run in a playthrough -- is what keeps rival Vesper from challenging somebody
//    with nothing to send out.
//
// Category ZM_Intro, deliberately distinct from ZM_Starter / ZM_Story / ZM_Data:
// those cover the surfaces this beat COMPOSES, and a shared category would bury a
// regression in the composition among the units for its parts.
// ============================================================================

#include <cstring>   // strcmp -- the arrival-tag resolver's contract

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Components/ZM_Interactable.h"     // ZM_NpcRaisesStarterChoice / ZM_IntroStoryFlagForArrival (PURE statics -- nothing is constructed)
#include "Zenithmon/Components/ZM_UI_MenuStack.h"     // ZM_UI_MenuStack::ApplyStarterGrant (a PURE static over a state)
#include "Zenithmon/Source/Data/ZM_NpcData.h"
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"
#include "Zenithmon/Source/Data/ZM_StoryFlags.h"
#include "Zenithmon/Source/Data/ZM_TrainerData.h"
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"
#include "Zenithmon/Source/Interaction/ZM_TrainerSightFsm.h"   // ZM_MayTrainerEngage -- the partyless arm
#include "Zenithmon/Source/Party/ZM_GameState.h"
#include "Zenithmon/Source/Party/ZM_Monster.h"
#include "Zenithmon/Source/Party/ZM_StarterChoice.h"

namespace
{
	// The three intro flag indices, spelled as LITERALS. Never read back off the
	// enum and compared to itself: an expectation derived from the code under test
	// moves with it and can never fail. These values are the persisted WIRE bit
	// indices in save module 4, so a renumbering is a versioned codec change.
	constexpr u_int uINTRO_LEFT_HOME_INDEX  = 0u;
	constexpr u_int uINTRO_MET_PROF_INDEX   = 1u;
	constexpr u_int uINTRO_STARTER_INDEX    = 2u;

	// A NON-Fernfawn pick, used everywhere a grant is exercised. The old shipped
	// seed was Fernfawn, so asserting against Fernfawn would be satisfiable by the
	// very state the beat exists to delete.
	constexpr ZM_STARTER_CHOICE eNON_DEFAULT_CHOICE = ZM_STARTER_CHOICE_KINDLET;

	const ZM_NpcData& Npc(u_int u) { return ZM_GetNpcData((ZM_NPC_ID)u); }

	// A flag set with exactly one registered flag raised, built through
	// ZM_StoryFlagSet's OWN raw-index setter rather than ZM_SetStoryFlag -- so a
	// routing unit cannot inherit, and then agree with, a bug in the accessor it is
	// standing next to.
	ZM_StoryFlagSet MakeFlagSetWith(u_int uRawIndex)
	{
		ZM_StoryFlagSet xFlags;
		xFlags.Set(uRawIndex, true);
		return xFlags;
	}

	// Does this party hold a record of this species anywhere (not just at the lead)?
	bool PartyHoldsSpecies(const ZM_Party& xParty, ZM_SPECIES_ID eSpecies)
	{
		for (u_int u = 0u; u < xParty.Count(); ++u)
		{
			if (xParty.Get(u).m_eSpecies == eSpecies)
			{
				return true;
			}
		}
		return false;
	}

	// The PlayerHome -> Dawnmere edge's own spawn tag, read from the compiled table
	// exactly as the resolver under test reads it. Never spelled "FromHome" here:
	// a literal would keep this suite green through a world-table rename that had
	// already silently stopped the flag from ever being set.
	const char* HomeToDawnmereTag()
	{
		const ZM_WorldSpec& xHome = ZM_GetWorldSpec(ZM_SCENE_PLAYERHOME);
		if (xHome.m_pxConnections == nullptr)
		{
			return "";
		}
		for (u_int u = 0u; u < xHome.m_uConnectionCount; ++u)
		{
			if (xHome.m_pxConnections[u].m_eTarget == ZM_SCENE_DAWNMERE
				&& xHome.m_pxConnections[u].m_szSpawnTag != nullptr)
			{
				return xHome.m_pxConnections[u].m_szSpawnTag;
			}
		}
		return "";
	}
}

// ---- The starting state -----------------------------------------------------

// ★ THE UNIT THE WHOLE BEAT RESTS ON. If a new game hands out a starter, the lab
// choice is cosmetic: the picker would still raise (it refuses only a FULL party),
// the player would end up holding TWO monsters, and every downstream assertion
// about "the chosen species" would be satisfiable by the old seed.
ZENITH_TEST(ZM_Intro, Intro_NewGameStartsPartylessSoTheLabChoiceIsReal)
{
	const ZM_GameState xState = ZM_MakeNewGameState();

	ZENITH_ASSERT_EQ(xState.m_xParty.Count(), 0u,
		"a new game ships %u party member(s) -- the player is supposed to leave home "
		"with NOTHING and choose a starter from Professor Aster",
		xState.m_xParty.Count());
	ZENITH_ASSERT_TRUE(xState.m_xParty.IsEmpty(),
		"a new game's party does not report EMPTY");
	// The predicate the trainer/battle paths actually consult, asserted separately:
	// it keys on emptiness ALONE (never on fainting), and this is the state that
	// makes its false arm reachable for the first time.
	ZENITH_ASSERT_FALSE(ZM_CanEnterBattle(xState),
		"ZM_CanEnterBattle answers TRUE for a fresh new game, so the partyless "
		"stretch between the front door and the lab does not exist");

	// ...and the story is completely unwritten, which is what makes Aster's gate
	// select his PRE-starter lines on a first visit.
	ZENITH_ASSERT_EQ(xState.m_xStoryFlags.Count(), 0u,
		"a new game ships %u story-flag bit(s) already set",
		xState.m_xStoryFlags.Count());
}

// ★ THE ACCEPTED COST OF THE ABOVE, MADE INTO A GUARD. A partyless player walks
// past rival Vesper's 8 m sight cone on the way to the lab. This is the decision
// layer that keeps him silent, and it is driven with ZM_CanEnterBattle applied to
// the REAL new-game state rather than a hand-written `false` -- so re-adding a
// starter grant to production reds this unit instead of leaving it decorative.
ZENITH_TEST(ZM_Intro, Intro_PartylessNewGamePlayerIsNeverEngagedByATrainer)
{
	ZM_GameState xState = ZM_MakeNewGameState();
	const bool bCanBattleAtStart = ZM_CanEnterBattle(xState);
	ZENITH_ASSERT_FALSE(bCanBattleAtStart,
		"the fixture is not actually partyless, so the walk below proves nothing");

	ZENITH_ASSERT_GT(ZM_GetTrainerCount(), 0u,
		"the trainer roster is empty, so this walk is vacuous");

	// EVERY registered trainer, and BOTH the defeat-flag and session-latch arms
	// cleared -- i.e. the most permissive configuration there is. A partyless player
	// must still be refused by all of them.
	for (u_int u = 0u; u < ZM_GetTrainerCount(); ++u)
	{
		const ZM_TrainerData& xRow = ZM_GetTrainerData((ZM_TRAINER_ID)u);
		ZENITH_ASSERT_FALSE(
			ZM_MayTrainerEngage(xRow, /* defeatFlagSet */ false,
				/* sessionLatchSet */ false, bCanBattleAtStart),
			"trainer '%s' would engage a player with an EMPTY party -- the battle "
			"would open with nothing to send out",
			xRow.m_szDisplayName);
	}

	// THE ANTI-VACUITY ARM. Without it a ZM_MayTrainerEngage stubbed to return
	// false forever passes the walk above outright. Grant the starter and the same
	// rows must become engageable.
	ZENITH_ASSERT_TRUE(ZM_ApplyStarterChoice(xState, eNON_DEFAULT_CHOICE),
		"the starter grant the anti-vacuity arm depends on was refused");
	const bool bCanBattleAfterGrant = ZM_CanEnterBattle(xState);
	ZENITH_ASSERT_TRUE(bCanBattleAfterGrant,
		"ZM_CanEnterBattle still answers FALSE after a successful grant");

	u_int uEngageableAfterGrant = 0u;
	for (u_int u = 0u; u < ZM_GetTrainerCount(); ++u)
	{
		const ZM_TrainerData& xRow = ZM_GetTrainerData((ZM_TRAINER_ID)u);
		if (ZM_MayTrainerEngage(xRow, false, false, bCanBattleAfterGrant))
		{
			++uEngageableAfterGrant;
		}
	}
	ZENITH_ASSERT_GT(uEngageableAfterGrant, 0u,
		"no authored trainer engages even a fully-armed player -- the partyless walk "
		"above is vacuous, because nothing could have engaged in the first place");
}

// ---- The raise routing (ZM_NpcRaisesStarterChoice) --------------------------

// The dispatch mechanism, stated as a test: Aster is still a plain TALKER row and
// the picker is raised by an NPC-IDENTITY routing rule beside his dialogue, NOT by
// a fourth ZM_NPC_ROLE. Every other row in the roster must be untouched by it.
ZENITH_TEST(ZM_Intro, Intro_RoutingRaisesTheStarterScreenOnlyForTheProfessor)
{
	const ZM_StoryFlagSet xFresh;   // nothing has happened yet

	u_int uRaisers = 0u;
	for (u_int u = 0u; u < ZM_NPC_COUNT; ++u)
	{
		const bool bRaises = ZM_NpcRaisesStarterChoice((ZM_NPC_ID)u, xFresh);
		if (bRaises)
		{
			++uRaisers;
		}
		const bool bShouldRaise = (u == (u_int)ZM_NPC_PROF_ASTER);
		ZENITH_ASSERT_TRUE(bRaises == bShouldRaise,
			"'%s' answers %s to the starter-raise routing on a fresh save (expected %s)",
			Npc(u).m_szDisplayName,
			bRaises ? "TRUE" : "FALSE",
			bShouldRaise ? "TRUE" : "FALSE");
	}
	ZENITH_ASSERT_EQ(uRaisers, 1u,
		"%u roster row(s) raise the starter picker; there must be exactly one",
		uRaisers);

	// ...and he is still routed through the DIALOGUE seam, which is the half a new
	// role would have changed.
	ZENITH_ASSERT_EQ((u_int)ZM_RaiseKindForRole(
		ZM_GetNpcData(ZM_NPC_PROF_ASTER).m_eRole),
		(u_int)ZM_NPC_RAISE_DIALOGUE,
		"the professor no longer routes through the DIALOGUE seam -- the picker is "
		"supposed to be raised BESIDE his lines, never instead of them");
}

// ★★ THE ONE-SHOT GUARD, AND THE NAME SAYS WHERE IT LIVES. Not in the grant, not
// on the row's story gate: in the RAISE path.
ZENITH_TEST(ZM_Intro, Intro_RoutingRefusesASecondStarterOnceTheFlagIsSet)
{
	const ZM_StoryFlagSet xBefore;
	ZENITH_ASSERT_TRUE(ZM_NpcRaisesStarterChoice(ZM_NPC_PROF_ASTER, xBefore),
		"the professor does not offer a starter on a completely fresh save");

	const ZM_StoryFlagSet xAfter = MakeFlagSetWith(uINTRO_STARTER_INDEX);
	ZENITH_ASSERT_FALSE(ZM_NpcRaisesStarterChoice(ZM_NPC_PROF_ASTER, xAfter),
		"the professor still raises the starter picker after "
		"ZM_STORY_FLAG_STARTER_RECEIVED is set -- the intro can be replayed and the "
		"player can farm a monster per visit");

	// The guard must key on THIS flag and nothing else nearby. Setting either of the
	// other two intro flags must leave the offer standing.
	ZENITH_ASSERT_TRUE(ZM_NpcRaisesStarterChoice(
		ZM_NPC_PROF_ASTER, MakeFlagSetWith(uINTRO_LEFT_HOME_INDEX)),
		"leaving home closed the starter offer");
	ZENITH_ASSERT_TRUE(ZM_NpcRaisesStarterChoice(
		ZM_NPC_PROF_ASTER, MakeFlagSetWith(uINTRO_MET_PROF_INDEX)),
		"arriving at the lab closed the starter offer before anything was granted");

	// ...and the index the guard reads really is the one the enum names.
	ZENITH_ASSERT_EQ((u_int)ZM_STORY_FLAG_STARTER_RECEIVED, uINTRO_STARTER_INDEX,
		"ZM_STORY_FLAG_STARTER_RECEIVED moved off wire bit %u", uINTRO_STARTER_INDEX);
}

// ★ THE UNIT THAT STOPS THE WRONG BELIEF FROM BEING RE-DERIVED. It is tempting to
// read the one-shot property off the grant instead of off the routing. It is not
// there: this asserts, positively, that the shipped grant will hand out a SECOND
// monster to a state that already has one. Nothing here is a request to change
// ZM_ApplyStarterChoice -- its ~40 call sites depend on exactly this behaviour.
ZENITH_TEST(ZM_Intro, Intro_ApplyStarterChoiceItselfWouldHappilyGrantTwice)
{
	ZM_GameState xState = ZM_MakeNewGameState();

	ZENITH_ASSERT_TRUE(ZM_ApplyStarterChoice(xState, eNON_DEFAULT_CHOICE),
		"the first grant was refused on a fresh state");
	ZENITH_ASSERT_EQ(xState.m_xParty.Count(), 1u,
		"the first grant left %u party member(s)", xState.m_xParty.Count());

	// A SECOND call, same state, different choice. If this ever starts returning
	// false, the one-shot property has moved into the grant and the routing guard's
	// documentation (and the unit above) needs rewriting -- deliberately, not by
	// discovering it here.
	ZENITH_ASSERT_TRUE(ZM_ApplyStarterChoice(xState, ZM_STARTER_CHOICE_FERNFAWN),
		"ZM_ApplyStarterChoice refused a SECOND grant. That is a behaviour change: "
		"the intro's one-shot property is documented as living in "
		"ZM_NpcRaisesStarterChoice precisely because this call does not refuse");
	ZENITH_ASSERT_EQ(xState.m_xParty.Count(), 2u,
		"a second grant did not append a second monster (count %u)",
		xState.m_xParty.Count());
}

// TOTALITY, in this suite's idiom: no id, however degenerate, is UB and none of
// them raises a picker by accident.
ZENITH_TEST(ZM_Intro, Intro_RoutingIsTotalOverEveryIdIncludingTheSentinel)
{
	const ZM_StoryFlagSet xFresh;
	ZENITH_ASSERT_FALSE(ZM_NpcRaisesStarterChoice(ZM_NPC_NONE, xFresh),
		"the 'no NPC' sentinel raises the starter picker");
	ZENITH_ASSERT_FALSE(
		ZM_NpcRaisesStarterChoice((ZM_NPC_ID)((u_int)ZM_NPC_COUNT + 7u), xFresh),
		"an out-of-range npc id raises the starter picker");
	ZENITH_ASSERT_FALSE(ZM_NpcRaisesStarterChoice((ZM_NPC_ID)9999u, xFresh),
		"a garbage npc id raises the starter picker");
}

// ---- Aster's two line sets --------------------------------------------------

// POINTER IDENTITY on both polarities, because the two arrays are trivially easy
// to author the wrong way round and every roster-wide unit passes either way: they
// check that BOTH sets exist and fit the queue, never which is which.
ZENITH_TEST(ZM_Intro, Intro_AsterSpeaksHisPreStarterSetUntilTheFlagIsSet)
{
	const ZM_NpcData& xAster = ZM_GetNpcData(ZM_NPC_PROF_ASTER);
	ZENITH_ASSERT_NOT_NULL(xAster.m_paszLines, "the professor has no ordinary lines");
	ZENITH_ASSERT_NOT_NULL(xAster.m_paszGatedLines,
		"the professor authors no gated line set, so he says the same thing before "
		"and after the grant");

	const char* const* paszLines = nullptr;
	u_int uCount = 0u;

	// BEFORE the grant: the gate FAILS, so the selector must hand back the GATED
	// array -- which is the PRE-starter set.
	const ZM_StoryFlagSet xBefore;
	ZM_SelectNpcLines(xAster, xBefore, paszLines, uCount);
	ZENITH_ASSERT_TRUE(paszLines == xAster.m_paszGatedLines,
		"before the grant the professor speaks his ORDINARY set -- the two arrays "
		"are the wrong way round, and he greets a first-time player as though they "
		"had already chosen");
	ZENITH_ASSERT_EQ(uCount, xAster.m_uGatedLineCount,
		"the pre-starter line count does not match the gated array");
	ZENITH_ASSERT_GT(uCount, 0u, "the professor is MUTE on a first visit");

	// AFTER: the gate PASSES, so the ordinary set.
	const ZM_StoryFlagSet xAfter = MakeFlagSetWith(uINTRO_STARTER_INDEX);
	ZM_SelectNpcLines(xAster, xAfter, paszLines, uCount);
	ZENITH_ASSERT_TRUE(paszLines == xAster.m_paszLines,
		"after the grant the professor still speaks his PRE-starter set, so he keeps "
		"offering a starter he has already handed over");
	ZENITH_ASSERT_EQ(uCount, xAster.m_uLineCount,
		"the post-starter line count does not match the ordinary array");
	ZENITH_ASSERT_GT(uCount, 0u, "the professor is MUTE after the grant");

	// The two sets must actually DIFFER. Authoring the same array twice satisfies
	// every clause above while the beat reads identically on both sides of the gate.
	ZENITH_ASSERT_TRUE(xAster.m_paszLines != xAster.m_paszGatedLines,
		"the professor's two line sets are the SAME array");
	ZENITH_ASSERT_NOT_NULL(xAster.m_paszLines[0], "the post-starter set opens with a null line");
	ZENITH_ASSERT_NOT_NULL(xAster.m_paszGatedLines[0], "the pre-starter set opens with a null line");
	ZENITH_ASSERT_TRUE(
		std::strcmp(xAster.m_paszLines[0], xAster.m_paszGatedLines[0]) != 0,
		"the professor's two line sets open with identical text");
}

// ---- The grant + its flag (ZM_UI_MenuStack::ApplyStarterGrant) --------------

ZENITH_TEST(ZM_Intro, Intro_StarterGrantAppendsTheChosenSpeciesAndSetsTheFlag)
{
	ZM_GameState xState = ZM_MakeNewGameState();

	ZENITH_ASSERT_TRUE(
		ZM_UI_MenuStack::ApplyStarterGrant(xState, eNON_DEFAULT_CHOICE),
		"the grant was refused on a fresh new game");

	ZENITH_ASSERT_EQ(xState.m_xParty.Count(), 1u,
		"the grant left %u party member(s) instead of one", xState.m_xParty.Count());

	// ★ THE CHOSEN SPECIES, AND **NOT** FERNFAWN. Fernfawn is what the deleted seed
	// handed out, so an assertion that only checked "the party is non-empty" would be
	// satisfied by the exact state this beat exists to remove.
	const ZM_SPECIES_ID eExpected = ZM_ResolvePlayerStarterSpecies(eNON_DEFAULT_CHOICE);
	ZENITH_ASSERT_NE((u_int)eExpected, (u_int)ZM_SPECIES_FERNFAWN,
		"the fixture picked the OLD default species, so the clause below cannot "
		"discriminate a real choice from the deleted seed");
	// GUARDED, not trusted: the assert macros CONTINUE past a failure, and
	// ZM_Party::Get Zenith_Asserts out of range -- which calls Zenith_DebugBreak() in
	// EVERY configuration and would end the whole boot-unit run rather than fail this
	// one clause.
	if (xState.m_xParty.Count() > 0u)
	{
		ZENITH_ASSERT_EQ((u_int)xState.m_xParty.Get(0u).m_eSpecies, (u_int)eExpected,
			"the granted lead is species %u, not the chosen %u",
			(u_int)xState.m_xParty.Get(0u).m_eSpecies, (u_int)eExpected);
	}
	ZENITH_ASSERT_FALSE(PartyHoldsSpecies(xState.m_xParty, ZM_SPECIES_FERNFAWN),
		"the party holds a Fernfawn the player never chose");
	ZENITH_ASSERT_FALSE(xState.m_xCaught.IsSet(ZM_SPECIES_FERNFAWN),
		"the dex records a caught Fernfawn the player never owned");
	ZENITH_ASSERT_TRUE(xState.m_xCaught.IsSet(eExpected),
		"the dex does not record the species that was actually granted");

	// ...and the ONE statement this wrapper adds over the shipped grant.
	ZENITH_ASSERT_TRUE(
		ZM_IsStoryFlagSet(xState, ZM_STORY_FLAG_STARTER_RECEIVED),
		"the grant did not set ZM_STORY_FLAG_STARTER_RECEIVED, so Aster keeps "
		"offering, keeps his pre-starter lines, and the beat can be replayed");
	// It must set THAT flag and no other: the arrival flags belong to the warp tail.
	ZENITH_ASSERT_EQ(xState.m_xStoryFlags.Count(), 1u,
		"the grant set %u story flags; it owns exactly one",
		xState.m_xStoryFlags.Count());
}

ZENITH_TEST(ZM_Intro, Intro_StarterGrantRefusalLeavesTheStateAndFlagUntouched)
{
	ZM_GameState xState = ZM_MakeNewGameState();

	// An unregistered choice: refused by the shipped grant, and the flag must not be
	// set by the wrapper on the way past.
	ZENITH_ASSERT_FALSE(
		ZM_UI_MenuStack::ApplyStarterGrant(xState, ZM_STARTER_CHOICE_NONE),
		"the grant accepted the 'no choice' sentinel");
	ZENITH_ASSERT_FALSE(
		ZM_UI_MenuStack::ApplyStarterGrant(
			xState, (ZM_STARTER_CHOICE)((u_int)ZM_STARTER_CHOICE_COUNT + 5u)),
		"the grant accepted an out-of-range choice");
	ZENITH_ASSERT_EQ(xState.m_xParty.Count(), 0u,
		"a refused grant appended %u party member(s)", xState.m_xParty.Count());
	ZENITH_ASSERT_FALSE(ZM_IsStoryFlagSet(xState, ZM_STORY_FLAG_STARTER_RECEIVED),
		"a REFUSED grant set the starter flag -- the player would be locked out of "
		"the beat having received nothing");
	ZENITH_ASSERT_EQ(xState.m_xStoryFlags.Count(), 0u,
		"a refused grant wrote %u story-flag bit(s)", xState.m_xStoryFlags.Count());
}

// The OTHER refusal arm the shipped grant owns: a full party. It is unreachable on
// the intro path (the party is empty there), but the wrapper must not set the flag
// on it either -- a state that gained the flag without a monster would leave Aster
// silent about a starter the player never got.
ZENITH_TEST(ZM_Intro, Intro_StarterGrantOnAFullPartyLeavesTheFlagClear)
{
	ZM_GameState xState = ZM_MakeNewGameState();

	const ZM_Monster xFiller =
		ZM_BuildMonsterRecord(ZM_SPECIES_FERNFAWN, uZM_STARTER_LEVEL);
	while (!xState.m_xParty.IsFull())
	{
		if (!xState.m_xParty.Add(xFiller))
		{
			break;
		}
	}
	ZENITH_ASSERT_TRUE(xState.m_xParty.IsFull(),
		"the fixture could not fill the party, so the refusal below is not the "
		"full-party arm");

	const u_int uCountBefore = xState.m_xParty.Count();
	ZENITH_ASSERT_FALSE(
		ZM_UI_MenuStack::ApplyStarterGrant(xState, eNON_DEFAULT_CHOICE),
		"the grant accepted a choice into a FULL party");
	ZENITH_ASSERT_EQ(xState.m_xParty.Count(), uCountBefore,
		"a full-party refusal still changed the party count");
	ZENITH_ASSERT_FALSE(ZM_IsStoryFlagSet(xState, ZM_STORY_FLAG_STARTER_RECEIVED),
		"a full-party refusal still set the starter flag");
}

// ---- The arrival flags (ZM_IntroStoryFlagForArrival) ------------------------

ZENITH_TEST(ZM_Intro, Intro_ArrivalNamesLeftHomeOnlyOnTheHomeConnectionTag)
{
	const u_int uDawnmere = ZM_GetWorldSpec(ZM_SCENE_DAWNMERE).m_uBuildIndex;
	const char* const szHomeTag = HomeToDawnmereTag();

	ZENITH_ASSERT_TRUE(szHomeTag[0] != '\0',
		"the compiled ZM_SCENE_PLAYERHOME row carries no connection targeting "
		"Dawnmere, so the front door has no tag and the flag can never be set");

	ZENITH_ASSERT_EQ(
		(u_int)ZM_IntroStoryFlagForArrival(uDawnmere, szHomeTag),
		(u_int)ZM_STORY_FLAG_INTRO_LEFT_HOME,
		"arriving in Dawnmere on the front-door tag '%s' raises no intro flag",
		szHomeTag);

	// Dawnmere is reachable four ways and only ONE of them is leaving home. The
	// other offered tags must raise nothing -- a whiteout, a route return or the lab
	// door is not the intro.
	const ZM_WorldSpec& xDawnmere = ZM_GetWorldSpec(ZM_SCENE_DAWNMERE);
	ZENITH_ASSERT_GT(xDawnmere.m_uSpawnTagCount, 1u,
		"Dawnmere offers only one spawn tag, so the discrimination below is vacuous");
	u_int uOtherTagsChecked = 0u;
	for (u_int u = 0u; u < xDawnmere.m_uSpawnTagCount; ++u)
	{
		const char* const szTag = xDawnmere.m_pszSpawnTags[u];
		if (szTag == nullptr || std::strcmp(szTag, szHomeTag) == 0)
		{
			continue;
		}
		++uOtherTagsChecked;
		ZENITH_ASSERT_EQ((u_int)ZM_IntroStoryFlagForArrival(uDawnmere, szTag),
			(u_int)ZM_STORY_FLAG_NONE,
			"arriving in Dawnmere on '%s' claims the player just left home", szTag);
	}
	ZENITH_ASSERT_GT(uOtherTagsChecked, 0u,
		"no non-home Dawnmere tag was checked, so the discrimination is vacuous");
}

ZENITH_TEST(ZM_Intro, Intro_ArrivalNamesMetProfessorForTheLabAndNothingElse)
{
	const u_int uProfLab = ZM_GetWorldSpec(ZM_SCENE_PROFLAB).m_uBuildIndex;

	// The lab has exactly one door, so the tag is not part of the condition -- which
	// is exactly why BOTH of these must answer the same way.
	ZENITH_ASSERT_EQ((u_int)ZM_IntroStoryFlagForArrival(uProfLab, "Door"),
		(u_int)ZM_STORY_FLAG_MET_PROFESSOR,
		"arriving in the lab does not record having met the professor");
	ZENITH_ASSERT_EQ((u_int)ZM_IntroStoryFlagForArrival(uProfLab, "AnyOtherTag"),
		(u_int)ZM_STORY_FLAG_MET_PROFESSOR,
		"the lab arrival is tag-sensitive, but the lab has only one door");

	// Every OTHER registered scene must raise nothing. PlayerHome in particular:
	// arriving in the bedroom a new game starts in is not a story beat.
	u_int uSilentScenes = 0u;
	for (u_int u = 0u; u < ZM_GetSceneCount(); ++u)
	{
		const ZM_WorldSpec& xRow = ZM_GetWorldSpec((ZM_SCENE_ID)u);
		if (xRow.m_uBuildIndex == uProfLab
			|| xRow.m_uBuildIndex == ZM_GetWorldSpec(ZM_SCENE_DAWNMERE).m_uBuildIndex)
		{
			continue;   // the two destinations that DO raise something
		}
		++uSilentScenes;
		ZENITH_ASSERT_EQ((u_int)ZM_IntroStoryFlagForArrival(xRow.m_uBuildIndex, "Door"),
			(u_int)ZM_STORY_FLAG_NONE,
			"arriving in '%s' raises an intro story flag", xRow.m_szName);
	}
	ZENITH_ASSERT_GT(uSilentScenes, 0u,
		"no non-intro scene was checked, so the clause above is vacuous");
}

ZENITH_TEST(ZM_Intro, Intro_ArrivalResolverIsTotalOverGarbage)
{
	const u_int uDawnmere = ZM_GetWorldSpec(ZM_SCENE_DAWNMERE).m_uBuildIndex;

	ZENITH_ASSERT_EQ((u_int)ZM_IntroStoryFlagForArrival(uDawnmere, nullptr),
		(u_int)ZM_STORY_FLAG_NONE, "a null spawn tag raised a flag");
	ZENITH_ASSERT_EQ((u_int)ZM_IntroStoryFlagForArrival(uDawnmere, ""),
		(u_int)ZM_STORY_FLAG_NONE, "an empty spawn tag raised a flag");
	ZENITH_ASSERT_EQ((u_int)ZM_IntroStoryFlagForArrival(0xFFFFFFFFu, "FromHome"),
		(u_int)ZM_STORY_FLAG_NONE, "an unknown build index raised a flag");
	ZENITH_ASSERT_EQ((u_int)ZM_IntroStoryFlagForArrival(0xFFFFFFFFu, nullptr),
		(u_int)ZM_STORY_FLAG_NONE, "a wholly garbage arrival raised a flag");
}

// ---- The save-shape consequence ---------------------------------------------

// ★ THE ONE SAVE-VISIBLE EFFECT OF THIS WHOLE SLICE, BOOKED. Save module 4 writes
// (highest set index + 1) followed by ceil(count / 8) bytes, so a run that has
// completed the intro takes module 4 from a zero-flag payload to a THREE-flag one
// -- one extra byte in every save written thereafter. That is the DOCUMENTED
// no-migration growth path (Docs/SaveFormat.md), so no schema version bump is
// owed; this unit exists so the growth stays ONE byte rather than becoming five
// hundred the first time somebody allocates a sparse flag index.
ZENITH_TEST(ZM_Intro, Intro_TheThreeFlagsCostModuleFourExactlyOneByte)
{
	ZENITH_ASSERT_EQ((u_int)ZM_STORY_FLAG_INTRO_LEFT_HOME, uINTRO_LEFT_HOME_INDEX,
		"INTRO_LEFT_HOME moved off wire bit %u", uINTRO_LEFT_HOME_INDEX);
	ZENITH_ASSERT_EQ((u_int)ZM_STORY_FLAG_MET_PROFESSOR, uINTRO_MET_PROF_INDEX,
		"MET_PROFESSOR moved off wire bit %u", uINTRO_MET_PROF_INDEX);
	ZENITH_ASSERT_EQ((u_int)ZM_STORY_FLAG_STARTER_RECEIVED, uINTRO_STARTER_INDEX,
		"STARTER_RECEIVED moved off wire bit %u", uINTRO_STARTER_INDEX);

	ZM_GameState xState = ZM_MakeNewGameState();
	ZENITH_ASSERT_TRUE(
		ZM_SetStoryFlag(xState, ZM_STORY_FLAG_INTRO_LEFT_HOME, true),
		"INTRO_LEFT_HOME was refused by the typed setter");
	ZENITH_ASSERT_TRUE(
		ZM_SetStoryFlag(xState, ZM_STORY_FLAG_MET_PROFESSOR, true),
		"MET_PROFESSOR was refused by the typed setter");
	ZENITH_ASSERT_TRUE(
		ZM_SetStoryFlag(xState, ZM_STORY_FLAG_STARTER_RECEIVED, true),
		"STARTER_RECEIVED was refused by the typed setter");

	ZENITH_ASSERT_EQ(xState.m_xStoryFlags.Count(), 3u,
		"a completed intro leaves %u flag bit(s) set, not three",
		xState.m_xStoryFlags.Count());

	// (highest set index + 1) == 3 for indices {0,1,2}, and ceil(3/8) == 1.
	u_int uHighestSet = 0u;
	bool bAnySet = false;
	for (u_int u = 0u; u < (u_int)ZM_STORY_FLAG_COUNT; ++u)
	{
		if (xState.m_xStoryFlags.IsSet(u))
		{
			uHighestSet = u;
			bAnySet = true;
		}
	}
	ZENITH_ASSERT_TRUE(bAnySet, "the completed-intro fixture set no flag at all");
	const u_int uPayloadBytes = bAnySet ? ((uHighestSet + 1u + 7u) / 8u) : 0u;
	ZENITH_ASSERT_EQ(uPayloadBytes, 1u,
		"a completed intro costs module 4 %u payload byte(s) instead of one -- the "
		"highest set index is %u, and a sparse allocation here is charged to EVERY "
		"save this game ever writes",
		uPayloadBytes, uHighestSet);

	// Setting an already-set flag must be free, because the arrival tail re-runs on
	// every single walk through the front door and the lab doorway.
	ZENITH_ASSERT_TRUE(
		ZM_SetStoryFlag(xState, ZM_STORY_FLAG_MET_PROFESSOR, true),
		"re-setting an already-set flag was refused");
	ZENITH_ASSERT_EQ(xState.m_xStoryFlags.Count(), 3u,
		"re-setting an already-set flag changed the bit count to %u",
		xState.m_xStoryFlags.Count());
}
