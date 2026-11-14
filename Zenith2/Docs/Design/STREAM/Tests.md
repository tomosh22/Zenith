# STREAM design — tests

This file is part of the STREAM design family; the entry document is `Zenith2/Docs/Design/STREAM.md`. It lists every STREAM test by its own unique name with the case it asserts (each case is registered as its own CTest, [Z2-N-ARCH-179], so no name here is a template and none repeats another area's), the fixtures and how they are generated, and the gate step each group runs under. Every test is pass/fail with no pinned count (`Zenith2/Docs/Contracts.md` → "Tests and code hygiene gates"), and every piece of evidence is a registered test the gate runs: no test asks a person to run something and record a number. STREAM's classes are concrete and are tested as themselves: no test substitutes a double for `Zenith_StreamingWorldAsset`, `Zenith_StreamingSnapshotPool`, SCENE's world state, ASSET's registry, the memory governor or the ECS world; the seams are the real objects driven through their public and internal APIs, the start context's fault injector, ASSET's header-only test support, and the fixtures below. GPU work runs on real devices (WARP, lavapipe), never on a stand-in renderer.

## Test files

| File | Target | Group |
|---|---|---|
| `Zenith2/Engine/ZenithStreaming/Tests/Zenith_StreamingTypes_Tests.cpp` | `ZenithStreaming_Tests` | unit |
| `Tests/Zenith_StreamingWorldFormat_Tests.cpp` | `ZenithStreaming_Tests` | format |
| `Tests/Zenith_StreamingBands_Tests.cpp` | `ZenithStreaming_Tests` | unit |
| `Tests/Zenith_StreamingCellMachine_Tests.cpp` | `ZenithStreaming_Tests` | unit |
| `Tests/Zenith_StreamingAdmission_Tests.cpp` | `ZenithStreaming_Tests` | unit |
| `Tests/Zenith_StreamingPersistence_Tests.cpp` | `ZenithStreaming_Tests` | unit |
| `Tests/Zenith_StreamingReferences_Tests.cpp` | `ZenithStreaming_Tests` | unit |
| `Tests/Zenith_StreamingRegions_Tests.cpp` | `ZenithStreaming_Tests` | unit |
| `Tests/Zenith_StreamingRebase_Tests.cpp` | `ZenithStreaming_Tests` | unit |
| `Tests/Zenith_StreamingHLOD_Tests.cpp` | `ZenithStreaming_Tests` | unit |
| `Tests/Zenith_StreamingService_Tests.cpp` | `ZenithStreaming_Tests` | unit |
| `Tests/Zenith_StreamingSnapshotPool_Tests.cpp` | `ZenithStreaming_Tests` | unit, concurrency |
| `Tests/Zenith_StreamingModule_Tests.cpp` | `ZenithStreaming_Tests` | unit, fault injection, diagnostics |
| `Tests/Golden/stream-dump-unbound.json`, `Tests/Golden/stream-dump-bound.json` | committed JSON text, hand-written | golden files |
| `Tests/Support/Zenith_StreamingTestWorld.h` | header-only helper | fixtures |
| `Tests/Fuzz/StreamingWorld.cpp` | `Zenith_Fuzz_StreamingWorld` | fuzz |
| `Zenith2/Tools/ZenithCook/Tests/Streaming/Zenith_StreamingCook_Tests.cpp` | `ZenithCook_Tests` | cook |
| `Zenith2/Tools/ZenithCook/Tests/Streaming/Documents/*.streamworld.json` | committed JSON text | cook fixtures |
| `Zenith2/Tests/Streaming/Zenith_StreamingIntegration_Tests.cpp` | `Zenith_Integration_Streaming` | integration |
| `Zenith2/Tests/Streaming/Zenith_StreamingStress_Tests.cpp` | `Zenith_Integration_Streaming` | stress, determinism |
| `Zenith2/Tests/StreamingRender/Zenith_StreamingRender_Tests.cpp` | `Zenith_Integration_StreamingRender` | render (WARP, lavapipe) |
| `Zenith2/Tests/Memory/Internal/Zenith_MemorySheddingCases.h` (one appended row) | the memory suite | shedding |

## Unit tests

### Types and records

| Test | Asserts |
|---|---|
| `Test_StreamingTypes_LimitsMatchFormats` | every limit constant equals the "Limits" table of `Zenith2/Docs/Design/STREAM/Formats.md`; `uSTREAMING_SNAPSHOT_SLOTS` equals `uSTREAMING_MAX_CONCURRENT_LOADS` + `uSTREAMING_MAX_QUEUED_CELLS` |
| `Test_StreamingTypes_RecordLayouts` | `sizeof` and field offsets of the cell runtime (56), `Zenith_StreamingCellStatusComponent` (288), `Zenith_StreamingCellOverrideSlot` (12), source runtime (96), admission entry (24), snapshot slot (64, `alignof` 64), entity table (8,320), cell mirror (32), HLOD runtime (32) records and `Zenith_StreamingError` (16) |
| `Test_StreamingTypes_ToStringCoversEveryEnumerator` | every enumerator of every STREAM enum has a distinct non-"unknown" name, and an out-of-range value gives "unknown" |
| `Test_StreamingTypes_HookTagStable` | `Zenith_MakeStreamingHookTag(cell, tick)` packs cell + 1 in bits 0–31 and the tick's low word in 32–63; two cells or two ticks never give one tag; cell 0 gives a non-zero tag |
| `Test_StreamingTypes_GroupIDMatchesHashOfGuid` | `Zenith_MakeStreamingGroupID` equals `Zenith_Hash64` of the GUID's 16-byte binary form for three fixed GUIDs |
| `Test_StreamingComponents_RegisteredShape` | the seven components, the struct `Zenith.Streaming.CellOverrideSlot` and the resource register with the stable names, property IDs, types (`CellScene` properties are `PERSISTENT_ID`; `WorldRef` property 3 a fixed array of 64 slots; `CellStatus` properties 9 and 10 fixed arrays of 16 `U64`), defaults (`DrivesHLOD` false, `CellStatus` State UNLOADED), ranges, authorities and serialized and editor-hidden flags of `Zenith2/Docs/Design/STREAM/PublicAPI.md` → "Zenith_StreamingComponents header" (every STREAM-written property serialized); `Freeze` succeeds |

### Bands and the desired set

| Test | Asserts |
|---|---|
| `Test_StreamingBands_DistanceToCellXZ` | the distance of points inside, on the edge of, beside and diagonal to a cell equals the closed form, for positive and negative coordinates |
| `Test_StreamingBands_DistanceToCellXYZ` | the vertical term applies only to XYZ grids |
| `Test_StreamingBands_BandThresholdsInclusive` | a distance exactly at each radius × scale falls in the more urgent band |
| `Test_StreamingBands_RadiusScaleClamped` | scales 0.1 and 10 behave as 0.25 and 4 |
| `Test_StreamingBands_ForwardBiasOrdersAhead` | with β 0.25, a cell ahead at 200 m sorts before a cell behind at 180 m; with β 0 the order inverts; no band changes when the source rotates |
| `Test_StreamingBands_EnumerationClipAt4096` | a 32 m grid with a 4,000 m prefetch radius examines at most 4,096 coordinates, never clips below the KEEP radius, and logs one clip line |
| `Test_StreamingBands_MostUrgentSourceWins` | two sources: the band is the more urgent; a band tie is won by the higher `Priority` (key field `3 − priority`, priority 3 first, priority 0 last), then by the lower simulation ID |
| `Test_StreamingBands_SourcesCappedAtSixteen` | 20 enabled sources: the 16 lowest simulation IDs drive the bands, the other 4 are ignored, and one line is logged per world |
| `Test_StreamingBands_OverridesApplyOnlyWhenAllowed` | FORCE_ACTIVE and FORCE_UNLOADED change bands when `stream.debug.allowOverrides` is true and are refused with `STREAMING_ERROR_OVERRIDE_NOT_ALLOWED` (counted in `stream.overrides.refused`) when false; an override of a coordinate with no cell counts `stream.overrides.unknown_cell` |
| `Test_StreamingBands_ForceUnloadedTakesDependents` | FORCE_UNLOADED on a collision cell forces every gameplay cell depending on it to NONE; the dependents unload first and the collision cell after them; a FORCE_ACTIVE dependent of it loses |
| `Test_StreamingSources_PreviousPositionPersisted` | step 5 writes a source's absolute position and tick into properties 10 to 13 every tick; a 300 m move in one tick is a teleport only when property 13 is the previous tick; a teleport playback sets property 14 and the next update counts one teleport and clears it; a source absent for a tick gets no distance test on its return |
| `Test_StreamingDesired_KeepRetainsNeverStarts` | a cell in KEEP stays when LOADING or ACTIVE and is never issued from UNLOADED |
| `Test_StreamingDesired_ClosurePinsDependencies` | a wanted gameplay cell pins the collision cells under it at its band; a KEEP cell pins at KEEP; the closure is one pass in topological order for a three-grid chain |
| `Test_StreamingDesired_DependencyOutlivesDependents` | an unwanted collision cell under an ACTIVE gameplay cell stays ACTIVE until the gameplay cell is UNLOADED |
| `Test_StreamingDesired_UnloadDelayCountsDown` | a 30-tick delay unloads at tick 30 of being unwanted and resets when the cell is wanted again |

### Cell state machine

One test per transition row of `Zenith2/Docs/Design/STREAM/Runtime.md` → "Transitions". Each builds a world with `Zenith_StreamingTestWorld`, drives the cell to the row's From state, produces the row's event through the real SCENE world state (its additive set, its commit, its publication with the hook, or an external `Zenith_RecordUnloadAdditiveScene`), and asserts the To state, the slot bits, the counters and the recorded actions:

| Test | Row | From → To |
|---|---|---|
| `Test_StreamingCellMachine_RowT1` | T1: issued in step 9 | UNLOADED → REQUESTED |
| `Test_StreamingCellMachine_RowT2` | T2: OnLoadCommitted with an instance | REQUESTED → LOADING |
| `Test_StreamingCellMachine_RowT3` | T3: OnLoadCommitted refused | REQUESTED → FAILED |
| `Test_StreamingCellMachine_RowT4` | T4: still REQUESTED in step 4 and absent from the commit-refused inbox drained in step 3 (the load buffer was rejected at merge) | REQUESTED → UNLOADED |
| `Test_StreamingCellMachine_RowT5` | T5: obs QUEUED | LOADING → QUEUED |
| `Test_StreamingCellMachine_RowT6` | T6: obs FAILED | LOADING → FAILED |
| `Test_StreamingCellMachine_RowT7` | T7: unwanted, issued in step 8 | LOADING → CANCELLING |
| `Test_StreamingCellMachine_RowT8` | T8: admitted in step 10 | QUEUED → ACTIVATING |
| `Test_StreamingCellMachine_RowT9` | T9: unwanted, issued in step 8 | QUEUED → UNLOADING |
| `Test_StreamingCellMachine_RowT10` | T10: obs FAILED | QUEUED → FAILED |
| `Test_StreamingCellMachine_RowT11` | T11: obs ACTIVE and the cell's ApplyCellState played | ACTIVATING → ACTIVE |
| `Test_StreamingCellMachine_RowT12` | T12: obs ACTIVE, ApplyCellState not played | ACTIVATING → ACTIVE |
| `Test_StreamingCellMachine_RowT13` | T13: obs QUEUED | ACTIVATING → QUEUED |
| `Test_StreamingCellMachine_RowT14` | T14: obs FAILED | ACTIVATING → FAILED |
| `Test_StreamingCellMachine_RowT15` | T15: unwanted with delay 0, or preempted, issued in step 8 | ACTIVE → UNLOADING |
| `Test_StreamingCellMachine_RowT16` | T16: obs gone | UNLOADING → UNLOADED |
| `Test_StreamingCellMachine_RowT17` | T17: obs ACTIVE or QUEUED | UNLOADING → ACTIVE or QUEUED |
| `Test_StreamingCellMachine_RowT18` | T18: the discard inbox names the cell | CANCELLING → UNLOADED |
| `Test_StreamingCellMachine_RowT19` | T19: obs LOADING | CANCELLING → LOADING |
| `Test_StreamingCellMachine_RowT20` | T20: obs UNLOADED and no discard yet | CANCELLING → CANCELLING |
| `Test_StreamingCellMachine_RowT21` | T21: retry tick reached | FAILED → UNLOADED |
| `Test_StreamingCellMachine_RowT22` | T22: rebinding to the same world | FAILED_PERMANENT → UNLOADED |
| `Test_StreamingCellMachine_RowT23` | T23: world unbinding | any but UNLOADED → as the unload rows |
| `Test_StreamingCellMachine_RowT24` | T24: obs UNLOADED: other code unloaded the instance | LOADING → CANCELLING |
| `Test_StreamingCellMachine_RowT25` | T25: obs gone | QUEUED → UNLOADED |
| `Test_StreamingCellMachine_RowT26` | T26: obs gone | ACTIVATING → UNLOADED |
| `Test_StreamingCellMachine_RowT27` | T27: obs gone | ACTIVE → UNLOADED |
| `Test_StreamingCellMachine_RowT28` | T28: obs ACTIVE and the anchor's handle is no longer valid | ACTIVE → UNLOADING |
| `Test_StreamingCellMachine_RowT29` | T29: obs FAILED | UNLOADING → FAILED |
| `Test_StreamingCellMachine_RowT30` | T30: obs QUEUED | CANCELLING → QUEUED |
| `Test_StreamingCellMachine_RowT31` | T31: obs FAILED | CANCELLING → FAILED |
| `Test_StreamingCellMachine_RowT32` | T32: obs FAILED | FAILED with release pending → FAILED |
| `Test_StreamingCellMachine_RowT33` | T33: obs gone | FAILED with release pending → FAILED |
| `Test_StreamingCellMachine_RowT34` | T34: an observation the matrix of "Totality" names T34 | REQUESTED, LOADING, QUEUED, ACTIVATING, ACTIVE, UNLOADING, CANCELLING, FAILED → UNLOADING when the instance is live, otherwise UNLOADED |

| Test | Asserts |
|---|---|
| `Test_StreamingCellMachine_TableIsTotal` | for every cell of the matrix of "Totality" (seven states × six observed instance states) and the three REQUESTED resolutions, exactly one row fires, and it is the row the matrix names |
| `Test_StreamingCellMachine_CommitRefusedDoesNotConsumeRetry` | a `SCENE_ERROR_LIMIT` commit refusal schedules a retry with retries unchanged |
| `Test_StreamingCellMachine_CapacityFailureDoesNotConsumeRetry` | published failures with `SCENE_ERROR_LIMIT` (start refused), `OUT_OF_MEMORY`, `MEMORY_REFUSED` and `ACTIVATION_CAPACITY` leave the retries unchanged, back off 30, 60, 120 … ticks up to `stream.cell.retryTicks`, count `stream.cells.capacity_failures`, and never reach FAILED_PERMANENT after 20 consecutive failures; the backoff resets when the cell activates; with `stream.cell.capacityRetryTicks` 3,600 and 255 consecutive failures the retry tick is now + `stream.cell.retryTicks` (no shift above 31 bits, no overflow) |
| `Test_StreamingCellMachine_AssetCapacityFailureDoesNotConsumeRetry` | ASSET's fixture fails the cell scene's load with `ASSET_ERROR_OUT_OF_MEMORY` 20 consecutive times (SCENE publishes `SCENE_ERROR_ASSET_UNAVAILABLE` with that detail, row ST2 (i)): the cell is never FAILED_PERMANENT, its retries stay 0 and its backoff follows the capacity row; the same for `QUEUE_FULL`, `REGISTRY_FULL`, `SHUTTING_DOWN` and `TIMEOUT` |
| `Test_StreamingCellMachine_AssetTransientFailureIsBounded` | `ASSET_ERROR_IO` on every attempt: failures 1 to 4 consume no retry (backoffs 30, 60, 120, 240), failures 5 to 7 consume retries 1 to 3 (600 each), failure 8 makes the cell FAILED_PERMANENT; `ASSET_ERROR_NOT_INSTALLED`, `ASSET_ERROR_DEPENDENCY_FAILED` and a zero detail behave the same (the last also counting `stream.cells.unknown_failure_detail`); one success between failures resets the transient count |
| `Test_StreamingCellMachine_AssetDetailClassifiesEveryCode` | every `Zenith_AssetErrorCode` enumerator, 0 and one value outside the enumerators, carried as the detail of a published `ASSET_UNAVAILABLE`, falls in the class of the ASSET detail table of Runtime.md "Failure classification", and every `Zenith_SceneErrorCode` enumerator in the class of the SCENE table |
| `Test_StreamingCellMachine_RetriesExhaustToPermanent` | four consecutive content failures (`stream.cell.maxRetries` 3: the first load and its three retries; `ASSET_UNAVAILABLE` with `ASSET_ERROR_CORRUPT`, `COOKED_CORRUPT`, `DUPLICATE_INSTANCE`) make the cell FAILED_PERMANENT; a rebinding resets it |
| `Test_StreamingCellMachine_ActivationTooLargeIsPermanent` | a published `SCENE_ERROR_ACTIVATION_TOO_LARGE` makes the cell FAILED_PERMANENT at once |
| `Test_StreamingCellMachine_CatalogueMissIsPermanent` | a cell whose scene GUID is absent from the catalogue is FAILED_PERMANENT at issue, with the ASSET code as detail |
| `Test_StreamingCellMachine_ActivatingNotInterruptible` | a cell unwanted while ACTIVATING reaches ACTIVE, then UNLOADING next tick |
| `Test_StreamingCellMachine_NoLoadInReleaseTick` | a cell whose release is recorded in a tick is not a load candidate in that tick and loads at the next without `DUPLICATE_INSTANCE` |
| `Test_StreamingBinding_RefusalsAndRebinding` | a runtime-created world reference is refused with `STREAMING_ERROR_WORLDREF_RUNTIME`, a second enabled one with `STREAMING_ERROR_WORLD_DUPLICATE`, each logged once; disabling the reference goes BOUND → UNBINDING → NONE with every cell unloaded through paced unloads; re-enabling it rebinds and resets FAILED_PERMANENT cells; `binding.state` in the dump reads "unbinding" while it unbinds |
| `Test_StreamingBinding_BoundAboveCapWarns` | a world whose worst-case live instances exceed `stream.maxLiveInstances` binds, logs one `stream.world` line naming both numbers and sets `stream.world.bound_exceeds_cap` to 1 |
| `Test_StreamingBinding_PrecisionWarningWithoutRebase` | with rebasing off, a world with a cell 5 km from the origin sets `stream.rebase.precision_warning` and logs once; a world within 4,096 m does not |

### Admission and pacing

| Test | Asserts |
|---|---|
| `Test_StreamingAdmission_KeyIsTotalOrder` | equal bands, priorities, roles and distances order by grid then cell index |
| `Test_StreamingAdmission_RoleOrderCollisionFirst` | at equal band and distance, collision loads before navigation before gameplay before audio before large |
| `Test_StreamingAdmission_ConcurrentCap` | with 40 wanted cells and the cap 16, at most 16 cells are REQUESTED + LOADING + CANCELLING at every tick, and `loads_deferred` counts the rest |
| `Test_StreamingAdmission_PendingCapBoundsQueued` | with admission held (dependencies never ACTIVE), LOADING + CANCELLING + QUEUED never exceeds `stream.maxConcurrentCellLoads` + `stream.maxQueuedCells`, and no load is issued while it is reached; second case: with the source oscillating so that 8 QUEUED cells become unwanted every tick and the container capacity forced to 0 for every unload buffer over 50 ticks (each unload rejected, T17), SCENE's LOADING + QUEUED instance count and STREAM's LOADING + CANCELLING + QUEUED count never exceed C + Q at any phase 11, and the UNLOADING cells flagged "unload of a QUEUED instance" are counted in pending in every step 9 |
| `Test_StreamingAdmission_NewRequestsPerTickCap` | at most 4 load actions per tick |
| `Test_StreamingAdmission_LiveCapAndPreemption` | at the live cap, a REQUIRED candidate cuts the delay of the least urgent unwanted ACTIVE cell; with none, it preempts the least urgent KEEP-only ACTIVE cell (`stream.cells.preempted`), which unloads next tick, and the REQUIRED cell loads |
| `Test_StreamingAdmission_ActivationCellsCap` | at most 4 admissions per tick |
| `Test_StreamingAdmission_ActivationEntitiesCap` | admissions stop at 4,096 entities, except the first cell of a tick, admitted at 10,000 entities |
| `Test_StreamingAdmission_WaitsForDependenciesActive` | a QUEUED gameplay cell is not admitted while its collision cell is ACTIVATING, and is admitted the tick after |
| `Test_StreamingAdmission_UnloadsFarthestFirst` | with 12 unload candidates and the cap 8, the 8 least urgent unload first and `stream.cells.unload_deferred` counts 4 |
| `Test_StreamingAdmission_BlockedByPermanentDependency` | a gameplay cell whose collision dependency is FAILED_PERMANENT is never issued and counts `stream.cells.blocked_by_dependency` |
| `Test_StreamingAdmission_RefusedAdmissionNotCountedBySCENE` | a QUEUED cell refused by the hook for 20 ticks is not failed by SCENE's `scene.activationRetryTicks` |

### Action buffers

| Test | Asserts |
|---|---|
| `Test_StreamingActions_OneBufferPerAction` | each action has its own merge key `(0, ordinal)`; ordinals follow rebuild actions, unloads, loads, holder repairs and status-holder actions, admissions, region destroys, rebase |
| `Test_StreamingActions_RejectedLoadReverts` | a load buffer rejected at merge (container capacity forced to 0) leaves the cell UNLOADED next tick with both slot bits clear (T4) |
| `Test_StreamingActions_RejectedUnloadKeepsCapture` | a rejected unload leaves the cell ACTIVE and its next unload captures the then-current values (T17) |
| `Test_StreamingActions_RejectedActivationLateApply` | SCENE's activation accepted but STREAM's ApplyCellState buffer rejected: the next update re-records it and the restored values appear one tick late (T12, `late_state_apply`), overwriting a value a phase-5 system wrote to a restored property in the activation tick; a persistent entity destroyed in phase 5 of the activation tick keeps its committed tombstone through the late `ApplyCellState` (admission, not the playback, cleared the bits) and is DESTROYED in the delta after the next unload |
| `Test_StreamingActions_ApplyMeetsQueuedInstance` | STREAM's admission buffer accepted but SCENE's activation rejected: ApplyCellState and BindReferences count `stream.cells.state_apply_skipped` (never `stream.commands.malformed`), leave "state applied" clear, and the cell activates at its re-admission with its restored values |

### Persistence

| Test | Asserts |
|---|---|
| `Test_StreamingPersistence_DeltaRoundTrip` | encode then decode of 0, 1 and 1,024 records, with present and absent type lists, gives the same records; the encoding is byte-identical when records are given in any order |
| `Test_StreamingPersistence_DeltaGoldenBytes` | the encoding of a fixed delta (one DESTROYED record, one STATE record with one present `U32` type and one absent type) equals the byte array written by hand in the test source from Formats.md "Cell delta encoding", except the four check bytes (D-SEL-HASH, open, Stage 1) |
| `Test_StreamingPersistence_DecoderChecks` | each of D1 to D9 is triggered by one mutated input and returns its detail; D9 drops the record and counts `stream.persistence.records_dropped` |
| `Test_StreamingPersistence_CaptureEligibleKindsOnly` | a configured type with a string ID, an entity, an asset and a container property captures only its eligible properties and counts `stream.persistence.ineligible_properties` once |
| `Test_StreamingPersistence_StateRestoredAtActivation` | modified values of three persistent entities reappear after unload and reload, visible from phase 2 of the activation tick, never their authored values first |
| `Test_StreamingPersistence_ComponentAddedAndRemovedSurvive` | a configured `Looted` tag added at runtime to a persistent chest and a configured component removed from another reappear, respectively stay absent, after unload and reload, from phase 2 of the activation tick (SCENE's amendment during BUILDING, not a subsequent sync point); an added component carries its captured values |
| `Test_StreamingPersistence_UnconfiguredTypeKeepsAuthoredPresence` | a record written with a type the loading build no longer configures leaves that type's presence authored and counts `stream.persistence.unconfigured_types` |
| `Test_StreamingPersistence_TombstoneSkipsAtPreparation` | a destroyed persistent entity is not spawned on reload, and an ENTITY property naming it is null |
| `Test_StreamingPersistence_UnloadDestroysAreNotTombstones` | the destroys of an unload record no tombstone, and the "unload in progress" flag clears at the unload's `Streaming.UnbindReferences` playback; second case: a QUEUED cell unloaded (T9) and a LOADING cell cancelled (T7) never set the flag, and when each cell is loaded and activated again, a persistent entity destroyed in phase 5 of its activation tick gets its pending bit and is DESTROYED in the delta after the next unload |
| `Test_StreamingPersistence_DeathInLaterPhaseAfterRejectedUnload` | an unload buffer rejected, then a death in phase 5 of that tick: no tombstone bit is written; the re-issued unload's capture writes DESTROYED by the absence rule; the entity is not spawned at reload |
| `Test_StreamingPersistence_PhaseOneDestroyInUnloadTick` | a phase-1 system ordered after `Streaming.Update` destroys a persistent entity in the tick that records its cell's unload: the entity reloads with the state captured in that update (the defined window of Runtime.md "Tombstones") |
| `Test_StreamingPersistence_ManyDeathsInOneTickAllTombstoned` | 5 cells of 1,000 persistent entities each, every entity destroyed in one tick: every pending bit is written at its destroy's playback, the next update moves all 5,000 into the committed bits, nothing is dropped, and every destroyed entity is DESTROYED in its cell's delta after unload |
| `Test_StreamingPersistence_TombstonesWrittenAtDestroyPlayback` | a death at sync point 10 sets its pending bit in `CellStatus` property 10 within that playback; the holder read through the world after that tick's end-of-tick playback, before the next update (what a phase-11 capture holds), carries it; the next update commits it; an external unload played at sync point 10 sets pending bits that T27 discards (`stream.persistence.tombstones_dropped`) while the committed bits of earlier deaths reach the merged delta |
| `Test_StreamingPersistence_StatusPersistedOnHolder` | after every transition row of a scripted run (failures of each class, a delay started, cut and cleared, a preemption, a release pending, a delta merge rejected once), the cell's `CellStatus` equals its runtime record field by field at the end of the step, and the holder of a cell that was never issued does not exist; second case: the commit of a cell's first load is refused (`SCENE_ERROR_DUPLICATE_INSTANCE`: the test loaded the cell scene itself first), so the holder its load buffer spawned exists from that sync point with State REQUESTED, and at the end of step 3 of the next update the holder is mapped and its `CellStatus` equals the record (FAILED, the backoff's retry tick, the failure counter) |
| `Test_StreamingPersistence_DefaultHolderPruned` | a cell with no persistent entity loaded, activated and unloaded has its holder destroyed in step 14 of the tick T16 runs (`stream.persistence.holders_pruned` + 1 at the next update, which leaves the record at the default status and re-spawns nothing); a cell whose delta holds a record, a FAILED cell and a cell with a committed tombstone keep theirs; with the prune buffer rejected (container capacity 0) the holder stays, is not pruned in the following ticks, and is pruned when its cell next enters UNLOADED; the cell's next load spawns a new holder with State REQUESTED |
| `Test_StreamingPersistence_HolderSpawnedWithFirstLoad` | a cell's first load buffer spawns its holder; with that buffer rejected (container capacity 0) neither the holder nor the load exists and T4 applies; a catalogue miss of a holder-less cell records a status-holder action and the cell is FAILED_PERMANENT, and with that action rejected it is UNLOADED again at the next update; gameplay destroying a holder makes the next update re-spawn it (`stream.persistence.holders_respawned`) |
| `Test_StreamingPersistence_ReparentedOutSurvivesWithoutDuplicate` | a persistent entity reparented out of its cell survives the unload and is not spawned again at reload |
| `Test_StreamingPersistence_OverflowDropsStateKeepsTombstones` | an encoding of exactly 65,535 bytes is stored unchanged; one above 65,535 bytes drops STATE records in descending ID, keeps every DESTROYED record, logs once and counts `delta_overflow` |
| `Test_StreamingPersistence_WorldPositionAnchorRelative` | a flagged `VEC3` captured before a rebase restores at the right absolute position after it |
| `Test_StreamingPersistence_SchemaMigratedAtRestore` | a record of a component written at schema version 1 is migrated to version 2 before it is applied |
| `Test_StreamingPersistence_CorruptDeltaActivatesAuthored` | a corrupted holder container activates the cell authored and logs `STREAMING_ERROR_DELTA_CORRUPT` |
| `Test_StreamingSettings_RefusesDerivedTransforms` | `Zenith.WorldTransform` or `Zenith.PreviousWorldTransform` in `stream.persistence.componentTypes`, and a quantum above twice the threshold, fail `Zenith_ReadStreamingSettings` with `STREAMING_ERROR_CONFIG_INVALID` naming the key |

### References, regions, rebase

| Test | Asserts |
|---|---|
| `Test_StreamingReferences_BindBothDirectionsSecondActivationWins` | for A → B, activating A then B and B then A both bind the slot |
| `Test_StreamingReferences_UnbindNullsOnlyStaleHandle` | unloading B nulls A's slot; a slot gameplay re-pointed elsewhere is left untouched |
| `Test_StreamingReferences_FixedArrayElement` | element 2 of a fixed array binds without changing elements 0, 1, 3 |
| `Test_StreamingReferences_MissingTargetCountsUnresolved` | a tombstoned target leaves null and counts `unresolved`, logged once |
| `Test_StreamingRegions_FlagFollowsCollisionCell` | a region-bound entity's flag is false over an unloaded collision cell and true the tick after it activates |
| `Test_StreamingRegions_DestroyPolicyOnTransition` | DESTROY destroys exactly on the true → false transition between two consecutive bound evaluations |
| `Test_StreamingRegions_UnboundWorldAllTrue` | no binding: every flag true |
| `Test_StreamingRegions_GameplayGridFallback` | a world with no collision grid uses its first gameplay grid |
| `Test_StreamingRebase_ThresholdIntervalQuantum` | no shift at 2,048 m; at 2,049 m a shift of exactly 2,048 m (quantum 2,048); none again within 30 ticks; `|L| ≤ q/2` after each shift |
| `Test_StreamingRebase_ShiftInActivationTick` | a tick with an admitted cell records the shift; the activated cell's entities, placed with the pre-shift O, end at `anchor − O_new` (no postponement) |
| `Test_StreamingRebase_RootTranslationExact` | cells admitted after a shift are placed at `anchor − O` to within the binary32 rounding of one subtraction |
| `Test_StreamingRebase_DisabledByDefault` | with the default configuration no shift is ever recorded |
| `Test_StreamingRebase_PrecisionGauge` | with the primary at 2,198 m and a content grid scaled by 4, `stream.precision.cells_beyond_radius` counts exactly the ACTIVE cells with a corner beyond 4,096 m |

### HLOD and the service

| Test | Asserts |
|---|---|
| `Test_StreamingHLOD_CutNestsByDistance` | at distances 5,000, 1,500 and 300 m the cut displays L2, then the L1 nodes, then content, with no region covered twice |
| `Test_StreamingHLOD_ActiveContentSuppressesProxy` | a node with one ACTIVE base cell is never displayed |
| `Test_StreamingHLOD_ProxyHeldUntilCellActive` | a cell walked into from beyond its activation radius keeps its level-1 proxy displayed through REQUESTED, LOADING, QUEUED and loses it in the frame its content exists; an UNLOADING cell's proxy returns in the frame of the unload; at no frame is the region uncovered and uncounted |
| `Test_StreamingHLOD_ScaledSourceLeavesNoUncoveredRing` | a driving source with radius scale 0.5: every base cell between `Ract · 0.5` and `Ract` is covered by a proxy or counted as a gap, never neither |
| `Test_StreamingHLOD_DrivingSourceSelection` | with `DrivesHLOD` false everywhere the primary drives the cut; with no primary, the lowest simulation ID; a preload marker never drives it by default |
| `Test_StreamingHLOD_NotReadyFallsBackAndCountsGaps` | an L2 node whose mesh is not resident falls back to its L1 children; missing L1 nodes count gaps |
| `Test_StreamingHLOD_ResidencyHysteresis` | meshes are requested within max + 64 m and released beyond max + 128 m (scale 1) |
| `Test_StreamingHLOD_ExtractorKeysAndOrder` | the extractor writes module keys (bit 63 set), REMOVEs then ADDs in ascending key, and nothing for an unchanged cut (through `Zenith_FixtureExtractionResolver`) |
| `Test_StreamingHLOD_FullExtractionReemits` | `IsFullExtraction()` re-emits every displayed node as ADD and no REMOVE |
| `Test_StreamingHLOD_RebaseUpdatesTransforms` | a rebase writes UPDATE with the shifted translation for every displayed node and no history reset |
| `Test_StreamingService_BoosterRaisesPriority` | a LOADING cell's booster request is HIGH, CRITICAL within 60 ticks of its source's teleport, and is destroyed in the frame the cell leaves LOADING or CANCELLING |
| `Test_StreamingService_PrefetchCappedAndCancelled` | at most 8 new prefetches per frame; a prefetch is cancelled when its cell becomes wanted or leaves the band |
| `Test_StreamingService_PrefetchPeakDeferred` | with the governor forced to refuse `TRANSITION_PEAK_STREAMING_DOUBLE_RESIDENCY`, a prefetch under a displayed proxy is deferred and counts `stream.peak.deferred`; granted, the reservation is released when the node leaves the cut |
| `Test_StreamingService_RequiredLoadReservesDoubleResidency` | a required load under a displayed proxy reserves its estimated bytes in the first frame it is LOADING, without delaying the load, and holds the reservation until the cell holds content |
| `Test_StreamingService_RequiredLoadRefusedReleasesProxy` | with the reservation refused, the covering proxy's meshes are released in the same frame, its region counts gaps until the cell activates, `stream.peak.proxy_released` + 1, and the governor's reserved peak never exceeds the row |
| `Test_StreamingService_FrameBudgetResumes` | with `stream.service.frameBudgetUs` 100 and a large backlog, work stops at the budget and resumes in the same order next frame |
| `Test_StreamingService_TeleportToReadyMeasured` | the gauge equals the frames' clock difference between the teleport and the first ready frame |
| `Test_StreamingService_GateRules` | the gate of a live load opens on group completion, on `UNKNOWN_GROUP`, on `DEVICE_LOST`, 300 frames after the stored request frame, after 30 frames of `QUEUE_FULL`, and at `Acquire` headless |
| `Test_StreamingService_GroupRequestedForLoadCommittedInSameTick` | a load recorded in phase 1 and committed at that tick's sync point (never observed REQUESTED by the epilogue) has its group requested in that frame's epilogue |
| `Test_StreamingService_DebugViewRecords` | with `stream.debug.view` naming a grid in a development build, the service writes one XZ rectangle per cell within 2 km in its state colour, one outline per displayed HLOD node and one circle per source radius; nothing in a Shipping-configured run |
| `Test_StreamingService_CountersDifferencedPerFrame` | with three ticks in one frame, the service publishes the sum of the three ticks' events and writes nothing into `Zenith.Streaming.State` |

### Snapshot pool

| Test | Asserts |
|---|---|
| `Test_StreamingSnapshotPool_EveryInterleavingOfOneSlot` | every interleaving of a slot's six concurrent operations (simulation release, preparation release, gate open, gate poll, tombstone and amendment query, tag lookup) across two threads, sequenced by explicit barriers: after each step the mask, the tag entry and the FREE transition match the protocol, and the slot is reused only when FREE |
| `Test_StreamingSnapshotPool_ConcurrentGateAndEnd` | 8 threads run 100,000 acquire/gate/skip/amend/end/release cycles over 112 slots; no slot is lost or double-freed, every tag resolves to its slot while held (`win-asan`) |
| `Test_StreamingSnapshotPool_RetiredUntilFree` | detaching a world with two running preparations retires the pool, the hook answers both safely through the pointer they captured, and the next attach sweep destroys it |
| `Test_StreamingSnapshotPool_EntityTableAnswers` | `IsTombstoned` and `GetAmendment` answer from the table for persistent entries and "nothing" for any other entity |

### Module and diagnostics

| Test | Asserts |
|---|---|
| `Test_StreamingModule_RegisterTypesOnlyTypesAndKeys` | `RegisterTypes` adds the types and keys and nothing else |
| `Test_StreamingModule_StartRegistersEverything` | the kind loader, four command kinds, four bindings and the consumer are registered |
| `Test_StreamingModule_ScheduleFreezes` | a world with the module attached freezes its schedule with no diagnostic: `Streaming.Update` before `Scene.ActivateAdditive`, the service and the extractor in the epilogue, every declared read and write accepted |
| `Test_StreamingModule_AttachDetachRepeat` | 100 attach/detach cycles leave no registration, allocation or retiring pool behind, and detach allocates nothing |
| `Test_StreamingModule_SettingsRangesAndTypes` | an unknown persistent type fails Start with `STREAMING_ERROR_CONFIG_INVALID` naming the key; out-of-range keys are refused by the configuration schema |
| `Test_StreamingBindings_ReadWorld` | the three `READ_WORLD` bindings return this tick's cell states from systems and behaviours; in a rebase tick, from a phase-5 system, `IsPositionActive` and `GetCellState` answer for the shifted local position and `GetWorldOrigin` returns the new origin; an unknown grid raises a controlled Luau error |
| `Test_StreamingBindings_OverrideToolsOnly` | `Streaming.SetCellOverride` is a context error outside a tools invocation |
| `Test_StreamingState_DumpShape` | `WriteDump` of an unbound and of a bound fixture world equals the committed golden files `Tests/Golden/stream-dump-unbound.json` and `-bound.json` byte for byte |
| `Test_StreamingDiagnostics_EveryCounterPublished` | a scripted run that triggers every event publishes every counter of `Zenith2/Docs/Design/STREAM.md` → "Counters" through the profiler hooks with its kind, suffixed by the world tag |
| `Test_StreamingDiagnostics_LogCategoriesAndLimits` | every STREAM log line of the failure-path tests is in one of the seven categories, and the per-event limits hold (one line per cell per 600 ticks after the first) |
| `Test_StreamingDiagnostics_ZoneNames` | the profiler hook capture of one tick and one frame holds exactly the zones of "Profiler zones" |

## Cook and format tests

| Test | Asserts |
|---|---|
| `Test_StreamingWorldFormat_RoundTrip` | a generated world of 3 grids, 1,000 cells, references, persistent entries and HLOD nodes encodes, parses and compares equal |
| `Test_StreamingWorldFormat_ByteIdenticalAnyInputOrder` | shuffled descriptor spans give the same bytes (10 seeds) |
| `Test_StreamingWorldFormat_LimitsAtAndAbove` | each limit of "Limits" at the limit loads; at limit + 1 fails with `STREAMING_ERROR_WORLD_LIMIT` or the cook's `COOK_ERROR_STREAM_LIMIT` ([Z2-N-ARCH-953]) |
| `Test_StreamingWorldFormat_LoaderCancelFreesPartial` | cancelling the kind loader between `BeginLoad` and activation frees the partial block (`Cancel`, `AcknowledgeCancel`) |
| `Test_StreamingWorldFormat_FindCellHashCoversRange` | `FindCell` finds every cell of a grid with coordinates at ±2^20 and returns invalid for every absent neighbour; `FindCellByScene` finds every cell by its scene GUID and returns invalid for an unknown GUID |
| `Test_StreamingCook_ValidDocumentCooks` | the example document of Formats.md cooks; the report member lists per-grid B and B' |
| `Test_StreamingCook_DerivesAnchorAndPersistent` | anchor ID, entity count, persistent entries and estimated bytes are derived from the cell scenes, not authored |
| `Test_StreamingCook_WorstCaseBound` | the bound of "Worst-case active cells" for four grid configurations equals the closed form: one where a dependent AUDIO grid's reach exceeds its COLLISION dependency's, so the pinned reach dominates `B(h)`; and a three-grid chain gameplay → navigation → collision with a far-reaching gameplay grid, where reach(collision) is `r_gp + c_gp + m_nav · c_nav + c_nav + m_col · c_col` and exceeds both `r_col` and the one-level term through navigation alone; and a world whose gameplay cell carries an explicit `dependsOn` naming a navigation cell 10 km away that depends on four collision cells, where E(navigation) is 1, E(collision) is 4 and both grids' B' include them |
| `Test_StreamingCook_CellEntityLimitFollowsSpawnCap` | with `ecs.maxSpawnsPerTick` configured at 8,192, a cell of 8,193 entities fails V16 naming 8,192 |
| `Test_StreamingCook_DeterministicOnRepeat` | two cooks of one document give identical bytes and DDC keys |
| `Test_StreamingCook_Migration_V1Identity` | the version-1 chain is empty and a version-1 document opens without a migration plan |

### Reader check tests

One test per reader check of Formats.md "Reader checks": the minimal mutation of a valid generated world that violates the check fails the load with its number as detail (`STREAMING_ERROR_WORLD_LIMIT` for S3, `STREAMING_ERROR_WORLD_CORRUPT` otherwise):

| Test | Check |
|---|---|
| `Test_StreamingWorldFormat_ReaderCheckS1` | S1: exactly 8 sections, kinds 1 … 8, each index 0 |
| `Test_StreamingWorldFormat_ReaderCheckS2` | S2: the world section decodes to exactly 64 bytes; flags and reserved zero; the world ID equals the header GUID |
| `Test_StreamingWorldFormat_ReaderCheckS3` | S3: G, C, the persistent, reference and HLOD counts within "Limits" (else `STREAMING_ERROR_WORLD_LIMIT`) |
| `Test_StreamingWorldFormat_ReaderCheckS4` | S4: every section's decoded size equals its count times its record bytes |
| `Test_StreamingWorldFormat_ReaderCheckS5` | S5: grid records: name charset, length, zero padding and uniqueness; role, metric and level count in range; reserved zero |
| `Test_StreamingWorldFormat_ReaderCheckS6` | S6: grid numeric fields finite and within the grid record's rules; the radii ordering; HLOD distances per level |
| `Test_StreamingWorldFormat_ReaderCheckS7` | S7: grid cell ranges contiguous, in grid order, covering `[0, C)` |
| `Test_StreamingWorldFormat_ReaderCheckS8` | S8: the grid dependency masks name only other existing grids and form an acyclic graph (Kahn's algorithm over ≤ 8 nodes) |
| `Test_StreamingWorldFormat_ReaderCheckS9` | S9: cell records: grid in range and matching its range; coordinates within ±2^20; entity count 1 … 16,384; reserved zero |
| `Test_StreamingWorldFormat_ReaderCheckS10` | S10: cells sorted strictly by (grid, z, x) (which also proves coordinate uniqueness) |
| `Test_StreamingWorldFormat_ReaderCheckS11` | S11: cell scene IDs non-null and unique (checked by sorting a scratch copy of the IDs) |
| `Test_StreamingWorldFormat_ReaderCheckS12` | S12: the four range families contiguous per "Cell records"; every count within its per-cell limit |
| `Test_StreamingWorldFormat_ReaderCheckS13` | S13: dependency runs strictly ascending, in range, no self-dependency |
| `Test_StreamingWorldFormat_ReaderCheckS14` | S14: the cell dependency graph is acyclic (iterative Kahn's algorithm over C nodes with the dependents table built in step L3 of the loader) |
| `Test_StreamingWorldFormat_ReaderCheckS15` | S15: persistent entries: cell equals the cell whose range holds them; mode 1 … 2; sorted by persistent ID within the cell; IDs unique across the world (checked on the sorted by-ID index built in step L4) |
| `Test_StreamingWorldFormat_ReaderCheckS16` | S16: reference records: cells in range and different; grouped by target per the cell ranges; element is 0xFFFFFFFF or < 65,536; reserved zero; no duplicate (source entity, type, property, element) |
| `Test_StreamingWorldFormat_ReaderCheckS17` | S17: section 7: each index < reference count; each appears exactly once; grouped by source cell per the cell ranges |
| `Test_StreamingWorldFormat_ReaderCheckS18` | S18: HLOD nodes: level, grid, mesh count, flags in range; mesh slots beyond the count null, within it non-null; reserved zero |
| `Test_StreamingWorldFormat_ReaderCheckS19` | S19: HLOD hierarchy: parent and child links consistent and in range; child coordinates inside their parent's region; level-1 nodes match their cell's coordinate and the cell names them; breadth-first order |
| `Test_StreamingWorldFormat_ReaderCheckS20` | S20: the world record's derived fields (maximum entities, estimated bytes) equal what the tables give |

### Validation message tests

One test per validation message of Formats.md "Validation messages", each cooking the committed document named after it under `Zenith2/Tools/ZenithCook/Tests/Streaming/Documents/` and asserting the cook error and the message text with its JSON pointer:

| Test | Document | Condition | Cook error |
|---|---|---|---|
| `Test_StreamingCook_MessageV1` | `Documents/V1.streamworld.json` | not JSON, wrong `$type`, schema version newer than 1 | `COOK_ERROR_STREAM_DOCUMENT` |
| `Test_StreamingCook_MessageV2` | `Documents/V2.streamworld.json` | a required member missing or of the wrong JSON type | `COOK_ERROR_STREAM_DOCUMENT` |
| `Test_StreamingCook_MessageV3` | `Documents/V3.streamworld.json` | grid count 0 or above 8; a grid name invalid or duplicated | `COOK_ERROR_STREAM_GRID` |
| `Test_StreamingCook_MessageV4` | `Documents/V4.streamworld.json` | an unknown role or metric | `COOK_ERROR_STREAM_GRID` |
| `Test_StreamingCook_MessageV5` | `Documents/V5.streamworld.json` | a cell size outside 32 … 8,192 or not a multiple of 32 | `COOK_ERROR_STREAM_GRID` |
| `Test_StreamingCook_MessageV6` | `Documents/V6.streamworld.json` | radii out of order, a non-finite number, height out of range | `COOK_ERROR_STREAM_RADII` |
| `Test_StreamingCook_MessageV7` | `Documents/V7.streamworld.json` | HLOD distances wrong length, decreasing, or below the activation radius | `COOK_ERROR_STREAM_RADII` |
| `Test_StreamingCook_MessageV8` | `Documents/V8.streamworld.json` | a cell coordinate beyond ±2^20 | `COOK_ERROR_STREAM_CELL_RANGE` |
| `Test_StreamingCook_MessageV9` | `Documents/V9.streamworld.json` | two cells with one coordinate, or one scene in two cells | `COOK_ERROR_STREAM_CELL_DUPLICATE` |
| `Test_StreamingCook_MessageV10` | `Documents/V10.streamworld.json` | a cell's scene is not a scene asset of the game | `COOK_ERROR_STREAM_CELL_SCENE_KIND` |
| `Test_StreamingCook_MessageV11` | `Documents/V11.streamworld.json` | a grid or cell dependency cycle | `COOK_ERROR_STREAM_DEPENDENCY_CYCLE` |
| `Test_StreamingCook_MessageV12` | `Documents/V12.streamworld.json` | the cell scene has no root carrying `Zenith.Streaming.CellAnchor`, or more than one root | `COOK_ERROR_STREAM_CELL_ANCHOR` |
| `Test_StreamingCook_MessageV13` | `Documents/V13.streamworld.json` | the anchor's cell scene (property 1) is not the cell's own scene, or its world ID disagrees with the document | `COOK_ERROR_STREAM_CELL_ANCHOR` |
| `Test_StreamingCook_MessageV14` | `Documents/V14.streamworld.json` | the anchor root's local transform is not the identity | `COOK_ERROR_STREAM_CELL_ANCHOR` |
| `Test_StreamingCook_MessageV15` | `Documents/V15.streamworld.json` | an entity position outside the cell's rectangle grown by the overlap margin, or outside the vertical extent | `COOK_ERROR_STREAM_CELL_BOUNDS` |
| `Test_StreamingCook_MessageV16` | `Documents/V16.streamworld.json` | the cell scene has more entities than min(16,384, the game's configured `ecs.maxSpawnsPerTick`, the value of Z2-B-ECS-003 once allocated, read from the cook context's configuration) | `COOK_ERROR_STREAM_LIMIT` |
| `Test_StreamingCook_MessageV17` | `Documents/V17.streamworld.json` | a `Zenith.Streaming.Persistent` component whose entity ID or cell scene (property 2) disagrees, or a persistent ID listed in two cells | `COOK_ERROR_STREAM_PERSISTENT_ID` |
| `Test_StreamingCook_MessageV18` | `Documents/V18.streamworld.json` | more than 1,024 persistent entities in a cell | `COOK_ERROR_STREAM_LIMIT` |
| `Test_StreamingCook_MessageV19` | `Documents/V19.streamworld.json` | more than 32 dependencies for a cell | `COOK_ERROR_STREAM_LIMIT` |
| `Test_StreamingCook_MessageV20` | `Documents/V20.streamworld.json` | an HLOD node whose grid, level, coordinate or meshes are invalid | `COOK_ERROR_STREAM_HLOD` |
| `Test_StreamingCook_MessageV21` | `Documents/V21.streamworld.json` | an HLOD node with no parent below the top level, or no children above level 1 | `COOK_ERROR_STREAM_HLOD` |
| `Test_StreamingCook_MessageV22` | `Documents/V22.streamworld.json` | a reference that fails step 6 | `COOK_ERROR_STREAM_REFERENCE` |
| `Test_StreamingCook_MessageV23` | `Documents/V23.streamworld.json` | the worst-case live-instance bound above 192 | `COOK_ERROR_STREAM_LIMIT` |
| `Test_StreamingCook_MessageV24` | `Documents/V24.streamworld.json` | any other limit of "Limits" | `COOK_ERROR_STREAM_LIMIT` |

## Integration tests

All run headless (no GPU and no renderer; none is `needs-gpu`), on each ECS backend (`ZenithECS_Custom`, `_Flecs`, `_EnTT`) through `Zenith_Integration_Streaming`. Every test that saves and restores does it through the real runtime: a save is written through `Zenith_SaveSystem` (the suite links ZenithSave and both compression backends, `Zenith2/Docs/Design/STREAM.md` → "Integration suite and fuzz target") and restored into a fresh world; a test that compares tick hashes across a restore records the run (`--record`) and continues it with `Zenith_RuntimeReplay_ResumeFromSave` (`Zenith2/Docs/Design/SAVE/RuntimeBridge.md` → "Runtime bridge declarations"), asserting `m_bHashesCompared` and `m_bVerified` against the recording, never a second live run, because the cell loads of the compared window publish at ticks their preparations' timing decides ([Z2-N-SCRIPT-338]). The cases that save or restore keep the names of the unit groups they extend:

| Test | Asserts |
|---|---|
| `Test_StreamingIntegration_TraversalLoadsAndUnloads` | a source walking 2 km across a generated 16 × 16-cell world: every cell within the activation radius is ACTIVE within 30 ticks of entering it; none beyond KEEP stays ACTIVE after its delay; the caps hold every tick |
| `Test_StreamingIntegration_TeleportReadiness` | a 5 km teleport: `Ready` false at once, true when every required cell is ACTIVE, `TeleportSequence` + 1; required cells load before every other |
| `Test_StreamingIntegration_TwoSourcesShareCells` | the pawn and a preload marker: their union is ACTIVE and each source's readiness is its own |
| `Test_StreamingIntegration_CollisionBeforeGameplay` | no gameplay cell is ever ACTIVE while a collision cell it depends on is not, sampled every tick |
| `Test_StreamingIntegration_RegionFreezesBodies` | a body over a cell that unloads has `InActiveRegion` false in the same tick its collision cell starts unloading (with PHYS's freeze once PHYS lands; until then the flag alone) |
| `Test_StreamingIntegration_PersistAcrossReload` | 50 random modifications, component additions and removals and destructions over 20 cells survive 10 traversal round trips |
| `Test_StreamingIntegration_ReferencesAcrossTraversal` | 100 cross-cell references are bound exactly while both cells are ACTIVE, sampled every tick |
| `Test_StreamingIntegration_RebaseLongJourney` | a 60 km straight journey at 300 m/s (12,000 ticks) starting at the world origin (O = 0, the primary at 0) through a strip world whose cells activate continuously: exactly 29 shifts are recorded (P passes 2,048 k for k = 1 … 29); the primary's local position never exceeds 2,198 m on an axis; every active entity's absolute position (O + p) stays within 1 mm of its authored absolute position |
| `Test_StreamingIntegration_SaveDuringStreaming` | saves at a tick with cells LOADING, QUEUED, ACTIVATING, UNLOADING and CANCELLING, then loads into a fresh world: the capture is COHERENT, every restored in-flight load is adopted, the cell set converges to the same set as the unsaved run within 60 ticks and every persistent value matches (with SAVE's `Test_SaveCrossSystem_LoadingInstanceReRequestedWithHookTag`) |
| `Test_StreamingIntegration_SaveWithManyQueuedCells` | with `stream.maxConcurrentCellLoads` 48 and `stream.maxQueuedCells` 64 (the defaults hold at most 48 in flight), a save with 100 cells LOADING or QUEUED (admission held by dependencies), restored: no acquisition is refused, every tombstone of those cells is honoured (no tombstoned entity is spawned), no instance is left QUEUED beyond the convergence window, and no load fails `DUPLICATE_INSTANCE` |
| `Test_StreamingIntegration_RestoreBeyondPoolReleases` | a save edited to hold 130 tagged in-flight cell loads: 112 are adopted, the rest are released (`stream.restore.unadopted`), none activates or resurrects a tombstoned entity, and each released cell loads afresh when wanted; second case: a save whose restored additive set leaves no room for one re-request, so SCENE refuses it (row ST2 (j)): `OnLoadCommitted` with instance 0 appends nothing, `stream.restore.unadopted` + 1, and the cell loads afresh when wanted |
| `Test_StreamingIntegration_RestoreRejectedActivationAdopts` | two COHERENT captures restored into fresh worlds: one at the tick SCENE's activation buffer of an admitted cell was rejected (container capacity 0 for it: the holder says ACTIVATING, SCENE's instance is QUEUED and is re-requested), and one at the tick the unload of a QUEUED cell was rejected (the holder says UNLOADING with "unload of a QUEUED instance", the instance QUEUED and re-requested); in each, the rebuild makes the cell UNLOADED and adopts its re-requested load as LOADING with its slot, no `ZENITH_ASSERT` fires, no discard reaches `uSTREAMING_INVALID_CELL`, the cell converges to the unsaved run's state within 60 ticks, and `IsEverySlotFree` holds after the run drains; a third case, a save edited so that two restored in-flight tagged instances name the same cell scene (two triples of one cell): R4 neutralises its slot (empty table, gate open, simulation bit released), releases it, its discarded publication counts `stream.hook.unknown_tags`, and the slot becomes FREE when its preparation ends |
| `Test_StreamingIntegration_RestoreActiveWithPendingStateApply` | a snapshot taken in the tick an activation's ApplyCellState buffer was rejected (anchor `Instance` 0, the holder's State ACTIVATING with "state applied" clear): the rebuild restores the cell ACTIVATING from its holder, the first restored update applies T12 and re-records ApplyCellState exactly as the uninterrupted run does, and the restored values appear; a second case with the cell's holder destroyed before the save adopts the instance by its anchor (R4) and re-spawns the holder |
| `Test_StreamingIntegration_ExternalUnloadAndDestroyedAnchor` | gameplay C++ unloads an ACTIVE cell's instance and, in another cell, destroys the anchor root: both cells become UNLOADED by T27 and T28, their deltas keep the earlier tombstones, and both reload when wanted; second case: the external unload is recorded in phase 10, so it plays at the tick's last sync point: T27 at the next update discards the cell's pending bits, no persistent entity of the cell is written DESTROYED by that unload, the entities reload, and nothing of that unload marks a subsequent instance of the cell |
| `Test_StreamingIntegration_ExactRestoreWithFailedAndDelayedCells` | a recorded run (`--record`) reaches a tick t at which one cell is FAILED mid-backoff after a capacity failure, one FAILED with a content retry consumed, one FAILED_PERMANENT, one ACTIVE in its unload delay, one ACTIVE preempted, one with a FORCE_UNLOADED override, one with pending and committed tombstones, a DESTROY-policy region-bound entity at a region edge, a source moving 5 m per tick and a teleport played at the last sync point, with no cell LOADING, QUEUED or CANCELLING and no tagged commit refused in tick t; it saves there (a save requested in phase 10, captured at phase 11, carrying the replay position and no `COHERENT_ONLY` flag) and runs on for 600 ticks, in which the FAILED cells retry and the moving source issues loads; `Zenith_RuntimeReplay_ResumeFromSave` restores the save into a fresh world and continues ticks t + 1 … t + 600 with every hash compared against the recording, and the result reports `m_bHashesCompared` and `m_bVerified` true, on each ECS backend |
| `Test_StreamingIntegration_RefusedCommitCaptureIsCoherent` | a recorded run (`--record`) in which SCENE refuses the commit of a cell's first load (the additive set filled to 256 by the game, so the holder exists only from the load buffer's sync point): the save taken at that tick's phase 11 carries `COHERENT_ONLY` naming `Zenith.Scene.AdditiveSet`, and a replay resumed from it reports `m_bHashesCompared` false; at the next tick the holder is mapped in step 3 before the commit-refused inbox is drained, T3 writes FAILED with its backoff into it, and the save taken at that tick's phase 11 carries no `COHERENT_ONLY`; a replay resumed from it with `Zenith_RuntimeReplay_ResumeFromSave` continues 600 ticks, through the cell's retry, with `m_bHashesCompared` and `m_bVerified` true |
| `Test_StreamingBands_OverridesPersistOnWorldReference` | a `Streaming.SetCellOverride` playback writes the bound reference's slot (replace, free on NONE, lowest free slot, a 65th distinct override refused and counted, refused while unbound); the world saved through `Zenith_SaveSystem` and restored into a fresh world holds the slots, and the override applies in the first tick after the restore; a rebinding to the same reference keeps it |
| `Test_StreamingRegions_DestroyNeedsBoundPreviousEvaluation` | an entity spawned with an authored `InActiveRegion` true, an entity whose world was unbound the tick before, and an entity whose last evaluation was 2 ticks earlier are not destroyed by an evaluation that computes false; properties 2 to 4 survive a save and restore into a fresh world, and a replay resumed from the save (`Zenith_RuntimeReplay_ResumeFromSave`) destroys in its first tick exactly the entities the recording destroys (`m_bVerified`) |
| `Test_StreamingAdmission_AdoptedLoadsAboveCapsIssueNothing` | a runtime started with `stream.maxConcurrentCellLoads` 48 saves with 48 cells LOADING, is shut down, and a runtime started with 16 restores the save into a fresh world: all 48 are adopted, no load is issued while 16 or more are in flight (each wanted candidate counted in `stream.cells.loads_deferred`), and issuing resumes when the count falls to 15; second case: FORCE_UNLOADED overrides on all 48 at once cancel them, their discarded publications arrive at one boundary, the discard inbox (capacity 112) takes every one without a `ZENITH_CHECK`, and every cell reaches UNLOADED by T18 |
| `Test_StreamingPersistence_DeltaSurvivesRecookWithInsertedCell` | deltas and anchors saved against one cook are restored into a fresh world against a re-cook that inserts a cell early in the (grid, z, x) order (every following index shifts): every delta applies to the cell of its `CellScene`, every saved destruction and state change survives |
| `Test_StreamingPersistence_RecookRemovedCellKeepsOrphanDelta` | a save restored against a re-cook that removes a cell leaves that cell's holder, never applied, counted in `stream.persistence.orphan_deltas`, and saved again with every record whose entity no longer belongs to any cell; a record whose entity was deleted from the world's content is dropped (`records_dropped`) |
| `Test_StreamingPersistence_RecookMovedEntityRehomed` | a save restored against a re-partition that doubles the gameplay cell size, which moves every persistent entity to a new cell: R5 sets the re-home bit on every source holder and moves nothing; step 14 re-homes each saved DESTROYED and STATE record to the entity's new cell once that cell is UNLOADED (`stream.persistence.records_rehomed`), at most `stream.unload.maxCellsPerTick` holders per tick; each new cell activates with the saved destructions and values; an existing record of the new cell wins over a re-homed one; a record whose new cell is ACTIVE stays in its source holder (`stream.persistence.rehome_deferred`), survives that holder's next capture with the bit kept, and is re-homed by step 14 in the tick its target cell has unloaded; an orphan emptied by re-homing is destroyed and the bit is cleared one tick after the last move; a re-home buffer rejected once (container capacity 0) is recorded again at the next tick. Second case: the run is recorded (`--record`), a save is taken in a tick before a deferred re-home's target unloads, and a replay resumed from it (`Zenith_RuntimeReplay_ResumeFromSave`) records the re-home in the same tick under the same merge key and reports `m_bHashesCompared` and `m_bVerified` true through it |
| `Test_StreamingService_GateRulesRestoredLoadHeadless` | headless, a load adopted by the rebuild after a restore into a fresh world has its gate opened by R4 right after `FillEntityTable`, and its preparation reaches BUILDING |
| `Test_StreamingIntegration_WorldTransitionDiscardsInFlight` | a transition with 8 cells LOADING discards them, leaves no slot held after the preparations end, and binds the new world fresh |
| `Test_StreamingIntegration_LiveInstanceCapWithGameAdditives` | with 100 of the game's own additive scenes and `stream.maxLiveInstances` 160, STREAM's loads see `SCENE_ERROR_LIMIT` once 156 of its cells are live, back off without consuming retries, recover when the game unloads 20 scenes, and the sum never exceeds SCENE's 256 (fewer than 65 game scenes cannot reach the limit, because STREAM's cap is at most 192) |
| `Test_StreamingIntegration_HeadlessGatesAndNoPresentation` | headless, every live load's gate opens at `Acquire` and every adopted load's in R4; no PSO group, HLOD mesh request or extraction record is issued; every simulation algorithm runs |
| `Test_StreamingIntegration_ShutdownMidLoad` | shutdown with 16 cells LOADING completes, every slot FREE, the governor reports no reservation |

## Stress and determinism tests

| Test | Asserts |
|---|---|
| `Test_Streaming_CancelReload1000Cycles_NoLeak` | 1,000 cycles, each picking a random cell (seeded, logged) and a random cancellation stage, and running it once with no booster and once with the service's booster of the cell scene alive: the stage is one of LOADING before start; ASSET `HEADER_READ` or `SECTIONS` `READING` (I/O, held by ASSET's gated mount source); ASSET `SECTIONS` `DECODING` (decompression of a zstd section of the cell scene, held by ASSET's stage hold, row ST20); ASSET `DELIVER` or `ACTIVATING` of a CPU-kind hard dependency (held by the same hold); SCENE `DECODING`, `DEPENDENCIES` and `BUILDING` (held by SCENE's preparation hold, row ST2 (h)); `GATED` (STREAM's own closed gate); QUEUED; after ACTIVATING; ACTIVE. The cell is forced wanted and unwanted with overrides. After the last cycle and once `IsEverySlotFree`: no entity of any cell exists, no asset handle of any cell scene or dependency is held (registry reference counts 0), no interned-string reference remains, no presentation handle remains, the governor holds no reservation, the heap's live allocation count equals its pre-test value, and `stream.cells.released` equals the number of loads issued ([Z2-N-CONTRACT-339]). Each stage's hit count (cycles whose cancellation ASSET's `asset.loads.cancelled.<stage>` or SCENE's `scene.prepare.cancelled.<state>` attributed to that stage) is printed and asserted above 0 |
| `Test_StreamingDeterminism_ReplayAcrossWorkers` | a recorded 3,000-tick traversal with teleports, persistence, references, rebases and forced memory pressure replays at 1, 2, 8 and all workers, with forced compaction and two chunk capacities, on each ECS backend: per-tick world hashes equal the recording's ([Z2-N-CONTRACT-356]) |
| `Test_StreamingDeterminism_CancelledLoadsReplay` | the 1,000-cycle cancellation stress run recorded, then replayed at 1 and all workers: per-tick hashes equal, every discarded publication is published at its recorded tick, and no replay tick is held by a cancelled preparation |
| `Test_StreamingDeterminism_DiscardResolvedAtPublication` | SCENE's preparation hold (row ST2 (h)) holds a cell's preparation at `BUILDING`; the test releases it after the boundary of tick t − 1 and waits until the operation has parked its success, then runs tick t, whose update records the cell's unload: the boundary after tick t logs exactly one publication of that completion ID, `PUBLICATION_KIND_GENERIC_RECORDED` `{instance, SCENE_ERROR_CANCELLED, 0}`, the cell reaches UNLOADED by T18, and the run's replay holds no tick for it and reproduces every hash; the variants releasing the hold after tick t's sync point and never releasing it (cancelled at the hold) log the identical publication |
| `Test_StreamingDeterminism_ReplayReleasesLateLocalDiscard` | the recording of the previous test replayed with the replay's own preparation of that cell held until 10 ticks after the recorded discard's tick: the recorded discard publishes at its tick, T18 runs there, the replay's late completion is dropped (`Complete` reports the ID already published, the sink does not assert), the operation's prepared scene is released, the slot becomes FREE, and every per-tick hash equals the recording's |
| `Test_StreamingDeterminism_MemoryPressureDoesNotChangeTicks` | forced WARNING and CRITICAL change no world hash |
| `Test_StreamingDeterminism_IODelayDoesNotChangeTicks` | ASSET's fixture delaying every read by 0 to 30 frames (seeded) changes only publication ticks, and a replay of each run reproduces it |
| `Test_StreamingStress_RapidTraversal` | a source at 300 m/s for 10,000 ticks: no cap is exceeded, no cell is stuck in a transitional state longer than 600 ticks, and `residency_miss_ticks` is reported |

## Render tests

`Zenith_Integration_StreamingRender` (`Zenith2/Tests/StreamingRender/`, `SUPPORTED ZENITH_WINDOWS`) creates a device on WARP and on lavapipe directly (`Zenith2/Docs/Design/RHI.md` → "Headless CI devices"; a GPU test executable is not a headless run), with the renderer and GEOM's real upload path; timings there are never budget evidence:

| Test | Asserts |
|---|---|
| `Test_Streaming_CancelReload1000Cycles_GPUUploads` | 1,000 seeded cycles over cells whose content holds meshes and textures, each cancelling between a dependency's upload submission and its completion (ASSET's `DELIVER`/`ACTIVATING`, held by ASSET's stage hold, row ST20): afterwards `Flux.Lifetime.PendingRetirements` returns to 0 within the frames in flight, `Flux.Geometry.PoolBytesUsed` and `Flux.Textures.ResidentBytes` return to their pre-test values, `RHI.Resources.LiveTextures` and `RHI.Resources.LiveBuffers` return to their pre-test values, and every residue assertion of `Test_Streaming_CancelReload1000Cycles_NoLeak` holds; the GPU-upload stage's hit count is asserted above 0 |
| `Test_StreamingService_GateRulesRestoredLoadRenderer` | with a renderer on the device, a load adopted by the rebuild after a restore (a save written through `Zenith_SaveSystem`, restored into a fresh world) gets its group requested in the first epilogue after R4 (its stored frame was 0), and its timeout counts from that frame |
| `Test_StreamingRender_PSOGateOpensOnGroupCompletion` | a cell's gate opens when its precached group completes on the device, and its activation creates no PSO on the frame thread |

## Fuzz target

`Zenith_Fuzz_StreamingWorld` (`Tests/Fuzz/StreamingWorld.cpp`, `win-asan`) feeds arbitrary bytes as the eight decoded sections to the reader of `Zenith_StreamingWorldKindLoader` (through its internal parse function, bypassing ASSET's container) and, separately, arbitrary bytes to `Zenith_DecodeStreamingCellDelta`. It asserts no crash, no sanitizer report and no allocation above the limits' sizes. Its corpus seeds are generated at test time from `Zenith_StreamingTestWorld` (no committed binary); the 60-second smoke runs under `Sanitizers -Preset win-asan`.

## Death and fault-injection tests

| Test | Asserts |
|---|---|
| `Test_StreamingModule_StartFaultPoints` | each Start fault point (`0x53570001` … `0x53570006`, in step order) fails Start with the stated error and leaves no registration or allocation of any earlier step |
| `Test_StreamingModule_AttachFaultPoints` | each AttachWorld fault point (`0x53570010` … `0x53570015`, in step order) fails the attach and unwinds every earlier step |
| `Test_StreamingWorldFormat_LoaderOutOfMemory` | heap fault injection at the loader's block fails the load with `STREAMING_ERROR_OUT_OF_MEMORY` and frees nothing twice |
| `Test_StreamingBinding_ArraysOutOfMemory` | heap fault injection at the binding's resize gives FAILED with `STREAMING_ERROR_OUT_OF_MEMORY` and no partial arrays |
| `Test_StreamingSnapshotPool_DestroyWithHeldSlotChecks` | destroying a pool with a held slot fires `ZENITH_CHECK` (a death test) |
| `Test_StreamingModule_RetiringListFullChecks` | a seventeenth pool retiring at once fires `ZENITH_CHECK` in DetachWorld (a death test) |
| `Test_StreamingCommands_MalformedPayloadsIgnored` | each of the four command kinds with a wrong-size or out-of-range payload is counted in `stream.commands.malformed` and changes nothing |

## Fixtures and generated content

- `Zenith_StreamingTestWorld` (`Tests/Support/`, header-only `inline` helpers, [Z2-N-ARCH-255]; the X184 rule that non-Core `Tests/Support/` is header-only) builds, from a seed and a small description (grids, cell counts, entities per cell, persistent fraction, reference count, HLOD levels, a re-cook variant inserting or removing cells): the cell scenes through SCENE's cooked-scene writer, the HLOD meshes through GEOM's mesh writer, the `.zstream` through `Zenith_WriteStreamingWorldFile`, all into memory, installed in an in-memory catalogue through ASSET's header-only `Zenith_MemoryMountSource.h` and `Zenith_AssetGatedMountSource.h`. No binary file is committed (every binary fixture is generated at test time).
- Golden files: `Tests/Golden/stream-dump-unbound.json` and `-bound.json`, written by hand from Formats.md "Stream dump format" for the two fixture worlds the dump test builds, and edited by hand whenever that format changes; the delta golden bytes live in the test source.
- Cook fixtures are JSON text under `Zenith2/Tools/ZenithCook/Tests/Streaming/Documents/`, one per validation message (`V1.streamworld.json` … `V24.streamworld.json`) plus the valid example; their cell scenes are SCENE documents generated by the test into the cook's scratch source tree.
- The integration suites use the same builder for a 16 × 16 two-grid world (256 collision cells of 256 m, 1,024 gameplay cells of 128 m) and a 60 km strip world for rebasing.
- The shedding row appended to `axMEMORY_SHEDDING_CASES` names `MEMORY_SUBSYSTEM_STREAMING`, the counters `stream.shed.prefetch_cancelled`, `stream.shed.hlod_evicted` and `stream.shed.cells_evicted`, and the expectation that the world hash is unchanged (Z2-R-CORE-152).

## Assumed test support of other areas

<!-- assumption: pending ASSET (row ST20), SCENE (row ST2 (h)) -->
The cancellation stress holds a request at a named stage and checks which stage each cancellation hit through two things the owning areas do not yet declare:

- **ASSET** (row ST20): a header-only `Zenith2/Engine/ZenithAssets/Tests/Support/Zenith_AssetStageHold.h` that, over a test-access seam ASSET declares in its loader, parks one asset's pipeline at the start of `SECTIONS` `DECODING` or of `DELIVER`/`ACTIVATING` until released (each re-poll a cancellation point) and reports that it is parked; and the counters `asset.loads.cancelled.<stage>` (`QUEUED`, `HEADER_READ`, `READING`, `DECODING`, `DELIVER`, `ACTIVATING`) beside `asset.loads.cancelled`. I/O holds already exist (`Zenith_AssetGatedMountSource.h`).
- **SCENE** (row ST2 (h)): a header-only `Zenith2/Engine/ZenithScene/Tests/Support/Zenith_ScenePreparationHold.h` that parks a named instance's preparation at the start of `DECODING`, `DEPENDENCIES` or `BUILDING` until released, and the counters `scene.prepare.cancelled.<state>`. SCENE's own tests of the hook contract, which the ST2 patch registers, are `Test_AdditiveScenes_HookCallPoints`, `Test_AdditiveScenes_DiscardResolverRecordsCancelled` and `Test_AdditiveScenes_AssetErrorDetailPublished`; STREAM's integration tests above drive the same real hook end to end.

Until they land, the stress's I/O, `GATED`, QUEUED, ACTIVATING and ACTIVE stages run, and the held stages are the ticket's `blocks` links on those rows.
<!-- /assumption -->

## Gate steps per test group

| Group | Target | Gate step | Presets |
|---|---|---|---|
| unit, format, fault injection, diagnostics | `ZenithStreaming_Tests` | `Test` | `win-debug`, `win-release` |
| sanitizer, concurrency | `ZenithStreaming_Tests` | `Sanitizers -Preset win-asan` | `win-asan` |
| cook | `ZenithCook_Tests` | `Test` | `win-debug` |
| integration, stress, determinism | `Zenith_Integration_Streaming` | `Test` | `win-debug`; the 1,000-cycle stress and the determinism suite in `win-release` |
| render | `Zenith_Integration_StreamingRender` | `Test` | `win-release`, on WARP and lavapipe |
| fuzz smoke | `Zenith_Fuzz_StreamingWorld` | `Sanitizers -Preset win-asan` | `win-asan` |
| shedding row | the memory suite | `Test` | `win-debug` |
| layout and contracts | — | `ApiContractCheck`, `HeaderSelfContained`, `Libgraph`, `ForbiddenCheck`, `Complexity` | per the diff |
