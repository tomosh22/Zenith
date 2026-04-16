#ifndef MESH_VERT_FXH
#define MESH_VERT_FXH

// ============================================================================
// Unified mesh vertex shader
// Define ONE of: MESH_STATIC (default), MESH_ANIMATED, MESH_INSTANCED
// Optionally define SHADOWS for shadow-map depth-only output.
// ============================================================================

#include "../Common.fxh"

// --- Vertex attributes (common) ---
layout(location = 0) in vec3 a_xPosition;
layout(location = 1) in vec2 a_xUV;
layout(location = 2) in vec3 a_xNormal;
layout(location = 3) in vec3 a_xTangent;
layout(location = 4) in vec3 a_xBitangent;
layout(location = 5) in vec4 a_xColor;

#ifdef MESH_ANIMATED
layout(location = 6) in uvec4 a_xBoneIDs;
layout(location = 7) in vec4 a_xBoneWeights;
#endif

// --- Varyings (GBuffer pass only) ---
#ifndef SHADOWS
layout(location = 0) out vec2 o_xUV;
layout(location = 1) out vec3 o_xNormal;
layout(location = 2) out vec3 o_xWorldPos;

#ifdef MESH_ANIMATED
// Animated meshes output a full TBN matrix (bone-weighted tangent frame)
layout(location = 3) out mat3 o_xTBN;  // consumes locations 3, 4, 5
layout(location = 6) out vec4 o_xColor;
#else
// Static and instanced meshes output tangent + bitangent sign (reconstructed in fragment)
layout(location = 3) out vec3 o_xTangent;
layout(location = 4) out float o_fBitangentSign;
layout(location = 5) out vec4 o_xColor;
#ifdef MESH_INSTANCED
layout(location = 6) out vec4 o_xInstanceTint;
#endif
#endif // MESH_ANIMATED

#endif // SHADOWS

// --- Uniform bindings ---
#include "../DrawConstants.fxh"

#ifdef MESH_INSTANCED
// Reinterpret the 5th DrawConstants field as VAT animation texture parameters
#define g_xAnimTexParams g_xEmissiveParams
#endif

#ifdef MESH_ANIMATED
layout(set = 1, binding = 1) uniform Bones
{
	mat4 g_xBones[100];
};
#endif

#ifdef SHADOWS
// Shadow matrix — binding offset by 1 when Bones buffer is present
#ifdef MESH_ANIMATED
layout(std140, set = 1, binding = 2) uniform ShadowMatrix { mat4 g_xSunViewProjMat; };
#else
layout(std140, set = 1, binding = 1) uniform ShadowMatrix { mat4 g_xSunViewProjMat; };
#endif
#endif

// --- Instanced mesh resources ---
#ifdef MESH_INSTANCED
layout(std430, set = 1, binding = 6) readonly buffer TransformBuffer {
	mat4 g_axTransforms[];
};

layout(std430, set = 1, binding = 7) readonly buffer AnimDataBuffer {
	uvec4 g_axAnimData[];
};

layout(std430, set = 1, binding = 8) readonly buffer VisibleIndexBuffer {
	uint g_auVisibleIndices[];
};

layout(set = 1, binding = 9) uniform sampler2D g_xAnimationTex;

vec3 SampleVAT(uint uVertexID, uint uAnimIndex, float fAnimTime, uint uFrameCount, uint uTextureWidth, uint uTextureHeight)
{
	float fFrame = fAnimTime * float(uFrameCount - 1);
	uint uFrame = uint(fFrame);
	float fBlend = fract(fFrame);

	uFrame = min(uFrame, uFrameCount - 1);
	uint uNextFrame = min(uFrame + 1, uFrameCount - 1);

	uint uRow0 = uAnimIndex * uFrameCount + uFrame;
	uint uRow1 = uAnimIndex * uFrameCount + uNextFrame;

	vec2 xUV0 = vec2((float(uVertexID) + 0.5) / float(uTextureWidth), (float(uRow0) + 0.5) / float(uTextureHeight));
	vec2 xUV1 = vec2((float(uVertexID) + 0.5) / float(uTextureWidth), (float(uRow1) + 0.5) / float(uTextureHeight));

	vec3 xPos0 = texture(g_xAnimationTex, xUV0).xyz;
	vec3 xPos1 = texture(g_xAnimationTex, xUV1).xyz;

	return mix(xPos0, xPos1, fBlend);
}

