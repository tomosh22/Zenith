// =====================================================================
// Flux_BloomUpsample — Slang module.
//
// Subsystem: HDR. 9-tap tent filter for high-quality bloom upsample.
// Used with additive blending against the next-larger mip; weights sum
// to 1.0 (centre 4/16, edges 2/16, corners 1/16).
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

	float3 xResult = float3(0.0);
	// Corners — weight 1/16
	xResult += g_xPassSet.g_xSourceTex.Sample(xUV + float2(-xTexelSize.x, -xTexelSize.y)).rgb * 0.0625;
	xResult += g_xPassSet.g_xSourceTex.Sample(xUV + float2( xTexelSize.x, -xTexelSize.y)).rgb * 0.0625;
	xResult += g_xPassSet.g_xSourceTex.Sample(xUV + float2(-xTexelSize.x,  xTexelSize.y)).rgb * 0.0625;
	xResult += g_xPassSet.g_xSourceTex.Sample(xUV + float2( xTexelSize.x,  xTexelSize.y)).rgb * 0.0625;
	// Edges — weight 2/16
	xResult += g_xPassSet.g_xSourceTex.Sample(xUV + float2(-xTexelSize.x, 0.0)).rgb * 0.125;
	xResult += g_xPassSet.g_xSourceTex.Sample(xUV + float2( xTexelSize.x, 0.0)).rgb * 0.125;
	xResult += g_xPassSet.g_xSourceTex.Sample(xUV + float2(0.0, -xTexelSize.y)).rgb * 0.125;
	xResult += g_xPassSet.g_xSourceTex.Sample(xUV + float2(0.0,  xTexelSize.y)).rgb * 0.125;
	// Centre — weight 4/16
	xResult += g_xPassSet.g_xSourceTex.Sample(xUV).rgb * 0.25;

	return float4(xResult, 1.0);
}
