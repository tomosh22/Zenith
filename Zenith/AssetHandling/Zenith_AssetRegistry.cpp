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
#include "AssetHandling/Zenith_FontAsset.h"
#include "AssetHandling/Zenith_BehaviourGraphAsset.h"
#include "Prefab/Zenith_Prefab.h"
#include "Collections/Zenith_Vector.h"
#include "Profiling/Zenith_Profiling.h"
#include <fstream>

// Unified asset loaders
//
// Every typed file asset is loaded through ONE of two templates instead of a
// per-type free function. Both return Zenith_Result<Zenith_Asset*>: the loaded
// asset on success, or a specific Zenith_ErrorCode on failure. The Result lives
// only inside this asset-loading boundary; the public Get<T>()/Create<T>() facade
// still hands callers a raw T* (nullptr on failure), so nothing outside
// AssetHandling changes.
//
// The only real per-type differences — whether a procedural:// path is rejected,
// and the exact default args LoadFromFile wants — live in the trait below.

// Member-contract trait: the asset exposes a private Zenith_Status LoadFromFile(...).
// Specialize only where a type differs from "guard off, single-arg LoadFromFile".
template<typename T>
struct Zenith_AssetLoadTraits
{
	static constexpr bool kGuardProcedural = false;
	static Zenith_Status DoLoad(T* pxAsset, const std::string& strPath) { return pxAsset->LoadFromFile(strPath); }
};

// Texture loads WITH runtime mip generation by default (the registry contract).
template<>
struct Zenith_AssetLoadTraits<Zenith_TextureAsset>
{
	static constexpr bool kGuardProcedural = false;
	static Zenith_Status DoLoad(Zenith_TextureAsset* pxAsset, const std::string& strPath) { return pxAsset->LoadFromFile(strPath, /*bCreateMips*/ true); }
};

// Animation + MeshGeometry are created procedurally via Create<T>(), never loaded
// from a procedural:// path — reject it (preserves the historical INVALID_ARGUMENT).
template<>
struct Zenith_AssetLoadTraits<Zenith_AnimationAsset>
{
	static constexpr bool kGuardProcedural = true;
	static Zenith_Status DoLoad(Zenith_AnimationAsset* pxAsset, const std::string& strPath) { return pxAsset->LoadFromFile(strPath); }
};

template<>
struct Zenith_AssetLoadTraits<Zenith_MeshGeometryAsset>
{
	static constexpr bool kGuardProcedural = true;
	static Zenith_Status DoLoad(Zenith_MeshGeometryAsset* pxAsset, const std::string& strPath) { return pxAsset->LoadFromFile(strPath); }
};

// Member-contract loader: empty path -> a fresh procedural instance (the Create<T>()
// path); else new -> LoadFromFile -> delete-on-failure.
template<typename T>
Zenith_Result<Zenith_Asset*> LoadAssetGeneric(const std::string& strPath)
{
	if (strPath.empty())
	{
		return static_cast<Zenith_Asset*>(new T());
	}
	if constexpr (Zenith_AssetLoadTraits<T>::kGuardProcedural)
	{
		// Guarded types (Animation, MeshGeometry) are created via Create<T>(), never
		// loaded from a procedural:// path — reject it as the old loaders did.
		if (strPath.find("procedural://") == 0)
		{
			return Zenith_ErrorCode::INVALID_ARGUMENT;
		}
	}
	T* pxAsset = new T();
	Zenith_Status xStatus = Zenith_AssetLoadTraits<T>::DoLoad(pxAsset, strPath);
	if (!xStatus.IsOk())
	{
		delete pxAsset;
		return xStatus.Error();
	}
	return static_cast<Zenith_Asset*>(pxAsset);
}

// Static-factory loader: Mesh/Skeleton/Model own the new+parse internally and return
// Zenith_Result<T*> from a private static LoadFromFile. Empty path -> fresh instance.
template<typename T>
Zenith_Result<Zenith_Asset*> LoadAssetViaStaticFactory(const std::string& strPath)
{
	if (strPath.empty())
	{
		return static_cast<Zenith_Asset*>(new T());
	}
	Zenith_Result<T*> xRes = T::LoadFromFile(strPath.c_str());
	if (!xRes.IsOk())
	{
		return xRes.Error();
	}
	return static_cast<Zenith_Asset*>(xRes.Value());
}

