#include "Zenith.h"

// ============================================================================
// ZM_Tests_StarterChoice -- S8 item 1 SC3 units for the authored starter trio and
// the three operations over it: seed a NEW GAME (ZM_MakeNewGameState), grant the
// CHOSEN starter (ZM_ApplyStarterChoice), and ask whether the player owns anything
// to send out at all (ZM_CanEnterBattle).
//
// Everything here is PURE and headless: compiled const tables, free functions over
// a by-value ZM_GameState, and the frozen type chart. No ECS, no scene, no UI, no
// disk, no baked asset -- so every fixture is hermetic and no RequestSkip is needed.
//
// Category ZM_Starter, deliberately distinct from ZM_Party / ZM_Save / ZM_Data:
// these are the units for the surface SC3 introduces, and a shared category would
// bury a starter regression among the ~forty party-model units.
//
// ★ WHAT THIS FILE MUST NOT DO. Three of the functions it reaches
// Zenith_Assert on out-of-range input, and Zenith_Assert calls Zenith_DebugBreak()
// in EVERY configuration -- there is no build in which it degrades to a log. So a
// single bad value fed to ZM_GetSpeciesData, ZM_TypeChart::GetEffectiveness or
// ZM_BuildMonsterRecord does not fail one unit: it ENDS the whole ~2800-unit boot
// run and takes the gate down. Every walk below therefore GUARDS its own indices
// before it dereferences a table, and bails on a red rather than pressing on.
//
// The starter surface itself is TOTAL by contract and never asserts, which is what
// Accessor_OutOfRangeFailsClosedAndNeverAsserts and
// Apply_RejectsAnOutOfRangeChoiceWithoutMutatingTheState exist to prove -- with a
// Zenith_AssertCaptureScope, so "it never asserts" is an assertion rather than
// "the process happened not to die this time".
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "UnitTests/Zenith_AssertCapture.h"               // the totality proofs
#include "Zenithmon/Source/Data/ZM_ItemData.h"            // ZM_ITEM_CATCHORB / ZM_ITEM_SALVE
#include "Zenithmon/Source/Data/ZM_Learnsets.h"           // ZM_GetSpeciesLearnset
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"         // ZM_GetSpeciesData / ZM_RARITY_RARE
#include "Zenithmon/Source/Data/ZM_TrainerData.h"         // the shipped Vesper row
#include "Zenithmon/Source/Data/ZM_TypeChart.h"           // GetEffectiveness (the counter proof)
#include "Zenithmon/Source/Data/ZM_Types.h"               // ZM_TYPE / ZM_TYPE_COUNT / ZM_TYPE_NONE
#include "Zenithmon/Source/Party/ZM_GameState.h"          // ZM_MakeNewGameState + the durable model
#include "Zenithmon/Source/Party/ZM_Monster.h"            // ZM_Monster / uZM_MAX_MOVES
#include "Zenithmon/Source/Party/ZM_StarterChoice.h"

namespace
{
	// The starting economy, as LITERALS. Never read off ZM_MakeNewGameState and
	// compared to itself: an expectation derived from the code under test moves
	// with it and can never fail. These three numbers are the ruled S6 placeholder
	// seed, and a change to any of them is a save-visible tuning decision.
	constexpr u_int uEXPECTED_STARTER_MONEY = 3000u;
	constexpr u_int uEXPECTED_STARTER_ORBS  = 5u;
	constexpr u_int uEXPECTED_STARTER_HEALS = 3u;

	// The authored trio's size, as a LITERAL, for the same reason. Asserted AGAINST
	// ZM_GetStarterChoiceCount(), never used in place of it.
	constexpr u_int uEXPECTED_STARTER_COUNT = 3u;

	// Does a record's moveset contain a given move id? The identical helper in
	// ZM_Tests_Party.cpp has internal linkage there and is not reachable across TUs,
	// so this is a deliberate copy rather than a missing include.
	bool RecordHasMove(const ZM_Monster& xRec, ZM_MOVE_ID eMove)
	{
		for (u_int i = 0u; i < uZM_MAX_MOVES; ++i)
		{
			if (xRec.m_axMoves[i].m_eMove == eMove) { return true; }
		}
		return false;
	}

	// A row's PRIMARY type, or ZM_TYPE_NONE when the row's species is not a real dex
	// id. Guarded, because ZM_GetSpeciesData Zenith_Asserts out of range -- a walk
	// that indexed it straight from a table column would turn a mis-authored row
	// into a dead boot run instead of a red unit.
	ZM_TYPE PrimaryTypeOfStarter(ZM_STARTER_CHOICE eChoice)
	{
		const ZM_SPECIES_ID eSpecies = ZM_ResolvePlayerStarterSpecies(eChoice);
		if ((u_int)eSpecies >= (u_int)ZM_SPECIES_COUNT) { return ZM_TYPE_NONE; }
		return ZM_GetSpeciesData(eSpecies).m_aeTypes[0];
	}

	// True only for a type the 18x18 chart can be indexed with. ZM_TYPE_NONE aliases
	// ZM_TYPE_COUNT, so this one comparison rejects the empty second-type slot and
	// every garbage value together -- and it is the ONLY thing standing between the
	// counter walk and GetEffectiveness's assert.
	bool IsChartIndexableType(ZM_TYPE eType)
	{
		return (u_int)eType < (u_int)ZM_TYPE_COUNT;
	}
}

