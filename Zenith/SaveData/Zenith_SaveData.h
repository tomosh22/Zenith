#pragma once

#include "DataStream/Zenith_DataStream.h"
#include "FileAccess/Zenith_FileAccess.h"

#define ZENITH_SAVE_EXT ".zsave"

// Magic number: "ZENS" = 0x5A454E53 (Zenith Save)
static constexpr uint32_t uZENITH_SAVE_MAGIC = 0x5A454E53;

// Current file format version (bump when header layout changes)
static constexpr uint32_t uZENITH_SAVE_FORMAT_VERSION = 1;

// Save file header (written at start of every save file)
struct Zenith_SaveFileHeader
{
	uint32_t uMagic;
	uint32_t uFormatVersion;
	uint32_t uGameVersion;
	uint32_t uChecksum;
	uint64_t ulPayloadSize;
	uint64_t ulTimestamp;
};

namespace Zenith_SaveData
{
	// Initialize the save system. Must be called once at startup.
	// Determines platform-specific writable save directory.
	// szGameName: used to create a per-game subdirectory (e.g. "TilePuzzle")
	void Initialise(const char* szGameName);

	// Get the platform-specific save directory path (ends with /)
	// Windows: %APPDATA%/Zenith/<GameName>/
	// Android: <internal storage>/Zenith/<GameName>/
	const char* GetSaveDirectory();

	// Callback for writing game-specific data into a DataStream
	typedef void(*SaveWriteCallback)(Zenith_DataStream& xStream, void* pxUserData);

	// Callback for reading game-specific data from a DataStream
	// uGameVersion is the version stored in the save file header (for data migration)
	typedef void(*SaveReadCallback)(Zenith_DataStream& xStream, uint32_t uGameVersion, void* pxUserData);

	// Save game data to a named slot
	// szSlotName: e.g. "autosave", "save_0" (combined with save directory and .zsave extension)
	// uGameVersion: game-specific version for data migration
	// pfnWritePayload: callback that writes game data into the stream
	// pxUserData: passed through to the callback
	// Returns true on success
	bool Save(const char* szSlotName, uint32_t uGameVersion,
		SaveWriteCallback pfnWritePayload, void* pxUserData);

	// Load game data from a named slot
	// szSlotName: e.g. "autosave", "save_0"
	// pfnReadPayload: callback that reads game data from the stream
	// pxUserData: passed through to the callback
	// Returns true on success (file exists, valid magic, checksum matches)
	bool Load(const char* szSlotName, SaveReadCallback pfnReadPayload, void* pxUserData);

	// Check if a save slot exists on disk
	bool SlotExists(const char* szSlotName);

	// Delete a save slot from disk
	bool DeleteSlot(const char* szSlotName);

	// Compute CRC32 checksum of a data buffer
	uint32_t ComputeCRC32(const void* pData, uint64_t ulSize);
}
