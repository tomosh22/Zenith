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
	// direction of TRAVEL (into the scene, matching dbg_SunDir / GetSunDir)
	// is xSunDirTravel. Coefficients come from AtmosphereConfig scaled by the
	// same runtime Rayleigh/Mie scales the sky shaders receive.
	inline Zenith_Maths::Vector3 ComputeSunTransmittance(
		const Zenith_Maths::Vector3& xSunDirTravel,
		float fRayleighScale,
		float fMieScale)
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
			Zenith_Maths::Vector2(AtmosphereConfig::fRAYLEIGH_SCALE_HEIGHT, AtmosphereConfig::fMIE_SCALE_HEIGHT),
			AtmosphereConfig::fEARTH_RADIUS,
			AtmosphereConfig::fATMOSPHERE_RADIUS,
			AtmosphereConfig::uDEFAULT_LIGHT_SAMPLES);
	}

	// The derived sun key: chromaticity = per-channel transmittance, HDR
	// radiance scalar = the anchor. Consumers evaluate rgb * a, so this packs
	// E_sun(ground) = fSunIntensity * T_rgb exactly.
	inline Zenith_Maths::Vector4 ComputeSunColourRadiance(
		const Zenith_Maths::Vector3& xSunDirTravel,
		float fSunIntensity,
		float fRayleighScale,
		float fMieScale)
	{
		const Zenith_Maths::Vector3 xT = ComputeSunTransmittance(xSunDirTravel, fRayleighScale, fMieScale);
		return Zenith_Maths::Vector4(xT.x, xT.y, xT.z, fSunIntensity);
	}
}
