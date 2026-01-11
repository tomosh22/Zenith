#include "Zenith.h"
#include "AssetHandling/Zenith_AssetDatabase.h"
#include "AssetHandling/Zenith_AssetMeta.h"
#include "Core/Multithreading/Zenith_Multithreading.h"
#include "FileAccess/Zenith_FileAccess.h"
#include <algorithm>

// Static member initialization
std::unordered_map<Zenith_AssetGUID, Zenith_AssetDatabase::AssetEntry> Zenith_AssetDatabase::s_xAssetsByGUID;
std::unordered_map<std::string, Zenith_AssetGUID> Zenith_AssetDatabase::s_xGUIDsByPath;
std::unordered_map<Zenith_AssetGUID, std::unordered_set<Zenith_AssetGUID>> Zenith_AssetDatabase::s_xDependencies;
std::unordered_map<Zenith_AssetGUID, std::unordered_set<Zenith_AssetGUID>> Zenith_AssetDatabase::s_xDependents;
std::unordered_map<uint32_t, Zenith_AssetDatabase::ReloadCallback> Zenith_AssetDatabase::s_xReloadCallbacks;
uint32_t Zenith_AssetDatabase::s_uNextCallbackHandle = 1;
std::string Zenith_AssetDatabase::s_strProjectRoot;
bool Zenith_AssetDatabase::s_bInitialized = false;
Zenith_Mutex Zenith_AssetDatabase::s_xMutex;

//--------------------------------------------------------------------------
// Initialization
//--------------------------------------------------------------------------

void Zenith_AssetDatabase::Initialize(const std::string& strProjectRoot)
{
	Zenith_ScopedMutexLock xLock(s_xMutex);

	if (s_bInitialized)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "AssetDatabase already initialized, call Shutdown first");
		return;
	}

	s_strProjectRoot = NormalizePath(strProjectRoot);
	s_bInitialized = true;

	s_xAssetsByGUID.clear();
	s_xGUIDsByPath.clear();
	s_xDependencies.clear();
	s_xDependents.clear();

	Zenith_Log(LOG_CATEGORY_ASSET, "AssetDatabase initialized with root: %s", s_strProjectRoot.c_str());
}

void Zenith_AssetDatabase::Shutdown()
{
	Zenith_ScopedMutexLock xLock(s_xMutex);

	s_xAssetsByGUID.clear();
	s_xGUIDsByPath.clear();
	s_xDependencies.clear();
	s_xDependents.clear();
	s_xReloadCallbacks.clear();
	s_strProjectRoot.clear();
	s_bInitialized = false;

	Zenith_Log(LOG_CATEGORY_ASSET, "AssetDatabase shutdown");
}

bool Zenith_AssetDatabase::IsInitialized()
{
	return s_bInitialized;
}

const std::string& Zenith_AssetDatabase::GetProjectRoot()
{
	return s_strProjectRoot;
}

//--------------------------------------------------------------------------
// GUID <-> Path Lookups
//--------------------------------------------------------------------------

std::string Zenith_AssetDatabase::GetPathFromGUID(const Zenith_AssetGUID& xGUID)
{
	Zenith_ScopedMutexLock xLock(s_xMutex);

	auto xIt = s_xAssetsByGUID.find(xGUID);
	if (xIt != s_xAssetsByGUID.end())
	{
		return xIt->second.m_xMeta.m_strAssetPath;
	}
	return "";
}

Zenith_AssetGUID Zenith_AssetDatabase::GetGUIDFromPath(const std::string& strPath)
{
	Zenith_ScopedMutexLock xLock(s_xMutex);

	std::string strNormalized = NormalizePath(strPath);
	auto xIt = s_xGUIDsByPath.find(strNormalized);
	if (xIt != s_xGUIDsByPath.end())
	{
		return xIt->second;
	}
	return Zenith_AssetGUID::INVALID;
}

bool Zenith_AssetDatabase::AssetExists(const Zenith_AssetGUID& xGUID)
{
	Zenith_ScopedMutexLock xLock(s_xMutex);
	return s_xAssetsByGUID.find(xGUID) != s_xAssetsByGUID.end();
}

bool Zenith_AssetDatabase::AssetExistsByPath(const std::string& strPath)
{
	Zenith_ScopedMutexLock xLock(s_xMutex);
	std::string strNormalized = NormalizePath(strPath);
	return s_xGUIDsByPath.find(strNormalized) != s_xGUIDsByPath.end();
}

