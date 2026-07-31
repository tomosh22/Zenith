#pragma once

#include "Maths/Zenith_Maths.h"
#include "Flux/Skybox/Flux_SkyboxImpl.h"

// =====================================================================
// Flux_AtmosphereTransmittance — CPU mirror of the atmospheric
// transmittance integral in Shaders/Common/Atmosphere.slang.
//
// THE RADIOMETRIC CONTRACT (the reason this file exists): the engine
// has ONE light-energy anchor — AtmosphereConfig::fSUN_INTENSITY, the
// top-of-atmosphere solar irradiance in engine linear units. The sky
// the player sees, the IBL irradiance/prefilter cubes, AND the direct
// sun key are all derived from that single number through the SAME
// Rayleigh+Mie medium. The direct sun's radiance at the ground is
//
//     E_sun(ground) = fSUN_INTENSITY * T_rgb(sunDir)
//
// where T_rgb is the per-channel transmittance along the sun ray,
// computed here with the same coefficients, scale heights and midpoint
// quadrature as the shaders. There is no hand-authored sun colour: the
// warm tint at low sun IS the transmittance (Rayleigh removes blue
// first), and sun below the horizon goes dark because T -> 0.
//
// Pure functions, no engine-state reads — unit-tested in
// Flux_AtmosphereTransmittance.Tests.inl (hosted by Flux_Skybox.cpp).
// =====================================================================

namespace Flux_AtmosphereTransmittance
{
	// Sampled at the same reference height the IBL convolution shaders use
	// (Common.PBR.IBL_REFERENCE_HEIGHT_METERS) so the direct sun and the
	// convolved sky agree on the medium between them and space.
	constexpr float fREFERENCE_HEIGHT_METERS = 100.0f;

	// Mirror of Atmosphere.slang GetDensity: (rayleigh, mie) density at height.
	inline Zenith_Maths::Vector2 GetDensity(float fHeight, const Zenith_Maths::Vector2& xScaleHeights)
	{
		return Zenith_Maths::Vector2(
			expf(-fHeight / xScaleHeights.x),
			expf(-fHeight / xScaleHeights.y));
	}

	// Mirror of Atmosphere.slang RaySphereIntersect: both roots, (-1,-1) on miss.
	inline Zenith_Maths::Vector2 RaySphereIntersect(const Zenith_Maths::Vector3& xRayOrigin, const Zenith_Maths::Vector3& xRayDir, float fSphereRadius)
	{
		const float fB = 2.0f * glm::dot(xRayOrigin, xRayDir);
		const float fC = glm::dot(xRayOrigin, xRayOrigin) - fSphereRadius * fSphereRadius;
		const float fDiscriminant = fB * fB - 4.0f * fC;
		if (fDiscriminant < 0.0f)
		{
			return Zenith_Maths::Vector2(-1.0f, -1.0f);
		}
		const float fSqrtDisc = sqrtf(fDiscriminant);
		return Zenith_Maths::Vector2((-fB - fSqrtDisc) * 0.5f, (-fB + fSqrtDisc) * 0.5f);
	}

	// Mirror of Atmosphere.slang ComputeOpticalDepth: midpoint quadrature of
	// (rayleigh, mie) density along a finite ray segment.
	inline Zenith_Maths::Vector2 ComputeOpticalDepth(
		const Zenith_Maths::Vector3& xOrigin,
		const Zenith_Maths::Vector3& xDir,
		float fDistance,
		const Zenith_Maths::Vector2& xScaleHeights,
		float fPlanetRadius,
		u_int uSamples)
	{
		const float fStepSize = fDistance / static_cast<float>(uSamples);
		Zenith_Maths::Vector2 xOpticalDepth(0.0f, 0.0f);
		for (u_int u = 0; u < uSamples; u++)
		{
			const float fT = (static_cast<float>(u) + 0.5f) * fStepSize;
			const Zenith_Maths::Vector3 xSamplePos = xOrigin + xDir * fT;
			const float fHeight = glm::length(xSamplePos) - fPlanetRadius;
			xOpticalDepth += GetDensity(fHeight, xScaleHeights) * fStepSize;
		}
		return xOpticalDepth;
	}