// Static instance
Zenith_AssetRegistry* Zenith_AssetRegistry::s_pxInstance = nullptr;
std::string Zenith_AssetRegistry::s_strGameAssetsDir;
std::string Zenith_AssetRegistry::s_strEngineAssetsDir;

// Serializable asset type registry - uses function-local statics to avoid static initialization order fiasco
Zenith_HashMap<std::string, Zenith_AssetRegistry::SerializableAssetFactoryFn>& Zenith_AssetRegistry::GetSerializableTypeRegistry()
{
	static Zenith_HashMap<std::string, SerializableAssetFactoryFn> s_xRegistry;
	return s_xRegistry;
}

Zenith_Mutex_NoProfiling& Zenith_AssetRegistry::GetSerializableTypeRegistryMutex()
{
	static Zenith_Mutex_NoProfiling s_xMutex;
	return s_xMutex;
}

// Normalize path separators to forward slashes and strip any trailing slash so
// downstream string concatenations produce a clean `<dir>/<file>` path.
static void NormalizeAssetDirPath(std::string& strPathInOut)
{
	for (char& c : strPathInOut)
	{
		if (c == '\\')
		{
			c = '/';
		}
	}
	if (!strPathInOut.empty() && strPathInOut.back() == '/')
	{
		strPathInOut.pop_back();
	}
}

void Zenith_AssetRegistry::SetGameAssetsDir(const std::string& strPath)
{
	s_strGameAssetsDir = strPath;
	NormalizeAssetDirPath(s_strGameAssetsDir);
}

void Zenith_AssetRegistry::SetEngineAssetsDir(const std::string& strPath)
{
	s_strEngineAssetsDir = strPath;
	NormalizeAssetDirPath(s_strEngineAssetsDir);
}

std::string Zenith_AssetRegistry::ResolveAssetsDir(const std::string& strBakedDir, const char* szOverrideRoot, const std::string& strRelativeUnderRoot)
{
	if (szOverrideRoot == nullptr || szOverrideRoot[0] == '\0')
	{
		return strBakedDir;
	}
	std::string strRoot(szOverrideRoot);
	// Trim a trailing separator off the root so the join never doubles up
	// (`run.bat` passes "%~dp0", which ends in a backslash).
	while (!strRoot.empty() && (strRoot.back() == '/' || strRoot.back() == '\\'))
	{
		strRoot.pop_back();
	}
	return strRoot + "/" + strRelativeUnderRoot;
}

