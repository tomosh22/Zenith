#version 450 core

#include "../Common.fxh"
#include "Flux_VolumetricCommon.fxh"

layout(location = 0) in vec2 a_xUV;
layout(location = 0) out vec4 o_xColour;

// Scratch buffer for push constants replacement
layout(std140, set = 0, binding = 1) uniform ApplyConstants
{
    vec4 u_xGridDimensions;
    float u_fNearZ;
    float u_fFarZ;
    uint u_uDebugMode;
    uint u_uDebugSliceIndex;
};

// Bindings (binding 0 is FrameConstants from Common.fxh)
layout(set = 0, binding = 2) uniform sampler2D u_xDepthTexture;
layout(set = 0, binding = 3) uniform sampler3D u_xLightingGrid;
layout(set = 0, binding = 4) uniform sampler3D u_xScatteringGrid;

// Debug mode constants
const uint DEBUG_FROXEL_DENSITY_SLICE = 3;
const uint DEBUG_FROXEL_DENSITY_MAX = 4;
const uint DEBUG_FROXEL_LIGHTING_SLICE = 5;

// Convert linear depth to froxel Z slice
float DepthToFroxelZ(float linearDepth)
{
    // Guard against division by zero
    float nearZ = max(u_fNearZ, SAFE_NEAR_Z);
    float farZ = max(u_fFarZ, nearZ * SAFE_DEPTH_RATIO);
    linearDepth = max(linearDepth, SAFE_NEAR_Z);

    float sliceNorm = log(linearDepth / nearZ) / log(farZ / nearZ);
    return clamp(sliceNorm, 0.0, 1.0);
}

// Sample froxel grid with trilinear filtering
vec4 SampleFroxelGrid(sampler3D grid, vec2 uv, float froxelZ)
{
    vec3 uvw = vec3(uv, froxelZ);
    return texture(grid, uvw);
}

void main()
{
    // Debug: Show single slice of density grid
    if (u_uDebugMode == DEBUG_FROXEL_DENSITY_SLICE)
    {
        float sliceZ = (float(u_uDebugSliceIndex) + 0.5) / u_xGridDimensions.z;
        vec4 scatterData = texture(u_xScatteringGrid, vec3(a_xUV, sliceZ));
        float extinction = scatterData.a;
        o_xColour = vec4(vec3(clamp(extinction, 0.0, 1.0)), 1.0);
        return;
    }

    // Debug: Max projection of density through all slices
    if (u_uDebugMode == DEBUG_FROXEL_DENSITY_MAX)
    {
        float maxExtinction = 0.0;
        for (int z = 0; z < int(u_xGridDimensions.z); z++)
        {
            float sliceZ = (float(z) + 0.5) / u_xGridDimensions.z;
            vec4 scatterData = texture(u_xScatteringGrid, vec3(a_xUV, sliceZ));
            maxExtinction = max(maxExtinction, scatterData.a);
        }
        o_xColour = vec4(vec3(clamp(maxExtinction, 0.0, 1.0)), 1.0);
        return;
    }

    // Debug: Single slice of lighting grid
    if (u_uDebugMode == DEBUG_FROXEL_LIGHTING_SLICE)
    {
        float sliceZ = (float(u_uDebugSliceIndex) + 0.5) / u_xGridDimensions.z;
        vec3 lighting = texture(u_xLightingGrid, vec3(a_xUV, sliceZ)).rgb;
        o_xColour = vec4(lighting, 1.0);
        return;
    }

    // Sample scene depth
    float depth = texture(u_xDepthTexture, a_xUV).r;

    // Reconstruct linear depth using precomputed inverse projection
    // Note: Engine uses +Z forward in view space
    vec4 clipPos = vec4(a_xUV * 2.0 - 1.0, depth, 1.0);
    vec4 viewPos = g_xInvProjMat * clipPos;
    viewPos /= viewPos.w;
    float linearDepth = viewPos.z;

    // Clamp to froxel range
    float nearZ = max(u_fNearZ, SAFE_NEAR_Z);
    float farZ = max(u_fFarZ, nearZ * SAFE_DEPTH_RATIO);
    linearDepth = clamp(linearDepth, nearZ, farZ);

    // Ray march through froxel grid
    // ========== FROXEL APPLY SAMPLE COUNT ==========
    // 32 samples provides good quality for typical froxel grids (160x90x64)
    // This is a pragmatic balance, NOT a physically-derived value.
    // The froxel grid is 64 slices deep, so 32 samples provides ~2x oversampling
    // with trilinear filtering for smooth results.
    // Quality guidance: 16 = fast/mobile, 32 = balanced, 64 = high quality
    const int NUM_SAMPLES = 32;

    vec3 accumulatedLight = vec3(0.0);
    float accumulatedTransmittance = 1.0;

    for (int i = 0; i < NUM_SAMPLES; i++)
    {
        // Sample depth (exponential distribution)
        float t = float(i + 1) / float(NUM_SAMPLES);
        float sampleDepth = nearZ * pow(linearDepth / nearZ, t);

        // Skip samples beyond scene geometry
        if (sampleDepth > linearDepth) break;

        // Convert to froxel coordinates
        float froxelZ = DepthToFroxelZ(sampleDepth);

        // Sample lighting and scattering grids
        vec3 inScatter = SampleFroxelGrid(u_xLightingGrid, a_xUV, froxelZ).rgb;
        vec4 scatterData = SampleFroxelGrid(u_xScatteringGrid, a_xUV, froxelZ);
        float extinction = scatterData.a;

        // Step size in depth
        float prevDepth = (i == 0) ? nearZ : nearZ * pow(linearDepth / nearZ, float(i) / float(NUM_SAMPLES));
        float stepSize = sampleDepth - prevDepth;

        // Beer-Lambert transmittance for this step
        float stepTransmittance = exp(-extinction * stepSize);

        // Accumulate in-scatter weighted by current transmittance
        accumulatedLight += inScatter * accumulatedTransmittance * (1.0 - stepTransmittance);

        // Update transmittance
        accumulatedTransmittance *= stepTransmittance;

        // Early exit if fully opaque
        if (accumulatedTransmittance < FOG_TRANSMITTANCE_EARLY_EXIT) break;
    }

    // Final output
    float fogAlpha = 1.0 - accumulatedTransmittance;

    o_xColour = vec4(accumulatedLight, fogAlpha);
}
