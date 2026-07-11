#include "Zenith.h"

#include "Zenithmon/Source/Battle/ZM_StatusLogic.h"
#include "Zenithmon/Source/Battle/ZM_MoveExecutor.h"   // ZM_MoveContext full definition
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"       // ZM_GetSpeciesData -> defender types

// ============================================================================
// ZM_StatusLogic -- SC4 major-status apply / block / gate / turn-end / cure.
// Constants (ZM-D-033): thaw 20%, full-para 25%, chips burn/poison 1/8, toxic
// n/16, sleep duration RandRange(1,3). Every draw is guarded on a status the
// monster actually holds, so a status-free monster pulls ZERO draws and the
// box-1 / SC1-SC3 goldens stay byte-identical.
// ============================================================================

// True iff the monster's species carries eType (either slot).
static bool g_HasType(const ZM_BattleMonster& xMon, ZM_TYPE eType)
{
	const ZM_SpeciesData& xSpecies = ZM_GetSpeciesData(xMon.m_eSpecies);
	return xSpecies.m_aeTypes[0] == eType || xSpecies.m_aeTypes[1] == eType;
}

bool ZM_StatusLogic::CanApplyMajor(const ZM_BattleMonster& xMon, ZM_MAJOR_STATUS eStatus)
{
	if (xMon.m_eStatus != ZM_MAJOR_STATUS_NONE)
	{
		return false;   // one major at a time
	}
	switch (eStatus)
	{
	case ZM_MAJOR_STATUS_BURN:      return !g_HasType(xMon, ZM_TYPE_FIRE);
	case ZM_MAJOR_STATUS_FREEZE:    return !g_HasType(xMon, ZM_TYPE_ICE);
	case ZM_MAJOR_STATUS_POISON:
	case ZM_MAJOR_STATUS_TOXIC:     return !g_HasType(xMon, ZM_TYPE_VENOM) && !g_HasType(xMon, ZM_TYPE_IRON);
	case ZM_MAJOR_STATUS_SLEEP:
	case ZM_MAJOR_STATUS_PARALYSIS: return true;   // no type block in box 2
	default:                        return false;  // NONE / COUNT -- not an inflictable major
	}
}

bool ZM_StatusLogic::ApplyMajor(ZM_MoveContext& xCtx, ZM_SIDE eTgt, ZM_MAJOR_STATUS eStatus)
{
	ZM_BattleSide&    xTgtSide = xCtx.m_pxState->Side(eTgt);
	ZM_BattleMonster& xTgt     = xTgtSide.Active();

	if (!CanApplyMajor(xTgt, eStatus))
	{
		return false;   // blocked -- caller announces MOVE_FAILED for a primary; silent for a secondary
	}

	xTgt.m_eStatus = eStatus;
	if (eStatus == ZM_MAJOR_STATUS_SLEEP)
	{
		xTgt.m_uStatusCounter = xCtx.RNG().RandRange(1u, 3u);   // the ONLY apply-time draw
	}
	else if (eStatus == ZM_MAJOR_STATUS_TOXIC)
	{
		xTgt.m_uStatusCounter = 1u;                              // ramp start; no draw
	}
	else
	{
		xTgt.m_uStatusCounter = 0u;
	}

	xCtx.Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_STATUS_APPLIED, eTgt, xTgtSide.m_uActiveSlot,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, (int)eStatus));
	return true;
}

bool ZM_StatusLogic::CureMajor(ZM_MoveContext& xCtx, ZM_SIDE eTgt)
{
	ZM_BattleSide&    xTgtSide = xCtx.m_pxState->Side(eTgt);
	ZM_BattleMonster& xTgt     = xTgtSide.Active();

	if (xTgt.m_eStatus == ZM_MAJOR_STATUS_NONE)
	{
		return false;
	}
	const ZM_MAJOR_STATUS eOld = xTgt.m_eStatus;
	xTgt.m_eStatus        = ZM_MAJOR_STATUS_NONE;
	xTgt.m_uStatusCounter = 0u;
	xCtx.Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_STATUS_CURED, eTgt, xTgtSide.m_uActiveSlot,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, (int)eOld));
	return true;
}