// ############################################################################
// A. The authored table
// ############################################################################

// The rows are dense, self-identifying, and point at real dex species. Everything
// below walks this table by index, so an id/index mismatch here would silently
// re-point every other unit in the file.
ZENITH_TEST(ZM_Starter, Table_RowsAreDenseAndSelfConsistent)
{
	const u_int uCount = ZM_GetStarterChoiceCount();
	ZENITH_ASSERT_EQ(uCount, (u_int)ZM_STARTER_CHOICE_COUNT,
		"the accessor and the enum disagree about how many starters exist");
	ZENITH_ASSERT_EQ(uCount, uEXPECTED_STARTER_COUNT,
		"the authored trio gained or lost a member -- that is a content decision "
		"(Docs/GameDesignDocument.md section 4), so review this file's literals and "
		"the rival's counter row together rather than just bumping this number");

	for (u_int u = 0u; u < uCount; ++u)
	{
		const ZM_STARTER_CHOICE eChoice = (ZM_STARTER_CHOICE)u;
		ZENITH_ASSERT_TRUE(ZM_IsRegisteredStarterChoice(eChoice),
			"choice %u is inside the count but reads as unregistered", u);

		const ZM_StarterChoiceData& xRow = ZM_GetStarterChoice(eChoice);
		ZENITH_ASSERT_EQ((u_int)xRow.m_eChoice, u,
			"row %u carries choice id %u -- the row INDEX must equal the id, or every "
			"lookup in this file reads a neighbour's row", u, (u_int)xRow.m_eChoice);
		ZENITH_ASSERT_LT((u_int)xRow.m_eSpecies, (u_int)ZM_SPECIES_COUNT,
			"row %u names species %u, which is not a real dex id -- ZM_BuildMonsterRecord "
			"would Zenith_Assert on it and end the boot run", u, (u_int)xRow.m_eSpecies);
	}
}

// Every starter is a stage-1, SINGLE-TYPED, RARE species.
//
// The single-type clause is LOAD-BEARING, not decoration: it is what makes
// Counter_ColumnAgreesWithTheTypeChart safe to feed ZM_TypeChart::GetEffectiveness
// with a primary type. A starter that gained a second type at stage 1 would not
// break that unit's guard, but it WOULD mean the counter relationship the design
// promises is no longer decided by one lookup.
ZENITH_TEST(ZM_Starter, Table_EveryStarterIsAStageOneSingleTypedRareSpecies)
{
	const u_int uCount = ZM_GetStarterChoiceCount();
	ZENITH_ASSERT_GT(uCount, 0u, "an empty table makes this walk vacuous");

	for (u_int u = 0u; u < uCount; ++u)
	{
		const ZM_StarterChoiceData& xRow = ZM_GetStarterChoice((ZM_STARTER_CHOICE)u);
		if ((u_int)xRow.m_eSpecies >= (u_int)ZM_SPECIES_COUNT)
		{
			ZENITH_FAIL("a starter row names a species outside the dex; "
				"Table_RowsAreDenseAndSelfConsistent carries the detail");
			return;
		}

		const ZM_SpeciesData& xSpecies = ZM_GetSpeciesData(xRow.m_eSpecies);
		ZENITH_ASSERT_EQ(xSpecies.m_uEvoStage, 1u,
			"starter '%s' is evolution stage %u -- a gift starter is the FIRST stage of "
			"its family", xSpecies.m_szName, xSpecies.m_uEvoStage);
		ZENITH_ASSERT_EQ((u_int)xSpecies.m_eRarity, (u_int)ZM_RARITY_RARE,
			"starter '%s' is not RARE -- the trio is gift-only tier "
			"(Docs/GameDesignDocument.md section 4)", xSpecies.m_szName);
		ZENITH_ASSERT_EQ((u_int)xSpecies.m_aeTypes[1], (u_int)ZM_TYPE_NONE,
			"starter '%s' carries a SECOND type at stage 1 -- the counter triangle is "
			"defined over primary types alone, so the counter column below would no "
			"longer describe the whole matchup", xSpecies.m_szName);
	}
}

// No two choices hand out the same monster, and no row counters itself. Catches the
// copy-paste that gives two starters one species, and the fixed point that would
// make Counter_FormsOneCycleOverEveryChoice's walk stall on its first step.
ZENITH_TEST(ZM_Starter, Table_SpeciesAndChoicesAreDistinct)
{
	const u_int uCount = ZM_GetStarterChoiceCount();
	ZENITH_ASSERT_GT(uCount, 1u,
		"with fewer than two rows the distinctness walk below is vacuous");

	for (u_int u = 0u; u < uCount; ++u)
	{
		const ZM_StarterChoiceData& xRow = ZM_GetStarterChoice((ZM_STARTER_CHOICE)u);
		ZENITH_ASSERT_NE((u_int)xRow.m_eCounteredBy, (u_int)xRow.m_eChoice,
			"choice %u is countered by ITSELF -- the rival would mirror the player's "
			"pick instead of answering it", u);

		for (u_int v = u + 1u; v < uCount; ++v)
		{
			const ZM_StarterChoiceData& xOther = ZM_GetStarterChoice((ZM_STARTER_CHOICE)v);
			ZENITH_ASSERT_NE((u_int)xRow.m_eSpecies, (u_int)xOther.m_eSpecies,
				"choices %u and %u hand out the SAME species -- one of the three picks "
				"is not a real choice", u, v);
		}
	}
}

