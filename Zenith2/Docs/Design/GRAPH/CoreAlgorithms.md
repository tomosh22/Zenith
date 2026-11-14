# GRAPH design — ZenithGraph core algorithms

This file is part of the GRAPH design family; the entry document is `Zenith2/Docs/Design/GRAPH.md`. It specifies the internals of `Engine/ZenithGraph`: the private data layout, every declaration call, the compile pipeline step by step with its complexity and failure behaviour, reprioritisation, JSON writing and every diagnostic text. The public API is `Zenith2/Docs/Design/GRAPH.md` → "Zenith_Graph.h"; the specification is `Zenith2/Docs/Architecture/ZenithGraph.md`. Requirement IDs `Z2-R-GRAPH-NNN` are those of `Zenith2/Docs/Requirements/GRAPH.md`. Code blocks use tabs.

## Internal file map

| File | Declares or defines |
|---|---|
| `Engine/ZenithGraph/Internal/Zenith_GraphInternal.h` | `Zenith_GraphNodeRecord`, `Zenith_GraphResourceRecord`, `Zenith_GraphAccessEntry`, `Zenith_GraphOrderingRecord`, `Zenith_GraphErrorRecord`, `Zenith_GraphCallID`, the private members of `Zenith_Graph`, the compile context `Zenith_GraphCompileContext` |
| `Engine/ZenithGraph/Internal/Zenith_GraphIntervalMap.h` | `Zenith_GraphIntervalMap`, `Zenith_GraphInterval`, `Zenith_GraphReaderLink` |
| `Engine/ZenithGraph/Internal/Zenith_GraphText.h` | `Zenith_GraphTextWriter` |
| `Engine/ZenithGraph/Internal/*.cpp` | as `Zenith2/Docs/Design/GRAPH.md` → "ZenithGraph" lists them |

## Declaration tables

### Table storage

