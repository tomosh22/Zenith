#version 450 core

#include "../Common.fxh"

layout(location = 0) in vec3 a_xPosition;
layout(location = 1) in vec2 a_xUV;
layout(location = 2) in vec4 a_xInstancePositionRadius;
layout(location = 3) in vec4 a_xInstanceColour;

layout(location = 0) out vec2 o_xUV;
layout(location = 1) out vec4 o_xColour;

void main()
{
	o_xUV = a_xUV;
	o_xColour = a_xInstanceColour;
	
	vec3 xRightDir = normalize(vec3(g_xViewMat[0][0], g_xViewMat[1][0], g_xViewMat[2][0]));
	vec3 xUpDir = normalize(vec3(g_xViewMat[0][1], g_xViewMat[1][1], g_xViewMat[2][1]));
	vec3 xForwardDir = normalize(vec3(g_xViewMat[0][2], g_xViewMat[1][2], g_xViewMat[2][2]));
	
	mat3 xBillboardMatrix = mat3(xRightDir, xUpDir, xForwardDir);
	
	vec3 xRotatedPos = xBillboardMatrix * a_xPosition;
	
	gl_Position = g_xViewProjMat * vec4(xRotatedPos * a_xInstancePositionRadius.w + a_xInstancePositionRadius.xyz, 1);
}