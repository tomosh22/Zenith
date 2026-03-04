#pragma once

#include "Components/TilePuzzleLevelData_Serialize.h"
#include <cstdint>

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
// Metadata I/O
// ============================================================================

/**
 * WriteMetadataAndLevel - Prepend metadata to the .tlvl file
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
