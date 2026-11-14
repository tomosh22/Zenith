# Zenith2 Scripting — public and internal types

Companion to `Zenith2/Docs/Scripting.md`: the declarations of every type that the Scripting document family names in a signature and does not declare where it is first used. The binding-ABI types (`Zenith_ScriptValue`, `Zenith_ScriptCallbackRef`, `Zenith_ScriptCallbackOutcome`, `Zenith_ScriptInvocationContext`, …) are declared in `Scripting/Bindings.md` → "Binding registration and the thunk ABI"; the transaction structures in `Scripting/TransactionStructures.md` → "Transaction data structures"; `Zenith_ScriptWorld` in `Scripting/LibraryLayout.md` → "Library layout and file map". Normative bullets share the `SCRIPT` numbering of the Scripting document family.

## Public types

<!-- elaboration -->
### ZenithScript public types

```cpp
// Zenith2/Engine/ZenithScript/Zenith_ScriptTypes.h
#pragma once
#include "ZenithReflect/Zenith_ScriptBinding.h"

struct Zenith_ScriptError;           // Zenith_ScriptError.h, which includes this header
class Zenith_ScriptVM;               // Internal

inline constexpr uint32 uSCRIPT_MAX_ROWS_PER_INVOCATION = 256;   // Scripting.md → "Component writes shadows and side-effect buffers"

inline constexpr uint32 uSCRIPT_INVALID_MODULE = 0xFFFFFFFFu;
inline constexpr uint32 uSCRIPT_MAX_PROTECTED_DEPTH = 8;         // Scripting.md → "Protected entry algorithm"
inline constexpr uint32 uSCRIPT_MAX_CHECKPOINT_FIXUPS = 16;      // Scripting.md → "Checkpoint format"
inline constexpr uint32 uSCRIPT_MAX_MODULES_PER_GAME = 8192;     // Scripting.md → "Untrusted inputs"
inline constexpr uint32 uSCRIPT_MAX_SOURCE_BYTES = 1u << 20;     // 1 MiB per module
inline constexpr uint32 uSCRIPT_MAX_WORKER_VM_SLOTS = 254;       // frame threads per world: a VM index stays below 0xFFFF and below 256 for DAP thread IDs
inline constexpr uint16 uSCRIPT_RESERVED_VM_INDEX = 0xFFFFu;     // never issued to a VM: ECS playback scopes (Scripting.md → "VM model and threading")
inline constexpr uint32 uSCRIPT_DIAGNOSTIC_RING_ENTRIES = 64;    // per world, and one ring in the runtime (Scripting.md → "The protected entry")
inline constexpr uint32 uSCRIPT_MAX_DECLARED_WRITE_PROPERTIES = 64;  // per declaration, after expansion to properties
inline constexpr uint32 uSCRIPT_MAX_STATE_FIELDS = 256;          // per state schema, nested fields included
inline constexpr uint32 uSCRIPT_MAX_NESTING = 200;               // syntactic nesting, Scripting.md → "Untrusted inputs"
inline constexpr uint32 uSCRIPT_SAMPLE_RING_ENTRIES = 65536;     // sampling profiler, Scripting.md → "Sampling profiler"
inline constexpr uint32 uSCRIPT_SAMPLE_PERIOD = 997;             // safepoints between samples
inline constexpr uint16 uSCRIPT_DAP_DEFAULT_PORT = 27851;
inline constexpr uint32 uSCRIPT_WATCHDOG_MAX_WORLDS = 64;        // world indices of the watchdog
inline constexpr uint32 uSCRIPT_ASYNC_STALE_TICKS = 600;         // age of a published-unconsumed result that SCRIPT_ASYNC_CAPACITY_HELD reports
inline constexpr uint64 ulSCRIPT_TXN_RETENTION_BLOCK_BYTES = 256ull * 1024;   // retention arena block (Scripting.md → "Component writes shadows and side-effect buffers")
inline constexpr uint32 uSCRIPT_TXN_RETAINED_BLOCKS = 1;         // blocks a retention arena keeps across Reset

struct Zenith_ScriptModuleID { uint32 m_uValue = uSCRIPT_INVALID_MODULE; };   // index in the game's module manifest (sorted path order)

// 64-bit: bits 63..48 VM index (0 main, 1 + worker index), bits 47..0 per-VM counter; never reused within a world session.
struct Zenith_ScriptInvocationID { uint64 m_ulValue = 0; };

struct Zenith_ScriptLogicalID
{
	uint64 m_ulSimulationID = 0;
	uint32 m_uInvocationOrdinal = 0;
};

enum Zenith_ScriptModuleKind : uint8
{
	SCRIPT_MODULE_KIND_CHUNK_SYSTEM,
	SCRIPT_MODULE_KIND_BEHAVIOUR,
	SCRIPT_MODULE_KIND_LIBRARY,
};

enum Zenith_ScriptInstanceState : uint8
{
	SCRIPT_INSTANCE_STATE_ACTIVE,
	SCRIPT_INSTANCE_STATE_QUARANTINED,
};

enum Zenith_ScriptViewKind : uint8
{
	SCRIPT_VIEW_KIND_ROW,
	SCRIPT_VIEW_KIND_COMPONENT,
	SCRIPT_VIEW_KIND_CONTAINER,
	SCRIPT_VIEW_KIND_CONTEXT,
	SCRIPT_VIEW_KIND_STATE,
};

// Zenith_ScriptNativePolicy: Scripting.md → "Native codegen conformance"; Zenith_ScriptRecoveryPolicy: Scripting.md → "Recovery policy".

struct Zenith_ScriptModuleManifestEntry
{
	Zenith_ScriptModuleID m_xID;
	Zenith_ScriptModuleKind m_eKind = SCRIPT_MODULE_KIND_LIBRARY;
	uint64 m_ulPathHash = 0;              // Hash64 of the manifest path (@game/…)
	uint32 m_uPathOffset = 0;             // into the manifest's string block
	uint32 m_uPathBytes = 0;
	uint32 m_uFirstDependency = 0;        // into the manifest's dependency array
	uint32 m_uDependencyCount = 0;
	uint32 m_uBytecodeAssetIndex = 0;     // the cooked .zlbc asset of the module
};

// One load-time name reference of a module ([Z2-N-SCRIPT-922]): a string-literal argument of a binding parameter marked with
// m_szResolveKind, as Zenith_ScriptExtractNameRefs records it and scripts.zlmf's name-reference block carries it
// (Zenith_ScriptModuleManifest::GetNameRefs). A hot-reload request carries the reloaded module's references ([Z2-N-SCRIPT-927]);
// the views are borrowed for the request, which copies them.
struct Zenith_ScriptNameRefRecord
{
	Zenith_StringView m_strKind;           // the name table's kind (Zenith_ScriptBindingTable::AddNameTable)
	Zenith_StringView m_strName;           // the literal argument
	uint32 m_uLine = 0;                   // 1-based source line of the call
};

/// @contract type Zenith_ScriptModuleManifest
/// Ownership: Loaded from the game's cooked module manifest by Zenith_ScriptModuleManifest::Load as a Zenith_Owned held by the
///   composition root; immutable after load and shared by every world of the game.
/// Thread affinity: IMMUTABLE. Every member is const after Load.
/// Invalidation: None.
/// Units: Sizes in bytes.
/// Errors: Load returns SCRIPT_ERROR_KIND_LOAD for a malformed or mismatched manifest; Find returns uSCRIPT_INVALID_MODULE for an unknown path.
/// Version: since Z2 Stage 3. Cooked data; re-cooked on any engine version change.
/// @endcontract
class Zenith_ScriptModuleManifest
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_ScriptModuleManifest>, Zenith_ScriptError> Load(Zenith_ByteSpan xCooked);
	Zenith_ScriptModuleID Find(const char* szPath, uint32 uPathBytes) const;   // O(1) hash lookup
	const Zenith_ScriptModuleManifestEntry& GetEntry(Zenith_ScriptModuleID xModule) const;
	Zenith_Span<const Zenith_ScriptModuleID> GetDependencies(Zenith_ScriptModuleID xModule) const;
	// the module's load-time name references from the name-reference block ([Z2-N-SCRIPT-922]); views into the manifest
	Zenith_Span<const Zenith_ScriptNameRefRecord> GetNameRefs(Zenith_ScriptModuleID xModule) const;
	uint32 GetModuleCount() const;
	Zenith_Span<const Zenith_ScriptStateSchemaRecord> GetStateSchemas() const;   // cooked state schemas, module-ID order
	Zenith_StringView GetPath(Zenith_ScriptModuleID xModule) const;
	uint64 GetBindingSetHash() const;     // the cook's value; compared at Load
	uint64 GetEngineVersionHash() const;
	// the module's deprecated-call records, ascending line; empty for a module that calls no deprecated binding
	Zenith_Span<const Zenith_ScriptDeprecatedCallRecord> GetDeprecatedCalls(Zenith_ScriptModuleID xModule) const;
};

// One deprecated call site of a cooked module, decoded from the manifest (value, IMMUTABLE, invalidated with its manifest;
// members non-const: Load decodes the records into the manifest's array after constructing it).
struct Zenith_ScriptDeprecatedCallRecord
{
	Zenith_ScriptFunctionID m_xFunction;       // the deprecated binding the line calls (the first in AST pre-order on that line)
	uint32 m_uLine = 0;                       // 1-based source line
};

// The decoded state-schema record of the cooked manifest (module ID, stable name, StateVersion, field range into the
// manifest's field table, maximum serialized bytes, shape hash); Zenith2/Docs/Design/SCRIPT/Compiler.md → "Module manifest format".
struct Zenith_ScriptStateSchemaRecord;

struct Zenith_ScriptBytecodeHeader       // the 48-byte .zlbc header of Scripting.md → "Cooked bytecode format"
{
	uint32 m_uMagic = 0;                // 'ZLBC'
	uint16 m_uContainerVersion = 0;
	uint8 m_uLuauBytecodeVersion = 0;
	uint8 m_uFlags = 0;                 // bit 0 native, bit 1 simulation, bit 2 line info, bit 3 stateless check passed
	uint64 m_ulLuauPinHash = 0;
	uint64 m_ulEngineVersionHash = 0;
	uint64 m_ulBindingSetHash = 0;
	uint64 m_ulSourceHash = 0;
	uint32 m_uModuleID = 0;
	uint32 m_uBlobBytes = 0;
};

struct Zenith_ScriptBytecode
{
	Zenith_ScriptBytecodeHeader m_xHeader;
	Zenith_ByteSpan m_xBlob;             // the Luau bytecode blob, in the arena the producer was given
};

struct Zenith_ScriptGCTuning             // values from Z2-B-SCRIPT-017 "Script GC tuning parameters"; no default is hard-coded
{
	uint32 m_uGoalPercent;
	uint32 m_uStepMultiplier;
	uint32 m_uStepSizeKB;
};

/// @contract value Zenith_ScriptWatchdogDesc
/// Ownership: Plain value; m_pxHeap, m_pxFaults and m_pxCrashHandler are borrowed for the watchdog's lifetime.
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct it and set the fields they need.
/// Invalidation: None; Create copies every member it keeps.
/// Units: m_uLimitMs and m_uPollMs milliseconds; m_uMaxWorlds worlds; m_uFrameThreadCount frame threads.
/// Errors: Zenith_ScriptWatchdog::Create returns SCRIPT_ERROR_INVALID_DESC for a zero limit, poll or frame-thread count, or
///   m_uMaxWorlds outside 1 … uSCRIPT_WATCHDOG_MAX_WORLDS.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_CrashHandler;                 // ZenithPlatform (Zenith2/Docs/Design/DEVT/Crash.md)

struct Zenith_ScriptWatchdogDesc
{
	uint32 m_uLimitMs = 0;              // Z2-B-SCRIPT-003
	uint32 m_uPollMs = 10;
	uint32 m_uMaxWorlds = 16;           // 1 … uSCRIPT_WATCHDOG_MAX_WORLDS (64)
	uint32 m_uFrameThreadCount = 0;     // Zenith_JobSystem::GetFrameThreadCount(); the table holds m_uMaxWorlds × (1 + this) entries
	Zenith_Heap* m_pxHeap = nullptr;
	Zenith_FaultInjector* m_pxFaults = nullptr;
	Zenith_CrashHandler* m_pxCrashHandler = nullptr;         // DEVT's crash handler (ZenithPlatform); null in sanitizer builds and tests
};

inline constexpr uint32 uFAULT_POINT_SCRIPT_WATCHDOG_THREAD = 0x53430020u;
inline constexpr uint32 uFAULT_POINT_SCRIPT_WATCHDOG_TABLE = 0x53430021u;

/// @contract type Zenith_ScriptWatchdog
/// Ownership: Created by the Runtime composition root as a Zenith_Owned, one per process; passed by pointer to every Zenith_ScriptWorld.
/// Thread affinity: ANY_THREAD. Register and Unregister are called by VM construction and destruction; the watchdog thread reads the registered VMs.
/// Invalidation: A VM is unregistered before it is destroyed; unregistration waits for an in-flight poll of that VM.
/// Units: Milliseconds of wall-clock time.
/// Errors: Register of a main VM returns SCRIPT_ERROR_WATCHDOG_TABLE_FULL when every one of the m_uMaxWorlds world indices is in
///   use; a worker VM's Register never fails (its world's block was reserved with the main VM). The table holds
///   m_uMaxWorlds × (1 + m_uFrameThreadCount) entries, at most 64 × 64.
/// Version: since Z2 Stage 3.
/// Locks: uLOCK_LEVEL_SCRIPT_WATCHDOG (700), short-hold; a debugger stop never takes it.
/// Hang: on a hang past twice the limit the watchdog thread calls m_pxCrashHandler->WriteHangReport(threadID, reason,
///   scriptStack) when the desc's pointer is set, logs the same text otherwise, then raises FATAL_CODE_SCRIPT_HANG.
/// @endcontract
class Zenith_ScriptWatchdog
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_ScriptWatchdog>, Zenith_ScriptError> Create(const Zenith_ScriptWatchdogDesc& xDesc);
	~Zenith_ScriptWatchdog();                            // joins the watchdog thread Zenith.ScriptWatchdog
	Zenith_Result<void, Zenith_ScriptError> Register(Zenith_ScriptVM& xVM);
	void Unregister(Zenith_ScriptVM& xVM);
	// No world-level suspension: the poll skips a VM whose m_bDebuggerStopped is set (Scripting.md → "Wall-clock watchdog").
};
```

