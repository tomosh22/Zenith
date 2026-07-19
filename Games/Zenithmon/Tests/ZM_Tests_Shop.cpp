#include "Zenith.h"

// ============================================================================
// ZM_Tests_Shop -- S6 item 2 (SC7) unit tests for the shop: the PURE transaction
// rules (ZM_ShopLogic buy / sell), the ZM_Bag::CanAdd predicate the buy ordering
// rests on, and the ZM_UI_Shop presenter's pure surface (row names, paging, the
// quantity clamp, the formatters, the all-or-nothing inventory, the confirm-by-name
// dispatch and the headless half of the presentation seam).
//
// Everything here is PURE -- no ECS, no scene, no graphics, no baked assets -- so
// every fixture is deterministic and hermetic and no RequestSkip is needed. Category
// ZM_Shop.
//
// Every bag fixture is built through the REAL ZM_Bag::Add (never by poking the pocket
// arrays) and every price is read out of the ITEM TABLE (never spelled as a literal),
// so a re-tune of the economy re-tunes these tests with it instead of breaking them.
// The load-bearing assertions are the ATOMICITY ones: on every refusal the money and
// the bag must be EXACTLY unchanged, because a shop that takes money without
// delivering goods is the one bug in this feature the player can never undo.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "ZenithECS/Zenith_Entity.h"                    // the invalid-handle presentation guard
#include "Zenithmon/Components/ZM_UI_MenuStack.h"       // the host's ROOT names (near-miss fixtures)
#include "Zenithmon/Source/Data/ZM_ItemData.h"
#include "Zenithmon/Source/Party/ZM_Bag.h"
#include "Zenithmon/Source/Party/ZM_GameState.h"        // ZM_MakeStarterGameState + uZM_MONEY_CAP
#include "Zenithmon/Source/Shop/ZM_ShopLogic.h"
#include "Zenithmon/Source/UI/ZM_UI_Bag.h"              // a bag ROW name (near-miss fixture)
#include "Zenithmon/Source/UI/ZM_UI_Shop.h"

#include <string>

namespace
{
	// The u_int ceiling, for the overflow fixtures (the logic guards against exactly
	// this, so the test has to be able to name it).
	constexpr u_int uTEST_UINT_MAX = static_cast<u_int>(-1);

	bool Contains(const std::string& strBody, const char* szNeedle)
	{
		return strBody.find(szNeedle) != std::string::npos;
	}

	// Bit-for-bit bag comparison -- what "NOTHING was mutated" actually means. Compares
	// every pocket's stack COUNT and every stored stack, so a shifted / resized / erased
	// entry anywhere shows up.
	bool BagsMatch(const ZM_Bag& xLeft, const ZM_Bag& xRight)
	{
		for (u_int uCategory = 0u; uCategory < (u_int)ZM_ITEM_CATEGORY_COUNT; ++uCategory)
		{
			const ZM_ITEM_CATEGORY eCategory = (ZM_ITEM_CATEGORY)uCategory;
			const u_int uStacks = xLeft.PocketStackCount(eCategory);
			if (uStacks != xRight.PocketStackCount(eCategory))
			{
				return false;
			}
			for (u_int uSlot = 0u; uSlot < uStacks; ++uSlot)
			{
				const ZM_ItemStack& xL = xLeft.PocketStack(eCategory, uSlot);
				const ZM_ItemStack& xR = xRight.PocketStack(eCategory, uSlot);
				if (xL.m_eItem != xR.m_eItem || xL.m_uCount != xR.m_uCount)
				{
					return false;
				}
			}
		}
		return true;
	}

	// The FIRST item the table prices at 0 to buy, found by WALKING the table -- never a
	// hard-coded id, so re-pricing an item cannot leave this fixture asserting against a
	// row that is now purchasable. ZM_ITEM_NONE when every item is purchasable.
	ZM_ITEM_ID FindUnpurchasableItem()
	{
		for (u_int u = 0u; u < (u_int)ZM_ITEM_COUNT; ++u)
		{
			const ZM_ITEM_ID eItem = (ZM_ITEM_ID)u;
			if (ZM_GetItemData(eItem).m_uBuyPrice == 0u)
			{
				return eItem;
			}
		}
		return ZM_ITEM_NONE;
	}

	// ...and the first the table will not buy back.
	ZM_ITEM_ID FindUnsellableItem()
	{
		for (u_int u = 0u; u < (u_int)ZM_ITEM_COUNT; ++u)
		{
			const ZM_ITEM_ID eItem = (ZM_ITEM_ID)u;
			if (ZM_GetItemData(eItem).m_uSellPrice == 0u)
			{
				return eItem;
			}
		}
		return ZM_ITEM_NONE;
	}

	// The first KEY-category item, whatever the table's key block happens to hold. The
	// key items are the concrete exploit the NOT_PURCHASABLE guard exists for.
	ZM_ITEM_ID FindKeyItem()
	{
		for (u_int u = 0u; u < (u_int)ZM_ITEM_COUNT; ++u)
		{
			const ZM_ITEM_ID eItem = (ZM_ITEM_ID)u;
			if (ZM_GetItemData(eItem).m_eCategory == ZM_ITEM_CATEGORY_KEY)
			{
				return eItem;
			}
		}
		return ZM_ITEM_NONE;
	}
}

// ============================================================================
// PART 1 -- ZM_Bag::CanAdd (the predicate the whole buy ordering rests on)
// ============================================================================

ZENITH_TEST(ZM_Shop, CanAdd_AgreesWithAddOnAcceptAndReject)
{
	// A NEW stack.
	{
		ZM_Bag xBag;
		xBag.Clear();
		ZENITH_ASSERT_TRUE(xBag.CanAdd(ZM_ITEM_CATCHORB, 1u), "an empty bag can take a new stack");
		ZENITH_ASSERT_TRUE(xBag.Add(ZM_ITEM_CATCHORB, 1u), "...and Add agrees");
	}
	// An EXISTING stack with room.
	{
		ZM_Bag xBag;
		xBag.Clear();
		ZENITH_ASSERT_TRUE(xBag.Add(ZM_ITEM_CATCHORB, 1u), "seed one");
		ZENITH_ASSERT_TRUE(xBag.CanAdd(ZM_ITEM_CATCHORB, 5u), "an existing stack with room can grow");
		ZENITH_ASSERT_TRUE(xBag.Add(ZM_ITEM_CATCHORB, 5u), "...and Add agrees");
		ZENITH_ASSERT_EQ(xBag.GetCount(ZM_ITEM_CATCHORB), 6u, "...by exactly the added count");
	}
	// An OVER-CAP add, from a stack already at the cap.
	{
		ZM_Bag xBag;
		xBag.Clear();
		ZENITH_ASSERT_TRUE(xBag.Add(ZM_ITEM_CATCHORB, uZM_BAG_MAX_STACK_COUNT),
			"seed a stack at the per-stack cap");
		ZENITH_ASSERT_FALSE(xBag.CanAdd(ZM_ITEM_CATCHORB, 1u),
			"a capped stack cannot take one more");
		ZENITH_ASSERT_FALSE(xBag.Add(ZM_ITEM_CATCHORB, 1u), "...and Add agrees");
		ZENITH_ASSERT_EQ(xBag.GetCount(ZM_ITEM_CATCHORB), uZM_BAG_MAX_STACK_COUNT,
			"...leaving the stack exactly at the cap");
	}
	// An INVALID id and a ZERO count.
	{
		ZM_Bag xBag;
		xBag.Clear();
		ZENITH_ASSERT_FALSE(xBag.CanAdd(ZM_ITEM_NONE, 1u),
			"ZM_ITEM_NONE is not addable (it IS ZM_ITEM_COUNT -- one range check covers both)");
		ZENITH_ASSERT_FALSE(xBag.Add(ZM_ITEM_NONE, 1u), "...and Add agrees");
		ZENITH_ASSERT_FALSE(xBag.CanAdd(ZM_ITEM_CATCHORB, 0u), "a zero count is not addable");
		ZENITH_ASSERT_FALSE(xBag.Add(ZM_ITEM_CATCHORB, 0u), "...and Add agrees");
		ZENITH_ASSERT_EQ(xBag.TotalStackCount(), 0u, "...and nothing was stored");
	}
}

ZENITH_TEST(ZM_Shop, CanAdd_DoesNotMutateTheBag)
{
	ZM_Bag xBag;
	xBag.Clear();
	ZENITH_ASSERT_TRUE(xBag.Add(ZM_ITEM_CATCHORB, 3u), "seed the ball pocket");
	ZENITH_ASSERT_TRUE(xBag.Add(ZM_ITEM_SALVE, 2u), "...and the medicine pocket");

	const ZM_Bag xBefore = xBag;
	ZENITH_ASSERT_TRUE(BagsMatch(xBag, xBefore), "the snapshot really is a copy of the bag");

	// Every branch of the predicate, on the SAME bag: accepted, capped, invalid, zero.
	(void)xBag.CanAdd(ZM_ITEM_CATCHORB, 1u);
	(void)xBag.CanAdd(ZM_ITEM_GREATORB, 1u);
	(void)xBag.CanAdd(ZM_ITEM_CATCHORB, uZM_BAG_MAX_STACK_COUNT);
	(void)xBag.CanAdd(ZM_ITEM_NONE, 1u);
	(void)xBag.CanAdd(ZM_ITEM_CATCHORB, 0u);

	ZENITH_ASSERT_TRUE(BagsMatch(xBag, xBefore),
		"CanAdd is a PREDICATE -- asking it anything must leave the bag bit-identical");
}

// ============================================================================
// PART 2 -- ZM_ShopLogic prices
// ============================================================================

ZENITH_TEST(ZM_Shop, Prices_ComeFromTheItemTable)
{
	for (u_int u = 0u; u < (u_int)ZM_ITEM_COUNT; ++u)
	{
		const ZM_ITEM_ID eItem = (ZM_ITEM_ID)u;
		ZENITH_ASSERT_EQ(ZM_ShopBuyPrice(eItem), ZM_GetItemData(eItem).m_uBuyPrice,
			"item %u's buy price is the table's", u);
		ZENITH_ASSERT_EQ(ZM_ShopSellPrice(eItem), ZM_GetItemData(eItem).m_uSellPrice,
			"item %u's sell price is the table's", u);
	}
	ZENITH_ASSERT_EQ(ZM_ShopBuyPrice(ZM_ITEM_NONE), 0u,
		"an out-of-range id has no buy price (and never indexes the table)");
	ZENITH_ASSERT_EQ(ZM_ShopSellPrice(ZM_ITEM_NONE), 0u, "...nor a sell price");
	ZENITH_ASSERT_EQ(ZM_ShopBuyPrice((ZM_ITEM_ID)((u_int)ZM_ITEM_COUNT + 500u)), 0u,
		"...however far out of range");
}

