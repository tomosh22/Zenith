#version 450 core

#include "../Common.fxh"
#include "../GBufferCommon.fxh"
#include "Flux_AtmosphereCommon.fxh"

layout(location = 0) in vec2 a_xUV;

// Atmosphere constants buffer
layout(std140, set = 0, binding = 1) uniform AtmosphereConstants
{
	vec4 g_xRayleighScatter;      // RGB = coefficients, W = scale height
	vec4 g_xMieScatter;           // RGB = scatter, W = scale height
	float g_fPlanetRadius;
	float g_fAtmosphereRadius;
	float g_fMieG;
	float g_fSunIntensity;
	float g_fRayleighScale;
	float g_fMieScale;
	float g_fAerialStrength;
	uint g_uDebugMode;
	uint g_uSkySamples;
	uint g_uLightSamples;
	vec2 g_xPad;
};

// Debug mode constants (must match Skybox_DebugMode enum)
#define SKYBOX_DEBUG_NONE 0
#define SKYBOX_DEBUG_RAYLEIGH_ONLY 1
#define SKYBOX_DEBUG_MIE_ONLY 2
#define SKYBOX_DEBUG_TRANSMITTANCE 3
#define SKYBOX_DEBUG_SCATTER_DIRECTION 4
#define SKYBOX_DEBUG_AERIAL_DEPTH 5
#define SKYBOX_DEBUG_SUN_DISK 6
#define SKYBOX_DEBUG_LUT_PREVIEW 7
#define SKYBOX_DEBUG_RAY_STEPS 8
#define SKYBOX_DEBUG_PHASE_FUNCTION 9

// False-color heatmap for debug visualization
vec3 DebugHeatmap(float fValue)
{
	fValue = clamp(fValue, 0.0, 1.0);
	if (fValue < 0.5)
		return mix(vec3(0, 0, 1), vec3(0, 1, 0), fValue * 2.0);
	else
		return mix(vec3(0, 1, 0), vec3(1, 0, 0), (fValue - 0.5) * 2.0);
}

void main()
{
	vec3 xRayDir = RayDir(a_xUV);
	// Note: g_xSunDir_Pad is direction light travels (from sun), negate to get direction TO sun
	vec3 xSunDir = normalize(-g_xSunDir_Pad.xyz);

	// Camera position in atmosphere space (planet center at origin)
	// Assume camera is at surface level + small offset
	vec3 xCamPos = vec3(0.0, g_fPlanetRadius + 100.0, 0.0);

	// Scaled scattering coefficients
	vec3 xRayleighCoeff = g_xRayleighScatter.rgb * g_fRayleighScale;
	float fMieCoeff = g_xMieScatter.r * g_fMieScale;
	vec2 xScaleHeights = vec2(g_xRayleighScatter.w, g_xMieScatter.w);

	// Compute atmosphere scattering
	vec4 xScatterResult = ComputeAtmosphereScattering(
		xCamPos,
		xRayDir,
		xSunDir,
		1e10,  // Max distance (infinite for sky)
		xRayleighCoeff,
		fMieCoeff,
		xScaleHeights,
		g_fMieG,
		g_fPlanetRadius,
		g_fAtmosphereRadius,
		g_fSunIntensity,
		g_uSkySamples,
		g_uLightSamples
	);

	vec3 xSkyColor = xScatterResult.rgb;

	// Add sun disk
	vec3 xSunDisk = RenderSunDisk(xRayDir, xSunDir, g_fSunIntensity * 5.0);
	xSkyColor += xSunDisk * xScatterResult.a;  // Attenuate sun by transmittance

	// Debug visualizations
	if (g_uDebugMode != SKYBOX_DEBUG_NONE)
	{
		switch (g_uDebugMode)
		{
			case SKYBOX_DEBUG_RAYLEIGH_ONLY:
			{
				// Compute Rayleigh only (set Mie to 0)
				vec4 xRayleighOnly = ComputeAtmosphereScattering(
					xCamPos, xRayDir, xSunDir, 1e10,
					xRayleighCoeff, 0.0,
					xScaleHeights, g_fMieG,
					g_fPlanetRadius, g_fAtmosphereRadius,
					g_fSunIntensity, g_uSkySamples, g_uLightSamples
				);
				xSkyColor = xRayleighOnly.rgb;
				break;
			}

			case SKYBOX_DEBUG_MIE_ONLY:
			{
				// Compute Mie only (set Rayleigh to 0)
				vec4 xMieOnly = ComputeAtmosphereScattering(
					xCamPos, xRayDir, xSunDir, 1e10,
					vec3(0.0), fMieCoeff,
					xScaleHeights, g_fMieG,
					g_fPlanetRadius, g_fAtmosphereRadius,
					g_fSunIntensity, g_uSkySamples, g_uLightSamples
				);
				xSkyColor = xMieOnly.rgb;
				break;
			}

			case SKYBOX_DEBUG_TRANSMITTANCE:
			{
				// Visualize transmittance as grayscale
				float fTransmittance = xScatterResult.a;
				xSkyColor = vec3(fTransmittance);
				break;
			}

			case SKYBOX_DEBUG_SCATTER_DIRECTION:
			{
				// Color-code view angle vs sun angle
				float fCosTheta = dot(xRayDir, xSunDir);
				float fAngle = acos(clamp(fCosTheta, -1.0, 1.0)) / 3.14159265;
				xSkyColor = DebugHeatmap(fAngle);
				break;
			}

			case SKYBOX_DEBUG_SUN_DISK:
			{
				// Isolate sun disk
				float fCosAngle = dot(xRayDir, xSunDir);
				float fSunAngle = acos(clamp(fCosAngle, -1.0, 1.0));
				float fNormAngle = fSunAngle / 0.02;  // Scaled for visibility
				xSkyColor = vec3(1.0 - clamp(fNormAngle, 0.0, 1.0));
				break;
			}

			case SKYBOX_DEBUG_RAY_STEPS:
			{
				// Heatmap of sample count (normalized)
				float fNormSteps = float(g_uSkySamples) / 64.0;
				xSkyColor = DebugHeatmap(fNormSteps);
				break;
			}

			case SKYBOX_DEBUG_PHASE_FUNCTION:
			{
				// Visualize phase functions
				float fCosTheta = dot(xRayDir, xSunDir);
				float fRayleighPhase = RayleighPhase(fCosTheta);
				float fMiePhase = MiePhase(fCosTheta, g_fMieG);

				// Red = Rayleigh, Green = Mie
				xSkyColor = vec3(fRayleighPhase * 2.0, fMiePhase * 0.5, 0.0);
				break;
			}
		}
	}

	// Output to G-Buffer
	// Sky has zero normals, high emissive, no material properties
	OutputToGBuffer(
		vec4(xSkyColor, 1.0),  // Diffuse
		vec3(0.0, 0.0, 0.0),   // Normal (zero for sky)
		0.0,                    // Ambient
		0.0,                    // Roughness
		0.0,                    // Metallic
		length(xSkyColor)       // Emissive (luminance for HDR)
	);
}
