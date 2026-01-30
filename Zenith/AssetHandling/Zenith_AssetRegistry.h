#pragma once

#include "Core/Multithreading/Zenith_Multithreading.h"
#include <string>
#include <unordered_map>
#include <typeindex>
#include <functional>

// Forward declarations
class Zenith_Asset;
class Zenith_TextureAsset;
class Zenith_MaterialAsset;
class Zenith_MeshAsset;
class Zenith_SkeletonAsset;
class Zenith_ModelAsset;
class Zenith_Prefab;

// Forward declare .zdata loader (defined in .cpp, used by RegisterAssetType<T>)
Zenith_Asset* LoadSerializableAsset(const std::string& strPath);

/**
 * Zenith_AssetRegistry - THE unified asset management system
 *
 * This singleton replaces all previous asset management systems:
 * - Zenith_AssetHandler (pools and caches)
 * - Zenith_AssetDatabase (GUID registry)
 * - Zenith_AssetRef caches (per-type static caches)
 * - Flux_MaterialAsset caches (material and texture caches)
 *
 * Features:
 * - Single unified cache for all asset types
 * - Path-based identification with prefixes (game: and engine:)
 * - Reference counting with automatic cleanup
 * - Support for procedural (code-created) assets
 * - Thread-safe operations
 * - Relative paths for cross-machine portability
 *
 * Path Prefixes:
 *   game:   - Resolves to GAME_ASSETS_DIR (e.g., "game:Textures/diffuse.ztex")
 *   engine: - Resolves to ENGINE_ASSETS_DIR (e.g., "engine:Materials/default.zmat")
 *
 * Usage:
 *   // Set directories at startup
 *   Zenith_AssetRegistry::SetGameAssetsDir(GAME_ASSETS_DIR);
 *   Zenith_AssetRegistry::SetEngineAssetsDir(ENGINE_ASSETS_DIR);
 *   Zenith_AssetRegistry::Initialize();
 *
 *   // Get singleton
 *   auto& reg = Zenith_AssetRegistry::Get();
 *
 *   // Load asset from file (using prefixed path)
 *   Zenith_TextureAsset* pTex = reg.Get<Zenith_TextureAsset>("game:Textures/diffuse.ztex");
 *
 *   // Create procedural asset
 *   Zenith_MeshAsset* pMesh = reg.Create<Zenith_MeshAsset>();
 *
 *   // Cleanup
 *   reg.UnloadUnused();  // Free assets with ref count 0
 */
class Zenith_AssetRegistry
{
public:
	/**
	 * Get the singleton instance
	 */
	static Zenith_AssetRegistry& Get();

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
	 * @param strPrefixedPath Path with prefix (e.g., "game:Textures/tex.ztex")
	 * @return Absolute path on disk
	 */
	static std::string ResolvePath(const std::string& strPrefixedPath);

	/**
	 * Convert an absolute path to a prefixed relative path
	 * @param strAbsolutePath Absolute path on disk
	 * @return Prefixed relative path (e.g., "game:Textures/tex.ztex"), or empty if not in known directories
	 */
	static std::string MakeRelativePath(const std::string& strAbsolutePath);

	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	/**
	 * Initialize the registry (call once at startup, after SetGameAssetsDir/SetEngineAssetsDir)
	 */
	static void Initialize();

	/**
	 * Initialize GPU-dependent assets (call after Vulkan/VMA is initialized)
	 * Must be called after Flux::EarlyInitialise()
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
	 * @param strPath Path to the asset file
	 * @return Pointer to the asset, or nullptr on failure
	 */
	template<typename T>
	T* Get(const std::string& strPath);

	/**
	 * Create a new procedural asset
	 * @return Pointer to the new asset with a generated path
	 */
	template<typename T>
	T* Create();

	/**
	 * Create a new procedural asset with a specific path
	 * Useful for caching primitives by path (e.g., "procedural://unit_cube")
	 * @param strPath The path to register the asset under
	 * @return Pointer to the new asset
	 */
	template<typename T>
	T* Create(const std::string& strPath);

	/**
	 * Check if an asset is loaded
	 * @param strPath Path to check
	 * @return true if asset is currently loaded
	 */
	bool IsLoaded(const std::string& strPath) const;

	//--------------------------------------------------------------------------
	// Asset Unloading
	//--------------------------------------------------------------------------

