#include "Zenith.h"

#include "Zenithmon/Source/Battle/ZM_MoveExecutor.h"
#include "Zenithmon/Source/Battle/ZM_DamageCalc.h"
#include "Zenithmon/Source/Data/ZM_MoveData.h"
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"

// ============================================================================
// ZM_MoveExecutor -- SC1 seam + SC2 stat-stage effects & category dispatch.
// Draw discipline (locked): per move -- accuracy (only if the move can miss) ->
// [STATUS-category primary: apply effect, no further draws] OR [damaging: immunity
// short-circuit -> crit -> damage-roll -> E3 secondary proc]. Emit order per
// connecting damaging hit: MOVE_USED -> { MISS | IMMUNE | ([CRIT] [SUPER|NOT]
// DAMAGE_DEALT [FAINT]) [STAT_STAGE_CHANGED] }. The acc/eva stat-stage fold reduces
// to the base-accuracy check at stage 0 (box 1 is always stage 0), and the E3
// secondary proc draws NOTHING for a NONE-effect move, so box-1 goldens never move.
//
// SC2 dispatch: Move().m_eCategory picks PRIMARY (STATUS-category, chance 100, no
// crit/roll/immunity/proc draws) vs SECONDARY (damaging move: ApplyDamagingHit
// first, then a RandBelow(100)<chance proc gate applies the stat change if the
// target survived). Both funnel every stat change through g_ApplyStatChange (the
// single clamp+emit site: STAT_STAGE_CHANGED, or MOVE_FAILED(STAT_MAXED) for a
// single-stat primary already at the +/-6 cap; a maxed secondary is silent; a
// multi-stat raise caps each stat silently and fails as a whole only if NOTHING
// moved). RAISE_CRIT adds the row magnitude to the attacker's m_iCritStage counter
// (cap 3) and emits nothing -- it has no ZM_BATTLE_STAT slot.
// ============================================================================

// ---- ZM_MoveContext accessors -- live resolution against the engine-owned state
ZM_BattleMonster& ZM_MoveContext::Atk()     { return m_pxState->Side(m_eAtk).Active(); }
ZM_BattleMonster& ZM_MoveContext::Def()     { return m_pxState->Side(m_eDef).Active(); }
ZM_BattleSide&    ZM_MoveContext::AtkSide() { return m_pxState->Side(m_eAtk); }
ZM_BattleSide&    ZM_MoveContext::DefSide() { return m_pxState->Side(m_eDef); }
ZM_BattleRNG&     ZM_MoveContext::RNG()     { return m_pxState->m_xRNG; }

const ZM_MoveData& ZM_MoveContext::Move() const
{
	return ZM_GetMoveData(m_pxState->Side(m_eAtk).Active().m_axMoves[m_uMoveSlot].m_eMove);
}

void ZM_MoveContext::Emit(const ZM_BattleEvent& xEvent)
{
	m_pxEvents->PushBack(xEvent);
}

// Emit FAINT once, exactly when the given side's active monster has hit 0 HP. The
// caller has already applied damage; this mirrors the engine's post-hit faint emit
// (side, active slot). Battle-over detection stays in ResolveTurn.
void ZM_MoveContext::MaybeFaint(ZM_SIDE eSide)
{
	ZM_BattleSide& xSide = m_pxState->Side(eSide);
	if (xSide.Active().m_uCurHP == 0u)
	{
		Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_FAINT, eSide, xSide.m_uActiveSlot));
	}
}

// ---- SC2 stat-stage helpers (file-local; the executor's grouped effect body) ---

