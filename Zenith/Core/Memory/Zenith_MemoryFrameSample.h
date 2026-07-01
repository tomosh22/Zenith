#pragma once

// Leaf POD snapshot of memory state for one frame. Deliberately dependency-free (no
// category enum, no allocator header) so the architecture-layer-0 profiler can
// include it without an up-edge onto the memory module. ZENITH_MEM_CAT_MAX is a
// fixed ceiling >= the live MEMORY_CATEGORY_COUNT (enforced by a static_assert in
// Zenith_MemoryManagement.cpp); m_uCategoryCount is how many entries are valid.
//
// Produced once per frame by Zenith_MemoryManagement::SampleFrame() (a pure counter
// read — never allocates), consumed by the profiler Memory tab, the HUD, and the
// text/CSV report. Assumes Zenith.h (u_int / u_int64 aliases) is already included,
// which is guaranteed by the precompiled-header contract.
static constexpr u_int ZENITH_MEM_CAT_MAX = 32u;

struct Zenith_MemoryFrameSample
{
	u_int64 m_ulTotalBytes = 0;             // live tracked bytes
	u_int64 m_ulPeakBytes = 0;              // peak live tracked bytes
	u_int64 m_ulTotalAllocations = 0;       // live tracked allocation count
	int64_t m_ilFrameDeltaBytes = 0;        // signed bytes delta this frame (FULL only; 0 at LITE)
	u_int   m_uFrameAllocations = 0;        // allocs this frame (FULL only; 0 at LITE)
	u_int   m_uFrameDeallocations = 0;      // frees this frame  (FULL only; 0 at LITE)
	u_int   m_uCategoryCount = 0;           // valid entries in the per-category arrays
	u_int64 m_aulCategoryBytes[ZENITH_MEM_CAT_MAX] = {};
	u_int64 m_aulCategoryPeak[ZENITH_MEM_CAT_MAX] = {};
	u_int64 m_aulCategoryCount[ZENITH_MEM_CAT_MAX] = {};
};