### ZenithScript error types

```cpp
// Zenith2/Engine/ZenithScript/Zenith_ScriptError.h
#pragma once
#include "ZenithReflect/Zenith_ScriptBinding.h"   // Zenith_ScriptErrorKind
#include "ZenithScript/Zenith_ScriptTypes.h"

inline constexpr uint32 uSCRIPT_ERROR_MESSAGE_BYTES = 512;
inline constexpr uint32 uSCRIPT_ERROR_TRACEBACK_BYTES = 2048;

/// @contract enum Zenith_ScriptErrorCode
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; meanings in Zenith2/Docs/Design/SCRIPT.md → "Error enumerations".
/// Version: since Z2 Stage 3. Numbered from 1; values never renumbered.
/// @endcontract
enum Zenith_ScriptErrorCode : uint8
{
	SCRIPT_ERROR_RUNTIME = 1, SCRIPT_ERROR_MISUSE, SCRIPT_ERROR_BUDGET, SCRIPT_ERROR_SNAPSHOT_CAP, SCRIPT_ERROR_RESERVATION,
	SCRIPT_ERROR_YIELD_IN_SYSTEM, SCRIPT_ERROR_CHECKPOINT, SCRIPT_ERROR_VM_MEMORY, SCRIPT_ERROR_WATCHDOG, SCRIPT_ERROR_INTERNAL,
	SCRIPT_ERROR_COMPILE, SCRIPT_ERROR_LOAD,                 // 1..12: one per Zenith_ScriptErrorKind, in kind order (kind + 1)
	SCRIPT_ERROR_INVALID_DESC,                               // 13: an argument outside its documented range
	SCRIPT_ERROR_OUT_OF_MEMORY,                              // 14: a C++-side allocation outside any transaction failed
	SCRIPT_ERROR_WORLD_HALTED,                               // 15: a request to a halted world
	SCRIPT_ERROR_UNKNOWN_MODULE,                             // 16
	SCRIPT_ERROR_NOT_STATELESS_VERIFIED,                     // 17: hot-reload bytecode without the stateless flag
	SCRIPT_ERROR_RELOAD_QUEUE_FULL,                          // 18
	SCRIPT_ERROR_WATCHDOG_TABLE_FULL,                        // 19
	SCRIPT_ERROR_DEBUGGER_UNAVAILABLE,                       // 20: only when Zenith_ScriptDebuggerDesc::m_bRequired
	SCRIPT_ERROR_BYTECODE_MISMATCH,                          // 21: a .zlbc or manifest field differs from the build
};

/// @contract value Zenith_ScriptError
/// Ownership: Plain value, 32 bytes.
/// Thread affinity: IMMUTABLE. Members non-const: it is the error of Zenith_Result, whose move assignment assigns it.
/// Invalidation: m_uDiagnosticSequence names an entry of its world's (or the runtime's) diagnostic store; after 64 newer
///   diagnostics it is overwritten and GetDiagnostic reports false.
/// Units: m_uLine is a 1-based source line (0 unknown).
/// Errors: m_eCode is never 0. For a Luau error m_eCode == Zenith_ScriptErrorCodeFromKind(m_eKind); a C++-API failure
///   (codes from SCRIPT_ERROR_INVALID_DESC on) carries m_eKind = SCRIPT_ERROR_KIND_LOAD.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_ScriptError
{
	Zenith_ScriptErrorCode m_eCode = SCRIPT_ERROR_RUNTIME;     // first member (Contracts.md → "Errors and results")
	Zenith_ScriptErrorKind m_eKind = SCRIPT_ERROR_KIND_RUNTIME;
	uint16 m_uCode = 0;                                       // ZS catalogue code; 0 for a plain script error
	Zenith_ScriptModuleID m_xModule;                           // module whose code raised, when known
	uint32 m_uLine = 0;
	uint32 m_uDiagnosticSequence = 0;                         // 0 = no diagnostic recorded
	uint32 m_uReserved = 0;
	uint64 m_ulSimulationID = 0;                              // the failing transaction's entity; 0 outside one

	bool IsAbortClass() const;         // BUDGET, SNAPSHOT_CAP, YIELD_IN_SYSTEM, CHECKPOINT
	bool IsFatal() const;              // RESERVATION, VM_MEMORY, WATCHDOG, INTERNAL
};
static_assert(sizeof(Zenith_ScriptError) <= 32);

/// @contract value Zenith_ScriptDiagnostic
/// Ownership: Plain value copied out of a diagnostic store by GetDiagnostic, or held by a Zenith_ScriptCompileReport.
/// Thread affinity: IMMUTABLE. Members non-const: the diagnostic ring overwrites its slots and GetDiagnostic assigns xOut.
/// Invalidation: None (a copy).
/// Units: Text is UTF-8, NUL-terminated, truncated at a code-point boundary; m_ulTick is the fixed-step tick (0 out of tick).
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_ScriptDiagnostic
{
	Zenith_ScriptError m_xError;
	uint32 m_uSequence = 0;
	uint16 m_uWorldTag = 0;
	uint32 m_uColumn = 0;
	uint64 m_ulTick = 0;
	char m_acMessage[uSCRIPT_ERROR_MESSAGE_BYTES] = {};       // the exact catalogue text for a ZS error, with the position prefix
	char m_acTraceback[uSCRIPT_ERROR_TRACEBACK_BYTES] = {};
};

constexpr Zenith_ScriptErrorCode Zenith_ScriptErrorCodeFromKind(Zenith_ScriptErrorKind eKind)
{
	return static_cast<Zenith_ScriptErrorCode>(static_cast<uint8>(eKind) + 1u);
}
constexpr const char* Zenith_ToString(Zenith_ScriptErrorCode eCode);   // "SCRIPT_ERROR_BUDGET", ...
```

