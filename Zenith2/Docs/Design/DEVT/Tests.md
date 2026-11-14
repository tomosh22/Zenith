# DEVT design: tests

This file is part of the DEVT design family; the entry document is `Zenith2/Docs/Design/DEVT.md`. It names every DEVT test: its target, file, the case it asserts and the gate step that runs it. Every test is a doctest case registered as its own CTest test, or a PowerShell assert-script case; no test pins a count, no test doubles a concrete engine class (tests construct the real `Zenith_Profiler`, `Zenith_DevConsole`, `Zenith_DebugDraw`, `Zenith_ProfilerOverlay` and `Zenith_BudgetRunRecord`, all of which run without a device), and no ticket asks a worker to run a build or a test: each test lands in the ticket that lands its code and the gate steps run it.

## Scope of this file

| Group | Section |
|---|---|
| targets, the support header | § "Test targets" |
| ZenithProfiling unit tests | § "ZenithProfiling unit tests", "Overlay tests", "Console tests", "Debug-draw tests", "Evidence tests" |
| ZenithPlatform crash tests | § "Crash tests" |
| scripts and tools | § "Quality-tool tests" |
| integration and render suites | § "Integration suites", "Render tests" |
| Shipping behaviour and the rule for development-service tests in `ShippingTest` | § "ShippingTest cases" |
| death cases and their suites | § "Death cases" |
| sanitizer, stress, device and acceptance runs | § "Stress and sanitizer runs", "Device and acceptance runs" |
| fixtures | § "Fixtures" |

## Test targets

