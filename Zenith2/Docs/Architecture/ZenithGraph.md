# Architecture — ZenithGraph shared core

This file is part of the Architecture document family. The entry document is `Zenith2/Docs/Architecture.md`; this file holds the full specification of its section "ZenithGraph shared core". Normative statements carry `[Z2-N-ARCH-NNN]` tags in the range 311–379.

## Role of the graph core

`ZenithGraph` is the one dependency-graph engine of Zenith2 (Z2-D-006). It knows nothing about CPUs, GPUs, ECS components or textures. It takes nodes, resources with versions and subresource ranges, declared accesses and explicit orderings; it derives hazard edges, detects cycles, orders the nodes, computes critical-path priorities, dependency counts and resource lifetime intervals, and dumps the whole thing as JSON. Two front ends consume it:

| Front end | Library | Nodes are | Resources are | Emits from the schedule |
|---|---|---|---|---|
| CPU frame graph | `ZenithJobs` | systems and engine stages | component fields, world resources, node scratch buffers | per-node dependency counters, a run order, affinity lanes, scratch-arena offsets (Architecture.md → "CPU frame graph and scheduler") |
| GPU render graph | `ZenithFlux` | render-graph passes | textures, buffers, their subresources | barriers, queue assignment, memory placement for aliasing (Zenith2/Docs/Rendering.md → "Render graph") |

- [Z2-N-ARCH-311] `ZenithGraph` depends on `ZenithCore` alone, and every hazard, ordering, cycle, priority and lifetime computation used by the CPU frame graph or the GPU render graph is performed by `ZenithGraph`; neither front end re-implements any of them.
- [Z2-N-ARCH-312] `ZenithGraph` carries backend meaning only as opaque integers (`m_uLane`, `m_uBackendUsage`, `m_ulBackendPayload`) that it stores, copies into the schedule and dumps, and never interprets.

<!-- informative -->
Zenith's render graph declared every resource twice, ordered passes by registration and aliased only same-format slots; its task system was one mutex-protected queue with blocking `WaitUntilComplete`. Frostbite's FrameGraph, Unreal's RDG and `UE::Tasks`, Destiny's job graph and Bevy/flecs access-derived scheduling are the prior art for a shared, access-derived graph core: one hazard model, one cycle diagnostic and one dump format serve both the CPU frame and the GPU frame.
<!-- /informative -->

## Public API

All declarations below live in the public root header `Engine/ZenithGraph/Zenith_Graph.h`. `Zenith_Arena` and `Zenith_Result` are declared in Zenith2/Docs/Contracts.md → "Ownership and allocation" and Zenith2/Docs/Contracts.md → "Errors and results"; `Zenith_Span`, `Zenith_StringView`, `Zenith_JsonWriter` and `Zenith_Hash64` are ZenithCore types this family assumes, listed with their semantics in Architecture.md → "Purpose and scope" (table "Assumed types").

### Handles

```cpp
inline constexpr uint32 uGRAPH_INVALID_INDEX = 0xFFFFFFFFu;

// Identifies one node of one Zenith_Graph. Valid for the lifetime of the graph that created it.
struct Zenith_GraphNodeHandle
{
	uint32 m_uIndex = uGRAPH_INVALID_INDEX;   // declaration index, 0-based, dense
	uint32 m_uGraphSerial = 0;                // serial of the owning graph; 0 = no graph

	constexpr bool IsValid() const { return m_uIndex != uGRAPH_INVALID_INDEX; }
};

// Identifies one VERSION of one resource of one Zenith_Graph.
// Version 0 is the resource's state before any declared write in this graph.
struct Zenith_GraphResourceHandle
{
	uint32 m_uIndex = uGRAPH_INVALID_INDEX;   // declaration index, 0-based, dense
	uint32 m_uVersion = 0;                    // incremented by every write
	uint32 m_uGraphSerial = 0;

	constexpr bool IsValid() const { return m_uIndex != uGRAPH_INVALID_INDEX; }
};
```

- [Z2-N-ARCH-313] Every `Zenith_Graph` receives a non-zero serial from its constructor's `uSerial` argument, supplied by the front end from a member counter it owns (never from a static or global), and every declaration call checks that each handle it receives carries the graph's own serial and an index below the current count.
- [Z2-N-ARCH-314] A resource handle names a version: `DeclareWrite` and `DeclareAccess` with a write mode return the handle of the new version, and a caller that wants to read or write the result of a write uses that returned handle.

### Access modes and subranges

```cpp
enum Zenith_GraphAccessMode : uint8
{
	GRAPH_ACCESS_READ,            // shared read of the named version
	GRAPH_ACCESS_WRITE,           // write of the declared subranges; other parts keep their contents
	GRAPH_ACCESS_READ_WRITE,      // read of the named version and write of the next, in one node
	GRAPH_ACCESS_WRITE_DISCARD,   // full overwrite of the declared subranges; their prior contents are undefined
};

// A contiguous run of subresources in the resource's flat subresource index space.
// GPU: Flux maps (mip, layer, plane) to a flat index. CPU: one resource per field, one subresource.
struct Zenith_GraphSubrange
{
	uint32 m_uFirst = 0;
	uint32 m_uCount = 0;
};
```

| Mode | Reads version v | Produces version v+1 | Starts a lifetime | Legal as the first access of a non-imported resource |
|---|---|---|---|---|
| `GRAPH_ACCESS_READ` | yes | no | no | no (undefined read) |
| `GRAPH_ACCESS_WRITE` | no | yes | yes | only when the resource lacks both first-write-discard flags |
| `GRAPH_ACCESS_READ_WRITE` | yes | yes | no | no (undefined read) |
| `GRAPH_ACCESS_WRITE_DISCARD` | no | yes | yes | yes (over every subresource under `GRAPH_RESOURCE_FLAG_FIRST_WRITE_DISCARD`; over the written intervals under `GRAPH_RESOURCE_FLAG_FIRST_WRITE_DISCARD_PER_SUBRESOURCE`) |

- [Z2-N-ARCH-315] An empty subrange span means the whole resource (`{0, m_uSubresourceCount}`); a subrange with `m_uCount == 0` or extending past `m_uSubresourceCount` is recorded as `GRAPH_ERROR_SUBRANGE_OUT_OF_BOUNDS` and reported by `Compile`.

### Resource and node descriptions

