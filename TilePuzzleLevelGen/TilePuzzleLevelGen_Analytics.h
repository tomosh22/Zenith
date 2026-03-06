#pragma once
/**
 * TilePuzzleLevelGen_Analytics.h - Run-wide analytics tracking and output
 *
 * Accumulates per-level generation statistics across an entire run and
 * writes a summary analytics.txt file with parameter influence analysis.
 */

#include <cstdio>
#include <cstdint>

#include "Components/TilePuzzle_LevelGenerator.h"

namespace TilePuzzleLevelGenAnalytics
{
	static constexpr uint32_t s_uMaxParamValues = 8;

	struct RunStats
	{
		uint32_t uTotalLevelsAttempted = 0;
		uint32_t uTotalLevelsSucceeded = 0;
		uint32_t uTotalLevelsFailed = 0;
		double fTotalTimeMs = 0.0;
		uint32_t uTotalSolverMoves = 0;

		uint32_t uTotalRetryRounds = 0;
		uint32_t uMaxRetryRounds = 0;
		uint32_t uTimedOutLevels = 0;
		uint32_t uTotalLowerBoundAccepts = 0;

		TilePuzzle_LevelGenerator::ParamValueStats xColorStats;
		TilePuzzle_LevelGenerator::ParamValueStats xCatsPerColorStats;
		TilePuzzle_LevelGenerator::ParamValueStats xShapeComplexityStats;
	};

	static void AccumulateLevel(
		RunStats& xRunStats,
		bool bSuccess,
		uint32_t uSolverMoves,
		double fTimeMs,
		const TilePuzzle_LevelGenerator::GenerationStats* pxStats,
		uint32_t uRetryRounds = 1,
		bool bTimedOut = false)
	{
		xRunStats.uTotalLevelsAttempted++;
		xRunStats.fTotalTimeMs += fTimeMs;
		xRunStats.uTotalRetryRounds += uRetryRounds;
		if (uRetryRounds > xRunStats.uMaxRetryRounds)
			xRunStats.uMaxRetryRounds = uRetryRounds;
		if (bTimedOut)
			xRunStats.uTimedOutLevels++;

		if (bSuccess)
		{
			xRunStats.uTotalLevelsSucceeded++;
			xRunStats.uTotalSolverMoves += uSolverMoves;
		}
		else
		{
			xRunStats.uTotalLevelsFailed++;
		}

		if (pxStats)
		{
			xRunStats.uTotalLowerBoundAccepts += pxStats->uLowerBoundAccepts;

			for (uint32_t v = 0; v < s_uMaxParamValues; ++v)
			{
				xRunStats.xColorStats.auScrambleFailures[v] += pxStats->xColorStats.auScrambleFailures[v];
				xRunStats.xColorStats.auSolverTooEasy[v] += pxStats->xColorStats.auSolverTooEasy[v];
				xRunStats.xColorStats.auSolverUnsolvable[v] += pxStats->xColorStats.auSolverUnsolvable[v];
				xRunStats.xColorStats.auSuccesses[v] += pxStats->xColorStats.auSuccesses[v];

				xRunStats.xCatsPerColorStats.auScrambleFailures[v] += pxStats->xCatsPerColorStats.auScrambleFailures[v];
				xRunStats.xCatsPerColorStats.auSolverTooEasy[v] += pxStats->xCatsPerColorStats.auSolverTooEasy[v];
				xRunStats.xCatsPerColorStats.auSolverUnsolvable[v] += pxStats->xCatsPerColorStats.auSolverUnsolvable[v];
				xRunStats.xCatsPerColorStats.auSuccesses[v] += pxStats->xCatsPerColorStats.auSuccesses[v];

				xRunStats.xShapeComplexityStats.auScrambleFailures[v] += pxStats->xShapeComplexityStats.auScrambleFailures[v];
				xRunStats.xShapeComplexityStats.auSolverTooEasy[v] += pxStats->xShapeComplexityStats.auSolverTooEasy[v];
				xRunStats.xShapeComplexityStats.auSolverUnsolvable[v] += pxStats->xShapeComplexityStats.auSolverUnsolvable[v];
				xRunStats.xShapeComplexityStats.auSuccesses[v] += pxStats->xShapeComplexityStats.auSuccesses[v];
			}
		}
	}

	static void WriteParamTable(FILE* pFile, const char* szParamName, const TilePuzzle_LevelGenerator::ParamValueStats& xStats)
	{
		fprintf(pFile, "\n--- %s ---\n", szParamName);
		fprintf(pFile, "%-8s %-12s %-12s %-14s %-12s %-10s\n",
			"Value", "ScrambleFail", "TooEasy", "Unsolvable", "Success", "SuccRate%");
		fprintf(pFile, "----------------------------------------------------------------------\n");

		for (uint32_t v = 0; v < s_uMaxParamValues; ++v)
		{
			uint32_t uTotal = xStats.auScrambleFailures[v] + xStats.auSolverTooEasy[v] +
				xStats.auSolverUnsolvable[v] + xStats.auSuccesses[v];
			if (uTotal == 0)
				continue;

			float fSuccessRate = (uTotal > 0) ? (100.0f * xStats.auSuccesses[v] / uTotal) : 0.0f;
			fprintf(pFile, "%-8u %-12u %-12u %-14u %-12u %-10.1f\n",
				v,
				xStats.auScrambleFailures[v],
				xStats.auSolverTooEasy[v],
				xStats.auSolverUnsolvable[v],
				xStats.auSuccesses[v],
				fSuccessRate);
		}
	}

