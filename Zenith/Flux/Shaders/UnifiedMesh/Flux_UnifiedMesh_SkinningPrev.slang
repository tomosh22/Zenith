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
// Positions-only on purpose: a motion vector needs only the reprojected position. This
// shader is a direct subset of Flux_UnifiedMesh_Skinning.slang (the position accumulation
// only), pinned headlessly by Flux_SkinPrevPositionRaw (Flux/UnifiedMesh/Flux_Skinning.h) —
// which delegates to the same Flux_SkinVertexCPU the full skinner's tests already pin.
//
// Memory model matches the main skinner: the packed 32-byte bind-pose verts are read as a
// FLAT WORD array (StructuredBuffer<uint>, 8 words/vertex) and DECODED by hand through
// Common/VertexFormats.slang. The prev arena's STORAGE stays float32 (3 words/vertex;
// nothing fetches it as a vertex attribute — the velocity VS reads it as an SSBO), but the
// VALUES are snapped onto the same half4 lattice the main skinner packs the CURRENT arena
// onto: the velocity VS subtracts this position from the fetch-decoded (half-quantised)
// current one, so only a lattice-matched prev makes an unchanged pose read back as zero
// motion. An exact-float32 prev leaves the per-vertex half rounding residual
// (~2^-11 * |p|) as a PERMANENT motion vector on every static skinned pose, which TAA's
// velocity-scaled history blend responds to. (The snap cancels exactly wherever the two
// dispatches' accumulations round to the same half — everywhere except an FMA-scheduling
// difference landing a value on the far side of a rounding boundary, where the residual
// is one lattice step, the same magnitude the unsnapped write carried always.)
// The prev palette lays out its per-skeleton blocks at the SAME bases as the current
// palette (Flux_BonePaletteHistory), so job.m_uBonePaletteBase is reused verbatim.
// =====================================================================

#include "Common/Bindings.slang"
#include "Common/SceneObjects.slang"   // SkinJob
#include "Common/VertexFormats.slang"  // Flux_Unpack* — the packed-attribute codec

static const uint kSKIN_INPUT_WORDS   = 8u;    // 32 / 4
static const uint kSKIN_PREV_WORDS    = 3u;    // positions only (12 / 4), float32
static const uint kSKIN_BONE_SENTINEL = 255u;  // terminates a vertex's influence list (the uint8 slot's reserved byte)

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

    // ---- decode the packed bind-pose vertex's position + influences (raw words) ----
    uint i = (job.m_uBindPoseVertBase + vx) * kSKIN_INPUT_WORDS;
    float3 xPos = Flux_UnpackHalf4(uint2(g_xPassSet.bindPose[i + 0u], g_xPassSet.bindPose[i + 1u])).xyz;
    uint4  xBoneIDs = Flux_UnpackUint8x4(g_xPassSet.bindPose[i + 6u]);
    float4 xWeights = Flux_UnpackUnorm8x4(g_xPassSet.bindPose[i + 7u]);

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
    // Half-lattice snap (see the header): pack through the SAME codec the main skinner
    // stores the current arena with, then decode back to float32 for storage, so a
    // static pose subtracts to exactly zero in the velocity VS.
    float3 xSnappedPos = Flux_UnpackHalf4(Flux_PackHalf4(float4(xFinalPos.xyz, 1.0))).xyz;
    uint o = (job.m_uOutVertBase + vx) * kSKIN_PREV_WORDS;
    g_xPassSet.prevSkinnedArena[o + 0u] = asuint(xSnappedPos.x);
    g_xPassSet.prevSkinnedArena[o + 1u] = asuint(xSnappedPos.y);
    g_xPassSet.prevSkinnedArena[o + 2u] = asuint(xSnappedPos.z);
}
