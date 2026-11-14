# SAVE requirement catalogue: Stages 4 and 6

## Purpose

The second file of the SAVE catalogue: the requirements whose `Stage` is S4 or S6, in the same format and the same `Z2-R-SAVE-NNN` numbering as `Zenith2/Docs/Requirements/SAVE.md`, which holds the purpose of the whole catalogue, the requirements of Stages 1 to 3, the coverage of inventory items (over both files) and the exclusions.

## Requirements

### Z2-R-SAVE-055 Snapshot CPU within its row

- **Statement:** The `Save.Snapshot` zone on frames that save stays within Z2-B-SAVE-002 in the W-OpenWorld runs on Windows and the S24.
- **Covers:** Z2-I-SAVE-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-SAVE-002
- **Acceptance evidence:** Workload runs per `Zenith2/Docs/Budgets.md` → "Frame-rate definitions and measurement procedure" (MP-CPUZONE) for the Stage 4 flagship milestone; `Zenith_Bench_Save` reports.
- **Design:** Zenith2/Docs/Design/SAVE.md → "Rows SAVE owns"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Phase-11 snapshot"; Zenith2/Docs/Design/SAVE/Tests.md → "Benchmarks and budget harness"

### Z2-R-SAVE-092 Shipped-build save fixtures

- **Statement:** At each shipped release, a recording ticket commits the release build's own save files for every fixture state, and every committed shipped file loads, migrates and restores to its expected dump.
- **Covers:** Z2-I-SAVE-08, Z2-I-SAVE-14
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** The `needs-human` recording ticket's committed files under `Tests/Fixtures/Migration/Save/Shipped/`; `Test_SaveMigration_ShippedFilesLoad` (`Test` step; Stage 6 acceptance).
- **Design:** Zenith2/Docs/Design/SAVE/Formats.md → "Frozen fixture writers"; Zenith2/Docs/Design/SAVE/Tests.md → "Migration and fixture tests"

### Z2-R-SAVE-098 Profile switch isolation

