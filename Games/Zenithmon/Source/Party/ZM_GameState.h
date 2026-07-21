#pragma once

#include "Zenithmon/Source/Party/ZM_Party.h"
#include "Zenithmon/Source/Party/ZM_BoxStorage.h"
#include "Zenithmon/Source/Party/ZM_Bag.h"           // ZM_Bag (S6 item 2 SC3)
#include "Zenithmon/Source/Battle/ZM_BattleTower.h" // ZM_TowerRun
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"   // ZM_SPECIES_ID, ZM_SPECIES_COUNT

// ============================================================================
// ZM_GameState -- the complete durable in-memory player model consumed by S7's
// save modules: party, boxes, dex, flags, badges, bag, money, daycare, tower,
// world resume position, and options. This file still performs no serialization
// or I/O. The pending-whiteout latch remains transient runtime coordination state.
// ============================================================================

// A per-species boolean set (seen or caught), indexed by ZM_SPECIES_ID. Backed by a
// fixed bool array sized to the dex; ZM_SPECIES_NONE (== ZM_SPECIES_COUNT) and any
// out-of-range id are ignored, so callers never index out of bounds.
struct ZM_SpeciesSet
{
	bool m_abFlags[ZM_SPECIES_COUNT] = {};

	void Mark(ZM_SPECIES_ID eSpecies)
	{
		if ((u_int)eSpecies < (u_int)ZM_SPECIES_COUNT) { m_abFlags[(u_int)eSpecies] = true; }
	}
	bool IsSet(ZM_SPECIES_ID eSpecies) const
	{
		return (u_int)eSpecies < (u_int)ZM_SPECIES_COUNT && m_abFlags[(u_int)eSpecies];
	}
	u_int Count() const
	{
		u_int uCount = 0u;
		for (u_int u = 0u; u < (u_int)ZM_SPECIES_COUNT; ++u)
		{
			if (m_abFlags[u]) { ++uCount; }
		}
		return uCount;
	}
};

// Fixed-capacity story flags. The model reserves the SaveFormat sanity ceiling
// up front; story content assigns stable indices within this range.
static const u_int uZM_MAX_STORY_FLAGS = 4096u;
static const u_int uZM_STORY_FLAG_BYTE_COUNT = uZM_MAX_STORY_FLAGS / 8u;

struct ZM_StoryFlagSet
{
	u_int8 m_auFlags[uZM_STORY_FLAG_BYTE_COUNT] = {};

	bool Set(u_int uIndex, bool bSet)
	{
		if (uIndex >= uZM_MAX_STORY_FLAGS) { return false; }
		const u_int uByte = uIndex / 8u;
		const u_int8 uMask = (u_int8)(1u << (uIndex % 8u));
		if (bSet) { m_auFlags[uByte] = (u_int8)(m_auFlags[uByte] | uMask); }
		else      { m_auFlags[uByte] = (u_int8)(m_auFlags[uByte] & (u_int8)~uMask); }
		return true;
	}

	bool IsSet(u_int uIndex) const
	{
		if (uIndex >= uZM_MAX_STORY_FLAGS) { return false; }
		const u_int8 uMask = (u_int8)(1u << (uIndex % 8u));
		return (m_auFlags[uIndex / 8u] & uMask) != 0u;
	}

	u_int Count() const
	{
		u_int uCount = 0u;
		for (u_int uIndex = 0u; uIndex < uZM_MAX_STORY_FLAGS; ++uIndex)
		{
			if (IsSet(uIndex)) { ++uCount; }
		}
		return uCount;
	}
};

static const u_int uZM_BADGE_COUNT = 8u;

// The durable daycare inventory deliberately owns full ZM_Monster records.
// Hatch progress belongs to the daycare aggregate, never to an individual egg.
static const u_int uZM_DAYCARE_PARENT_CAPACITY = 2u;
struct ZM_DaycareProgress
{
	u_int      m_uParentCount = 0u;
	ZM_Monster m_axParents[uZM_DAYCARE_PARENT_CAPACITY];
	bool       m_bEggPresent = false;
	ZM_Monster m_xEgg;
	u_int      m_uEggStepsRemaining = 0u;
};

static const u_int uZM_WORLD_SCENE_UNSET = 0xffffffffu;
static const u_int uZM_WORLD_SPAWN_TAG_CAPACITY = 32u;
struct ZM_WorldPosition
{
	u_int m_uSceneBuildIndex = uZM_WORLD_SCENE_UNSET;
	char  m_szSpawnTag[uZM_WORLD_SPAWN_TAG_CAPACITY] = {};
	float m_afPosition[3] = { 0.0f, 0.0f, 0.0f };
	float m_fYaw = 0.0f;
};

