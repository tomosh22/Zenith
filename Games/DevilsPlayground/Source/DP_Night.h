#pragma once

// ============================================================================
// DP_Night (MVP-1.3.5 Dawn half) — night-timer countdown. When the
// timer expires, dispatches DP_OnRunLost{Dawn} EXACTLY ONCE.
// ============================================================================
namespace DP_Night
{
	void StartNight(float fDurationSeconds);
	void TickNight(float fDt);
	float GetNightTimeRemaining();
	bool IsNightActive();
	bool HasDawnReached();
	void Reset();
}
