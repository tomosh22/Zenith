#include "Zenith.h"

// ============================================================================
// ZM_Tests_Natures -- integrity of the 25-nature table (category ZM_Data). Locks
// the exact 5x5 grid of (raised, lowered) stat pairs over the five non-HP stats,
// the five neutral natures on the diagonal, and the integer-percent multiplier
// contract that ZM_StatCalc will consume. See DecisionLog ZM-D-025.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"   // ZM_STAT
#include "Zenithmon/Source/Data/ZM_NatureData.h"

#include <cstring>

namespace
{
	constexpr u_int uEXPECTED_NATURES = 25;

	const ZM_NatureData& Nat(u_int i) { return ZM_GetNatureData((ZM_NATURE)i); }

	bool IsBattleStat(ZM_STAT e)   // the five non-HP stats a nature may touch
	{
		return e == ZM_STAT_ATTACK || e == ZM_STAT_DEFENSE || e == ZM_STAT_SPATTACK
			|| e == ZM_STAT_SPDEFENSE || e == ZM_STAT_SPEED;
	}
}

ZENITH_TEST(ZM_Data, Natures_CountAndSelfConsistent)
{
	ZENITH_ASSERT_EQ(ZM_GetNatureCount(), uEXPECTED_NATURES);
	ZENITH_ASSERT_EQ((u_int)ZM_NATURE_COUNT, uEXPECTED_NATURES);
	for (u_int i = 0; i < ZM_NATURE_COUNT; ++i)
	{
		ZENITH_ASSERT_EQ((u_int)Nat(i).m_eId, i, "nature row %u has mismatched m_eId", i);
	}
}

ZENITH_TEST(ZM_Data, Natures_NamesUniqueNonEmpty)
{
	for (u_int i = 0; i < ZM_NATURE_COUNT; ++i)
	{
		const char* szA = Nat(i).m_szName;
		ZENITH_ASSERT_NOT_NULL(szA);
		ZENITH_ASSERT_TRUE(szA[0] != '\0', "nature %u has an empty name", i);
		for (u_int j = i + 1; j < ZM_NATURE_COUNT; ++j)
		{
			ZENITH_ASSERT_FALSE(strcmp(szA, Nat(j).m_szName) == 0,
				"duplicate nature name '%s' at %u and %u", szA, i, j);
		}
	}
}

// Raised and lowered stats are always non-HP battle stats.
ZENITH_TEST(ZM_Data, Natures_StatsAreNonHP)
{
	for (u_int i = 0; i < ZM_NATURE_COUNT; ++i)
	{
		ZENITH_ASSERT_TRUE(IsBattleStat(Nat(i).m_eRaised), "%s raises a non-battle stat", Nat(i).m_szName);
		ZENITH_ASSERT_TRUE(IsBattleStat(Nat(i).m_eLowered), "%s lowers a non-battle stat", Nat(i).m_szName);
	}
}

// The 25 natures are exactly the 5x5 grid of (raised, lowered) pairs -- each pair
// appears once -- and exactly 5 are neutral (raised == lowered).
ZENITH_TEST(ZM_Data, Natures_EveryPairExactlyOnce)
{
	u_int uNeutral = 0;
	for (u_int r = ZM_STAT_ATTACK; r < ZM_STAT_COUNT; ++r)
	{
		for (u_int l = ZM_STAT_ATTACK; l < ZM_STAT_COUNT; ++l)
		{
			u_int uMatches = 0;
			for (u_int i = 0; i < ZM_NATURE_COUNT; ++i)
			{
				if ((u_int)Nat(i).m_eRaised == r && (u_int)Nat(i).m_eLowered == l)
				{
					++uMatches;
				}
			}
			ZENITH_ASSERT_EQ(uMatches, 1u, "pair (raise %u, lower %u) not present exactly once", r, l);
		}
	}
	for (u_int i = 0; i < ZM_NATURE_COUNT; ++i)
	{
		if (Nat(i).m_eRaised == Nat(i).m_eLowered) { ++uNeutral; }
	}
	ZENITH_ASSERT_EQ(uNeutral, 5u, "expected exactly 5 neutral natures");
}

// The percent multiplier: 110 for the raised stat, 90 for the lowered stat, 100
// everywhere else (all of HP, and every stat of a neutral nature).
ZENITH_TEST(ZM_Data, Natures_StatPercentContract)
{
	for (u_int i = 0; i < ZM_NATURE_COUNT; ++i)
	{
		const ZM_NatureData& x = Nat(i);
		const bool bNeutral = (x.m_eRaised == x.m_eLowered);
		for (u_int s = 0; s < ZM_STAT_COUNT; ++s)
		{
			u_int uExpected = 100u;
			if (!bNeutral && s == (u_int)x.m_eRaised)  { uExpected = 110u; }
			else if (!bNeutral && s == (u_int)x.m_eLowered) { uExpected = 90u; }
			ZENITH_ASSERT_EQ(ZM_GetNatureStatPercent(x.m_eId, (ZM_STAT)s), uExpected,
				"%s stat %u percent wrong", x.m_szName, s);
		}
		// HP is never touched by any nature.
		ZENITH_ASSERT_EQ(ZM_GetNatureStatPercent(x.m_eId, ZM_STAT_HP), 100u, "%s changes HP", x.m_szName);
	}
}

ZENITH_TEST(ZM_Data, Natures_AccessorContract)
{
	for (u_int i = 0; i < ZM_NATURE_COUNT; ++i)
	{
		ZENITH_ASSERT_STREQ(ZM_GetNatureName((ZM_NATURE)i), Nat(i).m_szName);
	}
	ZENITH_ASSERT_STREQ(ZM_GetNatureName(ZM_NATURE_COUNT), "NONE");
}
