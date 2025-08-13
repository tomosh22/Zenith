#pragma once
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "vulkan/vulkan.hpp"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#include "Flux/Flux_Types.h"

#define ZENITH_VULKAN_PER_FRAME_DESC_SET 0
#define ZENITH_VULKAN_PER_DRAW_DESC_SET 1

class Zenith_Vulkan_CommandBuffer;

class Zenith_Vulkan_PerFrame
{
public:
	Zenith_Vulkan_PerFrame() = default;

	void Initialise();
	void BeginFrame();
	vk::Fence m_xFence;
	
	//#TO_TODO: one per thread
	vk::DescriptorPool m_axDescriptorPools[1];
};

class Zenith_Vulkan
{
public:

	static vk::Format ShaderDataTypeToVulkanFormat(ShaderDataType t)
	{
		switch (t)
		{
		case SHADER_DATA_TYPE_FLOAT:	return vk::Format::eR32Sfloat;
		case SHADER_DATA_TYPE_FLOAT2:	return vk::Format::eR32G32Sfloat;
		case SHADER_DATA_TYPE_FLOAT3:	return vk::Format::eR32G32B32Sfloat;
		case SHADER_DATA_TYPE_FLOAT4:	return vk::Format::eR32G32B32A32Sfloat;
		case SHADER_DATA_TYPE_INT:		return vk::Format::eR32Sint;
		case SHADER_DATA_TYPE_INT2:		return vk::Format::eR32G32Sint;
		case SHADER_DATA_TYPE_INT3:		return vk::Format::eR32G32B32Sint;
		case SHADER_DATA_TYPE_INT4:		return vk::Format::eR32G32B32A32Sint;
		case SHADER_DATA_TYPE_UINT:		return vk::Format::eR32Uint;
		case SHADER_DATA_TYPE_UINT2:	return vk::Format::eR32G32Uint;
		case SHADER_DATA_TYPE_UINT3:	return vk::Format::eR32G32B32Uint;
		case SHADER_DATA_TYPE_UINT4:	return vk::Format::eR32G32B32A32Uint;
		}
		Zenith_Assert(false, "Unknown shader data type");
	}

	static vk::DescriptorSet CreateDescriptorSet(const vk::DescriptorSetLayout& xLayout, const vk::DescriptorPool& xPool)
	{
		vk::DescriptorSetAllocateInfo xInfo = vk::DescriptorSetAllocateInfo()
			.setDescriptorPool(xPool)
			.setDescriptorSetCount(1)
			.setPSetLayouts(&xLayout);

		return (s_xDevice.allocateDescriptorSets(xInfo)[0]);
	}

	static void Initialise();
	static void CreateInstance();
#ifdef ZENITH_DEBUG
	static void CreateDebugMessenger();
#endif
	static void CreateSurface();
	static void CreatePhysicalDevice();
	static void CreateQueueFamilies();
	static void CreateDevice();
	static void CreateCommandPools();
	static void CreateDefaultDescriptorPool();

#ifdef ZENITH_TOOLS
	static vk::RenderPass s_xImGuiRenderPass;
	static void InitialiseImGui();
	static void InitialiseImGuiRenderPass();
	static void ImGuiBeginFrame();
#endif

	static void BeginFrame();
	static void EndFrame();

	static void SubmitCommandBuffer(const Zenith_Vulkan_CommandBuffer* pxCmd, RenderOrder eOrder);

	static const vk::Instance& GetInstance() { return s_xInstance; }
	static const vk::PhysicalDevice& GetPhysicalDevice() { return s_xPhysicalDevice; }
	static const vk::Device& GetDevice() { return s_xDevice; }
	static const vk::CommandPool& GetCommandPool(CommandType eType) { return s_axCommandPools[eType]; }
	static const vk::Queue& GetQueue(CommandType eType) { return s_axQueues[eType]; }
	static const vk::DescriptorPool& GetCurrentPerFrameDescriptorPool();
	static const vk::SurfaceKHR& GetSurface() { return s_xSurface; }
	static const uint32_t GetQueueIndex(CommandType eType) { return s_auQueueIndices[eType]; }
	static const vk::DescriptorPool& GetDefaultDescriptorPool() { return s_xDefaultDescriptorPool; }
	static vk::Fence& GetCurrentInFlightFence();
	static vk::Fence& GetPreviousInFlightFence();
	static vk::Fence& GetNextInFlightFence();

	static const bool ShouldSubmitDrawCalls();
private:
	static vk::Instance s_xInstance;
	static VKAPI_ATTR vk::Bool32 VKAPI_CALL DebugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT eMessageSeverity,
		vk::DebugUtilsMessageTypeFlagsEXT eMessageType,
		const vk::DebugUtilsMessengerCallbackDataEXT* pxCallbackData,
		void* pUserData);
	static vk::DebugUtilsMessengerEXT s_xDebugMessenger;
	static vk::SurfaceKHR s_xSurface;
	static vk::PhysicalDevice s_xPhysicalDevice;
	static struct GPUCapabilities {
		uint32_t m_uMaxTextureWidth;
		uint32_t m_uMaxTextureHeight;
		uint32_t m_uMaxFramebufferWidth;
		uint32_t m_uMaxFramebufferHeight;
	} s_xGPUCapabilties;
	static uint32_t s_auQueueIndices[COMMANDTYPE_MAX];
	static vk::Device s_xDevice;
	static vk::Queue s_axQueues[COMMANDTYPE_MAX];
	static vk::CommandPool s_axCommandPools[COMMANDTYPE_MAX];
	static vk::DescriptorPool s_xDefaultDescriptorPool;

	static Zenith_Vulkan_PerFrame s_axPerFrame[MAX_FRAMES_IN_FLIGHT];
	static Zenith_Vulkan_PerFrame* s_pxCurrentFrame;
};