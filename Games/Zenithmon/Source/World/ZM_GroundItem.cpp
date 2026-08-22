#include "Zenith.h"

#include "Zenithmon/Source/World/ZM_GroundItem.h"

#include "Zenithmon/Source/Party/ZM_GameState.h"   // ZM_GameState -- the bag and the collected set

// ============================================================================
// ZM_GroundItem -- see the header for the contract. Data is a compiled const C
// array (ZM-D-009); the accessors are bounds-guarded free functions; the pickup
// path is two functions that share ONE accept rule.
// ============================================================================

namespace
{
	// ★ ROW ORDER **IS** ZM_GROUND_ITEM_ID ORDER, and that is contract, not tidiness:
	// the accessor indexes this array by the id, and save module 12 persists the id.
	// A reorder here silently re-points every existing save's collected bits.
	// ZM_Tests_GroundItem walks the table and fails if any row's m_eId stops matching
	// its own index.
	//
	// ★ THE ITEMS AND COUNTS ARE PROVISIONAL PLACEHOLDER TUNING, exactly as
	// ZM_GameState.cpp's starting economy is: enough to be a real pickup without
	// pre-empting S11's drop tuning. The two items are the two ZM_MakeNewGameState
	// already grants, so a route pickup is a top-up of something the player already
	// understands rather than new content that has to be explained.
	constexpr ZM_GroundItemInfo axZM_GROUND_ITEMS[] =
	{
		// id,                                  debug name,           item,             count
		{ ZM_GROUND_ITEM_ROUTE1_SOUTH_SALVE,   "Route1SouthSalve",   ZM_ITEM_SALVE,    1u },
		{ ZM_GROUND_ITEM_ROUTE1_LANE_CATCHORB, "Route1LaneCatchorb", ZM_ITEM_CATCHORB, 2u },
		{ ZM_GROUND_ITEM_ROUTE1_NORTH_SALVE,   "Route1NorthSalve",   ZM_ITEM_SALVE,    1u },
	};

	// The bound is DEDUCED, never spelled. With an explicit
	// [ZM_GROUND_ITEM_COUNT] this static_assert would be a tautology and a forgotten
	// row would merely zero-initialise the tail into a NULL-named prop yielding item
	// 0 -- a Catchorb -- which is a plausible-looking row nothing could see.
	static_assert(sizeof(axZM_GROUND_ITEMS) / sizeof(axZM_GROUND_ITEMS[0])
		== (u_int)ZM_GROUND_ITEM_COUNT,
		"the ground-item table must have exactly one row per ZM_GROUND_ITEM_ID");

	// The answer for an id no row covers. DOUBLY INERT rather than plausible:
	// ZM_ITEM_NONE is out of the item table's range AND the count is zero, and
	// ZM_Bag::CanAdd refuses either independently -- so a caller that skipped the
	// range check gets a prop that can never be taken, never a free Catchorb.
	constexpr ZM_GroundItemInfo xZM_GROUND_ITEM_UNKNOWN =
	{
		ZM_GROUND_ITEM_NONE, "UNKNOWN", ZM_ITEM_NONE, 0u
	};
}

const ZM_GroundItemInfo& ZM_GetGroundItemInfo(ZM_GROUND_ITEM_ID eId)
{
	if ((u_int)eId >= (u_int)ZM_GROUND_ITEM_COUNT)
	{
		// Non-fatal and LOUD: a row lookup on an id the registry does not carry means
		// mis-authored data or a mis-typed caller, and Shortfalls 1.6 forbids a silent
		// no-op there. NOT Zenith_Assert -- see the header: the boot units drive this
		// branch on purpose and a debug break would end the run rather than fail a test.
		Zenith_Error(LOG_CATEGORY_GAMEPLAY,
			"[ZM_GroundItem] no registry row for ground-item id %u (registry holds %u)",
			(u_int)eId, (u_int)ZM_GROUND_ITEM_COUNT);
		return xZM_GROUND_ITEM_UNKNOWN;
	}
	return axZM_GROUND_ITEMS[(u_int)eId];
}

u_int ZM_GetGroundItemCount()
{
	return (u_int)ZM_GROUND_ITEM_COUNT;
}

