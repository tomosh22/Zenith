#include "Zenith.h"

#include "Zenithmon/Source/Battle/ZM_BattleEngine.h"
#include "Zenithmon/Source/Battle/ZM_MoveExecutor.h"
#include "Zenithmon/Source/Battle/ZM_StatusLogic.h"
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

	const ZM_BattleMonster& xActive = m_xState.Side(eSide).Active();
	Zenith_Assert(!xActive.IsFainted(), "SubmitAction: active monster is fainted");
	const bool bCharged = (xActive.m_uVolatileMask & (u_int)ZM_VOLATILE_CHARGE) != 0u;
	const bool bLocked = (xActive.m_uVolatileMask & (u_int)ZM_VOLATILE_LOCK) != 0u;
	Zenith_Assert(!(bCharged && bLocked),
		"SubmitAction: active monster cannot be CHARGE and LOCK simultaneously");

	ZM_BattleAction xResolved = xAction;
	if (bCharged)
	{
		xResolved.m_uMoveSlot = xActive.m_uChargeMoveSlot;
	}
	else if (bLocked)
	{
		xResolved.m_uMoveSlot = xActive.m_uLockMoveSlot;
	}
	Zenith_Assert(xResolved.m_uMoveSlot < uZM_MAX_MOVES,
		"SubmitAction: move slot %u out of range", xResolved.m_uMoveSlot);

	const ZM_MoveSlot& xSlot = xActive.m_axMoves[xResolved.m_uMoveSlot];
	Zenith_Assert(xSlot.m_eMove != ZM_MOVE_NONE, "SubmitAction: move slot %u is empty", xResolved.m_uMoveSlot);
	Zenith_Assert(xSlot.m_uCurPP > 0u || bCharged,
		"SubmitAction: move slot %u has no PP", xResolved.m_uMoveSlot);

	m_axPending[eSide]   = xResolved;
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

	ResolvePreMovePhase();     // no-op until SC6 voluntary switch/item/run actions
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
	// Run / item / voluntary-switch actions arrive in SC6.
}

// Effective speed for turn ordering: the SPEED stat after its stat-stage multiplier,
// then a 1/4 integer cut if the monster is PARALYZED (SC4; ZM-D-033; NO draw). A
// status-free, stage-0 monster is unchanged, so box-1 / SC1-SC3 turn ordering is
// byte-identical.
static u_int g_EffectiveSpeed(const ZM_BattleMonster& xMon)
{
	u_int uSpeed = ZM_ApplyStatStage(xMon.m_auMaxStat[ZM_STAT_SPEED], xMon.m_aiStage[ZM_BATTLE_STAT_SPEED]);
	if (xMon.m_eStatus == ZM_MAJOR_STATUS_PARALYSIS)
	{
		uSpeed /= 4u;
	}
	return uSpeed;
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
		const u_int uPlayerSpeed = g_EffectiveSpeed(xPlayer);
		const u_int uEnemySpeed  = g_EffectiveSpeed(xEnemy);
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

	const ZM_SIDE eForcedSide = ExecuteMove(eFirst);
	if (!m_bOver && eForcedSide != eSecond
		&& !m_xState.Side(eSecond).Active().IsFainted())
	{
		ExecuteMove(eSecond);
	}
}

ZM_SIDE ZM_BattleEngine::ExecuteMove(ZM_SIDE eAtk)
{
	const ZM_SIDE eDef = (eAtk == ZM_SIDE_PLAYER) ? ZM_SIDE_ENEMY : ZM_SIDE_PLAYER;

	if (m_xState.Side(eAtk).Active().IsFainted())
	{
		return ZM_SIDE_COUNT;   // a fainted actor never acts
	}

	// The engine keeps owning the state / event sink / RNG; the context is a thin
	// view. ZM_MoveExecutor runs the whole post-guard body (PP, MOVE_USED, accuracy,
	// immunity, effect switch -> damaging hit + faint) with identical draw + emit order.
	ZM_MoveContext xCtx;
	xCtx.m_pxState   = &m_xState;
	xCtx.m_pxEvents  = &m_xEvents;
	xCtx.m_eAtk      = eAtk;
	xCtx.m_eDef      = eDef;
	xCtx.m_uMoveSlot = m_axPending[eAtk].m_uMoveSlot;
	const ZM_MoveResult xResult = ZM_MoveExecutor::Execute(xCtx);
	if (xResult.m_eForcedSwitchSide < ZM_SIDE_COUNT
		&& DoSwitch(xResult.m_eForcedSwitchSide, xResult.m_uForcedSwitchSlot))
	{
		return xResult.m_eForcedSwitchSide;
	}
	return ZM_SIDE_COUNT;
}

bool ZM_BattleEngine::DoSwitch(ZM_SIDE eSide, u_int uTargetSlot)
{
	if (eSide >= ZM_SIDE_COUNT)
	{
		return false;
	}
	ZM_BattleSide& xSide = m_xState.Side(eSide);
	if (!xSide.CanSwitchTo(uTargetSlot))
	{
		return false;
	}

	ZM_StatusLogic::ResetSwitchTransients(xSide.Active());
	xSide.m_uActiveSlot = uTargetSlot;
	Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_SWITCH_IN, eSide, uTargetSlot,
		ZM_MOVE_NONE, (u_int)xSide.Active().m_eSpecies));
	return true;
}

void ZM_BattleEngine::ResolveEndOfTurnPhase()
{
	// END-OF-TURN GLOBAL ORDER (ZM-D-033): the box-3 weather-chip slot is reserved
	// FIRST here (absent in box-2 tests -- no code yet). Then the per-side status ticks
	// in the LOCKED side order PLAYER-then-ENEMY. SC4 does the major chip (poison/toxic/
	// burn); leech-seed / trap / volatile-counter expiries append into this per-side
	// order in SC5. A status-free side's EndOfTurn emits nothing, so box-1 / SC1-SC3
	// end-of-turn streams (just TURN_END) stay byte-identical.
	ZM_StatusLogic::EndOfTurn(m_xState, m_xEvents, ZM_SIDE_PLAYER);
	ZM_StatusLogic::EndOfTurn(m_xState, m_xEvents, ZM_SIDE_ENEMY);

	const int iTurn = (int)m_xState.m_xField.m_uTurnCounter;
	Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_TURN_END, ZM_SIDE_COUNT, 0u, ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, iTurn));
}
