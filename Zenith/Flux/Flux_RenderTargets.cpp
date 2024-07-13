#include "Zenith.h"

#include "Flux/Flux_RenderTargets.h"

void Flux_RenderAttachmentBuilder::Build(Flux_RenderAttachment& xAttachment, RenderTargetType eType) {
	xAttachment.m_eColourFormat = m_eColourFormat;
	xAttachment.m_eDepthStencilFormat = m_eDepthStencilFormat;
	xAttachment.m_uWidth = m_uWidth;
	xAttachment.m_uHeight = m_uHeight;
	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		if (eType == RENDER_TARGET_TYPE_COLOUR) {
			xAttachment.m_axTargetTextures.push_back(Flux_Texture::CreateColourAttachment(m_uWidth, m_uHeight, m_eColourFormat, ColourFormatBitsPerPixel(m_eColourFormat)));
		}
		else if (eType == RENDER_TARGET_TYPE_DEPTHSTENCIL) {
			xAttachment.m_axTargetTextures.push_back(Flux_Texture::CreateDepthStencilAttachment(m_uWidth, m_uHeight, m_eDepthStencilFormat, DepthStencilFormatBitsPerPixel(m_eDepthStencilFormat)));
		}
	}
}