// =====================================================================
// Flux_UnifiedMesh_ToGBuffer — Slang module (unified GPU-driven opaque-mesh pipeline).
//
// The unified static opaque mesh G-buffer pass. Per-instance transform comes from
// the GPU-scene records (Common/SceneObjects.slang) via a two-level indirection:
//   gid   = VisibleIndexBuffer[bucketOffset + SV_InstanceID]   (cull-compacted)
//   di    = DrawItems[gid]                                       (PASS set 3)
//   model = Objects[di.objectIndex].model                        (PASS set 3)
// One DrawIndexedIndirect per bucket; the visible-index buffer is partitioned per
// bucket (bucketOffset passed in the per-bucket DrawConstants).
//
// Two-block descriptor shape (the binding-model "principled subset"):
//   PASS block @ set 3 (g_xPassSet): GPUSceneObject + GPUSceneDrawItem SSBOs,
//     bound ONCE per pass (never per bucket).
//   DRAW block @ set 4 (g_xDrawSet): per-bucket DrawConstants (material-table index
//     + bucketOffset) + the per-bucket visible-index SRV, churned per bucket via
//     BindDrawConstants (which resets only its own set 4). The PASS block pushes the
//     DRAW block to physical space 4 (a program with no PASS block lands DRAW at 3).
//
// The vertex transform (TransformUnifiedVertex) and the fragment uber-material write
// (EmitUnifiedFragment<GBufferPolicy>) are the SHARED definitions in
// Common/UnifiedMeshDraw.slang — identical to the velocity variant and (for the VAT +
// world transform) the shadow caster, so the three shells can never drift. Material
// evaluation is the SHARED uber-material path (identical to StaticMeshes /
// InstancedMeshes): g_axMaterials lives in the persistent GLOBAL set 0 and the index
// is draw-uniform, so the existing fragment shading is reused verbatim plus the
// per-instance colour tint.
// =====================================================================

#include "Common/UnifiedMeshDraw.slang"   // VsIn / UnifiedDrawConstantsLayout / DrawParams / shared transform + fragment + spine

struct VsOut
{
    float4 position      : SV_Position;
    float2 uv            : TEXCOORD0;
    float3 worldNormal   : TEXCOORD1;
    float3 worldPos      : TEXCOORD2;
    float3 worldTangent  : TEXCOORD3;
    float  bitangentSign : TEXCOORD4;
    float4 vertexColor   : TEXCOORD5;
    float4 instanceTint  : TEXCOORD6;
};

// PASS block (set 3) — the shared GPU-scene records, bound once per pass. Read-only
// in the graphics pipeline (StructuredBuffer, so Slang emits NonWritable; without
// vertexPipelineStoresAndAtomics a vertex-stage storage buffer must be NonWritable —
// the cull compute writes the same physical buffers via RWStructuredBuffer).
struct PassParams
{
    StructuredBuffer<GPUSceneObject>   Objects;
    StructuredBuffer<GPUSceneDrawItem> DrawItems;
};
ParameterBlock<PassParams> g_xPassSet;

// DRAW block (set 4) — declared AFTER the PASS block so it lands at physical space 4.
ParameterBlock<DrawParams> g_xDrawSet;

[shader("vertex")]
VsOut vsMain(VsIn input, uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
{
    VsOut o;

    UnifiedDrawConstantsLayout dc = g_xDrawSet.DrawConstants;

    // Two-level indirection: SV_InstanceID -> this bucket's visible slice -> global
    // draw-item index -> object transform.
    uint             gid = g_xDrawSet.VisibleIndexBuffer[dc.g_uBucketOffset + instanceId];
    GPUSceneDrawItem di  = g_xPassSet.DrawItems[gid];
    GPUSceneObject   obj = g_xPassSet.Objects[di.m_uObjectIndex];

    // Shared VAT sway + model transform + world-space TBN basis (byte-identical to the
    // velocity VS, and — for the position path — the shadow caster). Static objects pass
    // through VAT unchanged.
    UnifiedVertexBasis b = TransformUnifiedVertex(input, vertexId, obj, dc, g_xDrawSet.g_xAnimationTex);

    o.uv           = input.a_xUV;
    o.vertexColor  = input.a_xColor;
    o.worldNormal  = b.xWorldNormal;
    o.worldTangent = b.xWorldTangent;
    o.bitangentSign = b.fBitangentSign;
    o.instanceTint = UnpackRGBA8(di.m_uColorTintPacked);
    o.worldPos     = b.xWorldPos;
    o.position     = mul(GetViewProjMat(), float4(b.xWorldPos, 1.0));
    return o;
}

// Shared uber-material evaluation via EmitUnifiedFragment<GBufferPolicy> — identical to
// StaticMeshes/InstancedMeshes, plus the per-instance colour tint (applied inside
// EvaluateUnifiedGBuffer). The GBuffer policy emits the 4-target core unchanged.
[shader("fragment")]
GBufferOut fsMain(VsOut input, bool bFrontFace : SV_IsFrontFace)
{
    UnifiedFragmentSurface s;
    s.uv            = input.uv;
    s.worldNormal   = input.worldNormal;
    s.worldPos      = input.worldPos;
    s.worldTangent  = input.worldTangent;
    s.bitangentSign = input.bitangentSign;
    s.vertexColor   = input.vertexColor;
    s.instanceTint  = input.instanceTint;

    return EmitUnifiedFragment<GBufferPolicy>(s, g_xDrawSet.DrawConstants.g_uMaterialIndex,
        bFrontFace, float2(0.0, 0.0));
}