// True iff eEffect is one of the SC2 stat-stage kinds (LOWER_*/RAISE_*/RAISE_CRIT/
// the multi-stat RAISE_* combos). Non-stat kinds are routed by other sub-commits.
static bool g_IsStatEffect(ZM_MOVE_EFFECT eEffect)
{
	switch (eEffect)
	{
	case ZM_MOVE_EFFECT_LOWER_ATTACK:
	case ZM_MOVE_EFFECT_LOWER_DEFENSE:
	case ZM_MOVE_EFFECT_LOWER_SPATTACK:
	case ZM_MOVE_EFFECT_LOWER_SPDEFENSE:
	case ZM_MOVE_EFFECT_LOWER_SPEED:
	case ZM_MOVE_EFFECT_LOWER_ACCURACY:
	case ZM_MOVE_EFFECT_LOWER_EVASION:
	case ZM_MOVE_EFFECT_RAISE_ATTACK:
	case ZM_MOVE_EFFECT_RAISE_DEFENSE:
	case ZM_MOVE_EFFECT_RAISE_SPATTACK:
	case ZM_MOVE_EFFECT_RAISE_SPDEFENSE:
	case ZM_MOVE_EFFECT_RAISE_SPEED:
	case ZM_MOVE_EFFECT_RAISE_EVASION:
	case ZM_MOVE_EFFECT_RAISE_CRIT:
	case ZM_MOVE_EFFECT_RAISE_ATTACK_SPEED:
	case ZM_MOVE_EFFECT_RAISE_ATTACK_DEFENSE:
	case ZM_MOVE_EFFECT_RAISE_SPATK_SPDEF:
	case ZM_MOVE_EFFECT_RAISE_DEF_SPDEF:
	case ZM_MOVE_EFFECT_RAISE_ALL:
		return true;
	default:
		return false;
	}
}

// Apply ONE stat-stage delta to a target and emit. Clamp to [iZM_MIN_STAGE,
// iZM_MAX_STAGE]. Returns whether the stage actually moved (so a multi-stat caller
// can count changes and decide a single whole-move failure). If the stage is already
// at the cap in the requested direction (new == old): a single-stat STATUS-category
// PRIMARY emits MOVE_FAILED(STAT_MAXED) on the TARGET (the mon whose stage is capped);
// a damaging SECONDARY -- and any per-stat cap of a MULTI-stat effect (passed
// bPrimary=false) -- is silent. Otherwise set the stage and emit STAT_STAGE_CHANGED
// with m_iAmount = the ACTUAL signed delta applied (new-old, may be smaller than
// iDelta near the cap) and m_iAux = the stat.
static bool g_ApplyStatChange(ZM_MoveContext& xCtx, ZM_SIDE eTgt, ZM_BATTLE_STAT eStat,
	int iDelta, bool bPrimary)
{
	ZM_BattleSide&    xTgtSide = xCtx.m_pxState->Side(eTgt);
	ZM_BattleMonster& xTgt     = xTgtSide.Active();

	const int iOld = xTgt.m_aiStage[eStat];
	int iNew = iOld + iDelta;
	if (iNew > iZM_MAX_STAGE) { iNew = iZM_MAX_STAGE; }
	if (iNew < iZM_MIN_STAGE) { iNew = iZM_MIN_STAGE; }

	if (iNew == iOld)
	{
		// Already capped in the requested direction: a single-stat primary announces
		// the failure; a secondary proc / multi-stat per-stat cap is silent.
		if (bPrimary)
		{
			xCtx.Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_FAILED, eTgt, xTgtSide.m_uActiveSlot,
				ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, (int)ZM_MOVE_FAIL_STAT_MAXED));
		}
		return false;
	}

	xTgt.m_aiStage[eStat] = iNew;
	xCtx.Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_STAT_STAGE_CHANGED, eTgt, xTgtSide.m_uActiveSlot,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, iNew - iOld, (int)eStat));
	return true;
}

// Apply a MULTI-stat self-RAISE (RAISE_ATTACK_SPEED / RAISE_ALL / ...): raise each
// listed stat on the attacker by +iMagnitude. A per-stat cap is SILENT here -- each
// g_ApplyStatChange is invoked as a NON-primary so it emits no per-stat MOVE_FAILED;
// instead a SINGLE whole-move MOVE_FAILED(STAT_MAXED) fires only for a PRIMARY whose
// every target stat was already capped (nothing changed). Each stat that DID move
// still emits its own STAT_STAGE_CHANGED, in list order (so a partial cap is a clean
// "the rest changed" stream, never a contradictory failed-AND-changed one).
static void g_ApplyMultiStatRaise(ZM_MoveContext& xCtx, const ZM_BATTLE_STAT* peStats,
	u_int uCount, int iMagnitude, bool bPrimary)
{
	const ZM_SIDE eAtk = xCtx.m_eAtk;
	u_int uChanged = 0u;
	for (u_int i = 0u; i < uCount; ++i)
	{
		if (g_ApplyStatChange(xCtx, eAtk, peStats[i], +iMagnitude, /*bPrimary*/ false))
		{
			++uChanged;
		}
	}
	if (bPrimary && uChanged == 0u)
	{
		// Every target stat was already capped -> the move as a whole did nothing.
		ZM_BattleSide& xSelfSide = xCtx.m_pxState->Side(eAtk);
		xCtx.Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_FAILED, eAtk, xSelfSide.m_uActiveSlot,
			ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, (int)ZM_MOVE_FAIL_STAT_MAXED));
	}
}

