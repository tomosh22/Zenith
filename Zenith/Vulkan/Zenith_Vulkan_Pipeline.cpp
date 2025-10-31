/******************************************************************************
This file is part of the Newcastle Vulkan Tutorial Series

Author:Rich Davison
Contact:richgdavison@gmail.com
License: MIT (see LICENSE file at the top of the source tree)
*//////////////////////////////////////////////////////////////////////////////
#include "Zenith.h"

#include "Zenith_Vulkan_Pipeline.h"

#include "Zenith_Vulkan.h"
#include "Flux/Flux.h"
#include "Flux/Flux_RenderTargets.h"
#include "FileAccess/Zenith_FileAccess.h"

void Zenith_Vulkan_Shader::Initialise(const std::string& strVertex, const std::string& strFragment, const std::string& strGeometry, const std::string& strDomain, const std::string& strHull)
{
	const std::string strExtension = ".spv";
	std::string strShaderRoot(SHADER_SOURCE_ROOT);
	m_pcVertShaderCode = Zenith_FileAccess::ReadFile((strShaderRoot + strVertex + strExtension).c_str(), m_pcVertShaderCodeSize);
	m_pcFragShaderCode = Zenith_FileAccess::ReadFile((strShaderRoot + strFragment + strExtension).c_str(), m_pcFragShaderCodeSize);

	m_uStageCount = 2;

	if (strDomain.length())
	{
		Zenith_Assert(strHull.length(), "Found tesc but not tese");
		m_pcTescShaderCode = Zenith_FileAccess::ReadFile((strShaderRoot + strDomain + strExtension).c_str(), m_pcTescShaderCodeSize);
		m_pcTeseShaderCode = Zenith_FileAccess::ReadFile((strShaderRoot + strHull + strExtension).c_str(), m_pcTeseShaderCodeSize);
		m_uStageCount = 4;
		m_bTesselation = true;
	}

	m_xVertShaderModule = CreateShaderModule(m_pcVertShaderCode, m_pcVertShaderCodeSize);
	m_xFragShaderModule = CreateShaderModule(m_pcFragShaderCode, m_pcFragShaderCodeSize);

	m_xInfos = new vk::PipelineShaderStageCreateInfo[m_uStageCount];

	//vert
	m_xInfos[0].stage = vk::ShaderStageFlagBits::eVertex;
	m_xInfos[0].module = m_xVertShaderModule;
	m_xInfos[0].pName = "main";

	//frag
	m_xInfos[1].stage = vk::ShaderStageFlagBits::eFragment;
	m_xInfos[1].module = m_xFragShaderModule;
	m_xInfos[1].pName = "main";

	if (m_bTesselation) {
		m_xTescShaderModule = CreateShaderModule(m_pcTescShaderCode, m_pcTescShaderCodeSize);
		m_xTeseShaderModule = CreateShaderModule(m_pcTeseShaderCode, m_pcTeseShaderCodeSize);

		//vert
		m_xInfos[2].stage = vk::ShaderStageFlagBits::eTessellationControl;
		m_xInfos[2].module = m_xTescShaderModule;
		m_xInfos[2].pName = "main";

		//frag
		m_xInfos[3].stage = vk::ShaderStageFlagBits::eTessellationEvaluation;
		m_xInfos[3].module = m_xTeseShaderModule;
		m_xInfos[3].pName = "main";
	}
}

void Zenith_Vulkan_Shader::InitialiseCompute(const std::string& strCompute)
{
	const std::string strExtension = ".spv";
	std::string strShaderRoot(SHADER_SOURCE_ROOT);
	m_pcCompShaderCode = Zenith_FileAccess::ReadFile((strShaderRoot + strCompute + strExtension).c_str(), m_pcCompShaderCodeSize);
	
	m_xCompShaderModule = CreateShaderModule(m_pcCompShaderCode, m_pcCompShaderCodeSize);
	m_uStageCount = 1;
}

Zenith_Vulkan_Shader::~Zenith_Vulkan_Shader()
{
	const vk::Device xDevice = Zenith_Vulkan::GetDevice();
	xDevice.destroyShaderModule(m_xVertShaderModule);
	xDevice.destroyShaderModule(m_xFragShaderModule);
	xDevice.destroyShaderModule(m_xTescShaderModule);
	xDevice.destroyShaderModule(m_xTeseShaderModule);
	xDevice.destroyShaderModule(m_xCompShaderModule);
	delete[] m_pcVertShaderCode;
	delete[] m_pcFragShaderCode;
	delete[] m_pcTescShaderCode;
	delete[] m_pcTeseShaderCode;
	delete[] m_pcCompShaderCode;
	delete[] m_xInfos;
}

void Zenith_Vulkan_Shader::FillShaderStageCreateInfo(vk::GraphicsPipelineCreateInfo& xPipelineCreateInfo) const
{
	xPipelineCreateInfo.setStageCount(m_uStageCount);
	xPipelineCreateInfo.setPStages(m_xInfos);
}

vk::ShaderModule Zenith_Vulkan_Shader::CreateShaderModule(const char* szCode, uint64_t ulCodeLength)
{
	Zenith_Assert(strlen(szCode), "Shader code is empty");
	vk::ShaderModuleCreateInfo xCreateInfo = vk::ShaderModuleCreateInfo()
		.setCodeSize(ulCodeLength)
		.setPCode(reinterpret_cast<const uint32_t*>(szCode));
	return Zenith_Vulkan::GetDevice().createShaderModule(xCreateInfo);
}

