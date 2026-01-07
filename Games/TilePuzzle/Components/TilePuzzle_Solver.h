#pragma once
/**
 * TilePuzzle_Solver.h - BFS level solver
 *
 * Demonstrates: Algorithm implementation with pure C++ (no engine dependencies)
 *
 * Key concepts:
 * - Breadth-first search for optimal solution
 * - State space exploration with visited set
 * - Custom hash functions for complex state types
 * - Performance limiting to avoid infinite loops
 *
 * For TilePuzzle:
 * - State includes positions of all draggable shapes + remaining cat positions
 * - Shapes can only move if all cells would land on valid floor
 * - Cats are eliminated when matching colored shape overlaps them
 * - Level is solved when all cats are eliminated
 */

#include <queue>
#include <unordered_set>
#include <vector>
#include <algorithm>
#include <cstdint>

#include "TilePuzzle_Types.h"

// Constants for solver behavior
static constexpr uint32_t s_uTilePuzzleMaxSolverStates = 50000;  // Limit state exploration

/**
 * TilePuzzleSolverShapeState - Position of a single shape
 */
struct TilePuzzleSolverShapeState
{
	int32_t iOriginX;
	int32_t iOriginY;

	bool operator==(const TilePuzzleSolverShapeState& xOther) const
	{
		return iOriginX == xOther.iOriginX && iOriginY == xOther.iOriginY;
	}
};

/**
 * TilePuzzleSolverState - Represents a game state for the solver
 *
 * A state consists of:
 * - Positions of all draggable shapes
 * - Which cats have been eliminated (as bitmask)
 */
struct TilePuzzleSolverState
{
	std::vector<TilePuzzleSolverShapeState> axShapePositions;
	uint32_t uEliminatedCatsMask;  // Bitmask of eliminated cats (up to 32 cats)

	bool operator==(const TilePuzzleSolverState& xOther) const
	{
		return axShapePositions == xOther.axShapePositions &&
			   uEliminatedCatsMask == xOther.uEliminatedCatsMask;
	}
};

/**
 * TilePuzzleSolverStateHash - Custom hash function for TilePuzzleSolverState
 */
struct TilePuzzleSolverStateHash
{
	size_t operator()(const TilePuzzleSolverState& xState) const
	{
		size_t uHash = std::hash<uint32_t>()(xState.uEliminatedCatsMask);

		for (const auto& xShape : xState.axShapePositions)
		{
			uHash ^= std::hash<int32_t>()(xShape.iOriginX) + 0x9e3779b9 + (uHash << 6) + (uHash >> 2);
			uHash ^= std::hash<int32_t>()(xShape.iOriginY) + 0x9e3779b9 + (uHash << 6) + (uHash >> 2);
		}
		return uHash;
	}
};

/**
 * TilePuzzle_Solver - BFS-based level solver
 *
 * Explores all possible game states using breadth-first search
 * to find the minimum number of moves to solve the level.
 */
