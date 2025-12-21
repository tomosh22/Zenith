#version 450 core

#include "../Common.fxh"

layout(location = 0) in vec3 a_xPosition;
layout(location = 1) in vec2 a_xUV;
layout(location = 2) in vec2 a_xInstancePosition;
layout(location = 3) in vec2 a_xInstanceUV;
layout(location = 4) in uvec2 a_xInstanceTextRoot;
layout(location = 5) in float a_fInstanceTextSize;

layout(location = 0) out vec2 o_xUV;

void main()
{
	// Flip V coordinate for Vulkan (Y=-1 is top, so we need V=0 at top)
	o_xUV = vec2(a_xUV.x, 1.0 - a_xUV.y) / 10.0 + a_xInstanceUV;

	// Transform quad from [-1,1] to [0,1] - makes each character a unit square
	vec2 xQuadPos = a_xPosition.xy * 0.5 + 0.5;

	// Calculate pixel position:
	// - Text root (in pixels) + character offset + quad position, all scaled by text size
	vec2 xPixelPos = vec2(a_xInstanceTextRoot) + a_fInstanceTextSize * (a_xInstancePosition + xQuadPos);

	// Convert to Vulkan NDC: x,y in [-1,1] where (-1,-1) is top-left
	vec2 xNDC = xPixelPos * g_xRcpScreenDims * 2.0 - 1.0;

	gl_Position = vec4(xNDC, 0, 1);
}