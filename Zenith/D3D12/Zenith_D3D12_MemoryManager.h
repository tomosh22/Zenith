#pragma once

#include "Zenith.h"            // u_int / u_int64 / Zenith_Assert (defined before Flux.h in the PCH)
#include "Flux/Flux_Types.h"   // handles, enums, view structs, Flux_BindingSlot/SubresourceRange
#include "Flux/Flux_Fwd.h"     // the Flux_* aliases + forward decls of the other D3D12 classes

// ============================================================================
// NO-OP "null" D3D12 memory-manager backend.
//
// Mirrors Zenith_Vulkan_MemoryManager (the Flux_MemoryManager alias under
// ZENITH_D3D12). Its ONLY job is to COMPILE + LINK against the backend-neutral
// Flux surface and satisfy the conformance concepts:
//   - FluxBackendMemoryAlloc      (Flux_Concept_MemoryAlloc.h)
//   - FluxBackendTransientAliasing (Flux_Concept_MemoryAlloc.h, dual-positive)
//   - FluxBackendMemoryDelete     (Flux_Concept_MemoryDelete.h)
//
// Every method is an INLINE no-op stub. ZERO real rendering / allocation.
// No Vulkan / VMA headers; no vk:: references anywhere.
//
// Methods on the Vulkan class whose signatures mention vk::* / VkXxx /
// VmaXxx / Zenith_Vulkan_CommandBuffer are backend-internal and the engine
// never calls them on the neutral alias, so they are intentionally OMITTED
// here (see summary):
//   ImageTransitionBarrier, CreatePersistentlyMappedBuffer (+PersistentBuffer),
//   RegisterImageView, GetImageView, RegisterBufferDescriptor,
//   GetBufferDescriptor, GetVMAAllocator, DetermineImageViewType,
//   GetCommandBuffer.
// ============================================================================
class Zenith_D3D12_MemoryManager
{
public:

	Zenith_D3D12_MemoryManager() {}
	~Zenith_D3D12_MemoryManager() {}
	Zenith_D3D12_MemoryManager(const Zenith_D3D12_MemoryManager&) = delete;
	Zenith_D3D12_MemoryManager& operator=(const Zenith_D3D12_MemoryManager&) = delete;

	// ===== Lifecycle (FluxBackendMemoryAlloc) =====
	// The Vulkan MemoryManager registers OnFluxPerFrameEnd with Flux_RendererImpl
	// inside Initialise (Zenith_Vulkan_MemoryManager.cpp ~line 179). The null
	// backend has no per-frame deferred-deletion work to drive, so Initialise()
	// is a pure no-op and the callback registration is intentionally omitted.
	void Initialise() { }
	void Shutdown() { }

	void BeginFrame() { }
	void EndFrame(bool bDefer = true) { (void)bDefer; }

	// ===== Transient aliasing (FluxBackendTransientAliasing) =====
	// Null backend does NOT support real aliasing; the render graph falls back
	// to standalone allocation (gated on SupportsTransientAliasing()).
	bool SupportsTransientAliasing() { return false; }

	enum class AliasPoolMemoryKind : u_int
	{
		DeviceLocal,
		HostVisibleCoherent,
	};

	Flux_VRAMHandle CreateAliasPoolVRAM(u_int64 ulSize, u_int64 ulAlignment,
		AliasPoolMemoryKind eMemoryKind = AliasPoolMemoryKind::DeviceLocal)
	{
		(void)ulSize; (void)ulAlignment; (void)eMemoryKind;
		return Flux_VRAMHandle{}; // default = invalid
	}

	void ProbeImageMemoryRequirements(const Flux_SurfaceInfo& xInfo,
		u_int64& ulSizeOut,
		u_int64& ulAlignmentOut)
	{
		(void)xInfo;
		ulSizeOut = 0;
		ulAlignmentOut = 0;
	}

	Flux_VRAMHandle CreateAliasedImageVRAM(const Flux_SurfaceInfo& xInfo,
		Flux_VRAMHandle xPoolHandle,
		u_int64 ulOffsetInPool)
	{
		(void)xInfo; (void)xPoolHandle; (void)ulOffsetInPool;
		return Flux_VRAMHandle{}; // default = invalid
	}

	// Per-frame end callback (neutral signature: u_int + void*). No-op.
	static void OnFluxPerFrameEnd(u_int uRingIndex, void* pUserData) { (void)uRingIndex; (void)pUserData; }

