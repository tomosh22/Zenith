#pragma once

#include "Collections/Zenith_Vector.h"
#include "Flux/Flux_BackendTypes.h"
#include "Core/Multithreading/Zenith_Multithreading.h"
// Flux subsystems profile heavily, and many TUs that include Flux.h relied on
// profiling reaching them transitively through the (now removed) Flux_CommandList.h.
// Keep that transitive provision here so the include graph is behaviour-preserving.
#include "Profiling/Zenith_Profiling.h"

// Flux_SurfaceInfo and view structs (Flux_ShaderResourceView, Flux_*View) +
// Flux_RenderAttachment live in Flux_Types.h (cycle breaks against the backend
// and swapchain headers).
//
// Flux.h is the umbrella header for the renderer's core CPU-side types. Each
// group below lives in its own focused header; include this one to pull them all
// (the historical include footprint), or the specific sub-header for a narrower
// dependency:
//   Flux_RenderResources.h  — Flux_RenderAttachmentCube / Flux_Texture / Flux_Buffer / Flux_RenderAttachmentBuilder
//   Flux_GraphResource.h    — Flux_GraphResource (+ kind), attachment-ref / begin-info carriers, Flux_RenderPassEntry
//   Flux_TransientDesc.h    — Flux_TransientTextureDesc + Flux_TransientHandle / Flux_PassHandle
//   Flux_Pipeline.h         — Flux_PipelineSpecification + Flux_PipelineHelper
//   Flux_WorkDistribution.h — Flux_WorkDistribution
#include "Flux/Flux_RenderResources.h"
#include "Flux/Flux_GraphResource.h"
#include "Flux/Flux_TransientDesc.h"
#include "Flux/Flux_Pipeline.h"
#include "Flux/Flux_WorkDistribution.h"

class Flux_RenderGraph;

// The Flux renderer's public surface moved off the `class Flux` static
// facade onto `Flux_RendererImpl` (held by g_xEngine.FluxRenderer()).
// Migrate Flux::X(...) → g_xEngine.FluxRenderer().X(...). The Impl is
// declared in Flux_RendererImpl.h; including this header keeps you on
// the same include footprint as before, but you'll also want
//   #include "Flux/Flux_RendererImpl.h"
// in TUs that call the methods.
