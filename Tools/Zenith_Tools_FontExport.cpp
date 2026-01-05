#include "Zenith.h"
#include "Zenith_Tools_FontExport.h"
#include "Zenith_Tools_TextureExport.h"
#include "FileAccess/Zenith_FileAccess.h"

#include "Memory/Zenith_MemoryManagement_Disabled.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "assimp/include/stb_truetype.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"

#include <fstream>
#include <vector>
#include <algorithm>

// Extern function that must be implemented by game projects
extern const char* Project_GetName();

// Atlas configuration
static constexpr uint32_t DEFAULT_ATLAS_SIZE = 512;
static constexpr uint32_t GRID_SIZE = 10;  // 10x10 character grid
static constexpr uint32_t FIRST_CHAR = 32;  // Space
static constexpr uint32_t LAST_CHAR = 131;  // Extended ASCII
static constexpr float DEFAULT_FONT_SIZE = 40.0f;  // Pixels
static constexpr float DEFAULT_SHADOW_OFFSET = 2.0f;  // Pixels
static constexpr float SHADOW_ALPHA = 0.7f;  // Shadow transparency

static std::vector<uint8_t> LoadFileToBuffer(const std::string& strPath)
{
	std::ifstream xFile(strPath, std::ios::binary | std::ios::ate);
	if (!xFile.is_open())
	{
		Zenith_Warning(LOG_CATEGORY_TOOLS, "Failed to open font file: %s", strPath.c_str());
		return {};
	}

	std::streamsize ulSize = xFile.tellg();
	xFile.seekg(0, std::ios::beg);

	std::vector<uint8_t> xBuffer(ulSize);
	if (!xFile.read(reinterpret_cast<char*>(xBuffer.data()), ulSize))
	{
		Zenith_Warning(LOG_CATEGORY_TOOLS, "Failed to read font file: %s", strPath.c_str());
		return {};
	}

	return xBuffer;
}

static void BlendPixel(uint8_t* pDst, uint8_t uSrcR, uint8_t uSrcG, uint8_t uSrcB, uint8_t uSrcA)
{
	// Alpha blending: dst = src * srcA + dst * (1 - srcA)
	float fSrcA = uSrcA / 255.0f;
	float fInvSrcA = 1.0f - fSrcA;

	float fDstR = pDst[0] / 255.0f;
	float fDstG = pDst[1] / 255.0f;
	float fDstB = pDst[2] / 255.0f;
	float fDstA = pDst[3] / 255.0f;

	float fOutR = (uSrcR / 255.0f) * fSrcA + fDstR * fInvSrcA;
	float fOutG = (uSrcG / 255.0f) * fSrcA + fDstG * fInvSrcA;
	float fOutB = (uSrcB / 255.0f) * fSrcA + fDstB * fInvSrcA;
	float fOutA = fSrcA + fDstA * fInvSrcA;

	pDst[0] = static_cast<uint8_t>(std::min(fOutR * 255.0f, 255.0f));
	pDst[1] = static_cast<uint8_t>(std::min(fOutG * 255.0f, 255.0f));
	pDst[2] = static_cast<uint8_t>(std::min(fOutB * 255.0f, 255.0f));
	pDst[3] = static_cast<uint8_t>(std::min(fOutA * 255.0f, 255.0f));
}

void Zenith_Tools_FontExport::ExportFromFile(const std::string& strTTFPath, const std::string& strOutputPath)
{
	ExportFromFile(strTTFPath, strOutputPath, DEFAULT_ATLAS_SIZE, DEFAULT_FONT_SIZE, DEFAULT_SHADOW_OFFSET);
}

