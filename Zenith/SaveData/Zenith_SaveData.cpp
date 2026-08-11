#include "Zenith.h"

#include "SaveData/Zenith_SaveData.h"
#include "Core/Zenith_CommandLine.h"   // automated-test detection + sandbox flags
#include "Core/Zenith_PlatformStdio.h"

#include <cstring>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <string>

#ifdef ZENITH_ANDROID
#include <android_native_app_glue.h>
#endif

#ifdef ZENITH_WINDOWS
// DWORD / GetEnvironmentVariableA. Per the W5.2 note in Zenith.h, <Windows.h> is
// no longer in the PCH; this TU used to get it transitively via vulkan.hpp, but
// the Null / D3D12 backends pull no Vulkan, so include it explicitly here.
// Zenith_Win32.h carries the APIENTRY + LEAN_AND_MEAN guards.
#include "Core/Zenith_Win32.h"
#endif

namespace Zenith_SaveData
{
#ifdef ZENITH_INPUT_SIMULATOR
	// ============================================================================
	// MVP-0.4.3 test instrumentation -- recording log + readback stash. The
	// recording log captures every Save() call; the stash lets tests inject
	// "what disk would have returned" for a specific slot so a Load() short-
	// circuits to it. Both wiped only by ClearForTest.
	// ============================================================================
	static Zenith_Vector<WrittenSlot>     s_xWrittenSlotsLog;
	static Zenith_Vector<WrittenSlot>     s_xReadbackStash;

	static WrittenSlot* FindReadbackSlot(const char* szSlotName)
	{
		if (szSlotName == nullptr) return nullptr;
		for (auto& xSlot : s_xReadbackStash)
		{
			if (xSlot.m_strSlotName == szSlotName) return &xSlot;
		}
		return nullptr;
	}
#endif

	// ============================================================================
	// CRC32 Lookup Table (polynomial 0xEDB88320)
	// ============================================================================
	static uint32_t s_auCRC32Table[256];
	static bool s_bCRC32TableInitialised = false;

	static void InitialiseCRC32Table()
	{
		if (s_bCRC32TableInitialised)
			return;

		for (uint32_t i = 0; i < 256; ++i)
		{
			uint32_t uCRC = i;
			for (uint32_t j = 0; j < 8; ++j)
			{
				if (uCRC & 1)
					uCRC = (uCRC >> 1) ^ 0xEDB88320u;
				else
					uCRC >>= 1;
			}
			s_auCRC32Table[i] = uCRC;
		}
		s_bCRC32TableInitialised = true;
	}

	uint32_t ComputeCRC32(const void* pData, uint64_t ulSize)
	{
		InitialiseCRC32Table();

		uint32_t uCRC = 0xFFFFFFFF;
		const uint8_t* pBytes = static_cast<const uint8_t*>(pData);

		for (uint64_t i = 0; i < ulSize; ++i)
		{
			uCRC = s_auCRC32Table[(uCRC ^ pBytes[i]) & 0xFF] ^ (uCRC >> 8);
		}

		return uCRC ^ 0xFFFFFFFF;
	}

	// ============================================================================
	// Save Directory
	// ============================================================================
	static char s_acSaveDirectory[ZENITH_MAX_PATH_LENGTH] = { 0 };
	static bool s_bInitialised = false;
	// Single-init contract pin -- see GetInitialiseCallCount in the header.
	static uint32_t s_uInitialiseCallCount = 0;

	// ============================================================================
	// Automated-test save sandbox
	//
	// Everything below only ever runs when Zenith_CommandLine::IsAutomatedTestRun()
	// is true. The deletion contract is deliberately narrow: only *.zsave, only
	// non-recursively, only inside the ONE canonical directory resolved here at
	// Initialise time and proven to carry an ownership marker. Resolving the
	// canonical form once (rather than per delete) is what stops a junction
	// being swapped in later to redirect the deletes somewhere else.
	// ============================================================================
	static bool s_bTestSandboxActive = false;
	// Canonical sandbox directory WITHOUT a trailing separator -- the exact
	// directory WipeTestSandbox is allowed to delete inside.
	static char s_acTestSandboxDir[ZENITH_MAX_PATH_LENGTH] = { 0 };

	static constexpr const char* kszSandboxMarkerName = ".zenith_test_save_root";
	static constexpr const char* kszSandboxMarkerMagic = "ZENITH_TEST_SAVE_ROOT";

