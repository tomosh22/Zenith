#include "Zenith.h"

#include "Zenithmon/Source/World/ZM_GrassDensityMap.h"

#include "AssetHandling/Zenith_TextureAsset.h"
#include "FileAccess/Zenith_FileAccess.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <limits>

std::string ZM_GrassDensityMap::BuildCanonicalPath(const std::string& strTerrainAssetDirectory)
{
	const std::filesystem::path xPath = std::filesystem::path(strTerrainAssetDirectory)
		/ (std::string("GrassDensity") + ZENITH_TEXTURE_EXT);
	return xPath.lexically_normal().generic_string();
}

bool ZM_GrassDensityMap::Load(const std::string& strPath)
{
	Flux_SurfaceInfo xInfo;
	Zenith_Vector<uint8_t> xBytes;
	if (!Zenith_TextureAsset::LoadCPUData(strPath, xInfo, xBytes).IsOk())
	{
		Clear();
		return false;
	}

	return LoadDecoded(xInfo.m_eFormat, xInfo.m_uWidth, xInfo.m_uHeight,
		xBytes.GetDataPointer(), static_cast<size_t>(xBytes.GetSize()),
		uEXPECTED_WIDTH, uEXPECTED_HEIGHT, fWORLD_SIZE);
}

void ZM_GrassDensityMap::Clear()
{
	m_afPixels.clear();
	m_uWidth = 0;
	m_uHeight = 0;
	m_fWorldSize = 0.0f;
}

bool ZM_GrassDensityMap::LoadDecoded(TextureFormat eFormat, u_int uWidth,
	u_int uHeight, const void* pBytes, size_t ulByteCount, u_int uExpectedWidth,
	u_int uExpectedHeight, float fWorldSize)
{
	Clear();

	if (eFormat != TEXTURE_FORMAT_R32_SFLOAT
		|| uWidth != uExpectedWidth
		|| uHeight != uExpectedHeight
		|| uWidth == 0
		|| uHeight == 0
		|| fWorldSize <= 0.0f)
	{
		return false;
	}

	if (static_cast<size_t>(uWidth) > std::numeric_limits<size_t>::max()
		/ static_cast<size_t>(uHeight))
	{
		return false;
	}
	const size_t ulPixelCount = static_cast<size_t>(uWidth) * uHeight;
	if (ulPixelCount > std::numeric_limits<size_t>::max() / sizeof(float))
	{
		return false;
	}
	const size_t ulExpectedBytes = ulPixelCount * sizeof(float);
	if (pBytes == nullptr || ulByteCount != ulExpectedBytes)
	{
		return false;
	}

	m_afPixels.resize(ulPixelCount);
	memcpy(m_afPixels.data(), pBytes, ulExpectedBytes);
	m_uWidth = uWidth;
	m_uHeight = uHeight;
	m_fWorldSize = fWorldSize;
	return true;
}

float ZM_GrassDensityMap::SampleWorld(float fWorldX, float fWorldZ) const
{
	if (!IsLoaded())
	{
		return 0.0f;
	}

	const float fScaleX = static_cast<float>(m_uWidth) / m_fWorldSize;
	const float fScaleZ = static_cast<float>(m_uHeight) / m_fWorldSize;
	const float fPixelX = std::clamp(fWorldX * fScaleX, 0.0f,
		static_cast<float>(m_uWidth - 1));
	const float fPixelZ = std::clamp(fWorldZ * fScaleZ, 0.0f,
		static_cast<float>(m_uHeight - 1));

	const u_int uX0 = static_cast<u_int>(fPixelX);
	const u_int uZ0 = static_cast<u_int>(fPixelZ);
	const u_int uX1 = std::min(uX0 + 1, m_uWidth - 1);
	const u_int uZ1 = std::min(uZ0 + 1, m_uHeight - 1);
	const float fBlendX = fPixelX - static_cast<float>(uX0);
	const float fBlendZ = fPixelZ - static_cast<float>(uZ0);

	const float fTop = m_afPixels[static_cast<size_t>(uZ0) * m_uWidth + uX0]
		* (1.0f - fBlendX)
		+ m_afPixels[static_cast<size_t>(uZ0) * m_uWidth + uX1] * fBlendX;
	const float fBottom = m_afPixels[static_cast<size_t>(uZ1) * m_uWidth + uX0]
		* (1.0f - fBlendX)
		+ m_afPixels[static_cast<size_t>(uZ1) * m_uWidth + uX1] * fBlendX;
	return fTop * (1.0f - fBlendZ) + fBottom * fBlendZ;
}
