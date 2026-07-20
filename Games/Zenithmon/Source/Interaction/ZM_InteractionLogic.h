#pragma once

#include "Maths/Zenith_Maths.h"   // Zenith_Maths::Vector3 / Quat (the SC2 picker's inputs)

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
// SC1 landed the foundation: the enum, the tuning constants and the world gate.
// SC2 adds the CANDIDATE picker below (range / vertical band / facing), which is
// what the SC1 enum's NO_CANDIDATE / OUT_OF_RANGE / OUT_OF_VERTICAL_BAND /
// NOT_FACING / DEGENERATE_ORIGIN reasons were reserved for. Nothing calls either
// function yet -- the live wiring is a later sub-commit's job -- so this file
// stays pure and headlessly testable.
//
// The two halves compose in the obvious order at the eventual call site: the
// world gate first (is interacting legal AT ALL right now?), then the picker
// (WHICH interactable, if any?). Both report the same reject enum so the caller
// has exactly one reason value to log, poll and assert on.
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

// ---- The candidate picker (SC2) ---------------------------------------------

// One interactable's world presence, BY VALUE. The picker never reads the ECS, a
// scene or a physics body: the live call site (a later sub-commit) fills an array
// of these from whatever it has, and every rule below stays unit-testable.
struct ZM_InteractProbe
{
	Zenith_Maths::Vector3 m_xPosition = Zenith_Maths::Vector3(0.0f);
	// A per-NPC reach bonus ADDED to xTuning.m_fMaxDistance, so a physically large
	// interactable (a counter, a sign post) can be talked to from its edge rather
	// than from its centre. A NEGATIVE radius never shrinks reach below zero --
	// such a probe is simply never in range.
	float m_fRadius = 0.0f;
	// A parked / scripted-off NPC is not a candidate AT ALL: it neither wins nor
	// contributes a near-miss reason (see the reject precedence below).
	bool m_bEnabled = false;
};

// Where the player is and which way they are looking. m_xForward need NOT be
// normalised and need NOT be XZ-flat -- the picker flattens and normalises it with
// the SAME policy as ZM_ForwardFromRotation, so a camera-derived forward with
// pitch in it behaves identically to a yaw-only one.
struct ZM_InteractOrigin
{
	Zenith_Maths::Vector3 m_xPosition = Zenith_Maths::Vector3(0.0f);
	Zenith_Maths::Vector3 m_xForward  = Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
};

// The three thresholds, passed in rather than read from the constants above so a
// unit can move a boundary without touching shipped tuning. Seed one from
// fZM_INTERACT_MAX_DISTANCE / fZM_INTERACT_MIN_FACING_DOT / fZM_INTERACT_MAX_VERTICAL
// for the live values.
struct ZM_InteractTuning
{
	float m_fMaxDistance   = fZM_INTERACT_MAX_DISTANCE;
	float m_fMinFacingDot  = fZM_INTERACT_MIN_FACING_DOT;
	float m_fMaxVertical   = fZM_INTERACT_MAX_VERTICAL;
};

// Pick the best interaction candidate out of paxProbes[0 .. uCount).
//
// ACCEPTANCE, per probe, in this order:
//   1. m_bEnabled, or the probe does not exist as far as this function is concerned.
//   2. XZ distance (Y IGNORED) <= m_fMaxDistance + probe.m_fRadius. INCLUSIVE at the
//      boundary. Y is ignored here so a sunk or floating NPC stays reachable; the
//      height difference is policed separately by rule 3.
//   3. fabs(dY) <= m_fMaxVertical. INCLUSIVE. This is what stops interaction through
//      a floor / ceiling.
//   4. dot(forwardXZ, toCandidateXZ), both flattened and normalised, >= m_fMinFacingDot.
//      INCLUSIVE.
//
// WINNER: among all survivors the SMALLEST XZ distance wins, ties break to the
// LOWEST INDEX. The outcome therefore does NOT depend on array order (reversing the
// array picks the same probe), while remaining deterministic when two probes tie.
//
// COINCIDENT PROBE: a probe whose XZ position is within the degenerate epsilon of the
// origin's has no direction to face, so instead of a NaN dot product it SKIPS the cone
// test and counts as FACED -- you are standing on it. It still has to clear the reach
// and band tests, both of which it passes for any non-negative reach, and being at
// distance zero it then wins outright.
//
// DEGENERATE ORIGIN is checked FIRST, before any probe is looked at: if the origin's
// forward flattens to (near) nothing -- a straight up or straight down facing -- the
// cone test is meaningless, so the call returns ZM_INTERACT_REJECT_DEGENERATE_ORIGIN
// without walking the array.
//
// REJECT REPORTING is MOST-SPECIFIC-LAST: the reason returned is how FAR the best
// near-miss got, so a walk-up test can poll it and know how close the player is:
//   NO_CANDIDATE          - empty set, or every probe disabled
//   OUT_OF_RANGE          - enabled probes exist, none passed the distance test
//   OUT_OF_VERTICAL_BAND  - one passed distance, none passed the band
//   NOT_FACING            - one passed distance AND band, none passed the cone
//
// uBestIndexOut is the winner's index into paxProbes on ZM_INTERACT_OK. On EVERY
// reject it is set to uCount -- an unreachable index -- so a caller that ignores the
// return value cannot silently address probe 0.
ZM_INTERACT_REJECT ZM_PickInteractTarget(const ZM_InteractProbe* paxProbes, u_int uCount,
	const ZM_InteractOrigin& xOrigin,
	const ZM_InteractTuning& xTuning,
	u_int& uBestIndexOut);

// The player's facing as an XZ-flat UNIT vector: xRotation * (0,0,1), Y zeroed,
// normalised. Returns the ZERO vector for a straight-up / straight-down facing,
// which ZM_PickInteractTarget turns into ZM_INTERACT_REJECT_DEGENERATE_ORIGIN.
//
// It rotates +Z DELIBERATELY, and must never be rewritten in terms of
// glm::eulerAngles(quat).y: that decomposition collapses once the rotation is more
// than 90 degrees off +Z and has already cost this repo a full debugging cycle in
// RenderTest's tennis AI. A unit pins the 180-degree case specifically.
Zenith_Maths::Vector3 ZM_ForwardFromRotation(const Zenith_Maths::Quat& xRotation);
