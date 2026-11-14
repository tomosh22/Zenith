# GRAPH requirements — ZenithGraph shared core and the CPU frame graph

## Purpose

This catalogue breaks the GRAPH inventory items of `Zenith2/Docs/Requirements/Inventory.md` → "Inventory v1" into testable requirements `Z2-R-GRAPH-NNN`. The design that specifies each requirement is `Zenith2/Docs/Design/GRAPH.md` and its split files; each requirement names the headings that specify it and the exact evidence that proves it. The specification the design elaborates is `Zenith2/Docs/Architecture.md` → "ZenithGraph shared core" and `Zenith2/Docs/Architecture.md` → "CPU frame graph and scheduler". Stages are the stage whose acceptance the requirement serves; the ZenithGraph core requirements of Stage 2 are pulled forward into the Stage 1 sprint once the specification ticket is Done.

## Requirements

### Z2-R-GRAPH-001 Stage 1 specification confirmation

- **Statement:** The Stage 1 ticket "The CPU frame-graph and ZenithGraph specification" confirms the access model, hazard rules, schedule compilation, dynamic sub-jobs and pipelining of this design against the JOBS, ECS, MOD and RHI designs, resolves every GRAPH row of the cross-area notes, and appends its confirmation ruling to `Zenith2/Docs/DecisionLog.md` with an explicit `Z2-D-NNN` in its own diff.
- **Covers:** Z2-I-GRAPH-01, Z2-I-GRAPH-02, Z2-I-GRAPH-08, Z2-I-GRAPH-09, Z2-I-GRAPH-12
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-050, Z2-D-080, Z2-D-085
- **Budgets:** none
- **Acceptance evidence:** the DecisionLog entry appended by the specification ticket, checked by the coverage gate step in acceptance mode for Stage 1
- **Design:** `Zenith2/Docs/Design/GRAPH.md` → "Stage 1"; `Zenith2/Docs/Design/GRAPH.md` → "Relationship to the specification"

### Z2-R-GRAPH-002 Budget harness for the schedule recompile

- **Statement:** The harness `Zenith_Bench_ScheduleRecompile` under `Zenith2/Benchmarks/Budgets/GRAPH/` times frame-graph rebuild plus a forced-miss `Zenith_FrameScheduleCache::Acquire` for the workload frame graphs (10 repetitions, median and 95% CI), and its smoke test `Test_BudgetHarness_Graph_Smoke` runs one repetition.
- **Covers:** Z2-I-GRAPH-08
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-012, Z2-D-084
- **Budgets:** Z2-B-GRAPH-001
- **Acceptance evidence:** `Test_BudgetHarness_Graph_Smoke` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/GRAPH.md` → "Measurement harness"; `Zenith2/Docs/Design/GRAPH/Tests.md` → "Benchmarks"

### Z2-R-GRAPH-003 Schedule recompile row allocated

- **Statement:** The Stage 1 per-system budget ticket for GRAPH runs the harness on the reference desktop and the S24 and writes a value for Z2-B-GRAPH-001 into `Zenith2/Docs/Budgets.md` with its DecisionLog entry.
- **Covers:** Z2-I-GRAPH-08
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-012, Z2-D-084
- **Budgets:** Z2-B-GRAPH-001
- **Acceptance evidence:** the adopted row in `Zenith2/Docs/Budgets.md` → "Per-system rows" and the report under `Zenith2/Docs/Research/`, checked by the Stage 1 acceptance coverage run
- **Design:** `Zenith2/Docs/Design/GRAPH.md` → "Rows this area owns"; `Zenith2/Docs/Design/GRAPH.md` → "Stage 1"

### Z2-R-GRAPH-004 Zenith_Graph.h declares the specified API

- **Statement:** `Zenith2/Engine/ZenithGraph/Zenith_Graph.h` declares exactly the declarations of the design's header listing (the specification's plus the listed additions), each public type with one contract block, and ZenithGraph declares only ZenithCore as a dependency.
- **Covers:** Z2-I-GRAPH-01, Z2-I-GRAPH-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-047
- **Budgets:** none
- **Acceptance evidence:** the `HeaderSelfContained` and `ApiContractCheck` gate steps; `Test_Graph_ValueTypes_DefaultsInvalid` and `Test_Graph_Example` in the `Test` gate step (win-debug, win-release); the `Libgraph` gate step
- **Design:** `Zenith2/Docs/Design/GRAPH.md` → "Zenith_Graph.h"; `Zenith2/Docs/Design/GRAPH.md` → "Declarations added to the specification"; `Zenith2/Docs/Design/GRAPH.md` → "ZenithGraph"

### Z2-R-GRAPH-005 Declaration errors are sticky and never visible at the call

- **Statement:** A declaration error is recorded and the declaration continues; `Compile` writes every recorded and every derived diagnostic into the caller's `Zenith_GraphDiagnosticReport` and returns an error record carrying the first code and the count.
- **Covers:** Z2-I-GRAPH-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_Graph_DeclarationErrorsAreSticky` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Declaration algorithms"; `Zenith2/Docs/Design/GRAPH.md` → "Declaration errors"

### Z2-R-GRAPH-006 Poisoned handles do not cascade

- **Statement:** A failed declaration returns a poisoned handle carrying the graph's serial, and a subsequent call that receives it records nothing and does not assert.
- **Covers:** Z2-I-GRAPH-01, Z2-I-GRAPH-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_Graph_PoisonedHandle_NoCascade` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Error records and poisoned handles"

### Z2-R-GRAPH-007 Foreign and out-of-range handles

- **Statement:** A handle of another graph or an index at or above the table count triggers `ZENITH_ASSERT` where asserts are enabled and records `GRAPH_ERROR_INVALID_HANDLE` in every build.
- **Covers:** Z2-I-GRAPH-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphConformance_InvalidHandleRecorded` in the `Test` gate step (target `ZenithGraph_Conformance`) and the `Zenith2Memory` `Sanitizers` lines; `Test_Graph_ForeignHandle_Asserts` (z2-death-assert) in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Error records and poisoned handles"

### Z2-R-GRAPH-008 Graph serial is non-zero

- **Statement:** A `Zenith_Graph` constructed with serial 0 fails `ZENITH_CHECK`; front ends supply serials from a member counter.
- **Covers:** Z2-I-GRAPH-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_Graph_ZeroSerial_Checks` (z2-death-check) in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/GRAPH.md` → "Zenith_Graph.h"; `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Step 6 lowering into ZenithGraph"

### Z2-R-GRAPH-009 Access-mode semantics

- **Statement:** Each access mode reads and produces versions and is legal as a first access exactly as the specification's mode table states.
- **Covers:** Z2-I-GRAPH-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_Graph_AccessModeTable` in the `Test` gate step (win-debug, win-release); the conformance scenarios RawEdge, WarEdge, ReadWriteSingleNode and FirstWriteMustDiscard in the `Test` gate step (target `ZenithGraph_Conformance`) and the `Zenith2Memory` `Sanitizers` lines
- **Design:** `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "DeclareRead DeclareWrite and DeclareAccess"; `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 2 hazard derivation per resource"

### Z2-R-GRAPH-010 Subrange validation

- **Statement:** An empty subrange span means the whole resource; a zero count, a range past the subresource count, or a resource declared with zero subresources records `GRAPH_ERROR_SUBRANGE_OUT_OF_BOUNDS`.
- **Covers:** Z2-I-GRAPH-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_Graph_EmptySubrangeIsWholeResource`, `Test_Graph_ZeroSubresources_IsError` in the `Test` gate step (win-debug, win-release); `Test_GraphConformance_SubrangeOutOfBounds` in the `Test` gate step (target `ZenithGraph_Conformance`) and the `Zenith2Memory` `Sanitizers` lines
- **Design:** `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "DeclareRead DeclareWrite and DeclareAccess"; `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "AddNode and AddResource"

### Z2-R-GRAPH-011 Resource and node flags

- **Statement:** Unknown flag bits are asserted and cleared, `MarkOutput` sets `GRAPH_RESOURCE_FLAG_OUTPUT` idempotently, and the flags act as the specification defines.
- **Covers:** Z2-I-GRAPH-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_Graph_UnknownFlagBits_Cleared`, `Test_Graph_MarkOutputIdempotent` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "AddOrdering and MarkOutput"; `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "AddNode and AddResource"

### Z2-R-GRAPH-012 Opaque backend fields are never interpreted

- **Statement:** Lanes, backend usages and payloads are stored, copied into the schedule and the dump, and never change an edge, order, count or lifetime; payloads are excluded from the structure hash.
- **Covers:** Z2-I-GRAPH-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-055
- **Budgets:** none
- **Acceptance evidence:** `Test_Graph_OpaqueFieldsDoNotAffectSchedule` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/GRAPH.md` → "Canonical structure serialization"; `Zenith2/Docs/Design/GRAPH.md` → "Zenith_Graph.h"

### Z2-R-GRAPH-013 Arena-only allocation

