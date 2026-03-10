#version 450 core

#include "../Common.fxh"

layout(location = 0) out vec4 o_xColour;

layout(location = 0) in vec2 a_xUV;
layout(location = 1) in vec4 a_xColour;

layout(set = 0, binding = 1) uniform sampler2D g_xTexture;

// Overlay clip rect (x1, y1, x2, y2) in pixels. (-1,-1,-1,-1) = disabled.
// Text fragments inside this rect are discarded (occluded by overlay content box).
layout(std140, set = 0, binding = 2) uniform TextClipConstants
{
	vec4 g_xClipRect;
};

void main()
{
	// Discard fragments inside the overlay clip rect
	if (g_xClipRect.x >= 0.0)
	{
		vec2 xPixelPos = gl_FragCoord.xy;
		if (xPixelPos.x >= g_xClipRect.x && xPixelPos.x <= g_xClipRect.z &&
			xPixelPos.y >= g_xClipRect.y && xPixelPos.y <= g_xClipRect.w)
		{
			discard;
		}
	}

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