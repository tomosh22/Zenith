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
struct Flux_BufferLayout;
struct Flux_BufferElement;

// Command types
class Flux_CommandList;

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

// Platform abstraction types
// These are concrete Vulkan types, aliased via macros in Zenith_PlatformGraphics_Include.h.
// Flux source code uses the Flux_* aliases; a second backend would swap these declarations.
// See Flux_Backend.h for the backend contract documentation.
class Zenith_Vulkan_Pipeline;
class Zenith_Vulkan_PipelineBuilder;
class Zenith_Vulkan_ComputePipelineBuilder;
class Zenith_Vulkan_Shader;
class Zenith_Vulkan_Sampler;
class Zenith_Vulkan_RootSig;
class Zenith_Vulkan_RootSigBuilder;
class Zenith_Vulkan_CommandBuffer;
class Zenith_Vulkan_Swapchain;
class Zenith_Vulkan_MemoryManager;
class Zenith_Vulkan;
struct Zenith_Vulkan_VRAM;

// Handle type (lightweight, doesn't need Vulkan)
class Flux_VRAMHandle;
