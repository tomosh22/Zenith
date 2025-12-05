#include "Zenith.h"
#include "Zenith_Tools_TextureExport.h"
#include "Flux/Flux.h"
#define STB_IMAGE_IMPLEMENTATION
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "stb/stb_image.h"
#define STB_DXT_IMPLEMENTATION
#include "stb/stb_dxt.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"

static std::string ShaderDataTypeToString(ShaderDataType eType)
{
	switch (eType)
	{
	case SHADER_DATA_TYPE_FLOAT:
		return "Float";
	case SHADER_DATA_TYPE_FLOAT2:
		return "Float2";
	case SHADER_DATA_TYPE_FLOAT3:
		return "Float3";
	case SHADER_DATA_TYPE_FLOAT4:
		return "Float4";
	case SHADER_DATA_TYPE_UINT4:
		return "UInt4";
	default:
		Zenith_Assert(false, "Unknown data type");
		return "";
	}
}

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
	strFilename.replace(ulFindPos, strlen(szExtension), "ztx");

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
		Zenith_Log("Warning: BC3/BC5 compression not yet implemented, using BC1");
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
	Zenith_Log("Compressed texture %s: %dx%d, %.1f:1 compression ratio", strFilename.c_str(), iWidth, iHeight, fCompressionRatio);
}

void ExportTexture(const std::filesystem::directory_entry& xFile)
{
	const wchar_t* wszFilename = xFile.path().c_str();
	size_t ulLength = wcslen(wszFilename);
	char* szFilename = new char[ulLength + 1];
	wcstombs(szFilename, wszFilename, ulLength);
	szFilename[ulLength] = '\0';

	const char* aszExtensions[] =
	{
		"png",
		"jpg",
		"jpeg"
	};
	for (const char* szExt : aszExtensions)
	{
		if (!strcmp(szFilename + strlen(szFilename) - strlen(szExt), szExt))
		{
			std::string strFilename(szFilename);
			// Use BC1 compression by default for better performance
			Zenith_Tools_TextureExport::ExportFromFile(strFilename, szExt, TextureCompressionMode::BC1);
		}
	}
	
	delete[] szFilename;
}

void ExportAllTextures()
{
	for (const std::filesystem::directory_entry& xFile : std::filesystem::recursive_directory_iterator(GAME_ASSETS_DIR"Textures"))
	{
		ExportTexture(xFile);
	}
	for (const std::filesystem::directory_entry& xFile : std::filesystem::recursive_directory_iterator(ENGINE_ASSETS_DIR))
	{
		ExportTexture(xFile);
	}
}