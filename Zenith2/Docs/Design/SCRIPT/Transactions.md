# SCRIPT design — transactions

Companion to `Zenith2/Docs/Design/SCRIPT.md`: the per-invocation transaction end to end — the arena partition, the invocation layout, the write path, snapshots, copy-on-write shadows, side-effect routing through ECS command buffers and SCRIPT records, per-entity accounting, the merge node, capacity acceptance, commit, failure handling by kind, the quarantine store and the diagnostic store. It elaborates `Zenith2/Docs/Scripting.md` → "Transaction scope and the entity boundary", "Component writes shadows and side-effect buffers", "Commit and failure semantics" and "Merge-time capacity and asynchronous results", over the structures of `Zenith2/Docs/Scripting/TransactionStructures.md` → "Transaction data structures", and restates none of their bullets. Files are under `Zenith2/Engine/ZenithScript/Internal/`.

## Scope of this file

A transaction is the record of one invocation (Scripting.md glossary). Its lifetime spans three nodes: the script node that runs it (a chunk-system sub-job or the behaviour node), the script node's merge node (ECS's `<system>.Merge`, the "merge point" of Scripting.md), and the next tick boundary (disables). The ECS objects it uses are `Zenith_SystemContext`, `Zenith_ChunkView`, `Zenith_RawColumn`, `Zenith_CommandBuffer`, `Zenith_ContributionWriter`, `Zenith_EventWriter`, `Zenith_MergeContext` and `Zenith_MergeCapacity` (`Zenith2/Docs/Design/ECS/WorldAPI.md` → "Zenith_ChunkView header"; `Zenith2/Docs/Design/ECS/SystemAPI.md` → "Zenith_SystemContext header", "Zenith_MergeContext header").

## Transaction arena partition

<!-- assumption: pending core-doc (X116) -->
`Zenith2/Docs/Scripting.md` places every transaction's memory in "a per-worker (or, for behaviours, main-thread) transaction arena, reset to the transaction's mark after commit", pre-reserved at world creation to one invocation's worst case and growing fatally ([Z2-N-SCRIPT-211], [Z2-N-SCRIPT-224], [Z2-N-SCRIPT-225]; "Merge point algorithm" step 6). One worker interleaves sub-jobs of several script nodes of one phase (two chunk systems with disjoint access run concurrently, and a worker steals from both), so a single arena per worker cannot be reset to one node's mark without releasing another node's live transactions. This design keeps every substantive property of those bullets — all transaction memory is `Zenith_Arena` memory (`Zenith2/Docs/Contracts/Memory.md` → "Zenith_Arena") with markers, the reset check of [Z2-N-CONTRACT-230] and poisoning or retagging on reset ([Z2-N-CONTRACT-231], [Z2-N-CONTRACT-247]); one worst-case reservation per slot at world creation; fatal growth — and partitions the arenas per (script system, slot). SCRIPT has no allocator of its own: every byte below comes from `Zenith_Arena::TryAllocate`. The wording of [Z2-N-SCRIPT-211], [Z2-N-SCRIPT-224], [Z2-N-SCRIPT-225] and merge step 6 follows through cross-doc row X116.
<!-- /assumption -->

| Object | Definition |
|---|---|
| worst case W | `Zenith_ScriptTxnWorstCase() = uSCRIPT_MAX_ROWS_PER_INVOCATION × Z2-B-SCRIPT-012 + uSCRIPT_TXN_FIXED_BYTES`; the fixed part is the transaction header (128 B), the entity array (256 × 64 B), the error table (256 × 32 B) and the snapshot headers (64 × 64 B) = 32,896 B; for the main slot, plus the largest declared checkpoint slot (Z2-B-SCRIPT-015) |
| slot | a frame-thread slot `0 … F − 1` (`Zenith_JobContext::GetWorkerIndex()`, cross-area note J7), plus the main slot F, which the behaviour nodes use on the main thread; F is `Zenith_ScriptWorldDesc::m_uFrameThreadCount` |
| scratch arena | one `Zenith_Arena` per slot, `Zenith_ArenaDesc{ "script.txn.scratch", m_ulBlockBytes = S, m_ulMaxBytes = S, m_uRetainedBlocks = 1 }` with S = `round_up(2 × W, 64 KiB) + 64 KiB`; it holds the structures that grow while the invocation running on the slot executes (§ "Invocation layout in the arenas"); one invocation at a time uses it, because a slot's thread runs one invocation at a time (no fibers, and `ParallelFor` is never called inside an invocation) |
| retention arena | `Zenith_ArenaDesc{ "script.txn.retention", m_ulBlockBytes = ulSCRIPT_TXN_RETENTION_BLOCK_BYTES (256 KiB), m_ulMaxBytes = the entry's retention ceiling, m_uRetainedBlocks = uSCRIPT_TXN_RETAINED_BLOCKS (1) }`; it holds the retained transactions of one script system on one slot from their invocations to the system's merge |
| retention entry | `Zenith_ScriptTxnRetentionEntry { Zenith_Arena m_xArena; Zenith_Array<Zenith_ScriptTransaction*> m_axTransactions; std::atomic<bool> m_bInUse; uint32 m_uClaimingSystem; }` (Internal, `Internal/Zenith_ScriptTransaction.h`); `m_axTransactions` is a general-heap `Zenith_Array` whose capacity is kept across ticks |
| retention set | per slot, a `Zenith_Array<Zenith_Owned<Zenith_ScriptTxnRetentionEntry>>` of K_s entries, where K_s is the largest number of script systems of one script phase that can run on that slot: the chunk systems of the phase with the most chunk systems for a frame-thread slot, the behaviour systems of the phase with the most behaviour systems for the main slot |
| claim table | per script system, `Zenith_ScriptSystemRecord::m_auRetentionIndex`, a `Zenith_Array<uint16>` of F + 1 entries (0xFFFF = no entry claimed this tick); entry s is written only by slot s's thread and read and cleared by the system's merge node |
| retention ceiling | the cap of each retention arena, derived from existing rows by § "Retention ceiling derivation" as the most one script system can retain on one slot in a tick of a world within Z2-B-SCRIPT-012 and its capacities; the formula depends only on the node kind and the slot's W, so every entry of a frame-thread slot takes the chunk-system ceiling and every entry of the main slot the behaviour-system ceiling. It is no budget row of its own: it is a function of the rows, so it is the budget-row cap [Z2-N-CONTRACT-227] asks of every arena; it bounds the arena's reserved nominal bytes, which `Zenith_Arena`'s cap checks, not only its used bytes, and it admits the entry-0 pre-reservation, so a script can never trip it ([Z2-N-BUDGET-303]) |

