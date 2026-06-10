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
//   1. g_xEngine.FluxRenderer().PerFrameInitialise()              (no-op; the frame index
//                                                lives on FrameContext)
//   2. Backend Initialise()                     (sets up per-frame GPU state)
//   3. MemoryManager Initialise()               (sets up the deferred-deletion queue)
// And at shutdown, the reverse order:
//   1. MemoryManager Shutdown()                 (drains deferred deletions)
//   2. Backend Shutdown()
//   3. g_xEngine.FluxRenderer().PerFrameShutdown()
//
// Per-frame dispatch is direct, not callback-based: Flux_RendererImpl::BeginFrame
// calls the backend's PerFrameBegin() (fence wait, descriptor pool reset, typed
// deletion drain, scratch reset) via the neutral Flux_PlatformAPI alias, and
// Flux_RendererImpl::ProcessFrameEnd calls FluxMemory().ProcessDeferredDeletions()
// via the neutral Flux_MemoryManager alias. The null backend's forms are no-ops.

#include "Flux/Flux_Backend.h"

// Pull in the concrete backend classes so the concepts can substitute.

// ---- Vulkan backend conformance --------------------------------------------
#ifdef ZENITH_VULKAN

static_assert(FluxBackendDevice                <Zenith_Vulkan>,
	"Zenith_Vulkan does not satisfy FluxBackendDevice");

static_assert(FluxBackendMemoryAlloc           <Zenith_Vulkan_MemoryManager>,
	"Zenith_Vulkan_MemoryManager does not satisfy FluxBackendMemoryAlloc");

static_assert(FluxBackendMemoryDelete          <Zenith_Vulkan_MemoryManager>,
	"Zenith_Vulkan_MemoryManager does not satisfy FluxBackendMemoryDelete");

// Optional transient-aliasing slice (split out of FluxBackendMemoryAlloc in P10).
// Asserted as a dual positive: every shipping backend provides the methods (the
// null backend as false-returning / invalid-handle stubs). A future backend that
// omits aliasing would drop this assert and have render-graph call sites gate on
// SupportsTransientAliasing() (already the runtime contract).
static_assert(FluxBackendTransientAliasing     <Zenith_Vulkan_MemoryManager>,
	"Zenith_Vulkan_MemoryManager does not satisfy FluxBackendTransientAliasing");

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

#endif // ZENITH_VULKAN

// ---- D3D12 null-backend conformance ----------------------------------------
// The no-op null backend must satisfy EVERY concept (and, via the link step,
// every non-concept call the engine makes) -- this is the whole point of the
// backend: a compile/link proof that the Flux surface is renderer-neutral.
#ifdef ZENITH_D3D12

static_assert(FluxBackendDevice                <Zenith_D3D12>,
	"Zenith_D3D12 does not satisfy FluxBackendDevice");

static_assert(FluxBackendMemoryAlloc           <Zenith_D3D12_MemoryManager>,
	"Zenith_D3D12_MemoryManager does not satisfy FluxBackendMemoryAlloc");

static_assert(FluxBackendMemoryDelete          <Zenith_D3D12_MemoryManager>,
	"Zenith_D3D12_MemoryManager does not satisfy FluxBackendMemoryDelete");

static_assert(FluxBackendTransientAliasing     <Zenith_D3D12_MemoryManager>,
	"Zenith_D3D12_MemoryManager does not satisfy FluxBackendTransientAliasing");

static_assert(FluxBackendCommandRecorder       <Zenith_D3D12_CommandBuffer>,
	"Zenith_D3D12_CommandBuffer does not satisfy FluxBackendCommandRecorder (umbrella)");
static_assert(FluxBackendRecordingLifecycle    <Zenith_D3D12_CommandBuffer>,
	"Zenith_D3D12_CommandBuffer does not satisfy FluxBackendRecordingLifecycle");
static_assert(FluxBackendPipelineBinding       <Zenith_D3D12_CommandBuffer>,
	"Zenith_D3D12_CommandBuffer does not satisfy FluxBackendPipelineBinding");
static_assert(FluxBackendVertexIndexStreams    <Zenith_D3D12_CommandBuffer>,
	"Zenith_D3D12_CommandBuffer does not satisfy FluxBackendVertexIndexStreams");
static_assert(FluxBackendBasicDraws            <Zenith_D3D12_CommandBuffer>,
	"Zenith_D3D12_CommandBuffer does not satisfy FluxBackendBasicDraws");
static_assert(FluxBackendIndirectDraws         <Zenith_D3D12_CommandBuffer>,
	"Zenith_D3D12_CommandBuffer does not satisfy FluxBackendIndirectDraws");
static_assert(FluxBackendCompute               <Zenith_D3D12_CommandBuffer>,
	"Zenith_D3D12_CommandBuffer does not satisfy FluxBackendCompute");
static_assert(FluxBackendResourceBinding       <Zenith_D3D12_CommandBuffer>,
	"Zenith_D3D12_CommandBuffer does not satisfy FluxBackendResourceBinding");
static_assert(FluxBackendDynamicState          <Zenith_D3D12_CommandBuffer>,
	"Zenith_D3D12_CommandBuffer does not satisfy FluxBackendDynamicState");
#ifdef ZENITH_FLUX_PROFILING
static_assert(FluxBackendDebugMarkers          <Zenith_D3D12_CommandBuffer>,
	"Zenith_D3D12_CommandBuffer does not satisfy FluxBackendDebugMarkers");
#endif

static_assert(FluxBackendSync                  <Zenith_D3D12_CommandBuffer>,
	"Zenith_D3D12_CommandBuffer does not satisfy FluxBackendSync");

static_assert(FluxBackendPresentation          <Zenith_D3D12_Swapchain>,
	"Zenith_D3D12_Swapchain does not satisfy FluxBackendPresentation");

static_assert(FluxBackendShader                <Zenith_D3D12_Shader>,
	"Zenith_D3D12_Shader does not satisfy FluxBackendShader");

static_assert(FluxBackendPipelineBuilder       <Zenith_D3D12_PipelineBuilder>,
	"Zenith_D3D12_PipelineBuilder does not satisfy FluxBackendPipelineBuilder");

static_assert(FluxBackendComputePipelineBuilder<Zenith_D3D12_ComputePipelineBuilder>,
	"Zenith_D3D12_ComputePipelineBuilder does not satisfy FluxBackendComputePipelineBuilder");

static_assert(FluxBackendRootSigBuilder        <Zenith_D3D12_RootSigBuilder>,
	"Zenith_D3D12_RootSigBuilder does not satisfy FluxBackendRootSigBuilder");

#ifdef ZENITH_TOOLS
static_assert(FluxBackendImGuiTools            <Zenith_D3D12>,
	"Zenith_D3D12 does not satisfy FluxBackendImGuiTools");
#endif

#endif // ZENITH_D3D12
