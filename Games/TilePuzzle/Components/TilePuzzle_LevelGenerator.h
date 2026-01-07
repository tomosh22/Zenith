#pragma once
/**
 * TilePuzzle_LevelGenerator.h - Procedural level generation
 *
 * Demonstrates: Procedural content generation patterns
 *
 * Key concepts:
 * - Random number generation with std::mt19937
 * - Generation with validation (levels must be solvable)
 * - Fallback content when generation fails
 * - Parameter tuning for difficulty progression
 *
 * Generation algorithm:
 * 1. Create grid of floor cells
 * 2. Place static blockers randomly
 * 3. Place draggable shapes with colors
 * 4. Place cats with matching colors on valid floor cells
 * 5. Validate level is solvable using TilePuzzle_Solver
 * 6. Retry or use fallback if validation fails
 */

#include <random>
#include <vector>
#include <algorithm>
#include <cstdint>

#include "TilePuzzle_Types.h"
#include "TilePuzzle_Solver.h"

// Generation constants
static constexpr uint32_t s_uTilePuzzleMinGridSize = 5;
static constexpr uint32_t s_uTilePuzzleMaxGridSize = 8;
static constexpr int32_t s_iTilePuzzleMaxGenerationAttempts = 100;

/**
 * TilePuzzle_LevelGenerator - Procedural level generation
 *
 * Generates solvable puzzle levels with increasing difficulty.
 */
class TilePuzzle_LevelGenerator
{
public:
	/**
	 * DifficultyParams - Parameters for level difficulty
	 */
	struct DifficultyParams
	{
		uint32_t uMinGridWidth = 5;
		uint32_t uMaxGridWidth = 6;
		uint32_t uMinGridHeight = 5;
		uint32_t uMaxGridHeight = 6;
		uint32_t uNumColors = 2;           // Number of different colors (1-4)
		uint32_t uNumCatsPerColor = 1;     // Cats per color
		uint32_t uNumShapesPerColor = 1;   // Draggable shapes per color
		uint32_t uNumBlockers = 0;         // Static blockers
		uint32_t uMaxShapeSize = 1;        // Max cells per shape (1=single, 2=domino, etc)
	};

	/**
	 * GetDifficultyForLevel - Get difficulty parameters based on level number
	 */
	static DifficultyParams GetDifficultyForLevel(uint32_t uLevelNumber)
	{
		DifficultyParams xParams;

		if (uLevelNumber <= 2)
		{
			// Easy: small grid, 1-2 colors, single-cell shapes
			xParams.uMinGridWidth = 4;
			xParams.uMaxGridWidth = 5;
			xParams.uMinGridHeight = 4;
			xParams.uMaxGridHeight = 5;
			xParams.uNumColors = 1 + (uLevelNumber / 2);
			xParams.uNumCatsPerColor = 1;
			xParams.uNumShapesPerColor = 1;
			xParams.uNumBlockers = 0;
			xParams.uMaxShapeSize = 1;
		}
		else if (uLevelNumber <= 5)
		{
			// Medium: medium grid, 2-3 colors
			xParams.uMinGridWidth = 5;
			xParams.uMaxGridWidth = 6;
			xParams.uMinGridHeight = 5;
			xParams.uMaxGridHeight = 6;
			xParams.uNumColors = 2 + (uLevelNumber - 3) / 2;
			xParams.uNumCatsPerColor = 1;
			xParams.uNumShapesPerColor = 1;
			xParams.uNumBlockers = uLevelNumber - 2;
			xParams.uMaxShapeSize = 2;
		}
		else if (uLevelNumber <= 10)
		{
			// Hard: larger grid, 3-4 colors, multi-cell shapes
			xParams.uMinGridWidth = 6;
			xParams.uMaxGridWidth = 7;
			xParams.uMinGridHeight = 6;
			xParams.uMaxGridHeight = 7;
			xParams.uNumColors = 3 + (uLevelNumber - 6) / 3;
			xParams.uNumCatsPerColor = 1 + (uLevelNumber - 6) / 4;
			xParams.uNumShapesPerColor = 1;
			xParams.uNumBlockers = 2 + (uLevelNumber - 6);
			xParams.uMaxShapeSize = 3;
		}
		else
		{
			// Expert: large grid, 4 colors, complex shapes
			xParams.uMinGridWidth = 7;
			xParams.uMaxGridWidth = 8;
			xParams.uMinGridHeight = 7;
			xParams.uMaxGridHeight = 8;
			xParams.uNumColors = 4;
			xParams.uNumCatsPerColor = 2;
			xParams.uNumShapesPerColor = 1;
			xParams.uNumBlockers = 4 + (uLevelNumber - 11) / 2;
			xParams.uMaxShapeSize = 4;
		}

		// Clamp values
		xParams.uNumColors = std::min(xParams.uNumColors, static_cast<uint32_t>(TILEPUZZLE_COLOR_COUNT));
		xParams.uMaxShapeSize = std::min(xParams.uMaxShapeSize, 4u);

		return xParams;
	}

