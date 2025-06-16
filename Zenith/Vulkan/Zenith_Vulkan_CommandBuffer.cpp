#include "Zenith.h"

#include "Zenith_Vulkan_CommandBuffer.h"
#include "Zenith_Vulkan_Texture.h"

#include "Zenith_Vulkan.h"
#include "Zenith_Vulkan_Buffer.h"
#include "Flux/Flux.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"

//#TO purely for the static assert in SetIndexBuffer
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"

void Zenith_Vulkan_CommandBuffer::Initialise(CommandType eType /*= COMMANDTYPE_GRAPHICS*/, bool bAsChild /*= false*/)
{
	m_eCommandType = eType;
	m_bIsChild = bAsChild;
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();
	vk::CommandBufferAllocateInfo xAllocInfo{};
	xAllocInfo.commandPool = Zenith_Vulkan::GetCommandPool(eType);
	xAllocInfo.level = bAsChild ? vk::CommandBufferLevel::eSecondary : vk::CommandBufferLevel::ePrimary;
	xAllocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
	m_xCmdBuffers = xDevice.allocateCommandBuffers(xAllocInfo);

	m_xCompleteSems.resize(MAX_FRAMES_IN_FLIGHT);
	vk::SemaphoreCreateInfo xSemaphoreInfo;
	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		m_xCompleteSems[i] = xDevice.createSemaphore(xSemaphoreInfo);
	}
}

void Zenith_Vulkan_CommandBuffer::BeginRecording()
{
	m_xCurrentCmdBuffer = m_xCmdBuffers[Zenith_Vulkan_Swapchain::GetCurrentFrameIndex()];
	if (!m_bIsChild)
	{
		m_xCurrentCmdBuffer.begin(vk::CommandBufferBeginInfo());
	}
	else
	{
		Zenith_Assert(m_pxParent != nullptr, "Child has no parent");
		Zenith_Assert(m_pxParent->m_xCurrentRenderPass != VK_NULL_HANDLE && m_pxParent->m_xCurrentFramebuffer != VK_NULL_HANDLE, "Parent isn't recording a render pass");
		vk::CommandBufferInheritanceInfo xInheritInfo = vk::CommandBufferInheritanceInfo()
			.setRenderPass(m_pxParent->m_xCurrentRenderPass)
			.setFramebuffer(m_pxParent->m_xCurrentFramebuffer)
			.setSubpass(0);

		vk::CommandBufferBeginInfo xBeginInfo = vk::CommandBufferBeginInfo()
			.setPInheritanceInfo(&xInheritInfo)
			.setFlags(vk::CommandBufferUsageFlagBits::eRenderPassContinue);

		m_xCurrentCmdBuffer.begin(xBeginInfo);

		m_xCurrentCmdBuffer.setViewport(0, 1, &m_pxParent->m_xViewport);
		m_xCurrentCmdBuffer.setScissor(0, 1, &m_pxParent->m_xScissor);
	}

	m_uCurrentBindFreq = FLUX_MAX_DESCRIPTOR_SET_LAYOUTS;

	m_uDescriptorDirty = ~0u;

	m_bIsRecording = true;
}
void Zenith_Vulkan_CommandBuffer::EndRenderPass()
{
	m_xCurrentCmdBuffer.endRenderPass();
}
void Zenith_Vulkan_CommandBuffer::EndRecording(RenderOrder eOrder, bool bEndPass /*= true*/)
{
	//#TO_TODO: should I pass in false for bEndPass if this is a child instead of this check?
	if (bEndPass && !m_bIsChild)
	{
		m_xCurrentCmdBuffer.endRenderPass();
	}

	m_xCurrentCmdBuffer.end();
	if (!m_bIsChild)
	{
		Zenith_Vulkan::SubmitCommandBuffer(this, eOrder);
	}

	m_uCurrentBindFreq = FLUX_MAX_DESCRIPTOR_SET_LAYOUTS;

	m_bIsRecording = false;
}

