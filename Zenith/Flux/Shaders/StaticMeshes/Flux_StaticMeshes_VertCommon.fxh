#include "../Common.fxh"

layout(location = 0) in vec3 a_xPosition;
layout(location = 1) in vec2 a_xUV;
layout(location = 2) in vec3 a_xNormal;
layout(location = 3) in vec3 a_xTangent;
layout(location = 4) in vec3 a_xBitangent;
layout(location = 5) in vec4 a_xColor;

layout(location = 0) out vec2 o_xUV;
layout(location = 1) out vec3 o_xNormal;
layout(location = 2) out vec3 o_xWorldPos;
layout(location = 3) out vec3 o_xTangent;
layout(location = 4) out float o_fBitangentSign;  // Sign for bitangent reconstruction (saves 2 varyings)
layout(location = 5) out vec4 o_xColor;

// Material Constants (128 bytes, scratch buffer at binding 1, replaces push constants)
// Layout matches C++ MaterialPushConstants structure
layout(std140, set = 0, binding = 1) uniform PushConstants{
	mat4 g_xModelMatrix;       // 64 bytes - Model transform matrix
	vec4 g_xBaseColor;         // 16 bytes - RGBA base color multiplier
	vec4 g_xMaterialParams;    // 16 bytes - (metallic, roughness, alphaCutoff, occlusionStrength)
	vec4 g_xUVParams;          // 16 bytes - (tilingX, tilingY, offsetX, offsetY)
	vec4 g_xEmissiveParams;    // 16 bytes - (R, G, B, intensity)
};  // Total: 128 bytes

#ifdef SHADOWS
layout(std140, set = 1, binding=0) uniform ShadowMatrix{
	mat4 g_xSunViewProjMat;
};
#endif

void main()
{
	o_xUV = a_xUV;
	o_xColor = a_xColor;
	
	// OPTIMIZATION: Pre-compute normal matrix on CPU if possible
	// For now, compute inverse-transpose for non-uniform scaling support
	mat3 xNormalMatrix = transpose(inverse(mat3(g_xModelMatrix)));
	
	// Transform normal and tangent to world space
	o_xNormal = normalize(xNormalMatrix * a_xNormal);
	o_xTangent = normalize(xNormalMatrix * a_xTangent);
	
	// OPTIMIZATION: Compute bitangent sign (handedness) instead of passing full bitangent
	// This saves 2 varying components (vec3 -> float)
	// Bitangent will be reconstructed in fragment shader: B = cross(N, T) * sign
	vec3 xBitangent = normalize(xNormalMatrix * a_xBitangent);
	o_fBitangentSign = sign(dot(xBitangent, cross(o_xNormal, o_xTangent)));
	
	o_xWorldPos = (g_xModelMatrix * vec4(a_xPosition,1)).xyz;

	#ifdef SHADOWS
	gl_Position = g_xSunViewProjMat * vec4(o_xWorldPos,1);
	#else
	gl_Position = g_xViewProjMat * vec4(o_xWorldPos,1);
	#endif
}