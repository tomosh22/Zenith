// Atmosphere Common Functions
// Rayleigh and Mie scattering for physically-based sky rendering

// Constants from Flux_Skybox.h AtmosphereConfig namespace
#define EARTH_RADIUS 6360000.0
#define ATMOSPHERE_RADIUS 6420000.0
#define ATMOSPHERE_HEIGHT 60000.0

// Default scattering coefficients
#define DEFAULT_RAYLEIGH_COEFF vec3(5.8e-6, 13.5e-6, 33.1e-6)
#define DEFAULT_MIE_COEFF 3.996e-6
#define DEFAULT_RAYLEIGH_SCALE_HEIGHT 8000.0
#define DEFAULT_MIE_SCALE_HEIGHT 1200.0
#define DEFAULT_MIE_G 0.76
#define DEFAULT_SKY_SAMPLES 16u
#define DEFAULT_LIGHT_SAMPLES 8u

// Result structure for convenience functions
struct AtmosphereResult
{
	vec3 xColor;
	float fTransmittance;
};

// Rayleigh phase function
// Symmetric scattering pattern (blue sky)
float RayleighPhase(float fCosTheta)
{
	return (3.0 / (16.0 * 3.14159265)) * (1.0 + fCosTheta * fCosTheta);
}

// Mie phase function (Henyey-Greenstein approximation)
// Forward scattering pattern (sun haze)
float MiePhase(float fCosTheta, float fG)
{
	float fG2 = fG * fG;
	float fNum = (1.0 - fG2);
	float fDenom = 4.0 * 3.14159265 * pow(1.0 + fG2 - 2.0 * fG * fCosTheta, 1.5);
	return fNum / fDenom;
}

// Get density at height above planet surface
// Returns (Rayleigh density, Mie density)
vec2 GetDensity(float fHeight, vec2 xScaleHeights)
{
	vec2 xDensity;
	xDensity.x = exp(-fHeight / xScaleHeights.x);  // Rayleigh
	xDensity.y = exp(-fHeight / xScaleHeights.y);  // Mie
	return xDensity;
}

// Ray-sphere intersection
// Returns (near, far) intersection distances, or (-1, -1) if no intersection
vec2 RaySphereIntersect(vec3 xRayOrigin, vec3 xRayDir, float fSphereRadius)
{
	float fB = 2.0 * dot(xRayOrigin, xRayDir);
	float fC = dot(xRayOrigin, xRayOrigin) - fSphereRadius * fSphereRadius;
	float fDiscriminant = fB * fB - 4.0 * fC;

	if (fDiscriminant < 0.0)
	{
		return vec2(-1.0, -1.0);
	}

	float fSqrtDisc = sqrt(fDiscriminant);
	return vec2(
		(-fB - fSqrtDisc) * 0.5,
		(-fB + fSqrtDisc) * 0.5
	);
}

// Compute optical depth from point to atmosphere edge along direction
// Used for calculating transmittance
vec2 ComputeOpticalDepth(
	vec3 xOrigin,
	vec3 xDir,
	float fDistance,
	vec3 xRayleighCoeff,
	float fMieCoeff,
	vec2 xScaleHeights,
	float fPlanetRadius,
	uint uSamples)
{
	float fStepSize = fDistance / float(uSamples);
	vec2 xOpticalDepth = vec2(0.0);

	for (uint i = 0; i < uSamples; i++)
	{
		float fT = (float(i) + 0.5) * fStepSize;
		vec3 xSamplePos = xOrigin + xDir * fT;
		float fHeight = length(xSamplePos) - fPlanetRadius;

		vec2 xDensity = GetDensity(fHeight, xScaleHeights);
		xOpticalDepth += xDensity * fStepSize;
	}

	return xOpticalDepth;
}

