#include "Zenith.h"

#include "Zenithmon/Source/Battle/ZM_BattleEngine.h"
#include "Zenithmon/Source/Battle/ZM_DamageCalc.h"
#include "Zenithmon/Source/Data/ZM_MoveData.h"
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"

// ============================================================================
// Box-1 turn loop. Draw discipline (locked): per move, accuracy (only if the
// move can miss) -> crit -> damage-roll; single speed tie-break draw before any
// move on exact-equal effective speed. Emit order per connecting hit (locked):
// MOVE_USED -> { MISS | IMMUNE | ([CRIT] [SUPER|NOT] DAMAGE_DEALT [FAINT]) }.
// ============================================================================

void ZM_BattleEngine::Begin(const ZM_BattleConfig& xConfig,
	const ZM_BattleMonsterSpec* paxPlayer, u_int uPlayerCount,
	const ZM_BattleMonsterSpec* paxEnemy, u_int uEnemyCount,
	u_int64 ulSeed, u_int64 ulSeq)
{
	Zenith_Assert(paxPlayer != nullptr && uPlayerCount > 0u, "Begin: player party is empty");
	Zenith_Assert(paxEnemy != nullptr && uEnemyCount > 0u, "Begin: enemy party is empty");

	m_xConfig = xConfig;
	m_bOver   = false;
	m_eWinner = ZM_SIDE_COUNT;
	m_abSubmitted[ZM_SIDE_PLAYER] = false;
	m_abSubmitted[ZM_SIDE_ENEMY]  = false;

	ZM_BattleSide& xPlayer = m_xState.Side(ZM_SIDE_PLAYER);
	ZM_BattleSide& xEnemy  = m_xState.Side(ZM_SIDE_ENEMY);

	xPlayer.m_xParty.Clear();
	xEnemy.m_xParty.Clear();
	for (u_int u = 0; u < uPlayerCount; ++u)
	{
		xPlayer.m_xParty.PushBack(ZM_BuildBattleMonster(paxPlayer[u]));
	}
	for (u_int u = 0; u < uEnemyCount; ++u)
	{
		xEnemy.m_xParty.PushBack(ZM_BuildBattleMonster(paxEnemy[u]));
	}
	xPlayer.m_uActiveSlot = 0u;
	xEnemy.m_uActiveSlot  = 0u;

	m_xState.m_xField = ZM_FieldState();       // reset weather / turn counter
	m_xState.m_xRNG.Seed(ulSeed, ulSeq);

	m_xEvents.Clear();
	Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_BATTLE_BEGIN));
	Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_SWITCH_IN, ZM_SIDE_PLAYER, xPlayer.m_uActiveSlot,
		ZM_MOVE_NONE, (u_int)xPlayer.Active().m_eSpecies));
	Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_SWITCH_IN, ZM_SIDE_ENEMY, xEnemy.m_uActiveSlot,
		ZM_MOVE_NONE, (u_int)xEnemy.Active().m_eSpecies));
}

void ZM_BattleEngine::SubmitAction(ZM_SIDE eSide, const ZM_BattleAction& xAction)
{
	Zenith_Assert(!m_bOver, "SubmitAction: battle is already over");
	Zenith_Assert(eSide == ZM_SIDE_PLAYER || eSide == ZM_SIDE_ENEMY, "SubmitAction: invalid side");
	Zenith_Assert(xAction.m_eKind == ZM_ACTION_MOVE, "SubmitAction: box 1 only supports MOVE actions");
	Zenith_Assert(xAction.m_uMoveSlot < uZM_MAX_MOVES, "SubmitAction: move slot %u out of range", xAction.m_uMoveSlot);

	const ZM_BattleMonster& xActive = m_xState.Side(eSide).Active();
	Zenith_Assert(!xActive.IsFainted(), "SubmitAction: active monster is fainted");

	const ZM_MoveSlot& xSlot = xActive.m_axMoves[xAction.m_uMoveSlot];
	Zenith_Assert(xSlot.m_eMove != ZM_MOVE_NONE, "SubmitAction: move slot %u is empty", xAction.m_uMoveSlot);
	Zenith_Assert(xSlot.m_uCurPP > 0u, "SubmitAction: move slot %u has no PP", xAction.m_uMoveSlot);

	m_axPending[eSide]   = xAction;
	m_abSubmitted[eSide] = true;
}

