#pragma once

#include "Maths/Zenith_Maths.h"
#include "AssetHandling/Zenith_MaterialAsset.h"      // Zenith_MaterialResolved / Zenith_MaterialParams
#include "AssetHandling/Zenith_MaterialParamTable.h"  // MaterialTextureSlot / MATERIAL_TEXTURE_SLOT_COUNT
#include <cstring>                                     // memcpy

// ============================================================================
// Material feature flags — packed into Flux_MaterialGPU::m_xFlagsParams.x as uint
// bits. MUST stay in sync with MATERIAL_FLAG_* in Common/DrawConstants.slang.
// (Formerly MaterialDrawFlags in Flux_MaterialBinding.h, moved here with the
// per-draw → per-material record split.)
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
// Flux_MaterialGPU (176 bytes) — the per-material GPU record.
//
// One record per material lives in the GLOBAL set's g_axMaterials structured
// buffer; a draw selects its record via MeshDrawConstants::m_uMaterialIndex. The
// 9 texture slots are bindless indices into the BINDLESS set's g_axTextures[]
// table (set 2). Index 0 of g_axMaterials is the engine default material.
//
// LAYOUT RULE: must stay byte-identical to `MaterialGPU` in
// Common/MaterialTable.slang. All members are 16-byte aligned (float4 / the
// uint[12] tail) so the std430 structured-buffer layout matches with no scalar
// packing surprises.
// ============================================================================
struct Flux_MaterialGPU
{
	Zenith_Maths::Vector4 m_xBaseColor;        // offset   0 — RGBA tint
	Zenith_Maths::Vector4 m_xMaterialParams;   // offset  16 — (metallic, roughness, alphaCutoff, occlusionStrength)
	Zenith_Maths::Vector4 m_xUVParams;         // offset  32 — (tilingX, tilingY, offsetX, offsetY)
	Zenith_Maths::Vector4 m_xEmissiveParams;   // offset  48 — (R, G, B, intensity)
	Zenith_Maths::Vector4 m_xMaterialParams2;  // offset  64 — (specular, normalStrength, clearCoatStrength, clearCoatRoughness)
	Zenith_Maths::Vector4 m_xParallaxParams;   // offset  80 — (heightScale, pomMinSteps, pomMaxSteps, unused)
	Zenith_Maths::Vector4 m_xDetailParams;     // offset  96 — (detailTilingX, detailTilingY, detailNormalStrength, detailAlbedoStrength)
	Zenith_Maths::Vector4 m_xFlagsParams;      // offset 112 — (MaterialDrawFlags as uint bits in .x, unused x3)
	u_int                 m_auTexIdx[12];      // offset 128 — 9 bindless indices (MaterialTextureSlot order) + 3 pad
};
static_assert(sizeof(Flux_MaterialGPU) == 176, "Flux_MaterialGPU must be 176 bytes (mirrors MaterialGPU in Common/MaterialTable.slang)");

// ----------------------------------------------------------------------------
// Derive the per-material feature flag bits from a resolved parameter block +
// texture set. POM/detail only light up when the matching textures are present.
// (Same logic the old MaterialDrawConstants path used — moved here verbatim.)
// ----------------------------------------------------------------------------
inline u_int Flux_BuildMaterialDrawFlags(const Zenith_MaterialResolved& xResolved)
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

// ----------------------------------------------------------------------------
// Pack a resolved material + its 9 resolved bindless texture indices (in
// MaterialTextureSlot order, length MATERIAL_TEXTURE_SLOT_COUNT) into the GPU
// record. Pure (no GPU access) → unit-testable. Mirrors the param packing the
// old BuildMaterialDrawConstants did (Opaque/Translucent never alpha-test, so
// cutoff 0; Masked uses the authored cutoff).
// ----------------------------------------------------------------------------
inline void Flux_PackMaterialGPU(Flux_MaterialGPU& xOut, const Zenith_MaterialResolved& xResolved,
	const u_int* puTexIdx)
{
	const Zenith_MaterialParams& xParams = xResolved.m_xParams;

	const float fAlphaCutoff = (xParams.m_eBlendMode == MATERIAL_BLEND_MASKED) ? xParams.m_fAlphaCutoff : 0.0f;

	xOut.m_xBaseColor = xParams.m_xBaseColor;
	xOut.m_xMaterialParams = Zenith_Maths::Vector4(
		xParams.m_fMetallic, xParams.m_fRoughness, fAlphaCutoff, xParams.m_fOcclusionStrength);
	xOut.m_xUVParams = Zenith_Maths::Vector4(
		xParams.m_xUVTiling.x, xParams.m_xUVTiling.y, xParams.m_xUVOffset.x, xParams.m_xUVOffset.y);
	xOut.m_xEmissiveParams = Zenith_Maths::Vector4(
		xParams.m_xEmissiveColor.x, xParams.m_xEmissiveColor.y, xParams.m_xEmissiveColor.z, xParams.m_fEmissiveIntensity);
	xOut.m_xMaterialParams2 = Zenith_Maths::Vector4(
		xParams.m_fSpecular, xParams.m_fNormalStrength, xParams.m_fClearCoatStrength, xParams.m_fClearCoatRoughness);
	xOut.m_xParallaxParams = Zenith_Maths::Vector4(
		xParams.m_fHeightScale, xParams.m_fPOMMinSteps, xParams.m_fPOMMaxSteps, 0.0f);
	xOut.m_xDetailParams = Zenith_Maths::Vector4(
		xParams.m_xDetailTiling.x, xParams.m_xDetailTiling.y, xParams.m_fDetailNormalStrength, xParams.m_fDetailAlbedoStrength);

	const u_int uFlags = Flux_BuildMaterialDrawFlags(xResolved);
	Zenith_Maths::Vector4 xFlags(0.0f);
	memcpy(&xFlags.x, &uFlags, sizeof(u_int));
	xOut.m_xFlagsParams = xFlags;

	for (u_int u = 0; u < MATERIAL_TEXTURE_SLOT_COUNT; u++)
	{
		xOut.m_auTexIdx[u] = puTexIdx[u];
	}
	for (u_int u = MATERIAL_TEXTURE_SLOT_COUNT; u < 12; u++)
	{
		xOut.m_auTexIdx[u] = 0u;
	}
}
