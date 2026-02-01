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

// ---------- IBL Configuration ----------
// IBL Reference Height: Fixed altitude (meters above sea level) for IBL precomputation
//
// DESIGN TRADE-OFF:
// - Using camera height would require re-convolving IBL textures every frame (~costly)
// - Fixed height provides consistent ambient lighting regardless of camera altitude
// - This is intentional: IBL represents pre-computed environment, not real-time camera view
//
// SCENE GUIDANCE:
// - 100m (default): Standard for ground-level outdoor scenes, human-scale environments
// - 1000m-10000m: High-altitude scenes (aircraft, mountainous terrain)
// - 10m-50m: Indoor scenes with skylights, smaller scale environments
//
// Note: This affects atmospheric scattering color at the IBL sampling point.
// For most games at ground level, 100m provides natural sky colors without
// excessive atmospheric haze that would occur at higher altitudes.
const float IBL_REFERENCE_HEIGHT_METERS = 100.0;

// ---------- Geometry Function K-factors ----------
// These INTENTIONALLY differ between direct and IBL lighting per Epic Games specification
//
// MATHEMATICAL BACKGROUND:
// The Smith geometry function models microfacet self-shadowing/masking:
//   G_SchlickGGX(NdotV, k) = NdotV / (NdotV * (1-k) + k)
// The k parameter controls shadowing severity based on incoming light distribution.
//
// DIRECT/ANALYTIC LIGHTING: k = ((roughness + 1)^2) / 8
// - Used for point lights, directional lights, spot lights
// - The (r+1) term is Disney's roughness remapping: prevents overly dark edges at low
//   roughness by ensuring some self-shadowing even on smooth surfaces
// - Division by 8 comes from hemisphere integration geometry
// - Reference: Burley 2012, "Physically-Based Shading at Disney"
//
// IBL (Image-Based Lighting): k = (roughness^2) / 2
// - Used for environment maps and pre-integrated lighting
// - No (r+1) remapping because the convolution already accounts for the
//   roughness-dependent lobe distribution across the hemisphere
// - Division by 2 (vs 8) because IBL samples represent pre-integrated light
//   from multiple directions, reducing the effective shadowing term
// - Reference: Karis 2013, "Real Shading in UE4", SIGGRAPH Course Notes
//
const float PBR_GEOMETRY_K_DIRECT_DIVISOR = 8.0;
const float PBR_GEOMETRY_K_IBL_DIVISOR = 2.0;

// ---------- IBL Prefilter Configuration ----------
// Prefilter mip levels: 7 mips (128->64->32->16->8->4->2 pixels per face)
// Mip 0 = roughness 0 (mirror), Mip 6 = roughness 1 (fully rough)
// Linear mapping: mipLevel = roughness * (IBL_PREFILTER_MIP_COUNT - 1)
const float IBL_PREFILTER_MIP_COUNT = 7.0;

// Smooth surface blend threshold for IBL prefilter
// At roughness below this, blend with mirror reflection to avoid GGX aliasing
// when the distribution approaches a delta function
// Industry standard range: 0.02-0.08
const float IBL_SMOOTH_BLEND_THRESHOLD = 0.05;

// ---------- Common Epsilon Values ----------
// Used to prevent division by zero and other numerical instabilities
//
// PBR_EPSILON: General-purpose epsilon for most division guards
// PBR_EPSILON_SMALL: Very small epsilon for high-precision calculations
// PBR_DIRECTION_EPSILON: Minimum magnitude for valid direction vectors (screen-space effects)
//
// Note: GGX-specific epsilons may differ per-shader based on tuning. The deferred shader
// uses 0.0001 for GGX denominator (tuned for grazing angle stability), while importance
// sampling may use smaller values for precision.
const float PBR_EPSILON = 0.0001;
const float PBR_EPSILON_SMALL = 0.0000001;
const float PBR_DIRECTION_EPSILON = 0.0001;  // Minimum direction magnitude for SSR/SSGI

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
// GGX/TROWBRIDGE-REITZ NORMAL DISTRIBUTION FUNCTION
// Describes the statistical distribution of microfacet orientations
// ============================================================================

// GGX Normal Distribution Function (NDF)
// Returns the probability that microfacets are oriented to reflect light in the half-vector direction
// NdotH: dot(Normal, HalfVector) - clamped to [0,1]
// roughness: surface roughness [0,1]
//
// The GGX (Trowbridge-Reitz) distribution is industry-standard for real-time PBR:
// - Longer tail than Beckmann, producing more realistic highlights on rough surfaces
// - Used by Unreal Engine, Unity, Frostbite, and most modern renderers
float DistributionGGX(float NdotH, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH2 = NdotH * NdotH;

	float denom = (NdotH2 * (a2 - 1.0) + 1.0);
	denom = PI * denom * denom;

	return a2 / max(denom, PBR_EPSILON);
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
// MULTISCATTER BRDF APPROXIMATION
// Accounts for energy lost in single-scatter GGX at high roughness
// Reference: Fdez-Aguera 2019 "A Multiple-Scattering Microfacet Model"
// ============================================================================

// Multiscatter BRDF: Compensates for energy loss in single-scatter GGX
// At high roughness, single-scatter Cook-Torrance loses significant energy
// because light that scatters multiple times between microfacets isn't accounted for.
// This approximation recovers that lost energy, making rough metals appear correctly bright.
//
// F0: Base reflectivity at normal incidence
// NdotV: dot(Normal, ViewDir)
// roughness: Surface roughness [0, 1]
// brdfLUT: Pre-computed BRDF integration LUT sample (scale, bias)
//
// Returns: Corrected specular term to use instead of (F0 * brdfLUT.x + brdfLUT.y)
vec3 MultiscatterBRDF(vec3 F0, float NdotV, float roughness, vec2 brdfLUT)
{
	// Single-scatter energy
	vec3 FssEss = F0 * brdfLUT.x + brdfLUT.y;

	// Total single-scatter albedo
	float Ess = brdfLUT.x + brdfLUT.y;

	// Energy lost to multiple scattering
	float Ems = 1.0 - Ess;

	// Average Fresnel (approximation for hemisphere integral of F)
	//
	// MATHEMATICAL DERIVATION:
	// Favg = integral of F_schlick(theta) * sin(2*theta) over hemisphere
	//      = integral_0^(pi/2) [F0 + (1-F0)(1-cos(theta))^5] * sin(2*theta) d(theta)
	//
	// The integral of (1-cos(theta))^5 * sin(2*theta) from 0 to pi/2 evaluates to 1/21:
	//   integral_0^(pi/2) (1-cos(theta))^5 * 2*sin(theta)*cos(theta) d(theta) = 1/21
	//
	// Therefore: Favg = F0 + (1-F0)/21
	//
	// For dielectrics F0=0.04: Favg = 0.04 + 0.96/21 ≈ 0.086
	// For metals F0≈albedo: Favg ≈ F0 + (1-F0)/21
	//
	// Reference: Fdez-Aguera 2019, "A Multiple-Scattering Microfacet Model"
	// https://www.jcgt.org/published/0008/01/03/
	vec3 Favg = F0 + (1.0 - F0) / 21.0;

	// Multiple-scatter contribution
	// This is the energy that bounces multiple times and eventually exits
	vec3 Fms = FssEss * Favg / (1.0 - Ems * Favg);

	// Total energy: single-scatter + multi-scatter
	return FssEss + Fms * Ems;
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
