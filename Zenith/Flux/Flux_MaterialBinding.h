#pragma once

#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "Flux_CommandList.h"
#include "Maths/Zenith_Maths.h"

// ============================================================================
// Material Push Constants (128 bytes - Vulkan minimum guarantee)
// Used by StaticMeshes and AnimatedMeshes
// ============================================================================
struct MaterialPushConstants
{
	Zenith_Maths::Matrix4 m_xModelMatrix;       // 64 bytes
	Zenith_Maths::Vector4 m_xBaseColor;         // 16 bytes
	Zenith_Maths::Vector4 m_xMaterialParams;    // 16 bytes (metallic, roughness, alphaCutoff, occlusionStrength)
	Zenith_Maths::Vector4 m_xUVParams;          // 16 bytes (tilingX, tilingY, offsetX, offsetY)
	Zenith_Maths::Vector4 m_xEmissiveParams;    // 16 bytes (R, G, B, intensity)
};
static_assert(sizeof(MaterialPushConstants) == 128, "MaterialPushConstants must be 128 bytes");

// ============================================================================
// Terrain Material Push Constants (288 bytes)
// Holds properties for 4 splatmap-blended materials + terrain params
// Uploaded via scratch buffer UBO (not hardware push constants)
// ============================================================================
struct TerrainMaterialPushConstants
{
	// Per-material arrays (4 materials, 256 bytes total)
	Zenith_Maths::Vector4 m_axBaseColors[4];       // 64 bytes
	Zenith_Maths::Vector4 m_axUVParams[4];         // 64 bytes (tilingX, tilingY, offsetX, offsetY)
	Zenith_Maths::Vector4 m_axMaterialParams[4];   // 64 bytes (metallic, roughness, occlusionStrength, 0)
	Zenith_Maths::Vector4 m_axEmissiveParams[4];   // 64 bytes (R, G, B, intensity)

	// Terrain params (32 bytes)
	Zenith_Maths::Vector4 m_xTerrainParams;        // 16 bytes (originX, originZ, sizeX, sizeZ)
	Zenith_Maths::Vector4 m_xTerrainParams2;       // 16 bytes (materialCount as uint bits, debugMode as uint bits, 0, 0)
};
static_assert(sizeof(TerrainMaterialPushConstants) == 288, "TerrainMaterialPushConstants must be 288 bytes");

// ============================================================================
// Helper Functions
// ============================================================================

inline void BuildMaterialPushConstants(
	MaterialPushConstants& xOut,
	const Zenith_Maths::Matrix4& xModelMatrix,
	const Zenith_MaterialAsset* pxMaterial)
{
	xOut.m_xModelMatrix = xModelMatrix;

	if (pxMaterial)
	{
		xOut.m_xBaseColor = pxMaterial->GetBaseColor();
		xOut.m_xMaterialParams = Zenith_Maths::Vector4(
			pxMaterial->GetMetallic(),
			pxMaterial->GetRoughness(),
			pxMaterial->GetAlphaCutoff(),
			pxMaterial->GetOcclusionStrength()
		);

		const Zenith_Maths::Vector2& xTiling = pxMaterial->GetUVTiling();
		const Zenith_Maths::Vector2& xOffset = pxMaterial->GetUVOffset();
		xOut.m_xUVParams = Zenith_Maths::Vector4(
			xTiling.x, xTiling.y, xOffset.x, xOffset.y
		);

		const Zenith_Maths::Vector3& xEmissive = pxMaterial->GetEmissiveColor();
		xOut.m_xEmissiveParams = Zenith_Maths::Vector4(
			xEmissive.x, xEmissive.y, xEmissive.z,
			pxMaterial->GetEmissiveIntensity()
		);
	}
	else
	{
		// Default white material
		xOut.m_xBaseColor = Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
		xOut.m_xMaterialParams = Zenith_Maths::Vector4(0.0f, 0.5f, 0.5f, 1.0f);
		xOut.m_xUVParams = Zenith_Maths::Vector4(1.0f, 1.0f, 0.0f, 0.0f);
		xOut.m_xEmissiveParams = Zenith_Maths::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
	}
}

