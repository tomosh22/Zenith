# ECS design — the three backends

This file is part of the ECS design family; the entry document is `Zenith2/Docs/Design/ECS.md`. It specifies what every backend implements and how each stores data: the common backend contract, the shared entity table, change tables, the container heap and arbitration staging, then the custom backend's archetype and chunk layout with byte sizes, the flecs mapping and the EnTT mapping with its dynamic-storage adapter, and each backend's construction steps. The specification is `Zenith2/Docs/Architecture/ECSModel.md` → "Backend storage models" and `Zenith2/Docs/Architecture/DataDefinedComponents.md` → "Storage in every backend" (Z2-D-039, Z2-I-ECS-10, Z2-I-ECS-15).

## Common backend contract

Every backend implements `Zenith_World` and `Zenith_WorldPlaybackAccess` (`Zenith2/Docs/Design/ECS/WorldAPI.md` → "Zenith_World header") with these shared obligations; `ZenithECS_Conformance` proves each on every backend:

| Obligation | Rule |
|---|---|
| type erasure | storage is created per `Zenith_TypeID` on first use from the frozen registry's `Zenith_TypeInfo`; no backend includes a component header or names a C++ component type ([Z2-N-ARCH-681], [Z2-N-ARCH-705]) |
| identity | the backend owns one `Zenith_EntityTable` and stores the entity's handle and simulation ID alongside its row so chunk views expose them as arrays ([Z2-N-ARCH-685]) |
| columns | a chunk view exposes one `Zenith_RawColumn` per declared property with pointer, stride and optional row-index table ([Z2-N-ARCH-685]) |
| construction | a new component of a type whose `m_pfnConstruct` is null is the byte copy of `Zenith_TypeInfo::GetDefaultObject()` (every declared default, nested struct and fixed-array defaults included); a non-null `m_pfnConstruct` is called instead, with the hooks' `m_pUserData` last; a backend never leaves a constructed component zero-filled (`Zenith2/Docs/Design/ECS/Reflect.md` → "C++ hooks") |
| relocation | rows are relocated bytewise; bytes of a row are never duplicated without the copy hook, and every live component is destroyed exactly once through its destroy hook ([Z2-N-ARCH-602]) |
| containers | a row's `container<T>` blocks are freed from the world's container heap when the row is destroyed or the field replaced; a copied row gets new blocks; `SetContainer` always takes a new block within a reservation its caller holds, and `RewriteContainer` rewrites a lifetime-free container's elements in its existing block, allocating nothing (`Zenith2/Docs/Design/ECS/WorldAPI.md` → "Container writes without a merge reservation", "In-place container rewrite") |
| structural epoch | a chunk's epoch changes exactly when playback adds, removes or moves its rows or changes its layout ([Z2-N-ARCH-693]) |
| change state | a column version and per-row versions per (chunk, column), carried with rows on every move, and every relocation leaves the destination column version at least as new as the row it placed (§ "Change tables", the relocation rule) |
| query cache | a compiled query caches its matched chunks as `Zenith_ChunkBinding` records, refreshed only when stale, only by the sync-point node or out of tick ([Z2-N-ARCH-700]) |
| address stability | between a sync point (or the tick boundary) and the next sync point of the same world, no backend moves, reorders or frees a row, a column array, a change block, a row-index table or a `Zenith_ChunkBinding` that a binding of the phase references; row moves, swap-removes, compaction, flecs table moves and EnTT group re-sorts happen only inside playback, and regathering (EnTT view chunks, flecs slices) happens only in a refresh, which `Zenith2/Docs/Design/ECS/Queries.md` → "Matched-chunk cache and invalidation" confines to sync points and out-of-tick playback ([Z2-N-ARCH-480]); so a column stays at one address from an invocation's first write to its merge, which SCRIPT's per-row restore requires ([Z2-N-ARCH-709]) |
| world-level views | `GetChunk` and `GetChunkOfEntity` construct views with merge version 0, which are commit-disabled and read-only in every column; only the stamping paths of `Zenith2/Docs/Design/ECS/WorldAPI.md` → "Zenith_ChunkView header" make a view writable |
| playback writes | `SetProperty`, `SetContainer` and `RewriteContainer` record the playback's own pending bits per written (chunk, column) between `BeginPlaybackWrites` and `EndPlaybackWrites`, and outside that pair stamp the written row and column at once with the playback version `SetPlaybackVersion` set, which also stamps every row a playback creates (§ "Playback writes") |
| version sweep | `SweepChangeVersions` re-stamps every change block's aged versions (§ "Change tables"; `Zenith2/Docs/Design/ECS/Queries.md` → "Version wrap sweep"), and with threshold 0 every version other than the one passed (`RestoreSimulationState`) |
| capacity | chunk capacity `m_uChunkCapacityRows` (0 = default) and forced compaction on request ([Z2-N-ARCH-686]) |
| failure | every playback failure is a `Zenith_ECSError` logged at the point of failure; a factory unwinds on every failed step ([Z2-N-ARCH-684]) |
| no third party outside Internal | flecs and EnTT headers are included only under the backend's `Internal/` ([Z2-N-ARCH-681], [Z2-N-CONTRACT-043]) |

