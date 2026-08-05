#pragma once

// ============================================================================
// Zenith_BackendAliases
//
// THE render-backend seam: the compile-time selection guard plus the `Flux_*`
// aliases that name the selected backend's classes. The backend is chosen by
// the Sharpmake RenderBackend fragment, which defines exactly one of
// ZENITH_VULKAN / ZENITH_D3D12 / ZENITH_NULL_RENDERER (never more, never none).
//
// WHY THIS LIVES IN Core AND NOT IN Flux: every name below is a forward
// declaration of a BACKEND class -- Zenith_Vulkan* / Zenith_D3D12* /
// Zenith_Null*, which live in the Vulkan / D3D12 / Null modules -- selected by
// a BUILD-SYSTEM define. Not one of them is a Flux type. It sat in
// Flux/Flux_Fwd.h for convenience, which made the composition root
// (Core/Zenith_Engine.h, whose FluxBackend()/FluxMemory()/FluxSwapchain()
// accessors are the only things in Core that need these three aliases) carry a
// Core -> Flux include edge purely to reach a backend-selection block. Owning
// the seam here removes that edge without duplicating the selection: this is
// the single definition, and Flux/Flux_Fwd.h includes it. Same precedent as
// Core/ZenithConfig.h owning FLUX_MAX_TARGETS / FLUX_MAX_BINDINGS_PER_GROUP.
//
// This header stays LIGHT -- forward declarations and `using` aliases only, no
// full backend headers. Call sites that invoke methods on these types include
// the HEAVY Flux/Flux_BackendTypes.h. See Flux/Flux_Backend.h for the backend
// contract documentation.
// ============================================================================

// The guard runs first, so a mis-configured build fails here with a clear
// message instead of via a downstream cascade of "undeclared identifier" /
// ambiguous-alias errors.
//
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
#error "Zenith: more than one render backend is defined - select exactly one of ZENITH_VULKAN / ZENITH_D3D12 / ZENITH_NULL_RENDERER."
#endif

#if (ZENITH_BACKEND_BIT_VULKAN + ZENITH_BACKEND_BIT_D3D12 + ZENITH_BACKEND_BIT_NULL) < 1
#error "Zenith: no render backend defined - define exactly one of ZENITH_VULKAN / ZENITH_D3D12 / ZENITH_NULL_RENDERER."
#endif

#if defined(ZENITH_VULKAN)
class Zenith_Vulkan;
class Zenith_Vulkan_MemoryManager;
class Zenith_Vulkan_CommandBuffer;
class Zenith_Vulkan_Swapchain;
class Zenith_Vulkan_Pipeline;
class Zenith_Vulkan_PipelineBuilder;
class Zenith_Vulkan_Shader;
class Zenith_Vulkan_Sampler;
class Zenith_Vulkan_RootSig;
class Zenith_Vulkan_ComputePipelineBuilder;
class Zenith_Vulkan_RootSigBuilder;
class Zenith_Vulkan_VRAM;

using Flux_PlatformAPI            = Zenith_Vulkan;
using Flux_MemoryManager          = Zenith_Vulkan_MemoryManager;
using Flux_CommandBuffer          = Zenith_Vulkan_CommandBuffer;
using Flux_Swapchain              = Zenith_Vulkan_Swapchain;
using Flux_Pipeline               = Zenith_Vulkan_Pipeline;
using Flux_PipelineBuilder        = Zenith_Vulkan_PipelineBuilder;
using Flux_Shader                 = Zenith_Vulkan_Shader;
using Flux_Sampler                = Zenith_Vulkan_Sampler;
using Flux_RootSig                = Zenith_Vulkan_RootSig;
using Flux_ComputePipelineBuilder = Zenith_Vulkan_ComputePipelineBuilder;
using Flux_RootSigBuilder         = Zenith_Vulkan_RootSigBuilder;
using Flux_VRAM                   = Zenith_Vulkan_VRAM;

#elif defined(ZENITH_D3D12)
class Zenith_D3D12;
class Zenith_D3D12_MemoryManager;
class Zenith_D3D12_CommandBuffer;
class Zenith_D3D12_Swapchain;
class Zenith_D3D12_Pipeline;
class Zenith_D3D12_PipelineBuilder;
class Zenith_D3D12_Shader;
class Zenith_D3D12_Sampler;
class Zenith_D3D12_RootSig;
class Zenith_D3D12_ComputePipelineBuilder;
class Zenith_D3D12_RootSigBuilder;
class Zenith_D3D12_VRAM;

using Flux_PlatformAPI            = Zenith_D3D12;
using Flux_MemoryManager          = Zenith_D3D12_MemoryManager;
using Flux_CommandBuffer          = Zenith_D3D12_CommandBuffer;
using Flux_Swapchain              = Zenith_D3D12_Swapchain;
using Flux_Pipeline               = Zenith_D3D12_Pipeline;
using Flux_PipelineBuilder        = Zenith_D3D12_PipelineBuilder;
using Flux_Shader                 = Zenith_D3D12_Shader;
using Flux_Sampler                = Zenith_D3D12_Sampler;
using Flux_RootSig                = Zenith_D3D12_RootSig;
using Flux_ComputePipelineBuilder = Zenith_D3D12_ComputePipelineBuilder;
using Flux_RootSigBuilder         = Zenith_D3D12_RootSigBuilder;
using Flux_VRAM                   = Zenith_D3D12_VRAM;

#elif defined(ZENITH_NULL_RENDERER)
class Zenith_Null;
class Zenith_Null_MemoryManager;
class Zenith_Null_CommandBuffer;
class Zenith_Null_Swapchain;
class Zenith_Null_Pipeline;
class Zenith_Null_PipelineBuilder;
class Zenith_Null_Shader;
class Zenith_Null_Sampler;
class Zenith_Null_RootSig;
class Zenith_Null_ComputePipelineBuilder;
class Zenith_Null_RootSigBuilder;
class Zenith_Null_VRAM;

using Flux_PlatformAPI            = Zenith_Null;
using Flux_MemoryManager          = Zenith_Null_MemoryManager;
using Flux_CommandBuffer          = Zenith_Null_CommandBuffer;
using Flux_Swapchain              = Zenith_Null_Swapchain;
using Flux_Pipeline               = Zenith_Null_Pipeline;
using Flux_PipelineBuilder        = Zenith_Null_PipelineBuilder;
using Flux_Shader                 = Zenith_Null_Shader;
using Flux_Sampler                = Zenith_Null_Sampler;
using Flux_RootSig                = Zenith_Null_RootSig;
using Flux_ComputePipelineBuilder = Zenith_Null_ComputePipelineBuilder;
using Flux_RootSigBuilder         = Zenith_Null_RootSigBuilder;
using Flux_VRAM                   = Zenith_Null_VRAM;
#endif
