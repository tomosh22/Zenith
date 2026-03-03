#include "Zenith.h"
#pragma warning(disable: 4005) // APIENTRY macro redefinition (GLFW vs Windows SDK)
#include "Core/Memory/Zenith_MemoryManagement_Disabled.h"

#include "Components/TilePuzzle_LevelGenerator.h"
#include "Components/TilePuzzleLevelData_Serialize.h"
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

// ============================================================================
// Generation Modes
// ============================================================================
enum GenerationMode : uint8_t
{
	GENMODE_BATCH = 0,     // --batch START-END with preset difficulty
	GENMODE_DAILY          // --daily YEAR for daily puzzles
};

// ============================================================================
// Difficulty Tiers
// ============================================================================
enum DifficultyTier : uint8_t
{
	DIFFICULTY_TIER_TUTORIAL = 0,
	DIFFICULTY_TIER_EASY,
	DIFFICULTY_TIER_MEDIUM,
	DIFFICULTY_TIER_HARD,
	DIFFICULTY_TIER_EXPERT,
	DIFFICULTY_TIER_MASTER,
	DIFFICULTY_TIER_COUNT
};

static const char* DifficultyTierToString(DifficultyTier eTier)
{
	switch (eTier)
	{
	case DIFFICULTY_TIER_TUTORIAL: return "Tutorial";
	case DIFFICULTY_TIER_EASY:    return "Easy";
	case DIFFICULTY_TIER_MEDIUM:  return "Medium";
	case DIFFICULTY_TIER_HARD:    return "Hard";
	case DIFFICULTY_TIER_EXPERT:  return "Expert";
	case DIFFICULTY_TIER_MASTER:  return "Master";
	default:                      return "Unknown";
	}
}

// ============================================================================
// TilePuzzleLevelMetadata (v2)
// ============================================================================
struct TilePuzzleLevelMetadata
{
	// === Level structure (from v1) ===
	uint32_t uLevelNumber;
	uint32_t uGridWidth;
	uint32_t uGridHeight;
	uint32_t uNumColors;
	uint32_t uNumCatsPerColor;
	uint32_t uNumDraggableShapes;
	uint32_t uNumStaticBlockers;
	uint32_t uNumBlockerCats;
	uint32_t uNumConditionalShapes;
	uint32_t uParMoves;
	uint32_t uDifficultyTier;
	uint64_t ulLayoutHash;

	// === Registry matching (new) ===
	uint32_t uMaxShapeComplexity;       // Highest complexity tier (1-4) among draggable shapes
	uint32_t uMinShapeComplexity;       // Lowest complexity tier among draggable shapes
	uint32_t uMaxConditionalThreshold;  // Highest unlock threshold (0 if no conditionals)
	uint32_t uNumShapesPerColor;        // Draggable shapes / num colors
	uint32_t uTotalCats;                // Total cat count across all colors
	uint32_t uNumFloorCells;            // Number of floor cells (playable area)

	// === Generation provenance (new) ===
	uint64_t ulGenerationTimestamp;     // Unix timestamp (seconds since epoch)
	uint32_t uGenerationSeed;           // Global seed counter at time of generation
	uint32_t uGenerationTimeMs;         // Wall-clock generation time in milliseconds
	uint32_t uGenerationRetryRounds;    // Number of retry rounds needed
	uint32_t uGenerationTotalAttempts;  // Total attempts across all rounds
	uint32_t bGenerationTimedOut;       // Whether generation hit the timeout
};

// Metadata version for .tlvl header extension (s_uMETA_MAGIC defined in TilePuzzleLevelData_Serialize.h)
static constexpr uint32_t s_uMETA_VERSION = 2;

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
// V1 Difficulty Presets (6-tier curve)
// ============================================================================

/**
 * GetDifficultyTierForLevel - Maps level number to difficulty tier
 */
static DifficultyTier GetDifficultyTierForLevel(uint32_t uLevel)
{
	if (uLevel <= 10)  return DIFFICULTY_TIER_TUTORIAL;
	if (uLevel <= 25)  return DIFFICULTY_TIER_EASY;
	if (uLevel <= 45)  return DIFFICULTY_TIER_MEDIUM;
	if (uLevel <= 65)  return DIFFICULTY_TIER_HARD;
	if (uLevel <= 80)  return DIFFICULTY_TIER_EXPERT;
	return DIFFICULTY_TIER_MASTER;
}

/**
 * GetDifficultyPresetV1 - Returns DifficultyParams for the V1 difficulty curve
 *
 * Tuned for batch generation with preset-appropriate solver limits.
 * Lower tiers use less aggressive solver settings since they target fewer moves.
 */