class Zenith_Vulkan_DescriptorSetLayoutBuilder
{
public:
	Zenith_Vulkan_DescriptorSetLayoutBuilder(const std::string& name = "")
		: m_bUsingBindless(false)
		, m_bUsingDescriptorBuffer(false)
		, m_strDebugName(name)
	{
	};
	~Zenith_Vulkan_DescriptorSetLayoutBuilder() {};

	Zenith_Vulkan_DescriptorSetLayoutBuilder& WithSamplers(unsigned int uCount, vk::ShaderStageFlags eInShaders = vk::ShaderStageFlagBits::eAll)
	{
		vk::DescriptorSetLayoutBinding xBinding = vk::DescriptorSetLayoutBinding()
			.setBinding((uint32_t)m_xAddedBindings.size())
			.setDescriptorCount(uCount)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setStageFlags(eInShaders);

		m_xAddedBindings.emplace_back(xBinding);

		return *this;
	}
	Zenith_Vulkan_DescriptorSetLayoutBuilder& WithUniformBuffers(unsigned int uCount, vk::ShaderStageFlags eInShaders = vk::ShaderStageFlagBits::eAll)
	{
		vk::DescriptorSetLayoutBinding xBinding = vk::DescriptorSetLayoutBinding()
			.setBinding((uint32_t)m_xAddedBindings.size())
			.setDescriptorCount(uCount)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setStageFlags(eInShaders);

		m_xAddedBindings.emplace_back(xBinding);
		return *this;
	}
	Zenith_Vulkan_DescriptorSetLayoutBuilder& WithStorageBuffers(unsigned int uCount, vk::ShaderStageFlags eInShaders = vk::ShaderStageFlagBits::eAll)
	{
		vk::DescriptorSetLayoutBinding xBinding = vk::DescriptorSetLayoutBinding()
			.setBinding((uint32_t)m_xAddedBindings.size())
			.setDescriptorCount(uCount)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setStageFlags(eInShaders);

		m_xAddedBindings.emplace_back(xBinding);
		return *this;
	}

	Zenith_Vulkan_DescriptorSetLayoutBuilder& WithBindlessAccess()
	{
		m_bUsingBindless = true;
		return *this;
	}

	Zenith_Vulkan_DescriptorSetLayoutBuilder& WithDescriptorBufferAccess()
	{
		m_bUsingDescriptorBuffer = true;
		return *this;
	}

	Zenith_Vulkan_DescriptorSetLayoutBuilder& WithAccelStructures(unsigned int uCount, vk::ShaderStageFlags eInShaders = vk::ShaderStageFlagBits::eAll, vk::DescriptorBindingFlags eBindingFlags = (vk::DescriptorBindingFlags)0)
	{
		AddDescriptors(uCount, eInShaders, eBindingFlags).setDescriptorType(vk::DescriptorType::eAccelerationStructureKHR);
		return *this;
	}

	vk::DescriptorSetLayout Build(vk::Device device)
	{
		m_xCreateInfo.setPBindings(m_xAddedBindings.data());
		m_xCreateInfo.setBindingCount(m_xAddedBindings.size());

		vk::DescriptorSetLayoutBindingFlagsCreateInfoEXT xBindingFlagsInfo;
		std::vector< vk::DescriptorBindingFlags> xBindingFlags;

		//if (m_bUsingBindless)
		{
			for (int i = 0; i < m_xAddedBindings.size(); ++i)
			{
				if (m_xAddedBindings[i].descriptorType == vk::DescriptorType::eCombinedImageSampler)
				{
					//#TO_TODO: why do I get geometry and lighting flickering without this???
					xBindingFlags.push_back(vk::DescriptorBindingFlagBits::eUpdateAfterBind);
				}
				else
				{
					xBindingFlags.push_back((vk::DescriptorBindingFlagBits)0);
				}
			}
			xBindingFlagsInfo.setBindingFlags(xBindingFlags);
			m_xCreateInfo.pNext = &xBindingFlagsInfo;
			m_xCreateInfo.flags |= vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool;
		}

		vk::DescriptorSetLayout layout = device.createDescriptorSetLayout(m_xCreateInfo);
		return layout;
	}

	//static vk::DescriptorSetLayout FromSpecification(const TextureDescriptorSpecification spec);

protected:
	vk::DescriptorSetLayoutBinding& AddDescriptors(unsigned int uCount, vk::ShaderStageFlags eInShaders = vk::ShaderStageFlagBits::eAll, vk::DescriptorBindingFlags eBindingFlags = (vk::DescriptorBindingFlags)0)
	{
		vk::DescriptorSetLayoutBinding xBinding = vk::DescriptorSetLayoutBinding()
			.setBinding((uint32_t)m_xAddedBindings.size())
			.setDescriptorCount(uCount)
			.setStageFlags(eInShaders);

		m_xAddedBindings.emplace_back(xBinding);
		m_xAddedFlags.emplace_back(eBindingFlags);
		return m_xAddedBindings[m_xAddedBindings.size() - 1];
	}

	std::string	m_strDebugName;
	bool m_bUsingBindless;
	bool m_bUsingDescriptorBuffer;
	std::vector< vk::DescriptorSetLayoutBinding> m_xAddedBindings;
	std::vector< vk::DescriptorBindingFlags> m_xAddedFlags;

	vk::DescriptorSetLayoutCreateInfo m_xCreateInfo;
};

