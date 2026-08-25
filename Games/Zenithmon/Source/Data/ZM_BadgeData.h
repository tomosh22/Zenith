#pragma once

// ============================================================================
// ZM_BadgeData (S8, Gym 1 -- slice G1-1) -- the region's eight gym badges as
// INDICES and NAMES, and nothing else.
//
// ★★ THIS FILE ADDS NO STATE, AND THAT IS THE WHOLE POINT. Badge STATE has shipped
// since S5 and is complete: uZM_BADGE_COUNT, ZM_GameState::m_uBadgeMask,
// AwardBadge / HasBadge / GetBadgeCount (Source/Party/ZM_GameState.h), persisted as
// save module 5 (Source/Core/ZM_SaveSchema.cpp). What was missing is the other half
// of that pair -- WHICH BIT IS WHICH BADGE, and what a badge is called. A bare
// AwardBadge(0u) at a call site is an unnamed magic number that no reader and no
// test can check against the design document; AwardBadge(ZM_BADGE_BLOOM) is a
// claim. This file supplies exactly that much and stops.
//
// So: no mask, no setter, no count of its own (see the star below), no save-schema
// touch, and no new module. The table is the compiled-const idiom ZM-D-009
// established and ZM_TrainerData / ZM_ItemData / ZM_NpcData all follow -- a
// save-stable enum, a compiled `const` C array of rows, free-function accessors.
//
// ★ THE COUNT IS NOT RESTATED HERE. uZM_BADGE_COUNT = 8 already lives in
// Source/Party/ZM_GameState.h, where the mask that has to hold it lives, and a
// second spelling of it here would be an inventory nothing reconciles. This table
// NAMES the eight indices; ZM_BadgeData.cpp carries a static_assert that the two
// agree, which is a real tripwire in the one direction that matters -- a ninth
// badge id with an eight-bit mask underneath it would otherwise be a silent
// no-op award.
//
// ★ NOTHING HERE IS INVENTED. All eight names come from Docs/GameDesignDocument.md
// section 3.4 (GDD 192-201), and the INDEX->gym mapping from Docs/SaveFormat.md
// ("badgeMask uint8, Bits 0..7 = badges 1..8"): badge index i is gym i+1. Gym 1 is
// Fenna's, in Thornacre, and her badge is the Bloom Badge (GDD 194).
//
// ★ WHAT A BADGE ROW DELIBERATELY DOES NOT CARRY: the leader who awards it. Only
// one of the eight leaders exists as a ZM_TRAINER_ID today (ZM_TRAINER_GYM1_FENNA,
// added by this same slice), so a trainer column would be seven invented ids and
// one real one. The trainer -> badge link is G1-3's (ZM-70), which awards the badge
// on a leader win; it names both constants rather than adding a column here.
// ============================================================================

// Every badge, in GYM ORDER, which is also BIT ORDER in ZM_GameState::m_uBadgeMask.
//
// ★★ THESE ORDINALS ARE PERSISTED. The mask is written to save module 5 verbatim,
// so a reordering does not rename a badge -- it silently gives every existing save
// a different set of badges. APPEND ONLY, and there is nowhere to append to:
// uZM_BADGE_COUNT is 8 and the mask is a u_int8. A ninth badge is a save-format
// change, not an edit to this enum.
enum ZM_BADGE_ID : u_int
{
	ZM_BADGE_BLOOM,   // Gym 1 -- Fenna,    Grass,    Thornacre Town    (GDD 194)
	ZM_BADGE_KILN,    // Gym 2 -- Bram,     Fire,     Cinderfell Town   (GDD 195)
	ZM_BADGE_TIDE,    // Gym 3 -- Maris,    Water,    Tidegate City     (GDD 196)
	ZM_BADGE_COIL,    // Gym 4 -- Tessa,    Electric, Gearspring City   (GDD 197)
	ZM_BADGE_GALE,    // Gym 5 -- Aquilo,   Sky,      Skyshear Town     (GDD 198)
	ZM_BADGE_WISP,    // Gym 6 -- Morwenna, Phantom,  Umbermoor Town    (GDD 199)
	ZM_BADGE_RIME,    // Gym 7 -- Halvard,  Ice,      Frostvale City    (GDD 200)
	ZM_BADGE_CREST,   // Gym 8 -- Vardis,   Drake,    Stonereach City   (GDD 201)

	ZM_BADGE_ID_COUNT,
	ZM_BADGE_NONE = ZM_BADGE_ID_COUNT   // "no badge" sentinel
};

// One badge row. The id is spelled in the row as well as implied by its index, so
// the "row index equals id" invariant is checkable rather than assumed -- the same
// shape every other table in Source/Data uses.
struct ZM_BadgeData
{
	ZM_BADGE_ID		m_eId;
	const char*		m_szDisplayName;   // shown in the badge case / HUD / dialogue
};

// ============================================================================
// EVERY FUNCTION BELOW IS TOTAL: no argument value, however garbage, is UB, and
// none of them asserts. Zenith_Assert calls Zenith_DebugBreak() in EVERY
// configuration in this engine, and the boot units feed these functions the
// ZM_BADGE_NONE sentinel and out-of-range ids on purpose to pin their fail-closed
// answers -- an assert on a unit-pinned input does not report a bug, it ENDS the
// boot unit run and takes the whole gate down. A bad id that indicates MIS-AUTHORED
// DATA is diagnosed with a non-fatal Zenith_Error instead; a legitimate sentinel is
// not an error and logs nothing. (This is ZM_TrainerData's contract, verbatim, and
// deliberately NOT ZM_GetItemData's older asserting one.)
// ============================================================================

// TOTAL: an unregistered id (including the sentinel) yields the shared UNKNOWN row
// -- { ZM_BADGE_NONE, "UNKNOWN" } -- rather than a table read.
const ZM_BadgeData&	ZM_GetBadgeData(ZM_BADGE_ID eId);

// TOTAL: "NONE" for the sentinel, "UNKNOWN" out of range, and SILENT for both.
// Never returns null, because every caller is a log or dialogue format argument.
const char*			ZM_GetBadgeName(ZM_BADGE_ID eId);

// TOTAL. True only for 0..ZM_BADGE_ID_COUNT-1; ZM_BADGE_NONE aliases
// ZM_BADGE_ID_COUNT, so the sentinel and every garbage value are rejected by one
// comparison. This is the check a caller owes before handing an id to
// ZM_GameState::AwardBadge, which takes a bare u_int index.
bool				ZM_IsRegisteredBadge(ZM_BADGE_ID eId);
