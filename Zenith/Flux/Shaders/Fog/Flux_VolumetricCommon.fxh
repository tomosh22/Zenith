// Flux_VolumetricCommon.fxh - Shared utilities for volumetric rendering
// Beer-Lambert law, phase functions, froxel coordinate conversions

#ifndef FLUX_VOLUMETRIC_COMMON_FXH
#define FLUX_VOLUMETRIC_COMMON_FXH

#include "../PBRConstants.fxh"

// ============================================================================
// Physical Constants and Parameters
// ============================================================================

// Ambient irradiance ratio: fraction of sky light vs direct sun contribution to fog
// Physical basis: Clear sky ambient ~0.15-0.25, overcast ~0.4-0.6
//
// NOTE: This constant is now configurable at runtime via Flux_VolumeFogConstants.
// For shaders that have access to the VolumeFog uniform buffer, use:
//   u_xVolFogConstants.m_fAmbientIrradianceRatio
//
// This default is provided for shaders that don't have the uniform buffer bound.
// See: Flux_VolumeFog.h Flux_VolumeFogConstants struct
const float FOG_AMBIENT_IRRADIANCE_RATIO_DEFAULT = 0.25;

// ============================================================================
// Volumetric Fog Numerical Thresholds
// These pragmatic values balance quality vs performance for fog rendering
// ============================================================================

// Density skip threshold: Optimization to skip near-empty samples
// Physical basis: Beer-Lambert transmittance for density 0.001 over 1m is ~99.9%
// At this level, fog contribution is imperceptible but computation still occurs
const float FOG_DENSITY_SKIP_THRESHOLD = 0.001;

// Transmittance early-exit threshold: Stop marching when fully opaque
// Physical basis: Transmittance 0.01 = 1% light passes through = effectively opaque
// Lower values give marginally better quality in dense fog edges
const float FOG_TRANSMITTANCE_EARLY_EXIT = 0.01;

// Noise coordinate scale: Maps world-space to noise texture UV
// This value is NOT physically based - it's an artistic control for fog "scale"
// 0.01 = fog features are ~100 units wide (suitable for large outdoor scenes)
//
// NOTE: This constant is now configurable at runtime via Flux_VolumeFogConstants.
// For shaders that have access to the VolumeFog uniform buffer, use:
//   u_fNoiseWorldScale (in shader uniform block)
//
// This default is provided for shaders that don't have the uniform buffer bound.
// See: Flux_VolumeFog.h Flux_VolumeFogConstants struct
const float FOG_NOISE_WORLD_SCALE_DEFAULT = 0.01;

// ============================================================================
// UI-to-Physical Density Conversion
// ============================================================================

// UI DENSITY SCALING
// Artists work with intuitive 0.0-0.01 range in debug UI.
// Physical extinction coefficients are in inverse meters (m^-1).
// This scale converts UI values to physical units:
//   UI 0.01 * 100 = 1.0 m^-1 (dense fog, ~1m visibility)
//   UI 0.001 * 100 = 0.1 m^-1 (light fog, ~10m visibility)
const float FOG_UI_DENSITY_SCALE = 100.0;

// Helper function for consistent density conversion across all fog techniques
float UIToPhysicalDensity(float uiDensity)
{
    return uiDensity * FOG_UI_DENSITY_SCALE;
}

// ============================================================================
// Beer-Lambert Extinction
// ============================================================================

// Basic Beer-Lambert law: transmittance = e^(-density * distance)
float BeerLambert(float density, float distance)
{
    return exp(-density * distance);
}

// Beer-Lambert with separate scattering and absorption coefficients
// sigma_t (extinction) = sigma_s (scattering) + sigma_a (absorption)
float BeerLambertExtinction(float sigmaS, float sigmaA, float distance)
{
    float sigmaT = sigmaS + sigmaA;
    return exp(-sigmaT * distance);
}

// Powder effect for clouds - reduces darkness in center of dense volumes
// Based on Horizon Zero Dawn presentation
float BeerLambertPowder(float density, float distance, float powder)
{
    float beer = BeerLambert(density, distance);
    float beerPowder = BeerLambert(density * powder, distance);
    return mix(beer, beerPowder, 0.5);
}

// ============================================================================
// Phase Functions (scattering direction distribution)
// ============================================================================

