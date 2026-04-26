// =====================================================================
// Flux_Luminance — Slang module.
//
// Subsystem: HDR. 256-bin log-space luminance histogram, accumulated
// per-workgroup in groupshared memory then merged into a global buffer.
//
// Workgroup is 16x16 = 256 threads exactly so phase 1 (clear) and
// phase 3 (merge) can each handle one bin per thread; do not change
// without reworking those phases.
// =====================================================================

struct LuminanceConstantsLayout
{
	uint2 g_xImageSize;
	float g_fMinLogLum;
	float g_fLogLumRange;
};
[[vk::binding(0, 0)]] ConstantBuffer<LuminanceConstantsLayout> LuminanceConstants;
[[vk::binding(1, 0)]] Sampler2D g_xHDRTex;
[[vk::binding(2, 0)]] RWStructuredBuffer<uint> g_auHistogram;

groupshared uint s_auLocalHistogram[256];

float RGBToLuminance(float3 xColour)
{
	return dot(xColour, float3(0.2126, 0.7152, 0.0722));
}

[shader("compute")]
[numthreads(16, 16, 1)]
void csMain(uint3 dispatchId : SV_DispatchThreadID, uint groupIndex : SV_GroupIndex)
{
	uint2 xPixelCoord = dispatchId.xy;
	uint  uLocalIndex = groupIndex; // 0..255

	// Phase 1: each thread clears one bin.
	s_auLocalHistogram[uLocalIndex] = 0u;
	GroupMemoryBarrierWithGroupSync();

	// Phase 2: accumulate into local histogram.
	if (xPixelCoord.x < LuminanceConstants.g_xImageSize.x &&
		xPixelCoord.y < LuminanceConstants.g_xImageSize.y)
	{
		float2 xUV = (float2(xPixelCoord) + 0.5) / float2(LuminanceConstants.g_xImageSize);
		float3 xColour = g_xHDRTex.SampleLevel(xUV, 0).rgb;

		float fLuminance = RGBToLuminance(xColour);
		if (fLuminance >= 0.001)
		{
			float fLogLum = clamp((log2(fLuminance) - LuminanceConstants.g_fMinLogLum) /
								   LuminanceConstants.g_fLogLumRange, 0.0, 1.0);
			uint  uBin    = min((uint)(fLogLum * 255.0), 255u);
			uint  uOldVal;
			InterlockedAdd(s_auLocalHistogram[uBin], 1u, uOldVal);
		}
	}
	GroupMemoryBarrierWithGroupSync();

	// Phase 3: merge local histogram bin into the global buffer. Each
	// thread owns exactly one bin; merge only if non-zero to spare bus.
	uint uLocalCount = s_auLocalHistogram[uLocalIndex];
	if (uLocalCount > 0u)
	{
		uint uOldVal;
		InterlockedAdd(g_auHistogram[uLocalIndex], uLocalCount, uOldVal);
	}
}