vec4 UnpackRGBA8(uint uPacked)
{
	return vec4(
		float((uPacked >> 0)  & 0xFF) / 255.0,
		float((uPacked >> 8)  & 0xFF) / 255.0,
		float((uPacked >> 16) & 0xFF) / 255.0,
		float((uPacked >> 24) & 0xFF) / 255.0
	);
}
#endif // MESH_INSTANCED

// ============================================================================
// main()
// ============================================================================
void main()
{
	// --- Compute local-space position and (optionally) tangent frame ---

#ifdef MESH_ANIMATED
	// Bone-weighted position accumulation
	vec4 xFinalPosition = vec4(0.f);
	#ifndef SHADOWS
	vec3 xFinalNormal = vec3(0.f);
	vec3 xFinalTangent = vec3(0.f);
	vec3 xFinalBitangent = vec3(0.f);
	#endif

	for (uint u = 0; u < 4; u++)
	{
		if (a_xBoneIDs[u] == ~0u) break;

		mat4 xBoneTransform = g_xBones[a_xBoneIDs[u]];
		float fWeight = a_xBoneWeights[u];

		xFinalPosition += xBoneTransform * vec4(a_xPosition, 1.f) * fWeight;
		#ifndef SHADOWS
		xFinalNormal += mat3(xBoneTransform) * a_xNormal * fWeight;
		xFinalTangent += mat3(xBoneTransform) * a_xTangent * fWeight;
		xFinalBitangent += mat3(xBoneTransform) * a_xBitangent * fWeight;
		#endif
	}

	vec3 xWorldPos = (g_xModelMatrix * xFinalPosition).xyz;

#elif defined(MESH_INSTANCED)
	// Per-instance transform from SSBO
	uint uInstanceID = g_auVisibleIndices[gl_InstanceIndex];
	mat4 xModelMatrix = g_axTransforms[uInstanceID];

	uvec4 xAnimData = g_axAnimData[uInstanceID];
	vec4 xInstanceTint = UnpackRGBA8(xAnimData.z);

	vec3 xLocalPos = a_xPosition;
	bool bUseVAT = (g_xAnimTexParams.z > 0.0) && ((xAnimData.w & 2u) != 0u);
	if (bUseVAT)
	{
		uint uAnimIndex = (xAnimData.x >> 16) & 0xFFFFu;
		uint uFrameCount = xAnimData.x & 0xFFFFu;
		float fAnimTime = uintBitsToFloat(xAnimData.y);
		uint uTexWidth = uint(g_xAnimTexParams.x);
		uint uTexHeight = uint(g_xAnimTexParams.y);
		xLocalPos = SampleVAT(gl_VertexIndex, uAnimIndex, fAnimTime, uFrameCount, uTexWidth, uTexHeight);
	}

	vec3 xWorldPos = (xModelMatrix * vec4(xLocalPos, 1.0)).xyz;

#else // MESH_STATIC (default)
	vec3 xWorldPos = (g_xModelMatrix * vec4(a_xPosition, 1)).xyz;
#endif

	// --- Output ---

#ifdef SHADOWS
	gl_Position = g_xSunViewProjMat * vec4(xWorldPos, 1);
#else

	o_xUV = a_xUV;
	o_xColor = a_xColor;

	// Compute normal matrix
#ifdef MESH_INSTANCED
	mat3 xNormalMatrix = transpose(inverse(mat3(xModelMatrix)));
#else
	mat3 xNormalMatrix = transpose(inverse(mat3(g_xModelMatrix)));
#endif

#ifdef MESH_ANIMATED
	// Animated: output full TBN from bone-weighted vectors
	o_xNormal = normalize(xNormalMatrix * normalize(xFinalNormal));
	vec3 xTangent = normalize(xNormalMatrix * normalize(xFinalTangent));
	vec3 xBitangent = normalize(xNormalMatrix * normalize(xFinalBitangent));
	o_xTBN = mat3(xTangent, xBitangent, o_xNormal);
#else
	// Static / Instanced: output tangent + bitangent sign for reconstruction in fragment
	o_xNormal = normalize(xNormalMatrix * a_xNormal);
	o_xTangent = normalize(xNormalMatrix * a_xTangent);
	vec3 xBitangent = normalize(xNormalMatrix * a_xBitangent);
	o_fBitangentSign = sign(dot(xBitangent, cross(o_xNormal, o_xTangent)));
#ifdef MESH_INSTANCED
	o_xInstanceTint = xInstanceTint;
#endif
#endif

	o_xWorldPos = xWorldPos;
	gl_Position = g_xViewProjMat * vec4(xWorldPos, 1);
#endif // SHADOWS
}

#endif // MESH_VERT_FXH
