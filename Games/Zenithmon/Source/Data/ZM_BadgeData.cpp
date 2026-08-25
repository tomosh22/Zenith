#include "Zenith.h"
#include "Zenithmon/Source/Data/ZM_BadgeData.h"

#include "Zenithmon/Source/Party/ZM_GameState.h"   // uZM_BADGE_COUNT -- the mask this table names the bits of

// ============================================================================
// ZM_BadgeData -- the eight-row badge name table (S8, Gym 1 slice G1-1). Rows are
// in ZM_BADGE_ID order; s_axBadges[i].m_eId == i is asserted by the boot units.
// See the header for the per-field contract and for why this file adds no state.
// Column legend:
//   id, "display name"
//
// NOTHING IN THIS FILE MAY Zenith_Assert ON ITS ARGUMENTS. Zenith_Assert calls
// Zenith_DebugBreak() in EVERY configuration -- there is no build in which it
// degrades to a log. These functions are TOTAL by contract and the boot units pin
// that totality by feeding them the sentinel and out-of-range ids on purpose, so a
// defensive assert here does not catch a bug: it kills the process partway through
// the unit run at boot and takes the whole gate down with it.
// ============================================================================

namespace
{
	// The bound is DEDUCED, never spelled. With an explicit [ZM_BADGE_ID_COUNT] the
	// static_assert below would be a tautology -- true by construction -- and a
	// forgotten row would merely zero-initialise the tail into a nameless badge with
	// a null name. Deduced, a missing or extra row is a COMPILE error at the table.
	const ZM_BadgeData s_axBadges[] =
	{
		{ ZM_BADGE_BLOOM, "Bloom Badge" },
		{ ZM_BADGE_KILN,  "Kiln Badge"  },
		{ ZM_BADGE_TIDE,  "Tide Badge"  },
		{ ZM_BADGE_COIL,  "Coil Badge"  },
		{ ZM_BADGE_GALE,  "Gale Badge"  },
		{ ZM_BADGE_WISP,  "Wisp Badge"  },
		{ ZM_BADGE_RIME,  "Rime Badge"  },
		{ ZM_BADGE_CREST, "Crest Badge" },
	};

	static_assert(sizeof(s_axBadges) / sizeof(s_axBadges[0]) == ZM_BADGE_ID_COUNT,
		"the badge table must have exactly one row per ZM_BADGE_ID");

	// ★★ THE CROSS-TABLE TRIPWIRE THIS FILE EXISTS TO CARRY. ZM_GameState's mask is
	// a u_int8 sized by uZM_BADGE_COUNT, and AwardBadge REJECTS an index at or past
	// it by returning false -- silently, from the player's point of view. So a ninth
	// ZM_BADGE_ID would not be a badge that fails to persist; it would be a gym the
	// player can beat forever without ever being awarded anything, with every unit in
	// this file still green. It lives in the .cpp so the header stays free of
	// Source/Party.
	static_assert((u_int)ZM_BADGE_ID_COUNT == uZM_BADGE_COUNT,
		"the badge NAME table and the badge MASK disagree on how many badges the "
		"region has -- an id past the mask's width is an award that silently does "
		"nothing");

	// Handed back for an unregistered id. The alternative -- indexing anyway after an
	// assert -- reads off the end of the table the moment the break is stepped past.
	// Every field is the INERT answer, and the name is one no authored badge carries.
	const ZM_BadgeData s_xInvalidBadge = { ZM_BADGE_NONE, "UNKNOWN" };
}

bool ZM_IsRegisteredBadge(ZM_BADGE_ID eId)
{
	// ZM_BADGE_NONE aliases ZM_BADGE_ID_COUNT, so this single comparison rejects the
	// sentinel and every garbage value together.
	return (u_int)eId < (u_int)ZM_BADGE_ID_COUNT;
}

const ZM_BadgeData& ZM_GetBadgeData(ZM_BADGE_ID eId)
{
	if (!ZM_IsRegisteredBadge(eId))
	{
		// A row lookup for an id the region does not name is mis-authored data or a
		// mis-typed caller, so it is SAID OUT LOUD -- but non-fatally, because the
		// function is total and returning the UNKNOWN row is the contract.
		Zenith_Error(LOG_CATEGORY_GAMEPLAY,
			"[ZM_BadgeData] ZM_GetBadgeData: id %u is not a registered badge "
			"-- returning the UNKNOWN row", (u_int)eId);
		return s_xInvalidBadge;
	}
	return s_axBadges[(u_int)eId];
}

const char* ZM_GetBadgeName(ZM_BADGE_ID eId)
{
	// NONE is a legal value to name (a "which badge did you just win" field carries
	// it constantly), so it is distinguished from garbage rather than folded into it.
	if (eId == ZM_BADGE_NONE)
	{
		return "NONE";
	}
	if (!ZM_IsRegisteredBadge(eId))
	{
		return "UNKNOWN";
	}
	return s_axBadges[(u_int)eId].m_szDisplayName;
}
