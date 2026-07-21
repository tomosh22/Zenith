#include "Zenith.h"

// ============================================================================
// ZM_Tests_NpcData -- integrity of the authored NPC roster (category ZM_Data),
// S6 item 3 SC3. The table is CONTENT, so every failure mode here is an
// authoring mistake that would otherwise surface as a silently broken NPC in the
// world: a mute talker, an empty shop, a clerk stocking something the counter
// refuses to sell, a role arm with no content behind it.
//
// Everything is PURE: a compiled const table plus the pure price accessor. No
// ECS, no scene, no UI, no baked assets -- so no RequestSkip is needed.
//
// The two "walk everything" units (lines, stocked items) GUARD their walk with a
// non-zero total first: a loop bounded by a count that is itself zero passes
// vacuously and would keep passing after the content it exists to police was
// deleted.
// ============================================================================

#include <cstring>   // strcmp (display-name distinctness)

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/Data/ZM_HumanData.h"
#include "Zenithmon/Source/Data/ZM_ItemData.h"
#include "Zenithmon/Source/Data/ZM_NpcData.h"
#include "Zenithmon/Source/Data/ZM_StoryFlags.h"   // the gate a row's alternate line set hangs off
#include "Zenithmon/Source/Shop/ZM_ShopLogic.h"    // ZM_ShopBuyPrice -- the purchasable check
#include "Zenithmon/Source/UI/ZM_UI_DialogueBox.h" // uMAX_QUEUED_LINES -- the line cap's source

namespace
{
	// Spelled in the TEST, not read back off the table, so "the roster changed"
	// is a failure rather than a silently-agreeing tautology.
	constexpr u_int uEXPECTED_NPCS = 5;

	const ZM_NpcData& Npc(u_int i) { return ZM_GetNpcData((ZM_NPC_ID)i); }

	// Every rows-x-stock walk opens by asserting this is non-zero. Without it such
	// a walk runs ZERO iterations -- and so keeps passing -- the moment the stock
	// it exists to police is deleted.
	u_int TotalStock()
	{
		u_int uTotal = 0;
		for (u_int i = 0; i < ZM_NPC_COUNT; ++i)
		{
			uTotal += Npc(i).m_uStockCount;
		}
		return uTotal;
	}

	// ---- S7 item 2 SC1: flag-gated line sets -------------------------------

	bool RowIsGated(const ZM_NpcData& x)
	{
		return x.m_xLineGate.m_eFlag != ZM_STORY_FLAG_NONE;
	}

	u_int GatedRowCount()
	{
		u_int uTotal = 0;
		for (u_int i = 0; i < ZM_NPC_COUNT; ++i)
		{
			if (RowIsGated(Npc(i))) { ++uTotal; }
		}
		return uTotal;
	}

	// Same guard idiom as TotalStock: every rows-x-gated-lines walk opens by
	// asserting this is non-zero, because such a walk runs ZERO iterations -- and
	// so keeps passing -- the moment the refusal content it polices is deleted.
	u_int TotalGatedLines()
	{
		u_int uTotal = 0;
		for (u_int i = 0; i < ZM_NPC_COUNT; ++i)
		{
			uTotal += Npc(i).m_uGatedLineCount;
		}
		return uTotal;
	}

	// The first roster row carrying a real gate, or ZM_NPC_COUNT when there is none.
	// Callers REPORT the empty case rather than walking on -- the assert macros
	// continue, so an unguarded caller would index the table out of range.
	u_int FirstGatedRowIndex()
	{
		for (u_int i = 0; i < ZM_NPC_COUNT; ++i)
		{
			if (RowIsGated(Npc(i))) { return i; }
		}
		return ZM_NPC_COUNT;
	}

	// Flag-set fixtures are built through ZM_StoryFlagSet's OWN raw-index setter
	// rather than ZM_SetStoryFlag, so a selector unit cannot inherit -- and then
	// agree with -- a bug in the accessor sitting next to it.
	ZM_StoryFlagSet MakeGatePassingFlags(const ZM_NpcData& x)
	{
		ZM_StoryFlagSet xFlags;
		if (x.m_xLineGate.m_bRequireSet)
		{
			xFlags.Set((u_int)x.m_xLineGate.m_eFlag, true);
		}
		return xFlags;
	}

	ZM_StoryFlagSet MakeGateFailingFlags(const ZM_NpcData& x)
	{
		ZM_StoryFlagSet xFlags;
		if (!x.m_xLineGate.m_bRequireSet)
		{
			xFlags.Set((u_int)x.m_xLineGate.m_eFlag, true);
		}
		return xFlags;
	}

	ZM_StoryFlagSet MakeAllRegisteredFlagsSet()
	{
		ZM_StoryFlagSet xFlags;
		for (u_int u = 0; u < (u_int)ZM_STORY_FLAG_COUNT; ++u)
		{
			xFlags.Set(u, true);
		}
		return xFlags;
	}
}

// The roster size agrees with the enum AND with the count this suite was written
// against; a row added to the table without an enumerator (or the reverse) fails.
ZENITH_TEST(ZM_Data, Npc_CountMatchesEnum)
{
	ZENITH_ASSERT_EQ(ZM_GetNpcCount(), uEXPECTED_NPCS);
	ZENITH_ASSERT_EQ((u_int)ZM_NPC_COUNT, uEXPECTED_NPCS);
	ZENITH_ASSERT_EQ((u_int)ZM_NPC_NONE, (u_int)ZM_NPC_COUNT);
}

