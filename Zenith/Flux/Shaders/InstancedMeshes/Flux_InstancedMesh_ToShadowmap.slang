// =====================================================================
// Flux_InstancedMesh_ToShadowmap — Slang module.
//
// Subsystem: InstancedMeshes. Depth-only pass for instanced meshes.
// Per-instance transform from the same TransformBuffer as the G-buffer
// pass; VAT animation skipped (shadows of static-pose geometry are
// usually visually adequate, and skipping VAT cuts shadow-pass cost).
//
// Bindings:
//   set=1 binding=0  DrawConstants
//   set=1 binding=1  ShadowMatrix (no Bones UBO so binding stays at 1)
//   set=1 binding=6  TransformBuffer
//   set=1 binding=8  VisibleIndexBuffer
// =====================================================================

import Common.DrawConstants;

struct VsIn
{
	float3 a_xPosition  : POSITION;
	float2 a_xUV        : TEXCOORD0;
	float3 a_xNormal    : NORMAL;
	float3 a_xTangent   : TANGENT;
	float3 a_xBitangent : BINORMAL;
	float4 a_xColor     : COLOR;
};

struct ShadowMatrixLayout { float4x4 g_xSunViewProjMat; };
[[vk::binding(1, 1)]] ConstantBuffer<ShadowMatrixLayout> ShadowMatrix;

// Read-only in the graphics pipeline (see Flux_InstancedMesh_ToGBuffer
// for the vertexPipelineStoresAndAtomics rationale).
[[vk::binding(6, 1)]] StructuredBuffer<float4x4> TransformBuffer;
[[vk::binding(8, 1)]] StructuredBuffer<uint>     VisibleIndexBuffer;

[shader("vertex")]
float4 vsMain(VsIn input, uint instanceId : SV_InstanceID) : SV_Position
{
	uint     uInstanceID = VisibleIndexBuffer[instanceId];
	float4x4 xModelMatrix = TransformBuffer[uInstanceID];
	float3   xWorldPos    = mul(xModelMatrix, float4(input.a_xPosition, 1.0)).xyz;
	return mul(ShadowMatrix.g_xSunViewProjMat, float4(xWorldPos, 1.0));
}

[shader("fragment")]
void fsMain() {}
