#pragma once

// ============================================================================
// ZM_InteractionLogic (S6 item 3 SC1) -- the PURE "may the player interact right
// now?" gate, plus the tuning constants the later sub-commits' candidate picker
// will use. Free functions over plain bools: NO ECS, NO scene, NO physics, NO UI,
// NO statics, NO g_xEngine, NO I/O -- so every rule is unit-testable headlessly.
//
// This is deliberately the SAME doctrine as ZM_UI_MenuStack::ShouldOpenMenu: a
// pure all-bools-in predicate, with the impure lookups (IsActiveSceneOverworld,
// IsWarpInProgress, IsTransitionActive, IsMenuOpen, the player's movement flag)
// left at the thin live call site. A reader who knows the pause-open gate
// recognises this one on sight.
//
// It exists because ZM_UI_MenuStack::PushDialogueLines is deliberately UNGATED --
// the dialogue box must stay usable inside the battle scene for trainer lines --
// so the "can the player talk right now?" question is owned HERE instead.
//
// SC1 lands the foundation ONLY. Nothing calls ZM_ShouldInteract yet; the
// candidate picker (range / vertical band / facing) arrives in SC2, which is why
// the reject reasons for those tests already exist in the enum but no picker is
// declared: the enum's VALUES are asserted on by later windowed tests and must
// not shift under them.
// ============================================================================

// Why an interaction attempt was refused. ZM_INTERACT_OK is the only success
// value. APPEND-ONLY; NEVER reorder -- windowed tests in later sub-commits assert
// on these values, and a reorder would silently change what those assertions mean.
enum ZM_INTERACT_REJECT : u_int
{
	ZM_INTERACT_OK = 0u,
	ZM_INTERACT_REJECT_NO_INPUT_EDGE,          // the interact key was not pressed THIS frame
	ZM_INTERACT_REJECT_MENU_OPEN,              // a menu / dialogue owns the screen already
	ZM_INTERACT_REJECT_NOT_OVERWORLD,          // title screen or the additive battle scene
	ZM_INTERACT_REJECT_WARP_IN_PROGRESS,       // a door / warp fade owns the screen
	ZM_INTERACT_REJECT_BATTLE_TRANSITION,      // a battle fade owns the screen
	ZM_INTERACT_REJECT_PLAYER_FROZEN,          // player movement is disabled (scripted / frozen)
	ZM_INTERACT_REJECT_NO_CANDIDATE,           // (SC2) nothing interactable in the scene
	ZM_INTERACT_REJECT_OUT_OF_RANGE,           // (SC2) nearest candidate beyond fZM_INTERACT_MAX_DISTANCE
	ZM_INTERACT_REJECT_OUT_OF_VERTICAL_BAND,   // (SC2) candidate on another floor
	ZM_INTERACT_REJECT_NOT_FACING,             // (SC2) candidate behind the player
	ZM_INTERACT_REJECT_DEGENERATE_ORIGIN,      // (SC2) zero-length facing / coincident positions

	// NOT a reason -- the walkable bound the name-totality test iterates to.
	// APPEND before this, never reorder.
	ZM_INTERACT_REJECT_COUNT
};

// ---- Candidate-picker tuning (SC2 consumes these; spelled once, HERE) --------

// `inline` so all three have ONE definition across every TU: the ZENITH_ASSERT_*
// macros bind their operands by const reference, so SC2's units odr-use them.
// Maximum player-to-interactable planar distance, in world units.
inline constexpr float fZM_INTERACT_MAX_DISTANCE = 2.5f;
// Minimum dot(playerFacing, toCandidate) -- roughly a 140-degree forward cone.
inline constexpr float fZM_INTERACT_MIN_FACING_DOT = 0.35f;
// Maximum absolute height difference, so a candidate on another floor is ignored.
inline constexpr float fZM_INTERACT_MAX_VERTICAL = 2.0f;

// ---- The gate ---------------------------------------------------------------

// The global interaction gate: everything that is true of the WORLD rather than of
// a particular candidate. Blocker precedence is FIXED and unit-pinned, in exactly
// this order, so a reject reason is stable no matter how many blockers hold at once:
//   1. !bPressed                 -> NO_INPUT_EDGE
//   2. bMenuOpen                 -> MENU_OPEN
//   3. !bOverworld               -> NOT_OVERWORLD
//   4. bWarpInProgress           -> WARP_IN_PROGRESS
//   5. bBattleTransitionActive   -> BATTLE_TRANSITION
//   6. !bPlayerMovementEnabled   -> PLAYER_FROZEN
//   7. otherwise                 -> ZM_INTERACT_OK
// The edge test leads because ZM_InputActions::ReadInteractPressed is a NON-consuming
// read: several consumers may see the same edge in one frame, so mutual exclusion has
// to be expressed here explicitly (the bMenuOpen lock) and never assumed from consumption.
ZM_INTERACT_REJECT ZM_ShouldInteract(bool bPressed, bool bMenuOpen, bool bOverworld,
	bool bWarpInProgress, bool bBattleTransitionActive,
	bool bPlayerMovementEnabled);

// A stable short name for a reject reason, for the later windowed tests' failure
// messages. TOTAL: never returns nullptr and never indexes out of bounds -- any
// value outside the enumerated range yields "UNKNOWN".
const char* ZM_InteractRejectName(ZM_INTERACT_REJECT eReject);
