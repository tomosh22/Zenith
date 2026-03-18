#include "Zenith.h"
#pragma warning(disable: 4005) // APIENTRY macro redefinition (GLFW vs Windows SDK)
#include "Core/Memory/Zenith_MemoryManagement_Disabled.h"

#include "Components/TilePuzzle_LevelGenerator.h"
#include "Components/TilePuzzleLevelData_Serialize.h"
#include "TilePuzzleLevelMetadata.h"
#include "TilePuzzleLevelData_Image.h"
#include "TilePuzzleLevelGen_Analytics.h"
#include "Components/TilePuzzle_ConditionalValidator.h"

#include <filesystem>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <random>
#include <csignal>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>

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

// TilePuzzleLevelMetadata, s_uMETA_VERSION, ReadMetadataFromFile, WriteMetadataAndLevel
// are defined in TilePuzzleLevelMetadata.h

// ============================================================================
// Difficulty Tier Definitions
// ============================================================================

enum DifficultyTier : uint8_t
{
	DIFFICULTY_TIER_NONE = 0,
	DIFFICULTY_TIER_TUTORIAL_EARLY,
	DIFFICULTY_TIER_TUTORIAL_LATE,
	DIFFICULTY_TIER_EASY,
	DIFFICULTY_TIER_MEDIUM,
	DIFFICULTY_TIER_HARD,
	DIFFICULTY_TIER_EXPERT,
	DIFFICULTY_TIER_MASTER,
	DIFFICULTY_TIER_COUNT
};

struct TierConstraints
{
	uint32_t uMinGrid, uMaxGrid;
	uint32_t uMinColors, uMaxColors;
	uint32_t uMinCatsPerColor, uMaxCatsPerColor;
	uint32_t uMinBlockers, uMaxBlockers;
	uint32_t uMinComplexity, uMaxComplexity; // 1=Single, 2=+Domino, 3=+L/T/I, 4=+S/Z/O
	uint32_t uMinMoves, uMaxMoves;
	uint32_t uMinConditional, uMaxConditional;
	uint32_t uMinBlockerCats, uMaxBlockerCats;
};

static const TierConstraints s_axTierConstraints[] =
{
	{},                                                               // NONE (unused)
	{ 5,  6,  1, 2,  1, 2,  0, 0,  1, 1,   3,  5,  0, 0,  0, 0 },  // TUTORIAL_EARLY (single-cell only)
	{ 5,  6,  1, 2,  1, 2,  0, 0,  2, 2,   3,  5,  0, 0,  0, 0 },  // TUTORIAL_LATE (multi-cell introduced)
	{ 6,  7,  2, 3,  1, 2,  0, 1,  1, 3,   5,  8,  0, 0,  0, 0 },  // EASY
	{ 7,  8,  3, 3,  2, 2,  1, 2,  2, 3,   8, 12,  0, 0,  0, 1 },  // MEDIUM
	{ 8,  9,  3, 4,  2, 3,  1, 2,  3, 4,  10, 15,  0, 2,  0, 1 },  // HARD
	{ 9, 10,  4, 5,  2, 3,  2, 3,  3, 4,  15, 20,  1, 3,  0, 2 },  // EXPERT
	{ 9, 10,  4, 5,  3, 4,  2, 3,  3, 4,  18, 99,  1, 3,  1, 2 },  // MASTER
};

static const char* s_aszTierNames[] =
{
	"none", "tutorial-early", "tutorial-late", "easy", "medium", "hard", "expert", "master"
};

static DifficultyTier ParseTierName(const char* szName)
{
	for (uint32_t i = 1; i < DIFFICULTY_TIER_COUNT; ++i)
	{
		if (strcmp(szName, s_aszTierNames[i]) == 0)
			return static_cast<DifficultyTier>(i);
	}
	return DIFFICULTY_TIER_NONE;
}

// ============================================================================
// Shape Type to Complexity Mapping
// ============================================================================

static uint32_t ShapeTypeToComplexity(TilePuzzleShapeType eType)
{
	switch (eType)
	{
	case TILEPUZZLE_SHAPE_SINGLE: return 1;
	case TILEPUZZLE_SHAPE_DOMINO: return 2;
	case TILEPUZZLE_SHAPE_L:
	case TILEPUZZLE_SHAPE_T:
	case TILEPUZZLE_SHAPE_I:      return 3;
	case TILEPUZZLE_SHAPE_S:
	case TILEPUZZLE_SHAPE_Z:
	case TILEPUZZLE_SHAPE_O:      return 4;
	default:                      return 0;
	}
}

// ============================================================================
// Random Parameter Generation
// ============================================================================

/**
 * RandomizeDifficultyParams - Picks random values with ranges biased by min-moves target
 *
 * Each parameter is sampled independently. Constraints enforced:
 * - colors * catsPerColor + blockerCats <= 16 (solver bitmask limit)
 * - blockerCats <= blockers
 * - conditionalShapes <= colors - 1
 *
 * When uMinMoves is high (>=10), ranges are narrowed to params known to produce
 * harder puzzles (larger grids, more colors, higher complexity).
 */
static TilePuzzle_LevelGenerator::DifficultyParams RandomizeDifficultyParams(std::mt19937& xRng, uint32_t uMinMoves)
{
	TilePuzzle_LevelGenerator::DifficultyParams xParams;

	// Bias ranges based on min-moves target
	uint32_t uMinGrid = 5, uMaxGrid = 10;
	uint32_t uMinColors = 2, uMaxColors = 5;
	uint32_t uMinCats = 1, uMaxCats = 2;
	uint32_t uMinBlockers = 0, uMaxBlockers = 3;
	uint32_t uMinComplexity = 1, uMaxComplexity = 4;
	uint32_t uMinScramble = 100, uMaxScramble = 1500;

	if (uMinMoves >= 20)
	{
		// Very hard: tight range based on proven Master preset
		uMinGrid = 9; uMaxGrid = 10;
		uMinColors = 5; uMaxColors = 5;
		uMinCats = 2; uMaxCats = 2;
		uMinBlockers = 2; uMaxBlockers = 3;
		uMinComplexity = 3; uMaxComplexity = 4;
		uMinScramble = 300; uMaxScramble = 1500;
	}
	else if (uMinMoves >= 10)
	{
		// Hard range: larger grids, more colors, always 2 cats
		uMinGrid = 8; uMaxGrid = 10;
		uMinColors = 4; uMaxColors = 5;
		uMinCats = 2; uMaxCats = 2;
		uMinBlockers = 1; uMaxBlockers = 2;
		uMinComplexity = 2; uMaxComplexity = 4;
		uMinScramble = 300; uMaxScramble = 1000;
	}
	else if (uMinMoves >= 5)
	{
		// Medium range
		uMinGrid = 6; uMaxGrid = 9;
		uMinColors = 3; uMaxColors = 5;
		uMinCats = 1; uMaxCats = 2;
		uMinBlockers = 0; uMaxBlockers = 2;
		uMinComplexity = 2; uMaxComplexity = 4;
		uMinScramble = 200; uMaxScramble = 1000;
	}

	// Grid dimensions
	std::uniform_int_distribution<uint32_t> xGridDist(uMinGrid, uMaxGrid);
	uint32_t uGridW = xGridDist(xRng);
	uint32_t uGridH = xGridDist(xRng);
	xParams.uMinGridWidth = uGridW;
	xParams.uMaxGridWidth = uGridW;
	xParams.uMinGridHeight = uGridH;
	xParams.uMaxGridHeight = uGridH;

	// Colors
	std::uniform_int_distribution<uint32_t> xColorDist(uMinColors, uMaxColors);
	uint32_t uColors = xColorDist(xRng);
	xParams.uMinNumColors = uColors;
	xParams.uNumColors = uColors;

	// Cats per color
	std::uniform_int_distribution<uint32_t> xCatDist(uMinCats, uMaxCats);
	uint32_t uCats = xCatDist(xRng);
	xParams.uMinCatsPerColor = uCats;
	xParams.uNumCatsPerColor = uCats;

	xParams.uNumShapesPerColor = 1;

	// Blockers
	std::uniform_int_distribution<uint32_t> xBlockerDist(uMinBlockers, uMaxBlockers);
	uint32_t uBlockers = xBlockerDist(xRng);
	xParams.uMinBlockers = uBlockers;
	xParams.uNumBlockers = uBlockers;

	// Blocker cats: [0, 2] but capped at blockers (force at least 1 for hard levels)
	uint32_t uMinBlockerCatsRange = (uMinMoves >= 20 && uBlockers >= 1) ? 1 : 0;
	std::uniform_int_distribution<uint32_t> xBlockerCatDist(uMinBlockerCatsRange, std::min(2u, uBlockers));
	uint32_t uBlockerCats = (uBlockers > 0) ? xBlockerCatDist(xRng) : 0;
	xParams.uMinBlockerCats = uBlockerCats;
	xParams.uNumBlockerCats = uBlockerCats;

	// Enforce solver cat limit: colors * catsPerColor + blockerCats <= 16
	while (uColors * uCats + uBlockerCats > 16)
	{
		if (uCats > 1)
			uCats--;
		else if (uBlockerCats > 0)
			uBlockerCats--;
		else
			uColors--;
	}
	xParams.uMinNumColors = uColors;
	xParams.uNumColors = uColors;
	xParams.uMinCatsPerColor = uCats;
	xParams.uNumCatsPerColor = uCats;
	xParams.uMinBlockerCats = uBlockerCats;
	xParams.uNumBlockerCats = uBlockerCats;

	// Shape complexity
	std::uniform_int_distribution<uint32_t> xComplexDist(uMinComplexity, uMaxComplexity);
	uint32_t uComplexity = xComplexDist(xRng);
	xParams.uMinMaxShapeSize = uComplexity;
	xParams.uMaxShapeSize = uComplexity;

	// Conditional shapes: [0, 3] capped at (colors - 1)
	uint32_t uMaxCond = std::min(3u, uColors - 1);
	uint32_t uMinCond = (uMinMoves >= 10 && uMaxCond >= 1) ? 1 : 0;
	std::uniform_int_distribution<uint32_t> xCondDist(uMinCond, uMaxCond);
	uint32_t uCond = xCondDist(xRng);
	xParams.uMinConditionalShapes = uCond;
	xParams.uNumConditionalShapes = uCond;

	// Conditional threshold: [1, 5] if conditionals exist, else 0
	if (uCond > 0)
	{
		uint32_t uMaxThresh = (uMinMoves >= 15) ? 4 : 5;
		std::uniform_int_distribution<uint32_t> xThreshDist(1, uMaxThresh);
		uint32_t uThresh = xThreshDist(xRng);
		xParams.uMinConditionalThreshold = uThresh;
		xParams.uConditionalThreshold = uThresh;
	}
	else
	{
		xParams.uMinConditionalThreshold = 0;
		xParams.uConditionalThreshold = 0;
	}

	// Scramble moves
	std::uniform_int_distribution<uint32_t> xScrambleDist(uMinScramble, uMaxScramble);
	xParams.uScrambleMoves = xScrambleDist(xRng);
	xParams.uMinScrambleMoves = std::max(10u, xParams.uScrambleMoves / 10);

	// Solver settings - scale with target difficulty
	xParams.uMinSolverMoves = 4;
	if (uMinMoves >= 20)
	{
		// Fast 1M solver for workers (~4s rounds), 5M verification to confirm solvability.
		// Most state spaces are < 5M, so verification definitively resolves them.
		// Workers accept candidates via exact solutions or lower bounds to maximize throughput.
		xParams.uSolverStateLimit = 1000000;
		xParams.uDeepSolverStateLimit = 5000000;
		xParams.uMaxDeepVerificationsPerWorker = 0;
		xParams.uMaxAttempts = 500;
		xParams.uMinSolverMoves = 4;

		// Use guided scramble for 20+ move targets (reverse BFS can only reach
		// depth ~5-6 with 10M states due to exponential branching, too shallow)
		xParams.eScrambleMode = TilePuzzle_LevelGenerator::SCRAMBLE_MODE_GUIDED;

		// Multi-start scramble: try 20 trajectories, keep best displacement.
		// Per-restart failure ~97% → 20 restarts gives 97%^20 ≈ 54% failure vs 87% with 5.
		// Cost: 20 restarts × ~20ms each = 400ms per attempt, negligible vs solver time.
		xParams.uScrambleRestarts = 20;
	}
	else if (uMinMoves >= 10)
	{
		// Deeper solver for harder targets
		xParams.uSolverStateLimit = 1000000;
		xParams.uDeepSolverStateLimit = 5000000;
		xParams.uMaxDeepVerificationsPerWorker = 5;
		xParams.uMaxAttempts = 2000;

		// Guided scramble for 10+ move targets; reverse BFS for smaller grids
		// where branching factor is lower and BFS can reach deeper
		if (uGridW * uGridH <= 49)
		{
			xParams.eScrambleMode = TilePuzzle_LevelGenerator::SCRAMBLE_MODE_REVERSE_BFS;
			xParams.uReverseBFSStateLimit = 5000000;
		}
		else
		{
			xParams.eScrambleMode = TilePuzzle_LevelGenerator::SCRAMBLE_MODE_GUIDED;
		}
	}
	else
	{
		uint32_t uGridArea = uGridW * uGridH;
		xParams.uSolverStateLimit = 500000;
		xParams.uDeepSolverStateLimit = (uGridArea >= 64) ? 5000000 : 3000000;
		xParams.uMaxDeepVerificationsPerWorker = 5;
		xParams.uMaxAttempts = 3000;
		// Keep SCRAMBLE_MODE_RANDOM (default) for lower targets
	}

	return xParams;
}

