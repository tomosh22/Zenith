#include "Zenith.h"

#include "Zenithmon/Source/Battle/ZM_BattleEngine.h"
#include "Zenithmon/Source/Battle/ZM_MoveExecutor.h"
#include "Zenithmon/Source/Battle/ZM_StatusLogic.h"
#include "Zenithmon/Source/Battle/ZM_AbilityHooks.h"
#include "Zenithmon/Source/Battle/ZM_CatchCalc.h"
#include "Zenithmon/Source/Battle/ZM_DamageCalc.h"
#include "Zenithmon/Source/Battle/ZM_ExpAndLevel.h"
#include "Zenithmon/Source/Data/ZM_MoveData.h"
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"
#include "Zenithmon/Source/Data/ZM_ItemData.h"

// ============================================================================
// Box-1 turn loop. Draw discipline (locked): per move, accuracy (only if the
// move can miss) -> crit -> damage-roll; single speed tie-break draw before any
// move on exact-equal effective speed. Emit order per connecting hit (locked):
// MOVE_USED -> { MISS | IMMUNE | ([CRIT] [SUPER|NOT] DAMAGE_DEALT [FAINT]) }.
// ============================================================================

// Effective speed (stat-stage fold + paralysis 1/4 cut); defined below, forward-
// declared so the SC6 pre-move handlers (which precede it) can use it for flee odds.
static u_int g_EffectiveSpeed(const ZM_BattleMonster& xMon);

// Dispatch immediately after the incoming monster's SWITCH_IN event. A null
// row/slot is deliberately inert, preserving every NONE-ability stream and RNG
// cursor. Initial leads and later switches share this exact path.
static void g_DispatchSwitchInAbility(ZM_BattleState& xState,
	Zenith_Vector<ZM_BattleEvent>& xEvents, ZM_SIDE eSelf)
{
	ZM_BattleMonster& xMon = xState.Side(eSelf).Active();
	const ZM_AbilityHooks* pxHooks = ZM_GetAbilityHooks(xMon.m_eAbility);
	if (pxHooks == nullptr || pxHooks->pfnOnSwitchIn == nullptr)
	{
		return;
	}

	ZM_AbilityContext xCtx;
	xCtx.m_pxState = &xState;
	xCtx.m_pxEvents = &xEvents;
	xCtx.m_eSelf = eSelf;
	xCtx.m_eOther = eSelf == ZM_SIDE_PLAYER ? ZM_SIDE_ENEMY : ZM_SIDE_PLAYER;
	pxHooks->pfnOnSwitchIn(xCtx);
}

// SC5 end-of-turn ability heals (LOCKED EoT step 6): dispatched AFTER both sides'
// status ticks and BEFORE the TURN_END emit, in fixed PLAYER-then-ENEMY order. A
// fainted active is SKIPPED (Resolution 2: a mon KO'd earlier this turn is never
// healed/revived) -- it draws/emits nothing. Zero RNG draws, and a NONE-ability /
// no-TURN_END-hook active emits nothing, so a weather-NONE box-1/SC1-SC4 turn stays
// byte-identical. Mirrors the free-function dispatch of g_DispatchSwitchInAbility:
// no engine member state is needed since the ability context emits through the
// passed event sink.
static void g_DispatchTurnEndAbilities(ZM_BattleState& xState,
	Zenith_Vector<ZM_BattleEvent>& xEvents)
{
	for (u_int uSideIdx = 0u; uSideIdx < (u_int)ZM_SIDE_COUNT; ++uSideIdx)
	{
		const ZM_SIDE eSide = (ZM_SIDE)uSideIdx;
		ZM_BattleMonster& xMon = xState.Side(eSide).Active();
		if (xMon.IsFainted())
		{
			continue;   // Resolution 2 fainted-guard: no heal on a fainted active
		}
		const ZM_AbilityHooks* pxHooks = ZM_GetAbilityHooks(xMon.m_eAbility);
		if (pxHooks == nullptr || pxHooks->pfnOnTurnEnd == nullptr)
		{
			continue;
		}
		ZM_AbilityContext xCtx;
		xCtx.m_pxState  = &xState;
		xCtx.m_pxEvents = &xEvents;
		xCtx.m_eSelf    = eSide;
		xCtx.m_eOther   = (eSide == ZM_SIDE_PLAYER) ? ZM_SIDE_ENEMY : ZM_SIDE_PLAYER;
		pxHooks->pfnOnTurnEnd(xCtx);
	}
}

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
	m_auFleeAttempts[ZM_SIDE_PLAYER] = 0u;
	m_auFleeAttempts[ZM_SIDE_ENEMY]  = 0u;

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
	g_DispatchSwitchInAbility(m_xState, m_xEvents, ZM_SIDE_PLAYER);
	Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_SWITCH_IN, ZM_SIDE_ENEMY, xEnemy.m_uActiveSlot,
		ZM_MOVE_NONE, (u_int)xEnemy.Active().m_eSpecies));
	g_DispatchSwitchInAbility(m_xState, m_xEvents, ZM_SIDE_ENEMY);
	MarkCurrentParticipants();   // initial matchup; strict no-op for legacy award-off configs
}

