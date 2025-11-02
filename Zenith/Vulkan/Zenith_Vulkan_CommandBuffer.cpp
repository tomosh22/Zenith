#include "Zenith.h"

#include "Zenith_Vulkan_CommandBuffer.h"
#include "Zenith_Vulkan_Texture.h"

#include "Zenith_Vulkan.h"
#include "Zenith_Vulkan_Buffer.h"
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
	m_xCurrentCmdBuffer.begin(vk::CommandBufferBeginInfo());

	m_uCurrentBindFreq = FLUX_MAX_DESCRIPTOR_SET_LAYOUTS;

	m_uDescriptorDirty = ~0u;

	m_bIsRecording = true;
}
void Zenith_Vulkan_CommandBuffer::EndRenderPass()
{
	m_xCurrentCmdBuffer.endRenderPass();
	m_xCurrentRenderPass = VK_NULL_HANDLE;
}
void Zenith_Vulkan_CommandBuffer::EndRecording(RenderOrder eOrder, bool bEndPass /*= true*/)
{
	//#TO_TODO: should I pass in false for bEndPass if this is a child instead of this check?
	if (bEndPass)
	{
		m_xCurrentCmdBuffer.endRenderPass();
		m_xCurrentRenderPass = VK_NULL_HANDLE;
	}

	m_xCurrentCmdBuffer.end();

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

void Zenith_Vulkan_CommandBuffer::TransitionComputeResourcesBefore()
{
	// Transition all UAVs (storage images) from ShaderReadOnlyOptimal to General layout
	std::vector<vk::ImageMemoryBarrier> axBarriers;

	for (u_int uDescSet = 0; uDescSet < m_pxCurrentPipeline->m_xRootSig.m_uNumDescriptorSets; uDescSet++)
	{
		for (uint32_t i = 0; i < MAX_BINDINGS; i++)
		{
			// Check if we have a UAV bound at this slot
			Flux_UnorderedAccessView* pxUAV = m_xBindings[uDescSet].m_xUAVs[i];
			if (!pxUAV || !pxUAV->m_xImageView)
			{
				// Fallback to old storage image check for compatibility
				Zenith_Vulkan_Texture* pxTex = m_xBindings[uDescSet].m_xTextures[i].first;
				if (!pxTex || !pxTex->IsValid())
				{
					continue;
				}

				// Check if this is a storage image
				bool bIsStorageImage = (m_pxCurrentPipeline->m_xRootSig.m_axDescriptorTypes[uDescSet][i] == DESCRIPTOR_TYPE_STORAGE_IMAGE);
				
				if (!bIsStorageImage)
				{
					continue;
				}
				
				// Fallback path for legacy storage images
				vk::ImageSubresourceRange xSubRange = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);

				vk::ImageMemoryBarrier xBarrier = vk::ImageMemoryBarrier()
					.setSubresourceRange(xSubRange)
					.setImage(pxTex->GetImage())
					.setOldLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
					.setNewLayout(vk::ImageLayout::eGeneral)
					.setSrcAccessMask(vk::AccessFlagBits::eShaderRead)
					.setDstAccessMask(vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eShaderRead);

				axBarriers.push_back(xBarrier);
				continue;
			}
			else
			{
				// We have a UAV, use its VRAM handle to get the image
				Zenith_Vulkan_VRAM* pxVRAM = Zenith_Vulkan::GetVRAM(pxUAV->m_uVRAMHandle);
				if (!pxVRAM)
				{
					continue;
				}

				vk::ImageSubresourceRange xSubRange = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);

				vk::ImageMemoryBarrier xBarrier = vk::ImageMemoryBarrier()
					.setSubresourceRange(xSubRange)
					.setImage(pxVRAM->GetImage())
					.setOldLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
					.setNewLayout(vk::ImageLayout::eGeneral)
					.setSrcAccessMask(vk::AccessFlagBits::eShaderRead)
					.setDstAccessMask(vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eShaderRead);

				axBarriers.push_back(xBarrier);
				continue;
			}
		}
	}

	if (!axBarriers.empty())
	{
		m_xCurrentCmdBuffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eFragmentShader | vk::PipelineStageFlagBits::eComputeShader,
			vk::PipelineStageFlagBits::eComputeShader,
			vk::DependencyFlags(),
			0, nullptr,
			0, nullptr,
			axBarriers.size(), axBarriers.data()
		);
	}
}

