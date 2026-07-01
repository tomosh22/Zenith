#include "Zenith.h"
#include "Zenith_Tools_TextureExport.h"
// Wave-13 PCH slim round 2: <filesystem> was demoted out of Zenith.h. This TU
// uses std::filesystem (directory iteration in ExportAllTextures below), so it
// carries the explicit include.
#include <filesystem>

// Extern function that must be implemented by game projects - returns just the project name (e.g., "Test")
// Paths are constructed using ZENITH_ROOT (defined by build system) + project name
extern const char* Project_GetName();

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
#include <cmath>
#define STB_IMAGE_IMPLEMENTATION
#pragma warning(push, 0)
#include "stb/stb_image.h"
#define STB_DXT_IMPLEMENTATION
#include "stb/stb_dxt.h"
#pragma warning(pop)

bool Zenith_Tools_TextureExport::IsCompressedFormat(TextureFormat eFormat)
{
	return eFormat == TEXTURE_FORMAT_BC1_RGB_UNORM ||
		   eFormat == TEXTURE_FORMAT_BC1_RGBA_UNORM ||
		   eFormat == TEXTURE_FORMAT_BC3_RGBA_UNORM ||
		   eFormat == TEXTURE_FORMAT_BC5_RG_UNORM ||
		   eFormat == TEXTURE_FORMAT_BC7_RGBA_UNORM;
}

uint32_t Zenith_Tools_TextureExport::GetBytesPerBlockOrPixel(TextureFormat eFormat)
{
	switch (eFormat)
	{
	case TEXTURE_FORMAT_BC1_RGB_UNORM:
	case TEXTURE_FORMAT_BC1_RGBA_UNORM:
		return 8;  // 8 bytes per 4x4 block
	case TEXTURE_FORMAT_BC3_RGBA_UNORM:
	case TEXTURE_FORMAT_BC5_RG_UNORM:
	case TEXTURE_FORMAT_BC7_RGBA_UNORM:
		return 16; // 16 bytes per 4x4 block
	case TEXTURE_FORMAT_RGBA8_UNORM:
	case TEXTURE_FORMAT_BGRA8_UNORM:
		return 4;  // 4 bytes per pixel
	case TEXTURE_FORMAT_RGB8_UNORM:
		return 3;  // 3 bytes per pixel
	default:
		return 4;  // Default to 4 bytes per pixel
	}
}

static TextureFormat CompressionModeToFormat(TextureCompressionMode eCompression)
{
	switch (eCompression)
	{
	case TextureCompressionMode::BC1:
		return TEXTURE_FORMAT_BC1_RGB_UNORM;
	case TextureCompressionMode::BC1_Alpha:
		return TEXTURE_FORMAT_BC1_RGBA_UNORM;
	case TextureCompressionMode::BC3:
		return TEXTURE_FORMAT_BC3_RGBA_UNORM;
	case TextureCompressionMode::BC5:
		return TEXTURE_FORMAT_BC5_RG_UNORM;
	case TextureCompressionMode::Uncompressed:
	default:
		return TEXTURE_FORMAT_RGBA8_UNORM;
	}
}

// Compress RGBA data to BC1 format
static void CompressToBC1(const uint8_t* pRGBAData, uint8_t* pOutputData, int32_t iWidth, int32_t iHeight, bool bHasAlpha)
{
	const int32_t iBlocksX = (iWidth + 3) / 4;
	const int32_t iBlocksY = (iHeight + 3) / 4;
	
	uint8_t block[16 * 4]; // 4x4 block of RGBA pixels
	
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
				}
			}
			
			// Compress block
			uint8_t* pDstBlock = pOutputData + (by * iBlocksX + bx) * 8;
			stb_compress_dxt_block(pDstBlock, block, bHasAlpha ? 0 : 0, STB_DXT_HIGHQUAL);
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

