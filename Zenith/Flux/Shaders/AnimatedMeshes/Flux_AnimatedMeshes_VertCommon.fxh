#include "../Common.fxh"

layout(location = 0) in vec3 a_xPosition;
layout(location = 1) in vec2 a_xUV;
layout(location = 2) in vec3 a_xNormal;
layout(location = 3) in vec3 a_xTangent;
layout(location = 4) in vec3 a_xBitangent;
layout(location = 5) in vec4 a_xColor;
layout(location = 6) in uvec4 a_xBoneIDs;
layout(location = 7) in vec4 a_xBoneWeights;

layout(location = 0) out vec2 o_xUV;
layout(location = 1) out vec3 o_xNormal;
layout(location = 2) out vec3 o_xWorldPos;
layout(location = 3) out mat3 o_xTBN;
layout(location = 6) out vec4 o_xColor;

// Material Constants (128 bytes, scratch buffer at binding 1, replaces push constants)
// Layout matches C++ MaterialPushConstants structure
layout(std140, set = 0, binding = 1) uniform PushConstants{
	mat4 g_xModelMatrix;       // 64 bytes - Model transform matrix
	vec4 g_xBaseColor;         // 16 bytes - RGBA base color multiplier
	vec4 g_xMaterialParams;    // 16 bytes - (metallic, roughness, alphaCutoff, occlusionStrength)
	vec4 g_xUVParams;          // 16 bytes - (tilingX, tilingY, offsetX, offsetY)
	vec4 g_xEmissiveParams;    // 16 bytes - (R, G, B, intensity)
};  // Total: 128 bytes

layout(set = 1, binding = 0) uniform Bones
{
	mat4 g_xBones[100];
};

#ifdef SHADOWS
layout(std140, set = 1, binding=1) uniform ShadowMatrix{
	mat4 g_xSunViewProjMat;
};
#endif

void main()
{
	vec4 xFinalPosition = vec4(0.f);
	vec3 xFinalNormal = vec3(0.f);
	vec3 xFinalTangent = vec3(0.f);
	vec3 xFinalBitangent = vec3(0.f);
	
	for(uint u = 0; u < 4; u++)
	{
		if(a_xBoneIDs[u] == ~0u)
		{
			break;
		}
		
		mat4 xBoneTransform = g_xBones[a_xBoneIDs[u]];
		float fWeight = a_xBoneWeights[u];
		
		xFinalPosition += xBoneTransform * vec4(a_xPosition, 1.f) * fWeight;
		xFinalNormal += mat3(xBoneTransform) * a_xNormal * fWeight;
		xFinalTangent += mat3(xBoneTransform) * a_xTangent * fWeight;
		xFinalBitangent += mat3(xBoneTransform) * a_xBitangent * fWeight;
	}
	
	o_xUV = a_xUV;
	o_xColor = a_xColor;
	mat3 xNormalMatrix = transpose(inverse(mat3(g_xModelMatrix)));
	o_xNormal = normalize(xNormalMatrix * normalize(xFinalNormal));
	vec3 xTangent = normalize(xNormalMatrix * normalize(xFinalTangent));
	vec3 xBitangent = normalize(xNormalMatrix * normalize(xFinalBitangent));
	o_xTBN = mat3(xTangent, xBitangent, o_xNormal);

	o_xWorldPos = (g_xModelMatrix * xFinalPosition).xyz;

	#ifdef SHADOWS
	gl_Position = g_xSunViewProjMat * vec4(o_xWorldPos, 1);
	#else
	gl_Position = g_xViewProjMat * vec4(o_xWorldPos, 1);
	#endif
}
