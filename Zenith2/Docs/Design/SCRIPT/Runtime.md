# SCRIPT design — runtime

Companion to `Zenith2/Docs/Design/SCRIPT.md`: the internal structure of `Zenith_ScriptRuntime` and `Zenith_ScriptWorld`, the VM pool, the protected call, the allocator and interrupt hooks, the watchdog, GC scheduling, the script nodes and their placement in the frame graph, the behaviour instance directory, the tick-boundary handler, the asynchronous result table and random draws. It elaborates `Zenith2/Docs/Scripting.md` → "Luau integration and the exception firewall", "VM model and threading", "Behaviours and chunk systems", "Budgets safepoints and the watchdog" and "Merge-time capacity and asynchronous results", and restates none of their bullets. Every file named here is under `Zenith2/Engine/ZenithScript/` unless the path says otherwise.

## Scope of this file

This file covers everything that runs on a VM or schedules work onto one. The transaction machinery (arenas, snapshots, side effects, merge, commit) is Design/SCRIPT/Transactions.md; bindings, views, environments and the module loader are Design/SCRIPT/Bindings.md; state, recovery and hot reload are Design/SCRIPT/HotReload.md; the DAP server is Design/SCRIPT/Debugger.md.

## Script runtime internals

`Zenith_ScriptRuntime` (one per process, `MAIN_THREAD`) holds:

| Member | Type | Contents |
|---|---|---|
| `m_xDesc` | `Zenith_ScriptRuntimeDesc` | the creation descriptor (bindings, manifest, watchdog, types, recovery settings, native policy, debugger) |
| `m_axWorlds` | `Zenith_Array<Zenith_Owned<Zenith_ScriptWorld>>` | every world in creation order; bounded by the watchdog's world capacity (`Zenith_ScriptWatchdogDesc::m_uMaxWorlds`, § "Watchdog internals") |
| `m_xReloadQueue` | `Zenith_Array<Zenith_ScriptPendingReload>` | queued hot reloads (Design/SCRIPT/HotReload.md → "Hot-reload staging area"); one entry per (world, module), a second request for the same pair replaces the first |
| `m_xDiagnostics` | `Zenith_ScriptDiagnosticStore` | the runtime's own ring of 64 (reload failures that name no world) |
| `m_pxDebugger` | `Zenith_ScriptDap*` (owning, through `Zenith_Owned`) | the DAP service, created only when `!Zenith_IsShippingBuild()` and `m_xDebugger.m_bEnabled` |
| `m_bDebuggerAvailable` | `bool` | true when the DAP service exists and its listener was created; read by `IsDebuggerAvailable()` |
| `m_xReentrancyGuard` | `Zenith_ReentrancyGuard` | PublicTypes.md's |

`Create` steps (each fallible step names its fault point; a failure unwinds every earlier step in reverse, [Z2-N-CONTRACT-225]):

