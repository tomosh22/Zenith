#pragma once

#include "Zenithmon/Source/Party/ZM_Party.h"
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"   // ZM_SPECIES_ID, ZM_SPECIES_COUNT

// ============================================================================
// ZM_GameState -- the minimal persistent player state (S5 item 5 SC1): the party
// plus a caught-species set plus a pending-whiteout latch. A plain in-memory
// aggregate; at item 5 it is owned by the persistent ZM_GameStateManager (SC2)
// and lives for the session only (no disk save -- D3; S7 owns serialization).
// No ECS, no graphics, no I/O in this file.
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

struct ZM_GameState
{
	ZM_Party      m_xParty;
	ZM_SpeciesSet m_xCaught;
	bool          m_bPendingWhiteout = false;   // set on a battle loss (SC5); consumed by the whiteout warp

	void  MarkCaught(ZM_SPECIES_ID eSpecies)       { m_xCaught.Mark(eSpecies); }
	bool  IsCaught(ZM_SPECIES_ID eSpecies) const   { return m_xCaught.IsSet(eSpecies); }
	u_int GetCaughtCount() const                   { return m_xCaught.Count(); }
};

// The fixed starter GameState (D4): a single Fernfawn L5 party lead, its species
// marked caught, no pending whiteout. Used to seed the manager at boot (SC2) and
// to re-seed test isolation. Deterministic (no RNG).
ZM_GameState ZM_MakeStarterGameState();
