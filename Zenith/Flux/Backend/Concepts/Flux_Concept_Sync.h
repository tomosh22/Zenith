#pragma once

#include "Flux/Flux.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

// Concept: graph-emitted barrier emission. The render graph synthesises a
// portable Flux_RenderGraph_Barrier list per pass during Compile() and the
// backend command-buffer consumes that list through a single neutral entry
// point: ResourceBarrier. It takes the tagged Flux_GraphResource carrier (which
// knows whether it wraps an image or a buffer) plus a Flux_SubresourceRange and
// ResourceAccess enums; the backend dispatches on the resource kind internally
// to an image layout transition or a buffer memory barrier. The backend's own
// image/buffer barrier helpers + the vk-typed ImageTransitionBarrier variants
// are NOT in this concept — they are backend-internal.

template <typename T>
concept FluxBackendSync = requires(
	T& xRec,
	const Flux_SubresourceRange& xRange,
	ResourceAccess eSrcAccess,
	ResourceAccess eDstAccess,
	const Flux_GraphResource& xRes)
{
	{ xRec.ResourceBarrier(xRes, xRange, eSrcAccess, eDstAccess) } -> std::same_as<void>;
};