void Zenith_Vulkan_CommandBuffer::TransitionComputeResourcesAfter()
{
	// Only apply automatic barriers for compute pipelines
	if (m_eCurrentBindPoint != vk::PipelineBindPoint::eCompute)
	{
		return;
	}

	// Transition all UAVs (storage images) from General back to ShaderReadOnlyOptimal layout
	std::vector<vk::ImageMemoryBarrier> axBarriers;

	for (u_int uDescSet = 0; uDescSet < m_pxCurrentPipeline->m_xRootSig.m_uNumDescriptorSets; uDescSet++)
	{
		for (uint32_t i = 0; i < MAX_BINDINGS; i++)
		{
			// Check if we have a UAV bound at this slot
			Flux_UnorderedAccessView* pxUAV = m_xBindings[uDescSet].m_xUAVs[i];
			if (!pxUAV || !pxUAV->m_xImageView)
			{
				// Fallback to old storage image check for compatibility
				Zenith_Vulkan_Texture* pxTex = m_xBindings[uDescSet].m_xTextures[i].first;
				if (!pxTex || !pxTex->IsValid())
				{
					continue;
				}

				// Check if this is a storage image
				bool bIsStorageImage = (m_pxCurrentPipeline->m_xRootSig.m_axDescriptorTypes[uDescSet][i] == DESCRIPTOR_TYPE_STORAGE_IMAGE);
				
				if (!bIsStorageImage)
				{
					continue;
				}
				
				// Fallback path for legacy storage images
				vk::ImageSubresourceRange xSubRange = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);

				vk::ImageMemoryBarrier xBarrier = vk::ImageMemoryBarrier()
					.setSubresourceRange(xSubRange)
					.setImage(pxTex->GetImage())
					.setOldLayout(vk::ImageLayout::eGeneral)
					.setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
					.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eShaderRead)
					.setDstAccessMask(vk::AccessFlagBits::eShaderRead);

				axBarriers.push_back(xBarrier);
				continue;
			}
			else
			{
				// We have a UAV, use its VRAM handle to get the image
				Zenith_Vulkan_VRAM* pxVRAM = Zenith_Vulkan::GetVRAM(pxUAV->m_uVRAMHandle);
				if (!pxVRAM)
				{
					continue;
				}

				vk::ImageSubresourceRange xSubRange = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);

				vk::ImageMemoryBarrier xBarrier = vk::ImageMemoryBarrier()
					.setSubresourceRange(xSubRange)
					.setImage(pxVRAM->GetImage())
					.setOldLayout(vk::ImageLayout::eGeneral)
					.setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
					.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eShaderRead)
					.setDstAccessMask(vk::AccessFlagBits::eShaderRead);

				axBarriers.push_back(xBarrier);
			}
		}
	}

	if (!axBarriers.empty())
	{
		m_xCurrentCmdBuffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eComputeShader,
			vk::PipelineStageFlagBits::eFragmentShader | vk::PipelineStageFlagBits::eComputeShader,
			vk::DependencyFlags(),
			0, nullptr,
			0, nullptr,
			axBarriers.size(), axBarriers.data()
		);
	}
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
				// Skip bindings that don't exist in the pipeline layout
				if (m_pxCurrentPipeline->m_xRootSig.m_axDescriptorTypes[uDescSet][i] == DESCRIPTOR_TYPE_MAX)
				{
					continue;
				}
				
				// Count textures from legacy bindings, SRV/UAV, or direct ImageView bindings
				bool bHasTexture = (m_xBindings[uDescSet].m_xTextures[i].first != nullptr);
				bool bHasSRV = (m_xBindings[uDescSet].m_xSRVs[i] != nullptr && m_xBindings[uDescSet].m_xSRVs[i]->m_xImageView);
				bool bHasUAV = (m_xBindings[uDescSet].m_xUAVs[i] != nullptr && m_xBindings[uDescSet].m_xUAVs[i]->m_xImageView);
				bool bHasImageView = (m_xBindings[uDescSet].m_xImageViews[i] != VK_NULL_HANDLE);
				
				if (bHasTexture || bHasSRV || bHasUAV || bHasImageView)
				{
					uNumTextures++;
				}
			}

			uint32_t uNumBuffers = 0;
			for (uint32_t i = 0; i < MAX_BINDINGS; i++)
			{
				// Skip bindings that don't exist in the pipeline layout
				if (m_pxCurrentPipeline->m_xRootSig.m_axDescriptorTypes[uDescSet][i] == DESCRIPTOR_TYPE_MAX)
				{
					continue;
				}
				
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
				// Skip bindings that don't exist in the pipeline layout
				if (m_pxCurrentPipeline->m_xRootSig.m_axDescriptorTypes[uDescSet][i] == DESCRIPTOR_TYPE_MAX)
				{
					continue;
				}
				
				// Check for UAV or SRV first
				Flux_UnorderedAccessView* pxUAV = m_xBindings[uDescSet].m_xUAVs[i];
				Flux_ShaderResourceView* pxSRV = m_xBindings[uDescSet].m_xSRVs[i];
				vk::ImageView xDirectImageView = m_xBindings[uDescSet].m_xImageViews[i];
				
				vk::ImageView xImageView;
				bool bIsStorageImage = false;
				Zenith_Vulkan_Sampler* pxSampler = m_xBindings[uDescSet].m_xTextures[i].second;
				
				if (pxUAV && pxUAV->m_xImageView)
				{
					// Use UAV's ImageView
					xImageView = pxUAV->m_xImageView;
					bIsStorageImage = true;
				}
				else if (pxSRV && pxSRV->m_xImageView)
				{
					// Use SRV's ImageView
					xImageView = pxSRV->m_xImageView;
				}
				else if (xDirectImageView)
				{
					// Use directly stored ImageView (from VRAM handle binding)
					xImageView = xDirectImageView;
				}
				else
				{
					// Fallback to legacy texture binding
					Zenith_Vulkan_Texture* pxTex = m_xBindings[uDescSet].m_xTextures[i].first;
					if (!pxTex)
					{
						continue;
					}
					xImageView = pxTex->GetImageView();
					// Check if this is a legacy storage image
					bIsStorageImage = (m_pxCurrentPipeline->m_xRootSig.m_axDescriptorTypes[uDescSet][i] == DESCRIPTOR_TYPE_STORAGE_IMAGE);
				}

				if (!xImageView)
				{
					continue;
				}

				vk::ImageLayout eLayout;
				vk::DescriptorType eDescType = vk::DescriptorType::eCombinedImageSampler;
				
				if (bIsStorageImage)
				{
					eLayout = vk::ImageLayout::eGeneral;
					eDescType = vk::DescriptorType::eStorageImage;
				}
				else
				{
					eLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
				}

				//#TO_TODO: different samplers
				vk::DescriptorImageInfo& xInfo = xTexInfos.at(uCount)
					.setSampler(pxSampler ? pxSampler->GetSampler() : Flux_Graphics::s_xRepeatSampler.GetSampler())
					.setImageView(xImageView)
					.setImageLayout(eLayout);

				vk::WriteDescriptorSet& xWrite = xTexWrites.at(uCount)
					.setDescriptorType(eDescType)
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
				// Skip bindings that don't exist in the pipeline layout
				if (m_pxCurrentPipeline->m_xRootSig.m_axDescriptorTypes[uDescSet][i] == DESCRIPTOR_TYPE_MAX)
				{
					continue;
				}
				
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
			
			// Use the bind point set when the pipeline was bound
			m_xCurrentCmdBuffer.bindDescriptorSets(m_eCurrentBindPoint, m_pxCurrentPipeline->m_xRootSig.m_xLayout, uDescSet, 1, &m_axCurrentDescSet[uDescSet], 0, nullptr);
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

void Zenith_Vulkan_CommandBuffer::BeginRenderPass(Flux_TargetSetup& xTargetSetup, bool bClearColour /*= false*/, bool bClearDepth /*= false*/, bool bClearStencil /*= false*/)
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
		if (xTargetSetup.m_axColourAttachments[i].m_eFormat != TEXTURE_FORMAT_NONE)
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
	if (xTargetSetup.m_axColourAttachments[0].m_eFormat != TEXTURE_FORMAT_NONE)
	{
		uWidth = xTargetSetup.m_axColourAttachments[0].m_uWidth;
		uHeight = xTargetSetup.m_axColourAttachments[0].m_uHeight;
	}
	else
	{
		Zenith_Assert(xTargetSetup.m_pxDepthStencil->m_eFormat != TEXTURE_FORMAT_NONE, "Target setup has no attachments");
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
	m_eCurrentBindPoint = vk::PipelineBindPoint::eGraphics;
	m_xCurrentCmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pxPipeline->m_xPipeline);
	m_pxCurrentPipeline = pxPipeline;
	m_uDescriptorDirty = ~0u;
	memset(m_xBindings, 0, sizeof(m_xBindings));
	memset(m_aapxTextureCache, 0, sizeof(m_aapxTextureCache));
	memset(m_aapxBufferCache, 0, sizeof(m_aapxBufferCache));

}