	// Reads the run-id line out of a marker file. Returns false when the file is
	// missing, unreadable, or does not start with the magic.
	static bool ReadSandboxMarkerRunId(const std::filesystem::path& xMarkerPath, std::string& strRunIdOut)
	{
		strRunIdOut.clear();
		std::FILE* pxFile = Zenith_PlatformStdio::OpenFile(xMarkerPath.string().c_str(), "rb");
		if (pxFile == nullptr) return false;
		char acMagic[64] = { 0 };
		char acRunId[256] = { 0 };
		const bool bReadMagic = (std::fgets(acMagic, sizeof(acMagic), pxFile) != nullptr);
		const bool bReadRunId = (std::fgets(acRunId, sizeof(acRunId), pxFile) != nullptr);
		std::fclose(pxFile);
		if (!bReadMagic) return false;

		auto TrimEol = [](char* szText)
		{
			size_t uLen = std::strlen(szText);
			while (uLen > 0 && (szText[uLen - 1] == '\n' || szText[uLen - 1] == '\r')) szText[--uLen] = '\0';
		};
		TrimEol(acMagic);
		if (std::strcmp(acMagic, kszSandboxMarkerMagic) != 0) return false;
		if (bReadRunId)
		{
			TrimEol(acRunId);
			strRunIdOut = acRunId;
		}
		return true;
	}

	static void WriteSandboxMarker(const std::filesystem::path& xDirectory, const char* szRunId)
	{
		const std::filesystem::path xMarker = xDirectory / kszSandboxMarkerName;
		std::FILE* pxFile = Zenith_PlatformStdio::OpenFile(xMarker.string().c_str(), "wb");
		if (pxFile == nullptr) return;
		std::fprintf(pxFile, "%s\n%s\n", kszSandboxMarkerMagic, szRunId != nullptr ? szRunId : "");
		std::fclose(pxFile);
	}

	// The directory holding this executable, canonicalised. NOT the working
	// directory: `zenith test` launches the exe from the repo root, so a
	// CWD-relative sandbox would drop test saves straight into the repository.
	static bool GetModuleDirectory(std::filesystem::path& xOut)
	{
#ifdef ZENITH_WINDOWS
		char acModulePath[ZENITH_MAX_PATH_LENGTH] = { 0 };
		const DWORD uLen = ::GetModuleFileNameA(nullptr, acModulePath, ZENITH_MAX_PATH_LENGTH);
		if (uLen == 0 || uLen >= ZENITH_MAX_PATH_LENGTH) return false;
		std::error_code xEC;
		const std::filesystem::path xCanonical = std::filesystem::weakly_canonical(std::filesystem::path(acModulePath), xEC);
		if (xEC) return false;
		xOut = xCanonical.parent_path();
		return true;
#else
		std::error_code xEC;
		xOut = std::filesystem::current_path(xEC);
		return !xEC;
#endif
	}

	// Accepts a runner-supplied root only when every ownership check passes.
	static bool TryAcceptSuppliedSandboxRoot(const char* szRoot, const char* szRunId, std::filesystem::path& xOut)
	{
		if (szRoot == nullptr || szRoot[0] == '\0') return false;

		std::error_code xEC;
		// canonical (not weakly_canonical): the directory MUST already exist --
		// the engine never creates a supplied root -- and this resolves Windows
		// junctions/symlinks so the accepted path is the real one.
		const std::filesystem::path xCanonical = std::filesystem::canonical(std::filesystem::path(szRoot), xEC);
		if (xEC)
		{
			Zenith_Warning(LOG_CATEGORY_CORE,
				"SaveData: --test-save-root '%s' does not canonicalise (%s); falling back to the engine-owned sandbox",
				szRoot, xEC.message().c_str());
			return false;
		}
		if (!std::filesystem::is_directory(xCanonical, xEC) || xEC)
		{
			Zenith_Warning(LOG_CATEGORY_CORE,
				"SaveData: --test-save-root '%s' is not a directory; falling back to the engine-owned sandbox", szRoot);
			return false;
		}

		std::string strMarkerRunId;
		if (!ReadSandboxMarkerRunId(xCanonical / kszSandboxMarkerName, strMarkerRunId))
		{
			Zenith_Warning(LOG_CATEGORY_CORE,
				"SaveData: --test-save-root '%s' carries no ownership marker; falling back to the engine-owned sandbox. "
				"Only the test runner writes markers, and only under the artifacts root -- an unmarked directory is not ours to delete in.",
				szRoot);
			return false;
		}
		const char* szExpectedRunId = (szRunId != nullptr) ? szRunId : "";
		if (strMarkerRunId != szExpectedRunId)
		{
			Zenith_Warning(LOG_CATEGORY_CORE,
				"SaveData: --test-save-root '%s' marker run-id '%s' does not match --test-save-run-id '%s'; "
				"falling back to the engine-owned sandbox",
				szRoot, strMarkerRunId.c_str(), szExpectedRunId);
			return false;
		}

		xOut = xCanonical;
		return true;
	}

