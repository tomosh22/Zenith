// =====================================================================
// Flux_UnifiedMesh_Skinning — Slang module (unified GPU-driven opaque-mesh pipeline, Stage 5).
//
// GPU compute-skinning PRE-PASS. Skins each animated submesh-instance to OBJECT/LOCAL
// space and writes the result (in the static 24-byte vertex layout) into a shared
// skinned-vertex arena. The unified static G-buffer / shadow VS then consume the arena
// UNCHANGED (they read the per-object model matrix from the GPU scene and apply it +
// re-normalise) — so skeletal meshes flow through the existing cull/draw/shadow kernels
// with zero kernel-shader edits.
//
// Memory model (the load-bearing bit): the packed 32-byte bind-pose verts and the
// packed 24-byte output verts are accessed as FLAT WORD arrays (StructuredBuffer<uint> /
// RWStructuredBuffer<uint>), NOT StructuredBuffer<float3...>. std430 16-byte-aligns a
// vec3, which would NOT match the packed vertex layout, and there is no fetch unit on a
// compute path to decode the packed formats either — so this shader packs and unpacks by
// hand through Common/VertexFormats.slang, the Slang mirror of Flux/Flux_VertexCodec.h.
// The exact word offsets here are pinned headlessly by Flux_SkinVertexRaw
// (Flux/UnifiedMesh/Flux_Skinning.h) — this shader is a direct transliteration of that
// (and of Flux_SkinVertexCPU's math).
//   input  words (32B): pos@0-1 (half4) uv@2 (half2) normal@3 (snorm10) tangent@4 (snorm10,
//                       w = bitangent sign) colour@5 (unorm8x4) boneIDs@6 (uint8x4)
//                       weights@7 (unorm8x4)
//   output words (24B): pos@0-1 uv@2 normal@3 tangent@4 colour@5 — BYTE-IDENTICAL to the
//                       static-mesh VsIn (Common/StaticMeshVertex.slang), because the
//                       arena is bound as those pipelines' vertex buffer.
//
// The OUTPUT normal/tangent are re-normalised before packing, which the float32 arena did
// not do. A 10-bit snorm clamps per axis, so packing an un-normalised bone-weighted sum
// would change its DIRECTION and not merely its precision. The VS re-normalises anyway,
// so this is the same shading — it just has to happen before the quantisation, not after.
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
#include "Common/VertexFormats.slang"  // Flux_Pack*/Flux_Unpack* — the packed-attribute codec

static const uint kSKIN_INPUT_WORDS   = 8u;    // 32 / 4
static const uint kSKIN_OUTPUT_WORDS  = 6u;    // 24 / 4
static const uint kSKIN_BONE_SENTINEL = 255u;  // terminates a vertex's influence list (the uint8 slot's reserved byte)

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

// A direction that survives snorm10 quantisation: unit length, or the canonical
// default when there is nothing to normalise. Mirrors Flux_NormaliseForSnorm10
// (Flux_VertexCodec.h) — a zero-length sum has no direction, and 0/0 would pack a
// NaN as a plausible-looking one. The guard is on the BITS, like the CPU side: a
// float compare is exactly the predicate fast-math is licensed to fold away, and
// it would also pass +inf (corrupt palette data), where value * rsqrt(inf) packs
// a zero direction instead of the default. Positive-finite means: sign 0,
// exponent != 0xFF, magnitude != 0. (rsqrt vs the CPU's exact 1/sqrt differs by
// <= 2 ULP per Vulkan — far inside one snorm10 step of 1/511.)
float3 NormaliseOrDefault(float3 xValue, float3 xDefault)
{
    float fLengthSq = dot(xValue, xValue);
    uint uBits = asuint(fLengthSq);
    bool bPositiveFinite = ((uBits >> 31u) == 0u)
        && (((uBits >> 23u) & 0xFFu) != 0xFFu)
        && ((uBits & 0x7FFFFFFFu) != 0u);
    return bPositiveFinite ? (xValue * rsqrt(fLengthSq)) : xDefault;
}