ZENITH_TEST(ZM_Shop, KeyItems_AreUnpricedInBothDirections)
{
	// The concrete exploit the NOT_PURCHASABLE / NOT_SELLABLE guards exist for: if a key
	// item ever gained a price, a mart stocking it would sell progression outright.
	const ZM_ITEM_ID eKey = FindKeyItem();
	ZENITH_ASSERT_TRUE(eKey != ZM_ITEM_NONE,
		"the table must hold at least one KEY item or the guard tests are vacuous");
	for (u_int u = 0u; u < (u_int)ZM_ITEM_COUNT; ++u)
	{
		const ZM_ITEM_ID eItem = (ZM_ITEM_ID)u;
		if (ZM_GetItemData(eItem).m_eCategory != ZM_ITEM_CATEGORY_KEY)
		{
			continue;
		}
		ZENITH_ASSERT_EQ(ZM_ShopBuyPrice(eItem), 0u,
			"key item '%s' must not be purchasable", ZM_GetItemName(eItem));
		ZENITH_ASSERT_EQ(ZM_ShopSellPrice(eItem), 0u,
			"key item '%s' must not be sellable", ZM_GetItemName(eItem));
	}
}

// ============================================================================
// PART 3 -- ZM_ShopBuy
// ============================================================================

ZENITH_TEST(ZM_Shop, Buy_DebitsAndCreditsExactly)
{
	ZM_Bag xBag;
	xBag.Clear();
	u_int uMoney = 10000u;

	const u_int uPrice = ZM_ShopBuyPrice(ZM_ITEM_CATCHORB);
	ZENITH_ASSERT_GT(uPrice, 0u, "the fixture item must be purchasable or the test is vacuous");

	ZENITH_ASSERT_TRUE(ZM_ShopBuy(xBag, uMoney, ZM_ITEM_CATCHORB, 3u) == ZM_SHOP_OK,
		"a funded, roomy purchase is accepted");
	ZENITH_ASSERT_EQ(uMoney, 10000u - uPrice * 3u,
		"the purse is debited by price x quantity, from the TABLE price");
	ZENITH_ASSERT_EQ(xBag.GetCount(ZM_ITEM_CATCHORB), 3u, "...and the bag holds exactly the goods");

	// A second purchase stacks rather than opening a second entry.
	ZENITH_ASSERT_TRUE(ZM_ShopBuy(xBag, uMoney, ZM_ITEM_CATCHORB, 2u) == ZM_SHOP_OK,
		"a second purchase of the same item is accepted");
	ZENITH_ASSERT_EQ(xBag.GetCount(ZM_ITEM_CATCHORB), 5u, "...stacking onto the existing entry");
	ZENITH_ASSERT_EQ(xBag.PocketStackCount(ZM_GetItemData(ZM_ITEM_CATCHORB).m_eCategory), 1u,
		"...as ONE stack, not two");
	ZENITH_ASSERT_EQ(uMoney, 10000u - uPrice * 5u, "...and the purse tracks the whole spend");
}

ZENITH_TEST(ZM_Shop, Buy_RejectsAnInvalidItemOrQuantityWithoutMutating)
{
	ZM_Bag xBag;
	xBag.Clear();
	ZENITH_ASSERT_TRUE(xBag.Add(ZM_ITEM_CATCHORB, 1u), "seed something to notice a mutation in");
	const ZM_Bag xBefore = xBag;
	u_int uMoney = 5000u;

	ZENITH_ASSERT_TRUE(ZM_ShopBuy(xBag, uMoney, ZM_ITEM_NONE, 1u) == ZM_SHOP_ERR_INVALID_ITEM,
		"ZM_ITEM_NONE is not buyable (it IS ZM_ITEM_COUNT -- one range check)");
	ZENITH_ASSERT_TRUE(
		ZM_ShopBuy(xBag, uMoney, (ZM_ITEM_ID)((u_int)ZM_ITEM_COUNT + 7u), 1u) == ZM_SHOP_ERR_INVALID_ITEM,
		"...and so is any id past the end of the table");
	ZENITH_ASSERT_TRUE(ZM_ShopBuy(xBag, uMoney, ZM_ITEM_CATCHORB, 0u) == ZM_SHOP_ERR_INVALID_QUANTITY,
		"a zero quantity is refused");

	ZENITH_ASSERT_EQ(uMoney, 5000u, "no refused purchase touched the purse");
	ZENITH_ASSERT_TRUE(BagsMatch(xBag, xBefore), "...nor the bag");
}

ZENITH_TEST(ZM_Shop, Buy_RefusesAnItemThePriceTableWillNotSell)
{
	// Found by WALKING the table, never hard-coded: this guard is what stops a mart that
	// stocks a key item from handing it out FREE (every key item is priced 0).
	const ZM_ITEM_ID eFree = FindUnpurchasableItem();
	ZENITH_ASSERT_TRUE(eFree != ZM_ITEM_NONE,
		"the table must price at least one item at 0 or this guard is untestable");
	ZENITH_ASSERT_EQ(ZM_ShopBuyPrice(eFree), 0u, "the fixture item really is unpurchasable");

	ZM_Bag xBag;
	xBag.Clear();
	const ZM_Bag xBefore = xBag;
	u_int uMoney = 999999u;

	ZENITH_ASSERT_TRUE(ZM_ShopBuy(xBag, uMoney, eFree, 1u) == ZM_SHOP_ERR_NOT_PURCHASABLE,
		"'%s' is priced 0 and must NOT be purchasable", ZM_GetItemName(eFree));
	ZENITH_ASSERT_EQ(uMoney, 999999u, "...and a rich player still pays nothing for nothing");
	ZENITH_ASSERT_TRUE(BagsMatch(xBag, xBefore),
		"...and above all NEVER receives it: a price of 0 must not read as FREE");

	// The same guard, spelled against the KEY category the exploit actually lives in.
	const ZM_ITEM_ID eKey = FindKeyItem();
	if (eKey != ZM_ITEM_NONE)
	{
		ZENITH_ASSERT_TRUE(ZM_ShopBuy(xBag, uMoney, eKey, 1u) == ZM_SHOP_ERR_NOT_PURCHASABLE,
			"the key item '%s' cannot be bought", ZM_GetItemName(eKey));
		ZENITH_ASSERT_EQ(xBag.GetCount(eKey), 0u, "...and is not delivered");
	}
}

ZENITH_TEST(ZM_Shop, Buy_RefusesWhatThePlayerCannotAffordWithoutMutating)
{
	ZM_Bag xBag;
	xBag.Clear();
	const ZM_Bag xBefore = xBag;

	const u_int uPrice = ZM_ShopBuyPrice(ZM_ITEM_CATCHORB);
	ZENITH_ASSERT_GT(uPrice, 1u, "the fixture item needs a real price for the boundary below");

	u_int uMoney = uPrice - 1u;   // one short
	ZENITH_ASSERT_TRUE(ZM_ShopBuy(xBag, uMoney, ZM_ITEM_CATCHORB, 1u) == ZM_SHOP_ERR_CANNOT_AFFORD,
		"one short of the price is refused");
	ZENITH_ASSERT_EQ(uMoney, uPrice - 1u, "...with the purse EXACTLY untouched");
	ZENITH_ASSERT_TRUE(BagsMatch(xBag, xBefore), "...and no goods delivered");

	// Exactly enough IS enough (the boundary is <=, not <).
	uMoney = uPrice;
	ZENITH_ASSERT_TRUE(ZM_ShopBuy(xBag, uMoney, ZM_ITEM_CATCHORB, 1u) == ZM_SHOP_OK,
		"exactly the price is affordable");
	ZENITH_ASSERT_EQ(uMoney, 0u, "...spending the purse to zero");
	ZENITH_ASSERT_EQ(xBag.GetCount(ZM_ITEM_CATCHORB), 1u, "...and delivering the item");

	// ...and a bulk order the purse cannot cover is refused whole, not part-filled.
	uMoney = uPrice * 2u;
	ZENITH_ASSERT_TRUE(ZM_ShopBuy(xBag, uMoney, ZM_ITEM_CATCHORB, 3u) == ZM_SHOP_ERR_CANNOT_AFFORD,
		"a bulk order past the purse is refused");
	ZENITH_ASSERT_EQ(uMoney, uPrice * 2u, "...whole, with nothing deducted");
	ZENITH_ASSERT_EQ(xBag.GetCount(ZM_ITEM_CATCHORB), 1u, "...and nothing part-delivered");
}

ZENITH_TEST(ZM_Shop, Buy_AnOverflowingTotalIsNeverAFreePurchase)
{
	// price * qty is checked BEFORE the multiply. Without that check the product wraps a
	// u_int and a wildly over-priced order reads as costing a handful of coins -- the
	// classic "buy 21474837 orbs for 104" exploit.
	const u_int uPrice = ZM_ShopBuyPrice(ZM_ITEM_CATCHORB);
	ZENITH_ASSERT_GT(uPrice, 1u, "the fixture item needs a real price to overflow with");

	const u_int uWrappingQty = uTEST_UINT_MAX / uPrice + 1u;
	ZENITH_ASSERT_LT(uPrice * uWrappingQty, uPrice,
		"the fixture quantity really does WRAP the product (that is the whole hazard)");

	ZM_Bag xBag;
	xBag.Clear();
	const ZM_Bag xBefore = xBag;
	u_int uMoney = 3000u;

	ZENITH_ASSERT_TRUE(ZM_ShopBuy(xBag, uMoney, ZM_ITEM_CATCHORB, uWrappingQty) == ZM_SHOP_ERR_CANNOT_AFFORD,
		"an order whose total cannot even be represented is unaffordable, not cheap");
	ZENITH_ASSERT_EQ(uMoney, 3000u, "...with the purse untouched");
	ZENITH_ASSERT_TRUE(BagsMatch(xBag, xBefore), "...and nothing delivered");
}

ZENITH_TEST(ZM_Shop, Buy_TakesNoMoneyWhenTheBagCannotHoldTheGoods)
{
	// THE ATOMICITY GUARANTEE. ZM_Bag::Add refuses a stack past the per-stack cap, so the
	// naive "SpendMoney then Add" order would debit the player and deliver nothing. The
	// buy asks CanAdd FIRST; this test is what pins that ordering.
	ZM_Bag xBag;
	xBag.Clear();
	ZENITH_ASSERT_TRUE(xBag.Add(ZM_ITEM_CATCHORB, uZM_BAG_MAX_STACK_COUNT),
		"seed the stack at the per-stack cap");
	const ZM_Bag xBefore = xBag;

	const u_int uPrice = ZM_ShopBuyPrice(ZM_ITEM_CATCHORB);
	u_int uMoney = uPrice * 10u;   // richly funded: only the BAG can refuse this

	ZENITH_ASSERT_TRUE(ZM_ShopBuy(xBag, uMoney, ZM_ITEM_CATCHORB, 1u) == ZM_SHOP_ERR_NO_BAG_ROOM,
		"a capped stack refuses the purchase");
	ZENITH_ASSERT_EQ(uMoney, uPrice * 10u,
		"...and the purse is EXACTLY unchanged -- money must never be taken for goods that "
		"cannot be delivered");
	ZENITH_ASSERT_TRUE(BagsMatch(xBag, xBefore), "...and the bag is bit-identical too");

	// The refusal is about ROOM, not about the item: a different item still sells.
	ZENITH_ASSERT_TRUE(ZM_ShopBuy(xBag, uMoney, ZM_ITEM_SALVE, 1u) == ZM_SHOP_OK,
		"a roomy item is still purchasable from the same shop");
	ZENITH_ASSERT_EQ(xBag.GetCount(ZM_ITEM_SALVE), 1u, "...and is delivered");
}

