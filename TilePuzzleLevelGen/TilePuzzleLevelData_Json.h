#pragma once
/**
 * TilePuzzleLevelData_Json.h - Human-readable JSON export for TilePuzzleLevelData
 *
 * Exports level data as pretty-printed JSON with grid visualization,
 * shape/cat data, and comprehensive per-level generation analytics.
 */

#include <cstdio>
#include <cstdint>

#include "Components/TilePuzzle_Types.h"
#include "Components/TilePuzzle_LevelGenerator.h"

namespace TilePuzzleLevelJson
{
	static const char* ColorToString(TilePuzzleColor eColor)
	{
		switch (eColor)
		{
		case TILEPUZZLE_COLOR_RED:    return "Red";
		case TILEPUZZLE_COLOR_GREEN:  return "Green";
		case TILEPUZZLE_COLOR_BLUE:   return "Blue";
		case TILEPUZZLE_COLOR_YELLOW: return "Yellow";
		case TILEPUZZLE_COLOR_PURPLE: return "Purple";
		case TILEPUZZLE_COLOR_NONE:   return "None";
		default:                      return "Unknown";
		}
	}

	static const char* ShapeTypeToString(TilePuzzleShapeType eType)
	{
		switch (eType)
		{
		case TILEPUZZLE_SHAPE_SINGLE: return "Single";
		case TILEPUZZLE_SHAPE_DOMINO: return "Domino";
		case TILEPUZZLE_SHAPE_L:      return "L";
		case TILEPUZZLE_SHAPE_T:      return "T";
		case TILEPUZZLE_SHAPE_I:      return "I";
		case TILEPUZZLE_SHAPE_S:      return "S";
		case TILEPUZZLE_SHAPE_Z:      return "Z";
		case TILEPUZZLE_SHAPE_O:      return "O";
		default:                      return "Unknown";
		}
	}

	static void WriteParamStats(FILE* pFile, const char* szName, const TilePuzzle_LevelGenerator::ParamValueStats& xStats, bool bLastParam)
	{
		fprintf(pFile, "      \"%s\": {\n", szName);
		bool bFirstValue = true;
		for (uint32_t v = 0; v < s_uParamStatsMaxValues; ++v)
		{
			uint32_t uTotal = xStats.auScrambleFailures[v] + xStats.auSolverTooEasy[v] +
				xStats.auSolverUnsolvable[v] + xStats.auSuccesses[v];
			if (uTotal == 0)
				continue;

			if (!bFirstValue)
				fprintf(pFile, ",\n");
			bFirstValue = false;

			float fSuccessRate = 100.0f * xStats.auSuccesses[v] / uTotal;
			float fScrambleRate = 100.0f * xStats.auScrambleFailures[v] / uTotal;
			float fTooEasyRate = 100.0f * xStats.auSolverTooEasy[v] / uTotal;
			float fUnsolvableRate = 100.0f * xStats.auSolverUnsolvable[v] / uTotal;

			fprintf(pFile, "        \"%u\": { \"total\": %u, \"scrambleFail\": %u, \"tooEasy\": %u, \"unsolvable\": %u, \"success\": %u, \"successRate\": %.1f, \"scrambleRate\": %.1f, \"tooEasyRate\": %.1f, \"unsolvableRate\": %.1f }",
				v, uTotal,
				xStats.auScrambleFailures[v], xStats.auSolverTooEasy[v],
				xStats.auSolverUnsolvable[v], xStats.auSuccesses[v],
				fSuccessRate, fScrambleRate, fTooEasyRate, fUnsolvableRate);
		}
		fprintf(pFile, "\n      }%s\n", bLastParam ? "" : ",");
	}

