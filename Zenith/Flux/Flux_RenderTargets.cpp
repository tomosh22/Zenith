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
	
	// Create views for this color attachment
	xAttachment.m_pxSRV = new Flux_ShaderResourceView();
	xAttachment.m_pxSRV->m_pxTexture = xAttachment.m_pxTargetTexture;
	xAttachment.m_pxSRV->m_eViewType = VIEW_TYPE_SRV;

	xAttachment.m_pxRTV = new Flux_RenderTargetView();
	xAttachment.m_pxRTV->m_pxTexture = xAttachment.m_pxTargetTexture;
	xAttachment.m_pxRTV->m_eViewType = VIEW_TYPE_RTV;

	// Create UAV if requested via memory flags
	if (m_uMemoryFlags & MEMORY_FLAGS__UNORDERED_ACCESS)
	{
		xAttachment.m_pxUAV = new Flux_UnorderedAccessView();
		xAttachment.m_pxUAV->m_pxTexture = xAttachment.m_pxTargetTexture;
		xAttachment.m_pxUAV->m_eViewType = VIEW_TYPE_UAV;
	}
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

	// Create views for this depth/stencil attachment
	xAttachment.m_pxSRV = new Flux_ShaderResourceView();
	xAttachment.m_pxSRV->m_pxTexture = xAttachment.m_pxTargetTexture;
	xAttachment.m_pxSRV->m_eViewType = VIEW_TYPE_SRV;

	xAttachment.m_pxDSV = new Flux_DepthStencilView();
	xAttachment.m_pxDSV->m_pxTexture = xAttachment.m_pxTargetTexture;
	xAttachment.m_pxDSV->m_eViewType = VIEW_TYPE_DSV;
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