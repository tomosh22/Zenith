#pragma once

#include <cstdint>
#include <cstring>
#include "DataStream/Zenith_DataStream.h"

// Per-level completion record
struct TilePuzzleLevelRecord
{
	bool bCompleted;
	uint32_t uBestMoves;
	float fBestTime;
	uint32_t uBestStars;  // 0 = not rated, 1-3 = star rating
};

// Overall game save data
struct TilePuzzleSaveData
{
	static constexpr uint32_t uMAX_LEVELS = 100;
	static constexpr uint32_t uMAX_PINBALL_GATES = 10;
	static constexpr uint32_t uMAX_CATS = 100;
	static constexpr uint32_t uCAT_BITFIELD_BYTES = 16; // 128 bits, supports up to 128 cat IDs
	static constexpr uint32_t uMAX_LIVES = 5;
	static constexpr uint32_t uLIFE_REGEN_SECONDS = 1200; // 20 minutes
	static constexpr uint32_t uLIFE_REFILL_COST = 50;
	static constexpr uint32_t uGAME_SAVE_VERSION = 6;
	static constexpr uint32_t uTUTORIAL_COUNT = 6;

	// v1 fields
	uint32_t uHighestLevelReached;
	uint32_t uCurrentLevel;
	TilePuzzleLevelRecord axLevelRecords[uMAX_LEVELS];

	// v2 fields
	uint32_t uPinballScore;

	// v3 fields
	uint32_t uCoins;

	// v4 fields
	bool abPinballGateCleared[uMAX_PINBALL_GATES];

	// v5 fields
	uint8_t auStarRatings[uMAX_LEVELS];       // 0-3 stars per puzzle level
	uint32_t uTotalStars;                      // Cached sum of all stars
	uint8_t abCatsCollected[uCAT_BITFIELD_BYTES]; // 128-bit bitfield (cat ID -> bit)
	uint32_t uCatsCollectedCount;              // Cached count
	uint32_t uDailyStreak;                     // Consecutive login days
	uint32_t uLastDailyDate;                   // YYYYMMDD format
	uint16_t uPinballGateFlags;                // Bit per gate (10 gates cleared)
	uint32_t uDailyPuzzleBestMoves;            // Today's daily puzzle best
	uint32_t uLastDailyPuzzleDate;             // YYYYMMDD of last daily attempt
	uint32_t uLives;                           // Current lives count
	uint32_t uLastLifeRegenTime;               // Timestamp for life regeneration

	// v6 fields
	bool abTutorialShown[uTUTORIAL_COUNT];     // Tutorial overlays shown (level 1,6,11,26,46,+spare)
	bool bSoundEnabled;
	bool bMusicEnabled;
	bool bHapticsEnabled;

	void Reset()
	{
		uHighestLevelReached = 1;
		uCurrentLevel = 1;
		memset(axLevelRecords, 0, sizeof(axLevelRecords));
		uPinballScore = 0;
		uCoins = 0;
		memset(abPinballGateCleared, 0, sizeof(abPinballGateCleared));
		memset(auStarRatings, 0, sizeof(auStarRatings));
		uTotalStars = 0;
		memset(abCatsCollected, 0, sizeof(abCatsCollected));
		uCatsCollectedCount = 0;
		uDailyStreak = 0;
		uLastDailyDate = 0;
		uPinballGateFlags = 0;
		uDailyPuzzleBestMoves = 0;
		uLastDailyPuzzleDate = 0;
		uLives = uMAX_LIVES;
		uLastLifeRegenTime = 0;
		memset(abTutorialShown, 0, sizeof(abTutorialShown));
		bSoundEnabled = true;
		bMusicEnabled = true;
		bHapticsEnabled = true;
	}

	// ========================================================================
	// Coin Management
	// ========================================================================

