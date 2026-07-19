#include "Zenith.h"

// ============================================================================
// ZM_Tests_Bag -- S6 item 2 (SC3) unit tests for the pure bag + money model:
// ZM_Bag's pocketed, ascending-by-id stack container and ZM_GameState's saturating
// money helpers, plus the starter seed that SC6 (Bag screen) and SC7 (shop) both
// build on. Everything is PURE -- no ECS, no scene, no graphics, no baked assets --
// so every fixture is hermetic and no RequestSkip is needed.
//
// Two contracts are asserted hardest because SC7's shop transaction depends on
// them: every mutator is ALL-OR-NOTHING (a rejected Add after money was deducted
// would destroy the purse), and a count-0 stack is NEVER stored (SaveFormat module
// 6 forbids writing one). Pockets are resolved through ZM_GetItemData, never a
// hard-coded index, so a table re-categorisation shows up as a real failure.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/Party/ZM_Bag.h"
#include "Zenithmon/Source/Party/ZM_GameState.h"

namespace
{
	// Three BALL-pocket items in ASCENDING id order (CATCHORB < GREATORB < ULTRAORB),
	// used by the ordering tests -- they are ADDED descending and must read back
	// ascending.
	constexpr ZM_ITEM_ID eBALL_LOW  = ZM_ITEM_CATCHORB;
	constexpr ZM_ITEM_ID eBALL_MID  = ZM_ITEM_GREATORB;
	constexpr ZM_ITEM_ID eBALL_HIGH = ZM_ITEM_ULTRAORB;

	// A MEDICINE-pocket item, for the pocket-independence tests.
	constexpr ZM_ITEM_ID eMEDICINE = ZM_ITEM_SALVE;

	// An id past the end of the table (NONE is ZM_ITEM_COUNT itself; this is beyond).
	constexpr ZM_ITEM_ID eOUT_OF_RANGE = (ZM_ITEM_ID)((u_int)ZM_ITEM_COUNT + 7u);

	ZM_ITEM_CATEGORY PocketOf(ZM_ITEM_ID eItem)
	{
		return ZM_GetItemData(eItem).m_eCategory;
	}
}

// ---- Fresh state ------------------------------------------------------------

ZENITH_TEST(ZM_Bag, Fresh_IsCompletelyEmpty)
{
	ZM_Bag xBag;
	for (u_int uCategory = 0u; uCategory < (u_int)ZM_ITEM_CATEGORY_COUNT; ++uCategory)
	{
		ZENITH_ASSERT_EQ(xBag.PocketStackCount((ZM_ITEM_CATEGORY)uCategory), 0u,
			"a fresh bag has no stacks in pocket %s",
			ZM_ItemCategoryToString((ZM_ITEM_CATEGORY)uCategory));
	}
	ZENITH_ASSERT_EQ(xBag.TotalStackCount(), 0u, "a fresh bag writes 0 save entries");
	ZENITH_ASSERT_EQ(xBag.GetCount(eBALL_LOW), 0u, "a fresh bag holds none of a real item");
	ZENITH_ASSERT_FALSE(xBag.Has(eBALL_LOW), "Has is false for an item with no stack");
}

// ---- Add --------------------------------------------------------------------

ZENITH_TEST(ZM_Bag, Add_CreatesAStackInTheItemsOwnPocket)
{
	ZM_Bag xBag;
	ZENITH_ASSERT_TRUE(xBag.Add(eBALL_LOW, 3u), "adding a real item succeeds");
	ZENITH_ASSERT_EQ(xBag.GetCount(eBALL_LOW), 3u, "the whole requested count landed");
	ZENITH_ASSERT_TRUE(xBag.Has(eBALL_LOW), "the item is now held");
	ZENITH_ASSERT_EQ(xBag.TotalStackCount(), 1u, "one distinct item == one save entry");

	// The pocket is the item's TABLE category, never a hard-coded index.
	const ZM_ITEM_CATEGORY ePocket = PocketOf(eBALL_LOW);
	ZENITH_ASSERT_EQ(xBag.PocketStackCount(ePocket), 1u,
		"the stack landed in the pocket named by ZM_GetItemData");
	ZENITH_ASSERT_EQ((u_int)xBag.PocketStack(ePocket, 0u).m_eItem, (u_int)eBALL_LOW,
		"the pocket's first stack is the added item");
	ZENITH_ASSERT_EQ(xBag.PocketStack(ePocket, 0u).m_uCount, 3u,
		"the stored stack carries the count");
}