	// Resolves the sandbox directory for this run, creating + marking the
	// engine-owned fallback when no supplied root is accepted. Returns false
	// only if even the fallback could not be established (in which case the
	// caller keeps the production directory rather than guessing).
	static bool ResolveTestSandboxDirectory(const char* szGameName, std::filesystem::path& xOut)
	{
		const char* szRunId = Zenith_CommandLine::GetTestSaveRunId();
		if (szRunId == nullptr || szRunId[0] == '\0') szRunId = "default";

		if (TryAcceptSuppliedSandboxRoot(Zenith_CommandLine::GetTestSaveRoot(), szRunId, xOut))
		{
			return true;
		}

		std::filesystem::path xModuleDir;
		if (!GetModuleDirectory(xModuleDir))
		{
			Zenith_Warning(LOG_CATEGORY_CORE,
				"SaveData: could not resolve the module directory; the automated-test save sandbox is unavailable");
			return false;
		}

		const std::filesystem::path xFallback = xModuleDir / "TestSaveData" / szGameName / szRunId;
		std::error_code xEC;
		std::filesystem::create_directories(xFallback, xEC);
		if (xEC)
		{
			Zenith_Warning(LOG_CATEGORY_CORE, "SaveData: could not create the sandbox '%s': %s",
				xFallback.string().c_str(), xEC.message().c_str());
			return false;
		}
		// The engine created it, so the engine marks it -- keeping the deletion
		// contract uniform: WipeTestSandbox only ever deletes inside a
		// marker-bearing directory, whoever produced it.
		WriteSandboxMarker(xFallback, szRunId);

		const std::filesystem::path xCanonical = std::filesystem::canonical(xFallback, xEC);
		xOut = xEC ? xFallback : xCanonical;
		return true;
	}

	static void BuildSlotPath(const char* szSlotName, char* szOutPath, size_t uOutSize)
	{
		snprintf(szOutPath, uOutSize, "%s%s%s", s_acSaveDirectory, szSlotName, ZENITH_SAVE_EXT);
	}

