#pragma once

// Centralised <Windows.h> include for the handful of TUs that genuinely use Win32.
//
// W5.2 removed <Windows.h> from the precompiled header (Zenith.h names no Win32
// type). Win32-using TUs include it themselves. They USED to get it transitively
// via vulkan.hpp (pulled by Flux.h), but the D3D12 null backend does not include
// Vulkan, so the include must be explicit -- and it must handle two hazards that
// the accidental Vulkan-side ordering papered over:
//
//   * APIENTRY: GLFW (reachable via Zenith_Input.h / the window layer) #defines
//     it. In the Vulkan build vulkan.hpp's <windows.h> sets APIENTRY=WINAPI
//     before GLFW, so GLFW's guarded #define is skipped and there is no clash.
//     Under D3D12 nothing resets it, so minwindef.h's `#define APIENTRY WINAPI`
//     warns C4005 (fatal under /WX). Undef first so <windows.h> defines it cleanly.
//   * WIN32_LEAN_AND_MEAN trims the header; NOMINMAX is already set globally by
//     the build (and Zenith uses its own min/max).
//
// Every Win32-using TU should `#include "Core/Zenith_Win32.h"` instead of a raw
// `#include <Windows.h>`.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifdef APIENTRY
#undef APIENTRY
#endif
#include <Windows.h>
