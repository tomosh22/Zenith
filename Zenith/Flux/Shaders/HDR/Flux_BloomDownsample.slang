// =====================================================================
// Flux_BloomDownsample — Slang module.
//
// Subsystem: HDR. 13-tap downsample filter (CoD: AW style) for the
// bloom mip chain. Combines a tent filter with a box filter for smooth
// results without progressive brightening across mips.
// =====================================================================

#include "Common/Bindings.slang"   // spine: GLOBAL/VIEW/BINDLESS at sets 0/1/2
import Common.Fullscreen;

struct BloomConstantsLayout
{
	float  g_fThreshold;
	float  g_fIntensity;
	float2 g_xTexelSize;
};
// Pure fullscreen pass on its own source mip — no FrameConstants. The spine
// include still reserves sets 0/1/2 so this PassParams block lands at set 3.
struct PassParams
{
	ConstantBuffer<BloomConstantsLayout> BloomConstants;
	Sampler2D g_xSourceTex;
};
ParameterBlock<PassParams> g_xPassSet;

[shader("vertex")]
FullscreenVsOut vsMain(uint vertexId : SV_VertexID) { return FullscreenVS(vertexId); }

[shader("fragment")]
float4 fsMain(FullscreenVsOut i) : SV_Target
{
	float2 xUV        = i.uv;
	float2 xTexelSize = g_xPassSet.BloomConstants.g_xTexelSize;

	float3 xCenter = g_xPassSet.g_xSourceTex.Sample(xUV).rgb;

	float3 xInner = float3(0.0);
	xInner += g_xPassSet.g_xSourceTex.Sample(xUV + float2(-xTexelSize.x, 0.0)).rgb;
	xInner += g_xPassSet.g_xSourceTex.Sample(xUV + float2( xTexelSize.x, 0.0)).rgb;
	xInner += g_xPassSet.g_xSourceTex.Sample(xUV + float2(0.0, -xTexelSize.y)).rgb;
	xInner += g_xPassSet.g_xSourceTex.Sample(xUV + float2(0.0,  xTexelSize.y)).rgb;

	float3 xCorner = float3(0.0);
	xCorner += g_xPassSet.g_xSourceTex.Sample(xUV + float2(-xTexelSize.x, -xTexelSize.y)).rgb;
	xCorner += g_xPassSet.g_xSourceTex.Sample(xUV + float2( xTexelSize.x, -xTexelSize.y)).rgb;
	xCorner += g_xPassSet.g_xSourceTex.Sample(xUV + float2(-xTexelSize.x,  xTexelSize.y)).rgb;
	xCorner += g_xPassSet.g_xSourceTex.Sample(xUV + float2( xTexelSize.x,  xTexelSize.y)).rgb;

	float3 xOuter = float3(0.0);
	xOuter += g_xPassSet.g_xSourceTex.Sample(xUV + float2(-2.0 * xTexelSize.x, 0.0)).rgb;
	xOuter += g_xPassSet.g_xSourceTex.Sample(xUV + float2( 2.0 * xTexelSize.x, 0.0)).rgb;
	xOuter += g_xPassSet.g_xSourceTex.Sample(xUV + float2(0.0, -2.0 * xTexelSize.y)).rgb;
	xOuter += g_xPassSet.g_xSourceTex.Sample(xUV + float2(0.0,  2.0 * xTexelSize.y)).rgb;

	// Weights normalized to sum to 1.0 (centre 0.2, inner 0.4, corner 0.2,
	// outer 0.2) — see Flux_BloomDownsample.frag for rationale.
	float3 xResult = xCenter * 0.2 + xInner * 0.1 + xCorner * 0.05 + xOuter * 0.05;
	return float4(xResult, 1.0);
}
