#include "Zenith.h"
#include "Flux_CommandList.h"
#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"

Flux_CommandSetVertexBuffer::Flux_CommandSetVertexBuffer(const Flux_VertexBuffer* const pxVertexBuffer, const u_int uBindPoint)
	: m_pxVertexBuffer(pxVertexBuffer)
	, m_pxDynamicVertexBuffer(nullptr)
	, m_uBindPoint(uBindPoint)
{
	Zenith_Assert(pxVertexBuffer->GetBuffer().m_xVRAMHandle.IsValid(), "Vertex buffer has invalid VRAM handle - did you forget to upload to GPU?");
}

Flux_CommandSetVertexBuffer::Flux_CommandSetVertexBuffer(const Flux_DynamicVertexBuffer* const pxDynamicVertexBuffer, const u_int uBindPoint)
	: m_pxVertexBuffer(nullptr)
	, m_pxDynamicVertexBuffer(pxDynamicVertexBuffer)
	, m_uBindPoint(uBindPoint)
{
	Zenith_Assert(pxDynamicVertexBuffer->GetBuffer().m_xVRAMHandle.IsValid(), "Dynamic vertex buffer has invalid VRAM handle - did you forget to upload to GPU?");
}

Flux_CommandSetIndexBuffer::Flux_CommandSetIndexBuffer(const Flux_IndexBuffer* const pxIndexBuffer)
	: m_pxIndexBuffer(pxIndexBuffer)
{
	Zenith_Assert(pxIndexBuffer->GetBuffer().m_xVRAMHandle.IsValid(), "Index buffer has invalid VRAM handle - did you forget to upload to GPU?");
}