	// Mirror of Atmosphere.slang TransmittanceToAtmosphereTop: per-channel
	// transmittance from a point at radius fRadius along a ray whose cosine to
	// the local zenith is fMu, out to the top of the atmosphere. Returns zero
	// when the ray is blocked by the planet, so a below-horizon sun contributes
	// no direct light by construction.
	inline Zenith_Maths::Vector3 TransmittanceToAtmosphereTop(
		float fRadius,
		float fMu,
		const Zenith_Maths::Vector3& xRayleighCoeff,
		float fMieCoeff,
		const Zenith_Maths::Vector2& xScaleHeights,
		float fPlanetRadius,
		float fAtmosphereRadius,
		u_int uSamples)
	{
		const Zenith_Maths::Vector3 xPos(0.0f, 0.0f, fRadius);
		const Zenith_Maths::Vector3 xDir(sqrtf(glm::max(0.0f, 1.0f - fMu * fMu)), 0.0f, fMu);

		const Zenith_Maths::Vector2 xPlanet = RaySphereIntersect(xPos, xDir, fPlanetRadius);
		if (xPlanet.x > 0.0f)
		{
			return Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f);
		}

		const Zenith_Maths::Vector2 xAtmos = RaySphereIntersect(xPos, xDir, fAtmosphereRadius);
		const float fDist = glm::max(0.0f, xAtmos.y);

		const Zenith_Maths::Vector2 xOpticalDepth = ComputeOpticalDepth(
			xPos, xDir, fDist, xScaleHeights, fPlanetRadius, uSamples);

