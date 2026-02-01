#version 450 core

#include "../Common.fxh"
#include "../PBRConstants.fxh"

layout(location = 0) out vec4 o_xColour;

// Light data received from vertex shader (flat - per-instance)
layout(location = 0) flat in vec4 v_xPositionRange;
layout(location = 1) flat in vec4 v_xColorIntensity;
layout(location = 2) flat in vec3 v_xDirection;
layout(location = 3) flat in vec4 v_xSpotParams;
layout(location = 4) flat in uint v_uLightType;

// G-buffer textures
layout(set = 0, binding = 2) uniform sampler2D g_xDiffuseTex;
layout(set = 0, binding = 3) uniform sampler2D g_xNormalsAmbientTex;
layout(set = 0, binding = 4) uniform sampler2D g_xMaterialTex;
layout(set = 0, binding = 5) uniform sampler2D g_xDepthTex;

// BRDF LUT for multiscatter energy compensation
// Matches IBL implementation for consistent brightness between ambient and dynamic lights
layout(set = 0, binding = 9) uniform sampler2D g_xBRDFLUT;

// Light type constants (match vertex shader)
const uint LIGHT_TYPE_POINT = 0u;
const uint LIGHT_TYPE_SPOT = 1u;
const uint LIGHT_TYPE_DIRECTIONAL = 2u;

// ============================================================================
// DIRECTION CONVENTION
// ============================================================================
// v_xDirection stores the direction FROM light INTO scene (normalized on CPU).
// For BRDF computation, we need the direction FROM fragment TO light source.
// - For point/spot: computed as normalize(lightPos - fragPos)
// - For directional: negate v_xDirection to get light-to-fragment direction
// ============================================================================

// Threshold for skipping negligible light contributions
const float LIGHT_CONTRIBUTION_THRESHOLD = 0.001;

// Minimum distance clamp to prevent singularity at d=0 (1cm minimum)
const float MIN_LIGHT_DISTANCE = 0.01;

// ============================================================================
// PHYSICALLY-BASED ATTENUATION FUNCTIONS
// Reference: "Real Shading in Unreal Engine 4" (Karis, SIGGRAPH 2013)
// ============================================================================

// Physically-based windowed inverse-square attenuation
// Reference: "Real Shading in Unreal Engine 4" (Karis, SIGGRAPH 2013)
//
// This function provides:
// 1. Physically-correct inverse-square falloff with 4π normalization
// 2. Smooth windowing to zero at the range limit (no hard cutoff artifacts)
// 3. Minimum distance clamp to prevent singularity
//
// The 4π factor comes from the surface area of a sphere (4πr²).
// For a point light emitting I lumens uniformly in all directions,
// the illuminance at distance d is: E = I / (4π * d²) lux
float ComputeAttenuation(float fDistance, float fRange)
{
	// Squared terms for efficiency
	float d2 = fDistance * fDistance;
	float r2 = fRange * fRange;

	// Window function: saturate(1 - (d^2/r^2)^2)^2
	// Creates smooth falloff reaching exactly zero at range
	float fDistanceRatio = d2 / r2;
	float fWindow = clamp(1.0 - fDistanceRatio * fDistanceRatio, 0.0, 1.0);
	fWindow *= fWindow;  // Square for smoother transition

	// Clamp distance to minimum to prevent singularity at d=0
	float fClampedDist = max(fDistance, MIN_LIGHT_DISTANCE);

	// Physical inverse-square law: I / (4π * d²)
	// Window function modulates the result (not applied as divisor) for correct energy conservation
	float fAttenuation = (1.0 / (4.0 * PI * fClampedDist * fClampedDist)) * fWindow;

	return fAttenuation;
}

