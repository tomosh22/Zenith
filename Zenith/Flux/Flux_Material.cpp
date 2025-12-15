#include "Zenith.h"
#include "Flux/Flux_Material.h"
#include "DataStream/Zenith_DataStream.h"
#include "AssetHandling/Zenith_AssetHandler.h"

static constexpr const char* LOG_TAG = "[Material]";

// Material serialization version
static constexpr uint32_t MATERIAL_SERIALIZE_VERSION = 2;

void Flux_Material::WriteToDataStream(Zenith_DataStream& xStream) const
{
	// Version
	xStream << MATERIAL_SERIALIZE_VERSION;
	
	// Base color
	xStream << m_xBaseColor.x;
	xStream << m_xBaseColor.y;
	xStream << m_xBaseColor.z;
	xStream << m_xBaseColor.w;
	
	// Texture paths (NEW in version 2)
	xStream << m_strDiffusePath;
	xStream << m_strNormalPath;
	xStream << m_strRoughnessMetallicPath;
	xStream << m_strOcclusionPath;
	xStream << m_strEmissivePath;
	
	// Debug logging
	Zenith_Log("%s WriteToDataStream: diffuse='%s', normal='%s', roughMetal='%s'", 
		LOG_TAG, m_strDiffusePath.c_str(), m_strNormalPath.c_str(), m_strRoughnessMetallicPath.c_str());
}

void Flux_Material::ReadFromDataStream(Zenith_DataStream& xStream)
{
	uint32_t uVersion;
	xStream >> uVersion;
	
	Zenith_Log("%s ReadFromDataStream: version=%u", LOG_TAG, uVersion);
	
	// Base color
	xStream >> m_xBaseColor.x;
	xStream >> m_xBaseColor.y;
	xStream >> m_xBaseColor.z;
	xStream >> m_xBaseColor.w;
	
	// Texture paths (version 2+)
	if (uVersion >= 2)
	{
		xStream >> m_strDiffusePath;
		xStream >> m_strNormalPath;
		xStream >> m_strRoughnessMetallicPath;
		xStream >> m_strOcclusionPath;
		xStream >> m_strEmissivePath;
		
		Zenith_Log("%s ReadFromDataStream: diffuse='%s', normal='%s', roughMetal='%s'", 
			LOG_TAG, m_strDiffusePath.c_str(), m_strNormalPath.c_str(), m_strRoughnessMetallicPath.c_str());
		
		// Reload textures from paths
		ReloadTexturesFromPaths();
	}
}

void Flux_Material::ReloadTexturesFromPaths()
{
	// Helper lambda to load texture from path
	auto LoadTexture = [](const std::string& strPath) -> Flux_Texture
	{
		if (strPath.empty())
		{
			return Flux_Texture{};
		}
		
		Zenith_AssetHandler::TextureData xTexData = Zenith_AssetHandler::LoadTexture2DFromFile(strPath.c_str());
		if (!xTexData.pData)
		{
			Zenith_Log("%s Failed to load texture: %s", LOG_TAG, strPath.c_str());
			return Flux_Texture{};
		}
		
		Flux_Texture* pTexture = Zenith_AssetHandler::AddTexture(xTexData);
		xTexData.FreeAllocatedData();
		
		if (pTexture)
		{
			pTexture->m_strSourcePath = strPath;
			Zenith_Log("%s Loaded texture: %s", LOG_TAG, strPath.c_str());
			return *pTexture;
		}
		
		return Flux_Texture{};
	};
	
	// Reload each texture from its stored path
	if (!m_strDiffusePath.empty())
	{
		m_xDiffuse = LoadTexture(m_strDiffusePath);
	}
	
	if (!m_strNormalPath.empty())
	{
		m_xNormal = LoadTexture(m_strNormalPath);
	}
	
	if (!m_strRoughnessMetallicPath.empty())
	{
		m_xRoughnessMetallic = LoadTexture(m_strRoughnessMetallicPath);
	}
	
	if (!m_strOcclusionPath.empty())
	{
		m_xOcclusion = LoadTexture(m_strOcclusionPath);
	}
	
	if (!m_strEmissivePath.empty())
	{
		m_xEmissive = LoadTexture(m_strEmissivePath);
	}
}