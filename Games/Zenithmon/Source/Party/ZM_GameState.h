#pragma once

#include "Zenithmon/Source/Party/ZM_Party.h"
#include "Zenithmon/Source/Party/ZM_Bag.h"           // ZM_Bag (S6 item 2 SC3)
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"   // ZM_SPECIES_ID, ZM_SPECIES_COUNT

// ============================================================================
// ZM_GameState -- the minimal persistent player state (S5 item 5 SC1): the party
// plus a caught-species set plus a pending-whiteout latch, and (S6 item 2 SC3) the
// bag and money balance. A plain in-memory aggregate; it is owned by the persistent
// ZM_GameStateManager (SC2) and lives for the session only (no disk save -- D3; S7
// owns serialization). No ECS, no graphics, no I/O in this file.
// ============================================================================

// A per-species boolean set (caught / dex), indexed by ZM_SPECIES_ID. Backed by a
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

// The money ceiling (SaveFormat module 7 is a uint32, so this is a gameplay cap,
// not a storage one). Earnings saturate here rather than wrapping.
static constexpr u_int uZM_MONEY_CAP = 999999u;

struct ZM_GameState
{
	ZM_Party      m_xParty;
	ZM_SpeciesSet m_xCaught;
	ZM_Bag        m_xBag;
	u_int         m_uMoney = 0u;                // mirrors SaveFormat module 7 (uint32)
	bool          m_bPendingWhiteout = false;   // set on a battle loss (SC5); consumed by the whiteout warp

	void  MarkCaught(ZM_SPECIES_ID eSpecies)       { m_xCaught.Mark(eSpecies); }
	bool  IsCaught(ZM_SPECIES_ID eSpecies) const   { return m_xCaught.IsSet(eSpecies); }
	u_int GetCaughtCount() const                   { return m_xCaught.Count(); }

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
// marked caught, no pending whiteout, plus the starting economy (money + a couple of
// balls and salves). Used to seed the manager at boot (SC2) and to re-seed test
// isolation. Deterministic (no RNG).
ZM_GameState ZM_MakeStarterGameState();