void Zenith_Vulkan_CommandBuffer::CreateChild(Zenith_Vulkan_CommandBuffer& xChild)
{
	xChild.m_pxParent = this;
	m_bIsParent = true;
}

void Zenith_Vulkan_CommandBuffer::ExecuteChild(Zenith_Vulkan_CommandBuffer& xChild)
{
	m_xCurrentCmdBuffer.executeCommands(xChild.GetCurrentCmdBuffer());
}

void Zenith_Vulkan_CommandBuffer::EndAndCpuWait(bool bEndPass)
{
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();

	if (bEndPass)
	{
		m_xCurrentCmdBuffer.endRenderPass();
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

	//#TO_TODO: plug fence leak
}

void Zenith_Vulkan_CommandBuffer::SetVertexBuffer(const Flux_VertexBuffer& xVertexBuffer, uint32_t uBindPoint /*= 0*/)
{
	const vk::Buffer xBuffer = xVertexBuffer.GetBuffer().GetBuffer();
	//#TO_TODO: offsets
	vk::DeviceSize offsets[] = { 0 };
	m_xCurrentCmdBuffer.bindVertexBuffers(uBindPoint, 1, &xBuffer, offsets);
}

void Zenith_Vulkan_CommandBuffer::SetVertexBuffer(const Flux_DynamicVertexBuffer& xVertexBuffer, uint32_t uBindPoint /*= 0*/)
{
	const vk::Buffer xBuffer = xVertexBuffer.GetBuffer().GetBuffer();
	//#TO_TODO: offsets
	vk::DeviceSize offsets[] = { 0 };
	m_xCurrentCmdBuffer.bindVertexBuffers(uBindPoint, 1, &xBuffer, offsets);
}

void Zenith_Vulkan_CommandBuffer::SetIndexBuffer(const Flux_IndexBuffer& xIndexBuffer)
{
	const vk::Buffer xBuffer = xIndexBuffer.GetBuffer().GetBuffer();
	static_assert(std::is_same<Flux_MeshGeometry::IndexType, uint32_t>(), "#TO_TODO: stop hardcoding type");
	m_xCurrentCmdBuffer.bindIndexBuffer(xBuffer, 0, vk::IndexType::eUint32);
}

void Zenith_Vulkan_CommandBuffer::PrepareDrawCallDescriptors()
{
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();
	for (u_int uDescSet = 0; uDescSet < m_pxCurrentPipeline->m_xRootSig.m_uNumDescriptorSets; uDescSet++)
	{
		if (m_uDescriptorDirty & 1 << uDescSet)
		{
			vk::DescriptorSetLayout& xLayout = m_pxCurrentPipeline->m_xRootSig.m_axDescSetLayouts[uDescSet];

			vk::DescriptorSetAllocateInfo xInfo = vk::DescriptorSetAllocateInfo()
				.setDescriptorPool(Zenith_Vulkan::GetCurrentPerFrameDescriptorPool())
				.setDescriptorSetCount(1)
				.setPSetLayouts(&xLayout);

			m_axCurrentDescSet[uDescSet] = xDevice.allocateDescriptorSets(xInfo)[0];

			uint32_t uNumTextures = 0;
			for (uint32_t i = 0; i < MAX_BINDINGS; i++)
			{
				if (m_xBindings[uDescSet].m_xTextures[i] != nullptr)
				{
					uNumTextures++;
				}
			}

			uint32_t uNumBuffers = 0;
			for (uint32_t i = 0; i < MAX_BINDINGS; i++)
			{
				if (m_xBindings[uDescSet].m_xBuffers[i] != nullptr)
				{
					uNumBuffers++;
				}
			}

			std::vector<vk::DescriptorImageInfo> xTexInfos(uNumTextures);
			std::vector<vk::WriteDescriptorSet> xTexWrites(uNumTextures);
			uint32_t uCount = 0;
			for (uint32_t i = 0; i < MAX_BINDINGS; i++)
			{
				Zenith_Vulkan_Texture* pxTex = m_xBindings[uDescSet].m_xTextures[i];
				if (!pxTex)
				{
					continue;
				}

				vk::ImageLayout eLayout;
				switch (pxTex->GetTextureFormat())
				{
				case vk::Format::eD32Sfloat:
					eLayout = vk::ImageLayout::eDepthReadOnlyOptimal;
					break;
				case vk::Format::eD32SfloatS8Uint:
					eLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal;
					break;
				default:
					eLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
				}

				//#TO_TODO: different samplers
				vk::DescriptorImageInfo& xInfo = xTexInfos.at(uCount)
					.setSampler(Flux_Graphics::s_xDefaultSampler.GetSampler())
					.setImageView(pxTex->GetImageView())
					.setImageLayout(eLayout);

				vk::WriteDescriptorSet& xWrite = xTexWrites.at(uCount)
					.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
					.setDstSet(m_axCurrentDescSet[uDescSet])
					.setDstBinding(i)
					.setDstArrayElement(0)
					.setDescriptorCount(1)
					.setPImageInfo(&xInfo);

				uCount++;
			}

			xDevice.updateDescriptorSets(xTexWrites.size(), xTexWrites.data(), 0, nullptr);

			std::vector<vk::DescriptorBufferInfo> xBufferInfos(uNumBuffers);
			std::vector<vk::WriteDescriptorSet> xBufferWrites(uNumBuffers);
			uCount = 0;
			for (uint32_t i = 0; i < MAX_BINDINGS; i++)
			{
				Zenith_Vulkan_Buffer* pxBuf = m_xBindings[uDescSet].m_xBuffers[i];
				if (!pxBuf)
				{
					continue;
				}
				vk::Buffer xBuffer = pxBuf->GetBuffer();
				vk::DescriptorBufferInfo& xInfo = xBufferInfos.at(uCount)
					.setBuffer(xBuffer)
					.setOffset(0)
					.setRange(pxBuf->GetSize());

				vk::WriteDescriptorSet& xWrite = xBufferWrites.at(uCount)
					.setDescriptorType(vk::DescriptorType::eUniformBuffer)
					.setDstSet(m_axCurrentDescSet[uDescSet])
					.setDstBinding(i)
					.setDstArrayElement(0)
					.setDescriptorCount(1)
					.setPBufferInfo(&xInfo);

				uCount++;
			}

			xDevice.updateDescriptorSets(xBufferWrites.size(), xBufferWrites.data(), 0, nullptr);
			m_xCurrentCmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pxCurrentPipeline->m_xRootSig.m_xLayout, uDescSet, 1, &m_axCurrentDescSet[uDescSet], 0, nullptr);
			m_uDescriptorDirty &= ~(1 << uDescSet);
		}

		
	}
	
}

