#include "Zenith.h"
#pragma warning(disable: 4005) // APIENTRY macro redefinition (GLFW vs Windows SDK)
#include "Core/Memory/Zenith_MemoryManagement_Disabled.h"

#include "Components/TilePuzzle_LevelGenerator.h"
#include "TilePuzzleLevelData_Serialize.h"
#include "TilePuzzleLevelData_Json.h"
#include "TilePuzzleLevelData_Image.h"
#include "TilePuzzleLevelGen_Analytics.h"

#include <filesystem>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <random>
#include <csignal>

// Stub functions for standalone tool (same pattern as FluxCompiler)
const char* Project_GetGameAssetsDirectory() { return ""; }
const char* Project_GetName() { return "TilePuzzleLevelGen"; }

#ifdef ZENITH_TOOLS
void Zenith_EditorAddLogMessage(const char*, int, Zenith_LogCategory) {}
#endif

static volatile bool s_bRunning = true;

static void SignalHandler(int)
{
	s_bRunning = false;
}

static uint32_t FindNextRunNumber(const std::string& strOutputDir)
{
	uint32_t uMaxRun = 0;
	bool bFoundAny = false;

	if (!std::filesystem::exists(strOutputDir))
		return 0;

	for (auto& xEntry : std::filesystem::directory_iterator(strOutputDir))
	{
		if (!xEntry.is_directory())
			continue;

		std::string strName = xEntry.path().filename().string();
		if (strName.substr(0, 3) == "Run")
		{
			uint32_t uNum = 0;
			if (sscanf(strName.c_str() + 3, "%u", &uNum) == 1)
			{
				if (!bFoundAny || uNum >= uMaxRun)
				{
					uMaxRun = uNum;
					bFoundAny = true;
				}
			}
		}
	}

	return bFoundAny ? uMaxRun + 1 : 0;
}

static void AccumulateGenerationStats(
	TilePuzzle_LevelGenerator::GenerationStats& xAccum,
	const TilePuzzle_LevelGenerator::GenerationStats& xNew)
{
	xAccum.uTotalAttempts += xNew.uTotalAttempts;
	xAccum.uScrambleFailures += xNew.uScrambleFailures;
	xAccum.uSolverTooEasy += xNew.uSolverTooEasy;
	xAccum.uSolverUnsolvable += xNew.uSolverUnsolvable;
	xAccum.uTotalSuccesses += xNew.uTotalSuccesses;
	for (uint32_t v = 0; v < s_uParamStatsMaxValues; ++v)
	{
		xAccum.xColorStats.auScrambleFailures[v] += xNew.xColorStats.auScrambleFailures[v];
		xAccum.xColorStats.auSolverTooEasy[v] += xNew.xColorStats.auSolverTooEasy[v];
		xAccum.xColorStats.auSolverUnsolvable[v] += xNew.xColorStats.auSolverUnsolvable[v];
		xAccum.xColorStats.auSuccesses[v] += xNew.xColorStats.auSuccesses[v];

		xAccum.xCatsPerColorStats.auScrambleFailures[v] += xNew.xCatsPerColorStats.auScrambleFailures[v];
		xAccum.xCatsPerColorStats.auSolverTooEasy[v] += xNew.xCatsPerColorStats.auSolverTooEasy[v];
		xAccum.xCatsPerColorStats.auSolverUnsolvable[v] += xNew.xCatsPerColorStats.auSolverUnsolvable[v];
		xAccum.xCatsPerColorStats.auSuccesses[v] += xNew.xCatsPerColorStats.auSuccesses[v];

		xAccum.xShapeComplexityStats.auScrambleFailures[v] += xNew.xShapeComplexityStats.auScrambleFailures[v];
		xAccum.xShapeComplexityStats.auSolverTooEasy[v] += xNew.xShapeComplexityStats.auSolverTooEasy[v];
		xAccum.xShapeComplexityStats.auSolverUnsolvable[v] += xNew.xShapeComplexityStats.auSolverUnsolvable[v];
		xAccum.xShapeComplexityStats.auSuccesses[v] += xNew.xShapeComplexityStats.auSuccesses[v];
	}
}

