#pragma once
#include "Flux/Flux.h"
#include "Flux/Flux_Graphics.h"
#include "Maths/Zenith_Maths.h"
#include "AssetHandling/Zenith_AssetRef.h"
#include <string>
#include <unordered_map>

// Forward declarations
class Zenith_DataStream;

/**
 * Flux_MaterialAsset - A serializable material asset that references textures by path
 * 
 * This is the new material system designed to work like Unity/Unreal materials:
 * - Materials are assets that can be saved/loaded from disk (ZENITH_MATERIAL_EXT files)
 * - Textures are referenced by file path, not by raw GPU handles
 * - Materials are cached by path for reuse across the application
 * - Materials properly serialize and deserialize with full texture restoration
 * 
 * Usage:
 *   // Create a new material
 *   Flux_MaterialAsset* pMat = Flux_MaterialAsset::Create("MyMaterial");
 *   pMat->SetDiffuseTexturePath("Assets/Textures/diffuse" ZENITH_TEXTURE_EXT);
 *   pMat->SetBaseColor(Vector4(1,1,1,1));
 *   pMat->SaveToFile("Assets/Materials/MyMaterial" ZENITH_MATERIAL_EXT);
 * 
 *   // Load existing material
 *   Flux_MaterialAsset* pMat = Flux_MaterialAsset::LoadFromFile("Assets/Materials/MyMaterial" ZENITH_MATERIAL_EXT);
 * 
 *   // Get material for rendering (loads textures on demand)
 *   const Flux_Texture* pDiffuse = pMat->GetDiffuseTexture();
 */
class Flux_MaterialAsset
{
public:
	//--------------------------------------------------------------------------
	// Asset Management (Static Registry)
	//--------------------------------------------------------------------------
	
	/**
	 * Create a new material asset with optional name
	 * @param strName Display name for the material (optional)
	 * @return New material asset instance (owned by registry)
	 */
	static Flux_MaterialAsset* Create(const std::string& strName = "New Material");
	
	/**
	 * Load a material from file, or return cached version if already loaded
	 * @param strPath Path to ZENITH_MATERIAL_EXT file
	 * @return Material asset, or nullptr on failure
	 */
	static Flux_MaterialAsset* LoadFromFile(const std::string& strPath);
	
	/**
	 * Get a material by its asset path (must have been loaded or saved first)
	 * @param strPath Asset path
	 * @return Material asset, or nullptr if not found
	 */
	static Flux_MaterialAsset* GetByPath(const std::string& strPath);
	
	/**
	 * Unload a specific material and its textures from cache
	 * @param strPath Asset path of material to unload
	 */
	static void Unload(const std::string& strPath);
	
	/**
	 * Unload all materials and clear the cache
	 * Called during shutdown or scene unload
	 */
	static void UnloadAll();
	
	/**
	 * Reload all materials (reloads textures from disk)
	 * Useful after scene reload to restore textures
	 */
	static void ReloadAll();
	
	/**
	 * Get all loaded material paths for editor UI (file-cached materials only)
	 */
	static void GetAllLoadedMaterialPaths(std::vector<std::string>& outPaths);

	/**
	 * Get all materials (both file-cached and runtime-created) for editor UI
	 * Returns pointers to all materials that currently exist
	 */
	static void GetAllMaterials(std::vector<Flux_MaterialAsset*>& outMaterials);
	
	/**
	 * Initialize the material system (call once at startup)
	 */
	static void Initialize();
	
	/**
	 * Shutdown the material system (call once at shutdown)
	 */
	static void Shutdown();
	
	//--------------------------------------------------------------------------
	// Instance Methods
	//--------------------------------------------------------------------------
	
	Flux_MaterialAsset();
	~Flux_MaterialAsset();
	
	// Prevent copying (materials are reference types)
	Flux_MaterialAsset(const Flux_MaterialAsset&) = delete;
	Flux_MaterialAsset& operator=(const Flux_MaterialAsset&) = delete;
	
	//--------------------------------------------------------------------------
	// Material Properties
	//--------------------------------------------------------------------------
	
	// Name and path
	const std::string& GetName() const { return m_strName; }
	void SetName(const std::string& strName) { m_strName = strName; m_bDirty = true; }
	
	const std::string& GetAssetPath() const { return m_strAssetPath; }
	
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
	
	//--------------------------------------------------------------------------
	// Texture References (GUID-based)
	// Primary way to reference textures - survives asset moves/renames
	//--------------------------------------------------------------------------

	const TextureRef& GetDiffuseTextureRef() const { return m_xDiffuseTextureRef; }
	void SetDiffuseTextureRef(const TextureRef& xRef);

	const TextureRef& GetNormalTextureRef() const { return m_xNormalTextureRef; }
	void SetNormalTextureRef(const TextureRef& xRef);

	const TextureRef& GetRoughnessMetallicTextureRef() const { return m_xRoughnessMetallicTextureRef; }
	void SetRoughnessMetallicTextureRef(const TextureRef& xRef);

	const TextureRef& GetOcclusionTextureRef() const { return m_xOcclusionTextureRef; }
	void SetOcclusionTextureRef(const TextureRef& xRef);

