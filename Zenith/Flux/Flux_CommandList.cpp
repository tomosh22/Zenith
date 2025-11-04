#include "Zenith.h"
#include "Flux_CommandList.h"
#include "Flux/Flux.h"

void Flux_CommandBindBuffer::operator()(Flux_CommandBuffer* pxCmdBuf)
{
	pxCmdBuf->BindBuffer(m_pxBufferVRAM->m_xVRAMHandle, m_uBindPoint);
}
