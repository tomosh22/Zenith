#version 450 core

layout(location = 0) in vec3 v_xWorldPos;
layout(location = 1) in vec3 v_xNormal;
layout(location = 2) in vec2 v_xUV;
layout(location = 3) in vec4 v_xColor;
layout(location = 4) flat in uint v_uInstanceID;

layout(location = 0) out vec4 o_xColor;

#include "../Common.fxh"

// Grass constants
layout(std140, set = 0, binding = 1) uniform GrassConstants
{
	vec4 g_xWindParams;      // XY = direction, Z = strength, W = time
	vec4 g_xGrassParams;     // X = density scale, Y = max distance, Z = debug mode, W = pad
	vec4 g_xLODDistances;    // LOD0, LOD1, LOD2, MAX distances
};

// Debug mode constants
#define GRASS_DEBUG_NONE 0
#define GRASS_DEBUG_LOD_COLORS 1
#define GRASS_DEBUG_DENSITY_HEAT 3
#define GRASS_DEBUG_WIND_VECTORS 4
#define GRASS_DEBUG_HEIGHT_VARIATION 7

// False-color heatmap for debug visualization
vec3 DebugHeatmap(float fValue)
{
	fValue = clamp(fValue, 0.0, 1.0);
	if (fValue < 0.5)
		return mix(vec3(0, 0, 1), vec3(0, 1, 0), fValue * 2.0);
	else
		return mix(vec3(0, 1, 0), vec3(1, 0, 0), (fValue - 0.5) * 2.0);
}

// LOD level coloring
vec3 LODColor(uint uLOD)
{
	if (uLOD == 0u) return vec3(0, 1, 0);  // Green - LOD0
	if (uLOD == 1u) return vec3(1, 1, 0);  // Yellow - LOD1
	if (uLOD == 2u) return vec3(1, 0.5, 0);  // Orange - LOD2
	return vec3(1, 0, 0);  // Red - LOD3+
}

void main()
{
	uint uDebugMode = uint(g_xGrassParams.z);

	// Alpha test (discard transparent pixels)
	float fAlpha = v_xColor.a;
	if (fAlpha < 0.01)
	{
		discard;
	}

	// Base grass color with variation
	vec3 xBaseColor = vec3(0.2, 0.5, 0.15);  // Dark green

	// Add tip brightening (subsurface scattering approximation)
	float fHeightFactor = v_xUV.y;
	vec3 xTipColor = vec3(0.4, 0.7, 0.25);  // Lighter green
	vec3 xGrassColor = mix(xBaseColor, xTipColor, fHeightFactor * 0.7);

	// Apply color tint from instance data
	xGrassColor *= v_xColor.rgb;

	// Simple lighting
	vec3 xSunDir = normalize(g_xSunDir_Pad.xyz);
	vec3 xSunColor = g_xSunColour.rgb;

	// Wrap lighting for grass (no harsh shadows)
	float fNdotL = dot(v_xNormal, xSunDir) * 0.5 + 0.5;
	fNdotL = fNdotL * 0.8 + 0.2;  // Ambient minimum

	// Translucency - light passing through blade
	float fTranslucency = max(0.0, -dot(normalize(v_xNormal), xSunDir)) * fHeightFactor * 0.5;

	vec3 xLitColor = xGrassColor * xSunColor * (fNdotL + fTranslucency);

	// Add ambient from sky (approximate)
	vec3 xAmbient = vec3(0.3, 0.4, 0.5) * 0.2 * xGrassColor;
	xLitColor += xAmbient;

	// Debug visualizations
	if (uDebugMode != GRASS_DEBUG_NONE)
	{
		switch (uDebugMode)
		{
			case GRASS_DEBUG_LOD_COLORS:
			{
				// Match CPU LOD selection logic exactly (uses squared distance with < for thresholds)
				// LOD0: distSq < LOD0^2, LOD1: distSq < LOD1^2, etc.
				vec3 xToCam = v_xWorldPos - g_xCamPos_Pad.xyz;
				float fDistSq = dot(xToCam, xToCam);
				float fLOD0Sq = g_xLODDistances.x * g_xLODDistances.x;
				float fLOD1Sq = g_xLODDistances.y * g_xLODDistances.y;
				float fLOD2Sq = g_xLODDistances.z * g_xLODDistances.z;
				uint uLOD = 3u;  // Default to LOD3
				if (fDistSq < fLOD2Sq) uLOD = 2u;
				if (fDistSq < fLOD1Sq) uLOD = 1u;
				if (fDistSq < fLOD0Sq) uLOD = 0u;
				xLitColor = LODColor(uLOD);
				break;
			}

			case GRASS_DEBUG_HEIGHT_VARIATION:
			{
				xLitColor = DebugHeatmap(fHeightFactor);
				break;
			}
		}
	}

	o_xColor = vec4(xLitColor, fAlpha);
}
