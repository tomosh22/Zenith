#include "Zenith.h"
#include "Zenith_Tools_TextureExport.h"
// Wave-13 PCH slim round 2: <filesystem> was demoted out of Zenith.h. This TU
// uses std::filesystem (directory iteration in ExportAllTextures below), so it
// carries the explicit include.
#include <filesystem>

// Helper functions to construct asset paths from project name
static std::string GetGameAssetsDirectory()
{
	return std::string(ZENITH_ROOT) + "Games/" + Project_GetName() + "/Assets/";
}

static std::string GetEngineAssetsDirectory()
{
	return std::string(ZENITH_ROOT) + "Zenith/Assets/";
}
#include "Flux/Flux.h"
#include "AssetHandling/Zenith_TextureAsset.h"   // .ztxtr envelope id/schema constants
#include "DataStream/Zenith_StreamEnvelope.h"    // Zenith_WriteStreamHeader
#include <vector>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <utility>
#include <cmath>
#define STB_IMAGE_IMPLEMENTATION
#pragma warning(push, 0)
#include "stb/stb_image.h"
#define STB_DXT_IMPLEMENTATION
#include "stb/stb_dxt.h"
#pragma warning(pop)

bool Zenith_Tools_TextureExport::IsCompressedFormat(TextureFormat eFormat)
{
	// One definition: the Flux helper the loader and the GPU upload use.
	return ::IsCompressedFormat(eFormat);
}

uint32_t Zenith_Tools_TextureExport::GetBytesPerBlockOrPixel(TextureFormat eFormat)
{
	switch (eFormat)
	{
	case TEXTURE_FORMAT_BC1_RGB_UNORM:
	case TEXTURE_FORMAT_BC1_RGBA_UNORM:
	case TEXTURE_FORMAT_BC1_RGB_SRGB:
	case TEXTURE_FORMAT_BC1_RGBA_SRGB:
		return 8;  // 8 bytes per 4x4 block
	case TEXTURE_FORMAT_BC3_RGBA_UNORM:
	case TEXTURE_FORMAT_BC5_RG_UNORM:
	case TEXTURE_FORMAT_BC7_RGBA_UNORM:
	case TEXTURE_FORMAT_BC3_RGBA_SRGB:
	case TEXTURE_FORMAT_BC7_RGBA_SRGB:
		return 16; // 16 bytes per 4x4 block
	case TEXTURE_FORMAT_RGBA8_UNORM:
	case TEXTURE_FORMAT_RGBA8_SRGB:
	case TEXTURE_FORMAT_BGRA8_UNORM:
	case TEXTURE_FORMAT_BGRA8_SRGB:
		return 4;  // 4 bytes per pixel
	case TEXTURE_FORMAT_RGB8_UNORM:
		return 3;  // 3 bytes per pixel
	default:
		return 4;  // Default to 4 bytes per pixel
	}
}

TextureFormat Zenith_Tools_TextureExport::ResolveExportFormat(TextureCompressionMode eCompression, TextureColourSpace eColourSpace)
{
	const bool bSRGB = (eColourSpace == TextureColourSpace::SRGB);
	switch (eCompression)
	{
	case TextureCompressionMode::BC1:
		return bSRGB ? TEXTURE_FORMAT_BC1_RGB_SRGB : TEXTURE_FORMAT_BC1_RGB_UNORM;
	case TextureCompressionMode::BC1_Alpha:
		return bSRGB ? TEXTURE_FORMAT_BC1_RGBA_SRGB : TEXTURE_FORMAT_BC1_RGBA_UNORM;
	case TextureCompressionMode::BC3:
		return bSRGB ? TEXTURE_FORMAT_BC3_RGBA_SRGB : TEXTURE_FORMAT_BC3_RGBA_UNORM;
	case TextureCompressionMode::BC5:
		// Two-channel DATA (tangent-space normal): no sRGB twin exists and none
		// would be right -- a normal decoded through the EOTF is a bent normal.
		return TEXTURE_FORMAT_BC5_RG_UNORM;
	case TextureCompressionMode::Uncompressed:
	default:
		return bSRGB ? TEXTURE_FORMAT_RGBA8_SRGB : TEXTURE_FORMAT_RGBA8_UNORM;
	}
}

void Zenith_Tools_TextureExport::ResolveUsage(TextureUsage eUsage,
	TextureCompressionMode& eCompressionOut, TextureColourSpace& eColourSpaceOut)
{
	switch (eUsage)
	{
	case TextureUsage::BaseColour:
		eCompressionOut = TextureCompressionMode::BC1;         eColourSpaceOut = TextureColourSpace::SRGB;   return;
	case TextureUsage::BaseColourMasked:
		eCompressionOut = TextureCompressionMode::BC1_Alpha;   eColourSpaceOut = TextureColourSpace::SRGB;   return;
	case TextureUsage::NormalMap:
		eCompressionOut = TextureCompressionMode::BC5;         eColourSpaceOut = TextureColourSpace::Linear; return;
	case TextureUsage::LinearData:
		eCompressionOut = TextureCompressionMode::BC1;         eColourSpaceOut = TextureColourSpace::Linear; return;
	case TextureUsage::UncompressedColour:
		eCompressionOut = TextureCompressionMode::Uncompressed; eColourSpaceOut = TextureColourSpace::SRGB;   return;
	case TextureUsage::UncompressedData:
		eCompressionOut = TextureCompressionMode::Uncompressed; eColourSpaceOut = TextureColourSpace::Linear; return;
	}
	// No default: every enumerator is handled above, so adding one is a compile
	// warning here rather than a silent fall-through to somebody's favourite guess.
	Zenith_Assert(false, "unhandled TextureUsage");
	eCompressionOut = TextureCompressionMode::BC1;
	eColourSpaceOut = TextureColourSpace::Linear;
}

// The token spelled in a .ztexdecl. Kept beside ResolveUsage so a new usage
// cannot be added without giving it a spelling.
static const struct { const char* szToken; Zenith_Tools_TextureExport::TextureUsage eUsage; } s_axUSAGE_TOKENS[] =
{
	{ "BASE_COLOUR",         Zenith_Tools_TextureExport::TextureUsage::BaseColour },
	{ "BASE_COLOUR_MASKED",  Zenith_Tools_TextureExport::TextureUsage::BaseColourMasked },
	{ "NORMAL_MAP",          Zenith_Tools_TextureExport::TextureUsage::NormalMap },
	{ "LINEAR_DATA",         Zenith_Tools_TextureExport::TextureUsage::LinearData },
	{ "UNCOMPRESSED_COLOUR", Zenith_Tools_TextureExport::TextureUsage::UncompressedColour },
	{ "UNCOMPRESSED_DATA",   Zenith_Tools_TextureExport::TextureUsage::UncompressedData },
};

