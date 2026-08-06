// =====================================================================
// Flux_Grass_ToGBuffer — Slang module (GPU-driven grass).
//
// Two DrawIndexedIndirect calls per frame (HI then LO) read their instance counts
// from the grass indirect-args block. There is NO VERTEX BUFFER: the draw is
// indexed against the 48-entry blade index table, so SV_VertexID delivers the
// fetched index value, which IS the logical vertex id 0..14. firstInstance carries
// the partition base, so SV_StartInstanceLocation + SV_InstanceID addresses the
// visible-index list without a per-draw constant.
//
// Blades write the DEPTH-WRITING G-buffer as opaque geometry with the SUBSURFACE
// shading tag, so deferred lighting gives them wrap diffuse and warm scatter. They
// are NOT a forward blended pass over the lit scene.
//
// The pipeline must disable back-face culling: a blade is a two-sided sliver, and
// the fragment stage flips the lighting normal on back faces.
// =====================================================================

#include "Vegetation/Flux_GrassCommon.slang"   // spine sets 0/1/2 + records, tables, pose + shading cores

struct VsOut
{
	float4 position    : SV_Position;
	float3 worldPos    : TEXCOORD0;
	float3 normalWS    : TEXCOORD1;
	float2 heightTSide : TEXCOORD2;   // x = height fraction, y = -1/+1 edge (0 at the tip)
	float  cameraDist  : TEXCOORD3;
	nointerpolation uint typeFlags   : TEXCOORD4;
	nointerpolation uint clumpPacked : TEXCOORD5;
};

// PASS block (set 3). All three buffers are read-only in the vertex stage, so they
// are StructuredBuffer (Slang emits NonWritable) — vertexPipelineStoresAndAtomics
// is off.
struct PassParams
{
	ConstantBuffer<Flux_GrassDrawConstantsLayout> GrassDrawConstants;
	StructuredBuffer<Flux_GrassBladeInstance>     BladePool;
	StructuredBuffer<uint>                        VisibleIndices;
	StructuredBuffer<Flux_GrassTypeParamsGPU>     TypeParams;
};
ParameterBlock<PassParams> g_xPassSet;

[shader("vertex")]
VsOut vsMain(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID, uint startInstance : SV_StartInstanceLocation)
{
	VsOut o;

	uint uBladeIndex = g_xPassSet.VisibleIndices[startInstance + instanceId];
	Flux_GrassBladeInstance xBlade = g_xPassSet.BladePool[uBladeIndex];
	Flux_GrassTypeParamsGPU xType  = g_xPassSet.TypeParams[Flux_GrassTypeFlagsIndex(xBlade.m_uTypeFlags)];

	Flux_GrassVertexContext xCtx = Flux_GrassMakeVertexContext(g_xPassSet.GrassDrawConstants);
	Flux_GrassVertexOut xVert = Flux_GrassBuildBladeVertex(xBlade, xType, vertexId, xCtx);

	o.position    = mul(GetViewProjMat(), float4(xVert.m_xWorldPos, 1.0));
	o.worldPos    = xVert.m_xWorldPos;
	o.normalWS    = xVert.m_xNormalWS;
	o.heightTSide = float2(xVert.m_fHeightT, xVert.m_fSide);
	o.cameraDist  = xVert.m_fCameraDistance;
	o.typeFlags   = xBlade.m_uTypeFlags;
	o.clumpPacked = xBlade.m_uClumpPacked;
	return o;
}

[shader("fragment")]
GBufferOut fsMain(VsOut input, bool bFrontFace : SV_IsFrontFace)
{
	Flux_GrassFragmentIn xIn;
	xIn.m_xWorldPos       = input.worldPos;
	xIn.m_xNormalWS       = normalize(input.normalWS);
	xIn.m_fHeightT        = input.heightTSide.x;
	xIn.m_fSide           = input.heightTSide.y;
	xIn.m_fCameraDistance = input.cameraDist;
	xIn.m_uTypeFlags      = input.typeFlags;
	xIn.m_uClumpPacked    = input.clumpPacked;

	Flux_GrassTypeParamsGPU xType = g_xPassSet.TypeParams[Flux_GrassTypeFlagsIndex(input.typeFlags)];
	float4 xTail = g_xPassSet.GrassDrawConstants.m_xMaxDrawDistance_GlossCut_DebugMode_Pad;

	return Flux_GrassShadeBlade(xIn, xType, xTail.y, asuint(xTail.z), bFrontFace);
}