Rules:

1. **Construction.** `Zenith_ScriptWorld::Create` step 4 constructs the F + 1 scratch arenas and pre-reserves each with `TryAllocate(1, 16)` followed by `Reset()`, which allocates and retains its S-byte block. `RegisterScriptNodes` sizes every slot's retention set to its K_s (an arena constructor allocates nothing, [Z2-N-CONTRACT-228]) and pre-reserves entry 0 of every slot with `TryAllocate(W, 64)` followed by `Reset()`, which retains a block of at least W bytes. These two reservations are the pre-reservation of [Z2-N-SCRIPT-224]; a failure of either is `SCRIPT_ERROR_OUT_OF_MEMORY` from its call with nothing retained (fault point `uFAULT_POINT_SCRIPT_WORLD_TXN_ARENAS`). A reload whose staged declarations raise some K_s grows the sets inside `ApplyPendingReloads` (prologue, no tick running, every entry idle); a failure there fails the reload with its rollback (Design/SCRIPT/HotReload.md → "Rollback procedure").
2. **Claim.** A script system's first invocation on slot s in a tick finds claim-table entry s at 0xFFFF, scans slot s's retention set in index order for an entry whose `m_bInUse.load(std::memory_order_acquire)` is false, sets `m_bInUse.store(true, std::memory_order_relaxed)` and `m_uClaimingSystem`, and writes the entry's index into claim-table entry s. Only slot s's thread claims entries of its own set, so the claiming store needs no ordering of its own; the acquire load pairs with the release store of rule 5, so the merge's `Reset()` of that arena happens-before its reuse ([Z2-N-CONTRACT-312]). An exhausted set is unreachable, because at most K_s systems of a phase claim on one slot and every merge node of a phase completes before the phase's sync point; it is `ZENITH_CHECK`ed as a design defect.
3. **Invocation.** The invocation records the scratch arena's marker in `Zenith_ScriptTransaction::m_xArenaMarker`, allocates its retained parts from the claimed entry's arena and its growing parts from the scratch arena (§ "Invocation layout in the arenas"), copies the retained part of the growing structures into the entry's arena at its end, resets the scratch arena to `m_xArenaMarker` (`ResetToMarker`, which poisons the released range), and appends the transaction pointer to the entry's `m_axTransactions`. The append and every arena call run on the slot's thread.
4. **Growth.** A retention allocation that does not fit the current block takes the next retained block, then a new block from the root heap (an allocation above 256 KiB gets a block of its own, [Z2-N-CONTRACT-226]); every such growth is counted in `script.txn.arena_growths`. The root heap's refusal of a block, and a failed growth of `m_axTransactions`, are the fatal `SCRIPT_ERROR_KIND_RESERVATION` halt (ZS1403, Design/SCRIPT/Runtime.md → "World halt procedure"), because general-heap state depends on scheduling ([Z2-N-SCRIPT-225]). `TryAllocate` returning `ALLOC_ERROR_CAPACITY_EXCEEDED` — the retention ceiling reached — is `ZENITH_CHECK` in every configuration: the ceiling bounds what a world within its rows can retain, so reaching it means SCRIPT's own accounting is wrong (an engine defect), never a script error. The scratch arena never grows: S covers every invocation whose entities stay within their caps (§ "Invocation layout in the arenas"), so a scratch `TryAllocate` failure is an internal inconsistency and halts the world with `SCRIPT_ERROR_KIND_INTERNAL`.
5. **Release.** The system's merge node, after commit (§ "Merge node body" step 8), for every slot s whose claim-table entry is set: `Reset()` the entry's arena — which evaluates the live-object check of [Z2-N-CONTRACT-230], poisons or retags every used range ([Z2-N-CONTRACT-231]; rules A6 and H8 of `Zenith2/Docs/Contracts/Memory.md` → "ASan poisoning rules" and "HWASan tagging rules"), keeps its first block and returns the rest to the root heap — `Clear()` its `m_axTransactions` (capacity kept), `m_bInUse.store(false, std::memory_order_release)`, and set the claim-table entry to 0xFFFF. The merge node runs after every sub-job and continuation of its system node ([Z2-N-ARCH-395]), so the hand-off from the slots to the merge is a graph edge and each entry is used by one thread at a time ([Z2-N-CONTRACT-233]); the release store is the only hand-off back that is not a graph edge. A transaction pointer read after its merge reads poisoned or retagged memory, which the `win-asan` preset and the S24 HWASan build report (`Test_Script_Txn_RetainedTransactionPoisonedAfterMerge`).

Growth bound (F frame threads; K_c the most chunk systems of one script phase; K_b the most behaviour systems of one script phase):

| Quantity | Bound |
|---|---|
| scratch memory | (F + 1) × S bytes, reserved at world creation, never grows |
| retention entries | F × K_c + K_b, fixed between registrations |
| retention memory | within a tick, the tick's retained transactions, whose sum is the per-tick snapshot total that Z2-B-SCRIPT-013 measures and reports (§ "Per-entity accounting implementation"); between merges, each entry holds only its one retained block — its first block: the W + 64-byte (at least 256 KiB) pre-reservation for entry 0 of a slot, and the block of the entry's first allocation otherwise — because `Reset()` returns every block after the first ([Z2-N-CONTRACT-231]); one retained block is what makes the tail argument of § "Retention ceiling derivation" hold, since every block after the first is new and sized to fit the allocation that requested it |
| breach | a root-heap failure: world halt with `RESERVATION`, the tick flagged non-replayable; the retention ceiling: `ZENITH_CHECK` (§ "Retention ceiling derivation") |
| reporting | every scratch and retention arena's `GetReservedBytes()` is part of `MEMORY_SUBSYSTEM_SCRIPTS` (Design/SCRIPT/Runtime.md → "VM pool per world") |

