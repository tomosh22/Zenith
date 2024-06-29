#include "Zenith.h"

#include "Zenith_Vulkan_CommandBuffer.h"

#include "Zenith_Vulkan.h"
#include "Flux/Zenith_Flux.h"


Zenith_Vulkan_CommandBuffer::Zenith_Vulkan_CommandBuffer(CommandType eType /*= COMMANDTYPE_GRAPHICS*/) : m_eCommandType(eType)
{
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();
	vk::CommandBufferAllocateInfo xAllocInfo{};
	xAllocInfo.commandPool = Zenith_Vulkan::GetCommandPool(eType);
	xAllocInfo.level = vk::CommandBufferLevel::ePrimary;
	xAllocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
	m_xCmdBuffers = xDevice.allocateCommandBuffers(xAllocInfo);

	m_xCompleteSems.resize(MAX_FRAMES_IN_FLIGHT);
	vk::SemaphoreCreateInfo semaphoreInfo;
	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		m_xCompleteSems[i] = xDevice.createSemaphore(semaphoreInfo);
	}

	m_xRenderPasses.resize(MAX_FRAMES_IN_FLIGHT);
	m_xFramebuffers.resize(MAX_FRAMES_IN_FLIGHT);
}
void Zenith_Vulkan_CommandBuffer::BeginRecording()
{
	m_xCurrentCmdBuffer = m_xCmdBuffers[Zenith_Flux::GetFrameIndex()];
	m_xCurrentCmdBuffer.begin(vk::CommandBufferBeginInfo());

	m_eCurrentBindFreq = BINDING_FREQUENCY_MAX;

	m_bIsRecording = true;
}
void Zenith_Vulkan_CommandBuffer::EndRecording(RenderOrder eOrder, bool bEndPass /*= true*/)
{

	if (bEndPass)
	{
		m_xCurrentCmdBuffer.endRenderPass();
	}

	m_xCurrentCmdBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, {}, vk::MemoryBarrier(vk::AccessFlagBits::eDepthStencilAttachmentWrite, vk::AccessFlagBits::eVertexAttributeRead), {}, {});

	m_xCurrentCmdBuffer.end();
	Zenith_Flux::SubmitCommandBuffer(this, eOrder);

	m_eCurrentBindFreq = BINDING_FREQUENCY_MAX;

	m_bIsRecording = false;

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
}

void Zenith_Vulkan_CommandBuffer::SetVertexBuffer(Zenith_Vulkan_VertexBuffer* pxVertexBuffer, uint32_t uBindPoint /*= 0*/)
{
	STUBBED
	//const vk::Buffer& xBuffer = pxVertexBuffer->GetBuffer();
	////TODO: offsets
	//vk::DeviceSize offsets[] = { 0 };
	//m_xCurrentCmdBuffer.bindVertexBuffers(uBindPoint, 1, &xBuffer, offsets);
}

