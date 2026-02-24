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
 * Each generation attempt randomly selects parameter values from unified ranges.
 * All parameters are configurable static constexpr constants.
 *
 * Level generation is parallelized via Zenith_TaskArray. Each worker thread
 * gets its own RNG and result storage. The best result (highest solver move
 * count) is chosen as the final level.
 */

#include <random>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <thread>
#include "TilePuzzle_Types.h"
#include "TilePuzzle_Rules.h"
#include "TilePuzzle_Solver.h"
#include "Collections/Zenith_Vector.h"
#include "TaskSystem/Zenith_TaskSystem.h"

// Generation constants
static constexpr uint32_t s_uTilePuzzleMinGridSize = 5;
static constexpr uint32_t s_uTilePuzzleMaxGridSize = 12;
static constexpr int32_t s_iTilePuzzleMaxGenerationAttempts = 1000;

// Unified generation parameters (difficulty-focused tuning)
static constexpr uint32_t s_uGenMinGridWidth = 8;
static constexpr uint32_t s_uGenMaxGridWidth = 8;
static constexpr uint32_t s_uGenMinGridHeight = 8;
static constexpr uint32_t s_uGenMaxGridHeight = 8;
static constexpr uint32_t s_uGenMinColors = 3;
static constexpr uint32_t s_uGenMaxColors = 3;
static constexpr uint32_t s_uGenMinCatsPerColor = 3;
static constexpr uint32_t s_uGenMaxCatsPerColor = 3;
static constexpr uint32_t s_uGenNumShapesPerColor = 1;
static constexpr uint32_t s_uGenMinBlockers = 1;
static constexpr uint32_t s_uGenMaxBlockers = 2;
static constexpr uint32_t s_uGenMinShapeComplexity = 3;
static constexpr uint32_t s_uGenMaxShapeComplexity = 4;
static constexpr uint32_t s_uGenScrambleMoves = 600;      // More scramble = more displacement = harder
static constexpr uint32_t s_uGenMinBlockerCats = 0;
static constexpr uint32_t s_uGenMaxBlockerCats = 1;
static constexpr uint32_t s_uGenMinConditionalShapes = 0;
static constexpr uint32_t s_uGenMaxConditionalShapes = 1;
static constexpr uint32_t s_uGenMaxConditionalThreshold = 2;
static constexpr uint32_t s_uGenMinSolverMoves = 6;       // Raised floor - cut easy tail
static constexpr uint32_t s_uGenSolverStateLimit = 500000; // 500K states - fast unsolvable rejection
static constexpr uint32_t s_uGenMinScrambleMoves = 100;

// Per-parameter stats tracking
static constexpr uint32_t s_uParamStatsMaxValues = 8;

/**
 * TilePuzzle_LevelGenerator - Procedural level generation
 *
 * Generates solvable puzzle levels with increasing difficulty.
 */
class TilePuzzle_LevelGenerator
{
public:
	// Per-parameter-value outcome counters
	struct ParamValueStats
	{
		uint32_t auScrambleFailures[s_uParamStatsMaxValues] = {};
		uint32_t auSolverTooEasy[s_uParamStatsMaxValues] = {};
		uint32_t auSolverUnsolvable[s_uParamStatsMaxValues] = {};
		uint32_t auSuccesses[s_uParamStatsMaxValues] = {};
	};

	// Aggregated generation statistics (filled by GenerateLevel if pxStatsOut is provided)
	struct GenerationStats
	{
		uint32_t uTotalAttempts = 0;
		uint32_t uScrambleFailures = 0;
		uint32_t uSolverTooEasy = 0;
		uint32_t uSolverUnsolvable = 0;
		uint32_t uTotalSuccesses = 0;
		ParamValueStats xColorStats;
		ParamValueStats xCatsPerColorStats;
		ParamValueStats xShapeComplexityStats;
		uint32_t uWinningColors = 0;
		uint32_t uWinningCatsPerColor = 0;
		uint32_t uWinningShapeComplexity = 0;
		uint32_t uWinningBlockers = 0;
		uint32_t uWinningBlockerCats = 0;
		uint32_t uWinningConditionalShapes = 0;
		uint32_t uWinningConditionalThreshold = 0;
	};