// Box-downsample an RGBA8 image to half size (min 1px each axis). Linear-space
// 2x2 average — correct for linear data (normal/roughness/AO). TODO: sRGB-correct
// downsample for albedo (decode sRGB -> average -> re-encode); the byte average
// below matches the current (pre-mip) albedo look.
static void DownsampleBoxRGBA8(const uint8_t* pSrc, int32_t iSrcW, int32_t iSrcH,
	std::vector<uint8_t>& xDst, int32_t& iDstW, int32_t& iDstH)
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
			for (int32_t c = 0; c < 4; c++)
			{
				const uint32_t uSum =
					pSrc[(static_cast<size_t>(sy0) * iSrcW + sx0) * 4 + c] +
					pSrc[(static_cast<size_t>(sy0) * iSrcW + sx1) * 4 + c] +
					pSrc[(static_cast<size_t>(sy1) * iSrcW + sx0) * 4 + c] +
					pSrc[(static_cast<size_t>(sy1) * iSrcW + sx1) * 4 + c];
				xDst[(static_cast<size_t>(y) * iDstW + x) * 4 + c] = static_cast<uint8_t>((uSum + 2) / 4);
			}
		}
	}
}

// Generate a full mip chain from an RGBA8 mip 0, (optionally BC-)compress each
// level, and write the .ztxtr v2 layout (envelope + header + uNumMips +
// total-size + packed mip0..mipN-1). Per-mip byte sizes come from the shared
// CalculateMipDataSize — the SAME function the loader validates against
// (CalculateTotalMipChainSize) and the GPU upload offsets from — so the packed
// layout is in lockstep with both by construction, not by a parallel table.
static void ExportV2(const uint8_t* pRGBA0, const std::string& strFilename, int32_t iWidth, int32_t iHeight, TextureFormat eFormat)
{
	const bool bCompressed = IsCompressedFormat(eFormat);
	const uint32_t uNumMips = static_cast<uint32_t>(std::floor(std::log2(static_cast<double>(std::max(iWidth, iHeight)))) + 1);

	// Build the RGBA8 mip chain in memory (mip 0 = input).
	std::vector<std::vector<uint8_t>> xMipRGBA(uNumMips);
	std::vector<int32_t> aiMipW(uNumMips), aiMipH(uNumMips);
	aiMipW[0] = iWidth;
	aiMipH[0] = iHeight;
	xMipRGBA[0].assign(pRGBA0, pRGBA0 + static_cast<size_t>(iWidth) * iHeight * 4);
	for (uint32_t m = 1; m < uNumMips; m++)
	{
		DownsampleBoxRGBA8(xMipRGBA[m - 1].data(), aiMipW[m - 1], aiMipH[m - 1], xMipRGBA[m], aiMipW[m], aiMipH[m]);
	}

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
			case TEXTURE_FORMAT_BC1_RGB_UNORM:  CompressToBC1(xMipRGBA[m].data(), xMipC.data(), aiMipW[m], aiMipH[m], false); break;
			case TEXTURE_FORMAT_BC1_RGBA_UNORM: CompressToBC1(xMipRGBA[m].data(), xMipC.data(), aiMipW[m], aiMipH[m], true);  break;
			case TEXTURE_FORMAT_BC3_RGBA_UNORM: CompressToBC3(xMipRGBA[m].data(), xMipC.data(), aiMipW[m], aiMipH[m]);        break;
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

void Zenith_Tools_TextureExport::ExportFromFile(std::string strFilename, const char* szExtension, TextureCompressionMode eCompression)
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
		ExportFromData(pData, strFilename, iWidth, iHeight, TEXTURE_FORMAT_RGBA8_UNORM);
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
		ExportFromDataCompressed(pData, strFilename, iWidth, iHeight, eFinalCompression);
	}

	stbi_image_free(pData);
}

void Zenith_Tools_TextureExport::ExportFromData(const void* pData, const std::string& strFilename, int32_t iWidth, int32_t iHeight, TextureFormat eFormat)
{
	const size_t ulDataSize = iWidth * iHeight * 1 /*depth*/ * 4 /*bytes per pixel*/;

	Zenith_DataStream xStream;
	xStream << iWidth;
	xStream << iHeight;
	xStream << 1;
	xStream << eFormat;
	xStream << ulDataSize;
	xStream.WriteData(pData, ulDataSize);
	xStream.WriteToFile(strFilename.c_str());
}

