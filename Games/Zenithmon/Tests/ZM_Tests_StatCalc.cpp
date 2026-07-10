#include "Zenith.h"

// ============================================================================
// ZM_Tests_StatCalc -- golden-vector tests for the Gen-III+ stat formulas
// (category ZM_Data). The expected values are hand/independently computed (see
// the formula in ZM_StatCalc.h) so any drift in the integer math is caught as a
// deliberate change. See DecisionLog ZM-D-027.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"
#include "Zenithmon/Source/Data/ZM_NatureData.h"
#include "Zenithmon/Source/Data/ZM_StatCalc.h"

// HP = ((2*base + IV + EV/4) * level)/100 + level + 10
ZENITH_TEST(ZM_Data, StatCalc_HpGoldenVectors)
{
	ZENITH_ASSERT_EQ(ZM_CalcHPStat(45,   0,   0,   5), 19u);
	ZENITH_ASSERT_EQ(ZM_CalcHPStat(45,  31,   0,  50), 120u);
	ZENITH_ASSERT_EQ(ZM_CalcHPStat(100, 31, 252, 100), 404u);
	ZENITH_ASSERT_EQ(ZM_CalcHPStat(1,    0,   0,   1), 11u);
	ZENITH_ASSERT_EQ(ZM_CalcHPStat(255, 31, 252, 100), 714u);
	ZENITH_ASSERT_EQ(ZM_CalcHPStat(80,  31, 252,  50), 187u);
	ZENITH_ASSERT_EQ(ZM_CalcHPStat(60,   0,   0, 100), 230u);
}

// other = (((2*base + IV + EV/4)*level)/100 + 5) * naturePercent/100
ZENITH_TEST(ZM_Data, StatCalc_OtherGoldenVectors)
{
	ZENITH_ASSERT_EQ(ZM_CalcOtherStat(100, 31,   0,  50, 100), 120u);
	ZENITH_ASSERT_EQ(ZM_CalcOtherStat(100, 31,   0,  50, 110), 132u);
	ZENITH_ASSERT_EQ(ZM_CalcOtherStat(100, 31,   0,  50,  90), 108u);
	ZENITH_ASSERT_EQ(ZM_CalcOtherStat(100, 31, 252, 100, 100), 299u);
	ZENITH_ASSERT_EQ(ZM_CalcOtherStat(100, 31, 252, 100, 110), 328u);
	ZENITH_ASSERT_EQ(ZM_CalcOtherStat(100, 31, 252, 100,  90), 269u);
	ZENITH_ASSERT_EQ(ZM_CalcOtherStat(50,   0,   0,  50, 100), 55u);
	ZENITH_ASSERT_EQ(ZM_CalcOtherStat(50,   0,   0,   1, 100), 6u);
	ZENITH_ASSERT_EQ(ZM_CalcOtherStat(120, 31, 252, 100, 110), 372u);
	ZENITH_ASSERT_EQ(ZM_CalcOtherStat(70,  15, 100,  50,  90), 85u);
}

// ZM_CalcStat dispatches HP vs other and applies the nature to the right stats.
ZENITH_TEST(ZM_Data, StatCalc_NatureDispatch)
{
	// HP ignores nature entirely.
	ZENITH_ASSERT_EQ(ZM_CalcStat(ZM_STAT_HP, 100, 31, 252, 100, ZM_NATURE_RECKLESS),
		ZM_CalcHPStat(100, 31, 252, 100));

	// Reckless raises ATTACK (110%), lowers DEFENSE (90%), leaves the rest (100%).
	ZENITH_ASSERT_EQ(ZM_CalcStat(ZM_STAT_ATTACK, 100, 31, 0, 50, ZM_NATURE_RECKLESS),
		ZM_CalcOtherStat(100, 31, 0, 50, 110));
	ZENITH_ASSERT_EQ(ZM_CalcStat(ZM_STAT_DEFENSE, 100, 31, 0, 50, ZM_NATURE_RECKLESS),
		ZM_CalcOtherStat(100, 31, 0, 50, 90));
	ZENITH_ASSERT_EQ(ZM_CalcStat(ZM_STAT_SPEED, 100, 31, 0, 50, ZM_NATURE_RECKLESS),
		ZM_CalcOtherStat(100, 31, 0, 50, 100));

	// A neutral nature is 100% on every stat.
	ZENITH_ASSERT_EQ(ZM_CalcStat(ZM_STAT_ATTACK, 100, 31, 0, 50, ZM_NATURE_FERAL),
		ZM_CalcOtherStat(100, 31, 0, 50, 100));
}

// Stats are non-decreasing in base / IV / EV / level.
ZENITH_TEST(ZM_Data, StatCalc_Monotonic)
{
	ZENITH_ASSERT_LE(ZM_CalcOtherStat(100,  0, 0, 50, 100), ZM_CalcOtherStat(100, 31, 0, 50, 100));
	ZENITH_ASSERT_LE(ZM_CalcOtherStat(100, 31, 0, 50, 100), ZM_CalcOtherStat(100, 31, 252, 50, 100));
	ZENITH_ASSERT_LE(ZM_CalcOtherStat(100, 31, 0, 50, 100), ZM_CalcOtherStat(100, 31, 0, 100, 100));
	ZENITH_ASSERT_LE(ZM_CalcOtherStat(50, 31, 0, 50, 100),  ZM_CalcOtherStat(120, 31, 0, 50, 100));
	ZENITH_ASSERT_LE(ZM_CalcHPStat(45, 0, 0, 5), ZM_CalcHPStat(45, 0, 0, 50));
	// The nature down-multiplier never raises a stat above neutral.
	ZENITH_ASSERT_LE(ZM_CalcOtherStat(100, 31, 0, 50, 90), ZM_CalcOtherStat(100, 31, 0, 50, 100));
	ZENITH_ASSERT_LE(ZM_CalcOtherStat(100, 31, 0, 50, 100), ZM_CalcOtherStat(100, 31, 0, 50, 110));
}
