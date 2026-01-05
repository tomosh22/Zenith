#pragma once
/**
 * Sokoban_Solver.h - BFS level solver
 *
 * Demonstrates: Algorithm implementation with pure C++ (no engine dependencies)
 *
 * Key concepts:
 * - Breadth-first search for optimal solution
 * - State space exploration with visited set
 * - Custom hash functions for complex state types
 * - Performance limiting to avoid infinite loops
 *
 * This module is useful for:
 * - Validating generated levels are solvable
 * - Calculating minimum moves for scoring
 * - Demonstrating algorithm patterns in games
 */

#include <queue>
#include <unordered_set>
#include <vector>
#include <algorithm>
#include <cstdint>

#include "Sokoban_GridLogic.h"  // For SokobanTileType, SokobanDirection enums

// Constants for solver behavior
static constexpr uint32_t s_uMaxSolverStates = 100000;  // Limit state exploration

/**
 * SolverState - Represents a game state for the solver
 *
 * A state consists of:
 * - Player position (X, Y)
 * - Sorted list of box positions (as grid indices)
 *
 * Box positions are sorted for consistent hashing.
 */
struct SolverState
{
	uint32_t uPlayerX;
	uint32_t uPlayerY;
	std::vector<uint32_t> axBoxPositions;  // Sorted box positions

	bool operator==(const SolverState& xOther) const
	{
		return uPlayerX == xOther.uPlayerX &&
			   uPlayerY == xOther.uPlayerY &&
			   axBoxPositions == xOther.axBoxPositions;
	}
};

/**
 * SolverStateHash - Custom hash function for SolverState
 *
 * Uses XOR combination with prime number mixing.
 * Required for std::unordered_set.
 */
struct SolverStateHash
{
	size_t operator()(const SolverState& xState) const
	{
		size_t uHash = std::hash<uint32_t>()(xState.uPlayerX);
		uHash ^= std::hash<uint32_t>()(xState.uPlayerY) << 1;

		// Mix in box positions with golden ratio constant
		for (uint32_t uPos : xState.axBoxPositions)
		{
			uHash ^= std::hash<uint32_t>()(uPos) + 0x9e3779b9 + (uHash << 6) + (uHash >> 2);
		}
		return uHash;
	}
};

/**
 * Sokoban_Solver - BFS-based level solver
 *
 * Explores all possible game states using breadth-first search
 * to find the minimum number of moves to solve the level.
 */
