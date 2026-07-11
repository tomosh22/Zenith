#pragma once

#include "Zenithmon/Source/Battle/ZM_BattleState.h"
#include "Zenithmon/Source/Battle/ZM_BattleEvent.h"

// ============================================================================
// ZM_StatusLogic -- the major + volatile S2 box-2 status system (SC4-SC5;
// DecisionLog ZM-D-033). A namespace of pure-ish functions over the engine-owned
// ZM_MoveContext / ZM_BattleState, emitting into the passed event sink. The six
// MAJOR statuses are mutually exclusive (one m_eStatus slot per monster); the
// volatile half uses the exact ten m_uVolatileMask bits. ENDURE is deliberately
// a separate one-turn bool because it is not one of the GDD's ten volatiles.
//
// SC4 wired G2 FREEZE, G3 SLEEP, and G6 PARALYSIS; SC5 fills G1 RECHARGE,
// G4 FLINCH, and G5 CONFUSE without changing their LOCKED precedence. A
// status/volatile-free monster pulls ZERO draws through PreMoveGate and
// EndOfTurn is a no-op for an unaffected side, so box-1 / SC1-SC3 goldens stay
// BYTE-IDENTICAL (they never set m_eStatus or m_uVolatileMask).
//
// Counter discipline (LOCKED, ZM-D-033 risk #6): SLEEP is DECREMENT-THEN-CHECK
// in the gate (counter-- ; ==0 wakes and proceeds), so a counter of N means
// N-1 turns of forced sleep. TOXIC ramps in EndOfTurn: chip uses the counter,
// THEN increments (1/16, 2/16, 3/16, ...). The two never coexist (one major at
// a time), so both ride the shared m_uStatusCounter.
// ============================================================================

struct ZM_MoveContext;   // full definition in ZM_MoveExecutor.h (pulled in the .cpp)

// A pre-move gate either lets the move proceed or cancels it (spending NO PP and
// emitting NO MOVE_USED -- ZM-D-033 Q3). The cancel is decided in PHASE G, before
// ZM_MoveExecutor::Execute spends PP / announces MOVE_USED.
enum ZM_GATE_RESULT : u_int { ZM_GATE_PROCEED, ZM_GATE_CANCELLED };

namespace ZM_StatusLogic
{
	// PHASE G pre-move gates for the acting (attacker) monster. Runs at the TOP of
	// ZM_MoveExecutor::Execute, BEFORE PP / MOVE_USED. Precedence:
	//   G1 RECHARGE   -> cancel + end debt.                         [0 draws]
	//   G2 FREEZE     -> RandBelow(100) < 20 thaw (CureMajor) + proceed; else cancel
	//                    + MOVE_FAILED(FROZEN).                     [1 draw iff frozen]
	//   G3 SLEEP      -> counter-- ; ==0 wake (CureMajor) + proceed; else cancel
	//                    + MOVE_FAILED(ASLEEP).                     [0 draws]
	//   G4 FLINCH     -> FLINCH + end volatile + cancel.            [0 draws]
	//   G5 CONFUSE    -> gate; self-hit uses one 85..100 roll.      [1 or 2 draws]
	//   G6 PARALYSIS  -> RandBelow(100) < 25 -> cancel + MOVE_FAILED(FULLY_PARALYZED);
	//                    else proceed.                              [1 draw iff paralyzed]
	// The first cancelling gate stops every later gate.
	ZM_GATE_RESULT PreMoveGate(ZM_MoveContext& xCtx);

	// Shared stat-stage mutation used by moves and ability hooks. Clamps to
	// [-6,+6], emits the actual applied delta, and reports STAT_MAXED for a
	// capped primary. bFromFoe arms the box-3 ability-veto seam; a successful
	// veto emits ABILITY_TRIGGER and applies no stage change.
	bool ApplyStatChange(ZM_BattleState& xState, Zenith_Vector<ZM_BattleEvent>& xEvents,
		ZM_SIDE eTgt, ZM_BATTLE_STAT eStat, int iDelta, bool bPrimary, bool bFromFoe);

