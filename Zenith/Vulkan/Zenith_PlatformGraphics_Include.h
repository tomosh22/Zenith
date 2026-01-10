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

// Define platform abstraction macros FIRST
// These must be defined before any headers that use them (like Flux.h)
#define Flux_PlatformAPI Zenith_Vulkan
#define Flux_MemoryManager Zenith_Vulkan_MemoryManager
#define Flux_CommandBuffer Zenith_Vulkan_CommandBuffer
#define Flux_Swapchain Zenith_Vulkan_Swapchain
#define Flux_Pipeline Zenith_Vulkan_Pipeline
#define Flux_PipelineBuilder Zenith_Vulkan_PipelineBuilder
#define Flux_Shader Zenith_Vulkan_Shader
#define Flux_Sampler Zenith_Vulkan_Sampler
#define Flux_RootSig Zenith_Vulkan_RootSig

// Now include the actual headers (which may include Flux.h)
#include "Zenith_Vulkan.h"
#include "Zenith_Vulkan_MemoryManager.h"
#include "Zenith_Vulkan_CommandBuffer.h"
#include "Zenith_Vulkan_Swapchain.h"
#include "Zenith_Vulkan_Pipeline.h"