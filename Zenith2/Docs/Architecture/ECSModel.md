# Architecture — ECS model

This file is part of the Architecture document family. The entry document is `Zenith2/Docs/Architecture.md`; this file holds the full specification of its section "ECS model". Normative statements carry `[Z2-N-ARCH-NNN]` tags in the range 681–759. The Luau side of the same lifetimes (transactions, shadows, views in scripts) is specified in `Zenith2/Docs/Scripting.md → "Transaction scope and the entity boundary"` and the sections after it; this file states the ECS-side contract those rely on.

## Settled rules and ownership

Three ECS backends — Custom, flecs and EnTT through an adapter — are all delivered in Stage 2 behind one type-erased API (Z2-D-039). Entities and components are referenced by world handles (Z2-D-040, Zenith2/Docs/Contracts.md → "Handle kinds"). Structural changes are deferred to sync points; the transaction boundary for scripts is the entity (Z2-D-051); simulation results are independent of worker count, chunk layout and scheduling (Z2-D-019).

| Library | Contents |
|---|---|
| `ZenithECS` | `Zenith_World` (the backend interface), `Zenith_WorldSimulation` (the per-world systems, commands and playback driver), `Zenith_EntityHandle`, `Zenith_ComponentHandle`, `Zenith_CommandBuffer`, queries, chunk views, `Zenith_SystemRegistry`, `Zenith_MergeContext`, `Zenith_WorldRegistry`, the command-kind declarations and table (`Zenith_CommandKinds.h`: `Zenith_CommandKindDesc`, `Zenith_PfnCommandPlayback`, `Zenith_CommandKindTable`), `Zenith_EpilogueStageDesc` (`Zenith_System.h`), the tick-boundary access and observers (`Zenith_ECSObservers.h`), the conformance suite |
| `ZenithECS_Custom` | archetype chunk storage designed for runtime layouts; `Zenith_CreateWorld_Custom` |
| `ZenithECS_Flecs` | flecs integration; `Zenith_CreateWorld_Flecs` |
| `ZenithECS_EnTT` | EnTT integration with the dynamic-storage adapter; `Zenith_CreateWorld_EnTT` |

- [Z2-N-ARCH-681] Every ECS operation outside a backend library is expressed through `ZenithECS` types keyed by reflection IDs (`Zenith_TypeID`, `Zenith_PropertyID`); no flecs or EnTT type, header or symbol appears outside that backend's `Internal/` directory (Zenith2/Docs/Contracts.md → "API boundary").
- [Z2-N-ARCH-682] The game selects its ECS backend at link time through `backends.ecs` in `game.json`; the conformance executable `ZenithECS_Conformance` links all three and runs every scenario on each.

## Worlds and backends

### The backend interface

```cpp
// Engine/ZenithECS/Zenith_World.h
struct Zenith_EntityTableDesc
{
	uint32 m_uInitialGeneration = 0;               // generation of every slot's first issue; computed by Zenith_WorldRegistry, never 0
};

struct Zenith_WorldDesc
{
	uint16 m_uWorldTag = 0;                        // issued by Zenith_WorldRegistry; 1 … 4095
	Zenith_EntityTableDesc m_xEntityTable;          // the generation offset issued with the tag
	const Zenith_TypeRegistry* m_pxTypes = nullptr; // frozen; outlives the world
	Zenith_Heap* m_pxHeap = nullptr;                // general heap for storage
	uint32 m_uChunkCapacityRows = 0;               // 0 = backend default; tests set other values
	uint32 m_uMaxEntities = 1u << 20;              // at most 2^24 (Zenith2/Docs/Contracts.md → "Handle kinds")
	uint32 m_uContainerHeapBytes = 64u << 20;      // out-of-line field storage
	uint64 m_ulWorldSeed = 0;                      // script randomness and procedural seeds
	Zenith_FaultInjector* m_pxFaults = nullptr;     // the factory's fault injector ([Z2-N-CONTRACT-225]); null in production
	bool m_bEditWorld = false;                      // the edit-world flag, set by the world's creator from its purpose (true exactly
	                                                // for WORLD_PURPOSE_EDIT); read back by Zenith_World::IsEditWorld()
};

// Contract: implemented by one backend library each. Thread affinity: PHASE-dependent —
// chunk reads and declared-column writes from frame-graph nodes; structural calls only through
// Zenith_WorldPlaybackAccess on the world's sync-point node; EXTERNALLY_SYNCHRONISED otherwise.
class Zenith_World
{
public:
	virtual ~Zenith_World() = default;

	virtual uint16 GetWorldTag() const = 0;
	virtual Zenith_Result<Zenith_EntityLocation, Zenith_ECSError> Resolve(Zenith_EntityHandle xEntity) const = 0;
	virtual Zenith_Result<Zenith_QueryID, Zenith_ECSError> CompileQuery(const Zenith_QueryDesc& xDesc) = 0;
	virtual uint32 GetChunkCount(Zenith_QueryID xQuery) const = 0;
	virtual Zenith_ChunkView GetChunk(Zenith_QueryID xQuery, uint32 uChunkOrdinal,
		const Zenith_InvocationScope& xScope) = 0;
	virtual Zenith_WorldPlaybackAccess& GetPlaybackAccess(const Zenith_SyncPointToken& xToken) = 0;
	virtual void RunEndOfTick(const Zenith_SyncPointToken& xToken) = 0;
	virtual Zenith_WorldStats GetStats() const = 0;

	// Test and research hooks, compiled in every configuration, reachable only through Zenith_WorldTestAccess.
	virtual void ForceCompaction(const Zenith_SyncPointToken& xToken) = 0;
};

// Engine/ZenithECS_Flecs/Zenith_ECS_Flecs.h (one per backend, same shape)
Zenith_Result<Zenith_Owned<Zenith_World>, Zenith_ECSError> Zenith_CreateWorld_Flecs(
	const Zenith_WorldDesc& xDesc);
```

`Zenith_SyncPointToken` has a private constructor befriended by `Zenith_WorldSimulation` and `Zenith_WorldTestAccess`, so structural access is reachable from sync-point playback, world loading before the first tick, and the test access type.