	void AddCoins(int32_t iAmount)
	{
		if (iAmount < 0 && static_cast<uint32_t>(-iAmount) > uCoins)
		{
			uCoins = 0;
		}
		else
		{
			uCoins = static_cast<uint32_t>(static_cast<int32_t>(uCoins) + iAmount);
		}
	}

	bool SpendCoins(uint32_t uAmount)
	{
		if (uCoins < uAmount)
		{
			return false;
		}
		uCoins -= uAmount;
		return true;
	}

	// ========================================================================
	// Star Rating Management
	// ========================================================================

	void SetStarRating(uint32_t uLevel, uint8_t uStars)
	{
		if (uLevel == 0 || uLevel > uMAX_LEVELS)
			return;
		uint32_t uIndex = uLevel - 1;
		if (uStars > auStarRatings[uIndex])
		{
			uTotalStars -= auStarRatings[uIndex];
			auStarRatings[uIndex] = uStars;
			uTotalStars += uStars;
		}
	}

	uint8_t GetStarRating(uint32_t uLevel) const
	{
		if (uLevel == 0 || uLevel > uMAX_LEVELS)
			return 0;
		return auStarRatings[uLevel - 1];
	}

	// ========================================================================
	// Cat Collection (bitfield operations)
	// ========================================================================

	void CollectCat(uint32_t uCatID)
	{
		if (uCatID >= uCAT_BITFIELD_BYTES * 8)
			return;
		uint32_t uByteIndex = uCatID / 8;
		uint8_t uBitMask = 1u << (uCatID % 8);
		if (!(abCatsCollected[uByteIndex] & uBitMask))
		{
			abCatsCollected[uByteIndex] |= uBitMask;
			uCatsCollectedCount++;
		}
	}

	bool IsCatCollected(uint32_t uCatID) const
	{
		if (uCatID >= uCAT_BITFIELD_BYTES * 8)
			return false;
		uint32_t uByteIndex = uCatID / 8;
		uint8_t uBitMask = 1u << (uCatID % 8);
		return (abCatsCollected[uByteIndex] & uBitMask) != 0;
	}

	// ========================================================================
	// Pinball Gate Flags (bitfield operations)
	// ========================================================================

	void SetPinballGateCleared(uint32_t uGate)
	{
		if (uGate >= uMAX_PINBALL_GATES)
			return;
		uPinballGateFlags |= (1u << uGate);
		abPinballGateCleared[uGate] = true;
	}

	bool IsPinballGateCleared(uint32_t uGate) const
	{
		if (uGate >= uMAX_PINBALL_GATES)
			return false;
		return (uPinballGateFlags & (1u << uGate)) != 0;
	}

	// ========================================================================
	// Lives Management
	// ========================================================================

	void LoseLife()
	{
		if (uLives > 0)
		{
			uLives--;
		}
	}

	bool HasLives() const
	{
		return uLives > 0;
	}

	void RefillLives()
	{
		uLives = uMAX_LIVES;
	}

	bool TryRefillLivesWithCoins()
	{
		if (!SpendCoins(uLIFE_REFILL_COST))
			return false;
		RefillLives();
		return true;
	}

	void RegenerateLives(uint32_t uCurrentTime)
	{
		if (uLives >= uMAX_LIVES)
		{
			uLastLifeRegenTime = uCurrentTime;
			return;
		}

		if (uLastLifeRegenTime == 0)
		{
			uLastLifeRegenTime = uCurrentTime;
			return;
		}

		uint32_t uElapsed = uCurrentTime - uLastLifeRegenTime;
		uint32_t uLivesToRegen = uElapsed / uLIFE_REGEN_SECONDS;
		if (uLivesToRegen > 0)
		{
			uLives += uLivesToRegen;
			if (uLives > uMAX_LIVES)
				uLives = uMAX_LIVES;
			uLastLifeRegenTime += uLivesToRegen * uLIFE_REGEN_SECONDS;
		}
	}

