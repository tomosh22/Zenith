#version 450 core

#include "../Common.fxh"
#include "../PBRConstants.fxh"

layout(location = 0) out vec4 o_xColour;

layout(location = 0) in vec2 a_xUV;

// Scratch buffer for push constants replacement
layout(std140, set = 0, binding = 1) uniform DeferredShadingConstants
{
	uint g_bVisualiseCSMs;
	uint g_bIBLEnabled;
	uint g_uDebugMode;  // 0=normal, 1=show cyan (verify running), 2=show depth, 3=show diffuse
	uint g_bIBLDiffuseEnabled;
	uint g_bIBLSpecularEnabled;
	float g_fIBLIntensity;
	uint g_bShowBRDFLUT;
	uint g_bForceRoughness;
	float g_fForcedRoughness;
	uint g_bSSREnabled;
	uint g_bSSGIEnabled;
	float g_fAmbientFallbackIntensity;  // Configurable ambient when IBL disabled (default 0.03)
};

//#TO_TODO: these should really all be in one buffer
layout(std140, set = 0, binding=2) uniform ShadowMatrix0{
	mat4 g_xShadowMat0;
};
layout(std140, set = 0, binding=3) uniform ShadowMatrix1{
	mat4 g_xShadowMat1;
};
layout(std140, set = 0, binding=4) uniform ShadowMatrix2{
	mat4 g_xShadowMat2;
};
layout(std140, set = 0, binding=5) uniform ShadowMatrix3{
	mat4 g_xShadowMat3;
};

layout(set = 0, binding = 6) uniform sampler2D g_xDiffuseTex;
layout(set = 0, binding = 7) uniform sampler2D g_xNormalsAmbientTex;
layout(set = 0, binding = 8) uniform sampler2D g_xMaterialTex;
layout(set = 0, binding = 9) uniform sampler2D g_xDepthTex;

//#TO_TODO: texture arrays
layout(set = 0, binding = 10) uniform sampler2D g_xCSM0;
layout(set = 0, binding = 11) uniform sampler2D g_xCSM1;
layout(set = 0, binding = 12) uniform sampler2D g_xCSM2;
layout(set = 0, binding = 13) uniform sampler2D g_xCSM3;

// IBL textures
layout(set = 0, binding = 14) uniform sampler2D g_xBRDFLUT;
layout(set = 0, binding = 15) uniform samplerCube g_xIrradianceMap;
layout(set = 0, binding = 16) uniform samplerCube g_xPrefilteredMap;

// SSR texture (RGB = reflected color, A = confidence)
layout(set = 0, binding = 17) uniform sampler2D g_xSSRTex;

// SSGI texture (RGB = indirect diffuse color, A = confidence)
layout(set = 0, binding = 18) uniform sampler2D g_xSSGITex;


// ========== DEFERRED SHADING CONFIGURATION CONSTANTS ==========
// Note: Core PBR constants (PI, PBR_DIELECTRIC_F0, FRESNEL_POWER, PBR_MIN_ROUGHNESS, FresnelSchlick functions)
// are now centralized in PBRConstants.fxh for consistency across all shaders.

// BRDF LUT sampling range now defined in PBRConstants.fxh as:
// PBR_BRDF_LUT_MIN_ROUGHNESS = 0.01 and PBR_BRDF_LUT_MAX_ROUGHNESS = 0.99

// Epsilon values
// NUMERICAL STABILITY: These values balance precision with stability at grazing angles
// Note: Core epsilon values are defined in PBRConstants.fxh (PBR_EPSILON = 0.0001)
// Deferred shader uses matching values but keeps local definitions for specific tuning
const float DEFERRED_EPSILON = PBR_EPSILON;          // General epsilon for divisions (matches PBRConstants.fxh)
const float DEFERRED_EPSILON_SMALL = 0.0001;         // GGX epsilon - prevents underflow at grazing angles (was 1e-7, too small)
const float DEFERRED_DENOM_EPSILON = PBR_EPSILON;    // Specular denominator epsilon (matches PBRConstants.fxh)
const float NORMAL_LENGTH_EPSILON = 0.001;           // Minimum normal length (10x epsilon for normal robustness)
const float SSR_CONFIDENCE_EPSILON = 0.001;          // Minimum SSR confidence for blending

