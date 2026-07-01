#pragma once

#if ZENITH_MEMORY_TRACKING_ANY

#include <cstdint>

// -----------------------------------------------------------------------------
// Unified memory accounting.
//
// A single, tier-agnostic view that folds the engine CPU tracker together with the
// external allocators the process touches (Vulkan/VMA VRAM, Jolt, ImGui, GLFW, the
// OS process working set, the tracker's own overhead) into a flat table of uniform
// "sources". Each source is refreshed once per frame by a CAPTURELESS function
// pointer poll callback — so leaf libraries gain NO dependency on the tracker; the
// engine registers free-function polls that pull via the existing g_xEngine
// accessors (see Zenith_Engine.cpp InitialiseRuntimeServices).
//
// This is the single source of truth for the editor Memory panel, the profiler
// Memory tab, the live HUD, and the text/CSV report. Process-level static (the
// allocator's companion — see Core/CLAUDE.md "What Stays Static"); NOT on g_xEngine.
// -----------------------------------------------------------------------------

struct Zenith_MemorySource
{
	const char* m_szName = nullptr;      // static-lifetime label ("VMA VRAM", "Jolt", ...)
	u_int64     m_ulBytes = 0;           // live bytes (written by the poll)
	u_int64     m_ulAllocCount = 0;      // live allocations (written by the poll)
	u_int64     m_ulBudgetBytes = 0;     // 0 = unbudgeted
	bool        m_bIsVRAM = false;       // VRAM (GPU) vs process RAM — never cross-summed
};

// Captureless poll: fills the mutable bytes/count fields of its source in place.
using Zenith_MemorySourcePoll = void(*)(Zenith_MemorySource& xOut);

class Zenith_MemoryAccounting
{
public:
	static void Initialise();

	// Register a pollable source. szName must have static lifetime. Safe to call once
	// per source during engine boot (after Zenith_MemoryManagement::Initialise()).
	static void RegisterSource(const char* szName, Zenith_MemorySourcePoll pfnPoll, u_int64 ulBudgetBytes, bool bIsVRAM);

	// Refresh every source (invoke each poll). Call once per frame from EndFrame().
	static void PollAll();

	// Read access for panel / profiler / report.
	static u_int GetSourceCount();
	static const Zenith_MemorySource& GetSource(u_int uIndex);

	// Summed live bytes across sources, split by domain so VRAM is never added to RAM.
	static u_int64 GetTotalProcessRAM();
	static u_int64 GetTotalVRAM();

private:
	static constexpr u_int uMAX_SOURCES = 32;
	static Zenith_MemorySource     s_axSources[uMAX_SOURCES];
	static Zenith_MemorySourcePoll s_apfnPoll[uMAX_SOURCES];
	static u_int                   s_uCount;
	static bool                    s_bInitialised;
};

#endif // ZENITH_MEMORY_TRACKING_ANY
