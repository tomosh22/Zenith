#include "Zenith.h"

#include "Zenithmon/Source/Battle/ZM_Daycare.h"
#include "Zenithmon/Source/Battle/ZM_ExpAndLevel.h"   // ZM_ExpForLevel, ZM_LevelForExp, ZM_GetSpeciesGrowthRate
#include "Zenithmon/Source/Data/ZM_StatCalc.h"        // uZM_MIN_LEVEL, uZM_MAX_LEVEL

// ============================================================================
// ZM_Daycare implementation (S2 box 6 SC1). A pure state machine over
// ZM_DaycareState. The deposit/step/level model is RNG-free integer math that
// reuses the ZM_ExpAndLevel curves (mirrors the ZM-D-043 "no RNG in the exp
// path" invariant); RNG enters ONLY via ZM_DaycareCollectEgg -> ZM_GenerateEgg.
// See the header banner for the reduced-mechanic rulings (spec section 8).
// ============================================================================

namespace
{
	// The cumulative exp for a slot's current level (used to normalize an
	// UNSPECIFIED deposit and to anchor a defensively-unset counter). Clamps the
	// level into [1,100] before calling ZM_ExpForLevel (which asserts that band).
	u_int g_ExpFloorForSlot(const ZM_BattleMonsterSpec& xMon)
	{
		const ZM_GROWTH_RATE eRate = ZM_GetSpeciesGrowthRate(xMon.m_eSpecies);
		u_int uLevel = xMon.m_uLevel;
		if (uLevel < uZM_MIN_LEVEL) { uLevel = uZM_MIN_LEVEL; }
		if (uLevel > uZM_MAX_LEVEL) { uLevel = uZM_MAX_LEVEL; }
		return ZM_ExpForLevel(eRate, uLevel);
	}
}

u_int ZM_DaycareDeposit(ZM_DaycareState& xState, const ZM_BattleMonsterSpec& xMon)
{
	for (u_int uSlot = 0u; uSlot < uZM_DAYCARE_CAPACITY; ++uSlot)
	{
		ZM_DaycareSlot& xDst = xState.m_axSlots[uSlot];
		if (xDst.m_bOccupied)
		{
			continue;
		}
		xDst.m_bOccupied = true;
		xDst.m_xMonster  = xMon;
		// Normalize an UNSPECIFIED cumulative exp to this level's floor so a later
		// ZM_DaycareStep has a well-defined base to accumulate from.
		if (xDst.m_xMonster.m_uCurExp == uZM_EXP_UNSPECIFIED)
		{
			xDst.m_xMonster.m_uCurExp = g_ExpFloorForSlot(xDst.m_xMonster);
		}
		return uSlot;
	}
	return uZM_DAYCARE_CAPACITY;   // daycare full
}

bool ZM_DaycareWithdraw(ZM_DaycareState& xState, u_int uSlot, ZM_BattleMonsterSpec& xOut)
{
	if (uSlot >= uZM_DAYCARE_CAPACITY)
	{
		return false;
	}
	ZM_DaycareSlot& xSrc = xState.m_axSlots[uSlot];
	if (!xSrc.m_bOccupied)
	{
		return false;
	}
	xOut = xSrc.m_xMonster;         // the level/exp-updated spec
	xSrc = ZM_DaycareSlot{};        // clear occupancy + monster

	// A single-parent pair can no longer breed: reset egg progress.
	xState.m_uEggStepCounter = 0u;
	xState.m_bEggAvailable   = false;
	return true;
}

void ZM_DaycareStep(ZM_DaycareState& xState, u_int uSteps)
{
	if (uSteps == 0u)
	{
		return;
	}

	// Each occupied slot accumulates exp (saturating at the level-100 cap) and
	// recomputes its level. Pure integer; no RNG.
	for (u_int uSlot = 0u; uSlot < uZM_DAYCARE_CAPACITY; ++uSlot)
	{
		ZM_DaycareSlot& xSlot = xState.m_axSlots[uSlot];
		if (!xSlot.m_bOccupied)
		{
			continue;
		}
		const ZM_GROWTH_RATE eRate   = ZM_GetSpeciesGrowthRate(xSlot.m_xMonster.m_eSpecies);
		const u_int          uCapExp  = ZM_ExpForLevel(eRate, uZM_MAX_LEVEL);
		const u_int          uCurExp  = (xSlot.m_xMonster.m_uCurExp == uZM_EXP_UNSPECIFIED)
			? g_ExpFloorForSlot(xSlot.m_xMonster) : xSlot.m_xMonster.m_uCurExp;

		u_int64 ulNewExp = (u_int64)uCurExp + (u_int64)uSteps * (u_int64)uZM_DAYCARE_EXP_PER_STEP;
		if (ulNewExp > (u_int64)uCapExp)
		{
			ulNewExp = (u_int64)uCapExp;   // saturate at the level-100 exp cap; no wrap
		}
		xSlot.m_xMonster.m_uCurExp = (u_int)ulNewExp;
		xSlot.m_xMonster.m_uLevel  = ZM_LevelForExp(eRate, (u_int)ulNewExp);
	}

	// Egg cadence: the counter advances (saturating) ONLY while a compatible pair
	// is present -- a deterministic threshold, no RNG.
	if (ZM_DaycarePairCompatible(xState))
	{
		const u_int64 ulCounter = (u_int64)xState.m_uEggStepCounter + (u_int64)uSteps;
		if (ulCounter >= (u_int64)uZM_DAYCARE_EGG_STEP_THRESHOLD)
		{
			xState.m_uEggStepCounter = uZM_DAYCARE_EGG_STEP_THRESHOLD;
			xState.m_bEggAvailable   = true;
		}
		else
		{
			xState.m_uEggStepCounter = (u_int)ulCounter;
		}
	}
}

bool ZM_DaycarePairCompatible(const ZM_DaycareState& xState)
{
	if (!xState.m_axSlots[0].m_bOccupied || !xState.m_axSlots[1].m_bOccupied)
	{
		return false;
	}
	return ZM_AreCompatible(xState.m_axSlots[0].m_xMonster, xState.m_axSlots[1].m_xMonster);
}

u_int ZM_DaycareOccupancy(const ZM_DaycareState& xState)
{
	u_int uCount = 0u;
	for (u_int uSlot = 0u; uSlot < uZM_DAYCARE_CAPACITY; ++uSlot)
	{
		if (xState.m_axSlots[uSlot].m_bOccupied)
		{
			++uCount;
		}
	}
	return uCount;
}

bool ZM_DaycareCollectEgg(ZM_DaycareState& xState, ZM_BattleRNG& xRng,
	const ZM_BreedingParams& xParams, ZM_BattleMonsterSpec& xEggOut)
{
	if (!xState.m_bEggAvailable)
	{
		return false;
	}
	// Forward both parents; ZM_GenerateEgg derives the mother/father roles from gender
	// + the universal breeder (box-6 SC-B), so slot order does not matter. The
	// availability flag is only ever set for a compatible pair, so ZM_GenerateEgg's
	// precondition holds. Parents remain deposited (collecting an egg does not remove
	// them).
	xEggOut = ZM_GenerateEgg(xState.m_axSlots[0].m_xMonster,
		xState.m_axSlots[1].m_xMonster, xRng, xParams);
	xState.m_uEggStepCounter = 0u;
	xState.m_bEggAvailable   = false;
	return true;
}