// ############################################################################
// B. Accessor totality
// ############################################################################

// The whole accessor surface answers the sentinel and pure garbage without ever
// reaching Zenith_Assert.
//
// ★ THE Zenith_Error LINES THIS UNIT EMITS ARE EXPECTED OUTPUT, NOT A FAILURE.
// ZM_GetStarterChoice says out loud that a lookup for an unnamed choice is
// mis-authored data -- non-fatally, by contract. What would NOT be survivable is an
// assert: it breaks in every configuration, so an asserting accessor would end the
// boot unit run right here rather than red this one unit.
ZENITH_TEST(ZM_Starter, Accessor_OutOfRangeFailsClosedAndNeverAsserts)
{
	const ZM_STARTER_CHOICE aeBad[2] =
	{
		ZM_STARTER_CHOICE_NONE,
		(ZM_STARTER_CHOICE)99u
	};

	// NO ZENITH_ASSERT_* inside the capture scope -- it swallows framework failures
	// too -- and the hit count is copied out BEFORE the closing brace, because the
	// destructor restores the previous count.
	// EVERY landing slot is pre-seeded with the value this unit expects NOT to see,
	// so a capture scope that swallowed the whole loop -- or a loop that never ran --
	// fails loudly instead of passing on its own initialisers.
	bool              abRegistered[2]     = { true, true };
	ZM_SPECIES_ID     aePlayerSpecies[2]  = { ZM_SPECIES_FERNFAWN, ZM_SPECIES_FERNFAWN };
	ZM_SPECIES_ID     aeCounterSpecies[2] = { ZM_SPECIES_FERNFAWN, ZM_SPECIES_FERNFAWN };
	ZM_STARTER_CHOICE aeRowChoice[2]      = { ZM_STARTER_CHOICE_FERNFAWN, ZM_STARTER_CHOICE_FERNFAWN };
	ZM_SPECIES_ID     aeRowSpecies[2]     = { ZM_SPECIES_FERNFAWN, ZM_SPECIES_FERNFAWN };
	u_int uHits = 0u;
	{
		Zenith_AssertCaptureScope xCapture;
		for (u_int u = 0u; u < 2u; ++u)
		{
			abRegistered[u]     = ZM_IsRegisteredStarterChoice(aeBad[u]);
			const ZM_StarterChoiceData& xRow = ZM_GetStarterChoice(aeBad[u]);
			aeRowChoice[u]      = xRow.m_eChoice;
			aeRowSpecies[u]     = xRow.m_eSpecies;
			aePlayerSpecies[u]  = ZM_ResolvePlayerStarterSpecies(aeBad[u]);
			aeCounterSpecies[u] = ZM_ResolveCounterStarterSpecies(aeBad[u]);
		}
		uHits = (u_int)xCapture.GetHitCount();
	}

	ZENITH_ASSERT_EQ(uHits, 0u,
		"the starter accessors asserted on an unregistered choice -- Zenith_Assert "
		"breaks in EVERY config, so this would kill the whole boot unit run rather "
		"than fail one test");

	for (u_int u = 0u; u < 2u; ++u)
	{
		ZENITH_ASSERT_FALSE(abRegistered[u],
			"choice %u reads as a REGISTERED starter", (u_int)aeBad[u]);
		ZENITH_ASSERT_EQ((u_int)aeRowChoice[u], (u_int)ZM_STARTER_CHOICE_NONE,
			"choice %u got a row that names a real choice instead of the INVALID row",
			(u_int)aeBad[u]);
		ZENITH_ASSERT_EQ((u_int)aeRowSpecies[u], (u_int)ZM_SPECIES_NONE,
			"choice %u got a row carrying a real species -- a caller that skipped the "
			"id check could build a monster out of it", (u_int)aeBad[u]);
		ZENITH_ASSERT_EQ((u_int)aePlayerSpecies[u], (u_int)ZM_SPECIES_NONE,
			"choice %u resolved to a real PLAYER species", (u_int)aeBad[u]);
		ZENITH_ASSERT_EQ((u_int)aeCounterSpecies[u], (u_int)ZM_SPECIES_NONE,
			"choice %u resolved to a real COUNTER species", (u_int)aeBad[u]);
	}
}

// ############################################################################
// C. The counter relationship -- checked against the CHART, not restated
// ############################################################################

