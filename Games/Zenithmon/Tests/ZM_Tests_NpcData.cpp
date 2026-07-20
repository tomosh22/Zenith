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
#include "Zenithmon/Source/Shop/ZM_ShopLogic.h"    // ZM_ShopBuyPrice -- the purchasable check
#include "Zenithmon/Source/UI/ZM_UI_DialogueBox.h" // uMAX_QUEUED_LINES -- the line cap's source

namespace
{
	// Spelled in the TEST, not read back off the table, so "the roster changed"
	// is a failure rather than a silently-agreeing tautology.
	constexpr u_int uEXPECTED_NPCS = 4;

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
