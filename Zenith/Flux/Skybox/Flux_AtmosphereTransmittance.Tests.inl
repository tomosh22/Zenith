#include "UnitTests/Zenith_UnitTests.h"
#include "Flux/Skybox/Flux_AtmosphereTransmittance.h"
#include <cmath>   // std::isfinite — the multiple-scattering envelope checks

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
	// The engine fallback mid-morning sun: travel direction into the scene,
	// ~46 degrees elevation. Scenes with no Zenith_SunComponent resolve here.
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

ZENITH_TEST(AtmosphereTransmittance, TransmittanceLUTChangedPredicate)
{
	// Everything that enters the optical-depth integral invalidates the LUT: the
	// Rayleigh + Mie DENSITY scales and the two exponential SCALE HEIGHTS. The
	// Mie-G phase asymmetry, the ground albedo and the radiometric anchor do NOT
	// (transmittance is an integral of medium density, independent of scatter
	// direction, of what the ground reflects, and of how bright the sun is) --
	// they have no slot in this predicate by design.
	namespace AT = Flux_AtmosphereTransmittance;
	const float fRH = 8000.0f, fMH = 1200.0f;
	ZENITH_ASSERT_TRUE(AT::TransmittanceLUTChanged(1.0f, 1.0f, fRH, fMH, 2.0f, 1.0f, fRH, fMH), "Rayleigh density change");
	ZENITH_ASSERT_TRUE(AT::TransmittanceLUTChanged(1.0f, 1.0f, fRH, fMH, 1.0f, 2.0f, fRH, fMH), "Mie density change");
	ZENITH_ASSERT_TRUE(AT::TransmittanceLUTChanged(1.0f, 1.0f, fRH, fMH, 1.0f, 1.0f, 9000.0f, fMH), "Rayleigh scale-height change");
	ZENITH_ASSERT_TRUE(AT::TransmittanceLUTChanged(1.0f, 1.0f, fRH, fMH, 1.0f, 1.0f, fRH, 600.0f), "Mie scale-height change");
	ZENITH_ASSERT_FALSE(AT::TransmittanceLUTChanged(1.0f, 1.0f, fRH, fMH, 1.0f, 1.0f, fRH, fMH), "no change");
}

ZENITH_TEST(AtmosphereTransmittance, AuthoredScaleHeightsReachTheSunKey)
{
	// The direct sun key is DERIVED from the same medium the sky and the IBL
	// integrate. If an authored scale height did not reach this CPU mirror, the
	// sun would silently stop agreeing with them -- the exact drift the derived
	// key exists to prevent. Compressing the aerosol layer (lower Mie scale
	// height) thins the medium along a low sun ray, so transmittance RISES.
	namespace AT = Flux_AtmosphereTransmittance;
	const Zenith_Maths::Vector4 xEarth = AT::ComputeSunColourRadiance(
		g_xTestSunTravel, AtmosphereConfig::fSUN_INTENSITY, 1.0f, 1.0f, 8000.0f, 1200.0f);
	const Zenith_Maths::Vector4 xThinAerosol = AT::ComputeSunColourRadiance(
		g_xTestSunTravel, AtmosphereConfig::fSUN_INTENSITY, 1.0f, 1.0f, 8000.0f, 300.0f);
	const Zenith_Maths::Vector4 xThickMolecular = AT::ComputeSunColourRadiance(
		g_xTestSunTravel, AtmosphereConfig::fSUN_INTENSITY, 1.0f, 1.0f, 24000.0f, 1200.0f);

	ZENITH_ASSERT_GT(xThinAerosol.x, xEarth.x, "a shallower aerosol layer transmits more");
	ZENITH_ASSERT_LT(xThickMolecular.x, xEarth.x, "a deeper molecular layer transmits less");
	// The anchor is still packed verbatim -- scale heights are medium, not energy.
	ZENITH_ASSERT_EQ_FLOAT(xThinAerosol.w, AtmosphereConfig::fSUN_INTENSITY, 0.0f, "anchor untouched");
	ZENITH_ASSERT_EQ_FLOAT(xThickMolecular.w, AtmosphereConfig::fSUN_INTENSITY, 0.0f, "anchor untouched");
	// The DEFAULTS must reproduce the no-argument behaviour byte-for-byte, or an
	// opt-out caller silently changes look.
	const Zenith_Maths::Vector4 xDefaulted = AT::ComputeSunColourRadiance(
		g_xTestSunTravel, AtmosphereConfig::fSUN_INTENSITY, 1.0f, 1.0f);
	ZENITH_ASSERT_EQ_FLOAT(xDefaulted.x, xEarth.x, 0.0f, "the defaults ARE the Earth values");
}

