#include "UnitTests/Zenith_UnitTests.h"
#include "Flux/TAA/Flux_TAA_ResolveCPU.h"

// ============================================================================
// TAA resolve pure-core unit tests (Part 1). YCoCg, variance clip, neighbourhood
// moments, disocclusion, blend, Karis weighting, sharpen, upscale kernel + render-dim
// quantisation. No GPU / renderer — these pin the shader math a headless golden checks.
// ============================================================================

namespace
{
	void TAA_AssertVec3Near(const Zenith_Maths::Vector3& xA, const Zenith_Maths::Vector3& xB, float fEps, const char* strWhat)
	{
		ZENITH_ASSERT_EQ_FLOAT(xA.x, xB.x, fEps, "%s (x)", strWhat);
		ZENITH_ASSERT_EQ_FLOAT(xA.y, xB.y, fEps, "%s (y)", strWhat);
		ZENITH_ASSERT_EQ_FLOAT(xA.z, xB.z, fEps, "%s (z)", strWhat);
	}
}

// ---- YCoCg round-trip -------------------------------------------------------

ZENITH_TEST(TAAResolve, YCoCgRoundTrip)
{
	const Zenith_Maths::Vector3 axColours[4] = {
		Zenith_Maths::Vector3(0.2f, 0.5f, 0.8f),
		Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f),
		Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f),
		Zenith_Maths::Vector3(0.73f, 0.11f, 0.42f) };
	for (u_int u = 0; u < 4u; ++u)
	{
		const Zenith_Maths::Vector3 xRT = Flux_YCoCgToRGB(Flux_RGBToYCoCg(axColours[u]));
		TAA_AssertVec3Near(xRT, axColours[u], 1e-5f, "YCoCg round-trip");
	}
}

// ---- variance clip ----------------------------------------------------------

ZENITH_TEST(TAAResolve, VarianceClipInsideUnchanged)
{
	const Zenith_Maths::Vector3 xMean(0.5f), xSigma(0.1f);
	const Zenith_Maths::Vector3 xHist(0.55f, 0.5f, 0.5f);   // 0.5 sigma out — inside the gamma=1 box
	const Zenith_Maths::Vector3 xClip = Flux_TAAVarianceClip(xMean, xSigma, 1.0f, xHist);
	TAA_AssertVec3Near(xClip, xHist, 1e-6f, "history inside the box is unchanged");
}

ZENITH_TEST(TAAResolve, VarianceClipOutsidePulledToBoundary)
{
	const Zenith_Maths::Vector3 xMean(0.5f), xSigma(0.1f);
	const Zenith_Maths::Vector3 xHist(1.0f, 0.5f, 0.5f);    // 5 sigma out on x
	const Zenith_Maths::Vector3 xClip = Flux_TAAVarianceClip(xMean, xSigma, 1.0f, xHist);
	ZENITH_ASSERT_EQ_FLOAT(xClip.x, 0.6f, 1e-5f, "clipped to mean + gamma*sigma = 0.6");
	ZENITH_ASSERT_EQ_FLOAT(xClip.y, 0.5f, 1e-5f, "y unchanged");
}

ZENITH_TEST(TAAResolve, VarianceClipGammaWidensBox)
{
	const Zenith_Maths::Vector3 xMean(0.5f), xSigma(0.1f);
	const Zenith_Maths::Vector3 xHist(1.0f, 0.5f, 0.5f);    // 5 sigma out
	// gamma 6 => half-extent 0.6 >= 0.5 => now inside => kept unchanged.
	const Zenith_Maths::Vector3 xClip = Flux_TAAVarianceClip(xMean, xSigma, 6.0f, xHist);
	TAA_AssertVec3Near(xClip, xHist, 1e-5f, "larger gamma keeps the sample");
}

ZENITH_TEST(TAAResolve, VarianceClipZeroVarianceCollapsesToMean)
{
	const Zenith_Maths::Vector3 xMean(0.5f), xSigma(0.0f);   // flat neighbourhood
	const Zenith_Maths::Vector3 xHist(0.6f, 0.5f, 0.5f);
	const Zenith_Maths::Vector3 xClip = Flux_TAAVarianceClip(xMean, xSigma, 1.0f, xHist);
	ZENITH_ASSERT_EQ_FLOAT(xClip.x, 0.5f, 1e-3f, "zero variance collapses history to the mean");
}

