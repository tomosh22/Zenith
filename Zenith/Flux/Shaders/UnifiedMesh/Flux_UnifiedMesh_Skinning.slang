// =====================================================================
// Flux_UnifiedMesh_Skinning — Slang module (unified GPU-driven opaque-mesh pipeline, Stage 5).
//
// GPU compute-skinning PRE-PASS. Skins each animated submesh-instance to OBJECT/LOCAL
// space and writes the result (in the static 72-byte vertex layout) into a shared
// skinned-vertex arena. The unified static G-buffer / shadow VS then consume the arena
// UNCHANGED (they read the per-object model matrix from the GPU scene and apply it +
// re-normalise) — so skeletal meshes flow through the existing cull/draw/shadow kernels
// with zero kernel-shader edits.
//
// Memory model (the load-bearing bit): the packed 104-byte bind-pose verts and the
// packed 72-byte output verts are accessed as FLAT WORD arrays (StructuredBuffer<uint> /
// RWStructuredBuffer<uint>), NOT StructuredBuffer<float3...>. std430 16-byte-aligns a
// vec3, which would NOT match the 12-byte-packed vertex layout. The exact word offsets
// here are pinned headlessly by Flux_SkinVertexRaw (Flux/UnifiedMesh/Flux_Skinning.h) —
// this shader is a direct transliteration of that (and of Flux_SkinVertexCPU's math).
//   input  words (104B): pos@0 uv@3 normal@5 tangent@8 bitangent@11 color@14 boneIDs@18 weights@22
//   output words ( 72B): pos@0 uv@3 normal@5 tangent@8 bitangent@11 color@14
//
// Dispatch: 2D over (vertex, job) — one thread per (skin-job, local vertex). Thread
// (id.x = vertex, id.y = job); guards vx < job.vertCount and jobIdx < numJobs. Variable
// per-job vertex counts rule out the cull's uniform-stride 1D decode.
//
// The bone matrices are uploaded from Flux_SkeletonInstance::GetSkinningMatrices() (glm
// column-major), read here as float4x4 + used via mul(m, v) — the SAME convention the
// (proven) legacy animated VS uses for its bone CB, so no transpose is needed.
// =====================================================================

#include "Common/Bindings.slang"
#include "Common/SceneObjects.slang"   // SkinJob

static const uint kSKIN_INPUT_WORDS   = 26u;          // 104 / 4
static const uint kSKIN_OUTPUT_WORDS  = 18u;          //  72 / 4
static const uint kSKIN_BONE_SENTINEL = 0xFFFFFFFFu;  // terminates a vertex's influence list

struct SkinConstantsLayout
{
    uint numJobs;       // active skin-job count (id.y guard)
    uint paletteCount;  // total matrices in the palette (bone-index bounds guard)
    uint pad0;
    uint pad1;
};

struct PassParams
{
    ConstantBuffer<SkinConstantsLayout> SkinConstants;
    StructuredBuffer<uint>              bindPose;      // shared pool, raw words (kSKIN_INPUT_WORDS / vertex)
    StructuredBuffer<float4x4>          bonePalette;   // all live skeletons' skinning matrices, concatenated
    StructuredBuffer<SkinJob>           skinJobs;      // [numJobs]
    RWStructuredBuffer<uint>            skinnedArena;  // output, raw words (kSKIN_OUTPUT_WORDS / vertex)
};
ParameterBlock<PassParams> g_xPassSet;

