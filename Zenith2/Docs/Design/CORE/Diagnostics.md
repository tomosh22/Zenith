# CORE design: build information, diagnostics, results, logging, clocks and configuration

This file is part of the CORE design document family (entry: `Zenith2/Docs/Design/CORE.md`). It specifies `Zenith_BuildInfo.h`, the assertion and fatal-error machinery, diagnostic output and backtraces, the implementation of `Zenith_Result` and the error taxonomy, structured logging (levels, categories, the log router, sinks), the clocks, and the layered configuration store that the platform layer's configuration sources write into.

## Scope of this file

| Topic | Fixed elsewhere | Specified here |
|---|---|---|
| assertion macros, their enablement, report format, fatal codes | `Zenith2/Docs/Contracts.md` → "Errors and results" | macro expansions, the failure procedure, termination per platform |
| `Zenith_Result`, `Zenith_Err`, `ZENITH_TRY`, `ZENITH_TRY_ASSIGN` | `Zenith2/Docs/Contracts.md` → "Errors and results" | the implementation, the error-record form, the error taxonomy |
| the configuration defines and their reader | `Zenith2/Docs/Architecture/BuildSystem.md` → "Compile and link flags" ([Z2-N-ARCH-189]) | `Zenith_BuildInfo.h` in full |
| the profiler-hook table | `Zenith2/Docs/Contracts.md` → "No mutable global state" | the log-record hook added to it |
| configuration layers, provenance, `Zenith_Config` | `Zenith2/Docs/Architecture/PlatformLayer.md` → "Configuration layering" | the split: ZenithCore owns the value types, key schema and layered store; ZenithPlatform owns the sources (files through the virtual filesystem, the command line, user-layer persistence) and the `Zenith_Config` facade |
| crash dumps and symbol upload | DEVT and PLAT, D-SEL-CRASH (open, Stage 2) | only how CORE terminates so that the crash handler sees it |
| the fixed-step simulation clock | `Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Simulation clock" (ZenithJobs) | the monotonic clock it is fed from |

## Build information

```cpp
// Zenith2/Engine/ZenithCore/Zenith_BuildInfo.h
enum Zenith_BuildPlatform : uint8
{
	BUILD_PLATFORM_WINDOWS = 1,
	BUILD_PLATFORM_ANDROID,
	BUILD_PLATFORM_LINUX,
};

enum Zenith_BuildConfiguration : uint8
{
	BUILD_CONFIGURATION_DEBUG = 1,
	BUILD_CONFIGURATION_RELEASE,
	BUILD_CONFIGURATION_SHIPPING,
	BUILD_CONFIGURATION_SANITIZE,
	BUILD_CONFIGURATION_HWASAN,
};

/// @contract enum Zenith_SanitizerKind
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 1. One value per sanitizer preset family; a new sanitizer preset adds an enumerator.
/// @endcontract
enum Zenith_SanitizerKind : uint8
{
	SANITIZER_KIND_NONE = 1,      // no sanitizer defines set
	SANITIZER_KIND_ASAN,          // ZENITH_SANITIZER_ADDRESS (win-asan)
	SANITIZER_KIND_TSAN_UBSAN,    // ZENITH_SANITIZER_THREAD (linux-tsan: TSan plus UBSan)
	SANITIZER_KIND_HWASAN,        // ZENITH_SANITIZER_HWADDRESS (android-arm64-hwasan)
};

/// @contract functions Zenith_BuildInfo
/// Ownership: constexpr readers of the configuration defines; no state.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 1. The only readers of ZENITH_CONFIG_*, ZENITH_PLATFORM_* and ZENITH_SANITIZER_*
///   ([Z2-N-ARCH-189]).
/// @endcontract
constexpr Zenith_BuildPlatform Zenith_GetBuildPlatform();
constexpr Zenith_BuildConfiguration Zenith_GetBuildConfiguration();
constexpr bool Zenith_IsDebugBuild();        // ZENITH_DEBUG defined to 1 (Contracts glossary "Debug build")
constexpr bool Zenith_IsShippingBuild();     // Shipping configuration
constexpr bool Zenith_IsSanitizerBuild();    // any ZENITH_SANITIZER_* is 1
constexpr bool Zenith_IsAddressSanitizerBuild();
constexpr bool Zenith_IsHwAddressSanitizerBuild();
constexpr bool Zenith_IsThreadSanitizerBuild();
constexpr bool Zenith_AreAssertsEnabled();   // ZENITH_ASSERTS_ENABLED == 1
constexpr Zenith_SanitizerKind Zenith_GetSanitizerKind();   // from the preset's ZENITH_SANITIZER_* defines
```

<!-- elaboration -->
The functions compare macros with values, never test whether a macro is defined, so no `#if` on a platform macro appears in ZenithCore (rule F11): `zenith_apply_flags` defines every one of `ZENITH_CONFIG_DEBUG`, `ZENITH_CONFIG_RELEASE`, `ZENITH_CONFIG_SHIPPING`, `ZENITH_CONFIG_SANITIZE`, `ZENITH_CONFIG_HWASAN`, `ZENITH_PLATFORM_WINDOWS`, `ZENITH_PLATFORM_ANDROID`, `ZENITH_PLATFORM_LINUX`, `ZENITH_SANITIZER_ADDRESS`, `ZENITH_SANITIZER_HWADDRESS`, `ZENITH_SANITIZER_THREAD`, `ZENITH_DEBUG` and `ZENITH_ASSERTS_ENABLED` in every target and configuration, to `1` or `0`, through generator expressions on `$<CONFIG>` and the configure preset (`Zenith2/Docs/Architecture/BuildSystem.md` → "Compile and link flags", [Z2-N-ARCH-247] and [Z2-N-ARCH-248]). `Zenith_GetBuildPlatform()` is `ZENITH_PLATFORM_WINDOWS ? BUILD_PLATFORM_WINDOWS : ZENITH_PLATFORM_ANDROID ? BUILD_PLATFORM_ANDROID : BUILD_PLATFORM_LINUX`, and a `static_assert` in `Internal/Zenith_BuildInfoChecks.cpp` requires exactly one platform and exactly one configuration macro to be 1 and at most one `ZENITH_SANITIZER_*` macro to be 1. `Zenith_GetSanitizerKind()` returns `SANITIZER_KIND_ASAN`, `SANITIZER_KIND_TSAN_UBSAN` or `SANITIZER_KIND_HWASAN` for the one sanitizer define set to 1, and `SANITIZER_KIND_NONE` when none is; `Zenith_IsSanitizerBuild()` is `Zenith_GetSanitizerKind() != SANITIZER_KIND_NONE`. A test that must run in one sanitizer preset alone (for example COMPRESS's 4 GiB stream test under `win-asan`) selects itself with `if constexpr (Zenith_GetSanitizerKind() != SANITIZER_KIND_ASAN)` and reports itself skipped through doctest's `MESSAGE`, never through a preprocessor test switch ([Z2-N-CONTRACT-415]).

## Assertions and fatal errors

`Zenith2/Engine/ZenithCore/Zenith_Assert.h` declares the four macros of `Zenith2/Docs/Contracts.md` → "Errors and results" (table "Assertions") and `Zenith_FatalCode` (table "Fatal errors"), with enumerators numbered from 1:

```cpp
enum Zenith_FatalCode : uint8
{
	FATAL_CODE_OUT_OF_MEMORY = 1,
	FATAL_CODE_ARENA_EXHAUSTED,
	FATAL_CODE_QUEUE_FULL,
	FATAL_CODE_LOCK_ORDER,
	FATAL_CODE_ASSERTION,
	FATAL_CODE_SCRIPT_WATCHDOG,
	FATAL_CODE_DEVICE_LOST,
	FATAL_CODE_SCRIPT_HANG,
	FATAL_CODE_LOGGING_UNAVAILABLE,
};

enum Zenith_FailureKind : uint8
{
	FAILURE_KIND_ASSERT = 1, FAILURE_KIND_CHECK, FAILURE_KIND_FATAL, FAILURE_KIND_UNREACHABLE,
};

struct Zenith_SourceSite
{
	const char* m_szFile;
	uint32 m_uLine;
	const char* m_szFunction;
};
#define ZENITH_SOURCE_SITE() Zenith_SourceSite{ __FILE__, static_cast<uint32>(__LINE__), __func__ }

[[noreturn]] void Zenith_Fail(Zenith_FailureKind eKind, Zenith_FatalCode eCode, const char* szCondition,
	const Zenith_SourceSite& xSite, Zenith_StringView strMessage);

#if ZENITH_ASSERTS_ENABLED
#define ZENITH_ASSERT(Condition, ...) \
	do { if (!(Condition)) [[unlikely]] { Zenith_FailFormatted(FAILURE_KIND_ASSERT, FATAL_CODE_ASSERTION, #Condition, \
		ZENITH_SOURCE_SITE() __VA_OPT__(, __VA_ARGS__)); } } while (false)
#else
#define ZENITH_ASSERT(Condition, ...) static_cast<void>(sizeof(!(Condition)))
#endif
#define ZENITH_CHECK(Condition, ...) \
	do { if (!(Condition)) [[unlikely]] { Zenith_FailFormatted(FAILURE_KIND_CHECK, FATAL_CODE_ASSERTION, #Condition, \
		ZENITH_SOURCE_SITE() __VA_OPT__(, __VA_ARGS__)); } } while (false)
#define ZENITH_FATAL(Code, ...) \
	Zenith_FailFormatted(FAILURE_KIND_FATAL, Code, #Code, ZENITH_SOURCE_SITE() __VA_OPT__(, __VA_ARGS__))
#define ZENITH_UNREACHABLE() \
	Zenith_Fail(FAILURE_KIND_UNREACHABLE, FATAL_CODE_ASSERTION, "unreachable", ZENITH_SOURCE_SITE(), Zenith_StringView())
```

`FATAL_CODE_LOGGING_UNAVAILABLE` is the row of `Zenith2/Docs/Contracts.md` → "Fatal errors" raised by the composition root when `Zenith_LogRouter::StartDrainThread` fails at startup step 7 ("Log router"): logging is required infrastructure, so a process whose log records cannot reach a sink does not continue.

`Zenith_FailFormatted` is a `[[noreturn]]` variadic template that formats its optional `Zenith_FormatString` and arguments into a 1,024-byte local buffer with `Zenith_FormatTo` and calls `Zenith_Fail`. The `#if ZENITH_ASSERTS_ENABLED` in `Zenith_Assert.h` is the one configuration conditional the contract allows there ([Z2-N-CONTRACT-444]); `ZENITH_ASSERTS_ENABLED` is not a platform or backend macro, so rule F11 does not apply to it.

`Zenith_Fail`, both `Zenith_FailFormatted` overloads and `Zenith_TerminateProcess` are the only `[[noreturn]]` functions in ZenithCore, and each genuinely never returns: `Zenith_Fail` ends in `Zenith_TerminateProcess` on every path, including the recursive-failure path of step 1 ("Failure procedure"); `Zenith_FailFormatted` ends in `Zenith_Fail`; and `Zenith_TerminateProcess` raises a non-continuable exception and then calls `TerminateProcess`, or traps. The attribute is needed, because `ZENITH_FATAL` and `ZENITH_UNREACHABLE` end control paths of functions that return a value, and warnings-as-errors would reject such a path without it. `Zenith_DebugBreak` returns when the debugger continues, so it is not `[[noreturn]]`.

### Failure procedure

`Zenith_Fail` (`Internal/Diagnostics/Zenith_Fail.cpp`) runs these steps on the failing thread:

