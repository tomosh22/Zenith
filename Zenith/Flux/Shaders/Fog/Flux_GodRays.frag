#version 450 core

// Flux_GodRays.frag - Screen-space volumetric light shafts
// Radial blur from light source position with depth-based occlusion
//
// ========== ARTISTIC PARAMETERS (NOT PHYSICALLY-BASED) ==========
// This god rays effect is an ARTISTIC technique, not a physical simulation.
//
// Physical light scattering in participating media would use Beer-Lambert extinction:
//   transmittance = exp(-density * distance)
// combined with proper phase functions (Henyey-Greenstein, Mie scattering).
//
// Instead, this shader uses an exponential DECAY factor for the dramatic
// "crepuscular rays" look artists expect from games and films:
//   rayIntensity *= pow(decay, sampleIndex)
//
// DECAY VALUE GUIDE:
// +-----------+----------------------------------------------------+
// | Value     | Effect                                             |
// +-----------+----------------------------------------------------+
// | 0.95-0.99 | Very long rays (dramatic sunrise/sunset, churches) |
// | 0.85-0.95 | Medium rays (typical outdoor scenes)               |
// | 0.70-0.85 | Short rays (subtle god ray effect)                 |
// | < 0.70    | Very short (may not be visible)                    |
// +-----------+----------------------------------------------------+
//
// For physically-based volumetric light shafts, consider using the
// Froxel or Raymarch fog techniques with volumetric shadow sampling.
// Those use proper Beer-Lambert extinction and phase functions.

#include "../Common.fxh"

layout(location = 0) out vec4 o_xColour;
layout(location = 0) in vec2 a_xUV;

// Scratch buffer for push constants replacement
layout(std140, set = 0, binding = 1) uniform GodRaysConstants
{
	vec4 g_xLightScreenPos_Pad;  // xy = light screen pos (0-1)
	vec4 g_xParams;              // x = decay, y = exposure, z = density, w = weight
	uint g_uNumSamples;
	uint g_uDebugMode;
	float g_fPad0;
	float g_fPad1;
};

// Bindings
layout(set = 0, binding = 2) uniform sampler2D g_xDepthTex;

// Debug modes for god rays
const uint VOLFOG_DEBUG_GODRAYS_LIGHT_MASK = 21;
const uint VOLFOG_DEBUG_GODRAYS_OCCLUSION = 22;
const uint VOLFOG_DEBUG_GODRAYS_RADIAL_WEIGHTS = 23;

void main()
{
	vec2 lightScreenPos = g_xLightScreenPos_Pad.xy;
	float decay = g_xParams.x;
	float exposure = g_xParams.y;
	float density = g_xParams.z;
	float weight = g_xParams.w;

	// Calculate ray from pixel to light source
	vec2 deltaTexCoord = (a_xUV - lightScreenPos);

	// Normalize delta and scale by density and number of samples
	float distToLight = length(deltaTexCoord);
	deltaTexCoord *= 1.0 / float(g_uNumSamples) * density;

	// Current sample position
	vec2 texCoord = a_xUV;

	// Accumulated color and illumination decay
	vec3 color = vec3(0.0);
	float illuminationDecay = 1.0;

	// Sample depth at current pixel for reference
	float currentDepth = texture(g_xDepthTex, a_xUV).r;

	// Debug: Light mask mode
	if (g_uDebugMode == VOLFOG_DEBUG_GODRAYS_LIGHT_MASK)
	{
		// Show distance to light source
		float dist = length(a_xUV - lightScreenPos);
		o_xColour = vec4(vec3(1.0 - clamp(dist, 0.0, 1.0)), 1.0);
		return;
	}

	// Debug: Occlusion mode
	if (g_uDebugMode == VOLFOG_DEBUG_GODRAYS_OCCLUSION)
	{
		// Show occlusion pattern
		float totalOcclusion = 0.0;
		vec2 sampleCoord = a_xUV;
		for (uint i = 0; i < g_uNumSamples; i++)
		{
			float sampleDepth = texture(g_xDepthTex, sampleCoord).r;
			// Check if sample is occluded (in front of far plane)
			totalOcclusion += sampleDepth < 0.9999 ? 0.0 : 1.0;
			sampleCoord -= deltaTexCoord;
		}
		totalOcclusion /= float(g_uNumSamples);
		o_xColour = vec4(vec3(totalOcclusion), 1.0);
		return;
	}

	// Debug: Radial weights mode
	if (g_uDebugMode == VOLFOG_DEBUG_GODRAYS_RADIAL_WEIGHTS)
	{
		// Visualize decay weights along ray
		float decayVis = pow(decay, distToLight * float(g_uNumSamples));
		o_xColour = vec4(decayVis, decayVis * 0.5, 0.0, 1.0);
		return;
	}

	// Main god rays computation
	for (uint i = 0; i < g_uNumSamples; i++)
	{
		// Move sample position toward light
		texCoord -= deltaTexCoord;

		// Clamp to valid texture coordinates
		vec2 sampleCoord = clamp(texCoord, vec2(0.001), vec2(0.999));

		// Sample depth at this position
		float sampleDepth = texture(g_xDepthTex, sampleCoord).r;

		// Only accumulate if this sample hits the sky (far depth)
		// This creates light shafts where sky is visible
		float occlusion = sampleDepth > 0.9999 ? 1.0 : 0.0;

		// Use sun color for light contribution
		vec3 lightColor = g_xSunColour.rgb * occlusion;

		// Accumulate weighted sample
		color += lightColor * illuminationDecay * weight;

		// Update illumination decay
		illuminationDecay *= decay;
	}

	// Apply exposure
	color *= exposure;

	// Fade out effect near light source to avoid artifacts
	float fadeFactor = smoothstep(0.0, 0.1, distToLight);
	color *= fadeFactor;

	// Also fade if light is off-screen
	float offScreenFade = 1.0;
	if (lightScreenPos.x < -0.5 || lightScreenPos.x > 1.5 ||
		lightScreenPos.y < -0.5 || lightScreenPos.y > 1.5)
	{
		offScreenFade = 0.0;
	}
	else if (lightScreenPos.x < 0.0 || lightScreenPos.x > 1.0 ||
			 lightScreenPos.y < 0.0 || lightScreenPos.y > 1.0)
	{
		// Partially off-screen - fade based on distance
		vec2 clampedPos = clamp(lightScreenPos, vec2(0.0), vec2(1.0));
		float edgeDist = length(lightScreenPos - clampedPos);
		offScreenFade = 1.0 - clamp(edgeDist * 2.0, 0.0, 1.0);
	}
	color *= offScreenFade;

	o_xColour = vec4(color, 0.0);
}
