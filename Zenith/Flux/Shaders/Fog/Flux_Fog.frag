#version 450 core

#include "../Common.fxh"

layout(location = 0) out vec4 o_xColour;

layout(location = 0) in vec2 a_xUV;

// Scratch buffer for push constants replacement
layout(std140, set = 0, binding = 1) uniform FogConstants{
	vec4 g_xFogColour_Falloff;
};

layout(set = 0, binding = 2) uniform sampler2D g_xDepthTex;

void main()
{
	vec3 xWorldPos = GetWorldPosFromDepthTex(g_xDepthTex, a_xUV);
	
	vec3 xCameraToPixel = xWorldPos.xyz - g_xCamPos_Pad.xyz;
	
	float fDist = length(xCameraToPixel);
	
	//credit Inigo Quilez
	float fFogAmount = 1.0 - exp(-fDist * g_xFogColour_Falloff.w);
	float fSunAmount = max( dot(normalize(xCameraToPixel), -g_xSunDir_Pad.xyz), 0.0 );
    vec3  xFogColour  = mix( g_xFogColour_Falloff.xyz,
                           g_xSunColour.xyz,
                           pow(fSunAmount,8.0) );
	o_xColour = vec4(xFogColour, fFogAmount);
}