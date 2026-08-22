#include "Zenith.h"

// ============================================================================
// ZM_Tests_GroundItem -- the contract for the ground-item prop registry, the
// per-prop collected set, and the pure pickup path (ZM-D-201).
//
// Everything here is PURE and headless: a compiled const table, free functions
// over a by-value ZM_GameState, and (for the persistence unit) the ZM_SaveSchema
// writer/reader driven into a local owning stream. No slots, no disk, no ECS, no
// scene, no baked assets -- so no RequestSkip is needed.
//
// ★ THE LOAD-BEARING UNITS ARE Pickup_AStackAtTheCapLeavesBothTheBagAndTheFlag-
// Untouched AND Pickup_AFullPocketLeavesBothTheBagAndTheFlagUntouched. "Mark
// collected then Add" is the one ordering that destroys the item outright, and
// NOTHING ELSE in the game can see that mistake: the prop would still read as
// taken, the bag would be untouched, and both are perfectly legal states in
// isolation. Those two units are what make the order in ZM_TryPickUpGroundItem a
// contract rather than a comment.
//
// ★ THE WIRE UNIT IS Persistence_CollectedStateSurvivesASaveSchemaRoundTrip. The
// enum VALUE *is* the id persisted by save module 12, so a renumbering silently
// re-points every save's collected bits; the LITERAL wire ids are pinned in
// Tests/ZM_Tests_SaveMigration.cpp's v2 golden, which is where a claim phrased
// against (u_int)eId could not have been made.
// ============================================================================

#include <cstring>   // strcmp -- debug-name distinctness

#include "Core/Zenith_ErrorCode.h"
#include "Core/Zenith_TestFramework.h"
#include "DataStream/Zenith_DataStream.h"
#include "Zenithmon/Source/Core/ZM_SaveSchema.h"
#include "Zenithmon/Source/Party/ZM_Bag.h"
#include "Zenithmon/Source/Party/ZM_GameState.h"
#include "Zenithmon/Source/World/ZM_GroundItem.h"

namespace
{
	// An id no registry row can ever cover, distinct from the sentinel so the two
	// out-of-range shapes are driven separately (the sentinel is a value authored
	// data legitimately carries; this one is garbage).
	constexpr ZM_GROUND_ITEM_ID eZM_TEST_GROUND_ITEM_GARBAGE =
		(ZM_GROUND_ITEM_ID)(ZM_GROUND_ITEM_COUNT + 7u);

	// Compare the two things a pickup is allowed to move, and nothing else. Used by
	// every rejection unit: a refused pickup must leave BOTH bit-identical, and
	// checking only one of them is how "mark collected then Add" would slip through.
	void AssertBagAndCollectedUnchanged(const ZM_GameState& xBefore,
		const ZM_GameState& xAfter, const char* szContext)
	{
		for (u_int uItem = 0u; uItem < (u_int)ZM_ITEM_COUNT; ++uItem)
		{
			ZENITH_ASSERT_EQ(xAfter.m_xBag.GetCount((ZM_ITEM_ID)uItem),
				xBefore.m_xBag.GetCount((ZM_ITEM_ID)uItem),
				"%s changed the bag count of item %u", szContext, uItem);
		}
		for (u_int uProp = 0u; uProp < (u_int)ZM_GROUND_ITEM_COUNT; ++uProp)
		{
			ZENITH_ASSERT_EQ(xAfter.m_xCollectedGroundItems.IsSet((ZM_GROUND_ITEM_ID)uProp),
				xBefore.m_xCollectedGroundItems.IsSet((ZM_GROUND_ITEM_ID)uProp),
				"%s changed the collected flag of prop %u", szContext, uProp);
		}
		ZENITH_ASSERT_EQ(xAfter.m_xCollectedGroundItems.Count(),
			xBefore.m_xCollectedGroundItems.Count(),
			"%s changed the collected-prop count", szContext);
	}
}

