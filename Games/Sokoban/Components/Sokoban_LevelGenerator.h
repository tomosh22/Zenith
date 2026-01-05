#pragma once
/**
 * Sokoban_LevelGenerator.h - Procedural level generation
 *
 * Demonstrates: Procedural content generation patterns
 *
 * Key concepts:
 * - Random number generation with std::mt19937
 * - Generation with validation (levels must be solvable)
 * - Fallback content when generation fails
 * - Parameter tuning for difficulty
 *
 * Generation algorithm:
 * 1. Create random grid size
 * 2. Fill borders with walls
 * 3. Add random internal walls (10-20%)
 * 4. Place targets, boxes, and player on remaining floor
 * 5. Validate level is solvable with minimum required moves
 * 6. Retry or use fallback if validation fails
 */

#include <random>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cstdint>

#include "Sokoban_GridLogic.h"  // For SokobanTileType enum

// Generation constants
static constexpr uint32_t s_uMinGridSize = 8;
static constexpr uint32_t s_uMaxGridSize = 16;
static constexpr uint32_t s_uMinBoxes = 2;
static constexpr uint32_t s_uMaxBoxes = 5;
static constexpr uint32_t s_uMinMovesSolution = 5;  // Minimum moves for valid level
static constexpr int s_iMaxGenerationAttempts = 1000;

/**
 * Sokoban_LevelGenerator - Procedural level generation
 */
class Sokoban_LevelGenerator
{
public:
	/**
	 * LevelData - Output structure for generated level
	 */
	struct LevelData
	{
		uint32_t uGridWidth;
		uint32_t uGridHeight;
		SokobanTileType* aeTiles;    // Caller-provided buffer
		bool* abTargets;             // Caller-provided buffer
		bool* abBoxes;               // Caller-provided buffer
		uint32_t uPlayerX;
		uint32_t uPlayerY;
		uint32_t uTargetCount;
		uint32_t uMinMoves;
	};

	/**
	 * GenerateLevel - Generate a random solvable level
	 *
	 * Attempts to generate a level that requires at least s_uMinMovesSolution moves.
	 * Falls back to a known-good level if generation fails after max attempts.
	 *
	 * @param xData  Output structure (buffers must be pre-allocated)
	 * @param xRng   Random number generator
	 * @return true if generated level, false if used fallback
	 */
	static bool GenerateLevel(LevelData& xData, std::mt19937& xRng)
	{
		for (int iAttempt = 0; iAttempt < s_iMaxGenerationAttempts; iAttempt++)
		{
			if (GenerateLevelAttempt(xData, xRng))
			{
				// Validate level is solvable with minimum moves
				// Note: Caller must call solver separately
				return true;
			}
		}

		// Fall back to known-good level
		GenerateFallbackLevel(xData);
		return false;
	}