- **Statement:** `Zenith_Graph` allocates only from its constructor arena and `Compile` only from its schedule arena; neither touches the general heap.
- **Covers:** Z2-I-GRAPH-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphConformance_ArenaOnlyAllocation` in the `Test` gate step (target `ZenithGraph_Conformance`) and the `Zenith2Memory` `Sanitizers` lines
- **Design:** `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Compile step list"; `Zenith2/Docs/Design/GRAPH/Tests.md` → "Scenario table"

### Z2-R-GRAPH-014 Capacity failures are reported, never fatal

- **Statement:** Arena exhaustion or a `uGRAPH_MAX_*` or name-length limit records `GRAPH_ERROR_CAPACITY` with the design's text, turns subsequent declarations into no-ops, and never crashes or asserts, including when the schedule arena cannot hold the diagnostics.
- **Covers:** Z2-I-GRAPH-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphConformance_CapacityErrorReported` in the `Test` gate step (target `ZenithGraph_Conformance`) and the `Zenith2Memory` `Sanitizers` lines; `Test_Graph_NodeLimitReported`, `Test_Graph_NameTooLong_IsCapacityError` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/GRAPH.md` → "Arena exhaustion"; `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 13 diagnostics assembly"; `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Name storage"

### Z2-R-GRAPH-015 Names are copied

- **Statement:** Node and resource names, ordering reasons and the front-end tag are copied into the graph arena, so the caller's storage may die after the call.
- **Covers:** Z2-I-GRAPH-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_Graph_NamesCopied` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Name storage"

### Z2-R-GRAPH-016 No custom container

- **Statement:** GRAPH adds no custom container: every growable declaration table of `Zenith_Graph` and of `Zenith_FrameGraphBuilder` is a `Zenith_Array` in its arena tier, and references returned by `GetNodeDesc` and `GetResourceDesc` are valid only until the next `AddNode` or `AddResource`.
- **Covers:** Z2-I-GRAPH-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-041
- **Budgets:** none
- **Acceptance evidence:** `Test_Graph_DescReferencesInvalidatedByAdd` in the `Test` gate step (win-debug, win-release) (including win-asan); the `ForbiddenCheck` and `Libgraph` gate steps; review that no GRAPH diff adds a container type
- **Design:** `Zenith2/Docs/Design/GRAPH.md` → "Custom infrastructure records this area adds"; `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Table storage"

### Z2-R-GRAPH-017 Concurrent compile of one graph

- **Statement:** After declarations end, `Compile`, `WriteJson` and the getters run concurrently from several threads, each compile into its own arena, producing identical schedules.
- **Covers:** Z2-I-GRAPH-01, Z2-I-GRAPH-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphConformance_ConcurrentCompileOfOneGraph` (z2-stress) in the `Test` gate step (target `ZenithGraph_Conformance`) and the `Zenith2Memory` `Sanitizers` lines including linux-tsan
- **Design:** `Zenith2/Docs/Design/GRAPH.md` → "Thread affinity per type"; `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Compile step list"

### Z2-R-GRAPH-018 Version chain

- **Statement:** Every writing access consumes the latest version and returns the handle of the next; reads return their input handle; `GetLatestVersion` reports the write count.
- **Covers:** Z2-I-GRAPH-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_Graph_VersionChain` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "DeclareRead DeclareWrite and DeclareAccess"

### Z2-R-GRAPH-019 Version forks

- **Statement:** A write from a version that is not the latest records `GRAPH_ERROR_VERSION_FORK` naming the first writer of the next version, the second writer and the version, with the specification's text.
- **Covers:** Z2-I-GRAPH-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphConformance_VersionForkIsError` in the `Test` gate step (target `ZenithGraph_Conformance`) and the `Zenith2Memory` `Sanitizers` lines
- **Design:** `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "DeclareRead DeclareWrite and DeclareAccess"; `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Other diagnostics"

### Z2-R-GRAPH-020 Old-version reads

- **Statement:** A read of an older version orders before every overlapping write that produced a newer version.
- **Covers:** Z2-I-GRAPH-03, Z2-I-GRAPH-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphConformance_OldVersionReadOrdersBeforeWriter` in the `Test` gate step (target `ZenithGraph_Conformance`) and the `Zenith2Memory` `Sanitizers` lines
- **Design:** `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 1 flatten and group accesses"; `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 2 hazard derivation per resource"

### Z2-R-GRAPH-021 Imported resources start defined

- **Statement:** An imported resource starts with the virtual import writer, which creates no edge, so its reads and partial first writes are legal.
- **Covers:** Z2-I-GRAPH-03, Z2-I-GRAPH-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphConformance_ImportedReadIsLegal`, `Test_GraphConformance_ImportedPartialFirstWriteIsLegal` in the `Test` gate step (target `ZenithGraph_Conformance`) and the `Zenith2Memory` `Sanitizers` lines
- **Design:** `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 2 hazard derivation per resource"

### Z2-R-GRAPH-022 Read-after-write edges

- **Statement:** A read of a version produced by a node creates exactly one RAW edge from that writer to the reader over overlapping subresources.
- **Covers:** Z2-I-GRAPH-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphConformance_RawEdgeFromWriterToReader` in the `Test` gate step (target `ZenithGraph_Conformance`) and the `Zenith2Memory` `Sanitizers` lines
- **Design:** `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 2 hazard derivation per resource"

### Z2-R-GRAPH-023 Write-after-read edges

- **Statement:** Every reader of an interval gets a WAR edge to the next write that overlaps it.
- **Covers:** Z2-I-GRAPH-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphConformance_WarEdgeFromReaderToNextWriter` in the `Test` gate step (target `ZenithGraph_Conformance`) and the `Zenith2Memory` `Sanitizers` lines
- **Design:** `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 2 hazard derivation per resource"

### Z2-R-GRAPH-024 Write-after-write edges only without a reader

- **Statement:** Two overlapping writes with no reader between them create one WAW edge, and none when a reader intervenes.
- **Covers:** Z2-I-GRAPH-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphConformance_WawEdgeWithoutInterveningReader`, `Test_GraphConformance_NoWawEdgeWhenReaderIntervenes` in the `Test` gate step (target `ZenithGraph_Conformance`) and the `Zenith2Memory` `Sanitizers` lines
- **Design:** `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 2 hazard derivation per resource"

### Z2-R-GRAPH-025 Subrange overlap decides edges

- **Statement:** Accesses of disjoint subranges create no edge and may run concurrently; accesses overlapping in one subresource create an edge.
- **Covers:** Z2-I-GRAPH-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphConformance_DisjointSubrangeWritesHaveNoEdge`, `Test_GraphConformance_PartialOverlapCreatesEdge` in the `Test` gate step (target `ZenithGraph_Conformance`) and the `Zenith2Memory` `Sanitizers` lines
- **Design:** `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "The subresource interval map"

### Z2-R-GRAPH-026 Undefined reads

- **Statement:** A read of a never-written part of a non-imported resource with `GRAPH_RESOURCE_FLAG_REQUIRE_DEFINED_READS` records `GRAPH_ERROR_UNDEFINED_READ` with the uncovered range.
- **Covers:** Z2-I-GRAPH-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-057
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphConformance_UndefinedReadIsError` in the `Test` gate step (target `ZenithGraph_Conformance`) and the `Zenith2Memory` `Sanitizers` lines
- **Design:** `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 2 hazard derivation per resource"

### Z2-R-GRAPH-027 First write must discard

- **Statement:** The first write of a non-imported resource with `GRAPH_RESOURCE_FLAG_FIRST_WRITE_DISCARD` that is not `WRITE_DISCARD` over every subresource records `GRAPH_ERROR_FIRST_WRITE_NOT_DISCARD`; under `GRAPH_RESOURCE_FLAG_FIRST_WRITE_DISCARD_PER_SUBRESOURCE` instead, a first write of a subresource interval that is not `WRITE_DISCARD` records it naming that interval, and first writers of disjoint intervals gain no edge.
- **Covers:** Z2-I-GRAPH-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-057
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphConformance_FirstWriteMustDiscard` and `Test_GraphConformance_PerSubresourceFirstWriteDiscard` in the `Test` gate step (target `ZenithGraph_Conformance`) and the `Zenith2Memory` `Sanitizers` lines
- **Design:** `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 2 hazard derivation per resource"

### Z2-R-GRAPH-028 Read-write in one node

- **Statement:** A `READ_WRITE` access orders after the prior writer and before the next reader, and creates no self-edge.
- **Covers:** Z2-I-GRAPH-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphConformance_ReadWriteSingleNode` in the `Test` gate step (target `ZenithGraph_Conformance`) and the `Zenith2Memory` `Sanitizers` lines
- **Design:** `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 3 edge sort and deduplication"

### Z2-R-GRAPH-029 Explicit orderings

