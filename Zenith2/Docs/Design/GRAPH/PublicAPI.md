# GRAPH design — public API listings

This file is part of the GRAPH design family; the entry document is `Zenith2/Docs/Design/GRAPH.md`, whose `## Public API` section keeps every header's heading and cites this file for the listing moved here to keep the entry below 2,000 lines. Code blocks use tabs.

## Zenith_FrameSchedule.h listing

```cpp
// Zenith2/Engine/ZenithJobs/Zenith_FrameSchedule.h
#pragma once

#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_JsonWriter.h"
#include "ZenithGraph/Zenith_Graph.h"
#include "ZenithJobs/Zenith_JobTypes.h"           // Zenith_JobAffinity, Zenith_PfnJob (JOBS)
#include "ZenithJobs/Zenith_JobsErrors.h"
#include "ZenithJobs/Zenith_FrameGraph.h"

class Zenith_Heap;
struct Zenith_ZoneSite;

/// @contract value Zenith_FrameCompileOptions
/// Ownership: Plain value; m_auMeasuredCostsUs is read during the call only.
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct it and set the fields they need.
/// Invalidation: None.
/// Units: Microseconds, one entry per Zenith_FrameNodeID of the builder being compiled; generated nodes cost 1.
/// Errors: A non-empty span whose length differs from the builder's node count is ignored (cost hints are used) and
///   counted in frame.cost_table_mismatch.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_FrameCompileOptions
{
	bool m_bTransitiveReduction = true;
	Zenith_Span<const uint32> m_auMeasuredCostsUs;
};

/// @contract value Zenith_FrameNodeRuntime
/// Ownership: Plain value; m_pData is owned by the registrant; m_szName points into the schedule's arena once assembled.
/// Thread affinity: IMMUTABLE. Members non-const: the assembler writes each record field by field, m_pxZoneSite last.
/// Invalidation: With the schedule binding that holds it.
/// Units: m_uBuilderNode is a Zenith_FrameNodeID index, or uGRAPH_INVALID_INDEX for a generated node.
/// Errors: A null m_pfnRun on a node other than a generated fence is a programmer error (ZENITH_CHECK at assembly).
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_FrameNodeRuntime
{
	Zenith_PfnJob m_pfnRun = nullptr;
	void* m_pData = nullptr;
	const char* m_szName = nullptr;
	Zenith_JobAffinity m_eAffinity = JOB_AFFINITY_ANY;
	Zenith_FrameNodeKind m_eKind = FRAME_NODE_KIND_STAGE;
	uint16 m_uGroup = 0;
	uint16 m_uStage = 0;
	uint32 m_uBuilderNode = uGRAPH_INVALID_INDEX;
	const Zenith_ZoneSite* m_pxZoneSite = nullptr;   // set at assembly; lives as long as the schedule
};

/// @contract value Zenith_FrameTransientDesc
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: an element of the builder's transient table, written in place.
/// Invalidation: None.
/// Units: m_uCoreResource is a core resource index of the subgraph's graph; bytes and a power-of-two alignment <= 4096.
/// Errors: A zero size, a non-power-of-two alignment or an imported core resource is a programmer error (ZENITH_CHECK).
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_FrameTransientDesc
{
	uint32 m_uCoreResource = uGRAPH_INVALID_INDEX;
	uint32 m_uBuilderResource = uGRAPH_INVALID_INDEX;
	uint64 m_ulBytes = 0;
	uint32 m_uAlignment = 16;
};

/// @contract value Zenith_FrameSubgraphInput
/// Ownership: Borrowed views, read during Zenith_AssembleFrameSchedule; m_pxGraph was constructed with Zenith_TryConstruct
///   in the assembly descriptor's m_xGraphArena and is kept by the schedule, which ends it with Zenith_Destroy before
///   destroying that arena ([Z2-N-CONTRACT-229]).
/// Thread affinity: IMMUTABLE. Members non-const: its producer fills it field by field.
/// Invalidation: None.
/// Units: m_axRuns has one entry per core node index of m_pxGraph.
/// Errors: A run count different from the graph's node count is FRAME_GRAPH_ERROR_INVALID_DECLARATION.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_FrameSubgraphInput
{
	Zenith_Graph* m_pxGraph = nullptr;                           // nullptr = an empty subgraph; ended by the schedule
	Zenith_Span<const Zenith_FrameNodeRuntime> m_axRuns;
	Zenith_Span<const Zenith_FrameTransientDesc> m_axTransients;
};

/// @contract type Zenith_FrameAssemblyDesc
/// Ownership: Move-only; m_xGraphArena owns the arena the input graphs live in, and its ownership moves into the
///   assembled schedule, which keeps the graphs for names and dumps. The other members are borrowed views.
/// Thread affinity: OWNER_THREAD. Filled and consumed by one thread.
/// Invalidation: None.
/// Units: Indexed by Zenith_FrameSubgraph.
/// Errors: As Zenith_FrameSubgraphInput.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_FrameAssemblyDesc
{
	Zenith_Owned<Zenith_Arena> m_xGraphArena;                    // holds every m_pxGraph; moves into the schedule
	Zenith_FrameSubgraphInput m_axSubgraphs[uFRAME_SUBGRAPH_COUNT];
	bool m_bTransitiveReduction = true;
};

/// @contract type Zenith_FrameSchedule
/// Ownership: Created by Zenith_FrameGraphBuilder::Compile, Zenith_AssembleFrameSchedule or Zenith_FrameScheduleCache as
///   a Zenith_Owned (general heap); it owns an arena holding its structure (three graphs, three core schedules) or
///   borrows the structure of a cached parent, and owns its binding (run tables, zone sites, transient layout).
/// Thread affinity: IMMUTABLE. Shared by the executor, the cache and the profiler once created.
///   Members non-const: Compile builds its layout in place.
/// Invalidation: A schedule obtained from a cache is valid until the cache's ReleaseUnused releases it; an owned
///   schedule until its Zenith_Owned is destroyed. The executor never outlives a schedule it runs (the cache is told).
/// Units: Node indices are core node indices of the subgraph's graph.
/// Errors: WriteJson and WriteSubgraphJson return the writer's error; the getters assert a valid subgraph (ZENITH_ASSERT).
/// Version: since Z2 Stage 2. The frame dump is zenith2.frame-dump version 1.
/// @endcontract
class Zenith_FrameSchedule
{
public:
	~Zenith_FrameSchedule();
	Zenith_FrameSchedule(const Zenith_FrameSchedule&) = delete;
	Zenith_FrameSchedule& operator=(const Zenith_FrameSchedule&) = delete;

	const Zenith_GraphSchedule& GetSubgraph(Zenith_FrameSubgraph eSubgraph) const;
	const Zenith_Graph* GetGraph(Zenith_FrameSubgraph eSubgraph) const;       // nullptr for an empty subgraph
	const Zenith_FrameNodeRuntime& GetNodeRuntime(Zenith_FrameSubgraph eSubgraph, uint32 uNode) const;
	uint64 GetStructureHash() const;
	uint32 GetAffinityNodeCount(Zenith_JobAffinity eAffinity) const;
	/// @deviation Units: the RENDER subgraph's JOB_AFFINITY_ANY nodes with at least one kept-edge predecessor of lane
	///   JOB_AFFINITY_RENDER_SUBMIT (the jobs the render-submit thread makes ready and pushes to the injection queue);
	///   the other subgraphs contribute 0, because render-submit affinity is legal only in RENDER.
	uint32 GetInjectedNodeCount() const;
	uint64 GetTransientBytes(Zenith_FrameSubgraph eSubgraph) const;
	Zenith_Result<void, Zenith_JsonWriteError> WriteJson(Zenith_JsonWriter& xWriter) const;
	Zenith_Result<void, Zenith_JsonWriteError> WriteSubgraphJson(Zenith_FrameSubgraph eSubgraph,
		Zenith_JsonWriter& xWriter, const Zenith_GraphTimings* pxTimings) const;

private:
	// Layout: Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md, "Structures and bindings".
};

/// @contract value Zenith_FrameGraphDiagnosticReport
/// Ownership: Caller-owned; Zenith_AssembleFrameSchedule fills it with diagnostics formatted into the caller's
///   diagnostic arena. Not an error type: the text lives here, never in Zenith_FrameGraphError ([Z2-N-CONTRACT-480]).
/// Thread affinity: IMMUTABLE once filled. Members non-const: an out-parameter that Compile fills.
/// Invalidation: The span dies with the diagnostic arena passed to the call that filled it.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_FrameGraphDiagnosticReport
{
	Zenith_Span<const Zenith_FrameGraphDiagnostic> m_axDiagnostics;   // sorted by (code, node A, node B)
};

/// @contract functions Zenith_FrameScheduleAssembly
/// Ownership: The result is a new Zenith_Owned<Zenith_FrameSchedule> from xHeap, owning xDesc.m_xGraphArena and the graphs
///   in it; on failure the function ends every input graph with Zenith_Destroy and destroys xDesc.m_xGraphArena, and the
///   diagnostics are written into xReportOut, formatted in xDiagnosticArena.
/// Thread affinity: ANY_THREAD. The function touches only its arguments and xHeap.
/// Invalidation: None.
/// Units: As Zenith_FrameSubgraphInput.
/// Errors: Zenith_FrameGraphError { m_eCode, m_uDiagnosticCount, m_ulStructureHash }: core compile errors are wrapped as
///   FRAME_GRAPH_ERROR_GRAPH with the core text unchanged; allocation failure is FRAME_GRAPH_ERROR_OUT_OF_MEMORY; a
///   malformed input is FRAME_GRAPH_ERROR_INVALID_DECLARATION.
/// Version: since Z2 Stage 2.
/// @endcontract
Zenith_Result<Zenith_Owned<Zenith_FrameSchedule>, Zenith_FrameGraphError> Zenith_AssembleFrameSchedule(
	Zenith_FrameAssemblyDesc&& xDesc, Zenith_Heap& xHeap, Zenith_Arena& xDiagnosticArena,
	Zenith_FrameGraphDiagnosticReport& xReportOut);

/// @contract value Zenith_FrameMeasuredCosts
/// Ownership: A view over the executor's cost table.
/// Thread affinity: IMMUTABLE. Members non-const: filled one subgraph at a time.
/// Invalidation: Until the next RunFrame of the executor that produced it.
/// Units: Microseconds per core node index, per subgraph.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_FrameMeasuredCosts
{
	const Zenith_FrameSchedule* m_pxSchedule = nullptr;
	Zenith_Span<const uint32> m_aauCostsUs[uFRAME_SUBGRAPH_COUNT];
};

/// @contract value Zenith_FrameScheduleCacheDesc
/// Ownership: Plain value; m_pxHeap is borrowed for the cache's lifetime.
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct it and set the fields they need.
/// Invalidation: None.
/// Units: m_uCapacity in cached structures, 1 to 16.
/// Errors: A null heap or a capacity outside 1 to 16 is FRAME_GRAPH_ERROR_INVALID_DECLARATION from Create.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_FrameScheduleCacheDesc
{
	Zenith_Heap* m_pxHeap = nullptr;
	uint32 m_uCapacity = 4;
};

/// @contract value Zenith_FrameScheduleCacheStats
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: counters the cache accumulates.
/// Invalidation: None.
/// Units: Counts since the cache was created; m_ulLastCompileNs in nanoseconds.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_FrameScheduleCacheStats
{
	uint64 m_ulHits = 0;
	uint64 m_ulMisses = 0;
	uint64 m_ulHashCollisions = 0;
	uint64 m_ulReprioritisations = 0;
	uint64 m_ulEvictions = 0;
	uint64 m_ulFailedCompiles = 0;
	uint64 m_ulLastCompileNs = 0;
	uint32 m_uLiveStructures = 0;
	uint32 m_uLiveBindings = 0;
};

/// @contract type Zenith_FrameScheduleCache
/// Ownership: Created by Create as a Zenith_Owned; owned by the composition root for the process lifetime; owns every
///   structure and binding it hands out.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: A returned schedule stays valid until a ReleaseUnused call whose in-use list omits it.
/// Units: None.
/// Errors: Acquire and Reprioritise return Zenith_FrameGraphError (code, diagnostic count, structure hash; no text); on
///   failure nothing cached or handed out changes. Acquire writes its diagnostics into the builder it was given, whose
///   GetDiagnostics() returns them until the builder is destroyed ([Z2-N-ARCH-440]); Reprioritise's only failure is
///   FRAME_GRAPH_ERROR_OUT_OF_MEMORY with a count of 1, logged at the point of failure.
/// Reentrancy: NOT_APPLICABLE.
/// Locks: uLOCK_LEVEL_JOBS_SCHEDULE is not taken by the cache; the executor takes it when it swaps.
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_FrameScheduleCache
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_FrameScheduleCache>, Zenith_FrameGraphError> Create(
		const Zenith_FrameScheduleCacheDesc& xDesc);
	~Zenith_FrameScheduleCache();

	Zenith_Result<const Zenith_FrameSchedule*, Zenith_FrameGraphError> Acquire(
		Zenith_FrameGraphBuilder& xBuilder, const Zenith_FrameCompileOptions& xOptions);
	Zenith_Result<const Zenith_FrameSchedule*, Zenith_FrameGraphError> Reprioritise(
		const Zenith_FrameMeasuredCosts& xCosts);
	void ReleaseUnused(Zenith_Span<const Zenith_FrameSchedule* const> axInUse);
	Zenith_FrameScheduleCacheStats GetStats() const;
};
```