void ZM_BattleEngine::SubmitAction(ZM_SIDE eSide, const ZM_BattleAction& xAction)
{
	Zenith_Assert(!m_bOver, "SubmitAction: battle is already over");
	Zenith_Assert(eSide == ZM_SIDE_PLAYER || eSide == ZM_SIDE_ENEMY, "SubmitAction: invalid side");

	const ZM_BattleMonster& xActive = m_xState.Side(eSide).Active();
	Zenith_Assert(!xActive.IsFainted(), "SubmitAction: active monster is fainted");
	const bool bCharged = (xActive.m_uVolatileMask & (u_int)ZM_VOLATILE_CHARGE) != 0u;
	const bool bLocked = (xActive.m_uVolatileMask & (u_int)ZM_VOLATILE_LOCK) != 0u;
	Zenith_Assert(!(bCharged && bLocked),
		"SubmitAction: active monster cannot be CHARGE and LOCK simultaneously");

	if (xAction.m_eKind == ZM_ACTION_MOVE)
	{
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
		return;
	}

	// SC6 pre-move actions. A charged / locked active is committed to its stored move
	// (mainline: cannot switch / run mid-charge or mid-lock), so a non-MOVE action is
	// illegal for it; the caller must submit legal actions.
	Zenith_Assert(!bCharged && !bLocked,
		"SubmitAction: a charged/locked active must use its committed move, not a pre-move action");

	switch (xAction.m_eKind)
	{
	case ZM_ACTION_SWITCH:
		Zenith_Assert(xAction.m_uSwitchSlot < m_xState.Side(eSide).m_xParty.GetSize(),
			"SubmitAction: switch slot %u out of party range", xAction.m_uSwitchSlot);
		break;
	case ZM_ACTION_ITEM:
		Zenith_Assert(m_xConfig.m_bCanCatch, "SubmitAction: ITEM (catch) requires a wild-catch config");
		Zenith_Assert(xAction.m_eItem < ZM_ITEM_COUNT
			&& ZM_GetItemData(xAction.m_eItem).m_eCategory == ZM_ITEM_CATEGORY_BALL,
			"SubmitAction: the SC6 ITEM action only supports ball items");
		break;
	case ZM_ACTION_RUN:
		Zenith_Assert(m_xConfig.m_bCanFlee, "SubmitAction: RUN requires a wild-flee config");
		break;
	default:
		Zenith_Assert(false, "SubmitAction: unsupported action kind");
		break;
	}

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

	// PHASE order (ZM-D-033): pre-move run/item/switch resolve BEFORE any move. A
	// successful flee or capture ends the battle here, so the move + end-of-turn
	// phases are skipped and the turn is closed directly (TURN_END -> BATTLE_END).
	// When both sides submit MOVE, pre-move is inert and this is the box-1 flow.
	ResolvePreMovePhase();

	if (m_bOver)
	{
		Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_TURN_END, ZM_SIDE_COUNT, 0u, ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, iTurn));
		QueueTerminalEvolutions();
		Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_BATTLE_END, ZM_SIDE_COUNT, 0u, ZM_MOVE_NONE, ZM_SPECIES_NONE,
			(int)m_eWinner, 0));
	}
	else
	{
		ResolveMovePhase();
		AwardExpForNewFaints();    // box 4: move-KO exp appends right after the move phase's FAINT
		ResolveEndOfTurnPhase();   // box 1: emit TURN_END

		// battle-over check AFTER end-of-turn (box 2 status/weather can KO here too).
		if (!m_xState.Side(ZM_SIDE_PLAYER).HasUnfainted() || !m_xState.Side(ZM_SIDE_ENEMY).HasUnfainted())
		{
			m_bOver = true;
			const bool bPlayerAlive = m_xState.Side(ZM_SIDE_PLAYER).HasUnfainted();
			const bool bEnemyAlive  = m_xState.Side(ZM_SIDE_ENEMY).HasUnfainted();
			m_eWinner = (bPlayerAlive && !bEnemyAlive) ? ZM_SIDE_PLAYER
			          : ((bEnemyAlive && !bPlayerAlive) ? ZM_SIDE_ENEMY : ZM_SIDE_COUNT);
			QueueTerminalEvolutions();
			Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_BATTLE_END, ZM_SIDE_COUNT, 0u, ZM_MOVE_NONE, ZM_SPECIES_NONE,
				(int)m_eWinner, 0));
		}
	}

	m_abSubmitted[ZM_SIDE_PLAYER] = false;
	m_abSubmitted[ZM_SIDE_ENEMY]  = false;
}

