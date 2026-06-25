/******************************************************************************
This file is part of the Newcastle Vulkan Tutorial Series

Author:Rich Davison
Contact:richgdavison@gmail.com
License: MIT (see LICENSE file at the top of the source tree)
*//////////////////////////////////////////////////////////////////////////////
#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#include "Zenith_Vulkan_Pipeline.h"

#include "Zenith_Vulkan.h"
#include "Flux/Flux.h"
#include "Flux/Flux_RenderTargets.h"

// Hot-reload state was removed when the Slang migration retired the
// .vert/.frag string-pair path. Slang hot reload is tracked as a
// follow-up — it'll need a .slang file watcher that maps edits back to
// FluxShaderProgram IDs.

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
			.setFormat(g_xEngine.FluxBackend().ShaderDataTypeToVulkanFormat(xElement.m_eType)));
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
	Reset();
}

void Zenith_Vulkan_Pipeline::Reset()
{
	const vk::Device xDevice = g_xEngine.FluxBackend().GetDevice();

	if (m_xPipeline)
	{
		xDevice.destroyPipeline(m_xPipeline);
		m_xPipeline = VK_NULL_HANDLE;
	}

	if (m_xRenderPass)
	{
		xDevice.destroyRenderPass(m_xRenderPass);
		m_xRenderPass = VK_NULL_HANDLE;
	}

	if (m_xRootSig.m_xLayout)
	{
		xDevice.destroyPipelineLayout(m_xRootSig.m_xLayout);
		m_xRootSig.m_xLayout = VK_NULL_HANDLE;
	}

	for (u_int u = 0; u < m_xRootSig.m_uNumBindingGroups && u < FLUX_MAX_BINDING_GROUPS; u++)
	{
		if (m_xRootSig.m_axDescSetLayouts[u] && m_xRootSig.m_abOwnsDescSetLayout[u])
		{
			xDevice.destroyDescriptorSetLayout(m_xRootSig.m_axDescSetLayouts[u]);
		}
		// Always clear the slot, including borrowed layouts — the next
		// FromSpecification stamps a fresh handle (and ownership flag) into
		// every slot it uses.
		m_xRootSig.m_axDescSetLayouts[u] = VK_NULL_HANDLE;
		m_xRootSig.m_abOwnsDescSetLayout[u] = false;
	}

	// Reset binding-kind matrix and group count to default-constructed state
	for (u_int i = 0; i < FLUX_MAX_BINDING_GROUPS; i++)
	{
		for (u_int j = 0; j < FLUX_MAX_BINDINGS_PER_GROUP; j++)
		{
			m_xRootSig.m_aeBindingKinds[i][j] = FLUX_RESOURCE_KIND_UNKNOWN;
		}
		m_xRootSig.m_auActiveBindingMask[i] = 0;
	}
	m_xRootSig.m_uNumBindingGroups = UINT32_MAX;
	m_xRootSig.m_xReflection = Flux_ShaderReflection();
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
	const vk::PhysicalDevice& xPhysicalDevice = g_xEngine.FluxBackend().GetPhysicalDevice();
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

	Zenith_Vulkan& xVulkan = g_xEngine.FluxBackend();

	std::vector<vk::AttachmentDescription> xAttachmentDescs(uNumColourAttachments);
	std::vector<vk::AttachmentReference> xAttachmentRefs(uNumColourAttachments);
	for (uint32_t i = 0; i < uNumColourAttachments; i++)
	{
		xAttachmentDescs.at(i)
			.setFormat(xVulkan.ConvertToVkFormat_Colour(aeColourFormats[i]))
			.setSamples(vk::SampleCountFlagBits::e1)
			.setLoadOp(xVulkan.ConvertToVkLoadAction(eColourLoad))
			.setStoreOp(xVulkan.ConvertToVkStoreAction(eColourStore))
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
			.setFormat(xVulkan.ConvertToVkFormat_DepthStencil(eDepthStencilFormat))
			.setSamples(vk::SampleCountFlagBits::e1)
			.setLoadOp(xVulkan.ConvertToVkLoadAction(eDepthStencilLoad))
			.setStoreOp(xVulkan.ConvertToVkStoreAction(eDepthStencilStore))
			.setStencilLoadOp(xVulkan.ConvertToVkLoadAction(eDepthStencilLoad))
			.setStencilStoreOp(xVulkan.ConvertToVkStoreAction(eDepthStencilStore))
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

	return VkUnwrap(xVulkan.GetDevice().createRenderPass(xRenderPassInfo));
}

vk::Framebuffer Zenith_Vulkan_Pipeline::TargetSetupToFramebuffer(const Flux_RenderGraph_AttachmentRef* axColourAttachments, uint32_t uNumColourAttachments, const Flux_RenderGraph_AttachmentRef& xDepthStencil, uint32_t uWidth, uint32_t uHeight, const vk::RenderPass& xPass)
{
	const vk::Device& xDevice = g_xEngine.FluxBackend().GetDevice();
	auto& xVulkanMemory = g_xEngine.FluxMemory();
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

		axAttachments[i] = xVulkanMemory.GetImageView(xViewHandle);
		Zenith_Assert(axAttachments[i], "TargetSetupToFramebuffer: null image view for colour attachment %u (format %u, mip %u, layer %u)", i, static_cast<uint32_t>(rxRef.m_xResource.GetSurfaceInfo().m_eFormat), static_cast<uint32_t>(rxRef.m_uMip), static_cast<uint32_t>(rxRef.m_uLayer));
	}
	if (bHasDepth)
	{
		Zenith_Assert(xDepthStencil.m_xResource.GetKind() == Flux_GraphResourceKind::Image, "TargetSetupToFramebuffer: depth attachment must be 2D image kind");
		Flux_RenderAttachment* pxDepthStencil = xDepthStencil.m_xResource.AsImage();
		axAttachments[uNumAttachments - 1] = xVulkanMemory.GetImageView(pxDepthStencil->DSV().m_xImageViewHandle);
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
			.setColorWriteMask(vk::ColorComponentFlags(xSpec.m_axBlendStates[u].m_uColorWriteMask))
			.setBlendEnable(xSpec.m_axBlendStates[u].m_bBlendEnabled)
			.setAlphaBlendOp(vk::BlendOp::eAdd)
			.setColorBlendOp(vk::BlendOp::eAdd)
			.setSrcAlphaBlendFactor(FluxBlendFactorToVK(xSpec.m_axBlendStates[u].m_eSrcAlphaBlendFactor))
			.setSrcColorBlendFactor(FluxBlendFactorToVK(xSpec.m_axBlendStates[u].m_eSrcBlendFactor))
			.setDstAlphaBlendFactor(FluxBlendFactorToVK(xSpec.m_axBlendStates[u].m_eDstAlphaBlendFactor))
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

	// Dynamic cull mode removed: cull is baked statically into the pipeline via
	// m_eCullMode (vk::DynamicState::eCullMode / vkCmdSetCullMode is Vulkan 1.3 and
	// unavailable on the Android NDK loader, and the engine never set it per-draw).
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
	// Idempotent: tear down any existing GPU handles in xPipelineOut so the
	// hot-reload path (which re-runs subsystem build code on .slang change)
	// doesn't leak the previous vk::Pipeline / RenderPass / RootSig.
	xPipelineOut.Reset();

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

	xPipelineOut.m_xPipeline = VkUnwrap(g_xEngine.FluxBackend().GetDevice().createGraphicsPipeline(VK_NULL_HANDLE, xPipelineInfo));

	// Hot-reload registration is centralised, not pipeline-level: the watcher in
	// Flux_ShaderHotReload fires each owning feature's BuildPipelines() on .slang
	// edits, and Flux_ShaderHotReload::AutoRegisterFeatures derives the
	// program->feature wiring from the feature registry at boot. Pipeline-level
	// registration here would need to know which programs the pipeline was built
	// from (which the spec doesn't carry), and the per-subsystem RegisterSubsystem
	// boilerplate it replaced is gone.
}

// Map the canonical resource kind to the Vulkan descriptor type. The texture
// kinds collapse onto eCombinedImageSampler and both structured-buffer variants
// onto eStorageBuffer, matching the legacy BindingType buckets (so the produced
// layout is behaviourally identical to the pre-kind path).
static vk::DescriptorType FluxKindToVkDescriptorType(FluxResourceKind eKind)
{
	if (FluxKindIsUniformBuffer(eKind))  return vk::DescriptorType::eUniformBuffer;
	if (FluxKindIsStorageBuffer(eKind))  return vk::DescriptorType::eStorageBuffer;
	if (FluxKindIsSampledTexture(eKind)) return vk::DescriptorType::eCombinedImageSampler;
	if (FluxKindIsStorageImage(eKind))   return vk::DescriptorType::eStorageImage;
	// UNKNOWN / parameter-block / acceleration-structure should never reach a
	// real descriptor slot (filtered by m_bPresent + the AS reject below).
	return vk::DescriptorType::eUniformBuffer;
}

void Zenith_Vulkan_RootSigBuilder::FromSpecification(Zenith_Vulkan_RootSig& xRootSigOut, const Flux_PipelineLayout& xSpec)
{
	Zenith_Vulkan& xVulkan = g_xEngine.FluxBackend();

	// Clear stale binding metadata — hot-reload reuses static Flux_RootSig
	// objects, so the previous build's kinds survive unless we wipe them here
	// before re-populating from the new spec. (Reset on the owning
	// Flux_Pipeline is best-effort; some compute paths build a root sig as a
	// separate object.)
	for (u_int i = 0; i < FLUX_MAX_BINDING_GROUPS; i++)
	{
		for (u_int j = 0; j < FLUX_MAX_BINDINGS_PER_GROUP; j++)
		{
			xRootSigOut.m_aeBindingKinds[i][j] = FLUX_RESOURCE_KIND_UNKNOWN;
		}
		xRootSigOut.m_auActiveBindingMask[i] = 0;
	}

	xRootSigOut.m_uNumBindingGroups = xSpec.m_uNumBindingGroups;
	for (u_int uDescSet = 0; uDescSet < xSpec.m_uNumBindingGroups; uDescSet++)
	{
		const Flux_BindingGroupLayout& xLayout = xSpec.m_axBindingGroups[uDescSet];

		if (FluxKindIsUnboundedArray(xLayout.m_axBindings[0].m_eKind))
		{
			// Borrowed handle — Pipeline::Reset must skip its destruction.
			xRootSigOut.m_axDescSetLayouts[uDescSet]    = xVulkan.GetBindlessTexturesDescriptorSetLayout();
			xRootSigOut.m_abOwnsDescSetLayout[uDescSet] = false;
			continue;
		}

		vk::DescriptorSetLayoutCreateInfo xInfo = vk::DescriptorSetLayoutCreateInfo();
		vk::DescriptorSetLayoutBinding axBindings[FLUX_MAX_BINDINGS_PER_GROUP];
		u_int uNumDescriptors = 0;
		u_int uActiveMask     = 0;
		// Iterate every slot and emit a descriptor for each PRESENT binding (no
		// stop-at-first-gap) so sparse layouts are handled correctly. The vk
		// binding array is packed (indexed by uNumDescriptors) while setBinding
		// carries the real slot index.
		for (u_int uDesc = 0; uDesc < FLUX_MAX_BINDINGS_PER_GROUP; uDesc++)
		{
			const Flux_BindingGroupEntry& xEntry = xLayout.m_axBindings[uDesc];
			if (!xEntry.m_bPresent)
			{
				continue;
			}
			Zenith_Assert(xEntry.m_eKind != FLUX_RESOURCE_KIND_ACCELERATION_STRUCTURE,
				"Acceleration-structure bindings are not supported (reserved for the future TLAS set)");

			// Mirror the kind for the descriptor-write path + record the active bit.
			xRootSigOut.m_aeBindingKinds[uDescSet][uDesc] = xEntry.m_eKind;
			uActiveMask |= (1u << uDesc);

			// Non-bindless slots are count 1 (or a fixed array size); an
			// unbounded (count 0) slot here would be a mis-declared bindless
			// table — clamp to 1 defensively.
			const u_int uCount = (xEntry.m_uDescriptorCount == 0) ? 1u : xEntry.m_uDescriptorCount;

			axBindings[uNumDescriptors] = vk::DescriptorSetLayoutBinding()
				.setBinding(uDesc)
				.setDescriptorCount(uCount)
				.setStageFlags(vk::ShaderStageFlagBits::eAll)
				.setDescriptorType(FluxKindToVkDescriptorType(xEntry.m_eKind));
			uNumDescriptors++;
		}
		xRootSigOut.m_auActiveBindingMask[uDescSet] = uActiveMask;
		xInfo.setBindingCount(uNumDescriptors);
		xInfo.setPBindings(axBindings);
		xInfo.setFlags(vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool);

		xRootSigOut.m_axDescSetLayouts[uDescSet]    = VkUnwrap(xVulkan.GetDevice().createDescriptorSetLayout(xInfo));
		xRootSigOut.m_abOwnsDescSetLayout[uDescSet] = true;
	}
	// Push constants replaced with scratch buffer system - no push constant ranges needed
	vk::PipelineLayoutCreateInfo xPipelineLayoutInfo = vk::PipelineLayoutCreateInfo()
		.setPSetLayouts(xRootSigOut.m_axDescSetLayouts)
		.setSetLayoutCount(xSpec.m_uNumBindingGroups)
		.setPushConstantRangeCount(0)
		.setPPushConstantRanges(nullptr);

	xRootSigOut.m_xLayout = VkUnwrap(xVulkan.GetDevice().createPipelineLayout(xPipelineLayoutInfo));
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
