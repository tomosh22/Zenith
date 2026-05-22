#pragma once

#include "EntityComponent/Zenith_Entity.h"

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
}