| Target | Sources | Presets | Gate steps |
|---|---|---|---|
| `ZenithProfiling_Tests` | `Engine/ZenithProfiling/Tests/*.cpp` | `win` (Debug, Release, `ShippingTest`), `win-asan`, `linux-tsan` | `Test`, `Test -Label memory` (the library declares `TEST_LABELS memory`), `Sanitizers` (once X247 (c) puts `Zenith2/Engine/ZenithProfiling/**` in the `Zenith2Memory` paths, a ZenithProfiling diff triggers the category's two `Sanitizers` lines, [Z2-N-CONTRACT-357]) |
| `Zenith_Fuzz_ProfilerCapture`, `…_DevConsoleLine`, `…_BudgetRunRows` | `Engine/ZenithProfiling/Tests/Fuzz/` | `win-asan` | `Sanitizers -Preset win-asan` (60 s smoke) |
| `ZenithPlatform_Tests` (DEVT's crash files) | `Engine/ZenithPlatform/Tests/Zenith_CrashHandler_Tests.cpp` (the platform-neutral cases, and the Windows-realisation cases, which select themselves at run time through `Zenith_CrashHandlerTestAccess`), `Tests/Zenith_CrashHandlerDefect_Tests.cpp` (`Test_CrashDeath_*`, `Test_CrashHelper_*`, `Test_CrashEndToEnd_*` and the skipped child case `Test_CrashChild_RunScenario`), `Tests/Zenith_PlatformTestMain.cpp`; no `Tests/Windows/` or `Tests/Linux/` directory ([Z2-N-ARCH-162]) | `win` (Debug, Release, `ShippingTest`) and `win-asan` for every case (the crash cases take their in-process sanitizer branch in `win-asan`, § "Crash tests"); `linux-tsan` for the platform-neutral cases | `Test` (every case ordinary: none is a death-test suite entry), `Sanitizers` |
| `Zenith_Fuzz_CrashReport` | `Engine/ZenithPlatform/Tests/Fuzz/CrashReport.cpp` | `win-asan` | `Sanitizers` |
| `ZenithFlux_Tests` (DEVT's debug files) | `Engine/ZenithFlux/Tests/Debug/Flux_DebugOverlay_Tests.cpp` | `win` | `Test` (WARP, lavapipe) |
| `ZenithRuntime_Tests` (DEVT's runtime files) | `Engine/ZenithRuntime/Tests/DevTools/Zenith_DevTools_Tests.cpp` | `win` | `Test` |
| `Zenith_Fuzz_RemoteFrame` | `Engine/ZenithRuntime/Tests/Fuzz/RemoteFrame.cpp` | `win-asan` | `Sanitizers` |
| `Zenith_Integration_DevTools` | `Tests/DevTools/` | `win`, `win-asan`, `linux-tsan` | `Test`, `Sanitizers` |
| `Zenith_Integration_DevToolsRender` | `Tests/DevToolsRender/` | `win` | `Test` (WARP, lavapipe) |
| `Zenith_Integration_TsanSmoke` | `Tests/TsanSmoke/` | `win`, `linux-tsan` | `Test`, `Sanitizers -Preset linux-tsan` |
| `ZenithDevTool_Tests` | `Tools/DevTool/Tests/` | `win` (the tool is `ZENITH_TOOLS_HOST`, `win` alone) | `Test` |
| `Zenith_Bench_ProfilerOverhead` | `Benchmarks/ProfilerOverhead/` | `win` Release | `Test` (`--smoke`) |
| assert scripts | `Benchmarks/Evidence/Tests/Test-Zenith2Evidence.ps1`, `Tools/DevTool/Scripts/Tests/Test-Zenith2LinuxTsan.ps1`, `Benchmarks/Fuzz/Tests/Test-FuzzCampaign.ps1` | host `pwsh` | the `Coverage` step's script run (row DV17 (b)) |

Every test target the `win` tree supports is also built by the `win-shipping-test` build preset's `zenith_tests` and run by its test preset in the `ShippingTest` configuration ([Z2-N-ARCH-191], [Z2-N-ARCH-231]); the `Test` step runs that preset once PROC's protected edit adds it (row DV17 (f)). Until then, and in every ticket, the `ShippingTest` cases of § "ShippingTest cases" are specified in full and named in a ticket's Validation section, never as Definition-of-Done items, because a worker can observe no result the gate does not produce (WORKER-EVIDENCE).

`Engine/ZenithPlatform/Tests/Zenith_PlatformTestMain.cpp` is ZenithPlatform's own test `main`, so the crash tests can relaunch the test executable as the out-of-process helper (the handler's `Create` starts `<this executable> --zenith-crash-helper …`, § "Crash tests"). The helper dispatch needs the root heap ([Z2-N-CONTRACT-142]; Crash.md "Crash helper security contract") and follows `Zenith_ApplyCrtMathMode()`, the first statement of every `main` ([Z2-N-ARCH-889]); `Zenith_RunTestMain` installs its own heap and re-applies the mode harmlessly, so the main installs one for the dispatch alone and uninstalls it before forwarding:

```cpp
int main(int iArgc, char** ppszArgv)
{
	Zenith_ApplyCrtMathMode();
	Zenith_Heap xHeap{ Zenith_HeapDesc{} };
	Zenith_InstallRootHeap(&xHeap);
	int iExit = 0;
	const bool bHandled = Zenith_RunCrashHelperIfRequested(iArgc, ppszArgv, Zenith_CrashHelperIdentity{ "ZenithPlatformTests", 0 }, iExit);
	Zenith_InstallRootHeap(nullptr);
	if (bHandled)
	{
		return iExit;
	}
	return Zenith_RunTestMain(iArgc, ppszArgv, TEST_OUTPUT_ROUTE_STDOUT);
}
```

The crash cases' child process is not dispatched by this main: it is the skipped doctest case `Test_CrashChild_RunScenario` (`* doctest::skip()`), launched by ZenithTestSupport's `Zenith_TestProcess::LaunchSelf` (`--test-case=Test_CrashChild_RunScenario --no-skip`, the scenario in the environment variable `ZENITH_CRASH_TEST_SCENARIO`, standard output captured), which [Z2-N-ARCH-162] makes the one legal process boundary for test code (ruling X349 (a), `Zenith2/Docs/Design/CORE/Memory.md` → "Test child processes"). The child prints `crash-child-pid: <pid>` (flushed) before its scenario so the parent can find the files carrying its PID; a child whose scenario fails before its crash (a `Create` error) fails its case with one message, which the parent prints from the captured output. No test file calls a platform API: the Windows-realisation assertions (handle inheritance, the five-handle launch description, the helper's ready flag) go through `Zenith_CrashHandlerTestAccess`, whose bodies are ZenithPlatform's `Internal/Windows/Zenith_CrashHandlerTestAccess_Windows.cpp` and return "unsupported" from the `Internal/Android/` and `Internal/Linux/` files, and each such case selects itself at run time (`if (!Zenith_CrashHandlerTestAccess::IsWindowsRealisation()) { MESSAGE("…"); return; }`), as Z2T-CORE-106's cases do.

Defining `main` in the target keeps the linker from extracting `ZenithTestSupport`'s `Zenith_TestMain.o`; row DV29 (b) asks CORE to state that a test target may supply such a forwarding main.

`Zenith2/Engine/ZenithProfiling/Tests/Support/Zenith_ProfilerTestSupport.h` is header-only ([Z2-N-ARCH-255]); every helper is `inline` or a template:

```cpp
// Installs a profiler's hooks (and optionally a log router's) for the scope; restores the previous table on exit.
// The table is a plain struct: install it before any thread that can dispatch exists (the profiler's flush thread
// dispatches only after a segment is queued, which StartCapture orders after the install) and uninstall it after those
// threads are joined; a test that needs its own table (a counting wrapper) installs it before Create starts the flush thread.
class Zenith_ScopedProfilerInstall
{
public:
	explicit Zenith_ScopedProfilerInstall(Zenith_Profiler& xProfiler);
	~Zenith_ScopedProfilerInstall();
};
// An in-memory Zenith_ProfilerOutput: keeps every written file by name; can refuse the n-th write.
class Zenith_TestProfilerOutput final : public Zenith_ProfilerOutput { /* inline members */ };
// An in-memory Zenith_DevFileWriter with the same name validation and the same two limits (64 MiB, eight pending) as the
// runtime implementation; Complete() finishes the oldest pending write.
class Zenith_TestDevFileWriter final : public Zenith_DevFileWriter { /* inline members */ };
// A capturing Zenith_ProfilerGpuSink: keeps a copy of every submission (names copied) for RPROD's, RHI's and DEVT's tests.
class Zenith_TestProfilerGpuSink final : public Zenith_ProfilerGpuSink { /* inline members */ };
// An in-memory Zenith_EvidenceFileWriter: one growing byte array; can refuse the n-th Append.
class Zenith_TestEvidenceFileWriter final : public Zenith_EvidenceFileWriter { /* inline members */ };
// Builds a synthetic frame: begins and ends nZones zones on nThreads threads, then calls EndFrame.
inline void Zenith_TestEmitFrame(Zenith_Profiler& xProfiler, uint32 uThreads, uint32 uZonesPerThread);
```

The four helper classes implement DEVT's own interfaces (`Zenith_ProfilerOutput`, `Zenith_DevFileWriter`, `Zenith_ProfilerGpuSink`, `Zenith_EvidenceFileWriter`), which are interfaces; no test replaces a concrete engine class, and the header is header-only like every non-Core `Tests/Support/` (X184).

## ZenithProfiling unit tests

`Tests/Zenith_Profiler_Tests.cpp`, `Tests/Zenith_ProfilerNameTable_Tests.cpp`, `Tests/Zenith_ProfileZone_Tests.cpp` (the per-thread ring is CORE's `Zenith_SpscRing`, whose oracle, property and stress tests are CORE's, `Zenith2/Docs/Design/AUDIO.md` → "Assumed interfaces pending other areas"; ruling Q80). Every stress test below runs three sub-cases, with 2, 8 and `Zenith_GetHardwareConcurrency()` threads ([Z2-N-CONTRACT-357]):

| Test | Asserts |
|---|---|
| `Test_Profiler_FillHooks_WritesProfilerMembersOnly` | the six profiler members and `m_pUserData` are set (in a `ShippingTest` build the lock pair and the blocking-wait member stay null, both of the pair null); the three log members are untouched |
| `Test_Profiler_ZoneBeginEnd_AssembledIntoFrame` | one zone per begin/end pair with the right lane, depth, start ≤ end, name |
| `Test_Profiler_DynamicNameCopiedAtCall` | a zone named from a buffer overwritten immediately after the begin keeps the original name |
| `Test_Profiler_NameCutAt63Bytes` | a 100-byte name is stored as 62 bytes plus `~` |
| `Test_Profiler_ZoneValue_FirstKeptMultipleFlagged` | one value sets `HAS_VALUE`; two set `MULTIPLE_VALUES` and keep the first; the capture keeps both |
| `Test_Profiler_ZoneValueWithoutZoneIgnored` | a value with no open zone changes nothing |
| `Test_Profiler_CountersOnlyMode_DropsZonesKeepsCounters` | in counters-only mode no zone is assembled, counters still aggregate |
| `Test_Profiler_ModeSwitchMidZone_EndsBalanced` | a zone begun in capture mode and ended after a switch to counters-only is assembled whole with no repair flag; a zone begun in counters-only mode and ended after a switch to capture appends nothing and counts no orphan end |
| `Test_Profiler_TracyStartedMidZone_StackBalanced` | with a recording `Zenith_TracyBridgeTable` installed by `Zenith_ProfilerTestAccess::SetTracyBridge` (`Internal/Tracy/Zenith_TracyBridge.h`; the test defines its own recording functions, so no symbol is defined twice), a zone open when Tracy starts ends without popping Tracy's stack; a zone begun after the start pushes and pops once and passes the site's persistent source location; in `ShippingTest` the table records no call and every zone still balances (§ "ShippingTest cases") |
| `Test_Profiler_SiteZeroNeverCached` | a begin whose lookup times out (entry held "being written" through `Zenith_ProfilerNameTableTestAccess`) records site 0; after release the same call site records the real site |
| `Test_ProfileZone_MacroTwoScopesInOneBlock` | two `ZENITH_PROFILE_SCOPE` uses in one block and a third in a nested block compile under warnings-as-errors and assemble three zones with the right names and depths |
| `Test_Profiler_CountersOnly_GpuTimingsStillIngested` | in counters-only mode with `m_bGpuTimings` set, a submission yields GPU zones, `m_uGpuUs` and per-pass sums, and no CPU zone is assembled; with `m_bGpuTimings` clear nothing is ingested |
| `Test_Profiler_HookStress` | the stress sub-cases: every thread emits 10⁵ zones, values, counters and lock events while the main thread assembles frames; no drop below ring capacity, every zone well-formed, the hook path takes no mutex; `RegisterThreadLane` runs on the main thread while the threads claim, so the lane publication and the claim's binding CAS of Profiler.md "Thread lanes" are exercised, and every claiming thread is fresh, so `EndFrame` creates their automatic lanes (`linux-tsan` reports no race on any thread-state or lane-table field) |
| `Test_Profiler_CounterKinds_Cumulative` | frame value = change since the previous frame; latest = the published total |
| `Test_Profiler_CounterKinds_Delta` | frame value = the frame's sum; latest accumulates; a name ending `_delta` is shown verbatim |
| `Test_Profiler_CounterKinds_Gauge` | frame value = latest = the last published value |
| `Test_Profiler_CounterKindConflict_Counted` | a second kind for one name is applied with the first kind and counted |
| `Test_Profiler_CounterHistory_MinMaxMean` | over 200 frames the history covers the last 128 frame values exactly |
| `Test_Profiler_Lanes_RegisterIdempotent` | registering index 3 twice returns one lane; `BindCurrentThread` moves an explicitly bound thread's subsequent zones to its lane |
| `Test_Profiler_Lanes_BindByThreadName` | a thread named `Z2 Worker 02` that emitted zones before the registration of `Z2 Worker 02` moves to that lane at registration; a thread named after registration binds at claim; an unnamed thread gets an automatic lane named `Thread <id>`; 64 rounds race a registration against a claim of the same name and every round binds; on Android and Linux a registered 20-byte name binds the thread whose OS name is its first 15 bytes, and two registered names equal in their first 15 bytes log the ambiguity once |
| `Test_Profiler_Lanes_IndexZeroBindsCaller` | a registration of index 0 binds the calling thread whatever its OS name |
| `Test_Profiler_Lanes_FullReturnsNullAndLogsOnce` | the 57th CPU lane registration returns the null lane, counts `profiler.lanes_refused` and logs once; with all 56 CPU lanes taken, a new thread's state is placed by `EndFrame` on the overflow lane `<other threads>` (lane 64, created by `Create`), counted in `profiler.lanes_refused` and logged once, and its zones assemble on that lane |
| `Test_Profiler_Lanes_AutomaticLanesCreatedByAssembly` | 8 fresh threads, none matching a registered name, claim concurrently while the main thread assembles: after the next `EndFrame` every state has its own distinct automatic lane, named from its OS name or `Thread <id>`, created on the main thread (the claiming threads never write the lane table), and `linux-tsan` reports no race; a state that a registration bound by name during the same frame keeps the registered lane |
| `Test_Profiler_ThreadStatesExhausted_DropsAndWarnsOnce` | with 4 states, a 5th thread's zones are dropped, counted, and one warning is logged: by the next `EndFrame` on the main thread, never by the claiming thread (the 5th thread holds a level-990 lock when it first records, and no lock-order violation or log record happens in the hook); the test then runs five more frames while that thread keeps recording and exactly one warning has appeared in all (the `m_bExhaustionWarned` latch), and a claimant whose thread ID is 0 is recorded under the sentinel and warned about once |
| `Test_Profiler_RingFull_DropNewestAndRepair` | with a 1,024-record ring, 2,000 begin/end pairs in one frame drop the newest, mark the frame `TRUNCATED`, and leave every assembled zone well-formed |
| `Test_Profiler_OpenZoneCarriedAcrossFrames` | a zone begun in frame 1 and ended in frame 3 is emitted in frame 3 with `TRUNCATED_START` |
| `Test_Profiler_FrameSummary_Fields` | the summary's CPU sim, render and frame times equal the inputs' stamps; the hitch flag follows the threshold |
| `Test_Profiler_HistoryRing_600Frames` | `GetFrameSummary(599)` is the oldest of 600 frames; `GetFrameSummary(0)` the newest |
| `Test_Profiler_DetailedFrames_FreezeHolds` | frozen, the eight held frames stay readable while 20 more frames are assembled and the history advances |
| `Test_Profiler_ZoneStats_CountTotalMax` | per-site count, inclusive total and maximum over a frame equal a hand-computed oracle |
| `Test_Profiler_ZoneStats_LoopAndCallPathsAccumulate` | one function holding one `ZENITH_PROFILE_SCOPE` bracket, called 100 times in a loop and once each from two other zones at depths 1 and 2, then one `EndFrame`: `GetZoneStats` has one entry for the site with count 102, a total equal to the sum of the 102 durations and a maximum equal to the largest, the oracle read from the frame's detailed zones (`GetFrame(0)`); a second source line using the same literal adds into the same entry; a `ZENITH_PROFILE_SCOPE_NAMED` bracket entered 3 times with `"A"` and twice with `"B"` gives two entries, counts 3 and 2 |
| `Test_Profiler_ZoneStats_TwoThreadsAccumulate` | two threads each enter the same bracket 1,000 times inside one frame, overlapping in time: one entry, count 2,000, total equal to the sum of all 2,000 durations on both lanes (it may exceed the frame time) and maximum the largest of them |
| `Test_Profiler_ZoneStats_RecursiveSiteNotDoubleCounted` | a function that recurses 4 levels through one bracket: count 4, total equal to the outermost zone's duration (not the sum of the 4 nested durations), maximum the outermost; two such recursions in one frame give count 8 and the sum of the two outermost; a site A entered inside B inside A adds only the outer A to A's total, while B's total is B's duration; a lock zone of one name acquired inside a lock zone of the same name adds only the outer one |
| `Test_Profiler_ZoneStats_NextFrameStartsAtZero` | frame 1 closes the site 5 times; frame 2 closes it 0 times and its `GetZoneStats` has no entry for it (a reader sees count 0, total 0); frame 3 closes it twice and reports count 2 and the total of those 2 closures alone, not 7 |
| `Test_Profiler_ZoneStats_ResetOncePerFrame` | within one `EndFrame` the closures of one site from 3 thread states, 5,000 of them on one thread (more than one 4,096-event chunk), a repaired `TRUNCATED_END` zone and a carried zone ending in this frame all land in one entry whose count is their number; `GetZoneStats` read twice between two `EndFrame` calls returns equal spans; an `EndFrame` with no closure publishes an empty span; over 10 frames the sum of the site's counts equals the closures emitted, the carried zone counted once, in its end frame |
| `Test_Profiler_Gpu_SubmissionBecomesGpuZones` | a calibrated submission of 3 passes yields 3 GPU zones on the graphics lane and `m_uGpuUs` the span |
| `Test_Profiler_ZoneStats_GpuPassesAccumulate` | a calibrated submission with the pass `Blit[Main]` 3 times on the graphics queue and once on the compute queue gives one entry with count 4, the sum of the 4 durations and the largest as maximum; an older submission of the same queue drained by the same `EndFrame` adds nothing; the next `EndFrame` without a submission has no entry for the pass; the same holds in counters-only mode |
| `Test_Profiler_Gpu_UncalibratedAlignedToRenderStart` | an uncalibrated submission's first pass starts at the frame's render start |
| `Test_Profiler_Gpu_QueueFull_DroppedAndCounted` | the 9th submission before an `EndFrame` returns `PROFILER_ERROR_GPU_QUEUE_FULL` |
| `Test_Profiler_LockEvents_OnlyWhenEnabled` | with `captureLocks` off no `lock:` zone appears; on, an acquire–release pair becomes one; an acquire dispatched with `ulWaitTicks` above 0 also yields a `lockwait:<name>` zone ending at the acquire, and one dispatched with 0 yields none |
| `Test_Profiler_OwnLockNotRecorded` | a counting-only hook table (acquire, release and blocking-wait counters by name; no profiler member) is installed before `Create` starts the flush thread and uninstalled after the profiler is destroyed; a capture run through a `Zenith_TestProfilerOutput` with the real flush thread and `captureLocks` on produces no acquire, release or blocking-wait call from the segment-queue mutex or the flush thread's semaphore: the counts by name hold only the test's own locks, none at level 940 |
| `Test_Profiler_BlockingWait_MarkerZone` | `Zenith_DispatchBlockingWait("debugger-stop", 0)` yields a zero-length `wait:debugger-stop` marker |
| `Test_Profiler_BlockingWait_DurationZone` | `Zenith_DispatchBlockingWait("Test.Wait", 7000)` yields a `wait:Test.Wait` zone of 7,000 ns ending at the event; three such waits of 7,000, 3,000 and 9,000 ns in one frame give one `GetZoneStats()` entry for the site with count 3, total 19,000 and maximum 9,000; a wait of three frames' length dispatched in a later frame (and a contended acquire of the same length) is emitted in that frame with `TRUNCATED_START` on its `wait:` (and `lockwait:`) zone, and its full duration is added to the statistics of the frame in which it ends |
| `Test_Profiler_LockStats_ManyInstancesOneEntry` | 200 `Zenith_LockDesc` objects at 200 addresses, all with one name literal and level, each acquired and released once in one frame on one thread: `GetLockStats()` has one entry with acquisitions 200, `profiler.lock_stats_dropped_delta` is 0 and the bank holds one entry; a second literal of the same text at another address adds a second per-thread entry that merges into the same site |
| `Test_Profiler_LockStats_CondVarWaitEndsHold` | the dispatch sequence of a condition-variable wait (release, a blocking wait of 5 ms, acquire with wait 0) after an acquire 1 ms earlier: the first hold is about 1 ms, the second hold starts after the wait, and no hold covers the 5 ms; `wait:<primitive>` has the 5 ms |
| `Test_Profiler_OwnPrimitivesAreNoProfiling` | through `Zenith_ProfilerSyncTestAccess` (`Internal/Capture/Zenith_ProfilerSync.h`), `static_assert`s with `std::is_same_v` that the segment-queue mutex is a `Zenith_MutexNoProfiling` and the flush thread's semaphore a `Zenith_SemaphoreNoProfiling`; at run time a counting table installed before `Create` and uninstalled after the destructor sees no call during `Create`, a capture with one flushed segment, and the destructor |
| `Test_Profiler_OwnZonesRecorded` | with a running capture, the zone `Profiler.Assemble` is on the main thread's lane and `Profiler.FlushSegment` on the `Zenith.ProfFlush` lane in a later frame: both are emitted through the dispatch functions and survive `m_bInProfiler` |
| `Test_Profiler_LockStats_AccumulateOverUses` | one lock name used by three `Zenith_LockDesc` objects at three addresses, 100 times in a loop and 2 times from each of two other call paths, and a second name used once: `GetLockStats()` has one entry for the first name with acquisitions 106, contended equal to the number of dispatches with a wait, total wait equal to the sum of the waits dispatched, maximum wait the largest, total hold equal to the sum of the durations of the frame's `lock:<name>` zones (`GetFrame(0)`) and maximum hold the largest, and one entry for the second name |
| `Test_Profiler_LockStats_ResetAtEndFrame` | frame 1 uses the site 5 times; frame 2 none and `GetLockStats()` has no entry for it; frame 3 twice and the entry reports count 2, not 7; within one `EndFrame` the uses from 3 thread states land in one entry; `GetLockStats()` read twice between two `EndFrame` calls returns equal spans |
| `Test_Profiler_LockStats_ContendedWaitAndZone` | an acquire dispatched with `ulWaitTicks` 250,000 gives contended 1, total wait and maximum wait 250,000, and a `lockwait:<name>` zone of 250,000 ns on the thread's lane ending where the `lock:<name>` zone begins, both flagged with the thread's depth; an uncontended acquire of the same site adds an acquisition, no wait and no `lockwait:` zone; `FindSite("lockwait:<name>")` is non-zero |
| `Test_Profiler_LockStats_HoldAcrossFrames` | an acquire in frame 1 and its release in frame 2: frame 1's entry has acquisitions 1 and hold 0, frame 2's entry has acquisitions 0 and the hold, and the `lock:<name>` zone is emitted in frame 2 |
| `Test_Profiler_LockStats_GatedByCaptureLocks` | with `captureLocks` off, and with `PROFILER_MODE_COUNTERS_ONLY`, 100 acquire–release pairs leave `GetLockStats()` empty, the summary's lock fields 0 and no lock zone; with the gate open the next frame has the entries |
| `Test_Profiler_LockStats_TableOverflowCounted` | one thread uses 60 distinct lock names in one frame: 48 entries, 12 refusals in `profiler.lock_stats_dropped_delta`, no allocation after `Create` (recording heap) and a `lock:` zone for every acquire; 17 nested acquires and releases: the 17th is untracked and its release counted as dropped |
| `Test_Profiler_LockStats_FrameSummaryFields` | the summary's `m_uLockContended` equals the sum of the entries' contended counts and `m_uLockWaitUs` their total wait in microseconds, over 3 sites on 2 threads; 70,000 contended acquisitions saturate `m_uLockContended` at 65,535; with the gate closed both are 0 |
| `Test_Profiler_LockStats_BankHandoverNoLoss` | the stress sub-cases: each of N threads dispatches 10⁵ acquire–release pairs (a quarter with a wait) on 4 shared lock names while the main thread calls `EndFrame` in a loop, plus a sub-case in which the main thread hands the banks over in a tight loop so that the active bank flips on almost every iteration and an owner that backs out meets a bank being cleared (`m_uWriters` never underflows, nothing is lost); after the threads join and two more `EndFrame` calls, the acquisitions, the contended counts and the waits summed over every frame's `GetLockStats()` equal the totals dispatched, `profiler.lock_stats_dropped_delta` summed is 0, and `linux-tsan` reports no race |
| `Test_Profiler_Capture_LocksChunkRoundTrip` | a 10-frame capture with lock statistics in 6 frames: `DecodeLocks` returns for each such frame entries equal to that frame's `GetLockStats()`, the `STRINGS` chunk lists every `nameId`, frames without lock activity carry no `LOCKS` chunk, and header flag bit 3 is set |
| `Test_Profiler_HooksAllocateNothing` | with the fault-injecting heap set to fail every allocation after `Create`, 10,000 hook calls on 4 threads succeed and allocate nothing |
| `Test_Profiler_HooksTakeNoMutex` | the lock-order checker records no acquisition during 10,000 hook calls on a thread holding a level-990 lock |
| `Test_Profiler_Create_FailsAtValidate_Unwinds`, `…AtThreadStates…`, `…AtNameTables…`, `…AtCounters…`, `…AtFrameStores…`, `…AtFlushThread…` | each fault point returns its error and leaks nothing ([Z2-N-CONTRACT-341]) |
| `Test_Profiler_Destroy_JoinsFlushThread` | the destructor returns with the flush thread joined and every queued segment written or dropped |
| `Test_Profiler_Capture_FormatRoundTrip` | a 30-frame capture written to a test output is read back by `Zenith_ProfilerCaptureReader` with every zone, counter, lane, marker and memory snapshot equal |
| `Test_Profiler_Capture_SegmentsSelfContained` | each segment of a 3-segment capture parses alone and carries its own `STRINGS` |
| `Test_Profiler_Capture_RefusedWriteStops` | a refused segment write stops the capture with the logged message |
| `Test_Profiler_Capture_WriterBehindStops` | a blocked output with two segments queued stops the capture |
| `Test_Profiler_Capture_ReaderRejectsCorrupt` | truncated header, length overflow, counts beyond the bytes, a newer version: each returns its error and never reads out of bounds |
| `Test_Profiler_Capture_UnknownChunkSkipped` | a chunk of kind 200 is skipped by length |
| `Test_Profiler_Capture_MemorySnapshotEvery60Frames` | a 130-frame capture holds three `MEMORY` chunks and one per level change |
| `Test_Profiler_MemoryConsumer_ResidentBytesMatchesAllocations` | for the Windows and the Android desc of `Zenith_GetPlayerProfilerDesc`, `GetResidentBytes` equals the sum the test computes component by component from the desc and the constants of `Zenith_ProfilerTypes.h` (the rows of `Zenith2/Docs/Design/DEVT/Profiler.md` → "Memory layout and accounting"; no literal total) and equals the bytes the test's recording heap recorded for the profiler minus the one allocation of the `Zenith_Profiler` object itself (`sizeof(Zenith_Profiler)`, which `GetResidentBytes` does not count); pressure changes shed nothing |
| `Test_Profiler_Tracy_NotStartedWhenUnavailable` | with a recording bridge table installed and `m_bTracyAvailable` false, `ApplySettings` with Tracy enabled calls no table member and allocates no source-location array |
| `Test_Profiler_Gpu_SlotsRecycledThroughIndexRings` | 20 frames each submitting from a second thread (the sink's one caller at a time) and drained by `EndFrame`: every slot index returns to the free ring, no index is ever held by both rings, and `linux-tsan` reports no race on a slot's bytes |
| `Test_Profiler_EndDroppedRepairAcrossThreads` | an owner thread whose ring is full drops an end (setting `m_bEndDropped`) while the main thread assembles; the zones open on that thread are closed with `TRUNCATED_END` in the frame that drains after the flag was set, and the flag is clear afterwards |
| `Test_Profiler_RingFull_CountsDroppedTriple` | a thread whose ring is full drops the newest events; `profiler.rings.dropped` equals the refused pushes and `.high_water` the ring capacity |
| `Test_ProfilerNameTable_MatchesOracle` | seeded random `FindOrInsert` sequences of resident and new names, at least 10,000 operations per seed and 16 seeds, against a `std::unordered_map<std::string, uint32>` oracle: the same IDs, the same full-table refusals; the failing seed and operation index printed ([Z2-N-CONTRACT-288]) |
| `Test_ProfilerNameTable_Properties` | over generated name sets (seed printed): IDs are dense from 1, a published name's bytes never change, a hash collision with different text gets a distinct ID, the arena offset of every entry is within the arena, the 16,385th distinct site returns 0 and counts ([Z2-N-CONTRACT-289]); every published name is followed in the arena by a NUL byte (Tracy reads it as a C string); over a 256-byte arena, the name that does not fit leaves its entry in state 3 and returns 0, and every subsequent lookup of it on 4 threads returns 0 with zero spin iterations (`Zenith_ProfilerNameTableTestAccess::GetSpinIterations()`) while names published before it still resolve; with the profiler's own constants, (16,384 + 4,096 + 64) distinct 63-byte names, each stored with its terminator (64 bytes), fill the arena exactly and all fit |
| `Test_ProfilerNameTable_SizeOverflow_NotPowerOfTwo` | `Init` with a capacity of 1,000 (not a power of two) fails its `ZENITH_CHECK` (`z2-death-check`, [Z2-N-CONTRACT-290]); there is no `Try` operation, so no `TryFailure` test exists (`Zenith2/Docs/Design/DEVT/Profiler.md` → "Name tables") |
| `Test_ProfilerNameTable_SizeOverflow_AboveMaximum` | `Init` with a capacity of 131,072 (above 65,536) fails its `ZENITH_CHECK` (`z2-death-check`) |
| `Test_ProfilerNameTable_SizeOverflow_ExceedsStorage` | `Init` with a capacity of 1,024 over storage of 1,023 × 32 bytes fails its `ZENITH_CHECK` (`z2-death-check`); each of the three cases dies once in its own process |
| `Test_ProfilerNameTable_StressConcurrentInsert` | the stress sub-cases: every thread inserts the same 1,000 names and 1,000 names of its own; one ID per distinct name, no torn name |
| `Test_DevToolsConfig_RegistersEveryKey` | every key of the table in `Zenith2/Docs/Design/DEVT.md` → "Configuration keys" is registered with its type, range, default, mask and restart flag |
| `Test_DevToolsConfig_ShippingMasks` | `Zenith_RegisterDevToolsConfigKeys(xSchema, ePlatform, BUILD_CONFIGURATION_SHIPPING)` gives every non-Shipping key the engine-default layer alone and its off value, `devtools.telemetry.enabled` and `crash.enabled` their Shipping defaults; with `BUILD_CONFIGURATION_RELEASE` the table's masks |
| `Test_DevToolsConfig_ShippingGpuTimingsFollowEnabled` | `Zenith_ResolveProfilerSettings` with `BUILD_CONFIGURATION_SHIPPING` gives `m_bGpuTimings` the value of `devtools.profiler.enabled` for each of the four combinations of the two keys; with `BUILD_CONFIGURATION_RELEASE` it gives `devtools.profiler.gpuTimings`'s value |

`Tests/Zenith_Profiler_DefectTests.cpp`: `Test_ProfilerDefect_EndFrameOffMainThread` and `Test_ProfilerDefect_DestroyWhileInstalled` (`z2-death-assert`), `Test_ProfilerDefect_RingIndexCorruption` (`z2-death-check`, a `ZENITH_CHECK` on an assembler thread-state index beyond capacity); § "Death cases" classifies every DEVT death case by its macro.

## Overlay tests

`Tests/Zenith_ProfilerOverlay_Tests.cpp`, `Tests/Zenith_ProfilerExpression_Tests.cpp`, `Tests/Zenith_ProfilerPanels_Tests.cpp` (the overlay draws into a real `Zenith_DebugDraw`; assertions read the drained batches):

| Test | Asserts |
|---|---|
| `Test_ProfilerOverlay_StateCycle` | toggle cycles hidden → compact → full → hidden; `SetState` and the configuration key set it |
| `Test_ProfilerOverlay_HiddenDrawsNothing` | no record after `Update` in `HIDDEN` |
| `Test_ProfilerOverlay_CompactDrawsTopBarOnly` | every record lies inside the top bar's row |
| `Test_ProfilerOverlay_PanelNavigationWraps` | next from the last tab selects the first; previous from the first selects the last |
| `Test_ProfilerOverlay_TimelineZonesMatchFrame` | one rectangle per zone wider than a pixel, at x positions proportional to start times; sub-pixel zones merged |
| `Test_ProfilerOverlay_GpuViewLanes` | one lane per queue with GPU zones; the uncalibrated label appears for an uncalibrated frame; a pass occurring twice in the GPU frame is one table row with count 2 and the sum of both durations |
| `Test_ProfilerOverlay_LockTable` | a frame with three lock sites draws a row per site sorted by total wait then total hold, with the percentage, the summary header and the `live` mark when frozen; pressing `[locks]` runs `set devtools.profiler.captureLocks true` and a second press `false` on a recording console, and the button starts on when the key is already true; with `captureLocks` off it draws the off text and with no activity `no lock activity`; 20 sites draw 12 rows and scrolling moves the window |
| `Test_ProfilerOverlay_HistoryHitchMarkers` | a frame above the threshold draws a red bar and a marker; an excluded frame draws dim |
| `Test_ProfilerOverlay_HistoryStatistics` | mean, p50, p95, p99 and max over a fixture series equal the nearest-rank oracle |
| `Test_ProfilerOverlay_FreezeAndSelect` | frozen, selecting held frame 5 shows its zones while new frames arrive |
| `Test_ProfilerOverlay_HitTestMatchesLayout` | `HitTest` is true inside drawn regions and false outside |
| `Test_ProfilerOverlay_RecordBudgetInFull` | at 10,000 zones the overlay records at most 6,000 rectangles, 4,000 lines and 1,500 text records with 12,000 glyphs, and after one `DrainInto` every recorded record is in the batches with `m_ulDropped` unchanged (nothing lost between recording and drain) |
| `Test_ProfilerOverlay_ScaleFromDisplay` | scale 1 at 1080p and 1440p, 2 at the S24's 1440-pixel height with display scale 1.5 |
| `Test_ProfilerExpression_Grammar` | the expressions of `Tests/Fixtures/Expressions/valid.txt` compile; each line of `invalid.txt` fails at its recorded column with its recorded message |
| `Test_ProfilerExpression_EvaluatesCountersAndFunctions` | `sum`, `each`, `count`, `zone_ms`, `zone_count`, `zone_max_ms`, `frame_field`, `hmin`, `hmax`, `hmean`, arithmetic over a fixture profiler equal hand-computed values; over a fixture frame in which one site closed 4 times and a second site matching the same pattern twice, `zone_count` of the pattern is 6, `zone_ms` the sum of both totals and `zone_max_ms` the largest single closure, and a site with no closure in the frame reads 0 |
| `Test_ProfilerExpression_MissingCounterMarked` | an unpublished name evaluates to 0 with `m_bMissing`; it resolves once published |
| `Test_ProfilerExpression_DivisionByZeroUndefined` | yields 0 with `m_bUndefined` |
| `Test_ProfilerPanels_CatalogueCompiles` | every row of `Zenith_GetBuiltInPanels()` compiles; panel IDs are unique and match the ID grammar |
| `Test_ProfilerPanels_ProviderCalledOnlyWhenVisible` | a provider's `Build` runs only while its tab is selected; its token removes the tab |
| `Test_ProfilerPanels_ExactnessShown` | a row with exactness "may spill" draws its mark |

## Console tests

`Tests/Zenith_DevConsole_Tests.cpp`:

| Test | Asserts |
|---|---|
| `Test_DevConsole_CreateRefusedInShipping` | in `ShippingTest`, `Create(Zenith_DevConsoleDesc{})` returns `DEV_CONSOLE_ERROR_NOT_IN_SHIPPING`; in every other configuration it succeeds with only the built-ins registered (`GetCommandCount()` 6) |
| `Test_DevConsole_RegisterAndExecute` | a registered handler receives its arguments and user data; its output enters the history |
| `Test_DevConsole_TokenUnregisters` | after the token's destruction the name is unknown; a moved-from token is inert |
| `Test_DevConsole_NameGrammar` | `jobs.dump`, `r.pso.dumpMisses` and `help` accepted; `Jobs.dump`, `jobs..dump`, a 65-byte name rejected with `DEV_CONSOLE_ERROR_INVALID_NAME` |
| `Test_DevConsole_DuplicateAndFull` | a duplicate name fails; the 1,025th command fails `REGISTRY_FULL` |
| `Test_DevConsole_GrammarQuotesEscapesSeparators` | the lines of `Tests/Fixtures/Console/lines.txt` tokenise into their recorded tokens |
| `Test_DevConsole_ParseErrorsRunNothing` | an unterminated quote in the second statement means the first statement does not run |
| `Test_DevConsole_LimitsEnforced` | a 1,025-byte line, 33 tokens, 17 statements: each refused with its error |
| `Test_DevConsole_FailureStopsLineUnlessContinue` | a failing statement skips the rest; a `?`-prefixed next statement runs |
| `Test_DevConsole_NestedExecuteDepthLimit` | depth 4 runs; depth 5 fails |
| `Test_DevConsole_RegisterInsideHandlerDeferred` | a registration inside a handler takes effect after the outermost `Execute` |
| `Test_DevConsole_QueueLineFromThreads` | the stress sub-cases: the threads queue 64 lines between them; `ExecuteQueued` runs all 64 once each; the 65th before a drain fails `QUEUE_FULL` and is counted |
| `Test_DevConsole_SetRefusesSimulationAffecting` | `set` of a simulation-affecting key replies the refusal and leaves the value |
| `Test_DevConsole_SetRefusesRestartRequired` | likewise for a restart-required key |
| `Test_DevConsole_SetAppliesCommandLineLayer` | `set` of an allowed key changes the resolved value with provenance `console` and increments the store's revision |
| `Test_DevConsole_SetRefusalFromDiagnostics` | `set` of an out-of-range value, a wrong type and a key whose mask lacks the command-line layer each replies `refused: <message>` from the store's new diagnostic and leaves the value |
| `Test_DevConsole_SetDispatchesChangeRegistry` | a changing `set` fires the change registry once; a refused or unchanged `set` fires nothing (row DV22) |
| `Test_DevConsole_SetShadowsUserLayer` | after `set`, a user-layer commit of the same key leaves the console's value resolved, and the reply carries the shadowing note |
| `Test_DevConsole_HasCommandAndCount` | `HasCommand` and `GetCommandCount` reflect registration, token destruction and deferred registration inside a handler |
| `Test_DevConsole_GetShowsProvenance` | `get` prints the value, layer and source |
| `Test_DevConsole_CompleteCommandsAndKeys` | completion returns names in byte order |
| `Test_DevConsole_OutputCut` | output beyond 64 KiB is cut with the marker |
| `Test_DevFileWriter_NameRules` | the test writer and the runtime writer accept and refuse the same names (shared fixture list) |
| `Test_LogFileSink_ChunkFormat` | 1,000 records produce chunk files whose first line is the header and whose lines parse as `zenith2.log-record` with the record's fields |
| `Test_LogFileSink_ChunkRotationAndRetention` | a 64 KiB chunk size rotates at the boundary; the 65th chunk replaces the oldest of the run |
| `Test_LogFileSink_RefusedWriteCountsDrops` | a refusing output counts the dropped lines |
| `Test_LogFileSink_NoWriteAfterDetach` | after `Detach` a logged record produces no `WriteFile`; the chunk `Detach` wrote holds every record logged before it |
| `Test_LogFileSink_RetentionKeepsNewestEightRuns` | with the files of 10 earlier runs on a scratch user mount, startup removes the 3 oldest runs' files and keeps the rest |
| `Test_LogFileSink_ShippingWarningOnly` | a sink created with `m_eConfiguration = BUILD_CONFIGURATION_SHIPPING` writes a `WARNING` record and not an `INFO` record |
| `Test_LogFileSink_ShippingFloorFromBuildInfo` | a sink created with `m_eConfiguration = Zenith_GetBuildConfiguration()`, the composition root's value: in `ShippingTest` it writes the `WARNING` record and not the `INFO` record, and `devtools.log.file`'s registered default is false; in every other configuration it writes both |
| `Test_DevFrameDump_SchemaAndSections` | the dump parses, carries `schemaVersion` 1, the profiler section and every registered section; a failing section sets `truncated`; in `ShippingTest` `Create` returns `PROFILER_ERROR_NOT_IN_SHIPPING` and the case ends there |

`Engine/ZenithRuntime/Tests/DevTools/Zenith_DevTools_Tests.cpp`:

| Test | Asserts |
|---|---|
| `Test_DevToolsServer_HelloRequired` | a `COMMAND` before `HELLO` closes the connection with one `ERROR` |
| `Test_DevToolsServer_OversizePayloadCloses` | a header announcing 2 MiB closes the connection before reading the payload |
| `Test_DevToolsServer_CommandReply` | a `COMMAND` runs at the next `ExecuteQueued` and its `COMMAND_REPLY` carries the output |
| `Test_DevToolsServer_RemoteDisallowedRefused` | a command with `m_bRemoteAllowed` false is refused |
| `Test_DevToolsServer_FileGetRestricted` | `FILE_GET` of `../x`, `saves/a.zsave` and an absolute path are refused; `devtools/frames/1.json` is served in chunks |
| `Test_DevToolsServer_LogStreamBackpressure` | a slow client drops log lines of its stream with one "stream behind" error; capture segments are never dropped |
| `Test_DevToolsServer_CaptureStreamsSegmentsAndDone` | `CAPTURE_START` of 40 frames on a profiler with 1 MiB test segments yields `CAPTURE_SEGMENT` messages whose concatenated bytes equal the segments written to the user mount, each first message prefixed with its sequence and size, then one `CAPTURE_DONE` with the segment count |
| `Test_DevToolsServer_FrameSummaryStream` | a `FRAME_SUMMARY_SUBSCRIBE` with `everyFrames` 2 and two counters yields one 64-byte summary plus the two counter values every second frame; `{"stop":true}` ends it |
| `Test_RuntimeDevCommands_Registered` | every command of Console.md "Runtime development commands" is registered after startup step 13 |
| `Test_RuntimeDevCommands_EcsDumpArgumentForm` | `ecs.dump <tag> user:/devtools/e.json` writes the world's `zenith2.ecs-dump`; an unknown tag, a missing argument and `C:/x.json` are refused |
| `Test_RuntimeDevCommands_ExecRunsFileLines` | `exec fixture` replies at once; after the read completes the file's three lines run in order at the next step 3; a missing file logs the read error; a second `exec` while one is pending replies `exec: busy` |
| `Test_DevFileWriter_BusyAndTooLarge` | the ninth write while eight are pending is `DEV_FILE_ERROR_BUSY`; a 64 MiB + 1 byte write is `DEV_FILE_ERROR_TOO_LARGE`; after a reap a write succeeds |
| `Test_OverlayInputAdapter_ClaimsPointersOverOverlay` | (Stage 3, PLAT's input system driven by injected transitions) a press inside a drawn overlay region is claimed and absent from the game's frame; a press outside reaches the game |
| `Test_OverlayInputAdapter_ClaimsScrollOverScrollable` | `UI.Scroll` is claimed over a scrollable panel and not elsewhere |
| `Test_OverlayInputAdapter_NothingClaimedWhenHidden` | in `HIDDEN` no pointer and no action is claimed |
| `Test_DevTools_MemoryGaugeSumsConsumers` | `devtools.memory_bytes` equals the profiler's plus `Zenith_DevToolsMemoryConsumer`'s resident bytes, and is unchanged across WARNING and CRITICAL |
| `Test_DevTools_DetachOutputsBeforeAsyncIO` | `DetachOutputs` leaves no pending write and no output pointer in the profiler, the file sink or the telemetry; a log line and a telemetry event after it write nothing; an `ecs.dump` write and an `exec` read submitted before `BeginShutdown` are cancelled by it and complete (cancelled or written) before `DetachOutputs` returns, with their buffers freed |
| `Test_DevTools_ShippingCreatesNoDevServices` | the runtime's `Zenith_DevTools` created over a headless fixture runtime with `devtools.remote.enabled=true` and `devtools.telemetry.enabled=true` on the command line: in `ShippingTest` it holds no console (`GetDevConsole()` null), no file writer, no frame dump, no remote server and no telemetry, sets no profiler output, and still holds the overlay and the debug-draw recorder, both disabled; in every other configuration the console, the file writer, the frame dump and the telemetry exist and the server is listening |
| `Test_BudgetRunDriver_ReadsInputsWritesRecord` | with fixture `rows.json` and `fingerprint.json` in a temporary evidence directory, a headless 200-frame run (minimums overridden by the test desc) writes a record that `Test-Zenith2EvidenceRecord`'s schema rules accept |

The remote-protocol decoder is also fuzzed by `Zenith_Fuzz_RemoteFrame` (seed corpus of valid and truncated frames).

## Debug-draw tests

`Tests/Zenith_DebugDraw_Tests.cpp` and `Engine/ZenithFlux/Tests/Debug/Flux_DebugOverlay_Tests.cpp`:

| Test | Asserts |
|---|---|
| `Test_DebugDraw_ShapeExpansionCounts` | line, point, arrow, box, AABB, sphere, capsule, axes and frustum produce 1, 3, 5, 12, 12, 72, 100, 3 and 12 line records |
| `Test_DebugDraw_FrustumCornersBothDepthModes` | the 12 edges recorded for a standard-Z projection and for the reversed-Z projection of the same frustum are the same set of world-space segments, and match the hand-computed corners; for `Zenith_MakePerspectiveReversedZInfinite` the call records 8 finite edges, 4 near edges and 4 side edges 1,000 m long pointing away from the eye; a matrix with one NaN element is dropped and counted |
| `Test_DebugDraw_FullOverlayFrameDrainsWhole` | the overlay's `FULL` budget plus the console's (11,628 records, 16,000 text bytes) recorded on the main thread drain in one `DrainInto` with nothing dropped |
| `Test_DebugDraw_MainRingOverflowCounted` | 20,000 lines on the main thread drop the newest 3,616, counted in the main-ring triple, one warning |
| `Test_DebugDraw_StressRecordAndDrain` | the stress sub-cases: every thread records 10⁴ lines per iteration over 100 drains on worker threads; none lost below capacity |
| `Test_DebugDraw_ShapeDroppedWhole` | a sphere into a ring with 50 free records is dropped entirely |
| `Test_DebugDraw_DurationExpires` | a 1-s record survives drains totalling 0.9 s and is gone after 1.1 s |
| `Test_DebugDraw_DurationExpiresAfterLongUptime` | the same with the drain clock started at 10^6 s (`Zenith_DebugDrawTestAccess::SetDrainClockNs`) |
| `Test_DebugDraw_RefusedThreadNeverSharesRing` | with 3 other rings, 6 threads draw 1,000 lines each: at most 3 own a ring, no ring receives two threads' records, every refused thread's lines are counted dropped, and `Zenith_DebugDrawSlotsTestAccess::GetRingClaims()` is at most 6 (DebugDraw.md "Recorder slot alternative") |
| `Test_DebugDraw_BatchesSortedByWorldTag` | records of tags 3, 0, 7 recorded interleaved drain as three batches in tag order, stable within a tag |
| `Test_DebugDraw_ConcurrentRecorders` | 8 threads × 1,000 lines all drain, none lost below capacity |
| `Test_DebugDraw_NonFiniteDropped` | a NaN endpoint is dropped and counted; a line with a NaN `fDuration`, one with +infinity and one with −1 are each recorded with `m_uDurationMs` 0, drawn in this frame's drain only and absent from the next (DebugDraw.md "Durations and world tags") |
| `Test_DebugDraw_DisabledDropsAtFirstBranch` | with `SetEnabled(false)` nothing reaches a ring |
| `Test_DebugDraw_TextCut` | 300-byte text is stored as 254 bytes plus `~` |
| `Test_DebugFont_AtlasMatchesGlyphTable` | the atlas bytes decode to the 95 glyph bit patterns |
| `Test_DebugOverlay_RecordLayoutsMatchShader` | `sizeof` and member offsets of `Flux_DebugLineSegment` and `Flux_DebugQuad` equal the reflection of `DebugDrawRecords.slang` |
| `Test_DebugOverlay_NoPassWithoutRecords` | a view with no debug records adds no pass to the graph |
| `Test_DebugOverlay_UploadSizeAndCaps` | 70,000 lines are cut to 65,536 and counted; the upload is lines × 32 + quads × 32 bytes |
| `Test_DebugOverlay_ExtractorSplitsRecords` | 5,000 lines become three render-world records of at most 2,048 lines |
| `Test_DebugOverlay_ScreenTagZeroInEveryView` | tag-0 records reach the primary section with `m_bAllViews` and appear in a second world's view |
| `Test_DebugOverlay_NoPassDefinitionAddsNothing` | the API-only slice's definition of `Flux_AddDebugOverlay` adds no pass and links from RPROD's epilogue |

## Crash tests

`Engine/ZenithPlatform/Tests/Zenith_CrashHandler_Tests.cpp` and `Tests/Zenith_CrashHandlerDefect_Tests.cpp`. Every case is an ordinary doctest case; none is a death-test suite entry, because Contracts' `z2-death-check` passes only when the output matches `ZENITH (CHECK|FATAL|UNREACHABLE) FAILED` (`Zenith2/Docs/Contracts.md` → "Tests and code hygiene gates", table "Death and sanitizer-defect tests"), which a null write, `abort()`, a stack overflow or a second crash never prints. The five `Test_CrashDeath_*` cases instead crash a child process and inspect what it left:

1. The case launches the child through `Zenith_TestProcess::LaunchSelf` with `m_szTestCase = "Test_CrashChild_RunScenario"`, the variable `ZENITH_CRASH_TEST_SCENARIO=<scenario>` and `m_bCaptureStdout` true (no inherited handle), and reads the child's PID from its first output line `crash-child-pid: <pid>`.
2. The child (`Test_CrashChild_RunScenario`) builds what `Create` needs, exactly as the composition root does but with no runtime: a `Zenith_LogRouter` with no sink, the user mount through `Zenith_VirtualFileSystem::Create` with `m_strGameName = "ZenithPlatformTests"` (the identity the helper derives its directory from, Crash.md "Crash helper security contract"), a `Zenith_CrashHandlerDesc` with that identity, the executable path from `Zenith_GetTestExecutablePath()` and `m_bEnabled` true; it calls `Create` (the helper is a third process, started by `Create` from the same executable and dispatched by the test main), registers the scenario's threads, and performs the scenario: `null-write` (a store through a null `volatile uint32*`), `abort` (`abort()`), `zenith-fatal` (`ZENITH_FATAL(FATAL_CODE_QUEUE_FULL, "crash test")`; any existing `Zenith_FatalCode` serves, and the case computes its expected code from the same enumerator), `stack-overflow-registered` (unbounded recursion on a `Zenith_Thread` registered through `RegisterCurrentThread`), `second-crash` (two registered threads released by one atomic flag each perform the null write).
3. The case waits at most 30 s for the child (`Zenith_TestProcess::Wait`; on timeout it kills the child and fails), then asserts the child's exit code by Crash.md "Windows capture" step 3 (e): `0xC0000005`, 3, `0xE05A0000 | FATAL_CODE_QUEUE_FULL`, `0xC00000FD` and `0xC0000005` for the five scenarios in order.
4. It lists the crash directory through a `Zenith_VirtualFileSystem` of the same game name (`user:/crash/`) and keeps the files whose stem ends in `-<child PID>` (the stem is `<utc>-<pid>`, Crash.md "Crash report files"), so cases that CTest runs in parallel never see each other's files; it asserts the scenario's files and the report's `crash.kind`, then deletes exactly those files.
5. In a sanitizer configuration (`Zenith_IsSanitizerBuild()`, the `win-asan` preset) the case launches no child: it builds the desc with `m_bEnabled` from the `crash.enabled` default registered for the build (false there, Crash.md "Sanitizer builds"), asserts in process that `Create` returns `CRASH_HANDLER_ERROR_DISABLED`, and returns.

Because they are ordinary cases they run in every `win` configuration, `ShippingTest` included (§ "ShippingTest cases"), with no registry label of their own.

| Test | Asserts |
|---|---|
| `Test_CrashHandler_DisabledReturnsDisabled` | `m_bEnabled` false returns `CRASH_HANDLER_ERROR_DISABLED`, installs nothing |
| `Test_CrashHandler_SecondCreateRefused` | a second `Create` while the first lives returns `ALREADY_INSTALLED`, leaves the anchor unchanged, starts no second helper and writes no file whose stem carries the test process's PID |
| `Test_CrashHandler_AnchorSetAndCleared` | (Windows) after `Create` the anchor is the handler and `IsArmed()`; after destruction it is null, the test's sentinel unhandled-exception filter is active again and a new `Create` succeeds; a `Create` failing at the directory fault point leaves it null (Crash.md "Crash handler anchor") |
| `Test_CrashHandler_AnchorClaimProtocol` | every transition of the anchor's table through the neutral functions with a never-dereferenced stand-in address; the stress sub-cases race claims against a release for 10,000 rounds, exactly one winner per round (`linux-tsan` too) |
| `Test_CrashHandler_HelperStartsAndReportsReady` | (Windows realisation, selected at run time through `Zenith_CrashHandlerTestAccess`) `Create` succeeds with the helper's ready flag set within 5 s; the section and the three events are not inheritable once `Create` returns; `Zenith_CrashHelperBuildLaunch` names exactly five handles and the access mask `PROCESS_QUERY_INFORMATION \| PROCESS_VM_READ \| SYNCHRONIZE` |
| `Test_CrashHandler_AnnotationSeqlock` | the stress sub-cases: writers of one key and a reader loop; the reader never observes a value no writer wrote whole (all shared bytes atomic; run under `linux-tsan` too); every thread setting the same new key at once, for 100 fresh keys, leaves exactly one published entry per key (the `seq_cst` resolve of Crash.md "Annotations and breadcrumbs" step 4; meaningful on x86, whose store buffer would otherwise let two claimants publish) |
| `Test_CrashHandler_BreadcrumbSeqlock` | the same for the breadcrumb ring |
| `Test_CrashHandler_LogMirrorConcurrentWriters` | the stress sub-cases: writers of lines of random lengths 1 … 600 bytes (one to four slots, some cut at 480) lapping the mirror while a reader copies; every copied line was written whole, checked by the thread index, sequence and checksum each line carries; abandoned lines are counted |
| `Test_CrashHandler_LogMirrorOvertakenWriterAbandons` | through `Zenith_CrashLogMirrorTestAccess`: a writer claims sequence `s` and pauses; 512 further lines are committed, so line `s + 512` occupies its slot; the resumed writer abandons without storing, the reader returns line `s + 512` intact, and the dropped-lines count grew by 1 (Crash.md "Crash log mirror") |
| `Test_CrashHandler_LogMirrorSnapshotRule` | the two-copy snapshot walk the Windows helper uses: lines starting before the second cursor value minus 512 slots are dropped, subsequent committed lines kept, a line whose slot header differs between the two copies dropped, a stale-sequence or `WRITING` header skipped |
| `Test_CrashFatal_DecodeExceptionCode` | every `Zenith_FatalCode` decodes from `0xE05A0000 \| code` to its name; other codes are not fatal |
| `Test_CrashFatal_SlotFilledByFatalRecord` | a `LOG_LEVEL_FATAL` `core.diagnostics` record with field `code` fills the fatal slot; other records do not |
| `Test_CrashFinalize_ExitRecordMapping` | fake exit records of every reason, inside and outside 10 s of a pending report, map to an attached tombstone, a synthetic `os-exit` report, or nothing |
| `Test_CrashHandler_SourcesHaveNoShippingBranch` | no file under `Internal/CrashHandler/` or `Internal/<Platform>/Zenith_Crash*` contains `Zenith_IsShippingBuild` |
| `Test_CrashHelper_ForgedSectionRefused` | (Windows) the helper's validation refuses a wrong magic, a wrong version, a nonce mismatch, a PID mismatch, a short region and malformed arguments, writing nothing |
| `Test_CrashHelper_UnInheritedLaunchExits3` | (Windows realisation, selected at run time) `Zenith_RunCrashHelperIfRequested` called in process with the option and fabricated handles returns true with exit code 3 and writes no file |
| `Test_CrashHelper_SectionStringsBoundedAndEscaped` | (Windows) unterminated, quoted and invalid-UTF-8 section strings yield a report that parses with every value within its bound |
| `Test_CrashHandler_AnnotationsFullCounted` | the 65th key is counted, not stored |
| `Test_CrashHandler_BreadcrumbRingNewest64` | after 100 breadcrumbs the report lists the newest 64 in order |
| `Test_CrashHandler_LogMirrorLockFree` | the mirror copies lines while another thread holds the log router's sink lock |
| `Test_CrashHandler_FinalizeAttachesAndRetains` | with fixture pending reports and attachments, `Finalize` copies the newest attachment before the crash time, sets `finalized`, removes markers and keeps the newest `maxReports` |
| `Test_CrashHandler_FinalizeCorruptRenamed` | a malformed report becomes `.corrupt` |
| `Test_CrashHandler_HangReportFormat` | `WriteHangReport` writes the text format with every line |
| `Test_CrashHandler_JobThreadLifecycleThunk` | the thunk registers on `bStarted` and unregisters otherwise |
| `Test_CrashHandler_CreateFailsAtDirectory_Unwinds`, `…AtHelper…` (Windows) | each fault point returns its error with nothing installed (`…AtSignals…` is the Android realisation's and is exercised by the S24 device run, since no host preset arms the signal path) |
| `Test_CrashHandler_LinuxCreateUnsupported` | (Linux) `Create` returns `CRASH_HANDLER_ERROR_DISABLED` when disabled and `CRASH_HANDLER_ERROR_UNSUPPORTED` when enabled, installing nothing |
| `Test_CrashDeath_NullWrite` | (Windows, a child process, steps 1 to 5 above) scenario `null-write`: exit code `0xC0000005`; the child's PID stem has `.report.json` (kind `exception`, `exceptionCode` `0xC0000005`), `.log.txt`, `.pending` and `.dmp` |
| `Test_CrashDeath_Abort` | (Windows, a child process) scenario `abort`: exit code 3; a report through the CRT abort path |
| `Test_CrashDeath_ZenithFatal` | (Windows, a child process) scenario `zenith-fatal`: exit code `0xE05A0000 \| FATAL_CODE_QUEUE_FULL`; the report has kind `fatal` with the fatal code's name and the message `crash test` |
| `Test_CrashDeath_StackOverflowRegisteredThread` | (Windows, a child process) scenario `stack-overflow-registered`: exit code `0xC00000FD`; a report whose thread is the registered one |
| `Test_CrashDeath_SecondCrashNoSecondReport` | (Windows, a child process) scenario `second-crash`: exit code `0xC0000005`; exactly one `.report.json` carries the child's PID stem |
| `Test_CrashReport_SchemaRoundTrip` | a captured report parses and every field of Crash.md "Crash report schema" is present |
| `Test_CrashReport_SignalSafeEscaperMatchesJsonWriter` | (every host preset) the same calls through `Zenith_CrashJsonTail` and `Zenith_JsonWriter` with hostile strings (quotes, backslashes, every control byte, 0x7F, invalid and truncated UTF-8, strings at and beyond their bound) give byte-identical output; over a 1,024-byte buffer the document parses, carries `"truncated": true` and keeps the members written before the cut (Crash.md "Signal-safe report tail") |
| `Test_CrashReport_SignalSafeEscaperAllocatesNothing` | (every host preset) a full tail written through `Zenith_CrashJsonTail` under a fault-injecting heap that fails every allocation records no allocation |

## Evidence tests

`Tests/Zenith_BudgetRunRecord_Tests.cpp`, `Tests/Zenith_RunTelemetry_Tests.cpp`:

| Test | Asserts |
|---|---|
| `Test_BudgetStatistics_NearestRank` | p95 and p99 of the fixtures of `Tests/Fixtures/BudgetRun/stats.json` equal their recorded nearest-rank values, including `n = 1` and ties |
| `Test_BudgetStatistics_RollingMin` | the 1-s rolling minimum of a fixture present series equals the hand-computed value |
| `Test_BudgetRunRecord_ExclusionWindow` | frames in `[tReady, tReady + 10 s)` are flagged excluded and only that interval is listed |
| `Test_BudgetRunRecord_MinimumsAndInvalid` | 7,199 frames over 130 s is `invalid`; 7,200 frames over 120 s measured is valid |
| `Test_BudgetRunRecord_EveryBlockFieldPresent` | the written record has exactly the block's fields, defaults where unmeasured |
| `Test_BudgetRunRecord_RowsResult` | pass, fail and report (pending, `totalWorkP95`) results by the rules of Evidence.md "Evidence record writer" |
| `Test_BudgetRunRecord_CsvHeaderExact` | the CSV header is the [Z2-N-BUDGET-104] header byte for byte |
| `Test_BudgetRunRecord_RejectsBadInputs` | missing files, a newer schema, an unknown source kind, a workload mismatch, 129 rows: each returns its error with the row index or line |
| `Test_BudgetRunRecord_EmitterAddSample` | an `emitted` row takes the last value per frame from `AddSample` on two threads |
| `Test_RunTelemetry_EventsAndFlush` | events from 4 threads appear in order of time; `Flush` writes `run.json`; two flushes with rows between them write `…frames.0000.csv` and `…frames.0001.csv`, each starting with the header, whose concatenated rows are every frame in order with the configured counters |
| `Test_RunTelemetry_RefusedInShipping` | in `ShippingTest` `Create` returns `PROFILER_ERROR_NOT_IN_SHIPPING`; elsewhere it succeeds |
| `Test_BudgetRunRecord_GpuPassSumAcrossViews` | `DebugOverlay` sums `DebugOverlay[Main]` and `DebugOverlay[Editor]` and not `DebugOverlayX[Main]`; `DebugOverlay[Main]` matches one site; `DebugOverlay*` all three; no match samples NaN |
| `Test_BudgetRunRecord_ZoneSumAccumulatesSite` | a `zoneSum` row over one site that a real profiler saw closed 50 times in a frame, on two threads, samples the site's whole frame total (the sum of the 50 durations, not the last closure's), a row over two sites samples the sum of both totals, and the next frame, in which the site closes no zone, samples 0 |
| `Test_BudgetRunRecord_MemorySamplesExcludeRecord` | `processMemory` and `memorySubsystem` (`other`) samples are their inputs minus the record's `GetResidentBytes()`; another subsystem's sample is unchanged |
| `Test_BudgetRunRecord_GpuPassSumInCountersOnly` | a `gpuPassSum` row samples the pass sums of a counters-only profiler fed GPU submissions |
| `Test_BudgetRunRecord_EvidenceFilesAppendOnly` | with a `Zenith_TestEvidenceFileWriter` per file, three `FlushFrames` calls and `Write` append to the same frames file in order and write the record once; a refused append returns `BUDGET_RUN_ERROR_WRITE` |
| `Test_BudgetRunRecord_StressAddSample` | the stress sub-cases: every thread adds samples to its own and a shared row between `SampleFrame` calls; the last value per row wins, nothing torn |
| `Test_RunTelemetry_DetachStopsWrites` | after `Detach`, `Flush` returns `PROFILER_ERROR_NO_OUTPUT` and writes nothing |
| `Test_RunTelemetry_StressRecordEvent` | the stress sub-cases: events from every thread appear once each, in time order within each thread |

## Quality-tool tests

| Test | Kind | Asserts |
|---|---|---|
| `Pwsh_Evidence_*` (the fourteen cases of the table, `Pwsh_Evidence_LineEndingsIndependent` included) | `Test-Zenith2Evidence.ps1` | Evidence.md "Fingerprint invalidation tests" |
| `Pwsh_LinuxTsan_HostCheckModes` | `Test-Zenith2LinuxTsan.ps1` | QualityTools.md "Host verification": the commands of both modes; a Linux host without `-Native` exits 2 |
| `Pwsh_FuzzCampaign_CommandLine`, `…_CollectsArtefacts`, `…_ExitCodeOnReproducedFinding` | `Test-FuzzCampaign.ps1` | QualityTools.md "Fuzz campaign runner" |
| `Pwsh_Workload_ProfilerAndTracySwitches` | `Test-Zenith2Evidence.ps1` | Evidence.md "Workload runner": the profiler and Tracy switches per row set |
| `Pwsh_LinuxTsan_PathTranslation` | `Test-Zenith2LinuxTsan.ps1` | `C:\dev\Zenith` → `/mnt/c/dev/Zenith`; `D:\a b` → `/mnt/d/a b` quoted |
| `Pwsh_LinuxTsan_SanitizerOptionsExact` | same | the `TSAN_OPTIONS` and `UBSAN_OPTIONS` strings equal [Z2-N-ARCH-220]'s |
| `Pwsh_LinuxTsan_ExecQuoting` | same | an `-Exec` command with quotes and `$` reaches bash unchanged |
| `Pwsh_LinuxTsan_KernelCheckRefuses` | same | a mocked `mmap_rnd_bits` of 32 makes the runner exit 3 with the message |
| `Pwsh_Workload_RefusesShippingPackage` | `Test-Zenith2Evidence.ps1` | a fixture `release.json` with `"configuration": "Shipping"` is refused by the runner's package check |
| `Pwsh_Workload_RefusesPackageTreeMismatch` | same | a package whose tree differs from the checkout's is refused |
| `Pwsh_Workload_QualityKeysEachOnce` | same | against the hand-written `Fixtures/ConfigDump/config-dump.json`, the built command line names every `render.quality.*` key exactly once, mapped keys with the workload's value and the rest with their non-user value |
| `Test_DevTool_CaptureSummary` | `ZenithDevTool_Tests` | the summary of a test-time-generated capture lists its lanes and top sites; a site with two zones, one nested inside the other, is listed once with count 2 and its outer zone's duration as total; a capture with `LOCKS` chunks lists its lock sites sorted by total wait with the summed counts |
| `Test_CaptureConvert_TraceEventGolden` | same | the conversion of the capture the test builds (2 lanes, 3 zones, 1 counter, 1 marker, fixed timestamps) equals the hand-written text golden `Tests/Fixtures/Capture/trace_golden.json` |
| `Test_DevTool_LogViewOrdersAcrossChunks` | same | lines from three chunk files print in sequence order |
| `Test_DevTool_CrashSymbolizeMatchesModules` | same | addresses are mapped to modules and offsets; a build-ID mismatch is reported |
| `Test_DevTool_RemoteClientAgainstServer` | same | Console.md "Remote client": `remote exec "echo hi"` through PLAT's `Zenith_DevClient` (the tool's only socket code) against an in-process server |
| `Test_DevTool_AdbForwardCommandLine` | same | the `adb forward` and `adb logcat` argument lists, with a recording stand-in for the process function |
| `Test_DevTool_ContentReportJunit` | same | a fixture validation report renders to JUnit with one failure per error |
| `Test_DevTool_DashboardBuild` | same | three fixture evidence records produce one series with the regression boundary marked |
| `Test_<Stem>_Regressions` | each fuzzed library's tests | every committed regression input replays without a failure |

## Integration suites

`Zenith2/Tests/DevTools/` (`Zenith_Integration_DevTools`):

| Test | Asserts |
|---|---|
| `Test_FrameGraph_ZonesForEveryNode` | a real `Zenith_JobSystem` and `Zenith_FrameExecutor` with a 40-node fixture graph and an installed profiler produce, per frame, one zone per node on the lane that ran it, sub-job and continuation zones nested under their node, and lanes named `Z2 Main`, `Z2 Worker NN`, `Z2 RenderSubmit` |
| `Test_Profiler_ContentionRowsFromCounters` | over 300 frames of a fixture workload, the JOBS panel rows evaluate from counters alone, and the interval sums of idle %, busy % and overhead equal 100% within 1% over the interval (never per frame) |
| `Test_Profiler_JobsLanesRegisteredAtCreate` | `Zenith_JobSystem::Create` with `m_pxProfiler` and 3 workers registers one lane per slot and, with no bind call, every zone of `Z2 Worker 01` … `03` and `Z2 RenderSubmit` lands on its lane and the main thread's on `Z2 Main` |
| `Test_Profiler_SubJobZonesAccumulatePerSite` | a fixture node whose 64 sub-jobs run one bracket on the 3 workers in one frame gives one zone-statistics entry for that site with count 64 and a total equal to the sum of the 64 sub-job zones' durations on every worker lane; the next frame's entry counts that frame's 64 alone |
| `Test_Profiler_ContendedLock_ProducesLockWaitZone` | a real profiled `Zenith_Mutex` at a test-usable level, `captureLocks` on: thread A locks and signals, thread B calls `Lock` and blocks for about 5 ms until A unlocks; B's acquire reports a wait of at least 1 ms, `GetLockStats()` has an entry with contended at least 1 and a non-zero maximum wait, B's lane has a `lockwait:<name>` zone of that wait followed by the `lock:<name>` zone, and A's uncontended acquire added no wait |
| `Test_Profiler_LockStats_HammerOneLock` | the stress sub-cases: each of N threads (2, 8 and the hardware concurrency) locks and unlocks one real profiled `Zenith_Mutex` 10⁵ times with `captureLocks` on and a real capture flushing, while the main thread assembles frames; the acquisitions summed over every frame's `GetLockStats()` equal N × 10⁵ (less the counted `profiler.lock_stats_dropped_delta`, which is 0 here), contended never exceeds acquisitions, no `lockwait:` zone is longer than the maximum wait, and `linux-tsan` reports no race |
| `Test_Profiler_CondVarWait_HoldExcludesWait` | a real profiled `Zenith_Mutex` and `Zenith_ConditionVariable`, `captureLocks` on: a waiter holds the mutex for about 1 ms, waits on the condition variable for about 50 ms and is signalled; its site's maximum hold is under 20 ms, and the frame has a `wait:ConditionVariable` zone of at least 40 ms |
| `Test_Profiler_NoProfilingLock_NotRecorded` | the same threads also lock a `Zenith_MutexNoProfiling` beside the profiled one and the test confirms by its own counter that the unprofiled lock was contended: `GetLockStats()` has no entry and the frame no zone for it |
| `Test_Profiler_HookStressUnderJobs` | the stress sub-cases as job-system workers: zones, counters and lock events while the main thread assembles frames: no drop below capacity, every zone well-formed |
| `Test_DevConsole_GraphCommandsRegistered` | the executor, given a real `Zenith_DevConsole` and a recording `m_pfnDumpWrite`, registers `jobs.recompile` and `jobs.dump`; `jobs.dump <dir>` reaches the recording callback with three files (GRAPH's path, not DEVT's file writer) |
| `Test_DebugDraw_RecordedInJobsDrainedOnce` | lines recorded inside job bodies all appear in exactly one drain |

## Render tests

`Zenith2/Tests/DevToolsRender/` (`Zenith_Integration_DevToolsRender`, on WARP for D3D12 and lavapipe for Vulkan, not `needs-gpu`):

| Test | Asserts |
|---|---|
| `Test_DebugOverlayRender_LinesAndText` | a headless frame of a fixture world with 100 lines, 10 rectangles and a text string renders; the output image is compared with a golden generated at test time from the CPU reference rasteriser of the test (lines and glyphs rasterised on the CPU with the same expansion rules), FLIP mean ≤ 0.02 |
| `Test_DebugOverlayRender_DepthTestedDimmedBothModes` | in a standard-Z view and in a reversed-Z view, a depth-tested line behind a quad is drawn at 25% alpha; an untested one at full alpha |
| `Test_ProfilerOverlayRender_FullStateFrame` | the overlay in `FULL` over a 300-frame run renders without validation errors on both backends and its `DebugOverlay` pass appears in the frame dump |
| `Test_ProfilerOverlayRender_GpuViewHasPasses` | the GPU view lists the frame's passes with their view suffixes from real timestamps (WARP and lavapipe expose timestamps) |
| `Test_DebugOverlayRender_EditorViewTagFiltering` | a play-in-editor world's draws appear in its view and not in the edited world's |

## ShippingTest cases

Shipping behaviour that a non-Shipping build cannot show runs in `ShippingTest` (`win-shipping-test`, [Z2-N-ARCH-231]: every Shipping flag and define, `Zenith_IsShippingBuild()` true, development services refusing, Tracy unlinked). Symbol absence from a Player stays the `Libgraph` step's Shipping symbol check, which alone reads a Player's link closure ([Z2-N-ARCH-181]: `ShippingTest` is never a Player's).

The rule for every DEVT test that exercises a development service (the console, the file writer, the frame dump, the remote server, the runtime development commands, run telemetry, Tracy): its body is the two-branch statement `if constexpr (Zenith_IsShippingBuild()) { <assert the service's refusal> } else { <the test> }`, never an early `return`, so in `ShippingTest` the test body is a discarded statement (no development-only call is odr-used, and the optimised build's `/W4 /WX` sees no unreachable code, MSVC C4702), where the refusal is the service's own (`DEV_CONSOLE_ERROR_NOT_IN_SHIPPING` from `Zenith_DevConsole::Create`, `DEV_FILE_ERROR_NOT_IN_SHIPPING` from `Zenith_DevFileWriterUserMount::Write`, `PROFILER_ERROR_NOT_IN_SHIPPING` from `Zenith_DevFrameDump::Create` and `Zenith_RunTelemetry::Create`, `DEV_SERVICE_ERROR_NOT_IN_SHIPPING` from `Zenith_DevToolsServer::Create`), and a Tracy test asserts that its recording bridge table received no call. No test references a Tracy bridge function or a Tracy symbol outside `if constexpr (!Zenith_IsShippingBuild())`, so every test links in `ShippingTest`, where Tracy is not linked. The tests under the rule: every `Test_DevConsole_*`, `Test_DevFileWriter_*`, `Test_DevFrameDump_*`, `Test_DevToolsServer_*`, `Test_RuntimeDevCommands_*` and `Test_RunTelemetry_*` case, `Test_DevConsole_GraphCommandsRegistered`, `Test_DevTool_RemoteClientAgainstServer` (whose target `ZenithDevTool_Tests` is a host tool's and is not built in `ShippingTest`, so the rule never triggers there), `Test_Profiler_TracyStartedMidZone_StackBalanced` and `Test_Profiler_Tracy_NotStartedWhenUnavailable`. Other areas' tests of DEVT consumers that construct the real console follow the same rule (row DV32 for Z2T-GRAPH-050's console tests).

The lock and wait families join the rule, because in Shipping `FillHooks` leaves the lock pair and the blocking-wait member null and a profiled `Lock` skips the hook lookup altogether, so no lock or wait is ever recorded there: `Test_Profiler_LockEvents_*`, `Test_Profiler_OwnLockNotRecorded`, `Test_Profiler_BlockingWait_*`, `Test_Profiler_LockStats_*` (the unit cases and `Test_Profiler_LockStats_HammerOneLock`), `Test_Profiler_Capture_LocksChunkRoundTrip`, `Test_ProfilerOverlay_LockTable`, `Test_Profiler_ContendedLock_ProducesLockWaitZone`, `Test_Profiler_CondVarWait_HoldExcludesWait` and `Test_Profiler_NoProfilingLock_NotRecorded`. The Shipping branch asserts that `Zenith_GetProfilerHooks()` has a null lock pair and blocking-wait member after the profiler's `FillHooks`, and that `GetLockStats()` stays empty after the test's lock traffic and no `lock:`, `lockwait:` or `wait:` zone exists; the other branch is the test. `Test_Profiler_FillHooks_WritesProfilerMembersOnly` carries both branches already. Every other DEVT test exercises no development service and runs unchanged in `ShippingTest`; the death cases follow § "Death cases".

The cases that assert Shipping behaviour, each also asserting the contrasting behaviour in the other configurations so no case is empty anywhere:

| Case | Target | In `ShippingTest` | Elsewhere | Requirement |
|---|---|---|---|---|
| `Test_DevConsole_CreateRefusedInShipping` | `ZenithProfiling_Tests` | `Create` returns `DEV_CONSOLE_ERROR_NOT_IN_SHIPPING` | `Create` succeeds with the 6 built-ins | Z2-R-DEVT-054 |
| `Test_Profiler_ShippingStartsCountersOnlyWithoutTracy` | `ZenithProfiling_Tests` | `Zenith_GetPlayerProfilerDesc(Zenith_GetBuildPlatform(), Zenith_GetBuildConfiguration(), Zenith_IsSanitizerBuild())` has `PROFILER_MODE_COUNTERS_ONLY` and `m_bTracyAvailable` false; the profiler created from it with a recording bridge table installed and `ApplySettings` with Tracy enabled calls no table member, `GetMode()` is counters-only, a zone begin and end assemble no zone, and `GetResidentBytes` holds no Tracy source-location array | `PROFILER_MODE_CAPTURE`, Tracy available outside sanitizer builds | Z2-R-DEVT-016 |
| `Test_DevToolsConfig_ShippingMasksFromBuildInfo` | `ZenithProfiling_Tests` | `Zenith_RegisterDevToolsConfigKeys(xSchema, Zenith_GetBuildPlatform(), Zenith_GetBuildConfiguration())` gives every non-Shipping key the engine-default layer alone and its off value, and `Zenith_ResolveProfilerSettings` with `Zenith_GetBuildConfiguration()` makes GPU timings follow `devtools.profiler.enabled` | the configuration's own masks; GPU timings follow `devtools.profiler.gpuTimings` | Z2-R-DEVT-016, Z2-R-DEVT-027 |
| `Test_LogFileSink_ShippingFloorFromBuildInfo` | `ZenithProfiling_Tests` | `INFO` not written, `WARNING` written; `devtools.log.file` defaults to false | both written | Z2-R-DEVT-065 |
| `Test_RunTelemetry_RefusedInShipping` | `ZenithProfiling_Tests` | `Create` returns `PROFILER_ERROR_NOT_IN_SHIPPING` | `Create` succeeds | Z2-R-DEVT-096 |
| `Test_DevTools_ShippingCreatesNoDevServices` | `ZenithRuntime_Tests` | no console, file writer, frame dump, remote server or telemetry, no profiler output, even with the two keys set on the command line; overlay and recorder present and disabled | the services exist | Z2-R-DEVT-054, Z2-R-DEVT-073, Z2-R-DEVT-096, Z2-R-DEVT-119 |
| `Test_CrashDeath_NullWrite`, `…_Abort`, `…_ZenithFatal`, `…_StackOverflowRegisteredThread`, `…_SecondCrashNoSecondReport` | `ZenithPlatform_Tests` (ordinary cases crashing a child process, § "Crash tests") | the same exit codes and reports as in Debug and Release: crash capture works with every Shipping flag and define | as specified in § "Crash tests" | Z2-R-DEVT-086 |

`ShippingTest` cases appear in no ticket's Definition of Done while the `Test` step does not run `win-shipping-test` (the PROC hand-off, row DV17 (f)); each landing ticket names them in its Validation section, as SCRIPT's tickets do.

## Death cases

Every DEVT death case is classified by the macro that kills it, because asserts are compiled out in Release, Shipping and `ShippingTest` (`Zenith2/Docs/Contracts.md` → "Assertions" table, ruling Q71): a case that dies by `ZENITH_ASSERT` is `z2-death-assert`, which SHIP's registry gives `CONFIGURATIONS Debug Sanitize Hwasan`; a case that dies by `ZENITH_CHECK`, `ZENITH_FATAL` or a real fault is `z2-death-check`, registered in every configuration (`Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "ShippingTest configuration").

| Case | File | Dies by | Suite |
|---|---|---|---|
| `Test_ProfilerDefect_EndFrameOffMainThread` | `Engine/ZenithProfiling/Tests/Zenith_Profiler_DefectTests.cpp` | `ZENITH_ASSERT` (the main-thread affinity of `EndFrame`, DEVT.md "Luau errors and asserts") | `z2-death-assert` |
| `Test_ProfilerDefect_DestroyWhileInstalled` | same | `ZENITH_ASSERT` in `~Zenith_Profiler` (Profiler.md "Creation and shutdown") | `z2-death-assert` |
| `Test_ProfilerDefect_RingIndexCorruption` | same | `ZENITH_CHECK` on a thread-state index beyond capacity | `z2-death-check` |
| `Test_ProfilerNameTable_SizeOverflow_NotPowerOfTwo`, `…_AboveMaximum`, `…_ExceedsStorage` | `Engine/ZenithProfiling/Tests/Zenith_ProfilerNameTable_Tests.cpp` | `ZENITH_CHECK` in `Init`, one refusal per case (a death case dies once per process) | `z2-death-check` |

The `Test_CrashDeath_*` cases are not death cases: they crash a child process and are ordinary cases (§ "Crash tests"), because `z2-death-check` passes only on CORE's failure text. DEVT registers no other death case: the console's and the recorder's affinity asserts (`RegisterCommand` off the main thread, a drain from two threads, a console destroyed with a token registered) are review-enforced contract rules with no death test.

## Stress and sanitizer runs

- `Sanitizers -Preset win-asan`: `ZenithProfiling_Tests`, the DEVT fuzz smokes, `Zenith_Integration_DevTools`.
- `Sanitizers -Preset linux-tsan`: `ZenithProfiling_Tests` (the TSan targets are every stress test: `Test_Profiler_HookStress`, `Test_Profiler_LockStats_BankHandoverNoLoss`, `Test_ProfilerNameTable_StressConcurrentInsert`, `Test_DebugDraw_StressRecordAndDrain`, `Test_DevConsole_QueueLineFromThreads`, `Test_BudgetRunRecord_StressAddSample`, `Test_RunTelemetry_StressRecordEvent`), the platform-neutral `ZenithPlatform_Tests` crash cases (`Test_CrashHandler_AnnotationSeqlock`, `…_BreadcrumbSeqlock`, `…_LogMirrorConcurrentWriters`), `Zenith_Integration_DevTools` (`Test_Profiler_HookStressUnderJobs`, `Test_Profiler_LockStats_HammerOneLock`, `Test_DebugDraw_RecordedInJobsDrainedOnce`), `Zenith_Integration_TsanSmoke`. Each stress test runs at 2, 8 and the hardware concurrency ([Z2-N-CONTRACT-357]). Each stress test sizes its desc from its thread count `n`: the profiler's `m_uThreadStates = min(128, n + 2)` (the main thread and the test runner's own), the recorder's `m_uThreadRings = min(127, n + 1)`; a sub-case whose `n` exceeds the desc maximum (more than 126 threads) runs with `n` clamped to 126 and says so in its output, because past that point "no drop below capacity" would assert something the design does not promise. The step runs on a ZenithProfiling or ZenithPlatform crash-file diff once X247 (c) adds those paths to `Zenith2Memory`; the gate list itself is PROC's protected hand-off (row DV17 (d)).
- The crash cases never arm a handler under a sanitizer: in `win-asan` each `Test_CrashDeath_*` case takes its in-process branch (`Create` returns `CRASH_HANDLER_ERROR_DISABLED`, no child is launched; Crash.md "Sanitizer builds"). `Test_Profiler_Lanes_AutomaticLanesCreatedByAssembly` and `Test_CrashHandler_LogMirrorOvertakenWriterAbandons` join the `linux-tsan` targets above.

## Device and acceptance runs

| Run | Stage | Ticket kind | Evidence |
|---|---|---|---|
| `linux-tsan` host bring-up | 1 | its own `needs-human` TASK "Bring up the linux-tsan WSL host", blocking every recording ticket (QualityTools.md "Host verification") | `Zenith2/Docs/Research/LinuxTsanHost/<utc>/junit.xml` and `host-check.txt`; the Stage 1 acceptance re-runs `Test-LinuxTsanHost.ps1` |
| DEVT budget allocation | 1 | `needs-human` ruling "Stage 1 per-system budget ticket for DEVT" | `Zenith2/Docs/Research/Budgets/Stage1/DEVT/allocation.json` |
| D-SEL-CRASH | 2 | `needs-human` ruling | `Zenith2/Docs/Research/DSelCrash/Report.md` |
| name-table justification measurement | 2 | `needs-human` ruling running `Zenith_Bench_ProfilerOverhead`'s `NameTable_*` and `HashMapMutex_*` cases on both devices | the completed record `Custom container: Zenith_ProfilerNameTable` |
| S24 overlay, remote console and crash | 3 | `needs-human` steps of the Stage 3 acceptance | screenshots of the overlay in `FULL` on the S24, a remote console transcript over `adb forward`, a finalized and symbolized crash report from `crash.test access --confirm` |
| editor overlay | 3 | `needs-human` step | the overlay in an editor viewport with the play-in-editor world's tag filtering |
| Z2-B-DEVT-001, -002, -003 measurement | 3 | `needs-human` budget runs with `-ProfilerOverheadPair` (`Test_DebugOverlay_GpuCostWithinRow` is the run's name for the GPU row's record) | evidence records under `Zenith2/Docs/Research/Budgets/Stage3/` |
| fuzz campaign | 6 | `needs-human` acceptance step | `campaign.json` |

## Fixtures

| Fixture | Kind | Location |
|---|---|---|
| expression lines (valid, invalid with columns and messages) | committed text | `Engine/ZenithProfiling/Tests/Fixtures/Expressions/` |
| console lines and their tokens | committed text | `Engine/ZenithProfiling/Tests/Fixtures/Console/` |
| statistics series and expected percentiles | committed JSON | `Engine/ZenithProfiling/Tests/Fixtures/BudgetRun/` |
| `rows.json`, `fingerprint.json` samples (valid and malformed) | committed JSON | `Engine/ZenithProfiling/Tests/Fixtures/BudgetRun/` |
| captures | generated at test time by `Zenith_TestEmitFrame` and the capture writer (binary, never committed) | the test's temporary directory |
| trace-event golden | committed text | `Tools/DevTool/Tests/Fixtures/Capture/trace_golden.json`, hand-written for the test's fixed 3-zone capture; a deliberate change to the conversion edits it by hand in the same diff |
| crash reports, attachments | committed JSON plus test-time-generated files | `Engine/ZenithPlatform/Tests/Fixtures/Crash/` |
| fingerprint mini tree | committed text, assembled into a temporary git repository at test time | `Benchmarks/Evidence/Tests/Fixtures/MiniTree/` |
| fuzz seed corpora and regressions | committed small inputs | `Tests/Fuzz/Corpus/<Stem>/`, `Tests/Fuzz/Regressions/<Stem>/` |
| overlay render goldens | generated at test time by the CPU reference rasteriser | the test's temporary directory |
