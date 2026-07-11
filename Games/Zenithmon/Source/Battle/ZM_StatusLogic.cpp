#include "Zenith.h"

#include "Zenithmon/Source/Battle/ZM_StatusLogic.h"
#include "Zenithmon/Source/Battle/ZM_MoveExecutor.h"   // ZM_MoveContext full definition
#include "Zenithmon/Source/Battle/ZM_DamageCalc.h"
#include "Zenithmon/Source/Battle/ZM_AbilityHooks.h"
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"       // ZM_GetSpeciesData -> defender types

// ============================================================================
// ZM_StatusLogic -- SC4 major statuses + SC5 volatile lifecycle/gates/EOT.
// Constants (ZM-D-033): thaw 20%, confusion self-hit 33%, full-para 25%; chips
// burn/poison/leech/trap 1/8, toxic n/16. Every draw is guarded on state the
// monster actually holds or a newly successful application, so an unaffected
// monster pulls ZERO draws and the
// box-1 / SC1-SC3 goldens stay byte-identical.
// ============================================================================

// True iff the monster's species carries eType (either slot).
static bool g_HasType(const ZM_BattleMonster& xMon, ZM_TYPE eType)
{
	const ZM_SpeciesData& xSpecies = ZM_GetSpeciesData(xMon.m_eSpecies);
	return xSpecies.m_aeTypes[0] == eType || xSpecies.m_aeTypes[1] == eType;
}

static bool g_HasVolatile(const ZM_BattleMonster& xMon, ZM_VOLATILE eVolatile)
{
	return (xMon.m_uVolatileMask & (u_int)eVolatile) != 0u;
}

bool ZM_StatusLogic::ApplyStatChange(ZM_BattleState& xState,
	Zenith_Vector<ZM_BattleEvent>& xEvents, ZM_SIDE eTgt, ZM_BATTLE_STAT eStat,
	int iDelta, bool bPrimary, bool bFromFoe)
{
	ZM_BattleSide&    xTgtSide = xState.Side(eTgt);
	ZM_BattleMonster& xTgt     = xTgtSide.Active();

	// The hook slot is introduced in SC2 so later ability bodies can veto at the
	// one mutation seam. Until SC4 populates those slots this branch is inert.
	if (iDelta < 0 && bFromFoe)
	{
		const ZM_AbilityHooks* pxHooks = ZM_GetAbilityHooks(xTgt.m_eAbility);
		if (pxHooks != nullptr && pxHooks->pfnPreventStatDrop != nullptr)
		{
			ZM_AbilityContext xAbilityCtx;
			xAbilityCtx.m_pxState = &xState;
			xAbilityCtx.m_pxEvents = &xEvents;
			xAbilityCtx.m_eSelf = eTgt;
			xAbilityCtx.m_eOther = eTgt == ZM_SIDE_PLAYER ? ZM_SIDE_ENEMY : ZM_SIDE_PLAYER;
			if (pxHooks->pfnPreventStatDrop(xAbilityCtx, eStat))
			{
				xEvents.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_ABILITY_TRIGGER, eTgt,
					xTgtSide.m_uActiveSlot, ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, 0,
					(int)xTgt.m_eAbility));
				return false;
			}
		}
	}

	const int iOld = xTgt.m_aiStage[eStat];
	int iNew = iOld + iDelta;
	if (iNew > iZM_MAX_STAGE) { iNew = iZM_MAX_STAGE; }
	if (iNew < iZM_MIN_STAGE) { iNew = iZM_MIN_STAGE; }

	if (iNew == iOld)
	{
		if (bPrimary)
		{
			xEvents.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_FAILED, eTgt,
				xTgtSide.m_uActiveSlot, ZM_MOVE_NONE, ZM_SPECIES_NONE, 0,
				(int)ZM_MOVE_FAIL_STAT_MAXED));
		}
		return false;
	}

	xTgt.m_aiStage[eStat] = iNew;
	xEvents.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_STAT_STAGE_CHANGED, eTgt,
		xTgtSide.m_uActiveSlot, ZM_MOVE_NONE, ZM_SPECIES_NONE, iNew - iOld,
		(int)eStat));
	return true;
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

