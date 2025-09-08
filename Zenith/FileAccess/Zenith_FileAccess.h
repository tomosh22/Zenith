#pragma once
#include <fstream>

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
		FILE* pxFile = fopen(szFilename, "wb");
		fwrite(pData, ulSize, 1, pxFile);
		fclose(pxFile);
	}
}