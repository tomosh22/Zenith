#version 450 core

#include "../Common.fxh"
#include "../GBufferCommon.fxh"

layout(location = 0) in vec2 a_xUV;
layout(location = 1) in vec3 a_xNormal;
layout(location = 2) in vec3 a_xWorldPos;
layout(location = 3) in mat3 a_xTBN;

layout(set = 1, binding = 0) uniform sampler2D g_xDiffuseTex;
layout(set = 1, binding = 1) uniform sampler2D g_xNormalTex;
layout(set = 1, binding = 2) uniform sampler2D g_xRoughnessTex;
layout(set = 1, binding = 3) uniform sampler2D g_xMetallicTex;

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
	vec4 xDiffuse = texture(g_xDiffuseTex, a_xUV);
	vec3 xNormal = normalize(a_xTBN * texture(g_xNormalTex, a_xUV).xyz);
	float fRoughness = texture(g_xRoughnessTex, a_xUV).x;
	float fMetallic = texture(g_xMetallicTex, a_xUV).x;
	
	Light xLight;
	xLight.m_xPosition_Radius = vec4(0.,0.,0.,2000.);
	xLight.m_xColour = vec4(1.);
	
	OutputToGBuffer(xDiffuse, xNormal, 0.2, fRoughness, fMetallic, a_xWorldPos);
}