bool ZM_StatusLogic::ApplyMajor(ZM_BattleState& xState,
	Zenith_Vector<ZM_BattleEvent>& xEvents, ZM_SIDE eTgt, ZM_MAJOR_STATUS eStatus)
{
	ZM_BattleSide&    xTgtSide = xState.Side(eTgt);
	ZM_BattleMonster& xTgt     = xTgtSide.Active();

	if (!CanApplyMajor(xTgt, eStatus))
	{
		return false;   // blocked -- caller announces MOVE_FAILED for a primary; silent for a secondary
	}

	// SC4 STATUS_TRY: an ability may block this major AFTER the type/duplicate gate
	// (so a type-immune/duplicate block stays SILENT -- it fails CanApplyMajor first).
	// On an ability block emit ONE ABILITY_TRIGGER (m_iAmount = blocked status ordinal,
	// m_iTag = ability), apply nothing, and pull NO apply-time draw (a Wakeful-blocked
	// SLEEP never draws the RandRange(1,3) duration). NONE-ability targets skip this.
	const ZM_AbilityHooks* pxHooks = ZM_GetAbilityHooks(xTgt.m_eAbility);
	if (pxHooks != nullptr && pxHooks->pfnPreventMajor != nullptr
		&& pxHooks->pfnPreventMajor(eStatus))
	{
		xEvents.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_ABILITY_TRIGGER, eTgt,
			xTgtSide.m_uActiveSlot, ZM_MOVE_NONE, ZM_SPECIES_NONE, (int)eStatus, 0,
			(int)xTgt.m_eAbility));
		return false;
	}

	xTgt.m_eStatus = eStatus;
	if (eStatus == ZM_MAJOR_STATUS_SLEEP)
	{
		xTgt.m_uStatusCounter = xState.m_xRNG.RandRange(1u, 3u);   // the ONLY apply-time draw
	}
	else if (eStatus == ZM_MAJOR_STATUS_TOXIC)
	{
		xTgt.m_uStatusCounter = 1u;                              // ramp start; no draw
	}
	else
	{
		xTgt.m_uStatusCounter = 0u;
	}

	xEvents.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_STATUS_APPLIED, eTgt, xTgtSide.m_uActiveSlot,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, (int)eStatus));
	return true;
}

bool ZM_StatusLogic::ApplyMajor(ZM_MoveContext& xCtx, ZM_SIDE eTgt, ZM_MAJOR_STATUS eStatus)
{
	return ApplyMajor(*xCtx.m_pxState, *xCtx.m_pxEvents, eTgt, eStatus);
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

bool ZM_StatusLogic::CanApplyVolatile(const ZM_BattleMonster& xMon, ZM_VOLATILE eVolatile)
{
	if (eVolatile == ZM_VOLATILE_NONE)
	{
		return false;
	}
	// Protect is the one refreshable volatile: a repeat application re-emits
	// APPLIED without an intermediate ENDED event or any repeat-use RNG.
	if (eVolatile != ZM_VOLATILE_PROTECT && g_HasVolatile(xMon, eVolatile))
	{
		return false;
	}
	if (eVolatile == ZM_VOLATILE_LEECH_SEED && g_HasType(xMon, ZM_TYPE_GRASS))
	{
		return false;
	}
	switch (eVolatile)
	{
	case ZM_VOLATILE_CONFUSED:
	case ZM_VOLATILE_FLINCH:
	case ZM_VOLATILE_LEECH_SEED:
	case ZM_VOLATILE_PROTECT:
	case ZM_VOLATILE_CHARGE:
	case ZM_VOLATILE_SEMI_INVULN:
	case ZM_VOLATILE_RECHARGE:
	case ZM_VOLATILE_LOCK:
	case ZM_VOLATILE_TRAP:
	case ZM_VOLATILE_TAUNT:
		return true;
	default:
		return false;
	}
}

bool ZM_StatusLogic::ApplyVolatile(ZM_MoveContext& xCtx, ZM_SIDE eTgt,
	ZM_VOLATILE eVolatile, ZM_SIDE eSource)
{
	ZM_BattleSide& xSide = xCtx.m_pxState->Side(eTgt);
	ZM_BattleMonster& xMon = xSide.Active();
	if (!CanApplyVolatile(xMon, eVolatile))
	{
		return false;
	}

	// SC4 STATUS_TRY: an ability (Ownpace -> CONFUSED) may block this volatile AFTER
	// the duplicate/type gate. CanApplyVolatile stays PURE (the g_ApplyDamagingSecondary
	// preflight still lets a secondary-confuse proc DRAW), so the roll-then-veto order
	// holds: emit ONE ABILITY_TRIGGER (m_iAmount = blocked volatile bit), apply nothing,
	// pull NO duration draw. NONE-ability targets skip this branch (byte-identical).
	const ZM_AbilityHooks* pxHooks = ZM_GetAbilityHooks(xMon.m_eAbility);
	if (pxHooks != nullptr && pxHooks->pfnPreventVolatile != nullptr
		&& pxHooks->pfnPreventVolatile(eVolatile))
	{
		xCtx.Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_ABILITY_TRIGGER, eTgt,
			xSide.m_uActiveSlot, ZM_MOVE_NONE, ZM_SPECIES_NONE, (int)eVolatile, 0,
			(int)xMon.m_eAbility));
		return false;
	}

	u_int uDuration = 0u;
	switch (eVolatile)
	{
	case ZM_VOLATILE_CONFUSED:
		uDuration = xCtx.RNG().RandRange(1u, 4u);
		xMon.m_uConfuseTurns = uDuration;
		break;
	case ZM_VOLATILE_TRAP:
		uDuration = xCtx.RNG().RandRange(4u, 5u);
		xMon.m_uTrapTurns = uDuration;
		break;
	case ZM_VOLATILE_TAUNT:
		uDuration = 3u;
		xMon.m_uTauntTurns = uDuration;
		break;
	case ZM_VOLATILE_LOCK:
		uDuration = xCtx.RNG().RandRange(2u, 3u);
		xMon.m_uLockTurns = uDuration - 1u;
		break;
	case ZM_VOLATILE_LEECH_SEED:
		xMon.m_eLeechSourceSide = eSource;
		break;
	default:
		break;
	}

	xMon.m_uVolatileMask |= (u_int)eVolatile;
	const int iSourceTag = eVolatile == ZM_VOLATILE_LEECH_SEED
		? (int)eSource + 1 : 0;
	xCtx.Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_VOLATILE_APPLIED, eTgt,
		xSide.m_uActiveSlot, ZM_MOVE_NONE, ZM_SPECIES_NONE, (int)uDuration,
		(int)eVolatile, iSourceTag));
	return true;
}