// Row index == m_eId. Reordering rows without renumbering would silently
// mis-address every NPC (SC5 authors by id, SC4 dispatches on the row it fetched).
ZENITH_TEST(ZM_Data, Npc_EveryRowIdMatchesItsIndex)
{
	for (u_int i = 0; i < ZM_NPC_COUNT; ++i)
	{
		ZENITH_ASSERT_EQ((u_int)Npc(i).m_eId, i, "npc row %u has mismatched m_eId", i);
	}
}

ZENITH_TEST(ZM_Data, Npc_DisplayNamesNonEmptyAndUnique)
{
	// Null/empty FIRST, across every row, before any strcmp. The assert macros
	// record a failure without aborting the body, so a single interleaved loop
	// would strcmp row j's name before j had been null-checked -- and an enumerator
	// added with no matching row zero-inits to null, turning a named test failure
	// into a hard UB crash during units-at-boot.
	for (u_int i = 0; i < ZM_NPC_COUNT; ++i)
	{
		const char* sz = Npc(i).m_szDisplayName;
		ZENITH_ASSERT_NOT_NULL(sz);
		if (sz != nullptr)
		{
			ZENITH_ASSERT_TRUE(sz[0] != '\0', "npc %u has an empty display name", i);
		}
	}

	for (u_int i = 0; i < ZM_NPC_COUNT; ++i)
	{
		const char* szA = Npc(i).m_szDisplayName;
		for (u_int j = i + 1; j < ZM_NPC_COUNT; ++j)
		{
			const char* szB = Npc(j).m_szDisplayName;
			if ((szA == nullptr) || (szB == nullptr))
			{
				continue;   // already reported above
			}
			ZENITH_ASSERT_FALSE(strcmp(szA, szB) == 0,
				"duplicate npc display name '%s' at %u and %u", szA, i, j);
		}
	}
}

// An out-of-range role is a row SC4's dispatch switch would fall straight
// through -- an NPC that consumes the interact press and does nothing.
ZENITH_TEST(ZM_Data, Npc_EveryRoleIsInRange)
{
	for (u_int i = 0; i < ZM_NPC_COUNT; ++i)
	{
		ZENITH_ASSERT_LT((u_int)Npc(i).m_eRole, (u_int)ZM_NPC_ROLE_COUNT,
			"%s has a role outside the dispatch range", Npc(i).m_szDisplayName);
	}
}

// A row naming a deleted / never-authored appearance would spawn nothing visible.
ZENITH_TEST(ZM_Data, Npc_EveryHumanIdIsInRange)
{
	for (u_int i = 0; i < ZM_NPC_COUNT; ++i)
	{
		ZENITH_ASSERT_LT((u_int)Npc(i).m_eHuman, (u_int)ZM_HUMAN_COUNT,
			"%s names a human id outside the roster", Npc(i).m_szDisplayName);
	}
}

// Asserted against the DIALOGUE BOX's own capacity, never a literal: QueueLines
// is all-or-nothing, so a row that outgrew the queue leaves its NPC MUTE rather
// than merely truncated.
ZENITH_TEST(ZM_Data, Npc_LineCountWithinDialogueQueueCap)
{
	for (u_int i = 0; i < ZM_NPC_COUNT; ++i)
	{
		// Assert against the NAMED cap, not the UI constant directly -- that is what
		// gives uZM_NPC_MAX_LINES a consumer. It is DEFINED as the queue capacity
		// (and static_assert'd to it in ZM_NpcData.cpp), so this is still a
		// derivation and the literal 8 is still spelled nowhere.
		ZENITH_ASSERT_LE(Npc(i).m_uLineCount, uZM_NPC_MAX_LINES,
			"%s queues more lines than the dialogue box accepts", Npc(i).m_szDisplayName);
	}
}

// QueueLines also rejects a null / empty entry, and rejects the WHOLE batch when
// it does. The walk is guarded so it can never pass vacuously.
ZENITH_TEST(ZM_Data, Npc_EveryLineIsNonNullAndNonEmpty)
{
	u_int uTotalLines = 0;
	for (u_int i = 0; i < ZM_NPC_COUNT; ++i)
	{
		uTotalLines += Npc(i).m_uLineCount;
	}
	ZENITH_ASSERT_GT(uTotalLines, 0u, "the roster authors no dialogue at all");

	for (u_int i = 0; i < ZM_NPC_COUNT; ++i)
	{
		const ZM_NpcData& x = Npc(i);
		if (x.m_uLineCount > 0)
		{
			ZENITH_ASSERT_NOT_NULL(x.m_paszLines, "%s claims lines but has no array", x.m_szDisplayName);
		}
		for (u_int uLine = 0; uLine < x.m_uLineCount; ++uLine)
		{
			const char* szLine = x.m_paszLines[uLine];
			ZENITH_ASSERT_NOT_NULL(szLine, "%s line %u is null", x.m_szDisplayName, uLine);
			ZENITH_ASSERT_TRUE(szLine[0] != '\0', "%s line %u is empty", x.m_szDisplayName, uLine);
		}
	}
}

