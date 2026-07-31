// =====================================================================
// Common.MultiScatter — shared Slang module.
//
// The Hillaire multiple-scattering estimate, factored out of any pass so
// BOTH bakes can call it: the Skybox's copy (live medium, feeds the
// sky-view LUT) and the IBL's copy (frozen generation snapshot, feeds the
// irradiance/prefilter convolutions). They cannot share one LUT -- one of
// them would then be integrating the wrong medium -- but they must share
// the MATHS, or the visible sky and the ambient it lights the scene with
// would drift apart for the silliest possible reason.
//
// Declares no ParameterBlock and touches no spine accessor, so it is a
// first-class `import` module (see Shaders/SHADER_STYLE.md).
//
// Why it exists: single scattering alone loses the horizon brightening,
// the ambient lift under a bright sky, and most of the twilight glow --
// the sky reads flat and too dark, and an IBL captured from it inherits
// exactly the same deficit. Adding orders 2..infinity properly would be a
// nested integral per sample; Hillaire's approximation makes it tractable
// by assuming the multiply-scattered field is ISOTROPIC and spatially
// smooth, which lets the infinite series collapse to a closed form:
//   1. Integrate a UNIFORM-PHASE single-scatter pass over a sphere of
//      directions from the sample point, accumulating
//        L_2   -- the second-order radiance, and
//        f_ms  -- the fraction of light scattered back into the point.
//   2. The higher orders form a geometric series in f_ms, so
//        psi_ms = L_2 / (1 - f_ms).
// A consumer adds psi_ms * (rayleighDensity*Rs + mieDensity*Ms) * ds, with
// no phase term (the field is isotropic by assumption).
// =====================================================================

import Common.PBR;         // PI
import Common.Atmosphere;  // GetDensity / RaySphereIntersect / TransmittanceToAtmosphereTop

// Directions sampled over the sphere, and raymarch steps along each. 64/20 is
// Hillaire's production setting; this bakes only on a medium change, so the
// cost is a one-off rather than a per-frame budget item.
public static const uint MULTISCATTER_SPHERE_SAMPLES = 64u;
public static const uint MULTISCATTER_MARCH_STEPS    = 20u;
// Inner sun-ray optical-depth samples. Matches DEFAULT_LIGHT_SAMPLES so the
// bake integrates the same medium at the same fidelity as the sky solver.
public static const uint MULTISCATTER_TRANSMITTANCE_SAMPLES = 8u;

// Uniform-ish sphere sampling: a Fibonacci spiral. Deterministic (no RNG, so
// the bake is bit-reproducible) and far lower discrepancy than lat-long
// stratification at this sample count.
public float3 MultiScatterSphereDirection(uint uIndex, uint uCount)
{
	const float fGoldenAngle = 2.399963229728653; // pi * (3 - sqrt(5))
	float fZ   = 1.0 - (2.0 * (float)uIndex + 1.0) / (float)uCount;
	float fR   = sqrt(max(0.0, 1.0 - fZ * fZ));
	float fPhi = fGoldenAngle * (float)uIndex;
	return float3(fR * cos(fPhi), fR * sin(fPhi), fZ);
}

