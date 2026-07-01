#include "Zenith.h"
#include "AssetHandling/Zenith_MaterialParamTable.h"
#include "DataStream/Zenith_DataStream.h"

#include <cstring>

// ============================================================================
// Zenith_MaterialParams (de)serialization — the single source of truth for the
// schema-5 parameter field order. The material asset's WriteToDataStream and its
// v5 read branch both defer here, so the ~22-field list is written ONCE instead of
// being mirrored by hand in two directions. Byte-identical to the historical inline
// layout. Fields are streamed explicitly (padding never leaks into the file).
// ============================================================================
void Zenith_MaterialParams::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << static_cast<uint32_t>(m_eBlendMode);
	xStream << static_cast<uint32_t>(m_eShadingModel);
	xStream << m_bTwoSided;
	xStream << m_fAlphaCutoff;

	xStream << m_xBaseColor.x;
	xStream << m_xBaseColor.y;
	xStream << m_xBaseColor.z;
	xStream << m_xBaseColor.w;
	xStream << m_fMetallic;
	xStream << m_fRoughness;
	xStream << m_fSpecular;

	xStream << m_fNormalStrength;
	xStream << m_fHeightScale;
	xStream << m_fPOMMinSteps;
	xStream << m_fPOMMaxSteps;
	xStream << m_xDetailTiling.x;
	xStream << m_xDetailTiling.y;
	xStream << m_fDetailNormalStrength;
	xStream << m_fDetailAlbedoStrength;

	xStream << m_xEmissiveColor.x;
	xStream << m_xEmissiveColor.y;
	xStream << m_xEmissiveColor.z;
	xStream << m_fEmissiveIntensity;

	xStream << m_fOcclusionStrength;

	xStream << m_xUVTiling.x;
	xStream << m_xUVTiling.y;
	xStream << m_xUVOffset.x;
	xStream << m_xUVOffset.y;

	xStream << m_fClearCoatStrength;
	xStream << m_fClearCoatRoughness;
}

void Zenith_MaterialParams::ReadFromDataStream(Zenith_DataStream& xStream)
{
	uint32_t uBlendMode = 0;
	uint32_t uShadingModel = 0;
	xStream >> uBlendMode;
	xStream >> uShadingModel;
	m_eBlendMode = (uBlendMode < MATERIAL_BLEND_COUNT) ? static_cast<MaterialBlendMode>(uBlendMode) : MATERIAL_BLEND_OPAQUE;
	m_eShadingModel = (uShadingModel < MATERIAL_SHADING_COUNT) ? static_cast<MaterialShadingModel>(uShadingModel) : MATERIAL_SHADING_DEFAULT_LIT;
	xStream >> m_bTwoSided;
	xStream >> m_fAlphaCutoff;

	xStream >> m_xBaseColor.x;
	xStream >> m_xBaseColor.y;
	xStream >> m_xBaseColor.z;
	xStream >> m_xBaseColor.w;
	xStream >> m_fMetallic;
	xStream >> m_fRoughness;
	xStream >> m_fSpecular;

	xStream >> m_fNormalStrength;
	xStream >> m_fHeightScale;
	xStream >> m_fPOMMinSteps;
	xStream >> m_fPOMMaxSteps;
	xStream >> m_xDetailTiling.x;
	xStream >> m_xDetailTiling.y;
	xStream >> m_fDetailNormalStrength;
	xStream >> m_fDetailAlbedoStrength;

	xStream >> m_xEmissiveColor.x;
	xStream >> m_xEmissiveColor.y;
	xStream >> m_xEmissiveColor.z;
	xStream >> m_fEmissiveIntensity;

	xStream >> m_fOcclusionStrength;

	xStream >> m_xUVTiling.x;
	xStream >> m_xUVTiling.y;
	xStream >> m_xUVOffset.x;
	xStream >> m_xUVOffset.y;

	xStream >> m_fClearCoatStrength;
	xStream >> m_fClearCoatRoughness;
}

// ============================================================================
// The material parameter contract - see header for the design notes.
// Table order MUST match MaterialParamID order (asserted below) so the table
// can be indexed directly by ID.
// ============================================================================

namespace
{
	static const char* const ls_aszBlendModeLabels[] = { "Opaque", "Masked", "Translucent", "Additive" };
	static const char* const ls_aszShadingModelLabels[] = { "Default Lit", "Unlit", "Subsurface" };

