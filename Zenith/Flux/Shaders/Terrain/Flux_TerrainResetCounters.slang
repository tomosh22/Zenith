// =====================================================================
// Flux_TerrainResetCounters — Slang module.
//
// Subsystem: Terrain. Single-thread compute that zeroes a terrain's
// per-frame visible-count buffer before the culling dispatch reads it.
//
// Why this exists: the previous design did the reset via a CPU
// UploadBufferDataAtOffset issued mid-record. That host transfer write
// is invisible to the render graph's barrier synthesiser, so the
// graph emits no buffer-memory barrier for it — the compute consumer
// could read stale bytes. Moving the reset onto the GPU lets the
// graph track it as a normal pass write and emit the UAV→UAV barrier
// against the culling pass automatically.
//
// One thread, one buffer write — the whole point is that a separate
// dispatch (rather than a fold-into-the-cull-shader thread-0 trick)
// is the only way to get cross-workgroup memory visibility for the
// follow-up dispatch's atomic increments.
//
// Spine include reserves sets 0/1/2 (GLOBAL/VIEW/BINDLESS) so this pass's own
// resource lands in PassParams at set 3. This compute pass uses none of the spine
// constants — it writes only its own counter buffer — but still includes the
// spine so the persistent set layout stays uniform across shaders.
// =====================================================================

#include "Common/Bindings.slang"

struct PassParams
{
	RWStructuredBuffer<uint> visibleCount;
};
ParameterBlock<PassParams> g_xPassSet;

[shader("compute")]
[numthreads(1, 1, 1)]
void csMain()
{
	g_xPassSet.visibleCount[0] = 0u;
}