const char* Zenith_Tools_TextureExport::TextureUsageToken(TextureUsage eUsage)
{
	for (const auto& x : s_axUSAGE_TOKENS)
		if (x.eUsage == eUsage) return x.szToken;
	return "?";
}

bool Zenith_Tools_TextureExport::TextureUsageFromToken(const std::string& strToken, TextureUsage& eUsageOut)
{
	for (const auto& x : s_axUSAGE_TOKENS)
	{
		if (strToken == x.szToken) { eUsageOut = x.eUsage; return true; }
	}
	return false;
}

// Root-relative paths are compared forward-slashed and lower-cased: a Windows
// source tree is not case-stable, and a declaration that stops matching after
// somebody renames "Normal.jpg" to "normal.jpg" is the failure mode this whole
// file exists to remove.
static std::string NormalisePath(const std::string& strPath)
{
	std::string str = strPath;
	for (char& c : str)
	{
		if (c == '\\') c = '/';
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	}
	return str;
}

bool Zenith_Tools_TextureExport::TextureUsageManifest::ParseText(const std::string& strText)
{
	m_xEntries.clear();
	m_strError.clear();

	std::istringstream xStream(strText);
	std::string strLine;
	uint32_t uLineNumber = 0u;
	while (std::getline(xStream, strLine))
	{
		++uLineNumber;
		// Strip a trailing CR so a CRLF file parses identically to an LF one.
		if (!strLine.empty() && strLine.back() == '\r') strLine.pop_back();

		std::istringstream xLine(strLine);
		std::string strPath;
		if (!(xLine >> strPath))       continue;   // blank
		if (strPath[0] == '#')         continue;   // comment

		std::string strToken;
		if (!(xLine >> strToken))
		{
			m_strError = "line " + std::to_string(uLineNumber) + ": '" + strPath + "' has no usage token";
			return false;
		}
		TextureUsage eUsage;
		if (!TextureUsageFromToken(strToken, eUsage))
		{
			m_strError = "line " + std::to_string(uLineNumber) + ": unknown usage '" + strToken + "' for '" + strPath + "'";
			return false;
		}
		const std::string strKey = NormalisePath(strPath);
		for (const auto& x : m_xEntries)
		{
			if (x.first == strKey)
			{
				m_strError = "line " + std::to_string(uLineNumber) + ": '" + strPath + "' is declared twice";
				return false;
			}
		}
		m_xEntries.emplace_back(strKey, eUsage);
	}
	return true;
}

bool Zenith_Tools_TextureExport::TextureUsageManifest::LoadForRoot(const std::string& strRootDir)
{
	m_xEntries.clear();
	m_strError.clear();

	std::filesystem::path xPath = std::filesystem::path(strRootDir) / Filename();
	std::ifstream xFile(xPath);
	if (!xFile)
	{
		m_strError = "no " + std::string(Filename()) + " at " + xPath.string();
		return false;
	}
	std::ostringstream xBuffer;
	xBuffer << xFile.rdbuf();
	return ParseText(xBuffer.str());
}

bool Zenith_Tools_TextureExport::TextureUsageManifest::TryGetUsage(const std::string& strRelativePath, TextureUsage& eUsageOut) const
{
	const std::string strKey = NormalisePath(strRelativePath);
	for (const auto& x : m_xEntries)
	{
		if (x.first == strKey) { eUsageOut = x.second; return true; }
	}
	return false;
}

// ---------------------------------------------------------------------------
// BC1 (DXT1) helpers
// ---------------------------------------------------------------------------

static void Expand565(uint16_t u565, uint8_t* pRGB)
{
	// Same expansion stb_dxt uses for its palette (stb__From16Bit), so the index
	// search below matches against exactly the colours the GPU will reconstruct.
	const int iR = (u565 & 0xF800) >> 11;
	const int iG = (u565 & 0x07E0) >> 5;
	const int iB = (u565 & 0x001F);
	pRGB[0] = static_cast<uint8_t>((iR * 33) >> 2);
	pRGB[1] = static_cast<uint8_t>((iG * 65) >> 4);
	pRGB[2] = static_cast<uint8_t>((iB * 33) >> 2);
}

