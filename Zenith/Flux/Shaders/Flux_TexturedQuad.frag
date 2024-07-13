#version 450 core

layout(location = 0) out vec4 o_xColor;

layout(location = 0) in vec2 a_xUV;

layout(set = 0, binding = 0) uniform sampler2D g_xTexture;

void main()
{
	o_xColor = texture(g_xTexture, a_xUV);
}