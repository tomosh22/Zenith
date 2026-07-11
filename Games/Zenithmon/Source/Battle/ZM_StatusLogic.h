#pragma once

#include "Zenithmon/Source/Battle/ZM_BattleState.h"
#include "Zenithmon/Source/Battle/ZM_BattleEvent.h"

// ============================================================================
// ZM_StatusLogic -- the major-status half of the S2 box-2 status system (SC4;
// DecisionLog ZM-D-033). A namespace of pure-ish functions over the engine-owned
// ZM_MoveContext / ZM_BattleState, emitting into the passed event sink. The six
// MAJOR statuses are mutually exclusive (one m_eStatus slot per monster); the
// volatile half (m_uVolatileMask bits) lands in SC5.
//
// SC4 wires three of the six PHASE-G pre-move gates -- G2 FREEZE, G3 SLEEP,
// G6 PARALYSIS -- in their LOCKED precedence positions (the G1/G4/G5 slots are
// SC5 volatiles, left as clean no-op slots so the draw order never shifts when
// they light). A status-free monster pulls ZERO draws through PreMoveGate and
// EndOfTurn is a no-op for a status-free side, so box-1 / SC1-SC3 goldens stay
// BYTE-IDENTICAL (they never set m_eStatus).
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
	// ZM_MoveExecutor::Execute, BEFORE PP / MOVE_USED. Precedence (SC4 slots):
	//   G2 FREEZE     -> RandBelow(100) < 20 thaw (CureMajor) + proceed; else cancel
	//                    + MOVE_FAILED(FROZEN).                     [1 draw iff frozen]
	//   G3 SLEEP      -> counter-- ; ==0 wake (CureMajor) + proceed; else cancel
	//                    + MOVE_FAILED(ASLEEP).                     [0 draws]
	//   G6 PARALYSIS  -> RandBelow(100) < 25 -> cancel + MOVE_FAILED(FULLY_PARALYZED);
	//                    else proceed.                              [1 draw iff paralyzed]
	// (G1 RECHARGE / G4 FLINCH / G5 CONFUSE are SC5 volatiles -- no-op slots here.)
	// The three majors are mutually exclusive, so at most one gate fires.
	ZM_GATE_RESULT PreMoveGate(ZM_MoveContext& xCtx);

	// Block rule: fails if the monster ALREADY holds any major, or is type-immune to
	// this one -- BURN blocked on FIRE, FREEZE blocked on ICE, POISON/TOXIC blocked on
	// VENOM or IRON; SLEEP / PARALYSIS have NO type block in box 2.
	bool CanApplyMajor(const ZM_BattleMonster& xMon, ZM_MAJOR_STATUS eStatus);

	// Apply eStatus to side eTgt's active monster (if CanApplyMajor). On success sets
	// m_eStatus, seeds the counter (SLEEP = RandRange(1,3) [the ONLY apply-time draw];
	// TOXIC = 1, no draw; others 0), emits STATUS_APPLIED (iAux = ZM_MAJOR_STATUS), and
	// returns true. On block returns false and emits NOTHING (the caller decides whether
	// a primary announces MOVE_FAILED(STATUS_BLOCKED); a damaging secondary is silent).
	bool ApplyMajor(ZM_MoveContext& xCtx, ZM_SIDE eTgt, ZM_MAJOR_STATUS eStatus);

	// Clear side eTgt's active monster's major (STATUS_CURED, iAux = the cured status).
	// Returns false + emits nothing if there was no major. Used by natural wake/thaw
	// and by the CURE_STATUS move.
	bool CureMajor(ZM_MoveContext& xCtx, ZM_SIDE eTgt);

	// End-of-turn major chip for side eSide's active monster (called per side from
	// ResolveEndOfTurnPhase, BEFORE TURN_END): POISON = maxHP/8; TOXIC = maxHP*counter/16
	// then counter++; BURN = maxHP/8 (min-1 so it always ticks). Subtract (clamp 0), emit
	// STATUS_DAMAGE(iAmount = dmg, iAux = remaining HP, m_iTag = source status), then a
	// FAINT iff HP hit 0 (battle-over stays decided in ResolveTurn). A status-free (or
	// fainted) active is a no-op -- emits nothing, so box-1 end-of-turn streams are equal.
	void EndOfTurn(ZM_BattleState& xState, Zenith_Vector<ZM_BattleEvent>& xEvents, ZM_SIDE eSide);
}
