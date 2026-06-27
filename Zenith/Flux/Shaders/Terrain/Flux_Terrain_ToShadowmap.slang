// =====================================================================
// Flux_Terrain_ToShadowmap — Slang module.
//
// Subsystem: Terrain. Depth-only pass for cascaded shadow maps. Terrain
// vertices are already in world space; only the cascade's view×proj matrix
// is needed. Like every other caster (Phase 5.4), that matrix comes from the
// persistent VIEW set's all-cascade g_xShadowMatrices SSBO, selected by a
// per-pass cascade index held in this shader's own DRAW ParameterBlock (set 3,
// after the persistent 0/1/2 spine the include reserves).
//
// NOTE: terrain does NOT currently cast — Flux_TerrainImpl::RenderToShadowMap
// is stubbed and the call in ExecuteShadowCascade is commented out. This
// shader is deliberately kept on the live SSBO path (mirroring
// Flux_StaticMesh_ToShadowmap) so that enabling casting becomes a pure C++
// change, not a shader rewrite — and so it can never diverge into a stale
// per-CB binding that a future implementer would copy-paste into a bind-fail.
// =====================================================================

#include "Common/Bindings.slang"   // spine: GLOBAL/VIEW/BINDLESS at sets 0/1/2

struct VsIn
{
	float3 a_xPosition       : POSITION;
	float2 a_xUV             : TEXCOORD0;
	float4 a_xNormalPacked   : NORMAL;
	float4 a_xTangentPacked  : TANGENT;
};

// DRAW frequency (set 3, after the persistent spine). The all-cascade sun view×proj
// matrices are now in the persistent VIEW set (g_xViewSet.g_xShadowMatrices, Phase 5.4);
// this caster selects element TerrainShadowConstants.g_uShadowCascade (set per cascade pass).
struct TerrainShadowConstantsLayout { uint g_uShadowCascade; uint3 g_xPad; };
struct DrawParams
{
	ConstantBuffer<TerrainShadowConstantsLayout> TerrainShadowConstants;
};
ParameterBlock<DrawParams> g_xDrawSet;

[shader("vertex")]
float4 vsMain(VsIn input) : SV_Position
{
	return mul(g_xViewSet.g_xShadowMatrices[g_xDrawSet.TerrainShadowConstants.g_uShadowCascade], float4(input.a_xPosition, 1.0));
}

[shader("fragment")]
void fsMain() {}
