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
 *   auto* pMat = Zenith_AssetRegistry::Get<Zenith_MaterialAsset>("Assets/mat.zmat");
 *
 *   // Create new material
 *   auto* pMat = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
 *   pMat->SetName("MyMaterial");
 *   pMat->SetDiffuseTexture(TextureHandle("Assets/Textures/diffuse.ztxtr"));
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
	// Texture Setters (handle-based — covers both file-backed and procedural)
	//--------------------------------------------------------------------------

	void SetDiffuseTexture(TextureHandle xHandle);
	void SetNormalTexture(TextureHandle xHandle);
	void SetRoughnessMetallicTexture(TextureHandle xHandle);
	void SetOcclusionTexture(TextureHandle xHandle);
	void SetEmissiveTexture(TextureHandle xHandle);

	//--------------------------------------------------------------------------
	// Texture Handle Accessors (for material-copy and procedural preservation)
	//--------------------------------------------------------------------------

	const TextureHandle& GetDiffuseTextureHandle() const           { return m_xDiffuseTexture; }
	const TextureHandle& GetNormalTextureHandle() const            { return m_xNormalTexture; }
	const TextureHandle& GetRoughnessMetallicTextureHandle() const { return m_xRoughnessMetallicTexture; }
	const TextureHandle& GetOcclusionTextureHandle() const         { return m_xOcclusionTexture; }
	const TextureHandle& GetEmissiveTextureHandle() const          { return m_xEmissiveTexture; }

	//--------------------------------------------------------------------------
	// Texture Path Accessors (read-only convenience for UI / serialization)
	//--------------------------------------------------------------------------

	const std::string& GetDiffuseTexturePath() const           { return m_xDiffuseTexture.GetPath(); }
	const std::string& GetNormalTexturePath() const            { return m_xNormalTexture.GetPath(); }
	const std::string& GetRoughnessMetallicTexturePath() const { return m_xRoughnessMetallicTexture.GetPath(); }
	const std::string& GetOcclusionTexturePath() const         { return m_xOcclusionTexture.GetPath(); }
	const std::string& GetEmissiveTexturePath() const          { return m_xEmissiveTexture.GetPath(); }

	//--------------------------------------------------------------------------
	// Texture Accessors (returns loaded texture, or blank if not set)
	//--------------------------------------------------------------------------

	Zenith_TextureAsset* GetDiffuseTexture();
	Zenith_TextureAsset* GetNormalTexture();
	Zenith_TextureAsset* GetRoughnessMetallicTexture();
	Zenith_TextureAsset* GetOcclusionTexture();
	Zenith_TextureAsset* GetEmissiveTexture();

	//--------------------------------------------------------------------------
	// Default Textures (static, for fallback). Pinned via TextureHandle so
	// Zenith_AssetRegistry::UnloadUnused never frees them while the engine runs.
	//--------------------------------------------------------------------------

	static Zenith_TextureAsset* GetDefaultWhiteTexture();
	static Zenith_TextureAsset* GetDefaultNormalTexture();
	static void InitializeDefaults();
	static void ShutdownDefaults();

	// Drop the default texture handles before Zenith_AssetRegistry::Shutdown.
	// Called from Flux::ReleaseAssetReferences. Distinct from ShutdownDefaults
	// (which has been preserved as a no-op for callers that still invoke it).
	static void ReleaseDefaults();

private:
	friend class Zenith_AssetRegistry;
	friend Zenith_Result<Zenith_Asset*> LoadMaterialAsset(const std::string&);

	/**
	 * Load material data from file (private - use Zenith_AssetRegistry::Get)
	 * @param strPath Path to .zmat file
	 * @return SUCCESS, or an error code on failure
	 */
	Zenith_Status LoadFromFile(const std::string& strPath);

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

	// Texture handles — store either a path (file-backed, lazy-loaded via registry)
	// or a procedural pointer set via TextureHandle::Set(). Resolve() handles both.
	TextureHandle m_xDiffuseTexture;
	TextureHandle m_xNormalTexture;
	TextureHandle m_xRoughnessMetallicTexture;
	TextureHandle m_xOcclusionTexture;
	TextureHandle m_xEmissiveTexture;

	// Dirty flag
	bool m_bDirty = false;

	// Static default textures (pinned via handle so they survive UnloadUnused).
	static TextureHandle s_xDefaultWhite;
	static TextureHandle s_xDefaultNormal;
};

// Material file version
#define ZENITH_MATERIAL_FILE_VERSION 4

//--------------------------------------------------------------------------
// Register loader with asset registry
//--------------------------------------------------------------------------

void Zenith_MaterialAsset_RegisterLoader();
