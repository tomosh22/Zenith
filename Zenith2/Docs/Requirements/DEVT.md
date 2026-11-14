# DEVT requirement catalogue

The requirement catalogue of the DEVT area (Developer tools & quality). Every requirement is one testable obligation with the inventory items it covers, its stage, classification, governing decisions, budget rows, acceptance evidence and the design headings that specify it. The design is `Zenith2/Docs/Design/DEVT.md` and its split files under `Zenith2/Docs/Design/DEVT/`.

## Purpose

DEVT owns `ZenithProfiling` (the profiler-hook implementation, rings, lanes, frames, GPU ingestion, captures, Tracy forwarding), the on-screen profiler in game and editor on Windows and the S24 (Z2-D-005), the development console and file writer, the debug-draw recorder and the `DebugOverlay` pass, structured-log tooling, remote diagnostics, `Zenith_CrashHandler` and the D-SEL-CRASH (open, Stage 2) selection, the evidence-fingerprint specification and the budget-run evidence path (Z2-D-012, Z2-D-065), run telemetry and dashboards, the `linux-tsan` WSL host (Z2-D-011), fuzzing conventions, static analysis and `ZenithDevTool`. Every item `Z2-I-DEVT-01` … `Z2-I-DEVT-26` is covered below; the `linux-tsan` host requirements also cover `Z2-I-CORE-06`, whose sanitizer runs need the host.

Evidence phrases: "the `Zenith2Memory` lines" are the category's `Test -Label memory` line and its `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan` lines; "`needs-human` step" is a step of a stage acceptance ticket run by a person; every named test is specified in `Zenith2/Docs/Design/DEVT/Tests.md`. No requirement asks a worker to run a build or a test: each named test is run by the gate step given.

## Requirements

### Z2-R-DEVT-001 ZenithProfiling library target

- **Statement:** ZenithProfiling is one static library at `Zenith2/Engine/ZenithProfiling/` whose CMakeLists.txt is exactly `zenith_add_library(ZenithProfiling DEPENDS ZenithCore THIRD_PARTY tracy TEST_LABELS memory)` with the default `SUPPORTED ZENITH_ALL` matrix, and it includes no header of ZenithPlatform, ZenithJobs, ZenithFlux or any library above layer 1.
- **Covers:** Z2-I-DEVT-01, Z2-I-DEVT-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005, Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** the `Configure`, `Build`, `Libgraph` (no used-but-undeclared edge, no cycle) and `CompileCoverage` steps on every preset including linux-tsan and android-arm64
- **Design:** Zenith2/Docs/Design/DEVT.md → "ZenithProfiling target"; Zenith2/Docs/Design/DEVT.md → "Libraries DEVT code lives in"

### Z2-R-DEVT-002 API-only headers and forward-declarable types

- **Statement:** The headers `Zenith_ProfilerTypes.h`, `Zenith_Profiler.h`, `Zenith_ProfileZone.h`, `Zenith_DevConsole.h`, `Zenith_DevFileWriter.h` and `Zenith_DebugDraw.h` land as an API-only slice in the Stage 1 sprint (fix version `Z2 Stage 2`), each self-contained, with one contract block per public type, no function declared `noexcept` or `[[nodiscard]]`, and `class Zenith_Profiler;` sufficient for a consumer that only stores a pointer.
- **Covers:** Z2-I-DEVT-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045, Z2-D-047
- **Budgets:** none
- **Acceptance evidence:** the `HeaderSelfContained`, `ApiContractCheck` and `Build` steps on the API-only slice
- **Design:** Zenith2/Docs/Design/DEVT/Profiler.md → "Profiler public headers"; Zenith2/Docs/Design/DEVT.md → "ZenithProfiling public headers"

### Z2-R-DEVT-003 Hook table filling

- **Statement:** `Zenith_Profiler::FillHooks` writes the six profiler members of `Zenith_ProfilerHooks` and `m_pUserData` (in a Shipping build the lock pair and the blocking-wait member stay null), leaves the three log members untouched, and the Player installs the table it filled at startup step 2 together with the log router's members.
- **Covers:** Z2-I-DEVT-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_Profiler_FillHooks_WritesProfilerMembersOnly` in `ZenithProfiling_Tests` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Profiler.md → "Hook implementation"

### Z2-R-DEVT-004 Zone names copied at the call

- **Statement:** The zone-begin hook copies `Zenith_ZoneSite::m_szName` (at most 63 bytes, longer names cut and ending in `~`) into the profiler's name table at the call and retains no pointer to the caller's text, so a name formatted into a buffer that is reused immediately after the begin is recorded correctly.
- **Covers:** Z2-I-DEVT-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_Profiler_DynamicNameCopiedAtCall` and `Test_Profiler_NameCutAt63Bytes` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Profiler.md → "Hook implementation"; Zenith2/Docs/Design/DEVT/Profiler.md → "Name tables"

### Z2-R-DEVT-005 Zone values

- **Statement:** `Zenith_ProfileZoneValue(uint64)` and `Zenith_ProfileScope::SetValue` attach a value to the calling thread's innermost open zone; the assembled zone keeps the first value with `HAS_VALUE`, sets `MULTIPLE_VALUES` when more are attached, a capture keeps every value, and a value with no open zone changes nothing.
- **Covers:** Z2-I-DEVT-14, Z2-I-DEVT-17
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_Profiler_ZoneValue_FirstKeptMultipleFlagged` and `Test_Profiler_ZoneValueWithoutZoneIgnored` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Profiler.md → "Zenith_ProfileZone header"

### Z2-R-DEVT-006 Per-thread event rings

- **Statement:** Each recording thread appends 16-byte `Zenith_ProfilerEvent` records to its own CORE `Zenith_SpscRing<Zenith_ProfilerEvent>` (ruling Q80; no DEVT ring) of 16,384 records (Android 8,192) over storage `Create` allocates, with the full policy `QUEUE_FULL_POLICY_DROP_NEWEST` and the counter triple `profiler.rings.full`, `.dropped`, `.high_water` published from each ring's statistics.
- **Covers:** Z2-I-DEVT-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_Profiler_RingFull_CountsDroppedTriple` and `Test_Profiler_HookStress` (2, 8 and hardware-concurrency threads) on the `Zenith2Memory` lines; CORE's `Test_SpscRing_*` for the primitive
- **Design:** Zenith2/Docs/Design/DEVT/Profiler.md → "Per-thread event rings"; Zenith2/Docs/Design/DEVT/Profiler.md → "Event records"

### Z2-R-DEVT-007 Hooks lock-free and allocation-free

- **Statement:** No profiler hook takes a mutex, condition variable or semaphore of any kind (profiled or not), allocates, or makes a system call other than the monotonic clock read (and one thread-name query when a thread's state is first claimed), so hooks are callable under any held lock level, inside the OOM handler and inside the log router's sink lock.
- **Covers:** Z2-I-DEVT-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_Profiler_HooksAllocateNothing`, `Test_Profiler_HooksTakeNoMutex` and `Test_Profiler_HookStress` on the `Zenith2Memory` lines, including `Sanitizers -Preset linux-tsan` once X247 (c) adds ZenithProfiling's paths
- **Design:** Zenith2/Docs/Design/DEVT/Profiler.md → "Hook implementation"; Zenith2/Docs/Design/DEVT.md → "Lock-order entries"

### Z2-R-DEVT-008 Thread lanes

- **Statement:** `Zenith_Profiler::RegisterThreadLane(uint32 uLaneIndex, const char* szName)` returns a `Zenith_ProfilerLaneID` (idempotent per index; the null lane, counted and logged once, past 56 CPU lanes); a registered lane binds the thread whose OS name equals `szName` whether it was claimed before or after the registration, index 0 binds the calling thread, so JOBS's flow needs no bind call; `BindCurrentThread` is an optional explicit bind; any other thread gets an automatic lane named from its OS thread name, created by `EndFrame` on the main thread before it drains that thread's events, and once the 56 CPU lanes are taken every thread left without one shares the overflow lane `<other threads>` (lane 64, created by `Create`, counted in `profiler.lanes_refused`); names are compared over at most the platform's thread-name limit (63 bytes on Windows, 15 on Android and Linux); the main thread is the only writer of the lane table, a claim binds a registered lane only by a compare-exchange of the thread's lane from 0, and registration and claim publish the lane table and the thread state with `seq_cst` store-then-load pairs, so a registration racing a claim of the same name always binds and an automatic lane created afterwards never overwrites a registered binding.
- **Covers:** Z2-I-DEVT-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_Profiler_Lanes_RegisterIdempotent`, `…_BindByThreadName`, `…_IndexZeroBindsCaller`, `…_FullReturnsNullAndLogsOnce` (including the overflow-lane case), `…_AutomaticLanesCreatedByAssembly` (`Test` step; `Sanitizers -Preset linux-tsan`); `Test_Profiler_JobsLanesRegisteredAtCreate` (`Zenith_Integration_DevTools`)
- **Design:** Zenith2/Docs/Design/DEVT/Profiler.md → "Thread lanes"

### Z2-R-DEVT-009 Counter registry honours counter kinds

- **Statement:** The counter registry aggregates every counter by its `Zenith_CounterKind`: a CUMULATIVE counter's frame value is the change of the published total since the previous frame, a DELTA counter's frame value is the frame's sum, a GAUGE's is its last value; each keeps a 128-frame history of frame values with minimum, maximum and mean; a second kind for one name is counted and ignored.
- **Covers:** Z2-I-DEVT-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005, Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_Profiler_CounterKinds_Cumulative`, `…_Delta`, `…_Gauge`, `Test_Profiler_CounterKindConflict_Counted`, `Test_Profiler_CounterHistory_MinMaxMean` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Profiler.md → "Counter registry"

### Z2-R-DEVT-010 Frame assembly

- **Statement:** `Zenith_Profiler::EndFrame`, called at frame-loop step 3, drains every ring and assembles the frame's zones per lane ordered by start and depth, carries zones still open into the next frame (emitted with `TRUNCATED_START`), and repairs pairing lost to dropped records with `TRUNCATED_END` so every stored zone is well-formed.
- **Covers:** Z2-I-DEVT-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_Profiler_ZoneBeginEnd_AssembledIntoFrame`, `Test_Profiler_OpenZoneCarriedAcrossFrames`, `Test_Profiler_RingFull_DropNewestAndRepair` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Profiler.md → "Frame assembly"

### Z2-R-DEVT-011 Frame history and hitch detection

- **Statement:** The profiler keeps a ring of the last 600 64-byte frame summaries (frame, CPU simulation, CPU render and GPU times, scale, flags) and flags a frame `HITCH` when its frame time exceeds `devtools.profiler.hitchThresholdMs` (default 33.3 ms).
- **Covers:** Z2-I-DEVT-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_Profiler_FrameSummary_Fields`, `Test_Profiler_HistoryRing_600Frames` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Profiler.md → "Frame history and detailed frames"

### Z2-R-DEVT-012 Detailed frames and freeze

- **Statement:** The zones of the last eight frames (four on Android) are kept as detailed frames, and `Freeze(true)` holds them while the history, counters and statistics keep updating, until `Freeze(false)`.
- **Covers:** Z2-I-DEVT-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_Profiler_DetailedFrames_FreezeHolds`, `Test_ProfilerOverlay_FreezeAndSelect` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Profiler.md → "Frame history and detailed frames"; Zenith2/Docs/Design/DEVT/Overlay.md → "Freeze and frame selection"

### Z2-R-DEVT-013 Per-frame zone statistics

- **Statement:** Each frame's per-site zone statistics (count, inclusive total, maximum; accumulated per site by Z2-R-DEVT-143) are computed in time that depends only on the sites touched, never on the site table's capacity, and serve the overlay's `zone_ms` functions and the budget run's `zoneSum` and `gpuPassSum` rows.
- **Covers:** Z2-I-DEVT-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_Profiler_ZoneStats_CountTotalMax` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Profiler.md → "Zone statistics"

### Z2-R-DEVT-014 GPU timing ingestion

- **Statement:** `Zenith_Profiler::SubmitGpuTimings(const Zenith_ProfilerGpuSubmission&)`, the `Zenith_ProfilerGpuSink` member called by `Flux_PassTimingPublisher::Publish` through the sink the frame lifetime registers, turns each frame's pass timestamps into GPU zones on one lane per queue, aligned to the CPU clock when calibrated and to the render start otherwise, forwards calibrated zones to Tracy when active, and drops (counting) a submission when its eight-frame queue is full.
- **Covers:** Z2-I-DEVT-04, Z2-I-DEVT-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005, Z2-D-055
- **Budgets:** none
- **Acceptance evidence:** `Test_Profiler_Gpu_SubmissionBecomesGpuZones`, `…_UncalibratedAlignedToRenderStart`, `…_QueueFull_DroppedAndCounted` (`Test` step); `Test_ProfilerOverlayRender_GpuViewHasPasses` on WARP and lavapipe
- **Design:** Zenith2/Docs/Design/DEVT/Profiler.md → "GPU timing ingestion"

### Z2-R-DEVT-015 Runtime capture toggle

- **Statement:** CPU zone, value, lock and wait capture is switched at run time between `PROFILER_MODE_CAPTURE` and `PROFILER_MODE_COUNTERS_ONLY` by `devtools.profiler.enabled`, `SetMode` and the console's `profiler.mode`, with no rebuild; counters aggregate in both modes, and GPU timings follow `devtools.profiler.gpuTimings` alone.
- **Covers:** Z2-I-DEVT-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_Profiler_CountersOnlyMode_DropsZonesKeepsCounters`, `Test_Profiler_ModeSwitchMidZone_EndsBalanced` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Profiler.md → "Runtime toggle and modes"

### Z2-R-DEVT-016 Shipping profiler configuration

- **Statement:** In Shipping the profiler starts in counters-only mode from `Zenith_GetPlayerProfilerDesc`, references no Tracy symbol, writes no capture, and the overlay is hidden but can be shown by `devtools.profiler.enabled` and `devtools.profiler.visible` in the user or command-line layer, with no build flag.
- **Covers:** Z2-I-DEVT-10, Z2-I-DEVT-11
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005, Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** the `Libgraph` step's Shipping symbol check (no `___tracy_startup_profiler` in a Player's Shipping closure); `Test_DevToolsConfig_ShippingMasks` with `BUILD_CONFIGURATION_SHIPPING` and `Test_DevToolsConfig_ShippingGpuTimingsFollowEnabled` (`Test` step); the `ShippingTest` cases `Test_Profiler_ShippingStartsCountersOnlyWithoutTracy` and `Test_DevToolsConfig_ShippingMasksFromBuildInfo` (`win-shipping-test`, once the `Test` step runs it, row DV17 (f))
- **Design:** Zenith2/Docs/Design/DEVT/Profiler.md → "Runtime toggle and modes"; Zenith2/Docs/Design/DEVT.md → "Configuration keys"

### Z2-R-DEVT-017 Tracy forwarding

- **Statement:** With `devtools.tracy.enabled` set on the command line in a non-Shipping, non-sanitizer build, ZenithProfiling starts the Tracy client (on-demand, manual lifetime, loopback only, no broadcast, no crash handler, no sampling) and forwards zones (through one persistent source location per site and the non-allocating `___tracy_emit_zone_begin`), values, counters as plots, frame marks and calibrated GPU zones through the bridge table of `Internal/Tracy/` alone, which tests replace through `Zenith_ProfilerTestAccess::SetTracyBridge`.
- **Covers:** Z2-I-DEVT-11
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005, Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** the `Libgraph` step (Tracy headers only in `ZenithProfiling/Internal/Tracy/`); `Test_Profiler_Tracy_NotStartedWhenUnavailable`, `Test_Profiler_TracyStartedMidZone_StackBalanced` (`Test` step, and in `ShippingTest`)
- **Design:** Zenith2/Docs/Design/DEVT/Profiler.md → "Tracy forwarding"

