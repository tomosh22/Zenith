// =====================================================================
// Flux_Grass_ToShadowmap — Slang module (GPU-driven grass).
//
// Depth-only caster that draws the grass partitions into a CSM cascade. The pose is
// the SHARED Flux_GrassBuildBladeVertex — the same wind, the same displacement push
// and the same fold the lit blade holds — so a swaying blade's shadow tracks its
// motion instead of anchoring to a static silhouette. Only the final projection
// differs, and it comes from the cascade pass's own VIEW set (each cascade is a
// render view whose g_xView holds the sun ortho matrices), exactly as every other
// caster resolves it.
//
// Cascades draw the LO index range, so the vertex ids that arrive here are the LO
// strip's subset of the same 15 logical vertices. Thickening runs against the
// CASCADE projection, which is what stops a blade thinner than a shadow texel from
// dropping out of its own cascade.
// =====================================================================

#include "Vegetation/Flux_GrassCommon.slang"   // spine sets 0/1/2 + records, tables, shared pose

// PASS block (set 3). Identical shape to the G-buffer program's so the binding code
// is shared; the fragment stage reads none of it.
struct PassParams
{
	ConstantBuffer<Flux_GrassDrawConstantsLayout> GrassDrawConstants;
	StructuredBuffer<Flux_GrassBladeInstance>     BladePool;
	StructuredBuffer<uint>                        VisibleIndices;
	StructuredBuffer<Flux_GrassTypeParamsGPU>     TypeParams;
};
ParameterBlock<PassParams> g_xPassSet;

[shader("vertex")]
float4 vsMain(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID, uint startInstance : SV_StartInstanceLocation) : SV_Position
{
	uint uBladeIndex = g_xPassSet.VisibleIndices[startInstance + instanceId];
	Flux_GrassBladeInstance xBlade = g_xPassSet.BladePool[uBladeIndex];
	Flux_GrassTypeParamsGPU xType  = g_xPassSet.TypeParams[Flux_GrassTypeFlagsIndex(xBlade.m_uTypeFlags)];

	Flux_GrassVertexContext xCtx = Flux_GrassMakeVertexContext(g_xPassSet.GrassDrawConstants);
	Flux_GrassVertexOut xVert = Flux_GrassBuildBladeVertex(xBlade, xType, vertexId, xCtx);

	// The cascade pass's own VIEW set: g_xView holds the sun ortho view x proj.
	return mul(GetViewProjMat(), float4(xVert.m_xWorldPos, 1.0));
}

[shader("fragment")]
void fsMain() {}
