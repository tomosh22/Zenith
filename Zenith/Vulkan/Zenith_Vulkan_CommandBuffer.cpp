#include "Zenith.h"

#include "Zenith_Vulkan_CommandBuffer.h"

#include "Zenith_Vulkan.h"
#include "AssetHandling/Zenith_AssetHandler.h"
#include "Flux/Flux.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"

//#TO purely for the static assert in SetIndexBuffer
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"

void Zenith_Vulkan_CommandBuffer::Initialise(CommandType eType /*= COMMANDTYPE_GRAPHICS*/)
{
	m_eCommandType = eType;
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();
	vk::CommandBufferAllocateInfo xAllocInfo{};
	xAllocInfo.commandPool = Zenith_Vulkan::GetCommandPool(eType);
	xAllocInfo.level = vk::CommandBufferLevel::ePrimary;
	xAllocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
	m_xCmdBuffers = xDevice.allocateCommandBuffers(xAllocInfo);
}

void Zenith_Vulkan_CommandBuffer::BeginRecording()
{
	m_xCurrentCmdBuffer = m_xCmdBuffers[Zenith_Vulkan_Swapchain::GetCurrentFrameIndex()];
	vk::CommandBufferBeginInfo xBeginInfo;
	xBeginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
	m_xCurrentCmdBuffer.begin(xBeginInfo);
	m_uCurrentBindFreq = FLUX_MAX_DESCRIPTOR_SET_LAYOUTS;
	m_uDescriptorDirty = ~0u;
	
	// Clear descriptor set cache for this frame (descriptor pool gets reset per frame anyway)
	for (u_int i = 0; i < FLUX_MAX_DESCRIPTOR_SET_LAYOUTS; i++)
	{
		m_axDescriptorSetCache[i].descriptorSet = VK_NULL_HANDLE;
		m_axDescriptorSetCache[i].layout = VK_NULL_HANDLE;
	}
}

void Zenith_Vulkan_CommandBuffer::EndRenderPass()
{
	m_xCurrentCmdBuffer.endRenderPass();
	m_xCurrentRenderPass = VK_NULL_HANDLE;
}
void Zenith_Vulkan_CommandBuffer::EndRecording(RenderOrder eOrder, bool bEndPass /*= true*/)
{
	if (bEndPass)
	{
		m_xCurrentCmdBuffer.endRenderPass();
		m_xCurrentRenderPass = VK_NULL_HANDLE;
	}
	m_xCurrentCmdBuffer.end();
	m_uCurrentBindFreq = FLUX_MAX_DESCRIPTOR_SET_LAYOUTS;
}

void Zenith_Vulkan_CommandBuffer::EndAndCpuWait(bool bEndPass)
{
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();

	if (bEndPass)
	{
		m_xCurrentCmdBuffer.endRenderPass();
		m_xCurrentRenderPass = VK_NULL_HANDLE;
	}
	m_xCurrentCmdBuffer.end();

	vk::PipelineStageFlags eWaitStages = vk::PipelineStageFlagBits::eTransfer;

	vk::SubmitInfo xSubmitInfo = vk::SubmitInfo()
		.setCommandBufferCount(1)
		.setPCommandBuffers(&m_xCurrentCmdBuffer)
		.setWaitSemaphoreCount(0)
		.setSignalSemaphoreCount(0);

	vk::FenceCreateInfo fenceInfo;
	vk::Fence xFence = xDevice.createFence(fenceInfo);

	Zenith_Vulkan::GetQueue(m_eCommandType).submit(xSubmitInfo, xFence);

	xDevice.waitForFences(1, &xFence, VK_TRUE, UINT64_MAX);
	xDevice.destroyFence(xFence);
}

template<typename T>
void BindVertexBufferImpl(vk::CommandBuffer& cmdBuffer, const T& xVertexBuffer, uint32_t uBindPoint)
{
	const vk::Buffer xBuffer = Zenith_Vulkan::GetVRAM(xVertexBuffer.GetBuffer().m_xVRAMHandle)->GetBuffer();
	vk::DeviceSize offset = 0;
	cmdBuffer.bindVertexBuffers(uBindPoint, 1, &xBuffer, &offset);
}

