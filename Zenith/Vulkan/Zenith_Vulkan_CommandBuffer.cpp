#include "Zenith.h"

#include "Zenith_Vulkan_CommandBuffer.h"

#include "Zenith_Vulkan.h"
#include "Flux/Flux.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"

//#TO purely for the static assert in SetIndexBuffer
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"

#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#endif

void Zenith_Vulkan_CommandBuffer::Initialise(CommandType eType /*= COMMANDTYPE_GRAPHICS*/)
{
	m_eCommandType = eType;
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();
	vk::CommandBufferAllocateInfo xAllocInfo{};
	xAllocInfo.commandPool = Zenith_Vulkan::GetCommandPool(eType);
	xAllocInfo.level = vk::CommandBufferLevel::ePrimary;
	xAllocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
	m_xCmdBuffers = VkUnwrap(xDevice.allocateCommandBuffers(xAllocInfo));
}

void Zenith_Vulkan_CommandBuffer::InitialiseWithCustomPool(const vk::CommandPool& xCustomPool, u_int uWorkerIndex, CommandType eType /*= COMMANDTYPE_GRAPHICS*/)
{
	m_eCommandType = eType;
	m_uWorkerIndex = uWorkerIndex;
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();
	vk::CommandBufferAllocateInfo xAllocInfo{};
	xAllocInfo.commandPool = xCustomPool;
	xAllocInfo.level = vk::CommandBufferLevel::ePrimary;
	xAllocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
	m_xCmdBuffers = VkUnwrap(xDevice.allocateCommandBuffers(xAllocInfo));
}

void Zenith_Vulkan_CommandBuffer::BeginRecording()
{
	const u_int uFrameIndex = Zenith_Vulkan_Swapchain::GetCurrentFrameIndex();
	Zenith_Assert(uFrameIndex < MAX_FRAMES_IN_FLIGHT, "Frame index out of range: %u", uFrameIndex);
	Zenith_Assert(!m_xCmdBuffers.empty() && uFrameIndex < m_xCmdBuffers.size(), "Command buffers not allocated or frame index out of range");
	
	m_xCurrentCmdBuffer = m_xCmdBuffers[uFrameIndex];
	vk::CommandBufferBeginInfo xBeginInfo;
	xBeginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
	VkCheck(m_xCurrentCmdBuffer.begin(xBeginInfo));
	m_uCurrentBindFreq = FLUX_MAX_BINDING_GROUPS;
	m_uDescriptorDirty = ~0u;
	m_pxCurrentPipeline = nullptr;  // Reset pipeline to catch any stale access
	memset(m_xBindings, 0, sizeof(m_xBindings));
	
	// Clear descriptor set cache for this frame (descriptor pool gets reset per frame anyway)
	// CRITICAL: Must also clear the bindings to avoid false cache hits with stale pointers
	// If old cached bindings happened to match new bindings (same pointer addresses after
	// scene reload), we could return a stale descriptor set
	for (u_int i = 0; i < FLUX_MAX_BINDING_GROUPS; i++)
	{
		m_axDescriptorSetCache[i].descriptorSet = VK_NULL_HANDLE;
		m_axDescriptorSetCache[i].layout = VK_NULL_HANDLE;
		memset(&m_axDescriptorSetCache[i].bindings, 0, sizeof(DescSetBindings));
	}
}

void Zenith_Vulkan_CommandBuffer::EndRenderPass()
{
	m_xCurrentCmdBuffer.endRenderPass();
	m_xCurrentRenderPass = VK_NULL_HANDLE;
}
void Zenith_Vulkan_CommandBuffer::EndRecording(bool bEndPass /*= true*/)
{
	if (bEndPass)
	{
		m_xCurrentCmdBuffer.endRenderPass();
		m_xCurrentRenderPass = VK_NULL_HANDLE;
	}
	VkCheck(m_xCurrentCmdBuffer.end());
	m_uCurrentBindFreq = FLUX_MAX_BINDING_GROUPS;
}

void Zenith_Vulkan_CommandBuffer::EndAndCpuWait(bool bEndPass)
{
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();

	if (bEndPass)
	{
		m_xCurrentCmdBuffer.endRenderPass();
		m_xCurrentRenderPass = VK_NULL_HANDLE;
	}
	VkCheck(m_xCurrentCmdBuffer.end());

	vk::PipelineStageFlags eWaitStages = vk::PipelineStageFlagBits::eTransfer;

	vk::SubmitInfo xSubmitInfo = vk::SubmitInfo()
		.setCommandBufferCount(1)
		.setPCommandBuffers(&m_xCurrentCmdBuffer)
		.setWaitSemaphoreCount(0)
		.setSignalSemaphoreCount(0);

	vk::FenceCreateInfo fenceInfo;
	vk::Fence xFence = VkUnwrap(xDevice.createFence(fenceInfo));

	VkCheck(Zenith_Vulkan::GetQueue(m_eCommandType).submit(xSubmitInfo, xFence));

	vk::Result eResult = xDevice.waitForFences(1, &xFence, VK_TRUE, UINT64_MAX);
	Zenith_Assert(eResult == vk::Result::eSuccess, "Failed to wait for fence");
	xDevice.destroyFence(xFence);

	// Reset the command buffer so it can be recorded again
	VkCheck(m_xCurrentCmdBuffer.reset(vk::CommandBufferResetFlags()));
}

