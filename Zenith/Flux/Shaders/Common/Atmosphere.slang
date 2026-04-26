// =====================================================================
// Common.Atmosphere — shared Slang module.
//
// Slang port of Skybox/Flux_AtmosphereCommon.fxh. Rayleigh + Mie
// scattering for physically-based sky rendering, ray-sphere intersect,
// optical-depth integrator, full single-scatter atmosphere solver, and
// limb-darkened sun disk.
//
// Naming: Common.Volumetric already exposes a `RayleighPhase` helper
// with the identical formula. To avoid symbol collision when a shader
// imports both modules, Common.Atmosphere does NOT re-export
// RayleighPhase — shaders that need it pull it from Common.Volumetric.
// MiePhase IS exposed here because Volumetric's HenyeyGreenstein has a
// different name (the maths is the same).
// =====================================================================

import Common.PBR; // for PI

// ----- Atmosphere constants (defaults from Flux_Skybox.h AtmosphereConfig) -----

public static const float EARTH_RADIUS                  = 6360000.0;
public static const float ATMOSPHERE_RADIUS             = 6420000.0;
public static const float ATMOSPHERE_HEIGHT             = 60000.0;

public static const float3 DEFAULT_RAYLEIGH_COEFF       = float3(5.8e-6, 13.5e-6, 33.1e-6);
public static const float  DEFAULT_MIE_COEFF            = 3.996e-6;
public static const float  DEFAULT_RAYLEIGH_SCALE_HEIGHT = 8000.0;
public static const float  DEFAULT_MIE_SCALE_HEIGHT     = 1200.0;
public static const float  DEFAULT_MIE_G                = 0.76;
public static const uint   DEFAULT_SKY_SAMPLES          = 16u;
public static const uint   DEFAULT_LIGHT_SAMPLES        = 8u;

public struct AtmosphereResult
{
	public float3 xColor;
	public float  fTransmittance;
};

// Mie phase function (Henyey-Greenstein form). Same formula as
// Common.Volumetric.HenyeyGreenstein — exposed under the conventional
// atmospheric name so shader code reads naturally.
public float MiePhase(float fCosTheta, float fG)
{
	float fG2    = fG * fG;
	float fNum   = 1.0 - fG2;
	float fDenom = 4.0 * PI * pow(1.0 + fG2 - 2.0 * fG * fCosTheta, 1.5);
	return fNum / fDenom;
}

// Atmospheric density at a given height above the planet surface.
// Returns (rayleigh density, mie density) — both fall off exponentially
// with their own scale heights.
public float2 GetDensity(float fHeight, float2 xScaleHeights)
{
	return float2(exp(-fHeight / xScaleHeights.x),
				   exp(-fHeight / xScaleHeights.y));
}

// Ray-sphere intersect, both roots. Returns (-1, -1) on miss; near root
// can be negative when the origin is inside the sphere.
public float2 RaySphereIntersect(float3 xRayOrigin, float3 xRayDir, float fSphereRadius)
{
	float fB = 2.0 * dot(xRayOrigin, xRayDir);
	float fC = dot(xRayOrigin, xRayOrigin) - fSphereRadius * fSphereRadius;
	float fDiscriminant = fB * fB - 4.0 * fC;
	if (fDiscriminant < 0.0) return float2(-1.0, -1.0);
	float fSqrtDisc = sqrt(fDiscriminant);
	return float2((-fB - fSqrtDisc) * 0.5, (-fB + fSqrtDisc) * 0.5);
}

// Optical depth integrated along a finite ray segment. Used both inside
// the main scattering loop (light-ray path) and for view transmittance.
public float2 ComputeOpticalDepth(float3 xOrigin,
								   float3 xDir,
								   float  fDistance,
								   float3 xRayleighCoeff,
								   float  fMieCoeff,
								   float2 xScaleHeights,
								   float  fPlanetRadius,
								   uint   uSamples)
{
	float  fStepSize     = fDistance / (float)uSamples;
	float2 xOpticalDepth = float2(0.0, 0.0);
	for (uint i = 0; i < uSamples; i++)
	{
		float  fT         = ((float)i + 0.5) * fStepSize;
		float3 xSamplePos = xOrigin + xDir * fT;
		float  fHeight    = length(xSamplePos) - fPlanetRadius;
		xOpticalDepth    += GetDensity(fHeight, xScaleHeights) * fStepSize;
	}
	return xOpticalDepth;
}

