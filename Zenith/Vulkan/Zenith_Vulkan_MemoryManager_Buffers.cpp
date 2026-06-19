#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#include "Zenith_Vulkan_MemoryManager.h"
#include "Zenith_Vulkan_MemoryManager_Internal.h"
#include "Zenith_Vulkan_CommandBuffer.h"

#include "Zenith_Vulkan.h"

#include "Collections/Zenith_HashMap.h"
#include "Core/Zenith_CommandLine.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_PerFrame.h"
#include "Flux/Flux_RendererImpl.h"

//=============================================================================
// Buffer VRAM: creation (vertex/index/constant/indirect/read-write,
// persistently-mapped), uploads, and destruction.
//=============================================================================

void Zenith_Vulkan_MemoryManager::InitialiseVertexBuffer(const void* pData, size_t uSize, Flux_VertexBuffer& xBufferOut, bool bDeviceLocal /*= true*/)
{
	Flux_VRAMHandle xHandle = CreateBufferVRAM(static_cast<u_int>(uSize), static_cast<MemoryFlags>(1 << MEMORY_FLAGS__VERTEX_BUFFER), bDeviceLocal ? MEMORY_RESIDENCY_GPU : MEMORY_RESIDENCY_CPU);
	xBufferOut.GetBuffer().m_xVRAMHandle = xHandle;
	xBufferOut.GetBuffer().m_ulSize = uSize;

	if (pData)
	{
		UploadBufferData(xHandle, pData, uSize);
	}
}

void Zenith_Vulkan_MemoryManager::InitialiseDynamicVertexBuffer(const void* pData, size_t uSize, Flux_DynamicVertexBuffer& xBufferOut, bool bDeviceLocal /*= true*/)
{
	for (uint32_t u = 0; u < MAX_FRAMES_IN_FLIGHT; u++)
	{
		Flux_VRAMHandle xHandle = CreateBufferVRAM(static_cast<u_int>(uSize), static_cast<MemoryFlags>(1 << MEMORY_FLAGS__VERTEX_BUFFER), bDeviceLocal ? MEMORY_RESIDENCY_GPU : MEMORY_RESIDENCY_CPU);
		xBufferOut.GetBufferForFrameInFlight(u).m_xVRAMHandle = xHandle;
		xBufferOut.GetBufferForFrameInFlight(u).m_ulSize = uSize;
		
		if (pData)
		{
			UploadBufferData(xHandle, pData, uSize);
		}
	}
}

void Zenith_Vulkan_MemoryManager::InitialiseIndexBuffer(const void* pData, size_t uSize, Flux_IndexBuffer& xBufferOut)
{
	Flux_VRAMHandle xHandle = CreateBufferVRAM(static_cast<u_int>(uSize), static_cast<MemoryFlags>(1 << MEMORY_FLAGS__INDEX_BUFFER), MEMORY_RESIDENCY_GPU);
	xBufferOut.GetBuffer().m_xVRAMHandle = xHandle;
	xBufferOut.GetBuffer().m_ulSize = uSize;
	
	if (pData)
	{
		UploadBufferData(xHandle, pData, uSize);
	}
}

void Zenith_Vulkan_MemoryManager::InitialiseConstantBuffer(const void* pData, size_t uSize, Flux_ConstantBuffer& xBufferOut)
{
	Flux_VRAMHandle xHandle = CreateBufferVRAM(static_cast<u_int>(uSize), static_cast<MemoryFlags>(1 << MEMORY_FLAGS__SHADER_READ), MEMORY_RESIDENCY_CPU);
	Flux_Buffer& xBuffer = xBufferOut.GetBuffer();
	xBuffer.m_xVRAMHandle = xHandle;
	xBuffer.m_ulSize = uSize;

	Zenith_Vulkan_VRAM* pxVRAM = m_pxVulkan->GetVRAM(xHandle);
	Zenith_Assert(pxVRAM, "Invalid buffer VRAM handle");

	vk::DescriptorBufferInfo xBufferInfo;
	xBufferInfo.setBuffer(pxVRAM->GetBuffer());
	xBufferInfo.setOffset(0);
	xBufferInfo.setRange(uSize);

	Flux_ConstantBufferView& xCBV = xBufferOut.GetCBV();
	xCBV.m_xBufferDescHandle = RegisterBufferDescriptor(xBufferInfo);
	xCBV.m_xVRAMHandle = xHandle;

	if (pData)
	{
		UploadBufferData(xHandle, pData, uSize);
	}
}