std::string Zenith_AssetRegistry::ResolvePath(const std::string& strPrefixedPath)
{
	// Check for game: prefix
	if (strPrefixedPath.size() > 5 && strPrefixedPath.compare(0, 5, "game:") == 0)
	{
		std::string strRelative = strPrefixedPath.substr(5);
		if (s_strGameAssetsDir.empty())
		{
			return strRelative;
		}
		return s_strGameAssetsDir + "/" + strRelative;
	}

	// Check for engine: prefix
	if (strPrefixedPath.size() > 7 && strPrefixedPath.compare(0, 7, "engine:") == 0)
	{
		std::string strRelative = strPrefixedPath.substr(7);
		if (s_strEngineAssetsDir.empty())
		{
			return strRelative;
		}
		return s_strEngineAssetsDir + "/" + strRelative;
	}

	// Check for procedural: prefix (these don't resolve to files)
	if (strPrefixedPath.size() > 12 && strPrefixedPath.compare(0, 12, "procedural://") == 0)
	{
		return strPrefixedPath; // Return as-is
	}

	// No prefix - treat as absolute or relative path
	// Normalize backslashes to forward slashes for consistency
	std::string strNormalized = strPrefixedPath;
	for (char& c : strNormalized)
	{
		if (c == '\\')
		{
			c = '/';
		}
	}
	return strNormalized;
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

std::string Zenith_AssetRegistry::NormalizeAssetPath(const std::string& strPath)
{
	if (strPath.empty())
	{
		return strPath;
	}

	// Already has a recognized prefix - no normalization needed
	if (strPath.compare(0, 5, "game:") == 0 ||
		strPath.compare(0, 7, "engine:") == 0 ||
		strPath.compare(0, 12, "procedural://") == 0)
	{
		return strPath;
	}

	// Try to convert absolute path to prefixed relative path
	std::string strRelative = MakeRelativePath(strPath);
	return strRelative.empty() ? strPath : strRelative;
}

void Zenith_AssetRegistry::Initialize()
{
	// Zenith_Engine owns the instance and installs s_pxInstance before
	// calling here; this function only registers loaders.
	Zenith_Assert(s_pxInstance != nullptr, "Zenith_AssetRegistry::Initialize called before Zenith_Engine bound s_pxInstance");

	// Register asset loaders. Member-contract assets (private LoadFromFile -> Status)
	// go through LoadAssetGeneric<T>; the static-factory trio (private static
	// LoadFromFile -> Result<T*>) through LoadAssetViaStaticFactory<T>. Both decay to
	// the plain AssetLoaderFn function pointer (no std::function — house rule).
	s_pxInstance->RegisterLoader(Zenith_TypeIndex::Of<Zenith_TextureAsset>(), &LoadAssetGeneric<Zenith_TextureAsset>);
	s_pxInstance->RegisterLoader(Zenith_TypeIndex::Of<Zenith_MaterialAsset>(), &LoadAssetGeneric<Zenith_MaterialAsset>);
	s_pxInstance->RegisterLoader(Zenith_TypeIndex::Of<Zenith_MeshAsset>(), &LoadAssetViaStaticFactory<Zenith_MeshAsset>);
	s_pxInstance->RegisterLoader(Zenith_TypeIndex::Of<Zenith_SkeletonAsset>(), &LoadAssetViaStaticFactory<Zenith_SkeletonAsset>);
	s_pxInstance->RegisterLoader(Zenith_TypeIndex::Of<Zenith_ModelAsset>(), &LoadAssetViaStaticFactory<Zenith_ModelAsset>);
	s_pxInstance->RegisterLoader(Zenith_TypeIndex::Of<Zenith_Prefab>(), &LoadAssetGeneric<Zenith_Prefab>);
	s_pxInstance->RegisterLoader(Zenith_TypeIndex::Of<Zenith_AnimationAsset>(), &LoadAssetGeneric<Zenith_AnimationAsset>);
	s_pxInstance->RegisterLoader(Zenith_TypeIndex::Of<Zenith_MeshGeometryAsset>(), &LoadAssetGeneric<Zenith_MeshGeometryAsset>);
	s_pxInstance->RegisterLoader(Zenith_TypeIndex::Of<Zenith_FontAsset>(), &LoadAssetGeneric<Zenith_FontAsset>);

	// Behaviour Graph assets (.bgraph). RegisterAssetType wires BOTH the
	// serializable-type factory and the typed loader - the static-init
	// ZENITH_REGISTER_ASSET_TYPE in Zenith_BehaviourGraphAsset.cpp runs before
	// s_pxInstance is bound, so the loader half must be registered here.
	RegisterAssetType<Zenith_BehaviourGraphAsset>();

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
	// Drains state only; Zenith_Engine::Shutdown deletes the instance and
	// clears s_pxInstance after we return.
	if (s_pxInstance)
	{
		// Shutdown material defaults before unloading assets
		Zenith_MaterialAsset::ShutdownDefaults();

		s_pxInstance->UnloadAll();

		Zenith_Log(LOG_CATEGORY_ASSET, "AssetRegistry shutdown");
	}
}

bool Zenith_AssetRegistry::IsLoadedInternal(const std::string& strPath) const
{
	Zenith_ScopedMutexLock xLock(m_xMutex);
	return m_xAssetsByPath.Contains(strPath);
}

void Zenith_AssetRegistry::ForceUnloadInternal(const std::string& strPath)
{
	Zenith_ScopedMutexLock xLock(m_xMutex);

	Zenith_Asset** ppxAsset = m_xAssetsByPath.TryGet(strPath);
	if (ppxAsset != nullptr)
	{
		Zenith_Asset* pxAsset = *ppxAsset;

		if (m_bLifecycleLogging)
		{
			Zenith_Log(LOG_CATEGORY_ASSET, "AssetRegistry: Force unloading '%s' (ref count: %u)", strPath.c_str(), pxAsset->GetRefCount());
		}

		// Bind the cache key to a stable local BEFORE deleting the asset. Callers
		// routinely pass pxAsset->GetPath(), which is a reference INTO the asset's
		// own m_strPath — after `delete pxAsset` that reference dangles, so removing
		// with `strPath` directly would hash/compare freed memory (heap corruption).
		// (UnloadUnused is immune: it removes by copied keys collected up-front.)
		const std::string strKey = strPath;
		delete pxAsset;
		m_xAssetsByPath.Remove(strKey);
	}
}

void Zenith_AssetRegistry::UnloadUnusedInternal()
{
	Zenith_ScopedMutexLock xLock(m_xMutex);

	// Collect assets with ref count 0
	Zenith_Vector<std::string> xToRemove;
	for (Zenith_HashMap<std::string, Zenith_Asset*>::Iterator xIt(m_xAssetsByPath); !xIt.Done(); xIt.Next())
	{
		if (xIt.GetValue()->GetRefCount() == 0)
		{
			xToRemove.PushBack(xIt.GetKey());
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
		m_xAssetsByPath.Remove(strPath);
	}

	if (m_bLifecycleLogging && xToRemove.GetSize() > 0)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "AssetRegistry: Unloaded %u unused assets", xToRemove.GetSize());
	}
}

