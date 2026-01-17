#include "Zenith.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_Asset.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_MeshAsset.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"
#include "AssetHandling/Zenith_ModelAsset.h"

// Forward declare loaders (defined in respective asset .cpp files)
static Zenith_Asset* LoadTextureAsset(const std::string& strPath);
static Zenith_Asset* LoadMaterialAsset(const std::string& strPath);
static Zenith_Asset* LoadMeshAsset(const std::string& strPath);
static Zenith_Asset* LoadSkeletonAsset(const std::string& strPath);
static Zenith_Asset* LoadModelAsset(const std::string& strPath);

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

// Static instance
Zenith_AssetRegistry* Zenith_AssetRegistry::s_pxInstance = nullptr;

Zenith_AssetRegistry& Zenith_AssetRegistry::Get()
{
	ZENITH_ASSERT(s_pxInstance != nullptr, "Zenith_AssetRegistry not initialized! Call Initialize() first.");
	return *s_pxInstance;
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

	// Check cache first
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

	// Load the asset
	Zenith_Asset* pxAsset = itLoader->second(strPath);
	if (pxAsset == nullptr)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "AssetRegistry: Failed to load asset '%s'", strPath.c_str());
		return nullptr;
	}

	// Set path and add to cache
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

std::string Zenith_AssetRegistry::GenerateProceduralPath(const std::string& strPrefix)
{
	return "procedural://" + strPrefix + "_" + std::to_string(m_uNextProceduralId++);
}