// The authored m_eCounteredBy column is what the rival's team is built from, so it
// has to be true of the shipped type chart rather than merely self-consistent.
//
// ★ THE EXPECTATION IS COMPUTED, NEVER RE-SPELLED. Two independent claims:
//
//   (1) DERIVED: among the OTHER starters, EXACTLY ONE is super-effective into this
//       row's type, and it is the one m_eCounteredBy names. This is the claim that
//       can red a mis-authored column, because nothing in it reads the column to
//       decide what the answer should be.
//   (2) BOTH DIRECTIONS: the counter is 2x into the starter AND the starter is
//       resisted (0.5x) coming back. A one-directional check would pass a chart in
//       which the "counter" is also weak to its target -- a mutual-2x pairing is
//       not a counter, it is a coin flip.
//
// A unit that restated "Fernfawn is beaten by Kindlet" would agree with the column
// by construction and could never fail, whatever the chart said.
ZENITH_TEST(ZM_Starter, Counter_ColumnAgreesWithTheTypeChart)
{
	const u_int uCount = ZM_GetStarterChoiceCount();
	ZENITH_ASSERT_GT(uCount, 1u,
		"the derivation below needs at least one OTHER starter to choose between");

	for (u_int u = 0u; u < uCount; ++u)
	{
		const ZM_STARTER_CHOICE eChoice = (ZM_STARTER_CHOICE)u;
		const ZM_StarterChoiceData& xRow = ZM_GetStarterChoice(eChoice);

		// GUARD FIRST. ZM_TypeChart::GetEffectiveness Zenith_Asserts out of range and
		// ZM_TYPE_NONE aliases ZM_TYPE_COUNT, so an unresolvable type must stop this
		// walk dead rather than be handed to the chart.
		const ZM_TYPE eOwn = PrimaryTypeOfStarter(eChoice);
		if (!IsChartIndexableType(eOwn))
		{
			ZENITH_FAIL("a starter row has no chart-indexable primary type; "
				"Table_RowsAreDenseAndSelfConsistent carries the detail");
			return;
		}

		// ---- (1) DERIVE the counter from the chart alone -------------------------
		u_int uSuperEffectiveCount = 0u;
		ZM_STARTER_CHOICE eDerivedCounter = ZM_STARTER_CHOICE_NONE;
		for (u_int v = 0u; v < uCount; ++v)
		{
			if (v == u) { continue; }
			const ZM_TYPE eOther = PrimaryTypeOfStarter((ZM_STARTER_CHOICE)v);
			if (!IsChartIndexableType(eOther))
			{
				ZENITH_FAIL("a starter row has no chart-indexable primary type; "
					"Table_RowsAreDenseAndSelfConsistent carries the detail");
				return;
			}
			if (ZM_TypeChart::GetEffectiveness(eOther, eOwn) > 1.5f)
			{
				++uSuperEffectiveCount;
				eDerivedCounter = (ZM_STARTER_CHOICE)v;
			}
		}

		ZENITH_ASSERT_EQ(uSuperEffectiveCount, 1u,
			"exactly one OTHER starter must beat choice %u, but %u do -- with none the "
			"rival has no honest answer to that pick, and with two the 'counter' is a "
			"coin flip between them", u, uSuperEffectiveCount);
		if (uSuperEffectiveCount != 1u) { return; }
		ZENITH_ASSERT_EQ((u_int)xRow.m_eCounteredBy, (u_int)eDerivedCounter,
			"choice %u's authored counter is %u, but the SHIPPED TYPE CHART says the "
			"starter that beats it is %u -- either the column or the chart moved, and "
			"the rival would bring the wrong monster", u,
			(u_int)xRow.m_eCounteredBy, (u_int)eDerivedCounter);

		// ---- (2) BOTH DIRECTIONS on the authored pairing -------------------------
		const ZM_TYPE eCounter = PrimaryTypeOfStarter(xRow.m_eCounteredBy);
		if (!IsChartIndexableType(eCounter))
		{
			ZENITH_FAIL("a starter's authored counter has no chart-indexable primary "
				"type; Table_RowsAreDenseAndSelfConsistent carries the detail");
			return;
		}
		ZENITH_ASSERT_EQ_FLOAT(ZM_TypeChart::GetEffectiveness(eCounter, eOwn), 2.0f, 0.001f,
			"the counter of choice %u is not SUPER-EFFECTIVE into it", u);
		ZENITH_ASSERT_EQ_FLOAT(ZM_TypeChart::GetEffectiveness(eOwn, eCounter), 0.5f, 0.001f,
			"choice %u is not RESISTED by its counter coming back -- a mutual-2x pairing "
			"is a race, not the disadvantage the design promises the player", u);
	}
}

// Following m_eCounteredBy visits every choice exactly once and returns to the
// start. Rejects a 2-cycle between two rows that leaves the third unreachable --
// a shape every row of Counter_ColumnAgreesWithTheTypeChart would still pass.
ZENITH_TEST(ZM_Starter, Counter_FormsOneCycleOverEveryChoice)
{
	const u_int uCount = ZM_GetStarterChoiceCount();
	ZENITH_ASSERT_GT(uCount, 0u, "an empty table makes this walk vacuous");
	if (uCount == 0u || uCount > (u_int)ZM_STARTER_CHOICE_COUNT) { return; }

	bool abVisited[ZM_STARTER_CHOICE_COUNT] = {};
	ZM_STARTER_CHOICE eAt = ZM_STARTER_CHOICE_FERNFAWN;
	for (u_int uStep = 0u; uStep < uCount; ++uStep)
	{
		if (!ZM_IsRegisteredStarterChoice(eAt))
		{
			ZENITH_FAIL("the counter walk left the table -- a row's m_eCounteredBy "
				"names an unregistered choice");
			return;
		}
		ZENITH_ASSERT_FALSE(abVisited[(u_int)eAt],
			"the counter walk revisited choice %u after %u step(s) -- the column forms "
			"a SHORT cycle, so at least one starter is never anybody's counter and the "
			"rival can never be built against it", (u_int)eAt, uStep);
		if (abVisited[(u_int)eAt]) { return; }
		abVisited[(u_int)eAt] = true;
		eAt = ZM_GetStarterChoice(eAt).m_eCounteredBy;
	}

	ZENITH_ASSERT_EQ((u_int)eAt, (u_int)ZM_STARTER_CHOICE_FERNFAWN,
		"%u steps of the counter column did not return to the starting choice", uCount);
	for (u_int u = 0u; u < uCount; ++u)
	{
		ZENITH_ASSERT_TRUE(abVisited[u],
			"choice %u is not on the counter cycle at all", u);
	}
}