```cpp
enum Zenith_GraphResourceFlags : uint32
{
	GRAPH_RESOURCE_FLAG_NONE                  = 0,
	GRAPH_RESOURCE_FLAG_IMPORTED              = 1u << 0,  // defined contents exist before the graph runs
	GRAPH_RESOURCE_FLAG_OUTPUT                = 1u << 1,  // read after the graph; its final producers are never culled
	GRAPH_RESOURCE_FLAG_REQUIRE_DEFINED_READS = 1u << 2,  // reading a never-written part of a non-imported resource is an error
	GRAPH_RESOURCE_FLAG_FIRST_WRITE_DISCARD   = 1u << 3,  // the first write of a non-imported resource is WRITE_DISCARD over every subresource
	GRAPH_RESOURCE_FLAG_FIRST_WRITE_DISCARD_PER_SUBRESOURCE = 1u << 4,  // the first write of each subresource interval is WRITE_DISCARD over that interval
};

struct Zenith_GraphResourceDesc
{
	Zenith_StringView m_strName;            // copied into the graph arena; unique names are not required
	uint32 m_uSubresourceCount = 1;        // at least 1
	uint32 m_uFlags = GRAPH_RESOURCE_FLAG_NONE;
	uint64 m_ulBackendPayload = 0;         // opaque to ZenithGraph
};

enum Zenith_GraphNodeFlags : uint32
{
	GRAPH_NODE_FLAG_NONE        = 0,
	GRAPH_NODE_FLAG_SIDE_EFFECT = 1u << 0,  // never culled
};

struct Zenith_GraphNodeDesc
{
	Zenith_StringView m_strName;            // copied into the graph arena
	uint32 m_uLane = 0;                    // opaque execution lane (CPU affinity class, GPU queue)
	uint32 m_uCost = 1;                    // relative cost, at least 1, used for critical-path priority
	uint32 m_uFlags = GRAPH_NODE_FLAG_NONE;
	uint64 m_ulBackendPayload = 0;         // opaque to ZenithGraph
};

struct Zenith_GraphAccessDesc
{
	Zenith_GraphResourceHandle m_xResource;
	Zenith_GraphAccessMode m_eMode = GRAPH_ACCESS_READ;
	Zenith_Span<const Zenith_GraphSubrange> m_axSubranges;  // empty = whole resource; copied into the arena
	uint32 m_uBackendUsage = 0;            // opaque (Flux: SRV/UAV/attachment + stage; CPU: field access kind)
};

struct Zenith_GraphCompileOptions
{
	bool m_bCullUnusedNodes = false;        // Flux: true. CPU frame graph: false.
	bool m_bTransitiveReduction = true;     // drop edges implied by other edges before counting dependencies
	uint32 m_uTransitiveReductionNodeLimit = 4096;  // above this node count, reduction is skipped and reported
};
```

### The graph object

```cpp
// Contract: single-threaded construction; Compile and WriteJson are const and may run on any thread
// once construction has finished. Every allocation comes from the arena passed to the constructor,
// which outlives the graph. Not copyable, not movable.
class Zenith_Graph
{
public:
	Zenith_Graph(Zenith_Arena& xArena, Zenith_StringView strName, uint32 uSerial);
	Zenith_Graph(const Zenith_Graph&) = delete;
	Zenith_Graph& operator=(const Zenith_Graph&) = delete;

	Zenith_GraphResourceHandle AddResource(const Zenith_GraphResourceDesc& xDesc);
	Zenith_GraphNodeHandle AddNode(const Zenith_GraphNodeDesc& xDesc);

	void DeclareRead(Zenith_GraphNodeHandle xNode, Zenith_GraphResourceHandle xResource,
		Zenith_Span<const Zenith_GraphSubrange> axSubranges = {}, uint32 uBackendUsage = 0);

	Zenith_GraphResourceHandle DeclareWrite(Zenith_GraphNodeHandle xNode,
		Zenith_GraphResourceHandle xResource, Zenith_GraphAccessMode eMode,
		Zenith_Span<const Zenith_GraphSubrange> axSubranges = {}, uint32 uBackendUsage = 0);

	Zenith_GraphResourceHandle DeclareAccess(Zenith_GraphNodeHandle xNode,
		const Zenith_GraphAccessDesc& xAccess);

	void AddOrdering(Zenith_GraphNodeHandle xBefore, Zenith_GraphNodeHandle xAfter,
		Zenith_StringView strReason);

	void MarkOutput(Zenith_GraphResourceHandle xResource);

	Zenith_Result<Zenith_GraphSchedule, Zenith_GraphCompileError> Compile(
		const Zenith_GraphCompileOptions& xOptions, Zenith_Arena& xScheduleArena,
		Zenith_GraphDiagnosticReport& xReportOut) const;

	Zenith_Result<void, Zenith_JsonWriteError> WriteJson(Zenith_JsonWriter& xWriter,
		const Zenith_GraphSchedule* pxSchedule, const Zenith_GraphTimings* pxTimings) const;

	uint32 GetNodeCount() const;
	uint32 GetResourceCount() const;
	Zenith_StringView GetName() const;
	const Zenith_GraphNodeDesc& GetNodeDesc(Zenith_GraphNodeHandle xNode) const;
	const Zenith_GraphResourceDesc& GetResourceDesc(Zenith_GraphResourceHandle xResource) const;
	uint32 GetLatestVersion(Zenith_GraphResourceHandle xResource) const;
};
```

- [Z2-N-ARCH-316] No `Zenith_Graph` member function throws or carries an exception specification (Z2-D-141); declaration functions never fail visibly: a declaration error (invalid handle, version fork, bad subrange, arena exhaustion) is recorded as the graph's first sticky error, the call returns an invalid handle where it returns one, and `Compile` reports every recorded error.
- [Z2-N-ARCH-317] A declaration call that receives a handle from another graph or an out-of-range index triggers a programmer-error assert in builds with asserts on and records `GRAPH_ERROR_INVALID_HANDLE` in every build.
- [Z2-N-ARCH-318] `Zenith_Graph` allocates only from the arena passed to its constructor and `Compile` allocates only from `xScheduleArena` (its diagnostics included); neither touches the general heap, and the conformance test `Test_GraphConformance_ArenaOnlyAllocation` proves it with a failing general-heap hook.
- [Z2-N-ARCH-319] Graph captures and every object stored in a schedule arena are trivially destructible, because arenas release memory without running destructors (Zenith2/Docs/Contracts.md → "Ownership and allocation"); `Zenith_Graph` itself is not (it owns `Zenith_Array`s), so its owner destroys it with `Zenith_Destroy` before resetting or destroying the arena it was built in.
- [Z2-N-ARCH-320] `Zenith_Graph` is built by one thread; `Compile`, `WriteJson` and the getters are `const`, touch no mutable state, and may be called concurrently from several threads after construction ends, each concurrent `Compile` with its own schedule arena and its own `Zenith_GraphDiagnosticReport`.