void Zenith_Vulkan_CommandBuffer::Draw(uint32_t uNumVerts)
{
	PrepareDrawCallDescriptors();
	if (Zenith_Vulkan::ShouldSubmitDrawCalls())
	{
		m_xCurrentCmdBuffer.draw(uNumVerts, 0, 0, 0);
	}
}

void Zenith_Vulkan_CommandBuffer::DrawIndexed(uint32_t uNumIndices, uint32_t uNumInstances /*= 1*/, uint32_t uVertexOffset /*= 0*/, uint32_t uIndexOffset /*= 0*/, uint32_t uInstanceOffset /*= 0*/)
{
	PrepareDrawCallDescriptors();
	if (Zenith_Vulkan::ShouldSubmitDrawCalls())
	{
		m_xCurrentCmdBuffer.drawIndexed(uNumIndices, uNumInstances, uIndexOffset, uVertexOffset, uInstanceOffset);
	}
}

void Zenith_Vulkan_CommandBuffer::SubmitTargetSetup(Flux_TargetSetup& xTargetSetup, bool bClearColour /*= false*/, bool bClearDepth /*= false*/, bool bClearStencil /*= false*/)
{
	//#TO_TODO: how to clear depth/stencil independently
	LoadAction eColourLoad = bClearColour ? LOAD_ACTION_CLEAR : LOAD_ACTION_LOAD;
	StoreAction eColourStore = STORE_ACTION_STORE;
	LoadAction eDepthStencilLoad = bClearDepth ? LOAD_ACTION_CLEAR : LOAD_ACTION_LOAD;
	StoreAction eDepthStencilStore = STORE_ACTION_STORE;
	RenderTargetUsage eUsage = RENDER_TARGET_USAGE_RENDERTARGET;

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

	m_xCurrentRenderPass = Zenith_Vulkan_Pipeline::TargetSetupToRenderPass(xTargetSetup, eColourLoad, eColourStore, eDepthStencilLoad, eDepthStencilStore, eUsage);

	uint32_t uWidth, uHeight;
	if (xTargetSetup.m_axColourAttachments[0].m_eColourFormat != COLOUR_FORMAT_NONE)
	{
		uWidth = xTargetSetup.m_axColourAttachments[0].m_uWidth;
		uHeight = xTargetSetup.m_axColourAttachments[0].m_uHeight;
	}
	else
	{
		Zenith_Assert(xTargetSetup.m_pxDepthStencil->m_eDepthStencilFormat != DEPTHSTENCIL_FORMAT_NONE, "Target setup has no attachments");
		uWidth = xTargetSetup.m_pxDepthStencil->m_uWidth;
		uHeight = xTargetSetup.m_pxDepthStencil->m_uHeight;

	}

	m_xCurrentFramebuffer = Zenith_Vulkan_Pipeline::TargetSetupToFramebuffer(xTargetSetup, uWidth, uHeight, m_xCurrentRenderPass);

	vk::RenderPassBeginInfo xRenderPassInfo = vk::RenderPassBeginInfo()
		.setRenderPass(m_xCurrentRenderPass)
		.setFramebuffer(m_xCurrentFramebuffer)
		.setRenderArea({ {0,0}, {uWidth, uHeight} });

	vk::ClearValue axClearColour[FLUX_MAX_TARGETS+1];
	if (eColourLoad == LOAD_ACTION_CLEAR || eDepthStencilLoad == LOAD_ACTION_CLEAR)
	{
		bool bHasDepth = xTargetSetup.m_pxDepthStencil != nullptr;
		const uint32_t uNumAttachments = bHasDepth && eDepthStencilLoad == LOAD_ACTION_CLEAR ? uNumColourAttachments + 1 : uNumColourAttachments;
		for (uint32_t i = 0; i < uNumColourAttachments; i++)
		{
			axClearColour[i].color = vk::ClearColorValue(0.f, 0.f, 0.f, 0.f);
		}
		axClearColour[uNumColourAttachments].depthStencil = vk::ClearDepthStencilValue(1, 0);

		xRenderPassInfo.clearValueCount = uNumAttachments;
		xRenderPassInfo.pClearValues = axClearColour;
	}

	m_xCurrentCmdBuffer.beginRenderPass(xRenderPassInfo, m_bIsParent ? vk::SubpassContents::eSecondaryCommandBuffers : vk::SubpassContents::eInline);

	m_xViewport.x = 0;
	m_xViewport.y = 0;
	m_xViewport.width = uWidth;
	m_xViewport.height = uHeight;
	m_xViewport.minDepth = 0;
	m_xViewport.maxDepth = 1;

	m_xScissor.offset = vk::Offset2D(0, 0);
	m_xScissor.extent = vk::Extent2D(m_xViewport.width, m_xViewport.height);

	if (!m_bIsParent)
	{
		m_xCurrentCmdBuffer.setViewport(0, 1, &m_xViewport);
		m_xCurrentCmdBuffer.setScissor(0, 1, &m_xScissor);
	}
}

