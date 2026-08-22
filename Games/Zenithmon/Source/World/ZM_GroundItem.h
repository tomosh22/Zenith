#pragma once

#include "Zenithmon/Source/Data/ZM_ItemData.h"   // ZM_ITEM_ID / ZM_ITEM_COUNT -- what a prop YIELDS

// The pickup path writes a ZM_GameState and owns none: forward-declared and taken
// by reference, so this header stays below Source/Party and the include graph has
// no cycle (ZM_GameState.h includes THIS file, for ZM_GroundItemSet below).
struct ZM_GameState;

// ============================================================================
// ZM_GroundItem (S8, ZM-D-196 ruling 1 / ZM-D-201) -- the ground-item PROP
// registry, the per-prop collected set, and the one pure pickup path.
//
// A "ground item" is a world prop that yields ONE item stack, ONCE, forever. This
// file owns the three things that are true of it regardless of where it stands:
// WHICH prop (a save-stable id), WHAT it yields (a compiled const row), and
// WHETHER this save has already taken it (a per-prop bit that persists).
//
// PURE. No ECS, no scene, no physics, no g_xEngine, no allocation, no I/O, no
// ZENITH_TOOLS guard -- so every rule below is unit-testable headlessly, in the
// same doctrine as ZM_InteractionLogic and ZM_StoryFlags.
//
// EVERY function here is TOTAL: no argument value, however garbage, is UB and none
// of them asserts. That is not squeamishness -- Zenith_Assert breaks the process in
// EVERY configuration in this engine (Zenith.h defines ZENITH_ASSERT
// unconditionally, so the debug-break definition always wins), and the whole unit
// suite runs at boot. The units feed these functions ZM_GROUND_ITEM_NONE and
// out-of-range ids ON PURPOSE to pin their fail-closed answers, so an assert on one
// of those inputs would not fail a test -- it would END the boot unit run.
//
// ★ WHAT THIS FILE DELIBERATELY DOES NOT OWN.
//   * WHERE a prop stands. A prop's world position needs a MEASURED ground column
//     (Source/World/ZM_Route1Placement.h, "THE GROUND"), and measuring one needs a
//     warm bake plus a windowed raycast oracle. Seeding coordinates by hand would
//     be exactly the invented-literal failure that table's banner forbids. The
//     anchors belong in the placement header of whichever scene authors the prop,
//     in the same slice that authors it.
//   * The ECS component that turns "the player pressed E at a prop" into a call to
//     ZM_TryPickUpGroundItem below. That is a ZM_Interactable-shaped component at
//     the next free game ECS order, and it must be registered in Zenithmon.cpp
//     TWICE (the ZENITH_REGISTER_COMPONENT macro AND the ZENITH_TOOLS editor "Add
//     Component" registry -- miss the second and it silently vanishes from the
//     editor menu). Booked, not done here; see ZM-D-201.
//
// ★ AND IT DOES NOT ADD A SECOND INVENTORY. Adding an item to the bag is
// ZM_Bag::Add and nothing more. This file holds no counts, no stacks and no
// pockets -- only the record of which PROPS have been taken.
// ============================================================================

// ---- The prop registry --------------------------------------------------------

// SAVE-STABLE. The enum VALUE *is* the id persisted by save module 12
// (Docs/SaveFormat.md). APPEND ONLY before ZM_GROUND_ITEM_COUNT; never reorder,
// never renumber, and never reuse a retired value -- any such reassignment
// silently changes which prop an existing save remembers taking, which is a
// versioned codec change and not an edit to this header. Debug names are NOT
// persisted, so RENAMING a prop is free while renumbering one is not.
//
// ★ THE THREE ROWS BELOW ARE STILL RE-NUMBERABLE **TODAY**, AND WILL NOT BE FOR
// LONG. Nothing authors a ground-item prop into a committed scene yet, so no save
// in existence can carry one of these ids, so a pre-content reshuffle costs only
// the v2 golden in Tests/ZM_Tests_SaveMigration.cpp (which pins ids 0 and 2). The
// day a scene authors one, the append-only rule above becomes binding for good.
//
// Unlike ZM_STORY_FLAG_ID, DENSITY IS NOT A STORAGE CONTRACT here: module 12 is a
// sparse ASCENDING ID LIST rather than a bitset sized from the highest set index,
// so a numeric gap costs nothing on the wire. Allocate densely anyway, because
// ZM_GroundItemSet below IS a dense array and a gap wastes a byte of the model per
// unused id -- a preference, not a rule.
enum ZM_GROUND_ITEM_ID : u_int
{
	ZM_GROUND_ITEM_ROUTE1_SOUTH_SALVE   = 0u,
	ZM_GROUND_ITEM_ROUTE1_LANE_CATCHORB = 1u,
	ZM_GROUND_ITEM_ROUTE1_NORTH_SALVE   = 2u,