### The schedule

```cpp
enum Zenith_GraphEdgeReason : uint8
{
	GRAPH_EDGE_RAW,       // read after write
	GRAPH_EDGE_WAR,       // write after read
	GRAPH_EDGE_WAW,       // write after write
	GRAPH_EDGE_ORDERING,  // AddOrdering
};

struct Zenith_GraphEdge
{
	uint32 m_uFrom = uGRAPH_INVALID_INDEX;       // node declaration index
	uint32 m_uTo = uGRAPH_INVALID_INDEX;
	Zenith_GraphEdgeReason m_eReason = GRAPH_EDGE_ORDERING;
	uint32 m_uResource = uGRAPH_INVALID_INDEX;   // uGRAPH_INVALID_INDEX for ORDERING
	uint32 m_uVersion = 0;                       // the version whose hazard created the edge
	uint32 m_uOrderingIndex = uGRAPH_INVALID_INDEX;  // index of the AddOrdering call, for ORDERING
};

struct Zenith_GraphLifetime
{
	uint32 m_uFirstPosition = uGRAPH_INVALID_INDEX;  // position in m_axOrder of the first non-culled access
	uint32 m_uLastPosition = uGRAPH_INVALID_INDEX;   // position of the last non-culled access
};

struct Zenith_GraphAccessRecord
{
	uint32 m_uNode;
	uint32 m_uResource;
	uint32 m_uVersionRead;      // uGRAPH_INVALID_INDEX for WRITE and WRITE_DISCARD
	uint32 m_uVersionWritten;   // uGRAPH_INVALID_INDEX for READ
	Zenith_GraphAccessMode m_eMode;
	uint32 m_uFirstSubrange;    // index into m_axSubranges
	uint32 m_uSubrangeCount;
	uint32 m_uBackendUsage;
};

// Immutable. Every span points into the arena passed to Compile, which outlives the schedule.
struct Zenith_GraphSchedule
{
	Zenith_Span<const uint32> m_auOrder;                     // node indices in execution order; culled nodes absent
	Zenith_Span<const uint32> m_auPosition;                  // per node index: position in m_auOrder, or uGRAPH_INVALID_INDEX if culled
	Zenith_Span<const uint32> m_auDependencyCount;           // per node index: distinct predecessors after reduction
	Zenith_Span<const Zenith_GraphEdge> m_axEdges;            // sorted by (m_uFrom, m_uTo, m_eReason, m_uResource)
	Zenith_Span<const uint32> m_auSuccessorOffset;           // CSR: node n's out-edges are [offset[n], offset[n+1])
	Zenith_Span<const uint64> m_aulPriority;                 // per node index: critical-path priority
	Zenith_Span<const Zenith_GraphLifetime> m_axLifetimes;    // per resource index
	Zenith_Span<const Zenith_GraphAccessRecord> m_axAccesses; // grouped by node, in declaration order
	Zenith_Span<const uint32> m_auAccessOffset;              // CSR over m_axAccesses per node
	Zenith_Span<const Zenith_GraphSubrange> m_axSubranges;
	uint64 m_ulStructureHash = 0;                            // hash of the canonical structure (below)
	uint32 m_uCulledNodeCount = 0;
	uint32 m_uReducedEdgeCount = 0;                          // edges removed by transitive reduction
	bool m_bReductionSkipped = false;                         // node count above the reduction limit
};
```

- [Z2-N-ARCH-321] A `Zenith_GraphSchedule` is immutable after `Compile` returns, contains no pointer into the graph's arena, and stays valid for as long as the schedule arena lives, even after the `Zenith_Graph` is destroyed.

## Hazard derivation

`Compile` derives edges per resource from the version chain. Versions give a total order of the writes to one resource; subranges decide which accesses overlap.

### The version chain

- [Z2-N-ARCH-322] A write access consumes the resource's latest version v and produces v+1; a second write declared with a handle whose version is not the latest (two writers of the same version) is `GRAPH_ERROR_VERSION_FORK`, reported with both nodes and the version.
- [Z2-N-ARCH-323] A read access declared with version j observes the resource as left by the writes that produced versions 1..j; reading a version older than the latest is legal and orders the reader before every overlapping write that produced a version above j.

### The derivation algorithm

For each resource, independently:

1. Bucket the accesses by the version they observe (reads, the read half of `READ_WRITE`) and by the version they produce (writes, the write half of `READ_WRITE`).
2. Maintain an interval map over the flat subresource range `[0, m_uSubresourceCount)`. Each interval holds `{ uLastWriter, auReadersSinceLastWrite[] }`, initially `{ none, [] }`; for a resource with `GRAPH_RESOURCE_FLAG_IMPORTED` the initial writer is the virtual node "import", which creates no edge.
3. For v = 0, 1, …, latest:
   1. For each read that observes v, in node declaration order, and for each interval overlapping the read's subranges: if the interval has a last writer W, add the edge W → R (`GRAPH_EDGE_RAW`, version v); if it has none and the resource is not imported and carries `GRAPH_RESOURCE_FLAG_REQUIRE_DEFINED_READS`, record `GRAPH_ERROR_UNDEFINED_READ`; then append R to the interval's readers.
   2. For the write W that produces v+1 (at most one, by the version rule), and for each interval overlapping its subranges: add R → W (`GRAPH_EDGE_WAR`, version v) for every reader R in the interval other than W itself; if the interval has readers none, and a last writer P other than W, add P → W (`GRAPH_EDGE_WAW`); split the interval at the write's boundaries and set `{ uLastWriter = W, readers = [] }` on the written part.
   3. When W is the resource's first write, the resource is not imported and carries `GRAPH_RESOURCE_FLAG_FIRST_WRITE_DISCARD`, and W is not `GRAPH_ACCESS_WRITE_DISCARD` over every subresource, record `GRAPH_ERROR_FIRST_WRITE_NOT_DISCARD`. When the resource is not imported and carries `GRAPH_RESOURCE_FLAG_FIRST_WRITE_DISCARD_PER_SUBRESOURCE` instead, check each interval of the interval map that W writes and no earlier write has written: unless W is `GRAPH_ACCESS_WRITE_DISCARD` over that interval, record `GRAPH_ERROR_FIRST_WRITE_NOT_DISCARD` naming the interval; disjoint first writers of one resource therefore get no edge between them (bullet 324).
