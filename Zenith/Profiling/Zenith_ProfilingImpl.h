#pragma once

#include "Collections/Zenith_Vector.h"
#include "Profiling/Zenith_Profiling.h"

#include <chrono>
#include <unordered_map>

// Per-Engine profiling state. Phase 3b moves the 8 non-TLS module
// statics out of Zenith_Profiling.cpp onto this Impl, held by
// Zenith_Engine. The 5 TLS variables (tl_g_uCurrentDepth +
// tl_g_aeIndices + tl_g_aszLabels + tl_g_axStartPoints +
// tl_g_axEndPoints) stay at file scope -- they're per-OS-thread, not
// per-Engine, and represent the in-flight call-stack of nested
// profile regions on the calling thread.
//
// Members are public because the static facade in
// Zenith_Profiling.cpp accesses them directly. The Impl owns the
// data; the facade contains the logic that operates on it. Cleaner
// than wrapping each in a getter/setter when the only caller is the
// existing .cpp implementation.
class Zenith_ProfilingImpl
{
public:
	Zenith_ProfilingImpl() = default;
	~Zenith_ProfilingImpl() = default;

	Zenith_ProfilingImpl(const Zenith_ProfilingImpl&) = delete;
	Zenith_ProfilingImpl& operator=(const Zenith_ProfilingImpl&) = delete;

	std::unordered_map<u_int, Zenith_Vector<Zenith_Profiling::Event>> m_xEvents;
	std::unordered_map<u_int, Zenith_Vector<Zenith_Profiling::Event>> m_xPreviousFrameEvents;
	Zenith_Mutex_NoProfiling                                          m_xEventsMutex;
	std::chrono::time_point<std::chrono::high_resolution_clock>       m_xFrameStart;
	std::chrono::time_point<std::chrono::high_resolution_clock>       m_xFrameEnd;
	std::chrono::time_point<std::chrono::high_resolution_clock>       m_xPreviousFrameStart;
	std::chrono::time_point<std::chrono::high_resolution_clock>       m_xPreviousFrameEnd;

	// Pause latch. Toggled at frame boundaries by EndFrame so the
	// recorded final frame is internally consistent vs. the ImGui
	// checkbox toggling mid-frame. See Zenith_Profiling.cpp for the
	// dbg_bPauseRequested coupling logic.
	bool m_bPauseEffective = false;
};
