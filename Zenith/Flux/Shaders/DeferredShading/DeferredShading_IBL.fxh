#ifndef DEFERRED_SHADING_IBL_FXH
#define DEFERRED_SHADING_IBL_FXH

// ============================================================================
// Image-Based Lighting + Screen-Space effects (SSR, SSGI) composition
// Requires: PBRConstants.fxh, Common.fxh, and the following uniforms:
//   g_xBRDFLUT, g_xIrradianceMap, g_xPrefilteredMap, g_xSSRTex, g_xSSGITex
//   g_bIBLEnabled, g_bIBLDiffuseEnabled, g_bIBLSpecularEnabled, g_fIBLIntensity
//   g_bSSREnabled, g_bSSGIEnabled, g_fAmbientFallbackIntensity
// ============================================================================

const float NORMAL_LENGTH_EPSILON = 0.001;
const float SSR_CONFIDENCE_EPSILON = 0.001;

// Compute IBL ambient lighting using cubemap textures
// SSR is blended with IBL specular, SSGI replaces IBL diffuse where confident
vec3 ComputeIBLAmbient(vec3 xNormal, vec3 xViewDir, vec3 xAlbedo, float fMetallic, float fRoughness, float fAmbientOcclusion, vec2 xUV)
{
	float NdotV = max(dot(xNormal, xViewDir), PBR_EPSILON);
	vec3 F0 = PBR_DIELECTRIC_F0;
	F0 = mix(F0, xAlbedo, fMetallic);
	vec3 F = FresnelSchlickRoughness(NdotV, F0, fRoughness);

	// Sample BRDF LUT
	float fRoughnessForLUT = clamp(fRoughness, PBR_BRDF_LUT_MIN_ROUGHNESS, PBR_BRDF_LUT_MAX_ROUGHNESS);
	vec2 xBRDF = texture(g_xBRDFLUT, vec2(NdotV, fRoughnessForLUT)).rg;

	// SSR specular
	vec3 xSSRSpecular = vec3(0.0);
	float fSSRConfidence = 0.0;
	if (g_bSSREnabled != 0)
	{
		vec4 xSSR = texture(g_xSSRTex, xUV);
		fSSRConfidence = xSSR.a;
		vec3 xSSRFresnel = FresnelSchlick(NdotV, F0);
		xSSRSpecular = xSSR.rgb * xSSRFresnel;
	}

	// Simple ambient fallback when IBL is disabled
	if (g_bIBLEnabled == 0)
	{
		vec3 xAmbient = vec3(g_fAmbientFallbackIntensity) * xAlbedo * fAmbientOcclusion;
		vec3 xSSRFinal = xSSRSpecular * fSSRConfidence * fAmbientOcclusion;
		return xAmbient + xSSRFinal;
	}

	// Guard against zero/degenerate normals
	float fNormalLen = length(xNormal);
	if (fNormalLen < NORMAL_LENGTH_EPSILON)
	{
		return vec3(0.0);
	}

	// Energy conservation
	vec3 kS = F;
	vec3 kD = (1.0 - kS) * (1.0 - fMetallic);

	// Diffuse IBL
	vec3 xDiffuseIBL = vec3(0.0);
	if (g_bIBLDiffuseEnabled != 0)
	{
		vec3 xIrradiance = texture(g_xIrradianceMap, xNormal).rgb;
		xDiffuseIBL = kD * xIrradiance * xAlbedo;
	}

	// SSGI diffuse (replaces IBL diffuse where confident)
	vec3 xFinalDiffuse = xDiffuseIBL;
	if (g_bSSGIEnabled != 0)
	{
		vec4 xSSGI = texture(g_xSSGITex, xUV);
		float fSSGIConfidence = xSSGI.a;
		if (fSSGIConfidence > 0.001)
		{
			vec3 xSSGIDiffuse = kD * xSSGI.rgb * xAlbedo;
			xFinalDiffuse = mix(xDiffuseIBL, xSSGIDiffuse, fSSGIConfidence);
		}
	}

	// Specular IBL
	vec3 xSpecularIBL = vec3(0.0);
	if (g_bIBLSpecularEnabled != 0)
	{
		vec3 xReflect = reflect(-xViewDir, xNormal);
		float fLOD = fRoughness * (IBL_PREFILTER_MIP_COUNT - 1.0);
		vec3 xPrefilteredColor = textureLod(g_xPrefilteredMap, xReflect, fLOD).rgb;
		xSpecularIBL = xPrefilteredColor * MultiscatterBRDF(F0, NdotV, fRoughness, xBRDF);
	}

	// SSR/IBL specular blending
	vec3 xFinalSpecular;
	if (g_bSSREnabled != 0 && fSSRConfidence > SSR_CONFIDENCE_EPSILON)
	{
		xFinalSpecular = mix(xSpecularIBL, xSSRSpecular, fSSRConfidence);
	}
	else
	{
		xFinalSpecular = xSpecularIBL;
	}

	// Specular AO remapping (Lagarde/de Rousiers 2014)
	float fSpecularAO = clamp(pow(NdotV + fAmbientOcclusion, exp2(-16.0 * fRoughness - 1.0)) - 1.0 + fAmbientOcclusion, 0.0, 1.0);

	return (xFinalDiffuse * fAmbientOcclusion + xFinalSpecular * fSpecularAO) * g_fIBLIntensity;
}

#endif // DEFERRED_SHADING_IBL_FXH
