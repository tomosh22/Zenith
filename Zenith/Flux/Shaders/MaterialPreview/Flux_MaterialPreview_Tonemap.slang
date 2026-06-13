// =====================================================================
// Flux_MaterialPreview_Tonemap — Slang module.
//
// Subsystem: MaterialPreview (TOOLS-only editor material preview).
// Fixed-exposure ACES tonemap + gamma encode from the preview HDR
// target to the preview LDR target that ImGui samples. Deliberately
// independent of the scene HDR pipeline (no adaptation, no bloom) so
// the preview's exposure is stable while the user edits.
//
// Bindings:
//   PreviewTonemapConstants set=0 binding=0
//   g_xHDRTex               set=0 binding=1
// =====================================================================

import Common.Fullscreen;

struct PreviewTonemapConstantsLayout
{
	float g_fExposure;
};
[[vk::binding(0, 0)]] ConstantBuffer<PreviewTonemapConstantsLayout> PreviewTonemapConstants;

[[vk::binding(1, 0)]] Sampler2D g_xHDRTex;

[shader("vertex")]
FullscreenVsOut vsMain(uint vertexId : SV_VertexID) { return FullscreenVS(vertexId); }

// Narkowicz 2015 ACES approximation.
float3 ACESFilm(float3 x)
{
	const float a = 2.51;
	const float b = 0.03;
	const float c = 2.43;
	const float d = 0.59;
	const float e = 0.14;
	return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

[shader("fragment")]
float4 fsMain(FullscreenVsOut i) : SV_Target
{
	float3 xHDR = g_xHDRTex.Sample(i.uv).rgb * PreviewTonemapConstants.g_fExposure;
	float3 xLDR = ACESFilm(xHDR);
	// Gamma encode for the UNORM preview target (sampled by ImGui as-is).
	xLDR = pow(xLDR, float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2));
	return float4(xLDR, 1.0);
}
