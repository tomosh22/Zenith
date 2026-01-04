#pragma once
#include "Core/Zenith_GUID.h"
#include "AssetHandling/Zenith_AssetMeta.h"
#include "Collections/Zenith_Vector.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <functional>

/**
 * Zenith_AssetDatabase - Central registry for all project assets
 *
 * This is the core of the GUID-based asset management system:
 * - Maintains bidirectional GUID <-> Path mappings
 * - Tracks asset dependencies for hot-reload propagation
 * - Provides asset import, move, and delete operations
 * - Scans project directories to discover and register assets
 *
 * Usage:
 *   // Initialize at startup
 *   Zenith_AssetDatabase::Initialize("Assets/");
 *
 *   // Get path from GUID
 *   std::string path = Zenith_AssetDatabase::GetPathFromGUID(guid);
 *
 *   // Get GUID from path
 *   Zenith_AssetGUID guid = Zenith_AssetDatabase::GetGUIDFromPath("Assets/Textures/diffuse.ztex");
 *
 *   // Register dependencies for hot-reload
 *   Zenith_AssetDatabase::RegisterDependency(materialGUID, textureGUID);
 *
 *   // Check for file modifications (call each frame or periodically)
 *   Zenith_AssetDatabase::CheckForModifications();
 */
class Zenith_AssetDatabase
{
public:
	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	/**
	 * Initialize the asset database and scan for existing assets
	 * @param strProjectRoot Root directory to scan for assets
	 */
	static void Initialize(const std::string& strProjectRoot);

	/**
	 * Shutdown the asset database and free all resources
	 */
	static void Shutdown();

	/**
	 * Check if the database has been initialized
	 */
	static bool IsInitialized();

	/**
	 * Get the project root path
	 */
	static const std::string& GetProjectRoot();

	//--------------------------------------------------------------------------
	// GUID <-> Path Lookups
	//--------------------------------------------------------------------------

	/**
	 * Get the file path for an asset by its GUID
	 * @param xGUID The asset's GUID
	 * @return Asset path, or empty string if not found
	 */
	static std::string GetPathFromGUID(const Zenith_AssetGUID& xGUID);

	/**
	 * Get the GUID for an asset by its file path
	 * @param strPath The asset's file path (can be relative or absolute)
	 * @return Asset GUID, or INVALID if not found
	 */
	static Zenith_AssetGUID GetGUIDFromPath(const std::string& strPath);

	/**
	 * Check if an asset with the given GUID exists in the database
	 */
	static bool AssetExists(const Zenith_AssetGUID& xGUID);

	/**
	 * Check if an asset at the given path exists in the database
	 */
	static bool AssetExistsByPath(const std::string& strPath);

	/**
	 * Get the asset type for a GUID
	 */
	static Zenith_AssetType GetAssetType(const Zenith_AssetGUID& xGUID);

	/**
	 * Get all assets of a specific type
	 */
	static void GetAssetsByType(Zenith_AssetType eType, Zenith_Vector<Zenith_AssetGUID>& xOutGUIDs);

	//--------------------------------------------------------------------------
	// Asset Import/Registration
	//--------------------------------------------------------------------------

	/**
	 * Import a new asset file into the database
	 * Creates a .zmeta file and registers the asset
	 * @param strAssetPath Path to the asset file
	 * @return The newly generated GUID, or INVALID on failure
	 */
	static Zenith_AssetGUID ImportAsset(const std::string& strAssetPath);

	/**
	 * Register an existing asset with its meta file
	 * Used during project scanning
	 * @param xMeta The loaded asset meta
	 */
	static void RegisterAsset(const Zenith_AssetMeta& xMeta);

	/**
	 * Unregister an asset from the database
	 * Does not delete the actual files
	 * @param xGUID The asset's GUID
	 */
	static void UnregisterAsset(const Zenith_AssetGUID& xGUID);

	//--------------------------------------------------------------------------
	// Asset Operations
	//--------------------------------------------------------------------------

	/**
	 * Move/rename an asset to a new path
	 * Updates the meta file and internal mappings
	 * @param xGUID The asset's GUID
	 * @param strNewPath The new file path
	 * @return true on success
	 */
	static bool MoveAsset(const Zenith_AssetGUID& xGUID, const std::string& strNewPath);

	/**
	 * Delete an asset and its meta file
	 * @param xGUID The asset's GUID
	 * @return true on success
	 */
	static bool DeleteAsset(const Zenith_AssetGUID& xGUID);