// ============================================================================
// PART 4 -- ZM_ShopSell
// ============================================================================

ZENITH_TEST(ZM_Shop, Sell_CreditsExactlyAndErasesTheEmptiedStack)
{
	ZM_Bag xBag;
	xBag.Clear();
	ZENITH_ASSERT_TRUE(xBag.Add(ZM_ITEM_CATCHORB, 3u), "seed three");
	u_int uMoney = 0u;

	const u_int uPrice = ZM_ShopSellPrice(ZM_ITEM_CATCHORB);
	ZENITH_ASSERT_GT(uPrice, 0u, "the fixture item must be sellable or the test is vacuous");

	ZENITH_ASSERT_TRUE(ZM_ShopSell(xBag, uMoney, ZM_ITEM_CATCHORB, 2u) == ZM_SHOP_OK,
		"selling what is held is accepted");
	ZENITH_ASSERT_EQ(uMoney, uPrice * 2u, "the purse is credited by sell price x quantity");
	ZENITH_ASSERT_EQ(xBag.GetCount(ZM_ITEM_CATCHORB), 1u, "...and the stack shrinks by exactly that");

	const ZM_ITEM_CATEGORY ePocket = ZM_GetItemData(ZM_ITEM_CATCHORB).m_eCategory;
	ZENITH_ASSERT_EQ(xBag.PocketStackCount(ePocket), 1u, "the stack is still stored while non-empty");

	ZENITH_ASSERT_TRUE(ZM_ShopSell(xBag, uMoney, ZM_ITEM_CATCHORB, 1u) == ZM_SHOP_OK,
		"selling the last one is accepted");
	ZENITH_ASSERT_EQ(uMoney, uPrice * 3u, "...and paid for");
	ZENITH_ASSERT_EQ(xBag.GetCount(ZM_ITEM_CATCHORB), 0u, "...leaving none held");
	ZENITH_ASSERT_EQ(xBag.PocketStackCount(ePocket), 0u,
		"...and the emptied stack is ERASED, not parked at count 0");
}

ZENITH_TEST(ZM_Shop, Sell_RefusesMoreThanIsHeldWithoutMutating)
{
	ZM_Bag xBag;
	xBag.Clear();
	ZENITH_ASSERT_TRUE(xBag.Add(ZM_ITEM_CATCHORB, 2u), "seed two");
	const ZM_Bag xBefore = xBag;
	u_int uMoney = 500u;

	ZENITH_ASSERT_TRUE(ZM_ShopSell(xBag, uMoney, ZM_ITEM_CATCHORB, 3u) == ZM_SHOP_ERR_NOT_ENOUGH_HELD,
		"selling more than is held is refused");
	ZENITH_ASSERT_TRUE(ZM_ShopSell(xBag, uMoney, ZM_ITEM_GREATORB, 1u) == ZM_SHOP_ERR_NOT_ENOUGH_HELD,
		"...as is selling something not held at all");
	ZENITH_ASSERT_EQ(uMoney, 500u, "no refused sale credited the purse");
	ZENITH_ASSERT_TRUE(BagsMatch(xBag, xBefore), "...nor consumed the items");

	// The boundary: exactly what is held IS sellable.
	ZENITH_ASSERT_TRUE(ZM_ShopSell(xBag, uMoney, ZM_ITEM_CATCHORB, 2u) == ZM_SHOP_OK,
		"selling exactly the held count is accepted");
	ZENITH_ASSERT_EQ(xBag.GetCount(ZM_ITEM_CATCHORB), 0u, "...and takes them all");
}

ZENITH_TEST(ZM_Shop, Sell_RefusesAnItemTheShopWillNotBuy)
{
	// Found by WALKING the table: this is the guard that protects the key items from
	// being cashed in for progression-breaking money.
	const ZM_ITEM_ID eUnsellable = FindUnsellableItem();
	ZENITH_ASSERT_TRUE(eUnsellable != ZM_ITEM_NONE,
		"the table must price at least one item's sale at 0 or this guard is untestable");
	ZENITH_ASSERT_EQ(ZM_ShopSellPrice(eUnsellable), 0u, "the fixture item really is unsellable");

	ZM_Bag xBag;
	xBag.Clear();
	ZENITH_ASSERT_TRUE(xBag.Add(eUnsellable, 2u),
		"the player is HOLDING it -- the refusal must come from the price, not from the bag");
	const ZM_Bag xBefore = xBag;
	u_int uMoney = 0u;

	ZENITH_ASSERT_TRUE(ZM_ShopSell(xBag, uMoney, eUnsellable, 1u) == ZM_SHOP_ERR_NOT_SELLABLE,
		"'%s' fetches nothing and must not be sellable", ZM_GetItemName(eUnsellable));
	ZENITH_ASSERT_EQ(uMoney, 0u, "...paying nothing");
	ZENITH_ASSERT_TRUE(BagsMatch(xBag, xBefore),
		"...and above all NOT consuming the item for free");
}

ZENITH_TEST(ZM_Shop, Sell_RejectsAnInvalidItemOrQuantityWithoutMutating)
{
	ZM_Bag xBag;
	xBag.Clear();
	ZENITH_ASSERT_TRUE(xBag.Add(ZM_ITEM_CATCHORB, 2u), "seed something to notice a mutation in");
	const ZM_Bag xBefore = xBag;
	u_int uMoney = 250u;

	ZENITH_ASSERT_TRUE(ZM_ShopSell(xBag, uMoney, ZM_ITEM_NONE, 1u) == ZM_SHOP_ERR_INVALID_ITEM,
		"ZM_ITEM_NONE is not sellable");
	ZENITH_ASSERT_TRUE(
		ZM_ShopSell(xBag, uMoney, (ZM_ITEM_ID)((u_int)ZM_ITEM_COUNT + 3u), 1u) == ZM_SHOP_ERR_INVALID_ITEM,
		"...nor any id past the end of the table");
	ZENITH_ASSERT_TRUE(ZM_ShopSell(xBag, uMoney, ZM_ITEM_CATCHORB, 0u) == ZM_SHOP_ERR_INVALID_QUANTITY,
		"a zero quantity is refused");

	ZENITH_ASSERT_EQ(uMoney, 250u, "no refused sale touched the purse");
	ZENITH_ASSERT_TRUE(BagsMatch(xBag, xBefore), "...nor the bag");
}

ZENITH_TEST(ZM_Shop, Sell_RefusesWhenTheCreditWouldBeLostAtTheMoneyCap)
{
	// A purse at the cap would SWALLOW the payment while the goods were gone. The rule is
	// strict: unless the FULL credit fits, the sale is refused and nothing moves.
	ZM_Bag xBag;
	xBag.Clear();
	ZENITH_ASSERT_TRUE(xBag.Add(ZM_ITEM_CATCHORB, 2u), "seed two sellable items");
	const ZM_Bag xBefore = xBag;

	const u_int uPrice = ZM_ShopSellPrice(ZM_ITEM_CATCHORB);
	ZENITH_ASSERT_GT(uPrice, 1u, "the fixture item needs a real sell price for the boundary below");

	u_int uMoney = uZM_MONEY_CAP;
	ZENITH_ASSERT_TRUE(ZM_ShopSell(xBag, uMoney, ZM_ITEM_CATCHORB, 1u) == ZM_SHOP_ERR_MONEY_CAPPED,
		"a capped purse refuses the sale");
	ZENITH_ASSERT_EQ(uMoney, uZM_MONEY_CAP, "...leaving the purse at the cap");
	ZENITH_ASSERT_TRUE(BagsMatch(xBag, xBefore),
		"...and the item is NOT consumed -- a swallowed credit would be an unrecoverable loss");

	// A PARTIAL fit is still a refusal (one coin short of room for the whole credit).
	uMoney = uZM_MONEY_CAP - (uPrice - 1u);
	ZENITH_ASSERT_TRUE(ZM_ShopSell(xBag, uMoney, ZM_ITEM_CATCHORB, 1u) == ZM_SHOP_ERR_MONEY_CAPPED,
		"a credit that only PARTLY fits is refused too");
	ZENITH_ASSERT_EQ(uMoney, uZM_MONEY_CAP - (uPrice - 1u), "...with the purse untouched");
	ZENITH_ASSERT_TRUE(BagsMatch(xBag, xBefore), "...and the items still held");

	// ...and an EXACT fit is accepted (the boundary is >, not >=).
	uMoney = uZM_MONEY_CAP - uPrice;
	ZENITH_ASSERT_TRUE(ZM_ShopSell(xBag, uMoney, ZM_ITEM_CATCHORB, 1u) == ZM_SHOP_OK,
		"a credit that fits EXACTLY is accepted");
	ZENITH_ASSERT_EQ(uMoney, uZM_MONEY_CAP, "...landing the purse on the cap");
	ZENITH_ASSERT_EQ(xBag.GetCount(ZM_ITEM_CATCHORB), 1u, "...and taking exactly one item");
}

ZENITH_TEST(ZM_Shop, Sell_AnOverflowingCreditIsRefusedNotWrapped)
{
	// The mirror of the buy-side overflow guard. A quantity that wraps price * qty must
	// not produce a small credit (nor, worse, a wrapped purse).
	const u_int uPrice = ZM_ShopSellPrice(ZM_ITEM_CATCHORB);
	ZENITH_ASSERT_GT(uPrice, 1u, "the fixture item needs a real sell price to overflow with");
	const u_int uWrappingQty = uTEST_UINT_MAX / uPrice + 1u;

	ZM_Bag xBag;
	xBag.Clear();
	ZENITH_ASSERT_TRUE(xBag.Add(ZM_ITEM_CATCHORB, 1u), "seed one");
	const ZM_Bag xBefore = xBag;
	u_int uMoney = 0u;

	// The held-count guard fires first for a quantity this large, which is itself the
	// point: NO ordering of the guards may let a wrapped credit through.
	const ZM_SHOP_RESULT eResult = ZM_ShopSell(xBag, uMoney, ZM_ITEM_CATCHORB, uWrappingQty);
	ZENITH_ASSERT_TRUE(eResult != ZM_SHOP_OK,
		"a credit that cannot be represented is never a sale (result %u)", (u_int)eResult);
	ZENITH_ASSERT_EQ(uMoney, 0u, "...and pays nothing");
	ZENITH_ASSERT_TRUE(BagsMatch(xBag, xBefore), "...and consumes nothing");
}