void ZM_BattleEngine::ResolvePreMovePhase()
{
	// SC6 (ZM-D-035): voluntary SWITCH / ITEM(catch) / RUN resolve here, BEFORE any
	// move, in fixed PLAYER-then-ENEMY order (the EOT side-order convention). A
	// catch / flee sets m_bOver, so the loop stops the second side's pre-move action.
	// MOVE / NONE actions are left for the move phase, so a both-MOVE turn is a no-op
	// here (no draws, no events, no state change) and the box-1 goldens are unmoved.
	for (u_int uSideIdx = 0u; uSideIdx < (u_int)ZM_SIDE_COUNT; ++uSideIdx)
	{
		if (m_bOver)
		{
			return;
		}
		const ZM_SIDE eSide = (ZM_SIDE)uSideIdx;
		const ZM_BattleAction& xAction = m_axPending[eSide];
		switch (xAction.m_eKind)
		{
		case ZM_ACTION_SWITCH: DoVoluntarySwitch(eSide, xAction.m_uSwitchSlot); break;
		case ZM_ACTION_ITEM:   DoItemAction(eSide, xAction.m_eItem);            break;
		case ZM_ACTION_RUN:    DoRunAction(eSide);                              break;
		default: break;   // MOVE / NONE -> handled in ResolveMovePhase
		}
	}
}

// A voluntary switch-out. A TRAPPED active cannot leave (MOVE_FAILED(TRAPPED)); an
// otherwise-illegal destination (fainted / current / out of range) reports
// MOVE_FAILED(NO_SWITCH_TARGET). On success DoSwitch emits SWITCH_IN and resets the
// outgoing monster's battle-local transients. Either way the switching side does not
// move this turn.
void ZM_BattleEngine::DoVoluntarySwitch(ZM_SIDE eSide, u_int uTargetSlot)
{
	ZM_BattleSide& xSide = m_xState.Side(eSide);
	const ZM_BattleMonster& xActive = xSide.Active();
	if ((xActive.m_uVolatileMask & (u_int)ZM_VOLATILE_TRAP) != 0u)
	{
		Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_FAILED, eSide, xSide.m_uActiveSlot,
			ZM_MOVE_NONE, (u_int)xActive.m_eSpecies, 0, (int)ZM_MOVE_FAIL_TRAPPED));
		return;
	}
	if (!DoSwitch(eSide, uTargetSlot))
	{
		Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_FAILED, eSide, xSide.m_uActiveSlot,
			ZM_MOVE_NONE, (u_int)xActive.m_eSpecies, 0, (int)ZM_MOVE_FAIL_NO_SWITCH_TARGET));
	}
}

