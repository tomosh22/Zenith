#ifndef MESH_FRAG_FXH
#define MESH_FRAG_FXH

// ============================================================================
// Unified mesh fragment shader
// Define ONE of: MESH_STATIC (default), MESH_ANIMATED, MESH_INSTANCED
// Optionally define SHADOWS for shadow-map pass (fragment is a no-op).
// ============================================================================

#include "../Common.fxh"
#ifndef SHADOWS
#include "../GBufferCommon.fxh"
#include "../MaterialCommon.fxh"
#endif

// --- Varyings from vertex shader (GBuffer pass only) ---
#ifndef SHADOWS
layout(location = 0) in vec2 a_xUV;
layout(location = 1) in vec3 a_xNormal;
layout(location = 2) in vec3 a_xWorldPos;

#ifdef MESH_ANIMATED
layout(location = 3) in mat3 a_xTBN;  // Full TBN from bone-weighted tangent frame
layout(location = 6) in vec4 a_xColor;
#else
layout(location = 3) in vec3 a_xTangent;
layout(location = 4) in float a_fBitangentSign;
layout(location = 5) in vec4 a_xColor;
#ifdef MESH_INSTANCED
layout(location = 6) in vec4 a_xInstanceTint;
#endif
#endif // MESH_ANIMATED

#endif // SHADOWS

// --- DrawConstants (set=1, binding=0) ---
#include "../DrawConstants.fxh"

// --- Material textures ---
// Animated meshes offset by 1 (Bones UBO occupies binding 1)
#ifndef SHADOWS
#ifdef MESH_ANIMATED
#define TEXTURE_BINDING_BASE 2
#else
#define TEXTURE_BINDING_BASE 1
#endif

layout(set = 1, binding = TEXTURE_BINDING_BASE + 0) uniform sampler2D g_xDiffuseTex;
layout(set = 1, binding = TEXTURE_BINDING_BASE + 1) uniform sampler2D g_xNormalTex;
layout(set = 1, binding = TEXTURE_BINDING_BASE + 2) uniform sampler2D g_xRoughnessMetallicTex;
layout(set = 1, binding = TEXTURE_BINDING_BASE + 3) uniform sampler2D g_xOcclusionTex;
layout(set = 1, binding = TEXTURE_BINDING_BASE + 4) uniform sampler2D g_xEmissiveTex;
#endif

// ============================================================================
// main()
// ============================================================================
void main()
{
#ifndef SHADOWS
	// Apply UV transformation (tiling and offset)
	vec2 xUV = TransformUV(a_xUV, GetUVTiling(), GetUVOffset());

	// Sample diffuse texture and apply base color multiplier
	vec4 xDiffuse = SampleDiffuseWithBaseColor(g_xDiffuseTex, xUV, g_xBaseColor);

#ifdef MESH_INSTANCED
	// Apply per-instance color tint
	xDiffuse.rgb *= a_xInstanceTint.rgb;
#endif

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

	// Normal map to world space
#ifdef MESH_ANIMATED
	// Animated: TBN is pre-computed from bone-weighted vectors in vertex shader
	vec3 xNormalTangent = texture(g_xNormalTex, xUV).xyz * 2.0 - 1.0;
	vec3 xWorldNormal = normalize(a_xTBN * xNormalTangent);
#else
	// Static / Instanced: reconstruct TBN from tangent + bitangent sign
	mat3 TBN = BuildTBN(a_xNormal, a_xTangent, a_fBitangentSign);
	vec3 xWorldNormal = SampleNormalMap(g_xNormalTex, xUV, TBN);
#endif

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
#endif // SHADOWS
}

#endif // MESH_FRAG_FXH
