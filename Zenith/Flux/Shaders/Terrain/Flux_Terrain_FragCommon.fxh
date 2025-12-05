#include "../Common.fxh"
#ifndef SHADOWS
#include "../GBufferCommon.fxh"
#endif

// ========== Fragment Inputs ==========
#ifndef SHADOWS
layout(location = 0) in vec2 a_xUV;
#endif
layout(location = 1) in vec3 a_xNormal;
layout(location = 2) in vec3 a_xWorldPos;
layout(location = 3) in vec3 a_xTangent;
layout(location = 4) in float a_fMaterialLerp;
layout(location = 5) flat in uint a_uLODLevel;  // LOD level from vertex shader
layout(location = 6) in float a_fBitangentSign;  // Sign for bitangent reconstruction

// ========== Material Textures ==========
#ifndef SHADOWS
layout(set = 1, binding = 0) uniform sampler2D g_xDiffuseTex0;
layout(set = 1, binding = 1) uniform sampler2D g_xNormalTex0;
layout(set = 1, binding = 2) uniform sampler2D g_xRoughnessMetallicTex0;

layout(set = 1, binding = 3) uniform sampler2D g_xDiffuseTex1;
layout(set = 1, binding = 4) uniform sampler2D g_xNormalTex1;
layout(set = 1, binding = 5) uniform sampler2D g_xRoughnessMetallicTex1;
#endif

// Push constant for debug mode
layout(push_constant) uniform DebugConstants {
	uint debugVisualizeLOD;  // 0 = normal rendering, 1 = visualize LOD
} debugConstants;

void main(){
	#ifndef SHADOWS
	
	// ========== LOD Visualization Mode ==========
	if (debugConstants.debugVisualizeLOD != 0)
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
	
	// ========== Full Material Rendering with Normal Mapping ==========
	// Sample all textures for both materials
	vec4 xDiffuse0 = texture(g_xDiffuseTex0, a_xUV);
	vec4 xDiffuse1 = texture(g_xDiffuseTex1, a_xUV);
	vec3 xNormalMap0 = texture(g_xNormalTex0, a_xUV).xyz * 2.0 - 1.0;
	vec3 xNormalMap1 = texture(g_xNormalTex1, a_xUV).xyz * 2.0 - 1.0;
	vec2 xRM0 = texture(g_xRoughnessMetallicTex0, a_xUV).gb;
	vec2 xRM1 = texture(g_xRoughnessMetallicTex1, a_xUV).gb;
	
	// Lerp between materials based on vertex material lerp value
	float fLerp = a_fMaterialLerp;
	vec4 xDiffuse = mix(xDiffuse0, xDiffuse1, fLerp);
	vec3 xNormalTangent = normalize(mix(xNormalMap0, xNormalMap1, fLerp));
	vec2 xRM = mix(xRM0, xRM1, fLerp);
	
	// Reconstruct TBN matrix - bitangent reconstructed from normal and tangent
	vec3 N = normalize(a_xNormal);
	vec3 T = normalize(a_xTangent);
	vec3 B = cross(N, T) * a_fBitangentSign;
	mat3 TBN = mat3(T, B, N);
	
	// Transform normal from tangent space to world space
	vec3 xWorldNormal = normalize(TBN * xNormalTangent);
	
	OutputToGBuffer(xDiffuse, xWorldNormal, 0.2, xRM.x, xRM.y);
	
	#endif  // !SHADOWS
}