// Encode one 4x4 RGBA block in BC1's 3-COLOUR mode (colour0 <= colour1), where
// index 3 is "transparent black". stb_compress_dxt_block never emits this mode
// -- its `alpha` argument selects a 16-byte DXT5 block, not punch-through -- so
// BC1_RGBA was previously written with every texel opaque and the alpha
// silently dropped. Endpoints come from stb's own search over the OPAQUE texels
// (transparent ones are replaced by the opaque mean so they neither pull the
// PCA axis nor stretch the endpoint range), then the block is re-indexed
// against the three-entry palette with transparent texels pinned to index 3.
static void CompressBC1BlockPunchThrough(const uint8_t* pBlockRGBA, uint8_t* pDst)
{
	constexpr uint8_t uALPHA_OPAQUE_THRESHOLD = 128;   // BC1 alpha is 1 bit: >= 0.5 is opaque
	bool abTransparent[16];
	uint32_t auSum[3] = { 0, 0, 0 };
	int32_t iOpaque = 0;
	for (int32_t i = 0; i < 16; i++)
	{
		abTransparent[i] = pBlockRGBA[i * 4 + 3] < uALPHA_OPAQUE_THRESHOLD;
		if (!abTransparent[i])
		{
			auSum[0] += pBlockRGBA[i * 4 + 0];
			auSum[1] += pBlockRGBA[i * 4 + 1];
			auSum[2] += pBlockRGBA[i * 4 + 2];
			iOpaque++;
		}
	}

	uint16_t uC0 = 0, uC1 = 0;
	if (iOpaque > 0)
	{
		uint8_t auOpaqueBlock[16 * 4];
		for (int32_t i = 0; i < 16; i++)
		{
			if (abTransparent[i])
			{
				auOpaqueBlock[i * 4 + 0] = static_cast<uint8_t>(auSum[0] / iOpaque);
				auOpaqueBlock[i * 4 + 1] = static_cast<uint8_t>(auSum[1] / iOpaque);
				auOpaqueBlock[i * 4 + 2] = static_cast<uint8_t>(auSum[2] / iOpaque);
			}
			else
			{
				auOpaqueBlock[i * 4 + 0] = pBlockRGBA[i * 4 + 0];
				auOpaqueBlock[i * 4 + 1] = pBlockRGBA[i * 4 + 1];
				auOpaqueBlock[i * 4 + 2] = pBlockRGBA[i * 4 + 2];
			}
			auOpaqueBlock[i * 4 + 3] = 255;
		}
		uint8_t auEndpoints[8];
		stb_compress_dxt_block(auEndpoints, auOpaqueBlock, 0, STB_DXT_HIGHQUAL);
		uC0 = static_cast<uint16_t>(auEndpoints[0] | (auEndpoints[1] << 8));
		uC1 = static_cast<uint16_t>(auEndpoints[2] | (auEndpoints[3] << 8));
		if (uC0 > uC1)
		{
			std::swap(uC0, uC1);   // 3-colour mode is signalled by colour0 <= colour1
		}
	}

	// Palette: c0, c1, (c0 + c1) / 2, transparent.
	uint8_t auPalette[3][3];
	Expand565(uC0, auPalette[0]);
	Expand565(uC1, auPalette[1]);
	for (int32_t c = 0; c < 3; c++)
	{
		auPalette[2][c] = static_cast<uint8_t>((auPalette[0][c] + auPalette[1][c]) / 2);
	}

	uint32_t uMask = 0;
	for (int32_t i = 0; i < 16; i++)
	{
		uint32_t uIndex = 3;
		if (!abTransparent[i])
		{
			int32_t iBest = 0x7FFFFFFF;
			for (uint32_t p = 0; p < 3; p++)
			{
				const int32_t iDR = static_cast<int32_t>(pBlockRGBA[i * 4 + 0]) - auPalette[p][0];
				const int32_t iDG = static_cast<int32_t>(pBlockRGBA[i * 4 + 1]) - auPalette[p][1];
				const int32_t iDB = static_cast<int32_t>(pBlockRGBA[i * 4 + 2]) - auPalette[p][2];
				const int32_t iDist = iDR * iDR + iDG * iDG + iDB * iDB;
				if (iDist < iBest)
				{
					iBest = iDist;
					uIndex = p;
				}
			}
		}
		uMask |= uIndex << (2 * i);
	}

	pDst[0] = static_cast<uint8_t>(uC0);
	pDst[1] = static_cast<uint8_t>(uC0 >> 8);
	pDst[2] = static_cast<uint8_t>(uC1);
	pDst[3] = static_cast<uint8_t>(uC1 >> 8);
	pDst[4] = static_cast<uint8_t>(uMask);
	pDst[5] = static_cast<uint8_t>(uMask >> 8);
	pDst[6] = static_cast<uint8_t>(uMask >> 16);
	pDst[7] = static_cast<uint8_t>(uMask >> 24);
}

// Compress RGBA data to BC1 format. With bPunchThroughAlpha, any block holding a
// texel below 50% alpha is written in 3-colour mode with that texel transparent
// (BC1_RGBA); fully opaque blocks keep the 4-colour mode either way.
static void CompressToBC1(const uint8_t* pRGBAData, uint8_t* pOutputData, int32_t iWidth, int32_t iHeight, bool bPunchThroughAlpha)
{
	const int32_t iBlocksX = (iWidth + 3) / 4;
	const int32_t iBlocksY = (iHeight + 3) / 4;

	uint8_t block[16 * 4]; // 4x4 block of RGBA pixels

	for (int32_t by = 0; by < iBlocksY; by++)
	{
		for (int32_t bx = 0; bx < iBlocksX; bx++)
		{
			// Extract 4x4 block from source image
			bool bAnyTransparent = false;
			for (int32_t py = 0; py < 4; py++)
			{
				for (int32_t px = 0; px < 4; px++)
				{
					int32_t srcX = bx * 4 + px;
					int32_t srcY = by * 4 + py;

					// Clamp to image bounds (pad with edge pixels)
					srcX = (srcX < iWidth) ? srcX : (iWidth - 1);
					srcY = (srcY < iHeight) ? srcY : (iHeight - 1);

					const uint8_t* pSrcPixel = pRGBAData + (srcY * iWidth + srcX) * 4;
					uint8_t* pDstPixel = block + (py * 4 + px) * 4;

					pDstPixel[0] = pSrcPixel[0]; // R
					pDstPixel[1] = pSrcPixel[1]; // G
					pDstPixel[2] = pSrcPixel[2]; // B
					pDstPixel[3] = pSrcPixel[3]; // A
					bAnyTransparent |= pSrcPixel[3] < 128;
				}
			}

			// Compress block
			uint8_t* pDstBlock = pOutputData + (by * iBlocksX + bx) * 8;
			if (bPunchThroughAlpha && bAnyTransparent)
			{
				CompressBC1BlockPunchThrough(block, pDstBlock);
			}
			else
			{
				// alpha=0: an 8-byte colour-only block. (A non-zero alpha argument
				// makes stb write a 16-byte DXT5 block -- it is NOT punch-through.)
				stb_compress_dxt_block(pDstBlock, block, 0, STB_DXT_HIGHQUAL);
			}
		}
	}
}

// Compress RGBA data to BC3 format (DXT5)
// BC3 = BC4 alpha block (8 bytes) + BC1 color block (8 bytes) = 16 bytes per 4x4 block
static void CompressToBC3(const uint8_t* pRGBAData, uint8_t* pOutputData, int32_t iWidth, int32_t iHeight)
{
	const int32_t iBlocksX = (iWidth + 3) / 4;
	const int32_t iBlocksY = (iHeight + 3) / 4;

	uint8_t block[16 * 4];     // 4x4 block of RGBA pixels
	uint8_t alphaBlock[16];    // 4x4 block of just alpha values

	for (int32_t by = 0; by < iBlocksY; by++)
	{
		for (int32_t bx = 0; bx < iBlocksX; bx++)
		{
			// Extract 4x4 block from source image
			for (int32_t py = 0; py < 4; py++)
			{
				for (int32_t px = 0; px < 4; px++)
				{
					int32_t srcX = bx * 4 + px;
					int32_t srcY = by * 4 + py;

					// Clamp to image bounds (pad with edge pixels)
					srcX = (srcX < iWidth) ? srcX : (iWidth - 1);
					srcY = (srcY < iHeight) ? srcY : (iHeight - 1);

					const uint8_t* pSrcPixel = pRGBAData + (srcY * iWidth + srcX) * 4;
					uint8_t* pDstPixel = block + (py * 4 + px) * 4;

					pDstPixel[0] = pSrcPixel[0]; // R
					pDstPixel[1] = pSrcPixel[1]; // G
					pDstPixel[2] = pSrcPixel[2]; // B
					pDstPixel[3] = pSrcPixel[3]; // A

					// Extract alpha for BC4 compression
					alphaBlock[py * 4 + px] = pSrcPixel[3];
				}
			}

			// BC3 block layout: 8 bytes alpha (BC4) + 8 bytes color (BC1)
			uint8_t* pDstBlock = pOutputData + (by * iBlocksX + bx) * 16;

			// Compress alpha channel with BC4
			stb_compress_bc4_block(pDstBlock, alphaBlock);

			// Compress color with BC1 (alpha=0 means ignore alpha in color compression)
			stb_compress_dxt_block(pDstBlock + 8, block, 0, STB_DXT_HIGHQUAL);
		}
	}
}

