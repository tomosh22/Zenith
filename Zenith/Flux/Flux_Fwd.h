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

// View types (contain Vulkan handles - full definition requires vulkan.hpp)
struct Flux_ShaderResourceView;
struct Flux_UnorderedAccessView_Texture;
struct Flux_UnorderedAccessView_Buffer;
struct Flux_RenderTargetView;
struct Flux_DepthStencilView;
struct Flux_ConstantBufferView;

// Pipeline and rendering types
struct Flux_TargetSetup;
struct Flux_PipelineSpecification;
struct Flux_BlendState;
struct Flux_PipelineLayout;
struct Flux_DescriptorSetLayout;
struct Flux_DescriptorBinding;
struct Flux_VertexInputDescription;
struct Flux_BufferLayout;
struct Flux_BufferElement;

// Command types
class Flux_CommandList;

// Mesh and material types
class Flux_MeshGeometry;
class Flux_MaterialAsset;

// Model/animation types
class Flux_ModelInstance;
class Flux_MeshInstance;
class Flux_SkeletonInstance;
class Flux_AnimationController;
class Flux_AnimationStateMachine;
class Flux_IKSolver;

// Platform abstraction types (defined via macros, forward declare the Vulkan versions)
class Zenith_Vulkan_Pipeline;
class Zenith_Vulkan_PipelineBuilder;
class Zenith_Vulkan_Shader;
class Zenith_Vulkan_Sampler;
class Zenith_Vulkan_RootSig;
class Zenith_Vulkan_CommandBuffer;
class Zenith_Vulkan_Swapchain;
class Zenith_Vulkan_MemoryManager;
class Zenith_Vulkan;

// Handle type (lightweight, doesn't need Vulkan)
class Flux_VRAMHandle;