bool ZM_StatusLogic::EndVolatile(ZM_BattleState& xState,
	Zenith_Vector<ZM_BattleEvent>& xEvents, ZM_SIDE eSide, ZM_VOLATILE eVolatile)
{
	ZM_BattleSide& xSide = xState.Side(eSide);
	ZM_BattleMonster& xMon = xSide.Active();
	if (!g_HasVolatile(xMon, eVolatile))
	{
		return false;
	}

	xMon.m_uVolatileMask &= ~(u_int)eVolatile;
	switch (eVolatile)
	{
	case ZM_VOLATILE_CONFUSED:   xMon.m_uConfuseTurns = 0u; break;
	case ZM_VOLATILE_TRAP:       xMon.m_uTrapTurns = 0u; break;
	case ZM_VOLATILE_TAUNT:      xMon.m_uTauntTurns = 0u; break;
	case ZM_VOLATILE_LOCK:
		xMon.m_uLockTurns = 0u;
		xMon.m_uLockMoveSlot = uZM_MAX_MOVES;
		break;
	case ZM_VOLATILE_CHARGE:
		xMon.m_uChargeMoveSlot = uZM_MAX_MOVES;
		break;
	case ZM_VOLATILE_LEECH_SEED:
		xMon.m_eLeechSourceSide = ZM_SIDE_COUNT;
		break;
	default:
		break;
	}

	xEvents.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_VOLATILE_ENDED, eSide,
		xSide.m_uActiveSlot, ZM_MOVE_NONE, ZM_SPECIES_NONE, 0,
		(int)eVolatile));
	return true;
}

void ZM_StatusLogic::ResetSwitchTransients(ZM_BattleMonster& xMon)
{
	xMon.m_uVolatileMask = ZM_VOLATILE_NONE;
	xMon.m_uConfuseTurns = 0u;
	xMon.m_uTrapTurns = 0u;
	xMon.m_uTauntTurns = 0u;
	xMon.m_uLockTurns = 0u;
	xMon.m_uChargeMoveSlot = uZM_MAX_MOVES;
	xMon.m_uLockMoveSlot = uZM_MAX_MOVES;
	xMon.m_eLeechSourceSide = ZM_SIDE_COUNT;
	xMon.m_bEndureThisTurn = false;
	for (u_int u = 0u; u < ZM_BATTLE_STAT_COUNT; ++u)
	{
		xMon.m_aiStage[u] = 0;
	}
	xMon.m_iCritStage = 0;
}

