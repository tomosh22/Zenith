#pragma once

#include "Zenithmon/Source/Data/ZM_ItemData.h"   // ZM_ITEM_ID, ZM_ITEM_NONE

// ============================================================================
// ZM_BattleTypes -- leaf enums / constants / action / config for the S2 battle
// engine (box 1). Pure data declarations, no logic. Depends only on the item
// data table (for ZM_ITEM_ID in ZM_BattleAction). Everything reserved for later
// boxes (status / volatiles / weather / screens) is declared now so the ordinals
// never shift and box-1 goldens stay stable (append-only, ZM-D-009).
// ============================================================================

static const u_int uZM_MAX_PARTY_SIZE = 6u;
static const u_int uZM_MAX_MOVES      = 4u;
static const int   iZM_MIN_STAGE      = -6;
static const int   iZM_MAX_STAGE      =  6;

// The two battling sides. COUNT doubles as "none / draw".
enum ZM_SIDE : u_int { ZM_SIDE_PLAYER, ZM_SIDE_ENEMY, ZM_SIDE_COUNT };

// Major status persists on the monster (box 2 drives; box 1 is always NONE).
enum ZM_MAJOR_STATUS : u_int
{
	ZM_MAJOR_STATUS_NONE, ZM_MAJOR_STATUS_SLEEP, ZM_MAJOR_STATUS_POISON,
	ZM_MAJOR_STATUS_TOXIC, ZM_MAJOR_STATUS_BURN, ZM_MAJOR_STATUS_PARALYSIS,
	ZM_MAJOR_STATUS_FREEZE, ZM_MAJOR_STATUS_COUNT
};

// Battle-local volatiles bitmask (box 2 sets; box 1 mask == 0 -- reserved).
enum ZM_VOLATILE : u_int
{
	ZM_VOLATILE_NONE       = 0u,        ZM_VOLATILE_CONFUSED   = 1u<<0,
	ZM_VOLATILE_FLINCH     = 1u<<1,     ZM_VOLATILE_LEECH_SEED = 1u<<2,
	ZM_VOLATILE_PROTECT    = 1u<<3,     ZM_VOLATILE_CHARGE     = 1u<<4,
	ZM_VOLATILE_SEMI_INVULN= 1u<<5,     ZM_VOLATILE_RECHARGE   = 1u<<6,
	ZM_VOLATILE_LOCK       = 1u<<7,     ZM_VOLATILE_TRAP       = 1u<<8,
	ZM_VOLATILE_TAUNT      = 1u<<9
};

// Stat-stage slots: HP excluded by construction; +accuracy/evasion for box 2.
enum ZM_BATTLE_STAT : u_int
{
	ZM_BATTLE_STAT_ATTACK, ZM_BATTLE_STAT_DEFENSE, ZM_BATTLE_STAT_SPATTACK,
	ZM_BATTLE_STAT_SPDEFENSE, ZM_BATTLE_STAT_SPEED, ZM_BATTLE_STAT_ACCURACY,
	ZM_BATTLE_STAT_EVASION, ZM_BATTLE_STAT_COUNT
};

enum ZM_WEATHER : u_int { ZM_WEATHER_NONE, ZM_WEATHER_RAIN, ZM_WEATHER_SUN,
                          ZM_WEATHER_SAND, ZM_WEATHER_SNOW, ZM_WEATHER_COUNT }; // reserved box 3
enum ZM_SCREEN  : u_int { ZM_SCREEN_PHYSICAL, ZM_SCREEN_SPECIAL, ZM_SCREEN_VEIL,
                          ZM_SCREEN_COUNT };                                    // reserved box 3

enum ZM_ACTION_KIND : u_int { ZM_ACTION_MOVE, ZM_ACTION_SWITCH, ZM_ACTION_ITEM,
                              ZM_ACTION_RUN, ZM_ACTION_NONE };

struct ZM_BattleAction   // union-free; only the field(s) for m_eKind are read
{
	ZM_ACTION_KIND m_eKind       = ZM_ACTION_NONE;
	u_int          m_uMoveSlot   = 0u;              // MOVE   : 0..uZM_MAX_MOVES-1
	u_int          m_uSwitchSlot = 0u;              // SWITCH : party index          (box 2)
	ZM_ITEM_ID     m_eItem       = ZM_ITEM_NONE;    // ITEM   : ball/medicine         (box 2/catch)
};

struct ZM_BattleConfig
{
	bool  m_bIsWild   = false;
	bool  m_bCanCatch = false;
	bool  m_bCanFlee  = false;
	u_int m_uLevelCap = 0u;   // 0 == none; box 6 Battle Tower sets 50
	// reserved (box 5): ZM_AI_TIER m_aeSideAI[ZM_SIDE_COUNT] = { NONE, NONE };
};
