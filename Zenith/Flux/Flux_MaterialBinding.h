#pragma once

#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "Flux/Flux_BackendTypes.h"   // full Flux_CommandBuffer for the inline BindSRV helpers below

// ============================================================================
// Material feature flags — packed into MaterialDrawConstants::m_xFlagsParams.x
// as uint bits. MUST stay in sync with Common/DrawConstants.slang.
// ============================================================================
enum MaterialDrawFlags : u_int
{
	MATERIAL_DRAW_FLAG_UNLIT                 = 1u << 0,
	MATERIAL_DRAW_FLAG_TWO_SIDED_NORMAL_FLIP = 1u << 1,
	MATERIAL_DRAW_FLAG_POM                   = 1u << 2,
	MATERIAL_DRAW_FLAG_DETAIL_MAPS           = 1u << 3,
	MATERIAL_DRAW_FLAG_CLEARCOAT             = 1u << 4,
	MATERIAL_DRAW_FLAG_SKIN                  = 1u << 5,   // subsurface shading model
};

// ============================================================================
// Material Draw Constants (192 bytes)
// Used by StaticMeshes, AnimatedMeshes, InstancedMeshes, Translucency and the
// editor material preview. Bound through the per-frame scratch UBO
// (Zenith_Vulkan_CommandBuffer::BindDrawConstants) — NOT hardware push
// constants — so the 128-byte push-constant limit does not apply.
//
// LAYOUT RULE: the first five fields must never be reordered or resized —
// InstancedMeshes' VAT vertex path aliases m_xEmissiveParams by offset (112).
// New fields append only.
// ============================================================================
struct MaterialDrawConstants
{
	Zenith_Maths::Matrix4 m_xModelMatrix;       // 64 bytes (offset   0)
	Zenith_Maths::Vector4 m_xBaseColor;         // 16 bytes (offset  64)
	Zenith_Maths::Vector4 m_xMaterialParams;    // 16 bytes (offset  80) (metallic, roughness, alphaCutoff, occlusionStrength)
	Zenith_Maths::Vector4 m_xUVParams;          // 16 bytes (offset  96) (tilingX, tilingY, offsetX, offsetY)
	Zenith_Maths::Vector4 m_xEmissiveParams;    // 16 bytes (offset 112) (R, G, B, intensity) — VAT params for InstancedMeshes
	Zenith_Maths::Vector4 m_xMaterialParams2;   // 16 bytes (offset 128) (specular, normalStrength, clearCoatStrength, clearCoatRoughness)
	Zenith_Maths::Vector4 m_xParallaxParams;    // 16 bytes (offset 144) (heightScale, pomMinSteps, pomMaxSteps, unused)
	Zenith_Maths::Vector4 m_xDetailParams;      // 16 bytes (offset 160) (detailTilingX, detailTilingY, detailNormalStrength, detailAlbedoStrength)
	Zenith_Maths::Vector4 m_xFlagsParams;       // 16 bytes (offset 176) (MaterialDrawFlags as uint bits, unused x3)
};
static_assert(sizeof(MaterialDrawConstants) == 192, "MaterialDrawConstants must be 192 bytes (mirrored by Common/DrawConstants.slang)");

// ============================================================================
// Terrain Material Push Constants (288 bytes)
// Holds properties for 4 splatmap-blended materials + terrain params
// Uploaded via scratch buffer UBO (not hardware push constants)
// ============================================================================
struct TerrainMaterialDrawConstants
{
	// Per-material arrays (4 materials, 256 bytes total)
	Zenith_Maths::Vector4 m_axBaseColors[4];       // 64 bytes
	Zenith_Maths::Vector4 m_axUVParams[4];         // 64 bytes (tilingX, tilingY, offsetX, offsetY)
	Zenith_Maths::Vector4 m_axMaterialParams[4];   // 64 bytes (metallic, roughness, occlusionStrength, specular)
	Zenith_Maths::Vector4 m_axEmissiveParams[4];   // 64 bytes (R, G, B, intensity)

	// Terrain params (32 bytes)
	Zenith_Maths::Vector4 m_xTerrainParams;        // 16 bytes (originX, originZ, sizeX, sizeZ)
	Zenith_Maths::Vector4 m_xTerrainParams2;       // 16 bytes (materialCount as uint bits, debugMode as uint bits, 0, 0)
};
static_assert(sizeof(TerrainMaterialDrawConstants) == 288, "TerrainMaterialDrawConstants must be 288 bytes");

// ============================================================================
// Helper Functions
// ============================================================================

