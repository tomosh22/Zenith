#pragma once

#include "AssetHandling/Zenith_Asset.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "Maths/Zenith_Maths.h"
#include <string>

// Forward declarations
class Zenith_DataStream;
class Zenith_TextureAsset;

/**
 * Zenith_MaterialAsset - Material asset containing texture references and properties
 *
 * This is the new material system:
 * - Materials are assets managed by Zenith_AssetRegistry
 * - Textures are referenced by path using TextureHandle
 * - Reference counted for automatic cleanup
 *
 * Usage:
 *   // Load existing material
 *   auto* pMat = Zenith_AssetRegistry::Get().Get<Zenith_MaterialAsset>("Assets/mat.zmat");
 *
 *   // Create new material
 *   auto* pMat = Zenith_AssetRegistry::Get().Create<Zenith_MaterialAsset>();
 *   pMat->SetName("MyMaterial");
 *   pMat->SetDiffuseTexturePath("Assets/Textures/diffuse.ztex");
 *   pMat->SaveToFile("Assets/Materials/MyMaterial.zmat");
 *
 *   // Get texture for rendering
 *   const Zenith_TextureAsset* pDiffuse = pMat->GetDiffuseTexture();
 */
class Zenith_MaterialAsset : public Zenith_Asset
{
public:
	Zenith_MaterialAsset();
	~Zenith_MaterialAsset();

	// Non-copyable
	Zenith_MaterialAsset(const Zenith_MaterialAsset&) = delete;
	Zenith_MaterialAsset& operator=(const Zenith_MaterialAsset&) = delete;

	//--------------------------------------------------------------------------
	// Loading / Saving
	//--------------------------------------------------------------------------

	/**
	 * Save material data to file
	 * @param strPath Path to save to (becomes the asset path)
	 * @return true on success
	 */
	bool SaveToFile(const std::string& strPath);

	/**
	 * Reload material data from disk (uses stored path)
	 * @return true on success
	 */
	bool Reload();

	/**
	 * Write material data to stream
	 */
	void WriteToDataStream(Zenith_DataStream& xStream) const;

	/**
	 * Read material data from stream
	 */
	void ReadFromDataStream(Zenith_DataStream& xStream);

	//--------------------------------------------------------------------------
	// Material Properties
	//--------------------------------------------------------------------------

	// Name
	const std::string& GetName() const { return m_strName; }
	void SetName(const std::string& strName) { m_strName = strName; m_bDirty = true; }

	// Base color (multiplied with diffuse texture)
	const Zenith_Maths::Vector4& GetBaseColor() const { return m_xBaseColor; }
	void SetBaseColor(const Zenith_Maths::Vector4& xColor) { m_xBaseColor = xColor; m_bDirty = true; }

	// Metallic/Roughness factors
	float GetMetallic() const { return m_fMetallic; }
	void SetMetallic(float fMetallic) { m_fMetallic = fMetallic; m_bDirty = true; }

	float GetRoughness() const { return m_fRoughness; }
	void SetRoughness(float fRoughness) { m_fRoughness = fRoughness; m_bDirty = true; }

	// Emissive
	const Zenith_Maths::Vector3& GetEmissiveColor() const { return m_xEmissiveColor; }
	void SetEmissiveColor(const Zenith_Maths::Vector3& xColor) { m_xEmissiveColor = xColor; m_bDirty = true; }

	float GetEmissiveIntensity() const { return m_fEmissiveIntensity; }
	void SetEmissiveIntensity(float fIntensity) { m_fEmissiveIntensity = fIntensity; m_bDirty = true; }

	// Alpha/Transparency
	bool IsTransparent() const { return m_bTransparent; }
	void SetTransparent(bool bTransparent) { m_bTransparent = bTransparent; m_bDirty = true; }

	float GetAlphaCutoff() const { return m_fAlphaCutoff; }
	void SetAlphaCutoff(float fCutoff) { m_fAlphaCutoff = fCutoff; m_bDirty = true; }

	// UV Controls
	const Zenith_Maths::Vector2& GetUVTiling() const { return m_xUVTiling; }
	void SetUVTiling(const Zenith_Maths::Vector2& xTiling) { m_xUVTiling = xTiling; m_bDirty = true; }

	const Zenith_Maths::Vector2& GetUVOffset() const { return m_xUVOffset; }
	void SetUVOffset(const Zenith_Maths::Vector2& xOffset) { m_xUVOffset = xOffset; m_bDirty = true; }

	// Occlusion Strength
	float GetOcclusionStrength() const { return m_fOcclusionStrength; }
	void SetOcclusionStrength(float fStrength) { m_fOcclusionStrength = fStrength; m_bDirty = true; }

	// Render Flags
	bool IsTwoSided() const { return m_bTwoSided; }
	void SetTwoSided(bool bTwoSided) { m_bTwoSided = bTwoSided; m_bDirty = true; }

	bool IsUnlit() const { return m_bUnlit; }
	void SetUnlit(bool bUnlit) { m_bUnlit = bUnlit; m_bDirty = true; }