// Spot light cone falloff (smooth transition between inner and outer angles)
// Direction vectors are already normalized on CPU
float ComputeSpotConeFalloff(vec3 xLightToFrag, vec3 xSpotDir, float fCosInner, float fCosOuter)
{
	// Compare light-to-fragment direction with spot direction
	// Fragment is inside cone when directions align (dot product close to 1)
	float fCosAngle = dot(xLightToFrag, xSpotDir);

	// smoothstep for visually pleasing falloff in the penumbra region
	// fCosOuter < fCosInner (outer angle > inner angle in radians)
	// Epsilon guard prevents undefined behavior when inner == outer (degenerate cone)
	const float fConeEpsilon = 0.001;
	return smoothstep(fCosOuter - fConeEpsilon, fCosInner, fCosAngle);
}

// ============================================================================
// COOK-TORRANCE BRDF (matches deferred shader exactly)
// ============================================================================

vec3 CookTorrance_DynamicLight(
	vec3 xAlbedo,
	vec3 xNormal,
	vec3 xViewDir,
	vec3 xLightDir,
	vec3 xLightColor,
	float fMetallic,
	float fRoughness,
	float fAttenuation)
{
	vec3 xHalfDir = normalize(xLightDir + xViewDir);

	float NdotL = max(dot(xNormal, xLightDir), 0.0);
	float NdotV = max(dot(xNormal, xViewDir), PBR_EPSILON);
	float NdotH = max(dot(xNormal, xHalfDir), 0.0);
	float HdotV = max(dot(xHalfDir, xViewDir), 0.0);

	// Early exit if no light contribution
	if (NdotL <= PBR_EPSILON)
	{
		return vec3(0.0);
	}

	// Clamp roughness to valid range [MIN, 1.0] to prevent artifacts
	float fClampedRoughness = clamp(fRoughness, PBR_MIN_ROUGHNESS, 1.0);

	// Calculate F0 (base reflectivity at normal incidence)
	// Dielectrics have ~0.04, metals use albedo as F0
	vec3 F0 = PBR_DIELECTRIC_F0;
	F0 = mix(F0, xAlbedo, fMetallic);

	// Cook-Torrance BRDF using shared PBR functions from PBRConstants.fxh
	float D = DistributionGGX(NdotH, fClampedRoughness);
	float G = GeometrySmith_Direct(NdotV, NdotL, fClampedRoughness);
	vec3 F = FresnelSchlick(HdotV, F0);

	// Specular BRDF
	vec3 numerator = D * G * F;
	float denominator = 4.0 * NdotV * NdotL;
	vec3 specular = numerator / max(denominator, PBR_EPSILON);

	// ========== MULTISCATTER ENERGY COMPENSATION ==========
	// Use the same MultiscatterBRDF formula as IBL for consistent brightness
	// between dynamic lights and environment lighting.
	// Reference: Fdez-Aguera 2019 "A Multiple-Scattering Microfacet Model"
	float fRoughnessForLUT = clamp(fClampedRoughness, PBR_BRDF_LUT_MIN_ROUGHNESS, PBR_BRDF_LUT_MAX_ROUGHNESS);
	vec2 xBRDF = texture(g_xBRDFLUT, vec2(NdotV, fRoughnessForLUT)).rg;

	// Apply full multiscatter compensation (matches IBL implementation)
	vec3 xMultiscatter = MultiscatterBRDF(F0, NdotV, fRoughnessForLUT, xBRDF);

	// Scale specular by ratio of multiscatter to single-scatter energy
	vec3 xSingleScatter = F0 * xBRDF.x + xBRDF.y;
	specular *= xMultiscatter / max(xSingleScatter, vec3(PBR_EPSILON));

	// Energy conservation - what isn't reflected is refracted
	vec3 kS = F;
	vec3 kD = vec3(1.0) - kS;

	// Metals don't have diffuse lighting
	kD *= (1.0 - fMetallic);

	// Lambertian diffuse with proper energy normalization
	vec3 diffuse = kD * xAlbedo / PI;

	// Combine diffuse and specular with incoming radiance
	vec3 radiance = xLightColor * fAttenuation;
	return (diffuse + specular) * radiance * NdotL;
}

// ============================================================================
// MAIN
// ============================================================================