static vk::PipelineVertexInputStateCreateInfo VertexDescToVulkanDesc(const Flux_VertexInputDescription& xDesc, std::vector<vk::VertexInputBindingDescription>& xBindDescs, std::vector<vk::VertexInputAttributeDescription>& xAttrDescs)
{
	uint32_t uBindPoint = 0;
	const Flux_BufferLayout& xVertexLayout = xDesc.m_xPerVertexLayout;
	for (Zenith_Vector<Flux_BufferElement>::Iterator xIt(xVertexLayout.GetElements()); !xIt.Done(); xIt.Next())
	{
		const Flux_BufferElement& xElement = xIt.GetData();

		vk::VertexInputAttributeDescription xAttrDesc = vk::VertexInputAttributeDescription()
			.setBinding(0)
			.setLocation(uBindPoint)
			.setOffset(xElement.m_uOffset)
			.setFormat(Zenith_Vulkan::ShaderDataTypeToVulkanFormat(xElement._Type));
		xAttrDescs.push_back(xAttrDesc);
		uBindPoint++;
	}

	vk::VertexInputBindingDescription xBindDesc = vk::VertexInputBindingDescription()
		.setBinding(0)
		.setStride(xVertexLayout.GetStride())
		.setInputRate(vk::VertexInputRate::eVertex);
	xBindDescs.push_back(xBindDesc);

	const Flux_BufferLayout& xInstanceLayout = xDesc.m_xPerInstanceLayout;
	if (xDesc.m_xPerInstanceLayout.GetElements().GetSize())
	{
		for (Zenith_Vector<Flux_BufferElement>::Iterator xIt(xInstanceLayout.GetElements()); !xIt.Done(); xIt.Next())
		{
			const Flux_BufferElement& xElement = xIt.GetData();

			vk::VertexInputAttributeDescription xInstanceAttrDesc = vk::VertexInputAttributeDescription()
				.setBinding(1)
				.setLocation(uBindPoint)
				.setOffset(xElement.m_uOffset)
				.setFormat(Zenith_Vulkan::ShaderDataTypeToVulkanFormat(xElement._Type));
			xAttrDescs.push_back(xInstanceAttrDesc);
			uBindPoint++;
		}

		vk::VertexInputBindingDescription xInstanceBindDesc = vk::VertexInputBindingDescription()
			.setBinding(1)
			.setStride(xInstanceLayout.GetStride())
			.setInputRate(vk::VertexInputRate::eInstance);
		xBindDescs.push_back(xInstanceBindDesc);
	}

	return std::move(vk::PipelineVertexInputStateCreateInfo()
		.setVertexBindingDescriptionCount(xBindDescs.size())
		.setPVertexBindingDescriptions(xBindDescs.data())
		.setVertexAttributeDescriptionCount(xAttrDescs.size())
		.setPVertexAttributeDescriptions(xAttrDescs.data()));
}

Zenith_Vulkan_PipelineBuilder::Zenith_Vulkan_PipelineBuilder()
{
	m_axDynamicStateEnables[0] = vk::DynamicState::eViewport;
	m_axDynamicStateEnables[1] = vk::DynamicState::eScissor;

	m_xDynamicCreate.setDynamicStateCount(2);
	m_xDynamicCreate.setPDynamicStates(m_axDynamicStateEnables);

	m_xSampleCreate.setRasterizationSamples(vk::SampleCountFlagBits::e1);

	m_xViewportCreate.setViewportCount(1);
	m_xViewportCreate.setScissorCount(1);

	m_xPipelineCreate.setPViewportState(&m_xViewportCreate);

	m_xDepthStencilCreate.setDepthCompareOp(vk::CompareOp::eAlways)
		.setDepthTestEnable(false)
		.setDepthWriteEnable(false)
		.setStencilTestEnable(false)
		.setDepthBoundsTestEnable(false);

	m_xDepthRenderingFormat = vk::Format::eUndefined;
	m_xStencilRenderingFormat = vk::Format::eUndefined;

	m_xRasterCreate.setCullMode(vk::CullModeFlagBits::eNone)
		.setPolygonMode(vk::PolygonMode::eFill)
		.setFrontFace(vk::FrontFace::eCounterClockwise)
		.setLineWidth(1.0f);

	m_xInputAsmCreate.setTopology(vk::PrimitiveTopology::eTriangleList);
}

Zenith_Vulkan_Pipeline::~Zenith_Vulkan_Pipeline()
{
	STUBBED
}

Zenith_Vulkan_PipelineBuilder& Zenith_Vulkan_PipelineBuilder::WithDepthState(vk::CompareOp op, bool depthEnabled, bool writeEnabled, bool stencilEnabled)
{
	m_xDepthStencilCreate.setDepthCompareOp(op)
		.setDepthTestEnable(depthEnabled)
		.setDepthWriteEnable(writeEnabled)
		.setStencilTestEnable(stencilEnabled);
	return *this;
}

Zenith_Vulkan_PipelineBuilder& Zenith_Vulkan_PipelineBuilder::WithBlendState(vk::BlendFactor srcState, vk::BlendFactor dstState, bool isEnabled)
{
	vk::PipelineColorBlendAttachmentState pipeBlend;

	pipeBlend.setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA)
		.setBlendEnable(isEnabled)
		.setAlphaBlendOp(vk::BlendOp::eAdd)
		.setColorBlendOp(vk::BlendOp::eAdd)

		.setSrcAlphaBlendFactor(srcState)
		.setSrcColorBlendFactor(srcState)

		.setDstAlphaBlendFactor(dstState)
		.setDstColorBlendFactor(dstState);

	m_xBlendAttachStates.emplace_back(pipeBlend);

	return *this;
}

