#pragma once

struct ZM_GameState;
struct ZM_Party;

// ============================================================================
// ZM_CareCenter (S6 item 2 SC8) -- the town Care Center's heal: the player-facing
// lines the prompt is built from, the "does this party need it at all" predicate,
// and the heal itself applied to the live game state.
//
// 100% PURE: no ECS, no UI, no graphics, no I/O, no statics. The prompt is raised
// by ZM_UI_MenuStack::TryOpenCareCenterPrompt (which owns the dialogue + the yes/no
// choice) and S6 item 3's Care Center NPC talks through that same seam -- nothing
// in this file knows either exists.
// ============================================================================

// The question the prompt asks, and the two answer labels. String LITERALS, so an
// authoring site may hold them for the lifetime of the process.
const char* ZM_CareCenterPromptLine();
const char* ZM_CareCenterYesLabel();
const char* ZM_CareCenterNoLabel();
// The line shown once the heal has actually been applied (S6 item 3's NPC follows
// the prompt with it).
const char* ZM_CareCenterHealedLine();

// True when ANY member is below its max HP, carries a major status, or has a move
// below its max PP -- i.e. when a heal would actually change something. An EMPTY
// party needs no healing (false) and is never indexed.
bool ZM_PartyNeedsHealing(const ZM_Party& xParty);

// Restore the whole party (curHP, PP, status) via ZM_Party::HealAllFull. Returns
// whether anything actually needed it, so a caller can tell "healed" apart from
// "already fine" -- the heal itself is unconditional and idempotent.
bool ZM_ApplyCareCenterHeal(ZM_GameState& xStateInOut);