ZENITH_TEST(ZM_Shop, BuyThenSell_RoundTripsAgainstTheStarterState)
{
	// End to end on the REAL starter fixture: the screen the player actually opens.
	ZM_GameState xState = ZM_MakeStarterGameState();
	const u_int uMoneyBefore = xState.m_uMoney;
	const u_int uOrbsBefore = xState.m_xBag.GetCount(ZM_ITEM_CATCHORB);
	ZENITH_ASSERT_GT(uMoneyBefore, 0u, "the starter seeds money or this test is vacuous");
	ZENITH_ASSERT_GT(uOrbsBefore, 0u, "...and seeds orbs");

	const u_int uBuy = ZM_ShopBuyPrice(ZM_ITEM_CATCHORB);
	const u_int uSell = ZM_ShopSellPrice(ZM_ITEM_CATCHORB);
	ZENITH_ASSERT_TRUE(ZM_ShopBuy(xState.m_xBag, xState.m_uMoney, ZM_ITEM_CATCHORB, 2u) == ZM_SHOP_OK,
		"the starter can afford two more orbs");
	ZENITH_ASSERT_EQ(xState.m_xBag.GetCount(ZM_ITEM_CATCHORB), uOrbsBefore + 2u, "...and receives them");
	ZENITH_ASSERT_EQ(xState.m_uMoney, uMoneyBefore - uBuy * 2u, "...and pays for them");

	ZENITH_ASSERT_TRUE(ZM_ShopSell(xState.m_xBag, xState.m_uMoney, ZM_ITEM_CATCHORB, 2u) == ZM_SHOP_OK,
		"...and can sell them straight back");
	ZENITH_ASSERT_EQ(xState.m_xBag.GetCount(ZM_ITEM_CATCHORB), uOrbsBefore, "...returning the bag");
	ZENITH_ASSERT_EQ(xState.m_uMoney, uMoneyBefore - (uBuy - uSell) * 2u,
		"...at a loss of exactly the buy/sell spread (the shop is not a money printer)");
	ZENITH_ASSERT_LT(xState.m_uMoney, uMoneyBefore,
		"...which must be a real loss, or buy-then-sell would be an infinite money loop");
}

// ============================================================================
// PART 5 -- ZM_UI_Shop: the authored element contract
// ============================================================================

ZENITH_TEST(ZM_Shop, RowElementName_EveryInRangeRowIsDistinctAndNonEmpty)
{
	for (u_int u = 0u; u < ZM_UI_Shop::uROWS_PER_PAGE; ++u)
	{
		const char* szName = ZM_UI_Shop::RowElementName(u);
		ZENITH_ASSERT_NOT_NULL(szName, "row %u has a name", u);
		ZENITH_ASSERT_TRUE(szName[0] != '\0', "row %u's name is not the empty string", u);
		for (u_int v = 0u; v < u; ++v)
		{
			ZENITH_ASSERT_FALSE(std::string(szName) == ZM_UI_Shop::RowElementName(v),
				"row %u and row %u must not share an element name", u, v);
		}
	}
	ZENITH_ASSERT_STREQ(ZM_UI_Shop::RowElementName(ZM_UI_Shop::uROWS_PER_PAGE), "",
		"the first out-of-range row has no element name");
	ZENITH_ASSERT_STREQ(ZM_UI_Shop::RowElementName(9999u), "",
		"a wildly out-of-range row has no element name (never a dangling pointer)");
}

ZENITH_TEST(ZM_Shop, RowIndex_RoundTripsAndRejectsForeignNames)
{
	for (u_int u = 0u; u < ZM_UI_Shop::uROWS_PER_PAGE; ++u)
	{
		ZENITH_ASSERT_EQ(ZM_UI_Shop::RowIndexFromElementName(ZM_UI_Shop::RowElementName(u)), (int)u,
			"row %u round-trips through its element name", u);
	}
	ZENITH_ASSERT_EQ(ZM_UI_Shop::RowIndexFromElementName(nullptr), -1, "a null name is not a shop row");
	ZENITH_ASSERT_EQ(ZM_UI_Shop::RowIndexFromElementName(""), -1, "the empty name is not a shop row");
	ZENITH_ASSERT_EQ(ZM_UI_Shop::RowIndexFromElementName(ZM_UI_Bag::RowElementName(0u)), -1,
		"a BAG row is not a shop row (the two contracts must not collide)");
	ZENITH_ASSERT_EQ(ZM_UI_Shop::RowIndexFromElementName(ZM_UI_MenuStack::szROOT_BAG_NAME), -1,
		"...nor is a ROOT entry");
	ZENITH_ASSERT_EQ(ZM_UI_Shop::RowIndexFromElementName(ZM_UI_Shop::szCONFIRM_NAME), -1,
		"...nor is a control -- Present relies on telling them apart");
	ZENITH_ASSERT_EQ(ZM_UI_Shop::RowIndexFromElementName("Menu_ShopRow6"), -1,
		"the one-past-the-end row name resolves to no row");
}

ZENITH_TEST(ZM_Shop, Controls_AreDistinctNamedAndClassified)
{
	for (u_int u = 0u; u < ZM_UI_Shop::uCONTROL_COUNT; ++u)
	{
		const char* szName = ZM_UI_Shop::ControlElementName(u);
		ZENITH_ASSERT_TRUE(szName[0] != '\0', "control %u has a real name", u);
		ZENITH_ASSERT_TRUE(ZM_UI_Shop::IsControlElementName(szName),
			"control %u ('%s') classifies as a control", u, szName);
		ZENITH_ASSERT_EQ(ZM_UI_Shop::RowIndexFromElementName(szName), -1,
			"control %u ('%s') is not a row", u, szName);
		for (u_int v = 0u; v < u; ++v)
		{
			ZENITH_ASSERT_FALSE(std::string(szName) == ZM_UI_Shop::ControlElementName(v),
				"controls %u and %u must not share an element name", u, v);
		}
	}
	ZENITH_ASSERT_STREQ(ZM_UI_Shop::ControlElementName(ZM_UI_Shop::uCONTROL_COUNT), "",
		"an out-of-range control index has no name");

	ZENITH_ASSERT_FALSE(ZM_UI_Shop::IsControlElementName(nullptr), "a null name is no control");
	ZENITH_ASSERT_FALSE(ZM_UI_Shop::IsControlElementName(ZM_UI_Shop::RowElementName(0u)),
		"a row is no control");

	// Exit is singled out because the MENU STACK, not the screen, acts on it.
	ZENITH_ASSERT_TRUE(ZM_UI_Shop::IsExitElementName(ZM_UI_Shop::szEXIT_NAME),
		"the Exit control is recognised");
	ZENITH_ASSERT_FALSE(ZM_UI_Shop::IsExitElementName(ZM_UI_Shop::szCONFIRM_NAME),
		"...and Confirm is NOT Exit (confusing them would close the shop on every purchase)");
	ZENITH_ASSERT_FALSE(ZM_UI_Shop::IsExitElementName(nullptr), "...and nor is nothing-focused");
}

// ============================================================================
// PART 6 -- ZM_UI_Shop: paging + quantity
// ============================================================================

ZENITH_TEST(ZM_Shop, PageCount_IsNeverZeroAndRoundsUp)
{
	ZENITH_ASSERT_EQ(ZM_UI_Shop::PageCount(0u), 1u,
		"an EMPTY list still shows ONE blank page (0 pages would leave ClampPage nothing to "
		"clamp to)");
	ZENITH_ASSERT_EQ(ZM_UI_Shop::PageCount(1u), 1u, "a single entry fits on one page");
	ZENITH_ASSERT_EQ(ZM_UI_Shop::PageCount(ZM_UI_Shop::uROWS_PER_PAGE), 1u,
		"an exactly-full page is ONE page, not two");
	ZENITH_ASSERT_EQ(ZM_UI_Shop::PageCount(ZM_UI_Shop::uROWS_PER_PAGE + 1u), 2u,
		"one entry past a full page opens a second page");
	ZENITH_ASSERT_EQ(ZM_UI_Shop::PageCount(ZM_UI_Shop::uMAX_INVENTORY),
		(ZM_UI_Shop::uMAX_INVENTORY + ZM_UI_Shop::uROWS_PER_PAGE - 1u) / ZM_UI_Shop::uROWS_PER_PAGE,
		"a full inventory is fully reachable by paging");
}

ZENITH_TEST(ZM_Shop, ClampPage_ClampsBothEnds)
{
	const u_int uEntries = ZM_UI_Shop::uROWS_PER_PAGE * 2u + 1u;   // three pages
	const int iLast = (int)ZM_UI_Shop::PageCount(uEntries) - 1;
	ZENITH_ASSERT_EQ(iLast, 2, "the fixture really spans three pages");

	ZENITH_ASSERT_EQ(ZM_UI_Shop::ClampPage(-1, uEntries), 0, "a negative page clamps to the first");
	ZENITH_ASSERT_EQ(ZM_UI_Shop::ClampPage(-9999, uEntries), 0, "...however negative");
	ZENITH_ASSERT_EQ(ZM_UI_Shop::ClampPage(iLast + 1, uEntries), iLast,
		"one page past the end clamps to the last");
	ZENITH_ASSERT_EQ(ZM_UI_Shop::ClampPage(9999, uEntries), iLast, "...however far past");
	ZENITH_ASSERT_EQ(ZM_UI_Shop::ClampPage(1, uEntries), 1, "an in-range page is unchanged");
	ZENITH_ASSERT_EQ(ZM_UI_Shop::ClampPage(3, 0u), 0,
		"an empty list clamps everything onto its single blank page");
}

