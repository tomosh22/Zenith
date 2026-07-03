// =====================================================================
// Flux_TAA_Sharpen — RCAS-style post-resolve sharpen (compute).
//
// Temporal resolve slightly softens the image (history bilinear reproject +
// neighbourhood blend); a light min/max-limited sharpen restores crispness
// without ringing. 5-tap cross, clamped to the [min,max] of the 5 taps so it can
// never overshoot. amount == 0 is a pure identity (the effective kernel sums to 1
// for any amount), so the pass can stay structurally present and do nothing.
//
// A line-by-line mirror of Flux_TAASharpen in Flux/TAA/Flux_TAA_ResolveCPU.h (the
// golden-test parity contract). Reads the transient resolved output (.rgb; the .a
// linear-depth channel is not needed here) and writes the sharpened output the HDR
// bloom/tonemap consume via GetSceneColourForPostFX.
// =====================================================================

#include "Common/Bindings.slang"   // spine reserves sets 0/1/2 -> PassParams at set 3

struct TAASharpenConstantsLayout
{
	uint  g_uWidth;
	uint  g_uHeight;
	float g_fSharpenAmount;
	float g_fPad0;
};

struct PassParams
{
	Sampler2D                                 g_xSourceTex;   // transient resolved output
	RWTexture2D<float4>                       g_xOutputTex;   // sharpened output (post-FX source)
	ConstantBuffer<TAASharpenConstantsLayout> g_xSharpenConstants;
};
ParameterBlock<PassParams> g_xPassSet;

// Mirror of Flux_TAASharpen: sharpened = centre + amount*(centre - avg(neighbours)),
// clamped to the [min,max] of the 5 taps (no ring). amount 0 => identity.
float3 TAA_Sharpen(float3 xCentre, float3 xUp, float3 xDown, float3 xLeft, float3 xRight, float fAmount)
{
	float3 xAvg   = 0.25 * (xUp + xDown + xLeft + xRight);
	float3 xSharp = xCentre + fAmount * (xCentre - xAvg);
	float3 xMin   = min(xCentre, min(min(xUp, xDown), min(xLeft, xRight)));
	float3 xMax   = max(xCentre, max(max(xUp, xDown), max(xLeft, xRight)));
	return clamp(xSharp, xMin, xMax);
}

[shader("compute")]
[numthreads(8, 8, 1)]
void csMain(uint3 dispatchId : SV_DispatchThreadID)
{
	uint2 xPixel = dispatchId.xy;
	const uint uW = g_xPassSet.g_xSharpenConstants.g_uWidth;
	const uint uH = g_xPassSet.g_xSharpenConstants.g_uHeight;
	if (xPixel.x >= uW || xPixel.y >= uH)
	{
		return;
	}

	const float2 xTexel = 1.0 / float2(uW, uH);
	const float2 xUV    = (float2(xPixel) + 0.5) * xTexel;

	float3 xCentre = g_xPassSet.g_xSourceTex.SampleLevel(xUV, 0).rgb;
	float3 xUp     = g_xPassSet.g_xSourceTex.SampleLevel(xUV + float2(0.0, -1.0) * xTexel, 0).rgb;
	float3 xDown   = g_xPassSet.g_xSourceTex.SampleLevel(xUV + float2(0.0,  1.0) * xTexel, 0).rgb;
	float3 xLeft   = g_xPassSet.g_xSourceTex.SampleLevel(xUV + float2(-1.0, 0.0) * xTexel, 0).rgb;
	float3 xRight  = g_xPassSet.g_xSourceTex.SampleLevel(xUV + float2( 1.0, 0.0) * xTexel, 0).rgb;

	float3 xOut = TAA_Sharpen(xCentre, xUp, xDown, xLeft, xRight, g_xPassSet.g_xSharpenConstants.g_fSharpenAmount);
	g_xPassSet.g_xOutputTex[xPixel] = float4(xOut, 1.0);
}