	/**
	 * Force unload a specific asset
	 * WARNING: This will delete the asset even if ref count > 0
	 * @param strPath Path of asset to unload
	 */
	void Unload(const std::string& strPath);

	/**
	 * Unload all assets with ref count 0
	 * Call this periodically (e.g., during scene transitions) to free unused assets
	 */
	void UnloadUnused();

	/**
	 * Unload all assets (call at shutdown)
	 */
	void UnloadAll();

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
		T xTemp;  // Create temporary to get type name
		const char* szTypeName = xTemp.GetTypeName();
		RegisterSerializableAssetType(szTypeName, []() -> Zenith_Asset* { return new T(); });

		// Also register a loader for this type if instance exists
		if (s_pxInstance)
		{
			s_pxInstance->RegisterLoader(std::type_index(typeid(T)), [](const std::string& strPath) -> Zenith_Asset* {
				if (strPath.empty())
				{
					// Create empty instance for procedural assets
					return new T();
				}
				// Use the generic .zdata loader
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
	bool Save(Zenith_Asset* pxAsset, const std::string& strPath);

	/**
	 * Save a serializable asset to its current path
	 * @param pxAsset Asset to save (must have a non-procedural path set)
	 * @return true on success
	 */
	bool Save(Zenith_Asset* pxAsset);

	//--------------------------------------------------------------------------
	// Diagnostics
	//--------------------------------------------------------------------------

	/**
	 * Get number of loaded assets
	 */
	uint32_t GetLoadedAssetCount() const;

	/**
	 * Enable/disable lifecycle logging
	 */
	void EnableLifecycleLogging(bool bEnable) { m_bLifecycleLogging = bEnable; }

	/**
	 * Log all loaded assets (for debugging memory leaks)
	 */
	void LogLoadedAssets() const;

private:
	Zenith_AssetRegistry() = default;
	~Zenith_AssetRegistry() = default;

	// Non-copyable
	Zenith_AssetRegistry(const Zenith_AssetRegistry&) = delete;
	Zenith_AssetRegistry& operator=(const Zenith_AssetRegistry&) = delete;

	// Friend declaration for .zdata loader
	friend Zenith_Asset* LoadSerializableAsset(const std::string& strPath);

	// Internal loader registration (called by template specializations)
	using AssetLoaderFn = std::function<Zenith_Asset*(const std::string&)>;
	void RegisterLoader(std::type_index xType, AssetLoaderFn pfnLoader);

	// Internal getter for non-template code
	Zenith_Asset* GetInternal(std::type_index xType, const std::string& strPath);
	Zenith_Asset* CreateInternal(std::type_index xType);
	Zenith_Asset* CreateInternal(std::type_index xType, const std::string& strPath);

	// Generate a unique path for procedural assets
	std::string GenerateProceduralPath(const std::string& strPrefix);

	// Singleton instance
	static Zenith_AssetRegistry* s_pxInstance;

	// Asset directories (set before Initialize)
	static std::string s_strGameAssetsDir;
	static std::string s_strEngineAssetsDir;

	// Serializable asset type registry accessors (use function-local statics to avoid
	// static initialization order fiasco - registration happens during static init)
	static std::unordered_map<std::string, SerializableAssetFactoryFn>& GetSerializableTypeRegistry();
	static Zenith_Mutex_NoProfiling& GetSerializableTypeRegistryMutex();

	// .zdata file format constants
	static constexpr uint32_t ZDATA_MAGIC = 0x5441445A;  // "ZDAT" in little-endian
	static constexpr uint32_t ZDATA_VERSION = 1;

	// Unified asset cache: path -> asset
	std::unordered_map<std::string, Zenith_Asset*> m_xAssetsByPath;

	// Type-specific loaders
	std::unordered_map<std::type_index, AssetLoaderFn> m_xLoaders;

	// Thread safety
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
	return static_cast<T*>(GetInternal(std::type_index(typeid(T)), strPath));
}

template<typename T>
T* Zenith_AssetRegistry::Create()
{
	return static_cast<T*>(CreateInternal(std::type_index(typeid(T))));
}

template<typename T>
T* Zenith_AssetRegistry::Create(const std::string& strPath)
{
	return static_cast<T*>(CreateInternal(std::type_index(typeid(T)), strPath));
}