	static const Zenith_MaterialParamDesc ls_axParams[] =
	{
		{ MATERIAL_PARAM_BLEND_MODE,              "BlendMode",            "Blend Mode",             MATERIAL_PARAM_TYPE_ENUM,       0.f, 0.f,   MATERIAL_GROUP_SURFACE,       MATERIAL_PARAM_VISIBLE_ALWAYS,       ls_aszBlendModeLabels,    MATERIAL_BLEND_COUNT },
		{ MATERIAL_PARAM_SHADING_MODEL,           "ShadingModel",         "Shading Model",          MATERIAL_PARAM_TYPE_ENUM,       0.f, 0.f,   MATERIAL_GROUP_SURFACE,       MATERIAL_PARAM_VISIBLE_ALWAYS,       ls_aszShadingModelLabels, MATERIAL_SHADING_COUNT },
		{ MATERIAL_PARAM_TWO_SIDED,               "TwoSided",             "Two Sided",              MATERIAL_PARAM_TYPE_BOOL,       0.f, 1.f,   MATERIAL_GROUP_SURFACE,       MATERIAL_PARAM_VISIBLE_ALWAYS,       nullptr, 0 },
		{ MATERIAL_PARAM_ALPHA_CUTOFF,            "AlphaCutoff",          "Alpha Cutoff",           MATERIAL_PARAM_TYPE_FLOAT,      0.f, 1.f,   MATERIAL_GROUP_SURFACE,       MATERIAL_PARAM_VISIBLE_MASKED_ONLY,  nullptr, 0 },
		{ MATERIAL_PARAM_BASE_COLOR,              "BaseColor",            "Base Colour",            MATERIAL_PARAM_TYPE_COLOR4,     0.f, 1.f,   MATERIAL_GROUP_BASE,          MATERIAL_PARAM_VISIBLE_ALWAYS,       nullptr, 0 },
		{ MATERIAL_PARAM_METALLIC,                "Metallic",             "Metallic",               MATERIAL_PARAM_TYPE_FLOAT,      0.f, 1.f,   MATERIAL_GROUP_BASE,          MATERIAL_PARAM_VISIBLE_LIT_ONLY,     nullptr, 0 },
		{ MATERIAL_PARAM_ROUGHNESS,               "Roughness",            "Roughness",              MATERIAL_PARAM_TYPE_FLOAT,      0.f, 1.f,   MATERIAL_GROUP_BASE,          MATERIAL_PARAM_VISIBLE_LIT_ONLY,     nullptr, 0 },
		{ MATERIAL_PARAM_SPECULAR,                "Specular",             "Specular",               MATERIAL_PARAM_TYPE_FLOAT,      0.f, 1.f,   MATERIAL_GROUP_BASE,          MATERIAL_PARAM_VISIBLE_LIT_ONLY,     nullptr, 0 },
		{ MATERIAL_PARAM_NORMAL_STRENGTH,         "NormalStrength",       "Normal Strength",        MATERIAL_PARAM_TYPE_FLOAT,      0.f, 2.f,   MATERIAL_GROUP_NORMAL_DETAIL, MATERIAL_PARAM_VISIBLE_LIT_ONLY,     nullptr, 0 },
		{ MATERIAL_PARAM_HEIGHT_SCALE,            "HeightScale",          "Height Scale",           MATERIAL_PARAM_TYPE_FLOAT,      0.f, 0.2f,  MATERIAL_GROUP_NORMAL_DETAIL, MATERIAL_PARAM_VISIBLE_HEIGHT_TEX,   nullptr, 0 },
		{ MATERIAL_PARAM_POM_MIN_STEPS,           "POMMinSteps",          "Parallax Min Steps",     MATERIAL_PARAM_TYPE_FLOAT,      4.f, 64.f,  MATERIAL_GROUP_NORMAL_DETAIL, MATERIAL_PARAM_VISIBLE_HEIGHT_TEX,   nullptr, 0 },
		{ MATERIAL_PARAM_POM_MAX_STEPS,           "POMMaxSteps",          "Parallax Max Steps",     MATERIAL_PARAM_TYPE_FLOAT,      4.f, 64.f,  MATERIAL_GROUP_NORMAL_DETAIL, MATERIAL_PARAM_VISIBLE_HEIGHT_TEX,   nullptr, 0 },
		{ MATERIAL_PARAM_DETAIL_TILING,           "DetailTiling",         "Detail Tiling",          MATERIAL_PARAM_TYPE_FLOAT2,     0.01f, 64.f, MATERIAL_GROUP_NORMAL_DETAIL, MATERIAL_PARAM_VISIBLE_DETAIL_TEX,  nullptr, 0 },
		{ MATERIAL_PARAM_DETAIL_NORMAL_STRENGTH,  "DetailNormalStrength", "Detail Normal Strength", MATERIAL_PARAM_TYPE_FLOAT,      0.f, 2.f,   MATERIAL_GROUP_NORMAL_DETAIL, MATERIAL_PARAM_VISIBLE_DETAIL_TEX,   nullptr, 0 },
		{ MATERIAL_PARAM_DETAIL_ALBEDO_STRENGTH,  "DetailAlbedoStrength", "Detail Albedo Strength", MATERIAL_PARAM_TYPE_FLOAT,      0.f, 1.f,   MATERIAL_GROUP_NORMAL_DETAIL, MATERIAL_PARAM_VISIBLE_DETAIL_TEX,   nullptr, 0 },
		{ MATERIAL_PARAM_EMISSIVE_COLOR,          "EmissiveColor",        "Emissive Colour",        MATERIAL_PARAM_TYPE_COLOR3_HDR, 0.f, 1.f,   MATERIAL_GROUP_EMISSION,      MATERIAL_PARAM_VISIBLE_ALWAYS,       nullptr, 0 },
		{ MATERIAL_PARAM_EMISSIVE_INTENSITY,      "EmissiveIntensity",    "Emissive Intensity",     MATERIAL_PARAM_TYPE_FLOAT,      0.f, 100.f, MATERIAL_GROUP_EMISSION,      MATERIAL_PARAM_VISIBLE_ALWAYS,       nullptr, 0 },
		{ MATERIAL_PARAM_OCCLUSION_STRENGTH,      "OcclusionStrength",    "Occlusion Strength",     MATERIAL_PARAM_TYPE_FLOAT,      0.f, 1.f,   MATERIAL_GROUP_OCCLUSION,     MATERIAL_PARAM_VISIBLE_LIT_ONLY,     nullptr, 0 },
		{ MATERIAL_PARAM_UV_TILING,               "UVTiling",             "UV Tiling",              MATERIAL_PARAM_TYPE_FLOAT2,     0.01f, 64.f, MATERIAL_GROUP_UV,           MATERIAL_PARAM_VISIBLE_ALWAYS,       nullptr, 0 },
		{ MATERIAL_PARAM_UV_OFFSET,               "UVOffset",             "UV Offset",              MATERIAL_PARAM_TYPE_FLOAT2,     -10.f, 10.f, MATERIAL_GROUP_UV,           MATERIAL_PARAM_VISIBLE_ALWAYS,       nullptr, 0 },
		{ MATERIAL_PARAM_CLEARCOAT_STRENGTH,      "ClearCoat",            "Clear Coat",             MATERIAL_PARAM_TYPE_FLOAT,      0.f, 1.f,   MATERIAL_GROUP_CLEARCOAT,     MATERIAL_PARAM_VISIBLE_LIT_ONLY,     nullptr, 0 },
		{ MATERIAL_PARAM_CLEARCOAT_ROUGHNESS,     "ClearCoatRoughness",   "Clear Coat Roughness",   MATERIAL_PARAM_TYPE_FLOAT,      0.f, 1.f,   MATERIAL_GROUP_CLEARCOAT,     MATERIAL_PARAM_VISIBLE_CLEARCOAT_ON, nullptr, 0 },
	};
	static_assert(sizeof(ls_axParams) / sizeof(ls_axParams[0]) == MATERIAL_PARAM_COUNT,
		"Material param table must have exactly one entry per MaterialParamID");

