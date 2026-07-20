#pragma once

#include "Zenithmon/Source/Battle/ZM_BattleState.h"
#include "Zenithmon/Source/Battle/ZM_BattleEvent.h"

// ============================================================================
// ZM_BattleEngine -- the public battle API (S2 box 1). Deep-owns both parties,
// seeds the state RNG, resolves turns into an append-only event stream. Box 1 is
// the damaging-move keystone: priority/speed ordering, the pure Gen-V damage
// pipeline, faint + battle-over. The ~60-effect executor, status/ability/weather/
// exp/AI all bolt onto the phase-hook seams in later boxes with no API change.
// ============================================================================

class ZM_BattleEngine
{
public:
	// Builds + deep-owns both parties, seeds the state RNG, sets active slot 0, and emits
	// BATTLE_BEGIN then SWITCH_IN(player) then SWITCH_IN(enemy).
	void Begin(const ZM_BattleConfig& xConfig,
	           const ZM_BattleMonsterSpec* paxPlayer, u_int uPlayerCount,
	           const ZM_BattleMonsterSpec* paxEnemy,  u_int uEnemyCount,
	           u_int64 ulSeed, u_int64 ulSeq = 54ull);

	// Record a side's action for the pending turn. Asserts legality: MOVE (slot in
	// range, PP>0, active not fainted) or, from SC6, a pre-move SWITCH / ITEM (ball,
	// wild only) / RUN (wild only) on a non-charged/locked active. Overwrites a prior
	// submit for that side.
	void SubmitAction(ZM_SIDE eSide, const ZM_BattleAction& xAction);

	// Both sides must have submitted. Resolves the whole turn, appending events. No-op once over.
	void ResolveTurn();

	bool    IsOver() const        { return m_bOver; }
	ZM_SIDE GetWinnerSide() const { return m_eWinner; }   // valid iff IsOver(); COUNT == draw

	const Zenith_Vector<ZM_BattleEvent>& GetEvents() const { return m_xEvents; }
	u_int                 GetEventCount()          const { return m_xEvents.GetSize(); }
	const ZM_BattleEvent& GetEvent(u_int uIndex)   const { return m_xEvents.Get(uIndex); }
	const ZM_BattleState& GetState()               const { return m_xState; }
	// The config this battle was Begun with. Read-only, and the ONE source of the
	// battle's rules: presentation gates on it rather than keeping its own copy, so a
	// UI that offers an action the engine would assert on cannot exist (the wild-only
	// catch is exactly that case -- see SubmitAction's m_bCanCatch assert).
	const ZM_BattleConfig& GetConfig()             const { return m_xConfig; }
	ZM_BattleState&       GetStateMutable()              { return m_xState; }   // tests rig HP/stages pre-turn

	// Shared switch primitive (SC5 forced switch; SC6 voluntary switch reuses it).
	// Invalid/current/fainted destinations return false and emit nothing.
	bool DoSwitch(ZM_SIDE eSide, u_int uTargetSlot);

private:
	ZM_BattleState                m_xState;
	Zenith_Vector<ZM_BattleEvent> m_xEvents;                 // append-only output
	ZM_BattleAction               m_axPending[ZM_SIDE_COUNT];
	bool                          m_abSubmitted[ZM_SIDE_COUNT] = { false, false };
	ZM_BattleConfig               m_xConfig;
	bool                          m_bOver   = false;
	ZM_SIDE                       m_eWinner = ZM_SIDE_COUNT;
	u_int                         m_auFleeAttempts[ZM_SIDE_COUNT] = { 0u, 0u };   // SC6 wild-run counters

	// seams -- each phase is a hook point:
	void ResolvePreMovePhase();     // SC6: voluntary SWITCH/ITEM(catch)/RUN; may set m_bOver (flee/catch)
	void ResolveMovePhase();        // order MOVE-action sides by priority -> eff speed -> RNG tie-break; execute
	ZM_SIDE ExecuteMove(ZM_SIDE eAtk); // returns the side force-switched, or COUNT
	void ResolveEndOfTurnPhase();   // box 1: emit TURN_END only. box 2/3: status/weather/leech ticks
	void ResolveWeatherEndOfTurn(); // SC1: SAND/SNOW chip (PLAYER then ENEMY) + weather countdown/expiry
	void ResolveScreenEndOfTurn();  // SC1: screen countdown/expiry (PLAYER then ENEMY, PHYS then SPEC)
	void MarkCurrentParticipants(); // box 4: per-opponent opposing-active ledger; strict no-op when awards are off
	bool AwardsExpToSide(ZM_SIDE eSide) const;
	void AwardExpForNewFaints();    // box 4: scan every party member and credit each new faint exactly once
	void QueueTerminalEvolutions(); // box 4: immediately-before-BATTLE_END settlement only
	void Emit(const ZM_BattleEvent& x) { m_xEvents.PushBack(x); }

	// SC6 pre-move action handlers (fixed PLAYER-then-ENEMY order). A catch/flee sets
	// m_bOver + m_eWinner; the engine closes the turn (TURN_END + BATTLE_END).
	void DoVoluntarySwitch(ZM_SIDE eSide, u_int uTargetSlot);
	void DoItemAction(ZM_SIDE eSide, ZM_ITEM_ID eItem);   // ball == capture attempt on the opponent
	void DoRunAction(ZM_SIDE eSide);
};
