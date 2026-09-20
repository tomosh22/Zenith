//------------------------------------------------------------------------------
// Zenith_Vulkan_AccessMapping.Tests.inl — Vulkan-only unit tests for the
// Flux_ResourceAccessToVulkan translation engine code uses to build pipeline
// barriers from the neutral ResourceAccess enum (the seam the render graph's
// barrier synthesizer lowers into vk::AccessFlagBits + vk::PipelineStageFlags).
//
// ====================================================================
// HEADLESS-SAFETY. These tests run only in a ZENITH_VULKAN config, because the
// entire Zenith/Vulkan/ directory is compiled out of Null_* builds (the headless
// CI config) and out of D3D12_* builds — see Zenith/Null/CLAUDE.md and
// Zenith/D3D12/CLAUDE.md. The test bodies are pure integer/enum comparisons
// over the translated (layout, access mask, pipeline stage) triple, so they
// require no device and no command buffer; a Vulkan build runs them as part
// of the boot-time unit batch.
//
// HOSTED BY Zenith_Vulkan_CommandBuffer.cpp. The Vulkan access-mapping
// function lives there as `Flux_ResourceAccessToVulkan` in the file-static /
// extern-namespace scope; the test file is `#include`d at the bottom of that
// .cpp so the .inl can name it (and the vk:: enumerators it returns) without
// reaching through any new header. A separate Vulkan-ONLY test file is what
// the design plan calls for ("A separate Vulkan access-mapping test must
// assert that READ_INDIRECT_ARG maps to eIndirectCommandRead at
// eDrawIndirect; do not infer native stage/access values from the neutral
// graph test") — the neutral RenderGraph tests prove the GRAPH emits the
// right ResourceAccess transition edges; this test proves the BACKEND lowers
// each ResourceAccess to the right native (stage, access mask, layout) triple.
// ====================================================================
//------------------------------------------------------------------------------

#include "Core/Zenith_TestFramework.h"

#ifdef ZENITH_TESTING

// The translator lives in the same TU it is being tested from; no extra
// extern declaration needed. It is declared in Zenith_Vulkan_CommandBuffer.h
// so Zenith_Vulkan.cpp can reuse it for aliasing-barrier emission, hence the
// signature here matches the declaration exactly.
//   void Flux_ResourceAccessToVulkan(ResourceAccess eAccess, bool bIsDepth,
//       vk::ImageLayout& eOutLayout, vk::AccessFlags& eOutAccess,
//       vk::PipelineStageFlags& eOutStage);

namespace
{
	struct AccessTriple
	{
		vk::ImageLayout         eLayout;
		vk::AccessFlags         eAccess;
		vk::PipelineStageFlags  eStage;
	};

	AccessTriple Translate(ResourceAccess eAccess, bool bIsDepth = false)
	{
		AccessTriple xOut;
		Flux_ResourceAccessToVulkan(eAccess, bIsDepth, xOut.eLayout, xOut.eAccess, xOut.eStage);
		return xOut;
	}
}

//------------------------------------------------------------------------------
// Phase 6 of the terrain indirect-count compatibility plan — terrain pins:
//   * reset-count UAV write -> culling-count UAV write is UAV WAW;
//   * reset-argument UAV write -> culling-argument UAV write is UAV WAW;
//   * culling-argument UAV write -> GBuffer indirect-arg read maps to the
//     eIndirectCommandRead access at the eDrawIndirect pipeline stage
//     (the GPU command processor reading the AKXVkDrawIndexedIndirectCommand
//     records for vkCmdDrawIndexedIndirect / vkCmdDrawIndexedIndirectCount);
//   * culling-count UAV write -> counted GBuffer indirect-arg read remains
//     correct (since the count buffer is also READ_INDIRECT_ARG, the
//     SAME pipeline stage / access mask lower as the argument buffer's
//     read, so the recorder's barrier preserves the same memory visibility
//     regardless of whether count is natively consumed or ignored).
//------------------------------------------------------------------------------

ZENITH_TEST(VulkanAccessMapping, ReadIndirectArgMapsToIndirectCommandReadAtDrawIndirect)
{
	// The core pin: RESOURCE_ACCESS_READ_INDIRECT_ARG (buffer-only) lowers to
	// eIndirectCommandRead at eDrawIndirect. The render graph synthesizes
	// a buffer memory barrier with this stage/access on every pass that
	// reads an IndirectBuffer (a terrain GBuffer's DrawIndexedIndirectCount
	// call, or a PADDED_MULTI / PADDED_SINGLE fixed-indexed-indirect batch).
	// A drift here would emit a memory barrier against the wrong GPU stage
	// (e.g. eVertexInput, eComputeShader), and the prior-frame culling
	// compute write would not be visible to the command-processor read at
	// draw time -> reading stale indirect args (ghost draws).
	const AccessTriple xArg = Translate(RESOURCE_ACCESS_READ_INDIRECT_ARG, false);
	ZENITH_ASSERT_TRUE(static_cast<bool>(xArg.eAccess & vk::AccessFlagBits::eIndirectCommandRead),
		"READ_INDIRECT_ARG must include eIndirectCommandRead in the access mask");
	ZENITH_ASSERT_TRUE(static_cast<bool>(xArg.eStage & vk::PipelineStageFlagBits::eDrawIndirect),
		"READ_INDIRECT_ARG must include eDrawIndirect in the pipeline stage mask");
	// The argument buffer has no image-layout concept (buffer-only)—the
	// translator must report eUndefined so callers don't try to chain it
	// into an image layout transition.
	ZENITH_ASSERT_EQ(static_cast<VkImageLayout>(xArg.eLayout), static_cast<VkImageLayout>(vk::ImageLayout::eUndefined),
		"READ_INDIRECT_ARG layout must be eUndefined (buffer-only, no layout concept)");
}

