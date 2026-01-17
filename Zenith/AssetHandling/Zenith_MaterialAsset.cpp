#include "Zenith.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "DataStream/Zenith_DataStream.h"
#include <filesystem>

// Static default textures
Zenith_TextureAsset* Zenith_MaterialAsset::s_pxDefaultWhite = nullptr;
Zenith_TextureAsset* Zenith_MaterialAsset::s_pxDefaultNormal = nullptr;

//--------------------------------------------------------------------------
// Construction / Destruction
//--------------------------------------------------------------------------

Zenith_MaterialAsset::Zenith_MaterialAsset()
	: m_strName("New Material")
{
}

Zenith_MaterialAsset::~Zenith_MaterialAsset()
{
	// Handles will clean up their refs automatically
}

//--------------------------------------------------------------------------
// Loading / Saving
//--------------------------------------------------------------------------

bool Zenith_MaterialAsset::LoadFromFile(const std::string& strPath)
{
	if (!std::filesystem::exists(strPath))
	{
		Zenith_Error(LOG_CATEGORY_ASSET, "Material file not found: %s", strPath.c_str());
		return false;
	}

	Zenith_DataStream xStream;
	xStream.ReadFromFile(strPath.c_str());

	ReadFromDataStream(xStream);

	m_strPath = strPath;
	m_bDirty = false;

	Zenith_Log(LOG_CATEGORY_ASSET, "Loaded material: %s (name: %s)", strPath.c_str(), m_strName.c_str());
	return true;
}

bool Zenith_MaterialAsset::SaveToFile(const std::string& strPath)
{
	Zenith_DataStream xStream;
	WriteToDataStream(xStream);

	xStream.WriteToFile(strPath.c_str());

	m_strPath = strPath;
	m_bDirty = false;

	Zenith_Log(LOG_CATEGORY_ASSET, "Saved material to: %s", strPath.c_str());
	return true;
}

bool Zenith_MaterialAsset::Reload()
{
	if (m_strPath.empty())
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "Cannot reload material with empty path");
		return false;
	}

	return LoadFromFile(m_strPath);
}

void Zenith_MaterialAsset::WriteToDataStream(Zenith_DataStream& xStream) const
{
	// File version
	uint32_t uVersion = ZENITH_MATERIAL_FILE_VERSION;
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

	// UV Controls
	xStream << m_xUVTiling.x;
	xStream << m_xUVTiling.y;
	xStream << m_xUVOffset.x;
	xStream << m_xUVOffset.y;

	// Occlusion strength
	xStream << m_fOcclusionStrength;

	// Render flags
	xStream << m_bTwoSided;
	xStream << m_bUnlit;

	// Texture paths (version 4+: path-based, no GUIDs)
	xStream << m_xDiffuseTexture.GetPath();
	xStream << m_xNormalTexture.GetPath();
	xStream << m_xRoughnessMetallicTexture.GetPath();
	xStream << m_xOcclusionTexture.GetPath();
	xStream << m_xEmissiveTexture.GetPath();
}