1. If the thread's Debug thread state (`Zenith2/Docs/Contracts.md` → "No mutable global state", [Z2-N-CONTRACT-144]) has its in-failure flag set, write `ZENITH FATAL FAILED: recursive failure during failure report` through `Zenith_WriteDiagnosticLine` and go to step 7.
2. Set the in-failure flag.
3. Build the first line `ZENITH <KIND> FAILED: <condition> (<file>:<line>) <message>` ([Z2-N-CONTRACT-470]), `<KIND>` one of `ASSERT`, `CHECK`, `FATAL`, `UNREACHABLE`; for `ZENITH_FATAL` the condition text is the fatal code's enumerator name.
4. Write the line and then the backtrace (`Zenith_CaptureBacktrace` with 32 frames, skipping the failure machinery's own frames), one frame per line as `  #<n> <module>+0x<offset> <symbol>` (the symbol from dbghelp on Windows when available, otherwise omitted), through `Zenith_WriteDiagnosticLine`.
5. When a log-record hook is installed, emit a `LOG_LEVEL_FATAL` record in category `core.diagnostics` carrying the first line as its message and the fatal code as field `code`; the log router flushes its output sinks synchronously for a fatal record ("Log router").
6. For `FAILURE_KIND_ASSERT` with a debugger attached (`Zenith_IsDebuggerAttached()`), call `Zenith_DebugBreak()`.
7. Terminate through `Zenith_TerminateProcess(eCode)`.

| Platform | `Zenith_TerminateProcess(eCode)` |
|---|---|
| Windows (`Internal/Windows/Zenith_Terminate_Windows.cpp`) | `RaiseException(0xE05A0000 \| eCode, EXCEPTION_NONCONTINUABLE, 0, nullptr)`, so an installed unhandled-exception filter (the crash handler of D-SEL-CRASH (open, Stage 2)) writes its dump; then `TerminateProcess(GetCurrentProcess(), 0xE05A0000 \| eCode)` as the fallback |
| Android, Linux (`Internal/<Platform>/Zenith_Terminate_<Platform>.cpp`) | `__builtin_trap()`, which raises `SIGTRAP` or `SIGILL`, caught by an installed signal-based crash handler |

- A failure never runs destructors, `atexit` handlers or log-sink shutdown beyond step 5; the process image is left for the crash handler.
- The death-test suites match the first line ([Z2-N-CONTRACT-413]).

### Diagnostic output and backtraces

```cpp
// Zenith2/Engine/ZenithCore/Zenith_Diagnostics.h
/// @contract functions Zenith_Diagnostics
/// Ownership: Stateless platform functions; allocate nothing.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: Frame counts.
/// Errors: None; output failures are ignored (there is nowhere to report them).
/// Version: since Z2 Stage 1.
/// @endcontract
void Zenith_WriteDiagnosticLine(Zenith_StringView strLine);     // appends a newline
void Zenith_WriteTaggedDiagnosticLine(Zenith_StringView strTag, Zenith_StringView strLine);   // appends a newline; strTag ≤ 31 bytes, ASCII
uint32 Zenith_CaptureBacktrace(Zenith_Span<void*> xFrames, uint32 uSkipFrames);
void Zenith_WriteBacktrace(Zenith_Span<void* const> xFrames);
bool Zenith_IsDebuggerAttached();
void Zenith_DebugBreak();
[[noreturn]] void Zenith_TerminateProcess(Zenith_FatalCode eCode);
```

| Function | Windows | Android | Linux |
|---|---|---|---|
| `Zenith_WriteDiagnosticLine` | `OutputDebugStringW` (UTF-16 through `Zenith_Utf8ToUtf16` into a 2,048-unit local buffer, longer lines split) and `WriteFile` to `STD_ERROR_HANDLE` | `__android_log_write(ANDROID_LOG_ERROR, "Zenith", …)` and `write(2, …)` | `write(2, …)` |
| `Zenith_WriteTaggedDiagnosticLine` | `<tag>: <line>` composed into a 2,048-byte local buffer (longer lines split, each piece prefixed) and written as `Zenith_WriteDiagnosticLine` writes | `__android_log_write(ANDROID_LOG_INFO, <tag>, <line>)` with the tag copied into a 32-byte NUL-terminated local buffer and the line into a 2,048-byte one (longer lines split), and `write(2, …)` of `<tag>: <line>` | `write(2, …)` of `<tag>: <line>` |
| `Zenith_CaptureBacktrace` | `RtlCaptureStackBackTrace` | `_Unwind_Backtrace` | `_Unwind_Backtrace` |
| `Zenith_IsDebuggerAttached` | `IsDebuggerPresent` | `TracerPid` of `/proc/self/status` ≠ 0 | same as Android |
| `Zenith_DebugBreak` | `__debugbreak()` | `__builtin_debugtrap()` | `__builtin_debugtrap()` |

These are the functions the heap's out-of-memory path, the leak report and the failure procedure use, because they need no engine object; every other diagnostic goes through the structured log. `Zenith_WriteTaggedDiagnosticLine` exists for output that a reader filters by tag — the test main's tagged route (`Zenith2/Docs/Design/CORE/Memory.md` → "Test main and the Android test entry") is its first caller. A tag longer than 31 bytes, or one holding a byte outside printable ASCII, is `ZENITH_ASSERT` (the tag is always a literal, never data); the composition of tag and line is the internal `Zenith_ComposeTaggedLine(Zenith_StringView strTag, Zenith_StringView strLine, Zenith_Span<char> xOut) → uint32` in `Internal/Diagnostics/Zenith_TaggedLine.cpp`, shared by the three platform files and tested directly.

## Results and the error taxonomy

`Zenith2/Engine/ZenithCore/Zenith_Result.h` implements exactly the declarations of `Zenith2/Docs/Contracts.md` → "Errors and results" (subsection "Zenith_Result"): `Zenith_ErrorType`, `Zenith_GetErrorCode`, `Zenith_ErrorValue`, `Zenith_Err`, `Zenith_Result<T, E>`, `Zenith_Result<void, E>`, `ZENITH_TRY` and `ZENITH_TRY_ASSIGN`. The two error forms are those of [Z2-N-CONTRACT-480] (an error enumeration, or a trivially copyable struct of at most 32 bytes with a public enumeration `m_eCode`), read generically through `Zenith_GetErrorCode` ([Z2-N-CONTRACT-481]).

CORE's two error records satisfy the bound: `Zenith_JsonReadError` is 24 bytes (`m_eCode` at 0, `m_uLine` at 4, `m_uColumn` at 8, `m_ulOffset` at 16) and `Zenith_MemoryError` is 24 bytes (`m_eCode` and `m_ePeak` at 0 and 1, `m_ulRequestedBytes` at 8, `m_ulAvailableBytes` at 16); each carries a `static_assert(sizeof(…) <= 32)` beside its declaration. Every other CORE error type is an enumeration; `Zenith_ConfigError` in particular is an enumeration, because a struct carrying key and source views would be a string-carrying error, which [Z2-N-CONTRACT-480] forbids (the key, source and line travel in `Zenith_ConfigDiagnostic`, "Configuration store").

Implementation notes:

- Storage is exactly `{ T m_xValue; E m_eError; }`, with no union and no `bool`. `Zenith_Result` carries `static_assert(std::is_default_constructible_v<T>)` beside its `static_assert(Zenith_ErrorType<E>)`, because a failure result holds a default-constructed `T`. `Zenith_Result<void, E>` stores `{ E m_eError; }` alone.
- The state is the error code: a result is a success exactly when `Zenith_GetErrorCode(m_eError) == 0`. Error codes are never 0 ([Z2-N-CONTRACT-465], [Z2-N-CONTRACT-481]), so no other state is needed.
  - The success constructors move or copy the value into `m_xValue` and value-initialise `m_eError` (`E{}`), whose code is 0. An error record therefore never gives `m_eCode` a non-zero default member initializer; CORE's `Zenith_JsonReadError` and `Zenith_MemoryError` value-initialise it to 0.
  - The failure constructor default-constructs `m_xValue`, stores the error, and `ZENITH_ASSERT`s `Zenith_GetErrorCode(xError.m_eError) != 0`.
  - `IsOk()` is `Zenith_GetErrorCode(m_eError) == 0`, and `IsError()` is its negation.
- The accessors check the state with `ZENITH_ASSERT`, so the checks exist only where `ZENITH_ASSERTS_ENABLED=1`: `Value()` and `TakeValue()` assert `IsOk()`, and `Error()` asserts `IsError()`. Where asserts are off the accessors are plain member reads. A wrong-state read then returns the default-constructed `T`, or an error whose code is 0. It is still a programmer error, but it is no longer undefined behaviour.
- The layout is identical in every build and configuration. No member exists in some builds and not others ([Z2-N-CONTRACT-441], [Z2-N-CONTRACT-443]), and the debug checks add code only. `sizeof(Zenith_Result<T, E>)` is `sizeof(T)` plus `sizeof(E)` plus alignment padding, which is `sizeof(struct { T; E; })`. `sizeof(Zenith_Result<void, E>)` is `sizeof(E)`.
- The move constructor, move assignment and destructor are member-wise: they move `m_xValue`, copy `m_eError` (trivially copyable) and destroy `m_xValue`, whatever the state. Move assignment therefore exists exactly when `T` is move-assignable. Copy is not declared.
- A moved-from result keeps its state, because `m_eError` is copied rather than reset. After `TakeValue` the result still reports `IsOk()` and holds a moved-from `T`, which the caller does not read again (review confirms). The private members are exactly those of `Zenith2/Docs/Contracts.md` → "Errors and results".
- `Zenith_Result` is a plain class template: no `[[nodiscard]]`, no `final`, no `noexcept`.
- `ZENITH_TRY(Expr)` expands to `do { auto&& xZenithTryResult = (Expr); if (xZenithTryResult.IsError()) { return Zenith_Err(xZenithTryResult.Error()); } } while (false)`. It evaluates `Expr` once and returns the error unchanged; the enclosing function's own failure result then default-constructs its `T`.
- `ZENITH_TRY_ASSIGN(Declaration, Expr)` declares a uniquely named local with `__LINE__` concatenation and returns on error exactly as `ZENITH_TRY` does. On success it assigns `Declaration = std::move(local).TakeValue();`. On the error path the local's default-constructed `T` is destroyed with the local.

### Error taxonomy

<!-- elaboration -->
Every error enumeration is a library's own ([Z2-N-CONTRACT-465]); the base taxonomy that lets logs, counters, telemetry and generic code classify any of them is `Zenith_ErrorCategory`:

```cpp
// Zenith2/Engine/ZenithCore/Zenith_ErrorCategory.h
enum Zenith_ErrorCategory : uint8
{
	ERROR_CATEGORY_INVALID_ARGUMENT = 1,   // the caller passed a value outside the documented domain (recoverable forms only)
	ERROR_CATEGORY_OUT_OF_MEMORY,          // an allocation failed
	ERROR_CATEGORY_CAPACITY_EXCEEDED,      // a bounded capacity (cap, queue, table, budget) is full
	ERROR_CATEGORY_CORRUPT_DATA,           // untrusted input is malformed
	ERROR_CATEGORY_NOT_FOUND,              // a lookup found nothing
	ERROR_CATEGORY_STALE_HANDLE,           // a handle no longer resolves
	ERROR_CATEGORY_IO,                     // the operating system reported an I/O failure
	ERROR_CATEGORY_UNSUPPORTED,            // the platform, device or build lacks the feature
	ERROR_CATEGORY_VERSION_MISMATCH,       // a versioned input is from another version
	ERROR_CATEGORY_CANCELLED,              // the operation was cancelled
	ERROR_CATEGORY_TIMEOUT,                // a bounded wait expired
	ERROR_CATEGORY_PERMISSION,             // the OS or a policy refused access
	ERROR_CATEGORY_UNAVAILABLE,            // a service or resource is temporarily absent
};
constexpr const char* Zenith_ToString(Zenith_ErrorCategory eCategory);
```

- Every error enumeration's header declares, beside its `Zenith_ToString` ([Z2-N-CONTRACT-466]), `constexpr Zenith_ErrorCategory Zenith_GetErrorCategory(Zenith_<Subsystem>Error eError)`, a total mapping (every enumerator returns a category).
- An error record maps through its `m_eCode`.
- The CORE enumerations and their categories:

| Enumeration (header) | Enumerator → category |
|---|---|
| `Zenith_AllocError` (`Zenith_Heap.h`) | `OUT_OF_MEMORY` → OUT_OF_MEMORY; `SIZE_OVERFLOW`, `ALIGNMENT_UNSUPPORTED` → INVALID_ARGUMENT; `CAPACITY_EXCEEDED` → CAPACITY_EXCEEDED |
| `Zenith_NarrowError` (`Zenith_NarrowCast.h`) | all → INVALID_ARGUMENT |
| `Zenith_ThreadError` (`Zenith_Multithreading.h`) | `THREAD_ERROR_CREATE_FAILED` → UNAVAILABLE; `THREAD_ERROR_OUT_OF_MEMORY` → OUT_OF_MEMORY; `THREAD_ERROR_INVALID_DESC` → INVALID_ARGUMENT |
| `Zenith_QueueError` (`Zenith_QueuePolicy.h`) | `FULL` → CAPACITY_EXCEEDED; `OUT_OF_MEMORY` → OUT_OF_MEMORY |
| `Zenith_HandleError` (`Zenith_SlotTable.h`) | `NULL`, `STALE`, `FOREIGN` → STALE_HANDLE; `TABLE_FULL` → CAPACITY_EXCEEDED; `OUT_OF_MEMORY` → OUT_OF_MEMORY |
| `Zenith_StringError` (`Zenith_StringID.h`) | `OUT_OF_MEMORY` → OUT_OF_MEMORY; `INVALID_UTF8`, `EMBEDDED_NUL` → CORRUPT_DATA; `TOO_LONG`, `TABLE_FULL` → CAPACITY_EXCEEDED; `NOT_FOUND` → NOT_FOUND; `INVALID_DESC` → INVALID_ARGUMENT |
| `Zenith_UnicodeError` (`Zenith_Unicode.h`) | `DESTINATION_TOO_SMALL` → CAPACITY_EXCEEDED; the rest → CORRUPT_DATA |
| `Zenith_ParseError` (`Zenith_NumberText.h`) | all → CORRUPT_DATA |
| `Zenith_JsonReadErrorCode` (`Zenith_JsonDocument.h`) | `INPUT_TOO_LARGE`, `DEPTH_EXCEEDED`, `STRING_TOO_LONG`, `ARRAY_TOO_LONG`, `OBJECT_TOO_LARGE` → CAPACITY_EXCEEDED; `OUT_OF_MEMORY` → OUT_OF_MEMORY; the rest → CORRUPT_DATA |
| `Zenith_JsonAccessError` (`Zenith_JsonDocument.h`) | `WRONG_KIND`, `NOT_AN_INTEGER`, `OUT_OF_RANGE` → CORRUPT_DATA; `MISSING_MEMBER`, `INDEX_OUT_OF_RANGE` → NOT_FOUND; `OUT_OF_MEMORY` → OUT_OF_MEMORY; `INVALID_POINTER` (a `Zenith_DocumentTree::FindPath` pointer outside its grammar) → INVALID_ARGUMENT |
| `Zenith_JsonWriteError` (`Zenith_JsonWriter.h`) | `OUT_OF_MEMORY` → OUT_OF_MEMORY; `DEPTH_EXCEEDED` → CAPACITY_EXCEEDED; the rest → INVALID_ARGUMENT |
| `Zenith_ConfigError` (`Zenith_ConfigStore.h`) | "Configuration store" |
| `Zenith_RandomError` (`Zenith_Random.h`) | `ENTROPY_UNAVAILABLE` → UNAVAILABLE |
| `Zenith_PersistentIDError` (`Zenith_PersistentID.h`) | all → CORRUPT_DATA |
| `Zenith_OperationError` (`Zenith_OperationHandle.h`) | `Zenith2/Docs/Design/CORE/Threading.md` → "Zenith_OperationHandle implementation" |
| `Zenith_MemoryErrorCode` (`Zenith_MemoryGovernor.h`) | `MEMORY_ERROR_PEAK_REFUSED` → CAPACITY_EXCEEDED; `MEMORY_ERROR_BAD_BUDGETS` → INVALID_ARGUMENT; `MEMORY_ERROR_OUT_OF_MEMORY` → OUT_OF_MEMORY; `MEMORY_ERROR_SUBSYSTEM_REFUSED` → CAPACITY_EXCEEDED |

## Structured logging

<!-- elaboration -->
Logging primitives are CORE's (Z2-I-CORE-15); log tooling (the file sink, the on-screen console, remote streaming) is DEVT's, and file output goes through PLAT's virtual filesystem. Engine code in any library logs through macros that need no injected object: they reach the log router through a log-record hook in the installed profiler-hook table, so logging adds no global variable (Z2-D-046).

### Levels and categories

```cpp
// Zenith2/Engine/ZenithCore/Zenith_Log.h
enum Zenith_LogLevel : uint8
{
	LOG_LEVEL_TRACE = 1, LOG_LEVEL_DEBUG, LOG_LEVEL_INFO, LOG_LEVEL_WARNING, LOG_LEVEL_ERROR, LOG_LEVEL_FATAL,
	LOG_LEVEL_OFF,        // a filter value only; no record carries it
};

/// @contract value Zenith_LogCategory
/// Ownership: A constexpr constant defined once per category in its owning library's header.
/// Thread affinity: IMMUTABLE. Members const: ZENITH_LOG_CATEGORY aggregate-initialises it and records point at it.
/// Invalidation: None; categories live for the process.
/// Units: None.
/// Errors: A name that is not a lower-case dotted ASCII identifier fails a static_assert in ZENITH_LOG_CATEGORY.
/// Version: since Z2 Stage 1. Names are stable: tooling filters on them.
/// @endcontract
struct Zenith_LogCategory
{
	const char* const m_szName = nullptr;                          // "<library>.<topic>", e.g. "core.memory"
	const Zenith_LogLevel m_eCompiledMinimum = LOG_LEVEL_TRACE;    // records below it are compiled out
};
#define ZENITH_LOG_CATEGORY(Identifier, NameLiteral, CompiledMinimum) \
	inline constexpr Zenith_LogCategory Identifier{ NameLiteral, CompiledMinimum }

constexpr Zenith_LogLevel Zenith_GetCompiledLogFloor();   // Shipping: LOG_LEVEL_INFO; otherwise LOG_LEVEL_TRACE
```

| Level | Use | Shipping |
|---|---|---|
| `LOG_LEVEL_TRACE` | per-frame or per-item detail, off by default at run time | compiled out |
| `LOG_LEVEL_DEBUG` | developer detail | compiled out |
| `LOG_LEVEL_INFO` | lifecycle milestones (startup steps, world creation, loads) | kept |
| `LOG_LEVEL_WARNING` | a recoverable anomaly that changes behaviour (a rejected configuration value, an unknown field, a pending budget row) | kept |
| `LOG_LEVEL_ERROR` | a recoverable failure at its point of failure, carrying the context of [Z2-N-CONTRACT-464] | kept |
| `LOG_LEVEL_FATAL` | emitted only by the failure procedure | kept |

CORE's categories, in `Zenith_Log.h`: `xLOG_CATEGORY_CORE_MEMORY` (`core.memory`), `xLOG_CATEGORY_CORE_THREADING` (`core.threading`), `xLOG_CATEGORY_CORE_CONFIG` (`core.config`), `xLOG_CATEGORY_CORE_JSON` (`core.json`), `xLOG_CATEGORY_CORE_GOVERNOR` (`core.governor`), `xLOG_CATEGORY_CORE_STRINGS` (`core.strings`), `xLOG_CATEGORY_CORE_DIAGNOSTICS` (`core.diagnostics`); each with compiled minimum `LOG_LEVEL_TRACE`. Every other library declares its own categories named `<library short name>.<topic>` (for example `jobs.scheduler`, `rhi.vulkan`).

### Records and fields

```cpp
enum Zenith_LogFieldKind : uint8
{
	LOG_FIELD_KIND_BOOL = 1, LOG_FIELD_KIND_INT, LOG_FIELD_KIND_UINT, LOG_FIELD_KIND_DOUBLE, LOG_FIELD_KIND_TEXT,
};

struct Zenith_LogField
{
	const char* m_szKey;              // a string literal: lower camel case ASCII
	Zenith_LogFieldKind m_eKind;
	bool m_bBool;
	sint64 m_iInt;
	uint64 m_ulUInt;
	double m_dDouble;
	Zenith_StringView m_strText;
};

/// @contract value Zenith_LogRecord
/// Ownership: A view built on the logging thread's stack; every pointer and view lives for the duration of
///   one sink call. Sinks copy what they keep.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: Invalid after the sink call returns.
/// Units: m_ulMonotonicUs microseconds of Zenith_GetMonotonicTimeUs; m_ulUtcUs microseconds since the Unix epoch.
/// Errors: None.
/// Version: since Z2 Stage 1. The field set is the structured-log schema consumed by DEVT tooling.
/// @endcontract
struct Zenith_LogRecord
{
	Zenith_LogLevel m_eLevel;
	const Zenith_LogCategory* m_pxCategory;
	Zenith_SourceSite m_xSite;
	Zenith_StringView m_strMessage;          // formatted, valid UTF-8, at most 2,047 bytes
	Zenith_Span<const Zenith_LogField> m_axFields;
	uint64 m_ulMonotonicUs;
	uint64 m_ulUtcUs;
	uint64 m_ulThreadID;                    // Zenith_GetCurrentThreadID()
	uint64 m_ulSequence;                    // assigned by the router; strictly increasing per process
	uint64 m_ulFrameIndex;                  // the router's current frame index
	bool m_bTruncated;                       // the message did not fit
};
```

### Log macros

```cpp
#define ZENITH_LOG_TRACE(Category, ...)   ZENITH_LOG_AT(Category, LOG_LEVEL_TRACE, __VA_ARGS__)
#define ZENITH_LOG_DEBUG(Category, ...)   ZENITH_LOG_AT(Category, LOG_LEVEL_DEBUG, __VA_ARGS__)
#define ZENITH_LOG_INFO(Category, ...)    ZENITH_LOG_AT(Category, LOG_LEVEL_INFO, __VA_ARGS__)
#define ZENITH_LOG_WARNING(Category, ...) ZENITH_LOG_AT(Category, LOG_LEVEL_WARNING, __VA_ARGS__)
#define ZENITH_LOG_ERROR(Category, ...)   ZENITH_LOG_AT(Category, LOG_LEVEL_ERROR, __VA_ARGS__)
#define ZENITH_LOG_AT(Category, Level, ...) \
	do { if constexpr ((Level) >= Zenith_GetCompiledLogFloor() && (Level) >= (Category).m_eCompiledMinimum) { \
		Zenith_EmitLog((Category), (Level), ZENITH_SOURCE_SITE(), Zenith_Span<const Zenith_LogField>(), __VA_ARGS__); } } while (false)
#define ZENITH_LOG_FIELDS(Category, Level, FieldsSpan, ...) \
	do { if constexpr ((Level) >= Zenith_GetCompiledLogFloor() && (Level) >= (Category).m_eCompiledMinimum) { \
		Zenith_EmitLog((Category), (Level), ZENITH_SOURCE_SITE(), (FieldsSpan), __VA_ARGS__); } } while (false)

template<typename... TArgs>
void Zenith_EmitLog(const Zenith_LogCategory& xCategory, Zenith_LogLevel eLevel, const Zenith_SourceSite& xSite,
	Zenith_Span<const Zenith_LogField> axFields, Zenith_FormatString<std::type_identity_t<TArgs>...> xFormat, const TArgs&... xArgs);
```

`Zenith_EmitLog` runs these steps: (1) read `Zenith_GetProfilerHooks()`; if `m_pfnOnLogRecord` is null and the level is at least `LOG_LEVEL_WARNING`, format `[<LEVEL>] <category>: <message>` into a 2,048-byte local buffer and write it with `Zenith_WriteDiagnosticLine`, then return; if it is null and the level is lower, return; (2) otherwise ask the router's filter through `m_pfnIsLogLevelEnabled` (below), and return when filtered; (3) format the message into a 2,048-byte local buffer (setting `m_bTruncated` when the result did not fit); (4) fill `Zenith_LogRecord` (sequence and frame index 0; the router assigns them); (5) call the hook.

### The log-record hook

`Zenith_ProfilerHooks` carries the log-record hook after `m_pUserData` — `Zenith_PfnOnLogRecord m_pfnOnLogRecord`, `Zenith_PfnIsLogLevelEnabled m_pfnIsLogLevelEnabled` and `void* m_pLogUserData` (the `Zenith_LogRouter`) — as declared in `Zenith2/Docs/Contracts.md` → "The two installed hooks" ([Z2-N-CONTRACT-157]); CORE declares no member of its own there.

The table is still installed once by `main` and copied into the single allowlisted variable `g_xProfilerHooks` ([Z2-N-CONTRACT-143], [Z2-N-CONTRACT-152]); no variable is added. `Zenith_LogRouter::FillHooks(Zenith_ProfilerHooks&)` writes the three members.

The log-record members are not invoked through the `Zenith_Dispatch…` functions of the other six hooks ([Z2-N-CONTRACT-590]), and `Zenith_EmitLog` and the failure procedure keep calling them directly (Z2-D-142). A dispatch made inside a hook is refused, and the fatal record of failure step 5 is emitted from inside whatever hook asserted, including the re-entry assertion of the dispatch guard itself, so guarding the log hooks would lose exactly the record that explains the failure. The router is safe against recursion by type instead ([Z2-N-CONTRACT-591]): its sink-list lock (920) and each `Zenith_LogRingSink` ring lock (930) are `Zenith_MutexNoProfiling`, and its drain semaphore is a `Zenith_SemaphoreNoProfiling`, so a hook that logs takes no profiled primitive. A sink that does take one inside `Write` is reported by the dispatch guard in a build with asserts. The router's locks are unprofiled by necessity (a hook or the failure procedure can log), so contention on them does not appear as a `lock:` site; the router counts it itself as `m_ulContended` in `GetStats`, and the ring sink as `GetContended()`. `Zenith_LogRouter.cpp`, `Zenith_LogSinks.cpp`, `Zenith_Log.cpp` and `Zenith_Fail.cpp` are in the `profilerReachable` list of rule F26 (`Zenith2/Docs/Design/CORE/Threading.md` → "Lock profiling and hook dispatch").

<!-- informative -->
Amendment note (2026-09-25): routing log records through the installed profiler-hook table was accepted in the CORE round-1 review as an extension of the Z2-D-046 exemption (the hook table carries the log-record hooks; the log router is reached through it; no writable symbol is added, so the F16 allowlist gains no entry for it). The human ruled it on 2026-10-16 (ruling Q11 (a)), together with the Debug thread state's OOM and failure flags, as a dated extension of Z2-D-046; `Zenith2/Docs/Contracts.md` → "Writable-data allowlist entries" ([Z2-N-CONTRACT-152]) records it.
<!-- /informative -->

The router exists before the hook table is installed, because the table must hold its pointer, so `main` performs step 1b of `Zenith2/Docs/Architecture/CompositionRoot.md` → "Startup sequence", between steps 1 and 2: after `Zenith_InstallRootHeap(&xHeap)`, `Zenith_LogRouter::Create` with a desc whose ring size is the build-time constant `uLOG_RING_BYTES_DEFAULT` (1 MiB) and whose initial global level is the build's `log.level` default (no configuration exists before step 6, so nothing at step 1b reads a configuration key), then `FillHooks` on the table ZenithProfiling filled, then `Zenith_InstallProfilerHooks`. `Create` makes allocations only (the object, its ring and its override table): no thread, no lock taken, no hook called. When it fails, `main` writes `Zenith2: log router allocation failed` with `Zenith_WriteDiagnosticLine`, installs no hook, uninstalls the root heap and returns exit code 4 (`Zenith2/Docs/Design/MOD.md` → "Player headers"). No drain thread runs before step 7, and every record emitted from step 1b onward waits in the ring (dropping the oldest beyond the ring's capacity, counted in `log.router.dropped`).

