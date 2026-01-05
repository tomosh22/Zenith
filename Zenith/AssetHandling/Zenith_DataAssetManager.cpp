#include "Zenith.h"
#include "Zenith_DataAssetManager.h"
#include "DataStream/Zenith_DataStream.h"
#include <fstream>

// Static member definitions
std::unordered_map<std::string, DataAssetFactoryFn> Zenith_DataAssetManager::s_xTypeRegistry;
Zenith_Mutex Zenith_DataAssetManager::s_xRegistryMutex;
std::unordered_map<std::string, Zenith_DataAsset*> Zenith_DataAssetManager::s_xAssetCache;
Zenith_Mutex Zenith_DataAssetManager::s_xCacheMutex;

void Zenith_DataAssetManager::RegisterDataAssetType(const char* szTypeName, DataAssetFactoryFn pfnFactory)
{
	Zenith_ScopedMutexLock xLock(s_xRegistryMutex);
	s_xTypeRegistry[szTypeName] = pfnFactory;
	Zenith_Log(LOG_CATEGORY_ASSET, "Registered DataAsset type: %s", szTypeName);
}

Zenith_DataAsset* Zenith_DataAssetManager::CreateDataAsset(const char* szTypeName)
{
	Zenith_ScopedMutexLock xLock(s_xRegistryMutex);
	auto xIt = s_xTypeRegistry.find(szTypeName);
	if (xIt != s_xTypeRegistry.end())
	{
		return xIt->second();
	}
	Zenith_Log(LOG_CATEGORY_ASSET, "DataAsset type not registered: %s", szTypeName);
	return nullptr;
}

Zenith_DataAsset* Zenith_DataAssetManager::LoadDataAsset(const std::string& strPath)
{
	// Check cache first
	{
		Zenith_ScopedMutexLock xLock(s_xCacheMutex);
		auto xIt = s_xAssetCache.find(strPath);
		if (xIt != s_xAssetCache.end())
		{
			return xIt->second;
		}
	}

	// Open file
	std::ifstream xFile(strPath, std::ios::binary);
	if (!xFile.is_open())
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "Failed to open DataAsset file: %s", strPath.c_str());
		return nullptr;
	}

	// Read and validate magic number
	uint32_t uMagic = 0;
	xFile.read(reinterpret_cast<char*>(&uMagic), sizeof(uMagic));
	if (uMagic != ZDATA_MAGIC)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "Invalid DataAsset file (bad magic): %s", strPath.c_str());
		return nullptr;
	}

	// Read and validate version
	uint32_t uVersion = 0;
	xFile.read(reinterpret_cast<char*>(&uVersion), sizeof(uVersion));
	if (uVersion > ZDATA_VERSION)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "DataAsset file version %u is newer than supported (%u): %s",
			uVersion, ZDATA_VERSION, strPath.c_str());
		return nullptr;
	}

	// Read type name
	std::string strTypeName;
	char c;
	while (xFile.get(c) && c != '\0')
	{
		strTypeName += c;
	}

	if (strTypeName.empty())
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "DataAsset file has empty type name: %s", strPath.c_str());
		return nullptr;
	}

	// Create asset instance
	Zenith_DataAsset* pxAsset = CreateDataAsset(strTypeName.c_str());
	if (!pxAsset)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "Failed to create DataAsset of type '%s' from: %s",
			strTypeName.c_str(), strPath.c_str());
		return nullptr;
	}

	// Read remaining file data into buffer
	std::streampos xCurrentPos = xFile.tellg();
	xFile.seekg(0, std::ios::end);
	std::streamsize xDataSize = xFile.tellg() - xCurrentPos;
	xFile.seekg(xCurrentPos);

	if (xDataSize > 0)
	{
		std::vector<char> xBuffer(static_cast<size_t>(xDataSize));
		xFile.read(xBuffer.data(), xDataSize);

		// Create DataStream with external data and deserialize
		Zenith_DataStream xStream(xBuffer.data(), static_cast<uint64_t>(xDataSize));
		pxAsset->ReadFromDataStream(xStream);
	}

	pxAsset->SetFilePath(strPath);

	// Add to cache
	{
		Zenith_ScopedMutexLock xLock(s_xCacheMutex);
		s_xAssetCache[strPath] = pxAsset;
	}

	Zenith_Log(LOG_CATEGORY_ASSET, "Loaded DataAsset '%s' from: %s", strTypeName.c_str(), strPath.c_str());
	return pxAsset;
}

bool Zenith_DataAssetManager::SaveDataAsset(Zenith_DataAsset* pxAsset, const std::string& strPath)
{
	if (!pxAsset)
	{
		return false;
	}

	// Open file for writing
	std::ofstream xFile(strPath, std::ios::binary);
	if (!xFile.is_open())
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "Failed to create DataAsset file: %s", strPath.c_str());
		return false;
	}

	// Write magic number
	uint32_t uMagic = ZDATA_MAGIC;
	xFile.write(reinterpret_cast<const char*>(&uMagic), sizeof(uMagic));

	// Write version
	uint32_t uVersion = ZDATA_VERSION;
	xFile.write(reinterpret_cast<const char*>(&uVersion), sizeof(uVersion));

	// Write type name (null-terminated)
	const char* szTypeName = pxAsset->GetTypeName();
	xFile.write(szTypeName, strlen(szTypeName) + 1);

	// Serialize asset data
	Zenith_DataStream xStream;
	pxAsset->WriteToDataStream(xStream);

	// Write serialized data
	if (xStream.GetSize() > 0)
	{
		xFile.write(reinterpret_cast<const char*>(xStream.GetData()), xStream.GetSize());
	}

	pxAsset->SetFilePath(strPath);

	// Update cache
	{
		Zenith_ScopedMutexLock xLock(s_xCacheMutex);
		s_xAssetCache[strPath] = pxAsset;
	}

	Zenith_Log(LOG_CATEGORY_ASSET, "Saved DataAsset '%s' to: %s", szTypeName, strPath.c_str());
	return true;
}

bool Zenith_DataAssetManager::SaveDataAsset(Zenith_DataAsset* pxAsset)
{
	if (!pxAsset || pxAsset->GetFilePath().empty())
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "Cannot save DataAsset: no file path set");
		return false;
	}
	return SaveDataAsset(pxAsset, pxAsset->GetFilePath());
}

Zenith_DataAsset* Zenith_DataAssetManager::GetCachedAsset(const std::string& strPath)
{
	Zenith_ScopedMutexLock xLock(s_xCacheMutex);
	auto xIt = s_xAssetCache.find(strPath);
	if (xIt != s_xAssetCache.end())
	{
		return xIt->second;
	}
	return nullptr;
}

void Zenith_DataAssetManager::ClearCache()
{
	Zenith_ScopedMutexLock xLock(s_xCacheMutex);
	for (auto& xPair : s_xAssetCache)
	{
		delete xPair.second;
	}
	s_xAssetCache.clear();
}

bool Zenith_DataAssetManager::IsTypeRegistered(const char* szTypeName)
{
	Zenith_ScopedMutexLock xLock(s_xRegistryMutex);
	return s_xTypeRegistry.find(szTypeName) != s_xTypeRegistry.end();
}