// An NPC with no lines is scenery that eats the interact press. Deliberately
// UNCONDITIONAL rather than guarded on ZM_NPC_ROLE_TALKER: a role-guarded walk
// passes vacuously if no row is a talker (leaving this unit dependent on
// Npc_RolesCoverEveryDispatchArm keeping one alive), and a zero-line CARETAKER
// or SHOPKEEP is just as broken -- QueueLines rejects uCount == 0 for EVERY
// role, so all three arms feed it a greeting.
ZENITH_TEST(ZM_Data, Npc_EveryRowHasAtLeastOneLine)
{
	for (u_int i = 0; i < ZM_NPC_COUNT; ++i)
	{
		const ZM_NpcData& x = Npc(i);
		ZENITH_ASSERT_GT(x.m_uLineCount, 0u, "%s has nothing to say", x.m_szDisplayName);
	}
}

ZENITH_TEST(ZM_Data, Npc_StockWithinCap)
{
	for (u_int i = 0; i < ZM_NPC_COUNT; ++i)
	{
		ZENITH_ASSERT_LE(Npc(i).m_uStockCount, uZM_NPC_MAX_STOCK,
			"%s stocks past the per-row cap", Npc(i).m_szDisplayName);
	}
}

// The load-bearing one. m_uBuyPrice == 0 means NOT PURCHASABLE, and ZM_ShopBuy
// answers ZM_SHOP_ERR_NOT_PURCHASABLE for it -- so a stocked zero-price row would
// advertise an item the counter always refuses. For a KEY item it is worse: that
// is the ZM-D-120 hazard (a shop that would hand out a Badge Case) this guard
// exists to make impossible. The walk is guarded so it can never pass vacuously.
ZENITH_TEST(ZM_Data, Npc_EveryStockedItemIsPurchasable)
{
	u_int uTotalStock = 0;
	for (u_int i = 0; i < ZM_NPC_COUNT; ++i)
	{
		uTotalStock += Npc(i).m_uStockCount;
	}
	ZENITH_ASSERT_GT(uTotalStock, 0u, "the roster stocks no shop items at all");

	for (u_int i = 0; i < ZM_NPC_COUNT; ++i)
	{
		const ZM_NpcData& x = Npc(i);
		if (x.m_uStockCount > 0)
		{
			ZENITH_ASSERT_NOT_NULL(x.m_paeStock, "%s claims stock but has no array", x.m_szDisplayName);
		}
		for (u_int uSlot = 0; uSlot < x.m_uStockCount; ++uSlot)
		{
			const ZM_ITEM_ID eItem = x.m_paeStock[uSlot];
			ZENITH_ASSERT_GT(ZM_ShopBuyPrice(eItem), 0u,
				"%s stocks '%s', which is not purchasable", x.m_szDisplayName, ZM_GetItemName(eItem));
		}
	}
}

ZENITH_TEST(ZM_Data, Npc_EveryStockedItemIdIsInRange)
{
	ZENITH_ASSERT_GT(TotalStock(), 0u, "the roster stocks no shop items at all");

	for (u_int i = 0; i < ZM_NPC_COUNT; ++i)
	{
		const ZM_NpcData& x = Npc(i);
		for (u_int uSlot = 0; uSlot < x.m_uStockCount; ++uSlot)
		{
			ZENITH_ASSERT_LT((u_int)x.m_paeStock[uSlot], (u_int)ZM_ITEM_COUNT,
				"%s stock slot %u names an item id outside the table", x.m_szDisplayName, uSlot);
		}
	}
}

// Stock belongs to the one role that can open a shop, and that role must actually
// carry some: stock elsewhere is dead content, and a clerk without it is an empty
// counter.
ZENITH_TEST(ZM_Data, Npc_OnlyShopkeepsCarryStock)
{
	for (u_int i = 0; i < ZM_NPC_COUNT; ++i)
	{
		const ZM_NpcData& x = Npc(i);
		if (x.m_eRole == ZM_NPC_ROLE_SHOPKEEP)
		{
			ZENITH_ASSERT_GT(x.m_uStockCount, 0u, "shopkeep %s has an empty shop", x.m_szDisplayName);
		}
		else
		{
			ZENITH_ASSERT_EQ(x.m_uStockCount, 0u, "%s carries stock but never opens a shop", x.m_szDisplayName);
		}
	}
}

// A duplicate would list the same item twice on the shop screen.
ZENITH_TEST(ZM_Data, Npc_StockHasNoDuplicates)
{
	ZENITH_ASSERT_GT(TotalStock(), 0u, "the roster stocks no shop items at all");

	for (u_int i = 0; i < ZM_NPC_COUNT; ++i)
	{
		const ZM_NpcData& x = Npc(i);
		for (u_int uA = 0; uA < x.m_uStockCount; ++uA)
		{
			for (u_int uB = uA + 1; uB < x.m_uStockCount; ++uB)
			{
				ZENITH_ASSERT_NE((u_int)x.m_paeStock[uA], (u_int)x.m_paeStock[uB],
					"%s stocks '%s' twice (slots %u and %u)",
					x.m_szDisplayName, ZM_GetItemName(x.m_paeStock[uA]), uA, uB);
			}
		}
	}
}