	static void Write(
		const char* szPath,
		const TilePuzzleLevelData& xLevel,
		uint32_t uLevelNumber,
		const TilePuzzle_LevelGenerator::GenerationStats* pxStats,
		double fGenerationTimeMs,
		uint32_t uRetryRounds = 1,
		const TilePuzzle_LevelGenerator::DifficultyParams* pxParams = nullptr)
	{
		FILE* pFile = fopen(szPath, "w");
		if (!pFile)
			return;

		fprintf(pFile, "{\n");

		// Level info
		fprintf(pFile, "  \"levelNumber\": %u,\n", uLevelNumber);
		fprintf(pFile, "  \"gridWidth\": %u,\n", xLevel.uGridWidth);
		fprintf(pFile, "  \"gridHeight\": %u,\n", xLevel.uGridHeight);
		fprintf(pFile, "  \"minimumMoves\": %u,\n", xLevel.uMinimumMoves);

		// Grid visualization
		fprintf(pFile, "  \"grid\": [\n");
		for (uint32_t y = 0; y < xLevel.uGridHeight; ++y)
		{
			fprintf(pFile, "    \"");
			for (uint32_t x = 0; x < xLevel.uGridWidth; ++x)
			{
				uint32_t uIdx = y * xLevel.uGridWidth + x;
				if (uIdx < xLevel.aeCells.size())
				{
					switch (xLevel.aeCells[uIdx])
					{
					case TILEPUZZLE_CELL_EMPTY: fprintf(pFile, "."); break;
					case TILEPUZZLE_CELL_FLOOR: fprintf(pFile, "#"); break;
					default: fprintf(pFile, "?"); break;
					}
				}
			}
			fprintf(pFile, "\"%s\n", (y < xLevel.uGridHeight - 1) ? "," : "");
		}
		fprintf(pFile, "  ],\n");

		// Shapes
		fprintf(pFile, "  \"shapes\": [\n");
		for (size_t i = 0; i < xLevel.axShapes.size(); ++i)
		{
			const TilePuzzleShapeInstance& xShape = xLevel.axShapes[i];
			fprintf(pFile, "    {\n");
			fprintf(pFile, "      \"color\": \"%s\",\n", ColorToString(xShape.eColor));
			fprintf(pFile, "      \"originX\": %d,\n", xShape.iOriginX);
			fprintf(pFile, "      \"originY\": %d,\n", xShape.iOriginY);

			if (xShape.pxDefinition)
			{
				fprintf(pFile, "      \"type\": \"%s\",\n", ShapeTypeToString(xShape.pxDefinition->eType));
				fprintf(pFile, "      \"draggable\": %s,\n", xShape.pxDefinition->bDraggable ? "true" : "false");
				fprintf(pFile, "      \"cells\": [");
				for (size_t c = 0; c < xShape.pxDefinition->axCells.size(); ++c)
				{
					fprintf(pFile, "[%d, %d]%s",
						xShape.pxDefinition->axCells[c].iX,
						xShape.pxDefinition->axCells[c].iY,
						(c < xShape.pxDefinition->axCells.size() - 1) ? ", " : "");
				}
				fprintf(pFile, "]");
			}
			else
			{
				fprintf(pFile, "      \"type\": null,\n");
				fprintf(pFile, "      \"draggable\": false");
			}

			if (xShape.uUnlockThreshold > 0)
				fprintf(pFile, ",\n      \"unlockThreshold\": %u", xShape.uUnlockThreshold);

			fprintf(pFile, "\n    }%s\n", (i < xLevel.axShapes.size() - 1) ? "," : "");
		}
		fprintf(pFile, "  ],\n");

		// Cats
		fprintf(pFile, "  \"cats\": [\n");
		for (size_t i = 0; i < xLevel.axCats.size(); ++i)
		{
			const TilePuzzleCatData& xCat = xLevel.axCats[i];
			fprintf(pFile, "    {\n");
			fprintf(pFile, "      \"color\": \"%s\",\n", ColorToString(xCat.eColor));
			fprintf(pFile, "      \"gridX\": %d,\n", xCat.iGridX);
			fprintf(pFile, "      \"gridY\": %d,\n", xCat.iGridY);
			fprintf(pFile, "      \"onBlocker\": %s\n", xCat.bOnBlocker ? "true" : "false");
			fprintf(pFile, "    }%s\n", (i < xLevel.axCats.size() - 1) ? "," : "");
		}
		fprintf(pFile, "  ],\n");

		// Level composition - derived counts for quick understanding
		uint32_t uDraggableShapes = 0;
		uint32_t uBlockerShapes = 0;
		uint32_t uConditionalShapes = 0;
		uint32_t uTotalCats = static_cast<uint32_t>(xLevel.axCats.size());
		uint32_t uBlockerCats = 0;
		uint32_t uFloorCells = 0;
		for (size_t i = 0; i < xLevel.axShapes.size(); ++i)
		{
			if (xLevel.axShapes[i].uUnlockThreshold > 0)
				uConditionalShapes++;

			if (xLevel.axShapes[i].pxDefinition && xLevel.axShapes[i].pxDefinition->bDraggable)
				uDraggableShapes++;
			else
				uBlockerShapes++;
		}
		for (size_t i = 0; i < xLevel.axCats.size(); ++i)
		{
			if (xLevel.axCats[i].bOnBlocker)
				uBlockerCats++;
		}
		for (size_t i = 0; i < xLevel.aeCells.size(); ++i)
		{
			if (xLevel.aeCells[i] == TILEPUZZLE_CELL_FLOOR)
				uFloorCells++;
		}

		fprintf(pFile, "  \"composition\": {\n");
		fprintf(pFile, "    \"floorCells\": %u,\n", uFloorCells);
		fprintf(pFile, "    \"totalShapes\": %u,\n", static_cast<uint32_t>(xLevel.axShapes.size()));
		fprintf(pFile, "    \"draggableShapes\": %u,\n", uDraggableShapes);
		fprintf(pFile, "    \"blockerShapes\": %u,\n", uBlockerShapes);
		fprintf(pFile, "    \"conditionalShapes\": %u,\n", uConditionalShapes);
		fprintf(pFile, "    \"totalCats\": %u,\n", uTotalCats);
		fprintf(pFile, "    \"blockerCats\": %u\n", uBlockerCats);
		fprintf(pFile, "  },\n");

		// Generation analytics
		fprintf(pFile, "  \"generation\": {\n");
		fprintf(pFile, "    \"timeMs\": %.1f,\n", fGenerationTimeMs);
		fprintf(pFile, "    \"retryRounds\": %u,\n", uRetryRounds);

		if (pxStats)
		{
			// Attempt breakdown
			uint32_t uSolverPassed = pxStats->uTotalAttempts - pxStats->uScrambleFailures;
			float fScrambleRate = (pxStats->uTotalAttempts > 0) ? 100.0f * pxStats->uScrambleFailures / pxStats->uTotalAttempts : 0.0f;
			float fSuccessRate = (pxStats->uTotalAttempts > 0) ? 100.0f * pxStats->uTotalSuccesses / pxStats->uTotalAttempts : 0.0f;
			float fTooEasyRate = (uSolverPassed > 0) ? 100.0f * pxStats->uSolverTooEasy / uSolverPassed : 0.0f;
			float fUnsolvableRate = (uSolverPassed > 0) ? 100.0f * pxStats->uSolverUnsolvable / uSolverPassed : 0.0f;

			fprintf(pFile, "    \"totalAttempts\": %u,\n", pxStats->uTotalAttempts);
			fprintf(pFile, "    \"scrambleFailures\": %u,\n", pxStats->uScrambleFailures);
			fprintf(pFile, "    \"scrambleRate\": %.1f,\n", fScrambleRate);
			fprintf(pFile, "    \"solverTooEasy\": %u,\n", pxStats->uSolverTooEasy);
			fprintf(pFile, "    \"solverUnsolvable\": %u,\n", pxStats->uSolverUnsolvable);
			fprintf(pFile, "    \"totalSuccesses\": %u,\n", pxStats->uTotalSuccesses);
			fprintf(pFile, "    \"successRate\": %.1f,\n", fSuccessRate);
			fprintf(pFile, "    \"ofSolverPassed\": {\n");
			fprintf(pFile, "      \"total\": %u,\n", uSolverPassed);
			fprintf(pFile, "      \"tooEasyRate\": %.1f,\n", fTooEasyRate);
			fprintf(pFile, "      \"unsolvableRate\": %.1f\n", fUnsolvableRate);
			fprintf(pFile, "    },\n");

			// Winning attempt parameters
			fprintf(pFile, "    \"winningParams\": {\n");
			fprintf(pFile, "      \"colors\": %u,\n", pxStats->uWinningColors);
			fprintf(pFile, "      \"catsPerColor\": %u,\n", pxStats->uWinningCatsPerColor);
			fprintf(pFile, "      \"shapeComplexity\": %u,\n", pxStats->uWinningShapeComplexity);
			fprintf(pFile, "      \"blockers\": %u,\n", pxStats->uWinningBlockers);
			fprintf(pFile, "      \"blockerCats\": %u,\n", pxStats->uWinningBlockerCats);
			fprintf(pFile, "      \"conditionalShapes\": %u,\n", pxStats->uWinningConditionalShapes);
			fprintf(pFile, "      \"conditionalThreshold\": %u\n", pxStats->uWinningConditionalThreshold);
			fprintf(pFile, "    },\n");

			// Generation config (actual params used, or defaults if not provided)
			if (pxParams)
			{
				fprintf(pFile, "    \"config\": {\n");
				fprintf(pFile, "      \"maxAttempts\": %u,\n", pxParams->uMaxAttempts);
				fprintf(pFile, "      \"gridWidth\": [%u, %u],\n", pxParams->uMinGridWidth, pxParams->uMaxGridWidth);
				fprintf(pFile, "      \"gridHeight\": [%u, %u],\n", pxParams->uMinGridHeight, pxParams->uMaxGridHeight);
				fprintf(pFile, "      \"colors\": [%u, %u],\n", pxParams->uMinNumColors, pxParams->uNumColors);
				fprintf(pFile, "      \"catsPerColor\": [%u, %u],\n", pxParams->uMinCatsPerColor, pxParams->uNumCatsPerColor);
				fprintf(pFile, "      \"shapesPerColor\": %u,\n", pxParams->uNumShapesPerColor);
				fprintf(pFile, "      \"blockers\": [%u, %u],\n", pxParams->uMinBlockers, pxParams->uNumBlockers);
				fprintf(pFile, "      \"shapeComplexity\": [%u, %u],\n", pxParams->uMinMaxShapeSize, pxParams->uMaxShapeSize);
				fprintf(pFile, "      \"scrambleMoves\": [%u, %u],\n", pxParams->uMinScrambleMoves, pxParams->uScrambleMoves);
				fprintf(pFile, "      \"blockerCats\": [%u, %u],\n", pxParams->uMinBlockerCats, pxParams->uNumBlockerCats);
				fprintf(pFile, "      \"conditionalShapes\": [%u, %u],\n", pxParams->uMinConditionalShapes, pxParams->uNumConditionalShapes);
				fprintf(pFile, "      \"conditionalThreshold\": [%u, %u],\n", pxParams->uMinConditionalThreshold, pxParams->uConditionalThreshold);
				fprintf(pFile, "      \"minSolverMoves\": %u,\n", pxParams->uMinSolverMoves);
				fprintf(pFile, "      \"solverStateLimit\": %u,\n", pxParams->uSolverStateLimit);
				fprintf(pFile, "      \"deepSolverStateLimit\": %u,\n", pxParams->uDeepSolverStateLimit);
				fprintf(pFile, "      \"maxDeepVerificationsPerWorker\": %u\n", pxParams->uMaxDeepVerificationsPerWorker);
				fprintf(pFile, "    },\n");
			}
			else
			{
				fprintf(pFile, "    \"config\": {\n");
				fprintf(pFile, "      \"maxAttempts\": %d,\n", s_iTilePuzzleMaxGenerationAttempts);
				fprintf(pFile, "      \"gridWidth\": [%u, %u],\n", s_uGenMinGridWidth, s_uGenMaxGridWidth);
				fprintf(pFile, "      \"gridHeight\": [%u, %u],\n", s_uGenMinGridHeight, s_uGenMaxGridHeight);
				fprintf(pFile, "      \"colors\": [%u, %u],\n", s_uGenMinColors, s_uGenMaxColors);
				fprintf(pFile, "      \"catsPerColor\": [%u, %u],\n", s_uGenMinCatsPerColor, s_uGenMaxCatsPerColor);
				fprintf(pFile, "      \"shapesPerColor\": %u,\n", s_uGenNumShapesPerColor);
				fprintf(pFile, "      \"blockers\": [%u, %u],\n", s_uGenMinBlockers, s_uGenMaxBlockers);
				fprintf(pFile, "      \"shapeComplexity\": [%u, %u],\n", s_uGenMinShapeComplexity, s_uGenMaxShapeComplexity);
				fprintf(pFile, "      \"scrambleMoves\": %u,\n", s_uGenScrambleMoves);
				fprintf(pFile, "      \"blockerCats\": [%u, %u],\n", s_uGenMinBlockerCats, s_uGenMaxBlockerCats);
				fprintf(pFile, "      \"conditionalShapes\": [%u, %u],\n", s_uGenMinConditionalShapes, s_uGenMaxConditionalShapes);
				fprintf(pFile, "      \"conditionalThreshold\": [0, %u],\n", s_uGenMaxConditionalThreshold);
				fprintf(pFile, "      \"minSolverMoves\": %u,\n", s_uGenMinSolverMoves);
				fprintf(pFile, "      \"solverStateLimit\": %u,\n", s_uGenSolverStateLimit);
				fprintf(pFile, "      \"minScrambleMoves\": %u\n", s_uGenMinScrambleMoves);
				fprintf(pFile, "    },\n");
			}

			// Per-parameter breakdown
			fprintf(pFile, "    \"parameterStats\": {\n");
			WriteParamStats(pFile, "colors", pxStats->xColorStats, false);
			WriteParamStats(pFile, "catsPerColor", pxStats->xCatsPerColorStats, false);
			WriteParamStats(pFile, "shapeComplexity", pxStats->xShapeComplexityStats, true);
			fprintf(pFile, "    }\n");
		}
		else
		{
			fprintf(pFile, "    \"totalAttempts\": 0\n");
		}

		fprintf(pFile, "  }\n");
		fprintf(pFile, "}\n");

		fclose(pFile);
	}
}
