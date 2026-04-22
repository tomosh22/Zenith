#include "Zenith.h"
#include "FileAccess/Zenith_FileAccess.h"

#include <android/asset_manager.h>
#include <android/log.h>
#include <fstream>
#include <filesystem>

#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, "Zenith_FileAccess", __VA_ARGS__))

namespace Zenith_FileAccess
{
	static AAssetManager* s_pxAssetManager = nullptr;
	static char s_acWritableDir[ZENITH_MAX_PATH_LENGTH] = { 0 };

	void InitialisePlatform(void* pPlatformData)
	{
		s_pxAssetManager = static_cast<AAssetManager*>(pPlatformData);
	}

	void SetWritableDirectory(const char* szPath)
	{
		strncpy(s_acWritableDir, szPath, ZENITH_MAX_PATH_LENGTH - 1);
		s_acWritableDir[ZENITH_MAX_PATH_LENGTH - 1] = '\0';
		// Ensure trailing slash
		size_t uLen = strlen(s_acWritableDir);
		if (uLen > 0 && uLen < ZENITH_MAX_PATH_LENGTH - 1 && s_acWritableDir[uLen - 1] != '/')
		{
			s_acWritableDir[uLen] = '/';
			s_acWritableDir[uLen + 1] = '\0';
		}
	}

	static bool IsAbsolutePath(const char* szPath)
	{
		return szPath[0] == '/';
	}

	// Resolve relative paths to the writable directory
	static void ResolveWritablePath(const char* szFilename, char* szOut, size_t uOutSize)
	{
		if (IsAbsolutePath(szFilename) || s_acWritableDir[0] == '\0')
		{
			strncpy(szOut, szFilename, uOutSize - 1);
			szOut[uOutSize - 1] = '\0';
		}
		else
		{
			snprintf(szOut, uOutSize, "%s%s", s_acWritableDir, szFilename);
		}
	}

	char* ReadFile(const char* szFilename)
	{
		uint64_t ulSize;
		return ReadFile(szFilename, ulSize);
	}

	char* ReadFile(const char* szFilename, uint64_t& ulSize)
	{
		// Try AAssetManager first (APK assets)
		if (s_pxAssetManager)
		{
			AAsset* pxAsset = AAssetManager_open(s_pxAssetManager, szFilename, AASSET_MODE_BUFFER);
			if (pxAsset)
			{
				ulSize = AAsset_getLength(pxAsset);
				char* pcRet = static_cast<char*>(Zenith_MemoryManagement::Allocate(ulSize));
				AAsset_read(pxAsset, pcRet, ulSize);
				AAsset_close(pxAsset);
				return pcRet;
			}
		}

		// Fall back to filesystem (internal storage)
		char acResolvedPath[ZENITH_MAX_PATH_LENGTH];
		ResolveWritablePath(szFilename, acResolvedPath, ZENITH_MAX_PATH_LENGTH);

		std::ifstream xFile(acResolvedPath, std::ios::ate | std::ios::binary);
		if (!xFile.is_open())
		{
			LOGE("Failed to open file: %s (resolved: %s)", szFilename, acResolvedPath);
			Zenith_Assert(false, "Failed to open file %s", szFilename);
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

		// Try AAssetManager first (APK assets)
		if (s_pxAssetManager)
		{
			AAsset* pxAsset = AAssetManager_open(s_pxAssetManager, szFilename, AASSET_MODE_STREAMING);
			if (pxAsset)
			{
				const off_t ulAssetLen = AAsset_getLength(pxAsset);
				if (static_cast<uint64_t>(ulAssetLen) < ulSize)
				{
					AAsset_close(pxAsset);
					return false;
				}
				const int iRead = AAsset_read(pxAsset, pBuffer, ulSize);
				AAsset_close(pxAsset);
				return iRead == static_cast<int>(ulSize);
			}
		}

		// Fall back to filesystem
		char acResolvedPath[ZENITH_MAX_PATH_LENGTH];
		ResolveWritablePath(szFilename, acResolvedPath, ZENITH_MAX_PATH_LENGTH);

		std::ifstream xFile(acResolvedPath, std::ios::binary);
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
		// Resolve relative paths to writable internal storage
		char acResolvedPath[ZENITH_MAX_PATH_LENGTH]{ 0 };
		ResolveWritablePath(szFilename, acResolvedPath, ZENITH_MAX_PATH_LENGTH);
		Zenith_StringUtil::ReplaceAllChars(acResolvedPath, '\\', '/');

		// Create parent directories if needed
		char acDirPath[ZENITH_MAX_PATH_LENGTH];
		strncpy(acDirPath, acResolvedPath, ZENITH_MAX_PATH_LENGTH - 1);
		acDirPath[ZENITH_MAX_PATH_LENGTH - 1] = '\0';
		char* pLastSlash = strrchr(acDirPath, '/');
		if (pLastSlash)
		{
			*pLastSlash = '\0';
			std::error_code xEC;
			std::filesystem::create_directories(acDirPath, xEC);
		}

		std::ofstream xFile(acResolvedPath, std::ios::trunc | std::ios::binary);
		if (!xFile.is_open())
		{
			LOGE("Failed to open file for writing: %s", szFilename);
			Zenith_Assert(false, "Failed to open file %s for writing", szFilename);
			return;
		}

		xFile.write(static_cast<const char*>(pData), ulSize);
		xFile.close();
	}

	bool FileExists(const char* szFilename)
	{
		// Try AAssetManager first (APK assets)
		if (s_pxAssetManager)
		{
			AAsset* pxAsset = AAssetManager_open(s_pxAssetManager, szFilename, AASSET_MODE_STREAMING);
			if (pxAsset)
			{
				AAsset_close(pxAsset);
				return true;
			}
		}

		// Fall back to filesystem check
		char acResolvedPath[ZENITH_MAX_PATH_LENGTH];
		ResolveWritablePath(szFilename, acResolvedPath, ZENITH_MAX_PATH_LENGTH);
		std::ifstream xFile(acResolvedPath);
		return xFile.good();
	}
}