ZENITH_TEST(ZM_Shop, RowMapping_CoversEveryEntryExactlyOnce)
{
	const u_int uEntries = ZM_UI_Shop::uROWS_PER_PAGE + 3u;   // two pages, the second partial

	ZENITH_ASSERT_EQ(ZM_UI_Shop::StackIndexForRow(0u, 0u, uEntries), 0,
		"page 0 row 0 is the first entry");
	ZENITH_ASSERT_EQ(ZM_UI_Shop::StackIndexForRow(1u, 0u, uEntries), (int)ZM_UI_Shop::uROWS_PER_PAGE,
		"page 1 starts a whole page in");
	ZENITH_ASSERT_EQ(ZM_UI_Shop::StackIndexForRow(1u, 2u, uEntries), (int)(uEntries - 1u),
		"the final entry sits on the last live row");
	ZENITH_ASSERT_EQ(ZM_UI_Shop::StackIndexForRow(1u, 3u, uEntries), -1,
		"the FIRST row past the end maps to no entry");
	ZENITH_ASSERT_EQ(ZM_UI_Shop::StackIndexForRow(0u, ZM_UI_Shop::uROWS_PER_PAGE, uEntries), -1,
		"an out-of-range row index maps to no entry");
	ZENITH_ASSERT_EQ(ZM_UI_Shop::StackIndexForRow(2u, 0u, uEntries), -1,
		"a page past the end maps to no entry");
	ZENITH_ASSERT_EQ(ZM_UI_Shop::StackIndexForRow(0u, 0u, 0u), -1,
		"an EMPTY list's blank page maps nothing");

	ZENITH_ASSERT_EQ(ZM_UI_Shop::VisibleRowCount(0u, uEntries), ZM_UI_Shop::uROWS_PER_PAGE,
		"a full page shows every row");
	ZENITH_ASSERT_EQ(ZM_UI_Shop::VisibleRowCount(1u, uEntries), 3u,
		"the trailing page shows exactly the remainder");
	ZENITH_ASSERT_EQ(ZM_UI_Shop::VisibleRowCount(2u, uEntries), 0u, "a page past the end shows nothing");
	ZENITH_ASSERT_EQ(ZM_UI_Shop::VisibleRowCount(0u, 0u), 0u,
		"an EMPTY list shows no rows (the notice is not a row that maps to an entry)");

	// The per-page counts must sum back to the whole list -- a dropped or double-counted
	// entry fails here even if each page looked plausible alone.
	u_int uTotal = 0u;
	for (u_int u = 0u; u < ZM_UI_Shop::PageCount(uEntries); ++u)
	{
		uTotal += ZM_UI_Shop::VisibleRowCount(u, uEntries);
	}
	ZENITH_ASSERT_EQ(uTotal, uEntries, "every entry is reachable on exactly one page");
}

ZENITH_TEST(ZM_Shop, ClampQuantity_HoldsTheOneToNinetyNineRange)
{
	ZENITH_ASSERT_EQ(ZM_UI_Shop::ClampQuantity(0), ZM_UI_Shop::iMIN_QUANTITY,
		"a zero quantity clamps up to the minimum (0 is not a transaction)");
	ZENITH_ASSERT_EQ(ZM_UI_Shop::ClampQuantity(-40), ZM_UI_Shop::iMIN_QUANTITY, "...as does a negative");
	ZENITH_ASSERT_EQ(ZM_UI_Shop::ClampQuantity(ZM_UI_Shop::iMAX_QUANTITY + 1),
		ZM_UI_Shop::iMAX_QUANTITY, "one past the maximum clamps down");
	ZENITH_ASSERT_EQ(ZM_UI_Shop::ClampQuantity(9999), ZM_UI_Shop::iMAX_QUANTITY, "...however far past");
	ZENITH_ASSERT_EQ(ZM_UI_Shop::ClampQuantity(1), 1, "an in-range quantity is unchanged");
	ZENITH_ASSERT_EQ(ZM_UI_Shop::ClampQuantity(50), 50, "...anywhere in the range");
	ZENITH_ASSERT_EQ(ZM_UI_Shop::iMIN_QUANTITY, 1, "the minimum is one");
	ZENITH_ASSERT_EQ(ZM_UI_Shop::iMAX_QUANTITY, 99, "and the maximum is ninety-nine");
}

// ============================================================================
// PART 7 -- ZM_UI_Shop: the formatters
// ============================================================================

ZENITH_TEST(ZM_Shop, FormatRows_CarryTheNameTheCountAndTheTablePrice)
{
	const std::string strBuy = ZM_UI_Shop::FormatBuyRow(ZM_ITEM_CATCHORB);
	ZENITH_ASSERT_TRUE(Contains(strBuy, ZM_GetItemName(ZM_ITEM_CATCHORB)),
		"the buy row names the item through ZM_GetItemName ('%s')", strBuy.c_str());
	ZENITH_ASSERT_TRUE(Contains(strBuy, std::to_string(ZM_ShopBuyPrice(ZM_ITEM_CATCHORB)).c_str()),
		"...and carries the TABLE buy price ('%s')", strBuy.c_str());
	ZENITH_ASSERT_FALSE(strBuy == ZM_UI_Shop::FormatBuyRow(ZM_ITEM_SALVE),
		"the buy row tracks the item");

	const std::string strSell = ZM_UI_Shop::FormatSellRow(ZM_ITEM_CATCHORB, 4u);
	ZENITH_ASSERT_TRUE(Contains(strSell, ZM_GetItemName(ZM_ITEM_CATCHORB)),
		"the sell row names the item ('%s')", strSell.c_str());
	ZENITH_ASSERT_TRUE(Contains(strSell, "x4"), "...carries the held count ('%s')", strSell.c_str());
	ZENITH_ASSERT_TRUE(Contains(strSell, std::to_string(ZM_ShopSellPrice(ZM_ITEM_CATCHORB)).c_str()),
		"...and the TABLE sell price ('%s')", strSell.c_str());
	ZENITH_ASSERT_FALSE(strSell == ZM_UI_Shop::FormatSellRow(ZM_ITEM_CATCHORB, 5u),
		"the sell row tracks the held count");
	ZENITH_ASSERT_FALSE(strSell == strBuy,
		"a sell row must not read like a buy row (the player has to know which way money flows)");

	const std::string strEmpty = ZM_UI_Shop::FormatEmptyList();
	ZENITH_ASSERT_FALSE(strEmpty.empty(),
		"the empty-list notice must be a real label -- an empty string would draw nothing and the "
		"list would just look broken");
	ZENITH_ASSERT_FALSE(strEmpty == strBuy, "...and is distinguishable from a real row");
}

ZENITH_TEST(ZM_Shop, FormatHeader_CarriesTheModeTheMoneyAndTheQuantity)
{
	const std::string strHeader = ZM_UI_Shop::FormatHeader(ZM_SHOP_MODE_BUY, 3000u, 7u);
	ZENITH_ASSERT_TRUE(Contains(strHeader, ZM_UI_Shop::ModeToString(ZM_SHOP_MODE_BUY)),
		"the header carries the mode word ('%s')", strHeader.c_str());
	ZENITH_ASSERT_TRUE(Contains(strHeader, "3000"), "...the money ('%s')", strHeader.c_str());
	ZENITH_ASSERT_TRUE(Contains(strHeader, "7"), "...and the quantity ('%s')", strHeader.c_str());

	ZENITH_ASSERT_FALSE(strHeader == ZM_UI_Shop::FormatHeader(ZM_SHOP_MODE_SELL, 3000u, 7u),
		"the header tracks the mode");
	ZENITH_ASSERT_FALSE(strHeader == ZM_UI_Shop::FormatHeader(ZM_SHOP_MODE_BUY, 2999u, 7u),
		"...the money");
	ZENITH_ASSERT_FALSE(strHeader == ZM_UI_Shop::FormatHeader(ZM_SHOP_MODE_BUY, 3000u, 8u),
		"...and the quantity");

	ZENITH_ASSERT_STREQ(ZM_UI_Shop::ModeToString(ZM_SHOP_MODE_BUY), "BUY", "the buy mode is named");
	ZENITH_ASSERT_STREQ(ZM_UI_Shop::ModeToString(ZM_SHOP_MODE_SELL), "SELL", "...and so is sell");
	ZENITH_ASSERT_STREQ(ZM_UI_Shop::ModeToString(ZM_SHOP_MODE_COUNT), "",
		"an out-of-range mode names nothing (never an out-of-range table read)");
}

ZENITH_TEST(ZM_Shop, FormatResult_IsTotalAndEveryLineIsDistinctAndNonEmpty)
{
	// Walked, never spelled out: a result added later without a line would fail here
	// rather than silently reporting nothing to the player.
	for (u_int u = 0u; u < (u_int)ZM_SHOP_RESULT_COUNT; ++u)
	{
		const char* szLine = ZM_UI_Shop::FormatResult((ZM_SHOP_RESULT)u);
		ZENITH_ASSERT_NOT_NULL(szLine, "result %u has a report line", u);
		ZENITH_ASSERT_TRUE(szLine[0] != '\0',
			"result %u must report SOMETHING -- a blank line reads as the shop ignoring the player", u);
		for (u_int v = 0u; v < u; ++v)
		{
			ZENITH_ASSERT_FALSE(std::string(szLine) == ZM_UI_Shop::FormatResult((ZM_SHOP_RESULT)v),
				"results %u and %u must not share a report line (the player could not tell them "
				"apart)", u, v);
		}
	}
	ZENITH_ASSERT_TRUE(ZM_UI_Shop::FormatResult((ZM_SHOP_RESULT)ZM_SHOP_RESULT_COUNT)[0] != '\0',
		"even a stray result value reports something");
}

ZENITH_TEST(ZM_Shop, FormatHeaderLine_AddsTheReportOnlyOnceThereIsOne)
{
	const std::string strBare = ZM_UI_Shop::FormatHeaderLine(ZM_SHOP_MODE_BUY, 100u, 1u,
		/* bHasResult */ false, ZM_SHOP_OK);
	ZENITH_ASSERT_TRUE(strBare == ZM_UI_Shop::FormatHeader(ZM_SHOP_MODE_BUY, 100u, 1u),
		"before any transaction the header is JUST the standing part ('%s') -- reporting "
		"'Thank you!' to a player who has bought nothing would be a lie", strBare.c_str());

	const std::string strReported = ZM_UI_Shop::FormatHeaderLine(ZM_SHOP_MODE_BUY, 100u, 1u,
		/* bHasResult */ true, ZM_SHOP_ERR_CANNOT_AFFORD);
	ZENITH_ASSERT_TRUE(Contains(strReported, ZM_UI_Shop::FormatResult(ZM_SHOP_ERR_CANNOT_AFFORD)),
		"once there is a result the header carries its report ('%s')", strReported.c_str());
	ZENITH_ASSERT_TRUE(Contains(strReported, "100"), "...alongside the money ('%s')", strReported.c_str());
	ZENITH_ASSERT_FALSE(strReported == strBare, "...and reads differently from the bare header");
}

// ============================================================================
// PART 8 -- ZM_UI_Shop: the list model + the confirm dispatch
// ============================================================================

ZENITH_TEST(ZM_Shop, Fresh_StartsOnTheBuyListWithNoStockAndNoReport)
{
	ZM_UI_Shop xShop;
	ZENITH_ASSERT_TRUE(xShop.GetMode() == ZM_SHOP_MODE_BUY, "a fresh shop opens on the BUY list");
	ZENITH_ASSERT_EQ(xShop.GetPage(), 0, "...on the first page");
	ZENITH_ASSERT_EQ(xShop.GetQuantity(), 1u, "...one at a time");
	ZENITH_ASSERT_EQ(xShop.GetInventoryCount(), 0u, "...with no stock configured yet");
	ZENITH_ASSERT_FALSE(xShop.HasResult(), "...and nothing to report");
	ZENITH_ASSERT_TRUE(xShop.GetInventoryItem(0u) == ZM_ITEM_NONE, "...so no stocked id resolves");
}

