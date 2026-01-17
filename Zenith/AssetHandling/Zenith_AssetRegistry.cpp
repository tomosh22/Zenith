#include "Zenith.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_Asset.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_MeshAsset.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "AssetHandling/Zenith_AnimationAsset.h"
#include "AssetHandling/Zenith_MeshGeometryAsset.h"
#include "Prefab/Zenith_Prefab.h"

// Forward declare loaders (defined in respective asset .cpp files)
static Zenith_Asset* LoadTextureAsset(const std::string& strPath);
static Zenith_Asset* LoadMaterialAsset(const std::string& strPath);
static Zenith_Asset* LoadMeshAsset(const std::string& strPath);
static Zenith_Asset* LoadSkeletonAsset(const std::string& strPath);
static Zenith_Asset* LoadModelAsset(const std::string& strPath);
static Zenith_Asset* LoadPrefabAsset(const std::string& strPath);
static Zenith_Asset* LoadAnimationAsset(const std::string& strPath);
static Zenith_Asset* LoadMeshGeometryAsset(const std::string& strPath);

// Loader implementations
static Zenith_Asset* LoadTextureAsset(const std::string& strPath)
{
	if (strPath.empty())
	{
		// Create empty procedural texture
		return new Zenith_TextureAsset();
	}

	Zenith_TextureAsset* pxAsset = new Zenith_TextureAsset();
	if (!pxAsset->LoadFromFile(strPath, true))
	{
		delete pxAsset;
		return nullptr;
	}
	return pxAsset;
}

static Zenith_Asset* LoadMaterialAsset(const std::string& strPath)
{
	if (strPath.empty())
	{
		// Create empty material
		return new Zenith_MaterialAsset();
	}

	Zenith_MaterialAsset* pxAsset = new Zenith_MaterialAsset();
	if (!pxAsset->LoadFromFile(strPath))
	{
		delete pxAsset;
		return nullptr;
	}
	return pxAsset;
}

static Zenith_Asset* LoadMeshAsset(const std::string& strPath)
{
	if (strPath.empty())
	{
		// Create empty mesh
		return new Zenith_MeshAsset();
	}

	Zenith_MeshAsset* pxAsset = Zenith_MeshAsset::LoadFromFile(strPath.c_str());
	return pxAsset;
}

static Zenith_Asset* LoadSkeletonAsset(const std::string& strPath)
{
	if (strPath.empty())
	{
		// Create empty skeleton
		return new Zenith_SkeletonAsset();
	}

	Zenith_SkeletonAsset* pxAsset = Zenith_SkeletonAsset::LoadFromFile(strPath.c_str());
	return pxAsset;
}

static Zenith_Asset* LoadModelAsset(const std::string& strPath)
{
	if (strPath.empty())
	{
		// Create empty model
		return new Zenith_ModelAsset();
	}

	Zenith_ModelAsset* pxAsset = Zenith_ModelAsset::LoadFromFile(strPath.c_str());
	return pxAsset;
}

static Zenith_Asset* LoadPrefabAsset(const std::string& strPath)
{
	if (strPath.empty())
	{
		// Create empty prefab
		return new Zenith_Prefab();
	}

	Zenith_Prefab* pxAsset = new Zenith_Prefab();
	if (!pxAsset->LoadFromFile(strPath))
	{
		delete pxAsset;
		return nullptr;
	}
	return pxAsset;
}

static Zenith_Asset* LoadAnimationAsset(const std::string& strPath)
{
	if (strPath.empty())
	{
		// Create empty animation asset (for procedural animations)
		return new Zenith_AnimationAsset();
	}

	// Procedural assets are created via Create(), not loaded
	if (strPath.find("procedural://") == 0)
	{
		return nullptr;
	}

	Zenith_AnimationAsset* pxAsset = new Zenith_AnimationAsset();
	if (!pxAsset->LoadFromFile(strPath))
	{
		delete pxAsset;
		return nullptr;
	}
	return pxAsset;
}

static Zenith_Asset* LoadMeshGeometryAsset(const std::string& strPath)
{
	if (strPath.empty())
	{
		// Create empty mesh geometry asset (for procedural geometry)
		return new Zenith_MeshGeometryAsset();
	}

	// Procedural assets are created via Create(), not loaded
	if (strPath.find("procedural://") == 0)
	{
		return nullptr;
	}

	Zenith_MeshGeometryAsset* pxAsset = new Zenith_MeshGeometryAsset();
	if (!pxAsset->LoadFromFile(strPath))
	{
		delete pxAsset;
		return nullptr;
	}
	return pxAsset;
}

