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
layout(location = 4) in float a_fMaterialLerp;
layout(location = 5) flat in uint a_uLODLevel;  // LOD level from vertex shader
layout(location = 6) in float a_fBitangentSign;  // Sign for bitangent reconstruction
#endif

// ========== Terrain Material Constants (per-draw, set 1 binding 0) ==========
// Matches C++ TerrainMaterialPushConstants struct - uses full material system
layout(std140, set = 1, binding = 0) uniform TerrainMaterialConstants {
	// Material 0 (64 bytes)
	vec4 g_xBaseColor0;        // Material 0 base color
	vec4 g_xUVParams0;         // Material 0: (tilingX, tilingY, offsetX, offsetY)
	vec4 g_xMaterialParams0;   // Material 0: (metallic, roughness, occlusionStrength, visualizeLOD)
	vec4 g_xEmissiveParams0;   // Material 0: (R, G, B, intensity)

	// Material 1 (64 bytes)
	vec4 g_xBaseColor1;        // Material 1 base color
	vec4 g_xUVParams1;         // Material 1: (tilingX, tilingY, offsetX, offsetY)
	vec4 g_xMaterialParams1;   // Material 1: (metallic, roughness, occlusionStrength, unused)
	vec4 g_xEmissiveParams1;   // Material 1: (R, G, B, intensity)
};

// ========== Material Textures (per-draw, set 1 bindings 2-11) ==========
// Full material system: 5 textures per material (diffuse, normal, RM, occlusion, emissive)
#ifndef SHADOWS
// Material 0 textures
layout(set = 1, binding = 2) uniform sampler2D g_xDiffuseTex0;
layout(set = 1, binding = 3) uniform sampler2D g_xNormalTex0;
layout(set = 1, binding = 4) uniform sampler2D g_xRoughnessMetallicTex0;
layout(set = 1, binding = 5) uniform sampler2D g_xOcclusionTex0;
layout(set = 1, binding = 6) uniform sampler2D g_xEmissiveTex0;

// Material 1 textures
layout(set = 1, binding = 7) uniform sampler2D g_xDiffuseTex1;
layout(set = 1, binding = 8) uniform sampler2D g_xNormalTex1;
layout(set = 1, binding = 9) uniform sampler2D g_xRoughnessMetallicTex1;
layout(set = 1, binding = 10) uniform sampler2D g_xOcclusionTex1;
layout(set = 1, binding = 11) uniform sampler2D g_xEmissiveTex1;
#endif

void main(){
	#ifndef SHADOWS

	// ========== LOD Visualization Mode ==========
	// Packed into g_xMaterialParams0.w
	if (g_xMaterialParams0.w > 0.5)
	{
		vec3 lodColor;
		switch(a_uLODLevel)
		{
			case 0u: lodColor = vec3(1.0, 0.0, 0.0); break;  // Red
			case 1u: lodColor = vec3(0.0, 1.0, 0.0); break;  // Green
			case 2u: lodColor = vec3(0.0, 0.0, 1.0); break;  // Blue
			case 3u: lodColor = vec3(1.0, 0.0, 1.0); break;  // Magenta
			default: lodColor = vec3(1.0, 1.0, 0.0); break;  // Yellow (error)
		}

		OutputToGBuffer(vec4(lodColor, 1.0), a_xNormal, 0.2, 0.8, 0.0, 0.0);
		return;
	}

	// ========== Apply UV Transform for Both Materials ==========
	vec2 xUV0 = TransformUV(a_xUV, g_xUVParams0.xy, g_xUVParams0.zw);
	vec2 xUV1 = TransformUV(a_xUV, g_xUVParams1.xy, g_xUVParams1.zw);

	// ========== Sample All Material Textures ==========
	// Material 0
	vec4 xDiffuse0 = SampleDiffuseWithBaseColor(g_xDiffuseTex0, xUV0, g_xBaseColor0);
	vec3 xNormalMap0 = texture(g_xNormalTex0, xUV0).xyz * 2.0 - 1.0;
	vec2 xRM0 = texture(g_xRoughnessMetallicTex0, xUV0).gb;
	float fOcclusion0 = SampleOcclusion(g_xOcclusionTex0, xUV0, g_xMaterialParams0.z);
	float fEmissive0 = CalculateEmissiveLuminance(g_xEmissiveTex0, xUV0, g_xEmissiveParams0.rgb, g_xEmissiveParams0.w);

	// Material 1
	vec4 xDiffuse1 = SampleDiffuseWithBaseColor(g_xDiffuseTex1, xUV1, g_xBaseColor1);
	vec3 xNormalMap1 = texture(g_xNormalTex1, xUV1).xyz * 2.0 - 1.0;
	vec2 xRM1 = texture(g_xRoughnessMetallicTex1, xUV1).gb;
	float fOcclusion1 = SampleOcclusion(g_xOcclusionTex1, xUV1, g_xMaterialParams1.z);
	float fEmissive1 = CalculateEmissiveLuminance(g_xEmissiveTex1, xUV1, g_xEmissiveParams1.rgb, g_xEmissiveParams1.w);

	// ========== Lerp Between Materials ==========
	float fLerp = a_fMaterialLerp;
	vec4 xDiffuse = mix(xDiffuse0, xDiffuse1, fLerp);
	vec3 xNormalTangent = normalize(mix(xNormalMap0, xNormalMap1, fLerp));
	vec2 xRM = mix(xRM0, xRM1, fLerp);

	// Apply material roughness/metallic multipliers
	float fRoughness = xRM.x * mix(g_xMaterialParams0.y, g_xMaterialParams1.y, fLerp);
	float fMetallic = xRM.y * mix(g_xMaterialParams0.x, g_xMaterialParams1.x, fLerp);
	float fOcclusion = mix(fOcclusion0, fOcclusion1, fLerp);
	float fEmissive = mix(fEmissive0, fEmissive1, fLerp);

	// ========== Reconstruct TBN Matrix ==========
	mat3 TBN = BuildTBN(a_xNormal, a_xTangent, a_fBitangentSign);

	// Transform normal from tangent space to world space
	vec3 xWorldNormal = normalize(TBN * xNormalTangent);

	// ========== Output to G-Buffer (with emissive) ==========
	OutputToGBuffer(xDiffuse, xWorldNormal, fOcclusion, fRoughness, fMetallic, fEmissive);

	#endif  // !SHADOWS
}