// Compress RGBA data to BC5 (two-channel, R+G) — the right format for tangent-
// space normal maps. Each 4x4 block is two BC4 blocks back to back: red (bytes
// 0..7) then green (bytes 8..15), matching VK_FORMAT_BC5_UNORM_BLOCK. The shader
// reconstructs Z from RG (see Common/Material.slang SampleNormalMap).
static void CompressToBC5(const uint8_t* pRGBAData, uint8_t* pOutputData, int32_t iWidth, int32_t iHeight)
{
	const int32_t iBlocksX = (iWidth + 3) / 4;
	const int32_t iBlocksY = (iHeight + 3) / 4;

	uint8_t rBlock[16];
	uint8_t gBlock[16];

	for (int32_t by = 0; by < iBlocksY; by++)
	{
		for (int32_t bx = 0; bx < iBlocksX; bx++)
		{
			for (int32_t py = 0; py < 4; py++)
			{
				for (int32_t px = 0; px < 4; px++)
				{
					int32_t srcX = bx * 4 + px;
					int32_t srcY = by * 4 + py;
					srcX = (srcX < iWidth) ? srcX : (iWidth - 1);   // edge-clamp sub-4x4 mips
					srcY = (srcY < iHeight) ? srcY : (iHeight - 1);

					const uint8_t* pSrcPixel = pRGBAData + (static_cast<size_t>(srcY) * iWidth + srcX) * 4;
					rBlock[py * 4 + px] = pSrcPixel[0]; // R
					gBlock[py * 4 + px] = pSrcPixel[1]; // G
				}
			}

			uint8_t* pDstBlock = pOutputData + (static_cast<size_t>(by) * iBlocksX + bx) * 16;
			stb_compress_bc4_block(pDstBlock, rBlock);     // red  -> bytes 0..7
			stb_compress_bc4_block(pDstBlock + 8, gBlock); // green-> bytes 8..15
		}
	}
}

// ---------------------------------------------------------------------------
// Mip chain construction
// ---------------------------------------------------------------------------

// sRGB <-> linear, the piecewise IEC 61966-2-1 curve (NOT a 2.2 power). The
// decode side is a 256-entry table because the downsampler hits it 12 times per
// output texel.
static float SRGB8ToLinear(uint8_t u)
{
	static float s_afTable[256];
	static bool s_bBuilt = false;
	if (!s_bBuilt)
	{
		for (int32_t i = 0; i < 256; i++)
		{
			const float fC = static_cast<float>(i) / 255.0f;
			s_afTable[i] = (fC <= 0.04045f) ? (fC / 12.92f) : std::pow((fC + 0.055f) / 1.055f, 2.4f);
		}
		s_bBuilt = true;
	}
	return s_afTable[u];
}

static uint8_t LinearToSRGB8(float fLinear)
{
	fLinear = std::clamp(fLinear, 0.0f, 1.0f);
	const float fC = (fLinear <= 0.0031308f) ? (fLinear * 12.92f) : (1.055f * std::pow(fLinear, 1.0f / 2.4f) - 0.055f);
	return static_cast<uint8_t>(std::clamp(static_cast<int32_t>(fC * 255.0f + 0.5f), 0, 255));
}

// Box-downsample an RGBA8 image to half size (min 1px each axis), 2x2 average.
// bSRGB: the RGB channels are sRGB-encoded, so they are decoded to linear,
// averaged, and re-encoded -- a byte average of sRGB is an average of the
// ENCODED values and darkens every mip (a 50/50 mix of black and white must land
// at sRGB ~188, not 128). Alpha is linear coverage in both cases.
static void DownsampleBoxRGBA8(const uint8_t* pSrc, int32_t iSrcW, int32_t iSrcH,
	std::vector<uint8_t>& xDst, int32_t& iDstW, int32_t& iDstH, bool bSRGB)
{
	iDstW = std::max(1, iSrcW / 2);
	iDstH = std::max(1, iSrcH / 2);
	xDst.resize(static_cast<size_t>(iDstW) * iDstH * 4);

	for (int32_t y = 0; y < iDstH; y++)
	{
		for (int32_t x = 0; x < iDstW; x++)
		{
			const int32_t sx0 = std::min(x * 2, iSrcW - 1);
			const int32_t sy0 = std::min(y * 2, iSrcH - 1);
			const int32_t sx1 = std::min(sx0 + 1, iSrcW - 1);
			const int32_t sy1 = std::min(sy0 + 1, iSrcH - 1);
			const uint8_t* apSrc[4] =
			{
				pSrc + (static_cast<size_t>(sy0) * iSrcW + sx0) * 4,
				pSrc + (static_cast<size_t>(sy0) * iSrcW + sx1) * 4,
				pSrc + (static_cast<size_t>(sy1) * iSrcW + sx0) * 4,
				pSrc + (static_cast<size_t>(sy1) * iSrcW + sx1) * 4,
			};
			uint8_t* pDst = &xDst[(static_cast<size_t>(y) * iDstW + x) * 4];
			for (int32_t c = 0; c < 3; c++)
			{
				if (bSRGB)
				{
					const float fSum = SRGB8ToLinear(apSrc[0][c]) + SRGB8ToLinear(apSrc[1][c])
						+ SRGB8ToLinear(apSrc[2][c]) + SRGB8ToLinear(apSrc[3][c]);
					pDst[c] = LinearToSRGB8(fSum * 0.25f);
				}
				else
				{
					const uint32_t uSum = apSrc[0][c] + apSrc[1][c] + apSrc[2][c] + apSrc[3][c];
					pDst[c] = static_cast<uint8_t>((uSum + 2) / 4);
				}
			}
			const uint32_t uSumA = apSrc[0][3] + apSrc[1][3] + apSrc[2][3] + apSrc[3][3];
			pDst[3] = static_cast<uint8_t>((uSumA + 2) / 4);
		}
	}
}