1. Validate the descriptor: non-null bindings (sealed), manifest, watchdog, types (frozen), heap; recovery values in range; otherwise `SCRIPT_ERROR_INVALID_DESC`.
2. Reserve `m_axWorlds` for 16 worlds (`uFAULT_POINT_SCRIPT_RUNTIME_WORLD_TABLE`), `m_xReloadQueue` for 16 entries (`…_RELOAD_QUEUE`) and the diagnostic ring (`…_DIAGNOSTICS`); both arrays grow on demand from the general heap, on the main thread, outside any tick.
3. Only inside `if constexpr (!Zenith_IsShippingBuild())` and only when `m_xDebugger.m_bEnabled`: `Zenith_ScriptCreateDap(xDesc.m_xDebugger, *this)` (`…_DEBUGGER`). When the service or its listener cannot be created, the outcome depends on `m_xDebugger.m_bRequired`:
   - `m_bRequired` false (the Player with `--script-dap-port`, the editor): `Create` records the diagnostic `SCRIPT_DEBUGGER_UNAVAILABLE` (the port and PLAT's listener error) in the runtime's diagnostic store, logs it at `LOG_LEVEL_WARNING` in category `script.dap`, leaves `m_bDebuggerAvailable` false and continues; the runtime is created without a debugger and no error is returned.
   - `m_bRequired` true (the DAP tests' harness and nothing else): `Create` unwinds steps 1–2 and returns `SCRIPT_ERROR_DEBUGGER_UNAVAILABLE`.

   Fault-sweep expectation for `uFAULT_POINT_SCRIPT_RUNTIME_DEBUGGER` (`Test_Script_World_CreateFaultSweep`, `Test_Script_Runtime_DebuggerOptional`): with `m_bRequired` false the runtime is created, no listener exists, `IsDebuggerAvailable()` is false and the test heap reports no leak; with `m_bRequired` true `Create` returns `SCRIPT_ERROR_DEBUGGER_UNAVAILABLE` and the test heap reports no leak.

`CreateWorld(xDesc)` calls `Zenith_ScriptWorld::Create`; when the runtime has a debugger it then calls `m_pxDebugger->AttachWorld(*pxWorld)`, which allocates the world's stop block of 1 + `m_uFrameThreadCount` mailbox slots outside any lock and links it under the DAP mutex (Design/SCRIPT/Debugger.md → "DAP service ownership and lifetime"; fault point `uFAULT_POINT_SCRIPT_RUNTIME_DAP_SLOTS`, a failure destroys the new world and returns `SCRIPT_ERROR_OUT_OF_MEMORY`); it appends the result and returns the pointer. `DestroyWorld` drains nothing itself (the caller has drained the tick, [Z2-N-SCRIPT-088]), destroys the world in place — every VM of it first — then, with a debugger, calls `DetachWorld`, and compacts `m_axWorlds` preserving order. `FindWorld` is a linear search by tag (at most a handful of worlds).

## Script world internals

`Zenith_ScriptWorld` holds, beyond LibraryLayout.md's public members:

| Member | Type | Contents | Thread |
|---|---|---|---|
| `m_xMainVM` | `Zenith_Owned<Zenith_ScriptVM>` | the world main VM, VM index 0 | main |
| `m_axWorkerVMs` | `Zenith_FixedArray<Zenith_Owned<Zenith_ScriptVM>, uSCRIPT_MAX_WORKER_VM_SLOTS>` with `m_uSlotCount = m_uFrameThreadCount` live entries | one slot per frame thread, indexed by `Zenith_JobContext::GetWorkerIndex()`; slot s holds VM index s + 1 (cross-area note J7) | owning worker; main thread at quiescence |
| `m_axModules` | `Zenith_Array<Zenith_ScriptModuleState>` | per manifest module: kind, current generation, staged generation and its state (`STAGED`, `COMMITTED`), validated declaration, `m_bDisabled` (SCRIPT's own disable, read at entry by every node and merge body of the module), `m_bDisableAtNextTickBoundary`, first failure, recovery count, last failure tick | the main thread (load, prologue), the module's merge nodes (the failure flag and first failure) and the world's tick-boundary node (disable, go-live, recovery); read by the module's nodes; ordered as below |
| `m_axDeclarations` | `Zenith_Array<Zenith_ScriptDeclaration>` | validated declarations of the current generation (Transactions.md and Bindings.md read them); indexed by `Zenith_ScriptSystemRef::m_uDeclarationIndex` | IMMUTABLE between tick boundaries |
| `m_axSystems` | `Zenith_Array<Zenith_ScriptSystemRecord>` | one per registered script system: registration index (the system order), declaration index, query slot, reduction slots, node kind, the retention claim table | main (registration at step 17 and the prologue's schedule commit, Design/SCRIPT/HotReload.md → "Schedule commit in the prologue"); the tick-boundary node swaps in a committed generation's records; the claim table as Transactions.md → "Transaction arena partition" states |
| `m_xInstances` | `Zenith_ScriptInstanceDirectory` | behaviour instances (§ "Behaviour instance directory") | the main thread (behaviour nodes, the prologue, out of tick), the module's merge node (active checkpoint slot index, dirty flag, consumed-result marks) and the world's tick-boundary node (go-live and last-good restore states); ordered as below |
| `m_xArenas` | `Zenith_ScriptTxnArenaSet` | the F + 1 scratch arenas and the per-slot retention sets (Transactions.md → "Transaction arena partition") | as that section states |
| `m_xAsync` | `Zenith_AsyncResultTable` | § "Asynchronous result table" | as its contract states |
| `m_xCheckpoints` | `Zenith_ScriptCheckpointPools` | HotReload.md → "Checkpoint slot pools"; one pool per behaviour module generation | the pools: the main thread alone (reservation in the behaviour node's directory refresh, the prologue's staging and rollback, `CommitPendingReloads`' drain of `m_axRetired`); slot contents: the checkpoint serializer on the main thread, read by the tick-boundary node's last-good restore and by SAVE's phase-11 node; the instance's slot pair index: installed by the tick-boundary node at go-live; ordered as below |
| `m_axRetired` | `Zenith_Array<Zenith_ScriptRetiredBlock>` | C++ blocks a go-live retires — each instance's old checkpoint-pool element, a superseded chunk-system bytecode buffer, a replaced breakable-line array (Debugger.md → "Breakpoint storage and fan-out") — as `Zenith_ScriptRetiredBlock { uint8 m_uKind; uint32 m_uModule; uint32 m_uGeneration; uint32 m_uElement; void* m_pBlock; uint64 m_ulEpoch; }`; drained on the main thread by `CommitPendingReloads` step 1 and at world destruction; its capacity is reserved by the prologue's schedule commit before a generation is marked `COMMITTED`, so go-live appends without allocating | tick-boundary node (or the main thread's out-of-tick go-live) appends; main thread drains |
| `m_xDiagnostics` | `Zenith_ScriptDiagnosticStore` | Transactions.md → "Diagnostic store implementation" | any writer, main reader |
| `m_xCounters` | `Zenith_ScriptCounters` (per slot) | per-slot counters summed at the tick boundary into `m_xTickCounters` | owning slot |
| `m_xBreakpoints` | `Zenith_ScriptBreakpointSet` | development builds: the world's breakpoint set and generation (Debugger.md) | DAP thread writes under its lock; VMs read |
| `m_eState` | `Zenith_ScriptWorldState` + `std::atomic<bool> m_bHalted` | LibraryLayout.md's; the atomic mirrors it for `IsHalted` | main writes state; any thread sets the halt flag |
| `m_xTickAdapter` | `Zenith_ScriptTickBoundaryAdapter` | the `Zenith_TickBoundaryHandler` registered with the simulation | tick-boundary node |
| `m_aiMainVMReleases` | `Zenith_Array<int>` | registry references on the main VM released at a tick boundary (go-live, last-good restore), drained with `lua_unref` on the main thread by the world's next behaviour node (§ "Behaviour node body" step 1), by `Zenith_ScriptRuntime::CommitPendingReloads` in prologue step 6 of every frame (Design/SCRIPT/HotReload.md → "Schedule commit in the prologue" step 1) and before `lua_close` at world destruction; it holds at most the references released since the last drain — at most one entry per registry reference of the world's instance records and module generations, because a reference is appended once, when it is replaced — and its capacity is kept: the directory refresh and the prologue's schedule commit keep it, on the main thread, at least the number of references a tick boundary can append (three per instance record of the world, plus the module-table and `Migrate` references of every current and committed generation), so an append at a tick boundary never allocates (a growth failure is the refresh's `RESERVATION` halt, or the reload's rollback) | tick-boundary node appends; main thread drains; ordered as below |
| `m_xDebugGate` | `Zenith_ScriptDebugGate` | development builds: the world's stop count and parking gate (Debugger.md → "Stop protocol") | any frame thread, the DAP thread, under the DAP mutex |
| `m_pxStopBlock` | `Zenith_ScriptStopBlock*` (owned by the DAP service) | development builds with a debugger: the world's mailbox slots, one per VM index, set by `AttachWorld` before any VM of the world runs (Debugger.md → "DAP service ownership and lifetime") | written once on the main thread; each VM uses its own slot, the DAP thread reads the block under the DAP mutex |

Ordering of the tables written by more than one node: every writer runs at a point the frame structure orders against every other access of the same field. The prologue steps run with no frame-graph job ([Z2-N-ARCH-437]) and before the frame's first tick boundary ([Z2-N-ARCH-438]); a world's tick-boundary node is the first node of its tick and runs after every node of the previous tick ([Z2-N-ARCH-383]); a module's merge node runs after every sub-job of its system node ([Z2-N-ARCH-395]), the merges of a phase are serialised by the merge chain, and the phase's sync point follows them. So a merge's writes (a module's failure flag, its instances' checkpoint slot index, dirty flag and consumed marks) happen before their next reader (the module's next node, or the tick boundary), the tick boundary's writes (disabled flags, generations, restored instance states, appended releases) happen before every node of its tick, and no two writers of one field run concurrently. None of these fields is a Luau object, so their writers need no main-thread affinity; only the Luau work (behaviour invocations, checkpoint serialization, `OnRecover`, `Migrate`, console evaluation, `lua_unref`, fresh `self` and proxies) is confined to the main thread.

`Create` steps (fault points of PublicAPI.md → "Zenith_ScriptWorld header"):

1. Validate the descriptor: every pointer non-null except the three optional host interfaces; `m_uFrameThreadCount` (the one frame-thread quantity of the desc; LibraryLayout.md's `m_uWorkerCount` is replaced by it, cross-doc row X121) in 1 … `uSCRIPT_MAX_WORKER_VM_SLOTS` and at most the watchdog's `m_uFrameThreadCount`; every budget value non-zero; `m_ulWorkerVMCapBytes × m_uFrameThreadCount ≤ m_ulWorkerVMTotalBytes`, else `SCRIPT_ERROR_INVALID_DESC` with a diagnostic naming Z2-B-SCRIPT-011 (the `BR-REFUSE` of that row, [Z2-N-BUDGET-298]).
2. Create the main VM (§ "VM creation steps", role MAIN, index 0) (`…_MAIN_VM`).
3. Size the slot table to `m_uFrameThreadCount` empty slots (`…_SLOT_TABLE`).
4. Construct and pre-reserve the F + 1 scratch arenas (`…_TXN_ARENAS`; Transactions.md → "Transaction arena partition" rule 1); the retention sets are sized by `RegisterScriptNodes`, when the systems are known.
5. Create the async table: one `DeclareSource` per source kind the binding table declared, with in-flight capacities from `m_xCapacities` (`…_ASYNC_TABLE`).
6. Create the diagnostic ring (`…_DIAGNOSTICS`).
7. Register the tick-boundary adapter with `m_pxSimulation->AddTickBoundaryHandler` (`…_HANDLER`).
8. Register the main VM with the watchdog, which assigns the world its watchdog index and its block of 1 + F table entries (`…_WATCHDOG_REGISTER`; `SCRIPT_ERROR_WATCHDOG_TABLE_FULL` when all `m_uMaxWorlds` world indices are in use, § "Watchdog internals").

`LoadModules` (world load, main thread, before `RegisterScriptNodes`) loads every behaviour and chunk-system module of the manifest into the main VM (Design/SCRIPT/Bindings.md → "Module loader implementation"): each module's declaration is validated and compared with the cooked declaration record of the manifest (HotReload.md → "State registration at startup"), its field tables and perfect-hash tables are built, its behaviour state type is resolved in the frozen registry, and its chunk-system query description is prepared. A load failure of one module marks that module `DISABLED_FOR_SESSION` for the world, records `SCRIPT_MODULE_DISABLED` with the error in the world's diagnostic store, calls the listener's `OnModuleDisabled`, and continues with the others. `LoadModules` returns success whenever the world is usable — every module loaded, or some failed and are disabled — so an error value always means the operation did not happen ([Z2-N-CONTRACT-464]); `GetDisabledModuleCount()` and `IsModuleDisabled` report the partial outcome, and the composition root logs each disabled module from its listener. `LoadModules` returns an error only when the manifest itself is unusable (a `Zenith_ScriptModuleManifest` field the build refuses: `SCRIPT_ERROR_BYTECODE_MISMATCH`) or when the manifest names at least one behaviour or chunk-system module and none loaded (`SCRIPT_ERROR_LOAD`, naming the first failure); then no system is registered and startup step 17 fails. The world has no module-level halt at load: a broken module costs itself only ([Z2-N-SCRIPT-481] trust boundary).

## VM pool per world

A world's VMs are the main VM and at most `m_uFrameThreadCount` worker VMs. The pool rules:

| Rule | Mechanism |
|---|---|
| a worker VM is created by its owning slot on first use, inside the sub-job | § "Chunk-system node body" step 3 |
| a slot is never read or written by another thread while a frame of the world runs | JOBS's affinity contract (`Zenith2/Docs/Design/JOBS.md` → "Per-worker Luau VM affinity contract", points 1–5) |
| the main thread destroys worker VMs only between iterations | § "VM teardown" of Scripting.md; `Zenith_ScriptWorld::~Zenith_ScriptWorld` runs after the tick is drained |
| slot 0 is the main thread helping with frame work; it is distinct from the main VM | cross-area note J7; the main VM runs behaviours, slot 0's VM runs chunk-system leaves the main thread picks up |
| `ParallelFor` is never called from inside a Luau invocation | the chunk-system node body calls it once, from its node body, before any invocation |
| a VM belongs to one world | the slot table is a world member; nothing else holds a VM pointer except the watchdog's registration table; the DAP service holds none — it reaches a VM only through the VM's mailbox slot, indexed by VM index in the world's stop block allocated at world creation (Debugger.md → "Stop protocol") |

Per-VM memory: the main VM against `m_ulMainVMCapBytes` (Z2-B-SCRIPT-009), each worker VM against `m_ulWorkerVMCapBytes` (Z2-B-SCRIPT-010). The memory governor reads `Zenith_ScriptRuntime`'s resident total (`MEMORY_SUBSYSTEM_SCRIPTS`: every VM's `m_ulBytesInUse`, every transaction arena's reserved bytes, every checkpoint pool block, the async result arenas), which the runtime reports through its `Zenith_MemoryConsumer` (`Zenith2/Docs/Architecture.md` → "Memory governor"); under critical pressure the governor's script response is Architecture's table row "Scripts" (GC at four times the step budget; a full collection of every VM at the next frame boundary), carried out by § "Garbage collection scheduling".

## VM creation steps

`Zenith_ScriptVM::Create(xDesc)` (Internal, `Internal/Zenith_ScriptVM.cpp`):

1. Allocate the `Zenith_ScriptVM` object through `Zenith_TryNew` (failure: `SCRIPT_ERROR_OUT_OF_MEMORY` for the main VM at world creation; for a worker VM the caller turns it into the fatal `SCRIPT_ERROR_KIND_VM_MEMORY` of [Z2-N-SCRIPT-077]).
2. `lua_newstate(Zenith_ScriptAlloc, pxVM)`; a null result is the same failure.
3. Install the callbacks: `lua_callbacks(L)->interrupt = Zenith_ScriptInterrupt`, `->userdata = pxVM`, and, when the runtime has a debugger (never in Shipping), `->debugbreak`, `->debugstep`, `->debuginterrupt` of Debugger.md → "Stop protocol".
4. Under one `SCRIPT_CALL_KIND_NATIVE_BODY` protected call with limit 0: `luaL_openlibs(L)`; build the role's environment (Design/SCRIPT/Bindings.md → "Environment construction"); register the per-VM thunk table (Design/SCRIPT/Bindings.md → "Per-VM thunk tables"); create the invocation thread of a worker VM (`lua_newthread`, `luaL_sandboxthread`, registry reference kept in `m_iInvocationThreadRef`); `luaL_sandbox(L)`.
5. Configure GC with the tuning of the descriptor: `lua_gc(L, LUA_GCSETGOAL, goal)`, `LUA_GCSETSTEPMUL`, `LUA_GCSETSTEPSIZE`.
6. Set `m_uVMIndex`, `m_eRole`, `m_ulNextInvocationCounter = 1`, and mark construction complete (`m_bConstructed = true`), after which the Debug allocation check of [Z2-N-SCRIPT-037] is armed.
7. Register with the watchdog (a worker VM registers itself on its thread; `Register` takes the watchdog lock, § "Watchdog internals").

`~Zenith_ScriptVM`: unregister from the watchdog (waits for an in-flight poll of this VM), release every registry reference the VM's tables hold (retained callbacks, cached module tables, the invocation thread), then `lua_close(L)`.

## Protected call implementation

`Internal/Zenith_ScriptProtectedCall.cpp` holds Scripting.md's algorithm ("Protected entry algorithm") and nothing else of note: it is the only translation unit containing `lua_pcall`, `lua_resume`, `luau_load` and a `catch` clause in ZenithScript ([Z2-N-SCRIPT-035], [Z2-N-SCRIPT-030]). Implementation details a worker needs:

| Detail | Decision |
|---|---|
| saved state for nesting | a `Zenith_ScriptCallFrameState { uint64 m_ulLimit, m_ulCount; Zenith_ScriptInvocationID m_xInvocation; uint64 m_ulWatchdogStart; uint64 m_ulWatchdogRebase; int m_iStackTop; }` on the C stack of the call, restored at step 7; in development builds `m_ulWatchdogRebase` records the VM's `m_ulWatchdogRebaseTicks` at entry, and step 7 restores the enclosing start as `m_ulWatchdogStart + (m_ulWatchdogRebaseTicks − m_ulWatchdogRebase)`, so a debugger stop inside a nested call is excluded from the enclosing invocation's time as well (Debugger.md → "Stop protocol" rule 5) |
| stepping exit | in development builds, the exit of the outermost protected call of a VM whose `m_xStep.m_bActive` is still set (a step armed and not consumed by a stop) runs the step completion of Debugger.md → "Stop protocol" rule 6 before returning: it decrements the world's stopped count only when the VM's `m_bCounted` is true, under `ZENITH_CHECK(m_uStoppedCount > 0)`; a step consumed by a stop (rule 1) leaves nothing for the exit to do |
| nested limit | `m_ulSafepointLimit = (enclosing limit == 0) ? xRequest.m_ulSafepointLimit : enclosing limit − enclosing count`; a nested request never raises the enclosing limit |
| error handler | `Zenith_ScriptErrorHandler` is a C function pushed once per VM into the registry at creation (`m_iErrorHandlerRef`) and fetched with `lua_getref` before the callee; it writes `debug.traceback` text into the VM's `m_xErrorScratch` (one `Zenith_ScriptDiagnostic` per VM) |
| message conversion | the message is read with `lua_tolstring` inside the handler (the handler runs inside the protected region, so allocation is legal), truncated to 511 bytes at a code-point boundary |
| ZS parsing | after the position prefix `<chunk>:<line>: `, the text `ZS` followed by four digits and `: ` sets `m_uCode`; the line number sets `m_uLine` |
| abort value | an abort raised as the Zenith abort value (a light userdata with tag `uSCRIPT_ABORT_TAG`, pointer bits carrying the kind) is recognised by the handler; its message is built from the VM's `m_szPendingAbortMessage` written by the raiser |
| diagnostic record | on any error, the scratch is copied into the world's diagnostic store (Transactions.md → "Diagnostic store implementation") and the returned `Zenith_ScriptError::m_uDiagnosticSequence` names it; the message is also logged at `LOG_LEVEL_ERROR` in category `script.runtime` at this point ([Z2-N-CONTRACT-464]) unless the error is an expected merge-time or test diagnostic |
| exceptions | `catch (const std::exception&)` records `what()` into the scratch; `catch (...)` records `non-standard exception`; both classify `SCRIPT_ERROR_KIND_INTERNAL`, which the caller turns into the world halt of § "World halt procedure" |
| depth limit | `m_uProtectedDepth >= uSCRIPT_MAX_PROTECTED_DEPTH` before step 2 returns ZS1701 (`SCRIPT_ERROR_KIND_MISUSE`) without touching the Luau stack |
| `LUA_MULTRET` | refused by assertion on `m_uMaxResults` (engine programmer error) |

Callers of the protected call and the request each builds:

| Caller | Kind | Limit | Thread |
|---|---|---|---|
| chunk-system invocation (`Zenith_ScriptChunkRunner::RunChunk`) | `NATIVE_BODY` (the row trampoline) | per entity, reset by the trampoline (Transactions.md → "Invocation lifecycle") | owning worker |
| behaviour callback | `FUNCTION` on the main thread | Z2-B-SCRIPT-001, optionally lowered | main |
| behaviour `Run` segment | `RESUME` of the instance coroutine | Z2-B-SCRIPT-001, optionally lowered | main |
| `OnRecover`, each `Migrate` step | `FUNCTION` | Z2-B-SCRIPT-018 | main |
| module load (module scope) | `NATIVE_BODY` (loader) | Z2-B-SCRIPT-019 | main, or the owning worker for a lazy worker load |
| checkpoint serialization and restore | `NATIVE_BODY` | 0 | main |
| GC step, invocation-thread reset, breakpoint application | `NATIVE_BODY` | 0 | the VM's thread |
| callback re-entry (`InvokeCallback`) | `FUNCTION` | the enclosing remainder | the VM's thread |
| console evaluation, schema script migration op | `FUNCTION` in the TOOLS environment | Z2-B-SCRIPT-018 | main |

## Allocator hook implementation

`Internal/Zenith_ScriptAllocator.cpp` implements Scripting.md → "Memory cap and allocator hook" over `Zenith_Heap` (the root heap passed as `m_pxHeap`): `Allocate(size, 16)`, `Reallocate`, `Free`. `m_ulBytesInUse` is a plain `uint64` (owner-thread writes); the governor reads the sum through `std::atomic_ref<const uint64>` relaxed loads at the prologue, which tolerates a stale value because accounting is diagnostic. Luau's `ulOldSize` is trusted as the block's size (Luau passes the size it requested), so no header is added. A cap refusal sets `m_eRaisedKind = SCRIPT_ERROR_KIND_VM_MEMORY` before returning null, so the protected call classifies `LUA_ERRMEM` correctly even when the refusal happens inside a nested library call. The Debug protected-depth check asserts `m_uProtectedDepth > 0 || !m_bConstructed || m_bClosing` (`lua_close` frees outside any protected region).

## Interrupt callback implementation

`Internal/Zenith_ScriptInterrupt.cpp` implements Scripting.md → "Safepoint counting" with these specifics:

1. The owning VM is `static_cast<Zenith_ScriptVM*>(lua_callbacks(L)->userdata)` — the same for a VM's coroutine and invocation threads, because callbacks are per global state.
2. After a budget abort was raised in the current entity transaction, `m_bAbortLatched` stays set until the trampoline or the runner opens the next transaction, so every further safepoint re-raises ([Z2-N-SCRIPT-307]).
3. Raising: `m_eRaisedKind` is set, the abort message is formatted into `m_szPendingAbortMessage` with the catalogue text (ZS1401 with count, limit, module, simulation ID), then `lua_pushlightuserdata(L, Zenith_ScriptAbortValue(kind))` and `lua_error(L)`; the interrupt runs inside a protected region, so the raise unwinds to the protected entry through exception-enabled frames only.
4. Sampling (development builds, when enabled): `if (++m_uSampleCountdown == uSCRIPT_SAMPLE_PERIOD)` records a stack sample (Debugger.md → "Sampling profiler implementation"); sampling reads the stack with `lua_getinfo` and never counts a safepoint.
5. Debugger: a pending pause (the `m_bPauseRequested` of the VM's mailbox slot, relaxed load) enters the stop loop (Debugger.md → "Stop loop") only while the VM is inside an invocation or an out-of-tick call of Debugger.md → "Stop protocol" rule 8; elsewhere (a worker VM's lazy module scope) the flag stays set until the next invocation's first safepoint.

## Watchdog internals

`Internal/Zenith_ScriptWatchdog.cpp`. The watchdog owns one `Zenith_Thread` named `Zenith.ScriptWatchdog`, a stop flag, and, under one `Zenith_Mutex` at level `uLOCK_LEVEL_SCRIPT_WATCHDOG` (700, short-hold; row added to `Zenith2/Docs/Contracts.md` → "Threading and concurrency contract" by the ticket that lands the watchdog, [Z2-N-CONTRACT-320]):

| Member | Contents |
|---|---|
| `m_axEntries` | `Zenith_Array<Zenith_ScriptWatchdogEntry>` of `m_uMaxWorlds × (1 + m_uFrameThreadCount)` entries allocated at `Create` (fault point `uFAULT_POINT_SCRIPT_WATCHDOG_TABLE`); `Zenith_ScriptWatchdogEntry { Zenith_ScriptVM* m_pxVM; uint16 m_uWorldTag; uint8 m_uWorldIndex; }` |
| `m_auWorldIndexOfTag` | `Zenith_FixedArray<uint8, uECS_MAX_WORLD_TAG + 1>` — the tag → world-index map (0xFF = no index), 4,096 bytes |
| `m_auTagOfWorldIndex` | `Zenith_FixedArray<uint16, uSCRIPT_WATCHDOG_MAX_WORLDS>` — the inverse, for reports |

Capacity: `m_uMaxWorlds` ≤ `uSCRIPT_WATCHDOG_MAX_WORLDS` (64) and `m_uFrameThreadCount` ≤ 63 (JOBS's `jobs.workerCount` limit of 62 plus the main thread, `Zenith2/Docs/Design/JOBS.md` → "Configuration keys"), so the table holds at most 64 × 64 = 4,096 entries of 16 bytes (64 KiB). The composition root passes `m_uMaxWorlds = 16` (the design default: the Player's world, the editor's edit and play worlds, and transition headroom) and `m_uFrameThreadCount = Zenith_JobSystem::GetFrameThreadCount()`: on the S24 with the default seven workers that is 16 × 9 = 144 entries; on a desktop with 23 workers, 16 × 25 = 400. The world with index w owns entries `w × (1 + F) … w × (1 + F) + F`; its VM with index v (0 the main VM, 1 + s the worker VM of slot s) uses entry `w × (1 + F) + v`.

Registration:

1. `Register(xVM)` of a main VM (index 0): take the lock; if the VM's tag already has an index, the call is a programmer error (`ZENITH_ASSERT`); otherwise take the lowest free world index (none free: `SCRIPT_ERROR_WATCHDOG_TABLE_FULL`, nothing changed), record it in both maps, and fill entry 0 of the block.
2. `Register(xVM)` of a worker VM (index 1 + s): take the lock; fill entry `1 + s` of the block its tag's index names. It cannot fail: the block was reserved with the main VM, and `Zenith_ScriptWorld::Create` refused a world whose frame-thread count exceeds the watchdog's.
3. `Unregister(xVM)` takes the lock and clears the entry; unregistering the main VM (destroyed last, after every worker VM) also frees the world index and clears both map entries. Because the poll holds the lock while reading a VM, unregistration never races with a poll of that VM (the `Zenith_ScriptWatchdog` contract of `Zenith2/Docs/Scripting/PublicTypes.md` → "ZenithScript public types": unregistration waits for an in-flight poll).

<!-- assumption: pending core-doc (X121) -->
PublicTypes.md declares `Create(uint32 uLimitMs, uint32 uPollMs)`, `Register` failing with `SCRIPT_ERROR_KIND_INTERNAL` on a full 256-entry table, and `SuspendWorld(uint32 uWorldIndex, bool)`; `Zenith2/Docs/Scripting.md` → "Wall-clock watchdog" step 4 reads "while any VM of a world is stopped in the debugger, the watchdog ignores that world". This design uses `Create(const Zenith_ScriptWatchdogDesc&)` (Design/SCRIPT/PublicAPI.md → "Zenith_ScriptTypes header"), `SCRIPT_ERROR_WATCHDOG_TABLE_FULL` and the capacity rule above, and has no `SuspendWorld`: a debugger stop exempts the stopped VM through its own flag and excludes the stopped time by rebasing (below), because a world-level exemption toggled by the first stop and the last resume both trips a resumed invocation for the whole stop and can be cleared while another VM of the world is stopped. The declarations and the step's wording change with cross-doc rows X121, X151 (4) and X158.
<!-- /assumption -->

Poll loop (every `m_uPollMs`, 10 ms):

1. Read `now` once; lock; for each occupied entry: skip it while the VM's `m_bDebuggerStopped` is set (acquire; development builds, always clear otherwise); read the VM's `m_ulWatchdogStartTicks` as `start` (acquire); if non-zero and `now − start > limit`, re-read `m_bDebuggerStopped` (acquire) and, only when it is still clear, re-read the start time as `start′` (acquire) and decide on `start′` alone: when `start′` is non-zero and `now − start′ > limit`, set `m_bWatchdogTripped` (release) and record the trip time; if the entry tripped earlier and `now − start′ > 2 × limit`, record a hang for that VM. The second read of the start is what makes the decision sound. A VM can stop, stay stopped, rebase its start to `start + D` and clear its flag between the first read and the flag re-read (the watchdog thread preempted there): it stored the rebased start (release) before it cleared the flag (release), so the acquire load that sees the flag clear also sees the rebased start, and `now − start′` excludes the whole stop. A VM still stopped at the re-read is skipped, and a stop that begins after the re-read began after `now`, so every elapsed time a trip or a hang report acts on is running time.
2. Unlock; for each hang recorded, report through the crash handler (D-SEL-CRASH, open, Stage 2): the report carries the world tag, the VM index, the module and entity of the running invocation (read from the VM's `m_xCurrentInvocationID` and its invocation record) and the traceback captured with `lua_debugtrace` on the watchdog thread (Luau's debug trace reads the stack without allocating), then `ZENITH_FATAL(FATAL_CODE_SCRIPT_HANG)` (a code the watchdog ticket adds to Contracts' fatal table in the same diff, [Z2-N-CONTRACT-478]). The report has two branches, both ending in that fatal. With `Zenith_ScriptWatchdogDesc::m_pxCrashHandler` null (sanitizer builds, tests, a disabled handler) the report is logged at `LOG_LEVEL_FATAL` in category `script.watchdog` alone. With it set, the watchdog first calls `m_pxCrashHandler->WriteHangReport(ulThreadID, strReason, strScriptStack)` (`Zenith2/Docs/Design/DEVT/Crash.md` → "Hang reports"), then logs the same text: `ulThreadID` is the VM's `std::atomic<uint64> m_ulWatchdogThreadID`, which the protected entry stores (`Zenith_GetCurrentThreadID()`, `Zenith2/Docs/Design/CORE/Threading.md`, relaxed) before it publishes `m_ulWatchdogStartTicks` with release, so the poll's acquire read of the start time sees the thread that set it, `strReason` is `script VM '<world tag>:<VM index>' exceeded 2x the <limit> ms limit (<elapsed> ms)` and `strScriptStack` is the `lua_debugtrace` text, all three composed in the watchdog's preallocated 16 KiB report buffer, so the branch allocates nothing. The handler is `ANY_THREAD` for `WriteHangReport` and returns after a failed write (it logs `crash: hang report not written: <error>`), so the fatal always follows. Only the first hang of a poll is reported, because the fatal ends the process.

Debugger stops (development builds; Debugger.md → "Stop protocol" rules 1 and 5): a VM sets its own `m_bDebuggerStopped` before it blocks, and the poll skips it; when it resumes it adds the stop's duration D to its published start time (when an invocation is running) and to `m_ulWatchdogRebaseTicks`, and only then clears the flag. So the time a VM spends stopped never counts, and after a stop of any length its invocation has exactly the elapsed running time it had before the stop; a nested protected call restores the enclosing start with the same rebase (§ "Protected call implementation"). Only the stopped VM is exempt: another VM of the same world still running an invocation (Debugger.md → "Stop protocol" rule 2) is watched as usual, and a parked thread has no invocation running (start 0). There is no world-level suspension state, so no resume of one VM can clear another VM's exemption, and the watchdog takes no lock the stop path holds (the flag is an atomic, and the stop path never takes 700).

## Garbage collection scheduling

Each VM keeps `m_uGCAllowanceKB`, reset to `m_uGCStepKBPerTick` (Z2-B-SCRIPT-007, ×4 while the governor reports `MEMORY_PRESSURE_CRITICAL`) at the world's tick boundary for the main VM and at the first sub-job of the tick for a worker VM (the slot compares its `m_ulAllowanceTick` with the node's tick index). A step:

1. `uSliceKB = max(1, m_uGCStepKBPerTick / 8)`; skip when `m_uGCAllowanceKB == 0`.
2. `StepGC(min(uSliceKB, m_uGCAllowanceKB))` as a `NATIVE_BODY` protected call running `lua_gc(L, LUA_GCSTEP, kb)`; subtract the slice.
3. Record the step's duration in the VM's GC zone and counters (`script.gc.kb`, `script.gc.ns`).

Where steps run: the main VM after the body of each behaviour node (§ "Behaviour node body" step 6); each worker VM at the end of each chunk-system sub-job it executed. A full collection requested by the governor (critical level) runs `lua_gc(L, LUA_GCCOLLECT, 0)` on every VM of every world in the next frame prologue, on the main thread, which is legal because no frame job runs there (worker VMs are quiescent). GC progress never changes a safepoint count ([Z2-N-SCRIPT-302]) and is never observable by a script ([Z2-N-SCRIPT-084]).

Budgets.md states Z2-B-SCRIPT-007 "per frame" while Scripting.md's descriptive name states "per VM per tick"; this design applies it per VM per tick (the specification of the mechanism) and records the unit difference as reconciliation item S9, pending cross-doc row X119 (the Stage 1 per-system budget ticket for SCRIPT settles the unit).

## Node registration and frame-graph placement

<!-- elaboration -->
A world created with `Zenith_ScriptWorldDesc::m_bRunSimulationScripts` false (an EDIT or PREVIEW world, A3 cross-area row ED4) runs no script in simulation. `LoadModules` loads the manifest as usual, so diagnostics and declarations exist and the main VM serves the console. `RegisterScriptNodes` sizes nothing and registers no system: no chunk system, no behaviour node and no handler node. The behaviour instance directory attaches no instance, so a `Zenith.Behaviour` component stays inert. The tick-boundary handler still runs its steps, so asynchronous results publish and disables apply. `Test_Script_World_NoSimulationScripts` asserts that such a world registers zero systems, attaches no instance across ten ticks of a scene holding two behaviour entities, and still reports both modules loaded.

`RegisterScriptNodes(xSystems)` (startup step 17 and every schedule rebuild) sizes the transaction retention sets (Transactions.md → "Transaction arena partition" rule 1) and registers, through `Zenith_SystemRegistry::AddSystem` (`Zenith2/Docs/Design/ECS/SystemAPI.md` → "Zenith_System header"), one system per chunk-system module and one or two per behaviour module, in the order that defines their system order (ECS's registration index is the merge-key system order):

1. Per phase in phase order (Early scripts = `PHASE_EARLY_SCRIPTS`, Late scripts = `PHASE_LATE_SCRIPTS`):
2. the phase's chunk systems in a deterministic topological order of their `Before`/`After` constraints, ties broken by ascending stable source ID (Scripting.md [Z2-N-SCRIPT-152]); a cycle among script systems is a load error of every module in it (ZS1806 naming the cycle), and those modules are disabled;
3. then the phase's behaviour nodes in behaviour-module order (ascending module ID, [Z2-N-SCRIPT-161]): an Early module's Early node in the Early phase; in the Late phase, every Late module's node and every Early module's handler node (present when the module subscribes to events or its state schema is non-empty, so a checkpoint-only transaction can run, [Z2-N-SCRIPT-158]), merged in module-ID order.

Each registration:

| Field | Chunk system | Behaviour node |
|---|---|---|
| `m_szName` | `Script.<Name>` (the declaration's `Name`) | `Script.Behaviour.<Name>.Early` or `.Late` |
| `m_ePhase` | the declared phase | the node's phase |
| `m_pfnRun`, `m_pfnMerge` | `Zenith_ScriptChunkNodeRun`, `Zenith_ScriptMergeNodeRun` | `Zenith_ScriptBehaviourNodeRun`, `Zenith_ScriptMergeNodeRun` |
| `m_pUserData` | the world's `Zenith_ScriptSystemRecord` | the same |
| `m_eAffinity` | `JOB_AFFINITY_ANY` | `JOB_AFFINITY_MAIN` |
| `m_eWriterSource` | `WRITER_SOURCE_SCRIPT` | `WRITER_SOURCE_SCRIPT` |
| `m_eSystemKind` | `SYSTEM_KIND_SCRIPT_CHUNK` | `SYSTEM_KIND_SCRIPT_BEHAVIOURS` |
| `m_uCostHint` | the measured mean node time of the previous 60 ticks in µs (initially 100) | the same |
| builder calls | `Query` over `Reads ∪ Writes ∪ With` minus `Without`; `Read`/`Write` per property of the expanded sets; `ReadAnyRow` for declared cross-entity reads; `ReadResource` per `Uses`; `EmitsCommands()`; `EmitsEvent` per event type the module's bindings may emit (from the declaration's `Emits`, or every event type when absent); `Reduction` per declared reduction; `Before`/`After` per declared name, as `Script.<Name>`; `ConsumesMergeCapacity()` | `Query` over `Zenith_BehaviourComponent` plus the declared components; `Read`/`Write` per expanded property; `ReadsEvent` per `Subscribes` (Late node); `ReadResource` per `Uses`; `EmitsCommands()`; `EmitsEvent` as for systems; `ConsumesMergeCapacity()` |

Change filters (cross-area note E18(b)): the declaration keys of Scripting.md → "Chunk system declaration" carry no change filter, so no script system's query has a `QUERY_TERM_CHANGED` term, and SCRIPT keeps no row-level last-seen version of its own (the instance directory compares structural epochs, § "Behaviour instance directory"); SCRIPT's merge stamps written columns through `CommitRowChanges` at `GetMergeVersion()` as ECS requires. Should the declaration gain a change filter, it is an ECS query term that inherits ECS's rule (`Zenith2/Docs/Design/ECS/Queries.md` → "Change-filtered queries"), and any SCRIPT bookkeeping of a last-seen version takes `Zenith_MergeContext::GetReadVersion()`.

Every merge node, of a chunk system or of a behaviour system, takes the `ANY` affinity ECS gives merge nodes (`Zenith2/Docs/Design/ECS/Scheduling.md` → "Node and resource naming"); SCRIPT relies on no other affinity, because its merge body touches no Luau object (Transactions.md → "Merge node body").

`Zenith_SystemContext::GetChunkOfEntityInScope(slot, entity, scope, uRowOut)`, ECS's scope-taking form of `GetChunkOfEntity` (`Zenith2/Docs/Design/ECS/SystemAPI.md`; cross-doc row X192 applied), gives a behaviour's own-row view its script invocation ID.

Placement in the frame (world w; `Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Frame structure", `Zenith2/Docs/Architecture/FramePhases.md` → "The twelve phases"):

| Where | SCRIPT work | Node | Access |
|---|---|---|---|
| frame prologue step 6 | `Zenith_ScriptRuntime::ApplyPendingReloads` inside the registry window ZenithRuntime opens, then `Zenith_ScriptRuntime::CommitPendingReloads` in every frame after the window closed (or in its place): release-list drains, rollback of an unpublished window, the `Zenith_SystemReloadScope` of each world with a changed declaration (Design/SCRIPT/HotReload.md → "Schedule commit in the prologue") | none (main thread) | the type registry window; staging objects; each world's `Zenith_SystemRegistry` |
| frame prologue step 8 | the schedule recompile triggered by that commit in step 6 of the same frame ([Z2-N-ARCH-408]) | none | — |
| tick boundary step 1 | asynchronous publication (§ "Asynchronous result table") | `ECS.TickBoundary@W<tag>` | the world's publication state |
| tick boundary step 3 | disables, go-live of committed reloads, automatic recoveries, per-tick resets (§ "Tick-boundary handler") | `ECS.TickBoundary@W<tag>` | SCRIPT world state (C++ only); the system-enable requests through `Zenith_TickBoundaryAccess::RequestSystemEnabled` |
| phase 2, Early scripts | chunk systems; Early behaviour nodes; their merge nodes on the merge chain | `Script.<Name>`, `Script.Behaviour.<Name>.Early`, `….Merge` | declared properties, resources, events |
| phase 10, Late scripts | Late behaviour nodes and handler nodes; Late chunk systems; merge nodes | as above | as above, plus event reads |
| phase 11 | the save-snapshot node of SAVE reads `GetStateSnapshot()` | SAVE's | read-only |
| end of every sub-job, after each behaviour node | GC steps | inside the owning node | the VM only |
| frame-loop step 3 | `PollDebugger()` (development builds) | none (main thread) | the DAP queues |

## Chunk-system node body

`Zenith_ScriptChunkNodeRun(Zenith_SystemContext& xContext)` (`Internal/Zenith_ScriptChunkRunner.cpp`):

1. Read the system record from `xContext`'s user data; if the world is halted or the module's `m_bDisabled` is set, return without running script. This check is the disable (§ "Tick-boundary handler"): the node stays in the schedule and completes without running script, and disabling never recompiles ([Z2-N-ARCH-410]); ECS also skips the body once the tick boundary's `RequestSystemEnabled(…, false)` applies, and the check stays as the disable SCRIPT itself reads.
2. `uChunks = xContext.GetChunkCount(slot)`; build the invocation list: for each chunk ordinal, ranges of at most `uSCRIPT_MAX_ROWS_PER_INVOCATION` rows; the list is stored in the node arena; `xContext.Jobs().ParallelFor(uInvocations, uGrain = 1, leaf)` where each leaf runs a contiguous range of invocations. The grain and partition are scheduling choices with no effect on results ([Z2-N-SCRIPT-076]).
3. Leaf body (on slot s = `xLeafJobs.GetWorkerIndex()`): `xLeaf = xContext.ForLeaf(xLeafJobs)`; acquire the slot's VM, creating it on first use (§ "VM creation steps", role WORKER, index s + 1; a failure is the fatal VM-memory halt); apply a stale breakpoint set (Debugger.md → "Breakpoint storage and fan-out"); ensure the module's current generation is loaded on this VM (Design/SCRIPT/Bindings.md → "Module loader implementation", lazy load; a load failure on a worker VM of a module the main VM accepted is an internal inconsistency and halts the world with `SCRIPT_ERROR_KIND_INTERNAL`).
4. For each invocation of the range: in development builds, pass the world's debugger gate first (Debugger.md → "Stop protocol"; a closed gate parks the leaf before the invocation starts); issue its invocation ID from the VM (`(uint64(vmIndex) << 48) | m_ulNextInvocationCounter++`; a counter reaching 2⁴⁸ halts the world, [Z2-N-SCRIPT-862]); get the chunk view with `xLeaf.GetChunkInScope(slot, ordinal, Zenith_InvocationScope{ id })`; open the transaction in the slot's scratch arena and the system's retention entry on this slot (Transactions.md → "Invocation lifecycle"); run `Zenith_ScriptChunkRunner::RunChunk` through `Zenith_ScriptProtectedCall` (`NATIVE_BODY`); on a fatal outcome run § "World halt procedure" and stop the leaf; append the transaction to the entry's list (single writer: this slot's thread; the merge orders the transactions afterwards).
5. At the end of the leaf: reset the pooled invocation thread (`lua_resetthread` inside a limit-0 protected call), run the GC step of § "Garbage collection scheduling", add the slot's counters.

## Behaviour node body

`Zenith_ScriptBehaviourNodeRun(Zenith_SystemContext& xContext)` (`Internal/Zenith_ScriptBehaviourRunner.cpp`), main thread:

1. If the world is halted or the module's `m_bDisabled` is set, return without running script (as § "Chunk-system node body" step 1). Otherwise drain the world's main-VM release list (`lua_unref` of each queued reference, `m_aiMainVMReleases`) and create the fresh `self` and state proxy of every instance of this module that a last-good restore at a tick boundary left without them (§ "Tick-boundary handler"); then refresh the instance directory for this module (§ "Behaviour instance directory").
2. For each active instance of the module in ascending stable simulation ID, build its invocation list for this node (ordinals continue from the instance's `m_uNextOrdinal` of this tick, [Z2-N-SCRIPT-274]; `OnRecover` takes the reserved `uSCRIPT_RECOVERY_ORDINAL` and does not advance `m_uNextOrdinal`, Design/SCRIPT/HotReload.md → "Recovery invocations and exact restore" step 1, [Z2-N-SCRIPT-928]):
   - Early node of an Early module: `OnRecover` when recovery is pending and the module's phase is Early; `OnCreate` when the instance is new; the `OnUpdate` call or the `Run` segment when its wait condition holds.
   - Late node of a Late module: `OnRecover`, `OnCreate` (as above); one handler invocation per delivered event of a subscribed type, in delivery order, for every delivered event — none is dropped ([Z2-N-SCRIPT-283]); then `OnUpdate` or the `Run` segment. The delivered events are one shared list per node, not per instance: before the first instance the node merges `xContext.ReadEvents(type)` of every subscribed type by merge key into an array allocated once from the node arena with the exact count Σ over subscribed types of `ReadEvents(type).GetCount()` (the count arithmetic overflow-checked, [Z2-N-CONTRACT-283]), and each instance iterates it. The count is bounded by the tick's event capacity, Z2-B-GAME-003 (`ecs.maxEventsPerTick`, design default 65,536 while pending): ECS's `EVENT_RECORDS` class rejects every event above it at the emitting merge, so no more reach phase 9; at 24 bytes per entry the array is at most 1.5 MiB at the design default.
   - Late handler node of an Early module: `OnRecover` when pending and the declared phase is Late (never for Early), the handler invocations as above, then a checkpoint-only transaction when the state is dirty and no handler ran (the Early node's final invocation already checkpointed otherwise).
   - The final invocation of the instance in the tick is known at this point ([Z2-N-SCRIPT-389] and the checkpoint rule's step 1); it carries the checkpoint flag.
3. Run each invocation in order as one entity transaction (Transactions.md → "Invocation lifecycle"), each after passing the world's debugger gate in development builds (Debugger.md → "Stop protocol"): `FUNCTION` calls for callbacks and handlers; `RESUME` of the instance's coroutine for a `Run` segment (the coroutine is created by `OnCreate`'s or `OnRecover`'s successful return, or at the first segment when neither exists, with `lua_newthread` + `luaL_sandboxthread` inside a limit-0 protected call, and the coroutine main function pushed on it).
4. A `Run` segment that yields stores the yielded wait descriptor (§ "Wait conditions"); one that returns ends the coroutine (the instance keeps no `Run` until `OnRecover` starts one).
5. On a failed invocation: Transactions.md → "Failure handling by kind" (quarantine; the instance's remaining invocations of this node are skipped).
6. After the last instance: the main VM's GC step; publish the node's transactions to the merge (the merge node reads them from the system record).

### Wait conditions

`@zenith/Core`'s `Zenith.Wait*` functions yield a descriptor the runner interprets; a bare `coroutine.yield()` inside `Run` is `WaitTicks(ctx, 1)`:

| Luau call | Yielded descriptor | The segment resumes |
|---|---|---|
| `Zenith.WaitTicks(ctx, n)` (integer n ≥ 1) | `(1, n)` | in the first node invocation of the instance at tick ≥ current tick + n |
| `Zenith.WaitSeconds(ctx, s)` | `(1, ceil(s × tickHz))`, tickHz from `ctx.DeltaTime` | as WaitTicks |
| `Zenith.WaitAsync(ctx, ticket)` | `(2, completionID)` (a provisional ticket is resolved first, ZS1105 when expired) | in the first node invocation after the result is published |
| any other yielded value | — | treated as `WaitTicks(ctx, 1)` and counted in `script.wait.unknown` (a yield outside `Run` never reaches the runner: the environment raises ZS1302 first) |

Wait descriptors live in the instance record (not in state), so they are lost at quarantine and restore, which is why `OnRecover` restarts the coroutine ([Z2-N-SCRIPT-393]).

## Behaviour instance directory

`Zenith_ScriptInstanceDirectory` (main thread) keeps, per behaviour module, an array of `Zenith_ScriptInstance` records sorted by stable simulation ID:

| Field | Meaning |
|---|---|
| `m_ulSimulationID`, `m_xEntity` | the entity |
| `m_eState` | `SCRIPT_INSTANCE_STATE_ACTIVE` or `_QUARANTINED`, plus the directory-private `NEW`, `DORMANT` (component disabled), `PENDING_RECOVER`, `DESTROYED` (Design/SCRIPT.md → "Behaviour instance state machine") |
| `m_iSelfRef`, `m_iStateProxyRef`, `m_iCoroutineRef` | registry references on the main VM (0 = none) |
| `m_uCheckpointSlot` | the pooled slot pair (HotReload.md → "Checkpoint slot pools"); `m_uActiveSlot` 0 or 1 |
| `m_bDirty`, `m_uUnknownFieldBytes` | dirty flag; preserved unknown fields (HotReload.md → "Restore implementation") |
| `m_xWait` | the wait descriptor |
| `m_uNextOrdinal`, `m_uCurrentSegment` | per-tick ordinal counter; segment counter for view validation ([Z2-N-SCRIPT-442]) |
| `m_ulLastScanEpochSum` | used by the refresh below |

Refresh (step 1 of the behaviour node body):

1. For each chunk of the node's query, compare the chunk's structural epoch with the value recorded at the previous refresh; if every chunk matches and the chunk count is unchanged, the directory is current: return.
2. Otherwise walk every row of the query's chunks, read `Zenith_BehaviourComponent::m_xModule`, map it to a module ID through the world's `Zenith_HashMap<Zenith_StringID, Zenith_ScriptModuleID>` (built at load from the manifest paths), and collect the rows whose module is this node's.
3. Merge-walk the sorted collected simulation IDs against the module's instance array: a new ID inserts a `NEW` instance (checkpoint slots reserved now, on the main thread, outside any transaction; a pool growth failure is the fatal `SCRIPT_ERROR_KIND_RESERVATION` halt); a missing ID marks the instance `DESTROYED` and releases its registry references and slots; `m_bEnabled == false` marks it `DORMANT` (no invocation; its state is kept).
4. A path naming no manifest module is reported once per (entity, path) with `SCRIPT_MODULE_DISABLED` and ZS1801 text.

Cost: O(rows of the query) when structure changed, O(chunks) otherwise. An instance appears in the directory in the first node invocation after the spawn's sync point, so `OnCreate` runs at the first invocation of the instance in or after the phase that follows the spawn ([Z2-N-SCRIPT-282]).

## Tick-boundary handler

The adapter's `OnTickBoundary(Zenith_TickBoundaryStep eStep, uint64 ulTickStarting, Zenith_TickBoundaryAccess& xWorld)` (`Zenith2/Docs/Design/ECS/WorldAPI.md` → "Zenith_ECSObservers header"; cross-area note E12(d): property reads and writes only):

| Step | Actions, in order |
|---|---|
| `TICK_BOUNDARY_STEP_PUBLISH_ASYNC` | § "Asynchronous result table", publication procedure |
| `TICK_BOUNDARY_STEP_RESTORE_CINEMATICS` | nothing (CINE's step) |
| `TICK_BOUNDARY_STEP_APPLY_DISABLES` | 1. For every module with `m_bDisableAtNextTickBoundary`: set its `m_bDisabled` and clear the flag, emit `SCRIPT_MODULE_DISABLED` naming the first failure in merge order (the lowest (system order, logical ID) among its failures), call the listener's `OnModuleDisabled`. 2. Go-live of the reload generations committed in a prologue since the world's previous step 3 (HotReload.md → "Go-live at the tick boundary"), which clears `m_bDisabled` of a module whose reload went live ([Z2-N-SCRIPT-429]). 3. Automatic recoveries whose delay elapsed (HotReload.md → "Quarantine and recovery scheduling"), each clearing its module's `m_bDisabled`. 4. Reset per-tick SCRIPT state: the SCRIPT-held capacity counters of `Zenith_ScriptCapacityLimits`, each instance's `m_uNextOrdinal`, the per-tick counters (after copying them to `m_xTickCounters`), the snapshot-total accumulator, and the provisional-handle remap table of the tick before last. |

The tick-boundary node has ECS's `ANY` affinity (`Zenith2/Docs/Design/ECS/Scheduling.md` → "Node and resource naming"), so the adapter touches no Luau object and calls no `MAIN_THREAD` member of another library — in particular no `Zenith_SystemRegistry` member (`Zenith2/Docs/Design/ECS/SystemAPI.md` → "Zenith_System header": the registry is `MAIN_THREAD`): go-live and last-good restore change only C++ state (generation numbers, system records, live-state blocks, instance states, checkpoint slots, disabled flags) and append every main-VM registry reference they release to `m_aiMainVMReleases`; the module's next behaviour node drains that list and creates the fresh `self` and state proxies on the main thread before its first invocation (§ "Behaviour node body" step 1), which is also where `OnRecover` runs ([Z2-N-ARCH-389]: the main thread runs every world's main VM). Worker VMs reload a chunk-system generation on their own thread (§ "Chunk-system node body" step 3).

The disable is applied at step 3 of the boundary that follows the failure, so every invocation of the failing tick ran ([Z2-N-SCRIPT-189]), and it is exact to the tick because SCRIPT's own `m_bDisabled`, written here and read at the entry of every node and merge body of the module (§ "Chunk-system node body" step 1, § "Behaviour node body" step 1, Transactions.md → "Merge node body" step 1), is the disable: a disabled module's nodes stay in the schedule and complete without running script, and nothing recompiles ([Z2-N-ARCH-410]). A prologue call would not be exact: the number of ticks in a frame varies, so a disable deferred to the next prologue would take effect a scheduling-dependent number of ticks late.

ECS's `Zenith_TickBoundaryAccess::RequestSystemEnabled(uint32 uRegistrationIndex, bool bEnabled)` (`Zenith2/Docs/Design/ECS/Scheduling.md` → "Tick boundary and end-of-tick" step 3; cross-doc row X192 applied) is callable by a handler during `TICK_BOUNDARY_STEP_APPLY_DISABLES` on the tick-boundary node; ECS records the request in the registry's pending set and applies it right after the handlers of that step into the disabled snapshot of this tick, never recompiling. Item 1 requests `false` for each system of a module it disables and items 2 and 3 request `true` for each system of a module they re-enable, so ECS skips a disabled node's body as well and `Zenith_SystemRegistry::IsEnabled` reports the module's systems disabled; `m_bDisabled` stays the disable SCRIPT's own bodies read.

## Asynchronous result table

State (per world): an array of `uSCRIPT_ASYNC_SOURCE_KIND_CAPACITY` (32) source records `Zenith_ScriptAsyncSource { uint32 m_uMaxInFlight; uint32 m_uInFlight; uint32 m_uMaxResultBytes; Zenith_AsyncPublicationHandler* m_pxHandler; }` indexed by the `Zenith_AsyncSourceKind` value (0 … 31; the mirror of ZenithSave's `uASYNC_SOURCE_KIND_CAPACITY`, Design/SCRIPT/PublicAPI.md → "Zenith_ScriptBinding header", raised from 16 by ruling ASYNC-SOURCE-15-16, cross-doc row X449; SCRIPT owns the values as a registry, `Zenith2/Docs/Scripting.md` → "Asynchronous source kind registry", [Z2-N-SCRIPT-924]: 0–16 assigned or provisional as § "Source kind values" lists them, 17–31 unassigned; a consumer names a kind by its enumerator and cites the registry, never a number of its own, [Z2-N-SCRIPT-925]); a `DeclareSource`, `SetHandler`, `SetPublicationResolver` or `AllocateCompletionID` of a value at or above the capacity is engine misuse and fails `ZENITH_ASSERT` (every caller is engine code, before the first tick or on the merge chain); `m_ulNextCompletionID` (starts at 1); a completion ring — a bounded MPSC queue (CORE's `Zenith_BoundedQueue`, `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_BoundedQueue", full policy REJECT) of `Zenith_ScriptCompletionRecord { uint64 m_ulCompletionID; uint8 m_uPublicationKind; uint32 m_uPayloadOffset; uint32 m_uPayloadBytes; }` whose capacity is the sum of every source's `m_uMaxInFlight`; a result arena of Σ(`m_uMaxInFlight` × `m_uMaxResultBytes`) bytes divided into fixed per-source slots, a slot being owned by one completion ID from allocation to release; per slot its requester, `Zenith_ScriptModuleID m_xOwnerModule` and `uint64 m_ulOwnerSimulationID` — stamped by the commit of a script request (Transactions.md → "Commit implementation" step 2) and left at `uSCRIPT_INVALID_MODULE` by a C++ `AllocateCompletionID`; a published table sorted by completion ID holding the results visible to scripts. Per source also: the resolver pair (`Zenith_PfnAsyncPublicationResolve`, user data); `m_uInFlight` is a `std::atomic<uint32>`, because a retired table's `Complete` releases on any thread. Per table: `std::atomic<bool> m_bRetired`, `std::atomic<uint64> m_ulDiscarded`, and a published-ID set of the current replay window (the recorded IDs already published, kept until the window's slots are released). A C++ source declared after the world's creation and before its first tick (a module's through the module publication sink, `Zenith2/Docs/Design/MOD/RuntimeSeams.md` → "Source declaration and handler registration"; GAME's world-load source; the SAVE, PLAT and package bridges) reserves its slots from the world heap at its `DeclareSource`; a reservation failure leaves the source undeclared and returns through the declaring sink's rejection.

| Operation | Procedure |
|---|---|
| `AllocateCompletionID(kind)` | fails `ASYNC_TABLE_ERROR_FULL` when `m_uInFlight == m_uMaxInFlight`; else takes a free payload slot of the kind, `m_uInFlight += 1`, returns `m_ulNextCompletionID++`. Script requests call it at commit in merge order (Transactions.md → "Commit implementation"); their capacity was accepted at the merge, so FULL is unreachable for them |
| `Complete(id, kind, payload)` | any thread: copy the payload into the ID's slot (`PAYLOAD_TOO_LARGE` above the slot; a zero-length `GENERIC_RECORDED` payload is always accepted, the abandoned-request publication of [Z2-N-SCRIPT-931] that every decoder maps to its kind's failure), push the ring record; an unknown or completed ID returns `UNKNOWN_COMPLETION`, and so does, under a publication source, an ID in the published-ID set (its recorded entry was already published and its slot released; nothing is released twice, cross-area row ST21 (c)). On a retired table: release the slot, `m_uInFlight -= 1`, `m_ulDiscarded += 1`, publish nothing |
| `SetPublicationResolver(kind, pfn, user)` | before the first tick, at most one per kind (a second is `ZENITH_ASSERT`, engine code); stored with the source |
| replay adoption (tick boundary step 1, first, under a publication source) | `ulRecorded = m_pxPublicationSource->GetRecordedNextCompletionID(tag, tick)`; 0 (no record) changes nothing; `m_ulNextCompletionID > ulRecorded` is a replay divergence: the world halts (§ "World halt procedure", `SCRIPT_ERROR_INTERNAL`) with the diagnostic `replay divergence at tick <t>: next completion ID live <l>, recorded <r>` ([Z2-N-ARCH-794]); otherwise `m_ulNextCompletionID = ulRecorded`, which skips exactly the IDs the recording's prologue producers allocated live (ruling Q89 (a)). The tick's merge chain then allocates from the recorded value |
| publication (tick boundary step 1, tick T+1 starting) | 0. Under a publication source, the replay adoption above. 1. Drain the ring. 2. Live mode: sort the drained records by completion ID; for each record whose kind has a resolver, call it once, in completion-ID order (it may rewrite the publication kind and the payload, at most the kind's `uMaxResultBytes`); append what the resolvers left to the published table; build the `Zenith_ScriptPublication` list; call `m_pxPublicationObserver->OnPublished(tag, tick, list)`. Replay mode: take `GetRecorded(tag, tick)`; for each recorded entry, a recorded kind (`GENERIC_RECORDED`, `PLATFORM_RECORDED`, `WORLD_LOADED`) publishes the recorded payload (a live completion of the same ID is discarded); a recomputed kind publishes the live completion, which `CanRunTick` guaranteed has arrived; drained completions not in this tick's recorded list stay parked until their recorded tick; every recorded ID published joins the published-ID set; a recorded entry with no live slot (a prologue producer's, which allocated nothing under replay) goes to its kind's handler from the source's buffer and is never retained, because prologue kinds are C++-consumed; resolvers are never called. 3. For each publication with a handler (`ASYNC_SOURCE_PLATFORM`, the PLAT bridge's; `ASYNC_SOURCE_SCENE_LOAD`, ZenithRuntime's forwarder to SCENE), call `OnPublished(xPublication, xWorld)` with the tick boundary's `Zenith_TickBoundaryAccess` (Design/SCRIPT/PublicAPI.md → "Zenith_ScriptHost header"); then, when the slot's `m_xOwnerModule` is invalid (a C++ request, which C++ consumed), the slot is released right after and `m_uInFlight -= 1`; when it is valid (a script request of a handled kind, such as a script's `Scene.LoadAdditive`), the publication stays in the published table for the requester's ticket and is released by the consumption rule below. 4. Script results stay in the published table |
| `ticket:IsReady()` / `ticket:Result()` | binary search of the published table by completion ID from the current invocation (read-only; the table changes only at tick boundaries). For a source kind whose owner registered a result decoder (`Zenith_ScriptBindingTable::SetAsyncResultDecoder`, Design/SCRIPT/PublicAPI.md → "Zenith_ScriptBinding header"), `Result()` calls the decoder with the publication kind and the payload and returns a fresh Luau table built from the `Zenith_ScriptResultFields` it filled — one raw set per field, a `SCRIPT_VALUE_KIND_NIL` field omitting its key, a field carrying `m_szStaticString` pushed as that string, every other value through the marshalling of Design/SCRIPT/Bindings.md → "Marshalling implementation", a `SCRIPT_VALUE_KIND_STRUCT` field pointing into the payload included (copied into a frozen table, Design/SCRIPT/Bindings.md → "Struct fields of a decoded ticket result"); a decoder that returns false halts the world as an internal inconsistency (`SCRIPT_ERROR_KIND_INTERNAL`, § "World halt procedure"), because the payload came from the source's own producer or from the replay log that recorded it. Kind 8's decoder is SCENE's (below) |
| release of a script result | when an invocation that read it (`Result()` or a `WaitAsync` resume) commits, the merge records the ID; at the next tick boundary the slot is released and `m_uInFlight -= 1`; also when the owning instance is destroyed or quarantined. Retention by consumption keeps the arena bounded by the in-flight capacity; reconciliation item S8 records that Scripting.md's capacity table says "returned when: publication of the result" (cross-doc row X118) |
| unconsumed-result trace | a result a behaviour never reads holds one in-flight unit of its source kind until its instance is destroyed or quarantined, so a class can fill with results nobody will consume and every subsequent request of every module is rejected at the merge. Two traces name the holder: the counter `script.async.published_unconsumed.<kind>` (cumulative, the published and not yet released script results per source kind, at each tick boundary); and, when a request is rejected at a merge because its source kind is full and the oldest published-unconsumed result of that kind was published at least `uSCRIPT_ASYNC_STALE_TICKS` (600) ticks earlier, the diagnostic `SCRIPT_ASYNC_CAPACITY_HELD` naming the source kind, the oldest holder's module and simulation ID and its age in ticks, at most once per (source kind, holder) per world session (a Diagnostics row added with cross-doc row X121) |
| `Retire()` | main thread, out of tick, once (GAME's swap step 6 and teardown, `Zenith2/Docs/Design/GAME/WorldHost.md` → "Retired world asynchronous results"): set `m_bRetired`; drain the ring and discard every completed unpublished record (`m_ulDiscarded += n`, slots released); the published table is released with the table; nothing is published again and every handler and resolver pointer is ignored. `IsRetired`, `GetInFlightCount` (Σ `m_uInFlight`), `GetSourceInFlightCount(kind)` and `GetDiscardedCount` read atomics on any thread |
| `ListUnpublished(arena)`, `ListInFlight(arena)` | out of tick, or on the world's phase-11 snapshot node (after every merge node of the tick, before the next boundary): drain the ring into the slots first (out of tick only; on the node the ring is read in place), then one `Zenith_AsyncTableEntry` per completed unpublished record (with its slot's payload, recomputed kinds included) or per allocated, uncompleted slot, ascending completion ID, from the caller's arena; `ASYNC_TABLE_ERROR_FULL` when the arena is exhausted. Published script results held for a ticket are behaviour state's, not these lists' (cross-area row SV3 (a)) |
| `RestoreUnpublished(entries)`, `RestoreInFlight(entries)` | main thread, out of tick, after `RestoreNextCompletionID` and before the first tick after a load: each entry takes a free slot of its declared kind under its own ID (an ID ≥ the next completion ID, a repeated ID or an undeclared kind is `UNKNOWN_COMPLETION`, a payload above the kind's maximum `PAYLOAD_TOO_LARGE`, a kind at capacity `FULL`; the first refusal leaves every entry of the call unapplied); unpublished entries are pushed as completed records and publish at the next boundary; in-flight entries stay allocated for their producer's re-issue or for SAVE's `Complete` at restore (`Zenith2/Docs/Design/SAVE/Formats.md` → "Async section") |
| `AllocateCompletionID(ASYNC_SOURCE_SAVE)` on the phase-11 node | admitted for an engine autosave of a due tick only (the snapshot node is serialised after every merge node and before the next boundary, so the order is fixed); replay checkpoints, out-of-tick submissions, the resume write and the exit autosave allocate no ID (cross-area row SV3 (b)). An `ASYNC_SOURCE_SAVE` result has no awaiting instance (its slot's owner is invalid even for a script `Save` request, whose ID SAVE's bridge allocates), so it is released when its handler returns (SV3 (c)) |
| `CanRunTick(tag, tick)` (replay, prologue) | true unless a recorded recomputed entry of `tick` has not completed; the runtime then runs zero ticks this frame (external-tick mode under `--replay`, `Zenith2/Docs/Design/MOD.md` → "Frame loop driver") |

C++ producers declare their source records before the first tick. A module above ZenithScript reaches the table only through ZenithRuntime's module publication sink (orchestrator ruling Q76 (a); `Zenith2/Docs/Design/MOD/RuntimeSeams.md` → "Module publication sink"): the sink calls `DeclareSource` at the module's attach, `SetHandler` with its forwarder, `AllocateCompletionID` from a merge function of the world's merge chain (ordered with SCRIPT's commit allocations by the merge chain, so a recomputed kind reproduces its IDs under replay) or, for a recorded-only source, from the main thread in the frame prologue, and `Complete` for its parks; for a script source the module declared with `DeclareScriptAsyncSource` the forwarder runs for every publication of the kind, the slot of a ticketless publication is released after it and a script request's publication stays for its ticket, so a mixed kind (`ASYNC_SOURCE_ONLINE`) needs nothing further. A prologue producer of a recorded kind allocates nothing under `--replay` ([Z2-N-SCRIPT-341]); one that needs the recorded ID of its own request under replay — the world host's transition intake, whose `ASYNC_SOURCE_WORLD_LOAD` source holds one ID at a time — takes it from the log (`Zenith_ReplayLogReader::FindNextPublication` of the world and kind after the current tick, then `FindPublication` by that completion ID, `Zenith2/Docs/Design/SAVE/ReplayAPI.md`) and allocates nothing; a request whose ID the log never publishes needs no ID, because a replay completes nothing for it (cross-area row GA11 (d'); cross-doc item X345 (4)). SCENE's additive loads (cross-area rows S10, S21, S22) use kind 8, `ASYNC_SOURCE_SCENE_LOAD`: ZenithRuntime's implementation of `Zenith_ScenePublicationSink` calls `DeclareSource(8, scene.maxAdditiveInstances, 8)` and `SetHandler(8, <its forwarder>)` at world attach, maps `AllocateCompletionID`'s `ASYNC_TABLE_ERROR_FULL` to SCENE's 0, and parks results with `Complete`; both publication kinds carry the 8-byte `Zenith_SceneLoadResult` `{ m_uInstance, m_uCode, m_uDetail, m_uReserved }` (`Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_AdditiveScenes header"): success `PUBLICATION_KIND_GENERIC_RECOMPUTED` with code 0, failure `PUBLICATION_KIND_GENERIC_RECORDED` with the error's code and detail. Its handler forwards each publication to SCENE's publish handler at step 1, with the same `Zenith_TickBoundaryAccess`. A script's `Scene.LoadAdditive` is a script request of kind 8 (its completion ID assigned at commit, its slot's owner stamped), so its publication stays in the published table after the forwarder ran, for the script's ticket; a C++ load's slot is released after the forwarder as for every C++ request. SCENE registers kind 8's result decoder with its `Scene` bindings before `Seal` (`SetAsyncResultDecoder(8, …)`): it decodes the payload with `Zenith_DecodeSceneLoadResult` and fills `instance` (`m_uInstance` as a number; 0 when the load failed before an instance existed), `ok` (`m_uCode == 0`) and `error` (`m_szStaticString` = `Zenith_ToString` of the `Zenith_SceneErrorCode`, or NIL on success), so `ticket:Result()` returns `{ instance: number, ok: boolean, error: string? }`; a payload SCENE's decoder refuses makes the decoder return false. Nothing else in the table is SCENE-specific.

The completion-ID counter and the table are saved: `GetNextCompletionID` is written with the world's state (HotReload.md → "Behaviour state in saves") and restored before the first tick after a load.

### Source kind values

The registry is `Zenith2/Docs/Scripting.md` → "Asynchronous source kind registry" (SCRIPT owns it, [Z2-N-SCRIPT-924]); this table adds where each kind's in-flight capacity comes from. Values 12, 13 and 14 are assigned here in request order (cross-doc item X347, applied to the registry by core-doc patch f; ruling ASYNC-SOURCE-REGISTRY), and 15 and 16 by ruling ASYNC-SOURCE-15-16, which also raises the capacity to 32 so that later kinds need no capacity edit (cross-doc rows X392 and X449; cross-area rows NT2 and SP7):

| Value | Enumerator | Declared by | In-flight capacity from |
|---|---|---|---|
| 0 | `ASYNC_SOURCE_MODULE` | a module, through the module publication sink | the module's `Zenith_ModuleSourceDesc::m_uMaxInFlight` |
| 1 | `ASYNC_SOURCE_PATHFINDING` | AI (script requests) | Z2-B-SCRIPT-026's path value (`m_auAsyncInFlight[0]`, the AI row Z2-B-AI-003) |
| 2 | `ASYNC_SOURCE_STREAMING` | STREAM (script requests) | Z2-B-SCRIPT-026's streaming value |
| 3 | `ASYNC_SOURCE_PHYSICS_QUERY` | PHYS (script requests) | Z2-B-SCRIPT-026's physics-query value |
| 4 | `ASYNC_SOURCE_ONLINE` | ONLINE (script requests and its prologue notifications; a mixed kind) | Z2-B-SCRIPT-026's online value plus 8 notifications per world |
| 5 | `ASYNC_SOURCE_SAVE` | SAVE's runtime bridge | `uSaveMaxOutstanding` (`Zenith2/Docs/Design/SAVE.md` → "Asynchronous source capacity") |
| 6 | `ASYNC_SOURCE_WORLD_LOAD` | GAME's world host | 1 per world |
| 7 | `ASYNC_SOURCE_PLATFORM` | the PLAT bridge | the PLAT bridge's declaration (`Zenith2/Docs/Design/PLAT.md` → "Threading lifetime and error contract") |
| 8 | `ASYNC_SOURCE_SCENE_LOAD` | ZenithRuntime's scene sink | `scene.maxAdditiveInstances` |
| 9 | `ASYNC_SOURCE_ANIMATION` | ANIM, through the module sink | `animation.bindingQueueCapacity` + `animation.streaming.maxReadsInFlight` |
| 10 | `ASYNC_SOURCE_AUDIO` | AUDIO (script requests, `DeclareScriptAsyncSource`) | Z2-B-SCRIPT-026's audio value |
| 11 | `ASYNC_SOURCE_UI` | UI, through the module sink (prologue) | `ui.events.maxInFlight` |
| 12 | `ASYNC_SOURCE_CINEMATICS` | CINE, through the module sink (merge chain) | 64 per world (CINE's declaration) |
| 13 | `ASYNC_SOURCE_NAVIGATION` | AI, through the module sink (merge chain) | `ai.nav.maxLoadsInFlight` |
| 14 | `ASYNC_SOURCE_PACKAGE` | PACK's runtime package bridge (prologue) | `uPACKAGE_EVENT_QUEUE_CAPACITY` (512) |
| 15 | `ASYNC_SOURCE_NETWORK` | NET, through the module sink (prologue): snapshot fragments, session events and, at Stage 7, message pieces and peer-view records, every publication `PUBLICATION_KIND_GENERIC_RECORDED`, at most 1,024 bytes, never allocated under `--replay` (`Zenith2/Docs/Design/NET/Loopback.md` → "Fragment publication") | `net.client.maxFragmentsInFlight` (256) per client session world; 64 per server session world |
| 16 | `ASYNC_SOURCE_XR` | ZenithXR (Stage 7), a mixed kind declared with `DeclareScriptAsyncSource`: ticketless notifications allocated on the main thread in the frame prologue and never under `--replay`, consumed by the module's handler at tick-boundary step 1; every publication `PUBLICATION_KIND_GENERIC_RECORDED`, at most 64 bytes (`uXR_NOTIFY_PAYLOAD_BYTES`, `Zenith2/Docs/Design/SCOPE/XrPublicAPI.md` → "Event types") | `uXR_NOTIFY_MAX_OUTSTANDING` (8 per world) |
| 17 … 31 | none | unassigned | — |

## Committed checkpoints in the tick hash

`Zenith_ScriptWorld::AppendCommittedStateHash(xHash)` (core text: `Zenith2/Docs/Scripting/LibraryLayout.md` → "Public API of Zenith_ScriptWorld", X345) feeds the world-state hash of `Zenith2/Docs/Scripting.md` → "Determinism test matrix" its behaviour input: for every behaviour instance past NEW, in ascending (source ID, simulation ID), it appends the little-endian `uint64` source ID, `uint64` simulation ID, `uint8` lifecycle state, `uint32` checkpoint length and the active slot's committed checkpoint bytes (an instance with no committed checkpoint appends length 0). It reads only committed state, so it is called on the world's tick-boundary node before step 1 of the next tick (SAVE's replay recorder and player, through their hash-contribution hook, `Zenith2/Docs/Design/SAVE/RuntimeBridge.md`) or out of tick; it allocates nothing and is `ANY_THREAD` while no phase of the world runs. Dirty state not yet checkpointed is not hashed: the checkpoint is the committed form.

## Random draws

`Internal/Zenith_ScriptRandom.cpp` fills the 40-byte `Zenith_ScriptRandomKey` of Scripting.md → "Counter-based random numbers" from the running entity transaction: world seed (`m_ulWorldSeed`), source ID (`Zenith_ScriptSystemRef::m_ulSourceID`), tick, simulation ID, ordinal and the transaction's `m_uDrawCounter` (for a recovery transaction the ordinal is `uSCRIPT_RECOVERY_ORDINAL`, HotReload.md → "Recovery invocations and exact restore" step 2, [Z2-N-SCRIPT-928]), serialized little-endian field by field into a 40-byte buffer and hashed with `Zenith_Hash64` (D-SEL-HASH, open, Stage 1). The derivations of the Random API table call CORE's `Zenith_RandomToU32`, `Zenith_RandomToUnitDouble`, `Zenith_RandomToUnitFloat`, `Zenith_RandomRange`, `Zenith_RandomUnitVector` and `Zenith_RandomShuffle` with a draw functor that returns `Zenith_ScriptRandomDraw(key)` and increments the counter (`Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_RandomSource", cross-area note C17), so SCRIPT holds no second implementation. `ctx.Random` is a per-VM userdata whose methods read the VM's current entity transaction; used outside an invocation it raises ZS1001.

## World halt procedure

On a fatal outcome (VM memory cap, transaction-arena growth failure, watchdog, internal exception, invocation-counter wrap), on the thread that observed it. Every runner routes a non-OK protected-call outcome through `Zenith_ScriptRouteFatalOutcome(xWorld, xError, ulTick)` (`Internal/Zenith_ScriptHalt.h`), which runs the steps below for the fatal kinds `SCRIPT_ERROR_KIND_RESERVATION`, `_VM_MEMORY`, `_WATCHDOG` and `_INTERNAL` and returns true, and returns false for every other kind (the invocation's own rollback); the counter wrap is detected at issue and calls `Zenith_ScriptHaltWorld` directly:

1. `m_bHalted.store(true, release)`; the first thread to set it (compare-exchange) continues, others return.
2. Record the diagnostic; call `m_pxListener->OnWorldHalted(tag, tick, error)`; ZenithRuntime's listener sets `REPLAY_TICK_FLAG_NON_REPLAYABLE` on the tick's `TICK` record and records the halt for the frame loop ([Z2-N-SCRIPT-242]).
3. Every script node and merge node of the world checks `m_bHalted` at entry and before each invocation and returns without running script or committing; the merge of the current tick commits nothing ("the whole tick is abandoned").
4. At the next tick boundary the world is `SCRIPT_WORLD_STATE_HALTED`; `RequestHotReload`, `RequestModuleReload` and `RestoreSavedStates` return `SCRIPT_ERROR_WORLD_HALTED`.

What the runtime does with a halted world (the Player stops with exit code 1 after reporting; the editor stops the play world and keeps the edit world) is MOD's (cross-area note).

## Profiling implementation

Zones use `constexpr Zenith_ZoneSite` constants per zone and the installed profiler hooks (`Zenith2/Docs/Contracts.md` → "No mutable global state"): `Script.Node` (per node, from the frame executor's node zone), `Script.Invocation` with the module's name as a dynamic zone name copied at the call (cross-area note J10: the hook copies the name) and the entity count or simulation ID as the zone value, `Script.Merge`, `Script.Checkpoint`, `Script.Restore`, `Script.HotReload`, `Script.GC` ([Z2-N-SCRIPT-464], [Z2-N-SCRIPT-465]). Counters are published once per tick from the tick boundary through `Zenith_DispatchCounter` with `COUNTER_KIND_CUMULATIVE` for running totals and `COUNTER_KIND_DELTA` for per-tick values ([Z2-N-CONTRACT-155]); the names are Design/SCRIPT.md → "Counters". The on-screen profiler rows (DEVT's overlay) read these counters and zones; SCRIPT computes no row itself.

<!-- assumption: pending core-doc (X121) -->
`Zenith_ScriptCounters` of `Zenith2/Docs/Scripting/PublicTypes.md` → "ZenithScript internal types" (what `Zenith_ScriptDiagnostics::GetTickCounters` returns to tests) gains the fields behind the counters this design adds; the per-VM heap counter `script.vm.bytes.<index>` is read from each VM's `m_ulBytesInUse` and has no field:

```cpp
// elaboration: members appended to Zenith_ScriptCounters
	uint64 m_ulGCKilobytes = 0;               // script.gc.kb
	uint64 m_ulGCNanoseconds = 0;             // script.gc.ns
	uint64 m_ulTxnArenaGrowths = 0;           // script.txn.arena_growths (cumulative)
	uint64 m_ulDiagnosticsDropped = 0;        // script.diagnostics_dropped (cumulative)
	uint64 m_ulUnknownWaits = 0;              // script.wait.unknown (cumulative)
	uint32 m_auPublishedUnconsumed[uSCRIPT_ASYNC_SOURCE_KIND_CAPACITY] = {};  // script.async.published_unconsumed.<kind>, indexed by Zenith_AsyncSourceKind
```
<!-- /assumption -->

## Runtime internal files

| File | Contents |
|---|---|
| `Internal/Zenith_ScriptRuntimeImpl.cpp` | `Zenith_ScriptRuntime` members, `Zenith_ScriptRegisterEngineTypes` |
| `Internal/Zenith_ScriptWorldImpl.cpp` | `Zenith_ScriptWorld` members, `Create` steps, `LoadModules`, `RegisterScriptNodes` |
| `Internal/Zenith_ScriptVM.{h,cpp}` | `Zenith_ScriptVM`, `Zenith_ScriptVMDesc` (PublicTypes.md), creation and destruction |
| `Internal/Zenith_ScriptProtectedCall.{h,cpp}` | the protected entry (the one protection and `catch` site) |
| `Internal/Zenith_ScriptAllocator.cpp` | `Zenith_ScriptAlloc` |
| `Internal/Zenith_ScriptInterrupt.cpp` | `Zenith_ScriptInterrupt`, abort values |
| `Internal/Zenith_ScriptWatchdog.{h,cpp}` | `Zenith_ScriptWatchdog` members and thread |
| `Internal/Zenith_ScriptChunkRunner.{h,cpp}` | `Zenith_ScriptChunkNodeRun`, the row trampoline |
| `Internal/Zenith_ScriptBehaviourRunner.{h,cpp}` | `Zenith_ScriptBehaviourNodeRun`, wait conditions |
| `Internal/Zenith_ScriptInstanceDirectory.{h,cpp}` | the instance directory |
| `Internal/Zenith_ScriptTickBoundary.cpp` | the tick-boundary adapter |
| `Internal/Zenith_ScriptAsync.cpp` | `Zenith_AsyncResultTable`, publication, tickets |
| `Internal/Zenith_ScriptRandom.{h,cpp}` | the random key and draws |
| `Internal/Zenith_ScriptGC.cpp` | GC scheduling |
| `Internal/Zenith_ScriptProfiling.{h,cpp}` | zone sites, counter publication |
| `Internal/Zenith_ScriptDiagnostics.{h,cpp}` | `Zenith_ScriptDiagnostics` (PublicTypes.md), the diagnostic store |
| `Internal/Zenith_ScriptHalt.cpp` | the world halt procedure |
