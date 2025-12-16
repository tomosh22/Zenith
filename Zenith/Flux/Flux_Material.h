#pragma once
#include "Flux/Flux.h"
#include "Flux/Flux_Graphics.h"
#include <string>

// Forward declaration
class Zenith_DataStream;

/**
 * Flux_Material - A material that aggregates textures and material properties
 * 
 * UPDATED DESIGN:
 * - Now stores texture SOURCE PATHS for proper serialization
 * - Textures can be reloaded from paths after scene reload
 * - Use SetDiffuseWithPath() etc. to store both texture and path
 * - Serialization saves/loads texture paths for automatic restoration
 * 
 * For new code, prefer using Flux_MaterialAsset which has full asset management.
 * This class is maintained for backwards compatibility with existing systems.
 */
class Flux_Material
{
public:
	Flux_Material()
		: m_xBaseColor(1.0f, 1.0f, 1.0f, 1.0f)
	{
	}
	Flux_Material(Flux_Texture xDiffuse, Flux_Texture xNormal, Flux_Texture xRoughnessMetallic)
		: m_xDiffuse(xDiffuse)
		, m_xNormal(xNormal)
		, m_xRoughnessMetallic(xRoughnessMetallic)
		, m_xBaseColor(1.0f, 1.0f, 1.0f, 1.0f)
	{
	}

	void Reset()
	{
		m_xDiffuse = Flux_Texture{};
		m_xNormal = Flux_Texture{};
		m_xRoughnessMetallic = Flux_Texture{};
		m_xOcclusion = Flux_Texture{};
		m_xEmissive = Flux_Texture{};
		m_strDiffusePath.clear();
		m_strNormalPath.clear();
		m_strRoughnessMetallicPath.clear();
		m_strOcclusionPath.clear();
		m_strEmissivePath.clear();
	}

	//--------------------------------------------------------------------------
	// Texture Setters (legacy - no path storage)
	//--------------------------------------------------------------------------
	void SetDiffuse(const Flux_Texture& xTexture) { m_xDiffuse = xTexture; }
	void SetNormal(const Flux_Texture& xTexture) { m_xNormal = xTexture; }
	void SetRoughnessMetallic(const Flux_Texture& xTexture) { m_xRoughnessMetallic = xTexture; }
	void SetOcclusion(const Flux_Texture& xTexture) { m_xOcclusion = xTexture; }
	void SetEmissive(const Flux_Texture& xTexture) { m_xEmissive = xTexture; }
	
	//--------------------------------------------------------------------------
	// Texture Setters with Path (NEW - stores path for serialization)
	//--------------------------------------------------------------------------
	void SetDiffuseWithPath(const Flux_Texture& xTexture, const std::string& strPath)
	{
		m_xDiffuse = xTexture;
		m_strDiffusePath = strPath;
	}
	void SetNormalWithPath(const Flux_Texture& xTexture, const std::string& strPath)
	{
		m_xNormal = xTexture;
		m_strNormalPath = strPath;
	}
	void SetRoughnessMetallicWithPath(const Flux_Texture& xTexture, const std::string& strPath)
	{
		m_xRoughnessMetallic = xTexture;
		m_strRoughnessMetallicPath = strPath;
	}
	void SetOcclusionWithPath(const Flux_Texture& xTexture, const std::string& strPath)
	{
		m_xOcclusion = xTexture;
		m_strOcclusionPath = strPath;
	}
	void SetEmissiveWithPath(const Flux_Texture& xTexture, const std::string& strPath)
	{
		m_xEmissive = xTexture;
		m_strEmissivePath = strPath;
	}

	void SetBaseColor(const Zenith_Maths::Vector4& xColor) { m_xBaseColor = xColor; }

	//--------------------------------------------------------------------------
	// Texture Getters
	//--------------------------------------------------------------------------
	const Flux_Texture* GetDiffuse() const { return m_xDiffuse.m_xVRAMHandle.IsValid() ? &m_xDiffuse : GetBlankTexture(); }
	const Flux_Texture* GetNormal() const { return m_xNormal.m_xVRAMHandle.IsValid() ? &m_xNormal : GetBlankTexture(); }
	const Flux_Texture* GetRoughnessMetallic() const { return m_xRoughnessMetallic.m_xVRAMHandle.IsValid() ? &m_xRoughnessMetallic : GetBlankTexture(); }
	const Flux_Texture* GetOcclusion() const { return m_xOcclusion.m_xVRAMHandle.IsValid() ? &m_xOcclusion : GetBlankTexture(); }
	const Flux_Texture* GetEmissive() const { return m_xEmissive.m_xVRAMHandle.IsValid() ? &m_xEmissive : GetBlankTexture(); }
	const Zenith_Maths::Vector4& GetBaseColor() const { return m_xBaseColor; }
	
	//--------------------------------------------------------------------------
	// Path Getters (for serialization)
	//--------------------------------------------------------------------------
	const std::string& GetDiffusePath() const { return m_strDiffusePath; }
	const std::string& GetNormalPath() const { return m_strNormalPath; }
	const std::string& GetRoughnessMetallicPath() const { return m_strRoughnessMetallicPath; }
	const std::string& GetOcclusionPath() const { return m_strOcclusionPath; }
	const std::string& GetEmissivePath() const { return m_strEmissivePath; }
	
	//--------------------------------------------------------------------------
	// Check if textures have paths for serialization
	//--------------------------------------------------------------------------
	bool HasDiffusePath() const { return !m_strDiffusePath.empty(); }
	bool HasNormalPath() const { return !m_strNormalPath.empty(); }
	bool HasRoughnessMetallicPath() const { return !m_strRoughnessMetallicPath.empty(); }
	bool HasOcclusionPath() const { return !m_strOcclusionPath.empty(); }
	bool HasEmissivePath() const { return !m_strEmissivePath.empty(); }
	
	//--------------------------------------------------------------------------
	// Serialization
	//--------------------------------------------------------------------------
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);
	
	/**
	 * Reload textures from stored paths
	 * Call this after scene reload to restore textures
	 */
	void ReloadTexturesFromPaths();
	
	/**
	 * Delete textures that were loaded via ReloadTexturesFromPaths
	 * Call this before destroying the material to free texture slots
	 * Only deletes textures that have source paths stored
	 */
	void DeleteLoadedTextures();

private:
	static const Flux_Texture* GetBlankTexture()
	{
		return &Flux_Graphics::s_xWhiteBlankTexture2D;
	}

	// Texture data (GPU handles)
	Flux_Texture m_xDiffuse;
	Flux_Texture m_xNormal;
	Flux_Texture m_xRoughnessMetallic;
	Flux_Texture m_xOcclusion;
	Flux_Texture m_xEmissive;
	
	// Material properties
	Zenith_Maths::Vector4 m_xBaseColor;
	
	// Texture source paths (for serialization and reload)
	std::string m_strDiffusePath;
	std::string m_strNormalPath;
	std::string m_strRoughnessMetallicPath;
	std::string m_strOcclusionPath;
	std::string m_strEmissivePath;
};