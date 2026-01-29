#version 450 core

#include "../Common.fxh"
#include "Flux_NoiseCommon.fxh"
#include "Flux_VolumetricCommon.fxh"

layout(location = 0) in vec2 a_xUV;
layout(location = 0) out vec4 o_xColour;

// Scratch buffer for push constants replacement
layout(std140, set = 0, binding = 1) uniform RaymarchConstants
{
    vec4 u_xFogColour;        // RGB = fog color, A = unused
    vec4 u_xFogParams;        // x = density, y = scattering, z = absorption, w = max distance
    vec4 u_xNoiseParams;      // x = scale, y = speed, z = detail, w = time
    vec4 u_xHeightParams;     // x = base height, y = falloff, z = unused, w = unused
    uint u_uNumSteps;
    uint u_uDebugMode;
    uint u_uFrameIndex;
    float u_fPhaseG;          // Henyey-Greenstein asymmetry: -1=backscatter, 0=isotropic, 0.6=forward (typical fog)
    // Volumetric shadow parameters (unified with Froxel fog for consistent shadow softness)
    float u_fVolShadowBias;        // Shadow bias for volumetric samples
    float u_fVolShadowConeRadius;  // Cone spread radius in shadow space
    float u_fAmbientIrradianceRatio; // Sky/sun light ratio for ambient fog contribution
    float u_fNoiseWorldScale;      // World-to-texture coordinate scale for noise sampling
};

// Bindings (binding 0 is FrameConstants from Common.fxh)
layout(set = 0, binding = 2) uniform sampler2D u_xDepthTexture;
layout(set = 0, binding = 3) uniform sampler3D u_xNoiseTexture3D;
layout(set = 0, binding = 4) uniform sampler2D u_xBlueNoiseTexture;

// CSM Shadow maps for volumetric shadows
layout(set = 0, binding = 5) uniform sampler2D u_xCSM0;
layout(set = 0, binding = 6) uniform sampler2D u_xCSM1;
layout(set = 0, binding = 7) uniform sampler2D u_xCSM2;
layout(set = 0, binding = 8) uniform sampler2D u_xCSM3;

// Shadow matrices (individual bindings to match deferred shading pattern)
layout(std140, set = 0, binding = 9) uniform ShadowMatrix0 { mat4 u_xShadowMat0; };
layout(std140, set = 0, binding = 10) uniform ShadowMatrix1 { mat4 u_xShadowMat1; };
layout(std140, set = 0, binding = 11) uniform ShadowMatrix2 { mat4 u_xShadowMat2; };
layout(std140, set = 0, binding = 12) uniform ShadowMatrix3 { mat4 u_xShadowMat3; };

// Debug mode constants (9-12 for raymarch)
const uint DEBUG_RAYMARCH_STEP_COUNT = 9;
const uint DEBUG_RAYMARCH_ACCUMULATED_DENSITY = 10;
const uint DEBUG_RAYMARCH_NOISE_SAMPLE = 11;
const uint DEBUG_RAYMARCH_JITTER_PATTERN = 12;
const uint DEBUG_RAYMARCH_SHADOW = 13;

// Volumetric shadow constants
// Shadow bias and cone radius are now uniforms (u_fVolShadowBias, u_fVolShadowConeRadius)
// unified with Froxel fog for consistent shadow softness across techniques
const int VOL_SHADOW_SAMPLES = 4;                    // Cone samples for soft volumetric shadows (kept as const for loop unrolling)

