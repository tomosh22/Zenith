#include "Zenith.h"

// The null D3D12 memory manager's buffer initialisers touch the Flux_*Buffer
// WRAPPER types (GetBuffer / GetCBV / GetUAV / GetSRV), whose full definitions
// live in Flux_Buffers.h. That header pulls Flux.h, so it cannot be included
// from Zenith_D3D12_MemoryManager.h (which is itself reached via Flux.h's backend
// seam -> a cycle that leaves Flux_Buffer incompletely defined). This .cpp sits
// OUTSIDE that cycle, so it can include the full headers. The whole file is
// compiled only in D3D12 configs (the Vulkan source partition excludes D3D12/).
#include "Flux/Flux_Buffers.h"
#include "D3D12/Zenith_D3D12_MemoryManager.h"

// Single-buffer types expose GetBuffer(); stamp a dummy non-zero VRAM handle so
// engine validity asserts (e.g. Flux_HDR's histogram buffer) pass. No GPU
// resource is backing it.
void Zenith_D3D12_MemoryManager::InitialiseVertexBuffer(const void* pData, size_t uSize, Flux_VertexBuffer& xBufferOut, bool bDeviceLocal)
{
	(void)pData; (void)uSize; (void)bDeviceLocal;
	xBufferOut.GetBuffer().m_xVRAMHandle.SetValue(ms_uDummyHandle++);
	xBufferOut.GetBuffer().m_ulSize = uSize;
}

void Zenith_D3D12_MemoryManager::InitialiseIndexBuffer(const void* pData, size_t uSize, Flux_IndexBuffer& xBufferOut)
{
	(void)pData; (void)uSize;
	xBufferOut.GetBuffer().m_xVRAMHandle.SetValue(ms_uDummyHandle++);
	xBufferOut.GetBuffer().m_ulSize = uSize;
}

void Zenith_D3D12_MemoryManager::InitialiseConstantBuffer(const void* pData, size_t uSize, Flux_ConstantBuffer& xBufferOut)
{
	(void)pData; (void)uSize;
	xBufferOut.GetBuffer().m_xVRAMHandle.SetValue(ms_uDummyHandle++);
	xBufferOut.GetBuffer().m_ulSize = uSize;
	xBufferOut.GetCBV().m_xBufferDescHandle.SetValue(ms_uDummyHandle++);
	xBufferOut.GetCBV().m_xVRAMHandle = xBufferOut.GetBuffer().m_xVRAMHandle;
}

void Zenith_D3D12_MemoryManager::InitialiseIndirectBuffer(size_t uSize, Flux_IndirectBuffer& xBufferOut)
{
	(void)uSize;
	xBufferOut.GetBuffer().m_xVRAMHandle.SetValue(ms_uDummyHandle++);
	xBufferOut.GetBuffer().m_ulSize = uSize;
	xBufferOut.GetUAV().m_xBufferDescHandle.SetValue(ms_uDummyHandle++);
	xBufferOut.GetUAV().m_xVRAMHandle = xBufferOut.GetBuffer().m_xVRAMHandle;
}

void Zenith_D3D12_MemoryManager::InitialiseReadWriteBuffer(const void* pData, size_t uSize, Flux_ReadWriteBuffer& xBufferOut)
{
	(void)pData; (void)uSize;
	xBufferOut.GetBuffer().m_xVRAMHandle.SetValue(ms_uDummyHandle++);
	xBufferOut.GetBuffer().m_ulSize = uSize;
	xBufferOut.GetUAV().m_xBufferDescHandle.SetValue(ms_uDummyHandle++);
	xBufferOut.GetUAV().m_xVRAMHandle = xBufferOut.GetBuffer().m_xVRAMHandle;
	xBufferOut.GetSRV().m_xBufferDescHandle.SetValue(ms_uDummyHandle++);
	xBufferOut.GetSRV().m_xVRAMHandle = xBufferOut.GetBuffer().m_xVRAMHandle;
}

// Frame-indexed (Dynamic*) types: stamp every frame-in-flight buffer (they all
// expose GetBufferForFrameInFlight). Per-frame views are left default -- the
// no-op recorder never reads them.
void Zenith_D3D12_MemoryManager::InitialiseDynamicVertexBuffer(const void* pData, size_t uSize, Flux_DynamicVertexBuffer& xBufferOut, bool bDeviceLocal)
{
	(void)pData; (void)uSize; (void)bDeviceLocal;
	for (u_int u = 0; u < MAX_FRAMES_IN_FLIGHT; u++)
	{
		xBufferOut.GetBufferForFrameInFlight(u).m_xVRAMHandle.SetValue(ms_uDummyHandle++);
		xBufferOut.GetBufferForFrameInFlight(u).m_ulSize = uSize;
	}
}

void Zenith_D3D12_MemoryManager::InitialiseDynamicConstantBuffer(const void* pData, size_t uSize, Flux_DynamicConstantBuffer& xBufferOut)
{
	(void)pData; (void)uSize;
	for (u_int u = 0; u < MAX_FRAMES_IN_FLIGHT; u++)
	{
		xBufferOut.GetBufferForFrameInFlight(u).m_xVRAMHandle.SetValue(ms_uDummyHandle++);
		xBufferOut.GetBufferForFrameInFlight(u).m_ulSize = uSize;
		// Per-frame CBV (Flux_CommandBindCBV asserts the view's VRAM handle).
		xBufferOut.GetViewForFrameInFlight(u).m_xBufferDescHandle.SetValue(ms_uDummyHandle++);
		xBufferOut.GetViewForFrameInFlight(u).m_xVRAMHandle = xBufferOut.GetBufferForFrameInFlight(u).m_xVRAMHandle;
	}
}

void Zenith_D3D12_MemoryManager::InitialiseDynamicReadWriteBuffer(const void* pData, size_t uSize, Flux_DynamicReadWriteBuffer& xBufferOut)
{
	(void)pData; (void)uSize;
	for (u_int u = 0; u < MAX_FRAMES_IN_FLIGHT; u++)
	{
		xBufferOut.GetBufferForFrameInFlight(u).m_xVRAMHandle.SetValue(ms_uDummyHandle++);
		xBufferOut.GetBufferForFrameInFlight(u).m_ulSize = uSize;
		// Per-frame UAV + SRV (the binder asserts each view's VRAM handle; e.g.
		// the dynamic-lights LightBuffer is bound as an SRV_Buffer).
		xBufferOut.GetUAVForFrameInFlight(u).m_xBufferDescHandle.SetValue(ms_uDummyHandle++);
		xBufferOut.GetUAVForFrameInFlight(u).m_xVRAMHandle = xBufferOut.GetBufferForFrameInFlight(u).m_xVRAMHandle;
		xBufferOut.GetSRVForFrameInFlight(u).m_xBufferDescHandle.SetValue(ms_uDummyHandle++);
		xBufferOut.GetSRVForFrameInFlight(u).m_xVRAMHandle = xBufferOut.GetBufferForFrameInFlight(u).m_xVRAMHandle;
	}
}
