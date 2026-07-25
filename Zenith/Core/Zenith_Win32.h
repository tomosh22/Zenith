#pragma once

// Centralised <Windows.h> include for the handful of TUs that genuinely use Win32.
//
// W5.2 removed <Windows.h> from the precompiled header (Zenith.h names no Win32
// type). Win32-using TUs include it themselves. They USED to get it transitively
// via vulkan.hpp (pulled by Flux.h), but the GPU-less Null backend and the
// reserved D3D12 backend do not include Vulkan, so the include must be explicit
// -- and it must handle two hazards the accidental Vulkan-side ordering papered
// over:
//
//   * APIENTRY: GLFW (reachable from the PCH via the window layer) #defines it
//     as __stdcall when nothing else has yet. minwindef.h then spells it
//     `#define APIENTRY WINAPI` -- a textually different replacement list, so
//     the redefinition warns C4005, fatal under /WX. GLFW flags its own
//     definition with GLFW_APIENTRY_DEFINED, and that flag is what we key on:
//     undef EXACTLY the GLFW case.
//
//     Do NOT undef an APIENTRY that came from <windows.h> itself. In a TU where
//     the backend seam (Zenith_PlatformGraphics_Include_{Null,D3D12}.h) already
//     pulled <windows.h>, the include below is a no-op against its include
//     guard -- so a blind undef would leave APIENTRY PERMANENTLY undefined and
//     every later Win32 header (commdlg.h, shobjidl.h, psapi.h, ...) would fail
//     to parse. That is a real failure this header has already paid for.
//
//   * WIN32_LEAN_AND_MEAN trims the header; NOMINMAX is already set globally by
//     the build (and Zenith uses its own min/max).
//
// Every Win32-using TU -- and every backend seam header -- should
// `#include "Core/Zenith_Win32.h"` instead of a raw `#include <Windows.h>`.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifdef GLFW_APIENTRY_DEFINED
#undef APIENTRY
#undef GLFW_APIENTRY_DEFINED
#endif

#include <Windows.h>
