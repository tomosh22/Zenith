#pragma once

// Forward declarations for Flux types
// Include this header when you only need pointers/references to Flux types
// without pulling in the full Vulkan headers

// Core buffer types
struct Flux_Buffer;
struct Flux_Texture;
struct Flux_SurfaceInfo;
struct Flux_RenderAttachment;

// Buffer wrapper classes
class Flux_VertexBuffer;
class Flux_DynamicVertexBuffer;
class Flux_IndexBuffer;
class Flux_ConstantBuffer;
class Flux_DynamicConstantBuffer;
class Flux_IndirectBuffer;
class Flux_ReadWriteBuffer;
class Flux_DynamicReadWriteBuffer;

// View types (contain Vulkan handles - full definition requires vulkan.hpp)
struct Flux_ShaderResourceView;
struct Flux_UnorderedAccessView_Texture;
struct Flux_UnorderedAccessView_Buffer;
struct Flux_RenderTargetView;
struct Flux_DepthStencilView;
struct Flux_ConstantBufferView;

// Pipeline and rendering types
struct Flux_PipelineSpecification;
struct Flux_BlendState;
struct Flux_PipelineLayout;
struct Flux_BindingGroupLayout;
struct Flux_BindingGroupEntry;
struct Flux_VertexInputDescription;
class Flux_BufferLayout;
struct Flux_BufferElement;

// Mesh and material types
class Flux_MeshGeometry;
class Zenith_MaterialAsset;

// Model/animation types
class Flux_ModelInstance;
class Flux_MeshInstance;
class Flux_SkeletonInstance;
class Flux_AnimationController;
class Flux_AnimationStateMachine;
class Flux_IKSolver;

// ============================================================================
// Platform abstraction aliases (the LIGHT backend seam)
//
// The backend macro (ZENITH_VULKAN / ZENITH_D3D12, selected by the Sharpmake
// RenderBackend fragment) chooses which concrete backend classes the Flux_*
// aliases name. Only forward-declarations + the `using` aliases live here, so
// declaration-only consumers (the *Impl.h headers, Zenith_Engine.h) stay
// light. Method-call sites that invoke methods on these types include the
// HEAVY Flux_BackendTypes.h, which pulls the full backend headers. See
// Flux_Backend.h for the backend contract documentation.
// ============================================================================
#include "Flux/Flux_BackendGuard.h"

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
#endif

// Handle type (lightweight, doesn't need Vulkan)
class Flux_VRAMHandle;
