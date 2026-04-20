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
#include "DataStream/Zenith_DataStream.h"

#ifdef ZENITH_TOOLS
#include "Flux/Slang/Flux_ShaderHotReload.h"
#include <unordered_map>

// File-scope maps for hot reload - stores shader pointers and pipeline specs
// so the hot reload callback can access them
static std::unordered_map<Zenith_Vulkan_Pipeline*, Zenith_Vulkan_Shader*> s_xHotReloadShaderMap;
static std::unordered_map<Zenith_Vulkan_Pipeline*, Flux_PipelineSpecification> s_xHotReloadSpecMap;
#endif

// Zenith_Vulkan_Shader implementation lives in Zenith_Vulkan_Shader.cpp.

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
		m_xCreateInfo.setBindingCount(static_cast<uint32_t>(m_xAddedBindings.size()));

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

		vk::DescriptorSetLayout layout = VkUnwrap(device.createDescriptorSetLayout(m_xCreateInfo));
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

static void AddVertexAttributes(const Flux_BufferLayout& xLayout, uint32_t uBinding, vk::VertexInputRate eRate, 
	std::vector<vk::VertexInputBindingDescription>& xBindDescs, std::vector<vk::VertexInputAttributeDescription>& xAttrDescs, uint32_t& uBindPoint)
{
	if (xLayout.GetElements().GetSize() == 0)
		return;

	for (Zenith_Vector<Flux_BufferElement>::Iterator xIt(xLayout.GetElements()); !xIt.Done(); xIt.Next())
	{
		const Flux_BufferElement& xElement = xIt.GetData();
		xAttrDescs.push_back(vk::VertexInputAttributeDescription()
			.setBinding(uBinding)
			.setLocation(uBindPoint++)
			.setOffset(xElement.m_uOffset)
			.setFormat(Zenith_Vulkan::ShaderDataTypeToVulkanFormat(xElement.m_eType)));
	}

	xBindDescs.push_back(vk::VertexInputBindingDescription()
		.setBinding(uBinding)
		.setStride(xLayout.GetStride())
		.setInputRate(eRate));
}