	/**
	 * GenerateLevel - Generate a random solvable level
	 *
	 * @param xLevelOut   Output level data
	 * @param xRng        Random number generator
	 * @param uLevelNumber Level number for difficulty scaling
	 * @return true if generated level, false if used fallback
	 */
	static bool GenerateLevel(TilePuzzleLevelData& xLevelOut, std::mt19937& xRng, uint32_t uLevelNumber)
	{
		DifficultyParams xParams = GetDifficultyForLevel(uLevelNumber);

		for (int32_t iAttempt = 0; iAttempt < s_iTilePuzzleMaxGenerationAttempts; ++iAttempt)
		{
			xLevelOut = TilePuzzleLevelData();  // Reset

			if (GenerateLevelAttempt(xLevelOut, xRng, xParams))
			{
				// Verify solvability
				int32_t iSolution = TilePuzzle_Solver::SolveLevel(xLevelOut);
				if (iSolution > 0)
				{
					xLevelOut.uMinimumMoves = static_cast<uint32_t>(iSolution);
					return true;
				}
			}
		}

		// Fall back to known-good level
		GenerateFallbackLevel(xLevelOut, uLevelNumber);
		return false;
	}

private:
	// Static shape definitions that persist during level lifetime
	static std::vector<TilePuzzleShapeDefinition>& GetShapeDefinitions()
	{
		static std::vector<TilePuzzleShapeDefinition> s_axShapeDefinitions;
		return s_axShapeDefinitions;
	}