	static const Zenith_MaterialTextureSlotDesc ls_axTextureSlots[] =
	{
		{ MATERIAL_TEXTURE_BASE_COLOR,         "BaseColor",         "Base Colour",   "RGB = Albedo (sRGB), A = Opacity",      MATERIAL_TEXTURE_DEFAULT_WHITE,       true },
		{ MATERIAL_TEXTURE_NORMAL,             "Normal",            "Normal",        "RGB = Tangent-space normal",            MATERIAL_TEXTURE_DEFAULT_FLAT_NORMAL, false },
		{ MATERIAL_TEXTURE_ROUGHNESS_METALLIC, "RoughnessMetallic", "Rough/Metal",   "G = Roughness, B = Metallic",           MATERIAL_TEXTURE_DEFAULT_WHITE,       false },
		{ MATERIAL_TEXTURE_OCCLUSION,          "Occlusion",         "Occlusion",     "R = Ambient occlusion",                 MATERIAL_TEXTURE_DEFAULT_WHITE,       false },
		{ MATERIAL_TEXTURE_EMISSIVE,           "Emissive",          "Emissive",      "RGB = Emissive (x Colour x Intensity)", MATERIAL_TEXTURE_DEFAULT_WHITE,       true },
		{ MATERIAL_TEXTURE_HEIGHT,             "Height",            "Height",        "R = Height (1 = surface top)",          MATERIAL_TEXTURE_DEFAULT_WHITE,       false },
		{ MATERIAL_TEXTURE_DETAIL_ALBEDO,      "DetailAlbedo",      "Detail Albedo", "RGB = Detail albedo (x2 overlay)",      MATERIAL_TEXTURE_DEFAULT_WHITE,       true },
		{ MATERIAL_TEXTURE_DETAIL_NORMAL,      "DetailNormal",      "Detail Normal", "RGB = Tangent-space detail normal",     MATERIAL_TEXTURE_DEFAULT_FLAT_NORMAL, false },
		{ MATERIAL_TEXTURE_DETAIL_MASK,        "DetailMask",        "Detail Mask",   "R = Detail mask",                       MATERIAL_TEXTURE_DEFAULT_WHITE,       false },
	};
	static_assert(sizeof(ls_axTextureSlots) / sizeof(ls_axTextureSlots[0]) == MATERIAL_TEXTURE_SLOT_COUNT,
		"Material texture-slot table must have exactly one entry per MaterialTextureSlot");

