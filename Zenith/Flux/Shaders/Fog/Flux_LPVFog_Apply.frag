#version 450

#include "../Common.fxh"
#include "Flux_VolumetricCommon.fxh"

layout(location = 0) in vec2 a_xUV;
layout(location = 0) out vec4 o_xColour;

// Scratch buffer for push constants replacement
layout(std140, set = 0, binding = 1) uniform ApplyConstants
{
    vec4 u_axCascadeCenters[3];    // xyz = center, w = radius (packed to avoid alignment issues)
    uint u_uNumCascades;
    uint u_uDebugMode;
    uint u_uDebugCascade;
    float u_fPad0;
};

// Bindings (binding 0 is FrameConstants from Common.fxh)
layout(set = 0, binding = 2) uniform sampler2D u_xDepthTexture;
layout(set = 0, binding = 3) uniform sampler3D u_xLPVCascade0;
layout(set = 0, binding = 4) uniform sampler3D u_xLPVCascade1;
layout(set = 0, binding = 5) uniform sampler3D u_xLPVCascade2;
layout(set = 0, binding = 6) uniform sampler3D u_xNoiseTexture3D;

// Debug mode constants
const uint DEBUG_LPV_CASCADE_BOUNDS = 15;

// Sample LPV at world position, selecting appropriate cascade
vec3 SampleLPV(vec3 worldPos)
{
    // Find best cascade for this position
    for (uint cascade = 0; cascade < u_uNumCascades; cascade++)
    {
        vec3 cascadeCenter = u_axCascadeCenters[cascade].xyz;
        float cascadeRadius = u_axCascadeCenters[cascade].w;

        // Check if within cascade bounds
        vec3 offset = worldPos - cascadeCenter;
        if (all(lessThan(abs(offset), vec3(cascadeRadius))))
        {
            // Convert to normalized coordinates
            vec3 uvw = (offset / cascadeRadius) * 0.5 + 0.5;

            // Sample appropriate cascade
            if (cascade == 0)
                return texture(u_xLPVCascade0, uvw).rgb;
            else if (cascade == 1)
                return texture(u_xLPVCascade1, uvw).rgb;
            else
                return texture(u_xLPVCascade2, uvw).rgb;
        }
    }

    return vec3(0.0);  // Outside all cascades
}

// Get cascade index for position (for debug)
int GetCascadeIndex(vec3 worldPos)
{
    for (uint cascade = 0; cascade < u_uNumCascades; cascade++)
    {
        vec3 cascadeCenter = u_axCascadeCenters[cascade].xyz;
        float cascadeRadius = u_axCascadeCenters[cascade].w;
        vec3 offset = worldPos - cascadeCenter;

        if (all(lessThan(abs(offset), vec3(cascadeRadius))))
        {
            return int(cascade);
        }
    }
    return -1;
}

void main()
{
    // Sample depth
    float depth = texture(u_xDepthTexture, a_xUV).r;

    // Reconstruct world position using precomputed inverses
    vec4 clipPos = vec4(a_xUV * 2.0 - 1.0, depth, 1.0);
    vec4 viewPos = g_xInvProjMat * clipPos;
    viewPos /= viewPos.w;
    vec4 worldPos = g_xInvViewMat * viewPos;

    vec3 cameraPos = g_xCamPos_Pad.xyz;
    vec3 rayDir = normalize(worldPos.xyz - cameraPos);
    float maxDistance = length(worldPos.xyz - cameraPos);

    // Debug: Show cascade bounds
    if (u_uDebugMode == DEBUG_LPV_CASCADE_BOUNDS)
    {
        // Color code by cascade (red = 0, green = 1, blue = 2)
        vec3 cascadeColors[3] = vec3[3](
            vec3(1.0, 0.2, 0.2),
            vec3(0.2, 1.0, 0.2),
            vec3(0.2, 0.2, 1.0)
        );

        // Sample at mid-distance
        vec3 midPoint = cameraPos + rayDir * maxDistance * 0.5;
        int cascade = GetCascadeIndex(midPoint);

        if (cascade >= 0)
        {
            o_xColour = vec4(cascadeColors[cascade], 0.3);
        }
        else
        {
            o_xColour = vec4(0.0);
        }
        return;
    }

    // Ray march through fog, sampling LPV for lighting
    const int NUM_STEPS = 32;
    float stepSize = min(maxDistance, 500.0) / float(NUM_STEPS);

    vec3 accumulatedLight = vec3(0.0);
    float accumulatedTransmittance = 1.0;

    // Base fog density
    float baseDensity = 0.01;

    for (int i = 0; i < NUM_STEPS; i++)
    {
        float t = float(i) * stepSize;
        vec3 samplePos = cameraPos + rayDir * t;

        // Skip if beyond geometry
        if (t > maxDistance) break;

        // Sample density from noise (single scaling factor for proper variation)
        vec3 noiseCoord = samplePos * 0.02;
        float noise = texture(u_xNoiseTexture3D, noiseCoord).r;
        float density = baseDensity * noise;

        // Height falloff
        density *= HeightFogDensity(samplePos.y, 0.0, 0.01);

        if (density > 0.0001)
        {
            // Sample LPV for indirect lighting
            vec3 lpvLight = SampleLPV(samplePos);

            // Beer-Lambert extinction
            float extinction = BeerLambert(density, stepSize);

            // Accumulate in-scatter
            accumulatedLight += lpvLight * density * accumulatedTransmittance * (1.0 - extinction);

            // Update transmittance
            accumulatedTransmittance *= extinction;

            if (accumulatedTransmittance < 0.01) break;
        }
    }

    // Output
    float fogAlpha = 1.0 - accumulatedTransmittance;
    o_xColour = vec4(accumulatedLight, fogAlpha);
}
