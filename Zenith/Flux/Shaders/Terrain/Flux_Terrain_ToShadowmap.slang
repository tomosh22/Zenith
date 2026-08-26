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

#include "Common/Bindings.slang"       // spine: GLOBAL/VIEW/BINDLESS at sets 0/1/2
#include "Common/VertexFormats.slang"  // [VtxFmt] / [PerInstance] authoring attributes

// The depth-only pass reads only the position, but the VsIn still declares the whole
// terrain vertex — every field's [VtxFmt] tag included — because the layout describes
// the BUFFER, not what this stage happens to consume. Dropping them here would give
// the caster a different stride than the G-buffer pass.
struct VsIn
{
	[VtxFmt("snorm16x4")]       float4 a_xPosition       : POSITION;
	[VtxFmt("unorm16x2")]       float2 a_xUV             : TEXCOORD0;
	[VtxFmt("snorm10_10_10_2")] float4 a_xNormalPacked   : NORMAL;
	[VtxFmt("snorm10_10_10_2")] float4 a_xTangentPacked  : TANGENT;
};

// The caster needs the dequant box for the same reason it needs the layout: a snorm16
// position is not a position without it. Same block, same members as the G-buffer
// programs' — the whole point of keeping this shader live is that enabling terrain
// casting stays a pure C++ change, and a caster that fetched the box from somewhere
// else would shadow a different terrain than the one on screen. RenderToShadowMap is
// still stubbed, so nothing binds set 3 yet; whoever enables it binds
// m_xTerrainConstantsBuffer here exactly as ExecuteGBuffer does.
struct TerrainConstantsLayout
{
	float  g_fUVScale;
	float4 g_xPosQuantScale;   // xyz = this terrain's box extent; w = the UV dequant scale
	float4 g_xPosQuantBias;    // xyz = this terrain's box min
	float4 g_xTerrainDims;     // (chunk world size, world size X, world size Z, unused)
};

struct PassParams
{
	ConstantBuffer<TerrainConstantsLayout> TerrainConstants;
};
ParameterBlock<PassParams> g_xPassSet;

[shader("vertex")]
float4 vsMain(VsIn input) : SV_Position
{
	float3 xWorldPos = Flux_DequantPosition(input.a_xPosition,
		g_xPassSet.TerrainConstants.g_xPosQuantScale.xyz,
		g_xPassSet.TerrainConstants.g_xPosQuantBias.xyz);
	// The cascade pass's own VIEW set: g_xView holds the sun ortho view×proj.
	return mul(GetViewProjMat(), float4(xWorldPos, 1.0));
}

[shader("fragment")]
void fsMain() {}