enum ZM_TEXT_SPEED : u_int
{
	ZM_TEXT_SPEED_SLOW,
	ZM_TEXT_SPEED_NORMAL,
	ZM_TEXT_SPEED_FAST,
	ZM_TEXT_SPEED_COUNT
};

struct ZM_GameOptions
{
	ZM_TEXT_SPEED m_eTextSpeed = ZM_TEXT_SPEED_NORMAL;
};

// The money ceiling (SaveFormat module 7 is a uint32, so this is a gameplay cap,
// not a storage invariant). Full-width imported values above it are retained;
// while over cap, credits are a no-op rather than a clamp or wrap.
static constexpr u_int uZM_MONEY_CAP = 999999u;

struct ZM_GameState
{
	ZM_Party            m_xParty;
	ZM_BoxStorage       m_xBoxes;
	ZM_SpeciesSet       m_xSeen;
	ZM_SpeciesSet       m_xCaught;
	ZM_StoryFlagSet     m_xStoryFlags;
	u_int8              m_uBadgeMask = 0u;
	ZM_Bag              m_xBag;
	u_int               m_uMoney = 0u;                // full SaveFormat module-7 uint32
	ZM_DaycareProgress  m_xDaycare;
	ZM_TowerRun         m_xTowerRun;
	ZM_WorldPosition    m_xWorldPosition;
	ZM_GameOptions      m_xOptions;
	bool                m_bPendingWhiteout = false;   // transient; consumed by the whiteout warp, never saved

	void  MarkSeen(ZM_SPECIES_ID eSpecies)         { m_xSeen.Mark(eSpecies); }
	bool  IsSeen(ZM_SPECIES_ID eSpecies) const     { return m_xSeen.IsSet(eSpecies); }
	u_int GetSeenCount() const                     { return m_xSeen.Count(); }
	void  MarkCaught(ZM_SPECIES_ID eSpecies)       { m_xSeen.Mark(eSpecies); m_xCaught.Mark(eSpecies); }
	bool  IsCaught(ZM_SPECIES_ID eSpecies) const   { return m_xCaught.IsSet(eSpecies); }
	u_int GetCaughtCount() const                   { return m_xCaught.Count(); }

	bool AwardBadge(u_int uIndex)
	{
		if (uIndex >= uZM_BADGE_COUNT) { return false; }
		m_uBadgeMask = (u_int8)(m_uBadgeMask | (u_int8)(1u << uIndex));
		return true;
	}
	bool HasBadge(u_int uIndex) const
	{
		return uIndex < uZM_BADGE_COUNT && (m_uBadgeMask & (u_int8)(1u << uIndex)) != 0u;
	}
	u_int GetBadgeCount() const
	{
		u_int uCount = 0u;
		for (u_int uIndex = 0u; uIndex < uZM_BADGE_COUNT; ++uIndex)
		{
			if (HasBadge(uIndex)) { ++uCount; }
		}
		return uCount;
	}

	// Saturating credit. Headroom-first: m_uMoney + uAmount could wrap a u_int, so
	// the clamp is computed BEFORE the addition, never after. The headroom subtraction
	// is itself guarded because m_uMoney is a public field with no write invariant --
	// S7's loader assigns it straight from the module-7 uint32, where an edited save
	// can carry a value above the cap. An over-cap balance credits nothing (a no-op)
	// rather than underflowing the headroom and wrapping the purse.
	void AddMoney(u_int uAmount)
	{
		const u_int uHeadroom = (m_uMoney < uZM_MONEY_CAP) ? (uZM_MONEY_CAP - m_uMoney) : 0u;
		m_uMoney += (uAmount < uHeadroom) ? uAmount : uHeadroom;
	}
	// Debit. False with NO mutation when the balance cannot cover it (the guard the
	// SC7 shop transaction leans on -- money is only ever spent all-or-nothing).
	bool SpendMoney(u_int uAmount)
	{
		if (m_uMoney < uAmount) { return false; }
		m_uMoney -= uAmount;
		return true;
	}
};

// The fixed starter GameState (D4): a single Fernfawn L5 party lead, its species
// marked seen+caught, no pending whiteout, the starting economy, and default-empty
// values for every other durable module. Deterministic (no RNG).
ZM_GameState ZM_MakeStarterGameState();
