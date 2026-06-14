#pragma once


#include "Flux/Flux_RendererImpl.h"
#include "Core/Multithreading/Zenith_Multithreading.h"
#include "Core/Zenith_Result.h"
#include "Collections/Zenith_HashMap.h"
#include <string>

// Compile-time type index (no RTTI required)
// Each unique type T gets a unique address via function-local static
struct Zenith_TypeIndex
{
	size_t m_uId;

	bool operator==(const Zenith_TypeIndex& xOther) const { return m_uId == xOther.m_uId; }

	template<typename T>
	static Zenith_TypeIndex Of()
	{
		static const char s_cTag = 0;
		return Zenith_TypeIndex{ reinterpret_cast<size_t>(&s_cTag) };
	}
};

namespace std
{
	template<> struct hash<Zenith_TypeIndex>
	{
		size_t operator()(const Zenith_TypeIndex& x) const { return x.m_uId; }
	};
}

// Forward declarations
class Zenith_Asset;
class Zenith_TextureAsset;
class Zenith_MaterialAsset;
class Zenith_MeshAsset;
class Zenith_SkeletonAsset;
class Zenith_ModelAsset;
class Zenith_Prefab;

// Forward declare .zdata loader (defined in .cpp, used by RegisterAssetType<T>)
Zenith_Result<Zenith_Asset*> LoadSerializableAsset(const std::string& strPath);

/**
 * Zenith_AssetRegistry - THE unified asset cache (singleton).
 * Path-based IDs with "game:" / "engine:" prefixes for cross-machine portability;
 * ref-counted with UnloadUnused() cleanup; thread-safe.
 *
 * The static API (Get<T>(path) / Create<T>() / Save / UnloadUnused / ...) is the
 * CANONICAL access path — not a transitional forwarder. The engine owns the one
 * instance (g_xEngine.Assets(), set via s_pxInstance in InitialiseAssets); the
 * static methods delegate to it. Call sites use the static form throughout; there
 * is no planned migration to g_xEngine.Assets().X().
 * See AssetHandling/CLAUDE.md for usage and two-phase init order.
 */
class Zenith_AssetRegistry
{
	// Test code reads/writes the cached asset-dir strings to verify path
	// normalization edge cases (backslash conversion, trailing slash strip).
	friend class Zenith_UnitTests;

public:

	//--------------------------------------------------------------------------
	// Path Resolution
	//--------------------------------------------------------------------------

	/**
	 * Set the game assets directory (call before Initialize)
	 * @param strPath Absolute path to game assets directory
	 */
	static void SetGameAssetsDir(const std::string& strPath);

	/**
	 * Set the engine assets directory (call before Initialize)
	 * @param strPath Absolute path to engine assets directory
	 */
	static void SetEngineAssetsDir(const std::string& strPath);

	/**
	 * Resolve a prefixed path to an absolute path
	 * @param strPrefixedPath Path with prefix (e.g., "game:Textures/tex.ztxtr")
	 * @return Absolute path on disk
	 */
	static std::string ResolvePath(const std::string& strPrefixedPath);

	/**
	 * Convert an absolute path to a prefixed relative path
	 * @param strAbsolutePath Absolute path on disk
	 * @return Prefixed relative path (e.g., "game:Textures/tex.ztxtr"), or empty if not in known directories
	 */
	static std::string MakeRelativePath(const std::string& strAbsolutePath);

	/**
	 * Normalize a path for serialization: converts absolute paths to prefixed relative paths
	 * Already-prefixed paths (game:, engine:, procedural://) pass through unchanged
	 * @param strPath Path to normalize
	 * @return Normalized path suitable for cross-machine serialization
	 */
	static std::string NormalizeAssetPath(const std::string& strPath);

	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	/**
	 * Initialize the registry (call once at startup, after SetGameAssetsDir/SetEngineAssetsDir)
	 */
	static void Initialize();

	/**
	 * Initialize GPU-dependent assets (call after Vulkan/VMA is initialized)
	 * Must be called after g_xEngine.FluxRenderer().EarlyInitialise()
	 */
	static void InitializeGPUDependentAssets();

	/**
	 * Shutdown the registry (call once at shutdown)
	 */
	static void Shutdown();

	//--------------------------------------------------------------------------
	// Asset Loading
	//--------------------------------------------------------------------------

	/**
	 * Get an asset by path, loading if necessary
	 * Returns a raw pointer with NO AddRef applied — Zenith_AssetHandle does the
	 * ref-counting; hold a handle if the asset must survive UnloadUnused().
	 * @param strPath Path to the asset file (e.g., "game:Textures/diffuse.ztxtr")
	 * @return Pointer to the asset, or nullptr on failure
	 */
	template<typename T>
	static T* Get(const std::string& strPath);