	/**
	 * DifficultyParams - Parameters for level difficulty
	 */
	struct DifficultyParams
	{
		uint32_t uMinGridWidth = 5;
		uint32_t uMaxGridWidth = 6;
		uint32_t uMinGridHeight = 5;
		uint32_t uMaxGridHeight = 6;
		uint32_t uMinNumColors = 2;        // Min colors (random pick per attempt)
		uint32_t uNumColors = 2;           // Max colors (effective value set per attempt)
		uint32_t uMinCatsPerColor = 1;     // Min cats per color
		uint32_t uNumCatsPerColor = 1;     // Max cats per color (effective value set per attempt)
		uint32_t uNumShapesPerColor = 1;   // Draggable shapes per color
		uint32_t uMinBlockers = 0;         // Min static blockers
		uint32_t uNumBlockers = 0;         // Max static blockers (effective value set per attempt)
		uint32_t uMinMaxShapeSize = 1;     // Min shape complexity (1=single, 2=domino, 3=L/T/I, 4=all)
		uint32_t uMaxShapeSize = 1;        // Max shape complexity (effective value set per attempt)
		uint32_t uScrambleMoves = 15;      // Number of scramble moves for reverse generation
		uint32_t uMinBlockerCats = 0;      // Min cats on blockers
		uint32_t uNumBlockerCats = 0;      // Max cats on blockers (effective value set per attempt)
		uint32_t uMinConditionalShapes = 0;// Min conditional shapes
		uint32_t uNumConditionalShapes = 0;// Max conditional shapes (effective value set per attempt)
		uint32_t uMinConditionalThreshold = 1;// Min threshold (default 1 for backward compat)
		uint32_t uConditionalThreshold = 0;// Max eliminations to unlock conditional shapes (effective value set per attempt)
		uint32_t uMinSolverMoves = 0;     // Minimum solver-verified player moves (0 = no verification)
		uint32_t uSolverStateLimit = s_uGenSolverStateLimit; // BFS state limit for solver verification
		uint32_t uDeepSolverStateLimit = 0; // Deep solver limit for re-verifying "unsolvable" candidates (0 = disabled)
		uint32_t uMaxDeepVerificationsPerWorker = 0; // Max deep verifications per worker thread (0 = disabled)
		uint32_t uMinScrambleMoves = s_uGenMinScrambleMoves; // Min successful scramble moves to accept
		uint32_t uMaxAttempts = static_cast<uint32_t>(s_iTilePuzzleMaxGenerationAttempts); // Total generation attempts per round
	};

	/**
	 * GetDifficultyForLevel - Get difficulty parameters based on level number
	 */
	static DifficultyParams GetDifficultyForLevel(uint32_t /*uLevelNumber*/)
	{
		DifficultyParams xParams;
		xParams.uMinGridWidth = s_uGenMinGridWidth;
		xParams.uMaxGridWidth = s_uGenMaxGridWidth;
		xParams.uMinGridHeight = s_uGenMinGridHeight;
		xParams.uMaxGridHeight = s_uGenMaxGridHeight;
		xParams.uMinNumColors = s_uGenMinColors;
		xParams.uNumColors = s_uGenMaxColors;
		xParams.uMinCatsPerColor = s_uGenMinCatsPerColor;
		xParams.uNumCatsPerColor = s_uGenMaxCatsPerColor;
		xParams.uNumShapesPerColor = s_uGenNumShapesPerColor;
		xParams.uMinBlockers = s_uGenMinBlockers;
		xParams.uNumBlockers = s_uGenMaxBlockers;
		xParams.uMinMaxShapeSize = s_uGenMinShapeComplexity;
		xParams.uMaxShapeSize = s_uGenMaxShapeComplexity;
		xParams.uScrambleMoves = s_uGenScrambleMoves;
		xParams.uMinBlockerCats = s_uGenMinBlockerCats;
		xParams.uNumBlockerCats = s_uGenMaxBlockerCats;
		xParams.uMinConditionalShapes = s_uGenMinConditionalShapes;
		xParams.uNumConditionalShapes = s_uGenMaxConditionalShapes;
		xParams.uConditionalThreshold = s_uGenMaxConditionalThreshold;
		xParams.uMinSolverMoves = s_uGenMinSolverMoves;
		xParams.uSolverStateLimit = s_uGenSolverStateLimit;
		xParams.uMinScrambleMoves = s_uGenMinScrambleMoves;
		xParams.uMaxAttempts = static_cast<uint32_t>(s_iTilePuzzleMaxGenerationAttempts);

		// Clamp values
		xParams.uNumColors = std::min(xParams.uNumColors, static_cast<uint32_t>(TILEPUZZLE_COLOR_COUNT));
		xParams.uMinNumColors = std::min(xParams.uMinNumColors, xParams.uNumColors);
		xParams.uMaxShapeSize = std::min(xParams.uMaxShapeSize, 4u);
		xParams.uMinMaxShapeSize = std::min(xParams.uMinMaxShapeSize, xParams.uMaxShapeSize);

		return xParams;
	}