Zenith_Vulkan_PipelineBuilder& Zenith_Vulkan_PipelineBuilder::WithRaster(vk::CullModeFlagBits cullMode, vk::PolygonMode polyMode)
{
	m_xRasterCreate.setCullMode(cullMode).setPolygonMode(polyMode);
	return *this;
}

Zenith_Vulkan_PipelineBuilder& Zenith_Vulkan_PipelineBuilder::WithVertexInputState(const vk::PipelineVertexInputStateCreateInfo& spec)
{
	m_xVertexCreate = spec;
	return *this;
}

Zenith_Vulkan_PipelineBuilder& Zenith_Vulkan_PipelineBuilder::WithTopology(vk::PrimitiveTopology topology)
{
	m_xInputAsmCreate.setTopology(topology);
	return *this;
}

Zenith_Vulkan_PipelineBuilder& Zenith_Vulkan_PipelineBuilder::WithShader(const Zenith_Vulkan_Shader& shader)
{
	shader.FillShaderStageCreateInfo(m_xPipelineCreate);
	return *this;
}

Zenith_Vulkan_PipelineBuilder& Zenith_Vulkan_PipelineBuilder::WithLayout(vk::PipelineLayout layout)
{
	this->m_xPipelineLayout = layout;
	m_xPipelineCreate.setLayout(layout);
	return *this;
}

Zenith_Vulkan_PipelineBuilder& Zenith_Vulkan_PipelineBuilder::WithPushConstant(vk::ShaderStageFlags flags, uint32_t offset)
{
	const vk::PhysicalDevice& xPhysicalDevice = Zenith_Vulkan::GetPhysicalDevice();
	m_xAllPushConstants.emplace_back(vk::PushConstantRange(flags, offset, xPhysicalDevice.getProperties().limits.maxPushConstantsSize));
	return *this;
}

Zenith_Vulkan_PipelineBuilder& Zenith_Vulkan_PipelineBuilder::WithPass(vk::RenderPass renderPass)
{
	m_xPipelineCreate.setRenderPass(renderPass);
	return *this;
}

Zenith_Vulkan_PipelineBuilder& Zenith_Vulkan_PipelineBuilder::WithDepthStencilFormat(vk::Format depthFormat)
{
	m_xDepthRenderingFormat = depthFormat;
	m_xStencilRenderingFormat = depthFormat;
	return *this;
}

Zenith_Vulkan_PipelineBuilder& Zenith_Vulkan_PipelineBuilder::WithDepthFormat(vk::Format depthFormat)
{
	m_xDepthRenderingFormat = depthFormat;
	return *this;
}

Zenith_Vulkan_PipelineBuilder& Zenith_Vulkan_PipelineBuilder::WithTesselation()
{
	m_bUseTesselation = true;
	m_xInputAsmCreate.setTopology(vk::PrimitiveTopology::ePatchList);
	m_xTesselationCreate.setPatchControlPoints(3);
	return *this;
}

Zenith_Vulkan_PipelineBuilder& Zenith_Vulkan_PipelineBuilder::WithDescriptorSetLayout(uint32_t slot, vk::DescriptorSetLayout layout)
{
	m_xAllLayouts.push_back(layout);
	return *this;
}

//VulkanPipelineBuilder& VulkanPipelineBuilder::WithDescriptorBuffers() {
//	m_xPipelineCreate.flags |= vk::PipelineCreateFlagBits::eDescriptorBufferEXT;
//	return *this;
//}

void Zenith_Vulkan_PipelineBuilder::Build(Zenith_Vulkan_Pipeline& xPipelineOut, const Flux_PipelineSpecification& xSpec, vk::PipelineCache xCache /*= {}*/)
{
	STUBBED
}

vk::Format VceFormatToVKFormat(TextureFormat eFmt)
{
	switch (eFmt)
	{
	case TEXTURE_FORMAT_R16G16B16A16_UNORM:
		return vk::Format::eR16G16B16A16Unorm;
	case TEXTURE_FORMAT_BGRA8_SRGB:
		return vk::Format::eB8G8R8A8Srgb;
	case TEXTURE_FORMAT_BGRA8_UNORM:
		return vk::Format::eB8G8R8A8Unorm;
	default:
		Zenith_Assert(false, "Unsupported format");
	}
}

vk::CompareOp VceCompareFuncToVkCompareFunc(DepthCompareFunc eFunc)
{
	switch (eFunc)
	{
	case DEPTH_COMPARE_FUNC_GREATEREQUAL:
		return vk::CompareOp::eGreaterOrEqual;
	case DEPTH_COMPARE_FUNC_LESSEQUAL:
		return vk::CompareOp::eLessOrEqual;
	case DEPTH_COMPARE_FUNC_NEVER:
		return vk::CompareOp::eNever;
	case DEPTH_COMPARE_FUNC_ALWAYS:
		return vk::CompareOp::eAlways;
	case DEPTH_COMPARE_FUNC_DISABLED:
		return vk::CompareOp::eAlways;
	default:
		Zenith_Assert(false, "Unsupported blend factor");
	}
}