// Route a stat-effect kind to its stat(s)/direction/target and apply each via
// g_ApplyStatChange. iMagnitude is the stage count (1..3); LOWER applies -mag to the
// OPPONENT, RAISE applies +mag to SELF (per the kind, matching the row's m_eTarget).
// RAISE_CRIT has no ZM_BATTLE_STAT slot: it adds iMagnitude to the attacker's
// m_iCritStage counter (cap 3) and emits NO event (documented ZM-D-033 SC2 choice).
static void g_ApplyStatEffect(ZM_MoveContext& xCtx, ZM_MOVE_EFFECT eEffect, int iMagnitude, bool bPrimary)
{
	const ZM_SIDE eAtk = xCtx.m_eAtk;
	const ZM_SIDE eDef = xCtx.m_eDef;

	switch (eEffect)
	{
	// ---- LOWER (opponent) ----
	case ZM_MOVE_EFFECT_LOWER_ATTACK:    g_ApplyStatChange(xCtx, eDef, ZM_BATTLE_STAT_ATTACK,    -iMagnitude, bPrimary); break;
	case ZM_MOVE_EFFECT_LOWER_DEFENSE:   g_ApplyStatChange(xCtx, eDef, ZM_BATTLE_STAT_DEFENSE,   -iMagnitude, bPrimary); break;
	case ZM_MOVE_EFFECT_LOWER_SPATTACK:  g_ApplyStatChange(xCtx, eDef, ZM_BATTLE_STAT_SPATTACK,  -iMagnitude, bPrimary); break;
	case ZM_MOVE_EFFECT_LOWER_SPDEFENSE: g_ApplyStatChange(xCtx, eDef, ZM_BATTLE_STAT_SPDEFENSE, -iMagnitude, bPrimary); break;
	case ZM_MOVE_EFFECT_LOWER_SPEED:     g_ApplyStatChange(xCtx, eDef, ZM_BATTLE_STAT_SPEED,     -iMagnitude, bPrimary); break;
	case ZM_MOVE_EFFECT_LOWER_ACCURACY:  g_ApplyStatChange(xCtx, eDef, ZM_BATTLE_STAT_ACCURACY,  -iMagnitude, bPrimary); break;
	case ZM_MOVE_EFFECT_LOWER_EVASION:   g_ApplyStatChange(xCtx, eDef, ZM_BATTLE_STAT_EVASION,   -iMagnitude, bPrimary); break;

	// ---- RAISE (self) ----
	case ZM_MOVE_EFFECT_RAISE_ATTACK:    g_ApplyStatChange(xCtx, eAtk, ZM_BATTLE_STAT_ATTACK,    +iMagnitude, bPrimary); break;
	case ZM_MOVE_EFFECT_RAISE_DEFENSE:   g_ApplyStatChange(xCtx, eAtk, ZM_BATTLE_STAT_DEFENSE,   +iMagnitude, bPrimary); break;
	case ZM_MOVE_EFFECT_RAISE_SPATTACK:  g_ApplyStatChange(xCtx, eAtk, ZM_BATTLE_STAT_SPATTACK,  +iMagnitude, bPrimary); break;
	case ZM_MOVE_EFFECT_RAISE_SPDEFENSE: g_ApplyStatChange(xCtx, eAtk, ZM_BATTLE_STAT_SPDEFENSE, +iMagnitude, bPrimary); break;
	case ZM_MOVE_EFFECT_RAISE_SPEED:     g_ApplyStatChange(xCtx, eAtk, ZM_BATTLE_STAT_SPEED,     +iMagnitude, bPrimary); break;
	case ZM_MOVE_EFFECT_RAISE_EVASION:   g_ApplyStatChange(xCtx, eAtk, ZM_BATTLE_STAT_EVASION,   +iMagnitude, bPrimary); break;

	// ---- multi-stat RAISE (self, each named stat) -- silent per-stat cap, one
	//      whole-move MOVE_FAILED only if EVERY listed stat was already capped ----
	case ZM_MOVE_EFFECT_RAISE_ATTACK_SPEED:
	{
		const ZM_BATTLE_STAT aeStats[] = { ZM_BATTLE_STAT_ATTACK, ZM_BATTLE_STAT_SPEED };
		g_ApplyMultiStatRaise(xCtx, aeStats, 2u, iMagnitude, bPrimary);
		break;
	}
	case ZM_MOVE_EFFECT_RAISE_ATTACK_DEFENSE:
	{
		const ZM_BATTLE_STAT aeStats[] = { ZM_BATTLE_STAT_ATTACK, ZM_BATTLE_STAT_DEFENSE };
		g_ApplyMultiStatRaise(xCtx, aeStats, 2u, iMagnitude, bPrimary);
		break;
	}
	case ZM_MOVE_EFFECT_RAISE_SPATK_SPDEF:
	{
		const ZM_BATTLE_STAT aeStats[] = { ZM_BATTLE_STAT_SPATTACK, ZM_BATTLE_STAT_SPDEFENSE };
		g_ApplyMultiStatRaise(xCtx, aeStats, 2u, iMagnitude, bPrimary);
		break;
	}
	case ZM_MOVE_EFFECT_RAISE_DEF_SPDEF:
	{
		const ZM_BATTLE_STAT aeStats[] = { ZM_BATTLE_STAT_DEFENSE, ZM_BATTLE_STAT_SPDEFENSE };
		g_ApplyMultiStatRaise(xCtx, aeStats, 2u, iMagnitude, bPrimary);
		break;
	}
	case ZM_MOVE_EFFECT_RAISE_ALL:   // all five battle stats (not accuracy/evasion)
	{
		const ZM_BATTLE_STAT aeStats[] = {
			ZM_BATTLE_STAT_ATTACK, ZM_BATTLE_STAT_DEFENSE, ZM_BATTLE_STAT_SPATTACK,
			ZM_BATTLE_STAT_SPDEFENSE, ZM_BATTLE_STAT_SPEED };
		g_ApplyMultiStatRaise(xCtx, aeStats, 5u, iMagnitude, bPrimary);
		break;
	}

	// ---- RAISE_CRIT (self, counter; no stat slot, no event) ----
	case ZM_MOVE_EFFECT_RAISE_CRIT:
	{
		// Add the row magnitude (Killer Focus = +2), still capped at 3. No event/draw.
		ZM_BattleMonster& xSelf = xCtx.m_pxState->Side(eAtk).Active();
		xSelf.m_iCritStage += iMagnitude;
		if (xSelf.m_iCritStage > 3) { xSelf.m_iCritStage = 3; }
		break;
	}

	default:
		break;   // not a stat kind (routed by another sub-commit)
	}
}

