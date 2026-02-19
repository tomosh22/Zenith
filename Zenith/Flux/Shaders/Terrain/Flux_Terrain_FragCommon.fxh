#include "../Common.fxh"
#ifndef SHADOWS
#include "../GBufferCommon.fxh"
#include "../MaterialCommon.fxh"
#endif

// ========== Fragment Inputs ==========
// Varyings from vertex shader - only declared when not doing shadow pass
#ifndef SHADOWS
layout(location = 0) in vec2 a_xUV;
layout(location = 1) in vec3 a_xNormal;
layout(location = 2) in vec3 a_xWorldPos;
layout(location = 3) in vec3 a_xTangent;
layout(location = 4) flat in uint a_uLODLevel;  // LOD level from vertex shader
layout(location = 5) in float a_fBitangentSign;  // Sign for bitangent reconstruction
#endif

// ========== Terrain Material Constants (per-draw, set 1 binding 0) ==========
// Matches C++ TerrainMaterialPushConstants struct (288 bytes)
layout(std140, set = 1, binding = 0) uniform TerrainMaterialConstants {
	vec4 g_axBaseColors[4];       // 64 bytes
	vec4 g_axUVParams[4];         // 64 bytes (tilingX, tilingY, offsetX, offsetY)
	vec4 g_axMaterialParams[4];   // 64 bytes (metallic, roughness, occlusionStrength, 0)
	vec4 g_axEmissiveParams[4];   // 64 bytes (R, G, B, intensity)
	vec4 g_xTerrainParams;        // 16 bytes (originX, originZ, sizeX, sizeZ)
	vec4 g_xTerrainParams2;       // 16 bytes (materialCount bits, debugMode bits, 0, 0)
};

// ========== LOD Level Buffer (per-draw, set 1 binding 1) ==========
#ifndef SHADOWS
layout(std430, set = 1, binding = 1) buffer LODLevelBuffer {
	uint g_auLODLevels[];
};
#endif

// ========== Splatmap Texture (set 1 binding 2) ==========
#ifndef SHADOWS
layout(set = 1, binding = 2) uniform sampler2D g_xSplatmap;
#endif

// ========== Material Textures (per-draw, set 1 bindings 3-22) ==========
// 4 materials x 5 textures each (diffuse, normal, RM, occlusion, emissive)
#ifndef SHADOWS
// Material 0 textures (bindings 3-7)
layout(set = 1, binding = 3) uniform sampler2D g_xDiffuseTex0;
layout(set = 1, binding = 4) uniform sampler2D g_xNormalTex0;
layout(set = 1, binding = 5) uniform sampler2D g_xRoughnessMetallicTex0;
layout(set = 1, binding = 6) uniform sampler2D g_xOcclusionTex0;
layout(set = 1, binding = 7) uniform sampler2D g_xEmissiveTex0;

// Material 1 textures (bindings 8-12)
layout(set = 1, binding = 8) uniform sampler2D g_xDiffuseTex1;
layout(set = 1, binding = 9) uniform sampler2D g_xNormalTex1;
layout(set = 1, binding = 10) uniform sampler2D g_xRoughnessMetallicTex1;
layout(set = 1, binding = 11) uniform sampler2D g_xOcclusionTex1;
layout(set = 1, binding = 12) uniform sampler2D g_xEmissiveTex1;

// Material 2 textures (bindings 13-17)
layout(set = 1, binding = 13) uniform sampler2D g_xDiffuseTex2;
layout(set = 1, binding = 14) uniform sampler2D g_xNormalTex2;
layout(set = 1, binding = 15) uniform sampler2D g_xRoughnessMetallicTex2;
layout(set = 1, binding = 16) uniform sampler2D g_xOcclusionTex2;
layout(set = 1, binding = 17) uniform sampler2D g_xEmissiveTex2;

// Material 3 textures (bindings 18-22)
layout(set = 1, binding = 18) uniform sampler2D g_xDiffuseTex3;
layout(set = 1, binding = 19) uniform sampler2D g_xNormalTex3;
layout(set = 1, binding = 20) uniform sampler2D g_xRoughnessMetallicTex3;
layout(set = 1, binding = 21) uniform sampler2D g_xOcclusionTex3;
layout(set = 1, binding = 22) uniform sampler2D g_xEmissiveTex3;
#endif