```cpp
// Engine/ZenithECS/Zenith_World.h (continued)
struct Zenith_QueryID { uint32 m_uIndex = 0xFFFFFFFFu; };      // world-local, valid for the world's lifetime

// Contract: a capability value; carries nothing but proof of the caller. Not copyable outside its friends.
class Zenith_SyncPointToken
{
private:
	Zenith_SyncPointToken() = default;
	friend class Zenith_WorldSimulation;
	friend class Zenith_WorldTestAccess;
};

// Contract: test and research code alone (TestSeparationCheck condition 6, Zenith2/Docs/Contracts.md → "Test separation",
// fails a non-test caller); MAIN_THREAD between ticks.
class Zenith_WorldTestAccess
{
public:
	static void ForceCompaction(Zenith_World& xWorld);                               // at a sync point of the test driver
	static Zenith_WorldPlaybackAccess& Playback(Zenith_World& xWorld);   // direct structural setup in tests
};
```

```cpp
// Contract: the per-world driver; MAIN_THREAD for creation and destruction, sync-point node for playback.
class Zenith_WorldSimulation
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_WorldSimulation>, Zenith_ECSError> Create(
		Zenith_Owned<Zenith_World> xWorld, Zenith_Owned<Zenith_SystemRegistry> xSystems, Zenith_Heap& xHeap);

	Zenith_World& GetWorld();
	Zenith_Result<void, Zenith_ScheduleError> EmitInto(Zenith_FrameGraphBuilder& xBuilder);
	uint64 GetTickIndex() const;
	uint32 GetChangeVersion() const;      // in a frame: the merge version of the latest completed sync
	                                                              // point; out of tick: V(t, M) of the current tick t
	uint64 GetNextSimulationID() const;   // part of saved and replayed world state
};
```

- [Z2-N-ARCH-683] A structural change (spawn, destroy, add or remove component) is applied only through `Zenith_WorldPlaybackAccess`, which requires a `Zenith_SyncPointToken`; systems and scripts reach structure only by recording commands.
- [Z2-N-ARCH-684] A backend's factory unwinds everything it constructed when a subsequent construction step fails and returns the error (Zenith2/Docs/Contracts.md → "Threading and concurrency contract"); each construction step of each backend has a failure-injection test.

### Playback access and supporting types

```cpp
// Engine/ZenithECS/Zenith_ECSTypes.h
enum Zenith_ECSErrorCode : uint8
{
	ECS_ERROR_STALE_HANDLE = 1,      // generation mismatch or destroyed entity
	ECS_ERROR_WRONG_WORLD,           // the handle's world tag is another world's
	ECS_ERROR_NO_COMPONENT,          // the entity lacks the component
	ECS_ERROR_UNKNOWN_TYPE,          // the type ID is not registered, or not TYPE_KIND_COMPONENT
	ECS_ERROR_UNKNOWN_PROPERTY,
	ECS_ERROR_CAPACITY,              // entity, container-heap or query capacity exhausted
	ECS_ERROR_OUT_OF_MEMORY,
	ECS_ERROR_WORLD_TAGS_EXHAUSTED,  // Zenith_WorldRegistry::AcquireTag found all 4,095 tags live
	ECS_ERROR_BACKEND,               // a backend-internal failure; the backend logs its text at the point of failure
	ECS_ERROR_NULL_HANDLE,           // the null handle was resolved
	ECS_ERROR_INVALID_DESC,          // a desc field is outside its range (a Zenith_WorldDesc, a command-kind desc)
	ECS_ERROR_VALUE_SIZE,            // a property value's byte count differs from the property's reflected size
	ECS_ERROR_PHASE_NOT_SCHEDULABLE, // value 13: AddSystem received PHASE_RENDER_EXTRACTION, PHASE_COMMANDS_ONLY or a value above 11
};

// The error record of [Z2-N-CONTRACT-480]: the type and property involved, by ID; names are logged, not carried.
struct Zenith_ECSError
{
	Zenith_ECSErrorCode m_eCode;
	Zenith_PropertyID m_xProperty;   // zero when not applicable
	Zenith_TypeID m_xType;           // zero when not applicable
};

struct Zenith_PropertyValue
{
	const Zenith_PropertyID m_xProperty = {};
	const Zenith_ByteSpan m_xValue = {};        // the property's value in its reflected value type, not in any layout
};

struct Zenith_ComponentInit
{
	Zenith_TypeID m_xType;
	Zenith_Span<const Zenith_PropertyValue> m_axValues;   // absent properties take their registered defaults
};

struct Zenith_SpawnTicket { uint32 m_uBufferLocalIndex = 0xFFFFFFFFu; };   // resolved to a handle at playback
struct Zenith_QuerySlot { uint32 m_uIndex = 0xFFFFFFFFu; };                // bound at registry freeze

struct Zenith_EntityLocation
{
	const uint64 m_ulSimulationID = 0;
	const uint32 m_uChunkOrdinal = 0;         // backend-internal; valid until the next structural playback
	const uint32 m_uRow = 0;
	const bool m_bDestroyPending = false;
};

struct Zenith_WorldStats
{
	uint32 m_uEntities, m_uChunks, m_uArchetypes;
	uint64 m_ulComponentBytes, m_ulContainerHeapBytes;
};

// Contract: OWNER_THREAD — the world's sync-point node (or world loading before the first tick).
// Reentrancy: NONE. Every mutation is applied immediately to storage; no view is live while it runs.
class Zenith_WorldPlaybackAccess
{
public:
	virtual ~Zenith_WorldPlaybackAccess() = default;
	virtual uint16 GetWorldTag() const = 0;   // the world being played back (Zenith_World::GetWorldTag)
	virtual Zenith_Result<Zenith_EntityHandle, Zenith_ECSError> CreateEntity(uint64 ulSimulationID,
		Zenith_Span<const Zenith_ComponentInit> axComponents) = 0;
	virtual Zenith_Result<void, Zenith_ECSError> AddComponent(Zenith_EntityHandle xEntity,
		const Zenith_ComponentInit& xComponent) = 0;
	virtual Zenith_Result<void, Zenith_ECSError> RemoveComponent(Zenith_EntityHandle xEntity, Zenith_TypeID xType) = 0;
	// Writes the value and marks the row changed (MarkRowChanged semantics) in the playback's own pending bits;
	// the sync-point node commits them with CommitRowChanges after playback step 5.
	virtual Zenith_Result<void, Zenith_ECSError> SetProperty(Zenith_EntityHandle xEntity, Zenith_TypeID xType,
		Zenith_PropertyID xProperty, Zenith_ByteSpan xValue) = 0;
	virtual Zenith_Result<Zenith_ByteSpan, Zenith_ECSError> ReadProperty(Zenith_EntityHandle xEntity, Zenith_TypeID xType,
		Zenith_PropertyID xProperty) const = 0;
	virtual Zenith_Result<void, Zenith_ECSError> MarkDestroyPending(Zenith_EntityHandle xEntity) = 0;
	virtual bool IsDestroyPending(Zenith_EntityHandle xEntity) const = 0;
};
```

