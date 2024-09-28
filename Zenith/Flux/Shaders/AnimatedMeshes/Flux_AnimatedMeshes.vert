#version 450 core

#include "../Common.fxh"

layout(location = 0) in vec3 a_xPosition;
layout(location = 1) in vec2 a_xUV;
layout(location = 2) in vec3 a_xNormal;
layout(location = 3) in vec3 a_xTangent;
layout(location = 4) in vec3 a_xBitangent;
layout(location = 5) in uvec4 a_xBoneIDs;
layout(location = 6) in vec4 a_xBoneWeights;

layout(location = 0) out vec2 o_xUV;
layout(location = 1) out vec3 o_xNormal;
layout(location = 2) out vec3 o_xWorldPos;
layout(location = 3) out mat3 o_xTBN;

layout(push_constant) uniform ModelMatrix{
	mat4 g_xModelMatrix;
};

#if 0
layout(set = 1, binding = 0) uniform Bones
{
	mat4 g_xBones;
};
#endif

void main()
{

	vec4 xFinalPosition = vec4(0.f);
	for(uint u = 0; u < 4; u++)
	{
		#if 0
		if(a_xBoneIDs[u] == ~0u)
		{
			break;
		}
		
		xFinalPosition += g_xBones[a_xBoneIDs[u]] * vec4(a_xPosition, 1.f) * a_xBoneWeights[u];
		#else
		xFinalPosition += mat4(1.f) * vec4(a_xPosition, 1.f) * 0.25f;
		#endif
	}


	o_xUV = a_xUV;
	mat3 xNormalMatrix = transpose(inverse(mat3(g_xModelMatrix)));
	o_xNormal = normalize(xNormalMatrix * normalize(a_xNormal));
	vec3 xTangent = normalize(xNormalMatrix * normalize(a_xTangent));
	vec3 xBitangent = normalize(xNormalMatrix * normalize(a_xBitangent));
	o_xTBN = mat3(xTangent, xBitangent, o_xNormal);

	o_xWorldPos = (g_xModelMatrix * vec4(xFinalPosition.xyz,1)).xyz;
	gl_Position = g_xViewProjMat * vec4(o_xWorldPos,1);
}