	void Initialise(const char* szGameName)
	{
		Zenith_Assert(szGameName != nullptr && szGameName[0] != '\0', "SaveData: Game name cannot be empty");

		s_uInitialiseCallCount++;

		s_bTestSandboxActive = false;
		s_acTestSandboxDir[0] = '\0';

		// Automated-test runs NEVER touch the player's real save data. This is
		// the whole redirect: every Save/Load/DeleteSlot below resolves through
		// s_acSaveDirectory, so nothing else in the file needs to know.
		if (Zenith_CommandLine::IsAutomatedTestRun())
		{
			std::filesystem::path xSandbox;
			if (ResolveTestSandboxDirectory(szGameName, xSandbox))
			{
				const std::string strSandbox = xSandbox.string();
				snprintf(s_acTestSandboxDir, ZENITH_MAX_PATH_LENGTH, "%s", strSandbox.c_str());
				snprintf(s_acSaveDirectory, ZENITH_MAX_PATH_LENGTH, "%s/", strSandbox.c_str());
				s_bTestSandboxActive = true;
				s_bInitialised = true;
				// Cross-process residue: a previous run's slots would otherwise
				// be visible to this run's FIRST test, which never passes
				// through the between-tests reset.
				const uint32_t uWiped = WipeTestSandbox();
				Zenith_Log(LOG_CATEGORY_CORE,
					"SaveData: automated-test run -- saves sandboxed to '%s' (%u stale .zsave removed)",
					s_acSaveDirectory, uWiped);
				return;
			}
			Zenith_Warning(LOG_CATEGORY_CORE,
				"SaveData: automated-test run could not establish a save sandbox; falling back to the production "
				"save directory. Test saves WILL be written to real save data.");
		}

#ifdef ZENITH_WINDOWS
		char szAppData[ZENITH_MAX_PATH_LENGTH];
		DWORD uResult = GetEnvironmentVariableA("APPDATA", szAppData, ZENITH_MAX_PATH_LENGTH);
		Zenith_Assert(uResult > 0 && uResult < ZENITH_MAX_PATH_LENGTH, "SaveData: Failed to get APPDATA environment variable");
		snprintf(s_acSaveDirectory, ZENITH_MAX_PATH_LENGTH, "%s/Zenith/%s/", szAppData, szGameName);
#elif defined(ZENITH_ANDROID)
		// On Android, use the app's internal files directory
		// e.g. /data/data/<package>/files/Zenith/TilePuzzle/
		android_app* pxApp = Zenith_Window::GetAndroidApp();
		Zenith_Assert(pxApp && pxApp->activity && pxApp->activity->internalDataPath, "SaveData: android_app internalDataPath not available");
		snprintf(s_acSaveDirectory, ZENITH_MAX_PATH_LENGTH, "%s/Zenith/%s/", pxApp->activity->internalDataPath, szGameName);
#else
		snprintf(s_acSaveDirectory, ZENITH_MAX_PATH_LENGTH, "Zenith/%s/", szGameName);
#endif

		// Ensure directory exists
		std::error_code xEC;
		std::filesystem::create_directories(s_acSaveDirectory, xEC);
		Zenith_Assert(!xEC, "SaveData: Failed to create directory '%s': %s", s_acSaveDirectory, xEC.message().c_str());

		s_bInitialised = true;

		Zenith_Log(LOG_CATEGORY_CORE, "SaveData: Initialised save directory: %s", s_acSaveDirectory);
	}

	const char* GetSaveDirectory()
	{
		return s_acSaveDirectory;
	}

	bool IsUsingTestSandbox()
	{
		return s_bTestSandboxActive;
	}

	uint32_t GetInitialiseCallCount()
	{
		return s_uInitialiseCallCount;
	}

	// The whole deletion contract in one place: re-verify ownership, then delete
	// ONLY top-level *.zsave regular files. Split out from WipeTestSandbox so
	// the contract can be unit-tested against a scratch directory without the
	// process having to be a real automated-test run.
	static uint32_t WipeSandboxDirectory(const std::filesystem::path& xDir)
	{
		// Re-check the ownership marker at DELETE time as well as at accept
		// time: cheap, and it means a directory that stopped being ours between
		// the two is left alone.
		std::string strRunId;
		if (!ReadSandboxMarkerRunId(xDir / kszSandboxMarkerName, strRunId))
		{
			Zenith_Warning(LOG_CATEGORY_CORE,
				"SaveData: '%s' carries no ownership marker; refusing to delete anything in it",
				xDir.string().c_str());
			return 0;
		}

		uint32_t uDeleted = 0;
		std::error_code xEC;
		// NON-recursive, and only regular files whose extension is exactly
		// ZENITH_SAVE_EXT. Directories are never removed (the <run-id> dir is
		// the runner's to clean up), the marker is never removed, and no other
		// extension is ever touched.
		for (std::filesystem::directory_iterator xIt(xDir, xEC), xEnd; !xEC && xIt != xEnd; xIt.increment(xEC))
		{
			std::error_code xFileEC;
			if (!xIt->is_regular_file(xFileEC) || xFileEC) continue;
			if (xIt->path().extension() != ZENITH_SAVE_EXT) continue;
			std::error_code xRemoveEC;
			if (std::filesystem::remove(xIt->path(), xRemoveEC) && !xRemoveEC) ++uDeleted;
		}
		return uDeleted;
	}

	uint32_t WipeTestSandbox()
	{
		// Refuses to do anything unless THIS process established a sandbox.
		// A stray call in a normal run can therefore never reach real saves.
		if (!s_bTestSandboxActive || !s_bInitialised || s_acTestSandboxDir[0] == '\0')
		{
			return 0;
		}
		return WipeSandboxDirectory(std::filesystem::path(s_acTestSandboxDir));
	}

