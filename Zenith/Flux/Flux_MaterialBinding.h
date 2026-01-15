#pragma once

#include "Flux_MaterialAsset.h"
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
// Holds properties for 2 blended materials
// ============================================================================
struct TerrainMaterialPushConstants
{
	// Material 0 properties
	Zenith_Maths::Vector4 m_xBaseColor0;        // 16 bytes
	Zenith_Maths::Vector4 m_xUVParams0;         // 16 bytes (tilingX, tilingY, offsetX, offsetY)
	Zenith_Maths::Vector4 m_xMaterialParams0;   // 16 bytes (metallic, roughness, occlusionStrength, unused)

	// Material 1 properties
	Zenith_Maths::Vector4 m_xBaseColor1;        // 16 bytes
	Zenith_Maths::Vector4 m_xUVParams1;         // 16 bytes (tilingX, tilingY, offsetX, offsetY)
	Zenith_Maths::Vector4 m_xMaterialParams1;   // 16 bytes (metallic, roughness, occlusionStrength, unused)

	// Debug/control
	uint32_t m_uVisualizeLOD;                   // 4 bytes
	float m_fPad[7];                            // 28 bytes padding
};
static_assert(sizeof(TerrainMaterialPushConstants) == 128, "TerrainMaterialPushConstants must be 128 bytes");

// ============================================================================
// Helper Functions
// ============================================================================

inline void BuildMaterialPushConstants(
	MaterialPushConstants& xOut,
	const Zenith_Maths::Matrix4& xModelMatrix,
	const Flux_MaterialAsset* pxMaterial)
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
	const Flux_MaterialAsset* pxMaterial0,
	const Flux_MaterialAsset* pxMaterial1,
	bool bVisualizeLOD)
{
	auto BuildSingleMaterial = [](
		Zenith_Maths::Vector4& xBaseColorOut,
		Zenith_Maths::Vector4& xUVParamsOut,
		Zenith_Maths::Vector4& xMaterialParamsOut,
		const Flux_MaterialAsset* pxMat)
	{
		if (pxMat)
		{
			xBaseColorOut = pxMat->GetBaseColor();
			const Zenith_Maths::Vector2& xTiling = pxMat->GetUVTiling();
			const Zenith_Maths::Vector2& xOffset = pxMat->GetUVOffset();
			xUVParamsOut = Zenith_Maths::Vector4(xTiling.x, xTiling.y, xOffset.x, xOffset.y);
			xMaterialParamsOut = Zenith_Maths::Vector4(
				pxMat->GetMetallic(), pxMat->GetRoughness(),
				pxMat->GetOcclusionStrength(), 0.0f);
		}
		else
		{
			xBaseColorOut = Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
			xUVParamsOut = Zenith_Maths::Vector4(1.0f, 1.0f, 0.0f, 0.0f);
			xMaterialParamsOut = Zenith_Maths::Vector4(0.0f, 0.5f, 1.0f, 0.0f);
		}
	};

	BuildSingleMaterial(xOut.m_xBaseColor0, xOut.m_xUVParams0, xOut.m_xMaterialParams0, pxMaterial0);
	BuildSingleMaterial(xOut.m_xBaseColor1, xOut.m_xUVParams1, xOut.m_xMaterialParams1, pxMaterial1);
	xOut.m_uVisualizeLOD = bVisualizeLOD ? 1 : 0;
	memset(xOut.m_fPad, 0, sizeof(xOut.m_fPad));
}

// Bind 5 material textures (diffuse, normal, RM, occlusion, emissive)
inline void BindMaterialTextures(
	Flux_CommandList& xCommandList,
	Flux_MaterialAsset* pxMaterial,
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

// Bind 3 terrain material textures (diffuse, normal, RM) - no occlusion/emissive
inline void BindTerrainMaterialTextures(
	Flux_CommandList& xCommandList,
	Flux_MaterialAsset* pxMaterial,
	uint32_t uStartBinding = 0)
{
	xCommandList.AddCommand<Flux_CommandBindSRV>(
		&pxMaterial->GetDiffuseTexture()->m_xSRV, uStartBinding + 0);
	xCommandList.AddCommand<Flux_CommandBindSRV>(
		&pxMaterial->GetNormalTexture()->m_xSRV, uStartBinding + 1);
	xCommandList.AddCommand<Flux_CommandBindSRV>(
		&pxMaterial->GetRoughnessMetallicTexture()->m_xSRV, uStartBinding + 2);
}