// A ball item == a capture attempt on the OPPOSING active (wild only). Emits one
// CATCH_SHAKE per successful wobble (m_iAmount = shake index 1..4, m_iTag = ball id)
// then CATCH_RESULT (m_iAmount = caught 1/0, m_iAux = shake count, m_iTag = ball id).
// A capture ends the battle with the catching side as winner; the actual party-add is
// box 4/5. The event side/slot are the caught target's.
void ZM_BattleEngine::DoItemAction(ZM_SIDE eSide, ZM_ITEM_ID eItem)
{
	Zenith_Assert(m_xConfig.m_bCanCatch, "DoItemAction: catching requires a wild-catch config");
	const ZM_SIDE eTgt = (eSide == ZM_SIDE_PLAYER) ? ZM_SIDE_ENEMY : ZM_SIDE_PLAYER;
	ZM_BattleSide& xTgtSide = m_xState.Side(eTgt);
	const ZM_BattleMonster& xTarget = xTgtSide.Active();

	const ZM_CatchResult xResult = ZM_CatchCalc::Roll(xTarget, eItem, m_xState.m_xRNG);
	for (u_int k = 0u; k < xResult.m_uShakes; ++k)
	{
		Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_CATCH_SHAKE, eTgt, xTgtSide.m_uActiveSlot,
			ZM_MOVE_NONE, (u_int)xTarget.m_eSpecies, (int)(k + 1u), 0, (int)eItem));
	}
	Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_CATCH_RESULT, eTgt, xTgtSide.m_uActiveSlot,
		ZM_MOVE_NONE, (u_int)xTarget.m_eSpecies, xResult.m_bCaught ? 1 : 0,
		(int)xResult.m_uShakes, (int)eItem));

	if (xResult.m_bCaught)
	{
		m_bOver = true;
		m_eWinner = eSide;   // the catching side wins the wild battle
	}
}

// A run attempt (wild only). Guaranteed when the runner's effective speed >= the
// opponent's (no draw); otherwise ZM_RollFlee draws one RandBelow(256). A success
// emits FLEE and ends the battle with no winner (COUNT); a failure emits FLEE_FAILED
// and the turn continues (the runner forfeits its move). The attempt counter ramps
// the odds across repeated tries in the same battle.
void ZM_BattleEngine::DoRunAction(ZM_SIDE eSide)
{
	Zenith_Assert(m_xConfig.m_bCanFlee, "DoRunAction: fleeing requires a wild-flee config");
	const ZM_SIDE eOpp = (eSide == ZM_SIDE_PLAYER) ? ZM_SIDE_ENEMY : ZM_SIDE_PLAYER;
	const u_int uSelfSpeed = g_EffectiveSpeed(m_xState.Side(eSide).Active());
	const u_int uOppSpeed  = g_EffectiveSpeed(m_xState.Side(eOpp).Active());
	++m_auFleeAttempts[eSide];

	const bool bFled = ZM_RollFlee(uSelfSpeed, uOppSpeed, m_auFleeAttempts[eSide], m_xState.m_xRNG);
	const ZM_BattleSide& xSide = m_xState.Side(eSide);
	const u_int uSlot = xSide.m_uActiveSlot;
	const u_int uSpecies = (u_int)xSide.Active().m_eSpecies;
	if (bFled)
	{
		Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_FLEE, eSide, uSlot, ZM_MOVE_NONE, uSpecies));
		m_bOver = true;
		m_eWinner = ZM_SIDE_COUNT;   // a flee has no winner (battle abandoned)
	}
	else
	{
		Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_FLEE_FAILED, eSide, uSlot, ZM_MOVE_NONE, uSpecies));
	}
}

// Base effective speed shared by turn ordering and flee: the SPEED stat after its
// stat-stage multiplier, then a 1/4 integer cut if the monster is PARALYZED
// (SC4; ZM-D-033; NO draw).
static u_int g_EffectiveSpeed(const ZM_BattleMonster& xMon)
{
	u_int uSpeed = ZM_ApplyStatStage(xMon.m_auMaxStat[ZM_STAT_SPEED], xMon.m_aiStage[ZM_BATTLE_STAT_SPEED]);
	if (xMon.m_eStatus == ZM_MAJOR_STATUS_PARALYSIS)
	{
		uSpeed /= 4u;
	}
	return uSpeed;
}