ZENITH_TEST(ZM_Shop, SetInventory_IsAllOrNothing)
{
	ZM_UI_Shop xShop;
	const ZM_ITEM_ID aeGood[3] = { ZM_ITEM_CATCHORB, ZM_ITEM_SALVE, ZM_ITEM_GREATORB };
	ZENITH_ASSERT_TRUE(xShop.SetInventory(aeGood, 3u), "a valid stock list is accepted");
	ZENITH_ASSERT_EQ(xShop.GetInventoryCount(), 3u, "...whole");
	for (u_int u = 0u; u < 3u; ++u)
	{
		ZENITH_ASSERT_TRUE(xShop.GetInventoryItem(u) == aeGood[u], "...and in order (slot %u)", u);
	}
	ZENITH_ASSERT_TRUE(xShop.GetInventoryItem(3u) == ZM_ITEM_NONE, "...with nothing past the end");

	// Every rejection leaves the ACCEPTED list intact -- a half-written stock list would
	// let a mis-authored mart sell whatever happened to survive.
	const ZM_ITEM_ID aeBadId[2] = { ZM_ITEM_CATCHORB, ZM_ITEM_NONE };
	ZENITH_ASSERT_FALSE(xShop.SetInventory(aeBadId, 2u),
		"a list containing an out-of-range id is rejected WHOLE");
	ZENITH_ASSERT_EQ(xShop.GetInventoryCount(), 3u, "...leaving the previous stock intact");
	ZENITH_ASSERT_TRUE(xShop.GetInventoryItem(0u) == ZM_ITEM_CATCHORB, "...unchanged");

	ZENITH_ASSERT_FALSE(xShop.SetInventory(nullptr, 2u), "a null list is rejected");
	ZENITH_ASSERT_FALSE(xShop.SetInventory(aeGood, 0u),
		"an EMPTY stock list is rejected -- a mart with nothing to sell is a bug, not a screen");
	ZENITH_ASSERT_EQ(xShop.GetInventoryCount(), 3u, "...and neither disturbed the stock");

	// Capacity: exactly uMAX_INVENTORY is fine, one more is not.
	ZM_ITEM_ID aeFull[ZM_UI_Shop::uMAX_INVENTORY + 1u] = {};
	for (u_int u = 0u; u < ZM_UI_Shop::uMAX_INVENTORY + 1u; ++u)
	{
		aeFull[u] = ZM_ITEM_CATCHORB;
	}
	ZENITH_ASSERT_TRUE(xShop.SetInventory(aeFull, ZM_UI_Shop::uMAX_INVENTORY),
		"a full-capacity stock list is accepted");
	ZENITH_ASSERT_EQ(xShop.GetInventoryCount(), ZM_UI_Shop::uMAX_INVENTORY, "...whole");
	ZENITH_ASSERT_FALSE(xShop.SetInventory(aeFull, ZM_UI_Shop::uMAX_INVENTORY + 1u),
		"one past capacity is rejected (never a write past the array)");
	ZENITH_ASSERT_EQ(xShop.GetInventoryCount(), ZM_UI_Shop::uMAX_INVENTORY,
		"...leaving the accepted list intact");
}

ZENITH_TEST(ZM_Shop, SetInventory_StillAcceptsUnbuyableStockSoTheLogicGuardStaysLoadBearing)
{
	// Deliberate: refusing a price-0 item HERE would make ZM_ShopBuy's NOT_PURCHASABLE
	// guard unreachable from the UI, and that guard is the one standing between a
	// mis-authored mart and a free key item. It is listed, and the transaction refuses it.
	const ZM_ITEM_ID eFree = FindUnpurchasableItem();
	ZENITH_ASSERT_TRUE(eFree != ZM_ITEM_NONE, "the table must price something at 0 for this fixture");

	ZM_UI_Shop xShop;
	const ZM_ITEM_ID aeStock[1] = { eFree };
	ZENITH_ASSERT_TRUE(xShop.SetInventory(aeStock, 1u), "unbuyable stock is still a valid ID list");

	ZM_Bag xBag;
	xBag.Clear();
	const ZM_Bag xBefore = xBag;
	u_int uMoney = 999999u;

	const ZM_SHOP_RESULT eResult = xShop.Confirm(ZM_UI_Shop::szCONFIRM_NAME, xBag, uMoney);
	ZENITH_ASSERT_TRUE(eResult == ZM_SHOP_ERR_NOT_PURCHASABLE,
		"...and confirming it is refused by the LOGIC, not by the list");
	ZENITH_ASSERT_EQ(uMoney, 999999u, "...taking nothing");
	ZENITH_ASSERT_TRUE(BagsMatch(xBag, xBefore), "...and delivering nothing");
}

ZENITH_TEST(ZM_Shop, EntryCount_AndEntryAt_FollowTheMode)
{
	ZM_Bag xBag;
	xBag.Clear();
	ZENITH_ASSERT_TRUE(xBag.Add(ZM_ITEM_CATCHORB, 2u), "seed the ball pocket");
	ZENITH_ASSERT_TRUE(xBag.Add(ZM_ITEM_SALVE, 4u), "...and the medicine pocket");

	ZM_UI_Shop xShop;
	const ZM_ITEM_ID aeStock[3] = { ZM_ITEM_CATCHORB, ZM_ITEM_GREATORB, ZM_ITEM_ULTRAORB };
	ZENITH_ASSERT_TRUE(xShop.SetInventory(aeStock, 3u), "stock the shop");

	ZENITH_ASSERT_EQ(xShop.EntryCount(xBag), 3u, "the BUY list is the shop's stock");
	ZM_ITEM_ID eItem = ZM_ITEM_NONE;
	u_int uHeld = 0u;
	ZENITH_ASSERT_TRUE(xShop.EntryAt(xBag, 1u, eItem, uHeld), "...and resolves its entries in order");
	ZENITH_ASSERT_TRUE(eItem == ZM_ITEM_GREATORB, "...entry 1 is the second stocked id");
	ZENITH_ASSERT_FALSE(xShop.EntryAt(xBag, 3u, eItem, uHeld), "...with nothing past the end");

	u_int uMoney = 5000u;
	ZENITH_ASSERT_TRUE(xShop.Confirm(ZM_UI_Shop::szSELL_TAB_NAME, xBag, uMoney) == ZM_SHOP_OK,
		"switch to the SELL list");
	ZENITH_ASSERT_TRUE(xShop.GetMode() == ZM_SHOP_MODE_SELL, "...the mode really switched");
	ZENITH_ASSERT_EQ(xShop.EntryCount(xBag), xBag.TotalStackCount(),
		"the SELL list is every stack the player carries");
	ZENITH_ASSERT_EQ(xShop.EntryCount(xBag), 2u, "...which is two here");

	// The flattening is pocket order, then stack order -- deterministic, because ZM_Bag
	// keeps each pocket sorted.
	ZENITH_ASSERT_TRUE(xShop.EntryAt(xBag, 0u, eItem, uHeld), "sell entry 0 resolves");
	ZENITH_ASSERT_TRUE(eItem == ZM_ITEM_CATCHORB, "...the BALL pocket comes first");
	ZENITH_ASSERT_EQ(uHeld, 2u, "...with the held count");
	ZENITH_ASSERT_TRUE(xShop.EntryAt(xBag, 1u, eItem, uHeld), "sell entry 1 resolves");
	ZENITH_ASSERT_TRUE(eItem == ZM_ITEM_SALVE, "...the MEDICINE pocket second");
	ZENITH_ASSERT_EQ(uHeld, 4u, "...with its held count");
	ZENITH_ASSERT_FALSE(xShop.EntryAt(xBag, 2u, eItem, uHeld), "...and nothing past the end");
}

ZENITH_TEST(ZM_Shop, SellEntryAt_RejectsPastTheEndOfAnEmptyBag)
{
	ZM_Bag xBag;
	xBag.Clear();
	ZM_ITEM_ID eItem = ZM_ITEM_CATCHORB;
	u_int uHeld = 99u;
	ZENITH_ASSERT_FALSE(ZM_UI_Shop::SellEntryAt(xBag, 0u, eItem, uHeld),
		"an empty bag lists nothing to sell");
	ZENITH_ASSERT_TRUE(eItem == ZM_ITEM_NONE, "...and the outputs are cleared, not left stale");
	ZENITH_ASSERT_EQ(uHeld, 0u, "...both of them");
	ZENITH_ASSERT_FALSE(ZM_UI_Shop::SellEntryAt(xBag, 9999u, eItem, uHeld),
		"...however far past the end");
}

ZENITH_TEST(ZM_Shop, ModeSwitch_ResetsThePageAndTheQuantity)
{
	ZM_Bag xBag;
	xBag.Clear();
	ZM_UI_Shop xShop;
	// A stock list spanning two pages, so the page can be moved off 0.
	ZM_ITEM_ID aeStock[ZM_UI_Shop::uROWS_PER_PAGE + 2u] = {};
	for (u_int u = 0u; u < ZM_UI_Shop::uROWS_PER_PAGE + 2u; ++u)
	{
		aeStock[u] = (ZM_ITEM_ID)u;   // the first ids in the table, all in range
	}
	ZENITH_ASSERT_TRUE(xShop.SetInventory(aeStock, ZM_UI_Shop::uROWS_PER_PAGE + 2u),
		"stock two pages' worth");

	u_int uMoney = 10000u;
	ZENITH_ASSERT_TRUE(xShop.Confirm(ZM_UI_Shop::szNEXT_PAGE_NAME, xBag, uMoney) == ZM_SHOP_OK,
		"page forward");
	ZENITH_ASSERT_EQ(xShop.GetPage(), 1, "...onto page 1");
	ZENITH_ASSERT_TRUE(xShop.Confirm(ZM_UI_Shop::szQTY_UP_NAME, xBag, uMoney) == ZM_SHOP_OK,
		"step the quantity up");
	ZENITH_ASSERT_EQ(xShop.GetQuantity(), 2u, "...to two");

	ZENITH_ASSERT_TRUE(xShop.Confirm(ZM_UI_Shop::szSELL_TAB_NAME, xBag, uMoney) == ZM_SHOP_OK,
		"switch to SELL");
	ZENITH_ASSERT_TRUE(xShop.GetMode() == ZM_SHOP_MODE_SELL, "...the mode switched");
	ZENITH_ASSERT_EQ(xShop.GetPage(), 0,
		"...and the page RESET (the new list may be shorter -- a stale page would show blank)");
	ZENITH_ASSERT_EQ(xShop.GetQuantity(), 1u, "...and so did the quantity");

	// Re-confirming the mode already showing changes nothing (it is not a toggle).
	ZENITH_ASSERT_TRUE(xShop.Confirm(ZM_UI_Shop::szSELL_TAB_NAME, xBag, uMoney) == ZM_SHOP_OK,
		"confirming the ACTIVE tab is accepted");
	ZENITH_ASSERT_TRUE(xShop.GetMode() == ZM_SHOP_MODE_SELL, "...and stays on that mode");

	ZENITH_ASSERT_TRUE(xShop.Confirm(ZM_UI_Shop::szBUY_TAB_NAME, xBag, uMoney) == ZM_SHOP_OK,
		"switch back to BUY");
	ZENITH_ASSERT_TRUE(xShop.GetMode() == ZM_SHOP_MODE_BUY, "...the mode switched back");
	ZENITH_ASSERT_EQ(xShop.GetPage(), 0, "...with the page reset again");
	ZENITH_ASSERT_EQ(uMoney, 10000u, "none of the navigation touched the purse");
}