// Static instance
Zenith_AssetRegistry* Zenith_AssetRegistry::s_pxInstance = nullptr;
std::string Zenith_AssetRegistry::s_strGameAssetsDir;
std::string Zenith_AssetRegistry::s_strEngineAssetsDir;

Zenith_AssetRegistry& Zenith_AssetRegistry::Get()
{
	ZENITH_ASSERT(s_pxInstance != nullptr, "Zenith_AssetRegistry not initialized! Call Initialize() first.");
	return *s_pxInstance;
}

void Zenith_AssetRegistry::SetGameAssetsDir(const std::string& strPath)
{
	s_strGameAssetsDir = strPath;
	// Normalize path separators to forward slashes
	for (char& c : s_strGameAssetsDir)
	{
		if (c == '\\')
		{
			c = '/';
		}
	}
	// Remove trailing slash if present
	if (!s_strGameAssetsDir.empty() && s_strGameAssetsDir.back() == '/')
	{
		s_strGameAssetsDir.pop_back();
	}
}

void Zenith_AssetRegistry::SetEngineAssetsDir(const std::string& strPath)
{
	s_strEngineAssetsDir = strPath;
	// Normalize path separators to forward slashes
	for (char& c : s_strEngineAssetsDir)
	{
		if (c == '\\')
		{
			c = '/';
		}
	}
	// Remove trailing slash if present
	if (!s_strEngineAssetsDir.empty() && s_strEngineAssetsDir.back() == '/')
	{
		s_strEngineAssetsDir.pop_back();
	}
}

std::string Zenith_AssetRegistry::ResolvePath(const std::string& strPrefixedPath)
{
	// Check for game: prefix
	if (strPrefixedPath.size() > 5 && strPrefixedPath.compare(0, 5, "game:") == 0)
	{
		return s_strGameAssetsDir + "/" + strPrefixedPath.substr(5);
	}

	// Check for engine: prefix
	if (strPrefixedPath.size() > 7 && strPrefixedPath.compare(0, 7, "engine:") == 0)
	{
		return s_strEngineAssetsDir + "/" + strPrefixedPath.substr(7);
	}

	// Check for procedural: prefix (these don't resolve to files)
	if (strPrefixedPath.size() > 12 && strPrefixedPath.compare(0, 12, "procedural://") == 0)
	{
		return strPrefixedPath; // Return as-is
	}

	// No prefix - treat as absolute path (legacy support or already absolute)
	return strPrefixedPath;
}

std::string Zenith_AssetRegistry::MakeRelativePath(const std::string& strAbsolutePath)
{
	// Normalize the input path
	std::string strNormalized = strAbsolutePath;
	for (char& c : strNormalized)
	{
		if (c == '\\')
		{
			c = '/';
		}
	}

	// Check if it's under the game assets directory
	if (!s_strGameAssetsDir.empty() && strNormalized.size() > s_strGameAssetsDir.size())
	{
		if (strNormalized.compare(0, s_strGameAssetsDir.size(), s_strGameAssetsDir) == 0)
		{
			// Check for path separator after the directory
			if (strNormalized[s_strGameAssetsDir.size()] == '/')
			{
				return "game:" + strNormalized.substr(s_strGameAssetsDir.size() + 1);
			}
		}
	}

	// Check if it's under the engine assets directory
	if (!s_strEngineAssetsDir.empty() && strNormalized.size() > s_strEngineAssetsDir.size())
	{
		if (strNormalized.compare(0, s_strEngineAssetsDir.size(), s_strEngineAssetsDir) == 0)
		{
			// Check for path separator after the directory
			if (strNormalized[s_strEngineAssetsDir.size()] == '/')
			{
				return "engine:" + strNormalized.substr(s_strEngineAssetsDir.size() + 1);
			}
		}
	}

	// Not in a known directory - return empty string
	return "";
}

