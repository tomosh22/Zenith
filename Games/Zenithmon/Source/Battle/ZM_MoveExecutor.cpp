#include "Zenith.h"

#include "Zenithmon/Source/Battle/ZM_MoveExecutor.h"
#include "Zenithmon/Source/Battle/ZM_DamageCalc.h"
#include "Zenithmon/Source/Data/ZM_MoveData.h"
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"

// ============================================================================
// ZM_MoveExecutor -- SC1 byte-identical extraction of ZM_BattleEngine::ExecuteMove.
// Draw discipline (locked, unchanged): per move -- accuracy (only if the move can
// miss) -> immunity short-circuit before any crit draw -> crit -> damage-roll.
// Emit order per connecting hit: MOVE_USED -> { MISS | IMMUNE | ([CRIT] [SUPER|NOT]
// DAMAGE_DEALT [FAINT]) }. The acc/eva stat-stage fold reduces to the base-accuracy
// check at stage 0 (box 1 is always stage 0), so box-1 goldens never move.
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

// ---- The executor ----------------------------------------------------------
void ZM_MoveExecutor::Execute(ZM_MoveContext& xCtx)
{
	ZM_BattleMonster& xAtk     = xCtx.Atk();
	ZM_BattleSide&    xAtkSide = xCtx.AtkSide();
	const ZM_MoveData& xMove   = xCtx.Move();

	ZM_MoveSlot& xMoveSlot = xAtk.m_axMoves[xCtx.m_uMoveSlot];

	// Spend PP + announce (box 1 always has PP; NO_PP is a later box).
	--xMoveSlot.m_uCurPP;
	xCtx.Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_USED, xCtx.m_eAtk, xAtkSide.m_uActiveSlot, (u_int)xMove.m_eId));

	// (1) ACCURACY draw -- only if the move CAN miss (sure-hit short-circuit).
	// effAcc folds the acc/eva stat stages via the SAME ZM_ApplyStatStage idiom;
	// at stage 0 (box 1) the net stage is 0 -> ZM_ApplyStatStage(acc,0) == acc, so
	// this is exactly the base-accuracy check and the box-1 goldens are unchanged.
	if (xMove.m_uAccuracy != uZM_MOVE_ACCURACY_ALWAYS_HITS && xMove.m_uAccuracy < 100u)
	{
		const int iAccStage = xAtk.m_aiStage[ZM_BATTLE_STAT_ACCURACY] - xCtx.Def().m_aiStage[ZM_BATTLE_STAT_EVASION];
		const u_int uEffAcc = ZM_ApplyStatStage(xMove.m_uAccuracy, iAccStage);
		if (xCtx.RNG().RandBelow(100u) >= uEffAcc)
		{
			xCtx.Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_MISSED, xCtx.m_eAtk, xAtkSide.m_uActiveSlot, (u_int)xMove.m_eId));
			return;
		}
	}

	// Effectiveness is deterministic (no RNG) -- resolve immunity BEFORE a crit draw.
	ZM_BattleMonster&     xDef        = xCtx.Def();
	ZM_BattleSide&        xDefSide    = xCtx.DefSide();
	const ZM_SpeciesData& xDefSpecies = ZM_GetSpeciesData(xDef.m_eSpecies);
	const u_int uEffPct = ZM_EffectivenessPercent(xMove.m_eType, xDefSpecies.m_aeTypes[0], xDefSpecies.m_aeTypes[1]);
	if (uEffPct == 0u)
	{
		xCtx.Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_IMMUNE, xCtx.m_eDef, xDefSide.m_uActiveSlot));
		return;
	}

	// Effect dispatch: box 1 has only damaging NONE moves. NONE + default both route
	// through ApplyDamagingHit (the single damage-draw site); SC2+ adds real cases.
	switch (xMove.m_eEffect)
	{
	case ZM_MOVE_EFFECT_NONE:
	default:
		ApplyDamagingHit(xCtx);
		break;
	}
}

u_int ZM_MoveExecutor::ApplyDamagingHit(ZM_MoveContext& xCtx)
{
	ZM_BattleMonster&  xAtk     = xCtx.Atk();
	ZM_BattleMonster&  xDef     = xCtx.Def();
	ZM_BattleSide&     xDefSide = xCtx.DefSide();
	const ZM_MoveData& xMove    = xCtx.Move();

	const ZM_SpeciesData& xDefSpecies = ZM_GetSpeciesData(xDef.m_eSpecies);
	const u_int uEffPct = ZM_EffectivenessPercent(xMove.m_eType, xDefSpecies.m_aeTypes[0], xDefSpecies.m_aeTypes[1]);

	// (2) CRIT draw -- guaranteed (no draw) at critStage>=2; else 1/24.
	const bool bCrit = (xMove.m_uCritStage >= 2u) ? true : xCtx.RNG().Chance(1u, 24u);
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
