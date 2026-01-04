#include "Zenith.h"

#include "Core/Multithreading/Zenith_Multithreading.h"
#include "Flux/Flux_MaterialAsset.h"
#include "Flux/Flux_Graphics.h"
#include "AssetHandling/Zenith_AssetHandler.h"
#include "AssetHandling/Zenith_AssetDatabase.h"
#include "DataStream/Zenith_DataStream.h"
#include <algorithm>

// Static member initialization
Zenith_Mutex Flux_MaterialAsset::s_xCacheMutex;
std::unordered_map<std::string, Flux_MaterialAsset*> Flux_MaterialAsset::s_xMaterialCache;
std::unordered_map<std::string, Flux_Texture*> Flux_MaterialAsset::s_xTextureCache;
uint32_t Flux_MaterialAsset::s_uNextMaterialID = 1;
std::vector<Flux_MaterialAsset*> Flux_MaterialAsset::s_xAllMaterials;

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
	Zenith_ScopedMutexLock xLock(s_xCacheMutex);

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

	// Register in the global material list for editor visibility
	s_xAllMaterials.push_back(pMaterial);

	Zenith_Log("%s Created new material: %s (total: %zu)", LOG_TAG, pMaterial->m_strName.c_str(), s_xAllMaterials.size());

	return pMaterial;
}

Flux_MaterialAsset* Flux_MaterialAsset::LoadFromFile(const std::string& strPath)
{
	Zenith_ScopedMutexLock xLock(s_xCacheMutex);

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

	// Register in the global material list for editor visibility
	s_xAllMaterials.push_back(pMaterial);

	Zenith_Log("%s Loaded material from file: %s (name: %s, total: %zu)", LOG_TAG, strPath.c_str(), pMaterial->m_strName.c_str(), s_xAllMaterials.size());

	return pMaterial;
}

Flux_MaterialAsset* Flux_MaterialAsset::GetByPath(const std::string& strPath)
{
	Zenith_ScopedMutexLock xLock(s_xCacheMutex);

	auto it = s_xMaterialCache.find(strPath);
	if (it != s_xMaterialCache.end())
	{
		return it->second;
	}
	return nullptr;
}

void Flux_MaterialAsset::Unload(const std::string& strPath)
{
	Zenith_ScopedMutexLock xLock(s_xCacheMutex);

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
	Zenith_ScopedMutexLock xLock(s_xCacheMutex);

	Zenith_Log("%s Unloading all materials (%zu cached, %zu total)", LOG_TAG, s_xMaterialCache.size(), s_xAllMaterials.size());

	for (auto& pair : s_xMaterialCache)
	{
		delete pair.second;
	}
	s_xMaterialCache.clear();

	// Clear the global material list (materials were deleted above via cache)
	// Note: Some materials in s_xAllMaterials may not be in the cache (runtime-created)
	// Those will be cleaned up by component destructors
	s_xAllMaterials.clear();

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
	Zenith_ScopedMutexLock xLock(s_xCacheMutex);

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
	Zenith_ScopedMutexLock xLock(s_xCacheMutex);

	outPaths.clear();
	outPaths.reserve(s_xMaterialCache.size());

	for (const auto& pair : s_xMaterialCache)
	{
		outPaths.push_back(pair.first);
	}
}