	// ===== Wrapped buffer initialisers (FluxBackendMemoryAlloc) =====
	void InitialiseVertexBuffer(const void* pData, size_t uSize, Flux_VertexBuffer& xBufferOut, bool bDeviceLocal = true) { (void)pData; (void)uSize; (void)xBufferOut; (void)bDeviceLocal; }
	void InitialiseDynamicVertexBuffer(const void* pData, size_t uSize, Flux_DynamicVertexBuffer& xBufferOut, bool bDeviceLocal = true) { (void)pData; (void)uSize; (void)xBufferOut; (void)bDeviceLocal; }
	void InitialiseIndexBuffer(const void* pData, size_t uSize, Flux_IndexBuffer& xBufferOut) { (void)pData; (void)uSize; (void)xBufferOut; }
	void InitialiseConstantBuffer(const void* pData, size_t uSize, Flux_ConstantBuffer& xBufferOut) { (void)pData; (void)uSize; (void)xBufferOut; }
	void InitialiseDynamicConstantBuffer(const void* pData, size_t uSize, Flux_DynamicConstantBuffer& xBufferOut) { (void)pData; (void)uSize; (void)xBufferOut; }
	void InitialiseIndirectBuffer(size_t uSize, Flux_IndirectBuffer& xBufferOut) { (void)uSize; (void)xBufferOut; }
	void InitialiseReadWriteBuffer(const void* pData, size_t uSize, Flux_ReadWriteBuffer& xBufferOut) { (void)pData; (void)uSize; (void)xBufferOut; }
	void InitialiseDynamicReadWriteBuffer(const void* pData, size_t uSize, Flux_DynamicReadWriteBuffer& xBufferOut) { (void)pData; (void)uSize; (void)xBufferOut; }

	// ===== Upload paths (FluxBackendMemoryAlloc) =====
	void UploadBufferData(Flux_VRAMHandle xBufferHandle, const void* pData, size_t uSize) { (void)xBufferHandle; (void)pData; (void)uSize; }
	void UploadBufferDataAtOffset(Flux_VRAMHandle xBufferHandle, const void* pData, size_t uSize, size_t uDestOffset) { (void)xBufferHandle; (void)pData; (void)uSize; (void)uDestOffset; }

	// ===== Buffer destruction (FluxBackendMemoryDelete) =====
	void DestroyVertexBuffer(Flux_VertexBuffer& xBuffer) { (void)xBuffer; }
	void DestroyDynamicVertexBuffer(Flux_DynamicVertexBuffer& xBuffer) { (void)xBuffer; }
	void DestroyIndexBuffer(Flux_IndexBuffer& xBuffer) { (void)xBuffer; }
	void DestroyConstantBuffer(Flux_ConstantBuffer& xBuffer) { (void)xBuffer; }
	void DestroyDynamicConstantBuffer(Flux_DynamicConstantBuffer& xBuffer) { (void)xBuffer; }
	void DestroyIndirectBuffer(Flux_IndirectBuffer& xBuffer) { (void)xBuffer; }
	void DestroyReadWriteBuffer(Flux_ReadWriteBuffer& xBuffer) { (void)xBuffer; }
	void DestroyDynamicReadWriteBuffer(Flux_DynamicReadWriteBuffer& xBuffer) { (void)xBuffer; }

	// ===== VRAM allocation (FluxBackendMemoryAlloc) =====
	Flux_VRAMHandle CreateBufferVRAM(const u_int uSize, const MemoryFlags eFlags, MemoryResidency eResidency)
	{
		(void)uSize; (void)eFlags; (void)eResidency;
		Flux_VRAMHandle x; x.SetValue(ms_uDummyHandle++); return x;
	}
	Flux_VRAMHandle CreateTextureVRAM(const void* pData, const Flux_SurfaceInfo& xInfo, bool bCreateMips)
	{
		(void)pData; (void)xInfo; (void)bCreateMips;
		Flux_VRAMHandle x; x.SetValue(ms_uDummyHandle++); return x;
	}
	Flux_VRAMHandle CreateRenderTargetVRAM(const Flux_SurfaceInfo& xInfo)
	{
		(void)xInfo;
		Flux_VRAMHandle x; x.SetValue(ms_uDummyHandle++); return x;
	}