ZENITH_TEST(ZM_GroundItem, Registry_RowsAreDenseSelfConsistentAndDistinctlyNamed)
{
	ZENITH_ASSERT_EQ(ZM_GetGroundItemCount(), (u_int)ZM_GROUND_ITEM_COUNT,
		"the accessor and the enum disagree about the registry size");
	ZENITH_ASSERT_GT((u_int)ZM_GROUND_ITEM_COUNT, 0u,
		"an empty registry makes every positive unit in this file vacuous");
	ZENITH_ASSERT_EQ((u_int)ZM_GROUND_ITEM_NONE, (u_int)ZM_GROUND_ITEM_COUNT,
		"the sentinel must alias the count, so one range check covers both");

	for (u_int uProp = 0u; uProp < (u_int)ZM_GROUND_ITEM_COUNT; ++uProp)
	{
		const ZM_GroundItemInfo& xRow = ZM_GetGroundItemInfo((ZM_GROUND_ITEM_ID)uProp);
		// The row's own id must equal its INDEX. This is the check that catches a
		// reorder, which would silently re-point every existing save's collected bits.
		ZENITH_ASSERT_EQ((u_int)xRow.m_eId, uProp, "row %u carries the wrong id", uProp);
		ZENITH_ASSERT_NOT_NULL(xRow.m_szDebugName, "row %u has no debug name", uProp);
		if (xRow.m_szDebugName != nullptr)
		{
			ZENITH_ASSERT_NE((u_int)(unsigned char)xRow.m_szDebugName[0], 0u,
				"row %u has an empty debug name", uProp);
		}
		// A row must yield a REAL item, or the prop is un-takeable content.
		ZENITH_ASSERT_LT((u_int)xRow.m_eItem, (u_int)ZM_ITEM_COUNT,
			"row %u yields an item outside the item table", uProp);
		ZENITH_ASSERT_GE(xRow.m_uCount, 1u, "row %u yields zero copies", uProp);
		ZENITH_ASSERT_LE(xRow.m_uCount, uZM_BAG_MAX_STACK_COUNT,
			"row %u yields more copies than one stack can ever hold", uProp);

		for (u_int uOther = uProp + 1u; uOther < (u_int)ZM_GROUND_ITEM_COUNT; ++uOther)
		{
			const ZM_GroundItemInfo& xOther = ZM_GetGroundItemInfo((ZM_GROUND_ITEM_ID)uOther);
			if (xRow.m_szDebugName == nullptr || xOther.m_szDebugName == nullptr) { continue; }
			ZENITH_ASSERT_NE(strcmp(xRow.m_szDebugName, xOther.m_szDebugName), 0,
				"rows %u and %u share the debug name '%s'", uProp, uOther, xRow.m_szDebugName);
		}
	}
}

ZENITH_TEST(ZM_GroundItem, Registry_AccessorsAreTotalOnTheSentinelAndOutOfRange)
{
	// The UNKNOWN row must be DOUBLY inert: an out-of-range item AND a zero count,
	// either of which ZM_Bag::CanAdd refuses on its own. A plausible-looking fallback
	// row is how a caller that skipped its range check gets a free item.
	const ZM_GROUND_ITEM_ID aeBadIds[] =
	{
		ZM_GROUND_ITEM_NONE,
		eZM_TEST_GROUND_ITEM_GARBAGE,
		(ZM_GROUND_ITEM_ID)0xffffffffu,
	};
	for (const ZM_GROUND_ITEM_ID eBad : aeBadIds)
	{
		const ZM_GroundItemInfo& xRow = ZM_GetGroundItemInfo(eBad);
		ZENITH_ASSERT_EQ((u_int)xRow.m_eId, (u_int)ZM_GROUND_ITEM_NONE,
			"the fallback row for id %u is not the sentinel row", (u_int)eBad);
		ZENITH_ASSERT_GE((u_int)xRow.m_eItem, (u_int)ZM_ITEM_COUNT,
			"the fallback row for id %u yields a REAL item", (u_int)eBad);
		ZENITH_ASSERT_EQ(xRow.m_uCount, 0u,
			"the fallback row for id %u yields a non-zero count", (u_int)eBad);

		const ZM_Bag xEmptyBag;
		ZENITH_ASSERT_FALSE(xEmptyBag.CanAdd(xRow.m_eItem, xRow.m_uCount),
			"an EMPTY bag accepted the fallback row for id %u", (u_int)eBad);
	}

	ZENITH_ASSERT_NOT_NULL(ZM_GroundItemName(ZM_GROUND_ITEM_NONE));
	ZENITH_ASSERT_STREQ(ZM_GroundItemName(ZM_GROUND_ITEM_NONE), "NONE");
	ZENITH_ASSERT_STREQ(ZM_GroundItemName(eZM_TEST_GROUND_ITEM_GARBAGE), "UNKNOWN");
	for (u_int uProp = 0u; uProp < (u_int)ZM_GROUND_ITEM_COUNT; ++uProp)
	{
		const char* szName = ZM_GroundItemName((ZM_GROUND_ITEM_ID)uProp);
		ZENITH_ASSERT_NOT_NULL(szName, "prop %u has no name", uProp);
		if (szName == nullptr) { continue; }
		ZENITH_ASSERT_NE(strcmp(szName, "UNKNOWN"), 0,
			"registered prop %u names itself UNKNOWN", uProp);
		ZENITH_ASSERT_NE(strcmp(szName, "NONE"), 0,
			"registered prop %u names itself NONE", uProp);
	}
}

