#version 450 core

#include "../Common.fxh"

layout(location = 0) out vec4 o_xColour;

layout(location = 0) in vec2 a_xUV;
layout(location = 1) in vec4 a_xColour;

layout(set = 0, binding = 1) uniform sampler2D g_xTexture;

void main()
{
	vec4 xTexColour = texture(g_xTexture, a_xUV);

	// BC1 compression doesn't preserve alpha - transparent areas become black
	// Discard nearly black pixels to simulate transparency
	float fLuminance = dot(xTexColour.rgb, vec3(0.299, 0.587, 0.114));
	if (fLuminance < 0.1)
	{
		discard;
	}

	// Apply text color tint (white text * color = colored text)
	o_xColour = vec4(xTexColour.rgb * a_xColour.rgb, a_xColour.a);
}