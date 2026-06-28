// =====================================================================
// Flux_UnifiedMesh_Reset — Slang module (unified GPU-driven opaque-mesh pipeline).
//
// Per-frame GPU reset of the per-(view,bucket) indirect-draw commands, BEFORE the
// cull pass. One thread per (view,bucket) pair: writes the full
// VkDrawIndexedIndirectCommand (indexCount from the bucket's shared mesh, the rest
// zeroed). The cull pass then atomically increments each command's instanceCount.
// Writing every indirect word on device each frame (rather than a host seed) lets
// the render graph synthesise the reset->cull->draw barriers and removes the host
// WRITE_AFTER_READ race.
//
// VIEW dimension (Stage 2): view 0 = camera, views 1..N = the shadow cascades. The
// indirect buffer is one VkDrawIndexedIndirectCommand per (view,bucket), laid out
// view-major: command (v,b) at uint offset (v*numBuckets + b)*5. The index count is
// view-invariant (same mesh), so every view of a bucket gets bucketIndexCount[b].
//
// VkDrawIndexedIndirectCommand layout (uint[5] per command):
// [0]=indexCount, [1]=instanceCount, [2]=firstIndex, [3]=vertexOffset,
// [4]=firstInstance. All five written here; [1] is 0 then cull-incremented.
// =====================================================================

// Spine reserves sets 0/1/2 (GLOBAL/VIEW/BINDLESS); this pass's own resources land
// in PassParams at set 3. Uses none of the spine constants — keeps the layout uniform.
#include "Common/Bindings.slang"

struct ResetConstantsLayout
{
    uint numBuckets;   // bucket slot count this frame (per-view stride)
    uint numViews;     // active views this frame (1 = camera only; 1+cascades with shadows)
    uint pad0;
    uint pad1;
};

struct PassParams
{
    ConstantBuffer<ResetConstantsLayout> ResetConstants;
    RWStructuredBuffer<uint>             bucketIndexCount;  // [numBuckets] per-bucket mesh index count (view-invariant)
    RWStructuredBuffer<uint>             indirect;          // [numViews*numBuckets*5] VkDrawIndexedIndirectCommand
};
ParameterBlock<PassParams> g_xPassSet;

[shader("compute")]
[numthreads(64, 1, 1)]
void csMain(uint3 id : SV_DispatchThreadID)
{
    uint numBuckets = g_xPassSet.ResetConstants.numBuckets;
    uint tid = id.x;
    if (tid >= numBuckets * g_xPassSet.ResetConstants.numViews) return;

    uint v = tid / numBuckets;   // view index (0 = camera, 1.. = cascades)
    uint b = tid - v * numBuckets;

    uint base = (v * numBuckets + b) * 5u;
    g_xPassSet.indirect[base + 0u] = g_xPassSet.bucketIndexCount[b]; // indexCount (view-invariant)
    g_xPassSet.indirect[base + 1u] = 0u;                             // instanceCount (cull increments)
    g_xPassSet.indirect[base + 2u] = 0u;                             // firstIndex
    g_xPassSet.indirect[base + 3u] = 0u;                             // vertexOffset
    g_xPassSet.indirect[base + 4u] = 0u;                             // firstInstance
}
