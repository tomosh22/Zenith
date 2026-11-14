# SCRIPT design — public API

Companion to `Zenith2/Docs/Design/SCRIPT.md`: every public header of `ZenithScript` and `ZenithScriptCompiler`, and SCRIPT's own header `ZenithReflect/Zenith_ScriptBinding.h`, with full declarations and one contract block per type in the format of `Zenith2/Docs/Contracts.md` → "Contract blocks per type". Where `Zenith2/Docs/Scripting/PublicTypes.md` → "Public types", `Zenith2/Docs/Scripting/Bindings.md` → "Binding registration and the thunk ABI" or `Zenith2/Docs/Scripting/LibraryLayout.md` → "Library layout and file map" already declares a member, this file cites that declaration and adds only the contract block and the members marked `// elaboration`; a change to either is edited in both files in one diff ([Z2-N-SCRIPT-901]). `<!-- assumption: pending <AREA> -->` marks an interface assumed from another area's design; each is a row of the cross-area notes.

## Header list

| Header | Library | Declares | Section |
|---|---|---|---|
| `Zenith2/Engine/ZenithScript/Zenith_ScriptTypes.h` | ZenithScript | limits, IDs, module kinds, view kinds, instance states, `Zenith_ScriptNativePolicy`, the module manifest, the bytecode header and bytecode, GC tuning, the watchdog | § "Zenith_ScriptTypes header" |
| `Zenith2/Engine/ZenithScript/Zenith_ScriptError.h` | ZenithScript | `Zenith_ScriptErrorCode`, `Zenith_ScriptError`, `Zenith_ScriptDiagnostic`, `Zenith_ScriptErrorCodeFromKind`, `Zenith_ToString` | § "Zenith_ScriptError header" |
| `Zenith2/Engine/ZenithScript/Zenith_ScriptRecovery.h` | ZenithScript | `Zenith_ScriptRecoveryPolicy`, `Zenith_ScriptRecoverySettings`, `Zenith_ScriptRecoveryResolve` | § "Zenith_ScriptRecovery header" |
| `Zenith2/Engine/ZenithScript/Zenith_ScriptWorld.h` | ZenithScript | `Zenith_ScriptWorldState`, `Zenith_ScriptWorldDesc`, `Zenith_ScriptSafepointLimits`, `Zenith_ScriptCapacityLimits`, `Zenith_ScriptWorld` | § "Zenith_ScriptWorld header" |
| `Zenith2/Engine/ZenithScript/Zenith_ScriptRuntime.h` | ZenithScript | `Zenith_ScriptDebuggerDesc`, `Zenith_ScriptRuntimeDesc`, `Zenith_ScriptRuntime`, `Zenith_ScriptRegisterEngineTypes`, `Zenith_ScriptRegisterBehaviourStateTypes`, `Zenith_RegisterScriptConfigKeys` | § "Zenith_ScriptRuntime header" |
| `Zenith2/Engine/ZenithScript/Zenith_ScriptHost.h` | ZenithScript | the upward interfaces SCRIPT declares: `Zenith_ScriptWorldListener`, `Zenith_ScriptPublicationObserver`, `Zenith_ScriptPublicationSource`, `Zenith_AsyncPublicationHandler` | § "Zenith_ScriptHost header" |
| `Zenith2/Engine/ZenithScript/Zenith_ScriptAsync.h` | ZenithScript | `Zenith_ScriptPublication`, `Zenith_AsyncTableError`, `Zenith_PfnAsyncPublicationResolve`, `Zenith_AsyncTableEntry`, `Zenith_AsyncResultTable` | § "Zenith_ScriptAsync header" |
| `Zenith2/Engine/ZenithScript/Zenith_ScriptStateSnapshot.h` | ZenithScript | `Zenith_ScriptSavedState`, `Zenith_ScriptStateSnapshot` | § "Zenith_ScriptStateSnapshot header" |
| `Zenith2/Engine/ZenithScript/Zenith_ScriptBehaviourComponent.h` | ZenithScript | `Zenith_BehaviourComponent` | § "Zenith_ScriptBehaviourComponent header" |
| `Zenith2/Engine/ZenithScript/Zenith_ScriptCompileOptions.h` | ZenithScript | `Zenith_ScriptCompileOptions`, `Zenith_ScriptSandboxMutableGlobals`, `Zenith_ScriptProductionCompileOptions`, `Zenith_ScriptBindingSetHash`, `Zenith_ScriptEngineVersionHash` | § "Zenith_ScriptCompileOptions header" |
| `Zenith2/Engine/ZenithScript/Zenith_ScriptTypesEmitter.h` | ZenithScript | `Zenith_ScriptDefinitionFile`, `Zenith_PfnDefinitionSink`, `Zenith_ScriptEmitDefinitions` | § "Zenith_ScriptTypesEmitter header" |
| `Zenith2/Engine/ZenithReflect/Zenith_ScriptBinding.h` | ZenithReflect (a SCRIPT-owned file) | the Luau-free binding ABI | § "Zenith_ScriptBinding header" |
| `Zenith2/Engine/ZenithScriptCompiler/Zenith_ScriptCompile.h` | ZenithScriptCompiler | `Zenith_ScriptModuleSource`, `Zenith_ScriptCompileReport`, `Zenith_ScriptCompileModule`, `Zenith_ScriptFastcallSite`, `Zenith_ScriptListFastcallSites` | § "Zenith_ScriptCompile header" |
| `Zenith2/Engine/ZenithScriptCompiler/Zenith_ScriptStateless.h` | ZenithScriptCompiler | `Zenith_ScriptStatelessDiagnostic`, `Zenith_ScriptStatelessVerdict`, `Zenith_ScriptAcceptedModuleSet`, `Zenith_ScriptCheckStateless` | § "Zenith_ScriptStateless header" |
| `Zenith2/Engine/ZenithScriptCompiler/Zenith_ScriptCook.h` | ZenithScriptCompiler | `Zenith_ScriptCookDesc`, `Zenith_ScriptCookResult`, `Zenith_ScriptCookGame`, `Zenith_ScriptExtractDeclaration`, `Zenith_ScriptLintModule` | § "Zenith_ScriptCook header" |

Every fallible function in these headers returns a `Zenith_Result`; no header includes a Luau header, names a Luau type, names an STL type or includes a ZenithScript `Internal/` header ([Z2-N-SCRIPT-882], [Z2-N-SCRIPT-045], [Z2-N-CONTRACT-272]).

## Zenith_ScriptTypes header

The declarations of `Zenith2/Docs/Scripting/PublicTypes.md` → "ZenithScript public types" verbatim, plus the contract blocks and members below.

```cpp
// Zenith2/Engine/ZenithScript/Zenith_ScriptTypes.h (additions to the PublicTypes.md declarations)

// elaboration: limits the design relies on
inline constexpr uint32 uSCRIPT_MAX_WORKER_VM_SLOTS = 254;           // frame threads per world: VM index stays below 0xFFFF and below 256 for DAP thread IDs (JOBS allows at most 63)
inline constexpr uint16 uSCRIPT_RESERVED_VM_INDEX = 0xFFFFu;         // ECS playback scopes (Design/ECS/WorldAPI.md → "Zenith_ChunkView header")
inline constexpr uint32 uSCRIPT_DIAGNOSTIC_RING_ENTRIES = 64;        // per world, and one ring in the runtime
inline constexpr uint32 uSCRIPT_MAX_DECLARED_WRITE_PROPERTIES = 64;  // per declaration, after expansion to properties
inline constexpr uint32 uSCRIPT_MAX_STATE_FIELDS = 256;              // per state schema, nested fields included
inline constexpr uint32 uSCRIPT_MAX_NESTING = 200;                   // syntactic nesting, Scripting.md → "Untrusted inputs"
inline constexpr uint32 uSCRIPT_SAMPLE_RING_ENTRIES = 65536;         // sampling profiler, Scripting.md → "Sampling profiler"
inline constexpr uint32 uSCRIPT_SAMPLE_PERIOD = 997;                 // safepoints between samples
inline constexpr uint16 uSCRIPT_DAP_DEFAULT_PORT = 27851;
inline constexpr uint32 uSCRIPT_WATCHDOG_MAX_WORLDS = 64;            // world indices of the watchdog (a uint8 index; 64 × 64 entries bound the table)
inline constexpr uint32 uSCRIPT_ASYNC_STALE_TICKS = 600;             // age of a published-unconsumed result that SCRIPT_ASYNC_CAPACITY_HELD reports
inline constexpr uint64 ulSCRIPT_TXN_RETENTION_BLOCK_BYTES = 256ull * 1024;   // retention arena block (Transactions.md → "Transaction arena partition")
inline constexpr uint32 uSCRIPT_TXN_RETAINED_BLOCKS = 1;             // blocks a retention arena keeps across Reset (one: the ceiling's tail bound, Transactions.md → "Retention ceiling derivation")
inline constexpr uint32 uSCRIPT_RECOVERY_ORDINAL = 0xFFFFFFFFu;      // the reserved ordinal of every recovery invocation (OnRecover); never an ordinary ordinal (HotReload.md → "Recovery invocations and exact restore")

/// @contract enum Zenith_ScriptNativePolicy
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3. The values of Zenith2/Docs/Scripting.md → "Native codegen conformance"; the value in force per
///   platform is recorded in the Luau entry of ThirdParty/manifest.json and passed by the composition root.
/// @endcontract
enum Zenith_ScriptNativePolicy : uint8
{
	SCRIPT_NATIVE_POLICY_DISABLED,
	SCRIPT_NATIVE_POLICY_TOOLS_ONLY,
	SCRIPT_NATIVE_POLICY_ALL,
};
```

Contract blocks for the PublicTypes.md declarations of this header, one immediately before each declaration ([Z2-N-CONTRACT-111]):

