#version 450 core

#include "../Common.fxh"
#include "../PBRConstants.fxh"

layout(location = 0) out vec4 o_xColour;

layout(location = 0) in vec2 a_xUV;

// ========== UNIFORM BINDINGS ==========

layout(std140, set = 0, binding = 1) uniform DeferredShadingConstants
{
	uint g_bVisualiseCSMs;
	uint g_bIBLEnabled;
	uint g_uDebugMode;  // 0=normal, 1=show cyan, 2=show depth, 3=show diffuse
	uint g_bIBLDiffuseEnabled;
	uint g_bIBLSpecularEnabled;
	float g_fIBLIntensity;
	uint g_bShowBRDFLUT;
	uint g_bForceRoughness;
	float g_fForcedRoughness;
	uint g_bSSREnabled;
	uint g_bSSGIEnabled;
	float g_fAmbientFallbackIntensity;
};

//#TO_TODO: these should really all be in one buffer
layout(std140, set = 0, binding=2) uniform ShadowMatrix0{ mat4 g_xShadowMat0; };
layout(std140, set = 0, binding=3) uniform ShadowMatrix1{ mat4 g_xShadowMat1; };
layout(std140, set = 0, binding=4) uniform ShadowMatrix2{ mat4 g_xShadowMat2; };
layout(std140, set = 0, binding=5) uniform ShadowMatrix3{ mat4 g_xShadowMat3; };

layout(set = 0, binding = 6) uniform sampler2D g_xDiffuseTex;
layout(set = 0, binding = 7) uniform sampler2D g_xNormalsAmbientTex;
layout(set = 0, binding = 8) uniform sampler2D g_xMaterialTex;
layout(set = 0, binding = 9) uniform sampler2D g_xDepthTex;

//#TO_TODO: texture arrays
layout(set = 0, binding = 10) uniform sampler2D g_xCSM0;
layout(set = 0, binding = 11) uniform sampler2D g_xCSM1;
layout(set = 0, binding = 12) uniform sampler2D g_xCSM2;
layout(set = 0, binding = 13) uniform sampler2D g_xCSM3;

layout(set = 0, binding = 14) uniform sampler2D g_xBRDFLUT;
layout(set = 0, binding = 15) uniform samplerCube g_xIrradianceMap;
layout(set = 0, binding = 16) uniform samplerCube g_xPrefilteredMap;
layout(set = 0, binding = 17) uniform sampler2D g_xSSRTex;
layout(set = 0, binding = 18) uniform sampler2D g_xSSGITex;

// ========== COMPONENT HEADERS (after bindings, since functions reference uniforms) ==========

#include "DeferredShading_Lighting.fxh"
#include "DeferredShading_Shadows.fxh"
#include "DeferredShading_IBL.fxh"

// ========== DEBUG DISPLAY CONSTANTS ==========

const float BRDF_LUT_DISPLAY_SIZE = 0.2;
const float BRDF_LUT_DISPLAY_MARGIN = 0.02;
const float LIGHT_INTENSITY_THRESHOLD = 0.1;

// ========== main() ==========