void ZM_BattleEngine::ResolveTurn()
{
	if (m_bOver)
	{
		return;
	}
	Zenith_Assert(m_abSubmitted[ZM_SIDE_PLAYER] && m_abSubmitted[ZM_SIDE_ENEMY],
		"ResolveTurn: both sides must submit an action first");

	++m_xState.m_xField.m_uTurnCounter;
	const int iTurn = (int)m_xState.m_xField.m_uTurnCounter;
	Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_TURN_BEGIN, ZM_SIDE_COUNT, 0u, ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, iTurn));

	ResolvePreMovePhase();     // box 1: no-op (run/item/switch deferred to box 2)
	ResolveMovePhase();
	ResolveEndOfTurnPhase();   // box 1: emit TURN_END

	// battle-over check AFTER end-of-turn (box 2 status/weather can KO here too).
	if (!m_xState.Side(ZM_SIDE_PLAYER).HasUnfainted() || !m_xState.Side(ZM_SIDE_ENEMY).HasUnfainted())
	{
		m_bOver = true;
		const bool bPlayerAlive = m_xState.Side(ZM_SIDE_PLAYER).HasUnfainted();
		const bool bEnemyAlive  = m_xState.Side(ZM_SIDE_ENEMY).HasUnfainted();
		m_eWinner = (bPlayerAlive && !bEnemyAlive) ? ZM_SIDE_PLAYER
		          : ((bEnemyAlive && !bPlayerAlive) ? ZM_SIDE_ENEMY : ZM_SIDE_COUNT);
		Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_BATTLE_END, ZM_SIDE_COUNT, 0u, ZM_MOVE_NONE, ZM_SPECIES_NONE,
			(int)m_eWinner, 0));
	}

	m_abSubmitted[ZM_SIDE_PLAYER] = false;
	m_abSubmitted[ZM_SIDE_ENEMY]  = false;
}

void ZM_BattleEngine::ResolvePreMovePhase()
{
	// box 1: no-op. Run / item / switch actions arrive in box 2.
}

void ZM_BattleEngine::ResolveMovePhase()
{
	// Both actions are MOVE in box 1. Compute all ordering inputs BEFORE any
	// ExecuteMove mutates the parties.
	const ZM_BattleMonster& xPlayer = m_xState.Side(ZM_SIDE_PLAYER).Active();
	const ZM_BattleMonster& xEnemy  = m_xState.Side(ZM_SIDE_ENEMY).Active();

	const ZM_MOVE_ID ePlayerMove = xPlayer.m_axMoves[m_axPending[ZM_SIDE_PLAYER].m_uMoveSlot].m_eMove;
	const ZM_MOVE_ID eEnemyMove  = xEnemy.m_axMoves[m_axPending[ZM_SIDE_ENEMY].m_uMoveSlot].m_eMove;
	const int iPlayerPriority = ZM_GetMoveData(ePlayerMove).m_iPriority;
	const int iEnemyPriority  = ZM_GetMoveData(eEnemyMove).m_iPriority;

	ZM_SIDE eFirst;
	if (iPlayerPriority != iEnemyPriority)
	{
		eFirst = (iPlayerPriority > iEnemyPriority) ? ZM_SIDE_PLAYER : ZM_SIDE_ENEMY;
	}
	else
	{
		const u_int uPlayerSpeed = ZM_ApplyStatStage(xPlayer.m_auMaxStat[ZM_STAT_SPEED],
			xPlayer.m_aiStage[ZM_BATTLE_STAT_SPEED]);
		const u_int uEnemySpeed  = ZM_ApplyStatStage(xEnemy.m_auMaxStat[ZM_STAT_SPEED],
			xEnemy.m_aiStage[ZM_BATTLE_STAT_SPEED]);
		if (uPlayerSpeed != uEnemySpeed)
		{
			eFirst = (uPlayerSpeed > uEnemySpeed) ? ZM_SIDE_PLAYER : ZM_SIDE_ENEMY;
		}
		else
		{
			// The ONLY tie-break draw, and only on exact-equal effective speed.
			eFirst = (m_xState.m_xRNG.RandBelow(2u) == 0u) ? ZM_SIDE_PLAYER : ZM_SIDE_ENEMY;
		}
	}
	const ZM_SIDE eSecond = (eFirst == ZM_SIDE_PLAYER) ? ZM_SIDE_ENEMY : ZM_SIDE_PLAYER;

	ExecuteMove(eFirst);
	if (!m_bOver && !m_xState.Side(eSecond).Active().IsFainted())
	{
		ExecuteMove(eSecond);
	}
}

