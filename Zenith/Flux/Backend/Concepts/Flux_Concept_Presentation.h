#pragma once

#include "Flux/Flux.h"

// Concept: swapchain / presentation. Static methods on the backend swapchain
// type (aliased as Flux_Swapchain). Returns engine-typed values only —
// width/height as uint32_t, frame index as uint32_t, BeginFrame returns a
// bool indicating "frame is ready to render" (false on transient acquire
// failures like resize).
//
// GetCurrentFrameIndex now reads from Flux_PerFrame::GetRingIndex() so the
// engine has one source of truth for the per-frame ring slot — backends
// must mirror this contract: GetCurrentFrameIndex returns the value that
// matches Flux_PerFrame::GetRingIndex() at call time.

template <typename T>
concept FluxBackendPresentation = requires
{
	{ T::Initialise()                                                          } -> std::same_as<void>;
	{ T::BeginFrame()                                                          } -> std::same_as<bool>;
	{ T::EndFrame()                                                            } -> std::same_as<void>;
	{ T::GetWidth()                                                            } -> std::same_as<uint32_t>;
	{ T::GetHeight()                                                           } -> std::same_as<uint32_t>;
	{ T::GetCurrentFrameIndex()                                                } -> std::same_as<uint32_t>;
};
