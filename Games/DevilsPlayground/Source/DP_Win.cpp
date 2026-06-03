#include "Zenith.h"

#include "DP_Win.h"
#include "DPCommonTypes.h"
#include "DP_Tuning.h"
#include "DP_Query.h"

#include "ZenithECS/Zenith_EventSystem.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Maths/Zenith_Maths.h"
#include "DevilsPlayground_Tags.h"

#include "../Components/DPPlayerController_Behaviour.h"
#include "../Components/DPPentagram_Behaviour.h"

#include <cstdint>
#include <bit>

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
		// Count set objective bits (3-of-5 default victory threshold).
		const int iCollected =
			static_cast<int>(std::popcount(pxCtrl->m_uCollectedObjectivesMask));
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

	// 2026-05-26: returns true if a Pentagram is within the villager's
	// F-press range. Moved here from DPDoor_Behaviour::IsPentagramInRange
	// so the door header no longer needs to include the pentagram header
	// (cross-behaviour rule). Used to defer the door's close-on-F-press
	// when the same F-press is targeting the adjacent pentagram.
	bool IsPentagramInRange(Zenith_EntityID xVillager)
	{
		if (!xVillager.IsValid()) return false;
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xVillager);
		if (pxScene == nullptr) return false;
		Zenith_Entity xV = pxScene->TryGetEntity(xVillager);
		if (!xV.IsValid() || !xV.HasComponent<Zenith_TransformComponent>()) return false;
		Zenith_Maths::Vector3 xVPos;
		xV.GetComponent<Zenith_TransformComponent>().GetPosition(xVPos);

		bool bInRange = false;
		DP_Query::ForEachScriptInActiveScene<DPPentagram_Behaviour>(
			[&bInRange, &xVPos, pxScene](Zenith_EntityID xId, DPPentagram_Behaviour& xPent)
			{
				if (bInRange) return;  // already found one
				const float fR = xPent.GetInteractRadius();
				Zenith_Entity xP = pxScene->TryGetEntity(xId);
				if (!xP.IsValid() || !xP.HasComponent<Zenith_TransformComponent>()) return;
				Zenith_Maths::Vector3 xPPos;
				xP.GetComponent<Zenith_TransformComponent>().GetPosition(xPPos);
				const float fDx = xVPos.x - xPPos.x;
				const float fDz = xVPos.z - xPPos.z;
				if (fDx * fDx + fDz * fDz <= fR * fR) bInRange = true;
			});
		return bInRange;
	}
}
