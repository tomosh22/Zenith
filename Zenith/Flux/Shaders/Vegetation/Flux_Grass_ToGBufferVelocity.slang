// =====================================================================
// Flux_Grass_ToGBufferVelocity — the velocity variant of Flux_Grass_ToGBuffer.
//
// Recorded INSTEAD of the base grass G-buffer program inside the 5-attachment
// G-buffer pass when the velocity latch is on. Identical shading to the base (the
// shared Flux_GrassShadeBlade core), plus SV_Target4.
//
// Grass moves under the wind every frame, so a camera-reprojection-only motion
// vector would smear a swaying blade across its own trail. The vertex stage
// therefore builds the pose TWICE — once with this frame's wind constants and once
// with the previous frame's — and encodes the difference. This is only correct
// because a blade's identity is world-stable (its hash keys off its lattice node),
// so the record the previous frame produced for this blade held the same shape
// inputs and differed only in wind phase.
//
// The velocity variant is a SEPARATE program, never an extra SV_Target on the base:
// the base's GBufferOut is shared with 4-attachment passes that must not gain a
// fifth target (Common/Velocity.slang).
// =====================================================================

#include "Vegetation/Flux_GrassCommon.slang"   // spine sets 0/1/2 + records, tables, pose + shading cores
import Common.Velocity;                        // GBufferVelocityOut + Flux_EncodeVelocityUV + MakeGBufferVelocity

struct VsOut
{
	float4 position    : SV_Position;
	float3 worldPos    : TEXCOORD0;
	float3 normalWS    : TEXCOORD1;
	float2 heightTSide : TEXCOORD2;
	float  cameraDist  : TEXCOORD3;
	nointerpolation uint typeFlags   : TEXCOORD4;
	nointerpolation uint clumpPacked : TEXCOORD5;
	// SV_Position stays JITTERED so the raster is jittered; these two carry the
	// unjittered clip positions so the motion vector is jitter-free at the source.
	float4 clipCurNoJitter  : TEXCOORD6;
	float4 clipPrevNoJitter : TEXCOORD7;
};

// PASS block (set 3). Same shape as the base program plus the previous frame's wind
// block — the ONLY previous-frame state the pose depends on.
struct PassParams
{
	ConstantBuffer<Flux_GrassDrawConstantsLayout> GrassDrawConstants;
	ConstantBuffer<Flux_GrassWindBlock>           GrassPrevWindConstants;
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

	Flux_GrassVertexContext xPrevCtx = xCtx;
	xPrevCtx.m_xWind = Flux_GrassDecodeWind(g_xPassSet.GrassPrevWindConstants);
	Flux_GrassVertexOut xPrevVert = Flux_GrassBuildBladeVertex(xBlade, xType, vertexId, xPrevCtx);

	o.position         = mul(GetViewProjMat(), float4(xVert.m_xWorldPos, 1.0));
	o.clipCurNoJitter  = mul(GetViewProjMatNoJitter(), float4(xVert.m_xWorldPos, 1.0));
	o.clipPrevNoJitter = mul(GetPrevViewProjMatNoJitter(), float4(xPrevVert.m_xWorldPos, 1.0));
	o.worldPos    = xVert.m_xWorldPos;
	o.normalWS    = xVert.m_xNormalWS;
	o.heightTSide = float2(xVert.m_fHeightT, xVert.m_fSide);
	o.cameraDist  = xVert.m_fCameraDistance;
	o.typeFlags   = xBlade.m_uTypeFlags;
	o.clumpPacked = xBlade.m_uClumpPacked;
	return o;
}

[shader("fragment")]
GBufferVelocityOut fsMain(VsOut input, bool bFrontFace : SV_IsFrontFace)
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

	GBufferOut xCore = Flux_GrassShadeBlade(xIn, xType, xTail.y, asuint(xTail.z), bFrontFace);
	return MakeGBufferVelocity(xCore, Flux_EncodeVelocityUV(input.clipCurNoJitter, input.clipPrevNoJitter));
}