<!-- informative -->
One struct-form error per code enumeration (`Zenith2/Docs/Contracts.md` → "Errors and results"): SCRIPT's C++ API has exactly one, `Zenith_ScriptError` with `Zenith_ScriptErrorCode`. The binding ABI in ZenithReflect has two error types of its own, each with its own name and prefix: the struct `Zenith_ScriptThunkError` with `Zenith_ScriptThunkErrorCode` (`SCRIPT_THUNK_ERROR_`), and the enumeration `Zenith_ScriptBindingError` (`SCRIPT_BINDING_ERROR_`) of `Zenith_ScriptBindingTable::Seal` (`Scripting/Bindings.md` → "Binding registration and the thunk ABI"). No error type shares another's code enumeration.
<!-- /informative -->

```cpp
// Zenith2/Engine/ZenithScript/Zenith_ScriptRecovery.h: Zenith_ScriptRecoveryPolicy (Scripting.md → "Recovery policy") and the
// values game.json's "scriptRecovery" carries; Zenith_ScriptWorldDesc's recovery members are filled from one settings value.
struct Zenith_ScriptRecoverySettings
{
	Zenith_ScriptRecoveryPolicy m_ePolicy = SCRIPT_RECOVERY_POLICY_MANUAL;
	uint32 m_uDelayTicks = 60;                // 1 … 3600
	uint32 m_uMaxRecoveriesPerModule = 3;     // 0 … 100
};
// Without "scriptRecovery": manual in every configuration other than Shipping; automatic, 60 ticks, 3 recoveries in Shipping.
Zenith_ScriptRecoverySettings Zenith_ScriptRecoveryResolve(bool bDeclared, Zenith_ScriptRecoveryPolicy ePolicy,
	uint32 uDelayTicks, uint32 uMaxRecoveries);
```