[shader("compute")]
[numthreads(64, 1, 1)]
void csMain(uint3 id : SV_DispatchThreadID)
{
    uint jobIdx = id.y;
    if (jobIdx >= g_xPassSet.SkinConstants.numJobs) return;
    SkinJob job = g_xPassSet.skinJobs[jobIdx];

    uint vx = id.x;
    if (vx >= job.m_uVertCount) return;

    // ---- read the packed bind-pose vertex (raw words) ----
    uint i = (job.m_uBindPoseVertBase + vx) * kSKIN_INPUT_WORDS;
    float3 xPos = float3(asfloat(g_xPassSet.bindPose[i + 0u]), asfloat(g_xPassSet.bindPose[i + 1u]), asfloat(g_xPassSet.bindPose[i + 2u]));
    float2 xUV  = float2(asfloat(g_xPassSet.bindPose[i + 3u]), asfloat(g_xPassSet.bindPose[i + 4u]));
    float3 xNrm = float3(asfloat(g_xPassSet.bindPose[i + 5u]), asfloat(g_xPassSet.bindPose[i + 6u]), asfloat(g_xPassSet.bindPose[i + 7u]));
    float3 xTan = float3(asfloat(g_xPassSet.bindPose[i + 8u]), asfloat(g_xPassSet.bindPose[i + 9u]), asfloat(g_xPassSet.bindPose[i + 10u]));
    float3 xBit = float3(asfloat(g_xPassSet.bindPose[i + 11u]), asfloat(g_xPassSet.bindPose[i + 12u]), asfloat(g_xPassSet.bindPose[i + 13u]));
    float4 xCol = float4(asfloat(g_xPassSet.bindPose[i + 14u]), asfloat(g_xPassSet.bindPose[i + 15u]), asfloat(g_xPassSet.bindPose[i + 16u]), asfloat(g_xPassSet.bindPose[i + 17u]));
    uint4  xBoneIDs = uint4(g_xPassSet.bindPose[i + 18u], g_xPassSet.bindPose[i + 19u], g_xPassSet.bindPose[i + 20u], g_xPassSet.bindPose[i + 21u]);
    float4 xWeights = float4(asfloat(g_xPassSet.bindPose[i + 22u]), asfloat(g_xPassSet.bindPose[i + 23u]), asfloat(g_xPassSet.bindPose[i + 24u]), asfloat(g_xPassSet.bindPose[i + 25u]));

    // ---- bone-weighted accumulation to OBJECT space (no normalise — the VS re-normalises) ----
    float4 xFinalPos = float4(0.0, 0.0, 0.0, 0.0);
    float3 xFinalNrm = float3(0.0, 0.0, 0.0);
    float3 xFinalTan = float3(0.0, 0.0, 0.0);
    float3 xFinalBit = float3(0.0, 0.0, 0.0);

    for (uint u = 0; u < 4u; u++)
    {
        if (xBoneIDs[u] == kSKIN_BONE_SENTINEL) break;
        uint pi = job.m_uBonePaletteBase + xBoneIDs[u];
        if (pi >= g_xPassSet.SkinConstants.paletteCount) continue;   // defensive (corrupt data)

        float4x4 m = g_xPassSet.bonePalette[pi];
        float    w = xWeights[u];
        xFinalPos += mul(m, float4(xPos, 1.0)) * w;
        xFinalNrm += mul((float3x3)m, xNrm) * w;
        xFinalTan += mul((float3x3)m, xTan) * w;
        xFinalBit += mul((float3x3)m, xBit) * w;
    }

    // ---- write the packed 72-byte static output vertex (raw words) ----
    uint o = (job.m_uOutVertBase + vx) * kSKIN_OUTPUT_WORDS;
    g_xPassSet.skinnedArena[o + 0u]  = asuint(xFinalPos.x); g_xPassSet.skinnedArena[o + 1u]  = asuint(xFinalPos.y); g_xPassSet.skinnedArena[o + 2u]  = asuint(xFinalPos.z);
    g_xPassSet.skinnedArena[o + 3u]  = asuint(xUV.x);       g_xPassSet.skinnedArena[o + 4u]  = asuint(xUV.y);
    g_xPassSet.skinnedArena[o + 5u]  = asuint(xFinalNrm.x); g_xPassSet.skinnedArena[o + 6u]  = asuint(xFinalNrm.y); g_xPassSet.skinnedArena[o + 7u]  = asuint(xFinalNrm.z);
    g_xPassSet.skinnedArena[o + 8u]  = asuint(xFinalTan.x); g_xPassSet.skinnedArena[o + 9u]  = asuint(xFinalTan.y); g_xPassSet.skinnedArena[o + 10u] = asuint(xFinalTan.z);
    g_xPassSet.skinnedArena[o + 11u] = asuint(xFinalBit.x); g_xPassSet.skinnedArena[o + 12u] = asuint(xFinalBit.y); g_xPassSet.skinnedArena[o + 13u] = asuint(xFinalBit.z);
    g_xPassSet.skinnedArena[o + 14u] = asuint(xCol.x);      g_xPassSet.skinnedArena[o + 15u] = asuint(xCol.y);      g_xPassSet.skinnedArena[o + 16u] = asuint(xCol.z); g_xPassSet.skinnedArena[o + 17u] = asuint(xCol.w);
}