void Zenith_AssetRegistry::UnloadAllInternal()
{
	Zenith_ScopedMutexLock xLock(m_xMutex);

	if (m_bLifecycleLogging)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "AssetRegistry: Unloading all %u assets", m_xAssetsByPath.GetSize());
	}

	// Phase A: drain refcount==0 assets to a fixed point. Each delete may release
	// refs to other assets (e.g. a material releasing texture handles), dropping
	// them to zero — repeat until no more do.
	//
	// Zenith_HashMap's Iterator asserts on mid-iteration mutation (generation
	// guard), so unlike the old std::unordered_map erase-while-iterating, each
	// pass first collects the refcount==0 keys, then removes them after the
	// iterator is destroyed.
	bool bAnyDeleted;
	do
	{
		bAnyDeleted = false;

		Zenith_Vector<std::string> xToRemove;
		for (Zenith_HashMap<std::string, Zenith_Asset*>::Iterator xIt(m_xAssetsByPath); !xIt.Done(); xIt.Next())
		{
			if (xIt.GetValue()->GetRefCount() == 0)
			{
				xToRemove.PushBack(xIt.GetKey());
			}
		}

		for (const std::string& strPath : xToRemove)
		{
			if (m_bLifecycleLogging)
			{
				Zenith_Log(LOG_CATEGORY_ASSET, "AssetRegistry: Unloading '%s'", strPath.c_str());
			}
			delete m_xAssetsByPath[strPath];
			m_xAssetsByPath.Remove(strPath);
			bAnyDeleted = true;
		}
	} while (bAnyDeleted);

	// Phase B: anything still held is a real cycle or a leaked handle — log loudly
	// so the leak is discoverable but don't abort (cycles can be legitimate, and
	// we still need to free the memory either way).
	for (Zenith_HashMap<std::string, Zenith_Asset*>::Iterator xIt(m_xAssetsByPath); !xIt.Done(); xIt.Next())
	{
		Zenith_Warning(LOG_CATEGORY_ASSET,
			"AssetRegistry shutdown: '%s' still held with %u refs — cycle or leaked handle",
			xIt.GetKey().c_str(), xIt.GetValue()->GetRefCount());
	}

	// Phase C: force-delete the remaining assets (the registry itself is being torn
	// down, so we cannot leave the heap allocations dangling).
	for (Zenith_HashMap<std::string, Zenith_Asset*>::Iterator xIt(m_xAssetsByPath); !xIt.Done(); xIt.Next())
	{
		delete xIt.GetValue();
	}
	m_xAssetsByPath.Clear();
}

