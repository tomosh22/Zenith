#pragma once

#include "Zenithmon/Source/Battle/ZM_BattleState.h"
#include "Zenithmon/Source/Battle/ZM_BattleEvent.h"

// ============================================================================
// ZM_MoveExecutor -- the move-execution seam (S2 box 2, SC1-SC3; DecisionLog ZM-D-033).
// The engine hands each acting side to ZM_MoveExecutor::Execute via a thin
// ZM_MoveContext VIEW onto the engine-owned state + event sink (the engine keeps
// owning m_xState / m_xEvents / the RNG). Execute runs the post-fainted-guard body:
// PP spend, MOVE_USED, the OHKO level guard, accuracy (with the acc/eva stat-stage
// fold that is identity at stage 0), then the category dispatch.
//
// Dispatch (SC2): Move().m_eCategory decides PRIMARY vs SECONDARY. A STATUS-category
// move applies its effect DIRECTLY (chance 100, no crit/roll/immunity/proc draws).
// A damaging move (PHYSICAL/SPECIAL) resolves immunity, runs ApplyDamagingHit (the
// single crit/roll/DAMAGE_DEALT/FAINT emit-group), THEN, if it carries a stat
// secondary and the target survived, draws the E3 secondary proc and applies the
// stat change. NONE-effect damaging moves take the box-1 path unchanged.
//
// Dispatch (SC3): delivery-effect damaging kinds (MULTI_HIT / DOUBLE_HIT / RECOIL /
// DRAIN / FIXED_LEVEL / HALVE_HP / OHKO) route through g_ApplyDelivery -- every hit
// still funnels through ApplyDamagingHit (the ONE crit/roll site). Field/screen/
// hazard STATUS-category setters (WEATHER_* / SCREEN_* / HAZARD_SPIKES) write side/
// field state ONLY (no event, no countdown -- box 3 owns those). ApplyDamagingHit
// activates bScreen at the call site (a non-crit hit into an active matching screen
// halves post-type damage; crit bypasses).
//
// ApplyDamagingHit is the ONE damage-draw site. Its crit rate keys off the MOVE's
// m_uCritStage (>= 2 == guaranteed, no draw) and the attacker's accumulated
// m_iCritStage (RAISE_CRIT): 0 -> 1/24, 1 -> 1/8, 2 -> 1/2, >= 3 -> always. At crit
// stage 0 it is exactly Chance(1,24), so box-1 goldens stay byte-identical.
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
	// Execute one move: PP spend, MOVE_USED, accuracy, then the category dispatch
	// (STATUS-category primary effect | damaging hit + optional stat secondary).
	// Assumes the attacker is not fainted (the engine guards that before building
	// the context).
	void Execute(ZM_MoveContext& xCtx);

	// The single damaging emit-group / damage-draw site: crit -> roll
	// (RandRange(85,100)) -> [CRIT][SUPER|NOT] DAMAGE_DEALT [FAINT]. The crit draw is
	// skipped (guaranteed) when the move's m_uCritStage >= 2 or the attacker's
	// m_iCritStage >= 3; otherwise it is Chance(1,24)/(1,8)/(1,2) by crit stage
	// 0/1/2. Returns the raw damage rolled (pre-HP-clamp). Precondition: the defender
	// is not type-immune (Execute resolves immunity first).
	u_int ApplyDamagingHit(ZM_MoveContext& xCtx);
}