ZENITH_TEST(ZM_GroundItem, CollectedSet_MarkIsSetCountAndClearAreTotal)
{
	ZM_GroundItemSet xSet;
	ZENITH_ASSERT_EQ(xSet.Count(), 0u, "a default-constructed set is not empty");
	for (u_int uProp = 0u; uProp < (u_int)ZM_GROUND_ITEM_COUNT; ++uProp)
	{
		ZENITH_ASSERT_FALSE(xSet.IsSet((ZM_GROUND_ITEM_ID)uProp),
			"a default-constructed set already holds prop %u", uProp);
	}

	// Out-of-range writes are IGNORED, never a stray bit past the array.
	xSet.Mark(ZM_GROUND_ITEM_NONE);
	xSet.Mark(eZM_TEST_GROUND_ITEM_GARBAGE);
	ZENITH_ASSERT_EQ(xSet.Count(), 0u, "an out-of-range Mark changed the set");
	ZENITH_ASSERT_FALSE(xSet.IsSet(ZM_GROUND_ITEM_NONE));
	ZENITH_ASSERT_FALSE(xSet.IsSet(eZM_TEST_GROUND_ITEM_GARBAGE));

	for (u_int uProp = 0u; uProp < (u_int)ZM_GROUND_ITEM_COUNT; ++uProp)
	{
		xSet.Mark((ZM_GROUND_ITEM_ID)uProp);
		ZENITH_ASSERT_TRUE(xSet.IsSet((ZM_GROUND_ITEM_ID)uProp), "Mark of prop %u did not take", uProp);
		ZENITH_ASSERT_EQ(xSet.Count(), uProp + 1u, "count after marking prop %u", uProp);
		// IDEMPOTENT: a second Mark is not a second entry.
		xSet.Mark((ZM_GROUND_ITEM_ID)uProp);
		ZENITH_ASSERT_EQ(xSet.Count(), uProp + 1u, "re-marking prop %u double-counted", uProp);
	}

	xSet.Clear();
	ZENITH_ASSERT_EQ(xSet.Count(), 0u, "Clear left entries behind");
	for (u_int uProp = 0u; uProp < (u_int)ZM_GROUND_ITEM_COUNT; ++uProp)
	{
		ZENITH_ASSERT_FALSE(xSet.IsSet((ZM_GROUND_ITEM_ID)uProp),
			"Clear left prop %u set", uProp);
	}
}

ZENITH_TEST(ZM_GroundItem, PickupResultName_IsTotalAcrossTheEnumAndBeyond)
{
	for (u_int uResult = 0u; uResult < (u_int)ZM_GROUND_ITEM_PICKUP_COUNT; ++uResult)
	{
		const char* szName = ZM_GroundItemPickupName((ZM_GROUND_ITEM_PICKUP)uResult);
		ZENITH_ASSERT_NOT_NULL(szName, "result %u has no name", uResult);
		if (szName == nullptr) { continue; }
		ZENITH_ASSERT_NE(strcmp(szName, "UNKNOWN"), 0,
			"enumerated result %u names itself UNKNOWN", uResult);
	}
	ZENITH_ASSERT_STREQ(ZM_GroundItemPickupName(ZM_GROUND_ITEM_PICKUP_COUNT), "UNKNOWN");
	ZENITH_ASSERT_STREQ(
		ZM_GroundItemPickupName((ZM_GROUND_ITEM_PICKUP)0xffffffffu), "UNKNOWN");
	ZENITH_ASSERT_EQ((u_int)ZM_GROUND_ITEM_PICKUP_OK, 0u,
		"OK must stay zero -- callers test it as the only success value");
}

