#include "Zenith.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "Windows/Multithreading/Zenith_Windows_Multithreading.h"

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
	EnterCriticalSection(&m_xCriticalSection);
}
