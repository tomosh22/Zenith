#pragma once

#include "ZenithECS/Zenith_Entity.h"
#include "DevilsPlayground_Tags.h"

#include <cstdint>

// ============================================================================
// DP_Win — published by B3 (Pentagram).
// ============================================================================
namespace DP_Win
{
	uint32_t GetCollectedObjectivesMask();
	bool HasWon();
	void NotifyObjectiveCollected(DP_ItemTag eObjective,
	                              Zenith_EntityID xVillager  = Zenith_EntityID{},
	                              Zenith_EntityID xPentagram = Zenith_EntityID{});
	void Reset();

	// Cross-behaviour forwarder: returns true if a pentagram is within the
	// villager's F-press range. Mediates DPDoor_Component <-> DPPentagram_Component
	// without the door header including the pentagram header (cross-behaviour
	// rule). XZ squared-distance test against each pentagram's own interact
	// radius; short-circuits on the first hit.
	bool IsPentagramInRange(Zenith_EntityID xVillager);
}
