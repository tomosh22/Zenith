# Zenith2 Scripting

The Luau scripting runtime of Zenith2: how scripts are compiled, loaded, sandboxed and run; how every script invocation is a transaction over engine-visible state; how the outcome stays deterministic whatever the worker count, chunk layout or scheduling; and how failing scripts are contained, recovered, debugged and shipped.

This document is normative. Every obligation is a tagged bullet `[Z2-N-SCRIPT-NNN]`; informative rationale is fenced in `<!-- informative -->` blocks. Lines introduced by `<!-- elaboration -->` are design detail added to fill a gap in the programme plan, in its spirit; reviewers check those first.

## Purpose and scope

### What this document governs

- [Z2-N-SCRIPT-001] The library `ZenithScript` (`Zenith2/Engine/ZenithScript/`) implements everything this document specifies, and no other engine library links Luau.
- [Z2-N-SCRIPT-002] Luau is the one gameplay-scripting language of Zenith2 (Z2-D-016); there is no node-graph scripting runtime and no second scripting language.
- [Z2-N-SCRIPT-003] Every script invocation, whether a chunk-system call or a behaviour callback, is a transaction over engine-visible state as specified in "Transaction scope and the entity boundary" (Z2-D-016, Z2-D-051).
- [Z2-N-SCRIPT-004] A game contains no C++ (Z2-D-002), so every gameplay rule a game adds is Luau, data-defined component schemas (Z2-D-020), or content; quest, dialogue, inventory and interaction are Luau gameplay modules inside `Zenith2/Games/OpenWorldSlice/Scripts/`, never engine features (Z2-D-077).

The document covers, in order: the exception firewall that lets Luau's C++-exception error model coexist with an exceptions-off engine; the VM model; module loading, sandboxing, bindings and generated `.d.luau` types; the two script kinds (behaviours and chunk systems); the transaction model and its algorithms; determinism; budgets; merge-time capacity and asynchronous results; stateless-system enforcement; the behaviour state model and recovery; quarantine and hot reload; view validation and the misuse errors; debugging and profiling; sandboxing and security; shipping bytecode compatibility; and the conformance and fault-matrix tests that prove all of it.

### Split documents

This entry document holds the top-level contract of every section. Eight companion files carry detail that would push this file past the 2,000-line Read window; they share this document's `SCRIPT` normative numbering and are equally binding.

| File | Headings (`##`) |
|---|---|
| `Zenith2/Docs/Scripting/Bindings.md` | "Binding registration and the thunk ABI" · "Value marshalling" · "Component views through reflection" · "Luau type generation" · "Name mapping from Zenith Hungarian" · "Data-defined component types" · "Environments and library subsets" · "Module loader" |
| `Zenith2/Docs/Scripting/StatelessCorpus.md` | "Corpus layout" · "Adversarial corpus" · "Accepted corpus" · "Diagnostic codes" · "Alternative evaluation procedure" |
| `Zenith2/Docs/Scripting/MisuseCatalogue.md` | "Message format" · "Catalogue" · "Catalogue test obligations" |
| `Zenith2/Docs/Scripting/TransactionStructures.md` | "Transaction data structures" |
| `Zenith2/Docs/Scripting/PublicTypes.md` | "Public types" |
| `Zenith2/Docs/Scripting/BudgetRows.md` | "Budget rows used" |
| `Zenith2/Docs/Scripting/LibraryLayout.md` | "Library layout and file map" |
| `Zenith2/Docs/Scripting/ConformanceTests.md` | "Conformance test tables" |

### Capability coverage

The SCRIPT row of the capability inventory, item by item, and the section that specifies each item.

| Inventory item (SCRIPT) | Specified in |
|---|---|
| Luau compilation and bytecode | "Modules, bindings and generated types"; "Shipping bytecode compatibility" |
| module loading | "Modules, bindings and generated types"; `Scripting/Bindings.md` → "Module loader" |
| generated bindings and `.d.luau` | "Modules, bindings and generated types"; `Scripting/Bindings.md` |
| the DAP debugger | "Debugging and profiling" |
| sandboxing | "Modules, bindings and generated types"; "Sandboxing and security" |
| per-invocation transactions | "Transaction scope and the entity boundary"; "Component writes shadows and side-effect buffers"; "Commit and failure semantics" |
| stateless-system enforcement (D-LUAU-STATELESS, open, Stage 1) | "Stateless system enforcement"; `Scripting/StatelessCorpus.md` |
| explicit behaviour-state model and `OnRecover` | "Behaviour state model and recovery" |
| quarantine and last-good restore | "Quarantine and hot reload" |
| hot reload and migration | "Quarantine and hot reload" |
| per-worker VMs | "VM model and threading" |
| deterministic ordering rules | "Determinism rules and randomness" |
| GC/CPU/memory budgets | "VM model and threading"; "Budgets safepoints and the watchdog" |
| script profiling | "Debugging and profiling" |
| the exception firewall | "Luau integration and the exception firewall" |
| shipping bytecode version compatibility | "Shipping bytecode compatibility" |

### Governing decisions

| ID | Decision | Where it binds here |
|---|---|---|
| Z2-D-016 | Luau scripting; every invocation is a transaction over engine-visible state | whole document |
| Z2-D-017 | Luau exception firewall: `/EHs` and `-fexceptions` only in `ZenithScript/Internal`; protected entry for every call into Luau | "Luau integration and the exception firewall" |
| Z2-D-018 | One Luau VM per worker thread per world for chunk systems; behaviours on the world main VM | "VM model and threading" |
| Z2-D-019 | Determinism guarantee, including failing ticks | "Determinism rules and randomness"; "Fault-matrix tests" |
| Z2-D-020 | Data-defined component schemas in `*.schema.json`, registered at load as reflected types | `Scripting/Bindings.md` → "Data-defined component types" |
| Z2-D-023 | Script recovery through explicit serialized behaviour state and `OnRecover` | "Behaviour state model and recovery" |
| Z2-D-045 | No exceptions outside the Luau firewall; `Zenith_Result` for recoverable errors; asserts for programmer errors | "Luau integration and the exception firewall"; "Views epochs and misuse errors" |
| Z2-D-046 | No mutable global state except the root allocator and profiler hooks | "VM model and threading" |
| Z2-D-051 | The transaction boundary is the entity, never the chunk (pre-decided; D-SCRIPT-TXN refinement is open, Stage 1) | "Transaction scope and the entity boundary" |
| Z2-D-052 | Script budgets count safepoints; wall-clock is a fatal watchdog | "Budgets safepoints and the watchdog" |
| Z2-D-053 | Asynchronous results become visible at a tick boundary in completion-ID order | "Merge-time capacity and asynchronous results" |
| Z2-D-080 | Frame phases and authority (pre-decided; D-PHASES refinement is open, Stage 1) | "Behaviours and chunk systems"; "Transaction scope and the entity boundary" |
| Z2-D-038 | JSON for authoring, versioned binary for cooked data | "Shipping bytecode compatibility"; "Behaviour state model and recovery" |
| Z2-D-048 | Compatibility policy per kind | "Shipping bytecode compatibility"; "Quarantine and hot reload" |
| Z2-D-005 | On-screen profiler with Luau time and GC | "Debugging and profiling" |
| Z2-D-075 | Modding excluded | "Sandboxing and security" |

Open decisions this document depends on: D-LUAU-STATELESS (open, Stage 1), D-LUAUDBG (open, Stage 1), D-SCRIPT-TXN refinement (open, Stage 1; the pre-decided boundary is Z2-D-051), D-PHASES refinement (open, Stage 1; the pre-decided contract is Z2-D-080), D-SEL-HASH (open, Stage 1), D-SEL-HEAP (open, Stage 1), D-SEL-CRASH (open, Stage 2).

### Stage placement

| Stage | Scripting work |
|---|---|
| Z2 Stage 1 | the exception-firewall and VM-model specification including the Luau API linkage check; D-LUAU-STATELESS (open, Stage 1) prototype and adversarial corpus; D-LUAUDBG (open, Stage 1); D-SCRIPT-TXN refinement (open, Stage 1: entity boundary, safepoint budgets, merge-time capacity, async publication); the behaviour-state model; the determinism rules; the scripting budget rows |
| Z2 Stage 2 | the Reflect runtime type registration and data-defined components that bindings and `.d.luau` generation consume; the CPU frame graph that schedules script nodes |
| Z2 Stage 3 | the `ZenithScript` runtime: firewall, VMs, module loader, bindings, `.d.luau`, behaviours, chunk systems, transactions, merge, determinism tests, fault matrix, quarantine, hot reload, DAP, profiling, bytecode cooking (the "Luau gameplay" deliverable) |
| Z2 Stage 4 | the OpenWorldSlice Luau quest, dialogue and inventory modules; editor script workflows against the W-Editor script hot-reload row |
| Z2 Stage 6 | shipping bytecode qualification and security hardening evidence for scripts |

### Relationship to the other core documents

- The frame phases, sync points, per-field authority and lifecycle commands that script nodes obey are specified in `Zenith2/Docs/Architecture.md` → "Frame phases and authority" and `Zenith2/Docs/Architecture.md` → "Lifecycle commands"; this document specifies their script-side consequences.
- The determinism guarantee itself is stated in `Zenith2/Docs/Architecture.md` → "Determinism guarantee and mechanisms"; this document specifies the script mechanisms that uphold it.
- Chunks, columns, structural epochs and change-tracking versions are ECS concepts from `Zenith2/Docs/Architecture.md` → "ECS model".
- Error handling, ownership and the construct rules are `Zenith2/Docs/Contracts.md` → "Errors and results", `Zenith2/Docs/Contracts.md` → "Ownership and allocation" and `Zenith2/Docs/Contracts.md` → "Tests and code hygiene gates".
- Budget rows named in prose here live in `Zenith2/Docs/Budgets.md` → "Scripting rows", `Zenith2/Docs/Budgets.md` → "Per-system rows" or `Zenith2/Docs/Budgets.md` → "Editor iteration rows"; `Scripting/BudgetRows.md` → "Budget rows used" gives the row ID, heading and breach code of each.

### Glossary

| Term | Meaning in this document |
|---|---|
| behaviour | a Luau module instantiated once per entity that carries it, run on the world main VM in the Early scripts (phase 2) or Late scripts (phase 10) phase, with a declared serializable state table |
| chunk system | a stateless Luau module run over every matching entity, in parallel over ECS chunks, on per-worker per-world VMs |
| invocation | one physical call into Luau for script work: one chunk-system call over one chunk, or one behaviour callback or coroutine resume segment |
| entity transaction | the logical transaction of one entity inside an invocation; the unit of rollback, commit, merge ordering and budget accounting |
| field column | the bytes of one reflected field across the rows of one chunk, whatever the backend's physical storage; "column" in this document always means a field column |
| merge point | the continuation that runs after every sub-job of a script node has joined; merge-time capacity acceptance, reduction folding and commit happen there |
| sync point | the point between two frame phases at which buffered commands are played back |
| safepoint | a point at which Luau calls the interrupt callback with `gc == -1`: loop back-edges, calls and returns |
| stable simulation ID | the 64-bit per-world entity identity assigned at spawn playback in merge order, never reused within a world session, independent of chunk and storage index |
| logical invocation ID | the layout-independent identity of an entity transaction: (stable simulation ID, invocation ordinal) |
| abort class | the error kinds a script `pcall` does not swallow: budget exhaustion, snapshot-cap overflow, yield in a system, checkpoint failure |
| fatal | an event that halts the world and is not replayable: VM memory cap exhaustion, a transaction-arena growth failure, the wall-clock watchdog, an internal exception |

<!-- informative -->
### Zenith Behaviour Graph prior art

Zenith scripted gameplay through `.bgraph` node graphs interpreted by `Zenith/Scripting/Zenith_BehaviourGraph`, with around 140 hand-registered node types in `Zenith_GraphNode_Registration*.cpp`. Its execution context carried a wall-clock `m_fTimeSeconds`, entity targeting resolved through the global `Zenith_SceneSystem::Get()` service locator, and graph state lived in a per-instance blackboard mutated in place with no rollback. Zenith2 replaces it outright with Luau for three reasons visible in that code: the hand-written node library does not scale (every engine feature needed a node written by hand, where Zenith2 generates bindings from reflection); wall-clock time and in-place mutation made behaviour non-reproducible; and the service locator is forbidden in Zenith2 by Z2-D-046. The Behaviour Graph's resume-drive model for suspended nodes is the ancestor of Zenith2's coroutine behaviours, but Zenith2 never serializes a suspended execution position (Z2-D-023).
<!-- /informative -->

## Luau integration and the exception firewall

Luau's VM reports errors by throwing a C++ exception (`lua_exception`, from `luaD_throw`) when compiled as C++, and its compiler front end throws `Luau::CompileError` and parse errors. Zenith2 compiles everything else with exceptions off (Z2-D-045). The firewall confines exception-enabled code to a closed set of targets and guarantees that no Luau error ever unwinds through a frame compiled with exceptions off (Z2-D-017).

### Exception-enabled target list

The complete list of CMake targets compiled with C++ exceptions. Every other target in the tree compiles with exceptions off.

| Target | Contents | MSVC flag | clang flag | Linked into |
|---|---|---|---|---|
| `Luau.Ast` | Luau parser and AST (throws parse errors) | `/EHs` | `-fexceptions` | `ZenithScriptCompiler`, host tools |
| `Luau.Compiler` | bytecode compiler (throws `Luau::CompileError`) | `/EHs` | `-fexceptions` | `ZenithScriptCompiler` only; never `ZenithScript`, never a Player |
| `Luau.VM` | the virtual machine (`luaD_throw` throws `lua_exception`) | `/EHs` | `-fexceptions` | `ZenithScript` |
| `Luau.CodeGen` | native code generator for `--!native` modules | `/EHs` | `-fexceptions` | `ZenithScript` |
| `Luau.Config` | `.luaurc` parsing for the analyzer | `/EHs` | `-fexceptions` | `Luau.Analyze.CLI` only |
| `Luau.Analysis` | the type checker used at cook time | `/EHs` | `-fexceptions` | `Luau.Analyze.CLI` only |
| any further `Luau.*` library target that the pinned revision's `Luau.Analysis` depends on | as enumerated in the Luau entry of `ThirdParty/manifest.json` | `/EHs` | `-fexceptions` | `Luau.Analyze.CLI` only |
| `Luau.Analyze.CLI` | the `luau-analyze` host executable, run out of process by ZenithCook | `/EHs` | `-fexceptions` | nothing (a host tool) |
| `ZenithScript` (compile group `ZenithScript_EH`) | the `Internal/**` partition of `Zenith2/Engine/ZenithScript/` (the runtime: VMs, protected entry, dispatcher, transactions); links `Luau.VM` and `Luau.CodeGen` | `/EHs` | `-fexceptions` | games, ZenithEditor, ZenithCook, script conformance executables |
| `ZenithScriptCompiler` (compile group `ZenithScriptCompiler_EH`) | the `Internal/**` partition of `Zenith2/Engine/ZenithScriptCompiler/` (bytecode compilation, the D-LUAU-STATELESS check, the fast-call listing); links `Luau.Compiler` and `Luau.Ast`; `SUPPORTED ZENITH_HOST` | `/EHs` | `-fexceptions` | ZenithCook, ZenithEditor and test targets alone; never a Player in any configuration |

