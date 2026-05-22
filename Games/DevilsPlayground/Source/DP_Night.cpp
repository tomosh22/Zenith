#include "Zenith.h"

#include "DP_Night.h"
#include "DP_Player.h"
#include "DPCommonTypes.h"

#include "EntityComponent/Zenith_EventSystem.h"

namespace
{
	// Per-run timer state. Set by StartNight, ticked by TickNight,
	// cleared by Reset. The dispatch flag prevents repeat DP_OnRunLost
	// emits if TickNight is called multiple frames after timer hits 0.
	float g_fNightRemainingSec = 0.0f;
	bool  g_bNightActive       = false;
	bool  g_bDawnDispatched    = false;
}

namespace DP_Night
{
	void StartNight(float fDurationSeconds)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_Night::StartNight must be called from main thread");
		g_fNightRemainingSec = (fDurationSeconds > 0.0f) ? fDurationSeconds : 0.0f;
		g_bNightActive       = true;
		// Re-arm: a new run begins, even if a prior dawn already
		// dispatched.
		g_bDawnDispatched    = false;
	}

	void TickNight(float fDt)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_Night::TickNight must be called from main thread");
		if (!g_bNightActive)        return;
		if (g_bDawnDispatched)
		{
			// Stay at 0; do not dispatch again until StartNight()
			// or Reset() re-arms. Pinning the remaining time at 0
			// (rather than letting it drift negative) keeps the
			// GetNightTimeRemaining() readout stable for HUD use.
			g_fNightRemainingSec = 0.0f;
			return;
		}
		g_fNightRemainingSec -= fDt;
		if (g_fNightRemainingSec <= 0.0f)
		{
			g_fNightRemainingSec = 0.0f;
			g_bDawnDispatched    = true;
			// Forward the currently-possessed villager (best-effort
			// world position context for the visualiser). May be INVALID
			// if dawn hit between possessions; that's fine.
			Zenith_EventDispatcher::Get().Dispatch(
				DP_OnRunLost{
					DP_RunLostCause::Dawn,
					DP_Player::GetPossessedVillager(),
					Zenith_EntityID{} });
		}
	}

	float GetNightTimeRemaining()
	{
		return g_fNightRemainingSec;
	}

	bool IsNightActive()
	{
		return g_bNightActive;
	}

	bool HasDawnReached()
	{
		return g_bDawnDispatched;
	}

	void Reset()
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_Night::Reset must be called from main thread");
		g_fNightRemainingSec = 0.0f;
		g_bNightActive       = false;
		g_bDawnDispatched    = false;
	}
}