	/**
	 * GenerateLevelAttempt - Single attempt at random level generation
	 */
	static bool GenerateLevelAttempt(
		TilePuzzleLevelData& xLevelOut,
		std::mt19937& xRng,
		const DifficultyParams& xParams)
	{
		// Clear any previous shape definitions
		GetShapeDefinitions().clear();

		// Generate grid dimensions
		std::uniform_int_distribution<uint32_t> xWidthDist(xParams.uMinGridWidth, xParams.uMaxGridWidth);
		std::uniform_int_distribution<uint32_t> xHeightDist(xParams.uMinGridHeight, xParams.uMaxGridHeight);

		xLevelOut.uGridWidth = xWidthDist(xRng);
		xLevelOut.uGridHeight = xHeightDist(xRng);
		uint32_t uGridSize = xLevelOut.uGridWidth * xLevelOut.uGridHeight;

		// Initialize all cells as floor
		xLevelOut.aeCells.resize(uGridSize, TILEPUZZLE_CELL_FLOOR);

		// Create border of empty cells
		for (uint32_t x = 0; x < xLevelOut.uGridWidth; ++x)
		{
			xLevelOut.aeCells[x] = TILEPUZZLE_CELL_EMPTY;  // Top row
			xLevelOut.aeCells[(xLevelOut.uGridHeight - 1) * xLevelOut.uGridWidth + x] = TILEPUZZLE_CELL_EMPTY;  // Bottom row
		}
		for (uint32_t y = 0; y < xLevelOut.uGridHeight; ++y)
		{
			xLevelOut.aeCells[y * xLevelOut.uGridWidth] = TILEPUZZLE_CELL_EMPTY;  // Left column
			xLevelOut.aeCells[y * xLevelOut.uGridWidth + xLevelOut.uGridWidth - 1] = TILEPUZZLE_CELL_EMPTY;  // Right column
		}

		// Collect inner floor positions
		std::vector<std::pair<int32_t, int32_t>> axFloorPositions;
		for (uint32_t y = 1; y < xLevelOut.uGridHeight - 1; ++y)
		{
			for (uint32_t x = 1; x < xLevelOut.uGridWidth - 1; ++x)
			{
				axFloorPositions.push_back({static_cast<int32_t>(x), static_cast<int32_t>(y)});
			}
		}

		if (axFloorPositions.size() < 3)
		{
			return false;  // Grid too small
		}

		std::shuffle(axFloorPositions.begin(), axFloorPositions.end(), xRng);

		size_t uPositionIndex = 0;

		// Place static blockers
		for (uint32_t i = 0; i < xParams.uNumBlockers && uPositionIndex < axFloorPositions.size(); ++i)
		{
			auto [x, y] = axFloorPositions[uPositionIndex++];

			// Create a blocker shape definition
			GetShapeDefinitions().push_back(TilePuzzleShapes::GetSingleShape(false));  // Not draggable
			TilePuzzleShapeDefinition& xBlockerDef = GetShapeDefinitions().back();

			TilePuzzleShapeInstance xBlocker;
			xBlocker.pxDefinition = &xBlockerDef;
			xBlocker.iOriginX = x;
			xBlocker.iOriginY = y;
			xBlocker.eColor = TILEPUZZLE_COLOR_NONE;

			xLevelOut.axShapes.push_back(xBlocker);
		}

		// Place draggable shapes with colors
		for (uint32_t uColorIdx = 0; uColorIdx < xParams.uNumColors; ++uColorIdx)
		{
			TilePuzzleColor eColor = static_cast<TilePuzzleColor>(uColorIdx);

			for (uint32_t i = 0; i < xParams.uNumShapesPerColor; ++i)
			{
				if (uPositionIndex >= axFloorPositions.size())
					return false;  // Not enough space

				auto [x, y] = axFloorPositions[uPositionIndex++];

				// Select shape type based on difficulty
				TilePuzzleShapeType eShapeType;
				if (xParams.uMaxShapeSize <= 1)
				{
					eShapeType = TILEPUZZLE_SHAPE_SINGLE;
				}
				else if (xParams.uMaxShapeSize <= 2)
				{
					std::uniform_int_distribution<int> xShapeDist(0, 1);
					eShapeType = static_cast<TilePuzzleShapeType>(xShapeDist(xRng));  // SINGLE or DOMINO
				}
				else
				{
					std::uniform_int_distribution<int> xShapeDist(0, static_cast<int>(TILEPUZZLE_SHAPE_O));
					eShapeType = static_cast<TilePuzzleShapeType>(xShapeDist(xRng));
				}

				// Create shape definition
				GetShapeDefinitions().push_back(TilePuzzleShapes::GetShape(eShapeType, true));  // Draggable
				TilePuzzleShapeDefinition& xShapeDef = GetShapeDefinitions().back();

				TilePuzzleShapeInstance xShape;
				xShape.pxDefinition = &xShapeDef;
				xShape.iOriginX = x;
				xShape.iOriginY = y;
				xShape.eColor = eColor;

				// Verify shape fits on grid
				bool bFits = true;
				for (const auto& xOffset : xShapeDef.axCells)
				{
					int32_t iCellX = x + xOffset.iX;
					int32_t iCellY = y + xOffset.iY;
					if (iCellX < 1 || iCellY < 1 ||
						static_cast<uint32_t>(iCellX) >= xLevelOut.uGridWidth - 1 ||
						static_cast<uint32_t>(iCellY) >= xLevelOut.uGridHeight - 1)
					{
						bFits = false;
						break;
					}
				}

				if (!bFits)
				{
					// Fall back to single cell if shape doesn't fit
					GetShapeDefinitions().back() = TilePuzzleShapes::GetSingleShape(true);
				}

				xLevelOut.axShapes.push_back(xShape);
			}
		}

		// Place cats with colors
		for (uint32_t uColorIdx = 0; uColorIdx < xParams.uNumColors; ++uColorIdx)
		{
			TilePuzzleColor eColor = static_cast<TilePuzzleColor>(uColorIdx);

			for (uint32_t i = 0; i < xParams.uNumCatsPerColor; ++i)
			{
				if (uPositionIndex >= axFloorPositions.size())
					return false;  // Not enough space

				auto [x, y] = axFloorPositions[uPositionIndex++];

				TilePuzzleCatData xCat;
				xCat.eColor = eColor;
				xCat.iGridX = x;
				xCat.iGridY = y;
				xCat.uEntityID = INVALID_ENTITY_ID;
				xCat.bEliminated = false;
				xCat.fEliminationProgress = 0.f;

				xLevelOut.axCats.push_back(xCat);
			}
		}

		return true;
	}