vk::BlendFactor FluxBlendFactorToVK(BlendFactor eFactor)
{
	switch (eFactor)
	{
	case BLEND_FACTOR_SRCALPHA:
		return vk::BlendFactor::eSrcAlpha;
	case BLEND_FACTOR_ONEMINUSSRCALPHA:
		return vk::BlendFactor::eOneMinusSrcAlpha;
	case BLEND_FACTOR_ONE:
		return vk::BlendFactor::eOne;
	case BLEND_FACTOR_ZERO:
		return vk::BlendFactor::eZero;
	default:
		Zenith_Assert(false, "Unsupported blend factor");
	}
}

vk::RenderPass Zenith_Vulkan_Pipeline::TargetSetupToRenderPass(Flux_TargetSetup& xTargetSetup, LoadAction eColourLoad, StoreAction eColourStore, LoadAction eDepthStencilLoad, StoreAction eDepthStencilStore, RenderTargetUsage eUsage)
{
	uint32_t uNumColourAttachments = 0;
	for (uint32_t i = 0; i < FLUX_MAX_TARGETS; i++)
	{
		if (xTargetSetup.m_axColourAttachments[i].m_eFormat != TEXTURE_FORMAT_NONE)
		{
			uNumColourAttachments++;
		}
		else
		{
			break;
		}
	}

	vk::ImageLayout eLayout;
	switch (eUsage)
	{
	case RENDER_TARGET_USAGE_RENDERTARGET:
		eLayout = vk::ImageLayout::eColorAttachmentOptimal;
		break;
	case RENDER_TARGET_USAGE_PRESENT:
		eLayout = vk::ImageLayout::ePresentSrcKHR;
		break;
	default:
		Zenith_Assert(false, "Unsupported usage");
	}

	std::vector<vk::AttachmentDescription> xAttachmentDescs(uNumColourAttachments);
	std::vector<vk::AttachmentReference> xAttachmentRefs(uNumColourAttachments);
	for (uint32_t i = 0; i < uNumColourAttachments; i++)
	{
		const Flux_RenderAttachment& xTarget = xTargetSetup.m_axColourAttachments[i];
		xAttachmentDescs.at(i)
			.setFormat(Zenith_Vulkan_Texture::ConvertToVkFormat_Colour(xTarget.m_eFormat))
			.setSamples(vk::SampleCountFlagBits::e1)
			.setLoadOp(Zenith_Vulkan_Texture::ConvertToVkLoadAction(eColourLoad))
			.setStoreOp(Zenith_Vulkan_Texture::ConvertToVkStoreAction(eColourStore))
			.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
			.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
			.setInitialLayout(eLayout)
			.setFinalLayout(eLayout);

		xAttachmentRefs.at(i)
			.setAttachment(i)
			.setLayout(vk::ImageLayout::eColorAttachmentOptimal);
	}

	//should probably have a better way of checking for existence of depth/stencil target
	bool bHasDepth = xTargetSetup.m_pxDepthStencil != nullptr;
	vk::AttachmentDescription xDepthStencilAttachment;
	vk::AttachmentReference xDepthStencilAttachmentRef;
	if (bHasDepth)
	{
		xDepthStencilAttachment = vk::AttachmentDescription()
			.setFormat(Zenith_Vulkan_Texture::ConvertToVkFormat_DepthStencil(xTargetSetup.m_pxDepthStencil->m_eFormat))
			.setSamples(vk::SampleCountFlagBits::e1)
			.setLoadOp(Zenith_Vulkan_Texture::ConvertToVkLoadAction(eDepthStencilLoad))
			.setStoreOp(Zenith_Vulkan_Texture::ConvertToVkStoreAction(eDepthStencilStore))
			.setStencilLoadOp(Zenith_Vulkan_Texture::ConvertToVkLoadAction(eDepthStencilLoad))
			.setStencilStoreOp(Zenith_Vulkan_Texture::ConvertToVkStoreAction(eDepthStencilStore))
			.setInitialLayout(eDepthStencilLoad == LOAD_ACTION_LOAD ? vk::ImageLayout::eDepthStencilReadOnlyOptimal : vk::ImageLayout::eUndefined)
			.setFinalLayout(vk::ImageLayout::eDepthStencilReadOnlyOptimal);

		xDepthStencilAttachmentRef = vk::AttachmentReference()
			.setAttachment(uNumColourAttachments)
			.setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

		xAttachmentDescs.push_back(xDepthStencilAttachment);
		xAttachmentRefs.push_back(xDepthStencilAttachmentRef);
	}

	vk::SubpassDescription xSubpass = vk::SubpassDescription()
		.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
		.setColorAttachmentCount(uNumColourAttachments)
		.setPColorAttachments(xAttachmentRefs.data());
	if (bHasDepth)
	{
		xSubpass.setPDepthStencilAttachment(&xDepthStencilAttachmentRef);
	}

	vk::RenderPassCreateInfo xRenderPassInfo = vk::RenderPassCreateInfo()
		.setAttachmentCount(xAttachmentDescs.size())
		.setPAttachments(xAttachmentDescs.data())
		.setSubpassCount(1)
		.setPSubpasses(&xSubpass)
		.setDependencyCount(0)
		.setPDependencies(nullptr);

	return Zenith_Vulkan::GetDevice().createRenderPass(xRenderPassInfo);
}

