#include "Zenith.h"

#include "Zenithmon/Source/Save/ZM_Autosave.h"

#include "Core/Zenith_Result.h"

#include "Zenithmon/Components/ZM_GameStateManager.h"
#include "Zenithmon/Components/ZM_UI_MenuStack.h"
#include "Zenithmon/Source/Party/ZM_GameState.h"

// ============================================================================
// ZM_Autosave -- see the header for the contract.
//
// The ONE ordering that matters in this file: ZM_TryAutosave asks
// ZM_ShouldAutosave FIRST and only then captures and writes. Capturing before
// the policy check would mutate the live ZM_GameState's m_xWorldPosition on
// every refused attempt -- silently overwriting a good saved position with the
// pose the player happens to be standing in while a battle owns the screen.
//
// It also explains why this cannot fire from inside the warp's fade-in tail:
// ZM_SaveSlots::ResolveLiveSaveBlocker consults
// ZM_GameStateManager::IsWarpInProgress(), which is true for EVERY non-IDLE
// transition state, so an in-tail autosave always resolves WARP and silently
// never saves. The manager latches a one-shot and drains it from OnUpdate once
// the machine is IDLE.
// ============================================================================

namespace
{
	// A process-global counter, and deliberately so: autosaves are counted for the
	// whole run, not per manager instance, and the manager is destroyed and
	// re-authored across scene loads. ZM_ResetAutosaveForTests is what keeps it
	// from leaking between batched tests, and it is called from
	// ZM_GameStateManager::ResetRuntimeStateForTests ABOVE that function's
	// no-manager early-out for exactly that reason.
	u_int s_uAutosaveCount = 0u;
}

const char* ZM_AutosaveTriggerName(ZM_AUTOSAVE_TRIGGER eTrigger)
{
	switch (eTrigger)
	{
	case ZM_AUTOSAVE_TRIGGER_NONE:                return "NONE";
	case ZM_AUTOSAVE_TRIGGER_SCENE_ENTERED:       return "SCENE_ENTERED";
	case ZM_AUTOSAVE_TRIGGER_BADGE_EARNED:        return "BADGE_EARNED";
	case ZM_AUTOSAVE_TRIGGER_STORY_FLAG_SET:      return "STORY_FLAG_SET";
	case ZM_AUTOSAVE_TRIGGER_LEAGUE_ENTERED:      return "LEAGUE_ENTERED";
	case ZM_AUTOSAVE_TRIGGER_TOWER_STREAK_BANKED: return "TOWER_STREAK_BANKED";
	// A switch, not a table lookup, so COUNT and anything past it land here rather
	// than indexing off the end of an array.
	default:                                      return "UNKNOWN";
	}
}

bool ZM_IsAutosaveTriggerLive(ZM_AUTOSAVE_TRIGGER eTrigger)
{
	// Exactly one arm. Adding a second one here is the LAST step of shipping a
	// milestone, not the first -- a live trigger with no producer is dead code, and
	// a live trigger whose producer is half-written autosaves from a system that
	// does not exist yet.
	switch (eTrigger)
	{
	case ZM_AUTOSAVE_TRIGGER_SCENE_ENTERED:
		return true;
	default:
		return false;
	}
}

bool ZM_ShouldAutosave(ZM_AUTOSAVE_TRIGGER eTrigger, ZM_SaveSlots::ZM_SAVE_BLOCKER eBlocker,
	bool bMenuOpen)
{
	if (!ZM_IsAutosaveTriggerLive(eTrigger))
	{
		return false;
	}
	// Any blocker at all refuses -- this predicate deliberately does NOT inspect
	// WHICH one, so a blocker appended to ZM_SAVE_BLOCKER later is honoured here
	// with no edit. Comparing against NONE rather than listing the arms is what
	// makes that true.
	if (eBlocker != ZM_SaveSlots::ZM_SAVE_BLOCKER_NONE)
	{
		return false;
	}
	// The one condition autosave adds on top of the shared policy. The manual save
	// flow is reached THROUGH the menu, so an autosave firing underneath an open
	// menu would race the player's own slot choice.
	return !bMenuOpen;
}

bool ZM_TryAutosave(ZM_AUTOSAVE_TRIGGER eTrigger)
{
	const ZM_SaveSlots::ZM_SAVE_BLOCKER eBlocker = ZM_SaveSlots::ResolveLiveSaveBlocker();
	const bool bMenuOpen = ZM_UI_MenuStack::IsMenuOpen();
	if (!ZM_ShouldAutosave(eTrigger, eBlocker, bMenuOpen))
	{
		// Not an error: a refused autosave is the ordinary outcome of quitting to the
		// title, arriving mid-whiteout, or arriving with the menu open.
		return false;
	}

	ZM_GameState* pxState = nullptr;
	if (!ZM_GameStateManager::TryGetGameState(pxState) || pxState == nullptr)
	{
		return false;
	}

	// Capture BEFORE the write and only after the policy passed. A failed capture
	// aborts the whole autosave rather than writing a stale or UNSET position over
	// a good one.
	if (!ZM_GameStateManager::CaptureWorldPosition(*pxState))
	{
		Zenith_Error(LOG_CATEGORY_GAMEPLAY,
			"[ZM Autosave] '%s': could not capture the player's world position -- "
			"nothing was written", ZM_AutosaveTriggerName(eTrigger));
		return false;
	}

	const Zenith_Status xStatus = ZM_SaveSlots::WriteState(*pxState, ZM_SAVE_SLOT_AUTO);
	if (!xStatus.IsOk())
	{
		// Logged once and NEVER retried. The caller's latch is already spent, so a
		// disk that stays full costs one line per milestone rather than one per frame.
		Zenith_Error(LOG_CATEGORY_GAMEPLAY,
			"[ZM Autosave] '%s': writing the Auto slot failed",
			ZM_AutosaveTriggerName(eTrigger));
		return false;
	}

	++s_uAutosaveCount;
	return true;
}

u_int ZM_GetAutosaveCount()
{
	return s_uAutosaveCount;
}

#ifdef ZENITH_INPUT_SIMULATOR
void ZM_ResetAutosaveForTests()
{
	s_uAutosaveCount = 0u;
}
#endif
