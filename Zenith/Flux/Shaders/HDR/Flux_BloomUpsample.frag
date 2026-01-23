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
	// 9-tap tent filter for high quality upsampling
	// This creates a smooth blur while upscaling

	vec2 xUV = a_xUV;
	vec2 xTexelSize = g_xTexelSize;

	// 3x3 tent filter with bilinear sampling
	vec3 xResult = vec3(0.0);

	// Corner samples (weight: 1/16 each = 4/16 total)
	xResult += texture(g_xSourceTex, xUV + vec2(-xTexelSize.x, -xTexelSize.y)).rgb * 0.0625;
	xResult += texture(g_xSourceTex, xUV + vec2(xTexelSize.x, -xTexelSize.y)).rgb * 0.0625;
	xResult += texture(g_xSourceTex, xUV + vec2(-xTexelSize.x, xTexelSize.y)).rgb * 0.0625;
	xResult += texture(g_xSourceTex, xUV + vec2(xTexelSize.x, xTexelSize.y)).rgb * 0.0625;

	// Edge samples (weight: 2/16 each = 8/16 total)
	xResult += texture(g_xSourceTex, xUV + vec2(-xTexelSize.x, 0.0)).rgb * 0.125;
	xResult += texture(g_xSourceTex, xUV + vec2(xTexelSize.x, 0.0)).rgb * 0.125;
	xResult += texture(g_xSourceTex, xUV + vec2(0.0, -xTexelSize.y)).rgb * 0.125;
	xResult += texture(g_xSourceTex, xUV + vec2(0.0, xTexelSize.y)).rgb * 0.125;

	// Center sample (weight: 4/16)
	xResult += texture(g_xSourceTex, xUV).rgb * 0.25;

	o_xColour = vec4(xResult, 1.0);
}