vk::Framebuffer Zenith_Vulkan_Pipeline::TargetSetupToFramebuffer(Flux_TargetSetup& xTargetSetup, uint32_t uWidth, uint32_t uHeight, const vk::RenderPass& xPass)
{
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();
	bool bHasDepth = xTargetSetup.m_pxDepthStencil != nullptr;

	uint32_t uNumColourAttachments = 0;
	for (uint32_t i = 0; i < FLUX_MAX_TARGETS; i++)
	{
		if (xTargetSetup.m_axColourAttachments[i].m_eFormat != TEXTURE_FORMAT_NONE)
		{
			uNumColourAttachments++;
		}
		else
		{
			break;
		}
	}

	const uint32_t uNumAttachments = bHasDepth ? uNumColourAttachments + 1 : uNumColourAttachments;

	vk::FramebufferCreateInfo framebufferInfo{};

	vk::ImageView axAttachments[FLUX_MAX_TARGETS];
	for (uint32_t i = 0; i < uNumColourAttachments; i++)
	{
		axAttachments[i] = xTargetSetup.m_axColourAttachments[i].m_pxTargetTexture->GetImageView();
	}
	if (bHasDepth)
	{
		axAttachments[uNumAttachments - 1] = xTargetSetup.m_pxDepthStencil->m_pxTargetTexture->GetImageView();
	}

	framebufferInfo.renderPass = xPass;
	framebufferInfo.attachmentCount = uNumAttachments;
	framebufferInfo.pAttachments = axAttachments;

	framebufferInfo.width = uWidth;
	framebufferInfo.height = uHeight;

	framebufferInfo.layers = 1;

	vk::Framebuffer xFrameBuffer = xDevice.createFramebuffer(framebufferInfo);
	return xFrameBuffer;
}

