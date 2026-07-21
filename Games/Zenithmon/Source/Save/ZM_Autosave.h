#pragma once

#include "Zenithmon/Source/Save/ZM_SaveSlots.h"    // ZM_SaveSlots::ZM_SAVE_BLOCKER, ZM_SAVE_SLOT_AUTO

// ============================================================================
// ZM_Autosave (S7 item 2 SC3) -- the milestone autosave policy and its one live
// entry point.
//
// The POLICY is pure (ZM_ShouldAutosave takes every live condition as an
// argument) and the RUNTIME is a thin wrapper that gathers those conditions and
// defers to it. That split is what lets the whole "may this milestone save right
// now" decision be pinned by headless boot units with no scene, no manager and
// no disk.
//
// The permission half is DELIBERATELY NOT re-derived here: it forwards
// ZM_SaveSlots::ZM_SAVE_BLOCKER straight through, so the manual menu save and
// the milestone autosave can never end up with two different answers to "may the
// player save right now". The one condition autosave adds on top is the open
// menu, because the manual path is REACHED through the menu and the autosave
// path must not fire underneath it.
//
// NOTHING IN THIS FILE MAY Zenith_Assert ON ITS ARGUMENTS -- Zenith.h:138 makes
// Zenith_Assert fatal in every configuration and the boot units drive the
// out-of-range and sentinel arms on purpose. Every function is TOTAL.
// ============================================================================

// The FINALIZED milestone list. SaveFormat.md named four candidates and left the
// list to this item; all of them ship in the enum NOW so it never has to be
// reordered later. APPEND ONLY -- this enum is not persisted today, but it is
// the vocabulary telemetry and the UI will name, and renumbering it silently
// re-labels both.
//
// Declaring a trigger is NOT shipping it: ZM_IsAutosaveTriggerLive gates each
// arm, and today exactly one is live. A stage whose producer has not landed
// cannot autosave and therefore cannot ship untestable production code.
enum ZM_AUTOSAVE_TRIGGER : u_int
{
	ZM_AUTOSAVE_TRIGGER_NONE = 0u,
	ZM_AUTOSAVE_TRIGGER_SCENE_ENTERED,        // LIVE today: a completed warp into an overworld scene
	ZM_AUTOSAVE_TRIGGER_BADGE_EARNED,         // declared; producer lands with S8 gyms
	ZM_AUTOSAVE_TRIGGER_STORY_FLAG_SET,       // declared; producer lands with S8 story beats
	ZM_AUTOSAVE_TRIGGER_LEAGUE_ENTERED,       // declared; producer lands with S10
	ZM_AUTOSAVE_TRIGGER_TOWER_STREAK_BANKED,  // declared; producer lands with S11

	ZM_AUTOSAVE_TRIGGER_COUNT
};

// TOTAL: never returns nullptr; anything outside the enumerated range is
// "UNKNOWN".
const char* ZM_AutosaveTriggerName(ZM_AUTOSAVE_TRIGGER eTrigger);

// TOTAL. A DECLARED-but-not-yet-shipped trigger returns FALSE. NONE is false too:
// "no trigger" is the default-constructed value and must never be able to save.
bool ZM_IsAutosaveTriggerLive(ZM_AUTOSAVE_TRIGGER eTrigger);

// PURE full policy: the trigger is live AND the shared save-permission predicate
// reports no blocker AND no menu is open. Every term is load-bearing and each
// has its own single-condition unit, because a combined truth table still passes
// with any one term missing.
bool ZM_ShouldAutosave(ZM_AUTOSAVE_TRIGGER eTrigger, ZM_SaveSlots::ZM_SAVE_BLOCKER eBlocker,
	bool bMenuOpen);

// NOTE ON THE STORY-FLAG EDGE RULE: the rising-edge / idempotence resolver that
// would turn a milestone story-flag WRITE into ZM_AUTOSAVE_TRIGGER_STORY_FLAG_SET
// is deliberately NOT declared here. It is not in the SC3 scope, it has no
// producer until the S8 story beats land, and uncalled production surface cannot
// be pinned by anything a caller does -- so it lands WITH its producer, in the
// sub-commit that first calls it, together with its own units. The trigger enum
// arm above already reserves its vocabulary.

// RUNTIME. Consult ZM_ShouldAutosave against ZM_SaveSlots::ResolveLiveSaveBlocker()
// and the live menu state, capture the world position into the live ZM_GameState,
// then write ZM_SAVE_SLOT_AUTO. NEVER writes a manual slot.
//
// A refused or failed autosave logs and returns false and does NOT retry: the
// caller latches this ONE-SHOT, so a retry here would hammer the disk every frame
// for as long as the failure persisted.
bool ZM_TryAutosave(ZM_AUTOSAVE_TRIGGER eTrigger);

// Observability for the windowed tests: how many autosaves have actually landed
// this process. Deliberately a plain counter and not a timestamp -- the tests
// need to prove "exactly one more", not "recently".
u_int ZM_GetAutosaveCount();

#ifdef ZENITH_INPUT_SIMULATOR
// Clears the counter. Called from ZM_GameStateManager::ResetRuntimeStateForTests
// BEFORE its no-manager early-out, because this counter is a process global and
// a batched run that has just force-loaded FrontEnd has no manager to resolve.
void ZM_ResetAutosaveForTests();
#endif
