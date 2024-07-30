#version 450 core

#include "../Common.fxh"
#include "../GBufferCommon.fxh"

layout(location = 0) in vec2 a_xUV;
layout(location = 1) in vec3 a_xNormal;
layout(location = 2) in vec3 a_xWorldPos;
layout(location = 3) in float a_fMaterialLerp;

layout(set = 1, binding = 0) uniform sampler2D g_xDiffuseTex0;
layout(set = 1, binding = 1) uniform sampler2D g_xNormalTex0;
layout(set = 1, binding = 2) uniform sampler2D g_xRoughnessTex0;
layout(set = 1, binding = 3) uniform sampler2D g_xMetallicTex0;

layout(set = 1, binding = 4) uniform sampler2D g_xDiffuseTex1;
layout(set = 1, binding = 5) uniform sampler2D g_xNormalTex1;
layout(set = 1, binding = 6) uniform sampler2D g_xRoughnessTex1;
layout(set = 1, binding = 7) uniform sampler2D g_xMetallicTex1;

void main(){
	vec4 xDiffuse0 = texture(g_xDiffuseTex0, a_xUV);
	vec3 xNormal0 = texture(g_xNormalTex0, a_xUV).xyz;
	float fRoughness0 = texture(g_xRoughnessTex0, a_xUV).x;
	float fMetallic0 = texture(g_xMetallicTex0, a_xUV).x;
	
	vec4 xDiffuse1 = texture(g_xDiffuseTex1, a_xUV);
	vec3 xNormal1 = texture(g_xNormalTex1, a_xUV).xyz;
	float fRoughness1 = texture(g_xRoughnessTex1, a_xUV).x;
	float fMetallic1 = texture(g_xMetallicTex1, a_xUV).x;
	
	vec4 xDiffuse = mix(xDiffuse0, xDiffuse1, a_fMaterialLerp);
	vec3 xNormal = mix(xNormal0, xNormal1, a_fMaterialLerp);
	float fRoughness = mix(fRoughness0, fRoughness1, a_fMaterialLerp);
	float fMetallic = mix(fMetallic0, fMetallic1, a_fMaterialLerp);

	OutputToGBuffer(xDiffuse, xNormal, 0.2, fRoughness, fMetallic, a_xWorldPos);
}