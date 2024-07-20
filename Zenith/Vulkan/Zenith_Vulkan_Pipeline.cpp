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

Zenith_Vulkan_PipelineSpecification::Zenith_Vulkan_PipelineSpecification(Flux_VertexInputDescription xVertexInputDesc, Zenith_Vulkan_Shader* pxShader, std::vector<Flux_BlendState> xBlendStates, bool bDepthTestEnabled, bool bDepthWriteEnabled, DepthCompareFunc eDepthCompareFunc, std::vector<ColourFormat> aeColourFormats, DepthStencilFormat eDepthStencilFormat, bool bUsePushConstants, bool bUseTesselation, std::vector<std::array<uint32_t, DESCRIPTOR_TYPE_MAX>> xDescSetBindings, Flux_TargetSetup& xTargetSetup, LoadAction eColourLoad, StoreAction eColourStore, LoadAction eDepthStencilLoad, StoreAction eDepthStencilStore, RenderTargetUsage eUsage)
	: m_eVertexInputDesc(xVertexInputDesc)
	, m_pxShader(pxShader)
	, m_xBlendStates(xBlendStates)
	, m_bDepthTestEnabled(bDepthTestEnabled)
	, m_bDepthWriteEnabled(bDepthWriteEnabled)
	, m_eDepthCompareFunc(eDepthCompareFunc)
	, m_aeColourFormats(aeColourFormats)
	, m_eDepthStencilFormat(eDepthStencilFormat)
	, m_bUsePushConstants(bUsePushConstants)
	, m_bUseTesselation(bUseTesselation)
	, m_xDescSetBindings(xDescSetBindings)
	, m_xTargetSetup(xTargetSetup)
	, m_eColourLoadAction(eColourLoad)
	, m_eColourStoreAction(eColourStore)
	, m_eDepthStencilLoadAction(eDepthStencilLoad)
	, m_eDepthStencilStoreAction(eDepthStencilStore)
	, m_eTargetUsage(eUsage)
{
}

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

