#pragma once
#include <fstream>

namespace Zenith_FileAccess
{
	static char* ReadFile(const char* szFilename)
	{
		std::ifstream xFile(szFilename, std::ios::ate | std::ios::binary);
		Zenith_Assert(xFile.is_open(), "Failed to open file");

		uint64_t ulFileSize = xFile.tellg();
		char* pcRet = new char[ulFileSize];
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
		char* pcRet = new char[ulSize];
		xFile.seekg(0);
		xFile.read(pcRet, ulSize);
		xFile.close();
		return pcRet;
	}
}