### Zenith_ScriptRuntime

```cpp
// Zenith2/Engine/ZenithScript/Zenith_ScriptRuntime.h
#pragma once
#include "ZenithScript/Zenith_ScriptTypes.h"
#include "ZenithScript/Zenith_ScriptError.h"
#include "ZenithScript/Zenith_ScriptRecovery.h"  // Zenith_ScriptRecoverySettings (Scripting.md → "Recovery policy")
#include "ZenithCore/Zenith_ReentrancyGuard.h"
#include "ZenithCore/Zenith_BuildInfo.h"         // Zenith_BuildPlatform
#include "ZenithCore/Zenith_ConfigStore.h"       // Zenith_ConfigSchema, Zenith_ConfigError

class Zenith_TypeRegistry;           // ZenithReflect
class Zenith_MigrationScriptHost;    // ZenithReflect (Zenith_SchemaMigrator.h), Zenith2/Docs/Design/ECS/Schemas.md → "Script ops"
class Zenith_ScriptWorld;            // Scripting/LibraryLayout.md → "Library layout and file map"
struct Zenith_ScriptWorldDesc;

// The editor's console compile thunk for Zenith_ScriptWorld::ConsoleEvaluate: compiles strSource into xOutputArena with the
// production options of Zenith_ScriptCompileModule (ZenithScriptCompiler, which ZenithScript does not link). ANY_THREAD;
// a compile failure is SCRIPT_ERROR_COMPILE with the Luau line.
using Zenith_PfnScriptCompileConsole = Zenith_Result<Zenith_ScriptBytecode, Zenith_ScriptError> (*)(Zenith_StringView strSource,
	Zenith_Arena& xOutputArena);

/// @contract value Zenith_ScriptDebuggerDesc
/// Ownership: Plain value; m_strBindAddress borrows the command line's storage for the Create call.
/// Thread affinity: IMMUTABLE. Members non-const: the Player fills it from its arguments after default construction, as the
///   m_xDebugger member of Zenith_ScriptRuntimeDesc.
/// Invalidation: None.
/// Units: A TCP port.
/// Errors: Ignored in Shipping, where no debugger code is linked. A listener that cannot be created is an error of
///   Zenith_ScriptRuntime::Create (SCRIPT_ERROR_DEBUGGER_UNAVAILABLE) only when m_bRequired is set; otherwise the runtime is
///   created without a debugger, records SCRIPT_DEBUGGER_UNAVAILABLE and IsDebuggerAvailable() reports false.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_ScriptDebuggerDesc
{
	bool m_bEnabled = false;                     // --script-dap-port given, or the editor
	bool m_bRequired = false;                    // set by the DAP tests' harness only; the Player and the editor never set it
	uint16 m_uPort = uSCRIPT_DAP_DEFAULT_PORT;
	Zenith_StringView m_strBindAddress;          // empty = 127.0.0.1
};

struct Zenith_ScriptRuntimeDesc
{
	const Zenith_ScriptBindingTable* m_pxBindings = nullptr;   // sealed by the composition root
	const Zenith_ScriptModuleManifest* m_pxModules = nullptr;
	Zenith_ScriptWatchdog* m_pxWatchdog = nullptr;             // borrowed; owned by the composition root
	const Zenith_TypeRegistry* m_pxTypes = nullptr;            // frozen at startup step 10
	Zenith_ScriptRecoverySettings m_xRecovery;                 // Zenith_ScriptRecovery.h; game.json "scriptRecovery"
	Zenith_ScriptNativePolicy m_eNativePolicy = SCRIPT_NATIVE_POLICY_TOOLS_ONLY;
	Zenith_ScriptDebuggerDesc m_xDebugger;
	Zenith_Heap* m_pxHeap = nullptr;
	Zenith_FaultInjector* m_pxFaults = nullptr;
	Zenith_PfnScriptCompileConsole m_pfnCompileConsole = nullptr;   // the editor's thunk over Zenith_ScriptCompileModule; null outside the editor
};

/// @contract type Zenith_ScriptRuntime
/// Ownership: Created by ZenithRuntime (the composition root) as a Zenith_Owned, one per process, and returned by its
///   GetScripts(); owns every Zenith_ScriptWorld it creates and the queue of pending hot reloads; destroyed after every
///   world's tick is drained, destroying its worlds first.
/// Thread affinity: MAIN_THREAD. Every member runs on the main thread, in the frame prologue or at world creation and teardown.
/// Invalidation: DestroyWorld invalidates that world's pointer; a module generation staged by ApplyPendingReloads is dropped
///   if the window it was staged in does not publish.
/// Units: None.
/// Errors: ApplyPendingReloads returns the first failure (bytecode load, declaration, state-schema registration,
///   Migrate) as a Zenith_ScriptError after dropping every generation it staged; script errors never assert.
/// Reentrancy: NONE. ApplyPendingReloads runs Migrate steps (script) under the recovery safepoint row; no member of this
///   type may be called from inside them.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_ScriptRuntime
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_ScriptRuntime>, Zenith_ScriptError> Create(const Zenith_ScriptRuntimeDesc& xDesc);
	~Zenith_ScriptRuntime();

	Zenith_Result<Zenith_ScriptWorld*, Zenith_ScriptError> CreateWorld(const Zenith_ScriptWorldDesc& xDesc);
	void DestroyWorld(Zenith_ScriptWorld& xWorld);

	bool HasPendingReloads() const;     // ZenithRuntime opens a window only when true

	// Frame-prologue step 6, inside the hot-reload window ZenithRuntime has opened: stages every queued reload of every world
	// (world creation order, then module ID order), registering each new TYPE_KIND_BEHAVIOUR_STATE type in xRegistry and
	// migrating every instance into staging objects. The staged generations become current at the frame's first tick boundary
	// if xRegistry.GetRevision() has advanced past the revision observed on entry (EndHotReload published); otherwise they are dropped.
	Zenith_Result<void, Zenith_ScriptError> ApplyPendingReloads(Zenith_TypeRegistry& xRegistry);

	// Frame-prologue step 6, in every frame, after the hot-reload window closed (published or abandoned) or in its place when no
	// change was queued: drains every world's main-VM release list, rolls back the staged reloads of a window that did not
	// publish, commits each world's changed script systems through Zenith_SystemReloadScope (recompiled in prologue step 8 of the
	// same frame), rolls back a world whose commit fails, and marks the surviving generations COMMITTED; they go live at the
	// world's next tick boundary, which only swaps generations. Failures go to the runtime's diagnostic ring and the log.
	void CommitPendingReloads();

	Zenith_Result<void, Zenith_ScriptError> RequestCookedReload(Zenith_ScriptModuleID xModule, Zenith_ByteSpan xZlbc,
		Zenith_Span<const Zenith_ScriptNameRefRecord> axNameRefs = {});
		// development builds: decodes a re-cooked .zlbc and queues it, with the module's load-time name references, on every
		// world with RequestHotReload; its caller, the development Player's Zenith_FileWatcher on content:/Scripts/, re-reads the
		// re-cooked content:/Scripts/scripts.zlmf with Zenith_ScriptModuleManifest::Load (the cook's watch mode writes it after
		// the changed .zlbc files) and passes GetNameRefs(xModule) ([Z2-N-SCRIPT-927])
	void PollDebugger();                                 // frame-loop step 3; a no-op without a debugger
	bool CanRunTick(uint16 uWorldTag, uint64 ulTick) const;
		// replay: false while a recomputed result recorded for ulTick is incomplete (Scripting.md [Z2-N-SCRIPT-337]); true otherwise
	Zenith_ScriptWorld* FindWorld(uint16 uWorldTag);
	Zenith_Span<Zenith_ScriptWorld* const> GetWorlds();   // creation order
	bool GetDiagnostic(uint32 uSequence, Zenith_ScriptDiagnostic& xOut) const;   // the runtime's own ring
	bool IsDebuggerAvailable() const;      // false in Shipping, without m_bEnabled, or when the listener failed

private:
	Zenith_ReentrancyGuard m_xReentrancyGuard;
};

// Startup step 6, the Script slot of the engine types: registers Zenith_BehaviourComponent ("Zenith.Behaviour").
Zenith_Result<void, Zenith_ScriptError> Zenith_ScriptRegisterEngineTypes(Zenith_TypeRegistry& xTypes);

// Startup step 10, after the game's schemas.zreg and before Freeze: one "Behaviour.<Name>.State" (TYPE_KIND_BEHAVIOUR_STATE,
// Origin(TYPE_ORIGIN_SCHEMA)) per state-schema record of the cooked manifest, in module-ID order; SCRIPT_ERROR_LOAD names the module.
Zenith_Result<void, Zenith_ScriptError> Zenith_ScriptRegisterBehaviourStateTypes(Zenith_TypeRegistry& xTypes,
	const Zenith_ScriptModuleManifest& xManifest);

// Startup step 6, library-layer order: SCRIPT's configuration keys (Zenith2/Docs/Design/SCRIPT.md → "Configuration keys").
Zenith_Result<void, Zenith_ConfigError> Zenith_RegisterScriptConfigKeys(Zenith_ConfigSchema& xSchema,
	Zenith_BuildPlatform ePlatform);

// Returns the cooked migrate bytecode of a schema migration module (the cook's Schemas/<Component>.migrate.luau output).
// Called on the main thread from RunScriptOp; an unknown module is SCRIPT_ERROR_UNKNOWN_MODULE.
using Zenith_PfnScriptMigrationBytecode = Zenith_Result<Zenith_ByteSpan, Zenith_ScriptError> (*)(void* pUserData,
	Zenith_StringView strModule);

/// @contract value Zenith_ScriptMigrationHostDesc
/// Ownership: Plain value; m_pxHeap, m_pxFaults and the lookup's user data are borrowed and outlive the created host.
/// Thread affinity: IMMUTABLE. Members const: its creators aggregate-initialise it and pass it by const&.
/// Invalidation: None.
/// Units: m_ulVMCapBytes in bytes (the value of Z2-B-SCRIPT-009); m_uSafepointLimit in safepoints per op (Z2-B-SCRIPT-018).
/// Errors: A null heap or lookup, or a zero cap or limit, is SCRIPT_ERROR_INVALID_DESC from Zenith_ScriptCreateMigrationHost.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_ScriptMigrationHostDesc
{
	Zenith_Heap* const m_pxHeap = nullptr;
	Zenith_FaultInjector* const m_pxFaults = nullptr;
	const Zenith_PfnScriptMigrationBytecode m_pfnFindBytecode = nullptr;
	void* const m_pBytecodeUserData = nullptr;
	const uint64 m_ulVMCapBytes = 0;
	const uint32 m_uSafepointLimit = 0;
};

/// @contract functions Zenith_ScriptCreateMigrationHost
/// Ownership: Returns a Zenith_Owned host the caller destroys: ZenithRuntime for save loading, the editor for schema migrations.
/// Thread affinity: MAIN_THREAD; the host's RunScriptOp runs on the main thread, out of tick.
/// Invalidation: None.
/// Units: None.
/// Errors: SCRIPT_ERROR_INVALID_DESC for a malformed desc; the host's own VM is created on first use, so a VM creation failure
///   is the MIGRATION_ERROR_SCRIPT of that RunScriptOp (Zenith2/Docs/Design/SCRIPT/HotReload.md → "Schema migration script host").
/// Version: since Z2 Stage 3.
/// @endcontract
Zenith_Result<Zenith_Owned<Zenith_MigrationScriptHost>, Zenith_ScriptError> Zenith_ScriptCreateMigrationHost(
	const Zenith_ScriptMigrationHostDesc& xDesc);

inline constexpr uint32 uFAULT_POINT_SCRIPT_RUNTIME_WORLD_TABLE = 0x53430010u;
inline constexpr uint32 uFAULT_POINT_SCRIPT_RUNTIME_RELOAD_QUEUE = 0x53430011u;
inline constexpr uint32 uFAULT_POINT_SCRIPT_RUNTIME_DEBUGGER = 0x53430012u;
inline constexpr uint32 uFAULT_POINT_SCRIPT_RUNTIME_DIAGNOSTICS = 0x53430013u;
inline constexpr uint32 uFAULT_POINT_SCRIPT_RUNTIME_DAP_SLOTS = 0x53430014u;   // CreateWorld with a debugger
```