The backend-support types every backend shares (the entity table, the container heap, the chunk binding records) are public ZenithECS types, because backends cannot include `ZenithECS/Internal/`.

## Entity table

`Zenith_EntityTable` (`Zenith2/Docs/Design/ECS/WorldAPI.md` → "Zenith_EntityTable header"). Storage, reserved at `Create` for `m_uMaxEntities` slots:

| Field of a slot record (56 bytes) | Bytes | Meaning |
|---|---|---|
| simulation ID | 8 | 0 when free |
| persistent ID | 16 | null for runtime-only entities |
| backend location | 8 | custom: (chunk index << 32) \| row; flecs: the `ecs_entity_t`; EnTT: the `entt::entity` value |
| generation | 4 | 28 bits used |
| parent slot | 4 | 0xFFFFFFFF for a root |
| first child slot | 4 | 0xFFFFFFFF when none |
| next sibling slot | 4 | sibling list in ascending simulation ID |
| flags | 4 | `Zenith_EntitySlotFlags` |
| reserved | 4 | 0 |

Plus a free-slot bit set (1 bit per slot) with a lowest-free hint word, and a `Zenith_HashMap<Zenith_PersistentID, uint32>` for non-null persistent IDs (keyed by value, iterated never).

- **Allocate**: find the lowest set bit at or after the hint (O(slots / 64) worst case, O(1) amortised), clear it, set generation to the slot's next generation (the table's initial generation on a slot's first issue, [Z2-N-CONTRACT-100]), write the record and the persistent-ID map entry; no free slot is `ECS_ERROR_CAPACITY`. Slot choice is lowest-free-first in merge order, so it is deterministic ([Z2-N-ARCH-688]), and nothing depends on it.
- **Resolve**: null → `ECS_ERROR_NULL_HANDLE`; tag ≠ the world's → `ECS_ERROR_WRONG_WORLD`; slot ≥ capacity, slot not live or generation mismatch → `ECS_ERROR_STALE_HANDLE`. Every configuration ([Z2-N-CONTRACT-062]).
- **Release** (end-of-tick removal): unlink from the parent's child list, remove the persistent-ID entry; generation + 1; a generation that reaches `uECS_MAX_GENERATION` marks the slot retired and never frees it ([Z2-N-CONTRACT-070]); otherwise set the free bit and lower the hint.
- **Children**: the sibling list is kept sorted by simulation ID on insert (O(children)); `GetChildren` fills the caller's span in that order, which is the destroy cascade's depth-first order ([Z2-N-ARCH-549]).
- **Highest generation**: tracked as the maximum issued, reported at world destruction to `ReleaseTag`.

## Change tables

Per (chunk, column) the backend keeps a `uint32` column version and a `uint32` row version per row, in a change block allocated from the world heap beside, never inside, the chunk's storage: `4 × columns × (capacity + 1)` bytes per chunk. Row versions move with rows on every relocation (a swap-remove moves the last row's versions into the hole; a structural move copies them to the destination row and stamps the columns that are new to the row with the playback version `SetPlaybackVersion` set). The bytes are reported as `m_ulChangeTrackingBytes` and `ecs.change_bytes`, and counted in Z2-B-ECS-001. `SweepChangeVersions(xToken, c, uThreshold)` walks every change block of the world in any order (the result is order-independent: each version is rewritten from its own value and c alone) and replaces each column and row version s whose ring distance d(c, s) exceeds `uThreshold` with the version `uThreshold` merge points before c (so a threshold of 0 sets every version to c: a version ahead of c on the ring has distance above 0); the custom backend walks its change-block pool, flecs its per-(table, slice) blocks, EnTT its per-pool row-version arrays, whose view-chunk column versions are recomputed from the rows when the next chunk list is built.