	// ============================================================================
	// Save
	// ============================================================================
	bool Save(const char* szSlotName, uint32_t uGameVersion,
		SaveWriteCallback pfnWritePayload, void* pxUserData)
	{
		Zenith_Assert(s_bInitialised, "SaveData: Not initialised. Call Initialise() first");
		Zenith_Assert(szSlotName != nullptr, "SaveData: Slot name cannot be null");
		Zenith_Assert(pfnWritePayload != nullptr, "SaveData: Write callback cannot be null");

		// Write payload into a temporary stream
		Zenith_DataStream xPayloadStream;
		pfnWritePayload(xPayloadStream, pxUserData);

		uint64_t ulPayloadSize = xPayloadStream.GetCursor();

#ifdef ZENITH_INPUT_SIMULATOR
		// MVP-0.4.3: record the write so tests can audit what was persisted.
		// We copy the payload bytes BEFORE the header is added so the log
		// reflects the game-level data, not the file format scaffolding.
		{
			WrittenSlot xEntry;
			xEntry.m_strSlotName  = szSlotName;
			xEntry.m_uGameVersion = uGameVersion;
			if (ulPayloadSize > 0)
			{
				xEntry.m_xPayload.Reserve(static_cast<u_int>(ulPayloadSize));
				const uint8_t* pxBytes = static_cast<const uint8_t*>(xPayloadStream.GetData());
				for (uint64_t u = 0; u < ulPayloadSize; ++u)
				{
					xEntry.m_xPayload.PushBack(pxBytes[u]);
				}
			}
			s_xWrittenSlotsLog.PushBack(std::move(xEntry));
		}
#endif

		// Compute checksum over payload bytes
		uint32_t uChecksum = 0;
		if (ulPayloadSize > 0)
		{
			uChecksum = ComputeCRC32(xPayloadStream.GetData(), ulPayloadSize);
		}

		// Build header
		Zenith_SaveFileHeader xHeader;
		xHeader.uMagic = uZENITH_SAVE_MAGIC;
		xHeader.uFormatVersion = uZENITH_SAVE_FORMAT_VERSION;
		xHeader.uGameVersion = uGameVersion;
		xHeader.uChecksum = uChecksum;
		xHeader.ulPayloadSize = ulPayloadSize;
		xHeader.ulTimestamp = static_cast<uint64_t>(std::time(nullptr));

		// Write final file: header + payload
		Zenith_DataStream xFileStream;
		xFileStream.WriteData(&xHeader, sizeof(Zenith_SaveFileHeader));
		if (ulPayloadSize > 0)
		{
			xFileStream.WriteData(xPayloadStream.GetData(), ulPayloadSize);
		}

		// Build file path
		char szPath[ZENITH_MAX_PATH_LENGTH];
		BuildSlotPath(szSlotName, szPath, ZENITH_MAX_PATH_LENGTH);

		xFileStream.WriteToFile(szPath);

		Zenith_Log(LOG_CATEGORY_CORE, "SaveData: Saved to '%s' (%llu bytes payload, checksum=0x%08X)",
			szPath, ulPayloadSize, uChecksum);

		return true;
	}

	// ============================================================================
	// Load
	// ============================================================================
	bool Load(const char* szSlotName, SaveReadCallback pfnReadPayload, void* pxUserData)
	{
		// Wave9.1 (b): bool Load() preserves the legacy true/false contract for all
		// existing if(!Load(...)) callers; LoadEx carries the specific reason.
		return LoadEx(szSlotName, pfnReadPayload, pxUserData).IsOk();
	}