void main()
{
	// BRDF LUT overlay display
	if (g_bShowBRDFLUT != 0u)
	{
		float fSize = BRDF_LUT_DISPLAY_SIZE;
		float fMargin = BRDF_LUT_DISPLAY_MARGIN;
		float fMinX = 1.0 - fMargin - fSize;
		float fMaxX = 1.0 - fMargin;
		float fMinY = 1.0 - fMargin - fSize;
		float fMaxY = 1.0 - fMargin;

		bool bInRegion = (a_xUV.x > fMinX) && (a_xUV.x < fMaxX) && (a_xUV.y > fMinY) && (a_xUV.y < fMaxY);
		if (bInRegion)
		{
			vec2 xLutUV = vec2(
				(a_xUV.x - fMinX) / fSize,
				(a_xUV.y - fMinY) / fSize
			);
			vec2 xBRDF = texture(g_xBRDFLUT, xLutUV).rg;
			o_xColour = vec4(xBRDF.r, xBRDF.g, 0.0, 1.0);
			return;
		}
	}

	// Debug modes
	if (g_uDebugMode == 1u)
	{
		o_xColour = vec4(0.0, 1.0, 1.0, 1.0);
		return;
	}

	vec4 xDiffuse = texture(g_xDiffuseTex, a_xUV);
	float fDepth = texture(g_xDepthTex, a_xUV).r;

	if (g_uDebugMode == 2u)
	{
		if (fDepth >= 1.0)
			o_xColour = vec4(1.0, 0.0, 0.0, 1.0);
		else
			o_xColour = vec4(vec3(1.0 - fDepth), 1.0);
		return;
	}

	if (g_uDebugMode == 3u)
	{
		o_xColour = vec4(xDiffuse.rgb, 1.0);
		return;
	}

	// Sky pixels pass through
	if(fDepth == 1.0f)
	{
		o_xColour = xDiffuse;
		return;
	}

	// G-Buffer decode
	vec4 xMaterial = texture(g_xMaterialTex, a_xUV);
	vec4 xNormalAmbient = texture(g_xNormalsAmbientTex, a_xUV);
	vec3 xNormal = normalize(xNormalAmbient.xyz);
	float fAmbient = xNormalAmbient.w;

	vec3 xWorldPos = GetWorldPosFromDepthTex(g_xDepthTex, a_xUV);

	float fRoughness = xMaterial.x;
	float fMetallic = xMaterial.y;
	float fEmissive = xMaterial.z;

	if (g_bForceRoughness != 0u)
	{
		fRoughness = g_fForcedRoughness;
	}

	vec3 xViewDir = normalize(g_xCamPos_Pad.xyz - xWorldPos);

	// IBL + SSR + SSGI ambient
	vec3 xIBLAmbient = ComputeIBLAmbient(xNormal, xViewDir, xDiffuse.rgb, fMetallic, fRoughness, fAmbient, a_xUV);
	o_xColour.rgb = xIBLAmbient;

	// Direct lighting
	DirectionalLight xLight;
	xLight.m_xColour = g_xSunColour;
	xLight.m_xDirection = vec4(-g_xSunDir_Pad.xyz, 0.);

	// Cascaded shadow mapping
	const uint uNumCSMs = 4;
	mat4 axShadowMats[uNumCSMs] = {g_xShadowMat0, g_xShadowMat1, g_xShadowMat2, g_xShadowMat3};

	float fShadowFactor = 1.0;
	vec3 xLightDir = normalize(xLight.m_xDirection.xyz);
	float fNdotL = max(dot(xNormal, xLightDir), 0.0);

	vec2 texelSize = 1.0 / textureSize(g_xCSM0, 0);

	for(int iCascade = 0; iCascade < int(uNumCSMs); iCascade++)
	{
		vec4 xShadowSpace = axShadowMats[iCascade] * vec4(xWorldPos, 1);
		vec2 xSamplePos = xShadowSpace.xy / xShadowSpace.w * 0.5 + 0.5;
		float fCurrentDepth = xShadowSpace.z / xShadowSpace.w;

		if(xSamplePos.x < 0 || xSamplePos.x > 1 || xSamplePos.y < 0 || xSamplePos.y > 1)
			continue;
		if(fCurrentDepth < 0 || fCurrentDepth > 1.0)
			continue;

		if(g_bVisualiseCSMs > 0)
		{
			o_xColour = vec4(1.f / int(uNumCSMs)) * iCascade;
			return;
		}

		float fBias = max(SHADOW_MIN_BIAS, SHADOW_MAX_BIAS * (1.0 - fNdotL));
		float fBiasedDepth = fCurrentDepth - fBias;

		vec2 xEdgeDist = min(xSamplePos, 1.0 - xSamplePos);
		float fMinEdgeDist = min(xEdgeDist.x, xEdgeDist.y);
		bool bInBlendZone = (fMinEdgeDist < CASCADE_BLEND_DISTANCE) && (iCascade < int(uNumCSMs) - 1);

		float fShadow1 = SampleCascadeShadow(iCascade, xSamplePos, fBiasedDepth, texelSize);

		if (bInBlendZone)
		{
			float fBlendWeight = 1.0 - (fMinEdgeDist / CASCADE_BLEND_DISTANCE);
			vec4 xNextShadowSpace = axShadowMats[iCascade + 1] * vec4(xWorldPos, 1);
			vec2 xNextSamplePos = xNextShadowSpace.xy / xNextShadowSpace.w * 0.5 + 0.5;
			float fNextDepth = xNextShadowSpace.z / xNextShadowSpace.w;

			if (xNextSamplePos.x >= 0.0 && xNextSamplePos.x <= 1.0 &&
				xNextSamplePos.y >= 0.0 && xNextSamplePos.y <= 1.0 &&
				fNextDepth >= 0.0 && fNextDepth <= 1.0)
			{
				float fNextBiasedDepth = fNextDepth - fBias;
				float fShadow2 = SampleCascadeShadow(iCascade + 1, xNextSamplePos, fNextBiasedDepth, texelSize);
				fShadowFactor = mix(fShadow1, fShadow2, fBlendWeight);
			}
			else
			{
				fShadowFactor = fShadow1;
			}
		}
		else
		{
			fShadowFactor = fShadow1;
		}

		break;
	}

	// Apply shadow factor to direct lighting
	if(fNdotL > 0.0)
	{
		if(xLight.m_xColour.a > LIGHT_INTENSITY_THRESHOLD)
		{
			CookTorrance_Directional(o_xColour, xDiffuse, xLight, xNormal, fMetallic, fRoughness, xWorldPos);
		}
		o_xColour.rgb = xIBLAmbient + (o_xColour.rgb - xIBLAmbient) * fShadowFactor;
	}

	// Emissive (not affected by shadows or lighting)
	o_xColour.rgb += vec3(fEmissive);
	o_xColour.w = 1.f;
}