static TilePuzzle_LevelGenerator::DifficultyParams GetDifficultyPresetV1(uint32_t uLevel)
{
	TilePuzzle_LevelGenerator::DifficultyParams xParams;
	DifficultyTier eTier = GetDifficultyTierForLevel(uLevel);

	switch (eTier)
	{
	case DIFFICULTY_TIER_TUTORIAL:
		// Levels 1-10: Simplest puzzles for onboarding
		// 5-6 grid, 2 colors, 1 cat/color, single-cell shapes only, no blockers
		xParams.uMinGridWidth = 5;
		xParams.uMaxGridWidth = 6;
		xParams.uMinGridHeight = 5;
		xParams.uMaxGridHeight = 6;
		xParams.uMinNumColors = 2;
		xParams.uNumColors = 2;
		xParams.uMinCatsPerColor = 1;
		xParams.uNumCatsPerColor = 1;
		xParams.uNumShapesPerColor = 1;
		xParams.uMinBlockers = 0;
		xParams.uNumBlockers = 0;
		xParams.uMinBlockerCats = 0;
		xParams.uNumBlockerCats = 0;
		xParams.uMinConditionalShapes = 0;
		xParams.uNumConditionalShapes = 0;
		xParams.uConditionalThreshold = 0;
		xParams.uMinMaxShapeSize = 1;
		xParams.uMaxShapeSize = 1;
		xParams.uMinScrambleMoves = 10;
		xParams.uScrambleMoves = 100;
		xParams.uMinSolverMoves = 2;
		xParams.uSolverStateLimit = 100000;
		xParams.uDeepSolverStateLimit = 0;
		xParams.uMaxDeepVerificationsPerWorker = 0;
		xParams.uMaxAttempts = 500;
		break;

	case DIFFICULTY_TIER_EASY:
		// Levels 11-25: Introduce dominos and third color
		// 6-7 grid, 2-3 colors, 1-2 cats/color, single+domino, no blockers
		xParams.uMinGridWidth = 6;
		xParams.uMaxGridWidth = 7;
		xParams.uMinGridHeight = 6;
		xParams.uMaxGridHeight = 7;
		xParams.uMinNumColors = 2;
		xParams.uNumColors = 3;
		xParams.uMinCatsPerColor = 1;
		xParams.uNumCatsPerColor = 2;
		xParams.uNumShapesPerColor = 1;
		xParams.uMinBlockers = 0;
		xParams.uNumBlockers = 0;
		xParams.uMinBlockerCats = 0;
		xParams.uNumBlockerCats = 0;
		xParams.uMinConditionalShapes = 0;
		xParams.uNumConditionalShapes = 0;
		xParams.uConditionalThreshold = 0;
		xParams.uMinMaxShapeSize = 1;
		xParams.uMaxShapeSize = 2;
		xParams.uMinScrambleMoves = 15;
		xParams.uScrambleMoves = 200;
		xParams.uMinSolverMoves = 4;
		xParams.uSolverStateLimit = 200000;
		xParams.uDeepSolverStateLimit = 0;
		xParams.uMaxDeepVerificationsPerWorker = 0;
		xParams.uMaxAttempts = 1000;
		break;

	case DIFFICULTY_TIER_MEDIUM:
		// Levels 26-45: I-shapes, blockers introduced
		// 7-8 grid, 3-4 colors, 2 cats/color, up to I-shape, 1 blocker
		xParams.uMinGridWidth = 7;
		xParams.uMaxGridWidth = 8;
		xParams.uMinGridHeight = 7;
		xParams.uMaxGridHeight = 8;
		xParams.uMinNumColors = 3;
		xParams.uNumColors = 4;
		xParams.uMinCatsPerColor = 2;
		xParams.uNumCatsPerColor = 2;
		xParams.uNumShapesPerColor = 1;
		xParams.uMinBlockers = 1;
		xParams.uNumBlockers = 1;
		xParams.uMinBlockerCats = 0;
		xParams.uNumBlockerCats = 0;
		xParams.uMinConditionalShapes = 0;
		xParams.uNumConditionalShapes = 0;
		xParams.uConditionalThreshold = 0;
		xParams.uMinMaxShapeSize = 2;
		xParams.uMaxShapeSize = 3;
		xParams.uMinScrambleMoves = 20;
		xParams.uScrambleMoves = 400;
		xParams.uMinSolverMoves = 4;
		xParams.uSolverStateLimit = 500000;
		xParams.uDeepSolverStateLimit = 1000000;
		xParams.uMaxDeepVerificationsPerWorker = 3;
		xParams.uMaxAttempts = 1500;
		break;

	case DIFFICULTY_TIER_HARD:
		// Levels 46-65: All shapes, blocker-cats
		// 8-9 grid, 3-5 colors, 2 cats/color, all shapes, 1 blocker + 1 blocker-cat
		xParams.uMinGridWidth = 8;
		xParams.uMaxGridWidth = 9;
		xParams.uMinGridHeight = 8;
		xParams.uMaxGridHeight = 9;
		xParams.uMinNumColors = 3;
		xParams.uNumColors = 5;
		xParams.uMinCatsPerColor = 2;
		xParams.uNumCatsPerColor = 2;
		xParams.uNumShapesPerColor = 1;
		xParams.uMinBlockers = 1;
		xParams.uNumBlockers = 1;
		xParams.uMinBlockerCats = 0;
		xParams.uNumBlockerCats = 1;
		xParams.uMinConditionalShapes = 0;
		xParams.uNumConditionalShapes = 0;
		xParams.uConditionalThreshold = 0;
		xParams.uMinMaxShapeSize = 2;
		xParams.uMaxShapeSize = 4;
		xParams.uMinScrambleMoves = 30;
		xParams.uScrambleMoves = 600;
		xParams.uMinSolverMoves = 6;
		xParams.uSolverStateLimit = 500000;
		xParams.uDeepSolverStateLimit = 3000000;
		xParams.uMaxDeepVerificationsPerWorker = 5;
		xParams.uMaxAttempts = 2000;
		break;

	case DIFFICULTY_TIER_EXPERT:
		// Levels 66-80: Conditional shapes introduced
		// 8-9 grid, 4-5 colors, 2 cats/color, all shapes, 1-2 blockers, conditionals
		xParams.uMinGridWidth = 8;
		xParams.uMaxGridWidth = 9;
		xParams.uMinGridHeight = 8;
		xParams.uMaxGridHeight = 9;
		xParams.uMinNumColors = 4;
		xParams.uNumColors = 5;
		xParams.uMinCatsPerColor = 2;
		xParams.uNumCatsPerColor = 2;
		xParams.uNumShapesPerColor = 1;
		xParams.uMinBlockers = 1;
		xParams.uNumBlockers = 2;
		xParams.uMinBlockerCats = 0;
		xParams.uNumBlockerCats = 0;
		xParams.uMinConditionalShapes = 1;
		xParams.uNumConditionalShapes = 2;
		xParams.uMinConditionalThreshold = 1;
		xParams.uConditionalThreshold = 3;
		xParams.uMinMaxShapeSize = 2;
		xParams.uMaxShapeSize = 4;
		xParams.uMinScrambleMoves = 30;
		xParams.uScrambleMoves = 800;
		xParams.uMinSolverMoves = 6;
		xParams.uSolverStateLimit = 500000;
		xParams.uDeepSolverStateLimit = 5000000;
		xParams.uMaxDeepVerificationsPerWorker = 5;
		xParams.uMaxAttempts = 2500;
		break;

	case DIFFICULTY_TIER_MASTER:
		// Levels 81-100: Full complexity
		// 9-10 grid, 5 colors, 2 cats/color, all shapes, 2 blockers + blocker-cat, conditionals
		xParams.uMinGridWidth = 9;
		xParams.uMaxGridWidth = 10;
		xParams.uMinGridHeight = 9;
		xParams.uMaxGridHeight = 10;
		xParams.uMinNumColors = 5;
		xParams.uNumColors = 5;
		xParams.uMinCatsPerColor = 2;
		xParams.uNumCatsPerColor = 2;
		xParams.uNumShapesPerColor = 1;
		xParams.uMinBlockers = 2;
		xParams.uNumBlockers = 2;
		xParams.uMinBlockerCats = 0;
		xParams.uNumBlockerCats = 1;
		xParams.uMinConditionalShapes = 1;
		xParams.uNumConditionalShapes = 2;
		xParams.uMinConditionalThreshold = 1;
		xParams.uConditionalThreshold = 4;
		xParams.uMinMaxShapeSize = 2;
		xParams.uMaxShapeSize = 4;
		xParams.uMinScrambleMoves = 30;
		xParams.uScrambleMoves = 1000;
		xParams.uMinSolverMoves = 6;
		xParams.uSolverStateLimit = 500000;
		xParams.uDeepSolverStateLimit = 5000000;
		xParams.uMaxDeepVerificationsPerWorker = 5;
		xParams.uMaxAttempts = 3000;
		break;

	default:
		break;
	}

	return xParams;
}