	static const char* const ls_aszGroupDisplayNames[MATERIAL_GROUP_COUNT] =
	{
		"Surface Options",
		"Base",
		"Normal & Detail",
		"Emission",
		"Occlusion",
		"UV",
		"Clear Coat",
	};
}

namespace Zenith_MaterialParamTable
{
	u_int GetParamCount()
	{
		return MATERIAL_PARAM_COUNT;
	}

	const Zenith_MaterialParamDesc& GetParamDesc(MaterialParamID eID)
	{
		Zenith_Assert(eID < MATERIAL_PARAM_COUNT, "Invalid material param ID %u", eID);
		const Zenith_MaterialParamDesc& xDesc = ls_axParams[eID];
		Zenith_Assert(xDesc.m_eID == eID, "Material param table order out of sync with MaterialParamID");
		return xDesc;
	}

	const Zenith_MaterialParamDesc* FindParamByName(const char* szName)
	{
		if (szName == nullptr)
		{
			return nullptr;
		}
		for (u_int u = 0; u < MATERIAL_PARAM_COUNT; u++)
		{
			if (strcmp(ls_axParams[u].m_szName, szName) == 0)
			{
				return &ls_axParams[u];
			}
		}
		return nullptr;
	}

	u_int GetTextureSlotCount()
	{
		return MATERIAL_TEXTURE_SLOT_COUNT;
	}

	const Zenith_MaterialTextureSlotDesc& GetTextureSlotDesc(MaterialTextureSlot eSlot)
	{
		Zenith_Assert(eSlot < MATERIAL_TEXTURE_SLOT_COUNT, "Invalid material texture slot %u", eSlot);
		const Zenith_MaterialTextureSlotDesc& xDesc = ls_axTextureSlots[eSlot];
		Zenith_Assert(xDesc.m_eSlot == eSlot, "Material texture-slot table order out of sync with MaterialTextureSlot");
		return xDesc;
	}

	const Zenith_MaterialTextureSlotDesc* FindTextureSlotByName(const char* szName)
	{
		if (szName == nullptr)
		{
			return nullptr;
		}
		for (u_int u = 0; u < MATERIAL_TEXTURE_SLOT_COUNT; u++)
		{
			if (strcmp(ls_axTextureSlots[u].m_szName, szName) == 0)
			{
				return &ls_axTextureSlots[u];
			}
		}
		return nullptr;
	}

