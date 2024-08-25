#version 450 core

#include "../Common.fxh"

layout(location = 0) out vec4 o_xColour;

layout(location = 0) in vec2 a_xUV;

layout(set = 0, binding = 1) uniform sampler2D g_xTexture;

void main()
{
	o_xColour = texture(g_xTexture, a_xUV);
}