/**
 * RandomizeDifficultyParamsForTier - Picks random values constrained to a specific GDD difficulty tier
 *
 * Uses the TierConstraints table to enforce tight parameter ranges matching the GDD spec.
 * Falls back to the min-moves-based RandomizeDifficultyParams for solver/scramble settings.
 */
static TilePuzzle_LevelGenerator::DifficultyParams RandomizeDifficultyParamsForTier(
	std::mt19937& xRng, DifficultyTier eTier)
{
	const TierConstraints& xTC = s_axTierConstraints[eTier];

	TilePuzzle_LevelGenerator::DifficultyParams xParams;

	// Grid dimensions
	std::uniform_int_distribution<uint32_t> xGridDist(xTC.uMinGrid, xTC.uMaxGrid);
	uint32_t uGridW = xGridDist(xRng);
	uint32_t uGridH = xGridDist(xRng);
	xParams.uMinGridWidth = uGridW;
	xParams.uMaxGridWidth = uGridW;
	xParams.uMinGridHeight = uGridH;
	xParams.uMaxGridHeight = uGridH;

	// Colors
	std::uniform_int_distribution<uint32_t> xColorDist(xTC.uMinColors, xTC.uMaxColors);
	uint32_t uColors = xColorDist(xRng);
	xParams.uMinNumColors = uColors;
	xParams.uNumColors = uColors;

	// Cats per color
	std::uniform_int_distribution<uint32_t> xCatDist(xTC.uMinCatsPerColor, xTC.uMaxCatsPerColor);
	uint32_t uCats = xCatDist(xRng);
	xParams.uMinCatsPerColor = uCats;
	xParams.uNumCatsPerColor = uCats;

	xParams.uNumShapesPerColor = 1;

	// Blockers
	std::uniform_int_distribution<uint32_t> xBlockerDist(xTC.uMinBlockers, xTC.uMaxBlockers);
	uint32_t uBlockers = xBlockerDist(xRng);
	xParams.uMinBlockers = uBlockers;
	xParams.uNumBlockers = uBlockers;

	// Blocker cats
	uint32_t uMaxBC = std::min(xTC.uMaxBlockerCats, uBlockers);
	uint32_t uMinBC = std::min(xTC.uMinBlockerCats, uMaxBC);
	std::uniform_int_distribution<uint32_t> xBlockerCatDist(uMinBC, uMaxBC);
	uint32_t uBlockerCats = (uBlockers > 0) ? xBlockerCatDist(xRng) : 0;
	xParams.uMinBlockerCats = uBlockerCats;
	xParams.uNumBlockerCats = uBlockerCats;

	// Enforce solver cat limit: colors * catsPerColor + blockerCats <= 16
	while (uColors * uCats + uBlockerCats > 16)
	{
		if (uCats > 1)
			uCats--;
		else if (uBlockerCats > 0)
			uBlockerCats--;
		else
			uColors--;
	}
	xParams.uMinNumColors = uColors;
	xParams.uNumColors = uColors;
	xParams.uMinCatsPerColor = uCats;
	xParams.uNumCatsPerColor = uCats;
	xParams.uMinBlockerCats = uBlockerCats;
	xParams.uNumBlockerCats = uBlockerCats;

	// Shape complexity
	std::uniform_int_distribution<uint32_t> xComplexDist(xTC.uMinComplexity, xTC.uMaxComplexity);
	uint32_t uComplexity = xComplexDist(xRng);
	xParams.uMinMaxShapeSize = uComplexity;
	xParams.uMaxShapeSize = uComplexity;

	// Conditional shapes
	uint32_t uMaxCond = std::min(xTC.uMaxConditional, uColors - 1);
	uint32_t uMinCond = std::min(xTC.uMinConditional, uMaxCond);
	std::uniform_int_distribution<uint32_t> xCondDist(uMinCond, uMaxCond);
	uint32_t uCond = xCondDist(xRng);
	xParams.uMinConditionalShapes = uCond;
	xParams.uNumConditionalShapes = uCond;

	if (uCond > 0)
	{
		std::uniform_int_distribution<uint32_t> xThreshDist(1, 4);
		uint32_t uThresh = xThreshDist(xRng);
		xParams.uMinConditionalThreshold = uThresh;
		xParams.uConditionalThreshold = uThresh;
	}
	else
	{
		xParams.uMinConditionalThreshold = 0;
		xParams.uConditionalThreshold = 0;
	}

	// Scramble moves - scale with tier
	uint32_t uMinScramble, uMaxScramble;
	switch (eTier)
	{
	case DIFFICULTY_TIER_TUTORIAL_EARLY:
	case DIFFICULTY_TIER_TUTORIAL_LATE: uMinScramble = 50;  uMaxScramble = 200;  break;
	case DIFFICULTY_TIER_EASY:     uMinScramble = 100; uMaxScramble = 400;  break;
	case DIFFICULTY_TIER_MEDIUM:   uMinScramble = 200; uMaxScramble = 600;  break;
	case DIFFICULTY_TIER_HARD:     uMinScramble = 300; uMaxScramble = 800;  break;
	case DIFFICULTY_TIER_EXPERT:   uMinScramble = 300; uMaxScramble = 1000; break;
	case DIFFICULTY_TIER_MASTER:   uMinScramble = 300; uMaxScramble = 1500; break;
	default:                       uMinScramble = 100; uMaxScramble = 500;  break;
	}
	std::uniform_int_distribution<uint32_t> xScrambleDist(uMinScramble, uMaxScramble);
	xParams.uScrambleMoves = xScrambleDist(xRng);
	xParams.uMinScrambleMoves = std::max(10u, xParams.uScrambleMoves / 10);

	// Solver settings - delegate to the min-moves-based logic
	xParams.uMinSolverMoves = 4;
	uint32_t uTierMinMoves = xTC.uMinMoves;
	if (uTierMinMoves >= 20)
	{
		xParams.uSolverStateLimit = 1000000;
		xParams.uDeepSolverStateLimit = 5000000;
		xParams.uMaxDeepVerificationsPerWorker = 0;
		xParams.uMaxAttempts = 500;
		xParams.eScrambleMode = TilePuzzle_LevelGenerator::SCRAMBLE_MODE_GUIDED;
		xParams.uScrambleRestarts = 20;
	}
	else if (uTierMinMoves >= 10)
	{
		xParams.uSolverStateLimit = 1000000;
		xParams.uDeepSolverStateLimit = 5000000;
		xParams.uMaxDeepVerificationsPerWorker = 5;
		xParams.uMaxAttempts = 2000;
		if (uGridW * uGridH <= 49)
		{
			xParams.eScrambleMode = TilePuzzle_LevelGenerator::SCRAMBLE_MODE_REVERSE_BFS;
			xParams.uReverseBFSStateLimit = 5000000;
		}
		else
		{
			xParams.eScrambleMode = TilePuzzle_LevelGenerator::SCRAMBLE_MODE_GUIDED;
		}
	}
	else
	{
		uint32_t uGridArea = uGridW * uGridH;
		xParams.uSolverStateLimit = 500000;
		xParams.uDeepSolverStateLimit = (uGridArea >= 64) ? 5000000 : 3000000;
		xParams.uMaxDeepVerificationsPerWorker = 5;
		xParams.uMaxAttempts = 3000;
	}

	return xParams;
}

