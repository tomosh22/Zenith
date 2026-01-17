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
// Terrain Material Push Constants (128 bytes)
// Holds properties for 2 blended materials - uses full material system
// ============================================================================
struct TerrainMaterialPushConstants
{
	// Material 0 properties (64 bytes)
	Zenith_Maths::Vector4 m_xBaseColor0;        // 16 bytes
	Zenith_Maths::Vector4 m_xUVParams0;         // 16 bytes (tilingX, tilingY, offsetX, offsetY)
	Zenith_Maths::Vector4 m_xMaterialParams0;   // 16 bytes (metallic, roughness, occlusionStrength, visualizeLOD as float)
	Zenith_Maths::Vector4 m_xEmissiveParams0;   // 16 bytes (R, G, B, intensity)

	// Material 1 properties (64 bytes)
	Zenith_Maths::Vector4 m_xBaseColor1;        // 16 bytes
	Zenith_Maths::Vector4 m_xUVParams1;         // 16 bytes (tilingX, tilingY, offsetX, offsetY)
	Zenith_Maths::Vector4 m_xMaterialParams1;   // 16 bytes (metallic, roughness, occlusionStrength, unused)
	Zenith_Maths::Vector4 m_xEmissiveParams1;   // 16 bytes (R, G, B, intensity)
};
static_assert(sizeof(TerrainMaterialPushConstants) == 128, "TerrainMaterialPushConstants must be 128 bytes");

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
	const Zenith_MaterialAsset* pxMaterial0,
	const Zenith_MaterialAsset* pxMaterial1,
	bool bVisualizeLOD)
{
	auto BuildSingleMaterial = [](
		Zenith_Maths::Vector4& xBaseColorOut,
		Zenith_Maths::Vector4& xUVParamsOut,
		Zenith_Maths::Vector4& xMaterialParamsOut,
		Zenith_Maths::Vector4& xEmissiveParamsOut,
		const Zenith_MaterialAsset* pxMat,
		float fExtraParam = 0.0f)  // For visualizeLOD or other per-material flags
	{
		if (pxMat)
		{
			xBaseColorOut = pxMat->GetBaseColor();
			const Zenith_Maths::Vector2& xTiling = pxMat->GetUVTiling();
			const Zenith_Maths::Vector2& xOffset = pxMat->GetUVOffset();
			xUVParamsOut = Zenith_Maths::Vector4(xTiling.x, xTiling.y, xOffset.x, xOffset.y);
			xMaterialParamsOut = Zenith_Maths::Vector4(
				pxMat->GetMetallic(), pxMat->GetRoughness(),
				pxMat->GetOcclusionStrength(), fExtraParam);
			const Zenith_Maths::Vector3& xEmissive = pxMat->GetEmissiveColor();
			xEmissiveParamsOut = Zenith_Maths::Vector4(
				xEmissive.x, xEmissive.y, xEmissive.z,
				pxMat->GetEmissiveIntensity());
		}
		else
		{
			xBaseColorOut = Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
			xUVParamsOut = Zenith_Maths::Vector4(1.0f, 1.0f, 0.0f, 0.0f);
			xMaterialParamsOut = Zenith_Maths::Vector4(0.0f, 0.5f, 1.0f, fExtraParam);
			xEmissiveParamsOut = Zenith_Maths::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
		}
	};

	// Pack visualizeLOD flag into material 0's w component
	float fVisualizeLOD = bVisualizeLOD ? 1.0f : 0.0f;
	BuildSingleMaterial(xOut.m_xBaseColor0, xOut.m_xUVParams0, xOut.m_xMaterialParams0, xOut.m_xEmissiveParams0, pxMaterial0, fVisualizeLOD);
	BuildSingleMaterial(xOut.m_xBaseColor1, xOut.m_xUVParams1, xOut.m_xMaterialParams1, xOut.m_xEmissiveParams1, pxMaterial1, 0.0f);
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
