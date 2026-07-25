#pragma once

// ============================================================================
// Full Null-backend headers (the HEAVY side of the seam for ZENITH_NULL_RENDERER).
//
// Mirrors Vulkan/Zenith_PlatformGraphics_Include.h. Pulled by Flux_BackendTypes.h
// when ZENITH_NULL_RENDERER is defined. The Flux_* aliases (Flux_PlatformAPI = Zenith_Null,
// etc.) are already established by Flux_Fwd.h, which Flux_BackendTypes.h includes
// before this header, so we only need the full class definitions here.
//
// The Null backend performs zero real rendering: it is the GPU-less backend the
// headless/CI builds run on, and it compiles + links against the backend-neutral
// Flux surface. See Zenith/Null/CLAUDE.md.
// ============================================================================

// A real graphics backend pulls <windows.h> transitively via its native header
// (<d3d12.h>, vulkan.hpp). The null backend has no native header, so pull
// <windows.h> directly here -- this is the SAME point in the include graph where
// the Vulkan seam pulls it in via vulkan.hpp. Engine files that rely on
// transitively-available Win32 types (DWORD, __except / EXCEPTION_EXECUTE_HANDLER,
// GetEnvironmentVariableA) then compile identically under the Null config.
// Zenith_Win32.h is the ONE place the GLFW-APIENTRY / LEAN_AND_MEAN guards live.
#include "Core/Zenith_Win32.h"

#include "Zenith_Null.h"                 // device (Flux_PlatformAPI) + Sampler + VRAM
#include "Zenith_Null_MemoryManager.h"   // Flux_MemoryManager
#include "Zenith_Null_CommandBuffer.h"   // Flux_CommandBuffer
#include "Zenith_Null_Swapchain.h"       // Flux_Swapchain
#include "Zenith_Null_Pipeline.h"        // Shader / RootSig / Pipeline + builders
