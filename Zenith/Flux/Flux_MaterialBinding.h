#pragma once

#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_TextureAsset.h"

// ============================================================================
// Mesh Draw Constants (96 bytes)
// Per-draw constants for the shared mesh material path (UnifiedMesh,
// Translucency, MaterialPreview). Carries only
// the model matrix + the GPU material-table index + the InstancedMeshes VAT
// params. Material SCALARS and the 9 texture slots moved to the per-material GPU
// record (Flux_MaterialGPU / g_axMaterials) — a draw selects its material by index.
//
// Bound through the per-frame scratch UBO (BindDrawConstants), NOT hardware push
// constants. LAYOUT RULE: byte-identical to DrawConstantsLayout in
// Common/DrawConstants.slang. VAT rides m_xVATParams since the backend allows only
// ONE scratch/draw-constants buffer per descriptor set.
// ============================================================================
struct MeshDrawConstants
{
	Zenith_Maths::Matrix4 m_xModelMatrix;       // 64 bytes (offset  0)
	u_int                 m_uMaterialIndex;     //  4 bytes (offset 64) index into g_axMaterials
	u_int                 m_uShadowCascade;     //  4 bytes (offset 68) CSM cascade index (shadow caster passes; 0 otherwise)
	u_int                 m_uPad1;              //  4 bytes (offset 72)
	u_int                 m_uPad2;              //  4 bytes (offset 76)
	Zenith_Maths::Vector4 m_xVATParams;         // 16 bytes (offset 80) InstancedMeshes VAT (texW, texH, vat-enabled, unused); 0 for non-instanced
};
static_assert(sizeof(MeshDrawConstants) == 96, "MeshDrawConstants must be 96 bytes (mirrored by DrawConstantsLayout in Common/DrawConstants.slang)");

// Build the per-draw constants. uMaterialIndex comes from
// Flux_MaterialTable::GetOrCreateIndex (assigned on the main thread at gather). VAT
// params default to zero (non-instanced); the instanced path overwrites
// m_xVATParams after the call.
inline void BuildMeshDrawConstants(MeshDrawConstants& xOut, const Zenith_Maths::Matrix4& xModelMatrix, u_int uMaterialIndex)
{
	xOut.m_xModelMatrix   = xModelMatrix;
	xOut.m_uMaterialIndex = uMaterialIndex;
	xOut.m_uShadowCascade = 0u;
	xOut.m_uPad1 = 0u;
	xOut.m_uPad2 = 0u;
	xOut.m_xVATParams = Zenith_Maths::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
}

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

	// Phase 4c: GPU material-table index per splat slot. The shader loads
	// g_axMaterials[idx] for each of the 4 slots and samples its bindless texture
	// indices from g_axTextures[]. Draw-uniform (one set per terrain draw) →
	// no NonUniformResourceIndex. Mirrors uint4 g_xMaterialTableIndices in the shader.
	u_int                 m_auMaterialTableIndices[4]; // 16 bytes
};
static_assert(sizeof(TerrainMaterialDrawConstants) == 304, "TerrainMaterialDrawConstants must be 304 bytes (288 + uint4 material-table indices)");

// ============================================================================
// Helper Functions
// ============================================================================

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

	// Default to the reserved table slot 0; the caller fills the real per-slot
	// material-table indices (resolved on the main thread in PreRenderUpdate).
	for (u_int u = 0; u < 4; u++) xOut.m_auMaterialTableIndices[u] = 0u;
}