class Sokoban_Solver
{
public:
	/**
	 * SolveLevel - Find minimum moves to solve the level
	 *
	 * @param aeTiles      Grid tile types (walls, floors)
	 * @param abBoxes      Boolean grid of initial box positions
	 * @param abTargets    Boolean grid of target positions
	 * @param uPlayerX     Initial player X position
	 * @param uPlayerY     Initial player Y position
	 * @param uGridWidth   Grid width
	 * @param uGridHeight  Grid height
	 * @return Minimum moves to solve, or -1 if unsolvable/too complex
	 */
	static int32_t SolveLevel(
		const SokobanTileType* aeTiles,
		const bool* abBoxes,
		const bool* abTargets,
		uint32_t uPlayerX,
		uint32_t uPlayerY,
		uint32_t uGridWidth,
		uint32_t uGridHeight)
	{
		// Create initial state
		SolverState xInitialState;
		xInitialState.uPlayerX = uPlayerX;
		xInitialState.uPlayerY = uPlayerY;

		// Collect initial box positions
		for (uint32_t i = 0; i < uGridWidth * uGridHeight; i++)
		{
			if (abBoxes[i])
			{
				xInitialState.axBoxPositions.push_back(i);
			}
		}
		std::sort(xInitialState.axBoxPositions.begin(), xInitialState.axBoxPositions.end());

		// Check if already solved
		if (IsStateSolved(xInitialState, abTargets))
		{
			return 0;
		}

		// BFS setup
		std::queue<std::pair<SolverState, int32_t>> xQueue;
		std::unordered_set<SolverState, SolverStateHash> xVisited;

		xQueue.push({xInitialState, 0});
		xVisited.insert(xInitialState);

		// Direction deltas: UP, DOWN, LEFT, RIGHT
		int32_t aDeltaX[] = {0, 0, -1, 1};
		int32_t aDeltaY[] = {-1, 1, 0, 0};

		// BFS exploration
		while (!xQueue.empty() && xVisited.size() < s_uMaxSolverStates)
		{
			auto [xCurrentState, iMoves] = xQueue.front();
			xQueue.pop();

			// Try all four directions
			for (int iDir = 0; iDir < 4; iDir++)
			{
				int32_t iNewX = static_cast<int32_t>(xCurrentState.uPlayerX) + aDeltaX[iDir];
				int32_t iNewY = static_cast<int32_t>(xCurrentState.uPlayerY) + aDeltaY[iDir];

				// Bounds check
				if (iNewX < 0 || iNewY < 0 ||
					static_cast<uint32_t>(iNewX) >= uGridWidth ||
					static_cast<uint32_t>(iNewY) >= uGridHeight)
				{
					continue;
				}

				uint32_t uNewIndex = iNewY * uGridWidth + iNewX;

				// Can't walk into walls
				if (aeTiles[uNewIndex] == SOKOBAN_TILE_WALL)
				{
					continue;
				}

				// Check if there's a box at the new position
				auto it = std::find(
					xCurrentState.axBoxPositions.begin(),
					xCurrentState.axBoxPositions.end(),
					uNewIndex);

				SolverState xNewState = xCurrentState;
				xNewState.uPlayerX = iNewX;
				xNewState.uPlayerY = iNewY;

				if (it != xCurrentState.axBoxPositions.end())
				{
					// There's a box - try to push it
					int32_t iBoxNewX = iNewX + aDeltaX[iDir];
					int32_t iBoxNewY = iNewY + aDeltaY[iDir];

					// Bounds check for box destination
					if (iBoxNewX < 0 || iBoxNewY < 0 ||
						static_cast<uint32_t>(iBoxNewX) >= uGridWidth ||
						static_cast<uint32_t>(iBoxNewY) >= uGridHeight)
					{
						continue;
					}

					uint32_t uBoxNewIndex = iBoxNewY * uGridWidth + iBoxNewX;

					// Can't push into wall
					if (aeTiles[uBoxNewIndex] == SOKOBAN_TILE_WALL)
					{
						continue;
					}

					// Can't push into another box
					if (std::find(xCurrentState.axBoxPositions.begin(),
								  xCurrentState.axBoxPositions.end(),
								  uBoxNewIndex) != xCurrentState.axBoxPositions.end())
					{
						continue;
					}

					// Update box position in new state
					xNewState.axBoxPositions.erase(
						std::find(xNewState.axBoxPositions.begin(),
								  xNewState.axBoxPositions.end(),
								  uNewIndex));
					xNewState.axBoxPositions.push_back(uBoxNewIndex);
					std::sort(xNewState.axBoxPositions.begin(), xNewState.axBoxPositions.end());
				}

				// Skip if we've visited this state
				if (xVisited.find(xNewState) != xVisited.end())
				{
					continue;
				}

				// Check if this state is solved
				if (IsStateSolved(xNewState, abTargets))
				{
					return iMoves + 1;
				}

				// Add to queue for further exploration
				xVisited.insert(xNewState);
				xQueue.push({xNewState, iMoves + 1});
			}
		}

		// Unsolvable or too complex
		return -1;
	}

private:
	/**
	 * IsStateSolved - Check if all boxes are on targets
	 */
	static bool IsStateSolved(const SolverState& xState, const bool* abTargets)
	{
		for (uint32_t uBoxPos : xState.axBoxPositions)
		{
			if (!abTargets[uBoxPos])
			{
				return false;
			}
		}
		return !xState.axBoxPositions.empty();
	}
};
