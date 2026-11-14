# DEVT design: crash capture and reports

This file is part of the DEVT design family; the entry document is `Zenith2/Docs/Design/DEVT.md`. It specifies `Zenith_CrashHandler` (the noun of D-SEL-CRASH (open, Stage 2)), its lifecycle in the composition root, the one process-wide anchor through which the OS crash callbacks reach it (ruling Q88 (a)), the crash report files and schema, the signal-safe report tail, annotations, breadcrumbs and attachments, the lock-free crash log mirror, how a `ZENITH_FATAL` is recognised, capture on Windows (with the helper's security contract and the shared section's layout) and Android, the Linux test host (where capture is out of scope), sanitizer builds, the script hang report, thread registration, next-launch finalization, symbolization, and the D-SEL-CRASH selection with both candidate realisations. The code lives in ZenithPlatform (`Zenith_CrashHandler.h`, `Internal/CrashHandler/**`, `Internal/<Platform>/Zenith_Crash*`), which PLAT owns as a library and DEVT owns as files (`Zenith2/Docs/Design/DEVT.md` → "DEVT files in other libraries").

## Scope of this file

| Concern | Owner | Specified here |
|---|---|---|
| the failure procedure, `ZENITH_FATAL`, `Zenith_TerminateProcess` | CORE (`Zenith2/Docs/Design/CORE/Diagnostics.md` → "Failure procedure") | how the crash handler recognises a fatal from CORE's existing mechanism (§ "Fatal failures"); DEVT asks CORE for no new hook |
| the test main | CORE (`Zenith2/Engine/ZenithCore/Tests/Support/Zenith_TestMain.cpp`) | the one-line main ZenithPlatform's crash tests supply instead, which dispatches the helper (row DV29 (b)) |
| GPU fault breadcrumbs and the device-loss report | RHI (`Zenith2/Docs/Design/RHI/Recovery.md` → "Breadcrumbs", "Device-loss report") | associating the report with a crash report |
| the script watchdog | SCRIPT | the hang report it writes through this handler |
| symbol retention for releases | SHIP (`Zenith2/Docs/Design/SHIP/Release.md` → "Symbols") | symbolizing a report against retained symbols |
| crash-report upload | none: excluded (`Zenith2/Docs/Design/DEVT.md` → "What DEVT does not own") | reports stay on the user mount |

## Zenith_CrashHandler header

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_CrashHandler.h
#pragma once
#include "ZenithCore/Zenith_Heap.h"
#include "ZenithCore/Zenith_LogRouter.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringView.h"

class Zenith_VirtualFileSystem;
class Zenith_FaultInjector;

inline constexpr uint32 uCRASH_MAX_ANNOTATIONS = 64;
inline constexpr uint32 uCRASH_ANNOTATION_KEY_BYTES = 32;
inline constexpr uint32 uCRASH_ANNOTATION_VALUE_BYTES = 128;
inline constexpr uint32 uCRASH_MAX_ATTACHMENTS = 8;
inline constexpr uint32 uCRASH_BREADCRUMBS = 64;
inline constexpr uint32 uCRASH_BREADCRUMB_BYTES = 96;
inline constexpr uint32 uCRASH_LOG_MIRROR_BYTES = 65536;
inline constexpr uint32 uCRASH_MAX_THREADS = 64;
inline constexpr uint32 uCRASH_FATAL_MESSAGE_BYTES = 256;
inline constexpr uint32 uFAULT_POINT_CRASH_HANDLER_DIRECTORY = 0x44564301;
inline constexpr uint32 uFAULT_POINT_CRASH_HANDLER_HELPER = 0x44564302;
inline constexpr uint32 uFAULT_POINT_CRASH_HANDLER_SIGNALS = 0x44564303;
inline constexpr const char* szCRASH_HELPER_OPTION = "--zenith-crash-helper";

enum Zenith_CrashHandlerErrorCode : uint8 { /* the enumerators of Zenith2/Docs/Design/DEVT.md "Error enumerations" */ };

/// @contract value Zenith_CrashHandlerError
/// Ownership: Plain value (the record form of [Z2-N-CONTRACT-480]).
/// Thread affinity: IMMUTABLE; members non-const: held as the error member of Zenith_Result, which callers assign.
/// Invalidation: None.
/// Units: m_uOsError is the platform error code (GetLastError, errno) or 0.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_CrashHandlerError
{
	Zenith_CrashHandlerErrorCode m_eCode;
	uint32 m_uOsError;
};
constexpr const char* Zenith_ToString(Zenith_CrashHandlerErrorCode eCode);
constexpr Zenith_ErrorCategory Zenith_GetErrorCategory(Zenith_CrashHandlerErrorCode eCode);

/// @contract value Zenith_CrashHelperIdentity
/// Ownership: Plain value of compile-time facts the calling main supplies; never read from the shared section.
/// Thread affinity: IMMUTABLE; members non-const: MOD's runtime state and the editor's tool-runtime desc hold it in a
///   member filled after construction.
/// Invalidation: The views are string literals.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_CrashHelperIdentity
{
	Zenith_StringView m_strGame;          // the user mount's game folder name (the Player's game, "ZenithEditor", "ZenithPlatformTests")
	uint64 m_ulBuildIdentity = 0;        // the executable's own build identity
};

/// @contract value Zenith_CrashHandlerDesc
/// Ownership: Plain value copied by Create; the file system and router outlive the handler.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: Create returns CRASH_HANDLER_ERROR_DISABLED when m_bEnabled is false (not a failure).
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_CrashHandlerDesc
{
	const bool m_bEnabled = true;                        // crash.enabled; false in sanitizer builds
	const uint32 m_uMaxReports = 16;                     // crash.maxReports
	Zenith_VirtualFileSystem* const m_pxFiles = nullptr; // the user mount
	Zenith_LogRouter* const m_pxRouter = nullptr;        // the mirror sink is added to it
	const Zenith_CrashHelperIdentity m_xIdentity{};      // the same values the calling main passes to Zenith_RunCrashHelperIfRequested
	const uint64 m_ulCompositionHash = 0;
	const Zenith_StringView m_strExecutablePath{};       // Windows: the running executable, relaunched as the helper; its main
	                                                     // dispatches szCRASH_HELPER_OPTION (§ "Windows capture")
	Zenith_Heap* const m_pxHeap = nullptr;
	Zenith_FaultInjector* const m_pxFaultInjector = nullptr;
};

/// @contract value Zenith_CrashAttachment
/// Ownership: Plain value; the handler copies the path.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: AddAttachment returns CRASH_HANDLER_ERROR_ATTACHMENTS_FULL beyond uCRASH_MAX_ATTACHMENTS.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_CrashAttachment
{
	const Zenith_StringView m_strName{};     // e.g. "device-loss"; at most 32 bytes
	const Zenith_StringView m_strUserPath{}; // a user-mount path, e.g. "user:/crash/device_loss_*.json" (a trailing '*' glob)
	const bool m_bNewestOnly = true;         // with a glob: the newest match written before the crash
};

/// @contract type Zenith_CrashHandler
/// Ownership: Created by Create as a Zenith_Owned<Zenith_CrashHandler> by the composition root at startup step 7, after
///   the log sinks; destroyed last of the step-7 objects in shutdown step 10b. Owns the annotation table, the log mirror,
///   the fatal slot, the breadcrumb ring, the pre-formatted report buffers, the alternate signal stacks (Android) and the
///   helper process (Windows). At most one exists per process: Create claims the process-wide anchor
///   g_pxCrashHandlerAnchor before any side effect, and a second Create while one lives returns
///   CRASH_HANDLER_ERROR_ALREADY_INSTALLED having changed nothing (Crash.md "Crash handler anchor"). State is private
///   members of this class; the anchor, which holds only this object's address, is the one process-wide variable.
/// Thread affinity: MAIN_THREAD for Create, Finalize and destruction; ANY_THREAD for SetAnnotation, AddBreadcrumb,
///   RegisterCurrentThread, UnregisterCurrentThread, WriteHangReport, AddAttachment and GetResidentBytes.
/// Invalidation: None.
/// Units: Times in microseconds of Zenith_GetMonotonicTimeNs / 1000 and UTC microseconds.
/// Errors: Create returns Zenith_Result<…, Zenith_CrashHandlerError>; ANY_THREAD members never fail except as declared.
/// Reentrancy: NOT_APPLICABLE. It invokes no callbacks into callers; the crash path calls only async-signal-safe or
///   pre-resolved code (Crash.md "Windows capture", "Android capture").
/// Locks: none; every shared structure is lock-free with atomic storage (Crash.md "Annotations and breadcrumbs").
/// Version: since Z2 Stage 2 (API-only header); implementation Stage 3. Present in Shipping.
/// @endcontract
class Zenith_CrashHandler
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_CrashHandler>, Zenith_CrashHandlerError> Create(const Zenith_CrashHandlerDesc& xDesc);
	~Zenith_CrashHandler();

	// Next-launch work: finalizes reports of earlier runs and enforces retention (Crash.md "Next-launch finalization").
	uint32 Finalize();   // returns the number of reports finalized

	void SetAnnotation(Zenith_StringView strKey, Zenith_StringView strValue);   // a full table is counted, not returned
	void AddBreadcrumb(Zenith_StringView strText);                              // at most 96 bytes kept
	Zenith_Result<void, Zenith_CrashHandlerError> AddAttachment(const Zenith_CrashAttachment& xAttachment);

	void RegisterCurrentThread(Zenith_StringView strName);   // alternate stack, stack guarantee, name
	void UnregisterCurrentThread();
	// The JOBS lifecycle thunk: pass Zenith_CrashHandler::JobThreadLifecycle and the handler pointer in Zenith_JobSystemDesc.
	static void JobThreadLifecycle(uint32 uThreadIndex, const char* szThreadName, bool bStarted, void* pUserData);

	// The script watchdog: writes a hang report naming the hung thread, then returns (the caller then calls ZENITH_FATAL).
	void WriteHangReport(uint64 ulThreadID, Zenith_StringView strReason, Zenith_StringView strScriptStack);

	Zenith_LogSink& GetLogMirror();            // added to the router by Create; removed by the destructor
	bool IsArmed() const;
	uint64 GetResidentBytes() const;          // part of Z2-B-DEVT-001
};

/// @contract functions CrashHelperDispatch
/// Ownership: No state; on Windows, when the arguments request the helper, runs the whole helper process to completion,
///   allocating its buffers from the installed root heap and freeing them before it returns (Crash.md "Crash helper
///   security contract", section "Helper memory").
/// Thread affinity: MAIN_THREAD, the statement directly after Zenith_InstallRootHeap(&xHeap), which stays the first
///   statement of main ([Z2-N-CONTRACT-142]); before startup step 1b, so no log router, hook table or command line exists.
///   When it returns true, main uninstalls the root heap and returns iExitCode.
/// Invalidation: None.
/// Units: None.
/// Errors: Returns false when argv[1] is not szCRASH_HELPER_OPTION (and always on Android and Linux); otherwise true with
///   iExitCode set: 0 after a report or a clean parent exit, 3 when the arguments or the section fail validation (one
///   diagnostic line, nothing written), 4 when a report file could not be created.
/// Version: since Z2 Stage 2. Called by every main that creates a crash handler: the Player's (MOD), the editor's (EDIT) and
///   ZenithPlatform's crash-test main (rows DV11, DV27, DV29).
/// @endcontract
bool Zenith_RunCrashHelperIfRequested(int iArgc, char** ppszArgv, const Zenith_CrashHelperIdentity& xIdentity,
	int& iExitCode);