void Zenith_Vulkan_CommandBuffer::SetIndexBuffer(Zenith_Vulkan_IndexBuffer* pxIndexBuffer)
{
	STUBBED
	/*
	const vk::Buffer& xBuffer = pxIndexBuffer->GetBuffer();
	m_xCurrentCmdBuffer.bindIndexBuffer(xBuffer, 0, vk::IndexType::eUint32);
	*/
}
void Zenith_Vulkan_CommandBuffer::Draw(uint32_t uNumIndices, uint32_t uNumInstances /*= 1*/, uint32_t uVertexOffset /*= 0*/, uint32_t uIndexOffset /*= 0*/, uint32_t uInstanceOffset /*= 0*/)
{
	STUBBED
	/*
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();

	if (m_pxCurrentPipeline->m_axDescLayouts.size() > 1)
	{
		vk::DescriptorSetLayout& xLayout = m_pxCurrentPipeline->m_axDescLayouts[1];

		vk::DescriptorSetAllocateInfo xInfo = vk::DescriptorSetAllocateInfo()
			.setDescriptorPool(Zenith_Vulkan::GetCurrentPerFrameDescriptorPool())
			.setDescriptorSetCount(1)
			.setPSetLayouts(&xLayout);

		vk::DescriptorSet xSet = xDevice.allocateDescriptorSets(xInfo)[0];

		uint32_t uNumTextures = 0;
		for (uint32_t i = 0; i < MAX_BINDINGS; i++) {
			if (m_xBindings[BINDING_FREQUENCY_PER_DRAW].m_xTextures[i] != nullptr)
			{
				uNumTextures++;
			}
			else
			{
#if 0//def VCE_ASSERT
				for (uint32_t j = i + 1; j < MAX_BINDINGS; j++)
				{
					VCE_Assert(m_xBindings[BINDING_FREQUENCY_PER_DRAW].m_xTextures[j] == nullptr, "All non null textures must be contiguous");
				}
#endif
				break;
			}

		}

		uint32_t uNumBuffers = 0;
		for (uint32_t i = 0; i < MAX_BINDINGS; i++)
		{
			if (m_xBindings[BINDING_FREQUENCY_PER_DRAW].m_xBuffers[i] != nullptr)
			{
				uNumBuffers++;
			}
			else
			{
#if 0//def VCE_ASSERT
				for (uint32_t j = i + 1; j < MAX_BINDINGS; j++)
				{
					VCE_Assert(m_xBindings[BINDING_FREQUENCY_PER_DRAW].m_xBuffers[j] == nullptr, "All non null buffers must be contiguous");
				}
#endif
				break;
			}

		}

		std::vector<vk::DescriptorImageInfo> xTexInfos(uNumTextures);
		std::vector<vk::WriteDescriptorSet> xTexWrites(uNumTextures);
		uint32_t uCount = 0;
		for (uint32_t i = 0; i < uNumTextures; i++) {
			Zenith_Vulkan_Texture* pxTex = m_xBindings[BINDING_FREQUENCY_PER_DRAW].m_xTextures[i];

			vk::DescriptorImageInfo& xInfo = xTexInfos.at(uCount)
				.setSampler(pxTex->m_xSampler)
				.setImageView(pxTex->m_xImageView)
				.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

			vk::WriteDescriptorSet& xWrite = xTexWrites.at(uCount)
				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
				.setDstSet(xSet)
				.setDstBinding(uCount)
				.setDstArrayElement(0)
				.setDescriptorCount(1)
				.setPImageInfo(&xInfo);

			uCount++;
		}

		xDevice.updateDescriptorSets(xTexWrites.size(), xTexWrites.data(), 0, nullptr);

		std::vector<vk::DescriptorBufferInfo> xBufferInfos(uNumBuffers);
		std::vector<vk::WriteDescriptorSet> xBufferWrites(uNumBuffers);
		for (uint32_t i = 0; i < uNumBuffers; i++) {
			Zenith_Vulkan_Buffer* pxBuf = m_xBindings[BINDING_FREQUENCY_PER_DRAW].m_xBuffers[i];

			vk::DescriptorBufferInfo& xInfo = xBufferInfos.at(uCount)
				.setBuffer(pxVkBuf->m_xBuffer)
				.setOffset(0)
				.setRange(pxVkBuf->GetSize());

			vk::WriteDescriptorSet& xWrite = xBufferWrites.at(uCount)
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDstSet(xSet)
				.setDstBinding(uCount)
				.setDstArrayElement(0)
				.setDescriptorCount(1)
				.setPBufferInfo(&xInfo);

			uCount++;
		}

		xDevice.updateDescriptorSets(xBufferWrites.size(), xBufferWrites.data(), 0, nullptr);

		m_xCurrentCmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pxCurrentPipeline->m_xPipelineLayout, (int)BINDING_FREQUENCY_PER_DRAW, 1, &xSet, 0, nullptr);
	}

	m_xCurrentCmdBuffer.drawIndexed(uNumIndices, uNumInstances, uIndexOffset, uVertexOffset, uInstanceOffset);
	*/
}

void Zenith_Vulkan_CommandBuffer::SubmitTargetSetup(const char* szName)
{
	STUBBED
	/*
	VulkanRenderPass* pxRenderPass = m_pxRenderer->m_xTargetSetupPasses.at(szName);
	m_xCurrentRenderPass = pxRenderPass->m_xRenderPass;

	
	m_xCurrentFramebuffer = m_pxRenderer->m_xTargetSetupFramebuffers.at(szName).at(m_pxRenderer->m_currentFrame);


	vk::RenderPassBeginInfo xRenderPassInfo = vk::RenderPassBeginInfo()
		.setRenderPass(m_xCurrentRenderPass)
		.setFramebuffer(m_xCurrentFramebuffer)
		.setRenderArea({ {0,0}, m_pxRenderer->m_swapChainExtent });

	vk::ClearValue* axClearColour = nullptr;
	//im being lazy and assuming all render targets have the same load action
	if (pxRenderPass->m_eColourLoadAction == RendererAPI::TargetSetup::LoadAction::Clear) {
		bool bHasDepth = pxRenderPass->m_eDepthStencilFormat != RendererAPI::RenderAttachment::Format::None;
		const uint32_t uNumColourAttachments = pxRenderPass->m_uNumColourAttachments;
		const uint32_t uNumAttachments = bHasDepth ? uNumColourAttachments + 1 : uNumColourAttachments;
		axClearColour = new vk::ClearValue[uNumAttachments];
		std::array<float, 4> tempColour{ 0.f,0.f,0.f,1.f };
		for (uint32_t i = 0; i < uNumColourAttachments; i++)
		{
			axClearColour[i].color = { vk::ClearColorValue(tempColour) };
			axClearColour[i].depthStencil = vk::ClearDepthStencilValue(0, 0);
		}
		if(bHasDepth)
			axClearColour[uNumAttachments-1].depthStencil = vk::ClearDepthStencilValue(0, 0);

		xRenderPassInfo.clearValueCount = uNumAttachments;
		xRenderPassInfo.pClearValues = axClearColour;
	}


	m_xCurrentCmdBuffer.beginRenderPass(xRenderPassInfo, vk::SubpassContents::eInline);

	if (axClearColour != nullptr) delete[] axClearColour;

	//flipping because porting from opengl
	vk::Viewport xViewport{};
	xViewport.x = 0;
	xViewport.y = m_pxRenderer->m_swapChainExtent.height;
	xViewport.width = m_pxRenderer->m_swapChainExtent.width;
	xViewport.height = -1 * (float)m_pxRenderer->m_swapChainExtent.height;
	xViewport.minDepth = 0;
	xViewport.minDepth = 1;

	vk::Rect2D xScissor{};
	xScissor.offset = vk::Offset2D(0, 0);
	xScissor.extent = m_pxRenderer->m_swapChainExtent;

	m_xCurrentCmdBuffer.setViewport(0, 1, &xViewport);
	m_xCurrentCmdBuffer.setScissor(0, 1, &xScissor);
	*/
}

