#pragma once
/**
 * TilePuzzle_LevelGenerator.h - Procedural level generation via reverse scramble
 *
 * Generation algorithm (reverse scramble):
 * 1. Create grid of floor cells with border
 * 2. Place static blockers randomly
 * 3. Place cats on unoccupied floor cells
 * 4. Place shapes ON their matching cats (solved configuration)
 * 5. Scramble by making random valid moves using shared rules
 * 6. The reverse of the scramble is a valid solution (solvable by construction)
 *
 * Difficulty cycles Normal -> Hard -> Very Hard via (levelNumber - 1) % 3.
 * All difficulty parameters are configurable static constexpr constants.
 */

#include <random>
#include <vector>
#include <algorithm>
#include <cstdint>

#include "TilePuzzle_Types.h"
#include "TilePuzzle_Rules.h"
#include "Collections/Zenith_Vector.h"

// Generation constants
static constexpr uint32_t s_uTilePuzzleMinGridSize = 5;
static constexpr uint32_t s_uTilePuzzleMaxGridSize = 10;
static constexpr int32_t s_iTilePuzzleMaxGenerationAttempts = 1000;

// Normal difficulty
static constexpr uint32_t s_uNormalGridWidth = 6;
static constexpr uint32_t s_uNormalGridHeight = 8;
static constexpr uint32_t s_uNormalNumColors = 2;
static constexpr uint32_t s_uNormalNumCatsPerColor = 2;
static constexpr uint32_t s_uNormalNumShapesPerColor = 2;
static constexpr uint32_t s_uNormalNumBlockers = 4;
static constexpr uint32_t s_uNormalMaxShapeSize = 2;
static constexpr uint32_t s_uNormalScrambleMoves = 15;
static constexpr uint32_t s_uNormalNumBlockerCats = 0;
static constexpr uint32_t s_uNormalNumConditionalShapes = 0;
static constexpr uint32_t s_uNormalConditionalThreshold = 0;

// Hard difficulty
static constexpr uint32_t s_uHardGridWidth = 7;
static constexpr uint32_t s_uHardGridHeight = 9;
static constexpr uint32_t s_uHardNumColors = 3;
static constexpr uint32_t s_uHardNumCatsPerColor = 2;
static constexpr uint32_t s_uHardNumShapesPerColor = 2;
static constexpr uint32_t s_uHardNumBlockers = 8;
static constexpr uint32_t s_uHardMaxShapeSize = 3;
static constexpr uint32_t s_uHardScrambleMoves = 30;
static constexpr uint32_t s_uHardNumBlockerCats = 2;
static constexpr uint32_t s_uHardNumConditionalShapes = 0;
static constexpr uint32_t s_uHardConditionalThreshold = 0;