const char* ZM_GroundItemName(ZM_GROUND_ITEM_ID eId)
{
	// SILENT, unlike ZM_GetGroundItemInfo above. This function's callers are log
	// format arguments and assertion messages, so complaining here would double every
	// diagnostic the caller is already emitting.
	if (eId == ZM_GROUND_ITEM_NONE) { return "NONE"; }
	if ((u_int)eId >= (u_int)ZM_GROUND_ITEM_COUNT) { return "UNKNOWN"; }
	return axZM_GROUND_ITEMS[(u_int)eId].m_szDebugName;
}

const char* ZM_GroundItemPickupName(ZM_GROUND_ITEM_PICKUP eResult)
{
	switch (eResult)
	{
	case ZM_GROUND_ITEM_PICKUP_OK:                       return "OK";
	case ZM_GROUND_ITEM_PICKUP_REJECT_UNKNOWN_PROP:      return "UNKNOWN_PROP";
	case ZM_GROUND_ITEM_PICKUP_REJECT_ALREADY_COLLECTED: return "ALREADY_COLLECTED";
	case ZM_GROUND_ITEM_PICKUP_REJECT_BAG_REFUSED:       return "BAG_REFUSED";
	// A switch, not a table lookup, precisely so ZM_GROUND_ITEM_PICKUP_COUNT and
	// anything past it land here instead of reading off the end of an array.
	default:                                             return "UNKNOWN";
	}
}

ZM_GROUND_ITEM_PICKUP ZM_CanPickUpGroundItem(const ZM_GameState& xState,
	ZM_GROUND_ITEM_ID eId)
{
	// 1. The range check LEADS, and it is what keeps this function silent: it returns
	// before ZM_GetGroundItemInfo can log its (correct, but here unwanted) error for
	// an id a caller is merely ASKING about.
	if ((u_int)eId >= (u_int)ZM_GROUND_ITEM_COUNT)
	{
		return ZM_GROUND_ITEM_PICKUP_REJECT_UNKNOWN_PROP;
	}

	// 2. Already taken outranks a full bag -- see the header. An exhausted prop must
	// say so whatever else is true, or a player with a full pocket cannot tell "I
	// already have this" from "come back with room".
	if (xState.m_xCollectedGroundItems.IsSet(eId))
	{
		return ZM_GROUND_ITEM_PICKUP_REJECT_ALREADY_COLLECTED;
	}

	// 3. The bag's OWN rule, asked rather than re-implemented. A second copy of "is
	// there room" here could drift from ZM_Bag::Add's accept rule, and the drift would
	// surface as a prop that burns itself and delivers nothing.
	const ZM_GroundItemInfo& xRow = axZM_GROUND_ITEMS[(u_int)eId];
	if (!xState.m_xBag.CanAdd(xRow.m_eItem, xRow.m_uCount))
	{
		return ZM_GROUND_ITEM_PICKUP_REJECT_BAG_REFUSED;
	}

	return ZM_GROUND_ITEM_PICKUP_OK;
}

ZM_GROUND_ITEM_PICKUP ZM_TryPickUpGroundItem(ZM_GameState& xStateInOut,
	ZM_GROUND_ITEM_ID eId)
{
	// The accept/reject rule lives in exactly ONE place -- this IS
	// ZM_CanPickUpGroundItem plus the mutation, so the predicate a UI or an
	// interaction gate leans on can never drift from what the mutator actually does.
	const ZM_GROUND_ITEM_PICKUP eResult = ZM_CanPickUpGroundItem(xStateInOut, eId);
	if (eResult != ZM_GROUND_ITEM_PICKUP_OK)
	{
		return eResult;
	}

	const ZM_GroundItemInfo& xRow = axZM_GROUND_ITEMS[(u_int)eId];

	// ★★ ADD FIRST. The mark below happens ONLY on a successful add.
	//
	// This branch is unreachable while CanAdd and Add share their rule (Add IS CanAdd
	// plus the mutation), and it is written out anyway rather than asserted: if the
	// two ever DID drift, the failure this shape prevents is a prop that records
	// itself as taken while the player got nothing -- unrecoverable for the life of
	// the save, and invisible to every compiled-constant unit.
	if (!xStateInOut.m_xBag.Add(xRow.m_eItem, xRow.m_uCount))
	{
		return ZM_GROUND_ITEM_PICKUP_REJECT_BAG_REFUSED;
	}

	xStateInOut.m_xCollectedGroundItems.Mark(eId);
	return ZM_GROUND_ITEM_PICKUP_OK;
}