// Fraction of texels whose alpha, scaled by fScale, is at or above the cutoff.
static float AlphaCoverage(const uint8_t* pRGBA, size_t ulTexels, float fCutoff, float fScale)
{
	if (ulTexels == 0)
	{
		return 0.0f;
	}
	const int32_t iThreshold = static_cast<int32_t>(fCutoff * 255.0f + 0.5f);
	size_t ulAbove = 0;
	for (size_t i = 0; i < ulTexels; i++)
	{
		const int32_t iA = std::min(255, static_cast<int32_t>(pRGBA[i * 4 + 3] * fScale + 0.5f));
		if (iA >= iThreshold)
		{
			ulAbove++;
		}
	}
	return static_cast<float>(ulAbove) / static_cast<float>(ulTexels);
}

// Castano's coverage-preserving alpha: find the scale that makes this level's
// coverage match fTargetCoverage (coverage is monotonic in the scale, so a
// bisection finds it), and apply it. Without this a box-filtered leaf mask
// loses coverage every level -- the soft edge averages below the cutoff -- and a
// tree thins out with distance until it is a bare trunk.
static void ScaleAlphaToCoverage(std::vector<uint8_t>& xRGBA, float fCutoff, float fTargetCoverage)
{
	const size_t ulTexels = xRGBA.size() / 4;
	float fLo = 0.0f;
	float fHi = 8.0f;
	float fBest = 1.0f;
	float fBestErr = std::fabs(AlphaCoverage(xRGBA.data(), ulTexels, fCutoff, 1.0f) - fTargetCoverage);
	for (int32_t i = 0; i < 16; i++)
	{
		const float fMid = 0.5f * (fLo + fHi);
		const float fCov = AlphaCoverage(xRGBA.data(), ulTexels, fCutoff, fMid);
		const float fErr = std::fabs(fCov - fTargetCoverage);
		if (fErr < fBestErr)
		{
			fBestErr = fErr;
			fBest = fMid;
		}
		if (fCov < fTargetCoverage)
		{
			fLo = fMid;
		}
		else
		{
			fHi = fMid;
		}
	}
	for (size_t i = 0; i < ulTexels; i++)
	{
		xRGBA[i * 4 + 3] = static_cast<uint8_t>(std::min(255, static_cast<int32_t>(xRGBA[i * 4 + 3] * fBest + 0.5f)));
	}
}

// Build the full RGBA8 mip chain for an image: mip 0 is a copy of the input,
// each later level is the box filter of the previous one (sRGB-aware when
// bSRGB). With fAlphaCoverageCutoff > 0 every level below 0 has its alpha scaled
// so its coverage at that cutoff matches mip 0's; the downsample itself always
// runs on the UNSCALED chain so the scales do not compound level over level.
static void BuildMipChainRGBA8(const uint8_t* pRGBA0, int32_t iWidth, int32_t iHeight, bool bSRGB, float fAlphaCoverageCutoff,
	std::vector<std::vector<uint8_t>>& xMipRGBA, std::vector<int32_t>& aiMipW, std::vector<int32_t>& aiMipH)
{
	const uint32_t uNumMips = static_cast<uint32_t>(std::floor(std::log2(static_cast<double>(std::max(iWidth, iHeight)))) + 1);
	xMipRGBA.assign(uNumMips, {});
	aiMipW.assign(uNumMips, 0);
	aiMipH.assign(uNumMips, 0);
	aiMipW[0] = iWidth;
	aiMipH[0] = iHeight;
	xMipRGBA[0].assign(pRGBA0, pRGBA0 + static_cast<size_t>(iWidth) * iHeight * 4);

	const bool bPreserveCoverage = fAlphaCoverageCutoff > 0.0f;
	const float fTargetCoverage = bPreserveCoverage
		? AlphaCoverage(pRGBA0, static_cast<size_t>(iWidth) * iHeight, fAlphaCoverageCutoff, 1.0f) : 0.0f;

	std::vector<uint8_t> xPrevUnscaled;   // the unscaled parent when coverage scaling is on
	for (uint32_t m = 1; m < uNumMips; m++)
	{
		const std::vector<uint8_t>& xParent = (bPreserveCoverage && m > 1) ? xPrevUnscaled : xMipRGBA[m - 1];
		std::vector<uint8_t> xLevel;
		DownsampleBoxRGBA8(xParent.data(), aiMipW[m - 1], aiMipH[m - 1], xLevel, aiMipW[m], aiMipH[m], bSRGB);
		if (bPreserveCoverage)
		{
			xPrevUnscaled = xLevel;
			ScaleAlphaToCoverage(xLevel, fAlphaCoverageCutoff, fTargetCoverage);
		}
		xMipRGBA[m] = std::move(xLevel);
	}
}

// Generate a full mip chain from an RGBA8 mip 0, (optionally BC-)compress each
// level, and write the .ztxtr v2 layout (envelope + header + uNumMips +
// total-size + packed mip0..mipN-1). Per-mip byte sizes come from the shared
// CalculateMipDataSize — the SAME function the loader validates against
// (CalculateTotalMipChainSize) and the GPU upload offsets from — so the packed
// layout is in lockstep with both by construction, not by a parallel table.
static void ExportV2(const uint8_t* pRGBA0, const std::string& strFilename, int32_t iWidth, int32_t iHeight, TextureFormat eFormat, float fAlphaCoverageCutoff)
{
	const bool bCompressed = IsCompressedFormat(eFormat);

	// Build the RGBA8 mip chain in memory (mip 0 = input). The format decides the
	// filter: an sRGB format's bytes are EOTF-encoded and are averaged in linear.
	std::vector<std::vector<uint8_t>> xMipRGBA;
	std::vector<int32_t> aiMipW, aiMipH;
	BuildMipChainRGBA8(pRGBA0, iWidth, iHeight, IsSRGBFormat(eFormat), fAlphaCoverageCutoff, xMipRGBA, aiMipW, aiMipH);
	const uint32_t uNumMips = static_cast<uint32_t>(xMipRGBA.size());

	// Compress (or copy) each mip and concatenate, tightly packed mip0..mipN-1.
	std::vector<uint8_t> xPacked;
	for (uint32_t m = 0; m < uNumMips; m++)
	{
		if (bCompressed)
		{
			// Single source of truth for the per-mip byte count (shared with the
			// loader + GPU upload). The BC compressors below fill exactly
			// ceil(w/4)*ceil(h/4) blocks, which is what this returns.
			std::vector<uint8_t> xMipC(CalculateMipDataSize(eFormat, iWidth, iHeight, m));
			switch (eFormat)
			{
			case TEXTURE_FORMAT_BC1_RGB_UNORM:
			case TEXTURE_FORMAT_BC1_RGB_SRGB:   CompressToBC1(xMipRGBA[m].data(), xMipC.data(), aiMipW[m], aiMipH[m], false); break;
			case TEXTURE_FORMAT_BC1_RGBA_UNORM:
			case TEXTURE_FORMAT_BC1_RGBA_SRGB:  CompressToBC1(xMipRGBA[m].data(), xMipC.data(), aiMipW[m], aiMipH[m], true);  break;
			case TEXTURE_FORMAT_BC3_RGBA_UNORM:
			case TEXTURE_FORMAT_BC3_RGBA_SRGB:  CompressToBC3(xMipRGBA[m].data(), xMipC.data(), aiMipW[m], aiMipH[m]);        break;
			case TEXTURE_FORMAT_BC5_RG_UNORM:   CompressToBC5(xMipRGBA[m].data(), xMipC.data(), aiMipW[m], aiMipH[m]);        break;
			default: Zenith_Assert(false, "ExportV2: unsupported compressed format %d", static_cast<int>(eFormat)); break;
			}
			xPacked.insert(xPacked.end(), xMipC.begin(), xMipC.end());
		}
		else
		{
			xPacked.insert(xPacked.end(), xMipRGBA[m].begin(), xMipRGBA[m].end());
		}
	}

	Zenith_DataStream xStream;
	Zenith_WriteStreamHeader(xStream, uZENITH_TEXTURE_ASSET_TYPE_ID, uZENITH_TEXTURE_SCHEMA_V2);
	xStream << iWidth;
	xStream << iHeight;
	xStream << static_cast<int32_t>(1); // depth
	xStream << eFormat;
	xStream << uNumMips;
	xStream << static_cast<size_t>(xPacked.size());
	xStream.WriteData(xPacked.data(), xPacked.size());
	xStream.WriteToFile(strFilename.c_str());

	Zenith_Log(LOG_CATEGORY_TOOLS, "Exported v2 texture %s: %dx%d fmt %d, %u mips, %zu bytes",
		strFilename.c_str(), iWidth, iHeight, static_cast<int>(eFormat), uNumMips, xPacked.size());
}

