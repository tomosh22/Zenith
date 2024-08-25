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
	o_xUV = a_xUV/10. + a_xInstanceUV;
	
	vec2 xRoot = g_xRcpScreenDims * a_xInstanceTextRoot;
	vec2 xPos = xRoot + a_fInstanceTextSize * g_xRcpScreenDims * (a_xPosition.xy + a_xInstancePosition);

	xPos = xPos * 2.f - 1.f;
	gl_Position = vec4(xPos, 0, 1);
}