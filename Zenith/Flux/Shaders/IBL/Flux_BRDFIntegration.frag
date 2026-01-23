#version 450 core

// BRDF Integration LUT Generator
// Computes split-sum approximation for IBL specular lighting
// Output: RG = (scale, bias) for Fresnel term: F0 * scale + bias

#include "../Common.fxh"

layout(location = 0) in vec2 a_xUV;

layout(location = 0) out vec4 o_xColor;

const float PI = 3.14159265359;
// 512 samples provides nearly identical quality to 1024 with half the cost
// BRDF LUT is pre-computed once, so this mainly helps iteration during development
const uint SAMPLE_COUNT = 512u;

// Van der Corput sequence for quasi-random sampling
float RadicalInverse_VdC(uint bits)
{
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

// Hammersley sequence for low-discrepancy sampling
vec2 Hammersley(uint i, uint N)
{
	return vec2(float(i) / float(N), RadicalInverse_VdC(i));
}

// Importance sample GGX normal distribution
// Returns a half-vector in tangent space
vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness)
{
	float a = roughness * roughness;

	float phi = 2.0 * PI * Xi.x;
	float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
	float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

	// Spherical to cartesian (tangent space)
	vec3 H;
	H.x = cos(phi) * sinTheta;
	H.y = sin(phi) * sinTheta;
	H.z = cosTheta;

	// Tangent space to world space
	vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
	vec3 tangent = normalize(cross(up, N));
	vec3 bitangent = cross(N, tangent);

	vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
	return normalize(sampleVec);
}

// Schlick's approximation for geometry function (IBL version)
float GeometrySchlickGGX(float NdotV, float roughness)
{
	// For IBL, use k = (roughness^2) / 2
	float a = roughness;
	float k = (a * a) / 2.0;

	float nom = NdotV;
	float denom = NdotV * (1.0 - k) + k;

	// Guard against division by zero (can occur at grazing angles with very low roughness)
	return nom / max(denom, 0.0001);
}

// Smith's method for combined geometry term
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
	float NdotV = max(dot(N, V), 0.0);
	float NdotL = max(dot(N, L), 0.0);
	float ggx2 = GeometrySchlickGGX(NdotV, roughness);
	float ggx1 = GeometrySchlickGGX(NdotL, roughness);

	return ggx1 * ggx2;
}

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
			float G = GeometrySmith(N, V, L, roughness);
			// Guard against division by zero at grazing angles or when NdotH approaches 0
			float G_Vis = (G * VdotH) / max(NdotH * NdotV, 0.0001);
			float Fc = pow(1.0 - VdotH, 5.0);

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
	// Use smaller roughness minimum (0.001) to preserve polished surface data
	NdotV = max(NdotV, 0.001);
	roughness = max(roughness, 0.001);

	vec2 xResult = IntegrateBRDF(NdotV, roughness);

	// Output: R = scale, G = bias, BA = unused
	o_xColor = vec4(xResult, 0.0, 1.0);
}
