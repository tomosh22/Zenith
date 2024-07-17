#version 450 core

#include "Common.h"

layout(location = 0) out vec4 o_xColor;

layout(location = 0) in vec2 a_xUV;

void main()
{
	o_xColor = vec4(a_xUV, 0.f, 1.f);
}