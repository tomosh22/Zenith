#pragma once

#define ZENITH_TEXTURE_EXT		".ztxtr"
#define ZENITH_MESH_EXT			".zmesh"
#define ZENITH_MATERIAL_EXT		".zmtrl"
#define ZENITH_PREFAB_EXT		".zprfb"
#define ZENITH_SCENE_EXT		".zscen"
#define ZENITH_PARTICLES_EXT	".zptcl"

#define ZENITH_MAX_PATH_LENGTH 1024

// Platform-agnostic file access interface
// Implementations in platform-specific files:
// - Zenith/Windows/FileAccess/Zenith_Windows_FileAccess.cpp
// - Zenith/Android/FileAccess/Zenith_Android_FileAccess.cpp

namespace Zenith_FileAccess
{
	// Initialize platform-specific file access (e.g., set Android AAssetManager)
	void InitialisePlatform(void* pPlatformData);

	// Set the writable directory for file operations (Android: internalDataPath)
	// Relative paths in WriteFile/ReadFile(filesystem)/FileExists(filesystem) will be resolved relative to this
	void SetWritableDirectory(const char* szPath);

	// Read file into allocated memory (caller must free with FreeFileData)
	char* ReadFile(const char* szFilename);
	char* ReadFile(const char* szFilename, uint64_t& ulSize);

	// Read up to ulSize bytes from the start of szFilename into the caller-owned
	// pBuffer. Returns true only if the file opened and at least ulSize bytes
	// were read (partial reads are treated as failure). Intended for cheap
	// header-peek operations (e.g. magic + version) — avoids allocating and
	// reading the entire file just to inspect a prefix.
	bool ReadPrefix(const char* szFilename, void* pBuffer, uint64_t ulSize);

	// Free data returned by ReadFile
	void FreeFileData(char* pData);

	// Write data to file (tools-only on Android, uses filesystem)
	void WriteFile(const char* szFilename, const void* const pData, const uint64_t ulSize);

	// Check if a file exists
	bool FileExists(const char* szFilename);
}