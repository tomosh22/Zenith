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

		if (uLevelNumber <= 5)
		{
			// Tutorial: tiny grid, 1 color, single cells, no blockers
			xParams.uMinGridWidth = 4;
			xParams.uMaxGridWidth = 5;
			xParams.uMinGridHeight = 4;
			xParams.uMaxGridHeight = 5;
			xParams.uNumColors = 1;
			xParams.uNumCatsPerColor = 1;
			xParams.uNumShapesPerColor = 1;
			xParams.uNumBlockers = 0;
			xParams.uMaxShapeSize = 1;
		}
		else if (uLevelNumber <= 15)
		{
			// Easy: small grid, 1-2 colors, single cells, few blockers
			uint32_t uProgress = uLevelNumber - 6; // 0-9
			xParams.uMinGridWidth = 5;
			xParams.uMaxGridWidth = 5;
			xParams.uMinGridHeight = 5;
			xParams.uMaxGridHeight = 5;
			xParams.uNumColors = 1 + uProgress / 5;
			xParams.uNumCatsPerColor = 1;
			xParams.uNumShapesPerColor = 1;
			xParams.uNumBlockers = uProgress / 5;
			xParams.uMaxShapeSize = 1;
		}
		else if (uLevelNumber <= 30)
		{
			// Medium: medium grid, 2 colors, dominos, some blockers
			uint32_t uProgress = uLevelNumber - 16; // 0-14
			xParams.uMinGridWidth = 5;
			xParams.uMaxGridWidth = 5 + uProgress / 8;
			xParams.uMinGridHeight = 5;
			xParams.uMaxGridHeight = 5 + uProgress / 8;
			xParams.uNumColors = 2;
			xParams.uNumCatsPerColor = 1;
			xParams.uNumShapesPerColor = 1;
			xParams.uNumBlockers = 1 + uProgress / 7;
			xParams.uMaxShapeSize = 2;
		}
		else if (uLevelNumber <= 50)
		{
			// Hard: larger grid, 2-3 colors, I/L shapes, more blockers
			uint32_t uProgress = uLevelNumber - 31; // 0-19
			xParams.uMinGridWidth = 6;
			xParams.uMaxGridWidth = 6 + uProgress / 10;
			xParams.uMinGridHeight = 6;
			xParams.uMaxGridHeight = 6 + uProgress / 10;
			xParams.uNumColors = 2 + uProgress / 10;
			xParams.uNumCatsPerColor = 1;
			xParams.uNumShapesPerColor = 1;
			xParams.uNumBlockers = 2 + uProgress / 5;
			xParams.uMaxShapeSize = 3;
		}
		else if (uLevelNumber <= 70)
		{
			// Expert: large grid, 3 colors, all shapes, many blockers
			uint32_t uProgress = uLevelNumber - 51; // 0-19
			xParams.uMinGridWidth = 7;
			xParams.uMaxGridWidth = 7;
			xParams.uMinGridHeight = 7;
			xParams.uMaxGridHeight = 7;
			xParams.uNumColors = 3;
			xParams.uNumCatsPerColor = 1;
			xParams.uNumShapesPerColor = 1;
			xParams.uNumBlockers = 3 + uProgress / 4;
			xParams.uMaxShapeSize = 4;
		}
		else if (uLevelNumber <= 85)
		{
			// Master: large grid, 3-4 colors, all shapes, blockers, 2 cats/color
			uint32_t uProgress = uLevelNumber - 71; // 0-14
			xParams.uMinGridWidth = 7;
			xParams.uMaxGridWidth = 7 + uProgress / 8;
			xParams.uMinGridHeight = 7;
			xParams.uMaxGridHeight = 7 + uProgress / 8;
			xParams.uNumColors = 3 + uProgress / 8;
			xParams.uNumCatsPerColor = 2;
			xParams.uNumShapesPerColor = 1;
			xParams.uNumBlockers = 4 + uProgress / 3;
			xParams.uMaxShapeSize = 4;
		}
		else
		{
			// Grandmaster: max grid, 4 colors, all shapes, many blockers, 2 cats/color
			uint32_t uProgress = uLevelNumber - 86; // 0-14
			xParams.uMinGridWidth = 8;
			xParams.uMaxGridWidth = 8;
			xParams.uMinGridHeight = 8;
			xParams.uMaxGridHeight = 8;
			xParams.uNumColors = 4;
			xParams.uNumCatsPerColor = 2;
			xParams.uNumShapesPerColor = 1;
			xParams.uNumBlockers = 5 + uProgress / 3;
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
		// Seed RNG deterministically so the same level number always produces the same layout
		xRng.seed(uLevelNumber * 7919u + 104729u);

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
		GenerateFallbackLevel(xLevelOut);
		return false;
	}