void Zenith_Vulkan_CommandBuffer::SetPipeline(Zenith_Vulkan_Pipeline* pxPipeline)
{
	m_xCurrentCmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pxPipeline->m_xPipeline);
	m_pxCurrentPipeline = pxPipeline;
}

void Zenith_Vulkan_CommandBuffer::BindTexture(Zenith_Vulkan_Texture* pxTexture, uint32_t uBindPoint)
{
	Zenith_Assert(m_uCurrentBindFreq < FLUX_MAX_DESCRIPTOR_SET_LAYOUTS, "Haven't called BeginBind");

	if (pxTexture != m_aapxTextureCache[m_uCurrentBindFreq][uBindPoint])
	{
		m_uDescriptorDirty |= 1<<m_uCurrentBindFreq;
		m_aapxTextureCache[m_uCurrentBindFreq][uBindPoint] = pxTexture;
	}
	m_xBindings[m_uCurrentBindFreq].m_xTextures[uBindPoint] = pxTexture;
}

void Zenith_Vulkan_CommandBuffer::BindBuffer(Zenith_Vulkan_Buffer* pxBuffer, uint32_t uBindPoint)
{
	Zenith_Assert(m_uCurrentBindFreq < FLUX_MAX_DESCRIPTOR_SET_LAYOUTS, "Haven't called BeginBind");

	if (pxBuffer != m_aapxBufferCache[m_uCurrentBindFreq][uBindPoint])
	{
		m_uDescriptorDirty |= 1 << m_uCurrentBindFreq;
		m_aapxBufferCache[m_uCurrentBindFreq][uBindPoint] = pxBuffer;
	}
	m_xBindings[m_uCurrentBindFreq].m_xBuffers[uBindPoint] = pxBuffer;
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

void Zenith_Vulkan_CommandBuffer::BeginBind(u_int uDescSet)
{
	for (uint32_t i = 0; i < MAX_BINDINGS; i++)
	{
		m_xBindings[uDescSet].m_xBuffers[i] = nullptr;
		m_xBindings[uDescSet].m_xTextures[i] = nullptr;
	}
	m_uCurrentBindFreq = uDescSet;
}
void Zenith_Vulkan_CommandBuffer::ImageTransitionBarrier(vk::Image xImage, vk::ImageLayout eOldLayout, vk::ImageLayout eNewLayout, vk::ImageAspectFlags eAspect, vk::PipelineStageFlags eSrcStage, vk::PipelineStageFlags eDstStage, int uMipLevel, int uLayer)
{
	vk::ImageSubresourceRange xSubRange = vk::ImageSubresourceRange(eAspect, uMipLevel, 1, uLayer, 1);

	vk::ImageMemoryBarrier xMemoryBarrier = vk::ImageMemoryBarrier()
		.setSubresourceRange(xSubRange)
		.setImage(xImage)
		.setOldLayout(eOldLayout)
		.setNewLayout(eNewLayout);

	switch (eNewLayout)
	{
	case (vk::ImageLayout::eTransferDstOptimal):
		xMemoryBarrier.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
		break;
	case (vk::ImageLayout::eTransferSrcOptimal):
		xMemoryBarrier.setDstAccessMask(vk::AccessFlagBits::eTransferRead);
		break;
	case (vk::ImageLayout::eColorAttachmentOptimal):
		xMemoryBarrier.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
		break;
	case (vk::ImageLayout::eDepthAttachmentOptimal):
		xMemoryBarrier.setDstAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentWrite);
		break;
	case (vk::ImageLayout::eDepthStencilAttachmentOptimal):
		xMemoryBarrier.setDstAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentWrite);
		break;
	case (vk::ImageLayout::eShaderReadOnlyOptimal):
		xMemoryBarrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
		break;
	default:
		Zenith_Assert(false, "unknown layout");
		break;
	}

	m_xCurrentCmdBuffer.pipelineBarrier(eSrcStage, eDstStage, vk::DependencyFlags(), 0, nullptr, 0, nullptr, 1, &xMemoryBarrier);
}