At step 7 of the same table the composition root, which receives the router by reference from `main`, performs in order: (1) `SetGlobalLevel` from `log.level` and `SetCategoryLevel` for each `log.categoryLevels` entry; (2) `AddSink` for every sink; (3) `ResizeRing` with the resolved `log.ringBytes` when it differs from `uLOG_RING_BYTES_DEFAULT`, which first outputs every buffered record to the sinks just registered (a failure keeps the bootstrap ring and its records, is logged as a warning, and startup continues); (4) `ZENITH_ASSERT(xDesc.m_bStartDrainThread)` on the desc `main` used, then `StartDrainThread()`, so a desc that disables the thread cannot turn the fatal-on-failure rule below into a silent success. Every record emitted since step 1b therefore reaches the sinks registered at step 7, in sequence order.

`StartDrainThread()` failure is fatal: the composition root writes `ZENITH STARTUP: log drain thread did not start: <Zenith_ThreadError enumerator name>` with `Zenith_WriteDiagnosticLine` (standard error, and logcat on Android) and raises `ZENITH_FATAL(FATAL_CODE_LOGGING_UNAVAILABLE)`; the fatal record's synchronous path (`OnRecord` step 4) outputs the buffered records and then the fatal record to the registered sinks before the process terminates.

The order is Architecture's: [Z2-N-ARCH-846] ("before any engine object exists other than the log router of step 1b") and [Z2-N-CONTRACT-143] ("directly after the root heap and the log router's creation"); [Z2-N-ARCH-866] is unaffected, because the router is an object owned by `main`, not process-wide state. This section adds the router-side detail of each step.