// Full atmosphere single-scattering solver. Returns (in-scatter RGB,
// view transmittance). View transmittance is the average of the per-
// channel exponentials — mirrors the GLSL contract.
public float4 ComputeAtmosphereScattering(float3 xRayOrigin,
											float3 xRayDir,
											float3 xSunDir,
											float  fMaxDistance,
											float3 xRayleighCoeff,
											float  fMieCoeff,
											float2 xScaleHeights,
											float  fMieG,
											float  fPlanetRadius,
											float  fAtmosphereRadius,
											float  fSunIntensity,
											uint   uSkySamples,
											uint   uLightSamples)
{
	float2 xAtmosIntersect = RaySphereIntersect(xRayOrigin, xRayDir, fAtmosphereRadius);
	if (xAtmosIntersect.y < 0.0) return float4(0.0, 0.0, 0.0, 1.0); // ray misses atmosphere

	float fRayStart = max(0.0, xAtmosIntersect.x);
	float fRayEnd   = min(fMaxDistance, xAtmosIntersect.y);

	// Planet hit — the view ray terminates on the surface.
	float2 xPlanetIntersect = RaySphereIntersect(xRayOrigin, xRayDir, fPlanetRadius);
	if (xPlanetIntersect.x > 0.0) fRayEnd = min(fRayEnd, xPlanetIntersect.x);

	if (fRayEnd <= fRayStart) return float4(0.0, 0.0, 0.0, 1.0);

	float fStepSize = (fRayEnd - fRayStart) / (float)uSkySamples;
	float fCosTheta = dot(xRayDir, xSunDir);

	// Rayleigh phase formula inlined here — Common.Volumetric exposes a
	// public RayleighPhase that's identical, but we don't import it to
	// avoid forcing every consumer of Common.Atmosphere to also pull in
	// the volumetric helpers.
	float fRayleighPhase = (3.0 / (16.0 * PI)) * (1.0 + fCosTheta * fCosTheta);
	float fMiePhase      = MiePhase(fCosTheta, fMieG);

	float3 xInscatterR        = float3(0.0, 0.0, 0.0);
	float3 xInscatterM        = float3(0.0, 0.0, 0.0);
	float2 xOpticalDepthView  = float2(0.0, 0.0);

	for (uint i = 0; i < uSkySamples; i++)
	{
		float  fT         = fRayStart + ((float)i + 0.5) * fStepSize;
		float3 xSamplePos = xRayOrigin + xRayDir * fT;
		float  fHeight    = length(xSamplePos) - fPlanetRadius;
		float2 xDensity   = GetDensity(fHeight, xScaleHeights);

		xOpticalDepthView += xDensity * fStepSize;

		float2 xLightIntersect = RaySphereIntersect(xSamplePos, xSunDir, fAtmosphereRadius);
		if (xLightIntersect.y > 0.0)
		{
			// Sample is in shadow if the sun ray is blocked by the planet.
			float2 xLightPlanet = RaySphereIntersect(xSamplePos, xSunDir, fPlanetRadius);
			if (xLightPlanet.x < 0.0 || xLightPlanet.x > xLightIntersect.y)
			{
				float2 xOpticalDepthLight = ComputeOpticalDepth(
					xSamplePos, xSunDir, xLightIntersect.y,
					xRayleighCoeff, fMieCoeff, xScaleHeights,
					fPlanetRadius, uLightSamples);

				float2 xTotalOpticalDepth = xOpticalDepthView + xOpticalDepthLight;
				float3 xTransmittance = exp(
					-xRayleighCoeff * xTotalOpticalDepth.x
					- fMieCoeff      * xTotalOpticalDepth.y);

				xInscatterR += xDensity.x * xTransmittance * fStepSize;
				xInscatterM += xDensity.y * xTransmittance * fStepSize;
			}
		}
	}

	float3 xRayleighScatter = xInscatterR * xRayleighCoeff * fRayleighPhase;
	float3 xMieScatter      = xInscatterM * fMieCoeff      * fMiePhase;
	float3 xInscatter       = (xRayleighScatter + xMieScatter) * fSunIntensity;

	float3 xViewTransmittance = exp(
		-xRayleighCoeff * xOpticalDepthView.x
		- fMieCoeff      * xOpticalDepthView.y);
	float fTransmittance = dot(xViewTransmittance, float3(0.333, 0.333, 0.333));

	return float4(xInscatter, fTransmittance);
}

// Limb-darkened sun disk. Angular radius ~0.53 degrees. Returns the
// emissive contribution at xRayDir; outside the disk, returns black.
public float3 RenderSunDisk(float3 xRayDir, float3 xSunDir, float fSunIntensity)
{
	const float fSunAngularRadius = 0.00935;
	float fCosAngle = dot(xRayDir, xSunDir);
	float fSunAngle = acos(clamp(fCosAngle, -1.0, 1.0));
	if (fSunAngle < fSunAngularRadius)
	{
		float fLimbDarkening = 1.0 - 0.6 * (1.0 - sqrt(1.0 - pow(fSunAngle / fSunAngularRadius, 2.0)));
		float v = fSunIntensity * fLimbDarkening;
		return float3(v, v, v);
	}
	return float3(0.0, 0.0, 0.0);
}

// IBL convenience overload — uses the documented defaults so callers
// don't have to thread through the long parameter list.
public AtmosphereResult ComputeAtmosphereScattering(float3 xRayOrigin,
													  float3 xRayDir,
													  float3 xSunDir,
													  float  fSunIntensity)
{
	float4 xResult = ComputeAtmosphereScattering(
		xRayOrigin, xRayDir, xSunDir, 100000.0,
		DEFAULT_RAYLEIGH_COEFF, DEFAULT_MIE_COEFF,
		float2(DEFAULT_RAYLEIGH_SCALE_HEIGHT, DEFAULT_MIE_SCALE_HEIGHT),
		DEFAULT_MIE_G, EARTH_RADIUS, ATMOSPHERE_RADIUS,
		fSunIntensity, DEFAULT_SKY_SAMPLES, DEFAULT_LIGHT_SAMPLES);
	AtmosphereResult o;
	o.xColor         = xResult.rgb;
	o.fTransmittance = xResult.a;
	return o;
}
