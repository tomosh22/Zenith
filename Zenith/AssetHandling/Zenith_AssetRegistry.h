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

	// Internal loader registration (called by template specializations)
	using AssetLoaderFn = std::function<Zenith_Asset*(const std::string&)>;
	void RegisterLoader(std::type_index xType, AssetLoaderFn pfnLoader);

	// Internal getter for non-template code
	Zenith_Asset* GetInternal(std::type_index xType, const std::string& strPath);
	Zenith_Asset* CreateInternal(std::type_index xType);

	// Generate a unique path for procedural assets
	std::string GenerateProceduralPath(const std::string& strPrefix);

	// Singleton instance
	static Zenith_AssetRegistry* s_pxInstance;

	// Asset directories (set before Initialize)
	static std::string s_strGameAssetsDir;
	static std::string s_strEngineAssetsDir;

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