	Zenith_Status LoadEx(const char* szSlotName, SaveReadCallback pfnReadPayload, void* pxUserData)
	{
#ifdef ZENITH_INPUT_SIMULATOR
		// MVP-0.4.3: if a test staged a readback for this slot, short-circuit
		// the disk path and feed the staged payload to the read callback.
		// Runs BEFORE the Initialise / null-arg asserts so tests can exercise
		// load semantics without having to first call Initialise (which sets
		// up an APPDATA save directory we'd never use in a hermetic test).
		if (szSlotName != nullptr && pfnReadPayload != nullptr)
		{
			if (WrittenSlot* pxStash = FindReadbackSlot(szSlotName))
			{
				Zenith_DataStream xStream;
				if (pxStash->m_xPayload.GetSize() > 0)
				{
					xStream.WriteData(&pxStash->m_xPayload.Get(0), pxStash->m_xPayload.GetSize());
					xStream.SetCursor(0);
				}
				pfnReadPayload(xStream, pxStash->m_uGameVersion, pxUserData);
				return Zenith_ErrorCode::SUCCESS;
			}
		}
#endif

		Zenith_Assert(s_bInitialised, "SaveData: Not initialised. Call Initialise() first");
		Zenith_Assert(szSlotName != nullptr, "SaveData: Slot name cannot be null");
		Zenith_Assert(pfnReadPayload != nullptr, "SaveData: Read callback cannot be null");

		// Build file path
		char szPath[ZENITH_MAX_PATH_LENGTH];
		BuildSlotPath(szSlotName, szPath, ZENITH_MAX_PATH_LENGTH);

		// Check existence
		if (!Zenith_FileAccess::FileExists(szPath))
		{
			Zenith_Log(LOG_CATEGORY_CORE, "SaveData: No save file at '%s'", szPath);
			return Zenith_ErrorCode::FILE_NOT_FOUND;
		}

		// Read file
		Zenith_DataStream xFileStream;
		xFileStream.ReadFromFile(szPath);

		if (!xFileStream.IsValid())
		{
			Zenith_Warning(LOG_CATEGORY_CORE, "SaveData: Failed to read save file '%s'", szPath);
			return Zenith_ErrorCode::FILE_NOT_FOUND;
		}

		// Validate minimum size for header
		if (xFileStream.GetCapacity() < sizeof(Zenith_SaveFileHeader))
		{
			Zenith_Warning(LOG_CATEGORY_CORE, "SaveData: File too small to contain header '%s'", szPath);
			return Zenith_ErrorCode::CORRUPT_DATA;
		}

		// Read header
		Zenith_SaveFileHeader xHeader;
		xFileStream.ReadData(&xHeader, sizeof(Zenith_SaveFileHeader));

		// Validate magic number
		if (xHeader.uMagic != uZENITH_SAVE_MAGIC)
		{
			Zenith_Warning(LOG_CATEGORY_CORE, "SaveData: Invalid magic number in '%s' (expected 0x%08X, got 0x%08X)",
				szPath, uZENITH_SAVE_MAGIC, xHeader.uMagic);
			return Zenith_ErrorCode::BAD_MAGIC;
		}

		// Validate format version
		if (xHeader.uFormatVersion > uZENITH_SAVE_FORMAT_VERSION)
		{
			Zenith_Warning(LOG_CATEGORY_CORE, "SaveData: Save file format version %u is newer than supported %u",
				xHeader.uFormatVersion, uZENITH_SAVE_FORMAT_VERSION);
			return Zenith_ErrorCode::VERSION_MISMATCH;
		}

		// Validate payload size
		uint64_t ulRemainingBytes = xFileStream.GetCapacity() - sizeof(Zenith_SaveFileHeader);
		if (xHeader.ulPayloadSize > ulRemainingBytes)
		{
			Zenith_Warning(LOG_CATEGORY_CORE, "SaveData: Payload size mismatch in '%s' (header says %llu, file has %llu)",
				szPath, xHeader.ulPayloadSize, ulRemainingBytes);
			return Zenith_ErrorCode::CORRUPT_DATA;
		}

		// Validate checksum
		if (xHeader.ulPayloadSize > 0)
		{
			const uint8_t* pPayloadStart = static_cast<const uint8_t*>(xFileStream.GetData()) + sizeof(Zenith_SaveFileHeader);
			uint32_t uComputedChecksum = ComputeCRC32(pPayloadStart, xHeader.ulPayloadSize);

			if (uComputedChecksum != xHeader.uChecksum)
			{
				Zenith_Warning(LOG_CATEGORY_CORE, "SaveData: Checksum mismatch in '%s' (expected 0x%08X, computed 0x%08X). File may be corrupted.",
					szPath, xHeader.uChecksum, uComputedChecksum);
				return Zenith_ErrorCode::CORRUPT_DATA;
			}
		}

		// Create a DataStream wrapping just the payload for the read callback
		Zenith_DataStream xPayloadStream(
			const_cast<uint8_t*>(static_cast<const uint8_t*>(xFileStream.GetData()) + sizeof(Zenith_SaveFileHeader)),
			xHeader.ulPayloadSize);

		pfnReadPayload(xPayloadStream, xHeader.uGameVersion, pxUserData);

		Zenith_Log(LOG_CATEGORY_CORE, "SaveData: Loaded from '%s' (game version %u, %llu bytes payload)",
			szPath, xHeader.uGameVersion, xHeader.ulPayloadSize);

		return Zenith_ErrorCode::SUCCESS;
	}

