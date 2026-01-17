#include "Zenith.h"
#include "AssetHandling/Zenith_AssetRef.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "AssetHandling/Zenith_AsyncAssetLoader.h"
#include "Flux/Flux.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Prefab/Zenith_Prefab.h"

//--------------------------------------------------------------------------
// Template specialization: Zenith_TextureAsset
//--------------------------------------------------------------------------

template<>
Zenith_TextureAsset* Zenith_AssetRef<Zenith_TextureAsset>::LoadAsset(const std::string& strPath) const
{
	if (strPath.empty())
	{
		return nullptr;
	}

	// Load texture via registry
	Zenith_TextureAsset* pxTexture = Zenith_AssetRegistry::Get().Get<Zenith_TextureAsset>(strPath);
	if (!pxTexture)
	{
		Zenith_Error(LOG_CATEGORY_ASSET, "Failed to load texture from %s", strPath.c_str());
		return nullptr;
	}

	return pxTexture;
}

//--------------------------------------------------------------------------
// Template specialization: Zenith_MaterialAsset
//--------------------------------------------------------------------------

template<>
Zenith_MaterialAsset* Zenith_AssetRef<Zenith_MaterialAsset>::LoadAsset(const std::string& strPath) const
{
	if (strPath.empty())
	{
		return nullptr;
	}

	// Materials are loaded and cached via the AssetRegistry
	Zenith_MaterialAsset* pxMaterial = Zenith_AssetRegistry::Get().Get<Zenith_MaterialAsset>(strPath);
	if (!pxMaterial)
	{
		Zenith_Error(LOG_CATEGORY_ASSET, "Failed to load material from %s", strPath.c_str());
		return nullptr;
	}

	return pxMaterial;
}

//--------------------------------------------------------------------------
// Template specialization: Flux_MeshGeometry
//--------------------------------------------------------------------------

// Static storage for loaded meshes (since Flux_MeshGeometry doesn't have its own cache)
static std::unordered_map<std::string, Flux_MeshGeometry*> s_xMeshCache;
static Zenith_Mutex s_xMeshCacheMutex;

template<>
Flux_MeshGeometry* Zenith_AssetRef<Flux_MeshGeometry>::LoadAsset(const std::string& strPath) const
{
	if (strPath.empty())
	{
		return nullptr;
	}

	// Check cache first
	{
		Zenith_ScopedMutexLock xLock(s_xMeshCacheMutex);
		auto xIt = s_xMeshCache.find(strPath);
		if (xIt != s_xMeshCache.end())
		{
			return xIt->second;
		}
	}

	// Load from file
	Flux_MeshGeometry* pxMesh = new Flux_MeshGeometry();
	Flux_MeshGeometry::LoadFromFile(strPath.c_str(), *pxMesh);

	if (pxMesh->GetNumVerts() == 0)
	{
		Zenith_Error(LOG_CATEGORY_ASSET, "Failed to load mesh from %s", strPath.c_str());
		delete pxMesh;
		return nullptr;
	}

	// Add to cache
	{
		Zenith_ScopedMutexLock xLock(s_xMeshCacheMutex);
		s_xMeshCache[strPath] = pxMesh;
	}

	return pxMesh;
}

//--------------------------------------------------------------------------
// Template specialization: Zenith_ModelAsset
//--------------------------------------------------------------------------

// Static storage for loaded models
static std::unordered_map<std::string, Zenith_ModelAsset*> s_xModelCache;
static Zenith_Mutex s_xModelCacheMutex;

template<>
Zenith_ModelAsset* Zenith_AssetRef<Zenith_ModelAsset>::LoadAsset(const std::string& strPath) const
{
	if (strPath.empty())
	{
		return nullptr;
	}

	// Check cache first
	{
		Zenith_ScopedMutexLock xLock(s_xModelCacheMutex);
		auto xIt = s_xModelCache.find(strPath);
		if (xIt != s_xModelCache.end())
		{
			return xIt->second;
		}
	}

	// Load from file
	Zenith_ModelAsset* pxModel = new Zenith_ModelAsset();
	if (!pxModel->LoadFromFile(strPath.c_str()))
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "Failed to load model from %s", strPath.c_str());
		delete pxModel;
		return nullptr;
	}

	// Add to cache
	{
		Zenith_ScopedMutexLock xLock(s_xModelCacheMutex);
		s_xModelCache[strPath] = pxModel;
	}

	return pxModel;
}

