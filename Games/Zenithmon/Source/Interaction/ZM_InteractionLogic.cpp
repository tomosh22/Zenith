#include "Zenith.h"

#include "Zenithmon/Source/Interaction/ZM_InteractionLogic.h"

// ============================================================================
// ZM_InteractionLogic (S6 item 3 SC1). See the header for the contract; this file
// is the ORDER. Nothing here touches the ECS, the scene, the UI or the disk.
// ============================================================================

ZM_INTERACT_REJECT ZM_ShouldInteract(bool bPressed, bool bMenuOpen, bool bOverworld,
	bool bWarpInProgress, bool bBattleTransitionActive,
	bool bPlayerMovementEnabled)
{
	// The sequence IS the specification: the first blocker that holds is the one
	// reported, so each early-out below must stay exactly where it is.
	if (!bPressed)                  { return ZM_INTERACT_REJECT_NO_INPUT_EDGE; }
	if (bMenuOpen)                  { return ZM_INTERACT_REJECT_MENU_OPEN; }
	if (!bOverworld)                { return ZM_INTERACT_REJECT_NOT_OVERWORLD; }
	if (bWarpInProgress)            { return ZM_INTERACT_REJECT_WARP_IN_PROGRESS; }
	if (bBattleTransitionActive)    { return ZM_INTERACT_REJECT_BATTLE_TRANSITION; }
	if (!bPlayerMovementEnabled)    { return ZM_INTERACT_REJECT_PLAYER_FROZEN; }
	return ZM_INTERACT_OK;
}

const char* ZM_InteractRejectName(ZM_INTERACT_REJECT eReject)
{
	switch (eReject)
	{
	case ZM_INTERACT_OK:                          return "OK";
	case ZM_INTERACT_REJECT_NO_INPUT_EDGE:        return "NO_INPUT_EDGE";
	case ZM_INTERACT_REJECT_MENU_OPEN:            return "MENU_OPEN";
	case ZM_INTERACT_REJECT_NOT_OVERWORLD:        return "NOT_OVERWORLD";
	case ZM_INTERACT_REJECT_WARP_IN_PROGRESS:     return "WARP_IN_PROGRESS";
	case ZM_INTERACT_REJECT_BATTLE_TRANSITION:    return "BATTLE_TRANSITION";
	case ZM_INTERACT_REJECT_PLAYER_FROZEN:        return "PLAYER_FROZEN";
	case ZM_INTERACT_REJECT_NO_CANDIDATE:         return "NO_CANDIDATE";
	case ZM_INTERACT_REJECT_OUT_OF_RANGE:         return "OUT_OF_RANGE";
	case ZM_INTERACT_REJECT_OUT_OF_VERTICAL_BAND: return "OUT_OF_VERTICAL_BAND";
	case ZM_INTERACT_REJECT_NOT_FACING:           return "NOT_FACING";
	case ZM_INTERACT_REJECT_DEGENERATE_ORIGIN:    return "DEGENERATE_ORIGIN";
	// A switch, not a table lookup, precisely so COUNT and anything past it land
	// here instead of reading off the end of an array.
	default:                                      return "UNKNOWN";
	}
}