// ---- neighbourhood moments --------------------------------------------------

ZENITH_TEST(TAAResolve, MomentsConstantNeighbourhood)
{
	Zenith_Maths::Vector3 axS[9];
	for (u_int u = 0; u < 9u; ++u) { axS[u] = Zenith_Maths::Vector3(0.5f, 0.3f, 0.2f); }
	Zenith_Maths::Vector3 xMean, xSigma;
	Flux_TAANeighbourhoodMoments(axS, 9u, xMean, xSigma);
	TAA_AssertVec3Near(xMean, Zenith_Maths::Vector3(0.5f, 0.3f, 0.2f), 1e-6f, "constant mean");
	// One-pass variance (E[x^2] - E[x]^2) under /fp:fast leaves a tiny (~1.5e-4) residual on the
	// non-power-of-two channels (0.3, 0.2) — the GPU shader uses the same one-pass form, and a
	// sigma this small just collapses the clip box to the mean (correct for a flat neighbourhood).
	TAA_AssertVec3Near(xSigma, Zenith_Maths::Vector3(0.0f), 2e-3f, "constant sigma is ~zero");
}

ZENITH_TEST(TAAResolve, MomentsPinnedOutlier)
{
	Zenith_Maths::Vector3 axS[9];
	for (u_int u = 0; u < 9u; ++u) { axS[u] = Zenith_Maths::Vector3(0.0f); }
	axS[4] = Zenith_Maths::Vector3(0.9f, 0.0f, 0.0f);   // one bright sample
	Zenith_Maths::Vector3 xMean, xSigma;
	Flux_TAANeighbourhoodMoments(axS, 9u, xMean, xSigma);
	ZENITH_ASSERT_EQ_FLOAT(xMean.x, 0.1f, 1e-5f, "mean.x = 0.9/9");            // 0.1
	ZENITH_ASSERT_EQ_FLOAT(xSigma.x, sqrtf(0.08f), 1e-5f, "sigma.x = sqrt(E[x^2]-mean^2)");   // sqrt(0.09-0.01)
}

// ---- disocclusion -----------------------------------------------------------

