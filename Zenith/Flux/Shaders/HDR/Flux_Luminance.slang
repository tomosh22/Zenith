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

#include "Common/Bindings.slang"   // spine: GLOBAL/VIEW/BINDLESS at sets 0/1/2

struct LuminanceConstantsLayout
{
	uint2 g_xImageSize;
	float g_fMinLogLum;
	float g_fLogLumRange;
};
// Pure compute on its own HDR texture + histogram buffer — no FrameConstants.
// The spine include still reserves sets 0/1/2 so this PassParams lands at set 3.
struct PassParams
{
	ConstantBuffer<LuminanceConstantsLayout> LuminanceConstants;
	Sampler2D g_xHDRTex;
	RWStructuredBuffer<uint> g_auHistogram;
};
ParameterBlock<PassParams> g_xPassSet;

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
	if (xPixelCoord.x < g_xPassSet.LuminanceConstants.g_xImageSize.x &&
		xPixelCoord.y < g_xPassSet.LuminanceConstants.g_xImageSize.y)
	{
		float2 xUV = (float2(xPixelCoord) + 0.5) / float2(g_xPassSet.LuminanceConstants.g_xImageSize);
		float3 xColour = g_xPassSet.g_xHDRTex.SampleLevel(xUV, 0).rgb;

		float fLuminance = RGBToLuminance(xColour);
		// Dark pixels land in bin 0 rather than being dropped: skipping them let a
		// shadowed or night frame be metered by its few bright pixels alone, so
		// the exposure locked dark and never opened up.
		float fLogLum = clamp((log2(max(fLuminance, 1e-6)) - g_xPassSet.LuminanceConstants.g_fMinLogLum) /
							   g_xPassSet.LuminanceConstants.g_fLogLumRange, 0.0, 1.0);
		uint  uBin    = min((uint)(fLogLum * 255.0), 255u);
		// Centre-weighted metering (a camera's default pattern): the middle of the
		// frame counts three times, the mid-field twice, the edges once — so a
		// sky-heavy horizon shot no longer underexposes the subject on the ground.
		float2 xCentred = xUV - 0.5;
		float  fRadius  = length(xCentred * float2(1.0, 0.75));
		uint   uWeight  = (fRadius < 0.18) ? 3u : ((fRadius < 0.34) ? 2u : 1u);
		uint   uOldVal;
		InterlockedAdd(s_auLocalHistogram[uBin], uWeight, uOldVal);
	}
	GroupMemoryBarrierWithGroupSync();

	// Phase 3: merge local histogram bin into the global buffer. Each
	// thread owns exactly one bin; merge only if non-zero to spare bus.
	uint uLocalCount = s_auLocalHistogram[uLocalIndex];
	if (uLocalCount > 0u)
	{
		uint uOldVal;
		InterlockedAdd(g_xPassSet.g_auHistogram[uLocalIndex], uLocalCount, uOldVal);
	}
}