template<typename T>
void BindVertexBufferImpl(vk::CommandBuffer& cmdBuffer, const T& xVertexBuffer, uint32_t uBindPoint)
{
	Zenith_Assert(xVertexBuffer.GetBuffer().m_xVRAMHandle.IsValid(), "Vertex buffer has invalid VRAM handle - did you forget to upload to GPU?");
	Zenith_Vulkan_VRAM* pxVRAM = Zenith_Vulkan::GetVRAM(xVertexBuffer.GetBuffer().m_xVRAMHandle);
	Zenith_Assert(pxVRAM != nullptr, "GetVRAM returned null for vertex buffer");
	if (!pxVRAM) return;  // Safety guard for release builds
	const vk::Buffer xBuffer = pxVRAM->GetBuffer();
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
	Zenith_Assert(xIndexBuffer.GetBuffer().m_xVRAMHandle.IsValid(), "Index buffer has invalid VRAM handle - did you forget to upload to GPU?");
	Zenith_Vulkan_VRAM* pxVRAM = Zenith_Vulkan::GetVRAM(xIndexBuffer.GetBuffer().m_xVRAMHandle);
	Zenith_Assert(pxVRAM != nullptr, "GetVRAM returned null for index buffer");
	if (!pxVRAM) return;  // Safety guard for release builds
	const vk::Buffer xBuffer = pxVRAM->GetBuffer();
	static_assert(std::is_same<Flux_MeshGeometry::IndexType, uint32_t>(), "#TO_TODO: stop hardcoding type");
	m_xCurrentCmdBuffer.bindIndexBuffer(xBuffer, 0, vk::IndexType::eUint32);
}

void Zenith_Vulkan_CommandBuffer::BuildDescriptorWritesForSet(
	u_int uDescSet,
	vk::DescriptorBufferInfo* axBufferInfos, u_int& uNumBufferWrites,
	vk::DescriptorImageInfo* axTexInfos, u_int& uNumTexWrites,
	vk::WriteDescriptorSet* axWrites, u_int& uNumWrites)
{
	for (u_int u = 0; u < FLUX_MAX_BINDINGS_PER_GROUP; u++)
	{
		const BindingType eType = m_pxCurrentPipeline->m_xRootSig.m_axBindingTypes[uDescSet][u];
		if (eType == BINDING_TYPE_MAX) continue;

		// SRV → combined image sampler. Depth textures must use the
		// DepthStencilReadOnlyOptimal layout, not ShaderReadOnlyOptimal.
		const Flux_ShaderResourceView* const pxSRV = m_xBindings[uDescSet].m_xSRVs[u];
		if (pxSRV)
		{
			Zenith_Assert(pxSRV->m_xImageViewHandle.IsValid(), "SRV at descSet=%u binding=%u has null image view", uDescSet, u);

			Zenith_Vulkan_Sampler* pxSampler = m_xBindings[uDescSet].m_apxSamplers[u];
			vk::ImageLayout eLayout = pxSRV->m_bIsDepthStencil
				? vk::ImageLayout::eDepthStencilReadOnlyOptimal
				: vk::ImageLayout::eShaderReadOnlyOptimal;
			axTexInfos[uNumTexWrites] = vk::DescriptorImageInfo()
				.setSampler(pxSampler ? pxSampler->GetSampler() : Flux_Graphics::s_xRepeatSampler.GetSampler())
				.setImageView(Zenith_Vulkan_MemoryManager::GetImageView(pxSRV->m_xImageViewHandle))
				.setImageLayout(eLayout);

			axWrites[uNumWrites++] = vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
				.setDstSet(m_axCurrentDescSet[uDescSet])
				.setDstBinding(u)
				.setDstArrayElement(0)
				.setDescriptorCount(1)
				.setPImageInfo(axTexInfos + uNumTexWrites++);
		}

		// UAV image → storage image (general layout).
		const Flux_UnorderedAccessView_Texture* const pxUAV_Texture = m_xBindings[uDescSet].m_xUAV_Textures[u];
		if (pxUAV_Texture)
		{
			axTexInfos[uNumTexWrites] = vk::DescriptorImageInfo()
				.setImageView(Zenith_Vulkan_MemoryManager::GetImageView(pxUAV_Texture->m_xImageViewHandle))
				.setImageLayout(vk::ImageLayout::eGeneral);

			axWrites[uNumWrites++] = vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageImage)
				.setDstSet(m_axCurrentDescSet[uDescSet])
				.setDstBinding(u)
				.setDstArrayElement(0)
				.setDescriptorCount(1)
				.setPImageInfo(axTexInfos + uNumTexWrites++);
		}

		// UAV buffer → storage buffer.
		const Flux_UnorderedAccessView_Buffer* const pxUAV_Buffer = m_xBindings[uDescSet].m_xUAV_Buffers[u];
		if (pxUAV_Buffer)
		{
			axBufferInfos[uNumBufferWrites] = Zenith_Vulkan_MemoryManager::GetBufferDescriptor(pxUAV_Buffer->m_xBufferDescHandle);
			axWrites[uNumWrites++] = vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDstSet(m_axCurrentDescSet[uDescSet])
				.setDstBinding(u)
				.setDstArrayElement(0)
				.setDescriptorCount(1)
				.setPBufferInfo(axBufferInfos + uNumBufferWrites++);
		}

		// CBV → uniform buffer (or storage buffer if the binding type says so).
		const Flux_ConstantBufferView* const pxCBV = m_xBindings[uDescSet].m_xCBVs[u];
		if (pxCBV)
		{
			axBufferInfos[uNumBufferWrites] = Zenith_Vulkan_MemoryManager::GetBufferDescriptor(pxCBV->m_xBufferDescHandle);
			vk::DescriptorType eBufferType = (eType == BINDING_TYPE_STORAGE_BUFFER)
				? vk::DescriptorType::eStorageBuffer
				: vk::DescriptorType::eUniformBuffer;
			axWrites[uNumWrites++] = vk::WriteDescriptorSet()
				.setDescriptorType(eBufferType)
				.setDstSet(m_axCurrentDescSet[uDescSet])
				.setDstBinding(u)
				.setDstArrayElement(0)
				.setDescriptorCount(1)
				.setPBufferInfo(axBufferInfos + uNumBufferWrites++);
		}
	}

	// Scratch buffer (push-constant replacement) lives outside the per-binding
	// loop because its destination binding index comes from the binding record
	// itself, not the iteration index.
	const ScratchBufferBinding& xScratch = m_xBindings[uDescSet].m_xScratchBuffer;
	if (xScratch.m_bValid)
	{
		Zenith_Vulkan_PerFrame* pxFrame = Zenith_Vulkan::s_pxCurrentFrame;
		axBufferInfos[uNumBufferWrites] = vk::DescriptorBufferInfo()
			.setBuffer(pxFrame->GetScratchBuffer())
			.setOffset(xScratch.m_uOffset)
			.setRange(xScratch.m_uSize);

		axWrites[uNumWrites++] = vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setDstSet(m_axCurrentDescSet[uDescSet])
			.setDstBinding(xScratch.m_uBinding)
			.setDstArrayElement(0)
			.setDescriptorCount(1)
			.setPBufferInfo(axBufferInfos + uNumBufferWrites++);
	}
}

