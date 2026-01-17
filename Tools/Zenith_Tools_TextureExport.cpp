#include "Zenith.h"
#include "Zenith_Tools_TextureExport.h"

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
#include "stb/stb_image.h"
#define STB_DXT_IMPLEMENTATION
#include "stb/stb_dxt.h"
#include <opencv2/opencv.hpp>
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

void Zenith_Tools_TextureExport::ExportFromFile(std::string strFilename, const char* szExtension, TextureCompressionMode eCompression)
{
	int32_t iWidth, iHeight, iNumChannels;
	uint8_t* pData = stbi_load(strFilename.c_str(), &iWidth, &iHeight, &iNumChannels, STBI_rgb_alpha);

	size_t ulFindPos = strFilename.find(szExtension);
	strFilename.replace(ulFindPos-1, strlen(szExtension)+1, ZENITH_TEXTURE_EXT);

	if (eCompression == TextureCompressionMode::Uncompressed)
	{
		ExportFromData(pData, strFilename, iWidth, iHeight, TEXTURE_FORMAT_RGBA8_UNORM);
	}
	else
	{
		ExportFromDataCompressed(pData, strFilename, iWidth, iHeight, eCompression);
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
	case TextureCompressionMode::BC5:
		// BC3 and BC5 not yet implemented - fall through to BC1 for now
		CompressToBC1(static_cast<const uint8_t*>(pRGBAData), pCompressedData, iWidth, iHeight, false);
		Zenith_Warning(LOG_CATEGORY_TOOLS, "BC3/BC5 compression not yet implemented, using BC1");
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

void Zenith_Tools_TextureExport::ExportFromTifFile(const std::string& strFilename, TextureCompressionMode eCompression)
{
	// Load TIF with full bit depth preservation
	cv::Mat xImage = cv::imread(strFilename, cv::IMREAD_ANYDEPTH | cv::IMREAD_ANYCOLOR);
	if (xImage.empty())
	{
		Zenith_Log(LOG_CATEGORY_TOOLS, "Failed to load TIF file: %s", strFilename.c_str());
		return;
	}

	int32_t iWidth = xImage.cols;
	int32_t iHeight = xImage.rows;
	int iDepth = xImage.depth();
	int iChannels = xImage.channels();

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

	Zenith_Log(LOG_CATEGORY_TOOLS, "Exporting TIF %s: %dx%d, depth=%d, channels=%d",
		strFilename.c_str(), iWidth, iHeight, iDepth, iChannels);

	// Determine format and export based on bit depth and channel count
	if (iChannels == 1)
	{
		// Single-channel (heightmap) - preserve bit depth
		if (iDepth == CV_32F)
		{
			// 32-bit float single channel
			ExportFromDataWithFormat(xImage.data, strOutputFilename, iWidth, iHeight,
				TEXTURE_FORMAT_R32_SFLOAT, sizeof(float));
		}
		else if (iDepth == CV_16U)
		{
			// 16-bit unsigned single channel
			ExportFromDataWithFormat(xImage.data, strOutputFilename, iWidth, iHeight,
				TEXTURE_FORMAT_R16_UNORM, sizeof(uint16_t));
		}
		else
		{
			// 8-bit - convert to RGBA8 for compatibility
			cv::Mat xRGBA;
			cv::cvtColor(xImage, xRGBA, cv::COLOR_GRAY2RGBA);
			if (eCompression == TextureCompressionMode::Uncompressed)
			{
				ExportFromData(xRGBA.data, strOutputFilename, iWidth, iHeight, TEXTURE_FORMAT_RGBA8_UNORM);
			}
			else
			{
				ExportFromDataCompressed(xRGBA.data, strOutputFilename, iWidth, iHeight, eCompression);
			}
		}
	}
	else if (iChannels == 3 || iChannels == 4)
	{
		// RGB/RGBA image - convert to RGBA8
		cv::Mat xRGBA;
		if (iChannels == 3)
		{
			cv::cvtColor(xImage, xRGBA, cv::COLOR_BGR2RGBA);
		}
		else
		{
			cv::cvtColor(xImage, xRGBA, cv::COLOR_BGRA2RGBA);
		}

		// Handle higher bit depths by converting to 8-bit
		if (iDepth != CV_8U)
		{
			cv::Mat xConverted;
			if (iDepth == CV_16U)
			{
				xRGBA.convertTo(xConverted, CV_8UC4, 1.0 / 256.0);
			}
			else if (iDepth == CV_32F)
			{
				xRGBA.convertTo(xConverted, CV_8UC4, 255.0);
			}
			else
			{
				xRGBA.convertTo(xConverted, CV_8UC4);
			}
			xRGBA = xConverted;
		}

		if (eCompression == TextureCompressionMode::Uncompressed)
		{
			ExportFromData(xRGBA.data, strOutputFilename, iWidth, iHeight, TEXTURE_FORMAT_RGBA8_UNORM);
		}
		else
		{
			ExportFromDataCompressed(xRGBA.data, strOutputFilename, iWidth, iHeight, eCompression);
		}
	}
	else
	{
		Zenith_Log(LOG_CATEGORY_TOOLS, "Unsupported TIF channel count: %d", iChannels);
		return;
	}

	Zenith_Log(LOG_CATEGORY_TOOLS, "TIF export complete: %s -> %s", strFilename.c_str(), strOutputFilename.c_str());
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