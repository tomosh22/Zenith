// ============================================================================
// MaterialCommon.fxh - Centralized Material System
//
// This header defines all material-related structures and functions for the
// Zenith/Flux renderer. All shaders that use materials should include this.
// ============================================================================

#ifndef MATERIAL_COMMON_FXH
#define MATERIAL_COMMON_FXH

// ============================================================================
// Material Push Constants Structure
//
// Layout matches C++ MaterialPushConstants (128 bytes total for Vulkan compatibility)
// ============================================================================
// NOTE: This structure is expected to be defined in each shader that uses materials
// with a push_constant layout. The individual shaders should include this header
// and use the functions below.

// Material property access helpers (assumes g_xMaterialParams is available)
// g_xMaterialParams = vec4(metallic, roughness, alphaCutoff, occlusionStrength)
#define GetMetallicMultiplier()      g_xMaterialParams.x
#define GetRoughnessMultiplier()     g_xMaterialParams.y
#define GetAlphaCutoff()             g_xMaterialParams.z
#define GetOcclusionStrength()       g_xMaterialParams.w

// UV parameter access helpers (assumes g_xUVParams is available)
// g_xUVParams = vec4(tilingX, tilingY, offsetX, offsetY)
#define GetUVTiling()                g_xUVParams.xy
#define GetUVOffset()                g_xUVParams.zw

// Emissive parameter access helpers (assumes g_xEmissiveParams is available)
// g_xEmissiveParams = vec4(R, G, B, intensity)
#define GetEmissiveColor()           g_xEmissiveParams.rgb
#define GetEmissiveIntensity()       g_xEmissiveParams.w

// ============================================================================
// UV Transformation
// ============================================================================

// Apply UV tiling and offset transformation
vec2 TransformUV(vec2 xUV, vec2 xTiling, vec2 xOffset)
{
	return xUV * xTiling + xOffset;
}

// ============================================================================
// Texture Sampling Functions
// ============================================================================

// Sample diffuse texture and apply base color multiplier
vec4 SampleDiffuseWithBaseColor(sampler2D xTex, vec2 xUV, vec4 xBaseColor)
{
	return texture(xTex, xUV) * xBaseColor;
}

// Sample normal map and transform to world space using TBN matrix
vec3 SampleNormalMap(sampler2D xTex, vec2 xUV, mat3 xTBN)
{
	vec3 xNormalTangent = texture(xTex, xUV).xyz * 2.0 - 1.0;
	return normalize(xTBN * xNormalTangent);
}

// Sample roughness/metallic texture and apply material multipliers
void SampleRoughnessMetallic(sampler2D xTex, vec2 xUV,
							 float fRoughnessMult, float fMetallicMult,
							 out float fRoughness, out float fMetallic)
{
	vec2 xRM = texture(xTex, xUV).gb;  // G = roughness, B = metallic (standard layout)
	fRoughness = xRM.x * fRoughnessMult;
	fMetallic = xRM.y * fMetallicMult;
}

// Sample occlusion texture and apply strength
float SampleOcclusion(sampler2D xTex, vec2 xUV, float fStrength)
{
	float fAO = texture(xTex, xUV).r;
	return mix(1.0, fAO, fStrength);
}

// Calculate emissive contribution as luminance (stored in G-Buffer B channel)
float CalculateEmissiveLuminance(sampler2D xTex, vec2 xUV, vec3 xEmissiveColor, float fIntensity)
{
	vec3 xEmissiveTex = texture(xTex, xUV).rgb;
	vec3 xEmissive = xEmissiveTex * xEmissiveColor * fIntensity;
	// Convert to luminance for G-Buffer storage
	return dot(xEmissive, vec3(0.299, 0.587, 0.114));
}

// ============================================================================
// Full Material Sampling (convenience function)
//
// Samples all material textures and applies material property multipliers.
// Requires the following uniforms to be defined:
// - g_xDiffuseTex, g_xNormalTex, g_xRoughnessMetallicTex, g_xOcclusionTex, g_xEmissiveTex
// - g_xBaseColor, g_xMaterialParams, g_xUVParams, g_xEmissiveParams
// ============================================================================

struct MaterialSampleResult
{
	vec4 xDiffuse;
	vec3 xWorldNormal;
	float fRoughness;
	float fMetallic;
	float fOcclusion;
	float fEmissive;
};

// TBN matrix reconstruction helper
mat3 BuildTBN(vec3 xNormal, vec3 xTangent, float fBitangentSign)
{
	vec3 N = normalize(xNormal);
	vec3 T = normalize(xTangent);
	vec3 B = cross(N, T) * fBitangentSign;
	return mat3(T, B, N);
}

#endif // MATERIAL_COMMON_FXH