// Derive the per-draw feature flag bits from a resolved parameter block +
// texture set. POM/detail only light up when the matching textures are bound,
// so unset slots cost a uniform branch and nothing else.
inline u_int BuildMaterialDrawFlags(const Zenith_MaterialResolved& xResolved)
{
	const Zenith_MaterialParams& xParams = xResolved.m_xParams;
	u_int uFlags = 0;

	if (xParams.m_eShadingModel == MATERIAL_SHADING_UNLIT)
	{
		uFlags |= MATERIAL_DRAW_FLAG_UNLIT;
	}
	else if (xParams.m_eShadingModel == MATERIAL_SHADING_SUBSURFACE)
	{
		uFlags |= MATERIAL_DRAW_FLAG_SKIN;
	}
	if (xParams.m_bTwoSided)
	{
		uFlags |= MATERIAL_DRAW_FLAG_TWO_SIDED_NORMAL_FLIP;
	}
	if (xParams.m_fHeightScale > 0.0f && static_cast<bool>(*xResolved.m_apxTextures[MATERIAL_TEXTURE_HEIGHT]))
	{
		uFlags |= MATERIAL_DRAW_FLAG_POM;
	}
	if (static_cast<bool>(*xResolved.m_apxTextures[MATERIAL_TEXTURE_DETAIL_ALBEDO]) ||
		static_cast<bool>(*xResolved.m_apxTextures[MATERIAL_TEXTURE_DETAIL_NORMAL]))
	{
		uFlags |= MATERIAL_DRAW_FLAG_DETAIL_MAPS;
	}
	if (xParams.m_fClearCoatStrength > 0.0f)
	{
		uFlags |= MATERIAL_DRAW_FLAG_CLEARCOAT;
	}

	return uFlags;
}

inline void BuildMaterialDrawConstants(
	MaterialDrawConstants& xOut,
	const Zenith_Maths::Matrix4& xModelMatrix,
	Zenith_MaterialAsset* pxMaterial)
{
	xOut.m_xModelMatrix = xModelMatrix;

	if (pxMaterial)
	{
		const Zenith_MaterialResolved& xResolved = pxMaterial->GetResolved();
		const Zenith_MaterialParams& xParams = xResolved.m_xParams;

		// Opaque never alpha-tests: cutoff 0 disables the shader's discard.
		// Masked uses the authored cutoff. Translucent/Additive keep cutoff 0
		// (blending handles their alpha; the forward pass reads alpha direct).
		const float fAlphaCutoff = (xParams.m_eBlendMode == MATERIAL_BLEND_MASKED) ? xParams.m_fAlphaCutoff : 0.0f;

		xOut.m_xBaseColor = xParams.m_xBaseColor;
		xOut.m_xMaterialParams = Zenith_Maths::Vector4(
			xParams.m_fMetallic,
			xParams.m_fRoughness,
			fAlphaCutoff,
			xParams.m_fOcclusionStrength
		);
		xOut.m_xUVParams = Zenith_Maths::Vector4(
			xParams.m_xUVTiling.x, xParams.m_xUVTiling.y,
			xParams.m_xUVOffset.x, xParams.m_xUVOffset.y
		);
		xOut.m_xEmissiveParams = Zenith_Maths::Vector4(
			xParams.m_xEmissiveColor.x, xParams.m_xEmissiveColor.y, xParams.m_xEmissiveColor.z,
			xParams.m_fEmissiveIntensity
		);
		xOut.m_xMaterialParams2 = Zenith_Maths::Vector4(
			xParams.m_fSpecular,
			xParams.m_fNormalStrength,
			xParams.m_fClearCoatStrength,
			xParams.m_fClearCoatRoughness
		);
		xOut.m_xParallaxParams = Zenith_Maths::Vector4(
			xParams.m_fHeightScale,
			xParams.m_fPOMMinSteps,
			xParams.m_fPOMMaxSteps,
			0.0f
		);
		xOut.m_xDetailParams = Zenith_Maths::Vector4(
			xParams.m_xDetailTiling.x, xParams.m_xDetailTiling.y,
			xParams.m_fDetailNormalStrength,
			xParams.m_fDetailAlbedoStrength
		);

		const u_int uFlags = BuildMaterialDrawFlags(xResolved);
		Zenith_Maths::Vector4 xFlags(0.0f);
		memcpy(&xFlags.x, &uFlags, sizeof(u_int));
		xOut.m_xFlagsParams = xFlags;
	}
	else
	{
		// Default white material (UE-style blank canvas: grey plastic).
		xOut.m_xBaseColor = Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
		xOut.m_xMaterialParams = Zenith_Maths::Vector4(0.0f, 0.5f, 0.0f, 1.0f);
		xOut.m_xUVParams = Zenith_Maths::Vector4(1.0f, 1.0f, 0.0f, 0.0f);
		xOut.m_xEmissiveParams = Zenith_Maths::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
		xOut.m_xMaterialParams2 = Zenith_Maths::Vector4(0.5f, 1.0f, 0.0f, 0.1f);
		xOut.m_xParallaxParams = Zenith_Maths::Vector4(0.0f, 8.0f, 32.0f, 0.0f);
		xOut.m_xDetailParams = Zenith_Maths::Vector4(4.0f, 4.0f, 1.0f, 1.0f);
		xOut.m_xFlagsParams = Zenith_Maths::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
	}
}