- [Z2-N-ARCH-716] Playback steps 5 to 7 of Architecture/FramePhases.md → "Sync points and the tick boundary" mutate storage only through `Zenith_WorldPlaybackAccess`, whose failures are recoverable results logged with the command that caused them, never asserts; its `GetWorldTag` names the world being played back, the one key by which a module command's playback (Architecture/CompositionRoot.md → "Module interfaces", `Zenith_PfnCommandPlayback`) finds its per-world module state.
- [Z2-N-ARCH-717] Initial component values travel as `Zenith_ComponentInit` property-value sets keyed by property ID, never as bytes of a C++ layout, so a spawn or an added component is independent of every backend's storage layout (Z2-D-038).

### Backend storage models

<!-- elaboration -->
| Backend | Chunk | Default chunk capacity | Column layout | Structural change cost |
|---|---|---|---|---|
| Custom | an archetype chunk of 16 KiB | rows = ⌊(16,384 − header) / row bytes⌋, at most 1,024 | one column per property (structure of arrays) | row appended at the end of a chunk with space; removal swaps the last row in |
| flecs | a slice of up to capacity rows of a flecs table | 256 rows | one component per table column (array of structures); a property column has the component's stride | flecs table move |
| EnTT | a synthetic chunk: a run of up to capacity entities of an owning group, or a gathered run of a view with a row-index table | 256 rows | array of structures per pool; non-group queries add a row-index indirection | pool insert or erase, group re-sort |

- [Z2-N-ARCH-685] Every backend presents chunks through the same `Zenith_ChunkView`: a row count, one `Zenith_RawColumn` per declared property (pointer, stride, optional row-index table), the entity handle and stable simulation ID of each row, the chunk's structural epoch and each column's change version.
- [Z2-N-ARCH-686] Chunk capacity is configurable per world (`m_uChunkCapacityRows`) and forced compaction is available to tests, so the determinism tests run the same replay with two different capacities and with compaction forced after every sync point.

## Entity identity

| Identity | Type | Scope | Assigned | Serialized |
|---|---|---|---|---|
| entity handle | `Zenith_EntityHandle` (8 bytes: slot, generation, world tag; Zenith2/Docs/Contracts.md → "Handle kinds") | the issuing world | at spawn playback, in merge order | never |
| stable simulation ID | `uint64` | the world session | at spawn playback, in merge order, from the world's monotonic counter; never reused | yes, in saves and replays (with the counter) |
| persistent ID | `Zenith_PersistentID` | global | at authoring, or by the spawn descriptor | yes |
| network ID | `Zenith_NetworkID` | the session | by the authority (Architecture.md → "Multiplayer foundations") | in network messages |

- [Z2-N-ARCH-687] The stable simulation ID is the identity every deterministic ordering and every script random key uses; it never depends on slot index, chunk, row or storage order (Zenith2/Docs/Scripting.md → "Determinism rules and randomness").
- [Z2-N-ARCH-688] Slot indices are allocated at spawn playback in merge order from the world's free list, lowest free index first, so slot assignment is itself deterministic; no ordering or result depends on slot index nonetheless.

## Command buffers

```cpp
// Engine/ZenithECS/Zenith_CommandBuffer.h
struct Zenith_MergeKey
{
	uint32 m_uSystemOrder = 0;           // the recording system's registration index
	uint64 m_ulSimulationID = 0;         // the entity transaction's stable simulation ID; 0 for non-entity work
	uint32 m_uInvocationOrdinal = 0;     // the ordinal of the invocation within that entity's tick
	uint32 m_uSequence = 0;              // the record's position within the entity transaction
};

// Contract: OWNER_THREAD — one per invocation (or per entity transaction), arena-backed,
// committed to the world only by the recording invocation's commit.
class Zenith_CommandBuffer
{
public:
	Zenith_SpawnTicket Spawn(const Zenith_SpawnDesc& xDesc);
	void Destroy(Zenith_EntityHandle xEntity);
	void AddComponent(Zenith_EntityHandle xEntity, const Zenith_ComponentInit& xComponent);
	void RemoveComponent(Zenith_EntityHandle xEntity, Zenith_TypeID xType);
	void SetProperty(Zenith_EntityHandle xEntity, Zenith_TypeID xType, Zenith_PropertyID xProperty,
		Zenith_ByteSpan xValue);
	void Record(uint32 uModuleCommandKind, Zenith_ByteSpan xPayload);   // registered module commands
	// other lifecycle commands are free recording functions over Record(): Architecture.md → "Lifecycle commands"
	Zenith_Result<void, Zenith_ECSError> Reserve(uint32 uBytes);   // capacity reserved during the invocation
};
```

Each record is `{ uint16 m_uKind; uint16 m_uFlags; uint32 m_uPayloadBytes; Zenith_MergeKey m_xKey; }` followed by its payload, 8-byte aligned, in the invocation's arena.

