#pragma once

#include "Maths/Zenith_Maths.h"

class Zenith_DataStream;

// ============================================================================
// Zenith_MaterialParamTable - the material system's parameter contract.
//
// Every numeric/bool/enum material parameter is described once in a static
// table (name, type, range, group, visibility gate). The editor property
// grid, the editor-automation verbs, serialization order, and any future
// node-graph layer all key off this table rather than hardcoding parameter
// lists - adding a parameter here makes it appear everywhere.
//
// Parameter values are accessed through explicit switch-based getters keyed
// by MaterialParamID (not offsetof) so the table works identically on MSVC
// and clang/agde regardless of GLM's layout guarantees.
//
// Override bits: a material instance's override mask uses the MaterialParamID
// as the bit index for parameters, and (32 + MaterialTextureSlot) for texture
// slots. Both ranges are static_asserted to stay below their boundaries.
// ============================================================================

enum MaterialBlendMode : u_int8
{
	MATERIAL_BLEND_OPAQUE = 0,
	MATERIAL_BLEND_MASKED,
	MATERIAL_BLEND_TRANSLUCENT,
	MATERIAL_BLEND_ADDITIVE,
	MATERIAL_BLEND_COUNT
};

enum MaterialShadingModel : u_int8
{
	MATERIAL_SHADING_DEFAULT_LIT = 0,
	MATERIAL_SHADING_UNLIT,
	MATERIAL_SHADING_SUBSURFACE,   // skin: wrapped diffuse + warm scatter terminator
	MATERIAL_SHADING_COUNT
};

enum MaterialTextureSlot : u_int8
{
	MATERIAL_TEXTURE_BASE_COLOR = 0,
	MATERIAL_TEXTURE_NORMAL,
	MATERIAL_TEXTURE_ROUGHNESS_METALLIC,
	MATERIAL_TEXTURE_OCCLUSION,
	MATERIAL_TEXTURE_EMISSIVE,
	MATERIAL_TEXTURE_HEIGHT,
	MATERIAL_TEXTURE_DETAIL_ALBEDO,
	MATERIAL_TEXTURE_DETAIL_NORMAL,
	MATERIAL_TEXTURE_DETAIL_MASK,
	MATERIAL_TEXTURE_SLOT_COUNT
};

enum MaterialParamID : u_int8
{
	MATERIAL_PARAM_BLEND_MODE = 0,
	MATERIAL_PARAM_SHADING_MODEL,
	MATERIAL_PARAM_TWO_SIDED,
	MATERIAL_PARAM_ALPHA_CUTOFF,
	MATERIAL_PARAM_BASE_COLOR,
	MATERIAL_PARAM_METALLIC,
	MATERIAL_PARAM_ROUGHNESS,
	MATERIAL_PARAM_SPECULAR,
	MATERIAL_PARAM_NORMAL_STRENGTH,
	MATERIAL_PARAM_HEIGHT_SCALE,
	MATERIAL_PARAM_POM_MIN_STEPS,
	MATERIAL_PARAM_POM_MAX_STEPS,
	MATERIAL_PARAM_DETAIL_TILING,
	MATERIAL_PARAM_DETAIL_NORMAL_STRENGTH,
	MATERIAL_PARAM_DETAIL_ALBEDO_STRENGTH,
	MATERIAL_PARAM_EMISSIVE_COLOR,
	MATERIAL_PARAM_EMISSIVE_INTENSITY,
	MATERIAL_PARAM_OCCLUSION_STRENGTH,
	MATERIAL_PARAM_UV_TILING,
	MATERIAL_PARAM_UV_OFFSET,
	MATERIAL_PARAM_CLEARCOAT_STRENGTH,
	MATERIAL_PARAM_CLEARCOAT_ROUGHNESS,
	MATERIAL_PARAM_COUNT
};

// Override-mask layout: params occupy bits [0, 32), texture slots [32, 64).
static_assert(MATERIAL_PARAM_COUNT <= 32, "Material param override bits must fit below the texture-slot range");
static_assert(MATERIAL_TEXTURE_SLOT_COUNT <= 32, "Material texture override bits must fit in the upper 32 bits");
constexpr u_int uMATERIAL_TEXTURE_OVERRIDE_BIT_BASE = 32;

enum MaterialParamType : u_int8
{
	MATERIAL_PARAM_TYPE_FLOAT = 0,
	MATERIAL_PARAM_TYPE_FLOAT2,
	MATERIAL_PARAM_TYPE_COLOR3_HDR,
	MATERIAL_PARAM_TYPE_COLOR4,
	MATERIAL_PARAM_TYPE_BOOL,
	MATERIAL_PARAM_TYPE_ENUM
};

enum MaterialParamGroup : u_int8
{
	MATERIAL_GROUP_SURFACE = 0,
	MATERIAL_GROUP_BASE,
	MATERIAL_GROUP_NORMAL_DETAIL,
	MATERIAL_GROUP_EMISSION,
	MATERIAL_GROUP_OCCLUSION,
	MATERIAL_GROUP_UV,
	MATERIAL_GROUP_CLEARCOAT,
	MATERIAL_GROUP_COUNT
};

// Pin gating (UE-style): which editor rows are shown is a function of the
// material's current state, evaluated by the editor against these gates.
enum MaterialParamVisibility : u_int8
{
	MATERIAL_PARAM_VISIBLE_ALWAYS = 0,
	MATERIAL_PARAM_VISIBLE_MASKED_ONLY,   // blend mode == Masked
	MATERIAL_PARAM_VISIBLE_LIT_ONLY,      // shading model == DefaultLit
	MATERIAL_PARAM_VISIBLE_HEIGHT_TEX,    // a height texture is bound
	MATERIAL_PARAM_VISIBLE_DETAIL_TEX,    // any detail texture is bound
	MATERIAL_PARAM_VISIBLE_CLEARCOAT_ON   // clear coat strength > 0
};

