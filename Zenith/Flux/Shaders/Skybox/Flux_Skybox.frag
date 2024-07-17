#version 450 core

#include "Common.h"

layout(location = 0) out vec4 o_xColour;

layout(location = 0) in vec2 a_xUV;

vec3 RayDir(vec2 pixel)
{
	vec2 xNDC = pixel * 2. - 1.;

	vec4 xClipSpace = vec4(xNDC, 1., 1.);

	//#TO_TODO: invert this CPU side
	vec4 xViewSpace = inverse(g_xProjMat) * xClipSpace;
	xViewSpace.w = 0.;

	//#TO_TODO: same here
	vec3 xWorldSpace = (inverse(g_xViewMat) * xViewSpace).xyz;

	return normalize(xWorldSpace);
}


void main()
{
	vec3 xRayDir = RayDir(a_xUV);
	o_xColour = vec4(0.2, 0.3, 0.9, 1.);
	o_xColour += max(xRayDir.y + 0.3,0.);
}