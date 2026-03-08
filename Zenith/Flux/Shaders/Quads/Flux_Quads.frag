#version 450 core

#extension GL_EXT_nonuniform_qualifier : enable

#include "../Common.fxh"

layout(location = 0) out vec4 o_xColour;

layout(location = 0) in vec2 a_xUV;
layout(location = 1) in vec4 a_xColour;
layout(location = 2) flat in uint a_uTexture;
layout(location = 3) in float a_fCornerRadius;
layout(location = 4) in vec2 a_xSizePixels;
layout(location = 5) in vec4 a_xColour2;

layout (set = 1, binding = 0) uniform sampler2D g_axBindlessTextures[];

void main()
{
	vec4 xBaseColour;

	// Texture ID 0 means solid color (no texture sampling)
	// This avoids requiring a valid texture in the bindless array for simple UI rects
	if (a_uTexture == 0u)
	{
		xBaseColour = a_xColour;
	}
	else
	{
		// For textured quads, multiply texture by vertex color for tinting support
		xBaseColour = texture(g_axBindlessTextures[a_uTexture], a_xUV) * a_xColour;
	}

	// Gradient: blend colours vertically when colour2 sentinel is not set
	if (a_xColour2.x >= 0.0)
	{
		vec4 xTopColour = xBaseColour;
		vec4 xBottomColour = a_xColour2;
		if (a_uTexture != 0u)
		{
			xBottomColour *= texture(g_axBindlessTextures[a_uTexture], a_xUV);
		}
		xBaseColour = mix(xTopColour, xBottomColour, a_xUV.y);
	}

	// Rounded corners: SDF distance field
	if (a_fCornerRadius > 0.0)
	{
		vec2 xPixelPos = (a_xUV - 0.5) * a_xSizePixels;
		vec2 xHalfSize = a_xSizePixels * 0.5;
		vec2 xD = abs(xPixelPos) - (xHalfSize - a_fCornerRadius);
		float fDist = length(max(xD, 0.0)) + min(max(xD.x, xD.y), 0.0) - a_fCornerRadius;
		xBaseColour.a *= 1.0 - smoothstep(-1.0, 0.5, fDist);
		if (xBaseColour.a < 0.01)
		{
			discard;
		}
	}

	o_xColour = xBaseColour;
}
