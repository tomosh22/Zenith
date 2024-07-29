#version 450 core

#include "Common.h"

layout(location = 0) in vec3 a_xPosition;
layout(location = 1) in vec2 a_xUV;
layout(location = 2) in vec3 a_xNormal;
layout(location = 3) in vec3 a_xTangent;
layout(location = 4) in vec3 a_xBitangent;
layout(location = 5) in float a_fMaterialLerp;

layout(location = 0) out vec2 o_xUV;
layout(location = 1) out vec3 o_xNormal;
layout(location = 2) out vec3 o_xWorldPos;
layout(location = 3) out float o_fMaterialLerp;
layout(location = 4) out mat3 o_xTBN;

void main()
{
	o_xUV = a_xUV;
	o_xNormal = a_xNormal;
	vec3 xTangent = a_xTangent;
	vec3 xBitangent = cross(xTangent, o_xNormal);
	o_xTBN = mat3(o_xNormal, xTangent, xBitangent);
	o_xWorldPos = a_xPosition;
	o_fMaterialLerp = a_fMaterialLerp;

	gl_Position = g_xViewProjMat * vec4(o_xWorldPos,1);
}