// Weather speed abilities are deliberately move-order-only. Flee continues to
// call g_EffectiveSpeed directly, preserving its existing odds and draw stream.
static u_int g_EffectiveSpeedForMoveOrder(ZM_BattleState& xState,
	Zenith_Vector<ZM_BattleEvent>& xEvents, ZM_SIDE eSide)
{
	const ZM_BattleMonster& xMon = xState.Side(eSide).Active();
	u_int uSpeed = g_EffectiveSpeed(xMon);
	const ZM_AbilityHooks* pxHooks = ZM_GetAbilityHooks(xMon.m_eAbility);
	if (pxHooks == nullptr || pxHooks->pfnModifyStat == nullptr)
	{
		return uSpeed;
	}

	ZM_AbilityContext xCtx;
	xCtx.m_pxState = &xState;
	xCtx.m_pxEvents = &xEvents;
	xCtx.m_eSelf = eSide;
	xCtx.m_eOther = (eSide == ZM_SIDE_PLAYER)
		? ZM_SIDE_ENEMY : ZM_SIDE_PLAYER;
	return pxHooks->pfnModifyStat(xCtx, ZM_BATTLE_STAT_SPEED, uSpeed);
}

void ZM_BattleEngine::ResolveMovePhase()
{
	// Only MOVE-action sides fight this phase; a side that switched / used an item /
	// failed to flee already spent its turn in the pre-move phase.
	const bool bPlayerMoves = (m_axPending[ZM_SIDE_PLAYER].m_eKind == ZM_ACTION_MOVE);
	const bool bEnemyMoves  = (m_axPending[ZM_SIDE_ENEMY].m_eKind == ZM_ACTION_MOVE);

	// Exactly one mover: no ordering decision, so NO speed tie-break draw. The mover
	// strikes the current opposing active (possibly just switched in); ExecuteMove
	// self-guards a fainted actor.
	if (bPlayerMoves != bEnemyMoves)
	{
		ExecuteMove(bPlayerMoves ? ZM_SIDE_PLAYER : ZM_SIDE_ENEMY);
		return;
	}
	if (!bPlayerMoves)   // neither side moves (both took pre-move actions)
	{
		return;
	}

	// Both actions are MOVE (the box-1 path -- byte-identical ordering + draws).
	// Compute all ordering inputs BEFORE any ExecuteMove mutates the parties.
	const ZM_BattleMonster& xPlayer = m_xState.Side(ZM_SIDE_PLAYER).Active();
	const ZM_BattleMonster& xEnemy  = m_xState.Side(ZM_SIDE_ENEMY).Active();

	const ZM_MOVE_ID ePlayerMove = xPlayer.m_axMoves[m_axPending[ZM_SIDE_PLAYER].m_uMoveSlot].m_eMove;
	const ZM_MOVE_ID eEnemyMove  = xEnemy.m_axMoves[m_axPending[ZM_SIDE_ENEMY].m_uMoveSlot].m_eMove;
	int iPlayerPriority = ZM_GetMoveData(ePlayerMove).m_iPriority;
	int iEnemyPriority  = ZM_GetMoveData(eEnemyMove).m_iPriority;

	// SC5 QUICKDRAW (ZM-D-036): a holder draws one RandBelow(100); on < 30 it gains
	// +1 move-order priority this turn and announces ABILITY_TRIGGER (RULING (2):
	// proc is observable). The draw is taken ONLY when a QUICKDRAW holder is in the
	// both-move branch, in fixed PLAYER-then-ENEMY order, so a NONE-ability turn keeps
	// the single RandBelow(2) tie-break exactly (byte-identical). This changes move
	// ORDER only; flee stays on the raw-speed DoRunAction path and is untouched.
	if (xPlayer.m_eAbility == ZM_ABILITY_QUICKDRAW
		&& m_xState.m_xRNG.RandBelow(100u) < 30u)
	{
		++iPlayerPriority;
		Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_ABILITY_TRIGGER, ZM_SIDE_PLAYER,
			m_xState.Side(ZM_SIDE_PLAYER).m_uActiveSlot, ZM_MOVE_NONE, ZM_SPECIES_NONE,
			0, 0, (int)ZM_ABILITY_QUICKDRAW));
	}
	if (xEnemy.m_eAbility == ZM_ABILITY_QUICKDRAW
		&& m_xState.m_xRNG.RandBelow(100u) < 30u)
	{
		++iEnemyPriority;
		Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_ABILITY_TRIGGER, ZM_SIDE_ENEMY,
			m_xState.Side(ZM_SIDE_ENEMY).m_uActiveSlot, ZM_MOVE_NONE, ZM_SPECIES_NONE,
			0, 0, (int)ZM_ABILITY_QUICKDRAW));
	}

	ZM_SIDE eFirst;
	if (iPlayerPriority != iEnemyPriority)
	{
		eFirst = (iPlayerPriority > iEnemyPriority) ? ZM_SIDE_PLAYER : ZM_SIDE_ENEMY;
	}
	else
	{
		// Fixed PLAYER-then-ENEMY evaluation order keeps ability events stable.
		const u_int uPlayerSpeed = g_EffectiveSpeedForMoveOrder(
			m_xState, m_xEvents, ZM_SIDE_PLAYER);
		const u_int uEnemySpeed = g_EffectiveSpeedForMoveOrder(
			m_xState, m_xEvents, ZM_SIDE_ENEMY);
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
	g_DispatchSwitchInAbility(m_xState, m_xEvents, eSide);
	MarkCurrentParticipants();
	return true;
}

