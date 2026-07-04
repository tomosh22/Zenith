// =====================================================================
// Flux_UnifiedMesh_ToShadowmap — Slang module (unified GPU-driven opaque-mesh pipeline).
//
// Depth-only caster pass that writes the unified opaque static meshes into a CSM
// cascade — the Stage-2 shadow analogue of Flux_UnifiedMesh_ToGBuffer. The vertex
// transform is the SHARED TransformUnifiedVertexPosition (Common/UnifiedMeshDraw.slang),
// bit-for-bit the same VAT sway + model transform the GBuffer + velocity VS use, so the
// shadow pose matches the lit pose exactly; only the final projection differs (the
// cascade's sun view x proj instead of the camera view x proj). This is the STRUCTURAL
// replacement for the old hand-maintained "keep these BYTE-IDENTICAL or foliage self-
// shadows flicker" comment contract — there is now one position path, three callers.
//
//   gid   = VisibleIndexBuffer[bucketOffset + SV_InstanceID]   (cull-compacted, per-view)
//   di    = DrawItems[gid]                                       (PASS set 3)
//   model = Objects[di.objectIndex].model                        (PASS set 3)
//   clip  = GetViewProjMat() * world            (the cascade VIEW's own camera)
//
// Each cascade pass binds its own per-view VIEW set (Flux_RenderViews slot 1+c)
// whose g_xView holds the sun ortho matrices — the caster is view-agnostic, like
// any other pass. g_xShadowMatrices stays in the VIEW set for RECEIVERS only
// (deferred/translucency/fog sample it to project receiver positions).
//
// Two-block descriptor shape, identical to the GBuffer caster so binding code is
// shared:
//   PASS block @ set 3 (g_xPassSet): GPUSceneObject + GPUSceneDrawItem SSBOs.
//   DRAW block @ set 4 (g_xDrawSet): per-bucket DrawConstants (material index +
//     per-view bucketOffset) + the per-(view,bucket) visible-index SRV + VAT texture.
//
// Fragment runs the SHARED masked alpha cutout (identical to the StaticMeshes /
// InstancedMeshes shadow casters): opaque casters (cutoff 0) uniform-branch out →
// depth-only; masked casters discard cut-out texels so foliage/fences cast cut-out
// shadows. g_axMaterials lives in the persistent GLOBAL set 0; the base-colour
// texture is bindless (set 2).
// =====================================================================

#include "Common/UnifiedMeshDraw.slang"   // VsIn / UnifiedDrawConstantsLayout / DrawParams / shared transform + spine

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

// DRAW block (set 4) — declared AFTER the PASS block so it lands at physical space 4.
ParameterBlock<DrawParams> g_xDrawSet;

[shader("vertex")]
VsOut vsMain(VsIn input, uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
{
    VsOut o;

    UnifiedDrawConstantsLayout dc = g_xDrawSet.DrawConstants;

    // Same two-level indirection as the GBuffer VS → identical object transform.
    uint             gid = g_xDrawSet.VisibleIndexBuffer[dc.g_uBucketOffset + instanceId];
    GPUSceneDrawItem di  = g_xPassSet.DrawItems[gid];
    GPUSceneObject   obj = g_xPassSet.Objects[di.m_uObjectIndex];

    // Shared VAT sway + model transform — byte-identical to the GBuffer / velocity VS so
    // the shadow pose matches the lit pose exactly (foliage self-shadows never flicker).
    // The VAT-deformed local position is unused by the depth-only caster (dead-stripped).
    float3 xLocalPos;
    float3 xWorldPos = TransformUnifiedVertexPosition(input, vertexId, obj, dc,
        g_xDrawSet.g_xAnimationTex, xLocalPos);
    // The cascade pass's own VIEW set: g_xView holds the sun ortho view×proj.
    o.position    = mul(GetViewProjMat(), float4(xWorldPos, 1.0));
    o.uv          = input.a_xUV;
    o.vertexColor = input.a_xColor;
    return o;
}

[shader("fragment")]
void fsMain(VsOut input)
{
    UnifiedDrawConstantsLayout dc = g_xDrawSet.DrawConstants;
    MaterialGPU m = GetMaterial(dc.g_uMaterialIndex);
    // Uniform branch: opaque casters (cutoff 0) skip the sample entirely → depth-only
    // cost preserved. Masked casters run the SAME cutout as the G-buffer path.
    if (GetAlphaCutoff(m) > 0.0)
    {
        Sampler2D xBaseColorTex = GetBindlessTexture(m.g_xTexIdx0.x);
        SampleAlbedoWithCutout(xBaseColorTex,
            TransformUV(input.uv, GetUVTiling(m), GetUVOffset(m)), input.vertexColor, m);
    }
}