ZENITH_TEST(ZM_Bag, Add_SameItemStacksRatherThanDuplicating)
{
	ZM_Bag xBag;
	xBag.Add(eBALL_LOW, 3u);
	ZENITH_ASSERT_TRUE(xBag.Add(eBALL_LOW, 4u), "a second add of the same item succeeds");
	ZENITH_ASSERT_EQ(xBag.GetCount(eBALL_LOW), 7u, "the counts sum onto one stack");
	ZENITH_ASSERT_EQ(xBag.PocketStackCount(PocketOf(eBALL_LOW)), 1u,
		"stacking never creates a second entry for the same id");
	ZENITH_ASSERT_EQ(xBag.TotalStackCount(), 1u, "the save still writes a single entry");
}

ZENITH_TEST(ZM_Bag, Add_RejectsNoneAndOutOfRangeIds)
{
	ZM_Bag xBag;
	xBag.Add(eBALL_LOW, 2u);

	ZENITH_ASSERT_FALSE(xBag.Add(ZM_ITEM_NONE, 1u),
		"ZM_ITEM_NONE is rejected (it IS ZM_ITEM_COUNT, so the range check covers it)");
	ZENITH_ASSERT_FALSE(xBag.Add(eOUT_OF_RANGE, 1u), "an id past the table is rejected");

	ZENITH_ASSERT_EQ(xBag.TotalStackCount(), 1u, "a rejected add never creates a stack");
	ZENITH_ASSERT_EQ(xBag.GetCount(eBALL_LOW), 2u, "a rejected add never touches other stacks");
	ZENITH_ASSERT_EQ(xBag.GetCount(ZM_ITEM_NONE), 0u, "NONE is never held");
	ZENITH_ASSERT_EQ(xBag.GetCount(eOUT_OF_RANGE), 0u, "an out-of-range id is never held");
	ZENITH_ASSERT_FALSE(xBag.Has(eOUT_OF_RANGE), "Has is false for an out-of-range id");
}

ZENITH_TEST(ZM_Bag, Add_RejectsZeroCount)
{
	ZM_Bag xBag;
	xBag.Add(eBALL_LOW, 2u);
	ZENITH_ASSERT_FALSE(xBag.Add(eBALL_LOW, 0u), "adding zero of a held item is rejected");
	ZENITH_ASSERT_FALSE(xBag.Add(eBALL_MID, 0u), "adding zero of an unheld item is rejected");
	ZENITH_ASSERT_EQ(xBag.GetCount(eBALL_LOW), 2u, "a zero add never changes an existing count");
	ZENITH_ASSERT_EQ(xBag.TotalStackCount(), 1u, "a zero add never creates an empty stack");
}

ZENITH_TEST(ZM_Bag, Add_RejectsOverCapStackWithoutPartialCredit)
{
	// The all-or-nothing case SC7 depends on: money is deducted BEFORE Add, so a
	// "clamp and succeed" here would silently eat the difference.
	ZM_Bag xBag;
	ZENITH_ASSERT_TRUE(xBag.Add(eBALL_LOW, uZM_BAG_MAX_STACK_COUNT - 1u),
		"filling to one below the per-stack cap succeeds");
	ZENITH_ASSERT_FALSE(xBag.Add(eBALL_LOW, 2u),
		"an add that would push the stack past the cap is rejected outright");
	ZENITH_ASSERT_EQ(xBag.GetCount(eBALL_LOW), uZM_BAG_MAX_STACK_COUNT - 1u,
		"the rejected add left the pre-existing count exactly untouched (no partial credit)");

	// A huge count must not wrap the headroom arithmetic into an accidental success.
	ZENITH_ASSERT_FALSE(xBag.Add(eBALL_LOW, 0xFFFFFFFFu),
		"a count near UINT_MAX is rejected, not wrapped");
	ZENITH_ASSERT_FALSE(xBag.Add(eBALL_MID, 0xFFFFFFFFu),
		"a NEW stack larger than the cap is rejected too");
	// Probe the NEW-stack cap at exactly the boundary, not only 4.29 billion past it:
	// without this, weakening the new-stack threshold to anything below UINT_MAX-1
	// would still let every test pass while storing an over-cap stack.
	ZENITH_ASSERT_FALSE(xBag.Add(eBALL_MID, uZM_BAG_MAX_STACK_COUNT + 1u),
		"a NEW stack of exactly cap+1 is rejected (the boundary is inclusive)");
	ZENITH_ASSERT_EQ(xBag.GetCount(eBALL_MID), 0u, "the rejected new stack was never created");
	ZENITH_ASSERT_EQ(xBag.TotalStackCount(), 1u, "still exactly one stack");
	ZENITH_ASSERT_TRUE(xBag.Add(eBALL_MID, uZM_BAG_MAX_STACK_COUNT),
		"a NEW stack of exactly the cap is accepted");
	ZENITH_ASSERT_EQ(xBag.GetCount(eBALL_MID), uZM_BAG_MAX_STACK_COUNT,
		"the whole at-cap new stack landed");
}