- **Statement:** `AddOrdering` creates an ORDERING edge carrying the ordering's declaration index, and ordering a node before itself records `GRAPH_ERROR_SELF_ORDERING`.
- **Covers:** Z2-I-GRAPH-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphConformance_ExplicitOrderingEdge`, `Test_GraphConformance_SelfOrderingIsError` in the `Test` gate step (target `ZenithGraph_Conformance`) and the `Zenith2Memory` `Sanitizers` lines
- **Design:** `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "AddOrdering and MarkOutput"; `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 3 edge sort and deduplication"

### Z2-R-GRAPH-030 Edge deduplication and order

- **Statement:** Edges are deduplicated by (from, to) keeping the reason in the precedence RAW, WAR, WAW, ORDERING, and the kept list is sorted by (from, to, reason, resource).
- **Covers:** Z2-I-GRAPH-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphConformance_ReasonPrecedenceOnDuplicate` in the `Test` gate step (target `ZenithGraph_Conformance`) and the `Zenith2Memory` `Sanitizers` lines
- **Design:** `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 3 edge sort and deduplication"

### Z2-R-GRAPH-031 Hazard completeness

- **Statement:** On random graphs every pair of conflicting accesses is ordered by a path of kept edges, as a brute-force reference computes.
- **Covers:** Z2-I-GRAPH-02, Z2-I-GRAPH-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphConformance_RandomDagProperty` (z2-stress) in the `Test` gate step (target `ZenithGraph_Conformance`) and the `Zenith2Memory` `Sanitizers` lines
- **Design:** `Zenith2/Docs/Design/GRAPH/Tests.md` → "Scenario table"; `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Determinism proof obligations"

### Z2-R-GRAPH-032 Several subranges in one access

- **Statement:** An access with several subranges orders against exactly the writers and readers of the subresources those subranges cover, and the reader pool sized by the design's bound holds every reader link of a fragmented resource.
- **Covers:** Z2-I-GRAPH-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphConformance_MultipleSubrangesInOneAccess` in the `Test` gate step (target `ZenithGraph_Conformance`) and the `Zenith2Memory` `Sanitizers` lines; `Test_Graph_FragmentedReadsCompile` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "The subresource interval map"

### Z2-R-GRAPH-033 Culling rules

- **Statement:** With culling on, a node is live when it has the side-effect flag, is a final writer of an output resource, or is the source of a RAW or preserving-WAW edge into a live node; every other node is culled and absent from the order.
- **Covers:** Z2-I-GRAPH-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-055
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphConformance_CullUnusedNodes`, `Test_GraphConformance_SideEffectNodeNeverCulled`, `Test_GraphConformance_OutputKeepsProducers`, `Test_GraphConformance_PreservingWriteKeepsProducer` in the `Test` gate step (target `ZenithGraph_Conformance`) and the `Zenith2Memory` `Sanitizers` lines
- **Design:** `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 6 culling"

### Z2-R-GRAPH-034 Culled nodes leave no edges

- **Statement:** Edges incident to culled nodes are removed from the schedule, and orderings through a culled node are not preserved.
- **Covers:** Z2-I-GRAPH-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_Graph_CullDropsIncidentEdges` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 6 culling"

### Z2-R-GRAPH-035 The CPU front end never culls

- **Statement:** The CPU lowering compiles every subgraph with culling off, so every declared node is scheduled and run.
- **Covers:** Z2-I-GRAPH-04, Z2-I-GRAPH-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphConformance_NoCullingWhenDisabled` in the `Test` gate step (target `ZenithGraph_Conformance`) and the `Zenith2Memory` `Sanitizers` lines; `Test_FrameGraph_NoNodeCulled` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Step 7 compile and wrap"

### Z2-R-GRAPH-036 Cycle detection

- **Statement:** Tarjan's algorithm over all nodes reports one `GRAPH_ERROR_CYCLE` diagnostic per strongly connected component of more than one node.
- **Covers:** Z2-I-GRAPH-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_Graph_TwoCyclesTwoDiagnostics` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 4 cycle detection"

### Z2-R-GRAPH-037 Cycle path extraction

- **Statement:** Each cycle diagnostic names the BFS-shortest cycle from the component's lowest-index node back to itself, in path order.
- **Covers:** Z2-I-GRAPH-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_Graph_CyclePathFromLowestIndex` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 5 cycle path extraction and text"

### Z2-R-GRAPH-038 Cycle block text

- **Statement:** The cycle diagnostic text follows the cycle block format exactly: detail lines indented six spaces, the ORDERING form `After(<from>) declared by <to>: <reason>` of the specification and the subrange suffix after every access declared with explicit subranges, naming each edge's reason, resource, version and the two accesses or the ordering and its reason.
- **Covers:** Z2-I-GRAPH-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphConformance_CycleDiagnosticNamesAccesses`, `Test_GraphConformance_CycleOfOrderingsOnly` in the `Test` gate step (target `ZenithGraph_Conformance`) and the `Zenith2Memory` `Sanitizers` lines; `Test_GraphDiagnostics_WorkedExamples` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Cycle block"; `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Worked examples"

### Z2-R-GRAPH-039 Every error in one pass

- **Statement:** One compile reports every recorded and derived error, sorted by code and then by node A.
- **Covers:** Z2-I-GRAPH-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphConformance_AllErrorsReportedInOnePass` in the `Test` gate step (target `ZenithGraph_Conformance`) and the `Zenith2Memory` `Sanitizers` lines
- **Design:** `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Compile step list"; `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 13 diagnostics assembly"

### Z2-R-GRAPH-040 Message formats for every error

- **Statement:** Every core error code and variant produces exactly the text of the design's message table, and no diagnostic text contains a tab character.
- **Covers:** Z2-I-GRAPH-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphDiagnostics_MessageFormats` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Other diagnostics"; `Zenith2/Docs/Design/GRAPH.md` → "Error codes"

### Z2-R-GRAPH-041 Error enumeration and record shape

- **Statement:** `Zenith_GraphErrorCode` enumerators are numbered from 1, each enumeration of `Zenith_Graph.h` has a `Zenith_ToString` returning the enumerator name without prefix, and `Zenith_GraphCompileError` is the 16-byte span-free record `{ m_eCode, m_uDiagnosticCount, m_ulStructureHash }` whose code is the report's first diagnostic's code.
- **Covers:** Z2-I-GRAPH-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_Graph_ToStringNames` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/GRAPH.md` → "Zenith_Graph.h"

### Z2-R-GRAPH-042 Execution order

- **Statement:** The order is Kahn's algorithm over non-culled nodes with a ready set ordered by priority descending and declaration index ascending.
- **Covers:** Z2-I-GRAPH-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphConformance_TopologicalOrderRespectsEdges`, `Test_GraphConformance_ReadyTieBreakByDeclarationIndex` in the `Test` gate step (target `ZenithGraph_Conformance`) and the `Zenith2Memory` `Sanitizers` lines
- **Design:** `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 8 execution order"

### Z2-R-GRAPH-043 Critical-path priority

- **Statement:** A node's priority is its cost, clamped to at least 1, plus the maximum priority of its successors, in `uint64`.
- **Covers:** Z2-I-GRAPH-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphConformance_PriorityIsBottomLevel` in the `Test` gate step (target `ZenithGraph_Conformance`) and the `Zenith2Memory` `Sanitizers` lines; `Test_Graph_ZeroCostClampedToOne` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 7 critical-path priority"

### Z2-R-GRAPH-044 Transitive reduction

- **Statement:** With reduction on and at most the node limit, every edge implied by another path is removed; above the limit reduction is skipped, flagged and logged once by the front end.
- **Covers:** Z2-I-GRAPH-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphConformance_TransitiveReductionRemovesImpliedEdges`, `Test_GraphConformance_ReductionSkippedAboveLimit` in the `Test` gate step (target `ZenithGraph_Conformance`) and the `Zenith2Memory` `Sanitizers` lines
- **Design:** `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 9 transitive reduction"

### Z2-R-GRAPH-045 Dependency counts and successor ranges

- **Statement:** Each dependency count equals the node's in-degree over the kept edges, and every kept edge lies in its source's successor range.
- **Covers:** Z2-I-GRAPH-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphConformance_DependencyCountsMatchEdges` in the `Test` gate step (target `ZenithGraph_Conformance`) and the `Zenith2Memory` `Sanitizers` lines
- **Design:** `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 10 dependency counts and successor ranges"

### Z2-R-GRAPH-046 Reprioritisation equals compile

- **Statement:** `Zenith_GraphReprioritise` produces the schedule `Compile` produces for the same declarations and the new costs, self-contained in its arena, with edges, counts and hash unchanged; its only failure is `GRAPH_ERROR_CAPACITY`, reported through its `Zenith_GraphDiagnosticReport`.
- **Covers:** Z2-I-GRAPH-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphConformance_ReprioritiseMatchesCompile` in the `Test` gate step (target `ZenithGraph_Conformance`) and the `Zenith2Memory` `Sanitizers` lines
- **Design:** `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Reprioritisation"

### Z2-R-GRAPH-047 Structure hash

