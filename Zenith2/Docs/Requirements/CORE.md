# CORE requirement catalogue

This is the requirement catalogue of the CORE area (`Z2 CORE — Core & mathematics`, including the memory governor). Each requirement is one testable obligation with its inventory coverage, stage, classification, governing decisions, budget rows, acceptance evidence and the design headings of `Zenith2/Docs/Design/CORE.md` and its split files that specify it.

## Purpose

The catalogue decomposes every CORE item of `Zenith2/Docs/Requirements/Inventory.md` → "Inventory v1" into requirements `Z2-R-CORE-NNN`, the unit that leaf and integration tickets cover (`Zenith2/Docs/Roadmap.md` → "Board conventions"). Stages name the stage whose acceptance each requirement serves; a Stage 2 requirement whose governing decisions are settled is scheduled early by the pull-forward rule (`Zenith2/Docs/Roadmap.md` → "Pull-forward rule"). Classifications: `required` (delivered in its stage) and `research` (a selection decision: a harness ticket plus a `needs-human` ruling ticket, Z2-D-084). CORE has no deferred, excluded or game-side requirement.

## Requirements

### Z2-R-CORE-001 Owned pointer layout

- **Statement:** `Zenith_Owned<T>` is exactly one pointer in every configuration, move-only, with no reference count, deleter or header.
- **Covers:** Z2-I-CORE-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_Owned_SizeIsOnePointer`, `Test_Owned_MoveOnly_NullsSource` (`Test` gate step, every preset)
- **Design:** `Zenith2/Docs/Design/CORE/Memory.md` → "Owned pointer placement"; `Zenith2/Docs/Design/CORE.md` → "Handle kinds used"

### Z2-R-CORE-002 Owned creation and destruction

- **Statement:** `Zenith_New`, `Zenith_TryNew` and `Zenith_Delete` follow the Contracts steps: a failed `Zenith_TryNew` constructs nothing, and destruction runs the destructor once and frees to the root heap.
- **Covers:** Z2-I-CORE-01, Z2-I-CORE-17
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_Owned_TryNew_FailureConstructsNothing`, `Test_Owned_Reset_RunsDestructorAndFrees` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Memory.md` → "Owned pointer placement"; `Zenith2/Docs/Design/CORE/Memory.md` → "Memory tests"

### Z2-R-CORE-003 Owned base conversion check

- **Statement:** Converting `Zenith_Owned<Derived>` to `Zenith_Owned<Base>` checks the same-address rule with `ZENITH_CHECK` in every configuration, including `win-release`.
- **Covers:** Z2-I-CORE-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_Owned_BaseConversion_SameAddress_Checks` (`z2-death-check`), `Test_Owned_BaseConversion_FirstBase_Converts` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Memory.md` → "Owned pointer placement"

### Z2-R-CORE-004 Slot table generational resolution

- **Statement:** `Zenith_SlotTable` resolves a null, stale or foreign `Zenith_SlotID` to `HANDLE_ERROR_NULL`, `HANDLE_ERROR_STALE` or `HANDLE_ERROR_FOREIGN` in every configuration and never asserts on resolution.
- **Covers:** Z2-I-CORE-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_SlotTable_NullDoesNotResolve`, `Test_SlotTable_StaleAfterDestroy_DoesNotResolve`, `Test_SlotTable_ForeignOwner_DoesNotResolve` (`Test` gate step, including `win-release`)
- **Design:** `Zenith2/Docs/Design/CORE/Memory.md` → "Zenith_SlotTable"

### Z2-R-CORE-005 Slot table retirement and reuse

- **Statement:** A slot whose generation reaches 0xFFFFFFFF is retired and never reissued, freed slots are reused in FIFO order, a resolved value's address is stable until that slot is freed, and `ForEachLive` visits every live value once in ascending slot order.
- **Covers:** Z2-I-CORE-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_SlotTable_GenerationExhaustion_RetiresSlot`, `Test_SlotTable_FifoReuse_DelaysReissue`, `Test_SlotTable_PointerStableAcrossGrowth`, `Test_SlotTable_ForEachLive_AscendingOrder` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Memory.md` → "Zenith_SlotTable"

### Z2-R-CORE-006 Slot table container tests

- **Statement:** `Zenith_SlotTable` carries the oracle, property, Try-failure and size-overflow tests the container contract requires.
- **Covers:** Z2-I-CORE-02, Z2-I-CORE-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-041
- **Budgets:** none
- **Acceptance evidence:** `Test_SlotTable_MatchesOracle`, `Test_SlotTable_Properties`, `Test_SlotTable_TryFailure_LeavesUnchanged`, `Test_SlotTable_SizeOverflow_IsRejected` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Memory.md` → "Zenith_SlotTable"; `Zenith2/Docs/Design/CORE/Memory.md` → "Memory tests"

### Z2-R-CORE-007 Operation handle handle tests

- **Statement:** `Zenith_OperationHandle` passes the null, stale, foreign and generation-exhaustion handle tests in every configuration.
- **Covers:** Z2-I-CORE-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_OperationHandle_NullDoesNotResolve`, `Test_OperationHandle_StaleAfterDestroy_DoesNotResolve`, `Test_OperationHandle_ForeignOwner_DoesNotResolve`, `Test_OperationHandle_GenerationExhaustion_RetiresSlot` (`Test` gate step, including `win-release`)
- **Design:** `Zenith2/Docs/Design/CORE/Threading.md` → "Zenith_OperationHandle implementation"

### Z2-R-CORE-008 Heap backing selection

