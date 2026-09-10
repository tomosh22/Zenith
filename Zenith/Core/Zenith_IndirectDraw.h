#pragma once

// Backend-neutral indexed-indirect argument layout.  This belongs to Core
// because terrain authoring, render backends and Flux all exchange these bytes;
// no one renderer owns the ABI.
#include <cstdint>

struct Flux_IndirectDrawIndexedCommand
{
	uint32_t m_uIndexCount;
	uint32_t m_uInstanceCount;
	uint32_t m_uFirstIndex;
	int32_t  m_iVertexOffset;
	uint32_t m_uFirstInstance;
};
static_assert(sizeof(Flux_IndirectDrawIndexedCommand) == 20u,
	"Indexed indirect commands are five 32-bit words.");
static_assert(alignof(Flux_IndirectDrawIndexedCommand) == alignof(uint32_t),
	"Indexed indirect command records must be tightly packable.");

inline constexpr uint32_t uFLUX_INDIRECT_DRAW_INDEXED_WORD_COUNT = 5u;
inline constexpr uint32_t uFLUX_INDIRECT_DRAW_INDEXED_BYTE_STRIDE = sizeof(Flux_IndirectDrawIndexedCommand);
static_assert(uFLUX_INDIRECT_DRAW_INDEXED_BYTE_STRIDE ==
	uFLUX_INDIRECT_DRAW_INDEXED_WORD_COUNT * sizeof(uint32_t));

inline void Flux_ZeroIndirectDrawIndexedCommand(Flux_IndirectDrawIndexedCommand& xOut)
{
	xOut.m_uIndexCount = 0u;
	xOut.m_uInstanceCount = 0u;
	xOut.m_uFirstIndex = 0u;
	xOut.m_iVertexOffset = 0;
	xOut.m_uFirstInstance = 0u;
}