- **Statement:** The structure hash is `Zenith_Hash64` over the canonical serialization, streamed through `Zenith_Hash64Stream` with no scratch so that it cannot fail, excludes costs, payloads, reasons and names of the graph, and `Zenith_GraphStructureHash` and `Zenith_GraphCanonicalBytes` agree with the schedule's hash.
- **Covers:** Z2-I-GRAPH-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphConformance_StructureHashIgnoresCosts` in the `Test` gate step (target `ZenithGraph_Conformance`) and the `Zenith2Memory` `Sanitizers` lines; `Test_Graph_StructureHashMatchesSchedule` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/GRAPH.md` → "Canonical structure serialization"; `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 12 structure hash"

### Z2-R-GRAPH-048 Schedules are immutable and self-contained

- **Statement:** A schedule holds no pointer into the graph arena and stays valid after the graph is destroyed, for as long as its schedule arena lives; the graph, which is not trivially destructible, is ended with `Zenith_Destroy` before its arena is reset.
- **Covers:** Z2-I-GRAPH-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_Graph_ScheduleOutlivesGraph`, `Test_Graph_ArenaResetWithLiveGraph_Checks` (z2-death-check) in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/GRAPH.md` → "Zenith_Graph.h"; `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Compile step list"

### Z2-R-GRAPH-049 Large graphs compile

- **Statement:** The 10,000-node, 40,000-access random graph compiles with the expected edge count and no error in Release, and `Zenith_Bench_GraphCompile` reports its time and schedule-arena peak.
- **Covers:** Z2-I-GRAPH-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphConformance_LargeGraphCompletes` in the `Test` gate step (target `ZenithGraph_Conformance`) and the `Zenith2Memory` `Sanitizers` lines; the `Zenith_Bench_GraphCompile` report
- **Design:** `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Complexity and memory summary"; `Zenith2/Docs/Design/GRAPH/Tests.md` → "Benchmarks"

### Z2-R-GRAPH-050 Deterministic compile

- **Statement:** Identical declarations, options and costs compile to byte-identical schedules and hashes.
- **Covers:** Z2-I-GRAPH-04, Z2-I-GRAPH-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphConformance_OrderIsDeterministic` in the `Test` gate step (target `ZenithGraph_Conformance`) and the `Zenith2Memory` `Sanitizers` lines
- **Design:** `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Determinism proof obligations"

### Z2-R-GRAPH-051 Lifetime intervals

- **Statement:** Each resource's lifetime is the first and last position of its non-culled accesses, both invalid when there is none.
- **Covers:** Z2-I-GRAPH-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-057
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphConformance_LifetimeIntervals` in the `Test` gate step (target `ZenithGraph_Conformance`) and the `Zenith2Memory` `Sanitizers` lines
- **Design:** `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 11 lifetimes"

### Z2-R-GRAPH-052 Culled accesses do not extend lifetimes

- **Statement:** An access of a culled node does not extend any lifetime.
- **Covers:** Z2-I-GRAPH-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphConformance_CulledAccessesExcludedFromLifetime` in the `Test` gate step (target `ZenithGraph_Conformance`) and the `Zenith2Memory` `Sanitizers` lines
- **Design:** `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Step 11 lifetimes"

### Z2-R-GRAPH-053 CPU transient scratch placement

- **Statement:** Sized CPU transients are placed first-fit in a per-instance block so that two transients share bytes only when every accessor of one reaches every accessor of the other, and wherever `ZENITH_ASSERTS_ENABLED=1` the block is filled with `0xCD` at each instance launch.
- **Covers:** Z2-I-GRAPH-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameSchedule_TransientPlacementNoOverlap`, `Test_FrameExecutor_TransientStorageReused` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Transient scratch placement"

### Z2-R-GRAPH-054 Graph dump format

- **Statement:** `WriteJson` writes `zenith2.graph-dump` version 1 with the keys in the specified order, the specified array layout per array, 64-bit values and integers at or above 2^53 as hexadecimal strings and flags as prefix-less names, byte-identical for identical inputs in `JSON_WRITE_STYLE_COMPACT`, the style every golden uses.
- **Covers:** Z2-I-GRAPH-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphConformance_JsonDumpSchema` (golden) in the `Test` gate step (target `ZenithGraph_Conformance`) and the `Zenith2Memory` `Sanitizers` lines; `Test_GraphDump_LargeIntegersAsHex` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "JSON writing"; `Zenith2/Docs/Design/GRAPH.md` → "Graph dump schema zenith2.graph-dump version 1"

### Z2-R-GRAPH-055 Dump presence rules

- **Statement:** Schedule-only and timing-only keys appear exactly under the presence rules, and culled nodes carry no position.
- **Covers:** Z2-I-GRAPH-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphDump_OptionalSections` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/GRAPH.md` → "Graph dump schema zenith2.graph-dump version 1"

### Z2-R-GRAPH-056 Committed dump schemas

- **Statement:** The two JSON Schema files are committed under `Tests/Data/`, equal the design's blocks, and every golden dump validates against them.
- **Covers:** Z2-I-GRAPH-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphDump_GoldenValidatesAgainstSchema`, `Test_GraphDump_SchemaFileMatchesDesign` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/GRAPH/Formats.md` → "Graph dump JSON Schema"; `Zenith2/Docs/Design/GRAPH/Formats.md` → "Frame dump JSON Schema"

### Z2-R-GRAPH-057 Dump version refusal message

- **Statement:** `Zenith_GraphDumpVersionIsReadable` accepts exactly the supported versions and `Zenith_GraphDumpVersionMessage` formats the refusal text readers print.
- **Covers:** Z2-I-GRAPH-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphDump_NewerVersionMessage` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "JSON writing"

### Z2-R-GRAPH-058 Front-end dumps embed the core

- **Statement:** Each CPU frame-dump document embeds the core dump of its subgraph verbatim under `core`.
- **Covers:** Z2-I-GRAPH-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameSchedule_DumpEmbedsCore` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH.md` → "Frame dump schema zenith2.frame-dump version 1"

### Z2-R-GRAPH-059 Frame dump per subgraph

- **Statement:** The CPU front end writes one frame-dump document per subgraph, with runs, affinity, transients and, from the executor, the frame and timeline, and `jobs.dump` writes the three files of the current schedule.
- **Covers:** Z2-I-GRAPH-07, Z2-I-GRAPH-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameSchedule_DumpPerSubgraph` (golden `frame_dump_tick.json`) and `Test_FrameExecutor_ConsoleCommandsDevelopmentOnly` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH.md` → "Frame dump schema zenith2.frame-dump version 1"; `Zenith2/Docs/Design/GRAPH.md` → "Console commands"

### Z2-R-GRAPH-060 Dump of a foreign schedule

- **Statement:** `WriteJson` with a schedule not compiled from the graph fails `ZENITH_CHECK`.
- **Covers:** Z2-I-GRAPH-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphDump_ForeignSchedule_Checks` (z2-death-check) in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "JSON writing"

### Z2-R-GRAPH-061 Frame-graph public headers

- **Statement:** `Zenith_FrameGraph.h`, `Zenith_FrameSchedule.h`, `Zenith_FrameExecutor.h`, `Zenith_FixedOrderReduce.h` and the GRAPH declarations of `Zenith_JobsErrors.h` declare exactly the design's listings with contract blocks.
- **Covers:** Z2-I-GRAPH-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-047
- **Budgets:** none
- **Acceptance evidence:** the `HeaderSelfContained` and `ApiContractCheck` gate steps; `Test_FrameGraph_Example` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/GRAPH.md` → "Zenith_FrameGraph.h"; `Zenith2/Docs/Design/GRAPH/PublicAPI.md` → "Zenith_FrameSchedule.h listing"; `Zenith2/Docs/Design/GRAPH.md` → "Zenith_FrameExecutor.h"

### Z2-R-GRAPH-062 Declaration validation

- **Statement:** Every malformed frame-graph declaration listed by the design is `FRAME_GRAPH_ERROR_INVALID_DECLARATION` with its reason text.
- **Covers:** Z2-I-GRAPH-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameGraph_InvalidDeclaration_IsError` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Step 0 validation"; `Zenith2/Docs/Design/GRAPH.md` → "Error codes"

### Z2-R-GRAPH-063 Duplicate node names

- **Statement:** Two nodes of one (subgraph, group) with one name are `FRAME_GRAPH_ERROR_DUPLICATE_NODE_NAME`; equal names in different groups are legal.
- **Covers:** Z2-I-GRAPH-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameGraph_DuplicateNodeName_IsError` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Step 1 node names and qualified references"

### Z2-R-GRAPH-064 Ordering name resolution

- **Statement:** Ordering targets resolve by plain, `@W<tag>` and `@W*` names within the declaring subgraph; a missing required target is `FRAME_GRAPH_ERROR_ORDERING_TARGET_MISSING` and a missing optional one is ignored.
- **Covers:** Z2-I-GRAPH-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameGraph_AfterIfPresent_IgnoresMissing`, `Test_FrameGraph_QualifiedNameResolvesAcrossGroups`, `Test_FrameGraph_WildcardOrdersEveryWorld` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Step 1 node names and qualified references"

### Z2-R-GRAPH-065 Stage fences

