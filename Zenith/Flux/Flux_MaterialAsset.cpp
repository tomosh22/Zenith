#include "Zenith.h"
#include "Flux/Flux_MaterialAsset.h"
#include "Flux/Flux_Graphics.h"
#include "AssetHandling/Zenith_AssetHandler.h"
#include "DataStream/Zenith_DataStream.h"

// Static member initialization
std::unordered_map<std::string, Flux_MaterialAsset*> Flux_MaterialAsset::s_xMaterialCache;
std::unordered_map<std::string, Flux_Texture*> Flux_MaterialAsset::s_xTextureCache;
uint32_t Flux_MaterialAsset::s_uNextMaterialID = 1;

static constexpr const char* LOG_TAG = "[MaterialAsset]";

//------------------------------------------------------------------------------
// Static Registry Methods
//------------------------------------------------------------------------------

void Flux_MaterialAsset::Initialize()
{
	Zenith_Log("%s Material asset system initialized", LOG_TAG);
}

void Flux_MaterialAsset::Shutdown()
{
	UnloadAll();
	Zenith_Log("%s Material asset system shut down", LOG_TAG);
}

Flux_MaterialAsset* Flux_MaterialAsset::Create(const std::string& strName)
{
	Flux_MaterialAsset* pMaterial = new Flux_MaterialAsset();
	
	if (strName.empty())
	{
		pMaterial->m_strName = "Material_" + std::to_string(s_uNextMaterialID++);
	}
	else
	{
		pMaterial->m_strName = strName;
	}
	
	pMaterial->m_bDirty = true;
	
	Zenith_Log("%s Created new material: %s", LOG_TAG, pMaterial->m_strName.c_str());
	
	return pMaterial;
}

Flux_MaterialAsset* Flux_MaterialAsset::LoadFromFile(const std::string& strPath)
{
	// Check cache first
	auto it = s_xMaterialCache.find(strPath);
	if (it != s_xMaterialCache.end())
	{
		Zenith_Log("%s Returning cached material: %s", LOG_TAG, strPath.c_str());
		return it->second;
	}
	
	// Check if file exists before loading
	if (!std::filesystem::exists(strPath))
	{
		Zenith_Log("%s ERROR: Material file not found: %s", LOG_TAG, strPath.c_str());
		return nullptr;
	}
	
	// Load from file
	Zenith_DataStream xStream;
	xStream.ReadFromFile(strPath.c_str());
	
	Flux_MaterialAsset* pMaterial = new Flux_MaterialAsset();
	pMaterial->ReadFromDataStream(xStream);
	pMaterial->m_strAssetPath = strPath;
	pMaterial->m_bDirty = false;
	
	// Add to cache
	s_xMaterialCache[strPath] = pMaterial;
	
	Zenith_Log("%s Loaded material from file: %s (name: %s)", LOG_TAG, strPath.c_str(), pMaterial->m_strName.c_str());
	
	return pMaterial;
}

Flux_MaterialAsset* Flux_MaterialAsset::GetByPath(const std::string& strPath)
{
	auto it = s_xMaterialCache.find(strPath);
	if (it != s_xMaterialCache.end())
	{
		return it->second;
	}
	return nullptr;
}

void Flux_MaterialAsset::Unload(const std::string& strPath)
{
	auto it = s_xMaterialCache.find(strPath);
	if (it != s_xMaterialCache.end())
	{
		Zenith_Log("%s Unloading material: %s", LOG_TAG, strPath.c_str());
		delete it->second;
		s_xMaterialCache.erase(it);
	}
}

void Flux_MaterialAsset::UnloadAll()
{
	Zenith_Log("%s Unloading all materials (%zu cached)", LOG_TAG, s_xMaterialCache.size());
	
	for (auto& pair : s_xMaterialCache)
	{
		delete pair.second;
	}
	s_xMaterialCache.clear();
	
	// Also clear texture cache
	for (auto& pair : s_xTextureCache)
	{
		if (pair.second)
		{
			Zenith_AssetHandler::DeleteTexture(pair.second);
		}
	}
	s_xTextureCache.clear();
	
	Zenith_Log("%s All materials and textures unloaded", LOG_TAG);
}

void Flux_MaterialAsset::ReloadAll()
{
	Zenith_Log("%s Reloading all materials (%zu cached)", LOG_TAG, s_xMaterialCache.size());
	
	// Collect paths to reload
	std::vector<std::string> paths;
	for (const auto& pair : s_xMaterialCache)
	{
		paths.push_back(pair.first);
	}
	
	// Reload each material (this will reload textures)
	for (const std::string& strPath : paths)
	{
		auto it = s_xMaterialCache.find(strPath);
		if (it != s_xMaterialCache.end())
		{
			it->second->Reload();
		}
	}
	
	Zenith_Log("%s All materials reloaded", LOG_TAG);
}