- [Z2-N-SCRIPT-903] `Zenith_ScriptRuntime::ApplyPendingReloads` is called by ZenithRuntime alone, in frame-prologue step 6, inside the registry hot-reload window ZenithRuntime opened; ZenithScript never calls `BeginHotReload`, `EndHotReload` or `AbandonHotReload` (`Zenith2/Docs/Architecture.md` → "Composition root and Player").
- [Z2-N-SCRIPT-923] ZenithScript creates the schema migration host only through `Zenith_ScriptCreateMigrationHost` (its implementation `Zenith_ScriptMigrationHost` stays in `Internal/`), called by ZenithRuntime for save loading and by the editor for schema migrations; `ConsoleEvaluate` compiles only through `Zenith_ScriptRuntimeDesc::m_pfnCompileConsole`, which the editor sets to a thunk over `Zenith_ScriptCompileModule` and every other creator leaves null, so a null thunk makes `ConsoleEvaluate` return `SCRIPT_ERROR_INVALID_DESC` and ZenithScript never links ZenithScriptCompiler.
- [Z2-N-SCRIPT-904] ZenithRuntime calls `Zenith_ScriptRuntime::CommitPendingReloads()` in frame-prologue step 6 of every frame, after the hot-reload window closed or, when no change was queued, in its place; it opens ECS's `Zenith_SystemReloadScope` there (a `MAIN_THREAD` scope admitted in prologue steps 6 to 8 alone), so a reload's changed declarations are committed and the schedule recompiles in step 8 of the same frame, and the frame's first tick boundary only swaps the staged generations in (`Zenith2/Docs/Design/SCRIPT/HotReload.md` → "Schedule commit in the prologue").

### ZenithScript asynchronous results and host interfaces