	const TextureRef& GetEmissiveTextureRef() const { return m_xEmissiveTextureRef; }
	void SetEmissiveTextureRef(const TextureRef& xRef);

	//--------------------------------------------------------------------------
	// Texture Paths (for compatibility and editor UI)
	// NOTE: Setters load textures immediately to avoid threading issues during rendering
	// NOTE: These now convert paths to GUIDs internally
	//--------------------------------------------------------------------------

	std::string GetDiffuseTexturePath() const;
	void SetDiffuseTexturePath(const std::string& strPath);

	/**
	 * Directly set a diffuse texture (for procedurally generated textures)
	 * The texture must remain valid for the lifetime of this material
	 * @param pTexture Pointer to texture (not owned by material)
	 */
	void SetDiffuseTexture(Flux_Texture* pTexture) { m_pxDiffuseTexture = pTexture; m_bDirty = true; }

	std::string GetNormalTexturePath() const;
	void SetNormalTexturePath(const std::string& strPath);

	std::string GetRoughnessMetallicTexturePath() const;
	void SetRoughnessMetallicTexturePath(const std::string& strPath);

	std::string GetOcclusionTexturePath() const;
	void SetOcclusionTexturePath(const std::string& strPath);

	std::string GetEmissiveTexturePath() const;
	void SetEmissiveTexturePath(const std::string& strPath);
	
	//--------------------------------------------------------------------------
	// Texture Accessors (returns loaded texture, or blank if not set)
	// NOTE: Textures are loaded immediately when paths are set, not lazily
	//--------------------------------------------------------------------------
	
	const Flux_Texture* GetDiffuseTexture();
	const Flux_Texture* GetNormalTexture();
	const Flux_Texture* GetRoughnessMetallicTexture();
	const Flux_Texture* GetOcclusionTexture();
	const Flux_Texture* GetEmissiveTexture();
	
	//--------------------------------------------------------------------------
	// Serialization
	//--------------------------------------------------------------------------
	
	/**
	 * Save material to file
	 * @param strPath Path to save to (becomes the asset path)
	 * @return true on success
	 */
	bool SaveToFile(const std::string& strPath);
	
	/**
	 * Reload material from its asset path
	 * Reloads texture references from disk
	 */
	void Reload();
	
	/**
	 * Check if material has unsaved changes
	 */
	bool IsDirty() const { return m_bDirty; }
	
	/**
	 * Write material data to a data stream
	 */
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	
	/**
	 * Read material data from a data stream
	 */
	void ReadFromDataStream(Zenith_DataStream& xStream);
	
private:
	// Load a texture from path, with caching
	Flux_Texture* LoadTextureFromPath(const std::string& strPath);
	
	// Unload all cached textures for this material
	void UnloadTextures();
	
	// Material identity
	std::string m_strName;
	std::string m_strAssetPath;  // Path this material was loaded from / saved to
	
	// Material properties
	Zenith_Maths::Vector4 m_xBaseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	float m_fMetallic = 0.0f;
	float m_fRoughness = 0.5f;
	Zenith_Maths::Vector3 m_xEmissiveColor = { 0.0f, 0.0f, 0.0f };
	float m_fEmissiveIntensity = 0.0f;
	bool m_bTransparent = false;
	float m_fAlphaCutoff = 0.5f;
	
	// Texture references (GUID-based, serialized)
	TextureRef m_xDiffuseTextureRef;
	TextureRef m_xNormalTextureRef;
	TextureRef m_xRoughnessMetallicTextureRef;
	TextureRef m_xOcclusionTextureRef;
	TextureRef m_xEmissiveTextureRef;

	// Cached loaded textures (may come from TextureRef or procedural)
	Flux_Texture* m_pxDiffuseTexture = nullptr;
	Flux_Texture* m_pxNormalTexture = nullptr;
	Flux_Texture* m_pxRoughnessMetallicTexture = nullptr;
	Flux_Texture* m_pxOcclusionTexture = nullptr;
	Flux_Texture* m_pxEmissiveTexture = nullptr;
	
	// Dirty flag for unsaved changes
	bool m_bDirty = false;
	
	//--------------------------------------------------------------------------
	// Static Registry
	//--------------------------------------------------------------------------

	// Mutex for thread-safe access to static caches
	static Zenith_Mutex s_xCacheMutex;

	// Material cache by asset path
	static std::unordered_map<std::string, Flux_MaterialAsset*> s_xMaterialCache;

	// Texture cache by path (shared across materials)
	static std::unordered_map<std::string, Flux_Texture*> s_xTextureCache;

	// Next anonymous material ID (for unnamed materials)
	static uint32_t s_uNextMaterialID;

	// Registry of ALL materials (both file-cached and runtime-created)
	// Used by editor to display all materials regardless of how they were created
	static std::vector<Flux_MaterialAsset*> s_xAllMaterials;
};

// Material file format version (increment when format changes)
// Version 2: GUID-based texture references via TextureRef
#define MATERIAL_FILE_VERSION 2
