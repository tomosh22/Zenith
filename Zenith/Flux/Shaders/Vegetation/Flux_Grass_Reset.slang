// =====================================================================
// Flux_Grass_Reset — Slang module (GPU-driven grass).
//
// Per-frame reset of the 16 grass indirect-draw commands + the blade-pool append
// counter, BEFORE the placement pass. Writing every indirect word on device each
// frame (rather than seeding from the host) lets the render graph synthesise the
// reset -> placement -> draw barriers and removes the host WRITE_AFTER_READ race.
//
// Slot 0 draws the HI mesh (33 indices at firstIndex 0); slots 1-3 draw the LO
// mesh (15 indices at firstIndex 33). firstInstance carries that slot's base
// offset into the visible-index buffer, so the vertex stage recovers its partition
// from SV_StartInstanceLocation and no per-slot constant is needed downstream.
// Slots 4-15 are seeded with indexCount 0, which makes an accidental draw from a
// reserved slot a no-op rather than a read of stale instance counts.
// =====================================================================

#include "Vegetation/Flux_GrassCommon.slang"   // spine sets 0/1/2 + the pinned partition contract

// PASS block (set 3). No constants: every value written here is pinned by
// Flux_GrassCommon, so there is nothing the host can stage incorrectly.
struct PassParams
{
	RWStructuredBuffer<uint> IndirectArgs;   // [kGRASS_INDIRECT_SLOT_COUNT * kGRASS_INDIRECT_WORDS]
	RWStructuredBuffer<uint> BladeCounter;   // [1] blade-pool append cursor
};
ParameterBlock<PassParams> g_xPassSet;

// ============================================================================
// Seed the grass indirect-args block + zero the blade-pool cursor.
// Workgroup: 16x1x1 (16 threads — one per indirect slot, a single group)
// Shared memory: none
//
// Each thread writes one whole VkDrawIndexedIndirectCommand. Thread 0 additionally
// zeroes the blade-pool append counter, which the placement pass then drives with
// InterlockedAdd.
// ============================================================================
[shader("compute")]
[numthreads(16, 1, 1)]
void csMain(uint3 xDispatchId : SV_DispatchThreadID)
{
	uint uSlot = xDispatchId.x;
	if (uSlot >= kGRASS_INDIRECT_SLOT_COUNT) return;

	if (uSlot == 0u) g_xPassSet.BladeCounter[0] = 0u;

	uint uIndexCount    = 0u;
	uint uFirstIndex    = 0u;
	uint uFirstInstance = 0u;
	if (uSlot < kGRASS_LIVE_SLOT_COUNT)
	{
		bool bHiMesh   = (uSlot == kGRASS_SLOT_CAMERA_HI);
		uIndexCount    = bHiMesh ? kGRASS_HI_INDEX_COUNT : kGRASS_LO_INDEX_COUNT;
		uFirstIndex    = bHiMesh ? kGRASS_HI_FIRST_INDEX : kGRASS_LO_FIRST_INDEX;
		uFirstInstance = kauGRASS_PARTITION_BASE[uSlot];
	}

	uint uBase = uSlot * kGRASS_INDIRECT_WORDS;
	g_xPassSet.IndirectArgs[uBase + 0u] = uIndexCount;
	g_xPassSet.IndirectArgs[uBase + 1u] = 0u;              // instanceCount — the placement pass increments it
	g_xPassSet.IndirectArgs[uBase + 2u] = uFirstIndex;
	g_xPassSet.IndirectArgs[uBase + 3u] = 0u;              // vertexOffset — there is no vertex buffer
	g_xPassSet.IndirectArgs[uBase + 4u] = uFirstInstance;
}