- [Z2-N-SCRIPT-021] The targets in the table "Exception-enabled target list" are the complete set of targets compiled with C++ exceptions; adding a target to it needs a decision recorded against Z2-D-017. The configure checks the manifest against this table: an `exceptions: true` target outside its Luau rows, or a target an exception-enabled target links that is not itself listed, stops the configure, so a pinned revision that needs another target needs a decision recorded against Z2-D-017 first (`Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "Target mapping and host executables").
- [Z2-N-SCRIPT-022] The exception-enabled targets compile with `/EHs` on MSVC, never `/EHsc`, because `/EHsc` assumes that `extern "C"` functions never throw and may elide the cleanups around a call that reaches `lua_error`.
- [Z2-N-SCRIPT-023] The exception-enabled targets compile with `-fexceptions` on clang (Android NDK r27c and the `linux-tsan` host), and never with `-fno-exceptions`.
- [Z2-N-SCRIPT-024] The flags are applied by the build system's firewall toggle, not by a Scripting-owned helper: `ZenithScript` and `ZenithScriptCompiler` each pass `EXCEPTIONS INTERNAL` to `zenith_add_library`, which compiles the library's `Internal/**` partition as the object library `<Name>_EH` with exceptions and the rest of the library without (`Zenith2/Docs/Architecture.md` → "Build system and presets"); no other library may pass it.
<!-- elaboration -->
- [Z2-N-SCRIPT-025] Every translation unit of `ZenithScript` or `ZenithScriptCompiler` that includes a Luau header, raises or catches lives under that library's `Internal/`, so it is in the `<Name>_EH` compile group; a translation unit in a library root compiles with exceptions off and includes no Luau header.
<!-- elaboration -->
- [Z2-N-SCRIPT-052] `ZenithScriptCompiler` is the compile-time half of the firewall: a separate library declared `SUPPORTED ZENITH_HOST` in the build system's matrix, linked by the host tools ZenithCook and ZenithEditor and by test targets, and by no Player in any configuration, so no runtime target links the Luau compiler; its absence from every game's Shipping link closure is checked by the build system's Libgraph rule (`Zenith2/Docs/Architecture.md` → "Build system and presets"), not by dead-stripping, and `ZenithScript` has no dependency on it (the dependency runs the other way, for shared public types).
<!-- elaboration -->
- [Z2-N-SCRIPT-026] The ZenithScript test target (`Zenith2/Engine/ZenithScript/Tests/`) compiles with exceptions off like every other test target; the firewall tests observe the RAII objects of the exception-enabled production dispatcher instead of defining exception-enabled test code (see "The two RAII tests").

### Exceptions-off everywhere else

- [Z2-N-SCRIPT-027] Every target not in the table "Exception-enabled target list" compiles with exceptions off: on MSVC the inherited `/EHsc` is removed from `CMAKE_CXX_FLAGS`, `/EHs-c-` is added and `_HAS_EXCEPTIONS=0` is defined; on clang `-fno-exceptions` is added; every target in the table compiles with `/EHs` and an explicit `_HAS_EXCEPTIONS=1` on MSVC (`-fexceptions` on clang), and every target, in the table or not, compiles with RTTI off (`/GR-`, `-fno-rtti`); the third-party targets receive these as the rows of `Zenith2/Docs/Architecture/BuildSystem.md` → "Third-party targets and configuration variables".
- [Z2-N-SCRIPT-028] The `ForbiddenCheck` gate step reads the CMake File API compile groups and fails when `/EHs`, `/EHsc`, `/EHa` or `-fexceptions` appears on any compile group other than `ZenithScript_EH`, `ZenithScriptCompiler_EH` and the Luau targets of the table, or when the exceptions-off flags are missing from any other group (`Zenith2/Docs/Architecture.md` → "Build system and presets").
- [Z2-N-SCRIPT-029] The tokens `try`, `catch` and `throw` appear in C++ source only under `Zenith2/Engine/ZenithScript/Internal/` and `Zenith2/Engine/ZenithScriptCompiler/Internal/`; `ForbiddenCheck` fails on any occurrence elsewhere in `Zenith2/Engine`, `Zenith2/Tools`, `Zenith2/Player` or `Zenith2/Benchmarks`.
- [Z2-N-SCRIPT-030] Inside the two firewall directories, `throw` is never written by Zenith2 code (Luau alone throws), and `catch` appears in two files, `ZenithScript/Internal/Zenith_ScriptProtectedCall.cpp` and `ZenithScriptCompiler/Internal/Zenith_ScriptCompilerGuard.cpp`; `ForbiddenCheck` fails on any other occurrence.

### Luau API linkage check

Whether the pinned Luau's C API (`lua.h`, `lualib.h`, `luacode.h`) has C or C++ language linkage decides how dangerous `/EHsc` would be; `/EHs` is used either way.

- [Z2-N-SCRIPT-031] The Stage 1 firewall ticket measures the language linkage of the pinned Luau API by inspecting the symbols of the built `Luau.VM` and `Luau.Compiler` libraries (`dumpbin /SYMBOLS` on Windows, `llvm-nm` for Android): a mangled `lua_pcall` symbol (`?lua_pcall@@…` on MSVC, `_Z9lua_pcall…` on clang) is C++ linkage, an unmangled `lua_pcall` is C linkage.
- [Z2-N-SCRIPT-032] The measured linkage is recorded in the Luau entry of `ThirdParty/manifest.json` as `"apiLinkage": "cxx"` or `"apiLinkage": "c"`, together with the symbol names inspected.
- [Z2-N-SCRIPT-033] The CTest `Test_Script_Firewall_LuauApiLinkageRecorded` repeats the measurement on every run of the `Test` step and fails when the measured linkage differs from the recorded value, so a Luau pin bump that changes linkage cannot land unnoticed.

### The protected entry

Every entry into Luau from anywhere in Zenith2 goes through one function in `ZenithScript/Internal`.

```cpp
// Zenith2/Engine/ZenithScript/Internal/Zenith_ScriptProtectedCall.h
#pragma once
#include "Zenith_ScriptError.h"
#include "Internal/Zenith_ScriptVM.h"

struct lua_State;

enum Zenith_ScriptCallKind : uint8
{
	SCRIPT_CALL_KIND_FUNCTION,    // call the function below m_uArgCount arguments on m_pxThread's stack
	SCRIPT_CALL_KIND_RESUME,      // resume the coroutine m_pxThread with m_uArgCount arguments
	SCRIPT_CALL_KIND_NATIVE_BODY, // run m_pfnBody on m_pxThread under protection (loads, table builds, state (de)serialization)
};

enum Zenith_ScriptCallStatus : uint8
{
	SCRIPT_CALL_STATUS_RETURNED,  // the function returned; results are on the stack
	SCRIPT_CALL_STATUS_YIELDED,   // the coroutine yielded; yielded values are on its stack
};

using Zenith_ScriptNativeBody = int (*)(lua_State* pxState);  // a lua_CFunction; reads its context from upvalue 1 (light userdata)

struct Zenith_ScriptCallRequest
{
	Zenith_ScriptCallKind m_eKind = SCRIPT_CALL_KIND_FUNCTION;
	lua_State* m_pxThread = nullptr;             // the VM's main thread, a behaviour coroutine, or a worker VM thread
	uint32 m_uArgCount = 0;
	uint32 m_uMaxResults = 0;                   // LUA_MULTRET is not accepted; results beyond this are dropped
	Zenith_ScriptNativeBody m_pfnBody = nullptr; // SCRIPT_CALL_KIND_NATIVE_BODY only
	void* m_pBodyContext = nullptr;              // pushed as light userdata upvalue 1 of m_pfnBody
	uint64 m_ulSafepointLimit = 0;              // 0 = unbudgeted (load-time and tool calls only)
	Zenith_ScriptInvocationID m_xInvocationID;   // the invocation this call belongs to; views compare against it
};

struct Zenith_ScriptCallOutcome
{
	Zenith_ScriptCallStatus m_eStatus = SCRIPT_CALL_STATUS_RETURNED;
	uint32 m_uResultCount = 0;
	uint64 m_ulSafepoints = 0;                  // safepoints counted during this call (interrupt calls with gc == -1)
};

using Zenith_ScriptCallResult = Zenith_Result<Zenith_ScriptCallOutcome, Zenith_ScriptError>;

Zenith_ScriptCallResult Zenith_ScriptProtectedCall(Zenith_ScriptVM& xVM, const Zenith_ScriptCallRequest& xRequest);
```

```cpp
// Zenith2/Engine/ZenithReflect/Zenith_ScriptBinding.h (excerpt): the error kinds are declared with the Luau-free binding ABI
enum Zenith_ScriptErrorKind : uint8
{
	SCRIPT_ERROR_KIND_RUNTIME,          // error(), a runtime type error, or a thunk failure; catchable by script pcall
	SCRIPT_ERROR_KIND_MISUSE,           // a catalogued controlled misuse error (Scripting/MisuseCatalogue.md); catchable by script pcall
	SCRIPT_ERROR_KIND_BUDGET,           // safepoint budget exhausted; abort class
	SCRIPT_ERROR_KIND_SNAPSHOT_CAP,     // per-entity transaction memory cap exceeded; abort class
	SCRIPT_ERROR_KIND_RESERVATION,      // a transaction arena could not grow from the general heap; fatal
	SCRIPT_ERROR_KIND_YIELD_IN_SYSTEM,  // coroutine.yield inside a chunk system; abort class
	SCRIPT_ERROR_KIND_CHECKPOINT,       // behaviour checkpoint serialization failed; abort class
	SCRIPT_ERROR_KIND_VM_MEMORY,        // the VM memory cap was hit (LUA_ERRMEM); fatal
	SCRIPT_ERROR_KIND_WATCHDOG,         // the wall-clock watchdog tripped; fatal
	SCRIPT_ERROR_KIND_INTERNAL,         // a non-Luau C++ exception reached the protected entry; fatal
	SCRIPT_ERROR_KIND_COMPILE,          // parse or compile error (compile entry)
	SCRIPT_ERROR_KIND_LOAD,             // bytecode refused, stateless rejection, declaration or state-schema load error
};
```

The public error header `Zenith2/Engine/ZenithScript/Zenith_ScriptError.h` (no Luau types) declares `Zenith_ScriptErrorCode`, the 32-byte `Zenith_ScriptError`, `Zenith_ScriptDiagnostic`, `Zenith_ScriptErrorCodeFromKind` and `Zenith_ToString(Zenith_ScriptErrorCode)`; their declarations are `Scripting/PublicTypes.md` → "Public types" (heading "ZenithScript error types").

- [Z2-N-SCRIPT-055] `Zenith_ScriptError` is the `Zenith_Result` error of every ZenithScript C++ API in the struct form of `Zenith2/Docs/Contracts.md` → "Errors and results": a trivially copyable record of at most 32 bytes whose first member is `Zenith_ScriptErrorCode m_eCode` (numbered from 1: codes 1–12 are the twelve `Zenith_ScriptErrorKind` values plus one, codes 13–21 the C++-API failures), followed by the kind, the ZS catalogue code, the module, the line, the diagnostic sequence and the failing transaction's stable simulation ID; it carries no text.
- [Z2-N-SCRIPT-056] The message and traceback of every script failure are recorded, before the protected call returns, as a `Zenith_ScriptDiagnostic` in the diagnostic store of the failing world (one 64-slot store per `Zenith_ScriptWorld`, one more in `Zenith_ScriptRuntime` for failures outside a world), named by the error's `m_uDiagnosticSequence` and read through `Zenith_ScriptWorld::GetDiagnostic` or `Zenith_ScriptRuntime::GetDiagnostic`, out of tick or on the world's tick-boundary node; a slot overwritten by 64 newer diagnostics reads `false`, and sequence numbers are diagnostic only: no simulation value, merge decision or replayed quantity reads them.

- [Z2-N-SCRIPT-034] `Zenith_ScriptProtectedCall` is declared in `Internal/Zenith_ScriptProtectedCall.h`, defined in `Internal/Zenith_ScriptProtectedCall.cpp`, and callable by `ZenithScript/Internal` code alone; public `ZenithScript` APIs that run script route to it.
- [Z2-N-SCRIPT-035] `lua_pcall`, `lua_resume` and `luau_load` (and any further protected-call entry point the pinned `lua.h` declares, confirmed by the Stage 1 linkage ticket) appear in exactly one translation unit, `Internal/Zenith_ScriptProtectedCall.cpp`; `ForbiddenCheck` fails on any other occurrence.
- [Z2-N-SCRIPT-036] Every Luau API call that can allocate or raise runs inside a protected region established by `Zenith_ScriptProtectedCall`; the exceptions are `lua_newstate`, `lua_close` and the non-raising stack-inspection calls listed in `Scripting/Bindings.md` → "Binding registration and the thunk ABI".
- [Z2-N-SCRIPT-037] `Zenith_ScriptVM` keeps a protected-region depth counter `m_uProtectedDepth`; in Debug builds the VM allocator hook asserts that the counter is non-zero for every allocation after VM construction completes, which catches an unprotected allocating call on its first execution.
- [Z2-N-SCRIPT-038] `Zenith_ScriptProtectedCall` never lets any exception escape (it carries no exception specification, `Zenith2/Docs/Contracts.md` [Z2-N-CONTRACT-582]; its own catch clauses are the guarantee); its `catch (const std::exception&)` and `catch (...)` clauses, and the compiler guard's in "The ZenithScriptCompiler library", are the only `catch` clauses in Zenith2, and they convert a non-Luau exception into a `SCRIPT_ERROR_KIND_INTERNAL` (runtime) or `SCRIPT_ERROR_KIND_COMPILE` (compiler) error.
- [Z2-N-SCRIPT-039] `Zenith_ScriptProtectedCall` returns every Luau error as a `Zenith_ScriptError` value and never asserts on a script error: script misuse always raises a controlled Luau error and never relies on an assert (Z2-D-045).
- [Z2-N-SCRIPT-040] Restated (Z2-D-141): `Zenith_ScriptCallResult` is a `Zenith_Result` with no `[[nodiscard]]` (`Zenith2/Docs/Contracts.md` [Z2-N-CONTRACT-586]); every caller handles the script failure it reports or discards it explicitly with a reason ([Z2-N-CONTRACT-402]), which review confirms.

### Protected entry algorithm

`Zenith_ScriptProtectedCall(xVM, xRequest)`:

1. Record `xVM.m_uProtectedDepth += 1` through an RAII `Zenith_ScriptProtectedDepthScope`, and install the request's budget: `xVM.m_ulSafepointLimit = xRequest.m_ulSafepointLimit`, `xVM.m_ulSafepointCount = 0` (a nested call, made while an enclosing protected call is running, instead inherits the enclosing limit minus the enclosing count), `xVM.m_xCurrentInvocationID = xRequest.m_xInvocationID`, and publish the invocation start time to the watchdog (`xVM.m_ulWatchdogStartTicks.store(now, memory_order_release)`).
2. Push the error handler `Zenith_ScriptErrorHandler` (a `lua_CFunction` that captures `debug.traceback` into the VM's error scratch before the stack unwinds) below the callee and its arguments.
3. Dispatch on `m_eKind`:
   - `SCRIPT_CALL_KIND_FUNCTION`: `lua_pcall(m_pxThread, m_uArgCount, m_uMaxResults, iHandlerIndex)`;
   - `SCRIPT_CALL_KIND_RESUME`: `lua_resume(m_pxThread, nullptr, m_uArgCount)`; on an error status, the traceback is taken from the coroutine with `lua_debugtrace` before the thread is reset;
   - `SCRIPT_CALL_KIND_NATIVE_BODY`: push `m_pfnBody` as a C closure with `m_pBodyContext` as light-userdata upvalue 1, then `lua_pcall` it.
4. The whole of step 3 sits in `try { … } catch (const std::exception& xException) { … } catch (...) { … }`. Luau's own `lua_exception` does not reach these clauses because `lua_pcall` and `lua_resume` catch it; they exist for exceptions Luau does not own (for example `std::bad_alloc` from a compiler container), which become `SCRIPT_ERROR_KIND_INTERNAL`.
5. Classify the status: `LUA_OK` → `SCRIPT_CALL_STATUS_RETURNED`; `LUA_YIELD` → `SCRIPT_CALL_STATUS_YIELDED`; `LUA_ERRRUN` → the kind stored in `xVM.m_eRaisedKind` by whichever engine path raised (default `SCRIPT_ERROR_KIND_RUNTIME`); `LUA_ERRMEM` → `SCRIPT_ERROR_KIND_VM_MEMORY`; `LUA_ERRERR` → `SCRIPT_ERROR_KIND_INTERNAL`.
6. On error, copy the message (from the stack top, `tostring`-converted inside the handler) and the traceback into the VM's error scratch and then into a `Zenith_ScriptDiagnostic` of the world's diagnostic store, fill `Zenith_ScriptError` (`m_eCode = Zenith_ScriptErrorCodeFromKind(kind)`, the `ZS<code>` prefix parsed into `m_uCode` when present, the store's sequence into `m_uDiagnosticSequence`), pop the error, and restore the stack top recorded at step 1.
7. Clear the watchdog start time (a nested call leaves the enclosing call's start time in place), restore the enclosing budget and invocation ID (protected calls nest when an engine callback re-enters script) and add the nested call's safepoints to the enclosing count, and let the depth scope decrement `m_uProtectedDepth`.
8. Return `Zenith_ScriptCallOutcome` with `m_ulSafepoints = xVM.m_ulSafepointCount`, or the error.

Nesting is supported to a depth of `uSCRIPT_MAX_PROTECTED_DEPTH = 8`; a deeper re-entry is refused before step 2 with a `SCRIPT_ERROR_KIND_MISUSE` error (catalogue code ZS1701), which bounds the C stack used by callback re-entry.

- [Z2-N-SCRIPT-041] A nested protected call consumes the enclosing entity transaction's safepoint budget: its limit is the enclosing call's remaining budget and its safepoints are added to the enclosing count, so re-entry through a callback never resets a budget.
- [Z2-N-SCRIPT-042] Protected calls nest to a depth of at most `uSCRIPT_MAX_PROTECTED_DEPTH` (8); a re-entry beyond it is refused with the catalogued misuse error before any Luau code runs.

### noexcept rule for engine functions called from bindings

Bindings are split in two halves (full ABI in `Scripting/Bindings.md` → "Binding registration and the thunk ABI"): the dispatcher in `ZenithScript/Internal` (exception-enabled) converts between the Luau stack and a `Zenith_ScriptCallFrame`, and a thunk generated by a registration template in the owning engine library (exceptions off) converts between the frame and the engine function; the thunk holds no `lua_State`.

- [Z2-N-SCRIPT-043] Restated (Z2-D-141; the `noexcept` rule this heading names is withdrawn): an engine function reachable from a binding thunk carries no exception specification (`Zenith2/Docs/Contracts.md` [Z2-N-CONTRACT-582]) and cannot throw, because it and the thunk that `Zenith_ScriptBindingTable::AddFunction<auto pfnFunction>` instantiates are compiled in the owning engine library with exceptions off; `AddFunction` performs no nothrow check, and no Luau error unwinds through the thunk or the engine function, because the dispatcher raises only after the thunk has returned ([Z2-N-SCRIPT-044]) and a re-entry into script goes through the protected entry ([Z2-N-SCRIPT-046]).
- [Z2-N-SCRIPT-044] Every thunk has the signature `Zenith_ScriptThunkStatus (*)(Zenith_ScriptCallFrame& xFrame)` and reports failure by returning `SCRIPT_THUNK_STATUS_ERROR` with `xFrame.m_xError` filled; the dispatcher raises the Luau error after the thunk has returned, from inside the exception-enabled translation unit.
- [Z2-N-SCRIPT-045] Luau headers (`lua.h`, `lualib.h`, `luacode.h`, `luacodegen.h`, and anything under `Luau/`) are included only by `ZenithScript/Internal` and `ZenithScriptCompiler/Internal` translation units; `ForbiddenCheck` fails on an include anywhere else, and libgraph's leakage check fails if one reaches a public header.

### Engine callbacks that re-enter script

An engine function called from a binding may need to run script again (a physics query with a Luau filter callback, a UI binding raising a Luau handler). The re-entry goes through the same protected entry, so the inner script's error stops at the inner entry and returns to the engine frame as a value.

- [Z2-N-SCRIPT-046] An engine callback that runs script calls `Zenith_ScriptInvocationContext::InvokeCallback` on the context its thunk received; the interface is declared in ZenithReflect (`Scripting/Bindings.md` → "Binding registration and the thunk ABI") and implemented in `ZenithScript/Internal`, which enters Luau through `Zenith_ScriptProtectedCall`; no engine library holds a `lua_State`, calls a Luau function directly, or depends on `ZenithScript`.
- [Z2-N-SCRIPT-047] A Luau error raised inside a re-entered callback is caught at the inner protected entry and returned to the calling engine frame as a `Zenith_ScriptThunkError` value (its 8-byte code-led record: `m_eCode` of `Zenith_ScriptThunkErrorCode`, the kind plus one; the ZS code; the VM's error-scratch sequence), whose text the engine frame reads through `Zenith_ScriptInvocationContext::GetErrorMessage`; no engine frame compiled with exceptions off is ever unwound.
- [Z2-N-SCRIPT-048] An abort-class or fatal error returned from a re-entered callback is re-raised by the dispatcher of the outer binding after the engine function returns, with the same kind, so the outer invocation fails with it and budget exhaustion cannot be laundered through a callback.

```cpp
// Zenith2/Engine/ZenithReflect/Zenith_ScriptBinding.h (excerpt of the Zenith_ScriptInvocationContext interface)
virtual Zenith_Result<Zenith_ScriptCallbackOutcome, Zenith_ScriptThunkError> InvokeCallback(
	Zenith_ScriptCallbackRef xCallback,            // a Luau function captured as a registry reference by the dispatcher
	const Zenith_ScriptValue* axArgs, uint32 uArgCount,
	Zenith_ScriptValue* axResults, uint32 uResultCapacity) = 0;
```

### ForbiddenCheck rules

The Luau-specific rules the `ForbiddenCheck` gate step enforces. Paths are relative to `Zenith2/`.

| Rule | Pattern | Allowed only in | Diagnostic |
|---|---|---|---|
| no unprotected call | the tokens `lua_call` and `lua_callk` | `Engine/ZenithScript/Internal/**` (where every use sits inside a protected region) | `FORBIDDEN_LUA_CALL` |
| one protection site | `lua_pcall`, `lua_resume`, `luau_load` | `Engine/ZenithScript/Internal/Zenith_ScriptProtectedCall.cpp` | `FORBIDDEN_LUA_PROTECTION_SITE` |
| no Luau headers outside the firewall | `#include` of `lua.h`, `lualib.h`, `luacode.h`, `luacodegen.h`, `Luau/…` | `Engine/ZenithScript/Internal/**`, `Engine/ZenithScriptCompiler/Internal/**` | `FORBIDDEN_LUAU_INCLUDE` |
| exceptions confined | the tokens `try`, `catch`, `throw` | `Engine/ZenithScript/Internal/**`, `Engine/ZenithScriptCompiler/Internal/**` (`catch` in `Zenith_ScriptProtectedCall.cpp` and `Zenith_ScriptCompilerGuard.cpp` alone; `throw` nowhere) | `FORBIDDEN_EXCEPTION_TOKEN` |
| exception flags confined | `/EHs`, `/EHsc`, `/EHa`, `-fexceptions` in File API compile fragments | the targets in "Exception-enabled target list" | `FORBIDDEN_EXCEPTION_FLAG` |
| no raw error longjmp | `setjmp`, `longjmp`, `LUA_USE_LONGJMP` | nowhere | `FORBIDDEN_LONGJMP` |
| no bytecode compile at shipping runtime | `luau_compile`, `Luau::compile`, `Luau::Parser` | `Engine/ZenithScriptCompiler/Internal/**` | `FORBIDDEN_LUAU_COMPILE_SITE` |

- [Z2-N-SCRIPT-049] `ForbiddenCheck` enforces every row of the table "ForbiddenCheck rules"; a `lua_call` or `lua_callk` outside `ZenithScript/Internal` is a gate failure, whatever the file (Z2-D-017).
- [Z2-N-SCRIPT-050] Luau is always built with its default C++-exception error model; defining `LUA_USE_LONGJMP` would make `lua_error` skip C++ destructors and is a gate failure.

### The ZenithScriptCompiler library

<!-- elaboration -->
```cpp
// Zenith2/Engine/ZenithScriptCompiler/Internal/Zenith_ScriptCompilerGuard.h
using Zenith_ScriptCompilerBody = Zenith_Result<void, Zenith_ScriptError> (*)(void* pContext);

// Runs pfnBody inside try { … } catch (const Luau::CompileError&) { … } catch (const std::exception&) { … } catch (...) { … };
// Luau::CompileError and parse errors become SCRIPT_ERROR_KIND_COMPILE with the Luau line; anything else SCRIPT_ERROR_KIND_INTERNAL.
Zenith_Result<void, Zenith_ScriptError> Zenith_ScriptCompilerGuarded(Zenith_ScriptCompilerBody pfnBody, void* pContext);
```

- [Z2-N-SCRIPT-053] Every call from `ZenithScriptCompiler` into `Luau.Ast` or `Luau.Compiler` runs inside `Zenith_ScriptCompilerGuarded`, whose public callers (`Zenith_ScriptCompileModule`, `Zenith_ScriptCheckStateless`) let no exception escape and return `Zenith_Result`; no compiler exception reaches ZenithCook, ZenithEditor or a Player frame.
- [Z2-N-SCRIPT-054] The Shipping link closure of every game (the `win-shipping` and `android-arm64-shipping` build presets, as the build system defines it) contains neither `ZenithScriptCompiler`, `Luau.Compiler` nor `Luau.Ast`, which the `Libgraph` step checks (`Zenith2/Docs/Architecture.md` → "Build system and presets").

### The two RAII tests

The firewall exists so that RAII cleanup runs when a Luau error propagates through binding code. Two tests prove it.

| Test | Setup | Assertion |
|---|---|---|
| `Test_Script_Firewall_RaiiUnwindsThroughBinding` | A test binding `Test.FailingThunk` (registered through the ordinary thunk ABI from the exceptions-off test target) returns `SCRIPT_THUNK_STATUS_ERROR`; a second case calls a real binding with a wrong-typed argument so that the dispatcher's argument conversion raises ZS1511 through `luaL_error` two frames below the dispatcher, while a further RAII object of the conversion helper (`Zenith_ScriptArgScratch`) is live. In both cases the dispatcher's RAII objects are live when the error is raised: `Zenith_ScriptDispatchScope` (frame-arena mark and live-scope counter) and `Zenith_ScriptProtectedDepthScope`. | After `Zenith_ScriptProtectedCall` returns the error, `Zenith_ScriptDiagnostics::GetLiveDispatchScopes(xVM) == 0`, the frame arena is back at its pre-call mark, `m_uProtectedDepth` is back at its pre-call value, the error is `SCRIPT_ERROR_RUNTIME` (kind `SCRIPT_ERROR_KIND_RUNTIME`) and its diagnostic's message is the thunk's message (first case), or `SCRIPT_ERROR_MISUSE` (kind `SCRIPT_ERROR_KIND_MISUSE`) with the exact ZS1511 catalogue text in its diagnostic (second case), `Zenith_ScriptArgScratch` released its scratch, and a second call on the same VM succeeds. This is valid only because the dispatcher's translation unit is exception-enabled. |
| `Test_Script_Firewall_ReentrantCallbackErrorCaught` | Script calls `Test.CallBack(fn)`; its thunk calls an engine function in an exceptions-off test translation unit, which calls `Zenith_ScriptInvocationContext::InvokeCallback` with `fn`; `fn` raises `error("inner")`. The engine function sets `bReturnedNormally = true` after `InvokeCallback` returns and returns the error code to the thunk as a value; the outer script then returns 42. | `InvokeCallback` returned a `Zenith_ScriptThunkError` with `m_eCode == SCRIPT_THUNK_ERROR_RUNTIME`, and `GetErrorMessage` on it copied a message ending in `inner`; `bReturnedNormally == true` (so the exceptions-off frame was not unwound); the outer protected call returned `SCRIPT_CALL_STATUS_RETURNED` with result 42; the live-scope counter is 0. A variant makes `fn` exhaust its budget and asserts that the outer call fails with `SCRIPT_ERROR_KIND_BUDGET`. |

- [Z2-N-SCRIPT-051] `Test_Script_Firewall_RaiiUnwindsThroughBinding` and `Test_Script_Firewall_ReentrantCallbackErrorCaught` are part of the ZenithScript test target and pass on `win-debug`, `win-release`, `win-asan` and `linux-tsan`, and in the S24 HWASan run of each stage acceptance.

## VM model and threading

### VM ownership table

| VM | Owner (`Zenith_Owned` holder) | Count | Created | Runs | Thread affinity | Memory cap row |
|---|---|---|---|---|---|---|
| world main VM | `Zenith_ScriptWorld::m_xMainVM` | exactly 1 per world | when the world is created | every behaviour callback and coroutine segment of the world (Early and Late scripts phases), behaviour state checkpoints and restores, `OnRecover`, hot-reload migration, the editor script console | the main thread (the frame graph's main-thread affinity node) | the main VM memory cap row |
| chunk-system VM | `Zenith_ScriptWorld::m_axWorkerVMs[uWorkerIndex]` | at most 1 per (worker thread, world); a worker that never runs a chunk system of the world has none | lazily, on the first chunk-system sub-job of that world scheduled on that worker | chunk-system invocations of that world | the owning worker thread | the worker VM memory cap row |
| no VM | ZenithCook, the shipping Player's loader | 0 | — | compilation (`Zenith_ScriptCompileModule`) and cook-time type checking (`luau-analyze`, out of process) run without a VM | — | — |

- [Z2-N-SCRIPT-071] Each world owns exactly one main VM, and every behaviour of that world runs on it on the main thread, in the Early scripts or Late scripts phase the behaviour declares (Z2-D-018, Z2-D-080).
- [Z2-N-SCRIPT-072] Chunk systems run on per-worker, per-world VMs; a chunk-system VM is used by its owning worker thread alone and is never shared across worlds (Z2-D-018).
- [Z2-N-SCRIPT-073] Worlds never share a VM, a Luau value or a registry reference; world isolation (`Zenith2/Docs/Architecture.md` → "Worlds and isolation") holds for scripts as for ECS and physics.
- [Z2-N-SCRIPT-074] The VMs are owned by `Zenith_ScriptWorld` and passed nothing global: there is no process-wide VM, no static `lua_State*` and no service lookup of a VM (Z2-D-046).
- [Z2-N-SCRIPT-092] A world holds at most `uSCRIPT_MAX_WORKER_VM_SLOTS` (254) worker VM slots, so a VM index is at most 254; the index `0xFFFF` (`uSCRIPT_RESERVED_VM_INDEX`) is never issued to a VM and never appears in the top 16 bits of a `Zenith_ScriptInvocationID`, because ECS uses it for the invocation scopes of its own playbacks (`Zenith2/Docs/Design/ECS/WorldAPI.md` → "Zenith_ChunkView header").
- [Z2-N-SCRIPT-093] The members `m_bDebuggerStopped` and `m_ulWatchdogRebaseTicks` exist in every configuration (no layout-changing conditional, `Zenith2/Docs/Contracts.md` → "No layout-changing conditionals") and are written only in development builds, by the VM's own thread, as "Wall-clock watchdog" step 4 states.

```cpp
// Zenith2/Engine/ZenithScript/Internal/Zenith_ScriptVM.h (excerpt)
enum Zenith_ScriptVMRole : uint8
{
	SCRIPT_VM_ROLE_MAIN,
	SCRIPT_VM_ROLE_WORKER,
};

class Zenith_ScriptVM
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_ScriptVM>, Zenith_ScriptError> Create(const Zenith_ScriptVMDesc& xDesc);
	~Zenith_ScriptVM();                          // lua_close; releases every registry reference first

	lua_State* GetMainThread() const { return m_pxState; }
	Zenith_ScriptVMRole GetRole() const { return m_eRole; }
	uint64 GetBytesInUse() const { return m_ulBytesInUse; }

	void StepGC(uint32 uStepKB);               // one bounded incremental GC step, outside any invocation

	// fields read by the allocator hook and the interrupt callback
	uint64 m_ulBytesInUse = 0;
	uint64 m_ulMemoryCapBytes = 0;
	uint32 m_uProtectedDepth = 0;
	uint64 m_ulSafepointCount = 0;
	uint64 m_ulSafepointLimit = 0;
	Zenith_ScriptErrorKind m_eRaisedKind = SCRIPT_ERROR_KIND_RUNTIME;
	Zenith_ScriptInvocationID m_xCurrentInvocationID;
	std::atomic<uint64> m_ulWatchdogStartTicks;         // 0 when no invocation is running
	std::atomic<bool> m_bWatchdogTripped;
	std::atomic<uint32> m_uBreakpointGeneration;        // DAP fan-out (development builds)
	std::atomic<bool> m_bDebuggerStopped;                // set by the VM's own thread while it is stopped in the debugger (development builds; false otherwise)
	uint64 m_ulWatchdogRebaseTicks = 0;                  // running sum of the VM's debugger-stop durations (development builds; 0 otherwise)

private:
	lua_State* m_pxState = nullptr;
	Zenith_ScriptVMRole m_eRole = SCRIPT_VM_ROLE_MAIN;
	uint16 m_uVMIndex = 0;                               // 0 = main VM; 1 + worker slot for worker VMs; 0xFFFF (uSCRIPT_RESERVED_VM_INDEX) is never issued
	uint64 m_ulNextInvocationCounter = 0;                // low 48 bits of invocation IDs issued by this VM
};
```

### Lazy worker VM creation

1. A chunk-system sub-job starts on worker `w` for world `W` and reads `W.m_axWorkerVMs[w]`.
2. If the slot is empty, the sub-job calls `Zenith_ScriptVM::Create` with `SCRIPT_VM_ROLE_WORKER`, the worker VM memory cap, and the world's module set; the new VM gets the system environment (`Scripting/Bindings.md` → "Environments and library subsets") and loads no module yet.
3. The first invocation of system module `M` on that VM loads `M` and its accepted dependencies from the world's cooked bytecode (already validated by the D-LUAU-STATELESS check at world load), caches the module table in the VM's registry, and increments the VM's module-generation record for `M`.
4. Subsequent invocations reuse the cached module unless the world's module generation for `M` has advanced (hot reload), in which case step 3 repeats before the invocation.

- [Z2-N-SCRIPT-075] A worker VM slot is written only by its owning worker during a sub-job, and read or destroyed by the main thread only while no sub-job of the world is running (world teardown or a quiescent tick boundary); no lock guards the slot.
- [Z2-N-SCRIPT-076] Lazy creation and module loading have no observable effect on simulation results: which worker created a VM, and when, never changes any committed value.
- [Z2-N-SCRIPT-077] A failure to create a worker VM (the allocator refusing the initial state) is fatal for the world, reported as `SCRIPT_ERROR_KIND_VM_MEMORY`.

### Memory cap and allocator hook

Every VM is created with `lua_newstate(Zenith_ScriptAlloc, pxVM)`:

```cpp
// Zenith2/Engine/ZenithScript/Internal/Zenith_ScriptAllocator.cpp
void* Zenith_ScriptAlloc(void* pUserData, void* pBlock, size_t ulOldSize, size_t ulNewSize);
```

1. `pUserData` is the owning `Zenith_ScriptVM`.
2. `ulNewSize == 0`: free `pBlock` through the general heap (`Zenith_Heap`), subtract `ulOldSize` from `m_ulBytesInUse`, return `nullptr`.
3. Otherwise compute `ulGrowth = ulNewSize - ulOldSize` when growing; if `m_ulBytesInUse + ulGrowth > m_ulMemoryCapBytes`, return `nullptr` without touching `pBlock` (Luau raises `LUA_ERRMEM`, classified `SCRIPT_ERROR_KIND_VM_MEMORY`).
4. Otherwise reallocate through the general heap, which tracks sizes internally so no header is added; on heap failure return `nullptr`; on success update `m_ulBytesInUse`.
5. In Debug builds, assert `m_uProtectedDepth > 0` once the VM has finished construction (see "Protected entry algorithm").

- [Z2-N-SCRIPT-078] Every Luau allocation of every VM goes through `Zenith_ScriptAlloc` and the general heap; no Luau memory comes from a frame arena, a pool or a raw `malloc` (`Zenith2/Docs/Contracts.md` → "Ownership and allocation").
- [Z2-N-SCRIPT-079] The main VM's cap comes from the main VM memory cap row and every worker VM's cap from the worker VM memory cap row; the product of the worker cap and the worker count is bounded by the worker VM memory total row, and the memory governor's script budget (`Zenith2/Docs/Architecture.md` → "Memory governor") is the sum of the main and worker totals.
<!-- elaboration -->
- [Z2-N-SCRIPT-080] Hitting a VM memory cap is fatal and not replayable, like the watchdog: the error is `SCRIPT_ERROR_KIND_VM_MEMORY`, the world halts, and the event is reported through the configurable OOM handler, because a VM's heap occupancy depends on which invocations shared it and on GC progress, both of which vary with worker count, so treating it as a recoverable rollback would break the determinism guarantee. The reading of Z2-D-019 for the VM memory cap is confirmed by D-SCRIPT-TXN refinement (open, Stage 1).
- [Z2-N-SCRIPT-081] The memory caps are sized from the Scripting rows with headroom so that content within its per-entity snapshot and state budgets never reaches a VM cap; a workload run that reaches one fails its acceptance.

<!-- informative -->
Why the VM memory cap is excluded from the "exhaust budgets" clause of Z2-D-019. The plan lists memory caps among the Luau budgets, next to the safepoint budget, so the determinism guarantee could be read to cover them. It cannot, for a structural reason: a safepoint count and a per-entity snapshot charge depend on the entity's own processing alone, while a VM's heap occupancy at any moment depends on which invocations happened to share that VM (a function of worker count and scheduling) and on how far its incremental collector had progressed. A cap trip that rolled back one entity would therefore make the set of failed entities vary with worker count, chunk layout and scheduling, which is exactly what Z2-D-019 excludes. The cap is consequently a memory-governor event, not a script budget: the world halts and the event is not replayable. The shipping consequence is that a game whose scripts reach a VM cap stops with a fatal diagnostic; the mitigation is the sizing rule above (caps with headroom over the per-entity and state budgets, and workload acceptance failing on any cap trip). `Zenith2/Docs/Budgets.md` → "Scripting rows" describes the breach behaviour of the VM memory cap rows (Z2-B-SCRIPT-009, Z2-B-SCRIPT-010, Z2-B-SCRIPT-011); those rows are `BR-FATAL` (009, 010) and `BR-REFUSE` (011, refused at world creation), in agreement with this section.
<!-- /informative -->

### Garbage collection

- [Z2-N-SCRIPT-082] Luau's incremental collector stays enabled; each VM is configured at creation with `lua_gc(L, LUA_GCSETGOAL, uGoalPercent)`, `LUA_GCSETSTEPMUL` and `LUA_GCSETSTEPSIZE` from the script GC tuning row.
- [Z2-N-SCRIPT-083] Each world has a per-tick GC step budget (the script GC step row, in kilobytes of collector work per VM per tick): the main VM runs `Zenith_ScriptVM::StepGC` once after each behaviour node completes, and each worker VM runs it at the end of each chunk-system sub-job it executed, never inside an invocation.
- [Z2-N-SCRIPT-089] `StepGC`, invocation-thread creation, DAP breakpoint application and module loading each run as a `SCRIPT_CALL_KIND_NATIVE_BODY` protected call with `m_ulSafepointLimit = 0`, so every allocation they make happens inside a protected region; a `LUA_ERRMEM` raised by a GC step is classified `SCRIPT_ERROR_KIND_VM_MEMORY` like any other.
<!-- elaboration -->
- [Z2-N-SCRIPT-090] Each worker VM owns one pooled invocation thread, created once with `lua_newthread` and `luaL_sandboxthread` and reset with `lua_resetthread` after every chunk-system invocation; each behaviour instance owns one coroutine thread, created when its `Run` coroutine starts (`OnCreate` or `OnRecover`) and released at quarantine or destruction; no thread is allocated per entity or per invocation.
- [Z2-N-SCRIPT-084] Collection timing never affects simulation results: weak tables (`__mode`), `__gc` metamethods on script tables, `collectgarbage` and `gcinfo` are unavailable to scripts (see "Modules, bindings and generated types"), so no script can observe when collection happens.
- [Z2-N-SCRIPT-085] GC work is reported per VM to the on-screen profiler's Luau GC row (see "Debugging and profiling") and is never counted as safepoints.

### Floating-point environment

- [Z2-N-SCRIPT-086] Every thread that runs simulation or job-graph work has the fixed floating-point environment of `Zenith2/Docs/Contracts.md` → "Threading and concurrency contract" (its "Floating-point environment" table: round to nearest, flush-to-zero and denormals-are-zero set on x86-64, FZ and DN set on arm64, every exception masked or trapless), applied by `Zenith_ApplyFloatEnvironment()` at thread start and read back by `Zenith_FloatEnvironmentState Zenith_ReadFloatEnvironment()` (both declared by ZenithJobs in `Zenith_FloatEnvironment.h`; the state struct holds the raw MXCSR or FPCR bits); the per-toolchain call sequences below implement that state, and the job system asserts it at the start of every frame-graph node, so a chunk system computes identical results on any worker.

<!-- elaboration -->
| Toolchain (presets) | File | Call sequence | Read back by the test |
|---|---|---|---|
| MSVC x64 (`win`, `win-asan`) | `ZenithJobs/Internal/Windows/Zenith_FloatEnvironment.cpp` | `unsigned int uOld; _controlfp_s(&uOld, _RC_NEAR \| _DN_FLUSH \| _MCW_EM, _MCW_RC \| _MCW_DN \| _MCW_EM);` (MXCSR: RC = nearest, FTZ (bit 15) and DAZ (bit 6) set, all six mask bits set) | `_mm_getcsr() & 0xFFC0` equals `0x9FC0` |
| clang x86-64 (`linux-tsan`, `android-x86_64`) | `ZenithJobs/Internal/Linux/Zenith_FloatEnvironment.cpp`, `ZenithJobs/Internal/Android/Zenith_FloatEnvironment.cpp` (`#if defined(__x86_64__)` branch) | `_mm_setcsr((_mm_getcsr() & ~_MM_ROUND_MASK) \| _MM_MASK_MASK \| _MM_ROUND_NEAREST \| _MM_FLUSH_ZERO_ON \| _MM_DENORMALS_ZERO_ON);` | `_mm_getcsr() & 0xFFC0` equals `0x9FC0` |
| clang arm64 (`android-arm64`) | `ZenithJobs/Internal/Android/Zenith_FloatEnvironment.cpp` (`#if defined(__aarch64__)` branch) | `__builtin_arm_wsr64("fpcr", (__builtin_arm_rsr64("fpcr") & ~0x00C89F03ull) \| 0x03000000ull);` (clears FIZ (bit 0), AH (bit 1), the trap enables IOE, DZE, OFE, UFE, IXE (bits 8–12) and IDE (bit 15), FZ16 (bit 19) and RMode (bits 22–23, 00 = nearest); sets FZ (bit 24) and DN (bit 25)) | `__builtin_arm_rsr64("fpcr") & 0x03C89F03` equals `0x03000000` |
- [Z2-N-SCRIPT-091] `Test_Script_Determinism_WorkerFloatEnvironment` calls `Zenith_ReadFloatEnvironment()` on every worker and on the main thread, on every preset, and asserts the column "Read back by the test", which is the fixed state ZenithJobs' `Test_FloatEnvironment_WorkerThreads_MatchMain` asserts; it also asserts, as `Test_FloatEnvironment_Denormal_FlushesToZero` does, that a denormal product (`1e-30f * 1e-10f`) is zero on every worker.

### DAP fan-out

The debug adapter (see "Debugging and profiling") holds one breakpoint set per world and fans it out to every VM of the world.

1. A breakpoint change from the client increments the world's breakpoint generation and stores the new set.
2. The main VM applies the set at the next point where no invocation is running on it (between behaviour invocations, on the main thread), calling `lua_breakpoint` on every loaded function that contains a changed line.
3. Each worker VM compares its `m_uBreakpointGeneration` with the world's at the start of every sub-job and, when stale, applies the set on its own thread before the first invocation.
4. A VM created afterwards applies the current set as part of loading each module.

- [Z2-N-SCRIPT-087] Every VM of a world, main and worker, receives every breakpoint change before its next invocation starts, and a VM created after the change applies the current set when it loads each module.

### VM teardown

- [Z2-N-SCRIPT-088] World shutdown drains the tick (`Zenith2/Docs/Architecture.md` → "Lifecycle commands"), then destroys the worker VMs, then the main VM, on the main thread; no VM is destroyed while an invocation, a GC step or a DAP request is running on it.

## Modules, bindings and generated types

### Script sources and module kinds

A game's scripts live in `Zenith2/Games/<Name>/Scripts/**.luau`. Engine and module Luau libraries live in `Zenith2/Engine/ZenithScript/Scripts/**.luau` (`@zenith/Core`, the hand-written part of the `@zenith` prelude, whose typed surface is the generated `Zenith.luau`) and in each optional engine module's `Scripts/` directory.

| Kind | Recognised by | VM | State | Loaded when |
|---|---|---|---|---|
| chunk-system module | its return value is `Zenith.ChunkSystem({ … })` | worker VMs (and the main VM for validation) | none; statelessness enforced (D-LUAU-STATELESS) | world load (declaration validated), then lazily per worker VM |
| behaviour module | its return value is `Zenith.Behaviour({ … })` | main VM | per instance, declared `State` table only | world load |
| library module | any other return value (a table of functions and constants) | whichever VM requires it | none; statelessness enforced transitively when required by a system or behaviour module | on first `require` |

- [Z2-N-SCRIPT-101] A module's kind is decided by its return value at load time; a `Zenith.ChunkSystem` or `Zenith.Behaviour` declaration nested inside a library module's table, or created anywhere but as the module's return value, is a `SCRIPT_ERROR_KIND_LOAD` error.
- [Z2-N-SCRIPT-102] Every simulation module (chunk system, behaviour, and every library they require) starts with `--!strict`; ZenithCook refuses a non-strict simulation module.

### Module loading and sandboxing

Loading is specified step by step in `Scripting/Bindings.md` → "Module loader". Its contract:

- [Z2-N-SCRIPT-103] `require` accepts a constant string literal only, resolved at cook time against the game's module manifest (aliases `@game/…`, `@zenith/…`, `@types`, `@<module>/…`, and `./…` relative paths); a non-literal argument is a compile-time error reported by ZenithCook and a `SCRIPT_ERROR_KIND_LOAD` error when hot-reloaded source is loaded.
- [Z2-N-SCRIPT-104] `require` is callable at module scope only, while the module is being loaded; a call during an invocation raises the catalogued misuse error.
- [Z2-N-SCRIPT-105] Module dependency cycles are a load error naming the cycle (`a -> b -> a`).
- [Z2-N-SCRIPT-106] Each VM loads each module at most once per module generation; the cached module table is deep-frozen with `table.freeze` applied recursively to every table reachable from it at the end of its load.
- [Z2-N-SCRIPT-107] Each VM is sandboxed at creation: `luaL_openlibs`, then the reduction of the globals to the environment of its role, then `luaL_sandbox(L)`, which makes the built-in library tables and the global table read-only and enables safe-environment optimisations; every thread that runs an invocation (the pooled worker thread, each behaviour coroutine thread) is sandboxed with `luaL_sandboxthread` when it is created, so a global write lands in a per-thread proxy that the load-time check has already rejected.
- [Z2-N-SCRIPT-108] The environments are those of `Scripting/Bindings.md` → "Environments and library subsets"; a function absent from an environment is replaced by a stub that raises its catalogued misuse error, so a script that calls it gets a specific message rather than "attempt to call a nil value".

### Builtin fast path and mutable globals

At optimisation level 2 Luau's compiler emits `FASTCALL` (and, for library members, load-time import) sequences for known builtins, and in a VM whose global table carries the safe-environment flag set by `luaL_sandbox` the VM runs the builtin directly without reading the global. A replaced builtin would then be bypassed by optimised bytecode. `CompileOptions::mutableGlobals` tells the compiler that a global may be reassigned, so it emits an ordinary global read and call for that name.

| Global | Environment replacement | Luau fast path without the option | How the O2 fast path is disabled |
|---|---|---|---|
| `tostring` | Zenith `tostring` (no addresses) | `FASTCALL` builtin | `tostring` in `mutableGlobals` |
| `setmetatable` | Zenith `setmetatable` (refuses `__mode`, `__gc`, `__metatable`) | `FASTCALL` builtin | `setmetatable` in `mutableGlobals` |
| `getmetatable` | Zenith `getmetatable` (hides engine metatables) | `FASTCALL` builtin | `getmetatable` in `mutableGlobals` |
| `rawset` | stub raising ZS1602 | `FASTCALL` builtin | `rawset` in `mutableGlobals` |
| `pcall`, `xpcall` | Zenith versions re-raising abort-class errors | load-time import | `pcall`, `xpcall` in `mutableGlobals` |
| `print`, `require` | Zenith log sink; Zenith loader | load-time import | `print`, `require` in `mutableGlobals` |
| `getfenv`, `setfenv`, `collectgarbage`, `gcinfo`, `newproxy` | stubs | load-time import | each name in `mutableGlobals` |
| `math.random`, `math.randomseed` | stubs raising ZS1606 | library-member import; not a `FASTCALL` builtin in current Luau | `math` stays out of `mutableGlobals` so the other `math.*` builtins keep their fast path; this is the one exception to the rule below, safe because neither name is a `FASTCALL` builtin, and if the pinned revision's builtin table lists either name `math` joins `mutableGlobals` |
| `os.*` | table of stubs raising ZS1607 | library-member import | `os` in `mutableGlobals` |
| `coroutine.*` | stubs (chunk systems) or partial set (behaviours) | library-member import | `coroutine` in `mutableGlobals` |

- [Z2-N-SCRIPT-126] Every replaced or stubbed global root is listed in `mutableGlobals`, whether or not the pinned compiler treats it as a builtin, because an ordinary global read reaches the environment's value; the `math` row is the single exception, argued in its cell.

Unreplaced builtins (`type`, `typeof`, `rawget`, `rawequal`, `rawlen`, `select`, `math.*` apart from the two above, `string.*`, `bit32.*`, `vector.*`, `buffer.*`, `table.*`) keep their fast path.

- [Z2-N-SCRIPT-121] Every compile of a script (cook and hot reload) passes the names in the column "How the O2 fast path is disabled" as Luau's `CompileOptions::mutableGlobals` through `Zenith_ScriptCompileOptions::m_aszMutableGlobals`, so no `FASTCALL` or load-time import is emitted for a replaced or removed global and the environment's replacement is what runs.
- [Z2-N-SCRIPT-122] The mutable-globals list is one of the inputs of the binding-set hash (defined once in the table "Cooked bytecode format"), so changing it re-cooks every module.
- [Z2-N-SCRIPT-123] The Stage 1 firewall ticket verifies against the pinned revision that bytecode compiled with the production options contains no `FASTCALL` naming a replaced builtin: `Test_Script_Sandbox_NoFastcallForReplacedBuiltins`, in the ZenithScriptCompiler test target, lists the fast-call sites of the compiled trigger scripts of `Test_Script_Sandbox_EnvironmentSubset` with `Zenith_ScriptListFastcallSites` and asserts that no listed builtin name is a replaced one; the ticket adds `math` to the list when the pinned builtin table names `math.random` or `math.randomseed`.

### Why luaL_sandbox and frozen tables are insufficient

<!-- informative -->
`luaL_sandbox` freezes the global table and the built-in libraries; `luaL_sandboxthread` gives each thread a writable proxy so a global write does not leak between threads; `table.freeze` makes one table immutable. None of that makes a module stateless:

- A module-scope `local counter = 0` captured by a system function is an upvalue. Upvalues are not tables, so no freeze reaches them: `counter += 1` inside the system mutates state that survives across invocations and differs per worker VM.
- `table.freeze` is shallow. A frozen module table `M` whose field `M.Cache` is an unfrozen table leaves `M.Cache[k] = v` legal. Recursive freezing at load fixes tables that exist at the end of load, but not a table created after load and stored in an upvalue.
- A required library module can hold its own upvalue state and expose a function that mutates it; the requiring system never writes an upvalue itself, but calling the function has the same effect.
- Mutable userdata (a `buffer` created at module scope) and closures created by module-scope factories carry state that no table freeze sees.
- Stateful library functions (`math.random` with its hidden generator, `os.clock`) produce values that depend on history or wall-clock time.

So statelessness needs a mechanism of its own, which is decision D-LUAU-STATELESS (open, Stage 1), specified in "Stateless system enforcement". Sandboxing and deep freezing remain as defence in depth.
<!-- /informative -->

### Bindings in brief

- [Z2-N-SCRIPT-109] Engine functionality reaches Luau through bindings registered with ZenithReflect by the composition root for the engine and for every static module the game selects in `game.json` (Z2-D-024); a module that is not selected registers nothing and its bindings are absent from the game's environment and `.d.luau`.
- [Z2-N-SCRIPT-124] The Luau-free binding ABI (`Zenith_ScriptBinding.h`: values, call frames, thunks, descriptors, the binding table, the invocation-context interface) is a public header of ZenithReflect, so a library that owns bindings depends on ZenithReflect and never on ZenithScript, and the library DAG holds (`Zenith2/Docs/Architecture.md` → "Library dependency graph").
- [Z2-N-SCRIPT-125] A library at or above ZenithReflect registers its own bindings from an `Internal/` translation unit; the bindings of libraries below ZenithReflect (ZenithCore, ZenithGraph, ZenithJobs, ZenithProfiling, ZenithPlatform, ZenithVertexFormat, ZenithCompression) are registered by the composition root in `Zenith2/Engine/ZenithRuntime/Internal/ScriptBindings/`.
- [Z2-N-SCRIPT-110] Binding thunks are generated by the C++ registration template from the reflected function's signature; there is no hand-written marshalling code per function and no source-to-source binding generator.
- [Z2-N-SCRIPT-111] Components, including data-defined components, reach Luau through reflection-driven views (field ID, offset, kind), with no per-component generated code (`Scripting/Bindings.md` → "Component views through reflection").
- [Z2-N-SCRIPT-112] Every binding declares its allowed contexts (`SCRIPT_FUNCTION_CONTEXT_SYSTEM`, `SCRIPT_FUNCTION_CONTEXT_BEHAVIOUR`, `SCRIPT_FUNCTION_CONTEXT_LOAD`, `SCRIPT_FUNCTION_CONTEXT_TOOLS`) and its effect class (`SCRIPT_FUNCTION_EFFECT_PURE`, `SCRIPT_FUNCTION_EFFECT_READ_WORLD`, `SCRIPT_FUNCTION_EFFECT_BUFFERED`); a call from a disallowed context raises the catalogued misuse error.
- [Z2-N-SCRIPT-113] A binding with a side effect is `SCRIPT_FUNCTION_EFFECT_BUFFERED` and writes the effect into the invocation's side-effect sub-buffer through `Zenith_ScriptCallFrame::m_pxContext` (`Zenith_ScriptInvocationContext::AppendSideEffect`); no binding performs a world-visible side effect directly, and review confirms that every new side-effect kind is routed through the private buffer.

### Generated types in brief

- [Z2-N-SCRIPT-114] ZenithCook's `luau-types` step writes the game's Luau definition files from the live reflection registry (engine types, the selected modules' types, the game's data-defined schemas and every registered binding) to `Zenith2/out/<checkout-hash>/Generated/Luau/<Game>/`: `zenith.d.luau`, `components.d.luau`, the prelude module `Zenith.luau`, the descriptor module `Types.luau` and a `.luaurc` whose aliases match the runtime module resolver (`Scripting/Bindings.md` → "Luau type generation"); none of them is hand-edited or tracked. ZenithCook links every optional module and, before emitting, registers exactly the set the game's `game.json` selects (`Zenith2/Docs/Architecture.md` → "Engine modules and game projects").
- [Z2-N-SCRIPT-115] ZenithCook type-checks every script of the game with `luau-analyze --definitions=<each .d.luau>` out of process, and a type error in a simulation module fails the cook.
- [Z2-N-SCRIPT-116] Luau names are derived from Zenith Hungarian C++ names by the mapping in `Scripting/Bindings.md` → "Name mapping from Zenith Hungarian"; a derived name that collides or is not a valid Luau identifier is a generation error naming both C++ sources.
- [Z2-N-SCRIPT-117] The runtime module `@types` exposes, per VM, a deep-frozen table of reflected type and field descriptors built from the same registry, so every name in the `.d.luau` has a runtime descriptor with the same spelling.

### System environment library subset

The environments differ from stock Luau as follows: in both simulation environments `math.random`, `math.randomseed` and the whole `os` library are stubs; `getfenv`, `setfenv`, `collectgarbage`, `gcinfo`, `newproxy` and `rawset` are stubs; `loadstring`, `load`, `dofile`, `loadfile`, `io` and `package` do not exist; `pcall`, `xpcall`, `tostring`, `print`, `setmetatable`, `getmetatable` and `require` are Zenith replacements; `debug` keeps `traceback` and `info`; chunk systems get every `coroutine` function as a stub, behaviours get `coroutine.yield` inside `Run`. The per-function tables are `Scripting/Bindings.md` → "Environments and library subsets".

- [Z2-N-SCRIPT-118] `math.random`, `math.randomseed` and the whole `os` library are unavailable in both simulation environments (each name is a stub raising its catalogued error); script randomness comes from the counter-based `ctx.Random` of "Determinism rules and randomness", and script time from the tick-based context fields.
- [Z2-N-SCRIPT-119] The Zenith `pcall` and `xpcall` re-raise any abort-class or fatal error they catch, unchanged, so a script can catch its own `error()` values and catalogued misuse errors but can never swallow budget exhaustion, a snapshot-cap overflow, a yield-in-system error or a checkpoint failure.
- [Z2-N-SCRIPT-120] The Zenith `tostring` returns the type name without an address for tables, functions, threads and userdata that have no `__tostring`, so no address-dependent string reaches simulation state.

## Behaviours and chunk systems

Two script kinds exist, and they differ in every axis that matters for transactions:

| Axis | Behaviour | Chunk system |
|---|---|---|
| instance | one per entity carrying a `Behaviour` component that names the module | one per module per world |
| VM | the world main VM, main thread | per-worker per-world VMs |
| phase | `"Early"` (phase 2, Early scripts) or `"Late"` (phase 10, Late scripts); event handlers run in Late scripts | `"Early"` or `"Late"` |
| state | a declared, reflected, serializable `State` table per instance | none (stateless, enforced) |
| may yield | yes, in its `Run` coroutine | no; `coroutine.yield` raises an error |
| direct writes | its own entity's fields in its declared write set | the current entity's row of its declared writable fields |
| transaction | each callback or resume segment is one entity transaction | each entity processed in a call is one entity transaction |
| container and string fields | read and write through copy-on-write shadows | strings, entity and asset handles are read-only; containers are not exposed |

### Behaviour declaration

A behaviour module returns `Zenith.Behaviour(xDeclaration)`. The declaration table's keys:

| Key | Type | Required | Meaning |
|---|---|---|---|
| `Name` | string | yes | stable behaviour ID, `Area.Name` PascalCase segments; hashed into the behaviour's stable source ID (see "Counter-based random numbers") |
| `Phase` | `"Early"` \| `"Late"` | yes | the phase in which `OnUpdate` and `Run` segments execute |
| `Reads` | `{ ComponentType \| FieldDescriptor }` | no | components or fields read on the own entity or other entities |
| `Writes` | `{ ComponentType \| FieldDescriptor }` | no | own-entity components or fields written directly; a whole component expands to all its fields |
| `Subscribes` | `{ EventType }` | no | event types delivered to `OnEvent` |
| `Uses` | `{ WorldResource }` | no | world resources read through `READ_WORLD` bindings (`Zenith.Resources.PhysicsWorld`, `Zenith.Resources.NavMesh`, …), declared so the schedule compiler sees the access |
| `State` | `{ [string]: FieldSpec }` | yes (may be empty) | the declared state table schema (see "Behaviour state model and recovery") |
| `StateVersion` | integer ≥ 1 | yes | schema version of `State` |
| `Migrate` | `{ [integer]: (old) -> new }` | when `StateVersion > 1` | one step per version, `[n]` migrates n to n+1 |
| `OnCreate` | `(self, state, ctx) -> ()` | no | first invocation after the spawn is visible |
| `OnUpdate` | `(self, state, ctx) -> ()` | no | once per tick in `Phase` |
| `Run` | `(self, state, ctx) -> ()` | no | the coroutine main, resumed once per tick in `Phase` while its wait condition holds |
| `OnEvent` | `{ [EventType]: (self, state, event, ctx) -> () }` | when `Subscribes` is non-empty | handlers, run in Late scripts in event order |
| `OnRecover` | `(self, state, ctx) -> ((self, state, ctx) -> ())?` | when `Run` is present | restart entry after restore or hot reload; returns the coroutine main to start, or `nil` for `Run` |
| `Budget` | `{ SafepointsPerInvocation: integer? }` | no | lowers (never raises) the per-invocation limit of the behaviour safepoint row |

- [Z2-N-SCRIPT-141] A behaviour declaration is validated when the module loads: an unknown key, a missing required key, a `Phase` outside `"Early"`/`"Late"`, a write to a component the behaviour's own entity cannot carry, a `Run` without `OnRecover`, or a `Budget` above the budget row is a `SCRIPT_ERROR_KIND_LOAD` error naming the key.
<!-- elaboration -->
- [Z2-N-SCRIPT-142] Behaviour event handlers run in the Late scripts phase whatever the declared `Phase`, because events are delivered at phase 9 and phase 10 owns reactions to events (Z2-D-080; D-PHASES refinement, open, Stage 1, confirms this placement); within one instance, handlers run in event delivery order, then the `OnUpdate` call or `Run` segment of a Late behaviour.
- [Z2-N-SCRIPT-143] A behaviour writes directly only fields of its own entity that are in its declared write set and whose owning phase is the phase it is running in; every other write (another entity, or a field owned by another phase) is a command applied at the next sync point.
- [Z2-N-SCRIPT-156] Consequently an Early behaviour's event handler, which runs in phase 10, cannot write its own `Writes` fields owned by phase 2 directly: each such write in a handler is issued as `ctx:Command(self.Entity):Set(field, value)` and applied at the next sync point, and a direct write raises ZS1207; an author who wants handler writes applied directly declares the behaviour `"Late"`.
- [Z2-N-SCRIPT-144] `self` is a per-instance handle object (`Zenith.Self`), not a table the script may extend; assigning a field on it raises the catalogued misuse error, so per-instance data lives in `state` alone.

### Behaviour example

```luau
--!strict
-- Zenith2/Games/OpenWorldSlice/Scripts/Village/Door.luau
local Zenith = require("@zenith")
local T = require("@types")

local OPEN_TICKS = 30          -- module-scope constants are allowed; they are frozen at load
local MAX_ANGLE = 95

type DoorState = {
	Mode: "Closed" | "Opening" | "Open" | "Closing",
	Progress: number,
	Opener: Zenith.Entity?,
	OpenCount: number,
}

local function stepToward(state: DoorState, fTarget: number): boolean
	local fStep = 1 / OPEN_TICKS
	if state.Progress < fTarget then
		state.Progress = math.min(state.Progress + fStep, fTarget)
	else
		state.Progress = math.max(state.Progress - fStep, fTarget)
	end
	return state.Progress == fTarget
end

local function runDoor(self: Zenith.Self, state: DoorState, ctx: Zenith.BehaviourContext)
	while true do
		if state.Mode == "Opening" then
			if stepToward(state, 1) then
				state.Mode = "Open"
				ctx:Emit(T.Events.DoorOpened, { Door = self.Entity, Opener = state.Opener })
			end
		elseif state.Mode == "Closing" then
			if stepToward(state, 0) then
				state.Mode = "Closed"
			end
		elseif state.Mode == "Open" then
			Zenith.WaitSeconds(ctx, 3.0)                 -- a yield: this segment commits here
			state.Mode = "Closing"
		end
		local leaf = self:Get(T.DoorLeaf)               -- non-optional: DoorLeaf is in Writes; a fresh view each segment
		leaf.Angle = state.Progress * MAX_ANGLE
		Zenith.WaitTicks(ctx, 1)
	end
end

return Zenith.Behaviour({
	Name = "Village.Door",
	Phase = "Late",
	Reads = { T.Transform },
	Writes = { T.DoorLeaf.Angle },
	Subscribes = { T.Events.Interact },
	StateVersion = 2,
	State = {
		Mode = Zenith.Field.Enum({ "Closed", "Opening", "Open", "Closing" }, "Closed"),
		Progress = Zenith.Field.F32(0),
		Opener = Zenith.Field.Entity(),
		OpenCount = Zenith.Field.U32(0),
	},
	Migrate = {
		[1] = function(old: { [string]: any }): { [string]: any }   -- v1 had no OpenCount
			old.OpenCount = 0
			return old
		end,
	},
	OnEvent = {
		[T.Events.Interact] = function(self: Zenith.Self, state: DoorState, event: T.InteractEvent, ctx: Zenith.BehaviourContext)
			if state.Mode == "Closed" then
				state.Mode = "Opening"
				state.Opener = event.Instigator
				state.OpenCount += 1
			end
		end,
	},
	Run = runDoor,
	OnRecover = function(self: Zenith.Self, state: DoorState, ctx: Zenith.BehaviourContext)
		-- the declared Mode says where the door logic was; restart the loop there
		return runDoor
	end,
})
```

### Chunk system declaration

A chunk-system module returns `Zenith.ChunkSystem(xDeclaration)`:

| Key | Type | Required | Meaning |
|---|---|---|---|
| `Name` | string | yes | stable system ID; hashed into the stable source ID; unique per game |
| `Phase` | `"Early"` \| `"Late"` | yes | the script phase the system runs in |
| `Reads` | `{ ComponentType \| FieldDescriptor }` | no | fields read (own row and, subject to the cross-entity read rule, other rows) |
| `Writes` | `{ ComponentType \| FieldDescriptor }` | no | fields written in place on the current entity's row; each field's owning phase equals `Phase` |
| `With` / `Without` | `{ ComponentType }` | no | extra query filters (presence and absence) |
| `Before` / `After` | `{ string }` | no | explicit ordering against other systems of the same phase, by `Name` |
| `Uses` | `{ WorldResource }` | no | world resources read through `READ_WORLD` bindings, declared so the schedule compiler sees the access |
| `Reductions` | `{ [string]: ReductionSpec }` | no | named reductions, each `{ Kind = "SumF64" \| "SumI64" \| "Min" \| "Max" \| "Count", Target = FieldDescriptor }` on a singleton component |
| `Each` | `(e: Zenith.Row, ctx: Zenith.SystemContext) -> ()` | yes | called once per matching entity |
| `Budget` | `{ SafepointsPerEntity: integer? }` | no | lowers (never raises) the per-entity limit of the chunk-system safepoint row |

- [Z2-N-SCRIPT-145] The query of a chunk system is the entities that carry every component named in `Reads`, `Writes` and `With` and none named in `Without`; it is compiled once per module generation.
- [Z2-N-SCRIPT-146] A chunk system's declared read and write sets are expanded to fields and checked per field when the frame schedule compiles (`Zenith2/Docs/Architecture.md` → "Frame phases and authority"): a written field whose owning phase is not the system's `Phase` is a schedule-compile error (`SCRIPT_SCHEDULE_WRITE_OUTSIDE_AUTHORITY`), and two direct writers of one field in one phase are a schedule-compile error unless the component declares an arbitration rule.
<!-- elaboration -->
- [Z2-N-SCRIPT-147] Chunk systems declare `"Early"` or `"Late"` in v1; admitting chunk systems to further phases is part of D-PHASES refinement (open, Stage 1).
- [Z2-N-SCRIPT-148] `Each` receives one row; data flows between rows of one call only through reductions, events and commands, and D-LUAU-STATELESS's mechanism rejects any module-scope or upvalue state through which one row's processing could reach another's.

### Chunk system execution

1. The system's frame-graph node fans out one sub-job per chunk batch (the batch size is a scheduling choice with no effect on results).
2. A sub-job acquires its worker VM (see "Lazy worker VM creation"), creates a `Zenith_ScriptTransaction` for each chunk it processes (one chunk, or one range of at most `uSCRIPT_MAX_ROWS_PER_INVOCATION` rows of a larger chunk, is one invocation; the `Zenith_ChunkView` comes from `Zenith_SystemContext::GetChunk`), and runs the C++ row trampoline `Zenith_ScriptChunkRunner::RunChunk` under `Zenith_ScriptProtectedCall` with `SCRIPT_CALL_KIND_NATIVE_BODY`.
3. The trampoline loops over the chunk's rows from `uFirstRow`: it opens a `Zenith_EntityTransaction` for the row, resets the VM safepoint counter and sets the limit to the per-entity budget, points the row proxy at the row, and calls `Each` with `lua_call` (inside the protected region).
4. When `Each` returns, the trampoline marks the entity transaction `ENTITY_TXN_STATE_SUCCEEDED` and advances to the next row.
5. When `Each` raises, the protected call returns the error; the runner restores the failing row, discards its sub-buffer and shadows, marks it `ENTITY_TXN_STATE_FAILED` (see "Execution-time failure algorithm"), then re-enters the trampoline at `uFirstRow = uFailedRow + 1`.
6. After the last row, the transaction is handed to the node's merge point (see "Merge point algorithm") with its snapshots still alive.

- [Z2-N-SCRIPT-149] Each entity processed by a chunk-system call is its own entity transaction, and a failure in one row never prevents the subsequent rows of the same call from being processed (Z2-D-051).
- [Z2-N-SCRIPT-150] The row trampoline is C++ inside `ZenithScript/Internal`, so no trampoline safepoints are counted and each entity's safepoint count depends only on that entity's own processing.

### Chunk system example

```luau
--!strict
-- Zenith2/Games/OpenWorldSlice/Scripts/Combat/RegenerateStamina.luau
local Zenith = require("@zenith")
local T = require("@types")

local WINDED_SCALE = 0.5

return Zenith.ChunkSystem({
	Name = "Combat.RegenerateStamina",
	Phase = "Early",
	Reads = { T.Stats.MaxStamina, T.Stats.RegenPerSecond, T.Status.Winded },
	Writes = { T.Stats.Stamina },
	Without = { T.Dead },
	After = { "Combat.ApplyExhaustion" },
	Reductions = {
		TotalStamina = { Kind = "SumF64", Target = T.CombatTotals.TotalStamina },
		FullCount = { Kind = "Count", Target = T.CombatTotals.EntitiesAtFullStamina },
	},
	Each = function(e: Zenith.Row, ctx: Zenith.SystemContext)
		local fStamina = e.Stats.Stamina               -- own row: sees this call's own writes
		local fMax = e.Stats.MaxStamina
		if fStamina < fMax then
			local fRegen = e.Stats.RegenPerSecond * ctx.DeltaTime
			if e.Status.Winded then
				fRegen *= WINDED_SCALE
			end
			fRegen += ctx.Random:NextFloat() * 0.01    -- counter-based; identical on any worker, on any re-run
			fStamina = math.min(fStamina + fRegen, fMax)
			e.Stats.Stamina = fStamina                  -- in place; first write snapshots this chunk's Stamina column
			if fStamina >= fMax then
				ctx:Emit(T.Events.StaminaFull, { Entity = e.Entity })  -- buffered in this entity's sub-buffer
			end
		end
		ctx:Contribute("TotalStamina", fStamina)       -- folded at the merge point, never summed in place
		if fStamina >= fMax then
			ctx:Contribute("FullCount", 1)
		end
	end,
})
```

- [Z2-N-SCRIPT-157] The two example modules of this section are committed verbatim as `Zenith2/Engine/ZenithScript/Tests/Corpus/Representative/Village_Door.luau` and `Combat_RegenerateStamina.luau` (with the fixture schemas and events they name), so the cook's strict-mode type check and `Test_Script_Bytecode_DevAndCookIdentical` compile and type-check them on every `Test` step and the document's examples cannot drift out of validity.

### Schedule compilation of script nodes

- [Z2-N-SCRIPT-151] Each chunk-system module is one frame-graph node in its phase; all behaviour modules of one phase form one main-thread node per module in behaviour-module order, and every script node declares its field-level read and write sets so hazard analysis derives its edges (`Zenith2/Docs/Architecture.md` → "CPU frame graph and scheduler").
- [Z2-N-SCRIPT-158] A behaviour module declaring `"Early"` with a non-empty `Subscribes`, or whose instances can need a checkpoint-only transaction, contributes a second main-thread node in the Late scripts phase; the Late-phase nodes of all behaviour modules (the Late modules' own nodes and the Early modules' handler nodes) run in one sequence, in behaviour-module order, so an Early module's handler node sits at its module's position in that sequence (D-PHASES refinement, open, Stage 1, confirms this placement).
- [Z2-N-SCRIPT-152] System order, the first key of every merge order in this document, is the position of the script node in the compiled schedule's deterministic topological order for its phase, with ties broken by the stable source ID; it depends on the module set and declarations, never on timing.
- [Z2-N-SCRIPT-161] Behaviour-module order, which sequences every main-thread behaviour node of a phase, is ascending module ID, that is, the sorted manifest path order of the modules (behaviour declarations carry no `Before`/`After`); it is the system order of those nodes.
- [Z2-N-SCRIPT-159] The hazard edges from a script node to every node that depends on it originate at the script node's merge continuation, not at the join of its sub-jobs: a node (script or engine) that reads or writes a field column a script node writes starts only after that node's merge point has accepted or rejected every entity and committed, so no dependent ever observes an in-place write that a merge-time rejection subsequently restores; nodes whose access is disjoint from the script node still overlap with it.
- [Z2-N-SCRIPT-160] `Test_Script_Merge_DependentsRunAfterMerge`: system S2 reads its own row of field X written by system S1 in the same phase; S1's entity E is rejected at S1's merge for capacity; S2's invocation for E observes the restored (pre-S1) value of X, on 1 and 8 workers.
- [Z2-N-SCRIPT-153] Adding, removing or hot-reloading a script module with a changed declaration recompiles the schedule in the frame prologue (the loop steps before `RunFrame` that precede the frame's first tick boundary, where the hot-reload window and world swaps happen); a reload whose declaration is unchanged reuses the compiled schedule.

### Commands and cross-entity writes

- [Z2-N-SCRIPT-154] Every structural change a script requests (spawn, destroy, add or remove component, reparent) and every write to another entity or to a field outside the current phase's authority is a command recorded in the entity transaction's side-effect sub-buffer and played back at the next sync point; nothing a script does changes structure immediately.
- [Z2-N-SCRIPT-155] Commands that write the same field of the same entity are applied at playback in merge order, so the last in merge order wins, independent of worker count.

## Transaction scope and the entity boundary

The transaction boundary is the entity and not the chunk: D-SCRIPT-TXN as pre-decided (Z2-D-051); its Stage 1 refinement ticket, D-SCRIPT-TXN refinement (open, Stage 1), refines and confirms it. Outcomes are therefore independent of chunk layout and worker count.

### Invocations and entity transactions

- [Z2-N-SCRIPT-171] Every script invocation is a transaction: a chunk-system call over one chunk, a behaviour callback, or a behaviour coroutine resume segment (Z2-D-016).
- [Z2-N-SCRIPT-172] A chunk-system call iterates entities, and each entity's processing is its own logical transaction; a behaviour callback or segment is one entity's transaction.
- [Z2-N-SCRIPT-173] A failure rolls back only the failing entity's writes and side effects; the other entities of the same call and of the same tick commit normally.
- [Z2-N-SCRIPT-174] A behaviour coroutine commits at each yield: a yield ends the segment's entity transaction, and the next resume starts a new one.

### What is transactional

| State | Transactional mechanism | Visible to other entities |
|---|---|---|
| component field columns (declared-writable) | first-write snapshot per chunk, per-row restore | after the merge point |
| structural commands (spawn, destroy, add/remove component, reparent) | private side-effect sub-buffer | at the next sync point |
| field commands (cross-entity, cross-phase writes) | private side-effect sub-buffer | at the next sync point |
| events | private side-effect sub-buffer | at phase 9 delivery |
| spawns (entity IDs handed to the script) | provisional handle in the sub-buffer; released on rollback | at the next sync point |
| audio and VFX triggers | private side-effect sub-buffer | at commit |
| save requests | private side-effect sub-buffer | at the save-snapshot point (phase 11) |
| asynchronous requests | private side-effect sub-buffer; completion ID assigned at commit | results at a tick boundary |
| reduction contributions | private side-effect sub-buffer | folded at the merge point |
| component container and string fields | per-invocation copy-on-write shadow | after the merge point |
| behaviour state checkpoint | reserved double-buffered slot, swapped at commit | recovery and saves |

- [Z2-N-SCRIPT-175] Component columns, structural commands, events, spawns, triggers, save requests, asynchronous requests, reduction contributions, and component container and string fields are transactional, through the mechanisms in the table "What is transactional".
- [Z2-N-SCRIPT-176] Script randomness is counter-based, derived per invocation (see "Counter-based random numbers"); there is no shared mutable random stream, so rollback has nothing to restore and parallel workers cannot race on one.

### What is not transactional

- [Z2-N-SCRIPT-177] The Luau heap (script tables, globals, upvalues, coroutine stacks) is not transactional; a failing behaviour instance is quarantined, its heap state discarded, and it is restored from its last successfully committed serialized state (see "Quarantine and hot reload").
- [Z2-N-SCRIPT-178] Log output (`print`, diagnostics) is not transactional and is emitted immediately; it never feeds simulation state.

### Cross-entity read rule

The rule that makes the entity boundary layout-independent:

- [Z2-N-SCRIPT-179] A chunk system may read a field column it writes in this phase for its own row only.
- [Z2-N-SCRIPT-180] Other entities' rows are readable only in field columns that no node of the current phase declares as written; per-field authority makes this checkable when the schedule compiles, and a declared cross-entity read of such a column is a schedule-compile error (`SCRIPT_SCHEDULE_CROSS_ENTITY_READ_OF_WRITTEN_COLUMN`).
- [Z2-N-SCRIPT-181] At runtime, a read through a view or `ctx:Read(entity, field)` of another row in a column written in this phase raises the catalogued misuse error, in all builds.
- [Z2-N-SCRIPT-182] The same two rules bind behaviours: a behaviour reads another entity's field only when no node of the current phase writes it.

So no entity ever observes another entity's in-flight write, whatever the chunk order or worker.

### Reductions and shared structures

- [Z2-N-SCRIPT-183] Sums, counters, minima, maxima, spatial grids and every other structure shared across entities are never written in place across entities; each entity emits a contribution into its private sub-buffer (`ctx:Contribute(name, value)`), and contributions are folded at the merge point, in merge order, with the fixed-order reduction tree (see "Fixed-order reduction tree").
- [Z2-N-SCRIPT-184] A reduction's `Target` field is written at commit by the merge step and by nothing else in that phase; the schedule compiler treats the reducing system as the target field's direct writer.

### Snapshot lifetime

- [Z2-N-SCRIPT-185] Chunk snapshots live until the system's merge point, not merely until the call returns, because merge-time capacity rejection can still roll back an entity's rows afterwards.
- [Z2-N-SCRIPT-186] Change-tracking versions advance at merge, never at write.
- [Z2-N-SCRIPT-187] The per-tick total of snapshot memory is a budget row (the per-tick snapshot total row), measured and reported every tick; exceeding it raises a performance diagnostic and never a rollback, because the per-chunk snapshot total depends on chunk layout. Z2-B-SCRIPT-013 is `BR-REPORT` in `Zenith2/Docs/Budgets.md` → "Scripting rows", in agreement with this bullet.
- [Z2-N-SCRIPT-188] "Commit cannot fail" holds because acceptance and rejection are both decided at the merge point, before commit.

### Disable from the next tick boundary

- [Z2-N-SCRIPT-189] A failing chunk system or behaviour module is disabled for its world from the next tick boundary, never mid-tick, so every other invocation of the current tick runs regardless of which worker failed first.
- [Z2-N-SCRIPT-190] The failure is reported with the module, the entity's stable simulation ID, the tick, the error kind and code, the message and the traceback, and the module stays disabled for that world until it is reloaded (see "Quarantine and hot reload").

### Worked example three entities second fails

`Combat.RegenerateStamina` (above) runs over one chunk with three rows. The chunk's declared-writable column is `Stats.Stamina` (stride 4 bytes). The event queue for `StaminaFull` has one free slot left this tick. Entity stable simulation IDs: row 0 → A (sim ID 1007), row 1 → B (sim ID 1003), row 2 → C (sim ID 1011). All three reach full stamina this tick; B's `Each` raises an error after emitting its event.

| Step | Where | Action | Stamina column (rows 0/1/2) | Snapshot | Sub-buffers | Entity states |
|---|---|---|---|---|---|---|
| 0 | worker 3 | call starts; transaction T created | 90 / 40 / 70 | none | — | — |
| 1 | worker 3 | A: `e.Stats.Stamina = 100`, first write to this chunk; pending change bit of row 0 set | 100 / 40 / 70 | taken: `Stamina` = 90 / 40 / 70 | — | A OPEN |
| 2 | worker 3 | A emits `StaminaFull`, contributes 100 and 1; returns | 100 / 40 / 70 | kept | A: {event, contrib 100, contrib 1} | A SUCCEEDED |
| 3 | worker 3 | B writes 100, emits `StaminaFull`, then raises | 100 / 100 / 70 | kept | B: {event, …} | B OPEN |
| 4 | worker 3 | protected call returns the error; runner restores row 1 from the snapshot, clears row 1's pending change bit, discards B's sub-buffer and shadows | 100 / 40 / 70 | kept | A only | B FAILED; module flagged "disable at next tick boundary" |
| 5 | worker 3 | trampoline re-entered at row 2; C writes 100, emits, contributes | 100 / 40 / 100 | kept | A; C | C SUCCEEDED |
| 6 | worker 3 | call returns; T handed to the merge point with its snapshot alive | 100 / 40 / 100 | kept | A; C | — |
| 7 | merge point | succeeded entities sorted by logical ID: A (1007) then C (1011) | — | kept | — | — |
| 8 | merge point | A's demand (1 event slot) fits: accepted, remaining slots 0 | — | kept | — | A ACCEPTED |
| 9 | merge point | C's demand (1 event slot) exceeds the remaining 0: rejected; row 2 restored from the still-alive snapshot, C's sub-buffer discarded, diagnostic `SCRIPT_MERGE_REJECTED_CAPACITY` (class event queue) | 100 / 40 / 70 | kept | A | C REJECTED |
| 10 | merge point | reductions folded over accepted contributions in merge order: `TotalStamina` = 100, `FullCount` = 1 | — | kept | — | — |
| 11 | commit | A's event spliced into the reserved queue slot; `Stamina` column version advanced (A wrote it and was accepted); reduction targets written | 100 / 40 / 70 | dropped | — | A COMMITTED |
| 12 | next tick boundary | `Combat.RegenerateStamina` disabled for this world (B failed); C's rejection alone would not have disabled it | — | — | — | — |

The outcome is the same whatever chunk the three entities sit in, whatever worker ran them, and whether rows were processed in one call or three: merge order is by stable simulation ID, restores are per row, and the capacity decision is taken at the merge point in merge order.

- [Z2-N-SCRIPT-191] `Test_Script_Txn_EntityBoundaryMiddleFailure` reproduces this worked example with the three rows placed in one chunk, then in three chunks, then on three workers, and asserts the final column values 100 / 40 / 70, one delivered `StaminaFull` event (from A), `TotalStamina == 100`, `FullCount == 1`, and the module disabled from the next tick in every placement.

## Component writes shadows and side-effect buffers

### Data structures

The transaction structures, declared in `Internal/Zenith_ScriptTransaction.h`; their complete declarations, field by field, are `Scripting/TransactionStructures.md` → "Transaction data structures".

| Structure | One per | Holds |
|---|---|---|
| `Zenith_ScriptTransaction` | invocation (one chunk-system call over one chunk, or one behaviour callback or segment) | the invocation ID, the system, the arena mark, and spans of snapshots, entity transactions, side-effect records, shadows and payload bytes |
| `Zenith_EntityTransaction` | entity processed by the invocation | row, logical ID, state (`ENTITY_TXN_STATE_OPEN` … `ENTITY_TXN_STATE_COMMITTED`), its side-effect sub-buffer, its shadow range, its memory and safepoint accounting, its error |
| `Zenith_ColumnSnapshot` | declared-writable field column of a written chunk range (or of the behaviour's own row) | the chunk ordinal, the type and property IDs, the recorded `Zenith_RawColumn`, the captured row range, the gathered field bytes |
| `Zenith_SideEffectBuffer` | entity transaction | the record range, payload bytes, next sequence number and the per-class capacity demand |
| `Zenith_SideEffectRecord` | buffered side effect | its kind (`SIDE_EFFECT_KIND_*`), sequence number and payload location |
| `Zenith_CowShadow` | out-of-line field touched by a behaviour | the row and field, the committed storage it copies, the private copy, its reserved capacity and dirty flag |
| `Zenith_ScriptInvocationID` | invocation | 64 bits: VM index in bits 63..48, per-VM counter in bits 47..0 |
| `Zenith_ScriptLogicalID` | entity transaction | (stable simulation ID, invocation ordinal): the merge key after system order |

- [Z2-N-SCRIPT-211] Transaction memory (snapshots, records, payloads, shadows) is `Zenith_Arena` memory partitioned per (script system, frame-thread slot), because one worker interleaves sub-jobs of several script nodes of a phase: one scratch arena per frame-thread slot plus the main slot (the behaviour nodes' slot), holding the structures that grow while the slot's one running invocation executes and reset to that invocation's marker when it ends, and, per slot, retention arenas, one claimed by each script system at its first invocation on the slot in a tick, holding that system's transactions until its merge point and `Reset()` by that system's merge; its captures are trivially destructible, as every arena capture is (`Zenith2/Docs/Contracts.md` → "Ownership and allocation"), and ZenithScript has no allocator of its own.
<!-- elaboration -->
- [Z2-N-SCRIPT-224] At world creation each slot's scratch arena is pre-reserved to `round_up(2 × W, 64 KiB) + 64 KiB`, and entry 0 of each slot's retention set to W, where W is the worst case of one invocation: the per-entity snapshot cap (Z2-B-SCRIPT-012) times `uSCRIPT_MAX_ROWS_PER_INVOCATION` (256; a chunk with more rows is processed as several invocations of at most that many rows, which changes no result because the transaction boundary is the entity) plus the 32,896 fixed bytes of the transaction header and its entity, error and snapshot tables, plus, for the main slot, the largest declared checkpoint slot (Z2-B-SCRIPT-015); so every private reservation of an entity within its cap is served without touching the general heap, and every layout-independent overflow surfaces as `SNAPSHOT_CAP` (D-SCRIPT-TXN refinement, open, Stage 1, confirms this capacity model; `Zenith2/Docs/Design/SCRIPT/Transactions.md` → "Transaction arena partition").
- [Z2-N-SCRIPT-225] When transactions retained until the merge point push a retention arena beyond its retained block, it grows in blocks from the general heap; a growth failure is fatal (`SCRIPT_ERROR_KIND_RESERVATION`, world halted, not replayable), because general-heap state depends on scheduling and on every other subsystem, exactly as a VM cap trip does. A scratch arena never grows, and a retention arena's cap is derived from existing rows (Z2-B-SCRIPT-012, the world's entity capacity, Z2-B-GAME-003) so that no world within its rows reaches it: reaching that cap is a `ZENITH_CHECK` failure (an engine defect), never a script error, and adds no budget row (`Zenith2/Docs/Design/SCRIPT/Transactions.md` → "Retention ceiling derivation").

### Write path algorithm

A script write `e.Stats.Stamina = v` in a chunk system goes through the row proxy's `__newindex`, handled in `ZenithScript/Internal/Zenith_ScriptViews.cpp`:

1. Validate the view (invocation ID and structural epoch, see "Views epochs and misuse errors").
2. Resolve the field descriptor from the interned key string through the system's precomputed field table (perfect hash, O(1)).
3. Check the field is in the declared write set; otherwise raise the "write not declared" misuse error.
4. Check the target row is the current entity's row; otherwise raise the "write to another row" misuse error.
5. Check the field is not out-of-line (container or string) and not a handle; otherwise raise the misuse error for fields that chunk views expose for reading alone (ZS1205).
6. Convert and range-check `v` to the field's reflected kind (`Scripting/Bindings.md` → "Value marshalling"); on failure raise the type-mismatch or range misuse error.
7. If `m_bChunkSnapshotTaken` is false, take the first-write snapshot (below) and charge each entity of the invocation its row share.
8. Store the converted bytes in place through the field's `Zenith_RawColumn` (`xColumn = xChunk.GetColumn(xType, xProperty)`; row `r` lives at `xColumn.m_pData + (xColumn.m_puRowIndex ? xColumn.m_puRowIndex[r] : r) * xColumn.m_uStride`) and call `xColumn.MarkRowChanged(r)`, which sets the row's pending bit in the invocation's private change-bit set (`Zenith2/Docs/Architecture.md` → "ECS model").
9. Return; subsequent reads of the field in the same entity transaction read the stored value.

Behaviour writes through `self:Get(T.X).Field = v` follow the same steps with the behaviour's own entity as the current row and a row-granular snapshot taken on the first write of each invocation.

- [Z2-N-SCRIPT-212] Component writes go in place, so reads within the same entity transaction see the script's own writes.
- [Z2-N-SCRIPT-213] A component field with out-of-line storage (containers, strings) is never mutated in place from script; behaviours mutate it through its copy-on-write shadow, and chunk systems change strings and handles through commands.

### First-write snapshot

1. For each property in the system's declared write set that the chunk carries: obtain its `Zenith_RawColumn` from the invocation's `Zenith_ChunkView` and allocate `m_uRowCount * m_uFieldBytes` bytes in the transaction arena (`m_uRowCount` = the invocation's row range).
2. For each row `r` of the range, copy `m_uFieldBytes` from the row's address (write path step 8) to `m_puRowBytes + (r - m_uFirstRow) * m_uFieldBytes`; the row-index table makes this work for AoS and SoA storage alike.
3. Record the `Zenith_RawColumn` itself in the snapshot, so a restore needs no further ECS call.
4. Set `m_bChunkSnapshotTaken`.

- [Z2-N-SCRIPT-214] On the first write to a chunk, only that chunk's declared-writable field columns are snapshotted, over the invocation's row range; their change-tracking bits need no copy, because writes set only pending bits in the invocation's private set, which a rollback clears for the failing row; a column the job graph granted to another job is never snapshotted or restored, so jobs with disjoint access cannot clobber each other.
<!-- elaboration -->
- [Z2-N-SCRIPT-215] A behaviour invocation snapshots only its own entity's row (`m_uRowCount = 1`) of the declared-writable columns, on its first write in that invocation, because a behaviour transaction touches one row.
- [Z2-N-SCRIPT-216] Pending change bits are per (invocation, column) (`Zenith_RawColumn::m_pulRowChangeBits`), so the bits a job sets and clears belong to that job alone, and they are folded into the column's change-tracking state at the merge point for committed entities only (`Zenith2/Docs/Architecture.md` → "ECS model").

### Copy-on-write shadows

1. The first access through the behaviour API to an out-of-line field (`self:GetContainer(T.Inventory.Items)`, a string field read for write) of the own row allocates a `Zenith_CowShadow`, reserves `m_uShadowCapacityBytes` for the committed element count in the transaction arena, and copies the committed contents.
2. Reads through the container object (`:Get(i)`, `:Count()`, iteration) read the shadow, so the script reads back its own mutations.
3. Mutations (`:Set`, `:Insert`, `:Remove`, `:Clear`, string assignment) apply to the shadow; growth beyond the reserved capacity reserves more arena memory first and charges it to the entity's memory accounting, raising the per-entity snapshot-cap error when the cap would be exceeded.
4. At commit, a dirty shadow is swapped in ("Commit splice algorithm" step 2): a container through ECS's `Zenith_MergeContext::ReplaceContainer` into a block of the world's container heap accepted at the merge (capacity class container storage), the previous block released through ECS's deferred free at the end of the tick; a string interned and written in place. A subsequent invocation of the same instance in the same node chains its shadow on the earlier invocation's dirty shadow, so two invocations of one instance compose.
5. On failure or rejection, the shadow is dropped with the arena.

- [Z2-N-SCRIPT-217] A mutated container or string field gets a per-invocation copy-on-write shadow which the script reads back; the commit swaps it in and a failure drops it.
- [Z2-N-SCRIPT-218] Container fields are not exposed in chunk views; they are reached through the behaviour API alone.

### Side-effect sub-buffers

1. A buffered binding (`ctx:Emit`, `ctx:Spawn`, `ctx:Destroy`, `ctx:Command(...)`, `ctx:PlaySound`, `ctx:SpawnVfx`, `ctx:RequestSave`, `ctx:RequestPath`, `ctx:Contribute`) receives the current `Zenith_EntityTransaction` through its call frame.
2. An ECS-kind effect (spawn, destroy, add or remove component, reparent, field command, event, reduction contribution) is recorded in the entity's own ECS `Zenith_CommandBuffer`, which the ECS merge accepts or rejects through `Zenith_MergeContext`; a SCRIPT-held kind (audio trigger, VFX trigger, save request, asynchronous request) is serialized into the transaction's payload region as a `Zenith_SideEffectRecord` with `m_uSequence = m_uNextSequence++`. Both add their demand to `m_xDemand`, and both carriers take their sequence from the same per-entity counter, so every queue receives the records in (system order, logical invocation ID, sequence) order.
3. It charges the payload and record bytes (for an ECS record, the encoded size the command buffer reports) to the entity's memory accounting before writing; exceeding the per-entity cap raises the snapshot-cap error.
4. Nothing is appended to a world queue during the invocation.

- [Z2-N-SCRIPT-219] Structural commands, events, spawned entities, audio and VFX triggers, save requests, asynchronous requests and reduction contributions all go to the private per-entity sub-buffer of the running invocation (its ECS command buffer for the ECS kinds, its SCRIPT record list for the SCRIPT-held kinds), and nothing of it reaches a world queue or a consumer unless the entity transaction is accepted at the merge point: the ECS buffer through `Zenith_MergeContext::AcceptBuffer`, each SCRIPT record through its kind's `Zenith_ScriptSideEffectConsumer` at commit.
- [Z2-N-SCRIPT-226] Each SCRIPT-held side-effect kind has exactly one `Zenith_ScriptSideEffectConsumer` (the Luau-free ABI interface of `ZenithReflect/Zenith_ScriptBinding.h`, `Scripting/Bindings.md` → "Binding registration and the thunk ABI"), registered on the binding table with `SetSideEffectConsumer` before `Seal` by the library that owns the kind (audio and VFX: those modules; save requests: ZenithRuntime over ZenithSave; asynchronous requests: one ZenithRuntime multiplexer over the owning submitters); `Seal` refuses a buffered binding whose mask names a SCRIPT-held kind with no consumer (`SCRIPT_BINDING_ERROR_MISSING_CONSUMER`).

### Provisional spawn handles

- [Z2-N-SCRIPT-220] `ctx:Spawn(prefabOrArchetype)` returns a provisional `Zenith.Entity` (a handle with the provisional bit set and an invocation-local index) that the same entity transaction can pass to further commands; commands naming it are remapped to the real entity at playback.
- [Z2-N-SCRIPT-221] On rollback or rejection, the provisional handles issued by the entity transaction are released with its sub-buffer: no entity is created, no entity capacity is consumed, and the next accepted spawn in merge order receives the stable simulation ID it would have received had the failed spawn never been requested.
- [Z2-N-SCRIPT-222] A provisional handle stored into behaviour state is recorded in the checkpoint's fixup list and rewritten to the real handle when the spawn is played back; reading a provisional handle after playback resolves it through the world's one-tick remap table, and after that it is stale.

### Per-entity memory accounting

The per-entity transaction memory charged against the per-entity snapshot cap row is:

| Component | Bytes charged | Deterministic because |
|---|---|---|
| row share of column snapshots | Σ over the system's declared-writable fields of `m_uFieldBytes`, rounded up to 8 bytes | constant per system and archetype |
| copy-on-write shadows | reserved shadow capacity of each shadow the entity created | depends on the entity's own data |
| side-effect records and payloads | record size × record count + payload bytes | depends on the entity's own processing |
| behaviour checkpoint | the reserved checkpoint slot is pre-reserved at instance creation and not charged per invocation | constant per instance |

- [Z2-N-SCRIPT-223] The snapshot-memory cap is per entity, not per call, so it does not vary with chunk size; exceeding it raises a `SCRIPT_ERROR_KIND_SNAPSHOT_CAP` Luau error for that entity, which triggers that entity's rollback.

## Commit and failure semantics

### Failure classes

| Failure | Raised when | Kind | Scope of rollback | Module disabled from next tick | Replayable |
|---|---|---|---|---|---|
| script error | `error()`, a runtime type error, a thunk error, or an uncaught misuse error | `RUNTIME` / `MISUSE` | the entity transaction | yes | yes |
| interrupt abort (budget) | the safepoint count exceeds the limit | `BUDGET` | the entity transaction | yes | yes |
| `coroutine.yield` in a system | a chunk system yields | `YIELD_IN_SYSTEM` | the entity transaction | yes | yes |
| snapshot cap exceeded | the per-entity memory would exceed the cap | `SNAPSHOT_CAP` | the entity transaction | yes | yes |
| transaction-arena growth failure | a transaction arena already beyond its pre-reserved size cannot grow from the general heap (below the per-entity cap `SNAPSHOT_CAP` fires first) | `RESERVATION` | the whole tick is abandoned; world halts | — | no |
| checkpoint failure | the behaviour state cannot be serialized into its reserved slot | `CHECKPOINT` | the entity transaction | yes | yes |
| merge-time capacity rejection | the entity's demand exceeds the remaining shared capacity at the merge point | (diagnostic, no Luau error) | the entity transaction | no | yes |
| VM memory cap | the VM allocator refuses | `VM_MEMORY` | the whole tick is abandoned; world halts | — | no |
| watchdog | wall-clock limit exceeded | `WATCHDOG` | the whole tick is abandoned; world halts | — | no |
| internal exception | a non-Luau exception reaches the protected entry | `INTERNAL` | the whole tick is abandoned; world halts | — | no |

<!-- elaboration -->
- [Z2-N-SCRIPT-241] A merge-time capacity rejection rolls back the entity and reports a diagnostic, but does not disable the module, because it reflects shared world capacity rather than a fault of the script. (Confirmed by D-SCRIPT-TXN refinement, open, Stage 1.)
- [Z2-N-SCRIPT-242] A fatal event abandons the tick: no merge point of that tick commits, the world enters `SCRIPT_WORLD_STATE_HALTED`, the diagnostic is delivered to the game's fatal handler, and the tick's `REPLAY_RECORD_TICK` record carries `REPLAY_TICK_FLAG_NON_REPLAYABLE`, so a replay stops before it; this holds for every fatal script event (VM memory cap, transaction-arena growth failure, watchdog, internal exception).

### Execution-time failure algorithm

When an entity transaction raises (steps run on the thread that ran the invocation, before any other row is processed):

1. For each `Zenith_ColumnSnapshot` of the transaction, copy the failing row's `m_uFieldBytes` from `m_puRowBytes` back to the row's address in the recorded `Zenith_RawColumn`, and clear the row's pending bit (`m_pulRowChangeBits[r >> 6] &= ~(1ull << (r & 63))`); rows of other entities are untouched.
2. Drop every `Zenith_CowShadow` of the entity (`m_uFirstShadow`, `m_uShadowCount`).
3. Mark every SCRIPT record of the entity's sub-buffer discarded (the range stays in the arena until the transaction resets; it is skipped at merge) and abandon its ECS command buffer, which the merge rejects (`Zenith_MergeContext::RejectBuffer`), so its provisional spawn handles and demands are released.
4. Store the error in the transaction's error table; set `m_eState = ENTITY_TXN_STATE_FAILED`.
5. Set the module's `m_bDisableAtNextTickBoundary`.
6. For a behaviour instance, quarantine it (see "Quarantine semantics") and skip its remaining invocations of this tick; for a chunk system, continue with the next row.

- [Z2-N-SCRIPT-243] On failure, the failing entity's snapshotted rows are restored, its private buffers are discarded so no queued destroy or event survives, its shadows are dropped, and the error is reported (Z2-D-051).

### Merge point algorithm

The merge point is the continuation job of a script node, run after every sub-job of the node has joined; the merge points of one phase run as a chain in the stage's topological order (see "Merge chain across systems").

1. Collect the `Zenith_EntityTransaction` entries in state `SUCCEEDED` from every transaction of the node this tick.
2. Sort them by (logical ID `m_ulSimulationID`, then `m_uInvocationOrdinal`); system order is equal within a node. Complexity O(n) with an LSD radix sort over the 96-bit key (12 passes of 8 bits), or O(n log n) with a stable merge sort; ties are impossible because a logical ID is unique within a node and tick.
3. Walk the sorted list. For each entry compute whether its `m_xDemand` fits the remaining shared capacity of every class (see "Capacity classes"):
   - fits: subtract the demand from the remaining capacity, reserve the concrete slots (queue slots, entity slots, container-pool blocks) in merge order, set `ENTITY_TXN_STATE_ACCEPTED`;
   - does not fit: restore the entity's rows from the still-alive snapshots through their recorded `Zenith_RawColumn` (valid until the next sync point, because no playback runs between an invocation and its merge; `Zenith2/Docs/Architecture.md` → "ECS model"), clear its pending change bits, drop its shadows, reject its ECS command buffer (`Zenith_MergeContext::RejectBuffer`) and mark its SCRIPT records discarded, record `SCRIPT_MERGE_REJECTED_CAPACITY` with the capacity class and the entity, set `ENTITY_TXN_STATE_REJECTED`; for a behaviour instance, also reject its subsequent invocations in this node (see "Behaviour invocations of one instance").
4. Fold every accepted entity's contributions per reduction, in merge order, with the fixed-order reduction tree.
5. Commit (below).
6. Reset the system's retention arenas: for every slot on which the system claimed one this tick, `Reset()` the entry's arena, which drops the snapshots, and release the claim.

- [Z2-N-SCRIPT-244] The merge point decides acceptance and rejection of every entity transaction of a node before anything is committed, in merge order, so a rejection never depends on completion order.

### Commit splice algorithm

For each accepted entity in merge order:

1. Accept the entity's ECS command buffer (`Zenith_MergeContext::AcceptBuffer`): its structural and field commands, events and contributions join the phase's command and event queues in merge order. Then deliver each non-discarded SCRIPT record, in sequence order, to its kind's `Zenith_ScriptSideEffectConsumer::Consume(uWorldTag, eKind, ulCompletionID, xPayload)`; an asynchronous request first takes its completion ID from the world's `Zenith_AsyncResultTable` (`AllocateCompletionID`), whose payload slot is stamped with the requesting module and stable simulation ID. The capacity of both was accepted at step 3 of the merge, so neither has a failure path.
2. Swap each dirty `Zenith_CowShadow` into its field, in the entity's shadow order: a container field through ECS's `Zenith_MergeContext::ReplaceContainer`, which copies the shadow into a block of the world's container heap, swaps the row's container reference in place, marks the row changed at the merge version and queues the previous block for ECS's deferred free; a string field by interning the shadow's text and writing the `Zenith_StringID` in place with the row's pending change bit set.

`Zenith_MergeContext::ReplaceContainer(Zenith_ChunkView& xView, uint32 uRow, Zenith_TypeID xType, Zenith_PropertyID xProperty, Zenith_ByteSpan xElements, uint32 uElementCount)` is ECS's (`Engine/ZenithECS/Zenith_MergeContext.h`, `Zenith2/Docs/Design/ECS/SystemAPI.md` → "Zenith_MergeContext header"), with exactly the semantics of step 2: it has no failure path, because the `CONTAINER_BYTES` demand was accepted before commit; the new block is allocated in the world's container heap within that accepted demand, the row's container reference is swapped in place and stamped at the merge version, and the old block is freed by `FreeDeferred` at the end-of-tick flush. A container field is therefore visible after the merge point, as the table "What is transactional" states; the next-sync-point fallback through a container-replacement property command is unused (`Zenith2/Docs/Design/SCRIPT/Transactions.md` → "Copy-on-write shadow implementation").
3. For a behaviour invocation that produced a checkpoint, swap the instance's active checkpoint slot index (a single index flip).
4. Mark `ENTITY_TXN_STATE_COMMITTED`.

Then, once per node:

5. For every written column, call `CommitRowChanges` (through `Zenith_MergeContext::CommitRowChanges(xView, xType, xProperty, pulAcceptedRowBits)`, which forwards to `Zenith_ChunkView::CommitRowChanges`) with the pending bits of the accepted entities alone: it folds them into the column's change-tracking state and sets the column's change version to the merge's precomputed version `Zenith_MergeContext::GetMergeVersion()` (the version V(t, m) of this merge point in its stage's order), not to a counter read at commit time (`Zenith2/Docs/Architecture.md` → "ECS model").
6. Write each reduction's folded value into its target field and commit that column the same way through `CommitRowChanges`.

- [Z2-N-SCRIPT-245] On success, the snapshots are dropped, the private buffers and shadows are committed, and the written columns' change-tracking versions advance through `Zenith_ChunkView::CommitRowChanges` at the merge point; component writes need no copy because they are already in place.
- [Z2-N-SCRIPT-246] A commit never invalidates another invocation's view: structural epochs change only at sync-point playback, and a commit advances change-tracking versions alone.

### Commit cannot fail

- [Z2-N-SCRIPT-247] All capacity an invocation needs is reserved during the invocation (private arena memory for records, payloads, shadows and snapshots, served from the pre-reserved arena; the checkpoint slot at instance creation) or at the merge point (shared capacity); an entity that would exceed its private share raises the `SNAPSHOT_CAP` Luau error inside its transaction.
- [Z2-N-SCRIPT-248] Commit only splices memory that is already reserved and flips indices; it performs no allocation, no capacity check and no fallible operation, and it returns `void`.

### Merge chain across systems

<!-- elaboration -->
- [Z2-N-SCRIPT-249] The merge points of the script nodes of one phase run as a chain, each after the previous one completes (the frame graph adds a merge-chain edge between consecutive merge continuations), in the stage's cost-independent topological order: Kahn's algorithm over the stage's declared hazard and ordering edges, the chain itself excluded, ties broken by registration index, which for script systems is the system order of [Z2-N-SCRIPT-152] (`Zenith2/Docs/Architecture/ECSModel.md` [Z2-N-ARCH-692]); so a script system registered before a writer of a property it reads still schedules, the nodes' sub-jobs themselves still run in parallel, and shared capacity is consumed in chain order and, within one merge, in (logical invocation ID, sequence) order, independent of which node finished first and identical on every backend.
- [Z2-N-SCRIPT-250] Playback at the sync point applies the accepted records in merge-key order (system order, logical ID, sequence), whatever the chain order in which their merges accepted them, so the queue contents at the sync point are the concatenation, in system order, of each node's accepted records in (logical ID, sequence) order.

### Behaviour invocations of one instance

- [Z2-N-SCRIPT-251] When one instance has several invocations in one node (several event handlers, then its `Run` segment), each invocation is its own entity transaction with its own row snapshot; a subsequent invocation reads the in-place results of the earlier ones.
- [Z2-N-SCRIPT-252] If an earlier invocation of an instance fails or is rejected, every subsequent invocation of that instance in the same node is skipped or rejected with it, and the instance's row is restored to the snapshot of the earliest failed or rejected invocation, so no committed invocation ever builds on an uncommitted one.

## Determinism rules and randomness

The guarantee (Z2-D-019, stated in `Zenith2/Docs/Architecture.md` → "Determinism guarantee and mechanisms"): for the same backend and the same build, identical content and inputs give identical simulation results regardless of worker count, chunk layout or scheduling, including ticks where scripts fail or exhaust budgets. Cross-backend and cross-platform equality is not promised. This section specifies the script mechanisms.

### Ordering keys

| Ordered thing | Key | Where applied |
|---|---|---|
| command buffers from parallel invocations | (system order, logical invocation ID, sequence) | merge points and the merge chain |
| logical invocation ID | (stable simulation ID of the entity, invocation ordinal) | every entity transaction |
| behaviour execution within a node | (stable simulation ID, invocation ordinal) | the main-thread behaviour node |
| event delivery | the merged order of the emitting records | phase 9 |
| spawn ID assignment | the merged order of the spawn records at playback | each sync point |
| reduction folding | merge order of the contributing records | merge points |
| asynchronous result publication | completion ID | tick boundaries |

- [Z2-N-SCRIPT-271] Command buffers from parallel invocations are merged in (system order, logical invocation ID, sequence) order, never in completion order.
- [Z2-N-SCRIPT-272] The logical invocation ID of an entity transaction is (the entity's stable simulation ID, the invocation ordinal), never a chunk index, storage index, worker index or VM-local counter.
- [Z2-N-SCRIPT-273] Behaviours of one module and phase execute in ascending (stable simulation ID, invocation ordinal) order, so main-thread execution order is itself the merge order.
- [Z2-N-SCRIPT-274] The invocation ordinal of a behaviour invocation is its index among that instance's invocations in the current tick, in execution order (`OnCreate`, then per phase in phase order the event handlers in delivery order followed by `OnUpdate` or the `Run` segment, then a checkpoint-only transaction); a recovery invocation takes no ordinal of this sequence; a chunk-system invocation's ordinal is 0.
- [Z2-N-SCRIPT-928] A recovery invocation (`OnRecover`) is an entity transaction whose logical invocation ID is (stable simulation ID, `uSCRIPT_RECOVERY_ORDINAL` = 0xFFFFFFFF), a value no ordinary invocation reaches; its command buffer, SCRIPT records and random draws use that ordinal, so the instance's other invocations of the tick keep the ordinals, merge keys and draws of a tick without the recovery; within the merge an instance's recovery transaction is accepted or rejected before its ordinary invocations (the earliest-failure rule of [Z2-N-SCRIPT-252] treats it as the earliest), while playback stays in merge-key order.

### Counter-based random numbers

Script randomness is counter-based and derived per invocation. There is no stream object in the VM, no generator state in the world, and nothing to restore on rollback.

<!-- elaboration -->
The key of one draw is the 40-byte little-endian block:

```cpp
// Zenith2/Engine/ZenithScript/Internal/Zenith_ScriptRandom.h
struct Zenith_ScriptRandomKey          // 40 bytes, packed, little-endian on every platform
{
	uint64 m_ulWorldSeed;             // from the world's creation parameters (and the save / replay header)
	uint64 m_ulSourceID;              // stable source ID: Hash64 of the system or behaviour Name (UTF-8 bytes)
	uint64 m_ulTick;                  // the fixed-step simulation tick
	uint64 m_ulSimulationID;          // the entity's stable simulation ID
	uint32 m_uInvocationOrdinal;      // see "Ordering keys"; 0 for chunk systems
	uint32 m_uDrawCounter;            // 0 for the first draw of the entity transaction, +1 per 64-bit draw
};

uint64 Zenith_ScriptRandomDraw(const Zenith_ScriptRandomKey& xKey);  // = Hash64(bytes of xKey)
```

- [Z2-N-SCRIPT-275] Each 64-bit draw is `Hash64` of the 40-byte `Zenith_ScriptRandomKey` block (world seed, source ID, tick, stable simulation ID, invocation ordinal, draw counter); `Hash64` is the 64-bit hash function selected by D-SEL-HASH (open, Stage 1; the candidate is xxHash XXH3-64 with seed 0), and the same function computes `m_ulSourceID`.
- [Z2-N-SCRIPT-276] Randomness is never keyed by a chunk index, storage index, worker index or VM, all of which change with compaction and layout.
- [Z2-N-SCRIPT-277] The draw counter belongs to the entity transaction and starts at 0 for each entity transaction; re-running a failed invocation (in a replay) therefore produces the identical sequence, and two chunk systems running in parallel never share a counter.
- [Z2-N-SCRIPT-278] A change to the key layout, the hash function, or the derivation functions below changes every script's random sequence and therefore requires a replay-format version bump.

### Random API

| Luau call | Result | Derivation from draws `d0, d1, …` |
|---|---|---|
| `ctx.Random:NextU32()` | integer in [0, 2³²−1] | `d >> 32` |
| `ctx.Random:NextFloat()` | number in [0, 1) with 53 random bits | `(d >> 11) * 2⁻⁵³` |
| `ctx.Random:NextFloat32()` | number in [0, 1) exactly representable as f32 | `(d >> 40) * 2⁻²⁴` |
| `ctx.Random:Range(a, b)` | integer in [a, b], `b − a < 2³²` | Lemire's multiply-shift with rejection on the low word; a rejected draw consumes the next counter value |
| `ctx.Random:RangeFloat(a, b)` | number in [a, b) | `a + NextFloat() * (b − a)` |
| `ctx.Random:UnitVector()` | `vector` on the unit sphere | two `NextFloat` draws, `z = 1 − 2u`, `φ = 2πv` |
| `ctx.Random:Chance(p)` | boolean | `NextFloat() < p` |
| `ctx.Random:Shuffle(t)` | shuffles the array table `t` in place (a local table) | Fisher–Yates from the last index, one `Range` per step |

- [Z2-N-SCRIPT-279] `ctx.Random` is the only source of randomness available to simulation scripts, in both environments.

### Fixed-order reduction tree

1. At the merge point, the accepted contributions to one reduction are listed in merge order: `c[0] … c[n−1]`.
2. The list is cut into leaf blocks of 1,024 contributions (the last block may be shorter); the block boundaries depend on `n` alone.
3. Within a block, values are combined by a balanced pairwise tree: level 0 is the block's values; level k+1 combines elements `2i` and `2i+1` of level k; an odd trailing element is carried up unchanged.
4. The block results are combined by the same pairwise tree.
5. The combine operation is the reduction kind's: `+` in f64 for `SumF64`, `+` in wrapping i64 for `SumI64` and `Count`, `min`/`max` with NaN propagating for `Min`/`Max`.
6. Blocks may be evaluated in parallel sub-jobs; the result is identical because the tree shape depends on `n` alone and the order is the merge order.

- [Z2-N-SCRIPT-280] Parallel reductions use the fixed-order tree above, independent of worker count; the tree shape depends only on the number of accepted contributions and their merge order.

### Spawn ID assignment at playback

- [Z2-N-SCRIPT-281] Stable simulation IDs of spawned entities are assigned at sync-point playback, in merged order, from the world's monotonic simulation-ID counter; the counter's value is part of the world state that saves and replays record.
- [Z2-N-SCRIPT-282] Spawns are created at a sync point and are visible to the next phase (`Zenith2/Docs/Architecture.md` → "Lifecycle commands"); a behaviour's `OnCreate` runs at the first invocation of its instance in or after that phase.

### Event order

- [Z2-N-SCRIPT-283] Events are delivered in phase 9 in the merged order of their emitting records; within each subscribed instance, its handlers for them run in phase 10 in that same order, and instances are ordered as the behaviour-ordering bullet of "Ordering keys" states.
- [Z2-N-SCRIPT-284] An event emitted during phase 10 is delivered in phase 9 of the next tick.

### Iteration order and address-dependent values

<!-- elaboration -->
Luau hashes strings, numbers and booleans by value and deterministically; it hashes tables, functions, threads and userdata (including entity handles) by address. Iterating a table whose keys are address-hashed visits them in an order that can differ between VMs and runs.

- [Z2-N-SCRIPT-285] ZenithCook's script lint rejects, in every simulation module, a `pairs`/`next`/generalized `for … in t` iteration over a table whose inferred key type (from `luau-analyze` in strict mode) is not `number`, `string`, `boolean` or a string-literal union (`SCRIPT_LINT_ADDRESS_KEYED_ITERATION`); entity-keyed maps use `entity:Id()` (the stable simulation ID as a number) as the key.
- [Z2-N-SCRIPT-286] Behaviour state `Map` fields accept string, enum and integer keys alone, so no serialized or restored state depends on address order.
- [Z2-N-SCRIPT-287] No script-visible value exposes an address: the Zenith `tostring` hides addresses, and `Zenith.Entity` exposes its stable simulation ID and never its storage location.

### Determinism test matrix

Every determinism test runs one recorded replay under every combination of:

| Axis | Values |
|---|---|
| worker count | 1, 2, 8, all hardware threads |
| compaction | none; forced full chunk compaction at every sync point |
| chunk capacity | the backend default; an alternate capacity chosen so that no archetype's chunk boundaries coincide with the default's (for the custom backend: 13 rows per chunk) |
| ECS backend | Custom, flecs, EnTT (Z2-D-039) |

That is 16 runs per backend. The observable compared is the per-tick world-state hash: `Hash64` over every component field of every entity sorted by stable simulation ID, the event log of the tick, the played-back command log, every behaviour instance's committed checkpoint, and the simulation-ID counter.

- [Z2-N-SCRIPT-288] The same replay run with 1, 2, 8 and all workers, with and without forced compaction and with the default and the alternate chunk capacity, gives bit-identical per-tick world-state hashes, on each backend separately (`Test_Script_Determinism_ReplayMatrix`).

## Budgets safepoints and the watchdog

Script budgets count safepoints, not time (Z2-D-052). Wall-clock time serves as a watchdog alone, and tripping it is fatal.

### Safepoint counting

```cpp
// Zenith2/Engine/ZenithScript/Internal/Zenith_ScriptInterrupt.cpp
void Zenith_ScriptInterrupt(lua_State* pxState, int iGC);   // installed as lua_callbacks(L)->interrupt for every VM
```

1. If `iGC >= 0` the call is a GC step notification: return without counting.
2. Increment `m_ulSafepointCount` of the VM that owns `pxState`.
3. If the VM's `m_bWatchdogTripped` is set: set `m_eRaisedKind = SCRIPT_ERROR_KIND_WATCHDOG` and raise the catalogued watchdog error.
4. If `m_ulSafepointLimit != 0` and `m_ulSafepointCount > m_ulSafepointLimit`: set `m_eRaisedKind = SCRIPT_ERROR_KIND_BUDGET` and raise the catalogued budget error with the count, the limit, the module and the entity's stable simulation ID.
5. If a DAP pause request is pending for this VM (development builds), enter the debugger stop loop (see "Debugging and profiling").

- [Z2-N-SCRIPT-301] The script CPU budget is a safepoint count: Luau's interrupt fires at safepoints (loop back-edges, calls and returns) with `gc == -1`, and the count is deterministic for the same bytecode, so it is replayable.
- [Z2-N-SCRIPT-302] Interrupt calls with `gc >= 0` (GC step notifications) are never counted, because GC timing differs between VMs.
- [Z2-N-SCRIPT-303] Time spent inside C++ bindings is not counted; the wall-clock watchdog remains the backstop there.

### Budget scopes

| Scope | Limit source | Enforced as |
|---|---|---|
| one chunk-system entity | the chunk-system per-entity safepoint row, optionally lowered by the declaration's `Budget.SafepointsPerEntity` | `BUDGET` error for that entity |
| one behaviour invocation (callback or `Run` segment) | the behaviour per-invocation safepoint row, optionally lowered by `Budget.SafepointsPerInvocation` | `BUDGET` error for that invocation |
| `OnRecover` and `Migrate` steps | the recovery safepoint row | `BUDGET` error fails the recovery or migration |
| module load (module-scope code) | the module-load safepoint row | `LOAD` error |
| per tick, per world | the script safepoints per tick row | measured and reported, never enforced as a failure |

- [Z2-N-SCRIPT-304] Safepoint budgets are enforced per entity transaction (chunk-system entity or behaviour invocation), never per call and never per tick, so whether a budget is exceeded depends only on the entity's own processing.
- [Z2-N-SCRIPT-305] A declaration's `Budget` field can lower its limit below the budget row and never raise it.

### Abort-class errors and script pcall

- [Z2-N-SCRIPT-306] The budget error, the snapshot-cap error, the yield-in-system error and the checkpoint error are raised as a Zenith abort value (a tagged light userdata carrying the kind), which the Zenith `pcall` and `xpcall` re-raise, so they always reach the protected entry and roll back the entity transaction.
- [Z2-N-SCRIPT-307] After a budget abort has been raised, the interrupt callback raises it again at every further safepoint of the same entity transaction, so a script that somehow resumes execution cannot run past its budget.

### Native codegen conformance

Luau's native code generator (`--!native`, `luau_codegen_compile`) may not hit the same safepoints as the interpreter.

- [Z2-N-SCRIPT-308] D-SCRIPT-TXN refinement (open, Stage 1) includes the conformance test `Test_Script_Budget_NativeSafepointParity`, which runs every module of the budget adversarial corpus (`Zenith2/Engine/ZenithScript/Tests/Corpus/Budget/`: tight numeric loops, nested and generic `for` loops, `while` and `repeat` with `break`, deep recursion, vararg calls, fastcall built-ins, string and table library heavy code, closures created per call, metamethod-driven code) and of the representative corpus (`Zenith2/Engine/ZenithScript/Tests/Corpus/Representative/`) interpreted and native, over the same inputs, and asserts that each entity transaction's safepoint count and committed results match exactly. The test is staged: the Stage 1 D-SCRIPT-TXN ruling runs the budget corpus at VM level (each module's invocations as the transaction unit, with no world) on Windows x64 and the S24 and fixes `Zenith_ScriptNativePolicy` per platform from that run; the representative corpus, run per entity transaction, joins in Stage 3 as the extended test of the budget-scope ticket, once the world runtime exists; and no platform's `script.nativePolicy` is recorded `all` until that Stage 3 run passes on the platform (a Stage 1 policy is `disabled` or `toolsOnly`).
- [Z2-N-SCRIPT-309] If the parity test fails on a platform, native codegen is disabled for budget-limited simulation invocations on that platform (`Zenith_ScriptNativePolicy` = `SCRIPT_NATIVE_POLICY_TOOLS_ONLY`), and the policy is recorded in the Luau entry of `ThirdParty/manifest.json`; editor and tool invocations without a budget may still use native code.
- [Z2-N-SCRIPT-310] The parity test runs on Windows x64 and on the S24 (arm64) for every Luau pin change; a pin change that turns it red lands only together with the policy change.

```cpp
enum Zenith_ScriptNativePolicy : uint8
{
	SCRIPT_NATIVE_POLICY_DISABLED,     // no native code anywhere
	SCRIPT_NATIVE_POLICY_TOOLS_ONLY,   // native only for unbudgeted tool and editor invocations
	SCRIPT_NATIVE_POLICY_ALL,          // native for --!native modules everywhere; valid only while parity passes
};
```

### Wall-clock watchdog

`Zenith_ScriptWatchdog` is owned by the Runtime composition root (one per process, passed to every `Zenith_ScriptWorld`; no global).

1. A dedicated watchdog thread (`Zenith.ScriptWatchdog`) wakes every 10 ms and reads each registered VM's `m_ulWatchdogStartTicks`, under `uLOCK_LEVEL_SCRIPT_WATCHDOG`; a VM whose `m_bDebuggerStopped` is set is skipped.
2. If a VM's invocation has run longer than the script watchdog row's limit, it sets `m_bWatchdogTripped`; the next safepoint raises the fatal watchdog error.
3. If the invocation is still running after twice the limit (it is stuck inside a C++ binding, where no safepoint occurs), the watchdog reports a fatal hang through the crash handler (D-SEL-CRASH, open, Stage 2), which writes a dump including the Luau traceback captured with `lua_debugtrace`, and terminates the process with `ZENITH_FATAL(FATAL_CODE_SCRIPT_HANG)`.
4. While a VM is stopped in the debugger the watchdog skips that VM, and when the VM resumes it advances its invocation's start time by the stop's duration, so the stopped time does not count towards the limit; the world's other VMs stay watched.

<!-- elaboration -->
- [Z2-N-SCRIPT-315] A debugger stop exempts the stopped VM alone from the watchdog: the VM's own thread sets `m_bDebuggerStopped` (release) before it blocks, and on resuming adds the stop's duration to its published start time (when an invocation is running) and to `m_ulWatchdogRebaseTicks` before it clears the flag (release); the watchdog has no world-level suspension, and the poll decides a trip on a start time re-read after it saw the flag clear, so every elapsed time a trip or a hang report acts on is running time (`Zenith2/Docs/Design/SCRIPT/Runtime.md` → "Watchdog internals").

- [Z2-N-SCRIPT-311] Wall-clock time is only a watchdog: tripping it is a fatal, non-replayable diagnostic event, never a recoverable rollback, and it never changes a safepoint count.
- [Z2-N-SCRIPT-312] The watchdog's limit is the script watchdog row, set well above the time the largest per-entity safepoint budget can take on the S24, so a watchdog trip indicates a stuck binding or an engine defect, not a slow script.

### Snapshot caps

- [Z2-N-SCRIPT-313] Snapshot memory has a hard per-entity cap (the per-entity snapshot cap row), enforced as a `SNAPSHOT_CAP` Luau error, and a per-tick total (the per-tick snapshot total row), measured and reported.

### Checkpoint cost

- [Z2-N-SCRIPT-314] Behaviour checkpoint serialization cost is a budget row (the checkpoint cost row: bytes serialized per tick and main-thread time per tick for checkpoints), measured in every workload run; the reserved checkpoint slot size per instance is bounded by the checkpoint slot size row.

### Budget rows used

Every budget row this document names by a descriptive name (the main and worker VM memory caps, GC tuning and GC step, the safepoint budgets, the CPU and GC time rows, the watchdog, the snapshot, checkpoint and capacity rows, the hot-reload row) is mapped to its `Zenith2/Docs/Budgets.md` row ID, name, heading and breach code in `Scripting/BudgetRows.md` → "Budget rows used".

## Merge-time capacity and asynchronous results

### Capacity classes

Shared capacity is checked at the deterministic merge point, in merge order, and is not reserved first-come during parallel execution.

```cpp
// Zenith2/Engine/ZenithReflect/Zenith_ScriptBinding.h (excerpt): declared with the binding ABI, because thunks build demands
enum Zenith_ScriptCapacityClass : uint8
{
	SCRIPT_CAPACITY_CLASS_ENTITY_SLOTS,       // spawns: world entity capacity
	SCRIPT_CAPACITY_CLASS_COMMAND_QUEUE,      // structural and field commands: slots in the sync-point command queue
	SCRIPT_CAPACITY_CLASS_EVENT_QUEUE,        // events: slots in the tick's event queue
	SCRIPT_CAPACITY_CLASS_CONTAINER_STORAGE,  // committed container and string shadows: bytes in the world container pool
	SCRIPT_CAPACITY_CLASS_STRING_INTERN,      // strings new to the world's intern table: entries and bytes
	SCRIPT_CAPACITY_CLASS_AUDIO_TRIGGERS,     // audio triggers per tick
	SCRIPT_CAPACITY_CLASS_VFX_TRIGGERS,       // VFX triggers per tick
	SCRIPT_CAPACITY_CLASS_SAVE_REQUESTS,      // save requests per tick
	SCRIPT_CAPACITY_CLASS_ASYNC_REQUESTS,     // in-flight asynchronous requests per world
	SCRIPT_CAPACITY_CLASS_COUNT,
};

struct Zenith_ScriptCapacityDemand
{
	uint32 m_auCount[SCRIPT_CAPACITY_CLASS_COUNT] = {};   // slots, entries, or bytes for CONTAINER_STORAGE and STRING_INTERN
};
```

| Class | Remaining capacity at the start of a phase | Returned when |
|---|---|---|
| entity slots | world entity capacity minus live and pending entities | destroy playback at the end of the tick |
| command queue | Z2-B-ECS-004 "Structural command capacity per tick" minus commands queued this tick | queue drained at the sync point |
| event queue | Z2-B-GAME-003 "Event queue capacity per tick" minus events queued this tick | delivery at phase 9 |
| container storage | Z2-B-SCRIPT-016 "Container storage capacity per tick" and the free bytes of the world container pool | deferred free at the sync point |
| string intern | Z2-B-SCRIPT-022 "String-intern capacity per world session" minus entries and bytes in use | never within a world session |
| audio / VFX triggers | Z2-B-SCRIPT-023 and Z2-B-SCRIPT-024 minus triggers queued this tick | the next tick |
| save requests | Z2-B-SCRIPT-025 "Save-request capacity per tick" minus requests this tick | the next tick |
| async requests | Z2-B-SCRIPT-026 "Per-source asynchronous request capacity in flight" (one value each for script path requests, streaming loads, deferred physics queries and online calls) minus requests in flight or published and not yet released | release of the result: at the tick boundary after an invocation that read it commits, or when its owning instance is destroyed or quarantined |

- [Z2-N-SCRIPT-331] Shared capacity (entity IDs, command and event queues, container storage, the intern table, triggers, save and async requests) is checked at the merge point in merge order, never reserved first-come during parallel execution.
- [Z2-N-SCRIPT-332] An entity whose side effects exceed the remaining capacity of any class at its merge is rolled back deterministically as an entity-level failure with a `SCRIPT_MERGE_REJECTED_CAPACITY` diagnostic naming the class; commit still cannot fail, because acceptance happens before commit.
- [Z2-N-SCRIPT-333] A rejected entity's demand is not subtracted, so a subsequent entity in merge order with a smaller demand can still be accepted; acceptance is a pure function of the merge-ordered demands and the phase's starting capacity. (First-fit acceptance is confirmed by D-SCRIPT-TXN refinement, open, Stage 1.)

### Deterministic rejection

1. At the start of each phase, each class's remaining capacity is computed from world state after the preceding sync point, a value that depends on committed state alone.
2. Each merge point in the chain walks its accepted-candidate list in merge order and applies the fit test of "Merge point algorithm" step 3 to all classes at once.
3. A rejection is final for that tick; a script that needs to retry does so in a subsequent tick through its own logic (a behaviour state flag, or re-emitting next tick).

### Asynchronous requests and completion IDs

Asynchronous work requested by scripts: pathfinding (`ctx:RequestPath`), streaming loads (`ctx:RequestLoad`), physics queries deferred beyond the tick (`Physics.RequestQuery`, the binding the Physics module registers), and online-service calls (`ctx:RequestOnline`, against `Zenith_OnlineServices`).

1. The request is a `SIDE_EFFECT_KIND_ASYNC_REQUEST` record; the call returns a `Zenith.AsyncTicket` holding a provisional ticket index.
2. At commit, in merge order, the world assigns the request its completion ID from a monotonic per-world counter and submits it to the owning subsystem as an operation handle.
3. A rolled-back or rejected request is not submitted and consumes no completion ID.

- [Z2-N-SCRIPT-334] Completion IDs are assigned at commit in merge order from a per-world monotonic counter, so the completion ID of every request is independent of worker count and completion timing.
- [Z2-N-SCRIPT-339] A `Zenith.AsyncTicket` returned during an invocation holds a provisional ticket index; at commit it is remapped to the request's completion ID through the same one-tick remap table as provisional spawn handles, and a ticket still provisional after that window raises ZS1105.
- [Z2-N-SCRIPT-340] `Zenith.AsyncTicket` is not a serializable state kind: a coroutine that held a ticket across a yield and is restored by `OnRecover` re-issues its request.

### Asynchronous source kind registry

SCRIPT owns the values of `Zenith_AsyncSourceKind` (declared in `Engine/ZenithSave/Zenith_ReplayLog.h`, `Zenith2/Docs/Architecture.md` → "Multiplayer foundations") as a registry: this table assigns them, and every other document cites them.

| Value | Enumerator | Owner and results | In-flight capacity from |
|---|---|---|---|
| 0 | `ASYNC_SOURCE_MODULE` | any other module operation | the module's `Zenith_ModuleSourceDesc::m_uMaxInFlight` |
| 1 | `ASYNC_SOURCE_PATHFINDING` | `ctx:RequestPath` | Z2-B-SCRIPT-026's path value (the AI row Z2-B-AI-003) |
| 2 | `ASYNC_SOURCE_STREAMING` | `ctx:RequestLoad` of cooked content | Z2-B-SCRIPT-026's streaming value |
| 3 | `ASYNC_SOURCE_PHYSICS_QUERY` | `Physics.RequestQuery` (deferred physics queries; registered by the Physics module) | Z2-B-SCRIPT-026's physics-query value |
| 4 | `ASYNC_SOURCE_ONLINE` | ONLINE (`Zenith2/Docs/Design/ONLINE.md`), a mixed kind: ONLINE's script requests, whose results are read through tickets, and ONLINE's own notifications, whose completion IDs the module allocates on the main thread in the frame prologue (never under `--replay`) and which its module-side handler consumes at tick-boundary step 1 through the module publication sink; every publication `PUBLICATION_KIND_GENERIC_RECORDED`, at most 256 bytes | Z2-B-SCRIPT-026's online value plus 8 notifications per world |
| 5 | `ASYNC_SOURCE_SAVE` | SAVE's save, load and listing results, through ZenithRuntime's save bridge (`Zenith2/Docs/Design/SAVE/Formats.md`) | `uSaveMaxOutstanding` (`Zenith2/Docs/Design/SAVE.md` → "Asynchronous source capacity") |
| 6 | `ASYNC_SOURCE_WORLD_LOAD` | the world host's transition loads, `PUBLICATION_KIND_WORLD_LOADED` (`Zenith2/Docs/Design/GAME/WorldHost.md`) | 1 per world |
| 7 | `ASYNC_SOURCE_PLATFORM` | world-visible platform events, `PUBLICATION_KIND_PLATFORM_RECORDED` (the PLAT bridge, [Z2-N-ARCH-791]) | the PLAT bridge's declaration (`Zenith2/Docs/Design/PLAT.md` → "Threading lifetime and error contract") |
| 8 | `ASYNC_SOURCE_SCENE_LOAD` | SCENE's additive loads, the 8-byte `Zenith_SceneLoadResult` (`Zenith2/Docs/Design/SCENE/Loading.md`) | `scene.maxAdditiveInstances` |
| 9 | `ASYNC_SOURCE_ANIMATION` | ANIM's asset bindings and clip-chunk arrivals: success `PUBLICATION_KIND_GENERIC_RECOMPUTED`, failure `PUBLICATION_KIND_GENERIC_RECORDED` with the 24-byte `Zenith_AssetLoadResultRecord` (`Zenith2/Docs/Design/ANIM/Runtime.md`) | `animation.bindingQueueCapacity` + `animation.streaming.maxReadsInFlight` |
| 10 | `ASYNC_SOURCE_AUDIO` | provisional: AUDIO's script notifications (`Zenith.Audio.WhenFinished`, `WhenMarker`, `WhenDeviceChanged`, `Zenith2/Docs/Design/AUDIO.md`) | Z2-B-SCRIPT-026's audio value |
| 11 | `ASYNC_SOURCE_UI` | UI events published as recorded results, `PUBLICATION_KIND_GENERIC_RECORDED` (`Zenith2/Docs/Design/UI/Interaction.md`) | `ui.events.maxInFlight` |
| 12 | `ASYNC_SOURCE_CINEMATICS` | CINE's sequence and video bindings: success `PUBLICATION_KIND_GENERIC_RECOMPUTED` with no payload, failure `PUBLICATION_KIND_GENERIC_RECORDED` with ASSET's 24-byte load result; declared per world on the module publication sink, merge-chain allocation (`Zenith2/Docs/Design/CINE/Playback.md`) | 64 per world (CINE's declaration) |
| 13 | `ASYNC_SOURCE_NAVIGATION` | AI's navigation-file loads: the same two publication kinds and payload as 12, merge-chain allocation by `AI.NavIngest` (`Zenith2/Docs/Design/AI/Navigation.md`) | `ai.nav.maxLoadsInFlight` |
| 14 | `ASYNC_SOURCE_PACKAGE` | PACK's package, update and DLC events: `PUBLICATION_KIND_GENERIC_RECORDED` with the 96-byte payload, prologue allocation, never under `--replay` (`Zenith2/Docs/Design/PACK/Algorithms.md`) | `uPACKAGE_EVENT_QUEUE_CAPACITY` (512) |
| 15 | `ASYNC_SOURCE_NETWORK` | NET's snapshot fragments and session events, and at Stage 7 its message pieces and `PEER_VIEW` records (`Zenith2/Docs/Design/NET/Loopback.md` → "Fragment publication"): sink-declared per session world, allocated in the frame prologue only, never under `--replay`; every publication `PUBLICATION_KIND_GENERIC_RECORDED`, at most 1,024 bytes | `net.client.maxFragmentsInFlight` (256) on a client, 64 on a server |
| 16 | `ASYNC_SOURCE_XR` | ZenithXR (Stage 7, `Zenith2/Docs/Design/SCOPE/XrPublicAPI.md`), a mixed kind declared with `DeclareScriptAsyncSource`: ticketless notifications allocated on the main thread in the frame prologue, never under `--replay`, consumed by the module's handler at tick-boundary step 1; every publication `PUBLICATION_KIND_GENERIC_RECORDED`, at most 64 bytes | `uXR_NOTIFY_MAX_OUTSTANDING` (8 per world) |
| 17 … 31 | none | unassigned | — |

Values 9, 10 and 11 were assigned in drafting order (ANIM, AUDIO, UI), and 12, 13 and 14 in request order (CINE, AI, PACK; ruling ASYNC-SOURCE-REGISTRY); 15 and 16 were assigned by ruling ASYNC-SOURCE-15-16 (NET, SCOPE). The capacity column names where each kind's in-flight capacity comes from (`Zenith2/Docs/Design/SCRIPT/Runtime.md` → "Source kind values"). A provisional value is reserved for its owner exactly as an assigned one is; it differs in one respect, that its owner's design has not yet confirmed the use.

- [Z2-N-SCRIPT-924] The values of `Zenith_AsyncSourceKind` are assigned only in the table "Asynchronous source kind registry": a new kind takes the next unassigned value in the diff that first declares it, appending its row; a value is never renumbered or reused, at most `uASYNC_SOURCE_KIND_CAPACITY` (32) values exist, and the enumeration's declaration in `Zenith_ReplayLog.h` lists exactly the table's assigned and provisional rows.
- [Z2-N-SCRIPT-925] A consumer (an area design, a ticket, a `DeclareSource`, `DeclareScriptAsyncSource` or `DeclareAsyncSource` call) names a source kind by its enumerator and cites this registry, and never declares or writes a numeric value of its own; a document that states a value different from this table's is in conflict with it, and the table wins.

### Tick-boundary publication

1. Subsystems complete operations on their own threads at any time; a completed result is parked in the world's `Zenith_AsyncResultTable` through `Complete`, invisible to scripts.
2. At each tick boundary (after the last simulation phase of tick T and before phase 1 of tick T+1; render extraction, phase 12, runs once per frame and is not a tick boundary) of a world that is not paused, the world publishes every parked result, sorted by completion ID, into the table's published set; a source kind with a C++ handler (`Zenith_AsyncPublicationHandler`) receives each of its publications at that step with the tick boundary's `Zenith_TickBoundaryAccess`. A paused world applies the pause at its tick-boundary node and publishes nothing: results that complete while it is paused stay parked and publish in completion-ID order at the first tick boundary after it resumes, so the recorded tick of every publication is a tick that ran.
3. From tick T+1 on, `ticket:IsReady()` returns true and `ticket:Result()` returns the result; `Zenith.WaitAsync(ctx, ticket)` in a behaviour `Run` coroutine resumes in the first segment at or after publication.
4. A result for an entity that has been destroyed is published and discarded; a result for a world that has been transitioned away is discarded at the transition (`Zenith2/Docs/Architecture.md` → "Lifecycle commands").

- [Z2-N-SCRIPT-335] Asynchronous results (pathfinding, streaming loads, physics queries, online calls) become visible only at a tick boundary of a world that is not paused, in completion-ID order (Z2-D-053); a result completing while its world is paused publishes at the first tick boundary after the world resumes.
- [Z2-N-SCRIPT-341] The world's pending-result set and its monotonic completion-ID counter are owned by `Zenith_AsyncResultTable` (ZenithScript, public header `Zenith_ScriptAsync.h`, `Scripting/PublicTypes.md` → "Public types"), one per `Zenith_ScriptWorld`, which exists for every world, including one without script modules: producers declare their source kinds (`DeclareSource`), handlers (`SetHandler`) and resolvers (`SetPublicationResolver`) before the first tick, `AllocateCompletionID` runs on the world's merge chain or on the main thread in the frame prologue, an allocation outside a tick (the frame prologue) is made only for a RECORDED publication kind, and under `--replay` a prologue producer of a recorded kind that needs the recorded completion ID (the world host's transition intake) reads it from the replay log and allocates nothing while the table adopts, at each tick boundary, the next completion ID recorded in the `REPLAY_RECORD_TICK` record (a live counter above it being a replay divergence that halts the world, [Z2-N-ARCH-794]), `Complete` on any thread, and `GetNextCompletionID` / `RestoreNextCompletionID` save and restore the counter with the world's state; the replay recorder observes publications through `Zenith_ScriptPublicationObserver`, the replay reader supplies them through `Zenith_ScriptPublicationSource`, and under `--replay` `Zenith_ScriptRuntime::CanRunTick` holds a tick until every recomputed result recorded for it has completed.
- [Z2-N-SCRIPT-931] Every completion ID allocated from a world's `Zenith_AsyncResultTable` is completed exactly once by the producer that allocated it (or took ownership of it), including work its `DetachWorld` cancels, which it completes with its failure or cancellation payload (`Complete` being `ANY_THREAD`); a producer never counts a discarded result itself, because the table, live or retired, discards and counts it and the world host is the sole publisher of `world.async_discarded` ([Z2-N-ARCH-558]); and a table is destroyed only when no producer of its world can still complete into it, a world destroyed with results in flight retiring its table instead ([Z2-N-ARCH-558], [Z2-N-ARCH-560]).
- [Z2-N-SCRIPT-342] A published script result stays in the table until an invocation that read it (`ticket:Result()` or a `Zenith.WaitAsync` resume) commits, after which it is released at the next tick boundary, or until its owning instance is destroyed or quarantined; its in-flight unit of Z2-B-SCRIPT-026 returns at that release, not at publication, and a result no behaviour reads is traced by the cumulative counter `script.async.published_unconsumed.<kind>` and, when it blocks a request, by `SCRIPT_ASYNC_CAPACITY_HELD` (`Scripting/MisuseCatalogue.md` → "Catalogue").

### Replay-log recording

Publications are recorded in the replay log `ZRPL` owned by ZenithSave (`Engine/ZenithSave/Zenith_ReplayLog.h`, `Zenith2/Docs/Architecture.md` → "Multiplayer foundations"): each `REPLAY_RECORD_TICK` record carries, after its input commands, a publication count and one `Zenith_ReplayPublicationHeader` plus payload per published result, in completion-ID order. The publication tick is the enclosing `TICK` record's tick index and is not stored per result.

```cpp
// Engine/ZenithSave/Zenith_ReplayLog.h (excerpt; ZenithSave owns it, every field serialized in order, little-endian)
// Zenith_AsyncSourceKind (the values of "Asynchronous source kind registry") and Zenith_PublicationKind
// (PUBLICATION_KIND_GENERIC_RECOMPUTED, _GENERIC_RECORDED, _WORLD_LOADED, _PLATFORM_RECORDED) are declared there as Architecture lists them;
// Zenith_AsyncResultTable carries the source kind as its uint8 value and sizes uASYNC_SOURCE_KIND_CAPACITY (32) source kinds (0 … 31),
// mirrored by uSCRIPT_ASYNC_SOURCE_KIND_CAPACITY in ZenithReflect/Zenith_ScriptBinding.h because ZenithScript does not depend on ZenithSave.
struct Zenith_ReplayPublicationHeader        // 16 bytes, then m_uPayloadBytes of payload
{
	uint64 m_ulCompletionID;
	uint8 m_uKind;                           // Zenith_PublicationKind
	uint8 m_uSourceKind;                     // Zenith_AsyncSourceKind
	uint16 m_uReserved;                      // 0
	uint32 m_uPayloadBytes;                  // 0 for PUBLICATION_KIND_GENERIC_RECOMPUTED
};

enum Zenith_ReplayTickFlags : uint16
{
	REPLAY_TICK_FLAG_NONE = 0,
	REPLAY_TICK_FLAG_NON_REPLAYABLE = 1u << 0,   // set on the TICK record of a tick abandoned by a fatal event; a replay stops before it
};
```

| Script request | `m_uSourceKind` | `m_uKind` |
|---|---|---|
| `ctx:RequestPath` | `ASYNC_SOURCE_PATHFINDING` | `PUBLICATION_KIND_GENERIC_RECOMPUTED` |
| `ctx:RequestLoad` of cooked content (success) | `ASYNC_SOURCE_STREAMING` | `PUBLICATION_KIND_GENERIC_RECOMPUTED` |
| `ctx:RequestLoad` that failed on I/O | `ASYNC_SOURCE_STREAMING` | `PUBLICATION_KIND_GENERIC_RECORDED` |
| `Physics.RequestQuery` (deferred physics query; Physics module binding) | `ASYNC_SOURCE_PHYSICS_QUERY` | `PUBLICATION_KIND_GENERIC_RECOMPUTED` |
| the `Online.*` bindings (the Online module's namespace; a `SIDE_EFFECT_KIND_ASYNC_REQUEST` of that kind) | `ASYNC_SOURCE_ONLINE` | `PUBLICATION_KIND_GENERIC_RECORDED` |
| `Scene.LoadAdditive` (success) | `ASYNC_SOURCE_SCENE_LOAD` | `PUBLICATION_KIND_GENERIC_RECOMPUTED`, the 8-byte `Zenith_SceneLoadResult` with code 0 |
| `Scene.LoadAdditive` (failure) | `ASYNC_SOURCE_SCENE_LOAD` | `PUBLICATION_KIND_GENERIC_RECORDED`, the 8-byte `Zenith_SceneLoadResult` with the error's code and detail |

Consumer note for the three C++-handled source kinds. `ASYNC_SOURCE_UI` (11) carries UI events published as recorded results: one handler per world, a payload of at most 448 bytes, completion IDs allocated on the main thread in the frame prologue; the handler decodes each publication into the UI module's per-world publication buffer at tick-boundary step 1 (`Zenith2/Docs/Design/UI/Interaction.md`). `ASYNC_SOURCE_PLATFORM` (7) carries world-visible platform events (lifecycle, thermal, account, device, disk, locale, configuration): the PLAT bridge's `Zenith_AsyncPublicationHandler` receives each publication at tick-boundary step 1, its kind is `PUBLICATION_KIND_PLATFORM_RECORDED`, so a replay publishes the recorded 36-byte payload, and scripts see the event as the `Platform.*` event the bridge emits, delivered in phase 9 of that tick, not through a ticket. `ASYNC_SOURCE_SCENE_LOAD` (8) carries SCENE's additive loads, the 8-byte `Zenith_SceneLoadResult { m_uInstance, m_uCode, m_uDetail, m_uReserved }` on both publication kinds: ZenithRuntime declares the source with an 8-byte maximum and forwards each publication to SCENE at step 1; a C++ request's slot is released after the forwarder ran, and a script request's publication stays for its ticket, whose `Result()` returns `{ instance: number, ok: boolean, error: string? }` through the result decoder SCENE registers for kind 8 (`Zenith2/Docs/Design/SCRIPT/Runtime.md` → "Asynchronous result table"). `ASYNC_SOURCE_ONLINE` (4) is a mixed kind of the same shape: script results are read through tickets, decoded by ONLINE's registered result decoder, and ONLINE's own notifications, whose completion IDs the module allocates in the frame prologue, are consumed by its module-side handler at tick-boundary step 1 through the single module publication sink; a ticketless (C++-allocated) publication of a mixed kind has its slot released after the handler ran, while a script request's publication stays for its ticket.

- [Z2-N-SCRIPT-343] A source kind whose results C++ consumes (`ASYNC_SOURCE_PLATFORM`, `ASYNC_SOURCE_SCENE_LOAD`, `ASYNC_SOURCE_UI`, `ASYNC_SOURCE_ONLINE`) registers exactly one `Zenith_AsyncPublicationHandler` per world with `Zenith_AsyncResultTable::SetHandler` before the first tick; the handler runs at tick-boundary step 1, writes world state only through the tick boundary's `Zenith_TickBoundaryAccess` (property reads and writes, never structure), and records nothing a script sees before the tick's phases run.

- [Z2-N-SCRIPT-336] The tick at which each asynchronous result was published is written to the replay log, and replay publishes it at the same tick; recomputable results (pathfinding, physics queries, successful loads of cooked content) are recomputed and held until their recorded tick, and non-reproducible results (online calls, I/O failures) are replayed from the recorded payload.
- [Z2-N-SCRIPT-337] In a replay, a recomputed result that is not ready by its recorded publication tick holds the tick until it is ready, so the replayed simulation never observes a different publication tick.
- [Z2-N-SCRIPT-338] Publication ticks are simulation inputs: two live runs whose asynchronous work completes at different wall-clock moments may diverge, and replaying either run's log reproduces that run exactly.

## Stateless system enforcement

### The rule

- [Z2-N-SCRIPT-351] Chunk-system modules, and every library module they require, hold no state: nothing a chunk-system invocation does can be observed by a subsequent invocation except through committed component data, events and commands.
- [Z2-N-SCRIPT-352] The rule is enforced at load time by the mechanism D-LUAU-STATELESS selects; a module that violates it is rejected when it is loaded, never merely caught by a test or at runtime.

### Decision D-LUAU-STATELESS

D-LUAU-STATELESS (open, Stage 1) decides how the rule is enforced. It is a harness ticket (the prototype and the corpus) plus a ruling ticket (Z2-D-084).

| # | Alternative | Mechanism | Strengths | Weaknesses the prototype measures |
|---|---|---|---|---|
| 1 | Compile-time AST validation | a pass over the Luau AST (`Luau::AstStatBlock` from `Luau.Ast`) at cook time and at hot-reload compile, rejecting upvalue writes and any mutation reachable from module scope in system modules | zero runtime cost; precise diagnostics with line numbers | aliasing and inter-procedural flow (a module-scope table passed to a helper that mutates its parameter) need a conservative data-flow analysis; dynamic indexing defeats name-based checks |
| 2 | Isolated per-invocation environments | rebuild the module's closures and module-scope values for every call (re-run module scope in a fresh environment, or clone the module's function prototypes with fresh upvalues) | catches everything by construction: no state can survive an invocation | per-invocation cost (module-scope re-execution or closure cloning) multiplied by entity count; does not by itself reject at load, so it needs a load-time companion to satisfy the acceptance rule |
| 3 | Restricted scripting subset | systems may only bind, at module scope, literals, deep-frozen constant tables, functions, and `require` results of accepted modules; functions may not assign upvalues, call a mutating library function on an upvalue, or pass an upvalue to any function outside a non-mutating allowlist | simple, checkable syntactically | expressiveness: helper libraries need to be written in the subset; the allowlist needs maintenance |
| 4 | A combination | e.g. 3 as the load-time rejection plus 1's data-flow analysis for aliases, with 2 as a development-build audit mode | defence in depth | combined cost and complexity |

- [Z2-N-SCRIPT-353] The D-LUAU-STATELESS ruling is appended to `Zenith2/Docs/DecisionLog.md` with an explicit `Z2-D-NNN` in the ruling ticket's own diff, together with the measurements of `Scripting/StatelessCorpus.md` → "Alternative evaluation procedure".
- [Z2-N-SCRIPT-354] Whichever alternative D-LUAU-STATELESS selects, the enforcement covers captured locals, nested tables, imported modules, mutable userdata and stateful library functions, and each of those five has at least one adversarial corpus module.
- [Z2-N-SCRIPT-355] Stateful library functions (`math.random`, `math.randomseed`, the `os` library and similar) are removed from the system environment regardless of the alternative selected.

### Load-time interface

Every alternative presents the same interface to the loader, so the loader and the tests do not change when the ruling lands:

```cpp
// Zenith2/Engine/ZenithScriptCompiler/Zenith_ScriptStateless.h (public header of ZenithScriptCompiler)
enum Zenith_ScriptStatelessDiagnostic : uint8
{
	SCRIPT_STATELESS_OK,
	SCRIPT_STATELESS_UPVALUE_WRITE,
	SCRIPT_STATELESS_MODULE_SCOPE_MUTATION,
	SCRIPT_STATELESS_NESTED_TABLE_MUTATION,
	SCRIPT_STATELESS_GLOBAL_WRITE,
	SCRIPT_STATELESS_IMPORTED_STATE,
	SCRIPT_STATELESS_MUTABLE_USERDATA,
	SCRIPT_STATELESS_STATEFUL_LIBRARY,
	SCRIPT_STATELESS_METATABLE_STATE,
	SCRIPT_STATELESS_ENVIRONMENT_ACCESS,
	SCRIPT_STATELESS_COROUTINE_CAPTURE,
	SCRIPT_STATELESS_ALIAS_ESCAPE,
	SCRIPT_STATELESS_DYNAMIC_ACCESS,
	SCRIPT_STATELESS_SHARED_INSTANCE_TABLE,
};

struct Zenith_ScriptStatelessVerdict
{
	Zenith_ScriptStatelessDiagnostic m_eDiagnostic = SCRIPT_STATELESS_OK;
	uint32 m_uLine = 0;
	uint32 m_uColumn = 0;
	char m_acDetail[256] = {};             // e.g. "upvalue 'counter' (declared at line 3) assigned in Each"
};

Zenith_ScriptStatelessVerdict Zenith_ScriptCheckStateless(
	const Zenith_ScriptModuleSource& xModule,          // source text and its resolved dependency list
	Zenith_ScriptModuleKind eKind,                     // system, behaviour or library
	const Zenith_ScriptAcceptedModuleSet& xAccepted,
	Zenith_ScriptCompileReport* pxReport = nullptr);   // one diagnostic per rejection found; the first in source order is the verdict
```

- [Z2-N-SCRIPT-356] ZenithCook runs `Zenith_ScriptCheckStateless` on every chunk-system module, every behaviour module and every library module they require, and a rejection fails the cook with the diagnostic, line and detail; every hot-reload producer (ZenithEditor's in-process compile, ZenithCook's file-watch re-cook) runs it again on changed source before any bytecode reaches `Zenith_ScriptWorld::RequestHotReload`, which refuses bytecode whose `.zlbc` flags lack the stateless-verified bit.
- [Z2-N-SCRIPT-357] A library module is accepted for use by systems only if it passes the check as a library; a system that requires a rejected library is rejected with `SCRIPT_STATELESS_IMPORTED_STATE` naming the library.

### Acceptance

- [Z2-N-SCRIPT-358] D-LUAU-STATELESS is accepted when every module of the adversarial corpus (`Scripting/StatelessCorpus.md` → "Adversarial corpus") is rejected at load time with its expected diagnostic, and every module of the accepted corpus (`Scripting/StatelessCorpus.md` → "Accepted corpus") loads; `Test_Script_Stateless_AdversarialCorpusRejectedAtLoad` and `Test_Script_Stateless_AcceptedCorpusLoads` assert both.

### Corpus structure

The adversarial corpus has at least one module per pattern below; the full corpus with file names, expected diagnostics and the accepted counterpart of each pattern is `Scripting/StatelessCorpus.md` → "Adversarial corpus".

| # | Pattern | What it exploits | Covers |
|---|---|---|---|
| 1 | module-scope `local n = 0` incremented in `Each` | an upvalue survives across invocations and differs per worker VM | captured locals |
| 2 | module-scope `local cache = {}` written in `Each` (`cache[k] = v`) | a table held in an upvalue is not frozen by `luaL_sandbox` | captured locals, nested tables |
| 3 | returned module table with a nested table `M.Tuning.Values` mutated | `table.freeze` is shallow | nested tables |
| 4 | global write `Counter = (Counter or 0) + 1` | the sandbox's per-thread proxy accepts it at runtime | globals |
| 5 | `require("@game/Util")` whose `Util.Cache` table is mutated by the system | state held in an imported module | imported modules |
| 6 | an imported `Util.Next()` that increments its own upvalue | the system writes no upvalue itself; the callee does | imported modules |
| 7 | closure factory `local inc = makeCounter()` at module scope | the counter lives in the factory's upvalue, reached indirectly | captured locals |
| 8 | module-scope `local scratch = buffer.create(64)` written with `buffer.writeu32` | a mutable userdata-like object held at module scope | mutable userdata |
| 9 | a module-scope binding userdata (`Zenith.Scratch.New()`) mutated in `Each` | engine-provided mutable userdata | mutable userdata |
| 10 | `math.random()` and `math.randomseed(ctx.Tick)` | hidden library generator state | stateful library functions |
| 11 | `os.clock()` used to vary behaviour | wall-clock time as an input | stateful library functions |
| 12 | `setmetatable(proxy, { __index = function(_, k) memo[k] = … end })` | mutation hidden in a metamethod | metatables |
| 13 | `setmetatable({}, { __mode = "k" })` at module scope | GC-dependent contents | metatables |
| 14 | `getfenv(1).x = 1` | environment smuggling | globals |
| 15 | `table.insert(cache, v)` on an upvalue table | mutation through a library call rather than an assignment | nested tables |
| 16 | `local t = cache; t[k] = v` | aliasing an upvalue through a local | aliasing |
| 17 | `helper(cache)` where `helper` mutates its parameter | inter-procedural mutation | aliasing |
| 18 | `cache[string.format("%d", i)] = v` / `rawget(M, name)` | dynamic keys defeat name-based checks | dynamic access |
| 19 | a module-scope coroutine resumed inside `Each` | state carried in a suspended coroutine frame | captured locals |
| 20 | `ctx:Emit(T.Events.X, sharedTable)` then mutating `sharedTable` afterwards | a module-scope table escaping into engine-held data | aliasing |
| 21 | a behaviour module storing one module-scope table into every instance's state | cross-instance mutable sharing | behaviour extension |

- [Z2-N-SCRIPT-359] The adversarial corpus holds at least one module for each of the 21 patterns in the table "Corpus structure", and the corpus grows (never shrinks) when a new bypass is found.

### Behaviour module extension

- [Z2-N-SCRIPT-360] Behaviours cannot share mutable tables: behaviour modules are checked by the same mechanism for module-scope state, and cross-instance data goes through components, events or entity handles.
- [Z2-N-SCRIPT-361] Behaviour per-instance data lives in the declared `State` table alone; module-scope values of a behaviour module are constants (literals, deep-frozen tables, functions and accepted `require` results).

## Behaviour state model and recovery

Script recovery uses explicit serialized behaviour state plus a restart entry point (Z2-D-023). A suspended execution position is not part of any serialized state.

### Declared state schema

`State` maps field names to field specs built by `Zenith.Field.*`:

| Spec | Stored as | Default | Serialized as (little-endian) |
|---|---|---|---|
| `Zenith.Field.Bool(bDefault)` | boolean | `false` | u8 |
| `Zenith.Field.I32(iDefault)` / `U32` / `I64` / `U64` | integer number (range-checked; 64-bit kinds limited to ±2⁵³) | 0 | i32 / u32 / i64 / u64 |
| `Zenith.Field.F32(fDefault)` / `F64` | number (F32 rounds on write) | 0 | f32 / f64 |
| `Zenith.Field.Vec2()` / `Vec3()` | `vector` | zero | 2 / 3 × f32 |
| `Zenith.Field.Quat()` / `Vec4()` / `Colour()` | Zenith value userdata | identity / zero / opaque black | 4 × f32 |
| `Zenith.Field.String(uMaxBytes, strDefault)` | string | `""` | u16 length + UTF-8 bytes |
| `Zenith.Field.Enum({ … }, strDefault)` | string-literal union | first value | u16 index into the declared list |
| `Zenith.Field.PersistentId()` | `Zenith.PersistentId` | nil | 16 bytes (zero = nil) |
| `Zenith.Field.Entity()` | `Zenith.Entity?` | nil | always 17 bytes: a u8 tag (0 nil, 1 persistent ID, 2 stable simulation ID, 3 provisional handle) then 16 bytes (the persistent ID, or the simulation ID or provisional index zero-padded), so a provisional fixup rewrites the field in place |
| `Zenith.Field.Asset(T.AssetType)` | `Zenith.Asset?` | nil | asset GUID (16 bytes) |
| `Zenith.Field.Table({ … })` | nested state table with its own declared fields | defaults of its fields | the nested fields in declaration order |
| `Zenith.Field.Array(spec, uMaxCount)` | array of `spec` | empty | u16 count + elements |
| `Zenith.Field.Map(keySpec, valueSpec, uMaxCount)` | map with string, enum or integer keys | empty | u16 count + (key, value) pairs sorted by key |

Each field carries a stable field ID: `Id = n` when given, otherwise `Hash64(field name)` truncated to 32 bits; a rename keeps its ID with `RenamedFrom = "OldName"`.

- [Z2-N-SCRIPT-381] Serializable behaviour state is only the behaviour's declared state table: reflected fields of the kinds in the table "Declared state schema" (primitives, strings, persistent IDs, entity and asset handles, and nested tables and bounded arrays and maps of the same).
- [Z2-N-SCRIPT-382] The state schema is registered with ZenithReflect as a runtime type `Behaviour.<Name>.State` of kind `TYPE_KIND_BEHAVIOUR_STATE` (`Origin(TYPE_ORIGIN_SCHEMA)`) with stable field IDs and the declared `StateVersion`, at startup step 10, before the registry freezes, from the state-schema records of the cooked script manifest `content:/Scripts/scripts.zlmf` (`Zenith_ScriptRegisterBehaviourStateTypes(Zenith_TypeRegistry&, const Zenith_ScriptModuleManifest&)`, in module-ID order, after the game's `schemas.zreg`; the live declaration's shape is compared with the record at world load, ZS1806 on a mismatch), and afterwards only inside the hot-reload window, so it is serialized, migrated and shown in the inspector like any reflected type (`Zenith2/Docs/Architecture.md` → "Reflection and runtime type registration").
- [Z2-N-SCRIPT-383] A field ID is never reused within a behaviour's schema history; reusing one is a load error.

### Load errors

- [Z2-N-SCRIPT-384] Declaring a state field whose default or spec is a closure, a coroutine, a userdata other than the Zenith handle and value types, or a table without a declared schema is a `SCRIPT_ERROR_KIND_LOAD` error naming the field; closures, coroutines and userdata other than handles are not serializable.
- [Z2-N-SCRIPT-385] A schema whose maximum serialized size (computed from the declared maxima) exceeds the checkpoint slot size row is a load error; the maximum is computed at load, so serialization never needs an unbounded buffer.

### State proxy and dirty tracking

- [Z2-N-SCRIPT-386] The `state` value passed to callbacks is a proxy whose `__newindex` validates every write against the declared schema (kind, range, maximum length or count) and raises the catalogued misuse error on violation, so an unserializable value can never be stored.
- [Z2-N-SCRIPT-387] Every accepted write sets the instance's dirty flag; the flag is cleared when a checkpoint commits.
- [Z2-N-SCRIPT-388] Assigning a table to a `Table`, `Array` or `Map` field copies its contents into the proxy's storage (value semantics), so no two instances ever alias one Luau table through state.

### Checkpoint rule

1. A behaviour instance's final invocation of the tick is known when it starts: in the Late scripts phase, it is the instance's last scheduled invocation in the node (the handler list is fixed at phase 9 delivery); in the Early scripts phase, it is the `OnUpdate` or `Run` segment of an instance that subscribes to no events.
2. At the end of its final invocation (its return, or its yield), if the dirty flag is set, the engine serializes the state into the instance's inactive checkpoint slot, inside the entity transaction, before commit, under `Zenith_ScriptProtectedCall` with `SCRIPT_CALL_KIND_NATIVE_BODY`.
<!-- elaboration -->
3. An Early instance with event subscriptions that received no event this tick, and whose state is dirty, gets a checkpoint transaction with no script code in its module's Late-phase node (see "Schedule compilation of script nodes"), at the instance's position in that node: an entity transaction that runs no script code and performs step 2 alone.
4. A serialization failure raises a `SCRIPT_ERROR_KIND_CHECKPOINT` Luau error in that transaction, so the entity's transaction rolls back.
5. Commit swaps the active slot index, an operation with no failure path.

- [Z2-N-SCRIPT-389] The checkpoint is serialized into reserved memory inside the transaction, before commit, only when the declared state is dirty, and at most once per tick per instance: at the instance's last invocation in the tick, which ends in a return or a yield.
- [Z2-N-SCRIPT-390] A checkpoint serialization failure is a Luau error, so the entity's transaction rolls back; commit then swaps the checkpoint in, which cannot fail.
- [Z2-N-SCRIPT-391] Each behaviour instance owns two checkpoint slots reserved at instance creation, each of the schema's maximum serialized size; the active slot always holds the last successfully committed state.

### Checkpoint format

| Offset | Bytes | Field |
|---|---|---|
| 0 | 4 | magic `ZBST` |
| 4 | 2 | checkpoint format version (1) |
| 6 | 2 | `StateVersion` of the schema that wrote it |
| 8 | 8 | stable source ID of the behaviour |
| 16 | 8 | tick of the committing invocation |
| 24 | 4 | payload bytes |
| 28 | 4 | fixup count (provisional handles awaiting playback) |
| 32 | … | payload: fields in declaration order, each prefixed by its u32 field ID |
| … | 8 × fixups | fixup list: payload offset and provisional handle index |

The payload encoding is independent of C++ layout (Z2-D-038); saves embed the active slot of every instance at the save-snapshot point (phase 11).

- [Z2-N-SCRIPT-392] A checkpoint records at most `uSCRIPT_MAX_CHECKPOINT_FIXUPS` (16) provisional handles; a state holding more provisional handles at the checkpoint point fails serialization with `SCRIPT_ERROR_KIND_CHECKPOINT`, as does a payload (declared fields plus preserved unknown fields) larger than the slot.

### Restore algorithm

1. Read the active checkpoint slot (quarantine recovery, hot reload) or the saved blob (save load).
2. Verify magic, format version and source ID; a mismatch is a restore failure that leaves the instance quarantined with the diagnostic `SCRIPT_STATE_RESTORE_FAILED`.
3. If `StateVersion` is older than the module's, run the `Migrate` chain one step per version (see "State migration chain").
4. Decode each field by field ID; unknown field IDs are preserved in the instance's unknown-field blob and re-emitted on the next checkpoint, with a warning, so state written by a newer module version survives a round trip (Z2-D-048).
5. Resolve references (see "Stale references").
6. Build a fresh state proxy and a fresh `self`; when the instance requires a recovery invocation, call `OnRecover(self, state, ctx)` (see "OnRecover").

- [Z2-N-SCRIPT-393] Restore never deserializes a Luau function, coroutine or execution position; the instance's coroutine, if any, is created fresh by `OnRecover`.

### OnRecover

- [Z2-N-SCRIPT-394] `OnRecover(self, state, ctx)` runs only for an instance whose behaviour declares it (exactly the behaviours that declare `Run`, [Z2-N-SCRIPT-141]), on the main VM in the instance's declared phase of the first tick after recovery, before any other callback of the instance in that tick, as its own entity transaction under the recovery safepoint row and the recovery ordinal of [Z2-N-SCRIPT-928].
- [Z2-N-SCRIPT-929] A last-good restore, a hot-reload go-live or a save restore returns an instance of a behaviour that does not declare `OnRecover` to ACTIVE (DORMANT while disabled) with no recovery invocation, so its first invocation after the restore is the one a run without the restore would make.
- [Z2-N-SCRIPT-395] `OnRecover` returns the function to start as the instance's new `Run` coroutine, or `nil` to start `Run` itself; the coroutine restarts its logic from the phase recorded in the declared state (Z2-D-023).
- [Z2-N-SCRIPT-396] A failure inside `OnRecover` rolls back its transaction and leaves the instance quarantined with its restored checkpoint intact.
- [Z2-N-SCRIPT-930] The save-state snapshot holds one entry per instance past NEW (an empty checkpoint for an instance with no committed checkpoint, restoring its declared defaults) with the flags `m_bRunSuspended`, `m_bRecoveryPending` and `m_bQuarantinedOrDisabled`; a save restore gives an entry a recovery invocation only when its behaviour declares `OnRecover` and one flag is set, and a restore of a snapshot with no suspended `Run` and no quarantined or disabled entry continues bit-identically to the uninterrupted run (`Zenith2/Docs/Design/SCRIPT/HotReload.md` → "Recovery invocations and exact restore").

### Stale references

- [Z2-N-SCRIPT-397] On restore, entity and asset references are resolved through persistent IDs (and, for entities without one, stable simulation IDs within the same world session); anything that cannot be resolved becomes `nil`, and a `SCRIPT_STATE_REFERENCE_UNRESOLVED` diagnostic naming the instance, the field and the unresolved ID is logged.

### No shared mutable tables

- [Z2-N-SCRIPT-398] No Luau table is ever shared mutably between behaviour instances: state has value semantics, `self` is not extensible, and module scope is constant (see "Behaviour module extension").

### State migration chain

- [Z2-N-SCRIPT-399] `Migrate[n]` receives a plain Luau table decoded from a version-n checkpoint and returns a table valid for version n+1; the chain runs every step from the checkpoint's version to the module's `StateVersion`, each under the recovery safepoint row, and the result is validated against the new schema before it is accepted.
- [Z2-N-SCRIPT-400] Every `Migrate` step has a migration fixture per version under the game's `Scripts/Tests/Migration/<Behaviour>/v<n>.zbst`, and the fixture test loads each fixture with the current module; the content cook skips everything below the `Tests/` directory of the `Scripts` content root, so the fixtures are never cooked or packaged (`Zenith2/Docs/Architecture/BuildSystem.md` [Z2-N-ARCH-246]).

## Quarantine and hot reload

### Quarantine semantics

When a behaviour instance's entity transaction fails:

1. The entity transaction is rolled back (see "Execution-time failure algorithm").
2. The instance enters `SCRIPT_INSTANCE_STATE_QUARANTINED`: its Luau heap objects (its `Run` coroutine thread, its `self` object, its state proxy, any registry references it owns) are released; its component data and its active checkpoint slot are untouched.
3. Its remaining invocations of the tick are skipped; its pending events for subsequent ticks are dropped with a diagnostic count.
4. Its behaviour module is disabled for the world from the next tick boundary (every other instance of the module finishes the current tick normally).

- [Z2-N-SCRIPT-421] A failing behaviour instance is quarantined: its Luau heap state is discarded, and it is restored on reload from its last successfully committed serialized state, using the same mechanism as hot-reload migration.
- [Z2-N-SCRIPT-422] A failing chunk system has no heap state to quarantine; it is disabled for the world from the next tick boundary until it is reloaded.

### Recovery policy

<!-- elaboration -->
```cpp
enum Zenith_ScriptRecoveryPolicy : uint8
{
	SCRIPT_RECOVERY_POLICY_MANUAL,     // stay disabled until a hot reload or an explicit reload command (development default)
	SCRIPT_RECOVERY_POLICY_AUTOMATIC,  // reload the module automatically after uRecoveryDelayTicks, at most uMaxRecoveriesPerModule times per session (shipping default)
};
```

- [Z2-N-SCRIPT-423] The recovery policy is set per game in `game.json` (`"scriptRecovery": { "policy": "automatic", "delayTicks": 60, "maxRecoveriesPerModule": 3 }`; the `game.json` schema is `Zenith2/Docs/Architecture.md` → "Engine modules and game projects"); the automatic policy reloads a disabled module at a tick boundary with the last-good restore below, and after `maxRecoveriesPerModule` recoveries of one module in one session the module stays disabled for the session with a diagnostic.

### Last-good restore

1. At a tick boundary, for each instance of the reloaded module: take its active checkpoint slot.
2. Restore state by the restore algorithm into the instance's C++ state (live-state block and checkpoint slots); the tick-boundary node touches no Luau object, so the old instance's Luau objects are released, and its fresh `self` and state proxy created from the module, by the module's next behaviour node on the main thread, before that node calls `OnRecover`.
3. Re-enable the module from that tick.

- [Z2-N-SCRIPT-424] Last-good restore uses the instance's active checkpoint slot, which by the checkpoint rule is its last successfully committed state; state from a failed or rejected transaction never reaches it.

### Hot reload of behaviour modules

A source change detected by the editor's file watcher (or a reload command) triggers, in the next frame prologue (the main-thread loop steps before `RunFrame` that precede the frame's first tick boundary, where the type registry's hot-reload window opens and world swaps happen):

1. The bytecode is produced on the host: ZenithEditor compiles the new source in process through ZenithScriptCompiler (`Zenith_ScriptCompileModule`, then `Zenith_ScriptCheckStateless`) for play-in-editor worlds, and ZenithCook's file-watch mode re-cooks the `.zlbc` that a standalone non-`Shipping` Player (Windows or the S24) picks up from its content path; `Zenith_ScriptWorld::RequestHotReload` receives the bytecode with the module's load-time name references and queues them; ZenithScript opens no registry window itself.
2. In frame-prologue step 6, ZenithRuntime opens the registry window (`Zenith_TypeRegistry::BeginHotReload()`) and calls `Zenith_ScriptRuntime::ApplyPendingReloads(xRegistry)` (`Scripting/PublicTypes.md` → "Public types"), which loads each queued module into a staging generation on its world's main VM, validates its declaration, and registers its `Behaviour.<Name>.State` type (`TYPE_KIND_BEHAVIOUR_STATE`) in the open window. For every live instance: take its active checkpoint (a fresh checkpoint is serialized first for a non-quarantined instance whose state is dirty), migrate it through the new module's `Migrate` chain when `StateVersion` increased, and restore it into staging objects built from the new module.
3. If every step succeeded for every instance, ZenithRuntime closes the window with `EndHotReload()`, which validates and publishes the new registry revision, and then, still in prologue step 6, calls `Zenith_ScriptRuntime::CommitPendingReloads()` (`Scripting/PublicTypes.md` → "Public types"), which commits the changed declarations through ECS's `Zenith_SystemReloadScope`, so the schedule recompiles in prologue step 8 of the same frame when an access set, the phase, the ordering or the system set changed. At the frame's first tick boundary the staged generations are swapped in, and nothing else happens there: the new module generation becomes current, and the old instances' Luau objects are queued for release; the module's next behaviour node, on the main thread, releases them and creates the fresh `self` and state proxies before `OnRecover` runs for every instance in that tick.
4. If any step failed for any instance, `ApplyPendingReloads` returns the error and ZenithRuntime calls `AbandonHotReload()` (so the previous registry revision stays in effect); the same rollback follows when `EndHotReload()` fails validation: release every staging object, keep the old module generation and every instance's old heap state unchanged, and report the first error with its instance.

- [Z2-N-SCRIPT-425] Luau hot reload migrates state, with rollback on error: a reload either succeeds for every instance of the module or leaves the old module and every instance exactly as it was.
- [Z2-N-SCRIPT-426] A hot reload is applied at a tick boundary and never mid-tick.
- [Z2-N-SCRIPT-430] During play, a hot reload changes only `TYPE_KIND_BEHAVIOUR_STATE` types, inside the registry's hot-reload window in the frame prologue; a data-defined component schema changes only in the editor between play sessions, never in a running play world (`Zenith2/Docs/Architecture.md` → "Reflection and runtime type registration").
- [Z2-N-SCRIPT-427] Hot-reload latency from save to running code is measured against the script hot-reload row of the W-Editor targets.
- [Z2-N-SCRIPT-927] Every hot-reload request (`Zenith_ScriptWorld::RequestHotReload`, `Zenith_ScriptRuntime::RequestCookedReload`) carries the reloaded module's load-time name references as `Zenith_Span<const Zenith_ScriptNameRefRecord>` (`Scripting/PublicTypes.md` → "ZenithScript public types"; default empty, for a module with no marked argument), supplied by the editor from the `Zenith_ScriptExtractNameRefs` run it already makes and, for `RequestCookedReload`, by the development Player's file watcher from `Zenith_ScriptModuleManifest::GetNameRefs` of the re-cooked `content:/Scripts/scripts.zlmf`, which the cook's watch mode writes after the changed `.zlbc` files; the staging resolves them as the loader does at world load ([Z2-N-SCRIPT-922]) before go-live, and an unresolved reference fails that module's reload with ZS1807 and the rollback of [Z2-N-SCRIPT-425] or [Z2-N-SCRIPT-428], never degrading to the call-time ZS1511.

### Hot reload of chunk-system modules

1. Compile, check and validate the new module on the main VM as in step 1 above.
2. In prologue step 6, `Zenith_ScriptRuntime::CommitPendingReloads()` commits a changed declaration through `Zenith_SystemReloadScope`, so the schedule recompiles in prologue step 8 of the same frame when the access sets changed.
3. At the frame's first tick boundary, advance the world's module generation for the module; each worker VM reloads it before its next invocation of the module (see "Lazy worker VM creation").

- [Z2-N-SCRIPT-428] A chunk-system reload that fails leaves the previous module generation in use on every VM.

### Disable-from-next-tick rule

- [Z2-N-SCRIPT-429] Every failure reported during a tick (execution-time failure, checkpoint failure, `OnRecover` failure) disables its module from the next tick boundary; a reload in the same tick boundary re-enables it only if the reload succeeds.

## Views epochs and misuse errors

### View identity

Three independent counters scope what a script may touch (`Zenith2/Docs/Architecture.md` → "ECS model"):

| Counter | Scope | Changes when | Compared by |
|---|---|---|---|
| invocation ID | one invocation | every invocation start (per-VM counter, VM index in the top 16 bits) | every view and context access |
| structural epoch | one chunk | structural commands are played back at a sync point | every view access |
| change-tracking version | one field column | an accepted write is committed at a merge point | change queries; never by view validation |

```cpp
// the payload of every view userdata (row proxy, component view, container object, ctx)
struct Zenith_ScriptViewHeader
{
	Zenith_ScriptInvocationID m_xInvocationID;   // the invocation that created the view
	Zenith_ChunkView m_xChunk;                   // the invocation's chunk view; empty for ctx and container objects of behaviours
	uint32 m_uStructuralEpoch = 0;              // the chunk's epoch when the view was created
	uint32 m_uSegment = 0;                      // the behaviour coroutine segment that created it
	Zenith_ScriptViewKind m_eKind;               // ROW, COMPONENT, CONTAINER, CONTEXT, STATE
};
```

- [Z2-N-SCRIPT-441] A view (row proxy, component view, container object, invocation context) is valid only during the invocation that created it; each view is scoped to its invocation ID and becomes invalid when that invocation ends.
- [Z2-N-SCRIPT-442] Views never survive a yield: a behaviour view records its coroutine segment and is invalid in every subsequent segment.
- [Z2-N-SCRIPT-443] Retained or stale views raise a Luau error in all builds, Debug, Release and shipping alike, detected by comparing the view's invocation ID and segment with the VM's current ones and its structural epoch with the chunk's.
- [Z2-N-SCRIPT-444] Strings (interned IDs), entity handles and asset handles are read-only in chunk views and changed through commands; container fields are not exposed in chunk views.

### Validation on every access

1. For a behaviour view created by the running instance's own coroutine, `m_uSegment == instance.m_uCurrentSegment`, else raise ZS1002 (view used after a yield).
2. `m_xInvocationID == xVM.m_xCurrentInvocationID`, else raise ZS1001 (view used outside its invocation).
3. For chunk-bound views, `m_xChunk.IsValid(Zenith_InvocationScope{ xVM.m_xCurrentInvocationID.m_ulValue })` (the ECS view's invocation-ID and structural-epoch compare, `Zenith2/Docs/Architecture.md` → "ECS model"); step 2 has already matched the invocation, so a false result is an epoch change: raise ZS1003 (structure changed).
4. For the handle's world, `handle.world == xVM.world`, else raise ZS1102.

The cost is two or three integer compares per access; there is no build in which it is compiled out.

### Misuse error format

- [Z2-N-SCRIPT-445] Every script misuse raises a controlled Luau error of kind `SCRIPT_ERROR_KIND_MISUSE` (or the abort kind named in the catalogue) and never an assert, a crash or undefined behaviour (Z2-D-045).
- [Z2-N-SCRIPT-446] A misuse error's message is exactly `ZS<code>: <text>` with the text from `Scripting/MisuseCatalogue.md` → "Catalogue", after Luau's usual `<chunkname>:<line>: ` position prefix; placeholders in angle brackets are substituted and nothing else varies.
- [Z2-N-SCRIPT-447] Catalogue codes are stable: a code is never reused for a different misuse, and a changed message text keeps its code.

### Misuse catalogue summary

The complete catalogue, with every code, kind, trigger and exact text, is `Scripting/MisuseCatalogue.md` → "Catalogue". Codes are grouped by range: ZS10xx views and context, ZS11xx handles and tickets, ZS12xx field access, ZS13xx coroutines, ZS14xx budgets and fatal events, ZS15xx value marshalling, ZS16xx sandbox, ZS17xx bindings and contexts, ZS18xx modules, ZS19xx behaviour state. The view errors of this section are ZS1001 (outside its invocation), ZS1002 (after a yield) and ZS1003 (structure changed).

## Debugging and profiling

### DAP server design

The debugger is a Debug Adapter Protocol server inside `ZenithScript/Internal/Debug/`, built over Luau's debug hooks.

| Luau facility | Used for |
|---|---|
| `lua_callbacks(L)->debugbreak` | a breakpoint hit: stop the VM's thread and report a `stopped` event (reason `breakpoint`) |
| `lua_callbacks(L)->debugstep` with `lua_singlestep(L, 1)` | step in / step over / step out, implemented by comparing call depth (`lua_stackdepth`) at each step callback |
| `lua_callbacks(L)->debuginterrupt` and the interrupt callback | pause requests: the interrupt callback enters the stop loop at the next safepoint |
| `lua_breakpoint(L, iFuncIndex, iLine, bEnabled)` | setting and clearing line breakpoints on each loaded function prototype |
| `lua_getinfo`, `lua_getlocal`, `lua_getupvalue`, `lua_getargument` | stack traces, scopes and variables |
| `lua_debugtrace` | tracebacks for exceptions and `error` breakpoints |

Server model:

1. The Player (every configuration other than `Shipping`) and the editor create one `Zenith_ScriptDap` per process, owned by the composition root, listening on `127.0.0.1` at the port from the `--script-dap-port` argument (default 27851), bound to the loopback address.
2. Each VM of each world is a DAP thread: thread ID = (world tag × 256) + VM index, the world tag being the one world identifier every SCRIPT and ECS interface carries; the main VM is named `<World>/main`, worker VMs `<World>/worker<N>`.
3. Breakpoint sets are fanned out to every VM of the world (see "DAP fan-out").
4. When a VM stops, its thread blocks inside the debug callback, on its stop slot's `Zenith_ConditionVariableNoProfiling` paired with the DAP mutex (a `Zenith_MutexNoProfiling` at `uLOCK_LEVEL_SCRIPT_DAP`), inside the debugger-stop bracket; every other script node body of the world parks at the world's debugger gate before its next invocation (invocations already running finish, or stop at their own breakpoint), merge, tick-boundary and engine nodes do not park, and the watchdog skips the stopped VM alone (see "Wall-clock watchdog").
5. Variables are read while the VM is stopped, on the stopped thread, by marshalling DAP requests to it through the stop loop.
6. `continue` releases every stopped VM of the world and every parked node body.

<!-- elaboration -->
- [Z2-N-SCRIPT-469] A debugger stop blocks a thread inside a job body only under the development-build exemption of `Zenith2/Docs/Contracts.md` → "Threading and concurrency contract" ([Z2-N-CONTRACT-365]): the stopping or parking thread reads `Zenith_IsJobWorkerThread()`, calls `Zenith_MarkThreadAsJobWorker(false)`, acquires `uLOCK_LEVEL_SCRIPT_DAP` only inside that bracket, and once released reports its wait once as `"debugger-stop"` through `Zenith_DispatchBlockingWait`, before it restores the flag it read; the protocol, its deadlock-freedom argument and its wake-up rules are `Zenith2/Docs/Design/SCRIPT/Debugger.md` → "Stop protocol", and no Shipping build contains the bracket ([Z2-N-SCRIPT-461]).

- [Z2-N-SCRIPT-461] The DAP server is constructed only in configurations other than `Shipping` and `ShippingTest` (`if constexpr (!Zenith_IsShippingBuild())` in the composition root) and binds the loopback interface alone; libgraph's link-surface check asserts that no `Zenith_ScriptDap` symbol is present in a game's Shipping link closure.
<!-- elaboration -->
- [Z2-N-SCRIPT-468] The configuration test is `Zenith_IsShippingBuild()` from `ZenithCore/Zenith_BuildInfo.h`, the build system's `constexpr` reader of the configuration defines: `true` in the `Shipping` configuration (build presets `win-shipping`, `android-arm64-shipping`) and in the `ShippingTest` configuration that builds test targets with Shipping's code (build and test preset `win-shipping-test`, `Zenith2/Docs/Architecture/BuildSystem.md` [Z2-N-ARCH-231]), `false` in every other configuration and preset (`Zenith2/Docs/Architecture.md` → "Build system and presets"); it changes no layout (`Zenith2/Docs/Contracts.md` → "No layout-changing conditionals").
- [Z2-N-SCRIPT-462] A debugger stop suspends the world's script invocations (the stopped VM, and every other script node body at the world's debugger gate) and exempts the stopped VM from the watchdog, and never changes a safepoint count or any committed value, so a replay debugged step by step produces the same world-state hashes as an undebugged run.

### Decision D-LUAUDBG

D-LUAUDBG (open, Stage 1) chooses the debugger client. The server above is client-neutral DAP; the decision chooses what the human attaches with.

| Candidate | Notes the ruling weighs |
|---|---|
| VS Code with a Zenith DAP extension (thin launch/attach configuration) | a standard DAP client; Luau syntax and types through luau-lsp with the generated `.d.luau` |
| an existing Luau debugger extension speaking DAP | reuse; compatibility with Zenith's multi-VM thread model |
| a debugger panel inside ZenithEditor (ImGui) | integrated with play-in-editor and entity selection; more code to own |

- [Z2-N-SCRIPT-463] The D-LUAUDBG ruling is appended to `Zenith2/Docs/DecisionLog.md` with an explicit `Z2-D-NNN` in the ruling ticket's own diff and records the selected client, the supported DAP requests (at minimum `setBreakpoints`, `configurationDone`, `threads`, `stackTrace`, `scopes`, `variables`, `continue`, `next`, `stepIn`, `stepOut`, `pause`, `evaluate` restricted to reading locals), and the attach workflow on Windows and the S24 (over `adb forward tcp:27851 tcp:27851`).

### Profiling zones

- [Z2-N-SCRIPT-464] Every script node emits a CPU frame-graph zone, and every invocation emits a nested zone named `<Module>` with the entity count (chunk systems) or the stable simulation ID (behaviours), feeding the on-screen profiler and Tracy (Z2-D-005).
- [Z2-N-SCRIPT-465] Merge points, checkpoint serialization, restores, hot reloads and GC steps emit their own zones (`Script.Merge`, `Script.Checkpoint`, `Script.Restore`, `Script.HotReload`, `Script.GC`).

### On-screen profiler rows

| Row | Content | Source |
|---|---|---|
| Luau time | per tick: main-VM time (behaviours), worker-VM time summed and per worker (chunk systems), merge time; each as ms and against the script CPU time row | invocation and merge zones |
| Luau GC | per VM: heap bytes (`m_ulBytesInUse`) against its cap, GC step time this tick, KB stepped against the script GC step row, VM count | `Zenith_ScriptAlloc` counters, GC zones |
| Script counters | invocations, entity transactions, failures by kind, merge rejections by class, quarantined instances, disabled modules, checkpoints and bytes, snapshot bytes against the per-tick snapshot total row, safepoints against the per-tick row | transaction and merge counters |

- [Z2-N-SCRIPT-466] The on-screen profiler shows the Luau time, Luau GC and script counter rows in the game and the editor on Windows and the S24, behind the runtime toggle, as `Zenith2/Docs/Architecture.md` → "CPU frame graph and scheduler" specifies for every node.

### Sampling profiler

- [Z2-N-SCRIPT-467] In development builds, a script sampling profiler can be enabled at runtime: every 997th safepoint of a VM records the Luau call stack (`lua_getinfo` up to 32 frames) into a per-VM ring of 65,536 samples, dumped as a flame-graph JSON on request; sampling never changes safepoint counts or results.

## Sandboxing and security

### Trust boundary

<!-- informative -->
Scripts are game content: written by the game's developers, cooked into the game's package, and trusted to express gameplay. The engine nonetheless treats them as untrusted code with respect to its own integrity: a script bug, however severe, costs at most its own module (disabled, recovered) and never the engine, the process, other worlds, the filesystem or the user's machine. Modding is excluded (Z2-D-075), so no third-party script ever runs in a shipping game; the boundary still matters because saves, replays and content updates are external inputs that scripts consume.
<!-- /informative -->

- [Z2-N-SCRIPT-481] The script trust boundary is recorded in `Zenith2/Docs/Architecture.md` → "Security architecture" alongside the asset, remote-tool and update boundaries; this section is the script side of it.

### What a script can never reach

- [Z2-N-SCRIPT-482] A script can never reach the filesystem, processes, environment variables, sockets, dynamic libraries, raw memory addresses, another world's data, another VM's heap, the engine's `Internal/` state, or the debugger; no binding exposes any of them, and the environments contain no `io`, `os`, `package`, `load`, `loadstring`, `dofile`, `loadfile`, `getfenv`, `setfenv` or `debug` beyond `debug.traceback` and `debug.info`.
- [Z2-N-SCRIPT-483] A script can never load code: bytecode reaches a VM only through the module loader from the cooked package (shipping) or through `Zenith_ScriptCompileModule` from the game's `Scripts/` tree (development), and there is no binding that compiles or loads a string.
- [Z2-N-SCRIPT-484] A script can never exhaust the process: safepoint budgets, the per-entity snapshot cap, the VM memory caps and the watchdog bound its CPU and memory, and the protected-call depth limit bounds its C stack.

### Untrusted inputs

- [Z2-N-SCRIPT-485] Behaviour checkpoints read from saves and replays are untrusted input: the decoder bounds-checks every length and count against the declared schema before reading, rejects a mismatched magic, version or source ID, and never allocates more than the checkpoint slot size; it is fuzzed by `Fuzz_Script_CheckpointDecode` under `win-asan` with libFuzzer.
- [Z2-N-SCRIPT-486] Luau bytecode is not verified by Luau's loader, so bytecode is accepted only from package content whose integrity the asset system has verified (the cooked-content hashes and, for updates, the authenticated update manifest); saves and replays never contain bytecode.
- [Z2-N-SCRIPT-487] Hot-reload and cook-time compilation of source enforce parser limits: at most 1 MiB of source per module, 200 levels of syntactic nesting (Luau's `LUAI_MAXCCALLS`-equivalent recursion limit), and 8,192 modules per game; exceeding one is a `SCRIPT_ERROR_KIND_COMPILE` error.

### Shipping exposure

- [Z2-N-SCRIPT-488] Shipping builds contain no DAP server, no script console, no sampling profiler endpoint and no source compiler: no Player links `ZenithScriptCompiler`, and the `Libgraph` step asserts that a game's Shipping link closure contains neither that target, `Luau.Compiler`, `Luau.Ast` nor any `Zenith_ScriptDap` symbol.

## Shipping bytecode compatibility

### Pinning

- [Z2-N-SCRIPT-501] The Luau revision is pinned in `ThirdParty/manifest.json` by commit hash and release tag, together with its recorded API linkage, its native policy and the bytecode version it emits (`LBC_VERSION_TARGET`) and accepts (`LBC_VERSION_MIN`..`LBC_VERSION_MAX`).
- [Z2-N-SCRIPT-502] A Luau pin change is a dependency upgrade (Z2-D-082) that re-runs the linkage test, the native parity test, the D-LUAU-STATELESS corpus and the determinism matrix before it lands.

### Cooked bytecode format

ZenithCook writes one `.zlbc` file per module:

| Offset | Bytes | Field |
|---|---|---|
| 0 | 4 | magic `ZLBC` |
| 4 | 2 | container version (1) |
| 6 | 1 | Luau bytecode version byte (first byte of the Luau blob) |
| 7 | 1 | flags: bit 0 native requested (`--!native`), bit 1 simulation module, bit 2 debug line info present, bit 3 stateless check passed |
| 8 | 8 | Luau pin hash (first 8 bytes of the pinned commit hash) |
| 16 | 8 | engine version hash |
| 24 | 8 | binding-set hash, computed by `Zenith_ScriptBindingSetHash` as `Zenith_Hash64` over, in this order, little-endian with each string as u32 length + UTF-8 bytes: (1) every `Zenith_ScriptFunctionDesc` sorted by `m_xID` (ID, namespace, Luau name, each parameter's and result's type ID and kind, contexts, effect, side-effect mask, resource IDs); (2) every registered component and event type sorted by `Zenith_TypeID` (type ID, schema version, then each property ID and kind sorted by property ID); (3) the mutable-globals list in list order; (4) the compile options (optimization, debug and type-info levels) |
| 32 | 8 | source hash (`Hash64` of the UTF-8 source) |
| 40 | 4 | module ID (index in the game's module manifest) |
| 44 | 4 | Luau blob bytes |
| 48 | … | the Luau bytecode blob produced with optimization level 2, debug level 1 (line info), type-info level 1 |

- [Z2-N-SCRIPT-503] Cooked scripts are versioned binary data independent of C++ layout (Z2-D-038), compiled with the options in the table "Cooked bytecode format" for every configuration, so the safepoint counts measured in development equal those in shipping.

### Load-time verification

- [Z2-N-SCRIPT-504] The runtime refuses a `.zlbc` whose magic, container version, Luau pin hash, engine version hash or binding-set hash differs from the running build's, or whose Luau bytecode version lies outside the pinned accepted range, with a `SCRIPT_ERROR_KIND_LOAD` error naming the module and the mismatched field; it never attempts to run mismatched bytecode.

### Cook-time recompile rule

- [Z2-N-SCRIPT-505] Script bytecode is re-cooked on any engine version change and on any Luau pin change, as every cooked asset is (Z2-D-048); there is no runtime translation of older bytecode and no compatibility shim.
- [Z2-N-SCRIPT-506] Saves never contain bytecode or execution positions, so save migration depends on behaviour state schemas and their `Migrate` chains alone, never on bytecode compatibility.
- [Z2-N-SCRIPT-507] A content patch ships re-cooked bytecode for every changed module within one release line; a patch whose bytecode's engine version hash differs from the installed engine is refused by the loader.

### Development compilation

- [Z2-N-SCRIPT-508] Every hot-reload producer (ZenithEditor in process, ZenithCook's file-watch mode) compiles with the same `Zenith_ScriptCompileOptions` as the cook, including the mutable-globals list, so a hot-reloaded module and its cooked form produce identical bytecode.

```cpp
// Zenith2/Engine/ZenithScript/Zenith_ScriptCompileOptions.h (public, Luau-free; the sandbox owns the mutable-globals list)
#pragma once
#include "ZenithReflect/Zenith_ScriptBinding.h"
#include "ZenithReflect/Zenith_TypeRegistry.h"

struct Zenith_ScriptCompileOptions
{
	uint8 m_uOptimizationLevel = 2;
	uint8 m_uDebugLevel = 1;
	uint8 m_uTypeInfoLevel = 1;
	const char* const* m_aszMutableGlobals = nullptr;   // NULL-terminated; Zenith_ScriptSandboxMutableGlobals() in production
};

const char* const* Zenith_ScriptSandboxMutableGlobals();   // the names of "Builtin fast path and mutable globals"
Zenith_ScriptCompileOptions Zenith_ScriptProductionCompileOptions();

// The binding-set hash of the table "Cooked bytecode format", computed once by this function for both the .zlbc header and the .d.luau header.
uint64 Zenith_ScriptBindingSetHash(const Zenith_ScriptBindingTable& xBindings, const Zenith_TypeRegistry& xTypes,
	const Zenith_ScriptCompileOptions& xOptions);
```

```cpp
// Zenith2/Engine/ZenithScriptCompiler/Zenith_ScriptCompile.h (public; linked by ZenithCook and ZenithEditor)
#pragma once
#include "ZenithScript/Zenith_ScriptCompileOptions.h"
#include "ZenithScript/Zenith_ScriptError.h"
#include "ZenithScript/Zenith_ScriptTypes.h"

Zenith_Result<Zenith_ScriptBytecode, Zenith_ScriptError> Zenith_ScriptCompileModule(
	const Zenith_ScriptModuleSource& xSource,
	const Zenith_ScriptCompileOptions& xOptions,
	Zenith_Arena& xOutputArena,
	Zenith_ScriptCompileReport* pxReport = nullptr);   // parse and compile errors become SCRIPT_ERROR_COMPILE; every diagnostic also goes to pxReport

struct Zenith_ScriptFastcallSite
{
	char m_acBuiltinName[32] = {};                 // e.g. "tostring", "math.floor"; from the pinned LuauBuiltinFunction table
	uint32 m_uFunctionIndex = 0;                  // function prototype index in the bytecode
	uint32 m_uInstructionIndex = 0;               // instruction index within that function
};

// Decodes LOP_FASTCALL, FASTCALL1, FASTCALL2, FASTCALL2K and FASTCALL3 (implemented under Internal/, the one reader of Luau/Bytecode.h).
// The blob reader mirrors the proto format of the pinned Luau/VM/src/lvmload.cpp; the builtin-name table is the hand-maintained
// Internal/Zenith_ScriptBuiltinNames.inl, one entry per enumerator of the pinned LuauBuiltinFunction, re-baselined on every pin change;
// fills axSites (a caller-owned plain array) and returns the site count, or SCRIPT_ERROR_KIND_LOAD when uCapacity is too small or the blob is malformed.
Zenith_Result<uint32, Zenith_ScriptError> Zenith_ScriptListFastcallSites(const Zenith_ScriptBytecode& xBytecode,
	Zenith_ScriptFastcallSite* axSites, uint32 uCapacity);
```

## Required conformance tests

- [Z2-N-SCRIPT-523] Every test of this section is a doctest case under `Zenith2/Engine/ZenithScript/Tests/` (or, where marked, in the script conformance executable `ZenithScript_Conformance`, which links all three ECS backends; tests of `ZenithScriptCompiler` under `Zenith2/Engine/ZenithScriptCompiler/Tests/`), registered with CTest under the label `script-conformance` and run by the `Test` step, with its scenario scripts in `Zenith2/Engine/ZenithScript/Tests/Scenarios/<TestName>/`.

- [Z2-N-SCRIPT-521] Every test in the tables of this section, which live in `Scripting/ConformanceTests.md` → "Conformance test tables" under the five headings below, exists under exactly the name shown, asserts what its row states, and is added in the same ticket as the code it tests.
- [Z2-N-SCRIPT-522] Misuse tests assert the controlled Luau error and its exact catalogue text, and fail if any assert, crash or sanitizer report occurs.

### Transaction tests

The table "Transaction tests" is `Scripting/ConformanceTests.md` → "Conformance test tables" (heading "Transaction tests").

### Merge, capacity and determinism tests

The table "Merge, capacity and determinism tests" is `Scripting/ConformanceTests.md` → "Conformance test tables" (heading "Merge, capacity and determinism tests").

### VM, firewall and budget tests

The table "VM, firewall and budget tests" is `Scripting/ConformanceTests.md` → "Conformance test tables" (heading "VM, firewall and budget tests").

### State, recovery and reload tests

The table "State, recovery and reload tests" is `Scripting/ConformanceTests.md` → "Conformance test tables" (heading "State, recovery and reload tests").

### Module, binding, sandbox and tooling tests

The table "Module, binding, sandbox and tooling tests" is `Scripting/ConformanceTests.md` → "Conformance test tables" (heading "Module, binding, sandbox and tooling tests").

## Fault-matrix tests

### Fault kinds

| Fault | Injected by | Expected entity outcome |
|---|---|---|
| script error | the scenario's `FaultPlan` component: `Each` or a behaviour callback calls `error("fault")` at the planned tick | rollback, module disabled from the next tick |
| instruction budget exhausted | `FaultPlan` makes the invocation spin in a `while true do end` loop | ZS1401, rollback, module disabled |
| per-entity snapshot cap exceeded | `FaultPlan` makes a behaviour grow a container shadow, or a system emit payload, past the cap | ZS1402, rollback, module disabled |
| merge-time capacity rejection | `FaultPlan` makes entities emit more events and spawns than the phase's remaining capacity | rejection in merge order, no disable |

- [Z2-N-SCRIPT-544] Faults are injected by the scenario's own scripts reading a data-defined `FaultPlan` component (`Tick`, `Kind`, `Magnitude`); no production code path carries a test hook.

### Configuration axes

| Axis | Values |
|---|---|
| worker count | 1, 2, 8, all hardware threads |
| compaction | none; forced full compaction at every sync point |
| chunk capacity | two capacities: the backend default and the alternate capacity of "Determinism test matrix" |
| ECS backend | Custom, flecs, EnTT |

- [Z2-N-SCRIPT-545] Each fault kind runs the 16 configurations per backend, 48 runs in all per fault kind, plus a combined run in which all four kinds occur in the same ticks.

### Scenario

`Zenith2/Engine/ZenithScript/Tests/Scenarios/FaultArena/`: 5,000 entities over 6 archetypes; 4 chunk systems (2 Early, 2 Late) and 2 behaviour modules (one Early coroutine behaviour, one Late event-driven behaviour) on 400 of the entities; 300 ticks; spawns and destroys every tick so compaction changes layouts; faults planned at ticks 20, 80, 150 and 220 on entities spread over every archetype; automatic recovery policy with a 30-tick delay so recovery and `OnRecover` are exercised inside the run.

- [Z2-N-SCRIPT-546] The FaultArena scenario has exactly the parameters of the paragraph above; a change to any of them is a replay-format change that re-records every fault-matrix reference.

### Assertions

- [Z2-N-SCRIPT-541] Replays with injected failures (script error, instruction budget exhausted, per-entity snapshot cap exceeded, merge-time capacity rejection) run under 1, 2, 8 and all workers, with forced compaction and two chunk capacities, on each backend, and give bit-identical per-tick world-state hashes per backend.
- [Z2-N-SCRIPT-542] In every fault-matrix run, the set of failed entities, the set of rejected entities, the diagnostics' (tick, module, stable simulation ID, code) tuples, the disabled-module set per tick and the recovery ticks are identical across all configurations of the backend.

### Test list

| Test (conformance executable) | Fault kinds | Runs |
|---|---|---|
| `Test_Script_Fault_ScriptError` | script error | 16 per backend |
| `Test_Script_Fault_BudgetExhausted` | instruction budget exhausted | 16 per backend |
| `Test_Script_Fault_SnapshotCapExceeded` | per-entity snapshot cap exceeded | 16 per backend |
| `Test_Script_Fault_MergeCapacityRejection` | merge-time capacity rejection | 16 per backend |
| `Test_Script_Fault_Combined` | all four in the same ticks | 16 per backend |

- [Z2-N-SCRIPT-543] The five tests in the table "Test list" are registered with CTest under the labels `script-conformance` and `script-fault-matrix`, run in the `ZenithScript_Conformance` executable that links all three ECS backends, and each finishes within the per-test timeout of the Test step on the reference desktop.