ZENITH_TEST(ZM_Shop, Quantity_StepsAndClampsThroughTheConfirmDispatch)
{
	ZM_Bag xBag;
	xBag.Clear();
	ZM_UI_Shop xShop;
	const ZM_ITEM_ID aeStock[1] = { ZM_ITEM_CATCHORB };
	ZENITH_ASSERT_TRUE(xShop.SetInventory(aeStock, 1u), "stock the shop");
	u_int uMoney = 0u;

	ZENITH_ASSERT_EQ(xShop.GetQuantity(), 1u, "the quantity starts at one");
	xShop.Confirm(ZM_UI_Shop::szQTY_DOWN_NAME, xBag, uMoney);
	ZENITH_ASSERT_EQ(xShop.GetQuantity(), 1u, "...and never steps below one");

	for (int i = 0; i < ZM_UI_Shop::iMAX_QUANTITY + 5; ++i)
	{
		xShop.Confirm(ZM_UI_Shop::szQTY_UP_NAME, xBag, uMoney);
	}
	ZENITH_ASSERT_EQ(xShop.GetQuantity(), (u_int)ZM_UI_Shop::iMAX_QUANTITY,
		"...and never steps above the maximum");
	xShop.Confirm(ZM_UI_Shop::szQTY_DOWN_NAME, xBag, uMoney);
	ZENITH_ASSERT_EQ(xShop.GetQuantity(), (u_int)ZM_UI_Shop::iMAX_QUANTITY - 1u,
		"...stepping back down one at a time");
}

ZENITH_TEST(ZM_Shop, Confirm_BuysTheSelectedEntryAgainstTheLiveStateAndReportsIt)
{
	ZM_GameState xState = ZM_MakeStarterGameState();
	const u_int uMoneyBefore = xState.m_uMoney;
	const u_int uOrbsBefore = xState.m_xBag.GetCount(ZM_ITEM_CATCHORB);

	ZM_UI_Shop xShop;
	const ZM_ITEM_ID aeStock[2] = { ZM_ITEM_CATCHORB, ZM_ITEM_SALVE };
	ZENITH_ASSERT_TRUE(xShop.SetInventory(aeStock, 2u), "stock the mart");
	ZENITH_ASSERT_EQ(xShop.GetCursor(), 0, "a fresh shop selects the first entry");

	const ZM_SHOP_RESULT eResult =
		xShop.Confirm(ZM_UI_Shop::szCONFIRM_NAME, xState.m_xBag, xState.m_uMoney);
	ZENITH_ASSERT_TRUE(eResult == ZM_SHOP_OK, "confirming a funded purchase is accepted");
	ZENITH_ASSERT_EQ(xState.m_uMoney, uMoneyBefore - ZM_ShopBuyPrice(ZM_ITEM_CATCHORB),
		"...debiting the LIVE purse by the table price (the money is passed by REFERENCE, so a "
		"purchase actually persists)");
	ZENITH_ASSERT_EQ(xState.m_xBag.GetCount(ZM_ITEM_CATCHORB), uOrbsBefore + 1u,
		"...and crediting the LIVE bag");
	ZENITH_ASSERT_TRUE(xShop.HasResult(), "...and the screen now has something to report");
	ZENITH_ASSERT_TRUE(xShop.GetLastResult() == ZM_SHOP_OK, "...namely the success");

	// The quantity is honoured.
	xShop.Confirm(ZM_UI_Shop::szQTY_UP_NAME, xState.m_xBag, xState.m_uMoney);
	ZENITH_ASSERT_EQ(xShop.GetQuantity(), 2u, "step the quantity to two");
	ZENITH_ASSERT_TRUE(
		xShop.Confirm(ZM_UI_Shop::szCONFIRM_NAME, xState.m_xBag, xState.m_uMoney) == ZM_SHOP_OK,
		"...and buy again");
	ZENITH_ASSERT_EQ(xState.m_xBag.GetCount(ZM_ITEM_CATCHORB), uOrbsBefore + 3u,
		"...receiving the whole quantity");
	ZENITH_ASSERT_EQ(xState.m_uMoney, uMoneyBefore - ZM_ShopBuyPrice(ZM_ITEM_CATCHORB) * 3u,
		"...and paying for the whole quantity");
}

ZENITH_TEST(ZM_Shop, Confirm_SellsTheSelectedEntryAgainstTheLiveState)
{
	// The SELL half of the confirm dispatch -- the OTHER of the only two code paths that
	// move the player's money, and the one an empty-bag fixture can never reach (it
	// short-circuits on "nothing selected" long before ZM_ShopSell). A bag with exactly
	// ONE stack, so the SELL list has one entry and the fresh cursor sits on it.
	ZM_Bag xBag;
	xBag.Clear();
	ZENITH_ASSERT_TRUE(xBag.Add(ZM_ITEM_CATCHORB, 3u), "seed three orbs through the REAL Add");
	const u_int uPrice = ZM_ShopSellPrice(ZM_ITEM_CATCHORB);
	ZENITH_ASSERT_GT(uPrice, 0u, "the fixture item must be sellable or the test is vacuous");

	ZM_UI_Shop xShop;
	// The BUY stock is deliberately a DIFFERENT item, so a confirm dispatched down the
	// wrong branch could not even transact on the same row.
	const ZM_ITEM_ID aeStock[1] = { ZM_ITEM_SALVE };
	ZENITH_ASSERT_TRUE(xShop.SetInventory(aeStock, 1u), "stock the mart");

	// Broke on purpose: a mis-dispatched BUY would DEBIT, and from zero it could not even
	// be afforded -- so the OK + credit below pins the branch, not just the arithmetic.
	u_int uMoney = 0u;
	ZENITH_ASSERT_TRUE(xShop.Confirm(ZM_UI_Shop::szSELL_TAB_NAME, xBag, uMoney) == ZM_SHOP_OK,
		"switch to the SELL list");
	ZENITH_ASSERT_EQ(xShop.EntryCount(xBag), 1u, "...which lists the one stack the player holds");
	ZENITH_ASSERT_EQ(xShop.GetSelectedEntryIndex(xBag), 0, "...with that stack selected");

	ZENITH_ASSERT_TRUE(xShop.Confirm(ZM_UI_Shop::szCONFIRM_NAME, xBag, uMoney) == ZM_SHOP_OK,
		"confirming a sale is accepted");
	ZENITH_ASSERT_EQ(uMoney, uPrice,
		"...CREDITING the purse by the table sell price (a confirm routed to ZM_ShopBuy would "
		"have DEBITED it -- and from zero could not have paid at all)");
	ZENITH_ASSERT_EQ(xBag.GetCount(ZM_ITEM_CATCHORB), 2u,
		"...and taking exactly the quantity out of the bag");
	ZENITH_ASSERT_TRUE(xShop.HasResult() && xShop.GetLastResult() == ZM_SHOP_OK,
		"...and REPORTING the sale, so the header can confirm it to the player");

	// ...and the QUANTITY really reaches ZM_ShopSell (not just the item).
	ZENITH_ASSERT_TRUE(xShop.Confirm(ZM_UI_Shop::szQTY_UP_NAME, xBag, uMoney) == ZM_SHOP_OK,
		"step the quantity to two");
	ZENITH_ASSERT_EQ(xShop.GetQuantity(), 2u, "...it really is two");
	ZENITH_ASSERT_TRUE(xShop.Confirm(ZM_UI_Shop::szCONFIRM_NAME, xBag, uMoney) == ZM_SHOP_OK,
		"...and sell the remaining pair in one press");
	ZENITH_ASSERT_EQ(uMoney, uPrice * 3u, "the WHOLE quantity was credited");
	ZENITH_ASSERT_EQ(xBag.GetCount(ZM_ITEM_CATCHORB), 0u, "...and the whole quantity left the bag");
	ZENITH_ASSERT_EQ(xShop.EntryCount(xBag), 0u, "...emptying the sell list with it");
}

ZENITH_TEST(ZM_Shop, SelectedEntryIndex_ResolvesThePageRelativeCursorToAFlatIndex)
{
	// The cursor is a PAGE-RELATIVE ROW, and the flat list index only coincides with it on
	// page 0 -- which is exactly why callers get GetSelectedEntryIndex instead of being
	// left to read GetCursor as an entry number.
	ZM_Bag xBag;
	xBag.Clear();
	ZM_UI_Shop xShop;
	constexpr u_int uSTOCK = ZM_UI_Shop::uROWS_PER_PAGE + 2u;
	ZM_ITEM_ID aeStock[uSTOCK] = {};
	for (u_int u = 0u; u < uSTOCK; ++u)
	{
		aeStock[u] = (ZM_ITEM_ID)u;   // the first ids in the table, all in range
	}
	ZENITH_ASSERT_TRUE(xShop.SetInventory(aeStock, uSTOCK), "stock two pages' worth");

	u_int uMoney = 0u;
	ZENITH_ASSERT_EQ(xShop.GetCursor(), 0, "a fresh screen sits on row 0");
	ZENITH_ASSERT_EQ(xShop.GetSelectedEntryIndex(xBag), 0, "...which on page 0 IS entry 0");

	ZENITH_ASSERT_TRUE(xShop.Confirm(ZM_UI_Shop::szNEXT_PAGE_NAME, xBag, uMoney) == ZM_SHOP_OK,
		"page forward");
	ZENITH_ASSERT_EQ(xShop.GetPage(), 1, "...onto page 1");
	ZENITH_ASSERT_EQ(xShop.GetCursor(), 0, "...where the ROW is still 0");
	ZENITH_ASSERT_EQ(xShop.GetSelectedEntryIndex(xBag), (int)ZM_UI_Shop::uROWS_PER_PAGE,
		"...but the ENTRY is the first of the second page -- the two are NOT the same number, "
		"and a caller reading GetCursor as an entry index would transact on the wrong item");

	// The SELL list is a different length, so the same accessor has to re-resolve against
	// the live bag rather than the stock.
	ZENITH_ASSERT_TRUE(xShop.Confirm(ZM_UI_Shop::szSELL_TAB_NAME, xBag, uMoney) == ZM_SHOP_OK,
		"switch to the (empty) SELL list");
	ZENITH_ASSERT_EQ(xShop.GetSelectedEntryIndex(xBag), -1,
		"an empty list selects nothing at all");
}

