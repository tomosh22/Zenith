#version 450 core

// Irradiance convolution shader
// Generates diffuse irradiance map from environment (atmosphere or skybox)
// Uses hemisphere cosine-weighted sampling

#include "../Common.fxh"
#include "../Skybox/Flux_AtmosphereCommon.fxh"

layout(location = 0) out vec4 o_xColour;
layout(location = 0) in vec2 a_xUV;

// Push constants
layout(std140, set = 0, binding = 1) uniform IrradianceConstants
{
	uint g_uUseAtmosphere;   // 1 = sample from atmosphere, 0 = sample from skybox
	float g_fSunIntensity;
	uint g_uFaceIndex;       // Cubemap face index (0-5: +X, -X, +Y, -Y, +Z, -Z)
	float g_fPad;
};

// Skybox cubemap (used when g_uUseAtmosphere == 0)
layout(set = 0, binding = 2) uniform samplerCube g_xSkyboxCubemap;

const float PI = 3.14159265359;
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

// Sample environment in given direction
vec3 SampleEnvironment(vec3 xDir)
{
	if (g_uUseAtmosphere > 0u)
	{
		// Sample from procedural atmosphere
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
		// Sample from skybox cubemap
		return texture(g_xSkyboxCubemap, xDir).rgb;
	}
}

void main()
{
	// Get normal direction from UV + face index
	vec3 xNormal = CubemapFaceDirection(a_xUV, g_uFaceIndex);

	// Build tangent space basis
	vec3 xUp = abs(xNormal.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
	vec3 xRight = normalize(cross(xUp, xNormal));
	xUp = cross(xNormal, xRight);

	// Cosine-weighted importance sampling for diffuse irradiance
	// Using Hammersley sequence for well-distributed samples
	vec3 xIrradiance = vec3(0.0);

	for (uint i = 0u; i < SAMPLE_COUNT; i++)
	{
		// Hammersley sequence for quasi-random sampling
		float fXi1 = float(i) / float(SAMPLE_COUNT);
		// Van der Corput sequence for fXi2
		uint uBits = i;
		uBits = (uBits << 16u) | (uBits >> 16u);
		uBits = ((uBits & 0x55555555u) << 1u) | ((uBits & 0xAAAAAAAAu) >> 1u);
		uBits = ((uBits & 0x33333333u) << 2u) | ((uBits & 0xCCCCCCCCu) >> 2u);
		uBits = ((uBits & 0x0F0F0F0Fu) << 4u) | ((uBits & 0xF0F0F0F0u) >> 4u);
		uBits = ((uBits & 0x00FF00FFu) << 8u) | ((uBits & 0xFF00FF00u) >> 8u);
		float fXi2 = float(uBits) * 2.3283064365386963e-10; // 1/2^32

		// Cosine-weighted importance sampling:
		// For diffuse irradiance, we want samples weighted by cos(theta)
		// Using the transformation: cos(theta) = sqrt(1 - xi1)
		float fCosTheta = sqrt(1.0 - fXi1);
		float fSinTheta = sqrt(fXi1);  // sin^2 + cos^2 = 1, so sin = sqrt(1 - cos^2) = sqrt(xi1)
		float fPhi = 2.0 * PI * fXi2;

		// Convert to cartesian in tangent space
		vec3 xTangentSample = vec3(
			fSinTheta * cos(fPhi),
			fSinTheta * sin(fPhi),
			fCosTheta
		);

		// Transform to world space
		vec3 xSampleDir = xTangentSample.x * xRight + xTangentSample.y * xUp + xTangentSample.z * xNormal;

		// Sample environment (no explicit cosine weight needed - it's in the PDF)
		xIrradiance += SampleEnvironment(xSampleDir);
	}

	// Normalize by sample count
	// NOTE: Do NOT multiply by PI here. Cosine-weighted importance sampling already
	// accounts for the hemisphere integral via the PDF (cos(theta)/PI). The PI factors
	// cancel out, leaving just the average of samples.
	xIrradiance = xIrradiance / float(SAMPLE_COUNT);

	o_xColour = vec4(xIrradiance, 1.0);
}