// Geometry GGX K-factor for DIRECT/ANALYTIC lighting (per Epic Games UE4 PBR):
// k = ((roughness + 1)^2) / 8
// This INTENTIONALLY differs from IBL k = (roughness^2)/2 because environment lighting
// uses pre-integrated hemisphere sampling with different roughness remapping.
// See: https://blog.selfshadow.com/publications/s2013-shading-course/ (Karis, Real Shading in UE4)
const float GEOMETRY_K_DIVISOR = PBR_GEOMETRY_K_DIRECT_DIVISOR;

// Ambient fallback (when IBL disabled)
// PHYSICAL BASIS: Approximates minimal ambient contribution when environment map unavailable.
// This fallback ONLY activates when IBL is disabled (debug mode or missing environment map).
// When IBL is enabled, irradiance map provides physically-accurate ambient from environment.
// Value range guidance (configured via g_fAmbientFallbackIntensity uniform):
//   0.01 - Very dark (moonlit/caves), near-black ambient fill
//   0.03 - Dark interior (default), minimal fill light for visibility
//   0.05 - Overcast exterior, subtle ambient contribution
//   0.10 - Bright overcast, noticeable ambient fill
// Now configurable at runtime via DeferredShadingConstants.g_fAmbientFallbackIntensity

// Shadow PCF bias - tuned for 2048x2048 shadow maps at typical outdoor scene scale
// Min bias: prevents self-shadowing on surfaces facing the light (perpendicular)
// Max bias: used at grazing angles to prevent shadow acne on nearly-parallel surfaces
// For different shadow map resolutions, scale proportionally (e.g., 4096: halve values, 1024: double)
// These are in NDC units relative to the shadow map depth range
const float SHADOW_MIN_BIAS = 0.0005;                // NDC units, perpendicular surfaces
const float SHADOW_MAX_BIAS = 0.005;                 // NDC units, grazing angles (~10x min)
const float PCF_OFFSET_INNER = 0.5;                  // Inner PCF sample offset
const float PCF_OFFSET_OUTER = 1.5;                  // Outer PCF sample offset
const float PCF_SAMPLE_COUNT = 16.0;                 // Total PCF samples (4 gathers * 4 each)

// Cascade blending (AAA feature to eliminate visible seams)
// PHYSICAL BASIS: 15% blend zone is industry standard for smooth cascade transitions.
// Unreal Engine 4 uses 10-20% depending on cascade size and scene scale.
// Lower values (5-10%) give sharper but more visible transitions.
// Higher values (20-30%) reduce artifacts but waste shadow map resolution.
// See: "Cascaded Shadow Maps" (NVIDIA GPU Gems 3, Chapter 10)
const float CASCADE_BLEND_DISTANCE = 0.15;           // Blend over 15% of cascade edge

// Light thresholds
const float LIGHT_INTENSITY_THRESHOLD = 0.1;         // Minimum light intensity for contribution

// BRDF LUT debug display
const float BRDF_LUT_DISPLAY_SIZE = 0.2;            // Size of debug display (20% of screen)
const float BRDF_LUT_DISPLAY_MARGIN = 0.02;         // Margin from screen edge

// Note: FresnelSchlick() and FresnelSchlickRoughness() are now in PBRConstants.fxh

// GGX/Trowbridge-Reitz Normal Distribution Function
float DistributionGGX(float NdotH, float roughness) {
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH2 = NdotH * NdotH;

	float denom = (NdotH2 * (a2 - 1.0) + 1.0);
	denom = PI * denom * denom;

	return a2 / max(denom, DEFERRED_EPSILON_SMALL);
}

// Note: GeometrySchlickGGX_Direct() and GeometrySmith_Direct() are now centralized in PBRConstants.fxh
// for consistency across all shaders. The Direct variants use k = ((r+1)²)/8 per Epic Games specification.