// Henyey-Greenstein phase function
// g = asymmetry parameter: -1 (backscatter) to 1 (forward scatter), 0 = isotropic
float HenyeyGreenstein(float cosTheta, float g)
{
    float g2 = g * g;
    float denom = 1.0 + g2 - 2.0 * g * cosTheta;
    return (1.0 - g2) / (4.0 * PI * pow(denom, 1.5));
}

// Schlick approximation of Henyey-Greenstein (faster)
float SchlickPhase(float cosTheta, float g)
{
    float k = 1.55 * g - 0.55 * g * g * g;
    float kcosTheta = k * cosTheta;
    float denom = 1.0 + kcosTheta;
    return (1.0 - k * k) / (4.0 * PI * denom * denom);
}

// Two-lobe phase function (forward + back scatter)
// Common for clouds: strong forward scatter from sun, weak backscatter
float DualLobePhase(float cosTheta, float gForward, float gBackward, float blend)
{
    float phaseForward = HenyeyGreenstein(cosTheta, gForward);
    float phaseBackward = HenyeyGreenstein(cosTheta, gBackward);
    return mix(phaseBackward, phaseForward, blend);
}

// Rayleigh phase function (for small particles like air molecules)
float RayleighPhase(float cosTheta)
{
    return (3.0 / (16.0 * PI)) * (1.0 + cosTheta * cosTheta);
}

// ============================================================================
// Froxel (Frustum Voxel) Coordinate Conversions
// ============================================================================

// Safe depth constants to prevent division by zero
const float SAFE_NEAR_Z = 0.001;
const float SAFE_DEPTH_RATIO = 1.001;  // Minimum farZ/nearZ ratio

// Convert UV + linear depth to froxel grid coordinates
ivec3 UVDepthToFroxel(vec2 uv, float linearDepth, ivec3 gridSize, float nearZ, float farZ)
{
    // Guard against division by zero
    nearZ = max(nearZ, SAFE_NEAR_Z);
    farZ = max(farZ, nearZ * SAFE_DEPTH_RATIO);
    linearDepth = max(linearDepth, SAFE_NEAR_Z);

    // XY: directly from UV
    int x = int(uv.x * float(gridSize.x));
    int y = int(uv.y * float(gridSize.y));

    // Z: exponential distribution (more slices near camera)
    float normalizedDepth = (linearDepth - nearZ) / (farZ - nearZ);
    normalizedDepth = clamp(normalizedDepth, 0.0, 1.0);

    // Exponential slice distribution: more resolution near camera
    float expDepth = log(linearDepth / nearZ) / log(farZ / nearZ);
    expDepth = clamp(expDepth, 0.0, 1.0);

    int z = int(expDepth * float(gridSize.z - 1));

    return ivec3(
        clamp(x, 0, gridSize.x - 1),
        clamp(y, 0, gridSize.y - 1),
        clamp(z, 0, gridSize.z - 1)
    );
}

// Convert froxel grid coordinates to world position
vec3 FroxelToWorld(ivec3 froxelCoord, ivec3 gridSize, mat4 invViewProj, float nearZ, float farZ)
{
    // Guard against division by zero
    nearZ = max(nearZ, SAFE_NEAR_Z);
    farZ = max(farZ, nearZ * SAFE_DEPTH_RATIO);

    // XY: froxel center in NDC
    vec2 uv = (vec2(froxelCoord.xy) + 0.5) / vec2(gridSize.xy);
    vec2 ndc = uv * 2.0 - 1.0;

    // Z: exponential depth distribution
    float sliceNorm = (float(froxelCoord.z) + 0.5) / float(gridSize.z);
    float linearDepth = nearZ * pow(farZ / nearZ, sliceNorm);

    // Convert to clip space
    vec4 clipPos = vec4(ndc, 0.0, 1.0);
    vec4 viewPos = invViewProj * clipPos;
    vec3 viewDir = normalize(viewPos.xyz / viewPos.w);

    // Scale by depth to get world position (relative to camera)
    return viewDir * linearDepth;
}

// Get depth at froxel slice (exponential distribution)
float FroxelSliceToDepth(int slice, int numSlices, float nearZ, float farZ)
{
    // Guard against division by zero
    nearZ = max(nearZ, SAFE_NEAR_Z);
    farZ = max(farZ, nearZ * SAFE_DEPTH_RATIO);

    float sliceNorm = (float(slice) + 0.5) / float(numSlices);
    return nearZ * pow(farZ / nearZ, sliceNorm);
}