void Zenith_Vulkan_CommandBuffer::BindTexture(Zenith_Vulkan_Texture* pxTexture, uint32_t uBindPoint, Zenith_Vulkan_Sampler* pxSampler /*= nullptr*/)
{
	Zenith_Assert(m_uCurrentBindFreq < FLUX_MAX_DESCRIPTOR_SET_LAYOUTS, "Haven't called BeginBind");

	if (pxTexture != m_aapxTextureCache[m_uCurrentBindFreq][uBindPoint])
	{
		m_uDescriptorDirty |= 1<<m_uCurrentBindFreq;
		m_aapxTextureCache[m_uCurrentBindFreq][uBindPoint] = pxTexture;
	}
	m_xBindings[m_uCurrentBindFreq].m_xTextures[uBindPoint] = {pxTexture, pxSampler};
}

void Zenith_Vulkan_CommandBuffer::BindTextureHandle(uint32_t uVRAMHandle, uint32_t uBindPoint, Zenith_Vulkan_Sampler* pxSampler /*= nullptr*/)
{
	Zenith_Assert(m_uCurrentBindFreq < FLUX_MAX_DESCRIPTOR_SET_LAYOUTS, "Haven't called BeginBind");
	
	// Get SRV from AssetHandler
	Flux_ShaderResourceView* pxSRV = Zenith_AssetHandler::GetTextureSRVByHandle(uVRAMHandle);
	Zenith_Assert(pxSRV && pxSRV->m_xImageView, "No SRV found for texture handle");
	
	vk::ImageView xImageView = pxSRV->m_xImageView;
	m_uDescriptorDirty |= 1 << m_uCurrentBindFreq;
	
	// Store the ImageView and sampler
	m_xBindings[m_uCurrentBindFreq].m_xImageViews[uBindPoint] = xImageView;
	m_xBindings[m_uCurrentBindFreq].m_xTextures[uBindPoint] = {nullptr, pxSampler};
}

