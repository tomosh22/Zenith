#version 450 core

#include "../Common.fxh"
#include "../GBufferCommon.fxh"

layout(location = 0) in vec2 a_xUV;

layout(set = 0, binding = 1) uniform samplerCube g_xCubemap;

void main()
{
	vec3 xRayDir = RayDir(a_xUV);

	OutputToGBuffer(texture(g_xCubemap, xRayDir), vec3(0.), 0., 0., 0.);
}