static void g_CancelPendingCharge(ZM_MoveContext& xCtx)
{
	if (!g_HasVolatile(xCtx.Atk(), ZM_VOLATILE_CHARGE))
	{
		return;
	}
	ZM_StatusLogic::EndVolatile(*xCtx.m_pxState, *xCtx.m_pxEvents,
		xCtx.m_eAtk, ZM_VOLATILE_SEMI_INVULN);
	ZM_StatusLogic::EndVolatile(*xCtx.m_pxState, *xCtx.m_pxEvents,
		xCtx.m_eAtk, ZM_VOLATILE_CHARGE);
}

ZM_GATE_RESULT ZM_StatusLogic::PreMoveGate(ZM_MoveContext& xCtx)
{
	ZM_BattleSide&    xSide = xCtx.AtkSide();
	ZM_BattleMonster& xMon  = xCtx.Atk();

	// G1 RECHARGE -- consume the one-turn debt. [0 draws]
	if (g_HasVolatile(xMon, ZM_VOLATILE_RECHARGE))
	{
		xCtx.Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_FAILED, xCtx.m_eAtk,
			xSide.m_uActiveSlot, ZM_MOVE_NONE, ZM_SPECIES_NONE, 0,
			(int)ZM_MOVE_FAIL_RECHARGE));
		EndVolatile(*xCtx.m_pxState, *xCtx.m_pxEvents, xCtx.m_eAtk,
			ZM_VOLATILE_RECHARGE);
		g_CancelPendingCharge(xCtx);
		return ZM_GATE_CANCELLED;
	}

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
			g_CancelPendingCharge(xCtx);
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
			g_CancelPendingCharge(xCtx);
			return ZM_GATE_CANCELLED;
		}
	}

	// G4 FLINCH -- dedicated event is the sole cancellation reason. [0 draws]
	if (g_HasVolatile(xMon, ZM_VOLATILE_FLINCH))
	{
		xCtx.Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_FLINCH, xCtx.m_eAtk,
			xSide.m_uActiveSlot));
		EndVolatile(*xCtx.m_pxState, *xCtx.m_pxEvents, xCtx.m_eAtk,
			ZM_VOLATILE_FLINCH);
		g_CancelPendingCharge(xCtx);
		return ZM_GATE_CANCELLED;
	}

	// G5 CONFUSE -- one gate draw per attempted action. The duration counts G5
	// attempts, not EOTs. A self-hit is typeless 40-power physical damage with
	// one damage-roll draw and no accuracy/crit/type/STAB/weather/screen effects.
	if (g_HasVolatile(xMon, ZM_VOLATILE_CONFUSED))
	{
		Zenith_Assert(xMon.m_uConfuseTurns > 0u,
			"PreMoveGate: CONFUSED has zero remaining turns");
		const bool bSelfHit = xCtx.RNG().RandBelow(100u) < 33u;
		if (bSelfHit)
		{
			xCtx.Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_FAILED, xCtx.m_eAtk,
				xSide.m_uActiveSlot, ZM_MOVE_NONE, ZM_SPECIES_NONE, 0,
				(int)ZM_MOVE_FAIL_CONFUSED));

			ZM_DamageInput xIn;
			xIn.uLevel = xMon.m_uLevel;
			xIn.uPower = 40u;
			xIn.uAttack = ZM_ApplyStatStage(xMon.m_auMaxStat[ZM_STAT_ATTACK],
				xMon.m_aiStage[ZM_BATTLE_STAT_ATTACK]);
			xIn.uDefense = ZM_ApplyStatStage(xMon.m_auMaxStat[ZM_STAT_DEFENSE],
				xMon.m_aiStage[ZM_BATTLE_STAT_DEFENSE]);
			xIn.bStab = false;
			xIn.uEffectivenessPercent = 100u;
			xIn.bCrit = false;
			xIn.uRandomPercent = xCtx.RNG().RandRange(85u, 100u);
			xIn.bBurnedPhysical = xMon.m_eStatus == ZM_MAJOR_STATUS_BURN;
			const u_int uDmg = ZM_CalcDamage(xIn);
			xMon.m_uCurHP = uDmg >= xMon.m_uCurHP ? 0u : xMon.m_uCurHP - uDmg;
			xCtx.Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_DAMAGE_DEALT, xCtx.m_eAtk,
				xSide.m_uActiveSlot, ZM_MOVE_NONE, ZM_SPECIES_NONE, (int)uDmg,
				(int)xMon.m_uCurHP, ZM_VolatileDamageTag(ZM_VOLATILE_CONFUSED)));
			xCtx.MaybeFaint(xCtx.m_eAtk);
		}

		// Duration counts attempted G5 actions. Resolve the pass/self-hit first,
		// then consume this attempt and announce expiry before any later gate.
		--xMon.m_uConfuseTurns;
		if (xMon.m_uConfuseTurns == 0u)
		{
			EndVolatile(*xCtx.m_pxState, *xCtx.m_pxEvents, xCtx.m_eAtk,
				ZM_VOLATILE_CONFUSED);
		}
		if (bSelfHit)
		{
			g_CancelPendingCharge(xCtx);
			return ZM_GATE_CANCELLED;
		}
	}

	// G6 PARALYSIS -- 25% full-para. [1 draw iff paralyzed]
	if (xMon.m_eStatus == ZM_MAJOR_STATUS_PARALYSIS)
	{
		if (xCtx.RNG().RandBelow(100u) < 25u)
		{
			xCtx.Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_FAILED, xCtx.m_eAtk, xSide.m_uActiveSlot,
				ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, (int)ZM_MOVE_FAIL_FULLY_PARALYZED));
			g_CancelPendingCharge(xCtx);
			return ZM_GATE_CANCELLED;
		}
	}

	return ZM_GATE_PROCEED;
}

