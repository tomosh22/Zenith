#pragma once

#include "DataStream/Zenith_DataStream.h"
#include "Core/Zenith_Result.h"
#include "FileAccess/Zenith_FileAccess.h"   // ZENITH_MAX_PATH_LENGTH / ZENITH_SAVE_EXT

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
	// Initialize the save system. Called ONCE at startup, by the ENGINE
	// (Zenith_Engine::InitialiseProject, step 1 of the B12 boot order) with
	// Project_GetName(). Determines the platform-specific writable save
	// directory. szGameName: the per-game subdirectory (e.g. "TilePuzzle").
	//
	// ★ A GAME MUST NEVER CALL THIS. It used to be a per-game call and is not
	// any more: a SECOND Initialise re-enters the cross-process residue wipe
	// below, which would delete every slot an automated-test batch had written
	// so far -- silently, mid-run.
	void Initialise(const char* szGameName);

	// How many times Initialise() has run in this process. The SINGLE-INIT
	// contract pin: the engine unit BootInitDoesNotWipeAutomatedTestSandbox
	// asserts this is exactly 1, so a re-added game-owned call fails a gate
	// instead of quietly wiping a live sandbox.
	uint32_t GetInitialiseCallCount();

	// Get the save directory path (ends with /)
	// Windows: %APPDATA%/Zenith/<GameName>/
	// Android: <internal storage>/Zenith/<GameName>/
	// ...unless this process is an automated-test run, in which case it is an
	// owned SANDBOX directory instead — see below.
	const char* GetSaveDirectory();

	// ========================================================================
	// Automated-test save sandbox
	//
	// An automated-test run must never touch the player's real save data, and
	// must not leak a slot from one test into the next (or into the next
	// PROCESS). Initialise therefore redirects the whole save directory into a
	// sandbox whenever Zenith_CommandLine::IsAutomatedTestRun() is true.
	//
	// The sandbox is picked from one of exactly two TRUSTED roots:
	//   (a) the engine's own fallback, <exe dir>/TestSaveData/<run-id>/ —
	//       trusted because it is derived from this module's own path and
	//       created here;
	//   (b) a runner-supplied --test-save-root, accepted ONLY if it
	//       canonicalises, is a directory, and carries an ownership marker
	//       whose run-id matches --test-save-run-id. The runner is the only
	//       thing that writes markers, and only under the artifacts root, so
	//       marker presence IS the ownership proof.
	// A supplied root that fails any check is refused with a warning and the
	// engine falls back to (a). The engine never creates a supplied root.
	//
	// WipeTestSandbox() deletes ONLY *.zsave files, non-recursively, in the
	// exact marker-bearing canonical directory, and only when this process
	// actually established a sandbox. Never directories, never other
	// extensions, never traversal — the canonical form is resolved once at
	// Initialise and reused, so a junction cannot redirect a later delete.
	// Removing the <run-id> directory itself is the RUNNER's job.
	// ========================================================================

	// True when Initialise redirected saves into an owned sandbox.
	bool IsUsingTestSandbox();

	// Delete every .zsave in the sandbox. No-op (returns 0) when no sandbox is
	// active — so a stray call in a normal run can never touch real saves.
	// Returns the number of files deleted.
	uint32_t WipeTestSandbox();

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

	// Wipe the recording log + readback stash. Disk files are NOT touched
	// (WipeTestSandbox is the disk half, and it is deliberately separate: no
	// cleanup path may depend on the instrumentation log being present).
	void ClearForTest();

	// Scoped save-root override for boot units that do REAL disk I/O. Redirects
	// GetSaveDirectory() to a caller-owned directory for the scope and restores
	// the previous root (sandbox or production) on destruction, so a unit test
	// can exercise the genuine file path without writing anywhere it does not
	// own. Replaces the per-game ad-hoc slot-renaming schemes.
	class ScopedSaveRootForTest
	{
	public:
		explicit ScopedSaveRootForTest(const char* szDirectory);
		~ScopedSaveRootForTest();

		ScopedSaveRootForTest(const ScopedSaveRootForTest&) = delete;
		ScopedSaveRootForTest& operator=(const ScopedSaveRootForTest&) = delete;

	private:
		char m_acPrevDirectory[ZENITH_MAX_PATH_LENGTH];
		bool m_bPrevInitialised;
	};
#endif
}
