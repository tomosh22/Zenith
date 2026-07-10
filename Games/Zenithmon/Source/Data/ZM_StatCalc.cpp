#include "Zenith.h"
#include "Zenithmon/Source/Data/ZM_StatCalc.h"

// ============================================================================
// ZM_StatCalc -- Gen-III+ integer stat formulas (DecisionLog ZM-D-027). Golden
// vectors in Tests/ZM_Tests_StatCalc.cpp pin the exact outputs.
// ============================================================================

namespace
{
	// The shared inner term: ((2*base + IV + EV/4) * level) / 100. Truncating
	// division, done in the natural range (max ~ (2*255+31+63)*100/100 = 604).
	u_int StatCore(u_int uBase, u_int uIV, u_int uEV, u_int uLevel)
	{
		return ((2u * uBase + uIV + uEV / 4u) * uLevel) / 100u;
	}
}

u_int ZM_CalcHPStat(u_int uBase, u_int uIV, u_int uEV, u_int uLevel)
{
	Zenith_Assert(uLevel >= uZM_MIN_LEVEL && uLevel <= uZM_MAX_LEVEL, "ZM_CalcHPStat: level %u out of range", uLevel);
	Zenith_Assert(uIV <= uZM_MAX_IV, "ZM_CalcHPStat: IV %u > 31", uIV);
	return StatCore(uBase, uIV, uEV, uLevel) + uLevel + 10u;
}

u_int ZM_CalcOtherStat(u_int uBase, u_int uIV, u_int uEV, u_int uLevel, u_int uNaturePercent)
{
	Zenith_Assert(uLevel >= uZM_MIN_LEVEL && uLevel <= uZM_MAX_LEVEL, "ZM_CalcOtherStat: level %u out of range", uLevel);
	Zenith_Assert(uIV <= uZM_MAX_IV, "ZM_CalcOtherStat: IV %u > 31", uIV);
	const u_int uPreNature = StatCore(uBase, uIV, uEV, uLevel) + 5u;
	return (uPreNature * uNaturePercent) / 100u;
}

u_int ZM_CalcStat(ZM_STAT eStat, u_int uBase, u_int uIV, u_int uEV, u_int uLevel, ZM_NATURE eNature)
{
	if (eStat == ZM_STAT_HP)
	{
		return ZM_CalcHPStat(uBase, uIV, uEV, uLevel);
	}
	return ZM_CalcOtherStat(uBase, uIV, uEV, uLevel, ZM_GetNatureStatPercent(eNature, eStat));
}