- **Statement:** Consecutive stages of a (subgraph, group) are ordered through a generated fence that runs the declared stage barrier or nothing, and a second barrier for one stage is `FRAME_GRAPH_ERROR_DUPLICATE_STAGE_BARRIER`.
- **Covers:** Z2-I-GRAPH-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameGraph_StageBarrier_OrdersStages`, `Test_FrameGraph_FenceWithoutBarrierIsEmpty`, `Test_FrameGraph_DuplicateStageBarrier_IsError` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Step 3 stage fences"

### Z2-R-GRAPH-066 Orderings may not contradict stages

- **Statement:** A `Before` into a preceding stage or an `After` into a subsequent stage of the same group is `FRAME_GRAPH_ERROR_ORDERING_CONTRADICTS_STAGE` with the specified message.
- **Covers:** Z2-I-GRAPH-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameGraph_OrderingContradictsStage_IsError` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Step 4 ordering resolution"

### Z2-R-GRAPH-067 Version threading within a stage

- **Statement:** A stage's readers read the version its writer produces, except readers ordered before the writer, which read the prior version.
- **Covers:** Z2-I-GRAPH-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameGraph_ReaderAfterWriterWithinStage`, `Test_FrameGraph_BeforeFlipsReaderToPreWrite` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Step 5 version threading"

### Z2-R-GRAPH-068 Unordered writers

- **Statement:** Two writers of one frame resource in one stage are `FRAME_GRAPH_ERROR_UNORDERED_WRITERS`, naming every writer.
- **Covers:** Z2-I-GRAPH-08, Z2-I-GRAPH-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameGraph_UnorderedWriters_IsError` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Step 5 version threading"

### Z2-R-GRAPH-069 Resource keys and groups

- **Statement:** Resource keys are unique per builder, and a node accessing a resource of another group is `FRAME_GRAPH_ERROR_CROSS_GROUP_ACCESS`.
- **Covers:** Z2-I-GRAPH-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameGraph_DuplicateResourceKey_IsError`, `Test_FrameGraph_CrossGroupAccess_IsError` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Step 2 resource keys and groups"

### Z2-R-GRAPH-070 Worlds share no edges

- **Statement:** Nodes of different groups share no resource and no generated ordering, so worlds run concurrently.
- **Covers:** Z2-I-GRAPH-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameGraph_WorldsShareNoEdges` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Step 2 resource keys and groups"; `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Step 3 stage fences"

### Z2-R-GRAPH-071 Canonical lowering

- **Statement:** Lowering orders nodes by (group, stage, declaration index), names world nodes `<name>@W<group>`, maps persistent resources to imported ones and transients to unflagged ones, and uses lanes equal to affinity.
- **Covers:** Z2-I-GRAPH-08, Z2-I-GRAPH-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameGraph_LoweringCanonicalOrder` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Step 6 lowering into ZenithGraph"

### Z2-R-GRAPH-072 Core errors are wrapped unchanged

- **Statement:** Every core diagnostic reaches the caller as a `FRAME_GRAPH_ERROR_GRAPH` diagnostic, returned by the builder's `GetDiagnostics()` until the builder is destroyed, with the core text unchanged after the `frame graph: <Subgraph>: ` prefix, the core code in `m_uCoreCode` and builder node indices, sorted with every jobs-level error of the compile.
- **Covers:** Z2-I-GRAPH-08, Z2-I-GRAPH-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameGraph_CoreErrorWrappedUnchanged` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Step 7 compile and wrap"; `Zenith2/Docs/Design/GRAPH.md` → "Front-end error mapping"

### Z2-R-GRAPH-073 Schedule assembly from core graphs

- **Statement:** `Zenith_AssembleFrameSchedule` emits a frame schedule from up to three core graphs, taking ownership of their arena, with results equal to the core compile without culling, and writes any diagnostics into the caller's `Zenith_FrameGraphDiagnosticReport`.
- **Covers:** Z2-I-GRAPH-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameSchedule_AssembleFromCoreGraphs` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Schedule emission"; `Zenith2/Docs/Design/GRAPH/PublicAPI.md` → "Zenith_FrameSchedule.h listing"

### Z2-R-GRAPH-074 Run tables and affinity counts

- **Statement:** Emission builds a run table and a presorted root list per subgraph, affinity counts summed over the subgraphs with the tick counted once, and the injected-node count (RENDER ANY nodes with a render-submit predecessor).
- **Covers:** Z2-I-GRAPH-08, Z2-I-GRAPH-11
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameSchedule_AffinityCounts`, `Test_FrameSchedule_InjectedNodeCount` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Run tables"; `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Affinity counts and queue sizing"

### Z2-R-GRAPH-075 Schedules are not recomputed per frame

- **Statement:** Each frame executes the compiled schedule without compiling or reprioritising anything.
- **Covers:** Z2-I-GRAPH-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameExecutor_NoCompileDuringRun` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "RunFrame step by step"

### Z2-R-GRAPH-076 Schedule cache

- **Statement:** The cache keys structures by the combined structure hash confirmed by comparing the three per-subgraph canonical streams pairwise, rebinds run tables on a hit, compiles on a miss or a collision, and holds at most its capacity of unbound structures.
- **Covers:** Z2-I-GRAPH-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameGraph_ScheduleCache_HitsWithoutRecompile`, `Test_FrameScheduleCache_HitRebindsRunTable`, `Test_FrameScheduleCache_HashCollisionFallsBackToCompile` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Acquire"; `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Structures and bindings"; `Zenith2/Docs/Design/GRAPH/PublicAPI.md` → "Zenith_FrameSchedule.h listing"

### Z2-R-GRAPH-077 Recompile on triggers

- **Statement:** A recompile trigger leads to one rebuild and acquire in the frame loop, applied at the start of the next `RunFrame`.
- **Covers:** Z2-I-GRAPH-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameGraph_Recompile_OnWorldCreated` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Cache invalidation triggers"

### Z2-R-GRAPH-078 Schedule swap and release

- **Statement:** `RunFrame` swaps a pending schedule in under `uLOCK_LEVEL_JOBS_SCHEDULE` before any node of the frame runs, and the cache releases a schedule only when the executor no longer reports it in use.
- **Covers:** Z2-I-GRAPH-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameExecutor_SchedulesInUse` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Release at iteration end"; `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "RunFrame step by step"

### Z2-R-GRAPH-079 Failed recompiles keep the previous schedule

- **Statement:** A failed acquire changes no cached structure, binding or current schedule, returns the first code and the count, and leaves every diagnostic readable through the builder's `GetDiagnostics()`; a failed first compile stops startup with exit code 3.
- **Covers:** Z2-I-GRAPH-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-081
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameGraph_RecompileFailure_KeepsPreviousSchedule` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH.md` → "Compile failure on recompile"; `Zenith2/Docs/Design/GRAPH.md` → "Compile failure at startup"

### Z2-R-GRAPH-080 Reprioritisation from measured costs

- **Statement:** The executor keeps a per-node cost EWMA and reports reprioritisation due every `m_uReprioritiseIntervalFrames` frames, and the cache's reprioritisation changes only priorities, order, positions, lifetimes and placement.
- **Covers:** Z2-I-GRAPH-08, Z2-I-GRAPH-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameExecutor_CostEwma`, `Test_FrameScheduleCache_ReprioritiseKeepsStructure` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Reprioritise"; `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Timing capture and cost measurement"

### Z2-R-GRAPH-081 Disabled nodes need no recompile

- **Statement:** A disabled system's node stays in the schedule and completes without running its body, with no recompile.
- **Covers:** Z2-I-GRAPH-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameGraph_DisabledNode_SkipsWithoutRecompile` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Cache invalidation triggers"

### Z2-R-GRAPH-082 Schedule recompile measured

- **Statement:** The recompile runs inside the zone `Frame.ScheduleRecompile`, and its time is reported against Z2-B-GRAPH-001 in the workloads that exist at the Stage 2 budget confirmation.
- **Covers:** Z2-I-GRAPH-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-012
- **Budgets:** Z2-B-GRAPH-001
- **Acceptance evidence:** the Stage 2 evidence record of the harness run on Windows and the S24 under `Zenith2/Docs/Research/`
- **Design:** `Zenith2/Docs/Design/GRAPH.md` → "Frame-graph nodes this area runs"; `Zenith2/Docs/Design/GRAPH.md` → "Rows this area owns"

### Z2-R-GRAPH-083 Schedule recompile within its row

- **Statement:** The schedule recompile meets Z2-B-GRAPH-001 on W-OpenWorld and W-Editor on Windows and the S24.
- **Covers:** Z2-I-GRAPH-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-012
- **Budgets:** Z2-B-GRAPH-001
- **Acceptance evidence:** the Stage 4 acceptance evidence records of MP-CPUZONE over `Frame.ScheduleRecompile` (Run-Committed.ps1 -Acceptance -Stage 4)
- **Design:** `Zenith2/Docs/Design/GRAPH.md` → "Rows this area owns"; `Zenith2/Docs/Design/GRAPH.md` → "Stages 3 to 7"

### Z2-R-GRAPH-084 Sub-jobs join before successors

- **Statement:** A node completes only after its body, every sub-job it spawned and every continuation it registered have finished, and its successors observe all of their writes.
- **Covers:** Z2-I-GRAPH-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameGraph_SuccessorSeesSubJobWrites` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Node completion"; `Zenith2/Docs/Design/GRAPH.md` → "The executor to job-system seam"

