#pragma once

// Zenith_Core namespace - provides global frame timing and main loop
// Note: Converted from static-only class to namespace for idiomatic C++
// (classes should have instance state; use namespaces for grouping related functions)
namespace Zenith_Core
{
	// Frame timing accessors (inline for performance)
	inline void SetDt(const float fDt);
	inline float GetDt();
	inline void AddTimePassed(const float fDt);
	inline float GetTimePassed();

	// Main loop and render synchronization
	void Zenith_MainLoop();
	void UpdateTimers();

	// Wait for all render tasks to complete
	// Used by editor to ensure render tasks finish before scene transitions
	void WaitForAllRenderTasks();

	// Frame timing state (definitions in Zenith_Core.cpp)
	extern float g_fDt;
	extern float g_fTimePassed;
	extern std::chrono::high_resolution_clock::time_point g_xLastFrameTime;

	// Inline implementations
	inline void SetDt(const float fDt) { g_fDt = fDt; }
	inline float GetDt() { return g_fDt; }
	inline void AddTimePassed(const float fDt) { g_fTimePassed += fDt; }
	inline float GetTimePassed() { return g_fTimePassed; }
}