// ============================================================================
// Color-Independent Layout Hash (Fibonacci Hashing)
// ============================================================================

// Fibonacci hash constant for 64-bit: golden ratio * 2^64
static constexpr uint64_t s_ulFIBONACCI_HASH_CONSTANT = 11400714819323198485ull;

/**
 * FibonacciHash - Mix a value into a running hash using Fibonacci hashing
 */
static uint64_t FibonacciHash(uint64_t ulHash, uint64_t ulValue)
{
	ulHash ^= ulValue * s_ulFIBONACCI_HASH_CONSTANT;
	ulHash = (ulHash << 31) | (ulHash >> 33); // Rotate
	ulHash *= s_ulFIBONACCI_HASH_CONSTANT;
	return ulHash;
}

/**
 * ShapeEntry - Color-independent representation of a shape for hashing
 */
struct ShapeLayoutEntry
{
	int32_t iOriginX;
	int32_t iOriginY;
	uint8_t uShapeType;
	uint8_t bIsDraggable;

	bool operator<(const ShapeLayoutEntry& xOther) const
	{
		if (iOriginY != xOther.iOriginY) return iOriginY < xOther.iOriginY;
		if (iOriginX != xOther.iOriginX) return iOriginX < xOther.iOriginX;
		if (uShapeType != xOther.uShapeType) return uShapeType < xOther.uShapeType;
		return bIsDraggable < xOther.bIsDraggable;
	}

	bool operator==(const ShapeLayoutEntry& xOther) const
	{
		return iOriginX == xOther.iOriginX
			&& iOriginY == xOther.iOriginY
			&& uShapeType == xOther.uShapeType
			&& bIsDraggable == xOther.bIsDraggable;
	}
};

/**
 * CatLayoutEntry - Color-independent representation of a cat for hashing
 */
struct CatLayoutEntry
{
	int32_t iGridX;
	int32_t iGridY;
	uint8_t bOnBlocker;

	bool operator<(const CatLayoutEntry& xOther) const
	{
		if (iGridY != xOther.iGridY) return iGridY < xOther.iGridY;
		if (iGridX != xOther.iGridX) return iGridX < xOther.iGridX;
		return bOnBlocker < xOther.bOnBlocker;
	}

	bool operator==(const CatLayoutEntry& xOther) const
	{
		return iGridX == xOther.iGridX
			&& iGridY == xOther.iGridY
			&& bOnBlocker == xOther.bOnBlocker;
	}
};

/**
 * LevelLayoutSignature - Full color-independent structural data for comparison
 */
struct LevelLayoutSignature
{
	uint32_t uGridWidth;
	uint32_t uGridHeight;
	std::vector<TilePuzzleCellType> aeCells;
	std::vector<ShapeLayoutEntry> axShapes;
	std::vector<CatLayoutEntry> axCats;

	bool operator==(const LevelLayoutSignature& xOther) const
	{
		return uGridWidth == xOther.uGridWidth
			&& uGridHeight == xOther.uGridHeight
			&& aeCells == xOther.aeCells
			&& axShapes == xOther.axShapes
			&& axCats == xOther.axCats;
	}
};

/**
 * ComputeLayoutSignature - Extract color-independent structural signature from level data
 */
static LevelLayoutSignature ComputeLayoutSignature(const TilePuzzleLevelData& xLevel)
{
	LevelLayoutSignature xSig;
	xSig.uGridWidth = xLevel.uGridWidth;
	xSig.uGridHeight = xLevel.uGridHeight;
	xSig.aeCells = xLevel.aeCells;

	// Build sorted shape entries (color-independent)
	for (size_t i = 0; i < xLevel.axShapes.size(); ++i)
	{
		const TilePuzzleShapeInstance& xShape = xLevel.axShapes[i];
		ShapeLayoutEntry xEntry;
		xEntry.iOriginX = xShape.iOriginX;
		xEntry.iOriginY = xShape.iOriginY;
		xEntry.uShapeType = xShape.pxDefinition ? static_cast<uint8_t>(xShape.pxDefinition->eType) : 0;
		xEntry.bIsDraggable = (xShape.pxDefinition && xShape.pxDefinition->bDraggable) ? 1 : 0;
		xSig.axShapes.push_back(xEntry);
	}
	std::sort(xSig.axShapes.begin(), xSig.axShapes.end());

	// Build sorted cat entries (color-independent)
	for (size_t i = 0; i < xLevel.axCats.size(); ++i)
	{
		const TilePuzzleCatData& xCat = xLevel.axCats[i];
		CatLayoutEntry xEntry;
		xEntry.iGridX = xCat.iGridX;
		xEntry.iGridY = xCat.iGridY;
		xEntry.bOnBlocker = xCat.bOnBlocker ? 1 : 0;
		xSig.axCats.push_back(xEntry);
	}
	std::sort(xSig.axCats.begin(), xSig.axCats.end());

	return xSig;
}

/**
 * ComputeLayoutHash - Compute a color-independent hash for duplicate detection
 *
 * Hashes grid dimensions + cell layout + shape entries (sorted by position,
 * excluding color) + cat entries (sorted by position, excluding color).
 * Uses Fibonacci hashing for good distribution.
 */
static uint64_t ComputeLayoutHash(const TilePuzzleLevelData& xLevel)
{
	uint64_t ulHash = 0;

	// Hash grid dimensions
	ulHash = FibonacciHash(ulHash, xLevel.uGridWidth);
	ulHash = FibonacciHash(ulHash, xLevel.uGridHeight);

	// Hash cell layout
	for (size_t i = 0; i < xLevel.aeCells.size(); ++i)
	{
		ulHash = FibonacciHash(ulHash, static_cast<uint64_t>(xLevel.aeCells[i]));
	}

	// Build and sort shape entries (color-independent)
	std::vector<ShapeLayoutEntry> axShapeEntries;
	for (size_t i = 0; i < xLevel.axShapes.size(); ++i)
	{
		const TilePuzzleShapeInstance& xShape = xLevel.axShapes[i];
		ShapeLayoutEntry xEntry;
		xEntry.iOriginX = xShape.iOriginX;
		xEntry.iOriginY = xShape.iOriginY;
		xEntry.uShapeType = xShape.pxDefinition ? static_cast<uint8_t>(xShape.pxDefinition->eType) : 0;
		xEntry.bIsDraggable = (xShape.pxDefinition && xShape.pxDefinition->bDraggable) ? 1 : 0;
		axShapeEntries.push_back(xEntry);
	}
	std::sort(axShapeEntries.begin(), axShapeEntries.end());

	for (size_t i = 0; i < axShapeEntries.size(); ++i)
	{
		ulHash = FibonacciHash(ulHash, static_cast<uint64_t>(axShapeEntries[i].iOriginX));
		ulHash = FibonacciHash(ulHash, static_cast<uint64_t>(axShapeEntries[i].iOriginY));
		ulHash = FibonacciHash(ulHash, static_cast<uint64_t>(axShapeEntries[i].uShapeType));
		ulHash = FibonacciHash(ulHash, static_cast<uint64_t>(axShapeEntries[i].bIsDraggable));
	}

	// Build and sort cat entries (color-independent)
	std::vector<CatLayoutEntry> axCatEntries;
	for (size_t i = 0; i < xLevel.axCats.size(); ++i)
	{
		const TilePuzzleCatData& xCat = xLevel.axCats[i];
		CatLayoutEntry xEntry;
		xEntry.iGridX = xCat.iGridX;
		xEntry.iGridY = xCat.iGridY;
		xEntry.bOnBlocker = xCat.bOnBlocker ? 1 : 0;
		axCatEntries.push_back(xEntry);
	}
	std::sort(axCatEntries.begin(), axCatEntries.end());

	for (size_t i = 0; i < axCatEntries.size(); ++i)
	{
		ulHash = FibonacciHash(ulHash, static_cast<uint64_t>(axCatEntries[i].iGridX));
		ulHash = FibonacciHash(ulHash, static_cast<uint64_t>(axCatEntries[i].iGridY));
		ulHash = FibonacciHash(ulHash, static_cast<uint64_t>(axCatEntries[i].bOnBlocker));
	}

	return ulHash;
}

// ============================================================================
// Duplicate Detection State
// ============================================================================
struct DuplicateDetector
{
	// Map from layout hash to list of signatures (for collision resolution)
	std::unordered_map<uint64_t, std::vector<LevelLayoutSignature>> xHashToSignatures;
	uint32_t uDuplicateRejections = 0;

	/**
	 * IsDuplicate - Check if a level's layout is a duplicate of any previously seen layout
	 *
	 * On hash collision, performs full structural comparison to distinguish
	 * true duplicates from hash collisions.
	 *
	 * @return true if this layout is a true duplicate
	 */
	bool IsDuplicate(const TilePuzzleLevelData& xLevel, uint64_t ulHash)
	{
		auto xIt = xHashToSignatures.find(ulHash);
		if (xIt == xHashToSignatures.end())
		{
			return false;
		}

		// Hash collision - do full structural comparison
		LevelLayoutSignature xSig = ComputeLayoutSignature(xLevel);
		for (size_t i = 0; i < xIt->second.size(); ++i)
		{
			if (xSig == xIt->second[i])
			{
				return true; // True duplicate
			}
		}

		return false; // Hash collision but structurally different
	}

	/**
	 * RegisterLevel - Add a level's layout to the known set
	 */
	void RegisterLevel(const TilePuzzleLevelData& xLevel, uint64_t ulHash)
	{
		LevelLayoutSignature xSig = ComputeLayoutSignature(xLevel);
		xHashToSignatures[ulHash].push_back(std::move(xSig));
	}
};

// WriteMetadataAndLevel and ReadMetadataFromFile are in TilePuzzleLevelMetadata.h