### Z2-R-GRAPH-085 Chunk fan-out is unobservable

- **Statement:** The chunk-to-worker assignment of a node's `ParallelFor` never changes a result merged by index.
- **Covers:** Z2-I-GRAPH-09, Z2-I-GRAPH-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameGraph_ChunkFanOut_ResultIndependentOfWorkers` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Determinism proof obligations"

### Z2-R-GRAPH-086 Continuations complete before successors

- **Statement:** A node's final continuation, its deterministic merge step, runs before any successor of the node starts.
- **Covers:** Z2-I-GRAPH-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameGraph_MergeContinuationBeforeSuccessor` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Node completion"

### Z2-R-GRAPH-087 No blocking waits and no fibers

- **Statement:** No GRAPH code path waits inside a job and none uses a fiber or user-mode context; the main thread parks only inside `Zenith_JobsRunMainUntil` when nothing it may run is visible.
- **Covers:** Z2-I-GRAPH-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameGraph_NoBlockingWaitInNodes` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan); review of every GRAPH diff for fiber or context-switch APIs
- **Design:** `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "The node trampoline"; `Zenith2/Docs/Design/GRAPH.md` → "What this area does not own"

### Z2-R-GRAPH-088 Main-thread affinity

- **Statement:** Main-affinity nodes run with worker index 0, which the trampoline checks.
- **Covers:** Z2-I-GRAPH-11
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameGraph_MainAffinity_RunsOnMainThread` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "The node trampoline"

### Z2-R-GRAPH-089 Render-submit affinity

- **Statement:** Render-submit nodes run on the render-submit thread (worker index N+1), which the trampoline checks.
- **Covers:** Z2-I-GRAPH-11
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameGraph_RenderSubmitAffinity_RunsOnSubmitThread` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "The node trampoline"

### Z2-R-GRAPH-090 Render-submit affinity only in the render subgraph

- **Statement:** A node with render-submit affinity outside `FRAME_SUBGRAPH_RENDER` is `FRAME_GRAPH_ERROR_INVALID_DECLARATION`.
- **Covers:** Z2-I-GRAPH-11
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameGraph_RenderSubmitOutsideRender_IsError` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Step 0 validation"

### Z2-R-GRAPH-091 Affinity queues never fill

- **Statement:** `SetSchedule` reserves the affinity queues from the affinity node counts and the injection queue from the injected-node count (`Zenith_JobsReserveInjectionCapacity`), so no fatal full policy of those queues is ever reached.
- **Covers:** Z2-I-GRAPH-11
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameSchedule_AffinityNodeCountBoundsQueue` (z2-stress), `Test_FrameSchedule_InjectedNodeCount` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Affinity counts and queue sizing"; `Zenith2/Docs/Design/GRAPH.md` → "Bounded queues and full policies"

### Z2-R-GRAPH-092 Highest priority taken first

- **Statement:** Every push batch is sorted by priority descending and index ascending, so among nodes made ready together the consumer takes the highest-priority one first.
- **Covers:** Z2-I-GRAPH-11, Z2-I-GRAPH-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameExecutor_HighestPriorityReadyTakenFirst` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Push order and priority"

### Z2-R-GRAPH-093 Subgraph sequence

- **Statement:** `RunFrame` runs the tick instances and then the epilogue, with the render subgraph of the previous frame concurrently; the frame prologue runs on the main thread before `RunFrame` and is not an executor subgraph; the iteration in which shutdown is requested is the last, and no instance is launched after it returns.
- **Covers:** Z2-I-GRAPH-12
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameExecutor_SubgraphSequence`, `Test_FrameGraph_PrologueRunsNoJobs` (GRAPH half), `Test_FrameExecutor_ShutdownDrainsFrame` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Frame executor iteration state machine"; `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Instance completion and sequencing"

### Z2-R-GRAPH-094 Tick instances are sequential

- **Statement:** Tick instance t+1 starts after every node of tick instance t, and a frame with no tick still runs its epilogue once.
- **Covers:** Z2-I-GRAPH-12
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameExecutor_TicksSequential`, `Test_FrameExecutor_ZeroTicksStillRunsEpilogue` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Instance completion and sequencing"

### Z2-R-GRAPH-095 Render overlaps simulation

- **Statement:** RENDER(k−1) runs concurrently with the simulation of frame k with no edge between them.
- **Covers:** Z2-I-GRAPH-12
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** Z2-B-RPROD-021
- **Acceptance evidence:** `Test_FrameGraph_PipelinedRender_OverlapsSimulation` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "The iteration barrier"

### Z2-R-GRAPH-096 Iteration barrier

- **Statement:** `RunFrame` returns only when the epilogue and the render part are both done, so simulation never runs more than one frame ahead of rendering.
- **Covers:** Z2-I-GRAPH-12
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** Z2-B-RPROD-021
- **Acceptance evidence:** `Test_FrameGraph_SimulationNeverTwoFramesAhead` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "The iteration barrier"; `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Hand-off to Flux_RenderWorld"

### Z2-R-GRAPH-097 Pipeline depth telemetry

- **Statement:** Every frame publishes `pipeline.depth`, 1 when the render part ran and 0 otherwise.
- **Covers:** Z2-I-GRAPH-12
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** Z2-B-RPROD-021
- **Acceptance evidence:** `Test_FrameExecutor_PipelineDepthCounter` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Frame timeline and latency stamps"; `Zenith2/Docs/Design/GRAPH.md` → "Counters"

### Z2-R-GRAPH-098 Frame timeline

- **Statement:** Every frame publishes a `Zenith_FrameTimeline` whose stamps satisfy the specified orderings, for the input-latency attribution.
- **Covers:** Z2-I-GRAPH-12
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** Z2-B-RPROD-027
- **Acceptance evidence:** `Test_FrameExecutor_TimelineStampsOrdered` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Frame timeline and latency stamps"

### Z2-R-GRAPH-099 No render when disabled

- **Statement:** With `m_bRunRender` false, or before the first epilogue, no render instance runs and the pipeline depth is 0.
- **Covers:** Z2-I-GRAPH-12
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameExecutor_NoRenderWhenDisabled` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Headless and first-frame behaviour"

### Z2-R-GRAPH-100 Render runs the epilogue schedule

- **Statement:** RENDER(k−1) runs the render subgraph of the schedule whose epilogue ran in frame k−1, across any schedule swap.
- **Covers:** Z2-I-GRAPH-12
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameExecutor_RenderUsesEpilogueSchedule` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Hand-off to Flux_RenderWorld"

### Z2-R-GRAPH-101 Schedules in use are reported

- **Statement:** `GetSchedulesInUse` reports the current, the pending and the render schedule, so the cache never releases a schedule the executor may still run.
- **Covers:** Z2-I-GRAPH-12, Z2-I-GRAPH-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameExecutor_SchedulesInUse` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Release at iteration end"

### Z2-R-GRAPH-102 Priorities never change results

- **Statement:** Results are identical under any measured-cost table, because costs change only wall-clock order.
- **Covers:** Z2-I-GRAPH-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameGraph_Determinism_CostPerturbation` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Determinism proof obligations"

### Z2-R-GRAPH-103 Results independent of worker count

- **Statement:** A seeded random frame graph produces identical final state with 1, 2, 8 and all workers.
- **Covers:** Z2-I-GRAPH-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameGraph_Determinism_WorkerCounts` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Determinism proof obligations"

### Z2-R-GRAPH-104 Deterministic invocation IDs

- **Statement:** Invocation IDs follow the design's formula with bits 62 and 63 clear and are identical on every run with the same frame and tick sequence, and `RunFrame` refuses a tick count above 253, the bound of the 8-bit instance ordinal.
- **Covers:** Z2-I-GRAPH-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameExecutor_InvocationIDsDeterministic`, `Test_FrameExecutor_InvocationOrdinalBound` (z2-death-check) in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Invocation IDs"; `Zenith2/Docs/Design/GRAPH.md` → "Rows this area is measured against"

### Z2-R-GRAPH-105 Fixed-order reduction

- **Statement:** `Zenith_FixedOrderReduceSerial` and `Zenith_FixedOrderReduceParallel` reduce in the level-wise left-to-right pair tree and produce identical bytes for any worker count.
- **Covers:** Z2-I-GRAPH-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_FixedOrderReduce_MatchesSerialTree`, `Test_FixedOrderReduce_IdenticalAcrossWorkerCounts` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Fixed-order reduction"; `Zenith2/Docs/Design/GRAPH.md` → "Zenith_FixedOrderReduce.h"

### Z2-R-GRAPH-106 Float environment asserted per node

- **Statement:** The trampoline asserts the fixed floating-point environment at the start of every node.
- **Covers:** Z2-I-GRAPH-13, Z2-I-GRAPH-11
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameExecutor_FloatEnvironmentAssertedPerNode` (z2-death-assert) in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "The node trampoline"

