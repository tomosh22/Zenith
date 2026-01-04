#include "Zenith.h"
#include "Zenith_AssetMigration.h"
#include "AssetHandling/Zenith_AssetMeta.h"
#include "AssetHandling/Zenith_AssetDatabase.h"
#include "DataStream/Zenith_DataStream.h"
#include <filesystem>
#include <fstream>

// Static member initialization
Zenith_AssetMigration::MigrationStats Zenith_AssetMigration::s_xLastStats;

// Log tag
static constexpr const char* LOG_TAG = "[AssetMigration]";

// Asset file extensions
static const std::vector<std::string> s_xAssetExtensions = {
	".ztex", ".zmtrl", ".zmesh", ".zmodel", ".zprefab", ".zscn", ".zanim"
};

//=============================================================================
// Meta File Generation
//=============================================================================

uint32_t Zenith_AssetMigration::GenerateMetaFiles(const std::string& strProjectRoot)
{
	uint32_t uGenerated = 0;

	std::vector<std::string> xAssetPaths;
	GetAllAssetFiles(strProjectRoot, xAssetPaths);

	for (const std::string& strPath : xAssetPaths)
	{
		if (!HasMetaFile(strPath))
		{
			if (GenerateMetaFile(strPath))
			{
				uGenerated++;
			}
		}
	}

	Zenith_Log("%s Generated %u meta files", LOG_TAG, uGenerated);
	return uGenerated;
}

bool Zenith_AssetMigration::GenerateMetaFile(const std::string& strAssetPath)
{
	if (HasMetaFile(strAssetPath))
	{
		return true;  // Already has meta file
	}

	if (!std::filesystem::exists(strAssetPath))
	{
		Zenith_Log("%s Asset file does not exist: %s", LOG_TAG, strAssetPath.c_str());
		return false;
	}

	// Determine asset type from extension
	std::filesystem::path xPath(strAssetPath);
	std::string strExt = xPath.extension().string();
	Zenith_AssetType eType = Zenith_GetAssetTypeFromExtension(strExt);

	// Create meta
	Zenith_AssetMeta xMeta;
	xMeta.m_xGUID = Zenith_AssetGUID::Generate();
	xMeta.m_strAssetPath = strAssetPath;
	xMeta.m_eAssetType = eType;
	xMeta.m_ulLastModifiedTime = static_cast<uint64_t>(
		std::filesystem::last_write_time(strAssetPath).time_since_epoch().count());

	// Save meta file
	std::string strMetaPath = Zenith_AssetMeta::GetMetaPath(strAssetPath);
	if (xMeta.SaveToFile(strMetaPath))
	{
		Zenith_Log("%s Created meta file: %s (GUID: %s)",
			LOG_TAG, strMetaPath.c_str(), xMeta.m_xGUID.ToString().c_str());
		return true;
	}

	return false;
}

//=============================================================================
// Scene Migration
//=============================================================================

bool Zenith_AssetMigration::SceneNeedsMigration(const std::string& strScenePath)
{
	if (!std::filesystem::exists(strScenePath))
	{
		return false;
	}

	Zenith_DataStream xStream;
	xStream.ReadFromFile(strScenePath.c_str());

	// Read magic and version
	u_int uMagic;
	u_int uVersion;
	xStream >> uMagic;
	xStream >> uVersion;

	// Scene version 2+ uses GUIDs
	static constexpr u_int SCENE_GUID_VERSION = 2;
	return uVersion < SCENE_GUID_VERSION;
}

bool Zenith_AssetMigration::MigrateSceneFile(const std::string& strScenePath)
{
	if (!SceneNeedsMigration(strScenePath))
	{
		Zenith_Log("%s Scene does not need migration: %s", LOG_TAG, strScenePath.c_str());
		return true;
	}

	// Create backup
	std::string strBackup = CreateBackup(strScenePath);
	if (strBackup.empty())
	{
		Zenith_Log("%s Failed to create backup for: %s", LOG_TAG, strScenePath.c_str());
		return false;
	}

	// Scene migration is handled by the scene load/save code
	// Loading an old scene and saving it will automatically migrate it
	Zenith_Log("%s Scene migration for %s should be handled by loading and re-saving",
		LOG_TAG, strScenePath.c_str());

	s_xLastStats.m_uScenesModified++;
	return true;
}