// Sample volumetric shadow with soft cone sampling
// Returns shadow factor (1.0 = fully lit, 0.0 = fully shadowed)
float SampleVolumetricShadow(vec3 worldPos)
{
    mat4 axShadowMats[4] = mat4[4](u_xShadowMat0, u_xShadowMat1, u_xShadowMat2, u_xShadowMat3);

    // Find appropriate cascade
    for (int iCascade = 0; iCascade < 4; iCascade++)
    {
        vec4 xShadowSpace = axShadowMats[iCascade] * vec4(worldPos, 1.0);
        vec2 xSamplePos = xShadowSpace.xy / xShadowSpace.w * 0.5 + 0.5;
        float fCurrentDepth = xShadowSpace.z / xShadowSpace.w;

        // Check if within cascade bounds
        if (xSamplePos.x < 0.0 || xSamplePos.x > 1.0 ||
            xSamplePos.y < 0.0 || xSamplePos.y > 1.0 ||
            fCurrentDepth < 0.0 || fCurrentDepth > 1.0)
        {
            continue;
        }

        // Simple 4-sample cone pattern for soft volumetric shadows
        vec2 axOffsets[4] = vec2[4](
            vec2(-0.7071, -0.7071),
            vec2( 0.7071, -0.7071),
            vec2(-0.7071,  0.7071),
            vec2( 0.7071,  0.7071)
        );

        float fBiasedDepth = fCurrentDepth - u_fVolShadowBias;
        float fShadow = 0.0;

        for (int i = 0; i < VOL_SHADOW_SAMPLES; i++)
        {
            vec2 xOffset = axOffsets[i] * u_fVolShadowConeRadius;
            vec2 xSampleUV = xSamplePos + xOffset;

            float fShadowDepth;
            if (iCascade == 0)
                fShadowDepth = texture(u_xCSM0, xSampleUV).r;
            else if (iCascade == 1)
                fShadowDepth = texture(u_xCSM1, xSampleUV).r;
            else if (iCascade == 2)
                fShadowDepth = texture(u_xCSM2, xSampleUV).r;
            else
                fShadowDepth = texture(u_xCSM3, xSampleUV).r;

            fShadow += (fBiasedDepth <= fShadowDepth) ? 1.0 : 0.0;
        }

        return fShadow / float(VOL_SHADOW_SAMPLES);
    }

    // No cascade found (far from camera) - assume fully lit
    return 1.0;
}

// Sample density at world position using 3D noise
float SampleDensity(vec3 worldPos)
{
    // Use shared UI-to-physical density conversion from VolumetricCommon.fxh
    // This ensures consistent density behavior with froxel fog technique
    float baseDensity = UIToPhysicalDensity(u_xFogParams.x);

    // Height-based falloff
    float heightDensity = HeightFogDensity(worldPos.y, u_xHeightParams.x, u_xHeightParams.y);

    // Animated 3D noise for density variation
    vec3 noiseCoord = worldPos * u_xNoiseParams.x;
    noiseCoord.y += u_xNoiseParams.w; // Animate vertically

    // Sample pre-computed 3D noise texture using runtime-configurable world scale
    float noise = texture(u_xNoiseTexture3D, noiseCoord * u_fNoiseWorldScale).r;

    // Combine base density with height falloff and noise
    return baseDensity * heightDensity * noise;
}