void Zenith_Vulkan_CommandBuffer::SetVertexBuffer(const Flux_VertexBuffer& xVertexBuffer, uint32_t uBindPoint /*= 0*/)
{
	BindVertexBufferImpl(m_xCurrentCmdBuffer, xVertexBuffer, uBindPoint);
}

void Zenith_Vulkan_CommandBuffer::SetVertexBuffer(const Flux_DynamicVertexBuffer& xVertexBuffer, uint32_t uBindPoint /*= 0*/)
{
	BindVertexBufferImpl(m_xCurrentCmdBuffer, xVertexBuffer, uBindPoint);
}

void Zenith_Vulkan_CommandBuffer::SetIndexBuffer(const Flux_IndexBuffer& xIndexBuffer)
{
	const vk::Buffer xBuffer = Zenith_Vulkan::GetVRAM(xIndexBuffer.GetBuffer().m_xVRAMHandle)->GetBuffer();
	static_assert(std::is_same<Flux_MeshGeometry::IndexType, uint32_t>(), "#TO_TODO: stop hardcoding type");
	m_xCurrentCmdBuffer.bindIndexBuffer(xBuffer, 0, vk::IndexType::eUint32);
}

void Zenith_Vulkan_CommandBuffer::TransitionUAVs(vk::ImageLayout eOldLayout, vk::ImageLayout eNewLayout, vk::AccessFlags eSrcAccessFlags, vk::AccessFlags eDstAccessFlags, vk::PipelineStageFlags eSrcStages, vk::PipelineStageFlags eDstStages)
{
	vk::ImageMemoryBarrier axBarriers[MAX_BINDINGS];
	u_int uNumBarriers = 0;

	for (u_int uDescSet = 0; uDescSet < m_pxCurrentPipeline->m_xRootSig.m_uNumDescriptorSets; uDescSet++)
	{
		for (u_int i = 0; i < MAX_BINDINGS; i++)
		{
			const Flux_UnorderedAccessView* pxUAV = m_xBindings[uDescSet].m_xUAVs[i];
			if (!pxUAV || !pxUAV->m_xImageView) continue;

			Zenith_Vulkan_VRAM* pxVRAM = Zenith_Vulkan::GetVRAM(pxUAV->m_xVRAMHandle);
			Zenith_Assert(pxVRAM, "Invalid VRAM for UAV");

			axBarriers[uNumBarriers++] = vk::ImageMemoryBarrier()
				.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1))
				.setImage(pxVRAM->GetImage())
				.setOldLayout(eOldLayout)
				.setNewLayout(eNewLayout)
				.setSrcAccessMask(eSrcAccessFlags)
				.setDstAccessMask(eDstAccessFlags);
		}
	}

	m_xCurrentCmdBuffer.pipelineBarrier
	(
		eSrcStages,
		eDstStages,
		vk::DependencyFlags(),
		0, nullptr,
		0, nullptr,
		uNumBarriers, axBarriers
	);
}

