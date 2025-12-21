#pragma once
#include <fstream>

#define ZENITH_TEXTURE_EXT		".ztxtr"
#define ZENITH_MESH_EXT			".zmesh"
#define ZENITH_MATERIAL_EXT		".zmtrl"

#define ZENITH_MAX_PATH_LENGTH 1024

namespace Zenith_FileAccess
{
	static char* ReadFile(const char* szFilename)
	{
		std::ifstream xFile(szFilename, std::ios::ate | std::ios::binary);
		Zenith_Assert(xFile.is_open(), "Failed to open file");

		uint64_t ulFileSize = xFile.tellg();
		char* pcRet = static_cast<char*>(Zenith_MemoryManagement::Allocate(ulFileSize));
		xFile.seekg(0);
		xFile.read(pcRet, ulFileSize);
		xFile.close();
		return pcRet;
	}

	static char* ReadFile(const char* szFilename, uint64_t& ulSize)
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

	static void WriteFile(const char* szFilename, const void* const pData, const uint64_t ulSize)
	{
		char acFixedFilename[ZENITH_MAX_PATH_LENGTH]{0};
		strncpy(acFixedFilename, szFilename, strlen(szFilename));
		Zenith_StringUtil::ReplaceAllChars(acFixedFilename, '\\', '/');

		std::ofstream xFile(acFixedFilename, std::ios::trunc | std::ios::binary);
		Zenith_Assert(xFile.is_open(), "Failed to open file");

		xFile.write(static_cast<const char*>(pData), ulSize);
		xFile.close();
	}
}