	ZM_GROUND_ITEM_COUNT,
	ZM_GROUND_ITEM_NONE = ZM_GROUND_ITEM_COUNT   // "no prop" sentinel; NEVER persisted
};

// The registry can hold at most one entry per DISTINCT prop id, so
// ZM_GROUND_ITEM_COUNT is the hard upper bound on module 12's entryCount -- pinned
// here against the schema's 512-entry sanity cap (the same pin ZM_Bag.h puts on
// ZM_ITEM_COUNT) so appending props can never silently produce an unwritable save.
static_assert((u_int)ZM_GROUND_ITEM_COUNT <= 512u,
	"SaveFormat module 12 caps ground-item entries at 512; the prop registry has outgrown it");

// One registry row: which prop, what it is called in a log, and the ONE item stack
// it yields. The debug name is for logs, the editor and unit failure messages ONLY
// -- nothing persists it.
struct ZM_GroundItemInfo
{
	ZM_GROUND_ITEM_ID m_eId;
	const char*       m_szDebugName;
	// What the prop puts in the bag. A row whose item is out of range (including
	// ZM_ITEM_NONE, which IS ZM_ITEM_COUNT) is refused by ZM_Bag::CanAdd, so a
	// mis-authored row yields a prop nobody can take rather than a wrong item.
	ZM_ITEM_ID        m_eItem;
	// How many copies. PROVISIONAL placeholder tuning, in the same sense as
	// ZM_GameState.cpp's starting economy: real drop tuning is S11. A zero count is
	// likewise refused by CanAdd.
	u_int             m_uCount;
};

// TOTAL: an unregistered id (including the sentinel) yields the shared UNKNOWN row
// -- whose item and count BOTH fail ZM_Bag::CanAdd -- rather than a table read, and
// logs a non-fatal Zenith_Error because such a lookup means mis-authored data or a
// mis-typed caller. The ZM_GetStoryFlagInfo precedent, one for one.
const ZM_GroundItemInfo& ZM_GetGroundItemInfo(ZM_GROUND_ITEM_ID eId);

u_int ZM_GetGroundItemCount();   // == ZM_GROUND_ITEM_COUNT

// TOTAL: "NONE" for the sentinel, "UNKNOWN" out of range, and SILENT for both --
// naming a prop is what callers do WITH a bad id, not a second place to complain
// about it. Never returns null, because every caller is a log format argument.
const char* ZM_GroundItemName(ZM_GROUND_ITEM_ID eId);

// ---- The collected set --------------------------------------------------------

// Which props THIS SAVE has already taken, indexed by ZM_GROUND_ITEM_ID. Backed by
// a fixed bool array sized to the registry -- the ZM_SpeciesSet shape exactly --
// so ZM_GROUND_ITEM_NONE and any out-of-range id are ignored and callers never
// index out of bounds.
//
// ★ THERE IS NO PER-ID CLEAR, DELIBERATELY. A prop that has been taken stays taken
// for the life of the save; nothing in this game un-collects one. Clear() wipes the
// WHOLE set and exists for exactly two callers: a new game (which gets it by
// default construction) and the v1 -> v2 save migration, which must state the empty
// set rather than inherit it.
struct ZM_GroundItemSet
{
	bool m_abFlags[ZM_GROUND_ITEM_COUNT] = {};

