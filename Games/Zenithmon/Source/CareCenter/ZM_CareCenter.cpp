#include "Zenith.h"

#include "Zenithmon/Source/CareCenter/ZM_CareCenter.h"

#include "Zenithmon/Source/Party/ZM_GameState.h"   // ZM_GameState::m_xParty (the heal target)
#include "Zenithmon/Source/Party/ZM_Party.h"       // ZM_Party / ZM_Monster / ZM_MoveSlot

// ============================================================================
// ZM_CareCenter (S6 item 2 SC8). Pure lines + predicate + heal; see the header.
// ============================================================================

const char* ZM_CareCenterPromptLine()
{
	return "Rest your team and restore them to full health?";
}

const char* ZM_CareCenterYesLabel()
{
	return "Yes";
}

const char* ZM_CareCenterNoLabel()
{
	return "No";
}

const char* ZM_CareCenterHealedLine()
{
	return "Your team is fully rested!";
}

bool ZM_PartyNeedsHealing(const ZM_Party& xParty)
{
	// Count() bounds the walk, so Get() is never called out of range -- an empty party
	// falls straight through to false rather than asserting on slot 0.
	for (u_int u = 0u; u < xParty.Count(); ++u)
	{
		const ZM_Monster& xMember = xParty.Get(u);
		// Max HP is DERIVED from the tables (never stored), so this compares against the
		// same value HealToFull would write.
		if (xMember.m_uCurrentHp < xMember.GetMaxHP())
		{
			return true;
		}
		if (xMember.m_eStatus != ZM_MAJOR_STATUS_NONE)
		{
			return true;
		}
		for (u_int uMove = 0u; uMove < uZM_MAX_MOVES; ++uMove)
		{
			const ZM_MoveSlot& xSlot = xMember.m_axMoves[uMove];
			// An EMPTY slot is ZM_MOVE_NONE with 0/0 PP; testing the id first keeps the
			// intent explicit rather than leaning on 0 < 0 being false.
			if (xSlot.m_eMove != ZM_MOVE_NONE && xSlot.m_uCurPP < xSlot.m_uMaxPP)
			{
				return true;
			}
		}
	}
	return false;
}

bool ZM_ApplyCareCenterHeal(ZM_GameState& xStateInOut)
{
	// Sampled BEFORE the heal -- afterwards nothing needs healing by construction.
	const bool bNeeded = ZM_PartyNeedsHealing(xStateInOut.m_xParty);
	xStateInOut.m_xParty.HealAllFull();
	return bNeeded;
}