// ############################################################################
// D. ZM_MakeNewGameState -- the PARTYLESS half
// ############################################################################

// A new game owns the economy and NOTHING ELSE. This is the whole contract of the
// function the split created, and the one place the "no party, no dex" claim is
// asserted directly rather than inferred from a composed fixture.
ZENITH_TEST(ZM_Starter, NewGame_IsPartylessAndDexEmptyButKeepsTheEconomy)
{
	const ZM_GameState xState = ZM_MakeNewGameState();

	ZENITH_ASSERT_EQ(xState.m_xParty.Count(), 0u,
		"a NEW GAME grants no monster -- the starter is ZM_ApplyStarterChoice's job");
	ZENITH_ASSERT_TRUE(xState.m_xParty.IsEmpty(),
		"Count() and IsEmpty() disagree about the fresh party");
	ZENITH_ASSERT_EQ(xState.GetSeenCount(), 0u, "a new game has seen nothing");
	ZENITH_ASSERT_EQ(xState.GetCaughtCount(), 0u, "a new game has caught nothing");

	ZENITH_ASSERT_EQ(xState.m_uMoney, uEXPECTED_STARTER_MONEY,
		"the starting purse moved -- that is a save-visible tuning decision");
	ZENITH_ASSERT_EQ(xState.m_xBag.GetCount(ZM_ITEM_CATCHORB), uEXPECTED_STARTER_ORBS,
		"the seeded Catch Orb count moved");
	ZENITH_ASSERT_EQ(xState.m_xBag.GetCount(ZM_ITEM_SALVE), uEXPECTED_STARTER_HEALS,
		"the seeded Salve count moved");
	ZENITH_ASSERT_EQ(xState.m_xBag.TotalStackCount(), 2u,
		"those are the ONLY two starting stacks");

	ZENITH_ASSERT_EQ(xState.m_xStoryFlags.Count(), 0u,
		"a new game ships with story-flag bits set -- module 4 sizes from the highest "
		"SET bit, so a stray flag costs every save this game ever writes");
	ZENITH_ASSERT_EQ(xState.GetBadgeCount(), 0u, "a new game has no badges");
	ZENITH_ASSERT_EQ(xState.m_xBoxes.Count(), 0u, "a new game's boxes are empty");
	ZENITH_ASSERT_EQ(xState.m_xDaycare.m_uParentCount, 0u, "a new game's daycare is empty");
	ZENITH_ASSERT_EQ(xState.m_xTowerRun.m_uCurrentStreak, 0u, "a new game has no tower run");
	ZENITH_ASSERT_EQ(xState.m_xWorldPosition.m_uSceneBuildIndex, uZM_WORLD_SCENE_UNSET,
		"a new game's resume point must be UNSET, or Continue would look loadable");
	ZENITH_ASSERT_FALSE(xState.m_bPendingWhiteout,
		"a new game must not start with the transient whiteout latch armed");
}

// ############################################################################
// E. ZM_ApplyStarterChoice -- the GRANT
// ############################################################################

// Every registered choice grants exactly one full-health level-5 lead of the right
// species, carrying its level-1 move.
//
// Written as a WALK, not three hand-listed cases, so a fourth starter is covered
// the day it is appended rather than the day somebody remembers this file. It is
// also the first coverage KINDLET and FINLET have ever had as party records --
// nothing in this game had built either one before SC3.
ZENITH_TEST(ZM_Starter, Apply_GrantsExactlyOneLevelFiveLeadForEveryChoice)
{
	const u_int uCount = ZM_GetStarterChoiceCount();
	ZENITH_ASSERT_GT(uCount, 0u, "an empty table makes this walk vacuous");

	for (u_int u = 0u; u < uCount; ++u)
	{
		const ZM_STARTER_CHOICE eChoice = (ZM_STARTER_CHOICE)u;
		const ZM_StarterChoiceData& xRow = ZM_GetStarterChoice(eChoice);
		if ((u_int)xRow.m_eSpecies >= (u_int)ZM_SPECIES_COUNT)
		{
			ZENITH_FAIL("a starter row names a species outside the dex; "
				"Table_RowsAreDenseAndSelfConsistent carries the detail");
			return;
		}

		// A FRESH state per iteration. Reusing one would let the second grant land on
		// a party of 1 and the "exactly one" assertion would be measuring the loop.
		ZM_GameState xState = ZM_MakeNewGameState();
		const bool bGranted = ZM_ApplyStarterChoice(xState, eChoice);

		ZENITH_ASSERT_TRUE(bGranted, "granting choice %u reported failure", u);
		ZENITH_ASSERT_EQ(xState.m_xParty.Count(), 1u,
			"choice %u did not leave exactly one party member", u);
		if (xState.m_xParty.Count() != 1u) { return; }

		const ZM_Monster& xLead = xState.m_xParty.Get(0u);
		ZENITH_ASSERT_TRUE(xLead.IsValid(), "choice %u granted an invalid record", u);
		ZENITH_ASSERT_EQ((u_int)xLead.m_eSpecies, (u_int)xRow.m_eSpecies,
			"choice %u granted species %u, but its row names %u", u,
			(u_int)xLead.m_eSpecies, (u_int)xRow.m_eSpecies);
		ZENITH_ASSERT_EQ(xLead.m_uLevel, uZM_STARTER_LEVEL,
			"choice %u granted a level-%u starter", u, xLead.m_uLevel);
		ZENITH_ASSERT_FALSE(xLead.IsFainted(),
			"choice %u granted a FAINTED starter -- the player would white out on the "
			"first encounter", u);

		// The move matters as much as the species: a starter with an empty slot 0
		// cannot act, and ZM_BuildWildEnemySpec's opposite number would face a monster
		// with nothing to do. Read off the LEARNSET, never a hard-coded move id.
		const ZM_Learnset xLearnset = ZM_GetSpeciesLearnset(xRow.m_eSpecies);
		ZENITH_ASSERT_GT(xLearnset.m_uCount, 0u,
			"choice %u's species has an EMPTY learnset, so the move check below would "
			"be vacuous", u);
		if (xLearnset.m_uCount == 0u) { return; }
		ZENITH_ASSERT_TRUE(RecordHasMove(xLead, xLearnset.m_axMoves[0].m_eMove),
			"choice %u's starter is missing its level-1 move", u);
	}
}