void Zenith_Vulkan_CommandBuffer::UpdateDescriptorSets()
{
	Zenith_Profiling::BeginProfile(ZENITH_PROFILE_INDEX__VULKAN_UPDATE_DESCRIPTOR_SETS);
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();
	
	for (u_int uDescSet = 0; uDescSet < m_pxCurrentPipeline->m_xRootSig.m_uNumDescriptorSets; uDescSet++)
	{
		if (Zenith_Vulkan::ShouldOnlyUpdateDirtyDescriptors() && !(m_uDescriptorDirty & (1 << uDescSet))) continue;

		vk::DescriptorSetLayout& xLayout = m_pxCurrentPipeline->m_xRootSig.m_axDescSetLayouts[uDescSet];
		
		DescriptorSetCacheEntry& xCacheEntry = m_axDescriptorSetCache[uDescSet];
		if (Zenith_Vulkan::ShouldUseDescSetCache() &&
			xCacheEntry.descriptorSet != VK_NULL_HANDLE &&
			xCacheEntry.layout == xLayout &&
			xCacheEntry.bindings == m_xBindings[uDescSet])
		{
			m_axCurrentDescSet[uDescSet] = xCacheEntry.descriptorSet;
		}
		else
		{
			vk::DescriptorSetAllocateInfo xInfo = vk::DescriptorSetAllocateInfo()
				.setDescriptorPool(Zenith_Vulkan::GetCurrentPerFrameDescriptorPool())
				.setDescriptorSetCount(1)
				.setPSetLayouts(&xLayout);
			m_axCurrentDescSet[uDescSet] = xDevice.allocateDescriptorSets(xInfo)[0];

			// Stack-allocated arrays for building descriptor writes
			u_int uNumBufferWrites = 0;
			vk::DescriptorBufferInfo axBufferInfos[MAX_BINDINGS];

			u_int uNumTexWrites = 0;
			vk::DescriptorImageInfo axTexInfos[MAX_BINDINGS * 2]; //SRVs and UAVs

			u_int uNumWrites = 0;
			vk::WriteDescriptorSet axWrites[MAX_BINDINGS * 3]; //SRVs, UAVs and CBVs

			for (u_int u = 0; u < MAX_BINDINGS; u++)
			{
				const DescriptorType eType = m_pxCurrentPipeline->m_xRootSig.m_axDescriptorTypes[uDescSet][u];
				if (eType == DESCRIPTOR_TYPE_MAX) continue;

				const Flux_ShaderResourceView* const pxSRV = m_xBindings[uDescSet].m_xSRVs[u];
				if (pxSRV)
				{
					Zenith_Vulkan_Sampler* pxSampler = m_xBindings[uDescSet].m_apxSamplers[u];
					axTexInfos[uNumTexWrites] = vk::DescriptorImageInfo()
						.setSampler(pxSampler ? pxSampler->GetSampler() : Flux_Graphics::s_xRepeatSampler.GetSampler())
						.setImageView(pxSRV->m_xImageView)
						.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
					
					axWrites[uNumWrites++] = vk::WriteDescriptorSet()
						.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
						.setDstSet(m_axCurrentDescSet[uDescSet])
						.setDstBinding(u)
						.setDstArrayElement(0)
						.setDescriptorCount(1)
						.setPImageInfo(axTexInfos + uNumTexWrites++);
				}

				const Flux_UnorderedAccessView* const pxUAV = m_xBindings[uDescSet].m_xUAVs[u];
				if (pxUAV)
				{
					axTexInfos[uNumTexWrites] = vk::DescriptorImageInfo()
						.setImageView(pxUAV->m_xImageView)
						.setImageLayout(vk::ImageLayout::eGeneral);

					axWrites[uNumWrites++] = vk::WriteDescriptorSet()
						.setDescriptorType(vk::DescriptorType::eStorageImage)
						.setDstSet(m_axCurrentDescSet[uDescSet])
						.setDstBinding(u)
						.setDstArrayElement(0)
						.setDescriptorCount(1)
						.setPImageInfo(axTexInfos + uNumTexWrites++);
				}

				const Flux_ConstantBufferView* const pxCBV = m_xBindings[uDescSet].m_xCBVs[u];
				if (pxCBV)
				{
					axBufferInfos[uNumBufferWrites] = pxCBV->m_xBufferInfo;
					axWrites[uNumWrites++] = vk::WriteDescriptorSet()
						.setDescriptorType(vk::DescriptorType::eUniformBuffer)
						.setDstSet(m_axCurrentDescSet[uDescSet])
						.setDstBinding(u)
						.setDstArrayElement(0)
						.setDescriptorCount(1)
						.setPBufferInfo(axBufferInfos + uNumBufferWrites++);
				}
			}

			if (uNumWrites > 0)
			{
				xDevice.updateDescriptorSets(uNumWrites, axWrites, 0, nullptr);
			}
			
			xCacheEntry.layout = xLayout;
			xCacheEntry.bindings = m_xBindings[uDescSet];
			xCacheEntry.descriptorSet = m_axCurrentDescSet[uDescSet];
		}

		m_xCurrentCmdBuffer.bindDescriptorSets(m_eCurrentBindPoint, m_pxCurrentPipeline->m_xRootSig.m_xLayout, uDescSet, 1, &m_axCurrentDescSet[uDescSet], 0, nullptr);
		m_uDescriptorDirty &= ~(1 << uDescSet);
	}
	Zenith_Profiling::EndProfile(ZENITH_PROFILE_INDEX__VULKAN_UPDATE_DESCRIPTOR_SETS);
}

