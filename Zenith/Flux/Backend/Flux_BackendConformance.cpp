#include "Zenith.h"

#include "Flux/Flux_RendererImpl.h"
// Build-time conformance check: the active backend (selected via macro alias
// in Zenith_PlatformGraphics_Include.h) must satisfy every concept in
// Flux_Backend.h. If a backend method is renamed, removed, or has its
// signature changed in a way that diverges from the concept, the build
// fails here at the static_assert with a readable concept-failure
// diagnostic — much earlier than discovering the drift via crash on the
// first frame.
//
// Adding a second backend (DX12 / Metal / WebGPU): include its headers,
// add static_assert lines using the new backend's class names. The build
// fails on missing methods, surfacing the exact conformance gap.
//
// Required initialisation sequence (not expressible as a concept — document
// here so a new backend author doesn't discover it via crash):
//   1. g_xEngine.FluxRenderer().PerFrameInitialise()              (resets counter + callback arrays)
//   2. Backend Initialise()                     (backend may register callbacks here)
//   3. MemoryManager Initialise()               (registers OnFluxPerFrameEnd
//                                                AFTER the backend's begin-frame
//                                                callback, so deferred-deletion
//                                                drains at end-of-frame after any
//                                                in-flight submission is queued)
// And at shutdown, the reverse order:
//   1. MemoryManager Shutdown()                 (drains deferred deletions)
//   2. Backend Shutdown()
//   3. g_xEngine.FluxRenderer().PerFrameShutdown()                (clears callback arrays)
//
// Callback-registration ordering inside Initialise is load-bearing: the
// Vulkan begin-frame callback (fence wait, descriptor pool reset, typed
// deletion drain, scratch reset) MUST run before any other begin-frame
// subscriber, so backends register that callback first. See
// Flux_PerFrame.cpp for the subscriber-tally static_assert that catches
// new subscribers being added without updating the cap in ZenithConfig.h.

#include "Flux/Flux_Backend.h"

// Pull in the concrete backend classes so the concepts can substitute.

// ---- Vulkan backend conformance --------------------------------------------

static_assert(FluxBackendDevice                <Zenith_Vulkan>,
	"Zenith_Vulkan does not satisfy FluxBackendDevice");

static_assert(FluxBackendMemoryAlloc           <Zenith_Vulkan_MemoryManager>,
	"Zenith_Vulkan_MemoryManager does not satisfy FluxBackendMemoryAlloc");

static_assert(FluxBackendMemoryDelete          <Zenith_Vulkan_MemoryManager>,
	"Zenith_Vulkan_MemoryManager does not satisfy FluxBackendMemoryDelete");

// Check the umbrella concept AND each sub-concept individually. A failure on
// a sub-concept names the specific capability that regressed, which is much
// more actionable than "backend does not satisfy FluxBackendCommandRecorder"
// when the culprit is one missing Draw* overload.
static_assert(FluxBackendCommandRecorder       <Zenith_Vulkan_CommandBuffer>,
	"Zenith_Vulkan_CommandBuffer does not satisfy FluxBackendCommandRecorder (umbrella)");
static_assert(FluxBackendRecordingLifecycle    <Zenith_Vulkan_CommandBuffer>,
	"Zenith_Vulkan_CommandBuffer does not satisfy FluxBackendRecordingLifecycle");
static_assert(FluxBackendPipelineBinding       <Zenith_Vulkan_CommandBuffer>,
	"Zenith_Vulkan_CommandBuffer does not satisfy FluxBackendPipelineBinding");
static_assert(FluxBackendVertexIndexStreams    <Zenith_Vulkan_CommandBuffer>,
	"Zenith_Vulkan_CommandBuffer does not satisfy FluxBackendVertexIndexStreams");
static_assert(FluxBackendBasicDraws            <Zenith_Vulkan_CommandBuffer>,
	"Zenith_Vulkan_CommandBuffer does not satisfy FluxBackendBasicDraws");
static_assert(FluxBackendIndirectDraws         <Zenith_Vulkan_CommandBuffer>,
	"Zenith_Vulkan_CommandBuffer does not satisfy FluxBackendIndirectDraws");
static_assert(FluxBackendCompute               <Zenith_Vulkan_CommandBuffer>,
	"Zenith_Vulkan_CommandBuffer does not satisfy FluxBackendCompute");
static_assert(FluxBackendResourceBinding       <Zenith_Vulkan_CommandBuffer>,
	"Zenith_Vulkan_CommandBuffer does not satisfy FluxBackendResourceBinding");
static_assert(FluxBackendDynamicState          <Zenith_Vulkan_CommandBuffer>,
	"Zenith_Vulkan_CommandBuffer does not satisfy FluxBackendDynamicState");
#ifdef ZENITH_FLUX_PROFILING
static_assert(FluxBackendDebugMarkers          <Zenith_Vulkan_CommandBuffer>,
	"Zenith_Vulkan_CommandBuffer does not satisfy FluxBackendDebugMarkers");
#endif

static_assert(FluxBackendSync                  <Zenith_Vulkan_CommandBuffer>,
	"Zenith_Vulkan_CommandBuffer does not satisfy FluxBackendSync");

static_assert(FluxBackendPresentation          <Zenith_Vulkan_Swapchain>,
	"Zenith_Vulkan_Swapchain does not satisfy FluxBackendPresentation");

static_assert(FluxBackendShader                <Zenith_Vulkan_Shader>,
	"Zenith_Vulkan_Shader does not satisfy FluxBackendShader");

static_assert(FluxBackendPipelineBuilder       <Zenith_Vulkan_PipelineBuilder>,
	"Zenith_Vulkan_PipelineBuilder does not satisfy FluxBackendPipelineBuilder");

static_assert(FluxBackendComputePipelineBuilder<Zenith_Vulkan_ComputePipelineBuilder>,
	"Zenith_Vulkan_ComputePipelineBuilder does not satisfy FluxBackendComputePipelineBuilder");

static_assert(FluxBackendRootSigBuilder        <Zenith_Vulkan_RootSigBuilder>,
	"Zenith_Vulkan_RootSigBuilder does not satisfy FluxBackendRootSigBuilder");

// ---- Tools-only conformance ------------------------------------------------
// The ImGui-integration entry points are compiled out of shipping builds
// (#ifdef ZENITH_TOOLS gates their declarations). Assert conformance only
// when the methods actually exist — otherwise the concept substitution fails
// and the shipping build errors on a tools-only contract.

#ifdef ZENITH_TOOLS
static_assert(FluxBackendImGuiTools            <Zenith_Vulkan>,
	"Zenith_Vulkan does not satisfy FluxBackendImGuiTools");
#endif