void ZM_BattleEngine::ResolveEndOfTurnPhase()
{
	// END-OF-TURN GLOBAL ORDER (ZM-D-033): box-3 weather (SAND/SNOW chip then the
	// weather countdown) runs FIRST, then the screen countdown, THEN the per-side status
	// ticks in the LOCKED side order PLAYER-then-ENEMY, then TURN_END. SC4 does the major
	// chip (poison/toxic/burn); leech-seed / trap / volatile-counter expiries append into
	// the per-side EndOfTurn. A weather-NONE, screen-free, status-free turn adds no events
	// here, so box-1 / SC1-SC3 end-of-turn streams (just TURN_END) stay byte-identical.
	ResolveWeatherEndOfTurn();
	ResolveScreenEndOfTurn();
	ZM_StatusLogic::EndOfTurn(m_xState, m_xEvents, ZM_SIDE_PLAYER);
	ZM_StatusLogic::EndOfTurn(m_xState, m_xEvents, ZM_SIDE_ENEMY);
	g_DispatchTurnEndAbilities(m_xState, m_xEvents);   // SC5 step 6 (before TURN_END)
	AwardExpForNewFaints();   // box 4: weather/status-chip-KO exp appends before the turn closes

	const int iTurn = (int)m_xState.m_xField.m_uTurnCounter;
	Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_TURN_END, ZM_SIDE_COUNT, 0u, ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, iTurn));
}

// SC1 weather end-of-turn: the reserved-FIRST EOT slot. (1) SAND/SNOW deal a residual
// chip -- PLAYER-active then ENEMY-active, skipping a fainted active and any type-immune
// mon (SAND: EARTH/STONE/IRON; SNOW: ICE); RAIN/SUN never chip. Chip = maxHP/8 (min 1),
// underflow-clamped like the status chip; a chip to 0 emits FAINT (side, active slot)
// as the status path does. (2) The weather clock then ticks; on reaching 0 the weather
// clears and WEATHER_CHANGED-to-NONE fires. NO RNG draws (deterministic), so a NONE-
// weather turn adds nothing and the box-2 EOT streams stay byte-identical.
void ZM_BattleEngine::ResolveWeatherEndOfTurn()
{
	ZM_FieldState& xField = m_xState.m_xField;
	const ZM_WEATHER eWeather = xField.m_eWeather;

	if (eWeather == ZM_WEATHER_SAND || eWeather == ZM_WEATHER_SNOW)
	{
		for (u_int uSideIdx = 0u; uSideIdx < (u_int)ZM_SIDE_COUNT; ++uSideIdx)
		{
			const ZM_SIDE eSide = (ZM_SIDE)uSideIdx;
			ZM_BattleSide&    xSide = m_xState.Side(eSide);
			ZM_BattleMonster& xMon  = xSide.Active();
			if (xMon.IsFainted())
			{
				continue;
			}
			const bool bImmune = (eWeather == ZM_WEATHER_SAND)
				? (ZM_BattleMonsterHasType(xMon, ZM_TYPE_EARTH)
					|| ZM_BattleMonsterHasType(xMon, ZM_TYPE_STONE)
					|| ZM_BattleMonsterHasType(xMon, ZM_TYPE_IRON))
				: ZM_BattleMonsterHasType(xMon, ZM_TYPE_ICE);
			if (bImmune)
			{
				continue;
			}
			u_int uDmg = xMon.m_auMaxStat[ZM_STAT_HP] / 8u;
			if (uDmg == 0u) { uDmg = 1u; }
			xMon.m_uCurHP = uDmg >= xMon.m_uCurHP ? 0u : xMon.m_uCurHP - uDmg;
			Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_WEATHER_DAMAGE, eSide, xSide.m_uActiveSlot,
				ZM_MOVE_NONE, ZM_SPECIES_NONE, (int)uDmg, (int)xMon.m_uCurHP, (int)eWeather));
			if (xMon.IsFainted())
			{
				Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_FAINT, eSide, xSide.m_uActiveSlot));
			}
		}
	}

	if (eWeather != ZM_WEATHER_NONE && xField.m_uWeatherTurns > 0u)
	{
		--xField.m_uWeatherTurns;
		if (xField.m_uWeatherTurns == 0u)
		{
			xField.m_eWeather = ZM_WEATHER_NONE;
			Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_WEATHER_CHANGED, ZM_SIDE_COUNT, 0u,
				ZM_MOVE_NONE, ZM_SPECIES_NONE, (int)ZM_WEATHER_NONE, 0, (int)eWeather));
		}
	}
}

