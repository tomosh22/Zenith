// =====================================================================
// Flux_MaterialPreview_Tonemap — Slang module.
//
// Subsystem: MaterialPreview (TOOLS-only editor material preview).
// Fixed-exposure ACES tonemap + gamma encode from the preview HDR
// target to the preview LDR target that ImGui samples. Deliberately
// independent of the scene HDR pipeline (no adaptation, no bloom) so
// the preview's exposure is stable while the user edits.
//
// ParameterBlock spine: a pure fullscreen composite on its own HDR
// texture + exposure constant — no camera/sun. The spine include still
// reserves sets 0/1/2 so this PassParams block lands at set 3.
// =====================================================================

#include "Common/Bindings.slang"   // spine: GLOBAL/VIEW/BINDLESS at sets 0/1/2
import Common.Fullscreen;

struct PreviewTonemapConstantsLayout
{
	float g_fExposure;
};
// This pass's own resources land at set 3 (after the persistent spine sets).
struct PassParams
{
	ConstantBuffer<PreviewTonemapConstantsLayout> PreviewTonemapConstants;
	Sampler2D                                     g_xHDRTex;
};
ParameterBlock<PassParams> g_xPassSet;

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
	float3 xHDR = g_xPassSet.g_xHDRTex.Sample(i.uv).rgb * g_xPassSet.PreviewTonemapConstants.g_fExposure;
	float3 xLDR = ACESFilm(xHDR);
	// Gamma encode for the UNORM preview target (sampled by ImGui as-is).
	xLDR = pow(xLDR, float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2));
	return float4(xLDR, 1.0);
}
