#pragma once

#include "Zenithmon/Source/Battle/ZM_BattleState.h"
#include "Zenithmon/Source/Battle/ZM_BattleEvent.h"

// ============================================================================
// ZM_AbilityHooks -- the S2 box-3 function-pointer dispatch surface. The
// metadata table's ZM_ABILITY_HOOK mask remains the declaration/coverage record;
// this parallel table carries the executable hooks keyed by ZM_ABILITY_ID.
// WEATHER has no dedicated pfn: it is a condition read by weather-coupled hook
// bodies. Some MODIFY_STAT behavior is likewise engine-owned (see ZM-D-036).
// ============================================================================

struct ZM_MoveData;

// A live view over the engine-owned state and append-only event sink. Dispatch
// sites construct this on the stack; hook bodies never own battle state.
struct ZM_AbilityContext
{
	ZM_BattleState*				m_pxState  = nullptr;
	Zenith_Vector<ZM_BattleEvent>*	m_pxEvents = nullptr;
	ZM_SIDE					m_eSelf    = ZM_SIDE_COUNT;
	ZM_SIDE					m_eOther   = ZM_SIDE_COUNT;
	// Attacker's used-move slot. Defaulted at every dispatch site; set ONLY at the
	// E4 contact seam so Lastspite can sap the KO'ing attacker's used move. This is a
	// transient stack view, NOT the serialized ZM_BattleEvent POD, so the addition is
	// save-format-neutral and does not change sizeof(ZM_AbilityHooks).
	u_int					m_uOtherMoveSlot = uZM_MAX_MOVES;

	ZM_BattleMonster& SelfMon() const { return m_pxState->Side(m_eSelf).Active(); }
	ZM_BattleMonster& OtherMon() const { return m_pxState->Side(m_eOther).Active(); }
	ZM_BattleSide& SelfSide() const { return m_pxState->Side(m_eSelf); }
	ZM_BattleSide& OtherSide() const { return m_pxState->Side(m_eOther); }
	ZM_BattleRNG& RNG() const { return m_pxState->m_xRNG; }
	void Emit(const ZM_BattleEvent& xEvent) const { m_pxEvents->PushBack(xEvent); }
};

// Twelve executable slots realize the eleven metadata bits. MODIFY_STAT and
// ACCURACY each have two distinct signatures; WEATHER is engine-side only.
// Slots stay null until their ordered box-3 sub-commit supplies the behavior.
struct ZM_AbilityHooks
{
	void  (*pfnOnSwitchIn)(ZM_AbilityContext&) = nullptr;
	u_int (*pfnModifyStat)(const ZM_AbilityContext&, ZM_BATTLE_STAT, u_int uValue) = nullptr;
	bool  (*pfnPreventStatDrop)(const ZM_AbilityContext&, ZM_BATTLE_STAT) = nullptr;
	u_int (*pfnModifyDamageDealt)(const ZM_AbilityContext&, const ZM_MoveData&, u_int uDamage) = nullptr;
	u_int (*pfnModifyDamageTaken)(const ZM_AbilityContext&, const ZM_MoveData&, u_int uEffectivenessPercent,
		u_int uDamage) = nullptr;
	bool  (*pfnPreventMajor)(ZM_MAJOR_STATUS) = nullptr;
	bool  (*pfnPreventVolatile)(ZM_VOLATILE) = nullptr;
	void  (*pfnOnContact)(ZM_AbilityContext&, bool bSelfFainted) = nullptr;
	void  (*pfnOnTurnEnd)(ZM_AbilityContext&) = nullptr;
	void  (*pfnOnDealtFaint)(ZM_AbilityContext&) = nullptr;
	bool  (*pfnBypassAccuracy)(const ZM_AbilityContext&) = nullptr;
	int   (*pfnTypeInteraction)(const ZM_AbilityContext&, ZM_TYPE eMoveType) = nullptr;
};

// Returns the stable table row for a real ability, or nullptr for NONE/out of
// range. A real row may contain only null slots until its ordered SC lands.
const ZM_AbilityHooks* ZM_GetAbilityHooks(ZM_ABILITY_ID eId);