4. Append every explicit ordering as `GRAPH_EDGE_ORDERING`; an ordering whose two nodes are the same records `GRAPH_ERROR_SELF_ORDERING`.
5. Deduplicate edges by `(m_uFrom, m_uTo)`, keeping the first reason in the order RAW, WAR, WAW, ORDERING for the diagnostic record, and drop self-edges produced by a node that reads and writes the same parts.

- [Z2-N-ARCH-324] Two accesses of the same resource create an edge only when their subranges overlap; disjoint-subrange writes of the same resource produce no edge between them, so they may run concurrently.
- [Z2-N-ARCH-325] The derivation is deterministic: resources are processed in declaration order, accesses within a version bucket in node declaration order, and the resulting edge list is sorted by `(m_uFrom, m_uTo, m_eReason, m_uResource)`, so the same declarations always produce byte-identical schedules.
- [Z2-N-ARCH-326] Hazard derivation runs in O((A + R) log A) time for A accesses and R resources (interval-map operations) plus O(E log E) to sort E edges.

<!-- informative -->
The WAW edge is added only when no reader separates the two writes, because every reader of the first write already precedes the second write through its WAR edge, and the RAW edge orders the first write before that reader. The edge set is therefore the minimal set that preserves every hazard before transitive reduction runs.
<!-- /informative -->

### Culling

- [Z2-N-ARCH-327] With `m_bCullUnusedNodes` set, a node is culled when it lacks `GRAPH_NODE_FLAG_SIDE_EFFECT` and no non-culled node and no `GRAPH_RESOURCE_FLAG_OUTPUT` resource depends on any version it writes; culling is computed by a reverse reachability walk from side-effect nodes and output resources, and culled nodes are absent from `m_auOrder` with `uGRAPH_INVALID_INDEX` positions.
- [Z2-N-ARCH-328] The CPU frame graph compiles with `m_bCullUnusedNodes = false`, because every system runs every tick whether or not a subsequent node reads its output.

## Cycle detection and diagnostics

- [Z2-N-ARCH-329] `Compile` runs Tarjan's strongly-connected-components algorithm over the deduplicated edge set in O(N + E); any component with more than one node is `GRAPH_ERROR_CYCLE`, and the compile fails with one diagnostic per component.
- [Z2-N-ARCH-330] A cycle diagnostic names one concrete cycle inside its component, found by a breadth-first search from the component's lowest-index node back to itself, and lists every edge of that cycle with its reason, resource name, version and the two accesses that created it.

The cycle diagnostic text format (one block per component, stored in `Zenith_GraphDiagnostic::m_strText` and printed verbatim). The block's first line is `ZenithGraph: cycle among <k> nodes in graph '<graph name>'`, where `<k>` is the node count of the strongly connected component, which can exceed the number of edges of the one cycle the block lists:

```text
ZenithGraph: cycle among 3 nodes in graph 'CPUFrame/World0/Tick'
  [1] 'Physics.Step' -> 'Animation.PostPhysicsBlend'  RAW  'Zenith_RigidBody.m_xWorldTransform' v4
      write by 'Physics.Step' (WRITE), read by 'Animation.PostPhysicsBlend' (READ)
  [2] 'Animation.PostPhysicsBlend' -> 'Gameplay.CameraRig'  ORDERING
      After('Animation.PostPhysicsBlend') declared by 'Gameplay.CameraRig': "camera follows final pose"
  [3] 'Gameplay.CameraRig' -> 'Physics.Step'  WAR  'Zenith_PhysicsWorld' v2
      read by 'Gameplay.CameraRig' (READ v2), next write by 'Physics.Step' (READ_WRITE -> v3)
  break the cycle by removing one of these accesses or orderings
```

Each edge contributes two lines: the header line `  [<n>] '<from>' -> '<to>'  <REASON>` followed, for RAW, WAR and WAW, by two spaces and `'<resource>' v<version>`, and a detail line indented six spaces. The detail line has exactly one of these forms (`<mode>` is `READ`, `WRITE`, `READ_WRITE` or `WRITE_DISCARD`):

| Reason | Detail line |
|---|---|
| RAW | `write by '<A>' (<mode>), read by '<B>' (<mode>)` |
| WAR | `read by '<A>' (<mode> v<j>), next write by '<B>' (<mode> -> v<j+1>)` |
| WAW | `write by '<A>' (<mode> -> v<n>), next write by '<B>' (<mode> -> v<n+1>)` |
| ORDERING | `After('<from>') declared by '<to>': "<reason>"`, where `<from>` and `<to>` are the edge's two nodes and `<reason>` is the ordering's reason string (`""` when it is empty); a front end supplies no other phrasing |

An access with explicit subranges appends ` [<first>,<first+count>)` once per subrange, in ascending order, directly after its closing parenthesis; a whole-resource access appends nothing. The block ends with the fixed line `  break the cycle by removing one of these accesses or orderings`.

- [Z2-N-ARCH-348] Cycle diagnostics use exactly the header, detail-line and subrange forms above, so `Test_GraphConformance_CycleDiagnosticNamesAccesses` compares them byte for byte.

### Error codes and message formats