	/**
	 * GenerateLevel - Generate a solvable level using parallel reverse scramble
	 *
	 * Distributes generation attempts across worker threads via Zenith_TaskArray.
	 * Each worker gets its own RNG seed and result storage. After all workers
	 * finish, the result with the highest solver move count is selected.
	 *
	 * @param xLevelOut   Output level data
	 * @param xRng        Random number generator (unused, kept for API compat)
	 * @param uLevelNumber Level number for difficulty scaling
	 * @return true if generated level, false if all attempts failed
	 */
	static bool GenerateLevel(TilePuzzleLevelData& xLevelOut, std::mt19937& /*xRng*/, uint32_t uLevelNumber, GenerationStats* pxStatsOut = nullptr, uint32_t uSeedOffset = 0, const DifficultyParams* pxParamsOverride = nullptr)
	{
		DifficultyParams xParams = pxParamsOverride ? *pxParamsOverride : GetDifficultyForLevel(uLevelNumber);

		uint32_t uNumWorkers = std::max(1u, static_cast<uint32_t>(std::thread::hardware_concurrency()));

		// Allocate per-worker results
		std::vector<ParallelGenResult> axResults(uNumWorkers);

		ParallelGenData xData;
		xData.pxParams = &xParams;
		xData.pxResults = axResults.data();
		xData.uLevelNumber = uLevelNumber;
		xData.uTotalAttempts = xParams.uMaxAttempts;
		xData.uSeedOffset = uSeedOffset;

		Zenith_TaskArray xTaskArray(
			ZENITH_PROFILE_INDEX__SCENE_UPDATE,
			&ParallelGenerateTask,
			&xData,
			static_cast<u_int>(uNumWorkers),
			true);

		Zenith_TaskSystem::SubmitTaskArray(&xTaskArray);
		xTaskArray.WaitUntilComplete();

		// Find the best result across all workers
		int32_t iBestSolverResult = -1;
		int32_t iBestWorkerIdx = -1;

		uint32_t uTotalScrambleFailures = 0;
		uint32_t uTotalSolverTooEasy = 0;
		uint32_t uTotalSolverUnsolvable = 0;
		uint32_t uTotalSuccesses = 0;

		// Aggregate per-parameter stats
		ParamValueStats xTotalColorStats = {};
		ParamValueStats xTotalCatsStats = {};
		ParamValueStats xTotalShapeStats = {};

		for (uint32_t i = 0; i < uNumWorkers; ++i)
		{
			uTotalScrambleFailures += axResults[i].uScrambleFailures;
			uTotalSolverTooEasy += axResults[i].uSolverTooEasy;
			uTotalSolverUnsolvable += axResults[i].uSolverUnsolvable;
			uTotalSuccesses += axResults[i].uSuccessCount;

			for (uint32_t v = 0; v < s_uParamStatsMaxValues; ++v)
			{
				xTotalColorStats.auScrambleFailures[v] += axResults[i].xColorStats.auScrambleFailures[v];
				xTotalColorStats.auSolverTooEasy[v] += axResults[i].xColorStats.auSolverTooEasy[v];
				xTotalColorStats.auSolverUnsolvable[v] += axResults[i].xColorStats.auSolverUnsolvable[v];
				xTotalColorStats.auSuccesses[v] += axResults[i].xColorStats.auSuccesses[v];

				xTotalCatsStats.auScrambleFailures[v] += axResults[i].xCatsPerColorStats.auScrambleFailures[v];
				xTotalCatsStats.auSolverTooEasy[v] += axResults[i].xCatsPerColorStats.auSolverTooEasy[v];
				xTotalCatsStats.auSolverUnsolvable[v] += axResults[i].xCatsPerColorStats.auSolverUnsolvable[v];
				xTotalCatsStats.auSuccesses[v] += axResults[i].xCatsPerColorStats.auSuccesses[v];

				xTotalShapeStats.auScrambleFailures[v] += axResults[i].xShapeComplexityStats.auScrambleFailures[v];
				xTotalShapeStats.auSolverTooEasy[v] += axResults[i].xShapeComplexityStats.auSolverTooEasy[v];
				xTotalShapeStats.auSolverUnsolvable[v] += axResults[i].xShapeComplexityStats.auSolverUnsolvable[v];
				xTotalShapeStats.auSuccesses[v] += axResults[i].xShapeComplexityStats.auSuccesses[v];
			}

			if (axResults[i].bValid && axResults[i].iSolverResult > iBestSolverResult)
			{
				iBestSolverResult = axResults[i].iSolverResult;
				iBestWorkerIdx = static_cast<int32_t>(i);
			}
		}

		// Log per-parameter stats
		LogParamStats("Colors", uLevelNumber, xTotalColorStats);
		LogParamStats("CatsPerColor", uLevelNumber, xTotalCatsStats);
		LogParamStats("ShapeComplexity", uLevelNumber, xTotalShapeStats);

		// Fill optional stats output
		if (pxStatsOut)
		{
			pxStatsOut->uTotalAttempts = xParams.uMaxAttempts;
			pxStatsOut->uScrambleFailures = uTotalScrambleFailures;
			pxStatsOut->uSolverTooEasy = uTotalSolverTooEasy;
			pxStatsOut->uSolverUnsolvable = uTotalSolverUnsolvable;
			pxStatsOut->uTotalSuccesses = uTotalSuccesses;
			pxStatsOut->xColorStats = xTotalColorStats;
			pxStatsOut->xCatsPerColorStats = xTotalCatsStats;
			pxStatsOut->xShapeComplexityStats = xTotalShapeStats;
		}

		if (iBestWorkerIdx < 0)
		{
			Zenith_Log(LOG_CATEGORY_GAMEPLAY, "TilePuzzle: Failed to generate level %u after %u attempts across %u workers (scramble failures=%u, solver too easy=%u, solver unsolvable=%u)",
				uLevelNumber, xData.uTotalAttempts, uNumWorkers, uTotalScrambleFailures, uTotalSolverTooEasy, uTotalSolverUnsolvable);
			return false;
		}

		// Copy winning result's shape definitions to static storage and remap pointers
		ParallelGenResult& xWinner = axResults[iBestWorkerIdx];

		if (pxStatsOut)
		{
			pxStatsOut->uWinningColors = xWinner.uWinningColors;
			pxStatsOut->uWinningCatsPerColor = xWinner.uWinningCatsPerColor;
			pxStatsOut->uWinningShapeComplexity = xWinner.uWinningShapeComplexity;
			pxStatsOut->uWinningBlockers = xWinner.uWinningBlockers;
			pxStatsOut->uWinningBlockerCats = xWinner.uWinningBlockerCats;
			pxStatsOut->uWinningConditionalShapes = xWinner.uWinningConditionalShapes;
			pxStatsOut->uWinningConditionalThreshold = xWinner.uWinningConditionalThreshold;
		}
		Zenith_Vector<TilePuzzleShapeDefinition>& xStaticDefs = GetShapeDefinitions();
		xStaticDefs.Clear();
		xStaticDefs.Reserve(static_cast<uint32_t>(xWinner.axShapeDefinitions.size()));
		for (size_t i = 0; i < xWinner.axShapeDefinitions.size(); ++i)
		{
			xStaticDefs.PushBack(xWinner.axShapeDefinitions[i]);
		}

		// Remap shape instance pointers from worker-local definitions to static storage
		xLevelOut = std::move(xWinner.xLevel);
		size_t uDefIdx = 0;
		for (size_t i = 0; i < xLevelOut.axShapes.size(); ++i)
		{
			if (xLevelOut.axShapes[i].pxDefinition)
			{
				Zenith_Assert(uDefIdx < static_cast<size_t>(xStaticDefs.GetSize()), "Shape definition index out of range");
				xLevelOut.axShapes[i].pxDefinition = &xStaticDefs.Get(static_cast<uint32_t>(uDefIdx));
				uDefIdx++;
			}
		}

		xLevelOut.uMinimumMoves = static_cast<uint32_t>(iBestSolverResult);

		// Debug: verify solver result on final level
		int32_t iVerifyResult = TilePuzzle_Solver::SolveLevel(xLevelOut);
		Zenith_Log(LOG_CATEGORY_GAMEPLAY, "TilePuzzle: Level %u generated (solver=%d, verify=%d, scrambleFail=%u, tooEasy=%u, unsolvable=%u)",
			uLevelNumber, iBestSolverResult, iVerifyResult,
			uTotalScrambleFailures, uTotalSolverTooEasy, uTotalSolverUnsolvable);

		return true;
	}

private:
	// Per-worker generation result
	struct ParallelGenResult
	{
		TilePuzzleLevelData xLevel;
		std::vector<TilePuzzleShapeDefinition> axShapeDefinitions;
		int32_t iSolverResult = -1;
		bool bValid = false;
		uint32_t uScrambleFailures = 0;
		uint32_t uSolverTooEasy = 0;
		uint32_t uSolverUnsolvable = 0;
		uint32_t uSuccessCount = 0;
		ParamValueStats xColorStats;
		ParamValueStats xCatsPerColorStats;
		ParamValueStats xShapeComplexityStats;
		uint32_t uWinningColors = 0;
		uint32_t uWinningCatsPerColor = 0;
		uint32_t uWinningShapeComplexity = 0;
		uint32_t uWinningBlockers = 0;
		uint32_t uWinningBlockerCats = 0;
		uint32_t uWinningConditionalShapes = 0;
		uint32_t uWinningConditionalThreshold = 0;
	};

