// =====================================================================
// Flux_UnifiedMesh_SkinningPrev — TAA previous-pose skinning pre-pass.
//
// The velocity variant of Flux_UnifiedMesh_Skinning. Runs a SECOND compute-skinning
// dispatch that reuses the CURRENT frame's skin-jobs UNCHANGED (same bind-pose bases,
// same per-instance palette bases, same out bases) but reads the PREVIOUS-frame bone
// palette, and writes ONLY the skinned OBJECT-space POSITION into a compact prev arena
// (3 words / vertex). The UnifiedMesh velocity G-buffer VS reads this arena as a skinned
// vertex's prior-frame local position to encode the motion vector (uvPrev) for skeletal
// pose motion — the last deferred-motion source (rigid/VAT already reproject through the
// per-object PrevTransforms matrix; static geometry / camera are pure reprojection).
//
// Positions-only on purpose: a motion vector needs only the reprojected position, so the
// prev arena is 1/6 the size of the full 18-word arena. This shader is a direct subset of
// Flux_UnifiedMesh_Skinning.slang (the position accumulation only), pinned headlessly by
// Flux_SkinPrevPositionRaw (Flux/UnifiedMesh/Flux_Skinning.h) — which delegates to the same
// Flux_SkinVertexCPU the full skinner's tests already pin.
//
// Memory model matches the main skinner: the packed 104-byte bind-pose verts are read as a
// FLAT WORD array (StructuredBuffer<uint>, 26 words/vertex); the prev arena is written as a
// FLAT WORD array (RWStructuredBuffer<uint>, 3 words/vertex). The prev palette lays out its
// per-skeleton blocks at the SAME bases as the current palette (Flux_BonePaletteHistory), so
// job.m_uBonePaletteBase is reused verbatim.
// =====================================================================

#include "Common/Bindings.slang"
#include "Common/SceneObjects.slang"   // SkinJob

static const uint kSKIN_INPUT_WORDS   = 26u;          // 104 / 4
static const uint kSKIN_PREV_WORDS    = 3u;           // positions only (12 / 4)
static const uint kSKIN_BONE_SENTINEL = 0xFFFFFFFFu;  // terminates a vertex's influence list

struct SkinConstantsLayout
{
    uint numJobs;       // active skin-job count (id.y guard)
    uint paletteCount;  // total matrices in the PREV palette (bone-index bounds guard)
    uint pad0;
    uint pad1;
};

struct PassParams
{
    ConstantBuffer<SkinConstantsLayout> SkinConstants;
    StructuredBuffer<uint>              bindPose;          // shared pool, raw words (kSKIN_INPUT_WORDS / vertex)
    StructuredBuffer<float4x4>          prevBonePalette;   // PREVIOUS-frame palette, same bases as the current palette
    StructuredBuffer<SkinJob>           skinJobs;          // [numJobs] — SHARED with the current dispatch
    RWStructuredBuffer<uint>            prevSkinnedArena;  // output positions, raw words (kSKIN_PREV_WORDS / vertex)
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

    // ---- read the packed bind-pose vertex's position + influences (raw words) ----
    uint i = (job.m_uBindPoseVertBase + vx) * kSKIN_INPUT_WORDS;
    float3 xPos = float3(asfloat(g_xPassSet.bindPose[i + 0u]), asfloat(g_xPassSet.bindPose[i + 1u]), asfloat(g_xPassSet.bindPose[i + 2u]));
    uint4  xBoneIDs = uint4(g_xPassSet.bindPose[i + 18u], g_xPassSet.bindPose[i + 19u], g_xPassSet.bindPose[i + 20u], g_xPassSet.bindPose[i + 21u]);
    float4 xWeights = float4(asfloat(g_xPassSet.bindPose[i + 22u]), asfloat(g_xPassSet.bindPose[i + 23u]), asfloat(g_xPassSet.bindPose[i + 24u]), asfloat(g_xPassSet.bindPose[i + 25u]));

    // ---- bone-weighted accumulation to OBJECT space (position only, no normalise) ----
    float4 xFinalPos = float4(0.0, 0.0, 0.0, 0.0);
    for (uint u = 0; u < 4u; u++)
    {
        if (xBoneIDs[u] == kSKIN_BONE_SENTINEL) break;
        uint pi = job.m_uBonePaletteBase + xBoneIDs[u];
        if (pi >= g_xPassSet.SkinConstants.paletteCount) continue;   // defensive (corrupt data)

        float4x4 m = g_xPassSet.prevBonePalette[pi];
        xFinalPos += mul(m, float4(xPos, 1.0)) * xWeights[u];
    }

    // ---- write the 3-word position at the GLOBAL arena index (outBase + local vertex) ----
    uint o = (job.m_uOutVertBase + vx) * kSKIN_PREV_WORDS;
    g_xPassSet.prevSkinnedArena[o + 0u] = asuint(xFinalPos.x);
    g_xPassSet.prevSkinnedArena[o + 1u] = asuint(xFinalPos.y);
    g_xPassSet.prevSkinnedArena[o + 2u] = asuint(xFinalPos.z);
}