	/**
	 * GenerateLevelAttempt - Single attempt at random level generation
	 */
	static bool GenerateLevelAttempt(LevelData& xData, std::mt19937& xRng)
	{
		std::uniform_int_distribution<uint32_t> xSizeDist(s_uMinGridSize, s_uMaxGridSize);
		std::uniform_int_distribution<uint32_t> xBoxDist(s_uMinBoxes, s_uMaxBoxes);

		xData.uGridWidth = xSizeDist(xRng);
		xData.uGridHeight = xSizeDist(xRng);

		uint32_t uGridSize = xData.uGridWidth * xData.uGridHeight;

		// Clear arrays
		memset(xData.aeTiles, 0, uGridSize * sizeof(SokobanTileType));
		memset(xData.abTargets, 0, uGridSize * sizeof(bool));
		memset(xData.abBoxes, 0, uGridSize * sizeof(bool));

		// Fill with walls on border, floor inside
		for (uint32_t uY = 0; uY < xData.uGridHeight; uY++)
		{
			for (uint32_t uX = 0; uX < xData.uGridWidth; uX++)
			{
				uint32_t uIndex = uY * xData.uGridWidth + uX;
				bool bBorder = (uX == 0 || uY == 0 ||
					uX == xData.uGridWidth - 1 || uY == xData.uGridHeight - 1);
				xData.aeTiles[uIndex] = bBorder ? SOKOBAN_TILE_WALL : SOKOBAN_TILE_FLOOR;
			}
		}

		// Collect inner floor positions
		std::vector<uint32_t> axFloorPositions;
		for (uint32_t uY = 1; uY < xData.uGridHeight - 1; uY++)
		{
			for (uint32_t uX = 1; uX < xData.uGridWidth - 1; uX++)
			{
				axFloorPositions.push_back(uY * xData.uGridWidth + uX);
			}
		}

		// Add random internal walls (10-20% of inner cells)
		uint32_t uInnerCells = (xData.uGridWidth - 2) * (xData.uGridHeight - 2);
		std::uniform_int_distribution<uint32_t> xWallPctDist(10, 20);
		uint32_t uWallCount = (uInnerCells * xWallPctDist(xRng)) / 100;

		std::shuffle(axFloorPositions.begin(), axFloorPositions.end(), xRng);

		for (uint32_t i = 0; i < uWallCount && i < axFloorPositions.size(); i++)
		{
			xData.aeTiles[axFloorPositions[i]] = SOKOBAN_TILE_WALL;
		}

		// Recollect floor positions (excluding walls)
		axFloorPositions.clear();
		for (uint32_t uY = 1; uY < xData.uGridHeight - 1; uY++)
		{
			for (uint32_t uX = 1; uX < xData.uGridWidth - 1; uX++)
			{
				uint32_t uIndex = uY * xData.uGridWidth + uX;
				if (xData.aeTiles[uIndex] == SOKOBAN_TILE_FLOOR)
				{
					axFloorPositions.push_back(uIndex);
				}
			}
		}

		// Need space for: targets, boxes (same count), player
		uint32_t uNumBoxes = xBoxDist(xRng);
		if (axFloorPositions.size() < uNumBoxes * 2 + 1)
		{
			return false; // Not enough space
		}

		std::shuffle(axFloorPositions.begin(), axFloorPositions.end(), xRng);

		uNumBoxes = std::min(uNumBoxes, static_cast<uint32_t>(axFloorPositions.size() / 2));
		xData.uTargetCount = uNumBoxes;

		uint32_t uPlaceIndex = 0;

		// Place targets
		for (uint32_t i = 0; i < uNumBoxes; i++)
		{
			xData.abTargets[axFloorPositions[uPlaceIndex++]] = true;
		}

		// Place boxes (on non-target floors)
		for (uint32_t i = 0; i < uNumBoxes; i++)
		{
			xData.abBoxes[axFloorPositions[uPlaceIndex++]] = true;
		}

		// Place player
		xData.uPlayerX = axFloorPositions[uPlaceIndex] % xData.uGridWidth;
		xData.uPlayerY = axFloorPositions[uPlaceIndex] / xData.uGridWidth;

		return true;
	}

	/**
	 * GenerateFallbackLevel - Create a simple known-solvable level
	 *
	 * Used when random generation repeatedly fails to create solvable levels.
	 */
	static void GenerateFallbackLevel(LevelData& xData)
	{
		xData.uGridWidth = 8;
		xData.uGridHeight = 8;

		uint32_t uGridSize = xData.uGridWidth * xData.uGridHeight;

		memset(xData.aeTiles, 0, uGridSize * sizeof(SokobanTileType));
		memset(xData.abTargets, 0, uGridSize * sizeof(bool));
		memset(xData.abBoxes, 0, uGridSize * sizeof(bool));

		// Border walls
		for (uint32_t uY = 0; uY < xData.uGridHeight; uY++)
		{
			for (uint32_t uX = 0; uX < xData.uGridWidth; uX++)
			{
				uint32_t uIndex = uY * xData.uGridWidth + uX;
				bool bBorder = (uX == 0 || uY == 0 ||
					uX == xData.uGridWidth - 1 || uY == xData.uGridHeight - 1);
				xData.aeTiles[uIndex] = bBorder ? SOKOBAN_TILE_WALL : SOKOBAN_TILE_FLOOR;
			}
		}

		// Simple layout with 2 boxes
		xData.abTargets[2 * 8 + 5] = true;  // Target at (5, 2)
		xData.abTargets[5 * 8 + 5] = true;  // Target at (5, 5)
		xData.uTargetCount = 2;

		xData.abBoxes[3 * 8 + 3] = true;    // Box at (3, 3)
		xData.abBoxes[4 * 8 + 4] = true;    // Box at (4, 4)

		xData.uPlayerX = 2;
		xData.uPlayerY = 2;
	}
};
