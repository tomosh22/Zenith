#include "Zenith.h"

// ============================================================================
// ZM_Tests_Items -- schema integrity of the item table (category ZM_Data). The
// item rows are inert until the S2/S5 bag + battle logic interprets
// ZM_ITEM_EFFECT, so this suite locks the schema: index consistency, per-field
// ranges, the category/effect/price/consumable cross-rules, that every TM
// teaches a REAL move, and that every effect kind + category is populated. See
// DecisionLog ZM-D-024.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/Data/ZM_Types.h"
#include "Zenithmon/Source/Data/ZM_MoveData.h"
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"   // ZM_STAT_COUNT
#include "Zenithmon/Source/Data/ZM_ItemData.h"

#include <cstring>

namespace
{
	constexpr u_int uEXPECTED_ITEMS = 90;

	const ZM_ItemData& It(u_int uIndex)
	{
		return ZM_GetItemData((ZM_ITEM_ID)uIndex);
	}

	bool CategoryIsConsumable(ZM_ITEM_CATEGORY eCat)
	{
		switch (eCat)
		{
		case ZM_ITEM_CATEGORY_BALL:
		case ZM_ITEM_CATEGORY_MEDICINE:
		case ZM_ITEM_CATEGORY_BATTLE:
		case ZM_ITEM_CATEGORY_BERRY:
		case ZM_ITEM_CATEGORY_EVO:
		case ZM_ITEM_CATEGORY_FIELD:
			return true;
		default:   // HELD, TM, KEY
			return false;
		}
	}

	bool EffectUsesStatParam(ZM_ITEM_EFFECT e)
	{
		return e == ZM_ITEM_EFFECT_EV_BOOST
			|| e == ZM_ITEM_EFFECT_BATTLE_STAT_BOOST
			|| e == ZM_ITEM_EFFECT_HELD_CHOICE
			|| e == ZM_ITEM_EFFECT_BERRY_PINCH_BOOST;
	}

	bool EffectUsesTypeParam(ZM_ITEM_EFFECT e)
	{
		return e == ZM_ITEM_EFFECT_HELD_TYPE_BOOST
			|| e == ZM_ITEM_EFFECT_BERRY_TYPE_RESIST;
	}
}

// The roster is the expected size and the table is index-consistent.
ZENITH_TEST(ZM_Data, Items_CountAndSelfConsistent)
{
	ZENITH_ASSERT_EQ(ZM_GetItemCount(), uEXPECTED_ITEMS);
	ZENITH_ASSERT_EQ((u_int)ZM_ITEM_COUNT, uEXPECTED_ITEMS);
	ZENITH_ASSERT_EQ((u_int)ZM_ITEM_NONE, (u_int)ZM_ITEM_COUNT);
	for (u_int i = 0; i < ZM_ITEM_COUNT; ++i)
	{
		ZENITH_ASSERT_EQ((u_int)It(i).m_eId, i, "item row %u has mismatched m_eId", i);
	}
}

// Every name is present and unique.
ZENITH_TEST(ZM_Data, Items_NamesUniqueNonEmpty)
{
	for (u_int i = 0; i < ZM_ITEM_COUNT; ++i)
	{
		const char* szA = It(i).m_szName;
		ZENITH_ASSERT_NOT_NULL(szA);
		ZENITH_ASSERT_TRUE(szA[0] != '\0', "item %u has an empty name", i);
		for (u_int j = i + 1; j < ZM_ITEM_COUNT; ++j)
		{
			ZENITH_ASSERT_FALSE(strcmp(szA, It(j).m_szName) == 0,
				"duplicate item name '%s' at %u and %u", szA, i, j);
		}
	}
}

// Category and effect enums are in range.
ZENITH_TEST(ZM_Data, Items_EnumFieldsValid)
{
	for (u_int i = 0; i < ZM_ITEM_COUNT; ++i)
	{
		ZENITH_ASSERT_TRUE(It(i).m_eCategory < ZM_ITEM_CATEGORY_COUNT, "%s bad category", It(i).m_szName);
		ZENITH_ASSERT_TRUE(It(i).m_eEffect < ZM_ITEM_EFFECT_COUNT, "%s bad effect", It(i).m_szName);
	}
}

// Sell price never exceeds buy price; key items are priceless and effectless.
ZENITH_TEST(ZM_Data, Items_PricesAndKeyContract)
{
	for (u_int i = 0; i < ZM_ITEM_COUNT; ++i)
	{
		const ZM_ItemData& x = It(i);
		ZENITH_ASSERT_LE(x.m_uSellPrice, x.m_uBuyPrice, "%s sells for more than it buys", x.m_szName);
		if (x.m_eCategory == ZM_ITEM_CATEGORY_KEY)
		{
			ZENITH_ASSERT_EQ(x.m_uBuyPrice, 0u, "key item %s has a price", x.m_szName);
			ZENITH_ASSERT_EQ(x.m_uSellPrice, 0u, "key item %s is sellable", x.m_szName);
			ZENITH_ASSERT_EQ((u_int)x.m_eEffect, (u_int)ZM_ITEM_EFFECT_NONE, "key item %s has an effect", x.m_szName);
		}
	}
}

// The consumable flag matches the category's semantics.
ZENITH_TEST(ZM_Data, Items_ConsumableByCategory)
{
	for (u_int i = 0; i < ZM_ITEM_COUNT; ++i)
	{
		const ZM_ItemData& x = It(i);
		ZENITH_ASSERT_EQ(x.m_bConsumable, CategoryIsConsumable(x.m_eCategory),
			"%s consumable flag disagrees with its category", x.m_szName);
	}
}