	// ============================================================================
	// Utilities
	// ============================================================================
	bool SlotExists(const char* szSlotName)
	{
		Zenith_Assert(s_bInitialised, "SaveData: Not initialised");
		Zenith_Assert(szSlotName != nullptr, "SaveData: Slot name cannot be null");

		char szPath[ZENITH_MAX_PATH_LENGTH];
		BuildSlotPath(szSlotName, szPath, ZENITH_MAX_PATH_LENGTH);

		return Zenith_FileAccess::FileExists(szPath);
	}

	bool DeleteSlot(const char* szSlotName)
	{
		Zenith_Assert(s_bInitialised, "SaveData: Not initialised");
		Zenith_Assert(szSlotName != nullptr, "SaveData: Slot name cannot be null");

		char szPath[ZENITH_MAX_PATH_LENGTH];
		BuildSlotPath(szSlotName, szPath, ZENITH_MAX_PATH_LENGTH);

		if (!Zenith_FileAccess::FileExists(szPath))
		{
			return false;
		}

		std::filesystem::remove(szPath);
		Zenith_Log(LOG_CATEGORY_CORE, "SaveData: Deleted save slot '%s'", szPath);
		return true;
	}

#ifdef ZENITH_INPUT_SIMULATOR
	// ============================================================================
	// MVP-0.4.3 test instrumentation -- public API
	// ============================================================================
	const Zenith_Vector<WrittenSlot>& GetWrittenSlotsForTest()
	{
		return s_xWrittenSlotsLog;
	}

	void SetReadbackForTest(const char* szSlotName, uint32_t uGameVersion,
		const void* pData, uint64_t ulSize)
	{
		if (szSlotName == nullptr) return;

		WrittenSlot xSlot;
		xSlot.m_strSlotName  = szSlotName;
		xSlot.m_uGameVersion = uGameVersion;
		if (pData != nullptr && ulSize > 0)
		{
			xSlot.m_xPayload.Reserve(static_cast<u_int>(ulSize));
			const uint8_t* pxBytes = static_cast<const uint8_t*>(pData);
			for (uint64_t u = 0; u < ulSize; ++u)
			{
				xSlot.m_xPayload.PushBack(pxBytes[u]);
			}
		}

		// Update or insert in the stash.
		if (WrittenSlot* pxExisting = FindReadbackSlot(szSlotName))
		{
			*pxExisting = std::move(xSlot);
		}
		else
		{
			s_xReadbackStash.PushBack(std::move(xSlot));
		}
	}

	void ClearForTest()
	{
		s_xWrittenSlotsLog.Clear();
		s_xReadbackStash.Clear();
	}

	ScopedSaveRootForTest::ScopedSaveRootForTest(const char* szDirectory)
		: m_bPrevInitialised(s_bInitialised)
	{
		snprintf(m_acPrevDirectory, ZENITH_MAX_PATH_LENGTH, "%s", s_acSaveDirectory);

		if (szDirectory == nullptr || szDirectory[0] == '\0')
		{
			Zenith_Assert(false, "ScopedSaveRootForTest: directory cannot be empty");
			return;
		}

		std::error_code xEC;
		std::filesystem::create_directories(szDirectory, xEC);

		// Normalise to a trailing separator: BuildSlotPath concatenates
		// directly, so a missing one silently turns "<dir>" + "slot" into a
		// SIBLING file rather than a child.
		const size_t uLen = std::strlen(szDirectory);
		const bool bHasSeparator = (uLen > 0) && (szDirectory[uLen - 1] == '/' || szDirectory[uLen - 1] == '\\');
		snprintf(s_acSaveDirectory, ZENITH_MAX_PATH_LENGTH, "%s%s", szDirectory, bHasSeparator ? "" : "/");
		s_bInitialised = true;
	}

	ScopedSaveRootForTest::~ScopedSaveRootForTest()
	{
		snprintf(s_acSaveDirectory, ZENITH_MAX_PATH_LENGTH, "%s", m_acPrevDirectory);
		s_bInitialised = m_bPrevInitialised;
	}
#endif
}

#include "SaveData/Zenith_SaveData.Tests.inl"