// The grant marks the chosen species SEEN AND CAUGHT -- and marks nothing else.
//
// The seen-count clause is the load-bearing one: ZM_SaveSchema::ValidateState
// enforces caughtImpliesSeen, so a grant that reached for m_xCaught.Mark directly
// instead of MarkCaught would make EVERY save write fail -- and would do it
// silently here, because the party and the caught-set would both look right.
ZENITH_TEST(ZM_Starter, Apply_MarksOnlyTheChosenSpeciesSeenAndCaught)
{
	const u_int uCount = ZM_GetStarterChoiceCount();
	ZENITH_ASSERT_GT(uCount, 1u,
		"with a single row the 'and nothing else' clause below is vacuous");

	for (u_int u = 0u; u < uCount; ++u)
	{
		const ZM_STARTER_CHOICE eChoice = (ZM_STARTER_CHOICE)u;
		const ZM_SPECIES_ID eChosen = ZM_ResolvePlayerStarterSpecies(eChoice);

		ZM_GameState xState = ZM_MakeNewGameState();
		ZENITH_ASSERT_TRUE(ZM_ApplyStarterChoice(xState, eChoice),
			"granting choice %u reported failure", u);

		ZENITH_ASSERT_TRUE(xState.IsCaught(eChosen),
			"choice %u's species is not marked CAUGHT", u);
		ZENITH_ASSERT_TRUE(xState.IsSeen(eChosen),
			"choice %u's species is not marked SEEN -- ZM_SaveSchema's caughtImpliesSeen "
			"check would reject this state and every save write would fail", u);
		ZENITH_ASSERT_EQ(xState.GetCaughtCount(), 1u,
			"choice %u marked %u species caught, not one", u, xState.GetCaughtCount());
		ZENITH_ASSERT_EQ(xState.GetSeenCount(), 1u,
			"choice %u marked %u species seen, not one -- the dex must not gain the "
			"starters the player did NOT pick", u, xState.GetSeenCount());

		for (u_int v = 0u; v < uCount; ++v)
		{
			if (v == u) { continue; }
			const ZM_SPECIES_ID eOther = ZM_ResolvePlayerStarterSpecies((ZM_STARTER_CHOICE)v);
			ZENITH_ASSERT_FALSE(xState.IsCaught(eOther),
				"picking choice %u also registered choice %u's species as caught", u, v);
			ZENITH_ASSERT_FALSE(xState.IsSeen(eOther),
				"picking choice %u also registered choice %u's species as seen", u, v);
		}
	}
}