	/**
	 * GenerateFallbackLevel - Create a simple known-solvable level
	 */
	static void GenerateFallbackLevel(TilePuzzleLevelData& xLevelOut, uint32_t uLevelNumber)
	{
		// Clear shape definitions
		GetShapeDefinitions().clear();

		xLevelOut.uGridWidth = 5;
		xLevelOut.uGridHeight = 5;
		xLevelOut.aeCells.resize(25);

		// Fill grid: border empty, interior floor
		for (uint32_t y = 0; y < 5; ++y)
		{
			for (uint32_t x = 0; x < 5; ++x)
			{
				uint32_t uIdx = y * 5 + x;
				bool bBorder = (x == 0 || y == 0 || x == 4 || y == 4);
				xLevelOut.aeCells[uIdx] = bBorder ? TILEPUZZLE_CELL_EMPTY : TILEPUZZLE_CELL_FLOOR;
			}
		}

		// Add shape definitions
		GetShapeDefinitions().push_back(TilePuzzleShapes::GetSingleShape(true));  // Red shape
		GetShapeDefinitions().push_back(TilePuzzleShapes::GetSingleShape(true));  // Green shape

		// Red draggable shape at (1, 1)
		{
			TilePuzzleShapeInstance xShape;
			xShape.pxDefinition = &GetShapeDefinitions()[0];
			xShape.iOriginX = 1;
			xShape.iOriginY = 1;
			xShape.eColor = TILEPUZZLE_COLOR_RED;
			xLevelOut.axShapes.push_back(xShape);
		}

		// Green draggable shape at (3, 1)
		{
			TilePuzzleShapeInstance xShape;
			xShape.pxDefinition = &GetShapeDefinitions()[1];
			xShape.iOriginX = 3;
			xShape.iOriginY = 1;
			xShape.eColor = TILEPUZZLE_COLOR_GREEN;
			xLevelOut.axShapes.push_back(xShape);
		}

		// Red cat at (1, 3)
		{
			TilePuzzleCatData xCat;
			xCat.eColor = TILEPUZZLE_COLOR_RED;
			xCat.iGridX = 1;
			xCat.iGridY = 3;
			xCat.uEntityID = INVALID_ENTITY_ID;
			xCat.bEliminated = false;
			xCat.fEliminationProgress = 0.f;
			xLevelOut.axCats.push_back(xCat);
		}

		// Green cat at (3, 3)
		{
			TilePuzzleCatData xCat;
			xCat.eColor = TILEPUZZLE_COLOR_GREEN;
			xCat.iGridX = 3;
			xCat.iGridY = 3;
			xCat.uEntityID = INVALID_ENTITY_ID;
			xCat.bEliminated = false;
			xCat.fEliminationProgress = 0.f;
			xLevelOut.axCats.push_back(xCat);
		}

		xLevelOut.uMinimumMoves = 2;  // Known solution: 2 moves down
	}
};
