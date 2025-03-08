#include "../Common.fxh"
#ifndef SHADOWS
#include "../GBufferCommon.fxh"
#endif

layout(location = 0) in vec2 a_xUV;
layout(location = 1) in vec3 a_xNormal;
layout(location = 2) in vec3 a_xWorldPos;
layout(location = 3) in mat3 a_xTBN;

#ifndef SHADOWS
layout(set = 1, binding = 0) uniform sampler2D g_xDiffuseTex;
layout(set = 1, binding = 1) uniform sampler2D g_xNormalTex;
layout(set = 1, binding = 2) uniform sampler2D g_xRoughnessTex;
layout(set = 1, binding = 3) uniform sampler2D g_xMetallicTex;
#endif

void main(){
	#ifndef SHADOWS
	vec4 xDiffuse = texture(g_xDiffuseTex, a_xUV);
	vec3 xNormal = a_xTBN * (2 * texture(g_xNormalTex, a_xUV).xyz - 1.);
	float fRoughness = texture(g_xRoughnessTex, a_xUV).x;
	float fMetallic = texture(g_xMetallicTex, a_xUV).x;
	
	OutputToGBuffer(xDiffuse, xNormal, 0.2, fRoughness, fMetallic, a_xWorldPos);
	#endif
}