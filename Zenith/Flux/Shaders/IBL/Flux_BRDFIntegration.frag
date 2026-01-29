#version 450 core

// BRDF Integration LUT Generator
// Computes split-sum approximation for IBL specular lighting
// Output: RG = (scale, bias) for Fresnel term: F0 * scale + bias
//
// Note: Common sampling utilities (RadicalInverse_VdC, Hammersley, ImportanceSampleGGX)
// are now centralized in PBRConstants.fxh for consistency across all shaders.

#include "../Common.fxh"
#include "../PBRConstants.fxh"

layout(location = 0) in vec2 a_xUV;

layout(location = 0) out vec4 o_xColor;

// 512 samples provides nearly identical quality to 1024 with half the cost
// BRDF LUT is pre-computed once, so this mainly helps iteration during development
const uint SAMPLE_COUNT = 512u;

// Note: GeometrySchlickGGX_IBL() and GeometrySmith_IBL() are now centralized in PBRConstants.fxh
// for consistency across all shaders. The IBL variants use k = (r²)/2 per Epic Games specification.

// Integrate BRDF over hemisphere for given NdotV and roughness
vec2 IntegrateBRDF(float NdotV, float roughness)
{
	vec3 V;
	V.x = sqrt(1.0 - NdotV * NdotV);  // sin
	V.y = 0.0;
	V.z = NdotV;                        // cos

	float A = 0.0;  // Scale factor
	float B = 0.0;  // Bias factor

	vec3 N = vec3(0.0, 0.0, 1.0);

	for (uint i = 0u; i < SAMPLE_COUNT; ++i)
	{
		vec2 Xi = Hammersley(i, SAMPLE_COUNT);
		vec3 H = ImportanceSampleGGX(Xi, N, roughness);
		vec3 L = normalize(2.0 * dot(V, H) * H - V);

		float NdotL = max(L.z, 0.0);
		float NdotH = max(H.z, 0.0);
		float VdotH = max(dot(V, H), 0.0);

		if (NdotL > 0.0)
		{
			float fNdotV_sample = max(dot(N, V), 0.0);
			float G = GeometrySmith_IBL(fNdotV_sample, NdotL, roughness);
			// Guard against division by zero at grazing angles or when NdotH approaches 0
			float G_Vis = (G * VdotH) / max(NdotH * NdotV, 0.0001);
			float Fc = pow(1.0 - VdotH, PBR_FRESNEL_POWER);

			A += (1.0 - Fc) * G_Vis;
			B += Fc * G_Vis;
		}
	}

	A /= float(SAMPLE_COUNT);
	B /= float(SAMPLE_COUNT);

	return vec2(A, B);
}

void main()
{
	// UV.x = NdotV (view angle), UV.y = roughness
	float NdotV = a_xUV.x;
	float roughness = a_xUV.y;

	// Clamp to avoid singularities
	// BRDF LUT generation uses PBR_GGX_MIN_ROUGHNESS (0.005) to compute valid table entries.
	// This is intentionally smaller than PBR_BRDF_LUT_MIN_ROUGHNESS (0.01) used at runtime
	// to ensure the table contains valid data for all values that might be sampled.
	NdotV = max(NdotV, PBR_GGX_MIN_ROUGHNESS);
	roughness = max(roughness, PBR_GGX_MIN_ROUGHNESS);

	vec2 xResult = IntegrateBRDF(NdotV, roughness);

	// Output: R = scale, G = bias, BA = unused
	o_xColor = vec4(xResult, 0.0, 1.0);
}