/**
 * BuildMetadata - Extract metadata from a generated level
 */
static TilePuzzleLevelMetadata BuildMetadata(
	uint32_t uLevelNumber,
	const TilePuzzleLevelData& xLevel,
	uint32_t uDifficultyTier,
	uint64_t ulLayoutHash,
	uint32_t uGenerationSeed,
	double fGenerationTimeMs,
	uint32_t uRetryRounds,
	const TilePuzzle_LevelGenerator::GenerationStats* pxStats,
	bool bTimedOut)
{
	TilePuzzleLevelMetadata xMeta = {};
	xMeta.uLevelNumber = uLevelNumber;
	xMeta.uGridWidth = xLevel.uGridWidth;
	xMeta.uGridHeight = xLevel.uGridHeight;
	xMeta.uParMoves = xLevel.uMinimumMoves;
	xMeta.uDifficultyTier = uDifficultyTier;
	xMeta.ulLayoutHash = ulLayoutHash;

	// Count colors used
	std::unordered_set<uint8_t> xColorsUsed;
	for (size_t i = 0; i < xLevel.axShapes.size(); ++i)
	{
		if (xLevel.axShapes[i].pxDefinition && xLevel.axShapes[i].pxDefinition->bDraggable)
		{
			xColorsUsed.insert(static_cast<uint8_t>(xLevel.axShapes[i].eColor));
		}
	}
	xMeta.uNumColors = static_cast<uint32_t>(xColorsUsed.size());

	// Count cats per color (use first color's count as representative)
	std::unordered_map<uint8_t, uint32_t> xCatsPerColor;
	for (size_t i = 0; i < xLevel.axCats.size(); ++i)
	{
		xCatsPerColor[static_cast<uint8_t>(xLevel.axCats[i].eColor)]++;
	}
	uint32_t uMaxCatsPerColor = 0;
	for (auto& xPair : xCatsPerColor)
	{
		if (xPair.second > uMaxCatsPerColor)
			uMaxCatsPerColor = xPair.second;
	}
	xMeta.uNumCatsPerColor = uMaxCatsPerColor;

	// Count shapes, blockers, conditionals, and compute complexity
	uint32_t uDraggable = 0;
	uint32_t uBlockers = 0;
	uint32_t uConditional = 0;
	uint32_t uMaxComplexity = 0;
	uint32_t uMinComplexity = UINT32_MAX;
	uint32_t uMaxThreshold = 0;
	for (size_t i = 0; i < xLevel.axShapes.size(); ++i)
	{
		const TilePuzzleShapeInstance& xShape = xLevel.axShapes[i];
		if (xShape.pxDefinition && xShape.pxDefinition->bDraggable)
		{
			uDraggable++;
			if (xShape.uUnlockThreshold > 0)
				uConditional++;
			if (xShape.uUnlockThreshold > uMaxThreshold)
				uMaxThreshold = xShape.uUnlockThreshold;

			uint32_t uComplexity = ShapeTypeToComplexity(xShape.pxDefinition->eType);
			if (uComplexity > uMaxComplexity)
				uMaxComplexity = uComplexity;
			if (uComplexity < uMinComplexity)
				uMinComplexity = uComplexity;
		}
		else
		{
			uBlockers++;
		}
	}
	xMeta.uNumDraggableShapes = uDraggable;
	xMeta.uNumStaticBlockers = uBlockers;
	xMeta.uNumConditionalShapes = uConditional;
	xMeta.uMaxShapeComplexity = uMaxComplexity;
	xMeta.uMinShapeComplexity = (uDraggable > 0) ? uMinComplexity : 0;
	xMeta.uMaxConditionalThreshold = uMaxThreshold;
	xMeta.uNumShapesPerColor = (xMeta.uNumColors > 0) ? (uDraggable / xMeta.uNumColors) : 0;

	// Count blocker cats
	uint32_t uBlockerCats = 0;
	for (size_t i = 0; i < xLevel.axCats.size(); ++i)
	{
		if (xLevel.axCats[i].bOnBlocker)
			uBlockerCats++;
	}
	xMeta.uNumBlockerCats = uBlockerCats;

	// Total cats
	xMeta.uTotalCats = static_cast<uint32_t>(xLevel.axCats.size());

	// Count floor cells
	uint32_t uFloorCells = 0;
	for (size_t i = 0; i < xLevel.aeCells.size(); ++i)
	{
		if (xLevel.aeCells[i] == TILEPUZZLE_CELL_FLOOR)
			uFloorCells++;
	}
	xMeta.uNumFloorCells = uFloorCells;

	// Generation provenance
	xMeta.ulGenerationTimestamp = static_cast<uint64_t>(std::time(nullptr));
	xMeta.uGenerationSeed = uGenerationSeed;
	xMeta.uGenerationTimeMs = static_cast<uint32_t>(fGenerationTimeMs);
	xMeta.uGenerationRetryRounds = uRetryRounds;
	xMeta.uGenerationTotalAttempts = pxStats ? pxStats->uTotalAttempts : 0;
	xMeta.bGenerationTimedOut = bTimedOut ? 1 : 0;

	return xMeta;
}

// DoesLevelMatchParams removed — flat registry uses min-moves only

// ============================================================================
// Level Registry - Cache for reusing previously generated levels
// ============================================================================

struct RegistryEntry
{
	std::string strFilePath;
	TilePuzzleLevelMetadata xMetadata;
};

struct LevelRegistry
{
	std::vector<RegistryEntry> axEntries;
	std::unordered_set<std::string> xConsumedPaths;

	uint32_t uTotalEntries = 0;
	uint32_t uDuplicatesRemoved = 0;
	uint32_t uCacheHits = 0;
	uint32_t uCacheMisses = 0;

	void ScanAndLoad(const std::string& strRegistryDir)
	{
		printf("Scanning level registry: %s\n", strRegistryDir.c_str());
		fflush(stdout);

		if (!std::filesystem::exists(strRegistryDir))
		{
			printf("  Registry directory does not exist, will be created on first cache miss.\n\n");
			fflush(stdout);
			return;
		}

		std::unordered_map<uint64_t, std::vector<size_t>> xHashToIndices;

		for (auto& xEntry : std::filesystem::directory_iterator(strRegistryDir))
		{
			if (!xEntry.is_regular_file())
				continue;
			if (xEntry.path().extension() != ".tlvl")
				continue;

			TilePuzzleLevelMetadata xMeta = {};
			std::string strPath = xEntry.path().string();
			if (!ReadMetadataFromFile(strPath.c_str(), xMeta))
			{
				printf("  WARNING: Failed to read metadata from %s\n", strPath.c_str());
				continue;
			}

			size_t uIdx = axEntries.size();
			axEntries.push_back({strPath, xMeta});
			xHashToIndices[xMeta.ulLayoutHash].push_back(uIdx);
		}

		Deduplicate(xHashToIndices);

		uTotalEntries = static_cast<uint32_t>(axEntries.size());

		printf("  Registry loaded: %u entries", uTotalEntries);
		if (uDuplicatesRemoved > 0)
			printf(", %u duplicates removed", uDuplicatesRemoved);
		printf("\n\n");
		fflush(stdout);
	}

	const RegistryEntry* FindMatch(
		uint32_t uMinMoves,
		const DuplicateDetector& xDuplicateDetector,
		DifficultyTier eTier = DIFFICULTY_TIER_NONE) const
	{
		for (size_t i = 0; i < axEntries.size(); ++i)
		{
			const RegistryEntry& xEntry = axEntries[i];

			if (xConsumedPaths.count(xEntry.strFilePath))
				continue;

			if (xEntry.xMetadata.uParMoves < uMinMoves)
				continue;

			if (xDuplicateDetector.xHashToSignatures.count(xEntry.xMetadata.ulLayoutHash))
				continue;

			// When a tier is specified, filter by tier-compatible structural parameters
			if (eTier != DIFFICULTY_TIER_NONE)
			{
				const TierConstraints& xTC = s_axTierConstraints[eTier];
				const TilePuzzleLevelMetadata& xMeta = xEntry.xMetadata;

				if (xMeta.uGridWidth < xTC.uMinGrid || xMeta.uGridWidth > xTC.uMaxGrid)
					continue;
				if (xMeta.uGridHeight < xTC.uMinGrid || xMeta.uGridHeight > xTC.uMaxGrid)
					continue;
				if (xMeta.uNumColors < xTC.uMinColors || xMeta.uNumColors > xTC.uMaxColors)
					continue;
				if (xMeta.uNumStaticBlockers < xTC.uMinBlockers || xMeta.uNumStaticBlockers > xTC.uMaxBlockers)
					continue;
				if (xMeta.uMaxShapeComplexity > xTC.uMaxComplexity)
					continue;
				if (xMeta.uNumConditionalShapes > xTC.uMaxConditional)
					continue;
				if (xMeta.uParMoves > xTC.uMaxMoves && xTC.uMaxMoves < 99)
					continue;
			}

			return &xEntry;
		}

		return nullptr;
	}

