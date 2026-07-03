// =====================================================================
// Flux_UnifiedMesh_ToGBufferVelocity — the velocity variant of Flux_UnifiedMesh_ToGBuffer.
//
// Recorded INSTEAD of the base G-buffer program inside the 5-attachment unified
// G-buffer pass when the velocity latch is on (TAA active). Identical shading to the
// base (same EvaluateMaterialSurface), plus it writes MRT_INDEX_VELOCITY (SV_Target4)
// with the per-pixel motion vector from the UNJITTERED current/previous view-proj.
//
// Stage 3 computes CAMERA + STATIC velocity: prev world pos == current world pos, so a
// pixel's motion is pure camera reprojection (exactly correct for static geometry; a
// moving object / skinned pose gets camera-only velocity for now). Per-object previous
// transforms (moving rigid bodies) and previous skinned poses (skeletal motion) are a
// Stage-4 refinement — they only become observable once the temporal resolve exists.
// =====================================================================

#include "Common/Bindings.slang"
#include "Common/SceneObjects.slang"
#include "Common/VAT.slang"
import Common.MaterialTable;
import Common.Material;
import Common.MaterialSurface;
import Common.GBuffer;
import Common.Velocity;

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
    // Motion-vector clip positions, from the UNJITTERED matrices (SV_Position above stays
    // jittered so the raster is jittered; velocity is computed jitter-free).
    float4 clipCurNoJitter  : TEXCOORD7;
    float4 clipPrevNoJitter : TEXCOORD8;
};

struct PassParams
{
    StructuredBuffer<GPUSceneObject>   Objects;
    StructuredBuffer<GPUSceneDrawItem> DrawItems;
    // Stage 4.3: previous-frame object model matrices, index-locked to Objects (by
    // di.m_uObjectIndex). Uploaded parallel to Objects while the velocity latch is on.
    // Rigid/moving objects reproject through their PREVIOUS model matrix; a new / id-0
    // object has prev == current (camera-only velocity). Skinned POSE motion is a further
    // refinement (a previous skinned arena) — this already captures the model-transform
    // component of a moving skinned object.
    StructuredBuffer<float4x4>         PrevTransforms;
    // Stage 4.3b: previous-frame skinned OBJECT-space positions (positions-only, 3 words /
    // vertex), written by Flux_UnifiedMesh_SkinningPrev using the PREVIOUS bone palette. A
    // skinned draw reads its vertex's prior pose here (global index = draw's out-vert base +
    // SV_VertexID) so skeletal POSE motion no longer smears under TAA. Bound (with a valid
    // buffer) for every draw in the velocity pass; only skinned draws index it.
    StructuredBuffer<uint>             PrevSkinnedPosArena;
};
ParameterBlock<PassParams> g_xPassSet;

struct UnifiedDrawConstantsLayout
{
    uint   g_uMaterialIndex;
    uint   g_uBucketOffset;
    // Stage 4.3b (velocity variant only — byte-identical layout to the base ToGBuffer's
    // g_uPad0/g_uPad1, which ignore them): for a SKINNED draw, g_uSkinnedPrevVertBase is the
    // draw's arena slice base (uOutVertBase) and g_uIsSkinned is 1. Static/VAT draws leave both
    // 0 => the prev pose is the current local position through the previous transform.
    uint   g_uSkinnedPrevVertBase;
    uint   g_uIsSkinned;
    float4 g_xVATParams;
};

struct DrawParams
{
    ConstantBuffer<UnifiedDrawConstantsLayout> DrawConstants;
    StructuredBuffer<uint>                     VisibleIndexBuffer;
    Sampler2D                                  g_xAnimationTex;
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

    uint             gid = g_xDrawSet.VisibleIndexBuffer[dc.g_uBucketOffset + instanceId];
    GPUSceneDrawItem di  = g_xPassSet.DrawItems[gid];
    GPUSceneObject   obj = g_xPassSet.Objects[di.m_uObjectIndex];

    float4x4 xModelMatrix  = obj.m_xModelMatrix;
    float4   xInstanceTint = UnpackRGBA8(di.m_uColorTintPacked);

    float3   xLocalPos     = Flux_ApplyVAT(g_xDrawSet.g_xAnimationTex, vertexId, input.a_xPosition,
        obj.m_uFlags, obj.m_uVATAnimPacked, asfloat(obj.m_uVATAnimTime), dc.g_xVATParams);
    float3   xWorldPos     = mul(xModelMatrix, float4(xLocalPos, 1.0)).xyz;
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

    // Stage 4.3: reproject through the PREVIOUS-frame model matrix (index-locked to
    // Objects by object index). Same local position, previous transform => an exact
    // rigid / VAT motion vector, so a moving object no longer smears under TAA. A new /
    // id-0 object has prev == current model matrix => camera-only velocity (sub-texel).
    // The unjittered matrices keep the vector jitter-free at the source.
    float4x4 xPrevModelMatrix = g_xPassSet.PrevTransforms[di.m_uObjectIndex];

    // Stage 4.3b: a SKINNED draw takes its prior-frame OBJECT-space position from the prev
    // skinned arena (positions-only, 3 words/vertex) instead of the current local position, so
    // skeletal POSE motion is captured. Global arena index = draw's slice base + SV_VertexID
    // (the indirect vertexOffset is 0, so SV_VertexID is the submesh-local index; the slice
    // base rides the vertex-buffer bind offset — see Flux_UnifiedMesh_SkinningPrev / Reset).
    // Static / VAT draws keep the current local position (VAT sway is absorbed by the resolve
    // clamp). Both then go through the PREVIOUS model matrix.
    float3 xPrevLocalPos = xLocalPos;
    if (dc.g_uIsSkinned != 0u)
    {
        uint pv = (dc.g_uSkinnedPrevVertBase + vertexId) * 3u;
        xPrevLocalPos = float3(
            asfloat(g_xPassSet.PrevSkinnedPosArena[pv + 0u]),
            asfloat(g_xPassSet.PrevSkinnedPosArena[pv + 1u]),
            asfloat(g_xPassSet.PrevSkinnedPosArena[pv + 2u]));
    }
    float3   xPrevWorldPos    = mul(xPrevModelMatrix, float4(xPrevLocalPos, 1.0)).xyz;
    o.clipCurNoJitter  = mul(g_xViewSet.g_xView.g_xViewProjMatNoJitter,     float4(xWorldPos, 1.0));
    o.clipPrevNoJitter = mul(g_xViewSet.g_xView.g_xPrevViewProjMatNoJitter, float4(xPrevWorldPos, 1.0));
    return o;
}

[shader("fragment")]
GBufferVelocityOut fsMain(VsOut input, bool bFrontFace : SV_IsFrontFace)
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

    GBufferOut g = MakeGBuffer(xSurface);
    GBufferVelocityOut o;
    o.diffuse        = g.diffuse;
    o.normalsAmbient = g.normalsAmbient;
    o.material       = g.material;
    o.emissive       = g.emissive;
    o.velocity       = Flux_EncodeVelocityUV(input.clipCurNoJitter, input.clipPrevNoJitter);
    return o;
}