uint32_t Zenith_AssetRegistry::GetLoadedAssetCountInternal() const
{
	Zenith_ScopedMutexLock xLock(m_xMutex);
	return static_cast<uint32_t>(m_xAssetsByPath.GetSize());
}

void Zenith_AssetRegistry::LogLoadedAssetsInternal() const
{
	Zenith_ScopedMutexLock xLock(m_xMutex);

	Zenith_Log(LOG_CATEGORY_ASSET, "=== Loaded Assets (%u total) ===", m_xAssetsByPath.GetSize());

	for (Zenith_HashMap<std::string, Zenith_Asset*>::Iterator xIt(m_xAssetsByPath); !xIt.Done(); xIt.Next())
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "  [ref=%u] %s", xIt.GetValue()->GetRefCount(), xIt.GetKey().c_str());
	}

	Zenith_Log(LOG_CATEGORY_ASSET, "=================================");
}

void Zenith_AssetRegistry::RegisterLoader(Zenith_TypeIndex xType, AssetLoaderFn pfnLoader)
{
	m_xLoaders[xType] = pfnLoader;
}

Zenith_Asset* Zenith_AssetRegistry::GetInternal(Zenith_TypeIndex xType, const std::string& strPath)
{
	ZENITH_PROFILE_SCOPE("AssetRegistry Load");
	if (strPath.empty())
	{
		return nullptr;
	}

	Zenith_ScopedMutexLock xLock(m_xMutex);

	// Check cache first (using the prefixed path as key for portability)
	Zenith_Asset** ppxAsset = m_xAssetsByPath.TryGet(strPath);
	if (ppxAsset != nullptr)
	{
		return *ppxAsset;
	}

	// Find loader for this type
	AssetLoaderFn* ppfnLoader = m_xLoaders.TryGet(xType);
	if (ppfnLoader == nullptr)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "AssetRegistry: No loader registered for type");
		return nullptr;
	}

	// Resolve prefixed path to absolute path for file loading
	std::string strAbsolutePath = ResolvePath(strPath);

	// Load the asset using the absolute path
	Zenith_Result<Zenith_Asset*> xRes = (*ppfnLoader)(strAbsolutePath);
	if (!xRes.IsOk())
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "AssetRegistry: Failed to load asset '%s' (resolved: '%s') [error %u]",
			strPath.c_str(), strAbsolutePath.c_str(), static_cast<unsigned int>(xRes.Error()));
		return nullptr;
	}
	Zenith_Asset* pxAsset = xRes.Value();

	// Store the prefixed path (portable) in the asset and cache
	pxAsset->m_strPath = strPath;
	m_xAssetsByPath[strPath] = pxAsset;

	if (m_bLifecycleLogging)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "AssetRegistry: Loaded asset '%s'", strPath.c_str());
	}

	return pxAsset;
}

Zenith_Asset* Zenith_AssetRegistry::CreateInternal(Zenith_TypeIndex xType)
{
	Zenith_ScopedMutexLock xLock(m_xMutex);

	// Find loader - use empty path to indicate creation
	AssetLoaderFn* ppfnLoader = m_xLoaders.TryGet(xType);
	if (ppfnLoader == nullptr)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "AssetRegistry: No loader registered for type");
		return nullptr;
	}

	// Generate unique procedural path
	std::string strPath = GenerateProceduralPath("asset");

	// Create the asset (loader should handle empty path as "create new")
	Zenith_Result<Zenith_Asset*> xRes = (*ppfnLoader)("");
	if (!xRes.IsOk())
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "AssetRegistry: Failed to create procedural asset [error %u]",
			static_cast<unsigned int>(xRes.Error()));
		return nullptr;
	}
	Zenith_Asset* pxAsset = xRes.Value();

	// Set path and add to cache
	pxAsset->m_strPath = strPath;
	m_xAssetsByPath[strPath] = pxAsset;

	if (m_bLifecycleLogging)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "AssetRegistry: Created procedural asset '%s'", strPath.c_str());
	}

	return pxAsset;
}

