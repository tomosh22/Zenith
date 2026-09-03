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

ZENITH_TEST(HDRExposure, HistogramTopBinCoversTheSunDiscRadiance)
{
	// The brightest thing in the frame is the SUN DISC, not the sky. Its radiance
	// is the anchor's irradiance divided by the solar solid angle (see
	// Common/Atmosphere.slang RenderSunDisk), which is ~3600x the sky it sits in.
	// The histogram's top bin must reach it: when it did not, the disc, the sky
	// and every sunlit white surface shared one saturated bin, so no percentile
	// above ~0.9 could distinguish them and the highlight protection in
	// Flux_Adaptation could not see the surface it exists to protect.
	const float fSunSolidAngle = 3.14159265f
		* AtmosphereConfig::fSUN_ANGULAR_RADIUS * AtmosphereConfig::fSUN_ANGULAR_RADIUS;
	const float fSunDiscRadiance = AtmosphereConfig::fSUN_INTENSITY / fSunSolidAngle;
	const float fTopBinLuminance = exp2f(
		fHDR_HISTOGRAM_MIN_LOG_LUMINANCE + fHDR_HISTOGRAM_LOG_LUMINANCE_RANGE);
	ZENITH_ASSERT_GE(fTopBinLuminance, fSunDiscRadiance,
		"histogram top bin must cover the sun disc radiance, not just the sky");
	// ...and the sky, which is what the average metering actually reads.
	ZENITH_ASSERT_GE(fTopBinLuminance, AtmosphereConfig::fSUN_INTENSITY,
		"histogram top bin must cover the anchor (sky radiance bound)");
}
