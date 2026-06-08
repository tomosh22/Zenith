#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_RendererImpl.h"

// Concept: swapchain / presentation. Static methods on the backend swapchain
// type (aliased as Flux_Swapchain). Returns engine-typed values only —
// width/height as uint32_t, frame index as uint32_t, BeginFrame returns a
// bool indicating "frame is ready to render" (false on transient acquire
// failures like resize).
//
// GetCurrentFrameIndex now reads from g_xEngine.FluxRenderer().GetRingIndex() so the
// engine has one source of truth for the per-frame ring slot — backends
// must mirror this contract: GetCurrentFrameIndex returns the value that
// matches g_xEngine.FluxRenderer().GetRingIndex() at call time.

template <typename T>
concept FluxBackendPresentation = requires(T t)
{
	{ t.Initialise()                                                           } -> std::same_as<void>;
	// BeginFrame / EndFrame are instance methods — the main loop calls them via
	// g_xEngine.FluxSwapchain().BeginFrame() / .EndFrame().
	{ t.BeginFrame()                                                           } -> std::same_as<bool>;
	{ t.EndFrame()                                                             } -> std::same_as<void>;
	{ t.GetWidth()                                                             } -> std::same_as<uint32_t>;
	{ t.GetHeight()                                                            } -> std::same_as<uint32_t>;
	{ t.GetCurrentFrameIndex()                                                 } -> std::same_as<uint32_t>;
};
