#include "Zenith.h"

#include "SaveData/Zenith_SaveData.h"

#include <cstring>
#include <ctime>
#include <filesystem>

namespace Zenith_SaveData
{
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
		// On Android, use the internal files directory
		// The path is typically /data/data/<package>/files/
		// For now, use a relative path that the Android file access layer can resolve
		snprintf(s_acSaveDirectory, ZENITH_MAX_PATH_LENGTH, "Zenith/%s/", szGameName);
#else
		snprintf(s_acSaveDirectory, ZENITH_MAX_PATH_LENGTH, "Zenith/%s/", szGameName);
#endif

		// Ensure directory exists
		std::filesystem::create_directories(s_acSaveDirectory);

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
			return false;
		}

		// Read file
		Zenith_DataStream xFileStream;
		xFileStream.ReadFromFile(szPath);

		if (!xFileStream.IsValid())
		{
			Zenith_Warning(LOG_CATEGORY_CORE, "SaveData: Failed to read save file '%s'", szPath);
			return false;
		}

		// Validate minimum size for header
		if (xFileStream.GetSize() < sizeof(Zenith_SaveFileHeader))
		{
			Zenith_Warning(LOG_CATEGORY_CORE, "SaveData: File too small to contain header '%s'", szPath);
			return false;
		}

		// Read header
		Zenith_SaveFileHeader xHeader;
		xFileStream.ReadData(&xHeader, sizeof(Zenith_SaveFileHeader));

		// Validate magic number
		if (xHeader.uMagic != uZENITH_SAVE_MAGIC)
		{
			Zenith_Warning(LOG_CATEGORY_CORE, "SaveData: Invalid magic number in '%s' (expected 0x%08X, got 0x%08X)",
				szPath, uZENITH_SAVE_MAGIC, xHeader.uMagic);
			return false;
		}

		// Validate format version
		if (xHeader.uFormatVersion > uZENITH_SAVE_FORMAT_VERSION)
		{
			Zenith_Warning(LOG_CATEGORY_CORE, "SaveData: Save file format version %u is newer than supported %u",
				xHeader.uFormatVersion, uZENITH_SAVE_FORMAT_VERSION);
			return false;
		}

		// Validate payload size
		uint64_t ulRemainingBytes = xFileStream.GetSize() - sizeof(Zenith_SaveFileHeader);
		if (xHeader.ulPayloadSize > ulRemainingBytes)
		{
			Zenith_Warning(LOG_CATEGORY_CORE, "SaveData: Payload size mismatch in '%s' (header says %llu, file has %llu)",
				szPath, xHeader.ulPayloadSize, ulRemainingBytes);
			return false;
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
				return false;
			}
		}

		// Create a DataStream wrapping just the payload for the read callback
		Zenith_DataStream xPayloadStream(
			const_cast<uint8_t*>(static_cast<const uint8_t*>(xFileStream.GetData()) + sizeof(Zenith_SaveFileHeader)),
			xHeader.ulPayloadSize);

		pfnReadPayload(xPayloadStream, xHeader.uGameVersion, pxUserData);

		Zenith_Log(LOG_CATEGORY_CORE, "SaveData: Loaded from '%s' (game version %u, %llu bytes payload)",
			szPath, xHeader.uGameVersion, xHeader.ulPayloadSize);

		return true;
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
}
