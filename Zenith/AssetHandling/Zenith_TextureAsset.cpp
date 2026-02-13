#include "Zenith.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "Vulkan/Zenith_Vulkan_MemoryManager.h"
#include "Vulkan/Zenith_Vulkan.h"
#include "DataStream/Zenith_DataStream.h"
#include "Flux/Flux_Enums.h"
#include "Flux/Flux_Types.h"

// Unified data size calculation for both compressed and uncompressed textures
static size_t CalculateTextureDataSize(TextureFormat eFormat, uint32_t uWidth, uint32_t uHeight, uint32_t uDepth = 1)
{
	if (IsCompressedFormat(eFormat))
	{
		return CalculateCompressedTextureSize(eFormat, uWidth, uHeight);
	}
	return static_cast<size_t>(ColourFormatBytesPerPixel(eFormat)) * uWidth * uHeight * uDepth;
}

// Free an array of allocated cubemap face data up to uCount entries
static void FreeCubemapFaceData(void* apDatas[6], uint32_t uCount)
{
	for (uint32_t u = 0; u < uCount; u++)
	{
		if (apDatas[u])
		{
			Zenith_MemoryManagement::Deallocate(apDatas[u]);
			apDatas[u] = nullptr;
		}
	}
}

Zenith_TextureAsset::Zenith_TextureAsset()
{
	m_xSurfaceInfo = Flux_SurfaceInfo();
	m_xVRAMHandle = Flux_VRAMHandle();
	m_xSRV = Flux_ShaderResourceView();
}

Zenith_TextureAsset::~Zenith_TextureAsset()
{
	ReleaseGPU();
}

bool Zenith_TextureAsset::LoadFromFile(const std::string& strPath, bool bCreateMips)
{
	// Load texture data from file
	size_t ulDataSize;
	int32_t iWidth = 0, iHeight = 0, iDepth = 0;
	TextureFormat eFormat;

	Zenith_DataStream xStream;
	xStream.ReadFromFile(strPath.c_str());

	if (!xStream.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "Zenith_TextureAsset: Failed to read file '%s'", strPath.c_str());
		return false;
	}

	xStream >> iWidth;
	xStream >> iHeight;
	xStream >> iDepth;
	xStream >> eFormat;
	xStream >> ulDataSize;

	// Ensure depth is at least 1 for 2D textures (file may store 0 for non-3D textures)
	// Also recalculate expected data size since file may have stored wrong size
	const int32_t iCorrectedDepth = std::max(1, iDepth);
	const bool bIsCompressed = IsCompressedFormat(eFormat);
	const size_t ulExpectedDataSize = CalculateTextureDataSize(eFormat, iWidth, iHeight, iCorrectedDepth);

	// Use the larger of file-stored size or expected size for safety
	// If file stored size 0 but we expect data, use expected size
	// If file stored larger size, use that (might have extra padding)
	size_t ulAllocSize = std::max(ulDataSize, ulExpectedDataSize);

	if (ulAllocSize == 0)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "Zenith_TextureAsset: Zero data size for texture '%s' (dims: %dx%dx%d)", strPath.c_str(), iWidth, iHeight, iDepth);
		return false;
	}

	void* pData = Zenith_MemoryManagement::Allocate(ulAllocSize);
	if (!pData)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "Zenith_TextureAsset: Failed to allocate %zu bytes for texture '%s'", ulAllocSize, strPath.c_str());
		return false;
	}

	// Initialize to zero in case file has less data than expected
	memset(pData, 0, ulAllocSize);

	// Read actual data from file
	// If file stored ulDataSize=0 but dimensions are valid, use expected size for reading
	// (some files incorrectly store 0 for data size but still have pixel data)
	size_t ulReadSize = (ulDataSize > 0) ? ulDataSize : ulExpectedDataSize;
	if (ulReadSize > 0)
	{
		xStream.ReadData(pData, ulReadSize);
	}

	// Determine mip count
	const uint32_t uNumMips = (bCreateMips && !bIsCompressed)
		? static_cast<uint32_t>(std::floor(std::log2((std::max)(iWidth, iHeight))) + 1)
		: 1;

	// Set up surface info
	m_xSurfaceInfo.m_uWidth = static_cast<uint32_t>(iWidth);
	m_xSurfaceInfo.m_uHeight = static_cast<uint32_t>(iHeight);
	m_xSurfaceInfo.m_uDepth = static_cast<uint32_t>(iCorrectedDepth);
	m_xSurfaceInfo.m_uNumLayers = 1;
	m_xSurfaceInfo.m_eFormat = eFormat;
	m_xSurfaceInfo.m_eTextureType = TEXTURE_TYPE_2D;
	m_xSurfaceInfo.m_uNumMips = uNumMips;
	m_xSurfaceInfo.m_uMemoryFlags = 1 << MEMORY_FLAGS__SHADER_READ;

	// Create GPU resources
	m_xVRAMHandle = Flux_MemoryManager::CreateTextureVRAM(pData, m_xSurfaceInfo, bCreateMips);
	m_xSRV = Flux_MemoryManager::CreateShaderResourceView(m_xVRAMHandle, m_xSurfaceInfo);
	m_bGPUResourcesAllocated = true;

	// Free the staging data
	Zenith_MemoryManagement::Deallocate(pData);

	return true;
}