// Set up one frame's VRAM + descriptor for a dynamic (per-frame, host-visible)
// buffer and populate the caller's frame buffer struct. Returns the
// registered descriptor handle so callers can install it into the view types
// they expose (CBV-only for constant buffers, UAV+SRV mirror for RW buffers).
// Both variants share residency (CPU) and the same staging-buffer-free upload
// pattern; only the usage flags and view structures differ.
static Flux_BufferDescriptorHandle InitialiseDynamicBufferFrame(
	const void* pData, size_t uSize, MemoryFlags eFlags, Flux_Buffer& xBufferOut)
{
	// File-static free function — no 'this' to inject through. Recover the
	// memory-manager + Vulkan singletons once and route all member reaches
	// through these refs (the legitimate single re-entry per accessor).
	Zenith_Vulkan_MemoryManager& xSelf = g_xEngine.FluxMemory();
	Zenith_Vulkan& xVulkan = g_xEngine.FluxBackend();

	Flux_VRAMHandle xHandle = xSelf.CreateBufferVRAM(
		static_cast<u_int>(uSize), eFlags, MEMORY_RESIDENCY_CPU);
	xBufferOut.m_xVRAMHandle = xHandle;
	xBufferOut.m_ulSize = uSize;

	Zenith_Vulkan_VRAM* pxVRAM = xVulkan.GetVRAM(xHandle);
	Zenith_Assert(pxVRAM, "Invalid buffer VRAM handle");

	vk::DescriptorBufferInfo xBufferInfo;
	xBufferInfo.setBuffer(pxVRAM->GetBuffer());
	xBufferInfo.setOffset(0);
	xBufferInfo.setRange(uSize);

	const Flux_BufferDescriptorHandle xDesc = xSelf.RegisterBufferDescriptor(xBufferInfo);

	if (pData)
	{
		xSelf.UploadBufferData(xHandle, pData, uSize);
	}
	return xDesc;
}

void Zenith_Vulkan_MemoryManager::InitialiseDynamicConstantBuffer(const void* pData, size_t uSize, Flux_DynamicConstantBuffer& xBufferOut)
{
	for (uint32_t u = 0; u < MAX_FRAMES_IN_FLIGHT; u++)
	{
		Flux_Buffer& xBuffer = xBufferOut.GetBufferForFrameInFlight(u);
		const Flux_BufferDescriptorHandle xDesc = InitialiseDynamicBufferFrame(
			pData, uSize, static_cast<MemoryFlags>(1 << MEMORY_FLAGS__SHADER_READ), xBuffer);

		Flux_ConstantBufferView& xCBV = xBufferOut.GetCBVForFrameInFlight(u);
		xCBV.m_xBufferDescHandle = xDesc;
		xCBV.m_xVRAMHandle = xBuffer.m_xVRAMHandle;
	}
}

void Zenith_Vulkan_MemoryManager::InitialiseIndirectBuffer(size_t uSize, Flux_IndirectBuffer& xBufferOut)
{
	Flux_VRAMHandle xHandle = CreateBufferVRAM(static_cast<u_int>(uSize), static_cast<MemoryFlags>(1 << MEMORY_FLAGS__INDIRECT_BUFFER | 1 << MEMORY_FLAGS__UNORDERED_ACCESS), MEMORY_RESIDENCY_GPU);
	xBufferOut.GetBuffer().m_xVRAMHandle = xHandle;
	xBufferOut.GetBuffer().m_ulSize = uSize;

	Zenith_Vulkan_VRAM* pxVRAM = m_pxVulkan->GetVRAM(xHandle);
	Zenith_Assert(pxVRAM, "Invalid buffer VRAM handle");

	vk::DescriptorBufferInfo xBufferInfo;
	xBufferInfo.setBuffer(pxVRAM->GetBuffer());
	xBufferInfo.setOffset(0);
	xBufferInfo.setRange(uSize);

	Flux_UnorderedAccessView_Buffer& xUAV = xBufferOut.GetUAV();
	xUAV.m_xBufferDescHandle = RegisterBufferDescriptor(xBufferInfo);
	xUAV.m_xVRAMHandle = xHandle;
}

