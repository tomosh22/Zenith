#include "Zenith.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "Windows/Multithreading/Zenith_Windows_Multithreading.h"
#include "Profiling/Zenith_Profiling.h"

// L0 platform + infra contract for the AI leaf
// ===========================================
// ZenithAI + ZenithPhysics + ZenithECS + ZenithBase reference a small, fixed set of
// platform / infra symbols whose real implementations live engine-side (compiled
// into the aggregate, NOT into any leaf lib) and would drag g_xEngine in:
//   * Zenith_DebugBreak (assert hook)
//   * three Zenith_FileAccess fns (Zenith_DataStream file I/O, via zenithecs.lib)
//   * Zenith_Windows_Mutex_T<true>::Lock (profiling mutex behind Zenith_EventDispatcher
//     in zenithecs.lib) + the <false> non-profiling mutex (Physics deferred queue)
//   * Zenith_Profiling_Detail::BeginProfile/EndProfile (the bridge behind
//     Zenith_Profiling::Scope, which the AI core uses for per-stage timing; the REAL
//     impl in Zenith_Profiling.cpp reaches g_xEngine.Profiling()).
//
// These are the EXPLICIT, COMPLETE platform/infra floor of the AI leaf chain. Note
// what is NOT here: no g_xEngine, no Flux_*, no concrete Zenith_*Component. The AI
// core reaches the engine ONLY through the Zenith_AIWorldHooks function-pointer seam
// (null here -> safe no-ops). JPH:: / Zenith_Physics / ECS undefs resolve from the
// linked sibling leaf libs. If the AI core ever grew an engine dependency it would
// surface as a NEW unresolved external at this link (or the dumpbin UNDEF scan).

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

// Profiling bridge — no-op shims. The real impl (Zenith_Profiling.cpp) routes
// through g_xEngine.Profiling(); the AI core only needs the markers to be callable.
namespace Zenith_Profiling_Detail
{
	void BeginProfile(Zenith_ProfileIndex /*eIndex*/, const char* /*szLabel*/)
	{
	}

	void EndProfile(Zenith_ProfileIndex /*eIndex*/)
	{
	}
}

// Profiling-free mutex Locks (the real impls instrument via g_xEngine.Profiling()).
template<>
void Zenith_Windows_Mutex_T<true>::Lock()
{
	EnterCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(m_axCriticalSectionStorage));
}

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
