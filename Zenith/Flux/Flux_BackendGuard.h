#pragma once

// ============================================================================
// Flux_BackendGuard
//
// Enforces that EXACTLY ONE render backend is selected at compile time. The
// backend is chosen by the Sharpmake RenderBackend fragment, which defines
// exactly one of ZENITH_VULKAN / ZENITH_D3D12 / ZENITH_NULL_RENDERER (never
// more, never none). Every header that resolves a Flux_* backend alias includes
// this guard, so a mis-configured build fails here with a clear message instead
// of via a downstream cascade of "undeclared identifier" / ambiguous-alias
// errors.
// ============================================================================

// `defined()` is not portable inside a macro consumed by #if, so the count is
// assembled from three plain object-like macros instead.
#if defined(ZENITH_VULKAN)
#define ZENITH_BACKEND_BIT_VULKAN 1
#else
#define ZENITH_BACKEND_BIT_VULKAN 0
#endif

#if defined(ZENITH_D3D12)
#define ZENITH_BACKEND_BIT_D3D12 1
#else
#define ZENITH_BACKEND_BIT_D3D12 0
#endif

#if defined(ZENITH_NULL_RENDERER)
#define ZENITH_BACKEND_BIT_NULL 1
#else
#define ZENITH_BACKEND_BIT_NULL 0
#endif

#if (ZENITH_BACKEND_BIT_VULKAN + ZENITH_BACKEND_BIT_D3D12 + ZENITH_BACKEND_BIT_NULL) > 1
#error "Flux: more than one render backend is defined - select exactly one of ZENITH_VULKAN / ZENITH_D3D12 / ZENITH_NULL_RENDERER."
#endif

#if (ZENITH_BACKEND_BIT_VULKAN + ZENITH_BACKEND_BIT_D3D12 + ZENITH_BACKEND_BIT_NULL) < 1
#error "Flux: no render backend defined - define exactly one of ZENITH_VULKAN / ZENITH_D3D12 / ZENITH_NULL_RENDERER."
#endif