ZENITH_TEST(ZM_Bag, Add_ExactlyAtTheStackCapSucceeds)
{
	ZM_Bag xBag;
	ZENITH_ASSERT_TRUE(xBag.Add(eBALL_LOW, uZM_BAG_MAX_STACK_COUNT),
		"the per-stack cap is INCLUSIVE -- exactly uZM_BAG_MAX_STACK_COUNT is allowed");
	ZENITH_ASSERT_EQ(xBag.GetCount(eBALL_LOW), uZM_BAG_MAX_STACK_COUNT,
		"the full capped stack is stored");
	ZENITH_ASSERT_FALSE(xBag.Add(eBALL_LOW, 1u), "one more is the first rejection");
	ZENITH_ASSERT_EQ(xBag.GetCount(eBALL_LOW), uZM_BAG_MAX_STACK_COUNT,
		"the rejection left the capped stack alone");

	// The STACKING path has its own headroom check, and its inclusive boundary needs
	// pinning separately: a `>=` there instead of `>` would make a capped stack
	// unreachable by stacking while every other case still passed.
	ZENITH_ASSERT_TRUE(xBag.Add(eBALL_MID, uZM_BAG_MAX_STACK_COUNT - 1u),
		"a second stack fills to one below the cap");
	ZENITH_ASSERT_TRUE(xBag.Add(eBALL_MID, 1u),
		"stacking to EXACTLY the cap succeeds -- the boundary is inclusive on the stacking path too");
	ZENITH_ASSERT_EQ(xBag.GetCount(eBALL_MID), uZM_BAG_MAX_STACK_COUNT,
		"the stacked-to-cap count is exactly the cap");
	ZENITH_ASSERT_FALSE(xBag.Add(eBALL_MID, 1u), "and one past it is then rejected");
}

// NOTE: there is deliberately NO "pocket full" test. Filling a pocket would need
// more than uZM_BAG_MAX_STACKS_PER_POCKET (64) DISTINCT items in one category, and
// the largest category in the table today holds 25 (TM). The rejection branch is
// therefore unreachable by construction; ItemTable_NoCategoryCanOverflowAPocket
// below is what KEEPS it unreachable and will fail loudly if content growth ever
// pushes a category past the pocket capacity.

// ---- Remove -----------------------------------------------------------------

ZENITH_TEST(ZM_Bag, Remove_DecrementsWithoutErasingAboveZero)
{
	ZM_Bag xBag;
	xBag.Add(eBALL_LOW, 5u);
	ZENITH_ASSERT_TRUE(xBag.Remove(eBALL_LOW, 2u), "removing part of a stack succeeds");
	ZENITH_ASSERT_EQ(xBag.GetCount(eBALL_LOW), 3u, "the count dropped by exactly the amount");
	ZENITH_ASSERT_TRUE(xBag.Has(eBALL_LOW), "a partly-used stack is still held");
	ZENITH_ASSERT_EQ(xBag.PocketStackCount(PocketOf(eBALL_LOW)), 1u,
		"a stack above zero is never erased");
}

