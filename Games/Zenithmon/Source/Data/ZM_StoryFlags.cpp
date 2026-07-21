#include "Zenith.h"
#include "Zenithmon/Source/Data/ZM_StoryFlags.h"

// ============================================================================
// ZM_StoryFlags -- the registry table and its guarded accessors. Rows are in
// ZM_STORY_FLAG_ID order and s_axFlags[i].m_eId == i is a boot-tested invariant
// (Tests/ZM_Tests_StoryFlags.cpp), which is what makes "the enum value IS the
// wire bit index" a checked claim rather than a comment.
//
// NOTHING IN THIS FILE MAY Zenith_Assert ON ITS ARGUMENTS. Zenith.h:138 defines
// ZENITH_ASSERT unconditionally, so Zenith_Assert calls Zenith_DebugBreak() in
// EVERY configuration -- there is no build in which it degrades to a log. These
// functions are TOTAL by contract and the boot units pin that totality by
// feeding them the sentinel and out-of-range ids on purpose, so a defensive
// assert here does not catch a bug: it kills the process partway through the
// unit run at boot and takes the whole gate down with it. Diagnose a
// mis-authored id with a non-fatal Zenith_Error and return the fail-closed
// answer.
// ============================================================================

namespace
{
	// The bound is DEDUCED, never spelled. With an explicit [ZM_STORY_FLAG_COUNT]
	// the static_assert below would be a tautology -- true by construction -- and a
	// forgotten row would merely zero-initialise the tail into a nameless flag that
	// only the boot units could catch. Deduced, a missing or extra row is a COMPILE
	// error at the table itself.
	const ZM_StoryFlagInfo s_axFlags[] =
	{
		{ ZM_STORY_FLAG_INTRO_LEFT_HOME,  "INTRO_LEFT_HOME"  },
		{ ZM_STORY_FLAG_MET_PROFESSOR,    "MET_PROFESSOR"    },
		{ ZM_STORY_FLAG_STARTER_RECEIVED, "STARTER_RECEIVED" },
		{ ZM_STORY_FLAG_WARDEN_CLEARED,   "WARDEN_CLEARED"   },
		{ ZM_STORY_FLAG_ROUTE1_OPEN,      "ROUTE1_OPEN"      },
		{ ZM_STORY_FLAG_GYM1_DEFEATED,    "GYM1_DEFEATED"    },
	};

	static_assert(sizeof(s_axFlags) / sizeof(s_axFlags[0]) == ZM_STORY_FLAG_COUNT,
		"the registry table must have exactly one row per ZM_STORY_FLAG_ID");

	// Handed back for an out-of-range id. The alternative -- indexing anyway after
	// an assert -- reads off the end of the table the moment the break is stepped
	// past, and a registry whose job is to make bad ids safe should not have that
	// hole.
	const ZM_StoryFlagInfo s_xInvalidFlag = { ZM_STORY_FLAG_NONE, "UNKNOWN" };

	bool ZM_IsRegisteredFlag(ZM_STORY_FLAG_ID eFlag)
	{
		// ZM_STORY_FLAG_NONE aliases ZM_STORY_FLAG_COUNT, so this single comparison
		// rejects the sentinel and every garbage value together.
		return (u_int)eFlag < (u_int)ZM_STORY_FLAG_COUNT;
	}
}

const ZM_StoryFlagInfo& ZM_GetStoryFlagInfo(ZM_STORY_FLAG_ID eId)
{
	if (!ZM_IsRegisteredFlag(eId))
	{
		// A row lookup for an id the registry does not name is mis-authored data or a
		// mis-typed caller, so it is SAID OUT LOUD -- but non-fatally, because the
		// function is total and returning the UNKNOWN row is the contract.
		Zenith_Error(LOG_CATEGORY_GAMEPLAY,
			"[ZM_StoryFlags] ZM_GetStoryFlagInfo: id %u is not a registered story flag "
			"-- returning the UNKNOWN row", (u_int)eId);
		return s_xInvalidFlag;
	}
	return s_axFlags[(u_int)eId];
}