### Log router

```cpp
// Zenith2/Engine/ZenithCore/Zenith_LogRouter.h
/// @contract interface Zenith_LogSink
/// Ownership: Implemented by sinks (CORE's ring and console sinks; DEVT's file sink and on-screen console);
///   registered with the router by reference and unregistered before destruction.
/// Thread affinity: ANY_THREAD. Write is called on logging threads under uLOCK_LEVEL_LOG_SINKS; Output and Flush
///   are called on the router's drain thread, or on the failing thread on the fatal path (OnRecord step 4).
/// Invalidation: None.
/// Units: None.
/// Errors: Neither member can fail; a sink that cannot keep a record counts it as dropped.
/// Reentrancy: NONE. A sink never logs from Write or Output.
/// Version: since Z2 Stage 1. Source API.
/// @endcontract
class Zenith_LogSink
{
public:
	virtual ~Zenith_LogSink() = default;
	virtual Zenith_LogLevel GetMinimumLevel() const = 0;
	virtual void Write(const Zenith_LogRecord& xRecord) = 0;    // memory work only: no I/O, no wait, no callback
	virtual void Output(const Zenith_LogRecord& xRecord) = 0;   // I/O allowed; drain thread or the fatal path
	virtual void Flush() = 0;                                   // I/O allowed; drain thread or the fatal path
};

constexpr uint32 uLOG_RING_BYTES_DEFAULT = 1u << 20;         // 1 MiB: the ring from startup step 1b until ResizeRing at step 7
constexpr uint32 uLOG_RECORD_MAX_SERIALISED_BYTES = 4096;    // one serialised record at most; the drain thread's copy buffer
constexpr uint32 uLOG_RING_BYTES_MIN = uLOG_RECORD_MAX_SERIALISED_BYTES;   // a ring holds at least one record
constexpr uint32 uFAULT_POINT_LOG_ROUTER_DRAIN_THREAD = 0x434F0002;   // StartDrainThread, before the thread is created

struct Zenith_LogRouterDesc
{
	uint64 m_ulRingBytes = uLOG_RING_BYTES_DEFAULT;          // at least uLOG_RING_BYTES_MIN (ZENITH_ASSERT)
	uint32 m_uMaxCategoryOverrides = 64;                     // capacity of the category override table, at least 1
	Zenith_LogLevel m_eInitialGlobalLevel = LOG_LEVEL_INFO;   // main passes the build's log.level default (LOG_LEVEL_WARNING in Shipping)
	bool m_bStartDrainThread = true;    // false only in tests that drain synchronously with DrainForTest; main never sets it false
	Zenith_FaultInjector* m_pxFaultInjector = nullptr;        // tests
};

struct Zenith_LogRouterStats
{
	uint64 m_ulRecordsRouted = 0;      // records that passed the filter and were written to the ring (OnRecord step 3)
	uint64 m_ulRecordsFiltered = 0;    // records rejected by the filter (m_pfnIsLogLevelEnabled or OnRecord step 1)
	uint64 m_ulRingBytes = 0;          // the ring's current capacity
	Zenith_QueueStats m_xRing;          // the ring's full events (always 0), dropped records and high-water occupancy in bytes
	uint64 m_ulContended = 0;           // failed try-locks on uLOCK_LEVEL_LOG_SINKS (OnRecord step 3)
};

/// @contract type Zenith_LogRouter
/// Ownership: Created by Create as a Zenith_Owned<Zenith_LogRouter> in main after the root heap (startup step 1b;
///   Create starts no thread); the composition root registers the sinks, resizes the ring and starts the drain
///   thread at startup step 7; destroyed by main after ZenithRuntime and before the profiler hooks are uninstalled.
/// Thread affinity: ANY_THREAD for OnRecord, SetGlobalLevel, SetCategoryLevel and GetStats; MAIN_THREAD for
///   AddSink, RemoveSink, ResizeRing, StartDrainThread, SetFrameIndex, DrainForTest and destruction.
/// Invalidation: None.
/// Units: Ring capacity in bytes.
/// Errors: Create returns Zenith_Result<…, Zenith_AllocError>; AddSink beyond 16 sinks returns
///   ALLOC_ERROR_CAPACITY_EXCEEDED; SetCategoryLevel beyond m_uMaxCategoryOverrides returns the same; ResizeRing
///   returns ALLOC_ERROR_OUT_OF_MEMORY and keeps the current ring with every buffered record; StartDrainThread
///   returns THREAD_ERROR_CREATE_FAILED, which the composition root treats as FATAL_CODE_LOGGING_UNAVAILABLE.
/// Reentrancy: NONE. Sinks must not log.
/// Version: since Z2 Stage 1. Source API.
/// Locks: uLOCK_LEVEL_LOG_SINKS (920, short-hold) around Write fan-out and ring appends; a Zenith_MutexNoProfiling,
///   because a profiler hook or the failure procedure can log from inside a hook dispatch.
/// @endcontract
class Zenith_LogRouter
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_LogRouter>, Zenith_AllocError> Create(const Zenith_LogRouterDesc& xDesc);
	~Zenith_LogRouter();                      // stops and joins the drain thread, draining everything first
	void FillHooks(Zenith_ProfilerHooks& xHooks);
	Zenith_Result<void, Zenith_AllocError> AddSink(Zenith_LogSink& xSink);
	Zenith_Result<void, Zenith_AllocError> ResizeRing(uint64 ulRingBytes);   // before StartDrainThread; outputs the buffered records first
	Zenith_Result<void, Zenith_ThreadError> StartDrainThread();   // startup step 7, after AddSink; returns success and starts nothing when m_bStartDrainThread is false
	void RemoveSink(Zenith_LogSink& xSink);   // waits until the drain thread is not inside xSink
	void SetGlobalLevel(Zenith_LogLevel eLevel);
	Zenith_Result<void, Zenith_AllocError> SetCategoryLevel(Zenith_StringView strCategory, Zenith_LogLevel eLevel);
	void SetFrameIndex(uint64 ulFrameIndex);
	void OnRecord(const Zenith_LogRecord& xRecord);
	void DrainForTest();                      // m_bStartDrainThread == false only
	Zenith_LogRouterStats GetStats() const;
};
```

