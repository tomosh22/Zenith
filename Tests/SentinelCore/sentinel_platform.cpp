#include "Zenith.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "Windows/Multithreading/Zenith_Windows_Multithreading.h"
#include "Core/Callstack/Zenith_Callstack.h"
#include "Core/Zenith_Win32.h"

// L0 platform contract for the Core cluster
// ==========================================
// ZenithCore (+ZenithMaths/ZenithDataStream/ZenithFileAccess) reference a
// small, fixed set of platform symbols whose real implementations live in the
// Windows/ platform layer (compiled into ZenithWindows, never linked here):
// three Zenith_FileAccess entry points, the two Zenith_Windows_Mutex_T
// specializations ZenithCore's own memory tracker guards its allocation map
// with, Zenith_PlatformStdio::OpenFile (pulled in by Zenith_TestFramework.obj
// once anything else in that .obj is referenced), the assert-capture atomics,
// and the four Zenith_Callstack::Platform_* hooks. This is the SAME floor
// Tests/SentinelECS/sentinel_platform.cpp already documents — SentinelCore
// sits one layer lower (no ZenithECS at all), and needs exactly the same
// set, because every one of these symbols is reached from inside ZenithCore
// itself (Memory/, Core/Zenith_TestFramework.cpp, Core/Callstack/), not from
// anything ZenithECS adds on top. Their presence here is the EXPLICIT,
// COMPLETE enumeration of this cluster's platform dependency floor — no
// g_xEngine, no Flux_*, no JPH::/Zenith_Physics, no AI, no ZenithECS at all.

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

// Zenith_Windows_Mutex_T<true> / <false> — see Tests/SentinelECS/sentinel_platform.cpp's
// header comment for the full rationale (profiling-free Lock, no g_xEngine
// reach). <false> is the FULL-tier Zenith_MemoryTracker's allocation-map
// mutex; every ZenithCore allocation (which SentinelCore's own DataStream
// round-trip triggers) goes through it, so unlike SentinelECS this stub is
// not speculative — it is on the sentinel's own hot path.
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
