#include "../Common.fxh"
#ifndef SHADOWS
#include "../GBufferCommon.fxh"
#endif

layout(location = 0) in vec2 a_xUV;
layout(location = 1) in vec3 a_xNormal;
layout(location = 2) in vec3 a_xWorldPos;
layout(location = 3) in mat3 a_xTBN;
layout(location = 6) in vec4 a_xColor;

#ifndef SHADOWS
layout(set = 1, binding = 1) uniform sampler2D g_xDiffuseTex;
layout(set = 1, binding = 2) uniform sampler2D g_xNormalTex;
layout(set = 1, binding = 3) uniform sampler2D g_xRoughnessMetallicTex;
layout(set = 1, binding = 4) uniform sampler2D g_xOcclusionTex;
layout(set = 1, binding = 5) uniform sampler2D g_xEmissiveTex;
#endif

void main(){
	#ifndef SHADOWS
	vec4 xDiffuse = texture(g_xDiffuseTex, a_xUV);
	
	// If vertex has color data, multiply it with the texture
	if (a_xColor.a > 0.0f)
	{
		xDiffuse *= a_xColor;
	}
	
	vec3 xNormal = a_xTBN * (2 * texture(g_xNormalTex, a_xUV).xyz - 1.);
	vec2 xRoughnessMetallic = texture(g_xRoughnessMetallicTex, a_xUV).gb;
	
	OutputToGBuffer(xDiffuse, xNormal, 0.2, xRoughnessMetallic.x, xRoughnessMetallic.y);
	#endif
}
