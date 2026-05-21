#pragma once

// =============================================================================
// DPTutorial -- first-encounter tutorialisation.
//
// The HUD already shows a context-sensitive TutorialHint each frame (via
// DPHUDController::BuildTutorialHintForState). That's "you are HERE,
// here's the next action." This module adds the complementary piece:
// "first time you see X, here's a one-time explanation."
//
// Triggered on the rising-edge of significant events, each tip:
//   1. Shows for ~5 seconds in the TutorialOverlay HUD element.
//   2. Marks itself as "shown" so it doesn't fire again in this run.
//   3. Auto-dismisses on timer expiry OR when a newer tip displaces it.
//
// The "shown" flags reset at run-start (called from
// Project_RegisterScriptBehaviours' between-tests hook + when the
// player triggers Restart from the pause menu).
//
// Architecture: process-global flag table + the active-tip text +
// remaining-time, all read/written via the namespace functions.
// DPHUDController reads GetActiveTipText() / GetActiveTipRemaining()
// each frame to populate the HUD element.
// =============================================================================

#include "EntityComponent/Zenith_Entity.h"

namespace DP_Tutorial
{
	// Tip categories. Each fires AT MOST ONCE per run. Extending the
	// enum is the supported way to add a new tip; the implementation
	// table in DPTutorial.cpp maps each Kind to a text + trigger
	// source.
	enum class Kind : uint8_t
	{
		FirstPossession        = 0,  // Player clicks a villager + possession lands.
		FirstIronPickup        = 1,  // Auto-pickup of an Iron-tagged item.
		FirstKeyCrafted        = 2,  // Forge produces a Key.
		FirstLockedDoor        = 3,  // F-press on a locked door (no matching key).
		FirstDoorUnlocked      = 4,  // Door unlocked successfully (paired with Key).
		FirstObjectivePickup   = 5,  // Auto-pickup of an Objective1..5-tagged item.
		FirstObjectiveDelivered= 6,  // Pentagram accepts an objective.
		FirstPriestSpotted     = 7,  // DP_OnPriestAlerted{SawTarget} rising edge.
		FirstPriestInvestigate = 8,  // DP_OnPriestAlerted{HeardNoise} rising edge.
		FirstBurnout           = 9,  // Possessed villager life-timer expired.
		FirstSprintUse         = 10, // Held Shift while moving -- first sprint frame.
		FirstWalkQuietUse      = 11, // Held Ctrl while moving -- first walk-quiet frame.
		FirstBellSoulRing      = 12, // BellSoul pickup; bell rang.
		FirstBogWaterEvaporate = 13, // BogWater evaporated 8 s after drop.

		COUNT
	};

	// ----- Lifecycle -----

	// Subscribe to DP events that drive tutorial tips. Idempotent.
	// Called from DevilsPlayground::InitializeResources.
	void Initialize();

	// Unsubscribe + free state. Called from
	// DevilsPlayground::CleanupResources.
	void Shutdown();

	// Clear all shown-flags + drop the active tip. Called from the
	// between-tests hook + from DPPauseMenuController on Restart.
	void ResetForNewRun();

	// ----- Programmatic triggers (for tips without their own event) -----

	// Show the tip if it hasn't been shown yet this run. No-op for
	// already-shown kinds + during paused frames. Called from
	// gameplay sites without dedicated events (sprint, walk-quiet,
	// voluntary switch, etc.).
	void TriggerIfFirstTime(Kind eKind);

	// ----- HUD-side reads -----

	// Tick the active-tip timer. Called from DPHUDController each
	// frame; counts down the remaining seconds, clears the active tip
	// when it hits zero.
	void Tick(float fDt);

	// Active tip text + colour for the TutorialOverlay HUD element.
	// Returns nullptr when no tip is active.
	const char* GetActiveTipText();

	// Seconds remaining on the active tip. 0 when no tip is active.
	// HUD uses this for a fade-out cue in the last second.
	float GetActiveTipRemaining();

	// ----- Test accessors (ZENITH_INPUT_SIMULATOR only) -----

	// Has this kind fired at least once this run?
	bool IsTipShown(Kind eKind);

	// Total tips fired this run.
	uint32_t GetShownCount();

	// Per-kind tip text resolver. Pure -- callable without any state.
	// Used by Test_P5Tutorial_TipTextLookup to pin the strings.
	const char* GetTipTextForKind(Kind eKind);

	// Default per-tip display duration (seconds). Shared by all
	// kinds; tunable post-MVP per-kind if pacing requires it.
	constexpr float kDefaultTipDurationSeconds = 5.0f;
}