```cpp
// Zenith2/Engine/ZenithScript/Zenith_ScriptAsync.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithScript/Zenith_ScriptError.h"

class Zenith_AsyncPublicationHandler;      // Zenith_ScriptHost.h
class Zenith_Arena;                        // ZenithCore

/// @contract value Zenith_ScriptPublication
/// Ownership: Plain value; m_xPayload borrows the table's result arena (or the replay source's buffer).
/// Thread affinity: IMMUTABLE. Members non-const: the tick boundary sorts the drained publications in place by completion ID.
/// Invalidation: m_xPayload is valid until the result is released.
/// Units: m_uSourceKind and m_uPublicationKind hold Zenith_AsyncSourceKind and Zenith_PublicationKind values of
///   Zenith2/Docs/Architecture.md → "Multiplayer foundations" (ZenithSave owns those enumerations; ZenithScript carries the
///   uint8 values because it does not depend on ZenithSave).
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_ScriptPublication
{
	uint64 m_ulCompletionID = 0;
	uint8 m_uSourceKind = 0;
	uint8 m_uPublicationKind = 0;
	uint16 m_uReserved = 0;
	Zenith_ScriptModuleID m_xOwnerModule;              // the requesting behaviour's module, or uSCRIPT_INVALID_MODULE for C++ producers
	uint64 m_ulOwnerSimulationID = 0;                 // the requesting instance's entity, or 0
	Zenith_ByteSpan m_xPayload;
};

/// @contract enum Zenith_AsyncTableError
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: FULL: the source kind's in-flight capacity is exhausted (C++ producers only; script requests were accepted at the
///   merge); PAYLOAD_TOO_LARGE: above the source kind's declared maximum; UNKNOWN_COMPLETION: an ID never allocated or already completed.
/// Version: since Z2 Stage 3.
/// @endcontract
enum Zenith_AsyncTableError : uint8
{
	ASYNC_TABLE_ERROR_FULL = 1,
	ASYNC_TABLE_ERROR_PAYLOAD_TOO_LARGE,
	ASYNC_TABLE_ERROR_UNKNOWN_COMPLETION,
};
constexpr const char* Zenith_ToString(Zenith_AsyncTableError eError);

/// @contract value Zenith_AsyncTableEntry
/// Ownership: Plain value; m_xPayload borrows the arena the listing allocated from (ListUnpublished) or the caller's buffer
///   (RestoreUnpublished).
/// Thread affinity: IMMUTABLE. Members non-const: ListUnpublished fills the entries of an array it allocated first.
/// Invalidation: m_xPayload is valid while its arena or buffer is.
/// Units: 40 bytes; one entry of a world's asynchronous state, listed in ascending completion ID.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_AsyncTableEntry
{
	uint64 m_ulCompletionID = 0;
	uint8 m_uSourceKind = 0;
	uint8 m_uPublicationKind = 0;      // unpublished entries; 0 for an in-flight entry
	uint16 m_uReserved = 0;
	Zenith_ScriptModuleID m_xOwnerModule;            // uSCRIPT_INVALID_MODULE for a C++ request
	uint64 m_ulOwnerSimulationID = 0;
	Zenith_ByteSpan m_xPayload;                      // unpublished entries; empty for an in-flight entry
};

// A source kind's publication resolver (SetPublicationResolver): in live mode, at tick-boundary step 1, after the drained
// completions are sorted and before the publication observer logs them, it may rewrite one drained completion's publication
// kind and payload (at most uPayloadCapacity bytes, the kind's uMaxResultBytes); never called under a publication source.
using Zenith_PfnAsyncPublicationResolve = void (*)(uint64 ulCompletionID, uint8& uPublicationKind, uint8* pPayload,
	uint32 uPayloadCapacity, uint32& uPayloadBytes, void* pUserData);

/// @contract type Zenith_AsyncResultTable
/// Ownership: Owned by its Zenith_ScriptWorld (every world has one); the result arena is reserved at world creation from the
///   source kinds' declared in-flight capacities and maximum result sizes, a C++ source declared after creation (before the
///   first tick) reserving its slots at its DeclareSource; uSCRIPT_ASYNC_SOURCE_KIND_CAPACITY (32) source kinds (0 … 31). A table is destroyed
///   only when no producer of its world can still complete into it: a world destroyed with results in flight retires its
///   table, which outlives the world, discards and counts every later completion, and is released once nothing is in
///   flight ([Z2-N-ARCH-558], [Z2-N-ARCH-560]).
///   Every completion ID allocated from the table is completed exactly once by the producer that allocated it (or
///   took ownership of it), including work its DetachWorld cancels, completed with the producer's failure or cancellation
///   payload; a producer never counts a discarded result itself: the table, live or retired, discards and counts it, and
///   the world host alone publishes world.async_discarded (Zenith2/Docs/Scripting.md [Z2-N-SCRIPT-931]).
/// Thread affinity: ANY_THREAD_EXCEPT(AllocateCompletionID, SetHandler, SetPublicationResolver, DeclareSource, RestoreNextCompletionID). Complete runs on
///   any thread; AllocateCompletionID runs only on the world's merge chain or on the main thread in the frame prologue, a
///   prologue allocation only for a RECORDED publication kind and none at all from a prologue producer under --replay, where
///   the table adopts each REPLAY_RECORD_TICK record's next completion ID at the tick boundary ([Z2-N-SCRIPT-341]);
///   AllocateCompletionID(ASYNC_SOURCE_SAVE) also runs on the world's phase-11 snapshot node, for engine autosaves alone;
///   DeclareSource, SetHandler and SetPublicationResolver before the first tick; RestoreNextCompletionID out of tick; publication
///   on the tick-boundary node. Retire, IsSourceDeclared, RestoreUnpublished and RestoreInFlight are MAIN_THREAD and out of
///   tick (the two restores after RestoreNextCompletionID and before the world's first tick after a load); ListUnpublished and
///   ListInFlight run out of tick or on the phase-11 snapshot node; the counts and IsRetired are ANY_THREAD.
/// Invalidation: A script result is released at the tick boundary after an invocation that read it commits, or when its owning
///   instance is destroyed or quarantined; a C++ result right after its handler ran, an ASYNC_SOURCE_SAVE result included. A
///   zero-length PUBLICATION_KIND_GENERIC_RECORDED payload is the abandoned-request publication, which every decoder maps to
///   its failure.
/// Units: Completion IDs (per world, monotonic from 1); bytes.
/// Errors: Zenith_Result<…, Zenith_AsyncTableError>; never asserts on producer data. Under a publication source, Complete of an
///   ID whose recorded entry was already published returns ASYNC_TABLE_ERROR_UNKNOWN_COMPLETION and releases nothing twice.
/// Reentrancy: NONE. Handlers and resolvers run after the table's own state is updated; a resolver runs once per drained
///   completion of its kind, in completion-ID order, on the tick-boundary node, and the table logs, publishes and hands to the
///   handler what the resolver leaves.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_AsyncResultTable
{
public:
	void DeclareSource(uint8 uSourceKind, uint32 uMaxInFlight, uint32 uMaxResultBytes);   // before the first tick
	void SetHandler(uint8 uSourceKind, Zenith_AsyncPublicationHandler* pxHandler);           // before the first tick
	void SetPublicationResolver(uint8 uSourceKind, Zenith_PfnAsyncPublicationResolve pfnResolve, void* pUserData);   // before the first tick
	Zenith_Result<uint64, Zenith_AsyncTableError> AllocateCompletionID(uint8 uSourceKind);
	Zenith_Result<void, Zenith_AsyncTableError> Complete(uint64 ulCompletionID, uint8 uPublicationKind,
		Zenith_ByteSpan xPayload);
	uint64 GetNextCompletionID() const;       // saves and replays record it
	void RestoreNextCompletionID(uint64 ulNext);            // out of tick, save load
	bool IsRetired() const;
	bool IsSourceDeclared(uint8 uSourceKind) const;   // false after a DeclareSource whose slots could not be reserved
	// Once: discards and counts every completed unpublished record; afterwards Complete releases, discards and counts, and
	// nothing is published.
	void Retire();
	uint32 GetInFlightCount() const;                  // allocated and not yet released, every kind
	uint32 GetSourceInFlightCount(uint8 uSourceKind) const;
	uint64 GetDiscardedCount() const;                 // discarded at Retire and since
	// Spans allocated from xArena (ASYNC_TABLE_ERROR_FULL when it is exhausted), ascending completion ID.
	Zenith_Result<Zenith_Span<const Zenith_AsyncTableEntry>, Zenith_AsyncTableError> ListUnpublished(
		Zenith_Arena& xArena) const;                                 // completed and not yet published, with payloads
	Zenith_Result<Zenith_Span<const Zenith_AsyncTableEntry>, Zenith_AsyncTableError> ListInFlight(
		Zenith_Arena& xArena) const;                                 // allocated and not yet completed
	Zenith_Result<void, Zenith_AsyncTableError> RestoreUnpublished(
		Zenith_Span<const Zenith_AsyncTableEntry> axEntries);        // put back completed: published at the next boundary
	Zenith_Result<void, Zenith_AsyncTableError> RestoreInFlight(
		Zenith_Span<const Zenith_AsyncTableEntry> axEntries);        // put back allocated: completed by its producer
};
```

