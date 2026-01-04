#pragma once

#include "Core/Zenith_GUID.h"
#include <string>
#include <cstdint>

class Zenith_DataStream;

/**
 * Asset type enumeration
 * Used to identify what kind of asset a GUID references
 */
enum class Zenith_AssetType : uint32_t
{
	UNKNOWN = 0,
	TEXTURE,
	MESH,
	SKELETON,
	ANIMATION,
	MATERIAL,
	MODEL,
	PREFAB,
	SCENE,
	COUNT
};

/**
 * Get file extension for an asset type
 */
const char* Zenith_GetAssetTypeExtension(Zenith_AssetType eType);

/**
 * Determine asset type from file extension
 */
Zenith_AssetType Zenith_GetAssetTypeFromExtension(const std::string& strExtension);

/**
 * Zenith_TextureImportSettings - Import settings for textures
 */
struct Zenith_TextureImportSettings
{
	bool m_bGenerateMipmaps = true;
	bool m_bSRGB = true;
	bool m_bCompressed = true;
	uint32_t m_uMaxSize = 4096;

	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);
};

/**
 * Zenith_MeshImportSettings - Import settings for meshes
 */
struct Zenith_MeshImportSettings
{
	bool m_bCalculateNormals = false;
	bool m_bCalculateTangents = true;
	float m_fScale = 1.0f;
	bool m_bFlipUVs = false;

	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);
};

/**
 * Zenith_AssetMeta - Metadata file for an asset (.zmeta)
 *
 * Every asset in the project has an associated .zmeta file that stores:
 * - A unique GUID for stable references
 * - The asset's relative path
 * - Last modification timestamp
 * - Asset type
 * - Type-specific import settings
 *
 * Meta files are stored alongside their asset:
 *   Assets/Textures/MyTexture.ztxtr
 *   Assets/Textures/MyTexture.ztxtr.zmeta
 */
class Zenith_AssetMeta
{
public:
	static constexpr uint32_t META_VERSION = 1;
	static constexpr uint32_t META_MAGIC = 0x5A4D4554;  // "ZMET"
	static constexpr const char* META_EXTENSION = ".zmeta";

	Zenith_AssetMeta() = default;

	//--------------------------------------------------------------------------
	// Core Properties
	//--------------------------------------------------------------------------

	Zenith_AssetGUID m_xGUID;
	std::string m_strAssetPath;          // Relative path from project root
	uint64_t m_ulLastModifiedTime = 0;   // File modification timestamp
	Zenith_AssetType m_eAssetType = Zenith_AssetType::UNKNOWN;

	//--------------------------------------------------------------------------
	// Import Settings (type-specific)
	//--------------------------------------------------------------------------

	Zenith_TextureImportSettings m_xTextureSettings;
	Zenith_MeshImportSettings m_xMeshSettings;

	//--------------------------------------------------------------------------
	// File I/O
	//--------------------------------------------------------------------------

	/**
	 * Load meta data from file
	 * @param strMetaPath Full path to .zmeta file
	 * @return true on success
	 */
	bool LoadFromFile(const std::string& strMetaPath);

	/**
	 * Save meta data to file
	 * @param strMetaPath Full path to .zmeta file
	 * @return true on success
	 */
	bool SaveToFile(const std::string& strMetaPath) const;

	/**
	 * Get the meta file path for an asset
	 * @param strAssetPath Path to the asset file
	 * @return Path to the .zmeta file (asset path + .zmeta)
	 */
	static std::string GetMetaPath(const std::string& strAssetPath);

	/**
	 * Check if a meta file exists for an asset
	 */
	static bool MetaFileExists(const std::string& strAssetPath);

	/**
	 * Create a new meta file for an asset
	 * Generates a new GUID and detects asset type from extension
	 * @param strAssetPath Full path to the asset file
	 * @param strProjectRoot Project root for computing relative path
	 * @return true on success
	 */
	bool CreateForAsset(const std::string& strAssetPath, const std::string& strProjectRoot);

	//--------------------------------------------------------------------------
	// Serialization
	//--------------------------------------------------------------------------

	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

	//--------------------------------------------------------------------------
	// Utility
	//--------------------------------------------------------------------------

	/**
	 * Check if this meta data is valid (has a valid GUID)
	 */
	bool IsValid() const { return m_xGUID.IsValid(); }

	/**
	 * Update the modification timestamp to current time
	 */
	void UpdateModificationTime();
};