void Zenith_Vulkan_PipelineBuilder::FromSpecification(Zenith_Vulkan_Pipeline& xPipelineOut, const Flux_PipelineSpecification& xSpec)
{
	vk::GraphicsPipelineCreateInfo xPipelineInfo;

#pragma region Shaders
	xPipelineInfo.setStageCount(xSpec.m_pxShader->m_uStageCount);
	xPipelineInfo.setPStages(xSpec.m_pxShader->m_xInfos);
#pragma endregion

#pragma region VertexDesc

	std::vector<vk::VertexInputBindingDescription> xBindDescs;
	std::vector<vk::VertexInputAttributeDescription> xAttrDescs;

	uint32_t uBindPoint = 0;
	const Flux_BufferLayout& xVertexLayout = xSpec.m_xVertexInputDesc.m_xPerVertexLayout;
	if (xVertexLayout.GetElements().GetSize())
	{
		for (Zenith_Vector<Flux_BufferElement>::Iterator xIt(xVertexLayout.GetElements()); !xIt.Done(); xIt.Next())
		{
			const Flux_BufferElement& xElement = xIt.GetData();
			vk::VertexInputAttributeDescription xAttrDesc = vk::VertexInputAttributeDescription()
				.setBinding(0)
				.setLocation(uBindPoint)
				.setOffset(xElement.m_uOffset)
				.setFormat(Zenith_Vulkan::ShaderDataTypeToVulkanFormat(xElement._Type));
			xAttrDescs.push_back(xAttrDesc);
			uBindPoint++;
		}
	}

	vk::VertexInputBindingDescription xBindDesc = vk::VertexInputBindingDescription()
		.setBinding(0)
		.setStride(xVertexLayout.GetStride())
		.setInputRate(vk::VertexInputRate::eVertex);
	xBindDescs.push_back(xBindDesc);

	const Flux_BufferLayout& xInstanceLayout = xSpec.m_xVertexInputDesc.m_xPerInstanceLayout;
	if (xInstanceLayout.GetElements().GetSize())
	{
		for (Zenith_Vector<Flux_BufferElement>::Iterator xIt(xInstanceLayout.GetElements()); !xIt.Done(); xIt.Next())
		{
			const Flux_BufferElement& xElement = xIt.GetData();
			vk::VertexInputAttributeDescription xInstanceAttrDesc = vk::VertexInputAttributeDescription()
				.setBinding(1)
				.setLocation(uBindPoint)
				.setOffset(xElement.m_uOffset)
				.setFormat(Zenith_Vulkan::ShaderDataTypeToVulkanFormat(xElement._Type));
			xAttrDescs.push_back(xInstanceAttrDesc);
			uBindPoint++;
		}

		vk::VertexInputBindingDescription xInstanceBindDesc = vk::VertexInputBindingDescription()
			.setBinding(1)
			.setStride(xInstanceLayout.GetStride())
			.setInputRate(vk::VertexInputRate::eInstance);
		xBindDescs.push_back(xInstanceBindDesc);
	}

	vk::PipelineVertexInputStateCreateInfo xVertexDesc = vk::PipelineVertexInputStateCreateInfo()
		.setVertexBindingDescriptionCount(xBindDescs.size())
		.setPVertexBindingDescriptions(xBindDescs.data())
		.setVertexAttributeDescriptionCount(xAttrDescs.size())
		.setPVertexAttributeDescriptions(xAttrDescs.data());

	vk::PipelineInputAssemblyStateCreateInfo xTopology = vk::PipelineInputAssemblyStateCreateInfo();
	switch (xSpec.m_xVertexInputDesc.m_eTopology)
	{
	case(MESH_TOPOLOGY_TRIANGLES):
		xTopology.setTopology(vk::PrimitiveTopology::eTriangleList);
		break;
	default:
		xTopology.setTopology(vk::PrimitiveTopology::eTriangleList);
		break;
	}

	xPipelineInfo.setPVertexInputState(&xVertexDesc);
	xPipelineInfo.setPInputAssemblyState(&xTopology);
#pragma endregion

#pragma region BlendStates
	vk::PipelineColorBlendStateCreateInfo xBlendInfo;
	vk::PipelineColorBlendAttachmentState axBlendInfo[FLUX_MAX_TARGETS];
	for (u_int u = 0; u < xSpec.m_pxTargetSetup->GetNumColourAttachments(); u++)
	{
		vk::PipelineColorBlendAttachmentState& xBlendInfo = axBlendInfo[u]
			.setColorWriteMask(
				vk::ColorComponentFlagBits::eR |
				vk::ColorComponentFlagBits::eG |
				vk::ColorComponentFlagBits::eB |
				vk::ColorComponentFlagBits::eA)
			.setBlendEnable(xSpec.m_axBlendStates[u].m_bBlendEnabled)
			.setAlphaBlendOp(vk::BlendOp::eAdd)
			.setColorBlendOp(vk::BlendOp::eAdd)
			.setSrcAlphaBlendFactor(FluxBlendFactorToVK(xSpec.m_axBlendStates[u].m_eSrcBlendFactor))
			.setSrcColorBlendFactor(FluxBlendFactorToVK(xSpec.m_axBlendStates[u].m_eSrcBlendFactor))
			.setDstAlphaBlendFactor(FluxBlendFactorToVK(xSpec.m_axBlendStates[u].m_eDstBlendFactor))
			.setDstColorBlendFactor(FluxBlendFactorToVK(xSpec.m_axBlendStates[u].m_eDstBlendFactor));
	}
	xBlendInfo.setAttachmentCount(xSpec.m_pxTargetSetup->GetNumColourAttachments());
	xBlendInfo.setPAttachments(axBlendInfo);

	xPipelineInfo.setPColorBlendState(&xBlendInfo);
#pragma endregion

#pragma region DepthStencilState
	vk::PipelineDepthStencilStateCreateInfo xDepthStencilInfo = vk::PipelineDepthStencilStateCreateInfo()
		.setDepthCompareOp(VceCompareFuncToVkCompareFunc(xSpec.m_eDepthCompareFunc))
		.setDepthTestEnable(xSpec.m_bDepthTestEnabled)
		.setDepthWriteEnable(xSpec.m_bDepthWriteEnabled)
		.setDepthBoundsTestEnable(false)
		.setStencilTestEnable(false); //#TO_TODO: stencil

	xPipelineInfo.setPDepthStencilState(&xDepthStencilInfo);
#pragma endregion

#pragma region RenderPass
	xPipelineInfo.setRenderPass(Zenith_Vulkan_Pipeline::TargetSetupToRenderPass(*xSpec.m_pxTargetSetup, LOAD_ACTION_DONTCARE, STORE_ACTION_DONTCARE, LOAD_ACTION_DONTCARE, STORE_ACTION_DONTCARE, RENDER_TARGET_USAGE_RENDERTARGET));
#pragma endregion

#pragma region RasterState
	vk::PipelineRasterizationStateCreateInfo xRasterInfo = vk::PipelineRasterizationStateCreateInfo()
		.setCullMode(vk::CullModeFlagBits::eNone)
		.setFrontFace(vk::FrontFace::eCounterClockwise)
		.setPolygonMode(xSpec.m_bWireframe ? vk::PolygonMode::eLine : vk::PolygonMode::eFill)
		.setLineWidth(1.0f);
	if (xSpec.m_bDepthBias)
	{
		xRasterInfo
			.setDepthBiasEnable(true)
			.setDepthBiasConstantFactor(xSpec.m_fDepthBiasConstant)
			.setDepthBiasSlopeFactor(xSpec.m_fDepthBiasSlope)
			.setDepthBiasClamp(xSpec.m_fDepthBiasClamp);
	}

	xPipelineInfo.setPRasterizationState(&xRasterInfo);
#pragma endregion

#pragma region Viewport
	vk::PipelineViewportStateCreateInfo xViewportInfo = vk::PipelineViewportStateCreateInfo()
		.setViewportCount(1)
		.setScissorCount(1);

	xPipelineInfo.setPViewportState(&xViewportInfo);
#pragma endregion

#pragma region DynamicState
	vk::DynamicState aeDynamicState[] =
	{
		vk::DynamicState::eViewport,
		vk::DynamicState::eScissor,
	};
	vk::PipelineDynamicStateCreateInfo xDynamicState = vk::PipelineDynamicStateCreateInfo()
		.setDynamicStateCount(sizeof(aeDynamicState) / sizeof(aeDynamicState[0]))
		.setPDynamicStates(aeDynamicState);

	xPipelineInfo.setPDynamicState(&xDynamicState);
#pragma endregion

#pragma region Multisample
	vk::PipelineMultisampleStateCreateInfo xMultisampleInfo = vk::PipelineMultisampleStateCreateInfo()
		.setRasterizationSamples(vk::SampleCountFlagBits::e1);

	xPipelineInfo.setPMultisampleState(&xMultisampleInfo);
#pragma endregion

#pragma region PipelineLayout
	Zenith_Vulkan_RootSigBuilder::FromSpecification(xPipelineOut.m_xRootSig, xSpec.m_xPipelineLayout);
	xPipelineInfo.setLayout(xPipelineOut.m_xRootSig.m_xLayout);
#pragma endregion

	xPipelineOut.m_xPipeline = Zenith_Vulkan::GetDevice().createGraphicsPipeline(VK_NULL_HANDLE, xPipelineInfo).value;
}

