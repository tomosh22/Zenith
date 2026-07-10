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

	// Record a side's action for the pending turn. Asserts legality (box 1: kind==MOVE,
	// slot in range, PP>0, active not fainted). Overwrites a prior submit for that side.
	void SubmitAction(ZM_SIDE eSide, const ZM_BattleAction& xAction);

	// Both sides must have submitted. Resolves the whole turn, appending events. No-op once over.
	void ResolveTurn();

	bool    IsOver() const        { return m_bOver; }
	ZM_SIDE GetWinnerSide() const { return m_eWinner; }   // valid iff IsOver(); COUNT == draw

	const Zenith_Vector<ZM_BattleEvent>& GetEvents() const { return m_xEvents; }
	u_int                 GetEventCount()          const { return m_xEvents.GetSize(); }
	const ZM_BattleEvent& GetEvent(u_int uIndex)   const { return m_xEvents.Get(uIndex); }
	const ZM_BattleState& GetState()               const { return m_xState; }
	ZM_BattleState&       GetStateMutable()              { return m_xState; }   // tests rig HP/stages pre-turn

private:
	ZM_BattleState                m_xState;
	Zenith_Vector<ZM_BattleEvent> m_xEvents;                 // append-only output
	ZM_BattleAction               m_axPending[ZM_SIDE_COUNT];
	bool                          m_abSubmitted[ZM_SIDE_COUNT] = { false, false };
	ZM_BattleConfig               m_xConfig;
	bool                          m_bOver   = false;
	ZM_SIDE                       m_eWinner = ZM_SIDE_COUNT;

	// seams -- each phase is a hook point:
	void ResolvePreMovePhase();     // box 1: EMPTY (run/item/switch -> box 2)
	void ResolveMovePhase();        // order by priority -> eff speed -> RNG tie-break; execute
	void ExecuteMove(ZM_SIDE eAtk); // THE executor seam: box 1 = damaging path; box 2 = ZM_MoveExecutor
	void ResolveEndOfTurnPhase();   // box 1: emit TURN_END only. box 2/3: status/weather/leech ticks
	bool CheckFaintAndMaybeEnd(ZM_SIDE eDefender); // emit FAINT; report whether that side is wiped
	void Emit(const ZM_BattleEvent& x) { m_xEvents.PushBack(x); }
};