Zenith_AssetType Zenith_AssetDatabase::GetAssetType(const Zenith_AssetGUID& xGUID)
{
	Zenith_ScopedMutexLock xLock(s_xMutex);

	auto xIt = s_xAssetsByGUID.find(xGUID);
	if (xIt != s_xAssetsByGUID.end())
	{
		return xIt->second.m_xMeta.m_eAssetType;
	}
	return Zenith_AssetType::UNKNOWN;
}

void Zenith_AssetDatabase::GetAssetsByType(Zenith_AssetType eType, Zenith_Vector<Zenith_AssetGUID>& xOutGUIDs)
{
	Zenith_ScopedMutexLock xLock(s_xMutex);

	xOutGUIDs.Clear();
	for (const auto& xPair : s_xAssetsByGUID)
	{
		if (xPair.second.m_xMeta.m_eAssetType == eType)
		{
			xOutGUIDs.PushBack(xPair.first);
		}
	}
}

//--------------------------------------------------------------------------
// Asset Registration
//--------------------------------------------------------------------------

void Zenith_AssetDatabase::RegisterAsset(const Zenith_AssetMeta& xMeta)
{
	Zenith_ScopedMutexLock xLock(s_xMutex);

	std::string strNormalized = NormalizePath(xMeta.m_strAssetPath);

	AssetEntry xEntry;
	xEntry.m_xMeta = xMeta;
	xEntry.m_ulLastCheckedTime = xMeta.m_ulLastModifiedTime;

	s_xAssetsByGUID[xMeta.m_xGUID] = xEntry;
	s_xGUIDsByPath[strNormalized] = xMeta.m_xGUID;
}

void Zenith_AssetDatabase::UnregisterAsset(const Zenith_AssetGUID& xGUID)
{
	Zenith_ScopedMutexLock xLock(s_xMutex);

	auto xIt = s_xAssetsByGUID.find(xGUID);
	if (xIt != s_xAssetsByGUID.end())
	{
		std::string strNormalized = NormalizePath(xIt->second.m_xMeta.m_strAssetPath);
		s_xGUIDsByPath.erase(strNormalized);
		s_xAssetsByGUID.erase(xIt);

		// Clean up dependencies
		s_xDependencies.erase(xGUID);
		s_xDependents.erase(xGUID);

		// Remove from other assets' dependency lists
		for (auto& xPair : s_xDependencies)
		{
			xPair.second.erase(xGUID);
		}
		for (auto& xPair : s_xDependents)
		{
			xPair.second.erase(xGUID);
		}
	}
}

bool Zenith_AssetDatabase::GetAssetMeta(const Zenith_AssetGUID& xGUID, Zenith_AssetMeta& xOutMeta)
{
	Zenith_ScopedMutexLock xLock(s_xMutex);

	auto xIt = s_xAssetsByGUID.find(xGUID);
	if (xIt != s_xAssetsByGUID.end())
	{
		xOutMeta = xIt->second.m_xMeta;
		return true;
	}
	return false;
}

//--------------------------------------------------------------------------
// Dependency Tracking
//--------------------------------------------------------------------------

void Zenith_AssetDatabase::RegisterDependency(const Zenith_AssetGUID& xAsset, const Zenith_AssetGUID& xDependsOn)
{
	Zenith_ScopedMutexLock xLock(s_xMutex);

	s_xDependencies[xAsset].insert(xDependsOn);
	s_xDependents[xDependsOn].insert(xAsset);
}

void Zenith_AssetDatabase::UnregisterDependency(const Zenith_AssetGUID& xAsset, const Zenith_AssetGUID& xDependsOn)
{
	Zenith_ScopedMutexLock xLock(s_xMutex);

	auto xDepIt = s_xDependencies.find(xAsset);
	if (xDepIt != s_xDependencies.end())
	{
		xDepIt->second.erase(xDependsOn);
	}

	auto xDepentIt = s_xDependents.find(xDependsOn);
	if (xDepentIt != s_xDependents.end())
	{
		xDepentIt->second.erase(xAsset);
	}
}

void Zenith_AssetDatabase::ClearDependencies(const Zenith_AssetGUID& xAsset)
{
	Zenith_ScopedMutexLock xLock(s_xMutex);

	auto xDepIt = s_xDependencies.find(xAsset);
	if (xDepIt != s_xDependencies.end())
	{
		// Remove this asset from all its dependencies' dependent lists
		for (const auto& xDep : xDepIt->second)
		{
			auto xDepentIt = s_xDependents.find(xDep);
			if (xDepentIt != s_xDependents.end())
			{
				xDepentIt->second.erase(xAsset);
			}
		}
		xDepIt->second.clear();
	}
}