void Zenith_Vulkan_RootSigBuilder::FromSpecification(Zenith_Vulkan_RootSig& xRootSigOut, const Flux_PipelineLayout& xSpec)
{
	xRootSigOut.m_uNumDescriptorSets = xSpec.m_uNumDescriptorSets;
	for (u_int uDescSet = 0; uDescSet < xSpec.m_uNumDescriptorSets; uDescSet++)
	{
		const Flux_DescriptorSetLayout& xLayout = xSpec.m_axDescriptorSetLayouts[uDescSet];

		if (xLayout.m_axBindings[0].m_eType == DESCRIPTOR_TYPE_UNBOUNDED_TEXTURES)
		{
			xRootSigOut.m_axDescSetLayouts[uDescSet] = Zenith_Vulkan::GetBindlessTexturesDescriptorSetLayout();
			continue;
		}

		vk::DescriptorSetLayoutCreateInfo xInfo = vk::DescriptorSetLayoutCreateInfo();
		vk::DescriptorSetLayoutBinding axBindings[FLUX_MAX_DESCRIPTOR_BINDINGS];
		u_int uNumDescriptors = 0;
		for (u_int uDesc = 0; uDesc < FLUX_MAX_DESCRIPTOR_BINDINGS; uDesc++)
		{
			if (xLayout.m_axBindings[uDesc].m_eType == DESCRIPTOR_TYPE_MAX)
			{
				break;
			}

			// Store descriptor type for later use in command buffer
			xRootSigOut.m_axDescriptorTypes[uDescSet][uDesc] = xLayout.m_axBindings[uDesc].m_eType;

			vk::DescriptorSetLayoutBinding& xBinding = axBindings[uDesc]
				.setBinding(uDesc)
				.setDescriptorCount(1)
				.setStageFlags(vk::ShaderStageFlagBits::eAll);

			switch (xLayout.m_axBindings[uDesc].m_eType)
			{
			case(DESCRIPTOR_TYPE_BUFFER):
				xBinding.setDescriptorType(vk::DescriptorType::eUniformBuffer);
				break;
			case(DESCRIPTOR_TYPE_TEXTURE):
				xBinding.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
				break;
			case(DESCRIPTOR_TYPE_STORAGE_IMAGE):
				xBinding.setDescriptorType(vk::DescriptorType::eStorageImage);
				break;
			case(DESCRIPTOR_TYPE_UNBOUNDED_TEXTURES):
				Zenith_Assert(false, "Unbounded textures must be in their own table");
				break;
			}

			uNumDescriptors++;
		}
		xInfo.setBindingCount(uNumDescriptors);
		xInfo.setPBindings(axBindings);
		xInfo.setFlags(vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool);

		xRootSigOut.m_axDescSetLayouts[uDescSet] = Zenith_Vulkan::GetDevice().createDescriptorSetLayout(xInfo);
	}
	vk::PushConstantRange xPushConstantRange(vk::ShaderStageFlagBits::eAll, 0, Zenith_Vulkan::GetPhysicalDevice().getProperties().limits.maxPushConstantsSize);
	vk::PipelineLayoutCreateInfo xPipelineLayoutInfo = vk::PipelineLayoutCreateInfo()
		.setPSetLayouts(xRootSigOut.m_axDescSetLayouts)
		.setSetLayoutCount(xSpec.m_uNumDescriptorSets)
		.setPushConstantRangeCount(1)
		.setPPushConstantRanges(&xPushConstantRange);

	xRootSigOut.m_xLayout = Zenith_Vulkan::GetDevice().createPipelineLayout(xPipelineLayoutInfo);
}

// ========== COMPUTE PIPELINE BUILDER IMPLEMENTATION ==========

Zenith_Vulkan_ComputePipelineBuilder::Zenith_Vulkan_ComputePipelineBuilder()
{
}

Zenith_Vulkan_ComputePipelineBuilder& Zenith_Vulkan_ComputePipelineBuilder::WithShader(const Zenith_Vulkan_Shader& shader)
{
	m_pxShader = &shader;
	return *this;
}

Zenith_Vulkan_ComputePipelineBuilder& Zenith_Vulkan_ComputePipelineBuilder::WithLayout(vk::PipelineLayout layout)
{
	m_xLayout = layout;
	return *this;
}

void Zenith_Vulkan_ComputePipelineBuilder::Build(Zenith_Vulkan_Pipeline& pipelineOut)
{
	Zenith_Assert(m_pxShader != nullptr, "Compute shader not set");
	Zenith_Assert(m_xLayout != VK_NULL_HANDLE, "Pipeline layout not set");
	
	vk::PipelineShaderStageCreateInfo stageInfo = vk::PipelineShaderStageCreateInfo()
		.setStage(vk::ShaderStageFlagBits::eCompute)
		.setModule(m_pxShader->m_xCompShaderModule)
		.setPName("main");
	
	vk::ComputePipelineCreateInfo pipelineInfo = vk::ComputePipelineCreateInfo()
		.setStage(stageInfo)
		.setLayout(m_xLayout);
	
	vk::Result result = Zenith_Vulkan::GetDevice().createComputePipelines(
		VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipelineOut.m_xPipeline);
	
	if (result != vk::Result::eSuccess)
	{
		Zenith_Error("Failed to create compute pipeline");
	}
}
