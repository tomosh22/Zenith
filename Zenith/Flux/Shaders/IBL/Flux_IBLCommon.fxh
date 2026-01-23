// IBL Common Functions
// Helper functions for Image-Based Lighting sampling

const float IBL_PI = 3.14159265359;

// Fresnel-Schlick approximation
vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
	return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Fresnel-Schlick with roughness (for IBL)
vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

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

	// Fresnel with roughness
	vec3 F = FresnelSchlickRoughness(NdotV, F0, roughness);

	// Combine
	vec3 specular = prefilteredColor * (F * brdf.x + brdf.y);

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
	vec3 F0 = mix(vec3(0.04), albedo, metallic);

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
	vec3 F0 = mix(vec3(0.04), albedo, metallic);

	// Sample BRDF LUT
	vec2 brdf = texture(brdfLUT, vec2(NdotV, roughness)).rg;

	// Fresnel
	vec3 F = FresnelSchlickRoughness(NdotV, F0, roughness);

	// Simple hemisphere approximation
	// Diffuse: sky color from above
	float fNdotUp = max(dot(N, vec3(0.0, 1.0, 0.0)), 0.0);
	vec3 diffuseIBL = skyColor * fNdotUp * (1.0 - metallic) * albedo;

	// Specular: sky color in reflection direction
	vec3 R = reflect(-V, N);
	float fRdotUp = max(dot(R, vec3(0.0, 1.0, 0.0)), 0.0);
	vec3 specularColor = mix(vec3(0.1), skyColor, fRdotUp);
	vec3 specularIBL = specularColor * (F * brdf.x + brdf.y);

	return (diffuseIBL + specularIBL) * ao * intensity;
}
