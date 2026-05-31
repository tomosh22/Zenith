#pragma once

#include "DataStream/Zenith_DataStream.h"
#include "Core/Zenith_Result.h"

#ifdef ZENITH_INPUT_SIMULATOR
#include "Collections/Zenith_Vector.h"
#include <string>
#endif

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

	// Wave9.1 (b): graceful-load variant carrying the specific failure reason.
	// Same parameters as Load(); contains the real load body. Returns:
	//   SUCCESS          - load succeeded (or a staged test readback was served)
	//   FILE_NOT_FOUND   - missing file / unreadable stream
	//   BAD_MAGIC        - wrong magic number in the header
	//   VERSION_MISMATCH - file format version newer than supported
	//   CORRUPT_DATA     - too small for a header / payload-size mismatch / CRC mismatch
	// bool Load(...) is a thin wrapper: `return LoadEx(...).IsOk();`, so every
	// existing if(!Load(...)) caller keeps working unchanged.
	Zenith_Status LoadEx(const char* szSlotName, SaveReadCallback pfnReadPayload, void* pxUserData);

	// Check if a save slot exists on disk
	bool SlotExists(const char* szSlotName);

	// Delete a save slot from disk
	bool DeleteSlot(const char* szSlotName);

	// Compute CRC32 checksum of a data buffer
	uint32_t ComputeCRC32(const void* pData, uint64_t ulSize);

#ifdef ZENITH_INPUT_SIMULATOR
	// ========================================================================
	// Test instrumentation (MVP-0.4.3)
	//
	// Recording + readback hooks for headless save tests. Tests typically:
	//   1. Call ClearForTest() at setup to wipe recorded state.
	//   2. Use SetReadbackForTest(...) BEFORE the system-under-test calls
	//      Load() -- the staged payload bypasses disk and is returned to
	//      the read callback directly.
	//   3. Run gameplay that calls Save() / Load() through the normal API.
	//   4. Inspect GetWrittenSlotsForTest() to assert what was persisted
	//      (slot name + raw payload bytes + game version).
	//
	// The recording lives behind ZENITH_INPUT_SIMULATOR so shipping builds
	// stay clean. Save() / Load() still hit disk in test builds too unless
	// SetReadbackForTest stages a payload for the requested slot (in which
	// case Load() short-circuits to the stash without reading the file).
	// ========================================================================
	struct WrittenSlot
	{
		std::string m_strSlotName;
		uint32_t    m_uGameVersion = 0;
		Zenith_Vector<uint8_t> m_xPayload;
	};

	// Returns every Save() call since the last ClearForTest, in order. The
	// payload bytes are exactly what was written between the header and
	// EOF (no header, no CRC -- those live in the file on disk).
	const Zenith_Vector<WrittenSlot>& GetWrittenSlotsForTest();

	// Stage a payload that the next Load(szSlotName, ...) returns via its
	// read callback instead of reading from disk. The stash persists across
	// Save() / Load() calls; only ClearForTest wipes it.
	void SetReadbackForTest(const char* szSlotName, uint32_t uGameVersion,
		const void* pData, uint64_t ulSize);

	// Wipe the recording log + readback stash. Disk files are NOT touched.
	void ClearForTest();
#endif
}