	/**
	 * Get the meta data for an asset
	 * @param xGUID The asset's GUID
	 * @param xOutMeta Output meta data
	 * @return true if found
	 */
	static bool GetAssetMeta(const Zenith_AssetGUID& xGUID, Zenith_AssetMeta& xOutMeta);

	//--------------------------------------------------------------------------
	// Dependency Tracking
	//--------------------------------------------------------------------------

	/**
	 * Register that one asset depends on another
	 * Used for hot-reload propagation (e.g., material depends on texture)
	 * @param xAsset The dependent asset
	 * @param xDependsOn The asset being depended upon
	 */
	static void RegisterDependency(const Zenith_AssetGUID& xAsset, const Zenith_AssetGUID& xDependsOn);

	/**
	 * Remove a dependency relationship
	 */
	static void UnregisterDependency(const Zenith_AssetGUID& xAsset, const Zenith_AssetGUID& xDependsOn);

	/**
	 * Clear all dependencies for an asset
	 */
	static void ClearDependencies(const Zenith_AssetGUID& xAsset);

	/**
	 * Get all assets that this asset depends on
	 * @param xAsset The asset to query
	 * @return Vector of GUIDs this asset depends on
	 */
	static Zenith_Vector<Zenith_AssetGUID> GetDependencies(const Zenith_AssetGUID& xAsset);

	/**
	 * Get all assets that depend on this asset
	 * @param xAsset The asset to query
	 * @return Vector of GUIDs that depend on this asset
	 */
	static Zenith_Vector<Zenith_AssetGUID> GetDependents(const Zenith_AssetGUID& xAsset);

	//--------------------------------------------------------------------------
	// Hot-Reload
	//--------------------------------------------------------------------------

	/**
	 * Callback type for asset reload notifications
	 * @param xGUID The GUID of the reloaded asset
	 */
	using ReloadCallback = std::function<void(const Zenith_AssetGUID&)>;

	/**
	 * Register a callback to be notified when assets are reloaded
	 * @param pfnCallback The callback function
	 * @return Handle to unregister the callback later
	 */
	static uint32_t RegisterReloadCallback(ReloadCallback pfnCallback);

	/**
	 * Unregister a reload callback
	 * @param uHandle The handle returned from RegisterReloadCallback
	 */
	static void UnregisterReloadCallback(uint32_t uHandle);

	/**
	 * Check for file modifications and trigger reloads
	 * Call this each frame or periodically
	 */
	static void CheckForModifications();

	/**
	 * Force reload of a specific asset
	 * @param xGUID The asset to reload
	 */
	static void ReloadAsset(const Zenith_AssetGUID& xGUID);

	//--------------------------------------------------------------------------
	// Project Scanning
	//--------------------------------------------------------------------------

	/**
	 * Scan a directory recursively for assets
	 * Registers all found assets with valid .zmeta files
	 * Creates .zmeta files for new assets
	 * @param strDirectory Directory to scan
	 */
	static void ScanDirectory(const std::string& strDirectory);

	/**
	 * Refresh the entire project (rescan all directories)
	 */
	static void RefreshProject();

private:
	// Normalize a path for consistent lookups
	static std::string NormalizePath(const std::string& strPath);

	// Check if a file is an asset (not a meta file or other system file)
	static bool IsAssetFile(const std::string& strPath);

	// Notify dependents that an asset has been reloaded
	static void NotifyDependents(const Zenith_AssetGUID& xGUID);

	// Internal data structures
	struct AssetEntry
	{
		Zenith_AssetMeta m_xMeta;
		uint64_t m_ulLastCheckedTime = 0;
	};

	// GUID -> Asset entry
	static std::unordered_map<Zenith_AssetGUID, AssetEntry> s_xAssetsByGUID;

	// Normalized path -> GUID (for fast path lookups)
	static std::unordered_map<std::string, Zenith_AssetGUID> s_xGUIDsByPath;

	// Dependency graph: asset -> set of assets it depends on
	static std::unordered_map<Zenith_AssetGUID, std::unordered_set<Zenith_AssetGUID>> s_xDependencies;

	// Reverse dependency graph: asset -> set of assets that depend on it
	static std::unordered_map<Zenith_AssetGUID, std::unordered_set<Zenith_AssetGUID>> s_xDependents;

	// Reload callbacks
	static std::unordered_map<uint32_t, ReloadCallback> s_xReloadCallbacks;
	static uint32_t s_uNextCallbackHandle;

	// Project root path
	static std::string s_strProjectRoot;

	// Initialization flag
	static bool s_bInitialized;

	// Mutex for thread safety
	static Zenith_Mutex s_xMutex;
};