void Zenith_Tools_TextureExport::ExportFromFile(std::string strFilename, const char* szExtension, TextureCompressionMode eCompression, TextureColourSpace eColourSpace)
{
	int32_t iWidth, iHeight, iNumChannels;
	uint8_t* pData = stbi_load(strFilename.c_str(), &iWidth, &iHeight, &iNumChannels, STBI_rgb_alpha);

	if (!pData)
	{
		Zenith_Log(LOG_CATEGORY_TOOLS, "Failed to load texture: %s", strFilename.c_str());
		return;
	}

	size_t ulFindPos = strFilename.find(szExtension);
	strFilename.replace(ulFindPos-1, strlen(szExtension)+1, ZENITH_TEXTURE_EXT);

	// Detect alpha channel - use BC3 instead of BC1 if source has alpha
	bool bHasAlpha = (iNumChannels == 4);

	if (eCompression == TextureCompressionMode::Uncompressed)
	{
		ExportFromData(pData, strFilename, iWidth, iHeight, ResolveExportFormat(TextureCompressionMode::Uncompressed, eColourSpace));
	}
	else
	{
		// Upgrade BC1 to BC3 for textures with alpha
		TextureCompressionMode eFinalCompression = eCompression;
		if (bHasAlpha && eCompression == TextureCompressionMode::BC1)
		{
			eFinalCompression = TextureCompressionMode::BC3;
			Zenith_Log(LOG_CATEGORY_TOOLS, "Texture '%s' has alpha - using BC3 compression", strFilename.c_str());
		}
		ExportFromDataCompressed(pData, strFilename, iWidth, iHeight, eFinalCompression, eColourSpace);
	}

	stbi_image_free(pData);
}

void Zenith_Tools_TextureExport::ExportFromData(const void* pData, const std::string& strFilename, int32_t iWidth, int32_t iHeight, TextureFormat eFormat)
{
	// ★★ THIS WROTE A HEADERLESS FILE, AND IT WAS THE LAST PRODUCER OF ONE. The
	// bytes went out with no envelope at all -- straight to iWidth -- and the
	// loader recognised them only through a "no magic => assume the old layout"
	// branch. There is now exactly ONE .ztxtr layout: envelope, dimensions,
	// format, mip count, total size, packed levels. This writes that layout with a
	// single level, which is a legal chain of length one rather than a second
	// format.
	//
	// ★ SINGLE-MIP IS A REAL REQUIREMENT, not a shortcut, which is why the format
	// carries a COUNT rather than assuming a full chain. The MSDF font atlas must
	// not have mips at all -- naive downsampling breaks the median reconstruction
	// the text shader depends on (Zenith_FontAsset says so at the CreateFromData
	// call). Anything that is sampled minified belongs on the V2 chain writers.
	const size_t ulDataSize = static_cast<size_t>(iWidth) * static_cast<size_t>(iHeight)
		* 1 /*depth*/ * 4 /*bytes per pixel*/;

	Zenith_DataStream xStream;
	Zenith_WriteStreamHeader(xStream, uZENITH_TEXTURE_ASSET_TYPE_ID, uZENITH_TEXTURE_SCHEMA_V2);
	xStream << iWidth;
	xStream << iHeight;
	xStream << static_cast<int32_t>(1); // depth
	xStream << eFormat;
	xStream << static_cast<uint32_t>(1); // mip count -- this level only
	xStream << ulDataSize;
	xStream.WriteData(pData, ulDataSize);
	xStream.WriteToFile(strFilename.c_str());
}

void Zenith_Tools_TextureExport::ExportFromDataCompressed(const void* pRGBAData, const std::string& strFilename, int32_t iWidth, int32_t iHeight,
	TextureCompressionMode eCompression, TextureColourSpace eColourSpace, float fAlphaCoverageCutoff)
{
	// All compressed textures ship as .ztxtr v2 with a full, offline-baked mip
	// chain — BC formats can't be runtime blit-generated, so the mips MUST be in
	// the asset. ExportV2 owns mip generation, per-level BC compression (incl.
	// real BC5 for normal maps and BC1 punch-through), and the v2 on-disk layout.
	// The colour space picks the sRGB twin of the block format, so the bytes an
	// artist authored in sRGB are decoded by the SAMPLER and reach the shader
	// linear -- no shader-side pow, no baked-in OETF hack.
	const TextureFormat eFormat = ResolveExportFormat(eCompression, eColourSpace);
	ExportV2(static_cast<const uint8_t*>(pRGBAData), strFilename, iWidth, iHeight, eFormat, fAlphaCoverageCutoff);
}

