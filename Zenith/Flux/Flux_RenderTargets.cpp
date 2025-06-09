#include "Zenith.h"

#include "Flux/Flux_RenderTargets.h"

#include "AssetHandling/Zenith_AssetHandler.h"

void Flux_RenderAttachmentBuilder::Build(Flux_RenderAttachment& xAttachment, RenderTargetType eType, const std::string& strName)
{
	xAttachment.m_eColourFormat = m_eColourFormat;
	xAttachment.m_eDepthStencilFormat = m_eDepthStencilFormat;
	xAttachment.m_uWidth = m_uWidth;
	xAttachment.m_uHeight = m_uHeight;
	if (eType == RENDER_TARGET_TYPE_COLOUR)
	{
		xAttachment.m_pxTargetTexture = Zenith_AssetHandler::CreateColourAttachment(strName, m_uWidth, m_uHeight, m_eColourFormat, ColourFormatBitsPerPixel(m_eColourFormat));
	}
	else if (eType == RENDER_TARGET_TYPE_DEPTHSTENCIL)
	{
		xAttachment.m_pxTargetTexture = Zenith_AssetHandler::CreateDepthStencilAttachment(strName, m_uWidth, m_uHeight, m_eDepthStencilFormat, DepthStencilFormatBitsPerPixel(m_eDepthStencilFormat));
	}
	
}

void Flux_TargetSetup::AssignDepthStencil(Flux_RenderAttachment* pxDS)
{
	m_pxDepthStencil = pxDS;
}

const uint32_t Flux_TargetSetup::GetNumColourAttachments()
{
	uint32_t uRet = 0;
	for (Flux_RenderAttachment& xTarget : m_axColourAttachments)
	{
		if (xTarget.m_eColourFormat != COLOUR_FORMAT_NONE)
		{
			uRet++;
		}
		else
		{
			break;
		}
	}
	return uRet;
}