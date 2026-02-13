#pragma once
/**
 * TilePuzzle_Rules.h - Shared game rules (single source of truth)
 *
 * Contains ALL gameplay rule logic used by both the solver and the game.
 * This ensures the solver and runtime can never interpret rules differently.
 *
 * Rules:
 * - Shapes can only move onto valid floor cells within grid bounds
 * - Shapes cannot overlap static blockers or other draggable shapes
 * - Shapes cannot move onto a cat of a DIFFERENT color (blocked)
 * - Shapes CAN move onto a cat of the SAME color (cat gets eliminated)
 * - A level is complete when all cats are eliminated
 */

#include "TilePuzzle_Types.h"

namespace TilePuzzle_Rules
{
	// ========================================================================
	// Lightweight state views (no entity/visual data)
	// ========================================================================

	struct ShapeState
	{
		const TilePuzzleShapeDefinition* pxDefinition;
		int32_t iOriginX;
		int32_t iOriginY;
		TilePuzzleColor eColor;
	};

	struct CatState
	{
		int32_t iGridX;
		int32_t iGridY;
		TilePuzzleColor eColor;
	};

	// ========================================================================
	// Movement validation
	// ========================================================================

	/**
	 * CanMoveShape - Check if a draggable shape can move to a new position
	 *
	 * @param xLevel             Level data (grid cells, static blockers in axShapes)
	 * @param axDraggableShapes  Array of all draggable shapes with current positions
	 * @param uNumDraggableShapes Number of draggable shapes
	 * @param uMovingShapeIdx    Index into axDraggableShapes for the shape being moved
	 * @param iNewOriginX        Proposed new origin X
	 * @param iNewOriginY        Proposed new origin Y
	 * @param axCats             Array of cat states
	 * @param uNumCats           Number of cats
	 * @param uEliminatedCatsMask Bitmask of already-eliminated cats
	 * @return true if the move is valid
	 */
	inline bool CanMoveShape(
		const TilePuzzleLevelData& xLevel,
		const ShapeState* axDraggableShapes, size_t uNumDraggableShapes,
		size_t uMovingShapeIdx,
		int32_t iNewOriginX, int32_t iNewOriginY,
		const CatState* axCats, size_t uNumCats,
		uint32_t uEliminatedCatsMask)
	{
		const ShapeState& xMoving = axDraggableShapes[uMovingShapeIdx];
		if (!xMoving.pxDefinition)
			return false;

		for (size_t uCellIdx = 0; uCellIdx < xMoving.pxDefinition->axCells.size(); ++uCellIdx)
		{
			const TilePuzzleCellOffset& xOffset = xMoving.pxDefinition->axCells[uCellIdx];
			int32_t iCellX = iNewOriginX + xOffset.iX;
			int32_t iCellY = iNewOriginY + xOffset.iY;

			// 1. Bounds check
			if (iCellX < 0 || iCellY < 0 ||
				static_cast<uint32_t>(iCellX) >= xLevel.uGridWidth ||
				static_cast<uint32_t>(iCellY) >= xLevel.uGridHeight)
			{
				return false;
			}

			// 2. Floor check
			uint32_t uIdx = iCellY * xLevel.uGridWidth + iCellX;
			if (xLevel.aeCells[uIdx] != TILEPUZZLE_CELL_FLOOR)
			{
				return false;
			}

			// 3. Static blocker collision
			for (size_t i = 0; i < xLevel.axShapes.size(); ++i)
			{
				const TilePuzzleShapeInstance& xOther = xLevel.axShapes[i];
				if (!xOther.pxDefinition || xOther.pxDefinition->bDraggable)
					continue;

				for (size_t j = 0; j < xOther.pxDefinition->axCells.size(); ++j)
				{
					const TilePuzzleCellOffset& xOtherOffset = xOther.pxDefinition->axCells[j];
					int32_t iOtherX = xOther.iOriginX + xOtherOffset.iX;
					int32_t iOtherY = xOther.iOriginY + xOtherOffset.iY;
					if (iCellX == iOtherX && iCellY == iOtherY)
					{
						return false;
					}
				}
			}

			// 4. Other draggable shape collision
			for (size_t i = 0; i < uNumDraggableShapes; ++i)
			{
				if (i == uMovingShapeIdx)
					continue;

				const ShapeState& xOther = axDraggableShapes[i];
				if (!xOther.pxDefinition)
					continue;

				for (size_t j = 0; j < xOther.pxDefinition->axCells.size(); ++j)
				{
					const TilePuzzleCellOffset& xOtherOffset = xOther.pxDefinition->axCells[j];
					int32_t iOtherX = xOther.iOriginX + xOtherOffset.iX;
					int32_t iOtherY = xOther.iOriginY + xOtherOffset.iY;
					if (iCellX == iOtherX && iCellY == iOtherY)
					{
						return false;
					}
				}
			}

			// 5. Cat collision - wrong-color non-eliminated cats block movement
			for (size_t i = 0; i < uNumCats; ++i)
			{
				if (uEliminatedCatsMask & (1u << i))
					continue;

				if (axCats[i].iGridX == iCellX && axCats[i].iGridY == iCellY)
				{
					if (axCats[i].eColor != xMoving.eColor)
					{
						return false;
					}
					// Same color: allowed (cat will be eliminated after move)
				}
			}
		}

		return true;
	}

	// ========================================================================
	// Cat elimination
	// ========================================================================

	/**
	 * ComputeNewlyEliminatedCats - Find which cats are eliminated by current shape positions
	 *
	 * Returns a bitmask of cat indices that are NEWLY eliminated (not already in uAlreadyEliminatedMask).
	 * A cat is eliminated when a same-color draggable shape overlaps it.
	 */
	inline uint32_t ComputeNewlyEliminatedCats(
		const ShapeState* axDraggableShapes, size_t uNumDraggableShapes,
		const CatState* axCats, size_t uNumCats,
		uint32_t uAlreadyEliminatedMask)
	{
		uint32_t uNewlyEliminated = 0;

		for (size_t uShapeIdx = 0; uShapeIdx < uNumDraggableShapes; ++uShapeIdx)
		{
			const ShapeState& xShape = axDraggableShapes[uShapeIdx];
			if (!xShape.pxDefinition)
				continue;

			for (size_t uCellIdx = 0; uCellIdx < xShape.pxDefinition->axCells.size(); ++uCellIdx)
			{
				const TilePuzzleCellOffset& xOffset = xShape.pxDefinition->axCells[uCellIdx];
				int32_t iCellX = xShape.iOriginX + xOffset.iX;
				int32_t iCellY = xShape.iOriginY + xOffset.iY;

				for (size_t uCatIdx = 0; uCatIdx < uNumCats; ++uCatIdx)
				{
					if (uAlreadyEliminatedMask & (1u << uCatIdx))
						continue;

					if (axCats[uCatIdx].iGridX == iCellX &&
						axCats[uCatIdx].iGridY == iCellY &&
						axCats[uCatIdx].eColor == xShape.eColor)
					{
						uNewlyEliminated |= (1u << uCatIdx);
					}
				}
			}
		}

		return uNewlyEliminated;
	}

	// ========================================================================
	// Win condition
	// ========================================================================

	/**
	 * AreAllCatsEliminated - Check if all cats have been eliminated
	 */
	inline bool AreAllCatsEliminated(uint32_t uEliminatedMask, uint32_t uTotalCats)
	{
		if (uTotalCats == 0)
			return true;

		uint32_t uAllEliminatedMask = (1u << uTotalCats) - 1;
		return uEliminatedMask == uAllEliminatedMask;
	}
}
