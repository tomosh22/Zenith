#include "Zenith.h"

#include "Zenithmon/Source/Battle/ZM_BattleState.h"

// True if any monster on this side still has HP. Index loop (no STL iterators).
bool ZM_BattleSide::HasUnfainted() const
{
	for (u_int u = 0; u < m_xParty.GetSize(); ++u)
	{
		if (!m_xParty.Get(u).IsFainted())
		{
			return true;
		}
	}
	return false;
}

bool ZM_BattleSide::CanSwitchTo(u_int uSlot) const
{
	return uSlot < m_xParty.GetSize() && uSlot != m_uActiveSlot
		&& !m_xParty.Get(uSlot).IsFainted();
}

u_int ZM_BattleSide::FindLowestSwitchTarget() const
{
	for (u_int u = 0u; u < m_xParty.GetSize(); ++u)
	{
		if (CanSwitchTo(u))
		{
			return u;
		}
	}
	return uZM_MAX_PARTY_SIZE;
}