ZENITH_TEST(VulkanAccessMapping, WriteUavMapsToShaderWriteAtComputeShader)
{
	// The producer side of the terrain reset/cull edge: WRITE_UAV lowers to
	// eShaderWrite at eComputeShader (the only stage that issues storage
	// writes in this engine). The graph's UAV -> UAV barrier (reset ->
	// culling WAW on the count and argument buffers) and UAV -> READ_INDIRECT_ARG
	// barrier (culling -> GBuffer) both depend on this stage/access being
	// eComputeShader so the prior pass's dispatch is made visible to the
	// next pass's read.
	const AccessTriple xW = Translate(RESOURCE_ACCESS_WRITE_UAV, false);
	ZENITH_ASSERT_TRUE(static_cast<bool>(xW.eAccess & vk::AccessFlagBits::eShaderWrite),
		"WRITE_UAV must include eShaderWrite in the access mask");
	ZENITH_ASSERT_TRUE(static_cast<bool>(xW.eStage & vk::PipelineStageFlagBits::eComputeShader),
		"WRITE_UAV must include eComputeShader in the pipeline stage mask");
	ZENITH_ASSERT_EQ(static_cast<VkImageLayout>(xW.eLayout), static_cast<VkImageLayout>(vk::ImageLayout::eGeneral),
		"WRITE_UAV maps to GENERAL since the producer is a compute dispatch writing a storage buffer");
}

ZENITH_TEST(VulkanAccessMapping, ReadwriteUavMapsToBothShaderReadAndWrite)
{
	// Culling writes validecount with InterlockedAdd (a read-modify-write),
	// so the graph declares RESOURCE_ACCESS_READWRITE_UAV at the culling pass.
	// The translator must lower it to BOTH eShaderRead and eShaderWrite at
	// eComputeShader so the WAW barrier from the reset pass (which used
	// WRITE_UAV's eShaderWrite-only source) is a legal transition into the
	// culling pass's eShaderWrite destination and the prior READ is also
	// guaranteed visible.
	const AccessTriple xRW = Translate(RESOURCE_ACCESS_READWRITE_UAV, false);
	ZENITH_ASSERT_TRUE(static_cast<bool>(xRW.eAccess & vk::AccessFlagBits::eShaderRead),
		"READWRITE_UAV must include eShaderRead in the access mask (_interlockedAdd reads before writing)");
	ZENITH_ASSERT_TRUE(static_cast<bool>(xRW.eAccess & vk::AccessFlagBits::eShaderWrite),
		"READWRITE_UAV must include eShaderWrite in the access mask");
	ZENITH_ASSERT_TRUE(static_cast<bool>(xRW.eStage & vk::PipelineStageFlagBits::eComputeShader),
		"READWRITE_UAV must include eComputeShader in the pipeline stage mask");
}

ZENITH_TEST(VulkanAccessMapping, ArrayBufferSrvMapsToShaderReadAcrossVertexFragmentCompute)
{
	// Culling writes LODLevelBuffer; GBuffer vertex shader samples it as
	// StructuredBuffer<uint> (READ-ONLY SSBO). The translator must union
	// the stages an SSBO can be sampled from: vertex shader (terrain GBuffer
	// consumes it), fragment shader (other consumers of the buffer family),
	// compute shader (storage-bound reads). A drift that only handles one
	// stage produces a barrier that does not make the compute write visible
	// to, say, a vertex shader read -> LOD visualisation flashes garbage.
	const AccessTriple xSRV = Translate(RESOURCE_ACCESS_READ_BUFFER_SRV, false);
	ZENITH_ASSERT_TRUE(static_cast<bool>(xSRV.eAccess & vk::AccessFlagBits::eShaderRead),
		"READ_BUFFER_SRV must include eShaderRead in the access mask");
	ZENITH_ASSERT_TRUE(static_cast<bool>(xSRV.eStage & vk::PipelineStageFlagBits::eVertexShader),
		"READ_BUFFER_SRV must include eVertexShader (terrain GBuffer VS samples LODLevelBuffer)");
	ZENITH_ASSERT_TRUE(static_cast<bool>(xSRV.eStage & vk::PipelineStageFlagBits::eFragmentShader),
		"READ_BUFFER_SRV must include eFragmentShader (other buffers in the family)");
	ZENITH_ASSERT_TRUE(static_cast<bool>(xSRV.eStage & vk::PipelineStageFlagBits::eComputeShader),
		"READ_BUFFER_SRV must include eComputeShader (storage-bound compute reads)");
	ZENITH_ASSERT_EQ(static_cast<VkImageLayout>(xSRV.eLayout), static_cast<VkImageLayout>(vk::ImageLayout::eUndefined),
		"READ_BUFFER_SRV layout must be eUndefined (buffer-only)");
}

#endif // ZENITH_TESTING