	/**
	 * Create a new procedural asset
	 * @return Pointer to the new asset with a generated path
	 */
	template<typename T>
	static T* Create();

	/**
	 * Create a new procedural asset with a specific path
	 * Useful for caching primitives by path (e.g., "procedural://unit_cube")
	 * @param strPath The path to register the asset under
	 * @return Pointer to the new asset
	 */
	template<typename T>
	static T* Create(const std::string& strPath);

	/**
	 * Check if an asset is loaded
	 * @param strPath Path to check
	 * @return true if asset is currently loaded
	 */
	static bool IsLoaded(const std::string& strPath)
	{
		return s_pxInstance->IsLoadedInternal(strPath);
	}

	//--------------------------------------------------------------------------
	// Asset Unloading
	//--------------------------------------------------------------------------

	/**
	 * Delete a specific asset even if its ref count > 0 — existing handles dangle.
	 * Prefer UnloadUnused() unless deletion must be unconditional.
	 * @param strPath Path of asset to unload
	 */
	static void ForceUnload(const std::string& strPath)
	{
		s_pxInstance->ForceUnloadInternal(strPath);
	}

	/**
	 * Unload all assets with ref count 0
	 * Call this periodically (e.g., during scene transitions) to free unused assets
	 */
	static void UnloadUnused()
	{
		s_pxInstance->UnloadUnusedInternal();
	}

	/**
	 * Unload all assets (call at shutdown)
	 */
	static void UnloadAll()
	{
		s_pxInstance->UnloadAllInternal();
	}

	//--------------------------------------------------------------------------
	// Serializable Asset Support (.zdata files)
	//--------------------------------------------------------------------------

	/**
	 * Factory function type for creating serializable asset instances
	 */
	using SerializableAssetFactoryFn = Zenith_Asset*(*)();

	/**
	 * Register a serializable asset type (call during static initialization)
	 * Assets with GetTypeName() override should be registered here
	 * @param szTypeName The type name (must match GetTypeName() return value)
	 * @param pfnFactory Factory function to create instances
	 */
	static void RegisterSerializableAssetType(const char* szTypeName, SerializableAssetFactoryFn pfnFactory);

	/**
	 * Template helper for registering serializable asset types
	 * This registers both the type factory and a loader for the asset type
	 */
	template<typename T>
	static void RegisterAssetType()
	{
		// Type name from the static accessor (ZENITH_ASSET_TYPE_NAME emits it) —
		// no throwaway instance constructed just to read a compile-time string.
		const char* szTypeName = T::StaticTypeName();
		RegisterSerializableAssetType(szTypeName, []() -> Zenith_Asset* { return new T(); });

		// Also register a loader for this type if instance exists
		if (s_pxInstance)
		{
			s_pxInstance->RegisterLoader(Zenith_TypeIndex::Of<T>(), [](const std::string& strPath) -> Zenith_Result<Zenith_Asset*> {
				if (strPath.empty())
				{
					// Create empty instance for procedural assets (implicit SUCCESS)
					return static_cast<Zenith_Asset*>(new T());
				}
				// Use the generic .zdata loader (already returns a Zenith_Result)
				return LoadSerializableAsset(strPath);
			});
		}
	}

	/**
	 * Check if a serializable asset type is registered
	 */
	static bool IsSerializableTypeRegistered(const char* szTypeName);

	/**
	 * Save a serializable asset to a .zdata file
	 * The asset must have GetTypeName() and WriteToDataStream() implemented
	 * @param pxAsset Asset to save
	 * @param strPath Path to save to (prefixed or absolute)
	 * @return true on success
	 */
	static bool Save(Zenith_Asset* pxAsset, const std::string& strPath)
	{
		return s_pxInstance->SaveInternal(pxAsset, strPath);
	}

	/**
	 * Save a serializable asset to its current path
	 * @param pxAsset Asset to save (must have a non-procedural path set)
	 * @return true on success
	 */
	static bool Save(Zenith_Asset* pxAsset)
	{
		return s_pxInstance->SaveInternal(pxAsset);
	}

	//--------------------------------------------------------------------------
	// Diagnostics
	//--------------------------------------------------------------------------

	/**
	 * Get number of loaded assets
	 */
	static uint32_t GetLoadedAssetCount()
	{
		return s_pxInstance->GetLoadedAssetCountInternal();
	}

	/**
	 * Enable/disable lifecycle logging
	 */
	static void EnableLifecycleLogging(bool bEnable)
	{
		s_pxInstance->m_bLifecycleLogging = bEnable;
	}

