# SCRIPT design — test catalogue

Companion to `Zenith2/Docs/Design/SCRIPT.md`: every SCRIPT test with its file, target, the case it asserts and the gate step that runs it. `Zenith2/Docs/Scripting.md` → "Required conformance tests" and "Fault-matrix tests" are the specification of the tests they name; this file assigns each to a file and target and adds the tests for the compiler, the cook, the DAP server, hot reload and this design's own mechanisms. Every test is doctest unless marked, registered with CTest under the label `script-conformance` ([Z2-N-SCRIPT-523]), pass/fail only, and added in the same ticket as the code it tests ([Z2-N-SCRIPT-521]).

## Test files and targets

| Target | Created by | Directory | Links | Matrix and registration |
|---|---|---|---|---|
| `ZenithScript_Tests` | `zenith_add_library(ZenithScript … TEST_DEPENDS ZenithECS_Custom)` | `Zenith2/Engine/ZenithScript/Tests/` | ZenithScript, ZenithECS_Custom, ZenithTestSupport, doctest | `ZENITH_ALL`; CTest in `win`, `win-asan`, `linux-tsan`; built on Android and run in the S24 acceptance runs |
| `ZenithScript_Conformance` | the library's `Tests/Conformance/` with `TEST_DEPENDS ZenithECS_Custom ZenithECS_Flecs ZenithECS_EnTT` | `Zenith2/Engine/ZenithScript/Tests/Conformance/` | ZenithScript, all three ECS backends | as above |
| `ZenithScriptCompiler_Tests` | `zenith_add_library(ZenithScriptCompiler …)` | `Zenith2/Engine/ZenithScriptCompiler/Tests/` | ZenithScriptCompiler, ZenithScript, ZenithTestSupport, doctest | `ZENITH_HOST`; CTest in `win`, `win-asan`, `linux-tsan`; never built for Android, so the host-only tests live here |
| `ZenithReflect_Tests` (SCRIPT's cases) | ECS's target | `Zenith2/Engine/ZenithReflect/Tests/Zenith_ScriptBinding_Tests.cpp` | ZenithReflect | `ZENITH_ALL` |
| `Zenith_Fuzz_Script_*` | `Tests/Fuzz/<Stem>.cpp` | `ZenithScript/Tests/Fuzz/`, `ZenithScriptCompiler/Tests/Fuzz/` | libFuzzer | `win-asan`, 60-second smoke runs |
| `Zenith_Bench_Script*` | `zenith_add_benchmark` | `Zenith2/Benchmarks/Script*/` | ZenithScript, ZenithScriptCompiler, ZenithECS backends | `win`, CTest `--smoke` run |
| `Game_<Name>_ScriptMigrations` | the registry's `zenith_script_game_migrations(<Name>)` (SCRIPT block), one per game with `Scripts/Tests/Migration/` | `Zenith2/Games/<Name>/Scripts/Tests/Migration/` ([Z2-N-SCRIPT-400]) | the game's development Player with the gametest option `--script-migration-fixtures <dir>` (MOD, A5 cross-area row SB30), calling `Zenith_ScriptRestoreMigrationFixtures` | `win`, headless |

<!-- assumption: pending ARCH -->
`ZenithScript_Conformance` links all three ECS backends, but ZenithScript is not a backend family: this design assumes `zenith_add_library`'s `TEST_DEPENDS` targets are also linked into a non-backend library's `<Name>_Conformance` executable (`Zenith2/Docs/Architecture/BuildSystem.md` → "The zenith_add_library helper"; cross-area row SC13).
<!-- /assumption -->

Source files of `ZenithScript_Tests`: `Zenith_ScriptFirewall_Tests.cpp`, `Zenith_ScriptVM_Tests.cpp`, `Zenith_ScriptTxn_Tests.cpp`, `Zenith_ScriptMerge_Tests.cpp`, `Zenith_ScriptRandom_Tests.cpp`, `Zenith_ScriptBudget_Tests.cpp`, `Zenith_ScriptState_Tests.cpp`, `Zenith_ScriptRecovery_Tests.cpp`, `Zenith_ScriptHotReload_Tests.cpp`, `Zenith_ScriptModule_Tests.cpp`, `Zenith_ScriptSandbox_Tests.cpp`, `Zenith_ScriptBindings_Tests.cpp`, `Zenith_ScriptViews_Tests.cpp`, `Zenith_ScriptMisuse_Tests.cpp`, `Zenith_ScriptAsync_Tests.cpp`, `Zenith_ScriptDap_Tests.cpp`, `Zenith_ScriptProfiling_Tests.cpp`, `Zenith_ScriptDesign_Tests.cpp` (this design's own mechanisms), `Zenith_ScriptDefect_Tests.cpp` (intentional-defect tests, one CTest entry each), plus the header-only `Support/Zenith_ScriptTestWorld.h` and its sibling support headers (a non-ZenithCore `Tests/Support/` is never compiled, so every support function is `inline`, [Z2-N-ARCH-255]; a one-world harness over the Custom backend, a frame executor with a test scheduler hook, and the test bindings `Test.FailingThunk`, `Test.CallBack`, `Test.StoreCallback`, `Test.SleepMs`). Data: `Tests/Corpus/{Stateless,Budget,Representative}/`, `Tests/Scenarios/<TestName>/`, `Tests/Misuse/<Code>.luau`, `Tests/Golden/`.

## Firewall and VM tests

| Test | File | Case |
|---|---|---|
| `Test_Script_Firewall_RaiiUnwindsThroughBinding` | `Zenith_ScriptFirewall_Tests.cpp` | Scripting.md → "The two RAII tests" row 1 |
| `Test_Script_Firewall_ReentrantCallbackErrorCaught` | same | row 2, with the budget variant |
| `Test_Script_Firewall_LuauApiLinkageRecorded` | `ZenithScriptCompiler/Tests/Checks/Zenith_ScriptLuauLinkage.cmake` (a `cmake -P` CTest of the registry's SCRIPT block, label `z2-cmake`; host presets only, so the S24 runs never contain it) | the symbol inspection of Scripting.md → "Luau API linkage check" equals the manifest's `apiLinkage` read with `string(JSON)` (runs `dumpbin`/`llvm-nm` on the built `Luau.VM` and `Luau.Compiler`; on a host preset without the tool the check prints code 1 and fails, never skips) |
| `Test_Script_Firewall_LuauApiLinkageFlipped` | same script over `Checks/Fixtures/manifest_flipped.json` | the inverted `apiLinkage` makes the check print `ZENITH_VERIFY_CODE=3` |
| `Test_Script_Firewall_ProtectedDepthLimit` | same | nine nested re-entries; the ninth refused with ZS1701, no Luau code ran for it |
| `Test_Script_Firewall_InternalExceptionIsFatal` | same | the foreign-exception conversion `Zenith_ScriptForeignExceptionError` (the test seam both non-Luau `catch` clauses of the protected entry call; no Zenith2 code writes `throw`, [Z2-N-SCRIPT-030], and no test calls `operator new`, Contracts.md F1) returns `SCRIPT_ERROR_KIND_INTERNAL` with the text in the VM's error scratch and the VM's next call succeeds; the extended form passes that error to `Zenith_ScriptRouteFatalOutcome` (the routing every runner applies to a protected-call outcome, Runtime.md → "World halt procedure"), which halts the test world with `OnWorldHalted` called once and the next tick committing nothing, while a `SCRIPT_ERROR_KIND_RUNTIME` error halts nothing; the `catch` clause that produces a live internal error is a review item |
| `Test_Script_Firewall_UnprotectedAllocationAsserts` | same (death-test suite `z2-death-assert`) | a direct call of the allocator hook `Zenith_ScriptAlloc` outside any protected call after construction fires the Debug allocation assert (no `lua_*` call from `Tests/`, Contracts.md F7) |
| `Test_Script_VM_ParallelChunkSystemsSeparateWorkerVMs` | `Zenith_ScriptVM_Tests.cpp` | Scripting.md's row |
| `Test_Script_VM_LazyWorkerVMCreation` | same | 3 of 8 workers run chunk systems → exactly 3 worker VMs |
| `Test_Script_VM_MemoryCapFatal` | same | Scripting.md's row |
| `Test_Script_VM_WorkerTotalRefused` | same | a world whose worker cap × frame threads exceeds Z2-B-SCRIPT-011 fails `Create` with `SCRIPT_ERROR_INVALID_DESC` naming the row |
| `Test_Script_VM_SlotIndexMapping` | same | the VM a leaf on slot s uses reports VM index s + 1; the main VM reports 0; no invocation ID has VM index 0xFFFF |
| `Test_Script_VM_InvocationCounterWrapHalts` | same | a VM whose counter is set to 2⁴⁸ − 1 (test access) halts the world at the next issue |
| `Test_Script_VM_GcNotObservable` | same | Scripting.md's row |
| `Test_Script_VM_GcStepsWithinAllowance` | same | a VM never steps more than Z2-B-SCRIPT-007's test value per tick; critical pressure quadruples it |
| `Test_Script_VM_TeardownOrder` | same | world destruction destroys worker VMs before the main VM, after the tick drained, with no VM destroyed while a GC step runs |
| `Test_Script_Determinism_WorkerFloatEnvironment` | same | Scripting.md's row |
| `Test_Script_World_CreateFaultSweep` | `Zenith_ScriptDesign_Tests.cpp` | `Zenith_RunFaultSweep` over every `uFAULT_POINT_SCRIPT_WORLD_*`, `…_RUNTIME_*` and `…_WATCHDOG_*` point: no leak, no half-registered handler ([Z2-N-CONTRACT-225]); at `…_RUNTIME_DEBUGGER` the runtime is created without a debugger when `m_bRequired` is false and `Create` fails with `SCRIPT_ERROR_DEBUGGER_UNAVAILABLE` when it is true; at `…_RUNTIME_DAP_SLOTS` `CreateWorld` returns `SCRIPT_ERROR_OUT_OF_MEMORY`, and neither the world nor a stop block remains linked |
| `Test_Script_Watchdog_TableFull` | same | a watchdog with `m_uMaxWorlds = 2`: a third world's `Create` fails with `SCRIPT_ERROR_WATCHDOG_TABLE_FULL` and leaks nothing; after one world is destroyed a new world takes the freed index; a world whose `m_uFrameThreadCount` exceeds the watchdog's fails with `SCRIPT_ERROR_INVALID_DESC` |
| `Test_Script_Watchdog_StopExemptsOnlyTheStoppedVM` | same | two worlds with tags 7 and 4,001, each with two VMs running over-limit invocations (`Test.SleepMs` inside a binding); VM 1 of world 4,001 has `m_bDebuggerStopped` set by the stop path: the poll never trips it, while the other VM of world 4,001 and both VMs of world 7 are tripped; after its release the VM's start time is advanced by exactly its stop duration (test clock) and its flag is clear; and, through the test scheduler hook, the watchdog thread is preempted between its first read of a VM's start time and its re-read of the stop flag while that VM stops for 3 × the limit, resumes and clears its flag: the poll decides on the re-read, rebased start and neither trips the VM nor reports a hang |
| `Test_Script_World_ConsoleEvaluateShipping` | same | in the `ShippingTest` configuration (`win-shipping-test` preset, [Z2-N-ARCH-231]) `ConsoleEvaluate` returns `SCRIPT_ERROR_INVALID_DESC`, and the main VM's `m_ulBytesInUse` and invocation counter are unchanged |

## Transaction and merge tests

| Test | File | Case |
|---|---|---|
| `Test_Script_Txn_ConcurrentDisjointColumnWrites` | `Zenith_ScriptTxn_Tests.cpp` | Scripting.md → "Transaction tests" |
| `Test_Script_Txn_FailureAfterDestroyKeepsEntity` | same | as specified |
| `Test_Script_Txn_FailureAfterEventNotDelivered` | same | as specified |
| `Test_Script_Txn_SnapshotCapExhaustion` | same | as specified |
| `Test_Script_Txn_ContainerShadowDroppedOnFailure` | same | as specified |
| `Test_Script_Txn_SpawnIdsReleasedOnFailure` | same | as specified |
| `Test_Script_Txn_EntityBoundaryMiddleFailure` | same | the worked example in one chunk, three chunks, three workers |
| `Test_Script_Txn_CrossEntityReadRule` | same | as specified |
| `Test_Script_Txn_WriteOutsideAuthority` | same | as specified |
| `Test_Script_Txn_DisabledFromNextTickBoundary` | same | as specified |
| `Test_Script_Txn_CommitNeverInvalidatesOtherViews` | same | as specified |
| `Test_Script_Txn_RetentionArenasPerNode` | `Zenith_ScriptDesign_Tests.cpp` | two chunk systems' sub-jobs interleaved on one slot (test scheduler hook): each claims its own retention entry; the first merge resets and releases only its entry; the second node's transactions stay intact and commit; both claim-table entries read 0xFFFF after the merges |
| `Test_Script_Txn_ArenaGrowthFailureFatal` | same | a fault-injecting heap refuses a retention block when transactions retained until the merge exceed the pre-reservation: `SCRIPT_ERROR_KIND_RESERVATION` (ZS1403), world halted, tick flagged non-replayable |
| `Test_Script_Txn_RetentionCeilingDerived` | same | for a chunk system and a behaviour system under four world descs (C = 4 KiB, E = 1,024, V = 256; C = 4 KiB, E = 16, V = 256; the design defaults; E = 2²⁴ with V = 2³² − 1, which saturates), each retention arena's `m_ulMaxBytes` equals round_up(max(2 × U + 256 KiB, W_s + 64), 256 KiB) of Transactions.md → "Retention ceiling derivation" (2⁶⁴ − 1 when saturated); with E = 16 world creation and `RegisterScriptNodes` succeed and every frame-thread slot's chunk-system ceiling is 1,310,720 bytes, equal to round_up(W + 64, 256 KiB); a fragmentation case retains, invocation after invocation, allocations of 128 KiB + 64 bytes (just over half a block, so each block holds one and abandons a tail almost as large) up to the used bound U of a test system: the arena's `GetReservedBytes()` stays within the ceiling and no `CAPACITY_EXCEEDED` occurs; the FaultArena scenario's per-(system, slot) retention high-water mark stays below the ceiling at every worker count |
| `Test_Script_Txn_RetentionCeilingIsCheck` | same (death-test suite `z2-death-check`: a `ZENITH_CHECK` fires in every configuration) | a test world whose retention ceiling is set to one block through test access, running an invocation that retains more: the `ZENITH_CHECK` of Transactions.md → "Transaction arena partition" rule 4 fires; no `RESERVATION` error, halt or diagnostic is produced before it |
| `Test_Script_Txn_ScratchCopiedToRetention` | same | an invocation of three entities (the second failing) with records, payload, one dirty and one clean shadow: the retained copy holds the first and third entities' records and payload and the dirty shadow only, the copied shadow's `m_pShadowData` points into the retention arena, and the scratch arena's used bytes return to the invocation's marker |
| `Test_Script_Txn_RetainedTransactionPoisonedAfterMerge` | `Zenith_ScriptDefect_Tests.cpp` (intentional-defect test, its own CTest entry, `win-asan`) | reading a retained record after the system's merge reset its entry is reported by ASan as a use of poisoned memory (`Zenith2/Docs/Contracts/Memory.md` → "Required intentional-defect tests") |
| `Test_Script_Txn_ContainerVisibleAfterMerge` | `Zenith_ScriptDesign_Tests.cpp` | a behaviour appends to a container field; a native system of the same phase whose edge hangs off the behaviour's merge node reads the new elements; a rejected behaviour's container is unchanged; an engine read before the merge (a different phase's node) sees the old storage. The swap goes through ECS's `Zenith_MergeContext::ReplaceContainer` (cross-doc row X192 applied; no fallback path) |
| `Test_Script_Txn_SubsequentInvocationSeesEarlierShadow` | same | one behaviour instance whose handler appends A to a container field and whose `OnUpdate` in the same node appends B: the committed container ends with A then B; with the handler's invocation rejected at the merge, both are rejected and the container is unchanged |
| `Test_Script_Txn_StringFieldCommittedAtMerge` | same | a string field written by a behaviour is interned and written at the merge, read by a same-phase dependent after the merge, and unchanged when the entity is rejected |
| `Test_Script_Txn_AsyncRequestNotFromSystem` | same | `ctx:RequestPath` from a chunk system raises ZS1702 |
| `Test_Script_Merge_DependentsRunAfterMerge` | `Zenith_ScriptMerge_Tests.cpp` | Scripting.md's row, 1 and 8 workers |
| `Test_Script_Merge_OrderIndependentOfCompletion` | same | reverse completion order gives the same queues |
| `Test_Script_Merge_CapacityRejectionDeterministic` | same | 10 slots, 25 emitters: the 10 lowest simulation IDs accepted, every worker count |
| `Test_Script_Merge_SnapshotLivesUntilMerge` | same | as specified |
| `Test_Script_Merge_ChangeVersionsAdvanceAtMerge` | same | as specified |
| `Test_Script_Merge_ChainInSystemOrder` | same | as specified |
| `Test_Script_Merge_ScriptAndEcsClassesAtomic` | `Zenith_ScriptDesign_Tests.cpp` | a candidate that fits every ECS class but not the audio-trigger class is rejected and neither holder's remaining capacity changes; the next smaller candidate is accepted |
| `Test_Script_Merge_BehaviourSubsequentInvocationsRejected` | same | an instance whose first handler is rejected has its subsequent handler and `Run` segment rejected and its row restored to the first snapshot ([Z2-N-SCRIPT-252]) |
| `Test_Script_Merge_BehaviourMergeOffMainThread` | same | the test scheduler runs a behaviour system's merge node on a worker slot: it accepts, rejects and flips checkpoint slots correctly, and the main VM's Debug owner-thread check never fires |
| `Test_Script_Event_EveryDeliveredEventHandled` | same | 300 events of a subscribed type delivered in one tick to one instance run 300 handler invocations in delivery order with ordinals 0 … 299, then `OnUpdate` (ordinal 300); no event is dropped |
| `Test_Script_Reduction_FixedOrderTree` | `Zenith_ScriptMerge_Tests.cpp` | as specified |
| `Test_Script_Spawn_IdsAssignedAtPlaybackInMergeOrder` | same | as specified |
| `Test_Script_Event_DeliveryInMergeOrder` | same | as specified |
| `Test_Script_Async_PublishedAtTickBoundaryInCompletionOrder` | `Zenith_ScriptAsync_Tests.cpp` | as specified |
| `Test_Script_Async_ReplayPublishesAtRecordedTick` | same | as specified, with `CanRunTick` false until the late result arrives |
| `Test_Script_Async_PlatformKindHandled` | same | a `PUBLICATION_KIND_PLATFORM_RECORDED` result published through its handler at step 1 with the tick boundary's `Zenith_TickBoundaryAccess`, through which the test handler writes a property; a replay publishes the recorded payload and ignores a live completion |
| `Test_Script_Async_ResultReleasedOnConsumption` | same | a result read by a committed invocation is released at the next tick boundary and its in-flight capacity returned; an unread one stays until its instance is destroyed |
| `Test_Script_Async_HandledResultKeptForScriptTicket` | same | a test source kind with a C++ handler and a result decoder of SCENE's shape (an 8-byte `{ uint32 instance, uint8 code, uint8 detail, uint16 0 }` payload, the source declared with 8 result bytes): a script-requested result runs the handler at step 1 and stays in the published table; `ticket:Result()` returns `{ instance = 7, ok = true }` with no `error` key on success and `{ instance = 0, ok = false, error = "<code name>" }` on failure; a C++-requested result of the same kind is released right after its handler; the script result is released at the tick boundary after the reading invocation commits; a decoder returning false halts the world with `SCRIPT_ERROR_KIND_INTERNAL` |
| `Test_Script_Async_UnconsumedCapacityHeldDiagnostic` | same | a source kind with in-flight capacity 2; one instance requests twice and never reads the results: `script.async.published_unconsumed.<kind>` reads 2, and a third request 600 ticks after publication is rejected at the merge with one `SCRIPT_ASYNC_CAPACITY_HELD` naming the holder's module and simulation ID |
| `Test_Script_Diagnostics_ConcurrentWriters` | `Zenith_ScriptDesign_Tests.cpp` (also `linux-tsan`) | eight threads write 10,000 diagnostics each into one store while no reader runs, then the main thread reads every returned sequence: each read is either its own diagnostic or false, the false count equals `script.diagnostics_dropped` plus the overwritten count, and TSan reports no race |
| `Test_Script_View_RetainedPastInvocationErrors` | `Zenith_ScriptViews_Tests.cpp` | as specified, in Debug, Release and Shipping configurations of the test target |
| `Test_Script_View_UsedAfterYieldErrors` | same | as specified |
| `Test_Script_View_StaleEpochErrors` | same | as specified |

## Determinism and fault-matrix tests

| Test | File | Case |
|---|---|---|
| `Test_Script_Random_RerunIdentical` | `Zenith_ScriptRandom_Tests.cpp` | as specified |
| `Test_Script_Random_ParallelIdentical` | same | as specified |
| `Test_Script_Random_KeyLayout` | same | 40 bytes; golden draws for fixed keys; the derivations equal CORE's `Zenith_Random*` over the same draws |
| `Test_Script_Determinism_ReplayMatrix` | `Tests/Conformance/Zenith_ScriptDeterminism_Conformance.cpp` | the 16 configurations per backend of Scripting.md → "Determinism test matrix", identical per-tick world-state hashes (scenario `Tests/Scenarios/ReplayMatrix/`) |
| `Test_Script_Determinism_AddressKeyedIterationLint` | `ZenithScriptCompiler/Tests/Zenith_ScriptLint_Tests.cpp` | as specified |
| `Test_Script_Fault_ScriptError` | `Tests/Conformance/Zenith_ScriptFault_Conformance.cpp` | FaultArena, script errors, 16 runs per backend |
| `Test_Script_Fault_BudgetExhausted` | same | budget exhaustion |
| `Test_Script_Fault_SnapshotCapExceeded` | same | snapshot cap |
| `Test_Script_Fault_MergeCapacityRejection` | same | merge rejection |
| `Test_Script_Fault_Combined` | same | all four in the same ticks |

The five fault tests carry the extra label `script-fault-matrix` ([Z2-N-SCRIPT-543]); each asserts [Z2-N-SCRIPT-541] (hashes) and [Z2-N-SCRIPT-542] (failed, rejected, diagnostic, disabled and recovery sets). The FaultArena scenario is `Tests/Scenarios/FaultArena/` with exactly Scripting.md's parameters ([Z2-N-SCRIPT-546]); its `FaultPlan` schema is `Tests/Scenarios/FaultArena/Schemas/FaultPlan.schema.json`.

## State recovery and reload tests

| Test | File | Case |
|---|---|---|
| `Test_Script_Recovery_QuarantineThenReloadLastGood` | `Zenith_ScriptRecovery_Tests.cpp` | as specified |
| `Test_Script_Recovery_OnRecoverRestartsCoroutine` | same | as specified |
| `Test_Script_Recovery_AutomaticPolicyLimit` | same | automatic policy with delay 30 and limit 2: two recoveries at the expected ticks, then disabled for the session with `recovery limit reached` |
| `Test_Script_Recovery_DefaultsPerConfiguration` | same | `Zenith_ScriptRecoveryResolve(false, …)` is manual outside Shipping and automatic 60/3 in the Shipping configuration of the test target |
| `Test_Script_Behaviour_YieldBoundariesCommit` | `Zenith_ScriptState_Tests.cpp` | as specified |
| `Test_Script_System_YieldRaisesError` | same | as specified |
| `Test_Script_State_CheckpointSerializationFailureRollsBack` | same | as specified |
| `Test_Script_State_RestoreDestroyedReferenceBecomesNil` | same | as specified |
| `Test_Script_State_UnserializableKindsLoadError` | same | as specified |
| `Test_Script_State_CheckpointDirtyOnlyAtMostOncePerTick` | same | as specified |
| `Test_Script_State_NoSharedMutableTables` | same | as specified |
| `Test_Script_State_UnknownFieldsPreserved` | same | as specified |
| `Test_Script_State_CookedSchemaMatchesLive` | `Zenith_ScriptDesign_Tests.cpp` | a manifest whose state record differs from the live declaration fails the module's load with ZS1806 `State differs from the cooked schema` |
| `Test_Script_State_RegisteredAtStepTen` | same | `Zenith_ScriptRegisterEngineTypes` on a DECLARING registry (startup step 6) registers `Zenith.Behaviour` and no behaviour-state type; after `BindStringTable` and `RegisterSchemaBlob`, `Zenith_ScriptRegisterBehaviourStateTypes` registers one `Behaviour.<Name>.State` per state-schema record, published by `Freeze` in revision 1; a call after `Freeze` outside a window returns `SCRIPT_ERROR_LOAD` naming the module |
| `Test_Script_State_LiveStateRestoredOnFailure` | same | a state write followed by an error leaves the proxy reading the pre-invocation value in the next tick |
| `Test_Script_State_SaveRoundTrip` | same | `GetStateSnapshot` in phase 11, then `RestoreSavedStates` into a fresh world: every instance's state equal, `OnRecover` ran once, the next completion ID restored |
| `Test_Script_HotReload_MigrationSucceeds` | `Zenith_ScriptHotReload_Tests.cpp` | as specified |
| `Test_Script_HotReload_RollbackOnError` | same | as specified (100 instances; heap references and live-state hashes unchanged); every element the failed reload reserved in the new generation's checkpoint pool was returned and the unused new pool destroyed (the test heap reports no leak), and every instance's slot pair is the one it had before the request |
| `Test_Script_HotReload_RecoveryAfterVersionBumpUsesNewCheckpoint` | same | a reload raises `StateVersion` 1 → 2 with a `Migrate[1]`; the `OnRecover` the go-live schedules fails for one instance; the automatic recovery at the following tick boundary restores that instance's migrated v2 state, no `Migrate` runs there (the main VM's owner-thread check of `…_NoLuauAtTickBoundary` holds), a phase-11 snapshot of the go-live tick holds a v2 blob for every instance of the module, go-live itself performs no heap allocation (the test heap's counter), and after the next prologue every old-generation element was returned and the old pool destroyed |
| `Test_Script_HotReload_SystemGeneration` | same | as specified |
| `Test_Script_HotReload_AppliedAtTickBoundaryOnly` | same | a reload requested mid-frame changes nothing until the next frame's first tick boundary, and never inside a tick; with the world paused, a reload committed in the prologue, then a save loaded with `RestoreSavedStates`, then the world unpaused: the restore ran go-live first, the loaded state runs under the new generation from the first tick, and a `RequestModuleReload` made while a generation is committed likewise acts on the new generation |
| `Test_Script_HotReload_WindowNotPublishedDropsStaging` | same | `ApplyPendingReloads` succeeds but `EndHotReload` fails: `CommitPendingReloads` in the same prologue step rolls the staged generation back, releases each staged reference once, opens no system reload scope, and the next tick runs the old generation |
| `Test_Script_HotReload_UnverifiedBytecodeRefused` | same | `RequestHotReload` of a simulation module without flag bit 3 returns `SCRIPT_ERROR_NOT_STATELESS_VERIFIED` |
| `Test_Script_HotReload_ScheduleCommitFailureRollsBack` | same | a reload whose new `Writes` breaks authority is rejected at `Commit` in prologue step 6; the old generation and schedule run next tick; every instance's `self` and state-proxy references resolve on the main VM to the objects they named before the request and none was released, and every staged reference was released exactly once (the test VM's `lua_unref` counter) |
| `Test_Script_HotReload_AccessChangeScheduledSameFrame` | same | a reload that adds a `Write` of a property read by a native system of the same phase, in a frame of three ticks: the system reload scope is committed in prologue step 6 on the main thread, the schedule recompiles in step 8 of the same frame, and from the first tick of that frame the native reader is ordered after the script system's merge (the frame graph's edge exists and the reader observes the committed value in all three ticks) |
| `Test_Script_Runtime_ReleaseListDrainedWithoutBehaviours` | same | a world with chunk-system modules only: five consecutive frames each hot-reload one module; after every prologue step 6 the world's `m_aiMainVMReleases` is empty and the main VM's registry holds no reference of an old generation |
| `Test_Script_HotReload_NoLuauAtTickBoundary` | same | with the tick-boundary node forced onto a worker slot, a disable, a go-live and a last-good restore touch no Luau object there (the main VM's owner-thread check never fires) and call no `MAIN_THREAD` member of ECS (the Debug owner-thread check of `Zenith_SystemRegistry` never fires); the released references are drained and fresh `self` and state proxies created by the module's next behaviour node before `OnRecover` runs |
| `Test_Script_Module_DisabledNodesRunNoScript` | same | a module disabled at a tick boundary: in every subsequent tick its node and merge bodies return at entry (no invocation, no commit, no fold, the reduction target unchanged), the schedule is not recompiled, and the tick after a successful go-live runs the module again |
| `Test_Script_Migration_ScriptHostOp` | same | `Zenith_ScriptMigrationHost::RunScriptOp` runs a schema `script` op; a raising op returns `MIGRATION_ERROR_SCRIPT` with the input unchanged |

## Module binding and sandbox tests

| Test | File | Case |
|---|---|---|
| `Test_Script_Module_RequireLiteralOnly` | `Zenith_ScriptModule_Tests.cpp` | as specified |
| `Test_Script_Module_RequireCycleError` | same | as specified |
| `Test_Script_Module_DeepFrozen` | same | as specified |
| `Test_Script_Module_ManifestValidation` | same | truncated, reordered, cyclic and hash-mismatched manifests each fail `Load` with ZS1805 `manifest` |
| `Test_Script_Module_BehaviourDirectory` | `Zenith_ScriptDesign_Tests.cpp` | spawning, disabling (`Enabled = false`) and destroying behaviour entities creates, parks and releases instances at the documented invocations; an unknown module path yields one diagnostic |
| `Test_Script_Module_LoadModulesPartialFailure` | same | a manifest of three behaviour modules, one `.zlbc` failing verification: `LoadModules` succeeds, `GetDisabledModuleCount()` is 1, `OnModuleDisabled` is called once and the diagnostic is stored; with all three failing, `LoadModules` returns `SCRIPT_ERROR_LOAD` and `RegisterScriptNodes` registers no system |
| `Test_Script_Sandbox_EnvironmentSubset` | `Zenith_ScriptSandbox_Tests.cpp` | as specified |
| `Test_Script_Sandbox_PcallCannotCatchAbort` | same | as specified |
| `Test_Script_Sandbox_TostringNoAddress` | same | as specified |
| `Test_Script_Sandbox_NoCapabilityBindings` | same | the sealed engine binding table contains no function whose descriptor names a filesystem, process, socket or dynamic-library resource, and the environments contain no `io`, `os` function, `package`, `load`, `loadstring`, `dofile`, `loadfile` ([Z2-N-SCRIPT-482]) |
| `Test_Script_Bindings_DluauGolden` | `Zenith_ScriptBindings_Tests.cpp` | as specified |
| `Test_Script_Types_NameMapping` | same | as specified |
| `Test_Script_Bindings_ContextEnforced` | same | as specified |
| `Test_Script_Bindings_SealRejectsCollision` | `ZenithReflect/Tests/Zenith_ScriptBinding_Tests.cpp` | two functions with one Luau name in one namespace: `Seal` returns `SCRIPT_BINDING_ERROR_DUPLICATE_NAME`; a buffered audio binding with no audio consumer: `SCRIPT_BINDING_ERROR_MISSING_CONSUMER`; the table stays unsealed in both |
| `Test_Script_Bindings_UnmappedParameterRejected` | same (compile-failure test through CTest `WILL_FAIL` on a separate translation unit) | binding a function whose parameter has no `Zenith_ScriptValueTraits` mapping fails to compile with the documented message |
| `Test_Script_Bindings_SideEffectMask` | `Zenith_ScriptBindings_Tests.cpp` | a buffered binding appending a kind outside its mask returns a thunk error, never an assert |
| `Test_Script_Bindings_MarshallingLossless` | same | every row of Scripting/Bindings.md → "Value marshalling": each failure raises its code; no value is truncated, wrapped or clamped |
| `Test_Script_Misuse_Catalogue` | `Zenith_ScriptMisuse_Tests.cpp` | table-driven over `Tests/Misuse/<Code>.luau` |
| `Test_Script_Misuse_NeverAsserts` | same | as specified |
| `Test_Script_Budget_SafepointCountDeterministic` | `Zenith_ScriptBudget_Tests.cpp` | as specified |
| `Test_Script_Budget_ExhaustionRollsBack` | same | as specified |
| `Test_Script_Budget_GcInterruptsNotCounted` | same | as specified |
| `Test_Script_Budget_NativeSafepointParity` | same | as specified; runs on Windows x64 in the Test step and on the S24 in the acceptance run |
| `Test_Script_Budget_DeclarationLowersOnly` | same | `Budget.SafepointsPerEntity` above the row is a load error; below it lowers the limit |
| `Test_Script_Budget_RecoveryAndLoadScopes` | same | an `OnRecover`, a `Migrate` step and a module scope each fail at their own row's limit |
| `Test_Script_Watchdog_FatalNotRollback` | same | as specified |
| `Test_Script_Watchdog_SuspendedWhileDebugging` | `Zenith_ScriptDap_Tests.cpp` | a VM stopped at a breakpoint for 3 × the watchdog limit and then continued finishes its invocation with no trip, no world halt and no hang report; the same holds for a stop inside a callback re-entry (`Test.CallBack`), whose enclosing invocation resumes with its pre-stop elapsed time |
| `Test_Script_Input_UnknownActionAtLoad` | `Zenith_ScriptBindings_Tests.cpp` | a module naming an undeclared input action fails to load with ZS1807 (added with the input bindings) |

## Compiler and cook tests

| Test | File (`ZenithScriptCompiler/Tests/`) | Case |
|---|---|---|
| `Test_Script_Sandbox_NoFastcallForReplacedBuiltins` | `Zenith_ScriptFastcall_Tests.cpp` | as specified |
| `Test_Script_Stateless_AdversarialCorpusRejectedAtLoad` | `Zenith_ScriptStateless_Tests.cpp` | every manifest entry's verdict, diagnostic and line exactly |
| `Test_Script_Stateless_AcceptedCorpusLoads` | same | every accepted module passes and runs in a test world |
| `Test_Script_Stateless_RepresentativeCorpusAccepted` | same | the representative corpus and every simulation module of the repository games pass ([Z2-N-SCRIPT-761]) |
| `Test_Script_Bytecode_DevAndCookIdentical` | `Zenith_ScriptCook_Tests.cpp` | as specified |
| `Test_Script_Bytecode_VersionMismatchRefused` | same | as specified, each of the nine checks of Compiler.md's table |
| `Test_Script_Compile_ParserLimits` | `Zenith_ScriptCompile_Tests.cpp` | 1 MiB + 1 byte, nesting 201 and 8,193 modules each fail with `SCRIPT_ERROR_COMPILE` |
| `Test_Script_Compile_StrictRequired` | same | a simulation module without `--!strict` fails the cook |
| `Test_Script_Compile_GuardConvertsExceptions` | same | parse and compile errors become `SCRIPT_ERROR_COMPILE`; the foreign-exception conversion `Zenith_ScriptCompilerForeignExceptionError` (the seam both non-Luau `catch` clauses of the guard call) returns `SCRIPT_ERROR_INTERNAL` with the text in the report |
| `Test_Script_Cook_ManifestDeterministic` | `Zenith_ScriptCook_Tests.cpp` | two cooks of one tree produce byte-identical `.zlbc` files and `scripts.zlmf` |
| `Test_Script_Cook_FailureWritesNothing` | same | a cook with one broken module writes no file |
| `Test_Script_Cook_DeclarationExtraction` | same | literal declarations produce the expected records; a computed `State` fails with the documented message; a reused field ID fails |
| `Test_Script_Cook_TypeErrorFailsCook` | same | a strict-mode type error in a simulation module fails the cook (runs the pinned `luau-analyze`) |

## Debugger and profiling tests

| Test | File | Case |
|---|---|---|
| `Test_Script_Dap_BreakpointFanOut` | `Zenith_ScriptDap_Tests.cpp` | as specified, with a scripted CTest client (a test-support DAP client over the loopback listener, sending the token line) |
| `Test_Script_Dap_SessionRequests` | same | `initialize`, `attach`, `threads`, `setBreakpoints`, `configurationDone`, `stackTrace`, `scopes`, `variables`, `continue`, `next`, `stepIn`, `stepOut`, `pause`, `evaluate` each answer as Debugger.md → "Request handling" states; `evaluate` of a call is refused |
| `Test_Script_Dap_StepAcrossYield` | same | stepping over `Zenith.WaitTicks` stops at the next line in the next segment |
| `Test_Script_Dap_TokenRequired` | same | a connection that sends no token is closed; no request is answered |
| `Test_Script_Runtime_DebuggerOptional` | same | with the DAP port already bound: `m_bRequired` false creates the runtime, `IsDebuggerAvailable()` is false and `SCRIPT_DEBUGGER_UNAVAILABLE` is in the runtime's store; `m_bRequired` true makes `Create` return `SCRIPT_ERROR_DEBUGGER_UNAVAILABLE`; the test heap reports no leak in either |
| `Test_Script_Dap_StopBracketsJobWorkerFlag` | same | a breakpoint stop on a worker VM: the test blocking-wait hook records exactly one `"debugger-stop"` wait with the thread's job-worker flag false, no other blocking wait from a job body, and the flag true again after `continue` inside the same body; the Debug lock-order checker records every acquisition of the DAP mutex by a frame thread with the job-worker flag false, and no acquisition of a lock below 710 while it is held (Design/SCRIPT/Debugger.md → "Stop protocol" rule 4 and the DAP mutex rule) |
| `Test_Script_Dap_ParkedInvocationsResume` | same | world A with chunk systems S1 (breakpoint) and S2 (disjoint access) and a behaviour module, world B beside it, at 1, 2 and 8 frame threads (at 1, every frame thread is blocked): while S1 is stopped, S2's and the behaviour's next invocations park, no merge of A's phase after S1's commits and A's tick does not end; after `continue` every invocation runs and A's per-tick world-state hashes equal an undebugged run's. Two further cases: in the first frame after world load, the breakpoint is hit by a worker VM created lazily in that same tick (its slot of A's stop block was never registered): `continue` releases it and the tick ends; and A and B both stopped at once: `continue` on A's thread releases A only and the session stays STOPPED, `continue` on B's thread is accepted and releases B, and both worlds' ticks end |
| `Test_Script_Dap_StepOutOfInvocationContinuesWorld` | same | `stepOut` from the outermost function of an invocation: the DAP thread sends `continued` for the world, the other stopped VMs are released and parked invocations run |
| `Test_Script_Dap_ConnectionLossReleases` | same | the client closes its connection while two VMs are stopped and three invocations are parked: every one resumes, both VMs' `m_bDebuggerStopped` are clear, the world's stopped count is 0 and the session returns to LISTENING |
| `Test_Script_Dap_StepCompletionWakesParked` | same | at 1 and 2 frame threads, a `next` that runs off the end of the stopped invocation while two invocations of the world are parked: the stepping thread leaves the count under the DAP mutex, both parked invocations run, the tick ends, the DAP thread sends `continued`, and the stopped count is 0; repeated 1,000 times with the test scheduler hook placing the parkers between their predicate check and their wait |
| `Test_Script_Dap_RestopDuringReleaseStaysSuspended` | same | VMs B and C of one world stopped; the test scheduler hook orders C's re-stop at a second breakpoint between B's decrement and B's watchdog rebase: the stopped count is 1 after B leaves and 0 after C's release (each stop counted once), C is never tripped although its stop exceeds the limit, and no world state clears C's exemption |
| `Test_Script_Dap_PauseClearedOnContinue` | same | `pause` while one VM runs an invocation and two are parked: the running VM stops; after `continue` the parked VMs start their invocations and none of the three stops again |
| `Test_Script_Dap_WorkerModuleScopeNeverStops` | same | a breakpoint on a module-scope line and a pending `pause` while a worker VM lazily loads the module: the worker never stops in module scope, its first invocation of the module stops at the pause, and the same breakpoint stops the main VM's load at world load; after that out-of-tick stop is continued, `Zenith_IsJobWorkerThread()` is false on the main thread and the prologue's next blocking wait passes CORE's check |
| `Test_Script_Dap_ContinueAfterStepRunsFree` | same | `next` reaches its target line, then `continue`: the invocation runs to its end with no further stop, the world's stopped count is 0 afterwards (never wrapped), and the world's next invocation passes the gate's fast path; the same after `stepIn` into a callee followed by `continue`, after a breakpoint hit before the step target followed by `continue`, and after a `Run` step across a yield whose next segment stops at its first line and is then continued |
| `Test_Script_Dap_AbsentFromShipping` | `Libgraph` step (link-surface check), not a doctest | no `Zenith_ScriptDap` symbol in any game's Shipping closure |
| `Test_Script_Profiling_OverlayRows` | `Zenith_ScriptProfiling_Tests.cpp` | as specified |
| `Test_Script_Profiling_SamplerNoEffect` | same | sampling on and off gives identical safepoint counts and world-state hashes; the dump has the documented schema |

## Fuzz targets

| Target | Source | Input |
|---|---|---|
| `Zenith_Fuzz_Script_CheckpointDecode` (Scripting.md's `Fuzz_Script_CheckpointDecode`) | `ZenithScript/Tests/Fuzz/Script_CheckpointDecode.cpp` | arbitrary bytes against three fixed schemas; no out-of-bounds read, no allocation beyond the slot, every malformed input a restore failure ([Z2-N-SCRIPT-485]) |
| `Zenith_Fuzz_Script_Manifest` | `ZenithScript/Tests/Fuzz/Script_Manifest.cpp` | `Zenith_ScriptModuleManifest::Load` |
| `Zenith_Fuzz_Script_BytecodeHeader` | `ZenithScript/Tests/Fuzz/Script_BytecodeHeader.cpp` | the `.zlbc` header decoder and verification |
| `Zenith_Fuzz_Script_DapMessage` | `ZenithScript/Tests/Fuzz/Script_DapMessage.cpp` | DAP framing and request parsing |
| `Zenith_Fuzz_Script_Compile` | `ZenithScriptCompiler/Tests/Fuzz/Script_Compile.cpp` | source text through `Zenith_ScriptCompileModule` and `Zenith_ScriptCheckStateless` (parser limits hold, no crash) |

## Benchmarks and harnesses

| Target | Directory | Measures | Consumer |
|---|---|---|---|
| `Zenith_Bench_ScriptStateless` | `Zenith2/Benchmarks/ScriptStateless/` | every criterion of Scripting/StatelessCorpus.md → "Alternative evaluation procedure" per prototype | the D-LUAU-STATELESS ruling (Stage 1) |
| `Zenith_Bench_ScriptNativeParity` | `Zenith2/Benchmarks/ScriptNativeParity/` | interpreted versus native safepoint counts and results over the budget and representative corpora, on Windows x64 and the S24 | the D-SCRIPT-TXN refinement ruling (Stage 1) |
| `Zenith_Bench_BudgetsScript` | `Zenith2/Benchmarks/Budgets/SCRIPT/` | the MP-SCRIPT, MP-SUBMEM and MP-COUNTER procedures of every Z2-B-SCRIPT row on W-Dense, on the reference desktop and the S24 | the Stage 1 per-system budget ticket for SCRIPT |
| `Zenith_Bench_ScriptVM` | `Zenith2/Benchmarks/ScriptVM/` | worker-VM creation, invocation overhead per entity, dispatcher cost per binding call | regressions; W-Dense |
| `Zenith_Bench_ScriptHotReload` | `Zenith2/Benchmarks/ScriptHotReload/` | save-to-running latency of an editor hot reload | Z2-B-EDIT-002 |
| `ZenithScript_DapPrototype` (Stage 1 sprint) | `Zenith2/Engine/ZenithScript/Tests/` (`Test_Script_Dap_TokenRequired` and the skeleton form of `Test_Script_Dap_SessionRequests`) | the DAP skeleton with the scripted client | the D-LUAUDBG ruling |

The Stage 1 harnesses exchange measurements with their report-mode tests as line-oriented `.tsv` files (a schema line, then tab-separated rows), so no JSON reader is needed before Stage 2. Each benchmark registers its CTest `--smoke` run in the Test step; full runs write under `Zenith2/Docs/Research/<Topic>/` with raw data, repetitions, medians and 95% confidence intervals (Z2-D-064).

## Integration suites

SCRIPT's integration references are registered CTests the Test step runs; none is a hand-run or a recorded evidence file. A required-test group asserts a label only where a rule applies it ([Z2-N-ARCH-251], cross-doc row X215 applied): a library's `TEST_LABELS` and `CONFORMANCE_LABELS`, an integration suite's `TEST_LABELS`, and a doctest suite's name, which doctest's `ADD_LABELS` merges into the same `LABELS` property; a gametest line carries its registered `z2-game`; a registry death or defect entry carries its suite label alone (`z2-death-check` in every configuration, `z2-death-assert` under `@config:Debug,Sanitize`), never `script-conformance`; and a Shipping-only case is a case of the `ShippingTest` configuration, which only the `win-shipping-test` preset builds ([Z2-N-ARCH-231], cross-doc row X216 applied), listed under `@config:ShippingTest`.

The required-test check `Zenith2/Engine/ZenithScript/Tests/Required/Zenith_ScriptRequiredTests.cmake` (a `cmake -P` script) reads the build tree's CTest listing (`ctest --show-only=json-v1`) with `string(JSON)` and checks each entry of one list file `Tests/Required/<Group>.txt`. An entry is one line `<entry> <label>[,<label>…]`, optionally preceded by `@preset:<p>[,<p>…]` or `@config:<c>[,<c>…]`; it is satisfied by a registered CTest whose name equals it or ends in `.<entry>`, carrying every listed label. The check prints `script-required:` lines and one `ZENITH_VERIFY_CODE=<n>`: 0 pass, 1 listing unreadable, 2 an entry missing, 3 a label missing, 4 a malformed list line. The registry helper `zenith_script_required_group(<Group>)` registers `ScriptRequired_<Group>` (labels `z2-cmake;script-conformance`); `ScriptRequired_Selftest_Ok`, `_Missing` and `_Label` run the check over fixture listings.

| Entry | Kind | Covers |
|---|---|---|

The rows are added by the ticket that creates each entry: the `ScriptRequired_<Group>` checks (Stage3Firewall, Stage3Modules, Stage3FaultMatrix, Stage3Determinism, Stage3Budgets, Stage3State, Stage3Dap, Stage3Composition, Stage4Gameplay) and the integration suites under `Zenith2/Tests/` (`Zenith_Integration_ScriptAbi`, `_ScriptBindings`, `_ScriptAsync`, `_ScriptSaves`).

## Gate steps and labels

| Group | Gate steps |
|---|---|
| every `ZenithScript_Tests`, `ZenithScriptCompiler_Tests` and conformance case, the integration suites and the `ScriptRequired_*` checks | `Test` (label `script-conformance`: `TEST_LABELS` of the two library calls and `CONFORMANCE_LABELS` of ZenithScript's ([Z2-N-ARCH-251]), explicit `LABELS` on the registry's `cmake -P` checks, which also carry `z2-cmake`; SCRIPT's cases in `ZenithReflect_Tests` and `ZenithCook_Tests` sit in the doctest suite `script-conformance`, and the fault tests in the suite `script-fault-matrix`, which are CTest labels through `ADD_LABELS`, like an integration suite's `TEST_LABELS` ([Z2-N-ARCH-251], cross-doc row X215 applied)); the `ShippingTest` cases run in the `win-shipping-test` preset once the `Test` step runs it |
| the VM glue tests (`Zenith_ScriptVM_Tests.cpp`, `Zenith_ScriptFirewall_Tests.cpp`, `Zenith_ScriptTxn_Tests.cpp`, `Zenith_ScriptMerge_Tests.cpp`, `Zenith_ScriptDesign_Tests.cpp`) | additionally `Test -Label memory` and `Sanitizers -Preset win-asan`, `Sanitizers -Preset linux-tsan` (the `Zenith2Memory` lines; the label assignment rule is pending ARCH, cross-area note J12) |
| intentional-defect tests (`Zenith_ScriptDefect_Tests.cpp`) | `Sanitizers -Preset win-asan`, each its own CTest entry |
| fuzz smoke runs | `Sanitizers -Preset win-asan` |
| firewall and Luau rules | `ForbiddenCheck` (rows of Scripting.md → "ForbiddenCheck rules"), `Libgraph` (Shipping closure, link surface, leakage), `ApiContractCheck`, `HeaderSelfContained` |
| death tests | `Test` (`z2-death-assert` suite) |
| S24 runs (RAII tests under HWASan, native parity, DAP attach) | the stage acceptance runs (`needs-human` device tickets) |