void Zenith_Vulkan_CommandBuffer::UpdateDescriptorSets()
{
	Zenith_Profiling::BeginProfile(ZENITH_PROFILE_INDEX__VULKAN_UPDATE_DESCRIPTOR_SETS);
	const vk::Device& xDevice = Zenith_Vulkan::GetDevice();

	Zenith_Assert(m_uWorkerIndex < FLUX_NUM_WORKER_THREADS,
		"Invalid worker index: %u (max %u)", m_uWorkerIndex, FLUX_NUM_WORKER_THREADS);
	Zenith_Assert(m_pxCurrentPipeline, "UpdateDescriptorSets called with no pipeline set");

	const u_int uNumDescSets = m_pxCurrentPipeline->m_xRootSig.m_uNumBindingGroups;
	Zenith_Assert(uNumDescSets <= FLUX_MAX_BINDING_GROUPS,
		"Pipeline has too many descriptor sets: %u (max %u)", uNumDescSets, FLUX_MAX_BINDING_GROUPS);

	// Touch each binding slot to ensure memory is valid (catches corrupted `this`).
	for (u_int i = 0; i < uNumDescSets; i++)
	{
		volatile const void* pxCheck = &m_xBindings[i];
		(void)pxCheck;
	}

	for (u_int uDescSet = 0; uDescSet < uNumDescSets; uDescSet++)
	{
		if (Zenith_Vulkan::ShouldOnlyUpdateDirtyDescriptors() && !(m_uDescriptorDirty & (1 << uDescSet))) continue;

		vk::DescriptorSetLayout& xLayout = m_pxCurrentPipeline->m_xRootSig.m_axDescSetLayouts[uDescSet];
		DescriptorSetCacheEntry& xCacheEntry = m_axDescriptorSetCache[uDescSet];

		const bool bCacheHit = Zenith_Vulkan::ShouldUseDescSetCache() &&
			xCacheEntry.descriptorSet != VK_NULL_HANDLE &&
			xCacheEntry.layout == xLayout &&
			xCacheEntry.bindings == m_xBindings[uDescSet];

		if (bCacheHit)
		{
			m_axCurrentDescSet[uDescSet] = xCacheEntry.descriptorSet;
		}
		else
		{
			vk::DescriptorSetAllocateInfo xInfo = vk::DescriptorSetAllocateInfo()
				.setDescriptorPool(Zenith_Vulkan::GetPerFrameDescriptorPool(m_uWorkerIndex))
				.setDescriptorSetCount(1)
				.setPSetLayouts(&xLayout);
			m_axCurrentDescSet[uDescSet] = VkUnwrap(xDevice.allocateDescriptorSets(xInfo))[0];
			#ifdef ZENITH_DEBUG_VARIABLES
			Zenith_Vulkan::IncrementDescriptorSetAllocations();
			#endif

			vk::DescriptorBufferInfo axBufferInfos[FLUX_MAX_BINDINGS_PER_GROUP];
			vk::DescriptorImageInfo axTexInfos[FLUX_MAX_BINDINGS_PER_GROUP * 2]; //SRVs and UAVs
			vk::WriteDescriptorSet axWrites[FLUX_MAX_BINDINGS_PER_GROUP * 3]; //SRVs, UAVs and CBVs
			u_int uNumBufferWrites = 0;
			u_int uNumTexWrites = 0;
			u_int uNumWrites = 0;

			BuildDescriptorWritesForSet(uDescSet,
				axBufferInfos, uNumBufferWrites,
				axTexInfos, uNumTexWrites,
				axWrites, uNumWrites);

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

void Zenith_Vulkan_CommandBuffer::DrawIndexedIndirect(const Flux_IndirectBuffer* pxIndirectBuffer, uint32_t uDrawCount, uint32_t uOffset /*= 0*/, uint32_t uStride /*= 20*/)
{
	if (Zenith_Vulkan::ShouldSubmitDrawCalls())
	{
		UpdateDescriptorSets();
		Zenith_Vulkan_VRAM* pxVRAM = Zenith_Vulkan::GetVRAM(pxIndirectBuffer->GetBuffer().m_xVRAMHandle);
		Zenith_Assert(pxVRAM != nullptr, "GetVRAM returned null for indirect buffer");
		if (!pxVRAM) return;  // Safety guard for release builds
		const vk::Buffer xIndirectBuffer = pxVRAM->GetBuffer();
		m_xCurrentCmdBuffer.drawIndexedIndirect(xIndirectBuffer, uOffset, uDrawCount, uStride);
	}
}

void Zenith_Vulkan_CommandBuffer::DrawIndexedIndirectCount(const Flux_IndirectBuffer* pxIndirectBuffer, const Flux_IndirectBuffer* pxCountBuffer, uint32_t uMaxDrawCount, uint32_t uIndirectOffset /*= 0*/, uint32_t uCountOffset /*= 0*/, uint32_t uStride /*= 20*/)
{
	if (Zenith_Vulkan::ShouldSubmitDrawCalls())
	{
		UpdateDescriptorSets();
		Zenith_Vulkan_VRAM* pxIndirectVRAM = Zenith_Vulkan::GetVRAM(pxIndirectBuffer->GetBuffer().m_xVRAMHandle);
		Zenith_Vulkan_VRAM* pxCountVRAM = Zenith_Vulkan::GetVRAM(pxCountBuffer->GetBuffer().m_xVRAMHandle);
		Zenith_Assert(pxIndirectVRAM != nullptr && pxCountVRAM != nullptr, "GetVRAM returned null for indirect/count buffer");
		if (!pxIndirectVRAM || !pxCountVRAM) return;  // Safety guard for release builds
		const vk::Buffer xIndirectBuffer = pxIndirectVRAM->GetBuffer();
		const vk::Buffer xCountBuffer = pxCountVRAM->GetBuffer();
#ifdef ZENITH_ANDROID
		// Vulkan 1.2 functions aren't in Android's libvulkan.so - load dynamically
		static PFN_vkCmdDrawIndexedIndirectCount pfnDrawIndexedIndirectCount = reinterpret_cast<PFN_vkCmdDrawIndexedIndirectCount>(
			vkGetDeviceProcAddr(static_cast<VkDevice>(Zenith_Vulkan::GetDevice()), "vkCmdDrawIndexedIndirectCount"));
		Zenith_Assert(pfnDrawIndexedIndirectCount != nullptr, "vkCmdDrawIndexedIndirectCount not supported");
		pfnDrawIndexedIndirectCount(static_cast<VkCommandBuffer>(m_xCurrentCmdBuffer), static_cast<VkBuffer>(xIndirectBuffer), uIndirectOffset, static_cast<VkBuffer>(xCountBuffer), uCountOffset, uMaxDrawCount, uStride);
#else
		m_xCurrentCmdBuffer.drawIndexedIndirectCount(xIndirectBuffer, uIndirectOffset, xCountBuffer, uCountOffset, uMaxDrawCount, uStride);
#endif
	}
}

void Zenith_Vulkan_CommandBuffer::BeginRenderPass(const Flux_RenderGraph_AttachmentRef* axColourAttachments, uint32_t uNumColourAttachments, const Flux_RenderGraph_AttachmentRef& rxDepthStencil, bool bClearColour /*= false*/, bool bClearDepth /*= false*/, bool /*= false*/, bool bDepthIsReadOnly /*= false*/)
{
	// #TODO: expose DONT_CARE LoadOp as a third option. Today a pass that
	// fully overwrites its target (e.g. HDR_ToneMapping, Apply Lighting,
	// SSR RayMarch, SSAO Upsample) has to ClearTargets() because the
	// alternative is LOAD_ACTION_LOAD — which on tiled GPUs costs a bandwidth-
	// heavy tile-init read the pass doesn't need. Adding a third state to
	// Flux_RenderGraph_Pass (e.g. m_bDontCareLoad) and plumbing it here would
	// let those passes drop the clear without paying for a load.
	LoadAction eColourLoad = bClearColour ? LOAD_ACTION_CLEAR : LOAD_ACTION_LOAD;
	LoadAction eDepthStencilLoad = bClearDepth ? LOAD_ACTION_CLEAR : LOAD_ACTION_LOAD;

	TextureFormat aeColourFormats[FLUX_MAX_TARGETS] = {};
	for (uint32_t i = 0; i < uNumColourAttachments; i++)
	{
		aeColourFormats[i] = axColourAttachments[i].m_xResource.GetSurfaceInfo().m_eFormat;
	}
	const TextureFormat eDepthStencilFormat = rxDepthStencil.IsValid() ? rxDepthStencil.m_xResource.GetSurfaceInfo().m_eFormat : TEXTURE_FORMAT_NONE;

	m_xCurrentRenderPass = Zenith_Vulkan_Pipeline::TargetSetupToRenderPass(aeColourFormats, uNumColourAttachments, eDepthStencilFormat, eColourLoad, STORE_ACTION_STORE, eDepthStencilLoad, STORE_ACTION_STORE, RENDER_TARGET_USAGE_RENDERTARGET, bDepthIsReadOnly);
	Zenith_Assert(m_xCurrentRenderPass, "BeginRenderPass: TargetSetupToRenderPass returned null render pass");
	Zenith_Vulkan::s_pxCurrentFrame->DeferDestroyRenderPass(m_xCurrentRenderPass);

	// Mip-adjusted framebuffer dimensions. When a pass binds mip N of an
	// attachment (e.g. the IBL prefilter chain at mips 1..6), the framebuffer
	// extent must be the mip's own dimensions, not the base image's — otherwise
	// Vulkan rejects the render pass begin (VUID-VkRenderPassBeginInfo-framebuffer-*).
	uint32_t uWidth = 0;
	uint32_t uHeight = 0;
	if (uNumColourAttachments > 0 && axColourAttachments[0].IsValid()
		&& axColourAttachments[0].m_xResource.GetSurfaceInfo().m_eFormat != TEXTURE_FORMAT_NONE)
	{
		const Flux_SurfaceInfo& rxInfo = axColourAttachments[0].m_xResource.GetSurfaceInfo();
		const uint32_t uMip = axColourAttachments[0].m_uMip;
		uWidth  = (rxInfo.m_uWidth  >> uMip) ? (rxInfo.m_uWidth  >> uMip) : 1u;
		uHeight = (rxInfo.m_uHeight >> uMip) ? (rxInfo.m_uHeight >> uMip) : 1u;
	}
	else
	{
		Zenith_Assert(rxDepthStencil.IsValid() && rxDepthStencil.m_xResource.GetSurfaceInfo().m_eFormat != TEXTURE_FORMAT_NONE, "Target setup has no attachments");
		const Flux_SurfaceInfo& rxInfo = rxDepthStencil.m_xResource.GetSurfaceInfo();
		const uint32_t uMip = rxDepthStencil.m_uMip;
		uWidth  = (rxInfo.m_uWidth  >> uMip) ? (rxInfo.m_uWidth  >> uMip) : 1u;
		uHeight = (rxInfo.m_uHeight >> uMip) ? (rxInfo.m_uHeight >> uMip) : 1u;
	}

	vk::Framebuffer xFramebuffer = Zenith_Vulkan_Pipeline::TargetSetupToFramebuffer(axColourAttachments, uNumColourAttachments, rxDepthStencil, uWidth, uHeight, m_xCurrentRenderPass);
	Zenith_Vulkan::s_pxCurrentFrame->DeferDestroyFramebuffer(xFramebuffer);

	vk::RenderPassBeginInfo xRenderPassInfo = vk::RenderPassBeginInfo()
		.setRenderPass(m_xCurrentRenderPass)
		.setFramebuffer(xFramebuffer)
		.setRenderArea({ {0,0}, {uWidth, uHeight} });

	vk::ClearValue axClearColour[FLUX_MAX_TARGETS+1];
	if (eColourLoad == LOAD_ACTION_CLEAR || eDepthStencilLoad == LOAD_ACTION_CLEAR)
	{
		bool bHasDepth = rxDepthStencil.IsValid();
		const uint32_t uNumAttachments = bHasDepth && eDepthStencilLoad == LOAD_ACTION_CLEAR ? uNumColourAttachments + 1 : uNumColourAttachments;
		for (uint32_t i = 0; i < uNumColourAttachments; i++)
			axClearColour[i].color = vk::ClearColorValue(0.f, 0.f, 0.f, 0.f);
		axClearColour[uNumColourAttachments].depthStencil = vk::ClearDepthStencilValue(1, 0);
		xRenderPassInfo.clearValueCount = uNumAttachments;
		xRenderPassInfo.pClearValues = axClearColour;
	}

	m_xCurrentCmdBuffer.beginRenderPass(xRenderPassInfo, vk::SubpassContents::eInline);

	m_xViewport = vk::Viewport(0, 0, static_cast<float>(uWidth), static_cast<float>(uHeight), 0, 1);
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
}

void Zenith_Vulkan_CommandBuffer::BindSRV(const Flux_ShaderResourceView* pxSRV, uint32_t uBindPoint, Zenith_Vulkan_Sampler* pxSampler /*= nullptr*/)
{
	Zenith_Assert(m_uCurrentBindFreq < FLUX_MAX_BINDING_GROUPS, "Haven't called BeginBind");
	Zenith_Assert(pxSRV && pxSRV->m_xImageViewHandle.IsValid(), "Invalid SRV");
	m_uDescriptorDirty |= 1 << m_uCurrentBindFreq;
	m_xBindings[m_uCurrentBindFreq].m_xSRVs[uBindPoint] = pxSRV;
	m_xBindings[m_uCurrentBindFreq].m_apxSamplers[uBindPoint] = pxSampler;
}

void Zenith_Vulkan_CommandBuffer::BindUAV_Texture(const Flux_UnorderedAccessView_Texture* pxUAV, uint32_t uBindPoint)
{
	Zenith_Assert(m_uCurrentBindFreq < FLUX_MAX_BINDING_GROUPS, "Haven't called BeginBind");
	Zenith_Assert(pxUAV && pxUAV->m_xImageViewHandle.IsValid(), "Invalid UAV");
	m_uDescriptorDirty |= 1 << m_uCurrentBindFreq;
	m_xBindings[m_uCurrentBindFreq].m_xUAV_Textures[uBindPoint] = pxUAV;
	m_xBindings[m_uCurrentBindFreq].m_apxSamplers[uBindPoint] = nullptr;
}

void Zenith_Vulkan_CommandBuffer::BindUAV_Buffer(const Flux_UnorderedAccessView_Buffer* pxUAV, uint32_t uBindPoint)
{
	Zenith_Assert(m_uCurrentBindFreq < FLUX_MAX_BINDING_GROUPS, "Haven't called BeginBind");
	Zenith_Assert(pxUAV, "Invalid UAV");
	m_uDescriptorDirty |= 1 << m_uCurrentBindFreq;
	m_xBindings[m_uCurrentBindFreq].m_xUAV_Buffers[uBindPoint] = pxUAV;
	m_xBindings[m_uCurrentBindFreq].m_apxSamplers[uBindPoint] = nullptr;
}

void Zenith_Vulkan_CommandBuffer::BindCBV(const Flux_ConstantBufferView* pxCBV, uint32_t uBindPoint)
{
	Zenith_Assert(m_uCurrentBindFreq < FLUX_MAX_BINDING_GROUPS, "Haven't called BeginBind");
	Zenith_Assert(pxCBV, "Invalid CBV (null)");
	Zenith_Assert(uBindPoint < FLUX_MAX_BINDINGS_PER_GROUP, "Bind point out of range: %u", uBindPoint);
	// Validate the CBV has valid buffer info
	Zenith_Assert(pxCBV->m_xBufferDescHandle.IsValid(), "CBV has invalid buffer descriptor at bind point %u", uBindPoint);
	m_uDescriptorDirty |= 1 << m_uCurrentBindFreq;
	m_xBindings[m_uCurrentBindFreq].m_xCBVs[uBindPoint] = pxCBV;
}

void Zenith_Vulkan_CommandBuffer::BindAccelerationStruct(void*, uint32_t) {
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

void Zenith_Vulkan_CommandBuffer::BindDrawConstants(void* pData, size_t uSize, u_int uBinding)
{
	// Allocate from scratch buffer
	Zenith_Vulkan_PerFrame* pxFrame = Zenith_Vulkan::s_pxCurrentFrame;
	u_int uOffset = pxFrame->AllocateScratchBuffer(static_cast<u_int>(uSize), m_uWorkerIndex);

	// Copy data to scratch buffer
	void* pDst = static_cast<u_int8*>(pxFrame->GetScratchBufferMappedPtr()) + uOffset;
	memcpy(pDst, pData, uSize);

	// Stage scratch buffer binding for the current descriptor set
	// BeginBind() should have been called before this to set m_uCurrentBindFreq
	u_int uSet = m_uCurrentBindFreq;
	m_xBindings[uSet].m_xScratchBuffer.m_uOffset = uOffset;
	m_xBindings[uSet].m_xScratchBuffer.m_uSize = static_cast<u_int>(uSize);
	m_xBindings[uSet].m_xScratchBuffer.m_uBinding = uBinding;
	m_xBindings[uSet].m_xScratchBuffer.m_bValid = true;
	m_uDescriptorDirty |= 1 << uSet;
}

void Zenith_Vulkan_CommandBuffer::SetCullMode(CullMode eCullMode)
{
	vk::CullModeFlags eVkCull = vk::CullModeFlagBits::eNone;
	switch (eCullMode)
	{
	case CULL_MODE_NONE:  eVkCull = vk::CullModeFlagBits::eNone;  break;
	case CULL_MODE_FRONT: eVkCull = vk::CullModeFlagBits::eFront; break;
	case CULL_MODE_BACK:  eVkCull = vk::CullModeFlagBits::eBack;  break;
	}
	m_xCurrentCmdBuffer.setCullMode(eVkCull);
}

void Zenith_Vulkan_CommandBuffer::SetDepthBias(float fConstant, float fSlope, float fClamp)
{
	m_xCurrentCmdBuffer.setDepthBias(fConstant, fSlope, fClamp);
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

// Map an image layout to the access mask that operations using it produce or
// consume. Used to populate both src and dst access on a layout transition
// barrier — without these the sync validator (correctly) reports a hazard:
// e.g. transitioning eTransferDstOptimal → eTransferSrcOptimal without
// srcAccessMask=eTransferWrite leaves the prior copy unsynchronised against
// the layout change. Returns READ|WRITE pairs for attachment layouts because
// loadOp=LOAD reads the attachment and storeOp=STORE writes it; the validator
// will flag a barrier that allows only the write side.
static vk::AccessFlags AccessMaskForLayout(vk::ImageLayout eLayout)
{
	switch (eLayout)
	{
	case vk::ImageLayout::eUndefined:
	case vk::ImageLayout::ePresentSrcKHR:
		return vk::AccessFlagBits::eNone;
	case vk::ImageLayout::eTransferDstOptimal:
		return vk::AccessFlagBits::eTransferWrite;
	case vk::ImageLayout::eTransferSrcOptimal:
		return vk::AccessFlagBits::eTransferRead;
	case vk::ImageLayout::eColorAttachmentOptimal:
		return vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eColorAttachmentRead;
	case vk::ImageLayout::eDepthAttachmentOptimal:
	case vk::ImageLayout::eDepthStencilAttachmentOptimal:
		return vk::AccessFlagBits::eDepthStencilAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentRead;
	case vk::ImageLayout::eDepthStencilReadOnlyOptimal:
		return vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eShaderRead;
	case vk::ImageLayout::eShaderReadOnlyOptimal:
		return vk::AccessFlagBits::eShaderRead;
	case vk::ImageLayout::eGeneral:
		// UAV / storage image; could be read, written, or both. Conservative
		// choice covers the read-modify-write case and matches what the graph's
		// ResourceAccessToVulkan emits for READWRITE_UAV.
		return vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite;
	default:
		Zenith_Assert(false, "AccessMaskForLayout: unknown layout %d", (int)eLayout);
		return vk::AccessFlagBits::eNone;
	}
}

static vk::ImageMemoryBarrier CreateImageBarrier(vk::Image xImage, vk::ImageLayout eOldLayout, vk::ImageLayout eNewLayout, vk::ImageAspectFlags eAspect, uint32_t uMipLevel, uint32_t uLayer)
{
	return vk::ImageMemoryBarrier()
		.setSubresourceRange(vk::ImageSubresourceRange(eAspect, uMipLevel, 1, uLayer, 1))
		.setImage(xImage)
		.setOldLayout(eOldLayout)
		.setNewLayout(eNewLayout)
		.setSrcAccessMask(AccessMaskForLayout(eOldLayout))
		.setDstAccessMask(AccessMaskForLayout(eNewLayout));
}

void Zenith_Vulkan_CommandBuffer::ImageTransitionBarrier(vk::Image xImage, vk::ImageLayout eOldLayout, vk::ImageLayout eNewLayout, vk::ImageAspectFlags eAspect, vk::PipelineStageFlags eSrcStage, vk::PipelineStageFlags eDstStage, int uMipLevel, int uLayer)
{
	vk::ImageMemoryBarrier xMemoryBarrier = CreateImageBarrier(xImage, eOldLayout, eNewLayout, eAspect, uMipLevel, uLayer);
	m_xCurrentCmdBuffer.pipelineBarrier(eSrcStage, eDstStage, vk::DependencyFlags(), 0, nullptr, 0, nullptr, 1, &xMemoryBarrier);
}

void Zenith_Vulkan_CommandBuffer::ImageTransitionBarrierRange(vk::Image xImage,
	vk::ImageLayout eOldLayout, vk::ImageLayout eNewLayout,
	vk::ImageAspectFlags eAspect,
	vk::PipelineStageFlags eSrcStage, vk::PipelineStageFlags eDstStage,
	vk::AccessFlags eSrcAccess, vk::AccessFlags eDstAccess,
	uint32_t uBaseMip, uint32_t uMipCount,
	uint32_t uBaseLayer, uint32_t uLayerCount)
{
	vk::ImageMemoryBarrier xMemoryBarrier = vk::ImageMemoryBarrier()
		.setSubresourceRange(vk::ImageSubresourceRange(eAspect, uBaseMip, uMipCount, uBaseLayer, uLayerCount))
		.setImage(xImage)
		.setOldLayout(eOldLayout)
		.setNewLayout(eNewLayout)
		.setSrcAccessMask(eSrcAccess)
		.setDstAccessMask(eDstAccess);
	m_xCurrentCmdBuffer.pipelineBarrier(eSrcStage, eDstStage, vk::DependencyFlags(), 0, nullptr, 0, nullptr, 1, &xMemoryBarrier);
}

// Translate the engine-facing ResourceAccess enum into the Vulkan (layout,
// access mask, pipeline stage) triple required by a pipeline barrier.
// bIsDepth selects the depth variants for the depth-attachment layouts.
// Declared in Zenith_Vulkan_CommandBuffer.h so Zenith_Vulkan.cpp can reuse
// this translation for aliasing-barrier emission.
void Flux_ResourceAccessToVulkan(ResourceAccess eAccess, bool bIsDepth,
	vk::ImageLayout& eOutLayout, vk::AccessFlags& eOutAccess, vk::PipelineStageFlags& eOutStage)
{
	switch (eAccess)
	{
	case RESOURCE_ACCESS_UNDEFINED:
		eOutLayout = vk::ImageLayout::eUndefined;
		eOutAccess = {};
		eOutStage  = vk::PipelineStageFlagBits::eTopOfPipe;
		break;
	case RESOURCE_ACCESS_READ_SRV:
		eOutLayout = bIsDepth ? vk::ImageLayout::eDepthStencilReadOnlyOptimal
		                      : vk::ImageLayout::eShaderReadOnlyOptimal;
		eOutAccess = vk::AccessFlagBits::eShaderRead;
		// Sampled reads can come from either graphics or compute - match on both.
		eOutStage  = vk::PipelineStageFlagBits::eFragmentShader
		           | vk::PipelineStageFlagBits::eComputeShader;
		break;
	case RESOURCE_ACCESS_READ_DEPTH:
		eOutLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal;
		eOutAccess = vk::AccessFlagBits::eDepthStencilAttachmentRead;
		eOutStage  = vk::PipelineStageFlagBits::eEarlyFragmentTests
		           | vk::PipelineStageFlagBits::eLateFragmentTests;
		break;
	case RESOURCE_ACCESS_WRITE_RTV:
		eOutLayout = vk::ImageLayout::eColorAttachmentOptimal;
		// loadOp=LOAD reads the attachment; storeOp=STORE writes. The barrier
		// must allow both at the destination or sync-validator reports RAW.
		eOutAccess = vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eColorAttachmentRead;
		eOutStage  = vk::PipelineStageFlagBits::eColorAttachmentOutput;
		break;
	case RESOURCE_ACCESS_WRITE_DSV:
		eOutLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
		eOutAccess = vk::AccessFlagBits::eDepthStencilAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentRead;
		eOutStage  = vk::PipelineStageFlagBits::eEarlyFragmentTests
		           | vk::PipelineStageFlagBits::eLateFragmentTests;
		break;
	case RESOURCE_ACCESS_WRITE_UAV:
		eOutLayout = vk::ImageLayout::eGeneral;
		eOutAccess = vk::AccessFlagBits::eShaderWrite;
		eOutStage  = vk::PipelineStageFlagBits::eComputeShader;
		break;
	case RESOURCE_ACCESS_READWRITE_UAV:
		eOutLayout = vk::ImageLayout::eGeneral;
		eOutAccess = vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite;
		eOutStage  = vk::PipelineStageFlagBits::eComputeShader;
		break;
	default:
		Zenith_Assert(false, "ResourceAccessToVulkan: unhandled access enum %d", (int)eAccess);
		eOutLayout = vk::ImageLayout::eUndefined;
		eOutAccess = {};
		eOutStage  = vk::PipelineStageFlagBits::eTopOfPipe;
		break;
	}
}

// Internal helper: emit the actual pipeline barrier given the resolved
// VRAM handle / depth-aspect / subresource range / accesses. Both ImageTransition
// overloads (2D attachment + polymorphic GraphResource) funnel through here.
static void EmitImageTransition(Zenith_Vulkan_CommandBuffer& rxCmdBuf, vk::Image xImage, bool bIsDepth,
	uint32_t uBaseMip, uint32_t uMipCount, uint32_t uBaseLayer, uint32_t uLayerCount,
	ResourceAccess eSrcAccess, ResourceAccess eDstAccess)
{
	const vk::ImageAspectFlags eAspect = bIsDepth
		? vk::ImageAspectFlagBits::eDepth
		: vk::ImageAspectFlagBits::eColor;

	vk::ImageLayout eOldLayout, eNewLayout;
	vk::AccessFlags eSrcAccessMask, eDstAccessMask;
	vk::PipelineStageFlags eSrcStage, eDstStage;
	Flux_ResourceAccessToVulkan(eSrcAccess, bIsDepth, eOldLayout, eSrcAccessMask, eSrcStage);
	Flux_ResourceAccessToVulkan(eDstAccess, bIsDepth, eNewLayout, eDstAccessMask, eDstStage);

	// UNDEFINED source discards prior contents - the barrier is still a valid
	// layout transition and drivers do not physically discard pixels on this
	// path (tested with RenderDoc + validation). This is the safe "first touch
	// this frame" path for amortised / once-per-frame resources.
	if (eSrcAccess == RESOURCE_ACCESS_UNDEFINED)
	{
		eSrcAccessMask = {};
	}

	rxCmdBuf.ImageTransitionBarrierRange(xImage,
		eOldLayout, eNewLayout,
		eAspect,
		eSrcStage, eDstStage,
		eSrcAccessMask, eDstAccessMask,
		uBaseMip, uMipCount,
		uBaseLayer, uLayerCount);
}

void Zenith_Vulkan_CommandBuffer::ImageTransition(Flux_RenderAttachment* pxAttachment,
	uint32_t uBaseMip, uint32_t uMipCount,
	uint32_t uBaseLayer, uint32_t uLayerCount,
	ResourceAccess eSrcAccess, ResourceAccess eDstAccess)
{
	Zenith_Assert(pxAttachment != nullptr, "ImageTransition: null attachment");
	if (pxAttachment == nullptr) return;

	Zenith_Vulkan_VRAM* pxVRAM = Zenith_Vulkan::GetVRAM(pxAttachment->m_xVRAMHandle);
	Zenith_Assert(pxVRAM != nullptr, "ImageTransition: GetVRAM returned null");
	if (pxVRAM == nullptr) return;

	EmitImageTransition(*this, pxVRAM->GetImage(),
		IsDepthFormat(pxAttachment->m_xSurfaceInfo.m_eFormat),
		uBaseMip, uMipCount, uBaseLayer, uLayerCount,
		eSrcAccess, eDstAccess);
}

void Zenith_Vulkan_CommandBuffer::ImageTransition(const Flux_GraphResource& xResource,
	uint32_t uBaseMip, uint32_t uMipCount,
	uint32_t uBaseLayer, uint32_t uLayerCount,
	ResourceAccess eSrcAccess, ResourceAccess eDstAccess)
{
	Zenith_Assert(xResource.IsImageLike(), "ImageTransition (GraphResource): only image-like kinds supported");
	if (!xResource.IsImageLike()) return;

	Flux_VRAMHandle xHandle = xResource.GetVRAMHandle();
	Zenith_Assert(xHandle.IsValid(), "ImageTransition (GraphResource): invalid VRAM handle");
	if (!xHandle.IsValid()) return;

	Zenith_Vulkan_VRAM* pxVRAM = Zenith_Vulkan::GetVRAM(xHandle);
	Zenith_Assert(pxVRAM != nullptr, "ImageTransition (GraphResource): GetVRAM returned null");
	if (pxVRAM == nullptr) return;

	EmitImageTransition(*this, pxVRAM->GetImage(),
		IsDepthFormat(xResource.GetSurfaceInfo().m_eFormat),
		uBaseMip, uMipCount, uBaseLayer, uLayerCount,
		eSrcAccess, eDstAccess);
}

void Zenith_Vulkan_CommandBuffer::BufferBarrier(Flux_Buffer* pxBuffer,
	ResourceAccess eSrcAccess, ResourceAccess eDstAccess)
{
	Zenith_Assert(pxBuffer != nullptr, "BufferBarrier: null buffer");
	if (pxBuffer == nullptr) return;

	Zenith_Assert(pxBuffer->m_xVRAMHandle.IsValid(), "BufferBarrier: invalid VRAM handle");
	if (!pxBuffer->m_xVRAMHandle.IsValid()) return;

	Zenith_Vulkan_VRAM* pxVRAM = Zenith_Vulkan::GetVRAM(pxBuffer->m_xVRAMHandle);
	Zenith_Assert(pxVRAM != nullptr, "BufferBarrier: GetVRAM returned null");
	if (pxVRAM == nullptr) return;

	// Buffers have no layout — bIsDepth is irrelevant; pass false. We pull the
	// stage + access masks from the shared access translator and discard the
	// returned image layout.
	vk::ImageLayout        eOldLayoutUnused, eNewLayoutUnused;
	vk::AccessFlags        eSrcAccessMask, eDstAccessMask;
	vk::PipelineStageFlags eSrcStage, eDstStage;
	Flux_ResourceAccessToVulkan(eSrcAccess, /*bIsDepth*/ false, eOldLayoutUnused, eSrcAccessMask, eSrcStage);
	Flux_ResourceAccessToVulkan(eDstAccess, /*bIsDepth*/ false, eNewLayoutUnused, eDstAccessMask, eDstStage);

	// First-touch / undefined-source: skip the access mask. Matches the
	// EmitImageTransition behaviour and prevents the validation layer from
	// flagging an "uninitialised access bits" complaint.
	if (eSrcAccess == RESOURCE_ACCESS_UNDEFINED)
	{
		eSrcAccessMask = {};
	}

	vk::BufferMemoryBarrier xBarrier = vk::BufferMemoryBarrier()
		.setSrcAccessMask(eSrcAccessMask)
		.setDstAccessMask(eDstAccessMask)
		.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
		.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
		.setBuffer(pxVRAM->GetBuffer())
		.setOffset(0)
		.setSize(VK_WHOLE_SIZE);

	m_xCurrentCmdBuffer.pipelineBarrier(
		eSrcStage, eDstStage,
		vk::DependencyFlags{},
		0, nullptr,
		1, &xBarrier,
		0, nullptr);
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
	// All synchronisation (image layouts, UAV memory barriers, post-dispatch
	// buffer flushes) is owned by Flux_RenderGraph and emitted as prologue
	// barriers via ConsumeGraphPrologueBarriers before the pass begins. This
	// function is intentionally trivial — see Vulkan/CLAUDE.md.
	UpdateDescriptorSets();
	m_xCurrentCmdBuffer.dispatch(uGroupCountX, uGroupCountY, uGroupCountZ);
}

void Zenith_Vulkan_CommandBuffer::ImageBarrier(Flux_Texture* pxTexture, uint32_t uOldLayout, uint32_t uNewLayout)
{
	Zenith_Vulkan_VRAM* pxVRAM = Zenith_Vulkan::GetVRAM(pxTexture->m_xVRAMHandle);
	Zenith_Assert(pxVRAM != nullptr, "GetVRAM returned null for texture in ImageBarrier");
	if (!pxVRAM) return;  // Safety guard for release builds

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
		.setImage(pxVRAM->GetImage())
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

void Zenith_Vulkan_CommandBuffer::RenderImGui()
{
#ifdef ZENITH_TOOLS
	// Get ImGui draw data
	ImDrawData* pxDrawData = ImGui::GetDrawData();
	if (!pxDrawData || pxDrawData->TotalVtxCount == 0)
	{
		return;  // Nothing to render
	}

	// ImGui rendering must happen inside an active render pass
	// The render pass should have been begun by BeginRenderPass() before this is called
	Zenith_Assert(m_xCurrentRenderPass != VK_NULL_HANDLE, "ImGui rendering requires an active render pass");

	// Call ImGui's Vulkan rendering backend with raw Vulkan command buffer handle
	ImGui_ImplVulkan_RenderDrawData(pxDrawData, static_cast<VkCommandBuffer>(m_xCurrentCmdBuffer));
#endif
}

#ifdef ZENITH_DEBUG
void Zenith_Vulkan_CommandBuffer::BeginDebugMarker(const char* szName)
{
	vk::DebugUtilsLabelEXT xLabel = vk::DebugUtilsLabelEXT()
		.setPLabelName(szName)
		.setColor({ 1.0f, 1.0f, 1.0f, 1.0f });
	m_xCurrentCmdBuffer.beginDebugUtilsLabelEXT(xLabel, Zenith_Vulkan::GetDispatchLoader());
}

void Zenith_Vulkan_CommandBuffer::EndDebugMarker()
{
	m_xCurrentCmdBuffer.endDebugUtilsLabelEXT(Zenith_Vulkan::GetDispatchLoader());
}
#endif
