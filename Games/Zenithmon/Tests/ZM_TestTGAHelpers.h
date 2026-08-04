#pragma once
// ============================================================================
// ZM_TestTGAHelpers -- the deliberately narrow reader for engine-written
// framebuffer captures. Flux_Screenshot writes uncompressed 32-bit BGRA TGAs
// with a top-left origin; this test helper accepts exactly that contract so a
// graphics automated test can assert on the pixels that reached the swapchain.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "Core/Zenith_PlatformStdio.h"

struct ZM_TestTGAImage
{
	ZM_TestTGAImage() = default;
	ZM_TestTGAImage(const ZM_TestTGAImage&) = delete;
	ZM_TestTGAImage& operator=(const ZM_TestTGAImage&) = delete;
	~ZM_TestTGAImage() { delete[] m_puPixels; }

	uint32_t m_uWidth = 0u;
	uint32_t m_uHeight = 0u;
	uint8_t* m_puPixels = nullptr; // BGRA, row zero is the top row

	bool IsValid() const
	{
		return m_puPixels != nullptr && m_uWidth > 0u && m_uHeight > 0u;
	}

	const uint8_t* GetPixelBGRA(uint32_t uX, uint32_t uY) const
	{
		return m_puPixels
			+ (static_cast<size_t>(uY) * m_uWidth + uX) * 4u;
	}
};

inline bool ZM_TestLoadTGA(const char* szPath, ZM_TestTGAImage& xOut)
{
	std::FILE* pFile = Zenith_PlatformStdio::OpenFile(szPath, "rb");
	if (pFile == nullptr)
	{
		return false;
	}

	uint8_t aHeader[18] = {};
	if (std::fread(aHeader, 1u, sizeof(aHeader), pFile) != sizeof(aHeader))
	{
		std::fclose(pFile);
		return false;
	}

	const uint8_t uIdLength = aHeader[0];
	const uint8_t uImageType = aHeader[2];
	const uint32_t uWidth = static_cast<uint32_t>(aHeader[12])
		| (static_cast<uint32_t>(aHeader[13]) << 8u);
	const uint32_t uHeight = static_cast<uint32_t>(aHeader[14])
		| (static_cast<uint32_t>(aHeader[15]) << 8u);
	const uint8_t uBitsPerPixel = aHeader[16];
	const bool bTopLeftOrigin = (aHeader[17] & 0x20u) != 0u;
	if (uImageType != 2u || uBitsPerPixel != 32u || !bTopLeftOrigin
		|| uWidth == 0u || uHeight == 0u)
	{
		std::fclose(pFile);
		return false;
	}

	if (uIdLength != 0u)
	{
		std::fseek(pFile, static_cast<long>(uIdLength), SEEK_CUR);
	}

	const size_t ulBytes = static_cast<size_t>(uWidth) * uHeight * 4u;
	uint8_t* puPixels = new uint8_t[ulBytes];
	if (std::fread(puPixels, 1u, ulBytes, pFile) != ulBytes)
	{
		delete[] puPixels;
		std::fclose(pFile);
		return false;
	}
	std::fclose(pFile);

	delete[] xOut.m_puPixels;
	xOut.m_puPixels = puPixels;
	xOut.m_uWidth = uWidth;
	xOut.m_uHeight = uHeight;
	return true;
}
