#include "Zenith.h"
#include "Flux_CommandList.h"
#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"

Flux_CommandSetPipeline::Flux_CommandSetPipeline(Flux_Pipeline* pxPipeline)
	: m_pxPipeline(pxPipeline)
{
	Zenith_Assert(pxPipeline != nullptr, "Pipeline is null");
}

Flux_CommandSetVertexBuffer::Flux_CommandSetVertexBuffer(const Flux_VertexBuffer* const pxVertexBuffer, const u_int uBindPoint)
	: m_pxVertexBuffer(pxVertexBuffer)
	, m_pxDynamicVertexBuffer(nullptr)
	, m_uBindPoint(uBindPoint)
{
	Zenith_Assert(pxVertexBuffer != nullptr, "Vertex buffer is null");
	Zenith_Assert(pxVertexBuffer->GetBuffer().m_xVRAMHandle.IsValid(), "Vertex buffer has invalid VRAM handle - did you forget to upload to GPU?");
}

Flux_CommandSetVertexBuffer::Flux_CommandSetVertexBuffer(const Flux_DynamicVertexBuffer* const pxDynamicVertexBuffer, const u_int uBindPoint)
	: m_pxVertexBuffer(nullptr)
	, m_pxDynamicVertexBuffer(pxDynamicVertexBuffer)
	, m_uBindPoint(uBindPoint)
{
	Zenith_Assert(pxDynamicVertexBuffer != nullptr, "Dynamic vertex buffer is null");
	Zenith_Assert(pxDynamicVertexBuffer->GetBuffer().m_xVRAMHandle.IsValid(), "Dynamic vertex buffer has invalid VRAM handle - did you forget to upload to GPU?");
}

Flux_CommandSetIndexBuffer::Flux_CommandSetIndexBuffer(const Flux_IndexBuffer* const pxIndexBuffer)
	: m_pxIndexBuffer(pxIndexBuffer)
{
	Zenith_Assert(pxIndexBuffer != nullptr, "Index buffer is null");
	Zenith_Assert(pxIndexBuffer->GetBuffer().m_xVRAMHandle.IsValid(), "Index buffer has invalid VRAM handle - did you forget to upload to GPU?");
}

Flux_CommandBindCBV::Flux_CommandBindCBV(const Flux_ConstantBufferView* pxCBV, const u_int uBindPoint)
	: m_pxCBV(pxCBV)
	, m_uBindPoint(uBindPoint)
{
	Zenith_Assert(pxCBV != nullptr, "CBV is null");
	Zenith_Assert(pxCBV->m_xVRAMHandle.IsValid(), "CBV has invalid VRAM handle");
}

Flux_CommandBindSRV::Flux_CommandBindSRV(const Flux_ShaderResourceView* const pxSRV, const u_int uBindPoint, Flux_Sampler* pxSampler)
	: m_pxSRV(pxSRV)
	, m_uBindPoint(uBindPoint)
	, m_pxSampler(pxSampler)
{
	Zenith_Assert(pxSRV != nullptr, "SRV is null");
	Zenith_Assert(pxSRV->m_xVRAMHandle.IsValid(), "SRV has invalid VRAM handle");
}

Flux_CommandBindUAV_Texture::Flux_CommandBindUAV_Texture(const Flux_UnorderedAccessView_Texture* const pxUAV, const u_int uBindPoint)
	: m_pxUAV(pxUAV)
	, m_uBindPoint(uBindPoint)
{
	Zenith_Assert(pxUAV != nullptr, "UAV texture is null");
	Zenith_Assert(pxUAV->m_xVRAMHandle.IsValid(), "UAV texture has invalid VRAM handle");
}

Flux_CommandBindUAV_Buffer::Flux_CommandBindUAV_Buffer(const Flux_UnorderedAccessView_Buffer* const pxUAV, const u_int uBindPoint)
	: m_pxUAV(pxUAV)
	, m_uBindPoint(uBindPoint)
{
	Zenith_Assert(pxUAV != nullptr, "UAV buffer is null");
	Zenith_Assert(pxUAV->m_xVRAMHandle.IsValid(), "UAV buffer has invalid VRAM handle");
}

Flux_CommandDrawIndexedIndirect::Flux_CommandDrawIndexedIndirect(const Flux_IndirectBuffer* pxIndirectBuffer, u_int uDrawCount, u_int uOffset, u_int uStride)
	: m_pxIndirectBuffer(pxIndirectBuffer)
	, m_uDrawCount(uDrawCount)
	, m_uOffset(uOffset)
	, m_uStride(uStride)
{
	Zenith_Assert(pxIndirectBuffer != nullptr, "Indirect buffer is null");
	Zenith_Assert(pxIndirectBuffer->GetBuffer().m_xVRAMHandle.IsValid(), "Indirect buffer has invalid VRAM handle");
}

Flux_CommandDrawIndexedIndirectCount::Flux_CommandDrawIndexedIndirectCount(const Flux_IndirectBuffer* pxIndirectBuffer, const Flux_IndirectBuffer* pxCountBuffer, u_int uMaxDrawCount, u_int uIndirectOffset, u_int uCountOffset, u_int uStride)
	: m_pxIndirectBuffer(pxIndirectBuffer)
	, m_pxCountBuffer(pxCountBuffer)
	, m_uMaxDrawCount(uMaxDrawCount)
	, m_uIndirectOffset(uIndirectOffset)
	, m_uCountOffset(uCountOffset)
	, m_uStride(uStride)
{
	Zenith_Assert(pxIndirectBuffer != nullptr, "Indirect buffer is null");
	Zenith_Assert(pxIndirectBuffer->GetBuffer().m_xVRAMHandle.IsValid(), "Indirect buffer has invalid VRAM handle");
	Zenith_Assert(pxCountBuffer != nullptr, "Count buffer is null");
	Zenith_Assert(pxCountBuffer->GetBuffer().m_xVRAMHandle.IsValid(), "Count buffer has invalid VRAM handle");
}

Flux_CommandBindComputePipeline::Flux_CommandBindComputePipeline(Flux_Pipeline* pxPipeline)
	: m_pxPipeline(pxPipeline)
{
	Zenith_Assert(pxPipeline != nullptr, "Compute pipeline is null");
}