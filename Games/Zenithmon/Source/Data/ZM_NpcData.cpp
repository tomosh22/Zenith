#include "Zenith.h"
#include "Zenithmon/Source/Data/ZM_NpcData.h"
#include "Zenithmon/Source/UI/ZM_UI_Shop.h"   // uMAX_INVENTORY, for the cap static_assert below

// The two row caps must track the UI limits they exist to respect. Both UI
// guards are ALL-OR-NOTHING (ZM_UI_DialogueBox::QueueLines and ZM_UI_Shop::Open
// each reject the WHOLE list when it is oversized), so a drifted cap does not
// truncate -- it makes the NPC silently mute or its shop refuse to open, with a
// table test still passing. Pin both at COMPILE time. The include lives in the
// .cpp so the data header keeps exactly one UI edge (the derived line cap).
static_assert(uZM_NPC_MAX_LINES == ZM_UI_DialogueBox::uMAX_QUEUED_LINES,
	"the row line cap must BE the dialogue queue capacity, not merely equal it today");
static_assert(uZM_NPC_MAX_STOCK <= ZM_UI_Shop::uMAX_INVENTORY,
	"a row's stock cap must fit the shop screen, which rejects an oversized list outright");

// ============================================================================
// ZM_NpcData -- the authored Dawnmere NPCs (S6 item 3 SC3; S7 item 2 SC1 adds the
// gated warden). Rows are in ZM_NPC_ID order; s_axNpcs[i].m_eId == i is asserted
// by the tests. Per-row line and stock arrays are static and referenced by
// pointer + count, the same shape ZM_WorldSpec uses for its per-scene tables.
// Column legend:
//   id, "display name", role, human, lines, lineCount, stock, stockCount,
//   wanders, lineGate, gatedLines, gatedLineCount
//
// The gate columns are the last three because the rows are POSITIONAL: a field
// inserted higher up shifts every value after it one column left, and the first
// casualty would be m_bWanders (the Wanderer would silently stop patrolling).
//
// The clerk's stock is deliberately the six cheap starting-town staples, and
// EVERY one of them is priced (m_uBuyPrice > 0) -- a zero-price row is NOT
// purchasable and ZM_ShopBuy would refuse it at the counter, so a clerk stocking
// one would advertise something the player can never buy. That is locked by
// Npc_EveryStockedItemIsPurchasable in Tests/ZM_Tests_NpcData.cpp.
// ============================================================================

namespace
{
#define ZM_ARRLEN(a) ((u_int)(sizeof(a) / sizeof((a)[0])))

	// -- dialogue lines (greetings only; see the header on Care Center text) --
	const char* const s_aszLinesVillager[] =
	{
		"Welcome to Dawnmere! Nothing much happens here.",
		"The tall grass past the gate hides wild creatures.",
		"Professor Aster's lab is just up the lane.",
	};

	const char* const s_aszLinesClerk[] =
	{
		"Welcome to the Dawnmere Trade Post!",
		"Orbs, salves and cures. Everything a trainer needs.",
	};

	const char* const s_aszLinesCaretaker[] =
	{
		"Welcome to the Dawnmere Care Center.",
		"Your creatures are safe with us.",
	};

	const char* const s_aszLinesWanderer[] =
	{
		"I walk this village end to end, every day.",
		"Keeps the legs honest. You should try it!",
		"The Trade Post has what a traveller needs.",
	};

	// The warden is the SC1 gating demonstration: two DIFFERENT sets of lines for
	// the same row, chosen by ZM_STORY_FLAG_WARDEN_CLEARED. The ordinary set is
	// what an open way sounds like; the gated set is the refusal the player hears
	// until the flag flips. Both must stay short -- the dialogue box's line cap is
	// cumulative across a whole conversation.
	//
	// He is written as a warden of the LANE, because that is where SC1 actually
	// stands him: (478, 498) sits about a metre off the authored Home walkway's
	// centreline and some 37 m from the Route polyline (Zenithmon.cpp derives both).
	// When a real route is authored, move him onto it and rewrite these to match.
	const char* const s_aszLinesWarden[] =
	{
		"Lane's mine to watch, and it's all clear through today.",
		"Safe travels. I'll be along this walk if you need me.",
	};

	const char* const s_aszLinesWardenGated[] =
	{
		"Hold up. I'm posted on this lane and the way on isn't open.",
		"Come back once the crews have signed it off. Warden's orders.",
	};

	// -- shop stock (SHOPKEEP rows only) --
	// The data is entirely compile-time, so an author who pastes a ninth line
	// should find out at BUILD time rather than at boot. The runtime unit stays --
	// it still covers rows added later, and rows whose array is not named here.
	static_assert(ZM_ARRLEN(s_aszLinesVillager)  <= uZM_NPC_MAX_LINES, "villager outgrew the dialogue queue");
	static_assert(ZM_ARRLEN(s_aszLinesClerk)     <= uZM_NPC_MAX_LINES, "clerk outgrew the dialogue queue");
	static_assert(ZM_ARRLEN(s_aszLinesCaretaker) <= uZM_NPC_MAX_LINES, "caretaker outgrew the dialogue queue");
	static_assert(ZM_ARRLEN(s_aszLinesWanderer)  <= uZM_NPC_MAX_LINES, "wanderer outgrew the dialogue queue");
	static_assert(ZM_ARRLEN(s_aszLinesWarden)      <= uZM_NPC_MAX_LINES, "warden outgrew the dialogue queue");
	static_assert(ZM_ARRLEN(s_aszLinesWardenGated) <= uZM_NPC_MAX_LINES, "warden refusal outgrew the dialogue queue");

