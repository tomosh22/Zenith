#include "Zenith.h"
#include "FileAccess/Zenith_FileAccess.h"

#include <filesystem>
#include <fstream>

namespace Zenith_FileAccess
{
	void InitialisePlatform(void* pPlatformData)
	{
		// No-op for Windows - uses standard file I/O
		(void)pPlatformData;
	}

	void SetWritableDirectory(const char* szPath)
	{
		// No-op for Windows - relative paths work from the working directory
		(void)szPath;
	}

	char* ReadFile(const char* szFilename)
	{
		std::ifstream xFile(szFilename, std::ios::ate | std::ios::binary);
		const bool bOpen = xFile.is_open();
		Zenith_Check(bOpen, "Failed to open file %s", szFilename);
		if (!bOpen)
		{
			return nullptr;
		}

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
		const bool bOpen = xFile.is_open();
		Zenith_Check(bOpen, "Failed to open file %s", szFilename);
		if (!bOpen)
		{
			ulSize = 0;
			return nullptr;
		}

		ulSize = xFile.tellg();
		char* pcRet = static_cast<char*>(Zenith_MemoryManagement::Allocate(ulSize));
		xFile.seekg(0);
		xFile.read(pcRet, ulSize);
		xFile.close();
		return pcRet;
	}

	bool ReadPrefix(const char* szFilename, void* pBuffer, uint64_t ulSize)
	{
		if (pBuffer == nullptr || ulSize == 0)
		{
			return false;
		}
		std::ifstream xFile(szFilename, std::ios::binary);
		if (!xFile.is_open())
		{
			return false;
		}
		xFile.read(static_cast<char*>(pBuffer), ulSize);
		const bool bOK = (xFile.gcount() == static_cast<std::streamsize>(ulSize));
		xFile.close();
		return bOK;
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

		// Ensure the parent directory exists. Fresh CI checkouts (and any other
		// caller writing to a path under a never-existed subtree, e.g.
		// EditorAutomation generating .zscen files into Assets/Scenes/ when
		// that dir is .gitignore'd) hit ENOENT here otherwise -- the assert
		// below would fire with "Failed to open file ... for writing".
		std::error_code xMkdirErr;
		const std::filesystem::path xPath(acFixedFilename);
		if (xPath.has_parent_path())
		{
			std::filesystem::create_directories(xPath.parent_path(), xMkdirErr);
			// Ignore xMkdirErr -- if the mkdir genuinely failed, the open
			// below will fail too and trip the existing assert with a
			// consistent message.
		}

		std::ofstream xFile(acFixedFilename, std::ios::trunc | std::ios::binary);
		Zenith_Assert(xFile.is_open(), "Failed to open file %s for writing", szFilename);

		xFile.write(static_cast<const char*>(pData), ulSize);
		xFile.close();

		// Post-write diagnostic: the write()/flush-on-close above is otherwise
		// UNCHECKED, so a disk-full / permission / quota failure silently
		// produces a truncated or empty file with no signal in a shipping
		// build. .fail() picks up both a failed write (badbit) and a failed
		// flush during close (failbit). Log-and-continue only -- WriteFile is
		// void with many best-effort callers that must NOT hard-fail (the
		// open-check above is likewise non-fatal in shipping), so do NOT
		// promote this to a return/assert or change the signature.
		Zenith_Check(!xFile.fail(), "WriteFile: write failed for '%s' (disk full / permission / quota?)", szFilename);
	}

	bool FileExists(const char* szFilename)
	{
		std::ifstream xFile(szFilename);
		return xFile.good();
	}
}