inline void BuildTerrainMaterialPushConstants(
	TerrainMaterialPushConstants& xOut,
	const Zenith_MaterialAsset* const* ppxMaterials,
	u_int uMaterialCount,
	u_int uDebugMode,
	float fOriginX, float fOriginZ,
	float fSizeX, float fSizeZ)
{
	for (u_int u = 0; u < 4; u++)
	{
		const Zenith_MaterialAsset* pxMat = (u < uMaterialCount) ? ppxMaterials[u] : nullptr;
		if (pxMat)
		{
			xOut.m_axBaseColors[u] = pxMat->GetBaseColor();
			const Zenith_Maths::Vector2& xTiling = pxMat->GetUVTiling();
			const Zenith_Maths::Vector2& xOffset = pxMat->GetUVOffset();
			xOut.m_axUVParams[u] = Zenith_Maths::Vector4(xTiling.x, xTiling.y, xOffset.x, xOffset.y);
			xOut.m_axMaterialParams[u] = Zenith_Maths::Vector4(
				pxMat->GetMetallic(), pxMat->GetRoughness(),
				pxMat->GetOcclusionStrength(), 0.0f);
			const Zenith_Maths::Vector3& xEmissive = pxMat->GetEmissiveColor();
			xOut.m_axEmissiveParams[u] = Zenith_Maths::Vector4(
				xEmissive.x, xEmissive.y, xEmissive.z,
				pxMat->GetEmissiveIntensity());
		}
		else
		{
			xOut.m_axBaseColors[u] = Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
			xOut.m_axUVParams[u] = Zenith_Maths::Vector4(1.0f, 1.0f, 0.0f, 0.0f);
			xOut.m_axMaterialParams[u] = Zenith_Maths::Vector4(0.0f, 0.5f, 1.0f, 0.0f);
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

// Bind 5 material textures (diffuse, normal, RM, occlusion, emissive)
inline void BindMaterialTextures(
	Flux_CommandList& xCommandList,
	Zenith_MaterialAsset* pxMaterial,
	uint32_t uStartBinding = 0)
{
	xCommandList.AddCommand<Flux_CommandBindSRV>(
		&pxMaterial->GetDiffuseTexture()->m_xSRV, uStartBinding + 0);
	xCommandList.AddCommand<Flux_CommandBindSRV>(
		&pxMaterial->GetNormalTexture()->m_xSRV, uStartBinding + 1);
	xCommandList.AddCommand<Flux_CommandBindSRV>(
		&pxMaterial->GetRoughnessMetallicTexture()->m_xSRV, uStartBinding + 2);
	xCommandList.AddCommand<Flux_CommandBindSRV>(
		&pxMaterial->GetOcclusionTexture()->m_xSRV, uStartBinding + 3);
	xCommandList.AddCommand<Flux_CommandBindSRV>(
		&pxMaterial->GetEmissiveTexture()->m_xSRV, uStartBinding + 4);
}

// Bind 5 terrain material textures (diffuse, normal, RM, occlusion, emissive)
// Uses full material system - same as standard materials
inline void BindTerrainMaterialTextures(
	Flux_CommandList& xCommandList,
	Zenith_MaterialAsset* pxMaterial,
	uint32_t uStartBinding = 0)
{
	xCommandList.AddCommand<Flux_CommandBindSRV>(
		&pxMaterial->GetDiffuseTexture()->m_xSRV, uStartBinding + 0);
	xCommandList.AddCommand<Flux_CommandBindSRV>(
		&pxMaterial->GetNormalTexture()->m_xSRV, uStartBinding + 1);
	xCommandList.AddCommand<Flux_CommandBindSRV>(
		&pxMaterial->GetRoughnessMetallicTexture()->m_xSRV, uStartBinding + 2);
	xCommandList.AddCommand<Flux_CommandBindSRV>(
		&pxMaterial->GetOcclusionTexture()->m_xSRV, uStartBinding + 3);
	xCommandList.AddCommand<Flux_CommandBindSRV>(
		&pxMaterial->GetEmissiveTexture()->m_xSRV, uStartBinding + 4);
}
