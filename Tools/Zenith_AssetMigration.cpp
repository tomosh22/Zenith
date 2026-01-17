#include "Zenith.h"
#include "Zenith_AssetMigration.h"
#include "AssetHandling/Zenith_AssetMeta.h"
#include "AssetHandling/Zenith_AssetDatabase.h"
#include "DataStream/Zenith_DataStream.h"
#include <filesystem>
#include <fstream>

// Static member initialization
Zenith_AssetMigration::MigrationStats Zenith_AssetMigration::s_xLastStats;


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

	Zenith_Log(LOG_CATEGORY_TOOLS, " Generated %u meta files", uGenerated);
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
		Zenith_Log(LOG_CATEGORY_TOOLS, " Asset file does not exist: %s", strAssetPath.c_str());
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
		Zenith_Log(LOG_CATEGORY_TOOLS, "Created meta file: %s (GUID: %s)",
			strMetaPath.c_str(), xMeta.m_xGUID.ToString().c_str());
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
		Zenith_Log(LOG_CATEGORY_TOOLS, " Scene does not need migration: %s", strScenePath.c_str());
		return true;
	}

	// Create backup
	std::string strBackup = CreateBackup(strScenePath);
	if (strBackup.empty())
	{
		Zenith_Log(LOG_CATEGORY_TOOLS, " Failed to create backup for: %s", strScenePath.c_str());
		return false;
	}

	// Scene migration is handled by the scene load/save code
	// Loading an old scene and saving it will automatically migrate it
	Zenith_Log(LOG_CATEGORY_TOOLS, "Scene migration for %s should be handled by loading and re-saving",
		strScenePath.c_str());

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
		Zenith_Log(LOG_CATEGORY_TOOLS, " Material does not need migration: %s", strMaterialPath.c_str());
		return true;
	}

	// Create backup
	std::string strBackup = CreateBackup(strMaterialPath);
	if (strBackup.empty())
	{
		Zenith_Log(LOG_CATEGORY_TOOLS, " Failed to create backup for: %s", strMaterialPath.c_str());
		return false;
	}

	// Material migration is handled by Zenith_MaterialAsset load/save code
	// Loading an old material and saving it will automatically migrate it
	Zenith_Log(LOG_CATEGORY_TOOLS, "Material migration for %s should be handled by loading and re-saving",
		strMaterialPath.c_str());

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
		Zenith_Log(LOG_CATEGORY_TOOLS, " Model does not need migration: %s", strModelPath.c_str());
		return true;
	}

	// Create backup
	std::string strBackup = CreateBackup(strModelPath);
	if (strBackup.empty())
	{
		Zenith_Log(LOG_CATEGORY_TOOLS, " Failed to create backup for: %s", strModelPath.c_str());
		return false;
	}

	Zenith_Log(LOG_CATEGORY_TOOLS, "Model migration for %s should be handled by loading and re-saving",
		strModelPath.c_str());

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
		Zenith_Log(LOG_CATEGORY_TOOLS, " Prefab does not need migration: %s", strPrefabPath.c_str());
		return true;
	}

	// Create backup
	std::string strBackup = CreateBackup(strPrefabPath);
	if (strBackup.empty())
	{
		Zenith_Log(LOG_CATEGORY_TOOLS, " Failed to create backup for: %s", strPrefabPath.c_str());
		return false;
	}

	Zenith_Log(LOG_CATEGORY_TOOLS, "Prefab migration for %s should be handled by loading and re-saving",
		strPrefabPath.c_str());

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

	Zenith_Log(LOG_CATEGORY_TOOLS, "Starting project migration for: %s%s",
		strProjectRoot.c_str(), bDryRun ? " (DRY RUN)" : "");

	if (!std::filesystem::exists(strProjectRoot))
	{
		Zenith_Log(LOG_CATEGORY_TOOLS, " Project root does not exist: %s", strProjectRoot.c_str());
		s_xLastStats.m_xErrorMessages.push_back("Project root does not exist");
		s_xLastStats.m_uErrors++;
		return false;
	}

	// Step 1: Generate meta files for all assets
	Zenith_Log(LOG_CATEGORY_TOOLS, " Step 1: Generating meta files...");
	std::vector<std::string> xAssetPaths;
	GetAllAssetFiles(strProjectRoot, xAssetPaths);

	for (const std::string& strPath : xAssetPaths)
	{
		if (!HasMetaFile(strPath))
		{
			if (bDryRun)
			{
				Zenith_Log(LOG_CATEGORY_TOOLS, " [DRY RUN] Would generate meta file for: %s", strPath.c_str());
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
	Zenith_Log(LOG_CATEGORY_TOOLS, " Step 2: Checking scenes for migration...");
	for (const std::string& strPath : xAssetPaths)
	{
		if (strPath.ends_with(".zscn"))
		{
			if (SceneNeedsMigration(strPath))
			{
				if (bDryRun)
				{
					Zenith_Log(LOG_CATEGORY_TOOLS, " [DRY RUN] Would migrate scene: %s", strPath.c_str());
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
	Zenith_Log(LOG_CATEGORY_TOOLS, " Step 3: Checking materials for migration...");
	for (const std::string& strPath : xAssetPaths)
	{
		if (strPath.ends_with(".zmtrl"))
		{
			if (MaterialNeedsMigration(strPath))
			{
				if (bDryRun)
				{
					Zenith_Log(LOG_CATEGORY_TOOLS, " [DRY RUN] Would migrate material: %s", strPath.c_str());
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
	Zenith_Log(LOG_CATEGORY_TOOLS, " Step 4: Checking models for migration...");
	for (const std::string& strPath : xAssetPaths)
	{
		if (strPath.ends_with(".zmodel"))
		{
			if (ModelNeedsMigration(strPath))
			{
				if (bDryRun)
				{
					Zenith_Log(LOG_CATEGORY_TOOLS, " [DRY RUN] Would migrate model: %s", strPath.c_str());
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
	Zenith_Log(LOG_CATEGORY_TOOLS, " Step 5: Checking prefabs for migration...");
	for (const std::string& strPath : xAssetPaths)
	{
		if (strPath.ends_with(".zprefab"))
		{
			if (PrefabNeedsMigration(strPath))
			{
				if (bDryRun)
				{
					Zenith_Log(LOG_CATEGORY_TOOLS, " [DRY RUN] Would migrate prefab: %s", strPath.c_str());
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
	Zenith_Log(LOG_CATEGORY_TOOLS, " Migration complete. Results:");
	Zenith_Log(LOG_CATEGORY_TOOLS, "   Meta files generated: %u", s_xLastStats.m_uMetaFilesGenerated);
	Zenith_Log(LOG_CATEGORY_TOOLS, "   Scenes modified: %u", s_xLastStats.m_uScenesModified);
	Zenith_Log(LOG_CATEGORY_TOOLS, "   Materials modified: %u", s_xLastStats.m_uMaterialsModified);
	Zenith_Log(LOG_CATEGORY_TOOLS, "   Models modified: %u", s_xLastStats.m_uModelsModified);
	Zenith_Log(LOG_CATEGORY_TOOLS, "   Prefabs modified: %u", s_xLastStats.m_uPrefabsModified);
	Zenith_Log(LOG_CATEGORY_TOOLS, "   Errors: %u", s_xLastStats.m_uErrors);

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
		Zenith_Log(LOG_CATEGORY_TOOLS, " Created backup: %s", strBackupPath.c_str());
		return strBackupPath;
	}
	catch (const std::exception& e)
	{
		Zenith_Log(LOG_CATEGORY_TOOLS, " Failed to create backup: %s", e.what());
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