	void MarkConsumed(const RegistryEntry& xEntry)
	{
		xConsumedPaths.insert(xEntry.strFilePath);
	}

private:
	void Deduplicate(const std::unordered_map<uint64_t, std::vector<size_t>>& xHashToIndices)
	{
		std::unordered_set<size_t> xIndicesToRemove;

		for (auto& xPair : xHashToIndices)
		{
			if (xPair.second.size() <= 1)
				continue;

			struct LoadedEntry
			{
				size_t uOrigIndex;
				LevelLayoutSignature xSig;
			};
			std::vector<LoadedEntry> axLoaded;

			for (size_t idx : xPair.second)
			{
				const RegistryEntry& xRegEntry = axEntries[idx];

				Zenith_DataStream xStream;
				xStream.ReadFromFile(xRegEntry.strFilePath.c_str());
				if (!xStream.IsValid())
					continue;

				TilePuzzleLevelData xLevel = {};
				Zenith_Vector<TilePuzzleShapeDefinition> axDefs;
				if (!TilePuzzleLevelSerialize::Read(xStream, xLevel, axDefs))
					continue;

				LoadedEntry xLE;
				xLE.uOrigIndex = idx;
				xLE.xSig = ComputeLayoutSignature(xLevel);
				axLoaded.push_back(std::move(xLE));
			}

			for (size_t i = 0; i < axLoaded.size(); ++i)
			{
				if (xIndicesToRemove.count(axLoaded[i].uOrigIndex))
					continue;

				for (size_t j = i + 1; j < axLoaded.size(); ++j)
				{
					if (xIndicesToRemove.count(axLoaded[j].uOrigIndex))
						continue;

					if (axLoaded[i].xSig == axLoaded[j].xSig)
					{
						xIndicesToRemove.insert(axLoaded[j].uOrigIndex);

						const std::string& strPath = axEntries[axLoaded[j].uOrigIndex].strFilePath;
						std::filesystem::remove(strPath);
						std::string strPngPath = strPath.substr(0, strPath.size() - 5) + ".png";
						std::filesystem::remove(strPngPath);
						printf("  Removed duplicate: %s\n", strPath.c_str());
						uDuplicatesRemoved++;
					}
				}
			}
		}

		if (!xIndicesToRemove.empty())
		{
			std::vector<RegistryEntry> axFiltered;
			for (size_t i = 0; i < axEntries.size(); ++i)
			{
				if (!xIndicesToRemove.count(i))
					axFiltered.push_back(std::move(axEntries[i]));
			}
			axEntries = std::move(axFiltered);
		}
	}
};

// ============================================================================
// Registry Helper Functions
// ============================================================================

static bool CopyRegistryLevelToDestination(
	const RegistryEntry& xEntry,
	const std::string& strOutputDir,
	uint32_t uDestLevelNumber,
	TilePuzzleLevelData& xLevelOut,
	std::vector<TilePuzzleShapeDefinition>& axDefsOut)
{
	char szBaseName[64];
	snprintf(szBaseName, sizeof(szBaseName), "level_%04u", uDestLevelNumber);

	std::string strDestTlvl = strOutputDir + "/" + szBaseName + ".tlvl";
	std::string strDestPng = strOutputDir + "/" + szBaseName + ".png";

	// Copy .tlvl
	std::error_code xEc;
	std::filesystem::copy_file(
		xEntry.strFilePath, strDestTlvl,
		std::filesystem::copy_options::overwrite_existing, xEc);
	if (xEc)
	{
		printf(" [REGISTRY COPY FAILED: %s]", xEc.message().c_str());
		return false;
	}

	// Copy .png
	std::string strRegistryPng = xEntry.strFilePath.substr(0, xEntry.strFilePath.size() - 5) + ".png";
	std::filesystem::copy_file(
		strRegistryPng, strDestPng,
		std::filesystem::copy_options::overwrite_existing, xEc);
	if (xEc)
	{
		printf(" [REGISTRY PNG COPY FAILED: %s]", xEc.message().c_str());
		// Non-fatal - .tlvl was copied successfully
	}

	// Load level data for DuplicateDetector registration
	Zenith_DataStream xStream;
	xStream.ReadFromFile(xEntry.strFilePath.c_str());
	if (!xStream.IsValid())
		return false;

	Zenith_Vector<TilePuzzleShapeDefinition> axEngineDefs;
	if (!TilePuzzleLevelSerialize::Read(xStream, xLevelOut, axEngineDefs))
		return false;

	// Copy defs to std::vector
	axDefsOut.clear();
	for (uint32_t i = 0; i < axEngineDefs.GetSize(); ++i)
		axDefsOut.push_back(axEngineDefs.Get(i));

	// Remap shape definition pointers
	uint32_t uDefIdx = 0;
	for (size_t i = 0; i < xLevelOut.axShapes.size(); ++i)
	{
		if (xLevelOut.axShapes[i].pxDefinition)
		{
			xLevelOut.axShapes[i].pxDefinition = &axDefsOut[uDefIdx];
			uDefIdx++;
		}
	}

	return true;
}

static void RegisterLevelInRegistry(
	const std::string& strRegistryDir,
	const TilePuzzleLevelMetadata& xMeta,
	const TilePuzzleLevelData& xLevel,
	LevelRegistry& xRegistry)
{
	std::filesystem::create_directories(strRegistryDir);

	char szFileName[128];
	snprintf(szFileName, sizeof(szFileName), "%016llx.tlvl",
		static_cast<unsigned long long>(xMeta.ulLayoutHash));

	std::string strRegistryTlvl = strRegistryDir + "/" + szFileName;

	if (std::filesystem::exists(strRegistryTlvl))
		return;

	WriteMetadataAndLevel(strRegistryTlvl.c_str(), xMeta, xLevel);

	// Write companion .png
	std::string strRegistryPng = strRegistryTlvl.substr(0, strRegistryTlvl.size() - 5) + ".png";
	TilePuzzleLevelImage::Write(strRegistryPng.c_str(), xLevel);

	RegistryEntry xEntry;
	xEntry.strFilePath = strRegistryTlvl;
	xEntry.xMetadata = xMeta;
	xRegistry.axEntries.push_back(std::move(xEntry));
	xRegistry.uTotalEntries++;
}

// ============================================================================
// Utility Functions
// ============================================================================