void CookTorrance_Directional(inout vec4 xFinalColor, vec4 xDiffuse, DirectionalLight xLight, vec3 xNormal, float fMetal, float fRough, vec3 xWorldPos) {
	// Light direction is FROM the sun (already pointing away from sun towards surface)
	vec3 xLightDir = normalize(xLight.m_xDirection.xyz);
	vec3 xViewDir = normalize(g_xCamPos_Pad.xyz - xWorldPos);
	vec3 xHalfDir = normalize(xLightDir + xViewDir);

	float NdotL = max(dot(xNormal, xLightDir), 0.0);
	float NdotV = max(dot(xNormal, xViewDir), DEFERRED_EPSILON); // Prevent division by zero
	float NdotH = max(dot(xNormal, xHalfDir), 0.0);
	float HdotV = max(dot(xHalfDir, xViewDir), 0.0);

	// Early exit if no light contribution
	if(NdotL <= DEFERRED_EPSILON) {
		return;
	}

	// Clamp roughness to prevent artifacts
	float roughness = max(fRough, PBR_MIN_ROUGHNESS);

	// Calculate F0 (base reflectivity at normal incidence)
	// Dielectrics have ~0.04, metals use albedo as F0
	vec3 F0 = PBR_DIELECTRIC_F0;
	F0 = mix(F0, xDiffuse.rgb, fMetal);

	// Cook-Torrance BRDF components
	float D = DistributionGGX(NdotH, roughness);
	float G = GeometrySmith_Direct(NdotV, NdotL, roughness);
	vec3 F = FresnelSchlick(HdotV, F0);

	// Specular BRDF
	vec3 numerator = D * G * F;
	float denominator = 4.0 * NdotV * NdotL;
	vec3 specular = numerator / max(denominator, DEFERRED_DENOM_EPSILON);
	
	// Energy conservation - what isn't reflected is refracted
	vec3 kS = F; // Specular contribution
	vec3 kD = vec3(1.0) - kS; // Diffuse contribution
	
	// Metals don't have diffuse lighting
	kD *= (1.0 - fMetal);
	
	// Lambertian diffuse with proper energy normalization
	// Division by PI ensures energy conservation (hemisphere integral = 1)
	// Note: Disney/Burley diffuse was considered but Lambertian provides
	// sufficient visual quality for real-time rendering at lower ALU cost
	vec3 diffuse = kD * xDiffuse.rgb / PI;
	
	// Combine diffuse and specular with incoming radiance
	vec3 radiance = xLight.m_xColour.rgb * xLight.m_xColour.a;
	vec3 Lo = (diffuse + specular) * radiance * NdotL;
	
	xFinalColor.rgb += Lo;
	xFinalColor.a = 1.0;
}

// ========== SHADOW SAMPLING HELPER ==========
// Sample a single cascade's shadow with PCF16 filtering
// Returns shadow factor (1.0 = fully lit, 0.0 = fully shadowed)
float SampleCascadeShadow(int iCascade, vec2 xSamplePos, float fBiasedDepth, vec2 texelSize)
{
	float fShadow = 0.0;

	// 4x4 optimized PCF using 4 textureGather calls
	vec2 axOffsets[4] = vec2[4](
		vec2(-PCF_OFFSET_OUTER, -PCF_OFFSET_OUTER) * texelSize,
		vec2( PCF_OFFSET_INNER, -PCF_OFFSET_OUTER) * texelSize,
		vec2(-PCF_OFFSET_OUTER,  PCF_OFFSET_INNER) * texelSize,
		vec2( PCF_OFFSET_INNER,  PCF_OFFSET_INNER) * texelSize
	);

	for (int i = 0; i < 4; i++)
	{
		vec2 xSampleUV = xSamplePos + axOffsets[i];
		vec4 shadowDepths;

		if(iCascade == 0)
			shadowDepths = textureGather(g_xCSM0, xSampleUV, 0);
		else if(iCascade == 1)
			shadowDepths = textureGather(g_xCSM1, xSampleUV, 0);
		else if(iCascade == 2)
			shadowDepths = textureGather(g_xCSM2, xSampleUV, 0);
		else
			shadowDepths = textureGather(g_xCSM3, xSampleUV, 0);

		// Compare and accumulate
		vec4 comparison = step(vec4(fBiasedDepth), shadowDepths);
		fShadow += dot(comparison, vec4(1.0));
	}

	// Average samples for smooth shadow edges
	return fShadow / PCF_SAMPLE_COUNT;
}

