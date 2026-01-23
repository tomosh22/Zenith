#version 450 core

// Prefiltered environment map shader
// Generates roughness-based mip levels for specular IBL
// Uses GGX importance sampling

#include "../Common.fxh"
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

const float PI = 3.14159265359;
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

// Van der Corput sequence for quasi-random sampling
float RadicalInverse_VdC(uint bits)
{
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float(bits) * 2.3283064365386963e-10;
}

// Hammersley sequence
vec2 Hammersley(uint i, uint N)
{
	return vec2(float(i) / float(N), RadicalInverse_VdC(i));
}

// GGX importance sampling
vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness)
{
	float a = roughness * roughness;

	float phi = 2.0 * PI * Xi.x;
	float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
	float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

	// Spherical to cartesian
	vec3 H;
	H.x = cos(phi) * sinTheta;
	H.y = sin(phi) * sinTheta;
	H.z = cosTheta;

	// Tangent to world space
	vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
	vec3 tangent = normalize(cross(up, N));
	vec3 bitangent = cross(N, tangent);

	return normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

// Sample environment in given direction
vec3 SampleEnvironment(vec3 xDir)
{
	if (g_uUseAtmosphere > 0u)
	{
		// Note: g_xSunDir_Pad is direction light travels (from sun), negate to get direction TO sun
		vec3 xSunDir = normalize(-g_xSunDir_Pad.xyz);
		// IBL uses fixed reference height (100m above ground) - this is intentional
		// as IBL textures are pre-computed environment lighting, not camera-relative
		AtmosphereResult xResult = ComputeAtmosphereScattering(
			vec3(0.0, EARTH_RADIUS + 100.0, 0.0),
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

	// For very smooth surfaces (roughness < threshold), blend between mirror reflection
	// and sampled result to avoid visible discontinuity at the cutoff boundary
	vec3 xMirrorColor = SampleEnvironment(xN);
	float fBlendThreshold = 0.05;

	// Clamp roughness to avoid numerical issues in GGX sampling
	// NOTE: Do NOT early-return for roughness < 0.005 - this would bypass the
	// blending logic below and create a visible seam at the threshold. Instead,
	// clamp roughness and let the smoothstep blend handle the transition.
	float fClampedRoughness = max(g_fRoughness, 0.005);

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
	if (g_fRoughness < fBlendThreshold)
	{
		float fBlendFactor = smoothstep(0.005, fBlendThreshold, g_fRoughness);
		xPrefilteredColor = mix(xMirrorColor, xPrefilteredColor, fBlendFactor);
	}

	o_xColour = vec4(xPrefilteredColor, 1.0);
}