Every structure placed in these arenas is trivially destructible ([Z2-N-SCRIPT-861]), so `Reset()` and `ResetToMarker()` run no destructor and the [Z2-N-CONTRACT-230] check always passes; the one non-trivial member of an entry, `m_axTransactions`, lives in the entry object, not in an arena.

The retention memory adds nothing to the MEM-* sums of `Zenith2/Docs/Budgets.md`: it is transaction memory, which Budgets.md's allocation-bounds table already counts as drawn from Z2-B-SCRIPT-013 beside Z2-B-SCRIPT-012, -015 and -016, and the governor reads it under `MEMORY_SUBSYSTEM_SCRIPTS`.

## Retention ceiling derivation

The ceiling is computed for every script system by `RegisterScriptNodes`, and for a reload's staged systems by the prologue's schedule commit (Design/SCRIPT/HotReload.md → "Schedule commit in the prologue"), from the world's values of existing rows and capacities:

| Symbol | Value |
|---|---|
| C | `Zenith_ScriptWorldDesc::m_uSnapshotCapBytesPerEntity`, the world's Z2-B-SCRIPT-012 |
| E | the world's entity capacity, `Zenith_WorldDesc::m_uMaxEntities` (`ecs.maxEntitiesPerWorld`, `Zenith2/Docs/Design/ECS.md` → "Configuration keys") |
| V | the world's event capacity per tick, `ecs.maxEventsPerTick` (Z2-B-GAME-003) |
| W_s | the slot's worst case W of § "Transaction arena partition": `uSCRIPT_MAX_ROWS_PER_INVOCATION × C + 32,896` = 256 × C + 32,896 bytes for a frame-thread slot, plus the largest declared checkpoint slot (Z2-B-SCRIPT-015) for the main slot |
| Bk | `ulSCRIPT_TXN_RETENTION_BLOCK_BYTES`, 262,144 bytes (256 KiB), the arena's block size |
| B_inv | 5,248 bytes: the transaction header (128 B), the snapshot headers of at most 64 writable properties (64 × 64 B = 4,096 B) and the 64-byte alignment of at most 16 retention allocations of one invocation (1,024 B) |
| B_row | 96 + 5 × C bytes: the entity-array entry (64 B) and error-table entry (32 B) of one row, the entity's charged bytes (at most C: its row share, shadow reservations, SCRIPT records and payload and live-state snapshot, [Z2-N-SCRIPT-223]) and its uncharged shadow descriptors (`static_assert(sizeof(Zenith_CowShadow) <= 64)` in `Internal/Zenith_ScriptTransaction.h`; every shadow is charged at least 16 B, so its descriptor adds at most four times its charge, 4 × C in all) |
| N_inv, N_row | the invocations and rows one system can run on one slot in one tick. A chunk system processes each entity of its query once per tick and every invocation has at least one row, so N_inv ≤ E and N_row ≤ E. A behaviour system runs one row per invocation and at most 3 + V invocations per instance per node (`OnRecover`, `OnCreate`, one handler per delivered event of the tick, and `OnUpdate`, the `Run` segment or the checkpoint-only transaction), so N_inv = N_row ≤ E × (3 + V) |
| U | N_inv × B_inv + N_row × B_row, the most bytes one system can USE in one entry in one tick, every retention allocation counted with 64 bytes of alignment: E × (5,344 + 5 × C) for a chunk system, E × (3 + V) × (5,344 + 5 × C) for a behaviour system |