```cpp
// Zenith2/Engine/ZenithScript/Zenith_ScriptHost.h: the upward interfaces SCRIPT declares; ZenithRuntime (and the PLAT bridge
// and SCENE forwarder it installs) implements them, per Zenith2/Docs/Contracts.md → "Layering DAG" (callback direction).
#pragma once
#include "ZenithCore/Zenith_Span.h"
#include "ZenithScript/Zenith_ScriptError.h"

struct Zenith_ScriptPublication;           // Zenith_ScriptAsync.h
class Zenith_TickBoundaryAccess;           // ZenithECS

/// @contract interface Zenith_ScriptWorldListener
/// Ownership: Implemented by ZenithRuntime; passed by pointer in Zenith_ScriptWorldDesc; outlives the world.
/// Thread affinity: ANY_THREAD. OnWorldHalted on the thread that observed the fatal event (at most once per world);
///   OnModuleDisabled and OnModuleRecovered on the world's tick-boundary node; OnModuleReloaded on the tick-boundary node,
///   or the main thread for an out-of-tick go-live.
/// Invalidation: None.
/// Units: Ticks.
/// Errors: None; the listener logs.
/// Reentrancy: NONE. A listener never calls back into the script world; it records flags the frame loop reads.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_ScriptWorldListener
{
public:
	virtual ~Zenith_ScriptWorldListener() = default;
	virtual void OnWorldHalted(uint16 uWorldTag, uint64 ulTick, const Zenith_ScriptError& xError) = 0;
	virtual void OnModuleDisabled(uint16 uWorldTag, uint64 ulTick, Zenith_ScriptModuleID xModule, const Zenith_ScriptError& xFirstFailure) = 0;
	virtual void OnModuleRecovered(uint16 uWorldTag, uint64 ulTick, Zenith_ScriptModuleID xModule) = 0;
	// called at go-live for each reload generation that went live, in ascending module ID; the default does nothing
	virtual void OnModuleReloaded(uint16 uWorldTag, uint64 ulTick, Zenith_ScriptModuleID xModule, uint32 uGeneration) {}
};

/// @contract interface Zenith_ScriptPublicationObserver
/// Ownership: Implemented by the runtime's replay recorder (over ZenithSave's ZRPL writer); borrowed by the world.
/// Thread affinity: ANY_THREAD. Called on the world's tick-boundary node, step 1.
/// Invalidation: axPublished is valid for the call.
/// Units: Ticks; completion IDs.
/// Errors: None; a recorder failure is the recorder's.
/// Reentrancy: NONE.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_ScriptPublicationObserver
{
public:
	virtual ~Zenith_ScriptPublicationObserver() = default;
	virtual void OnPublished(uint16 uWorldTag, uint64 ulTick, Zenith_Span<const Zenith_ScriptPublication> axPublished) = 0;
};

/// @contract interface Zenith_ScriptPublicationSource
/// Ownership: Implemented by the runtime's replay reader under --replay; borrowed by the world.
/// Thread affinity: ANY_THREAD. GetRecorded on the world's tick-boundary node, step 1, and on the main thread in the frame
///   prologue (through Zenith_ScriptRuntime::CanRunTick); the two never overlap.
/// Invalidation: The returned span is valid until the next call.
/// Units: Ticks.
/// Errors: A log that has ended reports an empty span.
/// Reentrancy: NONE.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_ScriptPublicationSource
{
public:
	virtual ~Zenith_ScriptPublicationSource() = default;
	virtual Zenith_Span<const Zenith_ScriptPublication> GetRecorded(uint16 uWorldTag, uint64 ulTick) = 0;
	// The recorded next completion ID of the world's REPLAY_RECORD_TICK record for ulTick, 0 when the log holds none; called
	// at tick-boundary step 1 before GetRecorded, and the table adopts it ([Z2-N-ARCH-794]).
	virtual uint64 GetRecordedNextCompletionID(uint16 uWorldTag, uint64 ulTick) = 0;
};

/// @contract interface Zenith_AsyncPublicationHandler
/// Ownership: Implemented by a producer whose results C++ consumes at publication (the PLAT bridge for ASYNC_SOURCE_PLATFORM,
///   ZenithRuntime's SCENE forwarder for ASYNC_SOURCE_SCENE_LOAD); registered per source kind with
///   Zenith_AsyncResultTable::SetHandler before the first tick.
/// Thread affinity: ANY_THREAD. Called on the world's tick-boundary node, step 1.
/// Invalidation: xPublication's payload is valid for the call.
/// Units: None.
/// Errors: None.
/// Reentrancy: NONE. It writes world state only through xWorld (property reads and writes, never structure).
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_AsyncPublicationHandler
{
public:
	virtual ~Zenith_AsyncPublicationHandler() = default;
	virtual void OnPublished(const Zenith_ScriptPublication& xPublication, Zenith_TickBoundaryAccess& xWorld) = 0;
};
```

### ZenithScriptCompiler public types