Zenith_Vulkan_Shader::~Zenith_Vulkan_Shader()
{
	const vk::Device xDevice = Zenith_Vulkan::GetDevice();
	xDevice.destroyShaderModule(m_xVertShaderModule);
	xDevice.destroyShaderModule(m_xFragShaderModule);
	xDevice.destroyShaderModule(m_xTescShaderModule);
	xDevice.destroyShaderModule(m_xTeseShaderModule);
	delete[] m_pcVertShaderCode;
	delete[] m_pcFragShaderCode;
	delete[] m_pcTescShaderCode;
	delete[] m_pcTeseShaderCode;
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

static class Zenith_Vulkan_DescriptorSetLayoutBuilder
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

		if (m_bUsingBindless)
		{
			for (int i = 0; i < m_xAddedBindings.size(); ++i)
			{
				if (m_xAddedBindings[i].descriptorType != vk::DescriptorType::eAccelerationStructureKHR)
				{
					//#TO_TODO: why do I get geometry and lighting flickering without this???
					//xBindingFlags.push_back(vk::DescriptorBindingFlagBits::eUpdateAfterBind);
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
	for (const Flux_BufferElement& xElement : xVertexLayout.GetElements())
	{
		vk::VertexInputAttributeDescription xAttrDesc = vk::VertexInputAttributeDescription()
			.setBinding(0)
			.setLocation(uBindPoint)
			.setOffset(xElement._Offset)
			.setFormat(Zenith_Vulkan::ShaderDataTypeToVulkanFormat(xElement._Type));
		xAttrDescs.push_back(xAttrDesc);
		uBindPoint++;
	}

	vk::VertexInputBindingDescription xBindDesc = vk::VertexInputBindingDescription()
		.setBinding(0)
		.setStride(xVertexLayout.m_uStride)
		.setInputRate(vk::VertexInputRate::eVertex);
	xBindDescs.push_back(xBindDesc);



	const Flux_BufferLayout& xInstanceLayout = xDesc.m_xPerInstanceLayout;
	if (xDesc.m_xPerInstanceLayout.GetElements().size())
	{
		for (const Flux_BufferElement& xElement : xInstanceLayout.GetElements())
		{


			vk::VertexInputAttributeDescription xInstanceAttrDesc = vk::VertexInputAttributeDescription()
				.setBinding(1)
				.setLocation(uBindPoint)
				.setOffset(xElement._Offset)
				.setFormat(Zenith_Vulkan::ShaderDataTypeToVulkanFormat(xElement._Type));
			xAttrDescs.push_back(xInstanceAttrDesc);
			uBindPoint++;
		}

		vk::VertexInputBindingDescription xInstanceBindDesc = vk::VertexInputBindingDescription()
			.setBinding(1)
			.setStride(xInstanceLayout.m_uStride)
			.setInputRate(vk::VertexInputRate::eInstance);
		xBindDescs.push_back(xInstanceBindDesc);
	}

	return std::move(vk::PipelineVertexInputStateCreateInfo()
		.setVertexBindingDescriptionCount(xBindDescs.size())
		.setPVertexBindingDescriptions(xBindDescs.data())
		.setVertexAttributeDescriptionCount(xAttrDescs.size())
		.setPVertexAttributeDescriptions(xAttrDescs.data()));
}

void Zenith_Vulkan_Pipeline::BindDescriptorSets(vk::CommandBuffer& xCmd, const std::vector<vk::DescriptorSet>& axSets, vk::PipelineBindPoint eBindPoint, uint32_t ufirstSet) const
{
	xCmd.bindDescriptorSets(eBindPoint, m_xPipelineLayout, ufirstSet, axSets.size(), axSets.data(), 0, nullptr);
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

	m_xRasterCreate.setCullMode(vk::CullModeFlagBits::eBack)
		.setPolygonMode(vk::PolygonMode::eFill)
		.setFrontFace(vk::FrontFace::eCounterClockwise)
		.setLineWidth(1.0f);

	m_xInputAsmCreate.setTopology(vk::PrimitiveTopology::eTriangleList);
}

Zenith_Vulkan_Pipeline::~Zenith_Vulkan_Pipeline()
{
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();
	xDevice.destroyPipeline(m_xPipeline);
	xDevice.destroyPipelineLayout(m_xPipelineLayout);
	for (vk::DescriptorSetLayout xLayout : m_axDescLayouts)
	{
		xDevice.destroyDescriptorSetLayout(xLayout);
	}
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

	Zenith_Vulkan_PipelineBuilder& Zenith_Vulkan_PipelineBuilder::WithPass(vk::RenderPass& renderPass)
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

	Zenith_Vulkan_PipelineBuilder& Zenith_Vulkan_PipelineBuilder::WithColourFormats(const std::vector<ColourFormat>& formats)
	{
		m_xAllColourRenderingFormats = formats;
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
		assert(slot < 32);
		while (m_xAllLayouts.size() <= slot)
		{
			m_xAllLayouts.push_back(vk::DescriptorSetLayout());
		}
		m_xAllLayouts[slot] = layout;
		return *this;
	}

	//VulkanPipelineBuilder& VulkanPipelineBuilder::WithDescriptorBuffers() {
	//	m_xPipelineCreate.flags |= vk::PipelineCreateFlagBits::eDescriptorBufferEXT;
	//	return *this;
	//}

	void Zenith_Vulkan_PipelineBuilder::Build(Zenith_Vulkan_Pipeline& xPipelineOut, vk::PipelineCache xCache /*= {}*/)
	{
		const vk::Device& xDevice = Zenith_Vulkan::GetDevice();
		vk::PipelineLayoutCreateInfo xPipeLayoutCreate = vk::PipelineLayoutCreateInfo()
			.setSetLayouts(m_xAllLayouts)
			.setPushConstantRanges(m_xAllPushConstants);

		if (m_xBlendAttachStates.empty())
		{
			if (!m_xAllColourRenderingFormats.empty())
			{
				for (int i = 0; i < m_xAllColourRenderingFormats.size(); ++i)
				{
					WithBlendState(vk::BlendFactor::eSrcAlpha, vk::BlendFactor::eOneMinusSrcAlpha, false);
				}
			}
			else
			{
				WithBlendState(vk::BlendFactor::eSrcAlpha, vk::BlendFactor::eOneMinusSrcAlpha, false);
			}
		}

		m_xBlendCreate.setAttachments(m_xBlendAttachStates);
		m_xBlendCreate.setBlendConstants({ 1.0f, 1.0f, 1.0f, 1.0f });

		int uShouldReturn = 0;

		xPipelineOut.m_xPipelineLayout = xDevice.createPipelineLayout(xPipeLayoutCreate);

		m_xPipelineCreate.setPColorBlendState(&m_xBlendCreate)
			.setPDepthStencilState(&m_xDepthStencilCreate)
			.setPDynamicState(&m_xDynamicCreate)
			.setPInputAssemblyState(&m_xInputAsmCreate)
			.setPMultisampleState(&m_xSampleCreate)
			.setPRasterizationState(&m_xRasterCreate)
			.setLayout(xPipelineOut.m_xPipelineLayout)
			.setPVertexInputState(&m_xVertexCreate);

		if (m_bUseTesselation)
		{
			m_xPipelineCreate = m_xPipelineCreate.setPTessellationState(&m_xTesselationCreate);
		}
		xPipelineOut.m_xPipeline = xDevice.createGraphicsPipeline(VK_NULL_HANDLE, m_xPipelineCreate).value;
		
	}

	vk::Format VceFormatToVKFormat(ColourFormat eFmt)
	{
		switch (eFmt)
		{
		case COLOUR_FORMAT_BGRA8_SRGB:
			return vk::Format::eB8G8R8A8Srgb;
		case COLOUR_FORMAT_BGRA8_UNORM:
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

	vk::BlendFactor VceBlendFactorToVKBlendFactor(BlendFactor eFactor)
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


	Zenith_Vulkan_PipelineBuilder::DescriptorThings Zenith_Vulkan_PipelineBuilder::HandleDescriptors(const Zenith_Vulkan_PipelineSpecification& spec, Zenith_Vulkan_PipelineBuilder& xBuilder)
	{
		const vk::Device& xDevice = Zenith_Vulkan::GetDevice();
		uint32_t uSetIndex = 0;
		std::vector<vk::DescriptorSetLayout> xLayouts;
		std::vector<vk::DescriptorSet> xSets;

		for (std::array<uint32_t, DESCRIPTOR_TYPE_MAX> xSetDesc : spec.m_xDescSetBindings)
		{
			Zenith_Vulkan_DescriptorSetLayoutBuilder xDescBuilder = Zenith_Vulkan_DescriptorSetLayoutBuilder().WithBindlessAccess();

			for (uint32_t i = 0; i < xSetDesc[DESCRIPTOR_TYPE_BUFFER]; i++)
				xDescBuilder = xDescBuilder.WithUniformBuffers(1);
			for (uint32_t i = 0; i < xSetDesc[DESCRIPTOR_TYPE_TEXTURE]; i++)
				xDescBuilder = xDescBuilder.WithSamplers(1);
#ifdef VCE_RAYTRACING
			for (uint32_t i = 0; i < xSetDesc[DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE]; i++)
				xDescBuilder = xDescBuilder.WithAccelStructures(1);
#endif

			vk::DescriptorSetLayout xLayout = xDescBuilder.Build(xDevice);

			xBuilder = xBuilder.WithDescriptorSetLayout(uSetIndex, xLayout);

			xLayouts.push_back(xLayout);

			xSets.push_back(Zenith_Vulkan::CreateDescriptorSet(xLayout, Zenith_Vulkan::GetDefaultDescriptorPool()));

			uSetIndex++;
		}

		return { xLayouts, xSets };
		
	}

	vk::RenderPass Zenith_Vulkan_Pipeline::TargetSetupToRenderPass(Flux_TargetSetup& xTargetSetup, LoadAction eColourLoad, StoreAction eColourStore, LoadAction eDepthStencilLoad, StoreAction eDepthStencilStore, RenderTargetUsage eUsage)
	{
		uint32_t uNumColourAttachments = 0;
		for (uint32_t i = 0; i < FLUX_MAX_TARGETS; i++)
		{
			if (xTargetSetup.m_axColourAttachments[i].m_eColourFormat != COLOUR_FORMAT_NONE)
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
			eLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
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
				.setFormat(Zenith_Vulkan_Texture::ConvertToVkFormat_Colour(xTarget.m_eColourFormat))
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
		bool bHasDepth = xTargetSetup.m_xDepthStencil.m_eDepthStencilFormat != DEPTHSTENCIL_FORMAT_NONE;
		vk::AttachmentDescription xDepthStencilAttachment;
		vk::AttachmentReference xDepthStencilAttachmentRef;
		if (bHasDepth)
		{
			xDepthStencilAttachment = vk::AttachmentDescription()
				.setFormat(Zenith_Vulkan_Texture::ConvertToVkFormat_DepthStencil(xTargetSetup.m_xDepthStencil.m_eDepthStencilFormat))
				.setSamples(vk::SampleCountFlagBits::e1)
				.setLoadOp(Zenith_Vulkan_Texture::ConvertToVkLoadAction(eDepthStencilLoad))
				.setStoreOp(Zenith_Vulkan_Texture::ConvertToVkStoreAction(eDepthStencilStore))
				.setStencilLoadOp(Zenith_Vulkan_Texture::ConvertToVkLoadAction(eDepthStencilLoad))
				.setStencilStoreOp(Zenith_Vulkan_Texture::ConvertToVkStoreAction(eDepthStencilStore))
				.setInitialLayout(eDepthStencilLoad == LOAD_ACTION_LOAD ? vk::ImageLayout::eDepthStencilAttachmentOptimal : vk::ImageLayout::eUndefined)
				.setFinalLayout(Zenith_Vulkan_Texture::ConvertToVkTargetUsage(eUsage, RENDER_TARGET_TYPE_DEPTHSTENCIL));

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

		vk::SubpassDependency xDependency = vk::SubpassDependency()
			.setSrcSubpass(VK_SUBPASS_EXTERNAL)
			.setDstSubpass(0)
			.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests)
			.setSrcAccessMask(vk::AccessFlagBits::eNone)
			.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests)
			.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite);

		vk::RenderPassCreateInfo xRenderPassInfo = vk::RenderPassCreateInfo()
			.setAttachmentCount(xAttachmentDescs.size())
			.setPAttachments(xAttachmentDescs.data())
			.setSubpassCount(1)
			.setPSubpasses(&xSubpass)
			.setDependencyCount(1)
			.setPDependencies(&xDependency);

		return Zenith_Vulkan::GetDevice().createRenderPass(xRenderPassInfo);
	}

	vk::Framebuffer Zenith_Vulkan_Pipeline::TargetSetupToFramebuffer(Flux_TargetSetup& xTargetSetup, const vk::RenderPass& xPass)
	{
		const vk::Device& xDevice = Zenith_Vulkan::GetDevice();
		const uint32_t uFrameIndex = Zenith_Vulkan_Swapchain::GetCurrentFrameIndex();
		bool bHasDepth = xTargetSetup.m_xDepthStencil.m_eDepthStencilFormat != DEPTHSTENCIL_FORMAT_NONE;

		uint32_t uNumColourAttachments = 0;
		for (uint32_t i = 0; i < FLUX_MAX_TARGETS; i++)
		{
			if (xTargetSetup.m_axColourAttachments[i].m_eColourFormat != COLOUR_FORMAT_NONE)
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
			axAttachments[i] = xTargetSetup.m_axColourAttachments[i].m_axTargetTextures[uFrameIndex].GetImageView();
		}
		if (bHasDepth)
		{
			axAttachments[uNumAttachments - 1] = xTargetSetup.m_xDepthStencil.m_axTargetTextures[uFrameIndex].GetImageView();
		}

		framebufferInfo.renderPass = xPass;
		framebufferInfo.attachmentCount = uNumAttachments;
		framebufferInfo.pAttachments = axAttachments;

		//#TO assuming all targets are same resolution
		framebufferInfo.width = xTargetSetup.m_axColourAttachments[0].m_uWidth;
		framebufferInfo.height = xTargetSetup.m_axColourAttachments[0].m_uHeight;

		framebufferInfo.layers = 1;

		vk::Framebuffer xFrameBuffer = xDevice.createFramebuffer(framebufferInfo);
		return xFrameBuffer;
	}

	void Zenith_Vulkan_PipelineBuilder::FromSpecification(Zenith_Vulkan_Pipeline& xPipelineOut, const Zenith_Vulkan_PipelineSpecification& spec)
	{

		std::vector<vk::VertexInputBindingDescription> axBindDescs;
		std::vector<vk::VertexInputAttributeDescription> axAttrDescs;

		Zenith_Vulkan_PipelineBuilder xBuilder = Zenith_Vulkan_PipelineBuilder();
		if (spec.m_eVertexInputDesc.m_eTopology != MESH_TOPOLOGY_NONE)
		{
			xBuilder = xBuilder.WithVertexInputState(Zenith_Vulkan::VertexDescToVulkanDesc(spec.m_eVertexInputDesc, axBindDescs, axAttrDescs));

			//#TODO utility function
			vk::PrimitiveTopology eTopology;
			switch (spec.m_eVertexInputDesc.m_eTopology)
			{
			case(MESH_TOPOLOGY_TRIANGLES):
				eTopology = vk::PrimitiveTopology::eTriangleList;
				break;
			default:
				Zenith_Assert(false, "Invalid topolgy");
				break;

			}
			xBuilder = xBuilder.WithTopology(eTopology);
		}
		
		xBuilder = xBuilder.WithShader(*dynamic_cast<Zenith_Vulkan_Shader*>(spec.m_pxShader));
		for (const Flux_BlendState& xBlend : spec.m_xBlendStates)
		{
			xBuilder = xBuilder.WithBlendState(VceBlendFactorToVKBlendFactor(xBlend.m_eSrcBlendFactor), VceBlendFactorToVKBlendFactor(xBlend.m_eDstBlendFactor), xBlend.m_bBlendEnabled);
		}
		xBuilder = xBuilder.WithDepthState(VceCompareFuncToVkCompareFunc(spec.m_eDepthCompareFunc), spec.m_bDepthTestEnabled, spec.m_bDepthWriteEnabled, false);
		xBuilder = xBuilder.WithColourFormats(spec.m_aeColourFormats);
		xBuilder = xBuilder.WithDepthFormat(vk::Format::eD32Sfloat);
		
		xBuilder = xBuilder.WithPass(Zenith_Vulkan_Pipeline::TargetSetupToRenderPass(spec.m_xTargetSetup, spec.m_eColourLoadAction, spec.m_eColourStoreAction, spec.m_eDepthStencilLoadAction, spec.m_eDepthStencilStoreAction, spec.m_eTargetUsage));

		DescriptorThings xDescThings = HandleDescriptors(spec, xBuilder);

		if (spec.m_bUsePushConstants) {
			xBuilder = xBuilder.WithPushConstant(vk::ShaderStageFlagBits::eAll, 0);
		}

		if (spec.m_bUseTesselation)
			xBuilder = xBuilder.WithTesselation();

		//HACK HACK HACK HACK HACK HACK HACK HACK HACK
		//HACK HACK HACK HACK HACK HACK HACK HACK HACK
		//HACK HACK HACK HACK HACK HACK HACK HACK HACK
		//if (spec.m_strName == "PointLights")
			//xBuilder = xBuilder.WithRaster(vk::CullModeFlagBits::eBack);

		xBuilder = xBuilder.WithRaster(vk::CullModeFlagBits::eNone);

		xBuilder.Build(xPipelineOut);
		xPipelineOut.m_axDescLayouts = xDescThings.xLayouts;
		for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			xPipelineOut.m_axDescSets[i] = xDescThings.xSets;
		}
		xPipelineOut.m_bUsePushConstants = spec.m_bUsePushConstants;
	}
