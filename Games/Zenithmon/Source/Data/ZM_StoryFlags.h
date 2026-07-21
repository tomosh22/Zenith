#pragma once

#include "Zenithmon/Source/Party/ZM_GameState.h"   // ZM_GameState, ZM_StoryFlagSet, uZM_MAX_STORY_FLAGS

// ============================================================================
// ZM_StoryFlags (S7 item 2 SC1) -- the identity registry for story flags.
//
// Before this file, a story-flag index was a raw integer literal that existed
// only inside tests: nothing in gameplay named one, and nothing stopped two
// pieces of content from claiming the same bit. This is the ZM-D-009
// compiled-const-table idiom applied to that space -- a save-stable enum, a
// compiled const row table, bounds-guarded free-function accessors, and one
// pure gate predicate that content rows embed.
//
// The accessors are FREE FUNCTIONS rather than ZM_GameState members because the
// durable model is frozen by ZM-D-135; adding a named accessor here costs zero
// wire change and keeps the model a plain aggregate.
//
// EVERY function below is TOTAL: no argument value, however garbage, is UB and
// none of them asserts. That is not squeamishness -- Zenith_Assert breaks the
// process in EVERY configuration in this engine (Zenith.h:138 defines
// ZENITH_ASSERT unconditionally, so the debug-break definition always wins), and
// the boot units feed these functions the ZM_STORY_FLAG_NONE sentinel and
// out-of-range ids on purpose to pin their fail-closed answers. An assert on a
// unit-pinned input therefore does not report a bug, it ENDS the boot unit run.
// A bad id that indicates MIS-AUTHORED DATA is diagnosed with a non-fatal
// Zenith_Error instead; a legitimate sentinel is not an error and logs nothing.
// ============================================================================

// SAVE-STABLE. The enum VALUE *is* the persisted bit index in save module 4
// (ZM_SaveSchema.cpp writes highest-set-index+1 followed by ceil(count/8) bytes).
// APPEND ONLY before ZM_STORY_FLAG_COUNT; never reorder, never reuse a retired
// value -- a reassignment is a versioned codec change, not an edit here
// (Docs/SaveFormat.md, "index registry").
//
// Indices are allocated DENSELY FROM ZERO, and that density is a storage
// contract rather than tidiness: because module 4 sizes itself from the highest
// SET index, one sparse allocation (say 4000) would add ~500 bytes to EVERY save
// this game ever writes, and those bytes could not be reclaimed without a
// versioned codec change. Reserve a future flag by adding it here, not by
// leaving a numeric gap.
enum ZM_STORY_FLAG_ID : u_int
{
	ZM_STORY_FLAG_INTRO_LEFT_HOME    = 0u,
	ZM_STORY_FLAG_MET_PROFESSOR      = 1u,
	ZM_STORY_FLAG_STARTER_RECEIVED   = 2u,
	ZM_STORY_FLAG_WARDEN_CLEARED     = 3u,   // the SC1 gated-line demonstration
	ZM_STORY_FLAG_ROUTE1_OPEN        = 4u,   // reserved for S8 routes
	ZM_STORY_FLAG_GYM1_DEFEATED      = 5u,   // reserved for S8 gyms

	ZM_STORY_FLAG_COUNT,
	ZM_STORY_FLAG_NONE = ZM_STORY_FLAG_COUNT   // "no flag" sentinel; NEVER persisted
};

static_assert((u_int)ZM_STORY_FLAG_COUNT <= uZM_MAX_STORY_FLAGS,
	"the registry may never exceed the durable model's 4096-bit ceiling");

// One registry row. The debug name is for logs and the editor ONLY -- nothing
// persists it, so renaming a flag is free while renumbering one is not.
struct ZM_StoryFlagInfo
{
	ZM_STORY_FLAG_ID m_eId;
	const char*      m_szDebugName;
};

// TOTAL: an unregistered id (including the sentinel) yields the shared UNKNOWN
// row rather than a table read, and logs a non-fatal Zenith_Error because such a
// lookup means mis-authored data or a mis-typed caller.
const ZM_StoryFlagInfo&	ZM_GetStoryFlagInfo(ZM_STORY_FLAG_ID eId);
u_int					ZM_GetStoryFlagCount();                      // == ZM_STORY_FLAG_COUNT
// TOTAL: "NONE" for the sentinel, "UNKNOWN" out of range, and SILENT for both --
// naming a flag is what callers do WITH a bad id, not a second place to complain
// about it. Never returns null, because every caller is a log format argument.
const char*				ZM_StoryFlagName(ZM_STORY_FLAG_ID eId);

// Typed read/write, TOTAL and SILENT. Out-of-range and NONE writes return false
// with NO mutation; the matching reads return false. Nothing is logged: querying
// or clearing a flag you do not have is ordinary, and ZM_STORY_FLAG_NONE is a
// legitimate value a data row carries constantly, not an error. The sentinel must
// never fall through to ZM_StoryFlagSet::Set, which would happily set bit
// ZM_STORY_FLAG_COUNT.
bool ZM_SetStoryFlag(ZM_GameState& xStateInOut, ZM_STORY_FLAG_ID eFlag, bool bSet);
bool ZM_IsStoryFlagSet(const ZM_GameState& xState, ZM_STORY_FLAG_ID eFlag);

// Overloads for callers that hold a flag set directly (content gates, tests).
bool ZM_SetStoryFlag(ZM_StoryFlagSet& xFlagsInOut, ZM_STORY_FLAG_ID eFlag, bool bSet);
bool ZM_IsStoryFlagSet(const ZM_StoryFlagSet& xFlags, ZM_STORY_FLAG_ID eFlag);

// A content gate, embedded by value in authored data rows. A default-constructed
// gate is UNCONDITIONAL, so a data row that GAINS this field keeps its previous
// behaviour by construction rather than by every author remembering to fill it in.
struct ZM_StoryGate
{
	ZM_STORY_FLAG_ID m_eFlag       = ZM_STORY_FLAG_NONE;
	bool             m_bRequireSet = true;   // pass when SET; false => pass when CLEAR
};

// TOTAL. NONE passes unconditionally and silently. A garbage id FAILS CLOSED in
// every build -- a mis-authored gate must LOCK content, never open it -- and logs
// a non-fatal Zenith_Error naming the id, because a gate is the one place where a
// bad id really is authored data that someone has to fix. It does NOT assert:
// this is the branch Gate_OutOfRangeFailsClosed drives on purpose, and a
// Zenith_Assert would break the process mid-way through the boot unit run.
bool ZM_StoryGatePasses(const ZM_StoryGate& xGate, const ZM_StoryFlagSet& xFlags);

// Does setting this flag deserve a milestone autosave? Kept beside the registry
// so the milestone list cannot drift away from the flags it names. TOTAL and
// SILENT: the sentinel and any garbage id simply answer false.
bool ZM_IsMilestoneStoryFlag(ZM_STORY_FLAG_ID eFlag);
