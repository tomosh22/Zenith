// IBL Common Functions
// Helper functions for Image-Based Lighting sampling
//
// Note: Core PBR constants (PI, FresnelSchlick functions) are now centralized
// in PBRConstants.fxh for consistency across all shaders.

#include "../PBRConstants.fxh"

// Sample environment map with roughness-based LOD
// Assumes cubemap with 5 mip levels (0=smooth, 4=rough)
vec3 SamplePrefilteredEnvMap(samplerCube envMap, vec3 R, float roughness, float maxMip)
{
	float mipLevel = roughness * maxMip;
	return textureLod(envMap, R, mipLevel).rgb;
}

// Compute diffuse IBL contribution
vec3 ComputeDiffuseIBL(
	samplerCube irradianceMap,
	vec3 N,
	vec3 albedo,
	float metallic,
	float ao,
	float intensity)
{
	vec3 irradiance = texture(irradianceMap, N).rgb;

	// Diffuse is reduced by metallic (metals don't have diffuse)
	vec3 kD = (1.0 - metallic) * albedo;

	return kD * irradiance * ao * intensity;
}

// Compute specular IBL contribution
vec3 ComputeSpecularIBL(
	samplerCube prefilteredMap,
	sampler2D brdfLUT,
	vec3 N,
	vec3 V,
	vec3 F0,
	float roughness,
	float ao,
	float intensity,
	float maxMip)
{
	float NdotV = max(dot(N, V), 0.0);
	vec3 R = reflect(-V, N);

	// Sample prefiltered environment
	vec3 prefilteredColor = SamplePrefilteredEnvMap(prefilteredMap, R, roughness, maxMip);

	// Sample BRDF LUT
	vec2 brdf = texture(brdfLUT, vec2(NdotV, roughness)).rg;

	// Split-Sum approximation: F0 * scale + bias
	// Note: Fresnel is NOT applied here because the BRDF LUT's bias term (brdf.y)
	// already encodes Fresnel variation across viewing angles.
	// Using FresnelSchlickRoughness() here would double-apply the Fresnel effect.
	vec3 specular = prefilteredColor * (F0 * brdf.x + brdf.y);

	return specular * ao * intensity;
}

// Full IBL contribution (diffuse + specular)
vec3 ComputeIBL(
	samplerCube irradianceMap,
	samplerCube prefilteredMap,
	sampler2D brdfLUT,
	vec3 N,
	vec3 V,
	vec3 albedo,
	float metallic,
	float roughness,
	float ao,
	float intensity,
	float maxMip,
	bool bDiffuseEnabled,
	bool bSpecularEnabled)
{
	// F0 (reflectance at normal incidence)
	// Dielectrics: 0.04 (4% reflection)
	// Metals: use albedo as F0
	vec3 F0 = mix(PBR_DIELECTRIC_F0, albedo, metallic);

	vec3 result = vec3(0.0);

	if (bDiffuseEnabled)
	{
		result += ComputeDiffuseIBL(irradianceMap, N, albedo, metallic, ao, intensity);
	}

	if (bSpecularEnabled)
	{
		result += ComputeSpecularIBL(prefilteredMap, brdfLUT, N, V, F0, roughness, ao, intensity, maxMip);
	}

	return result;
}

// Simplified IBL for 2D textures (when cubemaps aren't available)
// Uses view direction to sample 2D panorama or fallback color
vec3 ComputeIBLFallback(
	sampler2D brdfLUT,
	vec3 N,
	vec3 V,
	vec3 albedo,
	float metallic,
	float roughness,
	float ao,
	vec3 skyColor,
	float intensity)
{
	float NdotV = max(dot(N, V), 0.0);

	// F0 (reflectance at normal incidence)
	vec3 F0 = mix(PBR_DIELECTRIC_F0, albedo, metallic);

	// Sample BRDF LUT
	vec2 brdf = texture(brdfLUT, vec2(NdotV, roughness)).rg;

	// Simple hemisphere approximation
	// Diffuse: sky color from above
	float fNdotUp = max(dot(N, vec3(0.0, 1.0, 0.0)), 0.0);
	vec3 diffuseIBL = skyColor * fNdotUp * (1.0 - metallic) * albedo;

	// Specular: sky color in reflection direction
	vec3 R = reflect(-V, N);
	float fRdotUp = max(dot(R, vec3(0.0, 1.0, 0.0)), 0.0);
	vec3 specularColor = mix(vec3(0.1), skyColor, fRdotUp);
	// Split-Sum: F0 * scale + bias (bias encodes Fresnel variation)
	vec3 specularIBL = specularColor * (F0 * brdf.x + brdf.y);

	return (diffuseIBL + specularIBL) * ao * intensity;
}
