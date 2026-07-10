#include "Zenith.h"

// ============================================================================
// ZM_Tests_BattleRNG -- determinism + distribution tests for the PCG32 battle
// RNG (category ZM_Data). The golden stream pins the exact algorithm (so a
// battle replays bit-for-bit from its seed); the rest lock the bounded/chance
// helpers. See DecisionLog ZM-D-027.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/Data/ZM_BattleRNG.h"

// A fixed seed produces a fixed, known 32-bit stream (independently computed).
ZENITH_TEST(ZM_Data, BattleRNG_GoldenStream)
{
	const u_int32 auGolden[8] = {
		0x2C6B5AC7u, 0xDFB253B3u, 0xB40C9B72u, 0xBB17F46Fu,
		0x5B46277Au, 0xAF30A295u, 0xA30728FAu, 0xBC192629u,
	};
	ZM_BattleRNG xRng(0x1234ull, 54ull);
	for (u_int i = 0; i < 8; ++i)
	{
		ZENITH_ASSERT_EQ(xRng.Next(), auGolden[i], "PCG32 golden stream diverged at %u", i);
	}
}

// Same seed -> identical streams.
ZENITH_TEST(ZM_Data, BattleRNG_Deterministic)
{
	ZM_BattleRNG xA(0xABCDEF01ull);
	ZM_BattleRNG xB(0xABCDEF01ull);
	for (u_int i = 0; i < 64; ++i)
	{
		ZENITH_ASSERT_EQ(xA.Next(), xB.Next(), "same-seed streams diverged at %u", i);
	}
}

// Different seeds -> different streams (they must not all coincide).
ZENITH_TEST(ZM_Data, BattleRNG_DifferentSeedsDiffer)
{
	ZM_BattleRNG xA(1ull);
	ZM_BattleRNG xB(2ull);
	bool bAnyDiffer = false;
	for (u_int i = 0; i < 8; ++i)
	{
		if (xA.Next() != xB.Next()) { bAnyDiffer = true; }
	}
	ZENITH_ASSERT_TRUE(bAnyDiffer, "distinct seeds produced identical streams");
}

// RandBelow(n) stays in [0, n); RandBelow(1) is always 0.
ZENITH_TEST(ZM_Data, BattleRNG_RandBelowInRange)
{
	ZM_BattleRNG xRng(0x55AA55AAull);
	const u_int auBounds[] = { 1u, 2u, 6u, 100u, 1000u };
	for (u_int b = 0; b < sizeof(auBounds) / sizeof(auBounds[0]); ++b)
	{
		const u_int uBound = auBounds[b];
		for (u_int i = 0; i < 2000; ++i)
		{
			const u_int uV = xRng.RandBelow(uBound);
			ZENITH_ASSERT_LT(uV, uBound, "RandBelow(%u) returned %u", uBound, uV);
		}
	}
	for (u_int i = 0; i < 32; ++i)
	{
		ZENITH_ASSERT_EQ(xRng.RandBelow(1u), 0u);
	}
}

// RandRange(lo,hi) is inclusive; a degenerate range returns the single value.
ZENITH_TEST(ZM_Data, BattleRNG_RandRangeInclusive)
{
	ZM_BattleRNG xRng(0x99887766ull);
	for (u_int i = 0; i < 4000; ++i)
	{
		const u_int uV = xRng.RandRange(10u, 20u);
		ZENITH_ASSERT_GE(uV, 10u);
		ZENITH_ASSERT_LE(uV, 20u);
	}
	for (u_int i = 0; i < 16; ++i)
	{
		ZENITH_ASSERT_EQ(xRng.RandRange(7u, 7u), 7u);
	}
}

// Chance contract: 0/d never true, d/d always true, and ~p over many trials.
ZENITH_TEST(ZM_Data, BattleRNG_ChanceContract)
{
	ZM_BattleRNG xRng(0xDEADBEEFull);
	for (u_int i = 0; i < 64; ++i)
	{
		ZENITH_ASSERT_FALSE(xRng.Chance(0u, 16u), "Chance(0,d) fired");
		ZENITH_ASSERT_TRUE(xRng.Chance(16u, 16u), "Chance(d,d) missed");
		ZENITH_ASSERT_FALSE(xRng.ChancePercent(0u), "ChancePercent(0) fired");
		ZENITH_ASSERT_TRUE(xRng.ChancePercent(100u), "ChancePercent(100) missed");
	}
	// ~50% over 10000 deterministic trials, generous tolerance (no flakiness).
	u_int uHits = 0;
	for (u_int i = 0; i < 10000; ++i)
	{
		if (xRng.Chance(1u, 2u)) { ++uHits; }
	}
	ZENITH_ASSERT_GE(uHits, 4500u, "Chance(1,2) fired only %u/10000 times", uHits);
	ZENITH_ASSERT_LE(uHits, 5500u, "Chance(1,2) fired %u/10000 times", uHits);
}
