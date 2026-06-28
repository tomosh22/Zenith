// =====================================================================
// Flux_UnifiedMesh_Reset — Slang module (unified GPU-driven opaque-mesh pipeline).
//
// Per-frame GPU reset of the per-bucket indirect-draw commands, BEFORE the cull
// pass. One thread per BUCKET: writes the full VkDrawIndexedIndirectCommand
// (indexCount from the bucket's shared mesh, the rest zeroed). The cull pass then
// atomically increments each bucket's instanceCount. Writing every indirect word
// on device each frame (rather than a host seed) lets the render graph synthesise
// the reset->cull->draw barriers and removes the host WRITE_AFTER_READ race.
//
// VkDrawIndexedIndirectCommand layout (uint[5] per bucket):
// [0]=indexCount, [1]=instanceCount, [2]=firstIndex, [3]=vertexOffset,
// [4]=firstInstance. All five written here; [1] is 0 then cull-incremented.
// =====================================================================

// Spine reserves sets 0/1/2 (GLOBAL/VIEW/BINDLESS); this pass's own resources land
// in PassParams at set 3. Uses none of the spine constants — keeps the layout uniform.
#include "Common/Bindings.slang"

struct ResetConstantsLayout
{
    uint numBuckets;   // valid bucket count this frame (threads beyond it early-out)
    uint pad0;
    uint pad1;
    uint pad2;
};

struct PassParams
{
    ConstantBuffer<ResetConstantsLayout> ResetConstants;
    RWStructuredBuffer<uint>             bucketIndexCount;  // [numBuckets] per-bucket mesh index count
    RWStructuredBuffer<uint>             indirect;          // [numBuckets*5] VkDrawIndexedIndirectCommand
};
ParameterBlock<PassParams> g_xPassSet;

[shader("compute")]
[numthreads(64, 1, 1)]
void csMain(uint3 id : SV_DispatchThreadID)
{
    uint b = id.x;
    if (b >= g_xPassSet.ResetConstants.numBuckets) return;

    uint base = b * 5u;
    g_xPassSet.indirect[base + 0u] = g_xPassSet.bucketIndexCount[b]; // indexCount
    g_xPassSet.indirect[base + 1u] = 0u;                             // instanceCount (cull increments)
    g_xPassSet.indirect[base + 2u] = 0u;                             // firstIndex
    g_xPassSet.indirect[base + 3u] = 0u;                             // vertexOffset
    g_xPassSet.indirect[base + 4u] = 0u;                             // firstInstance
}