// Very Hard difficulty
static constexpr uint32_t s_uVeryHardGridWidth = 10;
static constexpr uint32_t s_uVeryHardGridHeight = 12;
static constexpr uint32_t s_uVeryHardNumColors = 3;
static constexpr uint32_t s_uVeryHardNumCatsPerColor = 4;
static constexpr uint32_t s_uVeryHardNumShapesPerColor = 2;
static constexpr uint32_t s_uVeryHardNumBlockers = 12;
static constexpr uint32_t s_uVeryHardMaxShapeSize = 4;
static constexpr uint32_t s_uVeryHardScrambleMoves = 60;
static constexpr uint32_t s_uVeryHardNumBlockerCats = 2;
static constexpr uint32_t s_uVeryHardNumConditionalShapes = 1;
static constexpr uint32_t s_uVeryHardConditionalThreshold = 2;

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
		uint32_t uScrambleMoves = 15;      // Number of scramble moves for reverse generation
		uint32_t uNumBlockerCats = 0;      // Cats on blockers (eliminated by adjacency)
		uint32_t uNumConditionalShapes = 0;// Shapes that require N eliminations to unlock
		uint32_t uConditionalThreshold = 0;// Eliminations required to unlock conditional shapes
	};

	/**
	 * GetDifficultyForLevel - Get difficulty parameters based on level number
	 */
	static DifficultyParams GetDifficultyForLevel(uint32_t uLevelNumber)
	{
		DifficultyParams xParams;
		uint32_t uTier = (uLevelNumber - 1) % 3; // 0=Normal, 1=Hard, 2=VeryHard

		switch (uTier)
		{
		case 0: // Normal
			xParams.uMinGridWidth = s_uNormalGridWidth;
			xParams.uMaxGridWidth = s_uNormalGridWidth;
			xParams.uMinGridHeight = s_uNormalGridHeight;
			xParams.uMaxGridHeight = s_uNormalGridHeight;
			xParams.uNumColors = s_uNormalNumColors;
			xParams.uNumCatsPerColor = s_uNormalNumCatsPerColor;
			xParams.uNumShapesPerColor = s_uNormalNumShapesPerColor;
			xParams.uNumBlockers = s_uNormalNumBlockers;
			xParams.uMaxShapeSize = s_uNormalMaxShapeSize;
			xParams.uScrambleMoves = s_uNormalScrambleMoves;
			xParams.uNumBlockerCats = s_uNormalNumBlockerCats;
			xParams.uNumConditionalShapes = s_uNormalNumConditionalShapes;
			xParams.uConditionalThreshold = s_uNormalConditionalThreshold;
			break;
		case 1: // Hard
			xParams.uMinGridWidth = s_uHardGridWidth;
			xParams.uMaxGridWidth = s_uHardGridWidth;
			xParams.uMinGridHeight = s_uHardGridHeight;
			xParams.uMaxGridHeight = s_uHardGridHeight;
			xParams.uNumColors = s_uHardNumColors;
			xParams.uNumCatsPerColor = s_uHardNumCatsPerColor;
			xParams.uNumShapesPerColor = s_uHardNumShapesPerColor;
			xParams.uNumBlockers = s_uHardNumBlockers;
			xParams.uMaxShapeSize = s_uHardMaxShapeSize;
			xParams.uScrambleMoves = s_uHardScrambleMoves;
			xParams.uNumBlockerCats = s_uHardNumBlockerCats;
			xParams.uNumConditionalShapes = s_uHardNumConditionalShapes;
			xParams.uConditionalThreshold = s_uHardConditionalThreshold;
			break;
		case 2: // Very Hard
			xParams.uMinGridWidth = s_uVeryHardGridWidth;
			xParams.uMaxGridWidth = s_uVeryHardGridWidth;
			xParams.uMinGridHeight = s_uVeryHardGridHeight;
			xParams.uMaxGridHeight = s_uVeryHardGridHeight;
			xParams.uNumColors = s_uVeryHardNumColors;
			xParams.uNumCatsPerColor = s_uVeryHardNumCatsPerColor;
			xParams.uNumShapesPerColor = s_uVeryHardNumShapesPerColor;
			xParams.uNumBlockers = s_uVeryHardNumBlockers;
			xParams.uMaxShapeSize = s_uVeryHardMaxShapeSize;
			xParams.uScrambleMoves = s_uVeryHardScrambleMoves;
			xParams.uNumBlockerCats = s_uVeryHardNumBlockerCats;
			xParams.uNumConditionalShapes = s_uVeryHardNumConditionalShapes;
			xParams.uConditionalThreshold = s_uVeryHardConditionalThreshold;
			break;
		}

		// Clamp values
		xParams.uNumColors = std::min(xParams.uNumColors, static_cast<uint32_t>(TILEPUZZLE_COLOR_COUNT));
		xParams.uMaxShapeSize = std::min(xParams.uMaxShapeSize, 4u);

		return xParams;
	}

	/**
	 * GenerateLevel - Generate a solvable level using reverse scramble
	 *
	 * Algorithm: Start from a solved configuration (shapes on cats),
	 * scramble by making random valid moves. The reverse of the scramble
	 * is a valid solution, guaranteeing solvability by construction.
	 *
	 * @param xLevelOut   Output level data
	 * @param xRng        Random number generator
	 * @param uLevelNumber Level number for difficulty scaling
	 * @return true if generated level, false if used fallback
	 */
	static bool GenerateLevel(TilePuzzleLevelData& xLevelOut, std::mt19937& xRng, uint32_t uLevelNumber)
	{
		xRng.seed(uLevelNumber * 7919u + 104729u);

		DifficultyParams xParams = GetDifficultyForLevel(uLevelNumber);

		for (int32_t iAttempt = 0; iAttempt < s_iTilePuzzleMaxGenerationAttempts; ++iAttempt)
		{
			xLevelOut = TilePuzzleLevelData();

			uint32_t uSuccessfulMoves = 0;
			if (GenerateLevelAttempt(xLevelOut, xRng, xParams, uSuccessfulMoves))
			{
				xLevelOut.uMinimumMoves = uSuccessfulMoves;
				return true;
			}
		}

		GenerateFallbackLevel(xLevelOut);
		return false;
	}