namespace
{
	// A camera looking down +Z (the engine's view convention) at world z = fCamZ.
	// glm is column-major, so [3] is the translation column.
	Zenith_Maths::Matrix4 TAA_TestViewProj(float fCamZ)
	{
		const Zenith_Maths::Matrix4 xProj =
			Zenith_Maths::PerspectiveProjection(1.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
		Zenith_Maths::Matrix4 xView(1.0f);
		xView[3][2] = -fCamZ;
		return xProj * xView;
	}
}

// The precondition the whole reprojection rests on: Zenith_Maths::PerspectiveProjection
// is perspectiveLH_ZO, so clip.w IS view-space z. Pinned rather than trusted -- if the
// projection convention ever changes, this reddens instead of the disocclusion test
// silently comparing nonsense.
ZENITH_TEST(TAAResolve, ClipWIsViewZ)
{
	const Zenith_Maths::Matrix4 xProj =
		Zenith_Maths::PerspectiveProjection(1.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
	ZENITH_ASSERT_EQ_FLOAT(xProj[2][3], 1.0f, 1e-6f, "M[2][3] == 1 (clip.w picks up view z)");

	// End-to-end: a point 10 in front of a camera parked at world z = 2.
	const float fViewZ = Flux_TAAExpectedPrevViewZ(
		TAA_TestViewProj(2.0f), Zenith_Maths::Vector3(0.7f, -0.3f, 12.0f));
	ZENITH_ASSERT_EQ_FLOAT(fViewZ, 10.0f, 1e-4f, "clip.w == distance in front of the camera");
}

ZENITH_TEST(TAAResolve, ExpectedPrevViewZIsIndependentOfScreenPosition)
{
	// Only DEPTH may come out of this -- x/y offsets must not leak into it, or the
	// tolerance would have to absorb screen position as well as depth.
	const Zenith_Maths::Matrix4 xVP = TAA_TestViewProj(0.0f);
	ZENITH_ASSERT_EQ_FLOAT(Flux_TAAExpectedPrevViewZ(xVP, Zenith_Maths::Vector3(0.0f, 0.0f, 7.0f)),
		7.0f, 1e-4f, "centre");
	ZENITH_ASSERT_EQ_FLOAT(Flux_TAAExpectedPrevViewZ(xVP, Zenith_Maths::Vector3(3.5f, -2.25f, 7.0f)),
		7.0f, 1e-4f, "far off-axis, same depth");
}

// A flat surface facing the camera collapses the range to a point, which is where the
// relative threshold alone governs -- the same arithmetic the pre-range test had.
ZENITH_TEST(TAAResolve, DisocclusionAgreeKeeps)
{
	ZENITH_ASSERT_FALSE(Flux_TAADisocclusionReject(10.1f, 10.0f, 10.0f, 10.0f, 10.0f, 0.05f),
		"close depths kept");
}

ZENITH_TEST(TAAResolve, DisocclusionDisagreeRejects)
{
	ZENITH_ASSERT_TRUE(Flux_TAADisocclusionReject(15.0f, 10.0f, 10.0f, 10.0f, 10.0f, 0.05f),
		"far-apart depths rejected");
}

ZENITH_TEST(TAAResolve, DisocclusionThresholdBoundary)
{
	// Flat neighbourhood at 10: reject when hist > 10 + 0.05*hist, i.e. hist > 10.526.
	ZENITH_ASSERT_FALSE(Flux_TAADisocclusionReject(10.5f, 10.0f, 10.0f, 10.0f, 10.0f, 0.05f),
		"just inside threshold kept");
	ZENITH_ASSERT_TRUE (Flux_TAADisocclusionReject(11.0f, 10.0f, 10.0f, 10.0f, 10.0f, 0.05f),
		"just outside threshold rejected");
}

// ★ THE REGRESSION THIS WHOLE CHANGE EXISTS FOR. A static surface, a camera walking
// toward it: the reprojected comparison keeps the history, the current-depth
// comparison (what shipped) throws it away. Both halves are asserted, so the bug
// cannot be reintroduced without this test going red.
ZENITH_TEST(TAAResolve, DisocclusionKeepsStaticSurfaceUnderCameraDolly)
{
	const Zenith_Maths::Vector3   xWorld(0.0f, 0.0f, 2.0f);   // a surface that never moves
	const Zenith_Maths::Matrix4   xPrevViewProj = TAA_TestViewProj(0.0f);   // camera was at z = 0
	const float fHistoryPrevDepth = 2.0f;    // what the previous frame stored in the history alpha
	const float fCurViewZ         = 1.85f;   // the camera advanced 0.15 this frame

	const float fExpectedPrev = Flux_TAAExpectedPrevViewZ(xPrevViewProj, xWorld);
	ZENITH_ASSERT_EQ_FLOAT(fExpectedPrev, 2.0f, 1e-4f, "reprojection recovers the PREVIOUS depth");

	// Flat neighbourhood at the current depth, shifted back by the camera's own motion.
	ZENITH_ASSERT_FALSE(Flux_TAADisocclusionReject(
		fHistoryPrevDepth, fExpectedPrev, fCurViewZ, fCurViewZ, fCurViewZ, 0.05f),
		"a static surface under a camera dolly keeps its history");
	// Passing the CURRENT depth as the expected-previous depth is exactly what shipped:
	// the shift collapses to zero and the same pixel is rejected.
	ZENITH_ASSERT_TRUE(Flux_TAADisocclusionReject(
		fHistoryPrevDepth, fCurViewZ, fCurViewZ, fCurViewZ, fCurViewZ, 0.05f),
		"comparing against the CURRENT depth is what rejected it (the shipped bug)");
}

// ★ THE SECOND HALF OF THE SAME BUG. Even with the reprojection correct, a POINT
// comparison still rejects: the depth buffer is rasterised through the jitter, so a
// pixel on a sloped surface samples a different depth every frame. Measured, this was
// worth as much as the reprojection itself (2.97x -> 1.19x on the stability gate).
ZENITH_TEST(TAAResolve, DisocclusionKeepsJitteredDepthWithinNeighbourhoodRange)
{
	// Sloped surface: the 3x3 spans 9..11 and the previous jitter phase sampled 10.8.
	ZENITH_ASSERT_FALSE(Flux_TAADisocclusionReject(10.8f, 10.0f, 9.0f, 11.0f, 10.0f, 0.05f),
		"a jittered depth inside the neighbourhood range keeps its history");
	// The same sample against a POINT range (what a non-range test does) rejects.
	ZENITH_ASSERT_TRUE(Flux_TAADisocclusionReject(10.8f, 10.0f, 10.0f, 10.0f, 10.0f, 0.05f),
		"a point comparison rejects the very same pixel");
}

ZENITH_TEST(TAAResolve, DisocclusionKeepsSilhouettePixel)
{
	// An anti-aliased silhouette pixel legitimately alternates between the foreground
	// (3.0) and the background (40.0) as the jitter moves it. Its history IS a blend of
	// both surfaces -- rejecting it would destroy the edge TAA is there to smooth.
	ZENITH_ASSERT_FALSE(Flux_TAADisocclusionReject(40.0f, 3.0f, 3.0f, 40.0f, 3.0f, 0.05f),
		"a silhouette pixel keeps its history");
	// ... but a surface revealed from far behind the whole neighbourhood still rejects.
	ZENITH_ASSERT_TRUE(Flux_TAADisocclusionReject(200.0f, 3.5f, 3.0f, 4.0f, 3.5f, 0.05f),
		"a genuine disocclusion still rejects");
}

ZENITH_TEST(TAAResolve, DisocclusionRejectsBehindPreviousCamera)
{
	ZENITH_ASSERT_TRUE(Flux_TAADisocclusionReject(5.0f, -1.0f, 5.0f, 5.0f, 5.0f, 0.05f),
		"behind the prev camera");
	ZENITH_ASSERT_TRUE(Flux_TAADisocclusionReject(5.0f,  0.0f, 5.0f, 5.0f, 5.0f, 0.05f),
		"exactly on the prev eye plane");
}

// ---- blend alpha ramp + Karis ----------------------------------------------

ZENITH_TEST(TAAResolve, BlendAlphaRampEndpointsAndMonotonic)
{
	ZENITH_ASSERT_EQ_FLOAT(Flux_TAABlendAlpha(0.0f,  32.0f, 0.1f, 0.5f), 0.1f, 1e-6f, "stationary -> min alpha");
	ZENITH_ASSERT_EQ_FLOAT(Flux_TAABlendAlpha(32.0f, 32.0f, 0.1f, 0.5f), 0.5f, 1e-6f, "at threshold -> max alpha");
	ZENITH_ASSERT_EQ_FLOAT(Flux_TAABlendAlpha(64.0f, 32.0f, 0.1f, 0.5f), 0.5f, 1e-6f, "beyond threshold clamps to max");
	ZENITH_ASSERT_EQ_FLOAT(Flux_TAABlendAlpha(16.0f, 32.0f, 0.1f, 0.5f), 0.3f, 1e-6f, "half speed -> midpoint");
	// monotonic non-decreasing
	float fPrev = -1.0f;
	for (u_int u = 0; u <= 40u; ++u)
	{
		const float fA = Flux_TAABlendAlpha(static_cast<float>(u), 32.0f, 0.1f, 0.5f);
		ZENITH_ASSERT_GE(fA, fPrev, "alpha monotonic in speed at velPx=%u", u);
		fPrev = fA;
	}
}

ZENITH_TEST(TAAResolve, KarisWeightDecreasesWithLuma)
{
	ZENITH_ASSERT_EQ_FLOAT(Flux_TAAKarisWeight(0.0f), 1.0f,  1e-6f, "weight(0) = 1");
	ZENITH_ASSERT_EQ_FLOAT(Flux_TAAKarisWeight(1.0f), 0.5f,  1e-6f, "weight(1) = 1/2");
	ZENITH_ASSERT_EQ_FLOAT(Flux_TAAKarisWeight(3.0f), 0.25f, 1e-6f, "weight(3) = 1/4");
	ZENITH_ASSERT_LT(Flux_TAAKarisWeight(5.0f), Flux_TAAKarisWeight(1.0f), "brighter -> smaller weight");
}

ZENITH_TEST(TAAResolve, ResolveBlendEndpointsAndValidity)
{
	const Zenith_Maths::Vector3 xCur(0.8f, 0.2f, 0.1f);
	const Zenith_Maths::Vector3 xHist(0.1f, 0.6f, 0.9f);
	// invalid history -> current verbatim
	TAA_AssertVec3Near(Flux_TAAResolveBlend(xCur, xHist, 0.2f, false), xCur, 1e-6f, "invalid history -> current");
	// alpha 1 -> current only (history weight zero)
	TAA_AssertVec3Near(Flux_TAAResolveBlend(xCur, xHist, 1.0f, true), xCur, 1e-5f, "alpha=1 -> current");
	// alpha 0 -> history only
	TAA_AssertVec3Near(Flux_TAAResolveBlend(xCur, xHist, 0.0f, true), xHist, 1e-5f, "alpha=0 -> history");
}

// ---- sharpen ----------------------------------------------------------------

ZENITH_TEST(TAAResolve, SharpenAmountZeroIsIdentity)
{
	const Zenith_Maths::Vector3 xC(0.4f, 0.5f, 0.6f);
	const Zenith_Maths::Vector3 xU(0.1f), xD(0.9f), xL(0.2f), xR(0.7f);
	TAA_AssertVec3Near(Flux_TAASharpen(xC, xU, xD, xL, xR, 0.0f), xC, 1e-6f, "amount 0 -> centre");
}

ZENITH_TEST(TAAResolve, SharpenConstantNeighbourhoodUnchanged)
{
	const Zenith_Maths::Vector3 xC(0.5f, 0.5f, 0.5f);
	TAA_AssertVec3Near(Flux_TAASharpen(xC, xC, xC, xC, xC, 1.0f), xC, 1e-6f, "flat neighbourhood -> unchanged (kernel sums to 1)");
}

ZENITH_TEST(TAAResolve, SharpenCannotOvershootNeighbourhood)
{
	const Zenith_Maths::Vector3 xC(1.0f, 1.0f, 1.0f);      // centre is the local max
	const Zenith_Maths::Vector3 xN(0.5f, 0.5f, 0.5f);
	const Zenith_Maths::Vector3 xOut = Flux_TAASharpen(xC, xN, xN, xN, xN, 2.0f);
	// unclamped would be 1 + 2*(1-0.5) = 2.0; clamp to max tap (1.0) => no ring.
	TAA_AssertVec3Near(xOut, xC, 1e-6f, "sharpen clamped to neighbourhood max — no ringing");
}

// ---- upscale kernel + render-dim quantisation ------------------------------

ZENITH_TEST(TAAResolve, UpscaleWeightPeakAndDecay)
{
	ZENITH_ASSERT_EQ_FLOAT(Flux_TAAUpscaleWeight(0.0f, 0.47f), 1.0f, 1e-6f, "weight at distance 0 is 1");
	const float fW0 = Flux_TAAUpscaleWeight(0.0f, 0.47f);
	const float fW1 = Flux_TAAUpscaleWeight(0.5f, 0.47f);
	const float fW2 = Flux_TAAUpscaleWeight(1.0f, 0.47f);
	ZENITH_ASSERT_GT(fW0, fW1, "weight decreases with distance");
	ZENITH_ASSERT_GT(fW1, fW2, "weight decreases with distance");
	// normalised 3x3 support sums to 1, centre dominates.
	const float afDist[9] = { 1.41421356f, 1.0f, 1.41421356f, 1.0f, 0.0f, 1.0f, 1.41421356f, 1.0f, 1.41421356f };
	float fSum = 0.0f, fCentre = 0.0f;
	float afW[9];
	for (u_int u = 0; u < 9u; ++u) { afW[u] = Flux_TAAUpscaleWeight(afDist[u], 0.47f); fSum += afW[u]; }
	fCentre = afW[4] / fSum;
	float fNormSum = 0.0f;
	for (u_int u = 0; u < 9u; ++u) { fNormSum += afW[u] / fSum; }
	ZENITH_ASSERT_EQ_FLOAT(fNormSum, 1.0f, 1e-5f, "normalised kernel sums to 1");
	for (u_int u = 0; u < 9u; ++u)
	{
		if (u == 4u) continue;
		ZENITH_ASSERT_GT(fCentre, afW[u] / fSum, "centre weight dominates");
	}
}

ZENITH_TEST(TAAResolve, UpscaleReconstructUniformIsExact)
{
	// Uniform render source => the reconstructed colour equals it exactly (the Gaussian weights
	// normalise out). This pins the Σw normalisation the resolve shader's gather relies on.
	const Zenith_Maths::Vector3 xC(0.3f, 0.6f, 0.9f);
	auto fnUniform = [&](const Zenith_Maths::Vector2&) { return xC; };
	const Zenith_Maths::Vector3 xOut = Flux_TAAUpscaleReconstruct(
		Zenith_Maths::Vector2(0.371f, 0.629f), 960u, 540u, 0.47f, fnUniform);
	TAA_AssertVec3Near(xOut, xC, 1e-5f, "uniform render source reconstructs exactly");
}

ZENITH_TEST(TAAResolve, UpscaleReconstructCentresOnSampledUV)
{
	// Source encodes its own UV.x in red; the centre-weighted gather must reconstruct a red near
	// the output UV.x (within one render texel) — it does not drift off the sampled position.
	auto fnRampX = [](const Zenith_Maths::Vector2& xUV) { return Zenith_Maths::Vector3(xUV.x, 0.0f, 0.0f); };
	const float fUVx = 0.5f;
	const Zenith_Maths::Vector3 xOut = Flux_TAAUpscaleReconstruct(
		Zenith_Maths::Vector2(fUVx, 0.5f), 960u, 540u, 0.47f, fnRampX);
	ZENITH_ASSERT_LT(fabsf(xOut.x - fUVx), 2.0f / 960.0f, "reconstruct stays within a render texel of the sampled UV");
}

ZENITH_TEST(TAAResolve, ComputeRenderDimsIdentityAtScaleOne)
{
	Zenith_Maths::UVector2 xEven = Flux_TAAComputeRenderDims(1920u, 1080u, 1.0f);
	ZENITH_ASSERT_EQ(xEven.x, 1920u, "scale 1 identity width");
	ZENITH_ASSERT_EQ(xEven.y, 1080u, "scale 1 identity height");
	// identity path preserves ODD output dims exactly (no even-rounding).
	Zenith_Maths::UVector2 xOdd = Flux_TAAComputeRenderDims(1921u, 1081u, 1.0f);
	ZENITH_ASSERT_EQ(xOdd.x, 1921u, "scale 1 preserves odd width");
	ZENITH_ASSERT_EQ(xOdd.y, 1081u, "scale 1 preserves odd height");
}

ZENITH_TEST(TAAResolve, ComputeRenderDimsQuantisationAndClamp)
{
	Zenith_Maths::UVector2 xHalf = Flux_TAAComputeRenderDims(1920u, 1080u, 0.5f);
	ZENITH_ASSERT_EQ(xHalf.x, 960u, "0.5 scale width");
	ZENITH_ASSERT_EQ(xHalf.y, 540u, "0.5 scale height");

	Zenith_Maths::UVector2 xQuality = Flux_TAAComputeRenderDims(1920u, 1080u, 0.75f);
	ZENITH_ASSERT_EQ(xQuality.x % 2u, 0u, "0.75 width is even");
	ZENITH_ASSERT_EQ(xQuality.y % 2u, 0u, "0.75 height is even");
	ZENITH_ASSERT_LE(xQuality.x, 1920u, "render width never exceeds output");
	ZENITH_ASSERT_LE(xQuality.y, 1080u, "render height never exceeds output");

	// below the 0.5 floor clamps up to 0.5.
	Zenith_Maths::UVector2 xClamp = Flux_TAAComputeRenderDims(1920u, 1080u, 0.2f);
	ZENITH_ASSERT_EQ(xClamp.x, 960u, "sub-0.5 scale clamps to 0.5 width");
	ZENITH_ASSERT_EQ(xClamp.y, 540u, "sub-0.5 scale clamps to 0.5 height");
}