void Zenith_Vulkan_CommandBuffer::Draw(uint32_t uNumVerts)
{
	if (Zenith_Vulkan::ShouldSubmitDrawCalls())
	{
		UpdateDescriptorSets();
		m_xCurrentCmdBuffer.draw(uNumVerts, 0, 0, 0);
	}
}

void Zenith_Vulkan_CommandBuffer::DrawIndexed(uint32_t uNumIndices, uint32_t uNumInstances /*= 1*/, uint32_t uVertexOffset /*= 0*/, uint32_t uIndexOffset /*= 0*/, uint32_t uInstanceOffset /*= 0*/)
{
	if (Zenith_Vulkan::ShouldSubmitDrawCalls())
	{
		UpdateDescriptorSets();
		m_xCurrentCmdBuffer.drawIndexed(uNumIndices, uNumInstances, uIndexOffset, uVertexOffset, uInstanceOffset);
	}
}

void Zenith_Vulkan_CommandBuffer::BeginRenderPass(Flux_TargetSetup& xTargetSetup, bool bClearColour /*= false*/, bool bClearDepth /*= false*/, bool bClearStencil /*= false*/)
{
	LoadAction eColourLoad = bClearColour ? LOAD_ACTION_CLEAR : LOAD_ACTION_LOAD;
	LoadAction eDepthStencilLoad = bClearDepth ? LOAD_ACTION_CLEAR : LOAD_ACTION_LOAD;

	uint32_t uNumColourAttachments = 0;
	for (uint32_t i = 0; i < FLUX_MAX_TARGETS; i++)
	{
		if (xTargetSetup.m_axColourAttachments[i].m_xSurfaceInfo.m_eFormat == TEXTURE_FORMAT_NONE)
			break;
		uNumColourAttachments++;
	}

	m_xCurrentRenderPass = Zenith_Vulkan_Pipeline::TargetSetupToRenderPass(xTargetSetup, eColourLoad, STORE_ACTION_STORE, eDepthStencilLoad, STORE_ACTION_STORE, RENDER_TARGET_USAGE_RENDERTARGET);

	uint32_t uWidth, uHeight;
	if (xTargetSetup.m_axColourAttachments[0].m_xSurfaceInfo.m_eFormat != TEXTURE_FORMAT_NONE)
	{
		uWidth = xTargetSetup.m_axColourAttachments[0].m_xSurfaceInfo.m_uWidth;
		uHeight = xTargetSetup.m_axColourAttachments[0].m_xSurfaceInfo.m_uHeight;
	}
	else
	{
		Zenith_Assert(xTargetSetup.m_pxDepthStencil->m_xSurfaceInfo.m_eFormat != TEXTURE_FORMAT_NONE, "Target setup has no attachments");
		uWidth = xTargetSetup.m_pxDepthStencil->m_xSurfaceInfo.m_uWidth;
		uHeight = xTargetSetup.m_pxDepthStencil->m_xSurfaceInfo.m_uHeight;
	}

	vk::RenderPassBeginInfo xRenderPassInfo = vk::RenderPassBeginInfo()
		.setRenderPass(m_xCurrentRenderPass)
		.setFramebuffer(Zenith_Vulkan_Pipeline::TargetSetupToFramebuffer(xTargetSetup, uWidth, uHeight, m_xCurrentRenderPass))
		.setRenderArea({ {0,0}, {uWidth, uHeight} });

	vk::ClearValue axClearColour[FLUX_MAX_TARGETS+1];
	if (eColourLoad == LOAD_ACTION_CLEAR || eDepthStencilLoad == LOAD_ACTION_CLEAR)
	{
		bool bHasDepth = xTargetSetup.m_pxDepthStencil != nullptr;
		const uint32_t uNumAttachments = bHasDepth && eDepthStencilLoad == LOAD_ACTION_CLEAR ? uNumColourAttachments + 1 : uNumColourAttachments;
		for (uint32_t i = 0; i < uNumColourAttachments; i++)
			axClearColour[i].color = vk::ClearColorValue(0.f, 0.f, 0.f, 0.f);
		axClearColour[uNumColourAttachments].depthStencil = vk::ClearDepthStencilValue(1, 0);
		xRenderPassInfo.clearValueCount = uNumAttachments;
		xRenderPassInfo.pClearValues = axClearColour;
	}

	m_xCurrentCmdBuffer.beginRenderPass(xRenderPassInfo, vk::SubpassContents::eInline);

	m_xViewport = vk::Viewport(0, 0, uWidth, uHeight, 0, 1);
	m_xScissor = vk::Rect2D({0, 0}, {uWidth, uHeight});
	m_xCurrentCmdBuffer.setViewport(0, 1, &m_xViewport);
	m_xCurrentCmdBuffer.setScissor(0, 1, &m_xScissor);
}

