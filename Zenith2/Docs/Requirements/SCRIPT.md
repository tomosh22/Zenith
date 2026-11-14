# SCRIPT requirement catalogue

The requirement catalogue of the SCRIPT area (Scripting). Every requirement is one testable obligation with the inventory items it covers, its stage, classification, governing decisions, budget rows, acceptance evidence and the design headings that specify it. The design is `Zenith2/Docs/Design/SCRIPT.md` and its split files under `Zenith2/Docs/Design/SCRIPT/`; the specification is `Zenith2/Docs/Scripting.md` and its companions.

## Purpose

SCRIPT owns the Luau runtime `ZenithScript` and the host-only compiler `ZenithScriptCompiler`: the exception firewall, per-world VMs, compilation and cooked bytecode, module loading and sandboxing, the binding ABI and generated `.d.luau`, behaviours and chunk systems, per-invocation transactions with merge-time capacity, deterministic ordering and randomness, safepoint, GC and memory budgets, stateless-system enforcement, the behaviour state model with checkpoints and `OnRecover`, quarantine, recovery, hot reload and migration, asynchronous-result publication, the DAP debugger, script profiling and shipping bytecode compatibility. This catalogue is authoritative for coverage (Z2-D-066, Z2-D-067): every inventory item `Z2-I-SCRIPT-01` … `Z2-I-SCRIPT-23` is covered by at least one requirement below.

Evidence phrases: "the `Test` step (label `script-conformance`)" is the base Test step with every SCRIPT CTest case registered under that label; "the `Zenith2Memory` lines" are the category's `Test -Label memory` line and its `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan` lines (from Stage 2). Every named test is specified in `Zenith2/Docs/Design/SCRIPT/Tests.md`.

## Requirements

### Z2-R-SCRIPT-001 ZenithScript library target

- **Statement:** ZenithScript is one static library at `Zenith2/Engine/ZenithScript/` whose CMakeLists.txt is exactly the call of the design (DEPENDS ZenithCore ZenithReflect ZenithECS ZenithScene ZenithAssets ZenithJobs ZenithPlatform ZenithProfiling, THIRD_PARTY Luau, EXCEPTIONS INTERNAL, TEST_DEPENDS the three ECS backends) with the default ZENITH_ALL matrix, owning `Luau.VM` and `Luau.CodeGen`.
- **Covers:** Z2-I-SCRIPT-22, Z2-I-SCRIPT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-017, Z2-D-001, Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** the `Configure`, `Build`, `Libgraph` (no used-but-undeclared edge, no cycle, third-party ownership) and `CompileCoverage` steps on every preset including linux-tsan and android-arm64
- **Design:** Zenith2/Docs/Design/SCRIPT.md → "ZenithScript target"; Zenith2/Docs/Design/SCRIPT.md → "Library dependencies"; Zenith2/Docs/Design/SCRIPT.md → "Supported matrix and compile groups"

### Z2-R-SCRIPT-002 ZenithScriptCompiler library target

- **Statement:** ZenithScriptCompiler is one static library at `Zenith2/Engine/ZenithScriptCompiler/` declaring DEPENDS ZenithScript ZenithReflect ZenithCore, THIRD_PARTY Luau (owning `Luau.Ast` and `Luau.Compiler`), EXCEPTIONS INTERNAL and SUPPORTED ZENITH_HOST, linked only by ZenithCook, ZenithEditor and test targets.
- **Covers:** Z2-I-SCRIPT-01, Z2-I-SCRIPT-22
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-017
- **Budgets:** none
- **Acceptance evidence:** the `Configure`, `Build`, `Libgraph` and `CompileCoverage` steps; `Libgraph`'s Shipping-closure rule
- **Design:** Zenith2/Docs/Design/SCRIPT.md → "ZenithScriptCompiler target"; Zenith2/Docs/Design/SCRIPT.md → "ZenithScriptCompiler files"; Zenith2/Docs/Design/SCRIPT/Compiler.md → "Compiler library internals"

### Z2-R-SCRIPT-003 Public headers are Luau-free and contract-blocked

- **Statement:** Every root header of ZenithScript and ZenithScriptCompiler and `ZenithReflect/Zenith_ScriptBinding.h` includes no Luau, STL or Internal header, every fallible public function returns a Zenith_Result, and every public type carries one contract block.
- **Covers:** Z2-I-SCRIPT-22, Z2-I-SCRIPT-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-017, Z2-D-045, Z2-D-047
- **Budgets:** none
- **Acceptance evidence:** the `ApiContractCheck`, `HeaderSelfContained`, `ForbiddenCheck` (FORBIDDEN_LUAU_INCLUDE) and `Libgraph` (leakage) steps
- **Design:** Zenith2/Docs/Design/SCRIPT.md → "API rules"; Zenith2/Docs/Design/SCRIPT/PublicAPI.md → "Header list"; Zenith2/Docs/Design/SCRIPT.md → "Header index"

### Z2-R-SCRIPT-004 Luau headers confined to the firewall partitions

- **Statement:** Luau headers (`lua.h`, `lualib.h`, `luacode.h`, `luacodegen.h`, `Luau/*`) are included only by translation units under `ZenithScript/Internal/` and `ZenithScriptCompiler/Internal/`.
- **Covers:** Z2-I-SCRIPT-22
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-017
- **Budgets:** none
- **Acceptance evidence:** the `ForbiddenCheck` step (FORBIDDEN_LUAU_INCLUDE) and the `Libgraph` leakage check
- **Design:** Zenith2/Docs/Design/SCRIPT.md → "ZenithScript internal files"; Zenith2/Docs/Design/SCRIPT.md → "Supported matrix and compile groups"

### Z2-R-SCRIPT-005 Exception tokens confined to two catch sites

- **Statement:** The tokens `try` and `catch` appear only in `ZenithScript/Internal/Zenith_ScriptProtectedCall.cpp` and `ZenithScriptCompiler/Internal/Zenith_ScriptCompilerGuard.cpp`, and `throw` appears in no Zenith2 source.
- **Covers:** Z2-I-SCRIPT-22
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-017, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** the `ForbiddenCheck` step (FORBIDDEN_EXCEPTION_TOKEN)
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "Protected call implementation"; Zenith2/Docs/Design/SCRIPT/Compiler.md → "Compiler guard implementation"

### Z2-R-SCRIPT-006 One protection site

- **Statement:** `lua_pcall`, `lua_resume` and `luau_load` appear in exactly one translation unit, `ZenithScript/Internal/Zenith_ScriptProtectedCall.cpp`, and `lua_call`/`lua_callk` appear only under `ZenithScript/Internal/` inside protected regions.
- **Covers:** Z2-I-SCRIPT-22
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-017
- **Budgets:** none
- **Acceptance evidence:** the `ForbiddenCheck` step (FORBIDDEN_LUA_PROTECTION_SITE, FORBIDDEN_LUA_CALL)
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "Protected call implementation"; Zenith2/Docs/Design/SCRIPT/Bindings.md → "Environment construction"

### Z2-R-SCRIPT-007 Exception flags on the firewall compile groups alone

- **Statement:** `/EHs` (never `/EHsc`) and `-fexceptions` appear only on the compile groups `ZenithScript_EH`, `ZenithScriptCompiler_EH` and the Luau targets, and every other compile group carries the exceptions-off flags.
- **Covers:** Z2-I-SCRIPT-22
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-017
- **Budgets:** none
- **Acceptance evidence:** the `ForbiddenCheck` step (FORBIDDEN_EXCEPTION_FLAG over the CMake File API compile groups)
- **Design:** Zenith2/Docs/Design/SCRIPT.md → "Supported matrix and compile groups"; Zenith2/Docs/Design/SCRIPT.md → "ZenithScript target"

### Z2-R-SCRIPT-008 No longjmp error model

- **Statement:** Luau is built with its C++-exception error model: `LUA_USE_LONGJMP`, `setjmp` and `longjmp` appear nowhere in the tree.
- **Covers:** Z2-I-SCRIPT-22
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-017
- **Budgets:** none
- **Acceptance evidence:** the `ForbiddenCheck` step (FORBIDDEN_LONGJMP)
- **Design:** Zenith2/Docs/Design/SCRIPT.md → "Supported matrix and compile groups"

### Z2-R-SCRIPT-009 Luau API linkage measured, recorded and re-measured