enum MaterialTextureDefault : u_int8
{
	MATERIAL_TEXTURE_DEFAULT_WHITE = 0,
	MATERIAL_TEXTURE_DEFAULT_FLAT_NORMAL
};

// ----------------------------------------------------------------------------
// The complete numeric/bool/enum parameter state of a material.
// Defaults follow UE5 (white / 0 / 0.5 / 0.5 / black / 1): a freshly created
// material renders as sane grey plastic under IBL.
// ----------------------------------------------------------------------------
struct Zenith_MaterialParams
{
	// Surface
	MaterialBlendMode m_eBlendMode = MATERIAL_BLEND_OPAQUE;
	MaterialShadingModel m_eShadingModel = MATERIAL_SHADING_DEFAULT_LIT;
	bool m_bTwoSided = false;
	float m_fAlphaCutoff = 0.5f;

	// Base
	Zenith_Maths::Vector4 m_xBaseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	float m_fMetallic = 0.0f;
	float m_fRoughness = 0.5f;
	float m_fSpecular = 0.5f;	// UE convention: F0 = 0.08 * specular (0.5 -> 4%)

	// Normal & detail
	float m_fNormalStrength = 1.0f;
	float m_fHeightScale = 0.05f;
	float m_fPOMMinSteps = 8.0f;
	float m_fPOMMaxSteps = 32.0f;
	Zenith_Maths::Vector2 m_xDetailTiling = { 4.0f, 4.0f };
	float m_fDetailNormalStrength = 1.0f;
	float m_fDetailAlbedoStrength = 1.0f;

	// Emission (HDR: colour * intensity feeds bloom directly)
	Zenith_Maths::Vector3 m_xEmissiveColor = { 0.0f, 0.0f, 0.0f };
	float m_fEmissiveIntensity = 1.0f;

	// Occlusion
	float m_fOcclusionStrength = 1.0f;

	// UV
	Zenith_Maths::Vector2 m_xUVTiling = { 1.0f, 1.0f };
	Zenith_Maths::Vector2 m_xUVOffset = { 0.0f, 0.0f };

	// Clear coat
	float m_fClearCoatStrength = 0.0f;
	float m_fClearCoatRoughness = 0.1f;

	// Serialize the parameter block in the fixed schema-5 field order (the single
	// source of truth for that order — the material asset's WriteToDataStream and
	// its v5 read branch both defer to these). Called EXPLICITLY (never via
	// operator<< / >>) so the trivially-copyable memcpy dispatch can't fire and
	// leak struct padding into the file. ReadFromDataStream clamps the two enum
	// fields to their valid range (forward-compat with unknown future values).
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);
};

struct Zenith_MaterialParamDesc
{
	MaterialParamID m_eID;
	const char* m_szName;			// stable identifier: automation verbs, serialization docs, graph pins
	const char* m_szDisplayName;	// editor label
	MaterialParamType m_eType;
	float m_fMin;
	float m_fMax;
	MaterialParamGroup m_eGroup;
	MaterialParamVisibility m_eVisibility;
	const char* const* m_aszEnumLabels;	// MATERIAL_PARAM_TYPE_ENUM only
	u_int m_uNumEnumLabels;
};

struct Zenith_MaterialTextureSlotDesc
{
	MaterialTextureSlot m_eSlot;
	const char* m_szName;			// stable identifier
	const char* m_szDisplayName;	// editor label
	const char* m_szChannelHint;	// inline channel-packing hint shown in the slot label
	MaterialTextureDefault m_eDefault;
	bool m_bSRGB;					// editor hint: colour data vs linear data
};

namespace Zenith_MaterialParamTable
{
	// Parameter table - indexed by MaterialParamID (table order == enum order,
	// asserted at static-init). Display order in the editor is group-major.
	u_int GetParamCount();
	const Zenith_MaterialParamDesc& GetParamDesc(MaterialParamID eID);
	const Zenith_MaterialParamDesc* FindParamByName(const char* szName);

	u_int GetTextureSlotCount();
	const Zenith_MaterialTextureSlotDesc& GetTextureSlotDesc(MaterialTextureSlot eSlot);
	const Zenith_MaterialTextureSlotDesc* FindTextureSlotByName(const char* szName);

	const char* GetGroupDisplayName(MaterialParamGroup eGroup);

	// Typed value access. Float2/Color3/Color4 params travel through a Vector4
	// (valid components by type: xy / xyz / xyzw). Bool and enum params travel
	// through u_int. Calling the wrong accessor for a param's type asserts.
	float GetParamFloat(const Zenith_MaterialParams& xParams, MaterialParamID eID);
	void SetParamFloat(Zenith_MaterialParams& xParams, MaterialParamID eID, float fValue);
	Zenith_Maths::Vector4 GetParamVector(const Zenith_MaterialParams& xParams, MaterialParamID eID);
	void SetParamVector(Zenith_MaterialParams& xParams, MaterialParamID eID, const Zenith_Maths::Vector4& xValue);
	u_int GetParamInt(const Zenith_MaterialParams& xParams, MaterialParamID eID);
	void SetParamInt(Zenith_MaterialParams& xParams, MaterialParamID eID, u_int uValue);

	// Copy one parameter's value between param blocks (instance resolve overlay).
	void CopyParam(Zenith_MaterialParams& xDst, const Zenith_MaterialParams& xSrc, MaterialParamID eID);
}