bool Zenith_TextureAsset::CreateFromData(const void* pData, const Flux_SurfaceInfo& xSurfaceInfo, bool bCreateMips)
{
	if (!pData)
	{
		return false;
	}

	m_xSurfaceInfo = xSurfaceInfo;

	// Ensure proper memory flags
	if (m_xSurfaceInfo.m_uMemoryFlags == 0)
	{
		m_xSurfaceInfo.m_uMemoryFlags = 1 << MEMORY_FLAGS__SHADER_READ;
	}

	// Create GPU resources
	m_xVRAMHandle = Flux_MemoryManager::CreateTextureVRAM(pData, m_xSurfaceInfo, bCreateMips);
	m_xSRV = Flux_MemoryManager::CreateShaderResourceView(m_xVRAMHandle, m_xSurfaceInfo);
	m_bGPUResourcesAllocated = true;

	return m_xVRAMHandle.IsValid();
}

bool Zenith_TextureAsset::CreateCubemap(const void* apFaceData[6], const Flux_SurfaceInfo& xSurfaceInfo)
{
	if (!apFaceData)
	{
		return false;
	}

	// Calculate face data size
	const size_t ulLayerDataSize = CalculateTextureDataSize(xSurfaceInfo.m_eFormat,
		xSurfaceInfo.m_uWidth, xSurfaceInfo.m_uHeight);

	const size_t ulTotalDataSize = ulLayerDataSize * 6;

	// Concatenate face data
	void* pAllData = Zenith_MemoryManagement::Allocate(ulTotalDataSize);
	if (!pAllData)
	{
		return false;
	}

	for (uint32_t u = 0; u < 6; u++)
	{
		memcpy(static_cast<uint8_t*>(pAllData) + (u * ulLayerDataSize), apFaceData[u], ulLayerDataSize);
	}

	// Set up surface info for cubemap
	m_xSurfaceInfo = xSurfaceInfo;
	m_xSurfaceInfo.m_eTextureType = TEXTURE_TYPE_CUBE;
	m_xSurfaceInfo.m_uNumLayers = 6;
	if (m_xSurfaceInfo.m_uMemoryFlags == 0)
	{
		m_xSurfaceInfo.m_uMemoryFlags = 1 << MEMORY_FLAGS__SHADER_READ;
	}

	// Create GPU resources
	m_xVRAMHandle = Flux_MemoryManager::CreateTextureVRAM(pAllData, m_xSurfaceInfo, false);
	m_xSRV = Flux_MemoryManager::CreateShaderResourceView(m_xVRAMHandle, m_xSurfaceInfo);
	m_bGPUResourcesAllocated = true;

	Zenith_MemoryManagement::Deallocate(pAllData);

	return m_xVRAMHandle.IsValid();
}

bool Zenith_TextureAsset::LoadCubemapFromFiles(
	const char* szPathPX, const char* szPathNX,
	const char* szPathPY, const char* szPathNY,
	const char* szPathPZ, const char* szPathNZ)
{
	const char* aszPaths[6] = { szPathPX, szPathNX, szPathPY, szPathNY, szPathPZ, szPathNZ };
	void* apDatas[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
	size_t aulDataSizes[6] = { 0, 0, 0, 0, 0, 0 };
	uint32_t uWidth = 0, uHeight = 0, uDepth = 0;
	TextureFormat eFormat = TEXTURE_FORMAT_RGBA8_UNORM;

	for (uint32_t u = 0; u < 6; u++)
	{
		Zenith_DataStream xStream;
		xStream.ReadFromFile(aszPaths[u]);

		if (!xStream.IsValid())
		{
			Zenith_Error(LOG_CATEGORY_ASSET, "LoadCubemapFromFiles: Failed to read face %u from '%s'", u, aszPaths[u]);
			FreeCubemapFaceData(apDatas, u);
			return false;
		}

		TextureFormat eFaceFormat;
		xStream >> uWidth;
		xStream >> uHeight;
		xStream >> uDepth;
		xStream >> eFaceFormat;
		xStream >> aulDataSizes[u];

		if (u == 0)
		{
			eFormat = eFaceFormat;
		}

		apDatas[u] = Zenith_MemoryManagement::Allocate(aulDataSizes[u]);
		if (!apDatas[u])
		{
			Zenith_Log(LOG_CATEGORY_ASSET, "ERROR: Failed to allocate cubemap face %u", u);
			FreeCubemapFaceData(apDatas, u);
			return false;
		}
		xStream.ReadData(apDatas[u], aulDataSizes[u]);
	}

	// Set up surface info
	Flux_SurfaceInfo xInfo;
	xInfo.m_uWidth = uWidth;
	xInfo.m_uHeight = uHeight;
	xInfo.m_uDepth = uDepth;
	xInfo.m_uNumLayers = 6;
	xInfo.m_eFormat = eFormat;
	xInfo.m_uNumMips = 1;
	xInfo.m_uMemoryFlags = 1 << MEMORY_FLAGS__SHADER_READ;

	// Create cubemap from face data
	const void* apFaceData[6] = { apDatas[0], apDatas[1], apDatas[2], apDatas[3], apDatas[4], apDatas[5] };
	bool bSuccess = CreateCubemap(apFaceData, xInfo);

	// Free staging data
	FreeCubemapFaceData(apDatas, 6);

	return bSuccess;
}

void Zenith_TextureAsset::ReleaseGPU()
{
	if (m_bGPUResourcesAllocated && m_xVRAMHandle.IsValid())
	{
		Zenith_Vulkan_VRAM* pxVRAM = Zenith_Vulkan::GetVRAM(m_xVRAMHandle);
		Flux_MemoryManager::QueueVRAMDeletion(pxVRAM, m_xVRAMHandle, m_xSRV.m_xImageViewHandle);
		m_xSRV = Flux_ShaderResourceView();
		m_bGPUResourcesAllocated = false;
	}
}

