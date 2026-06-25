// =====================================================================
// Flux_Terrain_ToShadowmap — Slang module.
//
// Subsystem: Terrain. Depth-only pass for cascaded shadow maps. Terrain
// vertices are already in world space; only the cascade's view×proj
// matrix is needed — that per-draw cascade matrix lives in PassParams (set 3),
// not the camera VIEW set. The spine include reserves sets 0/1/2 (unused here)
// so PassParams lands at set 3, matching the persistent layout.
// =====================================================================

#include "Common/Bindings.slang"   // spine: GLOBAL/VIEW/BINDLESS at sets 0/1/2

struct VsIn
{
	float3 a_xPosition       : POSITION;
	float2 a_xUV             : TEXCOORD0;
	float4 a_xNormalPacked   : NORMAL;
	float4 a_xTangentPacked  : TANGENT;
};

struct ShadowMatrixLayout { float4x4 g_xSunViewProjMat; };
struct PassParams
{
	ConstantBuffer<ShadowMatrixLayout> ShadowMatrix;
};
ParameterBlock<PassParams> g_xPassSet;

[shader("vertex")]
float4 vsMain(VsIn input) : SV_Position
{
	return mul(g_xPassSet.ShadowMatrix.g_xSunViewProjMat, float4(input.a_xPosition, 1.0));
}

[shader("fragment")]
void fsMain() {}