inline void BuildTerrainMaterialDrawConstants(
	TerrainMaterialDrawConstants& xOut,
	Zenith_MaterialAsset* const* ppxMaterials,
	u_int uMaterialCount,
	u_int uDebugMode,
	float fOriginX, float fOriginZ,
	float fSizeX, float fSizeZ)
{
	for (u_int u = 0; u < 4; u++)
	{
		Zenith_MaterialAsset* pxMat = (u < uMaterialCount) ? ppxMaterials[u] : nullptr;
		if (pxMat)
		{
			const Zenith_MaterialParams& xParams = pxMat->GetResolved().m_xParams;
			xOut.m_axBaseColors[u] = xParams.m_xBaseColor;
			xOut.m_axUVParams[u] = Zenith_Maths::Vector4(
				xParams.m_xUVTiling.x, xParams.m_xUVTiling.y,
				xParams.m_xUVOffset.x, xParams.m_xUVOffset.y);
			xOut.m_axMaterialParams[u] = Zenith_Maths::Vector4(
				xParams.m_fMetallic, xParams.m_fRoughness,
				xParams.m_fOcclusionStrength, xParams.m_fSpecular);
			xOut.m_axEmissiveParams[u] = Zenith_Maths::Vector4(
				xParams.m_xEmissiveColor.x, xParams.m_xEmissiveColor.y, xParams.m_xEmissiveColor.z,
				xParams.m_fEmissiveIntensity);
		}
		else
		{
			xOut.m_axBaseColors[u] = Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
			xOut.m_axUVParams[u] = Zenith_Maths::Vector4(1.0f, 1.0f, 0.0f, 0.0f);
			xOut.m_axMaterialParams[u] = Zenith_Maths::Vector4(0.0f, 0.5f, 1.0f, 0.5f);
			xOut.m_axEmissiveParams[u] = Zenith_Maths::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
		}
	}

	// Terrain params
	xOut.m_xTerrainParams = Zenith_Maths::Vector4(fOriginX, fOriginZ, fSizeX, fSizeZ);

	// Pack materialCount and debugMode as uint bits
	u_int uMatCount = uMaterialCount;
	Zenith_Maths::Vector4 xParams2(0.0f);
	memcpy(&xParams2.x, &uMatCount, sizeof(u_int));
	memcpy(&xParams2.y, &uDebugMode, sizeof(u_int));
	xOut.m_xTerrainParams2 = xParams2;
}

// Canonical shader binding name per material texture slot — order matches
// MaterialTextureSlot in Zenith_MaterialParamTable.h and the [[vk::binding]]
// declarations in the mesh shaders.
inline const char* GetMaterialTextureBindingName(u_int uSlot)
{
	static const char* const ls_aszNames[MATERIAL_TEXTURE_SLOT_COUNT] =
	{
		"g_xBaseColorTex",
		"g_xNormalTex",
		"g_xRoughnessMetallicTex",
		"g_xOcclusionTex",
		"g_xEmissiveTex",
		"g_xHeightTex",
		"g_xDetailAlbedoTex",
		"g_xDetailNormalTex",
		"g_xDetailMaskTex",
	};
	Zenith_Assert(uSlot < MATERIAL_TEXTURE_SLOT_COUNT, "Invalid material texture slot %u", uSlot);
	return ls_aszNames[uSlot];
}

// Bind all 9 material texture slots (instance-aware: resolves through the
// parent chain, falling back to the pinned per-slot defaults).
inline void BindMaterialTextures(
	Flux_CommandBuffer& xCmdBuf,
	Zenith_MaterialAsset* pxMaterial,
	uint32_t uStartBinding = 0)
{
	for (u_int u = 0; u < MATERIAL_TEXTURE_SLOT_COUNT; u++)
	{
		Zenith_TextureAsset* pxTexture = pxMaterial->GetResolvedTexture(static_cast<MaterialTextureSlot>(u));
		xCmdBuf.BindSRV(&pxTexture->m_xSRV, uStartBinding + u);
	}
}

// Bind the 5 core terrain material textures (base colour, normal, RM,
// occlusion, emissive). Terrain's splat path does not consume the
// height/detail slots — 4 materials x 9 slots would exhaust the binding
// space for no visual gain (terrain has its own splat detail).
inline void BindTerrainMaterialTextures(
	Flux_CommandBuffer& xCmdBuf,
	Zenith_MaterialAsset* pxMaterial,
	uint32_t uStartBinding = 0)
{
	static const MaterialTextureSlot aeTerrainSlots[] =
	{
		MATERIAL_TEXTURE_BASE_COLOR,
		MATERIAL_TEXTURE_NORMAL,
		MATERIAL_TEXTURE_ROUGHNESS_METALLIC,
		MATERIAL_TEXTURE_OCCLUSION,
		MATERIAL_TEXTURE_EMISSIVE,
	};
	for (u_int u = 0; u < sizeof(aeTerrainSlots) / sizeof(aeTerrainSlots[0]); u++)
	{
		Zenith_TextureAsset* pxTexture = pxMaterial->GetResolvedTexture(aeTerrainSlots[u]);
		xCmdBuf.BindSRV(&pxTexture->m_xSRV, uStartBinding + u);
	}
}