void Zenith_Tools_TextureExport::ExportFromDataV2Uncompressed(const void* pRGBAData, const std::string& strFilename, int32_t iWidth, int32_t iHeight,
	TextureFormat eFormat, float fAlphaCoverageCutoff)
{
	// Same v2 offline-mip pipeline as the compressed path, but the format stays
	// uncompressed: ExportV2's non-compressed branch copies each box-downsampled
	// mip verbatim and writes the v2 envelope the loader validates + the PREBAKED
	// upload reads (it handles uncompressed formats via ColourFormatBytesPerPixel).
	// The colour space is the format's own (RGBA8_SRGB filters in linear).
	Zenith_Assert(!Zenith_Tools_TextureExport::IsCompressedFormat(eFormat), "ExportFromDataV2Uncompressed: format must be uncompressed");
	Zenith_Assert(ColourFormatBytesPerPixel(eFormat) == 4u, "ExportFromDataV2Uncompressed: the mip builder filters RGBA8 only");
	ExportV2(static_cast<const uint8_t*>(pRGBAData), strFilename, iWidth, iHeight, eFormat, fAlphaCoverageCutoff);
}

void Zenith_Tools_TextureExport::ExportFromDataWithFormat(const void* pData, const std::string& strFilename, int32_t iWidth, int32_t iHeight, TextureFormat eFormat, size_t ulBytesPerPixel)
{
	// ONE level, explicit bytes-per-pixel: for payloads the RGBA8 mip builder
	// cannot filter (R16_UNORM / R32_SFLOAT heightmaps, which are read by the
	// terrain tools at texel precision rather than sampled minified). A
	// four-byte colour or data map does not belong here -- it ships a single mip
	// and shimmers at distance; use ExportFromDataCompressed / V2Uncompressed.
	const size_t ulDataSize = static_cast<size_t>(iWidth) * iHeight * ulBytesPerPixel;

	// Same single .ztxtr layout as every other writer: envelope, dimensions,
	// format, level count, total size, packed levels — one level here.
	Zenith_DataStream xStream;
	Zenith_WriteStreamHeader(xStream, uZENITH_TEXTURE_ASSET_TYPE_ID, uZENITH_TEXTURE_SCHEMA_V2);
	xStream << iWidth;
	xStream << iHeight;
	xStream << static_cast<int32_t>(1); // depth
	xStream << eFormat;
	xStream << static_cast<uint32_t>(1); // this level only
	xStream << ulDataSize;
	xStream.WriteData(pData, ulDataSize);
	xStream.WriteToFile(strFilename.c_str());

	Zenith_Log(LOG_CATEGORY_TOOLS, "Exported texture %s: %dx%d, format %d, %zu bytes",
		strFilename.c_str(), iWidth, iHeight, static_cast<int>(eFormat), ulDataSize);
}

// Dispatch an RGBA8 buffer to the (un)compressed exporter based on eCompression.
// Heightmap-path data: always Linear.
static void ExportRGBA8Buffer(const uint8_t* puRGBA, const std::string& strOutputFilename, int32_t iWidth, int32_t iHeight, TextureCompressionMode eCompression)
{
	if (eCompression == TextureCompressionMode::Uncompressed)
	{
		Zenith_Tools_TextureExport::ExportFromData(puRGBA, strOutputFilename, iWidth, iHeight, TEXTURE_FORMAT_RGBA8_UNORM);
	}
	else
	{
		Zenith_Tools_TextureExport::ExportFromDataCompressed(puRGBA, strOutputFilename, iWidth, iHeight, eCompression, TextureColourSpace::Linear);
	}
}

void Zenith_Tools_TextureExport::ExportFromHeightmapImageFile(const std::string& strFilename, TextureCompressionMode eCompression)
{
	// Generate output filename (replace extension with .ztxtr)
	std::string strOutputFilename = strFilename;
	size_t ulDotPos = strOutputFilename.rfind('.');
	if (ulDotPos != std::string::npos)
	{
		strOutputFilename = strOutputFilename.substr(0, ulDotPos) + ZENITH_TEXTURE_EXT;
	}
	else
	{
		strOutputFilename += ZENITH_TEXTURE_EXT;
	}

	int32_t iWidth = 0, iHeight = 0, iChannels = 0;

	// Decode by source bit depth, preserving single-channel heightmap precision.
	// stb returns RGB(A) natively (unlike OpenCV's BGR), so no channel swap is needed.
	if (stbi_is_hdr(strFilename.c_str()))
	{
		float* pfData = stbi_loadf(strFilename.c_str(), &iWidth, &iHeight, &iChannels, 0);
		if (!pfData)
		{
			Zenith_Log(LOG_CATEGORY_TOOLS, "Failed to load image: %s", strFilename.c_str());
			return;
		}
		Zenith_Log(LOG_CATEGORY_TOOLS, "Exporting image %s: %dx%d, 32-bit float, channels=%d",
			strFilename.c_str(), iWidth, iHeight, iChannels);

		if (iChannels == 1)
		{
			// 32-bit float single channel (heightmap)
			ExportFromDataWithFormat(pfData, strOutputFilename, iWidth, iHeight, TEXTURE_FORMAT_R32_SFLOAT, sizeof(float));
		}
		else
		{
			const size_t ulCount = static_cast<size_t>(iWidth) * iHeight;
			uint8_t* puRGBA = new uint8_t[ulCount * 4];
			for (size_t i = 0; i < ulCount; i++)
			{
				for (int c = 0; c < 4; c++)
				{
					float fVal = (c < iChannels) ? pfData[i * iChannels + c] : (c == 3 ? 1.0f : 0.0f);
					int iVal = static_cast<int>(fVal * 255.0f + 0.5f);
					puRGBA[i * 4 + c] = static_cast<uint8_t>(iVal < 0 ? 0 : (iVal > 255 ? 255 : iVal));
				}
			}
			ExportRGBA8Buffer(puRGBA, strOutputFilename, iWidth, iHeight, eCompression);
			delete[] puRGBA;
		}
		stbi_image_free(pfData);
	}
	else if (stbi_is_16_bit(strFilename.c_str()))
	{
		uint16_t* pu16 = stbi_load_16(strFilename.c_str(), &iWidth, &iHeight, &iChannels, 0);
		if (!pu16)
		{
			Zenith_Log(LOG_CATEGORY_TOOLS, "Failed to load image: %s", strFilename.c_str());
			return;
		}
		Zenith_Log(LOG_CATEGORY_TOOLS, "Exporting image %s: %dx%d, 16-bit, channels=%d",
			strFilename.c_str(), iWidth, iHeight, iChannels);

		if (iChannels == 1)
		{
			// 16-bit unsigned single channel (heightmap)
			ExportFromDataWithFormat(pu16, strOutputFilename, iWidth, iHeight, TEXTURE_FORMAT_R16_UNORM, sizeof(uint16_t));
		}
		else
		{
			const size_t ulCount = static_cast<size_t>(iWidth) * iHeight;
			uint8_t* puRGBA = new uint8_t[ulCount * 4];
			for (size_t i = 0; i < ulCount; i++)
			{
				for (int c = 0; c < 4; c++)
				{
					if (c < iChannels)
						puRGBA[i * 4 + c] = static_cast<uint8_t>(pu16[i * iChannels + c] >> 8);
					else
						puRGBA[i * 4 + c] = (c == 3) ? 255 : 0;
				}
			}
			ExportRGBA8Buffer(puRGBA, strOutputFilename, iWidth, iHeight, eCompression);
			delete[] puRGBA;
		}
		stbi_image_free(pu16);
	}
	else
	{
		// 8-bit: force RGBA8 (covers gray/RGB/RGBA), matching the old GRAY2RGBA / RGB(A)2RGBA paths.
		uint8_t* puData = stbi_load(strFilename.c_str(), &iWidth, &iHeight, &iChannels, STBI_rgb_alpha);
		if (!puData)
		{
			Zenith_Log(LOG_CATEGORY_TOOLS, "Failed to load image: %s", strFilename.c_str());
			return;
		}
		Zenith_Log(LOG_CATEGORY_TOOLS, "Exporting image %s: %dx%d, 8-bit, channels=%d",
			strFilename.c_str(), iWidth, iHeight, iChannels);

		ExportRGBA8Buffer(puData, strOutputFilename, iWidth, iHeight, eCompression);
		stbi_image_free(puData);
	}

	Zenith_Log(LOG_CATEGORY_TOOLS, "Image export complete: %s -> %s", strFilename.c_str(), strOutputFilename.c_str());
}