ZENITH_TEST(ZM_Bag, Remove_ErasesTheStackAtExactlyZero)
{
	// The pocket keeps a SURVIVING neighbour so the erase-then-shift path is what runs
	// (and so the count-0 walk below has live slots to inspect -- against an empty
	// pocket it would prove nothing).
	ZM_Bag xBag;
	xBag.Add(eBALL_LOW, 2u);
	xBag.Add(eBALL_HIGH, 4u);
	xBag.Add(eMEDICINE, 1u);

	ZENITH_ASSERT_TRUE(xBag.Remove(eBALL_LOW, 2u), "removing the whole stack succeeds");
	ZENITH_ASSERT_EQ(xBag.GetCount(eBALL_LOW), 0u, "nothing is held afterwards");
	ZENITH_ASSERT_FALSE(xBag.Has(eBALL_LOW), "Has is false once the stack is gone");

	const ZM_ITEM_CATEGORY ePocket = PocketOf(eBALL_LOW);
	ZENITH_ASSERT_EQ(xBag.PocketStackCount(ePocket), 1u,
		"the emptied stack went and the sibling stayed -- the pocket count dropped by exactly one");
	ZENITH_ASSERT_EQ(xBag.TotalStackCount(), 2u,
		"the surviving ball stack and the untouched medicine entry remain");
	ZENITH_ASSERT_EQ((u_int)xBag.PocketStack(ePocket, 0u).m_eItem, (u_int)eBALL_HIGH,
		"the erase shifted the surviving stack down into slot 0");
	ZENITH_ASSERT_EQ(xBag.PocketStack(ePocket, 0u).m_uCount, 4u,
		"the shifted stack kept its count");

	// SaveFormat module 6: a count-0 entry is NEVER written, so none may be stored.
	// This walks the LIVE slots, which is why the pocket was left non-empty above.
	for (u_int u = 0u; u < xBag.PocketStackCount(ePocket); ++u)
	{
		ZENITH_ASSERT_GT(xBag.PocketStack(ePocket, u).m_uCount, 0u,
			"no zero-count stack survives anywhere in the pocket");
		ZENITH_ASSERT_NE((u_int)xBag.PocketStack(ePocket, u).m_eItem, (u_int)eBALL_LOW,
			"the erased id never lingers in a live slot");
	}
}

ZENITH_TEST(ZM_Bag, Remove_RejectsTooMuchOutOfRangeAndZero)
{
	ZM_Bag xBag;
	xBag.Add(eBALL_LOW, 2u);

	ZENITH_ASSERT_FALSE(xBag.Remove(eBALL_LOW, 3u), "removing more than is held is rejected");
	ZENITH_ASSERT_EQ(xBag.GetCount(eBALL_LOW), 2u, "the over-remove left the stack whole");

	ZENITH_ASSERT_FALSE(xBag.Remove(eBALL_MID, 1u), "removing an unheld item is rejected");
	ZENITH_ASSERT_FALSE(xBag.Remove(ZM_ITEM_NONE, 1u), "removing NONE is rejected");
	ZENITH_ASSERT_FALSE(xBag.Remove(eOUT_OF_RANGE, 1u), "removing an out-of-range id is rejected");
	ZENITH_ASSERT_FALSE(xBag.Remove(eBALL_LOW, 0u), "removing zero is rejected");

	ZENITH_ASSERT_EQ(xBag.GetCount(eBALL_LOW), 2u, "no rejected remove mutated the bag");
	ZENITH_ASSERT_EQ(xBag.TotalStackCount(), 1u, "no rejected remove changed the entry count");
}

// ---- Pocket ordering --------------------------------------------------------