void main()
{
    // Sample depth and reconstruct world position using Common.fxh helper
    vec3 worldPos = GetWorldPosFromDepthTex(u_xDepthTexture, a_xUV);

    // Camera position from Common.fxh
    vec3 cameraPos = g_xCamPos_Pad.xyz;

    // Ray direction and distance
    vec3 rayDir = normalize(worldPos - cameraPos);
    float maxDistance = min(length(worldPos - cameraPos), u_xFogParams.w);

    // Blue noise jitter for temporal stability
    ivec2 pixelCoord = ivec2(gl_FragCoord.xy);
    float jitter = BlueNoiseOffset(u_xBlueNoiseTexture, pixelCoord, int(u_uFrameIndex));

    // Debug: Jitter pattern visualization
    if (u_uDebugMode == DEBUG_RAYMARCH_JITTER_PATTERN)
    {
        o_xColour = vec4(vec3(jitter), 1.0);
        return;
    }

    // Ray marching parameters
    float stepSize = maxDistance / float(u_uNumSteps);
    float rayOffset = jitter * stepSize;

    // Accumulation variables
    vec3 accumulatedLight = vec3(0.0);
    float accumulatedTransmittance = 1.0;
    float accumulatedDensity = 0.0;
    float lastNoiseSample = 0.0;
    float lastShadowSample = 1.0;  // For debug visualization
    uint actualSteps = 0;

    // Sun/light direction from Common.fxh
    vec3 lightDir = normalize(-g_xSunDir_Pad.xyz);
    // Direct light color - no artificial boost
    // Beer-Lambert extinction is energy-conserving across all fog techniques
    // Using same sun color as deferred shading ensures consistent lighting
    vec3 lightColor = g_xSunColour.rgb;

    // March along the ray
    for (uint i = 0; i < u_uNumSteps; i++)
    {
        float t = rayOffset + float(i) * stepSize;
        if (t >= maxDistance) break;

        vec3 samplePos = cameraPos + rayDir * t;

        // Sample density at this position
        float density = SampleDensity(samplePos);
        lastNoiseSample = density;

        if (density > FOG_DENSITY_SKIP_THRESHOLD)
        {
            actualSteps++;
            accumulatedDensity += density * stepSize;

            // Sample volumetric shadow at this position
            float shadowTerm = SampleVolumetricShadow(samplePos);
            lastShadowSample = shadowTerm;

            // Beer-Lambert extinction
            float extinction = BeerLambertExtinction(
                u_xFogParams.y * density,  // scattering
                u_xFogParams.z * density,  // absorption
                stepSize
            );

            // Phase function for light scattering direction
            // u_fPhaseG controls asymmetry: 0.6 = typical fog (forward scattering)
            float cosTheta = dot(rayDir, lightDir);
            float phase = HenyeyGreenstein(cosTheta, u_fPhaseG);

            // In-scattering with energy-conserving albedo normalization
            // Apply shadow term to attenuate direct light contribution
            // Passing both scattering (y) and absorption (z) coefficients
            vec3 inScatter = InScatteringStep(
                lightColor * shadowTerm,  // Attenuate light by shadow
                density,
                u_xFogParams.y,   // scattering coefficient (sigma_s)
                u_xFogParams.z,   // absorption coefficient (sigma_a)
                phase,
                accumulatedTransmittance
            );

            accumulatedLight += inScatter * stepSize;

            // Update transmittance
            accumulatedTransmittance *= extinction;

            // Early exit if fully opaque
            if (accumulatedTransmittance < FOG_TRANSMITTANCE_EARLY_EXIT)
            {
                break;
            }
        }
    }

    // Debug visualizations
    if (u_uDebugMode == DEBUG_RAYMARCH_STEP_COUNT)
    {
        // Heat map: green = few steps, red = many steps
        float stepRatio = float(actualSteps) / float(u_uNumSteps);
        vec3 heatColor = mix(vec3(0.0, 1.0, 0.0), vec3(1.0, 0.0, 0.0), stepRatio);
        o_xColour = vec4(heatColor, 1.0);
        return;
    }

    if (u_uDebugMode == DEBUG_RAYMARCH_ACCUMULATED_DENSITY)
    {
        // Visualize accumulated density as grayscale
        float densityVis = 1.0 - exp(-accumulatedDensity * 0.5);
        o_xColour = vec4(vec3(densityVis), 1.0);
        return;
    }

    if (u_uDebugMode == DEBUG_RAYMARCH_NOISE_SAMPLE)
    {
        // Show last noise sample value
        o_xColour = vec4(vec3(lastNoiseSample), 1.0);
        return;
    }

    if (u_uDebugMode == DEBUG_RAYMARCH_SHADOW)
    {
        // Visualize volumetric shadow term (white = lit, black = shadowed)
        o_xColour = vec4(vec3(lastShadowSample), 1.0);
        return;
    }

    // Final fog color with ambient contribution - uses runtime-configurable ratio
    vec3 ambientLight = u_xFogColour.rgb * u_fAmbientIrradianceRatio;
    vec3 finalFog = accumulatedLight + ambientLight * (1.0 - accumulatedTransmittance);

    // Output with alpha for blending
    float fogAlpha = 1.0 - accumulatedTransmittance;
    o_xColour = vec4(finalFog, fogAlpha);
}
