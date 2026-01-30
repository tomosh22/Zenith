#version 450 core

// Prefiltered environment map shader
// Generates roughness-based mip levels for specular IBL
// Uses GGX importance sampling
//
// Note: Common sampling utilities (RadicalInverse_VdC, Hammersley, ImportanceSampleGGX)
// are now centralized in PBRConstants.fxh for consistency across all shaders.

#include "../Common.fxh"
#include "../PBRConstants.fxh"
#include "../Skybox/Flux_AtmosphereCommon.fxh"

layout(location = 0) out vec4 o_xColour;
layout(location = 0) in vec2 a_xUV;

// Push constants
layout(std140, set = 0, binding = 1) uniform PrefilterConstants
{
	float g_fRoughness;       // Current roughness level [0, 1]
	uint g_uUseAtmosphere;    // 1 = sample from atmosphere, 0 = sample from skybox
	float g_fSunIntensity;
	uint g_uFaceIndex;        // Cubemap face index (0-5: +X, -X, +Y, -Y, +Z, -Z)
};

// Skybox cubemap (used when g_uUseAtmosphere == 0)
layout(set = 0, binding = 2) uniform samplerCube g_xSkyboxCubemap;

// 64 samples is sufficient for prefiltered environment map due to the blurring
// effect at higher roughness levels. Reduces total cost from ~61M to ~30M samples.
const uint SAMPLE_COUNT = 64u;

// Convert UV + face index to cubemap direction
// Face order: 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z
vec3 CubemapFaceDirection(vec2 xUV, uint uFace)
{
	// Convert UV from [0,1] to [-1,1]
	vec2 xCoord = xUV * 2.0 - 1.0;

	vec3 xDir;
	switch (uFace)
	{
		case 0u: xDir = vec3( 1.0, -xCoord.y, -xCoord.x); break; // +X
		case 1u: xDir = vec3(-1.0, -xCoord.y,  xCoord.x); break; // -X
		case 2u: xDir = vec3( xCoord.x,  1.0,  xCoord.y); break; // +Y
		case 3u: xDir = vec3( xCoord.x, -1.0, -xCoord.y); break; // -Y
		case 4u: xDir = vec3( xCoord.x, -xCoord.y,  1.0); break; // +Z
		case 5u: xDir = vec3(-xCoord.x, -xCoord.y, -1.0); break; // -Z
		default: xDir = vec3(0.0, 1.0, 0.0); break;
	}
	return normalize(xDir);
}

// Note: RadicalInverse_VdC, Hammersley, and ImportanceSampleGGX are now in PBRConstants.fxh

// Sample environment in given direction
vec3 SampleEnvironment(vec3 xDir)
{
	if (g_uUseAtmosphere > 0u)
	{
		// Note: g_xSunDir_Pad is direction light travels (from sun), negate to get direction TO sun
		vec3 xSunDir = normalize(-g_xSunDir_Pad.xyz);
		// IBL uses fixed reference height (configurable via IBL_REFERENCE_HEIGHT_METERS in PBRConstants.fxh)
		// This is intentional: IBL textures are pre-computed environment lighting, not camera-relative
		AtmosphereResult xResult = ComputeAtmosphereScattering(
			vec3(0.0, EARTH_RADIUS + IBL_REFERENCE_HEIGHT_METERS, 0.0),
			normalize(xDir),
			xSunDir,
			g_fSunIntensity
		);
		return xResult.xColor;
	}
	else
	{
		return texture(g_xSkyboxCubemap, xDir).rgb;
	}
}

void main()
{
	// Get reflection direction from UV + face index
	vec3 xN = CubemapFaceDirection(a_xUV, g_uFaceIndex);
	vec3 xR = xN;
	vec3 xV = xR;

	vec3 xPrefilteredColor = vec3(0.0);
	float fTotalWeight = 0.0;

	// Blend threshold for very smooth surfaces (roughness < 0.05)
	// At extremely low roughness, GGX importance sampling produces aliasing artifacts
	// because the distribution approaches a delta function. Blending with a direct
	// mirror reflection avoids this discontinuity and provides correct behavior for
	// near-perfect mirrors. IBL_SMOOTH_BLEND_THRESHOLD is defined in PBRConstants.fxh.
	vec3 xMirrorColor = SampleEnvironment(xN);

	// Minimum roughness for GGX importance sampling
	// As roughness approaches 0, GGX distribution becomes a delta function, causing
	// numerical instability in sampling. This floor prevents division by near-zero
	// in the GGX formula while the smoothstep blend above ensures visual continuity.
	float fClampedRoughness = max(g_fRoughness, PBR_GGX_MIN_ROUGHNESS);

	for (uint i = 0u; i < SAMPLE_COUNT; i++)
	{
		vec2 Xi = Hammersley(i, SAMPLE_COUNT);
		vec3 H = ImportanceSampleGGX(Xi, xN, fClampedRoughness);
		vec3 L = normalize(2.0 * dot(xV, H) * H - xV);

		float NdotL = max(dot(xN, L), 0.0);

		if (NdotL > 0.0)
		{
			xPrefilteredColor += SampleEnvironment(L) * NdotL;
			fTotalWeight += NdotL;
		}
	}

	xPrefilteredColor = xPrefilteredColor / max(fTotalWeight, 0.001);

	// Blend with mirror reflection for smooth surfaces to avoid discontinuity
	if (g_fRoughness < IBL_SMOOTH_BLEND_THRESHOLD)
	{
		float fBlendFactor = smoothstep(PBR_GGX_MIN_ROUGHNESS, IBL_SMOOTH_BLEND_THRESHOLD, g_fRoughness);
		xPrefilteredColor = mix(xMirrorColor, xPrefilteredColor, fBlendFactor);
	}

	o_xColour = vec4(xPrefilteredColor, 1.0);
}
