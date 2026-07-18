#pragma once

#include "Zenithmon/Source/Party/ZM_Monster.h"
#include "Zenithmon/Source/Battle/ZM_BattleTypes.h"   // uZM_MAX_PARTY_SIZE (also reachable via ZM_Monster.h)

// ============================================================================
// ZM_Party -- the player's active party (S5 item 5 SC1): up to uZM_MAX_PARTY_SIZE
// (6) persistent ZM_Monster records, in slot order, plus a live count. A plain
// fixed-array aggregate -- no heap, no ECS, no I/O; header-only inline helpers.
//
// Item 5 sends only the LEAD into battle (single-active, D1); the 6-slot capacity
// exists so a caught monster joins as a bench member and the structure extends to
// multi-member battle later without a re-shape.
// ============================================================================

struct ZM_Party
{
	ZM_Monster m_axMembers[uZM_MAX_PARTY_SIZE];
	u_int      m_uCount = 0u;

	u_int Count() const   { return m_uCount; }
	bool  IsFull() const  { return m_uCount >= uZM_MAX_PARTY_SIZE; }
	bool  IsEmpty() const { return m_uCount == 0u; }

	// Append a record to the first free slot. Returns false (a strict no-op) when
	// the party is already full -- at item 5 the party starts at 1 and a single
	// catch reaches 2, so this never rejects in the shipped flow (boxes are S7).
	bool Add(const ZM_Monster& xRecord)
	{
		if (m_uCount >= uZM_MAX_PARTY_SIZE) { return false; }
		m_axMembers[m_uCount] = xRecord;
		++m_uCount;
		return true;
	}

	ZM_Monster& Get(u_int uIndex)
	{
		Zenith_Assert(uIndex < m_uCount, "ZM_Party::Get: index %u out of range (count %u)", uIndex, m_uCount);
		return m_axMembers[uIndex];
	}
	const ZM_Monster& Get(u_int uIndex) const
	{
		Zenith_Assert(uIndex < m_uCount, "ZM_Party::Get: index %u out of range (count %u)", uIndex, m_uCount);
		return m_axMembers[uIndex];
	}

	// The slot of the first non-fainted member, or 0 when the party is empty or
	// entirely fainted (the "or slot 0" fallback the battle send-out relies on).
	u_int LeadIndex() const
	{
		for (u_int u = 0u; u < m_uCount; ++u)
		{
			if (!m_axMembers[u].IsFainted()) { return u; }
		}
		return 0u;
	}
	ZM_Monster&       Lead()       { return m_axMembers[LeadIndex()]; }
	const ZM_Monster& Lead() const { return m_axMembers[LeadIndex()]; }

	// True when every member has curHP == 0 (the whiteout condition). An empty
	// party is vacuously all-fainted; in practice the party always holds the lead.
	bool AllFainted() const
	{
		for (u_int u = 0u; u < m_uCount; ++u)
		{
			if (!m_axMembers[u].IsFainted()) { return false; }
		}
		return true;
	}

	// Full-heal every member (curHP -> max, PP -> max, status cleared).
	void HealAllFull()
	{
		for (u_int u = 0u; u < m_uCount; ++u)
		{
			m_axMembers[u].HealToFull();
		}
	}
};
