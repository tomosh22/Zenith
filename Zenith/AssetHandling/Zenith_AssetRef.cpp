#include "Zenith.h"
#include "AssetHandling/Zenith_AssetRef.h"
#include "AssetHandling/Zenith_AssetHandler.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "Flux/Flux.h"
#include "Flux/Flux_MaterialAsset.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Prefab/Zenith_Prefab.h"

//--------------------------------------------------------------------------
// Template specialization: Flux_Texture
//--------------------------------------------------------------------------

template<>
Flux_Texture* Zenith_AssetRef<Flux_Texture>::LoadAsset(const std::string& strPath) const
{
	if (strPath.empty())
	{
		return nullptr;
	}

	// Load texture data from file
	Zenith_AssetHandler::TextureData xTexData = Zenith_AssetHandler::LoadTexture2DFromFile(strPath.c_str());
	if (!xTexData.pData)
	{
		Zenith_Error(LOG_CATEGORY_ASSET, "Failed to load texture from %s", strPath.c_str());
		return nullptr;
	}

	// Create GPU texture
	Flux_Texture* pxTexture = Zenith_AssetHandler::AddTexture(xTexData);
	xTexData.FreeAllocatedData();

	if (!pxTexture)
	{
		Zenith_Error(LOG_CATEGORY_ASSET, "Failed to create texture from %s", strPath.c_str());
		return nullptr;
	}

	return pxTexture;
}

//--------------------------------------------------------------------------
// Template specialization: Flux_MaterialAsset
//--------------------------------------------------------------------------

template<>
Flux_MaterialAsset* Zenith_AssetRef<Flux_MaterialAsset>::LoadAsset(const std::string& strPath) const
{
	if (strPath.empty())
	{
		return nullptr;
	}

	// Materials are cached by the MaterialAsset system
	Flux_MaterialAsset* pxMaterial = Flux_MaterialAsset::LoadFromFile(strPath);
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
