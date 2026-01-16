#include "../Common.fxh"
#ifndef SHADOWS
#include "../GBufferCommon.fxh"
#include "../MaterialCommon.fxh"
#endif

// Varyings from vertex shader - only declared when not doing shadow pass
#ifndef SHADOWS
layout(location = 0) in vec2 a_xUV;
layout(location = 1) in vec3 a_xNormal;
layout(location = 2) in vec3 a_xWorldPos;
layout(location = 3) in vec3 a_xTangent;
layout(location = 4) in float a_fBitangentSign;
layout(location = 5) in vec4 a_xColor;
layout(location = 6) in vec4 a_xInstanceTint;  // Per-instance color tint
#endif

// Material Constants (scratch buffer in per-draw set 1, must match vertex shader layout)
layout(std140, set = 1, binding = 0) uniform PushConstants{
	mat4 g_xModelMatrix;       // 64 bytes - unused for instanced
	vec4 g_xBaseColor;         // 16 bytes
	vec4 g_xMaterialParams;    // 16 bytes (metallic, roughness, alphaCutoff, occlusionStrength)
	vec4 g_xUVParams;          // 16 bytes (tilingX, tilingY, offsetX, offsetY)
	vec4 g_xEmissiveParams;    // 16 bytes (R, G, B, intensity)
};

#ifndef SHADOWS
layout(set = 1, binding = 1) uniform sampler2D g_xDiffuseTex;
layout(set = 1, binding = 2) uniform sampler2D g_xNormalTex;
layout(set = 1, binding = 3) uniform sampler2D g_xRoughnessMetallicTex;
layout(set = 1, binding = 4) uniform sampler2D g_xOcclusionTex;
layout(set = 1, binding = 5) uniform sampler2D g_xEmissiveTex;
#endif

void main(){
	#ifndef SHADOWS
	// Apply UV transformation (tiling and offset)
	vec2 xUV = TransformUV(a_xUV, GetUVTiling(), GetUVOffset());

	// Sample diffuse texture and apply base color multiplier
	vec4 xDiffuse = SampleDiffuseWithBaseColor(g_xDiffuseTex, xUV, g_xBaseColor);

	// Apply per-instance color tint
	xDiffuse.rgb *= a_xInstanceTint.rgb;

	// If vertex has color data, multiply it with the texture
	if (a_xColor.a > 0.0f)
	{
		xDiffuse *= a_xColor;
	}

	// Alpha test with material cutoff
	float fAlphaCutoff = GetAlphaCutoff();
	if (xDiffuse.a < fAlphaCutoff)
	{
		discard;
	}

	// Reconstruct TBN matrix - bitangent computed from normal and tangent
	mat3 TBN = BuildTBN(a_xNormal, a_xTangent, a_fBitangentSign);

	// Sample normal map and transform to world space
	vec3 xWorldNormal = SampleNormalMap(g_xNormalTex, xUV, TBN);

	// Sample roughness/metallic and apply material multipliers
	float fRoughness, fMetallic;
	SampleRoughnessMetallic(g_xRoughnessMetallicTex, xUV,
							GetRoughnessMultiplier(), GetMetallicMultiplier(),
							fRoughness, fMetallic);

	// Sample occlusion with strength
	float fOcclusion = SampleOcclusion(g_xOcclusionTex, xUV, GetOcclusionStrength());

	// Calculate emissive luminance
	float fEmissive = CalculateEmissiveLuminance(g_xEmissiveTex, xUV,
												  GetEmissiveColor(), GetEmissiveIntensity());

	OutputToGBuffer(xDiffuse, xWorldNormal, fOcclusion, fRoughness, fMetallic, fEmissive);
	#endif
}