- [Z2-N-ARCH-689] Command buffers from parallel invocations are merged in lexicographic `Zenith_MergeKey` order — (system order, logical invocation ID = (stable simulation ID, invocation ordinal), sequence) — never in completion order.
- [Z2-N-ARCH-690] A C++ system that records commands per entity records them under that entity's stable simulation ID (`Zenith_SystemContext::Commands(ulSimulationID)`), and a record not tied to an entity uses simulation ID 0 with an invocation ordinal the system derives deterministically from its inputs, never from a chunk index, worker index or completion order.
- [Z2-N-ARCH-691] Command-buffer memory, including space for payloads and for the provisional handles of spawns, is reserved during the invocation, so appending a committed buffer to the world at merge never allocates and cannot fail (Zenith2/Docs/Scripting.md → "Commit and failure semantics").
- [Z2-N-ARCH-692] Every system that declares `EmitsCommands`, `EmitsEvent`, a reduction or `ConsumesMergeCapacity`, and every script system, gets a generated merge node after its system node (C++ systems without a merge function get the default merge), so all shared capacity is consumed on the merge chain; the merge nodes of one phase of one world are chained in the stage's cost-independent topological order — Kahn's algorithm over the stage's declared hazard and ordering edges, the chain itself excluded, ties broken by registration index — so a reader registered before the writer of a property it reads still schedules, capacity is consumed in chain order independent of which system finished first, and playback still applies accepted buffers in merge-key order.

## Sync points and the structural epoch

The playback steps of a sync point are specified in Architecture/FramePhases.md → "Sync points and the tick boundary". The ECS-side state they maintain:

| State | Granularity | Changes when | Read by |
|---|---|---|---|
| structural epoch | per chunk, `uint32` | playback adds, removes or moves rows of the chunk, or changes its layout (compaction, archetype change) | view validation |
| invocation ID | per invocation, `uint64` | each system callback, behaviour callback or behaviour segment starts | view validation |
| change version | per (chunk, column), `uint32` | the column is written by an invocation whose writes are committed at a merge point; set to that merge point's precomputed merge version V(t, m); also stamped by playback (bullet 728) | change-filtered queries, replication, extraction |
| merge version V(t, m) | per (tick t, merge-point ordinal m), `uint32` | precomputed at `Freeze` and every recompile: ordinal 0 is the tick boundary, then per stage 1 … 11 its merge points in the stage's topological order (bullet 723) and then its sync point (stage 11: the end-of-tick playback), M the highest ordinal; `V(t, m) = 1 + (U(t, m) mod (2^32 − 1))` with the unwrapped `U(t, m) = B_t + m` over the world's `uint64` tick base B; 0 is never issued | the value assigned above; `Zenith_MergeContext::GetMergeVersion`, `GetReadVersion` |
| world change version | per world, `uint32` | `Zenith_WorldSimulation::GetChangeVersion()`: the merge version of the latest completed sync point in a frame, V(t, M) of the current tick out of tick | out-of-tick playback stamps; change-filtered extraction |
| query structure version | per world, `uint32` | playback creates or removes an archetype, table or chunk | query caches |

- [Z2-N-ARCH-693] A chunk's structural epoch is bumped only when structural commands are played back at a sync point (or by the end-of-tick playback), never by a property write, a commit or a merge.
- [Z2-N-ARCH-694] Change-tracking versions are per column, separate from the structural epoch and from invocation IDs, and they advance at the merge point of the writing invocation, never at the write itself; a failed or rejected invocation advances no version.
- [Z2-N-ARCH-720] `Zenith_RawColumn::MarkRowChanged(uRow)` sets the row's pending change bit in the invocation's private bit set and changes nothing else; at the invocation's merge point a committed entity transaction's bits are folded into the column's change-tracking state and the column's change version advances, while a rolled-back or rejected entity's bits are discarded with its snapshot (Zenith2/Docs/Scripting.md → "Commit and failure semantics").
- [Z2-N-ARCH-721] Every write a script invocation makes through a chunk view calls `MarkRowChanged` for the written row, and a C++ system either calls it per written row or, when it writes every row it iterates, marks the whole column written, which advances the version at merge the same way.
- [Z2-N-ARCH-723] The merge continuation of every invocation that wrote columns calls `Zenith_ChunkView::CommitRowChanges` once per written (chunk, column) with the row bits of its accepted entities alone, and the column's version becomes the precomputed merge version of the writing invocation's merge point (`Zenith_MergeContext::GetMergeVersion()`); the merge points of a stage are ordered by a deterministic topological order of the stage's declared hazards and orderings that ignores cost hints (ties by registration index), and a `QUERY_TERM_CHANGED` query compares against the version of the merge point immediately preceding its system's node in that order (`Zenith_MergeContext::GetReadVersion()`), its own merge's stamp excluded (Zenith2/Docs/Scripting.md → "Commit and failure semantics").
- [Z2-N-ARCH-728] Property commands applied at a sync point mark rows through `Zenith_WorldPlaybackAccess::SetProperty`, and after playback step 5 the sync-point node calls `CommitRowChanges` once per written (chunk, column) with the playback's own `Zenith_InvocationScope` at the sync point's merge version; no call other than `CommitRowChanges` advances a column's change version, except the playback-version stamps made through `Zenith_WorldPlaybackAccess::SetPlaybackVersion` — rows a playback creates, property writes made outside the playback write set in step 6 (redundant additions, ticket-addressed records), columns a move adds to a row, and the destination column version of a moved or compacted row (the newer of its current value and the row's version) — and the re-stamping of `Zenith_World::SweepChangeVersions` (the version wrap sweep) and `Zenith_WorldSimulation::RestoreSimulationState`.
- [Z2-N-ARCH-729] Tick-boundary handlers reach the world through the property-only `Zenith_TickBoundaryAccess` (`SetProperty`, `ReadProperty`, `ReadContainer`, `SetContainer`, `HasComponent`, `IsDestroyPending`, `FindByPersistentID`, `GetEntityTable`; no structural member; `SetContainer` is a property write that reserves its own container block and returns `ECS_ERROR_CAPACITY`, changing nothing, when the reservation is refused), and the property writes of tick-boundary steps 1 to 3 are committed after step 3 through `CommitRowChanges` at the tick boundary's merge version V(t, 0), with no epoch, query-structure-version or query-cache change (Architecture/FramePhases.md → "Sync points and the tick boundary").
- [Z2-N-ARCH-695] A commit never invalidates another invocation's view: commits change column contents and change versions, which views do not validate against, and never structural epochs.

## Views

