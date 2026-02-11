#include "Zenith.h"
#include "FileAccess/Zenith_FileAccess.h"

#include <fstream>

namespace Zenith_FileAccess
{
	void InitialisePlatform(void* pPlatformData)
	{
		// No-op for Windows - uses standard file I/O
		(void)pPlatformData;
	}

	char* ReadFile(const char* szFilename)
	{
		std::ifstream xFile(szFilename, std::ios::ate | std::ios::binary);
		Zenith_Assert(xFile.is_open(), "Failed to open file %s", szFilename);

		uint64_t ulFileSize = xFile.tellg();
		char* pcRet = static_cast<char*>(Zenith_MemoryManagement::Allocate(ulFileSize));
		xFile.seekg(0);
		xFile.read(pcRet, ulFileSize);
		xFile.close();
		return pcRet;
	}

	char* ReadFile(const char* szFilename, uint64_t& ulSize)
	{
		std::ifstream xFile(szFilename, std::ios::ate | std::ios::binary);
		Zenith_Assert(xFile.is_open(), "Failed to open file %s", szFilename);

		ulSize = xFile.tellg();
		char* pcRet = static_cast<char*>(Zenith_MemoryManagement::Allocate(ulSize));
		xFile.seekg(0);
		xFile.read(pcRet, ulSize);
		xFile.close();
		return pcRet;
	}

	void FreeFileData(char* pData)
	{
		Zenith_MemoryManagement::Deallocate(pData);
	}

	void WriteFile(const char* szFilename, const void* const pData, const uint64_t ulSize)
	{
		char acFixedFilename[ZENITH_MAX_PATH_LENGTH]{ 0 };
		// Use destination buffer size, not source string length, to prevent buffer overflow
		strncpy_s(acFixedFilename, sizeof(acFixedFilename), szFilename, _TRUNCATE);
		Zenith_StringUtil::ReplaceAllChars(acFixedFilename, '\\', '/');

		std::ofstream xFile(acFixedFilename, std::ios::trunc | std::ios::binary);
		Zenith_Assert(xFile.is_open(), "Failed to open file %s for writing", szFilename);

		xFile.write(static_cast<const char*>(pData), ulSize);
		xFile.close();
	}

	bool FileExists(const char* szFilename)
	{
		std::ifstream xFile(szFilename);
		return xFile.good();
	}
}