	// ===== View creation (FluxBackendMemoryAlloc) =====
	Flux_RenderTargetView CreateRenderTargetView(Flux_VRAMHandle xVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uMipLevel) { (void)xVRAMHandle; (void)xInfo; (void)uMipLevel; return {}; }
	Flux_RenderTargetView CreateRenderTargetViewForLayer(Flux_VRAMHandle xVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uLayer, uint32_t uMipLevel) { (void)xVRAMHandle; (void)xInfo; (void)uLayer; (void)uMipLevel; return {}; }
	Flux_DepthStencilView CreateDepthStencilView(Flux_VRAMHandle xVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uMipLevel) { (void)xVRAMHandle; (void)xInfo; (void)uMipLevel; return {}; }
	Flux_ShaderResourceView CreateShaderResourceView(Flux_VRAMHandle xVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uBaseMip = 0, uint32_t uMipCount = 1) { (void)xVRAMHandle; (void)xInfo; (void)uBaseMip; (void)uMipCount; return {}; }
	Flux_ShaderResourceView CreateShaderResourceViewForLayer(Flux_VRAMHandle xVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uLayer, uint32_t uBaseMip = 0, uint32_t uMipCount = 1) { (void)xVRAMHandle; (void)xInfo; (void)uLayer; (void)uBaseMip; (void)uMipCount; return {}; }
	Flux_UnorderedAccessView_Texture CreateUnorderedAccessView(Flux_VRAMHandle xVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uMipLevel) { (void)xVRAMHandle; (void)xInfo; (void)uMipLevel; return {}; }
	Flux_UnorderedAccessView_Texture CreateUnorderedAccessViewForSlice(Flux_VRAMHandle xVRAMHandle, const Flux_SurfaceInfo& xInfo, uint32_t uSlice, uint32_t uMipLevel) { (void)xVRAMHandle; (void)xInfo; (void)uSlice; (void)uMipLevel; return {}; }

	// ===== Handle release (neutral; counterparts of the vk:: register/get pair) =====
	void ReleaseImageViewHandle(Flux_ImageViewHandle xHandle) { (void)xHandle; }
	void ReleaseBufferDescriptorHandle(Flux_BufferDescriptorHandle xHandle) { (void)xHandle; }

	// ===== Deferred deletion (FluxBackendMemoryDelete) =====
	void QueueVRAMDeletion(Flux_VRAMHandle& xHandle,
		Flux_ImageViewHandle xRTV = Flux_ImageViewHandle(), Flux_ImageViewHandle xDSV = Flux_ImageViewHandle(),
		Flux_ImageViewHandle xSRV = Flux_ImageViewHandle(), Flux_ImageViewHandle xUAV = Flux_ImageViewHandle(),
		u_int uExtraFrameDelay = 0)
	{
		(void)xRTV; (void)xDSV; (void)xSRV; (void)xUAV; (void)uExtraFrameDelay;
		// Human-enforced invariant (see Flux_Concept_MemoryDelete.h): take the
		// handle by non-const reference and invalidate it to prevent double-free.
		xHandle = Flux_VRAMHandle{};
	}
	void QueueImageViewDeletion(Flux_ImageViewHandle xImageViewHandle) { (void)xImageViewHandle; }
	void ProcessDeferredDeletions() { }

	void FlushStagingBuffer() { }

	// ===== Memory accounting (neutral) =====
	void IncreaseImageMemoryUsage(u_int64 ulSize) { (void)ulSize; }
	void DecreaseImageMemoryUsage(u_int64 ulSize) { (void)ulSize; }
	const u_int64* GetImageMemoryUsagePtr() { return &m_ulImageMemoryUsed; }

	void IncreaseBufferMemoryUsage(u_int64 ulSize) { (void)ulSize; }
	void DecreaseBufferMemoryUsage(u_int64 ulSize) { (void)ulSize; }
	const u_int64* GetBufferMemoryUsagePtr() { return &m_ulBufferMemoryUsed; }

	void IncreaseMemoryUsage(u_int64 ulSize) { (void)ulSize; }
	void DecreaseMemoryUsage(u_int64 ulSize) { (void)ulSize; }
	const u_int64* GetMemoryUsagePtr() { return &m_ulMemoryUsed; }

	// VMA statistics (neutral struct — null backend reports zeroes).
	struct VMAStats
	{
		u_int64 m_ulTotalAllocatedBytes;
		u_int64 m_ulTotalUsedBytes;
		u_int64 m_ulAllocationCount;
	};
	VMAStats GetVMAStats() { return {}; }

private:
	static inline u_int ms_uDummyHandle = 1;

	// Backing storage so the Get*MemoryUsagePtr() accessors can return a valid
	// const pointer (mirrors the Vulkan members read by the editor memory panel).
	u_int64 m_ulImageMemoryUsed  = 0;
	u_int64 m_ulBufferMemoryUsed = 0;
	u_int64 m_ulMemoryUsed       = 0;
};