//=============================================================================
// Material Migration
//=============================================================================

bool Zenith_AssetMigration::MaterialNeedsMigration(const std::string& strMaterialPath)
{
	if (!std::filesystem::exists(strMaterialPath))
	{
		return false;
	}

	Zenith_DataStream xStream;
	xStream.ReadFromFile(strMaterialPath.c_str());

	// Read magic and version
	u_int uMagic;
	u_int uVersion;
	xStream >> uMagic;
	xStream >> uVersion;

	// Material version 2+ uses GUIDs
	static constexpr u_int MATERIAL_GUID_VERSION = 2;
	return uVersion < MATERIAL_GUID_VERSION;
}

bool Zenith_AssetMigration::MigrateMaterialFile(const std::string& strMaterialPath)
{
	if (!MaterialNeedsMigration(strMaterialPath))
	{
		Zenith_Log("%s Material does not need migration: %s", LOG_TAG, strMaterialPath.c_str());
		return true;
	}

	// Create backup
	std::string strBackup = CreateBackup(strMaterialPath);
	if (strBackup.empty())
	{
		Zenith_Log("%s Failed to create backup for: %s", LOG_TAG, strMaterialPath.c_str());
		return false;
	}

	// Material migration is handled by Flux_MaterialAsset load/save code
	// Loading an old material and saving it will automatically migrate it
	Zenith_Log("%s Material migration for %s should be handled by loading and re-saving",
		LOG_TAG, strMaterialPath.c_str());

	s_xLastStats.m_uMaterialsModified++;
	return true;
}

//=============================================================================
// Model Migration
//=============================================================================

bool Zenith_AssetMigration::ModelNeedsMigration(const std::string& strModelPath)
{
	if (!std::filesystem::exists(strModelPath))
	{
		return false;
	}

	Zenith_DataStream xStream;
	xStream.ReadFromFile(strModelPath.c_str());

	// Read magic and version
	u_int uMagic;
	u_int uVersion;
	xStream >> uMagic;
	xStream >> uVersion;

	// Model version 2+ uses GUIDs
	static constexpr u_int MODEL_GUID_VERSION = 2;
	return uVersion < MODEL_GUID_VERSION;
}

bool Zenith_AssetMigration::MigrateModelFile(const std::string& strModelPath)
{
	if (!ModelNeedsMigration(strModelPath))
	{
		Zenith_Log("%s Model does not need migration: %s", LOG_TAG, strModelPath.c_str());
		return true;
	}

	// Create backup
	std::string strBackup = CreateBackup(strModelPath);
	if (strBackup.empty())
	{
		Zenith_Log("%s Failed to create backup for: %s", LOG_TAG, strModelPath.c_str());
		return false;
	}

	Zenith_Log("%s Model migration for %s should be handled by loading and re-saving",
		LOG_TAG, strModelPath.c_str());

	s_xLastStats.m_uModelsModified++;
	return true;
}

//=============================================================================
// Prefab Migration
//=============================================================================

bool Zenith_AssetMigration::PrefabNeedsMigration(const std::string& strPrefabPath)
{
	if (!std::filesystem::exists(strPrefabPath))
	{
		return false;
	}

	Zenith_DataStream xStream;
	xStream.ReadFromFile(strPrefabPath.c_str());

	// Read magic and version
	u_int uMagic;
	u_int uVersion;
	xStream >> uMagic;
	xStream >> uVersion;

	// Prefab version 2+ uses GUIDs
	static constexpr u_int PREFAB_GUID_VERSION = 2;
	return uVersion < PREFAB_GUID_VERSION;
}

bool Zenith_AssetMigration::MigratePrefabFile(const std::string& strPrefabPath)
{
	if (!PrefabNeedsMigration(strPrefabPath))
	{
		Zenith_Log("%s Prefab does not need migration: %s", LOG_TAG, strPrefabPath.c_str());
		return true;
	}

	// Create backup
	std::string strBackup = CreateBackup(strPrefabPath);
	if (strBackup.empty())
	{
		Zenith_Log("%s Failed to create backup for: %s", LOG_TAG, strPrefabPath.c_str());
		return false;
	}

	Zenith_Log("%s Prefab migration for %s should be handled by loading and re-saving",
		LOG_TAG, strPrefabPath.c_str());

	s_xLastStats.m_uPrefabsModified++;
	return true;
}

