#pragma once

#include "AssetHandling/Zenith_Asset.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "AssetHandling/Zenith_MaterialParamTable.h"
#include "Maths/Zenith_Maths.h"
#include <string>

// Forward declarations
class Zenith_DataStream;
class Zenith_TextureAsset;

// Maximum parent-chain depth for material instances (cycle/degenerate guard).
constexpr u_int uMATERIAL_MAX_PARENT_DEPTH = 8;

// Sentinel for "not yet assigned a GPU material-table slot" (see Flux_MaterialTable).
constexpr u_int uFLUX_INVALID_MATERIAL_INDEX = 0xFFFFFFFFu;

// ----------------------------------------------------------------------------
// The fully-resolved view of a material after applying the parent chain.
// Renderers consume ONLY this (params + per-slot texture handles); they never
// walk the inheritance chain themselves. Texture pointers reference handles
// owned by materials in the chain - parents are ref-held by their children's
// MaterialHandle, so the pointers stay valid while the child is alive.
// ----------------------------------------------------------------------------
struct Zenith_MaterialResolved
{
	Zenith_MaterialParams m_xParams;
	const TextureHandle* m_apxTextures[MATERIAL_TEXTURE_SLOT_COUNT] = {};
};

/**
 * Zenith_MaterialAsset - parameter-based PBR material (UE5/Unity hybrid model).
 *
 * - All numeric/bool/enum parameters live in a Zenith_MaterialParams block
 *   described by the Zenith_MaterialParamTable reflection table (the contract
 *   shared by the editor UI, automation verbs, and any future graph layer).
 * - 9 texture slots (see MaterialTextureSlot), each falling back to a pinned
 *   1x1 default when unset.
 * - Material INSTANCES: a material may name a parent material and override a
 *   subset of params/textures. The override mask uses MaterialParamID as the
 *   bit index for params and (32 + slot) for textures. Non-overridden values
 *   track the parent live. GetResolved() returns the flattened result with
 *   stamp-based caching (cheap when nothing changed).
 * - Serialized as .zmat version 5. Versions 3/4 load with legacy mapping
 *   (Transparent -> Translucent, otherwise Masked - v4 always alpha-tested;
 *   Unlit -> shading model).
 *
 * Usage:
 *   auto* pMat = Zenith_AssetRegistry::Get<Zenith_MaterialAsset>("game:Materials/mat.zmat");
 *   pMat->SetRoughness(0.2f);
 *   pMat->SetTexture(MATERIAL_TEXTURE_BASE_COLOR, TextureHandle("game:Textures/albedo.ztxtr"));
 *
 *   // Instance workflow
 *   pInstance->SetParent(MaterialHandle("game:Materials/master.zmat"));
 *   pInstance->SetBaseColor({1,0,0,1});   // auto-marks the BaseColor override
 *   const Zenith_MaterialResolved& xRes = pInstance->GetResolved();
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

	bool SaveToFile(const std::string& strPath);
	bool Reload();
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

	// Envelope-aware, status-returning parse of an in-memory .zmtrl stream — the
	// file-load error contract (INVALID_ARGUMENT on wrong type id, VERSION_MISMATCH
	// on a newer schema; else reads new-format or legacy-headerless payload).
	// LoadFromFile is ReadFromFile + ParseStream; the void ReadFromDataStream above
	// delegates here. Public so round-trip / robustness tests can drive it stream-only.
	Zenith_Status ParseStream(Zenith_DataStream& xStream);

	//--------------------------------------------------------------------------
	// Identity
	//--------------------------------------------------------------------------

	const std::string& GetName() const { return m_strName; }
	void SetName(const std::string& strName) { m_strName = strName; MarkEdited(); }

	bool IsDirty() const { return m_bDirty; }

	//--------------------------------------------------------------------------
	// Parameter block access
	//
	// GetParams() returns this material's LOCAL values (what the editor shows
	// in its own rows). GetResolved() returns the flattened parent-chain view
	// renderers consume. ModifyParams() hands out mutable access and marks the
	// asset edited - callers editing via the table (editor/automation) must
	// also set the matching override bit themselves when a parent is set; the
	// typed setters below do both automatically.
	//--------------------------------------------------------------------------

	const Zenith_MaterialParams& GetParams() const { return m_xParams; }
	Zenith_MaterialParams& ModifyParams() { MarkEdited(); return m_xParams; }

	const Zenith_MaterialResolved& GetResolved();

	// Monotonic stamp bumped by every edit (params, textures, parent, name).
	u_int64 GetEditStamp() const { return m_uEditStamp; }

	// GPU material-table slot (Flux_MaterialGPU index into g_axMaterials). Assigned
	// lazily + persistently by Flux_MaterialTable::GetOrCreateIndex on the main thread;
	// worker record paths read it lock-free to fill MeshDrawConstants::m_uMaterialIndex.
	u_int GetMaterialTableIndex() const { return m_uMaterialTableIndex; }
	void  SetMaterialTableIndex(u_int uIndex) { m_uMaterialTableIndex = uIndex; }

	//--------------------------------------------------------------------------
	// Typed parameter setters/getters (local values).
	// Setters auto-mark the parameter's override bit when a parent is set.
	//--------------------------------------------------------------------------

	const Zenith_Maths::Vector4& GetBaseColor() const { return m_xParams.m_xBaseColor; }
	void SetBaseColor(const Zenith_Maths::Vector4& xColor) { m_xParams.m_xBaseColor = xColor; OnParamEdited(MATERIAL_PARAM_BASE_COLOR); }

	float GetMetallic() const { return m_xParams.m_fMetallic; }
	void SetMetallic(float fMetallic) { m_xParams.m_fMetallic = fMetallic; OnParamEdited(MATERIAL_PARAM_METALLIC); }

	float GetRoughness() const { return m_xParams.m_fRoughness; }
	void SetRoughness(float fRoughness) { m_xParams.m_fRoughness = fRoughness; OnParamEdited(MATERIAL_PARAM_ROUGHNESS); }

	float GetSpecular() const { return m_xParams.m_fSpecular; }
	void SetSpecular(float fSpecular) { m_xParams.m_fSpecular = fSpecular; OnParamEdited(MATERIAL_PARAM_SPECULAR); }

	const Zenith_Maths::Vector3& GetEmissiveColor() const { return m_xParams.m_xEmissiveColor; }
	void SetEmissiveColor(const Zenith_Maths::Vector3& xColor) { m_xParams.m_xEmissiveColor = xColor; OnParamEdited(MATERIAL_PARAM_EMISSIVE_COLOR); }

	float GetEmissiveIntensity() const { return m_xParams.m_fEmissiveIntensity; }
	void SetEmissiveIntensity(float fIntensity) { m_xParams.m_fEmissiveIntensity = fIntensity; OnParamEdited(MATERIAL_PARAM_EMISSIVE_INTENSITY); }

	float GetAlphaCutoff() const { return m_xParams.m_fAlphaCutoff; }
	void SetAlphaCutoff(float fCutoff) { m_xParams.m_fAlphaCutoff = fCutoff; OnParamEdited(MATERIAL_PARAM_ALPHA_CUTOFF); }

	const Zenith_Maths::Vector2& GetUVTiling() const { return m_xParams.m_xUVTiling; }
	void SetUVTiling(const Zenith_Maths::Vector2& xTiling) { m_xParams.m_xUVTiling = xTiling; OnParamEdited(MATERIAL_PARAM_UV_TILING); }

	const Zenith_Maths::Vector2& GetUVOffset() const { return m_xParams.m_xUVOffset; }
	void SetUVOffset(const Zenith_Maths::Vector2& xOffset) { m_xParams.m_xUVOffset = xOffset; OnParamEdited(MATERIAL_PARAM_UV_OFFSET); }

	float GetOcclusionStrength() const { return m_xParams.m_fOcclusionStrength; }
	void SetOcclusionStrength(float fStrength) { m_xParams.m_fOcclusionStrength = fStrength; OnParamEdited(MATERIAL_PARAM_OCCLUSION_STRENGTH); }

	float GetNormalStrength() const { return m_xParams.m_fNormalStrength; }
	void SetNormalStrength(float fStrength) { m_xParams.m_fNormalStrength = fStrength; OnParamEdited(MATERIAL_PARAM_NORMAL_STRENGTH); }

	float GetHeightScale() const { return m_xParams.m_fHeightScale; }
	void SetHeightScale(float fScale) { m_xParams.m_fHeightScale = fScale; OnParamEdited(MATERIAL_PARAM_HEIGHT_SCALE); }

	const Zenith_Maths::Vector2& GetDetailTiling() const { return m_xParams.m_xDetailTiling; }
	void SetDetailTiling(const Zenith_Maths::Vector2& xTiling) { m_xParams.m_xDetailTiling = xTiling; OnParamEdited(MATERIAL_PARAM_DETAIL_TILING); }

	float GetClearCoatStrength() const { return m_xParams.m_fClearCoatStrength; }
	void SetClearCoatStrength(float fStrength) { m_xParams.m_fClearCoatStrength = fStrength; OnParamEdited(MATERIAL_PARAM_CLEARCOAT_STRENGTH); }

	float GetClearCoatRoughness() const { return m_xParams.m_fClearCoatRoughness; }
	void SetClearCoatRoughness(float fRoughness) { m_xParams.m_fClearCoatRoughness = fRoughness; OnParamEdited(MATERIAL_PARAM_CLEARCOAT_ROUGHNESS); }

	MaterialBlendMode GetBlendMode() const { return m_xParams.m_eBlendMode; }
	void SetBlendMode(MaterialBlendMode eMode) { m_xParams.m_eBlendMode = eMode; OnParamEdited(MATERIAL_PARAM_BLEND_MODE); }

	MaterialShadingModel GetShadingModel() const { return m_xParams.m_eShadingModel; }
	void SetShadingModel(MaterialShadingModel eModel) { m_xParams.m_eShadingModel = eModel; OnParamEdited(MATERIAL_PARAM_SHADING_MODEL); }

	bool IsTwoSided() const { return m_xParams.m_bTwoSided; }
	void SetTwoSided(bool bTwoSided) { m_xParams.m_bTwoSided = bTwoSided; OnParamEdited(MATERIAL_PARAM_TWO_SIDED); }

	bool IsUnlit() const { return m_xParams.m_eShadingModel == MATERIAL_SHADING_UNLIT; }
	void SetUnlit(bool bUnlit) { SetShadingModel(bUnlit ? MATERIAL_SHADING_UNLIT : MATERIAL_SHADING_DEFAULT_LIT); }

	// Legacy transparency shim. v4 materials always alpha-tested, so "not
	// transparent" maps to Masked rather than Opaque.
	bool IsTransparent() const { return m_xParams.m_eBlendMode == MATERIAL_BLEND_TRANSLUCENT; }
	void SetTransparent(bool bTransparent) { SetBlendMode(bTransparent ? MATERIAL_BLEND_TRANSLUCENT : MATERIAL_BLEND_MASKED); }

	//--------------------------------------------------------------------------
	// Material instances (parent + per-param/texture overrides)
	//--------------------------------------------------------------------------

	// Returns false (and leaves the parent unchanged) on self-parent, cycle,
	// or chain deeper than uMATERIAL_MAX_PARENT_DEPTH.
	bool SetParent(const MaterialHandle& xParent);
	void ClearParent();
	const MaterialHandle& GetParentHandle() const { return m_xParentMaterial; }
	Zenith_MaterialAsset* GetParent() { return m_xParentMaterial.Resolve(); }
	bool HasParent() const { return static_cast<bool>(m_xParentMaterial); }

	u_int64 GetOverrideMask() const { return m_uOverrideMask; }
	bool HasOverride(u_int uBit) const { return (m_uOverrideMask & (1ull << uBit)) != 0; }
	void SetOverride(u_int uBit, bool bOverridden);
	bool HasParamOverride(MaterialParamID eID) const { return HasOverride(eID); }
	bool HasTextureOverride(MaterialTextureSlot eSlot) const { return HasOverride(uMATERIAL_TEXTURE_OVERRIDE_BIT_BASE + eSlot); }

	//--------------------------------------------------------------------------
	// Texture slots
	//--------------------------------------------------------------------------

	void SetTexture(MaterialTextureSlot eSlot, TextureHandle xHandle);
	const TextureHandle& GetTextureHandle(MaterialTextureSlot eSlot) const;
	const std::string& GetTexturePath(MaterialTextureSlot eSlot) const { return GetTextureHandle(eSlot).GetPath(); }

	// Resolves the slot's LOCAL handle, falling back to the slot's pinned
	// default (white / flat normal) when unset or failed to load.
	Zenith_TextureAsset* GetTexture(MaterialTextureSlot eSlot);

	// Resolves through the parent chain (instance-aware), then defaults.
	Zenith_TextureAsset* GetResolvedTexture(MaterialTextureSlot eSlot);

	// Legacy named accessors (BaseColor slot was historically "Diffuse").
	void SetDiffuseTexture(TextureHandle xHandle) { SetTexture(MATERIAL_TEXTURE_BASE_COLOR, std::move(xHandle)); }
	void SetNormalTexture(TextureHandle xHandle) { SetTexture(MATERIAL_TEXTURE_NORMAL, std::move(xHandle)); }
	void SetRoughnessMetallicTexture(TextureHandle xHandle) { SetTexture(MATERIAL_TEXTURE_ROUGHNESS_METALLIC, std::move(xHandle)); }
	void SetOcclusionTexture(TextureHandle xHandle) { SetTexture(MATERIAL_TEXTURE_OCCLUSION, std::move(xHandle)); }
	void SetEmissiveTexture(TextureHandle xHandle) { SetTexture(MATERIAL_TEXTURE_EMISSIVE, std::move(xHandle)); }

	const TextureHandle& GetDiffuseTextureHandle() const { return GetTextureHandle(MATERIAL_TEXTURE_BASE_COLOR); }
	const TextureHandle& GetNormalTextureHandle() const { return GetTextureHandle(MATERIAL_TEXTURE_NORMAL); }
	const TextureHandle& GetRoughnessMetallicTextureHandle() const { return GetTextureHandle(MATERIAL_TEXTURE_ROUGHNESS_METALLIC); }
	const TextureHandle& GetOcclusionTextureHandle() const { return GetTextureHandle(MATERIAL_TEXTURE_OCCLUSION); }
	const TextureHandle& GetEmissiveTextureHandle() const { return GetTextureHandle(MATERIAL_TEXTURE_EMISSIVE); }

	const std::string& GetDiffuseTexturePath() const { return GetTexturePath(MATERIAL_TEXTURE_BASE_COLOR); }
	const std::string& GetNormalTexturePath() const { return GetTexturePath(MATERIAL_TEXTURE_NORMAL); }
	const std::string& GetRoughnessMetallicTexturePath() const { return GetTexturePath(MATERIAL_TEXTURE_ROUGHNESS_METALLIC); }
	const std::string& GetOcclusionTexturePath() const { return GetTexturePath(MATERIAL_TEXTURE_OCCLUSION); }
	const std::string& GetEmissiveTexturePath() const { return GetTexturePath(MATERIAL_TEXTURE_EMISSIVE); }

	Zenith_TextureAsset* GetDiffuseTexture() { return GetTexture(MATERIAL_TEXTURE_BASE_COLOR); }
	Zenith_TextureAsset* GetNormalTexture() { return GetTexture(MATERIAL_TEXTURE_NORMAL); }
	Zenith_TextureAsset* GetRoughnessMetallicTexture() { return GetTexture(MATERIAL_TEXTURE_ROUGHNESS_METALLIC); }
	Zenith_TextureAsset* GetOcclusionTexture() { return GetTexture(MATERIAL_TEXTURE_OCCLUSION); }
	Zenith_TextureAsset* GetEmissiveTexture() { return GetTexture(MATERIAL_TEXTURE_EMISSIVE); }

	//--------------------------------------------------------------------------
	// Default Textures (static, for fallback). Pinned via TextureHandle so
	// Zenith_AssetRegistry::UnloadUnused never frees them while the engine runs.
	//--------------------------------------------------------------------------

	static Zenith_TextureAsset* GetDefaultWhiteTexture();
	static Zenith_TextureAsset* GetDefaultNormalTexture();
	static Zenith_TextureAsset* GetDefaultTextureForSlot(MaterialTextureSlot eSlot);
	static void InitializeDefaults();
	static void ShutdownDefaults();

	// Drop the default texture handles before Zenith_AssetRegistry::Shutdown.
	// Called from Flux::ReleaseAssetReferences. Distinct from ShutdownDefaults
	// (which has been preserved as a no-op for callers that still invoke it).
	static void ReleaseDefaults();

private:
	friend class Zenith_AssetRegistry;
	template<typename U> friend struct Zenith_AssetLoadTraits;   // DoLoad calls private LoadFromFile

	/**
	 * Load material data from file (private - use Zenith_AssetRegistry::Get)
	 */
	Zenith_Status LoadFromFile(const std::string& strPath);

	void MarkEdited() { m_bDirty = true; ++m_uEditStamp; }

	// Typed-setter hook: auto-mark the override bit when this material is an
	// instance (has a parent), then bump the edit stamp.
	void OnParamEdited(MaterialParamID eID)
	{
		if (HasParent())
		{
			m_uOverrideMask |= (1ull << eID);
		}
		MarkEdited();
	}

	// Resolve + validate the parent chain. Returns nullptr when there is no
	// parent, the parent fails to load, or the chain is cyclic/too deep
	// (reported once per offending configuration, then degrades to no-parent).
	Zenith_MaterialAsset* ResolveParentChecked();

	// Material identity
	std::string m_strName;

	// Parameter block (local values; see GetResolved for the instance view)
	Zenith_MaterialParams m_xParams;

	// Texture slots - each handle stores either a path (file-backed,
	// lazy-loaded via the registry) or a procedural pointer set via
	// TextureHandle::Set(). Resolve() handles both.
	TextureHandle m_axTextures[MATERIAL_TEXTURE_SLOT_COUNT];

	// Instance state
	MaterialHandle m_xParentMaterial;
	u_int64 m_uOverrideMask = 0;

	// GPU material-table slot (uFLUX_INVALID_MATERIAL_INDEX until first registered).
	u_int m_uMaterialTableIndex = uFLUX_INVALID_MATERIAL_INDEX;

	// Edit tracking / resolve cache
	u_int64 m_uEditStamp = 0;
	u_int64 m_uResolveStamp = 0;			// bumped each time m_xResolved rebuilds
	u_int64 m_uResolvedSelfStamp = 0;		// m_uEditStamp captured at last rebuild
	u_int64 m_uResolvedParentStamp = 0;		// parent's m_uResolveStamp captured at last rebuild
	Zenith_MaterialAsset* m_pxResolvedParent = nullptr;
	Zenith_MaterialResolved m_xResolved;
	bool m_bResolvedValid = false;
	bool m_bReportedParentError = false;

	// Dirty flag
	bool m_bDirty = false;

	// Static default textures (pinned via handle so they survive UnloadUnused).
	static TextureHandle s_xDefaultWhite;
	static TextureHandle s_xDefaultNormal;
};

// Material on-disk schema version now lives in AssetHandling/Zenith_AssetTypeIds.h
// (uZENITH_MATERIAL_SCHEMA_CURRENT == 5), alongside every other typed asset's id
// and schema. Legacy layouts still read: v5 = param block + specular/normal-strength/
// POM/detail/clear-coat + blend/shading enums + parent path + override mask + 9 slots;
// v4 = flat params, 5 texture slots by path; v3 = no UV/occlusion/flags block.

//--------------------------------------------------------------------------
// Register loader with asset registry
//--------------------------------------------------------------------------

void Zenith_MaterialAsset_RegisterLoader();