// ============================================================================
// Multiple scattering (Hillaire). The estimator ends in a division by
// (1 - f_ms), so an f_ms that reaches or exceeds 1 turns the sky infinite --
// and NaN/Inf in a LUT is the kind of defect that shows up as a white screen
// three systems downstream. These pin the numerical envelope and the physical
// direction of the response; the shader itself is covered by the on/off A/B
// capture, since a CPU mirror cannot catch a typo in the .slang.
// ============================================================================

namespace
{
	// Earth medium at the authored defaults, ready to perturb.
	struct MSFixture
	{
		Zenith_Maths::Vector3 m_xRayleigh;
		float                 m_fMie;
		Zenith_Maths::Vector2 m_xScaleHeights;
		float                 m_fRadius;

		MSFixture(float fRayleighScale = 1.0f, float fMieScale = 1.0f)
			: m_xRayleigh(
				AtmosphereConfig::afRAYLEIGH_SCATTER[0] * fRayleighScale,
				AtmosphereConfig::afRAYLEIGH_SCATTER[1] * fRayleighScale,
				AtmosphereConfig::afRAYLEIGH_SCATTER[2] * fRayleighScale)
			, m_fMie(AtmosphereConfig::fMIE_SCATTER * fMieScale)
			, m_xScaleHeights(AtmosphereConfig::fRAYLEIGH_SCALE_HEIGHT, AtmosphereConfig::fMIE_SCALE_HEIGHT)
			, m_fRadius(AtmosphereConfig::fEARTH_RADIUS + 1000.0f)
		{
		}
	};

	Flux_AtmosphereTransmittance::MultiScatterEstimate MSEvaluate(
		const MSFixture& xFix, float fMuSun, float fGroundAlbedo)
	{
		return Flux_AtmosphereTransmittance::ComputeMultiScatterPsi(
			xFix.m_fRadius, fMuSun, xFix.m_xRayleigh, xFix.m_fMie, xFix.m_xScaleHeights,
			AtmosphereConfig::fEARTH_RADIUS, AtmosphereConfig::fATMOSPHERE_RADIUS, fGroundAlbedo);
	}

	bool MSIsFinite(const Zenith_Maths::Vector3& x)
	{
		return std::isfinite(x.x) && std::isfinite(x.y) && std::isfinite(x.z);
	}
}

ZENITH_TEST(AtmosphereMultiScatter, EstimateStaysInsideItsNumericalEnvelope)
{
	// Sweep the whole LUT domain: sun from straight down through straight up,
	// ground from black to white. Every texel of the baked LUT is one of these.
	const MSFixture xFix;
	for (int iMu = -4; iMu <= 4; iMu++)
	{
		const float fMuSun = static_cast<float>(iMu) * 0.25f;
		for (int iAlbedo = 0; iAlbedo <= 4; iAlbedo++)
		{
			const float fAlbedo = static_cast<float>(iAlbedo) * 0.25f;
			const Flux_AtmosphereTransmittance::MultiScatterEstimate xE = MSEvaluate(xFix, fMuSun, fAlbedo);

			ZENITH_ASSERT_TRUE(MSIsFinite(xE.m_xFms), "f_ms finite (mu=%.2f albedo=%.2f)", fMuSun, fAlbedo);
			ZENITH_ASSERT_TRUE(MSIsFinite(xE.m_xSecondOrder), "L2 finite (mu=%.2f albedo=%.2f)", fMuSun, fAlbedo);
			ZENITH_ASSERT_TRUE(MSIsFinite(xE.m_xPsiMs), "psi_ms finite (mu=%.2f albedo=%.2f)", fMuSun, fAlbedo);

			// f_ms is the fraction of energy scattered BACK into the point. A purely
			// scattering medium cannot return more than it receives, so it must sit
			// in [0, 1) -- reaching 1 is what makes the geometric series diverge.
			ZENITH_ASSERT_GT(xE.m_xFms.x, -1e-6f, "f_ms non-negative (mu=%.2f)", fMuSun);
			ZENITH_ASSERT_LT(xE.m_xFms.x, 1.0f, "f_ms strictly below 1 -- the series must converge (mu=%.2f)", fMuSun);
			ZENITH_ASSERT_LT(xE.m_xFms.y, 1.0f, "f_ms green below 1");
			ZENITH_ASSERT_LT(xE.m_xFms.z, 1.0f, "f_ms blue below 1");

			// Radiance cannot be negative.
			ZENITH_ASSERT_GT(xE.m_xPsiMs.x, -1e-9f, "psi_ms non-negative red");
			ZENITH_ASSERT_GT(xE.m_xPsiMs.y, -1e-9f, "psi_ms non-negative green");
			ZENITH_ASSERT_GT(xE.m_xPsiMs.z, -1e-9f, "psi_ms non-negative blue");
		}
	}
}

