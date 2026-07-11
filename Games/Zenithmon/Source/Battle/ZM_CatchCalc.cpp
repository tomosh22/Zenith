#include "Zenith.h"

#include "Zenithmon/Source/Battle/ZM_CatchCalc.h"

// ============================================================================
// ZM_CatchCalc implementation (S2 box 2, SC6). Integer-only so a capture / flee
// replays bit-for-bit from the battle seed; see the header for the LOCKED formula
// (ZM-D-035) and the ZM-D-033 Q2 rate / status contract.
// ============================================================================

// Integer floor sqrt (binary search; NO floating point -- the catch value must be
// bit-exact). Inputs here never exceed 16,711,680 (== 255*65536, at a == 1), whose
// root is 4088, so a u_int search bounded by 65535 covers every case with margin.
static u_int g_IntSqrt(u_int uValue)
{
	u_int uLo = 0u;
	u_int uHi = (uValue < 65535u) ? uValue : 65535u;
	while (uLo < uHi)
	{
		const u_int uMid = uLo + (uHi - uLo + 1u) / 2u;
		if ((u_int64)uMid * (u_int64)uMid <= (u_int64)uValue) { uLo = uMid; }
		else                                                  { uHi = uMid - 1u; }
	}
	return uLo;
}

u_int ZM_CatchCalc::BaseCatchRate(ZM_RARITY eRarity)
{
	switch (eRarity)
	{
	case ZM_RARITY_COMMON:    return 190u;
	case ZM_RARITY_UNCOMMON:  return 120u;
	case ZM_RARITY_RARE:      return 45u;
	case ZM_RARITY_LEGENDARY: return 3u;
	default:                  return 0u;
	}
}

void ZM_CatchCalc::StatusModifier(ZM_MAJOR_STATUS eStatus, u_int& uNum, u_int& uDen)
{
	switch (eStatus)
	{
	case ZM_MAJOR_STATUS_SLEEP:
	case ZM_MAJOR_STATUS_FREEZE:
		uNum = 5u; uDen = 2u; return;   // x2.5
	case ZM_MAJOR_STATUS_PARALYSIS:
	case ZM_MAJOR_STATUS_POISON:
	case ZM_MAJOR_STATUS_TOXIC:
	case ZM_MAJOR_STATUS_BURN:
		uNum = 3u; uDen = 2u; return;   // x1.5
	default:
		uNum = 1u; uDen = 1u; return;   // x1
	}
}

u_int ZM_CatchCalc::BallCatchParam(ZM_ITEM_ID eBall)
{
	if (eBall >= ZM_ITEM_COUNT) { return 0u; }
	const ZM_ItemData& xItem = ZM_GetItemData(eBall);
	if (xItem.m_eCategory != ZM_ITEM_CATEGORY_BALL) { return 0u; }
	return xItem.m_uEffectParam;
}

bool ZM_CatchCalc::IsGuaranteedBall(ZM_ITEM_ID eBall)
{
	return BallCatchParam(eBall) >= 255u;
}

u_int ZM_CatchCalc::ModifiedCatchValue(u_int uMaxHP, u_int uCurHP, u_int uBaseRate,
	u_int uBallX10, u_int uStatusNum, u_int uStatusDen)
{
	if (uMaxHP == 0u || uStatusDen == 0u) { return 1u; }
	const u_int uHpTerm = 3u * uMaxHP - 2u * uCurHP;
	u_int64 ulA = ((u_int64)uHpTerm * (u_int64)uBaseRate * (u_int64)uBallX10)
		/ ((u_int64)3u * (u_int64)uMaxHP * (u_int64)10u);
	ulA = (ulA * (u_int64)uStatusNum) / (u_int64)uStatusDen;
	if (ulA < 1ull) { ulA = 1ull; }
	return (u_int)ulA;
}

u_int ZM_CatchCalc::ShakeProbability(u_int uCatchValueA)
{
	if (uCatchValueA >= 255u) { return 65536u; }   // sentinel: always passes
	const u_int uInner = g_IntSqrt(16711680u / uCatchValueA);
	const u_int uOuter = g_IntSqrt(uInner);
	if (uOuter == 0u) { return 65535u; }            // defensive (unreachable for a in [1,254])
	return 1048560u / uOuter;
}

ZM_CatchResult ZM_CatchCalc::Roll(const ZM_BattleMonster& xTarget, ZM_ITEM_ID eBall, ZM_BattleRNG& xRNG)
{
	ZM_CatchResult xResult;

	// A guaranteed ball (PRIMEORB) captures with no draws.
	if (IsGuaranteedBall(eBall))
	{
		xResult.m_bCaught = true;
		xResult.m_uShakes = 4u;
		xResult.m_uCatchValueA = 255u;
		return xResult;
	}

	const u_int uBallX10 = BallCatchParam(eBall);
	const u_int uBaseRate = BaseCatchRate(ZM_GetSpeciesData(xTarget.m_eSpecies).m_eRarity);
	u_int uNum = 1u;
	u_int uDen = 1u;
	StatusModifier(xTarget.m_eStatus, uNum, uDen);

	const u_int uA = ModifiedCatchValue(xTarget.m_auMaxStat[ZM_STAT_HP], xTarget.m_uCurHP,
		uBaseRate, uBallX10, uNum, uDen);
	xResult.m_uCatchValueA = uA;

	// a >= 255 captures with no draws (Gen-III/IV immediate catch).
	if (uA >= 255u)
	{
		xResult.m_bCaught = true;
		xResult.m_uShakes = 4u;
		return xResult;
	}

	const u_int uB = ShakeProbability(uA);
	u_int uShakes = 0u;
	for (u_int k = 0u; k < 4u; ++k)
	{
		if (xRNG.RandBelow(65536u) < uB) { ++uShakes; }
		else                             { break; }
	}
	xResult.m_uShakes = uShakes;
	xResult.m_bCaught = (uShakes == 4u);
	return xResult;
}

ZM_FleeOdds ZM_ComputeFleeOdds(u_int uSelfSpeed, u_int uOppSpeed, u_int uAttempt)
{
	ZM_FleeOdds xOdds;
	if (uSelfSpeed >= uOppSpeed || uOppSpeed == 0u)
	{
		xOdds.m_bGuaranteed = true;
		xOdds.m_uThreshold  = 0u;
		return xOdds;
	}
	xOdds.m_bGuaranteed = false;
	xOdds.m_uThreshold  = ((uSelfSpeed * 128u) / uOppSpeed + 30u * uAttempt) % 256u;
	return xOdds;
}

bool ZM_RollFlee(u_int uSelfSpeed, u_int uOppSpeed, u_int uAttempt, ZM_BattleRNG& xRNG)
{
	const ZM_FleeOdds xOdds = ZM_ComputeFleeOdds(uSelfSpeed, uOppSpeed, uAttempt);
	if (xOdds.m_bGuaranteed) { return true; }
	return xRNG.RandBelow(256u) < xOdds.m_uThreshold;
}