void main()
{
	// Get screen UV from fragment coordinates
	vec2 xUV = gl_FragCoord.xy * g_xRcpScreenDims;

	float fDepth = texture(g_xDepthTex, xUV).r;

	// Skip sky pixels (depth = 1.0)
	if (fDepth >= 1.0)
	{
		discard;
	}

	// Reconstruct world position from depth
	vec3 xWorldPos = GetWorldPosFromDepthTex(g_xDepthTex, xUV);

	// Sample G-buffer
	vec4 xDiffuse = texture(g_xDiffuseTex, xUV);
	vec4 xNormalAmbient = texture(g_xNormalsAmbientTex, xUV);
	vec4 xMaterial = texture(g_xMaterialTex, xUV);

	// Normalize normal (can become denormalized from interpolation/compression)
	vec3 xNormal = normalize(xNormalAmbient.xyz);
	float fRoughness = xMaterial.x;
	float fMetallic = xMaterial.y;

	vec3 xViewDir = normalize(g_xCamPos_Pad.xyz - xWorldPos);

	// Extract light data from varyings
	vec3 xLightPos = v_xPositionRange.xyz;
	float fRange = v_xPositionRange.w;
	vec3 xLightColor = v_xColorIntensity.xyz;
	float fIntensity = v_xColorIntensity.w;
	// Direction convention: v_xDirection points FROM light INTO scene (normalized on CPU)
	vec3 xLightDirStored = v_xDirection;
	uint uLightType = v_uLightType;

	vec3 xLightDir;
	// fIntensity is luminous power in lumens (point/spot) or lux (directional)
	// ComputeAttenuation() returns 1/(4*PI*d^2) * window, converting flux to irradiance
	float fAttenuation = fIntensity;

	if (uLightType == LIGHT_TYPE_POINT)
	{
		vec3 xToLight = xLightPos - xWorldPos;
		float fDistance = length(xToLight);

		// Light volume geometry should already cull pixels outside range,
		// but add safety check for edge cases
		if (fDistance > fRange)
		{
			discard;
		}

		xLightDir = xToLight / max(fDistance, MIN_LIGHT_DISTANCE);
		fAttenuation *= ComputeAttenuation(fDistance, fRange);
	}
	else if (uLightType == LIGHT_TYPE_SPOT)
	{
		vec3 xToLight = xLightPos - xWorldPos;
		float fDistance = length(xToLight);

		if (fDistance > fRange)
		{
			discard;
		}

		xLightDir = xToLight / max(fDistance, MIN_LIGHT_DISTANCE);

		// Get spot parameters
		float fCosInner = v_xSpotParams.x;
		float fCosOuter = v_xSpotParams.y;

		// Same attenuation as point lights (cone shape handled by falloff below)
		fAttenuation *= ComputeAttenuation(fDistance, fRange);

		// Apply cone falloff
		// xLightDirStored = direction FROM light INTO scene (the way the light points)
		// xLightDir = direction FROM fragment TO light (for BRDF, computed above)
		// For cone test, we need light-to-fragment = -xLightDir
		vec3 xLightToFrag = -xLightDir;
		float fConeFalloff = ComputeSpotConeFalloff(xLightToFrag, xLightDirStored, fCosInner, fCosOuter);
		fAttenuation *= fConeFalloff;
	}
	else  // LIGHT_TYPE_DIRECTIONAL
	{
		// xLightDirStored = direction FROM light INTO scene
		// xLightDir = direction FROM fragment TO light (for BRDF) = negated
		xLightDir = -xLightDirStored;
		// No distance attenuation for directional lights (intensity is already lux)
	}

	// Skip negligible contributions
	if (fAttenuation < LIGHT_CONTRIBUTION_THRESHOLD)
	{
		discard;
	}

	// Compute light contribution
	vec3 xResult = CookTorrance_DynamicLight(
		xDiffuse.rgb,
		xNormal,
		xViewDir,
		xLightDir,
		xLightColor,
		fMetallic,
		fRoughness,
		fAttenuation
	);

	// Output with alpha=0 for additive blending
	o_xColour = vec4(xResult, 0.0);
}