	void Mark(ZM_GROUND_ITEM_ID eId)
	{
		if ((u_int)eId < (u_int)ZM_GROUND_ITEM_COUNT) { m_abFlags[(u_int)eId] = true; }
	}

	bool IsSet(ZM_GROUND_ITEM_ID eId) const
	{
		return (u_int)eId < (u_int)ZM_GROUND_ITEM_COUNT && m_abFlags[(u_int)eId];
	}

	u_int Count() const
	{
		u_int uCount = 0u;
		for (u_int u = 0u; u < (u_int)ZM_GROUND_ITEM_COUNT; ++u)
		{
			if (m_abFlags[u]) { ++uCount; }
		}
		return uCount;
	}

	void Clear()
	{
		for (u_int u = 0u; u < (u_int)ZM_GROUND_ITEM_COUNT; ++u) { m_abFlags[u] = false; }
	}
};

// ---- The pickup path ----------------------------------------------------------

// Why a pickup was refused. ZM_GROUND_ITEM_PICKUP_OK is the only success value.
// APPEND-ONLY; never reorder -- units assert on these values. Nothing persists
// them.
enum ZM_GROUND_ITEM_PICKUP : u_int
{
	ZM_GROUND_ITEM_PICKUP_OK = 0u,
	ZM_GROUND_ITEM_PICKUP_REJECT_UNKNOWN_PROP,        // no such registry row (covers the sentinel)
	ZM_GROUND_ITEM_PICKUP_REJECT_ALREADY_COLLECTED,   // this save has taken it already
	ZM_GROUND_ITEM_PICKUP_REJECT_BAG_REFUSED,         // ZM_Bag::CanAdd said no (full pocket / stack cap / bad row)

	// NOT a reason -- the walkable bound the name-totality unit iterates to.
	ZM_GROUND_ITEM_PICKUP_COUNT
};

// A stable short name for a pickup outcome, for log lines and unit failure
// messages. TOTAL: never returns nullptr, never indexes out of bounds ("UNKNOWN"
// otherwise).
const char* ZM_GroundItemPickupName(ZM_GROUND_ITEM_PICKUP eResult);

// Would ZM_TryPickUpGroundItem succeed RIGHT NOW? NON-MUTATING and const: this is
// the pickup's entire accept/reject rule with the mutation removed, and
// ZM_TryPickUpGroundItem itself calls it, so the two can never disagree. Same
// doctrine -- and for the same reason -- as ZM_Bag::CanAdd, whose header records
// what "SpendMoney then Add" cost the shop.
//
// Blocker precedence is FIXED and unit-pinned, in exactly this order, so the answer
// is stable no matter how many blockers hold at once:
//   1. id outside the registry  -> REJECT_UNKNOWN_PROP
//   2. already collected        -> REJECT_ALREADY_COLLECTED
//   3. !ZM_Bag::CanAdd          -> REJECT_BAG_REFUSED
//   4. otherwise                -> ZM_GROUND_ITEM_PICKUP_OK
//
// The collected test leads the bag test on purpose: a prop already taken must
// report that it is taken even when the bag is also full, or a player with a full
// pocket could never tell an exhausted prop from a temporary refusal.
ZM_GROUND_ITEM_PICKUP ZM_CanPickUpGroundItem(const ZM_GameState& xState,
	ZM_GROUND_ITEM_ID eId);

// Take the prop: put its stack in the bag and record that this save has it.
//
// ★★ THE ORDER IS THE WHOLE FUNCTION. Add FIRST, mark collected ONLY on success.
// "Mark collected then Add" destroys the item outright when the add is refused --
// the same shape as the "SpendMoney then Add" trap ZM_Bag.h exists to warn about,
// but strictly worse, because a lost coin can be re-earned and a burnt prop is gone
// for the life of the save.
//
// ALL-OR-NOTHING: every rejection leaves BOTH the bag and the collected set
// bit-identical. Returns the same value ZM_CanPickUpGroundItem would have returned
// for the pre-call state.
ZM_GROUND_ITEM_PICKUP ZM_TryPickUpGroundItem(ZM_GameState& xStateInOut,
	ZM_GROUND_ITEM_ID eId);