ZENITH_TEST(ZM_GroundItem, Pickup_FirstPickupMovesTheItemIntoTheBagAndSetsTheFlag)
{
	ZM_GameState xState = ZM_MakeNewGameState();
	const ZM_GROUND_ITEM_ID eProp = ZM_GROUND_ITEM_ROUTE1_LANE_CATCHORB;
	const ZM_GroundItemInfo& xRow = ZM_GetGroundItemInfo(eProp);
	const u_int uBefore = xState.m_xBag.GetCount(xRow.m_eItem);

	ZENITH_ASSERT_FALSE(xState.m_xCollectedGroundItems.IsSet(eProp),
		"a new game already holds prop %s", ZM_GroundItemName(eProp));
	ZENITH_ASSERT_EQ((u_int)ZM_CanPickUpGroundItem(xState, eProp),
		(u_int)ZM_GROUND_ITEM_PICKUP_OK, "a new game cannot take %s", ZM_GroundItemName(eProp));

	const ZM_GROUND_ITEM_PICKUP eResult = ZM_TryPickUpGroundItem(xState, eProp);
	ZENITH_ASSERT_EQ((u_int)eResult, (u_int)ZM_GROUND_ITEM_PICKUP_OK,
		"pickup returned %s", ZM_GroundItemPickupName(eResult));
	ZENITH_ASSERT_EQ(xState.m_xBag.GetCount(xRow.m_eItem), uBefore + xRow.m_uCount,
		"the bag did not gain exactly the row's stack");
	ZENITH_ASSERT_TRUE(xState.m_xCollectedGroundItems.IsSet(eProp),
		"the prop was not recorded as collected");
	ZENITH_ASSERT_EQ(xState.m_xCollectedGroundItems.Count(), 1u,
		"one pickup marked more than one prop");
}

ZENITH_TEST(ZM_GroundItem, Pickup_SecondPickupOfTheSamePropIsAStrictNoOp)
{
	ZM_GameState xState = ZM_MakeNewGameState();
	const ZM_GROUND_ITEM_ID eProp = ZM_GROUND_ITEM_ROUTE1_SOUTH_SALVE;
	ZENITH_ASSERT_EQ((u_int)ZM_TryPickUpGroundItem(xState, eProp),
		(u_int)ZM_GROUND_ITEM_PICKUP_OK, "the first pickup failed");

	const ZM_GameState xAfterFirst = xState;
	for (u_int uAttempt = 0u; uAttempt < 3u; ++uAttempt)
	{
		ZENITH_ASSERT_EQ((u_int)ZM_CanPickUpGroundItem(xState, eProp),
			(u_int)ZM_GROUND_ITEM_PICKUP_REJECT_ALREADY_COLLECTED,
			"attempt %u: the predicate did not report ALREADY_COLLECTED", uAttempt);
		const ZM_GROUND_ITEM_PICKUP eResult = ZM_TryPickUpGroundItem(xState, eProp);
		ZENITH_ASSERT_EQ((u_int)eResult,
			(u_int)ZM_GROUND_ITEM_PICKUP_REJECT_ALREADY_COLLECTED,
			"attempt %u returned %s", uAttempt, ZM_GroundItemPickupName(eResult));
		AssertBagAndCollectedUnchanged(xAfterFirst, xState, "a repeat pickup");
	}
}

ZENITH_TEST(ZM_GroundItem, Pickup_AStackAtTheCapLeavesBothTheBagAndTheFlagUntouched)
{
	// The REACHABLE bag refusal: 999 of the yielded item already held, so CanAdd has
	// no headroom for even one more.
	const ZM_GROUND_ITEM_ID eProp = ZM_GROUND_ITEM_ROUTE1_SOUTH_SALVE;
	const ZM_GroundItemInfo& xRow = ZM_GetGroundItemInfo(eProp);

	ZM_GameState xState;
	ZENITH_ASSERT_TRUE(xState.m_xBag.Add(xRow.m_eItem, uZM_BAG_MAX_STACK_COUNT),
		"the fixture could not fill the stack to its cap");
	const ZM_GameState xBefore = xState;

	ZENITH_ASSERT_EQ((u_int)ZM_CanPickUpGroundItem(xState, eProp),
		(u_int)ZM_GROUND_ITEM_PICKUP_REJECT_BAG_REFUSED,
		"a capped stack did not refuse the prop");
	const ZM_GROUND_ITEM_PICKUP eResult = ZM_TryPickUpGroundItem(xState, eProp);
	ZENITH_ASSERT_EQ((u_int)eResult, (u_int)ZM_GROUND_ITEM_PICKUP_REJECT_BAG_REFUSED,
		"pickup returned %s", ZM_GroundItemPickupName(eResult));

	// ★ THE CLAUSE THIS WHOLE UNIT EXISTS FOR. "Mark collected then Add" would leave
	// the flag SET here with the bag untouched -- the prop burnt for the life of the
	// save and the item gone -- and nothing else in the game could see it.
	AssertBagAndCollectedUnchanged(xBefore, xState, "a pickup refused by the stack cap");
	ZENITH_ASSERT_FALSE(xState.m_xCollectedGroundItems.IsSet(eProp),
		"a refused pickup burnt the prop");

	// ...and once there IS room again, the prop is still there to take.
	ZENITH_ASSERT_TRUE(xState.m_xBag.Remove(xRow.m_eItem, xRow.m_uCount),
		"the fixture could not make room again");
	ZENITH_ASSERT_EQ((u_int)ZM_TryPickUpGroundItem(xState, eProp),
		(u_int)ZM_GROUND_ITEM_PICKUP_OK, "the prop did not survive its own refusal");
}

