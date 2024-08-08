#version 450 core

#include "../Common.fxh"

layout(location = 0) in vec3 a_xPosition;
layout(location = 1) in vec2 a_xUV;
layout(location = 2) in vec3 a_xNormal;
layout(location = 3) in vec3 a_xTangent;
layout(location = 4) in vec3 a_xBitangent;

layout(location = 0) out vec2 o_xUV;
layout(location = 1) out vec3 o_xNormal;
layout(location = 2) out vec3 o_xWorldPos;
layout(location = 3) out mat3 o_xTBN;

layout(push_constant) uniform ModelMatrix{
	mat4 g_xModelMatrix;
};

void main()
{
	o_xUV = a_xUV;
	mat3 xNormalMatrix = transpose(inverse(mat3(g_xModelMatrix)));
	o_xNormal = normalize(xNormalMatrix * normalize(a_xNormal));
	vec3 xTangent = normalize(xNormalMatrix * normalize(a_xTangent));
	vec3 xBitangent = normalize(xNormalMatrix * normalize(a_xBitangent));
	o_xTBN = mat3(xTangent, xBitangent, o_xNormal);
	o_xWorldPos = (g_xModelMatrix * vec4(a_xPosition,1)).xyz;

	gl_Position = g_xViewProjMat * vec4(o_xWorldPos,1);
}