// Pins the SC8 contract: exactly one NPC patrols. A second wanderer would break
// the rendezvous assumption SC8's test is built on; zero would leave the patrol
// code with no subject.
ZENITH_TEST(ZM_Data, Npc_ExactlyOneRowWanders)
{
	u_int uWanderers = 0;
	for (u_int i = 0; i < ZM_NPC_COUNT; ++i)
	{
		if (Npc(i).m_bWanders)
		{
			++uWanderers;
		}
	}
	ZENITH_ASSERT_EQ(uWanderers, 1u, "exactly one authored NPC may wander");
}

// Every dispatch arm SC4 will write has at least one row exercising it, so no
// branch ships without content behind it.
ZENITH_TEST(ZM_Data, Npc_RolesCoverEveryDispatchArm)
{
	for (u_int uRole = 0; uRole < ZM_NPC_ROLE_COUNT; ++uRole)
	{
		bool bFound = false;
		for (u_int i = 0; i < ZM_NPC_COUNT && !bFound; ++i)
		{
			bFound = ((u_int)Npc(i).m_eRole == uRole);
		}
		ZENITH_ASSERT_TRUE(bFound, "no authored NPC uses role %u", uRole);
	}
}

// ============================================================================
// S7 item 2 SC1 -- story-flag-gated line sets.
//
// A row may carry a second line array spoken while its gate FAILS. Every failure
// mode below is the same class as the ones above: an authoring mistake that
// surfaces as a silently broken NPC. The two new ways to break one are worse than
// the old ones, because both leave the roster looking fine: a gated row with no
// refusal lines goes MUTE the moment the gate closes, and a row whose two arrays
// are the same pointer keeps every selector test green while the feature does
// nothing at all.
// ============================================================================

// A gate naming a flag that was never added to the registry would be evaluated
// against a bit no story beat ever writes -- content locked forever.
ZENITH_TEST(ZM_Data, Npc_EveryRowGateReferencesARegisteredFlagOrNone)
{
	for (u_int i = 0; i < ZM_NPC_COUNT; ++i)
	{
		const ZM_NpcData& x = Npc(i);
		ZENITH_ASSERT_LE((u_int)x.m_xLineGate.m_eFlag, (u_int)ZM_STORY_FLAG_NONE,
			"%s gates on flag id %u, which is neither a registered flag nor the "
			"ungated sentinel", x.m_szDisplayName, (u_int)x.m_xLineGate.m_eFlag);
	}
}

// Pointer and count are a pair. A non-null pointer with count 0 is dead content;
// a null pointer with a non-zero count is a row that contradicts itself -- QueueLines
// refuses such a batch rather than walking it, so the cost is a MUTE NPC, not a crash.
ZENITH_TEST(ZM_Data, Npc_GatedLinePointerAndCountAgree)
{
	for (u_int i = 0; i < ZM_NPC_COUNT; ++i)
	{
		const ZM_NpcData& x = Npc(i);
		if (x.m_uGatedLineCount > 0)
		{
			ZENITH_ASSERT_NOT_NULL(x.m_paszGatedLines,
				"%s claims %u gated lines but has no array", x.m_szDisplayName,
				x.m_uGatedLineCount);
		}
		else
		{
			ZENITH_ASSERT_NULL(x.m_paszGatedLines,
				"%s carries a gated-line array with a count of zero", x.m_szDisplayName);
		}
	}
}

// QueueLines rejects a null / empty entry, and rejects the WHOLE batch when it
// does -- so one stray trailing comma leaves that NPC mute behind its gate.
ZENITH_TEST(ZM_Data, Npc_EveryGatedLineIsNonNullAndNonEmpty)
{
	ZENITH_ASSERT_GT(TotalGatedLines(), 0u, "the roster authors no gated dialogue at all");

	for (u_int i = 0; i < ZM_NPC_COUNT; ++i)
	{
		const ZM_NpcData& x = Npc(i);
		if (x.m_uGatedLineCount > 0)
		{
			ZENITH_ASSERT_NOT_NULL(x.m_paszGatedLines,
				"%s claims gated lines but has no array", x.m_szDisplayName);
		}
		if (x.m_paszGatedLines == nullptr)
		{
			continue;   // already reported; walking on would dereference null
		}
		for (u_int uLine = 0; uLine < x.m_uGatedLineCount; ++uLine)
		{
			const char* szLine = x.m_paszGatedLines[uLine];
			ZENITH_ASSERT_NOT_NULL(szLine, "%s gated line %u is null", x.m_szDisplayName, uLine);
			if (szLine != nullptr)
			{
				ZENITH_ASSERT_TRUE(szLine[0] != '\0', "%s gated line %u is empty",
					x.m_szDisplayName, uLine);
			}
		}
	}
}

// Asserted against the NAMED cap, as the ordinary-line unit is: QueueLines is
// all-or-nothing, so a gated set that outgrew the queue does not lose its last
// line -- it makes that NPC completely MUTE while the gate is closed.
ZENITH_TEST(ZM_Data, Npc_GatedLineCountFitsTheDialogueQueue)
{
	ZENITH_ASSERT_GT(TotalGatedLines(), 0u, "the roster authors no gated dialogue at all");

	for (u_int i = 0; i < ZM_NPC_COUNT; ++i)
	{
		ZENITH_ASSERT_LE(Npc(i).m_uGatedLineCount, uZM_NPC_MAX_LINES,
			"%s queues more gated lines than the dialogue box accepts",
			Npc(i).m_szDisplayName);
	}
}