<!-- elaboration -->
**Relocation rule.** A `QUERY_TERM_CHANGED` term compares a chunk's **column** version (`Zenith2/Docs/Design/ECS/Queries.md` → "Change-filtered queries"), so the column version must follow the rows a relocation moves in. Whenever a relocation places a row in a (chunk, column) — the destination of an add or remove move (§ "Custom structural operations"), a row compaction moves into a lower chunk (§ "Compaction"), a flecs table move, a flecs swap-remove whose moved row lands in another slice, or a slice renumbering (§ "Synthetic chunks") — that column version becomes the newer, under `Zenith_IsVersionNewer`, of its current value and the moved row's version for that column. A chunk (or slice) that an insert or a relocation creates starts each column version at the version of the first row placed in it, never at 0 (0 would compare as newer than every last-seen version above 2^31 and as older than the rest). A swap-remove within one chunk changes no column version, because the moved row's version is already at or below its own chunk's. Together with the stamping rules (commits and playback stamp a row and its column with one version; the sweep and `RestoreSimulationState` rewrite both by one monotone rule) this keeps the invariant every comparison rests on: a chunk's column version is at least as new as each of its rows' versions for that column, so a changed row is matched in whichever chunk it lands, independent of chunk capacity and compaction, the determinism axes of `Zenith2/Docs/Design/ECS/Tests.md` → "Determinism suite". The stores are part of the move, inside playback, and change no epoch beyond the move's own. EnTT needs no rule: a synthetic chunk's column version is recomputed from its rows (§ "Owning groups and views"). `Test_ECS_ChangedQuerySeesMovedRow` (conformance, both capacities) asserts it: in phase p a `SetProperty` of P on entity e and an `AddComponent` on e, then a phase p + 1 `QUERY_TERM_CHANGED(P)` system whose last-seen version is older than sync point p's matches e's destination chunk; a phase-6 in-place write of P on a row that forced end-of-tick compaction moves into a lower chunk is matched by a phase-2 `CHANGED(P)` system in the next tick; a chunk created by a relocation holds its first row's versions, never 0; and a moved row whose version is not newer than the destination column's adds no match.

## Playback writes

<!-- elaboration -->
The playback's own pending bits of [Z2-N-ARCH-728] ("Writes the value and marks the row changed … in the playback's own pending bits"), kept by every backend in one shape so the world simulation commits them without knowing the storage (`Zenith2/Docs/Design/ECS/Scheduling.md` → "Playback write commit"):