Every declaration table is a `Zenith_Array<T>` constructed on the graph arena (the arena tier of `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_Array"): `m_axNodes`, `m_axResources`, `m_axAccesses`, `m_axSubranges`, `m_axOrderings` and `m_axErrors`. Every append is `TryPushBack`; on failure the table is unchanged (the strong guarantee) and the graph records its capacity failure (§ "Error records and poisoned handles"). Growth reallocates by 1.5× inside the arena and abandons the old block there, so the graph arena's peak is at most about three times the final table bytes; front ends size the graph arena's `m_ulMaxBytes` from § "Complexity and memory summary" with that factor. A reallocation moves the records, which is why references returned by `GetNodeDesc` and `GetResourceDesc` are valid only until the next `AddNode` or `AddResource` (the contract block's Invalidation field); handles are indices and never move. GRAPH adds no container of its own (`Zenith2/Docs/Design/GRAPH.md` → "Custom infrastructure records this area adds", Z2-R-GRAPH-016).

### Node records

```cpp
struct Zenith_GraphNodeRecord            // 40 bytes
{
	Zenith_StringView m_strName;          // arena copy
	uint32 m_uLane;
	uint32 m_uCost;                      // as declared; clamped to >= 1 at compile
	uint32 m_uFlags;
	uint32 m_uAccessCount;               // accesses declared by this node
	uint64 m_ulBackendPayload;
};
```

`GetNodeDesc` returns a `Zenith_GraphNodeDesc` view reconstructed in a per-record shadow: the record stores exactly the desc's fields in the desc's order plus `m_uAccessCount` in its padding slot, so `GetNodeDesc` returns `reinterpret_cast`-free by keeping a `Zenith_GraphNodeDesc m_xDesc` as the first member in the implementation (the table above shows its fields flattened).

### Resource records

```cpp
struct Zenith_GraphResourceRecord        // 48 bytes
{
	Zenith_GraphResourceDesc m_xDesc;     // name is an arena copy; m_uFlags gains OUTPUT from MarkOutput
	uint32 m_uLatestVersion;             // 0 until the first write
	uint32 m_uLatestWriter;              // node index of the writer of m_uLatestVersion, uGRAPH_INVALID_INDEX before
	uint32 m_uAccessCount;
	uint32 m_uPad;
};
```

### Access records and subrange pool

```cpp
struct Zenith_GraphAccessEntry           // 32 bytes, the internal form of Zenith_GraphAccessRecord
{
	uint32 m_uNode;
	uint32 m_uResource;
	uint32 m_uVersionRead;               // uGRAPH_INVALID_INDEX for WRITE and WRITE_DISCARD
	uint32 m_uVersionWritten;            // uGRAPH_INVALID_INDEX for READ
	Zenith_GraphAccessMode m_eMode;
	uint32 m_uFirstSubrange;             // index into the subrange table; meaningless when m_uSubrangeCount == 0
	uint32 m_uSubrangeCount;             // 0 = the whole resource
	uint32 m_uBackendUsage;
};
```

Subranges are appended to `m_axSubranges` in declaration order; an access with an empty span appends none. Compile expands "whole resource" to the single subrange `{0, m_uSubresourceCount}` in the schedule (the dump's `minItems: 1`).

### Ordering records

```cpp
struct Zenith_GraphOrderingRecord        // 24 bytes
{
	uint32 m_uBefore;
	uint32 m_uAfter;
	Zenith_StringView m_strReason;        // arena copy; may be empty
};
```

### Error records and poisoned handles

```cpp
enum Zenith_GraphCallID : uint8
{
	GRAPH_CALL_ADD_NODE = 1, GRAPH_CALL_ADD_RESOURCE, GRAPH_CALL_DECLARE_READ, GRAPH_CALL_DECLARE_WRITE,
	GRAPH_CALL_DECLARE_ACCESS, GRAPH_CALL_ADD_ORDERING, GRAPH_CALL_MARK_OUTPUT,
};

struct Zenith_GraphErrorRecord           // 32 bytes
{
	Zenith_GraphErrorCode m_eCode;
	Zenith_GraphCallID m_eCall;
	uint16 m_uVariant;                   // selects the message format variant (see "Other diagnostics")
	uint32 m_uNodeA;
	uint32 m_uNodeB;
	uint32 m_uResource;
	uint32 m_uVersion;
	uint32 m_uValueA;                    // subrange first, handle serial, limit, …
	uint32 m_uValueB;                    // subrange count, handle index, …
	uint32 m_uValueC;                    // subresource count, current count, …
};
```

Errors go to `m_axErrors`. If appending an error record itself fails, the graph sets its capacity-failed flag, which `Compile` reports as the one `GRAPH_ERROR_CAPACITY` diagnostic; no error is ever silently lost without that diagnostic (Z2-R-GRAPH-014).

A poisoned handle is `{ uGRAPH_INVALID_INDEX, <this serial> }` for nodes and `{ uGRAPH_INVALID_INDEX, uGRAPH_POISON_VERSION, <this serial> }` for resources. Handle validation, used by every call that takes a handle, returns one of three verdicts (Z2-R-GRAPH-005, Z2-R-GRAPH-006, Z2-R-GRAPH-007):

| Condition | Verdict | Action |
|---|---|---|
| serial equals this graph's and index is `uGRAPH_INVALID_INDEX` | `POISONED` | skip the call; record nothing; no assert |
| serial differs, or index ≥ the table count, or a resource version above the resource's latest version | `INVALID` | `ZENITH_ASSERT(false, …)` naming the call; record `GRAPH_ERROR_INVALID_HANDLE` with (call, handle serial, handle index, count); skip the call |
| otherwise | `VALID` | proceed |

### Name storage

Names, ordering reasons and the front-end tag are copied into the graph arena with `TryAllocate(uBytes, 1)`; an empty name is stored as an empty view with no allocation. A name longer than `uGRAPH_MAX_NAME_BYTES` records `GRAPH_ERROR_CAPACITY` (variant `NAME_TOO_LONG`) and poisons the declaration. The front-end tag is limited to 15 bytes and truncated silently beyond (it is a fixed vocabulary: `core`, `cpu`, `gpu`) (Z2-R-GRAPH-015).

## Declaration algorithms

### AddNode and AddResource

`AddNode(xDesc)`:

1. If the capacity-failed flag is set, return a poisoned node handle.
2. If `xDesc.m_uFlags` has bits other than `GRAPH_NODE_FLAG_SIDE_EFFECT`, `ZENITH_ASSERT` and clear them.
3. If the node count equals `uGRAPH_MAX_NODES`, record `GRAPH_ERROR_CAPACITY` (variant `LIMIT`, value A = the limit), return poisoned.
4. Copy the name (§ "Name storage"); on failure set capacity-failed, record the capacity error, return poisoned.
5. Append the record; on failure as in step 4.
6. Return `{ index, serial }`.

`AddResource(xDesc)`: steps 1 to 6 with the resource table and `uGRAPH_MAX_RESOURCES`, plus: unknown flag bits are asserted and cleared; `m_uSubresourceCount == 0` records `GRAPH_ERROR_SUBRANGE_OUT_OF_BOUNDS` (variant `ZERO_SUBRESOURCES`) and returns poisoned; a count above `uGRAPH_MAX_SUBRESOURCES` records `GRAPH_ERROR_CAPACITY` (variant `LIMIT`). The returned handle has version 0. Both are O(1) amortised (Z2-R-GRAPH-010, Z2-R-GRAPH-011, Z2-R-GRAPH-014).

### DeclareRead DeclareWrite and DeclareAccess

`DeclareRead(n, r, s, u)` is `DeclareAccess(n, { r, GRAPH_ACCESS_READ, s, u })` discarding the returned handle. `DeclareWrite(n, r, m, s, u)` is `DeclareAccess(n, { r, m, s, u })`, with `m == GRAPH_ACCESS_READ` treated as an invalid enumerator for that call. `DeclareAccess(n, a)`:

1. Capacity-failed: return a poisoned resource handle.
2. Validate `n` and `a.m_xResource`; on `POISONED` or `INVALID`, return poisoned.
3. If `a.m_eMode > GRAPH_ACCESS_WRITE_DISCARD`: assert, record `GRAPH_ERROR_INVALID_HANDLE` (variant `BAD_MODE`), return poisoned.
4. For each subrange `{f, c}`: if `c == 0 || f > n_sub || c > n_sub − f` (overflow-safe), record `GRAPH_ERROR_SUBRANGE_OUT_OF_BOUNDS` (node, resource, f, c, n_sub) and return poisoned without declaring the access. Overlapping subranges inside one access are legal and harmless.
5. If the access count equals `uGRAPH_MAX_ACCESSES`: capacity error, poisoned.
6. Versions (Z2-R-GRAPH-018, Z2-R-GRAPH-019, Z2-R-GRAPH-020):
   - `READ`: version read = the handle's version (validation guaranteed ≤ latest); version written = `uGRAPH_INVALID_INDEX`.
   - `WRITE`, `WRITE_DISCARD`, `READ_WRITE`: if the handle's version ≠ the resource's latest version, record `GRAPH_ERROR_VERSION_FORK` with node A = the writer of version (handle version + 1), found by scanning this resource's access entries in declaration order for the one whose version written equals handle version + 1 (an error path, O(A)), node B = `n`, version = the handle's version; return poisoned. If latest + 1 would exceed `uGRAPH_MAX_VERSIONS`, capacity error. Otherwise version written = latest + 1; version read = latest for `READ_WRITE`, `uGRAPH_INVALID_INDEX` otherwise; set latest = version written and latest writer = `n`.
7. Append the subranges then the access entry (on failure: capacity-failed, poisoned; a partially appended subrange run is harmless because no access references it); increment the node's and the resource's access counts.
8. For a writing mode return `{ r.index, version written, serial }`; for `READ` return the input handle.

Complexity O(S) for S subranges, amortised.

### AddOrdering and MarkOutput

`AddOrdering(b, a, reason)`: capacity-failed returns; validate both handles; if `b.m_uIndex == a.m_uIndex`, record `GRAPH_ERROR_SELF_ORDERING` (node A = b, with the reason copied into the record's text slot at compile from the ordering reason argument, which is copied into the arena first) and skip; if the ordering count equals `uGRAPH_MAX_ORDERINGS`, capacity error; copy the reason; append. The ordering's declaration index is its position in the ordering table (Z2-R-GRAPH-029).

`MarkOutput(r)`: validate; set `GRAPH_RESOURCE_FLAG_OUTPUT` on the record's flags. Idempotent (Z2-R-GRAPH-011).

`SetFrontEndTag(t)`: copies up to 15 bytes; callable any number of times before `Compile`; default `core`.

## Compile pipeline

### Compile step list

`Compile(xOptions, xScheduleArena, xReportOut)` is `const`; it reads the graph and writes only `xScheduleArena` and `xReportOut` (Z2-R-GRAPH-013, Z2-R-GRAPH-017). All scratch and all results come from `xScheduleArena` through `TryAllocate` (and `Zenith_Array` in its arena tier where a count is not known in advance); every sort is `Zenith_TryStableSort` with scratch from the same arena (`Zenith2/Docs/Design/CORE/Containers.md` → "Sorting and searching"), and a failed allocation or sort anywhere in the compile records `GRAPH_ERROR_CAPACITY` (variant `ARENA`) and skips the remaining steps except 12 and 13, never asserting; results are allocated after scratch, so the schedule arena's peak is results plus scratch (§ "Complexity and memory summary"). Steps, in order:

| Step | Name | Runs when |
|---|---|---|
| 0 | reserve the emergency diagnostic; snapshot counts N, R, A, O | always |
| 1 | flatten and group accesses | no capacity failure |
| 2 | hazard derivation per resource | as step 1 |
| 3 | edge sort and deduplication | as step 1 |
| 4 | cycle detection | as step 1 |
| 5 | cycle path extraction | a cycle exists |
| 6 | culling | no error so far and `m_bCullUnusedNodes` |
| 7 | critical-path priority | no error so far |
| 8 | execution order | no error so far |
| 9 | transitive reduction | no error so far and `m_bTransitiveReduction` |
| 10 | dependency counts and successor ranges | no error so far |
| 11 | lifetimes | no error so far |
| 12 | structure hash | always: it is streamed with no allocation and cannot fail, and a failed compile returns it in `Zenith_GraphCompileError::m_ulStructureHash`, which the CPU cache uses to name the failing structure in its log line |
| 13 | diagnostics assembly | any error |

"Error so far" means a recorded declaration error, a capacity failure, or a derived error from steps 2 to 5. A graph with errors therefore reports every declaration error, every undefined read, every first-write violation and every cycle in one pass (Z2-R-GRAPH-039).

```cpp
// Engine/ZenithGraph/Internal/Zenith_GraphInternal.h
struct Zenith_GraphCandidateEdge         // 24 bytes, scratch
{
	uint32 m_uFrom;
	uint32 m_uTo;
	uint32 m_uResource;                  // uGRAPH_INVALID_INDEX for ORDERING
	uint32 m_uVersion;
	uint32 m_uOrderingIndex;
	Zenith_GraphEdgeReason m_eReason;
	bool m_bPreservingWrite;              // WAW whose destination access is GRAPH_ACCESS_WRITE (culling liveness)
	uint16 m_uPad;
};
```

### Step 1 flatten and group accesses

1. Copy the access entries into a contiguous array `axAccess[A]` (declaration order).
2. Counting sort by resource: `auResourceOffset[R + 1]` then `auByResource[A]`, stable in declaration order. O(A + R).
3. Counting sort by node for the schedule's `m_axAccesses` / `m_auAccessOffset` (grouped by node, declaration order within a node), expanding empty subrange spans to `{0, n_sub}` into the result `m_axSubranges`. O(A + N + S).
4. For each resource, build its event list in scratch: every read-capable access (`READ`, `READ_WRITE`) contributes a read event `(v = version read, phase 0, node, access index)`; every writing access contributes a write event `(v = version written − 1, phase 1, node, access index)`; sort the resource's events by `(v, phase, node, access index)` with `Zenith_TryStableSort`. O(A log A) overall.
5. Compute the reader-pool bound (§ "The subresource interval map"): L = Σ over read events a of min(S_a, 2 × W_r + 1), where S_a is the number of subresources a's subranges cover and W_r the write-event count of a's resource r; and the interval-array bound per resource, 2 × W_r + 1.

### Step 2 hazard derivation per resource

For each resource r, in declaration order (Z2-R-GRAPH-022 to Z2-R-GRAPH-028, Z2-R-GRAPH-031):

1. Initialise the interval map over `[0, n_sub)` with one interval: writer = `IMPORT` (`0xFFFFFFFE`) when r is imported, else `NONE` (`uGRAPH_INVALID_INDEX`); no readers. Set `bFirstWriteSeen = false`.
2. Walk r's sorted events. For a **read event** of node R at version v, for each subrange of the access, for each interval I overlapping it (§ "The subresource interval map"):
   - if I's writer is a node W ≠ R: append candidate `W → R`, `RAW`, version v;
   - if I's writer is `NONE`, r is not imported and r carries `GRAPH_RESOURCE_FLAG_REQUIRE_DEFINED_READS`: record a derived `GRAPH_ERROR_UNDEFINED_READ` (node R, resource r, version v, first = max(I.first, subrange.first), count = the overlap length); consecutive overlapping `NONE` intervals of one read merge into one diagnostic;
   - append R to I's reader list unless the list's last entry is R.
3. For a **write event** of node W producing v + 1 with mode M:
   - first-write rule: if `!bFirstWriteSeen`, r is not imported, r carries `GRAPH_RESOURCE_FLAG_FIRST_WRITE_DISCARD` and not `GRAPH_RESOURCE_FLAG_FIRST_WRITE_DISCARD_PER_SUBRESOURCE`, and not (M is `WRITE_DISCARD` and the access's subranges cover `[0, n_sub)` completely), record a derived `GRAPH_ERROR_FIRST_WRITE_NOT_DISCARD` (node W, resource r, mode M, the access's subranges); then set `bFirstWriteSeen = true`;
   - per-subresource first-write rule ([Z2-N-ARCH-346]): if r is not imported, r carries `GRAPH_RESOURCE_FLAG_FIRST_WRITE_DISCARD_PER_SUBRESOURCE` and M is not `WRITE_DISCARD`, then for each interval I overlapping the access's subranges whose writer is `NONE` (never written), record a derived `GRAPH_ERROR_FIRST_WRITE_NOT_DISCARD` (node W, resource r, mode M, first = max(I.first, subrange.first), count = the overlap length), consecutive overlapping `NONE` intervals of one write merging into one diagnostic; this runs over the same interval map before the write updates it, so disjoint first writers gain no edge and the bound stays O((A + R) log A). A resource carrying both first-write flags asserts at `AddResource` where asserts are enabled and is checked by this rule alone in every build ([Z2-N-ARCH-350]);
   - for each subrange and each overlapping interval I: for every reader Rd in I with Rd ≠ W append `Rd → W`, `WAR`, version v; if I has no reader at all and I's writer is a node P ≠ W, append `P → W`, `WAW`, version v, with `m_bPreservingWrite = (M == GRAPH_ACCESS_WRITE)`;
   - then set, over each written subrange, writer = W and readers = none (splitting at the subrange boundaries and merging neighbours with identical state).
4. Keep the final interval map's writers of every `GRAPH_RESOURCE_FLAG_OUTPUT` resource: they are the culling roots of step 6.

The WAW edge is omitted when any reader (including W itself through `READ_WRITE`) sits in the interval, because the RAW edge into that reader and the WAR edge out of it already order the two writes ([Z2-N-ARCH-324]'s informative note). Disjoint subranges touch disjoint intervals and produce no candidate (Z2-R-GRAPH-025).

### The subresource interval map

```cpp
// Engine/ZenithGraph/Internal/Zenith_GraphIntervalMap.h
struct Zenith_GraphReaderLink { uint32 m_uNode; uint32 m_uNext; };   // singly linked, in a scratch pool

struct Zenith_GraphInterval
{
	uint32 m_uFirst;             // inclusive
	uint32 m_uEnd;               // exclusive
	uint32 m_uWriter;            // node, IMPORT or NONE
	uint32 m_uFirstReader;       // index into the reader pool, uGRAPH_INVALID_INDEX = none
	uint32 m_uLastReader;        // for O(1) append and the duplicate check
};

class Zenith_GraphIntervalMap
{
public:
	void Reset(uint32 uSubresourceCount, uint32 uInitialWriter);
	template<typename TVisit> void ForEachOverlap(uint32 uFirst, uint32 uEnd, TVisit&& xVisit);
	bool AddReader(Zenith_GraphInterval& xInterval, uint32 uNode);   // false: pool exhausted
	bool Assign(uint32 uFirst, uint32 uEnd, uint32 uWriter);       // splits, clears readers, merges
};
```

The intervals of one resource are a sorted array of disjoint intervals covering `[0, n_sub)` (capacity 2 × writes of the resource + 1, allocated per resource from scratch sized in step 1). `ForEachOverlap` binary-searches the first interval with `m_uEnd > uFirst` and walks forward while `m_uFirst < uEnd`. `Assign` finds the first and last overlapping intervals, splits them at `uFirst` and `uEnd`, replaces the covered run with one interval, then merges it with a neighbour whose writer equals `uWriter` and that has no reader. Fast path: when `n_sub == 1` (every CPU resource), the map is one interval and every operation is O(1).

Cost: each operation is O(log I + k) for I intervals and k overlapped intervals. With the fast path for single-subresource resources and at most O(1) fragments overlapped per access in Flux's mip and layer patterns, the derivation meets the specification's O((A + R) log A); a pathological pattern (many single-subresource writes followed by many whole-resource reads) degrades to O(A · I), which the benchmark `Zenith_Bench_GraphCompile` reports and no budget gates (Z2-R-GRAPH-049). Sizing: a read that overlaps k intervals appends k reader links, and every interval it overlaps covers at least one of its subresources, so k ≤ min(S_a, 2 × W_r + 1); the reader pool is therefore allocated once with the bound L of step 1.5 (for every CPU resource, S_a = 1 and L is the read count), and each resource's interval array with 2 × W_r + 1 entries (each write splits at most two intervals and adds at most one). With these sizes `AddReader` and `Assign` cannot run out; if either nevertheless returns false (a defect in the bound), the compile records `GRAPH_ERROR_CAPACITY` (variant `ARENA`) and returns it, never asserting (Z2-R-GRAPH-049).

### Step 3 edge sort and deduplication

1. Append one candidate per ordering: `before → after`, `ORDERING`, ordering index, resource `uGRAPH_INVALID_INDEX`.
2. Drop candidates with `m_uFrom == m_uTo` (a node that reads and writes the same parts).
3. Sort by `(m_uFrom, m_uTo, m_eReason, m_uResource, m_uVersion, m_uOrderingIndex)` with `Zenith_TryStableSort`. O(E log E). Candidates are appended during step 2 to a `Zenith_Array<Zenith_GraphCandidateEdge>` in the schedule arena.
4. Keep the first candidate of each `(m_uFrom, m_uTo)` run; its reason, resource, version and ordering index are the edge's diagnostic record (RAW before WAR before WAW before ORDERING). The `m_bPreservingWrite` flag of the kept WAW candidate is the OR over the run's WAW candidates.
5. Build the CSR successor offsets over all N nodes from the deduplicated list (Z2-R-GRAPH-030).

### Step 4 cycle detection

Tarjan's algorithm, iterative (an explicit stack of `(node, next edge index)` frames, so deep graphs cannot overflow the thread stack), over all N nodes in index order and the deduplicated edges. It assigns each node a component ID; components are numbered in order of discovery. Every component with more than one node is a cycle; a self-loop cannot exist after step 3.2. O(N + E) time, 4 arrays of N `uint32` scratch (index, low-link, on-stack flag with component ID, the stack) (Z2-R-GRAPH-036).

### Step 5 cycle path extraction and text

For each cyclic component, in ascending order of its lowest node index s (Z2-R-GRAPH-037):

1. Breadth-first search from s over edges whose destination lies in the same component, visiting each node's successors in CSR order (ascending destination index), recording the parent edge of each first-visited node.
2. The first time an edge `u → s` is examined, stop: the cycle is s → … → u → s along parent edges.
3. Emit one `GRAPH_ERROR_CYCLE` diagnostic: node A = s, node B = u, the component's node count, and the path's edges in path order, formatted as § "Cycle block".

O(N_c + E_c) per component.

### Step 6 culling

With `m_bCullUnusedNodes` (Z2-R-GRAPH-033, Z2-R-GRAPH-034):

1. Roots: every node with `GRAPH_NODE_FLAG_SIDE_EFFECT`; every node kept by step 2.4 as a final writer of an output resource.
2. Build the reverse CSR of the deduplicated edges.
3. Worklist from the roots, marking live. For a live node n and each incoming edge `p → n`: mark p live when the edge's reason is `RAW`, or when it is `WAW` with `m_bPreservingWrite` (n's write keeps p's other contents). `WAR` and `ORDERING` edges never make a node live.

<!-- elaboration -->
This refines [Z2-N-ARCH-327]'s "depends on any version it writes": a WAR edge orders a reader before a subsequent writer and carries no data from the reader, an ORDERING edge carries no data at all, and a WAW edge into a `WRITE_DISCARD` (or into a write covering every subresource p wrote) overwrites p's contents without reading them, so none of the three makes p's output observable; only a read (RAW) or a partial overwrite that keeps p's other contents (preserving WAW) does.
4. Every node not live is culled: position `uGRAPH_INVALID_INDEX`, absent from the order, its incident edges removed from the edge list and the CSR rebuilt; `m_uCulledNodeCount` counts them.

An ordering through a culled node is not preserved: if A is ordered before culled C and C before B, A and B are unordered after culling. The CPU front end never culls ([Z2-N-ARCH-328], Z2-R-GRAPH-035), and Flux's orderings do not pass through passes without consumers. O(N + E).

### Step 7 critical-path priority

1. Compute any topological order of the non-culled nodes by Kahn's algorithm with a FIFO queue seeded in index order.
2. Walk it in reverse: `priority[n] = max(cost[n], 1) + max(priority[s] for s in successors(n))`, 0 for no successor term, in `uint64` (Z2-R-GRAPH-043).

Priorities computed over the deduplicated edges equal those over the reduced edges, because a removed edge u → v is implied by a longer path u → … → v whose priority sum is larger. O(N + E).

### Step 8 execution order

Kahn's algorithm over the non-culled nodes with a binary min-heap keyed by `(~priority, node index)` (so the highest priority, then the lowest index, pops first), decrementing in-degrees over the deduplicated edges. Pops give `m_auOrder`; `m_auPosition[n]` is n's pop index, `uGRAPH_INVALID_INDEX` for culled nodes (Z2-R-GRAPH-042). O((N + E) + N log N).

The order is identical whether in-degrees count the deduplicated or the reduced edge set: a node whose reduced predecessors have all popped has had all its deduplicated predecessors popped too, because each removed predecessor is an ancestor of a kept one. `Zenith_GraphReprioritise` relies on this (§ "Reprioritisation").

### Step 9 transitive reduction

If `m_bTransitiveReduction` and the non-culled count N' ≤ `m_uTransitiveReductionNodeLimit` (Z2-R-GRAPH-044):

1. Allocate N' bitsets of N' bits, indexed by position (O(N'² / 8) bytes of scratch).
2. For each node u in reverse order: clear `reach[u]`; for each successor v of u in ascending position order: if `reach[u]` has bit `position[v]`, mark edge u → v redundant; otherwise keep it and `reach[u] |= reach[v]`, then set bit `position[v]`.
3. Remove redundant edges; `m_uReducedEdgeCount` counts them.

Correctness: if v is reachable from u through another successor w, then position[w] < position[v], so w is processed first and `reach[w]` contains v. O(N' · E / 64). Above the limit, reduction is skipped, every edge is kept and `m_bReductionSkipped = true`; the front end logs it once per graph name.

### Step 10 dependency counts and successor ranges

The kept edges remain sorted by `(from, to)` (a subsequence of a sorted list). `m_auSuccessorOffset[N + 1]` is their CSR over all node indices (empty ranges for culled nodes); `m_auDependencyCount[n]` is n's in-degree over the kept edges; `m_axEdges` is the kept list copied into the result (the internal preserving flag dropped) (Z2-R-GRAPH-045). O(N + E).

### Step 11 lifetimes

For every access of a non-culled node: p = position[node]; `first[r] = min(first[r], p)`, `last[r] = max(last[r], p)`. Resources with no such access keep `uGRAPH_INVALID_INDEX` for both (Z2-R-GRAPH-051, Z2-R-GRAPH-052). O(A).

### Step 12 structure hash

Stream the canonical fields of `Zenith2/Docs/Design/GRAPH.md` → "Canonical structure serialization", in its order, into a `Zenith_Hash64Stream` (seed 0) with `Update` for the magic and name bytes, `UpdateU32` for every `uint32` field and `Update` of the mode byte plus three zero bytes; `Finish()` is the hash, equal to `Zenith_Hash64` of the concatenated stream (`Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_Hash64"). No scratch is used, so the step cannot fail. `Zenith_GraphStructureHash(graph)` runs exactly this code (no arena, no failure path). `Zenith_GraphCanonicalBytes(graph, arena)` writes the same bytes into `arena` and returns them; it is the one function of the three that can fail, returning `ALLOC_ERROR_CAPACITY_EXCEEDED` (or the arena's other `Zenith_AllocError`) when the arena is exhausted (Z2-R-GRAPH-047).

### Step 13 diagnostics assembly

1. Convert every error record and every derived error into a `Zenith_GraphDiagnostic`, formatting its text with `Zenith_GraphTextWriter` into the schedule arena (§ "Other diagnostics").
2. A capacity-failed graph contributes exactly one `GRAPH_ERROR_CAPACITY` diagnostic (variant `ARENA`, with N, R, A and the graph name) in addition to its earlier records.
3. Sort by `(m_eCode, m_uNodeA, m_uNodeB, m_uResource)` with a stable sort (ties keep recording order).
4. Set `xReportOut.m_axDiagnostics` to the sorted span (in the schedule arena, valid until it is reset, [Z2-N-ARCH-349]) and return `Zenith_Err(Zenith_GraphCompileError{ m_eCode = the first diagnostic's code, m_uDiagnosticCount = the span's count, m_ulStructureHash = step 12's hash })`. The record carries no text. On success `xReportOut.m_axDiagnostics` is set to an empty span.

If the schedule arena cannot hold the diagnostics, the emergency diagnostic reserved in step 0 is filled with the first diagnostic's code and the text `ZenithGraph: <n> errors; schedule arena exhausted while formatting them`, and the report is that one entry (count 1); if step 0's reservation itself failed, the report is a one-entry span over the static diagnostic `{ GRAPH_ERROR_CAPACITY, …, "ZenithGraph: schedule arena exhausted before compile" }` and the record is `{ GRAPH_ERROR_CAPACITY, 1, hash }` (Z2-R-GRAPH-014).

## Reprioritisation

`Zenith_GraphReprioritise(xSchedule, xNewCosts, xArena, xReportOut)` (Z2-R-GRAPH-046):

1. `ZENITH_CHECK(xNewCosts.GetCount() == xSchedule.m_auPosition.GetCount())` (one cost per node index; a mismatch is a front-end defect, never a runtime condition). The only failure of the remaining steps is arena exhaustion, which writes one `GRAPH_ERROR_CAPACITY` (variant `ARENA`) diagnostic into `xReportOut` and returns `{ GRAPH_ERROR_CAPACITY, 1, the schedule's hash }`.
2. Recompute priorities over the schedule's kept edges in reverse of the old order (a valid topological order of the kept edges): the same formula as step 7.
3. Recompute the order with the step 8 heap over the kept edges and the stored dependency counts (identical to Compile's order, § "Step 8 execution order").
4. Recompute positions and lifetimes from the new order (lifetimes are positions, so they follow the order; the accesses that bound each lifetime do not change).
5. Copy edges, successor offsets, dependency counts, accesses, access offsets, subranges, hash and counters into `xArena` unchanged, so the result holds no pointer into the input schedule's arena.

The result is byte-identical to `Compile` of the same declarations with the new costs (`Test_GraphConformance_ReprioritiseMatchesCompile`). Because the result is self-contained, a front end that compiled into a temporary arena calls `Zenith_GraphReprioritise` with the declared costs to move the schedule into a long-lived arena and then resets the temporary one (the CPU cache does this, `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Acquire"). O((N + E) log N + A).

This is [Z2-N-ARCH-339] as specified: edges, dependency counts and the accesses that bound each lifetime are unchanged, and positions, and therefore `m_axLifetimes`, follow the new order.

## JSON writing

`WriteJson(xWriter, pxSchedule, pxTimings)` (Z2-R-GRAPH-054, Z2-R-GRAPH-055, Z2-R-GRAPH-060):

0. The caller constructs the writer on its own `Zenith_Array<char>` with the style it wants and calls `Finish` after `WriteJson` returns (a dump embedded under a front end's `core` key is written into the front end's writer, mid-document). Every golden file uses `JSON_WRITE_STYLE_COMPACT`, whose bytes contain no whitespace; `JSON_WRITE_STYLE_PRETTY` output is validated against the schema and never compared byte for byte (`Zenith2/Docs/Design/CORE/Text.md` → "JSON writer", "Byte format").
1. If `pxSchedule` is not null, `ZENITH_CHECK(pxSchedule->m_ulStructureHash == Zenith_GraphStructureHash(*this) && pxSchedule->m_auPosition.GetCount() == GetNodeCount())`.
2. Emit the keys in this order, following the presence rules of `Zenith2/Docs/Design/GRAPH.md` → "Graph dump schema zenith2.graph-dump version 1": `schema` (`zenith2.graph-dump`), `schemaVersion` (1), `graph` (name), `frontEnd` (tag), `frame`, `structureHash`, `nodes`, `resources`, `accesses`, `orderings`, `edges`, `schedule`, `timings`.
3. `nodes[i]` keys in order: `id`, `name`, `lane`, `cost` (declared, unclamped), `flags`, `culled`, `position`, `priority`, `dependencyCount`, `payload`. `resources[i]`: `id`, `name`, `subresources`, `flags`, `latestVersion`, `lifetime` (`{ "first", "last" }` or `null`), `payload`. `accesses[i]` (declaration order): `node`, `resource`, `mode`, `versionRead`, `versionWritten`, `subranges` (expanded), `usage`. `orderings[i]`: `before`, `after`, `reason`. `edges[i]` (kept edges, schedule order): `from`, `to`, `reason`, then `resource`, `version` or `ordering`. `schedule`: `order`, `culled`, `reducedEdges`, `reductionSkipped`. `timings[i]` in the given order: `node`, `worker`, `startNs`, `endNs`.
4. Flags are arrays of names in ascending bit order without the `GRAPH_*_FLAG_` prefix; enumerations use `Zenith_ToString`.
5. Array layouts (the argument of every `BeginArray`): `JSON_ARRAY_LAYOUT_MULTILINE` for `nodes`, `resources`, `accesses`, `orderings`, `edges` and `timings`; `JSON_ARRAY_LAYOUT_INLINE` for every `flags` array, `subranges`, each `[first, count]` pair and `schedule.order`. In COMPACT style both layouts produce the same bytes; the layout matters only for PRETTY.
6. Numbers: every integer below 2^53 via `UInt`; a priority at or above 2^53 via `String` of its `0x%016x` form; `structureHash` and every `payload` always as that hexadecimal string.
7. Return `xWriter.GetStatus()`.

The writer allocates nothing itself; text for hexadecimal strings is formatted into a 19-byte stack buffer. O(N + R + A + E + T).

`Zenith_GraphDumpVersionIsReadable(v)` is `v >= 1 && v <= uGRAPH_DUMP_SCHEMA_VERSION`. `Zenith_GraphDumpVersionMessage(v, arena)` formats `ZenithGraph: dump schemaVersion <v> is newer than the supported version 1` for `v` above the supported version and `ZenithGraph: dump schemaVersion <v> is not a valid version` for 0 (Z2-R-GRAPH-057).

## Diagnostic text formats

### Text writer

```cpp
// Engine/ZenithGraph/Internal/Zenith_GraphText.h
class Zenith_GraphTextWriter
{
public:
	explicit Zenith_GraphTextWriter(Zenith_Arena& xArena);
	void Append(Zenith_StringView strText);
	void AppendQuoted(Zenith_StringView strText);   // 'text'
	void AppendDecimal(uint64 ulValue);
	void AppendHex64(uint64 ulValue);             // 0x + 16 lower-case digits
	void AppendRange(uint32 uFirst, uint32 uCount);   // [first,first+count)
	void NewLine();                                  // '\n'
	Zenith_Result<Zenith_StringView, Zenith_AllocError> Finish();
};
```

The writer appends to a `Zenith_Array<char>` in the arena tier (`Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_Array") and `Finish` returns a view of it; a failed append is sticky and `Finish` then returns `ALLOC_ERROR_CAPACITY_EXCEEDED` or the array's error. It depends on no formatting library ([Z2-N-CONTRACT-271] allows no `<cstdio>`). Lines are separated by `\n`, with no trailing newline.

### Cycle block

The format is the specification's (`Zenith2/Docs/Architecture/ZenithGraph.md` → "Cycle detection and diagnostics", [Z2-N-ARCH-348]), made exact (Z2-R-GRAPH-038). Byte rules: lines are separated by one LF with no trailing LF; header lines of edges begin with two spaces; detail lines begin with exactly six spaces; the node pair, the reason and the resource are separated by exactly two spaces; no tab character appears in any diagnostic text.

```text
ZenithGraph: cycle among <k> nodes in graph '<graph name>'
  [<i>] '<from>' -> '<to>'  <REASON>  '<resource name>' v<version>
      <detail line>
  [<i>] '<from>' -> '<to>'  ORDERING
      After('<from>') declared by '<to>': "<reason>"
  break the cycle by removing one of these accesses or orderings
```

- `<k>` is the strongly connected component's node count (it may exceed the path length); `<i>` counts path edges from 1.
- Detail lines per reason (`<mode>` is `Zenith_ToString` of the access mode):
  - RAW: `write by '<from>' (<mode of from's access>), read by '<to>' (<mode of to's access>)`
  - WAR: `read by '<from>' (<mode> v<version>), next write by '<to>' (<mode> -> v<version + 1>)`
  - WAW: `write by '<from>' (<mode> -> v<version>), next write by '<to>' (<mode> -> v<version + 1>)`
  - ORDERING: `After('<from>') declared by '<to>': "<reason>"`, where `<from>` and `<to>` are the edge's two core node names and `<reason>` is the ordering's reason string verbatim (`""` when it is empty); a front end supplies no other phrasing (`Zenith2/Docs/Architecture/ZenithGraph.md` → "Cycle detection and diagnostics"). The CPU front end's reasons are `explicit Before` and `explicit After` for declared orderings and `stage <s> fence` or `stage <s> -> <s'>` for fence orderings (`Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Step 4 ordering resolution", "Step 3 stage fences").
- An access declared with explicit subranges appends ` [<first>,<first+count>)` once per subrange, in ascending order of `<first>`, directly after its closing parenthesis; an access declared with an empty span (the whole resource) appends nothing.
- For a RAW, WAR or WAW edge the accesses named are the first ones in declaration order of each node on that resource and version.


### Other diagnostics

| Code | Variant | Text |
|---|---|---|
| `GRAPH_ERROR_VERSION_FORK` | — | `ZenithGraph: resource '<res>' version <v> is written by both '<nodeA>' and '<nodeB>'; the second writer must use the handle returned by the first` |
| `GRAPH_ERROR_UNDEFINED_READ` | — | `ZenithGraph: '<node>' reads subresources [<first>,<first+count>) of '<res>' v<v>, which no node has written` |
| `GRAPH_ERROR_FIRST_WRITE_NOT_DISCARD` | — | `ZenithGraph: first write of '<res>' by '<node>' is <mode> over [<ranges>]; the first write must be WRITE_DISCARD over every subresource` with `<ranges>` the access's subranges as `[a,b)` joined by `, ` |
| `GRAPH_ERROR_INVALID_HANDLE` | `HANDLE` | `ZenithGraph: '<call>' received a handle from graph serial <s> (this graph is <t>) or index <i> >= <count>` |
| `GRAPH_ERROR_INVALID_HANDLE` | `BAD_MODE` | `ZenithGraph: '<call>' received access mode <m>, which is not valid for it` |
| `GRAPH_ERROR_SUBRANGE_OUT_OF_BOUNDS` | `RANGE` | `ZenithGraph: '<node>' declares subrange [<first>,<first+count>) on '<res>', which has <n> subresources` |
| `GRAPH_ERROR_SUBRANGE_OUT_OF_BOUNDS` | `ZERO_SUBRESOURCES` | `ZenithGraph: resource '<res>' declares 0 subresources` |
| `GRAPH_ERROR_SELF_ORDERING` | — | `ZenithGraph: ordering '<node>' before itself ("<reason>")` |
| `GRAPH_ERROR_CAPACITY` | `ARENA` | `ZenithGraph: arena exhausted after <n> nodes, <r> resources and <a> accesses in graph '<name>'` |
| `GRAPH_ERROR_CAPACITY` | `LIMIT` | `ZenithGraph: <what> limit <limit> exceeded in graph '<name>'` with `<what>` one of `node`, `resource`, `access`, `ordering`, `subresource`, `version` |
| `GRAPH_ERROR_CAPACITY` | `NAME_TOO_LONG` | `ZenithGraph: name of <n> bytes exceeds 255 in graph '<name>'` |

`<call>` is the member name (`AddOrdering`, `DeclareRead`, `DeclareWrite`, `DeclareAccess`, `MarkOutput`); a node or resource is named by its name, or `#<index>` when its name is empty. The first four rows and `SELF_ORDERING`, `CAPACITY`/`ARENA` are the specification's formats verbatim; the variants are elaborations (Z2-R-GRAPH-040).

### Worked examples

A three-node CPU cycle as the core formats it (graph `CPUFrame/Tick`; world nodes carry the `@W<group>` suffix the CPU lowering adds, resources the names ECS gives them, `<TypeName>.<property>@W<tag>`); every detail line starts with six spaces:

```text
ZenithGraph: cycle among 3 nodes in graph 'CPUFrame/Tick'
  [1] 'Physics.Step@W1' -> 'Animation.PostPhysicsBlend@W1'  RAW  'Zenith_RigidBody.WorldTransform@W1' v4
      write by 'Physics.Step@W1' (WRITE), read by 'Animation.PostPhysicsBlend@W1' (READ)
  [2] 'Animation.PostPhysicsBlend@W1' -> 'Gameplay.CameraRig@W1'  ORDERING
      After('Animation.PostPhysicsBlend@W1') declared by 'Gameplay.CameraRig@W1': "explicit After"
  [3] 'Gameplay.CameraRig@W1' -> 'Physics.Step@W1'  WAR  'Zenith_PhysicsWorld@W1' v2
      read by 'Gameplay.CameraRig@W1' (READ v2), next write by 'Physics.Step@W1' (READ_WRITE -> v3)
  break the cycle by removing one of these accesses or orderings
```

The same cycle as the CPU front end reports it (one `FRAME_GRAPH_ERROR_GRAPH` diagnostic) begins `frame graph: Tick: ZenithGraph: cycle among 3 nodes …`, and at the ECS level `schedule: frame graph: Tick: ZenithGraph: cycle among 3 nodes …`. A cycle through a stage fence shows the fence as a node, `'Sync.Tick.G1.S5'`, with the ordering reason `stage 5 -> 6`.

A read with explicit subranges in a Flux cycle: `      write by 'Bloom.Down0' (WRITE_DISCARD) [0,1), read by 'Bloom.Down1' (READ) [0,1)`.

A version fork: node `A` writes `Tex` from version 0 (producing 1), node `B` writes `Tex` again with the version-0 handle:

```text
ZenithGraph: resource 'Tex' version 0 is written by both 'A' and 'B'; the second writer must use the handle returned by the first
```

## Complexity and memory summary

| Step | Time | Schedule-arena memory |
|---|---|---|
| declarations | O(1) amortised per call, O(S) subranges | records: 40 N + 48 R + 32 A + 8 S + 24 O + 32 D_err bytes plus names, up to 3× with `Zenith_Array` growth, in the graph arena |
| 1 flatten and group | O(A log A + R + N) | 4 (R + 1) + 4 A + 32 A (events, twice for the stable-sort scratch) + results 32 A + 4 (N + 1) + 8 S |
| 2 hazards | O((A + R) log A) with bounded fragmentation | intervals 20 × (2 W_r + 1) for the largest resource (reused), reader pool 8 L with L = Σ_reads min(S_a, 2 W_r + 1), candidates 24 E_c (up to 3× with growth) |
| 3 edges | O(E log E) | merge-sort buffer 24 E_c, CSR 4 (N + 1) |
| 4 and 5 cycles | O(N + E) | 16 N |
| 6 cull | O(N + E) | reverse CSR 4 (N + 1) + 4 E, flags N |
| 7 and 8 priority and order | O(N + E + N log N) | 8 N priorities, 4 N order, 4 N positions, heap 12 N |
| 9 reduction | O(N' · E / 64) | N'² / 8 bytes (2 MiB at the default limit of 4,096) |
| 10 counts | O(N + E) | results 24 E + 4 (N + 1) + 4 N |
| 11 lifetimes | O(A) | results 8 R |
| 12 hash | O(N + R + A + O + name bytes) | none (streamed) |
| 13 diagnostics | O(D log D + text) | 32 D + text |

For the 10,000-node, 40,000-access benchmark graph the schedule arena peaks below 32 MiB (reduction skipped above 4,096 nodes). Front ends size their schedule arenas' `m_ulMaxBytes` from these formulas with a factor of 2 (Z2-R-GRAPH-049).
