#ifndef PBR_CONSTANTS_FXH
#define PBR_CONSTANTS_FXH

// ============================================================================
// PHYSICALLY-BASED RENDERING CONSTANTS & COMMON FUNCTIONS
// Zenith Engine - Flux Renderer
//
// All values derived from physics/optics or industry standards (Epic Games, Unity)
// References:
// - Epic Games UE4 PBR: https://blog.selfshadow.com/publications/s2013-shading-course/
// - Frostbite PBR: https://seblagarde.files.wordpress.com/2015/07/course_notes_moving_frostbite_to_pbr_v32.pdf
// ============================================================================

// ---------- Mathematical Constants ----------
const float PI = 3.14159265359;
const float TWO_PI = 6.28318530718;
const float INV_PI = 0.31830988618;

// ---------- PBR Material Constants ----------

// Dielectric F0: Base reflectivity at normal incidence for non-metals
// Physics: Derived from index of refraction (IOR) 1.5 (typical for plastics, glass)
// Formula: F0 = ((n-1)/(n+1))^2 = ((1.5-1)/(1.5+1))^2 = 0.04
const vec3 PBR_DIELECTRIC_F0 = vec3(0.04);

// Fresnel Power: Exponent in Schlick's approximation
// Physics: Schlick's approximation uses pow(1-cosTheta, 5.0)
// This closely matches the exact Fresnel equations for most dielectric materials
const float PBR_FRESNEL_POWER = 5.0;

// Minimum roughness: Prevents numerical issues in GGX distribution
// Technical: At roughness=0, GGX D term becomes a delta function (singularity)
// Industry standards:
//   Unreal Engine 4: 0.015
//   Unity HDRP: 0.045 (higher for mobile compatibility)
//   Frostbite: 0.01-0.02
// Using 0.015 allows mirror-like surfaces (chrome, water) while preventing NaN/Inf
const float PBR_MIN_ROUGHNESS = 0.015;

// GGX minimum roughness for importance sampling
// Technical: Even lower than MIN_ROUGHNESS, used specifically for importance sampling
// where we need to avoid the delta function behavior in the GGX distribution
const float PBR_GGX_MIN_ROUGHNESS = 0.005;

// BRDF LUT sampling range - avoids singularities at extreme roughness values
// Min: Slightly below MIN_ROUGHNESS to ensure smooth sampling across the table
// Max: Avoids numerical instability at maximum roughness
// These are used when sampling the pre-computed BRDF integration LUT
const float PBR_BRDF_LUT_MIN_ROUGHNESS = 0.01;
const float PBR_BRDF_LUT_MAX_ROUGHNESS = 0.99;

// ---------- Geometry Function K-factors ----------
// These INTENTIONALLY differ between direct and IBL lighting per Epic Games specification
//
// Direct/Analytic lighting: k = ((roughness + 1)^2) / 8
// - Used for point lights, directional lights, spot lights
// - The (roughness + 1) remapping accounts for how specular roughness manifests
//   under analytic light sources
//
// IBL (Image-Based Lighting): k = (roughness^2) / 2
// - Used for environment maps and pre-integrated lighting
// - Different formula because IBL represents pre-integrated hemisphere lighting
//   where the roughness remapping is already partially baked in
const float PBR_GEOMETRY_K_DIRECT_DIVISOR = 8.0;
const float PBR_GEOMETRY_K_IBL_DIVISOR = 2.0;

// ---------- Common Epsilon Values ----------
// Used to prevent division by zero and other numerical instabilities
const float PBR_EPSILON = 0.0001;
const float PBR_EPSILON_SMALL = 0.0000001;

// ============================================================================
// COMMON PBR FUNCTIONS
// ============================================================================

// Fresnel-Schlick approximation
// Returns the reflectance at a given angle based on base reflectivity F0
// cosTheta: dot(N, V) or dot(H, V)
// F0: base reflectivity at normal incidence
vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
	return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), PBR_FRESNEL_POWER);
}

// Fresnel-Schlick with roughness modification for IBL ambient lighting
// At grazing angles on rough surfaces, Fresnel effect is reduced
// This modification ensures smooth transitions at roughness boundaries
vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), PBR_FRESNEL_POWER);
}

// ============================================================================
// SMITH'S SCHLICK-GGX GEOMETRY FUNCTION
// Models geometric self-shadowing and masking on microfacet surfaces
// ============================================================================