void ZM_StatusLogic::EndOfTurn(ZM_BattleState& xState, Zenith_Vector<ZM_BattleEvent>& xEvents, ZM_SIDE eSide)
{
	ZM_BattleSide&    xSide = xState.Side(eSide);
	ZM_BattleMonster& xMon  = xSide.Active();

	// 1. Major chip (SC4 behavior, now without an early return so later volatile
	// stages and one-turn cleanup can run).
	// SC5 (ZM-D-036): a TOXICTHRIVE holder's POISON/TOXIC chip is SKIPPED here -- its
	// net recovery comes from the EoT-ability heal (step 6). This is ABILITY-GATED, so
	// every non-TOXICTHRIVE mon (incl. NONE) is byte-identical; BURN is untouched (a mon
	// holds one major at a time). The skipped chip body also holds the TOXIC ramp
	// (++m_uStatusCounter), so a thriving holder's toxic counter does NOT advance --
	// consistent (it takes no toxic damage) and intended (ZM-D-036 R5).
	const ZM_MAJOR_STATUS eStatus = xMon.m_eStatus;
	const bool bToxicthriveSkip =
		(eStatus == ZM_MAJOR_STATUS_POISON || eStatus == ZM_MAJOR_STATUS_TOXIC)
		&& xMon.m_eAbility == ZM_ABILITY_TOXICTHRIVE;
	if (!xMon.IsFainted() && !bToxicthriveSkip && (eStatus == ZM_MAJOR_STATUS_POISON
		|| eStatus == ZM_MAJOR_STATUS_TOXIC || eStatus == ZM_MAJOR_STATUS_BURN))
	{
		const u_int uMaxHP = xMon.m_auMaxStat[ZM_STAT_HP];
		u_int uDmg = eStatus == ZM_MAJOR_STATUS_TOXIC
			? (uMaxHP * xMon.m_uStatusCounter) / 16u : uMaxHP / 8u;
		if (uDmg == 0u) { uDmg = 1u; }
		xMon.m_uCurHP = uDmg >= xMon.m_uCurHP ? 0u : xMon.m_uCurHP - uDmg;
		xEvents.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_STATUS_DAMAGE, eSide,
			xSide.m_uActiveSlot, ZM_MOVE_NONE, ZM_SPECIES_NONE, (int)uDmg,
			(int)xMon.m_uCurHP, (int)eStatus));
		if (eStatus == ZM_MAJOR_STATUS_TOXIC) { ++xMon.m_uStatusCounter; }
		if (xMon.IsFainted())
		{
			xEvents.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_FAINT, eSide,
				xSide.m_uActiveSlot));
		}
	}

	// 2. Leech Seed: scheduled raw chip on the victim, then heal the source
	// side's current living active by the ACTUAL amount restored (including 0).
	if (!xMon.IsFainted() && g_HasVolatile(xMon, ZM_VOLATILE_LEECH_SEED))
	{
		u_int uDmg = xMon.m_auMaxStat[ZM_STAT_HP] / 8u;
		if (uDmg == 0u) { uDmg = 1u; }
		const u_int uOldHP = xMon.m_uCurHP;
		const u_int uLost = uDmg >= uOldHP ? uOldHP : uDmg;
		xMon.m_uCurHP = uDmg >= uOldHP ? 0u : uOldHP - uDmg;
		xEvents.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_STATUS_DAMAGE, eSide,
			xSide.m_uActiveSlot, ZM_MOVE_NONE, ZM_SPECIES_NONE, (int)uDmg,
			(int)xMon.m_uCurHP, ZM_VolatileDamageTag(ZM_VOLATILE_LEECH_SEED)));
		if (xMon.IsFainted())
		{
			xEvents.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_FAINT, eSide,
				xSide.m_uActiveSlot));
		}

		const ZM_SIDE eSource = xMon.m_eLeechSourceSide;
		if (eSource < ZM_SIDE_COUNT)
		{
			ZM_BattleSide& xSourceSide = xState.Side(eSource);
			ZM_BattleMonster& xSource = xSourceSide.Active();
			if (!xSource.IsFainted())
			{
				const u_int uMissing = xSource.m_auMaxStat[ZM_STAT_HP] - xSource.m_uCurHP;
				const u_int uRestored = uLost < uMissing ? uLost : uMissing;
				xSource.m_uCurHP += uRestored;
				xEvents.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_DRAIN, eSource,
					xSourceSide.m_uActiveSlot, ZM_MOVE_NONE, ZM_SPECIES_NONE,
					(int)uRestored, (int)xSource.m_uCurHP));
			}
		}
	}

	// 3. Trap chips on every EOT including the application turn. A trap KO does
	// not emit a later expiry; surviving ticks decrement and end at zero.
	if (!xMon.IsFainted() && g_HasVolatile(xMon, ZM_VOLATILE_TRAP))
	{
		u_int uDmg = xMon.m_auMaxStat[ZM_STAT_HP] / 8u;
		if (uDmg == 0u) { uDmg = 1u; }
		xMon.m_uCurHP = uDmg >= xMon.m_uCurHP ? 0u : xMon.m_uCurHP - uDmg;
		xEvents.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_STATUS_DAMAGE, eSide,
			xSide.m_uActiveSlot, ZM_MOVE_NONE, ZM_SPECIES_NONE, (int)uDmg,
			(int)xMon.m_uCurHP, ZM_VolatileDamageTag(ZM_VOLATILE_TRAP)));
		if (xMon.IsFainted())
		{
			xEvents.PushBack(ZM_MakeEvent(ZM_BATTLE_EVENT_FAINT, eSide,
				xSide.m_uActiveSlot));
		}
		else
		{
			Zenith_Assert(xMon.m_uTrapTurns > 0u,
				"EndOfTurn: TRAP has zero remaining turns");
			--xMon.m_uTrapTurns;
			if (xMon.m_uTrapTurns == 0u)
			{
				EndVolatile(xState, xEvents, eSide, ZM_VOLATILE_TRAP);
			}
		}
	}

	// 4. Deterministic cleanup. These run even after a KO so one-turn state
	// cannot leak onto a fainted outgoing active.
	EndVolatile(xState, xEvents, eSide, ZM_VOLATILE_FLINCH);
	EndVolatile(xState, xEvents, eSide, ZM_VOLATILE_PROTECT);
	xMon.m_bEndureThisTurn = false;
	if (g_HasVolatile(xMon, ZM_VOLATILE_TAUNT))
	{
		Zenith_Assert(xMon.m_uTauntTurns > 0u,
			"EndOfTurn: TAUNT has zero remaining turns");
		--xMon.m_uTauntTurns;
		if (xMon.m_uTauntTurns == 0u)
		{
			EndVolatile(xState, xEvents, eSide, ZM_VOLATILE_TAUNT);
		}
	}
}