```cpp
// Engine/ZenithECS/Zenith_ChunkView.h
struct Zenith_InvocationScope
{
	uint64 m_ulInvocationID = 0;
};

/// @contract
/// Ownership: a borrowed view of one property column of one chunk; owns nothing.
/// Thread affinity: OWNER_THREAD — the invocation that obtained it.
/// Invalidation: with its chunk view (invocation end, or a structural epoch change).
/// Units: rows; m_uStride in bytes.
/// Errors: MarkRowChanged or MarkAllRowsChanged on a non-writable column, or a row >= m_uRowCount, is a programmer error (ZENITH_ASSERT);
///         script misuse is caught before this call by the view layer and raised as a Luau error.
/// Version: 1.
/// @endcontract
struct Zenith_RawColumn
{
	uint8* m_pData = nullptr;              // row 0; null when the property is not declared by this invocation
	uint32 m_uStride = 0;                  // bytes between consecutive rows
	uint32 m_uRowCount = 0;
	const uint32* m_puRowIndex = nullptr;  // null = identity; otherwise row r lives at m_puRowIndex[r]
	uint64* m_pulRowChangeBits = nullptr;  // the invocation's pending per-row change bits for this column
	bool m_bWritable = false;               // the invocation declared a write of this property

	// Records that row uRow of this column was written by the current invocation. Sets the row's
	// pending change bit only; the column's change version advances at the invocation's merge point.
	void MarkRowChanged(uint32 uRow);

	// Records that every row of this column was written by the current invocation (a C++ system that
	// writes each row it iterates); same merge-time semantics as MarkRowChanged.
	void MarkAllRowsChanged();
};

// Contract: OWNER_THREAD, valid for the invocation named by its scope; never stored beyond it.
class Zenith_ChunkView
{
public:
	uint32 GetRowCount() const;
	Zenith_RawColumn GetColumn(Zenith_TypeID xType, Zenith_PropertyID xProperty) const;
	const Zenith_EntityHandle* GetEntities() const;
	const uint64* GetSimulationIDs() const;
	uint32 GetStructuralEpoch() const;
	uint32 GetChangeVersion(Zenith_TypeID xType, Zenith_PropertyID xProperty) const;
	bool IsValid(const Zenith_InvocationScope& xCurrent) const;   // ID and epoch compare

	/// @contract
	/// Ownership: borrows the view; reads pulAcceptedRowBits (one bit per row, m_uRowCount bits) for the call alone.
	/// Thread affinity: the merge continuation of the invocation's node, after every sub-job has joined; one call per
	///                  (chunk, column) per merge; never concurrent with another invocation writing the same column.
	/// Invalidation: none; the view stays valid until its invocation ends.
	/// Units: rows.
	/// Errors: ECS_ERROR_STALE_HANDLE when the view is no longer valid for xScope; ECS_ERROR_UNKNOWN_PROPERTY when the
	///         column was not declared writable by the invocation. No partial effect on failure.
	/// Version: 1.
	/// @endcontract
	// Commit side of MarkRowChanged: folds the accepted entities' pending row bits into the column's change-tracking
	// state and sets the column's change version to the precomputed merge version of the committing merge point
	// (Zenith_MergeContext::GetMergeVersion() for an invocation; the sync point's or tick boundary's for a playback).
	Zenith_Result<void, Zenith_ECSError> CommitRowChanges(const Zenith_InvocationScope& xScope, Zenith_TypeID xType,
		Zenith_PropertyID xProperty, const uint64* pulAcceptedRowBits);
};

template<typename T> class Zenith_ColumnView;   // typed wrapper over Zenith_RawColumn for C++-registered properties
```

- [Z2-N-ARCH-696] Each view is scoped to the invocation ID that created it and becomes invalid when that invocation ends; `IsValid` compares the view's invocation ID with the current one and its structural epoch with the chunk's.
- [Z2-N-ARCH-697] A C++ system's use of an invalid view is a programmer error caught by `ZENITH_ASSERT` in builds with asserts on; a script's use of a retained or stale view raises a controlled Luau error in every build (Zenith2/Docs/Scripting.md → "Views epochs and misuse errors").
- [Z2-N-ARCH-698] `GetColumn` returns a column only for properties the invocation declared, writable only for properties it declared as writes; an undeclared property yields a null column and, in builds with asserts on, an assert.
- [Z2-N-ARCH-699] String, entity-handle and asset-handle properties are read-only in chunk views and are changed through commands, and container properties are not exposed in chunk views at all; they are reached through the behaviour API's copy-on-write shadows (Zenith2/Docs/Scripting.md → "Component writes shadows and side-effect buffers").

## Queries

```cpp
// Engine/ZenithECS/Zenith_Query.h
enum Zenith_QueryTermOp : uint8
{
	QUERY_TERM_WITH,           // the component is present
	QUERY_TERM_WITHOUT,        // the component is absent
	QUERY_TERM_OPTIONAL,       // columns present when the component is
	QUERY_TERM_CHANGED,        // present and its declared properties changed after the query's last-seen version
};

struct Zenith_QueryTerm
{
	const Zenith_TypeID m_xType = {};
	const Zenith_QueryTermOp m_eOp = QUERY_TERM_WITH;
	const Zenith_Span<const Zenith_PropertyID> m_axProperties = {};   // empty = every property
	const bool m_bWrite = false;
};

struct Zenith_QueryDesc
{
	const char* const m_szName = nullptr;
	const Zenith_Span<const Zenith_QueryTerm> m_axTerms = {};          // 1 to 16 terms
};
```

- [Z2-N-ARCH-700] A query's matched-chunk list is cached and refreshed lazily at its first use after a sync point whose playback changed the world's query structure version, and never during a phase, so a system's chunk list is stable for the whole phase.
- [Z2-N-ARCH-701] A system's queries and its access declaration agree: every property a query term names with `m_bWrite` is declared as a write of the system, and every other named property as a read, checked at `Freeze` of the system registry with `SCHEDULE_ERROR_QUERY_ACCESS_MISMATCH`.
- [Z2-N-ARCH-702] The chunk order a query iterates is the backend's storage order and may differ between runs, capacities and backends; nothing that simulation observes depends on it, because merges, reductions and events are ordered by merge key and fixed reduction trees.

## Deterministic reductions

- [Z2-N-ARCH-703] Reductions and shared structures (sums, counts, spatial grids) are never written in place across entities by parallel invocations: each entity emits a contribution into its invocation's private buffer, and contributions are folded at the merge point in merge order.
- [Z2-N-ARCH-704] A parallel reduction whose result must not depend on worker count uses a fixed-order reduction tree over contributions sorted by merge key (a balanced binary tree over the sorted list, pair-summed left to right), never a per-worker partial sum.

