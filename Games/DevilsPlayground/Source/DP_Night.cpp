#include "Zenith.h"

#include "DP_Night.h"
#include "DP_Player.h"
#include "DPCommonTypes.h"

#include "ZenithECS/Zenith_EventSystem.h"

#include "../Components/DPPlayerController_Behaviour.h"

namespace DP_Night
{
	void StartNight(float fDurationSeconds)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_Night::StartNight must be called from main thread");
		DPPlayerController_Behaviour* pxCtrl = DPPlayerController_Behaviour::Instance();
		if (pxCtrl == nullptr) return;
		pxCtrl->m_fNightRemainingSec = (fDurationSeconds > 0.0f) ? fDurationSeconds : 0.0f;
		pxCtrl->m_bNightActive       = true;
		// Re-arm: a new run begins, even if a prior dawn already
		// dispatched.
		pxCtrl->m_bDawnDispatched    = false;
	}

	void TickNight(float fDt)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_Night::TickNight must be called from main thread");
		DPPlayerController_Behaviour* pxCtrl = DPPlayerController_Behaviour::Instance();
		if (pxCtrl == nullptr || !pxCtrl->m_bNightActive) return;
		if (pxCtrl->m_bDawnDispatched)
		{
			// Stay at 0; do not dispatch again until StartNight() or
			// Reset() re-arms.
			pxCtrl->m_fNightRemainingSec = 0.0f;
			return;
		}
		pxCtrl->m_fNightRemainingSec -= fDt;
		if (pxCtrl->m_fNightRemainingSec <= 0.0f)
		{
			pxCtrl->m_fNightRemainingSec = 0.0f;
			pxCtrl->m_bDawnDispatched    = true;
			Zenith_EventDispatcher::Get().Dispatch(
				DP_OnRunLost{
					DP_RunLostCause::Dawn,
					DP_Player::GetPossessedVillager(),
					Zenith_EntityID{} });
		}
	}

	float GetNightTimeRemaining()
	{
		const DPPlayerController_Behaviour* pxCtrl = DPPlayerController_Behaviour::Instance();
		if (pxCtrl == nullptr) return 0.0f;
		return pxCtrl->m_fNightRemainingSec;
	}

	// "Night mode is running" -- stays true even after the dawn timer hits
	// zero and DP_OnRunLost{Dawn} has been dispatched (the flag is only
	// cleared by Reset() / a fresh StartNight). Callers that need the dawn
	// EDGE specifically should use HasDawnReached() instead.
	bool IsNightActive()
	{
		const DPPlayerController_Behaviour* pxCtrl = DPPlayerController_Behaviour::Instance();
		if (pxCtrl == nullptr) return false;
		return pxCtrl->m_bNightActive;
	}

	bool HasDawnReached()
	{
		const DPPlayerController_Behaviour* pxCtrl = DPPlayerController_Behaviour::Instance();
		if (pxCtrl == nullptr) return false;
		return pxCtrl->m_bDawnDispatched;
	}

	void Reset()
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_Night::Reset must be called from main thread");
		DPPlayerController_Behaviour* pxCtrl = DPPlayerController_Behaviour::Instance();
		if (pxCtrl == nullptr) return;
		pxCtrl->m_fNightRemainingSec = 0.0f;
		pxCtrl->m_bNightActive       = false;
		pxCtrl->m_bDawnDispatched    = false;
	}
}
