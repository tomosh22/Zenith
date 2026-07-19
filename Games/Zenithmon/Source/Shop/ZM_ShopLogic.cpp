#include "Zenith.h"

#include "Zenithmon/Source/Shop/ZM_ShopLogic.h"

#include "Zenithmon/Source/Party/ZM_Bag.h"         // ZM_Bag::CanAdd / Add / Remove / GetCount
#include "Zenithmon/Source/Party/ZM_GameState.h"   // uZM_MONEY_CAP (the ONE money ceiling)

// ============================================================================
// ZM_ShopLogic (S6 item 2 SC7). See the header for the contract; this file is the
// ordering. Nothing here touches the ECS, the UI or the disk.
// ============================================================================

namespace
{
	// The u_int ceiling, spelled once. price * qty is checked against it BEFORE the
	// multiply, so an absurd quantity can never wrap the total into a "free" purchase.
	constexpr u_int uZM_UINT_MAX = static_cast<u_int>(-1);
}

u_int ZM_ShopBuyPrice(ZM_ITEM_ID eItem)
{
	// ZM_GetItemData is bounds-ASSERTED, so the range test comes first. NONE is DEFINED
	// as ZM_ITEM_COUNT, so this one check covers it too.
	if ((u_int)eItem >= (u_int)ZM_ITEM_COUNT) { return 0u; }
	return ZM_GetItemData(eItem).m_uBuyPrice;
}

u_int ZM_ShopSellPrice(ZM_ITEM_ID eItem)
{
	if ((u_int)eItem >= (u_int)ZM_ITEM_COUNT) { return 0u; }
	return ZM_GetItemData(eItem).m_uSellPrice;
}

ZM_SHOP_RESULT ZM_ShopBuy(ZM_Bag& xBag, u_int& uMoneyInOut, ZM_ITEM_ID eItem, u_int uQty)
{
	if ((u_int)eItem >= (u_int)ZM_ITEM_COUNT) { return ZM_SHOP_ERR_INVALID_ITEM; }
	if (uQty == 0u) { return ZM_SHOP_ERR_INVALID_QUANTITY; }

	// The price-0 guard is NOT cosmetic: every key item (Badge Case, Region Map, Angle
	// Rod, ...) is priced 0, so without it a shop stocking one would hand it out FREE.
	const u_int uPrice = ZM_ShopBuyPrice(eItem);
	if (uPrice == 0u) { return ZM_SHOP_ERR_NOT_PURCHASABLE; }

	// Headroom-first (the ZM_Bag::Add / ZM_GameState::AddMoney idiom): a total that
	// cannot even be REPRESENTED is, for any real purse, unaffordable -- and computing
	// it first would wrap it small enough to look affordable.
	if (uQty > uZM_UINT_MAX / uPrice) { return ZM_SHOP_ERR_CANNOT_AFFORD; }
	const u_int uTotal = uPrice * uQty;
	if (uTotal > uMoneyInOut) { return ZM_SHOP_ERR_CANNOT_AFFORD; }

	// ASK THE BAG BEFORE TAKING THE MONEY. ZM_Bag::Add is all-or-nothing and refuses a
	// stack past uZM_BAG_MAX_STACK_COUNT (or a new stack in a full pocket), so the
	// "SpendMoney then Add" order would debit the player for goods never delivered.
	if (!xBag.CanAdd(eItem, uQty)) { return ZM_SHOP_ERR_NO_BAG_ROOM; }

	uMoneyInOut -= uTotal;
	const bool bAdded = xBag.Add(eItem, uQty);
	// Unreachable: ZM_Bag::Add IS ZM_Bag::CanAdd plus the mutation, and nothing can
	// touch the bag between the two calls. Asserted rather than handled, because a
	// false here would mean the money above was taken for nothing.
	Zenith_Assert(bAdded,
		"ZM_ShopBuy: the bag rejected an Add that CanAdd accepted (item %u, qty %u)",
		(u_int)eItem, uQty);
	(void)bAdded;
	return ZM_SHOP_OK;
}

ZM_SHOP_RESULT ZM_ShopSell(ZM_Bag& xBag, u_int& uMoneyInOut, ZM_ITEM_ID eItem, u_int uQty)
{
	if ((u_int)eItem >= (u_int)ZM_ITEM_COUNT) { return ZM_SHOP_ERR_INVALID_ITEM; }
	if (uQty == 0u) { return ZM_SHOP_ERR_INVALID_QUANTITY; }

	// The mirror of the buy-price guard, and what actually protects the key items: a
	// sell price of 0 means the shop will not take it at any quantity.
	const u_int uPrice = ZM_ShopSellPrice(eItem);
	if (uPrice == 0u) { return ZM_SHOP_ERR_NOT_SELLABLE; }

	if (xBag.GetCount(eItem) < uQty) { return ZM_SHOP_ERR_NOT_ENOUGH_HELD; }

	// The same overflow care as the buy side. A credit that cannot be represented
	// certainly cannot fit under the cap, so it takes the capped exit.
	if (uQty > uZM_UINT_MAX / uPrice) { return ZM_SHOP_ERR_MONEY_CAPPED; }
	const u_int uCredit = uPrice * uQty;

	// A purse at (or above) the cap would swallow the payment while the items were
	// gone. The rule is STRICT: unless the FULL credit fits, the sale is refused and
	// nothing is mutated -- a partial payment for a whole stack is exactly the silent
	// loss this guard exists to prevent.
	const u_int uHeadroom = (uMoneyInOut < uZM_MONEY_CAP) ? (uZM_MONEY_CAP - uMoneyInOut) : 0u;
	if (uCredit > uHeadroom) { return ZM_SHOP_ERR_MONEY_CAPPED; }

	const bool bRemoved = xBag.Remove(eItem, uQty);
	// Unreachable: GetCount >= uQty was just checked and nothing can touch the bag in
	// between. Asserted because a false here would pay for goods never taken.
	Zenith_Assert(bRemoved,
		"ZM_ShopSell: the bag rejected a Remove of %u x item %u it reported holding",
		uQty, (u_int)eItem);
	(void)bRemoved;
	uMoneyInOut += uCredit;
	return ZM_SHOP_OK;
}
