# Zenith2 Scripting — required conformance test tables

Companion to `Zenith2/Docs/Scripting.md` (whose conformance-test section, bullets [Z2-N-SCRIPT-521] to [Z2-N-SCRIPT-523], is the top-level contract): the tables of every conformance test the Scripting document family names, split out for length. A quoted heading without a path in a cell is a heading of `Zenith2/Docs/Scripting.md`. Normative bullets share the `SCRIPT` numbering of the Scripting document family.

## Conformance test tables

### Transaction tests

| Test | Assertion |
|---|---|
| `Test_Script_Txn_ConcurrentDisjointColumnWrites` | two chunk systems in one phase writing disjoint fields of the same component over the same chunks, on 8 workers, one of them failing on every 3rd entity: the other system's writes are all committed and never restored; the failing system's successful rows are committed |
| `Test_Script_Txn_FailureAfterDestroyKeepsEntity` | an invocation queues `ctx:Destroy(e)` then raises: `e` is alive after the sync point and at the end of the tick |
| `Test_Script_Txn_FailureAfterEventNotDelivered` | an invocation emits an event then raises: no subscriber sees the event in phase 9 or 10 |
| `Test_Script_Txn_SnapshotCapExhaustion` | a behaviour growing a container shadow past the per-entity cap fails with `SCRIPT_ERROR_KIND_SNAPSHOT_CAP` / ZS1402; its rows and container are unchanged; other entities commit |
| `Test_Script_Txn_ContainerShadowDroppedOnFailure` | a behaviour mutates a container field, reads back its own mutation (asserted inside the script), then raises: the committed container is byte-identical to before |
| `Test_Script_Txn_SpawnIdsReleasedOnFailure` | an invocation spawns 3 entities then raises: no entity is created, entity capacity is unchanged, and the next successful spawn receives the stable simulation ID it receives in a control run without the failed spawn |
| `Test_Script_Txn_EntityBoundaryMiddleFailure` | the worked example of Scripting.md → "Worked example three entities second fails", in one chunk, three chunks and on three workers |
| `Test_Script_Txn_CrossEntityReadRule` | a system declaring a cross-entity read of a column written in its phase fails schedule compilation with `SCRIPT_SCHEDULE_CROSS_ENTITY_READ_OF_WRITTEN_COLUMN`; an undeclared runtime read of another row of an own written column raises ZS1204 |
| `Test_Script_Txn_WriteOutsideAuthority` | a system writing a field owned by another phase fails schedule compilation with `SCRIPT_SCHEDULE_WRITE_OUTSIDE_AUTHORITY`; the same write through `ctx:Command` is applied at the next sync point |
| `Test_Script_Txn_DisabledFromNextTickBoundary` | a system failing on its first entity still processes every other entity in that tick, and runs no invocation in the next tick |
| `Test_Script_Txn_CommitNeverInvalidatesOtherViews` | systems A and B of one phase with disjoint access (so B has no hazard edge on A's merge): A's merge and commit run while B's sub-jobs run, and B's views stay valid |
| `Test_Script_Merge_DependentsRunAfterMerge` | see Scripting.md → "Schedule compilation of script nodes": a dependent's own-row read of a column whose writer's entity is rejected at merge observes the restored value, on 1 and 8 workers |

### Merge, capacity and determinism tests

| Test | Assertion |
|---|---|
| `Test_Script_Merge_OrderIndependentOfCompletion` | sub-jobs forced to complete in reverse order (test scheduler hook) produce the same queue contents as forward order |
| `Test_Script_Merge_CapacityRejectionDeterministic` | with 10 event slots and 25 emitting entities spread over chunks and workers, exactly the 10 lowest stable simulation IDs are accepted, the rest rolled back with `SCRIPT_MERGE_REJECTED_CAPACITY`, in every worker count |
| `Test_Script_Merge_SnapshotLivesUntilMerge` | an entity rejected at the merge point after its call returned has its rows restored to their pre-call values |
| `Test_Script_Merge_ChangeVersionsAdvanceAtMerge` | a column's change-tracking version is unchanged during the invocation and advances exactly once at the merge point when an accepted entity wrote it; it does not advance when every writer failed |
| `Test_Script_Merge_ChainInSystemOrder` | two systems of one phase competing for entity capacity: the earlier system in system order always wins, whichever finishes first |
| `Test_Script_Reduction_FixedOrderTree` | a `SumF64` over 10,000 contributions with values chosen to expose summation-order error gives the bit-identical result for 1, 2, 8 and all workers and both chunk capacities, and equals a reference fold computed by the test in merge order with the documented tree |
| `Test_Script_Spawn_IdsAssignedAtPlaybackInMergeOrder` | spawns from parallel invocations receive stable simulation IDs in (system order, logical ID, sequence) order |
| `Test_Script_Event_DeliveryInMergeOrder` | handlers observe events in merged order, independent of worker count |
| `Test_Script_Random_RerunIdentical` | re-running a failed invocation in a replay yields the identical draw sequence |
| `Test_Script_Random_ParallelIdentical` | two chunk systems drawing in parallel produce the same per-entity sequences as when run serially, and as with 1 worker |
| `Test_Script_Random_KeyLayout` | `Zenith_ScriptRandomKey` is 40 bytes and draws for a fixed key equal the recorded golden values |
| `Test_Script_Determinism_ReplayMatrix` (conformance executable) | the matrix of Scripting.md → "Determinism test matrix" gives identical per-tick world-state hashes per backend |
| `Test_Script_Determinism_AddressKeyedIterationLint` | ZenithCook rejects an iteration over a table keyed by entities with `SCRIPT_LINT_ADDRESS_KEYED_ITERATION`, and accepts the same loop keyed by `entity:Id()` |
| `Test_Script_Async_PublishedAtTickBoundaryInCompletionOrder` | results completing in scrambled wall-clock order are visible from the next tick, in completion-ID order |
| `Test_Script_Async_ReplayPublishesAtRecordedTick` | a replay whose recomputed path result is ready early still publishes it at the recorded tick; one ready late holds the tick |

### VM, firewall and budget tests

| Test | Assertion |
|---|---|
| `Test_Script_VM_ParallelChunkSystemsSeparateWorkerVMs` | two chunk systems in parallel on separate worker VMs: each worker's VM identity (`Zenith_ScriptDiagnostics::GetVMIndex`) is distinct, results equal the 1-worker run |
| `Test_Script_VM_LazyWorkerVMCreation` | a world whose chunk systems run on 3 of 8 workers creates exactly 3 worker VMs |
| `Test_Script_VM_MemoryCapFatal` | a script allocating past the worker VM cap produces `SCRIPT_ERROR_KIND_VM_MEMORY`, halts the world and sets `REPLAY_TICK_FLAG_NON_REPLAYABLE` on the tick's `TICK` record |
| `Test_Script_Determinism_WorkerFloatEnvironment` | every worker and the main thread read back the fixed state of Scripting.md → "Floating-point environment" (FTZ/DAZ set on x86-64, FZ/DN set on arm64) and flush a denormal product to zero |
| `Test_Script_VM_GcNotObservable` | `collectgarbage`, `gcinfo`, `__mode` and `__gc` are unavailable; results are identical with GC step budgets of 0 KB and 1,024 KB |
| `Test_Script_Firewall_RaiiUnwindsThroughBinding` | see Scripting.md → "The two RAII tests" |
| `Test_Script_Firewall_ReentrantCallbackErrorCaught` | see Scripting.md → "The two RAII tests" |
| `Test_Script_Firewall_LuauApiLinkageRecorded` | the measured Luau API linkage equals the value recorded in `ThirdParty/manifest.json` |
| `Test_Script_Firewall_ProtectedDepthLimit` | nine nested callback re-entries: the ninth is refused with ZS1701 and no Luau code runs for it |
| `Test_Script_Budget_SafepointCountDeterministic` | a representative module's per-entity safepoint counts are identical across 10 runs, 1 and 8 workers, and both chunk capacities |
| `Test_Script_Budget_ExhaustionRollsBack` | a spinning `Each` fails with ZS1401 for that entity only; a script `pcall` around the spin does not catch it |
| `Test_Script_Budget_GcInterruptsNotCounted` | forcing GC steps inside an invocation does not change its safepoint count |
| `Test_Script_Budget_NativeSafepointParity` | interpreted and native safepoint counts and results match over the budget corpus (see Scripting.md → "Native codegen conformance"); run on Windows x64 and the S24 |
| `Test_Script_Watchdog_FatalNotRollback` | a binding that sleeps past the watchdog limit produces a fatal `SCRIPT_ERROR_KIND_WATCHDOG` (or, when stuck past twice the limit, the hang report), never a rollback |

### State, recovery and reload tests

| Test | Assertion |
|---|---|
| `Test_Script_Recovery_QuarantineThenReloadLastGood` | an instance failing at tick 50 after committing state at tick 49 is quarantined; after reload its state equals the tick-49 checkpoint and `OnRecover` ran once |
| `Test_Script_Behaviour_YieldBoundariesCommit` | a coroutine writing a component, yielding, then raising in the next segment: the first segment's write is committed, the second's is rolled back |
| `Test_Script_View_RetainedPastInvocationErrors` | a closure capturing a component view in a local is stored through the test binding `Test.StoreCallback` (retained with `Zenith_ScriptInvocationContext::RetainCallback`) and invoked in a subsequent invocation through `Zenith_ScriptInvocationContext::InvokeCallback`: the view access raises ZS1001 in Debug, Release and the shipping configuration |
| `Test_Script_View_UsedAfterYieldErrors` | a view obtained before `Zenith.WaitTicks` and used after it raises ZS1002 |
| `Test_Script_View_StaleEpochErrors` | a view whose chunk was restructured at a sync point raises ZS1003 |
| `Test_Script_System_YieldRaisesError` | `coroutine.yield()` inside `Each` raises ZS1301 (abort class) and rolls back that entity |
| `Test_Script_Stateless_AdversarialCorpusRejectedAtLoad` | every adversarial corpus module is rejected at load with its expected diagnostic |
| `Test_Script_Stateless_AcceptedCorpusLoads` | every accepted corpus module loads and runs |
| `Test_Script_State_CheckpointSerializationFailureRollsBack` | a behaviour that stores 17 provisional spawn handles into state in its final invocation of the tick exceeds `uSCRIPT_MAX_CHECKPOINT_FIXUPS` and raises `SCRIPT_ERROR_KIND_CHECKPOINT` (ZS1404); the invocation's writes and side effects roll back; the previous checkpoint stays active |
| `Test_Script_State_RestoreDestroyedReferenceBecomesNil` | an instance whose state references an entity destroyed before restore gets `nil` in that field and a `SCRIPT_STATE_REFERENCE_UNRESOLVED` diagnostic |
| `Test_Script_State_UnserializableKindsLoadError` | declaring a function, thread or foreign userdata field fails to load with the field named |
| `Test_Script_State_CheckpointDirtyOnlyAtMostOncePerTick` | an instance with three invocations per tick and dirty state serializes exactly once per tick; an unchanged instance never |
| `Test_Script_State_NoSharedMutableTables` | assigning one table to two instances' state fields and mutating it afterwards changes neither instance's state |
| `Test_Script_State_UnknownFieldsPreserved` | a checkpoint with a field ID unknown to the module round-trips with a warning |
| `Test_Script_Recovery_OnRecoverRestartsCoroutine` | after restore, the coroutine started by `OnRecover` resumes logic from the phase recorded in state, and no execution position was restored |
| `Test_Script_HotReload_MigrationSucceeds` | a reload with `StateVersion` 1 → 2 migrates every instance through `Migrate[1]` and resumes |
| `Test_Script_HotReload_RollbackOnError` | a reload whose `Migrate[1]` fails for one of 100 instances leaves all 100 on the old module with unchanged heap state |
| `Test_Script_HotReload_SystemGeneration` | a chunk-system reload is picked up by every worker VM before its next invocation, and a failing reload leaves the old generation in use |

### Module, binding, sandbox and tooling tests

| Test | Assertion |
|---|---|
| `Test_Script_Module_RequireLiteralOnly` | a non-literal `require` argument is rejected at compile; a `require` during an invocation raises its catalogued misuse error |
| `Test_Script_Module_RequireCycleError` | a two-module cycle fails to load with `a -> b -> a` in the message |
| `Test_Script_Module_DeepFrozen` | writing into a nested table of a loaded module raises the pinned Luau readonly-table error listed in `Scripting/MisuseCatalogue.md` → "Catalogue" |
| `Test_Script_Sandbox_EnvironmentSubset` | trigger scripts compiled with the production options (O2 and the mutable-globals list): every function listed as removed in `Scripting/Bindings.md` → "Environments and library subsets" raises its catalogue message in both environments; every listed function is present; the replacements run (`tostring({}) == "table"`, `setmetatable(t, { __mode = "k" })` raises ZS1601, `rawset` raises ZS1602) |
| `Test_Script_Sandbox_NoFastcallForReplacedBuiltins` (ZenithScriptCompiler test target) | `Zenith_ScriptListFastcallSites` over the `EnvironmentSubset` trigger scripts compiled with `Zenith_ScriptProductionCompileOptions()` returns sites none of whose builtin names, compared by the root segment before the first `.` (so `math.floor` compares as `math`) or whole when there is none, is in `Zenith_ScriptSandboxMutableGlobals()`; the test includes no Luau header |
| `Test_Script_Sandbox_PcallCannotCatchAbort` | `pcall` around a budget, snapshot-cap, yield-in-system or checkpoint abort re-raises it; `pcall` around `error("x")` returns `false, "x"` |
| `Test_Script_Sandbox_TostringNoAddress` | `tostring({})` is `"table"`; no address appears in any string produced by the environment |
| `Test_Script_Bindings_DluauGolden` | the `.d.luau` generated for a fixture registry (engine test types plus two data-defined schemas) is byte-identical to the committed golden file `Tests/Golden/fixture.d.luau` |
| `Test_Script_Types_NameMapping` | every case of the mapping table in `Scripting/Bindings.md` → "Name mapping from Zenith Hungarian" maps as listed, and each collision case fails generation with both sources named |
| `Test_Script_Bindings_ContextEnforced` | a behaviour-only binding called from a chunk system raises its catalogued error; a load-only binding called during an invocation likewise |
| `Test_Script_Misuse_Catalogue` | table-driven: every catalogue row's trigger script raises exactly its catalogue message |
| `Test_Script_Misuse_NeverAsserts` | the catalogue scripts run in a Debug build with an assert handler that fails the test: no assert fires |
| `Test_Script_Dap_BreakpointFanOut` | a breakpoint set before and after two worker VMs are created stops each VM at the line; continuing yields the same world-state hash as an undebugged run |
| `Test_Script_Bytecode_VersionMismatchRefused` | `.zlbc` files with a flipped engine hash, Luau pin hash, binding-set hash and bytecode version are each refused with the mismatched field named |
| `Test_Script_Bytecode_DevAndCookIdentical` | in-process compilation and the cook produce byte-identical Luau blobs for the representative corpus |
| `Test_Script_Profiling_OverlayRows` | after a tick with behaviours, chunk systems and GC, the Luau time, Luau GC and script counter rows are populated with non-zero values |
| `Fuzz_Script_CheckpointDecode` (libFuzzer, `win-asan`) | the checkpoint decoder never reads out of bounds, never allocates beyond the slot size, and reports every malformed input as a restore failure |