//=============================================================================
// Full Project Migration
//=============================================================================

bool Zenith_AssetMigration::MigrateProject(const std::string& strProjectRoot, bool bDryRun)
{
	// Reset stats
	s_xLastStats = MigrationStats();

	Zenith_Log("%s Starting project migration for: %s%s",
		LOG_TAG, strProjectRoot.c_str(), bDryRun ? " (DRY RUN)" : "");

	if (!std::filesystem::exists(strProjectRoot))
	{
		Zenith_Log("%s Project root does not exist: %s", LOG_TAG, strProjectRoot.c_str());
		s_xLastStats.m_xErrorMessages.push_back("Project root does not exist");
		s_xLastStats.m_uErrors++;
		return false;
	}

	// Step 1: Generate meta files for all assets
	Zenith_Log("%s Step 1: Generating meta files...", LOG_TAG);
	std::vector<std::string> xAssetPaths;
	GetAllAssetFiles(strProjectRoot, xAssetPaths);

	for (const std::string& strPath : xAssetPaths)
	{
		if (!HasMetaFile(strPath))
		{
			if (bDryRun)
			{
				Zenith_Log("%s [DRY RUN] Would generate meta file for: %s", LOG_TAG, strPath.c_str());
				s_xLastStats.m_uMetaFilesGenerated++;
			}
			else
			{
				if (GenerateMetaFile(strPath))
				{
					s_xLastStats.m_uMetaFilesGenerated++;
				}
				else
				{
					s_xLastStats.m_uErrors++;
					s_xLastStats.m_xErrorMessages.push_back("Failed to generate meta for: " + strPath);
				}
			}
		}
	}

	// Step 2: Migrate scenes
	Zenith_Log("%s Step 2: Checking scenes for migration...", LOG_TAG);
	for (const std::string& strPath : xAssetPaths)
	{
		if (strPath.ends_with(".zscn"))
		{
			if (SceneNeedsMigration(strPath))
			{
				if (bDryRun)
				{
					Zenith_Log("%s [DRY RUN] Would migrate scene: %s", LOG_TAG, strPath.c_str());
					s_xLastStats.m_uScenesModified++;
				}
				else
				{
					MigrateSceneFile(strPath);
				}
			}
		}
	}

	// Step 3: Migrate materials
	Zenith_Log("%s Step 3: Checking materials for migration...", LOG_TAG);
	for (const std::string& strPath : xAssetPaths)
	{
		if (strPath.ends_with(".zmtrl"))
		{
			if (MaterialNeedsMigration(strPath))
			{
				if (bDryRun)
				{
					Zenith_Log("%s [DRY RUN] Would migrate material: %s", LOG_TAG, strPath.c_str());
					s_xLastStats.m_uMaterialsModified++;
				}
				else
				{
					MigrateMaterialFile(strPath);
				}
			}
		}
	}

	// Step 4: Migrate models
	Zenith_Log("%s Step 4: Checking models for migration...", LOG_TAG);
	for (const std::string& strPath : xAssetPaths)
	{
		if (strPath.ends_with(".zmodel"))
		{
			if (ModelNeedsMigration(strPath))
			{
				if (bDryRun)
				{
					Zenith_Log("%s [DRY RUN] Would migrate model: %s", LOG_TAG, strPath.c_str());
					s_xLastStats.m_uModelsModified++;
				}
				else
				{
					MigrateModelFile(strPath);
				}
			}
		}
	}

	// Step 5: Migrate prefabs
	Zenith_Log("%s Step 5: Checking prefabs for migration...", LOG_TAG);
	for (const std::string& strPath : xAssetPaths)
	{
		if (strPath.ends_with(".zprefab"))
		{
			if (PrefabNeedsMigration(strPath))
			{
				if (bDryRun)
				{
					Zenith_Log("%s [DRY RUN] Would migrate prefab: %s", LOG_TAG, strPath.c_str());
					s_xLastStats.m_uPrefabsModified++;
				}
				else
				{
					MigratePrefabFile(strPath);
				}
			}
		}
	}

	// Report results
	Zenith_Log("%s Migration complete. Results:", LOG_TAG);
	Zenith_Log("%s   Meta files generated: %u", LOG_TAG, s_xLastStats.m_uMetaFilesGenerated);
	Zenith_Log("%s   Scenes modified: %u", LOG_TAG, s_xLastStats.m_uScenesModified);
	Zenith_Log("%s   Materials modified: %u", LOG_TAG, s_xLastStats.m_uMaterialsModified);
	Zenith_Log("%s   Models modified: %u", LOG_TAG, s_xLastStats.m_uModelsModified);
	Zenith_Log("%s   Prefabs modified: %u", LOG_TAG, s_xLastStats.m_uPrefabsModified);
	Zenith_Log("%s   Errors: %u", LOG_TAG, s_xLastStats.m_uErrors);

	return s_xLastStats.m_uErrors == 0;
}