// STATUS-category PRIMARY: the effect IS the point of the move (chance 100). SC2
// lights only the stat-stage kinds here; other STATUS-category effects (status /
// volatile / heal / field) light up in SC3+ and are a MOVE_USED-only no-op for now.
static void g_ApplyStatusCategoryPrimary(ZM_MoveContext& xCtx)
{
	const ZM_MoveData& xMove = xCtx.Move();
	if (g_IsStatEffect(xMove.m_eEffect))
	{
		g_ApplyStatEffect(xCtx, xMove.m_eEffect, xMove.m_iEffectMagnitude, /*bPrimary*/ true);
	}
}

// E3 SECONDARY proc for a DAMAGING move that carries a stat effect. Draws NOTHING for
// NONE or a not-yet-lit (status/volatile) secondary, so box-1 NONE moves stay
// byte-identical. Requires the damaged target (defender) to have survived the hit.
// Proc gate: m_uEffectChance >= 100 is unconditional (no draw); else one RandBelow(100).
static void g_ApplyDamagingSecondary(ZM_MoveContext& xCtx)
{
	const ZM_MoveData& xMove = xCtx.Move();
	if (!g_IsStatEffect(xMove.m_eEffect))
	{
		return;   // NONE / non-stat secondary: no proc draw (byte-identical box 1)
	}
	if (xCtx.Def().m_uCurHP == 0u)
	{
		return;   // a fainted target takes no secondary (E3 requires target alive)
	}
	if (xMove.m_uEffectChance < 100u)
	{
		if (xCtx.RNG().RandBelow(100u) >= xMove.m_uEffectChance)
		{
			return;   // proc failed
		}
	}
	g_ApplyStatEffect(xCtx, xMove.m_eEffect, xMove.m_iEffectMagnitude, /*bPrimary*/ false);
}