//--------------------------------------------------------------------------
// Template specialization: Zenith_Prefab
//--------------------------------------------------------------------------

// Static storage for loaded prefabs
static std::unordered_map<std::string, Zenith_Prefab*> s_xPrefabCache;
static Zenith_Mutex s_xPrefabCacheMutex;

template<>
Zenith_Prefab* Zenith_AssetRef<Zenith_Prefab>::LoadAsset(const std::string& strPath) const
{
	if (strPath.empty())
	{
		return nullptr;
	}

	// Check cache first
	{
		Zenith_ScopedMutexLock xLock(s_xPrefabCacheMutex);
		auto xIt = s_xPrefabCache.find(strPath);
		if (xIt != s_xPrefabCache.end())
		{
			return xIt->second;
		}
	}

	// Load from file
	Zenith_Prefab* pxPrefab = new Zenith_Prefab();
	if (!pxPrefab->LoadFromFile(strPath))
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "Failed to load prefab from %s", strPath.c_str());
		delete pxPrefab;
		return nullptr;
	}

	// Add to cache
	{
		Zenith_ScopedMutexLock xLock(s_xPrefabCacheMutex);
		s_xPrefabCache[strPath] = pxPrefab;
	}

	return pxPrefab;
}

//--------------------------------------------------------------------------
// Cache cleanup functions (call at shutdown)
//--------------------------------------------------------------------------

namespace Zenith_AssetRefInternal
{
	void ClearMeshCache()
	{
		Zenith_ScopedMutexLock xLock(s_xMeshCacheMutex);
		for (auto& xPair : s_xMeshCache)
		{
			delete xPair.second;
		}
		s_xMeshCache.clear();
	}

	void ClearModelCache()
	{
		Zenith_ScopedMutexLock xLock(s_xModelCacheMutex);
		for (auto& xPair : s_xModelCache)
		{
			delete xPair.second;
		}
		s_xModelCache.clear();
	}

	void ClearPrefabCache()
	{
		Zenith_ScopedMutexLock xLock(s_xPrefabCacheMutex);
		for (auto& xPair : s_xPrefabCache)
		{
			delete xPair.second;
		}
		s_xPrefabCache.clear();
	}

	void ClearAllAssetRefCaches()
	{
		ClearMeshCache();
		ClearModelCache();
		ClearPrefabCache();
		// Note: Textures and Materials have their own cache systems
	}
}

//--------------------------------------------------------------------------
// Async Loading Method Implementations
//--------------------------------------------------------------------------

// Texture
template<>
void Zenith_AssetRef<Zenith_TextureAsset>::LoadAsync(
	AssetLoadCompleteFn pfnOnComplete,
	void* pxUserData,
	AssetLoadFailFn pfnOnFail)
{
	Zenith_AsyncAssetLoader::LoadAsync<Flux_Texture>(m_xGUID, pfnOnComplete, pxUserData, pfnOnFail);
}

template<>
bool Zenith_AssetRef<Zenith_TextureAsset>::IsReady() const
{
	if (m_pxCached.load(std::memory_order_acquire) != nullptr)
		return true;
	return Zenith_AsyncAssetLoader::GetLoadState(m_xGUID) == AssetLoadState::LOADED;
}

template<>
AssetLoadState Zenith_AssetRef<Zenith_TextureAsset>::GetLoadState() const
{
	if (m_pxCached.load(std::memory_order_acquire) != nullptr)
		return AssetLoadState::LOADED;
	return Zenith_AsyncAssetLoader::GetLoadState(m_xGUID);
}

