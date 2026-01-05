#pragma once
/**
 * Sokoban_GridLogic.h - Movement and puzzle logic
 *
 * Demonstrates: Pure game logic separated from engine integration
 *
 * Key concepts:
 * - Grid-based movement with direction deltas
 * - Box pushing mechanics
 * - Win condition checking
 * - State queries (can move, can push)
 */

#include <cstdint>

// Tile types for the Sokoban grid
enum SokobanTileType
{
	SOKOBAN_TILE_FLOOR,
	SOKOBAN_TILE_WALL,
	SOKOBAN_TILE_TARGET,
	SOKOBAN_TILE_BOX,
	SOKOBAN_TILE_BOX_ON_TARGET,
	SOKOBAN_TILE_PLAYER,
	SOKOBAN_TILE_COUNT
};

// Movement directions
enum SokobanDirection
{
	SOKOBAN_DIR_UP,
	SOKOBAN_DIR_DOWN,
	SOKOBAN_DIR_LEFT,
	SOKOBAN_DIR_RIGHT,
	SOKOBAN_DIR_NONE
};

/**
 * Sokoban_GridLogic - Static utility class for game logic
 *
 * All methods are pure functions that operate on game state arrays.
 * This separation makes the logic testable and reusable.
 */
class Sokoban_GridLogic
{
public:
	/**
	 * GetDirectionDelta - Convert direction enum to X/Y deltas
	 *
	 * In Sokoban, Y increases downward (screen coordinates).
	 * UP = -Y, DOWN = +Y, LEFT = -X, RIGHT = +X
	 */
	static void GetDirectionDelta(SokobanDirection eDir, int32_t& iDeltaX, int32_t& iDeltaY)
	{
		iDeltaX = 0;
		iDeltaY = 0;

		switch (eDir)
		{
		case SOKOBAN_DIR_UP:    iDeltaY = -1; break;
		case SOKOBAN_DIR_DOWN:  iDeltaY = 1;  break;
		case SOKOBAN_DIR_LEFT:  iDeltaX = -1; break;
		case SOKOBAN_DIR_RIGHT: iDeltaX = 1;  break;
		default: break;
		}
	}

	/**
	 * CanMove - Check if player can move in a direction
	 *
	 * @param aeTiles     Grid tile types (walls, floors)
	 * @param abBoxes     Boolean grid of box positions
	 * @param uPlayerX    Current player X position
	 * @param uPlayerY    Current player Y position
	 * @param uGridWidth  Grid width
	 * @param uGridHeight Grid height
	 * @param eDir        Direction to check
	 * @return true if movement is possible
	 */
	static bool CanMove(
		const SokobanTileType* aeTiles,
		const bool* abBoxes,
		uint32_t uPlayerX,
		uint32_t uPlayerY,
		uint32_t uGridWidth,
		uint32_t uGridHeight,
		SokobanDirection eDir)
	{
		int32_t iDeltaX, iDeltaY;
		GetDirectionDelta(eDir, iDeltaX, iDeltaY);

		// Calculate new position
		int32_t iNewX = static_cast<int32_t>(uPlayerX) + iDeltaX;
		int32_t iNewY = static_cast<int32_t>(uPlayerY) + iDeltaY;

		// Bounds check
		if (iNewX < 0 || iNewY < 0 ||
			static_cast<uint32_t>(iNewX) >= uGridWidth ||
			static_cast<uint32_t>(iNewY) >= uGridHeight)
		{
			return false;
		}

		uint32_t uNewIndex = iNewY * uGridWidth + iNewX;

		// Can't walk into walls
		if (aeTiles[uNewIndex] == SOKOBAN_TILE_WALL)
		{
			return false;
		}

		// If there's a box, check if we can push it
		if (abBoxes[uNewIndex])
		{
			return CanPushBox(aeTiles, abBoxes, iNewX, iNewY, uGridWidth, uGridHeight, eDir);
		}

		return true;
	}

	/**
	 * CanPushBox - Check if a box can be pushed in a direction
	 *
	 * A box can be pushed if the destination is:
	 * - Within bounds
	 * - Not a wall
	 * - Not occupied by another box
	 */
	static bool CanPushBox(
		const SokobanTileType* aeTiles,
		const bool* abBoxes,
		uint32_t uBoxX,
		uint32_t uBoxY,
		uint32_t uGridWidth,
		uint32_t uGridHeight,
		SokobanDirection eDir)
	{
		int32_t iDeltaX, iDeltaY;
		GetDirectionDelta(eDir, iDeltaX, iDeltaY);

		int32_t iDestX = static_cast<int32_t>(uBoxX) + iDeltaX;
		int32_t iDestY = static_cast<int32_t>(uBoxY) + iDeltaY;

		// Bounds check
		if (iDestX < 0 || iDestY < 0 ||
			static_cast<uint32_t>(iDestX) >= uGridWidth ||
			static_cast<uint32_t>(iDestY) >= uGridHeight)
		{
			return false;
		}

		uint32_t uDestIndex = iDestY * uGridWidth + iDestX;

		// Can't push into walls
		if (aeTiles[uDestIndex] == SOKOBAN_TILE_WALL)
		{
			return false;
		}

		// Can't push into another box
		if (abBoxes[uDestIndex])
		{
			return false;
		}

		return true;
	}

	/**
	 * PushBox - Move a box in the specified direction
	 *
	 * Call this ONLY after CanPushBox returns true.
	 * Modifies the abBoxes array in place.
	 */
	static void PushBox(
		bool* abBoxes,
		uint32_t uFromX,
		uint32_t uFromY,
		uint32_t uGridWidth,
		SokobanDirection eDir)
	{
		int32_t iDeltaX, iDeltaY;
		GetDirectionDelta(eDir, iDeltaX, iDeltaY);

		uint32_t uFromIndex = uFromY * uGridWidth + uFromX;
		uint32_t uToX = uFromX + iDeltaX;
		uint32_t uToY = uFromY + iDeltaY;
		uint32_t uToIndex = uToY * uGridWidth + uToX;

		abBoxes[uFromIndex] = false;
		abBoxes[uToIndex] = true;
	}

	/**
	 * CountBoxesOnTargets - Count how many boxes are on target positions
	 */
	static uint32_t CountBoxesOnTargets(
		const bool* abBoxes,
		const bool* abTargets,
		uint32_t uGridSize)
	{
		uint32_t uCount = 0;
		for (uint32_t i = 0; i < uGridSize; i++)
		{
			if (abBoxes[i] && abTargets[i])
			{
				uCount++;
			}
		}
		return uCount;
	}

	/**
	 * CheckWinCondition - Check if all boxes are on targets
	 */
	static bool CheckWinCondition(
		const bool* abBoxes,
		const bool* abTargets,
		uint32_t uGridSize,
		uint32_t uTargetCount)
	{
		return CountBoxesOnTargets(abBoxes, abTargets, uGridSize) == uTargetCount && uTargetCount > 0;
	}
};