ZENITH_TEST(ZM_GroundItem, Pickup_AFullPocketLeavesBothTheBagAndTheFlagUntouched)
{
	// The pocket is FORCED full rather than filled from the item table: the table
	// cannot fill one, and ZM_Tests_Bag's per-category walk is what keeps it that way
	// (uZM_BAG_MAX_STACKS_PER_POCKET is >= the largest category). The branch still
	// exists in ZM_Bag::CanAdd, so the pickup's response to it is still a contract.
	const ZM_GROUND_ITEM_ID eProp = ZM_GROUND_ITEM_ROUTE1_SOUTH_SALVE;
	const ZM_GroundItemInfo& xRow = ZM_GetGroundItemInfo(eProp);
	const ZM_ITEM_CATEGORY eCategory = ZM_GetItemData(xRow.m_eItem).m_eCategory;

	ZM_GameState xState;
	xState.m_xBag.m_auPocketCount[(u_int)eCategory] = uZM_BAG_MAX_STACKS_PER_POCKET;
	const ZM_GameState xBefore = xState;

	ZENITH_ASSERT_EQ((u_int)ZM_CanPickUpGroundItem(xState, eProp),
		(u_int)ZM_GROUND_ITEM_PICKUP_REJECT_BAG_REFUSED,
		"a full pocket did not refuse the prop");
	const ZM_GROUND_ITEM_PICKUP eResult = ZM_TryPickUpGroundItem(xState, eProp);
	ZENITH_ASSERT_EQ((u_int)eResult, (u_int)ZM_GROUND_ITEM_PICKUP_REJECT_BAG_REFUSED,
		"pickup returned %s", ZM_GroundItemPickupName(eResult));
	AssertBagAndCollectedUnchanged(xBefore, xState, "a pickup refused by a full pocket");
	ZENITH_ASSERT_FALSE(xState.m_xCollectedGroundItems.IsSet(eProp),
		"a refused pickup burnt the prop");
}

ZENITH_TEST(ZM_GroundItem, Pickup_UnknownAndSentinelPropsAreRejectedWithoutSideEffects)
{
	const ZM_GROUND_ITEM_ID aeBadIds[] =
	{
		ZM_GROUND_ITEM_NONE,
		eZM_TEST_GROUND_ITEM_GARBAGE,
		(ZM_GROUND_ITEM_ID)0xffffffffu,
	};
	for (const ZM_GROUND_ITEM_ID eBad : aeBadIds)
	{
		ZM_GameState xState = ZM_MakeNewGameState();
		const ZM_GameState xBefore = xState;

		ZENITH_ASSERT_EQ((u_int)ZM_CanPickUpGroundItem(xState, eBad),
			(u_int)ZM_GROUND_ITEM_PICKUP_REJECT_UNKNOWN_PROP,
			"the predicate accepted unregistered prop %u", (u_int)eBad);
		const ZM_GROUND_ITEM_PICKUP eResult = ZM_TryPickUpGroundItem(xState, eBad);
		ZENITH_ASSERT_EQ((u_int)eResult, (u_int)ZM_GROUND_ITEM_PICKUP_REJECT_UNKNOWN_PROP,
			"unregistered prop %u returned %s", (u_int)eBad,
			ZM_GroundItemPickupName(eResult));
		AssertBagAndCollectedUnchanged(xBefore, xState, "a pickup of an unregistered prop");
	}
}