void Zenith_Vulkan_MemoryManager::InitialiseReadWriteBuffer(const void* pData, size_t uSize, Flux_ReadWriteBuffer& xBufferOut)
{
	Flux_VRAMHandle xHandle = CreateBufferVRAM(static_cast<u_int>(uSize), static_cast<MemoryFlags>(1 << MEMORY_FLAGS__UNORDERED_ACCESS | 1 << MEMORY_FLAGS__SHADER_READ), MEMORY_RESIDENCY_GPU);
	xBufferOut.GetBuffer().m_xVRAMHandle = xHandle;
	xBufferOut.GetBuffer().m_ulSize = uSize;

	Zenith_Vulkan_VRAM* pxVRAM = m_pxVulkan->GetVRAM(xHandle);
	Zenith_Assert(pxVRAM, "Invalid buffer VRAM handle");

	vk::DescriptorBufferInfo xBufferInfo;
	xBufferInfo.setBuffer(pxVRAM->GetBuffer());
	xBufferInfo.setOffset(0);
	xBufferInfo.setRange(uSize);

	const Flux_BufferDescriptorHandle xBufferDescHandle = RegisterBufferDescriptor(xBufferInfo);

	Flux_UnorderedAccessView_Buffer& xUAV = xBufferOut.GetUAV();
	xUAV.m_xBufferDescHandle = xBufferDescHandle;
	xUAV.m_xVRAMHandle = xHandle;

	// Mirror the same descriptor / VRAM handles into the read-only SSBO view.
	// vk::DescriptorType::eStorageBuffer is identical for StructuredBuffer<T>
	// and RWStructuredBuffer<T>; the distinct CPU view type only exists so the
	// render-graph access classifier and the shader binder can tell apart the
	// read-only and read-write paths.
	Flux_ShaderResourceView_Buffer& xSRV = xBufferOut.GetSRV();
	xSRV.m_xBufferDescHandle = xBufferDescHandle;
	xSRV.m_xVRAMHandle = xHandle;

	if (pData)
	{
		UploadBufferData(xHandle, pData, uSize);
	}
}

void Zenith_Vulkan_MemoryManager::InitialiseDynamicReadWriteBuffer(const void* pData, size_t uSize, Flux_DynamicReadWriteBuffer& xBufferOut)
{
	// CPU-resident (host-visible) to allow direct writes without staging buffer.
	// Per-frame uploads through the GPU staging buffer are unsafe because the staging
	// buffer can be overwritten by the next frame before the GPU executes the copy.
	for (uint32_t u = 0; u < MAX_FRAMES_IN_FLIGHT; u++)
	{
		Flux_Buffer& xBuffer = xBufferOut.GetBufferForFrameInFlight(u);
		const Flux_BufferDescriptorHandle xDesc = InitialiseDynamicBufferFrame(
			pData, uSize,
			static_cast<MemoryFlags>(1 << MEMORY_FLAGS__UNORDERED_ACCESS | 1 << MEMORY_FLAGS__SHADER_READ),
			xBuffer);

		Flux_UnorderedAccessView_Buffer& xUAV = xBufferOut.GetUAVForFrameInFlight(u);
		xUAV.m_xBufferDescHandle = xDesc;
		xUAV.m_xVRAMHandle = xBuffer.m_xVRAMHandle;

		// Mirror the same descriptor / VRAM handles into the read-only SSBO view.
		// vk::DescriptorType::eStorageBuffer is identical for StructuredBuffer<T>
		// and RWStructuredBuffer<T>; the distinct CPU view type only exists so the
		// render-graph access classifier and the shader binder can tell apart the
		// read-only and read-write paths.
		Flux_ShaderResourceView_Buffer& xSRV = xBufferOut.GetSRVForFrameInFlight(u);
		xSRV.m_xBufferDescHandle = xDesc;
		xSRV.m_xVRAMHandle = xBuffer.m_xVRAMHandle;
	}
}