		return Zenith_Maths::Vector3(
			expf(-xRayleighCoeff.x * xOpticalDepth.x - fMieCoeff * xOpticalDepth.y),
			expf(-xRayleighCoeff.y * xOpticalDepth.x - fMieCoeff * xOpticalDepth.y),
			expf(-xRayleighCoeff.z * xOpticalDepth.x - fMieCoeff * xOpticalDepth.y));
	}

	// Per-channel sun transmittance at the reference height for a sun whose
	// direction of TRAVEL (into the scene, matching Zenith_SunComponent /
	// Flux_GraphicsImpl::GetSunDir)
	// is xSunDirTravel. Coefficients come from AtmosphereConfig scaled by the
	// same runtime Rayleigh/Mie scales the sky shaders receive.
	inline Zenith_Maths::Vector3 ComputeSunTransmittance(
		const Zenith_Maths::Vector3& xSunDirTravel,
		float fRayleighScale,
		float fMieScale,
		float fRayleighScaleHeight = AtmosphereConfig::fRAYLEIGH_SCALE_HEIGHT,
		float fMieScaleHeight = AtmosphereConfig::fMIE_SCALE_HEIGHT)
	{
		const Zenith_Maths::Vector3 xToSun = -glm::normalize(xSunDirTravel);
		// World up is +Y; at the reference point the local zenith IS +Y, so the
		// zenith cosine is just the y component of the to-sun direction.
		const float fMu = xToSun.y;
		const Zenith_Maths::Vector3 xRayleigh(
			AtmosphereConfig::afRAYLEIGH_SCATTER[0] * fRayleighScale,
			AtmosphereConfig::afRAYLEIGH_SCATTER[1] * fRayleighScale,
			AtmosphereConfig::afRAYLEIGH_SCATTER[2] * fRayleighScale);
		return TransmittanceToAtmosphereTop(
			AtmosphereConfig::fEARTH_RADIUS + fREFERENCE_HEIGHT_METERS,
			fMu,
			xRayleigh,
			AtmosphereConfig::fMIE_SCATTER * fMieScale,
			Zenith_Maths::Vector2(fRayleighScaleHeight, fMieScaleHeight),
			AtmosphereConfig::fEARTH_RADIUS,
			AtmosphereConfig::fATMOSPHERE_RADIUS,
			AtmosphereConfig::uDEFAULT_LIGHT_SAMPLES);
	}

	// The derived sun key: chromaticity = per-channel transmittance, HDR
	// radiance scalar = the anchor. Consumers evaluate rgb * a, so this packs
	// E_sun(ground) = fSunIntensity * T_rgb exactly.
	//
	// The scale heights default to the Earth values so an opt-out caller keeps
	// byte-identical behaviour; a scene that authors them MUST pass them, or the
	// direct sun key stops agreeing with the sky and the IBL (which is the whole
	// point of deriving it here rather than authoring a colour).
	inline Zenith_Maths::Vector4 ComputeSunColourRadiance(
		const Zenith_Maths::Vector3& xSunDirTravel,
		float fSunIntensity,
		float fRayleighScale,
		float fMieScale,
		float fRayleighScaleHeight = AtmosphereConfig::fRAYLEIGH_SCALE_HEIGHT,
		float fMieScaleHeight = AtmosphereConfig::fMIE_SCALE_HEIGHT)
	{
		const Zenith_Maths::Vector3 xT = ComputeSunTransmittance(
			xSunDirTravel, fRayleighScale, fMieScale, fRayleighScaleHeight, fMieScaleHeight);
		return Zenith_Maths::Vector4(xT.x, xT.y, xT.z, fSunIntensity);
	}

	// =====================================================================
	// Transmittance-LUT invalidation predicate (PURE -- no engine state).
	//
	// An atmosphere-model change must invalidate the right renderer work, and
	// which work depends on which physical term the parameter enters:
	//   - Rayleigh/Mie DENSITY scales enter the transmittance integral -> a
	//     change invalidates the transmittance LUT, the per-frame sky-view LUT,
	//     the direct sun key AND the IBL capture.
	//   - The Mie-G PHASE asymmetry enters only the angular scatter
	//     distribution (the sky radiance), NOT transmittance -> a change
	//     invalidates the sky-view LUT + the IBL capture but NOT this LUT.
	// The IBL-capture half of that split lives in Flux_IBLEnvironment::Differs
	// (Flux/IBL/Flux_IBLImpl.h), which additionally covers the sun direction;
	// this predicate is only about the transmittance LUT. Extracted as a pure
	// function so the invalidation logic is unit-testable without a live render
	// graph (mirrors Flux_IBLRegen). Exact comparison: the values are authored
	// floats / engine constants, applied verbatim.
	// =====================================================================

	// Did the transmittance LUT's inputs change? Everything that enters the
	// optical-depth integral: the Rayleigh + Mie DENSITY scales and the two
	// exponential SCALE HEIGHTS. NOT Mie-G (phase, not density), NOT the ground
	// albedo (capture-only), NOT the radiometric anchor, NOT the sun direction.
	inline bool TransmittanceLUTChanged(
		float fPrevRayleigh, float fPrevMie, float fPrevRayleighHeight, float fPrevMieHeight,
		float fCurRayleigh,  float fCurMie,  float fCurRayleighHeight,  float fCurMieHeight)
	{
		return fPrevRayleigh       != fCurRayleigh
			|| fPrevMie            != fCurMie
			|| fPrevRayleighHeight != fCurRayleighHeight
			|| fPrevMieHeight      != fCurMieHeight;
	}

	// =====================================================================
	// Multiple-scattering LUT bake constants.
	//
	// Mirrors MultiScatterConstantsLayout in
	// Shaders/Skybox/Flux_MultiScatterLUT.slang. There are TWO consumers with
	// TWO separate LUTs, deliberately:
	//   - the Skybox bakes from the LIVE medium (the visible sky must track an
	//     authored change the frame it happens), and
	//   - the IBL bakes from its frozen GENERATION SNAPSHOT (a mid-generation
	//     medium change must not leak into faces still being convolved).
	// One shared LUT would force one of them to be wrong. At 32x32 RGBA16F the
	// duplicate costs ~8 KB, which is not a trade worth thinking about.
	// =====================================================================
	struct MultiScatterConstants
	{
		float m_afRayleighScatter[4];  // rgb = density-scaled coefficients, w = scale height
		float m_afMieScatter[4];       // rgb = density-scaled coefficient,  w = scale height
		float m_fPlanetRadius;
		float m_fAtmosphereRadius;
		float m_fGroundAlbedo;
		float m_fPad;
	};
	static_assert(sizeof(MultiScatterConstants) == 48,
		"multi-scatter bake constants must match the Slang layout");

	// =====================================================================
	// CPU mirror of Shaders/Common/MultiScatter.slang ComputeMultiScatterPsi.
	//
	// Same role as the transmittance mirror above: the estimator is a closed-form
	// approximation with a DIVISION BY (1 - f_ms) in it, and a numerical f_ms >= 1
	// would turn the sky infinite. Mirroring it on the CPU makes the bounds and
	// the monotonicity properties assertable without a GPU readback.
	//
	// LIMIT (be honest about what this proves): a mirror validates the ALGORITHM,
	// not the .slang. A typo in the shader would not be caught here -- that is
	// what the on/off A/B capture is for. What this does catch is the class of
	// bug that silently produces NaN, negative radiance, or a runaway series.
	// =====================================================================

	struct MultiScatterEstimate
	{
		Zenith_Maths::Vector3 m_xPsiMs      = Zenith_Maths::Vector3(0.0f); // the stored quantity
		Zenith_Maths::Vector3 m_xSecondOrder = Zenith_Maths::Vector3(0.0f); // L_2
		Zenith_Maths::Vector3 m_xFms        = Zenith_Maths::Vector3(0.0f); // scattered-back fraction
	};

	// Mirrors MultiScatterSphereDirection (Fibonacci spiral, deterministic).
	inline Zenith_Maths::Vector3 MultiScatterSphereDirection(u_int uIndex, u_int uCount)
	{
		constexpr float fGOLDEN_ANGLE = 2.399963229728653f; // pi * (3 - sqrt(5))
		const float fZ = 1.0f - (2.0f * static_cast<float>(uIndex) + 1.0f) / static_cast<float>(uCount);
		const float fR = sqrtf(glm::max(0.0f, 1.0f - fZ * fZ));
		const float fPhi = fGOLDEN_ANGLE * static_cast<float>(uIndex);
		return Zenith_Maths::Vector3(fR * cosf(fPhi), fR * sinf(fPhi), fZ);
	}

	inline MultiScatterEstimate ComputeMultiScatterPsi(
		float fRadius,
		float fMuSun,
		const Zenith_Maths::Vector3& xRayleighCoeff,
		float fMieCoeff,
		const Zenith_Maths::Vector2& xScaleHeights,
		float fPlanetRadius,
		float fAtmosphereRadius,
		float fGroundAlbedo,
		u_int uSphereSamples = 64u,
		u_int uMarchSteps = 20u,
		u_int uTransmittanceSamples = 8u)
	{
		constexpr float fPI = 3.14159265358979323846f;
		const Zenith_Maths::Vector3 xPos(0.0f, 0.0f, fRadius);
		const Zenith_Maths::Vector3 xSunDir(
			sqrtf(glm::max(0.0f, 1.0f - fMuSun * fMuSun)), 0.0f, fMuSun);
		const float fUniformPhase = 1.0f / (4.0f * fPI);

		Zenith_Maths::Vector3 xL2ndOrder(0.0f);
		Zenith_Maths::Vector3 xFmsAccum(0.0f);

		for (u_int uDir = 0u; uDir < uSphereSamples; uDir++)
		{
			const Zenith_Maths::Vector3 xRayDir = MultiScatterSphereDirection(uDir, uSphereSamples);

			const Zenith_Maths::Vector2 xAtmos = RaySphereIntersect(xPos, xRayDir, fAtmosphereRadius);
			const Zenith_Maths::Vector2 xPlanet = RaySphereIntersect(xPos, xRayDir, fPlanetRadius);
			float fRayEnd = glm::max(0.0f, xAtmos.y);
			const bool bHitGround = xPlanet.x > 0.0f;
			if (bHitGround) fRayEnd = glm::min(fRayEnd, xPlanet.x);
			if (fRayEnd <= 0.0f) continue;

			const float fStepSize = fRayEnd / static_cast<float>(uMarchSteps);
			Zenith_Maths::Vector3 xThroughput(1.0f);
			Zenith_Maths::Vector3 xLuminance(0.0f);
			Zenith_Maths::Vector3 xFmsThisRay(0.0f);

			for (u_int uStep = 0u; uStep < uMarchSteps; uStep++)
			{
				const float fT = (static_cast<float>(uStep) + 0.5f) * fStepSize;
				const Zenith_Maths::Vector3 xSamplePos = xPos + xRayDir * fT;
				const float fSampleR = glm::length(xSamplePos);
				const Zenith_Maths::Vector2 xDensity = GetDensity(fSampleR - fPlanetRadius, xScaleHeights);

				const Zenith_Maths::Vector3 xScattering = xRayleighCoeff * xDensity.x + fMieCoeff * xDensity.y;
				const Zenith_Maths::Vector3 xExtinction = xScattering;

				const Zenith_Maths::Vector3 xStepTransmittance = glm::exp(-xExtinction * fStepSize);
				const Zenith_Maths::Vector3 xSafeExtinction = glm::max(xExtinction, Zenith_Maths::Vector3(1e-9f));
				const Zenith_Maths::Vector3 xIntegral =
					(Zenith_Maths::Vector3(1.0f) - xStepTransmittance) / xSafeExtinction;

				xFmsThisRay += xThroughput * xScattering * xIntegral * fUniformPhase * (4.0f * fPI);

				const float fSampleMuSun = glm::dot(xSamplePos / fSampleR, xSunDir);
				const Zenith_Maths::Vector3 xSunTrans = TransmittanceToAtmosphereTop(
					fSampleR, fSampleMuSun, xRayleighCoeff, fMieCoeff, xScaleHeights,
					fPlanetRadius, fAtmosphereRadius, uTransmittanceSamples);
				xLuminance += xThroughput * xSunTrans * xScattering * xIntegral * fUniformPhase;

				xThroughput *= xStepTransmittance;
			}

			if (bHitGround && fGroundAlbedo > 0.0f)
			{
				const Zenith_Maths::Vector3 xHit = xPos + xRayDir * fRayEnd;
				const float fHitR = glm::length(xHit);
				const float fMuG = glm::dot(xHit / fHitR, xSunDir);
				if (fMuG > 0.0f)
				{
					const Zenith_Maths::Vector3 xSunTrans = TransmittanceToAtmosphereTop(
						fHitR, fMuG, xRayleighCoeff, fMieCoeff, xScaleHeights,
						fPlanetRadius, fAtmosphereRadius, uTransmittanceSamples);
					xLuminance += xThroughput * xSunTrans * fMuG * (fGroundAlbedo / fPI);
				}
			}

			xL2ndOrder += xLuminance;
			xFmsAccum += xFmsThisRay;
		}

		const float fInvSamples = 1.0f / static_cast<float>(uSphereSamples);
		MultiScatterEstimate xOut;
		xOut.m_xSecondOrder = xL2ndOrder * fInvSamples;
		xOut.m_xFms = xFmsAccum * fInvSamples;
		const Zenith_Maths::Vector3 xOneMinusF =
			glm::max(Zenith_Maths::Vector3(1.0f) - xOut.m_xFms, Zenith_Maths::Vector3(1e-4f));
		xOut.m_xPsiMs = xOut.m_xSecondOrder / xOneMinusF;
		return xOut;
	}

	// The coefficients are pre-scaled by the authored densities here, exactly as
	// the sky and capture shaders do, so the LUT and its consumers integrate the
	// same medium.
	inline MultiScatterConstants BuildMultiScatterConstants(
		float fRayleighScale, float fMieScale,
		float fRayleighScaleHeight, float fMieScaleHeight,
		float fGroundAlbedo)
	{
		MultiScatterConstants x = {};
		x.m_afRayleighScatter[0] = AtmosphereConfig::afRAYLEIGH_SCATTER[0] * fRayleighScale;
		x.m_afRayleighScatter[1] = AtmosphereConfig::afRAYLEIGH_SCATTER[1] * fRayleighScale;
		x.m_afRayleighScatter[2] = AtmosphereConfig::afRAYLEIGH_SCATTER[2] * fRayleighScale;
		x.m_afRayleighScatter[3] = fRayleighScaleHeight;
		const float fMie = AtmosphereConfig::fMIE_SCATTER * fMieScale;
		x.m_afMieScatter[0] = fMie;
		x.m_afMieScatter[1] = fMie;
		x.m_afMieScatter[2] = fMie;
		x.m_afMieScatter[3] = fMieScaleHeight;
		x.m_fPlanetRadius     = AtmosphereConfig::fEARTH_RADIUS;
		x.m_fAtmosphereRadius = AtmosphereConfig::fATMOSPHERE_RADIUS;
		x.m_fGroundAlbedo     = fGroundAlbedo;
		return x;
	}
}
