#include "../Common.fxh"
#ifndef SHADOWS
#include "../GBufferCommon.fxh"
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
// Matches C++ TerrainMaterialPushConstants struct
// Note: Moved from set 0 to set 1 so FrameConstants can be bound once per frame
layout(std140, set = 1, binding = 0) uniform TerrainMaterialConstants {
	vec4 g_xBaseColor0;        // Material 0 base color
	vec4 g_xUVParams0;         // Material 0: (tilingX, tilingY, offsetX, offsetY)
	vec4 g_xMaterialParams0;   // Material 0: (metallic, roughness, occlusionStrength, unused)

	vec4 g_xBaseColor1;        // Material 1 base color
	vec4 g_xUVParams1;         // Material 1: (tilingX, tilingY, offsetX, offsetY)
	vec4 g_xMaterialParams1;   // Material 1: (metallic, roughness, occlusionStrength, unused)

	uint g_uVisualizeLOD;      // 0 = normal rendering, 1 = visualize LOD
	float g_fPad[7];           // Padding to 128 bytes
};

// ========== Material Textures (per-draw, set 1 bindings 2-7) ==========
#ifndef SHADOWS
layout(set = 1, binding = 2) uniform sampler2D g_xDiffuseTex0;
layout(set = 1, binding = 3) uniform sampler2D g_xNormalTex0;
layout(set = 1, binding = 4) uniform sampler2D g_xRoughnessMetallicTex0;

layout(set = 1, binding = 5) uniform sampler2D g_xDiffuseTex1;
layout(set = 1, binding = 6) uniform sampler2D g_xNormalTex1;
layout(set = 1, binding = 7) uniform sampler2D g_xRoughnessMetallicTex1;
#endif

void main(){
	#ifndef SHADOWS

	// ========== LOD Visualization Mode ==========
	if (g_uVisualizeLOD != 0u)
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

		OutputToGBuffer(vec4(lodColor, 1.0), a_xNormal, 0.2, 0.8, 0.0);
		return;
	}

	// ========== Apply UV Transform for Both Materials ==========
	vec2 xUV0 = a_xUV * g_xUVParams0.xy + g_xUVParams0.zw;
	vec2 xUV1 = a_xUV * g_xUVParams1.xy + g_xUVParams1.zw;

	// ========== Full Material Rendering with Normal Mapping ==========
	// Sample all textures for both materials with transformed UVs
	vec4 xDiffuse0 = texture(g_xDiffuseTex0, xUV0) * g_xBaseColor0;
	vec4 xDiffuse1 = texture(g_xDiffuseTex1, xUV1) * g_xBaseColor1;
	vec3 xNormalMap0 = texture(g_xNormalTex0, xUV0).xyz * 2.0 - 1.0;
	vec3 xNormalMap1 = texture(g_xNormalTex1, xUV1).xyz * 2.0 - 1.0;
	vec2 xRM0 = texture(g_xRoughnessMetallicTex0, xUV0).gb;
	vec2 xRM1 = texture(g_xRoughnessMetallicTex1, xUV1).gb;

	// Lerp between materials based on vertex material lerp value
	float fLerp = a_fMaterialLerp;
	vec4 xDiffuse = mix(xDiffuse0, xDiffuse1, fLerp);
	vec3 xNormalTangent = normalize(mix(xNormalMap0, xNormalMap1, fLerp));
	vec2 xRM = mix(xRM0, xRM1, fLerp);

	// Apply material roughness/metallic multipliers
	float fRoughness = xRM.x * mix(g_xMaterialParams0.y, g_xMaterialParams1.y, fLerp);
	float fMetallic = xRM.y * mix(g_xMaterialParams0.x, g_xMaterialParams1.x, fLerp);
	float fOcclusion = mix(g_xMaterialParams0.z, g_xMaterialParams1.z, fLerp);

	// Reconstruct TBN matrix - bitangent reconstructed from normal and tangent
	vec3 N = normalize(a_xNormal);
	vec3 T = normalize(a_xTangent);
	vec3 B = cross(N, T) * a_fBitangentSign;
	mat3 TBN = mat3(T, B, N);

	// Transform normal from tangent space to world space
	vec3 xWorldNormal = normalize(TBN * xNormalTangent);

	OutputToGBuffer(xDiffuse, xWorldNormal, fOcclusion, fRoughness, fMetallic);

	#endif  // !SHADOWS
}