// One walked asset root: every PNG/JPG under it, exported with the usage its
// TextureUsage.ztexdecl DECLARES.
//
// ★ AN UNDECLARED TEXTURE IS NOT EXPORTED, AND SAYS SO. The alternative is to
// pick a default, which is exactly the shape of the bug this replaced: the old
// walk assumed BC1/linear for everything, the newer one inferred BC5 from the
// substring "normal", and both were silent. A missing .ztxtr surfaces at the
// first load with the file's name in it; a wrongly-encoded one surfaces as a
// rendering artefact nobody can trace back here.
static void ExportTextureRoot(const std::string& strRootDir)
{
	if (!std::filesystem::exists(strRootDir))
	{
		// A fresh clone / CI has no Assets tree at all -- nothing to declare and
		// nothing to export. recursive_directory_iterator would throw.
		return;
	}

	// Collect the source textures first, so a missing manifest can name what it
	// would have had to describe instead of just "some directory".
	std::vector<std::filesystem::path> xSources;
	for (const std::filesystem::directory_entry& xFile : std::filesystem::recursive_directory_iterator(strRootDir))
	{
		if (!xFile.is_regular_file()) continue;
		std::string strExt = xFile.path().extension().string();
		std::transform(strExt.begin(), strExt.end(), strExt.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		if (strExt == ".png" || strExt == ".jpg" || strExt == ".jpeg")
			xSources.push_back(xFile.path());
	}
	if (xSources.empty()) return;

	Zenith_Tools_TextureExport::TextureUsageManifest xManifest;
	if (!xManifest.LoadForRoot(strRootDir))
	{
		Zenith_Error(LOG_CATEGORY_TOOLS, "[TextureExport] %zu source textures under '%s' but the usage declarations could not be read: %s. "
			"Every texture's colour space and compression is DECLARED, never inferred -- add %s.",
			xSources.size(), strRootDir.c_str(), xManifest.GetError().c_str(),
			Zenith_Tools_TextureExport::TextureUsageManifest::Filename());
		return;
	}

	uint32_t uExported = 0u;
	uint32_t uUndeclared = 0u;
	for (const std::filesystem::path& xPath : xSources)
	{
		const std::string strRelative =
			std::filesystem::relative(xPath, std::filesystem::path(strRootDir)).generic_string();

		Zenith_Tools_TextureExport::TextureUsage eUsage;
		if (!xManifest.TryGetUsage(strRelative, eUsage))
		{
			// Named individually: a list of files is what somebody has to paste
			// into the manifest, and a count alone makes them go and find them.
			Zenith_Error(LOG_CATEGORY_TOOLS, "[TextureExport] '%s' is not declared in %s/%s -- add a line "
				"'%s <USAGE>' (BASE_COLOUR / BASE_COLOUR_MASKED / NORMAL_MAP / LINEAR_DATA / "
				"UNCOMPRESSED_COLOUR / UNCOMPRESSED_DATA). NOT exported.",
				strRelative.c_str(), strRootDir.c_str(),
				Zenith_Tools_TextureExport::TextureUsageManifest::Filename(), strRelative.c_str());
			++uUndeclared;
			continue;
		}

		TextureCompressionMode eCompression;
		TextureColourSpace eColourSpace;
		Zenith_Tools_TextureExport::ResolveUsage(eUsage, eCompression, eColourSpace);
		Zenith_Tools_TextureExport::ExportFromFile(xPath.string(),
			xPath.extension().string().c_str() + 1, eCompression, eColourSpace);
		++uExported;
	}

	// A declared path with no file is a rename nobody finished. It costs nothing
	// to notice and is otherwise invisible until the .ztxtr it named goes stale.
	for (const std::pair<std::string, Zenith_Tools_TextureExport::TextureUsage>& xEntry : xManifest.GetEntries())
	{
		bool bFound = false;
		for (const std::filesystem::path& xPath : xSources)
		{
			std::string strRelative =
				std::filesystem::relative(xPath, std::filesystem::path(strRootDir)).generic_string();
			std::transform(strRelative.begin(), strRelative.end(), strRelative.begin(),
				[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			if (strRelative == xEntry.first) { bFound = true; break; }
		}
		if (!bFound)
		{
			Zenith_Error(LOG_CATEGORY_TOOLS, "[TextureExport] %s/%s declares '%s' (%s) but no such source texture exists -- stale entry.",
				strRootDir.c_str(), Zenith_Tools_TextureExport::TextureUsageManifest::Filename(),
				xEntry.first.c_str(), Zenith_Tools_TextureExport::TextureUsageToken(xEntry.second));
		}
	}

	Zenith_Log(LOG_CATEGORY_TOOLS, "[TextureExport] %s: %u exported, %u undeclared, %zu declarations",
		strRootDir.c_str(), uExported, uUndeclared, xManifest.GetCount());
}

void ExportAllTextures()
{
	ExportTextureRoot(GetGameAssetsDirectory() + "Textures");
	ExportTextureRoot(GetEngineAssetsDirectory());
}

#include "Zenith_Tools_TextureExport.Tests.inl"