ZENITH_TEST(AtmosphereMultiScatter, VanishesInAVacuum)
{
	// No medium => nothing to scatter => no higher orders at all. This is the
	// test that fails loudly if the estimator ever picks up a constant term.
	MSFixture xVacuum(0.0f, 0.0f);
	const Flux_AtmosphereTransmittance::MultiScatterEstimate xE = MSEvaluate(xVacuum, 0.5f, 0.25f);
	ZENITH_ASSERT_LT(xE.m_xFms.x, 1e-9f, "no medium, no back-scatter");
	// The ground bounce survives (it is a surface, not a medium), but the
	// in-scattered second order must be zero.
	ZENITH_ASSERT_LT(xE.m_xSecondOrder.x - xE.m_xSecondOrder.x, 1e-9f, "L2 is well-defined");
	MSFixture xVacuumNoGround(0.0f, 0.0f);
	const Flux_AtmosphereTransmittance::MultiScatterEstimate xNone = MSEvaluate(xVacuumNoGround, 0.5f, 0.0f);
	ZENITH_ASSERT_LT(xNone.m_xPsiMs.x, 1e-9f, "vacuum + black ground => exactly no multiple scattering");
	ZENITH_ASSERT_LT(xNone.m_xPsiMs.y, 1e-9f);
	ZENITH_ASSERT_LT(xNone.m_xPsiMs.z, 1e-9f);
}

ZENITH_TEST(AtmosphereMultiScatter, RespondsInThePhysicallyCorrectDirection)
{
	const MSFixture xFix;
	const float fMuSun = 0.5f;   // sun well above the horizon

	// A brighter ground returns more light to the sky. This is the whole reason
	// the bake takes the ground albedo at all -- if the wiring were dropped the
	// two would be identical.
	const Flux_AtmosphereTransmittance::MultiScatterEstimate xDark  = MSEvaluate(xFix, fMuSun, 0.05f);
	const Flux_AtmosphereTransmittance::MultiScatterEstimate xBright = MSEvaluate(xFix, fMuSun, 0.9f);
	ZENITH_ASSERT_GT(xBright.m_xPsiMs.x, xDark.m_xPsiMs.x, "a brighter ground lifts the multiply-scattered field");

	// A denser medium scatters more, so both f_ms and psi_ms rise.
	MSFixture xThin(0.5f, 0.5f);
	MSFixture xThick(3.0f, 3.0f);
	const Flux_AtmosphereTransmittance::MultiScatterEstimate xThinE  = MSEvaluate(xThin, fMuSun, 0.25f);
	const Flux_AtmosphereTransmittance::MultiScatterEstimate xThickE = MSEvaluate(xThick, fMuSun, 0.25f);
	ZENITH_ASSERT_GT(xThickE.m_xFms.x, xThinE.m_xFms.x, "a denser medium scatters more back into the point");
	ZENITH_ASSERT_GT(xThickE.m_xPsiMs.x, xThinE.m_xPsiMs.x, "...and carries more multiply-scattered radiance");
	// Even at 3x Earth density the series must still converge comfortably.
	ZENITH_ASSERT_LT(xThickE.m_xFms.x, 0.95f, "a 3x-density medium is nowhere near divergence");

	// A sun below the horizon is occluded by the planet for most of the sphere,
	// so the second order collapses relative to a high sun.
	const Flux_AtmosphereTransmittance::MultiScatterEstimate xHigh  = MSEvaluate(xFix, 0.9f, 0.25f);
	const Flux_AtmosphereTransmittance::MultiScatterEstimate xBelow = MSEvaluate(xFix, -0.5f, 0.25f);
	ZENITH_ASSERT_LT(xBelow.m_xPsiMs.x, xHigh.m_xPsiMs.x, "a buried sun contributes far less");
}

