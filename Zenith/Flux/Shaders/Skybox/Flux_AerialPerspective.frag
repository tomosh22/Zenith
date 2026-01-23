#version 450 core

#include "../Common.fxh"
#include "Flux_AtmosphereCommon.fxh"

layout(location = 0) in vec2 a_xUV;

layout(location = 0) out vec4 o_xColor;

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

layout(set = 0, binding = 2) uniform sampler2D g_xDepthTex;

// Debug mode constants (must match Skybox_DebugMode enum)
#define SKYBOX_DEBUG_NONE 0
#define SKYBOX_DEBUG_AERIAL_DEPTH 5

// Reconstruct world position from depth
vec3 GetWorldPosition(vec2 xUV, float fDepth)
{
	vec2 xNDC = xUV * 2.0 - 1.0;
	vec4 xClipSpace = vec4(xNDC, fDepth, 1.0);
	vec4 xViewSpace = g_xInvProjMat * xClipSpace;
	xViewSpace /= xViewSpace.w;
	return (g_xInvViewMat * xViewSpace).xyz;
}

// Simplified aerial perspective calculation
// Blends scene color towards sky color based on distance
vec4 ComputeAerialPerspective(
	vec3 xCamPos,
	vec3 xWorldPos,
	vec3 xSunDir,
	vec3 xRayleighCoeff,
	float fMieCoeff,
	vec2 xScaleHeights,
	float fStrength)
{
	vec3 xRayDir = normalize(xWorldPos - xCamPos);
	float fDistance = length(xWorldPos - xCamPos);

	// Scale distance for more visible effect (world units to km)
	float fScaledDist = fDistance * 0.001 * fStrength;

	// Compute optical depth for the view ray segment
	// Use camera height for density calculation
	// (could be improved to average along ray for large altitude differences)
	float fHeight = max(0.0, xCamPos.y);
	vec2 xDensity = GetDensity(fHeight, xScaleHeights);

	// Optical depth along view ray
	float fRayleighDepth = xDensity.x * fScaledDist * 1000.0;  // Back to meters
	float fMieDepth = xDensity.y * fScaledDist * 1000.0;

	// Transmittance (how much of original color reaches eye)
	vec3 xTransmittance = exp(
		-xRayleighCoeff * fRayleighDepth
		- fMieCoeff * fMieDepth);

	// Inscatter (light scattered towards eye)
	// Use a fraction of full sky scattering
	float fCosTheta = dot(xRayDir, xSunDir);
	float fRayleighPhase = RayleighPhase(fCosTheta);
	float fMiePhase = MiePhase(fCosTheta, 0.76);

	vec3 xInscatter = (xRayleighCoeff * fRayleighPhase + fMieCoeff * fMiePhase)
		* (1.0 - xTransmittance) * g_fSunIntensity * 0.1;

	// Blend factor (how much aerial perspective to apply)
	float fFog = 1.0 - dot(xTransmittance, vec3(0.333));

	return vec4(xInscatter, fFog);
}

void main()
{
	float fDepth = texture(g_xDepthTex, a_xUV).r;

	// Skip sky pixels (at far plane)
	if (fDepth >= 1.0)
	{
		o_xColor = vec4(0.0, 0.0, 0.0, 0.0);
		return;
	}

	vec3 xWorldPos = GetWorldPosition(a_xUV, fDepth);
	vec3 xCamPos = g_xCamPos_Pad.xyz;
	// Note: g_xSunDir_Pad is direction light travels (from sun), negate to get direction TO sun
	// This must match Flux_Atmosphere.frag for consistent lighting
	vec3 xSunDir = normalize(-g_xSunDir_Pad.xyz);

	// Convert camera to atmosphere space (preserve X,Z for correct perspective calculations)
	vec3 xAtmosCamPos = vec3(xCamPos.x, g_fPlanetRadius + xCamPos.y, xCamPos.z);

	// Scaled coefficients
	vec3 xRayleighCoeff = g_xRayleighScatter.rgb * g_fRayleighScale;
	float fMieCoeff = g_xMieScatter.r * g_fMieScale;
	vec2 xScaleHeights = vec2(g_xRayleighScatter.w, g_xMieScatter.w);

	// Compute aerial perspective
	vec4 xAerial = ComputeAerialPerspective(
		xCamPos,
		xWorldPos,
		xSunDir,
		xRayleighCoeff,
		fMieCoeff,
		xScaleHeights,
		g_fAerialStrength
	);

	// Debug visualization
	if (g_uDebugMode == SKYBOX_DEBUG_AERIAL_DEPTH)
	{
		// Visualize depth/distance as heatmap
		float fDistance = length(xWorldPos - xCamPos);
		float fNormDist = clamp(fDistance / 10000.0, 0.0, 1.0);  // 10km range
		vec3 xHeatmap;
		if (fNormDist < 0.5)
			xHeatmap = mix(vec3(0, 0, 1), vec3(0, 1, 0), fNormDist * 2.0);
		else
			xHeatmap = mix(vec3(0, 1, 0), vec3(1, 0, 0), (fNormDist - 0.5) * 2.0);
		o_xColor = vec4(xHeatmap, 0.8);
		return;
	}

	// Output: RGB = inscatter color, A = blend factor
	o_xColor = xAerial;
}
