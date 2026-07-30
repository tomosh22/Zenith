#include "UnitTests/Zenith_UnitTests.h"
#include "Flux/Skybox/Flux_AtmosphereTransmittance.h"

// ============================================================================
// Flux_AtmosphereTransmittance unit tests — the lock on the "one radiometric
// anchor" contract. The direct sun key is DERIVED (anchor * per-channel
// atmospheric transmittance, same medium as the sky/IBL); these tests pin the
// physics that derivation encodes, so replacing it with a hand-tuned colour
// again means confronting a red test that cites the radiometry.
//
// Hosted by Flux_Skybox.cpp (always-linked TU) so MSVC static-init
// dead-stripping cannot silently drop the registrations.
// ============================================================================

namespace
{
	// The shipped mid-morning sun (dbg_SunDir): travel direction into the
	// scene, ~46 degrees elevation.
	const Zenith_Maths::Vector3 g_xTestSunTravel(-0.4f, -0.7f, -0.55f);
}

ZENITH_TEST(AtmosphereTransmittance, ZenithMatchesFlatAtmosphereClosedForm)
{
	// At mu = 1 (sun at zenith) the ray is radial, so the spherical optical
	// depth reduces EXACTLY to the flat-atmosphere closed form
	//   OD = H_scale * (exp(-h0/H) - exp(-hTop/H))
	// per species; the only remaining difference is midpoint-quadrature error.
	namespace AT = Flux_AtmosphereTransmittance;
	const Zenith_Maths::Vector3 xT = AT::ComputeSunTransmittance(
		Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f), 1.0f, 1.0f);

	const float fTopHeight = AtmosphereConfig::fATMOSPHERE_HEIGHT;
	const float fODRayleigh = AtmosphereConfig::fRAYLEIGH_SCALE_HEIGHT *
		(expf(-AT::fREFERENCE_HEIGHT_METERS / AtmosphereConfig::fRAYLEIGH_SCALE_HEIGHT)
		 - expf(-fTopHeight / AtmosphereConfig::fRAYLEIGH_SCALE_HEIGHT));
	const float fODMie = AtmosphereConfig::fMIE_SCALE_HEIGHT *
		(expf(-AT::fREFERENCE_HEIGHT_METERS / AtmosphereConfig::fMIE_SCALE_HEIGHT)
		 - expf(-fTopHeight / AtmosphereConfig::fMIE_SCALE_HEIGHT));

	const float afRayleigh[3] = {
		AtmosphereConfig::afRAYLEIGH_SCATTER[0],
		AtmosphereConfig::afRAYLEIGH_SCATTER[1],
		AtmosphereConfig::afRAYLEIGH_SCATTER[2] };
	for (int i = 0; i < 3; i++)
	{
		const float fExpected = expf(
			-afRayleigh[i] * fODRayleigh
			- AtmosphereConfig::fMIE_SCATTER * fODMie);
		// 8-sample midpoint quadrature (DEFAULT_LIGHT_SAMPLES, mirroring the
		// shader exactly) vs the closed form: agree to ~1% on the blue channel
		// (the coarse Mie/Rayleigh tail sampling costs a fraction of a percent
		// of transmittance). 2% headroom still reds any real units error —
		// a swapped coefficient, wrong scale height or dropped channel is >10%.
		ZENITH_ASSERT_EQ_FLOAT(xT[i], fExpected, 0.02f * fExpected,
			"zenith transmittance channel %d vs closed form", i);
	}
}

ZENITH_TEST(AtmosphereTransmittance, RayleighOrdersChannelsRedThroughBlue)
{
	// Rayleigh removes short wavelengths first, so along any above-horizon sun
	// ray T.r > T.g > T.b strictly. This is WHY the derived sun key is warm —
	// swap or flatten the coefficients and this reds.
	namespace AT = Flux_AtmosphereTransmittance;
	const Zenith_Maths::Vector3 xT = AT::ComputeSunTransmittance(g_xTestSunTravel, 1.0f, 1.0f);
	ZENITH_ASSERT_GT(xT.x, xT.y, "red must transmit more than green");
	ZENITH_ASSERT_GT(xT.y, xT.z, "green must transmit more than blue");
}

ZENITH_TEST(AtmosphereTransmittance, LowerSunIsDimmerAndWarmerEveryStep)
{
	// Transmittance must fall monotonically in every channel as the sun drops,
	// and the blue/red ratio with it: golden hour emerges from the medium, not
	// from an authored tint.
	namespace AT = Flux_AtmosphereTransmittance;
	float fPrevSum = 4.0f;
	float fPrevBlueOverRed = 2.0f;
	for (int i = 1; i <= 8; i++)
	{
		const float fElevRad = (90.0f - 10.0f * static_cast<float>(i)) * 3.14159265f / 180.0f;
		const Zenith_Maths::Vector3 xTravel(-cosf(fElevRad), -sinf(fElevRad), 0.0f);
		const Zenith_Maths::Vector3 xT = AT::ComputeSunTransmittance(xTravel, 1.0f, 1.0f);
		const float fSum = xT.x + xT.y + xT.z;
		const float fBlueOverRed = xT.z / glm::max(xT.x, 1e-6f);
		ZENITH_ASSERT_LT(fSum, fPrevSum, "transmittance not monotone at elevation step %d", i);
		ZENITH_ASSERT_LT(fBlueOverRed, fPrevBlueOverRed, "warmth not monotone at elevation step %d", i);
		fPrevSum = fSum;
		fPrevBlueOverRed = fBlueOverRed;
	}
}