	// Block rule: fails if the monster ALREADY holds any major, or is type-immune to
	// this one -- BURN blocked on FIRE, FREEZE blocked on ICE, POISON/TOXIC blocked on
	// VENOM or IRON; SLEEP / PARALYSIS have NO type block in box 2.
	bool CanApplyMajor(const ZM_BattleMonster& xMon, ZM_MAJOR_STATUS eStatus);

	// Apply eStatus to side eTgt's active monster (if CanApplyMajor). On success sets
	// m_eStatus, seeds the counter (SLEEP = RandRange(1,3) [the ONLY apply-time draw];
	// TOXIC = 1, no draw; others 0), emits STATUS_APPLIED (iAux = ZM_MAJOR_STATUS), and
	// returns true. On block returns false and emits NOTHING (the caller decides whether
	// a primary announces MOVE_FAILED(STATUS_BLOCKED); a damaging secondary is silent).
	//
	// The state+events overload is the core: SC4 ability bodies (e.g. contact procs)
	// status the attacker from a ZM_AbilityContext without a ZM_MoveContext. It also
	// hosts the SC4 STATUS_TRY veto -- after CanApplyMajor, a target's pfnPreventMajor
	// may block the status, emitting ONE ABILITY_TRIGGER (m_iAmount = blocked status)
	// and applying nothing (no apply-time draw). The ZM_MoveContext overload forwards.
	bool ApplyMajor(ZM_BattleState& xState, Zenith_Vector<ZM_BattleEvent>& xEvents,
		ZM_SIDE eTgt, ZM_MAJOR_STATUS eStatus);
	bool ApplyMajor(ZM_MoveContext& xCtx, ZM_SIDE eTgt, ZM_MAJOR_STATUS eStatus);

	// Clear side eTgt's active monster's major (STATUS_CURED, iAux = the cured status).
	// Returns false + emits nothing if there was no major. Used by natural wake/thaw
	// and by the CURE_STATUS move.
	bool CureMajor(ZM_MoveContext& xCtx, ZM_SIDE eTgt);

	// Centralized volatile lifecycle. ApplyVolatile owns every guarded duration
	// draw and emits VOLATILE_APPLIED. EndVolatile clears the bit plus its matching
	// counter/metadata and emits VOLATILE_ENDED. Duplicate/blocked operations are
	// false and silent except PROTECT, whose duplicate application refreshes it and
	// emits a fresh APPLIED event. The move executor decides whether a primary
	// reports MOVE_FAILED. LEECH_SEED additionally rejects GRASS targets.
	bool CanApplyVolatile(const ZM_BattleMonster& xMon, ZM_VOLATILE eVolatile);
	bool ApplyVolatile(ZM_MoveContext& xCtx, ZM_SIDE eTgt, ZM_VOLATILE eVolatile,
		ZM_SIDE eSource = ZM_SIDE_COUNT);
	bool EndVolatile(ZM_BattleState& xState, Zenith_Vector<ZM_BattleEvent>& xEvents,
		ZM_SIDE eSide, ZM_VOLATILE eVolatile);

	// Silent switch-out reset. Preserves HP/PP and the major status+counter, but
	// clears all battle-local volatiles, their metadata, stages, crit and Endure.
	void ResetSwitchTransients(ZM_BattleMonster& xMon);

	// End-of-turn fixed per-side order: major chip -> leech -> trap -> cleanup.
	// Cleanup clears FLINCH, PROTECT, Endure and decrements TAUNT even after a KO;
	// a KO from an earlier chip suppresses later chip sources. TRAP duration counts
	// EOT chips (including its application turn). CONFUSED/LOCK are action-based.
	void EndOfTurn(ZM_BattleState& xState, Zenith_Vector<ZM_BattleEvent>& xEvents, ZM_SIDE eSide);
}
