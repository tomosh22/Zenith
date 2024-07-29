#version 450 core

#include "Common.h"

layout(location = 0) out vec4 o_xColour;

layout(location = 0) in vec2 a_xUV;
layout(location = 1) in vec3 a_xNormal;
layout(location = 2) in vec3 a_xWorldPos;
layout(location = 3) in float a_fMaterialLerp;
layout(location = 4) in mat3 a_xTBN;

layout(set = 1, binding = 0) uniform sampler2D g_xDiffuseTex0;
layout(set = 1, binding = 1) uniform sampler2D g_xNormalTex0;
layout(set = 1, binding = 2) uniform sampler2D g_xRoughnessTex0;
layout(set = 1, binding = 3) uniform sampler2D g_xMetallicTex0;

layout(set = 1, binding = 4) uniform sampler2D g_xDiffuseTex1;
layout(set = 1, binding = 5) uniform sampler2D g_xNormalTex1;
layout(set = 1, binding = 6) uniform sampler2D g_xRoughnessTex1;
layout(set = 1, binding = 7) uniform sampler2D g_xMetallicTex1;

struct Light
{
	vec4 m_xPosition_Radius;
	vec4 m_xColour;
};

void CookTorrance_Point(inout vec4 xFinalColor, vec4 xDiffuse, Light xLight, vec3 xNormal, float fMetal, float fRough, float fReflectivity) {
	vec3 xLightDir = normalize(xLight.m_xPosition_Radius.xyz - a_xWorldPos);
	vec3 xViewDir = normalize(g_xCamPos_Pad.xyz - a_xWorldPos);
	vec3 xHalfDir = normalize(xLightDir + xViewDir);

	float xNormalDotLightDir = max(dot(xNormal, xLightDir), 0.0001);
	float xNormalDotViewDir = max(dot(xNormal, xViewDir), 0.0001);
	float xNormalDotHalfDir = max(dot(xNormal, xHalfDir), 0.0001);
	float xHalfDirDotViewDir = max(dot(xHalfDir, xViewDir), 0.0001);
	

	float fF = fReflectivity + (1 - fReflectivity) * pow((1-xHalfDirDotViewDir), 5);

	fF = 1. - fF;
	


	float fD = pow(fRough, 2) / (3.14 * pow((pow(xNormalDotHalfDir, 2) * (pow(fRough, 2) - 1) + 1), 2));

	

	float fK = pow(fRough + 1., 2) / 8;
	float fG = xNormalDotViewDir / (xNormalDotViewDir * (1 - fK) + fK);

	float fDFG = fD * fF * fG;

	vec4 xSurface = xDiffuse * vec4(xLight.m_xColour.xyz, 1) * xLight.m_xColour.w;
	vec4 xC = xSurface * (1 - fMetal);

	vec4 xBRDF = ((1 - fF) * (xC / 3.14)) + (fDFG / (4 * xNormalDotLightDir * xNormalDotViewDir));

	float dist = length(xLight.m_xPosition_Radius.xyz - a_xWorldPos);
	float fAtten = 1. - clamp(dist / xLight.m_xPosition_Radius.w , 0., 1.);

	xFinalColor += xBRDF * xNormalDotLightDir * fAtten * xLight.m_xColour;
	xFinalColor.a = 1.;
}

void main(){
	vec4 xDiffuse0 = texture(g_xDiffuseTex0, a_xUV);
	vec3 xNormal0 = normalize(a_xTBN * texture(g_xNormalTex0, a_xUV).xyz);
	float fRoughness0 = texture(g_xRoughnessTex0, a_xUV).x;
	float fMetallic0 = texture(g_xMetallicTex0, a_xUV).x;
	
	vec4 xDiffuse1 = texture(g_xDiffuseTex1, a_xUV);
	vec3 xNormal1 = normalize(a_xTBN * texture(g_xNormalTex1, a_xUV).xyz);
	float fRoughness1 = texture(g_xRoughnessTex1, a_xUV).x;
	float fMetallic1 = texture(g_xMetallicTex1, a_xUV).x;
	
	vec4 xDiffuse = mix(xDiffuse0, xDiffuse1, a_fMaterialLerp);
	vec3 xNormal = mix(xNormal0, xNormal1, a_fMaterialLerp);
	float fRoughness = mix(fRoughness0, fRoughness1, a_fMaterialLerp);
	float fMetallic = mix(fMetallic0, fMetallic1, a_fMaterialLerp);
	
	Light xLight;
	xLight.m_xPosition_Radius = vec4(g_xCamPos_Pad.xyz,10000.);
	xLight.m_xColour = vec4(1.);
	
	o_xColour = vec4(0.);
	//CookTorrance_Point(o_xColour, xDiffuse, xLight, xNormal, fMetallic, fRoughness, 0.9f);
}