#version 450 core

layout(location = 0) out vec4 o_xColour;

layout(location = 0) in vec2 a_xUV;

// Scratch buffer for push constants replacement
layout(std140, set = 0, binding = 0) uniform BloomConstants
{
	float g_fThreshold;
	float g_fIntensity;
	vec2 g_xTexelSize;
};

layout(set = 0, binding = 1) uniform sampler2D g_xHDRTex;

// Calculate luminance
float Luminance(vec3 color)
{
	return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

void main()
{
	vec3 xColor = texture(g_xHDRTex, a_xUV).rgb;

	// Extract bright areas using threshold with soft knee
	float fLuminance = Luminance(xColor);

	// Soft knee threshold: smooth transition from 0 to 1 contribution
	// fKneeWidth controls the range over which the transition occurs (half threshold)
	float fKneeWidth = g_fThreshold * 0.5;
	float fContribution;
	if (fKneeWidth > 0.0001)
	{
		fContribution = max(0.0, fLuminance - g_fThreshold + fKneeWidth);
		fContribution = min(fContribution / fKneeWidth, 1.0);
		fContribution = fContribution * fContribution * (3.0 - 2.0 * fContribution); // Smoothstep
	}
	else
	{
		// When threshold is near zero, use simple step function
		fContribution = fLuminance > 0.0 ? 1.0 : 0.0;
	}

	// Apply threshold
	vec3 xBright = xColor * fContribution;

	o_xColour = vec4(xBright, 1.0);
}
