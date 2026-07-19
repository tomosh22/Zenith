#pragma once

#include "Zenithmon/Source/Data/ZM_ItemData.h"   // ZM_ITEM_ID / ZM_GetItemData (the price source)

struct ZM_Bag;

// ============================================================================
// ZM_ShopLogic (S6 item 2 SC7) -- the pure buy/sell transaction rules. Free
// functions over a bag and a money balance: NO ECS, NO UI, NO I/O, no statics.
// ZM_UI_Shop is only a presenter over this; every rule that decides whether the
// player's money moves lives HERE so it can be unit-tested without a scene.
//
// Prices come from the ITEM TABLE (ZM_GetItemData), never from a literal: a row's
// m_uBuyPrice of 0 means "not purchasable" and its m_uSellPrice of 0 means "not
// sellable", which is how every KEY item (Badge Case, Region Map, Angle Rod, ...)
// is protected -- they are all priced 0/0.
//
// The ordering inside ZM_ShopBuy is the load-bearing part of this file: the bag is
// asked whether it CAN take the goods BEFORE the money is deducted. ZM_Bag::Add is
// all-or-nothing and rejects when a stack would pass uZM_BAG_MAX_STACK_COUNT, so the
// naive "spend then add" sequence can take the money and deliver nothing. On ANY
// error NOTHING is mutated -- not the bag, not the balance.
// ============================================================================

// Why a transaction was refused. ZM_SHOP_OK is the only success value; every other
// enumerator means the bag and the money were left BIT-IDENTICAL.
enum ZM_SHOP_RESULT : u_int
{
	ZM_SHOP_OK = 0u,
	ZM_SHOP_ERR_INVALID_ITEM,       // id at or past ZM_ITEM_COUNT (covers ZM_ITEM_NONE)
	ZM_SHOP_ERR_INVALID_QUANTITY,   // quantity 0
	ZM_SHOP_ERR_NOT_PURCHASABLE,    // m_uBuyPrice == 0
	ZM_SHOP_ERR_NOT_SELLABLE,       // m_uSellPrice == 0 (covers every KEY item)
	ZM_SHOP_ERR_CANNOT_AFFORD,      // the total exceeds the balance (or overflows a u_int)
	ZM_SHOP_ERR_NOT_ENOUGH_HELD,    // the bag holds fewer than the requested count
	ZM_SHOP_ERR_NO_BAG_ROOM,        // the bag would reject the Add (per-stack / per-pocket cap)
	ZM_SHOP_ERR_MONEY_CAPPED,       // the sale credit would be lost at the money cap

	// NOT part of the spoken result set -- the walkable bound the formatter totality
	// test iterates to. APPEND before this, never reorder.
	ZM_SHOP_RESULT_COUNT
};

// The table's buy price for eItem; 0 for an out-of-range id AND for every item that
// is not purchasable, so a caller may treat "0" as "the shop cannot sell this".
u_int ZM_ShopBuyPrice(ZM_ITEM_ID eItem);
// ...and the sell price; 0 means "the shop will not buy this" (key items).
u_int ZM_ShopSellPrice(ZM_ITEM_ID eItem);

// Buy uQty copies of eItem, paying out of uMoneyInOut. Guard -> CanAdd -> deduct ->
// Add, in that order: no path may take money without delivering the items. Returns
// ZM_SHOP_OK on success (money debited by price * qty, bag credited by uQty), and on
// every other result leaves BOTH arguments untouched.
ZM_SHOP_RESULT ZM_ShopBuy(ZM_Bag& xBag, u_int& uMoneyInOut, ZM_ITEM_ID eItem, u_int uQty);

// Sell uQty copies of eItem, crediting uMoneyInOut. The mirror, including the
// credit-loss guard: a purse at uZM_MONEY_CAP would silently swallow the payment
// while the items were gone, so a credit that does not fit IN FULL is refused
// (ZM_SHOP_ERR_MONEY_CAPPED) with nothing mutated.
ZM_SHOP_RESULT ZM_ShopSell(ZM_Bag& xBag, u_int& uMoneyInOut, ZM_ITEM_ID eItem, u_int uQty);
