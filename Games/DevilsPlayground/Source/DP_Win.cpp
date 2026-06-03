#include "Zenith.h"

#include "DP_Win.h"
#include "DPCommonTypes.h"
#include "DP_Tuning.h"

#include "ZenithECS/Zenith_EventSystem.h"
#include "DevilsPlayground_Tags.h"

#include "../Components/DPPlayerController_Behaviour.h"

#include <cstdint>

namespace DP_Win
{
	uint32_t GetCollectedObjectivesMask()
	{
		const DPPlayerController_Behaviour* pxCtrl = DPPlayerController_Behaviour::Instance();
		if (pxCtrl == nullptr) return 0;
		return pxCtrl->m_uCollectedObjectivesMask;
	}

	bool HasWon()
	{
		const DPPlayerController_Behaviour* pxCtrl = DPPlayerController_Behaviour::Instance();
		if (pxCtrl == nullptr) return false;
		return pxCtrl->m_bHasWon;
	}

	void NotifyObjectiveCollected(DP_ItemTag eObjective,
	                              Zenith_EntityID xVillager,
	                              Zenith_EntityID xPentagram)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_Win::NotifyObjectiveCollected must be called from main thread");
		DPPlayerController_Behaviour* pxCtrl = DPPlayerController_Behaviour::Instance();
		if (pxCtrl == nullptr) return;
		const uint32_t uBit = DP_ObjectiveTagToBit(eObjective);
		if (uBit == 0) return;
		pxCtrl->m_uCollectedObjectivesMask |= uBit;
		// 2026-05-21 balance pass: was `mask == DP_ALL_OBJECTIVES_MASK`
		// (all 5 objectives required), changed to popcount(mask) >=
		// tuning-value. This lets the design ratchet between "5 of 5"
		// and "3 of 5" without touching code.
		const int iRequired = DP_Tuning::Get<int>(
			"night.reagents_required_for_victory");
		// Manual popcount -- avoids std::popcount C++20 header dep.
		int iCollected = 0;
		for (uint32_t u = pxCtrl->m_uCollectedObjectivesMask; u; u >>= 1)
		{
			iCollected += static_cast<int>(u & 1u);
		}
		if (iCollected >= iRequired && !pxCtrl->m_bHasWon)
		{
			pxCtrl->m_bHasWon = true;
			Zenith_EventDispatcher::Get().Dispatch(
				DP_OnVictory{ xVillager, xPentagram });
		}
	}

	void Reset()
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_Win::Reset must be called from main thread");
		DPPlayerController_Behaviour* pxCtrl = DPPlayerController_Behaviour::Instance();
		if (pxCtrl == nullptr) return;
		pxCtrl->m_uCollectedObjectivesMask = 0;
		pxCtrl->m_bHasWon                  = false;
	}
}
