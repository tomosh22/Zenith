#version 450 core

#include "../Common.fxh"

layout(location = 0) out vec4 o_xColour;

layout(location = 0) in vec2 a_xUV;
layout(location = 1) in vec4 a_xColour;

layout(set = 0, binding = 1) uniform sampler2D g_xTexture;

void main()
{
	vec4 xTexColour = texture(g_xTexture, a_xUV);

	// Discard fully transparent pixels
	if (xTexColour.a < 0.01)
	{
		discard;
	}

	// Apply text color tint (white text * color = colored text)
	// Shadow areas (black in RGB) will show through as dark, text areas (white) take the tint
	o_xColour = vec4(xTexColour.rgb * a_xColour.rgb, xTexColour.a * a_xColour.a);
}