Ceiling = round_up(max(2 × U + Bk, W_s + 64), 256 KiB), computed in `uint64` with overflow-checked arithmetic that saturates at 2⁶⁴ − 1 (a ceiling above what the root heap can supply then means that the heap's refusal, a `RESERVATION` halt of [Z2-N-SCRIPT-225], comes first).

Why 2 × U + Bk bounds what `Zenith_Arena` checks. The cap applies to the nominal total — used bytes plus the nominal size of every block ([Z2-N-CONTRACT-261]) — and is checked only when a new block is requested (`Zenith2/Docs/Contracts/Memory.md` → "Zenith_Arena", `TryAllocate` step 4). Every block switch abandons the old block's tail. The entry keeps one retained block (§ "Transaction arena partition"), so within a tick the cursor starts in that block, and a new block is requested only after the cursor has left it; every block after it is new and sized `max(Bk, a + 64)` for the allocation a that requested it, so a moves to exactly the next block and fits there. Each abandoned tail is therefore smaller than the allocation that moved on plus its alignment (64 bytes), and each allocation moves on at most once, so the tails together are at most U, the sum over all of the tick's allocations of their sizes plus 64. At the request for allocation a, the nominal total is the used bytes so far, plus the tails, plus the new block: when `a + 64 ≤ Bk` that is at most U + U + Bk; when it is larger, the new block is `a + 64`, which the used-bytes bound already contains, so the total is at most 2 × U. Hence the reserved total never exceeds 2 × U + Bk. The second term admits the pre-reservation of rule 1: entry 0's `TryAllocate(W_s, 64)` requests a block of nominal size W_s + 64 while nothing else is used, which 2 × U + Bk need not cover in a small world.

Worked case: C = 4 KiB and E = 16 (a test or preview world) give W = 256 × 4,096 + 32,896 = 1,081,472 bytes on a frame-thread slot, U = 16 × (5,344 + 20,480) = 413,184 bytes for a chunk system, 2 × U + Bk = 1,088,512 and W + 64 = 1,081,536, so the chunk-system ceiling of every frame-thread slot is round_up(1,088,512, 256 KiB) = 1,310,720 bytes — the same value as round_up(W + 64, 256 KiB) — which admits the pre-reservation, and world creation succeeds; the formula without the second term and the factor 2 gave 524,288 bytes, below the 1,081,536-byte pre-reservation, so `RegisterScriptNodes` failed with `CAPACITY_EXCEEDED`.

The bound is loose by design: it must hold for every world within its rows, and a legal world's retention is measured by Z2-B-SCRIPT-013, not bounded by this value. `Test_Script_Txn_RetentionCeilingDerived` checks the formula, the small world and the fragmentation case, and `Test_Script_Txn_RetentionCeilingIsCheck` that reaching it is a `ZENITH_CHECK` and never a script error.

## Invocation lifecycle

One invocation, from issue to release (steps name the node that runs them):

| # | Node | Step |
|---|---|---|
| 1 | script node | issue the invocation ID; set `xVM.m_xCurrentInvocationID`; claim the system's retention entry on this slot when this is its first invocation here this tick; record the scratch marker (§ "Transaction arena partition" rules 2 and 3) |
| 2 | script node | construct `Zenith_ScriptTransaction` with its entity array and error table in the retention arena: invocation ID, `Zenith_ScriptSystemRef`, row range, `m_xArenaMarker`; the growing structures start in the scratch arena (§ "Invocation layout in the arenas") |
| 3 | script node | for each row (chunk system) or for the one invocation (behaviour): open a `Zenith_EntityTransaction` (`ENTITY_TXN_STATE_OPEN`), record its logical ID (the row's stable simulation ID from `Zenith_ChunkView::GetSimulationIDs`, and the ordinal), obtain its ECS command buffer `xLeaf.Commands(ulSimulationID, uOrdinal)`, event writer and contribution writer (§ "Side-effect routing"), reset the VM's safepoint count to 0 and its limit to the entity's budget, clear `m_bAbortLatched`, set the entity's draw counter to 0 |
| 4 | script node | run the script (the row trampoline's `lua_call` of `Each`, or the behaviour's call or resume); writes and side effects follow § "Write path implementation" and § "Side-effect routing" |
| 5 | script node | on return or yield: `ENTITY_TXN_STATE_SUCCEEDED`; for the instance's final invocation with dirty state, serialize the checkpoint into the inactive slot (HotReload.md → "Checkpoint serializer"), a failure being a `CHECKPOINT` error of this entity |
| 6 | script node | on error: § "Failure handling by kind" (restore the row, discard, record) |
| 7 | script node | after the last row: copy the retained part of the growing structures into the retention arena and reset the scratch arena to `m_xArenaMarker` (§ "Invocation layout in the arenas"); append the transaction pointer to the claimed entry's `m_axTransactions`, which only this slot's thread writes, so the sub-jobs of one node on several slots never share a list |
| 8 | merge node | § "Merge node body": collect every claimed entry's transactions, sort, accept or reject, fold, commit, release the entries |
| 9 | tick boundary | failures recorded by the merge disable their module ([Z2-N-SCRIPT-189]) |

The per-entity safepoint reset at step 3 happens inside the C++ row trampoline, so no trampoline code is counted and each entity's count depends on its own processing ([Z2-N-SCRIPT-150]).

## Invocation layout in the arenas

<!-- assumption: pending core-doc (X116) -->
`Zenith2/Docs/Scripting/TransactionStructures.md` → "Transaction data structures" annotates `Zenith_ScriptTransaction::m_xArenaMarker` with "everything below is in the worker's transaction arena". In this design `m_xArenaMarker` is the marker of the slot's scratch arena taken at step 1 of the invocation, and the transaction with its spans lives in the (system, slot) retention arena; the comment changes with X116.
<!-- /assumption -->

| Structure | Arena | Allocated | Size |
|---|---|---|---|
| `Zenith_ScriptTransaction` header | retention | step 2 | 128 B |
| entity array (`m_axEntities`) | retention | step 2 | rows × `sizeof(Zenith_EntityTransaction)` (64 B) |
| error table | retention | step 2 | rows × `sizeof(Zenith_ScriptError)` (32 B); written on failure only |
| snapshot headers and row bytes (`m_axSnapshots`, `m_puRowBytes`) | retention | at the first write, when the writable properties the chunk binds and the row count are known | properties × `sizeof(Zenith_ColumnSnapshot)` + Σ field bytes × rows |
| live-state snapshot of a behaviour instance | retention | at the first state write (Design/SCRIPT/HotReload.md → "State proxy implementation") | the live-state block size |
| side-effect records | scratch while the invocation runs; retained copy at step 7 | a doubling array starting at 16 records | 16 B each |
| payload bytes | scratch; retained copy at step 7 | a doubling byte array starting at 256 B | as the bindings serialized them |
| shadow descriptors and shadow data | scratch; the dirty shadows' copies at step 7 | a doubling descriptor array starting at 4; each shadow's data doubling (§ "Copy-on-write shadow implementation") | as charged |

Why S bounds the scratch use: every live byte of a growing structure is charged to its entity before it is allocated (§ "Per-entity accounting implementation"), so the live total is at most W; a doubling structure has abandoned at most as many bytes as it holds live, so the arena holds at most 2 × W plus 16-byte alignment padding, which the extra 64 KiB of S covers for the at most 4,096 allocations an invocation can make (each structure doubles at most 32 times, and ZS1402 stops an entity's shadows long before 4,096 of them).

Step 7 copies, into one retention allocation of 16-byte-aligned regions: the records of every entity whose state is `SUCCEEDED` (a failed entity's records are discarded, never copied), their payload bytes, and the dirty shadows' descriptors and data; it then sets `m_axRecords`, `m_axPayload` and `m_axShadows` to the copies. Record payload offsets are relative to the payload span's base and each copied shadow's `m_pShadowData` is set to its copy, so no other field changes. Then `ResetToMarker(m_xArenaMarker)` releases and poisons the whole scratch range of the invocation. The invocation's retained size is the sum of its retention allocations, which is what the per-tick total of § "Per-entity accounting implementation" accumulates.

`m_axSnapshots`, `m_axEntities`, `m_axRecords`, `m_axShadows` and `m_axPayload` of `Zenith_ScriptTransaction` are spans over retention memory from step 7 until the merge releases the entry.

## Write path implementation

`Scripting.md` → "Write path algorithm" is the algorithm; `Internal/Zenith_ScriptViews.cpp` implements steps 1–9 with these structures:

- The declaration's writable field table: a perfect-hash map (built at load, Design/SCRIPT/Bindings.md → "Userdata tags and view objects") from interned key string to `Zenith_ScriptFieldBinding { Zenith_TypeID m_xType; Zenith_PropertyID m_xProperty; uint8 m_uWritableIndex; Zenith_PropertyValueType m_eType; uint32 m_uFieldBytes; bool m_bOutOfLine; bool m_bHandle; }`; `m_uWritableIndex` indexes the invocation's `Zenith_RawColumn` cache (filled from `Zenith_ChunkView::GetColumn` when the view is created, so step 8 does no ECS call).
- Step 7 (first-write snapshot): § "Snapshot capture and restore".
- Step 8 stores the converted bytes and calls `Zenith_RawColumn::MarkRowChanged(r)` on the cached column; the pending bits are the invocation's own (`m_pulRowChangeBits` allocated by `GetChunkInScope` in the leaf's arena), so no other job's bits are touched ([Z2-N-SCRIPT-216]).
- Behaviour writes use the entity's row in the view `GetChunkOfEntityInScope` returns (`Zenith2/Docs/Design/ECS/SystemAPI.md`; Runtime.md → "Node registration and frame-graph placement") with `m_uRowCount = 1` snapshots ([Z2-N-SCRIPT-215]).

## Snapshot capture and restore

Capture (on the first write of the invocation, [Z2-N-SCRIPT-214]):

1. For each property of the declaration's expanded write set that the chunk binds (in property order): write a `Zenith_ColumnSnapshot` header — chunk ordinal, type and property IDs, the cached `Zenith_RawColumn`, `m_uFirstRow` = the invocation's first row, `m_uRowCount` = its row count, `m_uFieldBytes` — and gather the rows' bytes into the snapshot row region through the row-index table.
2. Charge each entity of the invocation `round_up_8(Σ field bytes)` (its row share); an entity whose share alone exceeds the cap fails with `SNAPSHOT_CAP` at its first write, which is layout-independent because the share depends on the declaration and archetype alone.
3. Set `m_bChunkSnapshotTaken`.

Restore of row r (failure at step 6, rejection at the merge): for each snapshot, copy `m_uFieldBytes` from `m_puRowBytes + (r − m_uFirstRow) × m_uFieldBytes` to the row's address in the recorded column, and clear r's pending bit. The recorded `Zenith_RawColumn` stays valid until the merge because no structural playback runs between a phase's invocations and its merge nodes (sync points follow the phase), which is the lifetime `Zenith_ChunkView` guarantees until its invocation's merge ([Z2-N-SCRIPT-185]).

## Copy-on-write shadow implementation

`Scripting.md` → "Copy-on-write shadows" steps 1–5; implementation specifics:

| Step | Detail |
|---|---|
| creation | the shadow's source is the most recent dirty shadow of the same (entity, property) among the earlier invocations of the same behaviour instance in this node — retained in the retention arena until the merge — or, when there is none, the container's committed storage read through `Zenith_ContainerRef` (`Zenith2/Docs/Architecture.md` → "Data-defined components"); the shadow reserves `max(16, source count × 3 / 2)` elements, charged to the entity. Chaining on the earlier invocation is what makes two invocations of one instance in one node (a handler, then `OnUpdate`) compose instead of the second replacement overwriting the earlier one; it is safe because a subsequent invocation of an instance is rejected whenever an earlier one of the same node is failed or rejected ([Z2-N-SCRIPT-252]) |
| growth | doubling, charged before copying; exceeding the cap raises ZS1402 (`SNAPSHOT_CAP`) with the shadow unchanged |
| string fields | a string field written by a behaviour becomes a shadow of UTF-8 bytes; at commit it is interned with CORE's `Zenith_StringTable::Intern` (ANY_THREAD, `Zenith2/Docs/Design/CORE/Text.md` → "String interning"; capacity class string intern, accepted in § "Capacity acceptance across ECS and SCRIPT classes") and the resulting `Zenith_StringID` is written in place into the row's column with the row's pending change bit set, so the merge's `CommitRowChanges` publishes it at the merge version |
| commit | the merge accepted the container-heap bytes (ECS class `CONTAINER_BYTES`); commit calls `xMerge.ReplaceContainer(xView, uRow, xType, xProperty, xElements, uCount)`, which copies the shadow into a container-heap block ECS allocates, swaps the row's container reference in place, marks the row changed at the merge version and queues the previous block for ECS's deferred free |
| drop | nothing to do: the shadow lives in the scratch arena, which step 7 resets |

Visibility is therefore Scripting.md's: a committed container or string field is visible to every reader after the merge point, exactly as a declared-writable column is (table "What is transactional"; [Z2-N-SCRIPT-217]), because a node that reads it has its edge from the merge node ([Z2-N-SCRIPT-159]); the script reads its own mutations through the shadow for the rest of the invocation. ECS keeps the container heap and its deferred free; the swap happens inside the merge, through ECS.

`Zenith_MergeContext::ReplaceContainer(Zenith_ChunkView& xView, uint32 uRow, Zenith_TypeID xType, Zenith_PropertyID xProperty, Zenith_ByteSpan xElements, uint32 uElementCount)` is ECS's member (`Zenith2/Docs/Design/ECS/SystemAPI.md` → "Zenith_MergeContext header"; cross-doc row X192 applied): it cannot fail (the `CONTAINER_BYTES` demand was accepted before commit), it allocates the new block from the world's container heap, swaps the reference in the row, marks the row changed at `GetMergeVersion()` and defers the old block's release to ECS's end-of-tick free. No fallback path exists: containers commit at the merge exactly as strings do.

## Side-effect routing

`Scripting.md` → "Side-effect sub-buffers" gives each entity transaction a private sub-buffer. This design keeps one private buffer per entity split over two carriers, so ECS kinds reuse ECS's merge and playback:

| Kind | Carrier | Recorded through | At the merge |
|---|---|---|---|
| `SIDE_EFFECT_KIND_SPAWN`, `DESTROY`, `ADD_COMPONENT`, `REMOVE_COMPONENT`, `REPARENT`, `FIELD_COMMAND` | the entity's `Zenith_CommandBuffer` (`xLeaf.Commands(simID, ordinal)`) | `Zenith_ScriptInvocationContext::AppendSideEffect`, which encodes the ECS record | `AcceptBuffer` or `RejectBuffer` |
| `SIDE_EFFECT_KIND_EVENT` | the same buffer, `ECS_RECORD_EVENT` through the entity's `Zenith_EventWriter` | as above | as above |
| `SIDE_EFFECT_KIND_CONTRIBUTION` | the same buffer, through `Zenith_ContributionWriter::Contribute(slot, value)` | `ctx:Contribute(name, value)` resolved to the reduction slot at load | folded by `Zenith_MergeContext::FoldReductions` over accepted buffers ([Z2-N-SCRIPT-183]) |
| `SIDE_EFFECT_KIND_AUDIO_TRIGGER`, `VFX_TRIGGER`, `SAVE_REQUEST`, `ASYNC_REQUEST`, `XR_RECENTRE` (a zero-byte payload, consumed by ZenithXR from Stage 7; cross-area row SP7, cross-doc row X450) | the transaction's `Zenith_SideEffectRecord` list and payload region | `AppendSideEffect` | delivered at commit to the kind's `Zenith_ScriptSideEffectConsumer` |

Both carriers are private to the entity until the merge accepts it ([Z2-N-SCRIPT-219]). Each entity's `Zenith_SideEffectBuffer` counts its SCRIPT records (`m_uFirstRecord`, `m_uRecordCount`, `m_uNextSequence`) and its demand for every class (`m_xDemand`, including the ECS classes computed by `Zenith_MergeContext::ComputeDemand` of its command buffer at the merge). The merge key of an ECS record is ECS's (system order, simulation ID, ordinal, record sequence); SCRIPT records carry `m_uSequence` from the same per-entity counter, so every queue a consumer or ECS receives is in (system order, logical invocation ID, sequence) order ([Z2-N-SCRIPT-271]).

`AppendSideEffect` checks, in order: the binding's `m_uSideEffects` mask (else `SCRIPT_THUNK_STATUS_ERROR`, [Z2-N-SCRIPT-563]); the invocation kind allows the side effect (a chunk system cannot issue `ASYNC_REQUEST`: tickets are held only by behaviours, so a system gets ZS1702); the per-entity charge (record + payload bytes; over the cap → ZS1402); then writes. Nothing reaches a world queue during the invocation ([Z2-N-SCRIPT-219] step 4).

A provisional spawn: `ctx:Spawn` records the ECS spawn record, which yields a `Zenith_SpawnTicket` (`Zenith2/Docs/Design/ECS/WorldAPI.md` → "Zenith_ECSTypes header"); the returned `Zenith.Entity` carries the provisional bit and the ticket's invocation-local index. Commands naming it are recorded with the ticket; ECS remaps tickets to entities at playback, and after the sync point SCRIPT resolves a provisional handle through `Zenith_WorldSimulation::GetSpawnResults()` (the one-tick remap table of [Z2-N-SCRIPT-222]); a handle still provisional after the following sync point raises ZS1103. A rejected or failed entity's spawn records are never played back, so no entity or simulation ID is consumed ([Z2-N-SCRIPT-221]).

## Per-entity accounting implementation

Each entity transaction's `m_uMemoryBytes` is charged, before each allocation, with the amounts of `Scripting.md` → "Per-entity memory accounting": the row share at the first write, each shadow's reserved capacity, each SCRIPT record (16 B) plus payload, and each ECS record's encoded size (the size `Zenith_CommandBuffer` reports for the record). A charge that would exceed `m_uSnapshotCapBytesPerEntity` raises ZS1402 (`SNAPSHOT_CAP`) for that entity ([Z2-N-SCRIPT-223]). The per-tick total (Z2-B-SCRIPT-013) is the sum of every invocation's retained size (§ "Invocation layout in the arenas"), accumulated at step 7 into the slot's counter and reported at the tick boundary; exceeding the row emits `SCRIPT_SNAPSHOT_TOTAL_EXCEEDED` and never rolls anything back ([Z2-N-SCRIPT-187]).

## Merge node body

`Zenith_ScriptMergeNodeRun(Zenith_MergeContext& xMerge)` (`Internal/Zenith_ScriptMerge.cpp`), which runs after every sub-job of the system node and after the previous merge of the phase's chain:

1. If the world is halted, return (the tick commits nothing, Runtime.md → "World halt procedure"); if the module's `m_bDisabled` is set, return without committing or folding anything (its node ran no invocation this tick; Runtime.md → "Tick-boundary handler").
2. For every slot whose claim-table entry is set, walk the entry's `m_axTransactions`; collect every `Zenith_EntityTransaction` whose state is `SUCCEEDED` into a candidate array in the merge scratch (`xMerge.GetScratch()`), with a pointer to its transaction; collect `FAILED` entries into the failure list. The slot order of the walk is irrelevant, because step 3 sorts.
3. Sort candidates by (simulation ID, ordinal) with an LSD radix sort (12 passes over the 96-bit key, O(n)); failures likewise. The ordinal digit is `uOrdinal + 1` modulo 2^32, so an instance's recovery transaction (`uSCRIPT_RECOVERY_ORDINAL`) sorts before its ordinary invocations (HotReload.md → "Recovery invocations and exact restore" step 3; [Z2-N-SCRIPT-928])
4. For each candidate in order: compute the demand (§ "Capacity acceptance across ECS and SCRIPT classes"); if it fits, accept (subtract, `ENTITY_TXN_STATE_ACCEPTED`); otherwise reject: restore its rows (§ "Snapshot capture and restore"), `xMerge.RejectBuffer(buffer)`, mark its SCRIPT records discarded, record `SCRIPT_MERGE_REJECTED_CAPACITY` (module, tick, simulation ID, class, demand, remaining), `ENTITY_TXN_STATE_REJECTED`; for a behaviour instance, reject every subsequent candidate of the same instance in this node and restore the row to the earliest failed or rejected invocation's snapshot ([Z2-N-SCRIPT-252]).
5. For each failed entry: `xMerge.RejectBuffer(buffer)` (its buffer was already abandoned at failure; rejecting it keeps ECS's accounting of every buffer the invocation created exact).
6. Commit (§ "Commit implementation").
7. Record the node's failures for the tick boundary: the module's `m_bDisableAtNextTickBoundary` and its first failure in merge order.
8. Release every claimed retention entry of the system (§ "Transaction arena partition" rule 5).

Nothing in steps 1–8 touches a Luau object or main-VM state. A rejection restores rows and live-state blocks from the retained snapshots and marks SCRIPT records discarded; a behaviour instance is quarantined only at failure time, on the main thread, by its behaviour node (§ "Failure handling by kind"), never by a merge; and the instance-record fields the merge writes (the active checkpoint slot index, the dirty flag, the consumed-result marks) belong to the node's own module, whose next reader — its next behaviour node or the tick boundary — is ordered after this merge by the phase's sync point. The behaviour merge node therefore runs with the `ANY` affinity ECS gives every merge node (`Zenith2/Docs/Design/ECS/Scheduling.md` → "Node and resource naming"), on whatever frame thread the executor picks, and needs no affinity of its own; `Test_Script_Merge_BehaviourMergeOffMainThread` forces it onto a worker.

## Capacity acceptance across ECS and SCRIPT classes

The nine classes of `Scripting.md` → "Capacity classes" split between the two holders:

| Class | Holder | Remaining at phase start |
|---|---|---|
| entity slots, command queue, event queue, container storage | ECS `Zenith_MergeCapacity` (classes `ECS_CAPACITY_CLASS_ENTITY_SLOTS`, `COMMAND_RECORDS`, `EVENT_RECORDS`, `CONTAINER_BYTES`) | `Zenith2/Docs/Design/ECS/Scheduling.md` → "Capacity acceptance" |
| string intern | SCRIPT (`Zenith_ScriptCapacityLimits`) | row minus entries and bytes interned this world session |
| audio triggers, VFX triggers, save requests | SCRIPT | row minus accepted this tick |
| async requests | SCRIPT, per source | row minus in flight or published-unconsumed (Runtime.md → "Asynchronous result table") |

Fit test for one candidate (all classes at once, first-fit, [Z2-N-SCRIPT-333]): compute `xECS = xMerge.ComputeDemand(buffer)` and the SCRIPT demand from `m_xDemand`; accept only if every SCRIPT class fits and `xMerge.GetCapacity().TryAccept(xECS)` returns true; `TryAccept` changes nothing when it fails, and SCRIPT subtracts its classes only after `TryAccept` succeeded, so a rejection leaves both holders unchanged. Acceptance is a pure function of the merge-ordered demands and the starting capacities ([Z2-N-SCRIPT-333]). String-intern demand is the bytes of strings the candidate's records and shadows would intern that are not yet interned in the world (checked with a read-only lookup of CORE's `Zenith_StringTable`, `Zenith2/Docs/Design/CORE/Text.md` → "String interning"); interning itself happens at playback.

## Commit implementation

For each accepted candidate in merge order ([Z2-N-SCRIPT-248]; nothing here can fail):

1. `xMerge.AcceptBuffer(buffer)`: the entity's ECS records join the phase's command and event queues in merge order.
2. SCRIPT records in sequence order: `ASYNC_REQUEST` → `AllocateCompletionID(kind)` (capacity accepted in step 4, so it succeeds), stamp the completion's payload slot with the requester (`m_xOwnerModule`, `m_ulOwnerSimulationID`: the instance's module and simulation ID), so its publication stays in the published table for the ticket even when the kind has a C++ handler (Design/SCRIPT/Runtime.md → "Asynchronous result table"), remap the entity's provisional ticket to the completion ID in the tick's remap table, then `consumer->Consume(tag, kind, completionID, payload)`; `AUDIO_TRIGGER`, `VFX_TRIGGER`, `SAVE_REQUEST` → `consumer->Consume(tag, kind, 0, payload)`.
3. Dirty shadows → `ReplaceContainer` for a container field and intern-and-write-in-place for a string field (§ "Copy-on-write shadow implementation"), in the entity's shadow order; both mark the row changed before step 7 runs.
4. A checkpoint serialized in this transaction → flip the instance's active slot index; clear its dirty flag ([Z2-N-SCRIPT-391]).
5. Asynchronous results read by this invocation → mark them consumed (released at the next tick boundary).
6. `ENTITY_TXN_STATE_COMMITTED`.

Then once for the node:

7. For every writable view the system's invocations created, `xMerge.CommitRowChanges(view, type, property, acceptedBits)` with the pending bits of accepted entities only (bits of rejected and failed entities were cleared at restore), which advances the column's change version at the merge's version ([Z2-N-SCRIPT-245], [Z2-N-SCRIPT-186]).
8. `xMerge.FoldReductions()`: accepted contributions in merge order through GRAPH's fixed-order tree; the tree equals Scripting.md's (`Zenith2/Docs/Design/ECS/Scheduling.md` → "Events and reductions"), so `Test_Script_Reduction_FixedOrderTree`'s reference fold applies unchanged.

A commit changes versions only, never a structural epoch, so no other invocation's view becomes invalid ([Z2-N-SCRIPT-246]).

## Failure handling by kind

On the invocation's thread, at the moment the protected call returns an error (Scripting.md → "Execution-time failure algorithm" steps 1–6):

| Kind (`Zenith_ScriptErrorKind`) | Entity outcome | Module | World |
|---|---|---|---|
| `RUNTIME`, `MISUSE`, `BUDGET`, `YIELD_IN_SYSTEM`, `SNAPSHOT_CAP`, `CHECKPOINT` | restore the row, clear pending bits, drop shadows, abandon the command buffer (it will be rejected at the merge), mark SCRIPT records discarded, store the error in the error table, `ENTITY_TXN_STATE_FAILED`; a behaviour instance is quarantined (§ "Quarantine store") and its remaining invocations of the tick are skipped; a chunk system continues at the next row | `m_bDisableAtNextTickBoundary` set at the merge | unaffected |
| `RESERVATION`, `VM_MEMORY`, `WATCHDOG`, `INTERNAL` | none: the tick is abandoned | — | halted (Runtime.md → "World halt procedure") |
| `COMPILE`, `LOAD` | cannot occur inside an invocation (compile happens on the host; load at module scope is a load failure of the module) | disabled at load | unaffected |

A chunk system whose row fails re-enters the trampoline at the next row inside the same protected call's successor (`uFirstRow = uFailedRow + 1`), because the Luau stack of the failed call was unwound by the protected call ([Z2-N-SCRIPT-149]).

## Quarantine store

Quarantine keeps nothing in the Luau heap ([Z2-N-SCRIPT-177], [Z2-N-SCRIPT-421]); the store is the instance record itself plus a per-world list:

| Data | Where | Released |
|---|---|---|
| active checkpoint slot (the last committed state) | the instance's pooled slot pair, untouched | at instance destruction |
| preserved unknown fields | the instance's unknown-field block in the checkpoint pool | with the slot pair |
| the failure (`Zenith_ScriptError` and diagnostic sequence) | `Zenith_ScriptQuarantineEntry { uint64 m_ulSimulationID; Zenith_ScriptModuleID m_xModule; uint64 m_ulTick; Zenith_ScriptError m_xError; }` in the world's quarantine list (sorted by module, simulation ID) | at last-good restore or destruction |
| Luau objects (`self`, state proxy, coroutine, retained callbacks, published-unconsumed async results) | released at quarantine: `lua_unref` of each reference on the main VM; results released in the table | — |
| pending events for subsequent ticks | dropped, counted in `SCRIPT_EVENTS_DROPPED_QUARANTINE` | — |

Quarantine runs on the main thread, in the behaviour node whose invocation failed (behaviours run there), and never in a merge node; a behaviour rejected at its merge is not quarantined (rejection is not a fault, [Z2-N-SCRIPT-241]) — its row is restored and it runs normally next tick.

## Diagnostic store implementation

<!-- assumption: pending core-doc (X115) -->
The diagnostic store exists because the Contracts-form `Zenith_ScriptError` carries no text (Design/SCRIPT/PublicAPI.md → "Zenith_ScriptError header"); Scripting.md's declared error still carries the message and traceback until cross-doc row X115 lands.
<!-- /assumption -->

`Zenith_ScriptDiagnosticStore` (one per world, one in the runtime): `uSCRIPT_DIAGNOSTIC_RING_ENTRIES` (64) slots of `{ std::atomic<uint32> m_uBusy; uint32 m_uSequence; Zenith_ScriptDiagnostic m_xDiagnostic; }`, `std::atomic<uint32> m_uNext` (starting at 1) and `std::atomic<uint32> m_uDropped`.

- **Write** (any thread of the world, at the point of failure): `seq = m_uNext.fetch_add(1, std::memory_order_relaxed)` — relaxed, because the counter only has to hand out unique values and publishes no data; slot = `seq % 64`; `if (slot.m_uBusy.exchange(1, std::memory_order_acquire) != 0)` another writer holds the slot (a writer 64 sequences ahead that has not finished): the diagnostic is dropped, `m_uDropped.fetch_add(1, std::memory_order_relaxed)`, and `seq` is still returned (a subsequent read of it reports false). Otherwise copy the diagnostic, set `m_uSequence = seq`, then `slot.m_uBusy.store(0, std::memory_order_release)`, which publishes the copy to the next acquirer of the slot.
- **Read** (`GetDiagnostic(seq, xOut)`): permitted out of tick on the main thread and on the world's tick-boundary node, both of which the frame graph orders after every node that writes the world's store (every script and merge node of the previous tick precedes the tick boundary, [Z2-N-ARCH-383]); the read is `slot.m_uBusy.load(std::memory_order_acquire) == 0 && slot.m_uSequence == seq`, then a plain copy. The runtime's store is written and read on the main thread alone.
- **In-tick text**: a thunk that needs the text of an error `InvokeCallback` just returned calls `Zenith_ScriptInvocationContext::GetErrorMessage`, which copies from the VM's own error scratch (`m_xErrorScratch`, written by the nested protected call on the same thread) when the scratch's sequence equals the error's, and returns 0 otherwise; it never reads the store, so no in-tick reader races a writer.

Sequence numbers depend on failure timing across workers, so they are diagnostic only: no simulation value, merge decision or replayed quantity reads them, and the fault-matrix comparisons use (tick, module, simulation ID, code) tuples ([Z2-N-SCRIPT-542]). `m_uDropped` is published as `script.diagnostics_dropped`.

## Worked lifecycle traces

The worked example of `Scripting.md` → "Worked example three entities second fails" maps onto this design as follows (worker 3 is frame-thread slot 3, VM index 4):

| Scripting.md step | This design |
|---|---|
| 0 | the leaf on slot 3 claims a retention entry of slot 3 for `Script.Combat.RegenerateStamina`; transaction header, entity array of 3 and error table in it; records and payload start in slot 3's scratch arena |
| 1 | first write: snapshot header for `Stats.Stamina` (4 B × 3 rows); each entity charged 8 B |
| 2 | `ctx:Emit` → `ECS_RECORD_EVENT` in A's command buffer; `ctx:Contribute` → two contribution records in A's buffer |
| 3–4 | B raises: row 1 restored, pending bit cleared, B's buffer abandoned, error table entry 1, B `FAILED` |
| 5–6 | re-entry at row 2; C succeeds; A's and C's records copied into the retention entry (B's discarded); scratch reset to the marker; transaction appended to the entry's list |
| 7 | merge: candidates A (1007), C (1011) sorted |
| 8–9 | A's demand {EVENT_RECORDS 1, COMMAND_RECORDS 2 (contributions count as records)} fits; C's event does not: row 2 restored, `RejectBuffer`, `SCRIPT_MERGE_REJECTED_CAPACITY` |
| 10–11 | `AcceptBuffer(A)`; `CommitRowChanges(Stamina, bits{row 0})`; `FoldReductions` writes `TotalStamina = 100`, `FullCount = 1` |
| 12 | the merge releases slot 3's entry (arena reset and poisoned); tick boundary step 3: `Combat.RegenerateStamina` disabled; listener told |

The event-queue capacity used by the example (one free slot) is the ECS `EVENT_RECORDS` remaining capacity; whether contribution records count against `COMMAND_RECORDS` is ECS's `ComputeDemand` rule, so the test sets both remaining capacities explicitly.
