#version 450 core

#include "../Common.fxh"

layout(location = 0) in vec3 a_xPosition;
layout(location = 1) in vec2 a_xUV;

layout(location = 0) out vec2 o_xUV;
layout(location = 2) out vec3 o_xWorldPos;

void main()
{
	o_xUV = a_xUV;
	o_xWorldPos = a_xPosition;

	gl_Position = g_xViewProjMat * vec4(o_xWorldPos,1);
}