Zenith_Asset* Zenith_AssetRegistry::CreateInternal(Zenith_TypeIndex xType, const std::string& strPath)
{
	Zenith_ScopedMutexLock xLock(m_xMutex);

	// Find loader
	AssetLoaderFn* ppfnLoader = m_xLoaders.TryGet(xType);
	if (ppfnLoader == nullptr)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "AssetRegistry: No loader registered for type");
		return nullptr;
	}

	// Create the asset (loader should handle empty path as "create new")
	Zenith_Result<Zenith_Asset*> xRes = (*ppfnLoader)("");
	if (!xRes.IsOk())
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "AssetRegistry: Failed to create procedural asset [error %u]",
			static_cast<unsigned int>(xRes.Error()));
		return nullptr;
	}
	Zenith_Asset* pxAsset = xRes.Value();

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

//------------------------------------------------------------------------------
// Serializable Asset Support (.zdata files)
//------------------------------------------------------------------------------

void Zenith_AssetRegistry::RegisterSerializableAssetType(const char* szTypeName, SerializableAssetFactoryFn pfnFactory)
{
	Zenith_ScopedMutexLock_T<Zenith_Mutex_NoProfiling> xLock(GetSerializableTypeRegistryMutex());
	GetSerializableTypeRegistry()[szTypeName] = pfnFactory;
	Zenith_Log(LOG_CATEGORY_ASSET, "AssetRegistry: Registered serializable type: %s", szTypeName);
}

bool Zenith_AssetRegistry::IsSerializableTypeRegistered(const char* szTypeName)
{
	Zenith_ScopedMutexLock_T<Zenith_Mutex_NoProfiling> xLock(GetSerializableTypeRegistryMutex());
	return GetSerializableTypeRegistry().Contains(szTypeName);
}

bool Zenith_AssetRegistry::SaveInternal(Zenith_Asset* pxAsset, const std::string& strPath)
{
	if (!pxAsset)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "AssetRegistry: Cannot save null asset");
		return false;
	}

	const char* szTypeName = pxAsset->GetTypeName();
	if (!szTypeName)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "AssetRegistry: Cannot save asset - GetTypeName() returned nullptr");
		return false;
	}

	// Resolve prefixed path to absolute path for file writing
	std::string strAbsolutePath = ResolvePath(strPath);

	// Open file for writing
	std::ofstream xFile(strAbsolutePath, std::ios::binary);
	if (!xFile.is_open())
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "AssetRegistry: Failed to create file: %s", strAbsolutePath.c_str());
		return false;
	}

	// Write magic number
	uint32_t uMagic = ZDATA_MAGIC;
	xFile.write(reinterpret_cast<const char*>(&uMagic), sizeof(uMagic));

	// Write version
	uint32_t uVersion = ZDATA_VERSION;
	xFile.write(reinterpret_cast<const char*>(&uVersion), sizeof(uVersion));

	// Write type name (null-terminated)
	xFile.write(szTypeName, strlen(szTypeName) + 1);

	// Serialize asset data
	Zenith_DataStream xStream;
	pxAsset->WriteToDataStream(xStream);

	// Write serialized data
	if (xStream.GetCursor() > 0)
	{
		xFile.write(reinterpret_cast<const char*>(xStream.GetData()), xStream.GetCursor());
	}

	// Update the asset's path if it was procedural
	if (pxAsset->IsProcedural())
	{
		Zenith_ScopedMutexLock xLock(m_xMutex);

		// Remove from old procedural path in cache
		m_xAssetsByPath.Remove(pxAsset->m_strPath);

		// Update path and re-cache with new path
		pxAsset->m_strPath = strPath;
		m_xAssetsByPath[strPath] = pxAsset;
	}

	if (m_bLifecycleLogging)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "AssetRegistry: Saved '%s' to: %s", szTypeName, strAbsolutePath.c_str());
	}

	return true;
}

bool Zenith_AssetRegistry::SaveInternal(Zenith_Asset* pxAsset)
{
	if (!pxAsset)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "AssetRegistry: Cannot save null asset");
		return false;
	}

	if (pxAsset->IsProcedural())
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "AssetRegistry: Cannot save asset - no file path set (procedural asset)");
		return false;
	}

	return Save(pxAsset, pxAsset->GetPath());
}