// psi_ms at (radius, sun-zenith cosine) for the given medium.
//
// The sun-ray term is integrated ANALYTICALLY rather than sampled from the
// transmittance LUT. That is not a micro-optimisation, it is an ordering
// requirement: IBL is a render feature declared BEFORE Skybox, so the IBL's
// bake sampling the Skybox's transmittance LUT would be a read whose only
// writer is declared later in the setup walk -- the render graph drops that
// edge and its barrier. Integrating here costs a few ms on a medium change and
// removes the LUT's interpolation error from the result as a bonus.
public float3 ComputeMultiScatterPsi(float  fRadius,
									  float  fMuSun,
									  float3 xRayleighCoeff,
									  float  fMieCoeff,
									  float2 xScaleHeights,
									  float  fPlanetRadius,
									  float  fAtmosphereRadius,
									  float  fGroundAlbedo)
{
	// Sample point on the local zenith axis (+Z here, matching
	// TransmittanceToAtmosphereTop's frame), and the sun in that same frame.
	float3 xPos    = float3(0.0, 0.0, fRadius);
	float3 xSunDir = float3(sqrt(max(0.0, 1.0 - fMuSun * fMuSun)), 0.0, fMuSun);

	// Isotropic phase: the whole point of the approximation.
	const float fUniformPhase = 1.0 / (4.0 * PI);

	float3 xL2ndOrder = float3(0.0, 0.0, 0.0);
	float3 xFmsAccum  = float3(0.0, 0.0, 0.0);

	for (uint uDir = 0u; uDir < MULTISCATTER_SPHERE_SAMPLES; uDir++)
	{
		float3 xRayDir = MultiScatterSphereDirection(uDir, MULTISCATTER_SPHERE_SAMPLES);

		// March to the atmosphere top, or to the ground if this direction hits it.
		float2 xAtmos  = RaySphereIntersect(xPos, xRayDir, fAtmosphereRadius);
		float2 xPlanet = RaySphereIntersect(xPos, xRayDir, fPlanetRadius);
		float fRayEnd = max(0.0, xAtmos.y);
		bool bHitGround = xPlanet.x > 0.0;
		if (bHitGround) fRayEnd = min(fRayEnd, xPlanet.x);
		if (fRayEnd <= 0.0) continue;

		float fStepSize = fRayEnd / (float)MULTISCATTER_MARCH_STEPS;

		float3 xThroughput = float3(1.0, 1.0, 1.0);   // view-path transmittance
		float3 xLuminance  = float3(0.0, 0.0, 0.0);   // 2nd-order radiance along this ray
		float3 xFmsThisRay = float3(0.0, 0.0, 0.0);   // scattered-back fraction

		for (uint uStep = 0u; uStep < MULTISCATTER_MARCH_STEPS; uStep++)
		{
			float  fT         = ((float)uStep + 0.5) * fStepSize;
			float3 xSamplePos = xPos + xRayDir * fT;
			float  fSampleR   = length(xSamplePos);
			float  fHeight    = fSampleR - fPlanetRadius;
			float2 xDensity   = GetDensity(fHeight, xScaleHeights);

			float3 xScattering = xRayleighCoeff * xDensity.x + fMieCoeff * xDensity.y;
			// Extinction == scattering here: the medium is treated as purely
			// scattering. TODO(ozone): once an absorption layer exists it must be
			// added to xExtinction ONLY, never to xScattering -- ozone removes
			// energy without redirecting it, and folding it into the scattering
			// term would brighten the sky instead of tinting it.
			float3 xExtinction = xScattering;

			// Analytic in-scatter integral over the step (Hillaire's energy-
			// conserving form) rather than a midpoint sample: at 20 steps the
			// midpoint version visibly under-integrates near the ground.
			float3 xStepTransmittance = exp(-xExtinction * fStepSize);
			float3 xSafeExtinction    = max(xExtinction, float3(1e-9, 1e-9, 1e-9));
			float3 xIntegral          = (float3(1.0, 1.0, 1.0) - xStepTransmittance) / xSafeExtinction;

			// f_ms: fraction of energy scattered back into the point, phase-free.
			xFmsThisRay += xThroughput * xScattering * xIntegral * fUniformPhase * (4.0 * PI);

			// Second-order radiance: sunlight reaching this sample, scattered once
			// more with the uniform phase. TransmittanceToAtmosphereTop returns
			// exactly zero when the planet blocks the sun ray, so it doubles as
			// the shadow test.
			float  fSampleMuSun = dot(xSamplePos / fSampleR, xSunDir);
			float3 xSunTrans    = TransmittanceToAtmosphereTop(
				fSampleR, fSampleMuSun, xRayleighCoeff, fMieCoeff, xScaleHeights,
				fPlanetRadius, fAtmosphereRadius, MULTISCATTER_TRANSMITTANCE_SAMPLES);
			xLuminance += xThroughput * xSunTrans * xScattering * xIntegral * fUniformPhase;

			xThroughput *= xStepTransmittance;
		}

		// Ground bounce: light that reaches the surface, is reflected
		// Lambertian, and re-enters the sky. Without it the lower hemisphere
		// contributes nothing to the multiply-scattered field and the whole
		// effect is under-energised over bright terrain.
		if (bHitGround && fGroundAlbedo > 0.0)
		{
			float3 xHit  = xPos + xRayDir * fRayEnd;
			float  fHitR = length(xHit);
			float  fMuG  = dot(xHit / fHitR, xSunDir);
			if (fMuG > 0.0)
			{
				float3 xSunTrans = TransmittanceToAtmosphereTop(
					fHitR, fMuG, xRayleighCoeff, fMieCoeff, xScaleHeights,
					fPlanetRadius, fAtmosphereRadius, MULTISCATTER_TRANSMITTANCE_SAMPLES);
				xLuminance += xThroughput * xSunTrans * fMuG * (fGroundAlbedo / PI);
			}
		}

		xL2ndOrder += xLuminance;
		xFmsAccum  += xFmsThisRay;
	}

	// Average over the sphere of directions.
	float fInvSamples = 1.0 / (float)MULTISCATTER_SPHERE_SAMPLES;
	xL2ndOrder *= fInvSamples;
	xFmsAccum  *= fInvSamples;

	// Geometric series over the remaining orders. f_ms < 1 physically (a purely
	// scattering medium cannot return more than it receives), but clamp anyway:
	// a numerical f_ms >= 1 would blow the division up into an infinite sky.
	float3 xOneMinusF = max(float3(1.0, 1.0, 1.0) - xFmsAccum, float3(1e-4, 1e-4, 1e-4));
	return xL2ndOrder / xOneMinusF;
}
