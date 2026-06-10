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
#define STB_IMAGE_IMPLEMENTATION
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#pragma warning(push, 0)
#include "stb/stb_image.h"
#define STB_DXT_IMPLEMENTATION
#include "stb/stb_dxt.h"
#pragma warning(pop)
#include "Memory/Zenith_MemoryManagement_Enabled.h"

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
	TextureFormat eFormat = CompressionModeToFormat(eCompression);
	
	// Calculate compressed data size
	const int32_t iBlocksX = (iWidth + 3) / 4;
	const int32_t iBlocksY = (iHeight + 3) / 4;
	const uint32_t uBytesPerBlock = GetBytesPerBlockOrPixel(eFormat);
	const size_t ulCompressedSize = iBlocksX * iBlocksY * uBytesPerBlock;
	
	// Allocate compressed data buffer
	uint8_t* pCompressedData = new uint8_t[ulCompressedSize];
	
	// Compress based on format
	switch (eCompression)
	{
	case TextureCompressionMode::BC1:
		CompressToBC1(static_cast<const uint8_t*>(pRGBAData), pCompressedData, iWidth, iHeight, false);
		break;
	case TextureCompressionMode::BC1_Alpha:
		CompressToBC1(static_cast<const uint8_t*>(pRGBAData), pCompressedData, iWidth, iHeight, true);
		break;
	case TextureCompressionMode::BC3:
		CompressToBC3(static_cast<const uint8_t*>(pRGBAData), pCompressedData, iWidth, iHeight);
		break;
	case TextureCompressionMode::BC5:
		// BC5 not yet implemented - fall through to BC1 for now
		CompressToBC1(static_cast<const uint8_t*>(pRGBAData), pCompressedData, iWidth, iHeight, false);
		Zenith_Warning(LOG_CATEGORY_TOOLS, "BC5 compression not yet implemented, using BC1");
		eFormat = TEXTURE_FORMAT_BC1_RGB_UNORM;
		break;
	default:
		Zenith_Assert(false, "Unknown compression mode");
		break;
	}
	
	// Write to file
	Zenith_DataStream xStream;
	xStream << iWidth;
	xStream << iHeight;
	xStream << 1; // depth
	xStream << eFormat;
	xStream << ulCompressedSize;
	xStream.WriteData(pCompressedData, ulCompressedSize);
	xStream.WriteToFile(strFilename.c_str());
	
	delete[] pCompressedData;
	
	// Log compression stats
	const size_t ulUncompressedSize = iWidth * iHeight * 4;
	const float fCompressionRatio = static_cast<float>(ulUncompressedSize) / static_cast<float>(ulCompressedSize);
	Zenith_Log(LOG_CATEGORY_TOOLS, "Compressed texture %s: %dx%d, %.1f:1 compression ratio", strFilename.c_str(), iWidth, iHeight, fCompressionRatio);
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