#include "Zenith.h"

#include "SaveData/Zenith_SaveData.h"

#include <cstring>
#include <ctime>
#include <filesystem>

#ifdef ZENITH_ANDROID
#include <android_native_app_glue.h>
#endif

#ifdef ZENITH_WINDOWS
// DWORD / GetEnvironmentVariableA. Per the W5.2 note in Zenith.h, <Windows.h> is
// no longer in the PCH; this TU used to get it transitively via vulkan.hpp, but
// the D3D12 backend does not pull Vulkan, so include it directly here. GLFW (in
// the PCH) leaves APIENTRY defined and -- unlike the Vulkan build -- no earlier
// <windows.h> reset it, so undef before minwindef.h redefines it (C4005 / WX).
#ifdef APIENTRY
#undef APIENTRY
#endif
#include <Windows.h>
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

	static void BuildSlotPath(const char* szSlotName, char* szOutPath, size_t uOutSize)
	{
		snprintf(szOutPath, uOutSize, "%s%s%s", s_acSaveDirectory, szSlotName, ZENITH_SAVE_EXT);
	}

	void Initialise(const char* szGameName)
	{
		Zenith_Assert(szGameName != nullptr && szGameName[0] != '\0', "SaveData: Game name cannot be empty");

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
#endif
}