Zenith_Vector<Zenith_AssetGUID> Zenith_AssetDatabase::GetDependencies(const Zenith_AssetGUID& xAsset)
{
	Zenith_ScopedMutexLock xLock(s_xMutex);

	Zenith_Vector<Zenith_AssetGUID> xResult;
	auto xIt = s_xDependencies.find(xAsset);
	if (xIt != s_xDependencies.end())
	{
		xResult.Reserve(static_cast<u_int>(xIt->second.size()));
		for (const auto& xDep : xIt->second)
		{
			xResult.PushBack(xDep);
		}
	}
	return xResult;
}

Zenith_Vector<Zenith_AssetGUID> Zenith_AssetDatabase::GetDependents(const Zenith_AssetGUID& xAsset)
{
	Zenith_ScopedMutexLock xLock(s_xMutex);

	Zenith_Vector<Zenith_AssetGUID> xResult;
	auto xIt = s_xDependents.find(xAsset);
	if (xIt != s_xDependents.end())
	{
		xResult.Reserve(static_cast<u_int>(xIt->second.size()));
		for (const auto& xDep : xIt->second)
		{
			xResult.PushBack(xDep);
		}
	}
	return xResult;
}

//--------------------------------------------------------------------------
// Utilities
//--------------------------------------------------------------------------

std::string Zenith_AssetDatabase::NormalizePath(const std::string& strPath)
{
	std::string strResult = strPath;

	// Convert backslashes to forward slashes
	std::replace(strResult.begin(), strResult.end(), '\\', '/');

	// Remove trailing slash
	while (!strResult.empty() && strResult.back() == '/')
	{
		strResult.pop_back();
	}

	// Convert to lowercase for case-insensitive comparison (Windows)
#ifdef _WIN32
	std::transform(strResult.begin(), strResult.end(), strResult.begin(), ::tolower);
#endif

	return strResult;
}

//--------------------------------------------------------------------------
// Hot-Reload
//--------------------------------------------------------------------------

uint32_t Zenith_AssetDatabase::RegisterReloadCallback(ReloadCallback pfnCallback)
{
	Zenith_ScopedMutexLock xLock(s_xMutex);
	uint32_t uHandle = s_uNextCallbackHandle++;
	s_xReloadCallbacks[uHandle] = pfnCallback;
	return uHandle;
}

void Zenith_AssetDatabase::UnregisterReloadCallback(uint32_t uHandle)
{
	Zenith_ScopedMutexLock xLock(s_xMutex);
	s_xReloadCallbacks.erase(uHandle);
}

void Zenith_AssetDatabase::CheckForModifications()
{
	// This is handled by the FileWatcher now
	// This method is kept for backward compatibility
}

void Zenith_AssetDatabase::ReloadAsset(const Zenith_AssetGUID& xGUID)
{
	if (!xGUID.IsValid())
	{
		return;
	}

	Zenith_Log(LOG_CATEGORY_ASSET, "Reloading asset %s", xGUID.ToString().c_str());

	// Notify registered callbacks
	Zenith_Vector<ReloadCallback> xCallbacksCopy;
	{
		Zenith_ScopedMutexLock xLock(s_xMutex);
		for (const auto& xPair : s_xReloadCallbacks)
		{
			xCallbacksCopy.PushBack(xPair.second);
		}
	}

	for (Zenith_Vector<ReloadCallback>::Iterator xIt(xCallbacksCopy); !xIt.Done(); xIt.Next())
	{
		xIt.GetData()(xGUID);
	}

	// Notify dependent assets
	NotifyDependents(xGUID);
}

void Zenith_AssetDatabase::NotifyDependents(const Zenith_AssetGUID& xGUID)
{
	Zenith_Vector<Zenith_AssetGUID> xDependentsList = GetDependents(xGUID);

	for (Zenith_Vector<Zenith_AssetGUID>::Iterator xIt(xDependentsList); !xIt.Done(); xIt.Next())
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "Cascading reload to dependent %s", xIt.GetData().ToString().c_str());
		ReloadAsset(xIt.GetData());
	}
}

//--------------------------------------------------------------------------
// Project Scanning
//--------------------------------------------------------------------------

