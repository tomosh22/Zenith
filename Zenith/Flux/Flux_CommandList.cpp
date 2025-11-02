#include "Zenith.h"
#include "Flux_CommandList.h"
#include "Flux/Flux.h"

void Flux_CommandBindTexture::operator()(Flux_CommandBuffer* pxCmdBuf)
{
	pxCmdBuf->BindTextureHandle(m_pxTexture->m_xVRAMHandle.AsUInt(), m_uBindPoint, m_pxSampler);
}
