#pragma once

#include "Zenith_Vulkan.h"
#include "Zenith_Vulkan_MemoryManager.h"
#include "Zenith_Vulkan_CommandBuffer.h"
#include "Zenith_Vulkan_Swapchain.h"
#include "Zenith_Vulkan_Pipeline.h"
#include "Zenith_Vulkan_Texture.h"
#include "Zenith_Vulkan_Buffer.h"

using Flux_PlatformAPI = Zenith_Vulkan;
using Flux_MemoryManager = Zenith_Vulkan_MemoryManager;
using Flux_CommandBuffer = Zenith_Vulkan_CommandBuffer;
using Flux_Swapchain = Zenith_Vulkan_Swapchain;
using Flux_Pipeline = Zenith_Vulkan_Pipeline;
using Flux_PipelineBuilder = Zenith_Vulkan_PipelineBuilder;
using Flux_Shader = Zenith_Vulkan_Shader;
using Flux_Texture = Zenith_Vulkan_Texture;
using Flux_Sampler = Zenith_Vulkan_Sampler;
using Flux_Buffer = Zenith_Vulkan_Buffer;
using Flux_PipelineSpecification = Zenith_Vulkan_PipelineSpecification;