void Zenith_Vulkan_CommandBuffer::BindSRV(Flux_ShaderResourceView* pxSRV, uint32_t uBindPoint, Zenith_Vulkan_Sampler* pxSampler /*= nullptr*/)
{
	Zenith_Assert(m_uCurrentBindFreq < FLUX_MAX_DESCRIPTOR_SET_LAYOUTS, "Haven't called BeginBind");
	Zenith_Assert(pxSRV && pxSRV->m_xImageView, "Invalid SRV");

	vk::ImageView xImageView = pxSRV->m_xImageView;
	// Note: Cache comparison disabled for now since we're using ImageView directly
	// Could add ImageView caching if needed for performance
	m_uDescriptorDirty |= 1 << m_uCurrentBindFreq;
	
	m_xBindings[m_uCurrentBindFreq].m_xSRVs[uBindPoint] = pxSRV;
	// Store sampler in textures slot (will need to extract ImageView later during descriptor writes)
	m_xBindings[m_uCurrentBindFreq].m_xTextures[uBindPoint] = {nullptr, pxSampler};
}

void Zenith_Vulkan_CommandBuffer::BindUAV(Flux_UnorderedAccessView* pxUAV, uint32_t uBindPoint)
{
	Zenith_Assert(m_uCurrentBindFreq < FLUX_MAX_DESCRIPTOR_SET_LAYOUTS, "Haven't called BeginBind");
	Zenith_Assert(pxUAV && pxUAV->m_xImageView, "Invalid UAV");

	vk::ImageView xImageView = pxUAV->m_xImageView;
	// Note: Cache comparison disabled for now since we're using ImageView directly
	m_uDescriptorDirty |= 1 << m_uCurrentBindFreq;
	
	m_xBindings[m_uCurrentBindFreq].m_xUAVs[uBindPoint] = pxUAV;
	m_xBindings[m_uCurrentBindFreq].m_xTextures[uBindPoint] = {nullptr, nullptr};
}

