#pragma once

#include "vulkan/vulkan.hpp"
#include "Flux/Flux_Types.h"

class Zenith_Vulkan_CommandBuffer;
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

	static vk::PipelineVertexInputStateCreateInfo VertexDescToVulkanDesc(const Flux_VertexInputDescription& xDesc, std::vector<vk::VertexInputBindingDescription>& axBindDescs, std::vector<vk::VertexInputAttributeDescription>& axAttrDescs)
	{
		uint32_t uBindPoint = 0;
		const Flux_BufferLayout& xVertexLayout = xDesc.m_xPerVertexLayout;
		for (const Flux_BufferElement& element : xVertexLayout.GetElements())
		{


			vk::VertexInputAttributeDescription xAttrDesc = vk::VertexInputAttributeDescription()
				.setBinding(0)
				.setLocation(uBindPoint)
				.setOffset(element._Offset)
				.setFormat(ShaderDataTypeToVulkanFormat(element._Type));
			axAttrDescs.push_back(xAttrDesc);
			uBindPoint++;
		}

		vk::VertexInputBindingDescription xBindDesc = vk::VertexInputBindingDescription()
			.setBinding(0)
			.setStride(xVertexLayout.m_uStride)
			.setInputRate(vk::VertexInputRate::eVertex);
		axBindDescs.push_back(xBindDesc);



		const Flux_BufferLayout& xInstanceLayout = xDesc.m_xPerInstanceLayout;
		if (xDesc.m_xPerInstanceLayout.GetElements().size())
		{
			for (const Flux_BufferElement& element : xInstanceLayout.GetElements())
			{


				vk::VertexInputAttributeDescription xInstanceAttrDesc = vk::VertexInputAttributeDescription()
					.setBinding(1)
					.setLocation(uBindPoint)
					.setOffset(element._Offset)
					.setFormat(ShaderDataTypeToVulkanFormat(element._Type));
				axAttrDescs.push_back(xInstanceAttrDesc);
				uBindPoint++;
			}

			vk::VertexInputBindingDescription xInstanceBindDesc = vk::VertexInputBindingDescription()
				.setBinding(1)
				.setStride(xInstanceLayout.m_uStride)
				.setInputRate(vk::VertexInputRate::eInstance);
			axBindDescs.push_back(xInstanceBindDesc);
		}

		return std::move(vk::PipelineVertexInputStateCreateInfo()
			.setVertexBindingDescriptionCount(axBindDescs.size())
			.setPVertexBindingDescriptions(axBindDescs.data())
			.setVertexAttributeDescriptionCount(axAttrDescs.size())
			.setPVertexAttributeDescriptions(axAttrDescs.data()));
	}

	static void Initialise();
	static void CreateInstance();
	static void CreateDebugMessenger();
	static void CreateSurface();
	static void CreatePhysicalDevice();
	static void CreateQueueFamilies();
	static void CreateDevice();
	static void CreateCommandPools();
	static void CreateDefaultDescriptorPool();

	static void BeginFrame();
	static void EndFrame();

	static void SubmitCommandBuffer(const Zenith_Vulkan_CommandBuffer* pxCmd, RenderOrder eOrder)
	{
		for (const Zenith_Vulkan_CommandBuffer* pxExistingCmd : s_xPendingCommandBuffers[eOrder])
		{
			Zenith_Assert(pxExistingCmd != pxCmd, "Command buffer has already been submitted");
		}
		s_xPendingCommandBuffers[eOrder].push_back(pxCmd);
	}

	static void RecreatePerFrameDescriptorPool();

	static const vk::PhysicalDevice& GetPhysicalDevice() { return s_xPhysicalDevice; }
	static const vk::Device& GetDevice() { return s_xDevice; }
	static const vk::CommandPool& GetCommandPool(CommandType eType) { return s_axCommandPools[eType]; }
	static const vk::Queue& GetQueue(CommandType eType) { return s_axQueues[eType]; }
	static const vk::DescriptorPool& GetCurrentPerFrameDescriptorPool();
	static const vk::SurfaceKHR& GetSurface() { return s_xSurface; }
	static const uint32_t GetQueueIndex(CommandType eType) { return s_auQueueIndices[eType]; }
	static const vk::DescriptorPool& GetDefaultDescriptorPool() { return s_xDefaultDescriptorPool; }
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
	static vk::DescriptorPool s_axPerFrameDescriptorPools[MAX_FRAMES_IN_FLIGHT];

	static std::vector<const Zenith_Vulkan_CommandBuffer*> s_xPendingCommandBuffers[RENDER_ORDER_MAX];
};