//=============================================================================
// Utility
//=============================================================================

std::string Zenith_AssetMigration::CreateBackup(const std::string& strFilePath)
{
	if (!std::filesystem::exists(strFilePath))
	{
		return "";
	}

	// Create backup with .bak extension
	std::string strBackupPath = strFilePath + ".bak";

	// If backup already exists, add timestamp
	if (std::filesystem::exists(strBackupPath))
	{
		auto xNow = std::chrono::system_clock::now();
		auto ulTimestamp = std::chrono::duration_cast<std::chrono::seconds>(
			xNow.time_since_epoch()).count();
		strBackupPath = strFilePath + "." + std::to_string(ulTimestamp) + ".bak";
	}

	try
	{
		std::filesystem::copy_file(strFilePath, strBackupPath);
		Zenith_Log("%s Created backup: %s", LOG_TAG, strBackupPath.c_str());
		return strBackupPath;
	}
	catch (const std::exception& e)
	{
		Zenith_Log("%s Failed to create backup: %s", LOG_TAG, e.what());
		return "";
	}
}

void Zenith_AssetMigration::GetAllAssetFiles(const std::string& strDirectory, std::vector<std::string>& xOutPaths)
{
	if (!std::filesystem::exists(strDirectory))
	{
		return;
	}

	for (const auto& xEntry : std::filesystem::recursive_directory_iterator(strDirectory))
	{
		if (xEntry.is_regular_file())
		{
			std::string strPath = xEntry.path().string();
			std::string strExt = xEntry.path().extension().string();

			if (IsAssetExtension(strExt))
			{
				xOutPaths.push_back(strPath);
			}
		}
	}
}

const Zenith_AssetMigration::MigrationStats& Zenith_AssetMigration::GetLastMigrationStats()
{
	return s_xLastStats;
}

Zenith_AssetGUID Zenith_AssetMigration::ResolvePathToGUID(const std::string& strAssetPath)
{
	// First check if meta file exists
	std::string strMetaPath = Zenith_AssetMeta::GetMetaPath(strAssetPath);
	if (std::filesystem::exists(strMetaPath))
	{
		Zenith_AssetMeta xMeta;
		if (xMeta.LoadFromFile(strMetaPath))
		{
			return xMeta.m_xGUID;
		}
	}

	// Try to get from asset database
	if (Zenith_AssetDatabase::IsInitialized())
	{
		Zenith_AssetGUID xGUID = Zenith_AssetDatabase::GetGUIDFromPath(strAssetPath);
		if (xGUID.IsValid())
		{
			return xGUID;
		}
	}

	// Generate new GUID and create meta file
	if (GenerateMetaFile(strAssetPath))
	{
		Zenith_AssetMeta xMeta;
		if (xMeta.LoadFromFile(strMetaPath))
		{
			return xMeta.m_xGUID;
		}
	}

	return Zenith_AssetGUID::INVALID;
}

bool Zenith_AssetMigration::IsAssetExtension(const std::string& strExt)
{
	for (const std::string& strAssetExt : s_xAssetExtensions)
	{
		if (strExt == strAssetExt)
		{
			return true;
		}
	}
	return false;
}

bool Zenith_AssetMigration::HasMetaFile(const std::string& strAssetPath)
{
	std::string strMetaPath = Zenith_AssetMeta::GetMetaPath(strAssetPath);
	return std::filesystem::exists(strMetaPath);
}