void Zenith_Tools_FontExport::ExportFromFile(const std::string& strTTFPath, const std::string& strOutputPath,
										   uint32_t uAtlasSize, float fFontSize, float fShadowOffset)
{
	// Load TTF file
	std::vector<uint8_t> xFontBuffer = LoadFileToBuffer(strTTFPath);
	if (xFontBuffer.empty())
	{
		return;
	}

	// Initialize stb_truetype
	stbtt_fontinfo xFontInfo;
	if (!stbtt_InitFont(&xFontInfo, xFontBuffer.data(), 0))
	{
		Zenith_Warning(LOG_CATEGORY_TOOLS, "Failed to initialize font: %s", strTTFPath.c_str());
		return;
	}

	// Calculate scale for desired font size
	float fScale = stbtt_ScaleForPixelHeight(&xFontInfo, fFontSize);

	// Get font metrics
	int32_t iAscent, iDescent, iLineGap;
	stbtt_GetFontVMetrics(&xFontInfo, &iAscent, &iDescent, &iLineGap);
	float fAscent = iAscent * fScale;

	// Calculate cell size
	uint32_t uCellSize = uAtlasSize / GRID_SIZE;

	// Allocate atlas buffer (RGBA)
	uint32_t uAtlasBytes = uAtlasSize * uAtlasSize * 4;
	uint8_t* pAtlas = new uint8_t[uAtlasBytes];
	memset(pAtlas, 0, uAtlasBytes);  // Clear to transparent black

	// Shadow offset in integer pixels
	int32_t iShadowOffsetX = static_cast<int32_t>(fShadowOffset);
	int32_t iShadowOffsetY = static_cast<int32_t>(fShadowOffset);

	// Render each character
	for (uint32_t uChar = FIRST_CHAR; uChar <= LAST_CHAR; uChar++)
	{
		uint32_t uIndex = uChar - FIRST_CHAR;
		uint32_t uCellX = uIndex % GRID_SIZE;
		uint32_t uCellY = uIndex / GRID_SIZE;

		// Get glyph bitmap
		int32_t iWidth, iHeight, iOffsetX, iOffsetY;
		uint8_t* pGlyphBitmap = stbtt_GetCodepointBitmap(&xFontInfo, 0, fScale, uChar,
														&iWidth, &iHeight, &iOffsetX, &iOffsetY);

		if (!pGlyphBitmap || iWidth == 0 || iHeight == 0)
		{
			// Skip empty glyphs (like space)
			if (pGlyphBitmap)
			{
				stbtt_FreeBitmap(pGlyphBitmap, nullptr);
			}
			continue;
		}

		// Calculate glyph position within cell (centered horizontally, baseline aligned)
		int32_t iGlyphCenterX = static_cast<int32_t>(uCellSize) / 2;
		int32_t iGlyphStartX = iGlyphCenterX + iOffsetX - iWidth / 2;
		int32_t iGlyphStartY = static_cast<int32_t>(fAscent) + iOffsetY;

		// Clamp to ensure we don't go outside cell bounds
		iGlyphStartX = std::max(0, iGlyphStartX);
		iGlyphStartY = std::max(0, iGlyphStartY);

		// Calculate cell origin in atlas
		uint32_t uCellOriginX = uCellX * uCellSize;
		uint32_t uCellOriginY = uCellY * uCellSize;

		// First pass: Render shadow (black, offset)
		for (int32_t y = 0; y < iHeight; y++)
		{
			for (int32_t x = 0; x < iWidth; x++)
			{
				uint8_t uAlpha = pGlyphBitmap[y * iWidth + x];
				if (uAlpha == 0) continue;

				int32_t iAtlasX = uCellOriginX + iGlyphStartX + x + iShadowOffsetX;
				int32_t iAtlasY = uCellOriginY + iGlyphStartY + y + iShadowOffsetY;

				// Bounds check
				if (iAtlasX < 0 || iAtlasX >= static_cast<int32_t>(uAtlasSize) ||
					iAtlasY < 0 || iAtlasY >= static_cast<int32_t>(uAtlasSize))
				{
					continue;
				}

				uint8_t* pPixel = pAtlas + (iAtlasY * uAtlasSize + iAtlasX) * 4;
				uint8_t uShadowAlpha = static_cast<uint8_t>(uAlpha * SHADOW_ALPHA);
				BlendPixel(pPixel, 0, 0, 0, uShadowAlpha);  // Black shadow
			}
		}

		// Second pass: Render main glyph (white)
		for (int32_t y = 0; y < iHeight; y++)
		{
			for (int32_t x = 0; x < iWidth; x++)
			{
				uint8_t uAlpha = pGlyphBitmap[y * iWidth + x];
				if (uAlpha == 0) continue;

				int32_t iAtlasX = uCellOriginX + iGlyphStartX + x;
				int32_t iAtlasY = uCellOriginY + iGlyphStartY + y;

				// Bounds check
				if (iAtlasX < 0 || iAtlasX >= static_cast<int32_t>(uAtlasSize) ||
					iAtlasY < 0 || iAtlasY >= static_cast<int32_t>(uAtlasSize))
				{
					continue;
				}

				uint8_t* pPixel = pAtlas + (iAtlasY * uAtlasSize + iAtlasX) * 4;
				BlendPixel(pPixel, 255, 255, 255, uAlpha);  // White glyph
			}
		}

		stbtt_FreeBitmap(pGlyphBitmap, nullptr);
	}

	// Export using texture export system (uncompressed RGBA for proper alpha)
	Zenith_Tools_TextureExport::ExportFromData(pAtlas, strOutputPath, uAtlasSize, uAtlasSize, TEXTURE_FORMAT_RGBA8_UNORM);

	Zenith_Log(LOG_CATEGORY_TOOLS, "Exported font atlas: %s (%ux%u, %u characters)",
			  strOutputPath.c_str(), uAtlasSize, uAtlasSize, LAST_CHAR - FIRST_CHAR + 1);

	delete[] pAtlas;
}

void ExportDefaultFontAtlas()
{
	std::string strFontPath = std::string(ENGINE_ASSETS_DIR) + "Fonts/LiberationMono-Regular.ttf";
	std::string strOutputPath = std::string(ENGINE_ASSETS_DIR) + "Textures/Font/FontAtlas" ZENITH_TEXTURE_EXT;

	Zenith_Tools_FontExport::ExportFromFile(strFontPath, strOutputPath);
}
