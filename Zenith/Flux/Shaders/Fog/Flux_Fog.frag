#version 450 core

#include "../Common.fxh"

layout(location = 0) out vec4 o_xColour;

layout(location = 0) in vec2 a_xUV;

layout(set = 0, binding = 1) uniform sampler2D g_xDepthTex;

layout(push_constant) uniform FogConstants{
	float g_fFallof;
};

void main()
{
	float fDepth = texture(g_xDepthTex, a_xUV).x;
	
	float fDist = (1. * 5000.) / (5000. - fDepth * (5000. - 1.));
	
	float fFogAmount = 1.0 - exp(-fDist * g_fFallof);
    vec3 xFogColour  = vec3(0.5,0.6,0.7);
	o_xColour = vec4(xFogColour, fFogAmount);
}