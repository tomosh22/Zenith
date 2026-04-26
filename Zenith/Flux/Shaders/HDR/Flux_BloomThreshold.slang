// =====================================================================
// Flux_BloomThreshold — Slang module.
//
// Subsystem: HDR. Extracts bright areas from the HDR scene target with
// a soft-knee threshold. Output drives the downsample chain.
//
// Shares its vertex stage with the other bloom passes via
// Common.Fullscreen.
// =====================================================================

import Common.Fullscreen;

struct BloomConstantsLayout
{
	float  g_fThreshold;
	float  g_fIntensity;
	float2 g_xTexelSize;
};
[[vk::binding(0, 0)]] ConstantBuffer<BloomConstantsLayout> BloomConstants;
[[vk::binding(1, 0)]] Sampler2D g_xHDRTex;

[shader("vertex")]
FullscreenVsOut vsMain(uint vertexId : SV_VertexID) { return FullscreenVS(vertexId); }

float Luminance(float3 colour)
{
	return dot(colour, float3(0.2126, 0.7152, 0.0722));
}

[shader("fragment")]
float4 fsMain(FullscreenVsOut i) : SV_Target
{
	float3 xColour = g_xHDRTex.Sample(i.uv).rgb;

	// Soft knee threshold: smooth transition from 0 to 1 contribution.
	// fKneeWidth controls the range over which the transition occurs.
	float fLuminance   = Luminance(xColour);
	float fKneeWidth   = BloomConstants.g_fThreshold * 0.5;
	float fContribution;
	if (fKneeWidth > 0.0001)
	{
		fContribution = max(0.0, fLuminance - BloomConstants.g_fThreshold + fKneeWidth);
		fContribution = min(fContribution / fKneeWidth, 1.0);
		fContribution = fContribution * fContribution * (3.0 - 2.0 * fContribution);
	}
	else
	{
		fContribution = fLuminance > 0.0 ? 1.0 : 0.0;
	}

	return float4(xColour * fContribution, 1.0);
}
