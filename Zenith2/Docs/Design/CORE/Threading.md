# CORE design: threading primitives, callbacks and operations

This file is part of the CORE design document family (entry: `Zenith2/Docs/Design/CORE.md`). It implements `Zenith_Multithreading` and the concurrency contract that `Zenith2/Docs/Contracts.md` → "Threading and concurrency contract" states for ZenithCore's primitives: threads, mutexes and the lock-order checker, condition variables and semaphores (each in a profiled and an unprofiled variant, with the dispatch guard that keeps profiling from recursing), the Debug thread state, `Zenith_ReentrancyGuard`, `Zenith_CallbackRegistry` and `Zenith_OperationHandle` with its operation table. The job system, the work-stealing deques and the floating-point environment are ZenithJobs's (JOBS; `Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Job system").

## Scope of this file

| Topic | Fixed by Contracts | Added here |
|---|---|---|
| headers and per-platform implementations | table "Zenith_Multithreading" of the concurrency section | full declarations, native storage, per-platform calls |
| lock ordering and the checker | "Lock ordering table", "Debug lock-order checker" | four new rows, the `TryLock` rule, condition-variable interaction |
| callback lifetime, deferred destruction | "Callback lifetime", "Deferred destruction during dispatch" | entry storage, flags, the dispatch algorithm |
| cancellation completion | "Cancellation completion" | the operation table, the executor ticket, status and progress publication |
| reentrancy guard | "Reentrancy notation" | layout and algorithm |
| no-allocation regions | nothing (row AU6 of the cross-area notes) | `Zenith_NoAllocationScope` and the heap's check |
| lock profiling and the recursion defence | [Z2-N-CONTRACT-589], [Z2-N-CONTRACT-590], [Z2-N-CONTRACT-591] | the variants' reports, the contended-wait measurement, the dispatch guard and the ZenithCore sources a hook dispatch reaches |

## Threads

```cpp
// Zenith2/Engine/ZenithCore/Zenith_Multithreading.h
using Zenith_PfnThreadEntry = void (*)(void* pUserData);

enum Zenith_ThreadPriority : uint8
{
	THREAD_PRIORITY_BELOW_NORMAL = 1,
	THREAD_PRIORITY_NORMAL,
	THREAD_PRIORITY_ABOVE_NORMAL,
	THREAD_PRIORITY_TIME_CRITICAL,        // the audio device thread alone
};

enum Zenith_ThreadError : uint8
{
	THREAD_ERROR_CREATE_FAILED = 1,       // the OS refused (resource limits)
	THREAD_ERROR_OUT_OF_MEMORY,           // the start block could not be allocated
	THREAD_ERROR_INVALID_DESC,            // null entry, empty name, stack below 64 KiB
};

struct Zenith_ThreadDesc
{
	const char* m_szName = nullptr;                  // ASCII, at most 15 bytes (the pthread name limit)
	Zenith_PfnThreadEntry m_pfnEntry = nullptr;
	void* m_pUserData = nullptr;
	uint64 m_ulStackBytes = 1024ull * 1024;
	Zenith_ThreadPriority m_ePriority = THREAD_PRIORITY_NORMAL;
	uint64 m_ulAffinityMask = 0;                    // 0: any core; otherwise bit per logical core
	Zenith_FaultInjector* m_pxFaultInjector = nullptr;   // tests; uFAULT_POINT_THREAD_NATIVE_CREATE
};

constexpr uint32 uFAULT_POINT_THREAD_NATIVE_CREATE = 0x434F0001;   // Create step 3, before CreateThread / pthread_create

/// @contract type Zenith_Thread
/// Ownership: Created by Create as a value; move-only; joined by exactly one owner before destruction
///   ([Z2-N-CONTRACT-313]). Detached threads do not exist.
/// Thread affinity: OWNER_THREAD. Join is called by the owner, never by the thread itself.
/// Invalidation: None.
/// Units: Stack size in bytes.
/// Errors: Create returns Zenith_Result<Zenith_Thread, Zenith_ThreadError> (THREAD_ERROR_INVALID_DESC,
///   THREAD_ERROR_OUT_OF_MEMORY for the start block, THREAD_ERROR_CREATE_FAILED for the native creation or an
///   injected failure at uFAULT_POINT_THREAD_NATIVE_CREATE); destruction of a joinable thread
///   is ZENITH_CHECK; Join from the thread itself is ZENITH_CHECK; Join on a job worker is ZENITH_CHECK wherever
///   ZENITH_ASSERTS_ENABLED=1 ([Z2-N-CONTRACT-315]).
/// Version: since Z2 Stage 1. Source API.
/// @endcontract
class Zenith_Thread
{
public:
	constexpr Zenith_Thread() = default;
	static Zenith_Result<Zenith_Thread, Zenith_ThreadError> Create(const Zenith_ThreadDesc& xDesc);
	Zenith_Thread(Zenith_Thread&& xOther);
	Zenith_Thread& operator=(Zenith_Thread&& xOther);   // ZENITH_CHECK: this is not joinable
	Zenith_Thread(const Zenith_Thread&) = delete;
	Zenith_Thread& operator=(const Zenith_Thread&) = delete;
	~Zenith_Thread();
	void Join();
	bool IsJoinable() const;
	uint64 GetThreadID() const;
private:
	uint64 m_ulNativeHandle = 0;       // HANDLE on Windows, pthread_t elsewhere; identical layout on every platform
	uint64 m_ulThreadID = 0;
};

uint64 Zenith_GetCurrentThreadID();      // GetCurrentThreadId / gettid
uint32 Zenith_GetHardwareConcurrency();  // logical cores available to the process
void Zenith_YieldThread();                              // SwitchToThread / sched_yield
void Zenith_SetThreadName(const char* szName);          // the calling thread
void Zenith_MarkThreadAsJobWorker(bool bRunningJob);   // true on entry to a job body, false on its exit (JOBS toggles it)
bool Zenith_IsJobWorkerThread();          // a job body is running on the calling thread now
void Zenith_CpuRelax();                                 // one spin-wait hint: _mm_pause / __builtin_ia32_pause / __builtin_arm_yield
void Zenith_SleepUs(uint64 ulMicroseconds);            // a blocking wait: ZENITH_CHECK on a job worker
```

`Zenith_CpuRelax` is implemented in `Internal/Windows/` (`_mm_pause`), `Internal/Linux/` (`__builtin_ia32_pause`) and `Internal/Android/` (`__builtin_arm_yield` for arm64, `__builtin_ia32_pause` for the x86_64 emulator ABI, selected by the one architecture test in `Internal/Zenith_CompilerSupport.h`); JOBS uses it for its spin loops instead of a private shim.

`Create` runs these steps: (1) validate the desc (`THREAD_ERROR_INVALID_DESC`); (2) `Zenith_TryNew` a start block `{ desc copy, name copy[16] }` (`THREAD_ERROR_OUT_OF_MEMORY`); (3) when `m_pxFaultInjector` is non-null and `ShouldFail(uFAULT_POINT_THREAD_NATIVE_CREATE)` returns true, free the block and return `THREAD_ERROR_CREATE_FAILED`; (4) create the thread (`CreateThread` with `STACK_SIZE_PARAM_IS_A_RESERVATION`; `pthread_create` with `pthread_attr_setstacksize`); on failure free the block and return `THREAD_ERROR_CREATE_FAILED`; the new thread sets its name (`SetThreadDescription` with the UTF-16 name; `pthread_setname_np`), priority and affinity (`SetThreadPriority`, `SetThreadAffinityMask`; `setpriority` on the thread's TID and `sched_setaffinity`, best effort, failures logged in `core.threading`), deletes the start block, and calls the entry.

| Platform file | Contents |
|---|---|
| `Internal/Windows/Zenith_Thread_Windows.cpp`, `Zenith_Mutex_Windows.cpp`, `Zenith_ConditionVariable_Windows.cpp`, `Zenith_Semaphore_Windows.cpp` | Win32 implementations |
| `Internal/Android/Zenith_Thread_Android.cpp` … `Zenith_Semaphore_Android.cpp` | pthreads implementations |
| `Internal/Linux/Zenith_Thread_Linux.cpp` … `Zenith_Semaphore_Linux.cpp` | pthreads implementations, textually parallel to the Android files (the per-platform directory rule gives POSIX code no shared directory); `Test_Threading_PosixParity` asserts both behave identically by running the same suite in `linux-tsan` and on the emulator build |

## Platform threading classes

<!-- elaboration -->
`Zenith_Mutex`, `Zenith_ConditionVariable` and `Zenith_Semaphore` each wrap a native OS object, and each exists in two variants chosen at compile time: the profiled variant reports to the installed profiler hooks, and the unprofiled variant (`Zenith_MutexNoProfiling`, `Zenith_ConditionVariableNoProfiling`, `Zenith_SemaphoreNoProfiling`) never reads the hook table ([Z2-N-CONTRACT-589]). Each is therefore one class template per platform, on one `bool bProfiled`, that holds its native object as a typed member. There is no opaque byte array and no pointer to a separately allocated native object. The public names are aliases of the build platform's two instantiations.

| Public aliases (`<true>`, `<false>`) | Windows class template and native member | Android class template and native members | Linux class template and native members |
|---|---|---|---|
| `Zenith_Mutex`, `Zenith_MutexNoProfiling` | `Zenith_WindowsMutex_T<bProfiled>`: `void* m_pSRWLock` (an `SRWLOCK`) | `Zenith_AndroidMutex_T<bProfiled>`: `pthread_mutex_t m_xMutex` | `Zenith_LinuxMutex_T<bProfiled>`: `pthread_mutex_t m_xMutex` |
| `Zenith_ConditionVariable`, `Zenith_ConditionVariableNoProfiling` | `Zenith_WindowsConditionVariable_T<bProfiled>`: `void* m_pConditionVariable` (a `CONDITION_VARIABLE`) | `Zenith_AndroidConditionVariable_T<bProfiled>`: `pthread_cond_t m_xCondition` | `Zenith_LinuxConditionVariable_T<bProfiled>`: `pthread_cond_t m_xCondition` |
| `Zenith_Semaphore`, `Zenith_SemaphoreNoProfiling` | `Zenith_WindowsSemaphore_T<bProfiled>`: `void* m_pHandle` (the `HANDLE` of `CreateSemaphoreW`) | `Zenith_AndroidSemaphore_T<bProfiled>`: `sem_t m_xSemaphore`, `uint m_uMaxValue` | `Zenith_LinuxSemaphore_T<bProfiled>`: `sem_t m_xSemaphore`, `uint m_uMaxValue` |

- **One header per platform.** A platform's three class templates live in `Zenith2/Engine/ZenithCore/Platform/<Platform>/Zenith_PlatformThreading.h`. `zenith_add_library()` adds exactly the target platform's directory to ZenithCore's public include directories ([Z2-N-CONTRACT-583]): for a library that has a `Platform/` directory, which is ZenithCore alone, it runs `target_include_directories(<Name> PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Platform/${CMAKE_SYSTEM_NAME}")`, and `CMAKE_SYSTEM_NAME` is `Windows`, `Android` or `Linux` in the three platforms' presets. So `#include "Zenith_PlatformThreading.h"` resolves to one file in every build, and no shared file tests a platform macro (rule F11, [Z2-N-CONTRACT-442]). The out-of-line definitions stay under `Internal/<Platform>/`. The `HeaderSelfContained` step compiles each platform's header with that platform's directory on the include path.
- **The aliases.** Each platform header ends with its six aliases, for example `using Zenith_Mutex = Zenith_WindowsMutex_T<true>;` and `using Zenith_MutexNoProfiling = Zenith_WindowsMutex_T<false>;`, and the same pair for the condition variable and the semaphore, because only a file that is already platform-specific can name the platform class template without a preprocessor conditional. `Zenith_Mutex.h` includes the platform header, and `Zenith_ConditionVariable.h` and `Zenith_Semaphore.h` include `Zenith_Mutex.h`, so each public header makes its aliases visible. Engine code outside these headers and their `Internal/<Platform>/` implementations names only the aliases, never a platform class template (review confirms).
- **Two variants, one body.** The member functions of each class template are declared in the platform header and defined in its `Internal/<Platform>/` source, which ends with the explicit instantiations of both values (`template class Zenith_WindowsMutex_T<true>;` and `<false>`); the header carries the matching `extern template class` declarations, so no other translation unit instantiates a body and `<windows.h>` and the POSIX calls stay in the platform source. The hook calls sit under `if constexpr (bProfiled)`, so the `<false>` instantiation contains no reference to `Zenith_GetProfilerHooks`, to any `Zenith_Dispatch…` function or to the clock. The member templates `Wait` and `WaitFor` of the condition variable, which must accept a `Zenith_ScopedLock` of either mutex type, are the only members defined in the header; each opens with `static_assert(TMutex::bIS_PROFILED == bProfiled, …)`, which refuses a condition variable of one variant waiting on a mutex of the other.
- **Windows.** An `SRWLOCK` and a `CONDITION_VARIABLE` are one pointer each, with an all-zero initial value (`SRWLOCK_INIT`, `CONDITION_VARIABLE_INIT`), and a `HANDLE` is a `void*`. The Windows header therefore stores all three as `void*` members initialised to `nullptr`, and it includes no Windows header. Each `Internal/Windows/` file has `static_assert(sizeof(SRWLOCK) == sizeof(void*) && alignof(SRWLOCK) == alignof(void*))`, and the same for `CONDITION_VARIABLE`. It passes `reinterpret_cast<PSRWLOCK>(&m_pSRWLock)`, `reinterpret_cast<PCONDITION_VARIABLE>(&m_pConditionVariable)` and `static_cast<HANDLE>(m_pHandle)` to the Win32 calls.
- **Android and Linux.** The header includes `<pthread.h>` and `<semaphore.h>` and no other platform header, and the classes hold the POSIX objects by value. `m_uMaxValue` holds the semaphore's maximum count, which `Post` checks, because a `sem_t` has no maximum of its own. Windows needs no such member, because `ReleaseSemaphore` refuses a count above the maximum given to `CreateSemaphoreW`.
- **The exemption.** The three headers are the one signposted exception of `Zenith2/Docs/Contracts.md` → "Library directory layout" [Z2-N-CONTRACT-583] (Z2-D-141). It covers the public header outside `Internal/<Platform>/`, the two POSIX includes, and the one case where a type's layout differs between platforms ([Z2-N-CONTRACT-441]); on one platform each class has one layout in every configuration. That bullet is the statement of the exemption; "No public header includes a third-party or platform header" (`Zenith2/Docs/Design/CORE.md` → "Rules across the API") relies on it.
- **The signpost.** Each file carries, directly after `#pragma once`, the four-line declaration comment block of [Z2-N-CONTRACT-578] with the file as its subject ([Z2-N-CONTRACT-584]). The blocks are exactly these:

```cpp
// Zenith2/Engine/ZenithCore/Platform/Android/Zenith_PlatformThreading.h
#pragma once
// ZENITH-EXEMPTION Platform/Android/Zenith_PlatformThreading.h: Z2-D-141
// Allowlist: Zenith2/Tools/Gates/forbidden_allowlist.json, entries "F18-PLATFORM-THREADING-ANDROID-PTHREAD" and "F18-PLATFORM-THREADING-ANDROID-SEMAPHORE"
// Contract: Zenith2/Docs/Contracts.md → "Library directory layout" [Z2-N-CONTRACT-583]
// Why: the per-platform threading classes hold their native pthread and semaphore objects as typed members, the old Zenith pattern.
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_LockDesc.h"

#include <pthread.h>
#include <semaphore.h>

// Zenith2/Engine/ZenithCore/Platform/Linux/Zenith_PlatformThreading.h
#pragma once
// ZENITH-EXEMPTION Platform/Linux/Zenith_PlatformThreading.h: Z2-D-141
// Allowlist: Zenith2/Tools/Gates/forbidden_allowlist.json, entries "F18-PLATFORM-THREADING-LINUX-PTHREAD" and "F18-PLATFORM-THREADING-LINUX-SEMAPHORE"
// Contract: Zenith2/Docs/Contracts.md → "Library directory layout" [Z2-N-CONTRACT-583]
// Why: the per-platform threading classes hold their native pthread and semaphore objects as typed members, the old Zenith pattern.
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_LockDesc.h"

#include <pthread.h>
#include <semaphore.h>

// Zenith2/Engine/ZenithCore/Platform/Windows/Zenith_PlatformThreading.h
#pragma once
// ZENITH-EXEMPTION Platform/Windows/Zenith_PlatformThreading.h: Z2-D-141
// Allowlist: none; this file includes no platform header, so Zenith2/Tools/Gates/forbidden_allowlist.json has no entry for it
// Contract: Zenith2/Docs/Contracts.md → "Library directory layout" [Z2-N-CONTRACT-583]
// Why: the per-platform threading classes hold the pointer-sized SRWLOCK, CONDITION_VARIABLE and semaphore HANDLE as void* members, so <windows.h> reaches no public header.
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_LockDesc.h"
```

- **Shared behaviour lives once.** The lock-order checks, the held-lock stack and the blocking-wait rule are free functions that every platform class template calls around its native call, so each platform file holds only the native calls, these calls and the `if constexpr (bProfiled)` hook calls. `Internal/Profiling/Zenith_LockOrderChecker.h` and `.cpp` hold the checker, and `Internal/Threading/Zenith_BlockingWait.h` holds the blocking-wait functions. Each function's body differs by configuration exactly as [Z2-N-CONTRACT-444] allows. The checker's functions take the mutex's `Zenith_LockDesc` and its address, so one non-template checker serves both variants:

| Function | Called by | Does |
|---|---|---|
| `void Zenith_LockOrderCheckAcquire(const Zenith_LockDesc& xDesc, const void* pLock)` | `Lock`, before the native lock, wherever `Zenith_AreAssertsEnabled()` | the order and recursion checks of [Z2-N-CONTRACT-322] and [Z2-N-CONTRACT-323] |
| `void Zenith_LockOrderPush(const Zenith_LockDesc& xDesc, const void* pLock)` | `Lock` and a successful `TryLock`, after the native lock; a condition-variable wait, after it returns | push the held-lock stack (asserts-enabled builds only) |
| `void Zenith_LockOrderPop(const Zenith_LockDesc& xDesc, const void* pLock)` | `Unlock`, before the native unlock; a condition-variable wait, before it blocks | the owner `ZENITH_ASSERT` and the pop (asserts-enabled builds only) |
| `void Zenith_LockOrderCheckWait(const Zenith_LockDesc& xDesc, const void* pLock)` | a condition-variable wait, before the pop | `ZENITH_CHECK` that the waited mutex is not short-hold and is the only lock held (asserts-enabled builds only) |
| `bool Zenith_IsLockLevelRegistered(uint16 uLevel)` | the mutex constructor, wherever asserts are enabled | whether the level has a row ([Z2-N-CONTRACT-320]) |
| `void Zenith_CheckBlockingAllowed(const char* szPrimitive)` | every blocking call of either variant, before it blocks | the job-worker `ZENITH_CHECK` ([Z2-N-CONTRACT-315], asserts-enabled builds only); it reads no hook |
| `template<bool bProfiled> class Zenith_BlockingWaitScope` (`Zenith_BlockingWaitScope<bProfiled> xScope(szPrimitive);`) | around the native wait of every blocking call, after `Zenith_CheckBlockingAllowed` | `<true>`: the constructor sets `m_bTimed = Zenith_IsBlockingWaitHookInstalled()` and, when set, reads `Zenith_GetMonotonicTimeNs()`; the destructor, after the wait has returned, calls `Zenith_DispatchBlockingWait(szPrimitive, m_bTimed ? now - start : 0)`. `<false>`: an empty object that names no hook. The primitives pass their own `bProfiled`; `Zenith_Thread::Join` and `Zenith_SleepUs` pass `true` |

  The profiled hook calls of `Lock`, `TryLock` and `Unlock` are specified in "Mutexes and lock ordering", and the dispatch functions they call in "Lock profiling and hook dispatch".
- **One contract block per class template.** Each platform class template carries the contract block shown below for its aliases. The text is the same in the three headers, with the platform class template's name on the `@contract` line. The alias declarations carry no block of their own.
- **Recursion through the profiler is impossible by construction.** The earlier rule of this section that there is no unprofiled variant (every mutex calls the installed lock hooks, and the profiler stops its own recursion at run time by a lock level and an in-profiler flag) is withdrawn by Z2-D-142. In its place are three independent layers, specified in "Lock profiling and hook dispatch": the profiler and every ZenithCore source a hook dispatch can reach use only the `…NoProfiling` variants (types); `ForbiddenCheck` rule F26 refuses a profiled primitive in those sources (gate); and every hook call goes through a dispatch function whose `thread_local` depth refuses re-entry in every build (run time). `Zenith2/Docs/Design/DEVT/Profiler.md` → "Lock and wait events" no longer relies on a lock at level 940 being skipped.

## Mutexes and lock ordering

```cpp
// Zenith2/Engine/ZenithCore/Zenith_LockDesc.h
// struct Zenith_LockDesc: exactly as Zenith2/Docs/Contracts.md "Threading and concurrency contract". It has a header
// of its own because the platform threading headers hold it by value; Zenith_Mutex.h includes it first.

// Zenith2/Engine/ZenithCore/Zenith_Mutex.h
#include "ZenithCore/Zenith_LockDesc.h"
#include "Zenith_PlatformThreading.h"      // the build platform's header ("Platform threading classes"): the class templates and the six aliases

/// @contract type Zenith_ScopedLock
/// Ownership: Holds a pointer to a mutex of either variant; the mutex outlives it. Not copyable or movable.
/// Thread affinity: OWNER_THREAD; it unlocks on the thread that locked.
/// Invalidation: None.
/// Units: None.
/// Errors: As Lock and Unlock of the mutex type.
/// Version: since Z2 Stage 1. A template on the mutex type since Z2-D-142; class template argument deduction
///   keeps `Zenith_ScopedLock xLock(xMutex);` as the way to write it, for Zenith_Mutex and Zenith_MutexNoProfiling alike.
/// Locks: the mutex's level.
/// @endcontract
template<typename TMutex>
class Zenith_ScopedLock
{
public:
	explicit Zenith_ScopedLock(TMutex& xMutex)
		: m_pxMutex(&xMutex)
	{
		m_pxMutex->Lock();
	}
	~Zenith_ScopedLock() { m_pxMutex->Unlock(); }
	Zenith_ScopedLock(const Zenith_ScopedLock&) = delete;
	Zenith_ScopedLock& operator=(const Zenith_ScopedLock&) = delete;
	TMutex& GetMutex() const { return *m_pxMutex; }
private:
	TMutex* m_pxMutex;
};

// Zenith2/Engine/ZenithCore/Platform/Windows/Zenith_PlatformThreading.h (the mutex part)
/// @contract type Zenith_WindowsMutex_T
/// Ownership: Embedded by value in its owner; never copied or moved.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: Wait reported to the acquire hook in nanoseconds of Zenith_GetMonotonicTimeNs().
/// Errors: A desc whose level has no row, a recursive Lock, or an order violation is ZENITH_CHECK or
///   ZENITH_FATAL(FATAL_CODE_LOCK_ORDER) wherever ZENITH_ASSERTS_ENABLED=1; Unlock by a non-owner is ZENITH_ASSERT.
/// Version: since Z2 Stage 1. Source API, named through the aliases Zenith_Mutex (bProfiled true) and
///   Zenith_MutexNoProfiling (bProfiled false) since Z2-D-142.
/// Locks: the level of its desc. The profiled instantiation reports each acquisition and release to the installed
///   profiler hooks through Zenith_DispatchLockAcquire and Zenith_DispatchLockRelease; the unprofiled instantiation
///   never reads the hook table.
/// @endcontract
template<bool bProfiled>
class Zenith_WindowsMutex_T
{
public:
	static constexpr bool bIS_PROFILED = bProfiled;
	explicit Zenith_WindowsMutex_T(const Zenith_LockDesc& xDesc);
	~Zenith_WindowsMutex_T();
	Zenith_WindowsMutex_T(const Zenith_WindowsMutex_T&) = delete;
	Zenith_WindowsMutex_T& operator=(const Zenith_WindowsMutex_T&) = delete;
	void Lock();
	bool TryLock();
	void Unlock();
	const Zenith_LockDesc& GetDesc() const;
private:
	template<bool bConditionProfiled> friend class Zenith_WindowsConditionVariable_T;   // SleepConditionVariableSRW takes the SRWLOCK
	void* m_pSRWLock = nullptr;                     // an SRWLOCK; SRWLOCK_INIT is all-zero
	Zenith_LockDesc m_xDesc;
};
extern template class Zenith_WindowsMutex_T<true>;      // defined and instantiated in Internal/Windows/Zenith_Mutex_Windows.cpp
extern template class Zenith_WindowsMutex_T<false>;
using Zenith_Mutex = Zenith_WindowsMutex_T<true>;
using Zenith_MutexNoProfiling = Zenith_WindowsMutex_T<false>;

// Zenith2/Engine/ZenithCore/Platform/Android/Zenith_PlatformThreading.h (the mutex part). The Linux header is the
// same with Linux in place of Android in every name.
/// @contract type Zenith_AndroidMutex_T
/// (the fields of Zenith_WindowsMutex_T)
/// @endcontract
template<bool bProfiled>
class Zenith_AndroidMutex_T
{
public:
	static constexpr bool bIS_PROFILED = bProfiled;
	explicit Zenith_AndroidMutex_T(const Zenith_LockDesc& xDesc);   // pthread_mutex_init with PTHREAD_MUTEX_NORMAL
	~Zenith_AndroidMutex_T();                                       // pthread_mutex_destroy
	Zenith_AndroidMutex_T(const Zenith_AndroidMutex_T&) = delete;
	Zenith_AndroidMutex_T& operator=(const Zenith_AndroidMutex_T&) = delete;
	void Lock();
	bool TryLock();
	void Unlock();
	const Zenith_LockDesc& GetDesc() const;
private:
	template<bool bConditionProfiled> friend class Zenith_AndroidConditionVariable_T;   // pthread_cond_wait takes the pthread_mutex_t
	pthread_mutex_t m_xMutex;
	Zenith_LockDesc m_xDesc;
};
extern template class Zenith_AndroidMutex_T<true>;
extern template class Zenith_AndroidMutex_T<false>;
using Zenith_Mutex = Zenith_AndroidMutex_T<true>;
using Zenith_MutexNoProfiling = Zenith_AndroidMutex_T<false>;
```

- `Lock` has one body for both variants, with the profiled part under `if constexpr (bProfiled && !Zenith_IsShippingBuild())` (in a Shipping build the profiled variant is compiled as the unprofiled one: no try-first and no hook lookup, because the Shipping profiler installs no lock hooks):
  1. Wherever `Zenith_AreAssertsEnabled()`, `Zenith_LockOrderCheckAcquire(m_xDesc, this)` validates the acquisition ([Z2-N-CONTRACT-322]).
  2. `<false>`: the native lock is taken (`AcquireSRWLockExclusive`; `pthread_mutex_lock`), `Zenith_LockOrderPush(m_xDesc, this)` pushes the held-lock stack, and `Lock` returns. No hook table is read.
  3. `<true>`: the native lock is tried without blocking (`TryAcquireSRWLockExclusive`; `pthread_mutex_trylock`, where `EBUSY` is contention and any other non-zero result is `ZENITH_CHECK`). On success the stack is pushed and `Zenith_DispatchLockAcquire(m_xDesc, this, 0)` runs.
  4. `<true>`, contended: `bTimed = Zenith_IsLockAcquireHookInstalled()`; when `bTimed`, `ulStart = Zenith_GetMonotonicTimeNs()`; the thread blocks on the native lock; the stack is pushed; when `bTimed`, `ulWait = Zenith_GetMonotonicTimeNs() - ulStart`, else 0; then `Zenith_DispatchLockAcquire(m_xDesc, this, max(ulWait, 1))` runs (with no clock read, the wait is 1). The clock is read only on this path and only with an acquire hook installed, so with no hook installed a contended `Lock` adds one try-lock, one load and one branch over the unprofiled variant (the cost of a dispatch is in "What the variants report"). The second clock read lies inside the critical section, which lengthens a contended hold by about one clock read. The wait passed is `max(ulWait, 1)`: a failed try-lock that wins within one clock tick still reports 1, because 0 means "acquired without blocking".
- `TryLock`: a successful try-lock pushes the stack and, on `<true>`, runs `Zenith_DispatchLockAcquire(m_xDesc, this, 0)`; it skips the ordering check, because a try-lock cannot deadlock. A failed try-lock calls no hook. A subsequent blocking `Lock` is checked against every held entry, including a try-locked one.
- `Unlock`: `Zenith_LockOrderPop(m_xDesc, this)`; on `<true>`, a local copy `xDesc = m_xDesc` is taken; then the native unlock (`ReleaseSRWLockExclusive`; `pthread_mutex_unlock`); then, on `<true>`, `Zenith_DispatchLockRelease(xDesc, this)`. The copy comes before the native unlock because the moment the lock is released another thread may acquire it, finish and destroy the object that embeds the mutex: the dispatch and every hook see only the copy (its name pointer is a literal that outlives the mutex), and `this` is an identity token that nothing dereferences after the unlock. `TryLock` and `Unlock` have the same Shipping form as `Lock`, and so do the release and acquire dispatches of a condition-variable wait.
- A mutex is never recursive, on any platform or in either variant. An `SRWLOCK` is not recursive, and the POSIX mutex is created `PTHREAD_MUTEX_NORMAL`, never `PTHREAD_MUTEX_RECURSIVE`. Locking a mutex that is already on the calling thread's held stack is `ZENITH_CHECK` wherever asserts are enabled; elsewhere it deadlocks, as the native lock does.
- The constructor stores the desc and, wherever asserts are enabled, checks that its level has a row ([Z2-N-CONTRACT-320]), in both variants.
- `bIS_PROFILED` lets generic code, `Zenith_ScopedLock` users and the condition variable's `static_assert` ask which variant a mutex type is, without naming the platform class.

### Lock-order rows added by CORE

The landing tickets add these rows to `Zenith2/Docs/Contracts.md` → "Threading and concurrency contract" (table "Lock ordering table") in their diffs ([Z2-N-CONTRACT-320]); each constant is declared in the owning library's `Internal/` header:

| Level | Constant | Owner | Guards | Short-hold | Same level by address | Acquired on |
|---|---|---|---|---|---|---|
| 380 | `uLOCK_LEVEL_OPERATION_TABLE` | ZenithCore | an operation table's slot allocation and free list | yes | no | any thread |
| 850 | `uLOCK_LEVEL_STRING_TABLE` | ZenithCore | the intern map and page publication of `Zenith_StringTable` | yes | no | any thread |
| 930 | `uLOCK_LEVEL_LOG_RING_SINK` | ZenithCore | a `Zenith_LogRingSink`'s ring | yes | no | any thread (under `uLOCK_LEVEL_LOG_SINKS` from `Write`) |
| 980 | `uLOCK_LEVEL_HEAP_ALIGNED_TABLE` | ZenithCore | the Windows system backing's over-aligned side table | yes | no | any thread |

Ordering facts the rows rely on: interning allocates from the root heap under 850, which may enter the OOM handler at 900; a sink's `Write` at 920 takes its ring lock at 930; the heap takes 980 inside the backing call and releases it before it records a leak-table entry at 990, so the two are never held together.

## Condition variables and semaphores

```cpp
// Zenith2/Engine/ZenithCore/Zenith_ConditionVariable.h and Zenith2/Engine/ZenithCore/Zenith_Semaphore.h
#include "ZenithCore/Zenith_Mutex.h"      // the platform header, the aliases and Zenith_ScopedLock

// Zenith2/Engine/ZenithCore/Platform/Windows/Zenith_PlatformThreading.h (the condition-variable and semaphore part)
template<typename TMutex> class Zenith_ScopedLock;      // Zenith_Mutex.h; the wait members take it by reference

/// @contract type Zenith_WindowsConditionVariable_T
/// Ownership: Embedded by value; never copied or moved.
/// Thread affinity: ANY_THREAD. Waits happen only on threads that are not job workers.
/// Invalidation: None.
/// Units: Timeouts in microseconds.
/// Errors: waiting with a short-hold mutex, waiting on a job worker, or waiting while holding any lock other
///   than the one being waited on is ZENITH_CHECK wherever ZENITH_ASSERTS_ENABLED=1. Waiting on a mutex of the other
///   variant does not compile (static_assert).
/// Version: since Z2 Stage 1. Named through the aliases Zenith_ConditionVariable (bProfiled true) and
///   Zenith_ConditionVariableNoProfiling (bProfiled false) since Z2-D-142.
/// Locks: waits on a mutex of its own variant only. The profiled instantiation calls the blocking-wait hook
///   through Zenith_DispatchBlockingWait after the wait returns, with the time it blocked, named by the paired mutex's
///   desc, and reports a lock release before and an acquire after a wait; the unprofiled instantiation never reads the hook table.
/// @endcontract
template<bool bProfiled>
class Zenith_WindowsConditionVariable_T
{
public:
	static constexpr bool bIS_PROFILED = bProfiled;
	Zenith_WindowsConditionVariable_T();
	~Zenith_WindowsConditionVariable_T();
	Zenith_WindowsConditionVariable_T(const Zenith_WindowsConditionVariable_T&) = delete;
	Zenith_WindowsConditionVariable_T& operator=(const Zenith_WindowsConditionVariable_T&) = delete;
	template<typename TMutex>
	void Wait(Zenith_ScopedLock<TMutex>& xLock)
	{
		static_assert(TMutex::bIS_PROFILED == bProfiled, "a condition variable waits only on a mutex of its own variant: Zenith_ConditionVariable on Zenith_Mutex, Zenith_ConditionVariableNoProfiling on Zenith_MutexNoProfiling");
		WaitOn(xLock.GetMutex());
	}
	template<typename TMutex>
	bool WaitFor(Zenith_ScopedLock<TMutex>& xLock, uint64 ulTimeoutUs)   // false on timeout
	{
		static_assert(TMutex::bIS_PROFILED == bProfiled, "a condition variable waits only on a mutex of its own variant: Zenith_ConditionVariable on Zenith_Mutex, Zenith_ConditionVariableNoProfiling on Zenith_MutexNoProfiling");
		return WaitForOn(xLock.GetMutex(), ulTimeoutUs);
	}
	void NotifyOne();
	void NotifyAll();
private:
	void WaitOn(Zenith_WindowsMutex_T<bProfiled>& xMutex);                                // defined in Internal/Windows/Zenith_ConditionVariable_Windows.cpp
	bool WaitForOn(Zenith_WindowsMutex_T<bProfiled>& xMutex, uint64 ulTimeoutUs);
	void* m_pConditionVariable = nullptr;     // a CONDITION_VARIABLE; CONDITION_VARIABLE_INIT is all-zero
};
extern template class Zenith_WindowsConditionVariable_T<true>;
extern template class Zenith_WindowsConditionVariable_T<false>;
using Zenith_ConditionVariable = Zenith_WindowsConditionVariable_T<true>;
using Zenith_ConditionVariableNoProfiling = Zenith_WindowsConditionVariable_T<false>;

/// @contract type Zenith_WindowsSemaphore_T
/// Ownership: Embedded by value; never copied or moved.
/// Thread affinity: ANY_THREAD. Wait and WaitFor only on threads that are not job workers.
/// Invalidation: None.
/// Units: Counts; timeouts in microseconds.
/// Errors: Post beyond uMaxCount is ZENITH_CHECK; a wait on a job worker is ZENITH_CHECK wherever
///   ZENITH_ASSERTS_ENABLED=1.
/// Version: since Z2 Stage 1. Named through the aliases Zenith_Semaphore (bProfiled true) and
///   Zenith_SemaphoreNoProfiling (bProfiled false) since Z2-D-142.
/// Locks: none. The profiled instantiation calls the blocking-wait hook through Zenith_DispatchBlockingWait after
///   Wait and WaitFor return, with the time they blocked; the unprofiled instantiation never reads the hook table.
/// @endcontract
template<bool bProfiled>
class Zenith_WindowsSemaphore_T
{
public:
	static constexpr bool bIS_PROFILED = bProfiled;
	Zenith_WindowsSemaphore_T(uint32 uInitialCount, uint32 uMaxCount, const char* szName = "Zenith_Semaphore");   // CreateSemaphoreW(nullptr, initial, max, nullptr); szName has static storage duration and is the name Wait and WaitFor report
	~Zenith_WindowsSemaphore_T();                                         // CloseHandle
	Zenith_WindowsSemaphore_T(const Zenith_WindowsSemaphore_T&) = delete;
	Zenith_WindowsSemaphore_T& operator=(const Zenith_WindowsSemaphore_T&) = delete;
	void Post(uint32 uCount = 1);
	void Wait();
	bool WaitFor(uint64 ulTimeoutUs);
	bool TryWait();
private:
	void* m_pHandle = nullptr;                // the HANDLE of CreateSemaphoreW
	const char* m_szName = "Zenith_Semaphore";   // the primitive name reported after a wait
};
extern template class Zenith_WindowsSemaphore_T<true>;
extern template class Zenith_WindowsSemaphore_T<false>;
using Zenith_Semaphore = Zenith_WindowsSemaphore_T<true>;
using Zenith_SemaphoreNoProfiling = Zenith_WindowsSemaphore_T<false>;

// Zenith2/Engine/ZenithCore/Platform/Android/Zenith_PlatformThreading.h (the condition-variable and semaphore part).
// The Linux header is the same with Linux in place of Android in every name. Each class template carries the contract
// block of its Windows counterpart.
template<bool bProfiled>
class Zenith_AndroidConditionVariable_T
{
public:
	static constexpr bool bIS_PROFILED = bProfiled;
	Zenith_AndroidConditionVariable_T();       // pthread_cond_init with pthread_condattr_setclock(CLOCK_MONOTONIC)
	~Zenith_AndroidConditionVariable_T();      // pthread_cond_destroy
	Zenith_AndroidConditionVariable_T(const Zenith_AndroidConditionVariable_T&) = delete;
	Zenith_AndroidConditionVariable_T& operator=(const Zenith_AndroidConditionVariable_T&) = delete;
	template<typename TMutex>
	void Wait(Zenith_ScopedLock<TMutex>& xLock)
	{
		static_assert(TMutex::bIS_PROFILED == bProfiled, "a condition variable waits only on a mutex of its own variant: Zenith_ConditionVariable on Zenith_Mutex, Zenith_ConditionVariableNoProfiling on Zenith_MutexNoProfiling");
		WaitOn(xLock.GetMutex());
	}
	template<typename TMutex>
	bool WaitFor(Zenith_ScopedLock<TMutex>& xLock, uint64 ulTimeoutUs)   // false on timeout
	{
		static_assert(TMutex::bIS_PROFILED == bProfiled, "a condition variable waits only on a mutex of its own variant: Zenith_ConditionVariable on Zenith_Mutex, Zenith_ConditionVariableNoProfiling on Zenith_MutexNoProfiling");
		return WaitForOn(xLock.GetMutex(), ulTimeoutUs);
	}
	void NotifyOne();
	void NotifyAll();
private:
	void WaitOn(Zenith_AndroidMutex_T<bProfiled>& xMutex);
	bool WaitForOn(Zenith_AndroidMutex_T<bProfiled>& xMutex, uint64 ulTimeoutUs);
	pthread_cond_t m_xCondition;
};
extern template class Zenith_AndroidConditionVariable_T<true>;
extern template class Zenith_AndroidConditionVariable_T<false>;
using Zenith_ConditionVariable = Zenith_AndroidConditionVariable_T<true>;
using Zenith_ConditionVariableNoProfiling = Zenith_AndroidConditionVariable_T<false>;

template<bool bProfiled>
class Zenith_AndroidSemaphore_T
{
public:
	static constexpr bool bIS_PROFILED = bProfiled;
	Zenith_AndroidSemaphore_T(uint32 uInitialCount, uint32 uMaxCount, const char* szName = "Zenith_Semaphore");   // sem_init(&m_xSemaphore, 0, initial); szName has static storage duration
	~Zenith_AndroidSemaphore_T();                                         // sem_destroy
	Zenith_AndroidSemaphore_T(const Zenith_AndroidSemaphore_T&) = delete;
	Zenith_AndroidSemaphore_T& operator=(const Zenith_AndroidSemaphore_T&) = delete;
	void Post(uint32 uCount = 1);
	void Wait();
	bool WaitFor(uint64 ulTimeoutUs);
	bool TryWait();
private:
	sem_t m_xSemaphore;
	uint m_uMaxValue;                         // uMaxCount: sem_t has no maximum of its own
	const char* m_szName;                     // the primitive name reported after a wait
};
extern template class Zenith_AndroidSemaphore_T<true>;
extern template class Zenith_AndroidSemaphore_T<false>;
using Zenith_Semaphore = Zenith_AndroidSemaphore_T<true>;
using Zenith_SemaphoreNoProfiling = Zenith_AndroidSemaphore_T<false>;
```

| Member | Windows | Android, Linux |
|---|---|---|
| condition variable `Wait`, `WaitFor` | `SleepConditionVariableSRW` on the mutex's `SRWLOCK` (reached as a friend), timeout in milliseconds rounded up, `false` on `ERROR_TIMEOUT` | `pthread_cond_wait`, or `pthread_cond_timedwait` against an absolute `CLOCK_MONOTONIC` deadline, on the mutex's `pthread_mutex_t` (reached as a friend); `false` on `ETIMEDOUT` |
| `NotifyOne`, `NotifyAll` | `WakeConditionVariable`, `WakeAllConditionVariable` | `pthread_cond_signal`, `pthread_cond_broadcast` |
| semaphore `Post(uCount)` | `ReleaseSemaphore(handle, uCount, nullptr)`; `ZENITH_CHECK` that it succeeded, since it fails above the maximum | `ZENITH_CHECK` that `sem_getvalue` plus `uCount` is at most `m_uMaxValue`, then `uCount` calls of `sem_post` |
| semaphore `Wait`, `WaitFor`, `TryWait` | `WaitForSingleObject` with `INFINITE`, the timeout in milliseconds rounded up, or 0 | `sem_wait`, `sem_clockwait(CLOCK_MONOTONIC)` against an absolute deadline, `sem_trywait`; each retried on `EINTR` |

- The condition-variable wait runs, in order: `Zenith_CheckBlockingAllowed`, `Zenith_LockOrderCheckWait` and `Zenith_LockOrderPop` (both on the waited mutex's desc and address); on the profiled pair `Zenith_DispatchLockRelease(desc, mutex)`; the native wait inside a `Zenith_BlockingWaitScope<bProfiled>` named by the mutex (`xMutex.GetDesc().m_szName`, so the record is `wait:<mutex name>` and ten queues are ten sites); `Zenith_LockOrderPush`; and on the profiled pair `Zenith_DispatchLockAcquire(desc, mutex, 0)`. The checker sees the true held set ([Z2-N-CONTRACT-322]), the `lock:` hold on the mutex's site ends at the wait and begins again after it, and the time asleep, including the wake-up re-acquisition that happens inside the operating-system call, is the blocking-wait record; the acquire reports wait 0 because it cannot be separated from that sleep. The mutex outlives the wait, so the desc is referenced, not copied. The unprofiled pair makes none of the three dispatches.
- Every blocking call (`Wait`, `WaitFor`, `Zenith_Thread::Join`, `Zenith_SleepUs`, `Zenith_Semaphore::Wait`) runs `Zenith_CheckBlockingAllowed` with its primitive name before blocking, in either variant, so the job-worker check of [Z2-N-CONTRACT-315] applies to the unprofiled variants too. The profiled variants, `Zenith_Thread::Join` and `Zenith_SleepUs` then wait inside a `Zenith_BlockingWaitScope<true>`, whose destructor calls `Zenith_DispatchBlockingWait(name, ulWaitTicks)` after the wait has returned, in every build; the clock is read only when a blocking-wait hook is installed. The unprofiled variants read no hook table. The name a scope reports is the paired mutex's `m_szName` for a condition variable, the semaphore's own static-storage name (default `Zenith_Semaphore`) for a semaphore, and `Zenith_Thread::Join` or `Zenith_SleepUs` for those. `TryWait` and `NotifyOne` do not block and call nothing.
- Timed waits compute an absolute deadline from `CLOCK_MONOTONIC` (`pthread_condattr_setclock`), and `sem_clockwait` where available (bionic API 30 or above, satisfied by minSdk 34), so wall-clock changes never shorten or extend a wait.

## Lock profiling and hook dispatch

<!-- elaboration -->
This section specifies Z2-D-142 ([Z2-N-CONTRACT-589], [Z2-N-CONTRACT-590], [Z2-N-CONTRACT-591]): what a profiled primitive reports, the dispatch functions through which every profiling hook is invoked, and the three independent layers that make recursion through the profiler impossible. The profiler's side (per-site lock statistics, the `lockwait:` zones, the per-thread tables merged at `EndFrame`) is `Zenith2/Docs/Design/DEVT/Profiler.md` → "Lock and wait events".

### What the variants report

| Call | `Zenith_Mutex` (profiled) | `Zenith_MutexNoProfiling` |
|---|---|---|
| `Lock`, uncontended | the native try-lock succeeds; push; `Zenith_DispatchLockAcquire(desc, this, 0)` | native lock; push; no hook |
| `Lock`, contended | the native try-lock fails; with an acquire hook installed, read `Zenith_GetMonotonicTimeNs()`; block; push; read it again; `Zenith_DispatchLockAcquire(desc, this, wait)` | as uncontended |
| `TryLock` | success: push and `Zenith_DispatchLockAcquire(desc, this, 0)`; failure: no hook | success: push; failure: nothing |
| `Unlock` | pop; native unlock; `Zenith_DispatchLockRelease(desc, this)` | pop; native unlock |
| `Wait`, `WaitFor` of a condition variable or semaphore | `Zenith_DispatchBlockingWait(name, wait)` after the wait returns (the clock is read only with a blocking-wait hook installed); a condition variable also dispatches a lock release before the wait and a lock acquire with wait 0 after it | the job-worker check only |

- **What a `NoProfiling` variant still does.** It skips hook dispatch and nothing else. The lock-order checks apply unchanged (the level-registration check at construction, `Zenith_LockOrderCheckAcquire`, the held-lock stack, `Zenith_LockOrderCheckWait` and the condition-variable pop and push), and so does the asserts-only job-worker `ZENITH_CHECK` of [Z2-N-CONTRACT-315] through `Zenith_CheckBlockingAllowed` before every blocking call. The idle park of a job worker is not a violation of that check for the reason it never was: `m_bJobWorker` is true only while a job body runs, so a worker parked between bodies (the job system parks on a `Zenith_SemaphoreNoProfiling` and reports its own `jobs.park` wait) passes it, and a wait inside a job body fails it in either variant. A caller that wants a wait recorded under its own name dispatches `Zenith_DispatchBlockingWait` itself.
- **The wait unit.** `ulWaitTicks` is a difference of two `Zenith_GetMonotonicTimeNs()` readings, so one tick is one nanosecond on every platform ("Clocks" in `Zenith2/Docs/Design/CORE/Diagnostics.md`). It covers the time from the failed try-lock to the moment the native lock is held, so it includes the scheduling delay after the holder released. It is 0 for an acquire that did not block and for a successful `TryLock`, and at least 1 for one that did (`max(wait, 1)`), so 0 means "acquired without blocking". The same unit is used by the blocking-wait hook's `ulWaitTicks`.
- **The cost.** Every dispatch begins with `Zenith_GetProfilerHooks()`, an out-of-line call that returns the table, then a load and a branch; with no table installed (or the member null) that is the whole cost, paid twice by a profiled uncontended `Lock` and `Unlock` pair on top of the try-lock. With a table installed and the member set, a dispatch adds two more out-of-line calls on thread-local state (`Zenith_EnterHookDispatch`, `Zenith_LeaveHookDispatch`) and the indirect hook call, which returns early when the profiler's lock gate is off. All of these figures are pending measurement: DEVT's `Zenith_Bench_ProfilerOverhead` cases `Mutex_LockUnlock_Uncontended_NoTable`, `_GateOff` and `_Capture` set them, and the budgets of the zone, counter and lock rows are re-set from them. The acquire dispatch runs after the native lock, so its whole cost (with capture on, the profiler's record work) is inside the critical section and lengthens the hold the profiler measures; the release dispatch runs after the native unlock and does not. In a Shipping build the profiled lock operations skip the lookup altogether under `if constexpr (bProfiled && !Zenith_IsShippingBuild())` and cost what the unprofiled variant costs; a non-Shipping build whose table leaves the lock pair null pays the first cost only. A contended `Lock` adds the load of `Zenith_IsLockAcquireHookInstalled()` and reads no clock without an acquire hook.
- **A condition-variable wait.** A profiled wait reports the release of the mutex before the native wait and an acquire with wait 0 after it, so the `lock:` hold ends at the wait and begins again after it; the time asleep is the blocking-wait hook's `ulWaitTicks`, recorded as `wait:<primitive>` with a duration. The re-acquisition after the wake-up is inside the operating-system call and is part of that duration, not of the acquire's wait.
- **Thread safety.** A dispatch reads the hook table, which `main` writes before any other thread exists and after the last one ends (a test follows the same rule: it installs a table before it creates any thread, including the profiler's flush thread, and uninstalls only after joining them all, because the table is a plain struct and a concurrent install is a data race), and writes only the calling thread's own `m_uHookDepth`. What the hook does with its arguments is the profiler's: it writes only the calling thread's ring and lock-site table and takes no lock ("Hook implementation" in `Zenith2/Docs/Design/DEVT/Profiler.md`).

### Hook dispatch and the recursion guard

The six dispatch functions are declared, with bodies, in `Zenith2/Docs/Contracts.md` → "No mutable global state" (the code block of `Zenith_ProfilerHooks.h`). Each takes exactly the arguments of its hook less the user-data pointer, which the dispatch supplies from the table:

| Function | Hook it invokes |
|---|---|
| `void Zenith_DispatchZoneBegin(const Zenith_ZoneSite& xSite)` | `m_pfnOnZoneBegin` |
| `void Zenith_DispatchZoneEnd()` | `m_pfnOnZoneEnd` |
| `void Zenith_DispatchCounter(const char* szName, sint64 iValue, Zenith_CounterKind eKind)` | `m_pfnOnCounter` |
| `void Zenith_DispatchLockAcquire(const Zenith_LockDesc& xDesc, const void* pLock, uint64 ulWaitTicks)` | `m_pfnOnLockAcquire` |
| `void Zenith_DispatchLockRelease(const Zenith_LockDesc& xDesc, const void* pLock)` | `m_pfnOnLockRelease` |
| `void Zenith_DispatchBlockingWait(const char* szPrimitive, uint64 ulWaitTicks)` | `m_pfnOnBlockingWait` |

Each dispatch runs these steps:

1. Read `Zenith_GetProfilerHooks()`; when the hook member is null, return. This is the whole cost with no hook installed. The pairs are never half-set: `Zenith_InstallProfilerHooks` refuses, with `ZENITH_CHECK` in every build, a table in which exactly one of `m_pfnOnZoneBegin` and `m_pfnOnZoneEnd` is non-null, or exactly one of `m_pfnOnLockAcquire` and `m_pfnOnLockRelease` is non-null, so a dispatched begin always has a dispatchable end, and callers never store hook pointers to pair them.
2. Call `Zenith_EnterHookDispatch()` (`Internal/Profiling/Zenith_DebugThreadState.cpp`). When `g_xDebugThreadState.m_uHookDepth` is non-zero it increments `m_uHookReentryCount`; when the count has just become 1 it writes one line, `ZENITH PROFILER: hook re-entered on this thread: a hook used a profiled primitive`, through `Zenith_WriteDiagnosticLine` (no lock, no allocation, so a build without asserts still reports the first refusal of each thread once); then it fails `ZENITH_ASSERT(false, "profiler hook re-entered: a hook used a profiled primitive")` wherever asserts are enabled, and returns false; the dispatch then returns without calling the hook. Otherwise it sets the depth to 1 and returns true.
3. Call the hook with the arguments and the table's `m_pUserData`.
4. Call `Zenith_LeaveHookDispatch()`, which sets the depth to 0.

- The depth is 0 or 1: a refused dispatch never raises it. A hook that throws does not exist (exceptions are off outside the Luau firewall), so the leave call always runs.
- `Zenith_GetHookDepth()` and `Zenith_GetHookReentryCount()` return the calling thread's two members, for tests; nothing sums them across threads, because no thread reads another's thread-local state, which is why the first refusal on each thread writes its own diagnostic line.
- The two out-of-line calls of steps 2 and 4 are made only when a hook is installed; their cost is part of the dispatch cost under "What the variants report" and of the budgets the profiler's overhead benchmark sets.
- **The log-record hooks are not dispatched through the guard.** `m_pfnOnLogRecord` and `m_pfnIsLogLevelEnabled` belong to the log router, which `Zenith_EmitLog` and the failure procedure call directly ("The log-record hook" in `Zenith2/Docs/Design/CORE/Diagnostics.md`). If they were dispatched, the fatal record that step 5 of the failure procedure emits from inside a hook that asserted (including the re-entry assertion above) would itself be refused and lost, and a warning logged by a profiler path would vanish. The router's recursion safety is by type instead: its locks are `Zenith_MutexNoProfiling` and its drain thread waits on a `Zenith_SemaphoreNoProfiling` (layer (a)); a sink that takes a profiled primitive inside `Write` is caught by layer (c), because the sink runs under the router's call. The router never dispatches a counter from `OnRecord` (its ring counters are read through `GetStats`), and a thread-local flag in the Debug thread state, `m_bInLogRouter`, makes a record logged from inside `OnRecord` (a sink or a hook that logs) a counted drop in `m_uLogNestedDropCount` instead of a second take of the sink lock; a hook never logs because a profiler thread state ran out, since a hook may run under a lock above the sink lock (the profiler defers that warning to `EndFrame`).
- `Zenith_IsZoneHookInstalled()` (zone-begin hook installed, and therefore zone-end), `Zenith_IsLockAcquireHookInstalled()` and `Zenith_IsBlockingWaitHookInstalled()` are the three sanctioned cost gates: a caller gates work done only to build a zone (formatting a name) on the first, a profiled `Lock` gates its clock reads on the second, and a blocking primitive gates its clock reads around the wait on the third. They are the only null tests outside the dispatch functions, and F26 (ii) admits them because they live in `Zenith_ProfilerHooks.h`.
- No code outside `Zenith_ProfilerHooks.h` calls a hook member other than the two log members; rule F26 (ii) refuses it ([Z2-N-CONTRACT-591]).

### The three layers

1. **Types (layer (a)).** The profiler (every source under `Zenith2/Engine/ZenithProfiling/`) and every ZenithCore source a hook dispatch can reach name only `Zenith_MutexNoProfiling`, `Zenith_ConditionVariableNoProfiling` and `Zenith_SemaphoreNoProfiling`. The profiler's capture-segment queue mutex (level 940) is a `Zenith_MutexNoProfiling`, and its flush thread waits on unprofiled primitives. The old rule that a lock at level 940 is never recorded is withdrawn as a recursion defence: the type makes it unnecessary, and level 940 is an ordinary row of the lock ordering table.
2. **Gate (layer (b)).** `ForbiddenCheck` rule F26 refuses a profiled `Zenith_Mutex`, `Zenith_ConditionVariable` or `Zenith_Semaphore`, or a `<true>` platform template, in `Zenith2/Engine/ZenithProfiling/**`, `Zenith2/Engine/ZenithCore/Internal/Profiling/**` and every file of the `profilerReachable` list, and refuses a direct hook-table call outside `Zenith_ProfilerHooks.h` (`Zenith2/Docs/Design/PROC/StructuralGates.md` → "ForbiddenCheck rule table"). The next subsection is CORE's contribution to the `profilerReachable` list.
3. **Run time (layer (c)).** The dispatch guard above, in every build. Rule F26 is lexical and has holes: it cannot see a pointer stored from the hook table and called later, a CORE type that embeds a profiled mutex and is used from profiler code (a `Zenith_CallbackRegistry`, for example), or a log sink's `Write` that takes a profiled lock (DEVT's file sink and console sources are in the `profilerReachable` list for that reason, and review checks the rest); layer (c) covers all of them at run time. A hook that breaks the contract (Tracy's, a test's, a future profiler's) recurses at most one level; a build with asserts reports it at the first occurrence, and any build counts it. The profiler's own `m_bInProfiler` remains as defence in depth.

### ZenithCore sources a hook dispatch reaches

A profiler hook runs inside a dispatch, so the ZenithCore code it calls, and the code those calls reach when something fails, may not use a profiled primitive. The rows below are the `profilerReachable` contribution of CORE (the debug thread state, `Zenith_ProfilerHooks.cpp` and the lock-order checker lie under `Internal/Profiling/`, which F26 (i) covers by directory). Paths are relative to `Zenith2/Engine/ZenithCore/`.

| File | Why a hook dispatch reaches it | Primitives |
|---|---|---|
| `Zenith_ProfilerHooks.h` | the dispatch functions themselves | none |
| `Zenith_Clock.h`, `Internal/Windows/Zenith_Clock_Windows.cpp`, `Internal/Android/Zenith_Clock_Android.cpp`, `Internal/Linux/Zenith_Clock_Linux.cpp` | the hooks timestamp with `Zenith_GetMonotonicTimeNs()` | none (stateless OS clock reads) |
| `Zenith_SpscRing.h` | the per-thread event rings of the profiler | none (wait-free, atomics only) |
| `Internal/Threading/Zenith_Thread.cpp`, `Internal/Windows/Zenith_Thread_Windows.cpp`, `Internal/Android/Zenith_Thread_Android.cpp`, `Internal/Linux/Zenith_Thread_Linux.cpp` | `Zenith_GetCurrentThreadID` and `Zenith_SetThreadName` at the claim of a profiler thread slot | none; `Join` and `Zenith_SleepUs` call the blocking-wait dispatch, which no hook may reach |
| `Internal/Diagnostics/Zenith_Fail.cpp`, `Internal/Windows/Zenith_Diagnostics_Windows.cpp`, `Internal/Android/Zenith_Diagnostics_Android.cpp`, `Internal/Linux/Zenith_Diagnostics_Linux.cpp`, `Internal/Windows/Zenith_Terminate_Windows.cpp`, `Internal/Android/Zenith_Terminate_Android.cpp`, `Internal/Linux/Zenith_Terminate_Linux.cpp` | an assertion inside a hook, including the re-entry assertion, runs the failure procedure | none |
| `Zenith_Log.h`, `Zenith_LogRouter.h`, `Zenith_LogSinks.h`, `Internal/Diagnostics/Zenith_Log.cpp`, `Internal/Diagnostics/Zenith_LogRouter.cpp`, `Internal/Diagnostics/Zenith_LogSinks.cpp` | a hook that logs, and the fatal record of the failure procedure | `Zenith_MutexNoProfiling` at levels 920 and 930, `Zenith_SemaphoreNoProfiling` on the drain thread |

- `Zenith_Span`, `Zenith_StringView`, `Zenith_Format` and `Zenith_Types` are pure value code that names no primitive and are not listed. `Zenith_Heap` is not listed because a hook allocates nothing (the hook-function row of `Zenith2/Docs/Design/DEVT.md` states it); `Zenith_StringTable` is not listed because the profiler keeps its own name table.
- DEVT's own log-sink and console sources join the `profilerReachable` list as well (a PROC hand-off over DEVT-owned files): a sink's `Write` runs inside a hook that logs.
- A file added to this table is added to the `profilerReachable` list by a PROC hand-off, never by the ticket that makes the file reachable.

## Debug thread state

`g_xDebugThreadState` is the `thread_local` variable in `Zenith2/Engine/ZenithCore/Internal/Profiling/Zenith_DebugThreadState.cpp` allowlisted by `Zenith2/Docs/Contracts.md` → "Writable-data allowlist entries" ([Z2-N-CONTRACT-152]), an exemption of Z2-D-046 extended 2026-10-16 by ruling Q11 (a) ([Z2-N-CONTRACT-144]); Z2-D-142 adds two members and no variable, so the entry gains no extension line. Its definition, and its `extern` declaration in `Internal/Profiling/Zenith_DebugThreadState.h`, each carry the declaration comment block of [Z2-N-CONTRACT-578]:

| Field | Type | Maintained |
|---|---|---|
| `m_axHeld` | `Zenith_HeldLock[16]` (`{ uint16 level; const void* address; const char* name; }`) | wherever `ZENITH_ASSERTS_ENABLED=1` |
| `m_uHeldCount` | `uint32` | same |
| `m_bJobWorker` | `bool` | every build: true exactly while a job body runs on the thread (`Zenith_MarkThreadAsJobWorker(true)` on body entry, `(false)` on exit), so a worker's own idle park between bodies is not a job blocking ([Z2-N-CONTRACT-315] checks job bodies, not threads) |
| `m_bInOOMHandler` | `bool` | every build (`Zenith2/Docs/Design/CORE/Memory.md` → "Emergency reserve and the default handler") |
| `m_bInFailure` | `bool` | every build (`Zenith2/Docs/Design/CORE/Diagnostics.md` → "Failure procedure") |
| `m_uNoAllocationDepth` | `uint32` | wherever `ZENITH_ASSERTS_ENABLED=1`: the nesting depth of `Zenith_NoAllocationScope` on the thread ("Zenith_NoAllocationScope") |
| `m_uHookDepth` | `uint32` | every build, by `Zenith_EnterHookDispatch` and `Zenith_LeaveHookDispatch`: 1 exactly while a profiling hook invoked through a `Zenith_Dispatch…` function runs on the thread, otherwise 0 ("Lock profiling and hook dispatch") |
| `m_uHookReentryCount` | `uint32` | every build: the number of dispatches this thread refused because it was already inside a hook; never reset |
| `m_bInLogRouter` | `bool` | every build: true while `Zenith_LogRouter::OnRecord` runs on the thread ("Log router" in `Zenith2/Docs/Design/CORE/Diagnostics.md`) |
| `m_uLogNestedDropCount` | `uint32` | every build: the number of non-fatal records this thread logged from inside `OnRecord` and the router dropped; never reset |

The struct is identical in every configuration; configurations differ only in which fields their code updates ([Z2-N-CONTRACT-444]). The `Why:` line of the declaration block ([Z2-N-CONTRACT-578]) and the `comment` of the allowlist entry read: the per-thread state the lock-order checker needs (held-lock stack), with the job-worker, OOM and failure flags, the no-allocation depth, and the hook depth and re-entry count that stop a profiler hook from recursing, and the router's nested-record flag and count. `Zenith_EnterHookDispatch`, `Zenith_LeaveHookDispatch`, `Zenith_GetHookDepth` and `Zenith_GetHookReentryCount` are defined in `Zenith_DebugThreadState.cpp`, beside the no-allocation functions, as one function each.

## Zenith_NoAllocationScope

<!-- elaboration -->
A no-allocation scope makes a region of code that must never allocate fail loudly in every asserting configuration. AUDIO's device callback holds one for its whole body (row AU6); any other real-time region may use it too. The depth is the Debug thread state field `m_uNoAllocationDepth` (table "Debug thread state"), so the struct keeps one layout in every configuration ([Z2-N-CONTRACT-444]). Only the code that updates and checks the field differs by configuration.

```cpp
// Zenith2/Engine/ZenithCore/Zenith_NoAllocationScope.h
/// @contract functions Zenith_NoAllocationScope
/// Ownership: Per-thread depth in the Debug thread state; no allocation.
/// Thread affinity: ANY_THREAD; each call affects only the calling thread.
/// Invalidation: None.
/// Units: None.
/// Errors: Wherever ZENITH_ASSERTS_ENABLED=1, Zenith_Heap::Allocate, TryAllocate and Free on a thread whose depth is
///   non-zero fail ZENITH_ASSERT with the message "allocation inside Zenith_NoAllocationScope"; Leave at depth 0 is
///   ZENITH_ASSERT; a depth above 2^31 is ZENITH_ASSERT. With ZENITH_ASSERTS_ENABLED=0 every member is an empty
///   inline body and the heap checks nothing.
/// Version: since Z2 Stage 2.
/// @endcontract
void Zenith_EnterNoAllocationScope();   // ++depth on the calling thread
void Zenith_LeaveNoAllocationScope();   // --depth; ZENITH_ASSERT(depth > 0)
uint32 Zenith_GetNoAllocationDepth();   // the calling thread's depth (0 where asserts are off)

class Zenith_NoAllocationScope
{
public:
	Zenith_NoAllocationScope() { Zenith_EnterNoAllocationScope(); }
	~Zenith_NoAllocationScope() { Zenith_LeaveNoAllocationScope(); }
	Zenith_NoAllocationScope(const Zenith_NoAllocationScope&) = delete;
	Zenith_NoAllocationScope& operator=(const Zenith_NoAllocationScope&) = delete;
};
```

### No-allocation check steps

1. `Zenith_EnterNoAllocationScope`, `Zenith_LeaveNoAllocationScope` and `Zenith_GetNoAllocationDepth` are defined in `Internal/Profiling/Zenith_DebugThreadState.cpp` as one function each, whose body is selected by `ZENITH_ASSERTS_ENABLED` ([Z2-N-CONTRACT-444]). With asserts enabled they increment, decrement (after `ZENITH_ASSERT(depth > 0)`) and read `g_xDebugThreadState.m_uNoAllocationDepth`. With asserts disabled they do nothing, and Get returns 0.
2. `Zenith_Heap::Allocate`, `TryAllocate` and `Free` in `Internal/Memory/Zenith_Heap.cpp` begin, wherever `ZENITH_ASSERTS_ENABLED=1`, with `ZENITH_ASSERT(g_xDebugThreadState.m_uNoAllocationDepth == 0, "allocation inside Zenith_NoAllocationScope")`. `Free(nullptr)` is checked too, so a stray deallocation path is caught even when it receives nothing to free.
   - The check runs before the backing call and before the leak table, so the assert report (which allocates nothing: `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Failure procedure") is the only work done.
   - Arenas and pools are not checked: they allocate from blocks their owner already holds. An arena growth reaches `Zenith_Heap::TryAllocate` and is caught there.
3. The scope never changes allocation results in a non-asserting configuration, so `win-release` and `Shipping` builds behave identically with and without it.

## Zenith_ReentrancyGuard

```cpp
// Zenith2/Engine/ZenithCore/Zenith_ReentrancyGuard.h
/// @contract type Zenith_ReentrancyGuard
/// Ownership: A member of a type whose Reentrancy field is NONE or ALLOWED(…) ([Z2-N-CONTRACT-351]).
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: CheckNotInCallback on the invoking thread during a callback is ZENITH_ASSERT; Exit without Enter is
///   ZENITH_ASSERT.
/// Version: since Z2 Stage 1. 8 bytes, identical in every configuration.
/// @endcontract
class Zenith_ReentrancyGuard
{
public:
	void Enter();                           // records the thread and increments the depth
	void Exit();
	void CheckNotInCallback() const;        // asserts the calling thread is not the invoking one
private:
	std::atomic<uint32> m_uThreadID{ 0 };           // low 32 bits of the invoking thread's ID; 0 when idle
	uint32 m_uDepth = 0;
};
static_assert(sizeof(Zenith_ReentrancyGuard) == 8);
```

`Enter` and `Exit` update the fields in every build; `CheckNotInCallback` compares only where `ZENITH_ASSERTS_ENABLED=1`. Thread IDs are 32-bit on every supported platform (`DWORD`; `pid_t` from `gettid`), so the low 32 bits are the whole ID.

## Zenith_CallbackRegistry implementation

The declarations are `Zenith2/Docs/Contracts.md` → "Threading and concurrency contract" (subsection "Callback lifetime"), in `Zenith2/Engine/ZenithCore/Zenith_CallbackRegistry.h`. The registry is constructed with `Zenith_CallbackRegistryDesc { const char* m_szName; uint32 m_uMaxEntries = 256; }` (every registry is bounded, [Z2-N-CONTRACT-342]); `Register` beyond the cap returns `ALLOC_ERROR_CAPACITY_EXCEEDED`.

Entry storage: pages of 64 entries from the root heap (addresses stable, so a thread spinning on an entry's flag never sees it move), each entry `{ PfnCallback m_pfnCallback; void* m_pUserData; uint32 m_uGeneration; std::atomic<uint32> m_uState; }` with state bits `LIVE = 1`, `REMOVED = 2`, `IN_FLIGHT = 4`. The registry base holds `std::atomic<uint64> m_ulDispatchingThread` (0 when idle), a `Zenith_Mutex` at `uLOCK_LEVEL_CALLBACK_REGISTRY`, a free list of entry indices, and a deferred-destroy list (`Zenith_Array` of `{ object, destroy function }`).

`Dispatch(xArgs…)`:

1. `ZENITH_ASSERT` the registry is not already dispatching on this thread (Reentrancy `NONE` for `Dispatch`); record the dispatching thread.
2. Under the lock read the current high-water entry count `n`; release. Entries registered during the dispatch are not called by it.
3. For each index `i < n`: take the lock; if the entry is `LIVE` and not `REMOVED`, set `IN_FLIGHT` and copy the function pointer and user data; release; call the callback with the copies; then clear `IN_FLIGHT` with `store(release)` on the registry-owned state word (the dispatcher reads no user data after the call returns, [Z2-N-CONTRACT-332]).
4. After the loop: take the lock; move every `REMOVED` entry that is not `IN_FLIGHT` to the free list with its generation incremented; release; clear the dispatching thread; run and clear the deferred-destroy list in order.

`Unregister(xToken)`:

1. A null or stale token (generation mismatch) is a no-op.
2. Take the lock; set `REMOVED`; if no dispatch is running, free the entry at once; release.
3. If the calling thread is the dispatching thread, return (self-unregistration and unregistration of another entry during dispatch never wait, [Z2-N-CONTRACT-327], [Z2-N-CONTRACT-328]).
4. Otherwise spin with `Zenith_YieldThread` while `IN_FLIGHT` is set ([Z2-N-CONTRACT-326]); the lock is not held while spinning. The spin is a blocking wait for [Z2-N-CONTRACT-315]'s purposes when the entry is in flight on another thread, so `ZENITH_CHECK` fails it on a job worker.

`DeferDestroy(p, pfn)` outside a dispatch runs `pfn(p)` at once.

## Zenith_OperationHandle implementation

`Zenith_OperationHandle` is declared in `Zenith2/Engine/ZenithCore/Zenith_OperationHandle.h` exactly as `Zenith2/Docs/Contracts.md` → "Zenith_OperationHandle" states, including `GetFailureCode()` (the issuing subsystem's error enumerator value, 0 unless FAILED, [Z2-N-CONTRACT-101]) and `GetSlotID()` (slot and generation, for diagnostics and handle tests). CORE adds its error enumeration and the table below:

```cpp
enum Zenith_OperationError : uint8
{
	OPERATION_ERROR_TABLE_FULL = 1,       // Issue at the table's cap
	OPERATION_ERROR_OUT_OF_MEMORY,        // Create could not allocate the slots
	OPERATION_ERROR_NULL_HANDLE,
	OPERATION_ERROR_STALE_HANDLE,
	OPERATION_ERROR_FOREIGN_HANDLE,
};

struct Zenith_OperationTableDesc
{
	const char* m_szName = "operations";
	uint32 m_uMaxOperations = 0;         // the issuing subsystem's in-flight row (e.g. streaming requests)
};

/// @contract type Zenith_OperationTable
/// Ownership: Created by Create as a Zenith_Owned<Zenith_OperationTable> and owned by the issuing subsystem;
///   outlives every handle and ticket it issued (shutdown step 4 of Zenith2/Docs/Contracts.md "Threading and
///   concurrency contract" completes every operation first).
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: Progress in [0, 1].
/// Errors: Issue returns OPERATION_ERROR_TABLE_FULL; QueryStatus returns the handle errors; destruction with a
///   live slot is ZENITH_CHECK.
/// Version: since Z2 Stage 2. Source API.
/// Locks: uLOCK_LEVEL_OPERATION_TABLE (380, short-hold) for Issue and slot release only.
/// @endcontract
class Zenith_OperationTable
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_OperationTable>, Zenith_OperationError> Create(const Zenith_OperationTableDesc& xDesc);
	Zenith_Result<Zenith_OperationIssue, Zenith_OperationError> Issue();
	Zenith_Result<Zenith_OperationStatus, Zenith_OperationError> QueryStatus(Zenith_SlotID xID) const;
	uint32 GetLiveCount() const;
};

struct Zenith_OperationIssue
{
	Zenith_OperationHandle m_xHandle;     // for the requester
	Zenith_OperationTicket m_xTicket;     // for the executor
};

/// @contract type Zenith_OperationTicket
/// Ownership: The executor's half of an operation; move-only; completing it releases the executor's reference.
/// Thread affinity: OWNER_THREAD (the executor thread that holds it; it may be handed over at a graph edge).
/// Invalidation: None.
/// Units: Progress in [0, 1].
/// Errors: completing twice, or destroying an uncompleted ticket that holds a slot, is ZENITH_CHECK. A
///   default-constructed ticket is null (the failure value of Zenith_Result<Zenith_OperationIssue, …>).
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_OperationTicket
{
public:
	constexpr Zenith_OperationTicket() = default;   // the null ticket: no slot; destroying it checks nothing
	void SetRunning();
	void SetProgress(float fProgress);                 // clamped to [current, 0.999999]; never decreases
	bool IsCancelRequested() const;      // acquire load
	void CompleteSucceeded();
	void CompleteFailed(uint16 uFailureCode);         // ZENITH_ASSERT uFailureCode != 0
	void CompleteCancelled();                          // after releasing everything the operation held
	uint64 GetIssueID() const;
};
```

Slot layout (a fixed array of `m_uMaxOperations` slots allocated by `Create`, 64-byte aligned): `std::atomic<uint8> m_eStatus`, `std::atomic<uint32> m_uProgressBits` (float bits), `std::atomic<bool> m_bCancelRequested`, `std::atomic<uint8> m_uReferences` (2 at issue: handle and ticket), `std::atomic<uint16> m_uFailureCode`, `uint32 m_uGeneration`, `uint64 m_ulIssueID`.

| Event | Effect |
|---|---|
| `Issue` | lock; pop a free slot (FIFO) or `OPERATION_ERROR_TABLE_FULL`; status `PENDING`, progress 0, references 2, issue ID from a per-table counter (strictly increasing from 1); unlock |
| `RequestCancel` (handle) | `m_bCancelRequested.store(true, release)`; idempotent; never blocks ([Z2-N-CONTRACT-334]) |
| `SetRunning` | `PENDING → RUNNING` (release store) |
| `SetProgress(p)` | CAS loop raising the stored progress to `min(p, 0.999999f)` when larger |
| `CompleteSucceeded` | store `SUCCEEDED` (release), then progress 1.0 (release), then drop the executor's reference; a reader that sees progress 1.0 sees `SUCCEEDED` ([Z2-N-CONTRACT-094]) |
| `CompleteFailed(c)` | store the failure code, then `FAILED` (release), then drop the reference |
| `CompleteCancelled` | store `CANCELLED` (release), then drop the reference; the executor released its resources before this call ([Z2-N-CONTRACT-335]) |
| handle destruction or move-assignment | if not complete: `RequestCancel`; drop the requester's reference ([Z2-N-CONTRACT-091]) |
| last reference dropped | lock; increment the generation (retiring the slot at `0xFFFFFFFF`); push the slot on the FIFO; unlock |

- `GetSlotID().m_uSlot` is always below the table's `m_uMaxOperations`, so an issuing subsystem may keep per-operation executor state (for example JOBS's move-only `Zenith_OperationTicket` side array) in an array indexed by it.
- A cancellation that races completion follows [Z2-N-CONTRACT-337]: the executor that has already completed successfully reports `SUCCEEDED`.
- The requester's typed result accessors live in the issuing subsystem, keyed by the handle's slot, and are valid while the handle is held and the status is `SUCCEEDED` ([Z2-N-CONTRACT-092]).
- Every operation kind carries the 1,000-cycle issue-then-cancel stress test of [Z2-N-CONTRACT-339] in its own library; CORE carries the table's.

## Threading tests

In a Shipping build the profiled lock operations skip the hook lookup ([Z2-N-CONTRACT-589]), so every case below that depends on a lock dispatch takes the two-branch form: `if constexpr (Zenith_IsShippingBuild())` asserts that no lock record is produced and the primitive still works, otherwise the case runs as described. Tests install their hook table before any other thread exists, including a flush thread, and uninstall it after joining ([Z2-N-CONTRACT-143]).

| Test ID | File | Asserts | Suite, presets |
|---|---|---|---|
| `Test_Thread_CreateJoin_RunsEntryOnce` | `Zenith_Thread_Tests.cpp` | the entry ran once with its user data; `IsJoinable` false after `Join` | default |
| `Test_Thread_DestroyJoinable_Checks` | `Zenith_Thread_DefectTests.cpp` | [Z2-N-CONTRACT-313] | `z2-death-check` |
| `Test_Thread_Create_FailsAtStartBlock_Unwinds` | `Zenith_Thread_Tests.cpp` | with the heap countdown at 1, `THREAD_ERROR_OUT_OF_MEMORY`, no thread started, no leak | default |
| `Test_Thread_Create_FailsAtNativeCreate_Unwinds` | `Zenith_Thread_Tests.cpp` | with a test injector failing `uFAULT_POINT_THREAD_NATIVE_CREATE`, `THREAD_ERROR_CREATE_FAILED`, the entry never runs, the process thread count is unchanged and the start block is freed (zero leaks) | default |
| `Test_Thread_InvalidDesc_Rejected` | `Zenith_Thread_Tests.cpp` | null entry, empty name, 16-byte name, 32 KiB stack | default |
| `Test_Threading_PosixParity` | `Zenith_Thread_Tests.cpp` | the threading suite's cases produce the same results under `linux-tsan` and `android-x86_64` (the case set is identical; this test is the named inventory of it) | default |
| `Test_LockOrder_Inversion_IsReported` | `Zenith_LockOrder_DefectTests.cpp` | [Z2-N-CONTRACT-324] | `z2-death-check` (asserts-enabled presets) |
| `Test_LockOrder_SameLevelDescendingAddress_IsReported` | `Zenith_LockOrder_DefectTests.cpp` | [Z2-N-CONTRACT-324] | `z2-death-check` |
| `Test_LockOrder_AscendingLevels_Passes` | `Zenith_LockOrder_Tests.cpp` | [Z2-N-CONTRACT-324] | default |
| `Test_LockOrder_TryLockOutOfOrder_Allowed` | `Zenith_LockOrder_Tests.cpp` | a try-lock of a lower level succeeds and a following blocking lock above every held level passes | default |
| `Test_LockOrder_Recursive_Checks` | `Zenith_LockOrder_DefectTests.cpp` | locking a held mutex fails `ZENITH_CHECK` | `z2-death-check` |
| `Test_LockOrder_UnregisteredLevel_Checks` | `Zenith_LockOrder_DefectTests.cpp` | a desc level with no row fails at construction ([Z2-N-CONTRACT-320]) | `z2-death-check` |
| `Test_LockOrder_Overflow_Checks` | `Zenith_LockOrder_DefectTests.cpp` | a 17th held lock fails | `z2-death-check` |
| `Test_Mutex_Contention_Stress` | `Zenith_Mutex_Tests.cpp` | 2, 8 and hardware-concurrency threads incrementing a guarded counter 10^6 times reach the exact total; `linux-tsan` clean | `z2-stress` |
| `Test_PlatformThreading_Aliases_NotCopyableNotMovable` | `Zenith_PlatformThreading_Tests.cpp` | through the six aliases alone, so it compiles against each platform's class templates in its own presets: `Zenith_Mutex`, `Zenith_ConditionVariable`, `Zenith_Semaphore` and their `NoProfiling` aliases are neither copy- nor move-constructible nor assignable; `bIS_PROFILED` is true on the first three and false on the others; `Zenith_Mutex::GetDesc` returns the desc's name and level; `Zenith_ScopedLock` deduces its mutex type for both mutex aliases | default, every preset |
| `Test_Mutex_TryLock_HeldByOtherThread_Fails` | `Zenith_Mutex_Tests.cpp` | `TryLock` on a mutex another `Zenith_Thread` holds returns false and leaves the calling thread's held stack unchanged; after that thread unlocks, `TryLock` returns true | default |
| `Test_Semaphore_PostBeyondMax_Checks` | `Zenith_Semaphore_DefectTests.cpp` | `Post(2)` on a semaphore at count 1 with maximum 2 fails `ZENITH_CHECK`: the `ReleaseSemaphore` refusal on Windows, the `m_uMaxValue` check on Android and Linux | `z2-death-check` |
| `Test_ConditionVariable_WaitFor_TimesOut` | `Zenith_ConditionVariable_Tests.cpp` | returns false within the timeout plus 50 ms | default |
| `Test_ConditionVariable_ShortHoldMutex_Checks` | `Zenith_ConditionVariable_DefectTests.cpp` | waiting with a short-hold mutex fails | `z2-death-check` |
| `Test_Semaphore_PostWait_Counts` | `Zenith_Semaphore_Tests.cpp` | counts across threads; `TryWait` at zero returns false | default |
| `Test_BlockingWait_OnJobWorker_Checks` | `Zenith_Thread_DefectTests.cpp` | `Zenith_SleepUs`, semaphore and condition waits between `Zenith_MarkThreadAsJobWorker(true)` and `(false)` fail ([Z2-N-CONTRACT-315]) | `z2-death-check` |
| `Test_BlockingWait_BetweenJobBodies_Allowed` | `Zenith_Thread_Tests.cpp` | the same waits after `Zenith_MarkThreadAsJobWorker(false)` on the same thread succeed, as a worker's idle park does | default |
| `Test_BlockingWait_CallsHook` | `Zenith_Thread_Tests.cpp` | each blocking primitive of the profiled variants, `Zenith_Thread::Join` and `Zenith_SleepUs` call the installed blocking-wait hook with their name (a condition variable with its mutex's desc name, a semaphore with its own name or `Zenith_Semaphore`), after the wait returns, through `Zenith_DispatchBlockingWait`; a wait of 30 ms reports at least 20,000,000 ticks and a wait that returns at once reports a small value, and with no blocking-wait hook installed the clock is not read (`Zenith_IsBlockingWaitHookInstalled()` false) | default |
| `Test_NoAllocationScope_ApiSurface_NotCopyable` | `Zenith_RealTimeChannels_ApiSurface_Tests.cpp` | the RAII class is neither copy-constructible nor copy-assignable, and `Zenith_GetNoAllocationDepth()` returns `uint32` | default |
| `Test_NoAllocationScope_HeapAllocateAsserts` | `Zenith_NoAllocationScope_DefectTests.cpp` | `Zenith_Heap::Allocate` and `TryAllocate` inside a scope fail with `ZENITH ASSERT FAILED:` and the scope's message | `z2-death-assert` |
| `Test_NoAllocationScope_HeapFreeAsserts` | `Zenith_NoAllocationScope_DefectTests.cpp` | `Zenith_Heap::Free` of a block allocated before the scope, and `Free(nullptr)`, fail inside it | `z2-death-assert` |
| `Test_NoAllocationScope_LeaveAtZero_Asserts` | `Zenith_NoAllocationScope_DefectTests.cpp` | `Zenith_LeaveNoAllocationScope` with no scope entered | `z2-death-assert` |
| `Test_NoAllocationScope_NestingAndRelease` | `Zenith_NoAllocationScope_Tests.cpp` | two nested scopes give depth 2 then 1 then 0 (in asserting presets; 0 throughout in `win-release`); after the outer scope ends the thread allocates and frees normally | default |
| `Test_NoAllocationScope_OtherThreadsUnaffected` | `Zenith_NoAllocationScope_Tests.cpp` | while one `Zenith_Thread` holds a scope, another allocates and frees 1,000 blocks; the holder pushes and pops a `Zenith_SpscRing` over caller storage without failing | default |
| `Test_NoAllocationScope_ArenaFromHeldBlockAllowed` | `Zenith_NoAllocationScope_Tests.cpp` | an arena allocation served from its current block inside a scope succeeds (only a growth reaching the heap is checked) | default |
| `Test_ReentrancyGuard_CallbackReentry_Asserts` | `Zenith_ReentrancyGuard_DefectTests.cpp` | `CheckNotInCallback` inside `Enter`/`Exit` on the same thread | `z2-death-assert` |
| `Test_ReentrancyGuard_OtherThread_Passes` | `Zenith_ReentrancyGuard_Tests.cpp` | another thread's check passes while a callback runs | default |
| `Test_CallbackRegistry_SelfUnregister_DoesNotWait` | `Zenith_CallbackRegistry_Tests.cpp` | [Z2-N-CONTRACT-333] | default |
| `Test_CallbackRegistry_UnregisterOtherDuringDispatch_SkipsIt` | `Zenith_CallbackRegistry_Tests.cpp` | [Z2-N-CONTRACT-333] | default |
| `Test_CallbackRegistry_CrossThreadUnregister_WaitsForInFlight` | `Zenith_CallbackRegistry_Tests.cpp` | [Z2-N-CONTRACT-333]; also `linux-tsan` | default, `z2-stress` |
| `Test_CallbackRegistry_DestroyDuringDispatch_IsDeferred` | `Zenith_CallbackRegistry_Tests.cpp` | [Z2-N-CONTRACT-333] | default |
| `Test_CallbackRegistry_RegisterDuringDispatch_NotCalledThisDispatch` | `Zenith_CallbackRegistry_Tests.cpp` | the new entry is called from the next dispatch | default |
| `Test_CallbackRegistry_Cap_ReturnsCapacityExceeded` | `Zenith_CallbackRegistry_Tests.cpp` | the 257th registration fails and changes nothing | default |
| `Test_CallbackRegistry_Reentrancy_NestedDispatch_Asserts` | `Zenith_CallbackRegistry_DefectTests.cpp` | a callback dispatching its own registry | `z2-death-assert` |
| `Test_OperationHandle_NullDoesNotResolve` | `Zenith_OperationHandle_Tests.cpp` | [Z2-N-CONTRACT-096] | default, every preset |
| `Test_OperationHandle_StaleAfterDestroy_DoesNotResolve` | `Zenith_OperationHandle_Tests.cpp` | `QueryStatus` of a released slot's old ID returns `OPERATION_ERROR_STALE_HANDLE` | default, every preset |
| `Test_OperationHandle_ForeignOwner_DoesNotResolve` | `Zenith_OperationHandle_Tests.cpp` | another table's ID returns `OPERATION_ERROR_FOREIGN_HANDLE` | default, every preset |
| `Test_OperationHandle_GenerationExhaustion_RetiresSlot` | `Zenith_OperationHandle_Tests.cpp` | a slot released at generation `0xFFFFFFFE` is never reissued | default, every preset |
| `Test_OperationHandle_DestroyIncomplete_RequestsCancel` | `Zenith_OperationHandle_Tests.cpp` | the ticket observes the cancel; the slot is released only after the ticket completes | default |
| `Test_OperationHandle_ProgressReachesOneOnlyOnSuccess` | `Zenith_OperationHandle_Tests.cpp` | `SetProgress(1.0f)` stores less than 1; `CompleteSucceeded` then 1.0; a concurrent reader never sees 1.0 with another status (`z2-stress`, `linux-tsan`) | default, `z2-stress` |
| `Test_OperationHandle_CancelRaceWithSuccess_ReportsSucceeded` | `Zenith_OperationHandle_Tests.cpp` | [Z2-N-CONTRACT-337] | default |
| `Test_OperationTable_IssueCancelStress_NoLeak` | `Zenith_OperationHandle_Tests.cpp` | 1,000 issue-then-cancel cycles at randomised stages on 4 executor threads leave zero live slots and zero leaks ([Z2-N-CONTRACT-339]) | `z2-stress` |
| `Test_Mutex_LockHooks_Called` | `Zenith_Mutex_Tests.cpp` | with `Zenith_CapturingHooks` installed, an uncontended `Lock` and `Unlock` of a `Zenith_Mutex` produce one acquire record with the desc name and wait 0, then one release record | default |
| `Test_Mutex_Contended_ReportsMeasuredWait` | `Zenith_Mutex_Tests.cpp` | a handshake, not a sleep race: the holder thread locks a `Zenith_Mutex` and sets an atomic flag; the test thread waits for the flag, sets a second flag and locks; the holder waits for the second flag, sleeps 50 ms and unlocks. The test thread's acquire record's wait is at least 10,000,000 ticks (10 ms) and below 5 s; the holder's own acquire recorded wait 0 (never 1 or more) | default |
| `Test_Mutex_Unlock_ReleaseHookSeesCopy` | `Zenith_Mutex_Tests.cpp` | a `Zenith_Mutex` embedded in a heap object that a second thread destroys as soon as it acquires the lock: the release hook of the first thread's `Unlock` reads the desc's name and level without a sanitizer report (`win-asan`, `linux-tsan`), and a hook that keeps the `pLock` address only compares it for equality | default |
| `Test_Mutex_TryLock_ReportsZeroWaitOnSuccessOnly` | `Zenith_Mutex_Tests.cpp` | a successful `TryLock` records one acquire with wait 0; a `TryLock` that fails against a held mutex records nothing | default |
| `Test_Mutex_Contended_NoHooksInstalled_Works` | `Zenith_Mutex_Tests.cpp` | with no hook table installed (`Zenith_IsLockAcquireHookInstalled()` false), 8 threads incrementing a guarded counter 10^5 times each reach the exact total through the contended path | default |
| `Test_MutexNoProfiling_NeverCallsHooks` | `Zenith_Mutex_Tests.cpp` | with `Zenith_CapturingHooks` installed, `Lock`, `TryLock`, `Unlock` and a contended `Lock` of a `Zenith_MutexNoProfiling` leave zero acquire and zero release records, while the same sequence on a `Zenith_Mutex` records them | default |
| `Test_ConditionVariableNoProfiling_NeverCallsHooks` | `Zenith_ConditionVariable_Tests.cpp` | `Wait`, `WaitFor` and `NotifyAll` of a `Zenith_ConditionVariableNoProfiling` on a `Zenith_MutexNoProfiling` leave zero blocking-wait records and zero lock records; the same sequence on the profiled pair records one `Zenith_ConditionVariable` blocking wait per blocking call | default |
| `Test_SemaphoreNoProfiling_NeverCallsHooks` | `Zenith_Semaphore_Tests.cpp` | `Wait` and `WaitFor` of a `Zenith_SemaphoreNoProfiling` leave zero blocking-wait records; a `Zenith_Semaphore` records one per call | default |
| `Test_Semaphore_Name_ReportedAfterWait` | `Zenith_Semaphore_Tests.cpp` | with the capturing hooks installed, a `Zenith_Semaphore` built with a literal name reports that name after `Wait`, one built with the default reports `Zenith_Semaphore`, and a profiled condition-variable wait reports its mutex's desc name | default |
| `Test_BlockingWait_NoProfilingVariants_StillCheckJobWorker` | `Zenith_Thread_DefectTests.cpp` | a `Zenith_SemaphoreNoProfiling` wait between `Zenith_MarkThreadAsJobWorker(true)` and `(false)` fails `ZENITH_CHECK`, with no hook installed | `z2-death-check` (asserts-enabled presets) |
| `Test_LockOrder_NoProfilingMutex_IsChecked` | `Zenith_LockOrder_DefectTests.cpp` | an inversion between two `Zenith_MutexNoProfiling` locks is reported as `FATAL_CODE_LOCK_ORDER`, as for the profiled variant | `z2-death-check` (asserts-enabled presets) |
| `Test_ConditionVariable_NoProfilingOnProfiledMutex_DoesNotCompile` | `Zenith_ConditionVariable_NoProfilingOnProfiledMutex_CompileFail.cpp.in` | a CTest `WILL_FAIL` compile (registered with `zenith_core_compile_fail`, `EXPECT` regex `waits only on a mutex of its own variant`): `Zenith_ConditionVariableNoProfiling::Wait` given a `Zenith_ScopedLock` of a `Zenith_Mutex` | compile-fail |
| `Test_ConditionVariable_ProfiledOnNoProfilingMutex_DoesNotCompile` | `Zenith_ConditionVariable_ProfiledOnNoProfilingMutex_CompileFail.cpp.in` | the same, for `Zenith_ConditionVariable::WaitFor` given a `Zenith_ScopedLock` of a `Zenith_MutexNoProfiling` | compile-fail |
| `Test_HookDispatch_EachDispatchCallsItsHook` | `Zenith_ProfilerHooks_Tests.cpp` | each of the six dispatch functions calls its hook once with its arguments and the table's `m_pUserData` (zone and lock hooks installed as pairs, the partner a no-op); with its member and its partner null, or with no table installed, it calls nothing and leaves `Zenith_GetHookDepth()` and `Zenith_GetHookReentryCount()` at 0 | default |
| `Test_HookDispatch_DepthIsOneInsideHook` | `Zenith_ProfilerHooks_Tests.cpp` | a hook reads `Zenith_GetHookDepth() == 1`; after the dispatch returns it is 0 | default |
| `Test_HookDispatch_OtherThreadUnaffected` | `Zenith_Semaphore_Tests.cpp` | while one thread is inside a hook (parked on a `Zenith_SemaphoreNoProfiling`), another thread's dispatch runs its hook, and neither thread's re-entry count changes | default |
| `Test_HookDispatch_ReentrantAcquireHook_DoesNotRecurse` | `Zenith_Mutex_Tests.cpp` | with asserts disabled: an acquire hook that locks and unlocks another profiled `Zenith_Mutex` runs exactly once per outer lock, not recursively, the outer `Lock` returns and the thread's re-entry count is 2 per outer lock (the inner `Lock` and the inner `Unlock` are each refused); with asserts enabled the case returns at once, and `Test_HookDispatch_Reentry_Asserts` is the proof (the process stops at the first re-entry instead of recursing) | default |
| `Test_HookDispatch_Reentry_CountedPerThread` | `Zenith_ProfilerHooks_Tests.cpp` | with asserts disabled: a zone-begin hook (with a no-op zone-end partner) that calls `Zenith_DispatchCounter` is refused, so the counter hook never runs; the calling thread's `Zenith_GetHookReentryCount()` is 1, the zone-begin hook ran once and the depth is back to 0; with asserts enabled the case returns at once | default |
| `Test_HookDispatch_Reentry_Asserts` | `Zenith_ProfilerHooks_DefectTests.cpp` | the same zone-begin hook dispatching a counter terminates with `ZENITH ASSERT FAILED:` and the message `profiler hook re-entered: a hook used a profiled primitive` | `z2-death-assert` |
| `Test_HookDispatch_LogRecordHook_NotGuarded` | `Zenith_ProfilerHooks_Tests.cpp` | a zone-begin hook (with a no-op zone-end partner) that emits a log record with `ZENITH_LOG` while its dispatch runs: the record reaches the installed log-record hook and the re-entry count stays 0 | default |
| `Test_LogRouter_LogFromInsideHook_DoesNotReenter` | `Zenith_LogRouter_Tests.cpp` | with a real `Zenith_LogRouter` installed and its ring filled until records are being dropped (the drops read through `GetStats`, no counter dispatched from `OnRecord`), a zone-begin hook (with a no-op zone-end partner) that logs at `LOG_LEVEL_WARNING` takes the router's 920 and 930 locks inside its dispatch without a re-entry (count 0, no assert in any preset), and the record is in the ring | default |
| `Test_ProfilerHooks_Install_PairedTables_Accepted` | `Zenith_ProfilerHooks_Tests.cpp` | `Zenith_InstallProfilerHooks` accepts a table with every member null, with both zone hooks, with both lock hooks, and with only the counter, blocking-wait or log members set; `Zenith_IsZoneHookInstalled()` and `Zenith_IsLockAcquireHookInstalled()` read false, true, true and false as the table changes | default |
| `Test_ProfilerHooks_Install_ZoneBeginWithoutEnd_Checks` | `Zenith_ProfilerHooks_DefectTests.cpp` | installing a table with `m_pfnOnZoneBegin` set and `m_pfnOnZoneEnd` null terminates with `ZENITH CHECK FAILED:` | `z2-death-check` (every preset) |
| `Test_ProfilerHooks_Install_ZoneEndWithoutBegin_Checks` | `Zenith_ProfilerHooks_DefectTests.cpp` | the same for `m_pfnOnZoneEnd` set and `m_pfnOnZoneBegin` null | `z2-death-check` (every preset) |
| `Test_ProfilerHooks_Install_LockReleaseWithoutAcquire_Checks` | `Zenith_ProfilerHooks_DefectTests.cpp` | installing a table with `m_pfnOnLockRelease` set and `m_pfnOnLockAcquire` null terminates with `ZENITH CHECK FAILED:` | `z2-death-check` (every preset) |
| `Test_ProfilerHooks_Install_LockAcquireWithoutRelease_Checks` | `Zenith_ProfilerHooks_DefectTests.cpp` | the same for `m_pfnOnLockAcquire` set and `m_pfnOnLockRelease` null | `z2-death-check` (every preset) |
| `Test_ConditionVariable_ProfiledWait_ReportsReleaseThenAcquire` | `Zenith_ConditionVariable_Tests.cpp` | with `Zenith_CapturingHooks` installed, a thread holding a `Zenith_Mutex` calls `WaitFor` on a `Zenith_ConditionVariable` and is notified after 30 ms: the records in sequence order are acquire (wait 0) of the lock, release (before the wait), blocking-wait of at least 20,000,000 ticks, acquire with wait 0, then the final release at unlock | default |
| `Test_HookDispatch_Reentry_WritesOneDiagnosticLine` | `Zenith_ProfilerHooks_DefectTests.cpp` | a zone-begin hook (with a no-op zone-end partner) dispatching a counter three times on one thread writes the line `ZENITH PROFILER: hook re-entered on this thread` exactly once (registered with `PASS_REGEX` on the line and a `FAIL_REGEX` on two lines; in asserting presets the process stops after the first line and the assertion) | `z2-death-check` (every preset) |
| `Test_Log_Router_NestedRecord_DroppedAndCounted` | `Zenith_LogRouter_Tests.cpp` | a sink whose `Write` logs a non-fatal record: the nested record is not delivered, `m_uLogNestedDropCount` becomes 1, `m_bInLogRouter` is false again afterwards; a sink whose `Write` logs a `LOG_LEVEL_FATAL` record and then a non-fatal one: the fatal record still reaches the diagnostic output and the following non-fatal record is still dropped (the flag is saved on entry and restored on exit, so a nested exit does not clear it) | default |
| `Test_Mutex_ApiSurface_PlatformAlias` | `Zenith_Memory_ApiSurface_Tests.cpp` | through the two mutex aliases alone: `Zenith_Mutex` and `Zenith_MutexNoProfiling` are different types, neither copyable nor movable, constructible from `const Zenith_LockDesc&`, with `bIS_PROFILED` true and false and `GetDesc` returning `const Zenith_LockDesc&`; `Zenith_ScopedLock` deduces either mutex type | default, every preset |
| `Test_Integration_Threading_ProfiledLocksUnderCapture` | `Zenith_ThreadingDiagnostics_IntegrationTests.cpp` (`Zenith2/Tests/Core/`) | 8 `Zenith_Thread`s contend on two profiled `Zenith_Mutex`es and one `Zenith_MutexNoProfiling` under `Zenith_CapturingHooks`: profiled acquire and release records balance and equal the lock count, a contended acquire reports a non-zero wait, the unprofiled lock records nothing, and no thread's re-entry count rises | `z2-stress` |