void Zenith_Vulkan_CommandBuffer::SetPipeline(Zenith_Vulkan_Pipeline* pxPipeline)
{
	STUBBED
	/*
	m_xCurrentCmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pxPipeline->m_xPipeline);
	std::vector<vk::DescriptorSet> axSets;
	//new pipelines (skinned meshes)
	if (pxPipeline->m_axDescLayouts.size()) {
		for (const vk::DescriptorSet set : pxPipeline->m_axDescSets[m_pxRenderer->m_currentFrame])
			axSets.push_back(set);
	}
	m_xCurrentCmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pxPipeline->m_xPipelineLayout, 0, axSets.siz(), axSets.data(), 0, nullptr);

	m_pxCurrentPipeline = pxPipeline;
	*/
}

void Zenith_Vulkan_CommandBuffer::BindTexture(void* pxTexture, uint32_t uBindPoint, uint32_t uSet) {
	STUBBED
	/*
	VCE_Assert(m_eCurrentBindFreq < BINDING_FREQUENCY_MAX, "Haven't called BeginBind");

	if (m_eCurrentBindFreq == BINDING_FREQUENCY_PER_FRAME) {
		VulkanTexture2D* pxTex = reinterpret_cast<VulkanTexture2D*>(pxTexture);

		vk::DescriptorImageInfo xInfo = vk::DescriptorImageInfo()
			.setSampler(pxTex->m_xSampler)
			.setImageView(pxTex->m_xImageView)
			.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

		vk::WriteDescriptorSet xWrite = vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			//#TO index 0 for per frame set
			.setDstSet(m_pxCurrentPipeline->m_axDescSets[m_pxRenderer->m_currentFrame][0])
			.setDstBinding(uBindPoint)
			.setDstArrayElement(0)
			.setDescriptorCount(1)
			.setPImageInfo(&xInfo);

		m_pxRenderer->GetDevice().updateDescriptorSets(1, &xWrite, 0, nullptr);
	}
	else if(m_eCurrentBindFreq == BINDING_FREQUENCY_PER_DRAW)
		m_xBindings[m_eCurrentBindFreq].m_xTextures[uBindPoint] = reinterpret_cast<Texture*>(pxTexture);
	*/
}

void Zenith_Vulkan_CommandBuffer::BindBuffer(void* pxBuffer, uint32_t uBindPoint, uint32_t uSet) {
	STUBBED
	/*
	VCE_Assert(m_eCurrentBindFreq < BINDING_FREQUENCY_MAX, "Haven't called BeginBind");

	if (m_eCurrentBindFreq == BINDING_FREQUENCY_PER_FRAME) {
		VulkanBuffer* pxBuf = reinterpret_cast<VulkanBuffer*>(pxBuffer);

		vk::DescriptorBufferInfo xInfo = vk::DescriptorBufferInfo()
			.setBuffer(pxBuf->m_xBuffer)
			.setOffset(0)
			.setRange(pxBuf->GetSize());

		vk::WriteDescriptorSet xWrite = vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			//#TO index 0 for per frame set
			.setDstSet(m_pxCurrentPipeline->m_axDescSets[m_pxRenderer->m_currentFrame][0])
			.setDstBinding(uBindPoint)
			.setDescriptorCount(1)
			.setPBufferInfo(&xInfo);

		m_pxRenderer->GetDevice().updateDescriptorSets(1, &xWrite, 0, nullptr);

	}
	else if (m_eCurrentBindFreq == BINDING_FREQUENCY_PER_DRAW)
		m_xBindings[m_eCurrentBindFreq].m_xBuffers[uBindPoint] = reinterpret_cast<Buffer*>(pxBuffer);
	*/
}

