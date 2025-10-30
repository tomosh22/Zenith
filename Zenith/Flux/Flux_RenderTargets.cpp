#include "Zenith.h"

#include "Flux/Flux_RenderTargets.h"

#include "AssetHandling/Zenith_AssetHandler.h"

void Flux_RenderAttachmentBuilder::BuildColour(Flux_RenderAttachment& xAttachment, const std::string& strName)
{
	Flux_SurfaceInfo xInfo;
	xInfo.m_uWidth = m_uWidth;
	xInfo.m_uHeight = m_uHeight;
	xInfo.m_eFormat = m_eFormat;
	xInfo.m_uNumMips = 1;
	xInfo.m_uNumLayers = 1;
	xInfo.m_uMemoryFlags = m_uMemoryFlags;


	xAttachment.m_eFormat = m_eFormat;
	xAttachment.m_uWidth = m_uWidth;
	xAttachment.m_uHeight = m_uHeight;
	xAttachment.m_pxTargetTexture = Zenith_AssetHandler::CreateColourAttachment(strName, xInfo);
	
}

void Flux_RenderAttachmentBuilder::BuildDepthStencil(Flux_RenderAttachment& xAttachment, const std::string& strName)
{
	Flux_SurfaceInfo xInfo;
	xInfo.m_uWidth = m_uWidth;
	xInfo.m_uHeight = m_uHeight;
	xInfo.m_eFormat = m_eFormat;
	xInfo.m_uNumMips = 1;
	xInfo.m_uNumLayers = 1;
	xInfo.m_uMemoryFlags = m_uMemoryFlags;

	xAttachment.m_eFormat = m_eFormat;
	xAttachment.m_uWidth = m_uWidth;
	xAttachment.m_uHeight = m_uHeight;
	xAttachment.m_pxTargetTexture = Zenith_AssetHandler::CreateDepthStencilAttachment(strName, xInfo);

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
		if (xTarget.m_eFormat != TEXTURE_FORMAT_NONE)
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