// Main atmosphere scattering computation
// Returns (inscatter RGB, transmittance)
vec4 ComputeAtmosphereScattering(
	vec3 xRayOrigin,
	vec3 xRayDir,
	vec3 xSunDir,
	float fMaxDistance,
	vec3 xRayleighCoeff,
	float fMieCoeff,
	vec2 xScaleHeights,
	float fMieG,
	float fPlanetRadius,
	float fAtmosphereRadius,
	float fSunIntensity,
	uint uSkySamples,
	uint uLightSamples)
{
	// Intersect ray with atmosphere
	vec2 xAtmosIntersect = RaySphereIntersect(xRayOrigin, xRayDir, fAtmosphereRadius);

	if (xAtmosIntersect.y < 0.0)
	{
		return vec4(0.0, 0.0, 0.0, 1.0);
	}

	// Clamp ray segment to atmosphere
	float fRayStart = max(0.0, xAtmosIntersect.x);
	float fRayEnd = min(fMaxDistance, xAtmosIntersect.y);

	// Check for planet intersection
	vec2 xPlanetIntersect = RaySphereIntersect(xRayOrigin, xRayDir, fPlanetRadius);
	if (xPlanetIntersect.x > 0.0)
	{
		fRayEnd = min(fRayEnd, xPlanetIntersect.x);
	}

	if (fRayEnd <= fRayStart)
	{
		return vec4(0.0, 0.0, 0.0, 1.0);
	}

	float fStepSize = (fRayEnd - fRayStart) / float(uSkySamples);
	float fCosTheta = dot(xRayDir, xSunDir);

	// Phase functions
	float fRayleighPhase = RayleighPhase(fCosTheta);
	float fMiePhase = MiePhase(fCosTheta, fMieG);

	// Accumulated values
	vec3 xInscatterR = vec3(0.0);
	vec3 xInscatterM = vec3(0.0);
	vec2 xOpticalDepthView = vec2(0.0);

	// Ray march through atmosphere
	for (uint i = 0; i < uSkySamples; i++)
	{
		float fT = fRayStart + (float(i) + 0.5) * fStepSize;
		vec3 xSamplePos = xRayOrigin + xRayDir * fT;
		float fHeight = length(xSamplePos) - fPlanetRadius;

		// Local density at sample point
		vec2 xDensity = GetDensity(fHeight, xScaleHeights);

		// Accumulate optical depth along view ray
		xOpticalDepthView += xDensity * fStepSize;

		// Compute optical depth along light ray (from sample to sun)
		vec2 xLightIntersect = RaySphereIntersect(xSamplePos, xSunDir, fAtmosphereRadius);

		if (xLightIntersect.y > 0.0)
		{
			// Check if light ray hits planet (sample is in shadow)
			vec2 xLightPlanet = RaySphereIntersect(xSamplePos, xSunDir, fPlanetRadius);
			if (xLightPlanet.x < 0.0 || xLightPlanet.x > xLightIntersect.y)
			{
				vec2 xOpticalDepthLight = ComputeOpticalDepth(
					xSamplePos, xSunDir, xLightIntersect.y,
					xRayleighCoeff, fMieCoeff, xScaleHeights,
					fPlanetRadius, uLightSamples);

				// Total optical depth: view path + light path
				vec2 xTotalOpticalDepth = xOpticalDepthView + xOpticalDepthLight;

				// Transmittance
				vec3 xTransmittance = exp(
					-xRayleighCoeff * xTotalOpticalDepth.x
					- fMieCoeff * xTotalOpticalDepth.y);

				// Accumulate inscattered light
				xInscatterR += xDensity.x * xTransmittance * fStepSize;
				xInscatterM += xDensity.y * xTransmittance * fStepSize;
			}
		}
	}

	// Apply scattering coefficients and phase functions
	vec3 xRayleighScatter = xInscatterR * xRayleighCoeff * fRayleighPhase;
	vec3 xMieScatter = xInscatterM * fMieCoeff * fMiePhase;

	vec3 xInscatter = (xRayleighScatter + xMieScatter) * fSunIntensity;

	// View transmittance
	vec3 xViewTransmittance = exp(
		-xRayleighCoeff * xOpticalDepthView.x
		- fMieCoeff * xOpticalDepthView.y);

	// Average transmittance for alpha
	float fTransmittance = dot(xViewTransmittance, vec3(0.333));

	return vec4(xInscatter, fTransmittance);
}

// Sun disk rendering
vec3 RenderSunDisk(vec3 xRayDir, vec3 xSunDir, float fSunIntensity)
{
	float fSunAngularRadius = 0.00935;  // ~0.53 degrees
	float fCosAngle = dot(xRayDir, xSunDir);
	float fSunAngle = acos(clamp(fCosAngle, -1.0, 1.0));

	if (fSunAngle < fSunAngularRadius)
	{
		// Limb darkening approximation
		float fLimbDarkening = 1.0 - 0.6 * (1.0 - sqrt(1.0 - pow(fSunAngle / fSunAngularRadius, 2.0)));
		return vec3(fSunIntensity * fLimbDarkening);
	}

	return vec3(0.0);
}

// Convenience wrapper for IBL sampling with default parameters
// Takes fewer parameters and returns an AtmosphereResult struct
AtmosphereResult ComputeAtmosphereScattering(
	vec3 xRayOrigin,
	vec3 xRayDir,
	vec3 xSunDir,
	float fSunIntensity)
{
	// Use defaults for most parameters
	vec4 xResult = ComputeAtmosphereScattering(
		xRayOrigin,
		xRayDir,
		xSunDir,
		100000.0,  // Max distance
		DEFAULT_RAYLEIGH_COEFF,
		DEFAULT_MIE_COEFF,
		vec2(DEFAULT_RAYLEIGH_SCALE_HEIGHT, DEFAULT_MIE_SCALE_HEIGHT),
		DEFAULT_MIE_G,
		float(EARTH_RADIUS),
		float(ATMOSPHERE_RADIUS),
		fSunIntensity,
		DEFAULT_SKY_SAMPLES,
		DEFAULT_LIGHT_SAMPLES);

	AtmosphereResult xOutput;
	xOutput.xColor = xResult.rgb;
	xOutput.fTransmittance = xResult.a;
	return xOutput;
}