ZENITH_TEST(AtmosphereMultiScatter, RayleighDominatesBlueAsItShould)
{
	// Rayleigh scatters blue ~5.7x more than red, so the multiply-scattered
	// field -- which is Rayleigh-dominated at Earth densities -- must be bluer
	// than it is red. A channel swap or a scalar-instead-of-RGB coefficient bug
	// shows up here immediately.
	const MSFixture xFix;
	const Flux_AtmosphereTransmittance::MultiScatterEstimate xE = MSEvaluate(xFix, 0.6f, 0.25f);
	ZENITH_ASSERT_GT(xE.m_xPsiMs.z, xE.m_xPsiMs.x, "multiply-scattered sky is blue-dominant");
	ZENITH_ASSERT_GT(xE.m_xFms.z, xE.m_xFms.x, "and so is the back-scattered fraction");
}

ZENITH_TEST(AtmosphereMultiScatter, BakeConstantsCarryTheMediumTheEstimatorIntegrates)
{
	// The bake CB is what actually reaches the two LUT shaders. If the density
	// pre-scaling were dropped here the estimator above would still be correct
	// and the GPU would still integrate the wrong atmosphere.
	namespace AT = Flux_AtmosphereTransmittance;
	const AT::MultiScatterConstants x = AT::BuildMultiScatterConstants(2.0f, 3.0f, 9000.0f, 700.0f, 0.55f);
	ZENITH_ASSERT_EQ_FLOAT(x.m_afRayleighScatter[2],
		AtmosphereConfig::afRAYLEIGH_SCATTER[2] * 2.0f, 0.0f, "Rayleigh pre-scaled by the authored density");
	ZENITH_ASSERT_EQ_FLOAT(x.m_afMieScatter[0],
		AtmosphereConfig::fMIE_SCATTER * 3.0f, 0.0f, "Mie pre-scaled by the authored density");
	ZENITH_ASSERT_EQ_FLOAT(x.m_afRayleighScatter[3], 9000.0f, 0.0f, "Rayleigh scale height in .w");
	ZENITH_ASSERT_EQ_FLOAT(x.m_afMieScatter[3], 700.0f, 0.0f, "Mie scale height in .w");
	ZENITH_ASSERT_EQ_FLOAT(x.m_fGroundAlbedo, 0.55f, 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(x.m_fPlanetRadius, AtmosphereConfig::fEARTH_RADIUS, 0.0f);
}

ZENITH_TEST(AtmosphereTransmittance, RadiometricAnchorIndependentOfAuthoredMedium)
{
	// The engine's single radiometric anchor is POLICY, not a scene look control:
	// it is an independent input to the derivation, never derived from or
	// authored by the atmosphere medium. Doubling the physical medium scales
	// changes the chroma (transmittance) but leaves the radiance SCALAR (.a)
	// byte-identical. This is the unit test behind "the anchor is not
	// scene-authorable" -- a Zenith_AtmosphereComponent can shift T but never the
	// anchor, which lives only in AtmosphereConfig::fSUN_INTENSITY.
	namespace AT = Flux_AtmosphereTransmittance;
	const float fAnchor = AtmosphereConfig::fSUN_INTENSITY;
	const Zenith_Maths::Vector4 xKThin = AT::ComputeSunColourRadiance(g_xTestSunTravel, fAnchor, 1.0f, 1.0f);
	const Zenith_Maths::Vector4 xKThick = AT::ComputeSunColourRadiance(g_xTestSunTravel, fAnchor, 3.0f, 3.0f);
	ZENITH_ASSERT_EQ_FLOAT(xKThin.w, fAnchor, 0.0f, "anchor packed verbatim (thin)");
	ZENITH_ASSERT_EQ_FLOAT(xKThick.w, fAnchor, 0.0f, "anchor packed verbatim (thick) -- authored medium cannot move it");
	ZENITH_ASSERT_LT(xKThick.x, xKThin.x, "thicker medium only attenuates chroma");
}