// An unregistered choice grants NOTHING and mutates NOTHING.
//
// ★ THIS IS THE UNIT THAT WOULD END THE BOOT RUN IF VALIDATION WERE MISSING.
// ZM_BuildMonsterRecord Zenith_Asserts on both its arguments and then FALLS
// THROUGH into ZM_GetSpeciesAbilities / ZM_GetSpeciesGrowthRate /
// ZM_GetSpeciesLearnset, each of which bounds-asserts and then reads out of range.
// So "the assert will catch it" is not a design -- the validation has to be a
// genuine early return, and this unit is what proves it is.
//
// The Zenith_Error lines it emits are EXPECTED output, not a failure.
//
// State equality is asserted FIELD-WISE, never by memcmp'ing a copy: the padding
// bytes of a copy-constructed ZM_GameState are unspecified and a byte compare
// would flake.
ZENITH_TEST(ZM_Starter, Apply_RejectsAnOutOfRangeChoiceWithoutMutatingTheState)
{
	const ZM_STARTER_CHOICE aeBad[2] =
	{
		ZM_STARTER_CHOICE_NONE,
		(ZM_STARTER_CHOICE)99u
	};

	ZM_GameState xState = ZM_MakeNewGameState();

	// NO ZENITH_ASSERT_* inside the capture scope, and the hit count copied out
	// before the closing brace (the destructor restores the previous count).
	bool abGranted[2] = { true, true };
	u_int uHits = 0u;
	{
		Zenith_AssertCaptureScope xCapture;
		for (u_int u = 0u; u < 2u; ++u)
		{
			abGranted[u] = ZM_ApplyStarterChoice(xState, aeBad[u]);
		}
		uHits = (u_int)xCapture.GetHitCount();
	}

	ZENITH_ASSERT_EQ(uHits, 0u,
		"ZM_ApplyStarterChoice reached an assert on an unregistered choice -- it "
		"validated too late (or not at all), and Zenith_Assert breaks in EVERY config, "
		"so in a real run this ends the whole boot unit gate");

	for (u_int u = 0u; u < 2u; ++u)
	{
		ZENITH_ASSERT_FALSE(abGranted[u],
			"choice %u reported a successful grant", (u_int)aeBad[u]);
	}

	ZENITH_ASSERT_EQ(xState.m_xParty.Count(), 0u,
		"a rejected choice still put a monster in the party");
	ZENITH_ASSERT_EQ(xState.GetCaughtCount(), 0u, "a rejected choice still marked a species caught");
	ZENITH_ASSERT_EQ(xState.GetSeenCount(), 0u, "a rejected choice still marked a species seen");
	ZENITH_ASSERT_EQ(xState.m_uMoney, uEXPECTED_STARTER_MONEY, "a rejected choice moved the purse");
	ZENITH_ASSERT_EQ(xState.m_xBag.GetCount(ZM_ITEM_CATCHORB), uEXPECTED_STARTER_ORBS,
		"a rejected choice moved the bag");
	ZENITH_ASSERT_EQ(xState.m_xBag.GetCount(ZM_ITEM_SALVE), uEXPECTED_STARTER_HEALS,
		"a rejected choice moved the bag");
}

// The grant owns PARTY AND DEX and nothing else. Catches the author who folds the
// economy, the badge mask, or the story beat into ZM_ApplyStarterChoice.
//
// The story-flag clause is deliberate and load-bearing: ZM_STORY_FLAG_STARTER_RECEIVED
// exists and is LEFT CLEAR by SC3, because setting it would move the save bytes and
// break the behaviour-preservation contract this sub-commit is built on. A later
// sub-commit owns that beat -- and when it lands, this assertion is what will say so.
ZENITH_TEST(ZM_Starter, Apply_TouchesOnlyPartyAndDex)
{
	ZM_GameState xState = ZM_MakeNewGameState();

	const u_int         uMoneyBefore   = xState.m_uMoney;
	const u_int         uOrbsBefore    = xState.m_xBag.GetCount(ZM_ITEM_CATCHORB);
	const u_int         uHealsBefore   = xState.m_xBag.GetCount(ZM_ITEM_SALVE);
	const u_int         uStacksBefore  = xState.m_xBag.TotalStackCount();
	const u_int         uFlagsBefore   = xState.m_xStoryFlags.Count();
	const u_int         uBadgesBefore  = xState.GetBadgeCount();
	const u_int         uBoxesBefore   = xState.m_xBoxes.Count();
	const u_int         uParentsBefore = xState.m_xDaycare.m_uParentCount;
	const u_int         uStreakBefore  = xState.m_xTowerRun.m_uCurrentStreak;
	const u_int         uSceneBefore   = xState.m_xWorldPosition.m_uSceneBuildIndex;
	const ZM_TEXT_SPEED eSpeedBefore   = xState.m_xOptions.m_eTextSpeed;
	const bool          bWhiteoutBefore = xState.m_bPendingWhiteout;

	ZENITH_ASSERT_TRUE(ZM_ApplyStarterChoice(xState, ZM_STARTER_CHOICE_FERNFAWN),
		"the grant under test reported failure, so the no-side-effect walk below "
		"would be measuring a no-op");

	ZENITH_ASSERT_EQ(xState.m_uMoney, uMoneyBefore, "the grant moved the purse");
	ZENITH_ASSERT_EQ(xState.m_xBag.GetCount(ZM_ITEM_CATCHORB), uOrbsBefore, "the grant moved the orbs");
	ZENITH_ASSERT_EQ(xState.m_xBag.GetCount(ZM_ITEM_SALVE), uHealsBefore, "the grant moved the salves");
	ZENITH_ASSERT_EQ(xState.m_xBag.TotalStackCount(), uStacksBefore, "the grant added a bag stack");
	ZENITH_ASSERT_EQ(xState.m_xStoryFlags.Count(), uFlagsBefore,
		"the grant set a STORY FLAG. ZM_STORY_FLAG_STARTER_RECEIVED is deliberately "
		"left clear in SC3: setting it moves the save bytes, which this sub-commit "
		"forbids. If a later sub-commit is turning that beat on, move this assertion "
		"with it rather than deleting it");
	ZENITH_ASSERT_EQ(xState.GetBadgeCount(), uBadgesBefore, "the grant awarded a badge");
	ZENITH_ASSERT_EQ(xState.m_xBoxes.Count(), uBoxesBefore, "the grant stored a boxed monster");
	ZENITH_ASSERT_EQ(xState.m_xDaycare.m_uParentCount, uParentsBefore, "the grant touched the daycare");
	ZENITH_ASSERT_EQ(xState.m_xTowerRun.m_uCurrentStreak, uStreakBefore, "the grant touched the tower run");
	ZENITH_ASSERT_EQ(xState.m_xWorldPosition.m_uSceneBuildIndex, uSceneBefore,
		"the grant wrote a resume point -- Continue would look loadable on a brand-new game");
	ZENITH_ASSERT_EQ((u_int)xState.m_xOptions.m_eTextSpeed, (u_int)eSpeedBefore,
		"the grant changed a player OPTION");
	ZENITH_ASSERT_EQ(xState.m_bPendingWhiteout, bWhiteoutBefore,
		"the grant armed the transient whiteout latch");
}

