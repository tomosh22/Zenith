#include "Zenith.h"

#include "Zenithmon/Source/Battle/ZM_DamageCalc.h"
#include "Zenithmon/Source/Data/ZM_TypeChart.h"   // ZM_TypeChart::GetDualTypeEffectiveness

// Effective stat after a stat-stage multiplier. Box 1 always passes stage 0
// (identity: floor(stat*2/2) == stat), but the general Gen-V formula is
// implemented so box 2 can raise/lower stages with no change here.
u_int ZM_ApplyStatStage(u_int uStat, int iStage)
{
	if (iStage >= 0)
	{
		return (uStat * (u_int)(2 + iStage)) / 2u;
	}
	// iStage < 0 -> denominator (2 - iStage) is in [3, 8] for stages [-1,-6].
	return (uStat * 2u) / (u_int)(2 - iStage);
}

// Type effectiveness as an exact integer percent. The chart only ever returns
// {0,0.5,1,2} per lookup and their products {0,0.25,0.5,1,2,4}, so *100 lands
// exactly on {0,25,50,100,200,400}; the +0.5 truncation is a safe round.
u_int ZM_EffectivenessPercent(ZM_TYPE eMoveType, ZM_TYPE eDefType1, ZM_TYPE eDefType2)
{
	const float fMultiplier = ZM_TypeChart::GetDualTypeEffectiveness(eMoveType, eDefType1, eDefType2);
	return (u_int)(fMultiplier * 100.0f + 0.5f);
}

// The locked Gen-V pipeline: floor after every multiply. 64-bit products where a
// multiply could exceed 32 bits (the level*power*attack term at high level/power,
// and the per-percent modifiers) so the result is overflow-proof yet bit-identical
// to the intended integer floor at every step.
u_int ZM_CalcDamage(const ZM_DamageInput& xIn)
{
	Zenith_Assert(xIn.uDefense > 0u, "ZM_CalcDamage: defending stat must be > 0");
	Zenith_Assert(xIn.uWeatherDen > 0u, "ZM_CalcDamage: weather denominator must be > 0");

	// a = floor(2*level/5) + 2
	const u_int uA = (2u * xIn.uLevel) / 5u + 2u;

	// t = floor( a * power * attack / defense )  -- 64-bit product, then / defense
	const u_int64 ulT = ((u_int64)uA * (u_int64)xIn.uPower * (u_int64)xIn.uAttack) / (u_int64)xIn.uDefense;

	// base = floor( t / 50 ) + 2
	u_int uD = (u_int)(ulT / 50ull) + 2u;

	// weather (box 1 identity 1/1)
	uD = (u_int)(((u_int64)uD * (u_int64)xIn.uWeatherNum) / (u_int64)xIn.uWeatherDen);
	// crit x1.5 (GDD-locked 1/24 rate, x1.5 multiplier)
	if (xIn.bCrit)
	{
		uD = (u_int)(((u_int64)uD * 3ull) / 2ull);
	}
	// random roll 85..100
	uD = (u_int)(((u_int64)uD * (u_int64)xIn.uRandomPercent) / 100ull);
	// STAB x1.5
	if (xIn.bStab)
	{
		uD = (u_int)(((u_int64)uD * 3ull) / 2ull);
	}
	// type effectiveness (0 -> immune)
	uD = (u_int)(((u_int64)uD * (u_int64)xIn.uEffectivenessPercent) / 100ull);
	// burn (box 1 false)
	if (xIn.bBurnedPhysical)
	{
		uD = uD / 2u;
	}
	// screen (box 1 false)
	if (xIn.bScreen)
	{
		uD = uD / 2u;
	}

	return (xIn.uEffectivenessPercent == 0u) ? 0u : (uD < 1u ? 1u : uD);
}
