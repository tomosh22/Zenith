#include "Zenith.h"
#include "FileAccess/Zenith_FileAccess.h"

#include <android/asset_manager.h>
#include <android/log.h>
#include <fstream>

#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, "Zenith_FileAccess", __VA_ARGS__))

namespace Zenith_FileAccess
{
	static AAssetManager* s_pxAssetManager = nullptr;

	void InitialisePlatform(void* pPlatformData)
	{
		s_pxAssetManager = static_cast<AAssetManager*>(pPlatformData);
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

		// Fall back to filesystem (external storage)
		std::ifstream xFile(szFilename, std::ios::ate | std::ios::binary);
		if (!xFile.is_open())
		{
			LOGE("Failed to open file: %s", szFilename);
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

	void FreeFileData(char* pData)
	{
		Zenith_MemoryManagement::Deallocate(pData);
	}

	void WriteFile(const char* szFilename, const void* const pData, const uint64_t ulSize)
	{
		// Writing is only supported to external storage, not APK assets
		char acFixedFilename[ZENITH_MAX_PATH_LENGTH]{ 0 };
		strncpy(acFixedFilename, szFilename, strlen(szFilename));
		Zenith_StringUtil::ReplaceAllChars(acFixedFilename, '\\', '/');

		std::ofstream xFile(acFixedFilename, std::ios::trunc | std::ios::binary);
		if (!xFile.is_open())
		{
			LOGE("Failed to open file for writing: %s", szFilename);
			Zenith_Assert(false, "Failed to open file %s for writing", szFilename);
			return;
		}

		xFile.write(static_cast<const char*>(pData), ulSize);
		xFile.close();
	}
}