// ---- The executor ----------------------------------------------------------
void ZM_MoveExecutor::Execute(ZM_MoveContext& xCtx)
{
	ZM_BattleMonster& xAtk     = xCtx.Atk();
	ZM_BattleSide&    xAtkSide = xCtx.AtkSide();
	const ZM_MoveData& xMove   = xCtx.Move();

	ZM_MoveSlot& xMoveSlot = xAtk.m_axMoves[xCtx.m_uMoveSlot];

	// M1: spend PP + announce (box 1 always has PP; NO_PP is a later box).
	--xMoveSlot.m_uCurPP;
	xCtx.Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_USED, xCtx.m_eAtk, xAtkSide.m_uActiveSlot, (u_int)xMove.m_eId));

	// M5: ACCURACY draw -- only if the move CAN miss (sure-hit short-circuit). Applies
	// to STATUS-category and damaging moves alike. effAcc folds the acc/eva stat stages
	// via the SAME ZM_ApplyStatStage idiom; at stage 0 (box 1) the net stage is 0 ->
	// ZM_ApplyStatStage(acc,0) == acc, so this is exactly the base-accuracy check and
	// the box-1 goldens are unchanged.
	if (xMove.m_uAccuracy != uZM_MOVE_ACCURACY_ALWAYS_HITS && xMove.m_uAccuracy < 100u)
	{
		// Fold acc/eva stages, but CLAMP the net stage delta to [iZM_MIN_STAGE,
		// iZM_MAX_STAGE] first: the raw difference ranges [-12,+12], yet a stat stage
		// only ever multiplies within its +/-6 envelope. At stage 0 (box 1) and for
		// |delta| <= 6 (every existing test) the clamp is identity, so no golden moves.
		int iAccStage = xAtk.m_aiStage[ZM_BATTLE_STAT_ACCURACY] - xCtx.Def().m_aiStage[ZM_BATTLE_STAT_EVASION];
		if (iAccStage > iZM_MAX_STAGE) { iAccStage = iZM_MAX_STAGE; }
		if (iAccStage < iZM_MIN_STAGE) { iAccStage = iZM_MIN_STAGE; }
		const u_int uEffAcc = ZM_ApplyStatStage(xMove.m_uAccuracy, iAccStage);
		if (xCtx.RNG().RandBelow(100u) >= uEffAcc)
		{
			xCtx.Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_MISSED, xCtx.m_eAtk, xAtkSide.m_uActiveSlot, (u_int)xMove.m_eId));
			return;
		}
	}

	// STATUS-category dispatch: the effect is the PRIMARY -- no immunity/crit/roll/proc
	// draws. Apply it directly (chance is effectively 100) and return.
	if (xMove.m_eCategory == ZM_MOVE_CATEGORY_STATUS)
	{
		g_ApplyStatusCategoryPrimary(xCtx);
		return;
	}

	// Damaging move (PHYSICAL/SPECIAL). M6: effectiveness is deterministic (no RNG) --
	// resolve immunity BEFORE any crit draw.
	ZM_BattleMonster&     xDef        = xCtx.Def();
	ZM_BattleSide&        xDefSide    = xCtx.DefSide();
	const ZM_SpeciesData& xDefSpecies = ZM_GetSpeciesData(xDef.m_eSpecies);
	const u_int uEffPct = ZM_EffectivenessPercent(xMove.m_eType, xDefSpecies.m_aeTypes[0], xDefSpecies.m_aeTypes[1]);
	if (uEffPct == 0u)
	{
		xCtx.Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_IMMUNE, xCtx.m_eDef, xDefSide.m_uActiveSlot));
		return;
	}

	// E1/E2: the single damaging emit-group (crit -> roll -> DAMAGE_DEALT -> FAINT).
	ApplyDamagingHit(xCtx);

	// E3: SECONDARY stat proc -- only for a damaging move carrying a stat effect, and
	// only if the target survived. NONE-effect moves take no draw here (box-1 path).
	g_ApplyDamagingSecondary(xCtx);
}

