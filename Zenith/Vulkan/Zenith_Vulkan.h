#pragma once

#include "vulkan/vulkan.hpp"
#include "Flux/Zenith_Flux_Enums.h"


class Zenith_Vulkan
{
public:
	static void Initialise();
	static void CreateInstance();
	static void CreateDebugMessenger();
	static void CreateSurface();
	static void CreatePhysicalDevice();
	static void CreateQueueFamilies();
	static void CreateDevice();
	static void CreateCommandPools();

	static void BeginFrame();
	static void RecreatePerFrameDescriptorPool();

	static const vk::PhysicalDevice& GetPhysicalDevice() { return s_xPhysicalDevice; }
	static const vk::Device& GetDevice() { return s_xDevice; }
	static const vk::CommandPool& GetCommandPool(CommandType eType) { return s_axCommandPools[eType]; }
	static const vk::Queue& GetQueue(CommandType eType) { return s_axQueues[eType]; }
	static const vk::DescriptorPool& GetCurrentPerFrameDescriptorPool();
	static const vk::SurfaceKHR& GetSurface() { return s_xSurface; }
	static const uint32_t GetQueueIndex(CommandType eType) { return s_auQueueIndices[eType]; }
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
	static vk::DescriptorPool s_axPerFrameDescriptorPools[MAX_FRAMES_IN_FLIGHT];
};