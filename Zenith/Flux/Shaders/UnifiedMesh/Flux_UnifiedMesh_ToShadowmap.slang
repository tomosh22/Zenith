// =====================================================================
// Flux_UnifiedMesh_ToShadowmap — Slang module (unified GPU-driven opaque-mesh pipeline).
//
// Depth-only caster pass that writes the unified opaque static meshes into a CSM
// cascade — the Stage-2 shadow analogue of Flux_UnifiedMesh_ToGBuffer. The vertex
// transform is BYTE-IDENTICAL to the GBuffer VS (same two-level indirection into the
// GPU-scene records) so the shadow pose matches the lit pose exactly; only the final
// projection differs (the cascade's sun view×proj instead of the camera view×proj).
//
//   gid   = VisibleIndexBuffer[bucketOffset + SV_InstanceID]   (cull-compacted, per-view)
//   di    = DrawItems[gid]                                       (PASS set 3)
//   model = Objects[di.objectIndex].model                        (PASS set 3)
//   clip  = g_xViewSet.g_xShadowMatrices[shadowCascade] * world  (VIEW set 1, Phase 5.4)
//
// Two-block descriptor shape, identical to the GBuffer caster so binding code is
// shared:
//   PASS block @ set 3 (g_xPassSet): GPUSceneObject + GPUSceneDrawItem SSBOs.
//   DRAW block @ set 4 (g_xDrawSet): per-bucket DrawConstants (material index +
//     per-view bucketOffset + cascade) + the per-(view,bucket) visible-index SRV.
//
// Fragment runs the SHARED masked alpha cutout (identical to the StaticMeshes /
// InstancedMeshes shadow casters): opaque casters (cutoff 0) uniform-branch out →
// depth-only; masked casters discard cut-out texels so foliage/fences cast cut-out
// shadows. g_axMaterials lives in the persistent GLOBAL set 0; the base-colour
// texture is bindless (set 2).
// =====================================================================

#include "Common/Bindings.slang"
#include "Common/SceneObjects.slang"
#include "Common/VAT.slang"
import Common.MaterialTable;
import Common.Material;
import Common.MaterialSurface;

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
    float4 position    : SV_Position;
    float2 uv          : TEXCOORD0;
    float4 vertexColor : TEXCOORD1;
};

// PASS block (set 3) — the shared GPU-scene records, bound once per cascade.
struct PassParams
{
    StructuredBuffer<GPUSceneObject>   Objects;
    StructuredBuffer<GPUSceneDrawItem> DrawItems;
};
ParameterBlock<PassParams> g_xPassSet;

// DRAW block (set 4) — per-bucket constants + the (view,bucket) slice of the visible
// list. Same UnifiedDrawConstantsLayout as the GBuffer caster (g_uShadowCascade live here).
struct UnifiedDrawConstantsLayout
{
    uint   g_uMaterialIndex;
    uint   g_uBucketOffset;   // per-view base: cascadeView*totalDrawItems + bucketOffset[b]
    uint   g_uShadowCascade;  // selects g_xViewSet.g_xShadowMatrices[cascade]
    uint   g_uPad0;
    float4 g_xVATParams;      // xy = VAT texture size, z = >0 when the bucket has a VAT texture
};

struct DrawParams
{
    ConstantBuffer<UnifiedDrawConstantsLayout> DrawConstants;
    StructuredBuffer<uint>                     VisibleIndexBuffer;
    Sampler2D                                  g_xAnimationTex;   // per-bucket VAT positions (POINT sampler; white dummy when none)
};
ParameterBlock<DrawParams> g_xDrawSet;

[shader("vertex")]
VsOut vsMain(VsIn input, uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
{
    VsOut o;

    UnifiedDrawConstantsLayout dc = g_xDrawSet.DrawConstants;

    // Same two-level indirection as the GBuffer VS → identical world transform.
    uint             gid = g_xDrawSet.VisibleIndexBuffer[dc.g_uBucketOffset + instanceId];
    GPUSceneDrawItem di  = g_xPassSet.DrawItems[gid];
    GPUSceneObject   obj = g_xPassSet.Objects[di.m_uObjectIndex];

    // IDENTICAL VAT deformation to the GBuffer VS — the shadow pose must match the lit pose
    // exactly or self-shadows flicker as foliage sways.
    float3 xLocalPos = Flux_ApplyVAT(g_xDrawSet.g_xAnimationTex, vertexId, input.a_xPosition,
        obj.m_uFlags, obj.m_uVATAnimPacked, asfloat(obj.m_uVATAnimTime), dc.g_xVATParams);
    float3 xWorldPos = mul(obj.m_xModelMatrix, float4(xLocalPos, 1.0)).xyz;
    o.position    = mul(g_xViewSet.g_xShadowMatrices[dc.g_uShadowCascade], float4(xWorldPos, 1.0));
    o.uv          = input.a_xUV;
    o.vertexColor = input.a_xColor;
    return o;
}

[shader("fragment")]
void fsMain(VsOut input)
{
    UnifiedDrawConstantsLayout dc = g_xDrawSet.DrawConstants;
    MaterialGPU m = g_xGlobalSet.g_axMaterials[dc.g_uMaterialIndex];
    // Uniform branch: opaque casters (cutoff 0) skip the sample entirely → depth-only
    // cost preserved. Masked casters run the SAME cutout as the G-buffer path.
    if (GetAlphaCutoff(m) > 0.0)
    {
        Sampler2D xBaseColorTex = g_xBindlessSet.g_axTextures[m.g_xTexIdx0.x];
        SampleAlbedoWithCutout(xBaseColorTex,
            TransformUV(input.uv, GetUVTiling(m), GetUVOffset(m)), input.vertexColor, m);
    }
}
