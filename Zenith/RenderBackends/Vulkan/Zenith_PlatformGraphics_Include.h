#pragma once

// Forward declare classes first - these are the Vulkan implementations
// Aliases need to be visible BEFORE includes that use them as types
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

// ============================================================================
// Platform abstraction aliases
// These map Flux_* names to the Vulkan backend types. Flux source code should
// use these aliases (not the concrete Zenith_Vulkan_* names) so that a second
// backend can be added by swapping this header. See Flux_Backend.h for the
// backend contract documentation.
//
// These are `using` declarations rather than `#define` so that IDE
// "go to definition" jumps to the alias declaration (and from there to the
// real class), and so the names show up in diagnostic messages as Flux_X
// rather than being silently rewritten by the preprocessor.
// ============================================================================
using Flux_PlatformAPI               = Zenith_Vulkan;
using Flux_MemoryManager             = Zenith_Vulkan_MemoryManager;
using Flux_CommandBuffer             = Zenith_Vulkan_CommandBuffer;
using Flux_Swapchain                 = Zenith_Vulkan_Swapchain;
using Flux_Pipeline                  = Zenith_Vulkan_Pipeline;
using Flux_PipelineBuilder           = Zenith_Vulkan_PipelineBuilder;
using Flux_Shader                    = Zenith_Vulkan_Shader;
using Flux_Sampler                   = Zenith_Vulkan_Sampler;
using Flux_RootSig                   = Zenith_Vulkan_RootSig;
using Flux_ComputePipelineBuilder    = Zenith_Vulkan_ComputePipelineBuilder;
using Flux_RootSigBuilder            = Zenith_Vulkan_RootSigBuilder;
using Flux_VRAM                      = Zenith_Vulkan_VRAM;

// Now include the actual headers (which may include Flux.h)
#include "Zenith_Vulkan.h"
#include "Zenith_Vulkan_MemoryManager.h"
#include "Zenith_Vulkan_Swapchain.h"
#include "Zenith_Vulkan_Pipeline.h"