`OnRecord` algorithm. Before step 1 it reads the calling thread's `m_bInLogRouter` (Debug thread state): when it is set and the record is below `LOG_LEVEL_FATAL`, the record was logged from inside `OnRecord` (a sink's `Write` or `Output`, which must not log) and is dropped, counted in the thread's `m_uLogNestedDropCount`, so it never takes the sink lock a second time; a nested `LOG_LEVEL_FATAL` record continues through step 4's try-lock path. Otherwise the flag is set for the rest of the call. Every call saves the flag's previous value on entry and restores it on every exit, so the exit of a nested fatal record does not clear the flag its caller set. A hook never logs because a profiler resource ran out, since a hook may run under a lock above 920 and `OnRecord` takes 920 with the order-checked `Lock`; the profiler defers such a warning to its `EndFrame`.

1. Filter: the record passes when its level is at least the category override (looked up by name in the fixed override table, a linear probe over at most `m_uMaxCategoryOverrides` hashed entries whose level is an `std::atomic<uint8>`) or, absent an override, the global level (an `std::atomic<uint8>` initialised from `m_eInitialGlobalLevel`, changed by `SetGlobalLevel`). A rejected record increments `m_ulRecordsFiltered`. A record admitted under the initial level and emitted before step 7 is delivered even when the level applied at step 7 would reject it: filtering happens at emission.
2. Assign `m_ulSequence` (`fetch_add` on an atomic counter) and `m_ulFrameIndex` (an atomic the main thread sets each frame).
3. Take `uLOCK_LEVEL_LOG_SINKS` (a `TryLock` first; a failure increments `m_ulContended` before the blocking `Lock`); call `Write` on every sink whose minimum level admits the record; serialise the record (header, message bytes, fields; a record whose serialised form would exceed `uLOG_RECORD_MAX_SERIALISED_BYTES` drops its trailing fields until it fits and is marked truncated) into the router's byte ring with policy `QUEUE_FULL_POLICY_DROP_OLDEST`: each record is framed by a leading `uint32` byte length, and when a record does not fit, whole records are dropped from the head until it does, each counted once in `log.router.dropped` (the ring is the router's own structure in `Internal/Diagnostics/Zenith_LogRouter.cpp`, not a `Zenith_RingBuffer<T>`, and publishes `Zenith_QueueStats`), the ring's counters (`log.router.full`, always 0 under DROP_OLDEST, `log.router.dropped` and `log.router.high_water`, the names of [Z2-N-CONTRACT-345]) are kept in `Zenith_QueueStats` and read through `GetStats`; `OnRecord` itself dispatches no counter, so a record logged from inside a hook never reaches a counter hook; release the lock; post the drain semaphore.
4. For `LOG_LEVEL_FATAL`: step 3 takes `uLOCK_LEVEL_LOG_SINKS` with `TryLock`; when it fails (the failing thread already holds a log lock, for example an assert inside a sink's `Write`), the ring append and every sink call are skipped and the record reaches the diagnostic output alone, through `Zenith_Fail` steps 3 and 4. Otherwise, after step 3, when no drain thread is running (before step 7, or after a failed `StartDrainThread`), first copy every buffered record out of the ring and call `Output` on every sink, oldest first, so the records that precede the failure reach the sinks ahead of it; then call `Output` with the fatal record and `Flush` on every sink, directly on the failing thread and outside the lock (a running drain thread may be inside a sink; sinks tolerate that concurrent fatal call on a best-effort basis).

`ResizeRing(ulRingBytes)` (`ZENITH_ASSERT` that no drain thread is running and that `ulRingBytes` is at least `uLOG_RING_BYTES_MIN`; a size equal to the current one returns success and changes nothing): (1) allocate the new ring; on failure return `ALLOC_ERROR_OUT_OF_MEMORY` with the current ring and every buffered record unchanged; (2) repeatedly take the lock, copy the oldest record out, release the lock and call `Output` on each sink; records appended during the loop are drained by it, and step 3 runs once a pass finds the ring empty; (3) with the lock held and the ring empty, install the new ring (its counters continue from the old ring's) and release the lock; (4) `Flush` every sink and free the old ring. A record emitted by another thread during step 2 lands in the old ring and is output by the same loop; one emitted after step 3 lands in the new ring. The router object itself is never re-created after step 1b, because the installed hook table holds its address.

`StartDrainThread()` (once): (1) the fault point `uFAULT_POINT_LOG_ROUTER_DRAIN_THREAD`; (2) `Zenith_Thread::Create` of `Zenith.LogDrain`, passing the router's `m_pxFaultInjector` in the thread desc; on failure it returns the `Zenith_ThreadError` and creates nothing, and the buffered records stay in the ring. The thread's first iteration outputs every record buffered since step 1b, oldest first.

The drain thread (named `Zenith.LogDrain`, a `Zenith_Thread` owned by the router, not a job worker) never dequeues while no sink is registered: it waits on the drain semaphore (a `Zenith_SemaphoreNoProfiling`) again, and `AddSink` posts it. Otherwise it waits on the drain semaphore, then repeatedly takes the lock, copies the oldest serialised record out of the ring into its local 4 KiB buffer, releases the lock and calls `Output` on each sink; every 100 ms or when the ring empties it calls `Flush`. `~Zenith_LogRouter` stores `m_bStopping` (`std::atomic<bool>`, release), posts the drain semaphore and joins the thread; the thread's loop, reading `m_bStopping` with acquire after each wake, exits after one final drain pass that outputs every remaining record and flushes. With no sink registered the remaining records are discarded and each is counted in `log.router.dropped`. A router whose drain thread never started performs the same final pass on the destroying thread.

CORE's sinks:

| Sink | Header | Behaviour |
|---|---|---|
| `Zenith_LogConsoleSink` | `Zenith_LogSinks.h` | `Write` does nothing; `Output` formats one line `<UTC ISO-8601> <LEVEL> <category> <message>` followed by ` <key>=<value>` per field, where `<LEVEL>` is `TRACE`, `DEBUG`, `INFO`, `WARNING`, `ERROR` or `FATAL`, integers are decimal, doubles their shortest round-trip text, booleans `true`/`false`, and text values are enclosed in `"` with `"` and `\` escaped by a backslash, and writes it with `Zenith_WriteDiagnosticLine` |
| `Zenith_LogRingSink` | `Zenith_LogSinks.h` | `Write` appends the formatted line to its own fixed byte ring (`QUEUE_FULL_POLICY_DROP_OLDEST`); `Zenith_Result<uint32, Zenith_AllocError> CopyRecentLines(Zenith_Array<char>& acOut, uint32 uMaxLines) const` appends the most recent at most `uMaxLines` lines, oldest first, each ending in LF, under the sink lock (a `Zenith_MutexNoProfiling` at level 930), and returns the number copied; `uint64 GetContended() const` returns its failed try-locks on its own lock (level 930), counted the same way; used by the on-screen console, crash annotations and tests |

### Shutdown row

The router's drain thread is joined by the router, which `main` destroys after destroying ZenithRuntime and before uninstalling the profiler hooks and the root heap: row 11 of `Zenith2/Docs/Contracts.md` → "Shutdown ownership order" ("destroys ZenithRuntime, then the log router (which drains its ring and joins its drain thread), then uninstalls the profiler hooks").

## Clocks

```cpp
// Zenith2/Engine/ZenithCore/Zenith_Clock.h
/// @contract functions Zenith_Clock
/// Ownership: Stateless readers of the OS clocks.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: Microseconds (Us) and nanoseconds (Ns); UTC microseconds since 1970-01-01T00:00:00Z.
/// Errors: None; the monotonic clock never fails on the supported platforms.
/// Version: since Z2 Stage 1. Simulation code never reads either clock ([Z2-N-ARCH-885]); the frame loop
///   converts monotonic deltas into ticks through Zenith_SimulationClock.
/// @endcontract
uint64 Zenith_GetMonotonicTimeUs();
uint64 Zenith_GetMonotonicTimeNs();
uint64 Zenith_GetMonotonicResolutionNs();   // the clock's tick length, rounded up to 1 ns

struct Zenith_UtcTime
{
	sint64 m_iUnixMicroseconds = 0;
};
Zenith_UtcTime Zenith_GetUtcTime();
uint32 Zenith_FormatUtcIso8601(Zenith_UtcTime xTime, Zenith_Span<char> xOut32);       // "2026-09-25T14:03:07.123456Z"
uint32 Zenith_FormatUtcTimestampCompact(Zenith_UtcTime xTime, Zenith_Span<char> xOut20);   // "20260925T140307Z" (backup directories)

class Zenith_Stopwatch
{
public:
	void Start();                         // records Zenith_GetMonotonicTimeUs()
	uint64 GetElapsedUs() const;
	uint64 Restart();      // elapsed, then Start
private:
	uint64 m_ulStartUs = 0;
};
```

| Function | Windows | Android and Linux |
|---|---|---|
| monotonic | `QueryPerformanceCounter`, with the frequency read once per call through `QueryPerformanceFrequency` (a constant the OS caches); microseconds as `(c / f) × 10^6 + ((c mod f) × 10^6) / f` so no intermediate overflows | `clock_gettime(CLOCK_MONOTONIC)` |
| UTC | `GetSystemTimePreciseAsFileTime`, converted from 100 ns ticks since 1601 | `clock_gettime(CLOCK_REALTIME)` |

- `Zenith_GetMonotonicTimeNs()` is also the clock of the lock profiler: the wait a profiled `Lock` reports as `ulWaitTicks` is a difference of two of its readings, so one tick is one nanosecond ([Z2-N-CONTRACT-589]). The function takes no lock and calls no hook, which is why a hook dispatch may call it.
- The monotonic clock does not advance while an Android device is suspended; the simulation clock's per-frame clamp (`Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Simulation clock") bounds the first frame after resume.
- The compact UTC timestamp is the `<UTC timestamp>` of authoring-document backup directories ([Z2-N-CONTRACT-541]).
- The clock functions are declared once, in ZenithCore's `Zenith_Clock.h`: ZenithProfiling (layer 1) and ZenithJobs (layer 2) read the monotonic clock and depend on no ZenithPlatform header, and a function has one declaration, so ZenithPlatform declares no clock function of its own and its code calls ZenithCore's (the reconciliation of `Zenith2/Docs/Architecture/PlatformLayer.md` → "Settled rules and ownership" is recorded in `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md`).

## Configuration store

<!-- elaboration -->
The split between CORE and PLAT: ZenithCore owns everything a library below ZenithPlatform needs in order to declare and read configuration (value types, key descriptors, the key schema, the layered store with provenance, the JSON layer parser, the dump); ZenithPlatform owns every source (reading `content:/Config/…` files through the virtual filesystem, the command line's `--set` values, persisting the user layer to `user:/config/user.config.json`) and the `Zenith_Config` facade of `Zenith2/Docs/Architecture/PlatformLayer.md` → "Configuration layering", which owns one `Zenith_ConfigStore` and forwards `Get`, `GetProvenance`, `SetUserValue` and `WriteDump` to it. Libraries at layers 0 to 2 (for example ZenithJobs's `jobs.workerCount`) register keys through the CORE schema without depending on ZenithPlatform.

<!-- assumption: pending PLAT -->
The configuration types are declared once, in `ZenithCore/Zenith_ConfigStore.h` below. `Zenith2/Docs/Architecture.md` → "Purpose and scope" (table "Assumed types") names this document as the declarer of `Zenith_ConfigValue`, `Zenith_ConfigKeyDesc`, `Zenith_ConfigProvenance`, `Zenith_ConfigLayer`, `Zenith_ConfigKey`, `Zenith_ConfigError`, `Zenith_ConfigSchema` and `Zenith_ConfigStore`, and `Zenith2/Docs/Architecture/PlatformLayer.md` → "Configuration layering" includes `ZenithCore/Zenith_ConfigStore.h` and declares the `Zenith_Config` facade over it; both are aligned with this declaration. `Zenith2/Docs/Design/PLAT/Configuration.md` → "Ownership split with CORE" consumes the store unchanged together with `Zenith_ConfigLayerEntry`, `GetLayerEntry`, `ClearLayer` and `ApplySimulationAffecting`, which this document now declares; that document's assumption on CORE is the part still pending.
<!-- /assumption -->

<!-- assumption: pending PLAT -->
```cpp
// Zenith2/Engine/ZenithCore/Zenith_ConfigStore.h
enum Zenith_ConfigLayer : uint8
{
	CONFIG_LAYER_ENGINE_DEFAULT, CONFIG_LAYER_PLATFORM, CONFIG_LAYER_GAME,
	CONFIG_LAYER_QUALITY_TIER, CONFIG_LAYER_USER, CONFIG_LAYER_COMMAND_LINE,
	CONFIG_LAYER_COUNT,
};

enum Zenith_ConfigValueType : uint8
{
	CONFIG_VALUE_TYPE_BOOL, CONFIG_VALUE_TYPE_INT, CONFIG_VALUE_TYPE_FLOAT, CONFIG_VALUE_TYPE_STRING, CONFIG_VALUE_TYPE_ENUM,
};

/// @contract value Zenith_ConfigValue
/// Ownership: Plain value; m_strValue views text owned by the Zenith_ConfigStore (or a string literal in a desc).
/// Thread affinity: IMMUTABLE. Members non-const: the store's slots and layer entries reassign it.
/// Invalidation: m_strValue is invalid after the store that owns it is destroyed.
/// Units: The key's; numeric keys document units in their names (Zenith2/Docs/Design/CORE/Maths.md "Units").
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_ConfigValue
{
	Zenith_ConfigValueType m_eType = CONFIG_VALUE_TYPE_BOOL;
	bool m_bValue = false;
	sint64 m_iValue = 0;
	float m_fValue = 0.0f;             // FLOAT keys
	Zenith_StringView m_strValue;      // STRING text, and the enumerator name of ENUM
	uint32 m_uEnumIndex = 0;          // CONFIG_VALUE_TYPE_ENUM
	static constexpr Zenith_ConfigValue MakeBool(bool bValue);
	static constexpr Zenith_ConfigValue MakeInt(sint64 iValue);
	static constexpr Zenith_ConfigValue MakeFloat(float fValue);
	static constexpr Zenith_ConfigValue MakeString(Zenith_StringView strValue);
	static constexpr Zenith_ConfigValue MakeEnum(uint32 uIndex);
};

struct Zenith_ConfigKeyDesc
{
	const char* m_szKey;                    // dotted, "<area>.<name>", lower camel case per segment, at most 128 bytes
	Zenith_ConfigValueType m_eType;
	Zenith_ConfigValue m_xDefault;
	Zenith_ConfigValue m_xMin, m_xMax;      // numeric keys; equal Min and Max mean "unbounded" when both are zero
	uint32 m_uAllowedLayerMask;            // bit per Zenith_ConfigLayer; CONFIG_LAYER_ENGINE_DEFAULT is always allowed
	bool m_bRequiresRestart;
	bool m_bSimulationAffecting;            // added: written into every replay header ([Z2-N-ARCH-930])
	bool m_bRequired;                       // added: no default; a layer must supply it before ValidateRequired
	Zenith_Span<const char* const> m_axEnumValues;   // added: CONFIG_VALUE_TYPE_ENUM names, in index order
	const char* m_szDescription;            // added: one sentence, shown by the dump and the editor
};

struct Zenith_ConfigProvenance
{
	Zenith_ConfigLayer m_eLayer;
	Zenith_StringView m_strSource;          // file path or "command line"; "default" for the engine-default layer
	uint32 m_uLine;                        // 0 for defaults and the command line
};

enum Zenith_ConfigError : uint8
{
	CONFIG_ERROR_UNKNOWN_KEY = 1,
	CONFIG_ERROR_INVALID_KEY_NAME,
	CONFIG_ERROR_DUPLICATE_KEY,
	CONFIG_ERROR_SCHEMA_FROZEN,
	CONFIG_ERROR_WRONG_TYPE,
	CONFIG_ERROR_OUT_OF_RANGE,
	CONFIG_ERROR_INVALID_ENUM_VALUE,
	CONFIG_ERROR_LAYER_NOT_ALLOWED,
	CONFIG_ERROR_PARSE,
	CONFIG_ERROR_NESTED_OBJECT,
	CONFIG_ERROR_REQUIRED_KEY_MISSING,
	CONFIG_ERROR_OUT_OF_MEMORY,
};

struct Zenith_ConfigKey
{
	uint32 m_uIndex = 0xFFFFFFFFu;         // index in the frozen schema
	constexpr bool IsValid() const { return m_uIndex != 0xFFFFFFFFu; }
};

struct Zenith_ConfigLayerEntry               // the per-layer slot of resolution step 1, read-only
{
	Zenith_ConfigValue m_xValue;
	Zenith_ConfigProvenance m_xWhere;
};

struct Zenith_ConfigDiagnostic
{
	Zenith_ConfigError m_eError;
	Zenith_StringView m_strKey;
	Zenith_ConfigProvenance m_xWhere;
};

/// @contract type Zenith_ConfigSchema
/// Ownership: Created by Create as a Zenith_Owned<Zenith_ConfigSchema>; owned by the composition root.
/// Thread affinity: MAIN_THREAD until Freeze; ANY_THREAD and IMMUTABLE afterwards. Members non-const: Register
///   fills the schema after construction, before Freeze.
/// Invalidation: Zenith_ConfigKey values stay valid for the schema's lifetime.
/// Units: None.
/// Errors: Register returns CONFIG_ERROR_INVALID_KEY_NAME, CONFIG_ERROR_DUPLICATE_KEY, CONFIG_ERROR_SCHEMA_FROZEN,
///   CONFIG_ERROR_WRONG_TYPE (a default of another type) or CONFIG_ERROR_OUT_OF_MEMORY.
/// Version: since Z2 Stage 2. Key names are stable: a rename is an authoring-document migration of the
///   configuration files (project settings are an authoring-document type).
/// @endcontract
class Zenith_ConfigSchema
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_ConfigSchema>, Zenith_ConfigError> Create();
	Zenith_Result<Zenith_ConfigKey, Zenith_ConfigError> Register(const Zenith_ConfigKeyDesc& xDesc);
	void Freeze();
	bool IsFrozen() const;
	Zenith_Result<Zenith_ConfigKey, Zenith_ConfigError> FindKey(Zenith_StringView strKey) const;
	const Zenith_ConfigKeyDesc& GetDesc(Zenith_ConfigKey xKey) const;   // ZENITH_ASSERT valid
	uint32 GetKeyCount() const;
};

/// @contract type Zenith_ConfigStore
/// Ownership: Created by Create over a frozen schema; owned by ZenithPlatform's Zenith_Config (or a test).
///   Text is copied into the store's own arena.
/// Thread affinity: MAIN_THREAD. Values needed on other threads are copied into their owners' descs.
/// Invalidation: Views returned by Get stay valid until the store is destroyed (text is never freed);
///   CommitStagedUserValues changes the resolved values of the committed keys.
/// Units: None.
/// Errors: ApplyJsonLayer returns CONFIG_ERROR_PARSE for unparsable input or CONFIG_ERROR_OUT_OF_MEMORY;
///   ApplySimulationAffecting returns CONFIG_ERROR_PARSE for a malformed list and applies nothing;
///   every per-key problem becomes a Zenith_ConfigDiagnostic and a logged warning, never a failure
///   ([Z2-N-ARCH-929]); typed getters on the wrong type are ZENITH_CHECK.
/// Reentrancy: ALLOWED(Get, GetProvenance). Change callbacks may read, never write.
/// Version: since Z2 Stage 2. Source API.
/// @endcontract
class Zenith_ConfigStore
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_ConfigStore>, Zenith_ConfigError> Create(const Zenith_ConfigSchema& xSchema);
	Zenith_Result<void, Zenith_ConfigError> ApplyJsonLayer(Zenith_ConfigLayer eLayer, Zenith_ByteSpan xJson, Zenith_StringView strSource);
	Zenith_Result<void, Zenith_ConfigError> ApplyValueText(Zenith_ConfigLayer eLayer, Zenith_StringView strKey,
		Zenith_StringView strValue, Zenith_StringView strSource, uint32 uLine);
	Zenith_Result<void, Zenith_ConfigError> ValidateRequired() const;   // names the first missing key in its diagnostic
	Zenith_Result<void, Zenith_ConfigError> StageUserValue(Zenith_ConfigKey xKey, const Zenith_ConfigValue& xValue);
	uint32 CommitStagedUserValues();                                            // frame prologue; returns the number changed
	const Zenith_ConfigValue& Get(Zenith_ConfigKey xKey) const;
	bool GetBool(Zenith_ConfigKey xKey) const;
	sint64 GetInt(Zenith_ConfigKey xKey) const;
	float GetFloat(Zenith_ConfigKey xKey) const;
	Zenith_StringView GetString(Zenith_ConfigKey xKey) const;
	uint32 GetEnumIndex(Zenith_ConfigKey xKey) const;
	Zenith_ConfigProvenance GetProvenance(Zenith_ConfigKey xKey) const;
	Zenith_Span<const Zenith_ConfigDiagnostic> GetDiagnostics() const;
	Zenith_CallbackRegistry<void(Zenith_ConfigKey)>& GetUserChangeRegistry();
	uint64 GetRevision() const;
	Zenith_Result<void, Zenith_JsonWriteError> WriteDump(Zenith_JsonWriter& xWriter) const;
	Zenith_Result<void, Zenith_JsonWriteError> WriteLayer(Zenith_ConfigLayer eLayer, Zenith_JsonWriter& xWriter) const;
	Zenith_Result<void, Zenith_ConfigError> WriteSimulationAffecting(Zenith_Array<uint8>& axOut) const;   // replay header list
	Zenith_Result<void, Zenith_ConfigError> ApplySimulationAffecting(Zenith_ByteSpan xList, Zenith_StringView strSource);   // into CONFIG_LAYER_COMMAND_LINE
	const Zenith_ConfigLayerEntry* GetLayerEntry(Zenith_ConfigKey xKey, Zenith_ConfigLayer eLayer) const;   // null when that layer's slot is unset
	void ClearLayer(Zenith_ConfigLayer eLayer);   // unsets every slot of eLayer and recomputes resolution
};

// ZenithCore's own keys (memory, log, strings); every library has one such function, called by the composition root.
Zenith_Result<void, Zenith_ConfigError> Zenith_RegisterCoreConfigKeys(Zenith_ConfigSchema& xSchema, Zenith_BuildPlatform ePlatform);
```
<!-- /assumption -->

`Zenith_Config` (`Zenith2/Docs/Architecture/PlatformLayer.md` → "Configuration layering") forwards to this store and schema:

| `Zenith_Config` member | CORE call |
|---|---|
| `GetStore()` | the one `Zenith_ConfigStore` it owns |
| `FindKey(Zenith_StringView)` | `Zenith_ConfigSchema::FindKey` on the frozen schema the store was created over |
| `Get(Zenith_ConfigKey)` | `Zenith_ConfigStore::Get` |
| `GetProvenance(Zenith_ConfigKey)` | `Zenith_ConfigStore::GetProvenance` |
| `SetUserValue(Zenith_ConfigKey, const Zenith_ConfigValue&)` | `Zenith_ConfigStore::StageUserValue`, committed by `CommitStagedUserValues` at step 3 of the frame prologue |
| `WriteDump(Zenith_JsonWriter&)` | `Zenith_ConfigStore::WriteDump` |

<!-- informative -->
History: before the A3 Architecture batch (X49), `PlatformLayer.md` declared these types in `ZenithPlatform/Zenith_Config.h`, with `Zenith_ConfigError` as a 48-byte struct carrying the key and source text and with lookup by name; the batch adopted this document's form (one declaration in ZenithCore, `Zenith_ConfigError` an enumeration numbered from 1, per-key context in `Zenith_ConfigDiagnostic`, lookup through `Zenith_ConfigKey`). The C6 row of the cross-area notes records the reconciliation.
<!-- /informative -->

### Configuration file format

A configuration layer file is one JSON object (a root value that is not an object is `CONFIG_ERROR_PARSE` at the root's position, handled as resolution step 6) whose members are dotted keys and whose values are JSON booleans, numbers or strings (`{ "rhi.backend": "vulkan", "sim.tickHz": 30 }`), read with `Zenith_JsonLimits` defaults. An `INT` key accepts only integer text within `sint64`; a `FLOAT` key accepts any number whose magnitude is within `float` range (stored rounded to nearest `float`; a finite number beyond it is `CONFIG_ERROR_OUT_OF_RANGE`); an `ENUM` key accepts one of its value names; a `STRING` key accepts a string; `BOOL` accepts `true` or `false`. A nested object or array value is a `CONFIG_ERROR_NESTED_OBJECT` diagnostic for that member.

`ApplyValueText` (the `--set key=value` path) parses the text by the key's type: `true`/`false`; `Zenith_ParseInt64`; `Zenith_ParseDouble`; an enum name; any text for strings.

### Resolution algorithm

1. `Create` gives each key a slot per layer (`bool m_bSet; Zenith_ConfigValue m_xValue; Zenith_ConfigProvenance m_xWhere;`) and sets the engine-default slot from the desc unless `m_bRequired`.
2. Applying a value to layer L for key K: unknown key → `CONFIG_ERROR_UNKNOWN_KEY` diagnostic (a warning, [Z2-N-ARCH-929]); L not in the mask → `CONFIG_ERROR_LAYER_NOT_ALLOWED`; wrong type → `CONFIG_ERROR_WRONG_TYPE`; numeric outside `[m_xMin, m_xMax]` → `CONFIG_ERROR_OUT_OF_RANGE`; enum name unknown → `CONFIG_ERROR_INVALID_ENUM_VALUE`. Each diagnostic is appended and logged with `ZENITH_LOG_WARNING(xLOG_CATEGORY_CORE_CONFIG, …)` naming the key, the source and the line, and the slot is left unchanged, so the next-lower layer stays in effect. A valid value sets the slot with its provenance (line from the JSON node, `Zenith_JsonDocument::GetLine`).
3. The resolved value of K is the value of the highest layer whose slot is set; `Get` returns it in O(1) (the resolved index per key is recomputed on each apply).
4. `StageUserValue` validates as step 2 for `CONFIG_LAYER_USER` and stores the value in a staging list; `CommitStagedUserValues`, called by ZenithPlatform in the frame prologue, moves every staged value into the user slots, recomputes resolution, increments the revision and dispatches the change registry once per changed key on the main thread.
5. `ClearLayer(eLayer)` (`ZENITH_ASSERT(eLayer != CONFIG_LAYER_ENGINE_DEFAULT)`) unsets every slot of `eLayer`, recomputes resolution so each affected key falls to its next-lower set layer, and increments the revision; it dispatches no change callback (PLAT notifies tier changes itself), and staged user values are unaffected. `GetLayerEntry(xKey, eLayer)` returns the slot of step 1 when it is set and null otherwise; the pointer stays valid until the next apply, clear or commit on the store.
6. A whole-file `CONFIG_ERROR_PARSE` from `ApplyJsonLayer` is appended as a `Zenith_ConfigDiagnostic { CONFIG_ERROR_PARSE, "", { eLayer, strSource, line } }` and logged with the `Zenith_JsonReadError`'s line and column before the error is returned; the layer is left unchanged.
7. `WriteDump` writes `{ "<key>": { "value": …, "type": "…", "layer": "…", "source": "…", "line": N, "requiresRestart": bool, "simulationAffecting": bool }, … }` in ascending key byte order ([Z2-N-ARCH-937] consumes it).

### Simulation-affecting list

`WriteSimulationAffecting` appends, and `ApplySimulationAffecting` reads, this little-endian byte layout: `uint32` pair count, then per key in ascending key byte order a `uint16` key length and the key bytes, a `uint8` `Zenith_ConfigValueType`, and the value (`BOOL` one byte 0 or 1; `INT` an `sint64`; `FLOAT` the `uint32` bit pattern; `STRING` a `uint32` length and the bytes; `ENUM` a `uint16` length and the enumerator name, so a replay survives an enumerator reordering).

`ApplySimulationAffecting(xList, strSource)` validates the whole list before applying any value, under these parser limits: pair count at most the schema's key count, key length 1 … 128 bytes, string and enumerator text at most 4,096 bytes and valid UTF-8, keys strictly ascending, a type byte one of the five values, no byte after the last pair; any violation returns `CONFIG_ERROR_PARSE` with nothing applied. It then applies each pair to `CONFIG_LAYER_COMMAND_LINE` through resolution step 2 with provenance `{ CONFIG_LAYER_COMMAND_LINE, strSource, 0 }`, so a per-key problem (an unknown key, a wrong type, a value out of range) is a diagnostic and a warning, and a pair replaces any `--set` of the same key applied before it. The layer-mask test of step 2 is skipped for every replay pair: a recorded value always applies exactly, even for a key whose `m_uAllowedLayerMask` excludes `CONFIG_LAYER_COMMAND_LINE`, so a replay never runs silently with another value ([Z2-N-ARCH-930], Z2-D-019).

The list carries no version of its own: it is versioned by the enclosing replay header, and a change to this layout is a ZRPL format-version bump (SAVE; `Zenith2/Docs/Architecture.md` → "Multiplayer foundations").

### Core configuration keys

| Key | Type | Default (layer 1) | Allowed layers | Meaning |
|---|---|---|---|---|
| `log.level` | ENUM (`trace`, `debug`, `info`, `warning`, `error`, `off`) | `info` (Shipping: `warning`) | all | the router's global level, applied at startup step 7 (until then the desc's `m_eInitialGlobalLevel`, which `main` sets to this default) |
| `log.categoryLevels` | STRING | `""` | all | comma-separated `<category>=<level>` overrides |
| `log.ringBytes` | INT, `uLOG_RING_BYTES_MIN` … 2^32 − 1 | `uLOG_RING_BYTES_DEFAULT` (1 MiB); the pending row "Log router ring bytes" bounds it | platform, command line | the ring size `ResizeRing` applies at startup step 7; the ring from step 1b to step 7 is always `uLOG_RING_BYTES_DEFAULT` |
| `strings.maxStrings`, `strings.maxTextBytes` | INT | the pending row "String intern table capacity" | platform, command line | the intern table caps |
| `memory.budget.renderingBytes`, `memory.budget.streamingBytes`, `memory.budget.audioBytes`, `memory.budget.scriptsBytes`, `memory.budget.editorCachesBytes`, `memory.budget.assetsBytes`, `memory.budget.simulationBytes`, `memory.budget.otherBytes` (one per `Zenith_MemorySubsystem`; `otherBytes` is Z2-B-CORE-004 + Z2-B-CORE-005 + Z2-B-DEVT-001), `memory.budget.processBytes`, `memory.budget.deviceLocalBytes` | INT | from the generated budget table for the build platform (`Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Generated budget table") | engine default, command line | `Zenith_MemoryBudgets` |
| `memory.peak.streamingDoubleResidencyBytes`, `memory.peak.worldTransitionBytes`, `memory.peak.textureSwapBytes` | INT | the generated table | engine default, command line | transition peaks |
| `memory.threshold.warningPercent`, `memory.threshold.criticalPercent` | INT, 1 … 100 | the generated table (Z2-B-CORE-009, Z2-B-CORE-010) | engine default, command line | pressure thresholds |
| `memory.subsystemCriticalPercent`, `memory.hysteresisPercent`, `memory.hysteresisMs`, `memory.androidLowMemoryHoldMs` | INT | none: `m_bRequired`; supplied by ZenithCore's platform configuration fragment (`Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Tuning keys") | platform, command line | governor tuning |

## Diagnostics tests

| Test ID | File | Asserts |
|---|---|---|
| `Test_BuildInfo_SanitizerKind_MatchesPreset` | `Zenith_BuildInfo_Tests.cpp` | `Zenith_GetSanitizerKind()` equals the CTest-provided expected value for the preset (`NONE` in `win-debug`, `win-release`, `android-arm64`; `ASAN` in `win-asan`; `TSAN_UBSAN` in `linux-tsan`; `HWASAN` in `android-arm64-hwasan`), and `Zenith_IsSanitizerBuild()` agrees with it |
| `Test_BuildInfo_ExactlyOnePlatformAndConfiguration` | `Zenith_BuildInfo_Tests.cpp` | the `constexpr` readers agree with the preset the test was built in (compared with a CTest-provided expected value) |
| `Test_TaggedDiagnosticLine_Composes` | `Zenith_Diagnostics_Tests.cpp` | `Zenith_ComposeTaggedLine("Zenith2Test", "ok", …)` gives `Zenith2Test: ok` and its byte count; a 3,000-byte line composes into pieces of at most 2,048 bytes, each carrying the prefix; an empty line gives `Zenith2Test: ` |
| `Test_TaggedDiagnosticLine_LongTag_Asserts` | `Zenith_Diagnostics_DefectTests.cpp` | `z2-death-assert`: a 32-byte tag |
| `Test_Assert_Disabled_DoesNotEvaluate` | `Zenith_Assert_Tests.cpp` | in presets with asserts off, a condition with a side effect is not evaluated (compiled in a test-only TU with the macro's disabled form) |
| `Test_Assert_Failure_ReportsFormat` | `Zenith_Assert_DefectTests.cpp` | `z2-death-assert`: output first line matches `ZENITH ASSERT FAILED: x == 1 \(.*Zenith_Assert_DefectTests.cpp:[0-9]+\) value 2` |
| `Test_Check_Failure_ReportsFormat` | `Zenith_Assert_DefectTests.cpp` | `z2-death-check`, every preset including `win-release` |
| `Test_Fatal_ReportsCodeName` | `Zenith_Assert_DefectTests.cpp` | `z2-death-check`: `ZENITH FATAL FAILED: FATAL_CODE_QUEUE_FULL` |
| `Test_Unreachable_Reports` | `Zenith_Assert_DefectTests.cpp` | `z2-death-check` |
| `Test_Fail_RecursiveFailure_Terminates` | `Zenith_Assert_DefectTests.cpp` | a log hook that asserts during the fatal record terminates with the recursive-failure line; a sink that asserts inside `Write` (holding `uLOCK_LEVEL_LOG_SINKS`) terminates with the assert's first line and backtrace on the diagnostic output, no lock-order failure and no sink call |
| `Test_Types_ApiSurface_CstdintAliases` | `Zenith_Foundation_ApiSurface_Tests.cpp` | compile-only, every preset: `static_assert(std::is_same_v<uint64, std::uint64_t>)` and the same for `uint8`, `uint16`, `uint32`, `sint8` … `sint64` against `std::uint8_t` … `std::int64_t`; `std::is_same_v<uint, unsigned int>` and `std::is_same_v<sint, int>` (`Zenith2/Docs/Design/CORE.md` → "Foundation group") |
| `Test_Result_SuccessAndError_States` | `Zenith_Result_Tests.cpp` | `IsOk`, `IsError`, `Value`, `TakeValue`, `Error` for enum and error-record `E`; `Zenith_Result<void, E>`; `IsOk()` is true exactly when the stored error's code is 0: a success result's error has code 0 for both an enumeration and `Zenith_JsonReadError`, and a failure result's code is the one passed |
| `Test_Result_Layout_ValuePlusError` | `Zenith_Result_Tests.cpp` | `static_assert`s, in every preset: `sizeof(Zenith_Result<T, E>) == sizeof(struct { T; E; })` and `alignof` likewise for `T` in `uint8`, `uint64`, `Zenith_StringView`, a 40-byte struct and `E` in `Zenith_AllocError` and the 24-byte `Zenith_JsonReadError` (e.g. `Zenith_Result<uint64, Zenith_AllocError>` is 16 bytes); `sizeof(Zenith_Result<void, Zenith_AllocError>) == 1` and `sizeof(Zenith_Result<void, Zenith_JsonReadError>) == 24`; the literal sizes are the same in `win-debug` and `win-release` |
| `Test_Result_NonDefaultConstructibleValue_DoesNotCompile` | `Zenith_Result_NonDefaultConstructible_CompileFail.cpp.in` | CTest `WILL_FAIL` compile of `Zenith_Result<T, Zenith_AllocError>` for a `T` with no default constructor; the TU compiles once that `T` gains one |
| `Test_Result_WrongStateAccess_Asserts` | `Zenith_Result_DefectTests.cpp` | `z2-death-assert`, so registered only in presets with asserts enabled: `Value()` and `TakeValue()` of a failure result and `Error()` of a success result each terminate with `ZENITH ASSERT FAILED: ` (three cases) |
| `Test_Result_WrongStateAccess_UncheckedWhereAssertsOff` | `Zenith_Result_Tests.cpp` | where `Zenith_AreAssertsEnabled()` is false, `Value()` of a failure result returns the default-constructed `T` and `Error()` of a success result has code 0, without terminating; elsewhere the case reports itself skipped through `MESSAGE` |
| `Test_Result_Try_PropagatesSameEnum` | `Zenith_Result_Tests.cpp` | `ZENITH_TRY` and `ZENITH_TRY_ASSIGN` return the inner error and assign on success |
| `Test_ErrorCategory_EveryCoreEnumeratorMapped` | `Zenith_ErrorCategory_Tests.cpp` | each enumerator of every CORE error enumeration maps to a category and has a non-empty `Zenith_ToString` |
| `Test_Log_NoHook_WarningsToDiagnosticOutput` | `Zenith_Log_Tests.cpp` | with no hook, a warning reaches the diagnostic output and an info record does not (captured through a test output redirection) |
| `Test_Log_Router_NoSinks_RecordsStayInRing` | `Zenith_LogRouter_Tests.cpp` | a router created with the default desc and hooked, with no sink and no drain thread, receives 10 records and creates no thread (the process thread count is unchanged, `GetStats().m_xRing.m_ulDropped` is 0); a sink registered after the 10 emissions, then `StartDrainThread`, delivers all 10 to the sink's `Output` in sequence order |
| `Test_Log_Router_DrainThreadWithoutSinks_KeepsRecords` | `Zenith_LogRouter_Tests.cpp` | a drain thread started with no sink registered leaves 10 emitted records in the ring (no `Output` call, `GetStats` unchanged after 100 ms); `AddSink` then delivers all 10 in sequence order; a second router in the same state destroyed with no sink returns (no hang) and its final `GetStats().m_xRing.m_ulDropped` equals the 10 held records |
| `Test_Log_Router_ResizeRing_OutputsBufferedThenReplaces` | `Zenith_LogRouter_Tests.cpp` | with 10 records buffered in the default ring and one sink added, `ResizeRing(65536)` delivers all 10 to `Output` in sequence order before it returns and `GetStats().m_ulRingBytes` becomes 65,536; the next record lands in the new ring; with the heap countdown failing the new ring's allocation it returns `ALLOC_ERROR_OUT_OF_MEMORY`, outputs nothing and keeps the 10 records and the 1 MiB ring; `ResizeRing(uLOG_RING_BYTES_DEFAULT)` changes nothing |
| `Test_Log_Router_Stats_DefaultsAndCounts` | `Zenith_LogRouter_Tests.cpp` | a default-desc router reports `m_ulRingBytes` equal to `uLOG_RING_BYTES_DEFAULT`; under the initial level `LOG_LEVEL_INFO` two info records and one debug record give `m_ulRecordsRouted` 2 and `m_ulRecordsFiltered` 1; `SetGlobalLevel(LOG_LEVEL_DEBUG)` admits the next debug record |
| `Test_Log_Router_Contended_CountsFailedTryLock` | `Zenith_LogRouter_Tests.cpp` | a sink whose `Write` blocks for 30 ms on one thread while a second thread logs: the second thread's record is delivered, `GetStats().m_ulContended` is at least 1, and a router logged to from one thread alone reports 0 | default |
| `Test_Log_Router_FatalWithoutDrainThread_OutputsBufferedFirst` | `Zenith_LogRouter_DefectTests.cpp` | `z2-death-check`: a router with a file-backed test sink and no drain thread, 3 records buffered, then `ZENITH_FATAL(FATAL_CODE_LOGGING_UNAVAILABLE)`: the flushed file holds the 3 records and then the fatal record, in sequence order |
| `Test_Log_Router_OversizedRecord_DropsTrailingFields` | `Zenith_LogRouter_Tests.cpp` | a record with a 2,047-byte message and three 1,500-byte text fields is serialised within `uLOG_RECORD_MAX_SERIALISED_BYTES`, reaches `Output` with the fields that fit and `m_bTruncated` set |
| `Test_Log_Router_FanOutAndSequence` | `Zenith_LogRouter_Tests.cpp` | two sinks receive every admitted record once, sequences strictly increase |
| `Test_Log_Router_CategoryOverrideFilters` | `Zenith_LogRouter_Tests.cpp` | `SetCategoryLevel` admits and rejects as configured; the global level applies to other categories |
| `Test_Log_Router_RingOverflow_DropsOldest` | `Zenith_LogRouter_Tests.cpp` | with a 4 KiB ring and the drain stopped, the oldest records are dropped and counted |
| `Test_Log_Router_ConcurrentLogging_NoLoss` | `Zenith_LogRouter_Tests.cpp` | `z2-stress`, 8 threads × 10,000 records with a large ring: every record drained exactly once; runs under `linux-tsan` |
| `Test_Log_Router_Fatal_FlushesSynchronously` | `Zenith_LogRouter_DefectTests.cpp` | `z2-death-check`: a ring-backed test sink's flushed file contains the fatal record |
| `Test_LogRingSink_CopyRecentLines_Order` | `Zenith_LogSinks_Tests.cpp` | after 10 records into a ring sized for 6 lines, `CopyRecentLines(…, 4)` returns records 7 … 10 oldest first; an allocation failure of the output returns `ALLOC_ERROR_OUT_OF_MEMORY` |
| `Test_LogConsoleSink_LineFormat` | `Zenith_LogSinks_Tests.cpp` | one record with a text field containing `"` and `\`, an integer, a double and a bool produces exactly the expected line (diagnostic output captured by redirection) |
| `Test_Log_Truncation_MarksRecord` | `Zenith_Log_Tests.cpp` | a 5,000-byte message is truncated to 2,047 bytes on a UTF-8 boundary with `m_bTruncated` |
| `Test_Log_Fields_DeliveredToSinks` | `Zenith_LogRouter_Tests.cpp` | a record with one field of each `Zenith_LogFieldKind` reaches a sink's `Write` and, after the drain, its `Output` with every key and value intact |
| `Test_Log_CompiledOut_InShipping` | `Zenith_Log_Tests.cpp` | in the Shipping test build, a `ZENITH_LOG_DEBUG` with a side-effecting argument does not evaluate it |
| `Test_Clock_Monotonic_NeverDecreases` | `Zenith_Clock_Tests.cpp` | 10^6 reads on 4 threads, each thread's sequence non-decreasing |
| `Test_Clock_Stopwatch_Elapsed` | `Zenith_Clock_Tests.cpp` | `GetElapsedUs` after `Zenith_SleepUs(20000)` is at least 20,000; `Restart` returns the elapsed value and restarts from 0 |
| `Test_Clock_Utc_FormatsIso8601` | `Zenith_Clock_Tests.cpp` | fixed instants format to their expected ISO and compact text |
| `Test_ConfigStore_LayerPrecedence` | `Zenith_ConfigStore_Tests.cpp` | a key set in all six layers resolves to the command line; removing layers steps down in order; provenance names each source and line |
| `Test_ConfigStore_RejectedValue_KeepsLowerLayer` | `Zenith_ConfigStore_Tests.cpp` | each rejection kind of step 2 leaves the lower value and records its diagnostic |
| `Test_ConfigStore_UnknownKey_Warns` | `Zenith_ConfigStore_Tests.cpp` | an unknown key produces `CONFIG_ERROR_UNKNOWN_KEY` and changes nothing |
| `Test_ConfigStore_RequiredMissing_Fails` | `Zenith_ConfigStore_Tests.cpp` | `ValidateRequired` fails naming the key |
| `Test_ConfigStore_StagedUserValue_CommitsAtPrologue` | `Zenith_ConfigStore_Tests.cpp` | `Get` is unchanged until `CommitStagedUserValues`, then changed; the change registry fires once |
| `Test_ConfigStore_Dump_SortedAndComplete` | `Zenith_ConfigStore_Tests.cpp` | the dump lists every key in byte order with provenance |
| `Test_ConfigStore_WriteLayer_RoundTrips` | `Zenith_ConfigStore_Tests.cpp` | `WriteLayer(CONFIG_LAYER_USER, …)` writes exactly the user-layer keys in byte order, and applying that output to a fresh store's user layer resolves every key to the same value |
| `Test_ConfigStore_SimulationAffecting_Serialized` | `Zenith_ConfigStore_Tests.cpp` | `WriteSimulationAffecting` lists exactly the keys registered with `m_bSimulationAffecting`, in key byte order, as length-prefixed key/value pairs ([Z2-N-ARCH-930]) |
| `Test_ConfigStore_SchemaFrozen_RejectsRegister` | `Zenith_ConfigStore_Tests.cpp` | `CONFIG_ERROR_SCHEMA_FROZEN` |
| `Test_ConfigStore_GetLayerEntry_ReportsEachLayer` | `Zenith_ConfigStore_Tests.cpp` | for a key set in the platform and command-line layers, `GetLayerEntry` returns each value with its source and line, and null for the four unset layers |
| `Test_ConfigStore_ClearLayer_ReResolves` | `Zenith_ConfigStore_Tests.cpp` | a key set in the quality-tier and platform layers resolves to the quality-tier value; `ClearLayer(CONFIG_LAYER_QUALITY_TIER)` makes it resolve to the platform value with platform provenance, increments `GetRevision`, dispatches no change callback and leaves a staged user value staged |
| `Test_ConfigStore_ApplySimulationAffecting_RoundTrips` | `Zenith_ConfigStore_Tests.cpp` | the bytes of `WriteSimulationAffecting` from a store with one simulation-affecting key of each type, applied to a fresh store, resolve every key to the same value with provenance `{ CONFIG_LAYER_COMMAND_LINE, "replay:test", 0 }` and override an earlier `--set` of one of them; a simulation-affecting key registered with the mask engine default \| game (no command line) still resolves to its recorded value with that provenance and records no `CONFIG_ERROR_LAYER_NOT_ALLOWED` diagnostic |
| `Test_ConfigStore_ApplySimulationAffecting_RejectsMalformed` | `Zenith_ConfigStore_Tests.cpp` | a truncated list, a count above the key count, unsorted keys, a 129-byte key, an unknown type byte and a trailing byte each return `CONFIG_ERROR_PARSE` and change no slot; an unknown key inside a well-formed list is a `CONFIG_ERROR_UNKNOWN_KEY` diagnostic and the other pairs apply |
| `Test_ConfigStore_ParseError_LoggedWithPosition` | `Zenith_ConfigStore_Tests.cpp` | `ApplyJsonLayer` of `{ "a": 1,, }` returns `CONFIG_ERROR_PARSE`, appends a diagnostic whose provenance names the source and line 1, and logs one warning carrying the line and column; a root array `[1]` and a root scalar `3` each return `CONFIG_ERROR_PARSE` with the root's line and change no slot |
| `Test_ConfigStore_Fuzz` (`Zenith_Fuzz_ConfigLayer`) | `Tests/Fuzz/ConfigLayer.cpp` | libFuzzer over `ApplyJsonLayer`: no crash, only diagnostics |