// A gated row must be able to speak on BOTH sides of its gate. A row that ships
// with a gate but no refusal lines is an NPC that goes silent the moment the
// content it guards is locked.
ZENITH_TEST(ZM_Data, Npc_EveryGatedRowAuthorsBothLineSets)
{
	ZENITH_ASSERT_GT(GatedRowCount(), 0u,
		"no roster row is gated, so the walk below is vacuous");

	for (u_int i = 0; i < ZM_NPC_COUNT; ++i)
	{
		const ZM_NpcData& x = Npc(i);
		if (!RowIsGated(x))
		{
			continue;
		}
		ZENITH_ASSERT_GT(x.m_uLineCount, 0u,
			"gated row %s has no ordinary lines", x.m_szDisplayName);
		ZENITH_ASSERT_LE(x.m_uLineCount, uZM_NPC_MAX_LINES,
			"gated row %s outgrew the dialogue queue on its ordinary lines", x.m_szDisplayName);
		ZENITH_ASSERT_NOT_NULL(x.m_paszLines,
			"gated row %s has no ordinary-line array", x.m_szDisplayName);

		ZENITH_ASSERT_GT(x.m_uGatedLineCount, 0u,
			"gated row %s has no refusal line -- it goes MUTE while its gate is closed",
			x.m_szDisplayName);
		ZENITH_ASSERT_LE(x.m_uGatedLineCount, uZM_NPC_MAX_LINES,
			"gated row %s outgrew the dialogue queue on its refusal lines", x.m_szDisplayName);
		ZENITH_ASSERT_NOT_NULL(x.m_paszGatedLines,
			"gated row %s has no refusal-line array", x.m_szDisplayName);
	}
}

// The converse, mirroring Npc_OnlyShopkeepsCarryStock: a refusal array on a row
// with no gate is unreachable content, which is what a deleted gate leaves behind.
ZENITH_TEST(ZM_Data, Npc_UngatedRowsCarryNoGatedLines)
{
	for (u_int i = 0; i < ZM_NPC_COUNT; ++i)
	{
		const ZM_NpcData& x = Npc(i);
		if (RowIsGated(x))
		{
			continue;
		}
		ZENITH_ASSERT_EQ(x.m_uGatedLineCount, 0u,
			"%s has no gate but authors %u refusal lines nothing can ever reach",
			x.m_szDisplayName, x.m_uGatedLineCount);
		ZENITH_ASSERT_NULL(x.m_paszGatedLines,
			"%s has no gate but carries a refusal-line array", x.m_szDisplayName);
	}
}

// The whole mechanism has exactly one live consumer in the shipped roster. Delete
// that row and every unit above passes vacuously while the feature is dead.
ZENITH_TEST(ZM_Data, Npc_AtLeastOneRowIsGated)
{
	ZENITH_ASSERT_GT(GatedRowCount(), 0u,
		"no authored NPC is flag-gated, so nothing in the world exercises story gating");
}

// ---- ZM_SelectNpcLines (pure) -----------------------------------------------

// POINTER IDENTITY, not string equality: the selector's contract is that it hands
// back the row's OWN array, so a copy, a reorder, or a rebuilt array is a failure
// even when the text happens to match.
ZENITH_TEST(ZM_Data, Select_ReturnsOrdinaryLinesWhenGatePasses)
{
	const u_int uRow = FirstGatedRowIndex();
	ZENITH_ASSERT_LT(uRow, (u_int)ZM_NPC_COUNT,
		"no roster row is gated, so this unit has nothing to select over");
	if (uRow >= (u_int)ZM_NPC_COUNT)
	{
		return;
	}

	const ZM_NpcData& x = Npc(uRow);
	const ZM_StoryFlagSet xFlags = MakeGatePassingFlags(x);
	ZENITH_ASSERT_TRUE(ZM_StoryGatePasses(x.m_xLineGate, xFlags),
		"the fixture does not actually open %s's gate", x.m_szDisplayName);

	const char* const* paszLines = nullptr;
	u_int uCount = 0;
	ZM_SelectNpcLines(x, xFlags, paszLines, uCount);

	ZENITH_ASSERT_TRUE(paszLines == x.m_paszLines,
		"%s's open gate must yield the row's OWN ordinary-line array", x.m_szDisplayName);
	ZENITH_ASSERT_EQ(uCount, x.m_uLineCount,
		"%s's open gate must yield the ordinary line COUNT", x.m_szDisplayName);
}

// The single most likely careless implementation is a selector that ignores its
// flag-set argument entirely. That one passes the unit above and fails only here.
ZENITH_TEST(ZM_Data, Select_ReturnsGatedLinesWhenGateFails)
{
	const u_int uRow = FirstGatedRowIndex();
	ZENITH_ASSERT_LT(uRow, (u_int)ZM_NPC_COUNT,
		"no roster row is gated, so this unit has nothing to select over");
	if (uRow >= (u_int)ZM_NPC_COUNT)
	{
		return;
	}

	const ZM_NpcData& x = Npc(uRow);
	const ZM_StoryFlagSet xFlags = MakeGateFailingFlags(x);
	ZENITH_ASSERT_FALSE(ZM_StoryGatePasses(x.m_xLineGate, xFlags),
		"the fixture does not actually close %s's gate", x.m_szDisplayName);

	const char* const* paszLines = nullptr;
	u_int uCount = 0;
	ZM_SelectNpcLines(x, xFlags, paszLines, uCount);

	ZENITH_ASSERT_TRUE(paszLines == x.m_paszGatedLines,
		"%s's closed gate must yield the row's OWN refusal-line array", x.m_szDisplayName);
	ZENITH_ASSERT_EQ(uCount, x.m_uGatedLineCount,
		"%s's closed gate must yield the refusal line COUNT", x.m_szDisplayName);
}