	static void LogParamStats(const char* szParamName, uint32_t uLevelNumber, const ParamValueStats& xStats)
	{
		char szBuffer[1024];
		int iPos = snprintf(szBuffer, sizeof(szBuffer), "TilePuzzle: Level %u %s:", uLevelNumber, szParamName);

		for (uint32_t v = 0; v < s_uParamStatsMaxValues && iPos < 960; ++v)
		{
			uint32_t uTotal = xStats.auScrambleFailures[v] + xStats.auSolverTooEasy[v] +
				xStats.auSolverUnsolvable[v] + xStats.auSuccesses[v];
			if (uTotal == 0)
				continue;

			iPos += snprintf(szBuffer + iPos, sizeof(szBuffer) - iPos,
				" %u=[scramble:%u easy:%u unsolvable:%u success:%u]",
				v, xStats.auScrambleFailures[v], xStats.auSolverTooEasy[v],
				xStats.auSolverUnsolvable[v], xStats.auSuccesses[v]);
		}

		Zenith_Log(LOG_CATEGORY_GAMEPLAY, "%s", szBuffer);
	}

	// Shared data passed to all workers
	struct ParallelGenData
	{
		const DifficultyParams* pxParams;
		ParallelGenResult* pxResults;
		uint32_t uLevelNumber;
		uint32_t uTotalAttempts;
		uint32_t uSeedOffset = 0;
	};

