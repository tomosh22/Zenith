#pragma once

#include <string>
#include <vector>
#include "Core/Zenith_GUID.h"

/**
 * Zenith_AssetMigration - Utility for migrating legacy assets to the new GUID-based system
 *
 * This tool handles:
 * - Generating .zmeta files for existing assets
 * - Migrating scene files from path-based to GUID-based references
 * - Migrating material files to use GUID texture references
 * - Full project migration with backup support
 *
 * Usage:
 *   // Generate meta files for all assets in a project
 *   Zenith_AssetMigration::GenerateMetaFiles("path/to/project");
 *
 *   // Migrate an entire project (generates meta files + updates all asset references)
 *   Zenith_AssetMigration::MigrateProject("path/to/project");
 */
class Zenith_AssetMigration
{
public:
	//--------------------------------------------------------------------------
	// Meta File Generation
	//--------------------------------------------------------------------------

	/**
	 * Generate .zmeta files for all assets in a directory (recursive)
	 * Skips assets that already have meta files
	 * @param strProjectRoot Root directory to scan
	 * @return Number of meta files generated
	 */
	static uint32_t GenerateMetaFiles(const std::string& strProjectRoot);

	/**
	 * Generate a .zmeta file for a single asset
	 * @param strAssetPath Path to the asset file
	 * @return true if meta file was created or already exists
	 */
	static bool GenerateMetaFile(const std::string& strAssetPath);

	//--------------------------------------------------------------------------
	// Scene Migration
	//--------------------------------------------------------------------------

	/**
	 * Migrate a scene file from path-based to GUID-based references
	 * Creates a backup before modifying
	 * @param strScenePath Path to the .zscn file
	 * @return true on success
	 */
	static bool MigrateSceneFile(const std::string& strScenePath);

	/**
	 * Check if a scene file needs migration (uses legacy format)
	 * @param strScenePath Path to the .zscn file
	 * @return true if migration is needed
	 */
	static bool SceneNeedsMigration(const std::string& strScenePath);

	//--------------------------------------------------------------------------
	// Material Migration
	//--------------------------------------------------------------------------

	/**
	 * Migrate a material file from path-based to GUID-based texture references
	 * Creates a backup before modifying
	 * @param strMaterialPath Path to the .zmtrl file
	 * @return true on success
	 */
	static bool MigrateMaterialFile(const std::string& strMaterialPath);

	/**
	 * Check if a material file needs migration
	 * @param strMaterialPath Path to the .zmtrl file
	 * @return true if migration is needed
	 */
	static bool MaterialNeedsMigration(const std::string& strMaterialPath);

	//--------------------------------------------------------------------------
	// Model Migration
	//--------------------------------------------------------------------------

	/**
	 * Migrate a model file from path-based to GUID-based references
	 * @param strModelPath Path to the .zmodel file
	 * @return true on success
	 */
	static bool MigrateModelFile(const std::string& strModelPath);

	/**
	 * Check if a model file needs migration
	 * @param strModelPath Path to the .zmodel file
	 * @return true if migration is needed
	 */
	static bool ModelNeedsMigration(const std::string& strModelPath);

	//--------------------------------------------------------------------------
	// Prefab Migration
	//--------------------------------------------------------------------------

	/**
	 * Migrate a prefab file to the new format with GUID
	 * @param strPrefabPath Path to the .zprefab file
	 * @return true on success
	 */
	static bool MigratePrefabFile(const std::string& strPrefabPath);

	/**
	 * Check if a prefab file needs migration
	 * @param strPrefabPath Path to the .zprefab file
	 * @return true if migration is needed
	 */
	static bool PrefabNeedsMigration(const std::string& strPrefabPath);

	//--------------------------------------------------------------------------
	// Full Project Migration
	//--------------------------------------------------------------------------

	/**
	 * Migrate an entire project to the new GUID-based system
	 * 1. Generates meta files for all assets
	 * 2. Migrates all scenes, materials, models, and prefabs
	 * 3. Creates backups of all modified files
	 *
	 * @param strProjectRoot Root directory of the project
	 * @param bDryRun If true, only reports what would be changed (no modifications)
	 * @return true if migration completed successfully
	 */
	static bool MigrateProject(const std::string& strProjectRoot, bool bDryRun = false);

	//--------------------------------------------------------------------------
	// Utility
	//--------------------------------------------------------------------------

	/**
	 * Create a backup of a file before modifying it
	 * @param strFilePath Path to the file
	 * @return Path to the backup file, or empty string on failure
	 */
	static std::string CreateBackup(const std::string& strFilePath);

	/**
	 * Get all asset files in a directory (recursive)
	 * @param strDirectory Directory to scan
	 * @param xOutPaths Output vector of asset paths
	 */
	static void GetAllAssetFiles(const std::string& strDirectory, std::vector<std::string>& xOutPaths);

	/**
	 * Get migration statistics from the last run
	 */
	struct MigrationStats
	{
		uint32_t m_uMetaFilesGenerated = 0;
		uint32_t m_uScenesModified = 0;
		uint32_t m_uMaterialsModified = 0;
		uint32_t m_uModelsModified = 0;
		uint32_t m_uPrefabsModified = 0;
		uint32_t m_uErrors = 0;
		std::vector<std::string> m_xErrorMessages;
	};

	static const MigrationStats& GetLastMigrationStats();

private:
	static MigrationStats s_xLastStats;

	// Helper to resolve a path to its GUID (looking up meta file or creating one)
	static Zenith_AssetGUID ResolvePathToGUID(const std::string& strAssetPath);

	// Helper to check if a file extension is an asset type
	static bool IsAssetExtension(const std::string& strExt);

	// Helper to check if meta file exists
	static bool HasMetaFile(const std::string& strAssetPath);
};