	const ZM_ITEM_ID s_aeStockClerk[] =
	{
		ZM_ITEM_CATCHORB,
		ZM_ITEM_SALVE,
		ZM_ITEM_ANTITOXIN,
		ZM_ITEM_BURNBALM,
		ZM_ITEM_THAWSPRAY,
		ZM_ITEM_ROUSEBELL,
	};

	const ZM_NpcData s_axNpcs[ZM_NPC_COUNT] =
	{
		{ ZM_NPC_VILLAGER,         "Villager",  ZM_NPC_ROLE_TALKER,    ZM_HUMAN_TOWN_VILLAGER,  s_aszLinesVillager,  ZM_ARRLEN(s_aszLinesVillager),  nullptr,        0,                          false, { ZM_STORY_FLAG_NONE, true },            nullptr,                0u                                 },
		{ ZM_NPC_TRADE_POST_CLERK, "Clerk",     ZM_NPC_ROLE_SHOPKEEP,  ZM_HUMAN_TOWN_SHOPKEEP,  s_aszLinesClerk,     ZM_ARRLEN(s_aszLinesClerk),     s_aeStockClerk, ZM_ARRLEN(s_aeStockClerk),  false, { ZM_STORY_FLAG_NONE, true },            nullptr,                0u                                 },
		{ ZM_NPC_CARETAKER,        "Caretaker", ZM_NPC_ROLE_CARETAKER, ZM_HUMAN_TOWN_CARETAKER, s_aszLinesCaretaker, ZM_ARRLEN(s_aszLinesCaretaker), nullptr,        0,                          false, { ZM_STORY_FLAG_NONE, true },            nullptr,                0u                                 },
		{ ZM_NPC_WANDERER,         "Wanderer",  ZM_NPC_ROLE_TALKER,    ZM_HUMAN_TOWN_ELDER,     s_aszLinesWanderer,  ZM_ARRLEN(s_aszLinesWanderer),  nullptr,        0,                          true,  { ZM_STORY_FLAG_NONE, true },            nullptr,                0u                                 },
		{ ZM_NPC_ROUTE_WARDEN,     "Warden",    ZM_NPC_ROLE_TALKER,    ZM_HUMAN_TOWN_ELDER,     s_aszLinesWarden,    ZM_ARRLEN(s_aszLinesWarden),    nullptr,        0,                          false, { ZM_STORY_FLAG_WARDEN_CLEARED, true },  s_aszLinesWardenGated,  ZM_ARRLEN(s_aszLinesWardenGated)   },
	};

#undef ZM_ARRLEN
}

const ZM_NpcData& ZM_GetNpcData(ZM_NPC_ID eId)
{
	Zenith_Assert(eId < ZM_NPC_COUNT, "ZM_GetNpcData: npc id out of range (%u)", (u_int)eId);
	return s_axNpcs[eId];
}

u_int ZM_GetNpcCount()
{
	return ZM_NPC_COUNT;
}

void ZM_SelectNpcLines(const ZM_NpcData& xRow, const ZM_StoryFlagSet& xFlags,
	const char* const*& paszLinesOut, u_int& uCountOut)
{
	// A row with no gated set falls back to its ordinary lines even when the gate
	// fails, so half-authoring a gate mutes nobody -- it just does nothing.
	const bool bHasGatedSet = xRow.m_paszGatedLines != nullptr && xRow.m_uGatedLineCount > 0u;
	const bool bUseGated = bHasGatedSet && !ZM_StoryGatePasses(xRow.m_xLineGate, xFlags);

	paszLinesOut = bUseGated ? xRow.m_paszGatedLines : xRow.m_paszLines;
	uCountOut = bUseGated ? xRow.m_uGatedLineCount : xRow.m_uLineCount;

	// The CLAMP guards ZM_UI_DialogueBox::QueueLines, which is all-or-nothing: an
	// oversized count is REJECTED WHOLE rather than truncated, so an over-cap row
	// would go completely mute instead of losing its tail. Eight lines beat none.
	//
	// The NULL guard is NOT a crash guard. QueueLines tests the array pointer as the
	// first disjunct of its first check and refuses the push, so a (null, non-zero)
	// pair already ends as a mute NPC there, not a dereference. It is here because
	// the selector owes its callers a pair that does not contradict itself: without
	// it the clamp below would validate the count and leave the pointer bogus, and
	// the guarantee would be QueueLines' property rather than this function's.
	if (paszLinesOut == nullptr)
	{
		uCountOut = 0u;
	}
	else if (uCountOut > uZM_NPC_MAX_LINES)
	{
		uCountOut = uZM_NPC_MAX_LINES;
	}
}