void Flux_MaterialAsset::GetAllMaterials(std::vector<Flux_MaterialAsset*>& outMaterials)
{
	Zenith_ScopedMutexLock xLock(s_xCacheMutex);

	outMaterials.clear();
	outMaterials.reserve(s_xAllMaterials.size());

	for (Flux_MaterialAsset* pMaterial : s_xAllMaterials)
	{
		if (pMaterial)
		{
			outMaterials.push_back(pMaterial);
		}
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

	// Remove from global material list
	auto it = std::find(s_xAllMaterials.begin(), s_xAllMaterials.end(), this);
	if (it != s_xAllMaterials.end())
	{
		s_xAllMaterials.erase(it);
	}
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
// Texture Reference Setters (GUID-based)
//------------------------------------------------------------------------------

void Flux_MaterialAsset::SetDiffuseTextureRef(const TextureRef& xRef)
{
	if (m_xDiffuseTextureRef.GetGUID() != xRef.GetGUID())
	{
		m_xDiffuseTextureRef = xRef;
		// Load texture immediately via path lookup
		std::string strPath = xRef.GetPath();
		m_pxDiffuseTexture = strPath.empty() ? nullptr : LoadTextureFromPath(strPath);
		m_bDirty = true;
	}
}

void Flux_MaterialAsset::SetNormalTextureRef(const TextureRef& xRef)
{
	if (m_xNormalTextureRef.GetGUID() != xRef.GetGUID())
	{
		m_xNormalTextureRef = xRef;
		std::string strPath = xRef.GetPath();
		m_pxNormalTexture = strPath.empty() ? nullptr : LoadTextureFromPath(strPath);
		m_bDirty = true;
	}
}

void Flux_MaterialAsset::SetRoughnessMetallicTextureRef(const TextureRef& xRef)
{
	if (m_xRoughnessMetallicTextureRef.GetGUID() != xRef.GetGUID())
	{
		m_xRoughnessMetallicTextureRef = xRef;
		std::string strPath = xRef.GetPath();
		m_pxRoughnessMetallicTexture = strPath.empty() ? nullptr : LoadTextureFromPath(strPath);
		m_bDirty = true;
	}
}

void Flux_MaterialAsset::SetOcclusionTextureRef(const TextureRef& xRef)
{
	if (m_xOcclusionTextureRef.GetGUID() != xRef.GetGUID())
	{
		m_xOcclusionTextureRef = xRef;
		std::string strPath = xRef.GetPath();
		m_pxOcclusionTexture = strPath.empty() ? nullptr : LoadTextureFromPath(strPath);
		m_bDirty = true;
	}
}

void Flux_MaterialAsset::SetEmissiveTextureRef(const TextureRef& xRef)
{
	if (m_xEmissiveTextureRef.GetGUID() != xRef.GetGUID())
	{
		m_xEmissiveTextureRef = xRef;
		std::string strPath = xRef.GetPath();
		m_pxEmissiveTexture = strPath.empty() ? nullptr : LoadTextureFromPath(strPath);
		m_bDirty = true;
	}
}

//------------------------------------------------------------------------------
// Texture Path Setters (compatibility - converts paths to GUID refs)
//------------------------------------------------------------------------------

void Flux_MaterialAsset::SetDiffuseTexturePath(const std::string& strPath)
{
	std::string strCurrentPath = m_xDiffuseTextureRef.GetPath();
	if (strCurrentPath != strPath)
	{
		// Try to find GUID for this path in the asset database
		if (!strPath.empty())
		{
			m_xDiffuseTextureRef.SetFromPath(strPath);
		}
		else
		{
			m_xDiffuseTextureRef.Clear();
		}
		// Load texture immediately to avoid threading issues during rendering
		m_pxDiffuseTexture = LoadTextureFromPath(strPath);
		m_bDirty = true;
	}
}

void Flux_MaterialAsset::SetNormalTexturePath(const std::string& strPath)
{
	std::string strCurrentPath = m_xNormalTextureRef.GetPath();
	if (strCurrentPath != strPath)
	{
		if (!strPath.empty())
		{
			m_xNormalTextureRef.SetFromPath(strPath);
		}
		else
		{
			m_xNormalTextureRef.Clear();
		}
		m_pxNormalTexture = LoadTextureFromPath(strPath);
		m_bDirty = true;
	}
}

void Flux_MaterialAsset::SetRoughnessMetallicTexturePath(const std::string& strPath)
{
	std::string strCurrentPath = m_xRoughnessMetallicTextureRef.GetPath();
	if (strCurrentPath != strPath)
	{
		if (!strPath.empty())
		{
			m_xRoughnessMetallicTextureRef.SetFromPath(strPath);
		}
		else
		{
			m_xRoughnessMetallicTextureRef.Clear();
		}
		m_pxRoughnessMetallicTexture = LoadTextureFromPath(strPath);
		m_bDirty = true;
	}
}

void Flux_MaterialAsset::SetOcclusionTexturePath(const std::string& strPath)
{
	std::string strCurrentPath = m_xOcclusionTextureRef.GetPath();
	if (strCurrentPath != strPath)
	{
		if (!strPath.empty())
		{
			m_xOcclusionTextureRef.SetFromPath(strPath);
		}
		else
		{
			m_xOcclusionTextureRef.Clear();
		}
		m_pxOcclusionTexture = LoadTextureFromPath(strPath);
		m_bDirty = true;
	}
}

void Flux_MaterialAsset::SetEmissiveTexturePath(const std::string& strPath)
{
	std::string strCurrentPath = m_xEmissiveTextureRef.GetPath();
	if (strCurrentPath != strPath)
	{
		if (!strPath.empty())
		{
			m_xEmissiveTextureRef.SetFromPath(strPath);
		}
		else
		{
			m_xEmissiveTextureRef.Clear();
		}
		m_pxEmissiveTexture = LoadTextureFromPath(strPath);
		m_bDirty = true;
	}
}

//------------------------------------------------------------------------------
// Texture Path Getters (get path from GUID via AssetDatabase)
//------------------------------------------------------------------------------

std::string Flux_MaterialAsset::GetDiffuseTexturePath() const
{
	return m_xDiffuseTextureRef.GetPath();
}

std::string Flux_MaterialAsset::GetNormalTexturePath() const
{
	return m_xNormalTextureRef.GetPath();
}

std::string Flux_MaterialAsset::GetRoughnessMetallicTexturePath() const
{
	return m_xRoughnessMetallicTextureRef.GetPath();
}

std::string Flux_MaterialAsset::GetOcclusionTexturePath() const
{
	return m_xOcclusionTextureRef.GetPath();
}

std::string Flux_MaterialAsset::GetEmissiveTexturePath() const
{
	return m_xEmissiveTextureRef.GetPath();
}

//------------------------------------------------------------------------------
// Texture Loading (with caching)
// NOTE: This function is not thread-safe. Materials should be created and have
// their textures set on the main thread during initialization, not from render threads.
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
	// Texture should already be loaded by SetDiffuseTexturePath
	return m_pxDiffuseTexture ? m_pxDiffuseTexture : &Flux_Graphics::s_xWhiteBlankTexture2D;
}

const Flux_Texture* Flux_MaterialAsset::GetNormalTexture()
{
	// Texture should already be loaded by SetNormalTexturePath
	return m_pxNormalTexture ? m_pxNormalTexture : &Flux_Graphics::s_xWhiteBlankTexture2D;
}

const Flux_Texture* Flux_MaterialAsset::GetRoughnessMetallicTexture()
{
	// Texture should already be loaded by SetRoughnessMetallicTexturePath
	return m_pxRoughnessMetallicTexture ? m_pxRoughnessMetallicTexture : &Flux_Graphics::s_xWhiteBlankTexture2D;
}

const Flux_Texture* Flux_MaterialAsset::GetOcclusionTexture()
{
	// Texture should already be loaded by SetOcclusionTexturePath
	return m_pxOcclusionTexture ? m_pxOcclusionTexture : &Flux_Graphics::s_xWhiteBlankTexture2D;
}

const Flux_Texture* Flux_MaterialAsset::GetEmissiveTexture()
{
	// Texture should already be loaded by SetEmissiveTexturePath
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
	// File version - now version 2 with GUID-based texture references
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

	// Texture references (GUID-based) - Version 2+
	m_xDiffuseTextureRef.WriteToDataStream(xStream);
	m_xNormalTextureRef.WriteToDataStream(xStream);
	m_xRoughnessMetallicTextureRef.WriteToDataStream(xStream);
	m_xOcclusionTextureRef.WriteToDataStream(xStream);
	m_xEmissiveTextureRef.WriteToDataStream(xStream);
}

void Flux_MaterialAsset::ReadFromDataStream(Zenith_DataStream& xStream)
{
	// File version
	uint32_t uVersion;
	xStream >> uVersion;

	if (uVersion != MATERIAL_FILE_VERSION)
	{
		Zenith_Log("%s ERROR: Unsupported material version %u (expected %u). Please re-export the material.",
			LOG_TAG, uVersion, MATERIAL_FILE_VERSION);
		return;
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

	// Texture references (GUID-based)
	m_xDiffuseTextureRef.ReadFromDataStream(xStream);
	m_xNormalTextureRef.ReadFromDataStream(xStream);
	m_xRoughnessMetallicTextureRef.ReadFromDataStream(xStream);
	m_xOcclusionTextureRef.ReadFromDataStream(xStream);
	m_xEmissiveTextureRef.ReadFromDataStream(xStream);

	// Load textures from GUID refs
	std::string strDiffusePath = m_xDiffuseTextureRef.GetPath();
	std::string strNormalPath = m_xNormalTextureRef.GetPath();
	std::string strRoughnessMetallicPath = m_xRoughnessMetallicTextureRef.GetPath();
	std::string strOcclusionPath = m_xOcclusionTextureRef.GetPath();
	std::string strEmissivePath = m_xEmissiveTextureRef.GetPath();

	m_pxDiffuseTexture = LoadTextureFromPath(strDiffusePath);
	m_pxNormalTexture = LoadTextureFromPath(strNormalPath);
	m_pxRoughnessMetallicTexture = LoadTextureFromPath(strRoughnessMetallicPath);
	m_pxOcclusionTexture = LoadTextureFromPath(strOcclusionPath);
	m_pxEmissiveTexture = LoadTextureFromPath(strEmissivePath);
}