	// Dirty flag
	bool IsDirty() const { return m_bDirty; }

	//--------------------------------------------------------------------------
	// Texture Path Setters (serialized)
	//--------------------------------------------------------------------------

	const std::string& GetDiffuseTexturePath() const { return m_xDiffuseTexture.GetPath(); }
	void SetDiffuseTexturePath(const std::string& strPath);

	const std::string& GetNormalTexturePath() const { return m_xNormalTexture.GetPath(); }
	void SetNormalTexturePath(const std::string& strPath);

	const std::string& GetRoughnessMetallicTexturePath() const { return m_xRoughnessMetallicTexture.GetPath(); }
	void SetRoughnessMetallicTexturePath(const std::string& strPath);

	const std::string& GetOcclusionTexturePath() const { return m_xOcclusionTexture.GetPath(); }
	void SetOcclusionTexturePath(const std::string& strPath);

	const std::string& GetEmissiveTexturePath() const { return m_xEmissiveTexture.GetPath(); }
	void SetEmissiveTexturePath(const std::string& strPath);

	//--------------------------------------------------------------------------
	// Direct Texture Setters (for procedurally generated textures)
	// Sets both the handle and the direct pointer
	//--------------------------------------------------------------------------

	void SetDiffuseTextureDirectly(Zenith_TextureAsset* pTexture);
	void SetNormalTextureDirectly(Zenith_TextureAsset* pTexture);
	void SetRoughnessMetallicTextureDirectly(Zenith_TextureAsset* pTexture);
	void SetOcclusionTextureDirectly(Zenith_TextureAsset* pTexture);
	void SetEmissiveTextureDirectly(Zenith_TextureAsset* pTexture);

	//--------------------------------------------------------------------------
	// Texture Accessors (returns loaded texture, or blank if not set)
	//--------------------------------------------------------------------------

	Zenith_TextureAsset* GetDiffuseTexture();
	Zenith_TextureAsset* GetNormalTexture();
	Zenith_TextureAsset* GetRoughnessMetallicTexture();
	Zenith_TextureAsset* GetOcclusionTexture();
	Zenith_TextureAsset* GetEmissiveTexture();

	//--------------------------------------------------------------------------
	// Default Textures (static, for fallback)
	//--------------------------------------------------------------------------

	static Zenith_TextureAsset* GetDefaultWhiteTexture();
	static Zenith_TextureAsset* GetDefaultNormalTexture();
	static void InitializeDefaults();
	static void ShutdownDefaults();

private:
	friend class Zenith_AssetRegistry;
	friend Zenith_Asset* LoadMaterialAsset(const std::string&);

	/**
	 * Load material data from file (private - use Zenith_AssetRegistry::Get)
	 * @param strPath Path to .zmat file
	 * @return true on success
	 */
	bool LoadFromFile(const std::string& strPath);

	// Material identity
	std::string m_strName;

	// Material properties
	Zenith_Maths::Vector4 m_xBaseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	float m_fMetallic = 0.0f;
	float m_fRoughness = 0.5f;
	Zenith_Maths::Vector3 m_xEmissiveColor = { 0.0f, 0.0f, 0.0f };
	float m_fEmissiveIntensity = 0.0f;
	bool m_bTransparent = false;
	float m_fAlphaCutoff = 0.5f;

	// UV Controls
	Zenith_Maths::Vector2 m_xUVTiling = { 1.0f, 1.0f };
	Zenith_Maths::Vector2 m_xUVOffset = { 0.0f, 0.0f };

	// Occlusion Strength
	float m_fOcclusionStrength = 1.0f;

	// Render Flags
	bool m_bTwoSided = false;
	bool m_bUnlit = false;

	// Texture handles (serialized by path)
	TextureHandle m_xDiffuseTexture;
	TextureHandle m_xNormalTexture;
	TextureHandle m_xRoughnessMetallicTexture;
	TextureHandle m_xOcclusionTexture;
	TextureHandle m_xEmissiveTexture;

	// Direct texture pointers (for procedural textures that bypass handles)
	Zenith_TextureAsset* m_pxDirectDiffuse = nullptr;
	Zenith_TextureAsset* m_pxDirectNormal = nullptr;
	Zenith_TextureAsset* m_pxDirectRoughnessMetallic = nullptr;
	Zenith_TextureAsset* m_pxDirectOcclusion = nullptr;
	Zenith_TextureAsset* m_pxDirectEmissive = nullptr;

	// Dirty flag
	bool m_bDirty = false;

	// Static default textures
	static Zenith_TextureAsset* s_pxDefaultWhite;
	static Zenith_TextureAsset* s_pxDefaultNormal;
};

// Material file version
#define ZENITH_MATERIAL_FILE_VERSION 4

//--------------------------------------------------------------------------
// Register loader with asset registry
//--------------------------------------------------------------------------

void Zenith_MaterialAsset_RegisterLoader();
