#pragma once

#include "ZenithECS/Zenith_Entity.h"

#include <cstdint>

// ============================================================================
// DP_Interactables — published by B3.
// Marker for non-behaviour interactables (props the editor flags as
// interactable). Currently a no-op stub; reserved for proximity-only
// world entities that don't carry their own DPInteractable_Behaviour.
// ============================================================================
namespace DP_Interactables
{
	enum class Kind : uint32_t
	{
		Door,
		DoubleDoor,
		Chest,
		Forge,
		Pentagram,
		NoiseMachine
	};

	void MarkAsInteractable(Zenith_EntityID xId, Kind eKind, void* pUserData);

	// Cross-behaviour forwarder for the HUD InteractHint readout. Scans the 5
	// interactable types in the active scene and returns a human-readable type
	// label ("door" / "chest" / "forge" / "pentagram" / "noise machine") for
	// the nearest one within the villager's range of it, or nullptr if none.
	// Moved here from DPHUDController_Behaviour so the HUD header no longer
	// includes the interactable behaviour headers (cross-behaviour rule).
	const char* FindNearestInteractableType(Zenith_EntityID xVillager);
}