	static void WriteKeyFindings(FILE* pFile, const char* szParamName, const TilePuzzle_LevelGenerator::ParamValueStats& xStats)
	{
		for (uint32_t v = 0; v < s_uMaxParamValues; ++v)
		{
			uint32_t uTotal = xStats.auScrambleFailures[v] + xStats.auSolverTooEasy[v] +
				xStats.auSolverUnsolvable[v] + xStats.auSuccesses[v];
			if (uTotal == 0)
				continue;

			float fSuccessRate = 100.0f * xStats.auSuccesses[v] / uTotal;
			if (fSuccessRate >= 80.0f)
			{
				fprintf(pFile, "  [HIGH SUCCESS] %s=%u: %.1f%% success rate (%u/%u)\n",
					szParamName, v, fSuccessRate, xStats.auSuccesses[v], uTotal);
			}
			else if (fSuccessRate <= 20.0f)
			{
				fprintf(pFile, "  [LOW SUCCESS]  %s=%u: %.1f%% success rate (%u/%u)\n",
					szParamName, v, fSuccessRate, xStats.auSuccesses[v], uTotal);
			}

			// Flag dominant failure modes
			if (uTotal > 0)
			{
				float fScrambleRate = 100.0f * xStats.auScrambleFailures[v] / uTotal;
				float fUnsolvableRate = 100.0f * xStats.auSolverUnsolvable[v] / uTotal;
				if (fScrambleRate >= 50.0f)
				{
					fprintf(pFile, "  [SCRAMBLE BOTTLENECK] %s=%u: %.1f%% scramble failures\n",
						szParamName, v, fScrambleRate);
				}
				if (fUnsolvableRate >= 50.0f)
				{
					fprintf(pFile, "  [SOLVER BOTTLENECK] %s=%u: %.1f%% solver unsolvable\n",
						szParamName, v, fUnsolvableRate);
				}
			}
		}
	}

	static void WriteAnalytics(const char* szPath, const RunStats& xRunStats)
	{
		FILE* pFile = fopen(szPath, "w");
		if (!pFile)
			return;

		fprintf(pFile, "==========================================================\n");
		fprintf(pFile, "TilePuzzle Level Generator - Run Analytics\n");
		fprintf(pFile, "==========================================================\n\n");

		// Run summary
		fprintf(pFile, "SUMMARY\n");
		fprintf(pFile, "-------\n");
		fprintf(pFile, "Total levels attempted:  %u\n", xRunStats.uTotalLevelsAttempted);
		fprintf(pFile, "Total levels succeeded:  %u\n", xRunStats.uTotalLevelsSucceeded);
		fprintf(pFile, "Total levels failed:     %u\n", xRunStats.uTotalLevelsFailed);
		if (xRunStats.uTotalLevelsAttempted > 0)
		{
			fprintf(pFile, "Overall success rate:    %.1f%%\n",
				100.0f * xRunStats.uTotalLevelsSucceeded / xRunStats.uTotalLevelsAttempted);
		}
		fprintf(pFile, "Total time:              %.1f ms (%.1f s)\n",
			xRunStats.fTotalTimeMs, xRunStats.fTotalTimeMs / 1000.0);
		if (xRunStats.uTotalLevelsAttempted > 0)
		{
			fprintf(pFile, "Avg time per level:      %.1f ms\n",
				xRunStats.fTotalTimeMs / xRunStats.uTotalLevelsAttempted);
		}
		if (xRunStats.uTotalLevelsSucceeded > 0)
		{
			fprintf(pFile, "Avg solver moves:        %.1f\n",
				static_cast<float>(xRunStats.uTotalSolverMoves) / xRunStats.uTotalLevelsSucceeded);
		}

		// Retry stats
		fprintf(pFile, "\nRETRY STATISTICS\n");
		fprintf(pFile, "----------------\n");
		fprintf(pFile, "Total retry rounds:      %u\n", xRunStats.uTotalRetryRounds);
		if (xRunStats.uTotalLevelsAttempted > 0)
		{
			fprintf(pFile, "Avg rounds per level:    %.1f\n",
				static_cast<float>(xRunStats.uTotalRetryRounds) / xRunStats.uTotalLevelsAttempted);
		}
		fprintf(pFile, "Max rounds for a level:  %u\n", xRunStats.uMaxRetryRounds);
		fprintf(pFile, "Timed-out levels:        %u\n", xRunStats.uTimedOutLevels);
		fprintf(pFile, "Lower-bound accepts:     %u\n", xRunStats.uTotalLowerBoundAccepts);

		// Per-parameter tables
		fprintf(pFile, "\n\nPARAMETER INFLUENCE ANALYSIS\n");
		fprintf(pFile, "============================\n");
		WriteParamTable(pFile, "Colors", xRunStats.xColorStats);
		WriteParamTable(pFile, "CatsPerColor", xRunStats.xCatsPerColorStats);
		WriteParamTable(pFile, "ShapeComplexity", xRunStats.xShapeComplexityStats);

		// Key findings
		fprintf(pFile, "\n\nKEY FINDINGS\n");
		fprintf(pFile, "============\n");
		WriteKeyFindings(pFile, "Colors", xRunStats.xColorStats);
		WriteKeyFindings(pFile, "CatsPerColor", xRunStats.xCatsPerColorStats);
		WriteKeyFindings(pFile, "ShapeComplexity", xRunStats.xShapeComplexityStats);

		fprintf(pFile, "\n==========================================================\n");

		fclose(pFile);
	}
}