```cpp
enum Zenith_GraphErrorCode : uint8
{
	GRAPH_ERROR_CYCLE = 1,
	GRAPH_ERROR_VERSION_FORK,
	GRAPH_ERROR_UNDEFINED_READ,
	GRAPH_ERROR_FIRST_WRITE_NOT_DISCARD,
	GRAPH_ERROR_INVALID_HANDLE,
	GRAPH_ERROR_SUBRANGE_OUT_OF_BOUNDS,
	GRAPH_ERROR_SELF_ORDERING,
	GRAPH_ERROR_CAPACITY,
};

struct Zenith_GraphDiagnostic
{
	Zenith_GraphErrorCode m_eCode;
	uint32 m_uNodeA = uGRAPH_INVALID_INDEX;
	uint32 m_uNodeB = uGRAPH_INVALID_INDEX;
	uint32 m_uResource = uGRAPH_INVALID_INDEX;
	uint32 m_uVersion = 0;
	Zenith_StringView m_strText;   // formatted message, in the schedule arena
};

// Caller-owned; Compile and Zenith_GraphReprioritise fill it. Not an error type: the text lives here.
struct Zenith_GraphDiagnosticReport
{
	Zenith_Span<const Zenith_GraphDiagnostic> m_axDiagnostics;  // in the schedule arena; sorted by code, then node A
};

// The error record of [Z2-N-CONTRACT-480]: fixed-size context only.
struct Zenith_GraphCompileError
{
	Zenith_GraphErrorCode m_eCode = GRAPH_ERROR_CAPACITY;  // the code of the report's first diagnostic
	uint32 m_uDiagnosticCount = 0;                        // at least 1: the entries of the caller's report
	uint64 m_ulStructureHash = 0;                         // of the declarations that failed
};
static_assert(sizeof(Zenith_GraphCompileError) <= 32);
```

| Code | Message format |
|---|---|
| `GRAPH_ERROR_CYCLE` | the cycle block above |
| `GRAPH_ERROR_VERSION_FORK` | `ZenithGraph: resource '<res>' version <v> is written by both '<nodeA>' and '<nodeB>'; the second writer must use the handle returned by the first` |
| `GRAPH_ERROR_UNDEFINED_READ` | `ZenithGraph: '<node>' reads subresources [<first>,<first+count>) of '<res>' v<v>, which no node has written` |
| `GRAPH_ERROR_FIRST_WRITE_NOT_DISCARD` | `ZenithGraph: first write of '<res>' by '<node>' is <mode> over [<ranges>]; the first write must be WRITE_DISCARD over every subresource`; under the per-subresource flag `ZenithGraph: first write of '<res>' subresources [<first>,<first+count>) by '<node>' is <mode>; the first write of each subresource must be WRITE_DISCARD` |
| `GRAPH_ERROR_INVALID_HANDLE` | `ZenithGraph: '<call>' received a handle from graph serial <s> (this graph is <t>) or index <i> >= <count>` |
| `GRAPH_ERROR_SUBRANGE_OUT_OF_BOUNDS` | `ZenithGraph: '<node>' declares subrange [<first>,<first+count>) on '<res>', which has <n> subresources` |
| `GRAPH_ERROR_SELF_ORDERING` | `ZenithGraph: ordering '<node>' before itself ("<reason>")` |
| `GRAPH_ERROR_CAPACITY` | `ZenithGraph: arena exhausted after <n> nodes, <r> resources and <a> accesses in graph '<name>'` |

- [Z2-N-ARCH-331] `Compile` reports every error it finds in one pass, not just the first, sorted by code and then by first node index, so one failed compile lists every broken declaration.
- [Z2-N-ARCH-349] A failed `Compile` writes every diagnostic into `xReportOut`, whose entries stay valid until `xScheduleArena` is reset, and returns a `Zenith_GraphCompileError` carrying the first diagnostic's code and the diagnostic count; the record carries no text (Zenith2/Docs/Contracts.md → "Errors and results").

Error codes by front end (the CPU front end wraps every core diagnostic in `SCHEDULE_ERROR_GRAPH` or reports its own earlier diagnosis; the GPU front-end codes are those of Zenith2/Docs/Rendering.md → "Render graph"):

| Core code | CPU front end | GPU front end |
|---|---|---|
| `GRAPH_ERROR_CYCLE` | `SCHEDULE_ERROR_GRAPH` (the core text) | `FLUX_RG_ERROR_CYCLE` |
| `GRAPH_ERROR_VERSION_FORK` | preceded by `SCHEDULE_ERROR_DUPLICATE_WRITER` or `FRAME_GRAPH_ERROR_UNORDERED_WRITERS`; otherwise `SCHEDULE_ERROR_GRAPH` | `FLUX_RG_ERROR_STALE_WRITE` |
| `GRAPH_ERROR_UNDEFINED_READ` | not raised: CPU resources are imported | `FLUX_RG_ERROR_READ_BEFORE_WRITE` |
| `GRAPH_ERROR_FIRST_WRITE_NOT_DISCARD` | not raised: CPU resources are imported | `FLUX_RG_ERROR_READ_BEFORE_WRITE` |
| `GRAPH_ERROR_INVALID_HANDLE`, `GRAPH_ERROR_SUBRANGE_OUT_OF_BOUNDS`, `GRAPH_ERROR_SELF_ORDERING`, `GRAPH_ERROR_CAPACITY` | `SCHEDULE_ERROR_GRAPH` | the Flux code Rendering.md assigns, carrying the core text |

- [Z2-N-ARCH-345] Each front end maps core error codes as the table "Error codes by front end" states and carries the core diagnostic text unchanged in its own diagnostics.
- [Z2-N-ARCH-346] The GPU front end declares every transient resource with `GRAPH_RESOURCE_FLAG_REQUIRE_DEFINED_READS` and `GRAPH_RESOURCE_FLAG_FIRST_WRITE_DISCARD_PER_SUBRESOURCE`, which is how the first-write rule of Zenith2/Docs/Rendering.md → "Transient aliasing" reaches the core: each first write of a subresource interval is declared with its real mode over its real range, so first writers of disjoint subresources of one transient gain no ordering edge; the CPU front end declares every persistent resource `GRAPH_RESOURCE_FLAG_IMPORTED` and sets no first-write flag on arbitration staging resources.
- [Z2-N-ARCH-350] A resource carries at most one of `GRAPH_RESOURCE_FLAG_FIRST_WRITE_DISCARD` and `GRAPH_RESOURCE_FLAG_FIRST_WRITE_DISCARD_PER_SUBRESOURCE`; a resource declared with both triggers a programmer-error assert in builds with asserts on and is checked by the per-subresource rule in every build, and that check runs over the interval map of the derivation algorithm in the same O((A + R) log A) bound.

## Ordering, priorities and dependency counts

### Topological order

- [Z2-N-ARCH-332] The execution order is Kahn's algorithm over the non-culled nodes with a ready set ordered by (critical-path priority descending, node declaration index ascending), so the order is a valid topological order and identical for identical declarations and costs.

### Critical-path priority

- [Z2-N-ARCH-333] A node's priority is its cost plus the maximum priority of its successors (its bottom level): computed in reverse topological order in O(N + E), summed in `uint64`, with `m_uCost` clamped to at least 1.