// Smith's Schlick-GGX for DIRECT/ANALYTIC lights (point, directional, spot)
// Uses k = ((roughness + 1)^2) / 8 per Epic Games specification
// The (roughness + 1) remapping accounts for how specular roughness manifests
// under analytic light sources with precise angular extent
// NdotV: dot(Normal, View) or dot(Normal, Light) - clamped to [0,1]
// roughness: surface roughness [0,1]
float GeometrySchlickGGX_Direct(float NdotV, float roughness)
{
	float r = (roughness + 1.0);
	float k = (r * r) / PBR_GEOMETRY_K_DIRECT_DIVISOR;

	float denom = NdotV * (1.0 - k) + k;
	// Epsilon guard prevents division instability at grazing angles
	// where denominator can approach zero with high roughness values
	return NdotV / max(denom, PBR_EPSILON);
}

// Smith's Schlick-GGX for IBL (Image-Based Lighting / environment maps)
// Uses k = (roughness^2) / 2 per Epic Games specification
// Different formula because IBL represents pre-integrated hemisphere lighting
// where the roughness remapping is already partially baked into the convolution
// NdotV: dot(Normal, View) or dot(Normal, Light) - clamped to [0,1]
// roughness: surface roughness [0,1]
float GeometrySchlickGGX_IBL(float NdotV, float roughness)
{
	float k = (roughness * roughness) / PBR_GEOMETRY_K_IBL_DIVISOR;

	float denom = NdotV * (1.0 - k) + k;
	// Epsilon guard prevents division instability at grazing angles
	return NdotV / max(denom, PBR_EPSILON);
}

// Combined Smith geometry function for view and light directions
// Combines masking (view direction) and shadowing (light direction) terms
// NdotV: dot(Normal, View) - clamped to [0,1]
// NdotL: dot(Normal, Light) - clamped to [0,1]
// roughness: surface roughness [0,1]
float GeometrySmith_Direct(float NdotV, float NdotL, float roughness)
{
	float ggx1 = GeometrySchlickGGX_Direct(NdotV, roughness);
	float ggx2 = GeometrySchlickGGX_Direct(NdotL, roughness);
	return ggx1 * ggx2;
}

float GeometrySmith_IBL(float NdotV, float NdotL, float roughness)
{
	float ggx1 = GeometrySchlickGGX_IBL(NdotV, roughness);
	float ggx2 = GeometrySchlickGGX_IBL(NdotL, roughness);
	return ggx1 * ggx2;
}

// ============================================================================
// IMPORTANCE SAMPLING UTILITIES
// Used for Monte Carlo integration in IBL precomputation and SSR
// ============================================================================

// Van der Corput sequence for quasi-random sampling
// Generates low-discrepancy sequence values for better sampling distribution
// than pure random numbers, reducing noise in Monte Carlo integration
float RadicalInverse_VdC(uint bits)
{
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

// Hammersley sequence for low-discrepancy 2D sampling
// Combines linear sequence with Van der Corput for 2D quasi-random points
// i: sample index
// N: total number of samples
vec2 Hammersley(uint i, uint N)
{
	return vec2(float(i) / float(N), RadicalInverse_VdC(i));
}

// GGX importance sampling - generates sample direction based on GGX distribution
// Returns a half-vector H in world space for use with microfacet BRDF
// Xi: 2D quasi-random sample (from Hammersley)
// N: surface normal
// roughness: surface roughness [0, 1]
vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness)
{
	// Convert roughness to alpha (Disney convention: alpha = roughness^2)
	float a = roughness * roughness;

	// Generate spherical coordinates from quasi-random sample
	// Using GGX distribution: D(h) = alpha^2 / (PI * ((N.H)^2 * (alpha^2 - 1) + 1)^2)
	float phi = TWO_PI * Xi.x;
	float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
	float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

	// Spherical to cartesian (tangent space)
	vec3 H;
	H.x = cos(phi) * sinTheta;
	H.y = sin(phi) * sinTheta;
	H.z = cosTheta;

	// Build tangent-space basis from normal
	// Handle edge case where N is nearly parallel to (0,0,1)
	vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
	vec3 tangent = normalize(cross(up, N));
	vec3 bitangent = cross(N, tangent);

	// Transform from tangent space to world space
	return normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

#endif // PBR_CONSTANTS_FXH
