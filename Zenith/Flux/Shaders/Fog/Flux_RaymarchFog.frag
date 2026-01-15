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
    float u_fPad0;
};

// Bindings (binding 0 is FrameConstants from Common.fxh)
layout(set = 0, binding = 2) uniform sampler2D u_xDepthTexture;
layout(set = 0, binding = 3) uniform sampler3D u_xNoiseTexture3D;
layout(set = 0, binding = 4) uniform sampler2D u_xBlueNoiseTexture;

// Debug mode constants (9-12 for raymarch)
const uint DEBUG_RAYMARCH_STEP_COUNT = 9;
const uint DEBUG_RAYMARCH_ACCUMULATED_DENSITY = 10;
const uint DEBUG_RAYMARCH_NOISE_SAMPLE = 11;
const uint DEBUG_RAYMARCH_JITTER_PATTERN = 12;

// Sample density at world position using 3D noise
float SampleDensity(vec3 worldPos)
{
    float baseDensity = u_xFogParams.x;

    // Height-based falloff
    float heightDensity = HeightFogDensity(worldPos.y, u_xHeightParams.x, u_xHeightParams.y);

    // Animated 3D noise for density variation
    vec3 noiseCoord = worldPos * u_xNoiseParams.x;
    noiseCoord.y += u_xNoiseParams.w; // Animate vertically

    // Sample pre-computed 3D noise texture
    float noise = texture(u_xNoiseTexture3D, noiseCoord * 0.01).r;

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
    uint actualSteps = 0;

    // Sun/light direction from Common.fxh
    vec3 lightDir = normalize(-g_xSunDir_Pad.xyz);
    vec3 lightColor = g_xSunColour.rgb * 2.0;

    // March along the ray
    for (uint i = 0; i < u_uNumSteps; i++)
    {
        float t = rayOffset + float(i) * stepSize;
        if (t >= maxDistance) break;

        vec3 samplePos = cameraPos + rayDir * t;

        // Sample density at this position
        float density = SampleDensity(samplePos);
        lastNoiseSample = density;

        if (density > 0.001)
        {
            actualSteps++;
            accumulatedDensity += density * stepSize;

            // Beer-Lambert extinction
            float extinction = BeerLambertExtinction(
                u_xFogParams.y * density,  // scattering
                u_xFogParams.z * density,  // absorption
                stepSize
            );

            // Phase function for light scattering direction
            float cosTheta = dot(rayDir, lightDir);
            float phase = HenyeyGreenstein(cosTheta, 0.6); // Forward scattering

            // In-scattering (simplified - no shadow sampling)
            vec3 inScatter = InScatteringStep(
                lightColor,
                density,
                u_xFogParams.y,
                phase,
                accumulatedTransmittance
            );

            accumulatedLight += inScatter * stepSize;

            // Update transmittance
            accumulatedTransmittance *= extinction;

            // Early exit if fully opaque
            if (accumulatedTransmittance < 0.01)
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

    // Final fog color with ambient contribution
    vec3 ambientLight = u_xFogColour.rgb * 0.3;
    vec3 finalFog = accumulatedLight + ambientLight * (1.0 - accumulatedTransmittance);

    // Output with alpha for blending
    float fogAlpha = 1.0 - accumulatedTransmittance;
    o_xColour = vec4(finalFog, fogAlpha);
}
