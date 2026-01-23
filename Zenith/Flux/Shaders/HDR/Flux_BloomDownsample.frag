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

layout(set = 0, binding = 1) uniform sampler2D g_xSourceTex;

void main()
{
	// 13-tap downsample filter (high quality blur)
	// Uses a tent filter combined with box filter for smooth results
	// This is based on the Call of Duty: Advanced Warfare bloom implementation

	vec2 xUV = a_xUV;
	vec2 xTexelSize = g_xTexelSize;

	// Sample pattern (13 taps total)
	// Center sample
	vec3 xCenter = texture(g_xSourceTex, xUV).rgb;

	// Inner samples (4 taps at 1 texel offset)
	vec3 xInner = vec3(0.0);
	xInner += texture(g_xSourceTex, xUV + vec2(-xTexelSize.x, 0.0)).rgb;
	xInner += texture(g_xSourceTex, xUV + vec2(xTexelSize.x, 0.0)).rgb;
	xInner += texture(g_xSourceTex, xUV + vec2(0.0, -xTexelSize.y)).rgb;
	xInner += texture(g_xSourceTex, xUV + vec2(0.0, xTexelSize.y)).rgb;

	// Corner samples (4 taps at diagonal 1 texel offset)
	vec3 xCorner = vec3(0.0);
	xCorner += texture(g_xSourceTex, xUV + vec2(-xTexelSize.x, -xTexelSize.y)).rgb;
	xCorner += texture(g_xSourceTex, xUV + vec2(xTexelSize.x, -xTexelSize.y)).rgb;
	xCorner += texture(g_xSourceTex, xUV + vec2(-xTexelSize.x, xTexelSize.y)).rgb;
	xCorner += texture(g_xSourceTex, xUV + vec2(xTexelSize.x, xTexelSize.y)).rgb;

	// Outer samples (4 taps at 2 texel offset)
	vec3 xOuter = vec3(0.0);
	xOuter += texture(g_xSourceTex, xUV + vec2(-2.0 * xTexelSize.x, 0.0)).rgb;
	xOuter += texture(g_xSourceTex, xUV + vec2(2.0 * xTexelSize.x, 0.0)).rgb;
	xOuter += texture(g_xSourceTex, xUV + vec2(0.0, -2.0 * xTexelSize.y)).rgb;
	xOuter += texture(g_xSourceTex, xUV + vec2(0.0, 2.0 * xTexelSize.y)).rgb;

	// Weighted combination for smooth falloff (normalized to sum to 1.0)
	// Original weights summed to 1.25 which caused progressive brightening across mip chain.
	// Normalized: Center=0.2, Inner=0.4 (4×0.1), Corner=0.2 (4×0.05), Outer=0.2 (4×0.05)
	vec3 xResult = xCenter * 0.2 + xInner * 0.1 + xCorner * 0.05 + xOuter * 0.05;

	o_xColour = vec4(xResult, 1.0);
}