static vk::PipelineVertexInputStateCreateInfo VertexDescToVulkanDesc(const Flux_VertexInputDescription& xDesc, std::vector<vk::VertexInputBindingDescription>& xBindDescs, std::vector<vk::VertexInputAttributeDescription>& xAttrDescs)
{
	uint32_t uBindPoint = 0;
	AddVertexAttributes(xDesc.m_xPerVertexLayout, 0, vk::VertexInputRate::eVertex, xBindDescs, xAttrDescs, uBindPoint);
	AddVertexAttributes(xDesc.m_xPerInstanceLayout, 1, vk::VertexInputRate::eInstance, xBindDescs, xAttrDescs, uBindPoint);

	return std::move(vk::PipelineVertexInputStateCreateInfo()
		.setVertexBindingDescriptionCount(static_cast<uint32_t>(xBindDescs.size()))
		.setPVertexBindingDescriptions(xBindDescs.data())
		.setVertexAttributeDescriptionCount(static_cast<uint32_t>(xAttrDescs.size()))
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
	const vk::Device xDevice = Zenith_Vulkan::GetDevice();

	// Destroy pipeline if valid
	if (m_xPipeline)
	{
		xDevice.destroyPipeline(m_xPipeline);
		m_xPipeline = VK_NULL_HANDLE;
	}

	// Destroy render pass if valid
	if (m_xRenderPass)
	{
		xDevice.destroyRenderPass(m_xRenderPass);
		m_xRenderPass = VK_NULL_HANDLE;
	}

	// Destroy root signature resources
	if (m_xRootSig.m_xLayout)
	{
		xDevice.destroyPipelineLayout(m_xRootSig.m_xLayout);
		m_xRootSig.m_xLayout = VK_NULL_HANDLE;
	}

	// Destroy descriptor set layouts
	for (u_int u = 0; u < m_xRootSig.m_uNumBindingGroups && u < FLUX_MAX_BINDINGS_PER_GROUP; u++)
	{
		if (m_xRootSig.m_axDescSetLayouts[u])
		{
			xDevice.destroyDescriptorSetLayout(m_xRootSig.m_axDescSetLayouts[u]);
			m_xRootSig.m_axDescSetLayouts[u] = VK_NULL_HANDLE;
		}
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

Zenith_Vulkan_PipelineBuilder& Zenith_Vulkan_PipelineBuilder::WithDescriptorSetLayout(vk::DescriptorSetLayout layout)
{
	m_xAllLayouts.push_back(layout);
	return *this;
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
		return vk::CompareOp::eAlways;
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
		return vk::BlendFactor::eZero;
	}
}

vk::RenderPass Zenith_Vulkan_Pipeline::TargetSetupToRenderPass(const TextureFormat* aeColourFormats, uint32_t uNumColourAttachments, TextureFormat eDepthStencilFormat, LoadAction eColourLoad, StoreAction eColourStore, LoadAction eDepthStencilLoad, StoreAction eDepthStencilStore, RenderTargetUsage eUsage, bool bDepthIsReadOnly)
{
	const bool bHasDepth = eDepthStencilFormat != TEXTURE_FORMAT_NONE;

	vk::ImageLayout eLayout = vk::ImageLayout::eColorAttachmentOptimal;
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
		break;
	}

	std::vector<vk::AttachmentDescription> xAttachmentDescs(uNumColourAttachments);
	std::vector<vk::AttachmentReference> xAttachmentRefs(uNumColourAttachments);
	for (uint32_t i = 0; i < uNumColourAttachments; i++)
	{
		xAttachmentDescs.at(i)
			.setFormat(Zenith_Vulkan::ConvertToVkFormat_Colour(aeColourFormats[i]))
			.setSamples(vk::SampleCountFlagBits::e1)
			.setLoadOp(Zenith_Vulkan::ConvertToVkLoadAction(eColourLoad))
			.setStoreOp(Zenith_Vulkan::ConvertToVkStoreAction(eColourStore))
			.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
			.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
			.setInitialLayout(eLayout)
			.setFinalLayout(eLayout);

		xAttachmentRefs.at(i)
			.setAttachment(i)
			.setLayout(vk::ImageLayout::eColorAttachmentOptimal);
	}

	vk::AttachmentDescription xDepthStencilAttachment;
	vk::AttachmentReference xDepthStencilAttachmentRef;
	if (bHasDepth)
	{
		// initialLayout == finalLayout == working layout. The render graph
		// (Flux_RenderGraph::SynthesizeBarriers) puts the depth image in this
		// layout BEFORE BeginRenderPass via a prologue barrier; the render
		// pass itself never transitions. This holds for both LOAD and CLEAR
		// loadOps — for CLEAR the prior state may be UNDEFINED, but the
		// graph's barrier handles that as a discard transition into the
		// working layout, not the render pass.
		const vk::ImageLayout eDepthLayout = bDepthIsReadOnly
			? vk::ImageLayout::eDepthStencilReadOnlyOptimal
			: vk::ImageLayout::eDepthStencilAttachmentOptimal;
		xDepthStencilAttachment = vk::AttachmentDescription()
			.setFormat(Zenith_Vulkan::ConvertToVkFormat_DepthStencil(eDepthStencilFormat))
			.setSamples(vk::SampleCountFlagBits::e1)
			.setLoadOp(Zenith_Vulkan::ConvertToVkLoadAction(eDepthStencilLoad))
			.setStoreOp(Zenith_Vulkan::ConvertToVkStoreAction(eDepthStencilStore))
			.setStencilLoadOp(Zenith_Vulkan::ConvertToVkLoadAction(eDepthStencilLoad))
			.setStencilStoreOp(Zenith_Vulkan::ConvertToVkStoreAction(eDepthStencilStore))
			.setInitialLayout(eDepthLayout)
			.setFinalLayout(eDepthLayout);

		xDepthStencilAttachmentRef = vk::AttachmentReference()
			.setAttachment(uNumColourAttachments)
			.setLayout(eDepthLayout);

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
		.setAttachmentCount(static_cast<uint32_t>(xAttachmentDescs.size()))
		.setPAttachments(xAttachmentDescs.data())
		.setSubpassCount(1)
		.setPSubpasses(&xSubpass)
		.setDependencyCount(0)
		.setPDependencies(nullptr);

	return VkUnwrap(Zenith_Vulkan::GetDevice().createRenderPass(xRenderPassInfo));
}

vk::Framebuffer Zenith_Vulkan_Pipeline::TargetSetupToFramebuffer(const Flux_RenderGraph_AttachmentRef* axColourAttachments, uint32_t uNumColourAttachments, const Flux_RenderGraph_AttachmentRef& xDepthStencil, uint32_t uWidth, uint32_t uHeight, const vk::RenderPass& xPass)
{
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();
	const bool bHasDepth = xDepthStencil.IsValid();
	const uint32_t uNumAttachments = bHasDepth ? uNumColourAttachments + 1 : uNumColourAttachments;

	Zenith_Assert(uNumAttachments > 0, "TargetSetupToFramebuffer: no attachments");
	Zenith_Assert(uWidth > 0 && uHeight > 0, "TargetSetupToFramebuffer: invalid dimensions %ux%u", uWidth, uHeight);
	Zenith_Assert(xPass, "TargetSetupToFramebuffer: null render pass");

	vk::ImageView axAttachments[FLUX_MAX_TARGETS];
	for (uint32_t i = 0; i < uNumColourAttachments; i++)
	{
		const Flux_RenderGraph_AttachmentRef& rxRef = axColourAttachments[i];
		Zenith_Assert(rxRef.IsValid(), "TargetSetupToFramebuffer: invalid colour attachment %u", i);
		Zenith_Assert(rxRef.m_xResource.IsImageLike(), "TargetSetupToFramebuffer: colour attachment %u is not image-like", i);

		// 2D vs Cube path: 2D images have one axis (mip), cubes have two (mip + face).
		// The asymmetry is intrinsic — a 2D RTV ignores layer because Flux_RenderAttachment
		// is single-layer by construction; a cube needs both axes to pick a single
		// face's RTV. Asserts here catch (a) misuse of a non-zero layer with a 2D
		// attachment and (b) any future Flux_GraphResourceKind that satisfies
		// IsImageLike() but isn't yet handled.
		Flux_ImageViewHandle xViewHandle;
		if (rxRef.m_xResource.GetKind() == Flux_GraphResourceKind::Image)
		{
			Zenith_Assert(rxRef.m_uLayer == 0, "TargetSetupToFramebuffer: colour attachment %u is 2D but uLayer=%u (expected 0)", i, static_cast<uint32_t>(rxRef.m_uLayer));
			xViewHandle = rxRef.m_xResource.AsImage()->RTV(rxRef.m_uMip).m_xImageViewHandle;
		}
		else
		{
			Zenith_Assert(rxRef.m_xResource.GetKind() == Flux_GraphResourceKind::ImageCube, "TargetSetupToFramebuffer: colour attachment %u has unhandled image-like kind", i);
			xViewHandle = rxRef.m_xResource.AsImageCube()->RTV(rxRef.m_uMip, rxRef.m_uLayer).m_xImageViewHandle;
		}

		axAttachments[i] = Zenith_Vulkan_MemoryManager::GetImageView(xViewHandle);
		Zenith_Assert(axAttachments[i], "TargetSetupToFramebuffer: null image view for colour attachment %u (format %u, mip %u, layer %u)", i, static_cast<uint32_t>(rxRef.m_xResource.GetSurfaceInfo().m_eFormat), static_cast<uint32_t>(rxRef.m_uMip), static_cast<uint32_t>(rxRef.m_uLayer));
	}
	if (bHasDepth)
	{
		Zenith_Assert(xDepthStencil.m_xResource.GetKind() == Flux_GraphResourceKind::Image, "TargetSetupToFramebuffer: depth attachment must be 2D image kind");
		Flux_RenderAttachment* pxDepthStencil = xDepthStencil.m_xResource.AsImage();
		axAttachments[uNumAttachments - 1] = Zenith_Vulkan_MemoryManager::GetImageView(pxDepthStencil->DSV().m_xImageViewHandle);
		Zenith_Assert(axAttachments[uNumAttachments - 1], "TargetSetupToFramebuffer: null depth image view (format %u)", static_cast<uint32_t>(pxDepthStencil->m_xSurfaceInfo.m_eFormat));
	}

	vk::FramebufferCreateInfo framebufferInfo = vk::FramebufferCreateInfo()
		.setRenderPass(xPass)
		.setAttachmentCount(uNumAttachments)
		.setPAttachments(axAttachments)
		.setWidth(uWidth)
		.setHeight(uHeight)
		.setLayers(1);

	return VkUnwrap(xDevice.createFramebuffer(framebufferInfo));
}

// Per-substruct builders extracted from FromSpecification. Each returns its
// Vk struct by value. Builders whose struct holds pointers into a caller-
// supplied array (color blend attachments, dynamic states) take those arrays
// as out-params so the arrays outlive the returned struct.

static vk::PipelineInputAssemblyStateCreateInfo BuildInputAssemblyState(const Flux_PipelineSpecification& xSpec)
{
	vk::PipelineInputAssemblyStateCreateInfo xTopology;
	switch (xSpec.m_xVertexInputDesc.m_eTopology)
	{
	case(MESH_TOPOLOGY_TRIANGLES):
		xTopology.setTopology(vk::PrimitiveTopology::eTriangleList);
		break;
	default:
		xTopology.setTopology(vk::PrimitiveTopology::eTriangleList);
		break;
	}
	return xTopology;
}

static vk::PipelineColorBlendStateCreateInfo BuildColorBlendState(const Flux_PipelineSpecification& xSpec,
	vk::PipelineColorBlendAttachmentState (&axOutBlendInfo)[FLUX_MAX_TARGETS])
{
	for (u_int u = 0; u < xSpec.m_uNumColourAttachments; u++)
	{
		axOutBlendInfo[u]
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
	vk::PipelineColorBlendStateCreateInfo xBlendInfo;
	xBlendInfo.setAttachmentCount(xSpec.m_uNumColourAttachments);
	xBlendInfo.setPAttachments(axOutBlendInfo);
	return xBlendInfo;
}

static vk::PipelineDepthStencilStateCreateInfo BuildDepthStencilState(const Flux_PipelineSpecification& xSpec)
{
	return vk::PipelineDepthStencilStateCreateInfo()
		.setDepthCompareOp(VceCompareFuncToVkCompareFunc(xSpec.m_eDepthCompareFunc))
		.setDepthTestEnable(xSpec.m_bDepthTestEnabled)
		.setDepthWriteEnable(xSpec.m_bDepthWriteEnabled)
		.setDepthBoundsTestEnable(false)
		.setStencilTestEnable(false); //#TO_TODO: stencil
}

static vk::PipelineRasterizationStateCreateInfo BuildRasterizationState(const Flux_PipelineSpecification& xSpec)
{
	// Convert Flux CullMode to Vulkan CullModeFlagBits
	vk::CullModeFlagBits eVkCullMode = vk::CullModeFlagBits::eNone;
	switch (xSpec.m_eCullMode)
	{
	case CULL_MODE_NONE:  eVkCullMode = vk::CullModeFlagBits::eNone;  break;
	case CULL_MODE_FRONT: eVkCullMode = vk::CullModeFlagBits::eFront; break;
	case CULL_MODE_BACK:  eVkCullMode = vk::CullModeFlagBits::eBack;  break;
	}

	vk::PipelineRasterizationStateCreateInfo xRasterInfo = vk::PipelineRasterizationStateCreateInfo()
		.setCullMode(eVkCullMode)
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
	return xRasterInfo;
}

static vk::PipelineViewportStateCreateInfo BuildViewportState()
{
	return vk::PipelineViewportStateCreateInfo()
		.setViewportCount(1)
		.setScissorCount(1);
}

static vk::PipelineDynamicStateCreateInfo BuildDynamicState(const Flux_PipelineSpecification& xSpec,
	vk::DynamicState (&aeOutDynamicState)[4], u_int& uOutCount)
{
	// Viewport and scissor are always dynamic. Cull mode and depth bias are
	// optionally dynamic per Flux_PipelineSpecification flags.
	aeOutDynamicState[0] = vk::DynamicState::eViewport;
	aeOutDynamicState[1] = vk::DynamicState::eScissor;
	uOutCount = 2;

	if (xSpec.m_bDynamicCullMode)
	{
		aeOutDynamicState[uOutCount++] = vk::DynamicState::eCullMode;
	}
	if (xSpec.m_bDynamicDepthBias)
	{
		aeOutDynamicState[uOutCount++] = vk::DynamicState::eDepthBias;
	}

	return vk::PipelineDynamicStateCreateInfo()
		.setDynamicStateCount(uOutCount)
		.setPDynamicStates(aeOutDynamicState);
}

static vk::PipelineMultisampleStateCreateInfo BuildMultisampleState()
{
	return vk::PipelineMultisampleStateCreateInfo()
		.setRasterizationSamples(vk::SampleCountFlagBits::e1);
}

void Zenith_Vulkan_PipelineBuilder::FromSpecification(Zenith_Vulkan_Pipeline& xPipelineOut, const Flux_PipelineSpecification& xSpec)
{
	vk::GraphicsPipelineCreateInfo xPipelineInfo;

	// Shaders
	xPipelineInfo.setStageCount(xSpec.m_pxShader->m_uStageCount);
	xPipelineInfo.setPStages(xSpec.m_pxShader->m_xInfos);

	// Vertex input + topology
	std::vector<vk::VertexInputBindingDescription> xBindDescs;
	std::vector<vk::VertexInputAttributeDescription> xAttrDescs;
	vk::PipelineVertexInputStateCreateInfo xVertexDesc = VertexDescToVulkanDesc(xSpec.m_xVertexInputDesc, xBindDescs, xAttrDescs);
	vk::PipelineInputAssemblyStateCreateInfo xTopology = BuildInputAssemblyState(xSpec);
	xPipelineInfo.setPVertexInputState(&xVertexDesc);
	xPipelineInfo.setPInputAssemblyState(&xTopology);

	// Color blend — attachment array lives here so the struct's pointer stays valid.
	vk::PipelineColorBlendAttachmentState axBlendInfo[FLUX_MAX_TARGETS];
	vk::PipelineColorBlendStateCreateInfo xBlendInfo = BuildColorBlendState(xSpec, axBlendInfo);
	xPipelineInfo.setPColorBlendState(&xBlendInfo);

	// Depth / stencil
	vk::PipelineDepthStencilStateCreateInfo xDepthStencilInfo = BuildDepthStencilState(xSpec);
	xPipelineInfo.setPDepthStencilState(&xDepthStencilInfo);

	// Render pass
	xPipelineInfo.setRenderPass(Zenith_Vulkan_Pipeline::TargetSetupToRenderPass(xSpec.m_aeColourAttachmentFormats, xSpec.m_uNumColourAttachments, xSpec.m_eDepthStencilFormat, LOAD_ACTION_DONTCARE, STORE_ACTION_DONTCARE, LOAD_ACTION_DONTCARE, STORE_ACTION_DONTCARE, RENDER_TARGET_USAGE_RENDERTARGET));

	// Rasterization
	vk::PipelineRasterizationStateCreateInfo xRasterInfo = BuildRasterizationState(xSpec);
	xPipelineInfo.setPRasterizationState(&xRasterInfo);

	// Viewport (sizes are dynamic)
	vk::PipelineViewportStateCreateInfo xViewportInfo = BuildViewportState();
	xPipelineInfo.setPViewportState(&xViewportInfo);

	// Dynamic state — backing array lives here.
	vk::DynamicState aeDynamicState[4];
	u_int uDynamicStateCount = 0;
	vk::PipelineDynamicStateCreateInfo xDynamicState = BuildDynamicState(xSpec, aeDynamicState, uDynamicStateCount);
	xPipelineInfo.setPDynamicState(&xDynamicState);

	// Multisample
	vk::PipelineMultisampleStateCreateInfo xMultisampleInfo = BuildMultisampleState();
	xPipelineInfo.setPMultisampleState(&xMultisampleInfo);

	// Pipeline layout / root signature
	Zenith_Vulkan_RootSigBuilder::FromSpecification(xPipelineOut.m_xRootSig, xSpec.m_xPipelineLayout);
	xPipelineInfo.setLayout(xPipelineOut.m_xRootSig.m_xLayout);

	xPipelineOut.m_xPipeline = VkUnwrap(Zenith_Vulkan::GetDevice().createGraphicsPipeline(VK_NULL_HANDLE, xPipelineInfo));

#ifdef ZENITH_TOOLS
	// Register pipeline for hot reload if shader has source paths
	if (!xSpec.m_pxShader->m_strVertexPath.empty() && !xSpec.m_pxShader->m_strFragmentPath.empty())
	{
		// Store shader pointer and pipeline spec in file-scope maps for hot reload callback
		s_xHotReloadShaderMap[&xPipelineOut] = const_cast<Zenith_Vulkan_Shader*>(xSpec.m_pxShader);
		s_xHotReloadSpecMap[&xPipelineOut] = xSpec;

		Flux_ShaderHotReload::RegisterPipeline(&xPipelineOut,
			xSpec.m_pxShader->m_strVertexPath, xSpec.m_pxShader->m_strFragmentPath,
			[](Zenith_Vulkan_Pipeline* pxPipeline, const std::string& strVertPath, const std::string& strFragPath) -> bool
			{
				// Get the shader and spec for this pipeline from file-scope maps
				auto itShader = s_xHotReloadShaderMap.find(pxPipeline);
				auto itSpec = s_xHotReloadSpecMap.find(pxPipeline);
				if (itShader == s_xHotReloadShaderMap.end() || itSpec == s_xHotReloadSpecMap.end())
				{
					Zenith_Error(LOG_CATEGORY_RENDERER, "Hot reload failed: Pipeline not found in maps");
					return false;
				}

				Zenith_Vulkan_Shader* pxShader = itShader->second;
				Flux_PipelineSpecification& xSpec = itSpec->second;

				// Recompile the shader from source
				if (!pxShader->InitialiseFromSource(strVertPath, strFragPath))
				{
					Zenith_Error(LOG_CATEGORY_RENDERER, "Hot reload failed: Shader compilation failed");
					return false;
				}

				// Destroy the old pipeline
				if (pxPipeline->m_xPipeline)
				{
					Zenith_Vulkan::GetDevice().destroyPipeline(pxPipeline->m_xPipeline);
					pxPipeline->m_xPipeline = nullptr;
				}

				// Destroy the old root sig resources
				if (pxPipeline->m_xRootSig.m_xLayout)
				{
					Zenith_Vulkan::GetDevice().destroyPipelineLayout(pxPipeline->m_xRootSig.m_xLayout);
					pxPipeline->m_xRootSig.m_xLayout = nullptr;
				}
				for (u_int i = 0; i < pxPipeline->m_xRootSig.m_uNumBindingGroups; i++)
				{
					if (pxPipeline->m_xRootSig.m_axDescSetLayouts[i])
					{
						Zenith_Vulkan::GetDevice().destroyDescriptorSetLayout(pxPipeline->m_xRootSig.m_axDescSetLayouts[i]);
						pxPipeline->m_xRootSig.m_axDescSetLayouts[i] = nullptr;
					}
				}

				// Recreate the pipeline with updated shader
				Zenith_Vulkan_PipelineBuilder::FromSpecification(*pxPipeline, xSpec);

				Zenith_Log(LOG_CATEGORY_RENDERER, "Hot reload succeeded for pipeline: %s + %s",
						   strVertPath.c_str(), strFragPath.c_str());
				return true;
			});
	}
#endif
}

void Zenith_Vulkan_RootSigBuilder::FromSpecification(Zenith_Vulkan_RootSig& xRootSigOut, const Flux_PipelineLayout& xSpec)
{
	xRootSigOut.m_uNumBindingGroups = xSpec.m_uNumBindingGroups;
	for (u_int uDescSet = 0; uDescSet < xSpec.m_uNumBindingGroups; uDescSet++)
	{
		const Flux_BindingGroupLayout& xLayout = xSpec.m_axBindingGroups[uDescSet];

		if (xLayout.m_axBindings[0].m_eType == BINDING_TYPE_UNBOUNDED_TEXTURES)
		{
			xRootSigOut.m_axDescSetLayouts[uDescSet] = Zenith_Vulkan::GetBindlessTexturesDescriptorSetLayout();
			continue;
		}

		vk::DescriptorSetLayoutCreateInfo xInfo = vk::DescriptorSetLayoutCreateInfo();
		vk::DescriptorSetLayoutBinding axBindings[FLUX_MAX_BINDINGS_PER_GROUP];
		u_int uNumDescriptors = 0;
		for (u_int uDesc = 0; uDesc < FLUX_MAX_BINDINGS_PER_GROUP; uDesc++)
		{
			if (xLayout.m_axBindings[uDesc].m_eType == BINDING_TYPE_MAX)
			{
				break;
			}

			// Store descriptor type for later use in command buffer
			xRootSigOut.m_axBindingTypes[uDescSet][uDesc] = xLayout.m_axBindings[uDesc].m_eType;

			vk::DescriptorSetLayoutBinding& xBinding = axBindings[uDesc]
				.setBinding(uDesc)
				.setDescriptorCount(1)
				.setStageFlags(vk::ShaderStageFlagBits::eAll);

			switch (xLayout.m_axBindings[uDesc].m_eType)
			{
			case(BINDING_TYPE_BUFFER):
				xBinding.setDescriptorType(vk::DescriptorType::eUniformBuffer);
				break;
			case(BINDING_TYPE_STORAGE_BUFFER):
				xBinding.setDescriptorType(vk::DescriptorType::eStorageBuffer);
				break;
			case(BINDING_TYPE_TEXTURE):
				xBinding.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
				break;
			case(BINDING_TYPE_STORAGE_IMAGE):
				xBinding.setDescriptorType(vk::DescriptorType::eStorageImage);
				break;
			case(BINDING_TYPE_UNBOUNDED_TEXTURES):
				Zenith_Assert(false, "Unbounded textures must be in their own table");
				break;
			case(BINDING_TYPE_ACCELERATION_STRUCTURE):
			case(BINDING_TYPE_MAX):
				break;
			}

			uNumDescriptors++;
		}
		xInfo.setBindingCount(uNumDescriptors);
		xInfo.setPBindings(axBindings);
		xInfo.setFlags(vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool);

		xRootSigOut.m_axDescSetLayouts[uDescSet] = VkUnwrap(Zenith_Vulkan::GetDevice().createDescriptorSetLayout(xInfo));
	}
	// Push constants replaced with scratch buffer system - no push constant ranges needed
	vk::PipelineLayoutCreateInfo xPipelineLayoutInfo = vk::PipelineLayoutCreateInfo()
		.setPSetLayouts(xRootSigOut.m_axDescSetLayouts)
		.setSetLayoutCount(xSpec.m_uNumBindingGroups)
		.setPushConstantRangeCount(0)
		.setPPushConstantRanges(nullptr);

	xRootSigOut.m_xLayout = VkUnwrap(Zenith_Vulkan::GetDevice().createPipelineLayout(xPipelineLayoutInfo));
}

void Zenith_Vulkan_RootSigBuilder::FromReflection(Zenith_Vulkan_RootSig& xRootSigOut, const Flux_ShaderReflection& xReflection)
{
	// Generate pipeline layout from reflection data
	Flux_PipelineLayout xLayout;
	xReflection.PopulateLayout(xLayout);

	// Use the existing FromSpecification to create the Vulkan resources
	FromSpecification(xRootSigOut, xLayout);

	// Store reflection data for runtime name-based binding lookups
	xRootSigOut.m_xReflection = xReflection;
}

// Compute pipeline builder lives in Zenith_Vulkan_ComputePipelineBuilder.cpp.
