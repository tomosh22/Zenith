#include "Zenith.h"
#include "Zenith_AssetMeta.h"
#include "DataStream/Zenith_DataStream.h"
#include "FileAccess/Zenith_FileAccess.h"

#include <chrono>
#include <algorithm>

// Asset type extensions (must match Zenith_AssetType enum order)
static const char* s_aszAssetTypeExtensions[] =
{
	"",           // UNKNOWN
	".ztxtr",     // TEXTURE
	".zmesh",     // MESH
	".zskel",     // SKELETON
	".zanim",     // ANIMATION
	".zmtrl",     // MATERIAL
	".zmodel",    // MODEL
	".zprfb",     // PREFAB
	".zscn",      // SCENE
};
static_assert(sizeof(s_aszAssetTypeExtensions) / sizeof(s_aszAssetTypeExtensions[0]) == static_cast<size_t>(Zenith_AssetType::COUNT),
	"Asset type extensions array size mismatch");

const char* Zenith_GetAssetTypeExtension(Zenith_AssetType eType)
{
	if (eType >= Zenith_AssetType::COUNT)
		return "";
	return s_aszAssetTypeExtensions[static_cast<size_t>(eType)];
}

Zenith_AssetType Zenith_GetAssetTypeFromExtension(const std::string& strExtension)
{
	// Convert to lowercase for comparison
	std::string strLower = strExtension;
	std::transform(strLower.begin(), strLower.end(), strLower.begin(), ::tolower);

	for (size_t i = 0; i < static_cast<size_t>(Zenith_AssetType::COUNT); ++i)
	{
		if (strLower == s_aszAssetTypeExtensions[i])
		{
			return static_cast<Zenith_AssetType>(i);
		}
	}
	return Zenith_AssetType::UNKNOWN;
}

//------------------------------------------------------------------------------
// Zenith_TextureImportSettings
//------------------------------------------------------------------------------

void Zenith_TextureImportSettings::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << m_bGenerateMipmaps;
	xStream << m_bSRGB;
	xStream << m_bCompressed;
	xStream << m_uMaxSize;
}

void Zenith_TextureImportSettings::ReadFromDataStream(Zenith_DataStream& xStream)
{
	xStream >> m_bGenerateMipmaps;
	xStream >> m_bSRGB;
	xStream >> m_bCompressed;
	xStream >> m_uMaxSize;
}

//------------------------------------------------------------------------------
// Zenith_MeshImportSettings
//------------------------------------------------------------------------------

void Zenith_MeshImportSettings::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << m_bCalculateNormals;
	xStream << m_bCalculateTangents;
	xStream << m_fScale;
	xStream << m_bFlipUVs;
}

void Zenith_MeshImportSettings::ReadFromDataStream(Zenith_DataStream& xStream)
{
	xStream >> m_bCalculateNormals;
	xStream >> m_bCalculateTangents;
	xStream >> m_fScale;
	xStream >> m_bFlipUVs;
}

//------------------------------------------------------------------------------
// Zenith_AssetMeta
//------------------------------------------------------------------------------

bool Zenith_AssetMeta::LoadFromFile(const std::string& strMetaPath)
{
	Zenith_DataStream xStream;
	xStream.ReadFromFile(strMetaPath.c_str());

	// Validate magic number
	uint32_t uMagic = 0;
	xStream >> uMagic;
	if (uMagic != META_MAGIC)
	{
		Zenith_Log("Error: Invalid meta file magic number in %s", strMetaPath.c_str());
		return false;
	}

	// Validate version
	uint32_t uVersion = 0;
	xStream >> uVersion;
	if (uVersion > META_VERSION)
	{
		Zenith_Log("Error: Unsupported meta file version %u in %s", uVersion, strMetaPath.c_str());
		return false;
	}

	ReadFromDataStream(xStream);
	return true;
}

bool Zenith_AssetMeta::SaveToFile(const std::string& strMetaPath) const
{
	Zenith_DataStream xStream;

	// Write header
	xStream << META_MAGIC;
	xStream << META_VERSION;

	WriteToDataStream(xStream);

	xStream.WriteToFile(strMetaPath.c_str());
	return true;
}

std::string Zenith_AssetMeta::GetMetaPath(const std::string& strAssetPath)
{
	return strAssetPath + META_EXTENSION;
}

bool Zenith_AssetMeta::MetaFileExists(const std::string& strAssetPath)
{
	std::string strMetaPath = GetMetaPath(strAssetPath);
	return Zenith_FileAccess::FileExists(strMetaPath.c_str());
}

bool Zenith_AssetMeta::CreateForAsset(const std::string& strAssetPath, const std::string& strProjectRoot)
{
	// Generate a new GUID
	m_xGUID = Zenith_AssetGUID::Generate();

	// Compute relative path
	if (strAssetPath.find(strProjectRoot) == 0)
	{
		// Remove project root prefix
		m_strAssetPath = strAssetPath.substr(strProjectRoot.length());
		// Remove leading slash if present
		if (!m_strAssetPath.empty() && (m_strAssetPath[0] == '/' || m_strAssetPath[0] == '\\'))
		{
			m_strAssetPath = m_strAssetPath.substr(1);
		}
	}
	else
	{
		// Use full path if not under project root
		m_strAssetPath = strAssetPath;
	}

	// Determine asset type from extension
	size_t uDotPos = strAssetPath.rfind('.');
	if (uDotPos != std::string::npos)
	{
		std::string strExtension = strAssetPath.substr(uDotPos);
		m_eAssetType = Zenith_GetAssetTypeFromExtension(strExtension);
	}

	// Update modification time
	UpdateModificationTime();

	// Save the meta file
	std::string strMetaPath = GetMetaPath(strAssetPath);
	return SaveToFile(strMetaPath);
}

void Zenith_AssetMeta::WriteToDataStream(Zenith_DataStream& xStream) const
{
	// Core properties
	xStream << m_xGUID;
	xStream << m_strAssetPath;
	xStream << m_ulLastModifiedTime;
	xStream << static_cast<uint32_t>(m_eAssetType);

	// Type-specific settings
	switch (m_eAssetType)
	{
	case Zenith_AssetType::TEXTURE:
		xStream << m_xTextureSettings;
		break;
	case Zenith_AssetType::MESH:
		xStream << m_xMeshSettings;
		break;
	default:
		// No additional settings for other types
		break;
	}
}

void Zenith_AssetMeta::ReadFromDataStream(Zenith_DataStream& xStream)
{
	// Core properties
	xStream >> m_xGUID;
	xStream >> m_strAssetPath;
	xStream >> m_ulLastModifiedTime;
	uint32_t uAssetType = 0;
	xStream >> uAssetType;
	m_eAssetType = static_cast<Zenith_AssetType>(uAssetType);

	// Type-specific settings
	switch (m_eAssetType)
	{
	case Zenith_AssetType::TEXTURE:
		xStream >> m_xTextureSettings;
		break;
	case Zenith_AssetType::MESH:
		xStream >> m_xMeshSettings;
		break;
	default:
		// No additional settings for other types
		break;
	}
}

void Zenith_AssetMeta::UpdateModificationTime()
{
	auto now = std::chrono::system_clock::now();
	m_ulLastModifiedTime = std::chrono::duration_cast<std::chrono::seconds>(
		now.time_since_epoch()
	).count();
}