// ACES Filmic Tone Mapping
vec3 ACESFilm(vec3 x) {
	float a = 2.51;
	float b = 0.03;
	float c = 2.43;
	float d = 0.59;
	float e = 0.14;
	return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// Reinhard Tone Mapping (simpler alternative)
vec3 ReinhardToneMapping(vec3 color) {
	return color / (color + vec3(1.0));
}

// Uncharted 2 Tone Mapping
vec3 Uncharted2Tonemap(vec3 x) {
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

// IBL_PREFILTER_MIP_COUNT is now defined in PBRConstants.fxh (7.0)

// Compute IBL ambient lighting using cubemap textures
// AAA-quality: SSR is blended with IBL specular using proper BRDF weighting
vec3 ComputeIBLAmbient(vec3 xNormal, vec3 xViewDir, vec3 xAlbedo, float fMetallic, float fRoughness, float fAmbientOcclusion, vec2 xUV)
{
	// Calculate F0 and Fresnel (needed for IBL and SSR)
	float NdotV = max(dot(xNormal, xViewDir), 0.0);
	vec3 F0 = PBR_DIELECTRIC_F0;
	F0 = mix(F0, xAlbedo, fMetallic);
	vec3 F = FresnelSchlickRoughness(NdotV, F0, fRoughness);

	// Sample BRDF LUT for specular contribution
	float fRoughnessForLUT = clamp(fRoughness, PBR_BRDF_LUT_MIN_ROUGHNESS, PBR_BRDF_LUT_MAX_ROUGHNESS);
	vec2 xBRDF = texture(g_xBRDFLUT, vec2(NdotV, fRoughnessForLUT)).rg;

	// ========== SSR SPECULAR (Physically Correct) ==========
	// SSR provides direct reflected radiance from the scene (single ray, not hemisphere integral)
	// Unlike IBL which requires BRDF integration (F * scale + bias), SSR is direct reflection
	//
	// Use Fresnel-Schlick for physically correct reflectance:
	// - Dielectrics: F0 = 0.04 (4% at normal incidence, increasing at grazing angles)
	// - Metals: F0 = albedo (high reflectance at all angles)
	// This is physically accurate - dielectric reflections ARE weak at normal incidence
	vec3 xSSRSpecular = vec3(0.0);
	float fSSRConfidence = 0.0;
	if (g_bSSREnabled != 0)
	{
		vec4 xSSR = texture(g_xSSRTex, xUV);
		fSSRConfidence = xSSR.a;

		// SSR is direct reflected radiance (single ray sample from scene)
		// Fresnel weighting matches IBL specular for seamless confidence blending
		vec3 xSSRFresnel = FresnelSchlickRoughness(NdotV, F0, fRoughness);
		xSSRSpecular = xSSR.rgb * xSSRFresnel;
	}

	// Simple ambient fallback when IBL is disabled
	if (g_bIBLEnabled == 0)
	{
		// Return basic ambient + properly weighted SSR
		vec3 xAmbient = vec3(g_fAmbientFallbackIntensity) * xAlbedo * fAmbientOcclusion;
		vec3 xSSRFinal = xSSRSpecular * fSSRConfidence * fAmbientOcclusion;
		return xAmbient + xSSRFinal;
	}

	// Guard against zero/degenerate normals (e.g., from sky pixels that slip through)
	float fNormalLen = length(xNormal);
	if (fNormalLen < NORMAL_LENGTH_EPSILON)
	{
		return vec3(0.0);
	}
	xNormal = xNormal / fNormalLen;

	// Energy conservation
	vec3 kS = F;
	vec3 kD = (1.0 - kS) * (1.0 - fMetallic);

	// Diffuse IBL - sample irradiance cubemap using normal direction
	vec3 xDiffuseIBL = vec3(0.0);
	if (g_bIBLDiffuseEnabled != 0)
	{
		vec3 xIrradiance = texture(g_xIrradianceMap, xNormal).rgb;
		xDiffuseIBL = kD * xIrradiance * xAlbedo;
	}

	// ========== SSGI DIFFUSE (Screen-Space Global Illumination) ==========
	// SSGI provides indirect diffuse lighting from nearby on-screen geometry
	// It REPLACES (not adds to) IBL diffuse where SSGI has valid data
	// This is physically correct: SSGI is more accurate for local bounces,
	// while IBL provides the distant environment contribution
	vec3 xFinalDiffuse = xDiffuseIBL;
	if (g_bSSGIEnabled != 0)
	{
		vec4 xSSGI = texture(g_xSSGITex, xUV);
		float fSSGIConfidence = xSSGI.a;

		if (fSSGIConfidence > 0.001)
		{
			// SSGI provides pre-weighted indirect radiance
			// Apply kD for energy conservation with specular
			vec3 xSSGIDiffuse = kD * xSSGI.rgb * xAlbedo;

			// Blend SSGI with IBL diffuse based on confidence
			// High confidence = use SSGI, low confidence = use IBL
			xFinalDiffuse = mix(xDiffuseIBL, xSSGIDiffuse, fSSGIConfidence);
		}
	}

	// Specular IBL - sample prefiltered cubemap using reflection direction
	vec3 xSpecularIBL = vec3(0.0);
	if (g_bIBLSpecularEnabled != 0)
	{
		vec3 xReflect = reflect(-xViewDir, xNormal);
		// LOD based on roughness
		float fLOD = fRoughness * (IBL_PREFILTER_MIP_COUNT - 1.0);
		vec3 xPrefilteredColor = textureLod(g_xPrefilteredMap, xReflect, fLOD).rgb;
		// Multiscatter BRDF: Recovers energy lost in single-scatter GGX at high roughness
		// Standard split-sum (F0 * scale + bias) loses energy because it only accounts for
		// single-scatter events. MultiscatterBRDF adds the contribution from light that
		// bounces multiple times between microfacets before exiting.
		// Reference: Fdez-Aguera 2019 "A Multiple-Scattering Microfacet Model"
		// Reference: Epic Games "Real Shading in UE4" (Karis, SIGGRAPH 2013)
		xSpecularIBL = xPrefilteredColor * MultiscatterBRDF(F0, NdotV, fRoughness, xBRDF);
	}

	// ========== AAA SSR/IBL BLENDING ==========
	// Blend SSR with IBL specular based on confidence
	// Both use the same BRDF formula so they blend seamlessly
	vec3 xFinalSpecular;
	if (g_bSSREnabled != 0 && fSSRConfidence > SSR_CONFIDENCE_EPSILON)
	{
		// Lerp between IBL specular and SSR based on confidence
		// High confidence = use SSR, low confidence = use IBL
		xFinalSpecular = mix(xSpecularIBL, xSSRSpecular, fSSRConfidence);
	}
	else
	{
		xFinalSpecular = xSpecularIBL;
	}

	// Specular AO remapping (Lagarde/de Rousiers 2014 - "Moving Frostbite to PBR")
	// Smooth surfaces reflect from a narrow cone, so ambient occlusion affects them less.
	// Rough surfaces spread specular wider, approaching diffuse AO behavior.
	// Formula: specularAO = saturate(pow(NdotV + AO, exp2(-16*roughness - 1)) - 1 + AO)
	float fSpecularAO = clamp(pow(NdotV + fAmbientOcclusion, exp2(-16.0 * fRoughness - 1.0)) - 1.0 + fAmbientOcclusion, 0.0, 1.0);

	// Combine diffuse (IBL + SSGI blend) with final specular, apply AO (different for diffuse vs specular)
	return (xFinalDiffuse * fAmbientOcclusion + xFinalSpecular * fSpecularAO) * g_fIBLIntensity;
}

void main()
{
	// Show BRDF LUT as overlay in bottom-right corner
	if (g_bShowBRDFLUT != 0u)
	{
		// Display region: bottom-right corner, configurable size
		float fSize = BRDF_LUT_DISPLAY_SIZE;
		float fMargin = BRDF_LUT_DISPLAY_MARGIN;
		float fMinX = 1.0 - fMargin - fSize;
		float fMaxX = 1.0 - fMargin;
		float fMinY = 1.0 - fMargin - fSize;
		float fMaxY = 1.0 - fMargin;

		// Check if we're in the BRDF LUT display region
		bool bInRegion = (a_xUV.x > fMinX) && (a_xUV.x < fMaxX) && (a_xUV.y > fMinY) && (a_xUV.y < fMaxY);
		if (bInRegion)
		{
			// Map screen UV to BRDF LUT UV
			// X axis = NdotV (0 to 1), Y axis = roughness (0 to 1)
			vec2 xLutUV = vec2(
				(a_xUV.x - fMinX) / fSize,
				(a_xUV.y - fMinY) / fSize
			);

			// Sample BRDF LUT - RG channels contain scale and bias
			vec2 xBRDF = texture(g_xBRDFLUT, xLutUV).rg;

			// Visualize: R = scale (red), G = bias (green)
			o_xColour = vec4(xBRDF.r, xBRDF.g, 0.0, 1.0);
			return;
		}
	}

	// Debug mode 1: Output cyan to verify deferred shading is running
	if (g_uDebugMode == 1u)
	{
		o_xColour = vec4(0.0, 1.0, 1.0, 1.0); // Cyan
		return;
	}

	vec4 xDiffuse = texture(g_xDiffuseTex, a_xUV);
	float fDepth = texture(g_xDepthTex, a_xUV).r;

	// Debug mode 2: Visualize depth values
	if (g_uDebugMode == 2u)
	{
		// Show depth as grayscale (near=white, far=black, sky=red)
		if (fDepth >= 1.0)
			o_xColour = vec4(1.0, 0.0, 0.0, 1.0); // Red for sky/clear
		else
			o_xColour = vec4(vec3(1.0 - fDepth), 1.0); // Invert so near is brighter
		return;
	}

	// Debug mode 3: Visualize diffuse G-buffer
	if (g_uDebugMode == 3u)
	{
		o_xColour = vec4(xDiffuse.rgb, 1.0);
		return;
	}

	if(fDepth == 1.0f)
	{
		o_xColour = xDiffuse;
		return;
	}
	
	vec4 xMaterial = texture(g_xMaterialTex, a_xUV);
	vec4 xNormalAmbient = texture(g_xNormalsAmbientTex, a_xUV);
	// CRITICAL: Normals stored in G-buffer can become denormalized due to interpolation,
	// compression, or precision loss. Always normalize before use in lighting calculations.
	vec3 xNormal = normalize(xNormalAmbient.xyz);
	float fAmbient = xNormalAmbient.w;
	
	vec3 xWorldPos = GetWorldPosFromDepthTex(g_xDepthTex, a_xUV);
	
	float fRoughness = xMaterial.x;
	float fMetallic = xMaterial.y;
	float fEmissive = xMaterial.z;  // Emissive luminance from G-Buffer

	// Override roughness if force roughness is enabled (for IBL debugging)
	if (g_bForceRoughness != 0u)
	{
		fRoughness = g_fForcedRoughness;
	}
	
	// Calculate view direction for IBL
	vec3 xViewDir = normalize(g_xCamPos_Pad.xyz - xWorldPos);

	// Compute IBL ambient lighting (replaces simple ambient approximation)
	// SSR is blended with IBL specular when available
	vec3 xIBLAmbient = ComputeIBLAmbient(xNormal, xViewDir, xDiffuse.rgb, fMetallic, fRoughness, fAmbient, a_xUV);

	o_xColour.rgb = xIBLAmbient;

	DirectionalLight xLight;
	xLight.m_xColour = g_xSunColour;
	xLight.m_xDirection = vec4(-g_xSunDir_Pad.xyz, 0.);
	
	const uint uNumCSMs = 4;
	mat4 axShadowMats[uNumCSMs] = {g_xShadowMat0, g_xShadowMat1, g_xShadowMat2, g_xShadowMat3};
	
	float fShadowFactor = 1.0; // 1.0 = fully lit, 0.0 = fully shadowed
	
	// Calculate normal-based bias to prevent shadow acne
	vec3 xLightDir = normalize(xLight.m_xDirection.xyz);
	float fNdotL = max(dot(xNormal, xLightDir), 0.0);
	
	// ========== AAA CASCADE SHADOW BLENDING ==========
	// Try cascades in order from highest quality (0) to lowest (3)
	// Blend between cascades at edges to eliminate visible seams
	vec2 texelSize = 1.0 / textureSize(g_xCSM0, 0);

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

		if(g_bVisualiseCSMs > 0)
		{
			o_xColour = vec4(1.f / int(uNumCSMs)) * iCascade;
			return;
		}

		// Adaptive bias based on surface angle to light
		// Higher bias when surface is nearly parallel to light (grazing angles)
		float fBias = max(SHADOW_MIN_BIAS, SHADOW_MAX_BIAS * (1.0 - fNdotL));
		float fBiasedDepth = fCurrentDepth - fBias;

		// Calculate distance from cascade edge (for blending)
		vec2 xEdgeDist = min(xSamplePos, 1.0 - xSamplePos);
		float fMinEdgeDist = min(xEdgeDist.x, xEdgeDist.y);

		// Check if we're in the blend zone and there's a next cascade
		bool bInBlendZone = (fMinEdgeDist < CASCADE_BLEND_DISTANCE) && (iCascade < int(uNumCSMs) - 1);

		// Sample primary cascade
		float fShadow1 = SampleCascadeShadow(iCascade, xSamplePos, fBiasedDepth, texelSize);

		if (bInBlendZone)
		{
			// Calculate blend weight (0 at center, 1 at edge)
			float fBlendWeight = 1.0 - (fMinEdgeDist / CASCADE_BLEND_DISTANCE);

			// Check if next cascade is valid
			vec4 xNextShadowSpace = axShadowMats[iCascade + 1] * vec4(xWorldPos, 1);
			vec2 xNextSamplePos = xNextShadowSpace.xy / xNextShadowSpace.w * 0.5 + 0.5;
			float fNextDepth = xNextShadowSpace.z / xNextShadowSpace.w;

			// Only blend if next cascade is valid
			if (xNextSamplePos.x >= 0.0 && xNextSamplePos.x <= 1.0 &&
				xNextSamplePos.y >= 0.0 && xNextSamplePos.y <= 1.0 &&
				fNextDepth >= 0.0 && fNextDepth <= 1.0)
			{
				float fNextBiasedDepth = fNextDepth - fBias;
				float fShadow2 = SampleCascadeShadow(iCascade + 1, xNextSamplePos, fNextBiasedDepth, texelSize);

				// Smooth blend between cascades
				fShadowFactor = mix(fShadow1, fShadow2, fBlendWeight);
			}
			else
			{
				// Next cascade invalid, use only primary
				fShadowFactor = fShadow1;
			}
		}
		else
		{
			fShadowFactor = fShadow1;
		}

		// Found a valid cascade, stop searching
		break;
	}
	
	// Apply shadow factor to lighting
	// Don't apply shadows if the surface is facing away from light
	if(fNdotL > 0.0)
	{
		if(xLight.m_xColour.a > LIGHT_INTENSITY_THRESHOLD)
		{
			CookTorrance_Directional(o_xColour, xDiffuse, xLight, xNormal, fMetallic, fRoughness, xWorldPos);
		}

		// Modulate lighting by shadow factor
		// fShadowFactor: 1.0 = fully lit, 0.0 = fully shadowed
		// IBL ambient is not affected by shadows (it's indirect lighting)
		//
		// DESIGN CHOICE: AO is NOT applied to direct lighting, only to IBL (see ComputeIBLAmbient).
		// Rationale: Direct lights have explicit shadow maps for occlusion. AO represents
		// small-scale geometric self-shadowing that primarily affects ambient/indirect lighting.
		// Applying AO to direct light can cause over-darkening in crevices that are directly lit.
		// If desired, uncomment: fShadowFactor *= fAmbient;
		o_xColour.rgb = xIBLAmbient + (o_xColour.rgb - xIBLAmbient) * fShadowFactor;
	}

	// Add emissive contribution (not affected by shadows or lighting)
	// Emissive acts as self-illumination from the surface itself
	o_xColour.rgb += vec3(fEmissive);

	o_xColour.w = 1.f;
}