// =====================================================================
// Flux_StaticMesh_ToShadowmap — Slang module.
//
// Subsystem: StaticMeshes. Depth-only pass that writes static mesh
// geometry into a CSM cascade. Vertex shader transforms by the
// per-cascade sun view×proj; fragment is a no-op (depth-only target,
// no colour writes).
//
// Imports Common.DrawConstants for the model matrix. The cascade's
// view×proj is a separate CB at set=1 binding=1 supplied by the C++
// shadow pass.
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

[shader("vertex")]
float4 vsMain(VsIn input) : SV_Position
{
	float3 xWorldPos = mul(DrawConstants.g_xModelMatrix, float4(input.a_xPosition, 1.0)).xyz;
	return mul(ShadowMatrix.g_xSunViewProjMat, float4(xWorldPos, 1.0));
}

[shader("fragment")]
void fsMain()
{
	// Depth-only pass — no colour output.
}