// Loader function for .zdata files (serializable assets)
// This is a friend function of Zenith_AssetRegistry and is forward-declared in the header
Zenith_Result<Zenith_Asset*> LoadSerializableAsset(const std::string& strPath)
{
	if (strPath.empty())
	{
		// Cannot create without type - use registry.Create<T>() instead
		return Zenith_ErrorCode::INVALID_ARGUMENT;
	}

	// Open file
	std::ifstream xFile(strPath, std::ios::binary);
	if (!xFile.is_open())
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "AssetRegistry: Failed to open .zdata file: %s", strPath.c_str());
		return Zenith_ErrorCode::FILE_NOT_FOUND;
	}

	// Read and validate magic number
	uint32_t uMagic = 0;
	xFile.read(reinterpret_cast<char*>(&uMagic), sizeof(uMagic));
	if (uMagic != Zenith_AssetRegistry::ZDATA_MAGIC)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "AssetRegistry: Invalid .zdata file (bad magic): %s", strPath.c_str());
		return Zenith_ErrorCode::BAD_MAGIC;
	}

	// Read and validate version
	uint32_t uVersion = 0;
	xFile.read(reinterpret_cast<char*>(&uVersion), sizeof(uVersion));
	if (uVersion > Zenith_AssetRegistry::ZDATA_VERSION)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "AssetRegistry: .zdata file version %u is newer than supported (%u): %s",
			uVersion, Zenith_AssetRegistry::ZDATA_VERSION, strPath.c_str());
		return Zenith_ErrorCode::VERSION_MISMATCH;
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
		Zenith_Log(LOG_CATEGORY_ASSET, "AssetRegistry: .zdata file has empty type name: %s", strPath.c_str());
		return Zenith_ErrorCode::CORRUPT_DATA;
	}

	// Find factory for this type
	Zenith_AssetRegistry::SerializableAssetFactoryFn pfnFactory = nullptr;
	{
		Zenith_ScopedMutexLock_T<Zenith_Mutex_NoProfiling> xLock(Zenith_AssetRegistry::GetSerializableTypeRegistryMutex());
		Zenith_HashMap<std::string, Zenith_AssetRegistry::SerializableAssetFactoryFn>& xRegistry = Zenith_AssetRegistry::GetSerializableTypeRegistry();
		Zenith_AssetRegistry::SerializableAssetFactoryFn* ppfnFactory = xRegistry.TryGet(strTypeName);
		if (ppfnFactory != nullptr)
		{
			pfnFactory = *ppfnFactory;
		}
	}

	if (!pfnFactory)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "AssetRegistry: Serializable type '%s' not registered, cannot load: %s",
			strTypeName.c_str(), strPath.c_str());
		return Zenith_ErrorCode::CORRUPT_DATA;
	}

	// Create asset instance
	Zenith_Asset* pxAsset = pfnFactory();
	if (!pxAsset)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "AssetRegistry: Failed to create '%s' from: %s",
			strTypeName.c_str(), strPath.c_str());
		return Zenith_ErrorCode::CORRUPT_DATA;
	}

	// Read remaining file data into buffer
	std::streampos xCurrentPos = xFile.tellg();
	xFile.seekg(0, std::ios::end);
	std::streamsize xDataSize = xFile.tellg() - xCurrentPos;
	xFile.seekg(xCurrentPos);

	if (xDataSize > 0)
	{
		Zenith_Vector<char> xBuffer(static_cast<u_int>(xDataSize));
		xFile.read(xBuffer.GetDataPointer(), xDataSize);

		// Create DataStream with external data and deserialize
		Zenith_DataStream xStream(xBuffer.GetDataPointer(), static_cast<uint64_t>(xDataSize));
		pxAsset->ReadFromDataStream(xStream);
	}

	Zenith_Log(LOG_CATEGORY_ASSET, "AssetRegistry: Loaded serializable asset '%s' from: %s",
		strTypeName.c_str(), strPath.c_str());
	return pxAsset;
}

#include "AssetHandling/Zenith_AssetRegistry.Tests.inl"
