#include "UnitTests/Zenith_UnitTests.h"
#include "Flux/HDR/Flux_HDRImpl.h"
#include "Flux/Skybox/Flux_SkyboxImpl.h"

// ============================================================================
// Exposure-derivation unit tests — the lock on the two DERIVED (never tuned)
// auto-exposure constants. Re-tuning either by eye means editing a test that
// cites the standard the number comes from.
//
// Hosted by Flux_HDR.cpp (always-linked TU) per the dead-strip idiom.
// ============================================================================

ZENITH_TEST(HDRExposure, KeyIsTheISOSaturationTarget)
{
	// ISO 2720 reflected-light meter: metered average -> K / ISO of
	// saturation (K = 12.5, ISO 100), with Frostbite's 1.2 highlight
	// headroom. Spelled out so a drive-by re-tune cannot pass silently.
	ZENITH_ASSERT_EQ_FLOAT(fHDR_EXPOSURE_KEY_ISO, 12.5f / 120.0f, 1e-7f,
		"exposure key must be the ISO-derived saturation target, not a tuned value");
}

ZENITH_TEST(HDRExposure, HistogramTopBinCoversTheRadiometricAnchor)
{
	// The brightest non-emissive radiance in the frame is bounded by the sky,
	// which is bounded by the top-of-atmosphere sun intensity (the engine's
	// radiometric anchor). The histogram's top bin must reach it, else the sky
	// clips into the last bin and skews the metered average — which is exactly
	// what the old range-12 domain did (top 4.0 vs sky ~7).
	const float fTopBinLuminance = exp2f(
		fHDR_HISTOGRAM_MIN_LOG_LUMINANCE + fHDR_HISTOGRAM_LOG_LUMINANCE_RANGE);
	ZENITH_ASSERT_GE(fTopBinLuminance, AtmosphereConfig::fSUN_INTENSITY,
		"histogram top bin must cover the anchor (sky radiance bound)");
}