// TMs (and only TMs) teach a real move; non-TMs never carry a taught move.
ZENITH_TEST(ZM_Data, Items_TmTeachesRealMove)
{
	for (u_int i = 0; i < ZM_ITEM_COUNT; ++i)
	{
		const ZM_ItemData& x = It(i);
		if (x.m_eCategory == ZM_ITEM_CATEGORY_TM)
		{
			ZENITH_ASSERT_EQ((u_int)x.m_eEffect, (u_int)ZM_ITEM_EFFECT_TEACH_MOVE, "TM %s wrong effect", x.m_szName);
			ZENITH_ASSERT_TRUE(x.m_eTaughtMove < ZM_MOVE_COUNT, "TM %s teaches a non-move", x.m_szName);
		}
		else
		{
			ZENITH_ASSERT_NE((u_int)x.m_eEffect, (u_int)ZM_ITEM_EFFECT_TEACH_MOVE, "%s is not a TM but teaches", x.m_szName);
			ZENITH_ASSERT_EQ((u_int)x.m_eTaughtMove, (u_int)ZM_MOVE_NONE, "%s carries a taught move", x.m_szName);
		}
	}
}

// Balls (and only balls) capture; catch multiplier is at least 1.0x (x10).
ZENITH_TEST(ZM_Data, Items_BallContract)
{
	for (u_int i = 0; i < ZM_ITEM_COUNT; ++i)
	{
		const ZM_ItemData& x = It(i);
		if (x.m_eCategory == ZM_ITEM_CATEGORY_BALL)
		{
			ZENITH_ASSERT_EQ((u_int)x.m_eEffect, (u_int)ZM_ITEM_EFFECT_CATCH, "ball %s wrong effect", x.m_szName);
			ZENITH_ASSERT_GE(x.m_uEffectParam, 10u, "ball %s catch multiplier < 1.0x", x.m_szName);
		}
		else
		{
			ZENITH_ASSERT_NE((u_int)x.m_eEffect, (u_int)ZM_ITEM_EFFECT_CATCH, "%s catches but is not a ball", x.m_szName);
		}
	}
}

// Effect params that index a stat / type are in range for that enum.
ZENITH_TEST(ZM_Data, Items_EffectParamRanges)
{
	for (u_int i = 0; i < ZM_ITEM_COUNT; ++i)
	{
		const ZM_ItemData& x = It(i);
		if (EffectUsesStatParam(x.m_eEffect))
		{
			ZENITH_ASSERT_LT(x.m_uEffectParam, (u_int)ZM_STAT_COUNT, "%s stat param out of range", x.m_szName);
		}
		if (EffectUsesTypeParam(x.m_eEffect))
		{
			ZENITH_ASSERT_LT(x.m_uEffectParam, (u_int)ZM_TYPE_COUNT, "%s type param out of range", x.m_szName);
		}
		if (x.m_eEffect == ZM_ITEM_EFFECT_HEAL_HP)
		{
			ZENITH_ASSERT_GT(x.m_uEffectParam, 0u, "%s heals 0 HP", x.m_szName);
		}
	}
}

// Every effect kind is exercised by at least one item.
ZENITH_TEST(ZM_Data, Items_EveryEffectKindUsed)
{
	for (u_int e = 0; e < ZM_ITEM_EFFECT_COUNT; ++e)
	{
		bool bFound = false;
		for (u_int i = 0; i < ZM_ITEM_COUNT && !bFound; ++i)
		{
			bFound = ((u_int)It(i).m_eEffect == e);
		}
		ZENITH_ASSERT_TRUE(bFound, "no item uses effect kind %u", e);
	}
}

// Every category is populated.
ZENITH_TEST(ZM_Data, Items_EveryCategoryUsed)
{
	for (u_int c = 0; c < ZM_ITEM_CATEGORY_COUNT; ++c)
	{
		bool bFound = false;
		for (u_int i = 0; i < ZM_ITEM_COUNT && !bFound; ++i)
		{
			bFound = ((u_int)It(i).m_eCategory == c);
		}
		ZENITH_ASSERT_TRUE(bFound, "category %s (%u) has no items",
			ZM_ItemCategoryToString((ZM_ITEM_CATEGORY)c), c);
	}
}

// Accessors + category stringifier honour their contracts.
ZENITH_TEST(ZM_Data, Items_AccessorAndToStringContract)
{
	for (u_int i = 0; i < ZM_ITEM_COUNT; ++i)
	{
		ZENITH_ASSERT_STREQ(ZM_GetItemName((ZM_ITEM_ID)i), It(i).m_szName);
	}
	ZENITH_ASSERT_STREQ(ZM_GetItemName(ZM_ITEM_NONE), "NONE");

	for (u_int c = 0; c < ZM_ITEM_CATEGORY_COUNT; ++c)
	{
		const char* sz = ZM_ItemCategoryToString((ZM_ITEM_CATEGORY)c);
		ZENITH_ASSERT_NOT_NULL(sz);
		ZENITH_ASSERT_FALSE(strcmp(sz, "INVALID") == 0, "category %u stringifies as INVALID", c);
	}
	ZENITH_ASSERT_STREQ(ZM_ItemCategoryToString(ZM_ITEM_CATEGORY_COUNT), "INVALID");
}