class TilePuzzle_Solver
{
public:
	/**
	 * SolveLevel - Find minimum moves to solve the level
	 *
	 * @param xLevel  The level data to solve
	 * @return Minimum moves to solve, or -1 if unsolvable/too complex
	 */
	static int32_t SolveLevel(const TilePuzzleLevelData& xLevel)
	{
		// Create initial state
		TilePuzzleSolverState xInitialState;
		xInitialState.uEliminatedCatsMask = 0;

		// Collect indices of draggable shapes
		std::vector<size_t> axDraggableIndices;
		for (size_t i = 0; i < xLevel.axShapes.size(); ++i)
		{
			if (xLevel.axShapes[i].pxDefinition && xLevel.axShapes[i].pxDefinition->bDraggable)
			{
				axDraggableIndices.push_back(i);
				TilePuzzleSolverShapeState xShapeState;
				xShapeState.iOriginX = xLevel.axShapes[i].iOriginX;
				xShapeState.iOriginY = xLevel.axShapes[i].iOriginY;
				xInitialState.axShapePositions.push_back(xShapeState);
			}
		}

		// If no draggable shapes, check if already solved
		if (axDraggableIndices.empty())
		{
			return xLevel.axCats.empty() ? 0 : -1;
		}

		// Check if already solved
		if (IsStateSolved(xInitialState, static_cast<uint32_t>(xLevel.axCats.size())))
		{
			return 0;
		}

		// BFS setup
		std::queue<std::pair<TilePuzzleSolverState, int32_t>> xQueue;
		std::unordered_set<TilePuzzleSolverState, TilePuzzleSolverStateHash> xVisited;

		xQueue.push({xInitialState, 0});
		xVisited.insert(xInitialState);

		// Direction deltas: UP, DOWN, LEFT, RIGHT
		int32_t aiDeltaX[] = {0, 0, -1, 1};
		int32_t aiDeltaY[] = {-1, 1, 0, 0};

		// BFS exploration
		while (!xQueue.empty() && xVisited.size() < s_uTilePuzzleMaxSolverStates)
		{
			auto [xCurrentState, iMoves] = xQueue.front();
			xQueue.pop();

			// Try moving each draggable shape
			for (size_t uShapeIdx = 0; uShapeIdx < axDraggableIndices.size(); ++uShapeIdx)
			{
				size_t uOriginalShapeIdx = axDraggableIndices[uShapeIdx];
				const TilePuzzleShapeInstance& xShape = xLevel.axShapes[uOriginalShapeIdx];

				// Try all four directions
				for (int32_t iDir = 0; iDir < 4; ++iDir)
				{
					int32_t iNewOriginX = xCurrentState.axShapePositions[uShapeIdx].iOriginX + aiDeltaX[iDir];
					int32_t iNewOriginY = xCurrentState.axShapePositions[uShapeIdx].iOriginY + aiDeltaY[iDir];

					// Check if the move is valid
					if (!CanMoveShape(xLevel, xCurrentState, axDraggableIndices, uShapeIdx,
									  xShape, iNewOriginX, iNewOriginY))
					{
						continue;
					}

					// Create new state with shape moved
					TilePuzzleSolverState xNewState = xCurrentState;
					xNewState.axShapePositions[uShapeIdx].iOriginX = iNewOriginX;
					xNewState.axShapePositions[uShapeIdx].iOriginY = iNewOriginY;

					// Check for cat elimination
					CheckCatElimination(xLevel, xNewState, axDraggableIndices, uShapeIdx, xShape);

					// Skip if we've visited this state
					if (xVisited.find(xNewState) != xVisited.end())
					{
						continue;
					}

					// Check if this state is solved
					if (IsStateSolved(xNewState, static_cast<uint32_t>(xLevel.axCats.size())))
					{
						return iMoves + 1;
					}

					// Add to queue for further exploration
					xVisited.insert(xNewState);
					xQueue.push({xNewState, iMoves + 1});
				}
			}
		}

		// Unsolvable or too complex
		return -1;
	}