	uint32_t GetSecondsUntilNextLife(uint32_t uCurrentTime) const
	{
		if (uLives >= uMAX_LIVES)
			return 0;
		if (uLastLifeRegenTime == 0)
			return uLIFE_REGEN_SECONDS;
		uint32_t uElapsed = uCurrentTime - uLastLifeRegenTime;
		if (uElapsed >= uLIFE_REGEN_SECONDS)
			return 0;
		return uLIFE_REGEN_SECONDS - uElapsed;
	}

	// ========================================================================
	// Tutorial Tracking
	// ========================================================================

	void SetTutorialShown(uint32_t uIndex)
	{
		if (uIndex < uTUTORIAL_COUNT)
			abTutorialShown[uIndex] = true;
	}

	bool IsTutorialShown(uint32_t uIndex) const
	{
		if (uIndex >= uTUTORIAL_COUNT)
			return true;
		return abTutorialShown[uIndex];
	}

	// ========================================================================
	// Daily Streak
	// ========================================================================

	void UpdateDailyStreak(uint32_t uTodayDate)
	{
		if (uLastDailyDate == 0)
		{
			uDailyStreak = 1;
			uLastDailyDate = uTodayDate;
			return;
		}

		// Check if consecutive day (simple YYYYMMDD check)
		// This is an approximation - doesn't handle month boundaries perfectly
		// but is sufficient for gameplay purposes
		if (uTodayDate == uLastDailyDate)
		{
			return; // Already logged today
		}

		// Handle month rollover approximately
		uint32_t uLastDay = uLastDailyDate % 100;
		uint32_t uLastMonth = (uLastDailyDate / 100) % 100;
		uint32_t uTodayDay = uTodayDate % 100;
		uint32_t uTodayMonth = (uTodayDate / 100) % 100;

		bool bConsecutive = false;
		if (uTodayDay == uLastDay + 1 && uTodayMonth == uLastMonth)
		{
			bConsecutive = true;
		}
		else if (uTodayDay == 1 && uTodayMonth == uLastMonth + 1)
		{
			bConsecutive = true; // First of new month after last day of prev month
		}

		if (bConsecutive)
		{
			uDailyStreak++;
		}
		else
		{
			uDailyStreak = 1; // Reset streak
		}
		uLastDailyDate = uTodayDate;
	}

	// ========================================================================
	// Utility: Recalculate cached values
	// ========================================================================

	void RecalculateCachedValues()
	{
		uTotalStars = 0;
		for (uint32_t i = 0; i < uMAX_LEVELS; ++i)
		{
			uTotalStars += auStarRatings[i];
		}

		uCatsCollectedCount = 0;
		for (uint32_t i = 0; i < uCAT_BITFIELD_BYTES; ++i)
		{
			uint8_t uByte = abCatsCollected[i];
			while (uByte)
			{
				uCatsCollectedCount += (uByte & 1);
				uByte >>= 1;
			}
		}
	}
};

