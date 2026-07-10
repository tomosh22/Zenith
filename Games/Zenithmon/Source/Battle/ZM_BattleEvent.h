#pragma once

#include "Zenithmon/Source/Battle/ZM_BattleTypes.h"
#include "Zenithmon/Source/Data/ZM_MoveData.h"      // ZM_MOVE_NONE
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"   // ZM_SPECIES_NONE

// ============================================================================
// ZM_BattleEvent -- the keystone append-only output POD (S2 box 1). Flat scalar
// fields, no unions/pointers/strings, defaulted operator==. Later boxes only
// APPEND kinds (before ZM_BATTLE_EVENT_COUNT) and APPEND fields (default 0), so
// box-1 goldens never shift. Every event is built through ZM_MakeEvent so the
// engine and the golden vectors construct identically.
// ============================================================================

enum ZM_BATTLE_EVENT : u_int
{
	// ---- BOX 1 (emitted now) ----
	ZM_BATTLE_EVENT_BATTLE_BEGIN,    // ()
	ZM_BATTLE_EVENT_TURN_BEGIN,      // iAux = turn number (1-based)
	ZM_BATTLE_EVENT_TURN_END,        // iAux = turn number
	ZM_BATTLE_EVENT_SWITCH_IN,       // side, slot, speciesId
	ZM_BATTLE_EVENT_MOVE_USED,       // side=ATTACKER, slot, moveId
	ZM_BATTLE_EVENT_MOVE_MISSED,     // side=ATTACKER, slot, moveId
	ZM_BATTLE_EVENT_CRIT,            // side=DEFENDER, slot
	ZM_BATTLE_EVENT_SUPER_EFFECTIVE, // side=DEFENDER, slot, iAmount=effPercent (200/400)
	ZM_BATTLE_EVENT_NOT_EFFECTIVE,   // side=DEFENDER, slot, iAmount=effPercent (25/50)
	ZM_BATTLE_EVENT_IMMUNE,          // side=DEFENDER, slot   (0x; REPLACES DAMAGE_DEALT)
	ZM_BATTLE_EVENT_DAMAGE_DEALT,    // side=DEFENDER, slot, iAmount=RAW damage rolled (pre-clamp; may exceed remaining HP on a KO -- NOT "HP lost"), iAux=remaining HP after clamp
	ZM_BATTLE_EVENT_FAINT,           // side=DEFENDER, slot
	ZM_BATTLE_EVENT_BATTLE_END,      // iAmount = winner side (ZM_SIDE_COUNT == draw)

	// ---- RESERVED (declared now so box-1 ordinals never shift; append-only, ZM-D-009) ----
	ZM_BATTLE_EVENT_NO_PP, ZM_BATTLE_EVENT_MOVE_FAILED,                                    // box 2
	ZM_BATTLE_EVENT_STATUS_APPLIED, ZM_BATTLE_EVENT_STATUS_DAMAGE, ZM_BATTLE_EVENT_STATUS_CURED, // box 2
	ZM_BATTLE_EVENT_STAT_STAGE_CHANGED, ZM_BATTLE_EVENT_VOLATILE_APPLIED, ZM_BATTLE_EVENT_VOLATILE_ENDED, // box 2
	ZM_BATTLE_EVENT_FLINCH, ZM_BATTLE_EVENT_HEAL, ZM_BATTLE_EVENT_DRAIN, ZM_BATTLE_EVENT_RECOIL, ZM_BATTLE_EVENT_MULTI_HIT, // box 2
	ZM_BATTLE_EVENT_ABILITY_TRIGGER, ZM_BATTLE_EVENT_WEATHER_CHANGED, ZM_BATTLE_EVENT_WEATHER_DAMAGE, // box 3
	ZM_BATTLE_EVENT_SCREEN_SET, ZM_BATTLE_EVENT_SCREEN_EXPIRED,                            // box 3
	ZM_BATTLE_EVENT_EXP_GAINED, ZM_BATTLE_EVENT_LEVEL_UP, ZM_BATTLE_EVENT_MOVE_LEARNED, ZM_BATTLE_EVENT_EVOLUTION_QUEUED, // box 4
	ZM_BATTLE_EVENT_CATCH_SHAKE, ZM_BATTLE_EVENT_CATCH_RESULT, ZM_BATTLE_EVENT_FLEE, ZM_BATTLE_EVENT_FLEE_FAILED, // catch/bag
	ZM_BATTLE_EVENT_COUNT
};

struct ZM_BattleEvent
{
	ZM_BATTLE_EVENT m_eKind      = ZM_BATTLE_EVENT_COUNT;
	u_int           m_uSide      = ZM_SIDE_COUNT;    // COUNT == n/a
	u_int           m_uSlot      = 0u;               // party slot of the subject
	u_int           m_uMoveId    = ZM_MOVE_NONE;
	u_int           m_uSpeciesId = ZM_SPECIES_NONE;  // SWITCH_IN readability + doubles-forward
	int             m_iAmount    = 0;                // damage / effPercent / winner side
	int             m_iAux       = 0;                // remaining HP / turn number
	// APPEND future fields HERE (default 0). Box-1 goldens compare equal regardless.
	bool operator==(const ZM_BattleEvent&) const = default;   // trivially value-comparable
};

// Shared factory: engine AND golden vectors build events identically, so appended
// default-0 fields always compare equal on both sides.
inline ZM_BattleEvent ZM_MakeEvent(ZM_BATTLE_EVENT eKind, u_int uSide = ZM_SIDE_COUNT,
	u_int uSlot = 0u, u_int uMoveId = ZM_MOVE_NONE, u_int uSpeciesId = ZM_SPECIES_NONE,
	int iAmount = 0, int iAux = 0)
{
	ZM_BattleEvent x; x.m_eKind = eKind; x.m_uSide = uSide; x.m_uSlot = uSlot;
	x.m_uMoveId = uMoveId; x.m_uSpeciesId = uSpeciesId; x.m_iAmount = iAmount; x.m_iAux = iAux;
	return x;
}
