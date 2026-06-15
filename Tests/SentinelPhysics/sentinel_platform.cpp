#include "Zenith.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "Windows/Multithreading/Zenith_Windows_Multithreading.h"

// L0 platform contract for the Physics leaf
// ========================================
// ZenithPhysics + ZenithECS + ZenithBase reference a small, fixed set of platform
// symbols whose real implementations live in the Windows/ platform layer (compiled
// into the engine aggregate, NOT into any leaf lib): Zenith_DebugBreak (assert
// hook), three Zenith_FileAccess entry points (Zenith_DataStream file I/O the
// scene loader can call — pulled in via the linked zenithecs.lib), and
// Zenith_Windows_Mutex_T<true>::Lock (the profiling Zenith_Mutex behind
// Zenith_EventDispatcher's deferred-event queue in zenithecs.lib). The Physics
// layer itself adds NO new platform symbol: its deferred-collision queue uses the
// non-profiling Zenith_Mutex (inline Lock), and the Jolt backend it owns is
// self-contained inside zenithphysics.lib.
//
// Their presence here is the EXPLICIT, COMPLETE enumeration of the leaf chain's
// platform dependency floor. Note what is NOT here: no g_xEngine, no Flux_*, no
// AI, no concrete Zenith_*Component. JPH:: is resolved from within
// zenithphysics.lib (Physics OWNS Jolt), not shimmed. If the Physics core ever
// grew an engine dependency it would surface as a NEW unresolved external at this
// link (or in the dumpbin UNDEF scan that backs this proof) — never silently.

void Zenith_DebugBreak()
{
	__debugbreak();
}

namespace Zenith_FileAccess
{
	char* ReadFile(const char* /*szFilename*/, uint64_t& ulSize)
	{
		ulSize = 0u;
		return nullptr;
	}

	void WriteFile(const char* /*szFilename*/, const void* const /*pData*/, const uint64_t /*ulSize*/)
	{
	}

	bool FileExists(const char* /*szFilename*/)
	{
		return false;
	}
}

// Profiling-free Lock for the profiling Zenith_Mutex (the REAL impl instruments via
// g_xEngine.Profiling(), an ENGINE symbol). Correct for the single-threaded
// sentinel and keeps the link engine-free. Mirrors Tests/SentinelECS.
template<>
void Zenith_Windows_Mutex_T<true>::Lock()
{
	EnterCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(m_axCriticalSectionStorage));
}

// The Physics deferred-collision queue uses Zenith_Mutex_NoProfiling
// (= Zenith_Windows_Mutex_T<false>). Its ctor/dtor/Lock/Unlock are out-of-line
// (the impl moved out of the header so <Windows.h> can leave the PCH); the REAL
// impl is in Zenith_Windows_Multithreading.cpp, which we do NOT link (it would drag
// g_xEngine.Profiling in). The <false> specialization does no profiling anyway, so
// these are exactly the correct definitions and keep the link engine-free. They are
// L0 platform primitives — part of the documented dependency floor, NOT an engine
// coupling (no g_xEngine / Flux / AI / concrete component appears).
template<>
Zenith_Windows_Mutex_T<false>::Zenith_Windows_Mutex_T()
{
	InitializeCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(m_axCriticalSectionStorage));
}
template<>
Zenith_Windows_Mutex_T<false>::~Zenith_Windows_Mutex_T()
{
	DeleteCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(m_axCriticalSectionStorage));
}
template<>
void Zenith_Windows_Mutex_T<false>::Lock()
{
	EnterCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(m_axCriticalSectionStorage));
}
template<>
void Zenith_Windows_Mutex_T<false>::Unlock()
{
	LeaveCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(m_axCriticalSectionStorage));
}