ZENITH_TEST(ZM_Bag, Pocket_StaysSortedAscendingThroughInsertAndErase)
{
	// The sort is what the SC6 Bag screen renders and what S7 writes as the module-6
	// entry order, so it is pinned end-to-end: inserted in a MIXED order, read back
	// ascending, and still ascending after a middle-stack erase shifts the tail down.
	//
	// The insertion order is deliberately MID -> HIGH -> LOW, NOT descending. A
	// strictly descending insert is useless here: an "always prepend" implementation
	// would also produce an ascending pocket, so the test could not tell the two
	// apart. Mixed order kills BOTH degenerate mutants -- always-prepend yields
	// {LOW, HIGH, MID} and always-append yields {MID, HIGH, LOW}, and each fails the
	// slot assertions below.
	ZM_Bag xBag;
	ZENITH_ASSERT_TRUE(xBag.Add(eBALL_MID, 2u), "the middle id is added first");
	ZENITH_ASSERT_TRUE(xBag.Add(eBALL_HIGH, 1u), "then the highest id (appends)");
	ZENITH_ASSERT_TRUE(xBag.Add(eBALL_LOW, 3u), "then the lowest id (inserts at the front)");

	const ZM_ITEM_CATEGORY ePocket = PocketOf(eBALL_LOW);
	ZENITH_ASSERT_EQ(xBag.PocketStackCount(ePocket), 3u, "all three distinct ids are stored");
	ZENITH_ASSERT_EQ((u_int)xBag.PocketStack(ePocket, 0u).m_eItem, (u_int)eBALL_LOW,
		"slot 0 is the LOWEST id despite being added last");
	ZENITH_ASSERT_EQ((u_int)xBag.PocketStack(ePocket, 1u).m_eItem, (u_int)eBALL_MID,
		"slot 1 is the middle id");
	ZENITH_ASSERT_EQ((u_int)xBag.PocketStack(ePocket, 2u).m_eItem, (u_int)eBALL_HIGH,
		"slot 2 is the highest id");
	ZENITH_ASSERT_EQ(xBag.PocketStack(ePocket, 0u).m_uCount, 3u,
		"the shifting insert carried each stack's count with it");
	ZENITH_ASSERT_EQ(xBag.PocketStack(ePocket, 2u).m_uCount, 1u,
		"the first-added stack kept its count after two shifts");

	// Erase the MIDDLE stack: the tail shifts down and the order survives.
	ZENITH_ASSERT_TRUE(xBag.Remove(eBALL_MID, 2u), "the middle stack is removed entirely");
	ZENITH_ASSERT_EQ(xBag.PocketStackCount(ePocket), 2u, "the pocket dropped to two stacks");
	ZENITH_ASSERT_EQ((u_int)xBag.PocketStack(ePocket, 0u).m_eItem, (u_int)eBALL_LOW,
		"slot 0 still holds the lowest id");
	ZENITH_ASSERT_EQ((u_int)xBag.PocketStack(ePocket, 1u).m_eItem, (u_int)eBALL_HIGH,
		"the erase shifted the tail down -- the remaining stacks are contiguous and ascending");
	ZENITH_ASSERT_EQ(xBag.PocketStack(ePocket, 1u).m_uCount, 1u,
		"the shifted stack kept its count");
}

ZENITH_TEST(ZM_Bag, Pockets_AreIndependent)
{
	ZM_Bag xBag;
	xBag.Add(eMEDICINE, 4u);
	xBag.Add(eBALL_LOW, 1u);

	const ZM_ITEM_CATEGORY eBallPocket = PocketOf(eBALL_LOW);
	const ZM_ITEM_CATEGORY eMedPocket  = PocketOf(eMEDICINE);
	ZENITH_ASSERT_NE((u_int)eBallPocket, (u_int)eMedPocket,
		"the fixtures deliberately live in different pockets");
	ZENITH_ASSERT_EQ(xBag.PocketStackCount(eBallPocket), 1u, "the ball pocket has one stack");
	ZENITH_ASSERT_EQ(xBag.PocketStackCount(eMedPocket), 1u, "the medicine pocket has one stack");
	ZENITH_ASSERT_EQ(xBag.TotalStackCount(), 2u, "two pockets, two save entries");

	xBag.Remove(eBALL_LOW, 1u);
	ZENITH_ASSERT_EQ(xBag.PocketStackCount(eBallPocket), 0u, "the ball stack was erased");
	ZENITH_ASSERT_EQ(xBag.GetCount(eMEDICINE), 4u,
		"emptying one pocket never disturbs another pocket's stack");
}

// ---- Bounds safety ----------------------------------------------------------

ZENITH_TEST(ZM_Bag, PocketAccessors_AreBoundsSafe)
{
	ZM_Bag xBag;
	xBag.Add(eBALL_LOW, 1u);

	const ZM_ITEM_CATEGORY eBadCategory = (ZM_ITEM_CATEGORY)((u_int)ZM_ITEM_CATEGORY_COUNT + 3u);
	ZENITH_ASSERT_EQ(xBag.PocketStackCount(eBadCategory), 0u,
		"an out-of-range category reports 0 stacks rather than reading out of bounds");

	const ZM_ItemStack& xFromBadCategory = xBag.PocketStack(eBadCategory, 0u);
	ZENITH_ASSERT_EQ((u_int)xFromBadCategory.m_eItem, (u_int)ZM_ITEM_NONE,
		"an out-of-range category yields the empty stack");
	ZENITH_ASSERT_EQ(xFromBadCategory.m_uCount, 0u, "the empty stack holds nothing");

	// Index 1 is past the single stored stack (index 0 is valid).
	const ZM_ItemStack& xFromBadIndex = xBag.PocketStack(PocketOf(eBALL_LOW), 1u);
	ZENITH_ASSERT_EQ((u_int)xFromBadIndex.m_eItem, (u_int)ZM_ITEM_NONE,
		"an index past the pocket's live count yields the empty stack");
	ZENITH_ASSERT_EQ(xFromBadIndex.m_uCount, 0u, "the empty stack holds nothing");
	ZENITH_ASSERT_TRUE(&xFromBadCategory == &xFromBadIndex,
		"both out-of-range reads return the SAME shared empty stack (never a temporary)");
}