void Zenith_AssetRegistry::Initialize()
{
	ZENITH_ASSERT(s_pxInstance == nullptr, "Zenith_AssetRegistry already initialized!");
	s_pxInstance = new Zenith_AssetRegistry();

	// Register asset loaders
	s_pxInstance->RegisterLoader(std::type_index(typeid(Zenith_TextureAsset)), LoadTextureAsset);
	s_pxInstance->RegisterLoader(std::type_index(typeid(Zenith_MaterialAsset)), LoadMaterialAsset);
	s_pxInstance->RegisterLoader(std::type_index(typeid(Zenith_MeshAsset)), LoadMeshAsset);
	s_pxInstance->RegisterLoader(std::type_index(typeid(Zenith_SkeletonAsset)), LoadSkeletonAsset);
	s_pxInstance->RegisterLoader(std::type_index(typeid(Zenith_ModelAsset)), LoadModelAsset);
	s_pxInstance->RegisterLoader(std::type_index(typeid(Zenith_Prefab)), LoadPrefabAsset);
	s_pxInstance->RegisterLoader(std::type_index(typeid(Zenith_AnimationAsset)), LoadAnimationAsset);
	s_pxInstance->RegisterLoader(std::type_index(typeid(Zenith_MeshGeometryAsset)), LoadMeshGeometryAsset);

	// Note: Zenith_MaterialAsset::InitializeDefaults() must be called AFTER Vulkan/VMA
	// is initialized (after Flux::EarlyInitialise). See InitializeGPUDependentAssets().

	Zenith_Log(LOG_CATEGORY_ASSET, "AssetRegistry initialized");
}

void Zenith_AssetRegistry::InitializeGPUDependentAssets()
{
	// Initialize material default textures - requires VMA to be initialized
	Zenith_MaterialAsset::InitializeDefaults();

	Zenith_Log(LOG_CATEGORY_ASSET, "AssetRegistry GPU-dependent assets initialized");
}

void Zenith_AssetRegistry::Shutdown()
{
	if (s_pxInstance)
	{
		// Shutdown material defaults before unloading assets
		Zenith_MaterialAsset::ShutdownDefaults();

		s_pxInstance->UnloadAll();
		delete s_pxInstance;
		s_pxInstance = nullptr;

		Zenith_Log(LOG_CATEGORY_ASSET, "AssetRegistry shutdown");
	}
}

bool Zenith_AssetRegistry::IsLoaded(const std::string& strPath) const
{
	Zenith_ScopedMutexLock xLock(m_xMutex);
	return m_xAssetsByPath.find(strPath) != m_xAssetsByPath.end();
}

void Zenith_AssetRegistry::Unload(const std::string& strPath)
{
	Zenith_ScopedMutexLock xLock(m_xMutex);

	auto it = m_xAssetsByPath.find(strPath);
	if (it != m_xAssetsByPath.end())
	{
		Zenith_Asset* pxAsset = it->second;

		if (m_bLifecycleLogging)
		{
			Zenith_Log(LOG_CATEGORY_ASSET, "AssetRegistry: Force unloading '%s' (ref count: %u)", strPath.c_str(), pxAsset->GetRefCount());
		}

		delete pxAsset;
		m_xAssetsByPath.erase(it);
	}
}

void Zenith_AssetRegistry::UnloadUnused()
{
	Zenith_ScopedMutexLock xLock(m_xMutex);

	// Collect assets with ref count 0
	std::vector<std::string> xToRemove;
	for (const auto& xPair : m_xAssetsByPath)
	{
		if (xPair.second->GetRefCount() == 0)
		{
			xToRemove.push_back(xPair.first);
		}
	}

	// Delete them
	for (const std::string& strPath : xToRemove)
	{
		if (m_bLifecycleLogging)
		{
			Zenith_Log(LOG_CATEGORY_ASSET, "AssetRegistry: Unloading unused asset '%s'", strPath.c_str());
		}

		delete m_xAssetsByPath[strPath];
		m_xAssetsByPath.erase(strPath);
	}

	if (m_bLifecycleLogging && !xToRemove.empty())
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "AssetRegistry: Unloaded %zu unused assets", xToRemove.size());
	}
}

void Zenith_AssetRegistry::UnloadAll()
{
	Zenith_ScopedMutexLock xLock(m_xMutex);

	if (m_bLifecycleLogging)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "AssetRegistry: Unloading all %zu assets", m_xAssetsByPath.size());
	}

	for (auto& xPair : m_xAssetsByPath)
	{
		if (m_bLifecycleLogging && xPair.second->GetRefCount() > 0)
		{
			Zenith_Log(LOG_CATEGORY_ASSET, "AssetRegistry: Warning - unloading '%s' with ref count %u",
				xPair.first.c_str(), xPair.second->GetRefCount());
		}

		delete xPair.second;
	}

	m_xAssetsByPath.clear();
}