```

## Crash handler lifecycle

| Step | Action |
|---|---|
| startup step 1, in `main`, directly after `Zenith_InstallRootHeap(&xHeap)` | `int iExit = 0; if (Zenith_RunCrashHelperIfRequested(argc, argv, xIdentity, iExit)) { Zenith_InstallRootHeap(nullptr); return iExit; }` (a relaunched helper never starts the engine; in the Player this is the first action of `Zenith_PlayerMain` after its step 1, `Zenith2/Docs/Design/MOD.md` → "Player headers", row DV11) |
| startup step 7 (MOD's step-7 row; the Player, the editor and the crash tests' child process, never the host tools) | `Create`, in this order: validates the desc (`CRASH_HANDLER_ERROR_DISABLED` when `m_bEnabled` is false; the Linux stub returns `CRASH_HANDLER_ERROR_UNSUPPORTED` here, § "Linux capture"); allocates the handler object from `m_pxHeap`; claims the anchor (§ "Crash handler anchor"; `CRASH_HANDLER_ERROR_ALREADY_INSTALLED` with the object destroyed and nothing else touched); creates `user:/crash/` (fault point `…DIRECTORY`); computes the run stem `<utc yyyymmddThhmmssZ>-<pid>`; pre-builds the report buffers (the JSON head with game, build identity, composition hash, platform, device, OS version, already serialized; the crash path only fills the tail); adds the log mirror sink, which also fills the fatal slot (§ "Fatal failures"); prepares the platform capture (Windows: starts the helper, fault point `…HELPER`; Android: the alternate signal stack of the main thread); registers the main thread; arms the handler (`m_uArmed.store(1, release)`); and installs the OS entry points last (Windows: § "Windows capture" step 3; Android: `sigaction` for `SIGSEGV`, `SIGBUS`, `SIGFPE`, `SIGILL`, `SIGABRT`, `SIGTRAP`, `SIGSYS` with `SA_SIGINFO \| SA_ONSTACK`, fault point `…SIGNALS`), so no trampoline can reach a half-built handler. Every failure after the claim unwinds through the destructor, which releases the anchor. It installs no CORE hook: CORE has none for fatals, and DEVT needs none (§ "Fatal failures") |
| step 7, immediately after | `Finalize()` for earlier runs' reports (next-launch finalization) |
| runtime | `SetAnnotation` by the runtime (`world.primary`, `scene`, `frame`, `gpu.backend`, `gpu.adapter`, `memory.level`), breadcrumbs (world transitions, scene loads, device loss, lifecycle changes), attachments (RHI's device-loss report glob) |
| shutdown step 10b | the destructor, in this order: restores the previous OS entry points (the unhandled-exception filter, the CRT handlers, `SIGABRT`; on Android the seven previous `sigaction`s); releases the anchor (§ "Crash handler anchor": while a claim holds the handler the destructor retries every 1 ms, because a real capture ends the process during the wait and a stale claim on an unarmed handler is given back by the trampoline's step 5); clears `m_uArmed`; unregisters the main thread; removes the mirror sink; stops the helper (Windows: signals its exit event and waits up to 2 s, then terminates it); frees its buffers |

- Creation failure with `crash.enabled` true fails startup step 7 with exit code 4 (`Zenith2/Docs/Design/DEVT.md` → "Failure handling"); `CRASH_HANDLER_ERROR_DISABLED` is not a failure and the runtime continues with a null handler.
- Shipping: the handler is identical in Shipping (crash capture is not a development service): its sources contain no `Zenith_IsShippingBuild()` branch, which `Test_CrashHandler_SourcesHaveNoShippingBranch` asserts by scanning `Internal/CrashHandler/**` and the `Internal/<Platform>/Zenith_Crash*` files (the test receives the source directory as a compile definition, AUDIO's source-scan pattern); it opens no socket and writes only to the user mount, so it exposes nothing remotely (Z2-R-DEVT-086), and the `Libgraph` Shipping symbol check shows no development-service symbol beside it. Its behaviour with every Shipping flag and define is exercised by the `ShippingTest` configuration ([Z2-N-ARCH-231]): every `Test_CrashDeath_*` case of `Zenith2/Docs/Design/DEVT/Tests.md` → "Crash tests" is an ordinary case that crashes a child process (no death-test suite), so it runs in the `win-shipping-test` preset like every ordinary case (once the `Test` step runs that preset, a PROC hand-off, row DV17 (f)).
- Tools and the editor: the editor creates a handler whose desc is aggregate-initialised with `m_xIdentity` `{ "ZenithEditor", 0 }`; host tools (`ZenithCook`, `ZenithDevTool`) do not.

## Crash handler anchor

<!-- assumption: pending X273 (ruling Q88 (a), 2026-10-09; human may override, fallback Q88 (c)) -->
The OS entry points the custom realisation installs carry no user-data argument (`SetUnhandledExceptionFilter`'s filter, `_set_invalid_parameter_handler`, `_set_purecall_handler`, `signal(SIGABRT)` and the `sigaction` handlers of § "Android capture"), so a crash can reach the handler only through process-wide state. Ruling Q88 (a) admits exactly one such variable, recorded as a dated extension of Z2-D-046 by the core-document batch through X273 and its amendment X282 (which also names the crash path's claim as the anchor's third writer and the same trampoline's unclaim as its fourth write; with [Z2-N-CONTRACT-141], the allowlist entries of [Z2-N-CONTRACT-152] and [Z2-N-ARCH-866] reworded there), completed by X308 (the `g_` row of Contracts' "Naming and layout" table and WorkerGuide's forbidden-constructs row), row DV17 (g) (the protected `Zenith2/CLAUDE.md` naming bullet and non-negotiable 3, a PROC `needs-human` edit) and row DV35 (`Zenith2/Docs/Design/CORE.md`'s count of the allowlisted variables), and the `ForbiddenCheck` F16 writable-data scan allowlists exactly that symbol. Everything else the crash path uses (the section, the tables, the buffers, the capture's thread ID) is a member of the handler object the anchor points to. If the human overrides the ruling with Q88 (c), the custom realisation is withdrawn, D-SEL-CRASH selects Crashpad, the anchor file is not added, and `CRASH_HANDLER_ERROR_ALREADY_INSTALLED` and its tests are removed in that ruling's diff.
<!-- /assumption -->

```cpp
// Zenith2/Engine/ZenithPlatform/Internal/CrashHandler/Zenith_CrashHandlerAnchor.h
#pragma once
#include <atomic>
#include "ZenithCore/Zenith_Types.h"

class Zenith_CrashHandler;

// The one writable symbol of ZenithPlatform, defined in Zenith_CrashHandlerAnchor.cpp as
//   constinit std::atomic<Zenith_CrashHandler*> g_pxCrashHandlerAnchor{ nullptr };
extern std::atomic<Zenith_CrashHandler*> g_pxCrashHandlerAnchor;
static_assert(std::atomic<Zenith_CrashHandler*>::is_always_lock_free);   // its operations are async-signal-safe

enum Zenith_CrashAnchorClaim : uint8
{
	CRASH_ANCHOR_CLAIM_NONE = 1,      // no handler: the trampoline returns as the OS defines for an unhandled event
	CRASH_ANCHOR_CLAIM_OWNED,         // this thread now owns the capture of the handler written to pxOut
	CRASH_ANCHOR_CLAIM_IN_PROGRESS,   // another thread owns a capture: the second-crash rule applies
};

// The reserved value "a capture owns the handler": address 1, which no handler object can have (objects are 8-aligned).
Zenith_CrashHandler* Zenith_CrashAnchorCapturing();
// Create: compare_exchange_strong(nullptr -> &xHandler, acq_rel, acquire); false when another handler or a capture holds it.
bool Zenith_CrashAnchorInstall(Zenith_CrashHandler& xHandler);
// The destructor: compare_exchange_strong(&xHandler -> nullptr, acq_rel, acquire); false when a capture owns it.
bool Zenith_CrashAnchorRelease(Zenith_CrashHandler& xHandler);
// The trampolines: load(acquire), then compare_exchange_strong(p -> Zenith_CrashAnchorCapturing(), acq_rel, acquire).
// Never dereferences p; async-signal-safe.
Zenith_CrashAnchorClaim Zenith_CrashAnchorClaimCapture(Zenith_CrashHandler*& pxOut);
// The defensive undo of a claim on a handler that is not armed: store(&xHandler, release).
void Zenith_CrashAnchorUnclaim(Zenith_CrashHandler& xHandler);
```

The anchor's values and every transition (nothing else ever writes it):

| From | To | Written by | Memory order | On failure |
|---|---|---|---|---|
| `nullptr` | the handler's address | `Create`, through `Zenith_CrashAnchorInstall`, before any side effect | CAS `acq_rel`, failure `acquire` | `CRASH_HANDLER_ERROR_ALREADY_INSTALLED`: the new object is destroyed; no directory, buffer, helper or OS entry point was touched |
| the handler's address | `nullptr` | the destructor, through `Zenith_CrashAnchorRelease`, after restoring the previous OS entry points | CAS `acq_rel`, failure `acquire` | the value is `Zenith_CrashAnchorCapturing()`: a claim holds the handler, and the destructor retries: it sleeps 1 ms (`Sleep(1)` on Windows, `nanosleep` of 1 ms on Android) and repeats the CAS until it succeeds, never freeing the handler under a claim. A real capture terminates the process during the loop; a stale trampoline's transient claim on a handler that is not yet armed is given back by step 5's `Zenith_CrashAnchorUnclaim`, after which the next try succeeds (so a `Create` that fails at a fault point after its own claim always returns) |
| the handler's address | `Zenith_CrashAnchorCapturing()` | a trampoline, through `Zenith_CrashAnchorClaimCapture` | CAS `acq_rel`, failure `acquire` | the observed value is examined again (step 2 below); the anchor changes at most twice in a handler's life, so the loop is bounded |
| `Zenith_CrashAnchorCapturing()` | the handler's address | the claiming trampoline alone, through `Zenith_CrashAnchorUnclaim`, when the claimed handler is not armed | `store(release)` | none |

The trampoline, the one function every OS entry point routes to (§ "Windows capture" step 3, § "Android capture" step 2):

1. `p = g_pxCrashHandlerAnchor.load(acquire)`.
2. `p == nullptr`: not this handler's event. The unhandled-exception filter returns `EXCEPTION_CONTINUE_SEARCH`, the CRT handlers return, a signal handler returns (a re-executed fault, or the re-raise inside `abort`, then reaches the previous action the destructor restored before it released the anchor).
3. `p == Zenith_CrashAnchorCapturing()`: a second crash while another thread captures. It dereferences nothing and outlasts the capture: Windows `Sleep(1000)` in a loop for at most 30 s, during which the capturing thread's `TerminateProcess` ends the process (the capture waits at most 15 s on the helper's done event, § "Windows capture" step 3 (d), so a slow `MiniDumpWriteDump` is never cut off), and only at that bound `TerminateProcess(GetCurrentProcess(), <its own code>)` with the exit code of § "Windows capture" step 3 (e); Android `nanosleep` of 1 s in a loop for at most 30 s, then the default action for the signal and `tgkill` to re-raise it. So a second crash never writes a second report and never ends the process while the first capture runs.
4. Otherwise `compare_exchange_strong(p, Zenith_CrashAnchorCapturing(), acq_rel, acquire)`; on failure continue at step 2 with the value observed.
5. Owned: `p->m_uArmed.load(acquire)` must be 1, which the `release` store of `Create` makes visible together with every buffer built before it. Any other value means the handler is not armed (impossible while `Create` installs the entry points after arming, and kept because it costs one load): `Zenith_CrashAnchorUnclaim(*p)`, then step 2's behaviour.
6. Capture with `p`. The capture never gives the anchor back: it ends the process.

Why the trampoline claims through the anchor rather than through a flag member: a trampoline must not dereference the handler until it owns it, because the destructor may free the object between the trampoline's load and its first access. Its own successful CAS is that ownership, and after the destructor's release the CAS can no longer succeed, so the object is never used after it is freed. A handler created afterwards at the same address is live; `Create` claims the anchor before it arms, so a stale CAS that succeeds on it may find it not yet armed, and step 5 then gives the claim back (the destructor's retry of the second row waits out that window).

- Allowlist entry: `{ "symbol": "g_pxCrashHandlerAnchor", "source": "Zenith2/Engine/ZenithPlatform/Internal/CrashHandler/Zenith_CrashHandlerAnchor.cpp", "decision": "Z2-D-046" }` in `Zenith2/Tools/Gates/writable_data_allowlist.json` ([Z2-N-CONTRACT-153]; the entry cites Z2-D-046, which X273's dated extension amends). The file is protected, so the entry is PROC's `needs-human` edit (row DV17 (e)), landed before the Stage 3 crash implementation ticket together with row DV17 (g), the protected `Zenith2/CLAUDE.md` edit (the naming bullet reads "the five allowlisted variables" and non-negotiable 3 "Only the root heap and profiler hooks installed in `main` and, from startup step 7, the crash handler anchor"), because a worker on that ticket would otherwise stop on the contradiction (non-negotiable 18); X273, X282 and X308 land the core-document wording in the same batch.
- Lifetime: the anchor is non-null from startup step 7 to shutdown step 10b; X273 (3) rewords [Z2-N-ARCH-866] accordingly.
- Crashpad realisation: the anchor exists under either realisation. Under Crashpad, `Create` claims and the destructor releases it exactly as above and no trampoline reads it, because Crashpad installs its own handlers, whose process-wide objects are third-party code (no allowlist entry is added for them); there it serves only `CRASH_HANDLER_ERROR_ALREADY_INSTALLED`, which therefore does not depend on the realisation. X282 (2) states this scope in Z2-D-046's dated extension, so the human sees through Q88's flag that the exception is kept under both realisations.
- Linux: `Create` returns before the claim (§ "Linux capture"), so the anchor stays null there; the neutral functions above are still compiled and tested on Linux.
- Tests (`Zenith2/Docs/Design/DEVT/Tests.md` → "Crash tests"): `Test_CrashHandler_AnchorSetAndCleared` (Windows, through `Create`): after `Create` the anchor equals the handler and `IsArmed()` is true; after the handler's destruction it is null, the test's own sentinel unhandled-exception filter installed before `Create` is the active filter again, and a new `Create` succeeds; a `Create` that fails at `uFAULT_POINT_CRASH_HANDLER_DIRECTORY` leaves the anchor null. `Test_CrashHandler_SecondCreateRefused`: a second `Create` while the first lives returns `CRASH_HANDLER_ERROR_ALREADY_INSTALLED`, leaves the anchor unchanged, starts no second helper and writes no file into the crash directory (none whose stem carries the test process's PID; other cases' children write there concurrently). `Test_CrashHandler_AnchorClaimProtocol` (every host preset, `linux-tsan` included; the neutral functions with a stand-in address that is never dereferenced): every row of the table above, and the stress sub-cases (2, 8 and the hardware concurrency threads, [Z2-N-CONTRACT-357]), in which the threads race `Zenith_CrashAnchorClaimCapture` against the main thread's `Zenith_CrashAnchorRelease` for 10,000 rounds and each round has exactly one winner (one claim `OWNED` or the release `true`, never both). Each case leaves the anchor null, because the cases of `ZenithPlatform_Tests` share one process.

## Crash report files

All under `user:/crash/` (the user mount: `SDL_GetPrefPath("Zenith2", <Game>)` on Windows and the app's internal storage on Android, PLAT's mount table in `Zenith2/Docs/Design/PLAT/Filesystem.md`):

| File | Written | Content |
|---|---|---|
| `<stem>.report.json` | at crash (pre-formatted head plus tail; on Windows composed by the helper, on Android the tail written by `Zenith_CrashJsonTail`, § "Signal-safe report tail") and rewritten at finalization | the report (§ "Crash report schema") |
| `<stem>.dmp` | Windows, by the helper | a minidump (`MiniDumpWithIndirectlyReferencedMemory \| MiniDumpWithThreadInfo \| MiniDumpWithUnloadedModules \| MiniDumpWithProcessThreadData`) |
| `<stem>.tombstone.pb` | Android, at finalization | the debuggerd tombstone retrieved from `ApplicationExitInfo`, copied unmodified |
| `<stem>.log.txt` | at crash | the crash log mirror's text (§ "Crash log mirror") |
| `<stem>.hang.txt` | by `WriteHangReport` | the hang report (§ "Hang reports") |
| `<stem>.attach-<name>.<ext>` | at finalization | copies of the attachments (§ "Attachments") |
| `<stem>.corrupt` | at finalization | a report that failed to parse, renamed |
| `<stem>.pending` | at crash, removed at finalization | an empty marker meaning "not yet finalized" |

## Crash report schema

`zenith2.crash-report` version 1 (pretty-printed JSON):

```json
{
	"schema": "zenith2.crash-report",
	"schemaVersion": 1,
	"state": "captured",
	"run": "20261003T101500Z-4242",
	"game": "Zenithmon",
	"buildIdentity": "0x…",
	"compositionHash": "0x…",
	"configuration": "Release",
	"platform": "android",
	"device": { "model": "SM-S928B", "os": "Android 15", "abi": "arm64-v8a", "gpu": "Adreno 750", "driver": "…" },
	"crash": {
		"kind": "signal",
		"signal": "SIGSEGV", "code": "SEGV_MAPERR", "address": "0x0000000000000010",
		"exceptionCode": null,
		"fatalCode": null, "fatalMessage": null,
		"thread": { "id": 31337, "name": "Z2 Worker 03" },
		"uptimeUs": 123456789, "utc": "2026-10-03T10:17:02.123456Z", "frame": 7341
	},
	"modules": [ { "name": "libZenithmon.so", "base": "0x7a00000000", "size": 52428800, "buildId": "…" } ],
	"threads": [ { "id": 31337, "name": "Z2 Worker 03", "frames": [ "0x7a00123456", "0x7a00123000" ] } ],
	"annotations": { "world.primary": "Dawnmere", "frame": "7341", "gpu.backend": "vulkan" },
	"breadcrumbs": [ { "uptimeUs": 120000000, "text": "scene load Dawnmere" } ],
	"attachments": [ { "name": "device-loss", "file": "20261003T101500Z-4242.attach-device-loss.json" } ],
	"files": { "minidump": null, "tombstone": "20261003T101500Z-4242.tombstone.pb", "log": "20261003T101500Z-4242.log.txt" },
	"symbolized": null
}
```

- `state` is `captured` when written by the crash path, `finalized` after finalization, `symbolized` after `ZenithDevTool crash symbolize` adds `symbolized` (an array of resolved frames per thread).
- `crash.kind` is `signal` (Android), `exception` (Windows SEH, with `exceptionCode` such as `0xC0000005`), `fatal` (a `ZENITH_FATAL`, `ZENITH_CHECK`, `ZENITH_ASSERT` or `ZENITH_UNREACHABLE` failure, with `fatalCode` the `Zenith_FatalCode` enumerator name and `fatalMessage`; § "Fatal failures"), `hang` (the watchdog), or `os-exit` (an Android exit record with no pending report, § "Android capture").
- `threads[].frames` are raw return addresses of the crashing thread: on Windows the helper fills them from the minidump's stack walk (`StackWalk64` over the dumped context, in the helper process), and the minidump holds every thread; on Android they come from a frame-record walk in the signal handler (the arm64 ABI's frame records, which the NDK's clang keeps for non-leaf functions), best effort.
- Every string in a report is written by `Zenith_JsonWriter`, so it is escaped, except on the Android signal path, which writes the tail with the fixed-capacity escaper `Zenith_CrashJsonTail` (§ "Signal-safe report tail"), byte-identical to `Zenith_JsonWriter`'s escaping for the same input; a string from an annotation, a breadcrumb, a thread name or a log line is cut to its declared maximum and invalid UTF-8 is replaced by U+FFFD before writing.
- `"truncated": true` is an optional top-level member: present only when the signal-path escaper could not fit the whole tail into the pre-allocated report buffer and closed the document early (§ "Signal-safe report tail"); absent means the report is complete.
- Readers refuse a `schemaVersion` above 1 and skip unknown members.

## Signal-safe report tail

The Android signal handler cannot use `Zenith_JsonWriter`: CORE's writer appends to a `Zenith_Array<char>` whose views "are invalidated by every writing call (it may grow)" (`Zenith2/Docs/Design/CORE/Text.md` → "JSON writer"), that is, it calls the heap, and a crash inside the allocator with the heap lock held would deadlock the handler (no report, no re-raise, no tombstone). The escaped worst case of the tail also exceeds any buffer worth pre-allocating: 64 keys × 32 + 64 values × 128 + 64 breadcrumbs × 96 bytes at 6 bytes per `\u00XX` are 98,304 bytes before the frames, the fatal message and the structure. The signal path therefore writes the tail with its own writer:

```cpp
// Zenith2/Engine/ZenithPlatform/Internal/CrashHandler/Zenith_CrashReport.h (excerpt; defined in Zenith_CrashReport.cpp)
inline constexpr uint32 uCRASH_REPORT_BUFFER_BYTES = 65536;       // the report buffer Create pre-allocates (head + tail)
inline constexpr uint32 uCRASH_JSON_TAIL_MAX_DEPTH = 8;
inline constexpr uint32 uCRASH_JSON_TAIL_CLOSE_RESERVE = 96;      // held back for the truncation member and every closer

// A fixed-capacity, allocation-free, async-signal-safe JSON writer over the pre-allocated report buffer. It continues the
// head Create serialized (an open top-level object at depth 1 holding at least one member) and produces exactly the bytes
// Zenith_JsonWriter's JSON_WRITE_STYLE_PRETTY produces for the same calls and values (an empty container as {} or []).
class Zenith_CrashJsonTail
{
public:
	Zenith_CrashJsonTail(Zenith_MutableByteSpan xBuffer, uint32 uHeadBytes);
	bool Key(Zenith_StringView strKey);          // the next member's key; false once truncated
	bool String(Zenith_StringView strValue);     // cut, U+FFFD-replaced and escaped as below
	bool UInt(uint64 ulValue);
	bool Int(sint64 iValue);
	bool Hex(uint64 ulValue);                   // the string "0x" + 16 lowercase digits
	bool Null();
	bool BeginObject();
	bool EndObject();
	bool BeginArray();
	bool EndArray();
	uint32 Finish();                             // closes the document; returns its length in bytes
	bool IsTruncated() const;
};
```

- Signal safety: no allocation, no lock, no call outside the class's own loops (no `memcpy`, no `snprintf`: integers are formatted by digit loops), no `errno` change, bounded stack (no recursion; the open-container stack is a member array of `uCRASH_JSON_TAIL_MAX_DEPTH` bytes). The buffer is the handler's own, allocated by `Create`.
- Strings: the bytes are taken up to the first NUL or the field's declared bound, each invalid UTF-8 byte is replaced by U+FFFD, and the result is escaped by the string row of `Zenith2/Docs/Design/CORE/Text.md` → "Byte format" (`\"`, `\\`, `\b`, `\f`, `\n`, `\r`, `\t`, `\u00XX` with lowercase hex for every other byte below 0x20 and for 0x7F, every other code point verbatim), so the output is byte-identical to `Zenith_JsonWriter`'s for the same input.
- Capacity: before writing a member (its key and a scalar value) or opening a container, the writer computes the member's exact escaped size in one pass (1, 2 or 6 bytes per input byte, 3 per replaced byte, plus the separators and indentation). When `used + size + uCRASH_JSON_TAIL_CLOSE_RESERVE` exceeds the buffer, it writes nothing of that member, becomes truncated, and every subsequent call returns false without writing. `Finish` closes every open container below the top-level object, appends the member `"truncated": true` as the top-level object's last member, closes it and appends the final LF. The reserve covers the worst case: the closers of depth 8 (`Σ (1 + d)` for d = 1 … 8 = 44 bytes), the truncation member (20 bytes) and the LF, 65 bytes, under the 96 held back.
- Order of the tail, so truncation drops the least useful members first: `crash`, `threads` (the frame walk), `annotations`, `breadcrumbs`, `modules`, `attachments` (empty at crash), `files`, `symbolized` (null). A reader accepts a report with `truncated` true and treats every absent member as empty.
- Buffer size: the report buffer stays 64 KiB (`uCRASH_REPORT_BUFFER_BYTES`, the crash row of `Zenith2/Docs/Design/DEVT/Profiler.md` → "Memory layout and accounting"); a tail that does not fit truncates as above rather than growing the resident buffers. The common report (a few annotations and breadcrumbs of printable text, at most 64 frames) is far below it.
- The Windows path is unaffected: the helper composes its report with its own `Zenith_JsonWriter` in its own process (§ "Windows capture" step 4), where allocation is safe.
- Tests (`Zenith2/Docs/Design/DEVT/Tests.md` → "Crash tests", every host preset): `Test_CrashReport_SignalSafeEscaperMatchesJsonWriter` writes the same sequence of calls through `Zenith_CrashJsonTail` and through `Zenith_JsonWriter` with hostile strings (quotes, backslashes, every control byte, 0x7F, invalid and truncated UTF-8 sequences, strings at and beyond their bound) and asserts byte equality; with a 1,024-byte buffer and the same calls it asserts the document parses with CORE's JSON reader, carries `"truncated": true` and ends with the members written before the cut. `Test_CrashReport_SignalSafeEscaperAllocatesNothing` runs a full tail with a fault-injecting heap that fails every allocation and asserts the heap recorded none.

## Annotations and breadcrumbs

Both structures are written by any thread holding any lock and read by the crash path from an exception filter, a signal handler or (Windows) the helper's `ReadProcessMemory` copy, so every byte they share is atomic storage and every copy is a seqlock read; there is no plain-memory data race for `linux-tsan` (or the C++ memory model) to report.

- Annotations: a fixed table of 64 entries, each `{ std::atomic<uint32> m_uState; std::atomic<uint32> m_uSequence; std::atomic<uint64> m_aulKey[4]; std::atomic<uint64> m_aulValue[16]; }` (168 bytes; key 32 bytes, value 128 bytes, NUL-padded, packed little-endian into the words). `m_uState` is 0 free, 1 claimed (key being written), 3 key written and being resolved (visible to other writers, never to a report), 2 published (the one entry of its key; the only state a report reads).
- `SetAnnotation(key, value)`, so that one key never has two entries even when two threads set the same new key at once:
  1. Find: scan the table for an entry with `m_uState.load(acquire) == 2` whose key words (loaded `relaxed` after that acquire) equal the key; found → step 5 on it.
  2. Claim: `compare_exchange_strong(m_uState, 0 → 1, acq_rel)` on the first free entry `i`; none free → count `crash.annotations_dropped` once per key and return. Write the key words (`relaxed`), then `m_uState.store(3, seq_cst)`.
  3. Resolve: scan every other entry `j` with `m_uState.load(seq_cst)` in {2, 3} (the key words then loaded `relaxed`) and an equal key. When some such `j` is in state 2, or in state 3 with `j < i`, entry `i` yields: `m_uState.store(0, release)` on `i`, then wait for that `j` to reach 2 (spinning with `Zenith_CpuRelax` up to 1,024 iterations, the other writer being between two stores); if it does, step 5 on `j`; if `j` was itself released, or the spin ends, restart at step 1, at most 4 times, after which the write is counted in `crash.annotations_dropped` and dropped. Otherwise (every equal key found is a state-3 entry with a higher index, which will yield) `m_uState.store(2, release)` on `i` and step 5 on `i`. An entry in state 1 has no key yet and is ignored; its writer resolves against `i` when it reaches step 3, which the next step proves.
  4. Why exactly one claimant publishes (the Dekker argument, the same handshake as `Zenith2/Docs/Design/DEVT/Profiler.md` → "Thread lanes"): every claimant stores 3 and only then scans, and both the store and the scan's loads are `seq_cst`, so all of them lie in one total order S. Take two claimants `i` and `j` of one key. If `i`'s load of `j` does not see `j`'s store of 3 (it reads 0 or 1), that load precedes `j`'s store in S, so `i`'s store of 3 precedes `j`'s load of `i`, which therefore reads 3 or a subsequent value of `i` (2, or 0 after `i` yielded to a third entry that `j`'s same scan also sees in state 2 or 3). So of any two claimants at least one sees the other, and the lowest-index rule makes the higher-index one yield; the lowest-index claimant of a key always keeps its entry, so two or more concurrent first writers of one key end with one published entry. Under release/acquire alone both scans may read the other's old state 1 (the store-buffering outcome, which x86-TSO's store buffer produces), and both would publish. On x86 the `seq_cst` store compiles to `xchg` (on arm64 to `stlr`, which a subsequent `ldar` cannot pass), which forbids that StoreLoad reordering. The operations stay lock-free and async-signal-safe; `linux-tsan` cannot see the store-buffering outcome (it is not a data race), so the same-new-key case of `Test_CrashHandler_AnnotationSeqlock` is the evidence, meaningful on x86 because the reordering is real there.
  5. A value write is the seqlock writer: `s = m_uSequence.load(relaxed)`; `m_uSequence.store(s + 1, relaxed)` (odd: writing); `std::atomic_thread_fence(release)`; each value word `store(w, relaxed)`; `m_uSequence.store(s + 2, release)` (even: stable). Concurrent writers of one key are serialised by first taking the odd sequence with `compare_exchange_weak(s even → s + 1, acquire)` and retrying while it is odd, so two writers never interleave.
- The seqlock reader (the crash path and the Windows helper's copy) reads entries whose `m_uState` is 2 alone (key words `relaxed` after the state's `acquire`): `s1 = m_uSequence.load(acquire)`; when `s1` is odd the entry is skipped; each value word `load(relaxed)`; `std::atomic_thread_fence(acquire)`; `s2 = m_uSequence.load(relaxed)`; the copy is used only when `s1 == s2`, so a torn value is never reported. A full table counts `crash.annotations_dropped` once per key.
- Breadcrumbs: a 64-entry ring of `{ std::atomic<uint32> m_uSequence; std::atomic<uint64> m_ulUptimeUs; std::atomic<uint64> m_aulText[12]; }` (96 bytes of text), with a `std::atomic<uint64>` write index claimed by `fetch_add(1, relaxed)`, each entry written and read under the same seqlock protocol; the crash path copies the newest 64 valid entries in index order.
- Memory: the annotations are 64 × 168 = 10,752 bytes and the breadcrumbs 64 × 112 = 7,168 bytes, allocated by `Create`.
- Tests: `Test_CrashHandler_AnnotationSeqlock` and `Test_CrashHandler_BreadcrumbSeqlock` run writers and a reader at 2, 8 and the hardware concurrency threads ([Z2-N-CONTRACT-357]); the reader never observes a value that some writer did not write whole; `Test_CrashHandler_AnnotationSeqlock` also has every thread call `SetAnnotation` on the same new key at once, for 100 fresh keys, and asserts that each key ends with exactly one state-2 entry whose value is one that some thread wrote, and that the yielding claims returned their entries to state 0; both run under `Sanitizers -Preset linux-tsan` through the platform-neutral `Internal/CrashHandler/Zenith_CrashAnnotations.cpp`, which the Linux build compiles although the Linux handler never arms (§ "Linux capture").

## Attachments

- `AddAttachment` registers up to 8 user-mount files or globs. At crash time nothing is read (file I/O is not safe there); at finalization each attachment's newest matching file whose modification time precedes the crash's UTC (plus 10 s, so a device-loss report written while the process went down is included) is copied next to the report as `<stem>.attach-<name>.<ext>` and listed in `attachments`.
- The runtime registers RHI's `{ "device-loss", "user:/crash/device_loss_*.json", true }` (`Zenith2/Docs/Design/RHI/Recovery.md` → "Device-loss report"), so a crash after a device loss carries its `zenith2.device-loss` report; ASSET's asset dump (`user:/devtools/assets/*.json`, when a development build wrote one) and the newest `user:/devtools/frames/*.json` frame dump are registered in non-Shipping builds.
- The GPU fault breadcrumbs themselves are RHI's: they are inside the device-loss report, so the crash report carries them through the attachment rather than reading GPU memory at crash time.

## Crash log mirror

`Zenith_CrashLogMirror` (`Internal/CrashHandler/Zenith_CrashLogMirror.cpp`) is a `Zenith_LogSink` whose `Write` (memory only, on the logging thread) appends `"<uptime ms> <lvl> <cat>: <msg>\n"` into a 64 KiB ring; `Output` and `Flush` do nothing; its `GetMinimumLevel` is `LOG_LEVEL_INFO`. `Write` takes no lock and uses no profiled primitive: it runs on whichever thread logs, which can be inside a profiler hook, so the mirror's two source files are on PROC's `profilerReachable` list (`Zenith2/Docs/Design/DEVT/Profiler.md` → "Lock and wait events"). The crash path copies the ring's committed lines, oldest first, into `<stem>.log.txt` (`write(2)` on Android; the helper's `ReadProcessMemory` of the ring on Windows). This replaces CORE's `Zenith_LogRingSink::CopyRecentLines` on the crash path, because `CopyRecentLines` takes the sink's lock, which the crashing thread may already hold; the on-screen console keeps using `CopyRecentLines`, and CORE's API and tests are unchanged (row DV29 (a) asks CORE to correct `Zenith2/Docs/Design/CORE.md`'s sentence that the ring is "attached to crash reports by DEVT", and Z2T-CORE-048's cross-system line, to name this mirror).

Layout and memory orders. Every shared byte is atomic, every slot has one writer at a time (the writer that won its header), and a copied line is kept only when every one of its slot headers is unchanged across the copy, so concurrent writers, a writer overtaken between its claim and its stores, a reader in a signal handler, and the Windows helper's out-of-process copy never report a torn line:

- Storage: `std::atomic<uint64> m_aulWords[8192]` (65,536 bytes) as 512 slots of 16 words (128 bytes), and `std::atomic<uint64> m_ulSlotCursor`, the number of slots claimed since creation. Slot `s` (a sequence number) lives at words `(s % 512) × 16` to `(s % 512) × 16 + 15`.
- Slot header (word 0 of a slot): bits 0–6 the text bytes in this slot (1 … 120; 0 never appears in a committed header), bit 7 `WRITING` (set while the slot's owner stores its payload; never set in a committed header), bits 8–9 the slot's index within its line (0 … 3), bits 10–11 the line's slot count minus 1 (0 … 3), bits 12–15 zero, bits 16–63 the slot's own sequence number `s` (48 bits, which no run reaches: 2^48 slots of 128 bytes). Words 1 … 15 hold up to 120 text bytes, NUL-padded, packed little-endian. "Sequence `a` is at or after `b`" is computed through the 48-bit distance: `((a − b) mod 2^48) < 2^47`.
- Lines: `"<uptime ms> <lvl> <cat>: <msg>\n"`, cut to 480 bytes (4 slots), so a line occupies `k = ceil(length / 120)` consecutive slots and every slot boundary is known: a reader never has to search for where a line starts.
- Writer (`Write`, any logging thread): `s = m_ulSlotCursor.fetch_add(k, relaxed)`. It then takes each slot `s' ∈ {s + k − 1, …, s + 1, s}` in that order (the continuations first, the first slot last):
  1. `h = header(s').load(acquire)`. Unless `h` is 0 (a slot never written since `Create`), abandon the line when `h`'s sequence is at or after `s'` (the slot was overtaken by a newer claim) or `h` has `WRITING` set (another writer owns the slot).
  2. `compare_exchange_strong(h → { sequence s', WRITING, bytes 0, index, count }, acq_rel, acquire)`; a failure (another writer changed the header since step 1) abandons the line.
  3. `std::atomic_thread_fence(release)`, then the payload words (`relaxed`), then the header `{ sequence s', bytes, index, count }` with `WRITING` clear (`release`). The first slot's store is the line's commit.
  An abandoned line leaves its first slot uncommitted (its header still carries an older sequence, or a newer writer's), so no reader starts a line there; slots it already committed carry a nonzero index and are never read as the start of a line. Each abandonment adds 1 to the mirror's `std::atomic<uint64> m_ulDroppedLines` (`fetch_add`, `relaxed`) and publishes the running total as the CUMULATIVE counter `crash.log_mirror.dropped` through the installed hook table's `m_pfnOnCounter` (`Zenith2/Docs/Design/DEVT.md` → "Counters"). Consequence, stated: a writer preempted while it holds a slot's `WRITING` bit makes every newer line that reaches that slot abandon until it finishes; a writer overtaken before its CAS abandons its own line. Both are counted drops, never a torn line.
- Reader (the Android signal handler and the in-process tests): `end = m_ulSlotCursor.load(acquire)`; for `s` from `max(0, end − 512)` while `s < end`: `h0 = header(s).load(acquire)`; when `h0`'s sequence is not `s`, `WRITING` is set, its byte count is 0 or its index is not 0, the slot is not the committed first slot of a line (it is still being written, abandoned, or it continues a line that began before the window), so `s += 1` and continue; otherwise let `k` be `h0`'s slot count, stop when `s + k > end`, load the headers `h1 … h(k−1)` of slots `s + 1 … s + k − 1` (`relaxed`; each must be committed with its own sequence, `WRITING` clear and its index, else the line is skipped) and every payload word (`relaxed`) into the output buffer; then `std::atomic_thread_fence(acquire)`; re-load every slot header of the line (`relaxed`) and keep the line only when each equals the value read before the copy; `end2 = m_ulSlotCursor.load(relaxed)` stays as a prefilter (`end2 − s ≤ 512`); then `s += k`.
- Why the header re-check detects every overwrite: a payload word of a slot is stored only by a writer that first won that slot's header by its CAS (step 2), which writes a header value with a newer sequence and `WRITING` set; header values never repeat, because a slot's sequence only grows and a writer commits its sequence once. If any word the reader copied was stored by a writer other than the line's own committer, that writer's CAS is sequenced before its release fence and its payload stores, so the fence synchronises with the reader's acquire fence (sequenced after the load that read the store), and the reader's re-load of that slot's header returns the CAS value or a newer one, never the value read before the copy, so the line is dropped. If no copied word came from another writer, the copy is the committed line, which the header's `release` commit made visible whole. The cursor rule alone was blind to an earlier claimant overtaken between its `fetch_add` and its stores (its fence orders only its old claim); the per-slot ownership closes that case, because such a writer either fails the step-1 check or loses the CAS.
- The Windows helper: it reads the cursor with one `ReadProcessMemory` of 8 bytes (`end1`), the 65,536 bytes of words twice (copy A, then copy B), and the cursor again (`end2`), at the addresses the section header records (§ "Crash helper section layout"), each read completing before the next starts. It walks copy A with `end = end1` and keeps a line only when every slot header of the line is committed (own sequence, `WRITING` clear, its index) and identical in copy A and copy B, and the line lies inside the window (`s + k ≤ end1` and `end2 − s ≤ 512`); the payload is copy A's. The parent's other threads may keep logging for the up to 15 s the crashing thread waits, which this rule tolerates: every Windows preset is x64 (the `win` and `win-asan` configure presets use `msvc-x64.cmake`, `Zenith2/Docs/Architecture/BuildSystem.md` → "Preset reference"), and x64 stores become visible in program order, so any word a writer stored into copy A implies that the writer's earlier header CAS is visible to copy B, whose header then differs from copy A's.
- The shared walk is one function template, `Zenith_CrashLogMirrorWalk` (`Internal/CrashHandler/Zenith_CrashLogMirror.h`), instantiated over the live atomic words (in process, with the re-load as the second read) and over a pair of snapshots (the helper and the tests, with copy B as the second read), so both paths apply one rule. The same header declares `Zenith_CrashLogMirrorTestAccess` (tests only, friend of `Zenith_CrashLogMirror`): `ClaimSlots(Zenith_CrashLogMirror&, uint32 uSlots)` performs a writer's `fetch_add` alone and returns `s`, and `WriteClaimed(Zenith_CrashLogMirror&, uint64 ulSequence, Zenith_StringView strLine)` runs the writer's steps 1 to 3 for that claim, so a test can pause a writer between its claim and its stores.
- Output bytes: control bytes other than LF and tab, and each byte of an invalid UTF-8 sequence, are replaced by `?` (one byte for one byte) before the file is written, so the log text never exceeds the 61,440 text bytes the 512 slots hold and fits the helper's 65,536-byte log-text buffer (the "Helper memory" paragraph of § "Crash helper security contract").
- `Test_CrashHandler_LogMirrorLockFree` (the mirror copies lines while another thread holds the log router's sink lock), `Test_CrashHandler_LogMirrorConcurrentWriters` (writers at 2, 8 and the hardware concurrency threads write lines of random lengths 1 … 600 bytes, so lines of one to four slots interleave and some are cut at 480, lapping the ring several times while a reader copies; every copied line is one some writer wrote whole, byte for byte, which each writer makes checkable by writing its thread index, a per-thread sequence number and a checksum into the text; every line a writer abandoned is counted in `m_ulDroppedLines`), `Test_CrashHandler_LogMirrorOvertakenWriterAbandons` (through `Zenith_CrashLogMirrorTestAccess`: `ClaimSlots` takes sequence `s` for a one-slot line, 512 further one-slot lines are written and committed, so line `s + 512` occupies the same physical slot; `WriteClaimed` then resumes the stale writer, which abandons at step 1 and stores nothing; a reader returns line `s + 512` intact and `m_ulDroppedLines` grew by 1) and `Test_CrashHandler_LogMirrorSnapshotRule` (a snapshot pair whose second cursor value is 600 slots past the first: every line starting before `end2 − 512` is dropped and every subsequent committed line kept; a line whose header differs between copy A and copy B is dropped; a snapshot holding a header with a stale sequence or `WRITING` set is skipped) run in the `Test` step and under `Sanitizers -Preset linux-tsan`.

## Fatal failures

CORE's failure procedure has no fatal hook and DEVT asks for none; the handler recognises a fatal from two things CORE already does (`Zenith2/Docs/Design/CORE/Diagnostics.md` → "Failure procedure"):

1. Step 5: "when a log-record hook is installed, emit a `LOG_LEVEL_FATAL` record in category `core.diagnostics` carrying the first line as its message and the fatal code as field `code`". The crash log mirror is a sink, so its `Write` sees that record on the failing thread before termination; when the record's level is `LOG_LEVEL_FATAL`, its category `core.diagnostics` and it carries an integer field `code`, `Write` also fills the handler's fatal slot `{ std::atomic<uint32> m_uSequence; std::atomic<uint32> m_uCode; std::atomic<uint64> m_ulThreadID; std::atomic<uint64> m_aulMessage[32]; }` (offsets 0, 4, 8 and 16; 256 bytes of message; 272 bytes in all, `static_assert(sizeof(Zenith_CrashFatalSlot) == 272)`) under the seqlock protocol of § "Annotations and breadcrumbs".
2. Step 7, `Zenith_TerminateProcess(eCode)`: on Windows `RaiseException(0xE05A0000 | eCode, EXCEPTION_NONCONTINUABLE, 0, nullptr)`, which reaches the handler's unhandled-exception filter; on Android `__builtin_trap()`, which raises `SIGTRAP` or `SIGILL` into the signal handler.

Decoding in the crash path:

| Platform | Condition | Report |
|---|---|---|
| Windows | `(ExceptionCode & 0xFFFF0000) == 0xE05A0000` | `crash.kind = "fatal"`, `fatalCode` = the enumerator name of `ExceptionCode & 0xFF` (`Zenith_FatalCode` is a `uint8`), from a constexpr name table; `fatalMessage` from the fatal slot when its thread ID equals the faulting thread's and its seqlock read is valid, else null |
| Android | `SIGTRAP` or `SIGILL`, and the fatal slot is valid with the crashing thread's ID | `crash.kind = "fatal"`, `fatalCode` from the slot's code (the same table), `fatalMessage` from the slot |
| Android | `SIGTRAP` or `SIGILL` without a matching slot (a trap before the log hook was installed, or a real illegal instruction) | `crash.kind = "signal"` |

The name table is a constexpr array indexed by the code, so the lookup is async-signal-safe. `Test_CrashFatal_DecodeExceptionCode` (every enumerator's code decodes to its name; a code outside `0xE05A00xx` is not fatal) and `Test_CrashFatal_SlotFilledByFatalRecord` (feeding the mirror a `LOG_LEVEL_FATAL` `core.diagnostics` record with field `code` fills the slot; a record of another category or level does not) run on every host preset; `Test_CrashDeath_ZenithFatal` (Windows) proves the end-to-end path.

## Windows capture

The custom realisation (the Crashpad realisation is § "Crashpad realisation"):

1. `Create` creates the helper's shared objects, all non-inheritable (null security attributes): an unnamed pagefile-backed section of 64 KiB (`CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, 65536, nullptr)`; no name, so no other process can open it), two unnamed auto-reset events (crash, done) and one manual-reset event (exit) (`CreateEventW(nullptr, …, nullptr)`). It draws a 128-bit nonce from `BCryptGenRandom` and writes the section header (§ "Crash helper section layout"; validated as § "Crash helper security contract" states). It then launches the helper in this order:
   1. `DuplicateHandle(GetCurrentProcess(), GetCurrentProcess(), GetCurrentProcess(), &hParent, PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | SYNCHRONIZE, TRUE, 0)`: the handle to its own process, inheritable, with exactly the rights the helper uses: `PROCESS_QUERY_INFORMATION` and `PROCESS_VM_READ` for `MiniDumpWriteDump` with the dump type of § "Crash report files" (which has no `MiniDumpWithHandleData`, the one type that would need `PROCESS_DUP_HANDLE`), for `GetProcessId` and for `ReadProcessMemory`, and `SYNCHRONIZE` for its wait. It never carries `PROCESS_DUP_HANDLE`, which would let the helper duplicate any handle of the game, and so amount to full access.
   2. `DuplicateHandle(GetCurrentProcess(), hSection, GetCurrentProcess(), &hSectionInheritable, 0, TRUE, DUPLICATE_SAME_ACCESS)`, and the same for the three events: inheritable copies of the four objects. The originals stay non-inheritable.
   3. `InitializeProcThreadAttributeList` with one attribute and `UpdateProcThreadAttribute(…, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, …)` naming exactly the five inheritable handles; then `CreateProcessW(m_strExecutablePath, <command line>, nullptr, nullptr, TRUE, CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr, &xStartupInfoEx.StartupInfo, &xProcessInformation)` with `bInheritHandles = TRUE`, which the handle list restricts to those five. The command line is `<exe> --zenith-crash-helper <section> <crash> <done> <exit> <process> <nonce>`, the values of the inheritable copies (a child inherits each handle at the same value) and the nonce in lower-case hexadecimal.
   4. Immediately after `CreateProcessW` returns, whether it succeeded or not: `CloseHandle` on the five inheritable copies, `DeleteProcThreadAttributeList`, and `CloseHandle` on the helper's thread handle. The game keeps its non-inheritable originals and the helper's process handle (to stop the helper at shutdown).
   5. Wait up to 5 s for the helper's ready flag in the section; `CRASH_HANDLER_ERROR_HELPER_START` otherwise (the helper is terminated and every object closed).

   The inheritable copies therefore exist only between steps 2 and 4 of one call, on the main thread at startup step 7, before the composition root starts any other child process; and any other Zenith2 code that starts a child with `bInheritHandles = TRUE` passes its own `PROC_THREAD_ATTRIBUTE_HANDLE_LIST` (the rule X245 (d) records with the helper's trust rules), so the copies cannot leak into another child such as RenderDoc or the editor's cook server.
2. The helper (`Zenith_RunCrashHelperIfRequested`, `Internal/Windows/Zenith_CrashHelper_Windows.cpp`) validates its arguments and the section (§ "Crash helper security contract"), sets the ready flag, and waits on the crash event, the exit event and the inherited process handle.
3. In the game, `SetUnhandledExceptionFilter`, `_set_invalid_parameter_handler`, `_set_purecall_handler` and `signal(SIGABRT)` all route to the trampoline of § "Crash handler anchor", installed last by `Create` with each previous value saved. No vectored exception handler is installed: a vectored continue handler runs when an exception is continued, that is after some handler returned `EXCEPTION_CONTINUE_EXECUTION`, so routing it to the capture would turn handled exceptions into crash reports and terminations. The trampoline claims the handler (a second crash on any thread finds the capture claimed, waits up to 30 s for the capture to end the process and only then terminates with its own code, so it never writes a second report, § "Crash handler anchor" step 3) and, with the claimed handler: (a) stores the crashing thread's ID in the member `m_ulCaptureThreadID`; (b) copies the `EXCEPTION_POINTERS` address, the exception record and context and the thread ID into the section's crash record; (c) copies into the section's fixed fields the crash tail (code, address, uptime, frame index, the thread's registered name) and a seqlock copy of every annotation, breadcrumb and the fatal slot (§ "Crash helper section layout"); (d) sets the crash event and waits up to 15 s on the done event; (e) calls `TerminateProcess(GetCurrentProcess(), uExitCode)`, where `uExitCode` is defined by the entry point that reached the trampoline:

   | Entry point | `uExitCode` |
   |---|---|
   | the unhandled-exception filter (an SEH exception) | the exception's `ExceptionRecord->ExceptionCode`, e.g. `0xC0000005` for an access violation, `0xC00000FD` for a stack overflow |
   | the filter with a fatal (`(ExceptionCode & 0xFFFF0000) == 0xE05A0000`, § "Fatal failures") | `0xE05A0000 \| code`, CORE's fallback code of `Zenith_TerminateProcess` (`Zenith2/Docs/Design/CORE/Diagnostics.md` → "Failure procedure"), which is that exception's code |
   | `signal(SIGABRT)` (`abort()`) | 3, the CRT's own exit code for `abort()` |
   | `_set_invalid_parameter_handler` | `0xC0000417` (`STATUS_INVALID_CRUNTIME_PARAMETER`, the code the CRT's own invalid-parameter termination reports) |
   | `_set_purecall_handler` | 3, as `abort()` |

   A second crasher that reaches its 30-s bound terminates with the code of its own entry point by the same table. The crash tests assert these codes (`Zenith2/Docs/Design/DEVT/Tests.md` → "Crash tests").
4. The helper, on the crash event: re-validates the section header, writes `<stem>.dmp` with `MiniDumpWriteDump(parentProcess, parentPid, file, type, &exceptionInfo with ClientPointers = TRUE, …)` (the exception pointers are the parent's addresses, read by DbgHelp from the parent), copies the log mirror with the four `ReadProcessMemory` reads of § "Crash log mirror" (the cursor, exactly `uCRASH_LOG_MIRROR_BYTES` of words twice, the cursor again, from the addresses recorded in the header, validated as below), and composes `<stem>.log.txt`, `<stem>.report.json` (with its own `Zenith_JsonWriter`, from the section's fixed fields, `files.minidump` filled) and `<stem>.pending`; then sets the done event and exits 0. On the exit event or the parent's process handle signalling without a crash, it exits 0.
5. Fatals (`ZENITH_FATAL` and every failure kind) take the same path through CORE's `RaiseException(0xE05A0000 | code)` (§ "Fatal failures").
6. Writing the dump out of process avoids running `DbgHelp` in a corrupted process; the stack guarantee (`SetThreadStackGuarantee(64 KiB)` per registered thread) leaves the filter room to run on a stack overflow.
7. Not captured by this realisation: a `__fastfail` termination, which `/GS` cookie failures and Control Flow Guard violations raise (`/guard:cf` is on in Release and Shipping, [Z2-N-ARCH-259]), ends the process without running any in-process handler, so the custom writer produces no report for it. D-SEL-CRASH input 1 measures both candidates on a `__fastfail(FAST_FAIL_INVALID_ARG)` case and on a CFG violation (an indirect call through a pointer to a non-target address in a `/guard:cf` build), and the harness report records the custom writer's missing reports as a measured coverage deficiency against the "Windows and Android coverage" criterion (§ "D-SEL-CRASH candidates and acceptance"); whether Crashpad reports them (through a Windows Error Reporting runtime exception module, which needs a module entry the installer registers) is that candidate's measured result.

## Crash helper security contract

The helper mode is an entry point present in every configuration, Shipping included, that holds a handle to another process with `PROCESS_VM_READ`; its trust rules (recorded for `Zenith2/Docs/Architecture.md` → "Security architecture" by X245 (d) and row DV11):

| Threat | Rule |
|---|---|
| pointing the helper at an arbitrary process, or PID reuse | the helper takes no PID: it uses only the process handle the parent passed by inheritance, which carries `PROCESS_QUERY_INFORMATION \| PROCESS_VM_READ \| SYNCHRONIZE` alone (never `PROCESS_DUP_HANDLE`, § "Windows capture" step 1), and checks that the handle is valid (`GetHandleInformation`), that `GetProcessId(handle)` equals the section's `m_uParentPid`, and that its own parent process (`NtQueryInformationProcess` `ProcessBasicInformation`'s `InheritedFromUniqueProcessId`) has that PID |
| a forged or foreign section | the section is unnamed, so it reaches the helper only by inheritance; the helper checks the handle maps (`MapViewOfFile`) a region of at least 65,536 bytes (`VirtualQuery`), the header's magic `ZCRH` and version 1, that the nonce in the header equals the nonce on the command line (a constant-time 16-byte compare), and that `m_uParentPid` matches as above |
| handle inheritance leaking into another child | the game's section and events are created non-inheritable; inheritable duplicates exist only for the one `CreateProcessW` call that names exactly those five in `PROC_THREAD_ATTRIBUTE_HANDLE_LIST`, and are closed as soon as it returns (§ "Windows capture" step 1) |
| hostile arguments | the option must be argv[1] and followed by exactly six arguments; each handle value must be a 1 … 16 digit hexadecimal number and the nonce exactly 32 hexadecimal digits; anything else exits 3 |
| paths from the section | none is read: the helper derives the crash directory itself, `SDL_GetPrefPath("Zenith2", m_strGame)` + `crash\` from its compile-time `Zenith_CrashHelperIdentity` (the same executable's), and composes the stem `<utc>-<pid>` from the section's numeric run-start time (accepted only within 2020-01-01 … the helper's own clock + 1 day) and the validated parent PID; so a test that crashes a child process finds that child's files by the PID in their stem inside this one directory, never by a directory it passes (`Zenith2/Docs/Design/DEVT/Tests.md` → "Crash tests") |
| strings from the section | every text field is fixed-size in the section (thread name 64 bytes, annotation keys 32 and values 128, breadcrumbs 96, fatal message 256, device strings 64 each); the helper reads each with its declared bound, stops at the first NUL or the bound, replaces invalid UTF-8 and control bytes, and writes it only through `Zenith_JsonWriter`'s escaping; it never copies JSON text from the section |
| addresses from the section | the log mirror address and the exception-pointer address are used only as `ReadProcessMemory` and `MiniDumpWriteDump` arguments against the validated parent handle; a failed read yields an empty log file and a report with `files.log` null, never an out-of-bounds access in the helper |
| the helper as a gadget for another program | a process that starts the executable with the option but without the inherited handles fails validation (exit 3, one diagnostic line, nothing written); the helper opens no socket, takes no input after start other than the events, and writes only into its own user-mount crash directory |

Helper memory. The helper runs inside `main` after `Zenith_InstallRootHeap(&xHeap)` ([Z2-N-CONTRACT-142]; the dispatch is the next statement), so it allocates from the root heap like any engine code, and it allocates only once, after its arguments validated: a 131,072-byte buffer for the mirror's two word copies (§ "Crash log mirror"), a `Zenith_Array<char>` reserved to 65,536 bytes for the report's `Zenith_JsonWriter`, a 65,536-byte buffer for the log text, and a 1,024-byte path buffer, into which it copies `SDL_GetPrefPath`'s result before releasing that string with `SDL_free`. It frees all four before returning, so the heap's leak check at `main`'s end passes. It creates no log router and installs no hook table: its one diagnostic line goes through `Zenith_WriteDiagnosticLine`.

Every helper test is a case of `Engine/ZenithPlatform/Tests/` (no `Tests/Windows/` directory, [Z2-N-ARCH-162]) that reaches the Windows realisation through `Zenith_CrashHandlerTestAccess` (declared in `Internal/CrashHandler/Zenith_CrashHandlerState.h`; bodies in `Internal/Windows/Zenith_CrashHandlerTestAccess_Windows.cpp`, and in the Android and Linux files returning "unsupported") and selects itself at run time with `if (!Zenith_CrashHandlerTestAccess::IsWindowsRealisation()) { MESSAGE("…"); return; }`. `Test_CrashHelper_ForgedSectionRefused` calls `Zenith_CrashHandlerTestAccess::ValidateHelper` (the helper's `Zenith_CrashHelperValidate`) with a section of the wrong magic, a wrong version, a nonce mismatch, a PID mismatch, a short region and malformed arguments, and asserts each is refused with nothing written; `Test_CrashHelper_UnInheritedLaunchExits3` calls `Zenith_RunCrashHelperIfRequested` in process with the option and fabricated handle values and asserts that it returns true with exit code 3 and writes no file into the crash directory; `Test_CrashHelper_SectionStringsBoundedAndEscaped` fills every section text field with unterminated bytes, quotes and invalid UTF-8 and asserts the written report parses and every value is within its bound; `Test_CrashHandler_HelperStartsAndReportsReady` asserts that `Create` returns success with the helper's ready flag set within 5 s, that `Zenith_CrashHandlerTestAccess::AreSharedObjectsInheritable` reports false for the section and the three events once `Create` has returned (`GetHandleInformation`'s `HANDLE_FLAG_INHERIT`), and that the launch description `Zenith_CrashHelperBuildLaunch`, read through `Zenith_CrashHandlerTestAccess::GetHelperLaunch`, names exactly five handles and the parent-process access mask `PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | SYNCHRONIZE`.

## Crash helper section layout

The 64 KiB unnamed section is one cross-process format, read by the helper (a second instance of the same executable) and written by the game; both sides include `Zenith_CrashHelperSection` from `Internal/Windows/Zenith_CrashHelper_Windows.h`. Every field is plain bytes except the ready flag: the header is written by `Create` before the helper starts, the crash record and the fixed fields by the capturing thread before it sets the crash event, and the helper reads them only after its wait on that event returns, so the event orders them across the processes; the ready flag is the one field both processes touch concurrently (the helper stores it while `Create` polls it), so it is a `std::atomic<uint32>`, which is lock-free and therefore address-free across the two mappings. Offsets in bytes, little-endian:

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | `uint32 m_uMagic`: `5A 43 52 48` ("ZCRH") |
| 4 | 4 | `uint32 m_uVersion`: 1 |
| 8 | 16 | `uint8 m_auNonce[16]`: the 128-bit nonce, also passed on the command line |
| 24 | 4 | `uint32 m_uParentPid` |
| 28 | 4 | `std::atomic<uint32> m_uReady`: 0 until the helper validated the section, then 1 (`store(1, release)`; `Create` polls with `load(acquire)` for up to 5 s) |
| 32 | 8 | `uint64 m_ulRunStartUtcUs`: the run stem's time |
| 40 | 8 | `uint64 m_ulCompositionHash` |
| 48 | 8 | `uint64 m_ulLogMirrorWordsAddress`: the parent's address of the mirror's `m_aulWords` |
| 56 | 8 | `uint64 m_ulLogMirrorCursorAddress`: the parent's address of `m_ulSlotCursor` |
| 64 | 8 | `uint64 m_ulExceptionPointersAddress`: the parent's `EXCEPTION_POINTERS*`, for `MiniDumpWriteDump` with `ClientPointers = TRUE` |
| 72 | 4 | `uint32 m_uThreadID`: the crashing thread |
| 76 | 4 | `uint32 m_uEntryPoint`: the trampoline's entry point (1 exception filter, 2 `SIGABRT`, 3 invalid parameter, 4 pure call) |
| 80 | 152 | `EXCEPTION_RECORD m_xExceptionRecord` (x64 layout) |
| 232 | 8 | reserved, zero |
| 240 | 1,232 | `CONTEXT m_xContext` (x64 layout, 16-byte aligned) |
| 1,472 | 40 | the crash tail: `uint64 m_ulCode` (the exception code), `m_ulAddress` (the fault address), `m_ulUptimeUs`, `m_ulUtcUs`, `m_ulFrameIndex` |
| 1,512 | 64 | `char m_acThreadName[64]`: the registered name, NUL-padded |
| 1,576 | 320 | `char m_aacDevice[5][64]`: model, OS version, ABI, GPU, driver, NUL-padded |
| 1,896 | 24 | reserved, zero |
| 1,920 | 10,240 | the annotation copies: 64 × { `char m_acKey[32]`; `char m_acValue[128]` }, the seqlock reads of § "Annotations and breadcrumbs" (an entry with an empty key is unused) |
| 12,160 | 6,656 | the breadcrumb copies: 64 × { `uint64 m_ulUptimeUs`; `char m_acText[96]` }, oldest first |
| 18,816 | 8 | `uint32 m_uAnnotationCount`, `uint32 m_uBreadcrumbCount` |
| 18,824 | 272 | the fatal copy: `uint32 m_uCode`, `uint32 m_uValid` (1 when the seqlock read of the fatal slot succeeded), `uint64 m_ulThreadID`, `char m_acMessage[256]` |
| 19,096 | 46,440 | reserved, zero (the section is 65,536 bytes) |

`Zenith_CrashHelper_Windows.h` asserts the layout: `static_assert(sizeof(Zenith_CrashHelperSection) == 65536)`, `static_assert(offsetof(Zenith_CrashHelperSection, m_xContext) % 16 == 0)`, one `static_assert(offsetof(…) == <offset>)` per row above, and `static_assert(std::atomic<uint32>::is_always_lock_free)`. The helper validates the header (§ "Crash helper security contract") before reading any other field, and reads every text field with its bound. `Test_CrashHelper_SectionStringsBoundedAndEscaped` fills the text fields of this layout.

## Android capture

1. `Create` installs `sigaction` handlers for the seven signals with `SA_SIGINFO | SA_ONSTACK`, keeping the previous actions, as its last step (§ "Crash handler lifecycle"); every handler is the trampoline of § "Crash handler anchor"; every registered thread has a 64 KiB alternate signal stack (`sigaltstack`, allocated from the handler's own block at registration).
2. The trampoline (async-signal-safe code only) claims the handler through the anchor (a second signal on another thread finds the capture claimed, sleeps in 1-s `nanosleep` steps for at most 30 s, then restores the default action and re-raises with `tgkill`), stores the crashing thread's ID in `m_ulCaptureThreadID`, and, with the claimed handler, gathers the report tail's values: `siginfo_t` (signal, code, fault address), the thread ID (`gettid`) and name (from the registration table), uptime (`clock_gettime(CLOCK_MONOTONIC)`), the frame index (an atomic the runtime stores each frame), the fatal decode (§ "Fatal failures"), the annotations and breadcrumbs (§ "Annotations and breadcrumbs") and a frame-pointer walk of the crashing thread from the `ucontext_t` (at most 64 frames, each return address validated against the module ranges recorded at `Create` from `dl_iterate_phdr`; a frame record is dereferenced only when its address lies inside the crashing thread's stack bounds recorded at registration (§ "Thread registration"), is 16-byte aligned and is strictly above the previous record, so a corrupt frame pointer ends the walk instead of faulting inside the handler; a thread that is not registered has no recorded bounds and reports only the leaf frame, the PC and the link register from the `ucontext_t`); serializes the tail with `Zenith_CrashJsonTail` into the pre-allocated report buffer after the head (§ "Signal-safe report tail": no allocation, a `"truncated": true` member when it does not fit); writes `<stem>.report.json`, `<stem>.log.txt` and `<stem>.pending` with `open`/`write`/`close` into the pre-opened `crash/` directory file descriptor (`openat`); restores the previous action and re-raises the signal with `tgkill`, so Android's debuggerd still produces its tombstone out of process and the process dies with the original signal.
3. Next launch: `Finalize` asks for the previous process's exit records through PLAT's JNI bridge: `ActivityManager.getHistoricalProcessExitReasons(packageName, 0, 4)` (API 30+); <!-- assumption: pending PLAT --> PLAT's Android bridge exposes `Zenith_AndroidGetExitRecords(Zenith_Span<Zenith_AndroidExitRecord>, Zenith_Arena&)` returning reason, timestamp, PSS, description and the trace bytes (row DV13 (e)). <!-- /assumption --> The Android file `Internal/Android/Zenith_CrashExitInfo_Android.cpp` converts each into the platform-neutral `Zenith_CrashExitRecord { uint32 m_uReason; uint64 m_ulUtcMs; uint64 m_ulPssKb; Zenith_StringView m_strDescription; Zenith_ByteSpan m_xTrace; }` (`Internal/CrashHandler/Zenith_CrashFinalize.h`) and calls `Zenith_CrashMapExitRecords(Zenith_Span<const Zenith_CrashExitRecord>, Zenith_Span<const Zenith_CrashPendingReport>, Zenith_Span<Zenith_CrashExitDecision>)`, the mapping: a `REASON_CRASH_NATIVE` record within 10 s of a pending report's UTC attaches its trace bytes (the tombstone protobuf of `getTraceInputStream()`, API 31+, which the S24 has) to that report as `<stem>.tombstone.pb`; a `REASON_ANR`, `REASON_LOW_MEMORY` or `REASON_SIGNALED` record with no pending report within 10 s becomes a synthetic report with `crash.kind = "os-exit"`, the reason name, the description and the PSS; any other record is ignored. `Test_CrashFinalize_ExitRecordMapping` drives the mapping with fake records (each reason, inside and outside the 10 s window, with and without a pending report) on every host preset.
4. The `INTERNET` permission and every development service are irrelevant here: capture writes only into the app's own files.

## Linux capture

Out of scope. Linux is the test host only, and its one preset is `linux-tsan` (configuration `Sanitize`, `Zenith2/Docs/Architecture/BuildSystem.md` → "The linux-tsan host"), where `crash.enabled` is forced off (§ "Sanitizer builds"), so a Linux capture path would run nowhere. `Internal/Linux/Zenith_CrashHandler_Linux.cpp` therefore implements `Create` as returning `CRASH_HANDLER_ERROR_DISABLED` when `m_bEnabled` is false and `CRASH_HANDLER_ERROR_UNSUPPORTED` otherwise, installs nothing, and `Zenith_RunCrashHelperIfRequested` returns false. What does run on Linux is every platform-neutral part — the annotation and breadcrumb seqlocks, the log mirror, the fatal decode table, the report writer and parser, the finalization mapping and retention — whose tests are in § "Annotations and breadcrumbs", § "Crash log mirror", § "Fatal failures" and § "Next-launch finalization" and run under `Sanitizers -Preset linux-tsan`.

## Sanitizer builds

`crash.enabled` defaults to false in `win-asan`, `android-hwasan` and `linux-tsan` builds (`Zenith_IsSanitizerBuild()`), and its layer mask admits the engine default alone there, because the sanitizers install their own handlers for the same signals and exceptions and their reports are the evidence (the `z2-sanitizer-defect` suites of [Z2-N-CONTRACT-413], `Zenith2/Docs/Contracts.md` → "Tests and code hygiene gates"). `Create` with `m_bEnabled` false returns `CRASH_HANDLER_ERROR_DISABLED` and installs nothing; the runtime passes a null handler to the watchdog and the job system. The crash cases (`Test_CrashDeath_*`) are ordinary cases that crash a child process (`Zenith2/Docs/Design/DEVT/Tests.md` → "Crash tests"; no death-test suite, because `z2-death-check` passes only on CORE's failure text, which a real fault, `abort()` or a second crash never prints); they run in every `win` configuration, `ShippingTest` included (`win-shipping-test`, once the `Test` step runs that preset, row DV17 (f)), and in a sanitizer configuration each case asserts in process that `Create` with the build's `crash.enabled` default returns `CRASH_HANDLER_ERROR_DISABLED`, launches no child and returns; Android capture is proven by the Stage 3 S24 device run (`crash.test access --confirm`, `Zenith2/Docs/Design/DEVT/Tests.md` → "Device and acceptance runs") and by the host-run mapping, decode and report tests above.

## Hang reports

`WriteHangReport(threadID, reason, scriptStack)` is called by SCRIPT's watchdog thread when a VM exceeds twice its limit (`Zenith2/Docs/Design/SCRIPT.md` → "Failure handling" states the outcome), before it calls `ZENITH_FATAL`:

```text
zenith2.hang-report 1
run: 20261003T101500Z-4242
utc: 2026-10-03T10:17:02.123456Z
uptimeUs: 123456789
frame: 7341
thread: 31337 Z2 Worker 03
reason: script VM 'world:Dawnmere' exceeded 2x the 8 ms limit (17.2 ms)
script stack:
  <the text the watchdog passes, one frame per line>
annotations:
  world.primary = Dawnmere
breadcrumbs:
  120000000 scene load Dawnmere
```

- It writes `<stem>.hang.txt` with ordinary file calls (the process is not yet corrupted) and sets an annotation `hang = <stem>.hang.txt`; the following `ZENITH_FATAL` then produces the crash report with `crash.kind = "fatal"` and the hang file listed under `files`.
- Without a crash handler (sanitizer builds, tests) SCRIPT logs the same text and calls `ZENITH_FATAL` (SCRIPT's own fallback); the watchdog receives the handler through `Zenith_ScriptWatchdogDesc::m_pxCrashHandler` (row DV14; the field is `Zenith2/Docs/Scripting/PublicTypes.md`'s, requested as X248).

## Thread registration

- `RegisterCurrentThread(name)`: stores the thread ID, the name and the thread's stack bounds in the 64-entry registration table (lock-free claim, atomic words as the annotations; each entry is 64 bytes, the 48 bytes of ID, name and claim state plus `std::atomic<uint64> m_ulStackLow` and `m_ulStackHigh`), takes one of the alternate signal stacks and installs it (Android) or calls `SetThreadStackGuarantee` (Windows); `UnregisterCurrentThread` reverses it. The stack bounds come from `pthread_getattr_np` and `pthread_attr_getstack` at registration, outside any signal handler (Android; on Windows the helper walks stacks from the minidump and the fields stay 0); the frame walk of § "Android capture" step 2 reads them. Unregistered threads still crash into the handler; they lack a name, stack bounds and, on Android, an alternate stack (a stack overflow on such a thread terminates without a report).
- Android alternate stacks: `Create` allocates 32 stacks of 64 KiB (the crash row of `Zenith2/Docs/Design/DEVT/Profiler.md` → "Memory layout and accounting"), while the table takes 64 threads. A registration beyond the 32nd stack is recorded (ID, name, bounds) without an alternate stack, counted in the CUMULATIVE counter `crash.altstacks_refused` and logged once per run (`crash: alternate signal stacks exhausted (32); thread <name> registered without one`); the stack-overflow caveat of unregistered threads then applies to it, and every other crash on it is captured normally. The Player's registered threads on the S24 (the main thread, the JOBS workers and the render-submit thread for its eight cores, and the owners' threads of the third bullet below) are expected to stay well under 32; the counter shows a configuration where they do not.
- JOBS: the composition root passes `m_pfnThreadLifecycle = &Zenith_CrashHandler::JobThreadLifecycle` and `m_pThreadLifecycleUserData = pxCrashHandler` in `Zenith_JobSystemDesc` (`Zenith2/Docs/Design/JOBS.md` → "Public API" declares `Zenith_PfnJobThreadLifecycle`); the thunk calls `RegisterCurrentThread(szThreadName)` when `bStarted` and `UnregisterCurrentThread` otherwise. With a null handler the composition root passes a null callback.
- Other owned threads (`Zenith.ProfFlush`, the log router's drain thread, PLAT's I/O and watcher threads, SCRIPT's watchdog, audio threads) are registered by the composition root through their owners' lifecycle callbacks where they offer one; a thread without one is an unregistered thread, which the rule above covers.

## Next-launch finalization

`Finalize()` at startup step 7, on the main thread, with ordinary file I/O:

1. List `user:/crash/*.pending`. For each: parse `<stem>.report.json` (a report that fails CORE's JSON reader, or whose schema is not `zenith2.crash-report` version 1, is renamed `<stem>.corrupt` and skipped); apply the Android exit-record mapping (§ "Android capture"); copy the attachments (§ "Attachments"); set `state` to `finalized`; rewrite the report atomically; remove the marker. Synthetic `os-exit` reports are written here too.
2. Retention: keep the newest `crash.maxReports` reports by UTC; delete the files of older reports (every `<stem>.*`).
3. Log one line per finalized report, `crash: previous run <stem> crashed (<kind> <signal-or-code>) — report <file>`, and publish `crash.reports_pending` (finalized reports since the last `crash.list` or remote fetch).
4. Finalization never blocks startup for more than 200 ms: the work above is bounded by at most 16 pending reports and at most 64 MiB of attachments; anything beyond is left pending for the next launch.

The parser is fuzzed (`Zenith_Fuzz_CrashReport`, `Zenith2/Engine/ZenithPlatform/Tests/Fuzz/CrashReport.cpp`); `Test_CrashHandler_FinalizeAttachesAndRetains` and `Test_CrashHandler_FinalizeCorruptRenamed` run on every host preset.

## Symbolization

- `ZenithDevTool crash symbolize --report <file> --symbols <dir> [--out <file>]`: for each frame address in `threads[].frames`, finds its module from `modules[]`, subtracts the base, and resolves the offset with the pinned `llvm-symbolizer` (the manifest's LLVM toolchain, located with SHIP's `Get-Zenith2ToolPath -Tool llvm-symbolizer`, `Zenith2/Docs/Design/SHIP/Release.md` → "Symbolization tools") against the module's symbol file in `--symbols` (Windows: `<Game>.pdb`, matched by the PE's CodeView GUID and age; Android: the unstripped `lib<Game>.so`, matched by its GNU build ID); writes `symbolized` and sets `state` to `symbolized`.
- The Windows minidump opens directly in Visual Studio or WinDbg with the PDB. The Android tombstone is kept unmodified for the platform's own tooling; DEVT decodes nothing from it (no protobuf parser, so no untrusted-input decoder or fuzz target beyond the report parser), and the tool symbolizes the report's frame-pointer walk.
- Symbol files come from the build tree (`out/…/<preset>/<Config>/`) for development builds and from SHIP's symbol store (`Zenith2/Docs/Design/SHIP/Release.md` → "Symbol store layout") for releases; a build-ID mismatch is reported per module and those frames stay raw.
- `ZenithDevTool crash list [--dir <path>]` prints the reports of a directory (a fetched device copy) with kind, game, build and state.

## D-SEL-CRASH candidates and acceptance

| | Crashpad | Custom minidump writer |
|---|---|---|
| Windows capture | Crashpad's handler process (`crashpad_handler.exe` shipped beside the game), out of process, minidump into a local database | § "Windows capture": the game executable relaunched as its own helper, `MiniDumpWriteDump` out of process |
| Android capture | Crashpad's in-process client with its handler started as a separate executable in the APK's `lib/arm64-v8a/` (`libcrashpad_handler.so`), out of process, minidump | § "Android capture": an in-process signal handler writes the report, and debuggerd's `crash_dump` writes the tombstone out of process; no minidump |
| upload | disabled (no URL, database only) | none |
| third party | Crashpad and mini_chromium, owned by ZenithPlatform (`Zenith2/Docs/Architecture.md` → "Third-party dependencies", table "Dependencies admitted by open selections") | none |

The acceptance rule is the core documents', verbatim, and DEVT adds no rule of its own:

- `Zenith2/Docs/Architecture.md` → "Named selection decisions": criteria "Windows and Android coverage, symbol-server integration, size, maintenance"; rule "**Crashpad unless it cannot meet the 16 KB page and minSdk 34 constraints or the package budget**".
- `Zenith2/Docs/DecisionLog.md` → D-SEL-CRASH: "Out-of-process capture on Windows and Android, minidumps symbolised against the retained symbols, no exceptions in engine code, and no crash handler reachable as a remote tool in shipping builds".

The harness `Zenith2/Benchmarks/CrashPrototype/` measures, for each candidate on the reference desktop and the S24 in Release builds, these inputs to that rule:

| # | Input | Feeds |
|---|---|---|
| 1 | a report (minidump, or for the custom Android path the report plus tombstone) for each of null-pointer write, stack overflow, `abort()`, `ZENITH_FATAL`, a crash on a JOBS worker, and on Windows a `__fastfail(FAST_FAIL_INVALID_ARG)` and a Control Flow Guard violation in a `/guard:cf` build (§ "Windows capture" step 7: the custom writer is expected to miss both, recorded as a measured deficiency) | Windows and Android coverage |
| 2 | out-of-process capture on each platform, and whether the artefact is a minidump | DecisionLog's first two clauses |
| 3 | symbolized top frames naming the crashing function in every case, using only SHIP's retained symbols; and whether the candidate's artefacts are consumable from SHIP's symbol store by the standard tools (WinDbg and Visual Studio symbol paths on Windows, `llvm-symbolizer` by build ID on Android) without a DEVT-specific step | symbol-server integration |
| 4 | the APK and Windows install size delta, and every shared object it adds being 16 KB aligned; the minimum SDK it requires against minSdk 34 | size; the 16 KB page and minSdk 34 constraints; the package budget (Z2-B-PACK-001, Z2-B-PACK-002) |
| 5 | no exception thrown or caught in the candidate's code compiled into engine targets (the `ForbiddenCheck` scan over its compile groups), no network access, no extra Android permission, nothing written outside the app's own storage, nothing reachable remotely in Shipping | DecisionLog's last two clauses |
| 6 | installation cost at startup on the S24 (at most 5 ms, the startup row's share) and the integration and maintenance cost: lines of DEVT code, manifest and build changes, and the pinned upstream's update cadence | maintenance |

Applying the rule: the ruling selects Crashpad unless input 4 shows it cannot meet the 16 KB alignment, minSdk 34 or the package budget, in which case it selects the custom writer. The custom writer's Android path is out-of-process (debuggerd) but produces a tombstone, not a minidump, so it **fails DecisionLog's minidump clause on Android**; the harness report states that failure explicitly, and a ruling that selects the custom writer (because Crashpad was disqualified) records it in its own decision entry as the accepted deviation from that clause, or extends the custom candidate with an Android minidump writer and re-measures before ruling. The harness ticket's CTest `--smoke` runs the Windows custom capture against a child process that dereferences null and asserts the dump and report exist; the `needs-human` ruling ticket runs both candidates on both devices, commits `Zenith2/Docs/Research/DSelCrash/Report.md` (one section per input above, per candidate and platform, ending with the rule applied) and appends the ruling to `Zenith2/Docs/DecisionLog.md` with the next free `Z2-D-NNN` in its own diff (Z2-D-084, Z2-D-097).

## Crashpad realisation

If the ruling selects Crashpad: `Zenith_CrashHandler::Create` starts `crashpad::CrashpadClient::StartHandler` with the database `user:/crash/crashpad` (the mapped OS path), no URL, the annotations passed as Crashpad annotations (`crashpad::Annotation` objects in a `crashpad::AnnotationList`, updated by `SetAnnotation` from the same seqlock table on change), and the log mirror registered as an extra memory range (`crashpad::SimpleAddressRangeBag`); `Finalize` reads completed reports from Crashpad's database (`CrashReportDatabase::GetCompletedReports`), writes a `zenith2.crash-report` beside each minidump with the same schema (`files.minidump` pointing into the database) and applies retention through `CrashReportDatabase::DeleteReport`. The public header, the report schema, the fatal decode, attachments, hang reports, thread registration (Crashpad needs none, so the thunk records names only) and symbolization are unchanged; `Zenith_RunCrashHelperIfRequested` returns false (Crashpad's own handler executable replaces the relaunched helper). The Crashpad libraries are linked into ZenithPlatform under `Internal/` and `crashpad_handler` is packaged by SHIP's Windows and Android package rules.

## Custom realisation

If the ruling selects the custom writer, the realisation is § "Crash handler anchor", § "Windows capture", § "Crash helper security contract" and § "Android capture" exactly, with no third-party code and no process-wide state beyond the one allowlisted anchor; the manifest gains no entry, the `crashpad` row of the admitted-by-selection table is removed in the ruling's diff, and the ruling's entry carries the Android minidump deviation of § "D-SEL-CRASH candidates and acceptance".