### Z2-R-GRAPH-107 Conformance suite layout

- **Statement:** The scenarios, the builder interface, the comparator and the core builder live in `ZenithGraph/Tests/Conformance/`, the CPU builder in `ZenithJobs/Tests/GraphConformance/` and the Flux builder in `ZenithFlux/Tests/GraphConformance/`, all in `ZenithGraph_Conformance`.
- **Covers:** Z2-I-GRAPH-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-055
- **Budgets:** none
- **Acceptance evidence:** the `SourceLayout` and `TestSeparationCheck` gate steps; the CTest listing of `ZenithGraph_Conformance`
- **Design:** `Zenith2/Docs/Design/GRAPH/Tests.md` → "Test files"; `Zenith2/Docs/Design/GRAPH/Tests.md` → "Scenario model"

### Z2-R-GRAPH-108 Scenarios run through every builder

- **Statement:** Every scenario runs through each builder in its mask and each front end's result equals the core's compiled with that front end's culling option, in edges, order, counts, priorities, lifetimes and diagnostics.
- **Covers:** Z2-I-GRAPH-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-055
- **Budgets:** none
- **Acceptance evidence:** the `[Core]`, `[Cpu]` and `[Flux]` cases of the `Test` gate step (target `ZenithGraph_Conformance`) and the `Zenith2Memory` `Sanitizers` lines
- **Design:** `Zenith2/Docs/Design/GRAPH/Tests.md` → "Comparison rules"; `Zenith2/Docs/Design/GRAPH/Tests.md` → "Expectations per builder"

### Z2-R-GRAPH-109 CPU builder checks execution

- **Statement:** The CPU builder runs each compiled scenario on a four-worker job system for 20 frames and fails when any kept edge's source ends after its destination starts.
- **Covers:** Z2-I-GRAPH-14, Z2-I-GRAPH-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** the `[Cpu]` cases of the `Test` gate step (target `ZenithGraph_Conformance`) and the `Zenith2Memory` `Sanitizers` lines
- **Design:** `Zenith2/Docs/Design/GRAPH/Tests.md` → "Builders"

### Z2-R-GRAPH-110 Flux builder

- **Statement:** The Flux builder realises every scenario in its mask through the render graph by the design's mapping, compiling without a device.
- **Covers:** Z2-I-GRAPH-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-055
- **Budgets:** none
- **Acceptance evidence:** the `[Flux]` cases of the `Test` gate step (target `ZenithGraph_Conformance`) and the `Zenith2Memory` `Sanitizers` lines in win-debug and win-release
- **Design:** `Zenith2/Docs/Design/GRAPH/Tests.md` → "The Flux builder mapping"

### Z2-R-GRAPH-111 Conformance runs in the gates

- **Statement:** The graph conformance suite runs in the `Test` step on win-debug and win-release and under win-asan and linux-tsan when the `Zenith2Memory` sanitizer lines run, without the Flux builder on linux-tsan.
- **Covers:** Z2-I-GRAPH-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** the JUnit reports of `ZenithGraph_Conformance` from the `Test` gate step (target `ZenithGraph_Conformance`) and the `Zenith2Memory` `Sanitizers` lines
- **Design:** `Zenith2/Docs/Design/GRAPH.md` → "Test targets and gate steps"; `Zenith2/Docs/Design/GRAPH.md` → "Supported matrix"

### Z2-R-GRAPH-112 Scenario table complete

- **Statement:** Every test of the scenario table exists under its name and asserts its case.
- **Covers:** Z2-I-GRAPH-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** the CTest listing of `ZenithGraph_Conformance`, reviewed against the scenario table
- **Design:** `Zenith2/Docs/Design/GRAPH/Tests.md` → "Scenario table"

### Z2-R-GRAPH-113 Executor on the main thread

- **Statement:** Every executor member runs on the main thread except `GetTransientStorage` and `RequestCancel`, and the main thread helps with frame work inside `RunFrame`.
- **Covers:** Z2-I-GRAPH-11, Z2-I-GRAPH-12
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-081
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameExecutor_MainThreadHelps` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH.md` → "Thread affinity per type"; `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "RunFrame step by step"

### Z2-R-GRAPH-114 One lock, at the specified level

- **Statement:** GRAPH takes only the schedule mutex at `uLOCK_LEVEL_JOBS_SCHEDULE`, on the main thread, never from a job.
- **Covers:** Z2-I-GRAPH-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** the debug lock-order checker in every GRAPH test run under the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan); review
- **Design:** `Zenith2/Docs/Design/GRAPH.md` → "Lock-order entries"

### Z2-R-GRAPH-115 Cancelled frames complete

- **Statement:** After `RequestCancel` every node of the frame completes, bodies not yet started are skipped, no further tick instance starts, `RunFrame` returns, and the next frame runs normally; a frame whose epilogue completed under a set cancel flag is never rendered (no RENDER of it in the next iteration, pipeline depth 0), and after any cancelled iteration the next epilogue that seals performs a full-state render-world extraction (`IsRenderWorldResyncPending`).
- **Covers:** Z2-I-GRAPH-12, Z2-I-GRAPH-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameGraph_CancelledInstance_Completes`, `Test_FrameExecutor_CancelledFrameIsNotRendered`, `Test_FrameExecutor_CancelResyncsRenderWorld`, `Test_FrameExecutor_CancelStress` (z2-stress) in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Cancellation path"; `Zenith2/Docs/Design/GRAPH.md` → "Cancellation"

### Z2-R-GRAPH-116 Orderly shutdown

- **Statement:** Shutdown takes effect when the current iteration returns, with no further `RunFrame` and the in-flight render world discarded, destroys the executor before the job system and the cache after the executor, and destroying the executor during a frame fails `ZENITH_CHECK`.
- **Covers:** Z2-I-GRAPH-12
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameExecutor_ShutdownDrainsFrame`, `Test_FrameExecutor_DestroyDuringRun_Checks` (z2-death-check) in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH.md` → "Shutdown"

### Z2-R-GRAPH-117 Factories unwind on failure

- **Statement:** `Zenith_FrameExecutor::Create` and `Zenith_FrameScheduleCache::Create` unwind exactly what they built when any step fails, and every step has a failure-injection test.
- **Covers:** Z2-I-GRAPH-08, Z2-I-GRAPH-12
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045, Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameExecutor_Create_FailsAtObject_Unwinds`, `Test_FrameExecutor_Create_FailsAtIterationArena_Unwinds`, `Test_FrameExecutor_Create_FailsAtTimingRing_Unwinds`, `Test_FrameExecutor_Create_FailsAtCostBuffers_Unwinds`, `Test_FrameExecutor_Create_FailsAtConsoleRegistration_Unwinds`, `Test_FrameScheduleCache_Create_FailsAtObject_Unwinds`, `Test_FrameScheduleCache_Create_FailsAtRootTable_Unwinds`, `Test_FrameScheduleCache_Create_FailsAtBindingTable_Unwinds` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH.md` → "Allocation failure in the cache and executor"; `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Executor storage"

### Z2-R-GRAPH-118 Profiling zones

- **Statement:** Every executed node emits a zone named by its core name, fences by their generated names, and the recompile and compile emit their literal zones.
- **Covers:** Z2-I-GRAPH-08, Z2-I-GRAPH-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameExecutor_ZoneNames` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH.md` → "Profiling zones"; `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Zone sites"

### Z2-R-GRAPH-119 Counters published every frame

- **Statement:** The executor and the cache publish every counter of the design's table every frame.
- **Covers:** Z2-I-GRAPH-08, Z2-I-GRAPH-12
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-005
- **Budgets:** Z2-B-JOBS-001
- **Acceptance evidence:** `Test_FrameExecutor_CountersPublished` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH.md` → "Counters"

### Z2-R-GRAPH-120 Log messages

- **Statement:** Compiles, hits, failures, reprioritisations and skipped reductions log the design's messages, the skipped reduction once per graph name.
- **Covers:** Z2-I-GRAPH-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameScheduleCache_ReductionSkippedLoggedOnce` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH.md` → "Log categories and messages"

### Z2-R-GRAPH-121 Console commands in development builds

- **Statement:** `jobs.recompile` and `jobs.dump <path>` exist in development builds only.
- **Covers:** Z2-I-GRAPH-07, Z2-I-GRAPH-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameExecutor_ConsoleCommandsDevelopmentOnly` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH.md` → "Console commands"

### Z2-R-GRAPH-122 Bounded executor and cache structures

- **Statement:** The iteration arena, the timing ring, the pending schedule slot and the cache are bounded with the design's capacities and full policies and count their full events.
- **Covers:** Z2-I-GRAPH-08, Z2-I-GRAPH-12
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameExecutor_TimingRingDropsOldest`, `Test_FrameScheduleCache_EvictsLeastRecentlyUsed` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH.md` → "Bounded queues and full policies"; `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Executor storage"

### Z2-R-GRAPH-123 Stress under ThreadSanitizer