bool ZM_BattleEngine::AwardsExpToSide(ZM_SIDE eSide) const
{
	return m_xConfig.m_bAwardExp && eSide < ZM_SIDE_COUNT
		&& (m_xConfig.m_uExpAwardSideMask & (1u << (u_int)eSide)) != 0u;
}

// Each opponent owns the bitset of opposing party slots that were active while it
// was active. Re-evaluating after every successful switch naturally covers either
// side switching and bitwise OR makes repeated switches idempotent.
void ZM_BattleEngine::MarkCurrentParticipants()
{
	if (!m_xConfig.m_bAwardExp)
	{
		return;
	}
	for (u_int uRecipientSide = 0u; uRecipientSide < (u_int)ZM_SIDE_COUNT; ++uRecipientSide)
	{
		const ZM_SIDE eRecipientSide = (ZM_SIDE)uRecipientSide;
		if (!AwardsExpToSide(eRecipientSide))
		{
			continue;
		}
		const ZM_SIDE eOpponentSide = eRecipientSide == ZM_SIDE_PLAYER
			? ZM_SIDE_ENEMY : ZM_SIDE_PLAYER;
		const u_int uRecipientSlot = m_xState.Side(eRecipientSide).m_uActiveSlot;
		if (uRecipientSlot < 32u)
		{
			m_xState.Side(eOpponentSide).Active().m_uParticipantMask |= 1u << uRecipientSlot;
		}
	}
}

// Box 4 award sweep. Both existing call sites scan every monster on both sides;
// the defeated monster's credit bit makes the operation exactly-once across move
// and EOT sources. Configured recipient sides alone progress. For one defeated
// opponent, living participants each receive the independent full gross award,
// followed by living nonparticipants each receiving max(1,gross/2). Nobody shares
// a pool or remainder. Full EV yield precedes each recipient's EXP/stat mutation.
void ZM_BattleEngine::AwardExpForNewFaints()
{
	if (!m_xConfig.m_bAwardExp)
	{
		return;   // ZERO-PERTURBATION EARLY-OUT: no draws, no state change, no events
	}

	for (u_int uFaintedSide = 0u; uFaintedSide < (u_int)ZM_SIDE_COUNT; ++uFaintedSide)
	{
		const ZM_SIDE eFaintedSide = (ZM_SIDE)uFaintedSide;
		ZM_BattleSide& xDefeatedSide = m_xState.Side(eFaintedSide);
		for (u_int uDefeatedSlot = 0u; uDefeatedSlot < xDefeatedSide.m_xParty.GetSize(); ++uDefeatedSlot)
		{
			ZM_BattleMonster& xDefeated = xDefeatedSide.m_xParty.Get(uDefeatedSlot);
			if (!xDefeated.IsFainted() || xDefeated.m_bDefeatCredited)
			{
				continue;
			}
			xDefeated.m_bDefeatCredited = true;   // even when no recipient is eligible

			const ZM_SIDE eRecipientSide = eFaintedSide == ZM_SIDE_PLAYER
				? ZM_SIDE_ENEMY : ZM_SIDE_PLAYER;
			if (!AwardsExpToSide(eRecipientSide))
			{
				continue;
			}
			ZM_BattleSide& xRecipientSide = m_xState.Side(eRecipientSide);
			const u_int uGross = ZM_CalcExpGain(xDefeated.m_eSpecies,
				xDefeated.m_uLevel, m_xConfig.m_bIsTrainerBattle);
			const u_int uHalf = (uGross / 2u) > 0u ? (uGross / 2u) : 1u;
			const u_int uCap = m_xConfig.m_uLevelCap > 0u
				? m_xConfig.m_uLevelCap : uZM_MAX_LEVEL;
			const ZM_BaseStats xYield = ZM_GetSpeciesEVYield(xDefeated.m_eSpecies);

			for (u_int uGroup = 0u; uGroup < 2u; ++uGroup)
			{
				const bool bParticipantGroup = uGroup == 0u;
				for (u_int uRecipientSlot = 0u;
					uRecipientSlot < xRecipientSide.m_xParty.GetSize(); ++uRecipientSlot)
				{
					ZM_BattleMonster& xRecipient = xRecipientSide.m_xParty.Get(uRecipientSlot);
					if (xRecipient.IsFainted())
					{
						continue;
					}
					const bool bParticipated = uRecipientSlot < 32u
						&& (xDefeated.m_uParticipantMask & (1u << uRecipientSlot)) != 0u;
					if (bParticipated != bParticipantGroup)
					{
						continue;
					}
					ZM_AddEVYield(xRecipient.m_auEV, xYield);
					ZM_ApplyExpGain(xRecipient, bParticipated ? uGross : uHalf,
						m_xEvents, eRecipientSide, uRecipientSlot, uCap);
				}
			}
		}
	}
}