/**
 * GetMinMovesForTier - Returns the minimum par move target for batch mode
 * based on difficulty tier. These are the OUTER retry loop targets.
 */
static uint32_t GetMinMovesForTier(DifficultyTier eTier)
{
	switch (eTier)
	{
	case DIFFICULTY_TIER_TUTORIAL: return 2;
	case DIFFICULTY_TIER_EASY:    return 6;
	case DIFFICULTY_TIER_MEDIUM:  return 6;
	case DIFFICULTY_TIER_HARD:    return 8;
	case DIFFICULTY_TIER_EXPERT:  return 8;
	case DIFFICULTY_TIER_MASTER:  return 10;
	default:                      return 4;
	}
}

/**
 * GetMaxMovesForTier - Returns the maximum desired par moves for a tier.
 * Used for level validation: levels with par above this are still accepted
 * but we stop retrying once we hit the minimum.
 */
static uint32_t GetMaxMovesForTier(DifficultyTier eTier)
{
	switch (eTier)
	{
	case DIFFICULTY_TIER_TUTORIAL: return 6;
	case DIFFICULTY_TIER_EASY:    return 8;
	case DIFFICULTY_TIER_MEDIUM:  return 10;
	case DIFFICULTY_TIER_HARD:    return 12;
	case DIFFICULTY_TIER_EXPERT:  return 14;
	case DIFFICULTY_TIER_MASTER:  return 20;
	default:                      return 6;
	}
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

// ============================================================================
// Metadata Serialization
// ============================================================================

/**
 * WriteMetadataHeader - Prepend metadata to the .tlvl file
 *
 * Writes a metadata block before the standard TPLV data.
 * Format: META_MAGIC | META_VERSION | metadata fields | TPLV data...
 */
static void WriteMetadataAndLevel(
	const char* szPath,
	const TilePuzzleLevelMetadata& xMeta,
	const TilePuzzleLevelData& xLevel)
{
	Zenith_DataStream xStream;

	// Write metadata header
	xStream << s_uMETA_MAGIC;
	xStream << s_uMETA_VERSION;
	xStream << xMeta.uLevelNumber;
	xStream << xMeta.uGridWidth;
	xStream << xMeta.uGridHeight;
	xStream << xMeta.uNumColors;
	xStream << xMeta.uNumCatsPerColor;
	xStream << xMeta.uNumDraggableShapes;
	xStream << xMeta.uNumStaticBlockers;
	xStream << xMeta.uNumBlockerCats;
	xStream << xMeta.uNumConditionalShapes;
	xStream << xMeta.uParMoves;
	xStream << xMeta.uDifficultyTier;
	xStream << xMeta.ulLayoutHash;

	// v2 fields: registry matching
	xStream << xMeta.uMaxShapeComplexity;
	xStream << xMeta.uMinShapeComplexity;
	xStream << xMeta.uMaxConditionalThreshold;
	xStream << xMeta.uNumShapesPerColor;
	xStream << xMeta.uTotalCats;
	xStream << xMeta.uNumFloorCells;

	// v2 fields: generation provenance
	xStream << xMeta.ulGenerationTimestamp;
	xStream << xMeta.uGenerationSeed;
	xStream << xMeta.uGenerationTimeMs;
	xStream << xMeta.uGenerationRetryRounds;
	xStream << xMeta.uGenerationTotalAttempts;
	xStream << xMeta.bGenerationTimedOut;

	// Write standard level data (includes TPLV magic)
	TilePuzzleLevelSerialize::Write(xStream, xLevel);

	xStream.WriteToFile(szPath);
}

/**
 * ReadMetadataFromFile - Read v2 metadata header from a .tlvl file
 *
 * @return false if file lacks META header or is not v2
 */
static bool ReadMetadataFromFile(const char* szPath, TilePuzzleLevelMetadata& xMetaOut)
{
	Zenith_DataStream xStream;
	xStream.ReadFromFile(szPath);
	if (!xStream.IsValid())
		return false;

	uint32_t uMagic;
	xStream >> uMagic;
	if (uMagic != s_uMETA_MAGIC)
		return false;

	uint32_t uMetaVersion;
	xStream >> uMetaVersion;
	if (uMetaVersion != s_uMETA_VERSION)
		return false;

	xStream >> xMetaOut.uLevelNumber;
	xStream >> xMetaOut.uGridWidth;
	xStream >> xMetaOut.uGridHeight;
	xStream >> xMetaOut.uNumColors;
	xStream >> xMetaOut.uNumCatsPerColor;
	xStream >> xMetaOut.uNumDraggableShapes;
	xStream >> xMetaOut.uNumStaticBlockers;
	xStream >> xMetaOut.uNumBlockerCats;
	xStream >> xMetaOut.uNumConditionalShapes;
	xStream >> xMetaOut.uParMoves;
	xStream >> xMetaOut.uDifficultyTier;
	xStream >> xMetaOut.ulLayoutHash;
	xStream >> xMetaOut.uMaxShapeComplexity;
	xStream >> xMetaOut.uMinShapeComplexity;
	xStream >> xMetaOut.uMaxConditionalThreshold;
	xStream >> xMetaOut.uNumShapesPerColor;
	xStream >> xMetaOut.uTotalCats;
	xStream >> xMetaOut.uNumFloorCells;
	xStream >> xMetaOut.ulGenerationTimestamp;
	xStream >> xMetaOut.uGenerationSeed;
	xStream >> xMetaOut.uGenerationTimeMs;
	xStream >> xMetaOut.uGenerationRetryRounds;
	xStream >> xMetaOut.uGenerationTotalAttempts;
	xStream >> xMetaOut.bGenerationTimedOut;

	return true;
}

/**
 * BuildMetadata - Extract metadata from a generated level
 */
static TilePuzzleLevelMetadata BuildMetadata(
	uint32_t uLevelNumber,
	const TilePuzzleLevelData& xLevel,
	DifficultyTier eTier,
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
	xMeta.uDifficultyTier = static_cast<uint32_t>(eTier);
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

// ============================================================================
// DoesLevelMatchParams - Range-based matching for registry lookup
// ============================================================================

static bool DoesLevelMatchParams(
	const TilePuzzleLevelMetadata& xMeta,
	const TilePuzzle_LevelGenerator::DifficultyParams& xParams,
	uint32_t uMinMoves)
{
	if (xMeta.uGridWidth < xParams.uMinGridWidth || xMeta.uGridWidth > xParams.uMaxGridWidth)
		return false;
	if (xMeta.uGridHeight < xParams.uMinGridHeight || xMeta.uGridHeight > xParams.uMaxGridHeight)
		return false;
	if (xMeta.uNumColors < xParams.uMinNumColors || xMeta.uNumColors > xParams.uNumColors)
		return false;
	if (xMeta.uNumCatsPerColor < xParams.uMinCatsPerColor || xMeta.uNumCatsPerColor > xParams.uNumCatsPerColor)
		return false;
	if (xMeta.uNumShapesPerColor != xParams.uNumShapesPerColor)
		return false;
	if (xMeta.uNumStaticBlockers < xParams.uMinBlockers || xMeta.uNumStaticBlockers > xParams.uNumBlockers)
		return false;
	if (xMeta.uNumBlockerCats < xParams.uMinBlockerCats || xMeta.uNumBlockerCats > xParams.uNumBlockerCats)
		return false;
	if (xMeta.uNumConditionalShapes < xParams.uMinConditionalShapes || xMeta.uNumConditionalShapes > xParams.uNumConditionalShapes)
		return false;
	if (xMeta.uMaxShapeComplexity < xParams.uMinMaxShapeSize || xMeta.uMaxShapeComplexity > xParams.uMaxShapeSize)
		return false;
	if (xMeta.uMinShapeComplexity < xParams.uMinMaxShapeSize)
		return false;
	if (xMeta.uMaxConditionalThreshold > xParams.uConditionalThreshold)
		return false;
	if (xMeta.uNumConditionalShapes > 0 && xMeta.uMaxConditionalThreshold < xParams.uMinConditionalThreshold)
		return false;
	if (xMeta.uParMoves < uMinMoves)
		return false;
	return true;
}

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
	std::vector<RegistryEntry> axEntriesPerTier[DIFFICULTY_TIER_COUNT];
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

		for (uint32_t uTier = 0; uTier < DIFFICULTY_TIER_COUNT; ++uTier)
		{
			std::string strTierDir = strRegistryDir + "/" + DifficultyTierToString(static_cast<DifficultyTier>(uTier));

			if (!std::filesystem::exists(strTierDir))
				continue;

			// Collect all .tlvl files and their metadata
			std::unordered_map<uint64_t, std::vector<size_t>> xHashToIndices;

			for (auto& xEntry : std::filesystem::directory_iterator(strTierDir))
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

				if (xMeta.uDifficultyTier != uTier)
				{
					printf("  WARNING: Tier mismatch in %s (meta=%u, dir=%u)\n",
						strPath.c_str(), xMeta.uDifficultyTier, uTier);
					continue;
				}

				size_t uIdx = axEntriesPerTier[uTier].size();
				axEntriesPerTier[uTier].push_back({strPath, xMeta});
				xHashToIndices[xMeta.ulLayoutHash].push_back(uIdx);
			}

			// Deduplicate within this tier
			DeduplicateTier(uTier, xHashToIndices);
		}

		uTotalEntries = 0;
		for (uint32_t t = 0; t < DIFFICULTY_TIER_COUNT; ++t)
			uTotalEntries += static_cast<uint32_t>(axEntriesPerTier[t].size());

		printf("  Registry loaded: %u entries across %u tiers", uTotalEntries, DIFFICULTY_TIER_COUNT);
		if (uDuplicatesRemoved > 0)
			printf(", %u duplicates removed", uDuplicatesRemoved);
		printf("\n");
		for (uint32_t t = 0; t < DIFFICULTY_TIER_COUNT; ++t)
		{
			if (!axEntriesPerTier[t].empty())
			{
				printf("    %s: %zu levels\n",
					DifficultyTierToString(static_cast<DifficultyTier>(t)),
					axEntriesPerTier[t].size());
			}
		}
		printf("\n");
		fflush(stdout);
	}

	const RegistryEntry* FindMatch(
		DifficultyTier eTier,
		const TilePuzzle_LevelGenerator::DifficultyParams& xParams,
		uint32_t uMinMoves,
		const DuplicateDetector& xDuplicateDetector) const
	{
		if (eTier >= DIFFICULTY_TIER_COUNT)
			return nullptr;

		const auto& axEntries = axEntriesPerTier[eTier];
		for (size_t i = 0; i < axEntries.size(); ++i)
		{
			const RegistryEntry& xEntry = axEntries[i];

			if (xConsumedPaths.count(xEntry.strFilePath))
				continue;

			if (!DoesLevelMatchParams(xEntry.xMetadata, xParams, uMinMoves))
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
	void DeduplicateTier(
		uint32_t uTier,
		const std::unordered_map<uint64_t, std::vector<size_t>>& xHashToIndices)
	{
		std::unordered_set<size_t> xIndicesToRemove;

		for (auto& xPair : xHashToIndices)
		{
			if (xPair.second.size() <= 1)
				continue;

			// Multiple entries with same hash - need full structural comparison
			struct LoadedEntry
			{
				size_t uOrigIndex;
				LevelLayoutSignature xSig;
			};
			std::vector<LoadedEntry> axLoaded;

			for (size_t idx : xPair.second)
			{
				const RegistryEntry& xRegEntry = axEntriesPerTier[uTier][idx];

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

			// Compare all pairs, mark duplicates for removal
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
						// True duplicate - delete the second one
						xIndicesToRemove.insert(axLoaded[j].uOrigIndex);

						const std::string& strPath = axEntriesPerTier[uTier][axLoaded[j].uOrigIndex].strFilePath;
						std::filesystem::remove(strPath);
						// Also remove companion .png
						std::string strPngPath = strPath.substr(0, strPath.size() - 5) + ".png";
						std::filesystem::remove(strPngPath);
						printf("  Removed duplicate: %s\n", strPath.c_str());
						uDuplicatesRemoved++;
					}
				}
			}
		}

		// Remove marked entries from vector
		if (!xIndicesToRemove.empty())
		{
			std::vector<RegistryEntry> axFiltered;
			for (size_t i = 0; i < axEntriesPerTier[uTier].size(); ++i)
			{
				if (!xIndicesToRemove.count(i))
					axFiltered.push_back(std::move(axEntriesPerTier[uTier][i]));
			}
			axEntriesPerTier[uTier] = std::move(axFiltered);
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
	DifficultyTier eTier = static_cast<DifficultyTier>(xMeta.uDifficultyTier);
	if (eTier >= DIFFICULTY_TIER_COUNT)
		return;

	std::string strTierDir = strRegistryDir + "/" + DifficultyTierToString(eTier);
	std::filesystem::create_directories(strTierDir);

	char szFileName[128];
	snprintf(szFileName, sizeof(szFileName), "%016llx_%u.tlvl",
		static_cast<unsigned long long>(xMeta.ulLayoutHash), xMeta.uParMoves);

	std::string strRegistryTlvl = strTierDir + "/" + szFileName;

	if (std::filesystem::exists(strRegistryTlvl))
		return;

	WriteMetadataAndLevel(strRegistryTlvl.c_str(), xMeta, xLevel);

	// Write companion .png
	std::string strRegistryPng = strRegistryTlvl.substr(0, strRegistryTlvl.size() - 5) + ".png";
	TilePuzzleLevelImage::Write(strRegistryPng.c_str(), xLevel);

	RegistryEntry xEntry;
	xEntry.strFilePath = strRegistryTlvl;
	xEntry.xMetadata = xMeta;
	xRegistry.axEntriesPerTier[eTier].push_back(std::move(xEntry));
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
// Level Validation (per-tier)
// ============================================================================

/**
 * IsLevelValidForTier - Validates a level based on its difficulty tier
 *
 * Tutorial/Easy tiers have looser requirements. Higher tiers require
 * more complex shapes and specific blocker constraints.
 */
static bool IsLevelValidForTier(const TilePuzzleLevelData& xLevel, DifficultyTier eTier)
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

	// Blocker cell limit scales with tier
	uint32_t uMaxBlockerCells = 3;
	if (eTier >= DIFFICULTY_TIER_HARD) uMaxBlockerCells = 6;
	if (eTier >= DIFFICULTY_TIER_EXPERT) uMaxBlockerCells = 8;

	// Tutorial and Easy tiers: just need draggable shapes, no complexity requirement
	if (eTier <= DIFFICULTY_TIER_EASY)
	{
		return uDraggableShapes >= 1 && uTotalBlockerCells <= uMaxBlockerCells;
	}

	// Medium and above: require at least one complex draggable shape
	return uDraggableShapes >= 2 && bHasDraggableComplex && uTotalBlockerCells <= uMaxBlockerCells;
}

// ============================================================================
// Daily Puzzle Helpers
// ============================================================================

/**
 * IsLeapYear - Check if a year is a leap year
 */
static bool IsLeapYear(uint32_t uYear)
{
	return (uYear % 4 == 0 && uYear % 100 != 0) || (uYear % 400 == 0);
}

/**
 * DaysInYear - Get number of days in a year
 */
static uint32_t DaysInYear(uint32_t uYear)
{
	return IsLeapYear(uYear) ? 366 : 365;
}

/**
 * DaysInMonth - Get number of days in a specific month
 */
static uint32_t DaysInMonth(uint32_t uYear, uint32_t uMonth)
{
	static const uint32_t auDays[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
	if (uMonth < 1 || uMonth > 12)
		return 0;
	uint32_t uDays = auDays[uMonth - 1];
	if (uMonth == 2 && IsLeapYear(uYear))
		uDays = 29;
	return uDays;
}

/**
 * DayOfYearToDate - Convert day-of-year (1-based) to YYYYMMDD
 */
static uint32_t DayOfYearToDate(uint32_t uYear, uint32_t uDayOfYear)
{
	uint32_t uMonth = 1;
	uint32_t uDaysRemaining = uDayOfYear;
	while (uMonth <= 12)
	{
		uint32_t uDaysThisMonth = DaysInMonth(uYear, uMonth);
		if (uDaysRemaining <= uDaysThisMonth)
			break;
		uDaysRemaining -= uDaysThisMonth;
		uMonth++;
	}
	return uYear * 10000 + uMonth * 100 + uDaysRemaining;
}

// ============================================================================
// Core Generation Loop (shared between modes)
// ============================================================================

/**
 * GenerateSingleLevel - Generate a single level with retry loop
 *
 * @param xBestLevelOut     Output: best generated level data
 * @param axBestLevelDefs   Output: owned shape definitions for the best level
 * @param xLevelParams      Difficulty parameters to use
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
	const TilePuzzle_LevelGenerator::DifficultyParams& xLevelParams,
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
	uint32_t uStartSeed = 0;
	GenerationMode eMode = GENMODE_BATCH;
	uint32_t uBatchStart = 0;
	uint32_t uBatchEnd = 0;
	bool bPresetV1 = false;
	uint32_t uDailyYear = 0;

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
		else if (strcmp(argv[i], "--batch") == 0 && i + 1 < argc)
		{
			eMode = GENMODE_BATCH;
			// Parse START-END format
			const char* pszRange = argv[++i];
			if (sscanf(pszRange, "%u-%u", &uBatchStart, &uBatchEnd) != 2)
			{
				fprintf(stderr, "Error: --batch requires START-END format (e.g., --batch 1-100)\n");
				return 1;
			}
			if (uBatchStart == 0 || uBatchEnd == 0 || uBatchStart > uBatchEnd)
			{
				fprintf(stderr, "Error: --batch range must be 1-based and START <= END\n");
				return 1;
			}
		}
		else if (strcmp(argv[i], "--preset") == 0 && i + 1 < argc)
		{
			const char* pszPreset = argv[++i];
			if (strcmp(pszPreset, "v1") == 0)
			{
				bPresetV1 = true;
			}
			else
			{
				fprintf(stderr, "Error: Unknown preset '%s' (available: v1)\n", pszPreset);
				return 1;
			}
		}
		else if (strcmp(argv[i], "--daily") == 0 && i + 1 < argc)
		{
			eMode = GENMODE_DAILY;
			uDailyYear = static_cast<uint32_t>(atoi(argv[++i]));
			if (uDailyYear < 2000 || uDailyYear > 9999)
			{
				fprintf(stderr, "Error: --daily requires a 4-digit year (e.g., --daily 2026)\n");
				return 1;
			}
		}
	}

	// Validate that a mode was specified
	if (eMode == GENMODE_BATCH && uBatchStart == 0 && uBatchEnd == 0)
	{
		fprintf(stderr, "Error: No generation mode specified. Use --batch START-END or --daily YEAR.\n");
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
	// Mode-specific setup and execution
	// ========================================================================

	if (eMode == GENMODE_BATCH)
	{
		// --batch mode: generate levels with preset difficulty curve
		printf("Mode: Batch Generation (levels %u-%u)\n", uBatchStart, uBatchEnd);
		if (bPresetV1)
			printf("Preset: V1 difficulty curve\n");
		else
			printf("Preset: None (using default params)\n");
		printf("Timeout per level: %us, Seed: %u\n\n", uTimeoutSeconds, uStartSeed);
		fflush(stdout);

		// Create output directory (no Run subdirectory for batch mode)
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

		for (uint32_t uLevel = uBatchStart; uLevel <= uBatchEnd && s_bRunning; ++uLevel)
		{
			DifficultyTier eTier = bPresetV1 ? GetDifficultyTierForLevel(uLevel) : DIFFICULTY_TIER_MEDIUM;
			TilePuzzle_LevelGenerator::DifficultyParams xLevelParams = bPresetV1
				? GetDifficultyPresetV1(uLevel)
				: TilePuzzle_LevelGenerator::GetDifficultyForLevel(uLevel);

			uint32_t uLevelMinMoves = GetMinMovesForTier(eTier);
			uint32_t uLevelTimeout = uTimeoutSeconds;

			// For lower tiers, use shorter timeout since they should generate quickly
			if (bPresetV1 && eTier <= DIFFICULTY_TIER_EASY)
				uLevelTimeout = std::min(uTimeoutSeconds, 300u); // 5 min max for easy levels

			printf("Generating level %u [%s, tier=%s]...",
				uLevel,
				bPresetV1 ? "v1" : "default",
				DifficultyTierToString(eTier));
			fflush(stdout);

			// Registry lookup
			const RegistryEntry* pxMatch = xRegistry.FindMatch(eTier, xLevelParams, uLevelMinMoves, xDuplicateDetector);
			if (pxMatch)
			{
				TilePuzzleLevelData xCachedLevel = {};
				std::vector<TilePuzzleShapeDefinition> axCachedDefs;
				if (CopyRegistryLevelToDestination(*pxMatch, strOutputDir, uLevel, xCachedLevel, axCachedDefs))
				{
					uint64_t ulHash = pxMatch->xMetadata.ulLayoutHash;
					xDuplicateDetector.RegisterLevel(xCachedLevel, ulHash);
					xRegistry.MarkConsumed(*pxMatch);
					xRegistry.uCacheHits++;

					printf(" CACHE HIT (registry, solver=%u moves, tier=%s)\n",
						pxMatch->xMetadata.uParMoves,
						DifficultyTierToString(eTier));
					fflush(stdout);

					TilePuzzleLevelGenAnalytics::AccumulateLevel(xRunStats, true, pxMatch->xMetadata.uParMoves, 0.0, nullptr, 0, false);
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
					xBestLevel, axBestLevelDefs, xLevelParams,
					uLevelMinMoves, uLevelTimeout,
					uGlobalSeedCounter, uLevel,
					xAccumulatedStats, uRetryRounds, bTimedOut,
					&IsLevelValidForTier, eTier);

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
					TilePuzzleLevelMetadata xMeta = BuildMetadata(uLevel, xBestLevel, eTier, ulLayoutHash,
						uGlobalSeedCounter, fTimeMs, uRetryRounds, &xAccumulatedStats, bTimedOut);

					// Build output file paths
					char szBaseName[64];
					snprintf(szBaseName, sizeof(szBaseName), "level_%04u", uLevel);

					std::string strTlvlPath = strOutputDir + "/" + szBaseName + ".tlvl";
					std::string strPngPath = strOutputDir + "/" + szBaseName + ".png";

					// Write binary with metadata header
					WriteMetadataAndLevel(strTlvlPath.c_str(), xMeta, xBestLevel);

					// Write PNG
					TilePuzzleLevelImage::Write(strPngPath.c_str(), xBestLevel);

					// Register in level registry
					RegisterLevelInRegistry(strRegistryDir, xMeta, xBestLevel, xRegistry);

					printf(" OK (solver=%u moves, tier=%s, %.1fs, %u rounds%s)\n",
						uBestMoves,
						DifficultyTierToString(eTier),
						fTimeMs / 1000.0, uRetryRounds,
						bTimedOut ? " TIMEOUT" : "");

					// Accumulate stats
					TilePuzzleLevelGenAnalytics::AccumulateLevel(xRunStats, true, uBestMoves, fTimeMs, &xAccumulatedStats, uRetryRounds, bTimedOut);
				}
				else
				{
					printf(" FAILED (%.1fs, %u rounds%s)\n",
						fTimeMs / 1000.0, uRetryRounds,
						bTimedOut ? " TIMEOUT" : "");
					TilePuzzleLevelGenAnalytics::AccumulateLevel(xRunStats, false, 0, fTimeMs, &xAccumulatedStats, uRetryRounds, bTimedOut);
					break; // Don't retry on complete generation failure
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
	}
	else if (eMode == GENMODE_DAILY)
	{
		// --daily mode: generate daily puzzles for an entire year
		uint32_t uNumDays = DaysInYear(uDailyYear);
		printf("Mode: Daily Puzzle Generation (%u, %u days)\n", uDailyYear, uNumDays);
		printf("Difficulty: Mid-range (equivalent to levels 50-70)\n");
		printf("Timeout per level: %us\n\n", uTimeoutSeconds);
		fflush(stdout);

		// Create output directory
		std::filesystem::create_directories(strOutputDir);

		printf("Output directory: %s\n\n", strOutputDir.c_str());
		fflush(stdout);

		DuplicateDetector xDuplicateDetector;
		TilePuzzleLevelGenAnalytics::RunStats xRunStats = {};

		// Registry initialization
		std::string strRegistryDir = LEVELGEN_REGISTRY_DIR;
		LevelRegistry xRegistry;
		xRegistry.ScanAndLoad(strRegistryDir);

		for (uint32_t uDay = 1; uDay <= uNumDays && s_bRunning; ++uDay)
		{
			uint32_t uDate = DayOfYearToDate(uDailyYear, uDay);

			// Seed = YYYYMMDD for deterministic daily puzzles
			uint32_t uDailySeed = uDate;

			// Use mid-range difficulty (levels 50-70 equivalent = Hard tier)
			// Vary within the range based on day of year
			uint32_t uEquivalentLevel = 50 + (uDay % 21); // 50-70
			DifficultyTier eTier = GetDifficultyTierForLevel(uEquivalentLevel);
			TilePuzzle_LevelGenerator::DifficultyParams xDailyParams = GetDifficultyPresetV1(uEquivalentLevel);

			uint32_t uDailyMinMoves = GetMinMovesForTier(eTier);

			printf("Generating daily %u (day %u/%u) [tier=%s]...",
				uDate, uDay, uNumDays, DifficultyTierToString(eTier));
			fflush(stdout);

			// Registry lookup
			const RegistryEntry* pxMatch = xRegistry.FindMatch(eTier, xDailyParams, uDailyMinMoves, xDuplicateDetector);
			if (pxMatch)
			{
				TilePuzzleLevelData xCachedLevel = {};
				std::vector<TilePuzzleShapeDefinition> axCachedDefs;

				// Build daily output path
				char szBaseName[64];
				snprintf(szBaseName, sizeof(szBaseName), "daily_%08u", uDate);
				std::string strDailyOutputDir = strOutputDir;

				// Copy using daily filename
				std::string strDestTlvl = strDailyOutputDir + "/" + szBaseName + ".tlvl";
				std::string strDestPng = strDailyOutputDir + "/" + szBaseName + ".png";

				std::error_code xEc;
				std::filesystem::copy_file(
					pxMatch->strFilePath, strDestTlvl,
					std::filesystem::copy_options::overwrite_existing, xEc);

				if (!xEc)
				{
					// Copy .png
					std::string strRegistryPng = pxMatch->strFilePath.substr(0, pxMatch->strFilePath.size() - 5) + ".png";
					std::filesystem::copy_file(
						strRegistryPng, strDestPng,
						std::filesystem::copy_options::overwrite_existing, xEc);

					// Load for duplicate detector
					Zenith_DataStream xStream;
					xStream.ReadFromFile(pxMatch->strFilePath.c_str());
					if (xStream.IsValid())
					{
						Zenith_Vector<TilePuzzleShapeDefinition> axEngineDefs;
						if (TilePuzzleLevelSerialize::Read(xStream, xCachedLevel, axEngineDefs))
						{
							uint64_t ulHash = pxMatch->xMetadata.ulLayoutHash;
							xDuplicateDetector.RegisterLevel(xCachedLevel, ulHash);
							xRegistry.MarkConsumed(*pxMatch);
							xRegistry.uCacheHits++;

							printf(" CACHE HIT (registry, solver=%u moves, tier=%s)\n",
								pxMatch->xMetadata.uParMoves,
								DifficultyTierToString(eTier));
							fflush(stdout);

							TilePuzzleLevelGenAnalytics::AccumulateLevel(xRunStats, true, pxMatch->xMetadata.uParMoves, 0.0, nullptr, 0, false);
							continue;
						}
					}
				}
			}
			xRegistry.uCacheMisses++;

			auto xStartTime = std::chrono::high_resolution_clock::now();

			TilePuzzleLevelData xBestLevel = {};
			std::vector<TilePuzzleShapeDefinition> axBestLevelDefs;
			TilePuzzle_LevelGenerator::GenerationStats xAccumulatedStats = {};
			uint32_t uRetryRounds = 0;
			bool bTimedOut = false;

			uint32_t uSeedCounter = uDailySeed;

			uint32_t uBestMoves = GenerateSingleLevel(
				xBestLevel, axBestLevelDefs, xDailyParams,
				uDailyMinMoves, uTimeoutSeconds,
				uSeedCounter, uDate,
				xAccumulatedStats, uRetryRounds, bTimedOut,
				&IsLevelValidForTier, eTier);

			auto xEndTime = std::chrono::high_resolution_clock::now();
			double fTimeMs = std::chrono::duration<double, std::milli>(xEndTime - xStartTime).count();

			if (uBestMoves > 0)
			{
				uint64_t ulLayoutHash = ComputeLayoutHash(xBestLevel);

				// Build metadata
				TilePuzzleLevelMetadata xMeta = BuildMetadata(uDate, xBestLevel, eTier, ulLayoutHash,
					uSeedCounter, fTimeMs, uRetryRounds, &xAccumulatedStats, bTimedOut);
				xDuplicateDetector.RegisterLevel(xBestLevel, ulLayoutHash);

				// Build output file paths
				char szBaseName[64];
				snprintf(szBaseName, sizeof(szBaseName), "daily_%08u", uDate);

				std::string strTlvlPath = strOutputDir + "/" + szBaseName + ".tlvl";
				std::string strPngPath = strOutputDir + "/" + szBaseName + ".png";

				// Write binary with metadata
				WriteMetadataAndLevel(strTlvlPath.c_str(), xMeta, xBestLevel);

				// Write PNG
				TilePuzzleLevelImage::Write(strPngPath.c_str(), xBestLevel);

				// Register in level registry
				RegisterLevelInRegistry(strRegistryDir, xMeta, xBestLevel, xRegistry);

				printf(" OK (solver=%u moves, %.1fs, %u rounds%s)\n",
					uBestMoves, fTimeMs / 1000.0, uRetryRounds,
					bTimedOut ? " TIMEOUT" : "");

				TilePuzzleLevelGenAnalytics::AccumulateLevel(xRunStats, true, uBestMoves, fTimeMs, &xAccumulatedStats, uRetryRounds, bTimedOut);
			}
			else
			{
				printf(" FAILED (%.1fs, %u rounds%s)\n",
					fTimeMs / 1000.0, uRetryRounds,
					bTimedOut ? " TIMEOUT" : "");
				TilePuzzleLevelGenAnalytics::AccumulateLevel(xRunStats, false, 0, fTimeMs, &xAccumulatedStats, uRetryRounds, bTimedOut);
			}
			fflush(stdout);
		}

		// Write analytics
		std::string strAnalyticsPath = strOutputDir + "/analytics.txt";
		TilePuzzleLevelGenAnalytics::WriteAnalytics(strAnalyticsPath.c_str(), xRunStats);
		printf("\nAnalytics written to: %s\n", strAnalyticsPath.c_str());

		printf("Registry: %u cache hits, %u cache misses, %u total entries\n",
			xRegistry.uCacheHits, xRegistry.uCacheMisses, xRegistry.uTotalEntries);
	}

	// Shutdown
	Zenith_TaskSystem::Shutdown();

	printf("Done.\n");
	return 0;
}