	const char* GetGroupDisplayName(MaterialParamGroup eGroup)
	{
		Zenith_Assert(eGroup < MATERIAL_GROUP_COUNT, "Invalid material param group %u", eGroup);
		return ls_aszGroupDisplayNames[eGroup];
	}

	float GetParamFloat(const Zenith_MaterialParams& xParams, MaterialParamID eID)
	{
		switch (eID)
		{
			case MATERIAL_PARAM_ALPHA_CUTOFF:           return xParams.m_fAlphaCutoff;
			case MATERIAL_PARAM_METALLIC:               return xParams.m_fMetallic;
			case MATERIAL_PARAM_ROUGHNESS:              return xParams.m_fRoughness;
			case MATERIAL_PARAM_SPECULAR:               return xParams.m_fSpecular;
			case MATERIAL_PARAM_NORMAL_STRENGTH:        return xParams.m_fNormalStrength;
			case MATERIAL_PARAM_HEIGHT_SCALE:           return xParams.m_fHeightScale;
			case MATERIAL_PARAM_POM_MIN_STEPS:          return xParams.m_fPOMMinSteps;
			case MATERIAL_PARAM_POM_MAX_STEPS:          return xParams.m_fPOMMaxSteps;
			case MATERIAL_PARAM_DETAIL_NORMAL_STRENGTH: return xParams.m_fDetailNormalStrength;
			case MATERIAL_PARAM_DETAIL_ALBEDO_STRENGTH: return xParams.m_fDetailAlbedoStrength;
			case MATERIAL_PARAM_EMISSIVE_INTENSITY:     return xParams.m_fEmissiveIntensity;
			case MATERIAL_PARAM_OCCLUSION_STRENGTH:     return xParams.m_fOcclusionStrength;
			case MATERIAL_PARAM_CLEARCOAT_STRENGTH:     return xParams.m_fClearCoatStrength;
			case MATERIAL_PARAM_CLEARCOAT_ROUGHNESS:    return xParams.m_fClearCoatRoughness;
			default:
				Zenith_Assert(false, "GetParamFloat called for non-float material param %u", eID);
				return 0.0f;
		}
	}

	void SetParamFloat(Zenith_MaterialParams& xParams, MaterialParamID eID, float fValue)
	{
		switch (eID)
		{
			case MATERIAL_PARAM_ALPHA_CUTOFF:           xParams.m_fAlphaCutoff = fValue; break;
			case MATERIAL_PARAM_METALLIC:               xParams.m_fMetallic = fValue; break;
			case MATERIAL_PARAM_ROUGHNESS:              xParams.m_fRoughness = fValue; break;
			case MATERIAL_PARAM_SPECULAR:               xParams.m_fSpecular = fValue; break;
			case MATERIAL_PARAM_NORMAL_STRENGTH:        xParams.m_fNormalStrength = fValue; break;
			case MATERIAL_PARAM_HEIGHT_SCALE:           xParams.m_fHeightScale = fValue; break;
			case MATERIAL_PARAM_POM_MIN_STEPS:          xParams.m_fPOMMinSteps = fValue; break;
			case MATERIAL_PARAM_POM_MAX_STEPS:          xParams.m_fPOMMaxSteps = fValue; break;
			case MATERIAL_PARAM_DETAIL_NORMAL_STRENGTH: xParams.m_fDetailNormalStrength = fValue; break;
			case MATERIAL_PARAM_DETAIL_ALBEDO_STRENGTH: xParams.m_fDetailAlbedoStrength = fValue; break;
			case MATERIAL_PARAM_EMISSIVE_INTENSITY:     xParams.m_fEmissiveIntensity = fValue; break;
			case MATERIAL_PARAM_OCCLUSION_STRENGTH:     xParams.m_fOcclusionStrength = fValue; break;
			case MATERIAL_PARAM_CLEARCOAT_STRENGTH:     xParams.m_fClearCoatStrength = fValue; break;
			case MATERIAL_PARAM_CLEARCOAT_ROUGHNESS:    xParams.m_fClearCoatRoughness = fValue; break;
			default:
				Zenith_Assert(false, "SetParamFloat called for non-float material param %u", eID);
				break;
		}
	}

