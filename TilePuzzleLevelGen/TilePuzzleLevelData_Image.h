#pragma once
/**
 * TilePuzzleLevelData_Image.h - PNG image generation for TilePuzzleLevelData
 *
 * Renders a top-down pixel image of the level using stb_image_write.h.
 * Color scheme matches in-game materials from TilePuzzle.cpp.
 */

#include <cstdint>
#include <vector>
#include <cmath>

#include "Components/TilePuzzle_Types.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace TilePuzzleLevelImage
{
	static constexpr uint32_t s_uCellSize = 32;
	static constexpr uint32_t s_uGridLineWidth = 1;

	struct Color
	{
		uint8_t r, g, b;
	};

	static constexpr Color s_xEmptyColor = {25, 25, 38};
	static constexpr Color s_xFloorColor = {77, 77, 89};
	static constexpr Color s_xGridLineColor = {40, 40, 55};
	static constexpr Color s_xBlockerColor = {80, 50, 30};

	static Color GetShapeColor(TilePuzzleColor eColor)
	{
		switch (eColor)
		{
		case TILEPUZZLE_COLOR_RED:    return {230, 60, 60};
		case TILEPUZZLE_COLOR_GREEN:  return {60, 200, 60};
		case TILEPUZZLE_COLOR_BLUE:   return {60, 100, 230};
		case TILEPUZZLE_COLOR_YELLOW: return {230, 230, 60};
		case TILEPUZZLE_COLOR_PURPLE: return {180, 60, 230};
		case TILEPUZZLE_COLOR_NONE:   return s_xBlockerColor;
		default:                      return {128, 128, 128};
		}
	}

	static Color GetCatColor(TilePuzzleColor eColor)
	{
		// Slightly brighter than shapes for visibility
		switch (eColor)
		{
		case TILEPUZZLE_COLOR_RED:    return {255, 100, 100};
		case TILEPUZZLE_COLOR_GREEN:  return {100, 255, 100};
		case TILEPUZZLE_COLOR_BLUE:   return {100, 150, 255};
		case TILEPUZZLE_COLOR_YELLOW: return {255, 255, 100};
		case TILEPUZZLE_COLOR_PURPLE: return {220, 100, 255};
		default:                      return {200, 200, 200};
		}
	}

	static void SetPixel(std::vector<uint8_t>& axPixels, uint32_t uImgWidth, uint32_t uX, uint32_t uY, Color xColor)
	{
		uint32_t uIdx = (uY * uImgWidth + uX) * 3;
		axPixels[uIdx + 0] = xColor.r;
		axPixels[uIdx + 1] = xColor.g;
		axPixels[uIdx + 2] = xColor.b;
	}

	static void FillRect(std::vector<uint8_t>& axPixels, uint32_t uImgWidth,
		uint32_t uX, uint32_t uY, uint32_t uW, uint32_t uH, Color xColor)
	{
		for (uint32_t dy = 0; dy < uH; ++dy)
		{
			for (uint32_t dx = 0; dx < uW; ++dx)
			{
				SetPixel(axPixels, uImgWidth, uX + dx, uY + dy, xColor);
			}
		}
	}

	static void DrawDiamond(std::vector<uint8_t>& axPixels, uint32_t uImgWidth,
		uint32_t uCenterX, uint32_t uCenterY, uint32_t uRadius, Color xColor)
	{
		for (int32_t dy = -static_cast<int32_t>(uRadius); dy <= static_cast<int32_t>(uRadius); ++dy)
		{
			for (int32_t dx = -static_cast<int32_t>(uRadius); dx <= static_cast<int32_t>(uRadius); ++dx)
			{
				if (static_cast<uint32_t>(abs(dx) + abs(dy)) <= uRadius)
				{
					SetPixel(axPixels, uImgWidth,
						uCenterX + dx, uCenterY + dy, xColor);
				}
			}
		}
	}

	static void Write(const char* szPath, const TilePuzzleLevelData& xLevel)
	{
		uint32_t uImgWidth = xLevel.uGridWidth * s_uCellSize + (xLevel.uGridWidth + 1) * s_uGridLineWidth;
		uint32_t uImgHeight = xLevel.uGridHeight * s_uCellSize + (xLevel.uGridHeight + 1) * s_uGridLineWidth;

		std::vector<uint8_t> axPixels(uImgWidth * uImgHeight * 3);

		// Fill background with grid line color
		for (uint32_t i = 0; i < uImgWidth * uImgHeight; ++i)
		{
			axPixels[i * 3 + 0] = s_xGridLineColor.r;
			axPixels[i * 3 + 1] = s_xGridLineColor.g;
			axPixels[i * 3 + 2] = s_xGridLineColor.b;
		}

		// Draw cells
		for (uint32_t y = 0; y < xLevel.uGridHeight; ++y)
		{
			for (uint32_t x = 0; x < xLevel.uGridWidth; ++x)
			{
				uint32_t uPixelX = s_uGridLineWidth + x * (s_uCellSize + s_uGridLineWidth);
				uint32_t uPixelY = s_uGridLineWidth + y * (s_uCellSize + s_uGridLineWidth);

				uint32_t uCellIdx = y * xLevel.uGridWidth + x;
				Color xCellColor = (uCellIdx < xLevel.aeCells.size() && xLevel.aeCells[uCellIdx] == TILEPUZZLE_CELL_FLOOR)
					? s_xFloorColor : s_xEmptyColor;

				FillRect(axPixels, uImgWidth, uPixelX, uPixelY, s_uCellSize, s_uCellSize, xCellColor);
			}
		}

		// Draw shapes
		for (size_t i = 0; i < xLevel.axShapes.size(); ++i)
		{
			const TilePuzzleShapeInstance& xShape = xLevel.axShapes[i];
			if (!xShape.pxDefinition)
				continue;

			Color xColor = GetShapeColor(xShape.eColor);

			for (size_t c = 0; c < xShape.pxDefinition->axCells.size(); ++c)
			{
				int32_t iCellX = xShape.iOriginX + xShape.pxDefinition->axCells[c].iX;
				int32_t iCellY = xShape.iOriginY + xShape.pxDefinition->axCells[c].iY;

				if (iCellX < 0 || iCellY < 0 ||
					static_cast<uint32_t>(iCellX) >= xLevel.uGridWidth ||
					static_cast<uint32_t>(iCellY) >= xLevel.uGridHeight)
					continue;

				uint32_t uPixelX = s_uGridLineWidth + static_cast<uint32_t>(iCellX) * (s_uCellSize + s_uGridLineWidth);
				uint32_t uPixelY = s_uGridLineWidth + static_cast<uint32_t>(iCellY) * (s_uCellSize + s_uGridLineWidth);

				// Draw shape cell with 2px inset for visual clarity
				FillRect(axPixels, uImgWidth, uPixelX + 2, uPixelY + 2, s_uCellSize - 4, s_uCellSize - 4, xColor);
			}
		}

		// Draw cats as diamond markers
		for (size_t i = 0; i < xLevel.axCats.size(); ++i)
		{
			const TilePuzzleCatData& xCat = xLevel.axCats[i];

			if (xCat.iGridX < 0 || xCat.iGridY < 0 ||
				static_cast<uint32_t>(xCat.iGridX) >= xLevel.uGridWidth ||
				static_cast<uint32_t>(xCat.iGridY) >= xLevel.uGridHeight)
				continue;

			uint32_t uPixelX = s_uGridLineWidth + static_cast<uint32_t>(xCat.iGridX) * (s_uCellSize + s_uGridLineWidth);
			uint32_t uPixelY = s_uGridLineWidth + static_cast<uint32_t>(xCat.iGridY) * (s_uCellSize + s_uGridLineWidth);

			uint32_t uCenterX = uPixelX + s_uCellSize / 2;
			uint32_t uCenterY = uPixelY + s_uCellSize / 2;

			Color xCatColor = GetCatColor(xCat.eColor);
			DrawDiamond(axPixels, uImgWidth, uCenterX, uCenterY, s_uCellSize / 4, xCatColor);
		}

		stbi_write_png(szPath, static_cast<int>(uImgWidth), static_cast<int>(uImgHeight), 3, axPixels.data(), static_cast<int>(uImgWidth * 3));
	}
}
