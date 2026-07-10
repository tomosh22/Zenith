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
