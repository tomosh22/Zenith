// =====================================================================
// Flux_AnimatedMesh_ToShadowmap — Slang module.
//
// Subsystem: AnimatedMeshes. Depth-only pass for skeletal animation.
// Same bone-weighted position accumulation as the G-buffer path, but
// only the final clip-space position is needed.
//
// Shadow matrix lives at set=1 binding=2 because the Bones UBO occupies
// binding 1 — matches the legacy Mesh_Vert.fxh layout.
// =====================================================================

import Common.DrawConstants;

struct VsIn
{
	float3 a_xPosition    : POSITION;
	float2 a_xUV          : TEXCOORD0;
	float3 a_xNormal      : NORMAL;
	float3 a_xTangent     : TANGENT;
	float3 a_xBitangent   : BINORMAL;
	float4 a_xColor       : COLOR;
	uint4  a_xBoneIDs     : TEXCOORD1;
	float4 a_xBoneWeights : TEXCOORD2;
};

struct BonesLayout      { float4x4 g_xBones[100]; };
struct ShadowMatrixLayout { float4x4 g_xSunViewProjMat; };
[[vk::binding(1, 1)]] ConstantBuffer<BonesLayout>        Bones;
[[vk::binding(2, 1)]] ConstantBuffer<ShadowMatrixLayout> ShadowMatrix;

[shader("vertex")]
float4 vsMain(VsIn input) : SV_Position
{
	float4 xFinalPosition = float4(0.0, 0.0, 0.0, 0.0);
	for (uint u = 0; u < 4; u++)
	{
		if (input.a_xBoneIDs[u] == 0xFFFFFFFFu) break;
		float4x4 xBone = Bones.g_xBones[input.a_xBoneIDs[u]];
		xFinalPosition += mul(xBone, float4(input.a_xPosition, 1.0)) * input.a_xBoneWeights[u];
	}

	float3 xWorldPos = mul(DrawConstants.g_xModelMatrix, xFinalPosition).xyz;
	return mul(ShadowMatrix.g_xSunViewProjMat, float4(xWorldPos, 1.0));
}

[shader("fragment")]
void fsMain() {}
