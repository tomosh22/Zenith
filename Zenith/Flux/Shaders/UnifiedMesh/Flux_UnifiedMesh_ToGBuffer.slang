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
// Material evaluation is the SHARED uber-material path (identical to StaticMeshes /
// InstancedMeshes): g_axMaterials lives in the persistent GLOBAL set 0 and the index
// is draw-uniform, so the existing fragment shading is reused verbatim plus the
// per-instance colour tint. No VAT on the static path (foliage: Stage 3).
// =====================================================================

#include "Common/Bindings.slang"
#include "Common/SceneObjects.slang"
import Common.MaterialTable;
import Common.Material;
import Common.MaterialSurface;
import Common.GBuffer;

struct VsIn
{
    float3 a_xPosition  : POSITION;
    float2 a_xUV        : TEXCOORD0;
    float3 a_xNormal    : NORMAL;
    float3 a_xTangent   : TANGENT;
    float3 a_xBitangent : BINORMAL;
    float4 a_xColor     : COLOR;
};

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

// DRAW block (set 4) — per-bucket constants + the bucket's slice of the visible
// list. Declared AFTER the PASS block so it lands at physical space 4.
struct UnifiedDrawConstantsLayout
{
    uint g_uMaterialIndex;  // index into g_axMaterials (GLOBAL set 0)
    uint g_uBucketOffset;   // base offset of this bucket's slice in VisibleIndexBuffer
    uint g_uPad0;
    uint g_uPad1;
};

struct DrawParams
{
    ConstantBuffer<UnifiedDrawConstantsLayout> DrawConstants;
    StructuredBuffer<uint>                     VisibleIndexBuffer;
};
ParameterBlock<DrawParams> g_xDrawSet;

float4 UnpackRGBA8(uint uPacked)
{
    return float4(
        (float)((uPacked >> 0)  & 0xFFu) / 255.0,
        (float)((uPacked >> 8)  & 0xFFu) / 255.0,
        (float)((uPacked >> 16) & 0xFFu) / 255.0,
        (float)((uPacked >> 24) & 0xFFu) / 255.0);
}

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

    float4x4 xModelMatrix  = obj.m_xModelMatrix;
    float4   xInstanceTint = UnpackRGBA8(di.m_uColorTintPacked);

    float3   xWorldPos     = mul(xModelMatrix, float4(input.a_xPosition, 1.0)).xyz;
    float3x3 xNormalMatrix = transpose(Inverse3x3((float3x3)xModelMatrix));

    o.uv          = input.a_xUV;
    o.vertexColor = input.a_xColor;
    o.worldNormal = normalize(mul(xNormalMatrix, input.a_xNormal));
    o.worldTangent = normalize(mul(xNormalMatrix, input.a_xTangent));
    float3 xBitangent = normalize(mul(xNormalMatrix, input.a_xBitangent));
    o.bitangentSign = sign(dot(xBitangent, cross(o.worldNormal, o.worldTangent)));
    o.instanceTint = xInstanceTint;
    o.worldPos     = xWorldPos;
    o.position     = mul(g_xViewSet.g_xView.g_xViewProjMat, float4(xWorldPos, 1.0));
    return o;
}

// Shared uber-material evaluation — identical to StaticMeshes/InstancedMeshes, plus
// the per-instance colour tint applied to the resolved albedo.
[shader("fragment")]
GBufferOut fsMain(VsOut input, bool bFrontFace : SV_IsFrontFace)
{
    float3x3 TBN = BuildTBN(input.worldNormal, input.worldTangent, input.bitangentSign);
    float3 xViewDir = normalize(g_xViewSet.g_xView.g_xCamPos_Pad.xyz - input.worldPos);

    UnifiedDrawConstantsLayout dc = g_xDrawSet.DrawConstants;
    MaterialGPU m = g_xGlobalSet.g_axMaterials[dc.g_uMaterialIndex];

    Sampler2D xBaseColorTex         = g_xBindlessSet.g_axTextures[m.g_xTexIdx0.x];
    Sampler2D xNormalTex            = g_xBindlessSet.g_axTextures[m.g_xTexIdx0.y];
    Sampler2D xRoughnessMetallicTex = g_xBindlessSet.g_axTextures[m.g_xTexIdx0.z];
    Sampler2D xOcclusionTex         = g_xBindlessSet.g_axTextures[m.g_xTexIdx0.w];
    Sampler2D xEmissiveTex          = g_xBindlessSet.g_axTextures[m.g_xTexIdx1.x];
    Sampler2D xHeightTex            = g_xBindlessSet.g_axTextures[m.g_xTexIdx1.y];
    Sampler2D xDetailAlbedoTex      = g_xBindlessSet.g_axTextures[m.g_xTexIdx1.z];
    Sampler2D xDetailNormalTex      = g_xBindlessSet.g_axTextures[m.g_xTexIdx1.w];
    Sampler2D xDetailMaskTex        = g_xBindlessSet.g_axTextures[m.g_xTexIdx2.x];

    MaterialSurface xSurface = EvaluateMaterialSurface(
        xBaseColorTex, xNormalTex, xRoughnessMetallicTex,
        xOcclusionTex, xEmissiveTex, xHeightTex,
        xDetailAlbedoTex, xDetailNormalTex, xDetailMaskTex,
        input.uv, TBN, xViewDir, input.vertexColor, bFrontFace, m);

    xSurface.albedo.rgb *= input.instanceTint.rgb;

    return MakeGBuffer(xSurface);
}