void Flux_MaterialAsset::GetAllLoadedMaterialPaths(std::vector<std::string>& outPaths)
{
	outPaths.clear();
	outPaths.reserve(s_xMaterialCache.size());
	
	for (const auto& pair : s_xMaterialCache)
	{
		outPaths.push_back(pair.first);
	}
}

//------------------------------------------------------------------------------
// Instance Methods
//------------------------------------------------------------------------------

Flux_MaterialAsset::Flux_MaterialAsset()
{
}

Flux_MaterialAsset::~Flux_MaterialAsset()
{
	UnloadTextures();
}

void Flux_MaterialAsset::UnloadTextures()
{
	// Note: Textures are managed by the shared cache, we just clear our references
	m_pxDiffuseTexture = nullptr;
	m_pxNormalTexture = nullptr;
	m_pxRoughnessMetallicTexture = nullptr;
	m_pxOcclusionTexture = nullptr;
	m_pxEmissiveTexture = nullptr;
}

//------------------------------------------------------------------------------
// Texture Path Setters
//------------------------------------------------------------------------------

void Flux_MaterialAsset::SetDiffuseTexturePath(const std::string& strPath)
{
	if (m_strDiffuseTexturePath != strPath)
	{
		m_strDiffuseTexturePath = strPath;
		m_pxDiffuseTexture = nullptr;  // Will reload on next Get
		m_bDirty = true;
	}
}

void Flux_MaterialAsset::SetNormalTexturePath(const std::string& strPath)
{
	if (m_strNormalTexturePath != strPath)
	{
		m_strNormalTexturePath = strPath;
		m_pxNormalTexture = nullptr;
		m_bDirty = true;
	}
}

void Flux_MaterialAsset::SetRoughnessMetallicTexturePath(const std::string& strPath)
{
	if (m_strRoughnessMetallicTexturePath != strPath)
	{
		m_strRoughnessMetallicTexturePath = strPath;
		m_pxRoughnessMetallicTexture = nullptr;
		m_bDirty = true;
	}
}

void Flux_MaterialAsset::SetOcclusionTexturePath(const std::string& strPath)
{
	if (m_strOcclusionTexturePath != strPath)
	{
		m_strOcclusionTexturePath = strPath;
		m_pxOcclusionTexture = nullptr;
		m_bDirty = true;
	}
}

void Flux_MaterialAsset::SetEmissiveTexturePath(const std::string& strPath)
{
	if (m_strEmissiveTexturePath != strPath)
	{
		m_strEmissiveTexturePath = strPath;
		m_pxEmissiveTexture = nullptr;
		m_bDirty = true;
	}
}

//------------------------------------------------------------------------------
// Texture Loading (with caching)
//------------------------------------------------------------------------------

Flux_Texture* Flux_MaterialAsset::LoadTextureFromPath(const std::string& strPath)
{
	if (strPath.empty())
	{
		return nullptr;
	}
	
	// Check shared texture cache
	auto it = s_xTextureCache.find(strPath);
	if (it != s_xTextureCache.end())
	{
		return it->second;
	}
	
	// Load from file
	Zenith_AssetHandler::TextureData xTexData = Zenith_AssetHandler::LoadTexture2DFromFile(strPath.c_str());
	if (!xTexData.pData)
	{
		Zenith_Log("%s ERROR: Failed to load texture: %s", LOG_TAG, strPath.c_str());
		return nullptr;
	}
	
	Flux_Texture* pTexture = Zenith_AssetHandler::AddTexture(xTexData);
	xTexData.FreeAllocatedData();
	
	if (pTexture)
	{
		pTexture->m_strSourcePath = strPath;
		s_xTextureCache[strPath] = pTexture;
		Zenith_Log("%s Loaded texture: %s", LOG_TAG, strPath.c_str());
	}
	
	return pTexture;
}

//------------------------------------------------------------------------------
// Texture Accessors
//------------------------------------------------------------------------------

const Flux_Texture* Flux_MaterialAsset::GetDiffuseTexture()
{
	if (!m_pxDiffuseTexture && !m_strDiffuseTexturePath.empty())
	{
		m_pxDiffuseTexture = LoadTextureFromPath(m_strDiffuseTexturePath);
	}
	return m_pxDiffuseTexture ? m_pxDiffuseTexture : &Flux_Graphics::s_xWhiteBlankTexture2D;
}

const Flux_Texture* Flux_MaterialAsset::GetNormalTexture()
{
	if (!m_pxNormalTexture && !m_strNormalTexturePath.empty())
	{
		m_pxNormalTexture = LoadTextureFromPath(m_strNormalTexturePath);
	}
	return m_pxNormalTexture ? m_pxNormalTexture : &Flux_Graphics::s_xWhiteBlankTexture2D;
}

const Flux_Texture* Flux_MaterialAsset::GetRoughnessMetallicTexture()
{
	if (!m_pxRoughnessMetallicTexture && !m_strRoughnessMetallicTexturePath.empty())
	{
		m_pxRoughnessMetallicTexture = LoadTextureFromPath(m_strRoughnessMetallicTexturePath);
	}
	return m_pxRoughnessMetallicTexture ? m_pxRoughnessMetallicTexture : &Flux_Graphics::s_xWhiteBlankTexture2D;
}

