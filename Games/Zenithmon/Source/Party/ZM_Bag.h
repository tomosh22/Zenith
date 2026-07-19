#pragma once

#include "Zenithmon/Source/Data/ZM_ItemData.h"   // ZM_ITEM_ID / ZM_ITEM_CATEGORY / ZM_GetItemData

// ============================================================================
// ZM_Bag -- the player's item bag (S6 item 2 SC3): one fixed-capacity pocket per
// ZM_ITEM_CATEGORY, each holding { item id, count } stacks kept sorted ASCENDING
// by ZM_ITEM_ID. A plain aggregate -- no heap, no ECS, no I/O, no UI. The SC6 Bag
// screen renders a pocket straight off PocketStack(), and the sorted order is what
// gives it a deterministic row order without a sort pass of its own.
//
// The shape mirrors SaveFormat.md module 6 one-for-one (entryCount, then
// { itemId, count } entries with count >= 1), so S7 serializes it without a
// re-shape. Every mutator is ALL-OR-NOTHING: a rejected Add/Remove leaves the bag
// bit-identical, because SC7's shop transaction deducts money and then calls Add()
// -- a partial add would silently destroy the player's money.
// ============================================================================

// One stored stack == one module-6 entry. A STORED stack always has m_uCount >= 1;
// Remove erases a stack that reaches zero rather than parking a count-0 entry (the
// save schema forbids writing those).
struct ZM_ItemStack
{
	ZM_ITEM_ID m_eItem  = ZM_ITEM_NONE;
	u_int      m_uCount = 0u;
};

// Per-pocket capacity. A pocket must hold EVERY item of its category at once, so
// this is >= the largest per-category item count in the table (the largest today is
// TM at 25). ZM_Tests_Bag walks the table and fails loudly if content growth ever
// pushes a category past this -- that test is what keeps "pocket full" unreachable.
static constexpr u_int uZM_BAG_MAX_STACKS_PER_POCKET = 64u;

// Per-stack cap: a single item id never exceeds this many copies.
static constexpr u_int uZM_BAG_MAX_STACK_COUNT = 999u;

// The bag can hold at most one stack per DISTINCT item id, so ZM_ITEM_COUNT is the
// hard upper bound on module 6's entryCount -- pinned here against the schema's
// 512-entry sanity cap so appending items can never silently produce an unwritable
// save.
static_assert((u_int)ZM_ITEM_COUNT <= 512u,
	"SaveFormat module 6 caps bag entries at 512; the item table has outgrown it");

struct ZM_Bag
{
	ZM_ItemStack m_axPocket[ZM_ITEM_CATEGORY_COUNT][uZM_BAG_MAX_STACKS_PER_POCKET];
	u_int        m_auPocketCount[ZM_ITEM_CATEGORY_COUNT] = {};

	void Clear();

	// Add uCount copies of eItem. Rejects (strict no-op, returns false) an id at or
	// past ZM_ITEM_COUNT -- which covers ZM_ITEM_NONE, since NONE IS ZM_ITEM_COUNT --
	// a zero count, a resulting stack past uZM_BAG_MAX_STACK_COUNT, and a new stack
	// in a full pocket. Otherwise stacks onto the existing entry, or inserts a new
	// one keeping the pocket ascending by id.
	bool Add(ZM_ITEM_ID eItem, u_int uCount);

	// Remove uCount copies. Rejects (strict no-op) an out-of-range id, a zero count,
	// or removing more than is held. A stack that reaches zero is ERASED.
	bool Remove(ZM_ITEM_ID eItem, u_int uCount);

	u_int GetCount(ZM_ITEM_ID eItem) const;
	bool  Has(ZM_ITEM_ID eItem) const { return GetCount(eItem) > 0u; }

	u_int PocketStackCount(ZM_ITEM_CATEGORY eCategory) const;

	// Bounds-guarded read. An out-of-range category or index yields a shared empty
	// stack -- never an out-of-range element, never a dangling temporary.
	const ZM_ItemStack& PocketStack(ZM_ITEM_CATEGORY eCategory, u_int uIndex) const;

	// Total stored stacks across every pocket == SaveFormat module 6 entryCount.
	u_int TotalStackCount() const;
};