// ---- Clear ------------------------------------------------------------------

ZENITH_TEST(ZM_Bag, Clear_EmptiesAPopulatedBag)
{
	ZM_Bag xBag;
	xBag.Add(eBALL_LOW, 5u);
	xBag.Add(eBALL_HIGH, 2u);
	xBag.Add(eMEDICINE, 3u);
	ZENITH_ASSERT_EQ(xBag.TotalStackCount(), 3u, "the fixture bag is populated");

	xBag.Clear();
	ZENITH_ASSERT_EQ(xBag.TotalStackCount(), 0u, "Clear leaves no save entries");
	for (u_int uCategory = 0u; uCategory < (u_int)ZM_ITEM_CATEGORY_COUNT; ++uCategory)
	{
		ZENITH_ASSERT_EQ(xBag.PocketStackCount((ZM_ITEM_CATEGORY)uCategory), 0u,
			"Clear emptied pocket %s", ZM_ItemCategoryToString((ZM_ITEM_CATEGORY)uCategory));
	}
	ZENITH_ASSERT_EQ(xBag.GetCount(eBALL_LOW), 0u, "Clear dropped the held counts");
	ZENITH_ASSERT_FALSE(xBag.Has(eMEDICINE), "nothing is held after Clear");

	ZENITH_ASSERT_TRUE(xBag.Add(eBALL_LOW, 1u), "a cleared bag is immediately reusable");
	ZENITH_ASSERT_EQ(xBag.GetCount(eBALL_LOW), 1u,
		"the re-add starts from zero -- Clear left no stale count behind");
}

// ---- Item-table invariants --------------------------------------------------

ZENITH_TEST(ZM_Bag, ItemTable_NoCategoryCanOverflowAPocket)
{
	// A pocket must hold EVERY item of its category at once. This walks the shipped
	// table and will fail loudly the moment S9/S10 content pushes a category past
	// uZM_BAG_MAX_STACKS_PER_POCKET -- the invariant that makes Add's "pocket full"
	// rejection unreachable in practice.
	u_int auPerCategory[ZM_ITEM_CATEGORY_COUNT] = {};
	for (u_int u = 0u; u < (u_int)ZM_ITEM_COUNT; ++u)
	{
		const u_int uCategory = (u_int)ZM_GetItemData((ZM_ITEM_ID)u).m_eCategory;
		ZENITH_ASSERT_LT(uCategory, (u_int)ZM_ITEM_CATEGORY_COUNT,
			"item %s has a category inside the enumerated range", ZM_GetItemName((ZM_ITEM_ID)u));
		++auPerCategory[uCategory];
	}

	for (u_int uCategory = 0u; uCategory < (u_int)ZM_ITEM_CATEGORY_COUNT; ++uCategory)
	{
		ZENITH_ASSERT_LE(auPerCategory[uCategory], uZM_BAG_MAX_STACKS_PER_POCKET,
			"pocket %s holds %u distinct items, within the %u-stack capacity",
			ZM_ItemCategoryToString((ZM_ITEM_CATEGORY)uCategory),
			auPerCategory[uCategory], uZM_BAG_MAX_STACKS_PER_POCKET);
	}
}

ZENITH_TEST(ZM_Bag, ItemTable_FitsTheSaveEntryCap)
{
	// One stack per distinct id, so the item count IS the worst-case module-6
	// entryCount; SaveFormat.md caps that at 512.
	ZENITH_ASSERT_LE((u_int)ZM_ITEM_COUNT, 512u,
		"SaveFormat module 6 rejects a bag with more than 512 entries");
	ZENITH_ASSERT_EQ(ZM_GetItemCount(), (u_int)ZM_ITEM_COUNT,
		"the table accessor agrees with the enum terminator");
}

// ---- Money ------------------------------------------------------------------

ZENITH_TEST(ZM_Bag, Money_AddAccumulates)
{
	ZM_GameState xState;
	ZENITH_ASSERT_EQ(xState.m_uMoney, 0u, "a default state starts broke");
	xState.AddMoney(500u);
	xState.AddMoney(250u);
	ZENITH_ASSERT_EQ(xState.m_uMoney, 750u, "credits accumulate");
	xState.AddMoney(0u);
	ZENITH_ASSERT_EQ(xState.m_uMoney, 750u, "a zero credit is a no-op");
}

