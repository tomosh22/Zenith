// =====================================================================
// Flux_Grass_IndirectFixup — Slang module (GPU-driven grass).
//
// Runs between the placement pass and the draws. The placement pass reserves each
// survivor's output slot with an unconditional InterlockedAdd on the command's
// instanceCount, so a frame that overshoots a partition leaves that count ABOVE
// the partition's capacity even though the out-of-range appends were dropped.
// Drawing that count would read past the end of the partition into the next one.
//
// This pass clamps each live slot's instanceCount to its capacity, which is the
// point at which an over-budget frame becomes "the far blades are missing" rather
// than "the cascade draws the camera partition's blades". Reserved slots are
// forced to zero instances.
// =====================================================================

#include "Vegetation/Flux_GrassCommon.slang"   // spine sets 0/1/2 + the pinned partition caps

// PASS block (set 3). No constants: the caps are pinned by Flux_GrassCommon.
struct PassParams
{
	RWStructuredBuffer<uint> IndirectArgs;   // [kGRASS_INDIRECT_SLOT_COUNT * kGRASS_INDIRECT_WORDS]
};
ParameterBlock<PassParams> g_xPassSet;

// ============================================================================
// Clamp each grass indirect command's instanceCount to its partition capacity.
// Workgroup: 16x1x1 (16 threads — one per indirect slot, a single group)
// Shared memory: none
//
// One thread per slot; live slots clamp against kauGRASS_PARTITION_CAP, reserved
// slots are zeroed.
// ============================================================================
[shader("compute")]
[numthreads(16, 1, 1)]
void csMain(uint3 xDispatchId : SV_DispatchThreadID)
{
	uint uSlot = xDispatchId.x;
	if (uSlot >= kGRASS_INDIRECT_SLOT_COUNT) return;

	uint uCountWord = uSlot * kGRASS_INDIRECT_WORDS + 1u;
	if (uSlot >= kGRASS_LIVE_SLOT_COUNT)
	{
		g_xPassSet.IndirectArgs[uCountWord] = 0u;
		return;
	}

	uint uCap = kauGRASS_PARTITION_CAP[uSlot];
	g_xPassSet.IndirectArgs[uCountWord] = min(g_xPassSet.IndirectArgs[uCountWord], uCap);
}