u_int ZM_GetStoryFlagCount()
{
	return (u_int)ZM_STORY_FLAG_COUNT;
}

const char* ZM_StoryFlagName(ZM_STORY_FLAG_ID eId)
{
	// NONE is a legal value to name (data rows carry it constantly), so it is
	// distinguished from garbage rather than folded into it.
	if (eId == ZM_STORY_FLAG_NONE)
	{
		return "NONE";
	}
	if (!ZM_IsRegisteredFlag(eId))
	{
		return "UNKNOWN";
	}
	return s_axFlags[(u_int)eId].m_szDebugName;
}

bool ZM_SetStoryFlag(ZM_StoryFlagSet& xFlagsInOut, ZM_STORY_FLAG_ID eFlag, bool bSet)
{
	if (!ZM_IsRegisteredFlag(eFlag))
	{
		return false;
	}
	return xFlagsInOut.Set((u_int)eFlag, bSet);
}

bool ZM_IsStoryFlagSet(const ZM_StoryFlagSet& xFlags, ZM_STORY_FLAG_ID eFlag)
{
	if (!ZM_IsRegisteredFlag(eFlag))
	{
		return false;
	}
	return xFlags.IsSet((u_int)eFlag);
}

bool ZM_SetStoryFlag(ZM_GameState& xStateInOut, ZM_STORY_FLAG_ID eFlag, bool bSet)
{
	return ZM_SetStoryFlag(xStateInOut.m_xStoryFlags, eFlag, bSet);
}

bool ZM_IsStoryFlagSet(const ZM_GameState& xState, ZM_STORY_FLAG_ID eFlag)
{
	return ZM_IsStoryFlagSet(xState.m_xStoryFlags, eFlag);
}

bool ZM_StoryGatePasses(const ZM_StoryGate& xGate, const ZM_StoryFlagSet& xFlags)
{
	// Order matters: NONE aliases COUNT, so the "ungated" answer must be decided
	// before the range check would classify it as garbage.
	if (xGate.m_eFlag == ZM_STORY_FLAG_NONE)
	{
		return true;
	}
	if (!ZM_IsRegisteredFlag(xGate.m_eFlag))
	{
		// A gate naming an id the registry does not know is MIS-AUTHORED DATA, so it is
		// diagnosed -- but with a log, never an assert. Zenith_Assert breaks the process
		// in every configuration (see the file header), and this exact branch is driven
		// on purpose by Gate_OutOfRangeFailsClosed during the boot unit run, so an
		// assert here would end that run instead of pinning the fail-closed answer.
		Zenith_Error(LOG_CATEGORY_GAMEPLAY,
			"[ZM_StoryFlags] ZM_StoryGatePasses: gate references an UNREGISTERED story "
			"flag (%u) -- failing closed", (u_int)xGate.m_eFlag);
		// FAIL CLOSED, and deliberately NOT written as `IsSet(id) == m_bRequireSet`:
		// that form returns TRUE for an unregistered id whenever the gate wanted the
		// flag CLEAR, i.e. a typo would UNLOCK content instead of locking it.
		return false;
	}

	const bool bSet = xFlags.IsSet((u_int)xGate.m_eFlag);
	return xGate.m_bRequireSet ? bSet : !bSet;
}

bool ZM_IsMilestoneStoryFlag(ZM_STORY_FLAG_ID eFlag)
{
	// A switch rather than a table, so ZM_STORY_FLAG_NONE and anything past it land
	// on the default arm instead of indexing off the end of an array.
	switch (eFlag)
	{
	case ZM_STORY_FLAG_WARDEN_CLEARED:
	case ZM_STORY_FLAG_ROUTE1_OPEN:
	case ZM_STORY_FLAG_GYM1_DEFEATED:
		return true;
	default:
		return false;
	}
}
