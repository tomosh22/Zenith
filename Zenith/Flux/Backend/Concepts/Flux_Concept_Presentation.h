#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_RendererImpl.h"

// Concept: swapchain / presentation. Static methods on the backend swapchain
// type (aliased as Flux_Swapchain). Returns engine-typed values only —
// width/height as uint32_t, frame index as uint32_t, BeginFrame returns a
// bool indicating "frame is ready to render" (false on transient acquire
// failures like resize).
//
// GetCurrentFrameIndex reads from g_xEngine.Frame().GetRingIndex() so the
// engine has one source of truth for the per-frame ring slot (FrameContext
// owns the single frame-index variable) — backends must mirror this contract:
// GetCurrentFrameIndex returns the value that matches
// g_xEngine.Frame().GetRingIndex() at call time.

template <typename T>
concept FluxBackendPresentation = requires(T t, uint32_t& uNumColour, Flux_RenderAttachment*& pxDepth)
{
	{ t.Initialise()                                                           } -> std::same_as<void>;
	// BeginFrame / EndFrame are instance methods — the main loop calls them via
	// g_xEngine.FluxSwapchain().BeginFrame() / .EndFrame().
	{ t.BeginFrame()                                                           } -> std::same_as<bool>;
	{ t.EndFrame()                                                             } -> std::same_as<void>;
	{ t.GetWidth()                                                             } -> std::same_as<uint32_t>;
	{ t.GetHeight()                                                            } -> std::same_as<uint32_t>;
	{ t.GetCurrentFrameIndex()                                                 } -> std::same_as<uint32_t>;
	// The acquired backbuffer exposed as a neutral render target. The backend-
	// neutral Flux_Present feature reads its colour format to build the present
	// pipeline; backends MUST expose it so presentation stays render-API-neutral.
	{ t.GetCurrentSwapchainTarget(uNumColour, pxDepth)                         } -> std::same_as<Flux_RenderAttachment*>;
};
