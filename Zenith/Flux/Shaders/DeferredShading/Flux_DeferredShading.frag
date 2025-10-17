#version 450 core

#include "../Common.fxh"

layout(location = 0) out vec4 o_xColour;

layout(location = 0) in vec2 a_xUV;

//#TO_TODO: these should really all be in one buffer
layout(std140, set = 0, binding=1) uniform ShadowMatrix0{
	mat4 g_xShadowMat0;
};
layout(std140, set = 0, binding=2) uniform ShadowMatrix1{
	mat4 g_xShadowMat1;
};
layout(std140, set = 0, binding=3) uniform ShadowMatrix2{
	mat4 g_xShadowMat2;
};
layout(std140, set = 0, binding=4) uniform ShadowMatrix3{
	mat4 g_xShadowMat3;
};

layout(set = 0, binding = 5) uniform sampler2D g_xDiffuseTex;
layout(set = 0, binding = 6) uniform sampler2D g_xNormalsAmbientTex;
layout(set = 0, binding = 7) uniform sampler2D g_xMaterialTex;
layout(set = 0, binding = 8) uniform sampler2D g_xWorldPosTex;
layout(set = 0, binding = 9) uniform sampler2D g_xDepthTex;

//#TO_TODO: texture arrays
layout(set = 0, binding = 10) uniform sampler2D g_xCSM0;
layout(set = 0, binding = 11) uniform sampler2D g_xCSM1;
layout(set = 0, binding = 12) uniform sampler2D g_xCSM2;
layout(set = 0, binding = 13) uniform sampler2D g_xCSM3;

#define HandleShadow(uIndex)

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
	
	if(texture(g_xDepthTex, a_xUV).r == 1.0f)
	{
		o_xColour = xDiffuse;
		return;
	}
	
	vec4 xMaterial = texture(g_xMaterialTex, a_xUV);
	vec4 xNormalAmbient = texture(g_xNormalsAmbientTex, a_xUV);
	vec3 xNormal = xNormalAmbient.xyz;
	float fAmbient = xNormalAmbient.w;
	vec3 xWorldPos = texture(g_xWorldPosTex, a_xUV).xyz;
	
	o_xColour.xyz = xDiffuse.xyz * fAmbient;

	DirectionalLight xLight;
	xLight.m_xColour = g_xSunColour;
	xLight.m_xDirection = vec4(-g_xSunDir_Pad.xyz, 0.);
	
	const uint uNumCSMs = 4;
	mat4 axShadowMats[uNumCSMs] = {g_xShadowMat0, g_xShadowMat1, g_xShadowMat2, g_xShadowMat3};
	
	bool bInShadow = false;
	
	// Try cascades in order from highest quality (0) to lowest (2)
	// Break when we find one that contains the fragment
	for(int iCascade = 0; iCascade < int(uNumCSMs); iCascade++)
	{
		// Transform to shadow space for this cascade
		vec4 xShadowSpace = axShadowMats[iCascade] * vec4(xWorldPos, 1);
		vec2 xSamplePos = xShadowSpace.xy / xShadowSpace.w * 0.5 + 0.5;
		float fCurrentDepth = xShadowSpace.z / xShadowSpace.w;
		
		// Check if sample position is within valid bounds [0, 1]
		if(xSamplePos.x < 0 || xSamplePos.x > 1 || xSamplePos.y < 0 || xSamplePos.y > 1)
		{
			continue; // Try next cascade
		}
		
		// Check if depth is within valid range
		if(fCurrentDepth < 0 || fCurrentDepth > 1.0)
		{
			continue; // Try next cascade
		}
		
		// Sample the appropriate shadow map
		float fShadowDepth = 0.0;
		if(iCascade == 0)
		{
			fShadowDepth = texture(g_xCSM0, xSamplePos).x;
		}
		else if(iCascade == 1)
		{
			fShadowDepth = texture(g_xCSM1, xSamplePos).x;
		}
		else if(iCascade == 2)
		{
			fShadowDepth = texture(g_xCSM2, xSamplePos).x;
		}
		else if(iCascade == 3)
		{
			fShadowDepth = texture(g_xCSM3, xSamplePos).x;
		}
		
		// Use larger bias for distant cascades (lower resolution per world unit)
		float fBias = 0.0005 * (1.0 + float(iCascade) * 0.5);
		
		// Depth comparison with adaptive bias
		if(fCurrentDepth > fShadowDepth + fBias)
		{
			bInShadow = true;
		}
		
		// Found a valid cascade, stop searching
		break;
	}
	
	if(bInShadow)
	{
		o_xColour.w = 1.f;
		return;
	}
	
	

	float fReflectivity = 0.5f;
	if(xLight.m_xColour.a > 0.1)
	{
		CookTorrance_Directional(o_xColour, xDiffuse, xLight, xNormal, xMaterial.y, xMaterial.x, fReflectivity, xWorldPos);
	}
	o_xColour.w = 1.f;
}