	/**
	 * Log all loaded assets (for debugging memory leaks)
	 */
	static void LogLoadedAssets()
	{
		s_pxInstance->LogLoadedAssetsInternal();
	}

private:
	Zenith_AssetRegistry() = default;
	~Zenith_AssetRegistry() = default;

	// Non-copyable
	Zenith_AssetRegistry(const Zenith_AssetRegistry&) = delete;
	Zenith_AssetRegistry& operator=(const Zenith_AssetRegistry&) = delete;

	// Friend declaration for .zdata loader
	friend Zenith_Result<Zenith_Asset*> LoadSerializableAsset(const std::string& strPath);

	// Internal loader registration (called by template specializations).
	// Plain function pointer (not std::function — house rule). The free loaders
	// register directly as &LoadXxxAsset; the RegisterAssetType<T> loader lambda
	// is captureless and decays to a function pointer.
	using AssetLoaderFn = Zenith_Result<Zenith_Asset*>(*)(const std::string&);
	static_assert(std::is_trivially_destructible_v<Zenith_Result<Zenith_Asset*>>);
	void RegisterLoader(Zenith_TypeIndex xType, AssetLoaderFn pfnLoader);

	// Internal implementations (called via static public wrappers)
	Zenith_Asset* GetInternal(Zenith_TypeIndex xType, const std::string& strPath);
	Zenith_Asset* CreateInternal(Zenith_TypeIndex xType);
	Zenith_Asset* CreateInternal(Zenith_TypeIndex xType, const std::string& strPath);
	bool IsLoadedInternal(const std::string& strPath) const;
	void ForceUnloadInternal(const std::string& strPath);
	void UnloadUnusedInternal();
	void UnloadAllInternal();
	uint32_t GetLoadedAssetCountInternal() const;
	void LogLoadedAssetsInternal() const;
	bool SaveInternal(Zenith_Asset* pxAsset, const std::string& strPath);
	bool SaveInternal(Zenith_Asset* pxAsset);

	// Generate a unique path for procedural assets
	std::string GenerateProceduralPath(const std::string& strPrefix);

	// Instance owned by Zenith_Engine (lifetime); the static facade is the single
	// canonical access path. s_pxInstance is installed during engine init.
	static Zenith_AssetRegistry* s_pxInstance;
	friend class Zenith_Engine;

	// Asset directories (set before Initialize)
	static std::string s_strGameAssetsDir;
	static std::string s_strEngineAssetsDir;

	// Serializable asset type registry accessors (use function-local statics to avoid
	// static initialization order fiasco - registration happens during static init)
	static Zenith_HashMap<std::string, SerializableAssetFactoryFn>& GetSerializableTypeRegistry();
	static Zenith_Mutex_NoProfiling& GetSerializableTypeRegistryMutex();

	// .zdata file format constants
	static constexpr uint32_t ZDATA_MAGIC = 0x5441445A;  // "ZDAT" in little-endian
	static constexpr uint32_t ZDATA_VERSION = 1;

	// Unified asset cache: path -> asset
	Zenith_HashMap<std::string, Zenith_Asset*> m_xAssetsByPath;

	// Type-specific loaders
	Zenith_HashMap<Zenith_TypeIndex, AssetLoaderFn> m_xLoaders;

	// Thread safety. CONTRACT: Zenith_Mutex MUST be recursive — Get<T>() runs the
	// type-specific loader inside this lock, and a loader that calls Get<U>() for a
	// sub-asset (e.g. a model loader pulling in its meshes) re-enters the registry.
	// Zenith_Mutex is recursive on Windows (CRITICAL_SECTION) and Android
	// (PTHREAD_MUTEX_RECURSIVE) — see Zenith_Windows_Multithreading.h:15 and
	// Zenith_Android_Multithreading.h:15. A future port that swaps in a
	// non-recursive primitive will deadlock on the first nested Get<>.
	mutable Zenith_Mutex m_xMutex;

	// Procedural asset ID counter
	uint32_t m_uNextProceduralId = 0;

	// Lifecycle logging flag
	bool m_bLifecycleLogging = false;
};

//--------------------------------------------------------------------------
// Template implementations
//--------------------------------------------------------------------------

template<typename T>
T* Zenith_AssetRegistry::Get(const std::string& strPath)
{
	return static_cast<T*>(s_pxInstance->GetInternal(Zenith_TypeIndex::Of<T>(), strPath));
}

template<typename T>
T* Zenith_AssetRegistry::Create()
{
	return static_cast<T*>(s_pxInstance->CreateInternal(Zenith_TypeIndex::Of<T>()));
}

template<typename T>
T* Zenith_AssetRegistry::Create(const std::string& strPath)
{
	return static_cast<T*>(s_pxInstance->CreateInternal(Zenith_TypeIndex::Of<T>(), strPath));
}