// Evolution remains a terminal level-trigger only. All EXP/LEVEL/MOVE events are
// already complete; this is called directly before each terminal BATTLE_END.
void ZM_BattleEngine::QueueTerminalEvolutions()
{
	if (!m_xConfig.m_bAwardExp)
	{
		return;
	}
	for (u_int uSide = 0u; uSide < (u_int)ZM_SIDE_COUNT; ++uSide)
	{
		const ZM_SIDE eSide = (ZM_SIDE)uSide;
		if (!AwardsExpToSide(eSide))
		{
			continue;
		}
		ZM_BattleSide& xSide = m_xState.Side(eSide);
		for (u_int uSlot = 0u; uSlot < xSide.m_xParty.GetSize(); ++uSlot)
		{
			ZM_BattleMonster& xMon = xSide.m_xParty.Get(uSlot);
			if (!xMon.m_bLevelledThisBattle || xMon.m_bEvolutionQueued)
			{
				continue;
			}
			const ZM_SPECIES_ID eTarget = ZM_CheckEvolveEligible(xMon);
			if (eTarget == ZM_SPECIES_NONE)
			{
				continue;
			}
			xMon.m_bEvolutionQueued = true;
			Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_EVOLUTION_QUEUED, eSide, uSlot,
				ZM_MOVE_NONE, (u_int)eTarget, (int)xMon.m_uLevel, 0));
		}
	}
}

// SC1 screen end-of-turn countdown: PLAYER then ENEMY, PHYSICAL then SPECIAL. A non-zero
// slot ticks; on reaching 0 it announces SCREEN_EXPIRED (owner side, active slot,
// m_iAmount = screen). VEIL has no setter so it never ticks. box 1/2 set no screens, so
// this adds no events and their EOT streams stay byte-identical.
void ZM_BattleEngine::ResolveScreenEndOfTurn()
{
	const ZM_SCREEN aeScreens[2] = { ZM_SCREEN_PHYSICAL, ZM_SCREEN_SPECIAL };
	for (u_int uSideIdx = 0u; uSideIdx < (u_int)ZM_SIDE_COUNT; ++uSideIdx)
	{
		const ZM_SIDE  eSide = (ZM_SIDE)uSideIdx;
		ZM_BattleSide& xSide = m_xState.Side(eSide);
		for (u_int i = 0u; i < 2u; ++i)
		{
			const ZM_SCREEN eScreen = aeScreens[i];
			if (xSide.m_auScreenTurns[eScreen] > 0u)
			{
				--xSide.m_auScreenTurns[eScreen];
				if (xSide.m_auScreenTurns[eScreen] == 0u)
				{
					Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_SCREEN_EXPIRED, eSide, xSide.m_uActiveSlot,
						ZM_MOVE_NONE, ZM_SPECIES_NONE, (int)eScreen, 0, 0));
				}
			}
		}
	}
}
