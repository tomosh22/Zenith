#ifndef DEFERRED_SHADING_LIGHTING_FXH
#define DEFERRED_SHADING_LIGHTING_FXH

// ============================================================================
// Cook-Torrance PBR direct lighting
// Requires: PBRConstants.fxh included before this header
// ============================================================================

// Epsilon values for numerical stability in BRDF calculations
const float DEFERRED_EPSILON = PBR_EPSILON;
const float DEFERRED_DENOM_EPSILON = PBR_EPSILON;

void CookTorrance_Directional(inout vec4 xFinalColor, vec4 xDiffuse, DirectionalLight xLight, vec3 xNormal, float fMetal, float fRough, vec3 xWorldPos) {
	vec3 xLightDir = normalize(xLight.m_xDirection.xyz);
	vec3 xViewDir = normalize(g_xCamPos_Pad.xyz - xWorldPos);
	vec3 xHalfDir = normalize(xLightDir + xViewDir);

	float NdotL = max(dot(xNormal, xLightDir), 0.0);
	float NdotV = max(dot(xNormal, xViewDir), DEFERRED_EPSILON);
	float NdotH = max(dot(xNormal, xHalfDir), 0.0);
	float HdotV = max(dot(xHalfDir, xViewDir), 0.0);

	if(NdotL <= DEFERRED_EPSILON) {
		return;
	}

	float roughness = max(fRough, PBR_MIN_ROUGHNESS);

	vec3 F0 = PBR_DIELECTRIC_F0;
	F0 = mix(F0, xDiffuse.rgb, fMetal);

	float D = DistributionGGX(NdotH, roughness);
	float G = GeometrySmith_Direct(NdotV, NdotL, roughness);
	vec3 F = FresnelSchlick(HdotV, F0);

	vec3 numerator = D * G * F;
	float denominator = 4.0 * NdotV * NdotL;
	vec3 specular = numerator / max(denominator, DEFERRED_DENOM_EPSILON);

	vec3 kS = F;
	vec3 kD = vec3(1.0) - kS;
	kD *= (1.0 - fMetal);

	vec3 diffuse = kD * xDiffuse.rgb / PI;
	vec3 radiance = xLight.m_xColour.rgb * xLight.m_xColour.a;
	vec3 Lo = (diffuse + specular) * radiance * NdotL;

	xFinalColor.rgb += Lo;
	xFinalColor.a = 1.0;
}

#endif // DEFERRED_SHADING_LIGHTING_FXH
