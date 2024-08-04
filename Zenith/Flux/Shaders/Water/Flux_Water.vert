#version 450 core

#include "../Common.fxh"

layout(location = 0) in vec3 a_xPosition;
layout(location = 1) in vec2 a_xUV;

layout(location = 0) out vec2 o_xUV;
layout(location = 1) out vec3 o_xNormal;
layout(location = 2) out vec3 o_xWorldPos;

void main()
{
	o_xUV = a_xUV;
	vec3 xTangent = 	vec3(1.,0.,0.);
	vec3 xBitangent = 	vec3(0.,0.,1.);
	vec3 xNormal = 		vec3(0.,1.,0.);
	mat3 xTBN = mat3(xTangent, xBitangent, xNormal);
	
	o_xNormal = xTBN * vec3(0.,0.,1.);
	o_xWorldPos = a_xPosition;

	gl_Position = g_xViewProjMat * vec4(o_xWorldPos,1);
}