<!-- elaboration -->
The CPU front end sets `m_uCost` from the node's exponentially weighted mean duration over the last 64 frames, quantised to microseconds, when it recompiles; a first compile uses the declared cost hint. Priorities decide which ready node a worker takes first and do not influence results (Architecture.md → "Determinism guarantee and mechanisms"), so a cost change needs no structural recompile: the front end recomputes priorities in place (§ "Recompilation and caching support").

### Transitive reduction and dependency counts

- [Z2-N-ARCH-334] With `m_bTransitiveReduction` set and at most `m_uTransitiveReductionNodeLimit` non-culled nodes, `Compile` removes every edge u → v for which another path from u to v exists, using per-node reachability bitsets in reverse topological order (O(N · E / 64) time, O(N² / 8) bytes of schedule-arena scratch); above the limit, reduction is skipped, `m_bReductionSkipped` is set, and the front end logs it once.
- [Z2-N-ARCH-335] `m_auDependencyCount[n]` equals the number of distinct predecessors of n in the edge set the schedule keeps, and every edge the schedule keeps appears in n's successor range of its source; the conformance test `Test_GraphConformance_DependencyCountsMatchEdges` checks both properties on every scenario.

## Lifetime intervals

- [Z2-N-ARCH-336] For each resource, `m_axLifetimes` records the positions (in `m_auOrder`) of its first and last non-culled access; a resource with no non-culled access has both positions `uGRAPH_INVALID_INDEX`.
- [Z2-N-ARCH-337] Lifetimes are the one input either front end uses for memory reuse: Flux places transients by lifetime per `(queue, position)` (Zenith2/Docs/Rendering.md → "Transient aliasing"), and the CPU front end reuses frame-arena scratch between two transient resources whose lifetimes do not overlap and where every accessor of the one first in position order reaches every accessor of the other through the kept edges, because concurrently running nodes are not ordered by position alone.

<!-- informative -->
For the GPU, a position-based interval is necessary but not sufficient on its own, because two passes on different queues can have overlapping wall-clock execution even when their positions do not overlap. Flux therefore refines the core's interval with its queue assignment before placing memory; the core supplies the ordered intervals, Flux supplies the cross-queue rules.
<!-- /informative -->

## Structure hash

- [Z2-N-ARCH-338] `m_ulStructureHash` is `Zenith_Hash64` over the canonical little-endian serialization of: node count, each node's name bytes, lane and flags; resource count, each resource's name bytes, subresource count and flags; each access record (node, resource, mode, versions, subranges, backend usage); each ordering (before, after); in declaration order. Costs and backend payloads are excluded, so a cost change never changes the hash.

The hash algorithm is the one ZenithCore exposes as `Zenith_Hash64`, whose implementation is settled by D-SEL-HASH (open, Stage 1).

## Recompilation and caching support

The core itself caches nothing; front ends cache schedules keyed by the structure hash. The core supplies two functions for that:

```cpp
// Recomputes m_aulPriority and the priority-dependent m_auOrder of an existing schedule for new costs.
// The edge set and dependency counts are unchanged; lifetime positions follow the new order.
// xNewCosts has one entry per node index. The only failure is GRAPH_ERROR_CAPACITY.
Zenith_Result<Zenith_GraphSchedule, Zenith_GraphCompileError> Zenith_GraphReprioritise(
	const Zenith_GraphSchedule& xSchedule, Zenith_Span<const uint32> xNewCosts,
	Zenith_Arena& xScheduleArena, Zenith_GraphDiagnosticReport& xReportOut);

// Canonical structure hash of a graph without compiling it (same value as m_ulStructureHash).
uint64 Zenith_GraphStructureHash(const Zenith_Graph& xGraph);
```

- [Z2-N-ARCH-339] `Zenith_GraphReprioritise` produces the same order `Compile` would produce for the same declarations with the new costs, and never alters edges, dependency counts or the accesses that bound each resource's lifetime; positions, and therefore `m_axLifetimes`, follow the new order; the conformance test `Test_GraphConformance_ReprioritiseMatchesCompile` checks the equality on random graphs.

## JSON dump

`WriteJson` writes the graph (and, when given, its schedule and measured timings) for the on-screen profiler's graph view and the offline graph viewer. `Zenith_GraphTimings` is supplied by the front end:

```cpp
struct Zenith_GraphNodeTiming
{
	uint32 m_uNode;          // node declaration index
	uint32 m_uWorker;        // worker or queue index that ran it
	uint64 m_ulStartNs;      // relative to the frame start
	uint64 m_ulEndNs;
};

struct Zenith_GraphTimings
{
	uint64 m_ulFrameIndex = 0;
	Zenith_Span<const Zenith_GraphNodeTiming> m_axNodes;
};
```

The dump schema, `zenith2.graph-dump` version 1:

```json
{
	"schema": "zenith2.graph-dump",
	"schemaVersion": 1,
	"graph": "CPUFrame/World0/Tick",
	"frontEnd": "cpu",
	"frame": 1234,
	"structureHash": "0x9f2c4410aa01be73",
	"nodes": [
		{
			"id": 0,
			"name": "Input.Pump",
			"lane": 1,
			"cost": 12,
			"flags": ["SIDE_EFFECT"],
			"culled": false,
			"position": 0,
			"priority": 431,
			"dependencyCount": 0,
			"payload": "0x0000000000000003"
		}
	],
	"resources": [
		{
			"id": 0,
			"name": "Zenith_Transform.m_xLocal",
			"subresources": 1,
			"flags": ["IMPORTED"],
			"latestVersion": 2,
			"lifetime": { "first": 3, "last": 9 },
			"payload": "0x0000000000000000"
		}
	],
	"accesses": [
		{ "node": 4, "resource": 0, "mode": "READ_WRITE", "versionRead": 0, "versionWritten": 1, "subranges": [[0, 1]], "usage": 2 }
	],
	"orderings": [
		{ "before": 4, "after": 7, "reason": "camera follows final pose" }
	],
	"edges": [
		{ "from": 4, "to": 7, "reason": "RAW", "resource": 0, "version": 1 }
	],
	"schedule": {
		"order": [0, 1, 4, 7],
		"culled": 0,
		"reducedEdges": 3,
		"reductionSkipped": false
	},
	"timings": [
		{ "node": 4, "worker": 2, "startNs": 104000, "endNs": 188500 }
	]
}
```