## Type-erased API surface

Every entry point above takes reflection IDs; C++ templates are thin inline wrappers that resolve `T::xTYPE_ID` and property constants and call the type-erased function.

| Operation | Type-erased form | C++ convenience |
|---|---|---|
| resolve a component for reading | `Zenith_World::Resolve` + `Zenith_ChunkView::GetColumn(xType, xProperty)` | `Zenith_ColumnView<T>` |
| declare access | `Zenith_SystemBuilder::Read/Write(xType, axProperties)` | `Zenith_Read<T>(xBuilder)` |
| query | `Zenith_QueryTerm { xType, … }` | `Zenith_QueryTermFor<T>()` |
| set a property | `Zenith_CommandBuffer::SetProperty(xEntity, xType, xProperty, xBytes)` | `Zenith_SetProperty<T>(xBuffer, xEntity, &T::m_xField, xValue)` |
| add a component | `AddComponent(xEntity, Zenith_ComponentInit{ xType, axValues })` | `Zenith_AddComponent<T>(xBuffer, xEntity, const T&)` |

- [Z2-N-ARCH-705] Every public ECS function is type-erased by reflection ID; a template in an ECS public header is an inline wrapper over a type-erased function and adds no storage behaviour, so data-defined components use exactly the same code paths as C++-registered ones.

## World registry

```cpp
// Engine/ZenithECS/Zenith_WorldRegistry.h
struct Zenith_WorldTagIssue
{
	const uint16 m_uTag = 0;                  // 1 … 4095
	const uint32 m_uInitialGeneration = 0;    // copied into Zenith_WorldDesc::m_xEntityTable; never 0
};

// Contract: MAIN_THREAD. Owned by the composition root; passed to whatever creates worlds.
class Zenith_WorldRegistry
{
public:
	Zenith_Result<Zenith_WorldTagIssue, Zenith_ECSError> AcquireTag();   // round-robin over 1 … 4095
	void ReleaseTag(uint16 uTag, uint32 uHighestGenerationIssued);                  // reported by the world at destruction
	uint32 GetLiveWorldCount() const;
};
```

- [Z2-N-ARCH-706] World tags come from the `Zenith_WorldRegistry` object the composition root owns (Zenith2/Docs/Contracts.md → "Handle kinds"); a world never chooses its own tag, and no registry exists as global state.
- [Z2-N-ARCH-724] `Zenith_WorldRegistry` lives in `ZenithECS` and reports failure as `Zenith_ECSError` (`ECS_ERROR_WORLD_TAGS_EXHAUSTED` when every tag is live); the composition root creates it in startup step 10, before the type registry's `Freeze`.
- [Z2-N-ARCH-725] `AcquireTag` issues, with the tag, an initial generation in one of two cases: when the tag was issued before, one above the highest generation any previous world with that tag issued (as reported through `ReleaseTag`), a value above `uECS_MAX_GENERATION` wrapping to 1; when the tag was never issued, the creation-counter offset of Zenith2/Docs/Contracts.md → "Handle kinds" ([Z2-N-CONTRACT-099]); the creator copies it into `Zenith_WorldDesc::m_xEntityTable`, so a handle held from a destroyed world never resolves in a new world that reuses its tag.

## Scheduling contract

### The system registry

```cpp
// Engine/ZenithECS/Zenith_System.h (continued)
// Contract: MAIN_THREAD; one per world, owned by its Zenith_WorldSimulation; registrations are accepted
// until Freeze and again only inside a schedule recompile triggered by a script reload (Architecture/CPUFrameGraph.md).
class Zenith_SystemRegistry
{
public:
	Zenith_SystemBuilder& AddSystem(const Zenith_SystemDesc& xDesc);
	Zenith_SystemBuilder& AddEpilogueStage(const Zenith_EpilogueStageDesc& xDesc);   // subgraph EPILOGUE, stage 12
	Zenith_Result<void, Zenith_ScheduleError> Freeze();                       // checks every declaration
	Zenith_Span<const Zenith_ScheduleDiagnostic> GetDiagnostics() const;     // of the latest Freeze or EmitInto
	uint32 GetSystemCount() const;
	uint32 GetRegistrationIndex(const char* szSystemName) const;  // the merge-key system order; ~0u if absent
	void SetEnabled(uint32 uRegistrationIndex, bool bEnabled);                  // applied at the next tick boundary
};
```

- [Z2-N-ARCH-722] A system's registration index, returned by `GetRegistrationIndex` and used as the merge key's system order, is the order of its `AddSystem` or `AddEpilogueStage` call in the world's registry, which the canonical module order makes identical on every run.
- [Z2-N-ARCH-726] An epilogue stage registered with `AddEpilogueStage` (the one descriptor type is `Zenith_EpilogueStageDesc`, Architecture/CompositionRoot.md → "Startup sequence") is stored with `m_ePhase = PHASE_NONE` and is exempt from `SCHEDULE_ERROR_PHASE_NONE`, the authority checks and bullets 503–504; `EmitInto` places it in stage 12 of the EPILOGUE subgraph of its world's group, beside the extractors.
- [Z2-N-ARCH-727] An epilogue stage's builder accepts component reads, queries, world-resource reads and `WriteResource` of a world resource that it is the sole consumer of (the stage drains it, as `Audio.Update` drains `Zenith_AudioCommandQueue`); any component write is `SCHEDULE_ERROR_READ_ONLY_PHASE_WRITE`.
- [Z2-N-ARCH-730] `Zenith_SystemRegistry::AddSystem` (and a reload scope's `AddSystem` or `ReplaceSystem`) rejects `PHASE_RENDER_EXTRACTION`, `PHASE_COMMANDS_ONLY` and every value above `PHASE_SAVE_SNAPSHOT` at the call with `ECS_ERROR_PHASE_NOT_SCHEDULABLE`, because the TICK subgraph holds phases 1 to 11 and phase-12 work is EPILOGUE work registered through `AddEpilogueStage` or the extractor path; the rejection fails the registering module's attach with `MODULE_ERROR_REGISTRATION`, which the composition root reads through `GetRejectedRegistrations` (`Test_SystemRegistry_UnschedulablePhase_Rejected` in `Engine/ZenithECS/Tests/`).
- [Z2-N-ARCH-731] A world resource owns one phase from 1 to 11: a resource owning phase 11 (`PHASE_SAVE_SNAPSHOT`) is a snapshot buffer (SAVE's `Zenith.SaveSnapshotBuffer`, NET's `Zenith.Net.Capture`), the one kind of write phase 11 admits, no resource owns phase 12, and the phase passed to `Zenith_WorldAttachContext::AddWorldResource` equals the resource type's `m_eResourceOwningPhase` (otherwise `SCHEDULE_ERROR_RESOURCE_AUTHORITY`); a module whose systems advance world-clock-like state for the tick (the Environment module's clock, weather, wind and activation systems) owns its resources in `PHASE_INPUT`, so phase-2 scripts see the tick's state (Architecture/FramePhases.md → "The twelve phases").

