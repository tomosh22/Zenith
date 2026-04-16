#pragma once

// Forward declare classes first - these are the Vulkan implementations
// Macros need to be defined BEFORE includes that use them
class Zenith_Vulkan;
class Zenith_Vulkan_MemoryManager;
class Zenith_Vulkan_CommandBuffer;
class Zenith_Vulkan_Swapchain;
class Zenith_Vulkan_Pipeline;
class Zenith_Vulkan_PipelineBuilder;
class Zenith_Vulkan_Shader;
class Zenith_Vulkan_Sampler;
class Zenith_Vulkan_RootSig;

// ============================================================================
// Platform abstraction macros
// These map Flux_* names to the Vulkan backend types. Flux source code should
// use these aliases (not the concrete Zenith_Vulkan_* names) so that a second
// backend can be added by swapping this header. See Flux_Backend.h for the
// backend contract documentation.
// ============================================================================
#define Flux_PlatformAPI Zenith_Vulkan
#define Flux_MemoryManager Zenith_Vulkan_MemoryManager
#define Flux_CommandBuffer Zenith_Vulkan_CommandBuffer
#define Flux_Swapchain Zenith_Vulkan_Swapchain
#define Flux_Pipeline Zenith_Vulkan_Pipeline
#define Flux_PipelineBuilder Zenith_Vulkan_PipelineBuilder
#define Flux_Shader Zenith_Vulkan_Shader
#define Flux_Sampler Zenith_Vulkan_Sampler
#define Flux_RootSig Zenith_Vulkan_RootSig
#define Flux_ComputePipelineBuilder Zenith_Vulkan_ComputePipelineBuilder
#define Flux_RootSigBuilder Zenith_Vulkan_RootSigBuilder
#define Flux_VRAM Zenith_Vulkan_VRAM

// Now include the actual headers (which may include Flux.h)
#include "Zenith_Vulkan.h"
#include "Zenith_Vulkan_MemoryManager.h"
#include "Zenith_Vulkan_CommandBuffer.h"
#include "Zenith_Vulkan_Swapchain.h"
#include "Zenith_Vulkan_Pipeline.h"