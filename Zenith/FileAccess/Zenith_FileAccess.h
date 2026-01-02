#pragma once

#define ZENITH_TEXTURE_EXT		".ztxtr"
#define ZENITH_MESH_EXT			".zmesh"
#define ZENITH_MATERIAL_EXT		".zmtrl"
#define ZENITH_PREFAB_EXT		".zprfb"

#define ZENITH_MAX_PATH_LENGTH 1024

// Platform-agnostic file access interface
// Implementations in platform-specific files:
// - Zenith/Windows/FileAccess/Zenith_Windows_FileAccess.cpp
// - Zenith/Android/FileAccess/Zenith_Android_FileAccess.cpp

namespace Zenith_FileAccess
{
	// Initialize platform-specific file access (e.g., set Android AAssetManager)
	void InitialisePlatform(void* pPlatformData);

	// Read file into allocated memory (caller must free with FreeFileData)
	char* ReadFile(const char* szFilename);
	char* ReadFile(const char* szFilename, uint64_t& ulSize);

	// Free data returned by ReadFile
	void FreeFileData(char* pData);

	// Write data to file (tools-only on Android, uses filesystem)
	void WriteFile(const char* szFilename, const void* const pData, const uint64_t ulSize);
}