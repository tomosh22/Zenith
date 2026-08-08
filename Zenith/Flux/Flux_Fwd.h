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
// The selection guard + the Flux_* backend aliases live in
// Core/Zenith_BackendAliases.h -- every name they declare is a BACKEND class
// (Zenith_Vulkan* / Zenith_D3D12* / Zenith_Null*) chosen by a build-system
// define, not a Flux type, and the composition root (Core/Zenith_Engine.h)
// needs them without taking a Core -> Flux edge. Included here so every
// existing Flux_Fwd.h consumer keeps resolving Flux_CommandBuffer /
// Flux_Pipeline / Flux_Shader / ... exactly as before. Method-call sites
// include the HEAVY Flux_BackendTypes.h. See Flux_Backend.h for the backend
// contract documentation.
// ============================================================================
#include "Core/Zenith_BackendAliases.h"

// Handle type (lightweight, doesn't need Vulkan)
class Flux_VRAMHandle;