ZENITH_TEST(ZM_Shop, Confirm_RefusalsAreReportedAndSpendNothing)
{
	ZM_Bag xBag;
	xBag.Clear();
	const ZM_Bag xBefore = xBag;

	ZM_UI_Shop xShop;
	const ZM_ITEM_ID aeStock[1] = { ZM_ITEM_CATCHORB };
	ZENITH_ASSERT_TRUE(xShop.SetInventory(aeStock, 1u), "stock the mart");

	u_int uMoney = 0u;   // broke
	const ZM_SHOP_RESULT eResult = xShop.Confirm(ZM_UI_Shop::szCONFIRM_NAME, xBag, uMoney);
	ZENITH_ASSERT_TRUE(eResult == ZM_SHOP_ERR_CANNOT_AFFORD, "a broke player cannot buy");
	ZENITH_ASSERT_EQ(uMoney, 0u, "...and pays nothing");
	ZENITH_ASSERT_TRUE(BagsMatch(xBag, xBefore), "...and receives nothing");
	ZENITH_ASSERT_TRUE(xShop.HasResult() && xShop.GetLastResult() == ZM_SHOP_ERR_CANNOT_AFFORD,
		"...and the refusal is REPORTED, so the header can tell the player why");

	// The SELL side with an empty bag: the list is empty, so nothing is selected and the
	// press is reported rather than silently swallowed.
	ZENITH_ASSERT_TRUE(xShop.Confirm(ZM_UI_Shop::szSELL_TAB_NAME, xBag, uMoney) == ZM_SHOP_OK,
		"switch to SELL over an empty bag");
	ZENITH_ASSERT_EQ(xShop.EntryCount(xBag), 0u, "...the sell list really is empty");
	ZENITH_ASSERT_TRUE(
		xShop.Confirm(ZM_UI_Shop::szCONFIRM_NAME, xBag, uMoney) == ZM_SHOP_ERR_INVALID_ITEM,
		"...and confirming with nothing selected is refused, not ignored");
	ZENITH_ASSERT_EQ(uMoney, 0u, "...paying nothing");
	ZENITH_ASSERT_TRUE(BagsMatch(xBag, xBefore), "...and taking nothing");
}

ZENITH_TEST(ZM_Shop, Confirm_ARowTheExitAndAForeignNameSpendNothing)
{
	ZM_GameState xState = ZM_MakeStarterGameState();
	const u_int uMoneyBefore = xState.m_uMoney;
	const ZM_Bag xBagBefore = xState.m_xBag;

	ZM_UI_Shop xShop;
	const ZM_ITEM_ID aeStock[1] = { ZM_ITEM_CATCHORB };
	ZENITH_ASSERT_TRUE(xShop.SetInventory(aeStock, 1u), "stock the mart");

	// A ROW only SELECTS -- confirming one must never transact, or a stray press while
	// the focus sits on the list would spend the player's money.
	ZENITH_ASSERT_TRUE(
		xShop.Confirm(ZM_UI_Shop::RowElementName(0u), xState.m_xBag, xState.m_uMoney) == ZM_SHOP_OK,
		"confirming a row is inert");
	// The Exit control belongs to the MENU STACK (a by-value screen cannot pop itself).
	ZENITH_ASSERT_TRUE(
		xShop.Confirm(ZM_UI_Shop::szEXIT_NAME, xState.m_xBag, xState.m_uMoney) == ZM_SHOP_OK,
		"confirming Exit is inert HERE (the menu stack pops the screen)");
	ZENITH_ASSERT_TRUE(
		xShop.Confirm(nullptr, xState.m_xBag, xState.m_uMoney) == ZM_SHOP_OK,
		"a null focused name (nothing focused) is inert");
	ZENITH_ASSERT_TRUE(
		xShop.Confirm(ZM_UI_MenuStack::szROOT_BAG_NAME, xState.m_xBag, xState.m_uMoney) == ZM_SHOP_OK,
		"a foreign element name is inert");

	ZENITH_ASSERT_EQ(xState.m_uMoney, uMoneyBefore, "none of those presses touched the purse");
	ZENITH_ASSERT_TRUE(BagsMatch(xState.m_xBag, xBagBefore), "...nor the bag");
	ZENITH_ASSERT_FALSE(xShop.HasResult(),
		"...and none of them left a transaction report (they were not transactions)");
}

ZENITH_TEST(ZM_Shop, Reset_DropsTheStockAndTheWholeSession)
{
	ZM_Bag xBag;
	xBag.Clear();
	ZM_UI_Shop xShop;
	const ZM_ITEM_ID aeStock[2] = { ZM_ITEM_CATCHORB, ZM_ITEM_SALVE };
	ZENITH_ASSERT_TRUE(xShop.SetInventory(aeStock, 2u), "stock the mart");

	u_int uMoney = 0u;
	xShop.Confirm(ZM_UI_Shop::szSELL_TAB_NAME, xBag, uMoney);
	xShop.Confirm(ZM_UI_Shop::szQTY_UP_NAME, xBag, uMoney);
	xShop.Confirm(ZM_UI_Shop::szCONFIRM_NAME, xBag, uMoney);   // refused (empty bag) -> a report
	ZENITH_ASSERT_TRUE(xShop.GetMode() == ZM_SHOP_MODE_SELL, "the fixture really is mid-session");
	ZENITH_ASSERT_EQ(xShop.GetQuantity(), 2u, "...on the quantity");
	ZENITH_ASSERT_TRUE(xShop.HasResult(), "...and carrying a report");

	xShop.Reset();
	ZENITH_ASSERT_TRUE(xShop.GetMode() == ZM_SHOP_MODE_BUY, "Reset returns to the BUY list");
	ZENITH_ASSERT_EQ(xShop.GetPage(), 0, "...the first page");
	ZENITH_ASSERT_EQ(xShop.GetQuantity(), 1u, "...a quantity of one");
	ZENITH_ASSERT_FALSE(xShop.HasResult(), "...and no report");
	ZENITH_ASSERT_EQ(xShop.GetInventoryCount(), 0u,
		"...and DROPS the stock: leaving a mart must not leave its shelves resolvable");
}

// ============================================================================
// PART 9 -- the host enum guard + the presentation seam (headless half)
// ============================================================================

ZENITH_TEST(ZM_Shop, HostScreenEnum_ShopIsAppendedAndNotARootEntry)
{
	// Save-stable append: SHOP was added AFTER DIALOGUE, so the six pre-existing screens
	// keep their values. Pinning the literal catches an accidental reorder that would
	// silently re-map serialized screen ids.
	ZENITH_ASSERT_EQ((u_int)ZM_MENU_SCREEN_SHOP, 6u,
		"SHOP is the 7th enumerator (NONE/ROOT/PARTY/BAG/DEX/DIALOGUE/SHOP) -- append-only");
	ZENITH_ASSERT_EQ((u_int)ZM_MENU_SCREEN_DIALOGUE, 5u,
		"...and DIALOGUE kept its value");
	ZENITH_ASSERT_TRUE((u_int)ZM_MENU_SCREEN_SHOP < (u_int)ZM_MENU_SCREEN_COUNT,
		"SHOP is within the enumerated range");

	// A mart is entered by talking to its clerk, never from the pause menu.
	for (u_int uAction = (u_int)ZM_MENU_ACTION_NONE; uAction <= (u_int)ZM_MENU_ACTION_CLOSE; ++uAction)
	{
		ZENITH_ASSERT_NE(
			(u_int)ZM_UI_MenuStack::RootActionToScreen((ZM_MENU_ACTION)uAction),
			(u_int)ZM_MENU_SCREEN_SHOP,
			"no ROOT entry opens the shop -- it is raised only by OpenShop / TryOpenShop");
	}
}

ZENITH_TEST(ZM_Shop, PresentAndHide_OnAnInvalidRootAreBestEffortNoOps)
{
	// Present short-circuits on an INVALID root handle right after settling the page /
	// quantity clamp and before it touches any scene, so the guard is headlessly testable.
	// The contract pinned here is that presentation never disturbs the MODEL -- and above
	// all never moves money.
	ZM_Bag xBag;
	xBag.Clear();
	ZENITH_ASSERT_TRUE(xBag.Add(ZM_ITEM_CATCHORB, 2u), "seed the bag");
	const ZM_Bag xBefore = xBag;

	ZM_UI_Shop xShop;
	const ZM_ITEM_ID aeStock[2] = { ZM_ITEM_CATCHORB, ZM_ITEM_SALVE };
	ZENITH_ASSERT_TRUE(xShop.SetInventory(aeStock, 2u), "stock the mart");

	Zenith_Entity xNoRoot;
	ZENITH_ASSERT_FALSE(xNoRoot.IsValid(), "a default-constructed entity handle is invalid");

	xShop.Present(xNoRoot, xBag, 1234u);
	ZENITH_ASSERT_TRUE(xShop.GetMode() == ZM_SHOP_MODE_BUY, "presenting to a missing root keeps the mode");
	ZENITH_ASSERT_EQ(xShop.GetPage(), 0, "...the page");
	ZENITH_ASSERT_EQ(xShop.GetQuantity(), 1u, "...the quantity");
	ZENITH_ASSERT_EQ(xShop.GetCursor(), 0, "...and the selection");
	ZENITH_ASSERT_EQ(xShop.GetInventoryCount(), 2u, "...and the stock");
	ZENITH_ASSERT_TRUE(BagsMatch(xBag, xBefore), "presentation never mutates the bag");

	xShop.Hide(xNoRoot);
	ZENITH_ASSERT_EQ(xShop.GetCursor(), -1, "a hidden screen selects nothing");
	ZENITH_ASSERT_EQ(xShop.GetInventoryCount(), 2u,
		"...but Hide does NOT drop the session stock (Reset owns that)");
	ZENITH_ASSERT_TRUE(BagsMatch(xBag, xBefore), "...and hiding never mutates the bag either");

	// ...and the model keeps working while nothing is drawn.
	u_int uMoney = 10000u;
	ZENITH_ASSERT_TRUE(xShop.Confirm(ZM_UI_Shop::szQTY_UP_NAME, xBag, uMoney) == ZM_SHOP_OK,
		"the quantity stepper keeps working while nothing is drawn");
	ZENITH_ASSERT_EQ(xShop.GetQuantity(), 2u, "...stepping to two");
}
