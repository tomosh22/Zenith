# GRAPH design — tests

This file is part of the GRAPH design family; the entry document is `Zenith2/Docs/Design/GRAPH.md`. It lists every test of the area with the case it asserts, the file it lives in, the target and gate step that runs it, and its owner where a specification table names a test another area implements. Test names follow `Zenith2/Docs/Contracts.md` → "Tests and code hygiene gates" (test ID `Test_<Type>_<Behaviour>`, doctest name `"<Library>/<Type>/<Behaviour>"`). Every test prints the seed of any randomness it uses. Stress tests carry the doctest suite `z2-stress`; death tests carry `z2-death-check` or `z2-death-assert`.

## Test files

| File | Target | Holds |
|---|---|---|
| `Zenith2/Engine/ZenithGraph/Tests/Zenith_Graph_Tests.cpp` | `ZenithGraph_Tests` | `Test_Graph_*` |
| `Zenith2/Engine/ZenithGraph/Tests/Zenith_Graph_DefectTests.cpp` | `ZenithGraph_Tests` | the death tests of `Zenith_Graph` |
| `Zenith2/Engine/ZenithGraph/Tests/Zenith_GraphDiagnostics_Tests.cpp` | `ZenithGraph_Tests` | `Test_GraphDiagnostics_*` |
| `Zenith2/Engine/ZenithGraph/Tests/Zenith_GraphDump_Tests.cpp` | `ZenithGraph_Tests` | `Test_GraphDump_*` |
| `Zenith2/Engine/ZenithGraph/Tests/Examples/Zenith_Graph_Example.cpp` | `ZenithGraph_Tests` | the compiled example the contract block cites |
| `Zenith2/Engine/ZenithGraph/Tests/Data/zenith2.graph-dump.v1.schema.json` | data | the committed JSON Schema |
| `Zenith2/Engine/ZenithGraph/Tests/Golden/graph_dump_fixed.json`, `graph_dump_no_schedule.json`, `graph_dump_culled.json` | data | golden dumps |
| `Zenith2/Engine/ZenithGraph/Tests/Conformance/Zenith_GraphConformance.h` | `ZenithGraph_Conformance` | the builder interface, the scenario registry, the comparator |
| `Zenith2/Engine/ZenithGraph/Tests/Conformance/Zenith_GraphConformance_Scenarios.cpp` | `ZenithGraph_Conformance` | every scenario of § "Scenario table" |
| `Zenith2/Engine/ZenithGraph/Tests/Conformance/Zenith_GraphConformance_CoreBuilder.cpp` | `ZenithGraph_Conformance` | the core builder |
| `Zenith2/Engine/ZenithGraph/Tests/Conformance/Zenith_GraphConformance_Runner.cpp` | `ZenithGraph_Conformance` | one doctest case per scenario per builder, named `<doctest name>[Core]`, `[Cpu]`, `[Flux]` |
| `Zenith2/Engine/ZenithGraph/Tests/Conformance/Zenith_GraphConformance_Reference.cpp` | `ZenithGraph_Conformance` | the brute-force hazard reference and the random generators |
| `Zenith2/Engine/ZenithJobs/Tests/GraphConformance/Zenith_GraphConformance_CpuBuilder.cpp` | `ZenithGraph_Conformance` | the CPU builder |
| `Zenith2/Engine/ZenithFlux/Tests/GraphConformance/Flux_GraphConformance_FluxBuilder.cpp` | `ZenithGraph_Conformance` (`ZENITH_GPU` presets) | the Flux builder |
| `Zenith2/Engine/ZenithJobs/Tests/Zenith_FrameGraph_Tests.cpp` | `ZenithJobs_Tests` | `Test_FrameGraph_*` |
| `Zenith2/Engine/ZenithJobs/Tests/Zenith_FrameSchedule_Tests.cpp` | `ZenithJobs_Tests` | `Test_FrameSchedule_*` |
| `Zenith2/Engine/ZenithJobs/Tests/Zenith_FrameScheduleCache_Tests.cpp` | `ZenithJobs_Tests` | `Test_FrameScheduleCache_*` |
| `Zenith2/Engine/ZenithJobs/Tests/Zenith_FrameExecutor_Tests.cpp` | `ZenithJobs_Tests` | `Test_FrameExecutor_*` |
| `Zenith2/Engine/ZenithJobs/Tests/Zenith_FrameExecutor_DefectTests.cpp` | `ZenithJobs_Tests` | executor death tests |
| `Zenith2/Engine/ZenithJobs/Tests/Zenith_FixedOrderReduce_Tests.cpp` | `ZenithJobs_Tests` | `Test_FixedOrderReduce_*` |
| `Zenith2/Engine/ZenithJobs/Tests/Examples/Zenith_FrameGraph_Example.cpp`, `Zenith_FrameExecutor_Example.cpp` | `ZenithJobs_Tests` | compiled examples |
| `Zenith2/Engine/ZenithJobs/Tests/Data/zenith2.frame-dump.v1.schema.json`, `Zenith2/Engine/ZenithJobs/Tests/Golden/frame_dump_tick.json` | data | the frame-dump schema and golden |
| `Zenith2/Engine/ZenithJobs/Tests/Support/Zenith_FrameTestGraphs.h` | `ZenithJobs_Tests` | the synthetic graph generators shared by executor, determinism and stress tests (header-only: `inline` functions and templates, never compiled on its own; a non-Core library's `Tests/Support/` holds header-only helpers and a `.cpp` there is a configure error, [Z2-N-ARCH-255]) |

## Graph core unit tests

In `ZenithGraph_Tests`, run in the `Test` step and in the `Zenith2Memory` sanitizer lines.

| Test | Asserts | Requirement |
|---|---|---|
| `Test_Graph_ValueTypes_DefaultsInvalid` | default handles report `IsValid() == false`; the descriptor defaults equal the header's | Z2-R-GRAPH-004 |
| `Test_Graph_Example` | the compiled example builds a three-node graph, compiles and writes a dump without error | Z2-R-GRAPH-004 |
| `Test_Graph_DeclarationErrorsAreSticky` | a graph with a bad subrange keeps accepting declarations, and `Compile` reports the subrange error plus a cycle declared after it | Z2-R-GRAPH-005 |
| `Test_Graph_PoisonedHandle_NoCascade` | after a version fork, three calls with the returned poisoned handle add no diagnostic and do not assert (asserts enabled) | Z2-R-GRAPH-006 |
| `Test_Graph_ForeignHandle_Asserts` (`z2-death-assert`) | a handle of another graph asserts in an assert build | Z2-R-GRAPH-007 |
| `Test_Graph_ZeroSerial_Checks` (`z2-death-check`) | constructing a graph with serial 0 fails `ZENITH_CHECK` | Z2-R-GRAPH-008 |
| `Test_Graph_AccessModeTable` | for each mode, reads-version, produces-version and first-access legality match the specification's table on a defined-read, first-write-discard resource | Z2-R-GRAPH-009 |
| `Test_Graph_EmptySubrangeIsWholeResource` | an empty span and `{0, n}` produce identical schedules and hashes | Z2-R-GRAPH-010 |
| `Test_Graph_ZeroSubresources_IsError` | `m_uSubresourceCount = 0` reports `GRAPH_ERROR_SUBRANGE_OUT_OF_BOUNDS` with the `ZERO_SUBRESOURCES` text | Z2-R-GRAPH-010 |
| `Test_Graph_UnknownFlagBits_Cleared` (`z2-death-assert` variant and a non-assert variant) | unknown bits assert where asserts are on and are cleared (the dump shows none) where they are off | Z2-R-GRAPH-011 |
| `Test_Graph_MarkOutputIdempotent` | two `MarkOutput` calls equal one (hash and dump) | Z2-R-GRAPH-011 |
| `Test_Graph_OpaqueFieldsDoNotAffectSchedule` | varying lanes, usages and payloads changes no order, edge, count or lifetime; lanes and usages change the hash, payloads do not | Z2-R-GRAPH-012 |
| `Test_Graph_NodeLimitReported` | with `uGRAPH_MAX_NODES` replaced by a test arena cap, the node past the arena reports `GRAPH_ERROR_CAPACITY` (`ARENA` text) and subsequent calls return poisoned handles | Z2-R-GRAPH-014 |
| `Test_Graph_NameTooLong_IsCapacityError` | a 256-byte name reports the `NAME_TOO_LONG` text | Z2-R-GRAPH-014 |
| `Test_Graph_NamesCopied` | names, reasons and tags stay intact after the caller overwrites its buffers | Z2-R-GRAPH-015 |
| `Test_Graph_DescReferencesInvalidatedByAdd` | a `GetNodeDesc` reference taken before 1,000 further `AddNode` calls is not used after them (the test re-fetches and compares by value, documenting the Invalidation rule; ASan catches a stale read in `win-asan`) | Z2-R-GRAPH-016 |
| `Test_Graph_FragmentedReadsCompile` | a 64-subresource resource written once per subresource, then read whole by 200 nodes, compiles with the expected WAR and RAW edges and no capacity error (the reader-pool bound holds) | Z2-R-GRAPH-032 |
| `Test_Graph_VersionChain` | three writes return versions 1, 2, 3; `GetLatestVersion` is 3; a read handle is returned unchanged | Z2-R-GRAPH-018 |
| `Test_Graph_CullDropsIncidentEdges` | with culling on, a culled node's edges are absent from `m_axEdges` and from the CSR, and an ordering through it is not preserved | Z2-R-GRAPH-034 |
| `Test_Graph_TwoCyclesTwoDiagnostics` | two disjoint cycles give two `GRAPH_ERROR_CYCLE` diagnostics sorted by their lowest node | Z2-R-GRAPH-036 |
| `Test_Graph_CyclePathFromLowestIndex` | in a four-node component with two cycles, the reported path starts at the lowest index and is the BFS-shortest | Z2-R-GRAPH-037 |
| `Test_GraphDiagnostics_MessageFormats` | one fixture per code and variant produces exactly the text of `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Other diagnostics" | Z2-R-GRAPH-040 |
| `Test_GraphDiagnostics_WorkedExamples` | the worked examples of `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "Worked examples" reproduce byte for byte, and no diagnostic text of any fixture contains a tab character | Z2-R-GRAPH-038 |
| `Test_Graph_ToStringNames` | every enumerator's `Zenith_ToString` equals its name without prefix; error codes start at 1 | Z2-R-GRAPH-041 |
| `Test_Graph_ZeroCostClampedToOne` | cost 0 and cost 1 give identical priorities | Z2-R-GRAPH-043 |
| `Test_Graph_StructureHashMatchesSchedule` | `Zenith_GraphStructureHash` equals `m_ulStructureHash`; `Zenith_GraphCanonicalBytes` hashes to it | Z2-R-GRAPH-047 |
| `Test_Graph_ScheduleOutlivesGraph` | a schedule read after the graph is ended with `Zenith_Destroy` and its arena reset (the schedule arena intact) is unchanged | Z2-R-GRAPH-048 |
| `Test_Graph_ArenaResetWithLiveGraph_Checks` (`z2-death-check`) | resetting a graph's arena while the graph is live fails the arena's `ZENITH_CHECK` ([Z2-N-CONTRACT-230]), because `Zenith_Graph` is not trivially destructible | Z2-R-GRAPH-048 |
| `Test_GraphDump_OptionalSections` | without schedule and timings the dump has no `edges`, `schedule`, `timings`, `frame`, `position`, `lifetime`; culled nodes carry `culled: true` and no position | Z2-R-GRAPH-055 |
| `Test_GraphDump_GoldenValidatesAgainstSchema` | the three goldens validate against the committed schema (test-side validator over the STL) | Z2-R-GRAPH-056 |
| `Test_GraphDump_SchemaFileMatchesDesign` | the committed schema file equals the block in `Zenith2/Docs/Design/GRAPH/Formats.md` → "Graph dump JSON Schema" (the test reads both files) | Z2-R-GRAPH-056 |
| `Test_GraphDump_NewerVersionMessage` | version 2 is unreadable with the exact message; 1 is readable; 0 has its own message | Z2-R-GRAPH-057 |
| `Test_GraphDump_ForeignSchedule_Checks` (`z2-death-check`) | `WriteJson` with another graph's schedule fails `ZENITH_CHECK` | Z2-R-GRAPH-060 |
| `Test_GraphDump_LargeIntegersAsHex` | a priority of 2^53 is written as `0x0020000000000000`; smaller values as numbers | Z2-R-GRAPH-054 |

## Graph conformance suite

### Scenario model

A scenario is a function `void (*)(Zenith_GraphConformanceBuilder& xBuilder)` that declares resources, nodes, accesses and orderings through the builder interface of `Zenith2/Docs/Architecture/ZenithGraph.md` → "Conformance suite", plus a static descriptor:

```cpp
// Engine/ZenithGraph/Tests/Conformance/Zenith_GraphConformance.h (test code)
enum Zenith_GraphConformanceBuilderKind : uint8 { GRAPH_CONFORMANCE_CORE, GRAPH_CONFORMANCE_CPU, GRAPH_CONFORMANCE_FLUX };

struct Zenith_GraphConformanceScenario
{
	const char* m_szName;                             // the test ID, e.g. "Test_GraphConformance_RawEdgeFromWriterToReader"
	void (*m_pfnBuild)(Zenith_GraphConformanceBuilder& xBuilder);
	Zenith_GraphCompileOptions m_xOptions;            // the options the core builder compiles with for the Flux comparison
	uint32 m_uBuilders;                              // bit per builder kind that runs it
	void (*m_pfnCheck)(const Zenith_GraphConformanceResult& xResult);   // the scenario's own assertions, run per builder
};

struct Zenith_GraphConformanceResult
{
	Zenith_GraphConformanceBuilderKind m_eBuilder;
	bool m_bCompiled;
	Zenith_GraphSchedule m_xSchedule;                 // valid when m_bCompiled
	Zenith_GraphCompileError m_xError;                // valid when !m_bCompiled: first code, count, structure hash
	Zenith_Span<const Zenith_GraphDiagnostic> m_axDiagnostics;   // from the builder's Zenith_GraphDiagnosticReport: core codes and texts, front-end prefixes removed
	Zenith_Span<const uint32> m_auScenarioToCore;    // scenario node index -> core node index of the builder's graph
};
```

Scenario resources that are not imported declare `GRAPH_RESOURCE_FLAG_REQUIRE_DEFINED_READS | GRAPH_RESOURCE_FLAG_FIRST_WRITE_DISCARD` and write them first with `WRITE_DISCARD` over every subresource, unless the scenario is about those flags; scenario nodes carry `GRAPH_NODE_FLAG_SIDE_EFFECT` unless the scenario is about culling. That keeps every shared scenario expressible identically by all three builders.

### Builders

| Builder | File | How it realises a scenario |
|---|---|---|
| core | `Zenith_GraphConformance_CoreBuilder.cpp` | a `Zenith_Graph` directly; `Compile` with the scenario's options (and once more with culling off for the CPU comparison) |
| CPU | `Zenith2/Engine/ZenithJobs/Tests/GraphConformance/Zenith_GraphConformance_CpuBuilder.cpp` | builds the scenario into a `Zenith_Graph` inside an owned arena (ended with `Zenith_Destroy` by the schedule that takes the arena, or by the builder before it resets the arena), assembles it with `Zenith_AssembleFrameSchedule` as the TICK subgraph (run bodies record start and end sequence numbers from an atomic counter), takes `GetSubgraph(FRAME_SUBGRAPH_TICK)` as the schedule, then runs the schedule on a `Zenith_JobSystem` with 4 workers through a `Zenith_FrameExecutor` for 20 frames of one tick and asserts, for every kept edge `u → v`, `end(u) < start(v)` in every frame; assembly errors are converted back into core diagnostics through `m_uCoreCode` and the prefix removal |
| Flux | `Zenith2/Engine/ZenithFlux/Tests/GraphConformance/Flux_GraphConformance_FluxBuilder.cpp` | § "The Flux builder mapping"; compile only, no GPU work |

### Comparison rules

For each scenario and each builder in its mask, the runner builds, compiles and runs the scenario's check; then it compares (Z2-R-GRAPH-108):

| Pair | Core compiled with | Compared |
|---|---|---|
| CPU vs core | the scenario's options with `m_bCullUnusedNodes = false` (the CPU front end never culls) | through `m_auScenarioToCore`: the edge set (from, to, reason, resource, version, ordering index), the order, dependency counts, priorities and lifetimes; for failures, the sorted `(code, node A, node B, text)` lists |
| Flux vs core | the scenario's options with `m_bCullUnusedNodes = true` | the same projections |

Any difference fails the test and prints both schedules as graph dumps. The core builder's own run is `[Core]`; the comparisons run inside `[Cpu]` and `[Flux]`.

### Scenario table

Builders: C core, P CPU, F Flux. "Spec" marks a test of `Zenith2/Docs/Architecture/ZenithGraph.md` → "Conformance suite"; the others are added by this design (Z2-R-GRAPH-112).

| Test | Builders | Asserts | Origin |
|---|---|---|---|
| `Test_GraphConformance_RawEdgeFromWriterToReader` | C P F | a write then a read of the new version yields exactly one RAW edge | Spec |
| `Test_GraphConformance_WarEdgeFromReaderToNextWriter` | C P F | a read of v then a write producing v+1 yields a WAR edge reader → writer | Spec |
| `Test_GraphConformance_WawEdgeWithoutInterveningReader` | C P F | two overlapping writes with no reader yield one WAW edge | Spec |
| `Test_GraphConformance_NoWawEdgeWhenReaderIntervenes` | C P F | write, read, write yields RAW and WAR edges and no WAW edge | Spec |
| `Test_GraphConformance_DisjointSubrangeWritesHaveNoEdge` | C P F | writes to disjoint subranges of one resource produce no edge | Spec |
| `Test_GraphConformance_PartialOverlapCreatesEdge` | C P F | writes whose subranges overlap in one subresource produce one edge | Spec |
| `Test_GraphConformance_MultipleSubrangesInOneAccess` | C P F | an access with subranges {0,1} and {3,1} orders against writers of 0 and 3 and not of 1 or 2 | added |
| `Test_GraphConformance_ReadWriteSingleNode` | C P F | a `READ_WRITE` access orders after the prior writer and before the next reader, with no self-edge | Spec |
| `Test_GraphConformance_OldVersionReadOrdersBeforeWriter` | C P F | a read of version 1 declared after the write of version 2 orders before that write | Spec |
| `Test_GraphConformance_ExplicitOrderingEdge` | C P F | `AddOrdering` yields an ORDERING edge carrying the ordering index | Spec |
| `Test_GraphConformance_ReasonPrecedenceOnDuplicate` | C P F | a pair ordered by RAW and by an ordering keeps one edge with reason RAW | added |
| `Test_GraphConformance_VersionForkIsError` | C P F | two writes from the same version fail with `GRAPH_ERROR_VERSION_FORK` naming both nodes | Spec |
| `Test_GraphConformance_UndefinedReadIsError` | C P F | a read of a never-written, non-imported resource with the defined-read flag fails with `GRAPH_ERROR_UNDEFINED_READ` | Spec |
| `Test_GraphConformance_ImportedReadIsLegal` | C P F | the same read of an imported resource compiles | Spec |
| `Test_GraphConformance_ImportedPartialFirstWriteIsLegal` | C P F | a partial first write of an imported resource compiles | added |
| `Test_GraphConformance_FirstWriteMustDiscard` | C P F | a partial first write of a first-write-discard resource fails with `GRAPH_ERROR_FIRST_WRITE_NOT_DISCARD` | Spec |
| `Test_GraphConformance_PerSubresourceFirstWriteDiscard` | C P F | under `GRAPH_RESOURCE_FLAG_FIRST_WRITE_DISCARD_PER_SUBRESOURCE`, six `WRITE_DISCARD` first writes of six disjoint subresources compile with no edge between the writers, and a partial `WRITE` as the first write of an interval fails with `GRAPH_ERROR_FIRST_WRITE_NOT_DISCARD` naming that interval | Spec |
| `Test_GraphConformance_SubrangeOutOfBounds` | C P F | a subrange past the subresource count fails with `GRAPH_ERROR_SUBRANGE_OUT_OF_BOUNDS` | Spec |
| `Test_GraphConformance_InvalidHandleRecorded` | C P F | a handle from another graph records `GRAPH_ERROR_INVALID_HANDLE` (assert disabled in the test) | Spec |
| `Test_GraphConformance_SelfOrderingIsError` | C P F | ordering a node before itself fails with `GRAPH_ERROR_SELF_ORDERING` | Spec |
| `Test_GraphConformance_CycleDiagnosticNamesAccesses` | C P F | a three-node cycle's diagnostic text matches the format byte for byte | Spec |
| `Test_GraphConformance_CycleOfOrderingsOnly` | C P F | a cycle made only of orderings is reported with detail lines `After('<from>') declared by '<to>': "<reason>"`, the reasons verbatim and `""` for an empty one | added |
| `Test_GraphConformance_AllErrorsReportedInOnePass` | C P F | a graph with a fork and a cycle reports both diagnostics | Spec |
| `Test_GraphConformance_TopologicalOrderRespectsEdges` | C P F | for every kept edge, the source's position is below the destination's | Spec |
| `Test_GraphConformance_OrderIsDeterministic` | C P F | two compiles of the same declarations produce identical schedules and hashes | Spec |
| `Test_GraphConformance_PriorityIsBottomLevel` | C P F | priorities on a hand-built diamond equal the analytic bottom levels | Spec |
| `Test_GraphConformance_ReadyTieBreakByDeclarationIndex` | C P F | equal-priority ready nodes run in declaration order | Spec |
| `Test_GraphConformance_TransitiveReductionRemovesImpliedEdges` | C P F | a → b → c plus a → c keeps a → b and b → c only | Spec |
| `Test_GraphConformance_ReductionSkippedAboveLimit` | C P F | a graph above the node limit sets `m_bReductionSkipped` and keeps every edge | Spec |
| `Test_GraphConformance_DependencyCountsMatchEdges` | C P F | counts equal in-degrees of the kept edge set on every scenario (run as a check over every other scenario's result too) | Spec |
| `Test_GraphConformance_CullUnusedNodes` | C P F | with culling on, a node whose writes nobody reads is culled (P: compared with core without culling, so nothing is culled) | Spec |
| `Test_GraphConformance_SideEffectNodeNeverCulled` | C P F | a side-effect node survives culling | Spec |
| `Test_GraphConformance_OutputKeepsProducers` | C P F | the producers of an output resource survive culling | Spec |
| `Test_GraphConformance_PreservingWriteKeepsProducer` | C P F | a partial `WRITE` over an earlier producer keeps that producer live; a `WRITE_DISCARD` over it does not | added |
| `Test_GraphConformance_NoCullingWhenDisabled` | C P F | with culling off, every node is scheduled | Spec |
| `Test_GraphConformance_LifetimeIntervals` | C P F | lifetimes equal the first and last access positions | Spec |
| `Test_GraphConformance_CulledAccessesExcludedFromLifetime` | C P F | a culled node's access does not extend a lifetime | Spec |
| `Test_GraphConformance_StructureHashIgnoresCosts` | C P F | changing costs keeps the hash; changing a name or access changes it | Spec |
| `Test_GraphConformance_ReprioritiseMatchesCompile` | C | reprioritising equals compiling with the new costs, on 200 random graphs (the CPU path is covered by `Test_FrameScheduleCache_ReprioritiseKeepsStructure`) | Spec |
| `Test_GraphConformance_RandomDagProperty` (`z2-stress`) | C P F | on 1,000 random DAGs (seeded, 1–300 nodes), the kept edges preserve every hazard of a brute-force reference and the order is topological | Spec |
| `Test_GraphConformance_JsonDumpSchema` | C | the dump of a fixed graph, written with `JSON_WRITE_STYLE_COMPACT`, matches the committed golden file byte for byte; the same dump in `JSON_WRITE_STYLE_PRETTY` validates against the schema (front-end dumps have their own goldens) | Spec |
| `Test_GraphConformance_ArenaOnlyAllocation` | C | construction and compile succeed with a general-heap hook that fails every allocation, with the graph and schedule arenas pre-grown (one allocation and `Reset` each) before the hook is armed; the CPU and Flux builders allocate from the heap by design | Spec |
| `Test_GraphConformance_CapacityErrorReported` | C | a graph whose arena runs out reports `GRAPH_ERROR_CAPACITY` and never crashes | Spec |
| `Test_GraphConformance_ConcurrentCompileOfOneGraph` (`z2-stress`) | C | four threads compiling the same graph, each into its own arena, produce identical schedules (run under `linux-tsan`) | Spec |
| `Test_GraphConformance_LargeGraphCompletes` | C P F | the 10,000-node, 40,000-access compile succeeds with the expected edge count and no error in `Release` | Spec |

The brute-force reference of `Test_GraphConformance_RandomDagProperty`: for every pair of accesses (a before b in the version order of one resource) that overlap in a subresource and where at least one writes, and every read of a never-written subresource, it computes the required ordering, then checks reachability over the kept edges with a Floyd–Warshall closure (≤ 300 nodes). Random graphs draw 1–8 resources of 1–8 subresources, 1–300 nodes, 1–6 accesses per node with uniformly random modes and subranges (resolving would-be forks by using the latest handle), and 0–20 orderings consistent with a hidden random topological order.

### Expectations per builder

A builder absent from a scenario's mask is absent as [Z2-N-ARCH-343] states: the five scenarios that exercise the core API itself run through the core builder alone. Every builder's `Compile(Zenith_Arena&, Zenith_GraphDiagnosticReport&)` fills the report; the CPU builder converts the assembly's `Zenith_FrameGraphDiagnosticReport` back into core diagnostics through `m_uCoreCode` and the prefix removal, and the Flux builder maps Flux codes back to core codes, so every result's `m_axDiagnostics` holds core codes and core texts. The CPU builder's comparison uses the core compiled without culling because the CPU front end never culls ([Z2-N-ARCH-328]); the Flux builder's uses the core with culling. The CPU builder additionally proves at run time that every kept edge is honoured by the executor on real threads, which is the CPU back end's share of the shared logic (Z2-R-GRAPH-109). The CPU front end's translation layer above the core (stages, fences, version threading) is outside the shared logic and is covered by § "Builder and translation tests".

### The Flux builder mapping

<!-- assumption: pending RHI -->
The Flux builder reaches the render graph through a test-only access type the RHI design provides, `Flux_RenderGraphTestAccess` in `Zenith2/Engine/ZenithFlux/Tests/`, with `AddRawComputePass(Flux_RenderGraph&, Zenith_StringView strName, uint32 uCostHint, bool bSideEffect) → uint32`, `AddRawAccess(Flux_RenderGraph&, uint32 uPass, Flux_RGBuffer xBuffer, Zenith_GraphAccessMode eMode, uint64 ulOffset, uint64 ulBytes)`, `AddRawOrdering(Flux_RenderGraph&, uint32 uBefore, uint32 uAfter, Zenith_StringView strReason)`, `CompileWithoutDevice(Flux_RenderGraph&) → Zenith_Result<const Zenith_GraphSchedule*, Flux_RGError>` and `GetCoreNodeIndex(const Flux_RenderGraph&, uint32 uPass)`.
<!-- /assumption -->

| Scenario element | Flux realisation |
|---|---|
| resource with n subresources | a `Flux_RGBuffer` of n × 256 bytes; subresource i is bytes [256 i, 256 i + 256) |
| `GRAPH_RESOURCE_FLAG_IMPORTED` | an imported external buffer |
| not imported | a transient buffer (Flux adds `REQUIRE_DEFINED_READS | FIRST_WRITE_DISCARD_PER_SUBRESOURCE` itself, [Z2-N-ARCH-346]); a scenario first write that the core builder declares under `FIRST_WRITE_DISCARD` either covers every subresource with `WRITE_DISCARD` or, where it must fail, is a `WRITE`, so both flags give the same result |
| `GRAPH_RESOURCE_FLAG_OUTPUT` | `MarkOutput` |
| node | a raw compute pass; `GRAPH_NODE_FLAG_SIDE_EFFECT` → `Flux_SideEffect`; cost → the pass's cost hint |
| `READ`, `WRITE`, `READ_WRITE`, `WRITE_DISCARD` | an SRV read, a partial UAV write, a UAV read-write, a `[FullCoverage]` UAV write, each over the subranges' byte ranges |
| ordering | `AddRawOrdering` |
| diagnostics | Flux codes mapped back to core codes by `Zenith2/Docs/Rendering/RenderGraph.md` → "Core error mapping", texts unchanged |

The Flux builder is written in Stage 2 after the render-graph compile exists (Z2-R-GRAPH-110).

## CPU frame graph tests

In `ZenithJobs_Tests` unless stated; they run in the `Test` step and, for the `Zenith2Memory` category, under `win-asan` and `linux-tsan`. "Spec" marks a test of `Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Required tests".

### Builder and translation tests

| Test | Asserts | Origin | Requirement |
|---|---|---|---|
| `Test_FrameGraph_Example` | the compiled example declares two worlds' systems and an engine stage, compiles and runs one frame | added | Z2-R-GRAPH-061 |
| `Test_FrameGraph_InvalidDeclaration_IsError` | one fixture per `FRAME_GRAPH_ERROR_INVALID_DECLARATION` reason produces that reason's text | added | Z2-R-GRAPH-062 |
| `Test_FrameGraph_RenderSubmitOutsideRender_IsError` | a render-submit node in the TICK subgraph is rejected | added | Z2-R-GRAPH-090 |
| `Test_FrameGraph_DuplicateNodeName_IsError` | two nodes named alike in one (subgraph, group) fail; the same name in two groups compiles | added | Z2-R-GRAPH-063 |
| `Test_FrameGraph_AfterIfPresent_IgnoresMissing` | an `AfterIfPresent` on an absent node compiles; `After` on it fails | Spec | Z2-R-GRAPH-064 |
| `Test_FrameGraph_QualifiedNameResolvesAcrossGroups` | `After("X@W2")` from group 0 orders after group 2's `X` only | added | Z2-R-GRAPH-064 |
| `Test_FrameGraph_WildcardOrdersEveryWorld` | `After("X@W*")` orders after `X` in every world; with no world it is missing when required | added | Z2-R-GRAPH-064 |
| `Test_FrameGraph_StageBarrier_OrdersStages` | every node of stage p completes before the sync point, and the sync point before stage p+1 | Spec | Z2-R-GRAPH-065 |
| `Test_FrameGraph_FenceWithoutBarrierIsEmpty` | stages without a barrier are still ordered, through a fence with no body | added | Z2-R-GRAPH-065 |
| `Test_FrameGraph_DuplicateStageBarrier_IsError` | two barriers for one stage fail | added | Z2-R-GRAPH-065 |
| `Test_FrameGraph_OrderingContradictsStage_IsError` | a backward `Before` fails with the message format; a backward `After` with its message | Spec | Z2-R-GRAPH-066 |
| `Test_FrameGraph_ReaderAfterWriterWithinStage` | a same-stage reader observes the writer's value | Spec | Z2-R-GRAPH-067 |
| `Test_FrameGraph_BeforeFlipsReaderToPreWrite` | a reader declaring `Before(writer)` observes the prior value | Spec | Z2-R-GRAPH-067 |
| `Test_FrameGraph_UnorderedWriters_IsError` | two same-stage writers fail with `FRAME_GRAPH_ERROR_UNORDERED_WRITERS` | Spec | Z2-R-GRAPH-068 |
| `Test_FrameGraph_DuplicateResourceKey_IsError` | two resources with one key fail naming both | added | Z2-R-GRAPH-069 |
| `Test_FrameGraph_CrossGroupAccess_IsError` | a world-1 node reading a world-2 resource fails | added | Z2-R-GRAPH-069 |
| `Test_FrameGraph_WorldsShareNoEdges` | two worlds' nodes have no edge between them and run concurrently | Spec | Z2-R-GRAPH-070 |
| `Test_FrameGraph_LoweringCanonicalOrder` | two builders declaring the same nodes in a different interleaving of worlds, with identical per-world order, lower to identical graphs and hashes | added | Z2-R-GRAPH-071 |
| `Test_FrameGraph_NoNodeCulled` | a node whose writes nobody reads is scheduled and run | added | Z2-R-GRAPH-035 |
| `Test_FrameGraph_CoreErrorWrappedUnchanged` | a cycle through two systems returns `{ FRAME_GRAPH_ERROR_GRAPH, 1, hash }`, and `GetDiagnostics()` holds one diagnostic whose text is `frame graph: Tick: ` + the core text, with `m_uCoreCode` = `GRAPH_ERROR_CYCLE` and builder node indices, readable until the builder is destroyed | added | Z2-R-GRAPH-072 |
| `Test_FrameGraph_DiagnosticMessageFormats` | one fixture per frame-graph code produces the exact text of the entry's table | added | Z2-R-GRAPH-124 |
| `Test_FrameGraph_ErrorCodeToString` | names without prefix; values from 1 | added | Z2-R-GRAPH-125 |
| `Test_FrameGraph_DisabledNode_SkipsWithoutRecompile` | disabling a system (through its ECS-style trampoline flag in the test) completes its node immediately with zero recompiles | Spec | Z2-R-GRAPH-081 |

### Emission and cache tests

| Test | Asserts | Requirement |
|---|---|---|
| `Test_FrameSchedule_AssembleFromCoreGraphs` | assembling hand-built core graphs yields schedules whose `GetSubgraph` equals the core compile without culling | Z2-R-GRAPH-073 |
| `Test_FrameSchedule_AffinityCounts` | `GetAffinityNodeCount` sums per lane over the three subgraphs, the tick counted once | Z2-R-GRAPH-074 |
| `Test_FrameSchedule_InjectedNodeCount` | a RENDER subgraph with 3 ANY nodes after `Flux.Submit` (render-submit) and 2 ANY nodes with no render-submit predecessor reports 3; `SetSchedule` reserves `max(m_uInjectionCapacity, 2 × 3 + 256)` in the injection queue | Z2-R-GRAPH-091 |
| `Test_FrameSchedule_AffinityNodeCountBoundsQueue` (`z2-stress`) | 1,000 frames of a graph with 50 main-affinity nodes, each with 3 continuations, never reach the main-affinity queue's capacity (`jobs.main.high_water` below it) | Z2-R-GRAPH-091 |
| `Test_FrameSchedule_TransientPlacementNoOverlap` | on 200 random graphs, conflicting transients never share bytes, and two strictly ordered transients do share | Z2-R-GRAPH-053 |
| `Test_FrameSchedule_DumpEmbedsCore` | each subgraph document's `core` equals `Zenith_Graph::WriteJson` of that subgraph byte for byte | Z2-R-GRAPH-058 |
| `Test_FrameSchedule_DumpPerSubgraph` | `WriteJson` writes three documents in subgraph order; `frame_dump_tick.json` golden (COMPACT style) matches byte for byte | Z2-R-GRAPH-059 |
| `Test_FrameGraph_ScheduleCache_HitsWithoutRecompile` (Spec; in `Zenith_FrameScheduleCache_Tests.cpp`) | 1,000 frames with no trigger perform zero recompiles | Z2-R-GRAPH-076 |
| `Test_FrameScheduleCache_HitRebindsRunTable` | a hit with a builder whose `m_pData` pointers differ runs the new pointers | Z2-R-GRAPH-076 |
| `Test_FrameScheduleCache_EvictsLeastRecentlyUsed` | with capacity 2, a third structure evicts the least recently acquired unbound one; a bound one is never evicted | Z2-R-GRAPH-122 |
| `Test_FrameScheduleCache_HashCollisionFallsBackToCompile` | a forced equal hash with different bytes (test hook on the hash function input) compiles and counts a collision | Z2-R-GRAPH-076 |
| `Test_FrameGraph_Recompile_OnWorldCreated` | creating a world (a new group in the rebuilt builder) recompiles once, applied at the next `RunFrame` | Z2-R-GRAPH-077 |
| `Test_FrameGraph_RecompileFailure_KeepsPreviousSchedule` | an invalid change keeps the old schedule current and reports the diagnostic | Z2-R-GRAPH-079 |
| `Test_FrameScheduleCache_ReprioritiseKeepsStructure` | reprioritised schedules keep edges, counts and hash, and their order equals a fresh compile with the measured costs | Z2-R-GRAPH-080 |
| `Test_FrameScheduleCache_Create_FailsAtObject_Unwinds`, `Test_FrameScheduleCache_Create_FailsAtRootTable_Unwinds`, `Test_FrameScheduleCache_Create_FailsAtBindingTable_Unwinds` | the cache's three construction steps (the object from the heap; the root table reserved to `m_uCapacity + 1` entries; the binding table reserved to 16) each fail alone and unwind with zero leaks | Z2-R-GRAPH-117 |
| `Test_FrameScheduleCache_ReductionSkippedLoggedOnce` | two compiles of a 5,000-node graph log the skipped reduction once | Z2-R-GRAPH-120 |

### Executor tests

| Test | Asserts | Requirement |
|---|---|---|
| `Test_FrameExecutor_NoCompileDuringRun` | 1,000 frames increment `graph.compiles` zero times after the first acquire | Z2-R-GRAPH-075 |
| `Test_FrameGraph_SuccessorSeesSubJobWrites` | a node fanning out `ParallelFor` over 100,000 items, and its successor, observe every item written | Z2-R-GRAPH-084 |
| `Test_FrameGraph_ChunkFanOut_ResultIndependentOfWorkers` | a chunk fan-out merging by index yields identical bytes with 1, 2, 8 and all workers | Z2-R-GRAPH-085 |
| `Test_FrameGraph_MergeContinuationBeforeSuccessor` | a node's final continuation runs before any successor starts | Z2-R-GRAPH-086 |
| `Test_FrameGraph_NoBlockingWaitInNodes` | the blocking-wait hook reports zero waits from GRAPH code paths across 1,000 frames | Z2-R-GRAPH-087 |
| `Test_FrameGraph_MainAffinity_RunsOnMainThread` (Spec) | main-affinity nodes run with worker index 0 | Z2-R-GRAPH-088 |
| `Test_FrameGraph_RenderSubmitAffinity_RunsOnSubmitThread` (Spec) | render-submit nodes run with index N+1 and the submit thread never runs other work | Z2-R-GRAPH-089 |
| `Test_FrameExecutor_HighestPriorityReadyTakenFirst` | with one worker, among three nodes made ready by one completion the highest-priority one starts first | Z2-R-GRAPH-092 |
| `Test_FrameExecutor_MainThreadHelps` | with zero free workers busy elsewhere, the main thread runs ANY-affinity nodes | Z2-R-GRAPH-113 |
| `Test_FrameGraph_CancelledInstance_Completes` (Spec) | `RequestCancel` mid-frame completes every counter and returns from `RunFrame`; the next frame runs normally | Z2-R-GRAPH-115 |
| `Test_FrameExecutor_CancelledFrameIsNotRendered` | a cancel during TICK(k) skips EPILOGUE(k)'s bodies; iteration k+1 launches no RENDER(k), reports pipeline depth 0, `m_ulRenderedFrameIndex` 0 and an epilogue end stamp of 0 for frame k, and iteration k+2 renders frame k+1; a cancel that lands while `Render.Seal` runs and makes it return early at its cancel check (no body skipped, `m_uSkippedBodies` 0) also leaves frame k unrendered; a cancel arriving after EPILOGUE(k) completed still gets RENDER(k) in iteration k+1 | Z2-R-GRAPH-115 |
| `Test_FrameExecutor_CancelResyncsRenderWorld` | after a cancel during TICK(k), `IsRenderWorldResyncPending()` is true once `RunFrame(k)` returns; a probe extractor in EPILOGUE(k+1) sees the render-world writer in full-extraction mode, and the flag is false after `RunFrame(k+1)`; a second cancel during EPILOGUE(k+1) keeps it true until the first uncancelled epilogue seals; a cancel that lands while RENDER(k−1) runs after EPILOGUE(k) sealed sets it; twenty uncancelled frames never set it | Z2-R-GRAPH-115 |
| `Test_FrameExecutor_InvocationOrdinalBound` (`z2-death-check`) | `RunFrame` with `m_uTickCount = 254` fails `ZENITH_CHECK` | Z2-R-GRAPH-104 |
| `Test_FrameExecutor_CancelStress` (`z2-stress`) | 1,000 frames each cancelled at a random node complete without a leak or hang | Z2-R-GRAPH-115 |
| `Test_FrameExecutor_ShutdownDrainsFrame` | a shutdown requested from a TICK(N+1) node body takes effect when `RunFrame(N+1)` returns: that iteration ran every tick, EPILOGUE(N+1) and RENDER(N); afterwards the executor is idle, no further instance is launched, and the executor is destroyed without another `RunFrame`, so render world N+1 is never rendered; the same holds when the request also calls `RequestCancel` | Z2-R-GRAPH-116 |
| `Test_FrameExecutor_DestroyDuringRun_Checks` (`z2-death-check`) | destroying the executor from a node body fails `ZENITH_CHECK` | Z2-R-GRAPH-116 |
| `Test_FrameExecutor_Create_FailsAtObject_Unwinds`, `Test_FrameExecutor_Create_FailsAtIterationArena_Unwinds`, `Test_FrameExecutor_Create_FailsAtTimingRing_Unwinds`, `Test_FrameExecutor_Create_FailsAtCostBuffers_Unwinds`, `Test_FrameExecutor_Create_FailsAtConsoleRegistration_Unwinds` | the executor's five construction steps (the object from the heap; the iteration arena object; the timeline ring; the timing and cost buffers reserved for 256 nodes per subgraph; the development-console registration, driven through `m_pxFaultInjector` at `FRAME_EXECUTOR_FAULT_POINT_CONSOLE_REGISTRATION` with a test console in `m_pxConsole`) each fail alone and unwind with zero leaks, no command left registered | Z2-R-GRAPH-117 |
| `Test_FrameExecutor_ZoneNames` | a captured frame (test profiler hooks) has one zone per executed node named as § "Zone sites", with fence names | Z2-R-GRAPH-118 |
| `Test_FrameExecutor_CountersPublished` | every counter of the entry's table is published every frame with the expected value on a fixed graph | Z2-R-GRAPH-119 |
| `Test_FrameExecutor_ConsoleCommandsDevelopmentOnly` | `jobs.recompile` forces a miss and `jobs.dump` writes three files in a development test build; neither is registered when the development-services flag is off | Z2-R-GRAPH-121 |
| `Test_FrameExecutor_TimingRingDropsOldest` | with a ring of 2, the third frame's timeline replaces the first and `frame.timings.dropped` increments | Z2-R-GRAPH-122 |
| `Test_FrameExecutor_CostEwma` | a node sleeping-free busy loop of fixed iterations converges to a stable cost; `IsReprioritiseDue` after 256 frames | Z2-R-GRAPH-080 |
| `Test_FrameExecutor_FloatEnvironmentAssertedPerNode` (`z2-death-assert`) | a node body that changes the rounding mode makes the next node's trampoline fail the float-environment assert | Z2-R-GRAPH-106 |
| `Test_FrameExecutor_SchedulesInUse` | `GetSchedulesInUse` lists the current, the pending and the render schedule; `ReleaseUnused` with that list frees none of them across a swap | Z2-R-GRAPH-101 |
| `Test_FrameExecutor_TransientStorageReused` | two strictly ordered sized transients return overlapping storage; the tick block is reused across tick instances | Z2-R-GRAPH-053 |

### Pipelining tests

| Test | Asserts | Requirement |
|---|---|---|
| `Test_FrameExecutor_SubgraphSequence` | the recorded order inside `RunFrame` is ticks then epilogue; no node runs before `RunFrame` is called; render overlaps and ends before `RunFrame` returns | Z2-R-GRAPH-093 |
| `Test_FrameExecutor_TicksSequential` | with T = 3, every node of tick t completes before any node of tick t+1 starts | Z2-R-GRAPH-094 |
| `Test_FrameExecutor_ZeroTicksStillRunsEpilogue` | T = 0 runs no tick instance and the epilogue once | Z2-R-GRAPH-094 |
| `Test_FrameGraph_PipelinedRender_OverlapsSimulation` (Spec) | RENDER(k−1) and TICK(k) zones overlap in time on a 4-worker run | Z2-R-GRAPH-095 |
| `Test_FrameGraph_SimulationNeverTwoFramesAhead` (Spec) | across 10,000 frames, extraction k never starts before RENDER(k−2) completes | Z2-R-GRAPH-096 |
| `Test_FrameExecutor_PipelineDepthCounter` | `pipeline.depth` is 0 on the first frame and 1 on every subsequent frame with render on | Z2-R-GRAPH-097 |
| `Test_FrameExecutor_TimelineStampsOrdered` | the timeline stamps satisfy the orderings of `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Frame timeline and latency stamps" on 1,000 frames | Z2-R-GRAPH-098 |
| `Test_FrameExecutor_NoRenderWhenDisabled` | `m_bRunRender = false` launches no render node and the depth is 0 | Z2-R-GRAPH-099 |
| `Test_FrameGraph_PrologueRunsNoJobs` (Spec; GRAPH's half) | across 1,000 frames, no node trampoline runs while `m_uIterationRunning` is 0, i.e. between the iteration-done store and the next `RunFrame`; the half asserting that the eight prologue steps run in order is MOD's, in the same test in `Zenith2/Tests/FrameLoop/` | Z2-R-GRAPH-093 |
| `Test_FrameExecutor_RenderUsesEpilogueSchedule` | after a swap, RENDER(k−1) runs the render subgraph of the schedule whose epilogue ran in frame k−1, and that schedule is reported in use until then | Z2-R-GRAPH-100 |

The specification's `Test_RenderWorld_ImmutableAfterSeal`, `Test_RenderWorld_RingSlotReuse` and `Test_RenderWorld_HoldsNoSimulationPointer` are RPROD's (in `Zenith2/Engine/ZenithFlux/Tests/`), because `Flux_RenderWorld` is a Flux type ZenithJobs cannot see; `Test_FrameGraph_ZonesForEveryNode` is DEVT's (in `Zenith2/Engine/ZenithProfiling/Tests/` or the DEVT integration suite); `Test_FrameGraph_AudioUpdateOncePerFrame` is AUDIO's; the `Test_JobSystem_*`, `Test_JobContext_*` and `Test_SimulationClock_*` tests are JOBS'.

### Determinism tests

| Test | Asserts | Requirement |
|---|---|---|
| `Test_FrameGraph_Determinism_WorkerCounts` | a seeded 200-node random graph whose node bodies fold their inputs' values into their outputs yields identical final state with 1, 2, 8 and all workers over 100 frames | Z2-R-GRAPH-103 |
| `Test_FrameGraph_Determinism_CostPerturbation` | the same with 20 random measured-cost tables applied through reprioritisation | Z2-R-GRAPH-102 |
| `Test_FrameExecutor_InvocationIDsDeterministic` | two runs of 50 frames with a fixed tick sequence record identical invocation IDs per node | Z2-R-GRAPH-104 |
| `Test_FixedOrderReduce_MatchesSerialTree` | the parallel form equals the serial form for counts 0, 1, 2, 3, 1,023, 1,024, 1,025 and 100,000, with a non-associative float combine | Z2-R-GRAPH-105 |
| `Test_FixedOrderReduce_IdenticalAcrossWorkerCounts` | the parallel form yields identical bytes with 1, 2, 8 and all workers | Z2-R-GRAPH-105 |

### Stress and sanitizer tests

| Test | Asserts | Requirement |
|---|---|---|
| `Test_FrameGraph_StressUnderTsan` (Spec, `z2-stress`) | 10,000 frames of a 200-node random graph with sub-jobs run clean under `linux-tsan` with 2, 8 and all workers | Z2-R-GRAPH-123 |
| `Test_GraphConformance_ConcurrentCompileOfOneGraph` | as above | Z2-R-GRAPH-017 |
| `Test_FrameExecutor_CancelStress` | as above, also under `linux-tsan` | Z2-R-GRAPH-115 |

## Benchmarks

| Benchmark | Location | Measures | Gated |
|---|---|---|---|
| `Zenith_Bench_GraphCompile` | `Zenith2/Benchmarks/Graph/` | core compile of the 10,000-node, 40,000-access random graph, and the fragmentation pattern of `Zenith2/Docs/Design/GRAPH/CoreAlgorithms.md` → "The subresource interval map" | reported, never gated |
| `Zenith_Bench_ScheduleRecompile` | `Zenith2/Benchmarks/Budgets/GRAPH/` | rebuild plus forced-miss `Acquire` of the workload frame graphs; the Z2-B-GRAPH-001 harness | reported; the ruling uses it |
| `Test_BudgetHarness_Graph_Smoke` | `Zenith2/Benchmarks/Budgets/GRAPH/` | one repetition of the harness completes | `Test` step |

## Fixtures and golden files

| File | Produced by | Regenerated when |
|---|---|---|
| `Zenith2/Engine/ZenithGraph/Tests/Golden/graph_dump_fixed.json` | the fixed 6-node graph of `Test_GraphConformance_JsonDumpSchema` with schedule and timings, `JSON_WRITE_STYLE_COMPACT` (every golden is COMPACT) | D-SEL-HASH (open, Stage 1) is ruled (the `structureHash` value) or the dump format changes (a schema version bump) |
| `graph_dump_no_schedule.json`, `graph_dump_culled.json` | `Test_GraphDump_OptionalSections` | as above |
| `Zenith2/Engine/ZenithJobs/Tests/Golden/frame_dump_tick.json` | `Test_FrameSchedule_DumpPerSubgraph` | as above, or `uFRAME_LOWERING_VERSION` changes |
| the two schema files under `Tests/Data/` | hand-written, equal to `Zenith2/Docs/Design/GRAPH/Formats.md` | the format changes |

A golden file is regenerated only by a ticket that changes the format or rules the decision, in the same diff, never to make a failing test pass (Z2-R-GRAPH-054).
