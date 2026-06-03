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
}