void Zenith_Vulkan_CommandBuffer::SetPipeline(Zenith_Vulkan_Pipeline* pxPipeline)
{
	m_eCurrentBindPoint = vk::PipelineBindPoint::eGraphics;
	m_xCurrentCmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pxPipeline->m_xPipeline);
	m_pxCurrentPipeline = pxPipeline;
	m_uDescriptorDirty = ~0u;
	memset(m_xBindings, 0, sizeof(m_xBindings));

}

void Zenith_Vulkan_CommandBuffer::BindSRV(const Flux_ShaderResourceView* pxSRV, uint32_t uBindPoint, Zenith_Vulkan_Sampler* pxSampler /*= nullptr*/)
{
	Zenith_Assert(m_uCurrentBindFreq < FLUX_MAX_DESCRIPTOR_SET_LAYOUTS, "Haven't called BeginBind");
	Zenith_Assert(pxSRV && pxSRV->m_xImageView, "Invalid SRV");
	m_uDescriptorDirty |= 1 << m_uCurrentBindFreq;
	m_xBindings[m_uCurrentBindFreq].m_xSRVs[uBindPoint] = pxSRV;
	m_xBindings[m_uCurrentBindFreq].m_apxSamplers[uBindPoint] = pxSampler;
}

void Zenith_Vulkan_CommandBuffer::BindUAV(const Flux_UnorderedAccessView* pxUAV, uint32_t uBindPoint)
{
	Zenith_Assert(m_uCurrentBindFreq < FLUX_MAX_DESCRIPTOR_SET_LAYOUTS, "Haven't called BeginBind");
	Zenith_Assert(pxUAV && pxUAV->m_xImageView, "Invalid UAV");
	m_uDescriptorDirty |= 1 << m_uCurrentBindFreq;
	m_xBindings[m_uCurrentBindFreq].m_xUAVs[uBindPoint] = pxUAV;
	m_xBindings[m_uCurrentBindFreq].m_apxSamplers[uBindPoint] = nullptr;
}

void Zenith_Vulkan_CommandBuffer::BindCBV(const Flux_ConstantBufferView* pxCBV, uint32_t uBindPoint)
{
	Zenith_Assert(m_uCurrentBindFreq < FLUX_MAX_DESCRIPTOR_SET_LAYOUTS, "Haven't called BeginBind");
	Zenith_Assert(pxCBV, "Invalid CBV");
	m_uDescriptorDirty |= 1 << m_uCurrentBindFreq;
	m_xBindings[m_uCurrentBindFreq].m_xCBVs[uBindPoint] = pxCBV;
}

