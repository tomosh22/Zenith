#version 450 core

#include "../Common.fxh"

layout(location = 0) out vec4 o_xColour;

layout(location = 0) in vec2 a_xUV;
layout(location = 2) in vec3 a_xWorldPos;

layout(set = 1, binding = 0) uniform sampler2D g_xNormalTex;

void main()
{
	vec3 xTangent = 	vec3(1.,0.,0.);
	vec3 xBitangent = 	vec3(0.,0.,1.);
	vec3 xNormal = 		vec3(0.,1.,0.);
	mat3 xTBN = mat3(xTangent, xBitangent, xNormal);
	
	xNormal = xTBN * texture(g_xNormalTex, a_xUV).xyz;
	
	DirectionalLight xLight;
	xLight.m_xColour = g_xSunColour;
	xLight.m_xDirection = vec4(g_xSunDir_Pad.xyz, 0.);
	
	vec3 xDiffuse = vec3(0.2,0.3,0.5);
	
	o_xColour = vec4(xDiffuse * dot(xNormal, xLight.m_xDirection.xyz * -1.),0.5);
}