private:
	/**
	 * IsSameColorCatAdjacent - Check if placing a cat at (iX, iY) with the given color
	 * would be adjacent (including diagonals) to any existing cat of the same color.
	 */
	static bool IsSameColorCatAdjacent(
		const std::vector<TilePuzzleCatData>& axCats,
		TilePuzzleColor eColor,
		int32_t iX, int32_t iY)
	{
		for (size_t i = 0; i < axCats.size(); ++i)
		{
			if (axCats[i].eColor != eColor)
				continue;
			int32_t iDX = axCats[i].iGridX - iX;
			int32_t iDY = axCats[i].iGridY - iY;
			if (iDX >= -1 && iDX <= 1 && iDY >= -1 && iDY <= 1)
				return true;
		}
		return false;
	}

	// Static shape definitions that persist during level lifetime
	static Zenith_Vector<TilePuzzleShapeDefinition>& GetShapeDefinitions()
	{
		static Zenith_Vector<TilePuzzleShapeDefinition> s_axShapeDefinitions;
		return s_axShapeDefinitions;
	}

	/**
	 * ComputeCoveredMask - Find which cats are currently overlapped by a same-color shape
	 *
	 * Unlike the game's eliminated mask (permanent), the covered mask is recomputed
	 * from current positions each time. Used during scramble to track which cats
	 * are currently "hidden" under shapes.
	 */
	static uint32_t ComputeCoveredMask(
		const TilePuzzle_Rules::ShapeState* axShapes, size_t uNumShapes,
		const TilePuzzle_Rules::CatState* axCats, size_t uNumCats)
	{
		return TilePuzzle_Rules::ComputeNewlyEliminatedCats(
			axShapes, uNumShapes,
			axCats, uNumCats,
			0);
	}

	/**
	 * TryScrambleMove - Attempt to move a shape during scramble
	 *
	 * Validates the move via shared rules, passing the covered mask as the
	 * eliminated mask. If valid, updates both the ShapeState array and the
	 * level data's shape positions.
	 *
	 * Extensibility point: future shape types (tandem, conditional) extend this method.
	 */
	static bool TryScrambleMove(
		const TilePuzzleLevelData& xLevel,
		TilePuzzle_Rules::ShapeState* axDraggableShapes, size_t uNumDraggableShapes,
		const std::vector<size_t>& axDraggableIndices,
		const TilePuzzle_Rules::CatState* axCats, size_t uNumCats,
		size_t uShapeIdx,
		int32_t iDeltaX, int32_t iDeltaY,
		uint32_t uCoveredMask,
		TilePuzzleLevelData& xLevelOut)
	{
		int32_t iNewOriginX = axDraggableShapes[uShapeIdx].iOriginX + iDeltaX;
		int32_t iNewOriginY = axDraggableShapes[uShapeIdx].iOriginY + iDeltaY;

		if (!TilePuzzle_Rules::CanMoveShape(
			xLevel,
			axDraggableShapes, uNumDraggableShapes,
			uShapeIdx,
			iNewOriginX, iNewOriginY,
			axCats, uNumCats,
			uCoveredMask))
		{
			return false;
		}

		axDraggableShapes[uShapeIdx].iOriginX = iNewOriginX;
		axDraggableShapes[uShapeIdx].iOriginY = iNewOriginY;
		xLevelOut.axShapes[axDraggableIndices[uShapeIdx]].iOriginX = iNewOriginX;
		xLevelOut.axShapes[axDraggableIndices[uShapeIdx]].iOriginY = iNewOriginY;

		return true;
	}

	/**
	 * GenerateLevelAttempt - Single attempt using reverse scramble
	 *
	 * Phase 1: Create grid with borders, place static blockers
	 * Phase 2: Place normal cats + blocker-cats
	 * Phase 3: Place shapes on/adjacent to cats (solved configuration), mark conditionals
	 * Phase 4: Pre-scramble conditional shapes, then main scramble
	 */
	static bool GenerateLevelAttempt(
		TilePuzzleLevelData& xLevelOut,
		std::mt19937& xRng,
		const DifficultyParams& xParams,
		uint32_t& uSuccessfulMovesOut)
	{
		GetShapeDefinitions().Clear();
		GetShapeDefinitions().Reserve(
			xParams.uNumBlockers +
			xParams.uNumColors * xParams.uNumShapesPerColor +
			xParams.uNumBlockerCats);

		// ---- Phase 1: Grid + static blockers ----

		std::uniform_int_distribution<uint32_t> xWidthDist(xParams.uMinGridWidth, xParams.uMaxGridWidth);
		std::uniform_int_distribution<uint32_t> xHeightDist(xParams.uMinGridHeight, xParams.uMaxGridHeight);

		xLevelOut.uGridWidth = xWidthDist(xRng);
		xLevelOut.uGridHeight = xHeightDist(xRng);
		uint32_t uGridSize = xLevelOut.uGridWidth * xLevelOut.uGridHeight;

		xLevelOut.aeCells.resize(uGridSize, TILEPUZZLE_CELL_FLOOR);

		for (uint32_t x = 0; x < xLevelOut.uGridWidth; ++x)
		{
			xLevelOut.aeCells[x] = TILEPUZZLE_CELL_EMPTY;
			xLevelOut.aeCells[(xLevelOut.uGridHeight - 1) * xLevelOut.uGridWidth + x] = TILEPUZZLE_CELL_EMPTY;
		}
		for (uint32_t y = 0; y < xLevelOut.uGridHeight; ++y)
		{
			xLevelOut.aeCells[y * xLevelOut.uGridWidth] = TILEPUZZLE_CELL_EMPTY;
			xLevelOut.aeCells[y * xLevelOut.uGridWidth + xLevelOut.uGridWidth - 1] = TILEPUZZLE_CELL_EMPTY;
		}

		static constexpr uint32_t uMAX_OCCUPANCY = s_uTilePuzzleMaxGridSize * s_uTilePuzzleMaxGridSize;
		bool abOccupied[uMAX_OCCUPANCY] = {};

		std::vector<std::pair<int32_t, int32_t>> axFloorPositions;
		for (uint32_t y = 1; y < xLevelOut.uGridHeight - 1; ++y)
		{
			for (uint32_t x = 1; x < xLevelOut.uGridWidth - 1; ++x)
			{
				axFloorPositions.push_back({static_cast<int32_t>(x), static_cast<int32_t>(y)});
			}
		}

		if (axFloorPositions.size() < 3)
			return false;

		std::shuffle(axFloorPositions.begin(), axFloorPositions.end(), xRng);

		// Track blocker positions for blocker-cat placement
		std::vector<size_t> axBlockerShapeIndices;
		for (uint32_t i = 0; i < xParams.uNumBlockers; ++i)
		{
			bool bPlaced = false;
			for (size_t p = 0; p < axFloorPositions.size(); ++p)
			{
				auto [x, y] = axFloorPositions[p];
				uint32_t uIdx = y * xLevelOut.uGridWidth + x;
				if (abOccupied[uIdx])
					continue;

				GetShapeDefinitions().PushBack(TilePuzzleShapes::GetSingleShape(false));
				TilePuzzleShapeDefinition& xBlockerDef = GetShapeDefinitions().Get(GetShapeDefinitions().GetSize() - 1);

				TilePuzzleShapeInstance xBlocker;
				xBlocker.pxDefinition = &xBlockerDef;
				xBlocker.iOriginX = x;
				xBlocker.iOriginY = y;
				xBlocker.eColor = TILEPUZZLE_COLOR_NONE;
				xLevelOut.axShapes.push_back(xBlocker);

				axBlockerShapeIndices.push_back(xLevelOut.axShapes.size() - 1);
				abOccupied[uIdx] = true;
				bPlaced = true;
				break;
			}
			if (!bPlaced)
				return false;
		}

		// ---- Phase 2: Place cats ----

		// Normal cats on unoccupied floor cells
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
					if (IsSameColorCatAdjacent(xLevelOut.axCats, eColor, x, y))
						continue;

					TilePuzzleCatData xCat;
					xCat.eColor = eColor;
					xCat.iGridX = x;
					xCat.iGridY = y;
					xCat.uEntityID = INVALID_ENTITY_ID;
					xCat.bEliminated = false;
					xCat.bOnBlocker = false;
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

		// Blocker-cats: place on existing blocker positions
		uint32_t uBlockerCatsPlaced = 0;
		uint32_t uBlockerCatsToPlace = std::min(xParams.uNumBlockerCats, static_cast<uint32_t>(axBlockerShapeIndices.size()));
		for (uint32_t i = 0; i < uBlockerCatsToPlace; ++i)
		{
			const TilePuzzleShapeInstance& xBlocker = xLevelOut.axShapes[axBlockerShapeIndices[i]];

			// Find a color that doesn't violate same-color adjacency
			TilePuzzleColor eColor = TILEPUZZLE_COLOR_NONE;
			for (uint32_t uC = 0; uC < xParams.uNumColors; ++uC)
			{
				TilePuzzleColor eCandidate = static_cast<TilePuzzleColor>((i + uC) % xParams.uNumColors);
				if (!IsSameColorCatAdjacent(xLevelOut.axCats, eCandidate, xBlocker.iOriginX, xBlocker.iOriginY))
				{
					eColor = eCandidate;
					break;
				}
			}
			if (eColor == TILEPUZZLE_COLOR_NONE)
				return false;

			TilePuzzleCatData xCat;
			xCat.eColor = eColor;
			xCat.iGridX = xBlocker.iOriginX;
			xCat.iGridY = xBlocker.iOriginY;
			xCat.uEntityID = INVALID_ENTITY_ID;
			xCat.bEliminated = false;
			xCat.bOnBlocker = true;
			xCat.fEliminationProgress = 0.f;
			xLevelOut.axCats.push_back(xCat);
			uBlockerCatsPlaced++;
		}

		// ---- Phase 3: Place shapes (solved configuration) ----

		// Normal shapes: overlap their matching cats
		size_t uCatIdx = 0;
		size_t uFirstNormalDraggableShape = xLevelOut.axShapes.size();
		for (uint32_t uColorIdx = 0; uColorIdx < xParams.uNumColors; ++uColorIdx)
		{
			TilePuzzleColor eColor = static_cast<TilePuzzleColor>(uColorIdx);

			for (uint32_t i = 0; i < xParams.uNumShapesPerColor; ++i)
			{
				// Find a normal (non-blocker) cat of this color
				int32_t iTargetCatIdx = -1;
				for (size_t c = uCatIdx; c < xLevelOut.axCats.size(); ++c)
				{
					if (xLevelOut.axCats[c].eColor == eColor && !xLevelOut.axCats[c].bOnBlocker)
					{
						iTargetCatIdx = static_cast<int32_t>(c);
						uCatIdx = c + 1;
						break;
					}
				}
				if (iTargetCatIdx < 0)
					return false;

				const TilePuzzleCatData& xTargetCat = xLevelOut.axCats[iTargetCatIdx];

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

				GetShapeDefinitions().PushBack(TilePuzzleShapes::GetShape(eShapeType, true));
				TilePuzzleShapeDefinition& xShapeDef = GetShapeDefinitions().Get(GetShapeDefinitions().GetSize() - 1);

				// Try to place shape so one of its cells overlaps the target cat
				bool bPlaced = false;
				for (size_t c = 0; c < xShapeDef.axCells.size(); ++c)
				{
					int32_t iOriginX = xTargetCat.iGridX - xShapeDef.axCells[c].iX;
					int32_t iOriginY = xTargetCat.iGridY - xShapeDef.axCells[c].iY;

					bool bFits = true;
					for (size_t k = 0; k < xShapeDef.axCells.size(); ++k)
					{
						int32_t iCellX = iOriginX + xShapeDef.axCells[k].iX;
						int32_t iCellY = iOriginY + xShapeDef.axCells[k].iY;

						if (iCellX < 1 || iCellY < 1 ||
							static_cast<uint32_t>(iCellX) >= xLevelOut.uGridWidth - 1 ||
							static_cast<uint32_t>(iCellY) >= xLevelOut.uGridHeight - 1)
						{
							bFits = false;
							break;
						}

						uint32_t uCellIdx = iCellY * xLevelOut.uGridWidth + iCellX;
						if (abOccupied[uCellIdx])
						{
							bool bIsCatCell = false;
							for (size_t catCheck = 0; catCheck < xLevelOut.axCats.size(); ++catCheck)
							{
								if (xLevelOut.axCats[catCheck].iGridX == iCellX &&
									xLevelOut.axCats[catCheck].iGridY == iCellY &&
									xLevelOut.axCats[catCheck].eColor == eColor &&
									!xLevelOut.axCats[catCheck].bOnBlocker)
								{
									bIsCatCell = true;
									break;
								}
							}
							if (!bIsCatCell)
							{
								bFits = false;
								break;
							}
						}
					}

					if (bFits)
					{
						TilePuzzleShapeInstance xShape;
						xShape.pxDefinition = &xShapeDef;
						xShape.iOriginX = iOriginX;
						xShape.iOriginY = iOriginY;
						xShape.eColor = eColor;
						xLevelOut.axShapes.push_back(xShape);

						for (size_t k = 0; k < xShapeDef.axCells.size(); ++k)
						{
							int32_t iCellX = iOriginX + xShapeDef.axCells[k].iX;
							int32_t iCellY = iOriginY + xShapeDef.axCells[k].iY;
							abOccupied[iCellY * xLevelOut.uGridWidth + iCellX] = true;
						}

						bPlaced = true;
						break;
					}
				}

				if (!bPlaced)
				{
					GetShapeDefinitions().Get(GetShapeDefinitions().GetSize() - 1) = TilePuzzleShapes::GetSingleShape(true);
					TilePuzzleShapeDefinition& xSingleDef = GetShapeDefinitions().Get(GetShapeDefinitions().GetSize() - 1);

					TilePuzzleShapeInstance xShape;
					xShape.pxDefinition = &xSingleDef;
					xShape.iOriginX = xTargetCat.iGridX;
					xShape.iOriginY = xTargetCat.iGridY;
					xShape.eColor = eColor;
					xLevelOut.axShapes.push_back(xShape);

					abOccupied[xTargetCat.iGridY * xLevelOut.uGridWidth + xTargetCat.iGridX] = true;
				}
			}
		}

		// Blocker-cat shapes: single-cell placed adjacent to blocker-cat
		static const int32_t aiAdjacentDX[] = {0, 0, -1, 1};
		static const int32_t aiAdjacentDY[] = {-1, 1, 0, 0};

		for (uint32_t i = 0; i < uBlockerCatsPlaced; ++i)
		{
			size_t uBlockerCatIndex = xLevelOut.axCats.size() - uBlockerCatsPlaced + i;
			const TilePuzzleCatData& xBlockerCat = xLevelOut.axCats[uBlockerCatIndex];

			int32_t aiOrder[] = {0, 1, 2, 3};
			std::shuffle(std::begin(aiOrder), std::end(aiOrder), xRng);

			bool bPlaced = false;
			for (int32_t j = 0; j < 4; ++j)
			{
				int32_t iAdj = aiOrder[j];
				int32_t iX = xBlockerCat.iGridX + aiAdjacentDX[iAdj];
				int32_t iY = xBlockerCat.iGridY + aiAdjacentDY[iAdj];

				if (iX < 1 || iY < 1 ||
					static_cast<uint32_t>(iX) >= xLevelOut.uGridWidth - 1 ||
					static_cast<uint32_t>(iY) >= xLevelOut.uGridHeight - 1)
					continue;

				uint32_t uIdx = iY * xLevelOut.uGridWidth + iX;
				if (abOccupied[uIdx] || xLevelOut.aeCells[uIdx] != TILEPUZZLE_CELL_FLOOR)
					continue;

				GetShapeDefinitions().PushBack(TilePuzzleShapes::GetSingleShape(true));
				TilePuzzleShapeDefinition& xShapeDef = GetShapeDefinitions().Get(GetShapeDefinitions().GetSize() - 1);

				TilePuzzleShapeInstance xShape;
				xShape.pxDefinition = &xShapeDef;
				xShape.iOriginX = iX;
				xShape.iOriginY = iY;
				xShape.eColor = xBlockerCat.eColor;
				xLevelOut.axShapes.push_back(xShape);

				abOccupied[uIdx] = true;
				bPlaced = true;
				break;
			}
			if (!bPlaced)
				return false;
		}

		// Mark conditional shapes (only normal shapes, not blocker-cat shapes)
		if (xParams.uNumConditionalShapes > 0 && xParams.uConditionalThreshold > 0)
		{
			uint32_t uConditionalCount = 0;
			for (size_t i = uFirstNormalDraggableShape;
				i < xLevelOut.axShapes.size() && uConditionalCount < xParams.uNumConditionalShapes;
				++i)
			{
				if (xLevelOut.axShapes[i].pxDefinition && xLevelOut.axShapes[i].pxDefinition->bDraggable)
				{
					xLevelOut.axShapes[i].uUnlockThreshold = xParams.uConditionalThreshold;
					uConditionalCount++;
				}
			}
		}

		// ---- Phase 4: Scramble ----

		// Build draggable shape state arrays
		std::vector<size_t> axDraggableIndices;
		Zenith_Vector<TilePuzzle_Rules::ShapeState> axShapeStates;
		for (size_t i = 0; i < xLevelOut.axShapes.size(); ++i)
		{
			if (xLevelOut.axShapes[i].pxDefinition && xLevelOut.axShapes[i].pxDefinition->bDraggable)
			{
				axDraggableIndices.push_back(i);
				TilePuzzle_Rules::ShapeState xState;
				xState.pxDefinition = xLevelOut.axShapes[i].pxDefinition;
				xState.iOriginX = xLevelOut.axShapes[i].iOriginX;
				xState.iOriginY = xLevelOut.axShapes[i].iOriginY;
				xState.eColor = xLevelOut.axShapes[i].eColor;
				xState.uUnlockThreshold = xLevelOut.axShapes[i].uUnlockThreshold;
				axShapeStates.PushBack(xState);
			}
		}

		if (axDraggableIndices.empty())
			return false;

		// Build cat state array
		Zenith_Vector<TilePuzzle_Rules::CatState> axCatStates;
		for (size_t i = 0; i < xLevelOut.axCats.size(); ++i)
		{
			TilePuzzle_Rules::CatState xCatState;
			xCatState.iGridX = xLevelOut.axCats[i].iGridX;
			xCatState.iGridY = xLevelOut.axCats[i].iGridY;
			xCatState.eColor = xLevelOut.axCats[i].eColor;
			xCatState.bOnBlocker = xLevelOut.axCats[i].bOnBlocker;
			axCatStates.PushBack(xCatState);
		}

		uint32_t uNumCats = static_cast<uint32_t>(xLevelOut.axCats.size());
		uint32_t uAllCatsBits = (1u << uNumCats) - 1u;

		uint32_t uCoveredMask = ComputeCoveredMask(
			axShapeStates.GetDataPointer(), axShapeStates.GetSize(),
			axCatStates.GetDataPointer(), axCatStates.GetSize());
		uint32_t uEverCoveredMask = uCoveredMask;

		int32_t aiScrambleDeltaX[] = {0, 0, -1, 1};
		int32_t aiScrambleDeltaY[] = {-1, 1, 0, 0};
		std::uniform_int_distribution<int32_t> xDirDist(0, 3);

		// Pre-scramble: move conditional shapes off their cats while coveredMask is high
		for (uint32_t i = 0; i < axShapeStates.GetSize(); ++i)
		{
			if (axShapeStates.Get(i).uUnlockThreshold == 0)
				continue;

			for (uint32_t uAttempt = 0; uAttempt < 20; ++uAttempt)
			{
				int32_t iDir = xDirDist(xRng);
				if (TryScrambleMove(
					xLevelOut,
					axShapeStates.GetDataPointer(), axShapeStates.GetSize(),
					axDraggableIndices,
					axCatStates.GetDataPointer(), axCatStates.GetSize(),
					i,
					aiScrambleDeltaX[iDir], aiScrambleDeltaY[iDir],
					uCoveredMask,
					xLevelOut))
				{
					uCoveredMask = ComputeCoveredMask(
						axShapeStates.GetDataPointer(), axShapeStates.GetSize(),
						axCatStates.GetDataPointer(), axCatStates.GetSize());
					uEverCoveredMask |= uCoveredMask;
					break;
				}
			}
		}

		// Main scramble
		uint32_t uSuccessfulMoves = 0;
		uint32_t uMaxIterations = xParams.uScrambleMoves * 10;
		std::uniform_int_distribution<size_t> xShapeDist(0, axDraggableIndices.size() - 1);

		for (uint32_t uIter = 0; uIter < uMaxIterations; ++uIter)
		{
			size_t uShapeIdx = xShapeDist(xRng);
			int32_t iDir = xDirDist(xRng);

			if (TryScrambleMove(
				xLevelOut,
				axShapeStates.GetDataPointer(), axShapeStates.GetSize(),
				axDraggableIndices,
				axCatStates.GetDataPointer(), axCatStates.GetSize(),
				uShapeIdx,
				aiScrambleDeltaX[iDir], aiScrambleDeltaY[iDir],
				uCoveredMask,
				xLevelOut))
			{
				uCoveredMask = ComputeCoveredMask(
					axShapeStates.GetDataPointer(), axShapeStates.GetSize(),
					axCatStates.GetDataPointer(), axCatStates.GetSize());
				uEverCoveredMask |= uCoveredMask;
				uSuccessfulMoves++;

				if (uEverCoveredMask == uAllCatsBits &&
					uCoveredMask == 0 &&
					uSuccessfulMoves >= xParams.uScrambleMoves)
				{
					break;
				}
			}
		}

		// All cats must have been covered at some point, and none currently covered
		if (uEverCoveredMask != uAllCatsBits || uCoveredMask != 0)
			return false;

		uSuccessfulMovesOut = uSuccessfulMoves;
		return true;
	}

	/**
	 * GenerateFallbackLevel - Create a simple known-solvable level
	 */
	static void GenerateFallbackLevel(TilePuzzleLevelData& xLevelOut)
	{
		xLevelOut = TilePuzzleLevelData();

		GetShapeDefinitions().Clear();
		GetShapeDefinitions().Reserve(2);

		xLevelOut.uGridWidth = 5;
		xLevelOut.uGridHeight = 5;
		xLevelOut.aeCells.resize(25);

		for (uint32_t y = 0; y < 5; ++y)
		{
			for (uint32_t x = 0; x < 5; ++x)
			{
				uint32_t uIdx = y * 5 + x;
				bool bBorder = (x == 0 || y == 0 || x == 4 || y == 4);
				xLevelOut.aeCells[uIdx] = bBorder ? TILEPUZZLE_CELL_EMPTY : TILEPUZZLE_CELL_FLOOR;
			}
		}

		GetShapeDefinitions().PushBack(TilePuzzleShapes::GetSingleShape(true));
		GetShapeDefinitions().PushBack(TilePuzzleShapes::GetSingleShape(true));

		{
			TilePuzzleShapeInstance xShape;
			xShape.pxDefinition = &GetShapeDefinitions().Get(0);
			xShape.iOriginX = 1;
			xShape.iOriginY = 1;
			xShape.eColor = TILEPUZZLE_COLOR_RED;
			xLevelOut.axShapes.push_back(xShape);
		}

		{
			TilePuzzleShapeInstance xShape;
			xShape.pxDefinition = &GetShapeDefinitions().Get(1);
			xShape.iOriginX = 3;
			xShape.iOriginY = 1;
			xShape.eColor = TILEPUZZLE_COLOR_GREEN;
			xLevelOut.axShapes.push_back(xShape);
		}

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

		xLevelOut.uMinimumMoves = 2;
	}
};