### Z2-R-DEVT-018 Profiler memory preallocation and accounting

- **Statement:** Every profiler buffer except the capture arena is allocated by `Create`; the profiler and `Zenith_DevToolsMemoryConsumer` are the two `MEMORY_SUBSYSTEM_OTHER` consumers whose `GetResidentBytes` equal their live allocations, both shed nothing under pressure, and `Zenith_DevTools::EndFrame` publishes their sum as the GAUGE `devtools.memory_bytes`, Z2-B-DEVT-001's source. Each `GetResidentBytes` is computed from the same constants as the design's tables, counting a CORE `Zenith_BoundedQueue` at the size of its one allocation.
- **Covers:** Z2-I-DEVT-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040, Z2-D-046
- **Budgets:** Z2-B-DEVT-001
- **Acceptance evidence:** `Test_Profiler_MemoryConsumer_ResidentBytesMatchesAllocations`, `Test_DevTools_MemoryGaugeSumsConsumers` (`Test` step) and DEVT's case row of `axMEMORY_SHEDDING_CASES`
- **Design:** Zenith2/Docs/Design/DEVT/Profiler.md → "Memory layout and accounting"

### Z2-R-DEVT-019 Lock and blocking-wait events

- **Statement:** With `devtools.profiler.captureLocks` on, lock acquire and release hooks become `lock:<name>` zones a contended acquire also a `lockwait:<name>` zone, and blocking waits, reported after they return, `wait:<primitive>` zones of the measured wait; no hook looks at a lock's level, because the profiler's own level-940 lock is an unprofiled type that reports to no hook (Z2-R-DEVT-146).
- **Covers:** Z2-I-DEVT-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_Profiler_LockEvents_OnlyWhenEnabled`, `Test_Profiler_OwnLockNotRecorded`, `Test_Profiler_BlockingWait_MarkerZone`, `Test_Profiler_BlockingWait_DurationZone`, `Test_Profiler_LockStats_ContendedWaitAndZone` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Profiler.md → "Lock and wait events"

### Z2-R-DEVT-020 Profiler creation unwinding and errors

- **Statement:** `Zenith_Profiler::Create` validates its desc and, at each of its six fault points, unwinds everything built before it and returns its `Zenith_ProfilerError`; a creation failure at Player step 2 exits with code 4 and installs no hook.
- **Covers:** Z2-I-DEVT-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_Profiler_Create_FailsAtValidate_Unwinds` and the five sibling fault-point tests (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Profiler.md → "Creation and shutdown"; Zenith2/Docs/Design/DEVT.md → "Error enumerations"

### Z2-R-DEVT-021 Profiler shutdown and the flush thread

- **Statement:** The profiler is destroyed after `Zenith_InstallProfilerHooks(nullptr)` in shutdown row 11; its destructor asserts it is no longer installed, stops any capture, joins `Zenith.ProfFlush` and frees every buffer.
- **Covers:** Z2-I-DEVT-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_Profiler_Destroy_JoinsFlushThread` (`Test` step); `Test_ProfilerDefect_DestroyWhileInstalled` (`z2-death-assert`, Tests.md "Death cases")
- **Design:** Zenith2/Docs/Design/DEVT/Profiler.md → "Creation and shutdown"; Zenith2/Docs/Design/DEVT/Profiler.md → "Capture writer and the flush thread"

### Z2-R-DEVT-022 Capture file format

- **Statement:** A capture is a set of self-contained `.zprof` segment files, format version 1: a 96-byte little-endian header with magic `ZPRF`, then length-prefixed chunks (strings, lanes, frames, counters, log, markers, memory, zone values, lock statistics, end) padded to 8 bytes; readers validate every length and count, skip unknown chunk kinds and refuse a newer version.
- **Covers:** Z2-I-DEVT-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_Profiler_Capture_FormatRoundTrip`, `…_SegmentsSelfContained`, `…_ReaderRejectsCorrupt`, `…_UnknownChunkSkipped`, `…_LocksChunkRoundTrip` (`Test` step); `Zenith_Fuzz_ProfilerCapture` (`Sanitizers -Preset win-asan`)
- **Design:** Zenith2/Docs/Design/DEVT/Profiler.md → "Capture format"

### Z2-R-DEVT-023 Capture start stop and segment output

- **Statement:** `StartCapture(frames, withLocks)` records from the next frame into 16 MiB segments written by `Zenith.ProfFlush` through `Zenith_ProfilerOutput` to `user:/devtools/captures/<id>/`; a refused write or a writer two segments behind stops the capture with a logged reason; `StopCapture` closes it at the next frame.
- **Covers:** Z2-I-DEVT-14, Z2-I-DEVT-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_Profiler_Capture_RefusedWriteStops`, `Test_Profiler_Capture_WriterBehindStops` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Profiler.md → "Capture writer and the flush thread"

### Z2-R-DEVT-024 Memory snapshots in captures

- **Statement:** A capture holds a `MEMORY` chunk with the memory governor's statistics every 60 frames and at every pressure-level change.
- **Covers:** Z2-I-DEVT-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-057
- **Budgets:** none
- **Acceptance evidence:** `Test_Profiler_Capture_MemorySnapshotEvery60Frames` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Profiler.md → "Capture format"

### Z2-R-DEVT-025 Name-table justification record

- **Statement:** The diff adding `Zenith_ProfilerNameTable` appends the justification record `Custom container: Zenith_ProfilerNameTable` to the DecisionLog with an explicit ID, `Status: Provisional` and every template field (kind, alternative, claimed improvement, `Measured evidence: pending`, maintenance cost, acceptance criteria, contract, decision), completed in place by the `needs-human` ruling that runs `Zenith_Bench_ProfilerOverhead`'s `NameTable_*` cases against their `HashMapMutex_*` alternatives on both reference devices; DEVT adds no record for a ring, which is CORE's by ruling Q80.
- **Covers:** Z2-I-DEVT-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-041
- **Budgets:** none
- **Acceptance evidence:** the DecisionLog entry in its diff; the ruling's completed record with the harness cases' medians; the container tests its record's contract field names (`Test_ProfilerNameTable_MatchesOracle`, `…_Properties`, the three `z2-death-check` cases `…_SizeOverflow_NotPowerOfTwo`, `…_AboveMaximum` and `…_ExceedsStorage`, and `…_StressConcurrentInsert`, `Test` step)
- **Design:** Zenith2/Docs/Design/DEVT.md → "Justification records DEVT adds"; Zenith2/Docs/Design/DEVT/Profiler.md → "Name tables"

### Z2-R-DEVT-026 Profiler test support header

- **Statement:** `Zenith2/Engine/ZenithProfiling/Tests/Support/Zenith_ProfilerTestSupport.h` is header-only and provides `Zenith_ScopedProfilerInstall`, `Zenith_TestProfilerOutput`, `Zenith_TestDevFileWriter`, `Zenith_TestProfilerGpuSink`, `Zenith_TestEvidenceFileWriter` and `Zenith_TestEmitFrame`, implementing only DEVT's own interfaces and doubling no concrete engine class.
- **Covers:** Z2-I-DEVT-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** the `Build` step (a compiled-language source under `Tests/Support/` is a configure error) and every DEVT test that uses the helpers
- **Design:** Zenith2/Docs/Design/DEVT/Tests.md → "Test targets"; Zenith2/Docs/Design/DEVT.md → "ZenithProfiling test files"

### Z2-R-DEVT-027 DEVT configuration keys

- **Statement:** `Zenith_RegisterDevToolsConfigKeys(xSchema, ePlatform, eConfiguration)` registers every key of the DEVT key table with its type, range, default, layer mask and restart flag; with `BUILD_CONFIGURATION_SHIPPING` the keys marked non-Shipping are still registered, with the engine-default layer alone and their off value.
- **Covers:** Z2-I-DEVT-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_DevToolsConfig_RegistersEveryKey`, `Test_DevToolsConfig_ShippingMasks` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT.md → "Configuration keys"

### Z2-R-DEVT-028 DEVT counters and log categories

- **Statement:** DEVT publishes every counter of its counter table with the declared kind and the `_delta` suffix rule, and logs under the categories `devtools.profiler`, `devtools.console`, `devtools.debugdraw`, `devtools.remote`, `devtools.evidence`, `devtools.telemetry` and `crash`, each capacity warning once per run.
- **Covers:** Z2-I-DEVT-07, Z2-I-DEVT-22
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** the counter assertions inside the ring, debug-draw and console tests; the `devtools` panel in `Test_ProfilerPanels_CatalogueCompiles` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT.md → "Counters"; Zenith2/Docs/Design/DEVT.md → "Log categories"

### Z2-R-DEVT-029 Overlay states and the runtime toggle

- **Statement:** The on-screen profiler has the states hidden, compact and full, cycled by `Dev.ToggleProfiler`, set by `devtools.profiler.visible` and the console's `profiler.visible`; hidden draws nothing and costs only the profiler's own frame assembly.
- **Covers:** Z2-I-DEVT-01, Z2-I-DEVT-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_ProfilerOverlay_StateCycle`, `Test_ProfilerOverlay_HiddenDrawsNothing`, `Test_ProfilerOverlay_CompactDrawsTopBarOnly` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Overlay.md → "Overlay state machine"

### Z2-R-DEVT-030 CPU frame-graph timeline view

- **Statement:** The timeline view draws the selected frame's zones on one row band per CPU lane in the profiler's lane order, merging sub-pixel zones, marking truncated zones, labelling zones that fit, marking the four frame-timeline stamps, and zooming and panning with the pointer.
- **Covers:** Z2-I-DEVT-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_ProfilerOverlay_TimelineZonesMatchFrame` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Overlay.md → "Timeline view"

### Z2-R-DEVT-031 GPU pass timeline view

- **Statement:** The GPU view draws the resolved GPU frame's pass zones per queue with their view suffixes and a table of passes by time and per-view sums, labelling uncalibrated frames.
- **Covers:** Z2-I-DEVT-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005, Z2-D-055
- **Budgets:** none
- **Acceptance evidence:** `Test_ProfilerOverlay_GpuViewLanes` (`Test` step); `Test_ProfilerOverlayRender_GpuViewHasPasses` on WARP and lavapipe
- **Design:** Zenith2/Docs/Design/DEVT/Overlay.md → "GPU view"

### Z2-R-DEVT-032 Frame-time history with hitch markers

- **Statement:** The frame-history view draws the last 600 frames as split bars with hitch markers and excluded frames dimmed, and reports mean, p50, p95, p99 and maximum by nearest rank over the visible history.
- **Covers:** Z2-I-DEVT-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_ProfilerOverlay_HistoryHitchMarkers`, `Test_ProfilerOverlay_HistoryStatistics` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Overlay.md → "Frame history view"

### Z2-R-DEVT-033 Memory panel with aliasing savings

- **Statement:** The memory panel shows, in order, CORE's governor level, process and device-local usage, heap counters, every subsystem's resident bytes against its budget, the unattributed residual and refusals; the arenas' used, reserved and high-water bytes; the pools that publish live and capacity slot gauges and RHI's pool categories; RHI's device-local and per-heap usage against budget and its eight categories; and RPROD's transient memory with aliasing, without aliasing, the savings in bytes and percent and the 300-frame peak.
- **Covers:** Z2-I-DEVT-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005, Z2-D-057
- **Budgets:** none
- **Acceptance evidence:** `Test_ProfilerPanels_CatalogueCompiles` and `Test_ProfilerPanels_ProviderCalledOnlyWhenVisible` (`Test` step); `Test_ProfilerOverlayRender_FullStateFrame` on WARP and lavapipe
- **Design:** Zenith2/Docs/Design/DEVT/Overlay.md → "Built-in panel catalogue"

### Z2-R-DEVT-034 Counter panel

- **Statement:** The counter panel lists every registered counter in byte order with kind, frame value, latest and history minimum, maximum and mean, filtered by `profiler.filter`.
- **Covers:** Z2-I-DEVT-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_ProfilerPanels_CatalogueCompiles` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Overlay.md → "Panels"

### Z2-R-DEVT-035 Panel expression language

- **Statement:** Panel rows are expressions over counter frame values, `latest`, history functions, `sum`/`each`/`count` patterns, zone statistics and frame-summary fields with arithmetic, compiled at overlay creation into at most 64 instructions; a compile error names its column and message; evaluation never fails (missing counters and division by zero are marked).
- **Covers:** Z2-I-DEVT-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_ProfilerExpression_Grammar`, `…_EvaluatesCountersAndFunctions`, `…_MissingCounterMarked`, `…_DivisionByZeroUndefined` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Overlay.md → "Expression language"

### Z2-R-DEVT-036 Built-in panel catalogue

- **Statement:** The built-in panel catalogue is a set of constexpr row tables, one file per owning area under `Internal/Panels/`, in a fixed tab order, each row with label, expression, kind, unit, scale, budget row, budget value and exactness; DEVT's Stage 2 catalogue tickets write the Stage 2 areas' rows from their designs, and from Stage 3 an area adds its rows in the ticket that adds the counters they read.
- **Covers:** Z2-I-DEVT-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_ProfilerPanels_CatalogueCompiles` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Overlay.md → "Built-in panel catalogue"

### Z2-R-DEVT-037 Contention rows from JOBS counters

- **Statement:** The JOBS contention rows are computed from JOBS's counters alone, show their exactness, and are tested on interval sums, never per-frame identities.
- **Covers:** Z2-I-DEVT-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_Profiler_ContentionRowsFromCounters` in `Zenith_Integration_DevTools` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Overlay.md → "Built-in panel catalogue"; Zenith2/Docs/Design/DEVT/Tests.md → "Integration suites"

### Z2-R-DEVT-038 Luau time and GC rows

- **Statement:** The `script` panel shows SCRIPT's per-VM script time per phase and GC time and memory rows, from SCRIPT's counters, once SCRIPT's gameplay VMs land.
- **Covers:** Z2-I-DEVT-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_ProfilerPanels_CatalogueCompiles` with the `script` rows (`Test` step); the Stage 3 overlay device run
- **Design:** Zenith2/Docs/Design/DEVT/Overlay.md → "Built-in panel catalogue"

### Z2-R-DEVT-039 Panel providers

- **Statement:** `Zenith_ProfilerPanelProvider` lets a library above ZenithProfiling contribute a panel built on demand; ZenithRuntime provides the startup report, configuration dump, graph, platform and transient-memory panels, each built only while its tab is visible.
- **Covers:** Z2-I-DEVT-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_ProfilerPanels_ProviderCalledOnlyWhenVisible` (`Test` step); `Test_RuntimeDevCommands_Registered`
- **Design:** Zenith2/Docs/Design/DEVT/Overlay.md → "Panel providers"

### Z2-R-DEVT-040 Overlay input and pointer claims

- **Statement:** The overlay input adapter maps `Dev.ToggleProfiler`, `Dev.ProfilerNextPanel`, `Dev.ProfilerPreviousPanel`, `Dev.ProfilerFreeze`, `Dev.ProfilerCapture`, `Dev.ProfilerOlderFrame`, `Dev.ProfilerNewerFrame` (no two sharing a binding) and pointer input into `Zenith_ProfilerOverlayInput`, and as a presentation claimant claims every pointer press the overlay's layout hit-tests and the scroll over a scrollable panel, claiming nothing while hidden.
- **Covers:** Z2-I-DEVT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_ProfilerOverlay_HitTestMatchesLayout` (`Test` step); `Test_OverlayInputAdapter_ClaimsPointersOverOverlay`, `…_ClaimsScrollOverScrollable`, `…_NothingClaimedWhenHidden` (`ZenithRuntime_Tests`, `Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Overlay.md → "Overlay input"

### Z2-R-DEVT-041 Overlay drawing through debug-draw primitives

- **Statement:** The overlay draws only screen rectangles, lines and text through `Zenith_DebugDraw`, within 6,000 rectangles, 4,000 lines and 1,500 text records carrying 12,000 glyphs per frame in the full state, all of which the recorder's main ring holds and the next drain delivers, and therefore appears through the render world one frame after the data it shows.
- **Covers:** Z2-I-DEVT-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005, Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_ProfilerOverlay_RecordBudgetInFull`, `Test_ProfilerOverlay_ScaleFromDisplay` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Overlay.md → "Overlay drawing"; Zenith2/Docs/Design/DEVT/Overlay.md → "Overlay layout"

### Z2-R-DEVT-042 Overlay freeze and frame selection

- **Statement:** While frozen the timeline shows one of the eight held frames chosen by the frame selector, its own older and newer actions, or a history click, and unfrozen a click on one of the last eight history bars shows that frame until the next one.
- **Covers:** Z2-I-DEVT-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_ProfilerOverlay_FreezeAndSelect` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Overlay.md → "Freeze and frame selection"

### Z2-R-DEVT-043 Graph view and dump version refusal

- **Statement:** The graph panel lists the CPU frame graph's nodes from `GetLastTimings` with stage, time and critical-path mark, and refuses a dump whose `schemaVersion` is newer than its reader with `Zenith_GraphDumpVersionMessage`'s text while the rest of the panel stays live.
- **Covers:** Z2-I-DEVT-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_ProfilerPanels_ProviderCalledOnlyWhenVisible` (`Test` step) with a fixture dump of a newer version
- **Design:** Zenith2/Docs/Design/DEVT/Overlay.md → "Graph view"

### Z2-R-DEVT-044 On-screen profiler on the S24

- **Statement:** On the S24 the overlay uses UI scale 2, touch-sized tabs, the gamepad toggle and, once PLAT adds the recogniser (row DV13), the three-finger-tap toggle, the safe-area insets, and adds PLAT's thermal counters to the compact top bar.
- **Covers:** Z2-I-DEVT-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** the Stage 3 S24 overlay device run (`needs-human` step)
- **Design:** Zenith2/Docs/Design/DEVT/Overlay.md → "S24 overlay"

### Z2-R-DEVT-045 In-editor on-screen profiler

- **Statement:** The editor creates its own overlay over the editor process's profiler, drawn only in editor views by world tag, toggled from the editor toolbar and its shortcut table, with the same catalogue.
- **Covers:** Z2-I-DEVT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** the Stage 3 editor overlay run (`needs-human` step); `Test_DebugOverlayRender_EditorViewTagFiltering` on WARP and lavapipe
- **Design:** Zenith2/Docs/Design/DEVT/Overlay.md → "Editor overlay"

### Z2-R-DEVT-046 Overlay GPU capture button

- **Statement:** The GPU view's capture button runs the console line `r.capture 1` when a console exists and a capture tool is attached, and shows the reply.
- **Covers:** Z2-I-DEVT-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** the Stage 3 S24 overlay device run (`needs-human` step)
- **Design:** Zenith2/Docs/Design/DEVT/Overlay.md → "GPU view"

### Z2-R-DEVT-047 Console command registry

- **Statement:** `Zenith_DevConsole::RegisterCommand(const Zenith_DevCommandDesc&)` registers a named command with a handler `Zenith_PfnDevCommand`, user data, usage and help, and returns a move-only `Zenith_DevCommandToken` that unregisters it; names follow the dotted grammar, duplicates and a 1,025th command are refused; `HasCommand` and `GetCommandCount` let a registrant's tests inspect the real console, and destroying the console with a token still registered asserts.
- **Covers:** Z2-I-DEVT-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_DevConsole_RegisterAndExecute`, `…_TokenUnregisters`, `…_NameGrammar`, `…_DuplicateAndFull`, `…_HasCommandAndCount` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Console.md → "Zenith_DevConsole header"; Zenith2/Docs/Design/DEVT/Console.md → "Command registry"

### Z2-R-DEVT-048 Console grammar

- **Statement:** A console line of at most 1,024 bytes is tokenised (blanks, double quotes with four escapes, `;` separators, `#` comments, at most 32 tokens per statement and 16 statements) before any statement runs, and a parse error runs nothing.
- **Covers:** Z2-I-DEVT-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_DevConsole_GrammarQuotesEscapesSeparators`, `…_ParseErrorsRunNothing`, `…_LimitsEnforced` (`Test` step); `Zenith_Fuzz_DevConsoleLine` (`Sanitizers -Preset win-asan`)
- **Design:** Zenith2/Docs/Design/DEVT/Console.md → "Console grammar"

### Z2-R-DEVT-049 Built-in console commands

- **Statement:** The console provides `help`, `set`, `get`, `keys`, `echo` and `history`, and `Zenith_DevTools` registers `exec` (an asynchronous read of the development file, its lines run at frame-loop step 3) and the `profiler.*`, `devtools.*` and `crash.*` commands of the built-in command table.
- **Covers:** Z2-I-DEVT-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_DevConsole_RegisterAndExecute`, `Test_RuntimeDevCommands_Registered`, `Test_RuntimeDevCommands_ExecRunsFileLines` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Console.md → "Built-in commands"

### Z2-R-DEVT-050 Configuration variables through the console

- **Statement:** `set <key> <value>` applies to the command-line layer with provenance `console`, refusing unknown, simulation-affecting, restart-required and command-line-disallowed keys and every value the store rejects through a diagnostic, each with the reason; a changing value dispatches the store's change registry; a console value shadows the user layer for the session and says so; `get` shows value, layer and source; console values are never persisted.
- **Covers:** Z2-I-DEVT-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_DevConsole_SetRefusesSimulationAffecting`, `…_SetRefusesRestartRequired`, `…_SetAppliesCommandLineLayer`, `…_SetRefusalFromDiagnostics`, `…_SetDispatchesChangeRegistry`, `…_SetShadowsUserLayer`, `…_GetShowsProvenance` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Console.md → "Configuration variables"

### Z2-R-DEVT-051 Console output and history

- **Statement:** Every console input, output and error line enters a 256-line history and the log under `devtools.console`; a remote command's output also returns in its reply; output beyond 64 KiB per execution is cut with a marker.
- **Covers:** Z2-I-DEVT-22
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_DevConsole_OutputCut`, `Test_DevConsole_CompleteCommandsAndKeys` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Console.md → "Console output and history"

### Z2-R-DEVT-052 On-screen console view

- **Statement:** The on-screen console, toggled by `Dev.ToggleConsole`, shows recent log lines from the ring sink interleaved with console history and edits its input line through PLAT's text-input session, with history and completion.
- **Covers:** Z2-I-DEVT-22
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** the Stage 3 overlay device run (`needs-human` step)
- **Design:** Zenith2/Docs/Design/DEVT/Console.md → "On-screen console"

### Z2-R-DEVT-053 Development file writer

- **Statement:** Every development file a DEVT-registered command or a holder of the writer writes goes through `Zenith_DevFileWriter::Write` to `user:/devtools/<name>`, with names restricted to at most four `[A-Za-z0-9_.-]` segments, at most 64 MiB per file (`DEV_FILE_ERROR_TOO_LARGE`) and eight writes pending (`DEV_FILE_ERROR_BUSY`), buffers allocated per write; GRAPH's `jobs.dump` keeps its own user-mount callback and ECS's `ecs.dump` its user-mount path.
- **Covers:** Z2-I-DEVT-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_DevFileWriter_NameRules`, `Test_DevFileWriter_BusyAndTooLarge` (`Test` step); `Test_DevConsole_GraphCommandsRegistered` (`Zenith_Integration_DevTools`)
- **Design:** Zenith2/Docs/Design/DEVT/Console.md → "Zenith_DevFileWriter header"

### Z2-R-DEVT-054 Console absent in Shipping

- **Statement:** In Shipping `Zenith_DevConsole::Create` returns `DEV_CONSOLE_ERROR_NOT_IN_SHIPPING`, every consumer receives a null console, and the console's execution entry symbol is absent from the executable.
- **Covers:** Z2-I-DEVT-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** the `Libgraph` Shipping symbol check (`Zenith_DevConsole::ExecuteStatement` absent from a Player's Shipping closure); the `ShippingTest` cases `Test_DevConsole_CreateRefusedInShipping` and `Test_DevTools_ShippingCreatesNoDevServices` (`win-shipping-test`, once the `Test` step runs it, row DV17 (f))
- **Design:** Zenith2/Docs/Design/DEVT/Console.md → "Shipping absence of development services"

### Z2-R-DEVT-055 Runtime development commands

- **Statement:** ZenithRuntime registers `ecs.dump <worldTag> <path>` (ECS's form, writing to a user-mount path only), `asset.dump`, `scene.dump`, `config.dump`, `memory.stats`, `startup.report` and `world.list` at startup step 13, each calling its owning area's writer.
- **Covers:** Z2-I-DEVT-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_RuntimeDevCommands_Registered`, `Test_RuntimeDevCommands_EcsDumpArgumentForm` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Console.md → "Runtime development commands"

### Z2-R-DEVT-056 DEVT frame dump

- **Statement:** `Zenith_DevFrameDump` (declared in `Zenith_DevFrameDump.h` with `Create`, `AddSection`, `RequestFrames` and `WriteFrame`) writes, for `devtools.dump [frames]`, `zenith2.devtools-dump` version 1 per frame (summary, zones, counters, and every registered section, with `truncated` set when a section fails) to `user:/devtools/frames/<frame>.json`.
- **Covers:** Z2-I-DEVT-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_DevFrameDump_SchemaAndSections` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Console.md → "DEVT frame dump"

### Z2-R-DEVT-057 Debug-draw recorder API

- **Statement:** `Zenith_DebugDraw` offers world lines, points, arrows, boxes, AABBs, spheres, capsules, axes, frustums and 3D text over CORE's `Zenith_Vector3`, `Zenith_Quaternion` and `Zenith_Matrix4`, and screen lines, rectangles and text, with colours in `Zenith_ColourToSrgb8Packed` packing, callable from any thread, expanding shapes into line records on the recording thread, dropping a shape whole when it does not fit, and drawing a frustum whose edges do not depend on the view's depth mode. A frustum whose far corners are at infinity (CORE's infinite reversed-Z projection) draws its near edges and 1,000 m side edges, and a matrix argument is checked element by element for finiteness.
- **Covers:** Z2-I-DEVT-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_DebugDraw_ShapeExpansionCounts`, `…_FrustumCornersBothDepthModes` (including the infinite reversed-Z case), `…_ShapeDroppedWhole` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/DebugDraw.md → "Zenith_DebugDraw header"; Zenith2/Docs/Design/DEVT/DebugDraw.md → "Shape expansion"

### Z2-R-DEVT-058 Debug-draw rings and drain

- **Statement:** Each recording thread writes public 48-byte `Zenith_DebugDrawRecord`s to its own CORE `Zenith_SpscRing` pair; the main ring of 16,384 records and 64 KiB of text holds the overlay's and console's full frame, other rings 1,024 records, every overflow is dropped newest-first and counted in the ring triples; the drain node drains every ring into a double-buffered staging store sorted by world tag, and nothing on the path takes a lock.
- **Covers:** Z2-I-DEVT-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-050, Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_DebugDraw_ConcurrentRecorders`, `…_BatchesSortedByWorldTag`, `…_FullOverlayFrameDrainsWhole`, `…_MainRingOverflowCounted`, `…_StressRecordAndDrain` (`Test` step, `Sanitizers -Preset linux-tsan`); `Test_DebugDraw_RecordedInJobsDrainedOnce`
- **Design:** Zenith2/Docs/Design/DEVT/DebugDraw.md → "Recorder rings and drain"; Zenith2/Docs/Design/DEVT/DebugDraw.md → "Recorder records"

### Z2-R-DEVT-059 Debug-draw durations world tags and view filtering

- **Statement:** A debug draw with a duration persists until its deadline on the drain's `uint64` nanosecond platform-time clock, at any uptime; world tags route draws to their world's views, tag 0 to every main and editor view; capture, preview and texture views draw none.
- **Covers:** Z2-I-DEVT-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_DebugDraw_DurationExpires`, `Test_DebugDraw_DurationExpiresAfterLongUptime` (`Test` step); `Test_DebugOverlay_ScreenTagZeroInEveryView`, `Test_DebugOverlayRender_EditorViewTagFiltering`
- **Design:** Zenith2/Docs/Design/DEVT/DebugDraw.md → "Durations and world tags"

### Z2-R-DEVT-060 Render-world debug-draw records

- **Statement:** `Flux_DebugDraw.h` declares `Flux_RenderDebugDraw` (lines or quads spans of at most 2,048 elements) and the 32-byte GPU records `Flux_DebugLineSegment` and `Flux_DebugQuad` whose layouts equal `DebugDrawRecords.slang`, appended through `Flux_RenderWorldWriter::AddDebugDraw`, which copies the spans into the slot arena.
- **Covers:** Z2-I-DEVT-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_DebugOverlay_RecordLayoutsMatchShader`, `Test_DebugOverlay_ExtractorSplitsRecords` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/DebugDraw.md → "Flux_DebugDraw header"

### Z2-R-DEVT-061 DebugOverlay pass

- **Statement:** `Flux_AddDebugOverlay`, declared once in RPROD's `Flux_Features.h` and defined by DEVT (a no-pass definition in the API slice), adds no pass when a view has no debug record; otherwise it uploads the view's records and draws lines (untested, then depth-tested with hidden fragments dimmed to 25% without discard, the comparison chosen by the view's depth mode in `m_uReversedZ`) and quads with vertex pulling, premultiplied alpha over the post-tone-mapping output, on every tier.
- **Covers:** Z2-I-DEVT-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-050, Z2-D-055
- **Budgets:** none
- **Acceptance evidence:** `Test_DebugOverlay_NoPassWithoutRecords`, `…_NoPassDefinitionAddsNothing` (`Test` step); `Test_DebugOverlayRender_LinesAndText`, `…_DepthTestedDimmedBothModes` on WARP and lavapipe
- **Design:** Zenith2/Docs/Design/DEVT/DebugDraw.md → "DebugOverlay pass"; Zenith2/Docs/Design/DEVT/DebugDraw.md → "DebugLines program"; Zenith2/Docs/Design/DEVT/DebugDraw.md → "DebugText program"

### Z2-R-DEVT-062 Debug font atlas

- **Statement:** The debug font is the 95 printable ASCII glyphs of 8 × 8 pixels as a constexpr table in `Flux_DebugFontGlyphs.h`, expanded at renderer creation into a 128 × 48 `R8_UNORM` persistent texture.
- **Covers:** Z2-I-DEVT-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_DebugFont_AtlasMatchesGlyphTable` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/DebugDraw.md → "Debug font"

### Z2-R-DEVT-063 Debug-draw extractor

- **Statement:** `DevTools.ExtractDebugDraws@W<tag>` runs as an extractor of each world with the explicit orderings `After(DevTools.DrainDebugDraws@W0)` and `Before(Render.Seal@W0)` and no resource shared with the group-0 drain node, expands text into glyph quads, splits records at 2,048 elements, enforces 65,536 lines and 32,768 quads per frame and counts every dropped record; neither node is registered in headless runs.
- **Covers:** Z2-I-DEVT-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_DebugOverlay_UploadSizeAndCaps`, `…_ExtractorSplitsRecords` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/DebugDraw.md → "Debug-draw extractor"

### Z2-R-DEVT-064 Debug overlay GPU cost

- **Statement:** The `DebugOverlay` and `HiZDebugOverlay` passes together stay within Z2-B-DEVT-003 by MP-GPUPASS with the overlay in its full state, and the overlay's upload stays at most 3 MiB per frame inside Z2-B-RHI-006.
- **Covers:** Z2-I-DEVT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-DEVT-003
- **Acceptance evidence:** the Stage 3 Z2-B-DEVT-003 budget run's evidence record
- **Design:** Zenith2/Docs/Design/DEVT/DebugDraw.md → "Overlay upload budget"; Zenith2/Docs/Design/DEVT.md → "Rows DEVT owns"

### Z2-R-DEVT-065 JSON Lines file log sink

- **Statement:** `Zenith_LogFileSink` encodes each record on the log router's drain thread as one JSON line into 256 KiB chunks written to `user:/devtools/logs/<stem>.<NNNN>.jsonl`, at most 64 chunks per run and 8 runs kept, counting dropped lines; its `Detach` at MOD shutdown step 6 removes it from the router and writes the last chunk before `Zenith_AsyncIO` is destroyed; with a Shipping configuration in its desc it writes warnings and above only.
- **Covers:** Z2-I-DEVT-22
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_LogFileSink_ChunkFormat`, `…_ChunkRotationAndRetention`, `…_RefusedWriteCountsDrops`, `…_RetentionKeepsNewestEightRuns`, `…_NoWriteAfterDetach`, `…_ShippingWarningOnly` (`Test` step); the `ShippingTest` case `Test_LogFileSink_ShippingFloorFromBuildInfo` (`win-shipping-test`, once the `Test` step runs it)
- **Design:** Zenith2/Docs/Design/DEVT/Console.md → "Log file sink"

### Z2-R-DEVT-066 Log record schema

- **Statement:** Every log chunk starts with a `zenith2.log-record` version 1 header line and each record line carries `seq`, `us`, `utc`, `lvl`, `cat`, `msg`, `file`, `line`, `tid`, `frame`, `fields` and `trunc` when truncated.
- **Covers:** Z2-I-DEVT-22
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_LogFileSink_ChunkFormat` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Console.md → "Log record schema"

### Z2-R-DEVT-067 Log viewer

- **Statement:** `ZenithDevTool log view` prints chunks in sequence order across files with level, category and text filters and a follow mode.
- **Covers:** Z2-I-DEVT-22
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_DevTool_LogViewOrdersAcrossChunks` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/QualityTools.md → "ZenithDevTool"

### Z2-R-DEVT-068 Remote diagnostics server

- **Statement:** With `devtools.remote.enabled`, a non-Shipping build serves the `profiler` service on PLAT's loopback listener (port 27852), polled at frame-loop step 3, requiring the session token and then `HELLO` within 10 s, closing a connection on any malformed message.
- **Covers:** Z2-I-DEVT-18
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_DevToolsServer_HelloRequired` (`Test` step); the Stage 3 remote device run
- **Design:** Zenith2/Docs/Design/DEVT/Console.md → "Remote diagnostics server"

### Z2-R-DEVT-069 Remote protocol framing

- **Statement:** Remote messages are frames of a 4-byte length (at most 1 MiB, checked before reading), 2-byte type and 2-byte request ID followed by a compact JSON or binary payload.
- **Covers:** Z2-I-DEVT-18
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_DevToolsServer_OversizePayloadCloses` (`Test` step); `Zenith_Fuzz_RemoteFrame` (`Sanitizers -Preset win-asan`)
- **Design:** Zenith2/Docs/Design/DEVT/Console.md → "Remote protocol"

### Z2-R-DEVT-070 Remote console and log streaming

- **Statement:** A remote client runs console lines (`COMMAND` → `COMMAND_REPLY` after the next queue drain, refusing commands not remotely allowed) and subscribes to log lines and frame summaries, whose oldest messages are dropped with one error when the client falls behind.
- **Covers:** Z2-I-DEVT-18
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_DevToolsServer_CommandReply`, `…_RemoteDisallowedRefused`, `…_LogStreamBackpressure`, `…_FrameSummaryStream` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Console.md → "Remote messages"

### Z2-R-DEVT-071 Remote capture and file fetch

- **Statement:** A remote client starts a capture whose segments stream to it unchanged, lists and fetches files restricted to `user:/devtools/` and `user:/crash/`, and receives files in 1 MiB chunks.
- **Covers:** Z2-I-DEVT-18
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_DevToolsServer_CaptureStreamsSegmentsAndDone`, `…_FileGetRestricted`, `…_LogStreamBackpressure` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Console.md → "Remote messages"

### Z2-R-DEVT-072 Remote client and the adb workflow

- **Statement:** `ZenithDevTool remote` provides console, exec, logs, capture, ls, get and frames subcommands over the tool's own Windows socket client and, with `--adb`, runs `adb forward` and reads the session token from logcat through the tool's own process code.
- **Covers:** Z2-I-DEVT-18
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_DevTool_RemoteClientAgainstServer`, `Test_DevTool_AdbForwardCommandLine` (`Test` step); the Stage 3 remote device run (`needs-human` step): a transcript of `ZenithDevTool remote --adb console` against the S24
- **Design:** Zenith2/Docs/Design/DEVT/Console.md → "Remote client"

### Z2-R-DEVT-073 Remote server absent in Shipping

- **Statement:** No Shipping build creates the remote server or contains its message-handling symbol `Zenith_DevToolsServer::HandleMessage`, and `Zenith_DevToolsServer::Create` refuses in a Shipping-flag build.
- **Covers:** Z2-I-DEVT-18
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** the `Libgraph` Shipping symbol check (`Zenith_DevToolsServer::HandleMessage` absent from a Player's Shipping closure); the `ShippingTest` cases `Test_DevTools_ShippingCreatesNoDevServices` and the Shipping branch of every `Test_DevToolsServer_*` case (`Zenith_DevToolsServer::Create` returns `DEV_SERVICE_ERROR_NOT_IN_SHIPPING`)
- **Design:** Zenith2/Docs/Design/DEVT/Console.md → "Shipping absence of development services"

### Z2-R-DEVT-074 D-SEL-CRASH selection

- **Statement:** D-SEL-CRASH (open, Stage 2) is decided by a harness ticket that builds `Zenith2/Benchmarks/CrashPrototype/` (both candidates on Windows and Android, measuring coverage, out-of-process capture and minidump production, symbol-server integration, size against the 16 KB, minSdk 34 and package-budget constraints, the exception and remote-reachability checks and maintenance, with a CTest `--smoke`) and a `needs-human` ruling that applies the core documents' rule verbatim (Crashpad unless it cannot meet the 16 KB page and minSdk 34 constraints or the package budget, with DecisionLog's clauses), records that the custom candidate's Android path fails the minidump clause, commits `Zenith2/Docs/Research/DSelCrash/Report.md` and appends the ruling with the next free decision ID.
- **Covers:** Z2-I-DEVT-19
- **Stage:** S2
- **Classification:** research
- **Decisions:** Z2-D-084, Z2-D-097
- **Budgets:** none
- **Acceptance evidence:** the harness CTest smoke (`Test` step); the ruling's report (one section per measured input, the rule applied) and DecisionLog entry
- **Design:** Zenith2/Docs/Design/DEVT/Crash.md → "D-SEL-CRASH candidates and acceptance"; Zenith2/Docs/Design/DEVT.md → "Selection decisions DEVT owns"

### Z2-R-DEVT-075 Crash handler API-only header

- **Statement:** `Zenith_CrashHandler.h` (ZenithPlatform) lands in Stage 2 as an API-only header declaring `Create`, `Finalize`, `SetAnnotation`, `AddBreadcrumb`, `AddAttachment`, `RegisterCurrentThread`, `UnregisterCurrentThread`, `JobThreadLifecycle`, `WriteHangReport`, `GetLogMirror`, `GetResidentBytes` and `Zenith_RunCrashHelperIfRequested` (called directly after `Zenith_InstallRootHeap`), each with its contract block.
- **Covers:** Z2-I-DEVT-19
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045, Z2-D-047
- **Budgets:** none
- **Acceptance evidence:** the `HeaderSelfContained`, `ApiContractCheck` and `Build` steps on the Stage 2 API-only slice
- **Design:** Zenith2/Docs/Design/DEVT/Crash.md → "Zenith_CrashHandler header"

### Z2-R-DEVT-076 Windows out-of-process crash capture

- **Statement:** On Windows an unhandled exception, invalid parameter, pure call, abort or any failure-procedure termination reaches the handler through its trampoline (no vectored handler), which signals an out-of-process helper (the running executable relaunched with `--zenith-crash-helper`, inheriting the section, events and process handle) that writes a minidump, the log mirror and the report it composes itself, and a second concurrent crash writes no second report and never ends the process while the first capture runs; the process ends with the exit code its entry point defines (the exception code, `0xE05A0000 | code` for a fatal, 3 for `abort()` and a pure call, `0xC0000417` for an invalid parameter); `__fastfail` terminations, which no in-process handler sees, are measured by the D-SEL-CRASH harness and recorded there.
- **Covers:** Z2-I-DEVT-19
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-084
- **Budgets:** none
- **Acceptance evidence:** `Test_CrashDeath_NullWrite`, `…_Abort`, `…_ZenithFatal`, `…_SecondCrashNoSecondReport` (ordinary cases that crash a child process, the skipped case `Test_CrashChild_RunScenario` launched through ZenithTestSupport's `Zenith_TestProcess::LaunchSelf` ([Z2-N-ARCH-162], ruling X349 (a)), and assert its exit code and the report files carrying its PID; every `win` configuration including `ShippingTest`, `Test` step); D-SEL-CRASH harness input 1 (`Zenith2/Docs/Research/DSelCrash/Report.md`)
- **Design:** Zenith2/Docs/Design/DEVT/Crash.md → "Windows capture"; Zenith2/Docs/Design/DEVT/Crash.md → "Crash helper section layout"; Zenith2/Docs/Design/DEVT/Tests.md → "Crash tests"

### Z2-R-DEVT-077 Android crash capture

- **Statement:** On Android an async-signal-safe handler on each registered thread's alternate stack writes the report (its tail serialised by `Zenith_CrashJsonTail`, a fixed-capacity, allocation-free escaper over the pre-allocated 64 KiB report buffer whose strings are byte-identical to `Zenith_JsonWriter`'s, closing the document with `"truncated": true` when the tail does not fit; its frame walk dereferences only frame records inside the thread's registered stack bounds) and log mirror, then re-raises so debuggerd writes its tombstone, which the next launch copies from `ApplicationExitInfo`; OS kills without a pending report become `os-exit` reports.
- **Covers:** Z2-I-DEVT-19
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-084
- **Budgets:** none
- **Acceptance evidence:** `Test_CrashFinalize_ExitRecordMapping`, `Test_CrashReport_SignalSafeEscaperMatchesJsonWriter`, `Test_CrashReport_SignalSafeEscaperAllocatesNothing` on every host preset (`Test` step); the Stage 3 S24 crash device run (`needs-human` step): a finalized report with its tombstone from `crash.test access --confirm`
- **Design:** Zenith2/Docs/Design/DEVT/Crash.md → "Android capture"; Zenith2/Docs/Design/DEVT/Crash.md → "Signal-safe report tail"

### Z2-R-DEVT-078 Linux host without capture and sanitizer behaviour

- **Statement:** Linux, the test host, has no crash capture: its handler returns `CRASH_HANDLER_ERROR_DISABLED` or `CRASH_HANDLER_ERROR_UNSUPPORTED` and installs nothing, its platform-neutral crash code runs under `linux-tsan`, and every sanitizer build defaults `crash.enabled` to false with the engine-default layer alone, installing no handler, so the death tests run in the `win` Debug and Release configurations alone.
- **Covers:** Z2-I-DEVT-19
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** `Test_CrashHandler_LinuxCreateUnsupported` (`linux-tsan`, `Sanitizers` step); `Test_DevToolsConfig_RegistersEveryKey` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Crash.md → "Linux capture"; Zenith2/Docs/Design/DEVT/Crash.md → "Sanitizer builds"

### Z2-R-DEVT-079 Crash report format

- **Statement:** A crash writes `user:/crash/<stem>.report.json` (`zenith2.crash-report` version 1 with run, game, build, device, crash kind and location, modules, crashing-thread frames, annotations, breadcrumbs, attachments, files and state, and the member `"truncated": true` when the Android signal path could not fit the whole tail), `<stem>.log.txt` and a pending marker, beside the platform's dump.
- **Covers:** Z2-I-DEVT-19
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_CrashReport_SchemaRoundTrip`, `Test_CrashReport_SignalSafeEscaperMatchesJsonWriter` (`Test` step); `Zenith_Fuzz_CrashReport` (`Sanitizers -Preset win-asan`)
- **Design:** Zenith2/Docs/Design/DEVT/Crash.md → "Crash report files"; Zenith2/Docs/Design/DEVT/Crash.md → "Crash report schema"; Zenith2/Docs/Design/DEVT/Crash.md → "Signal-safe report tail"

### Z2-R-DEVT-080 Crash annotations and breadcrumbs

- **Statement:** Annotations (64 seqlocked entries, one published entry per key even when two threads set a new key at once, because the resolve's store and scan are `seq_cst`) and breadcrumbs (a 64-entry seqlocked ring), whose shared bytes are all atomic words with the specified memory orders, are written lock-free from any thread and read torn-free by the crash path; the crash log is read from the lock-free crash log mirror (128-byte slots of atomic words, each slot owned by one writer at a time through a compare-exchange of its header that sets a `WRITING` bit, a writer abandoning its line, counted, when the slot was overtaken or is held, each line committed by a release store of its first slot, and a copied line kept only when every slot header is unchanged across the copy), never through a lock.
- **Covers:** Z2-I-DEVT-21
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_CrashHandler_AnnotationSeqlock` (including the same-new-key case), `…_BreadcrumbSeqlock`, `…_BreadcrumbRingNewest64`, `…_LogMirrorLockFree`, `…_LogMirrorConcurrentWriters`, `…_LogMirrorOvertakenWriterAbandons`, `…_LogMirrorSnapshotRule` (`Test` step; `Sanitizers -Preset linux-tsan`)
- **Design:** Zenith2/Docs/Design/DEVT/Crash.md → "Annotations and breadcrumbs"; Zenith2/Docs/Design/DEVT/Crash.md → "Crash log mirror"

### Z2-R-DEVT-081 Crash attachments and device-loss association

- **Statement:** At finalization each registered attachment's newest file written before the crash time plus 10 s is copied beside the report, so a crash after a device loss carries RHI's `zenith2.device-loss` report with its GPU breadcrumbs.
- **Covers:** Z2-I-DEVT-21
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_CrashHandler_FinalizeAttachesAndRetains` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Crash.md → "Attachments"

### Z2-R-DEVT-082 Next-launch finalization and retention

- **Statement:** `Finalize` at startup step 7 completes every pending report of earlier runs within 200 ms, renames unparsable reports `.corrupt`, keeps the newest `crash.maxReports`, and logs one line per finalized report.
- **Covers:** Z2-I-DEVT-19
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_CrashHandler_FinalizeAttachesAndRetains`, `…_FinalizeCorruptRenamed` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Crash.md → "Next-launch finalization"

### Z2-R-DEVT-083 Script hang report

- **Statement:** `WriteHangReport` writes `<stem>.hang.txt` (`zenith2.hang-report 1` with the thread, reason, script stack, annotations and breadcrumbs) and an annotation naming it, before the watchdog's `ZENITH_FATAL` produces the crash report.
- **Covers:** Z2-I-DEVT-19
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_CrashHandler_HangReportFormat` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Crash.md → "Hang reports"

### Z2-R-DEVT-084 Thread registration

- **Statement:** Threads register with the crash handler (name, stack bounds, alternate signal stack or stack guarantee; on Android a registration beyond the 32 pre-allocated alternate stacks is recorded without one and counted); JOBS's threads register through `Zenith_CrashHandler::JobThreadLifecycle` passed as `m_pfnThreadLifecycle`, and a null handler means a null callback.
- **Covers:** Z2-I-DEVT-19
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_CrashHandler_JobThreadLifecycleThunk` (`Test` step); `Test_CrashDeath_StackOverflowRegisteredThread` (an ordinary case crashing a child process, every `win` configuration, `Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Crash.md → "Thread registration"

### Z2-R-DEVT-085 Crash symbolization

- **Statement:** `ZenithDevTool crash symbolize` resolves every frame against the module's PDB or unstripped shared object matched by build identity with the pinned `llvm-symbolizer`, reports mismatches per module, and sets the report's state to `symbolized`; `crash list` lists a directory's reports.
- **Covers:** Z2-I-DEVT-20
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_DevTool_CrashSymbolizeMatchesModules` (`Test` step); the symbolized report of the Stage 3 S24 crash run
- **Design:** Zenith2/Docs/Design/DEVT/Crash.md → "Symbolization"

### Z2-R-DEVT-086 Crash capture in Shipping without remote exposure

- **Statement:** The crash handler is present and armed in Shipping with no Shipping-dependent branch in its sources, writes only to the user mount and opens no socket.
- **Covers:** Z2-I-DEVT-19
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_CrashHandler_SourcesHaveNoShippingBranch` (`Test` step); the `Test_CrashDeath_*` cases in `ShippingTest` (ordinary cases crashing a child process, `win-shipping-test`, once the `Test` step runs it, row DV17 (f)); the `Libgraph` Shipping symbol check (no development-service symbol)
- **Design:** Zenith2/Docs/Design/DEVT/Crash.md → "Crash handler lifecycle"; Zenith2/Docs/Design/DEVT/Console.md → "Shipping absence of development services"

### Z2-R-DEVT-087 Evidence fingerprint specification

- **Statement:** The evidence fingerprint is the seven members of the evidence block computed by `Get-Zenith2EvidenceFingerprint`: closure lines of path and blob SHA over the measured preset's CMake File API sources, the headers `ninja -t deps` recorded for them in that preset, and `composition.json`; `toolchain` and `thirdParty` as SHIP's `toolchainHash` and `dependenciesHash` from `Get-Zenith2ManifestHashes`; the shader blob set; the content hash; the workload definition; the harness version; written as `fingerprint.json`. Every blob is the one `git add` would store (`git hash-object --stdin-paths` with filters applied, checked against the index's stage-0 blob), so the fingerprint is independent of the machine's line endings.
- **Covers:** Z2-I-DEVT-26
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-065
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Evidence_*` cases of `Test-Zenith2Evidence.ps1` run by the `Coverage` step
- **Design:** Zenith2/Docs/Design/DEVT/Evidence.md → "Evidence fingerprint specification"; Zenith2/Docs/Design/DEVT/Evidence.md → "Fingerprint algorithm"

### Z2-R-DEVT-088 Fingerprint invalidation tests

- **Statement:** The assert script proves, over SHIP's fixture manifests, that a dependency change, a toolchain bump, a middleware bump, a content bump and a preset-only header edit each invalidate an evidence record, that an unrelated change and a whitespace-only manifest edit do not, and that a record missing a field is rejected. It also proves that CRLF and LF checkouts of one tree, under either `core.autocrlf` value, give the same `closure`.
- **Covers:** Z2-I-DEVT-26
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-065
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Evidence_DependencyChange`, `…_ToolchainBump`, `…_MiddlewareBump`, `…_ContentBump`, `…_PresetOnlyHeader`, `…_UnrelatedChange`, `…_ManifestWhitespaceOnly`, `…_LineEndingsIndependent`, `…_RecordRejectedOnMismatch`, `…_RecordMissingField` (`Coverage` step)
- **Design:** Zenith2/Docs/Design/DEVT/Evidence.md → "Fingerprint invalidation tests"; Zenith2/Docs/Design/DEVT/Evidence.md → "Zenith2Evidence module"

### Z2-R-DEVT-089 Workload runner

- **Statement:** `Run-Workload.ps1` runs Release builds only, launches the tree's build or, with `-Package`, a Release measurement package of the same tree (refusing Shipping packages and tree mismatches), learns every registered `render.quality.*` key from a `--config-dump` pre-launch and passes each exactly once (the workload's value for mapped keys, the highest non-user layer's otherwise), passes the budget-run switches, the profiler and Tracy switches and the scripted input, handles cache states, repetitions and `-Research`, and validates each record.
- **Covers:** Z2-I-DEVT-26
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012, Z2-D-065
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Workload_RefusesShippingPackage`, `…_RefusesPackageTreeMismatch`, `…_QualityKeysEachOnce`, `…_ProfilerAndTracySwitches` (`Coverage` step); the Stage 3 budget runs
- **Design:** Zenith2/Docs/Design/DEVT/Evidence.md → "Workload runner"

### Z2-R-DEVT-090 Row source bindings

- **Statement:** `Zenith2/Benchmarks/Workloads/RowSources.json` binds every measurable row to one source kind (frame time, hitch, dynres, node spans, GPU passes, counter, zone sum, memory subsystem, process memory, GPU memory, emitted), and the runner refuses a workload row without a binding. A `gpuPassSum` entry without brackets matches the pass on every view (`<name>` and `<name>[<suffix>]`, summed), an entry with a suffix matches exactly, and `*` is allowed.
- **Covers:** Z2-I-DEVT-26
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** `Test_BudgetRunRecord_RejectsBadInputs`, `Test_BudgetRunRecord_GpuPassSumAcrossViews` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Evidence.md → "Row source bindings"

### Z2-R-DEVT-091 Budget-run record and statistics

- **Statement:** `Zenith_BudgetRunRecord` reads `rows.json` and `fingerprint.json` from the evidence directory, samples every row each presented frame and computes nearest-rank statistics with no interpolation or outlier removal.
- **Covers:** Z2-I-DEVT-26
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** `Test_BudgetStatistics_NearestRank`, `…_RollingMin` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Evidence.md → "Zenith_BudgetRunRecord header"; Zenith2/Docs/Design/DEVT/Evidence.md → "Budget-run sampling"; Zenith2/Docs/Design/DEVT/Evidence.md → "Statistics"

### Z2-R-DEVT-092 Evidence record writer

- **Statement:** The record writer emits exactly the fields of the evidence block with their defaults and per-row results, and the per-frame CSV with exactly the header `frameIndex,presentNs,frameMs,cpuSimMs,cpuRenderMs,gpuMs,scaleX,scaleY,excluded,hitch`, each into one PLAT host file created once in the evidence directory and only appended to.
- **Covers:** Z2-I-DEVT-26
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** `Test_BudgetRunRecord_EveryBlockFieldPresent`, `…_RowsResult`, `…_CsvHeaderExact`, `…_EvidenceFilesAppendOnly` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Evidence.md → "Evidence record writer"; Zenith2/Docs/Design/DEVT/Evidence.md → "Per-frame capture CSV"

### Z2-R-DEVT-093 Budget-run measured interval and run length

- **Statement:** A budget run excludes only `[tReady, tReady + 10 s)`, runs until 120 s and 7,200 measured frames (30 min sustained), loops the input script when the workload loops, reports the loop count, and is invalid otherwise.
- **Covers:** Z2-I-DEVT-26
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** `Test_BudgetRunRecord_ExclusionWindow`, `…_MinimumsAndInvalid` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Evidence.md → "Measured interval and run length"

### Z2-R-DEVT-094 Area row emitters

- **Statement:** An area emits a row's per-frame value with `FindRow` and `AddSample` from any thread, either node-side (its own function, called from its frame-graph node or `Flux.BuildGraph`, with the record from the module start context or the renderer desc) or as a driver-called emitter `void (*)(Zenith_BudgetRunRecord&)`; the last value since the previous `SampleFrame` wins.
- **Covers:** Z2-I-DEVT-26
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** `Test_BudgetRunRecord_EmitterAddSample`, `…_StressAddSample` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Evidence.md → "Area row emitters"

### Z2-R-DEVT-095 Profiler disabled during budget runs

- **Statement:** Every budget run disables the on-screen profiler's zones and overlay and Tracy except for rows that read profiler zones, while GPU timings stay ingested so MP-GPUPASS rows are measured, and the profiler overhead rows are measured from an enabled-versus-disabled run pair.
- **Covers:** Z2-I-DEVT-26
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Workload_ProfilerAndTracySwitches` (`Coverage` step); `Test_BudgetRunRecord_GpuPassSumInCountersOnly` (`Test` step); the Stage 3 `-ProfilerOverheadPair` records
- **Design:** Zenith2/Docs/Design/DEVT/Evidence.md → "Budget-run sampling"

### Z2-R-DEVT-096 Run telemetry and frame telemetry

- **Statement:** `Zenith_RunTelemetry` records events from any thread and per-run values, writes `zenith2.run-telemetry` version 1 and one numbered frames chunk file (`<stem>.frames.<NNNN>.csv`, each with the header, at most 64 MiB of chunks per run, then `framesTruncated`) per `Flush` when its 512 KiB buffer fills, at `devtools.telemetry.flush`, at shutdown and at its detach (the 60-s timer rewrites `run.json` alone), and writes nothing after `Detach`; it is never created while `devtools.telemetry.enabled` is false, the Shipping value, and `Create` refuses in a Shipping-flag build.
- **Covers:** Z2-I-DEVT-26
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_RunTelemetry_EventsAndFlush`, `…_DetachStopsWrites`, `…_StressRecordEvent`, `Test_DevToolsConfig_ShippingMasks` (`Test` step); the `ShippingTest` cases `Test_RunTelemetry_RefusedInShipping` and `Test_DevTools_ShippingCreatesNoDevServices`
- **Design:** Zenith2/Docs/Design/DEVT/Evidence.md → "Run telemetry"; Zenith2/Docs/Design/DEVT/Evidence.md → "Frame telemetry"

### Z2-R-DEVT-097 Regression dashboards

- **Statement:** `ZenithDevTool dashboard build` turns evidence records, run telemetry and cook reports into `zenith2.dashboard` version 1 JSON and one self-contained HTML page with every row series against its value and the 10% regression boundary.
- **Covers:** Z2-I-DEVT-26
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** `Test_DevTool_DashboardBuild` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Evidence.md → "Regression dashboards"

### Z2-R-DEVT-098 linux-tsan WSL distro provisioning

- **Statement:** The `linux-tsan` WSL2 host is the manifest's pinned distro imported by `Setup-Zenith2.ps1` with a non-root default user `zenith` and DEVT's `wsl.conf` (systemd on, Windows `PATH` not appended), restarted after configuration.
- **Covers:** Z2-I-CORE-06
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-008, Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** `Test-LinuxTsanHost.ps1` checks 1 and 2, called by `Setup-Zenith2.ps1 -Check`; the Stage 1 host bring-up TASK's committed `host-check.txt`
- **Design:** Zenith2/Docs/Design/DEVT/QualityTools.md → "WSL distro provisioning"; Zenith2/Docs/Design/DEVT/QualityTools.md → "Kernel setting"

### Z2-R-DEVT-099 linux-tsan toolchain and dependency mirror

- **Statement:** The host's compiler is the `llvm` pin under `$HOME/.zenith2/toolchain/llvm/<version>/` with `llvm-symbolizer`, the build tools are the pinned Linux archives, the toolchain file checks the compiler against the pin, every dependency is built from verified sources in `$HOME/.zenith2/thirdparty/`, and `vm.mmap_rnd_bits` is at most 28 at every start.
- **Covers:** Z2-I-CORE-06
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-008, Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** `Test-LinuxTsanHost.ps1` checks 3 to 8
- **Design:** Zenith2/Docs/Design/DEVT/QualityTools.md → "Toolchain and dependency mirror"; Zenith2/Docs/Design/DEVT/QualityTools.md → "The linux-tsan toolchain file"

### Z2-R-DEVT-100 linux-tsan host runner

- **Statement:** `Invoke-LinuxTsan.ps1` configures, builds, tests, executes and copies out in the `linux-tsan` binary directory on the Linux filesystem through `wsl.exe -d <distro> -e bash -lc`, with the fixed sanitizer options, refusing to run when the kernel setting is wrong.
- **Covers:** Z2-I-CORE-06
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_LinuxTsan_PathTranslation`, `…_SanitizerOptionsExact`, `…_ExecQuoting`, `…_KernelCheckRefuses` (`Coverage` step)
- **Design:** Zenith2/Docs/Design/DEVT/QualityTools.md → "Host runner"

### Z2-R-DEVT-101 TSan smoke suite

- **Statement:** The TSan smoke suite proves the host detects a data race, a lock-order inversion and a signed overflow and passes the atomic and mutex variants.
- **Covers:** Z2-I-CORE-06
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** `Test_TsanSmoke_*` in `Zenith_Integration_TsanSmoke` (`Sanitizers -Preset linux-tsan`); the Stage 1 host bring-up TASK's committed JUnit report
- **Design:** Zenith2/Docs/Design/DEVT/QualityTools.md → "TSan smoke suite"

### Z2-R-DEVT-102 linux-tsan host verification

- **Statement:** `Test-LinuxTsanHost.ps1` verifies, in WSL mode on a Windows host or with `-Native` on a Linux host, the distro, user and `wsl.conf` (WSL mode), the kernel setting, every pinned version including the LLVM pin, the absence of Windows tools on the Linux `PATH` and a working TSan build, exiting non-zero on any mismatch.
- **Covers:** Z2-I-CORE-06
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_LinuxTsan_HostCheckModes` (`Coverage` step); the script's run inside `Setup-Zenith2.ps1 -Check`
- **Design:** Zenith2/Docs/Design/DEVT/QualityTools.md → "Host verification"

### Z2-R-DEVT-103 libgraph gate in CI

- **Statement:** The `Libgraph` step is a hard CI gate on every `Zenith2/**` diff covering layering, the API boundary, undeclared edges, third-party confinement and the Shipping symbol check including DEVT's forbidden symbols.
- **Covers:** Z2-I-DEVT-12
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-007
- **Budgets:** none
- **Acceptance evidence:** the Stage 0 PROC tickets that land the step list this requirement; the step's report on every diff
- **Design:** Zenith2/Docs/Design/DEVT/QualityTools.md → "Gates in CI"

### Z2-R-DEVT-104 Complexity gate in CI

- **Statement:** The `Complexity` step is a hard CI gate running every threshold of `complexity_profile.json`, including `max-file-loc`, with exceptions only as profile entries carrying a decision ID.
- **Covers:** Z2-I-DEVT-13
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-007
- **Budgets:** none
- **Acceptance evidence:** the Stage 0 PROC tickets that land the step list this requirement; the step's report on every diff
- **Design:** Zenith2/Docs/Design/DEVT/QualityTools.md → "Gates in CI"

### Z2-R-DEVT-105 Fuzzing conventions and regression inputs

- **Statement:** Every fuzz target is `Tests/Fuzz/<Stem>.cpp` with a seed corpus in `Tests/Fuzz/Corpus/<Stem>/`, an optional dictionary, and committed regressions in `Tests/Fuzz/Regressions/<Stem>/` replayed by an ordinary unit test; DEVT's five parsers of untrusted input have targets.
- **Covers:** Z2-I-DEVT-24
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-011, Z2-D-028
- **Budgets:** none
- **Acceptance evidence:** the 60-second smokes of `Zenith_Fuzz_ProfilerCapture`, `…_DevConsoleLine`, `…_BudgetRunRows` (ZenithProfiling), `…_RemoteFrame` (ZenithRuntime), `…_CrashReport` (ZenithPlatform) (`Sanitizers -Preset win-asan`); `Test_<Stem>_Regressions`
- **Design:** Zenith2/Docs/Design/DEVT/QualityTools.md → "Fuzzing"

### Z2-R-DEVT-106 Fuzz campaign runner

- **Statement:** `Run-FuzzCampaign.ps1`, with its logic in `Zenith2FuzzCampaign.psm1`, builds every fuzz target, runs each for the stated duration against an accumulating working corpus, reproduces each finding once and writes `zenith2.fuzz-campaign` version 1 with exit code 1 on any reproduced finding.
- **Covers:** Z2-I-DEVT-24
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_FuzzCampaign_CommandLine`, `…_CollectsArtefacts`, `…_ExitCodeOnReproducedFinding` of `Test-FuzzCampaign.ps1` (`Coverage` step)
- **Design:** Zenith2/Docs/Design/DEVT/QualityTools.md → "Fuzz campaign runner"

### Z2-R-DEVT-107 clang-tidy configuration

- **Statement:** `Zenith2/.clang-tidy` enables the configured checks with warnings as errors, `bugprone-assert-side-effect` for `ZENITH_ASSERT` and `ZENITH_CHECK`, and the engine, tools, Player and benchmarks header filter; suppressions carry a decision ID.
- **Covers:** Z2-I-DEVT-25
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** the stage-acceptance clang-tidy run on the `win` and `linux-tsan` compile databases
- **Design:** Zenith2/Docs/Design/DEVT/QualityTools.md → "Static analysis"

### Z2-R-DEVT-108 Content validation reports

- **Statement:** `ZenithDevTool content report` renders ASSET's validation report as Markdown and JUnit and exits 1 when it holds an error.
- **Covers:** Z2-I-DEVT-23
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_DevTool_ContentReportJunit` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/QualityTools.md → "Content validation reports"

### Z2-R-DEVT-109 Asset profiling view

- **Statement:** Asset loads are profiled live through the zone values ASSET attaches and offline by `ZenithDevTool capture assets`, which groups ASSET's decode and kind-loader zones by GUID with counts and times and joins the cook report's paths and sizes.
- **Covers:** Z2-I-DEVT-17
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_DevTool_CaptureSummary` and the asset grouping assertion of `ZenithDevTool_Tests` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/QualityTools.md → "Asset profiling"

### Z2-R-DEVT-110 ZenithDevTool host tool

- **Statement:** `ZenithDevTool` is a `win` host tool target in `Zenith2/Tools/DevTool/` providing the capture, crash, remote, log, dashboard and content subcommands with exit codes 0, 1 and 2, refusing newer schema versions, and carrying its own Windows socket-client and process code.
- **Covers:** Z2-I-DEVT-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** the `Build` and `Test` steps on `ZenithDevTool_Tests`
- **Design:** Zenith2/Docs/Design/DEVT/QualityTools.md → "ZenithDevTool"

### Z2-R-DEVT-111 Capture conversion to trace events

- **Statement:** `ZenithDevTool capture convert` streams a capture into Chrome trace-event JSON (lane metadata, complete events with values, counter events, frame and marker instants, memory counters, truncation marks) in bounded memory.
- **Covers:** Z2-I-DEVT-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_CaptureConvert_TraceEventGolden` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/QualityTools.md → "Capture conversion"

### Z2-R-DEVT-112 Profiler and telemetry memory within its row

- **Statement:** DEVT's resident buffers (profiler, debug-draw recorder, console, overlay, file sink, telemetry, crash handler), measured as the gauge `devtools.memory_bytes`, stay within Z2-B-DEVT-001 by MP-SUBMEM on both platforms, excluding the on-demand buffers absent from budget runs.
- **Covers:** Z2-I-DEVT-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-DEVT-001
- **Acceptance evidence:** the Stage 3 Z2-B-DEVT-001 budget run's evidence record
- **Design:** Zenith2/Docs/Design/DEVT.md → "Rows DEVT owns"; Zenith2/Docs/Design/DEVT/Profiler.md → "Memory layout and accounting"

### Z2-R-DEVT-113 Profiler CPU overhead within its row

- **Statement:** `DevTools.EndFrame`, `DevTools.DrainDebugDraws` and every `DevTools.ExtractDebugDraws@W<tag>` together stay within Z2-B-DEVT-002 by MP-CPUZONE with the overlay full, with the hook cost reported from the enabled-versus-disabled pair.
- **Covers:** Z2-I-DEVT-14
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-DEVT-002
- **Acceptance evidence:** the Stage 3 Z2-B-DEVT-002 budget run's evidence records (`-ProfilerOverheadPair`)
- **Design:** Zenith2/Docs/Design/DEVT.md → "Rows DEVT owns"; Zenith2/Docs/Design/DEVT/Profiler.md → "Profiler cost model"

### Z2-R-DEVT-114 Stage 1 DEVT budget allocation

- **Statement:** The `needs-human` ruling "Stage 1 per-system budget ticket for DEVT", blocked by the CORE, JOBS and RPROD rulings, allocates Z2-B-DEVT-001 to Z2-B-DEVT-003 from the derivations of the design and published figures and commits `Zenith2/Docs/Research/Budgets/Stage1/DEVT/allocation.json`.
- **Covers:** Z2-I-DEVT-01
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-DEVT-001, Z2-B-DEVT-002, Z2-B-DEVT-003
- **Acceptance evidence:** the ruling's allocation file, checked by MP-ALLOC
- **Design:** Zenith2/Docs/Design/DEVT.md → "Rows DEVT owns"; Zenith2/Docs/Design/DEVT.md → "Stage 1"

### Z2-R-DEVT-115 Frame-graph zones integration test

- **Statement:** A real job system and frame executor with an installed profiler produce one zone per node per frame on the lane that ran it, with sub-job and continuation zones nested and lanes named as JOBS registers them.
- **Covers:** Z2-I-DEVT-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameGraph_ZonesForEveryNode` in `Zenith_Integration_DevTools` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Tests.md → "Integration suites"

### Z2-R-DEVT-116 Stage 2 on-screen profiler integration

- **Statement:** The Stage 2 on-screen profiler renders in its full state over a 300-frame headless run on WARP and lavapipe with no validation error, its `DebugOverlay` pass in the frame dump and debug lines and text matching the CPU reference.
- **Covers:** Z2-I-DEVT-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005, Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_ProfilerOverlayRender_FullStateFrame`, `Test_DebugOverlayRender_LinesAndText` in `Zenith_Integration_DevToolsRender` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Tests.md → "Render tests"; Zenith2/Docs/Design/DEVT.md → "Stage 2"

### Z2-R-DEVT-117 Stage 3 device and editor integration

- **Statement:** In Stage 3 the overlay runs on the S24 and in the editor, the remote console works over `adb forward`, and a deliberate crash on the S24 yields a finalized, symbolized report.
- **Covers:** Z2-I-DEVT-02, Z2-I-DEVT-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** the Stage 3 acceptance's `needs-human` device steps of DEVT
- **Design:** Zenith2/Docs/Design/DEVT/Tests.md → "Device and acceptance runs"; Zenith2/Docs/Design/DEVT.md → "Stage 3"

### Z2-R-DEVT-118 Sanitizer runs of DEVT code

- **Statement:** Every DEVT unit and integration test that exercises concurrency, including each stress test at 2, 8 and the hardware concurrency threads and the platform-neutral crash structures, runs under `win-asan` and `linux-tsan`, and the crash death tests run only outside sanitizer builds. Each stress desc is sized from its thread count (`min(128, n + 2)` thread states, `min(127, n + 1)` recorder rings), and a sub-case beyond the desc maximum is clamped and says so.
- **Covers:** Z2-I-DEVT-14, Z2-I-CORE-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan` on `ZenithProfiling_Tests`, the platform-neutral `ZenithPlatform_Tests` crash cases and `Zenith_Integration_DevTools`, triggered once X247 (c) adds the paths
- **Design:** Zenith2/Docs/Design/DEVT/Tests.md → "Stress and sanitizer runs"

### Z2-R-DEVT-119 Shipping forbidden symbols for DEVT services

- **Statement:** `shipping_forbidden_symbols.json` lists DEVT's development entry symbols, each declared in the design: `Zenith_DevConsole::ExecuteStatement`, `Zenith_DevFileWriterUserMount::Write`, `Zenith_DevFrameDump::WriteFrame`, `Zenith_DevToolsServer::HandleMessage`, `___tracy_startup_profiler` and `Zenith_GraphPanelProvider::ReadDumpFile`, so a Shipping executable containing any fails the `Libgraph` step.
- **Covers:** Z2-I-DEVT-10, Z2-I-DEVT-18
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** the `Libgraph` step's Shipping symbol check after the PROC edit that adds the entries; the `ShippingTest` case `Test_DevTools_ShippingCreatesNoDevServices` for the behaviour behind each symbol
- **Design:** Zenith2/Docs/Design/DEVT/Console.md → "Shipping absence of development services"

### Z2-R-DEVT-120 Crash handler behaviour

- **Statement:** The composition root creates `Zenith_CrashHandler` at startup step 7 and destroys it last of the step-7 objects; a second `Create` is refused through the anchor with no side effect; each fault point unwinds; a creation failure with `crash.enabled` true exits with code 4; every main that creates a handler calls `Zenith_RunCrashHelperIfRequested` directly after installing its root heap.
- **Covers:** Z2-I-DEVT-19
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045, Z2-D-084
- **Budgets:** none
- **Acceptance evidence:** `Test_CrashHandler_DisabledReturnsDisabled`, `…_SecondCreateRefused`, `…_CreateFailsAtDirectory_Unwinds`, `…_CreateFailsAtHelper_Unwinds`, `…_HelperStartsAndReportsReady` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Crash.md → "Crash handler lifecycle"

### Z2-R-DEVT-121 Crash helper security contract

- **Statement:** The Windows crash helper takes no PID and no path: it uses only inherited handles, validates the unnamed section's magic, version, 128-bit nonce and parent PID against the inherited process handle, derives the crash directory and stem itself, bounds and escapes every section string, and exits 3 writing nothing when any check fails. The helper allocates its four buffers once from the root heap that `main` installed and frees them before returning.
- **Covers:** Z2-I-DEVT-19
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-084
- **Budgets:** none
- **Acceptance evidence:** `Test_CrashHelper_ForgedSectionRefused`, `…_UnInheritedLaunchExits3`, `…_SectionStringsBoundedAndEscaped` (`win`, `Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Crash.md → "Crash helper security contract"; Zenith2/Docs/Design/DEVT/Crash.md → "Windows capture"

### Z2-R-DEVT-122 Fatal recognition without a CORE hook

- **Statement:** The crash path reports `crash.kind = fatal` with the fatal code's name and message from CORE's existing failure procedure alone (the `0xE05A0000` exception code on Windows, the `LOG_LEVEL_FATAL` `core.diagnostics` record the mirror sees, and the trap on Android), with no new CORE hook.
- **Covers:** Z2-I-DEVT-19
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_CrashFatal_DecodeExceptionCode`, `Test_CrashFatal_SlotFilledByFatalRecord` (`Test` step); `Test_CrashDeath_ZenithFatal` (an ordinary case crashing a child process, every `win` configuration, `Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Crash.md → "Fatal failures"

### Z2-R-DEVT-123 Crash structures race-free under TSan

- **Statement:** Every byte the crash anchor, annotations, breadcrumbs, fatal slot and log mirror share between threads is atomic storage accessed with the specified claim, seqlock, resolve (`seq_cst`) and commit memory orders; each mirror slot has one writer at a time (a header compare-exchange setting `WRITING`, the line abandoned when the slot is overtaken or held), the in-process reader keeps a line only when every slot header is unchanged across its copy and the Windows helper only when the headers are committed and equal in its two copies, so `linux-tsan` reports no race and no torn line is copied in their stress tests, an overtaken writer included.
- **Covers:** Z2-I-DEVT-21
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_CrashHandler_AnchorClaimProtocol`, `Test_CrashHandler_AnnotationSeqlock`, `…_BreadcrumbSeqlock`, `…_LogMirrorConcurrentWriters`, `…_LogMirrorOvertakenWriterAbandons`, `…_LogMirrorSnapshotRule` under `Sanitizers -Preset linux-tsan`
- **Design:** Zenith2/Docs/Design/DEVT/Crash.md → "Annotations and breadcrumbs"; Zenith2/Docs/Design/DEVT/Crash.md → "Crash log mirror"

### Z2-R-DEVT-124 GPU timing sink path

- **Statement:** GPU timings reach the profiler through `Zenith_ProfilerGpuSink`: the renderer registers `Flux_RendererDesc::m_pxGpuTimingSink` with `Flux_FrameLifetime::SetGpuTimingSink`, RHI's caller passes the registered sink to the stateless `Flux_PassTimingPublisher::Publish`, and a null sink publishes nothing.
- **Covers:** Z2-I-DEVT-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-055
- **Budgets:** none
- **Acceptance evidence:** `Test_PassTimingPublisher_ZonesNamedWithSuffix` (RPROD, with `Zenith_TestProfilerGpuSink`) and `Test_FrameLifetime_TimestampsPublishedAtSlotReuse` (RHI) after their revalidation (rows DV5, DV24); `Test_Profiler_Gpu_SubmissionBecomesGpuZones` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Profiler.md → "GPU timing ingestion"; Zenith2/Docs/Design/DEVT/Profiler.md → "Zenith_Profiler header"

### Z2-R-DEVT-125 GPU timings independent of the zone mode

- **Statement:** GPU timing ingestion, the GPU zones and the per-pass sums follow `devtools.profiler.gpuTimings`, never the zone mode, so a counters-only profiler (every budget run) still yields MP-GPUPASS data.
- **Covers:** Z2-I-DEVT-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005, Z2-D-055
- **Budgets:** none
- **Acceptance evidence:** `Test_Profiler_CountersOnly_GpuTimingsStillIngested`, `Test_BudgetRunRecord_GpuPassSumInCountersOnly` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Profiler.md → "GPU timing ingestion"; Zenith2/Docs/Design/DEVT/Profiler.md → "Runtime toggle and modes"

### Z2-R-DEVT-126 Zone macros expand the line number

- **Statement:** `ZENITH_PROFILE_SCOPE` and `ZENITH_PROFILE_SCOPE_NAMED` paste `__LINE__` through a two-level concatenation, so two scopes in one block and nested scopes compile under warnings-as-errors with distinct, non-shadowing identifiers.
- **Covers:** Z2-I-DEVT-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_ProfileZone_MacroTwoScopesInOneBlock` (`Build` and `Test` steps)
- **Design:** Zenith2/Docs/Design/DEVT/Profiler.md → "Zenith_ProfileZone header"

### Z2-R-DEVT-127 Zone balance across mode and Tracy changes

- **Statement:** Every zone end matches its own begin across mode switches and Tracy starts (per-level captured and Tracy masks), a dropped begin never leaves an end, and a site ID of 0 is never cached.
- **Covers:** Z2-I-DEVT-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_Profiler_ModeSwitchMidZone_EndsBalanced`, `Test_Profiler_TracyStartedMidZone_StackBalanced`, `Test_Profiler_SiteZeroNeverCached` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Profiler.md → "Hook implementation"

### Z2-R-DEVT-128 Outputs detached before the async IO service is destroyed

- **Statement:** At MOD shutdown step 6, before `Zenith_AsyncIO` is destroyed, `Zenith_DevTools::DetachOutputs` detaches the log file sink, the telemetry and the profiler's output and waits for their writes and for every development-file write and `exec` read (each cancelled at `BeginShutdown`, with its buffer owned by a writer or exec slot until it completes), so no DEVT object writes or reads through the I/O service after Contracts step 8.
- **Covers:** Z2-I-DEVT-14, Z2-I-DEVT-22
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_DevTools_DetachOutputsBeforeAsyncIO`, `Test_LogFileSink_NoWriteAfterDetach` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT.md → "Cancellation and shutdown"; Zenith2/Docs/Design/DEVT/Console.md → "Log file sink"

### Z2-R-DEVT-129 Overlay never overflows the main ring

- **Statement:** The overlay checks the main ring's free records before each view and draws merged runs with `<detail cut>` instead of overflowing, so a full-state frame is never truncated by a ring drop.
- **Covers:** Z2-I-DEVT-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_ProfilerOverlay_RecordBudgetInFull`, `Test_DebugDraw_FullOverlayFrameDrainsWhole` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/DebugDraw.md → "Recorder rings and drain"; Zenith2/Docs/Design/DEVT/Overlay.md → "Overlay drawing"

### Z2-R-DEVT-130 Recorder slot per ruling Q79

- **Statement:** The recorder finds a thread's ring through the profiler's thread slot when ruling Q79 admits it, and otherwise through its own lock-free thread-ID table with no thread-local storage, with an identical public API, rings, drain and tests on both paths.
- **Covers:** Z2-I-DEVT-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** every `Test_DebugDraw_*` case on the path built (`Test` step); the `ForbiddenCheck` writable-data scan (one allowlisted ZenithProfiling symbol on either path)
- **Design:** Zenith2/Docs/Design/DEVT/Profiler.md → "Thread slots and the thread-local variable"; Zenith2/Docs/Design/DEVT/DebugDraw.md → "Recorder slot alternative"

### Z2-R-DEVT-131 Evidence files created once and appended

- **Statement:** The budget-run driver creates the evidence record and the frames CSV once each with `Zenith_CreateFileInCommandLineHostDirectory` and only appends to them through `Zenith_EvidenceFileWriter` over `Zenith_HostFile`, never through `Zenith_ProfilerOutput`.
- **Covers:** Z2-I-DEVT-26
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** `Test_BudgetRunRecord_EvidenceFilesAppendOnly` (`Test` step); `Test_BudgetRunDriver_ReadsInputsWritesRecord` (`ZenithRuntime_Tests`)
- **Design:** Zenith2/Docs/Design/DEVT/Evidence.md → "Zenith_BudgetRunRecord header"; Zenith2/Docs/Design/DEVT/Evidence.md → "Per-frame capture CSV"

### Z2-R-DEVT-132 linux-tsan host bring-up task

- **Statement:** The `needs-human` Stage 1 TASK Bring up the linux-tsan WSL host, blocked by DEVT's four host tickets and SHIP's setup-script ticket, runs the setup, the host check and the TSan smoke suite on the reference desktop and commits the JUnit report and host-check output; every golden-recording ticket that needs the host blocks on it, and the Stage 1 acceptance re-runs the host check.
- **Covers:** Z2-I-CORE-06
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-008, Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** `Zenith2/Docs/Research/LinuxTsanHost/<utc>/junit.xml` and `host-check.txt` in the TASK's diff
- **Design:** Zenith2/Docs/Design/DEVT/QualityTools.md → "Host verification"; Zenith2/Docs/Design/DEVT.md → "Stage 1"

### Z2-R-DEVT-133 Crash handler anchor

- **Statement:** The custom crash realisation's OS callbacks reach the handler through exactly one process-wide variable, `std::atomic<Zenith_CrashHandler*> g_pxCrashHandlerAnchor` in `Zenith2/Engine/ZenithPlatform/Internal/CrashHandler/Zenith_CrashHandlerAnchor.cpp`, allowlisted for the `ForbiddenCheck` F16 scan with Z2-D-046 as extended by ruling Q88 (a); `Create` claims it (`nullptr` to the handler, `acq_rel`) before any side effect, its failure being `CRASH_HANDLER_ERROR_ALREADY_INSTALLED`; the handler is armed before the OS entry points are installed; a trampoline dereferences the handler only after its own claim (the handler to the reserved capturing value); the destructor restores the OS entry points and then releases the anchor, parking instead when a capture owns it; every other piece of crash state is a member of the handler.
- **Covers:** Z2-I-DEVT-19
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-046, Z2-D-084, D-SEL-CRASH (open, Stage 2)
- **Budgets:** none
- **Acceptance evidence:** `Test_CrashHandler_AnchorSetAndCleared`, `Test_CrashHandler_SecondCreateRefused` (`Test` step); `Test_CrashHandler_AnchorClaimProtocol` (`Test` step; `Sanitizers -Preset linux-tsan`); the `ForbiddenCheck` writable-data scan with the one allowlist entry (row DV17 (e))
- **Design:** Zenith2/Docs/Design/DEVT/Crash.md → "Crash handler anchor"; Zenith2/Docs/Design/DEVT/Crash.md → "Crash handler lifecycle"

### Z2-R-DEVT-134 Shipping behaviour proven in ShippingTest

- **Statement:** Every DEVT behaviour that only a Shipping-flag build shows (the console refusing, the counters-only start without Tracy, the configuration masks and GPU-timing rule under the real build information, the log sink's warning floor, telemetry refusing, the runtime creating no development service, crash capture with every Shipping flag) has a named `ShippingTest` case run by the `win-shipping-test` preset that also asserts the contrasting behaviour in every other configuration, while symbol absence from a Player stays the `Libgraph` Shipping symbol check; no ticket's Definition of Done names a `ShippingTest` case while the `Test` step does not run that preset.
- **Covers:** Z2-I-DEVT-10, Z2-I-DEVT-11, Z2-I-DEVT-18, Z2-I-DEVT-19
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_DevConsole_CreateRefusedInShipping`, `Test_Profiler_ShippingStartsCountersOnlyWithoutTracy`, `Test_DevToolsConfig_ShippingMasksFromBuildInfo`, `Test_LogFileSink_ShippingFloorFromBuildInfo`, `Test_RunTelemetry_RefusedInShipping`, `Test_DevTools_ShippingCreatesNoDevServices` and the `Test_CrashDeath_*` cases (ordinary cases crashing a child process) in `win-shipping-test` (the `Test` step once PROC adds the preset, row DV17 (f))
- **Design:** Zenith2/Docs/Design/DEVT/Tests.md → "ShippingTest cases"; Zenith2/Docs/Design/DEVT.md → "API rules"

### Z2-R-DEVT-135 Development-service tests correct in ShippingTest

- **Statement:** Every DEVT test that exercises a development service (the console, the file writer, the frame dump, the remote server, the runtime development commands, run telemetry, Tracy) is the two-branch statement `if constexpr (Zenith_IsShippingBuild()) { <the refusal asserts> } else { <the test> }`, whose first branch asserts the service's own refusal (or that the recording Tracy bridge table received no call), never an early `return`, so the test body is a discarded statement in `ShippingTest` (no development-only call odr-used, no unreachable code under `/W4 /WX`), and no test references a Tracy bridge function or a Tracy symbol outside `if constexpr (!Zenith_IsShippingBuild())`, so every DEVT test target builds, links and passes in `ShippingTest`.
- **Covers:** Z2-I-DEVT-10, Z2-I-DEVT-11, Z2-I-DEVT-18
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005, Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** the `Build` and `Test` steps with `-Preset win-shipping-test` over `ZenithProfiling_Tests`, `ZenithRuntime_Tests` and `Zenith_Integration_DevTools` (once PROC adds the preset, row DV17 (f)); review of each landing diff against Tests.md "ShippingTest cases"
- **Design:** Zenith2/Docs/Design/DEVT/Tests.md → "ShippingTest cases"; Zenith2/Docs/Design/DEVT/Console.md → "Shipping absence of development services"

### Z2-R-DEVT-136 Crash helper launch handles and access rights

- **Statement:** On Windows the crash handler creates its section and three events non-inheritable, duplicates inheritable copies of them and of a parent-process handle carrying exactly `PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | SYNCHRONIZE` (never `PROCESS_DUP_HANDLE`) immediately before one `CreateProcessW` with `bInheritHandles = TRUE` and a `PROC_THREAD_ATTRIBUTE_HANDLE_LIST` of exactly those five, and closes the copies as soon as it returns, so the helper starts and reports ready within 5 s and no other child can inherit them.
- **Covers:** Z2-I-DEVT-19
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-084, D-SEL-CRASH (open, Stage 2)
- **Budgets:** none
- **Acceptance evidence:** `Test_CrashHandler_HelperStartsAndReportsReady`, `Test_CrashHelper_UnInheritedLaunchExits3` (`win`, `Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Crash.md → "Windows capture"; Zenith2/Docs/Design/DEVT/Crash.md → "Crash helper security contract"

### Z2-R-DEVT-137 Profiler shared state race-free under TSan

- **Statement:** Every profiler field that a recording thread and the main thread share is atomic or published by an atomic with a stated order: thread-state claims move 0, 1, 2 with the plain fields published by the `seq_cst` store of 2; `m_bEndDropped` is stored `release` and taken with `exchange(false, acq_rel)`; the lane table is written by the main thread alone (registration, the automatic and overflow lanes `EndFrame` creates) and published by a `seq_cst` lane-count store that a claim reads with `seq_cst` after publishing its state; a thread's lane is bound only by compare-exchanges (the claim from 0, the registration and assembly from the value they read); the profiler-level switches (mode, lock capture, GPU timings, Tracy active) are atomics with stated orders; GPU slots move between a free and a ready CORE `Zenith_SpscRing<uint8>`; each thread's lock-site banks are handed to the main thread by a `seq_cst` writer-count handshake, so a bank is merged only when no hook is inside it; so `linux-tsan` reports no race in the profiler's stress tests.
- **Covers:** Z2-I-DEVT-14, Z2-I-DEVT-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-011, Z2-D-041
- **Budgets:** none
- **Acceptance evidence:** `Test_Profiler_HookStress`, `Test_Profiler_Lanes_BindByThreadName`, `Test_Profiler_Lanes_AutomaticLanesCreatedByAssembly`, `Test_Profiler_Gpu_SlotsRecycledThroughIndexRings`, `Test_Profiler_EndDroppedRepairAcrossThreads`, `Test_Profiler_LockStats_BankHandoverNoLoss` (`Test` step; `Sanitizers -Preset linux-tsan`); `Test_Profiler_HookStressUnderJobs` (`Zenith_Integration_DevTools` under `linux-tsan`)
- **Design:** Zenith2/Docs/Design/DEVT/Profiler.md → "Thread slots and the thread-local variable"; Zenith2/Docs/Design/DEVT/Profiler.md → "Thread lanes"; Zenith2/Docs/Design/DEVT/Profiler.md → "GPU timing ingestion"; Zenith2/Docs/Design/DEVT/Profiler.md → "Lock statistics"

### Z2-R-DEVT-138 Name arena exhaustion never spins

- **Statement:** The profiler's name arena holds a 63-byte name for every entry of the site, counter and lane tables (1,314,816 bytes), and a table over a smaller arena reserves a name's bytes after winning the hash and, when they do not fit, leaves the entry in the terminal state 3, so every subsequent lookup of that name returns site 0 at once with no spin.
- **Covers:** Z2-I-DEVT-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-041
- **Budgets:** Z2-B-DEVT-001
- **Acceptance evidence:** `Test_ProfilerNameTable_Properties` (the 256-byte-arena and worst-case cases), `Test_ProfilerNameTable_StressConcurrentInsert` (`Test` step; `Sanitizers -Preset linux-tsan`)
- **Design:** Zenith2/Docs/Design/DEVT/Profiler.md → "Name tables"

### Z2-R-DEVT-139 Shipping GPU timings follow the on-screen profiler toggle

- **Statement:** In Shipping the on-screen profiler toggle is `devtools.profiler.enabled`: `Zenith_ResolveProfilerSettings` gives the profiler's GPU-timing setting that key's resolved value and re-applies it on every change, and RPROD's `r.graph.timestamps` follows the same key, so showing the overlay in Shipping inserts and ingests GPU timestamps in the same frame and the GPU view is filled; no configuration key is written.
- **Covers:** Z2-I-DEVT-04, Z2-I-DEVT-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005, Z2-D-055
- **Budgets:** none
- **Acceptance evidence:** `Test_DevToolsConfig_ShippingGpuTimingsFollowEnabled` (`Test` step); the `ShippingTest` case `Test_DevToolsConfig_ShippingMasksFromBuildInfo`
- **Design:** Zenith2/Docs/Design/DEVT/Profiler.md → "Runtime toggle and modes"; Zenith2/Docs/Design/DEVT.md → "Configuration keys"

### Z2-R-DEVT-140 Budget-run record excluded from memory samples

- **Statement:** The budget-run record is a `MEMORY_SUBSYSTEM_OTHER` consumer of its own, outside Z2-B-DEVT-001, and the `processMemory` and `memorySubsystem` `other` samples of a budget run subtract its `GetResidentBytes()`, so the measurement apparatus never inflates the run's memory rows.
- **Covers:** Z2-I-DEVT-26
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** `Test_BudgetRunRecord_MemorySamplesExcludeRecord` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Evidence.md → "Zenith_BudgetRunRecord header"; Zenith2/Docs/Design/DEVT/Evidence.md → "Row source bindings"

### Z2-R-DEVT-141 Console handlers never write simulation state

- **Statement:** No console command handler, DEVT's or a registrant's, writes simulation-visible state directly: a command that must change a simulation submits an input command through that world's recorded command stream or refuses itself while the world records or replays, as `Zenith_DevCommandDesc`'s contract states, so a typed command never desynchronises a replay.
- **Covers:** Z2-I-DEVT-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** review of every diff that registers a command against `Zenith_DevCommandDesc`'s contract; `Test_DevConsole_SetRefusesSimulationAffecting` (`Test` step) for DEVT's own `set`
- **Design:** Zenith2/Docs/Design/DEVT/Console.md → "Zenith_DevConsole header"; Zenith2/Docs/Design/DEVT.md → "Determinism"

### Z2-R-DEVT-142 Death cases classified by their macro

- **Statement:** Every DEVT death case that dies by `ZENITH_ASSERT` is registered `z2-death-assert` (Debug, Sanitize, Hwasan), and every case that dies by `ZENITH_CHECK` or `ZENITH_FATAL` with CORE's failure text is `z2-death-check` (every configuration), one refusal per case, so no death case fails in Release, Shipping or `ShippingTest` because its assert is compiled out; a crash case, which dies by a real fault, `abort()` or a second crash and prints no failure text, is not a death case but an ordinary case that crashes a child process.
- **Covers:** Z2-I-DEVT-14, Z2-I-DEVT-19
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_ProfilerDefect_EndFrameOffMainThread`, `Test_ProfilerDefect_DestroyWhileInstalled` (`z2-death-assert`), `Test_ProfilerDefect_RingIndexCorruption`, `Test_ProfilerNameTable_SizeOverflow_NotPowerOfTwo`, `…_AboveMaximum`, `…_ExceedsStorage` (`z2-death-check`), each in the `Test` step's suite of its label; the `Test_CrashDeath_*` cases as ordinary cases (Tests.md "Crash tests")
- **Design:** Zenith2/Docs/Design/DEVT/Tests.md → "Death cases"

### Z2-R-DEVT-143 Profiling brackets accumulate per site per frame

- **Statement:** One profiling bracket (one `ZENITH_PROFILE_SCOPE` or `ZENITH_PROFILE_SCOPE_NAMED` site) may be entered any number of times per frame, in a loop, from several call paths, nested in itself and on several threads, and the profiler accumulates every closure of that site into the site's one frame statistic, keyed on the site and never on a zone or scope object: the count is every closure, the maximum the longest single closure, and the total the inclusive duration summed over closures and threads, where a closure begun inside an open zone of the same site on the same thread adds nothing, so no nanosecond is counted twice; the statistics are reset exactly once per frame, at the start of `EndFrame`'s assembly, so the next frame starts at zero, and GPU pass sites accumulate by the same rule.
- **Covers:** Z2-I-DEVT-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_Profiler_ZoneStats_LoopAndCallPathsAccumulate`, `Test_Profiler_ZoneStats_TwoThreadsAccumulate`, `Test_Profiler_ZoneStats_RecursiveSiteNotDoubleCounted`, `Test_Profiler_ZoneStats_NextFrameStartsAtZero`, `Test_Profiler_ZoneStats_ResetOncePerFrame`, `Test_Profiler_ZoneStats_GpuPassesAccumulate`, `Test_ProfilerExpression_EvaluatesCountersAndFunctions`, `Test_BudgetRunRecord_ZoneSumAccumulatesSite` (`Test` step); `Test_Profiler_SubJobZonesAccumulatePerSite` in `Zenith_Integration_DevTools` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Profiler.md → "Zone statistics"; Zenith2/Docs/Design/DEVT/Profiler.md → "Zenith_ProfileZone header"

### Z2-R-DEVT-144 Lock statistics accumulate per lock site per frame

- **Statement:** With `devtools.profiler.captureLocks` on in capture mode, the profiler keeps for every lock site (a lock name) and every frame the acquisitions, the contended acquisitions, the total and maximum wait and the total and maximum hold, accumulating every use of the site in the frame across mutex instances, loops and threads (a condition-variable wait ends a hold and the wake-up starts a new one) and resetting the totals exactly once per frame, at the start of `EndFrame`'s assembly; a contended acquire (a measured wait above zero) also produces a `lockwait:<name>` zone beside the `lock:<name>` hold zone; the statistics are readable through `GetLockStats`, drawn by the overlay's lock table, written to captures as the `LOCKS` chunk and summed into the frame summary's `m_uLockContended` and `m_uLockWaitUs`.
- **Covers:** Z2-I-DEVT-14, Z2-I-DEVT-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_Profiler_LockStats_AccumulateOverUses`, `Test_Profiler_LockStats_ResetAtEndFrame`, `Test_Profiler_LockStats_ContendedWaitAndZone`, `Test_Profiler_LockStats_HoldAcrossFrames`, `Test_Profiler_LockStats_CondVarWaitEndsHold`, `Test_Profiler_LockStats_GatedByCaptureLocks`, `Test_Profiler_LockStats_FrameSummaryFields`, `Test_Profiler_Capture_LocksChunkRoundTrip`, `Test_ProfilerOverlay_LockTable` (`Test` step); `Test_Profiler_ContendedLock_ProducesLockWaitZone` and `Test_Profiler_CondVarWait_HoldExcludesWait` in `Zenith_Integration_DevTools` (`Test` step)
- **Design:** Zenith2/Docs/Design/DEVT/Profiler.md → "Lock statistics"; Zenith2/Docs/Design/DEVT/Profiler.md → "Lock and wait events"; Zenith2/Docs/Design/DEVT/Profiler.md → "Capture format"; Zenith2/Docs/Design/DEVT/Overlay.md → "Timeline view"

### Z2-R-DEVT-145 Lock statistics are collected race-free

- **Statement:** Every lock hook writes only the calling thread's own state: its event ring, its held-lock stack and its lock-site bank; the per-thread lock-site table is fixed-capacity (64 slots, 48 usable, two banks per thread), open-addressed and keyed by lock name and level, never by mutex address, a full table or held-lock stack is counted and never grown, and the main thread merges a bank into the frame totals only at `EndFrame`, after the rings are drained, through a writer-count handshake that grants it a bank only when no hook is inside it; no hook takes a lock, allocates or blocks, so N threads using one profiled lock while the profiler captures lose no acquisition beyond the counted refusals and `linux-tsan` reports no race.
- **Covers:** Z2-I-DEVT-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-011, Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_Profiler_LockStats_BankHandoverNoLoss`, `Test_Profiler_LockStats_TableOverflowCounted`, `Test_Profiler_LockStats_ManyInstancesOneEntry`, `Test_Profiler_HooksTakeNoMutex`, `Test_Profiler_HooksAllocateNothing` (`Test` step; `Sanitizers -Preset linux-tsan`); `Test_Profiler_LockStats_HammerOneLock` in `Zenith_Integration_DevTools` (`Test` step; `Sanitizers -Preset linux-tsan`)
- **Design:** Zenith2/Docs/Design/DEVT/Profiler.md → "Lock statistics"; Zenith2/Docs/Design/DEVT/Profiler.md → "Thread slots and the thread-local variable"

### Z2-R-DEVT-146 The profiler cannot recurse through its own primitives

- **Statement:** Every mutex, condition variable and semaphore of ZenithProfiling (the capture-segment queue's level-940 mutex, the flush thread's semaphore and any idle wait) is a `Zenith_MutexNoProfiling`, `Zenith_ConditionVariableNoProfiling` or `Zenith_SemaphoreNoProfiling`, which never reads the hook table, so no lock the profiler takes can reach a hook; ZenithProfiling's non-test sources name no profiled alias (`ForbiddenCheck` F26), the former run-time rule that a level-940 lock is never recorded is withdrawn, and `m_bInProfiler` stays as defence in depth alone.
- **Covers:** Z2-I-DEVT-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_Profiler_OwnPrimitivesAreNoProfiling`, `Test_Profiler_OwnLockNotRecorded` (`Test` step); `Test_Profiler_NoProfilingLock_NotRecorded` in `Zenith_Integration_DevTools` (`Test` step); PROC's `ForbiddenCheck` F26 fixtures
- **Design:** Zenith2/Docs/Design/DEVT/Profiler.md → "Lock and wait events"; Zenith2/Docs/Design/DEVT.md → "Lock-order entries"

### Z2-R-DEVT-147 The profiler's own code reaches hooks only through the dispatch functions

- **Statement:** Every hook call that ZenithProfiling's own code makes (the `Zenith_ProfileScope` constructor and destructor, `EndFrame`'s `Profiler.Assemble` zone and the flush thread's `Profiler.FlushSegment` zone) goes through CORE's `Zenith_DispatchZoneBegin` and `Zenith_DispatchZoneEnd`, never through a `m_pfn` member of the hook table; `Zenith_GetProfilerHooks()` is read by the profiler's install code and by tests alone.
- **Covers:** Z2-I-DEVT-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_Profiler_OwnZonesRecorded` (`Test` step); PROC's `ForbiddenCheck` F26 fixtures
- **Design:** Zenith2/Docs/Design/DEVT/Profiler.md → "Hook implementation"; Zenith2/Docs/Design/DEVT/Profiler.md → "Zenith_ProfileZone header"; Zenith2/Docs/Design/DEVT/Profiler.md → "Capture writer and the flush thread"

## Coverage of inventory items

| Inventory item | Item | Requirements |
|---|---|---|
| Z2-I-DEVT-01 | in-game on-screen profiler | Z2-R-DEVT-001, Z2-R-DEVT-029, Z2-R-DEVT-039, Z2-R-DEVT-040, Z2-R-DEVT-041, Z2-R-DEVT-057, Z2-R-DEVT-058, Z2-R-DEVT-059, Z2-R-DEVT-060, Z2-R-DEVT-061, Z2-R-DEVT-062, Z2-R-DEVT-063, Z2-R-DEVT-064, Z2-R-DEVT-114, Z2-R-DEVT-116, Z2-R-DEVT-129, Z2-R-DEVT-130 |
| Z2-I-DEVT-02 | in-editor on-screen profiler | Z2-R-DEVT-045, Z2-R-DEVT-117 |
| Z2-I-DEVT-03 | profiler view: CPU frame-graph timeline | Z2-R-DEVT-008, Z2-R-DEVT-012, Z2-R-DEVT-030, Z2-R-DEVT-042, Z2-R-DEVT-043, Z2-R-DEVT-115, Z2-R-DEVT-144 |
| Z2-I-DEVT-04 | profiler view: per-pass GPU timings | Z2-R-DEVT-014, Z2-R-DEVT-031, Z2-R-DEVT-124, Z2-R-DEVT-125, Z2-R-DEVT-139 |
| Z2-I-DEVT-05 | profiler view: frame-time history and hitches | Z2-R-DEVT-011, Z2-R-DEVT-032 |
| Z2-I-DEVT-06 | profiler view: memory including aliasing savings | Z2-R-DEVT-033 |
| Z2-I-DEVT-07 | profiler view: counters | Z2-R-DEVT-009, Z2-R-DEVT-028, Z2-R-DEVT-034, Z2-R-DEVT-035, Z2-R-DEVT-036, Z2-R-DEVT-037 |
| Z2-I-DEVT-08 | profiler view: Luau time and GC | Z2-R-DEVT-038 |
| Z2-I-DEVT-09 | on-screen profiler on Windows and the S24 | Z2-R-DEVT-044, Z2-R-DEVT-117 |
| Z2-I-DEVT-10 | on-screen profiler runtime toggle | Z2-R-DEVT-015, Z2-R-DEVT-016, Z2-R-DEVT-027, Z2-R-DEVT-029, Z2-R-DEVT-047, Z2-R-DEVT-048, Z2-R-DEVT-049, Z2-R-DEVT-050, Z2-R-DEVT-054, Z2-R-DEVT-119, Z2-R-DEVT-134, Z2-R-DEVT-135, Z2-R-DEVT-139, Z2-R-DEVT-141 |
| Z2-I-DEVT-11 | Tracy integration | Z2-R-DEVT-016, Z2-R-DEVT-017, Z2-R-DEVT-134, Z2-R-DEVT-135 |
| Z2-I-DEVT-12 | libgraph gate in CI | Z2-R-DEVT-103 |
| Z2-I-DEVT-13 | complexity gate in CI | Z2-R-DEVT-104 |
| Z2-I-DEVT-14 | CPU profiling | Z2-R-DEVT-001, Z2-R-DEVT-002, Z2-R-DEVT-003, Z2-R-DEVT-004, Z2-R-DEVT-005, Z2-R-DEVT-006, Z2-R-DEVT-007, Z2-R-DEVT-010, Z2-R-DEVT-013, Z2-R-DEVT-019, Z2-R-DEVT-020, Z2-R-DEVT-021, Z2-R-DEVT-022, Z2-R-DEVT-023, Z2-R-DEVT-025, Z2-R-DEVT-026, Z2-R-DEVT-053, Z2-R-DEVT-055, Z2-R-DEVT-056, Z2-R-DEVT-110, Z2-R-DEVT-111, Z2-R-DEVT-113, Z2-R-DEVT-118, Z2-R-DEVT-126, Z2-R-DEVT-127, Z2-R-DEVT-128, Z2-R-DEVT-137, Z2-R-DEVT-138, Z2-R-DEVT-142, Z2-R-DEVT-143, Z2-R-DEVT-144, Z2-R-DEVT-145, Z2-R-DEVT-146, Z2-R-DEVT-147 |
| Z2-I-DEVT-15 | GPU profiling | Z2-R-DEVT-014, Z2-R-DEVT-046, Z2-R-DEVT-137 |
| Z2-I-DEVT-16 | memory profiling | Z2-R-DEVT-018, Z2-R-DEVT-023, Z2-R-DEVT-024, Z2-R-DEVT-112 |
| Z2-I-DEVT-17 | asset profiling | Z2-R-DEVT-005, Z2-R-DEVT-109 |
| Z2-I-DEVT-18 | remote Android diagnostics | Z2-R-DEVT-068, Z2-R-DEVT-069, Z2-R-DEVT-070, Z2-R-DEVT-071, Z2-R-DEVT-072, Z2-R-DEVT-073, Z2-R-DEVT-119, Z2-R-DEVT-134, Z2-R-DEVT-135 |
| Z2-I-DEVT-19 | crash dumps | Z2-R-DEVT-074, Z2-R-DEVT-075, Z2-R-DEVT-076, Z2-R-DEVT-077, Z2-R-DEVT-078, Z2-R-DEVT-079, Z2-R-DEVT-082, Z2-R-DEVT-083, Z2-R-DEVT-084, Z2-R-DEVT-086, Z2-R-DEVT-120, Z2-R-DEVT-121, Z2-R-DEVT-122, Z2-R-DEVT-133, Z2-R-DEVT-134, Z2-R-DEVT-136, Z2-R-DEVT-142 |
| Z2-I-DEVT-20 | crash symbols | Z2-R-DEVT-085 |
| Z2-I-DEVT-21 | GPU fault breadcrumbs | Z2-R-DEVT-080, Z2-R-DEVT-081, Z2-R-DEVT-123 |
| Z2-I-DEVT-22 | structured logging | Z2-R-DEVT-028, Z2-R-DEVT-051, Z2-R-DEVT-052, Z2-R-DEVT-065, Z2-R-DEVT-066, Z2-R-DEVT-067, Z2-R-DEVT-128 |
| Z2-I-DEVT-23 | content validation | Z2-R-DEVT-108 |
| Z2-I-DEVT-24 | fuzzing | Z2-R-DEVT-105, Z2-R-DEVT-106 |
| Z2-I-DEVT-25 | static analysis | Z2-R-DEVT-107 |
| Z2-I-DEVT-26 | regression dashboards | Z2-R-DEVT-087, Z2-R-DEVT-088, Z2-R-DEVT-089, Z2-R-DEVT-090, Z2-R-DEVT-091, Z2-R-DEVT-092, Z2-R-DEVT-093, Z2-R-DEVT-094, Z2-R-DEVT-095, Z2-R-DEVT-096, Z2-R-DEVT-097, Z2-R-DEVT-131, Z2-R-DEVT-140 |
| Z2-I-CORE-06 | sanitizer integration | Z2-R-DEVT-098, Z2-R-DEVT-099, Z2-R-DEVT-100, Z2-R-DEVT-101, Z2-R-DEVT-102, Z2-R-DEVT-118, Z2-R-DEVT-132 |

`Z2-I-CORE-06` is CORE's item; DEVT's requirements serve its `linux-tsan` half (the host its sanitizer runs need, Stage 1 deliverable 4), and CORE's catalogue carries the heap, arena and pool instrumentation.

## Excluded and deferred

No DEVT inventory item is excluded, deferred or game-side. One requirement is `research`: Z2-R-DEVT-074, the D-SEL-CRASH selection, whose ruling gates the Stage 3 crash realisation and never the Stage 2 API-only header. Out of DEVT's scope, with the owner named: crash-report upload and any hosted collection service (excluded: reports stay on the device's user mount, Z2-D-022 governs any challenge); remote tools and the development console in Shipping (`Zenith2/Docs/Architecture.md` → "Security architecture"); a build-flag profiler (Z2-D-005); GPU timestamp queries and calibration (RPROD, RHI); GPU fault breadcrumbs and the device-loss report (RHI; DEVT attaches the report); RenderDoc and PIX captures (RHI; DEVT's button runs `r.capture`); each area's counters, zones, rows and dumps (the publishing area); the gate scripts themselves (PROC); WSL provisioning and the manifest pins (SHIP); the Luau debugger and script sampling profiler (SCRIPT); symbol retention (SHIP).