bool Zenith_AssetDatabase::IsAssetFile(const std::string& strPath)
{
	// Skip meta files
	if (strPath.find(Zenith_AssetMeta::META_EXTENSION) != std::string::npos)
	{
		return false;
	}

	// Skip hidden files
	std::string strFilename = strPath.substr(strPath.find_last_of("/\\") + 1);
	if (!strFilename.empty() && strFilename[0] == '.')
	{
		return false;
	}

	// Skip temp/backup files
	if (strPath.find('~') != std::string::npos)
	{
		return false;
	}

	return true;
}

void Zenith_AssetDatabase::ScanDirectory(const std::string& strDirectory)
{
	Zenith_Log(LOG_CATEGORY_ASSET, "Scanning directory: %s", strDirectory.c_str());

	// Note: Implementation would recursively scan the directory,
	// load or create .zmeta files, and register assets
	// This is a placeholder for the full implementation
}

void Zenith_AssetDatabase::RefreshProject()
{
	if (!s_bInitialized)
	{
		return;
	}

	Zenith_Log(LOG_CATEGORY_ASSET, "Refreshing project");
	ScanDirectory(s_strProjectRoot);
}

Zenith_AssetGUID Zenith_AssetDatabase::ImportAsset(const std::string& strAssetPath)
{
	// Check if asset already exists in memory
	Zenith_AssetGUID xExistingGUID = GetGUIDFromPath(strAssetPath);
	if (xExistingGUID.IsValid())
	{
		return xExistingGUID;
	}

	// Check if .zmeta file already exists on disk (from a previous run)
	Zenith_AssetMeta xMeta;
	std::string strMetaPath = Zenith_AssetMeta::GetMetaPath(strAssetPath);
	if (Zenith_AssetMeta::MetaFileExists(strAssetPath))
	{
		// Load existing meta file to preserve the GUID
		if (!xMeta.LoadFromFile(strMetaPath))
		{
			Zenith_Error(LOG_CATEGORY_ASSET, "Failed to load existing meta file: %s", strMetaPath.c_str());
			return Zenith_AssetGUID::INVALID;
		}
		Zenith_Log(LOG_CATEGORY_ASSET, "Loaded existing meta for: %s -> %s", strAssetPath.c_str(), xMeta.m_xGUID.ToString().c_str());
	}
	else
	{
		// Create new meta file
		if (!xMeta.CreateForAsset(strAssetPath, s_strProjectRoot))
		{
			Zenith_Error(LOG_CATEGORY_ASSET, "Failed to create meta for: %s", strAssetPath.c_str());
			return Zenith_AssetGUID::INVALID;
		}

		// Save meta file
		if (!xMeta.SaveToFile(strMetaPath))
		{
			Zenith_Error(LOG_CATEGORY_ASSET, "Failed to save meta file: %s", strMetaPath.c_str());
			return Zenith_AssetGUID::INVALID;
		}
		Zenith_Log(LOG_CATEGORY_ASSET, "Created new meta for: %s -> %s", strAssetPath.c_str(), xMeta.m_xGUID.ToString().c_str());
	}

	// Use the absolute path passed to ImportAsset for registration and lookups.
	// The meta file stores a relative path for portability, but at runtime we need
	// absolute paths for file system operations.
	xMeta.m_strAssetPath = strAssetPath;

	// Register the asset
	RegisterAsset(xMeta);

	return xMeta.m_xGUID;
}

bool Zenith_AssetDatabase::MoveAsset(const Zenith_AssetGUID& xGUID, const std::string& strNewPath)
{
	Zenith_ScopedMutexLock xLock(s_xMutex);

	auto xIt = s_xAssetsByGUID.find(xGUID);
	if (xIt == s_xAssetsByGUID.end())
	{
		return false;
	}

	// Update path mappings
	std::string strOldNormalized = NormalizePath(xIt->second.m_xMeta.m_strAssetPath);
	std::string strNewNormalized = NormalizePath(strNewPath);

	s_xGUIDsByPath.erase(strOldNormalized);
	s_xGUIDsByPath[strNewNormalized] = xGUID;

	// Update meta
	xIt->second.m_xMeta.m_strAssetPath = strNewPath;

	Zenith_Log(LOG_CATEGORY_ASSET, "Moved asset %s -> %s", strOldNormalized.c_str(), strNewNormalized.c_str());
	return true;
}

bool Zenith_AssetDatabase::DeleteAsset(const Zenith_AssetGUID& xGUID)
{
	UnregisterAsset(xGUID);
	return true;
}
