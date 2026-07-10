#pragma once

#include "Zenithmon/Source/Battle/ZM_BattleState.h"
#include "Zenithmon/Source/Battle/ZM_BattleEvent.h"

// ============================================================================
// ZM_MoveExecutor -- the move-execution seam (S2 box 2, SC1; DecisionLog ZM-D-033).
// The engine hands each acting side to ZM_MoveExecutor::Execute via a thin
// ZM_MoveContext VIEW onto the engine-owned state + event sink (the engine keeps
// owning m_xState / m_xEvents / the RNG). SC1 is a byte-identical extraction of
// ExecuteMove's post-fainted-guard body: PP spend, MOVE_USED, accuracy (with the
// acc/eva stat-stage fold that is identity at stage 0), immunity short-circuit,
// then the single damaging emit-group. ZM_MoveExecutor::ApplyDamagingHit is the
// ONE damage-draw site (crit -> roll -> [CRIT][SUPER|NOT] DAMAGE_DEALT [FAINT]).
// Box 1 only has damaging NONE moves, so the effect switch routes NONE + default
// through ApplyDamagingHit; the ~60-effect body arrives on this seam in SC2+.
// The executor emits NO strings (ZM-D-010): behaviour IS the event stream.
// ============================================================================

struct ZM_MoveData;   // full definition pulled in the .cpp (accuracy/power/type rows)

// A thin view onto the engine's state + event sink for one move execution. Holds
// no storage of its own beyond the target coordinates; every accessor resolves
// live against the pointed-at state so mid-move mutations (HP, PP) are visible.
struct ZM_MoveContext
{
	ZM_BattleState*                m_pxState   = nullptr;   // engine-owned battle state
	Zenith_Vector<ZM_BattleEvent>* m_pxEvents  = nullptr;   // == engine's event sink
	ZM_SIDE                        m_eAtk      = ZM_SIDE_COUNT;
	ZM_SIDE                        m_eDef      = ZM_SIDE_COUNT;
	u_int                          m_uMoveSlot = 0u;         // attacker's pending move slot

	ZM_BattleMonster&       Atk();        // attacker's active monster
	ZM_BattleMonster&       Def();        // defender's active monster
	ZM_BattleSide&          AtkSide();    // attacker side
	ZM_BattleSide&          DefSide();    // defender side
	ZM_BattleRNG&           RNG();        // == m_pxState->m_xRNG (the ONE draw source)
	const ZM_MoveData&      Move() const; // the attacker's pending move row
	void Emit(const ZM_BattleEvent& xEvent);   // PushBack into m_pxEvents
	void MaybeFaint(ZM_SIDE eSide);            // emit FAINT once when that side's active HP is 0
};

namespace ZM_MoveExecutor
{
	// Execute one move: PP spend, MOVE_USED, accuracy, immunity, effect switch
	// (box 1: NONE + default -> ApplyDamagingHit). Assumes the attacker is not
	// fainted (the engine guards that before building the context).
	void Execute(ZM_MoveContext& xCtx);

	// The single damaging emit-group / damage-draw site: crit (RandBelow(24) unless
	// critStage>=2) -> roll (RandRange(85,100)) -> [CRIT][SUPER|NOT] DAMAGE_DEALT
	// [FAINT]. Returns the raw damage rolled (pre-HP-clamp). Precondition: the
	// defender is not type-immune (Execute resolves immunity first).
	u_int ApplyDamagingHit(ZM_MoveContext& xCtx);
}