```cpp
// Zenith2/Engine/ZenithScriptCompiler/Zenith_ScriptCompile.h and Zenith_ScriptStateless.h
// (Zenith_ScriptCompileOptions and Zenith_ScriptBindingSetHash are ZenithScript's: Scripting.md → "Shipping bytecode compatibility")
struct Zenith_ScriptModuleSource
{
	Zenith_ScriptModuleID m_xModule;
	Zenith_ScriptModuleKind m_eKind = SCRIPT_MODULE_KIND_LIBRARY;
	const char* m_szPath = nullptr;          // manifest path, UTF-8
	uint32 m_uPathBytes = 0;
	const char* m_szSource = nullptr;        // UTF-8 source, at most uSCRIPT_MAX_SOURCE_BYTES
	uint32 m_uSourceBytes = 0;
	Zenith_Span<const Zenith_ScriptModuleID> m_axDependencies;   // resolved require literals, in source order
};

/// @contract type Zenith_ScriptCompileReport
/// Ownership: Caller-owned; filled by Zenith_ScriptCompileModule and Zenith_ScriptCheckStateless; holds up to 64 diagnostics in fixed storage.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None.
/// Units: 1-based lines and columns.
/// Errors: Diagnostics beyond 64 are counted in GetDroppedCount and not stored.
/// Version: since Z2 Stage 1.
/// @endcontract
class Zenith_ScriptCompileReport
{
public:
	uint32 GetCount() const;
	const Zenith_ScriptDiagnostic& Get(uint32 uIndex) const;
	uint32 GetDroppedCount() const;
	void Add(const Zenith_ScriptDiagnostic& xDiagnostic);
	void Clear();
};

class Zenith_ScriptAcceptedModuleSet       // library modules already accepted by the stateless check for this game
{
public:
	void Add(Zenith_ScriptModuleID xModule);
	bool Contains(Zenith_ScriptModuleID xModule) const;
	uint32 GetCount() const;
private:
	Zenith_BitArray m_xAccepted;             // one bit per manifest entry (a CORE container; the name is pending CORE)
};
```

### ZenithScript internal types

```cpp
// Zenith2/Engine/ZenithScript/Internal/Zenith_ScriptVM.h
struct Zenith_ScriptVMDesc
{
	Zenith_ScriptVMRole m_eRole = SCRIPT_VM_ROLE_MAIN;
	uint16 m_uVMIndex = 0;                  // 0 main VM, 1 + worker index
	uint64 m_ulMemoryCapBytes = 0;
	Zenith_ScriptWorld* m_pxWorld = nullptr; // borrowed: module set, manifest, bindings, breakpoints
	Zenith_ScriptGCTuning m_xGCTuning;
	Zenith_ScriptNativePolicy m_eNativePolicy = SCRIPT_NATIVE_POLICY_TOOLS_ONLY;
	Zenith_ScriptWatchdog* m_pxWatchdog = nullptr;
};

// Zenith2/Engine/ZenithScript/Internal/Zenith_ScriptTransaction.h
struct Zenith_ScriptSystemRef
{
	Zenith_ScriptModuleID m_xModule;
	uint32 m_uSystemOrder = 0;              // Scripting.md → "Schedule compilation of script nodes"
	uint64 m_ulSourceID = 0;                // Hash64 of the declaration's Name
	uint32 m_uDeclarationIndex = 0;         // into the world's table of validated declarations of the current module generation
};

// Zenith2/Engine/ZenithScript/Internal/Zenith_ScriptDiagnostics.h (no Luau types; read by the profiler overlay and the tests)
struct Zenith_ScriptCounters
{
	uint64 m_ulInvocations = 0;
	uint64 m_ulEntityTransactions = 0;
	uint64 m_aulFailuresByKind[12] = {};    // indexed by Zenith_ScriptErrorKind
	uint64 m_aulRejectionsByClass[SCRIPT_CAPACITY_CLASS_COUNT] = {};
	uint32 m_uQuarantinedInstances = 0;
	uint32 m_uDisabledModules = 0;
	uint64 m_ulCheckpoints = 0;
	uint64 m_ulCheckpointBytes = 0;
	uint64 m_ulSnapshotBytes = 0;
	uint64 m_ulSafepoints = 0;
	uint64 m_ulGCKilobytes = 0;               // script.gc.kb
	uint64 m_ulGCNanoseconds = 0;             // script.gc.ns
	uint64 m_ulTxnArenaGrowths = 0;           // script.txn.arena_growths (cumulative)
	uint64 m_ulDiagnosticsDropped = 0;        // script.diagnostics_dropped (cumulative)
	uint64 m_ulUnknownWaits = 0;              // script.wait.unknown (cumulative)
	uint32 m_auPublishedUnconsumed[uSCRIPT_ASYNC_SOURCE_KIND_CAPACITY] = {};  // script.async.published_unconsumed.<kind>, indexed by the Zenith_AsyncSourceKind value
};

struct Zenith_ScriptDiagnostics
{
	static uint32 GetLiveDispatchScopes(const Zenith_ScriptVM& xVM);
	static uint32 GetProtectedDepth(const Zenith_ScriptVM& xVM);
	static uint64 GetFrameArenaMark(const Zenith_ScriptVM& xVM);
	static uint16 GetVMIndex(const Zenith_ScriptVM& xVM);
	static uint32 GetWorkerVMCount(const Zenith_ScriptWorld& xWorld);
	static const Zenith_ScriptCounters& GetTickCounters(const Zenith_ScriptWorld& xWorld);   // counters of the last completed tick
};
```

- [Z2-N-SCRIPT-901] Every type in this file is declared at the path its code block names, with exactly these members; a change to a public type here follows the source-API rule of `Zenith2/Docs/Contracts.md` → "Compatibility policy per kind" and is edited in this file in the same diff.
- [Z2-N-SCRIPT-902] `Zenith_ScriptDiagnostics` is `Internal/` and reachable only by ZenithScript itself, its tests and the profiler overlay through the counters it copies out; it holds no mutable global state (every accessor reads the VM or world passed to it).

### Types owned by other libraries

| Type | Owning library | Specified in |
|---|---|---|
| `Zenith_Result<T, E>`, `Zenith_Owned<T>`, `Zenith_Span<T>`, `Zenith_ByteSpan`, `Zenith_Arena`, `Zenith_ArenaMarker`, `Zenith_BitArray`, `Zenith_ReentrancyGuard`, `Zenith_PersistentID`, `Zenith_Hash64`, `Zenith_IsShippingBuild`, `Zenith_BuildPlatform`, `Zenith_StringView`, `Zenith_Heap`, `Zenith_FaultInjector`, `Zenith_ConfigSchema`, `Zenith_ConfigError` | ZenithCore | `Zenith2/Docs/Contracts.md` → "Ownership and allocation"; `Zenith2/Docs/Design/CORE.md` |
| `Zenith_TypeID`, `Zenith_PropertyID` (`ZenithReflect/Zenith_ReflectIDs.h`), `Zenith_TypeRegistry` | ZenithReflect | `Zenith2/Docs/Contracts.md` → "Reflection"; `Zenith2/Docs/Architecture.md` → "Reflection and runtime type registration" |
| `Zenith_World`, `Zenith_WorldSimulation`, `Zenith_EntityHandle`, `Zenith_ChunkView`, `Zenith_RawColumn` (with `MarkRowChanged`), `Zenith_InvocationScope`, `Zenith_SystemContext`, `Zenith_ContainerRef`, `Zenith_SystemRegistry`, `Zenith_SystemReloadScope`, `Zenith_MergeContext`, `Zenith_TickBoundaryAccess` | ZenithECS | `Zenith2/Docs/Architecture.md` → "ECS model"; `Zenith2/Docs/Architecture.md` → "Data-defined components" |
| `Zenith_JobSystem`, `Zenith_FrameGraphBuilder` | ZenithJobs | `Zenith2/Docs/Architecture.md` → "CPU frame graph and scheduler" |
| `Zenith_AssetHandle<T>` | ZenithAssets | `Zenith2/Docs/Design/ASSET.md` |