Flux_VRAMHandle Zenith_Vulkan_MemoryManager::CreateBufferVRAM(const u_int uSize, const MemoryFlags eFlags, MemoryResidency eResidency)
{
	// Headless guard: when Flux::EarlyInitialise was skipped (Zenith_CommandLine::IsHeadless()),
	// g_xEngine.FluxMemory().m_xAllocator stays VK_NULL_HANDLE. Upstream asset-load paths still
	// invoke buffer creation; return an empty handle so they get a benign
	// invalid VRAM handle to store rather than asserting in vmaCreateBuffer.
	if (m_xAllocator == VK_NULL_HANDLE)
	{
		return Flux_VRAMHandle();
	}

	vk::BufferUsageFlags eUsageFlags = vk::BufferUsageFlagBits::eTransferDst;
	
	if (eFlags & 1 << MEMORY_FLAGS__VERTEX_BUFFER)
		eUsageFlags |= vk::BufferUsageFlagBits::eVertexBuffer;
	if (eFlags & 1 << MEMORY_FLAGS__INDEX_BUFFER)
		eUsageFlags |= vk::BufferUsageFlagBits::eIndexBuffer;
	if (eFlags & 1 << MEMORY_FLAGS__INDIRECT_BUFFER)
		eUsageFlags |= vk::BufferUsageFlagBits::eIndirectBuffer;
	if (eFlags & 1 << MEMORY_FLAGS__UNORDERED_ACCESS)
		eUsageFlags |= vk::BufferUsageFlagBits::eStorageBuffer;
	if (eFlags & 1 << MEMORY_FLAGS__SHADER_READ)
		eUsageFlags |= vk::BufferUsageFlagBits::eUniformBuffer;

	vk::BufferCreateInfo xBufferInfo = vk::BufferCreateInfo()
		.setSize(uSize)
		.setUsage(eUsageFlags)
		.setSharingMode(vk::SharingMode::eExclusive);

	VmaAllocationCreateInfo xAllocInfo = {};
	if (eResidency == MEMORY_RESIDENCY_CPU)
	{
		xAllocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		xAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
		xAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
	}
	else if (eResidency == MEMORY_RESIDENCY_GPU)
	{
		xAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
	}

	const vk::BufferCreateInfo::NativeType xBufferInfo_Native = xBufferInfo;

	VkBuffer xBuffer = VK_NULL_HANDLE;
	VmaAllocation xAllocation = VK_NULL_HANDLE;
	VkResult eResult = vmaCreateBuffer(m_xAllocator, &xBufferInfo_Native, &xAllocInfo, &xBuffer, &xAllocation, nullptr);
	// Wave9.1 (c) / mirrors WS8.3: surface a GPU-OOM (or any vmaCreateBuffer
	// failure) via the release-survivable check tier — logged for diagnosability
	// in all shipping configs — but keep the existing invalid-handle contract that
	// callers already tolerate. Do NOT promote this to a hard assert/return change.
	Zenith_Check(eResult == VK_SUCCESS, "vmaCreateBuffer failed (size=%u, result=%d) - returning invalid handle", uSize, (int)eResult);
	if (eResult != VK_SUCCESS)
	{
		// Return invalid handle on allocation failure
		return Flux_VRAMHandle();
	}

	Zenith_Vulkan_VRAM* pxVRAM = new Zenith_Vulkan_VRAM(vk::Buffer(xBuffer), xAllocation, m_xAllocator, uSize);
	Flux_VRAMHandle xHandle = m_pxVulkan->RegisterVRAM(pxVRAM);

	return xHandle;
}

Zenith_Vulkan_MemoryManager::PersistentBuffer Zenith_Vulkan_MemoryManager::CreatePersistentlyMappedBuffer(u_int uSize, vk::BufferUsageFlags eUsageFlags)
{
	PersistentBuffer xResult = {};
	xResult.m_uSize = uSize;

	// Headless guard: see CreateBufferVRAM for rationale.
	if (m_xAllocator == VK_NULL_HANDLE)
	{
		return xResult;
	}

	vk::BufferCreateInfo xBufferInfo = vk::BufferCreateInfo()
		.setSize(uSize)
		.setUsage(eUsageFlags)
		.setSharingMode(vk::SharingMode::eExclusive);

	VmaAllocationCreateInfo xAllocInfo = {};
	xAllocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
	xAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
	xAllocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

	const vk::BufferCreateInfo::NativeType xBufferInfo_Native = xBufferInfo;

	VkBuffer xBuffer = VK_NULL_HANDLE;
	VmaAllocationInfo xAllocationInfo = {};
	VkResult eResult = vmaCreateBuffer(m_xAllocator, &xBufferInfo_Native, &xAllocInfo, &xBuffer, &xResult.m_xAllocation, &xAllocationInfo);
	Zenith_Assert(eResult == VK_SUCCESS, "vmaCreateBuffer failed for persistent buffer with result %d", static_cast<int>(eResult));

	xResult.m_xBuffer = vk::Buffer(xBuffer);
	xResult.m_pMappedPtr = xAllocationInfo.pMappedData;

	Zenith_Assert(xResult.m_pMappedPtr != nullptr, "Persistent buffer mapping failed");

	return xResult;
}