void Zenith_Vulkan_CommandBuffer::BindAccelerationStruct(void* pxStruct, uint32_t uBindPoint, uint32_t uSet) {
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
	STUBBED
	/*
	m_xCurrentCmdBuffer.pushConstants(m_pxCurrentPipeline->m_xPipelineLayout, vk::ShaderStageFlagBits::eAll, 0, uSize, pData);
	*/
}

void Zenith_Vulkan_CommandBuffer::BeginBind(BindingFrequency eFreq)
{
	for (uint32_t i = 0; i < MAX_BINDINGS; i++)
	{
		m_xBindings[eFreq].m_xBuffers[i] = nullptr;
		m_xBindings[eFreq].m_xTextures[i] = nullptr;
	}
	m_eCurrentBindFreq = eFreq;
}
void Zenith_Vulkan_CommandBuffer::ImageTransitionBarrier(vk::Image xImage, vk::ImageLayout eOldLayout, vk::ImageLayout eNewLayout,vk::ImageAspectFlags eAspect, vk::PipelineStageFlags eSrcStage, vk::PipelineStageFlags eDstStage, int uMipLevel, int uLayer)
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

void Zenith_Vulkan_CommandBuffer::CopyBufferToBuffer(Zenith_Vulkan_Buffer* pxSrc, Zenith_Vulkan_Buffer* pxDst, size_t uSize, size_t uSrcOffset) {
	STUBBED
	//vk::BufferCopy xCopyRegion(uSrcOffset, 0, uSize);
	//m_xCurrentCmdBuffer.copyBuffer(pxSrc->m_xBuffer, pxDst->m_xBuffer, xCopyRegion);
}

void Zenith_Vulkan_CommandBuffer::CopyBufferToTexture(Zenith_Vulkan_Buffer* pxSrc, Zenith_Vulkan_Texture* pxDst, size_t uSrcOffset) {
	STUBBED
	/*vk::ImageSubresourceLayers xSubresource = vk::ImageSubresourceLayers()
		.setAspectMask(vk::ImageAspectFlagBits::eColor)
		.setMipLevel(0)
		.setBaseArrayLayer(0)
		.setLayerCount(1);


	vk::BufferImageCopy region = vk::BufferImageCopy()
		.setBufferOffset(uSrcOffset)
		.setBufferRowLength(0)
		.setBufferImageHeight(0)
		.setImageSubresource(xSubresource)
		.setImageOffset({ 0,0,0 })
		.setImageExtent({ pxDst->GetWidth(), pxDst->GetHeight(), 1 });

		
		m_xCurrentCmdBuffer.copyBufferToImage(pxSrc->m_xBuffer, pxDst->m_xImage, vk::ImageLayout::eTransferDstOptimal, 1, &region);
		*/
	
}
void Zenith_Vulkan_CommandBuffer::BlitTextureToTexture(Zenith_Vulkan_Texture* pxSrc, Zenith_Vulkan_Texture* pxDst, uint32_t uDstMip)
{
	STUBBED
	/*std::array<vk::Offset3D, 2> axSrcOffsets;
	axSrcOffsets.at(0).setX(0);
	axSrcOffsets.at(0).setY(0);
	axSrcOffsets.at(0).setZ(0);
	axSrcOffsets.at(1).setX(pxSrc->GetWidth());
	axSrcOffsets.at(1).setY(pxSrc->GetHeight());
	axSrcOffsets.at(1).setZ(1);

	vk::ImageSubresourceLayers xSrcSubresource = vk::ImageSubresourceLayers()
		.setAspectMask(vk::ImageAspectFlagBits::eColor)
		.setMipLevel(0)
		.setBaseArrayLayer(0)
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
		.setBaseArrayLayer(0)
		.setLayerCount(1);

	vk::ImageBlit xBlit = vk::ImageBlit()
		.setSrcOffsets(axSrcOffsets)
		.setDstOffsets(axDstOffsets)
		.setSrcSubresource(xSrcSubresource)
		.setDstSubresource(xDstSubresource);

	m_xCurrentCmdBuffer.blitImage(pxSrc->m_xImage, vk::ImageLayout::eTransferSrcOptimal, pxDst->m_xImage, vk::ImageLayout::eTransferDstOptimal, xBlit, vk::Filter::eLinear);
	*/
}