ZENITH_TEST(ZM_GroundItem, Pickup_RejectPrecedenceIsFixedWhenBlockersHoldTogether)
{
	// The precedence is FIXED so a reject reason is stable no matter how many blockers
	// hold at once -- and an ordering is only OBSERVABLE when two of them are true
	// simultaneously, which is exactly what this unit constructs. Every other
	// rejection unit here drives one blocker at a time and could not see a swap.
	const ZM_GROUND_ITEM_ID eProp = ZM_GROUND_ITEM_ROUTE1_SOUTH_SALVE;
	const ZM_GroundItemInfo& xRow = ZM_GetGroundItemInfo(eProp);

	// Collected AND the bag refuses THAT prop's stack: ALREADY_COLLECTED wins, so a
	// player standing at an exhausted prop with a capped stack is told the truth about
	// the PROP rather than a temporary-sounding "no room".
	ZM_GameState xBoth;
	xBoth.m_xCollectedGroundItems.Mark(eProp);
	ZENITH_ASSERT_TRUE(xBoth.m_xBag.Add(xRow.m_eItem, uZM_BAG_MAX_STACK_COUNT),
		"the fixture could not fill the stack to its cap");
	ZENITH_ASSERT_FALSE(xBoth.m_xBag.CanAdd(xRow.m_eItem, xRow.m_uCount),
		"the fixture's bag is not actually refusing, so this unit is vacuous");
	ZENITH_ASSERT_EQ((u_int)ZM_CanPickUpGroundItem(xBoth, eProp),
		(u_int)ZM_GROUND_ITEM_PICKUP_REJECT_ALREADY_COLLECTED,
		"a refusing bag masked the already-collected answer");
	ZENITH_ASSERT_EQ((u_int)ZM_TryPickUpGroundItem(xBoth, eProp),
		(u_int)ZM_GROUND_ITEM_PICKUP_REJECT_ALREADY_COLLECTED,
		"the mutator and the predicate disagree about precedence");

	// Unregistered, against the SAME loaded state: UNKNOWN_PROP wins outright. The
	// range check leads, so the lookup never reaches ZM_GetGroundItemInfo -- which is
	// what keeps a caller merely ASKING about a bad id out of the error log.
	ZENITH_ASSERT_EQ((u_int)ZM_CanPickUpGroundItem(xBoth, ZM_GROUND_ITEM_NONE),
		(u_int)ZM_GROUND_ITEM_PICKUP_REJECT_UNKNOWN_PROP,
		"an unregistered prop did not outrank every other blocker");
}

ZENITH_TEST(ZM_GroundItem, Pickup_TwoPropsYieldingTheSameItemAreIndependentlyCollected)
{
	// Collected state is per-PROP, never per-ITEM. Two props on the same route can
	// both hand out a Salve, and taking one must not exhaust the other -- which is
	// precisely the bug a collected set keyed on ZM_ITEM_ID would ship.
	const ZM_GROUND_ITEM_ID eFirst = ZM_GROUND_ITEM_ROUTE1_SOUTH_SALVE;
	const ZM_GROUND_ITEM_ID eSecond = ZM_GROUND_ITEM_ROUTE1_NORTH_SALVE;
	const ZM_GroundItemInfo& xFirstRow = ZM_GetGroundItemInfo(eFirst);
	const ZM_GroundItemInfo& xSecondRow = ZM_GetGroundItemInfo(eSecond);
	ZENITH_ASSERT_EQ((u_int)xFirstRow.m_eItem, (u_int)xSecondRow.m_eItem,
		"this unit is vacuous unless the two props yield the SAME item");

	ZM_GameState xState;
	const u_int uBefore = xState.m_xBag.GetCount(xFirstRow.m_eItem);
	ZENITH_ASSERT_EQ((u_int)ZM_TryPickUpGroundItem(xState, eFirst),
		(u_int)ZM_GROUND_ITEM_PICKUP_OK, "the first prop refused");
	ZENITH_ASSERT_EQ((u_int)ZM_CanPickUpGroundItem(xState, eSecond),
		(u_int)ZM_GROUND_ITEM_PICKUP_OK,
		"taking the first prop exhausted the second");
	ZENITH_ASSERT_EQ((u_int)ZM_TryPickUpGroundItem(xState, eSecond),
		(u_int)ZM_GROUND_ITEM_PICKUP_OK, "the second prop refused");

	ZENITH_ASSERT_EQ(xState.m_xBag.GetCount(xFirstRow.m_eItem),
		uBefore + xFirstRow.m_uCount + xSecondRow.m_uCount,
		"the two props did not both deliver");
	ZENITH_ASSERT_EQ(xState.m_xCollectedGroundItems.Count(), 2u,
		"the two props did not both record themselves");
}

