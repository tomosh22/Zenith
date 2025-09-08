#version 450 core

#extension GL_EXT_nonuniform_qualifier : enable

#include "../Common.fxh"

layout(location = 0) out vec4 o_xColour;

layout(location = 0) in vec2 a_xUV;
layout(location = 1) in vec4 a_xColour;
layout(location = 2) flat in uint a_uTexture;

layout (set = 1, binding = 0) uniform sampler2D g_axBindlessTextures[];

void main()
{
	o_xColour = texture(g_axBindlessTextures[a_uTexture], a_xUV);
}