ZENITH_TEST(ZM_Bag, Money_AddSaturatesAtTheCapWithoutWrapping)
{
	ZM_GameState xState;
	xState.AddMoney(uZM_MONEY_CAP - 10u);
	ZENITH_ASSERT_EQ(xState.m_uMoney, uZM_MONEY_CAP - 10u, "the state is seeded near the cap");

	// The wrap trap: m_uMoney + uAmount computed first would overflow a u_int and
	// clamp to a small number instead of the cap.
	xState.AddMoney(0xFFFFFFFFu);
	ZENITH_ASSERT_EQ(xState.m_uMoney, uZM_MONEY_CAP,
		"a huge credit saturates at exactly uZM_MONEY_CAP -- it never wraps");

	xState.AddMoney(1u);
	ZENITH_ASSERT_EQ(xState.m_uMoney, uZM_MONEY_CAP, "crediting a capped purse is a no-op");

	// m_uMoney is a public field with no write invariant, and S7's loader assigns it
	// straight from the module-7 uint32 -- an edited save can seat it ABOVE the cap.
	// Crediting must then be a no-op, NOT an underflowed headroom that wraps the purse.
	xState.m_uMoney = uZM_MONEY_CAP + 1000u;
	xState.AddMoney(0xFFFFFFFFu);
	ZENITH_ASSERT_EQ(xState.m_uMoney, uZM_MONEY_CAP + 1000u,
		"an over-cap balance credits nothing and never wraps (the headroom guard holds)");
}

ZENITH_TEST(ZM_Bag, Money_SpendDeductsOnSuccess)
{
	ZM_GameState xState;
	xState.AddMoney(1000u);
	ZENITH_ASSERT_TRUE(xState.SpendMoney(400u), "an affordable spend succeeds");
	ZENITH_ASSERT_EQ(xState.m_uMoney, 600u, "the balance dropped by exactly the amount");
}

ZENITH_TEST(ZM_Bag, Money_SpendRejectsUnaffordableAndAllowsExactBalance)
{
	ZM_GameState xState;
	xState.AddMoney(1000u);
	ZENITH_ASSERT_FALSE(xState.SpendMoney(1001u), "one over the balance is rejected");
	ZENITH_ASSERT_EQ(xState.m_uMoney, 1000u, "a rejected spend never mutates the balance");
	ZENITH_ASSERT_FALSE(xState.SpendMoney(0xFFFFFFFFu), "a huge spend is rejected, not wrapped");
	ZENITH_ASSERT_EQ(xState.m_uMoney, 1000u, "still untouched");

	ZENITH_ASSERT_TRUE(xState.SpendMoney(1000u), "spending EXACTLY the balance succeeds");
	ZENITH_ASSERT_EQ(xState.m_uMoney, 0u, "an exact spend leaves zero");
	ZENITH_ASSERT_FALSE(xState.SpendMoney(1u), "a broke purse cannot spend anything");
	ZENITH_ASSERT_EQ(xState.m_uMoney, 0u, "and never goes negative (it would wrap enormous)");
}

// ---- Starter seed -----------------------------------------------------------

ZENITH_TEST(ZM_Bag, Starter_SeedsTheEconomyAndKeepsTheOldGuarantees)
{
	const ZM_GameState xState = ZM_MakeStarterGameState();

	ZENITH_ASSERT_EQ(xState.m_uMoney, 3000u, "the starter purse is the ruled placeholder 3000");
	ZENITH_ASSERT_EQ(xState.m_xBag.GetCount(ZM_ITEM_CATCHORB), 5u, "five Catch Orbs to start");
	ZENITH_ASSERT_EQ(xState.m_xBag.GetCount(ZM_ITEM_SALVE), 3u, "three Salves to start");
	ZENITH_ASSERT_EQ(xState.m_xBag.TotalStackCount(), 2u,
		"those are the ONLY two starting stacks");

	// The pre-existing SC1 starter guarantees must survive the seed change.
	ZENITH_ASSERT_EQ(xState.m_xParty.Count(), 1u, "still exactly one starter party member");
	ZENITH_ASSERT_TRUE(xState.IsCaught(xState.m_xParty.Get(0u).m_eSpecies),
		"the starter's species is still marked caught");
	ZENITH_ASSERT_FALSE(xState.m_bPendingWhiteout, "still no pending whiteout");
}

