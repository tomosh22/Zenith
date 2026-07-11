#pragma once

#include "Zenithmon/Source/Battle/ZM_BattleMonster.h"
#include "Zenithmon/Source/Data/ZM_BattleRNG.h"
#include "Collections/Zenith_Vector.h"

// ============================================================================
// ZM_BattleState -- sides, field, and RNG (S2 box 1). The battle DEEP-OWNS its
// monsters by value: a battle is a pure function of (specs, config, seed), so it
// replays bit-exact, clones for AI look-ahead, and saves mid-battle. The RNG
// LIVES IN STATE (its cursor must snapshot/clone with everything else). The
// append-only event stream lives in the engine (output, not state).
// ============================================================================

struct ZM_BattleSide
{
	Zenith_Vector<ZM_BattleMonster> m_xParty;   // deep-owned; box-1 size 1, vector for switch/doubles
	u_int m_uActiveSlot = 0u;
	// reserved side conditions (box 3), inert/zero in box 1:
	u_int m_auScreenTurns[ZM_SCREEN_COUNT] = { 0u, 0u, 0u };
	u_int m_uHazardSpikeLayers = 0u;

	ZM_BattleMonster&       Active()       { return m_xParty.Get(m_uActiveSlot); }
	const ZM_BattleMonster& Active() const { return m_xParty.Get(m_uActiveSlot); }
	bool HasUnfainted() const;   // index-based loop, no STL iterators
	bool CanSwitchTo(u_int uSlot) const;
	u_int FindLowestSwitchTarget() const;   // uZM_MAX_PARTY_SIZE == none
};

struct ZM_FieldState
{
	ZM_WEATHER m_eWeather      = ZM_WEATHER_NONE;  // reserved box 3
	u_int      m_uWeatherTurns = 0u;               // reserved box 3
	u_int      m_uTurnCounter  = 0u;               // ++ each ResolveTurn; first turn == 1
};

struct ZM_BattleState
{
	ZM_BattleSide m_axSides[ZM_SIDE_COUNT];
	ZM_FieldState m_xField;
	ZM_BattleRNG  m_xRNG;                          // RNG LIVES IN STATE (snapshot/clone/replay)

	ZM_BattleSide&       Side(ZM_SIDE e)       { return m_axSides[e]; }
	const ZM_BattleSide& Side(ZM_SIDE e) const { return m_axSides[e]; }
};