```cpp
/// @contract value Zenith_ScriptModuleID
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: a handle is copy-assigned and stored in containers and in other types' members.
/// Invalidation: Valid for the module manifest that issued it; module IDs are sorted-path indices and change when a module
///   is added or removed, so they are never persisted (saves key behaviour state by stable source ID).
/// Units: An index into the game's module manifest.
/// Errors: uSCRIPT_INVALID_MODULE is the invalid value.
/// Version: since Z2 Stage 3.
/// @endcontract

/// @contract value Zenith_ScriptInvocationID
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: a handle is copy-assigned and stored in containers and in other types' members.
/// Invalidation: None; never reused within a world session ([Z2-N-SCRIPT-862]).
/// Units: Bits 63..48 the VM index (0 main VM, 1 + worker slot), bits 47..0 the per-VM counter, which starts at 1, so 0 is
///   never issued; VM index 0xFFFF is never issued (it names ECS playback scopes).
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract

/// @contract value Zenith_ScriptLogicalID
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: a handle is copy-assigned and stored in containers and in other types' members.
/// Invalidation: None.
/// Units: A stable simulation ID and an invocation ordinal (Scripting.md → "Ordering keys").
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract

/// @contract enum Zenith_ScriptModuleKind
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract

/// @contract enum Zenith_ScriptInstanceState
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract

/// @contract enum Zenith_ScriptViewKind
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract

/// @contract value Zenith_ScriptModuleManifestEntry
/// Ownership: Plain value inside a Zenith_ScriptModuleManifest.
/// Thread affinity: IMMUTABLE. Members non-const: Load decodes each entry field by field.
/// Invalidation: With its manifest.
/// Units: Offsets and sizes in bytes into the manifest's blocks.
/// Errors: None; Load validated every field.
/// Version: since Z2 Stage 3. The serialized form is Design/SCRIPT/Compiler.md → "Module manifest format".
/// @endcontract

/// @contract value Zenith_ScriptBytecodeHeader
/// Ownership: Plain value decoded field by field from a .zlbc container (never copied as a C++ layout, Z2-D-038).
/// Thread affinity: IMMUTABLE. Members non-const: decoded field by field.
/// Invalidation: None.
/// Units: Hashes are Zenith_Hash64 values; sizes in bytes.
/// Errors: None; the loader's verification of Scripting.md → "Load-time verification" reports mismatches.
/// Version: since Z2 Stage 3. Container version 1.
/// @endcontract

/// @contract value Zenith_ScriptBytecode
/// Ownership: m_xBlob borrows the arena its producer was given; the header is a copy.
/// Thread affinity: IMMUTABLE. Members non-const: its producer fills it as an out-parameter.
/// Invalidation: m_xBlob is invalid when its arena is reset.
/// Units: Bytes.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract

/// @contract value Zenith_ScriptGCTuning
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: filled from the budget rows at world creation.
/// Invalidation: None.
/// Units: Percent, multiplier and KB, from Z2-B-SCRIPT-017.
/// Errors: A zero field is SCRIPT_ERROR_INVALID_DESC at world creation.
/// Version: since Z2 Stage 3.
/// @endcontract
```

`Zenith_ScriptModuleManifest` and `Zenith_ScriptWatchdog` keep the contract blocks of `Zenith2/Docs/Scripting/PublicTypes.md` → "ZenithScript public types". Added members:

<!-- assumption: pending core-doc (X121) -->
The watchdog descriptor replaces PublicTypes.md's `Create(uint32 uLimitMs, uint32 uPollMs)`, `Register` fails with `SCRIPT_ERROR_WATCHDOG_TABLE_FULL` instead of `SCRIPT_ERROR_KIND_INTERNAL`, the table is sized by the capacity rule of Design/SCRIPT/Runtime.md → "Watchdog internals" instead of a fixed 256 entries, and `SuspendWorld(uint32 uWorldIndex, bool)` is removed: a debugger stop exempts only the stopped VM, through the VM's own flag and a rebase of its start time (Design/SCRIPT/Debugger.md → "Stop protocol" rule 5), so no watchdog member serves it; the manifest accessors are additions. These declaration changes are mirrored into PublicTypes.md by cross-doc rows X121, X151 (4) and X158 ([Z2-N-SCRIPT-901]).
<!-- /assumption -->

```cpp
// elaboration: Zenith_ScriptModuleManifest
	Zenith_Span<const Zenith_ScriptStateSchemaRecord> GetStateSchemas() const;   // cooked state schemas, module-ID order
	Zenith_StringView GetPath(Zenith_ScriptModuleID xModule) const;
	uint64 GetBindingSetHash() const;                                        // the cook's value; compared at Load
	uint64 GetEngineVersionHash() const;
	// the module's deprecated-call records (Compiler.md → "Module manifest format", the deprecated-call block), ascending line;
	// empty for a module that calls no deprecated binding (cross-area row MT2 (c); core text cross-doc row X451)
	Zenith_Span<const Zenith_ScriptDeprecatedCallRecord> GetDeprecatedCalls(Zenith_ScriptModuleID xModule) const;

// elaboration: one deprecated call site of a cooked module, decoded from the manifest; contract block value, IMMUTABLE
// (members non-const: decoded into the manifest's array), invalidated with its manifest, since Z2 Stage 3
struct Zenith_ScriptDeprecatedCallRecord
{
	Zenith_ScriptFunctionID m_xFunction;       // the deprecated binding the line calls (the first in AST pre-order on that line)
	uint32 m_uLine = 0;                       // 1-based source line
};

// elaboration: Zenith_ScriptWatchdog::Create takes a descriptor so it carries a fault injector ([Z2-N-CONTRACT-225])
/// @contract value Zenith_ScriptWatchdogDesc
/// Ownership: Plain value; m_pxHeap, m_pxFaults and m_pxCrashHandler are borrowed for the watchdog's lifetime.
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct it and set the fields they need.
/// Invalidation: None; Create copies every member it keeps.
/// Units: m_uLimitMs and m_uPollMs milliseconds; m_uMaxWorlds worlds; m_uFrameThreadCount frame threads.
/// Errors: Zenith_ScriptWatchdog::Create returns SCRIPT_ERROR_INVALID_DESC for a zero limit, poll or frame-thread count, or
///   m_uMaxWorlds outside 1 … uSCRIPT_WATCHDOG_MAX_WORLDS.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_ScriptWatchdogDesc
{
	uint32 m_uLimitMs = 0;                    // Z2-B-SCRIPT-003
	uint32 m_uPollMs = 10;
	uint32 m_uMaxWorlds = 16;                 // 1 … uSCRIPT_WATCHDOG_MAX_WORLDS
	uint32 m_uFrameThreadCount = 0;           // Zenith_JobSystem::GetFrameThreadCount(); the table holds m_uMaxWorlds × (1 + this) entries
	Zenith_Heap* m_pxHeap = nullptr;
	Zenith_FaultInjector* m_pxFaults = nullptr;
	Zenith_CrashHandler* m_pxCrashHandler = nullptr;         // DEVT's crash handler (ZenithPlatform); null in sanitizer builds and tests
};
	static Zenith_Result<Zenith_Owned<Zenith_ScriptWatchdog>, Zenith_ScriptError> Create(const Zenith_ScriptWatchdogDesc& xDesc);
		// SCRIPT_ERROR_INVALID_DESC for a zero limit, poll or frame-thread count, or m_uMaxWorlds outside 1 … 64
	Zenith_Result<void, Zenith_ScriptError> Register(Zenith_ScriptVM& xVM);
		// a main VM takes a world index: SCRIPT_ERROR_WATCHDOG_TABLE_FULL when all m_uMaxWorlds are in use; a worker VM never fails
	// no SuspendWorld: the poll skips a VM whose m_bDebuggerStopped is set (Design/SCRIPT/Runtime.md → "Watchdog internals")
```

