#include "Zenith.h"

#include "Flux/Flux_RenderTargets.h"

#include "AssetHandling/Zenith_AssetHandler.h"

void Flux_RenderAttachmentBuilder::BuildColour(Flux_RenderAttachment& xAttachment, const std::string& strName)
{
	// Check if attachment already has VRAM allocated and queue it for deletion
	if (xAttachment.m_xVRAMHandle.IsValid())
	{
		Zenith_Vulkan_VRAM* pxOldVRAM = Zenith_Vulkan::GetVRAM(xAttachment.m_xVRAMHandle);
		Flux_MemoryManager::QueueVRAMDeletion(pxOldVRAM, xAttachment.m_xVRAMHandle,
			xAttachment.m_pxRTV.m_xImageView, VK_NULL_HANDLE, 
			xAttachment.m_pxSRV.m_xImageView, xAttachment.m_pxUAV.m_xImageView);
	}

	Flux_SurfaceInfo xInfo;
	xInfo.m_uWidth = m_uWidth;
	xInfo.m_uHeight = m_uHeight;
	xInfo.m_eFormat = m_eFormat;
	xInfo.m_uNumMips = 1;
	xInfo.m_uNumLayers = 1;
	xInfo.m_uMemoryFlags = m_uMemoryFlags;

	// Create actual target VRAM and store it in registry
	xAttachment.m_xVRAMHandle = Flux_MemoryManager::CreateRenderTargetVRAM(xInfo);
	xAttachment.m_xSurfaceInfo = xInfo;
	
	// Create RTV with mips
	xAttachment.m_pxRTV.m_xImageView = Flux_MemoryManager::CreateRenderTargetView(xAttachment.m_xVRAMHandle, xInfo, 0);
	xAttachment.m_pxRTV.m_xVRAMHandle = xAttachment.m_xVRAMHandle;

	// Create SRV with mips
	xAttachment.m_pxSRV.m_xImageView = Flux_MemoryManager::CreateShaderResourceView(xAttachment.m_xVRAMHandle, xInfo, 0, xInfo.m_uNumMips);
	xAttachment.m_pxSRV.m_xVRAMHandle = xAttachment.m_xVRAMHandle;

	// Create UAV with mips if requested by memory flags
	if (m_uMemoryFlags & MEMORY_FLAGS__UNORDERED_ACCESS)
	{
		xAttachment.m_pxUAV.m_xImageView = Flux_MemoryManager::CreateUnorderedAccessView(xAttachment.m_xVRAMHandle, xInfo, 0);
		xAttachment.m_pxUAV.m_xVRAMHandle = xAttachment.m_xVRAMHandle;
	}
}

void Flux_RenderAttachmentBuilder::BuildDepthStencil(Flux_RenderAttachment& xAttachment, const std::string& strName)
{
	// Check if attachment already has VRAM allocated and queue it for deletion
	if (xAttachment.m_xVRAMHandle.IsValid())
	{
		Zenith_Vulkan_VRAM* pxOldVRAM = Zenith_Vulkan::GetVRAM(xAttachment.m_xVRAMHandle);
		Flux_MemoryManager::QueueVRAMDeletion(pxOldVRAM, xAttachment.m_xVRAMHandle,
			VK_NULL_HANDLE, xAttachment.m_pxDSV.m_xImageView, 
			xAttachment.m_pxSRV.m_xImageView, VK_NULL_HANDLE);
	}

	Flux_SurfaceInfo xInfo;
	xInfo.m_uWidth = m_uWidth;
	xInfo.m_uHeight = m_uHeight;
	xInfo.m_eFormat = m_eFormat;
	xInfo.m_uNumMips = 1;
	xInfo.m_uNumLayers = 1;
	xInfo.m_uMemoryFlags = m_uMemoryFlags;

	// Create actual target VRAM and store it in registry
	xAttachment.m_xVRAMHandle = Flux_MemoryManager::CreateRenderTargetVRAM(xInfo);
	xAttachment.m_xSurfaceInfo = xInfo;

	// Create DSV with mips
	xAttachment.m_pxDSV.m_xImageView = Flux_MemoryManager::CreateDepthStencilView(xAttachment.m_xVRAMHandle, xInfo, 0);
	xAttachment.m_pxDSV.m_xVRAMHandle = xAttachment.m_xVRAMHandle;

	// Create SRV with mips
	xAttachment.m_pxSRV.m_xImageView = Flux_MemoryManager::CreateShaderResourceView(xAttachment.m_xVRAMHandle, xInfo, 0, xInfo.m_uNumMips);
	xAttachment.m_pxSRV.m_xVRAMHandle = xAttachment.m_xVRAMHandle;
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
		if (xTarget.m_xSurfaceInfo.m_eFormat != TEXTURE_FORMAT_NONE)
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