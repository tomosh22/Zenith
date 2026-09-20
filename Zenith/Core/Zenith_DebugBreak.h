#pragma once

#include <atomic>
#include <cstdint>

// Platform-agnostic debug break
// Declaration only - implementation in platform-specific .cpp files
// Windows: Zenith/Windows/Zenith_Windows_DebugBreak.cpp
// Android: Zenith/Android/Zenith_Android_DebugBreak.cpp

void Zenith_DebugBreak();

// Assert-capture hook: when g_bAssertCaptureActive is true, Zenith_DebugBreak
// records a hit instead of halting. Tests set this via Zenith_AssertCaptureScope.
// Defined in the platform DebugBreak .cpp so all builds that compile the platform
// layer automatically pick up the hook.
extern std::atomic<uint32_t> g_uAssertCaptureHitCount;
extern std::atomic<bool>     g_bAssertCaptureActive;
