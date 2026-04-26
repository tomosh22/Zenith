// =====================================================================
// Flux_BRDFIntegration — Slang module.
//
// Subsystem: IBL. Pre-computes the split-sum BRDF integration LUT that
// the deferred lighting pass samples for image-based specular. Runs
// once at IBL init; output: RG = (scale, bias) such that the IBL
// specular term is F0 * scale + bias.
//
// Imports Common.PBR for Hammersley / ImportanceSampleGGX /
// GeometrySmith_IBL / PBR_FRESNEL_POWER. Standalone otherwise — does
// not depend on FrameConstants.
// =====================================================================

import Common.Fullscreen;
import Common.PBR;

[shader("vertex")]
FullscreenVsOut vsMain(uint vertexId : SV_VertexID) { return FullscreenVS(vertexId); }

// 512 samples matches GLSL — quality is nearly identical to 1024 at
// half the cost; the LUT is pre-computed so this mainly affects
// iteration time during development.
static const uint SAMPLE_COUNT = 512u;

float2 IntegrateBRDF(float NdotV, float roughness)
{
	float3 V;
	V.x = sqrt(1.0 - NdotV * NdotV);
	V.y = 0.0;
	V.z = NdotV;

	float A = 0.0;
	float B = 0.0;
	float3 N = float3(0.0, 0.0, 1.0);

	for (uint i = 0u; i < SAMPLE_COUNT; ++i)
	{
		float2 Xi = Hammersley(i, SAMPLE_COUNT);
		float3 H  = ImportanceSampleGGX(Xi, N, roughness);
		float3 L  = normalize(2.0 * dot(V, H) * H - V);

		float NdotL = max(L.z, 0.0);
		float NdotH = max(H.z, 0.0);
		float VdotH = max(dot(V, H), 0.0);

		if (NdotL > 0.0)
		{
			float fNdotV_sample = max(dot(N, V), 0.0);
			float G     = GeometrySmith_IBL(fNdotV_sample, NdotL, roughness);
			// Guard against division by zero at grazing angles where
			// NdotH * NdotV approaches 0.
			float G_Vis = (G * VdotH) / max(NdotH * NdotV, 0.0001);
			float Fc    = pow(1.0 - VdotH, PBR_FRESNEL_POWER);

			A += (1.0 - Fc) * G_Vis;
			B += Fc * G_Vis;
		}
	}

	A /= (float)SAMPLE_COUNT;
	B /= (float)SAMPLE_COUNT;
	return float2(A, B);
}

[shader("fragment")]
float4 fsMain(FullscreenVsOut input) : SV_Target
{
	float NdotV     = input.uv.x;
	float roughness = input.uv.y;

	// Clamp to avoid GGX singularities. BRDF LUT generation uses
	// PBR_GGX_MIN_ROUGHNESS (0.005); this is intentionally smaller than
	// PBR_BRDF_LUT_MIN_ROUGHNESS (0.01) used at runtime to ensure the
	// table has valid entries across the sampled range.
	NdotV     = max(NdotV,     PBR_GGX_MIN_ROUGHNESS);
	roughness = max(roughness, PBR_GGX_MIN_ROUGHNESS);

	float2 xResult = IntegrateBRDF(NdotV, roughness);
	return float4(xResult, 0.0, 1.0);
}