	Zenith_Maths::Vector4 GetParamVector(const Zenith_MaterialParams& xParams, MaterialParamID eID)
	{
		switch (eID)
		{
			case MATERIAL_PARAM_BASE_COLOR:     return xParams.m_xBaseColor;
			case MATERIAL_PARAM_EMISSIVE_COLOR: return Zenith_Maths::Vector4(xParams.m_xEmissiveColor, 0.0f);
			case MATERIAL_PARAM_DETAIL_TILING:  return Zenith_Maths::Vector4(xParams.m_xDetailTiling, 0.0f, 0.0f);
			case MATERIAL_PARAM_UV_TILING:      return Zenith_Maths::Vector4(xParams.m_xUVTiling, 0.0f, 0.0f);
			case MATERIAL_PARAM_UV_OFFSET:      return Zenith_Maths::Vector4(xParams.m_xUVOffset, 0.0f, 0.0f);
			default:
				Zenith_Assert(false, "GetParamVector called for non-vector material param %u", eID);
				return Zenith_Maths::Vector4(0.0f);
		}
	}

	void SetParamVector(Zenith_MaterialParams& xParams, MaterialParamID eID, const Zenith_Maths::Vector4& xValue)
	{
		switch (eID)
		{
			case MATERIAL_PARAM_BASE_COLOR:     xParams.m_xBaseColor = xValue; break;
			case MATERIAL_PARAM_EMISSIVE_COLOR: xParams.m_xEmissiveColor = Zenith_Maths::Vector3(xValue); break;
			case MATERIAL_PARAM_DETAIL_TILING:  xParams.m_xDetailTiling = Zenith_Maths::Vector2(xValue); break;
			case MATERIAL_PARAM_UV_TILING:      xParams.m_xUVTiling = Zenith_Maths::Vector2(xValue); break;
			case MATERIAL_PARAM_UV_OFFSET:      xParams.m_xUVOffset = Zenith_Maths::Vector2(xValue); break;
			default:
				Zenith_Assert(false, "SetParamVector called for non-vector material param %u", eID);
				break;
		}
	}

	u_int GetParamInt(const Zenith_MaterialParams& xParams, MaterialParamID eID)
	{
		switch (eID)
		{
			case MATERIAL_PARAM_BLEND_MODE:    return xParams.m_eBlendMode;
			case MATERIAL_PARAM_SHADING_MODEL: return xParams.m_eShadingModel;
			case MATERIAL_PARAM_TWO_SIDED:     return xParams.m_bTwoSided ? 1u : 0u;
			default:
				Zenith_Assert(false, "GetParamInt called for non-int material param %u", eID);
				return 0;
		}
	}

	void SetParamInt(Zenith_MaterialParams& xParams, MaterialParamID eID, u_int uValue)
	{
		switch (eID)
		{
			case MATERIAL_PARAM_BLEND_MODE:
				Zenith_Assert(uValue < MATERIAL_BLEND_COUNT, "Invalid blend mode %u", uValue);
				xParams.m_eBlendMode = static_cast<MaterialBlendMode>(uValue);
				break;
			case MATERIAL_PARAM_SHADING_MODEL:
				Zenith_Assert(uValue < MATERIAL_SHADING_COUNT, "Invalid shading model %u", uValue);
				xParams.m_eShadingModel = static_cast<MaterialShadingModel>(uValue);
				break;
			case MATERIAL_PARAM_TWO_SIDED:
				xParams.m_bTwoSided = (uValue != 0);
				break;
			default:
				Zenith_Assert(false, "SetParamInt called for non-int material param %u", eID);
				break;
		}
	}

	void CopyParam(Zenith_MaterialParams& xDst, const Zenith_MaterialParams& xSrc, MaterialParamID eID)
	{
		const Zenith_MaterialParamDesc& xDesc = GetParamDesc(eID);
		switch (xDesc.m_eType)
		{
			case MATERIAL_PARAM_TYPE_FLOAT:
				SetParamFloat(xDst, eID, GetParamFloat(xSrc, eID));
				break;
			case MATERIAL_PARAM_TYPE_FLOAT2:
			case MATERIAL_PARAM_TYPE_COLOR3_HDR:
			case MATERIAL_PARAM_TYPE_COLOR4:
				SetParamVector(xDst, eID, GetParamVector(xSrc, eID));
				break;
			case MATERIAL_PARAM_TYPE_BOOL:
			case MATERIAL_PARAM_TYPE_ENUM:
				SetParamInt(xDst, eID, GetParamInt(xSrc, eID));
				break;
			default:
				Zenith_Assert(false, "CopyParam: unhandled material param type %u", xDesc.m_eType);
				break;
		}
	}
}
