#version 450 core

#include "../Common.fxh"

layout(location = 0) out vec4 o_xColour;

layout(location = 0) in vec2 a_xUV;

layout(set = 0, binding = 1) uniform sampler2D g_xDiffuseTex;
layout(set = 0, binding = 2) uniform sampler2D g_xNormalsAmbientTex;
layout(set = 0, binding = 3) uniform sampler2D g_xMaterialTex;
layout(set = 0, binding = 4) uniform sampler2D g_xWorldPosTex;

void CookTorrance_Directional(inout vec4 xFinalColor, vec4 xDiffuse, DirectionalLight xLight, vec3 xNormal, float fMetal, float fRough, float fReflectivity, vec3 xWorldPos) {
	vec3 xLightDir = xLight.m_xDirection.xyz;
	vec3 xViewDir = normalize(g_xCamPos_Pad.xyz - xWorldPos);
	vec3 xHalfDir = normalize(xLightDir + xViewDir);

	float xNormalDotLightDir = max(dot(xNormal, xLightDir), 0.0001);
	float xNormalDotViewDir = max(dot(xNormal, xViewDir), 0.0001);
	float xNormalDotHalfDir = max(dot(xNormal, xHalfDir), 0.0001);
	float xHalfDirDotViewDir = max(dot(xHalfDir, xViewDir), 0.0001);
	

	float fF = fReflectivity + (1 - fReflectivity) * pow((1-xHalfDirDotViewDir), 5.);

	fF = 1. - fF;

	float fD = pow(fRough, 2.) / (3.14 * pow((pow(xNormalDotHalfDir, 2) * (pow(fRough, 2.) - 1.) + 1.), 2.));

	float fK = pow(fRough + 1., 2) / 8.;
	float fG = xNormalDotViewDir / (xNormalDotViewDir * (1 - fK) + fK);

	float fDFG = fD * fF * fG;

	vec4 xSurface = xDiffuse * vec4(xLight.m_xColour.xyz, 1) * xLight.m_xColour.w;
	vec4 xC = xSurface * (1. - fMetal);

	vec4 xBRDF = ((1. - fF) * (xC / 3.14)) + (fDFG / (4. * xNormalDotLightDir * xNormalDotViewDir));

	xFinalColor += xBRDF * xNormalDotLightDir * xLight.m_xColour;
	xFinalColor.a = 1.;
}

void main()
{
	vec4 xDiffuse = texture(g_xDiffuseTex, a_xUV);
	vec4 xMaterial = texture(g_xMaterialTex, a_xUV);
	vec4 xNormalAmbient = texture(g_xNormalsAmbientTex, a_xUV);
	vec3 xNormal = xNormalAmbient.xyz;
	float fAmbient = xNormalAmbient.w;
	vec3 xWorldPos = texture(g_xWorldPosTex, a_xUV).xyz;

	o_xColour.xyz = xDiffuse.xyz * fAmbient;

	DirectionalLight xLight;
	xLight.m_xColour = g_xSunColour;
	xLight.m_xDirection = vec4(-g_xSunDir_Pad.xyz, 0.);

	float fReflectivity = 0.5f;
	if(xLight.m_xColour.a > 0.1)
	{
		CookTorrance_Directional(o_xColour, xDiffuse, xLight, xNormal, xMaterial.y, xMaterial.x, fReflectivity, xWorldPos);
	}
	o_xColour.w = 1.f;
}