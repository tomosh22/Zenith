// =====================================================================
// Flux_InstanceReset — Slang module.
//
// Subsystem: InstancedMeshes. Per-frame GPU reset of the culling output
// counters AND the indirect-draw command's static fields. Runs as a
// single-thread compute dispatch BEFORE the culling pass each frame:
// writes the full VkDrawIndexedIndirectCommand (indexCount from the
// per-group draw constants, the rest zeroed) and zeroes the running
// visible-instance counter. The culling pass then atomically increments
// instanceCount.
//
// This replaces the old out-of-band host UploadBufferData of BOTH the
// counter reset AND the indirect static-field seed. The host seed was
// safe at init but a runtime SetMesh swap would race the prior frame's
// DrawIndexedIndirect read of the same persistent buffer
// (WRITE_AFTER_READ). Writing every indirect word on device each frame,
// from a graph-tracked reset compute pass, lets the render graph
// synthesise the reset->cull->draw barriers automatically and removes
// the host race entirely.
//
// VkDrawIndexedIndirectCommand layout (uint[5]):
// [0]=indexCount, [1]=instanceCount, [2]=firstIndex, [3]=vertexOffset,
// [4]=firstInstance. All five are written here every frame; [1]
// (instanceCount) is set to 0 and then atomically incremented by the
// culling pass.
// =====================================================================

// Spine include reserves sets 0/1/2 (GLOBAL/VIEW/BINDLESS) so this pass's own
// resources land in PassParams at set 3. This compute pass uses none of the
// spine constants — it writes only its own counter/indirect buffers — but still
// includes the spine so the persistent set layout stays uniform across shaders.
#include "Common/Bindings.slang"

// Per-group draw constants: the mesh index count for the indirect draw.
// Bound via Flux_ShaderBinder::BindDrawConstants (per-frame scratch UBO),
// mirroring the GBuffer/shadow DrawConstants binding. Only indexCount is
// read; the remaining words pad it out so the bound payload is a tidy
// 16-byte block.
struct ResetDrawConstantsLayout
{
	uint indexCount;
	uint pad0;
	uint pad1;
	uint pad2;
};

struct PassParams
{
	ConstantBuffer<ResetDrawConstantsLayout> DrawConstants;
	RWStructuredBuffer<uint>                 visibleCount;
	RWStructuredBuffer<uint>                 indirectInstanceCount;
};
ParameterBlock<PassParams> g_xPassSet;

[shader("compute")]
[numthreads(1, 1, 1)]
void csMain(uint3 id : SV_DispatchThreadID)
{
	g_xPassSet.visibleCount[0] = 0u;

	// Full VkDrawIndexedIndirectCommand write (all five words every frame).
	g_xPassSet.indirectInstanceCount[0] = g_xPassSet.DrawConstants.indexCount; // indexCount
	g_xPassSet.indirectInstanceCount[1] = 0u;                       // instanceCount (culling increments)
	g_xPassSet.indirectInstanceCount[2] = 0u;                       // firstIndex
	g_xPassSet.indirectInstanceCount[3] = 0u;                       // vertexOffset
	g_xPassSet.indirectInstanceCount[4] = 0u;                       // firstInstance
}
