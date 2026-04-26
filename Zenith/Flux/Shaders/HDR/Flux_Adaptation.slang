// =====================================================================
// Flux_Adaptation — Slang module.
//
// Subsystem: HDR. Reads the per-frame luminance histogram and computes
// an adapted exposure scalar with eye-adaptation smoothing. Single
// 256-thread workgroup; only thread 0 does the reduction (the rest just
// stage the histogram into groupshared).
// =====================================================================

struct AdaptationConstantsLayout
{
	float g_fMinLogLum;
	float g_fLogLumRange;
	float g_fDeltaTime;
	float g_fAdaptationSpeed;
	float g_fTargetLuminance;
	float g_fMinExposure;
	float g_fMaxExposure;
	float g_fLowPercentile;
	float g_fHighPercentile;
	uint  g_uTotalPixels;
	uint  g_uPad0;
	uint  g_uPad1;
};
[[vk::binding(0, 0)]] ConstantBuffer<AdaptationConstantsLayout> AdaptationConstants;
[[vk::binding(1, 0)]] RWStructuredBuffer<uint>  g_auHistogram;
[[vk::binding(2, 0)]] globallycoherent RWStructuredBuffer<float> g_afExposureData;

groupshared uint  s_auLocalHistogram[256];
groupshared float s_afPartialSum[256];

[shader("compute")]
[numthreads(256, 1, 1)]
void csMain(uint3 dispatchId : SV_DispatchThreadID)
{
	uint uLocalIdx = dispatchId.x; // single workgroup, dispatchId == localIdx

	s_auLocalHistogram[uLocalIdx] = g_auHistogram[uLocalIdx];
	GroupMemoryBarrierWithGroupSync();
	s_afPartialSum[uLocalIdx] = (float)s_auLocalHistogram[uLocalIdx];
	GroupMemoryBarrierWithGroupSync();

	if (uLocalIdx != 0) return;

	// Histogram total + max bin (the latter feeds the tone-mapping
	// histogram-overlay debug mode so it doesn't loop 256 times per pixel).
	float fTotalCount   = 0.0;
	uint  uMaxBinCount  = 1u;
	for (uint i = 0; i < 256; i++)
	{
		fTotalCount   += (float)s_auLocalHistogram[i];
		uMaxBinCount   = max(uMaxBinCount, s_auLocalHistogram[i]);
	}
	g_afExposureData[3] = (float)uMaxBinCount;

	if (fTotalCount < 1.0)
	{
		g_afExposureData[0] = 0.18;
		g_afExposureData[2] = 1.0;
		return;
	}

	// Percentile bounds — ignore outliers at both ends.
	float fLowThreshold  = fTotalCount * AdaptationConstants.g_fLowPercentile;
	float fHighThreshold = fTotalCount * AdaptationConstants.g_fHighPercentile;
	float fRunningCount  = 0.0;
	uint  uLowBin        = 0;
	uint  uHighBin       = 255;
	for (uint i = 0; i < 256; i++)
	{
		fRunningCount += (float)s_auLocalHistogram[i];
		if (fRunningCount < fLowThreshold)  uLowBin  = i + 1;
		if (fRunningCount <= fHighThreshold) uHighBin = i;
	}

	// Weighted average luminance within valid range.
	float fWeightedSum = 0.0;
	float fValidCount  = 0.0;
	for (uint i = uLowBin; i <= uHighBin; i++)
	{
		float fBinValue = ((float)i + 0.5) / 256.0;
		float fCount    = (float)s_auLocalHistogram[i];
		fWeightedSum   += fBinValue * fCount;
		fValidCount    += fCount;
	}
	if (fValidCount < 1.0) fValidCount = 1.0;

	float fAvgLogLum    = (fWeightedSum / fValidCount) * AdaptationConstants.g_fLogLumRange + AdaptationConstants.g_fMinLogLum;
	float fAvgLuminance = exp2(fAvgLogLum);

	// Target exposure = key / average. Clamp to user range.
	float fTargetExposure = AdaptationConstants.g_fTargetLuminance / max(fAvgLuminance, 0.001);
	fTargetExposure       = clamp(fTargetExposure,
								  AdaptationConstants.g_fMinExposure,
								  AdaptationConstants.g_fMaxExposure);
	g_afExposureData[0] = fAvgLuminance;
	g_afExposureData[2] = fTargetExposure;

	// Eye adaptation — smooth exponential approach to the target.
	float fCurrentExposure = g_afExposureData[1];
	float fAdaptationRate  = 1.0 - exp(-AdaptationConstants.g_fDeltaTime * AdaptationConstants.g_fAdaptationSpeed);
	g_afExposureData[1]    = lerp(fCurrentExposure, fTargetExposure, fAdaptationRate);

	// Histogram clearing happens at the start of next frame's Luminance
	// dispatch; nothing to reset here.
}
