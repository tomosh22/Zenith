#pragma once

// ============================================================================
// Flux_BackendTypes (the HEAVY backend seam)
//
// Pulls the FULL concrete backend headers that sit behind the Flux_* aliases.
// Include this in method-call sites (.cpp files, Flux.h) that actually invoke
// methods on the backend types. Declaration-only consumers should include the
// LIGHT Flux_Fwd.h instead (forward-decls + aliases, no full headers), so the
// engine header footprint stays small. See Flux_Backend.h for the contract.
// ============================================================================

#include "Flux/Flux_Fwd.h"   // backend guard + forward-decls + Flux_* aliases

#if defined(ZENITH_VULKAN)
#include "Zenith_PlatformGraphics_Include.h"          // full Vulkan backend headers
#elif defined(ZENITH_D3D12)
#include "Zenith_PlatformGraphics_Include_D3D12.h"    // full D3D12 null-backend headers
#endif