[shader("compute")]
[numthreads(64, 1, 1)]
void csMain(uint3 id : SV_DispatchThreadID)
{
    uint jobIdx = id.y;
    if (jobIdx >= g_xPassSet.SkinConstants.numJobs) return;
    SkinJob job = g_xPassSet.skinJobs[jobIdx];

    uint vx = id.x;
    if (vx >= job.m_uVertCount) return;

    // ---- read + decode the packed bind-pose vertex (raw words) ----
    uint i = (job.m_uBindPoseVertBase + vx) * kSKIN_INPUT_WORDS;
    float3 xPos = Flux_UnpackHalf4(uint2(g_xPassSet.bindPose[i + 0u], g_xPassSet.bindPose[i + 1u])).xyz;
    float2 xUV  = Flux_UnpackHalf2(g_xPassSet.bindPose[i + 2u]);
    float3 xNrm = Flux_UnpackSnorm10_10_10_2(g_xPassSet.bindPose[i + 3u]).xyz;
    float4 xTan = Flux_UnpackSnorm10_10_10_2(g_xPassSet.bindPose[i + 4u]);   // w = bitangent sign
    float4 xCol = Flux_UnpackUnorm8x4(g_xPassSet.bindPose[i + 5u]);
    uint4  xBoneIDs = Flux_UnpackUint8x4(g_xPassSet.bindPose[i + 6u]);
    float4 xWeights = Flux_UnpackUnorm8x4(g_xPassSet.bindPose[i + 7u]);

    // ---- bone-weighted accumulation to OBJECT space ----
    float4 xFinalPos = float4(0.0, 0.0, 0.0, 0.0);
    float3 xFinalNrm = float3(0.0, 0.0, 0.0);
    float3 xFinalTan = float3(0.0, 0.0, 0.0);
    float3x3 xBlend3 = float3x3(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);

    for (uint u = 0; u < 4u; u++)
    {
        if (xBoneIDs[u] == kSKIN_BONE_SENTINEL) break;
        uint pi = job.m_uBonePaletteBase + xBoneIDs[u];
        if (pi >= g_xPassSet.SkinConstants.paletteCount) continue;   // defensive (corrupt data)

        float4x4 m = g_xPassSet.bonePalette[pi];
        float    w = xWeights[u];
        xFinalPos += mul(m, float4(xPos, 1.0)) * w;
        xFinalNrm += mul((float3x3)m, xNrm) * w;
        xFinalTan += mul((float3x3)m, xTan.xyz) * w;
        xBlend3   += (float3x3)m * w;
    }

    // det(blend) < 0 = a mirrored blend (negative bone scale): the skinned frame's
    // handedness flipped, so the sign the VS rebuilds the bitangent with flips too.
    // The uncompressed path carried this implicitly by skinning the bitangent VECTOR
    // and re-deriving the sign; the passthrough must keep that parity. A zero det
    // (no influences / singular blend) keeps the authored sign. Mirrors
    // Flux_SkinAttributesCPU in Flux_Skinning.h.
    float fTanW = (determinant(xBlend3) < 0.0) ? -xTan.w : xTan.w;
    float3 xNrmOut = NormaliseOrDefault(xFinalNrm, float3(0.0, 1.0, 0.0));
    float3 xTanOut = NormaliseOrDefault(xFinalTan, float3(1.0, 0.0, 0.0));

    // ---- pack + write the 24-byte static output vertex (raw words) ----
    // Position lane 3 is 1.0 and normal lane 3 is 0.0 because that is what the CPU
    // packer leaves in the lanes its 3-lane sources do not reach — the arena and a
    // CPU-packed static mesh have to be the same bytes.
    uint2 xPosWords = Flux_PackHalf4(float4(xFinalPos.xyz, 1.0));
    uint o = (job.m_uOutVertBase + vx) * kSKIN_OUTPUT_WORDS;
    g_xPassSet.skinnedArena[o + 0u] = xPosWords.x;
    g_xPassSet.skinnedArena[o + 1u] = xPosWords.y;
    g_xPassSet.skinnedArena[o + 2u] = Flux_PackHalf2(xUV);
    g_xPassSet.skinnedArena[o + 3u] = Flux_PackSnorm10_10_10_2(float4(xNrmOut, 0.0));
    g_xPassSet.skinnedArena[o + 4u] = Flux_PackSnorm10_10_10_2(float4(xTanOut, fTanW));
    g_xPassSet.skinnedArena[o + 5u] = Flux_PackUnorm8x4(xCol);
}