// Get slice index for given depth
int DepthToFroxelSlice(float depth, int numSlices, float nearZ, float farZ)
{
    // Guard against division by zero
    nearZ = max(nearZ, SAFE_NEAR_Z);
    farZ = max(farZ, nearZ * SAFE_DEPTH_RATIO);
    depth = max(depth, SAFE_NEAR_Z);

    float sliceNorm = log(depth / nearZ) / log(farZ / nearZ);
    return clamp(int(sliceNorm * float(numSlices)), 0, numSlices - 1);
}

// ============================================================================
// In-Scattering Integration
// ============================================================================

// Physical inscattering with normalized single-scattering albedo
// L_inscatter = L_light * albedo * sigma_s * phase * transmittance
// Where albedo = sigma_s / sigma_t and sigma_t = sigma_s + sigma_a (extinction coefficient)
// This ensures energy conservation: scattered light cannot exceed extincted light
//
// Parameters:
// lightColor: incoming light color
// density: local density at sample point
// scatteringCoeff: scattering coefficient (sigma_s) - probability of scattering
// absorptionCoeff: absorption coefficient (sigma_a) - probability of absorption
// phase: phase function value for viewing direction (Henyey-Greenstein etc.)
// transmittance: accumulated transmittance along view ray
vec3 InScatteringStep(vec3 lightColor, float density, float scatteringCoeff, float absorptionCoeff, float phase, float transmittance)
{
    // Total extinction = scattering + absorption
    float sigmaT = scatteringCoeff + absorptionCoeff;

    // Single-scattering albedo: fraction of extinction that scatters (vs absorbed)
    // Physically: albedo in [0,1] where 0 = pure absorption, 1 = pure scattering
    float albedo = (sigmaT > 0.0001) ? (scatteringCoeff / sigmaT) : 0.0;

    float scattering = density * scatteringCoeff * phase;
    return lightColor * albedo * scattering * transmittance;
}

// Legacy overload for backwards compatibility (assumes zero absorption)
// DEPRECATED: Use the 6-parameter version for physically correct results
vec3 InScatteringStep(vec3 lightColor, float density, float scatteringCoeff, float phase, float transmittance)
{
    // With zero absorption, albedo = sigma_s / sigma_s = 1.0 (pure scattering)
    return InScatteringStep(lightColor, density, scatteringCoeff, 0.0, phase, transmittance);
}

// Approximate shadow from fog (exponential shadow map style)
float ExponentialShadow(float depth, float shadowMapDepth, float c)
{
    return clamp(exp(-c * (depth - shadowMapDepth)), 0.0, 1.0);
}

// ============================================================================
// Height-based Fog Density
// ============================================================================

// Exponential height falloff
float HeightFogDensity(float worldY, float baseHeight, float falloff)
{
    return exp(-max(0.0, worldY - baseHeight) * falloff);
}

// Layer-based fog (ground fog)
float GroundFogDensity(float worldY, float groundLevel, float thickness, float density)
{
    float heightAboveGround = worldY - groundLevel;
    if (heightAboveGround < 0.0)
        return density;
    if (heightAboveGround > thickness)
        return 0.0;

    float t = heightAboveGround / thickness;
    return density * (1.0 - t * t); // Quadratic falloff
}

// ============================================================================
// Temporal Jitter
// ============================================================================

// Apply sub-voxel jitter to ray start position for temporal filtering
vec3 ApplyTemporalJitter(vec3 rayOrigin, vec3 rayDir, vec2 jitter, float stepSize)
{
    // Jitter along ray direction
    float jitterAmount = (jitter.x * 0.5 + 0.5) * stepSize;
    return rayOrigin + rayDir * jitterAmount;
}

// Blue noise dithering offset
float BlueNoiseOffset(sampler2D blueNoiseTex, ivec2 pixelCoord, int frameIndex)
{
    // Spatiotemporal blue noise
    ivec2 coord = pixelCoord % textureSize(blueNoiseTex, 0);
    float noise = texelFetch(blueNoiseTex, coord, 0).r;

    // Temporal offset using golden ratio
    float goldenRatio = 1.61803398875;
    noise = fract(noise + float(frameIndex) * goldenRatio);

    return noise;
}

#endif // FLUX_VOLUMETRIC_COMMON_FXH