u_int ZM_MoveExecutor::ApplyDamagingHit(ZM_MoveContext& xCtx)
{
	ZM_BattleMonster&  xAtk     = xCtx.Atk();
	ZM_BattleMonster&  xDef     = xCtx.Def();
	ZM_BattleSide&     xDefSide = xCtx.DefSide();
	const ZM_MoveData& xMove    = xCtx.Move();

	const ZM_SpeciesData& xDefSpecies = ZM_GetSpeciesData(xDef.m_eSpecies);
	const u_int uEffPct = ZM_EffectivenessPercent(xMove.m_eType, xDefSpecies.m_aeTypes[0], xDefSpecies.m_aeTypes[1]);

	// (2) CRIT draw. A move with m_uCritStage >= 2 is a guaranteed crit (no draw), as
	// box 1. Otherwise the attacker's accumulated RAISE_CRIT counter sets the rate:
	// 0 -> 1/24, 1 -> 1/8, 2 -> 1/2, >= 3 -> always. At crit stage 0 this is exactly
	// Chance(1,24) -- one RandBelow(24) -- so box-1 goldens (crit stage always 0) are
	// byte-identical. (SC2: the move's own high-crit m_uCritStage==1 is NOT combined
	// with the counter here -- box-1 treated stage 1 as 1/24 too; only >= 2 short-
	// circuits to guaranteed.)
	bool bCrit;
	if (xMove.m_uCritStage >= 2u || xAtk.m_iCritStage >= 3)
	{
		bCrit = true;
	}
	else
	{
		u_int uCritDen = 24u;             // crit stage 0
		if (xAtk.m_iCritStage == 1)      { uCritDen = 8u; }
		else if (xAtk.m_iCritStage == 2) { uCritDen = 2u; }
		bCrit = xCtx.RNG().Chance(1u, uCritDen);
	}
	// (3) DAMAGE ROLL draw -- 85..100 inclusive.
	const u_int uRoll = xCtx.RNG().RandRange(85u, 100u);

	const bool bPhysical = (xMove.m_eCategory == ZM_MOVE_CATEGORY_PHYSICAL);
	const ZM_SpeciesData& xAtkSpecies = ZM_GetSpeciesData(xAtk.m_eSpecies);

	ZM_DamageInput xIn;
	xIn.uLevel  = xAtk.m_uLevel;
	xIn.uPower  = xMove.m_uPower;
	xIn.uAttack = ZM_ApplyStatStage(
		bPhysical ? xAtk.m_auMaxStat[ZM_STAT_ATTACK] : xAtk.m_auMaxStat[ZM_STAT_SPATTACK],
		bPhysical ? xAtk.m_aiStage[ZM_BATTLE_STAT_ATTACK] : xAtk.m_aiStage[ZM_BATTLE_STAT_SPATTACK]);
	xIn.uDefense = ZM_ApplyStatStage(
		bPhysical ? xDef.m_auMaxStat[ZM_STAT_DEFENSE] : xDef.m_auMaxStat[ZM_STAT_SPDEFENSE],
		bPhysical ? xDef.m_aiStage[ZM_BATTLE_STAT_DEFENSE] : xDef.m_aiStage[ZM_BATTLE_STAT_SPDEFENSE]);
	xIn.bStab = (xMove.m_eType == xAtkSpecies.m_aeTypes[0] || xMove.m_eType == xAtkSpecies.m_aeTypes[1]);
	xIn.uEffectivenessPercent = uEffPct;
	xIn.bCrit = bCrit;
	xIn.uRandomPercent = uRoll;
	// box-2/3 seams keep their identity defaults (weather 1/1, no burn, no screen).

	const u_int uDmg = ZM_CalcDamage(xIn);

	if (bCrit)
	{
		xCtx.Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_CRIT, xCtx.m_eDef, xDefSide.m_uActiveSlot));
	}
	if (uEffPct > 100u)
	{
		xCtx.Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_SUPER_EFFECTIVE, xCtx.m_eDef, xDefSide.m_uActiveSlot,
			ZM_MOVE_NONE, ZM_SPECIES_NONE, (int)uEffPct));
	}
	else if (uEffPct < 100u)
	{
		xCtx.Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_NOT_EFFECTIVE, xCtx.m_eDef, xDefSide.m_uActiveSlot,
			ZM_MOVE_NONE, ZM_SPECIES_NONE, (int)uEffPct));
	}

	xDef.m_uCurHP = (uDmg >= xDef.m_uCurHP) ? 0u : (xDef.m_uCurHP - uDmg);
	xCtx.Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_DAMAGE_DEALT, xCtx.m_eDef, xDefSide.m_uActiveSlot,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, (int)uDmg, (int)xDef.m_uCurHP));

	xCtx.MaybeFaint(xCtx.m_eDef);   // emits FAINT iff HP hit 0; battle-over decided in ResolveTurn

	return uDmg;
}
