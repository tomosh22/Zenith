#include "../Common.fxh"
#ifndef SHADOWS
#include "../GBufferCommon.fxh"
#endif

layout(location = 0) in vec2 a_xUV;
layout(location = 1) in vec3 a_xNormal;
layout(location = 2) in vec3 a_xWorldPos;
layout(location = 3) in vec3 a_xTangent;
layout(location = 4) in float a_fBitangentSign;
layout(location = 5) in vec4 a_xColor;

#ifndef SHADOWS
layout(set = 1, binding = 0) uniform sampler2D g_xDiffuseTex;
layout(set = 1, binding = 1) uniform sampler2D g_xNormalTex;
layout(set = 1, binding = 2) uniform sampler2D g_xRoughnessMetallicTex;
layout(set = 1, binding = 3) uniform sampler2D g_xOcclusionTex;
layout(set = 1, binding = 4) uniform sampler2D g_xEmissiveTex;
#endif

void main(){
	#ifndef SHADOWS
	// Sample diffuse texture
	vec4 xDiffuse = texture(g_xDiffuseTex, a_xUV);
	
	// If vertex has color data, multiply it with the texture
	if (a_xColor.a > 0.0f)
	{
		xDiffuse *= a_xColor;
	}
	
	// Early alpha discard
	if (xDiffuse.a < 0.01f)
	{
		discard;
	}
	
	// Reconstruct TBN matrix - bitangent computed from normal and tangent
	vec3 N = normalize(a_xNormal);
	vec3 T = normalize(a_xTangent);
	vec3 B = cross(N, T) * a_fBitangentSign;
	mat3 TBN = mat3(T, B, N);
	
	// Sample normal map and transform to world space
	vec3 xNormalTangent = texture(g_xNormalTex, a_xUV).xyz * 2.0 - 1.0;
	vec3 xWorldNormal = normalize(TBN * xNormalTangent);
	
	// Sample roughness/metallic
	vec2 xRoughnessMetallic = texture(g_xRoughnessMetallicTex, a_xUV).gb;
	
	OutputToGBuffer(xDiffuse, xWorldNormal, 0.3, xRoughnessMetallic.x, xRoughnessMetallic.y);
	#endif
}