void Zenith_Vulkan_CommandBuffer::CopyBufferToBuffer(Zenith_Vulkan_Buffer* pxSrc, Zenith_Vulkan_Buffer* pxDst, size_t uSize, size_t uSrcOffset)
{
	vk::BufferCopy xCopyRegion(uSrcOffset, 0, uSize);
	m_xCurrentCmdBuffer.copyBuffer(pxSrc->GetBuffer(), pxDst->GetBuffer(), xCopyRegion);
}

void Zenith_Vulkan_CommandBuffer::CopyBufferToTexture(Zenith_Vulkan_Buffer* pxSrc, Zenith_Vulkan_Texture* pxDst, size_t uSrcOffset, uint32_t uNumLayers)
{
	vk::ImageSubresourceLayers xSubresource = vk::ImageSubresourceLayers()
		.setAspectMask(vk::ImageAspectFlagBits::eColor)
		.setMipLevel(0)
		.setBaseArrayLayer(0)
		.setLayerCount(uNumLayers);

	vk::BufferImageCopy region = vk::BufferImageCopy()
		.setBufferOffset(uSrcOffset)
		.setBufferRowLength(0)
		.setBufferImageHeight(0)
		.setImageSubresource(xSubresource)
		.setImageOffset({ 0,0,0 })
		.setImageExtent({ pxDst->GetWidth(), pxDst->GetHeight(), 1 });

	m_xCurrentCmdBuffer.copyBufferToImage(pxSrc->GetBuffer(), pxDst->
		GetImage(), vk::ImageLayout::eTransferDstOptimal, 1, &region);
}
void Zenith_Vulkan_CommandBuffer::BlitTextureToTexture(Zenith_Vulkan_Texture* pxSrc, Zenith_Vulkan_Texture* pxDst, uint32_t uDstMip, uint32_t uDstLayer)
{
	std::array<vk::Offset3D, 2> axSrcOffsets;
	axSrcOffsets.at(0).setX(0);
	axSrcOffsets.at(0).setY(0);
	axSrcOffsets.at(0).setZ(0);
	axSrcOffsets.at(1).setX(pxSrc->GetWidth());
	axSrcOffsets.at(1).setY(pxSrc->GetHeight());
	axSrcOffsets.at(1).setZ(1);

	vk::ImageSubresourceLayers xSrcSubresource = vk::ImageSubresourceLayers()
		.setAspectMask(vk::ImageAspectFlagBits::eColor)
		.setMipLevel(0)
		.setBaseArrayLayer(uDstLayer)
		.setLayerCount(1);

	std::array<vk::Offset3D, 2> axDstOffsets;
	axDstOffsets.at(0).setX(0);
	axDstOffsets.at(0).setY(0);
	axDstOffsets.at(0).setZ(0);
	axDstOffsets.at(1).setX(pxSrc->GetWidth() / std::pow(2, uDstMip));
	axDstOffsets.at(1).setY(pxSrc->GetHeight() / std::pow(2, uDstMip));
	axDstOffsets.at(1).setZ(1);

	vk::ImageSubresourceLayers xDstSubresource = vk::ImageSubresourceLayers()
		.setAspectMask(vk::ImageAspectFlagBits::eColor)
		.setMipLevel(uDstMip)
		.setBaseArrayLayer(uDstLayer)
		.setLayerCount(1);

	vk::ImageBlit xBlit = vk::ImageBlit()
		.setSrcOffsets(axSrcOffsets)
		.setDstOffsets(axDstOffsets)
		.setSrcSubresource(xSrcSubresource)
		.setDstSubresource(xDstSubresource);

	m_xCurrentCmdBuffer.blitImage(pxSrc->GetImage(), vk::ImageLayout::eTransferSrcOptimal, pxDst->GetImage(), vk::ImageLayout::eTransferDstOptimal, xBlit, vk::Filter::eLinear);
}