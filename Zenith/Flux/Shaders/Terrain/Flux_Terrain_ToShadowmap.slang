// =====================================================================
// Flux_Terrain_ToShadowmap — Slang module.
//
// Subsystem: Terrain. Depth-only pass for cascaded shadow maps. Terrain
// vertices are already in world space; only the cascade's view×proj matrix
// is needed. Like every other caster, that matrix comes from the cascade
// pass's own per-view VIEW set (g_xViewSet.g_xView — each cascade is a
// render view whose g_xView holds the sun ortho matrices; see
// Flux/RenderViews/Flux_RenderViews.h). g_xShadowMatrices stays receivers-only.
//
// NOTE: terrain does NOT currently cast — Flux_TerrainImpl::RenderToShadowMap
// is stubbed and the call in ExecuteShadowCascade is commented out. This
// shader is deliberately kept on the live per-view convention (mirroring
// Flux_UnifiedMesh_ToShadowmap) so that enabling casting becomes a pure C++
// change, not a shader rewrite — and so it can never diverge into a stale
// binding that a future implementer would copy-paste into a bind-fail.
// =====================================================================

#include "Common/Bindings.slang"   // spine: GLOBAL/VIEW/BINDLESS at sets 0/1/2

struct VsIn
{
	float3 a_xPosition       : POSITION;
	float2 a_xUV             : TEXCOORD0;
	float4 a_xNormalPacked   : NORMAL;
	float4 a_xTangentPacked  : TANGENT;
};

[shader("vertex")]
float4 vsMain(VsIn input) : SV_Position
{
	// The cascade pass's own VIEW set: g_xView holds the sun ortho view×proj.
	return mul(g_xViewSet.g_xView.g_xViewProjMat, float4(input.a_xPosition, 1.0));
}

[shader("fragment")]
void fsMain() {}