static void AccumulateGenerationStats(
	TilePuzzle_LevelGenerator::GenerationStats& xAccum,
	const TilePuzzle_LevelGenerator::GenerationStats& xNew)
{
	xAccum.uTotalAttempts += xNew.uTotalAttempts;
	xAccum.uScrambleFailures += xNew.uScrambleFailures;
	xAccum.uSolverTooEasy += xNew.uSolverTooEasy;
	xAccum.uSolverUnsolvable += xNew.uSolverUnsolvable;
	xAccum.uTotalSuccesses += xNew.uTotalSuccesses;
	xAccum.uLowerBoundAccepts += xNew.uLowerBoundAccepts;
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

// ============================================================================
// Level Validation
// ============================================================================

/**
 * IsLevelValid - Structural validation for generated levels
 *
 * When eTier != DIFFICULTY_TIER_NONE, validates against the tier's GDD constraints.
 */
static bool IsLevelValid(const TilePuzzleLevelData& xLevel, DifficultyTier eTier)
{
	uint32_t uDraggableShapes = 0;
	uint32_t uTotalBlockerCells = 0;
	uint32_t uStaticBlockers = 0;
	uint32_t uMaxComplexity = 0;
	std::unordered_set<uint8_t> xColors;

	for (size_t i = 0; i < xLevel.axShapes.size(); ++i)
	{
		const auto& xShape = xLevel.axShapes[i];
		if (!xShape.pxDefinition)
			continue;
		if (xShape.pxDefinition->bDraggable)
		{
			uDraggableShapes++;
			xColors.insert(xShape.eColor);
			uint32_t uC = ShapeTypeToComplexity(xShape.pxDefinition->eType);
			if (uC > uMaxComplexity)
				uMaxComplexity = uC;
		}
		else
		{
			uStaticBlockers++;
			uTotalBlockerCells += static_cast<uint32_t>(xShape.pxDefinition->axCells.size());
		}
	}

	// Basic validation
	if (uDraggableShapes < 1 || uTotalBlockerCells > 12)
		return false;

	// Tier-specific validation
	if (eTier != DIFFICULTY_TIER_NONE)
	{
		const TierConstraints& xTC = s_axTierConstraints[eTier];
		uint32_t uNumColors = static_cast<uint32_t>(xColors.size());

		if (xLevel.uGridWidth < xTC.uMinGrid || xLevel.uGridWidth > xTC.uMaxGrid)
			return false;
		if (xLevel.uGridHeight < xTC.uMinGrid || xLevel.uGridHeight > xTC.uMaxGrid)
			return false;
		if (uNumColors < xTC.uMinColors || uNumColors > xTC.uMaxColors)
			return false;
		if (uStaticBlockers < xTC.uMinBlockers || uStaticBlockers > xTC.uMaxBlockers)
			return false;
		if (uMaxComplexity > xTC.uMaxComplexity)
			return false;
	}

	return true;
}

// ============================================================================
// Core Generation Loop (shared between modes)
// ============================================================================

/**
 * GenerateSingleLevel - Generate a single level with retry loop
 *
 * @param xBestLevelOut     Output: best generated level data
 * @param axBestLevelDefs   Output: owned shape definitions for the best level
 * @param xParamRng         RNG for randomizing params each retry round
 * @param uMinMoves         Minimum par moves target for retry loop
 * @param uTimeoutSeconds   Per-level timeout
 * @param uGlobalSeedCounter  In/out: global seed counter, incremented per round
 * @param uLevelNumber      Level number for display
 * @param pxAccumulatedStats  Output: accumulated generation stats
 * @param uRetryRoundsOut   Output: number of retry rounds used
 * @param bTimedOutOut       Output: whether generation timed out
 * @param pfnValidate        Optional validation function pointer (replaces lambda)
 * @param eTier              Difficulty tier for validation
 * @return Best moves achieved (0 = complete failure)
 */
typedef bool (*PfnLevelValidate)(const TilePuzzleLevelData& xLevel, DifficultyTier eTier);

static uint32_t GenerateSingleLevel(
	TilePuzzleLevelData& xBestLevelOut,
	std::vector<TilePuzzleShapeDefinition>& axBestLevelDefs,
	std::mt19937& xParamRng,
	uint32_t uMinMoves,
	uint32_t uTimeoutSeconds,
	uint32_t& uGlobalSeedCounter,
	uint32_t /*uLevelNumber*/,
	TilePuzzle_LevelGenerator::GenerationStats& xAccumulatedStats,
	uint32_t& uRetryRoundsOut,
	bool& bTimedOutOut,
	PfnLevelValidate pfnValidate,
	DifficultyTier eTier)
{
	std::mt19937 xRng(42); // Unused by GenerateLevel but required by API

	xBestLevelOut = {};
	axBestLevelDefs.clear();
	uint32_t uBestMoves = 0;
	uint32_t uRetryRound = 0;
	bTimedOutOut = false;
	xAccumulatedStats = {};

	// Accept threshold: for verified generation, the 5M solver gives exact solutions.
	// Levels in the 20+ tier max out around 20-23 moves (limited by the puzzle design).
	// Accept at 60% of target or 15 (whichever is lower) to find levels efficiently,
	// while still getting the hardest possible levels within the time budget.
	uint32_t uAcceptThreshold = uMinMoves;
	if (uMinMoves >= 20)
		uAcceptThreshold = std::min(uMinMoves * 3 / 5, 15u);

	auto xStartTime = std::chrono::high_resolution_clock::now();

	while (s_bRunning)
	{
		// Re-randomize params each retry round
		TilePuzzle_LevelGenerator::DifficultyParams xLevelParams =
			(eTier != DIFFICULTY_TIER_NONE)
				? RandomizeDifficultyParamsForTier(xParamRng, eTier)
				: RandomizeDifficultyParams(xParamRng, uMinMoves);

		TilePuzzleLevelData xLevel = {};
		TilePuzzle_LevelGenerator::GenerationStats xStats = {};
		bool bSuccess = TilePuzzle_LevelGenerator::GenerateLevel(
			xLevel, xRng, uGlobalSeedCounter, &xStats,
			uRetryRound, &xLevelParams);
		uGlobalSeedCounter++;

		AccumulateGenerationStats(xAccumulatedStats, xStats);

		bool bValid = true;
		if (pfnValidate)
			bValid = pfnValidate(xLevel, eTier);

		if (bSuccess && bValid && xLevel.uMinimumMoves > uBestMoves)
		{
			xBestLevelOut = std::move(xLevel);
			uBestMoves = xBestLevelOut.uMinimumMoves;

			// Copy shape definitions to local storage so they survive
			// subsequent GenerateLevel calls (which Clear the static vector)
			axBestLevelDefs.clear();
			for (size_t i = 0; i < xBestLevelOut.axShapes.size(); ++i)
			{
				if (xBestLevelOut.axShapes[i].pxDefinition)
					axBestLevelDefs.push_back(*xBestLevelOut.axShapes[i].pxDefinition);
			}
			// Remap pointers to local storage
			size_t uDefIdx = 0;
			for (size_t i = 0; i < xBestLevelOut.axShapes.size(); ++i)
			{
				if (xBestLevelOut.axShapes[i].pxDefinition)
				{
					xBestLevelOut.axShapes[i].pxDefinition = &axBestLevelDefs[uDefIdx];
					uDefIdx++;
				}
			}
		}

		// Progress output per round (use stderr for immediate visibility in piped contexts)
		auto xNow = std::chrono::high_resolution_clock::now();
		double fElapsedSec = std::chrono::duration<double>(xNow - xStartTime).count();
		fprintf(stderr, " [round %u: best=%u, %.0fs]", uRetryRound + 1, uBestMoves, fElapsedSec);

		if (uBestMoves >= uAcceptThreshold)
			break;

		uRetryRound++;

		// Check timeout
		if (fElapsedSec >= static_cast<double>(uTimeoutSeconds))
		{
			bTimedOutOut = true;
			break;
		}
	}

	uRetryRoundsOut = uRetryRound + 1;
	return uBestMoves;
}

// ============================================================================
// Registry Validation (parallel task)
// ============================================================================

enum ValidationResult : uint8_t { VALIDATE_PENDING, VALIDATE_VERIFIED, VALIDATE_UNVERIFIED, VALIDATE_UNSOLVABLE, VALIDATE_LOAD_FAILED };

struct ValidationEntry
{
	ValidationResult eResult = VALIDATE_PENDING;
	int32_t iExactMoves = -1;
	int32_t iLowerBound = 0;
	uint32_t uStoredMoves = 0;
};

struct ValidateTaskData
{
	const std::vector<std::string>* paxFilePaths;
	ValidationEntry* paxResults;
	uint32_t uMaxStates;
};

static void ValidateRegistryTask(void* pData, u_int uInvocationIndex, u_int /*uNumInvocations*/)
{
	ValidateTaskData* pxData = static_cast<ValidateTaskData*>(pData);

	const std::string& strPath = (*pxData->paxFilePaths)[uInvocationIndex];
	ValidationEntry& xEntry = pxData->paxResults[uInvocationIndex];

	// Load level from .tlvl
	Zenith_DataStream xStream;
	xStream.ReadFromFile(strPath.c_str());
	if (!xStream.IsValid())
	{
		xEntry.eResult = VALIDATE_LOAD_FAILED;
		return;
	}

	TilePuzzleLevelData xLevel;
	Zenith_Vector<TilePuzzleShapeDefinition> axDefs;
	if (!TilePuzzleLevelSerialize::Read(xStream, xLevel, axDefs))
	{
		xEntry.eResult = VALIDATE_LOAD_FAILED;
		return;
	}

	xEntry.uStoredMoves = xLevel.uMinimumMoves;

	// Run solver
	int32_t iLowerBound = 0;
	int32_t iResult = TilePuzzle_Solver::SolveLevel(xLevel, pxData->uMaxStates, &iLowerBound);

	if (iResult >= 0)
	{
		xEntry.eResult = VALIDATE_VERIFIED;
		xEntry.iExactMoves = iResult;
	}
	else if (iLowerBound > 0)
	{
		xEntry.eResult = VALIDATE_UNVERIFIED;
		xEntry.iLowerBound = iLowerBound;
	}
	else
	{
		xEntry.eResult = VALIDATE_UNSOLVABLE;
	}
}

// ============================================================================
// Conditional Validation (parallel task)
// ============================================================================

struct ConditionalValidationEntry
{
	bool bLoadSuccess = false;
	bool bModified = false;
	uint32_t uRemovedCount = 0;
	uint32_t uOriginalConditionals = 0;
};

struct ConditionalValidationTaskData
{
	const std::vector<std::string>* paxFilePaths;
	ConditionalValidationEntry* paxResults;
};

static void ValidateConditionalsTask(void* pData, u_int uInvocationIndex, u_int /*uNumInvocations*/)
{
	ConditionalValidationTaskData* pxData = static_cast<ConditionalValidationTaskData*>(pData);
	const std::string& strPath = (*pxData->paxFilePaths)[uInvocationIndex];
	ConditionalValidationEntry& xEntry = pxData->paxResults[uInvocationIndex];

	// Load level from .tlvl
	Zenith_DataStream xStream;
	xStream.ReadFromFile(strPath.c_str());
	if (!xStream.IsValid())
		return;

	TilePuzzleLevelData xLevel;
	Zenith_Vector<TilePuzzleShapeDefinition> axDefs;
	if (!TilePuzzleLevelSerialize::Read(xStream, xLevel, axDefs))
		return;

	xEntry.bLoadSuccess = true;

	// Run conditional validation
	TilePuzzle_ConditionalValidator::ValidationResult xValResult =
		TilePuzzle_ConditionalValidator::ValidateConditionalShapes(xLevel);

	xEntry.uOriginalConditionals = xValResult.uOriginalConditionalCount;
	xEntry.uRemovedCount = xValResult.uRemovedCount;

	if (xValResult.uRemovedCount > 0)
	{
		xEntry.bModified = true;

		// Re-read metadata, update conditional fields, re-save
		TilePuzzleLevelMetadata xMeta;
		if (ReadMetadataFromFile(strPath.c_str(), xMeta))
		{
			TilePuzzle_ConditionalValidator::UpdateMetadataConditionals(xMeta, xLevel);
			WriteMetadataAndLevel(strPath.c_str(), xMeta, xLevel);
		}
	}
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[])
{
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);
	printf("TilePuzzle Level Generator\n");
	printf("==========================\n\n");
	fflush(stdout);

	// Parse CLI arguments
	std::string strOutputDir = LEVELGEN_OUTPUT_DIR;
	uint32_t uTimeoutSeconds = 7200; // 2 hours default
	srand(time(0));
	uint32_t uStartSeed = rand();
	uint32_t uCount = 0;
	uint32_t uMinMoves = 0;
	DifficultyTier eTier = DIFFICULTY_TIER_NONE;
	bool bValidateRegistry = false;
	bool bValidateConditionals = false;
	bool bDeleteInvalid = false;
	bool bProfile = false;
	uint32_t uValidateMaxStates = 5000000; // 5M default for validation

	for (int i = 1; i < argc; ++i)
	{
		if (strcmp(argv[i], "--output") == 0 && i + 1 < argc)
		{
			strOutputDir = argv[++i];
		}
		else if (strcmp(argv[i], "--timeout") == 0 && i + 1 < argc)
		{
			uTimeoutSeconds = static_cast<uint32_t>(atoi(argv[++i]));
		}
		else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc)
		{
			uStartSeed = static_cast<uint32_t>(atoi(argv[++i]));
		}
		else if (strcmp(argv[i], "--count") == 0 && i + 1 < argc)
		{
			uCount = static_cast<uint32_t>(atoi(argv[++i]));
		}
		else if (strcmp(argv[i], "--min-moves") == 0 && i + 1 < argc)
		{
			uMinMoves = static_cast<uint32_t>(atoi(argv[++i]));
		}
		else if (strcmp(argv[i], "--tier") == 0 && i + 1 < argc)
		{
			eTier = ParseTierName(argv[++i]);
			if (eTier == DIFFICULTY_TIER_NONE)
			{
				fprintf(stderr, "Error: Unknown tier '%s'. Valid tiers: tutorial-early, tutorial-late, easy, medium, hard, expert, master\n", argv[i]);
				return 1;
			}
		}
		else if (strcmp(argv[i], "--validate-registry") == 0)
		{
			bValidateRegistry = true;
		}
		else if (strcmp(argv[i], "--delete-invalid") == 0)
		{
			bDeleteInvalid = true;
		}
		else if (strcmp(argv[i], "--validate-max-states") == 0 && i + 1 < argc)
		{
			uValidateMaxStates = static_cast<uint32_t>(atoi(argv[++i]));
		}
		else if (strcmp(argv[i], "--validate-conditionals") == 0)
		{
			bValidateConditionals = true;
		}
		else if (strcmp(argv[i], "--profile") == 0)
		{
			bProfile = true;
			TilePuzzle_Solver::s_bDetailedProfiling = true;
		}
	}

	// When --tier is specified, derive --min-moves from the tier constraints if not explicitly set
	if (eTier != DIFFICULTY_TIER_NONE && uMinMoves == 0)
	{
		uMinMoves = s_axTierConstraints[eTier].uMinMoves;
	}

	// Validate required arguments (not needed for --validate-registry or --validate-conditionals)
	if (!bValidateRegistry && !bValidateConditionals)
	{
		if (uCount == 0)
		{
			fprintf(stderr, "Error: --count <N> is required (number of levels to generate).\n");
			fprintf(stderr, "Usage: tilepuzzlelevelgen --count N --min-moves M [--tier TIER] [--output DIR] [--timeout S] [--seed N] [--profile]\n");
			fprintf(stderr, "       tilepuzzlelevelgen --count N --tier TIER [--output DIR] [--timeout S] [--seed N] [--profile]\n");
			fprintf(stderr, "       tilepuzzlelevelgen --validate-registry [--delete-invalid] [--validate-max-states N]\n");
			fprintf(stderr, "       tilepuzzlelevelgen --validate-conditionals\n");
			fprintf(stderr, "Tiers: tutorial-early, tutorial-late, easy, medium, hard, expert, master\n");
			return 1;
		}
		if (uMinMoves == 0)
		{
			fprintf(stderr, "Error: --min-moves <M> or --tier <TIER> is required.\n");
			return 1;
		}
	}

	// Minimal engine init (same sequence as Zenith_Core::Zenith_Init, first 4 calls)
	Zenith_MemoryManagement::Initialise();
	Zenith_Profiling::Initialise();
	Zenith_Multithreading::RegisterThread(true);
	Zenith_TaskSystem::Inititalise();

	// Set up Ctrl+C handler
	signal(SIGINT, SignalHandler);

	// ========================================================================
	// Registry Validation Mode (parallel)
	// ========================================================================
	if (bValidateRegistry)
	{
		std::string strRegistryDir = LEVELGEN_REGISTRY_DIR;
		printf("Mode: Registry Validation (max states=%u%s, parallel)\n\n", uValidateMaxStates, bDeleteInvalid ? ", delete-invalid" : "");
		fflush(stdout);

		if (!std::filesystem::exists(strRegistryDir))
		{
			printf("Registry directory does not exist: %s\n", strRegistryDir.c_str());
			return 1;
		}

		std::vector<std::string> axFilePaths;
		for (auto& xEntry : std::filesystem::directory_iterator(strRegistryDir))
		{
			if (xEntry.is_regular_file() && xEntry.path().extension() == ".tlvl")
				axFilePaths.push_back(xEntry.path().string());
		}

		uint32_t uNumFiles = static_cast<uint32_t>(axFilePaths.size());
		printf("Found %u .tlvl files in registry\n\n", uNumFiles);
		fflush(stdout);

		if (uNumFiles == 0)
		{
			printf("Nothing to validate.\n");
			return 0;
		}

		std::vector<ValidationEntry> axResults(uNumFiles);

		ValidateTaskData xTaskData;
		xTaskData.paxFilePaths = &axFilePaths;
		xTaskData.paxResults = axResults.data();
		xTaskData.uMaxStates = uValidateMaxStates;

		auto xStartTime = std::chrono::high_resolution_clock::now();

		printf("Validating %u levels in parallel...\n\n", uNumFiles);
		fflush(stdout);

		// Process in batches to avoid overflowing the task queue (max 128 entries)
		static constexpr uint32_t uBATCH_SIZE = 64;
		for (uint32_t uBatchStart = 0; uBatchStart < uNumFiles; uBatchStart += uBATCH_SIZE)
		{
			uint32_t uBatchEnd = std::min(uBatchStart + uBATCH_SIZE, uNumFiles);
			uint32_t uBatchCount = uBatchEnd - uBatchStart;

			// Create a task data view for this batch
			std::vector<std::string> axBatchPaths(axFilePaths.begin() + uBatchStart, axFilePaths.begin() + uBatchEnd);
			ValidateTaskData xBatchData;
			xBatchData.paxFilePaths = &axBatchPaths;
			xBatchData.paxResults = axResults.data() + uBatchStart;
			xBatchData.uMaxStates = uValidateMaxStates;

			printf("  Batch %u-%u/%u...\n", uBatchStart + 1, uBatchEnd, uNumFiles);
			fflush(stdout);

			Zenith_TaskArray xTaskArray(
				ZENITH_PROFILE_INDEX__SCENE_UPDATE,
				&ValidateRegistryTask,
				&xBatchData,
				static_cast<u_int>(uBatchCount),
				true);

			Zenith_TaskSystem::SubmitTaskArray(&xTaskArray);
			xTaskArray.WaitUntilComplete();
		}

		auto xEndTime = std::chrono::high_resolution_clock::now();
		double fElapsedSec = std::chrono::duration<double>(xEndTime - xStartTime).count();

		// Aggregate and print results
		uint32_t uVerified = 0, uUnsolvable = 0, uUnverified = 0, uLoadFailed = 0;
		std::vector<std::string> axUnsolvablePaths;
		std::vector<std::string> axUnverifiedPaths;

		for (uint32_t i = 0; i < uNumFiles; ++i)
		{
			const std::string& strPath = axFilePaths[i];
			std::string strFileName = std::filesystem::path(strPath).filename().string();
			const ValidationEntry& xEntry = axResults[i];

			switch (xEntry.eResult)
			{
			case VALIDATE_VERIFIED:
				printf("  %s: VERIFIED (exact=%d moves, stored=%u)\n", strFileName.c_str(), xEntry.iExactMoves, xEntry.uStoredMoves);
				uVerified++;
				break;
			case VALIDATE_UNVERIFIED:
				printf("  %s: UNVERIFIED (state limit hit, lower bound=%d, stored=%u)\n", strFileName.c_str(), xEntry.iLowerBound, xEntry.uStoredMoves);
				uUnverified++;
				axUnverifiedPaths.push_back(strPath);
				break;
			case VALIDATE_UNSOLVABLE:
				printf("  %s: UNSOLVABLE (no solution exists, stored=%u)\n", strFileName.c_str(), xEntry.uStoredMoves);
				uUnsolvable++;
				axUnsolvablePaths.push_back(strPath);
				break;
			case VALIDATE_LOAD_FAILED:
				printf("  %s: LOAD FAILED\n", strFileName.c_str());
				uLoadFailed++;
				break;
			default:
				break;
			}
		}

		// Delete unsolvable levels if requested
		if (bDeleteInvalid && !axUnsolvablePaths.empty())
		{
			printf("\nDeleting %zu unsolvable levels...\n", axUnsolvablePaths.size());
			for (const auto& strPath : axUnsolvablePaths)
			{
				std::filesystem::remove(strPath);
				std::string strPng = strPath.substr(0, strPath.size() - 5) + ".png";
				std::filesystem::remove(strPng);
				printf("  DELETED %s\n", std::filesystem::path(strPath).filename().string().c_str());
			}
		}

		printf("\n========================================\n");
		printf("Registry Validation Results\n");
		printf("========================================\n");
		printf("Total files:   %u\n", uNumFiles);
		printf("Verified:      %u (exact solution found)\n", uVerified);
		printf("Unverified:    %u (state limit hit, lower bound valid)\n", uUnverified);
		printf("UNSOLVABLE:    %u (no solution exists)\n", uUnsolvable);
		printf("Load failed:   %u\n", uLoadFailed);
		printf("Time:          %.1f seconds\n", fElapsedSec);
		printf("========================================\n");

		if (!axUnverifiedPaths.empty() && axUnverifiedPaths.size() <= 30)
		{
			printf("\nUnverified levels (need higher --validate-max-states):\n");
			for (const auto& strPath : axUnverifiedPaths)
				printf("  %s\n", std::filesystem::path(strPath).filename().string().c_str());
		}

		fflush(stdout);
		return uUnsolvable > 0 ? 1 : 0;
	}

	// ========================================================================
	// Conditional Validation Mode (parallel)
	// ========================================================================
	if (bValidateConditionals)
	{
		std::string strRegistryDir = LEVELGEN_REGISTRY_DIR;
		printf("Mode: Conditional Validation (parallel)\n\n");
		fflush(stdout);

		if (!std::filesystem::exists(strRegistryDir))
		{
			printf("Registry directory does not exist: %s\n", strRegistryDir.c_str());
			return 1;
		}

		// Collect all .tlvl files
		std::vector<std::string> axFilePaths;
		for (auto& xEntry : std::filesystem::directory_iterator(strRegistryDir))
		{
			if (xEntry.is_regular_file() && xEntry.path().extension() == ".tlvl")
				axFilePaths.push_back(xEntry.path().string());
		}

		uint32_t uNumFiles = static_cast<uint32_t>(axFilePaths.size());
		printf("Found %u .tlvl files in registry\n\n", uNumFiles);
		fflush(stdout);

		if (uNumFiles == 0)
		{
			printf("Nothing to validate.\n");
			return 0;
		}

		std::vector<ConditionalValidationEntry> axResults(uNumFiles);

		auto xStartTime = std::chrono::high_resolution_clock::now();

		printf("Validating conditionals in %u levels (parallel)...\n\n", uNumFiles);
		fflush(stdout);

		// Process in batches (same pattern as validate-registry)
		static constexpr uint32_t uBATCH_SIZE = 64;
		for (uint32_t uBatchStart = 0; uBatchStart < uNumFiles && s_bRunning; uBatchStart += uBATCH_SIZE)
		{
			uint32_t uBatchEnd = std::min(uBatchStart + uBATCH_SIZE, uNumFiles);
			uint32_t uBatchCount = uBatchEnd - uBatchStart;

			std::vector<std::string> axBatchPaths(axFilePaths.begin() + uBatchStart, axFilePaths.begin() + uBatchEnd);
			ConditionalValidationTaskData xBatchData;
			xBatchData.paxFilePaths = &axBatchPaths;
			xBatchData.paxResults = axResults.data() + uBatchStart;

			printf("  Batch %u-%u/%u...\n", uBatchStart + 1, uBatchEnd, uNumFiles);
			fflush(stdout);

			Zenith_TaskArray xTaskArray(
				ZENITH_PROFILE_INDEX__SCENE_UPDATE,
				&ValidateConditionalsTask,
				&xBatchData,
				static_cast<u_int>(uBatchCount),
				true);

			Zenith_TaskSystem::SubmitTaskArray(&xTaskArray);
			xTaskArray.WaitUntilComplete();
		}

		auto xEndTime = std::chrono::high_resolution_clock::now();
		double fElapsedSec = std::chrono::duration<double>(xEndTime - xStartTime).count();

		// Aggregate and print results
		uint32_t uLoadFailed = 0, uNoConditionals = 0, uUnchanged = 0, uModified = 0;
		uint32_t uTotalRemoved = 0;

		for (uint32_t i = 0; i < uNumFiles; ++i)
		{
			const std::string& strPath = axFilePaths[i];
			std::string strFileName = std::filesystem::path(strPath).filename().string();
			const ConditionalValidationEntry& xEntry = axResults[i];

			if (!xEntry.bLoadSuccess)
			{
				printf("  %s: LOAD FAILED\n", strFileName.c_str());
				uLoadFailed++;
			}
			else if (xEntry.uOriginalConditionals == 0)
			{
				uNoConditionals++;
			}
			else if (xEntry.bModified)
			{
				printf("  %s: MODIFIED (%u/%u conditionals removed)\n",
					strFileName.c_str(), xEntry.uRemovedCount, xEntry.uOriginalConditionals);
				uModified++;
				uTotalRemoved += xEntry.uRemovedCount;
			}
			else
			{
				printf("  %s: UNCHANGED (%u conditionals all affect gameplay)\n",
					strFileName.c_str(), xEntry.uOriginalConditionals);
				uUnchanged++;
			}
		}

		printf("\n========================================\n");
		printf("Conditional Validation Results\n");
		printf("========================================\n");
		printf("Total files:     %u\n", uNumFiles);
		printf("No conditionals: %u (skipped)\n", uNoConditionals);
		printf("Unchanged:       %u (all conditionals affect gameplay)\n", uUnchanged);
		printf("Modified:        %u (pointless conditionals removed)\n", uModified);
		printf("Total removed:   %u conditional thresholds\n", uTotalRemoved);
		printf("Load failed:     %u\n", uLoadFailed);
		printf("Time:            %.1f seconds\n", fElapsedSec);
		printf("========================================\n");

		fflush(stdout);
		return 0;
	}

	// ========================================================================
	// Random Generation
	// ========================================================================

	if (eTier != DIFFICULTY_TIER_NONE)
		printf("Mode: Tier Generation (%u levels, tier=%s, min-moves=%u)\n", uCount, s_aszTierNames[eTier], uMinMoves);
	else
		printf("Mode: Random Generation (%u levels, min-moves=%u)\n", uCount, uMinMoves);
	printf("Timeout per level: %us, Seed: %u\n\n", uTimeoutSeconds, uStartSeed);
	fflush(stdout);

	// Create output directory
	std::filesystem::create_directories(strOutputDir);
	printf("Output directory: %s\n\n", strOutputDir.c_str());
	fflush(stdout);

	// Duplicate detection and analytics tracking
	DuplicateDetector xDuplicateDetector;
	TilePuzzleLevelGenAnalytics::RunStats xRunStats = {};

	uint32_t uGlobalSeedCounter = uStartSeed;

	// Registry initialization
	std::string strRegistryDir = LEVELGEN_REGISTRY_DIR;
	LevelRegistry xRegistry;
	xRegistry.ScanAndLoad(strRegistryDir);

	// Parameter randomization RNG (separate from generation RNG)
	std::mt19937 xParamRng(uStartSeed * 104729 + 42);

	uint32_t uLevelNum = 1;

	for (uint32_t uIdx = 0; uIdx < uCount && s_bRunning; ++uIdx)
	{
		printf("Generating level %u/%u...", uIdx + 1, uCount);
		fflush(stdout);

		// Registry lookup
		const RegistryEntry* pxMatch = xRegistry.FindMatch(uMinMoves, xDuplicateDetector, eTier);
		if (pxMatch)
		{
			TilePuzzleLevelData xCachedLevel = {};
			std::vector<TilePuzzleShapeDefinition> axCachedDefs;
			if (CopyRegistryLevelToDestination(*pxMatch, strOutputDir, uLevelNum, xCachedLevel, axCachedDefs))
			{
				uint64_t ulHash = pxMatch->xMetadata.ulLayoutHash;
				xDuplicateDetector.RegisterLevel(xCachedLevel, ulHash);
				xRegistry.MarkConsumed(*pxMatch);
				xRegistry.uCacheHits++;

				printf(" CACHE HIT (registry, solver=%u moves)\n", pxMatch->xMetadata.uParMoves);
				fflush(stdout);

				TilePuzzleLevelGenAnalytics::AccumulateLevel(xRunStats, true, pxMatch->xMetadata.uParMoves, 0.0, nullptr, 0, false);
				uLevelNum++;
				continue;
			}
		}
		xRegistry.uCacheMisses++;

		auto xStartTime = std::chrono::high_resolution_clock::now();

		// Retry with duplicate detection
		static constexpr uint32_t s_uMAX_DUPLICATE_RETRIES = 10;
		uint32_t uDuplicateRetries = 0;
		bool bAccepted = false;

		while (!bAccepted && uDuplicateRetries < s_uMAX_DUPLICATE_RETRIES && s_bRunning)
		{
			if (bProfile)
				Zenith_Profiling::ClearEvents();

			TilePuzzleLevelData xBestLevel = {};
			std::vector<TilePuzzleShapeDefinition> axBestLevelDefs;
			TilePuzzle_LevelGenerator::GenerationStats xAccumulatedStats = {};
			uint32_t uRetryRounds = 0;
			bool bTimedOut = false;

			uint32_t uBestMoves = GenerateSingleLevel(
				xBestLevel, axBestLevelDefs, xParamRng,
				uMinMoves, uTimeoutSeconds,
				uGlobalSeedCounter, uIdx,
				xAccumulatedStats, uRetryRounds, bTimedOut,
				&IsLevelValid, eTier);

			auto xEndTime = std::chrono::high_resolution_clock::now();
			double fTimeMs = std::chrono::duration<double, std::milli>(xEndTime - xStartTime).count();

			if (bProfile)
				Zenith_Profiling::WriteTextReport(stderr);

			if (uBestMoves > 0)
			{
				// Check for duplicates
				uint64_t ulLayoutHash = ComputeLayoutHash(xBestLevel);

				if (xDuplicateDetector.IsDuplicate(xBestLevel, ulLayoutHash))
				{
					xDuplicateDetector.uDuplicateRejections++;
					uDuplicateRetries++;
					printf(" [DUPLICATE - retrying]");
					fflush(stdout);
					continue;
				}

				// Accept the level
				xDuplicateDetector.RegisterLevel(xBestLevel, ulLayoutHash);
				bAccepted = true;

				// Validate conditional shapes — remove any that don't affect minimum moves
				{
					auto xValResult = TilePuzzle_ConditionalValidator::ValidateConditionalShapes(xBestLevel);
					if (xValResult.uRemovedCount > 0)
						printf(" [%u/%u conditionals removed - didn't affect solve]",
							xValResult.uRemovedCount, xValResult.uOriginalConditionalCount);
				}

				// Build metadata
				TilePuzzleLevelMetadata xMeta = BuildMetadata(uLevelNum, xBestLevel,
					static_cast<uint32_t>(eTier), ulLayoutHash,
					uGlobalSeedCounter, fTimeMs, uRetryRounds, &xAccumulatedStats, bTimedOut);

				// Build output file paths
				char szBaseName[64];
				snprintf(szBaseName, sizeof(szBaseName), "level_%04u", uLevelNum);

				std::string strTlvlPath = strOutputDir + "/" + szBaseName + ".tlvl";
				std::string strPngPath = strOutputDir + "/" + szBaseName + ".png";

				// Write binary with metadata header
				WriteMetadataAndLevel(strTlvlPath.c_str(), xMeta, xBestLevel);

				// Write PNG
				TilePuzzleLevelImage::Write(strPngPath.c_str(), xBestLevel);

				// Register in level registry
				RegisterLevelInRegistry(strRegistryDir, xMeta, xBestLevel, xRegistry);

				printf(" OK (solver=%u moves, %ux%u grid, %u colors, %.1fs, %u rounds%s)\n",
					uBestMoves,
					xBestLevel.uGridWidth, xBestLevel.uGridHeight,
					xMeta.uNumColors,
					fTimeMs / 1000.0, uRetryRounds,
					bTimedOut ? " TIMEOUT" : "");

				// Accumulate stats
				TilePuzzleLevelGenAnalytics::AccumulateLevel(xRunStats, true, uBestMoves, fTimeMs, &xAccumulatedStats, uRetryRounds, bTimedOut);
				uLevelNum++;
			}
			else
			{
				printf(" FAILED (%.1fs, %u rounds%s)\n",
					fTimeMs / 1000.0, uRetryRounds,
					bTimedOut ? " TIMEOUT" : "");
				TilePuzzleLevelGenAnalytics::AccumulateLevel(xRunStats, false, 0, fTimeMs, &xAccumulatedStats, uRetryRounds, bTimedOut);
				break;
			}
			fflush(stdout);
		}

		if (uDuplicateRetries >= s_uMAX_DUPLICATE_RETRIES)
		{
			printf(" FAILED (max duplicate retries exceeded)\n");
			fflush(stdout);
		}
	}

	// Write run analytics
	std::string strAnalyticsPath = strOutputDir + "/analytics.txt";
	TilePuzzleLevelGenAnalytics::WriteAnalytics(strAnalyticsPath.c_str(), xRunStats);
	printf("\nAnalytics written to: %s\n", strAnalyticsPath.c_str());

	// Write profiling report
	if (bProfile)
	{
		std::string strProfilingPath = strOutputDir + "/profiling.txt";
		FILE* pProfilingFile = fopen(strProfilingPath.c_str(), "w");
		if (pProfilingFile)
		{
			Zenith_Profiling::WriteTextReport(pProfilingFile);
			fclose(pProfilingFile);
			printf("Profiling written to: %s\n", strProfilingPath.c_str());
		}
	}

	printf("Registry: %u cache hits, %u cache misses, %u total entries\n",
		xRegistry.uCacheHits, xRegistry.uCacheMisses, xRegistry.uTotalEntries);

	// Shutdown
	Zenith_TaskSystem::Shutdown();

	printf("Done.\n");
	return 0;
}