void Zenith_Vulkan_CommandBuffer::BindAccelerationStruct(void* pxStruct, uint32_t uBindPoint) {
	STUBBED
		/*
		Zenith_Assert(m_eCurrentBindFreq < BINDING_FREQUENCY_MAX, "Haven't called BeginBind");

		if (m_eCurrentBindFreq == BINDING_FREQUENCY_PER_FRAME)
		{
			vk::AccelerationStructureKHR& xTlas = *reinterpret_cast<vk::AccelerationStructureKHR*>(pxStruct);

			vk::WriteDescriptorSetAccelerationStructureKHR xInfo = vk::WriteDescriptorSetAccelerationStructureKHR()
				.setAccelerationStructureCount(1)
				.setAccelerationStructures(xTlas);

			vk::WriteDescriptorSet xWrite = vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eAccelerationStructureKHR)
				//#TO index 0 for per frame set
				.setDstSet(m_pxCurrentPipeline->m_axDescSets[m_pxRenderer->m_currentFrame][0])
				.setDstBinding(uBindPoint)
				.setDescriptorCount(1)
				.setPNext(&xInfo);

			m_pxRenderer->GetDevice().updateDescriptorSets(1, &xWrite, 0, nullptr);
		}
	#if 0
		else if (m_eCurrentBindFreq == RendererAPI::BINDING_FREQUENCY_PER_DRAW)
			m_xBindings[m_eCurrentBindFreq].m_xBuffers[uBindPoint] = reinterpret_cast<Buffer*>(pxBuffer);
	#endif
		*/
}

void Zenith_Vulkan_CommandBuffer::PushConstant(void* pData, size_t uSize)
{
	m_xCurrentCmdBuffer.pushConstants(m_pxCurrentPipeline->m_xRootSig.m_xLayout, vk::ShaderStageFlagBits::eAll, 0, uSize, pData);
}

void Zenith_Vulkan_CommandBuffer::SetShoudClear(const bool bClear)
{
	m_bShouldClear = bClear;
}

void Zenith_Vulkan_CommandBuffer::UseBindlessTextures(const uint32_t uSet)
{
	m_xCurrentCmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pxCurrentPipeline->m_xRootSig.m_xLayout, uSet, 1, &Zenith_Vulkan::GetBindlessTexturesDescriptorSet(), 0, nullptr);
	m_uDescriptorDirty &= ~(1 << uSet);
}

void Zenith_Vulkan_CommandBuffer::BeginBind(u_int uDescSet)
{
	memset(&m_xBindings[uDescSet], 0, sizeof(DescSetBindings));
	m_uCurrentBindFreq = uDescSet;
}

static vk::ImageMemoryBarrier CreateImageBarrier(vk::Image xImage, vk::ImageLayout eOldLayout, vk::ImageLayout eNewLayout, vk::ImageAspectFlags eAspect, uint32_t uMipLevel, uint32_t uLayer)
{
	vk::ImageMemoryBarrier xMemoryBarrier = vk::ImageMemoryBarrier()
		.setSubresourceRange(vk::ImageSubresourceRange(eAspect, uMipLevel, 1, uLayer, 1))
		.setImage(xImage)
		.setOldLayout(eOldLayout)
		.setNewLayout(eNewLayout);

	switch (eNewLayout)
	{
	case vk::ImageLayout::eTransferDstOptimal:
		xMemoryBarrier.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
		break;
	case vk::ImageLayout::eTransferSrcOptimal:
		xMemoryBarrier.setDstAccessMask(vk::AccessFlagBits::eTransferRead);
		break;
	case vk::ImageLayout::eColorAttachmentOptimal:
		xMemoryBarrier.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
		break;
	case vk::ImageLayout::eDepthAttachmentOptimal:
	case vk::ImageLayout::eDepthStencilAttachmentOptimal:
		xMemoryBarrier.setDstAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentWrite);
		break;
	case vk::ImageLayout::eShaderReadOnlyOptimal:
		xMemoryBarrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
		break;
	case vk::ImageLayout::ePresentSrcKHR:
	case vk::ImageLayout::eDepthStencilReadOnlyOptimal:
		//#TO_TODO: do we need an access mask for these?
		xMemoryBarrier.setDstAccessMask(vk::AccessFlagBits::eNone);
		break;
	default:
		Zenith_Assert(false, "unknown layout");
		break;
	}

	return xMemoryBarrier;
}