// An UNGATED row must answer the same way under every flag state.
//
// The fixture is LOCAL and carries a REAL gated array with a non-zero count, which
// is the whole point: every ungated row in the shipped table leaves
// m_paszGatedLines null, so the selector short-circuits on "has no gated set" and
// ZM_StoryGatePasses is never called at all. Against such a row this unit could not
// see a selector bug of any kind -- it would agree with a selector that ignored the
// gate machinery entirely.
//
// With a gated set present, the mutation it now catches is a selector (or gate
// predicate) that treats ZM_STORY_FLAG_NONE as bit index 0: under the all-CLEAR set
// a require-SET gate on bit 0 FAILS, and this row would speak the warden's refusal
// lines instead of its own. Both flag states are driven because the two gate
// polarities break under opposite ones.
ZENITH_TEST(ZM_Data, Select_UngatedRowIgnoresFlagsEntirely)
{
	const ZM_NpcData& xUngatedSource = ZM_GetNpcData(ZM_NPC_VILLAGER);
	const ZM_NpcData& xGatedSource = ZM_GetNpcData(ZM_NPC_ROUTE_WARDEN);
	ZENITH_ASSERT_EQ((u_int)xUngatedSource.m_xLineGate.m_eFlag, (u_int)ZM_STORY_FLAG_NONE,
		"the villager is this unit's UNGATED line source; she has acquired a gate");
	ZENITH_ASSERT_NOT_NULL(xUngatedSource.m_paszLines, "the ungated source has no lines");
	ZENITH_ASSERT_NOT_NULL(xGatedSource.m_paszGatedLines,
		"the warden supplies the refusal array this fixture must NOT select");
	ZENITH_ASSERT_GT(xGatedSource.m_uGatedLineCount, 0u,
		"a zero gated count would short-circuit the selector, which is the very "
		"weakness this unit was rewritten to remove");
	if ((xUngatedSource.m_paszLines == nullptr)
		|| (xGatedSource.m_paszGatedLines == nullptr)
		|| (xGatedSource.m_uGatedLineCount == 0u))
	{
		return;   // already reported; the fixture below would prove nothing
	}

	// UNGATED (m_eFlag == NONE) yet fully equipped to speak a refusal set.
	ZM_NpcData x = xUngatedSource;
	x.m_xLineGate = ZM_StoryGate{};
	x.m_paszGatedLines = xGatedSource.m_paszGatedLines;
	x.m_uGatedLineCount = xGatedSource.m_uGatedLineCount;
	ZENITH_ASSERT_TRUE(x.m_paszLines != x.m_paszGatedLines,
		"the fixture's two arrays are the same pointer, so the identity checks below "
		"could not tell the two sets apart");

	const ZM_StoryFlagSet xClear;
	const ZM_StoryFlagSet xAllSet = MakeAllRegisteredFlagsSet();
	ZENITH_ASSERT_GT(xAllSet.Count(), 0u,
		"the all-set fixture is empty, so the second case below repeats the first");

	const char* const* paszClearLines = nullptr;
	u_int uClearCount = 0;
	ZM_SelectNpcLines(x, xClear, paszClearLines, uClearCount);
	ZENITH_ASSERT_TRUE(paszClearLines == x.m_paszLines,
		"an ungated row must speak its ordinary lines with no flags set, even when it "
		"carries a refusal set a NONE-as-index-0 bug could reach for");
	ZENITH_ASSERT_EQ(uClearCount, x.m_uLineCount);

	const char* const* paszSetLines = nullptr;
	u_int uSetCount = 0;
	ZM_SelectNpcLines(x, xAllSet, paszSetLines, uSetCount);
	ZENITH_ASSERT_TRUE(paszSetLines == x.m_paszLines,
		"an ungated row must speak the SAME lines with every story flag set");
	ZENITH_ASSERT_EQ(uSetCount, x.m_uLineCount);
}