- **Set.** `BeginPlaybackWrites(xArena)` clears the backend's playback write set and remembers the arena. A successful `SetProperty`, `SetContainer` or `RewriteContainer` (of a non-empty container) of (entity, type, property) finds the entity's backend chunk and row, looks up the pair (property, backend chunk key) in the set (a `Zenith_HashMap<uint64, uint32>` keyed by `Zenith_Hash64` over type ID ‖ property ID ‖ chunk key, whose values index a `Zenith_Array` of records; the map is never iterated), on first touch appends a record holding a zeroed bit block of ⌈chunk rows / 64⌉ words from `xArena`, and sets the row's bit. A failed call sets nothing.
- **Chunk key per backend.** Custom: the global chunk index of the chunk header. flecs: `Zenith_Hash64` of the table's sorted Zenith type IDs ‖ the slice index. EnTT: the property's pool index ‖ the block index ⌊pool index / 256⌋, a block of 256 consecutive pool indices; the record's rows are pool indices within that block.
- **Order.** `GetPlaybackWrite(i, …)` returns the i-th record after sorting the records once by (type ID, property ID, chunk key) at the first `GetPlaybackWrite` call after the last property call.
- **View.** Each record's view is built on a one-column `Zenith_ChunkBinding` in `xArena`: row count = the chunk's (or block's) rows, the column's data, stride, row versions and column version of that chunk (for EnTT the pool's rows at the block's indices, with no row-index table), the structural epoch pointer of the chunk (custom header epoch, flecs slice epoch, EnTT pool epoch), and null entity and simulation-ID arrays, because a playback write view is used only to commit; the view carries the playback scope and version passed to `GetPlaybackWrite` and is commit-enabled.
- **End.** `EndPlaybackWrites()` forgets the set; the blocks die with the arena. The world simulation commits a playback's writes before that playback's first structural call (directly after step 5 at a sync point and out of tick, after step 3 at the tick boundary), so no row with a recorded bit moves before it is committed; a structural call between `BeginPlaybackWrites` and `EndPlaybackWrites` is `ZENITH_ASSERT`. A spawn's and an add's init values are applied inside `CreateEntity` and `AddComponent` and never enter the set: the new rows take the playback version as their row and column versions directly (`Zenith2/Docs/Design/ECS/Queries.md` → "Change versions and merge versions" item 4).
- **Playback version.** `SetPlaybackVersion(uVersion)` records the version of the playback in progress (the world simulation sets it at the start of every playback and resets it to 0 at the end, `Zenith2/Docs/Design/ECS/Scheduling.md` → "Playback write commit" item 0). It stamps every row `CreateEntity`, `CreateEntityWithIdentity` and `AddComponent` create, the columns a structural move adds to a row, and every property write outside the set. A call that would stamp with it — `CreateEntity`, `CreateEntityWithIdentity`, `AddComponent`, or a `SetProperty` or `SetContainer` outside the set — while it is 0 is `ZENITH_ASSERT` and stamps nothing; removal (`RemoveComponent`, `DestroyNow`, including world teardown) stamps nothing and needs no version.
<!-- assumption: pending core-doc batch (X147) -->
- **Core wording.** [Z2-N-ARCH-728]'s "no call other than `CommitRowChanges` advances a column's change version" and `Zenith2/Docs/Architecture/FramePhases.md` → "Sync points and the tick boundary" step 9's "through `Zenith_ChunkView::CommitRowChanges` alone" admit, in the core-doc batch that applies X147, the playback-version stamps of this section (rows a playback creates, columns a move adds, writes outside the set) and the relocation rule of § "Change tables"; until then a worker follows this section.
<!-- /assumption -->
- **Outside the set.** A successful `SetProperty`, `SetContainer` or `RewriteContainer` (of a non-empty container) called while no write set is open (step 6 of a sync point or of out-of-tick playback: a redundant `ADD_COMPONENT`'s values, a ticket-addressed record) writes the value, then stores the playback version into the row's version and into its (chunk, column)'s column version — on EnTT the pool's row version alone, from which every synthetic chunk's column version is recomputed when the next chunk list is built (§ "Change tables") — and records nothing; the chunk's epoch is unchanged (`Zenith2/Docs/Design/ECS/Scheduling.md` → "Property writes during structural playback"). The two stores give the row and the column exactly the versions a write-set commit would, so no reader can tell the paths apart.

`Test_ECS_PlaybackWritesCommittedOncePerChunkColumn` (`Zenith2/Docs/Design/ECS/Scheduling.md` → "Playback write commit") proves the shape on every backend.

## Container heap

`Zenith_ContainerHeap` (`Zenith2/Docs/Design/ECS/WorldAPI.md` → "Zenith_ContainerHeap header"): one reservation of `m_uContainerHeapBytes` carved by a buddy allocator into power-of-two blocks from 16 bytes; a block index is (order, offset / 16) packed in 32 bits; free lists per order are LIFO arrays with a deterministic split and merge rule (lowest address first). Reservations at merge (`Reserve`) only adjust a counter, so acceptance never allocates; `AllocateReserved` at playback takes a block within the reserved bytes and cannot fail, because a buddy allocator whose free bytes cover a request rounded to its block size always has a block when the reservation used the rounded size ([Z2-N-ARCH-691]). A container field's elements are stored in the element's value encoding with lifetime-carrying elements (asset handles) managed through the element type's hooks. A spawn needing container storage beyond the heap is rejected at merge with a diagnostic, never an assert (`Test_ECS_ContainerHeapExhaustion`); destroying rows returns every block (`Test_DataComponent_ContainerFieldReleased`). Every caller of `SetContainer` with non-zero block bytes holds a reservation of its size class (the table of `Zenith2/Docs/Design/ECS/WorldAPI.md` → "Container writes without a merge reservation"); `RewriteContainer` changes the values of a container whose element type carries no lifetime inside its existing block and touches the heap's counters not at all (`Zenith2/Docs/Design/ECS/WorldAPI.md` → "In-place container rewrite"), the same steps on all three backends, since the cell `{ m_uBlock, m_uCount }` and the heap are shared types.

## Arbitration staging

For each arbitrated property and each declared writer source, the backend keeps per chunk a staging column (field bytes × capacity), a weight column (`float` × capacity, `WeightedBlend` only) and a written-bit set, allocated in a per-world staging arena at `SetArbitrationStaging` for existing chunks and at chunk creation for new ones, cleared by `ResetArbitrationStaging` at every sync point. The staging binding of a writer's view points at its source's staging column (`Zenith2/Docs/Design/ECS/Queries.md` → "Views and validation" step 4).

## Custom backend

`ZenithECS_Custom` — archetype chunk storage designed for runtime layouts from the start ([Z2-N-ARCH-601], table "Backend storage models").

### Archetypes

An archetype is the sorted set of a row's component type IDs (at most `uECS_MAX_COMPONENTS_PER_ENTITY` = 64). Records:

| Archetype record | Contents |
|---|---|
| type set | sorted `Zenith_TypeID` array |
| column table | one column per (component, property) in (alignment descending, type ID, property ID) order, each with value type, field bytes, offset of its array in the chunk, the property's `Zenith_PropertyInfo*`; two leading identity columns: entity handles (8 B) and simulation IDs (8 B) |
| row bytes R | 16 + Σ field bytes |
| capacity C | the world's `m_uChunkCapacityRows` when non-zero, else min(1,024, ⌊(16,384 − 64 − 16 × (columns + 2)) / R⌋), at least 1 |
| chunk bytes | 16 KiB when the default capacity fits, else 64 + Σ (16-byte-aligned column arrays), rounded to a multiple of 16 KiB |
| chunks | `Zenith_Array<uint32>` of chunk indices in creation order |
| add and remove edges | `Zenith_HashMap<Zenith_TypeID, uint32>` transition caches (type → destination archetype) |

Archetypes are found through a `Zenith_HashMap<uint64, uint32>` keyed by `Zenith_Hash64` over the sorted type-ID bytes (a key of data, never of an address), with the full set compared on a hash match. A component with no properties (a tag) adds no column.

### Chunk layout

One chunk is one block from the world heap, 16-byte aligned:

| Offset | Bytes | Content |
|---|---|---|
| 0 | 64 | header: archetype index `uint32`; index in archetype `uint32`; row count `uint32`; capacity `uint32`; structural epoch `uint32`; flags `uint32`; global chunk index `uint32`; change-block index `uint32`; free-list link `uint64`; 24 bytes reserved |
| 64 | 8 × C, aligned 16 | entity handles |
| next | 8 × C, aligned 16 | stable simulation IDs |
| next | field bytes × C, aligned 16, per column | one array per property (structure of arrays) |

Worked sizes: `Zenith.Transform` with three properties (`vec3` 12 B, `quat` 16 B, `vec3` 12 B) alone: R = 16 + 40 = 56, C = ⌊(16,384 − 64 − 80) / 56⌋ = 290 rows; the example `OpenWorldSlice.Stats` of `Zenith2/Docs/Architecture/DataDefinedComponents.md` (f32 × 3, entity 8, asset 16, container 8, string 4): R = 16 + 48 = 64, C = ⌊(16,384 − 64 − 144) / 64⌋ = 252. A column's stride is its field size; a property's column pointer plus `row × stride` is the row's value, with no row-index table ([Z2-N-ARCH-685]).

### Custom structural operations

- **Insert** (spawn, destination of a move): the archetype's lowest-index chunk with a free row, else a new chunk (appended to the archetype, query structure version + 1); the row is appended at the end of the chunk; components constructed from defaults (hooks) then the init values applied by property ID; the destination's column versions follow the relocation rule of § "Change tables" (a new chunk starts at the first row's versions).
- **Remove** (end-of-tick destroy, source of a move): destroy the removed row's components through hooks (a move destroys only the components not carried), free its container blocks (destroy), then move the chunk's last row into the hole bytewise with its row versions, update the moved entity's backend location; a chunk left empty is freed (query structure version + 1).
- **Add or remove a component**: find or create the destination archetype through the edge cache, insert into it, relocate every carried column's bytes and row versions (each carried destination column version becoming the newer of its value and the row's, § "Change tables"), construct the new component (add) or destroy the removed one (remove), remove from the source; both chunks' epochs + 1.
- Complexity O(row bytes) per operation, O(1) amortised chunk search with a per-archetype "first chunk with space" index.

### Compaction

At the end-of-tick playback, an archetype whose rows would fit in at least one fewer chunk (Σ rows ≤ (chunks − 1) × C) is compacted: rows are moved from the highest-index chunk into free rows of the lowest-index chunks, in row order, until the last chunk empties and is freed, each moved row's versions with it and each destination column version by the relocation rule of § "Change tables"; every touched chunk's epoch + 1 ([Z2-N-ARCH-480]). `ForceCompaction` compacts every archetype fully at the next sync point or end of tick of the test driver. Storage order changes, results do not ([Z2-N-ARCH-702]).

### Queries

A compiled query keeps the matched archetype list (every archetype containing R and none of X), examines only archetypes created since its last refresh, and rebuilds its chunk bindings from the matched archetypes' chunks on refresh; a binding's column pointers are the chunk's column arrays (null for an absent optional component).

## flecs backend

`ZenithECS_Flecs` integrates the manifest's pinned flecs (v4 API) under `Internal/` alone. The backend uses flecs as storage and query engine only: no flecs systems, pipelines, observers, deferred mode, threads or modules are used, and every structural change happens in ECS playback.

### flecs mapping

| Zenith concept | flecs concept |
|---|---|
| world | one `ecs_world_t` created with `ecs_mini()` (no addons) |
| component type | a runtime component `ecs_component_init` with the layout's size and alignment, named by the stable name, hooks set with `ecs_set_hooks_id` ([Z2-N-ARCH-601] table "Storage in every backend") |
| component lifecycle | `ecs_type_hooks_t` ctor, dtor, copy and move forwarding to the Zenith hooks, with the `Zenith_TypeInfo*` in the hooks' context pointer; asset fields thereby ref-count through the value-kind hooks |
| identity | every entity carries the component `Zenith.Identity` (16 bytes: handle, simulation ID); the entity table's backend location is the `ecs_entity_t` |
| archetype | a flecs table |
| chunk | a slice of up to capacity rows (default 256) of a flecs table: slice k covers rows [k × C, min((k + 1) × C, count)) |
| property column | the component column's base + the property's offset; stride = the component's size (array of structures) |
| entity handles and simulation IDs | the `Zenith.Identity` column's two fields, stride 16 |
| structural epoch | per (table, slice) in a side record keyed by `Zenith_Hash64` over the table's sorted Zenith type IDs; + 1 when playback inserts into, removes from or reorders the slice |
| query | an `ecs_query_t` with terms `WITH` → `EcsIn`/`EcsInOut` term, `WITHOUT` → `EcsNot`, `OPTIONAL` → `EcsOptional`, `CHANGED` → an `EcsIn` term (change filtering is Zenith's own); cached (`EcsQueryCacheAuto`) |

### Synthetic chunks

At refresh the backend iterates the flecs query once (`ecs_query_iter`, `ecs_query_next`), and for each result table appends one `Zenith_ChunkBinding` per slice, with column pointers from `ecs_field_w_size` offset by the slice's first row. Table row order changes whenever flecs removes a row (it moves the last row into the hole); the backend mirrors every such move into its change tables and slice epochs by recomputing, after each structural operation on table T, the rows affected (the removed row and the table's former last row), and applies the relocation rule of § "Change tables" to the slice a moved row lands in when it is not the row's former slice; a slice renumbering (compaction) applies it to every row that changes slice.

### flecs structural operations

`ecs_new`, `ecs_add_id`, `ecs_remove_id`, `ecs_delete` and `ecs_set_id` inside playback only; initial values are written through `ecs_ensure_id` into the new component after flecs constructed it with the ctor hook; the change-table rows are moved as the flecs table moves are mirrored, and the destination table's slice column versions follow the relocation rule of § "Change tables". flecs table storage is flecs's (allocations through flecs's OS API, which the backend sets to route to the world heap before creating the world); compaction is flecs's own table management plus the backend's slice renumbering, so `ForceCompaction` bumps every slice epoch and refreshes every query.

## EnTT backend and the dynamic-storage adapter

`ZenithECS_EnTT` integrates the manifest's pinned EnTT (header-only, v3 API) under `Internal/` alone.

### EnTT mapping

| Zenith concept | EnTT concept |
|---|---|
| world | one `entt::basic_registry<entt::entity, Zenith_EnTTAllocator<entt::entity>>`; the allocator routes to the world heap |
| component type | a named storage `registry.storage<Zenith_EnTTBlock<N>>(uStorageID)` per type, N the smallest size class ≥ the layout size (8, 16, 32, 64, 128, 256, 512, 1,024 bytes), uStorageID the type's dense per-world index (never a hash, so two types never share a pool) |
| element | `struct Zenith_EnTTBlock<N> { alignas(16) uint8 m_auBytes[N]; }`, trivially copyable, so EnTT's own relocations (swap-and-pop, sort) are bytewise relocations that the relocation rule allows |
| component lifecycle | the adapter runs the Zenith construct hook after `emplace`, the destroy hook before `erase`, and the copy hook when copying a row; EnTT never runs a meaningful constructor or destructor on a block |
| identity | a storage `Zenith.Identity` of 16-byte blocks (handle, simulation ID) on every entity; the entity table's backend location is the `entt::entity` |
| chunk | a synthetic chunk: a run of up to capacity entities (default 256) of an adapter owning group, or a gathered run of a view with row-index tables |
| property column | the pool's block array + the property's offset; stride = N |
| structural epoch | per pool a `uint32` epoch, + 1 at every playback that inserts, erases or reorders in the pool; a synthetic chunk's epoch is the wrapping sum of its query's required pools' epochs |

### Size classes

| Layout bytes | Block | Waste per element |
|---|---|---|
| 1–8 | 8 | 0–7 |
| 9–16 | 16 | 0–7 |
| 17–32 | 32 | 0–15 |
| 33–64 | 64 | 0–31 |
| 65–128 | 128 | 0–63 |
| 129–256 | 256 | 0–127 |
| 257–512 | 512 | 0–255 |
| 513–1,024 | 1,024 | 0–511 |

A block of class N is aligned to min(N, 16) (`alignas(N < 16 ? N : 16)`), which covers every component of that class (a component's alignment divides its size and is at most 16) and keeps `sizeof` of the block equal to N, so the 8-byte class really is 8 bytes and the waste column is exact. The waste is part of the adapter's memory overhead the research reports.

### Owning groups and views

EnTT's owning groups are typed at compile time and cannot own runtime-named storages, so the adapter implements its own **adapter owning groups** with EnTT's sparse-set operations: a group owns a set of pools and keeps the entities present in all of them packed at indices [0, K) of each owned pool in the same order, maintained on insert (swap the new entity into position K of each owned pool with the sparse set's element swap, then K + 1) and on erase (swap to K − 1, then K − 1), exactly EnTT's owning-group algorithm. Policy, deterministic in query-compile order (registration order): a compiled query whose required set has two or more pools, none owned by an existing group, creates a group over those pools; every other query uses a view. A pool belongs to at most one group.

- **Group chunks**: chunk c covers pool indices [c × C, min((c + 1) × C, K)); columns point into each owned pool at index c × C with stride N and no row-index table; handles and simulation IDs are gathered at refresh from the identity pool into per-chunk arrays (8 + 8 bytes per row).
- **View chunks**: the leading pool is the smallest required pool; its entities are iterated in pool order, filtered by presence in the other required pools and absence from the excluded ones, and gathered into runs of C; each column gets a row-index table (the entity's index in that pool, 4 bytes per row per required pool), and handles and simulation IDs are gathered.
- A group chunk's row versions are the pool's row versions at the same indices; a view chunk's are reached through its row-index tables. The column version a view reports is the newest of its rows' versions under `Zenith_IsVersionNewer` (never the integer maximum, which picks the wrong version across a wrap of the ring 1 … 2^32 − 1), computed when the invocation's chunk list is built (O(rows)); the newest is well defined because the version wrap sweep keeps every stored version within 2^30 + 2^29 + M < 2^31 merge points of the world's current version, so any two rows' versions compare consistently (`Zenith2/Docs/Design/ECS/Queries.md` → "Version wrap sweep" item 5).

### Adapter cost model

Every component that reaches the EnTT backend through `Zenith_ECS` goes through the adapter: the backend library cannot include a C++ component header (layering), so C++-registered and data-defined components alike are stored as byte blocks. Only the research harness measures typed EnTT, outside `Zenith_ECS` (`Zenith2/Docs/Design/ECS/Tests.md` → "Benchmarks and research scenarios"). The adapter's costs the research reports separately (Z2-D-064):

| Cost | Where it arises | How it is measured |
|---|---|---|
| copying | hook calls on emplace, erase and row copies; gathered identity arrays | `Bench_ECS_Adapter_Churn` against `Bench_ECSNative_EnTT_Churn` |
| extra indirection | row-index tables in view chunks | `Bench_ECS_Iterate2` and `Bench_ECS_Iterate4` (view queries) against the typed EnTT views |
| memory overhead | size-class waste, identity pool, change tables, row-index tables | `ecs.component_bytes`, `ecs.change_bytes`, `ecs.adapter_waste_bytes` sampled at the end of each scenario |
| chunk synthesis | refresh gathering at every sync point that changed structure | the `ECS.QueryRefresh` zone time per sync point in the churn and add-remove scenarios |

The observation that the adapter covers every component, not only data-defined ones, is evidence for flagged proposal P1 and is recorded with it (`Zenith2/Docs/Design/ECS.md` → "Flagged proposal P1").

## Schema revision in the edit world

When the editor publishes a new schema revision (`Zenith2/Docs/Design/ECS/Schemas.md` → "Editor re-registration"), each backend of the edit world (a world whose `Zenith_World::IsEditWorld()` is true, `Zenith2/Docs/Design/ECS/WorldAPI.md` → "The edit-world flag"), out of tick, for each component whose `Zenith_TypeInfo` changed: captures every row's field set (`Zenith_CaptureFieldSet`), migrates it (`Zenith_MigrateFieldSet`), creates storage for the new layout (a new archetype, flecs component or pool), re-applies each row's set, destroys the old storage through the old hooks, and bumps every affected epoch and the query structure version; handles and simulation IDs are unchanged. A play world never observes this ([Z2-N-ARCH-648]).

## Factory construction steps

Each factory performs these steps in order; a failing step returns its error and the partially built world's destructor unwinds exactly the earlier steps ([Z2-N-ARCH-684], [Z2-N-CONTRACT-340]); each step has a fault point and a `Test_ECS_FactoryFailureUnwinds` case per backend ([Z2-N-CONTRACT-341]):

| Step | Custom | flecs | EnTT | Fault point constant |
|---|---|---|---|---|
| 1 | validate the desc (`ECS_ERROR_INVALID_DESC`) and keep `m_bEditWorld` for `IsEditWorld` (`Zenith2/Docs/Design/ECS/WorldAPI.md` → "The edit-world flag") | same | same | — |
| 2 | allocate the world object (`Zenith_TryNew`) | same | same | heap fault countdown |
| 3 | create the entity table | same | same | `uFAULT_POINT_ENTITY_TABLE_*` |
| 4 | create the container heap | same | same | `uFAULT_POINT_CONTAINER_HEAP_RESERVE` |
| 5 | create the archetype index and the empty archetype | set flecs's OS API allocators, create the flecs world | create the registry with the world allocator | `uFAULT_POINT_ECS_<BACKEND>_STORAGE` |
| 6 | create the query table and the staging arena | register `Zenith.Identity` | create the identity storage | `uFAULT_POINT_ECS_<BACKEND>_IDENTITY` |
| 7 | create the change-block pool | same | same | `uFAULT_POINT_ECS_<BACKEND>_CHANGE` |

flecs failures that flecs reports by aborting are prevented, not handled: the backend never passes flecs an invalid argument (every ID it passes is one it created), and flecs's abort hook is set to `ZENITH_FATAL(FATAL_CODE_ASSERTION, "flecs: …")` so an internal flecs assertion terminates with a report rather than undefined behaviour.

## Backend internal files

| Library | Internal files |
|---|---|
| `ZenithECS_Custom` | `Internal/Zenith_CustomWorld.{h,cpp}` (the world and playback access), `Internal/Zenith_CustomArchetypes.{h,cpp}`, `Internal/Zenith_CustomChunks.{h,cpp}`, `Internal/Zenith_CustomQueries.cpp`, `Internal/Zenith_CustomCompaction.cpp`, `Internal/Zenith_CustomStaging.cpp` |
| `ZenithECS_Flecs` | `Internal/Zenith_FlecsWorld.{h,cpp}`, `Internal/Zenith_FlecsTypes.cpp` (component registration and hooks), `Internal/Zenith_FlecsQueries.cpp`, `Internal/Zenith_FlecsSlices.cpp` (synthetic chunks, epochs, change mirroring), `Internal/Zenith_FlecsOsApi.cpp` (allocator and abort routing) — the only files including `flecs.h` |
| `ZenithECS_EnTT` | `Internal/Zenith_EnTTWorld.{h,cpp}`, `Internal/Zenith_EnTTAdapter.{h,cpp}` (blocks, size classes, hooks), `Internal/Zenith_EnTTGroups.cpp` (adapter owning groups), `Internal/Zenith_EnTTViews.cpp` (gathered chunks, row-index tables), `Internal/Zenith_EnTTAllocator.h` — the only files including EnTT headers |

Each backend's `Tests/ConformanceAdapter/Zenith_<Backend>ConformanceAdapter.cpp` registers its factory with the conformance suite (`Zenith2/Docs/Architecture/BuildSystem.md` → "Test, conformance and fuzz targets").