### The system context

```cpp
// Engine/ZenithECS/Zenith_SystemContext.h
class Zenith_EventWriter;          // Emit(Zenith_TypeID xEventType, Zenith_Span<const Zenith_PropertyValue> axValues)
class Zenith_ContributionWriter;   // Contribute(Zenith_TypeID xReductionTarget, Zenith_ByteSpan xContribution)

// Contract: OWNER_THREAD — valid only for the system invocation it is passed to; every accessor
// grants exactly what the system's registration declared.
class Zenith_SystemContext
{
public:
	Zenith_JobContext& Jobs();                            // sub-jobs and continuations
	uint16 GetWorldTag() const;
	uint64 GetTickIndex() const;
	uint64 GetTickDurationUs() const;
	Zenith_InvocationScope GetScope() const;
	uint32 GetChunkCount(const Zenith_QuerySlot& xQuery) const;
	Zenith_ChunkView GetChunk(const Zenith_QuerySlot& xQuery, uint32 uChunkOrdinal);
	void* GetResource(Zenith_TypeID xResourceType);        // null unless declared
	template<typename T> T* Resource() { return static_cast<T*>(GetResource(T::xTYPE_ID)); }
	Zenith_CommandBuffer& Commands(uint64 ulSimulationID);   // the entity transaction's buffer; 0 = non-entity work
	Zenith_EventWriter& Events(uint64 ulSimulationID);      // declared event types only
	Zenith_ContributionWriter& Contributions(uint64 ulSimulationID);
	Zenith_Arena& GetScratch();                            // the node arena
};
```

- [Z2-N-ARCH-718] A `Zenith_SystemContext` exists for one invocation, is created by the frame-graph node that runs the system, and is neither copied nor stored; a sub-job reaches it through the pointer its node passes in its job data, within that node's lifetime.
- [Z2-N-ARCH-719] `GetResource` returns a world resource only when the system declared a read or write of that resource type and returns null otherwise; resources are injected by declaration, never looked up as services (Zenith2/Docs/Contracts.md → "No mutable global state").

### The merge context

```cpp
// Engine/ZenithECS/Zenith_MergeContext.h
// Contract: OWNER_THREAD — the system's merge node (or its final continuation when it has none), after every sub-job
// of the system node has joined.
class Zenith_MergeContext
{
public:
	Zenith_JobContext& Jobs();
	Zenith_InvocationScope GetSystemScope() const;           // the system node's invocation scope
	uint32 GetMergeVersion() const;                         // V(t, m) of this merge point
	uint32 GetReadVersion() const;                          // V(t, R(S)): the preceding merge point's
	Zenith_MergeCapacity& GetCapacity();
	Zenith_Arena& GetScratch();
	void* GetUserData() const;                                // the system's m_pUserData
	Zenith_Span<Zenith_CommandBuffer* const> GetCommandBuffers() const;   // ascending base key
	Zenith_Span<Zenith_ChunkView> GetWrittenViews();
	Zenith_ECSCapacityDemand ComputeDemand(const Zenith_CommandBuffer& xBuffer) const;
	void AcceptBuffer(Zenith_CommandBuffer& xBuffer);   // appended to the phase's command and event queues in merge order
	void RejectBuffer(Zenith_CommandBuffer& xBuffer);   // discarded; counted in ecs.capacity_rejections_delta
	Zenith_Result<void, Zenith_ECSError> CommitRowChanges(Zenith_ChunkView& xView, Zenith_TypeID xType,
		Zenith_PropertyID xProperty, const uint64* pulAcceptedRowBits);
	void CommitAllWrittenRows();                        // the default merge: every pending bit of every written view
	void FoldReductions();                              // accepted contributions, merge order, fixed-order tree
};
```

- [Z2-N-ARCH-732] The ECS public types this file declares (`Zenith_WorldPlaybackAccess` with its playback-write members and `SetPlaybackVersion`, `Zenith_MergeContext`, `Zenith_World` with `SweepChangeVersions`, `Zenith_SystemContext` with `GetPreviousReadVersion`, `Zenith_SystemRegistry` with `GetRejectedRegistrations`, `Zenith_CommandBuffer`, `Zenith_WorldSimulation`) also carry the members `Zenith2/Docs/Design/ECS/WorldAPI.md` and `Zenith2/Docs/Design/ECS/SystemAPI.md` mark elaboration, and those designs' headers are the complete declarations; a member this file declares keeps its name and meaning there.

The job graph schedules ECS systems from their declared read and write access (Architecture/CPUFrameGraph.md → "Frame graph builder"). It supports cancellation, thread affinity, a fixed step with interpolation, pause and time scale, bounded catch-up and orderly shutdown (Architecture/CPUFrameGraph.md → "Execution").

- [Z2-N-ARCH-707] A system's callback runs once per tick per world as one frame-graph node; it iterates its query's chunks inline or fans out with `ParallelFor` over the chunk count, and every chunk it touches in a phase belongs to that phase's stable chunk list.
- [Z2-N-ARCH-708] Within one invocation, a system writes only the columns it declared, and two invocations that the graph runs concurrently never write the same column of the same chunk, because the schedule gives each (property, phase) a unique writer or an arbitration node.

## Script lifetimes on the ECS side

The Luau transaction model (Z2-D-016, Z2-D-051) relies on these ECS-side guarantees; its own rules are in Zenith2/Docs/Scripting.md → "Transaction scope and the entity boundary".