- **Statement:** Ten thousand frames of a 200-node random graph with sub-jobs run clean under linux-tsan with 2, 8 and all workers.
- **Covers:** Z2-I-GRAPH-09, Z2-I-GRAPH-10, Z2-I-GRAPH-12
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameGraph_StressUnderTsan` (z2-stress) in the `Sanitizers -Preset linux-tsan` line
- **Design:** `Zenith2/Docs/Design/GRAPH.md` → "Sanitizer and stress runs"; `Zenith2/Docs/Design/GRAPH/Tests.md` → "Stress and sanitizer tests"

### Z2-R-GRAPH-124 Frame-graph message formats

- **Statement:** Every frame-graph error code produces exactly the text of the design's frame-graph error table.
- **Covers:** Z2-I-GRAPH-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameGraph_DiagnosticMessageFormats` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH.md` → "Error codes"

### Z2-R-GRAPH-125 Frame-graph error enumeration

- **Statement:** `Zenith_FrameGraphErrorCode` keeps the specification's five codes first, numbers every enumerator from 1, and has a `Zenith_ToString`, and `Zenith_FrameGraphError` is the 16-byte span-free record `{ m_eCode, m_uDiagnosticCount, m_ulStructureHash }`, its texts read through `Zenith_FrameGraphBuilder::GetDiagnostics()`.
- **Covers:** Z2-I-GRAPH-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameGraph_ErrorCodeToString` in the `Test` gate step and the `Zenith2Memory` `Sanitizers` lines (win-asan, linux-tsan)
- **Design:** `Zenith2/Docs/Design/GRAPH.md` → "Frame-graph errors in Zenith_JobsErrors.h"

## Coverage of inventory items

| Item | Text | Requirements |
|---|---|---|
| Z2-I-GRAPH-01 | `ZenithGraph`: access declarations | Z2-R-GRAPH-001, Z2-R-GRAPH-004, Z2-R-GRAPH-005, Z2-R-GRAPH-006, Z2-R-GRAPH-007, Z2-R-GRAPH-009, Z2-R-GRAPH-010, Z2-R-GRAPH-011, Z2-R-GRAPH-012, Z2-R-GRAPH-013, Z2-R-GRAPH-014, Z2-R-GRAPH-015, Z2-R-GRAPH-016, Z2-R-GRAPH-017 |
| Z2-I-GRAPH-02 | `ZenithGraph`: hazard edges | Z2-R-GRAPH-001, Z2-R-GRAPH-020, Z2-R-GRAPH-021, Z2-R-GRAPH-022, Z2-R-GRAPH-023, Z2-R-GRAPH-024, Z2-R-GRAPH-025, Z2-R-GRAPH-026, Z2-R-GRAPH-027, Z2-R-GRAPH-028, Z2-R-GRAPH-029, Z2-R-GRAPH-030, Z2-R-GRAPH-031, Z2-R-GRAPH-032 |
| Z2-I-GRAPH-03 | `ZenithGraph`: versioned handles | Z2-R-GRAPH-004, Z2-R-GRAPH-006, Z2-R-GRAPH-008, Z2-R-GRAPH-018, Z2-R-GRAPH-019, Z2-R-GRAPH-020, Z2-R-GRAPH-021 |
| Z2-I-GRAPH-04 | `ZenithGraph`: topological sort with critical-path priority | Z2-R-GRAPH-017, Z2-R-GRAPH-033, Z2-R-GRAPH-034, Z2-R-GRAPH-035, Z2-R-GRAPH-042, Z2-R-GRAPH-043, Z2-R-GRAPH-044, Z2-R-GRAPH-045, Z2-R-GRAPH-046, Z2-R-GRAPH-047, Z2-R-GRAPH-048, Z2-R-GRAPH-049, Z2-R-GRAPH-050, Z2-R-GRAPH-080, Z2-R-GRAPH-092 |
| Z2-I-GRAPH-05 | `ZenithGraph`: cycle diagnostics | Z2-R-GRAPH-036, Z2-R-GRAPH-037, Z2-R-GRAPH-038, Z2-R-GRAPH-039, Z2-R-GRAPH-040, Z2-R-GRAPH-041, Z2-R-GRAPH-072 |
| Z2-I-GRAPH-06 | `ZenithGraph`: lifetime intervals | Z2-R-GRAPH-051, Z2-R-GRAPH-052, Z2-R-GRAPH-053 |
| Z2-I-GRAPH-07 | `ZenithGraph`: JSON dump | Z2-R-GRAPH-054, Z2-R-GRAPH-055, Z2-R-GRAPH-056, Z2-R-GRAPH-057, Z2-R-GRAPH-058, Z2-R-GRAPH-059, Z2-R-GRAPH-060, Z2-R-GRAPH-121 |
| Z2-I-GRAPH-08 | CPU frame graph: compiled schedule | Z2-R-GRAPH-001, Z2-R-GRAPH-002, Z2-R-GRAPH-003, Z2-R-GRAPH-035, Z2-R-GRAPH-059, Z2-R-GRAPH-061, Z2-R-GRAPH-062, Z2-R-GRAPH-063, Z2-R-GRAPH-064, Z2-R-GRAPH-065, Z2-R-GRAPH-066, Z2-R-GRAPH-067, Z2-R-GRAPH-068, Z2-R-GRAPH-069, Z2-R-GRAPH-070, Z2-R-GRAPH-071, Z2-R-GRAPH-072, Z2-R-GRAPH-073, Z2-R-GRAPH-074, Z2-R-GRAPH-075, Z2-R-GRAPH-076, Z2-R-GRAPH-077, Z2-R-GRAPH-078, Z2-R-GRAPH-079, Z2-R-GRAPH-080, Z2-R-GRAPH-081, Z2-R-GRAPH-082, Z2-R-GRAPH-083, Z2-R-GRAPH-101, Z2-R-GRAPH-114, Z2-R-GRAPH-117, Z2-R-GRAPH-118, Z2-R-GRAPH-119, Z2-R-GRAPH-120, Z2-R-GRAPH-121, Z2-R-GRAPH-122, Z2-R-GRAPH-124, Z2-R-GRAPH-125 |
| Z2-I-GRAPH-09 | CPU frame graph: dynamic sub-jobs | Z2-R-GRAPH-001, Z2-R-GRAPH-084, Z2-R-GRAPH-085, Z2-R-GRAPH-109, Z2-R-GRAPH-118, Z2-R-GRAPH-123 |
| Z2-I-GRAPH-10 | CPU frame graph: continuations with no fibers | Z2-R-GRAPH-086, Z2-R-GRAPH-087, Z2-R-GRAPH-115, Z2-R-GRAPH-123 |
| Z2-I-GRAPH-11 | CPU frame graph: affinity | Z2-R-GRAPH-074, Z2-R-GRAPH-088, Z2-R-GRAPH-089, Z2-R-GRAPH-090, Z2-R-GRAPH-091, Z2-R-GRAPH-092, Z2-R-GRAPH-106, Z2-R-GRAPH-113 |
| Z2-I-GRAPH-12 | CPU frame graph: render-extraction pipelining | Z2-R-GRAPH-001, Z2-R-GRAPH-093, Z2-R-GRAPH-094, Z2-R-GRAPH-095, Z2-R-GRAPH-096, Z2-R-GRAPH-097, Z2-R-GRAPH-098, Z2-R-GRAPH-099, Z2-R-GRAPH-100, Z2-R-GRAPH-101, Z2-R-GRAPH-113, Z2-R-GRAPH-115, Z2-R-GRAPH-116, Z2-R-GRAPH-117, Z2-R-GRAPH-119, Z2-R-GRAPH-122, Z2-R-GRAPH-123 |
| Z2-I-GRAPH-13 | CPU frame graph: deterministic ordering | Z2-R-GRAPH-031, Z2-R-GRAPH-050, Z2-R-GRAPH-068, Z2-R-GRAPH-071, Z2-R-GRAPH-085, Z2-R-GRAPH-102, Z2-R-GRAPH-103, Z2-R-GRAPH-104, Z2-R-GRAPH-105, Z2-R-GRAPH-106 |
| Z2-I-GRAPH-14 | a graph conformance suite shared with Flux | Z2-R-GRAPH-107, Z2-R-GRAPH-108, Z2-R-GRAPH-109, Z2-R-GRAPH-110, Z2-R-GRAPH-111, Z2-R-GRAPH-112 |

## Excluded and deferred

No GRAPH inventory item is excluded or deferred: all fourteen are required in Stage 2, and the area has no Stage 7 capability. Two capabilities adjacent to the area are excluded or owned elsewhere, each with its decision:

| Capability | Classification | Decision | Owner |
|---|---|---|---|
| fibers or any user-mode context switching in the job graph | excluded | Z2-D-006 | none (Z2-R-GRAPH-087 enforces the absence) |
| the job system, the simulation clock, background jobs | required, owned by JOBS | Z2-D-006 | JOBS (`Zenith2/Docs/Requirements/JOBS.md`) |
| `Flux_RenderWorld` and its ring | required, owned by RPROD | Z2-D-050 | RPROD |
