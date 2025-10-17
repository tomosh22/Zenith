#version 450 core

#include "../Common.fxh"
#include "../GBufferCommon.fxh"

layout(location = 0) in vec2 a_xUV;
layout(location = 1) in vec3 a_xNormal;
layout(location = 2) in vec3 a_xWorldPos;
layout(location = 3) in mat3 a_xTBN;

layout(set = 1, binding = 1) uniform sampler2D g_xDiffuseTex;
layout(set = 1, binding = 2) uniform sampler2D g_xNormalTex;
layout(set = 1, binding = 3) uniform sampler2D g_xRoughnessMetallicTex;
layout(set = 1, binding = 4) uniform sampler2D g_xOcclusionTex;
layout(set = 1, binding = 5) uniform sampler2D g_xEmissiveTex;

void main(){
	vec4 xDiffuse = texture(g_xDiffuseTex, a_xUV);
	vec3 xNormal = a_xTBN * (2 * texture(g_xNormalTex, a_xUV).xyz - 1.);
	vec2 xRoughnessMetallic = texture(g_xRoughnessMetallicTex, a_xUV).gb;
	
	OutputToGBuffer(xDiffuse, xNormal, 0.2, xRoughnessMetallic.x, xRoughnessMetallic.y);
}