- [Z2-N-ARCH-709] The transaction boundary is the entity, never the chunk: the ECS supports per-row restore of a chunk's declared-writable columns (snapshot per chunk on first write, restore per row) and per-entity discard of side-effect sub-buffers, so a failure rolls back only the failing entity's writes.
- [Z2-N-ARCH-710] A chunk-column snapshot covers only the columns the invocation declared writable (their pending change bits are private to the invocation and need no copy, bullet 720), and lives until the system's merge point, because merge-time capacity rejection can still roll back an entity's rows after the call returns.
- [Z2-N-ARCH-711] A chunk system reads a column it writes in the same phase only for its own row, and other entities' rows only in columns not written in that phase; the schedule compiler marks each (system, column) pair own-row-only or any-row from per-field authority, and the view enforces it (Zenith2/Docs/Scripting.md → "Transaction scope and the entity boundary").
- [Z2-N-ARCH-712] Spawn IDs handed to a script during an invocation are provisional and are released on rollback; the entity IDs and stable simulation IDs of accepted spawns are assigned at playback in merge order.

## Diagnostics

- [Z2-N-ARCH-713] Every world publishes, once per tick after the end-of-tick playback, each suffixed `.<worldTag>` and with its kind in the counter hook's `eKind` (Zenith2/Docs/Contracts.md → "Threading and concurrency contract"), the gauges `ecs.entities`, `ecs.chunks`, `ecs.archetypes` and `ecs.snapshot_bytes` (the tick's peak) and the deltas `ecs.commands_delta`, `ecs.command_overwrites_delta`, `ecs.commands_stale_delta`, `ecs.commands_on_destroyed_delta`, `ecs.playback_us_delta` (the tick's sum of the sync-point and end-of-tick zones), `ecs.rollbacks_delta` and `ecs.capacity_rejections_delta` to the on-screen profiler; the full counter table, with its further rows, is `Zenith2/Docs/Design/ECS.md` → "Counters".

## Required tests

The table "ECS conformance tests" lists the tests of this section; they are conformance scenarios in `Engine/ZenithECS/Tests/Conformance/`, run once per backend by `ZenithECS_Conformance`, in the `Test` step and in the `Zenith2ECS` category's gate lines.

- [Z2-N-ARCH-714] Every test in the table "ECS conformance tests" exists under the name given, runs on all three backends, and asserts the case given.
- [Z2-N-ARCH-715] The determinism tests of the table run the same replay with 1, 2, 8 and all workers, with forced compaction and with two chunk capacities, on each backend, and assert bit-identical per-tick world-state hashes per backend (the hash of Zenith2/Docs/Scripting.md → "Determinism rules and randomness").

ECS conformance tests:

| Test | Asserts |
|---|---|
| `Test_ECS_SpawnVisibleNextPhase` | a spawn recorded in phase p is absent from phase p's queries and present in phase p+1's |
| `Test_ECS_DestroyAtTickEnd` | a destroyed entity resolves until the end of its tick and is stale in the next |
| `Test_ECS_HandleGenerationRetire` | a slot at the maximum generation is retired and never reissued |
| `Test_ECS_ForeignWorldHandle_Fails` | a handle resolved in another world fails with the wrong-world error |
| `Test_ECS_MergeOrderIndependentOfCompletion` | shuffled completion order of 64 invocations yields identical playback |
| `Test_ECS_SpawnIDsInMergeOrder` | spawn handles and simulation IDs are identical with 1, 2, 8 and all workers |
| `Test_ECS_EpochBumpedOnlyByStructure` | property commands and commits leave epochs unchanged; a spawn into a chunk bumps it |
| `Test_ECS_ChangeVersionAdvancesAtMerge` | a written column's version equals its merge point's precomputed merge version; a failed write advances nothing |
| `Test_ECS_CommitRowChangesAdvancesVersionOnce` | per backend, a merge committing two entities' bits sets the column version to the merge's precomputed merge version once; a rejected entity's bits leave no trace |
| `Test_ECS_MergeVersionsIndependentOfWorkers` | every column's version after 60 ticks is identical with 1, 2, 8 and all workers |
| `Test_ECS_ChangedQueryReaderRegisteredBeforeWriter` | a merge-node reader registered before the merge-node writer of a property it reads schedules, merges after the writer and sees the write in the same tick |
| `Test_ECS_TickBoundaryPropertyWriteCommitted` | a property written through `Zenith_TickBoundaryAccess` at tick-boundary step 2 is matched by a phase-1 `QUERY_TERM_CHANGED` query, with row version V(t, 0) || `Test_ECS_ChangedQueryFilter` | a `QUERY_TERM_CHANGED` query returns exactly the chunks whose declared properties changed |
| `Test_ECS_QueryStableWithinPhase` | a query's chunk list does not change during a phase even when commands are recorded |
| `Test_ECS_ViewInvalidAfterInvocation` | a view kept past its invocation reports invalid |
| `Test_ECS_ViewInvalidAfterStructuralChange` | a view of a chunk whose epoch changed reports invalid |
| `Test_ECS_UndeclaredColumnIsNull` | an undeclared property's column is null (assert disabled) |
| `Test_ECS_DisjointColumnWritesConcurrent` | two systems writing disjoint properties of one component run concurrently and both commit |
| `Test_ECS_PerRowRestore` | restoring one row of a snapshotted chunk leaves the other rows' writes intact |
| `Test_ECS_FixedOrderReduction` | a float sum over 100,000 contributions is bit-identical with 1, 2, 8 and all workers |
| `Test_ECS_DeterministicReplay` | a 600-tick replay of the determinism fixture gives identical tick hashes across workers, compaction and capacities |
| `Test_ECS_DeterministicReplayWithFailures` | the same replay with injected script errors, budget exhaustion, snapshot-cap overflow and capacity rejection gives identical tick hashes (the fault matrix of Zenith2/Docs/Scripting.md → "Fault-matrix tests") |
| `Test_ECS_ContainerHeapExhaustion` | a spawn needing container storage beyond the heap is rejected at merge with a diagnostic, never an assert |
| `Test_ECS_FactoryFailureUnwinds` | each injected construction failure of each backend's factory returns an error and leaks nothing |