ZENITH_TEST(ZM_Bag, Starter_ItemsLandInTheirTablePockets)
{
	const ZM_GameState xState = ZM_MakeStarterGameState();

	const ZM_ITEM_CATEGORY eBallPocket = PocketOf(ZM_ITEM_CATCHORB);
	const ZM_ITEM_CATEGORY eMedPocket  = PocketOf(ZM_ITEM_SALVE);
	ZENITH_ASSERT_EQ((u_int)eBallPocket, (u_int)ZM_ITEM_CATEGORY_BALL,
		"the Catch Orb is a BALL item in the table");
	ZENITH_ASSERT_EQ((u_int)eMedPocket, (u_int)ZM_ITEM_CATEGORY_MEDICINE,
		"the Salve is a MEDICINE item in the table");

	ZENITH_ASSERT_EQ(xState.m_xBag.PocketStackCount(eBallPocket), 1u,
		"one stack in the ball pocket");
	ZENITH_ASSERT_EQ((u_int)xState.m_xBag.PocketStack(eBallPocket, 0u).m_eItem,
		(u_int)ZM_ITEM_CATCHORB, "the ball pocket's stack is the Catch Orb");
	ZENITH_ASSERT_EQ(xState.m_xBag.PocketStackCount(eMedPocket), 1u,
		"one stack in the medicine pocket");
	ZENITH_ASSERT_EQ((u_int)xState.m_xBag.PocketStack(eMedPocket, 0u).m_eItem,
		(u_int)ZM_ITEM_SALVE, "the medicine pocket's stack is the Salve");
}

// ---- The SC7 transaction shape (no shop code involved) ----------------------

ZENITH_TEST(ZM_Bag, Transaction_BuyThenSellRoundTrip)
{
	// Locks the shape SC7's shop will wrap in guards: SPEND then Add for a buy,
	// Remove then AddMoney for a sell. Prices come from the table, never literals.
	const ZM_ITEM_ID eGoods = ZM_ITEM_TONIC;
	const u_int uBuy  = ZM_GetItemData(eGoods).m_uBuyPrice;
	const u_int uSell = ZM_GetItemData(eGoods).m_uSellPrice;
	ZENITH_ASSERT_GT(uBuy, 0u, "the fixture item is purchasable");
	ZENITH_ASSERT_LE(uSell, uBuy, "sell price never exceeds buy price (the table invariant)");

	ZM_GameState xState;
	xState.AddMoney(uBuy * 2u);
	const u_int uStartMoney = xState.m_uMoney;

	// Buy one.
	ZENITH_ASSERT_TRUE(xState.SpendMoney(uBuy), "the purse covers one unit");
	ZENITH_ASSERT_TRUE(xState.m_xBag.Add(eGoods, 1u), "the bought unit goes into the bag");
	ZENITH_ASSERT_EQ(xState.m_uMoney, uStartMoney - uBuy, "the buy debited exactly the price");
	ZENITH_ASSERT_EQ(xState.m_xBag.GetCount(eGoods), 1u, "the bag holds the bought unit");

	// Sell it back.
	ZENITH_ASSERT_TRUE(xState.m_xBag.Remove(eGoods, 1u), "the sold unit leaves the bag");
	xState.AddMoney(uSell);
	ZENITH_ASSERT_EQ(xState.m_uMoney, uStartMoney - uBuy + uSell,
		"the sell credited exactly the sell price");
	ZENITH_ASSERT_EQ(xState.m_xBag.GetCount(eGoods), 0u, "the bag is back to holding none");
	ZENITH_ASSERT_EQ(xState.m_xBag.TotalStackCount(), 0u,
		"the emptied stack was erased, not left as a count-0 entry");

	// An unaffordable buy must leave BOTH sides untouched -- the failure mode the
	// all-or-nothing contract exists to prevent.
	const u_int uBeforeFailedBuy = xState.m_uMoney;
	ZENITH_ASSERT_FALSE(xState.SpendMoney(uZM_MONEY_CAP),
		"a purchase beyond the purse is rejected before anything is added");
	ZENITH_ASSERT_EQ(xState.m_uMoney, uBeforeFailedBuy, "the failed buy kept the purse intact");
	ZENITH_ASSERT_EQ(xState.m_xBag.TotalStackCount(), 0u, "and added nothing to the bag");
}