- **Statement:** `SetActiveProfile` refuses while work is queued and in replay mode, clears retained checkpoints and slot tables, recovers the new profile's journals, writes the last-profile record, and no save of the previous profile is listed, loaded or retained afterwards.
- **Covers:** Z2-I-SAVE-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveSystem_ProfileSwitchIsolation` (`Test` step); ONLINE's account-change integration case.
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Profile switching"; Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_SaveSystem header"

### Z2-R-SAVE-113 Resuming a replay from a save

- **Statement:** Resuming a replay from a save restores the save under the publication source and continues from the tick after the save's replay position: from an EXACT save every tick hash is compared and matches; from a COHERENT save no hash is compared and the run reports `m_bHashesCompared = false`, counts the unverified ticks and logs why.
- **Covers:** Z2-I-SAVE-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveRuntime_ReplayFromSaveSeeksPosition` (both cases) (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Resuming a replay from a save"; Zenith2/Docs/Design/SAVE/Formats.md → "File header"; Zenith2/Docs/Design/SAVE.md → "Reproducibility boundaries"

### Z2-R-SAVE-114 Replay checkpoints at the configured interval

- **Statement:** With `replay.checkpointIntervalTicks` above 0, phase 11 of every multiple of the interval appends a one-world CHECKPOINT record after that tick's record, allocating no completion ID and changing no world state, and the header flag is set.
- **Covers:** Z2-I-SAVE-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_ReplayLog_CheckpointIndex`, `Test_SaveRuntime_RecordingDoesNotChangeHashes` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Formats.md → "Records SAVE writes"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Replay recording"

### Z2-R-SAVE-115 Restored worlds in the replay log

- **Statement:** A world created from a save is logged at its swap as `WORLD_BEGIN` followed by a CHECKPOINT holding a fresh one-world `SAVE_KIND_REPLAY_CHECKPOINT` capture of the restored world (codec NONE, sequence 0, UTC 0), never the source file, and a replay restores that checkpoint and starts at the following tick.
- **Covers:** Z2-I-SAVE-13, Z2-I-SAVE-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_ReplayLog_RestoredSessionConvention`, `Test_SaveRuntime_RestoredWorldCheckpointIsFreshCapture` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Formats.md → "Restored-world convention"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Replay playback"

### Z2-R-SAVE-116 Seeking through a checkpoint equals a full replay

- **Statement:** Seeking a replay to tick T through the latest EXACT checkpoint at or before T gives the same world hash as replaying from the session start, and runs fewer than the checkpoint interval of ticks after the restore.
- **Covers:** Z2-I-SAVE-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_ReplaySeek_CheckpointMatchesFullReplay` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Replay seeking"; Zenith2/Docs/Design/SAVE/RuntimeBridge.md → "Runtime bridge declarations"

### Z2-R-SAVE-117 Seeking falls back for coherent checkpoints

- **Statement:** A seek finds only EXACT checkpoints: when every checkpoint at or before the target carries `COHERENT_ONLY` (a behaviour suspended mid-`Run`, a quarantined instance or one of a disabled or disabling module, a loading additive instance, or a COHERENT backend at the checkpoint tick), it replays from the session start and reports the FULL strategy.
- **Covers:** Z2-I-SAVE-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_ReplaySeek_CoherentCheckpointsFallBackToFull` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Replay seeking"; Zenith2/Docs/Design/SAVE.md → "Reproducibility boundaries"

### Z2-R-SAVE-118 In-flight work re-issued at a seek

- **Statement:** An asynchronous request in flight at an EXACT checkpoint tick is re-issued with its completion ID and publishes at its recorded tick after the seek.
- **Covers:** Z2-I-SAVE-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_ReplaySeek_InFlightAsyncReissued` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Replay seeking"; Zenith2/Docs/Design/SAVE/Formats.md → "Async section"

### Z2-R-SAVE-119 Replay readers accept every format version of the release

- **Statement:** The replay reader of a release reads every `ZRPL` format version written by builds of the same major release.
- **Covers:** Z2-I-SAVE-14
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_ReplayCompat_ReadsEveryFormatVersion` over logs generated at test time by each frozen replay writer (`Test` step; Stage 6 acceptance).
- **Design:** Zenith2/Docs/Design/SAVE/ReplayAPI.md → "Zenith_ReplayLog header"; Zenith2/Docs/Design/SAVE/Formats.md → "Replay limits"

### Z2-R-SAVE-120 Checkpoints of older logs load as saves

- **Statement:** A CHECKPOINT image inside a log of any build of the current major release opens with `PrepareFromImage`, migrates and restores, even though the log itself is refused for replay by build.
- **Covers:** Z2-I-SAVE-14
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_ReplayCompat_OldCheckpointLoadsAsSave` (`Test` step; Stage 6 acceptance).
- **Design:** Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_SaveSystem header"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Migration pipeline"

### Z2-R-SAVE-121 Replay compatibility across a format bump

- **Statement:** A replay format bump keeps the previous version's writer frozen under `Zenith2/Engine/ZenithSave/Tests/Frozen/Replay_V<n>/` (a compiled test source, never `Tests/Support/`) and adds the reader path and the compatibility tests in the same diff.
- **Covers:** Z2-I-SAVE-14
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_ReplayCompat_ReadsEveryFormatVersion` (fails when a bump lands without its frozen writer) (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Formats.md → "Frozen fixture writers"; Zenith2/Docs/Design/SAVE/Tests.md → "Replay tests"

### Z2-R-SAVE-122 Persistence components saved like every component

- **Statement:** The persistent state of unloaded content, held in components (STREAM's `Zenith.Streaming.CellDelta` container and `Zenith.Streaming.Persistent`, and game components), is saved and restored bit-exactly by the ENTITIES section with no SAVE-specific store.
- **Covers:** Z2-I-SAVE-03, Z2-I-SAVE-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_Snapshot_RoundTripAllPropertyTypes` (containers) (`Test` step); `Test_SaveCrossSystem_UnloadSaveReloadEquivalent`.
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Persistent world and entity state"; Zenith2/Docs/Design/SAVE/Formats.md → "Entities section"

### Z2-R-SAVE-123 Loading instances re-requested with their hook tag

- **Statement:** An additive instance that was LOADING or QUEUED at the snapshot is re-requested at restore with its saved scene, hook tag (stored opaque pending SCENE's `m_ulHookTag`), duplicate flag and unknown-type policy, and an instance that was ACTIVE is restored from ENTITIES and never reloaded.
- **Covers:** Z2-I-SAVE-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveCrossSystem_LoadingInstanceReRequestedWithHookTag` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Formats.md → "Participant section framing"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Persistent world and entity state"

### Z2-R-SAVE-124 World-level state saved through resources and module participants

- **Statement:** World-level simulation state is saved either as a reflected world resource (the environment clock and weather, the simulation input contexts, the world host's continuity base) in RESOURCES or, when it lives outside reflection (physics bodies, animation graph state, navigation and in-flight path requests, cinematic playback), by its module's participant section; STREAM registers no participant.
- **Covers:** Z2-I-SAVE-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_Snapshot_WorldResourcesRoundTrip` (`Test` step); each module's participant test (PHYS, ANIM, AI, CINE) and `Test_SaveCrossSystem_SaveDuringStreamingWithAnimationAndPhysics`.
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Persistent world and entity state"; Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_SaveParticipant header"

### Z2-R-SAVE-125 Unload save load and reload reproduce state

- **Statement:** Modifying persistent entities of a cell, unloading it, saving, loading into a fresh world and reloading the cell gives those entities their modified values and every other entity its authored values.
- **Covers:** Z2-I-SAVE-03, Z2-I-SAVE-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveCrossSystem_UnloadSaveReloadEquivalent` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Persistent world and entity state"; Zenith2/Docs/Design/SAVE/Tests.md → "Cross-system failure tests"

### Z2-R-SAVE-126 Destroyed persistent entities stay destroyed

- **Statement:** A persistent authored entity destroyed before its cell unloaded is not spawned again after a save, a load and the cell's reload.
- **Covers:** Z2-I-SAVE-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveCrossSystem_DestroyedPersistentStaysDestroyed` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Persistent world and entity state"; Zenith2/Docs/Design/SAVE/Tests.md → "Cross-system failure tests"

### Z2-R-SAVE-127 Persistent gameplay state of the open-world slice

- **Statement:** OpenWorldSlice's persistent-world saves (quest, dialogue and inventory state in components and behaviour state, changed and destroyed world entities in cell deltas) reload with every recorded change after travelling across at least three cells.
- **Covers:** Z2-I-SAVE-03, Z2-I-SAVE-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-021, Z2-D-077
- **Budgets:** none
- **Acceptance evidence:** The OpenWorldSlice Stage 4 playthrough segment that saves, quits, relaunches and loads (Stage 4 acceptance, `Run-Committed.ps1 -Acceptance -Stage 4`).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Persistent world and entity state"; Zenith2/Docs/Design/SAVE.md → "Stage 4"

### Z2-R-SAVE-128 Inconsistent owners excluded from snapshots

- **Statement:** Entities of an additive instance whose activation or unload is incomplete at the snapshot point, and their descendants (however their simulation IDs are ordered), are excluded by the additive-set participant; activating instances are re-requested at restore and unloading ones are not recorded.
- **Covers:** Z2-I-SAVE-12, Z2-I-SAVE-18
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Snapshot_ExclusionsDropSubtrees` (`Test` step); `Test_SaveCrossSystem_SaveDuringStreamingWithAnimationAndPhysics`.
- **Design:** Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_SaveParticipant header"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Phase-11 snapshot"

### Z2-R-SAVE-129 Phase-11 snapshots consistent while jobs run

- **Statement:** A snapshot taken in phase 11 while physics sub-jobs and asynchronous requests are active equals the out-of-tick snapshot of the same state.
- **Covers:** Z2-I-SAVE-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveCrossSystem_ConsistentSnapshotWhileJobsActive` (`Test` step, `Zenith_Integration_SaveCrossSystem`).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Phase-11 snapshot"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Out-of-tick snapshot"

### Z2-R-SAVE-130 Saving during streaming with animation and physics

- **Statement:** A save requested while cells load, activate and unload, with animated characters and simulated bodies, contains every resident cell's entities, no entity of an incomplete instance, the streaming components and the physics and animation participants' sections, and carries `COHERENT_ONLY` because an instance is loading, for each physics backend.
- **Covers:** Z2-I-SAVE-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-003
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveCrossSystem_SaveDuringStreamingWithAnimationAndPhysics` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Tests.md → "Cross-system failure tests"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Phase-11 snapshot"

### Z2-R-SAVE-131 An interrupted write during that save

- **Statement:** A power loss at each journal step of that save leaves, after recovery, a loadable world equal to either the previous save or the new one.
- **Covers:** Z2-I-SAVE-17
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveCrossSystem_InterruptedWriteThenRestoreConsistent` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Tests.md → "Cross-system failure tests"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Journal recovery"

### Z2-R-SAVE-132 No half-streamed cells after a restore

- **Statement:** A restored world contains no entity of an instance that was incomplete at the snapshot, and such instances are loaded again through the additive-set participant's re-request.
- **Covers:** Z2-I-SAVE-18
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveCrossSystem_SaveDuringStreamingWithAnimationAndPhysics`, `Test_SaveCrossSystem_RestoreHasNoDuplicatedOrLostEntities` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Tests.md → "Cross-system failure tests"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Restore pipeline"

### Z2-R-SAVE-133 No duplicated or lost entities

- **Statement:** After a restore and the re-streaming of excluded cells, the world's entity set equals the saved set plus each re-streamed entity exactly once.
- **Covers:** Z2-I-SAVE-18
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveCrossSystem_RestoreHasNoDuplicatedOrLostEntities` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Tests.md → "Cross-system failure tests"

### Z2-R-SAVE-134 Physics and animation coherent after a restore

- **Statement:** After a restore every body's transform equals its entity's and every character's pose matches its animation state; the continuation's tick hashes are claimed only for an EXACT capture (no suspended `Run`, no quarantined or disabled instance, no loading instance, an EXACT physics backend), and are checked against the original run's recording: the run records with `--record`, saves out of tick between ticks 300 and 301, and a replay resumed from the save reproduces the recorded hashes of ticks 301 … 600 (never two live runs, whose streaming, animation and path results publish at I/O-dependent ticks); every other capture carries `COHERENT_ONLY` and is held to the coherence assertions.
- **Covers:** Z2-I-SAVE-18
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-003, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveCrossSystem_PhysicsAndAnimationCoherent`, per physics backend, its EXACT variant through `Zenith_RuntimeReplay_ResumeFromSave` asserting `m_bHashesCompared` and `m_bVerified` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Tests.md → "Cross-system failure tests"; Zenith2/Docs/Design/SAVE.md → "Reproducibility boundaries"

### Z2-R-SAVE-135 A thousand save and load cycles leave nothing behind

- **Statement:** 1,000 cycles of save, load and 10 ticks leave no entity, asset reference, operation slot or heap allocation behind.
- **Covers:** Z2-I-SAVE-18, Z2-I-SAVE-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveCrossSystem_RepeatedSaveLoad1000` (`Test` step, Debug leak table).
- **Design:** Zenith2/Docs/Design/SAVE/Tests.md → "Cross-system failure tests"

### Z2-R-SAVE-138 Android backup rules

- **Statement:** The Android package excludes the resume file, journals, temporaries and quarantined saves from device backup and transfer and includes the saves and their backups.
- **Covers:** Z2-I-SAVE-05
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027, Z2-D-010
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveDevice_S24_BackupRulesExcludeTransient` device run record (`needs-human`; Stage 6 acceptance).
- **Design:** Zenith2/Docs/Design/SAVE/Formats.md → "File names and directories"; Zenith2/Docs/Design/SAVE/Tests.md → "Platform storage tests"

### Z2-R-SAVE-139 Saves survive an installed update

- **Statement:** Saves written by release N load after a Play installed update to release N + 1 of the same major, migrated, and release N's resume state is discarded.
- **Covers:** Z2-I-SAVE-08, Z2-I-SAVE-14
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveDevice_InstalledUpdateMigration` device run record with PACK's installed-update test (`needs-human`; Stage 6 acceptance).
- **Design:** Zenith2/Docs/Design/SAVE/Tests.md → "Platform storage tests"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Migration pipeline"

### Z2-R-SAVE-141 Fuzzing for the full duration before shipping

- **Statement:** Each SAVE fuzz target runs its full duration without a finding in the Stage 6 security hardening evidence.
- **Covers:** Z2-I-SAVE-06
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** The Stage 6 acceptance fuzz run record ([Z2-N-ARCH-960]).
- **Design:** Zenith2/Docs/Design/SAVE/Tests.md → "Fuzz targets"

### Z2-R-SAVE-146 Save compatibility policy demonstrated

- **Statement:** Stage 6 shipping qualification demonstrates forward save migration from every shipped save version of the current major release with the shipped-build fixtures.
- **Covers:** Z2-I-SAVE-08, Z2-I-SAVE-14
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveMigration_ShippedFilesLoad` and `Test_SaveMigration_FixturePerVersion` in the Stage 6 acceptance run ([Z2-N-CONTRACT-551]).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Migration pipeline"; Zenith2/Docs/Design/SAVE.md → "Stage 6"

### Z2-R-SAVE-150 Recording changes no simulation state

- **Statement:** Replay checkpoints, out-of-tick submissions, the resume write and the exit autosave allocate no completion ID and no serial and change no world state, so a run with `--record` and checkpoints has the tick hashes and SAVE completion IDs of the same run without them whenever both runs' SAVE results publish at the same ticks (gated storage), and the recorded run replays with every hash verified.
- **Covers:** Z2-I-SAVE-11, Z2-I-SAVE-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveRuntime_RecordingDoesNotChangeHashes` (gated storage, then a replay of the recording), `Test_SaveSystem_OutOfTickAllocatesNoCompletionID` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Replay recording"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Request intake"; Zenith2/Docs/Design/SAVE.md → "Determinism obligations"

### Z2-R-SAVE-157 Script loads hand off with two completion IDs

- **Statement:** A script `Save.Load` allocates its `ASYNC_SOURCE_SAVE` and its linked `ASYNC_SOURCE_WORLD_LOAD` completion IDs at intake in merge order; the first always publishes `Zenith.SaveCompleted` with `Operation` 4, and the second is owned by GAME's transition after the hand-off or completed by SAVE with `WORLD_TRANSITION_FAILURE_PREPARE` whenever the load failed, was refused at intake or was cancelled; when the linked ID cannot be allocated the load completes `SAVE_ERROR_TRANSITION_BUSY` and nothing is queued; in replay mode the restored world comes from the log's restored session.
- **Covers:** Z2-I-SAVE-01, Z2-I-SAVE-18
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveRuntime_LoadTransitionsWorld`, `Test_SaveRuntime_LoadFailureStaysInWorld` (`Test` step, `Zenith_Integration_SaveRuntime`).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Script loads and the world host"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Load pipeline"; Zenith2/Docs/Design/SAVE/PublicAPI.md → "Script-facing API"

### Z2-R-SAVE-158 Replay reader lookahead for world transitions

- **Statement:** `Zenith_ReplayLogReader` answers `FindNextPublication` (the first publication of a source kind in a world's session after a given tick), `GetSessionAfterSwap` and `GetRestoredCheckpoint` without a cursor, returning false or null for anything the log lacks.
- **Covers:** Z2-I-SAVE-13, Z2-I-SAVE-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_ReplayLog_FindNextPublication`, `Test_ReplayLog_SessionAfterSwap` (`Test` step).
- **Design:** Zenith2/Docs/Design/SAVE/ReplayAPI.md → "Zenith_ReplayLog header"

### Z2-R-SAVE-163 Scenario save test runs under the fidelity rule

- **Statement:** Architecture's `Test_Scenario_SaveAtSnapshotDuringStreaming` asserts tick-hash reproduction only for an EXACT capture, and as a replay resumed from the save reproducing the original run's recorded tick hashes (the run records with `--record`, the phase-10 save carries its replay position, and `Zenith_RuntimeReplay_ResumeFromSave` compares every tick; no delivery rule or write gate is needed), and SAVE's coherence assertions for a COHERENT capture (a loading cell, a behaviour suspended mid-`Run`, or a quarantined or disabled instance).
- **Covers:** Z2-I-SAVE-16, Z2-I-SAVE-18
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** GAME's `Test_Scenario_SaveAtSnapshotDuringStreaming` (`Test` step), reconciled through cross-doc item X205; `Test_SaveCrossSystem_PhysicsAndAnimationCoherent`.
- **Design:** Zenith2/Docs/Design/SAVE.md → "Scenario tests that consume SAVE"; Zenith2/Docs/Design/SAVE.md → "Reproducibility boundaries"

### Z2-R-SAVE-167 Loading additive instances re-issued under recorded IDs

- **Statement:** A LOADING or QUEUED additive instance is saved with its in-flight `ASYNC_SOURCE_SCENE_LOAD` completion ID and re-requested at restore under that ID without allocating a new one, so a replay of a restored session publishes the recorded load result to the very request the restored set waits for.
- **Covers:** Z2-I-SAVE-12, Z2-I-SAVE-13, Z2-I-SAVE-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveRuntime_AdditiveLoadReissuedUnderRecordedID` (`Test` step, `Zenith_Integration_SaveRuntime`).
- **Design:** Zenith2/Docs/Design/SAVE/Formats.md → "Participant section framing"; Zenith2/Docs/Design/SAVE/Formats.md → "Async section"; Zenith2/Docs/Design/SAVE/RuntimeBridge.md → "Runtime bridge declarations"

### Z2-R-SAVE-168 Replay publication lookup by completion ID

- **Statement:** `Zenith_ReplayLogReader::FindPublication(uWorldTag, uSourceKind, ulCompletionID, xOut)` returns the recorded publication of that completion ID as a 32-byte `Zenith_ReplayPublicationPeek` (tick, completion ID, publication kind, the first 8 payload bytes zero-padded) through a per-(session, source kind) index built at `Open`, and false when the log holds none.
- **Covers:** Z2-I-SAVE-15, Z2-I-SAVE-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_ReplayLog_FindPublicationByCompletionID` (`Test` step, `ZenithSave_Tests`).
- **Design:** Zenith2/Docs/Design/SAVE/ReplayAPI.md → "Zenith_ReplayLog header"

### Z2-R-SAVE-169 Refused script loads release the world load slot

- **Statement:** Every refusal or cancellation of a script `Save.Load` before its load runs completes both its `ASYNC_SOURCE_SAVE` ID (with the error) and its linked `ASYNC_SOURCE_WORLD_LOAD` ID (with GAME's `WORLD_TRANSITION_FAILURE_PREPARE` through `CompleteLinkedLoadRefused`), and a `Save.Load` whose linked ID cannot be allocated completes `SAVE_ERROR_TRANSITION_BUSY` and queues nothing, so a subsequent transition of the world is accepted.
- **Covers:** Z2-I-SAVE-01, Z2-I-SAVE-18
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_SaveRuntime_LoadFailureStaysInWorld` (refused, cancelled and busy cases) (`Test` step, `Zenith_Integration_SaveRuntime`).
- **Design:** Zenith2/Docs/Design/SAVE/Algorithms.md → "Script loads and the world host"; Zenith2/Docs/Design/SAVE/Algorithms.md → "Request intake"; Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_SaveSystem header"
