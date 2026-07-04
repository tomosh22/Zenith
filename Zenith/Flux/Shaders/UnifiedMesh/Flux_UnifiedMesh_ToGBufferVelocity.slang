// =====================================================================
// Flux_UnifiedMesh_ToGBufferVelocity — the velocity variant of Flux_UnifiedMesh_ToGBuffer.
//
// Recorded INSTEAD of the base G-buffer program inside the 5-attachment unified
// G-buffer pass when the velocity latch is on (TAA active). Identical shading to the
// base (the SHARED EvaluateUnifiedGBuffer via EmitUnifiedFragment), plus it writes
// MRT_INDEX_VELOCITY (SV_Target4) with the per-pixel motion vector from the UNJITTERED
// current/previous view-proj. The extra MRT is supplied by a shell-local
// GBufferVelocityPolicy conforming to the shared IUnifiedMeshOutput interface
// (Common/UnifiedMeshDraw.slang) — the base GBuffer shell uses GBufferPolicy over the
// same seam, so the two lit shells share the whole material path and differ only by
// their output policy.
//
// Stage 3 computes CAMERA + STATIC velocity: prev world pos == current world pos, so a
// pixel's motion is pure camera reprojection (exactly correct for static geometry; a
// moving object / skinned pose gets camera-only velocity for now). Per-object previous
// transforms (moving rigid bodies) and previous skinned poses (skeletal motion) are a
// Stage-4 refinement — they only become observable once the temporal resolve exists.
// =====================================================================

#include "Common/UnifiedMeshDraw.slang"   // VsIn / UnifiedDrawConstantsLayout / DrawParams / shared transform + fragment + spine
import Common.Velocity;                    // GBufferVelocityOut / Flux_EncodeVelocityUV / MakeGBufferVelocity

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

// DRAW block (set 4) — declared AFTER the PASS block so it lands at physical space 4.
ParameterBlock<DrawParams> g_xDrawSet;

// This shell's output policy over the shared IUnifiedMeshOutput seam: append the
// per-pixel motion vector to the shared 4-target G-buffer core (the base GBuffer shell
// conforms GBufferPolicy over the same interface and drops the velocity).
struct GBufferVelocityPolicy : IUnifiedMeshOutput
{
    typealias TOut = GBufferVelocityOut;
    static GBufferVelocityOut Emit(GBufferOut xCore, float2 xVelocityUV)
    {
        return MakeGBufferVelocity(xCore, xVelocityUV);
    }
}

[shader("vertex")]
VsOut vsMain(VsIn input, uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
{
    VsOut o;

    UnifiedDrawConstantsLayout dc = g_xDrawSet.DrawConstants;

    uint             gid = g_xDrawSet.VisibleIndexBuffer[dc.g_uBucketOffset + instanceId];
    GPUSceneDrawItem di  = g_xPassSet.DrawItems[gid];
    GPUSceneObject   obj = g_xPassSet.Objects[di.m_uObjectIndex];

    // Shared VAT sway + model transform + world-space TBN basis — byte-identical to the
    // base GBuffer VS (and, for the position path, the shadow caster). b.xLocalPos is the
    // VAT-deformed local position; the prev-pose reprojection below reuses it.
    UnifiedVertexBasis b = TransformUnifiedVertex(input, vertexId, obj, dc, g_xDrawSet.g_xAnimationTex);

    o.uv           = input.a_xUV;
    o.vertexColor  = input.a_xColor;
    o.worldNormal  = b.xWorldNormal;
    o.worldTangent = b.xWorldTangent;
    o.bitangentSign = b.fBitangentSign;
    o.instanceTint = UnpackRGBA8(di.m_uColorTintPacked);
    o.worldPos     = b.xWorldPos;
    o.position     = mul(GetViewProjMat(), float4(b.xWorldPos, 1.0));

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
    float3 xPrevLocalPos = b.xLocalPos;
    if (dc.g_uIsSkinned != 0u)
    {
        uint pv = (dc.g_uSkinnedPrevVertBase + vertexId) * 3u;
        xPrevLocalPos = float3(
            asfloat(g_xPassSet.PrevSkinnedPosArena[pv + 0u]),
            asfloat(g_xPassSet.PrevSkinnedPosArena[pv + 1u]),
            asfloat(g_xPassSet.PrevSkinnedPosArena[pv + 2u]));
    }
    float3   xPrevWorldPos    = mul(xPrevModelMatrix, float4(xPrevLocalPos, 1.0)).xyz;
    o.clipCurNoJitter  = mul(GetViewProjMatNoJitter(),     float4(b.xWorldPos, 1.0));
    o.clipPrevNoJitter = mul(GetPrevViewProjMatNoJitter(), float4(xPrevWorldPos, 1.0));
    return o;
}

[shader("fragment")]
GBufferVelocityOut fsMain(VsOut input, bool bFrontFace : SV_IsFrontFace)
{
    UnifiedFragmentSurface s;
    s.uv            = input.uv;
    s.worldNormal   = input.worldNormal;
    s.worldPos      = input.worldPos;
    s.worldTangent  = input.worldTangent;
    s.bitangentSign = input.bitangentSign;
    s.vertexColor   = input.vertexColor;
    s.instanceTint  = input.instanceTint;

    // Motion vector from the UNJITTERED clip positions (jitter-free at the source).
    float2 xVelocity = Flux_EncodeVelocityUV(input.clipCurNoJitter, input.clipPrevNoJitter);

    return EmitUnifiedFragment<GBufferVelocityPolicy>(s, g_xDrawSet.DrawConstants.g_uMaterialIndex,
        bFrontFace, xVelocity);
}
