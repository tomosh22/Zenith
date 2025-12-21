#version 450 core

#include "../Common.fxh"

layout(location = 0) in vec3 a_xPosition;
layout(location = 1) in vec2 a_xUV;
layout(location = 2) in uvec4 a_xInstancePositionSize;
layout(location = 3) in vec4 a_xInstanceColour;
layout(location = 4) in uint a_uTexture;
layout(location = 5) in vec2 a_xUVMult_UVAdd;

layout(location = 0) out vec2 o_xUV;
layout(location = 1) out vec4 o_xColour;
layout(location = 2) flat out uint o_uTexture;

void main()
{
	// Flip V coordinate for Vulkan (Y=-1 is top, so we need V=0 at top)
	vec2 xFlippedUV = vec2(a_xUV.x, 1.0 - a_xUV.y);
	o_xUV = xFlippedUV * a_xUVMult_UVAdd.x + a_xUVMult_UVAdd.y;
	o_xColour = a_xInstanceColour;
	o_uTexture = a_uTexture;

	// Transform quad vertices from [-1,1] to [0,1] range for proper scaling
	vec2 xQuadPos = a_xPosition.xy * 0.5 + 0.5;

	// Calculate pixel position: scale by size, add position offset
	vec2 xPixelPos = xQuadPos * vec2(a_xInstancePositionSize.zw) + vec2(a_xInstancePositionSize.xy);

	// Convert pixel coordinates to Vulkan NDC: x,y in [-1,1] where (-1,-1) is top-left
	vec2 xNDC = xPixelPos * g_xRcpScreenDims * 2.0 - 1.0;
	gl_Position = vec4(xNDC, 0, 1);
}