	// Task function called by each worker thread
	static void ParallelGenerateTask(void* pData, u_int uInvocationIndex, u_int uNumInvocations)
	{
		ParallelGenData* pxGenData = static_cast<ParallelGenData*>(pData);
		ParallelGenResult& xResult = pxGenData->pxResults[uInvocationIndex];
		const DifficultyParams& xParams = *pxGenData->pxParams;

		// Each worker gets a unique portion of attempts
		uint32_t uAttemptsPerWorker = pxGenData->uTotalAttempts / uNumInvocations;
		uint32_t uExtraAttempts = pxGenData->uTotalAttempts % uNumInvocations;
		if (uInvocationIndex < uExtraAttempts)
			uAttemptsPerWorker++;

		// Unique RNG seed per worker (seed offset varies across retry rounds)
		std::mt19937 xRng(pxGenData->uLevelNumber * 7919u + 104729u + uInvocationIndex * 31337u + pxGenData->uSeedOffset * 999983u);

		// Distributions for varied parameters
		std::uniform_int_distribution<uint32_t> xColorDist(xParams.uMinNumColors, xParams.uNumColors);
		std::uniform_int_distribution<uint32_t> xCatsDist(xParams.uMinCatsPerColor, xParams.uNumCatsPerColor);
		std::uniform_int_distribution<uint32_t> xShapeDist(xParams.uMinMaxShapeSize, xParams.uMaxShapeSize);
		std::uniform_int_distribution<uint32_t> xBlockerDist(xParams.uMinBlockers, xParams.uNumBlockers);
		std::uniform_int_distribution<uint32_t> xBlockerCatDist(xParams.uMinBlockerCats, xParams.uNumBlockerCats);
		std::uniform_int_distribution<uint32_t> xConditionalShapeDist(xParams.uMinConditionalShapes, xParams.uNumConditionalShapes);

		uint32_t uDeepVerifyCount = 0;

		for (uint32_t iAttempt = 0; iAttempt < uAttemptsPerWorker; ++iAttempt)
		{
			// Pick random values for varied parameters
			DifficultyParams xAttemptParams = xParams;
			xAttemptParams.uNumColors = xColorDist(xRng);
			xAttemptParams.uNumCatsPerColor = xCatsDist(xRng);
			xAttemptParams.uMaxShapeSize = xShapeDist(xRng);
			xAttemptParams.uNumBlockers = xBlockerDist(xRng);
			xAttemptParams.uNumBlockerCats = std::min(xBlockerCatDist(xRng), xAttemptParams.uNumBlockers);
			xAttemptParams.uNumConditionalShapes = xConditionalShapeDist(xRng);
			if (xAttemptParams.uNumConditionalShapes > 0 && xParams.uConditionalThreshold > 0)
			{
				uint32_t uMinThresh = std::max(1u, xParams.uMinConditionalThreshold);
				std::uniform_int_distribution<uint32_t> xThreshDist(uMinThresh, xParams.uConditionalThreshold);
				xAttemptParams.uConditionalThreshold = xThreshDist(xRng);
			}
			else
			{
				xAttemptParams.uConditionalThreshold = 0;
			}

			uint32_t uColors = xAttemptParams.uNumColors;
			uint32_t uCats = xAttemptParams.uNumCatsPerColor;
			uint32_t uShape = xAttemptParams.uMaxShapeSize;

			TilePuzzleLevelData xCandidateLevel;
			std::vector<TilePuzzleShapeDefinition> axCandidateDefs;

			uint32_t uSuccessfulMoves = 0;
			if (GenerateLevelAttempt(xCandidateLevel, xRng, xAttemptParams, uSuccessfulMoves, axCandidateDefs))
			{
				// Skip solver if scramble barely moved shapes - level is trivially easy
				if (uSuccessfulMoves < xParams.uMinScrambleMoves)
				{
					xResult.uSolverTooEasy++;
					xResult.xColorStats.auSolverTooEasy[uColors]++;
					xResult.xCatsPerColorStats.auSolverTooEasy[uCats]++;
					xResult.xShapeComplexityStats.auSolverTooEasy[uShape]++;
					continue;
				}

				int32_t iSolverResult = TilePuzzle_Solver::SolveLevel(xCandidateLevel, xParams.uSolverStateLimit);
				if (iSolverResult == -1 && xParams.uDeepSolverStateLimit > 0
					&& xParams.uMaxDeepVerificationsPerWorker > 0
					&& uDeepVerifyCount < xParams.uMaxDeepVerificationsPerWorker)
				{
					// Two-tier solver: re-verify "unsolvable" candidates at higher limit.
					// Limited per worker to control cost.
					// The unsolvable candidates have large BFS state spaces that may contain
					// deep 15+ move solutions the fast solver couldn't verify.
					uDeepVerifyCount++;
					iSolverResult = TilePuzzle_Solver::SolveLevel(xCandidateLevel, xParams.uDeepSolverStateLimit);
				}
				if (iSolverResult == -1)
				{
					// Solver hit state limit even at deep limit - reject and retry
					xResult.uSolverUnsolvable++;
					xResult.xColorStats.auSolverUnsolvable[uColors]++;
					xResult.xCatsPerColorStats.auSolverUnsolvable[uCats]++;
					xResult.xShapeComplexityStats.auSolverUnsolvable[uShape]++;
					continue;
				}
				if (xAttemptParams.uMinSolverMoves > 0 &&
					static_cast<uint32_t>(iSolverResult) < xAttemptParams.uMinSolverMoves)
				{
					xResult.uSolverTooEasy++;
					xResult.xColorStats.auSolverTooEasy[uColors]++;
					xResult.xCatsPerColorStats.auSolverTooEasy[uCats]++;
					xResult.xShapeComplexityStats.auSolverTooEasy[uShape]++;
					continue;
				}

				// Found a valid result - record success stats
				xResult.uSuccessCount++;
				xResult.xColorStats.auSuccesses[uColors]++;
				xResult.xCatsPerColorStats.auSuccesses[uCats]++;
				xResult.xShapeComplexityStats.auSuccesses[uShape]++;

				// Keep the result with the highest solver move count
				if (iSolverResult > xResult.iSolverResult)
				{
					xResult.xLevel = std::move(xCandidateLevel);
					xResult.axShapeDefinitions = std::move(axCandidateDefs);
					xResult.iSolverResult = iSolverResult;
					xResult.bValid = true;
					xResult.uWinningColors = uColors;
					xResult.uWinningCatsPerColor = uCats;
					xResult.uWinningShapeComplexity = uShape;
					xResult.uWinningBlockers = xAttemptParams.uNumBlockers;
					xResult.uWinningBlockerCats = xAttemptParams.uNumBlockerCats;
					xResult.uWinningConditionalShapes = xAttemptParams.uNumConditionalShapes;
					xResult.uWinningConditionalThreshold = xAttemptParams.uConditionalThreshold;

					// Remap pointers after move - definitions are now in xResult.axShapeDefinitions
					size_t uDefIdx = 0;
					for (size_t i = 0; i < xResult.xLevel.axShapes.size(); ++i)
					{
						if (xResult.xLevel.axShapes[i].pxDefinition)
						{
							xResult.xLevel.axShapes[i].pxDefinition = &xResult.axShapeDefinitions[uDefIdx];
							uDefIdx++;
						}
					}
				}
			}
			else
			{
				xResult.uScrambleFailures++;
				xResult.xColorStats.auScrambleFailures[uColors]++;
				xResult.xCatsPerColorStats.auScrambleFailures[uCats]++;
				xResult.xShapeComplexityStats.auScrambleFailures[uShape]++;
			}
		}
	}

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
	 *
	 * @param axShapeDefsOut  Per-attempt shape definitions storage (thread-safe)
	 */
	static bool GenerateLevelAttempt(
		TilePuzzleLevelData& xLevelOut,
		std::mt19937& xRng,
		const DifficultyParams& xParams,
		uint32_t& uSuccessfulMovesOut,
		std::vector<TilePuzzleShapeDefinition>& axShapeDefsOut)
	{
		axShapeDefsOut.clear();
		axShapeDefsOut.reserve(
			xParams.uNumBlockers +
			xParams.uNumColors * xParams.uNumShapesPerColor);

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
		// Multi-cell blockers create walls/corridors that reduce per-drag reachable positions,
		// lowering the BFS branching factor and enabling deeper optimal solutions.
		static constexpr TilePuzzleShapeType aeBlockerTypes[] = {
			TILEPUZZLE_SHAPE_I,      // 3 cells, linear wall
			TILEPUZZLE_SHAPE_DOMINO, // 2 cells, small wall
			TILEPUZZLE_SHAPE_SINGLE, // 1 cell, point obstacle
		};
		static constexpr uint32_t uNumBlockerTypes = sizeof(aeBlockerTypes) / sizeof(aeBlockerTypes[0]);
		std::uniform_int_distribution<uint32_t> xBlockerTypeDist(0, uNumBlockerTypes - 1);

		std::vector<size_t> axBlockerShapeIndices;
		for (uint32_t i = 0; i < xParams.uNumBlockers; ++i)
		{
			bool bPlaced = false;

			// Try several random blocker types, falling back to smaller shapes
			for (uint32_t uTypeAttempt = 0; uTypeAttempt < uNumBlockerTypes * 2 && !bPlaced; ++uTypeAttempt)
			{
				TilePuzzleShapeType eType = aeBlockerTypes[xBlockerTypeDist(xRng)];
				TilePuzzleShapeDefinition xCandidateDef = TilePuzzleShapes::GetShape(eType, false);

				for (size_t p = 0; p < axFloorPositions.size() && !bPlaced; ++p)
				{
					auto [x, y] = axFloorPositions[p];

					// Check all cells of the blocker shape fit on floor and are unoccupied
					bool bFits = true;
					for (size_t c = 0; c < xCandidateDef.axCells.size(); ++c)
					{
						int32_t iCellX = x + xCandidateDef.axCells[c].iX;
						int32_t iCellY = y + xCandidateDef.axCells[c].iY;

						if (iCellX < 1 || iCellY < 1 ||
							static_cast<uint32_t>(iCellX) >= xLevelOut.uGridWidth - 1 ||
							static_cast<uint32_t>(iCellY) >= xLevelOut.uGridHeight - 1)
						{
							bFits = false;
							break;
						}

						uint32_t uIdx = iCellY * xLevelOut.uGridWidth + iCellX;
						if (abOccupied[uIdx])
						{
							bFits = false;
							break;
						}
					}

					if (!bFits)
						continue;

					// Place the blocker
					axShapeDefsOut.push_back(xCandidateDef);
					TilePuzzleShapeDefinition& xBlockerDef = axShapeDefsOut.back();

					TilePuzzleShapeInstance xBlocker;
					xBlocker.pxDefinition = &xBlockerDef;
					xBlocker.iOriginX = x;
					xBlocker.iOriginY = y;
					xBlocker.eColor = TILEPUZZLE_COLOR_NONE;
					xLevelOut.axShapes.push_back(xBlocker);

					axBlockerShapeIndices.push_back(xLevelOut.axShapes.size() - 1);

					// Mark all cells as occupied
					for (size_t c = 0; c < xBlockerDef.axCells.size(); ++c)
					{
						uint32_t uIdx = (y + xBlockerDef.axCells[c].iY) * xLevelOut.uGridWidth + (x + xBlockerDef.axCells[c].iX);
						abOccupied[uIdx] = true;
					}
					bPlaced = true;
				}
			}

			// Fallback: place a single-cell blocker if multi-cell failed
			if (!bPlaced)
			{
				for (size_t p = 0; p < axFloorPositions.size(); ++p)
				{
					auto [x, y] = axFloorPositions[p];
					uint32_t uIdx = y * xLevelOut.uGridWidth + x;
					if (abOccupied[uIdx])
						continue;

					axShapeDefsOut.push_back(TilePuzzleShapes::GetSingleShape(false));
					TilePuzzleShapeDefinition& xBlockerDef = axShapeDefsOut.back();

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

				// Build list of shape types to try, from desired complexity down to Single
				TilePuzzleShapeType aeTypesToTry[8];
				uint32_t uNumTypesToTry = 0;

				if (xParams.uMaxShapeSize >= 4)
				{
					std::uniform_int_distribution<int> xShapeDist(
						static_cast<int>(TILEPUZZLE_SHAPE_L),
						static_cast<int>(TILEPUZZLE_SHAPE_O));
					aeTypesToTry[uNumTypesToTry++] = static_cast<TilePuzzleShapeType>(xShapeDist(xRng));
				}
				if (xParams.uMaxShapeSize >= 3)
				{
					std::uniform_int_distribution<int> xShapeDist(
						static_cast<int>(TILEPUZZLE_SHAPE_L),
						static_cast<int>(TILEPUZZLE_SHAPE_I));
					aeTypesToTry[uNumTypesToTry++] = static_cast<TilePuzzleShapeType>(xShapeDist(xRng));
				}
				if (xParams.uMaxShapeSize >= 2)
					aeTypesToTry[uNumTypesToTry++] = TILEPUZZLE_SHAPE_DOMINO;
				aeTypesToTry[uNumTypesToTry++] = TILEPUZZLE_SHAPE_SINGLE;

				// Try each shape type with all 4 rotations before falling back
				bool bPlaced = false;
				for (uint32_t uTypeIdx = 0; uTypeIdx < uNumTypesToTry && !bPlaced; ++uTypeIdx)
				{
					TilePuzzleShapeDefinition xBaseDef = TilePuzzleShapes::GetShape(aeTypesToTry[uTypeIdx], true);

					// Try 4 rotations (rotation is meaningless for Single/O but harmless)
					TilePuzzleShapeDefinition xRotDef = xBaseDef;
					for (uint32_t uRot = 0; uRot < 4 && !bPlaced; ++uRot)
					{
						if (uRot > 0)
							xRotDef = TilePuzzleShapes::RotateShape90(xRotDef);

						// Try each cell of the shape as the anchor on the target cat
						for (size_t c = 0; c < xRotDef.axCells.size(); ++c)
						{
							int32_t iOriginX = xTargetCat.iGridX - xRotDef.axCells[c].iX;
							int32_t iOriginY = xTargetCat.iGridY - xRotDef.axCells[c].iY;

							bool bFits = true;
							for (size_t k = 0; k < xRotDef.axCells.size(); ++k)
							{
								int32_t iCellX = iOriginX + xRotDef.axCells[k].iX;
								int32_t iCellY = iOriginY + xRotDef.axCells[k].iY;

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
								axShapeDefsOut.push_back(xRotDef);
								TilePuzzleShapeDefinition& xPlacedDef = axShapeDefsOut.back();

								TilePuzzleShapeInstance xShape;
								xShape.pxDefinition = &xPlacedDef;
								xShape.iOriginX = iOriginX;
								xShape.iOriginY = iOriginY;
								xShape.eColor = eColor;
								xLevelOut.axShapes.push_back(xShape);

								for (size_t k = 0; k < xPlacedDef.axCells.size(); ++k)
								{
									int32_t iCellX = iOriginX + xPlacedDef.axCells[k].iX;
									int32_t iCellY = iOriginY + xPlacedDef.axCells[k].iY;
									abOccupied[iCellY * xLevelOut.uGridWidth + iCellX] = true;
								}

								bPlaced = true;
								break;
							}
						}
					}
				}
			}
		}

		// Mark conditional shapes with incrementing thresholds for sequential solving.
		// With threshold step = catsPerColor, shapes unlock one at a time as each
		// color group is eliminated: shape 1 needs catsPerColor eliminated,
		// shape 2 needs 2*catsPerColor, etc. This forces solve order A→B→C.
		if (xParams.uNumConditionalShapes > 0 && xParams.uConditionalThreshold > 0)
		{
			uint32_t uConditionalCount = 0;
			for (size_t i = uFirstNormalDraggableShape;
				i < xLevelOut.axShapes.size() && uConditionalCount < xParams.uNumConditionalShapes;
				++i)
			{
				if (xLevelOut.axShapes[i].pxDefinition && xLevelOut.axShapes[i].pxDefinition->bDraggable)
				{
					uint32_t uThreshold = xParams.uConditionalThreshold * (uConditionalCount + 1);
					xLevelOut.axShapes[i].uUnlockThreshold = uThreshold;
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
				xState.uUnlockThreshold = 0; // Scramble ignores unlock thresholds; they only apply during gameplay
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

		uint32_t uCoveredMask = ComputeCoveredMask(
			axShapeStates.GetDataPointer(), axShapeStates.GetSize(),
			axCatStates.GetDataPointer(), axCatStates.GetSize());

		int32_t aiScrambleDeltaX[] = {0, 0, -1, 1};
		int32_t aiScrambleDeltaY[] = {-1, 1, 0, 0};
		std::uniform_int_distribution<int32_t> xDirDist(0, 3);

		// Pre-scramble: move conditional shapes off their cats
		for (uint32_t i = 0; i < axShapeStates.GetSize(); ++i)
		{
			if (xLevelOut.axShapes[axDraggableIndices[i]].uUnlockThreshold == 0)
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
					0,
					xLevelOut))
				{
					uCoveredMask = ComputeCoveredMask(
						axShapeStates.GetDataPointer(), axShapeStates.GetSize(),
						axCatStates.GetDataPointer(), axCatStates.GetSize());
					break;
				}
			}
		}

		// Main scramble - two modes:
		// 1. Uncover: move shapes OFF cats they're currently covering
		// 2. Random: random moves for distance
		uint32_t uNumCats = static_cast<uint32_t>(xLevelOut.axCats.size());
		size_t uNumDraggable = axDraggableIndices.size();
		uint32_t uSuccessfulMoves = 0;
		uint32_t uMaxIterations = xParams.uScrambleMoves * 10;
		std::uniform_int_distribution<size_t> xShapeDist(0, axDraggableIndices.size() - 1);

		for (uint32_t uIter = 0; uIter < uMaxIterations; ++uIter)
		{
			bool bMoved = false;

			// Mode 1 - Uncover: when cats are currently covered, move the covering shape away
			if (uCoveredMask != 0)
			{
				// Build shuffled shape order
				size_t auShapeOrder[s_uMaxSolverShapes];
				for (size_t i = 0; i < uNumDraggable; ++i)
					auShapeOrder[i] = i;
				for (size_t i = uNumDraggable; i > 1; --i)
				{
					std::uniform_int_distribution<size_t> xSwap(0, i - 1);
					size_t j = xSwap(xRng);
					size_t tmp = auShapeOrder[i - 1];
					auShapeOrder[i - 1] = auShapeOrder[j];
					auShapeOrder[j] = tmp;
				}

				for (size_t si = 0; si < uNumDraggable && !bMoved; ++si)
				{
					size_t uShapeIdx = auShapeOrder[si];
					const TilePuzzle_Rules::ShapeState& xShape = axShapeStates.Get(static_cast<uint32_t>(uShapeIdx));

					// Check if this shape covers any same-color cat
					bool bCoversCat = false;
					for (uint32_t uCovCheckIdx = 0; uCovCheckIdx < uNumCats; ++uCovCheckIdx)
					{
						if (!(uCoveredMask & (1u << uCovCheckIdx)))
							continue;
						if (axCatStates.Get(uCovCheckIdx).eColor != xShape.eColor)
							continue;
						for (size_t k = 0; k < xShape.pxDefinition->axCells.size(); ++k)
						{
							int32_t iCX = xShape.iOriginX + xShape.pxDefinition->axCells[k].iX;
							int32_t iCY = xShape.iOriginY + xShape.pxDefinition->axCells[k].iY;
							if (iCX == axCatStates.Get(uCovCheckIdx).iGridX && iCY == axCatStates.Get(uCovCheckIdx).iGridY)
							{
								bCoversCat = true;
								break;
							}
						}
						if (bCoversCat) break;
					}

					if (!bCoversCat)
						continue;

					// Try all 4 directions in random order
					int32_t aiDirOrder[4] = {0, 1, 2, 3};
					for (int32_t i = 3; i > 0; --i)
					{
						std::uniform_int_distribution<int32_t> xSwapDir(0, i);
						int32_t j = xSwapDir(xRng);
						int32_t tmp = aiDirOrder[i];
						aiDirOrder[i] = aiDirOrder[j];
						aiDirOrder[j] = tmp;
					}

					for (int32_t d = 0; d < 4; ++d)
					{
						if (TryScrambleMove(
							xLevelOut,
							axShapeStates.GetDataPointer(), axShapeStates.GetSize(),
							axDraggableIndices,
							axCatStates.GetDataPointer(), axCatStates.GetSize(),
							uShapeIdx,
							aiScrambleDeltaX[aiDirOrder[d]], aiScrambleDeltaY[aiDirOrder[d]],
							0,
							xLevelOut))
						{
							bMoved = true;
							break;
						}
					}
				}
			}
			// Mode 2 - Random one-cell move (any shape, any direction)
			if (!bMoved)
			{
				size_t uShapeIdx = xShapeDist(xRng);
				int32_t iDir = xDirDist(xRng);
				bMoved = TryScrambleMove(
					xLevelOut,
					axShapeStates.GetDataPointer(), axShapeStates.GetSize(),
					axDraggableIndices,
					axCatStates.GetDataPointer(), axCatStates.GetSize(),
					uShapeIdx,
					aiScrambleDeltaX[iDir], aiScrambleDeltaY[iDir],
					0,
					xLevelOut);
			}

			if (bMoved)
			{
				uCoveredMask = ComputeCoveredMask(
					axShapeStates.GetDataPointer(), axShapeStates.GetSize(),
					axCatStates.GetDataPointer(), axCatStates.GetSize());
				uSuccessfulMoves++;

				if (uCoveredMask == 0 &&
					uSuccessfulMoves >= xParams.uScrambleMoves)
				{
					break;
				}
			}
		}

		// No cats currently covered by same-color shapes
		if (uCoveredMask != 0)
			return false;

		uSuccessfulMovesOut = uSuccessfulMoves;
		return true;
	}

};