const Flux_Texture* Flux_MaterialAsset::GetOcclusionTexture()
{
	if (!m_pxOcclusionTexture && !m_strOcclusionTexturePath.empty())
	{
		m_pxOcclusionTexture = LoadTextureFromPath(m_strOcclusionTexturePath);
	}
	return m_pxOcclusionTexture ? m_pxOcclusionTexture : &Flux_Graphics::s_xWhiteBlankTexture2D;
}

const Flux_Texture* Flux_MaterialAsset::GetEmissiveTexture()
{
	if (!m_pxEmissiveTexture && !m_strEmissiveTexturePath.empty())
	{
		m_pxEmissiveTexture = LoadTextureFromPath(m_strEmissiveTexturePath);
	}
	return m_pxEmissiveTexture ? m_pxEmissiveTexture : &Flux_Graphics::s_xWhiteBlankTexture2D;
}

//------------------------------------------------------------------------------
// Serialization
//------------------------------------------------------------------------------

bool Flux_MaterialAsset::SaveToFile(const std::string& strPath)
{
	Zenith_DataStream xStream;
	WriteToDataStream(xStream);
	
	xStream.WriteToFile(strPath.c_str());
	
	m_strAssetPath = strPath;
	m_bDirty = false;
	
	// Add to cache if not already cached
	if (s_xMaterialCache.find(strPath) == s_xMaterialCache.end())
	{
		s_xMaterialCache[strPath] = this;
	}
	
	Zenith_Log("%s Saved material to: %s", LOG_TAG, strPath.c_str());
	return true;
}

void Flux_MaterialAsset::Reload()
{
	if (m_strAssetPath.empty())
	{
		Zenith_Log("%s Cannot reload material without asset path", LOG_TAG);
		return;
	}
	
	Zenith_Log("%s Reloading material: %s", LOG_TAG, m_strAssetPath.c_str());
	
	// Clear cached textures so they reload
	UnloadTextures();
	
	// Reload from file
	if (!std::filesystem::exists(m_strAssetPath))
	{
		Zenith_Log("%s ERROR: Material file not found: %s", LOG_TAG, m_strAssetPath.c_str());
		return;
	}
	
	Zenith_DataStream xStream;
	xStream.ReadFromFile(m_strAssetPath.c_str());
	ReadFromDataStream(xStream);
	m_bDirty = false;
}

void Flux_MaterialAsset::WriteToDataStream(Zenith_DataStream& xStream) const
{
	// File version
	uint32_t uVersion = MATERIAL_FILE_VERSION;
	xStream << uVersion;
	
	// Material identity
	xStream << m_strName;
	
	// Material properties
	xStream << m_xBaseColor.x;
	xStream << m_xBaseColor.y;
	xStream << m_xBaseColor.z;
	xStream << m_xBaseColor.w;
	
	xStream << m_fMetallic;
	xStream << m_fRoughness;
	
	xStream << m_xEmissiveColor.x;
	xStream << m_xEmissiveColor.y;
	xStream << m_xEmissiveColor.z;
	xStream << m_fEmissiveIntensity;
	
	xStream << m_bTransparent;
	xStream << m_fAlphaCutoff;
	
	// Texture paths
	xStream << m_strDiffuseTexturePath;
	xStream << m_strNormalTexturePath;
	xStream << m_strRoughnessMetallicTexturePath;
	xStream << m_strOcclusionTexturePath;
	xStream << m_strEmissiveTexturePath;
}

void Flux_MaterialAsset::ReadFromDataStream(Zenith_DataStream& xStream)
{
	// File version
	uint32_t uVersion;
	xStream >> uVersion;
	
	if (uVersion > MATERIAL_FILE_VERSION)
	{
		Zenith_Log("%s WARNING: Material file version %u is newer than supported version %u",
			LOG_TAG, uVersion, MATERIAL_FILE_VERSION);
	}
	
	// Material identity
	xStream >> m_strName;
	
	// Material properties
	xStream >> m_xBaseColor.x;
	xStream >> m_xBaseColor.y;
	xStream >> m_xBaseColor.z;
	xStream >> m_xBaseColor.w;
	
	xStream >> m_fMetallic;
	xStream >> m_fRoughness;
	
	xStream >> m_xEmissiveColor.x;
	xStream >> m_xEmissiveColor.y;
	xStream >> m_xEmissiveColor.z;
	xStream >> m_fEmissiveIntensity;
	
	xStream >> m_bTransparent;
	xStream >> m_fAlphaCutoff;
	
	// Texture paths
	xStream >> m_strDiffuseTexturePath;
	xStream >> m_strNormalTexturePath;
	xStream >> m_strRoughnessMetallicTexturePath;
	xStream >> m_strOcclusionTexturePath;
	xStream >> m_strEmissiveTexturePath;
	
	// Clear cached textures so they reload from new paths
	UnloadTextures();
}