ZM_GATE_RESULT ZM_StatusLogic::PreMoveGate(ZM_MoveContext& xCtx)
{
	ZM_BattleSide&    xSide = xCtx.AtkSide();
	ZM_BattleMonster& xMon  = xCtx.Atk();

	// G1 RECHARGE -- SC5 volatile gate (no-op slot; no draw).

	// G2 FREEZE -- 20% thaw, else cancel. [1 draw iff frozen]
	if (xMon.m_eStatus == ZM_MAJOR_STATUS_FREEZE)
	{
		if (xCtx.RNG().RandBelow(100u) < 20u)
		{
			CureMajor(xCtx, xCtx.m_eAtk);   // thaw -> STATUS_CURED, then proceed this turn
		}
		else
		{
			xCtx.Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_FAILED, xCtx.m_eAtk, xSide.m_uActiveSlot,
				ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, (int)ZM_MOVE_FAIL_FROZEN));
			return ZM_GATE_CANCELLED;
		}
	}

	// G3 SLEEP -- decrement-then-check; wake at 0, else cancel. [0 draws]
	if (xMon.m_eStatus == ZM_MAJOR_STATUS_SLEEP)
	{
		--xMon.m_uStatusCounter;
		if (xMon.m_uStatusCounter == 0u)
		{
			CureMajor(xCtx, xCtx.m_eAtk);   // wake -> STATUS_CURED, then act this turn
		}
		else
		{
			xCtx.Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_FAILED, xCtx.m_eAtk, xSide.m_uActiveSlot,
				ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, (int)ZM_MOVE_FAIL_ASLEEP));
			return ZM_GATE_CANCELLED;
		}
	}

	// G4 FLINCH / G5 CONFUSE -- SC5 volatile gates (no-op slots; no draw).

	// G6 PARALYSIS -- 25% full-para. [1 draw iff paralyzed]
	if (xMon.m_eStatus == ZM_MAJOR_STATUS_PARALYSIS)
	{
		if (xCtx.RNG().RandBelow(100u) < 25u)
		{
			xCtx.Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_FAILED, xCtx.m_eAtk, xSide.m_uActiveSlot,
				ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, (int)ZM_MOVE_FAIL_FULLY_PARALYZED));
			return ZM_GATE_CANCELLED;
		}
	}

	return ZM_GATE_PROCEED;
}

void ZM_StatusLogic::EndOfTurn(ZM_BattleState& xState, Zenith_Vector<ZM_BattleEvent>& xEvents, ZM_SIDE eSide)
{
	ZM_BattleSide&    xSide = xState.Side(eSide);
	ZM_BattleMonster& xMon  = xSide.Active();

	// A fainted active takes no chip (its replacement is an SC5 concern). Box-1-safe.
	if (xMon.IsFainted())
	{
		return;
	}

	const ZM_MAJOR_STATUS eStatus = xMon.m_eStatus;
	if (eStatus != ZM_MAJOR_STATUS_POISON && eStatus != ZM_MAJOR_STATUS_TOXIC
		&& eStatus != ZM_MAJOR_STATUS_BURN)
	{
		return;   // NONE / SLEEP / FREEZE / PARALYSIS do not chip
	}

	const u_int uMaxHP = xMon.m_auMaxStat[ZM_STAT_HP];
	u_int uDmg = (eStatus == ZM_MAJOR_STATUS_TOXIC)
		? (uMaxHP * xMon.m_uStatusCounter) / 16u
		: uMaxHP / 8u;
	if (uDmg == 0u) { uDmg = 1u; }   // min-1 chip (maxHP < 8, or a 0-counter guard) so it always ticks

	xMon.m_uCurHP = (uDmg >= xMon.m_uCurHP) ? 0u : (xMon.m_uCurHP - uDmg);

	xEvents.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_STATUS_DAMAGE, eSide, xSide.m_uActiveSlot,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, (int)uDmg, (int)xMon.m_uCurHP, (int)eStatus));

	if (eStatus == ZM_MAJOR_STATUS_TOXIC)
	{
		++xMon.m_uStatusCounter;   // ramp AFTER charging this tick
	}

	if (xMon.m_uCurHP == 0u)
	{
		xEvents.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_FAINT, eSide, xSide.m_uActiveSlot));
	}
}