ZENITH_TEST(ZM_GroundItem, Pickup_EveryRegistryRowIsCollectableFromANewGameState)
{
	// A CONTENT check, not a logic one: every shipped row must actually be takeable by
	// a real player. A row whose item id, category or count the bag refuses is a prop
	// nobody can ever pick up, and no other unit in this file would notice.
	ZM_GameState xState = ZM_MakeNewGameState();
	for (u_int uProp = 0u; uProp < (u_int)ZM_GROUND_ITEM_COUNT; ++uProp)
	{
		const ZM_GROUND_ITEM_ID eProp = (ZM_GROUND_ITEM_ID)uProp;
		const ZM_GroundItemInfo& xRow = ZM_GetGroundItemInfo(eProp);
		const u_int uBefore = xState.m_xBag.GetCount(xRow.m_eItem);
		const ZM_GROUND_ITEM_PICKUP eResult = ZM_TryPickUpGroundItem(xState, eProp);
		ZENITH_ASSERT_EQ((u_int)eResult, (u_int)ZM_GROUND_ITEM_PICKUP_OK,
			"prop %s (%u) is not collectable from a new game: %s",
			ZM_GroundItemName(eProp), uProp, ZM_GroundItemPickupName(eResult));
		ZENITH_ASSERT_EQ(xState.m_xBag.GetCount(xRow.m_eItem), uBefore + xRow.m_uCount,
			"prop %s did not deliver its stack", ZM_GroundItemName(eProp));
	}
	ZENITH_ASSERT_EQ(xState.m_xCollectedGroundItems.Count(), (u_int)ZM_GROUND_ITEM_COUNT,
		"collecting every prop did not mark every prop");
}

ZENITH_TEST(ZM_GroundItem, Pickup_CanPickUpPredictsExactlyWhatTryPickUpDoes)
{
	// The two share one accept rule by construction (TryPickUp CALLS CanPickUp), and
	// this drives that claim over every id -- the sentinel included -- against four
	// differently-shaped states, so a future respelling that split them apart reds
	// here rather than in the field.
	//
	// ★ THE STATES ARE BUILT ONE AT A TIME RATHER THAN HELD IN AN ARRAY, and the
	// predicate is asked against xBase DIRECTLY rather than against a third copy. A
	// ZM_GameState is ~80 KB (16x30 box slots plus a 4096-bit flag block), so an array
	// of four plus working copies is a large multiple of the frame every other unit
	// in this suite lives inside. Two live at once here, which is what the existing
	// codec units already hold. Nothing is lost by dropping the probe copy:
	// ZM_CanPickUpGroundItem takes a `const ZM_GameState&`, so a no-mutation check on
	// it would be re-testing the type system.
	for (u_int uState = 0u; uState < 4u; ++uState)
	{
		ZM_GameState xBase;
		switch (uState)
		{
		case 0u: break;                                 // the default aggregate
		case 1u: xBase = ZM_MakeNewGameState(); break;  // a fresh playthrough
		case 2u:                                        // every yielded stack at its cap
			xBase.m_xBag.Add(ZM_ITEM_SALVE, uZM_BAG_MAX_STACK_COUNT);
			xBase.m_xBag.Add(ZM_ITEM_CATCHORB, uZM_BAG_MAX_STACK_COUNT);
			break;
		default:                                        // nothing left to take
			xBase = ZM_MakeNewGameState();
			for (u_int uProp = 0u; uProp < (u_int)ZM_GROUND_ITEM_COUNT; ++uProp)
			{
				xBase.m_xCollectedGroundItems.Mark((ZM_GROUND_ITEM_ID)uProp);
			}
			break;
		}

		for (u_int uProp = 0u; uProp <= (u_int)ZM_GROUND_ITEM_COUNT; ++uProp)
		{
			const ZM_GROUND_ITEM_ID eProp = (ZM_GROUND_ITEM_ID)uProp;
			const ZM_GROUND_ITEM_PICKUP ePredicted = ZM_CanPickUpGroundItem(xBase, eProp);

			ZM_GameState xMutated = xBase;
			const ZM_GROUND_ITEM_PICKUP eActual = ZM_TryPickUpGroundItem(xMutated, eProp);
			ZENITH_ASSERT_EQ((u_int)eActual, (u_int)ePredicted,
				"state %u prop %u: predicted %s, got %s", uState, uProp,
				ZM_GroundItemPickupName(ePredicted), ZM_GroundItemPickupName(eActual));

			if (eActual != ZM_GROUND_ITEM_PICKUP_OK)
			{
				AssertBagAndCollectedUnchanged(xBase, xMutated,
					"a refused ZM_TryPickUpGroundItem");
			}
		}
	}
}