void ZM_BattleEngine::ExecuteMove(ZM_SIDE eAtk)
{
	const ZM_SIDE eDef = (eAtk == ZM_SIDE_PLAYER) ? ZM_SIDE_ENEMY : ZM_SIDE_PLAYER;
	ZM_BattleSide& xAtkSide = m_xState.Side(eAtk);
	ZM_BattleSide& xDefSide = m_xState.Side(eDef);
	ZM_BattleMonster& xAtk = xAtkSide.Active();
	ZM_BattleMonster& xDef = xDefSide.Active();

	if (xAtk.IsFainted())
	{
		return;   // a fainted actor never acts
	}

	const u_int uSlot = m_axPending[eAtk].m_uMoveSlot;
	ZM_MoveSlot& xMoveSlot = xAtk.m_axMoves[uSlot];
	const ZM_MoveData& xMove = ZM_GetMoveData(xMoveSlot.m_eMove);

	// Spend PP + announce (box 1 always has PP; NO_PP is box 2).
	--xMoveSlot.m_uCurPP;
	Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_USED, eAtk, xAtkSide.m_uActiveSlot, (u_int)xMove.m_eId));

	// (1) ACCURACY draw -- only if the move CAN miss (sure-hit short-circuit).
	if (xMove.m_uAccuracy != uZM_MOVE_ACCURACY_ALWAYS_HITS && xMove.m_uAccuracy < 100u)
	{
		if (m_xState.m_xRNG.RandBelow(100u) >= xMove.m_uAccuracy)
		{
			Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_MISSED, eAtk, xAtkSide.m_uActiveSlot, (u_int)xMove.m_eId));
			return;
		}
	}

	// Effectiveness is deterministic (no RNG) -- resolve immunity BEFORE a crit draw.
	const ZM_SpeciesData& xDefSpecies = ZM_GetSpeciesData(xDef.m_eSpecies);
	const u_int uEffPct = ZM_EffectivenessPercent(xMove.m_eType, xDefSpecies.m_aeTypes[0], xDefSpecies.m_aeTypes[1]);
	if (uEffPct == 0u)
	{
		Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_IMMUNE, eDef, xDefSide.m_uActiveSlot));
		return;
	}

	// (2) CRIT draw -- guaranteed (no draw) at critStage>=2; else 1/24.
	const bool bCrit = (xMove.m_uCritStage >= 2u) ? true : m_xState.m_xRNG.Chance(1u, 24u);
	// (3) DAMAGE ROLL draw -- 85..100 inclusive.
	const u_int uRoll = m_xState.m_xRNG.RandRange(85u, 100u);

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
		Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_CRIT, eDef, xDefSide.m_uActiveSlot));
	}
	if (uEffPct > 100u)
	{
		Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_SUPER_EFFECTIVE, eDef, xDefSide.m_uActiveSlot,
			ZM_MOVE_NONE, ZM_SPECIES_NONE, (int)uEffPct));
	}
	else if (uEffPct < 100u)
	{
		Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_NOT_EFFECTIVE, eDef, xDefSide.m_uActiveSlot,
			ZM_MOVE_NONE, ZM_SPECIES_NONE, (int)uEffPct));
	}

	xDef.m_uCurHP = (uDmg >= xDef.m_uCurHP) ? 0u : (xDef.m_uCurHP - uDmg);
	Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_DAMAGE_DEALT, eDef, xDefSide.m_uActiveSlot,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, (int)uDmg, (int)xDef.m_uCurHP));

	if (xDef.m_uCurHP == 0u)
	{
		CheckFaintAndMaybeEnd(eDef);   // emits FAINT; battle-over decided in ResolveTurn
	}
}

void ZM_BattleEngine::ResolveEndOfTurnPhase()
{
	const int iTurn = (int)m_xState.m_xField.m_uTurnCounter;
	Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_TURN_END, ZM_SIDE_COUNT, 0u, ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, iTurn));
}

bool ZM_BattleEngine::CheckFaintAndMaybeEnd(ZM_SIDE eDefender)
{
	ZM_BattleSide& xDefSide = m_xState.Side(eDefender);
	Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_FAINT, eDefender, xDefSide.m_uActiveSlot));
	// box 1 is 1v1: the side is now wiped, so ResolveTurn's post-phase check ends
	// the battle. box 2 seam: if the side has reserves, request a forced switch here.
	return !xDefSide.HasUnfainted();
}
