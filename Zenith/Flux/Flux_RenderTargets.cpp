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

	// Create actual target VRAM and store it in registry
	xAttachment.m_uVRAMHandle = Zenith_AssetHandler::CreateColourAttachment(strName, xInfo);
	xAttachment.m_eFormat = m_eFormat;
	xAttachment.m_uWidth = m_uWidth;
	xAttachment.m_uHeight = m_uHeight;
	
	// Create RTV with mips
	xAttachment.m_pxRTV = new Flux_RenderTargetView();
	xAttachment.m_pxRTV->m_xImageView = Flux_MemoryManager::CreateRenderTargetView(xAttachment.m_uVRAMHandle, xInfo, 0);
	xAttachment.m_pxRTV->m_uVRAMHandle = xAttachment.m_uVRAMHandle;
	xAttachment.m_pxRTV->m_eViewType = VIEW_TYPE_RTV;

	// Create SRV with mips
	xAttachment.m_pxSRV = new Flux_ShaderResourceView();
	xAttachment.m_pxSRV->m_xImageView = Flux_MemoryManager::CreateShaderResourceView(xAttachment.m_uVRAMHandle, xInfo, 0, xInfo.m_uNumMips);
	xAttachment.m_pxSRV->m_uVRAMHandle = xAttachment.m_uVRAMHandle;
	xAttachment.m_pxSRV->m_eViewType = VIEW_TYPE_SRV;

	// Create UAV with mips if requested by memory flags
	if (m_uMemoryFlags & MEMORY_FLAGS__UNORDERED_ACCESS)
	{
		xAttachment.m_pxUAV = new Flux_UnorderedAccessView();
		xAttachment.m_pxUAV->m_xImageView = Flux_MemoryManager::CreateUnorderedAccessView(xAttachment.m_uVRAMHandle, xInfo, 0);
		xAttachment.m_pxUAV->m_uVRAMHandle = xAttachment.m_uVRAMHandle;
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

	// Create actual target VRAM and store it in registry
	xAttachment.m_uVRAMHandle = Zenith_AssetHandler::CreateDepthStencilAttachment(strName, xInfo);
	xAttachment.m_eFormat = m_eFormat;
	xAttachment.m_uWidth = m_uWidth;
	xAttachment.m_uHeight = m_uHeight;

	// Create DSV with mips
	xAttachment.m_pxDSV = new Flux_DepthStencilView();
	xAttachment.m_pxDSV->m_xImageView = Flux_MemoryManager::CreateDepthStencilView(xAttachment.m_uVRAMHandle, xInfo, 0);
	xAttachment.m_pxDSV->m_uVRAMHandle = xAttachment.m_uVRAMHandle;
	xAttachment.m_pxDSV->m_eViewType = VIEW_TYPE_DSV;

	// Create SRV with mips
	xAttachment.m_pxSRV = new Flux_ShaderResourceView();
	xAttachment.m_pxSRV->m_xImageView = Flux_MemoryManager::CreateShaderResourceView(xAttachment.m_uVRAMHandle, xInfo, 0, xInfo.m_uNumMips);
	xAttachment.m_pxSRV->m_uVRAMHandle = xAttachment.m_uVRAMHandle;
	xAttachment.m_pxSRV->m_eViewType = VIEW_TYPE_SRV;
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