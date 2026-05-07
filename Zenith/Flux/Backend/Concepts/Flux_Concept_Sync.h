#pragma once

#include "Flux/Flux.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

// Concept: graph-emitted barrier emission. The render graph synthesises a
// portable Flux_RenderGraph_Barrier list per pass during Compile() and the
// backend command-buffer consumes that list via these engine-typed
// ImageTransition overloads.
//
// Both overloads take engine-typed inputs (Flux_RenderAttachment* or the
// tagged Flux_GraphResource carrier) and ResourceAccess enums. The vk-typed
// ImageTransitionBarrier / ImageTransitionBarrierRange overloads on the
// Vulkan backend are intentionally NOT in this concept — they are
// backend-internal helpers that the engine layer must not reach for.

template <typename T>
concept FluxBackendSync = requires(
	T& xRec,
	uint32_t uBaseMip,
	uint32_t uMipCount,
	uint32_t uBaseLayer,
	uint32_t uLayerCount,
	ResourceAccess eSrcAccess,
	ResourceAccess eDstAccess,
	Flux_RenderAttachment* pxAttachment,
	const Flux_GraphResource& xRes)
{
	// Per-attachment subresource transition (used directly by techniques
	// in pre-graph-barrier-synthesis code paths; still useful for any
	// pass that needs an explicit mid-frame transition).
	{ xRec.ImageTransition(pxAttachment,
	                       uBaseMip, uMipCount, uBaseLayer, uLayerCount,
	                       eSrcAccess, eDstAccess)                             } -> std::same_as<void>;

	// Polymorphic overload — handles both 2D images and cubes via the
	// graph-resource tag. Used by the render-graph prologue-barrier emitter
	// inside RecordCommandBuffersTask.
	{ xRec.ImageTransition(xRes,
	                       uBaseMip, uMipCount, uBaseLayer, uLayerCount,
	                       eSrcAccess, eDstAccess)                             } -> std::same_as<void>;
};
