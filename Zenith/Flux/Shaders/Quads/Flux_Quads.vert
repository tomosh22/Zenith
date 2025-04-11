#version 450 core

#include "../Common.fxh"

layout(location = 0) in vec3 a_xPosition;
layout(location = 1) in vec2 a_xUV;
layout(location = 2) in vec4 a_xInstancePositionSize;
layout(location = 3) in vec4 a_xInstanceColour;

layout(location = 0) out vec2 o_xUV;
layout(location = 1) out vec4 o_xColour;

void main()
{
	o_xUV = a_xUV;
	o_xColour = a_xInstanceColour;
	
	gl_Position = vec4(((a_xPosition.xy * a_xInstancePositionSize.zw) + a_xInstancePositionSize.xy) * g_xRcpScreenDims, 0, 1);
}