ZENITH_TEST(ZM_GroundItem, Persistence_CollectedStateSurvivesASaveSchemaRoundTrip)
{
	ZM_GameState xSource = ZM_MakeNewGameState();
	ZENITH_ASSERT_EQ((u_int)ZM_TryPickUpGroundItem(xSource, ZM_GROUND_ITEM_ROUTE1_SOUTH_SALVE),
		(u_int)ZM_GROUND_ITEM_PICKUP_OK, "the fixture pickup failed");
	ZENITH_ASSERT_EQ((u_int)ZM_TryPickUpGroundItem(xSource, ZM_GROUND_ITEM_ROUTE1_NORTH_SALVE),
		(u_int)ZM_GROUND_ITEM_PICKUP_OK, "the fixture pickup failed");
	// Deliberately NOT all of them: a codec that marked every prop on load would pass
	// an all-collected fixture, and a codec that marked none would pass an empty one.
	ZENITH_ASSERT_FALSE(xSource.m_xCollectedGroundItems.IsSet(ZM_GROUND_ITEM_ROUTE1_LANE_CATCHORB),
		"the fixture must leave at least one prop UNCOLLECTED to be non-vacuous");

	Zenith_DataStream xStream;
	const Zenith_Status xWriteStatus = ZM_SaveSchema::Write(xSource, xStream);
	ZENITH_ASSERT_TRUE(xWriteStatus.IsOk(),
		"the writer rejected a state carrying collected props (error %u)",
		(u_int)xWriteStatus.Error());
	if (!xWriteStatus.IsOk()) { return; }

	const uint64_t ulLength = xStream.GetCursor();
	Zenith_DataStream xInput(xStream.GetData(), ulLength);
	ZM_GameState xDecoded;
	// Pre-loaded with the OPPOSITE set, so "the decoder wrote nothing" cannot pass.
	xDecoded.m_xCollectedGroundItems.Mark(ZM_GROUND_ITEM_ROUTE1_LANE_CATCHORB);
	const Zenith_Status xReadStatus = ZM_SaveSchema::Read(xInput, ulLength, xDecoded);
	ZENITH_ASSERT_TRUE(xReadStatus.IsOk(), "the reader rejected its own output (error %u)",
		(u_int)xReadStatus.Error());
	if (!xReadStatus.IsOk()) { return; }

	for (u_int uProp = 0u; uProp < (u_int)ZM_GROUND_ITEM_COUNT; ++uProp)
	{
		ZENITH_ASSERT_EQ(xDecoded.m_xCollectedGroundItems.IsSet((ZM_GROUND_ITEM_ID)uProp),
			xSource.m_xCollectedGroundItems.IsSet((ZM_GROUND_ITEM_ID)uProp),
			"prop %u did not survive the round trip", uProp);
	}
	ZENITH_ASSERT_EQ(xDecoded.m_xCollectedGroundItems.Count(), 2u,
		"the decoded collected count is wrong");

	// A restored save must not hand the collected props out a second time, and must
	// still hand out the one that was left behind.
	ZENITH_ASSERT_EQ((u_int)ZM_CanPickUpGroundItem(xDecoded, ZM_GROUND_ITEM_ROUTE1_SOUTH_SALVE),
		(u_int)ZM_GROUND_ITEM_PICKUP_REJECT_ALREADY_COLLECTED,
		"a restored save re-offered a prop the player already took");
	ZENITH_ASSERT_EQ((u_int)ZM_CanPickUpGroundItem(xDecoded, ZM_GROUND_ITEM_ROUTE1_LANE_CATCHORB),
		(u_int)ZM_GROUND_ITEM_PICKUP_OK,
		"a restored save withheld a prop the player never took");
}