void Zenith_MaterialAsset::ReadFromDataStream(Zenith_DataStream& xStream)
{
	// File version
	uint32_t uVersion;
	xStream >> uVersion;

	if (uVersion > ZENITH_MATERIAL_FILE_VERSION)
	{
		Zenith_Error(LOG_CATEGORY_ASSET, "Unsupported material version %u (max: %u)",
			uVersion, ZENITH_MATERIAL_FILE_VERSION);
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

	// UV Controls (version 3+)
	if (uVersion >= 3)
	{
		xStream >> m_xUVTiling.x;
		xStream >> m_xUVTiling.y;
		xStream >> m_xUVOffset.x;
		xStream >> m_xUVOffset.y;

		xStream >> m_fOcclusionStrength;

		xStream >> m_bTwoSided;
		xStream >> m_bUnlit;
	}
	else
	{
		// Defaults for older versions
		m_xUVTiling = { 1.0f, 1.0f };
		m_xUVOffset = { 0.0f, 0.0f };
		m_fOcclusionStrength = 1.0f;
		m_bTwoSided = false;
		m_bUnlit = false;
	}

	// Texture references
	if (uVersion >= 4)
	{
		// Version 4+: Direct paths
		std::string strPath;

		xStream >> strPath;
		m_xDiffuseTexture.SetPath(strPath);

		xStream >> strPath;
		m_xNormalTexture.SetPath(strPath);

		xStream >> strPath;
		m_xRoughnessMetallicTexture.SetPath(strPath);

		xStream >> strPath;
		m_xOcclusionTexture.SetPath(strPath);

		xStream >> strPath;
		m_xEmissiveTexture.SetPath(strPath);
	}
	else if (uVersion >= 2)
	{
		// Version 2-3: GUID-based (old format) - no longer supported
		// Old materials need to be re-exported
		Zenith_Error(LOG_CATEGORY_ASSET, "Material %s uses old GUID format (v%u). Please re-export.",
			m_strName.c_str(), uVersion);
	}
}

//--------------------------------------------------------------------------
// Texture Path Setters
//--------------------------------------------------------------------------

void Zenith_MaterialAsset::SetDiffuseTexturePath(const std::string& strPath)
{
	m_xDiffuseTexture.SetPath(strPath);
	m_pxDirectDiffuse = nullptr;
	m_bDirty = true;
}

void Zenith_MaterialAsset::SetNormalTexturePath(const std::string& strPath)
{
	m_xNormalTexture.SetPath(strPath);
	m_pxDirectNormal = nullptr;
	m_bDirty = true;
}

void Zenith_MaterialAsset::SetRoughnessMetallicTexturePath(const std::string& strPath)
{
	m_xRoughnessMetallicTexture.SetPath(strPath);
	m_pxDirectRoughnessMetallic = nullptr;
	m_bDirty = true;
}

void Zenith_MaterialAsset::SetOcclusionTexturePath(const std::string& strPath)
{
	m_xOcclusionTexture.SetPath(strPath);
	m_pxDirectOcclusion = nullptr;
	m_bDirty = true;
}

void Zenith_MaterialAsset::SetEmissiveTexturePath(const std::string& strPath)
{
	m_xEmissiveTexture.SetPath(strPath);
	m_pxDirectEmissive = nullptr;
	m_bDirty = true;
}

//--------------------------------------------------------------------------
// Direct Texture Setters
//--------------------------------------------------------------------------

void Zenith_MaterialAsset::SetDiffuseTextureDirectly(Zenith_TextureAsset* pTexture)
{
	m_pxDirectDiffuse = pTexture;
	m_xDiffuseTexture.Clear();
	m_bDirty = true;
}

void Zenith_MaterialAsset::SetNormalTextureDirectly(Zenith_TextureAsset* pTexture)
{
	m_pxDirectNormal = pTexture;
	m_xNormalTexture.Clear();
	m_bDirty = true;
}

void Zenith_MaterialAsset::SetRoughnessMetallicTextureDirectly(Zenith_TextureAsset* pTexture)
{
	m_pxDirectRoughnessMetallic = pTexture;
	m_xRoughnessMetallicTexture.Clear();
	m_bDirty = true;
}

void Zenith_MaterialAsset::SetOcclusionTextureDirectly(Zenith_TextureAsset* pTexture)
{
	m_pxDirectOcclusion = pTexture;
	m_xOcclusionTexture.Clear();
	m_bDirty = true;
}

void Zenith_MaterialAsset::SetEmissiveTextureDirectly(Zenith_TextureAsset* pTexture)
{
	m_pxDirectEmissive = pTexture;
	m_xEmissiveTexture.Clear();
	m_bDirty = true;
}

//--------------------------------------------------------------------------
// Texture Accessors
//--------------------------------------------------------------------------

Zenith_TextureAsset* Zenith_MaterialAsset::GetDiffuseTexture()
{
	if (m_pxDirectDiffuse)
	{
		return m_pxDirectDiffuse;
	}
	Zenith_TextureAsset* pTex = m_xDiffuseTexture.Get();
	return pTex ? pTex : GetDefaultWhiteTexture();
}

Zenith_TextureAsset* Zenith_MaterialAsset::GetNormalTexture()
{
	if (m_pxDirectNormal)
	{
		return m_pxDirectNormal;
	}
	Zenith_TextureAsset* pTex = m_xNormalTexture.Get();
	return pTex ? pTex : GetDefaultNormalTexture();
}

Zenith_TextureAsset* Zenith_MaterialAsset::GetRoughnessMetallicTexture()
{
	if (m_pxDirectRoughnessMetallic)
	{
		return m_pxDirectRoughnessMetallic;
	}
	Zenith_TextureAsset* pTex = m_xRoughnessMetallicTexture.Get();
	return pTex ? pTex : GetDefaultWhiteTexture();
}

Zenith_TextureAsset* Zenith_MaterialAsset::GetOcclusionTexture()
{
	if (m_pxDirectOcclusion)
	{
		return m_pxDirectOcclusion;
	}
	Zenith_TextureAsset* pTex = m_xOcclusionTexture.Get();
	return pTex ? pTex : GetDefaultWhiteTexture();
}

Zenith_TextureAsset* Zenith_MaterialAsset::GetEmissiveTexture()
{
	if (m_pxDirectEmissive)
	{
		return m_pxDirectEmissive;
	}
	Zenith_TextureAsset* pTex = m_xEmissiveTexture.Get();
	return pTex ? pTex : GetDefaultWhiteTexture();
}

//--------------------------------------------------------------------------
// Default Textures
//--------------------------------------------------------------------------

Zenith_TextureAsset* Zenith_MaterialAsset::GetDefaultWhiteTexture()
{
	return s_pxDefaultWhite;
}

Zenith_TextureAsset* Zenith_MaterialAsset::GetDefaultNormalTexture()
{
	return s_pxDefaultNormal;
}

void Zenith_MaterialAsset::InitializeDefaults()
{
	// Create default white texture (1x1 white pixel)
	s_pxDefaultWhite = Zenith_AssetRegistry::Get().Create<Zenith_TextureAsset>();
	{
		uint32_t uWhite = 0xFFFFFFFF;
		Flux_SurfaceInfo xInfo;
		xInfo.m_uWidth = 1;
		xInfo.m_uHeight = 1;
		xInfo.m_uNumMips = 1;
		xInfo.m_eFormat = TEXTURE_FORMAT_RGBA8_UNORM;
		xInfo.m_eTextureType = TEXTURE_TYPE_2D;
		s_pxDefaultWhite->CreateFromData(&uWhite, xInfo, false);
	}

	// Create default normal texture (1x1 flat normal: 0.5, 0.5, 1.0)
	s_pxDefaultNormal = Zenith_AssetRegistry::Get().Create<Zenith_TextureAsset>();
	{
		uint32_t uNormal = 0xFFFF8080; // RGBA: 0.5, 0.5, 1.0, 1.0 in 8-bit (128, 128, 255, 255)
		Flux_SurfaceInfo xInfo;
		xInfo.m_uWidth = 1;
		xInfo.m_uHeight = 1;
		xInfo.m_uNumMips = 1;
		xInfo.m_eFormat = TEXTURE_FORMAT_RGBA8_UNORM;
		xInfo.m_eTextureType = TEXTURE_TYPE_2D;
		s_pxDefaultNormal->CreateFromData(&uNormal, xInfo, false);
	}

	Zenith_Log(LOG_CATEGORY_ASSET, "Material default textures initialized");
}

void Zenith_MaterialAsset::ShutdownDefaults()
{
	// Clear pointers - registry manages asset lifetime
	s_pxDefaultWhite = nullptr;
	s_pxDefaultNormal = nullptr;

	Zenith_Log(LOG_CATEGORY_ASSET, "Material default textures shut down");
}