ZENITH_TEST(AtmosphereTransmittance, BelowHorizonSunContributesNothing)
{
	// A sun travelling upward (sun below the horizon) is planet-occluded: zero
	// in all channels, so night needs no special case anywhere downstream.
	namespace AT = Flux_AtmosphereTransmittance;
	const Zenith_Maths::Vector3 xT = AT::ComputeSunTransmittance(
		Zenith_Maths::Vector3(0.2f, 0.9f, 0.1f), 1.0f, 1.0f);
	ZENITH_ASSERT_EQ_FLOAT(xT.x, 0.0f, 0.0f, "below-horizon red");
	ZENITH_ASSERT_EQ_FLOAT(xT.y, 0.0f, 0.0f, "below-horizon green");
	ZENITH_ASSERT_EQ_FLOAT(xT.z, 0.0f, 0.0f, "below-horizon blue");
}

ZENITH_TEST(AtmosphereTransmittance, TransmittanceStaysInUnitInterval)
{
	namespace AT = Flux_AtmosphereTransmittance;
	for (int i = 0; i <= 18; i++)
	{
		const float fElevRad = (static_cast<float>(i) * 10.0f - 90.0f) * 3.14159265f / 180.0f;
		const Zenith_Maths::Vector3 xTravel(-cosf(fElevRad), -sinf(fElevRad), 0.0f);
		const Zenith_Maths::Vector3 xT = AT::ComputeSunTransmittance(xTravel, 1.0f, 1.0f);
		for (int c = 0; c < 3; c++)
		{
			ZENITH_ASSERT_GE(xT[c], 0.0f, "channel %d below zero at step %d", c, i);
			ZENITH_ASSERT_LE(xT[c], 1.0f, "channel %d above one at step %d", c, i);
		}
	}
}

ZENITH_TEST(AtmosphereTransmittance, SunKeyIsAnchorTimesTransmittanceExactly)
{
	// The packed key must be radiance = anchor * T with NOTHING else in the
	// chain: chromaticity holds T verbatim, .a holds the anchor verbatim,
	// doubling the anchor leaves the chromaticity bit-identical. Any hidden
	// re-introduced multiplier reds this.
	namespace AT = Flux_AtmosphereTransmittance;
	const Zenith_Maths::Vector3 xT = AT::ComputeSunTransmittance(g_xTestSunTravel, 1.0f, 1.0f);
	const Zenith_Maths::Vector4 xKey1 = AT::ComputeSunColourRadiance(g_xTestSunTravel, 7.0f, 1.0f, 1.0f);
	const Zenith_Maths::Vector4 xKey2 = AT::ComputeSunColourRadiance(g_xTestSunTravel, 14.0f, 1.0f, 1.0f);
	ZENITH_ASSERT_EQ_FLOAT(xKey1.x, xT.x, 0.0f, "chromaticity.r is transmittance verbatim");
	ZENITH_ASSERT_EQ_FLOAT(xKey1.y, xT.y, 0.0f, "chromaticity.g is transmittance verbatim");
	ZENITH_ASSERT_EQ_FLOAT(xKey1.z, xT.z, 0.0f, "chromaticity.b is transmittance verbatim");
	ZENITH_ASSERT_EQ_FLOAT(xKey1.w, 7.0f, 0.0f, "radiance scalar is the anchor verbatim");
	ZENITH_ASSERT_EQ_FLOAT(xKey2.w, 14.0f, 0.0f, "radiance scalar scales with the anchor");
	ZENITH_ASSERT_EQ_FLOAT(xKey2.x, xKey1.x, 0.0f, "chromaticity independent of the anchor");
	// And the transmittance is a real attenuation, not a pass-through.
	ZENITH_ASSERT_GT(xT.x, 0.0f, "above-horizon sun transmits");
	ZENITH_ASSERT_LT(xT.x, 1.0f, "the atmosphere attenuates");
}

ZENITH_TEST(AtmosphereTransmittance, ScalesThickenTheMedium)
{
	// The runtime Rayleigh/Mie debug scales must reach the derived key: a
	// thicker medium transmits less in every channel.
	namespace AT = Flux_AtmosphereTransmittance;
	const Zenith_Maths::Vector3 xT1 = AT::ComputeSunTransmittance(g_xTestSunTravel, 1.0f, 1.0f);
	const Zenith_Maths::Vector3 xT2 = AT::ComputeSunTransmittance(g_xTestSunTravel, 2.0f, 2.0f);
	ZENITH_ASSERT_LT(xT2.x, xT1.x, "thicker medium: red");
	ZENITH_ASSERT_LT(xT2.y, xT1.y, "thicker medium: green");
	ZENITH_ASSERT_LT(xT2.z, xT1.z, "thicker medium: blue");
}
