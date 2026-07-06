#include "Zenith.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "Windows/Multithreading/Zenith_Windows_Multithreading.h"
#include "Core/Callstack/Zenith_Callstack.h"

// L0 platform contract for the ECS leaf
// =====================================
// ZenithECS + ZenithBase reference a small, fixed set of platform symbols whose
// real implementations live in the Windows/ platform layer (compiled into the
// engine aggregate, NOT into either leaf lib): Zenith_DebugBreak (assert hook),
// three Zenith_FileAccess entry points (Zenith_DataStream file I/O the scene
// loader can call), and Zenith_Windows_Mutex_T<true>::Lock (the profiling
// Zenith_Mutex behind Zenith_EventDispatcher's deferred-event queue, pulled in
// once 7b-2 routed the scene-lifecycle Fire* notifications through the dispatcher).
// The sentinel exercises only in-memory ECS paths (CreateScene / CreateEntityBare
// / AddComponent / Query / DestroyImmediate) and never touches the disk, so
// trivial definitions suffice.
//
// Their presence here is the EXPLICIT, COMPLETE enumeration of the leaf's platform
// dependency floor. Note what is NOT here: no g_xEngine, no Flux_*, no JPH:: /
// Zenith_Physics, no AI, no concrete Zenith_*Component. If the ECS core ever grew
// an engine dependency it would surface as a NEW unresolved external at this link
// (or in the dumpbin UNDEF scan that backs this proof) -- never silently.

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

// Zenith_Windows_Mutex_T<true> is the profiling Zenith_Mutex used by
// Zenith_EventDispatcher (ProcessDeferredEvents locks it). Lock() is the ONLY
// out-of-line member (ctor/dtor/TryLock/Unlock are inline in the header). The REAL
// impl (Windows/Multithreading/Zenith_Windows_Multithreading.cpp) instruments via
// g_xEngine.Profiling() -- an ENGINE symbol -- so linking that platform TU would
// drag the whole engine in and defeat the leaf proof. A profiling-free Lock (just
// EnterCriticalSection, balancing the inline Unlock's LeaveCriticalSection) is
// correct for the single-threaded sentinel and keeps the link engine-free.
template<>
void Zenith_Windows_Mutex_T<true>::Lock()
{
	EnterCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(m_axCriticalSectionStorage));
}

// Zenith_Windows_Mutex_T<false> is the non-profiling mutex Zenith_MemoryTracker
// guards its allocation map with (FULL tracking tier). Same rationale as the
// <true>::Lock shim above: the real platform TU instruments via engine symbols,
// so the sentinel supplies plain CRITICAL_SECTION bodies instead.
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

// Unit-test + callstack seams of the L0 memory system
// ====================================================
// ZenithBase's memory TUs reference two more L0-adjacent facilities whose real
// implementations stay in the aggregate (Core/Zenith_TestFramework.cpp,
// Core/Callstack/):
//   * Zenith_MemoryManagement registers its own unit tests (ZENITH_TESTING is
//     on in every current config) and asserts through Zenith_TestRunner in
//     DumpLargestAllocations.
//   * Zenith_MemoryTracker captures + formats allocation callstacks at the
//     FULL tracking tier (Debug).
// The sentinel never runs unit tests or dumps allocations, so registration is
// accepted-and-ignored and the runtime paths fail LOUD if ever reached. These
// definitions keep the leaf-proof link honest without dragging the aggregate's
// test framework (and its engine deps) into the sentinel.
#ifdef ZENITH_TESTING
Zenith_TestRunner& Zenith_TestRunner::Instance()
{
	static Zenith_TestRunner ls_xRunner;
	return ls_xRunner;
}

void Zenith_TestRunner::RegisterTest(Zenith_TestCase* /*pxCase*/)
{
	// Static-init registrars from linked zenithbase TUs land here; the sentinel
	// never calls RunAllTests, so the case list is not kept.
}

void Zenith_TestRunner::AssertTrue(bool bExpr, const char* strExpr, const char* strFile, int iLine, const char* /*strFormat*/, ...)
{
	if (!bExpr)
	{
		fprintf(stderr, "sentinel: AssertTrue(%s) FAILED at %s:%d\n", strExpr, strFile, iLine);
		Zenith_DebugBreak();
	}
}
#endif

void Zenith_Callstack::Initialise()
{
}

u_int Zenith_Callstack::Capture(void** /*apFrames*/, u_int /*uMaxFrames*/, u_int /*uSkipFrames*/)
{
	return 0u;
}

void Zenith_Callstack::FormatCallstack(void** /*apFrames*/, u_int /*uFrameCount*/, char* szBuffer, size_t ulBufferSize)
{
	if (szBuffer != nullptr && ulBufferSize > 0u)
	{
		szBuffer[0] = '\0';
	}
}