// Static write callback for Zenith_SaveData
static void TilePuzzle_WriteSaveData(Zenith_DataStream& xStream, void* pxUserData)
{
	TilePuzzleSaveData* pxData = static_cast<TilePuzzleSaveData*>(pxUserData);
	xStream << pxData->uHighestLevelReached;
	xStream << pxData->uCurrentLevel;
	for (uint32_t i = 0; i < TilePuzzleSaveData::uMAX_LEVELS; ++i)
	{
		xStream << pxData->axLevelRecords[i].bCompleted;
		xStream << pxData->axLevelRecords[i].uBestMoves;
		xStream << pxData->axLevelRecords[i].fBestTime;
		xStream << pxData->axLevelRecords[i].uBestStars;
	}
	xStream << pxData->uPinballScore;
	xStream << pxData->uCoins;
	for (uint32_t i = 0; i < TilePuzzleSaveData::uMAX_PINBALL_GATES; ++i)
	{
		xStream << pxData->abPinballGateCleared[i];
	}
	// v5 fields
	for (uint32_t i = 0; i < TilePuzzleSaveData::uMAX_LEVELS; ++i)
	{
		xStream << pxData->auStarRatings[i];
	}
	xStream << pxData->uTotalStars;
	for (uint32_t i = 0; i < TilePuzzleSaveData::uCAT_BITFIELD_BYTES; ++i)
	{
		xStream << pxData->abCatsCollected[i];
	}
	xStream << pxData->uCatsCollectedCount;
	xStream << pxData->uDailyStreak;
	xStream << pxData->uLastDailyDate;
	xStream << pxData->uPinballGateFlags;
	xStream << pxData->uDailyPuzzleBestMoves;
	xStream << pxData->uLastDailyPuzzleDate;
	xStream << pxData->uLives;
	xStream << pxData->uLastLifeRegenTime;
	// v6 fields
	for (uint32_t i = 0; i < TilePuzzleSaveData::uTUTORIAL_COUNT; ++i)
	{
		xStream << pxData->abTutorialShown[i];
	}
	xStream << pxData->bSoundEnabled;
	xStream << pxData->bMusicEnabled;
	xStream << pxData->bHapticsEnabled;
}

// Static read callback for Zenith_SaveData
static void TilePuzzle_ReadSaveData(Zenith_DataStream& xStream, uint32_t uGameVersion, void* pxUserData)
{
	TilePuzzleSaveData* pxData = static_cast<TilePuzzleSaveData*>(pxUserData);
	pxData->Reset();
	if (uGameVersion >= 1)
	{
		xStream >> pxData->uHighestLevelReached;
		xStream >> pxData->uCurrentLevel;
		for (uint32_t i = 0; i < TilePuzzleSaveData::uMAX_LEVELS; ++i)
		{
			xStream >> pxData->axLevelRecords[i].bCompleted;
			xStream >> pxData->axLevelRecords[i].uBestMoves;
			xStream >> pxData->axLevelRecords[i].fBestTime;
			if (uGameVersion >= 3)
			{
				xStream >> pxData->axLevelRecords[i].uBestStars;
			}
		}
	}
	if (uGameVersion >= 2)
	{
		xStream >> pxData->uPinballScore;
	}
	if (uGameVersion >= 3)
	{
		xStream >> pxData->uCoins;
	}
	if (uGameVersion >= 4)
	{
		for (uint32_t i = 0; i < TilePuzzleSaveData::uMAX_PINBALL_GATES; ++i)
		{
			xStream >> pxData->abPinballGateCleared[i];
		}
	}
	if (uGameVersion >= 5)
	{
		for (uint32_t i = 0; i < TilePuzzleSaveData::uMAX_LEVELS; ++i)
		{
			xStream >> pxData->auStarRatings[i];
		}
		xStream >> pxData->uTotalStars;
		for (uint32_t i = 0; i < TilePuzzleSaveData::uCAT_BITFIELD_BYTES; ++i)
		{
			xStream >> pxData->abCatsCollected[i];
		}
		xStream >> pxData->uCatsCollectedCount;
		xStream >> pxData->uDailyStreak;
		xStream >> pxData->uLastDailyDate;
		xStream >> pxData->uPinballGateFlags;
		xStream >> pxData->uDailyPuzzleBestMoves;
		xStream >> pxData->uLastDailyPuzzleDate;
		xStream >> pxData->uLives;
		xStream >> pxData->uLastLifeRegenTime;
	}
	if (uGameVersion >= 6)
	{
		for (uint32_t i = 0; i < TilePuzzleSaveData::uTUTORIAL_COUNT; ++i)
		{
			xStream >> pxData->abTutorialShown[i];
		}
		xStream >> pxData->bSoundEnabled;
		xStream >> pxData->bMusicEnabled;
		xStream >> pxData->bHapticsEnabled;
	}
}