- [Z2-N-ARCH-340] The dump is deterministic for identical inputs: keys in the order shown, arrays in declaration (or schedule) order, 64-bit values as `0x`-prefixed 16-digit lower-case hexadecimal strings, integers below 2^53 as JSON numbers, and flags as arrays of upper-case names without the `GRAPH_*_FLAG_` prefix.
- [Z2-N-ARCH-341] `schedule` is present only when a schedule is passed and `timings` only when timings are passed; readers ignore unknown keys, and a dump reader refuses a `schemaVersion` above the one it knows with a message naming both versions.
- [Z2-N-ARCH-347] A front end's dump embeds the core document of this section verbatim under the key `"core"` and adds its own data under sibling keys (the GPU front end's placement, barriers and queues; the CPU front end's affinity and timings), so every reader of the core schema reads every front-end dump.

## Complexity and capacity

| Operation | Time | Memory (arena) |
|---|---|---|
| `AddNode`, `AddResource`, `AddOrdering` | O(1) amortised | O(1) per call plus name bytes |
| `DeclareRead`, `DeclareWrite`, `DeclareAccess` | O(S) for S subranges | O(S) |
| hazard derivation | O((A + R) log A + E log E) | O(A + E) |
| culling | O(N + E) | O(N) |
| cycle detection | O(N + E) | O(N) |
| priority and order | O(N + E + N log N) | O(N) |
| transitive reduction | O(N · E / 64) | O(N² / 8) |
| lifetimes | O(A) | O(R) |
| `WriteJson` | O(N + R + A + E) | streaming |

- [Z2-N-ARCH-342] The CPU frame graph's schedule recompile meets the row "CPU frame-graph schedule recompile" (Z2-B-GRAPH-001, pending, allocated by the Stage 1 per-system budget ticket for GRAPH; Zenith2/Docs/Budgets.md → "Per-system rows"), measured on the workload games' real frame graphs; the 10,000-node, 40,000-access random graph is a benchmark reference (`Zenith_Bench_GraphCompile`, reported, not budgeted) and a completion smoke test (`Test_GraphConformance_LargeGraphCompletes`) in the `Test` step.

## Conformance suite

One suite covers the shared logic (`Engine/ZenithGraph/Tests/Conformance/`). Each scenario is a function that builds a declaration set through an abstract builder. There are three builders: `Zenith_Graph` directly (the core builder), the CPU frame-graph front end (`Engine/ZenithJobs/Tests/GraphConformance/`) and the render-graph front end (`Engine/ZenithFlux/Tests/GraphConformance/`); the suite runs each scenario through the builders its row of the table "Graph conformance tests" names and compares their schedules' edge sets, orders, dependency counts and lifetimes.

```cpp
// Engine/ZenithGraph/Tests/Conformance/Zenith_GraphConformance.h (test code, not a public API)
enum Zenith_GraphConformanceBuilderKind : uint8
{
	GRAPH_CONFORMANCE_CORE = 0,
	GRAPH_CONFORMANCE_CPU = 1,
	GRAPH_CONFORMANCE_FLUX = 2,
};

class Zenith_GraphConformanceBuilder
{
public:
	virtual ~Zenith_GraphConformanceBuilder() = default;
	virtual Zenith_GraphConformanceBuilderKind GetKind() const = 0;
	virtual void SetOptions(const Zenith_GraphCompileOptions& xOptions) = 0;    // before any declaration
	virtual uint32 AddResource(const Zenith_GraphResourceDesc& xDesc) = 0;
	virtual uint32 AddNode(const Zenith_GraphNodeDesc& xDesc) = 0;
	virtual void Access(uint32 uNode, uint32 uResource, Zenith_GraphAccessMode eMode,
		Zenith_Span<const Zenith_GraphSubrange> axSubranges) = 0;               // on the latest version
	// An access naming an explicit version: an old-version read, or a forking write
	// (Test_GraphConformance_OldVersionReadOrdersBeforeWriter, Test_GraphConformance_VersionForkIsError).
	virtual void AccessVersion(uint32 uNode, uint32 uResource, uint32 uVersion, Zenith_GraphAccessMode eMode,
		Zenith_Span<const Zenith_GraphSubrange> axSubranges) = 0;
	virtual void Order(uint32 uBefore, uint32 uAfter, const char* szReason) = 0;   // printed verbatim in cycle text
	virtual Zenith_Result<Zenith_GraphSchedule, Zenith_GraphCompileError> Compile(Zenith_Arena& xArena,
		Zenith_GraphDiagnosticReport& xReportOut) = 0;
	virtual Zenith_Span<const uint32> GetScenarioToCore() const = 0;  // scenario node -> core node
};
```

- [Z2-N-ARCH-343] Every scenario in the table "Graph conformance tests" runs through every builder its "Builders" cell names: the five scenarios that exercise the core API itself (`Test_GraphConformance_ArenaOnlyAllocation`, `Test_GraphConformance_CapacityErrorReported`, `Test_GraphConformance_ConcurrentCompileOfOneGraph`, `Test_GraphConformance_ReprioritiseMatchesCompile`, `Test_GraphConformance_JsonDumpSchema`) run through the core builder alone and every other scenario through all three, except that a scenario whose entry sets `m_bAssertsDisabledOnly` has no case where `ZENITH_ASSERTS_ENABLED` is 1, so of the presets bullet 344 names it runs only in `win-release`; a difference between any two builders' edge sets, orders, dependency counts or lifetimes fails the test.
- [Z2-N-ARCH-344] The graph conformance suite runs in the `Test` step on `win-debug` and `win-release`, and under `win-asan` and `linux-tsan` whenever the `Zenith2Memory` category's sanitizer lines run.

Graph conformance tests:

