#pragma once
/**
 * TilePuzzle_ConditionalValidator.h - Validate conditional shape thresholds
 *
 * Ensures conditional shapes (uUnlockThreshold > 0) only exist when they
 * genuinely affect the minimum number of moves to solve a level. For each
 * conditional shape, temporarily removes the threshold and re-solves; if
 * minimum moves are unchanged, the condition is pointless and is removed.
 *
 * Used by both TilePuzzleLevelGen (new generation + registry batch) and
 * TilePuzzleRegistryViewer (UI button).
 */

#include "TilePuzzle_Types.h"
#include "TilePuzzle_Solver.h"
#include <vector>
#include <algorithm>
#include <cstdio>

static constexpr uint32_t s_uCONDITIONAL_VALIDATOR_MAX_STATES = 5000000; // 5M

namespace TilePuzzle_ConditionalValidator
{
	struct ValidationResult
	{
		uint32_t uOriginalConditionalCount = 0;
		uint32_t uRemovedCount = 0;
		uint32_t uRemainingConditionalCount = 0;
		uint32_t uOriginalMaxThreshold = 0;
		uint32_t uNewMaxThreshold = 0;
	};

	/**
	 * ValidateConditionalShapes - Remove conditional thresholds that don't affect gameplay
	 *
	 * For each conditional shape, sets its threshold to 0 and re-solves. If the
	 * minimum moves are unchanged, the threshold is permanently removed (the shape
	 * becomes unconditional). Processes in ascending threshold order.
	 *
	 * Guarantees:
	 * - Solvability is preserved (only removes thresholds when solver confirms same min moves)
	 * - No game rule changes (only modifies level data, not engine logic)
	 *
	 * @param xLevel           Level data (modified in-place)
	 * @param uSolverMaxStates Maximum BFS states for the solver
	 * @param bVerbose         Print per-shape details to stdout
	 * @return Validation result with counts
	 */
	[[maybe_unused]] static ValidationResult ValidateConditionalShapes(
		TilePuzzleLevelData& xLevel,
		uint32_t uSolverMaxStates = s_uCONDITIONAL_VALIDATOR_MAX_STATES,
		bool bVerbose = false)
	{
		ValidationResult xResult = {};

		// Count original conditionals and find max threshold
		for (size_t i = 0; i < xLevel.axShapes.size(); ++i)
		{
			const TilePuzzleShapeInstance& xShape = xLevel.axShapes[i];
			if (xShape.pxDefinition && xShape.pxDefinition->bDraggable && xShape.uUnlockThreshold > 0)
			{
				xResult.uOriginalConditionalCount++;
				if (xShape.uUnlockThreshold > xResult.uOriginalMaxThreshold)
					xResult.uOriginalMaxThreshold = xShape.uUnlockThreshold;
			}
		}

		if (xResult.uOriginalConditionalCount == 0)
			return xResult;

		uint32_t uOriginalMinMoves = xLevel.uMinimumMoves;

		// Build list of conditional shape indices, sorted by threshold ascending
		// (process weakest constraints first)
		std::vector<size_t> axConditionalIndices;
		for (size_t i = 0; i < xLevel.axShapes.size(); ++i)
		{
			if (xLevel.axShapes[i].pxDefinition &&
				xLevel.axShapes[i].pxDefinition->bDraggable &&
				xLevel.axShapes[i].uUnlockThreshold > 0)
			{
				axConditionalIndices.push_back(i);
			}
		}
		std::sort(axConditionalIndices.begin(), axConditionalIndices.end(),
			[&](size_t a, size_t b)
			{
				return xLevel.axShapes[a].uUnlockThreshold < xLevel.axShapes[b].uUnlockThreshold;
			});

		// Greedy pass: try removing each threshold
		for (size_t idx = 0; idx < axConditionalIndices.size(); ++idx)
		{
			size_t uShapeIdx = axConditionalIndices[idx];
			uint32_t uOriginalThreshold = xLevel.axShapes[uShapeIdx].uUnlockThreshold;

			// Temporarily remove threshold
			xLevel.axShapes[uShapeIdx].uUnlockThreshold = 0;

			// Re-solve
			int32_t iNewMoves = TilePuzzle_Solver::SolveLevel(xLevel, uSolverMaxStates);

			if (iNewMoves >= 0 && static_cast<uint32_t>(iNewMoves) == uOriginalMinMoves)
			{
				// Condition was pointless — keep at 0
				xResult.uRemovedCount++;
				if (bVerbose)
				{
					printf("  Shape %zu: threshold %u -> 0 (moves unchanged at %u)\n",
						uShapeIdx, uOriginalThreshold, uOriginalMinMoves);
				}
			}
			else
			{
				// Condition matters for gameplay — restore
				xLevel.axShapes[uShapeIdx].uUnlockThreshold = uOriginalThreshold;
				if (bVerbose)
				{
					printf("  Shape %zu: threshold %u kept (new moves=%d vs original=%u)\n",
						uShapeIdx, uOriginalThreshold, iNewMoves, uOriginalMinMoves);
				}
			}
		}

		// Compute final stats
		xResult.uRemainingConditionalCount = xResult.uOriginalConditionalCount - xResult.uRemovedCount;
		xResult.uNewMaxThreshold = 0;
		for (size_t i = 0; i < xLevel.axShapes.size(); ++i)
		{
			if (xLevel.axShapes[i].pxDefinition &&
				xLevel.axShapes[i].pxDefinition->bDraggable &&
				xLevel.axShapes[i].uUnlockThreshold > xResult.uNewMaxThreshold)
			{
				xResult.uNewMaxThreshold = xLevel.axShapes[i].uUnlockThreshold;
			}
		}

		return xResult;
	}

	/**
	 * UpdateMetadataConditionals - Recompute conditional-related metadata fields
	 *
	 * Call after ValidateConditionalShapes to update uNumConditionalShapes and
	 * uMaxConditionalThreshold in the metadata to match the modified level.
	 */
	[[maybe_unused]] static void UpdateMetadataConditionals(
		TilePuzzleLevelMetadata& xMeta,
		const TilePuzzleLevelData& xLevel)
	{
		uint32_t uConditional = 0;
		uint32_t uMaxThreshold = 0;
		for (size_t i = 0; i < xLevel.axShapes.size(); ++i)
		{
			const TilePuzzleShapeInstance& xShape = xLevel.axShapes[i];
			if (xShape.pxDefinition && xShape.pxDefinition->bDraggable && xShape.uUnlockThreshold > 0)
			{
				uConditional++;
				if (xShape.uUnlockThreshold > uMaxThreshold)
					uMaxThreshold = xShape.uUnlockThreshold;
			}
		}
		xMeta.uNumConditionalShapes = uConditional;
		xMeta.uMaxConditionalThreshold = uMaxThreshold;
	}
}