void Zenith_Vulkan_CommandBuffer::ImageTransitionBarrier(vk::Image xImage, vk::ImageLayout eOldLayout, vk::ImageLayout eNewLayout, vk::ImageAspectFlags eAspect, vk::PipelineStageFlags eSrcStage, vk::PipelineStageFlags eDstStage, int uMipLevel, int uLayer)
{
	vk::ImageMemoryBarrier xMemoryBarrier = CreateImageBarrier(xImage, eOldLayout, eNewLayout, eAspect, uMipLevel, uLayer);
	m_xCurrentCmdBuffer.pipelineBarrier(eSrcStage, eDstStage, vk::DependencyFlags(), 0, nullptr, 0, nullptr, 1, &xMemoryBarrier);
}

void Zenith_Vulkan_CommandBuffer::BindComputePipeline(Zenith_Vulkan_Pipeline* pxPipeline)
{
	m_eCurrentBindPoint = vk::PipelineBindPoint::eCompute;
	m_pxCurrentPipeline = pxPipeline;
	m_xCurrentCmdBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, pxPipeline->m_xPipeline);
	m_uDescriptorDirty = ~0u;
}


void Zenith_Vulkan_CommandBuffer::Dispatch(uint32_t uGroupCountX, uint32_t uGroupCountY, uint32_t uGroupCountZ)
{
	UpdateDescriptorSets();

	TransitionUAVs(vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eGeneral,
		vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eShaderRead,
		vk::PipelineStageFlagBits::eFragmentShader | vk::PipelineStageFlagBits::eComputeShader,
		vk::PipelineStageFlagBits::eComputeShader);

	m_xCurrentCmdBuffer.dispatch(uGroupCountX, uGroupCountY, uGroupCountZ);

	TransitionUAVs(vk::ImageLayout::eGeneral, vk::ImageLayout::eShaderReadOnlyOptimal,
		vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderRead,
		vk::PipelineStageFlagBits::eComputeShader,
		vk::PipelineStageFlagBits::eFragmentShader | vk::PipelineStageFlagBits::eComputeShader);
}

void Zenith_Vulkan_CommandBuffer::ImageBarrier(Flux_Texture* pxTexture, uint32_t uOldLayout, uint32_t uNewLayout)
{
	vk::ImageLayout eOldLayout = static_cast<vk::ImageLayout>(uOldLayout);
	vk::ImageLayout eNewLayout = static_cast<vk::ImageLayout>(uNewLayout);
	
	vk::PipelineStageFlags eSrcStage = vk::PipelineStageFlagBits::eComputeShader;
	vk::PipelineStageFlags eDstStage = vk::PipelineStageFlagBits::eFragmentShader;
	vk::AccessFlags eSrcAccess = vk::AccessFlagBits::eShaderWrite;
	vk::AccessFlags eDstAccess = vk::AccessFlagBits::eShaderRead;
	
	if (eOldLayout == vk::ImageLayout::eUndefined)
	{
		eSrcStage = vk::PipelineStageFlagBits::eTopOfPipe;
		eSrcAccess = {};
	}
	
	vk::ImageMemoryBarrier barrier = vk::ImageMemoryBarrier()
		.setOldLayout(eOldLayout)
		.setNewLayout(eNewLayout)
		.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
		.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
		.setImage(Zenith_Vulkan::GetVRAM(pxTexture->m_xVRAMHandle)->GetImage())
		.setSubresourceRange(vk::ImageSubresourceRange()
			.setAspectMask(vk::ImageAspectFlagBits::eColor)
			.setBaseMipLevel(0)
			.setLevelCount(1)
			.setBaseArrayLayer(0)
			.setLayerCount(1))
		.setSrcAccessMask(eSrcAccess)
		.setDstAccessMask(eDstAccess);
	
	m_xCurrentCmdBuffer.pipelineBarrier(
		eSrcStage, eDstStage,
		{}, 0, nullptr, 0, nullptr, 1, &barrier);
}
