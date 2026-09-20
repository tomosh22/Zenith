#include "Zenith.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "Windows/Multithreading/Zenith_Windows_Multithreading.h"
#include "Core/Callstack/Zenith_Callstack.h"
#include "Profiling/Zenith_Profiling.h"

// CRITICAL_SECTION + Enter/Leave/Initialize/DeleteCriticalSection for the mutex
// shims below. Per the W5.2 note in Core/Zenith_Win32.h, <Windows.h> is NOT in the
// PCH; this TU used to get it transitively via vulkan.hpp (pulled by Flux.h), so
// only the Null_ / D3D12_ configs -- which include no Vulkan -- ever failed here.
// Always the guarded wrapper, never a raw <Windows.h> (APIENTRY / LEAN_AND_MEAN).
#include "Core/Zenith_Win32.h"

// L0 platform + infra contract for the AI leaf
// ===========================================
// ZenithAI + ZenithPhysics + ZenithECS + ZenithCore reference a small, fixed set of
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

// Profiling bridge -- no-op shims for the string-zone API (the profiler overhaul
// replaced the old Zenith_ProfileIndex enum markers). The real impl
// (Zenith_Profiling.cpp) routes through g_xEngine.Profiling(); the AI core only
// needs the zone markers to be callable.
namespace Zenith_Profiling_Detail
{
	u_int RegisterZone(const char* /*szName*/)
	{
		return 0u;
	}

	void BeginProfileZone(u_int /*uZone*/, const char* /*szName*/)
	{
	}

	void EndProfileZone(u_int /*uZone*/)
	{
	}
}

// Profiling-free mutex Locks (the real impls instrument via g_xEngine.Profiling()).
//
// Zenith_Windows_Multithreading.cpp static_asserts the header's opaque byte storage
// against the real CRITICAL_SECTION; that TU is deliberately NOT linked here, so
// re-assert it -- otherwise the reinterpret_casts below are unchecked.
static_assert(sizeof(CRITICAL_SECTION) <= 64, "m_axCriticalSectionStorage is too small for CRITICAL_SECTION");
static_assert(alignof(CRITICAL_SECTION) <= 8, "m_axCriticalSectionStorage alignment is too weak for CRITICAL_SECTION");

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

// Unit-test + callstack seams of the L0 memory system -- REMOVED (Phase 4a).
// ====================================================================
// This TU used to stub Zenith_TestRunner::Instance/RegisterTest/AssertTrue/
// AssertFalse/AssertNotNull and Zenith_Callstack::Initialise/Capture/
// FormatCallstack here, because their real implementations
// (Core/Zenith_TestFramework.cpp, Core/Callstack/) used to live ONLY in the
// aggregate, which this sentinel never links. Phase 4a dissolved ZenithBase
// and folded ALL of Zenith/Core (including Callstack/ and the top-level
// Zenith_TestFramework.cpp) into ZenithCoreLibProject, which this sentinel
// now links directly (see Sharpmake_SentinelAI.cs's mandatory ZenithCore
// dependency). The REAL implementations are linked now, so redefining them
// here would be a duplicate-symbol link error (LNK2005) -- do not re-add
// these definitions. Neither file references g_xEngine / Flux_ (verified),
// so linking them keeps the leaf-proof engine-free.
//
// They DO, however, reach a lower layer of PLATFORM hooks that still live
// only in Zenith/Windows (compiled into the aggregate, never linked here):
// Zenith_PlatformStdio::OpenFile (Zenith_TestRunner::RunAllTests writes a
// results file -- unreachable at runtime here, but still an external the
// whole Zenith_TestFramework.obj drags in once anything else in that .obj is
// referenced), the assert-capture atomics (declared extern in
// Core/Zenith_DebugBreak.h, defined in Zenith/Windows/Zenith_Windows_DebugBreak.cpp
// / Zenith/UnitTests/Zenith_AssertCapture.cpp, neither linked here), and
// Zenith_Callstack::Platform_Initialise/Platform_Shutdown/Platform_Capture/
// Platform_Symbolicate (defined in Zenith/Windows/Callstack/
// Zenith_Windows_Callstack.cpp, likewise not linked here). These four are the
// new, lower floor of the same documented platform-dependency contract this
// file has always stubbed -- trivial no-op definitions, since the sentinel
// never runs unit tests or captures a real callstack.
#include "Core/Zenith_PlatformStdio.h"
#include "Core/Zenith_DebugBreak.h"

std::atomic<uint32_t> g_uAssertCaptureHitCount{ 0 };
std::atomic<bool> g_bAssertCaptureActive{ false };

namespace Zenith_PlatformStdio
{
	FILE* OpenFile(const char* /*szPath*/, const char* /*szMode*/)
	{
		return nullptr;
	}
}

void Zenith_Callstack::Platform_Initialise()
{
}

void Zenith_Callstack::Platform_Shutdown()
{
}

u_int Zenith_Callstack::Platform_Capture(void** /*apFrames*/, u_int /*uMaxFrames*/, u_int /*uSkipFrames*/)
{
	return 0u;
}

bool Zenith_Callstack::Platform_Symbolicate(void* /*pAddress*/, Zenith_CallstackFrame& /*xFrameOut*/)
{
	return false;
}
