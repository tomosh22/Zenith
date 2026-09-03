// =====================================================================
// Flux_Adaptation — Slang module.
//
// Subsystem: HDR. Reads the per-frame luminance histogram and computes
// an adapted exposure scalar with eye-adaptation smoothing. Single
// 256-thread workgroup; only thread 0 does the reduction (the rest just
// stage the histogram into groupshared).
// =====================================================================

#include "Common/Bindings.slang"   // spine: GLOBAL/VIEW/BINDLESS at sets 0/1/2

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
// Pure compute on its own histogram + exposure buffers — no FrameConstants. The
// spine include still reserves sets 0/1/2 so this PassParams lands at set 3.
struct PassParams
{
	ConstantBuffer<AdaptationConstantsLayout> AdaptationConstants;
	RWStructuredBuffer<uint>  g_auHistogram;
	globallycoherent RWStructuredBuffer<float> g_afExposureData;
};
ParameterBlock<PassParams> g_xPassSet;

groupshared uint  s_auLocalHistogram[256];
groupshared float s_afPartialSum[256];

// ---- Highlight-protection tunables -------------------------------------
// The fraction of pixels allowed ABOVE the protected level: the sun disc,
// specular pinpricks and emissive sources are meant to clip, ordinary surfaces
// are not. The headroom is where that near-maximum lands in exposed scene units;
// AgX maps scene 6.0 to 0.905 display and 16.0 to 0.995 (measured against the
// shipped curve), so 6.0 leaves the brightest real surface bright but off the clip.
static const float fHIGHLIGHT_PERCENTILE = 0.99;
static const float fHIGHLIGHT_HEADROOM   = 6.0;

[shader("compute")]
[numthreads(256, 1, 1)]
void csMain(uint3 dispatchId : SV_DispatchThreadID)
{
	uint uLocalIdx = dispatchId.x; // single workgroup, dispatchId == localIdx

	s_auLocalHistogram[uLocalIdx] = g_xPassSet.g_auHistogram[uLocalIdx];
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
	g_xPassSet.g_afExposureData[3] = (float)uMaxBinCount;

	if (fTotalCount < 1.0)
	{
		g_xPassSet.g_afExposureData[0] = 0.18;
		g_xPassSet.g_afExposureData[2] = 1.0;
		// Nothing metered yet: resolve the "unmetered" seed sentinel (<= 0) to
		// identity so the tonemap never multiplies by zero.
		if (g_xPassSet.g_afExposureData[1] <= 0.0) g_xPassSet.g_afExposureData[1] = 1.0;
		return;
	}

	// Percentile bounds — ignore outliers at both ends.
	float fLowThreshold  = fTotalCount * g_xPassSet.AdaptationConstants.g_fLowPercentile;
	float fHighThreshold = fTotalCount * g_xPassSet.AdaptationConstants.g_fHighPercentile;
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

	float fAvgLogLum    = (fWeightedSum / fValidCount) * g_xPassSet.AdaptationConstants.g_fLogLumRange + g_xPassSet.AdaptationConstants.g_fMinLogLum;
	float fAvgLuminance = exp2(fAvgLogLum);

	// Target exposure = key / average.
	float fTargetExposure = g_xPassSet.AdaptationConstants.g_fTargetLuminance / max(fAvgLuminance, 0.001);

	// ---- Highlight protection (expose to the right, do not clip) ------------
	// Average-only metering has no idea how far the brightest real content sits
	// above the average, so a scene holding a bright lambertian surface in full
	// sun meters "correctly" and clips that surface to paper white. A
	// photographer stops down instead. Derive a second exposure that places the
	// NEAR-MAXIMUM scene luminance just under the tone curve's clipping point,
	// and take the SMALLER of the two so this can only protect highlights, never
	// brighten. Probing the near-maximum rather than the 95th percentile matters:
	// the surface that clips is by definition in the top few per cent, so a
	// mid-percentile probe protects nothing until it has dragged the whole
	// mid-tone range down with it.
	float fClipThreshold = fTotalCount * fHIGHLIGHT_PERCENTILE;
	float fClipRunning   = 0.0;
	uint  uClipBin       = uHighBin;
	for (uint c = 0; c < 256; c++)
	{
		fClipRunning += (float)s_auLocalHistogram[c];
		if (fClipRunning <= fClipThreshold) uClipBin = c;
	}
	float fClipLogLum    = (((float)uClipBin + 0.5) / 256.0) * g_xPassSet.AdaptationConstants.g_fLogLumRange
						 + g_xPassSet.AdaptationConstants.g_fMinLogLum;
	float fClipLuminance = exp2(fClipLogLum);
	float fHighlightExposure = fHIGHLIGHT_HEADROOM / max(fClipLuminance, 0.001);
	fTargetExposure = min(fTargetExposure, fHighlightExposure);

	fTargetExposure       = clamp(fTargetExposure,
								  g_xPassSet.AdaptationConstants.g_fMinExposure,
								  g_xPassSet.AdaptationConstants.g_fMaxExposure);
	g_xPassSet.g_afExposureData[0] = fAvgLuminance;
	g_xPassSet.g_afExposureData[2] = fTargetExposure;

	// Eye adaptation — smooth exponential approach to the target. A current
	// exposure <= 0 is the "unmetered" seed sentinel (set at initialise /
	// reset / auto-exposure enable): SNAP to the first computed target so the
	// first metered frame is correctly exposed — there is no authored seed
	// value to tune.
	float fCurrentExposure = g_xPassSet.g_afExposureData[1];
	if (fCurrentExposure <= 0.0) fCurrentExposure = fTargetExposure;
	float fAdaptationRate  = 1.0 - exp(-g_xPassSet.AdaptationConstants.g_fDeltaTime * g_xPassSet.AdaptationConstants.g_fAdaptationSpeed);
	g_xPassSet.g_afExposureData[1]    = lerp(fCurrentExposure, fTargetExposure, fAdaptationRate);

	// Histogram clearing happens at the start of next frame's Luminance
	// dispatch; nothing to reset here.
}
