#pragma once

// ============================================================================
// Flux_BackendGuard
//
// Enforces that EXACTLY ONE render backend is selected at compile time. The
// backend is chosen by the Sharpmake RenderBackend fragment, which defines
// either ZENITH_VULKAN or ZENITH_D3D12 (never both, never neither). Every
// header that resolves a Flux_* backend alias includes this guard, so a
// mis-configured build fails here with a clear message instead of via a
// downstream cascade of "undeclared identifier" / ambiguous-alias errors.
// ============================================================================

#if defined(ZENITH_VULKAN) && defined(ZENITH_D3D12)
#error "Flux: both ZENITH_VULKAN and ZENITH_D3D12 are defined - select exactly one render backend."
#endif

#if !defined(ZENITH_VULKAN) && !defined(ZENITH_D3D12)
#error "Flux: no render backend defined - define exactly one of ZENITH_VULKAN / ZENITH_D3D12."
#endif