// The MUTE guard, over the full cross product of rows x {no flags, every flag}.
// Every shape checked below is one QueueLines REFUSES (it never dereferences a null
// array), so the failure this pins is an NPC that says nothing at all.
// The two flag states cover both gate polarities for any row: a require-SET gate
// fails in the first and passes in the second, a require-CLEAR gate the reverse.
ZENITH_TEST(ZM_Data, Select_NeverYieldsNullWithNonZeroCount)
{
	ZENITH_ASSERT_GT(ZM_GetNpcCount(), 0u, "an empty roster makes this walk vacuous");

	const ZM_StoryFlagSet axFLAG_CASES[2] = { ZM_StoryFlagSet(), MakeAllRegisteredFlagsSet() };
	const char* const aszCASE_NAMES[2] = { "no flags set", "every registered flag set" };

	for (u_int i = 0; i < ZM_NPC_COUNT; ++i)
	{
		const ZM_NpcData& x = Npc(i);
		for (u_int uCase = 0; uCase < 2u; ++uCase)
		{
			const char* const* paszLines = nullptr;
			u_int uCount = 0;
			ZM_SelectNpcLines(x, axFLAG_CASES[uCase], paszLines, uCount);

			// Every shape QueueLines refuses, and it refuses the WHOLE batch when it
			// does: a null array, a zero count, an oversized count, a null entry. Each
			// one costs the player the entire conversation, so each is checked here.
			ZENITH_ASSERT_GT(uCount, 0u, "%s has nothing to say with %s",
				x.m_szDisplayName, aszCASE_NAMES[uCase]);
			ZENITH_ASSERT_LE(uCount, uZM_NPC_MAX_LINES,
				"%s yields %u lines with %s, past the dialogue queue",
				x.m_szDisplayName, uCount, aszCASE_NAMES[uCase]);
			ZENITH_ASSERT_NOT_NULL(paszLines, "%s yields a null array with %s",
				x.m_szDisplayName, aszCASE_NAMES[uCase]);
			if (paszLines == nullptr)
			{
				continue;   // already reported; walking on would dereference null
			}
			for (u_int uLine = 0; uLine < uCount; ++uLine)
			{
				ZENITH_ASSERT_NOT_NULL(paszLines[uLine],
					"%s line %u is null with %s", x.m_szDisplayName, uLine, aszCASE_NAMES[uCase]);
			}
		}
	}
}

// The two SANITISERS, driven on purpose. No shipped table row can reach either:
// every row authors at most three lines against a cap of eight, and pointer/count
// agreement is already enforced by Npc_GatedLinePointerAndCountAgree and
// Npc_EveryLineIsNonNullAndNonEmpty. So both branches are DEAD to every unit above
// -- delete them and the whole suite stays green while the header's two advertised
// guarantees quietly stop holding. The fixtures here are therefore LOCAL COPIES,
// never table rows, deliberately malformed one field at a time.
ZENITH_TEST(ZM_Data, Select_SanitisesMalformedRow)
{
	// A REAL backing array, longer than the cap. The clamp is exercised over memory
	// that genuinely exists, so a clamped count still names initialised entries and
	// the case cannot be confused with reading off the end of a short array.
	constexpr u_int uOVER_CAP_LINES = uZM_NPC_MAX_LINES + 4u;
	const char* aszOverCap[uOVER_CAP_LINES];
	for (u_int u = 0u; u < uOVER_CAP_LINES; ++u)
	{
		aszOverCap[u] = "over-cap filler line";
	}

	const ZM_NpcData& xUngatedSource = ZM_GetNpcData(ZM_NPC_VILLAGER);
	ZENITH_ASSERT_EQ((u_int)xUngatedSource.m_xLineGate.m_eFlag, (u_int)ZM_STORY_FLAG_NONE,
		"cases (a) and (b) need an UNGATED source, or they take the gated path instead");

	// (a) NULL ARRAY, NON-ZERO COUNT -> drives the NULL GUARD. Without it the selector
	// emits (nullptr, 3), a pair that contradicts itself. QueueLines happens to refuse
	// it -- the NPC goes mute, it does not crash -- but that is the CALLEE's guard, and
	// the selector advertises this guarantee on its own. The guard is also what stops
	// the clamp below validating a count while the pointer beside it stays bogus.
	{
		ZM_NpcData x = xUngatedSource;
		x.m_paszLines = nullptr;
		x.m_uLineCount = 3u;

		// Seeded NON-NULL and non-zero going IN, so both outputs must be written --
		// a selector that returned early would leave the seed behind and fail here.
		const char* const* paszLines = aszOverCap;
		u_int uCount = 99u;
		ZM_SelectNpcLines(x, ZM_StoryFlagSet(), paszLines, uCount);
		ZENITH_ASSERT_EQ(uCount, 0u,
			"a null line array must yield a count of ZERO, whatever the row claimed");
		ZENITH_ASSERT_NULL(paszLines,
			"the selector still hands back the row's OWN (null) array -- it is the zero "
			"count above, not a substituted pointer, that makes this safe");
	}

	// (b) COUNT PAST THE CAP -> drives the CLAMP. Without it QueueLines is handed
	// 12 lines against a queue of 8, and because it is ALL-OR-NOTHING it rejects the
	// WHOLE batch: the NPC goes completely MUTE rather than losing its tail.
	{
		ZM_NpcData x = xUngatedSource;
		x.m_paszLines = aszOverCap;
		x.m_uLineCount = uOVER_CAP_LINES;

		const char* const* paszLines = nullptr;
		u_int uCount = 0u;
		ZM_SelectNpcLines(x, ZM_StoryFlagSet(), paszLines, uCount);
		ZENITH_ASSERT_TRUE(paszLines == aszOverCap,
			"the clamp must not swap the array out from under the caller");
		ZENITH_ASSERT_EQ(uCount, uZM_NPC_MAX_LINES,
			"an over-cap count must be clamped TO the cap, not passed through");
	}

	// ---- the same two malformations on the GATED side ------------------------
	// The gate must genuinely FAIL here, or (c) and (d) silently degrade into
	// repeats of (a) and (b) on the ordinary path.
	{
		const ZM_NpcData xGatedSource = ZM_GetNpcData(ZM_NPC_ROUTE_WARDEN);
		const ZM_StoryFlagSet xClear;   // the warden's gate is still shut
		ZENITH_ASSERT_NE((u_int)xGatedSource.m_xLineGate.m_eFlag, (u_int)ZM_STORY_FLAG_NONE,
			"the gated source lost its gate, so the cases below take the ordinary path");
		ZENITH_ASSERT_FALSE(ZM_StoryGatePasses(xGatedSource.m_xLineGate, xClear),
			"the fixture does not actually close the gate");
		ZENITH_ASSERT_NOT_NULL(xGatedSource.m_paszLines,
			"case (c) asserts the fallback lands on the ORDINARY array, which is missing");

		// (c) NULL GATED ARRAY, NON-ZERO GATED COUNT. This CANNOT reach the null
		// guard, by construction: the selector only switches to the gated set when
		// that pointer is already non-null, so a half-authored gate falls back to the
		// ordinary lines. That FALLBACK is the guarantee under test -- without it a
		// row whose refusal array was deleted would go mute the moment its gate shut.
		{
			ZM_NpcData x = xGatedSource;
			x.m_paszGatedLines = nullptr;
			x.m_uGatedLineCount = 3u;

			const char* const* paszLines = nullptr;
			u_int uCount = 0u;
			ZM_SelectNpcLines(x, xClear, paszLines, uCount);
			ZENITH_ASSERT_TRUE(paszLines == xGatedSource.m_paszLines,
				"a closed gate with no refusal array must fall back to the ordinary lines");
			ZENITH_ASSERT_EQ(uCount, xGatedSource.m_uLineCount,
				"the fallback must carry the ORDINARY count, not the phantom gated one");

			// ...and with the ordinary array gone as well, that fallback lands on
			// null -- the ONLY route by which a gate-failing row reaches the null
			// guard, which is what stops the contradictory pair escaping the selector.
			x.m_paszLines = nullptr;
			x.m_uLineCount = 2u;
			paszLines = aszOverCap;
			uCount = 99u;
			ZM_SelectNpcLines(x, xClear, paszLines, uCount);
			ZENITH_ASSERT_EQ(uCount, 0u,
				"a row with NEITHER array must yield a count of zero, not its claimed 2");
			ZENITH_ASSERT_NULL(paszLines);
		}

		// (d) OVER-CAP GATED COUNT -> drives the CLAMP on the gated side. Without it
		// the NPC is mute behind its own closed gate, which is the exact failure the
		// gating feature exists to avoid: the refusal is the only thing the player
		// can be told at that moment.
		{
			ZM_NpcData x = xGatedSource;
			x.m_paszGatedLines = aszOverCap;
			x.m_uGatedLineCount = uOVER_CAP_LINES;

			const char* const* paszLines = nullptr;
			u_int uCount = 0u;
			ZM_SelectNpcLines(x, xClear, paszLines, uCount);
			ZENITH_ASSERT_TRUE(paszLines == aszOverCap,
				"a closed gate must still yield the row's OWN refusal array");
			ZENITH_ASSERT_EQ(uCount, uZM_NPC_MAX_LINES,
				"an over-cap GATED count must be clamped TO the cap");
		}
	}
}