int main(int argc, char* argv[])
{
	printf("TilePuzzle Level Generator\n");
	printf("==========================\n\n");
	fflush(stdout);

	// Parse CLI arguments
	std::string strOutputDir = LEVELGEN_OUTPUT_DIR;
	uint32_t uMaxLevels = 0; // 0 = infinite
	uint32_t uTimeoutSeconds = 1800; // 30 minutes default
	uint32_t uMinMoves = 20;
	uint32_t uSolverLimit = 500000;
	uint32_t uStartSeed = 0;

	for (int i = 1; i < argc; ++i)
	{
		if (strcmp(argv[i], "--output") == 0 && i + 1 < argc)
		{
			strOutputDir = argv[++i];
		}
		else if (strcmp(argv[i], "--count") == 0 && i + 1 < argc)
		{
			uMaxLevels = static_cast<uint32_t>(atoi(argv[++i]));
		}
		else if (strcmp(argv[i], "--timeout") == 0 && i + 1 < argc)
		{
			uTimeoutSeconds = static_cast<uint32_t>(atoi(argv[++i]));
		}
		else if (strcmp(argv[i], "--min-moves") == 0 && i + 1 < argc)
		{
			uMinMoves = static_cast<uint32_t>(atoi(argv[++i]));
		}
		else if (strcmp(argv[i], "--solver-limit") == 0 && i + 1 < argc)
		{
			uSolverLimit = static_cast<uint32_t>(atoi(argv[++i]));
		}
		else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc)
		{
			uStartSeed = static_cast<uint32_t>(atoi(argv[++i]));
		}
	}

	// Minimal engine init (same sequence as Zenith_Core::Zenith_Init, first 4 calls)
	Zenith_MemoryManagement::Initialise();
	Zenith_Profiling::Initialise();
	Zenith_Multithreading::RegisterThread(true);
	Zenith_TaskSystem::Inititalise();

	// Set up Ctrl+C handler
	signal(SIGINT, SignalHandler);

	// Create run directory
	std::filesystem::create_directories(strOutputDir);
	uint32_t uRunNumber = FindNextRunNumber(strOutputDir);
	std::string strRunDir = strOutputDir + "/Run" + std::to_string(uRunNumber);
	std::filesystem::create_directories(strRunDir);

	printf("Output directory: %s\n", strRunDir.c_str());
	printf("Min moves: %u, Solver limit: %u, Timeout: %us, Seed: %u\n", uMinMoves, uSolverLimit, uTimeoutSeconds, uStartSeed);
	if (uMaxLevels > 0)
		printf("Generating %u levels\n\n", uMaxLevels);
	else
		printf("Running continuously (Ctrl+C to stop)\n\n");
	fflush(stdout);

	// Build tool-specific difficulty params
	// NOTE: uMinSolverMoves is the INTERNAL per-attempt filter (keep low so candidates survive).
	// The OUTER retry loop enforces the actual target (uMinMoves) by retrying until best >= target.
	//
	// Strategy for high-move solutions with flood-fill solver:
	// - 5 colors × 1 shape × 2 cats = 10 cats, 5 draggable shapes
	// - 2^10=1024 elimination states (vs 2^15=32K for 3 cats — unsolvable by BFS)
	// - 9×9 grid: 49 playable cells for 10 cats + 5 shapes + blocker
	// - 1-2 conditional shapes (threshold 1-4): phase gates force sequential solving
	// - 1 blocker (max 3 cells, post-validated)
	// - 500K fast solver + 5M deep verify (5 per worker)
	// - 3000 attempts/round
	TilePuzzle_LevelGenerator::DifficultyParams xToolParams = TilePuzzle_LevelGenerator::GetDifficultyForLevel(1);
	xToolParams.uMinSolverMoves = 6;               // Internal filter: accept anything >= 6
	xToolParams.uSolverStateLimit = uSolverLimit;   // BFS state limit
	// Deep verify: re-solve "unsolvable" candidates with 5M limit to recover deep solutions.
	// 81% of solver-passed candidates are "unsolvable" at 500K — many contain 18+ move solutions.
	// 5 verifications per worker (up from 3) catches more deep solutions while keeping rounds fast.
	// Fast rounds (5-8 min) allow more retry rounds, which matters more than deep-but-slow rounds.
	xToolParams.uDeepSolverStateLimit = 5000000;
	xToolParams.uMaxDeepVerificationsPerWorker = 5;

	// 9×9 grid: 49 playable cells for 10 cats + 5 shapes + blocker
	xToolParams.uMinGridWidth = 9;
	xToolParams.uMaxGridWidth = 9;
	xToolParams.uMinGridHeight = 9;
	xToolParams.uMaxGridHeight = 9;

	// Fixed 5 colors (1 draggable shape per color) × 2 cats = 10 cats, 5 shapes
	// NOTE: 3 cats/color (15 cats) creates 2^15=32K elimination states — unsolvable by BFS.
	// 2 cats/color (10 cats) = 2^10=1024 elimination states — tractable.
	xToolParams.uMinNumColors = 5;
	xToolParams.uNumColors = 5;
	xToolParams.uMinCatsPerColor = 2;
	xToolParams.uNumCatsPerColor = 2;

	// Fixed 1 blocker: max 3 blocker cells constraint means 2 blockers never works
	// (2 Dominoes = 4 cells > 3). Single blocker = 2-3 cells (Domino/I-shape).
	xToolParams.uMinBlockers = 1;
	xToolParams.uNumBlockers = 1;

	// Conditional shapes: 1-2 with threshold randomized 1-4 per attempt
	// More phase gates create deeper sequential solving (more moves)
	xToolParams.uMinConditionalShapes = 1;
	xToolParams.uNumConditionalShapes = 2;
	xToolParams.uConditionalThreshold = 4;

	// Shape complexity: 2-4 (Domino to L/T/S/Z)
	// At least one draggable shape must be complexity >= 2, so floor is 2
	xToolParams.uMinMaxShapeSize = 2;
	xToolParams.uMaxShapeSize = 4;

	// Lower scramble threshold for multi-cell shapes
	xToolParams.uMinScrambleMoves = 30;
	xToolParams.uScrambleMoves = 1000;

	// 3000 attempts per round: generates ~930 solver candidates (31% pass scramble),
	// of which ~750 are "unsolvable" — larger candidate pool for deep verification
	xToolParams.uMaxAttempts = 3000;

	// Run-wide analytics
	TilePuzzleLevelGenAnalytics::RunStats xRunStats = {};

	std::mt19937 xRng(42); // Unused by GenerateLevel but required by API

	// Global seed counter: each round uses a unique base seed regardless of which
	// level we're generating. This avoids getting stuck in bad seed regions —
	// previously level 2's seed space (base 120567) never contained 15-move puzzles,
	// while level 1's (base 112648) did. Now all levels share the same seed space.
	uint32_t uGlobalSeedCounter = uStartSeed;

	uint32_t uCurrentLevel = 1;
	while (s_bRunning && (uMaxLevels == 0 || uCurrentLevel <= uMaxLevels))
	{
		// All levels use same params: 1-2 blockers (max 3 cells), shapes 2-4
		// Post-generation validation enforces: >= 1 draggable Domino+ and <= 3 blocker cells
		TilePuzzle_LevelGenerator::DifficultyParams xLevelParams = xToolParams;
		const char* pszProfileName = "default";

		printf("Generating level %u [%s]...", uCurrentLevel, pszProfileName);
		fflush(stdout);

		// Validation: 4-5 draggable shapes, at least one with 2+ cells, max 3 total blocker cells
		auto IsLevelValid = [](const TilePuzzleLevelData& xLevel) -> bool
		{
			uint32_t uDraggableShapes = 0;
			bool bHasDraggableComplex = false;
			uint32_t uTotalBlockerCells = 0;
			for (size_t i = 0; i < xLevel.axShapes.size(); ++i)
			{
				const auto& xShape = xLevel.axShapes[i];
				if (!xShape.pxDefinition)
					continue;
				if (xShape.pxDefinition->bDraggable)
				{
					uDraggableShapes++;
					if (xShape.pxDefinition->axCells.size() >= 2)
						bHasDraggableComplex = true;
				}
				else
				{
					uTotalBlockerCells += static_cast<uint32_t>(xShape.pxDefinition->axCells.size());
				}
			}
			bool bValid = uDraggableShapes >= 4 && uDraggableShapes <= 5
				&& bHasDraggableComplex && uTotalBlockerCells <= 3;
			if (!bValid)
			{
				printf(" [REJECT: drag=%u complex=%d blockerCells=%u]",
					uDraggableShapes, bHasDraggableComplex ? 1 : 0, uTotalBlockerCells);
			}
			return bValid;
		};

		auto xStartTime = std::chrono::high_resolution_clock::now();

		TilePuzzleLevelData xBestLevel = {};
		std::vector<TilePuzzleShapeDefinition> axBestLevelDefs; // Local ownership of best level's shape definitions
		uint32_t uBestMoves = 0;
		uint32_t uRetryRound = 0;
		bool bTimedOut = false;
		TilePuzzle_LevelGenerator::GenerationStats xAccumulatedStats = {};

		// Retry loop: generate rounds until target move count reached or timeout
		while (s_bRunning)
		{
			TilePuzzleLevelData xLevel = {};
			TilePuzzle_LevelGenerator::GenerationStats xStats = {};
			bool bSuccess = TilePuzzle_LevelGenerator::GenerateLevel(
				xLevel, xRng, uGlobalSeedCounter, &xStats,
				uRetryRound, &xLevelParams);
			uGlobalSeedCounter++;

			AccumulateGenerationStats(xAccumulatedStats, xStats);

			if (bSuccess && IsLevelValid(xLevel) && xLevel.uMinimumMoves > uBestMoves)
			{
				xBestLevel = std::move(xLevel);
				uBestMoves = xBestLevel.uMinimumMoves;

				// Copy shape definitions to local storage so they survive
				// subsequent GenerateLevel calls (which Clear the static vector)
				axBestLevelDefs.clear();
				for (size_t i = 0; i < xBestLevel.axShapes.size(); ++i)
				{
					if (xBestLevel.axShapes[i].pxDefinition)
						axBestLevelDefs.push_back(*xBestLevel.axShapes[i].pxDefinition);
				}
				// Remap pointers to local storage
				size_t uDefIdx = 0;
				for (size_t i = 0; i < xBestLevel.axShapes.size(); ++i)
				{
					if (xBestLevel.axShapes[i].pxDefinition)
					{
						xBestLevel.axShapes[i].pxDefinition = &axBestLevelDefs[uDefIdx];
						uDefIdx++;
					}
				}
			}

			// Progress output per round
			auto xNow = std::chrono::high_resolution_clock::now();
			double fElapsedSec = std::chrono::duration<double>(xNow - xStartTime).count();
			printf(" [round %u: best=%u, %.0fs]", uRetryRound + 1, uBestMoves, fElapsedSec);
			fflush(stdout);

			if (uBestMoves >= uMinMoves)
				break;

			uRetryRound++;

			// Check timeout
			if (fElapsedSec >= static_cast<double>(uTimeoutSeconds))
			{
				bTimedOut = true;
				break;
			}
		}

		auto xEndTime = std::chrono::high_resolution_clock::now();
		double fTimeMs = std::chrono::duration<double, std::milli>(xEndTime - xStartTime).count();

		// Copy winning params into accumulated stats for JSON output
		if (uBestMoves > 0)
		{
			// Build output file paths
			char szBaseName[64];
			snprintf(szBaseName, sizeof(szBaseName), "level_%04u", uCurrentLevel);

			std::string strTlvlPath = strRunDir + "/" + szBaseName + ".tlvl";
			std::string strJsonPath = strRunDir + "/" + szBaseName + ".json";
			std::string strPngPath = strRunDir + "/" + szBaseName + ".png";

			// Write binary
			Zenith_DataStream xStream;
			TilePuzzleLevelSerialize::Write(xStream, xBestLevel);
			xStream.WriteToFile(strTlvlPath.c_str());

			// Write JSON
			TilePuzzleLevelJson::Write(strJsonPath.c_str(), xBestLevel, uCurrentLevel, &xAccumulatedStats, fTimeMs, uRetryRound + 1, &xLevelParams);

			// Write PNG
			TilePuzzleLevelImage::Write(strPngPath.c_str(), xBestLevel);

			printf(" OK (solver=%u moves, %.1fs, %u rounds%s)\n",
				uBestMoves, fTimeMs / 1000.0, uRetryRound + 1,
				bTimedOut ? " TIMEOUT" : "");

			TilePuzzleLevelGenAnalytics::AccumulateLevel(xRunStats, true, uBestMoves, fTimeMs, &xAccumulatedStats, uRetryRound + 1, bTimedOut);
		}
		else
		{
			printf(" FAILED (%.1fs, %u rounds%s)\n",
				fTimeMs / 1000.0, uRetryRound + 1,
				bTimedOut ? " TIMEOUT" : "");
			TilePuzzleLevelGenAnalytics::AccumulateLevel(xRunStats, false, 0, fTimeMs, &xAccumulatedStats, uRetryRound + 1, bTimedOut);
		}
		fflush(stdout);

		uCurrentLevel++;
	}

	// Write run analytics
	std::string strAnalyticsPath = strRunDir + "/analytics.txt";
	TilePuzzleLevelGenAnalytics::WriteAnalytics(strAnalyticsPath.c_str(), xRunStats);
	printf("\nAnalytics written to: %s\n", strAnalyticsPath.c_str());

	// Shutdown
	Zenith_TaskSystem::Shutdown();

	printf("Done.\n");
	return 0;
}