`Zenith_ScriptStateSchemaRecord` is the decoded state-schema record of Design/SCRIPT/Compiler.md → "Module manifest format" (module ID, stable name, `StateVersion`, field range into the manifest's field table, maximum serialized bytes, shape hash); its contract block is `value`, `IMMUTABLE`, invalidated with its manifest, `since Z2 Stage 3`. The watchdog's fault points: `uFAULT_POINT_SCRIPT_WATCHDOG_THREAD = 0x53430020u`, `uFAULT_POINT_SCRIPT_WATCHDOG_TABLE = 0x53430021u`.

## Zenith_ScriptError header

<!-- assumption: pending core-doc (X115) -->
This section's `Zenith_ScriptErrorCode`, the 32-byte `Zenith_ScriptError` and `Zenith_ScriptDiagnostic` replace the declaration of `Zenith2/Docs/Scripting.md` → "The protected entry"; the Scripting companions adopt them with cross-doc row X115 ([Z2-N-SCRIPT-901] requires both to change in one diff, which a design cannot do alone).
<!-- /assumption -->

`Zenith2/Docs/Scripting.md` → "The protected entry" declares `Zenith_ScriptError` with a 512-byte message and a 2,048-byte traceback. `Zenith_Result<T, E>` statically requires `E` to be an error enumeration or a trivially copyable struct of at most 32 bytes whose `m_eCode` is an enumeration numbered from 1 and which carries no string ([Z2-N-CONTRACT-480], [Z2-N-CONTRACT-482], [Z2-N-CONTRACT-465]), so that declaration does not compile as a `Zenith_Result` error. This design splits it, following the configuration-diagnostic and graph-report precedents: the 32-byte `Zenith_ScriptError` is the result error, and the text lives in a `Zenith_ScriptDiagnostic` read from a diagnostic store (§ "Zenith_ScriptWorld header", `GetDiagnostic`; Design/SCRIPT/Transactions.md → "Diagnostic store implementation"). Reconciliation item S1 of `Zenith2/Docs/Design/SCRIPT.md` → "Reconciliation items" routes the matching text change to `Scripting.md`.

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
/// Errors: None; meanings in Design/SCRIPT.md → "Error enumerations".
/// Version: since Z2 Stage 3. Numbered from 1 ([Z2-N-CONTRACT-465], [Z2-N-CONTRACT-482]); values never renumbered.
/// @endcontract
enum Zenith_ScriptErrorCode : uint8
{
	SCRIPT_ERROR_RUNTIME = 1, SCRIPT_ERROR_MISUSE, SCRIPT_ERROR_BUDGET, SCRIPT_ERROR_SNAPSHOT_CAP, SCRIPT_ERROR_RESERVATION,
	SCRIPT_ERROR_YIELD_IN_SYSTEM, SCRIPT_ERROR_CHECKPOINT, SCRIPT_ERROR_VM_MEMORY, SCRIPT_ERROR_WATCHDOG, SCRIPT_ERROR_INTERNAL,
	SCRIPT_ERROR_COMPILE, SCRIPT_ERROR_LOAD,                 // 1..12: one per Zenith_ScriptErrorKind, in kind order
	SCRIPT_ERROR_INVALID_DESC,                               // 13
	SCRIPT_ERROR_OUT_OF_MEMORY,                              // 14
	SCRIPT_ERROR_WORLD_HALTED,                               // 15
	SCRIPT_ERROR_UNKNOWN_MODULE,                             // 16
	SCRIPT_ERROR_NOT_STATELESS_VERIFIED,                     // 17
	SCRIPT_ERROR_RELOAD_QUEUE_FULL,                          // 18
	SCRIPT_ERROR_WATCHDOG_TABLE_FULL,                        // 19
	SCRIPT_ERROR_DEBUGGER_UNAVAILABLE,                       // 20: only when Zenith_ScriptDebuggerDesc::m_bRequired
	SCRIPT_ERROR_BYTECODE_MISMATCH,                          // 21
};

/// @contract value Zenith_ScriptError
/// Ownership: Plain value, 32 bytes.
/// Thread affinity: IMMUTABLE.
///   Members non-const: it is the E of a Zenith_Result, whose move assignment assigns it, and a member of Zenith_ScriptDiagnostic.
/// Invalidation: m_uDiagnosticSequence names an entry of its world's (or the runtime's) diagnostic store; after 64 newer
///   diagnostics it is overwritten and GetDiagnostic reports false.
/// Units: m_uLine is a 1-based source line (0 unknown).
/// Errors: m_eCode is never 0. For a Luau error m_eCode == Zenith_ScriptErrorCodeFromKind(m_eKind); a C++-API failure
///   (codes from SCRIPT_ERROR_INVALID_DESC on) carries m_eKind = SCRIPT_ERROR_KIND_LOAD.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_ScriptError
{
	Zenith_ScriptErrorCode m_eCode = SCRIPT_ERROR_RUNTIME;     // first member ([Z2-N-CONTRACT-480])
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
/// Thread affinity: IMMUTABLE. Members non-const: an element of the diagnostic ring, formatted in place.
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

The enumerators' meanings are `Zenith2/Docs/Design/SCRIPT.md` → "Error enumerations". The diagnostic store is written as Design/SCRIPT/Transactions.md → "Diagnostic store implementation" states; `m_uDiagnosticSequence` is valid when the caller sees the error, because the protected call writes the store before it returns.

One struct-form error per code enumeration ([Z2-N-CONTRACT-482]): SCRIPT's C++ API has exactly one, `Zenith_ScriptError` with `Zenith_ScriptErrorCode`, declared here. The binding ABI in ZenithReflect is a subsystem of its own and has two error types, each in one of [Z2-N-CONTRACT-482]'s forms with its own name and prefix: the struct `Zenith_ScriptThunkError` with its code enumeration `Zenith_ScriptThunkErrorCode` (`SCRIPT_THUNK_ERROR_`), and the enumeration `Zenith_ScriptBindingError` (`SCRIPT_BINDING_ERROR_`) of `Zenith_ScriptBindingTable::Seal` (§ "Zenith_ScriptBinding header"). No error type shares another's code enumeration, so the Contracts question cross-doc row X115 records — whether [Z2-N-CONTRACT-482] admits a second struct sharing `Zenith_ScriptErrorCode` — does not arise and needs no Contracts change; X115 carries the Scripting companions' patches alone.

## Zenith_ScriptRecovery header

The header `Zenith2/Docs/Design/MOD.md` → "Zenith_GameComposition.h" includes (cross-area note M16). It holds the policy of `Zenith2/Docs/Scripting.md` → "Recovery policy" and the values `game.json`'s `scriptRecovery` carries.

```cpp
// Zenith2/Engine/ZenithScript/Zenith_ScriptRecovery.h
#pragma once
#include "ZenithCore/Zenith_Types.h"

/// @contract enum Zenith_ScriptRecoveryPolicy
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3. The enumerators of Zenith2/Docs/Scripting.md → "Recovery policy".
/// @endcontract
enum Zenith_ScriptRecoveryPolicy : uint8
{
	SCRIPT_RECOVERY_POLICY_MANUAL,
	SCRIPT_RECOVERY_POLICY_AUTOMATIC,
};

/// @contract value Zenith_ScriptRecoverySettings
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct it and set the fields they need.
/// Invalidation: None.
/// Units: Ticks; recoveries per module per world session.
/// Errors: MOD validates game.json (delayTicks 1-3600, maxRecoveriesPerModule 0-100); a value outside those ranges passed by
///   C++ is SCRIPT_ERROR_INVALID_DESC at Zenith_ScriptRuntime::Create.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_ScriptRecoverySettings
{
	Zenith_ScriptRecoveryPolicy m_ePolicy = SCRIPT_RECOVERY_POLICY_MANUAL;
	uint32 m_uDelayTicks = 60;
	uint32 m_uMaxRecoveriesPerModule = 3;
};

// The settings when game.json carries no scriptRecovery: manual in every configuration other than Shipping; automatic with
// 60 ticks and 3 recoveries in Shipping (cross-area note M16, confirmed by this design).
Zenith_ScriptRecoverySettings Zenith_ScriptRecoveryResolve(bool bDeclared, Zenith_ScriptRecoveryPolicy ePolicy,
	uint32 uDelayTicks, uint32 uMaxRecoveries);
```

`Zenith_ScriptWorldDesc`'s `m_eRecoveryPolicy`, `m_uRecoveryDelayTicks` and `m_uMaxRecoveriesPerModule` (LibraryLayout.md) are filled from one `Zenith_ScriptRecoverySettings`.

## Zenith_ScriptWorld header

`Zenith_ScriptWorldState`, `Zenith_ScriptWorldDesc` and `Zenith_ScriptWorld` are `Zenith2/Docs/Scripting/LibraryLayout.md` → "Public API of Zenith_ScriptWorld", with its contract block. This design changes one signature and adds members:

<!-- assumption: pending core-doc (X121) -->
Every member marked `// elaboration` below is a declaration change LibraryLayout.md adopts with cross-doc row X121 (and `RegisterScriptNodes` with X86). `m_uFrameThreadCount` replaces LibraryLayout.md's `m_uWorkerCount`: they name one quantity, the desc keeps one member, and `Create` step 1 reads it (Design/SCRIPT/Runtime.md → "Script world internals"). `m_pxWorld` stays: `m_pxSimulation` is the simulation of that world.
<!-- /assumption -->

```cpp
// Zenith2/Engine/ZenithScript/Zenith_ScriptWorld.h (additions)
class Zenith_SystemRegistry;       // ZenithECS
class Zenith_WorldSimulation;      // ZenithECS
class Zenith_TypeRegistry;         // ZenithReflect
class Zenith_ScriptWorldListener;  // Zenith_ScriptHost.h
class Zenith_ScriptPublicationObserver;
class Zenith_ScriptPublicationSource;
class Zenith_AsyncResultTable;     // Zenith_ScriptAsync.h
class Zenith_ScriptStateSnapshot;  // Zenith_ScriptStateSnapshot.h
struct Zenith_ScriptSavedState;

/// @contract value Zenith_ScriptSafepointLimits
/// Ownership: Plain value, a member of Zenith_ScriptWorldDesc.
/// Thread affinity: IMMUTABLE. Members non-const: filled from the budget rows.
/// Invalidation: None; Zenith_ScriptWorld::Create copies it.
/// Units: Safepoints (interrupt callbacks counted per Zenith2/Docs/Scripting.md → "Safepoint counting"), per member row.
/// Errors: Zenith_ScriptWorld::Create returns SCRIPT_ERROR_INVALID_DESC when any member is zero.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_ScriptSafepointLimits
{
	uint64 m_ulBehaviourPerInvocation = 0;   // Z2-B-SCRIPT-001
	uint64 m_ulChunkSystemPerEntity = 0;     // Z2-B-SCRIPT-002
	uint64 m_ulRecoveryPerStep = 0;          // Z2-B-SCRIPT-018
	uint64 m_ulModuleLoad = 0;               // Z2-B-SCRIPT-019
};

/// @contract value Zenith_ScriptCapacityLimits
/// Ownership: Plain value, a member of Zenith_ScriptWorldDesc.
/// Thread affinity: IMMUTABLE. Members non-const: filled from the budget rows.
/// Invalidation: None; Zenith_ScriptWorld::Create copies it.
/// Units: Entries, bytes and records per tick, and in-flight requests, per member row.
/// Errors: Zenith_ScriptWorld::Create returns SCRIPT_ERROR_INVALID_DESC when any member is zero.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_ScriptCapacityLimits          // the SCRIPT-held classes; the ECS classes are Zenith_MergeCapacity's
{
	uint32 m_uStringInternEntries = 0;       // Z2-B-SCRIPT-022
	uint32 m_uStringInternBytes = 0;
	uint32 m_uAudioTriggersPerTick = 0;      // Z2-B-SCRIPT-023
	uint32 m_uVfxTriggersPerTick = 0;        // Z2-B-SCRIPT-024
	uint32 m_uSaveRequestsPerTick = 0;       // Z2-B-SCRIPT-025
	uint32 m_auAsyncInFlight[4] = {};        // path (Z2-B-AI-003), streaming, physics query, online (Z2-B-SCRIPT-026)
};

// elaboration: members of Zenith_ScriptWorldDesc
	Zenith_WorldSimulation* m_pxSimulation = nullptr;         // borrowed: tick index, system registry, tick-boundary handlers
	const Zenith_TypeRegistry* m_pxTypes = nullptr;           // frozen
	uint16 m_uWorldTag = 0;                                  // Zenith_WorldTagIssue::m_uTag
	uint32 m_uFrameThreadCount = 0;                          // replaces m_uWorkerCount; Zenith_JobSystem::GetFrameThreadCount(); 1 … uSCRIPT_MAX_WORKER_VM_SLOTS
	Zenith_ScriptGCTuning m_xGCTuning;                        // Z2-B-SCRIPT-017
	uint32 m_uGCStepKBPerTick = 0;                           // Z2-B-SCRIPT-007
	uint64 m_ulWorkerVMTotalBytes = 0;                       // Z2-B-SCRIPT-011
	uint32 m_uSnapshotCapBytesPerEntity = 0;                 // Z2-B-SCRIPT-012
	uint32 m_uCheckpointSlotBytes = 0;                       // Z2-B-SCRIPT-015
	Zenith_ScriptSafepointLimits m_xSafepoints;
	Zenith_ScriptCapacityLimits m_xCapacities;
	Zenith_ScriptWorldListener* m_pxListener = nullptr;       // borrowed; halts, disables, recoveries and reloads
	bool m_bRunSimulationScripts = true;                      // elaboration (X425): false in EDIT and PREVIEW worlds (Runtime.md → "Node registration and frame-graph placement")
	Zenith_ScriptPublicationObserver* m_pxPublicationObserver = nullptr;   // borrowed; the replay recorder, or null
	Zenith_ScriptPublicationSource* m_pxPublicationSource = nullptr;       // borrowed; under --replay, else null
	Zenith_Heap* m_pxHeap = nullptr;
	Zenith_FaultInjector* m_pxFaults = nullptr;

// elaboration: Zenith_ScriptWorld members. RegisterScriptNodes takes the world's system registry, not the frame-graph builder
// (reconciliation item S2; ECS reconciliation R10; Architecture/CompositionRoot.md → "Startup sequence" step 17).
	Zenith_Result<void, Zenith_ScriptError> RegisterScriptNodes(Zenith_SystemRegistry& xSystems);
	// LoadModules (LibraryLayout.md) succeeds when the world is usable: a module that fails to load is disabled for the session,
	// recorded in the diagnostic store and reported through OnModuleDisabled; an error means the manifest is unusable
	// (SCRIPT_ERROR_BYTECODE_MISMATCH) or no behaviour or chunk-system module of a non-empty manifest loaded (SCRIPT_ERROR_LOAD).
	uint32 GetDisabledModuleCount() const;
	Zenith_AsyncResultTable& GetAsyncResults();
	// core text since X345 (Scripting/LibraryLayout.md → "Public API of Zenith_ScriptWorld"): MAIN_THREAD, out of tick; the world keeps no table afterwards and GetAsyncResults asserts
	Zenith_Owned<Zenith_AsyncResultTable> DetachAsyncResults();
	// core text since X345 (Scripting/LibraryLayout.md → "Public API of Zenith_ScriptWorld"): every committed checkpoint into the tick hash (Design/SCRIPT/Runtime.md → "Committed checkpoints in the tick hash")
	void AppendCommittedStateHash(Zenith_Hash64Stream& xHash) const;
	const Zenith_ScriptStateSnapshot& GetStateSnapshot() const;       // phase-11 readers
	Zenith_Result<void, Zenith_ScriptError> RestoreSavedStates(Zenith_Span<const Zenith_ScriptSavedState> axStates);   // out of tick
	bool GetDiagnostic(uint32 uSequence, Zenith_ScriptDiagnostic& xOut) const;   // false when overwritten or unknown
	uint32 GetLatestDiagnosticSequence() const;
	uint16 GetWorldTag() const;
	Zenith_Result<uint32, Zenith_ScriptError> ConsoleEvaluate(Zenith_StringView strSource,
		Zenith_MutableByteSpan xResultText);          // development builds, out of tick: the editor console (TOOLS context); bytes written
		// Shipping: if constexpr (Zenith_IsShippingBuild()) returns SCRIPT_ERROR_INVALID_DESC without touching a VM ([Z2-N-SCRIPT-488])
	/// @deviation Thread affinity: ANY_THREAD. Reads an atomic flag.
	bool IsHalted() const;

// the composition root's reader of SCRIPT's allocated rows: exactly Scripting/LibraryLayout.md → "Public API of Zenith_ScriptWorld" (cross-doc row X219 applied).
/// @contract functions Zenith_ScriptReadWorldLimits
/// Ownership: Writes only the row-valued members of xDesc (m_ulMainVMCapBytes, m_ulWorkerVMCapBytes, m_ulWorkerVMTotalBytes,
///   m_xGCTuning, m_uGCStepKBPerTick, m_uSnapshotCapBytesPerEntity, m_uCheckpointSlotBytes, m_xSafepoints, m_xCapacities);
///   pointers, tag, seed and thread count stay the caller's.
/// Thread affinity: MAIN_THREAD. Startup step 16, before Zenith_ScriptRuntime::CreateWorld.
/// Invalidation: None.
/// Units: As the members' rows.
/// Errors: SCRIPT_ERROR_INVALID_DESC when a script.* key it reads is missing from xSchema; xDesc is then unchanged.
/// Version: since Z2 Stage 3.
/// @endcontract
Zenith_Result<void, Zenith_ScriptError> Zenith_ScriptReadWorldLimits(const Zenith_ConfigSchema& xSchema,
	const Zenith_ConfigStore& xStore, Zenith_ScriptWorldDesc& xDesc);   // the allocated rows; script.gc.stepKBPerTick sets the GC step
```

`OnTickBoundary(uint64)` of LibraryLayout.md is not called by the composition root: `Create` registers an internal `Zenith_TickBoundaryHandler` adapter with `Zenith_WorldSimulation::AddTickBoundaryHandler` (`Zenith2/Docs/Design/ECS/WorldAPI.md` → "Zenith_ECSObservers header"), and `OnTickBoundary` is the step-3 body that adapter calls (Design/SCRIPT/Runtime.md → "Tick-boundary handler"). It stays public for tests, which call it only on a world whose simulation is not running a frame.

`Zenith_ScriptWorld::Create` fault points (values carry the SCRIPT high 16 bits `0x5343`, cross-doc item X67):

```cpp
inline constexpr uint32 uFAULT_POINT_SCRIPT_WORLD_MAIN_VM = 0x53430001u;
inline constexpr uint32 uFAULT_POINT_SCRIPT_WORLD_SLOT_TABLE = 0x53430002u;
inline constexpr uint32 uFAULT_POINT_SCRIPT_WORLD_TXN_ARENAS = 0x53430003u;
inline constexpr uint32 uFAULT_POINT_SCRIPT_WORLD_ASYNC_TABLE = 0x53430004u;
inline constexpr uint32 uFAULT_POINT_SCRIPT_WORLD_HANDLER = 0x53430005u;
inline constexpr uint32 uFAULT_POINT_SCRIPT_WORLD_WATCHDOG_REGISTER = 0x53430006u;
inline constexpr uint32 uFAULT_POINT_SCRIPT_WORLD_DIAGNOSTICS = 0x53430007u;
```

## Zenith_ScriptRuntime header

`Zenith_ScriptRuntimeDesc` and `Zenith_ScriptRuntime` are `Zenith2/Docs/Scripting/PublicTypes.md` → "Zenith_ScriptRuntime", with its contract block; additions:

```cpp
// Zenith2/Engine/ZenithScript/Zenith_ScriptRuntime.h (additions)
#include "ZenithScript/Zenith_ScriptRecovery.h"
#include "ZenithCore/Zenith_BuildInfo.h"        // Zenith_BuildPlatform
#include "ZenithCore/Zenith_ConfigStore.h"      // Zenith_ConfigSchema, Zenith_ConfigError

/// @contract value Zenith_ScriptDebuggerDesc
/// Ownership: Plain value; m_strBindAddress borrows the command line's storage for the Create call.
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct it and set the fields they need.
/// Invalidation: None.
/// Units: A TCP port.
/// Errors: Ignored in Shipping, where no debugger code is linked ([Z2-N-SCRIPT-461]). A listener that cannot be created is an
///   error of Zenith_ScriptRuntime::Create (SCRIPT_ERROR_DEBUGGER_UNAVAILABLE) only when m_bRequired is set; otherwise the
///   runtime is created without a debugger and IsDebuggerAvailable() reports false.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_ScriptDebuggerDesc
{
	bool m_bEnabled = false;                     // --script-dap-port given, or the editor
	bool m_bRequired = false;                    // set by the DAP tests' harness only; the Player and the editor never set it
	uint16 m_uPort = uSCRIPT_DAP_DEFAULT_PORT;
	Zenith_StringView m_strBindAddress;          // empty = 127.0.0.1 (Design/PLAT/Platform.md → "Loopback socket")
};
```

<!-- assumption: pending core-doc (X121) -->
The members marked `// elaboration` below are additions Scripting/PublicTypes.md → "Zenith_ScriptRuntime" adopts with cross-doc row X121; `CommitPendingReloads` is added with cross-doc row X158 (Design/SCRIPT/HotReload.md → "Schedule commit in the prologue").
<!-- /assumption -->

```cpp
// elaboration: members of Zenith_ScriptRuntimeDesc
	const Zenith_TypeRegistry* m_pxTypes = nullptr;
	Zenith_ScriptRecoverySettings m_xRecovery;
	Zenith_ScriptNativePolicy m_eNativePolicy = SCRIPT_NATIVE_POLICY_TOOLS_ONLY;
	Zenith_ScriptDebuggerDesc m_xDebugger;
	Zenith_Heap* m_pxHeap = nullptr;
	Zenith_FaultInjector* m_pxFaults = nullptr;

// elaboration: Zenith_ScriptRuntime members
	Zenith_Result<void, Zenith_ScriptError> RequestCookedReload(Zenith_ScriptModuleID xModule, Zenith_ByteSpan xZlbc,
		Zenith_Span<const Zenith_ScriptNameRefRecord> axNameRefs = {});
		// exactly PublicTypes.md → "Zenith_ScriptRuntime" ([Z2-N-SCRIPT-927]): development builds; decodes a re-cooked .zlbc and
		// queues it, with the module's load-time name references, on every world with RequestHotReload; the caller is the
		// development Player's file watcher, passing GetNameRefs(xModule) of the re-read scripts.zlmf
	void PollDebugger();                                 // frame-loop step 3; a no-op without a debugger
	void CommitPendingReloads();
		// prologue step 6, every frame, after the hot-reload window closed or in its place: drains every world's main-VM release
		// list, rolls back staged reloads of a window that did not publish, commits each world's changed script systems through
		// Zenith_SystemReloadScope (recompiled in step 8 of the same frame) and rolls back a world whose Commit fails; failures
		// are recorded in the runtime's diagnostic ring and logged under script.reload (Design/SCRIPT/HotReload.md)
	bool CanRunTick(uint16 uWorldTag, uint64 ulTick) const;
		// replay: false while a recomputed result recorded for ulTick is incomplete ([Z2-N-SCRIPT-337]); always true otherwise
	Zenith_ScriptWorld* FindWorld(uint16 uWorldTag);
	Zenith_Span<Zenith_ScriptWorld* const> GetWorlds();   // creation order
	bool GetDiagnostic(uint32 uSequence, Zenith_ScriptDiagnostic& xOut) const;   // the runtime's own ring (reload failures)
	bool IsDebuggerAvailable() const;      // false in Shipping, without m_bEnabled, or when the listener failed

// Startup step 6, the Script slot of the engine types (Architecture/CompositionRoot.md → "Startup sequence"): registers
// Zenith_BehaviourComponent ("Zenith.Behaviour") while the registry is DECLARING.
Zenith_Result<void, Zenith_ScriptError> Zenith_ScriptRegisterEngineTypes(Zenith_TypeRegistry& xTypes);

// Startup step 10, after RegisterSchemaBlob and before Freeze (Design/ECS.md → "Behaviour-state type registration"): registers one
// "Behaviour.<Name>.State" (TYPE_KIND_BEHAVIOUR_STATE, Origin(TYPE_ORIGIN_SCHEMA)) per state-schema record of the cooked manifest,
// in module-ID order (reconciliation item S3; cross-area note E12(a); cross-doc row X122). SCRIPT_ERROR_LOAD names the module.
Zenith_Result<void, Zenith_ScriptError> Zenith_ScriptRegisterBehaviourStateTypes(Zenith_TypeRegistry& xTypes,
	const Zenith_ScriptModuleManifest& xManifest);

// Startup step 6, library-layer order (Design/MOD.md → "Configuration-key registration"): the keys of Design/SCRIPT.md →
// "Configuration keys", every one registered in every configuration.
Zenith_Result<void, Zenith_ConfigError> Zenith_RegisterScriptConfigKeys(Zenith_ConfigSchema& xSchema,
	Zenith_BuildPlatform ePlatform);
```

The console compile thunk (`Zenith_PfnScriptCompileConsole`, `Zenith_ScriptRuntimeDesc::m_pfnCompileConsole`) and the schema migration host's `Zenith_PfnScriptMigrationBytecode`, `Zenith_ScriptMigrationHostDesc` and `Zenith_ScriptCreateMigrationHost` are declared by `Zenith2/Docs/Scripting/PublicTypes.md` → "Zenith_ScriptRuntime" ([Z2-N-SCRIPT-923], cross-doc row X193 applied) and are not repeated here.

Fault points of `Zenith_ScriptRuntime::Create`: `uFAULT_POINT_SCRIPT_RUNTIME_WORLD_TABLE = 0x53430010u`, `uFAULT_POINT_SCRIPT_RUNTIME_RELOAD_QUEUE = 0x53430011u`, `uFAULT_POINT_SCRIPT_RUNTIME_DEBUGGER = 0x53430012u`, `uFAULT_POINT_SCRIPT_RUNTIME_DIAGNOSTICS = 0x53430013u`; of `Zenith_ScriptRuntime::CreateWorld` with a debugger: `uFAULT_POINT_SCRIPT_RUNTIME_DAP_SLOTS = 0x53430014u` (the world's stop block, Design/SCRIPT/Debugger.md → "DAP service ownership and lifetime").

## Zenith_ScriptHost header

The upward-notification interfaces SCRIPT declares; ZenithRuntime implements them ([Z2-N-CONTRACT-175]).

```cpp
// Zenith2/Engine/ZenithScript/Zenith_ScriptHost.h
#pragma once
#include "ZenithCore/Zenith_Span.h"
#include "ZenithScript/Zenith_ScriptError.h"

struct Zenith_ScriptPublication;           // Zenith_ScriptAsync.h
class Zenith_TickBoundaryAccess;           // ZenithECS (Design/ECS/WorldAPI.md → "Zenith_ECSObservers header")

/// @contract interface Zenith_ScriptWorldListener
/// Ownership: Implemented by ZenithRuntime; passed by pointer in Zenith_ScriptWorldDesc; outlives the world.
/// Thread affinity: OnWorldHalted on the thread that observed the fatal event (any frame thread, at most once per world);
///   OnModuleDisabled and OnModuleRecovered on the world's tick-boundary node.
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
	// elaboration (X425): a reload generation went live (HotReload.md → "Go-live at the tick boundary" step 5); on the world's
	// tick-boundary node, or on the main thread for an out-of-tick go-live. The default does nothing.
	virtual void OnModuleReloaded(uint16 uWorldTag, uint64 ulTick, Zenith_ScriptModuleID xModule, uint32 uGeneration) {}
};

/// @contract interface Zenith_ScriptPublicationObserver
/// Ownership: Implemented by the runtime's replay recorder (over ZenithSave's ZRPL writer); borrowed by the world.
/// Thread affinity: The world's tick-boundary node, step 1.
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
/// Thread affinity: GetRecorded on the world's tick-boundary node, step 1, and on the main thread in the frame prologue
///   (through Zenith_ScriptRuntime::CanRunTick); the two never overlap.
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
	// core text since X345 (Scripting/PublicTypes.md → "ZenithScript asynchronous results and host interfaces"): the recorded next completion ID of the world's TICK record for ulTick, 0 when the log
	// holds none; called at tick-boundary step 1 before GetRecorded (Design/SCRIPT/Runtime.md → "Asynchronous result table")
	virtual uint64 GetRecordedNextCompletionID(uint16 uWorldTag, uint64 ulTick) = 0;
};

/// @contract interface Zenith_AsyncPublicationHandler
/// Ownership: Implemented by a producer whose results C++ consumes at publication (the PLAT bridge for
///   ASYNC_SOURCE_PLATFORM, ZenithRuntime's SCENE forwarder for ASYNC_SOURCE_SCENE_LOAD and its module publication forwarder for
///   every module source, Zenith2/Docs/Design/MOD/RuntimeSeams.md → "The module publication forwarder"); registered per source
///   kind with Zenith_AsyncResultTable::SetHandler before the first tick.
/// Thread affinity: The world's tick-boundary node, step 1.
/// Invalidation: xPublication's payload is valid for the call.
/// Units: None.
/// Errors: None.
/// Reentrancy: NONE. It writes world state only through xWorld, the tick boundary's Zenith_TickBoundaryAccess (property reads and
///   writes, never structure; cross-area note E12(d)), and records nothing a script sees before the tick's phases run.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_AsyncPublicationHandler
{
public:
	virtual ~Zenith_AsyncPublicationHandler() = default;
	virtual void OnPublished(const Zenith_ScriptPublication& xPublication, Zenith_TickBoundaryAccess& xWorld) = 0;
};
```

The adapter passes the `Zenith_TickBoundaryAccess&` its own `OnTickBoundary` received (Design/SCRIPT/Runtime.md → "Tick-boundary handler"), which is the only world access a tick-boundary handler has.

<!-- assumption: pending ECS -->
PLAT's handler appends a `Platform.*` event for each platform publication, delivered in phase 9 of the same tick (`Zenith2/Docs/Design/PLAT.md` → "Frame-graph placement" step 3), but `Zenith_TickBoundaryAccess` has no event member. Cross-area row SC20 asks ECS for `Zenith_Result<void, Zenith_ECSError> EmitEvent(Zenith_TypeID xEventType, Zenith_ByteSpan xPayload, uint64 ulOrderKey)` on `Zenith_TickBoundaryAccess`, appending to the tick's event queue ahead of every system's events in ascending `ulOrderKey` (the completion ID), outside the merge-time event capacity because platform events are recorded, not requested. Until it lands the interim is PLAT's, named in SC20: the handler copies each publication's payload into the bridge's per-world list, and a phase-1 engine system of the bridge (`EmitsEvent` of every `Platform.*` type) emits them through its `Zenith_EventWriter` in completion-ID order, so they are delivered in phase 9 of the same tick; SCRIPT's interface is the same in both.
<!-- /assumption -->

## Zenith_ScriptAsync header

The per-world asynchronous result table of `Zenith2/Docs/Scripting.md` → "Asynchronous requests and completion IDs" and "Tick-boundary publication" ("the world's pending-result set"), public because producers outside ZenithScript (the PLAT bridge, streaming, saving) park results in it. The algorithms are Design/SCRIPT/Runtime.md → "Asynchronous result table".

<!-- assumption: pending ARCH -->
Architecture names "the world's pending-result set" and "the world's monotonic counter" without an owning type. This design makes the table a member of `Zenith_ScriptWorld`, which exists for every world (a world with no script module still has one), because Scripting.md owns publication; the cross-area notes carry the row for ARCH, PLAT and SAVE.
<!-- /assumption -->

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
/// Thread affinity: IMMUTABLE. Members non-const: copied into the async table's reused slots.
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
/// Thread affinity: IMMUTABLE. Members non-const: an element of the async table, reused slot by slot.
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

The block above is `Zenith2/Docs/Scripting/PublicTypes.md` → "ZenithScript asynchronous results and host interfaces" as cross-doc items X255, X271, X274, X278 and X345 applied it ([Z2-N-SCRIPT-901]), with one elaboration in the Ownership line (the slots of a source declared after creation). X345 (applied by core-doc patch f) added `Zenith_AsyncTableEntry` and the retirement, count, listing and restore members, which give GAME's world host the retirement it performs at a swap and at teardown (cross-area row GA10 (a)), SAVE the capture and restore of a world's asynchronous state (row SV3) and module producers their outstanding counts (row UI1 (c)). Algorithms: Design/SCRIPT/Runtime.md → "Asynchronous result table".

## Zenith_ScriptStateSnapshot header

The read side SAVE uses in phase 11 and the restore side it uses at load (Design/SCRIPT/HotReload.md → "Behaviour state in saves").

```cpp
// Zenith2/Engine/ZenithScript/Zenith_ScriptStateSnapshot.h
#pragma once
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_PersistentID.h"

/// @contract value Zenith_ScriptSavedState
/// Ownership: Plain value; m_xCheckpoint borrows the instance's active checkpoint slot (snapshot) or the save's buffer (restore).
/// Thread affinity: IMMUTABLE. Members non-const: the save capture fills one per instance.
/// Invalidation: A snapshot entry is valid until the end of phase 11 of the tick it was read in.
/// Units: The checkpoint bytes are the ZBST format of Zenith2/Docs/Scripting.md → "Checkpoint format".
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_ScriptSavedState
{
	Zenith_PersistentID m_xEntity;         // zero when the entity has none; then m_ulSimulationID identifies it
	uint64 m_ulSimulationID = 0;
	uint64 m_ulSourceID = 0;              // Hash64 of the behaviour Name
	Zenith_ByteSpan m_xCheckpoint;
	// empty m_xCheckpoint: the instance has no committed checkpoint and restores its declared defaults
	bool m_bRunSuspended = false;          // its Run coroutine has yielded without returning (restore restarts it: not exact)
	bool m_bRecoveryPending = false;       // PENDING_RECOVER: its OnRecover has not run yet (restored pending: exact)
	bool m_bQuarantinedOrDisabled = false; // QUARANTINED, or its module disabled or disabling (restored as a recovery: not exact)
};

/// @contract type Zenith_ScriptStateSnapshot
/// Ownership: Owned by its Zenith_ScriptWorld; returned by const reference.
/// Thread affinity: PHASE — readable in phase 11 (the read-only save-snapshot phase) and out of tick.
/// Invalidation: Returned entries are invalid after phase 11 ends.
/// Units: Entries in ascending (stable source ID, simulation ID).
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_ScriptStateSnapshot
{
public:
	uint32 GetCount() const;
	Zenith_ScriptSavedState Get(uint32 uIndex) const;
	uint64 GetNextCompletionID() const;
};
```

## Zenith_ScriptBehaviourComponent header

The component that attaches a behaviour module to an entity (Scripting.md, table in "Behaviours and chunk systems": "one per entity carrying a `Behaviour` component that names the module"). It is C++-registered, so scenes, prefabs, the inspector and saves treat it like any component.

```cpp
// Zenith2/Engine/ZenithScript/Zenith_ScriptBehaviourComponent.h
#pragma once
#include "ZenithCore/Zenith_StringID.h"
#include "ZenithReflect/Zenith_ReflectIDs.h"

/// @contract value Zenith_BehaviourComponent
/// Ownership: A component value stored by the ECS backend; stable name "Zenith.Behaviour", version 1, script name "Behaviour".
/// Thread affinity: PHASE — both properties have authority PHASE_COMMANDS_ONLY (Architecture/FramePhases.md → "Per-field authority"):
///   scripts change them through ctx:Command, authoring through the editor's commands.
/// Invalidation: None.
/// Units: m_xModule is the interned manifest path, e.g. "@game/Village/Door".
/// Errors: A path the manifest does not name yields no instance and one SCRIPT_MODULE_DISABLED diagnostic with ZS1801 for that
///   entity; the entity is otherwise unaffected.
/// Version: since Z2 Stage 3. Property IDs 1 (Module) and 2 (Enabled) are fixed.
/// @endcontract
struct Zenith_BehaviourComponent
{
	Zenith_StringID m_xModule;     // property 1 "Module", PROPERTY_TYPE_STRING_ID
	bool m_bEnabled = true;        // property 2 "Enabled": false keeps the instance dormant (no invocation, state kept)
};
```

## Zenith_ScriptCompileOptions header

Exactly `Zenith2/Docs/Scripting.md` → "Development compilation" (first code block). Contract blocks:

```cpp
/// @contract value Zenith_ScriptCompileOptions
/// Ownership: Plain value; m_aszMutableGlobals borrows a static-duration NULL-terminated array.
/// Thread affinity: IMMUTABLE. Members non-const: tests copy the production value and change one field.
/// Invalidation: None.
/// Units: Luau optimization, debug and type-info levels.
/// Errors: None; any value other than Zenith_ScriptProductionCompileOptions() is used only by tests, because the cook and
///   every hot-reload producer pass the production value ([Z2-N-SCRIPT-508]).
/// Version: since Z2 Stage 3. An input of the binding-set hash.
/// @endcontract

/// @contract functions Zenith_ScriptCompileOptionsFunctions
/// Ownership: Zenith_ScriptSandboxMutableGlobals returns a static constant array; Zenith_ScriptBindingSetHash reads its arguments.
/// Thread affinity: ANY_THREAD; the binding table is sealed and the registry frozen.
/// Invalidation: None.
/// Units: Hash64 values.
/// Errors: None.
/// Version: since Z2 Stage 3. The hash's input order is Scripting.md → "Cooked bytecode format", the field at offset 24.
/// @endcontract

// elaboration (cross-area row MT2 (e)): the one producer of the engine version hash of Scripting.md → "Cooked bytecode
// format" (offset 16) and of the module manifest (offset 8). Returns Zenith_GetEngineVersionKey(Zenith_GetEngineVersion())
// (ZenithCore/Zenith_EngineVersion.h, Zenith2/Docs/Design/MAINT/Versioning.md → "Ordering key"); a member of the functions
// block above. The cook fills Zenith_ScriptCookDesc::m_ulEngineVersionHash with it and the runtime compares against it.
uint64 Zenith_ScriptEngineVersionHash();
```

The key changes at every engine changelog entry (each compatibility event, the release, each release fix and the roll), so cooked bytecode re-cooks then and two builds of one commit agree. No committed file holds the value: bytecode and manifests are regenerated cook output ([Z2-N-ARCH-208]), and every test that needs a cooked header builds it at run time from `Zenith_ScriptEngineVersionHash()` (the test bytecode wrapper `Zenith_ScriptTestBytecode`, the test manifest builder `Zenith_ScriptTestManifest.h`), never from a committed fixture.

## Zenith_ScriptTypesEmitter header

```cpp
// Zenith2/Engine/ZenithScript/Zenith_ScriptTypesEmitter.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithScript/Zenith_ScriptError.h"

class Zenith_TypeRegistry;
class Zenith_ScriptBindingTable;
class Zenith_Arena;

/// @contract enum Zenith_ScriptDefinitionFile
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
enum Zenith_ScriptDefinitionFile : uint8
{
	SCRIPT_DEFINITION_FILE_ZENITH_DLUAU = 1,       // zenith.d.luau
	SCRIPT_DEFINITION_FILE_COMPONENTS_DLUAU,       // components.d.luau (from ZenithReflect's Zenith_EmitComponentLuauTypes)
	SCRIPT_DEFINITION_FILE_PRELUDE,                // Zenith.luau
	SCRIPT_DEFINITION_FILE_TYPES,                  // Types.luau
	SCRIPT_DEFINITION_FILE_LUAURC,                 // .luaurc
	SCRIPT_DEFINITION_FILE_BINDINGS_JSON,          // bindings.json, the binding listing (Design/SCRIPT/Bindings.md → "Binding listing")
};

using Zenith_PfnDefinitionSink = void (*)(Zenith_ScriptDefinitionFile eFile, Zenith_StringView strText, void* pUserData);

/// @contract functions Zenith_ScriptEmitDefinitions
/// Ownership: Writes UTF-8 text through the caller's sink, one call per completed file; allocates only from xScratch.
/// Thread affinity: The calling thread; the registry is frozen and the binding table sealed.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: SCRIPT_ERROR_LOAD, with a diagnostic naming both C++ sources, when a derived name collides or is invalid
///   ([Z2-N-SCRIPT-116]); no file is written when any error occurs.
/// Version: since Z2 Stage 3. Byte-stable output ([Z2-N-SCRIPT-621], ordered per [Z2-N-ARCH-611]).
/// @endcontract
Zenith_Result<void, Zenith_ScriptError> Zenith_ScriptEmitDefinitions(const Zenith_TypeRegistry& xTypes,
	const Zenith_ScriptBindingTable& xBindings, Zenith_StringView strGameName, Zenith_Arena& xScratch,
	Zenith_PfnDefinitionSink pfnSink, void* pUserData);
```

## Zenith_ScriptBinding header

`Zenith2/Docs/Scripting/Bindings.md` → "Public binding types" is the file's content; `Zenith2/Docs/Scripting.md` → "Capacity classes" declares `Zenith_ScriptCapacityClass` and `Zenith_ScriptCapacityDemand` in it. This design reshapes `Zenith_ScriptThunkError` to the Contracts error form with its own code enumeration (reconciliation item S1), gives `Seal` an enumeration-form error, adds the side-effect consumer interface and gives every type its contract block.

<!-- assumption: pending core-doc (X115, X117) -->
The thunk-error shape, `Zenith_ScriptThunkErrorCode`, `Zenith_ScriptBindingError`, the `Seal` signature, `Zenith_ScriptCallFrame::m_acErrorMessage` and `Zenith_ScriptInvocationContext::GetErrorMessage` are adopted by `Zenith2/Docs/Scripting/Bindings.md` → "Public binding types" with cross-doc row X115; `Zenith_ScriptSideEffectConsumer`, `SetSideEffectConsumer`, `DeclareAsyncSource` and `GetSideEffectConsumer` with cross-doc row X117(a).
<!-- /assumption --> The non-template members are defined in `Zenith2/Engine/ZenithReflect/Internal/Zenith_ScriptBindingTable.cpp`, a SCRIPT-owned file of ZenithReflect.

<!-- assumption: pending core-doc (X449, X450, X451) -->
`uSCRIPT_ASYNC_SOURCE_KIND_CAPACITY` (X449), the enumerator `SIDE_EFFECT_KIND_XR_RECENTRE` appended to `Zenith_SideEffectKind` before `SIDE_EFFECT_KIND_COUNT` (X450), and `Zenith_ScriptApiStability`, `Zenith_ScriptDeprecation`, `Zenith_ScriptFunctionVersion`, the five `Zenith_ScriptFunctionDesc` members, `AddFunction`'s trailing `xVersion` and `SetRegistrantLibrary` (X451) are adopted by `Zenith2/Docs/Scripting/Bindings.md` → "Public binding types" with those cross-doc rows ([Z2-N-SCRIPT-901]).
<!-- /assumption -->

```cpp
// Zenith2/Engine/ZenithReflect/Zenith_ScriptBinding.h (additions and replacements)

// elaboration: the thunk error's own code enumeration ([Z2-N-CONTRACT-482] struct form: Zenith_ScriptThunkError / _ErrorCode),
// one value per Zenith_ScriptErrorKind in kind order, numbered from 1 ([Z2-N-CONTRACT-465])
enum Zenith_ScriptThunkErrorCode : uint8
{
	SCRIPT_THUNK_ERROR_RUNTIME = 1, SCRIPT_THUNK_ERROR_MISUSE, SCRIPT_THUNK_ERROR_BUDGET, SCRIPT_THUNK_ERROR_SNAPSHOT_CAP,
	SCRIPT_THUNK_ERROR_RESERVATION, SCRIPT_THUNK_ERROR_YIELD_IN_SYSTEM, SCRIPT_THUNK_ERROR_CHECKPOINT, SCRIPT_THUNK_ERROR_VM_MEMORY,
	SCRIPT_THUNK_ERROR_WATCHDOG, SCRIPT_THUNK_ERROR_INTERNAL, SCRIPT_THUNK_ERROR_COMPILE, SCRIPT_THUNK_ERROR_LOAD,
};

// replaces Scripting/Bindings.md's 256-byte-message form ([Z2-N-CONTRACT-480]); the message moves to the call frame
struct Zenith_ScriptThunkError
{
	Zenith_ScriptThunkErrorCode m_eCode = SCRIPT_THUNK_ERROR_RUNTIME;   // the kind is m_eCode − 1; an abort kind is re-raised as that kind
	uint8 m_uReserved = 0;
	uint16 m_uCode = 0;                                           // ZS catalogue code, or 0
	uint32 m_uDiagnosticSequence = 0;                             // for errors InvokeCallback returns: the VM's error-scratch sequence
};
static_assert(sizeof(Zenith_ScriptThunkError) == 8);
constexpr Zenith_ScriptErrorKind Zenith_ScriptThunkErrorKind(const Zenith_ScriptThunkError& xError)
{
	return static_cast<Zenith_ScriptErrorKind>(static_cast<uint8>(xError.m_eCode) - 1u);
}
constexpr Zenith_ScriptThunkErrorCode Zenith_ScriptThunkErrorCodeFromKind(Zenith_ScriptErrorKind eKind)
{
	return static_cast<Zenith_ScriptThunkErrorCode>(static_cast<uint8>(eKind) + 1u);
}

// elaboration: the error of Zenith_ScriptBindingTable::Seal ([Z2-N-CONTRACT-482] enumeration form), the first failure in sealed order
enum Zenith_ScriptBindingError : uint8
{
	SCRIPT_BINDING_ERROR_DUPLICATE_ID = 1,        // two functions with one function ID
	SCRIPT_BINDING_ERROR_DUPLICATE_NAME,          // one namespace with two equal Luau names
	SCRIPT_BINDING_ERROR_INVALID_NAME,            // a namespace or Luau name outside rule 7 of Scripting/Bindings.md → "Rules"
	SCRIPT_BINDING_ERROR_INVALID_DESCRIPTOR,      // zero contexts, READ_WORLD without a resource, BUFFERED without a side-effect mask
	SCRIPT_BINDING_ERROR_MISSING_CONSUMER,        // a BUFFERED function whose mask names a SCRIPT-held kind with no consumer
	SCRIPT_BINDING_ERROR_UNKNOWN_NAME_KIND,       // a parameter's m_szResolveKind names a kind no AddNameTable declared ([Z2-N-SCRIPT-922])
	SCRIPT_BINDING_ERROR_DUPLICATE_NAME_TABLE,    // two AddNameTable calls for one kind, or one table holding a name twice
};
constexpr const char* Zenith_ToString(Zenith_ScriptBindingError eError);
constexpr const char* Zenith_ToString(Zenith_ScriptThunkErrorCode eCode);

// elaboration: member of Zenith_ScriptCallFrame
	char m_acErrorMessage[256] = {};             // UTF-8 message of a SCRIPT_THUNK_STATUS_ERROR; the dispatcher raises it verbatim

// core text since X346 (Scripting/Bindings.md → "Public binding types"; cross-area rows TD22 (a), AN10): the per-function user datum of a binding
//   Zenith_ScriptFunctionDesc gains   const void* m_pUserData = nullptr;   (borrowed; outlives the sealed table; not hashed)
//   AddFunction gains the trailing    const void* pUserData = nullptr
//   Zenith_ScriptInvocationContext gains
	virtual const void* GetFunctionUserData() const = 0;   // the called function's m_pUserData; null when none

// the helper every thunk fails through, in its template form: exactly Scripting/Bindings.md → "Public binding types" (cross-doc row
// X213 applied; a C-variadic body would need <cstdarg> and a printf-family formatter outside the allowed headers). Defined in
// Zenith_ScriptBinding.inl: Zenith_FormatTo into m_acErrorMessage (CORE, Design/CORE/Text.md → "Formatting"), then the tail.
template <typename... TArgs>
Zenith_ScriptThunkStatus Zenith_ScriptFailThunk(Zenith_ScriptCallFrame& xFrame, Zenith_ScriptErrorKind eKind, uint16 uCode,
	Zenith_FormatString<std::type_identity_t<TArgs>...> xFormat, const TArgs&... xArgs);
// the non-template tail (Internal/Zenith_ScriptBindingTable.cpp): sets m_xError, returns SCRIPT_THUNK_STATUS_ERROR
Zenith_ScriptThunkStatus Zenith_ScriptFailThunkFinish(Zenith_ScriptCallFrame& xFrame, Zenith_ScriptErrorKind eKind, uint16 uCode);

// elaboration: member of Zenith_ScriptInvocationContext
	virtual uint32 GetErrorMessage(const Zenith_ScriptThunkError& xError, Zenith_MutableByteSpan xOut) const = 0;
		// copies the text of an error InvokeCallback returned from the VM's error scratch (same thread); returns the bytes written,
		// 0 when a subsequent error on this VM replaced it (Design/SCRIPT/Transactions.md → "Diagnostic store implementation")

/// @contract interface Zenith_ScriptSideEffectConsumer
/// Ownership: Implemented by the library that owns a SCRIPT-held side-effect kind (audio and VFX triggers: those modules;
///   save requests: the runtime over ZenithSave; asynchronous requests: the owning subsystem); registered with
///   Zenith_ScriptBindingTable::SetSideEffectConsumer before Seal; outlives every world.
/// Thread affinity: Called on the committing world's merge node, in merge order; one world's calls never overlap, two
///   worlds' calls may run concurrently, so the consumer keeps per-world state keyed by uWorldTag.
/// Invalidation: xPayload is valid for the call.
/// Units: Payload bytes as the binding serialized them.
/// Errors: Consume cannot fail: the capacity the record needed was accepted at the merge before commit ([Z2-N-SCRIPT-248]).
/// Reentrancy: NONE. A consumer never calls into script.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_ScriptSideEffectConsumer
{
public:
	virtual ~Zenith_ScriptSideEffectConsumer() = default;
	virtual void Consume(uint16 uWorldTag, Zenith_SideEffectKind eKind, uint64 ulCompletionID, Zenith_ByteSpan xPayload) = 0;
		// ulCompletionID is the assigned ID for SIDE_EFFECT_KIND_ASYNC_REQUEST, 0 otherwise
};

// elaboration: a source kind's ticket-result decoder (cross-area row S22). It fills named fields that ticket:Result()
// turns into one Luau table (Design/SCRIPT/Runtime.md → "Asynchronous result table"); false means the payload is undecodable.
inline constexpr uint32 uSCRIPT_ASYNC_RESULT_MAX_FIELDS = 8;
struct Zenith_ScriptResultField
{
	const char* m_szName = nullptr;            // static storage; the table key
	Zenith_ScriptValue m_xValue;               // SCRIPT_VALUE_KIND_NIL omits the key
	const char* m_szStaticString = nullptr;    // when set, the field is this static string and m_xValue is ignored
};
struct Zenith_ScriptResultFields
{
	Zenith_ScriptResultField m_axFields[uSCRIPT_ASYNC_RESULT_MAX_FIELDS];
	uint32 m_uFieldCount = 0;
};
using Zenith_ScriptAsyncResultDecoder = bool (*)(uint8 uPublicationKind, Zenith_ByteSpan xPayload,
	Zenith_ScriptResultFields& xOut);

// elaboration: members of Zenith_ScriptBindingTable
	void SetSideEffectConsumer(Zenith_SideEffectKind eKind, Zenith_ScriptSideEffectConsumer& xConsumer);   // AUDIO_TRIGGER, VFX_TRIGGER, SAVE_REQUEST, ASYNC_REQUEST, XR_RECENTRE
	void DeclareAsyncSource(uint8 uSourceKind, uint32 uMaxResultBytes);    // sizes each world's result arena
	void SetAsyncResultDecoder(uint8 uSourceKind, Zenith_ScriptAsyncResultDecoder pfnDecode);   // before Seal; one per kind
	Zenith_ScriptAsyncResultDecoder GetAsyncResultDecoder(uint8 uSourceKind) const;    // null: no decoder
	Zenith_ScriptSideEffectConsumer* GetSideEffectConsumer(Zenith_SideEffectKind eKind) const;
	const Zenith_ScriptFunctionDesc* FindFunction(Zenith_ScriptFunctionID xID) const;   // O(1) after Seal
	bool IsSealed() const;
	Zenith_Result<void, Zenith_ScriptBindingError> Seal();   // replaces "void Seal()": a collision is an error value

// Scripting/Bindings.md → "Public binding types" ([Z2-N-SCRIPT-922]), repeated here for the table members' order:
//   Zenith_ScriptParamDesc gains  const char* m_szResolveKind = nullptr;   (a string parameter resolved against that kind's table)
//   void AddNameTable(Zenith_StringView strKind, Zenith_Span<const Zenith_StringView> axNames);   (before Seal, MAIN_THREAD)
// Scripting/Bindings.md → "Public binding types" ([Z2-N-SCRIPT-922]; cross-doc row X193 applied): the read side of the name tables,
// used by the loader and the dispatcher
	Zenith_Span<const Zenith_StringView> FindNameTable(Zenith_StringView strKind) const;   // empty span: no table of that kind

// elaboration: the source-kind capacity the binding table and ZenithScript size their per-kind arrays by. ZenithReflect and
// ZenithScript do not depend on ZenithSave, which declares Zenith_AsyncSourceKind and uASYNC_SOURCE_KIND_CAPACITY
// (Zenith2/Docs/Architecture.md → "Multiplayer foundations"), so this mirror carries the value and
// Test_Script_Async_SourceKindCapacityMatchesRegistry proves the two equal (ruling ASYNC-SOURCE-15-16; cross-doc row X449).
inline constexpr uint32 uSCRIPT_ASYNC_SOURCE_KIND_CAPACITY = 32;

// elaboration: the Luau API classification of a binding (cross-area row MT2 (a); Zenith2/Docs/Design/MAINT/Versioning.md →
// "Luau API classification"; core text cross-doc row X451). Design/SCRIPT/Bindings.md → "Binding stability and deprecation".
enum Zenith_ScriptApiStability : uint8
{
	SCRIPT_API_STABILITY_STABLE = 1,
	SCRIPT_API_STABILITY_EXPERIMENTAL,
	SCRIPT_API_STABILITY_INTERNAL,             // engine-only functions; never listed as stable surface
};
inline constexpr uint8 uSCRIPT_API_STAGE_NONE = 0xFF;   // "no stage": not experimental, not deprecated

struct Zenith_ScriptDeprecation
{
	const char* m_szReplacement = nullptr;     // "Zenith.<Namespace>.<Name>" of a sealed function; null when not deprecated
	uint8 m_uDeprecatedStage = uSCRIPT_API_STAGE_NONE;   // D, the engine MAJOR when the deprecating diff landed
	uint8 m_uRemovedStage = uSCRIPT_API_STAGE_NONE;      // D + 1 (8 when D is 7)
};

// the four inputs a registrant passes as AddFunction's trailing parameter; the default is stable since Z2 Stage 0
struct Zenith_ScriptFunctionVersion
{
	uint8 m_uSinceStage = 0;                  // the fix version that introduced the binding (0 … 7)
	Zenith_ScriptApiStability m_eStability = SCRIPT_API_STABILITY_STABLE;
	uint8 m_uExperimentalUntilStage = uSCRIPT_API_STAGE_NONE;   // G (since … 7) when experimental
	Zenith_ScriptDeprecation m_xDeprecation;
};

// Zenith_ScriptFunctionDesc gains, after m_pUserData (all but m_szLibrary are inputs of Zenith_ScriptBindingSetHash):
	uint8 m_uSinceStage = 0;
	Zenith_ScriptApiStability m_eStability = SCRIPT_API_STABILITY_STABLE;
	uint8 m_uExperimentalUntilStage = uSCRIPT_API_STAGE_NONE;
	Zenith_ScriptDeprecation m_xDeprecation;
	const char* m_szLibrary = nullptr;         // the registering library's CMake target name (SetRegistrantLibrary); not hashed

// AddFunction gains the trailing parameter, after pUserData, copied into the four descriptor members above:
//	const Zenith_ScriptFunctionVersion& xVersion = Zenith_ScriptFunctionVersion{}
// and the table gains, MAIN_THREAD, before Seal:
	void SetRegistrantLibrary(const char* szLibrary);   // static storage; stamps m_szLibrary of every later AddFunction
```

`Seal` returns an error value instead of `void` so an ID or Luau-name collision is reported without an assert (the composition root maps it to startup exit code 3); `AddFunction` after `Seal` stays `ZENITH_ASSERT`, because it is engine code ([Z2-N-SCRIPT-564]). `Zenith_ScriptFailThunk` sets `m_eCode = Zenith_ScriptThunkErrorCodeFromKind(eKind)`; the dispatcher raises `Zenith_ScriptThunkErrorKind(xFrame.m_xError)`.

Contract blocks for the Bindings.md declarations (each `Version: since Z2 Stage 2`, because the ABI lands before the runtime so binding owners compile against it; `Zenith_ScriptInvocationContext` keeps its block from Bindings.md). The table gives each type's fields; the header writes one block per type immediately before its declaration ([Z2-N-CONTRACT-111]):

| Type | Kind | Ownership | Thread affinity | Invalidation | Errors |
|---|---|---|---|---|---|
| `Zenith_ScriptFunctionID`, `Zenith_ScriptOpaqueHandle`, `Zenith_ScriptEnumValue`, `Zenith_ScriptStructValue`, `Zenith_ScriptTicket`, `Zenith_ScriptCallbackRef`, `Zenith_ScriptCallbackOutcome`, `Zenith_WorldResourceID`, `Zenith_ScriptCapacityDemand`, `Zenith_ScriptThunkError` | value | plain value | IMMUTABLE | `Zenith_ScriptStructValue::m_pData` with its frame arena; a callback reference when released | none |
| `Zenith_ScriptValue` | value | plain value, payload by kind | IMMUTABLE | as its payload | reading the payload of another kind is `ZENITH_ASSERT` (engine code) |
| `Zenith_ScriptErrorKind`, `Zenith_ScriptThunkErrorCode`, `Zenith_ScriptBindingError`, `Zenith_ScriptValueKind`, `Zenith_ScriptThunkStatus`, `Zenith_SideEffectKind`, `Zenith_ScriptCapacityClass`, `Zenith_ScriptInvocationKind`, `Zenith_ScriptFunctionContext`, `Zenith_ScriptFunctionEffect` | enum | plain enumeration | IMMUTABLE | none | none |
| `Zenith_ScriptCallFrame` | value | built by the dispatcher in the frame arena, borrowed by one thunk call | OWNER_THREAD | when the thunk returns | a thunk fails only by returning `SCRIPT_THUNK_STATUS_ERROR` |
| `Zenith_ScriptParamDesc`, `Zenith_ScriptFunctionDesc` | value | owned by the binding table | IMMUTABLE after `Seal` | none | validated at `Seal` |
| `Zenith_ScriptApiStability` | enum | plain enumeration | IMMUTABLE | none | none |
| `Zenith_ScriptDeprecation`, `Zenith_ScriptFunctionVersion` | value | plain values; `m_szReplacement` has static storage | IMMUTABLE | none | an inconsistent combination fails `Seal` with `SCRIPT_BINDING_ERROR_INVALID_DESCRIPTOR` (Design/SCRIPT/Bindings.md → "Binding stability and deprecation") |
| `Zenith_ScriptResultField`, `Zenith_ScriptResultFields` | value | plain values on the reading invocation's stack, filled by a decoder; the names and static strings have static storage | OWNER_THREAD (the invocation's thread) | when `Result()` returns | none |
| `Zenith_ScriptAsyncResultDecoder` | functions | a pure function of the source kind's owner (kind 8: SCENE's, over `Zenith_DecodeSceneLoadResult`), registered with `SetAsyncResultDecoder` before `Seal`; a second registration for one kind is `ZENITH_ASSERT` (engine code) | ANY_THREAD (called on the reading invocation's thread; reads only its arguments) | `xPayload` is valid for the call | returns false for a payload it cannot decode; `Result()` then halts the world as an internal inconsistency |
| `Zenith_ScriptBindingTable` | type | one per process, owned by the composition root, filled at startup steps 14 and 15 | `MAIN_THREAD` until `Seal`, `IMMUTABLE` after | none | as above |
| `Zenith_ScriptValueTraits<T>` | functions | specialised by T's owning library next to T | ANY_THREAD | none | `FromValue` of the wrong kind returns `SCRIPT_THUNK_STATUS_ERROR`, sets `xError.m_uCode = 1511`, carries no text (the error has none) and leaves the output unchanged, as ECS's trait contract states (`Zenith2/Docs/Design/ECS/WorldAPI.md` → "Zenith_ECSScriptTraits header"); the dispatcher, which knows the function and argument index, formats the ZS1511 catalogue text and raises it (Design/SCRIPT/Bindings.md → "Dispatcher implementation" step 5) |

## Zenith_ScriptCompile header

`Zenith2/Docs/Scripting.md` → "Development compilation" (second code block) and `Zenith2/Docs/Scripting/PublicTypes.md` → "ZenithScriptCompiler public types" (`Zenith_ScriptModuleSource`). Additions:

<!-- assumption: pending core-doc (X121) -->
The appended `pxReport` parameter of `Zenith_ScriptCompileModule` is a declaration change Scripting.md → "Development compilation" adopts with cross-doc row X121.
<!-- /assumption -->

```cpp
// Zenith2/Engine/ZenithScriptCompiler/Zenith_ScriptCompile.h (additions)

/// @contract type Zenith_ScriptCompileReport
/// Ownership: Caller-owned; filled by the compiler functions; holds up to 64 diagnostics in fixed storage.
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

// elaboration: the report parameter is appended to the signature of Scripting.md → "Development compilation"
Zenith_Result<Zenith_ScriptBytecode, Zenith_ScriptError> Zenith_ScriptCompileModule(
	const Zenith_ScriptModuleSource& xSource, const Zenith_ScriptCompileOptions& xOptions, Zenith_Arena& xOutputArena,
	Zenith_ScriptCompileReport* pxReport = nullptr);
```

Contract block of the compile functions (`functions`): the bytecode blob is written into `xOutputArena`; thread affinity `ANY_THREAD` (the cook compiles modules on several threads, each with its own arena and report); errors `SCRIPT_ERROR_COMPILE` with the Luau line for parse, compile and parser-limit errors, `SCRIPT_ERROR_INTERNAL` for a non-Luau exception, `SCRIPT_ERROR_LOAD` for a malformed blob or a short site array in `Zenith_ScriptListFastcallSites`; `Version: since Z2 Stage 1`.

## Zenith_ScriptStateless header

<!-- assumption: pending core-doc (X121) -->
The appended `pxReport` parameter of `Zenith_ScriptCheckStateless` is a declaration change Scripting.md → "Load-time interface" adopts with cross-doc row X121.
<!-- /assumption -->

Exactly `Zenith2/Docs/Scripting.md` → "Load-time interface" and `Zenith_ScriptAcceptedModuleSet` of PublicTypes.md, with one addition: an optional last parameter `Zenith_ScriptCompileReport* pxReport = nullptr` of `Zenith_ScriptCheckStateless` that receives one diagnostic per rejection found (the first rejection in source order is still the verdict). Contract blocks: `Zenith_ScriptStatelessDiagnostic` enum; `Zenith_ScriptStatelessVerdict` value (IMMUTABLE; `m_acDetail` UTF-8 in the shape of `Zenith2/Docs/Scripting/StatelessCorpus.md` → "Diagnostic codes"); `Zenith_ScriptAcceptedModuleSet` type (OWNER_THREAD; one per cook or per hot-reload producer); `Zenith_ScriptCheckStateless` functions (ANY_THREAD; runs under `Zenith_ScriptCompilerGuarded`; a guard failure yields the verdict `SCRIPT_STATELESS_DYNAMIC_ACCESS` with detail `internal: <kind>`, so no module is ever accepted by a failed check). All `Version: since Z2 Stage 1` (the prototype lands in Stage 1, Design/SCRIPT.md → "Stage 1").

## Zenith_ScriptCook header

The cook entry points ZenithCook calls (the pipeline is Design/SCRIPT/Compiler.md → "Cook pipeline").

```cpp
// Zenith2/Engine/ZenithScriptCompiler/Zenith_ScriptCook.h
#pragma once
#include "ZenithScriptCompiler/Zenith_ScriptCompile.h"
#include "ZenithScriptCompiler/Zenith_ScriptStateless.h"

class Zenith_TypeRegistry;
class Zenith_ScriptBindingTable;

using Zenith_PfnCookCancelled = bool (*)(void* pUserData);

/// @contract value Zenith_ScriptAnalyzerRun
/// Ownership: Plain value; m_strOutput views bytes in the arena passed to RunAnalyzer.
/// Thread affinity: IMMUTABLE.
/// Invalidation: m_strOutput is invalid once that arena is reset.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_ScriptAnalyzerRun
{
	const sint32 m_iExitCode = 0;
	const Zenith_StringView m_strOutput = {};        // the analyzer's captured standard output
};

/// @contract interface Zenith_ScriptCookHost
/// Ownership: Implemented and owned by the caller of Zenith_ScriptCookGame (ZenithCook's script step, test targets, the editor);
///   borrowed for the call. ZenithScriptCompiler is an Engine/ library, held to the runtime-library header rules of ForbiddenCheck
///   rule F4 ([Z2-N-CONTRACT-577], cross-doc row X214 applied), so every file, directory and process the cook touches goes
///   through this interface.
/// Thread affinity: The thread that called Zenith_ScriptCookGame; the cook's compile workers never call it.
/// Invalidation: Returned spans and views are valid until the arena they were allocated from is reset.
/// Units: Paths are host paths spelled as the desc spells them, with '/' separators; times in nanoseconds of the host file clock.
/// Errors: Each member returns SCRIPT_ERROR_LOAD on an I/O failure (the host logs the OS detail); the cook reports the path.
/// Reentrancy: NONE.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_ScriptCookHost
{
public:
	virtual ~Zenith_ScriptCookHost() = default;
	// every file under strDirectory whose name ends in strExtension, recursively, relative to it; any order (the cook sorts)
	virtual Zenith_Result<Zenith_Span<const Zenith_StringView>, Zenith_ScriptError> ListFiles(Zenith_StringView strDirectory,
		Zenith_StringView strExtension, Zenith_Arena& xArena) = 0;
	virtual bool Exists(Zenith_StringView strPath) = 0;
	virtual Zenith_Result<Zenith_ByteSpan, Zenith_ScriptError> ReadFile(Zenith_StringView strPath, Zenith_Arena& xArena) = 0;
	// replaces the file atomically (a temporary renamed into place), creating missing directories
	virtual Zenith_Result<void, Zenith_ScriptError> WriteFile(Zenith_StringView strPath, Zenith_ByteSpan xBytes) = 0;
	virtual Zenith_Result<uint64, Zenith_ScriptError> GetModifiedTimeNs(Zenith_StringView strPath) = 0;
	// runs the program at Zenith_ScriptCookDesc::m_strAnalyzerPath with these arguments and captures its standard output
	virtual Zenith_Result<Zenith_ScriptAnalyzerRun, Zenith_ScriptError> RunAnalyzer(Zenith_Span<const Zenith_StringView> axArguments,
		Zenith_Arena& xArena) = 0;
};

/// @contract value Zenith_ScriptCookDesc
/// Ownership: Plain value; every pointer and view borrowed for the call.
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct it and set the fields they need.
/// Invalidation: None.
/// Units: Paths are host paths (tools only).
/// Errors: A null registry, binding table, host or path is SCRIPT_ERROR_INVALID_DESC.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_ScriptCookDesc
{
	Zenith_StringView m_strGameName;
	Zenith_Span<const Zenith_StringView> m_axSourceRoots;       // "<alias>=<host dir>", e.g. "game=Games/<Name>/Scripts", "zenith=Engine/ZenithScript/Scripts"
	Zenith_StringView m_strGeneratedLuauDir;                     // ${ZENITH_OUT}/Generated/Luau/<Name>
	Zenith_StringView m_strOutputDir;                            // ${ZENITH_OUT}/Cooked/<Name>/<platform>/Scripts
	Zenith_StringView m_strAnalyzerPath;                         // ${ZENITH_HOST_TOOLS_DIR}/luau-analyze
	const Zenith_TypeRegistry* m_pxTypes = nullptr;              // frozen; exactly the game's selected set
	const Zenith_ScriptBindingTable* m_pxBindings = nullptr;     // sealed; exactly the game's selected set
	uint64 m_ulEngineVersionHash = 0;
	uint64 m_ulLuauPinHash = 0;
	uint32 m_uThreadCount = 1;
	bool m_bWatch = false;                                       // file-watch mode: re-cook changed modules until cancelled
	Zenith_PfnCookCancelled m_pfnCancelled = nullptr;            // polled between modules in watch mode
	void* m_pCancelUserData = nullptr;
	Zenith_ScriptCookHost* m_pxHost = nullptr;                    // borrowed; every file, directory and process access of the cook
};

/// @contract value Zenith_ScriptCookResult
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: counters the cook accumulates.
/// Invalidation: None.
/// Units: Module counts.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_ScriptCookResult
{
	uint32 m_uModules = 0;
	uint32 m_uRecompiled = 0;
	uint32 m_uErrors = 0;
};

/// @contract functions Zenith_ScriptCookFunctions
/// Ownership: Write the files named in Compiler.md → "Cook pipeline"; allocate from the heap passed through the registry's desc.
/// Thread affinity: The calling thread; Zenith_ScriptCookGame compiles on m_uThreadCount − 1 CORE Zenith_Thread workers of its own
///   plus the calling thread, and calls m_pxHost from the calling thread alone.
/// Invalidation: None.
/// Units: None.
/// Errors: Zenith_ScriptCookGame returns SCRIPT_ERROR_COMPILE or SCRIPT_ERROR_LOAD when any module fails, after reporting every
///   module's diagnostics; it writes no .zlbc and no manifest for a failed cook, so a partial cook never reaches a package.
/// Version: since Z2 Stage 3.
/// @endcontract
Zenith_Result<Zenith_ScriptCookResult, Zenith_ScriptError> Zenith_ScriptCookGame(const Zenith_ScriptCookDesc& xDesc,
	Zenith_ScriptCompileReport& xReport);

// Static extraction of a behaviour or chunk-system declaration record (Compiler.md → "Declaration extraction").
Zenith_Result<void, Zenith_ScriptError> Zenith_ScriptExtractDeclaration(const Zenith_ScriptModuleSource& xSource,
	Zenith_Arena& xOutputArena, Zenith_ByteSpan& xRecordOut, Zenith_ScriptCompileReport* pxReport);

// The address-keyed iteration lint over luau-analyze's annotated output (Compiler.md → "Address-keyed iteration lint").
Zenith_Result<void, Zenith_ScriptError> Zenith_ScriptLintModule(const Zenith_ScriptModuleSource& xSource,
	Zenith_StringView strAnnotatedSource, Zenith_ScriptCompileReport& xReport);
```

The cook tool itself uses the STL freely ([Z2-N-CONTRACT-275]) — its `Zenith_ScriptCookHost` implementation is where host files and the analyzer process are reached — and none crosses this API ([Z2-N-CONTRACT-276]).
