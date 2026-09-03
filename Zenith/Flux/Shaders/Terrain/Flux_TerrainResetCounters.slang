// =====================================================================
// Flux_TerrainResetCounters — Slang module.
//
// Subsystem: Terrain. Compute shader that resets a terrain's per-frame
// indirect-draw bookkeeping before the culling dispatch runs:
//   (a) writes visibleCount[0] = 0, and every shadow cascade's count to 0
//   (b) clears every record in the per-terrain CAMERA indirect-command buffer
//       AND every record of every SHADOW-CASCADE slot to the legal no-op
//       (all five words = 0)
//
// Why this exists (and what changed): the previous design did the count-
// only reset via a CPU UploadBufferDataAtOffset issued mid-record, and
// initialised the indirect-command buffer to zero ONCE at Zenith_TerrainComponent
// creation. That host transfer write is invisible to the render graph's
// barrier synthesiser, and the persistent argument buffer was never re-cleared,
// so culling's atomic prefix compaction leaves a STALE TAIL of commands from
// previous frames — a fixed indexed-indirect draw over the full range would
// replay those stale chunks every frame after visibility shrinks
// (many → few → zero ghost chunks).
//
// The fix is the zero-padded-to-max contract: a separate GPU reset pass
// clears the count AND every one of the 4,096 indirect-command records every
// frame, the existing culling shader compacts live commands into [0, N)
// and the cleared tail [N, 4096) stays all-zero no-ops, so a fixed indexed-
// indirect draw over the entire range is valid even when visibility falls
// to zero. The shadow-cascade slots are held to the SAME contract: each is a
// full chunk-capacity range the cull compacts a prefix into, so each is
// cleared whole here, every frame, by the same dispatch. An inactive cascade
// (shadows off) is therefore an empty slot, not a stale one. The render graph
// emits the UAV→UAV barriers against the culling pass automatically (the pass
// is declared as a Writes of these buffers in Flux_Terrain.cpp::SetupRenderGraph).
//
// A separate dispatch (rather than a fold-into-the-cull-shader thread-0 trick)
// is mandatory: there is no device-wide barrier across culling workgroups
// inside one dispatch, so a thread-0 reset would not be visible to the other
// workgroups' atomic increments.
//
// Spine include reserves sets 0/1/2 (GLOBAL/VIEW/BINDLESS) so this pass's own
// resource lands in PassParams at set 3. This compute pass uses none of the
// spine constants — it writes only its own counter + argument buffers — but
// still includes the spine so the persistent set layout stays uniform across
// shaders.
//
// Dispatch shape: ceil(TOTAL_CHUNKS / 64) groups of 64 threads = 64 groups
// for the 4096-chunk shipping grid (4096 = 64 * 64). Each thread clears ONE
// camera record and then strides through the shadow buffer at the camera
// record count, so it clears one record per cascade slot too. Every record
// count is queried from the bound buffers' GetDimensions, NOT embedded in
// the shader — adding a chunk-boundary config constant to the shader would
// silently desync from the allocation size should it ever change.
// =====================================================================

#include "Common/Bindings.slang"

// Shared 20-byte indexed-indirect-command ABI (terrain local include; the C++
// twin is Flux_IndirectDrawIndexedCommand in Flux/Backend/Flux_IndirectDraw.h).
// Both definitions are pinned by a static_assert in Flux_Terrain.cpp.
#include "Terrain/Flux_TerrainIndirectCommon.slang"

struct PassParams
{
	RWStructuredBuffer<uint>                          visibleCount;
	RWStructuredBuffer<Flux_TerrainIndexedIndirectCommand> indirectCommands;
	// Shadow-cascade slots (Flux_TerrainShadowCull.h layout), appended after the
	// camera pair so bindings 0-1 keep their numbers.
	RWStructuredBuffer<uint>                          shadowVisibleCount;
	RWStructuredBuffer<Flux_TerrainIndexedIndirectCommand> shadowIndirectCommands;
};
ParameterBlock<PassParams> g_xPassSet;

[shader("compute")]
[numthreads(64, 1, 1)]
void csMain(uint3 dispatchId : SV_DispatchThreadID)
{
	// Thread 0 zeroes the visible counts. The atomic compaction in
	// Flux_TerrainCulling.slang reads-and-increments these counters, so a
	// stale non-zero value would compound counts across frames and let the
	// compacted prefix overflow [visibleCount, TOTAL_CHUNKS).
	if (dispatchId.x == 0u)
	{
		g_xPassSet.visibleCount[0] = 0u;

		uint uShadowCountCount, uShadowCountStride;
		g_xPassSet.shadowVisibleCount.GetDimensions(uShadowCountCount, uShadowCountStride);
		for (uint c = 0u; c < uShadowCountCount; c++)
		{
			g_xPassSet.shadowVisibleCount[c] = 0u;
		}
	}

	// Each in-range thread writes one fully zero-initialised command. The
	// bounds check reads the argument buffer's dimensions rather than
	// embedding 4,096 in the shader (see the header). A record whose all
	// five words are zero is a legal no-op: indexCount == 0 makes the GPU
	// read no indices and rasterise no fragments, so a fixed indexed-indirect
	// draw over the padded [N, TOTAL_CHUNKS) tail emits only zero-count
	// no-op records — exactly the spec-safe fallback the recorder uses when
	// the device lacks native vkCmdDrawIndexedIndirectCount.
	uint uRecordCount, uStride;
	g_xPassSet.indirectCommands.GetDimensions(uRecordCount, uStride);
	if (dispatchId.x >= uRecordCount) return;

	Flux_TerrainIndexedIndirectCommand xZero;
	xZero.indexCount    = 0u;
	xZero.instanceCount = 0u;
	xZero.firstIndex    = 0u;
	xZero.vertexOffset  = 0;
	xZero.firstInstance = 0u;
	g_xPassSet.indirectCommands[dispatchId.x] = xZero;

	// The shadow slots: one record per cascade slot for this thread. Striding by
	// the CAMERA record count is what makes "one thread per chunk slot" cover
	// every cascade's copy of that slot, and the bound-buffer dimension is the
	// loop's only limit, so the slot count is never spelled here.
	uint uShadowRecordCount, uShadowStride;
	g_xPassSet.shadowIndirectCommands.GetDimensions(uShadowRecordCount, uShadowStride);
	for (uint r = dispatchId.x; r < uShadowRecordCount; r += uRecordCount)
	{
		g_xPassSet.shadowIndirectCommands[r] = xZero;
	}
}