private:
	// Static shape definitions that persist during level lifetime
	static Zenith_Vector<TilePuzzleShapeDefinition>& GetShapeDefinitions()
	{
		static Zenith_Vector<TilePuzzleShapeDefinition> s_axShapeDefinitions;
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
		// Clear any previous shape definitions and reserve capacity upfront.
		// pxDefinition pointers into this vector become dangling if it reallocates.
		GetShapeDefinitions().Clear();
		GetShapeDefinitions().Reserve(xParams.uNumBlockers + xParams.uNumColors * xParams.uNumShapesPerColor);

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

		// Occupancy grid - tracks which cells are already taken by placed objects
		static constexpr uint32_t uMAX_OCCUPANCY = s_uTilePuzzleMaxGridSize * s_uTilePuzzleMaxGridSize;
		bool abOccupied[uMAX_OCCUPANCY] = {};

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

		// Place static blockers
		for (uint32_t i = 0; i < xParams.uNumBlockers; ++i)
		{
			// Find first unoccupied position in shuffled list
			bool bPlaced = false;
			for (size_t p = 0; p < axFloorPositions.size(); ++p)
			{
				auto [x, y] = axFloorPositions[p];
				uint32_t uIdx = y * xLevelOut.uGridWidth + x;
				if (abOccupied[uIdx])
					continue;

				// Place blocker
				GetShapeDefinitions().PushBack(TilePuzzleShapes::GetSingleShape(false));
				TilePuzzleShapeDefinition& xBlockerDef = GetShapeDefinitions().Get(GetShapeDefinitions().GetSize() - 1);

				TilePuzzleShapeInstance xBlocker;
				xBlocker.pxDefinition = &xBlockerDef;
				xBlocker.iOriginX = x;
				xBlocker.iOriginY = y;
				xBlocker.eColor = TILEPUZZLE_COLOR_NONE;

				xLevelOut.axShapes.push_back(xBlocker);

				// Mark occupied
				abOccupied[uIdx] = true;
				bPlaced = true;
				break;
			}
			if (!bPlaced)
				return false;
		}

		// Place draggable shapes with colors
		for (uint32_t uColorIdx = 0; uColorIdx < xParams.uNumColors; ++uColorIdx)
		{
			TilePuzzleColor eColor = static_cast<TilePuzzleColor>(uColorIdx);

			for (uint32_t i = 0; i < xParams.uNumShapesPerColor; ++i)
			{
				// Select shape type based on difficulty
				TilePuzzleShapeType eShapeType;
				if (xParams.uMaxShapeSize <= 1)
				{
					eShapeType = TILEPUZZLE_SHAPE_SINGLE;
				}
				else if (xParams.uMaxShapeSize <= 2)
				{
					std::uniform_int_distribution<int> xShapeDist(0, 1);
					eShapeType = static_cast<TilePuzzleShapeType>(xShapeDist(xRng));
				}
				else
				{
					std::uniform_int_distribution<int> xShapeDist(0, static_cast<int>(TILEPUZZLE_SHAPE_O));
					eShapeType = static_cast<TilePuzzleShapeType>(xShapeDist(xRng));
				}

				// Create shape definition
				GetShapeDefinitions().PushBack(TilePuzzleShapes::GetShape(eShapeType, true));
				TilePuzzleShapeDefinition& xShapeDef = GetShapeDefinitions().Get(GetShapeDefinitions().GetSize() - 1);

				// Find a position where ALL cells of the shape fit and are unoccupied
				bool bPlaced = false;
				for (size_t p = 0; p < axFloorPositions.size(); ++p)
				{
					auto [x, y] = axFloorPositions[p];

					bool bFits = true;
					for (size_t c = 0; c < xShapeDef.axCells.size(); ++c)
					{
						int32_t iCellX = x + xShapeDef.axCells[c].iX;
						int32_t iCellY = y + xShapeDef.axCells[c].iY;

						// Check bounds (must be within inner area)
						if (iCellX < 1 || iCellY < 1 ||
							static_cast<uint32_t>(iCellX) >= xLevelOut.uGridWidth - 1 ||
							static_cast<uint32_t>(iCellY) >= xLevelOut.uGridHeight - 1)
						{
							bFits = false;
							break;
						}

						// Check occupancy
						uint32_t uCellIdx = iCellY * xLevelOut.uGridWidth + iCellX;
						if (abOccupied[uCellIdx])
						{
							bFits = false;
							break;
						}
					}

					if (bFits)
					{
						TilePuzzleShapeInstance xShape;
						xShape.pxDefinition = &xShapeDef;
						xShape.iOriginX = x;
						xShape.iOriginY = y;
						xShape.eColor = eColor;
						xLevelOut.axShapes.push_back(xShape);

						// Mark ALL cells of the shape as occupied
						for (size_t c = 0; c < xShapeDef.axCells.size(); ++c)
						{
							int32_t iCellX = x + xShapeDef.axCells[c].iX;
							int32_t iCellY = y + xShapeDef.axCells[c].iY;
							abOccupied[iCellY * xLevelOut.uGridWidth + iCellX] = true;
						}

						bPlaced = true;
						break;
					}
				}

				if (!bPlaced)
				{
					// Fall back to single cell shape
					GetShapeDefinitions().Get(GetShapeDefinitions().GetSize() - 1) = TilePuzzleShapes::GetSingleShape(true);
					TilePuzzleShapeDefinition& xSingleDef = GetShapeDefinitions().Get(GetShapeDefinitions().GetSize() - 1);

					for (size_t p = 0; p < axFloorPositions.size(); ++p)
					{
						auto [x, y] = axFloorPositions[p];
						uint32_t uIdx = y * xLevelOut.uGridWidth + x;
						if (abOccupied[uIdx])
							continue;

						TilePuzzleShapeInstance xShape;
						xShape.pxDefinition = &xSingleDef;
						xShape.iOriginX = x;
						xShape.iOriginY = y;
						xShape.eColor = eColor;
						xLevelOut.axShapes.push_back(xShape);

						abOccupied[uIdx] = true;
						bPlaced = true;
						break;
					}

					if (!bPlaced)
						return false;
				}
			}
		}

		// Place cats with colors
		for (uint32_t uColorIdx = 0; uColorIdx < xParams.uNumColors; ++uColorIdx)
		{
			TilePuzzleColor eColor = static_cast<TilePuzzleColor>(uColorIdx);

			for (uint32_t i = 0; i < xParams.uNumCatsPerColor; ++i)
			{
				bool bPlaced = false;
				for (size_t p = 0; p < axFloorPositions.size(); ++p)
				{
					auto [x, y] = axFloorPositions[p];
					uint32_t uIdx = y * xLevelOut.uGridWidth + x;
					if (abOccupied[uIdx])
						continue;

					TilePuzzleCatData xCat;
					xCat.eColor = eColor;
					xCat.iGridX = x;
					xCat.iGridY = y;
					xCat.uEntityID = INVALID_ENTITY_ID;
					xCat.bEliminated = false;
					xCat.fEliminationProgress = 0.f;
					xLevelOut.axCats.push_back(xCat);

					abOccupied[uIdx] = true;
					bPlaced = true;
					break;
				}
				if (!bPlaced)
					return false;
			}
		}

		return true;
	}

	/**
	 * GenerateFallbackLevel - Create a simple known-solvable level
	 */
	static void GenerateFallbackLevel(TilePuzzleLevelData& xLevelOut)
	{
		// Clear shape definitions
		GetShapeDefinitions().Clear();
		GetShapeDefinitions().Reserve(2);

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
		GetShapeDefinitions().PushBack(TilePuzzleShapes::GetSingleShape(true));  // Red shape
		GetShapeDefinitions().PushBack(TilePuzzleShapes::GetSingleShape(true));  // Green shape

		// Red draggable shape at (1, 1)
		{
			TilePuzzleShapeInstance xShape;
			xShape.pxDefinition = &GetShapeDefinitions().Get(0);
			xShape.iOriginX = 1;
			xShape.iOriginY = 1;
			xShape.eColor = TILEPUZZLE_COLOR_RED;
			xLevelOut.axShapes.push_back(xShape);
		}

		// Green draggable shape at (3, 1)
		{
			TilePuzzleShapeInstance xShape;
			xShape.pxDefinition = &GetShapeDefinitions().Get(1);
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
