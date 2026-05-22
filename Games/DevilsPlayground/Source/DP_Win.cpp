#include "Zenith.h"

#include "DP_Win.h"
#include "DPCommonTypes.h"
#include "DP_Tuning.h"

#include "EntityComponent/Zenith_EventSystem.h"
#include "DevilsPlayground_Tags.h"

#include <cstdint>

namespace
{
	// ---- DP_Win state (B3 Pentagram fills in) ----
	uint32_t g_uCollectedObjectivesMask = 0;
	bool     g_bHasWon = false;
}

namespace DP_Win
{
	uint32_t GetCollectedObjectivesMask()
	{
		return g_uCollectedObjectivesMask;
	}

	bool HasWon()
	{
		return g_bHasWon;
	}

	void NotifyObjectiveCollected(DP_ItemTag eObjective,
	                              Zenith_EntityID xVillager,
	                              Zenith_EntityID xPentagram)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_Win::NotifyObjectiveCollected must be called from main thread");
		const uint32_t uBit = DP_ObjectiveTagToBit(eObjective);
		if (uBit == 0) return;
		g_uCollectedObjectivesMask |= uBit;
		// 2026-05-21 balance pass: was `mask == DP_ALL_OBJECTIVES_MASK`
		// (all 5 objectives required), changed to popcount(mask) >=
		// tuning-value. This lets the design ratchet between "5 of 5"
		// and "3 of 5" without touching code.
		const int iRequired = DP_Tuning::Get<int>(
			"night.reagents_required_for_victory");
		// Manual popcount -- avoids std::popcount C++20 header dep.
		int iCollected = 0;
		for (uint32_t u = g_uCollectedObjectivesMask; u; u >>= 1)
		{
			iCollected += static_cast<int>(u & 1u);
		}
		if (iCollected >= iRequired && !g_bHasWon)
		{
			g_bHasWon = true;
			Zenith_EventDispatcher::Get().Dispatch(
				DP_OnVictory{ xVillager, xPentagram });
		}
	}

	void Reset()
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_Win::Reset must be called from main thread");
		g_uCollectedObjectivesMask = 0;
		g_bHasWon = false;
	}
}