void main(){
	#ifndef SHADOWS

	// ========== Debug Visualization Mode ==========
	uint uDebugMode = floatBitsToUint(g_xTerrainParams2.y);
	if (uDebugMode > 0u)
	{
		vec3 xDebugColor = vec3(1.0, 0.0, 1.0);  // Magenta fallback
		bool bHandled = true;

		switch(uDebugMode)
		{
			case 1u:  // LOD Level
			{
				switch(a_uLODLevel)
				{
					case 0u: xDebugColor = vec3(1.0, 0.0, 0.0); break;
					case 1u: xDebugColor = vec3(0.0, 1.0, 0.0); break;
					case 2u: xDebugColor = vec3(0.0, 0.0, 1.0); break;
					case 3u: xDebugColor = vec3(1.0, 0.0, 1.0); break;
					default: xDebugColor = vec3(1.0, 1.0, 0.0); break;
				}
				break;
			}
			case 2u:  // World Normals
				xDebugColor = a_xNormal * 0.5 + 0.5;
				break;
			case 3u:  // UVs
				xDebugColor = vec3(fract(a_xUV), 0.0);
				break;
			case 4u:  // Splatmap Weights
			{
				vec2 xSplatUV = (a_xWorldPos.xz - g_xTerrainParams.xy) / g_xTerrainParams.zw;
				vec4 xWeights = texture(g_xSplatmap, xSplatUV);
				xDebugColor = vec3(xWeights.r, xWeights.g, xWeights.b);
				break;
			}
			case 5u:  // Roughness (needs texture sampling)
			case 6u:  // Metallic (needs texture sampling)
			case 7u:  // Occlusion (needs texture sampling)
				bHandled = false;
				break;
			case 8u:  // World Position
				xDebugColor = fract(a_xWorldPos / 64.0);
				break;
			case 9u:  // Chunk Grid
			{
				vec2 xChunkLocal = mod(a_xWorldPos.xz, 64.0);
				float fBorder = step(xChunkLocal.x, 0.5) + step(63.5, xChunkLocal.x) +
				                step(xChunkLocal.y, 0.5) + step(63.5, xChunkLocal.y);
				xDebugColor = mix(vec3(0.2), vec3(1.0, 0.5, 0.0), min(fBorder, 1.0));
				break;
			}
			case 10u:  // Tangent
				xDebugColor = a_xTangent * 0.5 + 0.5;
				break;
			case 11u:  // Bitangent Sign
				xDebugColor = a_fBitangentSign > 0.0 ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 0.0, 1.0);
				break;
			default:
				break;
		}

		if (bHandled)
		{
			OutputToGBuffer(vec4(xDebugColor, 1.0), a_xNormal, 0.2, 0.8, 0.0, 0.0);
			return;
		}
	}

	// ========== Sample Splatmap ==========
	// RGBA = grass/rock/dirt/sand weights
	vec2 xSplatUV = (a_xWorldPos.xz - g_xTerrainParams.xy) / g_xTerrainParams.zw;
	vec4 xWeights = texture(g_xSplatmap, xSplatUV);

	// ========== Apply UV Transform for All 4 Materials ==========
	vec2 xUV0 = TransformUV(a_xUV, g_axUVParams[0].xy, g_axUVParams[0].zw);
	vec2 xUV1 = TransformUV(a_xUV, g_axUVParams[1].xy, g_axUVParams[1].zw);
	vec2 xUV2 = TransformUV(a_xUV, g_axUVParams[2].xy, g_axUVParams[2].zw);
	vec2 xUV3 = TransformUV(a_xUV, g_axUVParams[3].xy, g_axUVParams[3].zw);

	// ========== Sample All 4 Material Textures ==========
	// Material 0
	vec4 xDiffuse0 = SampleDiffuseWithBaseColor(g_xDiffuseTex0, xUV0, g_axBaseColors[0]);
	vec3 xNormalMap0 = texture(g_xNormalTex0, xUV0).xyz * 2.0 - 1.0;
	vec2 xRM0 = texture(g_xRoughnessMetallicTex0, xUV0).gb;
	float fOcclusion0 = SampleOcclusion(g_xOcclusionTex0, xUV0, g_axMaterialParams[0].z);
	float fEmissive0 = CalculateEmissiveLuminance(g_xEmissiveTex0, xUV0, g_axEmissiveParams[0].rgb, g_axEmissiveParams[0].w);

	// Material 1
	vec4 xDiffuse1 = SampleDiffuseWithBaseColor(g_xDiffuseTex1, xUV1, g_axBaseColors[1]);
	vec3 xNormalMap1 = texture(g_xNormalTex1, xUV1).xyz * 2.0 - 1.0;
	vec2 xRM1 = texture(g_xRoughnessMetallicTex1, xUV1).gb;
	float fOcclusion1 = SampleOcclusion(g_xOcclusionTex1, xUV1, g_axMaterialParams[1].z);
	float fEmissive1 = CalculateEmissiveLuminance(g_xEmissiveTex1, xUV1, g_axEmissiveParams[1].rgb, g_axEmissiveParams[1].w);

	// Material 2
	vec4 xDiffuse2 = SampleDiffuseWithBaseColor(g_xDiffuseTex2, xUV2, g_axBaseColors[2]);
	vec3 xNormalMap2 = texture(g_xNormalTex2, xUV2).xyz * 2.0 - 1.0;
	vec2 xRM2 = texture(g_xRoughnessMetallicTex2, xUV2).gb;
	float fOcclusion2 = SampleOcclusion(g_xOcclusionTex2, xUV2, g_axMaterialParams[2].z);
	float fEmissive2 = CalculateEmissiveLuminance(g_xEmissiveTex2, xUV2, g_axEmissiveParams[2].rgb, g_axEmissiveParams[2].w);

	// Material 3
	vec4 xDiffuse3 = SampleDiffuseWithBaseColor(g_xDiffuseTex3, xUV3, g_axBaseColors[3]);
	vec3 xNormalMap3 = texture(g_xNormalTex3, xUV3).xyz * 2.0 - 1.0;
	vec2 xRM3 = texture(g_xRoughnessMetallicTex3, xUV3).gb;
	float fOcclusion3 = SampleOcclusion(g_xOcclusionTex3, xUV3, g_axMaterialParams[3].z);
	float fEmissive3 = CalculateEmissiveLuminance(g_xEmissiveTex3, xUV3, g_axEmissiveParams[3].rgb, g_axEmissiveParams[3].w);

	// ========== Blend Materials Using Splatmap Weights ==========
	vec4 xDiffuse = xDiffuse0 * xWeights.r + xDiffuse1 * xWeights.g + xDiffuse2 * xWeights.b + xDiffuse3 * xWeights.a;
	vec3 xNormalTangent = normalize(xNormalMap0 * xWeights.r + xNormalMap1 * xWeights.g + xNormalMap2 * xWeights.b + xNormalMap3 * xWeights.a);

	// Blend roughness/metallic with per-material multipliers
	float fRoughness = xRM0.x * g_axMaterialParams[0].y * xWeights.r
	                 + xRM1.x * g_axMaterialParams[1].y * xWeights.g
	                 + xRM2.x * g_axMaterialParams[2].y * xWeights.b
	                 + xRM3.x * g_axMaterialParams[3].y * xWeights.a;
	float fMetallic = xRM0.y * g_axMaterialParams[0].x * xWeights.r
	                + xRM1.y * g_axMaterialParams[1].x * xWeights.g
	                + xRM2.y * g_axMaterialParams[2].x * xWeights.b
	                + xRM3.y * g_axMaterialParams[3].x * xWeights.a;
	float fOcclusion = fOcclusion0 * xWeights.r + fOcclusion1 * xWeights.g + fOcclusion2 * xWeights.b + fOcclusion3 * xWeights.a;
	float fEmissive = fEmissive0 * xWeights.r + fEmissive1 * xWeights.g + fEmissive2 * xWeights.b + fEmissive3 * xWeights.a;

	// ========== Texture-Dependent Debug Visualization (modes 5-7) ==========
	if (uDebugMode >= 5u && uDebugMode <= 7u)
	{
		vec3 xDebugColor;
		if (uDebugMode == 5u)       xDebugColor = vec3(fRoughness);
		else if (uDebugMode == 6u)  xDebugColor = vec3(fMetallic);
		else                        xDebugColor = vec3(fOcclusion);
		OutputToGBuffer(vec4(xDebugColor, 1.0), a_xNormal, 0.2, 0.8, 0.0, 0.0);
		return;
	}

	// ========== Reconstruct TBN Matrix ==========
	mat3 TBN = BuildTBN(a_xNormal, a_xTangent, a_fBitangentSign);

	// Transform normal from tangent space to world space
	vec3 xWorldNormal = normalize(TBN * xNormalTangent);

	// ========== Output to G-Buffer (with emissive) ==========
	OutputToGBuffer(xDiffuse, xWorldNormal, fOcclusion, fRoughness, fMetallic, fEmissive);

	#endif  // !SHADOWS
}