uint32_t Zenith_AssetRegistry::GetLoadedAssetCount() const
{
	Zenith_ScopedMutexLock xLock(m_xMutex);
	return static_cast<uint32_t>(m_xAssetsByPath.size());
}

void Zenith_AssetRegistry::LogLoadedAssets() const
{
	Zenith_ScopedMutexLock xLock(m_xMutex);

	Zenith_Log(LOG_CATEGORY_ASSET, "=== Loaded Assets (%zu total) ===", m_xAssetsByPath.size());

	for (const auto& xPair : m_xAssetsByPath)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "  [ref=%u] %s", xPair.second->GetRefCount(), xPair.first.c_str());
	}

	Zenith_Log(LOG_CATEGORY_ASSET, "=================================");
}

void Zenith_AssetRegistry::RegisterLoader(std::type_index xType, AssetLoaderFn pfnLoader)
{
	m_xLoaders[xType] = pfnLoader;
}

Zenith_Asset* Zenith_AssetRegistry::GetInternal(std::type_index xType, const std::string& strPath)
{
	if (strPath.empty())
	{
		return nullptr;
	}

	Zenith_ScopedMutexLock xLock(m_xMutex);

	// Check cache first (using the prefixed path as key for portability)
	auto itAsset = m_xAssetsByPath.find(strPath);
	if (itAsset != m_xAssetsByPath.end())
	{
		return itAsset->second;
	}

	// Find loader for this type
	auto itLoader = m_xLoaders.find(xType);
	if (itLoader == m_xLoaders.end())
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "AssetRegistry: No loader registered for type");
		return nullptr;
	}

	// Resolve prefixed path to absolute path for file loading
	std::string strAbsolutePath = ResolvePath(strPath);

	// Load the asset using the absolute path
	Zenith_Asset* pxAsset = itLoader->second(strAbsolutePath);
	if (pxAsset == nullptr)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "AssetRegistry: Failed to load asset '%s' (resolved: '%s')", strPath.c_str(), strAbsolutePath.c_str());
		return nullptr;
	}

	// Store the prefixed path (portable) in the asset and cache
	pxAsset->m_strPath = strPath;
	m_xAssetsByPath[strPath] = pxAsset;

	if (m_bLifecycleLogging)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "AssetRegistry: Loaded asset '%s'", strPath.c_str());
	}

	return pxAsset;
}

Zenith_Asset* Zenith_AssetRegistry::CreateInternal(std::type_index xType)
{
	Zenith_ScopedMutexLock xLock(m_xMutex);

	// Find loader - use empty path to indicate creation
	auto itLoader = m_xLoaders.find(xType);
	if (itLoader == m_xLoaders.end())
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "AssetRegistry: No loader registered for type");
		return nullptr;
	}

	// Generate unique procedural path
	std::string strPath = GenerateProceduralPath("asset");

	// Create the asset (loader should handle empty path as "create new")
	Zenith_Asset* pxAsset = itLoader->second("");
	if (pxAsset == nullptr)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "AssetRegistry: Failed to create procedural asset");
		return nullptr;
	}

	// Set path and add to cache
	pxAsset->m_strPath = strPath;
	m_xAssetsByPath[strPath] = pxAsset;

	if (m_bLifecycleLogging)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "AssetRegistry: Created procedural asset '%s'", strPath.c_str());
	}

	return pxAsset;
}

Zenith_Asset* Zenith_AssetRegistry::CreateInternal(std::type_index xType, const std::string& strPath)
{
	Zenith_ScopedMutexLock xLock(m_xMutex);

	// Find loader
	auto itLoader = m_xLoaders.find(xType);
	if (itLoader == m_xLoaders.end())
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "AssetRegistry: No loader registered for type");
		return nullptr;
	}

	// Create the asset (loader should handle empty path as "create new")
	Zenith_Asset* pxAsset = itLoader->second("");
	if (pxAsset == nullptr)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "AssetRegistry: Failed to create procedural asset");
		return nullptr;
	}

	// Set the specified path and add to cache
	pxAsset->m_strPath = strPath;
	m_xAssetsByPath[strPath] = pxAsset;

	if (m_bLifecycleLogging)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "AssetRegistry: Created procedural asset '%s'", strPath.c_str());
	}

	return pxAsset;
}

std::string Zenith_AssetRegistry::GenerateProceduralPath(const std::string& strPrefix)
{
	return "procedural://" + strPrefix + "_" + std::to_string(m_uNextProceduralId++);
}
