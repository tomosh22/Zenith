#pragma once
// ============================================================================
// DP_TestTGAHelpers — minimal TGA reader for screenshot-based test
// assertions. The engine only WRITES screenshots (Flux_Screenshot ->
// WriteSwapchainScreenshotTGA: uncompressed 32-bit BGRA, top-left origin,
// 18-byte header); this is the matching read-back so tests can assert on
// rendered pixels. Test-only — deliberately supports exactly the format the
// engine writes, nothing else.
// ============================================================================

#include <cstdint>
#include <cstdio>

struct DP_TestTGAImage
{
	DP_TestTGAImage() = default;
	DP_TestTGAImage(const DP_TestTGAImage&) = delete;
	DP_TestTGAImage& operator=(const DP_TestTGAImage&) = delete;
	~DP_TestTGAImage() { delete[] m_puPixels; }

	uint32_t m_uWidth  = 0;
	uint32_t m_uHeight = 0;
	uint8_t* m_puPixels = nullptr; // BGRA, row 0 = top row

	bool IsValid() const { return m_puPixels != nullptr && m_uWidth > 0 && m_uHeight > 0; }

	// BGRA channel fetch; (0,0) = top-left. No bounds checking beyond the
	// assert — test code indexes derived-from-dimensions coordinates.
	const uint8_t* GetPixelBGRA(uint32_t uX, uint32_t uY) const
	{
		return m_puPixels + (static_cast<size_t>(uY) * m_uWidth + uX) * 4u;
	}
};

// Loads an engine-written screenshot TGA. Returns false (and leaves xOut
// invalid) on missing file or any format the engine writer never produces.
inline bool DP_TestLoadTGA(const char* szPath, DP_TestTGAImage& xOut)
{
	std::FILE* pFile = nullptr;
	if (fopen_s(&pFile, szPath, "rb") != 0 || pFile == nullptr) return false;

	uint8_t aHeader[18] = {};
	if (std::fread(aHeader, 1, sizeof(aHeader), pFile) != sizeof(aHeader))
	{
		std::fclose(pFile);
		return false;
	}

	const uint8_t uIdLength  = aHeader[0];
	const uint8_t uImageType = aHeader[2];              // 2 = uncompressed truecolour
	const uint32_t uWidth  = aHeader[12] | (aHeader[13] << 8);
	const uint32_t uHeight = aHeader[14] | (aHeader[15] << 8);
	const uint8_t uBpp        = aHeader[16];
	const bool bTopLeftOrigin = (aHeader[17] & 0x20) != 0;
	if (uImageType != 2 || uBpp != 32 || !bTopLeftOrigin || uWidth == 0 || uHeight == 0)
	{
		std::fclose(pFile);
		return false;
	}
	if (uIdLength != 0)
	{
		std::fseek(pFile, uIdLength, SEEK_CUR);
	}

	const size_t ulBytes = static_cast<size_t>(uWidth) * uHeight * 4u;
	uint8_t* puPixels = new uint8_t[ulBytes];
	if (std::fread(puPixels, 1, ulBytes, pFile) != ulBytes)
	{
		delete[] puPixels;
		std::fclose(pFile);
		return false;
	}
	std::fclose(pFile);

	delete[] xOut.m_puPixels;
	xOut.m_puPixels = puPixels;
	xOut.m_uWidth   = uWidth;
	xOut.m_uHeight  = uHeight;
	return true;
}

// Mean absolute per-channel (BGR; alpha ignored) difference between the same
// axis-aligned window of two same-sized images. Window is clamped to the
// image. Returns 0 for degenerate inputs.
inline float DP_TestMeanAbsDiffBGR(const DP_TestTGAImage& xA, const DP_TestTGAImage& xB,
	uint32_t uX0, uint32_t uY0, uint32_t uX1, uint32_t uY1)
{
	if (!xA.IsValid() || !xB.IsValid()) return 0.0f;
	if (xA.m_uWidth != xB.m_uWidth || xA.m_uHeight != xB.m_uHeight) return 0.0f;
	uX1 = uX1 < xA.m_uWidth  ? uX1 : xA.m_uWidth;
	uY1 = uY1 < xA.m_uHeight ? uY1 : xA.m_uHeight;
	if (uX0 >= uX1 || uY0 >= uY1) return 0.0f;

	uint64_t ulTotal = 0;
	for (uint32_t uY = uY0; uY < uY1; ++uY)
	{
		for (uint32_t uX = uX0; uX < uX1; ++uX)
		{
			const uint8_t* pA = xA.GetPixelBGRA(uX, uY);
			const uint8_t* pB = xB.GetPixelBGRA(uX, uY);
			ulTotal += static_cast<uint64_t>(pA[0] > pB[0] ? pA[0] - pB[0] : pB[0] - pA[0]);
			ulTotal += static_cast<uint64_t>(pA[1] > pB[1] ? pA[1] - pB[1] : pB[1] - pA[1]);
			ulTotal += static_cast<uint64_t>(pA[2] > pB[2] ? pA[2] - pB[2] : pB[2] - pA[2]);
		}
	}
	const uint64_t ulSamples = static_cast<uint64_t>(uX1 - uX0) * (uY1 - uY0) * 3u;
	return static_cast<float>(ulTotal) / static_cast<float>(ulSamples);
}