| Test | Builders | Asserts |
|---|---|---|
| `Test_GraphConformance_RawEdgeFromWriterToReader` | all three | a write then a read of the new version yields exactly one RAW edge |
| `Test_GraphConformance_WarEdgeFromReaderToNextWriter` | all three | a read of v then a write producing v+1 yields a WAR edge reader → writer |
| `Test_GraphConformance_WawEdgeWithoutInterveningReader` | all three | two overlapping writes with no reader yield one WAW edge |
| `Test_GraphConformance_NoWawEdgeWhenReaderIntervenes` | all three | write, read, write yields RAW and WAR edges and no WAW edge |
| `Test_GraphConformance_DisjointSubrangeWritesHaveNoEdge` | all three | writes to disjoint subranges of one resource produce no edge |
| `Test_GraphConformance_PartialOverlapCreatesEdge` | all three | writes whose subranges overlap in one subresource produce one edge |
| `Test_GraphConformance_MultipleSubrangesInOneAccess` | all three | an access with subranges {0,1} and {3,1} orders against writers of 0 and 3 and not of 1 or 2 |
| `Test_GraphConformance_ReadWriteSingleNode` | all three | a `READ_WRITE` access orders after the prior writer and before the next reader, with no self-edge |
| `Test_GraphConformance_OldVersionReadOrdersBeforeWriter` | all three | a read of version 1 declared after the write of version 2 orders before that write |
| `Test_GraphConformance_ExplicitOrderingEdge` | all three | `AddOrdering` yields an ORDERING edge carrying the ordering index |
| `Test_GraphConformance_ReasonPrecedenceOnDuplicate` | all three | a pair ordered by RAW and by an ordering keeps one edge with reason RAW |
| `Test_GraphConformance_VersionForkIsError` | all three | two writes from the same version fail with `GRAPH_ERROR_VERSION_FORK` naming both nodes |
| `Test_GraphConformance_UndefinedReadIsError` | all three | a read of a never-written, non-imported resource with the defined-read flag fails with `GRAPH_ERROR_UNDEFINED_READ` |
| `Test_GraphConformance_ImportedReadIsLegal` | all three | the same read of an imported resource compiles |
| `Test_GraphConformance_ImportedPartialFirstWriteIsLegal` | all three | a partial first write of an imported resource compiles |
| `Test_GraphConformance_FirstWriteMustDiscard` | all three | a partial first write of a first-write-discard resource fails with `GRAPH_ERROR_FIRST_WRITE_NOT_DISCARD` |
| `Test_GraphConformance_PerSubresourceFirstWriteDiscard` | all three | under `GRAPH_RESOURCE_FLAG_FIRST_WRITE_DISCARD_PER_SUBRESOURCE`, six `WRITE_DISCARD` first writes of six disjoint subresources compile with no edge between the writers, and a partial `WRITE` as the first write of an interval fails with `GRAPH_ERROR_FIRST_WRITE_NOT_DISCARD` naming that interval |
| `Test_GraphConformance_SubrangeOutOfBounds` | all three | a subrange past the subresource count fails with `GRAPH_ERROR_SUBRANGE_OUT_OF_BOUNDS` |
| `Test_GraphConformance_InvalidHandleRecorded` | all three | a handle from another graph records `GRAPH_ERROR_INVALID_HANDLE`; cases exist only where `ZENITH_ASSERTS_ENABLED` is 0 (`m_bAssertsDisabledOnly`); the assert itself is `Test_Graph_ForeignHandle_Asserts` |
| `Test_GraphConformance_SelfOrderingIsError` | all three | ordering a node before itself fails with `GRAPH_ERROR_SELF_ORDERING` |
| `Test_GraphConformance_CycleDiagnosticNamesAccesses` | all three | a three-node cycle's diagnostic text matches the format above byte for byte |
| `Test_GraphConformance_CycleOfOrderingsOnly` | all three | a cycle made only of orderings is reported with detail lines `After('<from>') declared by '<to>': "<reason>"`, the reasons verbatim and `""` for an empty one |
| `Test_GraphConformance_AllErrorsReportedInOnePass` | all three | a graph with a fork and a cycle reports both diagnostics |
| `Test_GraphConformance_TopologicalOrderRespectsEdges` | all three | for every kept edge, the source's position is below the destination's |
| `Test_GraphConformance_OrderIsDeterministic` | all three | two compiles of the same declarations produce identical schedules and hashes |
| `Test_GraphConformance_PriorityIsBottomLevel` | all three | priorities on a hand-built diamond equal the analytic bottom levels |
| `Test_GraphConformance_ReadyTieBreakByDeclarationIndex` | all three | equal-priority ready nodes run in declaration order |
| `Test_GraphConformance_TransitiveReductionRemovesImpliedEdges` | all three | a → b → c plus a → c keeps a → b and b → c only |
| `Test_GraphConformance_ReductionSkippedAboveLimit` | all three | a graph above the node limit sets `m_bReductionSkipped` and keeps every edge |
| `Test_GraphConformance_DependencyCountsMatchEdges` | all three | counts equal in-degrees of the kept edge set on every scenario |
| `Test_GraphConformance_CullUnusedNodes` | all three | with culling on, a node whose writes nobody reads is culled |
| `Test_GraphConformance_SideEffectNodeNeverCulled` | all three | a side-effect node survives culling |
| `Test_GraphConformance_OutputKeepsProducers` | all three | the producers of an output resource survive culling |
| `Test_GraphConformance_PreservingWriteKeepsProducer` | all three | a partial `WRITE` over an earlier producer keeps that producer live; a `WRITE_DISCARD` over it does not |
| `Test_GraphConformance_NoCullingWhenDisabled` | all three | with culling off, every node is scheduled |
| `Test_GraphConformance_LifetimeIntervals` | all three | lifetimes equal the first and last access positions |
| `Test_GraphConformance_CulledAccessesExcludedFromLifetime` | all three | a culled node's access does not extend a lifetime |
| `Test_GraphConformance_StructureHashIgnoresCosts` | all three | changing costs keeps the hash; changing a name or access changes it |
| `Test_GraphConformance_ReprioritiseMatchesCompile` | core | reprioritising equals compiling with the new costs, on 200 random graphs |
| `Test_GraphConformance_RandomDagProperty` | all three | on 1,000 random DAGs (seeded, 1–300 nodes), the kept edges preserve every hazard of a brute-force reference and the order is topological |
| `Test_GraphConformance_JsonDumpSchema` | core | the dump of a fixed graph matches a committed golden file byte for byte |
| `Test_GraphConformance_ArenaOnlyAllocation` | core | construction and compile succeed with a general-heap hook that fails every allocation |
| `Test_GraphConformance_CapacityErrorReported` | core | a graph whose arena runs out reports `GRAPH_ERROR_CAPACITY` and never crashes |
| `Test_GraphConformance_ConcurrentCompileOfOneGraph` | core | four threads compiling the same graph produce identical schedules (run under `linux-tsan`) |
| `Test_GraphConformance_LargeGraphCompletes` | all three | the 10,000-node, 40,000-access compile succeeds with the expected edge count and no error in `Release` |
