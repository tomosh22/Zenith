#include "Zenith.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "Windows/Multithreading/Zenith_Windows_Multithreading.h"
#include "Core/Callstack/Zenith_Callstack.h"

// CRITICAL_SECTION + Enter/Leave/Initialize/DeleteCriticalSection for the mutex
// shims below. Per the W5.2 note in Core/Zenith_Win32.h, <Windows.h> is NOT in the
// PCH; this TU used to get it transitively via vulkan.hpp (pulled by Flux.h), so
// only the Null_ / D3D12_ configs -- which include no Vulkan -- ever failed here.
// Always the guarded wrapper, never a raw <Windows.h> (APIENTRY / LEAN_AND_MEAN).
#include "Core/Zenith_Win32.h"

// L0 platform contract for the Physics leaf
// ========================================
// ZenithPhysics + ZenithECS + ZenithCore reference a small, fixed set of platform
// symbols whose real implementations live in the Windows/ platform layer (compiled
// into the engine aggregate, NOT into any leaf lib): Zenith_DebugBreak (assert
// hook), three Zenith_FileAccess entry points (Zenith_DataStream file I/O the
// scene loader can call — pulled in via the linked zenithecs.lib), and
// Zenith_Windows_Mutex_T<true>::Lock (the profiling Zenith_Mutex behind
// Zenith_EventDispatcher's deferred-event queue in zenithecs.lib). The Physics
// layer itself adds NO new platform symbol: its deferred-collision queue uses the
// non-profiling Zenith_Mutex (inline Lock). As of Phase 4c the Jolt backend is
// no longer inside zenithphysics.lib at all — it is the separate ZenithJolt leaf
// (zenithjolt.lib, linked as a declared public dependency; see
// Build/Sharpmake_SentinelPhysics.cs), so it adds no platform symbol here either:
// Jolt's own sources never reference this file's shims.
//
// Their presence here is the EXPLICIT, COMPLETE enumeration of the leaf chain's
// platform dependency floor. Note what is NOT here: no g_xEngine, no Flux_*, no
// AI, no concrete Zenith_*Component. JPH:: is resolved from zenithjolt.lib (a
// declared leaf dependency, not shimmed here or owned by ZenithPhysics anymore
// — see the Phase 4c Jolt split in Build/Sharpmake_ZenithJolt.cs). If the
// Physics core ever grew an engine dependency it would surface as a NEW
// unresolved external at this link (or in the dumpbin UNDEF scan that backs
// this proof) — never silently.

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

// Unit-test + callstack seams of the L0 memory system -- REMOVED (Phase 4a).
// ====================================================================
// This TU used to stub Zenith_TestRunner::Instance/RegisterTest/AssertTrue and
// Zenith_Callstack::Initialise/Capture/FormatCallstack here, because their real
// implementations (Core/Zenith_TestFramework.cpp, Core/Callstack/) used to live
// ONLY in the aggregate, which this sentinel never links. Phase 4a dissolved
// ZenithBase and folded ALL of Zenith/Core (including Callstack/ and the
// top-level Zenith_TestFramework.cpp) into ZenithCoreLibProject, which this
// sentinel now links directly (see Sharpmake_SentinelPhysics.cs's mandatory
// ZenithCore dependency). The REAL implementations are linked now, so
// redefining them here would be a duplicate-symbol link error (LNK2005) --
// do not re-add these definitions. Neither file references g_xEngine / Flux_
// (verified), so linking them keeps the leaf-proof engine-free.
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
