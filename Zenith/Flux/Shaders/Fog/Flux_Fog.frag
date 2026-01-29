#version 450 core

#include "../Common.fxh"
#include "Flux_VolumetricCommon.fxh"

layout(location = 0) out vec4 o_xColour;

layout(location = 0) in vec2 a_xUV;

// Scratch buffer for push constants replacement
layout(std140, set = 0, binding = 1) uniform FogConstants{
	vec4 g_xFogColour_Falloff;
	float g_fPhaseG;  // Henyey-Greenstein asymmetry parameter
	float _pad0;
	float _pad1;
	float _pad2;
};

layout(set = 0, binding = 2) uniform sampler2D g_xDepthTex;

void main()
{
	vec3 xWorldPos = GetWorldPosFromDepthTex(g_xDepthTex, a_xUV);

	vec3 xCameraToPixel = xWorldPos.xyz - g_xCamPos_Pad.xyz;

	float fDist = length(xCameraToPixel);

	// Beer-Lambert extinction (unchanged)
	float fFogAmount = 1.0 - exp(-fDist * g_xFogColour_Falloff.w);

	// PHYSICALLY-BASED SUN SCATTERING using Henyey-Greenstein phase function
	// Replaces the original pow(cosTheta, 8.0) which was an ad-hoc approximation.
	//
	// Henyey-Greenstein phase function models how particles scatter light:
	//   g = 0.0: isotropic (equal scatter all directions)
	//   g = 0.8: strong forward scatter (typical for atmospheric haze/fog)
	//   g = 0.95: very strong forward scatter (Mie scattering, water droplets)
	//
	// g_fPhaseG is exposed as runtime debug variable: Render/Fog/Phase G

	vec3 xViewDir = normalize(xCameraToPixel);
	float fCosTheta = dot(xViewDir, -g_xSunDir_Pad.xyz);

	// Compute phase function value
	float fPhase = HenyeyGreenstein(fCosTheta, g_fPhaseG);

	// Normalize to [0,1] for color mixing
	// HG maximum occurs at cosTheta = 1.0 (looking directly at sun)
	float fPhaseMax = HenyeyGreenstein(1.0, g_fPhaseG);
	float fPhaseNormalized = fPhase / fPhaseMax;

	vec3 xFogColour = mix(g_xFogColour_Falloff.xyz, g_xSunColour.xyz, fPhaseNormalized);
	o_xColour = vec4(xFogColour, fFogAmount);
}