// ############################################################################
// F. ZM_CanEnterBattle
// ############################################################################

// The predicate keys on EMPTINESS ALONE, and the fainted clause is the whole point
// of the unit.
//
// ★ THE LAST ASSERTION IS SC3'S BEHAVIOUR-PRESERVATION RULING, WRITTEN DOWN.
// The "improved" form (!IsEmpty() && !AllFainted()) looks obviously better and is
// NOT inert: ZM_BattleWriteBack latches m_bPendingWhiteout on a loss and the heal
// only runs later, once ZM_GameStateManager sees an IDLE transition -- so an
// entirely fainted party is a REAL window that ZM_Interactable::TickTrainerSight
// ticks through every frame, and keying on fainting would change m_bMayEngage
// during it. This unit reds the moment anyone makes that change, which is exactly
// what it is for. (Note ZM_Party::AllFainted() answers TRUE for an EMPTY party --
// "vacuously all-fainted" -- which is what makes the wrong form look correct.)
ZENITH_TEST(ZM_Starter, CanEnterBattle_KeysOnPartyEmptinessAndIgnoresFainting)
{
	ZM_GameState xState = ZM_MakeNewGameState();
	ZENITH_ASSERT_FALSE(ZM_CanEnterBattle(xState),
		"a partyless player has nothing to send out");

	ZENITH_ASSERT_TRUE(ZM_ApplyStarterChoice(xState, ZM_STARTER_CHOICE_FERNFAWN),
		"the grant failed, so the transition below would be measuring nothing");
	ZENITH_ASSERT_TRUE(ZM_CanEnterBattle(xState),
		"a player holding a healthy starter can battle");
	if (xState.m_xParty.Count() != 1u) { return; }

	// Faint the lead by hand. m_uCurrentHp is a public field, so this is exactly the
	// shape a lost battle leaves behind before the deferred heal runs.
	xState.m_xParty.Get(0u).m_uCurrentHp = 0u;
	ZENITH_ASSERT_TRUE(xState.m_xParty.Get(0u).IsFainted(),
		"the hand-faint did not take, so the clause below is vacuous");
	ZENITH_ASSERT_TRUE(xState.m_xParty.AllFainted(),
		"a one-member party with a fainted lead must read as ALL fainted, or the "
		"tripwire below is not testing the form it exists to reject");

	ZENITH_ASSERT_TRUE(ZM_CanEnterBattle(xState),
		"ZM_CanEnterBattle now keys on FAINTING as well as emptiness. That is a LIVE "
		"behaviour change, not a tidy-up: the post-loss pre-heal window is real, and "
		"the trainer sight tick runs through it every frame, so m_bMayEngage would "
		"move during it. SC3 is behaviour-preserving by contract -- if this is a "
		"deliberate later change, it needs its own sub-commit and its own gate");
}

// ############################################################################
// G. The shipped rival, coupled to the new table
// ############################################################################

// The rival's authored team must be the counter to the starter the player actually
// receives. The row stays a LITERAL in ZM_TrainerData.cpp -- SC3 moves no
// behaviour -- so this unit is the coupling that catches a starter change which
// leaves Vesper holding the wrong monster.
ZENITH_TEST(ZM_Starter, Roster_VesperBringsTheAuthoredCounterToTheFernfawnStarter)
{
	const ZM_TrainerData& xVesper = ZM_GetTrainerData(ZM_TRAINER_RIVAL_VESPER);
	ZENITH_ASSERT_GE(xVesper.m_uPartyCount, 1u,
		"the rival's row has no team, so every clause below is vacuous");
	ZENITH_ASSERT_NOT_NULL(xVesper.m_paxParty, "a non-empty party count with a null array");
	if (xVesper.m_uPartyCount == 0u || xVesper.m_paxParty == nullptr) { return; }

	const ZM_SPECIES_ID eCounter =
		ZM_ResolveCounterStarterSpecies(ZM_STARTER_CHOICE_FERNFAWN);
	ZENITH_ASSERT_NE((u_int)eCounter, (u_int)ZM_SPECIES_NONE,
		"the Fernfawn row's counter column did not resolve to a species, so the "
		"comparison below would pass against a sentinel");

	ZENITH_ASSERT_EQ((u_int)xVesper.m_paxParty[0].m_eSpecies, (u_int)eCounter,
		"Vesper leads with species %u, but the starter table says the counter to the "
		"player's Fernfawn is %u -- the rival no longer answers the player's pick "
		"(Docs/GameDesignDocument.md section 3.3)",
		(u_int)xVesper.m_paxParty[0].m_eSpecies, (u_int)eCounter);
	ZENITH_ASSERT_EQ(xVesper.m_paxParty[0].m_uLevel, uZM_STARTER_LEVEL,
		"Vesper's lead is level %u against a level-%u starter -- the first rival "
		"battle is an even matchup by design",
		xVesper.m_paxParty[0].m_uLevel, uZM_STARTER_LEVEL);
}