void Zenith_Tools_TextureExport::ExportFromDataCompressed(const void* pRGBAData, const std::string& strFilename, int32_t iWidth, int32_t iHeight, TextureCompressionMode eCompression)
{
	// All compressed textures ship as .ztxtr v2 with a full, offline-baked mip
	// chain — BC formats can't be runtime blit-generated, so the mips MUST be in
	// the asset. ExportV2 owns mip generation, per-level BC compression (incl.
	// real BC5 for normal maps), and the v2 on-disk layout.
	const TextureFormat eFormat = CompressionModeToFormat(eCompression);
	ExportV2(static_cast<const uint8_t*>(pRGBAData), strFilename, iWidth, iHeight, eFormat);
}

void Zenith_Tools_TextureExport::ExportFromDataV2Uncompressed(const void* pRGBAData, const std::string& strFilename, int32_t iWidth, int32_t iHeight, TextureFormat eFormat)
{
	// Same v2 offline-mip pipeline as the compressed path, but the format stays
	// uncompressed: ExportV2's non-compressed branch copies each box-downsampled
	// mip verbatim and writes the v2 envelope the loader validates + the PREBAKED
	// upload reads (it handles uncompressed formats via ColourFormatBytesPerPixel).
	// Used for the sRGB albedo, which has no BC sRGB equivalent.
	Zenith_Assert(!Zenith_Tools_TextureExport::IsCompressedFormat(eFormat), "ExportFromDataV2Uncompressed: format must be uncompressed");
	ExportV2(static_cast<const uint8_t*>(pRGBAData), strFilename, iWidth, iHeight, eFormat);
}

void Zenith_Tools_TextureExport::ExportFromDataWithFormat(const void* pData, const std::string& strFilename, int32_t iWidth, int32_t iHeight, TextureFormat eFormat, size_t ulBytesPerPixel)
{
	const size_t ulDataSize = static_cast<size_t>(iWidth) * iHeight * ulBytesPerPixel;

	Zenith_DataStream xStream;
	xStream << iWidth;
	xStream << iHeight;
	xStream << 1; // depth
	xStream << eFormat;
	xStream << ulDataSize;
	xStream.WriteData(pData, ulDataSize);
	xStream.WriteToFile(strFilename.c_str());

	Zenith_Log(LOG_CATEGORY_TOOLS, "Exported texture %s: %dx%d, format %d, %zu bytes",
		strFilename.c_str(), iWidth, iHeight, static_cast<int>(eFormat), ulDataSize);
}

// Dispatch an RGBA8 buffer to the (un)compressed exporter based on eCompression.
static void ExportRGBA8Buffer(const uint8_t* puRGBA, const std::string& strOutputFilename, int32_t iWidth, int32_t iHeight, TextureCompressionMode eCompression)
{
	if (eCompression == TextureCompressionMode::Uncompressed)
	{
		Zenith_Tools_TextureExport::ExportFromData(puRGBA, strOutputFilename, iWidth, iHeight, TEXTURE_FORMAT_RGBA8_UNORM);
	}
	else
	{
		Zenith_Tools_TextureExport::ExportFromDataCompressed(puRGBA, strOutputFilename, iWidth, iHeight, eCompression);
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

void ExportTexture(const std::filesystem::directory_entry& xFile)
{
	std::string strFilename = xFile.path().string();

	const char* aszExtensions[] =
	{
		"png",
		"jpg",
		"jpeg"
	};
	for (const char* szExt : aszExtensions)
	{
		if (!strcmp(strFilename.c_str() + strFilename.length() - strlen(szExt), szExt))
		{
			// Use BC1 compression by default for better performance
			Zenith_Tools_TextureExport::ExportFromFile(strFilename, szExt, TextureCompressionMode::BC1);
		}
	}
}

void ExportAllTextures()
{
	std::string strGameTexturesDir = GetGameAssetsDirectory() + "Textures";
	for (const std::filesystem::directory_entry& xFile : std::filesystem::recursive_directory_iterator(strGameTexturesDir))
	{
		ExportTexture(xFile);
	}
	for (const std::filesystem::directory_entry& xFile : std::filesystem::recursive_directory_iterator(GetEngineAssetsDirectory()))
	{
		ExportTexture(xFile);
	}
}