void Zenith_Vulkan_MemoryManager::UploadBufferData(Flux_VRAMHandle xBufferHandle, const void* pData, size_t uSize)
{
	Zenith_Profiling::ScopeZone xProfileScope(ZENITH_PROFILE_ZONE("Vulkan Memory Manager Upload"));

	// Headless guard: see CreateBufferVRAM for rationale. With no allocator, all
	// upstream CreateBufferVRAM calls returned invalid handles, so GetVRAM would
	// hit the null-assert below — bail before that.
	if (m_xAllocator == VK_NULL_HANDLE)
	{
		return;
	}

	m_xMutex.Lock();
	const vk::Device& xDevice = m_pxVulkan->GetDevice();

	Zenith_Vulkan_VRAM* pxVRAM = m_pxVulkan->GetVRAM(xBufferHandle);
	Zenith_Assert(pxVRAM != nullptr || Zenith_CommandLine::IsHeadless(), "GetVRAM returned null in UploadBufferData");
	if (!pxVRAM)
	{
		m_xMutex.Unlock();
		return;  // Safety guard for release builds
	}
	const VmaAllocation& xAlloc = pxVRAM->GetAllocation();
	VkMemoryPropertyFlags eMemoryProps;
	vmaGetAllocationMemoryProperties(m_xAllocator, xAlloc, &eMemoryProps);

	if (eMemoryProps & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
	{
		void* pMap = nullptr;
		vmaMapMemory(m_xAllocator, xAlloc, &pMap);
		Zenith_Assert(pMap != nullptr, "Memory isn't mapped");
		memcpy(pMap, pData, uSize);
#ifdef ZENITH_ASSERT
		VkResult eResult =
#endif
		vmaFlushAllocation(m_xAllocator, xAlloc, 0, uSize);
		Zenith_Assert(eResult == VK_SUCCESS, "Failed to flush allocation");

		vmaUnmapMemory(m_xAllocator, xAlloc);
	}
	else
	{
		// If the allocation is larger than the entire staging buffer, use chunked upload
		if (uSize > g_uStagingPoolSize)
		{
			m_xMutex.Unlock();
			UploadBufferDataChunked(pxVRAM->GetBuffer(), pData, uSize);
			return;
		}

		PerFrameStaging& xStaging = CurrentStaging();
		if (xStaging.m_uNextFreeOffset + uSize > g_uStagingPoolSize)
		{
			// HandleStagingBufferFull blocks multi-seconds on GPU; release
			// the mutex so other threads' uploads aren't stalled. See
			// matching comment in UploadTextureData above.
			m_xMutex.Unlock();
			HandleStagingBufferFull();
			m_xMutex.Lock();
		}

		// Re-resolve staging slot in case HandleStagingBufferFull recycled it.
		PerFrameStaging& xStagingAfter = CurrentStaging();
		StagingMemoryAllocation xAllocation;
		xAllocation.m_eType = ALLOCATION_TYPE_BUFFER;
		xAllocation.m_xBufferMetadata.m_xBuffer = pxVRAM->GetBuffer();
		xAllocation.m_xBufferMetadata.m_uDestOffset = 0;
		xAllocation.m_uSize = uSize;
		xAllocation.m_uOffset = xStagingAfter.m_uNextFreeOffset;
		xStagingAfter.m_xAllocations.PushBack(xAllocation);

		void* pMap = VkUnwrap(xDevice.mapMemory(xStagingAfter.m_xMemory, xStagingAfter.m_uNextFreeOffset, uSize));
		memcpy(pMap, pData, uSize);
		xDevice.unmapMemory(xStagingAfter.m_xMemory);
		xStagingAfter.m_uNextFreeOffset += uSize;
		// Align to 16 bytes — see InitialiseTextureFromData for the rationale.
		// Buffer uploads are followed by texture uploads in the same staging
		// pool, so any allocator that leaves the cursor sub-16-aligned will
		// break vkCmdCopyBufferToImage for BC3/BC5/BC7.
		xStagingAfter.m_uNextFreeOffset = ALIGN(xStagingAfter.m_uNextFreeOffset, 16);
		if (xStagingAfter.m_uNextFreeOffset > xStagingAfter.m_uHighWaterMark)
			xStagingAfter.m_uHighWaterMark = xStagingAfter.m_uNextFreeOffset;
	}
	m_xMutex.Unlock();
}

void Zenith_Vulkan_MemoryManager::DestroySimpleBuffer(Flux_VRAMHandle& xHandle)
{
	if (!xHandle.IsValid())
	{
		return;
	}

	// QueueVRAMDeletion now resolves the record from the handle internally and
	// no-ops if it (and all views) are invalid, so call it unconditionally.
	QueueVRAMDeletion(xHandle);
}

void Zenith_Vulkan_MemoryManager::DestroyVertexBuffer(Flux_VertexBuffer& xBuffer)
{
	Flux_VRAMHandle xHandle = xBuffer.GetBuffer().m_xVRAMHandle;
	DestroySimpleBuffer(xHandle);
	xBuffer.Reset();
}

void Zenith_Vulkan_MemoryManager::DestroyDynamicVertexBuffer(Flux_DynamicVertexBuffer& xBuffer)
{
	for (uint32_t u = 0; u < MAX_FRAMES_IN_FLIGHT; u++)
	{
		Flux_VRAMHandle xHandle = xBuffer.GetBufferForFrameInFlight(u).m_xVRAMHandle;
		DestroySimpleBuffer(xHandle);
	}
	xBuffer.Reset();
}

void Zenith_Vulkan_MemoryManager::DestroyIndexBuffer(Flux_IndexBuffer& xBuffer)
{
	Flux_VRAMHandle xHandle = xBuffer.GetBuffer().m_xVRAMHandle;
	DestroySimpleBuffer(xHandle);
	xBuffer.Reset();
}

void Zenith_Vulkan_MemoryManager::DestroyConstantBuffer(Flux_ConstantBuffer& xBuffer)
{
	Flux_VRAMHandle xHandle = xBuffer.GetBuffer().m_xVRAMHandle;
	DestroySimpleBuffer(xHandle);
	xBuffer.Reset();
}

void Zenith_Vulkan_MemoryManager::DestroyDynamicConstantBuffer(Flux_DynamicConstantBuffer& xBuffer)
{
	for (uint32_t u = 0; u < MAX_FRAMES_IN_FLIGHT; u++)
	{
		Flux_VRAMHandle xHandle = xBuffer.GetBufferForFrameInFlight(u).m_xVRAMHandle;
		DestroySimpleBuffer(xHandle);
	}
	xBuffer.Reset();
}

void Zenith_Vulkan_MemoryManager::DestroyIndirectBuffer(Flux_IndirectBuffer& xBuffer)
{
	Flux_VRAMHandle xHandle = xBuffer.GetBuffer().m_xVRAMHandle;
	DestroySimpleBuffer(xHandle);
	xBuffer.Reset();
}

void Zenith_Vulkan_MemoryManager::DestroyReadWriteBuffer(Flux_ReadWriteBuffer& xBuffer)
{
	Flux_VRAMHandle xHandle = xBuffer.GetBuffer().m_xVRAMHandle;
	DestroySimpleBuffer(xHandle);
	xBuffer.Reset();
}

void Zenith_Vulkan_MemoryManager::DestroyDynamicReadWriteBuffer(Flux_DynamicReadWriteBuffer& xBuffer)
{
	for (uint32_t u = 0; u < MAX_FRAMES_IN_FLIGHT; u++)
	{
		Flux_VRAMHandle xHandle = xBuffer.GetBufferForFrameInFlight(u).m_xVRAMHandle;
		DestroySimpleBuffer(xHandle);
	}
	xBuffer.Reset();
}

void Zenith_Vulkan_MemoryManager::UploadBufferDataAtOffset(Flux_VRAMHandle xBufferHandle, const void* pData, size_t uSize, size_t uDestOffset)
{
	Zenith_Profiling::ScopeZone xProfileScope(ZENITH_PROFILE_ZONE("Vulkan Memory Manager Upload"));

	// Headless guard: see CreateBufferVRAM for rationale.
	if (m_xAllocator == VK_NULL_HANDLE)
	{
		return;
	}

	const vk::Device& xDevice = m_pxVulkan->GetDevice();
	Zenith_Vulkan_VRAM* pxVRAM = m_pxVulkan->GetVRAM(xBufferHandle);
	Zenith_Assert(pxVRAM != nullptr || Zenith_CommandLine::IsHeadless(), "GetVRAM returned null in UploadBufferDataAtOffset");
	if (!pxVRAM) return;  // Safety guard for release builds
	const VmaAllocation& xAlloc = pxVRAM->GetAllocation();
	VkMemoryPropertyFlags eMemoryProps;
	vmaGetAllocationMemoryProperties(m_xAllocator, xAlloc, &eMemoryProps);

	// For host-visible memory, map and copy directly with offset
	if (eMemoryProps & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
	{
		m_xMutex.Lock();
		void* pMap = nullptr;
		vmaMapMemory(m_xAllocator, xAlloc, &pMap);
		Zenith_Assert(pMap != nullptr, "Memory isn't mapped");

		// Copy to the destination offset
		memcpy(static_cast<uint8_t*>(pMap) + uDestOffset, pData, uSize);

#ifdef ZENITH_ASSERT
		VkResult eResult =
#endif
		vmaFlushAllocation(m_xAllocator, xAlloc, uDestOffset, uSize);
		Zenith_Assert(eResult == VK_SUCCESS, "Failed to flush allocation");

		vmaUnmapMemory(m_xAllocator, xAlloc);
		m_xMutex.Unlock();
	}
	else
	{
		if (uSize == 0)
			return;

		const uint8_t* pSrcData = static_cast<const uint8_t*>(pData);
		size_t uRemainingSize = uSize;
		size_t uCurrentSrcOffset = 0;
		size_t uCurrentDestOffset = uDestOffset;

		while (uRemainingSize > 0)
		{
			const size_t uChunkSize = (std::min)(uRemainingSize, g_uStagingPoolSize - 4096);

			m_xMutex.Lock();

			PerFrameStaging& xStaging = CurrentStaging();
			if (xStaging.m_uNextFreeOffset + uChunkSize > g_uStagingPoolSize)
				HandleStagingBufferFull();

			// Re-resolve after the potential mid-frame flush — HandleStagingBufferFull
			// resets the slot's offset but keeps the slot identity, so the
			// reference is still valid.
			PerFrameStaging& xStagingPost = CurrentStaging();

			StagingMemoryAllocation xAllocation;
			xAllocation.m_eType = ALLOCATION_TYPE_BUFFER;
			xAllocation.m_xBufferMetadata.m_xBuffer = pxVRAM->GetBuffer();
			xAllocation.m_xBufferMetadata.m_uDestOffset = uCurrentDestOffset;
			xAllocation.m_uSize = uChunkSize;
			xAllocation.m_uOffset = xStagingPost.m_uNextFreeOffset;
			xStagingPost.m_xAllocations.PushBack(xAllocation);

			void* pMap = VkUnwrap(xDevice.mapMemory(xStagingPost.m_xMemory, xStagingPost.m_uNextFreeOffset, uChunkSize));
			memcpy(pMap, pSrcData + uCurrentSrcOffset, uChunkSize);
			xDevice.unmapMemory(xStagingPost.m_xMemory);

			xStagingPost.m_uNextFreeOffset += uChunkSize;
			// 16-byte alignment — see InitialiseTextureFromData / UploadBufferData.
			xStagingPost.m_uNextFreeOffset = ALIGN(xStagingPost.m_uNextFreeOffset, 16);
			if (xStagingPost.m_uNextFreeOffset > xStagingPost.m_uHighWaterMark)
				xStagingPost.m_uHighWaterMark = xStagingPost.m_uNextFreeOffset;

			m_xMutex.Unlock();

			uCurrentSrcOffset += uChunkSize;
			uCurrentDestOffset += uChunkSize;
			uRemainingSize -= uChunkSize;
		}
	}
}
