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
};

// Overall game save data
struct TilePuzzleSaveData
{
	static constexpr uint32_t uMAX_LEVELS = 100;
	static constexpr uint32_t uGAME_SAVE_VERSION = 1;

	uint32_t uHighestLevelReached;
	uint32_t uCurrentLevel;
	TilePuzzleLevelRecord axLevelRecords[uMAX_LEVELS];

	void Reset()
	{
		uHighestLevelReached = 1;
		uCurrentLevel = 1;
		memset(axLevelRecords, 0, sizeof(axLevelRecords));
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
	}
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
		}
	}
}
