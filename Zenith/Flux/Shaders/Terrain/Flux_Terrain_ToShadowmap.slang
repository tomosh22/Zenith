// =====================================================================
// Flux_Terrain_ToShadowmap — Slang module.
//
// Subsystem: Terrain. Depth-only pass for cascaded shadow maps. Terrain
// vertices are already in world space; only the cascade's view×proj
// matrix is needed.
// =====================================================================

struct VsIn
{
	float3 a_xPosition       : POSITION;
	float2 a_xUV             : TEXCOORD0;
	float4 a_xNormalPacked   : NORMAL;
	float4 a_xTangentPacked  : TANGENT;
};

struct ShadowMatrixLayout { float4x4 g_xSunViewProjMat; };
[[vk::binding(0, 1)]] ConstantBuffer<ShadowMatrixLayout> ShadowMatrix;

[shader("vertex")]
float4 vsMain(VsIn input) : SV_Position
{
	return mul(ShadowMatrix.g_xSunViewProjMat, float4(input.a_xPosition, 1.0));
}

[shader("fragment")]
void fsMain() {}
