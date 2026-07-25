#pragma once

// ============================================================================
// Full D3D12 null-backend headers (the HEAVY side of the seam for ZENITH_D3D12).
//
// Mirrors Vulkan/Zenith_PlatformGraphics_Include.h. Pulled by Flux_BackendTypes.h
// when ZENITH_D3D12 is defined. The Flux_* aliases (Flux_PlatformAPI = Zenith_D3D12,
// etc.) are already established by Flux_Fwd.h, which Flux_BackendTypes.h includes
// before this header, so we only need the full class definitions here.
//
// The D3D12 backend is a NO-OP "null" backend: it compiles + links against the
// backend-neutral Flux surface to prove the concepts are renderer-agnostic, but
// performs zero real rendering. See Zenith/D3D12/CLAUDE.md.
// ============================================================================

// A real D3D12 backend would include <d3d12.h>, which transitively pulls
// <windows.h>. The null backend has no d3d12.h, so pull <windows.h> directly
// here -- this is the SAME point in the include graph where the Vulkan seam
// pulls it in via vulkan.hpp. Engine files that rely on transitively-available
// Win32 types (DWORD, __except / EXCEPTION_EXECUTE_HANDLER, GetEnvironmentVariableA)
// then compile identically under the D3D12 config. Zenith_Win32.h is the ONE
// place the GLFW-APIENTRY / LEAN_AND_MEAN guards live.
#include "Core/Zenith_Win32.h"

#include "Zenith_D3D12.h"                 // device (Flux_PlatformAPI) + Sampler + VRAM
#include "Zenith_D3D12_MemoryManager.h"   // Flux_MemoryManager
#include "Zenith_D3D12_CommandBuffer.h"   // Flux_CommandBuffer
#include "Zenith_D3D12_Swapchain.h"       // Flux_Swapchain
#include "Zenith_D3D12_Pipeline.h"        // Shader / RootSig / Pipeline + builders