	/**
	 * IsSolvable - Quick check if level is solvable
	 */
	static bool IsSolvable(const TilePuzzleLevelData& xLevel)
	{
		return SolveLevel(xLevel) >= 0;
	}

private:
	/**
	 * CanMoveShape - Check if a shape can move to a new position
	 */
	static bool CanMoveShape(
		const TilePuzzleLevelData& xLevel,
		const TilePuzzleSolverState& xState,
		const std::vector<size_t>& axDraggableIndices,
		size_t uMovingShapeIdx,
		const TilePuzzleShapeInstance& xShape,
		int32_t iNewOriginX,
		int32_t iNewOriginY)
	{
		if (!xShape.pxDefinition)
			return false;

		// Check each cell of the shape
		for (const auto& xOffset : xShape.pxDefinition->axCells)
		{
			int32_t iCellX = iNewOriginX + xOffset.iX;
			int32_t iCellY = iNewOriginY + xOffset.iY;

			// Bounds check
			if (iCellX < 0 || iCellY < 0 ||
				static_cast<uint32_t>(iCellX) >= xLevel.uGridWidth ||
				static_cast<uint32_t>(iCellY) >= xLevel.uGridHeight)
			{
				return false;
			}

			uint32_t uIdx = iCellY * xLevel.uGridWidth + iCellX;

			// Can't move onto empty cells
			if (xLevel.aeCells[uIdx] != TILEPUZZLE_CELL_FLOOR)
			{
				return false;
			}

			// Check collision with static blockers (non-draggable shapes)
			for (size_t i = 0; i < xLevel.axShapes.size(); ++i)
			{
				const auto& xOther = xLevel.axShapes[i];
				if (!xOther.pxDefinition || xOther.pxDefinition->bDraggable)
					continue;

				for (const auto& xOtherOffset : xOther.pxDefinition->axCells)
				{
					int32_t iOtherX = xOther.iOriginX + xOtherOffset.iX;
					int32_t iOtherY = xOther.iOriginY + xOtherOffset.iY;
					if (iCellX == iOtherX && iCellY == iOtherY)
					{
						return false;
					}
				}
			}

			// Check collision with other draggable shapes
			for (size_t i = 0; i < axDraggableIndices.size(); ++i)
			{
				if (i == uMovingShapeIdx)
					continue;

				const auto& xOther = xLevel.axShapes[axDraggableIndices[i]];
				if (!xOther.pxDefinition)
					continue;

				int32_t iOtherOriginX = xState.axShapePositions[i].iOriginX;
				int32_t iOtherOriginY = xState.axShapePositions[i].iOriginY;

				for (const auto& xOtherOffset : xOther.pxDefinition->axCells)
				{
					int32_t iOtherX = iOtherOriginX + xOtherOffset.iX;
					int32_t iOtherY = iOtherOriginY + xOtherOffset.iY;
					if (iCellX == iOtherX && iCellY == iOtherY)
					{
						return false;
					}
				}
			}
		}

		return true;
	}

	/**
	 * CheckCatElimination - Check and mark cats eliminated by the moved shape
	 */
	static void CheckCatElimination(
		const TilePuzzleLevelData& xLevel,
		TilePuzzleSolverState& xState,
		const std::vector<size_t>& axDraggableIndices,
		size_t uMovedShapeIdx,
		const TilePuzzleShapeInstance& xShape)
	{
		if (!xShape.pxDefinition)
			return;

		int32_t iShapeOriginX = xState.axShapePositions[uMovedShapeIdx].iOriginX;
		int32_t iShapeOriginY = xState.axShapePositions[uMovedShapeIdx].iOriginY;

		// Check each cell of the shape for cat overlap
		for (const auto& xOffset : xShape.pxDefinition->axCells)
		{
			int32_t iCellX = iShapeOriginX + xOffset.iX;
			int32_t iCellY = iShapeOriginY + xOffset.iY;

			// Check each cat
			for (size_t uCatIdx = 0; uCatIdx < xLevel.axCats.size(); ++uCatIdx)
			{
				// Skip if already eliminated
				if (xState.uEliminatedCatsMask & (1u << uCatIdx))
					continue;

				const auto& xCat = xLevel.axCats[uCatIdx];

				// Check if cat is at this cell and colors match
				if (xCat.iGridX == iCellX && xCat.iGridY == iCellY && xCat.eColor == xShape.eColor)
				{
					xState.uEliminatedCatsMask |= (1u << uCatIdx);
				}
			}
		}
	}

	/**
	 * IsStateSolved - Check if all cats are eliminated
	 */
	static bool IsStateSolved(const TilePuzzleSolverState& xState, uint32_t uTotalCats)
	{
		if (uTotalCats == 0)
			return true;

		// All cats eliminated means all bits up to uTotalCats are set
		uint32_t uAllEliminatedMask = (1u << uTotalCats) - 1;
		return xState.uEliminatedCatsMask == uAllEliminatedMask;
	}
};