- **Statement:** The Stage 1 firewall ticket records the pinned Luau API's language linkage and the inspected symbols in the Luau entry of `ThirdParty/manifest.json`, and a test repeats the measurement on every Test step and fails on a difference.
- **Covers:** Z2-I-SCRIPT-22
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-017
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Firewall_LuauApiLinkageRecorded` in the `Test` step (label `script-conformance`); the manifest diff of the Stage 1 firewall ticket
- **Design:** Zenith2/Docs/Design/SCRIPT/Tests.md → "Firewall and VM tests"; Zenith2/Docs/Design/SCRIPT.md → "Stage 1"

### Z2-R-SCRIPT-010 Protected entry algorithm

- **Statement:** `Zenith_ScriptProtectedCall` implements Scripting.md's eight-step algorithm, returns every Luau error as a `Zenith_ScriptError` value with its diagnostic recorded, converts any non-Luau exception to `SCRIPT_ERROR_KIND_INTERNAL`, and every allocating or raising Luau call after VM construction runs inside a protected region (asserted in Debug builds).
- **Covers:** Z2-I-SCRIPT-22
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-017, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Firewall_InternalExceptionIsFatal`, `Test_Script_Firewall_UnprotectedAllocationAsserts` (death suite) in the `Test` step and the `Zenith2Memory` lines (`Test -Label memory`, `Sanitizers -Preset win-asan`, `Sanitizers -Preset linux-tsan`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "Protected call implementation"; Zenith2/Docs/Design/SCRIPT/Runtime.md → "Allocator hook implementation"

### Z2-R-SCRIPT-011 Protected-call nesting and depth limit

- **Statement:** A nested protected call inherits the enclosing entity transaction's remaining safepoint budget and adds its count to it, and a re-entry beyond depth 8 is refused with ZS1701 before any Luau code runs.
- **Covers:** Z2-I-SCRIPT-22, Z2-I-SCRIPT-19
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-017, Z2-D-052
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Firewall_ProtectedDepthLimit` and the budget variant of `Test_Script_Firewall_ReentrantCallbackErrorCaught` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "Protected call implementation"

### Z2-R-SCRIPT-012 Compiler guard

- **Statement:** Every call from ZenithScriptCompiler into `Luau.Ast` or `Luau.Compiler` runs inside `Zenith_ScriptCompilerGuarded`, which converts parse and compile errors to `SCRIPT_ERROR_COMPILE` with the Luau location and any other exception to `SCRIPT_ERROR_INTERNAL`.
- **Covers:** Z2-I-SCRIPT-01, Z2-I-SCRIPT-22
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-017
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Compile_GuardConvertsExceptions` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Compiler.md → "Compiler guard implementation"; Zenith2/Docs/Design/SCRIPT/Compiler.md → "Compile procedure"

### Z2-R-SCRIPT-013 Callback re-entry through the invocation context

- **Statement:** An engine function that runs script does so only through `Zenith_ScriptInvocationContext::InvokeCallback`; an inner Luau error returns to the engine frame as a `Zenith_ScriptThunkError` value, and an inner abort-class or fatal error is re-raised by the outer dispatcher with the same kind.
- **Covers:** Z2-I-SCRIPT-22, Z2-I-SCRIPT-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-017
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Firewall_ReentrantCallbackErrorCaught` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Bindings.md → "Dispatcher implementation"

### Z2-R-SCRIPT-014 RAII firewall tests on every host preset and the S24

- **Statement:** The two RAII tests pass on win-debug, win-release, win-asan and linux-tsan and in the S24 HWASan run of each stage acceptance from Stage 3.
- **Covers:** Z2-I-SCRIPT-22
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-017, Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Firewall_RaiiUnwindsThroughBinding`, `Test_Script_Firewall_ReentrantCallbackErrorCaught` in the `Test` step and the `Zenith2Memory` lines (`Test -Label memory`, `Sanitizers -Preset win-asan`, `Sanitizers -Preset linux-tsan`); the S24 HWASan run of the Stage 3 acceptance ticket
- **Design:** Zenith2/Docs/Design/SCRIPT/Tests.md → "Firewall and VM tests"; Zenith2/Docs/Design/SCRIPT/Tests.md → "Gate steps and labels"

### Z2-R-SCRIPT-015 Shipping closure excludes the compiler and the debugger

- **Statement:** No game's Shipping link closure contains `ZenithScriptCompiler`, `Luau.Compiler`, `Luau.Ast` or any `Zenith_ScriptDap` symbol.
- **Covers:** Z2-I-SCRIPT-22, Z2-I-SCRIPT-06, Z2-I-SCRIPT-23
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-017, Z2-D-075
- **Budgets:** none
- **Acceptance evidence:** the `Libgraph` step (Shipping closure and link-surface checks, `Test_Script_Dap_AbsentFromShipping`)
- **Design:** Zenith2/Docs/Design/SCRIPT.md → "Supported matrix and compile groups"; Zenith2/Docs/Design/SCRIPT/Debugger.md → "Scope of this file"

### Z2-R-SCRIPT-016 Error records conform to the Contracts error form

- **Statement:** `Zenith_ScriptError` (code `Zenith_ScriptErrorCode`) and `Zenith_ScriptThunkError` (code `Zenith_ScriptThunkErrorCode`) are trivially copyable structs of at most 32 bytes whose first member `m_eCode` is their own code enumeration numbered from 1, `Seal` fails with the enumeration `Zenith_ScriptBindingError`, and message and traceback text is read from a diagnostic store or the VM's error scratch, never carried in the error (pending core-doc X115).
- **Covers:** Z2-I-SCRIPT-22
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** compilation of every `Zenith_Result<…, Zenith_ScriptError>`, `Zenith_Result<…, Zenith_ScriptThunkError>` and `Zenith_Result<void, Zenith_ScriptBindingError>` (the `static_assert(Zenith_ErrorType<E>)` of `Zenith_Result`) in the `Build` step; `Test_Script_Firewall_RaiiUnwindsThroughBinding` reading the ZS1511 text through the store; `Test_Script_Diagnostics_ConcurrentWriters` in the `Test` step and `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/SCRIPT/PublicAPI.md → "Zenith_ScriptError header"; Zenith2/Docs/Design/SCRIPT/PublicAPI.md → "Zenith_ScriptBinding header"; Zenith2/Docs/Design/SCRIPT/Transactions.md → "Diagnostic store implementation"; Zenith2/Docs/Design/SCRIPT.md → "Reconciliation items"

### Z2-R-SCRIPT-017 One main VM per world on the main thread

- **Statement:** Each script world owns exactly one main VM (VM index 0) created with the world, and every behaviour invocation, checkpoint serialization, OnRecover, Migrate step, console evaluation, main-VM reference release and fresh `self` or state proxy of that world runs on it on the main thread; a last-good restore's C++ decode and a go-live's generation swap at a tick boundary touch no Luau object and run on the world's tick-boundary node.
- **Covers:** Z2-I-SCRIPT-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-018
- **Budgets:** Z2-B-SCRIPT-009
- **Acceptance evidence:** `Test_Script_VM_SlotIndexMapping`, `Test_Script_HotReload_NoLuauAtTickBoundary` in the `Test` step and the `Zenith2Memory` lines (`Test -Label memory`, `Sanitizers -Preset win-asan`, `Sanitizers -Preset linux-tsan`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "Script world internals"; Zenith2/Docs/Design/SCRIPT/Runtime.md → "VM pool per world"; Zenith2/Docs/Design/SCRIPT/Runtime.md → "Behaviour node body"; Zenith2/Docs/Design/SCRIPT/Runtime.md → "Tick-boundary handler"

### Z2-R-SCRIPT-018 Worker VM slot table and VM indices

- **Statement:** Chunk-system VMs live in a per-world table of `Zenith_JobSystem::GetFrameThreadCount()` slots indexed by `Zenith_JobContext::GetWorkerIndex()`, slot s holding VM index s + 1, no VM index is 0xFFFF, and a slot is created or destroyed by the main thread only between iterations.
- **Covers:** Z2-I-SCRIPT-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-018
- **Budgets:** Z2-B-SCRIPT-010
- **Acceptance evidence:** `Test_Script_VM_SlotIndexMapping`, `Test_Script_VM_ParallelChunkSystemsSeparateWorkerVMs` in the `Test` step and the `Zenith2Memory` lines (`Test -Label memory`, `Sanitizers -Preset win-asan`, `Sanitizers -Preset linux-tsan`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "VM pool per world"; Zenith2/Docs/Design/SCRIPT/Runtime.md → "Script world internals"

### Z2-R-SCRIPT-019 Lazy worker VM creation

- **Statement:** A worker VM is created by its owning slot on the first chunk-system sub-job of the world on that slot, creation leaves no observable effect on results, and a creation failure halts the world with `SCRIPT_ERROR_KIND_VM_MEMORY`.
- **Covers:** Z2-I-SCRIPT-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-018, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_VM_LazyWorkerVMCreation` in the `Test` step and the `Zenith2Memory` lines (`Test -Label memory`, `Sanitizers -Preset win-asan`, `Sanitizers -Preset linux-tsan`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "Chunk-system node body"; Zenith2/Docs/Design/SCRIPT/Runtime.md → "VM creation steps"

### Z2-R-SCRIPT-020 No process-wide script state

- **Statement:** Every VM, arena, table and service of the script runtime is owned by `Zenith_ScriptRuntime` or a `Zenith_ScriptWorld`; no static `lua_State*`, global VM or service lookup exists.
- **Covers:** Z2-I-SCRIPT-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-046, Z2-D-018
- **Budgets:** none
- **Acceptance evidence:** the `ForbiddenCheck` writable-data scan over ZenithScript
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "Script runtime internals"; Zenith2/Docs/Design/SCRIPT/Runtime.md → "Script world internals"

### Z2-R-SCRIPT-021 Allocator hook and VM memory caps

- **Statement:** Every Luau allocation of every VM goes through `Zenith_ScriptAlloc` over the general heap and is refused when it would take the VM above its cap (the main VM against Z2-B-SCRIPT-009, worker VMs against Z2-B-SCRIPT-010).
- **Covers:** Z2-I-SCRIPT-20
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-052
- **Budgets:** Z2-B-SCRIPT-009, Z2-B-SCRIPT-010
- **Acceptance evidence:** `Test_Script_VM_MemoryCapFatal` in the `Test` step and the `Zenith2Memory` lines (`Test -Label memory`, `Sanitizers -Preset win-asan`, `Sanitizers -Preset linux-tsan`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "Allocator hook implementation"; Zenith2/Docs/Design/SCRIPT/Runtime.md → "VM pool per world"

### Z2-R-SCRIPT-022 VM memory-cap trip is fatal

- **Statement:** A VM memory-cap refusal halts the world with `SCRIPT_ERROR_KIND_VM_MEMORY`, abandons the tick without committing, reports through the world listener, and flags the tick `REPLAY_TICK_FLAG_NON_REPLAYABLE`.
- **Covers:** Z2-I-SCRIPT-20
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-052
- **Budgets:** Z2-B-SCRIPT-009, Z2-B-SCRIPT-010
- **Acceptance evidence:** `Test_Script_VM_MemoryCapFatal` in the `Test` step and the `Zenith2Memory` lines (`Test -Label memory`, `Sanitizers -Preset win-asan`, `Sanitizers -Preset linux-tsan`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "World halt procedure"; Zenith2/Docs/Design/SCRIPT.md → "Budget breach behaviour"

### Z2-R-SCRIPT-023 Worker VM total refused at world creation

- **Statement:** World creation fails with `SCRIPT_ERROR_INVALID_DESC` naming Z2-B-SCRIPT-011 when the worker VM cap times `m_uFrameThreadCount` (the desc's one frame-thread quantity) exceeds that row.
- **Covers:** Z2-I-SCRIPT-20
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-018
- **Budgets:** Z2-B-SCRIPT-011
- **Acceptance evidence:** `Test_Script_VM_WorkerTotalRefused` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "Script world internals"

### Z2-R-SCRIPT-024 GC tuning and bounded GC steps

- **Statement:** Each VM is created with the goal, step multiplier and step size of Z2-B-SCRIPT-017, and steps its collector only outside invocations (after each behaviour node, at the end of each sub-job) within an allowance of Z2-B-SCRIPT-007 per VM per tick, quadrupled under critical memory pressure.
- **Covers:** Z2-I-SCRIPT-18
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-052
- **Budgets:** Z2-B-SCRIPT-007, Z2-B-SCRIPT-017, Z2-B-SCRIPT-008
- **Acceptance evidence:** `Test_Script_VM_GcStepsWithinAllowance` in the `Test` step and the `Zenith2Memory` lines (`Test -Label memory`, `Sanitizers -Preset win-asan`, `Sanitizers -Preset linux-tsan`); Z2-B-SCRIPT-008 measured by `Zenith_Bench_BudgetsScript`
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "Garbage collection scheduling"; Zenith2/Docs/Design/SCRIPT/Runtime.md → "VM creation steps"

### Z2-R-SCRIPT-025 Collection timing unobservable

- **Statement:** No script can observe garbage collection: `collectgarbage`, `gcinfo`, `__mode` and `__gc` are unavailable, and results are identical with GC step budgets of 0 KB and 1,024 KB.
- **Covers:** Z2-I-SCRIPT-18, Z2-I-SCRIPT-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_VM_GcNotObservable`, `Test_Script_Budget_GcInterruptsNotCounted` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Bindings.md → "Environment construction"; Zenith2/Docs/Design/SCRIPT/Runtime.md → "Garbage collection scheduling"

### Z2-R-SCRIPT-026 Pooled invocation threads

- **Statement:** Each worker VM owns one sandboxed invocation thread reset after every invocation, each behaviour instance owns at most one coroutine thread, and no Luau thread is allocated per entity or per invocation.
- **Covers:** Z2-I-SCRIPT-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-018
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_VM_LazyWorkerVMCreation` (thread counts from `Zenith_ScriptDiagnostics`) and `Zenith_Bench_ScriptVM` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "VM creation steps"; Zenith2/Docs/Design/SCRIPT/Runtime.md → "Chunk-system node body"; Zenith2/Docs/Design/SCRIPT/Runtime.md → "Behaviour node body"

### Z2-R-SCRIPT-027 Invocation IDs

- **Statement:** Invocation IDs carry the VM index in bits 63..48 and a per-VM counter starting at 1 in bits 47..0, are never 0 and never reused within a world session, and a counter reaching 2^48 halts the world.
- **Covers:** Z2-I-SCRIPT-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-051
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_VM_InvocationCounterWrapHalts`, `Test_Script_VM_SlotIndexMapping` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/PublicAPI.md → "Zenith_ScriptTypes header"; Zenith2/Docs/Design/SCRIPT/Runtime.md → "Chunk-system node body"

### Z2-R-SCRIPT-028 VM teardown order

- **Statement:** World destruction happens after the tick is drained and destroys the worker VMs, then the main VM, on the main thread, never while an invocation, GC step or debugger request runs on a VM.
- **Covers:** Z2-I-SCRIPT-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-018
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_VM_TeardownOrder` in the `Test` step and the `Zenith2Memory` lines (`Test -Label memory`, `Sanitizers -Preset win-asan`, `Sanitizers -Preset linux-tsan`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "Script runtime internals"; Zenith2/Docs/Design/SCRIPT.md → "Construction and shutdown"

### Z2-R-SCRIPT-029 Cooked module manifest

- **Statement:** Each game's cook writes `scripts.zlmf` in the format of the design (sorted-path module IDs, dependencies, state-schema and system records, build hashes, a trailing content hash), and the runtime's `Load` refuses any malformed or mismatched manifest with ZS1805.
- **Covers:** Z2-I-SCRIPT-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Module_ManifestValidation`, `Test_Script_Cook_ManifestDeterministic` in the `Test` step (label `script-conformance`); `Zenith_Fuzz_Script_Manifest` in `Sanitizers -Preset win-asan`
- **Design:** Zenith2/Docs/Design/SCRIPT/Compiler.md → "Module manifest format"; Zenith2/Docs/Design/SCRIPT/Compiler.md → "Cook pipeline"

### Z2-R-SCRIPT-030 Literal require and cycle detection

- **Statement:** `require` accepts only a string literal resolved against the manifest, is callable only at module scope (ZS1803 otherwise), raises ZS1801 for an unknown path, and a dependency cycle fails with the cycle named `a -> b -> a` at cook and at load.
- **Covers:** Z2-I-SCRIPT-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-016
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Module_RequireLiteralOnly`, `Test_Script_Module_RequireCycleError` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Bindings.md → "Module loader implementation"; Zenith2/Docs/Design/SCRIPT/Compiler.md → "Cook pipeline"

### Z2-R-SCRIPT-031 Module generations and deep freeze

- **Statement:** Each VM loads each module at most once per module generation, from the world's bytecode arena, and deep-freezes every table reachable from its return value at the end of its load.
- **Covers:** Z2-I-SCRIPT-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-016
- **Budgets:** Z2-B-SCRIPT-019
- **Acceptance evidence:** `Test_Script_Module_DeepFrozen`, `Test_Script_HotReload_SystemGeneration` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Bindings.md → "Module loader implementation"; Zenith2/Docs/Design/SCRIPT/HotReload.md → "Chunk-system generations"

### Z2-R-SCRIPT-032 Module kinds and declaration validation

- **Statement:** A module's kind is decided by its return value, and a behaviour or chunk-system declaration failing any check of Scripting.md fails that module's load with ZS1806 naming the key, disabling the module for the world while the rest of the world runs.
- **Covers:** Z2-I-SCRIPT-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-016, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_State_UnserializableKindsLoadError`, `Test_Script_Budget_DeclarationLowersOnly` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Bindings.md → "Module loader implementation"; Zenith2/Docs/Design/SCRIPT/Runtime.md → "Script world internals"; Zenith2/Docs/Design/SCRIPT/Compiler.md → "Cook pipeline"

### Z2-R-SCRIPT-033 Strict mode for simulation modules

- **Statement:** Every chunk-system and behaviour module and every library they require starts with `--!strict`, and the cook refuses one that does not.
- **Covers:** Z2-I-SCRIPT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-016
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Compile_StrictRequired` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Compiler.md → "Compile procedure"

### Z2-R-SCRIPT-034 Role environments

- **Statement:** Each VM's environment is exactly the chunk-system or behaviour environment of Scripting/Bindings.md, every removed function is a stub raising its catalogue error, and every Zenith replacement runs.
- **Covers:** Z2-I-SCRIPT-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-016, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Sandbox_EnvironmentSubset` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Bindings.md → "Environment construction"; Zenith2/Docs/Design/SCRIPT/Bindings.md → "Per-VM thunk tables"

### Z2-R-SCRIPT-035 Mutable-globals compile option

- **Statement:** Every compile (cook and every hot-reload producer) passes `Zenith_ScriptSandboxMutableGlobals()` as Luau's `mutableGlobals`, and the list is an input of the binding-set hash.
- **Covers:** Z2-I-SCRIPT-07, Z2-I-SCRIPT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-016
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Bytecode_DevAndCookIdentical`, `Test_Script_Sandbox_EnvironmentSubset` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Compiler.md → "Compile procedure"; Zenith2/Docs/Design/SCRIPT/Bindings.md → "Environment construction"

### Z2-R-SCRIPT-036 No fast call reaches a replaced builtin

- **Statement:** Bytecode compiled with the production options contains no FASTCALL naming a replaced or stubbed builtin, verified against the pinned revision in Stage 1 and on every Test step.
- **Covers:** Z2-I-SCRIPT-07
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-016
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Sandbox_NoFastcallForReplacedBuiltins` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Compiler.md → "Fast-call listing"; Zenith2/Docs/Design/SCRIPT.md → "Stage 1"

### Z2-R-SCRIPT-037 Zenith pcall re-raises aborts

- **Statement:** The Zenith `pcall` and `xpcall` re-raise any abort-class or fatal error unchanged and return `false, message` for catchable errors.
- **Covers:** Z2-I-SCRIPT-07, Z2-I-SCRIPT-19
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-052
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Sandbox_PcallCannotCatchAbort`, `Test_Script_Budget_ExhaustionRollsBack` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Bindings.md → "Environment construction"

### Z2-R-SCRIPT-038 Address-free tostring

- **Statement:** No string produced by the script environment contains an address: the Zenith `tostring` returns the type name for tables, functions, threads and userdata without `__tostring`.
- **Covers:** Z2-I-SCRIPT-07, Z2-I-SCRIPT-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Sandbox_TostringNoAddress` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Bindings.md → "Environment construction"; Zenith2/Docs/Design/SCRIPT/Bindings.md → "Userdata tags and view objects"

### Z2-R-SCRIPT-039 Sandboxed VMs and threads

- **Statement:** Every VM runs `luaL_sandbox` after its environment is built and every invocation thread and behaviour coroutine is sandboxed with `luaL_sandboxthread` at creation.
- **Covers:** Z2-I-SCRIPT-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-016
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Sandbox_EnvironmentSubset` (global-write case raising ZS1608) in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "VM creation steps"; Zenith2/Docs/Design/SCRIPT/Bindings.md → "Environment construction"

### Z2-R-SCRIPT-040 No capability outside bindings

- **Statement:** No binding or environment member exposes the filesystem, processes, environment variables, sockets, dynamic libraries, raw addresses, another world's data, the debugger or a way to compile or load code.
- **Covers:** Z2-I-SCRIPT-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-075, Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Sandbox_NoCapabilityBindings`, `Test_Script_Sandbox_EnvironmentSubset` in the `Test` step (label `script-conformance`); review of every new binding
- **Design:** Zenith2/Docs/Design/SCRIPT/Bindings.md → "Environment construction"; Zenith2/Docs/Design/SCRIPT/Bindings.md → "Low-library binding files"; Zenith2/Docs/Design/SCRIPT/Bindings.md → "Per-VM thunk tables"

### Z2-R-SCRIPT-041 Parser limits

- **Statement:** Compilation refuses a module above 1 MiB, syntactic nesting above 200 and a game above 8,192 modules with `SCRIPT_ERROR_COMPILE`.
- **Covers:** Z2-I-SCRIPT-01, Z2-I-SCRIPT-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-016
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Compile_ParserLimits` in the `Test` step (label `script-conformance`); `Zenith_Fuzz_Script_Compile` in `Sanitizers -Preset win-asan`
- **Design:** Zenith2/Docs/Design/SCRIPT/Compiler.md → "Compile procedure"; Zenith2/Docs/Design/SCRIPT/Compiler.md → "Cook pipeline"

### Z2-R-SCRIPT-042 Production compile options

- **Statement:** Every cooked and hot-reloaded module is compiled at optimization level 2, debug level 1 and type-info level 1 with the production mutable-globals list, in every configuration.
- **Covers:** Z2-I-SCRIPT-01, Z2-I-SCRIPT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Bytecode_DevAndCookIdentical` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Compiler.md → "Compile procedure"; Zenith2/Docs/Design/SCRIPT/HotReload.md → "Development reload sources"

### Z2-R-SCRIPT-043 Cooked bytecode container

- **Statement:** The cook writes one `.zlbc` per module with the 48-byte header of Scripting.md written little-endian field by field, flag bit 3 set only for modules that passed the stateless check, and writes nothing when any module of the cook fails.
- **Covers:** Z2-I-SCRIPT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Cook_FailureWritesNothing`, `Test_Script_Cook_ManifestDeterministic` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Compiler.md → "Cooked bytecode container writer"; Zenith2/Docs/Design/SCRIPT/Compiler.md → "Cook pipeline"

### Z2-R-SCRIPT-044 Binding-set hash

- **Statement:** `Zenith_ScriptBindingSetHash` hashes the sorted function descriptors, the registered component and event types, the mutable-globals list and the compile options in the documented order, and the same value is written to every `.zlbc`, the manifest and the `.d.luau` header.
- **Covers:** Z2-I-SCRIPT-02, Z2-I-SCRIPT-04, Z2-I-SCRIPT-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038, D-SEL-HASH (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Bytecode_VersionMismatchRefused` (binding-set case), `Test_Script_Bindings_DluauGolden` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Compiler.md → "Cooked bytecode container writer"; Zenith2/Docs/Design/SCRIPT/Bindings.md → "Definition emitter procedure"

### Z2-R-SCRIPT-045 Load-time bytecode verification and trusted mounts

- **Statement:** The loader refuses a `.zlbc` failing any of the nine checks of the design with ZS1805 naming the field, never runs mismatched bytecode, and accepts bytecode only from the package and patch mounts in Shipping and additionally the loose mount in development builds.
- **Covers:** Z2-I-SCRIPT-02, Z2-I-SCRIPT-23
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Bytecode_VersionMismatchRefused` in the `Test` step (label `script-conformance`); `Zenith_Fuzz_Script_BytecodeHeader` in `Sanitizers -Preset win-asan`
- **Design:** Zenith2/Docs/Design/SCRIPT/Compiler.md → "Load-time verification procedure"; Zenith2/Docs/Design/SCRIPT/Bindings.md → "Module loader implementation"

### Z2-R-SCRIPT-046 Development and cooked bytecode identical

- **Statement:** In-process compilation by ZenithEditor and the cook produce byte-identical Luau blobs for the representative corpus.
- **Covers:** Z2-I-SCRIPT-02, Z2-I-SCRIPT-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Bytecode_DevAndCookIdentical` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/HotReload.md → "Development reload sources"; Zenith2/Docs/Design/SCRIPT/Compiler.md → "Compile procedure"

### Z2-R-SCRIPT-047 Re-cook rule and patch refusal

- **Statement:** Script bytecode is re-cooked on any engine version change and any Luau pin change with no runtime translation, and a patch whose bytecode's engine version hash differs from the installed engine is refused by the loader.
- **Covers:** Z2-I-SCRIPT-23
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** a packaged build with a mismatched patch refused (the shipping qualification run of the Stage 6 acceptance ticket); `Test_Script_Bytecode_VersionMismatchRefused`
- **Design:** Zenith2/Docs/Design/SCRIPT/Compiler.md → "Load-time verification procedure"; Zenith2/Docs/Design/SCRIPT.md → "Stage 6"

### Z2-R-SCRIPT-048 Luau pin record

- **Statement:** The Luau entry of `ThirdParty/manifest.json` records the commit hash, release tag, API linkage, native policy per platform and the bytecode versions emitted and accepted, and the runtime's verification reads the accepted range from the pinned build.
- **Covers:** Z2-I-SCRIPT-23, Z2-I-SCRIPT-22
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-082, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** the manifest entry checked by `Test_Script_Firewall_LuauApiLinkageRecorded` and the `Configure` step's manifest validation
- **Design:** Zenith2/Docs/Design/SCRIPT.md → "Stage 6"; Zenith2/Docs/Design/SCRIPT/Compiler.md → "Load-time verification procedure"

### Z2-R-SCRIPT-049 Pin change re-qualification

- **Statement:** A Luau pin change lands only with the linkage test, the native parity test, the stateless corpus, the determinism matrix, the builtin-name table and the pinned-text catalogue rows re-run and re-baselined in the same diff.
- **Covers:** Z2-I-SCRIPT-23
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-082, D-LUAU-STATELESS (open, Stage 1), D-SCRIPT-TXN refinement (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** the dependency-upgrade ticket's Test step run of every named test; review
- **Design:** Zenith2/Docs/Design/SCRIPT.md → "Stage 6"; Zenith2/Docs/Design/SCRIPT/Compiler.md → "Fast-call listing"

### Z2-R-SCRIPT-050 Shipping bytecode qualification run

- **Statement:** A packaged Shipping build of each flagship slice on Windows and the S24 loads only verified cooked bytecode, runs its playthrough script with no script refusal, and refuses a deliberately stale `.zlbc`.
- **Covers:** Z2-I-SCRIPT-23
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-048, Z2-D-021
- **Budgets:** none
- **Acceptance evidence:** the Stage 6 acceptance ticket's packaged-build run records
- **Design:** Zenith2/Docs/Design/SCRIPT.md → "Stage 6"

### Z2-R-SCRIPT-051 Binding ABI header in ZenithReflect

- **Statement:** The Luau-free binding ABI is `ZenithReflect/Zenith_ScriptBinding.h` with the declarations of Scripting/Bindings.md and the design's additions, includes no ZenithScript header, and every library owning bindings depends on ZenithReflect and never on ZenithScript.
- **Covers:** Z2-I-SCRIPT-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-016, Z2-D-017
- **Budgets:** none
- **Acceptance evidence:** the `Libgraph` step (no edge to ZenithScript from a binding owner); `ApiContractCheck`
- **Design:** Zenith2/Docs/Design/SCRIPT/PublicAPI.md → "Zenith_ScriptBinding header"; Zenith2/Docs/Design/SCRIPT.md → "SCRIPT files in other libraries"

### Z2-R-SCRIPT-052 Registration template checks

- **Statement:** `Zenith_ScriptBindingTable::AddFunction` fails to compile, with the documented message, for a function whose parameter or result has no value-kind mapping.
- **Covers:** Z2-I-SCRIPT-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Bindings_UnmappedParameterRejected` (compile-failure CTest) in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Bindings.md → "Binding table implementation"

### Z2-R-SCRIPT-053 Binding table sealing and IDs

- **Statement:** The binding table is sealed by the composition root before the first world; `Seal` returns a `Zenith_ScriptBindingError` for a function-ID or Luau-name collision, an invalid name or descriptor, or a buffered kind without a consumer, leaving the table unsealed, and `AddFunction` after sealing asserts.
- **Covers:** Z2-I-SCRIPT-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Bindings_SealRejectsCollision` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Bindings.md → "Binding table implementation"; Zenith2/Docs/Design/SCRIPT/PublicAPI.md → "Zenith_ScriptBinding header"

### Z2-R-SCRIPT-054 Dispatcher algorithm

- **Statement:** The dispatcher checks context, declared resources, arity and argument kinds in that order, raises the catalogued error of each failure, calls the thunk with the invocation context, raises a thunk error with its kind, and restores its frame-arena mark and live-scope counter on every path.
- **Covers:** Z2-I-SCRIPT-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-017
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Firewall_RaiiUnwindsThroughBinding`, `Test_Script_Bindings_ContextEnforced` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Bindings.md → "Dispatcher implementation"

### Z2-R-SCRIPT-055 Lossless value marshalling

- **Statement:** Every conversion of Scripting/Bindings.md's table raises its catalogued error on failure and never truncates, wraps or clamps a value.
- **Covers:** Z2-I-SCRIPT-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Bindings_MarshallingLossless` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Bindings.md → "Marshalling implementation"

### Z2-R-SCRIPT-056 Context and resource enforcement

- **Statement:** A binding called from a context it does not allow raises ZS1702 and a READ_WORLD binding whose resource is not in the calling module's `Uses` raises ZS1703.
- **Covers:** Z2-I-SCRIPT-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Bindings_ContextEnforced` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Bindings.md → "Dispatcher implementation"; Zenith2/Docs/Design/SCRIPT/Bindings.md → "Per-VM thunk tables"

### Z2-R-SCRIPT-057 Buffered bindings respect their side-effect mask

- **Statement:** A BUFFERED binding appends only record kinds in its mask, the append checks the mask on every call and reports a violation as a thunk error, and no binding performs a world-visible side effect directly.
- **Covers:** Z2-I-SCRIPT-04, Z2-I-SCRIPT-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-016
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Bindings_SideEffectMask` in the `Test` step (label `script-conformance`); review of every new side-effect kind
- **Design:** Zenith2/Docs/Design/SCRIPT/Transactions.md → "Side-effect routing"

### Z2-R-SCRIPT-058 Handle traits owned by the handle's library

- **Statement:** Entity, asset and persistent-ID handles cross the ABI as `Zenith_ScriptOpaqueHandle` through `Zenith_ScriptValueTraits` specialised by their owning library next to the type.
- **Covers:** Z2-I-SCRIPT-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** the `Libgraph` step; `Test_Script_Bindings_MarshallingLossless` (entity and asset rows)
- **Design:** Zenith2/Docs/Design/SCRIPT/Bindings.md → "Marshalling implementation"; Zenith2/Docs/Design/SCRIPT.md → "SCRIPT files in other libraries"

### Z2-R-SCRIPT-059 Bindings of libraries below Reflect

- **Statement:** Bindings of ZenithCore, ZenithGraph, ZenithJobs, ZenithProfiling, ZenithPlatform, ZenithVertexFormat and ZenithCompression are written in `ZenithRuntime/Internal/ScriptBindings/` and registered by the composition root.
- **Covers:** Z2-I-SCRIPT-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** the `Libgraph` step (no library below ZenithReflect depends on it); `Test_Script_Sandbox_EnvironmentSubset` (namespaces present)
- **Design:** Zenith2/Docs/Design/SCRIPT/Bindings.md → "Low-library binding files"

### Z2-R-SCRIPT-060 Side-effect consumers

- **Statement:** Each SCRIPT-held side-effect kind (audio and VFX triggers, save requests, asynchronous requests) is delivered at commit, in merge order, to the `Zenith_ScriptSideEffectConsumer` its owner registered before sealing, and a binding appending such a kind is refused at `Seal` when no consumer exists.
- **Covers:** Z2-I-SCRIPT-04, Z2-I-SCRIPT-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-016, Z2-D-051
- **Budgets:** Z2-B-SCRIPT-023, Z2-B-SCRIPT-024, Z2-B-SCRIPT-025
- **Acceptance evidence:** `Test_Script_Merge_ScriptAndEcsClassesAtomic`, `Test_Script_Bindings_SealRejectsCollision` (consumer case) in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Bindings.md → "Side-effect consumer registration"; Zenith2/Docs/Design/SCRIPT/Transactions.md → "Commit implementation"

### Z2-R-SCRIPT-061 Generated Luau definition outputs and runtime descriptors

- **Statement:** The luau-types step writes `zenith.d.luau`, `components.d.luau`, `Zenith.luau`, `Types.luau` and `.luaurc` for exactly the game's selected modules and schemas under `${ZENITH_OUT}/Generated/Luau/<Game>/`, never tracked, and the runtime `@types` table has one descriptor per generated name with the same spelling.
- **Covers:** Z2-I-SCRIPT-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-020, Z2-D-024
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Bindings_DluauGolden` in the `Test` step (label `script-conformance`); the `<Name>_Types` build target
- **Design:** Zenith2/Docs/Design/SCRIPT/Bindings.md → "Definition emitter procedure"; Zenith2/Docs/Design/SCRIPT/Bindings.md → "The @zenith prelude and @types modules"; Zenith2/Docs/Design/SCRIPT.md → "Script source trees and cooked outputs"

### Z2-R-SCRIPT-062 Byte-stable generated definitions

- **Statement:** Two generations from one registry and binding table produce identical bytes, reflected types ordered by stable name with fields by field ID, binding functions by (namespace, Luau name), with the binding-set hash and no timestamp in the header.
- **Covers:** Z2-I-SCRIPT-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-020
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Bindings_DluauGolden` against `Tests/Golden/fixture.d.luau` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Bindings.md → "Definition emitter procedure"; Zenith2/Docs/Design/SCRIPT.md → "Reconciliation items"

### Z2-R-SCRIPT-063 Hungarian name mapping

- **Statement:** Luau names are derived by the mapping rules of Scripting/Bindings.md, and a collision or invalid name fails generation with both C++ sources named and writes no file.
- **Covers:** Z2-I-SCRIPT-05, Z2-I-SCRIPT-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-034
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Types_NameMapping` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Bindings.md → "Definition emitter procedure"

### Z2-R-SCRIPT-064 Cook type check

- **Statement:** The cook type-checks every module with the pinned `luau-analyze` in strict mode against the generated definitions, out of process, and a type error in a simulation module fails the cook.
- **Covers:** Z2-I-SCRIPT-05, Z2-I-SCRIPT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-016
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Cook_TypeErrorFailsCook` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Compiler.md → "Cook pipeline"

### Z2-R-SCRIPT-065 Address-keyed iteration lint

- **Statement:** The cook rejects in every simulation module a generic `for` over a table whose key type is not number, string, boolean or a string-literal union, or cannot be inferred, with `SCRIPT_LINT_ADDRESS_KEYED_ITERATION`.
- **Covers:** Z2-I-SCRIPT-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Determinism_AddressKeyedIterationLint` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Compiler.md → "Address-keyed iteration lint"

### Z2-R-SCRIPT-066 Behaviour declaration validation

- **Statement:** A behaviour declaration with an unknown or missing key, a bad `Phase`, a write its entity cannot carry, a `Run` without `OnRecover` or a `Budget` above its row fails its module's load with ZS1806.
- **Covers:** Z2-I-SCRIPT-10, Z2-I-SCRIPT-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-023, Z2-D-080
- **Budgets:** Z2-B-SCRIPT-001
- **Acceptance evidence:** `Test_Script_Budget_DeclarationLowersOnly`, `Test_Script_State_UnserializableKindsLoadError` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Bindings.md → "Module loader implementation"; Zenith2/Docs/Design/SCRIPT/Compiler.md → "Declaration extraction"

### Z2-R-SCRIPT-067 Chunk system declaration and query

- **Statement:** A chunk system's query is the entities carrying every component of `Reads`, `Writes` and `With` and none of `Without`, compiled once per module generation, and its `Each` is called once per matching entity.
- **Covers:** Z2-I-SCRIPT-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-018, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Txn_EntityBoundaryMiddleFailure` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "Node registration and frame-graph placement"; Zenith2/Docs/Design/SCRIPT/Runtime.md → "Chunk-system node body"

### Z2-R-SCRIPT-068 Script systems through the system registry

- **Statement:** Every chunk system and behaviour node registers through `Zenith_SystemRegistry::AddSystem` with `SYSTEM_KIND_SCRIPT_CHUNK` or `SYSTEM_KIND_SCRIPT_BEHAVIOURS`, the descriptor and builder calls of the design and a merge function, in Scripting.md's system order (topological with source-ID ties, then behaviour-module order), which is ECS's registration index.
- **Covers:** Z2-I-SCRIPT-08, Z2-I-SCRIPT-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-018
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Merge_ChainInSystemOrder`, `Test_Script_Txn_WriteOutsideAuthority` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "Node registration and frame-graph placement"; Zenith2/Docs/Design/SCRIPT.md → "Reconciliation items"

### Z2-R-SCRIPT-069 Schedule-compile checks for script systems

- **Statement:** A script system writing a field outside its phase's authority, two direct writers without an arbitration rule, or a declared cross-entity read of a column written in the phase fails schedule compilation with the documented diagnostic, and the module is not run.
- **Covers:** Z2-I-SCRIPT-08, Z2-I-SCRIPT-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-051
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Txn_WriteOutsideAuthority`, `Test_Script_Txn_CrossEntityReadRule` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "Node registration and frame-graph placement"

### Z2-R-SCRIPT-070 Behaviour node placement and order

- **Statement:** Behaviour modules contribute one main-thread node per phase they run in (an Early module adding a Late handler node when it subscribes to events or has state), event handlers run in the Late scripts phase in delivery order, and behaviour nodes of a phase run in ascending module ID.
- **Covers:** Z2-I-SCRIPT-17, Z2-I-SCRIPT-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080, D-PHASES refinement (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Event_DeliveryInMergeOrder`, `Test_Script_State_CheckpointDirtyOnlyAtMostOncePerTick` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "Node registration and frame-graph placement"; Zenith2/Docs/Design/SCRIPT/Runtime.md → "Behaviour node body"

### Z2-R-SCRIPT-071 Dependents start after the merge

- **Statement:** Every node reading or writing a column a script node writes starts only after that script node's merge node committed, so no dependent observes a write a merge-time rejection restores.
- **Covers:** Z2-I-SCRIPT-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-051
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Merge_DependentsRunAfterMerge` (1 and 8 workers) in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "Node registration and frame-graph placement"; Zenith2/Docs/Design/SCRIPT/Transactions.md → "Merge node body"

### Z2-R-SCRIPT-072 Disabled script nodes

- **Statement:** A disabled module's nodes and merge nodes stay in the schedule and complete without running script, committing or folding — the disable is SCRIPT's own flag, set at the tick boundary without any `MAIN_THREAD` ECS call and read at the entry of every node and merge body of the module (ECS's matching disable pending ECS SC23) — and disabling never recompiles the schedule.
- **Covers:** Z2-I-SCRIPT-08, Z2-I-SCRIPT-12
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-051
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Txn_DisabledFromNextTickBoundary`, `Test_Script_Module_DisabledNodesRunNoScript`, `Test_Script_HotReload_NoLuauAtTickBoundary` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "Tick-boundary handler"; Zenith2/Docs/Design/SCRIPT/Runtime.md → "Chunk-system node body"; Zenith2/Docs/Design/SCRIPT/Transactions.md → "Merge node body"

### Z2-R-SCRIPT-073 Behaviour instance directory

- **Statement:** A behaviour instance exists for exactly the entities whose `Zenith.Behaviour` component names the module, is created at its first node invocation after the spawn's sync point (OnCreate then), is dormant while `Enabled` is false, and is released when its entity is destroyed.
- **Covers:** Z2-I-SCRIPT-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-023, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Module_BehaviourDirectory` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "Behaviour instance directory"; Zenith2/Docs/Design/SCRIPT/PublicAPI.md → "Zenith_ScriptBehaviourComponent header"; Zenith2/Docs/Design/SCRIPT.md → "Behaviour instance state machine"

### Z2-R-SCRIPT-074 Entity transaction boundary

- **Statement:** Each entity processed by a chunk-system invocation and each behaviour callback or coroutine segment is its own entity transaction, and a failure rolls back only the failing entity while the other entities of the call and tick commit.
- **Covers:** Z2-I-SCRIPT-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-051, Z2-D-016, D-SCRIPT-TXN refinement (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Txn_EntityBoundaryMiddleFailure` (one chunk, three chunks, three workers) in the `Test` step and the `Zenith2Memory` lines (`Test -Label memory`, `Sanitizers -Preset win-asan`, `Sanitizers -Preset linux-tsan`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Transactions.md → "Invocation lifecycle"; Zenith2/Docs/Design/SCRIPT/Transactions.md → "Failure handling by kind"; Zenith2/Docs/Design/SCRIPT/Transactions.md → "Worked lifecycle traces"

### Z2-R-SCRIPT-075 Row trampoline and per-entity budget

- **Statement:** The row trampoline is C++ inside the protected call, resets the safepoint count and limit at each entity, and continues with the next row after a failing row.
- **Covers:** Z2-I-SCRIPT-08, Z2-I-SCRIPT-19
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-051, Z2-D-052
- **Budgets:** Z2-B-SCRIPT-002
- **Acceptance evidence:** `Test_Script_Budget_SafepointCountDeterministic`, `Test_Script_Txn_DisabledFromNextTickBoundary` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Transactions.md → "Invocation lifecycle"; Zenith2/Docs/Design/SCRIPT/Runtime.md → "Chunk-system node body"

### Z2-R-SCRIPT-076 Transaction arena partition

- **Statement:** Every byte of transaction memory is `Zenith_Arena` memory: a scratch arena per slot, pre-reserved at world creation to twice one invocation's worst case and reset to the invocation's marker at its end, and a retention arena per (script system, slot) claimed at the system's first invocation on the slot and reset — poisoned or retagged — by that system's merge alone, so a merge never releases another system's transactions (pending core-doc X116).
- **Covers:** Z2-I-SCRIPT-08, Z2-I-SCRIPT-20
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-051, Z2-D-041, D-SCRIPT-TXN refinement (open, Stage 1)
- **Budgets:** Z2-B-SCRIPT-012, Z2-B-SCRIPT-015
- **Acceptance evidence:** `Test_Script_Txn_RetentionArenasPerNode`, `Test_Script_Txn_ScratchCopiedToRetention` in the `Test` step and the `Zenith2Memory` lines (`Test -Label memory`, `Sanitizers -Preset win-asan`, `Sanitizers -Preset linux-tsan`); `Test_Script_Txn_RetainedTransactionPoisonedAfterMerge` (intentional-defect test) in `Sanitizers -Preset win-asan`
- **Design:** Zenith2/Docs/Design/SCRIPT/Transactions.md → "Transaction arena partition"; Zenith2/Docs/Design/SCRIPT/Transactions.md → "Invocation layout in the arenas"; Zenith2/Docs/Design/SCRIPT.md → "Reconciliation items"

### Z2-R-SCRIPT-077 Transaction arena growth failure is fatal

- **Statement:** A retention-arena block the root heap cannot supply, or a failed growth of a retention entry's transaction list, halts the world with `SCRIPT_ERROR_KIND_RESERVATION` (ZS1403), abandoning the tick non-replayably; the scratch arenas never grow.
- **Covers:** Z2-I-SCRIPT-08, Z2-I-SCRIPT-20
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-051
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Txn_ArenaGrowthFailureFatal` in the `Test` step and the `Zenith2Memory` lines (`Test -Label memory`, `Sanitizers -Preset win-asan`, `Sanitizers -Preset linux-tsan`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Transactions.md → "Transaction arena partition"; Zenith2/Docs/Design/SCRIPT/Runtime.md → "World halt procedure"

### Z2-R-SCRIPT-078 Write path

- **Statement:** A script write validates the view, resolves the field in O(1), checks the declared write set, the current row, the field kind and the value, takes the first-write snapshot, stores in place and marks the row's pending change bit, so reads within the transaction see the write.
- **Covers:** Z2-I-SCRIPT-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-051
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Txn_ConcurrentDisjointColumnWrites`, `Test_Script_Misuse_Catalogue` (ZS1201, ZS1203, ZS1205) in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Transactions.md → "Write path implementation"

### Z2-R-SCRIPT-079 First-write snapshots

- **Statement:** On an invocation's first write only its declared-writable columns of the chunk range (one row for a behaviour) are snapshotted, a column granted to another job is never snapshotted or restored, and snapshots live until the merge.
- **Covers:** Z2-I-SCRIPT-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-051
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Txn_ConcurrentDisjointColumnWrites`, `Test_Script_Merge_SnapshotLivesUntilMerge` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Transactions.md → "Snapshot capture and restore"

### Z2-R-SCRIPT-080 Per-row restore on failure

- **Statement:** A failing entity's rows are restored from the snapshot, its pending bits cleared, its shadows dropped and its command buffer and records discarded, so no queued destroy, event or spawn of it survives.
- **Covers:** Z2-I-SCRIPT-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-051
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Txn_FailureAfterDestroyKeepsEntity`, `Test_Script_Txn_FailureAfterEventNotDelivered` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Transactions.md → "Snapshot capture and restore"; Zenith2/Docs/Design/SCRIPT/Transactions.md → "Failure handling by kind"

### Z2-R-SCRIPT-081 Copy-on-write shadows

- **Statement:** A behaviour's container or string field is mutated only through a per-invocation shadow the script reads back, committed at the merge — a container swapped through ECS's merge-time container replacement, a string interned and written in place — so every reader sees it after the merge point, and dropped on failure or rejection; chunk views never expose containers (pending ECS SC21; under the fallback, containers are visible from the next sync point through a container-replacement command whose element copy lives in the entity's ECS command buffer).
- **Covers:** Z2-I-SCRIPT-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-051
- **Budgets:** Z2-B-SCRIPT-016, Z2-B-SCRIPT-022
- **Acceptance evidence:** `Test_Script_Txn_ContainerShadowDroppedOnFailure`, `Test_Script_Txn_SnapshotCapExhaustion`, `Test_Script_Txn_ContainerVisibleAfterMerge` (both forms), `Test_Script_Txn_StringFieldCommittedAtMerge` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Transactions.md → "Copy-on-write shadow implementation"

### Z2-R-SCRIPT-082 Private side-effect buffers

- **Statement:** Every side effect goes to the entity's private carriers (its ECS command buffer for structural, field, event and contribution records; SCRIPT records for triggers, saves and async requests) and reaches no world queue before the merge accepts the entity.
- **Covers:** Z2-I-SCRIPT-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-051, Z2-D-016
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Txn_FailureAfterEventNotDelivered`, `Test_Script_Merge_OrderIndependentOfCompletion` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Transactions.md → "Side-effect routing"; Zenith2/Docs/Design/SCRIPT.md → "Reconciliation items"

### Z2-R-SCRIPT-083 Provisional spawn handles

- **Statement:** `ctx:Spawn` returns a provisional handle usable by subsequent commands of the same transaction, remapped at playback; a failed or rejected spawn consumes no entity or simulation ID, and a provisional handle used after its one-tick window raises ZS1103.
- **Covers:** Z2-I-SCRIPT-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-051, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Txn_SpawnIdsReleasedOnFailure`, `Test_Script_Spawn_IdsAssignedAtPlaybackInMergeOrder` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Transactions.md → "Side-effect routing"

### Z2-R-SCRIPT-084 Per-entity memory cap

- **Statement:** Every snapshot share, shadow byte, record and payload is charged to its entity before allocation, and exceeding Z2-B-SCRIPT-012 raises ZS1402 (`SNAPSHOT_CAP`) for that entity alone.
- **Covers:** Z2-I-SCRIPT-08, Z2-I-SCRIPT-20
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-051, Z2-D-052
- **Budgets:** Z2-B-SCRIPT-012, Z2-B-SCRIPT-013
- **Acceptance evidence:** `Test_Script_Txn_SnapshotCapExhaustion`, `Test_Script_Fault_SnapshotCapExceeded` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Transactions.md → "Per-entity accounting implementation"

### Z2-R-SCRIPT-085 Cross-entity read enforcement

- **Statement:** A runtime read of another entity's row in a column written in the current phase raises ZS1204 in every build.
- **Covers:** Z2-I-SCRIPT-08, Z2-I-SCRIPT-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-051
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Txn_CrossEntityReadRule` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Transactions.md → "Write path implementation"; Zenith2/Docs/Design/SCRIPT/Bindings.md → "Userdata tags and view objects"

### Z2-R-SCRIPT-086 Merge-time acceptance

- **Statement:** The merge sorts a node's succeeded entity transactions by (simulation ID, ordinal) and accepts each whose demand fits every ECS and SCRIPT capacity class at once, first-fit, rejecting the rest with their rows restored and `SCRIPT_MERGE_REJECTED_CAPACITY`, without disabling the module.
- **Covers:** Z2-I-SCRIPT-08, Z2-I-SCRIPT-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-051, D-SCRIPT-TXN refinement (open, Stage 1)
- **Budgets:** Z2-B-SCRIPT-016, Z2-B-SCRIPT-022, Z2-B-SCRIPT-023, Z2-B-SCRIPT-024, Z2-B-SCRIPT-025, Z2-B-SCRIPT-026
- **Acceptance evidence:** `Test_Script_Merge_CapacityRejectionDeterministic`, `Test_Script_Merge_ScriptAndEcsClassesAtomic` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Transactions.md → "Merge node body"; Zenith2/Docs/Design/SCRIPT/Transactions.md → "Capacity acceptance across ECS and SCRIPT classes"

### Z2-R-SCRIPT-087 Commit cannot fail

- **Statement:** Commit accepts buffers, delivers SCRIPT records, swaps checkpoints and commits row changes of accepted entities only, performing no allocation, capacity check or fallible operation.
- **Covers:** Z2-I-SCRIPT-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-051
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Merge_ChangeVersionsAdvanceAtMerge`, `Test_Script_Txn_EntityBoundaryMiddleFailure` in the `Test` step (label `script-conformance`); review
- **Design:** Zenith2/Docs/Design/SCRIPT/Transactions.md → "Commit implementation"

### Z2-R-SCRIPT-088 Merge chain in system order

- **Statement:** The merges of a phase's script nodes run one after another in system order, so shared capacity is consumed in (system order, logical ID, sequence) order whatever node finished first.
- **Covers:** Z2-I-SCRIPT-17, Z2-I-SCRIPT-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Merge_ChainInSystemOrder` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Transactions.md → "Merge node body"; Zenith2/Docs/Design/SCRIPT/Runtime.md → "Node registration and frame-graph placement"

### Z2-R-SCRIPT-089 Invocations of one behaviour instance

- **Statement:** When an instance has several invocations in one node, a failed or rejected invocation causes every subsequent invocation of the instance in that node to be skipped or rejected, with its row restored to the earliest failed or rejected invocation's snapshot.
- **Covers:** Z2-I-SCRIPT-08, Z2-I-SCRIPT-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-051
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Merge_BehaviourSubsequentInvocationsRejected` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Transactions.md → "Merge node body"; Zenith2/Docs/Design/SCRIPT/Runtime.md → "Behaviour node body"

### Z2-R-SCRIPT-090 Disable from the next tick boundary

- **Statement:** A module with any failure in a tick is disabled at step 3 of the next tick boundary, every other invocation of the failing tick having run, and the failure is reported with module, simulation ID, tick, kind, code, message and traceback.
- **Covers:** Z2-I-SCRIPT-08, Z2-I-SCRIPT-12
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-051
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Txn_DisabledFromNextTickBoundary` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "Tick-boundary handler"; Zenith2/Docs/Design/SCRIPT.md → "Module state machine"

### Z2-R-SCRIPT-091 Commits never invalidate views

- **Statement:** A commit advances change versions only, so another invocation's view stays valid across it.
- **Covers:** Z2-I-SCRIPT-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-051
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Txn_CommitNeverInvalidatesOtherViews` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Transactions.md → "Commit implementation"

### Z2-R-SCRIPT-092 View validation on every access

- **Statement:** Every view, container, context and state proxy is validated on every access in every build: ZS1001 outside its invocation, ZS1002 after a yield, ZS1003 after a structural change, ZS1102 for another world.
- **Covers:** Z2-I-SCRIPT-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-051, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_View_RetainedPastInvocationErrors` (Debug, Release, Shipping), `Test_Script_View_UsedAfterYieldErrors`, `Test_Script_View_StaleEpochErrors` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Bindings.md → "Userdata tags and view objects"

### Z2-R-SCRIPT-093 Merge keys and logical invocation IDs

- **Statement:** Command, event and SCRIPT-record order is (system order, logical invocation ID, sequence), the logical invocation ID being (stable simulation ID, invocation ordinal) and never a chunk, storage, worker or VM index.
- **Covers:** Z2-I-SCRIPT-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Merge_OrderIndependentOfCompletion`, `Test_Script_Spawn_IdsAssignedAtPlaybackInMergeOrder` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Transactions.md → "Side-effect routing"; Zenith2/Docs/Design/SCRIPT/Transactions.md → "Merge node body"; Zenith2/Docs/Design/SCRIPT.md → "Determinism obligations"

### Z2-R-SCRIPT-094 Invocation ordinals

- **Statement:** A behaviour invocation's ordinal is its index among its instance's invocations of the tick in the documented order (OnRecover, OnCreate, then per phase handlers then OnUpdate or Run, then checkpoint-only), and a chunk-system invocation's ordinal is 0.
- **Covers:** Z2-I-SCRIPT-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Random_RerunIdentical`, `Test_Script_State_CheckpointDirtyOnlyAtMostOncePerTick` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "Behaviour node body"

### Z2-R-SCRIPT-095 Counter-based script randomness

- **Statement:** `ctx.Random` is the only randomness available to simulation scripts, each draw being `Zenith_Hash64` of the 40-byte key of the running entity transaction with CORE's derivations, and re-running or parallelising invocations reproduces the same sequences.
- **Covers:** Z2-I-SCRIPT-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, D-SEL-HASH (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Random_RerunIdentical`, `Test_Script_Random_ParallelIdentical` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "Random draws"

### Z2-R-SCRIPT-096 Random key layout and format bump

- **Statement:** The random key is exactly 40 bytes with fixed golden draws, and any change to its layout, the hash or the derivations is a replay-format version bump.
- **Covers:** Z2-I-SCRIPT-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, D-SEL-HASH (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Random_KeyLayout` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "Random draws"

### Z2-R-SCRIPT-097 Event delivery order

- **Statement:** Events emitted by scripts are delivered in phase 9 in the merged order of their records, every delivered event of a subscribed type runs its handler for every subscribed instance in phase 10 in that order with no per-instance limit, and an event emitted in phase 10 is delivered in the next tick.
- **Covers:** Z2-I-SCRIPT-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-080
- **Budgets:** Z2-B-GAME-003
- **Acceptance evidence:** `Test_Script_Event_DeliveryInMergeOrder`, `Test_Script_Event_EveryDeliveredEventHandled` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "Behaviour node body"

### Z2-R-SCRIPT-098 Worker floating-point environment

- **Statement:** Every worker and the main thread read back the fixed floating-point state of Contracts and flush a denormal product to zero, on every preset.
- **Covers:** Z2-I-SCRIPT-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Determinism_WorkerFloatEnvironment` in the `Test` step and the `Zenith2Memory` lines (`Test -Label memory`, `Sanitizers -Preset win-asan`, `Sanitizers -Preset linux-tsan`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Tests.md → "Firewall and VM tests"

### Z2-R-SCRIPT-099 Determinism replay matrix

- **Statement:** One recorded replay run with 1, 2, 8 and all workers, with and without forced compaction and with the default and alternate chunk capacities gives bit-identical per-tick world-state hashes on each ECS backend.
- **Covers:** Z2-I-SCRIPT-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-039
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Determinism_ReplayMatrix` (conformance) in the `Test` step (label `script-conformance`); the Stage 3 acceptance ticket
- **Design:** Zenith2/Docs/Design/SCRIPT/Tests.md → "Determinism and fault-matrix tests"

### Z2-R-SCRIPT-100 Fault matrix

- **Statement:** The five FaultArena fault tests give bit-identical per-tick hashes and identical failed, rejected, diagnostic, disabled and recovery sets across the 16 configurations per backend, the scenario having exactly the specified parameters.
- **Covers:** Z2-I-SCRIPT-17, Z2-I-SCRIPT-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-051
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Fault_ScriptError`, `_BudgetExhausted`, `_SnapshotCapExceeded`, `_MergeCapacityRejection`, `_Combined` (labels `script-conformance`, `script-fault-matrix`) in the `Test` step; the Stage 3 acceptance ticket
- **Design:** Zenith2/Docs/Design/SCRIPT/Tests.md → "Determinism and fault-matrix tests"

### Z2-R-SCRIPT-101 Safepoint counting

- **Statement:** The interrupt callback counts only calls with gc == -1, raises the catalogued budget error when the count exceeds the entity transaction's limit, and GC notifications and time in bindings are never counted.
- **Covers:** Z2-I-SCRIPT-19
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-052
- **Budgets:** Z2-B-SCRIPT-001, Z2-B-SCRIPT-002
- **Acceptance evidence:** `Test_Script_Budget_SafepointCountDeterministic`, `Test_Script_Budget_GcInterruptsNotCounted` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "Interrupt callback implementation"

### Z2-R-SCRIPT-102 Budget scopes

- **Statement:** Chunk-system entities, behaviour invocations, OnRecover and Migrate steps and module scopes each run against their own row, a declaration can only lower its limit, and the per-tick total is reported without enforcement.
- **Covers:** Z2-I-SCRIPT-19
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-052
- **Budgets:** Z2-B-SCRIPT-001, Z2-B-SCRIPT-002, Z2-B-SCRIPT-018, Z2-B-SCRIPT-019, Z2-B-SCRIPT-020
- **Acceptance evidence:** `Test_Script_Budget_RecoveryAndLoadScopes`, `Test_Script_Budget_DeclarationLowersOnly` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "Protected call implementation"; Zenith2/Docs/Design/SCRIPT.md → "Budget breach behaviour"

### Z2-R-SCRIPT-103 Abort values

- **Statement:** Budget, snapshot-cap, yield-in-system and checkpoint errors are raised as the Zenith abort value, re-raised at every further safepoint of the same entity transaction, and always reach the protected entry.
- **Covers:** Z2-I-SCRIPT-19
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-052
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Budget_ExhaustionRollsBack`, `Test_Script_Sandbox_PcallCannotCatchAbort` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "Interrupt callback implementation"; Zenith2/Docs/Design/SCRIPT/Bindings.md → "Environment construction"

### Z2-R-SCRIPT-104 Native codegen parity

- **Statement:** The Stage 1 harness compares interpreted and native safepoint counts and results over the budget and representative corpora on Windows x64 and the S24, the D-SCRIPT-TXN refinement ruling fixes the native policy per platform, and the parity test re-runs on every Luau pin change.
- **Covers:** Z2-I-SCRIPT-19
- **Stage:** S1
- **Classification:** research
- **Decisions:** Z2-D-052, D-SCRIPT-TXN refinement (open, Stage 1), Z2-D-084
- **Budgets:** none
- **Acceptance evidence:** `Zenith_Bench_ScriptNativeParity` report under `Zenith2/Docs/Research/ScriptNativeParity/`; `Test_Script_Budget_NativeSafepointParity`; the D-SCRIPT-TXN refinement ruling in `DecisionLog.md`
- **Design:** Zenith2/Docs/Design/SCRIPT/Tests.md → "Benchmarks and harnesses"; Zenith2/Docs/Design/SCRIPT.md → "Stage 1"

### Z2-R-SCRIPT-105 Wall-clock watchdog

- **Statement:** A watchdog thread polling every 10 ms trips an invocation running past Z2-B-SCRIPT-003 (fatal world halt at its next safepoint), reports a hang past twice the limit through the crash handler with the Luau traceback, never trips a VM stopped in the debugger, excludes the stopped time from that invocation on resume, decides every trip and hang on a start time read after the stop flag was seen clear, so a stop between its reads is never counted, while the other VMs of the same world stay watched (pending core-doc X158).
- **Covers:** Z2-I-SCRIPT-19
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-052, D-SEL-CRASH (open, Stage 2)
- **Budgets:** Z2-B-SCRIPT-003
- **Acceptance evidence:** `Test_Script_Watchdog_FatalNotRollback`, `Test_Script_Watchdog_SuspendedWhileDebugging`, `Test_Script_Watchdog_StopExemptsOnlyTheStoppedVM` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "Watchdog internals"; Zenith2/Docs/Design/SCRIPT/Runtime.md → "World halt procedure"; Zenith2/Docs/Design/SCRIPT/Debugger.md → "Stop protocol"

### Z2-R-SCRIPT-106 Per-tick script counters reported

- **Statement:** The script runtime publishes each tick the counters of the design (invocations, failures by kind, rejections by class, quarantines, disables, checkpoints, snapshot bytes, safepoints, GC work, VM bytes, arena growths, published-unconsumed results, dropped diagnostics) and emits `SCRIPT_SNAPSHOT_TOTAL_EXCEEDED` above Z2-B-SCRIPT-013 without rolling anything back.
- **Covers:** Z2-I-SCRIPT-19, Z2-I-SCRIPT-20, Z2-I-SCRIPT-21
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-005, Z2-D-052
- **Budgets:** Z2-B-SCRIPT-013, Z2-B-SCRIPT-020, Z2-B-SCRIPT-021
- **Acceptance evidence:** `Test_Script_Profiling_OverlayRows` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT.md → "Counters"; Zenith2/Docs/Design/SCRIPT/Transactions.md → "Per-entity accounting implementation"

### Z2-R-SCRIPT-107 Completion IDs and tickets

- **Statement:** Asynchronous requests are behaviour-only side effects assigned completion IDs at commit in merge order from the world's monotonic counter; a rolled-back request consumes no ID, a provisional ticket is remapped at commit (ZS1105 after its window), and tickets are not serializable state.
- **Covers:** Z2-I-SCRIPT-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** Z2-B-SCRIPT-026
- **Acceptance evidence:** `Test_Script_Async_PublishedAtTickBoundaryInCompletionOrder`, `Test_Script_Txn_AsyncRequestNotFromSystem` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "Asynchronous result table"; Zenith2/Docs/Design/SCRIPT/Transactions.md → "Commit implementation"

### Z2-R-SCRIPT-108 Tick-boundary publication

- **Statement:** Completed results become visible only at tick-boundary step 1, sorted by completion ID, are handed to the publication observer and to the handler of their source kind together with the tick boundary's `Zenith_TickBoundaryAccess`, and a published script result — including a script request of a kind with a C++ handler — stays until consumed by a committed invocation or its owner is released (pending core-doc X118).
- **Covers:** Z2-I-SCRIPT-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** Z2-B-SCRIPT-026
- **Acceptance evidence:** `Test_Script_Async_PublishedAtTickBoundaryInCompletionOrder`, `Test_Script_Async_PlatformKindHandled`, `Test_Script_Async_ResultReleasedOnConsumption` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "Asynchronous result table"; Zenith2/Docs/Design/SCRIPT/PublicAPI.md → "Zenith_ScriptAsync header"; Zenith2/Docs/Design/SCRIPT.md → "Reconciliation items"

### Z2-R-SCRIPT-109 Replay publication at the recorded tick

- **Statement:** Under replay every result is published at its recorded tick, recorded kinds from the recorded payload and recomputed kinds from the live completion, and the runtime runs no tick whose recomputed results are incomplete.
- **Covers:** Z2-I-SCRIPT-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Async_ReplayPublishesAtRecordedTick` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "Asynchronous result table"; Zenith2/Docs/Design/SCRIPT/PublicAPI.md → "Zenith_ScriptHost header"

### Z2-R-SCRIPT-110 Declared state schema registration

- **Statement:** Every behaviour's declared state schema is registered as `Behaviour.<Name>.State` of `TYPE_KIND_BEHAVIOUR_STATE` at startup step 10, after the schema blob and before `Freeze`, from the cooked manifest record through `Zenith_ScriptRegisterBehaviourStateTypes` (the behaviour component staying in the step-6 engine types), the live declaration is checked against it by shape hash at load, and a field ID is never reused in a schema's history (pending core-doc X86, X122).
- **Covers:** Z2-I-SCRIPT-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-023, Z2-D-020
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_State_RegisteredAtStepTen`, `Test_Script_State_CookedSchemaMatchesLive`, `Test_Script_Cook_DeclarationExtraction` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/HotReload.md → "State registration at startup"; Zenith2/Docs/Design/SCRIPT/Compiler.md → "Declaration extraction"

### Z2-R-SCRIPT-111 State schema load errors

- **Statement:** A state field that is a closure, coroutine, foreign userdata or undeclared table, a non-literal state declaration, or a schema whose maximum exceeds Z2-B-SCRIPT-015 fails the cook or the load with the field named.
- **Covers:** Z2-I-SCRIPT-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-023
- **Budgets:** Z2-B-SCRIPT-015
- **Acceptance evidence:** `Test_Script_State_UnserializableKindsLoadError`, `Test_Script_Cook_DeclarationExtraction` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Compiler.md → "Declaration extraction"; Zenith2/Docs/Design/SCRIPT/HotReload.md → "State schema internals"

### Z2-R-SCRIPT-112 State proxy

- **Statement:** The `state` proxy validates every write against the schema (ZS1901 … ZS1906), copies tables by value, sets the dirty flag, keeps live state outside the Luau heap, and restores it when the invocation fails or is rejected.
- **Covers:** Z2-I-SCRIPT-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-023
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_State_NoSharedMutableTables`, `Test_Script_State_LiveStateRestoredOnFailure`, `Test_Script_Misuse_Catalogue` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/HotReload.md → "State proxy implementation"

### Z2-R-SCRIPT-113 Checkpoint rule

- **Statement:** A dirty instance is checkpointed into its inactive slot at most once per tick, at its final invocation of the tick (a checkpoint-only transaction when no invocation ran in the Late node), inside the transaction before commit.
- **Covers:** Z2-I-SCRIPT-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-023
- **Budgets:** Z2-B-SCRIPT-014, Z2-B-SCRIPT-021
- **Acceptance evidence:** `Test_Script_State_CheckpointDirtyOnlyAtMostOncePerTick` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/HotReload.md → "Checkpoint serializer"; Zenith2/Docs/Design/SCRIPT/Runtime.md → "Behaviour node body"

### Z2-R-SCRIPT-114 Checkpoint format

- **Statement:** Checkpoints use the ZBST format of Scripting.md written little-endian field by field, with provisional-handle fixups rewritten in place after playback.
- **Covers:** Z2-I-SCRIPT-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-023, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_State_UnknownFieldsPreserved`, `Test_Script_State_SaveRoundTrip` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/HotReload.md → "Checkpoint serializer"

### Z2-R-SCRIPT-115 Checkpoint failure and slot swap

- **Statement:** A checkpoint that cannot be serialized (more than 16 fixups, or a payload above the slot) raises ZS1404 inside the transaction and rolls it back, and commit makes the checkpoint active by one index flip.
- **Covers:** Z2-I-SCRIPT-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-023, Z2-D-051
- **Budgets:** Z2-B-SCRIPT-015
- **Acceptance evidence:** `Test_Script_State_CheckpointSerializationFailureRollsBack` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/HotReload.md → "Checkpoint serializer"; Zenith2/Docs/Design/SCRIPT/Transactions.md → "Commit implementation"

### Z2-R-SCRIPT-116 Checkpoint slot pools

- **Statement:** Each instance's two checkpoint slots and live-state block come from the pool of its module's current generation, reserved at instance creation outside any transaction or, for a reload, in the prologue from the new generation's pool, pools are touched only on the main thread, and a pool growth failure halts the world.
- **Covers:** Z2-I-SCRIPT-10, Z2-I-SCRIPT-20
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-023
- **Budgets:** Z2-B-SCRIPT-015
- **Acceptance evidence:** `Test_Script_Module_BehaviourDirectory` (slot accounting) in the `Test` step and the `Zenith2Memory` lines (`Test -Label memory`, `Sanitizers -Preset win-asan`, `Sanitizers -Preset linux-tsan`)
- **Design:** Zenith2/Docs/Design/SCRIPT/HotReload.md → "Checkpoint slot pools"

### Z2-R-SCRIPT-117 Restore algorithm

- **Statement:** Restore verifies magic, version and source ID, migrates older versions, decodes fields by ID preserving unknown fields, resolves references and schedules OnRecover, and never deserializes a function, coroutine or execution position.
- **Covers:** Z2-I-SCRIPT-10, Z2-I-SCRIPT-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-023, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_State_UnknownFieldsPreserved`, `Test_Script_Recovery_OnRecoverRestartsCoroutine` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/HotReload.md → "Restore implementation"

### Z2-R-SCRIPT-118 Stale references on restore

- **Statement:** On restore entity references resolve through persistent IDs (or simulation IDs within the session) and asset references through GUIDs; anything unresolvable becomes nil with `SCRIPT_STATE_REFERENCE_UNRESOLVED`.
- **Covers:** Z2-I-SCRIPT-10, Z2-I-SCRIPT-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-023
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_State_RestoreDestroyedReferenceBecomesNil` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/HotReload.md → "Restore implementation"

### Z2-R-SCRIPT-119 OnRecover

- **Statement:** OnRecover runs on the main VM in the instance's declared phase of the first tick after recovery, before its other callbacks, as its own entity transaction under Z2-B-SCRIPT-018, starts the returned function (or Run) as a fresh coroutine, and a failure re-quarantines the instance with its checkpoint intact.
- **Covers:** Z2-I-SCRIPT-11
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-023
- **Budgets:** Z2-B-SCRIPT-018
- **Acceptance evidence:** `Test_Script_Recovery_OnRecoverRestartsCoroutine`, `Test_Script_Recovery_QuarantineThenReloadLastGood` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "Behaviour node body"; Zenith2/Docs/Design/SCRIPT/HotReload.md → "Last-good restore"

### Z2-R-SCRIPT-120 Behaviour state in saves

- **Statement:** SAVE reads every instance's active checkpoint and the next completion ID in phase 11, and `RestoreSavedStates` restores them into a loaded world with OnRecover, skipping and reporting entries whose entity or module is missing.
- **Covers:** Z2-I-SCRIPT-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-023, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_State_SaveRoundTrip` in the `Test` step (label `script-conformance`); save/load with migration fixtures in the Stage 3 acceptance ticket
- **Design:** Zenith2/Docs/Design/SCRIPT/HotReload.md → "Behaviour state in saves"; Zenith2/Docs/Design/SCRIPT/PublicAPI.md → "Zenith_ScriptStateSnapshot header"

### Z2-R-SCRIPT-121 Checkpoint decoder is fuzzed

- **Statement:** The checkpoint decoder bounds-checks every length and count against the schema and blob before reading, never allocates beyond the slot, reports malformed input as a restore failure, and is fuzzed under win-asan.
- **Covers:** Z2-I-SCRIPT-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Zenith_Fuzz_Script_CheckpointDecode` smoke run in `Sanitizers -Preset win-asan`
- **Design:** Zenith2/Docs/Design/SCRIPT/HotReload.md → "Restore implementation"; Zenith2/Docs/Design/SCRIPT/Tests.md → "Fuzz targets"

### Z2-R-SCRIPT-122 Quarantine

- **Statement:** A failing behaviour instance is quarantined: its Luau objects, retained callbacks and unconsumed results are released, its pending events dropped with a count, and its component data and active checkpoint kept.
- **Covers:** Z2-I-SCRIPT-12
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-023
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Recovery_QuarantineThenReloadLastGood` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Transactions.md → "Quarantine store"; Zenith2/Docs/Design/SCRIPT.md → "Behaviour instance state machine"

### Z2-R-SCRIPT-123 Recovery policy

- **Statement:** The recovery policy comes from game.json's `scriptRecovery` (manual outside Shipping and automatic 60/3 in Shipping when absent); the automatic policy recovers a disabled module after the delay and stops after the per-session limit with a diagnostic.
- **Covers:** Z2-I-SCRIPT-12, Z2-I-SCRIPT-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-023
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Recovery_AutomaticPolicyLimit`, `Test_Script_Recovery_DefaultsPerConfiguration` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/HotReload.md → "Quarantine and recovery scheduling"; Zenith2/Docs/Design/SCRIPT/PublicAPI.md → "Zenith_ScriptRecovery header"

### Z2-R-SCRIPT-124 Last-good restore

- **Statement:** A recovery restores every instance of the module from its active checkpoint at a tick boundary, schedules OnRecover and re-enables the module, and state from a failed or rejected transaction never reaches a checkpoint.
- **Covers:** Z2-I-SCRIPT-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-023
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Recovery_QuarantineThenReloadLastGood` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/HotReload.md → "Last-good restore"

### Z2-R-SCRIPT-125 Hot-reload producers

- **Statement:** Every hot-reload producer compiles with the production options and runs the stateless check, and `RequestHotReload` refuses simulation bytecode lacking the stateless-verified flag or failing header verification.
- **Covers:** Z2-I-SCRIPT-14, Z2-I-SCRIPT-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048, D-LUAU-STATELESS (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_HotReload_UnverifiedBytecodeRefused`, `Test_Script_Bytecode_DevAndCookIdentical` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/HotReload.md → "Hot-reload staging area"; Zenith2/Docs/Design/SCRIPT/HotReload.md → "Development reload sources"

### Z2-R-SCRIPT-126 Staged hot reload with rollback

- **Statement:** `ApplyPendingReloads` stages every queued reload inside the registry window ZenithRuntime opened, and any failure of load, declaration, state registration, migration, restore, window validation or schedule commit — each detected in prologue step 6 before anything goes live — releases the staged objects exactly once, releases nothing else, and leaves every module, instance and the schedule exactly as before.
- **Covers:** Z2-I-SCRIPT-14, Z2-I-SCRIPT-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-023, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_HotReload_RollbackOnError`, `Test_Script_HotReload_WindowNotPublishedDropsStaging`, `Test_Script_HotReload_ScheduleCommitFailureRollsBack` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/HotReload.md → "ApplyPendingReloads procedure"; Zenith2/Docs/Design/SCRIPT/HotReload.md → "Schedule commit in the prologue"; Zenith2/Docs/Design/SCRIPT/HotReload.md → "Rollback procedure"

### Z2-R-SCRIPT-127 Hot-reload go-live

- **Statement:** A reload whose declaration's access, phase, ordering or system set changed commits through `Zenith_SystemReloadScope` in prologue step 6 so the schedule recompiles in step 8 of the same frame; the reload goes live at the world's next tick boundary step 3 (or, for a committed generation, first thing in an out-of-tick `RestoreSavedStates` or `RequestModuleReload` of the world) and never mid-tick; go-live allocates nothing, copies no bytecode and cannot fail, installs the migrated instances together with checkpoint slots of the new generation that already hold their migrated state at the new `StateVersion` (so no tick-boundary restore after it needs `Migrate`), schedules OnRecover, re-enables the module, queues the old generation's objects and retires its checkpoint elements and buffers exactly once, and changes only `TYPE_KIND_BEHAVIOUR_STATE` types (pending core-doc X158).
- **Covers:** Z2-I-SCRIPT-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-023, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_HotReload_AppliedAtTickBoundaryOnly`, `Test_Script_HotReload_MigrationSucceeds`, `Test_Script_HotReload_AccessChangeScheduledSameFrame`, `Test_Script_HotReload_RecoveryAfterVersionBumpUsesNewCheckpoint` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/HotReload.md → "Schedule commit in the prologue"; Zenith2/Docs/Design/SCRIPT/HotReload.md → "Go-live at the tick boundary"

### Z2-R-SCRIPT-128 Chunk-system reload generations

- **Statement:** A chunk-system reload advances the world's module generation and every worker VM reloads the module before its next invocation of it, and a failed reload leaves the previous generation on every VM.
- **Covers:** Z2-I-SCRIPT-14, Z2-I-SCRIPT-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-018
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_HotReload_SystemGeneration` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/HotReload.md → "Chunk-system generations"

### Z2-R-SCRIPT-129 Migrate chain and fixtures

- **Statement:** Each `Migrate[n]` runs on the main VM with no world access under Z2-B-SCRIPT-018, its result is validated against the next schema (ZS1907), and every step has a per-version fixture the game's migration test loads.
- **Covers:** Z2-I-SCRIPT-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-023, Z2-D-048
- **Budgets:** Z2-B-SCRIPT-018
- **Acceptance evidence:** `Test_Script_HotReload_MigrationSucceeds` in the `Test` step (label `script-conformance`); `Game_<Name>_ScriptMigrations` for every game with a versioned behaviour
- **Design:** Zenith2/Docs/Design/SCRIPT/HotReload.md → "Migration execution"

### Z2-R-SCRIPT-130 Schema script migration host

- **Statement:** SCRIPT implements `Zenith_MigrationScriptHost` with a sandboxed VM of no world access running each schema `script` op under Z2-B-SCRIPT-018, returning `MIGRATION_ERROR_SCRIPT` with the input unchanged on any failure.
- **Covers:** Z2-I-SCRIPT-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-020, Z2-D-048
- **Budgets:** Z2-B-SCRIPT-018
- **Acceptance evidence:** `Test_Script_Migration_ScriptHostOp` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/HotReload.md → "Schema migration script host"

### Z2-R-SCRIPT-131 Script hot-reload latency

- **Statement:** Editor hot-reload latency from save to running code is measured against Z2-B-EDIT-002 on the reference desktop.
- **Covers:** Z2-I-SCRIPT-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-EDIT-002
- **Acceptance evidence:** `Zenith_Bench_ScriptHotReload` report; the W-Editor measurement of the Stage 3 acceptance ticket
- **Design:** Zenith2/Docs/Design/SCRIPT/HotReload.md → "Development reload sources"; Zenith2/Docs/Design/SCRIPT/Tests.md → "Benchmarks and harnesses"

### Z2-R-SCRIPT-132 Stateless check interface

- **Statement:** `Zenith_ScriptCheckStateless` presents one interface for every alternative, runs on every chunk-system, behaviour and required library module at cook and in every hot-reload producer, and a rejection fails the cook with its diagnostic, line and detail.
- **Covers:** Z2-I-SCRIPT-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-LUAU-STATELESS (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Stateless_AdversarialCorpusRejectedAtLoad`, `Test_Script_HotReload_UnverifiedBytecodeRefused` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Compiler.md → "Cook pipeline"; Zenith2/Docs/Design/SCRIPT/PublicAPI.md → "Zenith_ScriptStateless header"; Zenith2/Docs/Design/SCRIPT/Compiler.md → "Stateless check prototype"

### Z2-R-SCRIPT-133 Adversarial corpus rejected at load

- **Statement:** Every module of the adversarial corpus (at least the 24 modules covering the 21 patterns) is rejected at load time with its expected diagnostic and line, and the corpus only grows.
- **Covers:** Z2-I-SCRIPT-09
- **Stage:** S1
- **Classification:** required
- **Decisions:** D-LUAU-STATELESS (open, Stage 1), Z2-D-018
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Stateless_AdversarialCorpusRejectedAtLoad` in the `Test` step (label `script-conformance`); the Stage 1 acceptance ticket
- **Design:** Zenith2/Docs/Design/SCRIPT/Compiler.md → "Stateless check prototype"; Zenith2/Docs/Design/SCRIPT/Tests.md → "Compiler and cook tests"

### Z2-R-SCRIPT-134 Accepted corpus loads

- **Statement:** Every module of the accepted corpus and every representative module loads and runs under the mechanism, a false rejection being a defect of the mechanism.
- **Covers:** Z2-I-SCRIPT-09
- **Stage:** S1
- **Classification:** required
- **Decisions:** D-LUAU-STATELESS (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Stateless_AcceptedCorpusLoads`, `Test_Script_Stateless_RepresentativeCorpusAccepted` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Compiler.md → "Stateless check prototype"; Zenith2/Docs/Design/SCRIPT/Tests.md → "Compiler and cook tests"

### Z2-R-SCRIPT-135 D-LUAU-STATELESS evaluation and ruling

- **Statement:** The Stage 1 harness measures every criterion of the alternative evaluation procedure for each prototype, and the ruling ticket appends the selected mechanism with the measurements to `DecisionLog.md`, the selection meeting every threshold.
- **Covers:** Z2-I-SCRIPT-09
- **Stage:** S1
- **Classification:** research
- **Decisions:** D-LUAU-STATELESS (open, Stage 1), Z2-D-084, Z2-D-097
- **Budgets:** none
- **Acceptance evidence:** `Zenith_Bench_ScriptStateless` report under `Zenith2/Docs/Research/ScriptStateless/`; the ruling entry in `DecisionLog.md`
- **Design:** Zenith2/Docs/Design/SCRIPT/Compiler.md → "Stateless alternatives prototypes"; Zenith2/Docs/Design/SCRIPT.md → "Stage 1"

### Z2-R-SCRIPT-136 Behaviour modules share no mutable tables

- **Statement:** Behaviour modules are checked by the same mechanism, and a module-scope table stored into instance state or self is rejected with `SCRIPT_STATELESS_SHARED_INSTANCE_TABLE`.
- **Covers:** Z2-I-SCRIPT-09, Z2-I-SCRIPT-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-LUAU-STATELESS (open, Stage 1), Z2-D-023
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Stateless_AdversarialCorpusRejectedAtLoad` (A21, A24), `Test_Script_State_NoSharedMutableTables` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Compiler.md → "Stateless check prototype"

### Z2-R-SCRIPT-137 Stateful library functions removed

- **Statement:** `math.random`, `math.randomseed` and the `os` library are stubs in both simulation environments whatever the D-LUAU-STATELESS ruling, and referencing them is a stateless rejection.
- **Covers:** Z2-I-SCRIPT-09, Z2-I-SCRIPT-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-LUAU-STATELESS (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Sandbox_EnvironmentSubset`, `Test_Script_Stateless_AdversarialCorpusRejectedAtLoad` (A10, A11) in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Bindings.md → "Environment construction"; Zenith2/Docs/Design/SCRIPT/Compiler.md → "Stateless check prototype"

### Z2-R-SCRIPT-138 Rejected libraries reject their importers

- **Statement:** A library module is usable by systems and behaviours only after it passed the check as a library, and a module requiring a rejected library is rejected with `SCRIPT_STATELESS_IMPORTED_STATE` naming it.
- **Covers:** Z2-I-SCRIPT-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-LUAU-STATELESS (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Stateless_AdversarialCorpusRejectedAtLoad` (A05, A06) in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Compiler.md → "Cook pipeline"; Zenith2/Docs/Design/SCRIPT/Compiler.md → "Stateless check prototype"

### Z2-R-SCRIPT-139 DAP server exposure and threads

- **Statement:** The DAP server exists only in configurations other than Shipping, binds the loopback interface through PLAT's listener with its session-token handshake, and exposes every VM of every world as a DAP thread with ID world tag × 256 + VM index (pending core-doc X121 for the tag).
- **Covers:** Z2-I-SCRIPT-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-LUAUDBG (open, Stage 1), Z2-D-075
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Dap_TokenRequired`, `Test_Script_Dap_SessionRequests` in the `Test` step (label `script-conformance`); `Test_Script_Dap_AbsentFromShipping` in the `Libgraph` step
- **Design:** Zenith2/Docs/Design/SCRIPT/Debugger.md → "Scope of this file"; Zenith2/Docs/Design/SCRIPT/Debugger.md → "Transport and framing"; Zenith2/Docs/Design/SCRIPT/Debugger.md → "Request handling"

### Z2-R-SCRIPT-140 Breakpoint fan-out

- **Statement:** Every VM of a world applies each breakpoint change before its next invocation, a VM created or a module loaded afterwards applies the current set — a worker VM only after the module's scope returned, so a worker VM never stops outside an invocation — and the DAP thread publishes each set without holding a lock.
- **Covers:** Z2-I-SCRIPT-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-LUAUDBG (open, Stage 1), Z2-D-018
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Dap_BreakpointFanOut`, `Test_Script_Dap_WorkerModuleScopeNeverStops` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Debugger.md → "Breakpoint storage and fan-out"; Zenith2/Docs/Design/SCRIPT/Debugger.md → "Stop protocol"

### Z2-R-SCRIPT-141 Debugger stops change no result

- **Statement:** A debugger stop suspends the world's script dispatch, never trips the watchdog on the stopped VM however long it lasts, and changes no safepoint count or committed value, so a debugged replay produces the undebugged world-state hashes (the stop's blocking wait is the bracketed exemption of Q44 ruling (a), pending core-doc X120).
- **Covers:** Z2-I-SCRIPT-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-LUAUDBG (open, Stage 1), Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Dap_BreakpointFanOut` (hash comparison), `Test_Script_Watchdog_SuspendedWhileDebugging`, `Test_Script_Dap_ParkedInvocationsResume`, `Test_Script_Dap_RestopDuringReleaseStaysSuspended` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Debugger.md → "Stop protocol"; Zenith2/Docs/Design/SCRIPT/Debugger.md → "Stop loop"; Zenith2/Docs/Design/SCRIPT/Debugger.md → "Debugger and simulation dispatch"

### Z2-R-SCRIPT-142 D-LUAUDBG client and request set

- **Statement:** The D-LUAUDBG ruling records the selected client, a request set including at least setBreakpoints, configurationDone, threads, stackTrace, scopes, variables, continue, next, stepIn, stepOut, pause and a locals-only evaluate, and the Windows and S24 attach workflows; the server implements that set.
- **Covers:** Z2-I-SCRIPT-06
- **Stage:** S1
- **Classification:** required
- **Decisions:** D-LUAUDBG (open, Stage 1), Z2-D-084, Z2-D-097
- **Budgets:** none
- **Acceptance evidence:** the D-LUAUDBG ruling entry in `DecisionLog.md`; `Test_Script_Dap_SessionRequests`, `Test_Script_Dap_StepAcrossYield` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Debugger.md → "Request handling"; Zenith2/Docs/Design/SCRIPT/Debugger.md → "Stepping"; Zenith2/Docs/Design/SCRIPT/Debugger.md → "Evaluate restrictions"; Zenith2/Docs/Design/SCRIPT.md → "Stage 1"

### Z2-R-SCRIPT-143 S24 debugger attach

- **Statement:** The selected client attaches to a development build on the S24 over `adb forward tcp:27851 tcp:27851` with the logged token and stops at a breakpoint on a worker VM and the main VM.
- **Covers:** Z2-I-SCRIPT-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-LUAUDBG (open, Stage 1), Z2-D-010
- **Budgets:** none
- **Acceptance evidence:** the S24 debugger device run record of the Stage 3 acceptance ticket
- **Design:** Zenith2/Docs/Design/SCRIPT/Debugger.md → "Attaching on Windows and the S24"

### Z2-R-SCRIPT-144 Profiling zones

- **Statement:** Every script node emits a zone, every invocation a nested zone named by its module with its entity count or simulation ID, and merges, checkpoints, restores, hot reloads and GC steps their own zones.
- **Covers:** Z2-I-SCRIPT-21
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Profiling_OverlayRows` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "Profiling implementation"; Zenith2/Docs/Design/SCRIPT.md → "Profiling zones summary"

### Z2-R-SCRIPT-145 On-screen profiler rows

- **Statement:** The Luau time, Luau GC and script counter rows are populated in the game and the editor on Windows and the S24 behind the runtime toggle from SCRIPT's zones and counters.
- **Covers:** Z2-I-SCRIPT-21
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** Z2-B-SCRIPT-004, Z2-B-SCRIPT-005, Z2-B-SCRIPT-006, Z2-B-SCRIPT-008
- **Acceptance evidence:** `Test_Script_Profiling_OverlayRows` in the `Test` step (label `script-conformance`); the on-screen profiler check of the Stage 3 acceptance ticket
- **Design:** Zenith2/Docs/Design/SCRIPT.md → "On-screen profiler rows summary"; Zenith2/Docs/Design/SCRIPT.md → "Counters"

### Z2-R-SCRIPT-146 Sampling profiler

- **Statement:** In development builds a runtime toggle samples every VM's Luau stack every 997th safepoint into a 65,536-sample ring and dumps a flame-graph JSON, without changing any safepoint count or result.
- **Covers:** Z2-I-SCRIPT-21
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-005, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Profiling_SamplerNoEffect` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Debugger.md → "Sampling profiler implementation"

### Z2-R-SCRIPT-147 Misuse errors

- **Statement:** Every script misuse raises a controlled Luau error whose message is exactly `ZS<code>: <text>` of the catalogue after the position prefix, never an assert, crash or undefined behaviour, in every build.
- **Covers:** Z2-I-SCRIPT-07, Z2-I-SCRIPT-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Misuse_Catalogue`, `Test_Script_Misuse_NeverAsserts` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT.md → "Luau errors versus asserts"; Zenith2/Docs/Design/SCRIPT/Runtime.md → "Protected call implementation"

### Z2-R-SCRIPT-148 Misuse catalogue trigger scripts

- **Statement:** Every catalogue row has a trigger script under `ZenithScript/Tests/Misuse/<Code>.luau` (or a scenario for non-Luau diagnostics), and a new misuse check adds its row and trigger in the same diff.
- **Covers:** Z2-I-SCRIPT-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Misuse_Catalogue` in the `Test` step (label `script-conformance`); review
- **Design:** Zenith2/Docs/Design/SCRIPT/Tests.md → "Module binding and sandbox tests"; Zenith2/Docs/Design/SCRIPT.md → "Test kinds and locations"

### Z2-R-SCRIPT-149 Input action names resolved at load

- **Statement:** An `Input` binding naming an action or context the loaded bindings do not declare fails its module's load with the catalogue row ZS1807, which the input-binding ticket adds to the catalogue.
- **Covers:** Z2-I-SCRIPT-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Input_UnknownActionAtLoad` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Bindings.md → "Load-time name references"

### Z2-R-SCRIPT-150 World halt reporting

- **Statement:** Every fatal script event halts only its world, abandons the tick without committing, calls the world listener exactly once, and makes subsequent requests to the world return `SCRIPT_ERROR_WORLD_HALTED`.
- **Covers:** Z2-I-SCRIPT-19, Z2-I-SCRIPT-20
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-052
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Firewall_InternalExceptionIsFatal`, `Test_Script_VM_MemoryCapFatal`, `Test_Script_Watchdog_FatalNotRollback` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "World halt procedure"; Zenith2/Docs/Design/SCRIPT.md → "Script world state machine"; Zenith2/Docs/Design/SCRIPT/PublicAPI.md → "Zenith_ScriptHost header"

### Z2-R-SCRIPT-151 Representative game modules load

- **Statement:** The Luau gameplay modules of the Stage 3 and Stage 4 games, including OpenWorldSlice's quest, dialogue and inventory samples, cook and load unchanged under the stateless mechanism.
- **Covers:** Z2-I-SCRIPT-09, Z2-I-SCRIPT-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-LUAU-STATELESS (open, Stage 1), Z2-D-077
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Stateless_RepresentativeCorpusAccepted` over every repository game in the `Test` step; the Stage 4 acceptance ticket
- **Design:** Zenith2/Docs/Design/SCRIPT.md → "Stage 4"; Zenith2/Docs/Design/SCRIPT/Tests.md → "Compiler and cook tests"

### Z2-R-SCRIPT-152 Gameplay rules stay game-side

- **Statement:** Quest, dialogue, inventory and interaction rules exist only as Luau modules inside `Games/OpenWorldSlice/Scripts/`, and no engine library implements them.
- **Covers:** Z2-I-SCRIPT-08
- **Stage:** S4
- **Classification:** excluded
- **Decisions:** Z2-D-077
- **Budgets:** none
- **Acceptance evidence:** review of the OpenWorldSlice tickets; the `Libgraph` step (no engine target under Games/); the GAME area owns the Luau samples themselves
- **Design:** Zenith2/Docs/Design/SCRIPT.md → "What SCRIPT does not own"; Zenith2/Docs/Design/SCRIPT.md → "Stage 4"

### Z2-R-SCRIPT-153 Stage 1 script budget allocation

- **Statement:** The Stage 1 per-system budget ticket for SCRIPT allocates every Z2-B-SCRIPT row with the procedures of Budgets.md from `Zenith_Bench_BudgetsScript` runs on the reference desktop and the S24, after the D-SCRIPT-TXN refinement ruling.
- **Covers:** Z2-I-SCRIPT-18, Z2-I-SCRIPT-19, Z2-I-SCRIPT-20
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-012, D-SCRIPT-TXN refinement (open, Stage 1)
- **Budgets:** Z2-B-SCRIPT-001, Z2-B-SCRIPT-002, Z2-B-SCRIPT-003, Z2-B-SCRIPT-004, Z2-B-SCRIPT-005, Z2-B-SCRIPT-006, Z2-B-SCRIPT-007, Z2-B-SCRIPT-008, Z2-B-SCRIPT-009, Z2-B-SCRIPT-010, Z2-B-SCRIPT-011, Z2-B-SCRIPT-012, Z2-B-SCRIPT-013, Z2-B-SCRIPT-014, Z2-B-SCRIPT-015, Z2-B-SCRIPT-016, Z2-B-SCRIPT-017, Z2-B-SCRIPT-018, Z2-B-SCRIPT-019, Z2-B-SCRIPT-020, Z2-B-SCRIPT-021, Z2-B-SCRIPT-022, Z2-B-SCRIPT-023, Z2-B-SCRIPT-024, Z2-B-SCRIPT-025, Z2-B-SCRIPT-026
- **Acceptance evidence:** the allocation table under `Zenith2/Docs/Research/Budgets/SCRIPT/` and the DecisionLog entry of the budget ticket; the Stage 1 acceptance ticket (every row `adopted`)
- **Design:** Zenith2/Docs/Design/SCRIPT.md → "Rows SCRIPT owns"; Zenith2/Docs/Design/SCRIPT.md → "Row needs recorded as pending"; Zenith2/Docs/Design/SCRIPT/Tests.md → "Benchmarks and harnesses"

### Z2-R-SCRIPT-154 No third-party script loading

- **Statement:** No runtime path loads script code from outside the package, patch and (development) loose mounts, and no mod or third-party script is ever run.
- **Covers:** Z2-I-SCRIPT-07
- **Stage:** S6
- **Classification:** excluded
- **Decisions:** Z2-D-075
- **Budgets:** none
- **Acceptance evidence:** the security hardening evidence of the Stage 6 acceptance ticket; `Test_Script_Sandbox_NoCapabilityBindings`
- **Design:** Zenith2/Docs/Design/SCRIPT.md → "What SCRIPT does not own"; Zenith2/Docs/Design/SCRIPT/Compiler.md → "Load-time verification procedure"

### Z2-R-SCRIPT-155 Debugger stop bracket and parking

- **Statement:** In development builds a debugger stop blocks its thread only inside `Zenith_MarkThreadAsJobWorker(false)` … a restore of the job-worker flag the thread had on entry, with the blocking-wait hook told `"debugger-stop"`; a step is consumed when it stops, so a `continue` after a step runs free; every decrement of the world's stopped count is guarded so it never goes below zero; each world's mailbox slots exist from world creation, so a release reaches a worker VM created in the stopped tick; every other script invocation of the stopped world parks at the world's gate before it starts (never inside one); each stop increments the world's stopped count once, and every change of the count, its notify and each parker's predicate check happen under the DAP mutex, including a stepping VM's exit from the count inside the bracket; the release clears every VM's pause flag; no thread holding the DAP mutex takes a lower lock; no merge of the stopped or parked nodes commits until the release; and every blocked thread resumes after one release whatever the number of blocked frame threads (Q44 ruling (a), pending core-doc X120).
- **Covers:** Z2-I-SCRIPT-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-LUAUDBG (open, Stage 1), Z2-D-019, Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Dap_StopBracketsJobWorkerFlag`, `Test_Script_Dap_ParkedInvocationsResume` (1, 2 and 8 frame threads, a same-tick worker VM, two stopped worlds), `Test_Script_Dap_ContinueAfterStepRunsFree`, `Test_Script_Dap_WorkerModuleScopeNeverStops`, `Test_Script_Dap_StepOutOfInvocationContinuesWorld`, `Test_Script_Dap_StepCompletionWakesParked`, `Test_Script_Dap_RestopDuringReleaseStaysSuspended`, `Test_Script_Dap_PauseClearedOnContinue`, `Test_Script_Dap_ConnectionLossReleases` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Debugger.md → "Stop protocol"; Zenith2/Docs/Design/SCRIPT/Debugger.md → "Stop loop"; Zenith2/Docs/Design/SCRIPT.md → "Lock-order entries"

### Z2-R-SCRIPT-156 Watchdog table capacity and world index

- **Statement:** The watchdog table holds `m_uMaxWorlds × (1 + frame threads)` entries (at most 4,096), a world takes a world index and a block of entries when its main VM registers, a world beyond `m_uMaxWorlds` fails creation with `SCRIPT_ERROR_WATCHDOG_TABLE_FULL`, a worker VM registration never fails, and the watchdog has no world-level suspension: a debugger stop exempts only the stopped VM through its own flag (pending core-doc X121, X158).
- **Covers:** Z2-I-SCRIPT-19, Z2-I-SCRIPT-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-052
- **Budgets:** Z2-B-SCRIPT-003
- **Acceptance evidence:** `Test_Script_Watchdog_TableFull`, `Test_Script_Watchdog_StopExemptsOnlyTheStoppedVM`, `Test_Script_World_CreateFaultSweep` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "Watchdog internals"; Zenith2/Docs/Design/SCRIPT/Runtime.md → "Script world internals"; Zenith2/Docs/Design/SCRIPT/PublicAPI.md → "Zenith_ScriptTypes header"

### Z2-R-SCRIPT-157 Debugger optional at runtime creation

- **Statement:** A DAP listener that cannot be created leaves the script runtime without a debugger, with `IsDebuggerAvailable()` false and the diagnostic `SCRIPT_DEBUGGER_UNAVAILABLE`, unless `Zenith_ScriptDebuggerDesc::m_bRequired` is set, in which case `Zenith_ScriptRuntime::Create` fails with `SCRIPT_ERROR_DEBUGGER_UNAVAILABLE` and leaks nothing.
- **Covers:** Z2-I-SCRIPT-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** D-LUAUDBG (open, Stage 1), Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Runtime_DebuggerOptional`, `Test_Script_World_CreateFaultSweep` (`…_RUNTIME_DEBUGGER`) in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "Script runtime internals"; Zenith2/Docs/Design/SCRIPT/PublicAPI.md → "Zenith_ScriptRuntime header"; Zenith2/Docs/Design/SCRIPT/Debugger.md → "Scope of this file"

### Z2-R-SCRIPT-158 Partial module load failure

- **Statement:** `LoadModules` succeeds while the world is usable, disabling each module that fails to load for the session with a stored diagnostic and one `OnModuleDisabled`, and fails only when the manifest is unusable or no behaviour or chunk-system module of a non-empty manifest loaded.
- **Covers:** Z2-I-SCRIPT-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Module_LoadModulesPartialFailure` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "Script world internals"; Zenith2/Docs/Design/SCRIPT.md → "Missing and corrupt input"

### Z2-R-SCRIPT-159 Unconsumed asynchronous results traced

- **Statement:** The script runtime counts published-unconsumed results per source kind and, when a request is rejected at the merge because its source kind is full while that kind's oldest published-unconsumed result is at least 600 ticks old, emits `SCRIPT_ASYNC_CAPACITY_HELD` naming that result's module and simulation ID (pending core-doc X118, X121).
- **Covers:** Z2-I-SCRIPT-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** Z2-B-SCRIPT-026
- **Acceptance evidence:** `Test_Script_Async_UnconsumedCapacityHeldDiagnostic` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "Asynchronous result table"; Zenith2/Docs/Design/SCRIPT.md → "Counters"

### Z2-R-SCRIPT-160 No script console in Shipping

- **Statement:** In Shipping `ConsoleEvaluate` returns `SCRIPT_ERROR_INVALID_DESC` without touching a VM, and the development configuration keys are registered with their disabled defaults and never read.
- **Covers:** Z2-I-SCRIPT-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-075
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_World_ConsoleEvaluateShipping` in the Shipping configuration of the test target in the `Test` step; the `Libgraph` Shipping-closure check
- **Design:** Zenith2/Docs/Design/SCRIPT/PublicAPI.md → "Zenith_ScriptWorld header"; Zenith2/Docs/Design/SCRIPT.md → "Configuration keys"

### Z2-R-SCRIPT-161 Merge and tick-boundary work touch no Luau object

- **Statement:** A script system's merge node and SCRIPT's tick-boundary handler run with the ECS `ANY` affinity, touch no Luau object and call no `MAIN_THREAD` member of another library: quarantine happens at failure in the behaviour node, module disables are SCRIPT's own flag, and main-VM releases and fresh `self` and state proxies of a tick boundary are deferred to the main thread (the module's next behaviour node, the prologue's drain).
- **Covers:** Z2-I-SCRIPT-08, Z2-I-SCRIPT-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-018, Z2-D-051
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Merge_BehaviourMergeOffMainThread`, `Test_Script_HotReload_NoLuauAtTickBoundary` in the `Test` step and the `Zenith2Memory` lines (`Test -Label memory`, `Sanitizers -Preset win-asan`, `Sanitizers -Preset linux-tsan`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Transactions.md → "Merge node body"; Zenith2/Docs/Design/SCRIPT/Runtime.md → "Tick-boundary handler"; Zenith2/Docs/Design/SCRIPT/HotReload.md → "Go-live at the tick boundary"

### Z2-R-SCRIPT-162 Retention ceiling derived from existing rows

- **Statement:** Each transaction retention arena's cap is computed from the world's Z2-B-SCRIPT-012, entity capacity and Z2-B-GAME-003 as round_up(max(2 × the used bound + one 256 KiB block, the slot's worst case W + 64), 256 KiB) (overflow-checked, saturating at 2^64 − 1), so it bounds the reserved bytes the arena's cap checks and admits the entry-0 pre-reservation in every world, no budget row exists for it, and a retention arena reaching it is a `ZENITH_CHECK`, never a script error, a `RESERVATION` halt or a diagnostic.
- **Covers:** Z2-I-SCRIPT-08, Z2-I-SCRIPT-20
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-051, Z2-D-012, D-SCRIPT-TXN refinement (open, Stage 1)
- **Budgets:** Z2-B-SCRIPT-012, Z2-B-SCRIPT-013, Z2-B-GAME-003
- **Acceptance evidence:** `Test_Script_Txn_RetentionCeilingDerived` in the `Test` step and the `Zenith2Memory` lines (`Test -Label memory`, `Sanitizers -Preset win-asan`, `Sanitizers -Preset linux-tsan`); `Test_Script_Txn_RetentionCeilingIsCheck` in the `Test` step (`z2-death-assert` suite)
- **Design:** Zenith2/Docs/Design/SCRIPT/Transactions.md → "Retention ceiling derivation"; Zenith2/Docs/Design/SCRIPT/Transactions.md → "Transaction arena partition"; Zenith2/Docs/Design/SCRIPT.md → "Row needs recorded as pending"

### Z2-R-SCRIPT-163 Schedule commit in the prologue

- **Statement:** `Zenith_ScriptRuntime::CommitPendingReloads` runs on the main thread in prologue step 6 of every frame after the hot-reload window closed (or in its place), opens a `Zenith_SystemReloadScope` only there, rolls back the staged reloads of a window that did not publish and of a world whose `Commit` fails, and marks the rest committed for the next tick boundary (pending core-doc X158, MOD SC24).
- **Covers:** Z2-I-SCRIPT-14, Z2-I-SCRIPT-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-023, Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_HotReload_AccessChangeScheduledSameFrame`, `Test_Script_HotReload_WindowNotPublishedDropsStaging`, `Test_Script_HotReload_ScheduleCommitFailureRollsBack` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/HotReload.md → "Schedule commit in the prologue"; Zenith2/Docs/Design/SCRIPT/Runtime.md → "Node registration and frame-graph placement"; Zenith2/Docs/Design/SCRIPT/PublicAPI.md → "Zenith_ScriptRuntime header"

### Z2-R-SCRIPT-164 Main-VM release list drained and bounded

- **Statement:** Main-VM registry references released at a tick boundary are released with `lua_unref` on the main thread by the world's next behaviour node, by `CommitPendingReloads` in every frame and before `lua_close` at world destruction, so the list holds at most the references released since the last drain, also in a world without an enabled behaviour module.
- **Covers:** Z2-I-SCRIPT-16, Z2-I-SCRIPT-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-018
- **Budgets:** Z2-B-SCRIPT-009
- **Acceptance evidence:** `Test_Script_Runtime_ReleaseListDrainedWithoutBehaviours`, `Test_Script_HotReload_NoLuauAtTickBoundary` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "Script world internals"; Zenith2/Docs/Design/SCRIPT/Runtime.md → "Behaviour node body"; Zenith2/Docs/Design/SCRIPT/HotReload.md → "Schedule commit in the prologue"

### Z2-R-SCRIPT-165 Invocations of one instance compose container writes

- **Statement:** A behaviour instance's subsequent invocation in the same node creates its container shadow from the earlier invocation's dirty shadow, so both invocations' container mutations commit in merge order, and the subsequent one is rejected with an earlier failed or rejected one.
- **Covers:** Z2-I-SCRIPT-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-051
- **Budgets:** Z2-B-SCRIPT-012
- **Acceptance evidence:** `Test_Script_Txn_SubsequentInvocationSeesEarlierShadow` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Transactions.md → "Copy-on-write shadow implementation"

### Z2-R-SCRIPT-166 Script results of handled source kinds and decoded tickets

- **Statement:** A script request of a source kind that has a C++ publication handler (kind 8, `ASYNC_SOURCE_SCENE_LOAD`, declared with 8 result bytes) keeps its publication in the published table for its ticket after the handler ran, while a C++ request of that kind is released after the handler; `ticket:Result()` of a kind with a registered result decoder returns the decoder's table, for kind 8 `{ instance, ok, error }` decoded from the 8-byte `Zenith_SceneLoadResult`, and an undecodable payload halts the world (cross-area row S22).
- **Covers:** Z2-I-SCRIPT-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** Z2-B-SCRIPT-026
- **Acceptance evidence:** `Test_Script_Async_HandledResultKeptForScriptTicket` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "Asynchronous result table"; Zenith2/Docs/Design/SCRIPT/PublicAPI.md → "Zenith_ScriptBinding header"; Zenith2/Docs/Design/SCRIPT/Transactions.md → "Commit implementation"

### Z2-R-SCRIPT-167 Recovery invocations consume no invocation ordinal

- **Statement:** A recovery invocation (`OnRecover`) runs as its own entity transaction under the reserved ordinal `uSCRIPT_RECOVERY_ORDINAL` (0xFFFFFFFF), consumes no invocation ordinal, so the instance's other invocations of that tick keep the ordinals, merge keys and random draws of a tick without the recovery, records its commands, events, side effects and draws under that reserved ordinal, and is accepted or rejected at the merge before the instance's ordinary invocations of the node; only an instance whose behaviour declares `OnRecover` is ever given one (orchestrator ruling Q82 (b), pending cross-doc row X267).
- **Covers:** Z2-I-SCRIPT-11, Z2-I-SCRIPT-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-023, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Recovery_NoOrdinalConsumed`, `Test_Script_Recovery_AcceptedBeforeOrdinaryInvocations`, `Test_Script_Recovery_NoOnRecoverNoInvocation` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/HotReload.md → "Recovery invocations and exact restore"; Zenith2/Docs/Design/SCRIPT/Runtime.md → "Behaviour node body"; Zenith2/Docs/Design/SCRIPT/Transactions.md → "Merge node body"

### Z2-R-SCRIPT-168 Saved behaviour state restores its recorded lifecycle

- **Statement:** `GetStateSnapshot` holds one entry per behaviour instance past NEW (an empty checkpoint for an instance with no committed checkpoint) with the flags `m_bRunSuspended`, `m_bRecoveryPending` and `m_bQuarantinedOrDisabled`; `RestoreSavedStates` gives an entry a recovery invocation only when its behaviour declares `OnRecover` and one of those flags is set, and otherwise restores it ACTIVE or DORMANT with no recovery invocation and no coroutine, so a restore of a snapshot with no suspended `Run` coroutine and no quarantined or disabled entry continues bit-identically to the uninterrupted run (orchestrator ruling Q82 (b), pending cross-doc row X267).
- **Covers:** Z2-I-SCRIPT-11, Z2-I-SCRIPT-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-023, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_State_SnapshotFlagsRecorded`, `Test_Script_State_RestoreWithoutSuspendedRunIsExact`, `Test_Script_State_RestoreSuspendedRunRecovers` in the `Test` step (label `script-conformance`)
- **Design:** Zenith2/Docs/Design/SCRIPT/HotReload.md → "Recovery invocations and exact restore"; Zenith2/Docs/Design/SCRIPT/HotReload.md → "Behaviour state in saves"; Zenith2/Docs/Design/SCRIPT/PublicAPI.md → "Zenith_ScriptStateSnapshot header"

### Z2-R-SCRIPT-169 Retiring a result table discards and counts

- **Statement:** `Zenith_ScriptWorld::DetachAsyncResults` hands the world's table to its caller, after which `Zenith_AsyncResultTable::Retire` discards and counts every completed unpublished record, and every later `Complete` on any thread releases its slot, discards and counts it without publishing; `GetInFlightCount`, `GetSourceInFlightCount` and `GetDiscardedCount` read atomically on any thread.
- **Covers:** Z2-I-SCRIPT-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_AsyncRetire_DiscardsAndCounts` (`ZenithScript_Tests`, Test step; also under `Sanitizers -Preset win-asan`)
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "Asynchronous result table"; Zenith2/Docs/Design/SCRIPT/PublicAPI.md → "Zenith_ScriptAsync header"; Zenith2/Docs/Design/SCRIPT/PublicAPI.md → "Zenith_ScriptWorld header"

### Z2-R-SCRIPT-170 Asynchronous state is listed and restored for saves

- **Statement:** `ListUnpublished` and `ListInFlight` return a world's completed unpublished results with payloads and its uncompleted requests in ascending completion ID, out of tick or on the phase-11 snapshot node; `RestoreUnpublished` and `RestoreInFlight` put them back under their own IDs before the first tick after a load and refuse an inconsistent entry as a value; `AllocateCompletionID(ASYNC_SOURCE_SAVE)` is admitted on the phase-11 node for an engine autosave; an `ASYNC_SOURCE_SAVE` result is released when its handler returns; a zero-length recorded payload is always accepted.
- **Covers:** Z2-I-SCRIPT-17, Z2-I-SCRIPT-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_AsyncSave_ListAndRestoreRoundTrip`, `Test_Script_AsyncSave_PhaseElevenAutosaveID` (`ZenithScript_Tests`, Test step)
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "Asynchronous result table"; Zenith2/Docs/Design/SCRIPT/PublicAPI.md → "Zenith_ScriptAsync header"

### Z2-R-SCRIPT-171 Publication resolvers rewrite drained live completions

- **Statement:** A source kind's resolver set with `SetPublicationResolver` before the first tick is called in live mode at tick-boundary step 1 once per drained completion of its kind, in completion-ID order, after sorting and before the publication observer, and what it leaves is what is logged, published and handed to the handler; it is never called under a publication source.
- **Covers:** Z2-I-SCRIPT-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_AsyncResolver_RewritesBeforeObserver` (`ZenithScript_Tests`, Test step)
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "Asynchronous result table"; Zenith2/Docs/Design/SCRIPT/PublicAPI.md → "Zenith_ScriptAsync header"

### Z2-R-SCRIPT-172 Replay adopts the recorded next completion ID

- **Statement:** Under a publication source the table first adopts, at each tick boundary, the next completion ID the tick's `REPLAY_RECORD_TICK` record carries (`GetRecordedNextCompletionID`), halts the world on a live counter above it, publishes a recorded entry with no live slot to its handler without retaining it, and returns `ASYNC_TABLE_ERROR_UNKNOWN_COMPLETION` for a live completion of an already published recorded ID, releasing nothing twice.
- **Covers:** Z2-I-SCRIPT-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_AsyncReplay_AdoptsRecordedNextID`, `Test_Script_AsyncReplay_LateLocalCompletionRefused` (`ZenithScript_Tests`, Test step)
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "Asynchronous result table"; Zenith2/Docs/Design/SCRIPT/PublicAPI.md → "Zenith_ScriptHost header"

### Z2-R-SCRIPT-173 Module sources enter the table through the runtime's sink

- **Statement:** A C++ source declared after the world's creation and before its first tick reserves its slots at its `DeclareSource`; a merge-chain allocation of a module source is ordered with the commit allocations of the merge chain; a handler on a script source the module declared runs for every publication of the kind, releasing a ticketless publication's slot after it and keeping a script request's publication for its ticket; and the registry's kinds 12 to 14 are assigned to CINE, AI and PACK with the capacity sources of "Source kind values".
- **Covers:** Z2-I-SCRIPT-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-016
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_AsyncSources_DeclaredAfterCreation`, `Test_Script_AsyncSources_MixedKindHandledAndTicketed` (`ZenithScript_Tests`, Test step)
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "Asynchronous result table"; Zenith2/Docs/Design/SCRIPT/Runtime.md → "Source kind values"

### Z2-R-SCRIPT-174 Committed checkpoints feed the tick hash

- **Statement:** `Zenith_ScriptWorld::AppendCommittedStateHash` appends every behaviour instance's committed checkpoint, with its source ID, simulation ID and lifecycle state, in ascending (source ID, simulation ID), allocating nothing, so two runs with equal committed behaviour state hash equally and a differing checkpoint changes the hash.
- **Covers:** Z2-I-SCRIPT-17, Z2-I-SCRIPT-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-023
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_StateHash_CommittedCheckpointsOnly` (`ZenithScript_Tests`, Test step)
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "Committed checkpoints in the tick hash"; Zenith2/Docs/Design/SCRIPT/PublicAPI.md → "Zenith_ScriptWorld header"

### Z2-R-SCRIPT-175 Buffered bindings read a per-function user datum

- **Statement:** A function registered with a user datum (`Zenith_ScriptFunctionDesc::m_pUserData`) reads it through `Zenith_ScriptInvocationContext::GetFunctionUserData` for the duration of its thunk call, null for a function registered without one, and the datum is not part of the binding-set hash.
- **Covers:** Z2-I-SCRIPT-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-016
- **Budgets:** none
- **Acceptance evidence:** `Test_ScriptBindings_FunctionUserDataReachesThunk` (`ZenithScript_Tests`, Test step)
- **Design:** Zenith2/Docs/Design/SCRIPT/Bindings.md → "Dispatcher implementation"; Zenith2/Docs/Design/SCRIPT/PublicAPI.md → "Zenith_ScriptBinding header"

### Z2-R-SCRIPT-176 Fixed-array fields of returned structs marshal as arrays

- **Statement:** A fixed-array field `T[N]` of a reflected value struct returned by value reads as a frozen Luau array of its N elements with null handles left `nil`, and a struct argument's array field accepts at most N elements, raising ZS1509 above N.
- **Covers:** Z2-I-SCRIPT-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-016
- **Budgets:** none
- **Acceptance evidence:** `Test_ScriptMarshal_StructFixedArrayField` (`ZenithScript_Tests`, Test step)
- **Design:** Zenith2/Docs/Design/SCRIPT/Bindings.md → "Marshalling implementation"

### Z2-R-SCRIPT-177 Bindings carry the Luau API classification

- **Statement:** Every binding function carries its since stage, stability, experimental-until stage, deprecation and registrant library, given through `AddFunction`'s `Zenith_ScriptFunctionVersion` and `SetRegistrantLibrary`; `Seal` refuses an inconsistent combination with `SCRIPT_BINDING_ERROR_INVALID_DESCRIPTOR` by rules B1 to B4, including a replacement that names no sealed function; and every member except the library is an input of the binding-set hash.
- **Covers:** Z2-I-SCRIPT-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-016
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Bindings_VersionValidatedAtSeal` (`ZenithReflect_Tests`, Test step), `Test_Script_Bindings_VersionInBindingSetHash` (`ZenithScript_Tests`, Test step)
- **Design:** Zenith2/Docs/Design/SCRIPT/Bindings.md → "Binding stability and deprecation"; Zenith2/Docs/Design/SCRIPT/PublicAPI.md → "Zenith_ScriptBinding header"

### Z2-R-SCRIPT-178 A call to a deprecated binding is diagnosed at load

- **Statement:** The cook records the first call to a deprecated binding on each line of a module in the manifest's deprecated-call block, and a world load records one `SCRIPT_BINDING_DEPRECATED_CALL` warning (ZS1808) per (module, line) with the exact catalogue text, logged in `script.bindings`, while the module loads and runs and no Luau error is raised.
- **Covers:** Z2-I-SCRIPT-04, Z2-I-SCRIPT-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-016, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Binding_DeprecatedCallDiagnosed` (`ZenithScript_Tests`, Test step), `Test_Script_Cook_DeprecatedCallsExtracted` (`ZenithScriptCompiler_Tests`, Test step)
- **Design:** Zenith2/Docs/Design/SCRIPT/Bindings.md → "Deprecated call sites"; Zenith2/Docs/Design/SCRIPT/Compiler.md → "Module manifest format"

### Z2-R-SCRIPT-179 The definitions record stability, the engine version and a binding listing

- **Statement:** The definition emitter writes the deprecated and experimental comment lines above those signatures in `Zenith.luau`, the engine version and binding-set hash in the `zenith.d.luau` header, and `bindings.json` (`zenith2.script-binding-listing` 1, every sealed function in sealed order); and the cook's and the runtime's engine version hash both come from `Zenith_ScriptEngineVersionHash`, so a changelog entry re-cooks bytecode and no committed file holds the value.
- **Covers:** Z2-I-SCRIPT-05, Z2-I-SCRIPT-23
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Bindings_ListingWritten`, `Test_Script_Bindings_StabilityCommentsEmitted` (`ZenithScript_Tests`, Test step), `Test_Script_Cook_EngineVersionHashProduced` (`ZenithScriptCompiler_Tests`, Test step)
- **Design:** Zenith2/Docs/Design/SCRIPT/Bindings.md → "Binding listing"; Zenith2/Docs/Design/SCRIPT/Bindings.md → "Definition emitter procedure"; Zenith2/Docs/Design/SCRIPT/PublicAPI.md → "Zenith_ScriptCompileOptions header"

### Z2-R-SCRIPT-180 Thirty-two asynchronous source kinds

- **Statement:** The binding table and every world's result table hold `uSCRIPT_ASYNC_SOURCE_KIND_CAPACITY` (32) source kinds, equal to ZenithSave's `uASYNC_SOURCE_KIND_CAPACITY`; declaring or allocating a kind at or above the capacity is engine misuse caught by `ZENITH_ASSERT`; and the registry assigns 15 to `ASYNC_SOURCE_NETWORK` and 16 to `ASYNC_SOURCE_XR` with the capacity sources of "Source kind values".
- **Covers:** Z2-I-SCRIPT-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Async_SourceKindCapacityMatchesRegistry`, `Test_Script_Async_KindAtCapacityAsserts` (`ZenithScript_Tests`, Test step; the second through the `z2-death-check` suite)
- **Design:** Zenith2/Docs/Design/SCRIPT/Runtime.md → "Asynchronous result table"; Zenith2/Docs/Design/SCRIPT/Runtime.md → "Source kind values"

### Z2-R-SCRIPT-181 The XR recentre side effect

- **Statement:** `SIDE_EFFECT_KIND_XR_RECENTRE` is appended to `Zenith_SideEffectKind` after `SIDE_EFFECT_KIND_CONTRIBUTION`; a buffered binding whose mask names it appends a zero-byte record that reaches its registered consumer at commit, a rolled-back invocation delivers nothing, and `Seal` refuses the mask without a consumer.
- **Covers:** Z2-I-SCRIPT-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-016
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Bindings_XrRecentreConsumed` (`ZenithScript_Tests`, Test step)
- **Design:** Zenith2/Docs/Design/SCRIPT/Bindings.md → "Side-effect consumer registration"; Zenith2/Docs/Design/SCRIPT/Transactions.md → "Side-effect routing"

## Coverage of inventory items

| Inventory item | Item | Requirements |
|---|---|---|
| Z2-I-SCRIPT-01 | Luau compilation | Z2-R-SCRIPT-001, Z2-R-SCRIPT-002, Z2-R-SCRIPT-012, Z2-R-SCRIPT-033, Z2-R-SCRIPT-035, Z2-R-SCRIPT-041, Z2-R-SCRIPT-042, Z2-R-SCRIPT-064 |
| Z2-I-SCRIPT-02 | Luau bytecode | Z2-R-SCRIPT-042, Z2-R-SCRIPT-043, Z2-R-SCRIPT-044, Z2-R-SCRIPT-045, Z2-R-SCRIPT-046 |
| Z2-I-SCRIPT-03 | module loading | Z2-R-SCRIPT-029, Z2-R-SCRIPT-030, Z2-R-SCRIPT-031, Z2-R-SCRIPT-032, Z2-R-SCRIPT-158, Z2-R-SCRIPT-178 |
| Z2-I-SCRIPT-04 | generated bindings | Z2-R-SCRIPT-003, Z2-R-SCRIPT-013, Z2-R-SCRIPT-044, Z2-R-SCRIPT-051, Z2-R-SCRIPT-052, Z2-R-SCRIPT-053, Z2-R-SCRIPT-054, Z2-R-SCRIPT-055, Z2-R-SCRIPT-056, Z2-R-SCRIPT-057, Z2-R-SCRIPT-058, Z2-R-SCRIPT-059, Z2-R-SCRIPT-060, Z2-R-SCRIPT-063, Z2-R-SCRIPT-149, Z2-R-SCRIPT-175, Z2-R-SCRIPT-176, Z2-R-SCRIPT-177, Z2-R-SCRIPT-178 |
| Z2-I-SCRIPT-05 | generated `.d.luau` | Z2-R-SCRIPT-044, Z2-R-SCRIPT-061, Z2-R-SCRIPT-062, Z2-R-SCRIPT-063, Z2-R-SCRIPT-064, Z2-R-SCRIPT-179 |
| Z2-I-SCRIPT-06 | the DAP debugger | Z2-R-SCRIPT-015, Z2-R-SCRIPT-139, Z2-R-SCRIPT-140, Z2-R-SCRIPT-141, Z2-R-SCRIPT-142, Z2-R-SCRIPT-143, Z2-R-SCRIPT-155, Z2-R-SCRIPT-157 |
| Z2-I-SCRIPT-07 | sandboxing | Z2-R-SCRIPT-034, Z2-R-SCRIPT-035, Z2-R-SCRIPT-036, Z2-R-SCRIPT-037, Z2-R-SCRIPT-038, Z2-R-SCRIPT-039, Z2-R-SCRIPT-040, Z2-R-SCRIPT-041, Z2-R-SCRIPT-137, Z2-R-SCRIPT-147, Z2-R-SCRIPT-148, Z2-R-SCRIPT-154, Z2-R-SCRIPT-160 |
| Z2-I-SCRIPT-08 | per-invocation transactions | Z2-R-SCRIPT-027, Z2-R-SCRIPT-057, Z2-R-SCRIPT-060, Z2-R-SCRIPT-066, Z2-R-SCRIPT-067, Z2-R-SCRIPT-068, Z2-R-SCRIPT-069, Z2-R-SCRIPT-071, Z2-R-SCRIPT-072, Z2-R-SCRIPT-074, Z2-R-SCRIPT-075, Z2-R-SCRIPT-076, Z2-R-SCRIPT-077, Z2-R-SCRIPT-078, Z2-R-SCRIPT-079, Z2-R-SCRIPT-080, Z2-R-SCRIPT-081, Z2-R-SCRIPT-082, Z2-R-SCRIPT-083, Z2-R-SCRIPT-084, Z2-R-SCRIPT-085, Z2-R-SCRIPT-086, Z2-R-SCRIPT-087, Z2-R-SCRIPT-088, Z2-R-SCRIPT-089, Z2-R-SCRIPT-090, Z2-R-SCRIPT-091, Z2-R-SCRIPT-092, Z2-R-SCRIPT-100, Z2-R-SCRIPT-147, Z2-R-SCRIPT-151, Z2-R-SCRIPT-152, Z2-R-SCRIPT-161, Z2-R-SCRIPT-162, Z2-R-SCRIPT-165, Z2-R-SCRIPT-181 |
| Z2-I-SCRIPT-09 | the stateless-system enforcement (D-LUAU-STATELESS) | Z2-R-SCRIPT-125, Z2-R-SCRIPT-132, Z2-R-SCRIPT-133, Z2-R-SCRIPT-134, Z2-R-SCRIPT-135, Z2-R-SCRIPT-136, Z2-R-SCRIPT-137, Z2-R-SCRIPT-138, Z2-R-SCRIPT-151 |
| Z2-I-SCRIPT-10 | the explicit behaviour-state model | Z2-R-SCRIPT-066, Z2-R-SCRIPT-070, Z2-R-SCRIPT-073, Z2-R-SCRIPT-089, Z2-R-SCRIPT-110, Z2-R-SCRIPT-111, Z2-R-SCRIPT-112, Z2-R-SCRIPT-113, Z2-R-SCRIPT-114, Z2-R-SCRIPT-115, Z2-R-SCRIPT-116, Z2-R-SCRIPT-117, Z2-R-SCRIPT-118, Z2-R-SCRIPT-120, Z2-R-SCRIPT-121, Z2-R-SCRIPT-136, Z2-R-SCRIPT-170, Z2-R-SCRIPT-174 |
| Z2-I-SCRIPT-11 | `OnRecover` | Z2-R-SCRIPT-119, Z2-R-SCRIPT-167, Z2-R-SCRIPT-168 |
| Z2-I-SCRIPT-12 | quarantine | Z2-R-SCRIPT-072, Z2-R-SCRIPT-090, Z2-R-SCRIPT-122, Z2-R-SCRIPT-123 |
| Z2-I-SCRIPT-13 | last-good restore | Z2-R-SCRIPT-117, Z2-R-SCRIPT-118, Z2-R-SCRIPT-123, Z2-R-SCRIPT-124 |
| Z2-I-SCRIPT-14 | hot reload | Z2-R-SCRIPT-046, Z2-R-SCRIPT-125, Z2-R-SCRIPT-126, Z2-R-SCRIPT-127, Z2-R-SCRIPT-128, Z2-R-SCRIPT-131, Z2-R-SCRIPT-163, Z2-R-SCRIPT-164 |
| Z2-I-SCRIPT-15 | migration | Z2-R-SCRIPT-126, Z2-R-SCRIPT-129, Z2-R-SCRIPT-130, Z2-R-SCRIPT-163 |
| Z2-I-SCRIPT-16 | per-worker VMs | Z2-R-SCRIPT-017, Z2-R-SCRIPT-018, Z2-R-SCRIPT-019, Z2-R-SCRIPT-020, Z2-R-SCRIPT-026, Z2-R-SCRIPT-028, Z2-R-SCRIPT-128, Z2-R-SCRIPT-156, Z2-R-SCRIPT-161, Z2-R-SCRIPT-164 |
| Z2-I-SCRIPT-17 | deterministic ordering rules | Z2-R-SCRIPT-025, Z2-R-SCRIPT-038, Z2-R-SCRIPT-065, Z2-R-SCRIPT-068, Z2-R-SCRIPT-069, Z2-R-SCRIPT-070, Z2-R-SCRIPT-085, Z2-R-SCRIPT-086, Z2-R-SCRIPT-088, Z2-R-SCRIPT-093, Z2-R-SCRIPT-094, Z2-R-SCRIPT-095, Z2-R-SCRIPT-096, Z2-R-SCRIPT-097, Z2-R-SCRIPT-098, Z2-R-SCRIPT-099, Z2-R-SCRIPT-100, Z2-R-SCRIPT-107, Z2-R-SCRIPT-108, Z2-R-SCRIPT-109, Z2-R-SCRIPT-159, Z2-R-SCRIPT-166, Z2-R-SCRIPT-167, Z2-R-SCRIPT-168, Z2-R-SCRIPT-169, Z2-R-SCRIPT-170, Z2-R-SCRIPT-171, Z2-R-SCRIPT-172, Z2-R-SCRIPT-173, Z2-R-SCRIPT-174, Z2-R-SCRIPT-180 |
| Z2-I-SCRIPT-18 | script GC budgets | Z2-R-SCRIPT-024, Z2-R-SCRIPT-025, Z2-R-SCRIPT-153 |
| Z2-I-SCRIPT-19 | script CPU budgets | Z2-R-SCRIPT-011, Z2-R-SCRIPT-037, Z2-R-SCRIPT-075, Z2-R-SCRIPT-101, Z2-R-SCRIPT-102, Z2-R-SCRIPT-103, Z2-R-SCRIPT-104, Z2-R-SCRIPT-105, Z2-R-SCRIPT-106, Z2-R-SCRIPT-150, Z2-R-SCRIPT-153, Z2-R-SCRIPT-156 |
| Z2-I-SCRIPT-20 | script memory budgets | Z2-R-SCRIPT-021, Z2-R-SCRIPT-022, Z2-R-SCRIPT-023, Z2-R-SCRIPT-076, Z2-R-SCRIPT-077, Z2-R-SCRIPT-084, Z2-R-SCRIPT-106, Z2-R-SCRIPT-116, Z2-R-SCRIPT-150, Z2-R-SCRIPT-153, Z2-R-SCRIPT-162 |
| Z2-I-SCRIPT-21 | script profiling | Z2-R-SCRIPT-106, Z2-R-SCRIPT-144, Z2-R-SCRIPT-145, Z2-R-SCRIPT-146 |
| Z2-I-SCRIPT-22 | the exception firewall | Z2-R-SCRIPT-001, Z2-R-SCRIPT-002, Z2-R-SCRIPT-003, Z2-R-SCRIPT-004, Z2-R-SCRIPT-005, Z2-R-SCRIPT-006, Z2-R-SCRIPT-007, Z2-R-SCRIPT-008, Z2-R-SCRIPT-009, Z2-R-SCRIPT-010, Z2-R-SCRIPT-011, Z2-R-SCRIPT-012, Z2-R-SCRIPT-013, Z2-R-SCRIPT-014, Z2-R-SCRIPT-015, Z2-R-SCRIPT-016, Z2-R-SCRIPT-048 |
| Z2-I-SCRIPT-23 | shipping bytecode version compatibility | Z2-R-SCRIPT-015, Z2-R-SCRIPT-045, Z2-R-SCRIPT-047, Z2-R-SCRIPT-048, Z2-R-SCRIPT-049, Z2-R-SCRIPT-050, Z2-R-SCRIPT-179 |

## Excluded and deferred

| Item | Classification | Rationale | Decision |
|---|---|---|---|
| third-party scripts and modding | excluded | no runtime path loads code from outside the package, patch and development loose mounts; the security boundary treats scripts as game content (Z2-R-SCRIPT-154) | Z2-D-075 |
| quest, dialogue, inventory and interaction systems | excluded | no engine library implements them: they are Luau gameplay modules of OpenWorldSlice, owned by GAME (Z2-R-SCRIPT-152) | Z2-D-077 |
| a node-graph scripting runtime or a second scripting language | excluded | Luau is the one gameplay-scripting language | Z2-D-016 |
| serialized execution positions (suspended coroutines in saves or checkpoints) | excluded | recovery restarts through `OnRecover` from declared state | Z2-D-023 |
| runtime translation of older bytecode | excluded | bytecode re-cooks on every engine or Luau pin change | Z2-D-048 |

No SCRIPT item is deferred beyond its inventory stage: every required item lands in Stage 3 (with Stage 1 decisions and harnesses) and shipping bytecode compatibility in Stage 6.
