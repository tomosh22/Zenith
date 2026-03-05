#include "Zenith.h"
#pragma warning(disable: 4005) // APIENTRY macro redefinition (GLFW vs Windows SDK)
#include "Core/Memory/Zenith_MemoryManagement_Disabled.h"

#include "Components/TilePuzzle_LevelGenerator.h"
#include "Components/TilePuzzleLevelData_Serialize.h"
#include "TilePuzzleLevelMetadata.h"
#include "TilePuzzleLevelData_Image.h"
#include "TilePuzzleLevelGen_Analytics.h"

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

// DifficultyTier kept only for PfnLevelValidate signature compatibility
enum DifficultyTier : uint8_t
{
	DIFFICULTY_TIER_NONE = 0,
	DIFFICULTY_TIER_COUNT
};

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
		uMinBlockers = 2; uMaxBlockers = 2;
		uMinComplexity = 2; uMaxComplexity = 4;
		uMinScramble = 300; uMaxScramble = 1000;
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
		// Maximum depth for 20+ move targets
		xParams.uSolverStateLimit = 2000000;
		xParams.uDeepSolverStateLimit = 10000000;
		xParams.uMaxDeepVerificationsPerWorker = 10;
		xParams.uMaxAttempts = 3000;
	}
	else if (uMinMoves >= 10)
	{
		// Deeper solver for harder targets
		xParams.uSolverStateLimit = 1000000;
		xParams.uDeepSolverStateLimit = 5000000;
		xParams.uMaxDeepVerificationsPerWorker = 5;
		xParams.uMaxAttempts = 2000;
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
		const DuplicateDetector& xDuplicateDetector) const
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
 * IsLevelValid - Basic structural validation for generated levels
 */
static bool IsLevelValid(const TilePuzzleLevelData& xLevel, DifficultyTier /*eTier*/)
{
	uint32_t uDraggableShapes = 0;
	uint32_t uTotalBlockerCells = 0;
	for (size_t i = 0; i < xLevel.axShapes.size(); ++i)
	{
		const auto& xShape = xLevel.axShapes[i];
		if (!xShape.pxDefinition)
			continue;
		if (xShape.pxDefinition->bDraggable)
			uDraggableShapes++;
		else
			uTotalBlockerCells += static_cast<uint32_t>(xShape.pxDefinition->axCells.size());
	}
	return uDraggableShapes >= 1 && uTotalBlockerCells <= 12;
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

	auto xStartTime = std::chrono::high_resolution_clock::now();

	while (s_bRunning)
	{
		// Re-randomize params each retry round (biased by min-moves target)
		TilePuzzle_LevelGenerator::DifficultyParams xLevelParams = RandomizeDifficultyParams(xParamRng, uMinMoves);

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
			bTimedOutOut = true;
			break;
		}
	}

	uRetryRoundsOut = uRetryRound + 1;
	return uBestMoves;
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[])
{
	printf("TilePuzzle Level Generator\n");
	printf("==========================\n\n");
	fflush(stdout);

	// Parse CLI arguments
	std::string strOutputDir = LEVELGEN_OUTPUT_DIR;
	uint32_t uTimeoutSeconds = 1800; // 30 minutes default
	srand(time(0));
	uint32_t uStartSeed = rand();
	uint32_t uCount = 0;
	uint32_t uMinMoves = 0;

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
	}

	// Validate required arguments
	if (uCount == 0)
	{
		fprintf(stderr, "Error: --count <N> is required (number of levels to generate).\n");
		fprintf(stderr, "Usage: tilepuzzlelevelgen --count N --min-moves M [--output DIR] [--timeout S] [--seed N]\n");
		return 1;
	}
	if (uMinMoves == 0)
	{
		fprintf(stderr, "Error: --min-moves <M> is required (minimum solver moves).\n");
		return 1;
	}

	// Minimal engine init (same sequence as Zenith_Core::Zenith_Init, first 4 calls)
	Zenith_MemoryManagement::Initialise();
	Zenith_Profiling::Initialise();
	Zenith_Multithreading::RegisterThread(true);
	Zenith_TaskSystem::Inititalise();

	// Set up Ctrl+C handler
	signal(SIGINT, SignalHandler);

	// ========================================================================
	// Random Generation
	// ========================================================================

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

		// Registry lookup (flat, min-moves only)
		const RegistryEntry* pxMatch = xRegistry.FindMatch(uMinMoves, xDuplicateDetector);
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
				&IsLevelValid, DIFFICULTY_TIER_NONE);

			auto xEndTime = std::chrono::high_resolution_clock::now();
			double fTimeMs = std::chrono::duration<double, std::milli>(xEndTime - xStartTime).count();

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

				// Build metadata
				TilePuzzleLevelMetadata xMeta = BuildMetadata(uLevelNum, xBestLevel, UINT32_MAX, ulLayoutHash,
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

	printf("Registry: %u cache hits, %u cache misses, %u total entries\n",
		xRegistry.uCacheHits, xRegistry.uCacheMisses, xRegistry.uTotalEntries);

	// Shutdown
	Zenith_TaskSystem::Shutdown();

	printf("Done.\n");
	return 0;
}