// Material
template<>
void Zenith_AssetRef<Zenith_MaterialAsset>::LoadAsync(
	AssetLoadCompleteFn pfnOnComplete,
	void* pxUserData,
	AssetLoadFailFn pfnOnFail)
{
	Zenith_AsyncAssetLoader::LoadAsync<Zenith_MaterialAsset>(m_xGUID, pfnOnComplete, pxUserData, pfnOnFail);
}

template<>
bool Zenith_AssetRef<Zenith_MaterialAsset>::IsReady() const
{
	if (m_pxCached.load(std::memory_order_acquire) != nullptr)
		return true;
	return Zenith_AsyncAssetLoader::GetLoadState(m_xGUID) == AssetLoadState::LOADED;
}

template<>
AssetLoadState Zenith_AssetRef<Zenith_MaterialAsset>::GetLoadState() const
{
	if (m_pxCached.load(std::memory_order_acquire) != nullptr)
		return AssetLoadState::LOADED;
	return Zenith_AsyncAssetLoader::GetLoadState(m_xGUID);
}

// Mesh
template<>
void Zenith_AssetRef<Flux_MeshGeometry>::LoadAsync(
	AssetLoadCompleteFn pfnOnComplete,
	void* pxUserData,
	AssetLoadFailFn pfnOnFail)
{
	Zenith_AsyncAssetLoader::LoadAsync<Flux_MeshGeometry>(m_xGUID, pfnOnComplete, pxUserData, pfnOnFail);
}

template<>
bool Zenith_AssetRef<Flux_MeshGeometry>::IsReady() const
{
	if (m_pxCached.load(std::memory_order_acquire) != nullptr)
		return true;
	return Zenith_AsyncAssetLoader::GetLoadState(m_xGUID) == AssetLoadState::LOADED;
}

template<>
AssetLoadState Zenith_AssetRef<Flux_MeshGeometry>::GetLoadState() const
{
	if (m_pxCached.load(std::memory_order_acquire) != nullptr)
		return AssetLoadState::LOADED;
	return Zenith_AsyncAssetLoader::GetLoadState(m_xGUID);
}

// Model
template<>
void Zenith_AssetRef<Zenith_ModelAsset>::LoadAsync(
	AssetLoadCompleteFn pfnOnComplete,
	void* pxUserData,
	AssetLoadFailFn pfnOnFail)
{
	Zenith_AsyncAssetLoader::LoadAsync<Zenith_ModelAsset>(m_xGUID, pfnOnComplete, pxUserData, pfnOnFail);
}

template<>
bool Zenith_AssetRef<Zenith_ModelAsset>::IsReady() const
{
	if (m_pxCached.load(std::memory_order_acquire) != nullptr)
		return true;
	return Zenith_AsyncAssetLoader::GetLoadState(m_xGUID) == AssetLoadState::LOADED;
}

template<>
AssetLoadState Zenith_AssetRef<Zenith_ModelAsset>::GetLoadState() const
{
	if (m_pxCached.load(std::memory_order_acquire) != nullptr)
		return AssetLoadState::LOADED;
	return Zenith_AsyncAssetLoader::GetLoadState(m_xGUID);
}

// Prefab
template<>
void Zenith_AssetRef<Zenith_Prefab>::LoadAsync(
	AssetLoadCompleteFn pfnOnComplete,
	void* pxUserData,
	AssetLoadFailFn pfnOnFail)
{
	Zenith_AsyncAssetLoader::LoadAsync<Zenith_Prefab>(m_xGUID, pfnOnComplete, pxUserData, pfnOnFail);
}

template<>
bool Zenith_AssetRef<Zenith_Prefab>::IsReady() const
{
	if (m_pxCached.load(std::memory_order_acquire) != nullptr)
		return true;
	return Zenith_AsyncAssetLoader::GetLoadState(m_xGUID) == AssetLoadState::LOADED;
}

template<>
AssetLoadState Zenith_AssetRef<Zenith_Prefab>::GetLoadState() const
{
	if (m_pxCached.load(std::memory_order_acquire) != nullptr)
		return AssetLoadState::LOADED;
	return Zenith_AsyncAssetLoader::GetLoadState(m_xGUID);
}