void Zenith_Vulkan_CommandBuffer::BindRTV(Flux_RenderTargetView* pxRTV, uint32_t uBindPoint)
{
	Zenith_Assert(m_uCurrentBindFreq < FLUX_MAX_DESCRIPTOR_SET_LAYOUTS, "Haven't called BeginBind");
	Zenith_Assert(pxRTV && pxRTV->m_xImageView, "Invalid RTV");

	vk::ImageView xImageView = pxRTV->m_xImageView;
	// Note: Cache comparison disabled for now since we're using ImageView directly
	m_uDescriptorDirty |= 1 << m_uCurrentBindFreq;
	
	m_xBindings[m_uCurrentBindFreq].m_xRTVs[uBindPoint] = pxRTV;
	m_xBindings[m_uCurrentBindFreq].m_xTextures[uBindPoint] = {nullptr, nullptr};
}

void Zenith_Vulkan_CommandBuffer::BindDSV(Flux_DepthStencilView* pxDSV, uint32_t uBindPoint)
{
	Zenith_Assert(m_uCurrentBindFreq < FLUX_MAX_DESCRIPTOR_SET_LAYOUTS, "Haven't called BeginBind");
	Zenith_Assert(pxDSV && pxDSV->m_xImageView, "Invalid DSV");

	vk::ImageView xImageView = pxDSV->m_xImageView;
	// Note: Cache comparison disabled for now since we're using ImageView directly
	m_uDescriptorDirty |= 1 << m_uCurrentBindFreq;
	
	m_xBindings[m_uCurrentBindFreq].m_xDSVs[uBindPoint] = pxDSV;
	m_xBindings[m_uCurrentBindFreq].m_xTextures[uBindPoint] = {nullptr, nullptr};
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
	for (uint32_t i = 0; i < MAX_BINDINGS; i++)
	{
		m_xBindings[uDescSet].m_xBuffers[i] = nullptr;
		m_xBindings[uDescSet].m_xTextures[i] = {nullptr, nullptr};
		m_xBindings[uDescSet].m_xSRVs[i] = nullptr;
		m_xBindings[uDescSet].m_xUAVs[i] = nullptr;
		m_xBindings[uDescSet].m_xRTVs[i] = nullptr;
		m_xBindings[uDescSet].m_xDSVs[i] = nullptr;
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

// ========== COMPUTE METHODS ==========

void Zenith_Vulkan_CommandBuffer::BindComputePipeline(Zenith_Vulkan_Pipeline* pxPipeline)
{
	m_eCurrentBindPoint = vk::PipelineBindPoint::eCompute;
	m_pxCurrentPipeline = pxPipeline;
	m_xCurrentCmdBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, pxPipeline->m_xPipeline);
	m_uDescriptorDirty = ~0u;
}

void Zenith_Vulkan_CommandBuffer::BindStorageImage(Zenith_Vulkan_Texture* pxTexture, uint32_t uBindPoint)
{
	Zenith_Assert(m_uCurrentBindFreq < FLUX_MAX_DESCRIPTOR_SET_LAYOUTS, "Must call BeginBind first");
	Zenith_Assert(uBindPoint < MAX_BINDINGS, "Bind point out of range");
	
	m_xBindings[m_uCurrentBindFreq].m_xTextures[uBindPoint] = { pxTexture, nullptr };
	
	if (m_aapxTextureCache[m_uCurrentBindFreq][uBindPoint] != pxTexture)
	{
		m_aapxTextureCache[m_uCurrentBindFreq][uBindPoint] = pxTexture;
		m_uDescriptorDirty |= (1 << m_uCurrentBindFreq);
	}
}

void Zenith_Vulkan_CommandBuffer::Dispatch(uint32_t uGroupCountX, uint32_t uGroupCountY, uint32_t uGroupCountZ)
{
	// Transition compute resources to the correct layout before binding
	TransitionComputeResourcesBefore();
	PrepareDrawCallDescriptors();
	m_xCurrentCmdBuffer.dispatch(uGroupCountX, uGroupCountY, uGroupCountZ);
	TransitionComputeResourcesAfter();
}

void Zenith_Vulkan_CommandBuffer::ImageBarrier(Zenith_Vulkan_Texture* pxTexture, uint32_t uOldLayout, uint32_t uNewLayout)
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
		.setImage(pxTexture->GetImage())
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
