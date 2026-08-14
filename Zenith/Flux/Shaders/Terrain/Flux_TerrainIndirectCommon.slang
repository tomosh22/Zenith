// =====================================================================
// Flux_TerrainIndirectCommon — shared Slang include for the indexed-
// indirect-command ABI used by the terrain reset + culling shaders.
//
// TEXTUAL #include (NOT `import`): the host shader's parameter declaration
// order matters (the spine ParameterBlocks must be declared FIRST), so this
// file is a pure declaration block, not a module. The host shader does:
//
//     #include "Common/Bindings.slang"          // spine sets 0/1/2
//     #include "Terrain/Flux_TerrainIndirectCommon.slang"  // this file
//     struct PassParams { RWStructuredBuffer<Flux_TerrainIndexedIndirectCommand> ...; }
//
// The C++ twin is Flux_IndirectDrawIndexedCommand in
// Flux/Backend/Flux_IndirectDraw.h, which carries the same five 32-bit words
// at the same byte stride (20). The CPU static_assert pins the layout; the
// GPU half (this file) MUST keep field ORDER and TYPE byte-for-byte matching.
// A drift reads every field after it at the wrong offset and produces
// plausible garbage rather than a crash, so the assertion is load-bearing.
//
//   word 0: indexCount    (uint32_t)
//   word 1: instanceCount (uint32_t)
//   word 2: firstIndex    (uint32_t)
//   word 3: vertexOffset  (int32_t — SIGNED, per VkDrawIndexedIndirectCommand)
//   word 4: firstInstance (uint32_t)
//
// vertexOffset is int32_t on purpose — the Slang compiler must not promote it
// to uint32_t. The culling shader writes (int)selectedLOD.vertexOffset into
// it because the GPU's indexed-indirect command reads it as a signed base
// vertex offset, and a silent uint promotion would wrap negative offsets to
// large positives and sample the wrong LOD region.
//
// The stride/word-count constants here match the C++ side
// (uFLUX_INDIRECT_DRAW_INDEXED_BYTE_STRIDE / uFLUX_INDIRECT_DRAW_INDEXED_WORD_COUNT)
// by value; a dedicated `static_assert` in Flux_Terrain.cpp pins both halves
// to the same numbers, since Slang constants are not reachable from C++.
// =====================================================================

#ifndef FLUX_TERRAIN_INDIRECT_COMMON_SLANG
#define FLUX_TERRAIN_INDIRECT_COMMON_SLANG

// 20-byte indexed-indirect command ABI — see the header doc above.
struct Flux_TerrainIndexedIndirectCommand
{
	uint indexCount;
	uint instanceCount;
	uint firstIndex;
	int  vertexOffset;   // SIGNED, per VkDrawIndexedIndirectCommand
	uint firstInstance;
};

// Named constants mirror the C++ side
// (uFLUX_INDIRECT_DRAW_INDEXED_WORD_COUNT / uFLUX_INDIRECT_DRAW_INDEXED_BYTE_STRIDE
// in Flux/Backend/Flux_IndirectDraw.h). Kept literal here because Slang has no
// path to a C++ header; the cross-language pin lives in Flux_Terrain.cpp's
// static_asserts.
static const uint uFLUX_TERRAIN_INDIRECT_WORD_COUNT      = 5u;
static const uint uFLUX_TERRAIN_INDIRECT_BYTE_STRIDE     = 20u;
static const uint uFLUX_TERRAIN_TOTAL_CHUNKS             = 4096u;

#endif // FLUX_TERRAIN_INDIRECT_COMMON_SLANG