- **Statement:** Sanitizer builds always back the general heap with the system allocator, and every other configuration uses the backing `uZENITH_HEAP_RULED_BACKING` names, which is the system allocator until the D-SEL-HEAP ruling edits it; no source list or type layout differs between the backings.
- **Covers:** Z2-I-CORE-03, Z2-I-CORE-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040, Z2-D-011, D-SEL-HEAP (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_Heap_*` rows pass in `win-asan` and `win-release`; `CompileCoverage` and `ForbiddenCheck` F11/F12 clean
- **Design:** `Zenith2/Docs/Design/CORE/Memory.md` → "Backing selection"

### Z2-R-CORE-009 System backing per platform

- **Statement:** The system backing allocates every power-of-two alignment from 1 to 4096 on Windows, Android and Linux, freeing an over-aligned Windows block through `_aligned_free` found by the over-aligned side table, and stores no per-allocation header.
- **Covers:** Z2-I-CORE-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_Heap_Alignment_AllPowersToMax`, `Test_Heap_OverAligned_FreeRoutesToAlignedFree`, `Test_Heap_OverAligned_UsableSize_Reported`, `Test_Heap_ZeroSize_AllocatesOneByte` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Memory.md` → "Backing calls per platform"; `Zenith2/Docs/Design/CORE/Memory.md` → "Over-aligned side table"

### Z2-R-CORE-010 mimalloc backing path

- **Statement:** When D-SEL-HEAP selects mimalloc, the non-sanitizer backing uses `mi_malloc_aligned`, `mi_free` and `mi_usable_size` for every alignment, with the mimalloc manifest entry owned by ZenithCore, and every heap test passes on it.
- **Covers:** Z2-I-CORE-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040, D-SEL-HEAP (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** the D-SEL-HEAP ruling entry in `Zenith2/Docs/DecisionLog.md`; `Test_Heap_*` rows green in `win-release` and `android-arm64` with the ruled backing (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Memory.md` → "Backing selection"; `Zenith2/Docs/Design/CORE/Memory.md` → "Backing calls per platform"

### Z2-R-CORE-011 Heap statistics

- **Statement:** `Zenith_Heap::GetStats` reports live bytes, live allocations, peak bytes, total allocations and failed attempts maintained from the backing's usable size.
- **Covers:** Z2-I-CORE-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_Heap_Stats_TrackLivePeakTotal` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Memory.md` → "Statistics"

### Z2-R-CORE-012 Debug leak report

- **Statement:** In Debug builds the heap reports every live allocation at shutdown with its size and a symbolised site, and a test executable with a leak exits with code 3.
- **Covers:** Z2-I-CORE-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_Heap_LeakTable_ReportsLeak` (`Test` gate step, Debug presets); the test main's exit-3 behaviour exercised by `Test_FaultSweep_SelfTest_DetectsLeak`
- **Design:** `Zenith2/Docs/Design/CORE.md` → "Library and file layout"; `Zenith2/Docs/Design/CORE/Memory.md` → "Fault-injection support"

### Z2-R-CORE-013 Bare allocation boundary

- **Statement:** Bare allocation calls appear only under `Zenith2/Engine/ZenithCore/Internal/Memory/` and the platform backing files it names, and every third-party allocator CORE owns routes to the root heap.
- **Covers:** Z2-I-CORE-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `ForbiddenCheck` rules F1 and F2; `Test_JsonReader_BackendAllocations_UseRootHeap` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Memory.md` → "Backing calls per platform"; `Zenith2/Docs/Design/CORE/Text.md` → "JSON reader"

### Z2-R-CORE-014 Arena allocation and failure

- **Statement:** `Zenith_Arena::TryAllocate` follows the Contracts steps, is O(1) amortised, and leaves the arena unchanged on failure; `Allocate` at the cap terminates with `FATAL_CODE_ARENA_EXHAUSTED` naming the arena.
- **Covers:** Z2-I-CORE-04, Z2-I-CORE-17
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** Z2-B-CORE-005
- **Acceptance evidence:** `Test_Arena_Allocate_WithinBlock_ReturnsAlignedMemory`, `Test_Arena_TryAllocate_PastCap_FailsAndLeavesArenaUnchanged`, `Test_Arena_Allocate_AtCap_IsFatal` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Memory.md` → "Arena internals"; `Zenith2/Docs/Design/CORE/Memory.md` → "Memory tests"

### Z2-R-CORE-015 Arena reset and markers

- **Statement:** `Reset` keeps the retained blocks and `ResetToMarker` keeps every block and earlier data, and both fail `ZENITH_CHECK` while a non-trivially-destructible object constructed after the reset point is live.
- **Covers:** Z2-I-CORE-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_Arena_ResetToMarker_KeepsEarlierAllocations`, `Test_Arena_Reset_KeepsRetainedBlocks`, `Test_Arena_ResetToMarker_WithLiveObject_Checks` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Memory.md` → "Arena internals"

### Z2-R-CORE-016 Arena cap outcomes preset-independent

- **Statement:** The arena cap counts nominal bytes only, so one fixed allocation sequence succeeds and fails identically in every preset, sanitizer presets included.
- **Covers:** Z2-I-CORE-04, Z2-I-CORE-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_Arena_CapOutcomes_IdenticalAcrossPresets` (`Test` gate step in every preset)
- **Design:** `Zenith2/Docs/Design/CORE/Memory.md` → "Arena internals"

### Z2-R-CORE-017 Pool allocation

- **Statement:** `Zenith_Pool` hands out fixed slots in O(1) from pages with an out-of-line free list, and a pool filled to `m_uMaxSlots`, emptied and refilled succeeds every time and then fails with `ALLOC_ERROR_CAPACITY_EXCEEDED`.
- **Covers:** Z2-I-CORE-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_Pool_FillFreeRefill_AtCap_Succeeds`, `Test_Pool_Alloc1_15_16_17Bytes` (`Test` gate step, every preset)
- **Design:** `Zenith2/Docs/Design/CORE/Memory.md` → "Pool internals"

### Z2-R-CORE-018 Pool misuse

- **Statement:** Freeing a pointer the pool did not issue, or freeing a slot twice, fails `ZENITH_ASSERT`.
- **Covers:** Z2-I-CORE-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_Pool_FreeForeignPointer_Asserts`, `Test_Pool_DoubleFree_Asserts` (`z2-death-assert`)
- **Design:** `Zenith2/Docs/Design/CORE/Memory.md` → "Pool internals"

### Z2-R-CORE-019 Pool quarantine

- **Statement:** In sanitizer builds a freed slot waits in a FIFO quarantine before reuse while the caller-visible capacity stays exactly `m_uMaxSlots`.
- **Covers:** Z2-I-CORE-05, Z2-I-CORE-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040, Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** `Test_Pool_UseAfterFree_IsCaught` (`Sanitizers -Preset win-asan` gate step); `Test_Pool_FillFreeRefill_AtCap_Succeeds` in `win-asan`
- **Design:** `Zenith2/Docs/Design/CORE/Memory.md` → "Pool internals"

### Z2-R-CORE-020 ASan defect detection

- **Statement:** Under `win-asan` the arena and pool poisoning rules A1 to A7 make every intentional-defect test's defect produce an AddressSanitizer report.
- **Covers:** Z2-I-CORE-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040, Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** the `win-asan` rows of the table "Required intentional-defect tests" of `Zenith2/Docs/Contracts/Memory.md` (`z2-sanitizer-defect`, `Sanitizers -Preset win-asan` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Memory.md` → "Scope of this file"; `Zenith2/Docs/Design/CORE.md` → "Test strategy"

### Z2-R-CORE-021 HWASan defect detection on the S24

- **Statement:** On the S24 HWASan build the tagging rules H0 to H12 make every intentional-defect test's defect produce an HWAddressSanitizer report, with retag-on-free and 16 … 255 tags.
- **Covers:** Z2-I-CORE-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040, Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** the HWASan rows of the table "Required intentional-defect tests" and `Test_HwasanTag_RetagOnFree_ChangesTag`, run on the S24 in the Stage 2 acceptance device step (`needs-human`)
- **Design:** `Zenith2/Docs/Design/CORE/Memory.md` → "Scope of this file"; `Zenith2/Docs/Design/CORE.md` → "Test strategy"

### Z2-R-CORE-022 Zenith_Function

- **Statement:** `Zenith_Function` stores a trivially destructible capture, invocable with its signature, in the caller's arena, allocates nothing for an empty capture, allocates nothing on invocation, and its `TryCreate` fails with the arena's error.
- **Covers:** Z2-I-CORE-04, Z2-I-CORE-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040, Z2-D-041
- **Budgets:** none
- **Acceptance evidence:** `Test_Function_CaptureInArena_Invokes`, `Test_Function_EmptyCapture_AllocatesNothing`, `Test_Function_TryCreate_PastCap_Fails`, `Test_Function_InvokeNull_Asserts` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Memory.md` → "Zenith_Function"

### Z2-R-CORE-023 OOM handler protocol

- **Statement:** `Zenith_Heap::Allocate` never returns nullptr: it calls the OOM handler under `uLOCK_LEVEL_HEAP_OOM`, retries on `OOM_RESPONSE_RETRY` up to `m_uMaxOOMRetries`, and otherwise terminates with `FATAL_CODE_OUT_OF_MEMORY` carrying the request and the heap statistics.
- **Covers:** Z2-I-CORE-17
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_Heap_OOMHandler_RetryAfterRelease_Succeeds` (`Test` gate step), `Test_Heap_DefaultHandler_ReleasesReserveThenAborts` (`z2-death-check`)
- **Design:** `Zenith2/Docs/Design/CORE/Memory.md` → "Emergency reserve and the default handler"

### Z2-R-CORE-024 OOM inside the handler

- **Statement:** An allocation failure on a thread already inside the OOM handler terminates at once with `FATAL_CODE_OUT_OF_MEMORY`.
- **Covers:** Z2-I-CORE-17
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_Heap_OOMInHandler_Terminates` (`z2-death-check`)
- **Design:** `Zenith2/Docs/Design/CORE/Memory.md` → "Emergency reserve and the default handler"; `Zenith2/Docs/Design/CORE/Threading.md` → "Debug thread state"

### Z2-R-CORE-025 Default handler and pressure relay

- **Statement:** The default OOM handler raises the `Zenith_OOMPressureFlag` passed as user data, releases the emergency reserve once and retries, then aborts, and the governor turns a raised flag into CRITICAL at its next update.
- **Covers:** Z2-I-CORE-17, Z2-I-CORE-18
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_Heap_OOMRelay_RaisesPressureFlag`, `Test_MemoryGovernor_OOMRelay_ForcesCritical` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Memory.md` → "OOM pressure relay"; `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Level evaluation"

### Z2-R-CORE-026 Heap fault countdown

- **Statement:** `m_ulFailAfterAllocations` and `SetFaultCountdown(n)` make exactly the n-th allocation call of that heap fail and no other.
- **Covers:** Z2-I-CORE-17
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_Heap_FaultCountdown_FailsNthOnly` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Memory.md` → "Fault-injection counter"

### Z2-R-CORE-027 Fault injector and sweep

- **Statement:** `Zenith_FaultInjector` is the interface of the Contracts, and `Zenith_RunFaultSweep` fails every visited fault point and every allocation of a factory in turn, asserting no leak, no running thread and no registration left behind.
- **Covers:** Z2-I-CORE-17
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_FaultSweep_SelfTest_DetectsLeak` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Memory.md` → "Fault-injection support"

### Z2-R-CORE-028 Container set and justification records

- **Statement:** ZenithCore provides exactly the containers of the table "Container set", and each custom container, the bounded queue and the `<charconv>` choice has its justification record appended to `Zenith2/Docs/DecisionLog.md` in the diff that adds its first line of code.
- **Covers:** Z2-I-CORE-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-041, Z2-D-097
- **Budgets:** none
- **Acceptance evidence:** one `Zenith2/Docs/DecisionLog.md` entry per JR-CORE label, checked in the Stage 1 acceptance review; the coverage gate step's decision-heading checks
- **Design:** `Zenith2/Docs/Design/CORE/Containers.md` → "Container set"; `Zenith2/Docs/Design/CORE/Containers.md` → "Justification records"

### Z2-R-CORE-029 Container benchmarks

- **Statement:** `Zenith_Bench_CoreContainers` measures every justification record's case against its standard alternative, with a CTest smoke run and raw data under `Zenith2/Docs/Research/CoreContainers/`.
- **Covers:** Z2-I-CORE-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-041, Z2-D-084
- **Budgets:** none
- **Acceptance evidence:** `Bench_CoreContainers_Smoke` (`Test` gate step); the committed report cited by each record
- **Design:** `Zenith2/Docs/Design/CORE/Containers.md` → "Justification records"; `Zenith2/Docs/Design/CORE.md` → "Library and file layout"

### Z2-R-CORE-030 Span

- **Statement:** `Zenith_Span<T>` is a 16-byte trivially copyable view whose index and subspan operations assert their bounds and which never allocates.
- **Covers:** Z2-I-CORE-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-041
- **Budgets:** none
- **Acceptance evidence:** `Test_Span_MatchesOracle`, `Test_Span_Properties`, `Test_Span_Subspan_OutOfRange_Asserts`, `Test_Span_AsBytes_CountsBytes` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_Span"

### Z2-R-CORE-031 Array strong guarantee

- **Statement:** `Zenith_Array` Try operations leave the array unchanged on failure, grow by 1.5× with overflow-checked sizes, and allocate only from the root heap or the arena it was constructed with.
- **Covers:** Z2-I-CORE-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-041
- **Budgets:** none
- **Acceptance evidence:** `Test_Array_Growth_StrongGuarantee`, `Test_Array_ArenaTier_NeverFreesToHeap`, `Test_Array_TryFailure_LeavesUnchanged`, `Test_Array_SizeOverflow_IsRejected` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_Array"

### Z2-R-CORE-032 Array oracle and properties

- **Statement:** `Zenith_Array` matches `std::vector` over seeded random operation sequences and keeps its invariants, including for move-only element types.
- **Covers:** Z2-I-CORE-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-041
- **Budgets:** none
- **Acceptance evidence:** `Test_Array_MatchesOracle`, `Test_Array_Properties`, `Test_Array_MoveOnlyElement_OwnedPointer`, `Test_Array_RemoveAtSwap_MovesLast` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_Array"; `Zenith2/Docs/Design/CORE/Containers.md` → "Container tests"

### Z2-R-CORE-033 FixedArray layout

- **Statement:** `Zenith_FixedArray<T, N>` holds exactly N elements with the size, alignment and offsets of `T[N]`, so it is the C++ form of a reflected fixed array and of the Luau `Zenith.FixedArray<T>` view.
- **Covers:** Z2-I-CORE-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-041, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_FixedArray_LayoutMatchesCArray`, `Test_FixedArray_MatchesOracle`, `Test_FixedArray_Index_OutOfRange_Asserts` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_FixedArray"; `Zenith2/Docs/Design/CORE.md` → "Dependencies"

### Z2-R-CORE-034 InlineArray never allocates

- **Statement:** `Zenith_InlineArray` never allocates, reports `ALLOC_ERROR_CAPACITY_EXCEEDED` from Try forms at capacity, and fails `ZENITH_CHECK` from non-Try forms at capacity.
- **Covers:** Z2-I-CORE-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-041
- **Budgets:** none
- **Acceptance evidence:** `Test_InlineArray_NeverAllocates`, `Test_InlineArray_Full_TryPushFails`, `Test_InlineArray_PushFull_Checks`, `Test_InlineArray_MatchesOracle` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_InlineArray"

### Z2-R-CORE-035 HashMap deterministic iteration

- **Statement:** `Zenith_HashMap` iterates in dense entry order, a function of the Insert and Remove sequence alone, identical across capacities, runs and platforms.
- **Covers:** Z2-I-CORE-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-041, Z2-D-019, D-SEL-HASH (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_HashMap_IterationOrder_IndependentOfCapacity`, `Test_HashMap_IterationOrder_IdenticalAcrossRuns` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_HashMap and Zenith_HashSet"

### Z2-R-CORE-036 HashMap correctness

- **Statement:** `Zenith_HashMap` keeps every key findable through growth, backward-shift removal and full hash collisions, and matches `std::unordered_map` over seeded operation sequences.
- **Covers:** Z2-I-CORE-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-041, D-SEL-HASH (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_HashMap_MatchesOracle`, `Test_HashMap_Properties`, `Test_HashMap_RemoveBackwardShift_KeepsAllFindable`, `Test_HashMap_Collisions_AllKeysRetained`, `Test_HashMap_TryFailure_LeavesUnchanged`, `Test_HashMap_SizeOverflow_IsRejected` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_HashMap and Zenith_HashSet"

### Z2-R-CORE-037 HashMap key order for simulation

- **Statement:** `TryGetKeyOrder` returns entry indices sorted by key, and simulation code iterates hashed containers only in that order.
- **Covers:** Z2-I-CORE-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_HashMap_KeyOrder_SortsByKey` (`Test` gate step); review of simulation diffs
- **Design:** `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_HashMap and Zenith_HashSet"

### Z2-R-CORE-038 HashSet and heterogeneous lookup

- **Statement:** `Zenith_HashSet` shares the map's algorithm and contract and `GetKeys` returns its keys in iteration order, and string-keyed maps accept `Zenith_StringView` lookups through the traits' heterogeneous `Hash` and `Equal` overloads without constructing a key.
- **Covers:** Z2-I-CORE-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-041
- **Budgets:** none
- **Acceptance evidence:** `Test_HashSet_MatchesOracle`, `Test_HashSet_Properties`, `Test_HashSet_GetKeys_MatchesIterationOrder`, `Test_HashMap_HeterogeneousLookup_StringView` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_HashMap and Zenith_HashSet"

### Z2-R-CORE-039 RingBuffer full policies

- **Statement:** `Zenith_RingBuffer` implements REJECT, GROW_THEN_REJECT, DROP_OLDEST, DROP_NEWEST and FATAL exactly as its policy table states and rejects blocking policies at construction.
- **Covers:** Z2-I-CORE-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-041
- **Budgets:** none
- **Acceptance evidence:** `Test_RingBuffer_Reject_ReturnsFull`, `Test_RingBuffer_GrowThenReject_GrowsToHardCap`, `Test_RingBuffer_DropOldest_OverwritesFront`, `Test_RingBuffer_DropNewest_DiscardsPushed`, `Test_RingBuffer_Fatal_Terminates`, `Test_RingBuffer_BlockPolicy_Checks`, `Test_RingBuffer_MatchesOracle` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_RingBuffer"

### Z2-R-CORE-040 Queue counters

- **Statement:** Every CORE queue publishes `<name>.full`, `<name>.dropped` and `<name>.high_water` through the installed counter hook whenever one of them changes.
- **Covers:** Z2-I-CORE-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-041
- **Budgets:** none
- **Acceptance evidence:** `Test_RingBuffer_Counters_PublishedOnChange` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_RingBuffer"; `Zenith2/Docs/Design/CORE.md` → "Diagnostics and profiling"

### Z2-R-CORE-041 BoundedQueue MPMC correctness

- **Statement:** `Zenith_BoundedQueue` delivers every pushed item exactly once with any number of producers and consumers, and is TSan clean.
- **Covers:** Z2-I-CORE-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-041
- **Budgets:** none
- **Acceptance evidence:** `Test_BoundedQueue_MPMC_NoLossNoDuplicate` (`z2-stress`, `Test` gate step and `Sanitizers -Preset linux-tsan` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_BoundedQueue"

### Z2-R-CORE-042 BoundedQueue blocking rules

- **Statement:** `PushWait` and `PopWait` block only on threads that are not job workers, wake on space or items, time out, and release every waiter on `WakeAllWaiters`; a non-power-of-two capacity or a growing policy is rejected at `Create`.
- **Covers:** Z2-I-CORE-07, Z2-I-JOBS-06 (the Z2-I-JOBS items are JOBS's; CORE owns their ZenithCore half)
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-041, Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_BoundedQueue_BlockProducer_WakesOnPop`, `Test_BoundedQueue_PopWait_TimesOut`, `Test_BoundedQueue_WakeAllWaiters_ReleasesConsumers`, `Test_BoundedQueue_PushWaitOnJobWorker_Checks`, `Test_BoundedQueue_NonPowerOfTwo_Checks` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_BoundedQueue"

### Z2-R-CORE-043 BitArray fixed capacity

- **Statement:** `Zenith_BitArray` is a fixed-capacity bit set sized once by `TryInitialise`, keeps bits beyond its count zero, and leaves its previous contents on a failed re-initialisation.
- **Covers:** Z2-I-CORE-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-041
- **Budgets:** none
- **Acceptance evidence:** `Test_BitArray_TailBitsStayZero`, `Test_BitArray_FindFirst_AcrossWords`, `Test_BitArray_ReinitialiseFailure_KeepsContents`, `Test_BitArray_MatchesOracle`, `Test_BitArray_Index_OutOfRange_Asserts` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_BitArray"; `Zenith2/Docs/Design/CORE.md` → "Dependencies"

### Z2-R-CORE-044 Deterministic sorting

- **Statement:** `Zenith_Sort` and `Zenith_StableSort` sort correctly for adversarial inputs, give identical output for identical input on every platform, and the stable sort preserves the order of equal elements.
- **Covers:** Z2-I-CORE-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-041, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Sort_AdversarialInputs_Sorted`, `Test_Sort_Deterministic_IdenticalOutput`, `Test_StableSort_PreservesEqualOrder`, `Test_StableSort_ScratchFailure_LeavesUnchanged` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Containers.md` → "Sorting and searching"

### Z2-R-CORE-045 StringView contract

- **Statement:** `Zenith_StringView` is a 16-byte (pointer, byte length) view with unsigned byte-wise ordering and asserted bounds, and its ASCII helpers trim and fold ASCII bytes only.
- **Covers:** Z2-I-CORE-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-041
- **Budgets:** none
- **Acceptance evidence:** `Test_StringView_Compare_ByteOrder`, `Test_StringView_Find_Cases`, `Test_StringView_AsciiHelpers`, `Test_StringView_Substr_OutOfRange_Asserts` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Text.md` → "Zenith_StringView"

### Z2-R-CORE-046 String UTF-8 invariant

- **Statement:** `Zenith_String` always holds NUL-terminated valid UTF-8 without embedded NUL; invalid, NUL-carrying or over-long input is rejected with the string unchanged.
- **Covers:** Z2-I-CORE-08, Z2-I-CORE-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-041
- **Budgets:** none
- **Acceptance evidence:** `Test_String_Properties`, `Test_String_EmbeddedNul_Rejected`, `Test_String_TryFailure_LeavesUnchanged`, `Test_String_SizeOverflow_IsRejected` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Text.md` → "Zenith_String"; `Zenith2/Docs/Design/CORE/Text.md` → "Encoding policy"

### Z2-R-CORE-047 String oracle

- **Statement:** `Zenith_String` matches `std::string` over seeded append, assign and clear sequences.
- **Covers:** Z2-I-CORE-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-041
- **Budgets:** none
- **Acceptance evidence:** `Test_String_MatchesOracle` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Text.md` → "Zenith_String"

### Z2-R-CORE-048 Interned string identity

- **Statement:** `Zenith_StringTable::Intern` returns one 4-byte `Zenith_StringID` per distinct text, 0 for the empty string, from any thread, and `GetText` returns a view that stays valid for the table's lifetime.
- **Covers:** Z2-I-CORE-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-020, D-SEL-HASH (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_StringTable_InternTwice_SameID`, `Test_StringTable_EmptyIsZero`, `Test_StringTable_TextViewsStableAcrossGrowth`, `Test_StringTable_ConcurrentIntern_Consistent` (`Test` gate step and `Sanitizers -Preset linux-tsan` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Text.md` → "String interning"

### Z2-R-CORE-049 String table caps

- **Statement:** The intern table rejects an invalid desc (a zero cap, a page size that is not a power of two from 64 to 65,536, a text chunk smaller than one maximal string) with `STRING_ERROR_INVALID_DESC` before allocating, enforces its string and byte caps with `STRING_ERROR_TABLE_FULL`, stays usable afterwards, issues 1-based IDs with 0 the empty string, and fails `ZENITH_CHECK` on `GetText` of an ID it never issued.
- **Covers:** Z2-I-CORE-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_StringTable_Create_RejectsInvalidDesc`, `Test_StringTable_EmptyIsZero`, `Test_StringTable_CapReached_ReturnsTableFull`, `Test_StringTable_GetTextUnknown_Checks` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Text.md` → "String interning"; `Zenith2/Docs/Design/CORE.md` → "Budget rows"

### Z2-R-CORE-050 String IDs never order simulation

- **Statement:** A `Zenith_StringID`'s numeric value never affects a simulation result: simulation compares IDs for equality only and orders, hashes and serializes interned strings by their text.
- **Covers:** Z2-I-CORE-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_StringTable_IDsDifferAcrossInternOrder_TextHashesEqual` (`Test` gate step); the determinism replay matrix of `Zenith2/Docs/Scripting.md` → "Determinism rules and randomness"
- **Design:** `Zenith2/Docs/Design/CORE/Text.md` → "String interning"

### Z2-R-CORE-051 UTF-8 validation

- **Statement:** `Zenith_ValidateUtf8` accepts exactly the well-formed sequences of the Unicode Standard and reports the first failure's code and offset.
- **Covers:** Z2-I-CORE-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Utf8_Validate_Corpus`, `Test_Utf8_RoundTripAllScalars` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Text.md` → "Unicode functions"

### Z2-R-CORE-052 UTF-16 conversion

- **Statement:** UTF-8 to UTF-16 and back is the identity for every scalar value, and an unpaired surrogate is rejected.
- **Covers:** Z2-I-CORE-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Utf16_RoundTrip_AndUnpaired` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Text.md` → "Unicode functions"

### Z2-R-CORE-053 Unicode policy

- **Statement:** Zenith2 performs no Unicode normalisation, compares text byte for byte, keeps identifiers ASCII-only through `Zenith_IsAsciiIdentifier`, and folds case for ASCII alone.
- **Covers:** Z2-I-CORE-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Unicode_AsciiIdentifier_Rules` (`Test` gate step); review
- **Design:** `Zenith2/Docs/Design/CORE/Text.md` → "Encoding policy"

### Z2-R-CORE-054 UTF-8 fuzzing

- **Statement:** The UTF-8 validator is fuzzed with libFuzzer and agrees with a reference decoder without out-of-bounds reads.
- **Covers:** Z2-I-CORE-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** `Zenith_Fuzz_Utf8` 60-second smoke (`Sanitizers -Preset win-asan` gate step); full duration at each stage acceptance
- **Design:** `Zenith2/Docs/Design/CORE/Text.md` → "Text tests"

### Z2-R-CORE-055 Checked formatting

- **Statement:** `Zenith_FormatTo` checks placeholders against argument kinds at compile time, formats every kind as its table states, and reports the full length on truncation without splitting a UTF-8 sequence.
- **Covers:** Z2-I-CORE-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Format_Placeholders_AllKinds`, `Test_Format_Truncation_ReportsFullLength`, `Test_Format_MismatchedPlaceholders_DoNotCompile` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Text.md` → "Formatting"

### Z2-R-CORE-056 Number text

- **Statement:** Integers are parsed by Zenith code with the stated grammar, doubles through the D-SEL-JSON backend's correctly rounded parser, and floats and doubles are formatted as their shortest round-trip text with `std::to_chars` confined to `Internal/Text/`.
- **Covers:** Z2-I-CORE-08, Z2-I-CORE-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-041, D-SEL-JSON (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_Parse_Integers_Grammar`, `Test_Parse_Double_Grammar`, `Test_Parse_Double_LongText_OutOfRange`, `Test_Format_DoubleShortest_RoundTrips` (`Test` gate step, every preset); `ForbiddenCheck` F4 with the JR-CORE-CHARCONV allowlist entry
- **Design:** `Zenith2/Docs/Design/CORE/Text.md` → "Number conversion"

### Z2-R-CORE-057 Numerical conventions

- **Statement:** CORE maths uses IEEE operations and CORE's deterministic transcendental functions alone (never a C runtime transcendental), scalar per lane, with checked helpers for every unsigned computation that may overflow.
- **Covers:** Z2-I-CORE-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Maths_CheckedArithmetic_Overflow`, `Test_Vector_Operations_MatchReference` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Maths.md` → "Numerical conventions"

### Z2-R-CORE-058 Degenerate inputs

- **Statement:** Operations undefined for their input have a checked form returning `bool` or a fallback, and the unchecked form asserts its precondition.
- **Covers:** Z2-I-CORE-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_Vector_TryNormalize_Degenerate`, `Test_Vector_Normalize_Zero_Asserts`, `Test_Matrix_Inverse_TimesSelfIsIdentity` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Maths.md` → "Numerical conventions"; `Zenith2/Docs/Design/CORE/Maths.md` → "Vector types"

### Z2-R-CORE-059 Narrowing checks

- **Statement:** `Zenith_NarrowCast` and `Zenith_TryNarrowCast` check range, sign and round trip before casting and pass the Contracts' narrowing tests, including under UBSan.
- **Covers:** Z2-I-CORE-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** `Test_NarrowCast_InRange_Passes`, `Test_NarrowCast_OutOfRange_Asserts`, `Test_TryNarrowCast_OutOfRange_ReturnsError`, `Test_TryNarrowCast_FloatToIntOverflow_ReturnsError`, `Test_TryNarrowCast_SignChange_ReturnsError`, `Test_TryNarrowCast_PrecisionLoss_ReturnsError` (`Test` gate step; the overflow case also `Sanitizers -Preset linux-tsan` gate step)
- **Design:** `Zenith2/Docs/Design/CORE.md` → "Library and file layout"; `Zenith2/Docs/Design/CORE.md` → "Test strategy"

### Z2-R-CORE-060 Units in contracts

- **Statement:** Every CORE public type's contract block names its units per the table "Units", and every quantity outside SI base units carries its unit suffix.
- **Covers:** Z2-I-CORE-11
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-047
- **Budgets:** none
- **Acceptance evidence:** `ApiContractCheck` (the `Units` field present and non-empty); review of the units against the table
- **Design:** `Zenith2/Docs/Design/CORE/Maths.md` → "Units"

### Z2-R-CORE-061 Colour space

- **Statement:** `Zenith_Colour` is linear Rec.709 with straight alpha, and the sRGB conversions use the piecewise transfer function so every 8-bit value round-trips, the packed 8-bit form is R in bits 0–7 through A in bits 24–31 after clamping, and premultiplication scales RGB by alpha alone.
- **Covers:** Z2-I-CORE-11
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Colour_SrgbRoundTrip_AllBytes`, `Test_Colour_Srgb8Packed_Layout`, `Test_Colour_Premultiply_ScalesRgbOnly` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_Colour"

### Z2-R-CORE-062 Coordinate system

- **Statement:** World and object space are right-handed with +Y up, model front +Z and view forward −Z, and the maths types' cross product, look-at and look-rotation follow it.
- **Covers:** Z2-I-CORE-12
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Vector_Cross_RightHanded`, `Test_Matrix_LookAt_LooksDownNegativeZ`, `Test_Quaternion_LookRotation_ModelFront` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Maths.md` → "Coordinate systems"

### Z2-R-CORE-063 Clip-space convention

- **Statement:** The projection builders produce +Y-up clip space with depth in [0, 1], standard Z mapping near to 0 and reversed Z mapping near to 1, including the reversed infinite form.
- **Covers:** Z2-I-CORE-12
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-061
- **Budgets:** none
- **Acceptance evidence:** `Test_Matrix_Perspective_DepthMapping` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Maths.md` → "Matrix types"; `Zenith2/Docs/Design/CORE/Maths.md` → "Coordinate systems"

### Z2-R-CORE-064 Euler convention

- **Statement:** Euler angles are (yaw, pitch, roll) about (+Y, +X, +Z) composed yaw then pitch then roll with the right-hand rule, and yaw 0 faces +Z.
- **Covers:** Z2-I-CORE-12
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Quaternion_EulerConvention` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Maths.md` → "Coordinate systems"; `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_Quaternion"

### Z2-R-CORE-065 Vector layouts

- **Statement:** `Zenith_Vector2/3/4`, `Zenith_Quaternion` and `Zenith_Colour` have the sizes, alignment 4 and member order of the reflected vec2, vec3, vec4, quat and colour layouts.
- **Covers:** Z2-I-CORE-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-020, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_Vector_Layout_MatchesSchema` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Maths.md` → "Vector types"

### Z2-R-CORE-066 Matrix layout

- **Statement:** `Zenith_Matrix4` is row-major with column vectors and its first three rows are the GPU scene's row-major float3x4.
- **Covers:** Z2-I-CORE-10, Z2-I-CORE-12
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Matrix_Rows3x4_MatchGpuSceneLayout` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Maths.md` → "Matrix types"

### Z2-R-CORE-067 Matrix operations

- **Statement:** TRS composition and decomposition, inversion and the affine inverse agree with their definitions within the stated tolerances; `Zenith_Matrix3` rotations are orthonormal, its normal matrix keeps normals perpendicular under non-uniform scale, `Zenith_MakeScale3` builds the diagonal scale and the asserting `Zenith_Inverse` equals `Zenith_TryInverse` for invertible input; 2D transforms are `Zenith_Matrix4` values with z = 0.
- **Covers:** Z2-I-CORE-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Matrix_TRS_ComposeDecompose`, `Test_Matrix_Inverse_TimesSelfIsIdentity`, `Test_Matrix3_FromQuaternion_Orthonormal`, `Test_Matrix3_NormalMatrix_NonUniformScale`, `Test_Matrix3_InverseAndScale` (`Test` gate step); `Test_Matrix3_InverseSingular_Asserts` (`z2-death-assert`)
- **Design:** `Zenith2/Docs/Design/CORE/Maths.md` → "Matrix types"; `Zenith2/Docs/Design/CORE/Maths.md` → "Coordinate systems"

### Z2-R-CORE-068 Quaternion operations

- **Statement:** Quaternion products compose like matrices, slerp takes the shortest path and hits its endpoints, and from-to handles antiparallel inputs.
- **Covers:** Z2-I-CORE-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Quaternion_ProductOrder_MatchesMatrix`, `Test_Quaternion_Slerp_ShortestPathAndEndpoints`, `Test_Quaternion_FromTo_Antiparallel` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_Quaternion"

### Z2-R-CORE-069 Bounds tests

- **Statement:** Frustum culling of boxes is conservative for every depth mode, and ray tests handle zero direction components.
- **Covers:** Z2-I-CORE-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Bounds_FrustumCulling_Conservative`, `Test_Bounds_RayAABB_ZeroDirectionComponents` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Maths.md` → "Bounds types"

### Z2-R-CORE-070 Hash64 implementation

- **Statement:** `Zenith_Hash64` is the D-SEL-HASH function with seed 0 by default, bit-identical on Windows and Android for the committed vectors, and its stream form equals the one-shot form.
- **Covers:** Z2-I-CORE-07, Z2-I-CORE-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019, D-SEL-HASH (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_Hash64_KnownVectors_MatchReference`, `Test_Hash64_CrossPlatform_Identical`, `Test_Hash64_StreamEqualsOneShot` (`Test` gate step, `win-release` and `android-arm64`)
- **Design:** `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_Hash64"

### Z2-R-CORE-071 Hash128 for persisted keys

- **Statement:** `Zenith_Hash128Of` gives the persisted-key hash of the ruled path, and persisted and cross-platform hash inputs come from explicit little-endian serialisation.
- **Covers:** Z2-I-CORE-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038, D-SEL-HASH (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_Hash64_KnownVectors_MatchReference` (128-bit vectors) (`Test` gate step); review
- **Design:** `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_Hash64"

### Z2-R-CORE-072 D-SEL-HASH selection

- **Statement:** The D-SEL-HASH harness measures XXH3-64/XXH3-128, XXH64 and a hand-rolled baseline for short keys, long inputs, collisions and cross-platform output, and the ruling ticket records the choice.
- **Covers:** Z2-I-CORE-07, Z2-I-CORE-13
- **Stage:** S1
- **Classification:** research
- **Decisions:** Z2-D-084, D-SEL-HASH (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Zenith2/Benchmarks/HashSelection/` smoke in the `Test` gate step; the report under `Zenith2/Docs/Research/HashSelection/`; the ruling entry in `Zenith2/Docs/DecisionLog.md`
- **Design:** `Zenith2/Docs/Design/CORE.md` → "Decisions"; `Zenith2/Docs/Design/CORE.md` → "Library and file layout"

### Z2-R-CORE-073 Counter-based random draws

- **Statement:** Each `Zenith_RandomSource` draw is `Zenith_Hash64` of the 24-byte little-endian block (seed, stream, counter), so restoring the counter replays the stream.
- **Covers:** Z2-I-CORE-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019, D-SEL-HASH (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_RandomSource_SequenceGolden`, `Test_RandomSource_CounterRestore_Replays` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_RandomSource"

### Z2-R-CORE-074 Shared random derivations

- **Statement:** The unit-interval, range, unit-vector and shuffle derivations equal the formulas of Scripting's table "Random API" bit for bit, and Scripting's `ctx.Random` calls these CORE functions rather than a second implementation.
- **Covers:** Z2-I-CORE-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_RandomSource_DerivationsMatchScriptTable` (`Test` gate step), whose reference values are computed in the test from the Scripting table's formulas written out independently of CORE's code; the SCRIPT dependency is recorded in the cross-area notes (C17)
- **Design:** `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_RandomSource"

### Z2-R-CORE-075 Unbiased ranges

- **Statement:** `Range(a, b)` uses Lemire's method with rejection, is unbiased and exact at its bounds, including the 2^32 span.
- **Covers:** Z2-I-CORE-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_RandomSource_Range_Unbiased` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_RandomSource"

### Z2-R-CORE-076 Entropy only outside simulation

- **Statement:** `CreateFromEntropy` seeds from the OS generator for identities and non-simulation variety, and runtime code draws randomness only through `Zenith_RandomSource`.
- **Covers:** Z2-I-CORE-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_RandomSource_Entropy_Distinct` (`Test` gate step); `ForbiddenCheck` F14; review of simulation diffs
- **Design:** `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_RandomSource"

### Z2-R-CORE-077 Persistent ID generation

- **Statement:** `Zenith_GeneratePersistentID` produces RFC 9562 version-4 IDs from two draws of a `Zenith_RandomSource`, never the null ID.
- **Covers:** Z2-I-CORE-13, Z2-I-CORE-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_PersistentID_Generate_VersionAndVariant` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_PersistentID functions"

### Z2-R-CORE-078 Persistent ID forms

- **Statement:** The persistent-ID text form is 36 lowercase characters, high word first, parsed strictly, and the binary form is the two words little-endian; fuzzed parsing never accepts a non-canonical text.
- **Covers:** Z2-I-CORE-08, Z2-I-CORE-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_PersistentID_TextRoundTrip`, `Test_PersistentID_Parse_Rejects`, `Test_PersistentID_BinaryLittleEndian` (`Test` gate step); `Zenith_Fuzz_PersistentID` (`Sanitizers -Preset win-asan` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_PersistentID functions"

### Z2-R-CORE-079 Monotonic clock

- **Statement:** `Zenith_GetMonotonicTimeUs` and `Zenith_GetMonotonicTimeNs` never decrease on any thread and are implemented per platform without intermediate overflow, and `Zenith_Stopwatch` measures elapsed microseconds on that clock.
- **Covers:** Z2-I-CORE-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Clock_Monotonic_NeverDecreases`, `Test_Clock_Stopwatch_Elapsed` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Clocks"

### Z2-R-CORE-080 UTC clock formats

- **Statement:** UTC time formats to ISO 8601 with microseconds and to the compact backup-directory timestamp.
- **Covers:** Z2-I-CORE-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Clock_Utc_FormatsIso8601` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Clocks"

### Z2-R-CORE-081 Clock placement

- **Statement:** The clocks live in ZenithCore so that libraries at layers 0 to 2, the profiler included, read them with no dependency on ZenithPlatform.
- **Covers:** Z2-I-CORE-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-007
- **Budgets:** none
- **Acceptance evidence:** `Libgraph` (no edge from ZenithProfiling or ZenithJobs to ZenithPlatform)
- **Design:** `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Clocks"; `Zenith2/Docs/Design/CORE.md` → "Reconciliation items"

### Z2-R-CORE-082 Simulation reads no clock

- **Statement:** Simulation code reads neither clock; the frame loop converts monotonic deltas into ticks through the JOBS simulation clock.
- **Covers:** Z2-I-CORE-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `ForbiddenCheck` F14; review of simulation diffs; the determinism replay matrix
- **Design:** `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Clocks"

### Z2-R-CORE-083 Assertion macros

- **Statement:** `ZENITH_ASSERT` is enabled exactly where `ZENITH_ASSERTS_ENABLED` is 1 and otherwise type-checks its condition without evaluating it, while `ZENITH_CHECK`, `ZENITH_FATAL` and `ZENITH_UNREACHABLE` are enabled in every configuration.
- **Covers:** Z2-I-CORE-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_Assert_Disabled_DoesNotEvaluate`, `Test_Assert_Failure_ReportsFormat`, `Test_Check_Failure_ReportsFormat`, `Test_Unreachable_Reports` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Assertions and fatal errors"

### Z2-R-CORE-084 Failure report

- **Statement:** A failure writes the Contracts' first line, then the backtrace, emits a fatal log record when a log hook is installed, breaks into an attached debugger for asserts, and terminates so an installed crash handler sees it.
- **Covers:** Z2-I-CORE-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_Assert_Failure_ReportsFormat`, `Test_Fatal_ReportsCodeName`, `Test_Log_Router_Fatal_FlushesSynchronously` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Failure procedure"; `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Diagnostic output and backtraces"

### Z2-R-CORE-085 Recursive failure

- **Statement:** A failure raised while a failure is being reported on the same thread terminates at once with the recursive-failure line.
- **Covers:** Z2-I-CORE-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_Fail_RecursiveFailure_Terminates` (`z2-death-check`)
- **Design:** `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Failure procedure"

### Z2-R-CORE-086 Result implementation

- **Statement:** `Zenith_Result<T, E>`, `Zenith_ErrorType` and `Zenith_GetErrorCode` implement `Zenith2/Docs/Contracts.md` → "Errors and results" exactly ([Z2-N-CONTRACT-480], [Z2-N-CONTRACT-481]) for error enumerations and for error structs of at most 32 bytes: a result stores exactly a `T` and an `E` (only the `E` for `void`), with the same layout in every build; it is a success exactly when its error code is 0; `T` must be default-constructible; reading the wrong state is `ZENITH_ASSERT`, checked only where asserts are enabled; and CORE's two error structs fit the 32-byte bound.
- **Covers:** Z2-I-CORE-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_Result_SuccessAndError_States`, `Test_Result_Layout_ValuePlusError`, `Test_Result_WrongStateAccess_UncheckedWhereAssertsOff`, `Test_Result_NonDefaultConstructibleValue_DoesNotCompile` (`Test` gate step); `Test_Result_WrongStateAccess_Asserts` (`z2-death-assert`); the `static_assert(sizeof(…) <= 32)` beside `Zenith_JsonReadError` and `Zenith_MemoryError` (`Build`)
- **Design:** `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Results and the error taxonomy"

### Z2-R-CORE-087 Result propagation

- **Statement:** `ZENITH_TRY` and `ZENITH_TRY_ASSIGN` evaluate their expression once and propagate an error unchanged within one error type.
- **Covers:** Z2-I-CORE-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045, Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** `Test_Result_Try_PropagatesSameEnum` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Results and the error taxonomy"

### Z2-R-CORE-088 Error taxonomy

- **Statement:** Every CORE error enumerator maps to a `Zenith_ErrorCategory` and has `Zenith_ToString` text, numbered from 1.
- **Covers:** Z2-I-CORE-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_ErrorCategory_EveryCoreEnumeratorMapped` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Error taxonomy"

### Z2-R-CORE-089 Build information

- **Statement:** The configuration defines are read only through `Zenith_BuildInfo.h`'s constexpr readers, every define is 0 or 1 in every target, and exactly one platform and one configuration are 1.
- **Covers:** Z2-I-CORE-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-001
- **Budgets:** none
- **Acceptance evidence:** `Test_BuildInfo_ExactlyOnePlatformAndConfiguration` (`Test` gate step); `ForbiddenCheck` F11
- **Design:** `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Build information"

### Z2-R-CORE-090 Log levels and compile-out

- **Statement:** Log macros compile out records below the Shipping floor or the category's compiled minimum, so their arguments are not evaluated.
- **Covers:** Z2-I-CORE-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Log_CompiledOut_InShipping` (`Test` gate step, Shipping test build)
- **Design:** `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Levels and categories"; `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Log macros"

### Z2-R-CORE-091 Log without a router

- **Statement:** With no log hook installed, warnings and errors reach the diagnostic output and lower levels are dropped.
- **Covers:** Z2-I-CORE-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Log_NoHook_WarningsToDiagnosticOutput` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Log macros"

### Z2-R-CORE-092 Log router fan-out

- **Statement:** The log router filters by category override and global level, assigns strictly increasing sequence numbers, and delivers every admitted record with its fields to every sink once.
- **Covers:** Z2-I-CORE-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Log_Router_FanOutAndSequence`, `Test_Log_Router_CategoryOverrideFilters`, `Test_Log_Fields_DeliveredToSinks` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Log router"

### Z2-R-CORE-093 Log sinks and drain

- **Statement:** Sinks do memory work only under `uLOCK_LEVEL_LOG_SINKS`, output happens on the router's drain thread (the failing thread on the fatal path), the ring drops the oldest records when full and publishes its `.full`, `.dropped` and `.high_water` counters, and concurrent logging loses nothing that fits.
- **Covers:** Z2-I-CORE-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Log_Router_RingOverflow_DropsOldest`, `Test_Log_Router_ConcurrentLogging_NoLoss` (`Test` gate step and `Sanitizers -Preset linux-tsan` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Log router"

### Z2-R-CORE-094 Fatal log flush

- **Statement:** A fatal record is written and flushed by every sink synchronously on the failing thread, unless the failing thread already holds a log lock, in which case the record reaches the diagnostic output alone.
- **Covers:** Z2-I-CORE-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Log_Router_Fatal_FlushesSynchronously`, `Test_Fail_RecursiveFailure_Terminates` (`z2-death-check`)
- **Design:** `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Log router"

### Z2-R-CORE-095 Log hook without a new global

- **Statement:** The log router is reached through the log members of the installed profiler-hook table, so logging adds no writable data symbol to the allowlist.
- **Covers:** Z2-I-CORE-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `ForbiddenCheck` F16 with the four-entry allowlist unchanged; `Test_Log_Router_FanOutAndSequence`
- **Design:** `Zenith2/Docs/Design/CORE/Diagnostics.md` → "The log-record hook"

### Z2-R-CORE-096 Log message truncation

- **Statement:** A message longer than 2,047 bytes is truncated on a UTF-8 boundary and the record says so.
- **Covers:** Z2-I-CORE-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Log_Truncation_MarksRecord` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Log macros"

### Z2-R-CORE-097 Threads joined by their owner

- **Statement:** `Zenith_Thread` is created through `Create` with name, stack, priority and affinity, is joined by exactly one owner, and a destroyed joinable thread fails `ZENITH_CHECK`.
- **Covers:** Z2-I-JOBS-01 (the Z2-I-JOBS items are JOBS's; CORE owns their ZenithCore half)
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_Thread_CreateJoin_RunsEntryOnce`, `Test_Thread_DestroyJoinable_Checks`, `Test_Thread_Create_FailsAtStartBlock_Unwinds`, `Test_Thread_InvalidDesc_Rejected` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Threading.md` → "Threads"

### Z2-R-CORE-098 POSIX parity

- **Statement:** The Android and Linux threading implementations behave identically across the threading suite.
- **Covers:** Z2-I-JOBS-01 (the Z2-I-JOBS items are JOBS's; CORE owns their ZenithCore half)
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Threading_PosixParity` (`Test` gate step under `linux-tsan` and `android-x86_64`)
- **Design:** `Zenith2/Docs/Design/CORE/Threading.md` → "Threads"

### Z2-R-CORE-099 Lock-order checker

- **Statement:** Every `Zenith_Mutex` and `Zenith_MutexNoProfiling` acquisition is checked against the lock-ordering table wherever asserts are enabled: an inversion, a same-level descending address, a recursive lock, an unregistered level or a 17th held lock is reported before it can deadlock, while try-locks skip the order check.
- **Covers:** Z2-I-CORE-15, Z2-I-JOBS-09 (the Z2-I-JOBS items are JOBS's; CORE owns their ZenithCore half)
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_LockOrder_Inversion_IsReported`, `Test_LockOrder_SameLevelDescendingAddress_IsReported`, `Test_LockOrder_AscendingLevels_Passes`, `Test_LockOrder_TryLockOutOfOrder_Allowed`, `Test_LockOrder_Recursive_Checks`, `Test_LockOrder_UnregisteredLevel_Checks`, `Test_LockOrder_Overflow_Checks`, `Test_LockOrder_NoProfilingMutex_IsChecked` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Threading.md` → "Mutexes and lock ordering"

### Z2-R-CORE-100 Lock rows added

- **Statement:** The lock levels 380, 850, 930 and 980 are rows of the Contracts' lock-ordering table, added to Contracts through the core-doc batch (cross-document item X163); the constants are registered in `auLOCK_ORDER_REGISTERED_LEVELS`.
- **Covers:** Z2-I-CORE-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_LockOrder_UnregisteredLevel_Checks` fails for any constant not registered in `auLOCK_ORDER_REGISTERED_LEVELS`; the four rows present in Contracts → "Threading and concurrency contract" once X163 is applied
- **Design:** `Zenith2/Docs/Design/CORE/Threading.md` → "Mutexes and lock ordering"

### Z2-R-CORE-101 Mutex contention

- **Statement:** Mutexes serialise correctly under 2, 8 and hardware-concurrency threads and are TSan clean.
- **Covers:** Z2-I-JOBS-01 (the Z2-I-JOBS items are JOBS's; CORE owns their ZenithCore half)
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Mutex_Contention_Stress` (`z2-stress`, `Sanitizers -Preset linux-tsan` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Threading.md` → "Mutexes and lock ordering"

### Z2-R-CORE-102 Blocking waits

- **Statement:** Condition variables and semaphores time out on the monotonic clock, refuse short-hold mutexes, fail `ZENITH_CHECK` while a job body runs on the thread (between `Zenith_MarkThreadAsJobWorker(true)` and `(false)`) but not in a worker's idle park between bodies, and every blocking primitive of the profiled variants calls the blocking-wait hook; the unprofiled variants call no hook but make the same job-worker check.
- **Covers:** Z2-I-JOBS-06, Z2-I-JOBS-09 (the Z2-I-JOBS items are JOBS's; CORE owns their ZenithCore half)
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_ConditionVariable_WaitFor_TimesOut`, `Test_ConditionVariable_ShortHoldMutex_Checks`, `Test_Semaphore_PostWait_Counts`, `Test_BlockingWait_OnJobWorker_Checks`, `Test_BlockingWait_BetweenJobBodies_Allowed`, `Test_BlockingWait_CallsHook`, `Test_BlockingWait_NoProfilingVariants_StillCheckJobWorker`, `Test_ConditionVariableNoProfiling_NeverCallsHooks`, `Test_SemaphoreNoProfiling_NeverCallsHooks` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Threading.md` → "Condition variables and semaphores"

### Z2-R-CORE-103 Debug thread state

- **Statement:** The thread-local Debug thread state has the same layout in every configuration and carries the held-lock stack, the job-worker flag, the OOM and failure flags, the no-allocation depth, and the hook depth and hook re-entry count of the dispatch guard.
- **Covers:** Z2-I-CORE-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `ForbiddenCheck` F9 and F16 (the variable is the allowlisted `g_xDebugThreadState`); `Test_Heap_OOMInHandler_Terminates`; `Test_Fail_RecursiveFailure_Terminates`; `Test_HookDispatch_DepthIsOneInsideHook`, `Test_HookDispatch_Reentry_CountedPerThread`
- **Design:** `Zenith2/Docs/Design/CORE/Threading.md` → "Debug thread state"

### Z2-R-CORE-104 Reentrancy guard

- **Statement:** `Zenith_ReentrancyGuard` is 8 bytes in every configuration and asserts a same-thread call from inside a callback while allowing other threads.
- **Covers:** Z2-I-CORE-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_ReentrancyGuard_CallbackReentry_Asserts`, `Test_ReentrancyGuard_OtherThread_Passes` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Threading.md` → "Zenith_ReentrancyGuard"

### Z2-R-CORE-105 Callback registry lifetime

- **Statement:** The callback registry never waits on self-unregistration, skips entries removed during dispatch, waits for an in-flight call on cross-thread unregistration, defers destruction requested during dispatch, and does not call entries registered during the dispatch.
- **Covers:** Z2-I-CORE-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_CallbackRegistry_SelfUnregister_DoesNotWait`, `Test_CallbackRegistry_UnregisterOtherDuringDispatch_SkipsIt`, `Test_CallbackRegistry_CrossThreadUnregister_WaitsForInFlight` (also `Sanitizers -Preset linux-tsan` gate step), `Test_CallbackRegistry_DestroyDuringDispatch_IsDeferred`, `Test_CallbackRegistry_RegisterDuringDispatch_NotCalledThisDispatch` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Threading.md` → "Zenith_CallbackRegistry implementation"

### Z2-R-CORE-106 Callback registry bounds

- **Statement:** A callback registry holds at most its desc's entry count and asserts nested dispatch on the same thread.
- **Covers:** Z2-I-CORE-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_CallbackRegistry_Cap_ReturnsCapacityExceeded`, `Test_CallbackRegistry_Reentrancy_NestedDispatch_Asserts` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Threading.md` → "Zenith_CallbackRegistry implementation"

### Z2-R-CORE-107 Operation status and cancellation

- **Statement:** An operation's status and progress are published with release ordering so progress reaches 1.0 only with SUCCEEDED, cancellation requests never block, a destroyed incomplete handle requests cancellation, and a success before cancellation is observed reports SUCCEEDED.
- **Covers:** Z2-I-CORE-02, Z2-I-JOBS-07 (the Z2-I-JOBS items are JOBS's; CORE owns their ZenithCore half)
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_OperationHandle_DestroyIncomplete_RequestsCancel`, `Test_OperationHandle_ProgressReachesOneOnlyOnSuccess` (also `Sanitizers -Preset linux-tsan` gate step), `Test_OperationHandle_CancelRaceWithSuccess_ReportsSucceeded` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Threading.md` → "Zenith_OperationHandle implementation"

### Z2-R-CORE-108 Operation cancel stress

- **Statement:** 1,000 issue-then-cancel cycles at randomised stages leave no live slot and no allocation.
- **Covers:** Z2-I-CORE-02, Z2-I-JOBS-07 (the Z2-I-JOBS items are JOBS's; CORE owns their ZenithCore half)
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_OperationTable_IssueCancelStress_NoLeak` (`z2-stress`, `Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Threading.md` → "Zenith_OperationHandle implementation"

### Z2-R-CORE-109 Configuration value types and schema

- **Statement:** ZenithCore declares the configuration value types, layers, key descriptors and provenance, and a library at any layer registers its keys in a `Zenith_ConfigSchema` that rejects invalid names, duplicates and registration after `Freeze`.
- **Covers:** Z2-I-CORE-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_ConfigStore_SchemaFrozen_RejectsRegister` (`Test` gate step); `Libgraph` (ZenithJobs registers keys without depending on ZenithPlatform)
- **Design:** `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Configuration store"

### Z2-R-CORE-110 Layer precedence and provenance

- **Statement:** The store resolves each key to its highest set layer of the six and records the layer, source and line of every value.
- **Covers:** Z2-I-CORE-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_ConfigStore_LayerPrecedence` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Resolution algorithm"

### Z2-R-CORE-111 Rejected configuration values

- **Statement:** A value set in a disallowed layer, of the wrong type, out of range or with an unknown enum name is a diagnostic and a logged warning naming key, source and line, and the next-lower layer stays in effect.
- **Covers:** Z2-I-CORE-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_ConfigStore_RejectedValue_KeepsLowerLayer` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Resolution algorithm"

### Z2-R-CORE-112 Unknown configuration keys

- **Statement:** An unknown key in any layer is a warning and changes nothing.
- **Covers:** Z2-I-CORE-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_ConfigStore_UnknownKey_Warns` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Resolution algorithm"

### Z2-R-CORE-113 Required configuration keys

- **Statement:** `ValidateRequired` fails naming the first required key no layer supplied.
- **Covers:** Z2-I-CORE-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_ConfigStore_RequiredMissing_Fails` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Configuration store"

### Z2-R-CORE-114 User values at the frame prologue

- **Statement:** User-layer values are staged and committed at the frame prologue on the main thread, and each committed change dispatches the change registry once.
- **Covers:** Z2-I-CORE-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_ConfigStore_StagedUserValue_CommitsAtPrologue` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Resolution algorithm"

### Z2-R-CORE-115 Configuration dump

- **Statement:** The dump lists every key with value, type, layer, source, line and flags in key byte order, and a single layer written with `WriteLayer` re-applies to the same values.
- **Covers:** Z2-I-CORE-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_ConfigStore_Dump_SortedAndComplete`, `Test_ConfigStore_WriteLayer_RoundTrips` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Resolution algorithm"

### Z2-R-CORE-116 Simulation-affecting keys

- **Statement:** The store serializes every simulation-affecting key for the replay header.
- **Covers:** Z2-I-CORE-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_ConfigStore_SimulationAffecting_Serialized` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Configuration store"

### Z2-R-CORE-117 Configuration file format

- **Statement:** A configuration layer file is one flat JSON object of dotted keys whose values are booleans, numbers or strings, and nested values are diagnostics.
- **Covers:** Z2-I-CORE-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** D-SEL-JSON (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_ConfigStore_RejectedValue_KeepsLowerLayer` (nested-object case) (`Test` gate step); `Zenith_Fuzz_ConfigLayer` (`Sanitizers -Preset win-asan` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Configuration file format"

### Z2-R-CORE-118 JSON writer byte format

- **Statement:** `Zenith_JsonWriter` produces exactly the byte format of its table in pretty and compact styles on both D-SEL-JSON paths.
- **Covers:** Z2-I-CORE-08, Z2-I-CORE-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_JsonWriter_ByteFormat_Golden` (`Test` gate step, every preset)
- **Design:** `Zenith2/Docs/Design/CORE/Text.md` → "JSON writer"; `Zenith2/Docs/Design/CORE/Text.md` → "Byte format"

### Z2-R-CORE-119 JSON writer errors

- **Statement:** The writer's first error is sticky and reported by `GetStatus` and `Finish`, covering structure misuse, non-finite numbers, invalid UTF-8, depth and allocation failure.
- **Covers:** Z2-I-CORE-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_JsonWriter_StructureErrors`, `Test_JsonWriter_NonFinite_Rejected`, `Test_JsonWriter_OutOfMemory_Sticky` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Text.md` → "JSON writer"

### Z2-R-CORE-120 JSON reader limits

- **Statement:** The reader enforces every JSON parser limit of the Security architecture, accepting input at each limit and rejecting it one above with the limit's error code and position.
- **Covers:** Z2-I-CORE-16, Z2-I-CORE-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** D-SEL-JSON (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_JsonReader_LimitsAtAndAboveEachLimit` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Text.md` → "JSON reader"

### Z2-R-CORE-121 JSON reader conformance

- **Statement:** The reader accepts and rejects the JSONTestSuite corpus as recorded, rejects a leading BOM and `\u0000`, and never asserts on any input.
- **Covers:** Z2-I-CORE-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** D-SEL-JSON (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_JsonReader_Conformance`, `Test_JsonReader_BomAndNul_Rejected` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Text.md` → "JSON reader"

### Z2-R-CORE-122 JSON duplicate keys

- **Statement:** Duplicate object keys are detected by sorting, never by hashing untrusted keys, and reported at the second occurrence.
- **Covers:** Z2-I-CORE-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** D-SEL-JSON (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_JsonReader_DuplicateKey_Rejected` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Text.md` → "JSON reader"

### Z2-R-CORE-123 JSON positions

- **Statement:** Every parsed node carries its 1-based line and byte column from Zenith's position pass, independent of the backend.
- **Covers:** Z2-I-CORE-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** D-SEL-JSON (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_JsonReader_Positions_LineColumn` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Text.md` → "JSON reader"

### Z2-R-CORE-124 JSON round trip

- **Statement:** Parsing then writing a canonical document reproduces it byte for byte, preserving member order and raw number text.
- **Covers:** Z2-I-CORE-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048, D-SEL-JSON (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_JsonReader_RoundTrip_PreservesOrderAndRawNumbers` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Text.md` → "JSON reader"

### Z2-R-CORE-125 JSON fuzzing

- **Statement:** The JSON reader is fuzzed under `win-asan` with no crash, no leak and no accepted input the writer cannot re-emit.
- **Covers:** Z2-I-CORE-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** `Zenith_Fuzz_JsonDocument` 60-second smoke (`Sanitizers -Preset win-asan` gate step); full duration at each stage acceptance
- **Design:** `Zenith2/Docs/Design/CORE/Text.md` → "JSON reader"

### Z2-R-CORE-126 D-SEL-JSON selection

- **Statement:** The D-SEL-JSON harness measures yyjson and simdjson through the backend-neutral parse, and the ruling ticket records the winner and adds its one backend source and manifest entry.
- **Covers:** Z2-I-CORE-16, Z2-I-CORE-08
- **Stage:** S1
- **Classification:** research
- **Decisions:** Z2-D-084, D-SEL-JSON (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Zenith2/Benchmarks/JsonSelection/` smoke in the `Test` gate step; the report under `Zenith2/Docs/Research/JsonSelection/`; the ruling entry in `Zenith2/Docs/DecisionLog.md`
- **Design:** `Zenith2/Docs/Design/CORE.md` → "Decisions"; `Zenith2/Docs/Design/CORE/Text.md` → "JSON reader"

### Z2-R-CORE-127 D-SEL-HEAP selection

- **Statement:** The D-SEL-HEAP harness replays mixed-size, cross-thread and spawn/destroy churn allocation traces on both backings, and the ruling ticket records the backing against the acceptance rule.
- **Covers:** Z2-I-CORE-03
- **Stage:** S1
- **Classification:** research
- **Decisions:** Z2-D-084, D-SEL-HEAP (open, Stage 1)
- **Budgets:** Z2-B-CORE-001, Z2-B-CORE-004
- **Acceptance evidence:** `Zenith2/Benchmarks/HeapSelection/` smoke in the `Test` gate step; the report under `Zenith2/Docs/Research/HeapSelection/`; the ruling entry in `Zenith2/Docs/DecisionLog.md`
- **Design:** `Zenith2/Docs/Design/CORE.md` → "Decisions"; `Zenith2/Docs/Design/CORE/Memory.md` → "Backing selection"

### Z2-R-CORE-128 Governor creation checks

- **Statement:** `Zenith_MemoryGovernor::Create` rejects zero budgets or thresholds, a warning threshold not below the critical one, and non-pending subsystem budgets above the process total with `MEMORY_ERROR_BAD_BUDGETS`.
- **Covers:** Z2-I-CORE-18
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-CORE-001, Z2-B-CORE-002, Z2-B-CORE-003
- **Acceptance evidence:** `Test_MemoryGovernor_BadBudgetsRejected` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Additions to the governor API"; `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Governor tests"

### Z2-R-CORE-129 Generated budget table

- **Statement:** The budget-table generator reads Budgets' Row index and Architecture's governor table "Budgets" at configure time, takes each budget's rows from Architecture's table alone, tokenises the Allocation-sums member cells with `max(…)` groups first, applies the value grammar and unit conversions, fills pending rows with the platform total and a pending flag, and stops configure on a malformed row or cell.
- **Covers:** Z2-I-CORE-18
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-CORE-001, Z2-B-CORE-002, Z2-B-CORE-003
- **Acceptance evidence:** `Test_BudgetTable_Generator_Fixtures` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Generated budget table"

### Z2-R-CORE-130 Budget key defaults

- **Statement:** Each `memory.*` key's default is the sum, for the build platform, of the rows Architecture's table "Budgets" lists for it, and `Zenith_ReadMemoryBudgets` fills every field of `Zenith_MemoryBudgets` from the resolved keys.
- **Covers:** Z2-I-CORE-18
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** `Test_BudgetTable_DefaultsMatchRowIndex` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Generated budget table"

### Z2-R-CORE-131 Rendering memory budget

- **Statement:** The governor's rendering budget is the sum of the eleven RHI, GEOM, RPROD, ADVGFX and VFX rows that Architecture's table "Budgets" lists for `MEMORY_SUBSYSTEM_RENDERING`, allocated by their areas' Stage 1 per-system budget tickets, and is pending (never shedding by itself) until then.
- **Covers:** Z2-I-CORE-19
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-RHI-001, Z2-B-RHI-002, Z2-B-RHI-006, Z2-B-RHI-010, Z2-B-RHI-011, Z2-B-GEOM-001, Z2-B-GEOM-002, Z2-B-RPROD-001, Z2-B-RPROD-002, Z2-B-ADVGFX-001, Z2-B-VFX-001
- **Acceptance evidence:** the rows adopted in `Zenith2/Docs/Budgets.md` → "Row index" at the Stage 1 acceptance; `Test_BudgetTable_DefaultsMatchRowIndex`
- **Design:** `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Generated budget table"

### Z2-R-CORE-132 Streaming memory budget

- **Statement:** The governor's streaming budget is Z2-B-STREAM-001, allocated by the Stage 1 per-system budget ticket for STREAM, and pending until then.
- **Covers:** Z2-I-CORE-20
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-STREAM-001
- **Acceptance evidence:** the row adopted at the Stage 1 acceptance; `Test_BudgetTable_DefaultsMatchRowIndex`
- **Design:** `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Generated budget table"

### Z2-R-CORE-133 Audio memory budget

- **Statement:** The governor's audio budget is Z2-B-AUDIO-001, allocated by the Stage 1 per-system budget ticket for AUDIO, and pending until then.
- **Covers:** Z2-I-CORE-21
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-AUDIO-001
- **Acceptance evidence:** the row adopted at the Stage 1 acceptance; `Test_BudgetTable_DefaultsMatchRowIndex`
- **Design:** `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Generated budget table"

### Z2-R-CORE-134 Script memory budget

- **Statement:** The governor's script budget is Z2-B-SCRIPT-009 plus -011 plus -013, allocated by the Stage 1 per-system budget ticket for SCRIPT, and pending until then.
- **Covers:** Z2-I-CORE-22
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-SCRIPT-009, Z2-B-SCRIPT-011, Z2-B-SCRIPT-013
- **Acceptance evidence:** the rows adopted at the Stage 1 acceptance; `Test_BudgetTable_DefaultsMatchRowIndex`
- **Design:** `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Generated budget table"

### Z2-R-CORE-135 Editor cache memory budget

- **Statement:** The governor's editor-cache budget is Z2-B-EDIT-001 in editor builds and 0 in a Player, allocated by the Stage 1 per-system budget ticket for EDIT.
- **Covers:** Z2-I-CORE-23
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-EDIT-001
- **Acceptance evidence:** the row adopted at the Stage 1 acceptance; `Test_BudgetTable_DefaultsMatchRowIndex`
- **Design:** `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Generated budget table"

### Z2-R-CORE-136 Governor tuning keys

- **Statement:** The four governor tuning keys are required configuration keys supplied by ZenithCore's platform configuration fragments, whose values the Stage 1 per-system budget ticket for CORE writes.
- **Covers:** Z2-I-CORE-18
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-CORE-009, Z2-B-CORE-010
- **Acceptance evidence:** `Test_ConfigStore_RequiredMissing_Fails`; the committed fragments' values cited by the CORE budget ruling
- **Design:** `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Tuning keys"

### Z2-R-CORE-137 CORE memory rows allocated

- **Statement:** The Stage 1 per-system budget ticket for CORE allocates Z2-B-CORE-004 to -011, the two pending row needs and the tuning keys, with its `allocation.json`, after `Check-Allocation.ps1` passes.
- **Covers:** Z2-I-CORE-18
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-012, Z2-D-084
- **Budgets:** Z2-B-CORE-004, Z2-B-CORE-005, Z2-B-CORE-006, Z2-B-CORE-007, Z2-B-CORE-008, Z2-B-CORE-009, Z2-B-CORE-010, Z2-B-CORE-011
- **Acceptance evidence:** `Budgets_CheckAllocation_Fixtures` and `Budgets_CoreArenaPoolProbe` smoke (`Test` gate step); `Zenith2/Docs/Research/Budgets/Stage1/CORE/allocation.json`; the allocating entry in `Zenith2/Docs/DecisionLog.md`
- **Design:** `Zenith2/Docs/Design/CORE.md` → "Budget rows"; `Zenith2/Docs/Design/CORE.md` → "Stage plan"

### Z2-R-CORE-138 Pressure from OS signals

- **Statement:** The governor maps the Windows low-memory notification to WARNING, an OS report from the platform layer to its level held for the Android hold time, and the OOM relay to CRITICAL.
- **Covers:** Z2-I-CORE-24
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_MemoryGovernor_OSSignalMapsToLevel`, `Test_MemoryGovernor_OOMRelay_ForcesCritical` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Level evaluation"; `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "OS memory queries"

### Z2-R-CORE-139 Pressure from budgets

- **Statement:** The governor raises WARNING and CRITICAL from process and device-local usage against the threshold rows and from each non-pending subsystem against its budget and critical factor, in the same update.
- **Covers:** Z2-I-CORE-25
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-CORE-009, Z2-B-CORE-010
- **Acceptance evidence:** `Test_MemoryGovernor_ProcessThresholds_RaiseLevel`, `Test_MemoryGovernor_SubsystemOverBudget_RaisesWarning` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Level evaluation"

### Z2-R-CORE-140 Pressure hysteresis

- **Statement:** Each pressure source rises at once and falls one step per satisfied margin-and-dwell; the combined level is the maximum of the source levels with no dwell of its own; clearing a forced level removes that source at once; the unattributed residual is reported and raises no level; every change of the combined level is notified.
- **Covers:** Z2-I-CORE-24, Z2-I-CORE-25
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_MemoryGovernor_HysteresisReturnsToNormal`, `Test_MemoryGovernor_ForcedLevel_AndClear`, `Test_MemoryGovernor_UnattributedResidual_Computed` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Level evaluation"; `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Accounting"

### Z2-R-CORE-141 Consumer notification

- **Statement:** Consumers are notified of every level change in registration order on the main thread, a late consumer receives the current level, and registration from inside a notification asserts.
- **Covers:** Z2-I-CORE-18
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_MemoryGovernor_NotifiesInRegistrationOrder`, `Test_MemoryGovernor_LateConsumer_ReceivesCurrentLevel`, `Test_MemoryGovernor_RegisterInsideCallback_Asserts` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Consumer registration"

### Z2-R-CORE-142 Accounting and residual

- **Statement:** The governor attributes resident bytes per subsystem from its consumers and computes the unattributed residual of the general heap against Z2-B-CORE-004, with arenas attributed against Z2-B-CORE-005.
- **Covers:** Z2-I-CORE-18
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-CORE-004, Z2-B-CORE-005
- **Acceptance evidence:** `Test_MemoryGovernor_UnattributedResidual_Computed` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Accounting"

### Z2-R-CORE-143 Governor counters

- **Statement:** The governor publishes every counter of its table each update, feeding the on-screen memory panel.
- **Covers:** Z2-I-CORE-18
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_MemoryGovernor_CountersPublished` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Counters"

### Z2-R-CORE-144 Shedding touches no simulation state

- **Statement:** Forcing CRITICAL changes no per-tick world-state hash over 600 ticks.
- **Covers:** Z2-I-CORE-26
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_MemoryGovernor_ShedsNoSimulationState` (`Zenith2/Tests/Memory/`, `Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Governor tests"

### Z2-R-CORE-145 Every subsystem sheds

- **Statement:** Each subsystem's consumer applies its row of the shedding table within one frame of a level change and reverts one frame after the level falls, proven by its counters.
- **Covers:** Z2-I-CORE-26
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_MemoryGovernor_ForcedWarningShedsEachConsumer`, `Test_MemoryGovernor_ForcedCriticalShedsEachConsumer` (`Zenith2/Tests/Memory/`, every subsystem present at Stage 4)
- **Design:** `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Governor tests"; `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Counters"

### Z2-R-CORE-146 Streaming double-residency peak

- **Statement:** Streaming reserves its double-residency peak from the governor before allocating and defers to a following frame on refusal.
- **Covers:** Z2-I-CORE-27
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-CORE-006
- **Acceptance evidence:** `Test_MemoryGovernor_TransitionPeaks_EachKind` (streaming case, `Zenith2/Tests/Memory/`)
- **Design:** `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Reservation algorithm"

### Z2-R-CORE-147 World-transition peak

- **Statement:** A world transition reserves its peak before loading the new world and fails its request on refusal.
- **Covers:** Z2-I-CORE-28
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-CORE-007
- **Acceptance evidence:** `Test_MemoryGovernor_TransitionPeaks_EachKind` (world-transition case)
- **Design:** `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Reservation algorithm"

### Z2-R-CORE-148 Texture-swap peak

- **Statement:** A streamed texture's reallocate-and-swap reserves its peak and defers on refusal.
- **Covers:** Z2-I-CORE-29
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-012, Z2-D-058
- **Budgets:** Z2-B-CORE-008
- **Acceptance evidence:** `Test_MemoryGovernor_TransitionPeaks_EachKind` (texture-swap case)
- **Design:** `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Reservation algorithm"

### Z2-R-CORE-149 Reservations never exceed peaks

- **Statement:** Concurrent reservations never sum above their peak row, refusals report the available bytes, and `Shrink` and destruction release exactly.
- **Covers:** Z2-I-CORE-18, Z2-I-CORE-27, Z2-I-CORE-28, Z2-I-CORE-29
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-CORE-006, Z2-B-CORE-007, Z2-B-CORE-008
- **Acceptance evidence:** `Test_MemoryGovernor_TransitionPeakRefusedAtCap`, `Test_MemoryGovernor_ConcurrentReservations_NeverExceedPeak` (`Test` gate step and `Sanitizers -Preset linux-tsan` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Reservation algorithm"

### Z2-R-CORE-150 Reservations in pressure and shutdown

- **Statement:** Outstanding reservations count toward process pressure, and a reservation live at governor destruction is reported and fails `ZENITH_CHECK`.
- **Covers:** Z2-I-CORE-18, Z2-I-CORE-27, Z2-I-CORE-28, Z2-I-CORE-29
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_MemoryGovernor_LiveReservationAtShutdown_Checks` (`z2-death-check`); `Test_MemoryGovernor_ProcessThresholds_RaiseLevel` (reservation case)
- **Design:** `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Reservation algorithm"

### Z2-R-CORE-151 Forced-level unit tests

- **Statement:** Unit tests force each pressure level through fake consumers and assert notification, hysteresis and clearing.
- **Covers:** Z2-I-CORE-30
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_MemoryGovernor_ForcedLevel_AndClear`, `Test_MemoryGovernor_NotifiesInRegistrationOrder` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Governor tests"

### Z2-R-CORE-152 Forced-level tests grow with subsystems

- **Statement:** Each subsystem's first shedding ticket adds its case to the two cross-system forced-level tests in the same diff.
- **Covers:** Z2-I-CORE-30
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_MemoryGovernor_ForcedWarningShedsEachConsumer` and `_ForcedCriticalShedsEachConsumer` listing every consumer present at the Stage 4 acceptance
- **Design:** `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Governor tests"

### Z2-R-CORE-153 S24 memory-pressure run

- **Statement:** A device run on the S24 raises OS memory pressure during W-OpenWorld play and records that every consumer shed and the process stayed within Z2-B-CORE-001.
- **Covers:** Z2-I-CORE-24, Z2-I-CORE-26
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-010
- **Budgets:** Z2-B-CORE-001
- **Acceptance evidence:** the device run record of the Stage 4 acceptance (`needs-human`, `needs-gpu`, `gpu-reason:requires-graphics-test`)
- **Design:** `Zenith2/Docs/Design/CORE.md` → "Stage plan"; `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Governor tests"

### Z2-R-CORE-154 Factory fault points

- **Statement:** Every CORE factory lists its fallible steps in order, declares a library-unique `uFAULT_POINT_*` constant for each non-allocation step (including `Zenith_Thread::Create`'s native creation and the governor's Windows low-memory handle) with a `Zenith_FaultInjector*` in its desc or parameters, and has one `Test_<Type>_<Factory>_FailsAt<Step>_Unwinds` test per step asserting its error, no leak, no running thread and no registration left.
- **Covers:** Z2-I-CORE-17
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** the tests of the table "Factory fault points", from `Test_StringTable_Create_RejectsInvalidDesc` and `Test_StringTable_Create_FailsAtObject_Unwinds` to `Test_Thread_Create_FailsAtStartBlock_Unwinds` and `Test_Thread_Create_FailsAtNativeCreate_Unwinds`, including `Test_MemoryGovernor_Create_LowMemoryHandleUnavailable_Warns` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Memory.md` → "Factory fault points"; `Zenith2/Docs/Design/CORE/Threading.md` → "Threads"

### Z2-R-CORE-155 Log router holds records until its sinks exist

- **Statement:** `Zenith_LogRouter::Create` allocates and starts no thread; every record emitted from startup step 1b is held in the ring, framed by length and dropped whole from the head on overflow; the drain thread starts only through `StartDrainThread` at step 7 after the sinks are added, never dequeues while no sink is registered, delivers every held record to the registered sinks in sequence order, and at destruction exits on its stop flag after a final pass that counts records it cannot output as dropped.
- **Covers:** Z2-I-CORE-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_Log_Router_NoSinks_RecordsStayInRing`, `Test_Log_Router_DrainThreadWithoutSinks_KeepsRecords`, `Test_LogRouter_StartDrainThread_FailsAtThread_Unwinds` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Diagnostics.md` → "The log-record hook"; `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Log router"

### Z2-R-CORE-156 Log ring sink

- **Statement:** `Zenith_LogRingSink::CopyRecentLines` returns the most recent lines oldest first and reports an output allocation failure.
- **Covers:** Z2-I-CORE-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_LogRingSink_CopyRecentLines_Order` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Log router"

### Z2-R-CORE-157 Log console sink format

- **Statement:** `Zenith_LogConsoleSink` writes exactly the documented line for every level and field kind, escaping quotes and backslashes in text fields.
- **Covers:** Z2-I-CORE-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_LogConsoleSink_LineFormat` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Log router"

### Z2-R-CORE-158 JSON document mutation

- **Statement:** The `Zenith_JsonDocument` mutation API inserts, removes and changes members and elements at any position, rejects out-of-range positions and allocation failures with the document unchanged, and writes the result byte-stably with untouched raw numbers preserved.
- **Covers:** Z2-I-CORE-08, Z2-I-CORE-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048, D-SEL-JSON (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_JsonDocument_Mutation_InsertRemoveRoundTrips` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Text.md` → "JSON reader"

### Z2-R-CORE-159 Heap backing state

- **Statement:** `Zenith_HeapBackingState` is declared in the public `Zenith_Heap.h` with its constructor in `Internal/`, has the same members in every configuration with unused members zero, and its constructor allocates nothing; the heap constructor's one allocation (the emergency reserve) is tolerated on failure ([Z2-N-CONTRACT-199]), so heap construction cannot fail; the Windows over-aligned side table is allocated by the first over-aligned allocation.
- **Covers:** Z2-I-CORE-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `HeaderSelfContained` for `Zenith_Heap.h`; `CompileCoverage` over every preset; `ForbiddenCheck` F12; `Test_Heap_OverAligned_FreeRoutesToAlignedFree` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Memory.md` → "Over-aligned side table"

### Z2-R-CORE-160 Sanitizer kind query

- **Statement:** `Zenith_GetSanitizerKind()` returns, at compile time, the preset's sanitizer (`SANITIZER_KIND_NONE`, `ASAN`, `TSAN_UBSAN` or `HWASAN`) from its `ZENITH_SANITIZER_*` defines, so a test selects itself for one sanitizer preset without a preprocessor test switch.
- **Covers:** Z2-I-CORE-15, Z2-I-CORE-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** `Test_BuildInfo_SanitizerKind_MatchesPreset` (`Test` gate step in every preset, including `win-asan`, `linux-tsan` and the S24 HWASan build)
- **Design:** `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Build information"

### Z2-R-CORE-161 Log ring bootstrap size and resize

- **Statement:** The log router is created at startup step 1b with the build-time ring size `uLOG_RING_BYTES_DEFAULT` (1 MiB) and reads no configuration key; `ResizeRing` at step 7 outputs every buffered record to the registered sinks before installing a ring of `log.ringBytes`, and on allocation failure keeps the bootstrap ring and its records.
- **Covers:** Z2-I-CORE-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_Log_Router_ResizeRing_OutputsBufferedThenReplaces`, `Test_Log_Router_Stats_DefaultsAndCounts` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Diagnostics.md` → "The log-record hook"; `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Log router"; `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Core configuration keys"

### Z2-R-CORE-162 Log drain thread start failure is fatal

- **Statement:** A `StartDrainThread` failure at startup step 7 is fatal: the composition root writes a diagnostic line and raises `FATAL_CODE_LOGGING_UNAVAILABLE`, and a fatal record raised while no drain thread runs outputs every buffered record before itself.
- **Covers:** Z2-I-CORE-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_Log_Router_FatalWithoutDrainThread_OutputsBufferedFirst` (`z2-death-check`); `Test_LogRouter_StartDrainThread_FailsAtThread_Unwinds` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Log router"; `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Assertions and fatal errors"; `Zenith2/Docs/Design/CORE.md` → "Failure handling"

### Z2-R-CORE-163 Log router statistics and limits

- **Statement:** `Zenith_LogRouter::GetStats` reports routed and filtered records, the ring's capacity and its queue counters; the initial global level is the desc's; the category override table holds at most `m_uMaxCategoryOverrides` entries; a record larger than `uLOG_RECORD_MAX_SERIALISED_BYTES` drops trailing fields and is marked truncated.
- **Covers:** Z2-I-CORE-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Log_Router_Stats_DefaultsAndCounts`, `Test_Log_Router_CategoryOverrideFilters`, `Test_Log_Router_OversizedRecord_DropsTrailingFields` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Log router"

### Z2-R-CORE-164 Configuration layer entries and clearing

- **Statement:** `GetLayerEntry` returns each layer's value and provenance for a key or null when unset, and `ClearLayer` unsets one layer, re-resolves every key to its next-lower set layer, increments the revision and dispatches no callback.
- **Covers:** Z2-I-CORE-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_ConfigStore_GetLayerEntry_ReportsEachLayer`, `Test_ConfigStore_ClearLayer_ReResolves` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Configuration store"; `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Resolution algorithm"

### Z2-R-CORE-165 Simulation-affecting list reader

- **Statement:** `ApplySimulationAffecting` reads exactly the byte layout `WriteSimulationAffecting` writes, validates the whole list under its parser limits before applying anything, and applies each pair to the command-line layer with the given source, skipping the layer-mask test so a recorded value always applies exactly.
- **Covers:** Z2-I-CORE-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_ConfigStore_ApplySimulationAffecting_RoundTrips`, `Test_ConfigStore_ApplySimulationAffecting_RejectsMalformed` (`Test` gate step); `Zenith_Fuzz_ConfigLayer` (`Sanitizers -Preset win-asan` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Simulation-affecting list"

### Z2-R-CORE-166 Configuration parse error position

- **Statement:** A whole-file parse error of a configuration layer, including a root value that is not an object, is returned as `CONFIG_ERROR_PARSE`, recorded as a diagnostic with its source and line, and logged with the line and column.
- **Covers:** Z2-I-CORE-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_ConfigStore_ParseError_LoggedWithPosition` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Resolution algorithm"

### Z2-R-CORE-167 SHA-256 conforms to FIPS 180-4 outside D-SEL-HASH

- **Statement:** `Zenith_Sha256Digest` and `Zenith_Sha256Stream` compute SHA-256 exactly per FIPS 180-4, writing the digest big-endian (H0 first) into `Zenith_Sha256Value` without allocating; a stream fed any split of a message equals the one-shot digest of it; the hexadecimal forms are 64 lowercase characters and parsing rejects anything else; and no D-SEL-HASH ruling changes the algorithm, because SCENE's derived authoring IDs and document journals and PLAT's patch mount store or check its digests.
- **Covers:** Z2-I-SCENE-01, Z2-I-SEC-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_Sha256_NistVectors`, `Test_Sha256_PaddingBoundaries`, `Test_Sha256_StreamEqualsOneShot`, `Test_Sha256_HexRoundTrip`, `Test_Sha256_NullWithSize_Asserts` (`Test` gate step, every preset that runs tests); `Test_Integration_Sha256_StableDigests` in `Zenith2/Tests/Core/` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_Sha256"; `Zenith2/Docs/Design/CORE/Maths.md` → "SHA-256 outside D-SEL-HASH"; `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_Sha256 declarations"; `Zenith2/Docs/Design/CORE/Maths.md` → "SHA-256 algorithm steps"; `Zenith2/Docs/Design/CORE/Maths.md` → "Maths tests"

### Z2-R-CORE-168 JSON document tree and pointer resolution

- **Statement:** `Zenith_DocumentTree` is a 16-byte non-owning cursor over one `Zenith_JsonDocument` subtree, the cursor every `Zenith_PfnMigrationStep` receives; `FindPath` resolves a JSON Pointer relative to the cursor's root by grammar G1 to G4 without allocating, changing the document or asserting on the pointer's bytes, returns `JSON_ACCESS_ERROR_INVALID_POINTER` for a malformed pointer before it visits any node, and returns `JSON_ACCESS_ERROR_MISSING_MEMBER`, `JSON_ACCESS_ERROR_INDEX_OUT_OF_RANGE` or `JSON_ACCESS_ERROR_WRONG_KIND` for a well-formed pointer the tree does not hold.
- **Covers:** Z2-I-CORE-08, Z2-I-ECS-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_DocumentTree_FindPath_MembersAndEscapes`, `Test_DocumentTree_FindPath_ArrayIndices`, `Test_DocumentTree_FindPath_InvalidPointer`, `Test_DocumentTree_FindPath_ScalarAndUnchanged`, `Test_DocumentTree_StepEditsThroughCursor`, `Test_DocumentTree_FindPath_MatchesReferenceResolver`, `Test_DocumentTree_NullRoot_Asserts` (`Test` gate step); `Test_Integration_Json_MigrationStepThroughDocumentTree` in `Zenith2/Tests/Core/` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Text.md` → "JSON document tree"; `Zenith2/Docs/Design/CORE/Text.md` → "Zenith_DocumentTree declarations"; `Zenith2/Docs/Design/CORE/Text.md` → "Pointer resolution steps"; `Zenith2/Docs/Design/CORE/Text.md` → "Text tests"

### Z2-R-CORE-169 Subsystem reservations bounded by the subsystem budget

- **Statement:** `Zenith_MemoryGovernor::ReserveSubsystemBytes` (`ANY_THREAD`, lock-free, allocating nothing) grants a move-only `Zenith_SubsystemReservation` exactly when the subsystem's resident bytes at the last `Update`, plus its outstanding subsystem reservations, plus the request stay within the subsystem's budget (the process total for a pending subsystem), and otherwise returns `MEMORY_ERROR_SUBSYSTEM_REFUSED` with `m_eSubsystem`, the requested and the available bytes while reserving nothing; concurrent grants never sum above the budget; `Shrink`, destruction and move assignment release exactly; transition reservations and subsystem reservations never count against each other's bound; outstanding subsystem reservations count toward their subsystem's pressure source and the process source; and a subsystem reservation live at governor destruction is logged and fails `ZENITH_CHECK`.
- **Covers:** Z2-I-CORE-18, Z2-I-CORE-25
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_MemoryGovernor_SubsystemReservationRefusedAtBudget`, `Test_MemoryGovernor_SubsystemReservation_CountsMeasuredBytes`, `Test_MemoryGovernor_SubsystemReservation_PendingUsesProcessTotal`, `Test_MemoryGovernor_SubsystemReservation_RaisesPressureSources`, `Test_MemoryGovernor_SubsystemReservation_IndependentOfTransitionPeaks`, `Test_MemoryGovernor_SubsystemReservation_MoveTransfersBytes`, `Test_MemoryGovernor_SubsystemReservation_ShrinkAboveBytes_Asserts` (`z2-death-assert`), `Test_MemoryGovernor_LiveSubsystemReservationAtShutdown_Checks` (`z2-death-check`) (`Test` gate step); `Test_MemoryGovernor_ConcurrentSubsystemReservations_NeverExceedBudget` (`Test` gate step and `Sanitizers -Preset linux-tsan` gate step); `Test_MemoryGovernor_SubsystemReservation_BootedRuntime` in `Zenith2/Tests/Memory/` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Subsystem reservation algorithm"; `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Grant steps"; `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Interaction with transition reservations and budgets"; `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Governor tests"

### Z2-R-CORE-170 Android test entry for the shared test main

- **Statement:** `ZenithTestSupport` runs every test executable's body through `Zenith_RunTestMain(iArgc, ppszArgv, eRoute)`, called by the ordinary `main` with `TEST_OUTPUT_ROUTE_STDOUT` and by an exported `extern "C"` `SDL_main` with `TEST_OUTPUT_ROUTE_TAGGED_DIAGNOSTIC`, so an `android-arm64` test executable linked into a test APK runs unchanged inside the Player activity; the tagged route writes each complete doctest output line through `Zenith_WriteTaggedDiagnosticLine` with tag `Zenith2Test` (logcat on Android, `Zenith2Test: <line>` on stderr elsewhere); `--zenith-test-output=stdout|tagged` overrides the route and is removed before doctest parses the command line, and any other value exits 2 before doctest runs; the root-heap, suite-exclusion and leak-exit-3 behaviour of the test main is unchanged on both entries.
- **Covers:** Z2-I-CORE-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-042, Z2-D-010
- **Budgets:** none
- **Acceptance evidence:** `Test_TestOutputLineBuffer_SplitsLines`, `Test_TestMain_RouteOption_Parsed`, `Test_TaggedDiagnosticLine_Composes`, `Test_TaggedDiagnosticLine_LongTag_Asserts` (`z2-death-assert`) (`Test` gate step); the CTest entries `ZenithCore_Tests.TaggedOutputRoute` and `ZenithCore_Tests.UnknownOutputRoute` (`Test` gate step); `ZenithTestSupport` with `Zenith_TestEntry_SDLMain.cpp` in the `Build -Preset android-arm64` gate step
- **Design:** `Zenith2/Docs/Design/CORE/Memory.md` → "Test main and the Android test entry"; `Zenith2/Docs/Design/CORE/Memory.md` → "Test main declarations"; `Zenith2/Docs/Design/CORE/Memory.md` → "Test main steps"; `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Diagnostic output and backtraces"

### Z2-R-CORE-171 Deterministic transcendental functions

- **Statement:** `Zenith_Sin`, `Zenith_Cos`, `Zenith_SinCos`, `Zenith_Tan`, `Zenith_Asin`, `Zenith_Acos`, `Zenith_Atan`, `Zenith_Atan2`, `Zenith_Exp`, `Zenith_Exp2`, `Zenith_Ln`, `Zenith_Log2`, `Zenith_Log10`, `Zenith_Pow`, `Zenith_Cbrt` and `Zenith_Sqrt`, each in binary64 and binary32, compute exactly the specified steps with the committed constants, using IEEE operations alone with no contraction, no C runtime call, no intrinsic other than the hardware square root and no run-time CPU dispatch, so that identical argument bits give identical result bits on every supported CPU, compiler and platform, whatever the CRT FMA3 flag and the FTZ/DAZ state; subnormal arguments are read and subnormal results returned as signed zeros, out-of-domain and NaN arguments return the canonical quiet NaN, `Zenith_SinCos` writes exactly `Zenith_Sin` and `Zenith_Cos`, and each function stays within its stated error bound over its stated domain; CORE's own maths types call these functions wherever they need a transcendental.
- **Covers:** Z2-I-CORE-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-047
- **Budgets:** none
- **Acceptance evidence:** `Test_Transcendental_ApiSurface_Overloads`, `Test_Transcendental_GoldenVectors_Trigonometric`, `Test_Transcendental_GoldenVectors_ExponentialLogarithm`, `Test_Transcendental_SpecialValues_Trigonometric`, `Test_Transcendental_SpecialValues_ExponentialLogarithm`, `Test_Transcendental_SinCosMatchesSinAndCos`, `Test_Transcendental_SymmetryBitExact`, `Test_Transcendental_SubnormalArgumentsReadAsZero`, `Test_Transcendental_SubnormalResultsFlushToZero` (`Test` gate step, every preset that runs tests, the golden vectors identical on each); `Test_Transcendental_WithinBoundOfHostLibrary_Trigonometric`, `Test_Transcendental_WithinBoundOfHostLibrary_ExponentialLogarithm` (informative, `Test` gate step); the CTest entry `ZenithCore.TranscendentalNoCrtCall` (`Test` gate step); `Test_Integration_Transcendental_MathsTypesUseTheSet`, `Test_Integration_Transcendental_IdenticalAcrossThreads` in `Zenith2/Tests/Core/` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Maths.md` → "Deterministic transcendental functions"; `Zenith2/Docs/Design/CORE/Maths.md` → "Transcendental declarations"; `Zenith2/Docs/Design/CORE/Maths.md` → "Shared evaluation rules"; `Zenith2/Docs/Design/CORE/Maths.md` → "Constants and coefficient tables"; `Zenith2/Docs/Design/CORE/Maths.md` → "Error bounds"; `Zenith2/Docs/Design/CORE/Maths.md` → "Golden vectors"

### Z2-R-CORE-172 CRT maths mode in every Windows main

- **Statement:** `Zenith_ApplyCrtMathMode()` calls `_set_FMA3_enable(0)` on Windows x64 and does nothing on Android and Linux, and `Zenith_IsCrtMathModeApplied()` reports the state; ZenithTestSupport's test main calls it as its first statement on every entry, so every test executable runs with the CRT's FMA3 dispatch off, and the deterministic functions return the same bits with the flag on or off.
- **Covers:** Z2-I-CORE-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_CoreMaths_Fma3DisabledAtStartup`, `Test_Transcendental_IndependentOfCrtFma3Mode` (`Test` gate step, `win-debug` and `win-release`; trivially true elsewhere); `Test_Integration_Transcendental_IdenticalAcrossThreads` in `Zenith2/Tests/Core/` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Maths.md` → "The CRT maths mode"; `Zenith2/Docs/Design/CORE/Memory.md` → "Test main steps"

### Z2-R-CORE-173 Single-producer single-consumer ring and triple buffer

- **Statement:** `Zenith_SpscRing<T>` over caller storage of a power-of-two N in [2, 2^20] holds exactly N elements, pushes and pops wait-free in FIFO order with the specified acquire and release orders, moves whole spans in at most two runs, counts drops (DROP_NEWEST) and high water, wraps its free-running 32-bit indices correctly and lets each role pass between threads by happens-before; `Zenith_TripleBuffer<T>` over three caller slots hands the newest published slot to the consumer without a copy or a torn read and counts superseded publishes; neither allocates, locks, logs, calls a profiler hook or makes a system call; both carry provisional justification records.
- **Covers:** Z2-I-CORE-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-041, Z2-D-011, Z2-D-097
- **Budgets:** none
- **Acceptance evidence:** `Test_SpscRing_MatchesOracle`, `Test_SpscRing_Properties`, `Test_SpscRing_TryFailure_LeavesUnchanged`, `Test_SpscRing_FifoOrderAndExactCapacity`, `Test_SpscRing_IndicesWrapPast32Bits`, `Test_SpscRing_SpansAcrossTheWrap`, `Test_SpscRing_StatsCountDropsAndHighWater`, `Test_SpscRing_InitRejectsBadCapacity` and `Test_SpscRing_SizeOverflow_IsRejected` (`z2-death-check`), `Test_SpscRing_UseBeforeInit_Asserts` (`z2-death-assert`), `Test_TripleBuffer_MatchesOracle`, `Test_TripleBuffer_Properties`, `Test_TripleBuffer_TryFailure_LeavesUnchanged`, `Test_TripleBuffer_InitialFrontIsSlotZero`, `Test_TripleBuffer_LatestWinsAndCountsSuperseded`, `Test_TripleBuffer_RepeatedBeginWriteReturnsSameSlot`, `Test_TripleBuffer_SizeOverflow_IsRejected` (`z2-death-check`) (`Test` gate step); `Test_SpscRing_StressRoleHandOver`, `Test_TripleBuffer_StressNoTornRead` (`Test` gate step and `Sanitizers -Preset linux-tsan` gate step); `Bench_CoreContainers_Smoke` cases `SpscRing_PushPop_16B`, `SpscRing_PushPop_192B`, `TripleBuffer_PublishAcquire_53KB`; `Test_Integration_RealTimeChannels_UnderNoAllocationScope` in `Zenith2/Tests/Core/` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_SpscRing"; `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_TripleBuffer"; `Zenith2/Docs/Design/CORE/Containers.md` → "Justification records"; `Zenith2/Docs/Design/CORE/Containers.md` → "Container tests"

### Z2-R-CORE-174 No-allocation scope checked by the heap

- **Statement:** Wherever `ZENITH_ASSERTS_ENABLED=1`, `Zenith_NoAllocationScope` (and `Zenith_EnterNoAllocationScope`/`Zenith_LeaveNoAllocationScope`) nests a per-thread depth in the Debug thread state, and `Zenith_Heap::Allocate`, `TryAllocate` and `Free` on a thread whose depth is non-zero fail `ZENITH_ASSERT` naming the scope before reaching the backing; other threads, and arena allocations served from a held block, are unaffected; leaving at depth 0 asserts; with asserts disabled every member is empty and the Debug thread state keeps one layout in every configuration.
- **Covers:** Z2-I-CORE-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_NoAllocationScope_HeapAllocateAsserts`, `Test_NoAllocationScope_HeapFreeAsserts`, `Test_NoAllocationScope_LeaveAtZero_Asserts` (`z2-death-assert`), `Test_NoAllocationScope_NestingAndRelease`, `Test_NoAllocationScope_OtherThreadsUnaffected`, `Test_NoAllocationScope_ArenaFromHeldBlockAllowed` (`Test` gate step); `Test_Integration_RealTimeChannels_UnderNoAllocationScope` in `Zenith2/Tests/Core/` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Threading.md` → "Zenith_NoAllocationScope"; `Zenith2/Docs/Design/CORE/Threading.md` → "No-allocation check steps"; `Zenith2/Docs/Design/CORE/Threading.md` → "Debug thread state"

### Z2-R-CORE-175 Test child-process launcher

- **Statement:** `ZenithTestSupport` provides `Zenith_TestProcess`, `Zenith_RunChildTestCase` and `Zenith_GetTestExecutablePath`, which launch the running test executable as a child running one named doctest case with added environment variables and extra arguments passed verbatim, wait with a timeout, kill and reap, capture standard output up to 1 MiB while draining it, and return the exit code or a `Zenith_TestProcessError`; the platform bodies are ZenithCore `Internal/Windows/` and `Internal/Linux/` files (Android reports unsupported) behind an internal header only the launcher includes, so no test source needs a platform conditional; a child never outlives its object.
- **Covers:** Z2-I-CORE-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-042, Z2-D-010
- **Budgets:** none
- **Acceptance evidence:** `Test_TestProcess_ChildExitCodeReturned`, `Test_TestProcess_StdoutCaptured`, `Test_TestProcess_VariablesAndArgumentsReachChild`, `Test_TestProcess_TimeoutThenKill`, `Test_TestProcess_DestructorKillsRunningChild`, `Test_TestProcess_OutputCapExceeded`, `Test_TestProcess_BadArguments_Rejected`, `Test_TestProcess_ExecutablePathIsSelf`, `Test_TestProcess_LaunchSelf_FailsAtArguments_Unwinds` (`Test` gate step, `win-debug`, `win-release`, `linux-tsan`); `Test_Integration_TestProcess_CrashingChildLeavesItsFile` in `Zenith2/Tests/Core/` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Memory.md` → "Test child processes"; `Zenith2/Docs/Design/CORE/Memory.md` → "Test child process platform matrix"; `Zenith2/Docs/Design/CORE/Memory.md` → "Test child process steps"

### Z2-R-CORE-176 Profiled and unprofiled primitives

- **Statement:** `Zenith_Mutex`, `Zenith_ConditionVariable` and `Zenith_Semaphore` each have an unprofiled twin (`Zenith_MutexNoProfiling`, `Zenith_ConditionVariableNoProfiling`, `Zenith_SemaphoreNoProfiling`), the two instantiations of one class template per platform on `bool bProfiled`; the unprofiled twin never reads the hook table, lock-level registration and the lock-order checks apply to both, `Zenith_ScopedLock` works with both, and a condition variable waits only on a mutex of its own variant, a `static_assert` refusing the mixed pairing.
- **Covers:** Z2-I-CORE-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-142, Z2-D-141
- **Budgets:** none
- **Acceptance evidence:** `Test_MutexNoProfiling_NeverCallsHooks`, `Test_ConditionVariableNoProfiling_NeverCallsHooks`, `Test_SemaphoreNoProfiling_NeverCallsHooks`, `Test_LockOrder_NoProfilingMutex_IsChecked`, `Test_BlockingWait_NoProfilingVariants_StillCheckJobWorker`, `Test_PlatformThreading_Aliases_NotCopyableNotMovable`, `Test_Semaphore_Name_ReportedAfterWait`, `Test_Mutex_ApiSurface_PlatformAlias`, the compile-fail entries `Test_ConditionVariable_NoProfilingOnProfiledMutex_DoesNotCompile` and `Test_ConditionVariable_ProfiledOnNoProfilingMutex_DoesNotCompile` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Threading.md → "Platform threading classes"`; `Zenith2/Docs/Design/CORE/Threading.md → "Mutexes and lock ordering"`; `Zenith2/Docs/Design/CORE/Threading.md → "Condition variables and semaphores"`; `Zenith2/Docs/Design/CORE/Threading.md → "Lock profiling and hook dispatch"`

### Z2-R-CORE-177 Lock acquire reports the wait

- **Statement:** A profiled `Lock` tries the native lock first and reports the acquisition to the acquire hook with a wait of 0; on contention it reads `Zenith_GetMonotonicTimeNs()` before and after blocking, only when an acquire hook is installed, and reports the measured wait as `ulWaitTicks`, at least 1 (0 means it did not block); `Unlock` reports the release from a copy of the desc; a profiled condition-variable wait reports a release before the native wait and an acquire with wait 0 after it, and the blocking-wait hook reports the time asleep after the wait returns, under the paired mutex's name (a semaphore under its own name); a successful `TryLock` reports a wait of 0 and a failed one calls no hook; with no hook installed the contended path still locks correctly and reads no clock.
- **Covers:** Z2-I-CORE-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-142, Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_Mutex_LockHooks_Called`, `Test_Mutex_Contended_ReportsMeasuredWait`, `Test_Mutex_TryLock_ReportsZeroWaitOnSuccessOnly`, `Test_Mutex_Contended_NoHooksInstalled_Works`, `Test_ConditionVariable_ProfiledWait_ReportsReleaseThenAcquire`, `Test_BlockingWait_CallsHook`, `Test_Integration_Threading_ProfiledLocksUnderCapture` (`Test` gate step)
- **Design:** `Zenith2/Docs/Design/CORE/Threading.md → "Mutexes and lock ordering"`; `Zenith2/Docs/Design/CORE/Threading.md → "Lock profiling and hook dispatch"`

### Z2-R-CORE-178 Hook dispatch and recursion guard

- **Statement:** Every invocation of the zone-begin, zone-end, counter, lock-acquire, lock-release and blocking-wait hooks goes through the inline `Zenith_Dispatch…` functions of `Zenith_ProfilerHooks.h`, which raise the calling thread's `m_uHookDepth` around the call; a dispatch made while the depth is non-zero does not call the hook, increments `m_uHookReentryCount` and, wherever asserts are enabled, fails `ZENITH_ASSERT`, so a hook that uses a profiled primitive recurses at most one level in every build, and the first refusal on each thread writes one diagnostic line; the guard state lives in the existing `g_xDebugThreadState` and adds no writable symbol.
- **Covers:** Z2-I-CORE-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-142, Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_ProfilerHooks_ApiSurface_Layout`, `Test_HookDispatch_EachDispatchCallsItsHook`, `Test_HookDispatch_DepthIsOneInsideHook`, `Test_HookDispatch_OtherThreadUnaffected`, `Test_HookDispatch_ReentrantAcquireHook_DoesNotRecurse`, `Test_HookDispatch_Reentry_CountedPerThread`, `Test_HookDispatch_Reentry_Asserts` (`z2-death-assert`), `Test_HookDispatch_Reentry_WritesOneDiagnosticLine` (`z2-death-check`), the four `Test_ProfilerHooks_Install_*_Checks` cases, `Test_HookDispatch_LogRecordHook_NotGuarded` (`Test` gate step); `ForbiddenCheck` F16 (no new writable symbol) and F26 (no direct hook-table call outside `Zenith_ProfilerHooks.h`)
- **Design:** `Zenith2/Docs/Design/CORE/Threading.md → "Lock profiling and hook dispatch"`; `Zenith2/Docs/Design/CORE/Threading.md → "Debug thread state"`

### Z2-R-CORE-179 Hook-reachable sources use unprofiled primitives

- **Statement:** The log router's sink-list and ring-sink locks and its drain semaphore are the unprofiled variants, and the ZenithCore sources a profiler hook dispatch can reach (the clock, `Zenith_SpscRing`, the Debug thread state, the thread identity functions, the failure procedure and the log router, as the design lists them) name no profiled `Zenith_Mutex`, `Zenith_ConditionVariable` or `Zenith_Semaphore`, so a hook that reads the clock, pushes an event ring, reads the thread identity or logs causes no re-entry; the router dispatches no counter from `OnRecord`, and a record logged from inside `OnRecord` is dropped and counted, and failed try-locks on the router's locks are counted in `m_ulContended`.
- **Covers:** Z2-I-CORE-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-142
- **Budgets:** none
- **Acceptance evidence:** `Test_LogRouter_LogFromInsideHook_DoesNotReenter`, `Test_Log_Router_NestedRecord_DroppedAndCounted`, `Test_Log_Router_Contended_CountsFailedTryLock`, `Test_HookDispatch_LogRecordHook_NotGuarded` (`Test` gate step); `ForbiddenCheck` F26 over the `profilerReachable` list (`Zenith2/Docs/Design/PROC/StructuralGates.md` → "ForbiddenCheck rule table")
- **Design:** `Zenith2/Docs/Design/CORE/Threading.md → "Lock profiling and hook dispatch"`

## Coverage of inventory items

| Inventory item | Item | Requirements |
|---|---|---|
| Z2-I-CORE-01 | ownership through `Zenith_Owned` | Z2-R-CORE-001, Z2-R-CORE-002, Z2-R-CORE-003 |
| Z2-I-CORE-02 | generational handles | Z2-R-CORE-004, Z2-R-CORE-005, Z2-R-CORE-006, Z2-R-CORE-007, Z2-R-CORE-077, Z2-R-CORE-078, Z2-R-CORE-105, Z2-R-CORE-106, Z2-R-CORE-107, Z2-R-CORE-108 |
| Z2-I-CORE-03 | the general heap | Z2-R-CORE-008, Z2-R-CORE-009, Z2-R-CORE-010, Z2-R-CORE-011, Z2-R-CORE-012, Z2-R-CORE-013, Z2-R-CORE-127, Z2-R-CORE-159, Z2-R-CORE-174 |
| Z2-I-CORE-04 | arenas | Z2-R-CORE-014, Z2-R-CORE-015, Z2-R-CORE-016, Z2-R-CORE-022 |
| Z2-I-CORE-05 | pools | Z2-R-CORE-017, Z2-R-CORE-018, Z2-R-CORE-019 |
| Z2-I-CORE-06 | sanitizer integration | Z2-R-CORE-008, Z2-R-CORE-016, Z2-R-CORE-019, Z2-R-CORE-020, Z2-R-CORE-021, Z2-R-CORE-160 |
| Z2-I-CORE-07 | containers | Z2-R-CORE-006, Z2-R-CORE-022, Z2-R-CORE-028, Z2-R-CORE-029, Z2-R-CORE-030, Z2-R-CORE-031, Z2-R-CORE-032, Z2-R-CORE-033, Z2-R-CORE-034, Z2-R-CORE-035, Z2-R-CORE-036, Z2-R-CORE-037, Z2-R-CORE-038, Z2-R-CORE-039, Z2-R-CORE-040, Z2-R-CORE-041, Z2-R-CORE-042, Z2-R-CORE-043, Z2-R-CORE-044, Z2-R-CORE-070, Z2-R-CORE-071, Z2-R-CORE-072, Z2-R-CORE-173 |
| Z2-I-CORE-08 | strings | Z2-R-CORE-045, Z2-R-CORE-046, Z2-R-CORE-047, Z2-R-CORE-048, Z2-R-CORE-049, Z2-R-CORE-050, Z2-R-CORE-055, Z2-R-CORE-056, Z2-R-CORE-078, Z2-R-CORE-118, Z2-R-CORE-119, Z2-R-CORE-120, Z2-R-CORE-121, Z2-R-CORE-122, Z2-R-CORE-124, Z2-R-CORE-125, Z2-R-CORE-126, Z2-R-CORE-158, Z2-R-CORE-168 |
| Z2-I-CORE-09 | Unicode | Z2-R-CORE-046, Z2-R-CORE-051, Z2-R-CORE-052, Z2-R-CORE-053, Z2-R-CORE-054 |
| Z2-I-CORE-10 | numerical conventions | Z2-R-CORE-056, Z2-R-CORE-057, Z2-R-CORE-058, Z2-R-CORE-059, Z2-R-CORE-065, Z2-R-CORE-066, Z2-R-CORE-067, Z2-R-CORE-068, Z2-R-CORE-069, Z2-R-CORE-171, Z2-R-CORE-172 |
| Z2-I-CORE-11 | units | Z2-R-CORE-060, Z2-R-CORE-061 |
| Z2-I-CORE-12 | coordinate systems | Z2-R-CORE-062, Z2-R-CORE-063, Z2-R-CORE-064, Z2-R-CORE-066 |
| Z2-I-CORE-13 | random streams | Z2-R-CORE-070, Z2-R-CORE-072, Z2-R-CORE-073, Z2-R-CORE-074, Z2-R-CORE-075, Z2-R-CORE-076, Z2-R-CORE-077 |
| Z2-I-CORE-14 | clocks | Z2-R-CORE-079, Z2-R-CORE-080, Z2-R-CORE-081, Z2-R-CORE-082 |
| Z2-I-CORE-15 | diagnostics | Z2-R-CORE-083, Z2-R-CORE-084, Z2-R-CORE-085, Z2-R-CORE-086, Z2-R-CORE-087, Z2-R-CORE-088, Z2-R-CORE-089, Z2-R-CORE-090, Z2-R-CORE-091, Z2-R-CORE-092, Z2-R-CORE-093, Z2-R-CORE-094, Z2-R-CORE-095, Z2-R-CORE-096, Z2-R-CORE-099, Z2-R-CORE-100, Z2-R-CORE-103, Z2-R-CORE-104, Z2-R-CORE-155, Z2-R-CORE-156, Z2-R-CORE-157, Z2-R-CORE-160, Z2-R-CORE-161, Z2-R-CORE-162, Z2-R-CORE-163, Z2-R-CORE-170, Z2-R-CORE-175, Z2-R-CORE-176, Z2-R-CORE-177, Z2-R-CORE-178, Z2-R-CORE-179 |
| Z2-I-CORE-16 | configuration | Z2-R-CORE-109, Z2-R-CORE-110, Z2-R-CORE-111, Z2-R-CORE-112, Z2-R-CORE-113, Z2-R-CORE-114, Z2-R-CORE-115, Z2-R-CORE-116, Z2-R-CORE-117, Z2-R-CORE-118, Z2-R-CORE-120, Z2-R-CORE-123, Z2-R-CORE-126, Z2-R-CORE-158, Z2-R-CORE-164, Z2-R-CORE-165, Z2-R-CORE-166 |
| Z2-I-CORE-17 | allocation-failure behaviour | Z2-R-CORE-002, Z2-R-CORE-014, Z2-R-CORE-023, Z2-R-CORE-024, Z2-R-CORE-025, Z2-R-CORE-026, Z2-R-CORE-027, Z2-R-CORE-154 |
| Z2-I-CORE-18 | a cross-system memory governor | Z2-R-CORE-025, Z2-R-CORE-128, Z2-R-CORE-129, Z2-R-CORE-130, Z2-R-CORE-136, Z2-R-CORE-137, Z2-R-CORE-141, Z2-R-CORE-142, Z2-R-CORE-143, Z2-R-CORE-149, Z2-R-CORE-150, Z2-R-CORE-169 |
| Z2-I-CORE-19 | per-subsystem memory budget: rendering | Z2-R-CORE-131 |
| Z2-I-CORE-20 | per-subsystem memory budget: streaming | Z2-R-CORE-132 |
| Z2-I-CORE-21 | per-subsystem memory budget: audio | Z2-R-CORE-133 |
| Z2-I-CORE-22 | per-subsystem memory budget: scripts | Z2-R-CORE-134 |
| Z2-I-CORE-23 | per-subsystem memory budget: editor caches | Z2-R-CORE-135 |
| Z2-I-CORE-24 | pressure levels (normal/warning/critical) derived from the OS | Z2-R-CORE-138, Z2-R-CORE-140, Z2-R-CORE-153 |
| Z2-I-CORE-25 | pressure levels (normal/warning/critical) derived from the budgets | Z2-R-CORE-139, Z2-R-CORE-140, Z2-R-CORE-169 |
| Z2-I-CORE-26 | defined shedding per subsystem at each pressure level | Z2-R-CORE-144, Z2-R-CORE-145, Z2-R-CORE-153 |
| Z2-I-CORE-27 | transition peak budgeted explicitly: streaming double-residency | Z2-R-CORE-146, Z2-R-CORE-149, Z2-R-CORE-150 |
| Z2-I-CORE-28 | transition peak budgeted explicitly: world transitions | Z2-R-CORE-147, Z2-R-CORE-149, Z2-R-CORE-150 |
| Z2-I-CORE-29 | transition peak budgeted explicitly: texture swaps | Z2-R-CORE-148, Z2-R-CORE-149, Z2-R-CORE-150 |
| Z2-I-CORE-30 | tests that force each memory pressure level | Z2-R-CORE-151, Z2-R-CORE-152 |

Cross-area items whose ZenithCore half CORE owns (the items themselves stay covered by their own area's catalogue):

| Inventory item | Owning area | CORE requirements |
|---|---|---|
| Z2-I-JOBS-01 | JOBS | Z2-R-CORE-097, Z2-R-CORE-098, Z2-R-CORE-101 |
| Z2-I-JOBS-06 | JOBS | Z2-R-CORE-042, Z2-R-CORE-102 |
| Z2-I-JOBS-07 | JOBS | Z2-R-CORE-107, Z2-R-CORE-108 |
| Z2-I-JOBS-09 | JOBS | Z2-R-CORE-099, Z2-R-CORE-102 |
| Z2-I-SCENE-01 | SCENE (the host SHA-256 its derived authoring IDs use, X152) | Z2-R-CORE-167 |
| Z2-I-SEC-08 | SEC (the archive digests PLAT's patch mount checks, X152) | Z2-R-CORE-167 |
| Z2-I-ECS-03 | ECS (the document cursor every migration step receives, cross-area notes C13 and S5) | Z2-R-CORE-168 |

## Excluded and deferred

CORE has no excluded or deferred inventory item. Nearby capabilities that CORE does not provide, each owned elsewhere or forbidden by a recorded decision:

| Capability | Classification for CORE | Rationale |
|---|---|---|
| STL smart pointers, `std::function` in runtime headers, exceptions | excluded | Z2-D-040, Z2-D-041, Z2-D-045: forbidden by standing rulings, not re-justified |
| a `Zenith_Ptr` checked non-owning pointer | excluded | Z2-D-040 |
| per-allocation headers and debug-only members | excluded | Z2-D-040; `Zenith2/Docs/Contracts.md` → "No layout-changing conditionals" |
| runtime Unicode normalisation and locale-dependent case folding | excluded | text shaping and localisation are UI's (D-SEL-TEXT (open, Stage 1/2)); CORE compares bytes (`Zenith2/Docs/Design/CORE/Text.md` → "Encoding policy") |
| a concurrent pool, a concurrent hash map, lists, deques, sorted maps, priority queues, small-buffer arrays | not provided | no consumer needs them; a ticket that needs one adds it with its justification record (Z2-D-041) |
| half-float and vertex codecs | owned by GEOM | `Zenith2/Docs/Rendering.md` → "Vertex data format" |
| file log sink, on-screen console, crash dumps | owned by DEVT and PLAT | Z2-I-DEVT-22, D-SEL-CRASH (open, Stage 2) |
| configuration sources and `Zenith_Config` | owned by PLAT | `Zenith2/Docs/Architecture/PlatformLayer.md` → "Configuration layering" |