// The mistake neither selector unit can see: paste the same array into both fields
// and Select_ReturnsOrdinaryLinesWhenGatePasses and
// Select_ReturnsGatedLinesWhenGateFails BOTH stay green while the warden says the
// same thing on both sides of his gate.
ZENITH_TEST(ZM_Data, Warden_OrdinaryAndGatedSetsAreDistinct)
{
	const ZM_NpcData& x = ZM_GetNpcData(ZM_NPC_ROUTE_WARDEN);

	ZENITH_ASSERT_NE((u_int)x.m_xLineGate.m_eFlag, (u_int)ZM_STORY_FLAG_NONE,
		"the warden is the SC1 gating demonstration and must carry a real gate");
	ZENITH_ASSERT_GT(x.m_uLineCount, 0u, "the warden has no ordinary lines");
	ZENITH_ASSERT_GT(x.m_uGatedLineCount, 0u, "the warden has no refusal lines");
	ZENITH_ASSERT_NOT_NULL(x.m_paszLines, "the warden has no ordinary-line array");
	ZENITH_ASSERT_NOT_NULL(x.m_paszGatedLines, "the warden has no refusal-line array");

	ZENITH_ASSERT_TRUE(x.m_paszLines != x.m_paszGatedLines,
		"the warden's two line sets are the SAME array -- the gate selects nothing");

	// A separate array whose first line was copy-pasted is the same bug one step
	// later, and pointer inequality alone cannot see it.
	if ((x.m_paszLines == nullptr) || (x.m_paszGatedLines == nullptr)
		|| (x.m_uLineCount == 0) || (x.m_uGatedLineCount == 0))
	{
		return;   // already reported above
	}
	const char* szOrdinary = x.m_paszLines[0];
	const char* szGated = x.m_paszGatedLines[0];
	ZENITH_ASSERT_NOT_NULL(szOrdinary, "the warden's first ordinary line is null");
	ZENITH_ASSERT_NOT_NULL(szGated, "the warden's first refusal line is null");
	if ((szOrdinary == nullptr) || (szGated == nullptr))
	{
		return;
	}
	ZENITH_ASSERT_FALSE(strcmp(szOrdinary, szGated) == 0,
		"the warden opens both line sets with the same text ('%s')", szOrdinary);
}
