# ECS design — systems, commands and playback

This file is part of the ECS design family; the entry document is `Zenith2/Docs/Design/ECS.md`. It specifies the scheduling contract ECS owns (Z2-I-ECS-09, Z2-I-ECS-05, Z2-I-ECS-17): the system registry, the per-field authority and schedule checks with their exact messages, the translation of a world's systems into the CPU frame graph (`Zenith_WorldSimulation::EmitInto`), merge nodes and the merge chain, command buffers and their record format, sync-point playback, the tick boundary and end-of-tick playback, the arbitration merge, events and reductions, out-of-tick playback, the world registry and the world-state hash. The specifications are `Zenith2/Docs/Architecture/FramePhases.md` (whose phase table, playback steps, authority rules, arbitration rules and message table are used verbatim), `Zenith2/Docs/Architecture/ECSModel.md` → "Command buffers", "Scheduling contract", "World registry" and `Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Frame graph builder" (subsection "Translation into the graph core").

## System registry internals

| Record | Fields | Notes |
|---|---|---|
| system | name (registry arena copy), kind, phase, run and merge functions, user data, affinity, writer source, cost hint, registration index, enabled flag, disable-at-next-boundary flag, tombstone flag, previous read version P (the read version of its latest completed invocation, 0 until the first; `Zenith_SystemContext::GetPreviousReadVersion`) | index = registration order ([Z2-N-ARCH-722]) |
| access | system index, type ID, property ID (0 = whole type before expansion), mode (read, write, any-row read, resource read, resource write) | expanded per property at Freeze ([Z2-N-ARCH-400]) |
| query slot | system index, normalised query, compiled `Zenith_QueryID`, last-seen version L, own version O | one compiled query per slot (`Zenith2/Docs/Design/ECS/Queries.md` → "Change-filtered queries") |
| ordering | system index, direction, target name, required flag | resolved at Freeze |
| event declaration | system index, event type, emits or reads | |
| reduction | system index, name, target type, target property, kind | |
| world resource | type ID, owning phase, pointer | added by `AddWorldResource` ([Z2-N-ARCH-719]) |
| diagnostics | `Zenith_ScheduleDiagnostic` array and text arena | of the latest Freeze or EmitInto ([Z2-N-ARCH-506]) |

`Freeze` runs § "Authority and schedule checks", compiles every query slot into the world (through the `Zenith_World` the world simulation bound at `Create`), computes the own-row marking (`Zenith2/Docs/Design/ECS/Queries.md` → "Own-row and any-row reads"), the merge points in each stage's order with every system's read point, and the merge versions (`Zenith2/Docs/Design/ECS/Queries.md` → "Stage order of merge points", "Change versions and merge versions"; a tick of 2^20 or more merge points fails `Freeze`, item 1 of the latter) and the arbitration staging set, and tells the backend (`SetArbitrationStaging`). A reload scope (`BeginReload` … `Commit`) edits a copy of the declarations and re-runs `Freeze` on the copy; a failure keeps the previous declarations ([Z2-N-ARCH-409]). `SetEnabled` records the request; the tick-boundary node applies it at step 3 ([Z2-N-ARCH-410]), never recompiling. `GetDeclarationHash` is `Zenith_Hash64Stream` over every record in index order, excluding costs and user data; it changes exactly when a recompile is needed.

<!-- elaboration -->
A failed frame-graph `Compile` after a successful `EmitInto` is reported back to the registry by the composition root through `Zenith_SystemRegistry::RecordGraphFailure(Zenith_Span<const Zenith_FrameGraphDiagnostic>)`, which appends one `SCHEDULE_ERROR_GRAPH` diagnostic per graph diagnostic, text `schedule: <graph text>`, node indices mapped back to registration indices; SCRIPT reads them to reject a reload (`Zenith2/Docs/Design/GRAPH.md` → "Front-end error mapping").

## Authority and schedule checks

### Check order

`Freeze` (and `EmitInto` when the registry changed) checks every declaration in one pass, collecting every error ([Z2-N-ARCH-498]):

1. **Names**: every system and epilogue-stage name 1 … 127 bytes with no `@` (`SCHEDULE_ERROR_INVALID_NAME`; the graph core reserves `@` for group qualifiers, cross-area note G13a); names unique in the world (`SCHEDULE_ERROR_DUPLICATE_SYSTEM_NAME`).
2. **Phases**: `PHASE_NONE` on a system (`SCHEDULE_ERROR_PHASE_NONE`; epilogue stages are exempt, [Z2-N-ARCH-726]); a behaviours system outside 2 and 10 (`SCHEDULE_ERROR_BEHAVIOUR_PHASE`, [Z2-N-ARCH-473]); a script chunk system outside 2 and 10 (`SCHEDULE_ERROR_SCRIPT_PHASE`, [Z2-N-ARCH-501]); a native system in 11 is accepted with component reads and writes of phase-11-owned world resources only (step 5). No system reaches `Freeze` with phase 12, `PHASE_COMMANDS_ONLY` or a value above 12: `AddSystem` rejected it at the call with `ECS_ERROR_PHASE_NOT_SCHEDULABLE` (`Zenith2/Docs/Design/ECS/SystemAPI.md` → "Zenith_System header"), because the TICK subgraph holds phases 1 to 11 and phase-12 work is EPILOGUE work registered through `AddEpilogueStage` or the extractor path ([Z2-N-ARCH-384], [Z2-N-ARCH-502]). <!-- assumption: pending core-doc batch 4 (X84) -->
3. **Types and properties**: every type registered (`SCHEDULE_ERROR_UNKNOWN_TYPE`); every property in the type's current revision (`SCHEDULE_ERROR_UNKNOWN_PROPERTY`).
4. **Expansion**: a whole-type declaration becomes one access per property ([Z2-N-ARCH-400], [Z2-N-ARCH-487]); a reduction target becomes a write of the target property ([Z2-N-SCRIPT-184]); a query term with `m_bWrite` must match a declared write and every other named property a declared read (`SCHEDULE_ERROR_QUERY_ACCESS_MISMATCH`, [Z2-N-ARCH-701]).
5. **Authority per write**, for system phase P and property authority A: A = `PHASE_COMMANDS_ONLY` → `SCHEDULE_ERROR_COMMANDS_ONLY_WRITE` ([Z2-N-ARCH-485]); a component write with P = 11 or from an epilogue stage → `SCHEDULE_ERROR_READ_ONLY_PHASE_WRITE` ([Z2-N-ARCH-476], [Z2-N-ARCH-503], [Z2-N-ARCH-727]); A ≠ P → `SCHEDULE_ERROR_AUTHORITY_VIOLATION` ([Z2-N-ARCH-483]). World resources: the owning phase O is the resource type's `m_eResourceOwningPhase` (1 … 11, `Zenith2/Docs/Design/ECS/Reflect.md` → "Freeze validation" rule 4); a resource added with `AddWorldResource` whose `eOwningPhase` differs from O → `SCHEDULE_ERROR_RESOURCE_AUTHORITY` (the message of `Zenith2/Docs/Design/ECS/SystemAPI.md` → "Zenith_System header"); a resource write with P ≠ O → `SCHEDULE_ERROR_RESOURCE_AUTHORITY` ([Z2-N-ARCH-488]), except the sole-consumer epilogue drain of [Z2-N-ARCH-727]. A resource with O = 11 (`PHASE_SAVE_SNAPSHOT`) is a snapshot buffer in the sense of [Z2-N-ARCH-503]: its only writers are phase-11 systems, which is the one write phase 11 admits; no resource owns phase 12.
6. **Writers per (property, phase)**: two or more direct writers without an arbitration declaration covering the property → `SCHEDULE_ERROR_DUPLICATE_WRITER` ([Z2-N-ARCH-484]); for an arbitrated property, a writer with `WRITER_SOURCE_NONE` → `SCHEDULE_ERROR_WRITER_SOURCE_MISSING`, two writers with one source → `SCHEDULE_ERROR_DUPLICATE_WRITER_SOURCE` ([Z2-N-ARCH-494]).
7. **Cross-entity reads**: a `ReadAnyRow` of a property written in the same phase → `SCHEDULE_ERROR_CROSS_ENTITY_READ` ([Z2-N-ARCH-711], [Z2-N-SCRIPT-180]).
8. **Orderings**: a required `Before`/`After` naming no system or stage of the world → `SCHEDULE_ERROR_ORDERING_TARGET_MISSING`; one contradicting the phase order → `SCHEDULE_ERROR_ORDERING_CONTRADICTS_PHASE` ([Z2-N-ARCH-401], [Z2-N-ARCH-404]); `…IfPresent` names that resolve to nothing are dropped.
9. **Declarations without a merge node**: none — any system declaring `EmitsCommands`, `EmitsEvent`, a reduction or `ConsumesMergeCapacity` gets one (§ "Merge nodes and the merge chain"), so no error exists for it.

Diagnostics are sorted by code, then system A's registration index, then system B's, then type ID, then property ID; each is logged at the point of failure under `ecs.schedule` ([Z2-N-ARCH-506]) and kept until the next `Freeze` or `EmitInto`. A startup compile with any error stops the game before its first tick with exit code 3 ([Z2-N-ARCH-498]). The checks run in every configuration including Shipping ([Z2-N-ARCH-499]).

### Messages

The messages of the sixteen specification codes are `Zenith2/Docs/Architecture/FramePhases.md` → "Schedule-compile errors" verbatim, with `<Type>` the type's script name, `<prop>` the property name, `<P>` and `<Q>` phase names from `Zenith_GetPhaseName`, `<S>` system names, `<w>` the world tag in decimal, `<typeId>` 16 lowercase hexadecimal digits, `<module A>` the part of a system name before its first `.`. The three appended codes:

| Code | Message format |
|---|---|
| `SCHEDULE_ERROR_CROSS_ENTITY_READ` | `schedule: system '<S>' (phase <P>) declares a cross-entity read of '<Type>.<prop>', which '<W>' writes in the same phase; read it on the system's own row only` |
| `SCHEDULE_ERROR_SCRIPT_PHASE` | `schedule: chunk system '<S>' declares phase <P>; chunk systems run in EarlyScripts or LateScripts` |
| `SCHEDULE_ERROR_INVALID_NAME` | `schedule: system name '<S>' is invalid: names are 1 to 127 bytes and contain no '@'` |

SCRIPT's names for two of these (`SCRIPT_SCHEDULE_WRITE_OUTSIDE_AUTHORITY` of [Z2-N-SCRIPT-146] and `SCRIPT_SCHEDULE_CROSS_ENTITY_READ_OF_WRITTEN_COLUMN` of [Z2-N-SCRIPT-180]) are `SCHEDULE_ERROR_AUTHORITY_VIOLATION` and `SCHEDULE_ERROR_CROSS_ENTITY_READ`; the registry reports one code per violation whichever kind of system caused it, so a script system and a C++ system produce identical diagnostics ([Z2-N-ARCH-592]).

## EmitInto translation

`Zenith_WorldSimulation::EmitInto(xBuilder)` runs on the main thread in startup step 18 and prologue step 8. It freezes the registry if needed (returning its errors), then declares the world's nodes, resources, accesses, barriers and orderings on the builder (`Zenith2/Docs/Design/GRAPH.md` → "Zenith_FrameGraph.h"). Complexity O(systems + accesses + properties). It obeys the validation rules GRAPH states for it (`Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Step 0 validation", cross-area note G13): no node name contains `@`; one `FRAME_NODE_KIND_TICK_BOUNDARY` node per world, alone at stage 0; arbitration staging resources non-persistent; disabling implemented in ECS's own trampoline.

### Node and resource naming

| Declared | Subgraph, kind, affinity | Name | Stage |
|---|---|---|---|
| the tick boundary | TICK, `FRAME_NODE_KIND_TICK_BOUNDARY`, ANY | `ECS.TickBoundary` | 0 |
| a native or script chunk system | TICK, `FRAME_NODE_KIND_SYSTEM`, the desc's affinity | the system name | its phase |
| a behaviours system | TICK, `FRAME_NODE_KIND_BEHAVIOURS`, MAIN | the system name | 2 or 10 |
| a merge node | TICK, `FRAME_NODE_KIND_STAGE`, ANY | `<system name>.Merge` | the system's phase |
| an arbitration node | TICK, `FRAME_NODE_KIND_ARBITRATION`, ANY | `Arbitrate.<Type>.<prop>` | the writers' phase |
| an epilogue stage or extractor | EPILOGUE, `FRAME_NODE_KIND_STAGE`, ANY | the stage name | 12 |
| an `AddSystem` desc with phase 12, `PHASE_COMMANDS_ONLY` or above 12 | none: rejected by `AddSystem` with `ECS_ERROR_PHASE_NOT_SCHEDULABLE`, never declared | — | — |

The core adds `@W<tag>` to every world node's name ([Z2-N-ARCH-423]; `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Step 6 lowering into ZenithGraph"). Resources:

| Resource | Name | Key (`Zenith_Hash64` over little-endian bytes) | Persistent |
|---|---|---|---|
| a property | `<Type>.<prop>@W<tag>` | tag (2) ‖ type ID (8) ‖ property ID (4) | yes |
| a world resource | `<ResourceScriptName>@W<tag>` | tag (2) ‖ type ID (8) ‖ `0xFFFFFFFF` (4) | yes |
| an arbitration staging slot | `<Type>.<prop>.<Source>@W<tag>` | tag (2) ‖ type ID (8) ‖ property ID (4) ‖ source (1) | no |
| an event queue of one event type, declared for its readers only | `Events.<Event>@W<tag>` | tag (2) ‖ type ID (8) ‖ `0xFFFFFFFE` (4) | yes |

A key collision is `FRAME_GRAPH_ERROR_DUPLICATE_RESOURCE_KEY` at compile, reported as `SCHEDULE_ERROR_GRAPH`. An event queue resource carries reads alone: no node of the TICK subgraph writes it, because an emitted event reaches the queue only at a sync point (§ "Events and reductions"), so any number of systems of one phase may emit one event type (§ "Stages and barriers").

### Stages and barriers

For each world, `SetStageBarrier` is declared for TICK stages 1 … 10 with the sync-point playback body (§ "Sync-point playback") and for stage 11 with the end-of-tick body (§ "Tick boundary and end-of-tick"); the builder generates the fences and the stage orderings ([Z2-N-ARCH-479]); GRAPH spells each fence `Sync.<Subgraph>.G<group>.S<stage>` (`Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Step 3 stage fences"), with the TICK subgraph's name and the world tag as the group, and ECS never spells a fence name itself: it declares barriers, and its diagnostics and zones name the stage (`ECS.SyncPoint.P<p>`). Phase 11 records no commands ([Z2-N-ARCH-504]), so stage 11's barrier asserts its phase committed nothing and runs the end-of-tick playback. The tick-boundary node at stage 0 runs before every phase-1 node of its world ([Z2-N-ARCH-505]).

Access declarations per system node: each read property → `Read(node, property resource)`; each written property → `Write(writer, property resource, bAlsoReads)`, where the writer is the merge node when the system has one and the system node otherwise; arbitrated writes → `Write(system node, its staging resource, false)`; resource reads and writes likewise; `ReadsEvent` → `Read(node, event resource)`; `EmitsEvent` declares no access to the event resource (the emitting system still gets its merge node, § "Merge nodes and the merge chain"), because events are appended to the queue at the sync point that closes the stage and delivered in phase 9, never written by a node, so emitters of one event type in one stage are not writers of one resource; explicit `Before`/`After` → `Before`/`After(node, name, bRequired)`. Version threading within a stage then orders readers after writers ([Z2-N-ARCH-402]).

<!-- elaboration -->
A frame resource has at most one writer per stage ([Z2-N-ARCH-403]; `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Step 5 version threading" rule 1). For a property, the authority and writer checks of § "Check order" (steps 5 and 6) report a second writer first; for a world resource, two systems of its owning phase that both declare `WriteResource` pass the ECS checks and fail the graph compile with `FRAME_GRAPH_ERROR_UNORDERED_WRITERS`, which `RecordGraphFailure` reports as one `SCHEDULE_ERROR_GRAPH` diagnostic naming both systems; a world resource therefore has one writer per stage. Arbitration staging slots and the event queues never reach that rule: each staging slot has its one writer source, and an event queue has no writer.

### Merge nodes and the merge chain

A system gets a merge node `M(S)` when it declares `EmitsCommands`, `EmitsEvent`, a reduction or `ConsumesMergeCapacity`, or is a script system ([Z2-N-ARCH-692]):

1. `M(S)` is ordered `After(S, required)`.
2. `M(S)` is ordered `After(M(S′), required)` for the merge node M(S′) that precedes it in the **stage order** of its world and phase (`Zenith2/Docs/Design/ECS/Queries.md` → "Stage order of merge points" item 7), forming the chain ([Z2-N-SCRIPT-249]). The stage order is computed from every other ordering of the stage (rules 1 and 3, the declared hazards and the explicit orderings) and never from the chain, and every chain edge points forward in it, so the chain agrees with every other ordering of the stage and cannot close a cycle: a system that reads a property and is registered before that property's writer, both with merge nodes, runs after the writer's merge by the read-after-write hazard and merges after it. Registration index stays the only tie-break, so the chain is a function of the declarations, identical on every backend, worker count and run. Capacity is therefore consumed in chain order; playback still applies the accepted buffers in merge-key order (§ "Steps" step 2).
3. `M(S)` is the declared writer of every property S writes, so every dependent of those columns starts after the merge ([Z2-N-SCRIPT-159]); S itself declares no write of them. S's own reads of those properties are pre-reads (S precedes `M(S)` by the explicit ordering of rule 1), and a node that declared `Before(S)` also declares `Before(M(S))`.
4. S's node publishes its invocation record into the world simulation's per-(system, tick instance) slot; `M(S)` reads it (ordered by rule 1).

A system without a merge node commits at its final continuation (§ "Commit and collection").

<!-- assumption: pending core-doc batch 4 (X155) -->
Rule 2 chains merge nodes in the stage order where [Z2-N-ARCH-692] and [Z2-N-SCRIPT-249] chain them in registration (system) order; the orchestrator's ruling (b) on Q49 adopts the stage order, and the core documents take it in the batch that applies X155. Until then a worker follows rule 2 as written here.
<!-- /assumption -->

**No merge-order diagnostic.** Because the chain cannot close a cycle, `Freeze` has no merge-order check and canonical module order never decides whether two systems can share a phase. A cycle is still possible among a stage's own declarations — two systems each reading a property the other's merge commits with no `Before` between them, or contradictory `Before`/`After` — and it is a genuine dependency cycle that the graph compile reports (`GRAPH_ERROR_CYCLE`, returned through `RecordGraphFailure` as `SCHEDULE_ERROR_GRAPH` naming the nodes). No cycle can span stages: every cross-stage ordering follows the phase order (an explicit ordering against it is `SCHEDULE_ERROR_ORDERING_CONTRADICTS_PHASE`, § "Check order" step 8), stage fences order every node of stage p before every node of stage p + 1, and the chain never leaves its stage. So no further diagnostic is needed (`Zenith2/Docs/Design/ECS/Queries.md` → "Stage order of merge points" item 6; `Test_ECS_ChangedQueryReaderRegisteredBeforeWriter`).

### Arbitration nodes

For every arbitrated (property, phase) with at least one writer: one node `Arbitrate.<Type>.<prop>` that reads every staging resource of the property and read-writes the property resource ([Z2-N-ARCH-497]; `Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Frame graph builder", translation step 4); each writer that also reads the property declares `Before(arbitration node)`, so it reads the committed value: `Zenith_ChunkView::GetCommittedColumn` gives it the property column as the previous merge left it, and no writer ever sees a staged value (`Zenith2/Docs/Design/ECS/Queries.md` → "Views and validation" step 4). The arbitration node's body is § "Arbitration merge". Step 4 of sync-point playback checks that no arbitrated property of the phase remains unmerged ([Z2-N-ARCH-479] step 4).

### Epilogue and extractors

Each epilogue stage registered with `AddEpilogueStage` (and each extractor, `m_bRenderExtractor = true`) is one EPILOGUE node at stage 12 of its world ([Z2-N-ARCH-726]); its reads and its sole-consumer resource write are declared on EPILOGUE resources with the same keys (the core gives each subgraph its own resource instance, `Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Step 2 resource keys and groups"). Each extractor declares `Before("Render.Seal@W0", true)` ([Z2-N-ARCH-439]); epilogue stages that are not extractors declare no ordering to the seal. `Render.Seal` is declared by MOD in every configuration, headless included (an assumption recorded for MOD). An extractor's system context has phase 12 and grants reads only ([Z2-N-ARCH-476]).

### Disabled systems

Every system node's `m_pfnRun` is ECS's trampoline `Zenith_ECSRunSystemNode`, with `m_pData` the system's node record:

1. If the world is paused, the system is disabled (the snapshot taken at the tick boundary) or tombstoned, return: the node completes without running the body and without recompiling ([Z2-N-ARCH-410]); its merge node, if any, also returns immediately.
2. Create the invocation record in the node arena: scope `Jobs().GetInvocationID()`, the merge version V(t, m_S) (for a system without a merge point, its read version), the read version V(t, R(S)) (`Zenith2/Docs/Design/ECS/Queries.md` → "Stage order of merge points" item 5), the system's previous read version P as it stands (what `GetPreviousReadVersion` returns in this invocation), the phase, empty buffer and view lists.
3. Construct the `Zenith_SystemContext` on the stack and call `m_pfnRun`.
4. When the system has no merge node, register its final continuation with `Continue`: it commits the written columns when the system wrote any (§ "Commit and collection") and then sets each query slot's (L, O) to (read version, merge version), the merge version counting only when the system has a merge point (O stays 0 otherwise), and the system's previous read version P to the read version.
5. Publish the invocation record for the merge node.

The merge node's trampoline `Zenith_ECSRunMergeNode` builds a `Zenith_MergeContext` over the published record, calls the desc's `m_pfnMerge` or the default merge, sets each of the system's query slots' (L, O) to (read version, merge version) and the system's previous read version P to the read version (`Zenith2/Docs/Design/ECS/Queries.md` → "Change-filtered queries"), and retires the invocation's views.

## Command buffers

### Record format

Every record is the 32-byte header of `Zenith2/Docs/Architecture/ECSModel.md` → "Command buffers" — `{ uint16 m_uKind; uint16 m_uFlags; uint32 m_uPayloadBytes; Zenith_MergeKey m_xKey; }` (8 + 24 bytes) — followed by its payload, 8-byte aligned. Payloads, little-endian, entity handles as their 64 bits (ticket handles included):

| Kind | Payload |
|---|---|
| SPAWN (1) | ticket `uint32`; component count `uint32`; persistent ID (16); parent `uint64`; then per component a component block |
| DESTROY (2) | entity `uint64` |
| ADD_COMPONENT (3) | entity `uint64`; one component block |
| REMOVE_COMPONENT (4) | entity `uint64`; type ID `uint64` |
| SET_PROPERTY (5) | entity `uint64`; type ID `uint64`; property ID `uint32`; value bytes `uint32`; value (value encoding) |
| SET_CONTAINER (6) | entity; type ID; property ID `uint32`; element count `uint32`; element bytes `uint32`; pad; elements |
| SET_PARENT (7) | entity `uint64`; parent `uint64` (0 = root) |
| MODULE (8) | command kind `uint32`; payload bytes `uint32`; payload |
| EVENT (9) | event type ID `uint64`; value count `uint32`; pad; property values |
| CONTRIBUTION (10) | reduction index `uint32`; pad; value (8 bytes, f64 or i64) |

A component block is: type ID `uint64`; value count `uint32`; block bytes `uint32`; then per value: property ID `uint32`, value bytes `uint32`, value, padding to 8. A module payload larger than its kind's `m_uMaxPayloadBytes`, or a kind recorded in a phase outside its mask, is dropped at record time with `ecs.commands_rejected_delta` and a log entry naming the kind and the system (never an assert, because scripts reach module commands through bindings).

### Recording

`Zenith_SystemContext::Commands(ulSimulationID, uOrdinal)` returns the logical invocation's buffer, creating it on first use in the calling context's node arena with base key `{ system order, ulSimulationID, uOrdinal, 0 }` and the cap Z2-B-ECS-004's per-invocation share (the whole per-tick value: the cap bounds one invocation's memory; the per-tick capacity is enforced at merge); repeated calls return the same buffer, so one logical invocation has one buffer and one sequence counter. Each recording call appends a header with `m_uSequence` = the next sequence number and copies the payload. `Reserve(uBytes)` pre-grows the buffer's arena region so subsequent records do not grow the arena ([Z2-N-ARCH-691]). `Events(sim)` and `Contributions(sim)` record into the same buffer, and `Events(sim, uOrdinal)` into the buffer of `Commands(sim, uOrdinal)`, so the events of one logical invocation are accepted or rejected with its commands; every one of these calls also sets the context's current simulation ID (`Zenith2/Docs/Design/ECS/SystemAPI.md` → "The current logical invocation"). A system that did not declare `EmitsCommands` and calls `Commands` gets `ZENITH_ASSERT` and a buffer whose records are discarded.

### Commit and collection

Each context links every buffer and every view with a writable column into its invocation's lock-free lists (an atomic push on the invocation record, `memory_order_acq_rel`; the list is read only after every sub-job has joined, whose completion orders the pushes before the read, [Z2-N-ARCH-395]).

The **default merge** (a C++ system's merge node):

1. Sort the invocation's buffers by base key (merge order; keys are unique per system and tick).
2. For each buffer in order: compute its demand (spawn count, playback record count, event count, container bytes); when the system registry has a merge reject filter (`Zenith2/Docs/Design/ECS/SystemAPI.md` → "Merge reject filter", tests alone) and it returns true, take the rejected branch below with the filter's class and skip `TryAccept`; otherwise `TryAccept` it against the phase's `Zenith_MergeCapacity`; accepted → `AcceptBuffer` (appended to the world's phase commit list); rejected → `RejectBuffer`, `ecs.capacity_rejections_delta` + 1, a log line `ecs: merge rejected <system> entity <simID> ordinal <o>: capacity class <class>` ([Z2-N-SCRIPT-332]). A rejected C++ logical invocation loses its commands, events and contributions; its in-place column writes stay, because C++ systems take no snapshots (rollback is the script transaction's mechanism, [Z2-N-ARCH-709]).
3. `CommitAllWrittenRows`: deduplicate the view list by (binding, column), OR duplicate bit sets, call `CommitRowChanges` once per (chunk, column) ([Z2-N-ARCH-723]).
4. `FoldReductions` (§ "Events and reductions").

A system with writes and no merge node runs steps 3 alone in its final continuation. SCRIPT's merge function performs its own acceptance (`Zenith2/Docs/Scripting.md` → "Merge point algorithm") and calls `AcceptBuffer`, `RejectBuffer` and `CommitRowChanges` with its accepted rows' bits, and `TryAccept` for the ECS capacity classes alongside its own classes.

## Sync-point playback

### Steps

The sync-point body of stage p of world w, single-threaded, performs the steps of `Zenith2/Docs/Architecture/FramePhases.md` → "Sync points and the tick boundary" in order ([Z2-N-ARCH-479]), with the playback scope `0xFFFF'0000'0000'0000 | ((tick × 16 + p) & 0x0000'FFFF'FFFF'FFFF)`:

1. Collect the phase commit list (every accepted buffer of phase p).
2. Sort by base key (a stable sort: the merge chain appends accepted buffers in chain order, the stage order of `Zenith2/Docs/Design/ECS/Queries.md` → "Stage order of merge points", not the registration order the base key starts with, so the sort costs O(n log n) for n buffers and O(n) when the two orders coincide); records play in (buffer key, sequence) order, which is the merge key order ([Z2-N-ARCH-689]).
3. Capacity: every buffer on the list was accepted by its merge; the step verifies it (`ZENITH_ASSERT` on an unaccepted buffer) and is otherwise a no-op ([Z2-N-ARCH-481]).
4. Arbitration check: every arbitrated property written in phase p was merged by its node (a checked no-op).
5. Property commands in merge order, except those that name a ticket handle (they wait for step 6): `SET_PROPERTY` through `Zenith_WorldPlaybackAccess::SetProperty` (a stale or foreign handle dropped with `ecs.commands_stale_delta`, [Z2-N-ARCH-490]; a value of the wrong size dropped with `ecs.commands_rejected_delta`; a numeric value outside a declared range clamped, per the `min`, `max` row of `Zenith2/Docs/Architecture/DataDefinedComponents.md` → "The schema file"); `SET_CONTAINER` allocates from the reserved container bytes, copies the elements through the element hooks and queues the old block for deferred free; each overwrite of a (entity, property) already set in this playback increments `ecs.command_overwrites_delta` (last in merge order wins); then the sync point commits the rows those calls marked at its own merge version ([Z2-N-ARCH-728]) by § "Playback write commit".
6. Structural commands in merge order: `SPAWN` (next simulation ID from the world's counter, lowest free slot, components constructed from defaults then the init values; the ticket's handle recorded in the spawn-result log; a stale parent or unknown type drops the spawn, which consumes no ID, [Z2-N-ARCH-546]); `ADD_COMPONENT` (on a pending-destroy entity dropped with `ecs.commands_on_destroyed_delta`, [Z2-N-ARCH-491]; on an entity that already has the component, its values are set through `SetProperty` exactly as property commands, stamped at once by § "Property writes during structural playback", and `ecs.commands_redundant_delta` + 1); `SET_PROPERTY` and `SET_CONTAINER` records that name a ticket handle, each at its own position in the merge order (its buffer's `SPAWN` of that ticket has a lower sequence number, so the ticket is resolved by then, and an `ADD_COMPONENT` of the same buffer recorded before it has been applied), stamped at once by the same section, a ticket of a dropped or rejected spawn making the record stale (`ecs.commands_stale_delta`); `REMOVE_COMPONENT` (absent: counted no-op); `SET_PARENT`; `DESTROY` (marks pending destroy, cascades to children in depth-first ascending simulation ID, [Z2-N-ARCH-549]; a second destroy of a pending entity is a counted no-op). Structural observers receive `STRUCTURAL_EVENT_SPAWNED` and `STRUCTURAL_EVENT_DESTROY_PENDING` in merge order, their follow-up commands queued for the next sync point ([Z2-N-ARCH-544]).
7. Module and lifecycle commands (`MODULE` records) in merge order, each through its kind's playback function ([Z2-N-ARCH-492], [Z2-N-ARCH-542]), in two passes: first every record whose kind is not play-last, then every record of a play-last kind, each pass in merge order (`Zenith2/Docs/Design/ECS/WorldAPI.md` → "Play-last command kinds"). A playback function may read `GetPlaybackVersion()`, which is the sync point's merge version throughout the step.
8. Events: `EVENT` records appended to the world's event queue in merge order (§ "Events and reductions").
9. Structural epochs: every chunk whose rows playback added, removed or moved, or whose layout changed, gets epoch + 1 ([Z2-N-ARCH-693]); property commands and commits never bump an epoch.
10. <!-- elaboration --> Post-playback: increment the query structure version if an archetype, table or chunk appeared or disappeared; refresh stale queries by rule 1 of `Zenith2/Docs/Design/ECS/Queries.md` → "Matched-chunk cache and invalidation"; reset the phase commit list, the arbitration staging written bits and the invocation slots of phase p; compute phase p+1's starting capacity (§ "Capacity acceptance"); publish the per-phase counters.

<!-- elaboration -->
Two consequences of the step order (property commands in step 5, structural commands in step 6) that merge order alone does not predict:

- **A redundant addition overrides step-5 property commands.** A redundant `ADD_COMPONENT` sets its values in step 6, after every step-5 `SET_PROPERTY` and `SET_CONTAINER` of the playback, so on a property both name the addition's value wins even when the property command has the greater merge key. This is the one exception to "last in merge order wins" (Z2-R-ECS-086); the step-6 write counts in `ecs.command_overwrites_delta` like any other overwrite.
- **A property command cannot reach a component added in the same playback.** A `SET_PROPERTY` or `SET_CONTAINER` naming an entity by handle (not by ticket) plays in step 5, before any `ADD_COMPONENT` of the playback, so on a component that an `ADD_COMPONENT` earlier in merge order adds it finds the entity without the component and is dropped with `ECS_ERROR_NO_COMPONENT` logged under `ecs.playback`, although it follows the addition in merge order. A system that sets values of a component it adds records them in the addition's component block, or, for an entity it spawns, addresses the ticket, which step 6 applies in merge order.

`Test_ECS_RedundantAddStampsVersion` asserts the first and `Test_ECS_AddRemoveSamePlayback` the second, on every backend.

The sync-point zone is `ECS.SyncPoint.P<p>` and its duration is added to `ecs.playback_us_delta` (Z2-B-ECS-002).

### Playback write commit

<!-- elaboration -->
The playback's own pending bits ([Z2-N-ARCH-728]) live in the backend's playback write set (`Zenith2/Docs/Design/ECS/Backends.md` → "Playback writes"); every playback that sets properties — a sync point's step 5, the tick boundary's steps 1 to 3, out-of-tick playback — commits them the same way:

0. At the start of every playback — each sync point and end-of-tick playback, the tick boundary, each out-of-tick playback, whether or not it sets properties — call `SetPlaybackVersion(uVersion)` on the world's playback access with the playback's version (below); at its end, `SetPlaybackVersion(0)`. Every row the playback creates and every property write it makes outside the write set is stamped with it (§ "Property writes during structural playback").
1. Before its first `SetProperty` or `SetContainer`, call `BeginPlaybackWrites(xArena)` on the world's playback access, with the tick arena (sync points and the tick boundary) or the simulation's out-of-tick arena. Every successful `SetProperty`, `SetContainer` or `RewriteContainer` sets the written row's bit in the block of its (backend chunk, column), which the backend allocates from `xArena` on first touch (one bit per row of the chunk, rounded up to 64-bit words, zeroed); a failed call sets nothing.
2. After the last property call, for `i` in 0 … `GetPlaybackWriteCount()` − 1 in order: `w = GetPlaybackWrite(i, xPlaybackScope, uVersion)`, then `w.m_xView.CommitRowChanges(xPlaybackScope, w.m_xType, w.m_xProperty, w.m_pulRowBits)` — one call per written (chunk, column) ([Z2-N-ARCH-723]); every bit is accepted, because playback has no rollback.
3. `EndPlaybackWrites()`; the arena owns the blocks and is released with it.

`uVersion` is the sync point's merge version (for the end-of-tick playback, V(t, M)), the tick boundary's merge version, or `Zenith_WorldSimulation::GetChangeVersion()` out of tick (`Zenith2/Docs/Design/ECS/Queries.md` → "Change versions and merge versions"); `xPlaybackScope` is the playback scope of `Zenith2/Docs/Design/ECS/WorldAPI.md` → "Zenith_ChunkView header" (table "Scope namespaces"). The pairs arrive in ascending (type ID, property ID, backend chunk key), so the commit order is deterministic, although no result depends on it (each pair stamps one version). Complexity O(written rows + pairs). `Test_ECS_PlaybackWritesCommittedOncePerChunkColumn` records, for one sync point, two properties of chunk A (one of them by two commands on two rows) and one property of chunk B, and asserts three commits (one per written chunk column), the written rows' and columns' versions equal to the sync point's merge version and the untouched rows' versions unchanged, on every backend.

### Property writes during structural playback

<!-- elaboration -->
Step 6 writes property values after the step-5 write set was committed and ended: the values of a redundant `ADD_COMPONENT` and every ticket-addressed `SET_PROPERTY` or `SET_CONTAINER`. The write set cannot stay open across step 6, because an add or remove moves a row to another archetype, table or pool position and a recorded (chunk, column, row) bit would then name a different row. These writes are stamped at once instead:

<!-- assumption: pending core-doc batch (X147) -->
[Z2-N-ARCH-728]'s "no call other than `CommitRowChanges` advances a column's change version" and `Zenith2/Docs/Architecture/FramePhases.md` → "Sync points and the tick boundary" step 9's "through `Zenith_ChunkView::CommitRowChanges` alone" admit, in the core-doc batch that applies X147, the playback-version stamps of this section and of `Zenith2/Docs/Design/ECS/Backends.md` → "Playback writes" (rows a playback creates, writes outside the write set, columns a move adds) and the relocation rule of `Zenith2/Docs/Design/ECS/Backends.md` → "Change tables"; until then a worker follows these rules.
<!-- /assumption -->

1. A `SetProperty` or `SetContainer` called outside `BeginPlaybackWrites` … `EndPlaybackWrites` writes the value and then sets the written row's version and its (chunk, column)'s column version to the playback version `SetPlaybackVersion` set, two stores per call through the backend's change table (`Zenith2/Docs/Design/ECS/Backends.md` → "Playback writes", item "Outside the set"); it changes no epoch, query structure version or cache.
2. The stamp is the version the set would have committed: the sync point's merge version in step 6, `GetChangeVersion()` in out-of-tick playback's step 6. A `QUERY_TERM_CHANGED` reader of phase p + 1 therefore sees the write exactly as it sees a step-5 write, and a row version is identical whichever path wrote it.
3. With playback version 0 (outside any playback) such a call is `ZENITH_ASSERT` and stamps nothing; the world simulation never makes it (`Test_PlaybackWrites_NoPlaybackVersion_Asserts`).

`Test_ECS_TicketPropertyCommandStampsVersion` records in one phase-3 buffer a spawn, an `AddComponent` on its ticket and a `SetProperty` of the added component on the ticket, and asserts that the spawned entity holds the set value, that its row version and the column version of that property equal sync point 3's merge version, and that a phase-4 `QUERY_TERM_CHANGED` query of the property matches the chunk; `Test_ECS_RedundantAddStampsVersion` adds a component an entity already has, with a changed value, and asserts the value is set, `ecs.commands_redundant_delta` + 1, the row's and column's versions equal the sync point's merge version, no epoch changed and a phase p + 1 `QUERY_TERM_CHANGED` query matches the chunk, on every backend.

### Capacity acceptance

At each phase start (after the preceding sync point, or the tick boundary for phase 1), the world simulation sets the remaining capacity of each ECS class from committed state alone ([Z2-N-SCRIPT-331] … [Z2-N-SCRIPT-333]):

| Class | Remaining at phase start |
|---|---|
| entity slots | min(free entity slots − spawns accepted this tick, the value of `ecs.maxSpawnsPerTick` (Z2-B-ECS-003) − spawns accepted this tick) |
| command records | `ecs.maxCommandsPerTick` (Z2-B-ECS-004) − records accepted this tick |
| event records | `ecs.maxEventsPerTick` (Z2-B-GAME-003) − events accepted this tick |
| container bytes | min(`ecs.maxContainerBytesPerTick` (Z2-B-SCRIPT-016) − bytes accepted this tick, the container heap's free bytes) |

`TryAccept` is first-fit over all classes at once and subtracts only an accepted demand ([Z2-N-SCRIPT-333]). The per-tick counts reset at tick-boundary step 4. The four configuration keys take their engine defaults from the generated budget table (`Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Generated budget table"); while a row is pending the default is the design default of `Zenith2/Docs/Design/ECS.md` → "Configuration keys".

## Tick boundary and end-of-tick

The tick-boundary node (`ECS.TickBoundary`, stage 0) performs, in order ([Z2-N-ARCH-505]):

0. <!-- elaboration --> Discard any leftover of a cancelled frame instance (phase commit lists, invocation slots, playback write sets, arbitration staging bits; `Zenith2/Docs/Design/ECS.md` → "Cancellation" item 3). Apply a pending `SetPaused`, then the step of an active tick run (`Zenith2/Docs/Design/ECS/WorldAPI.md` → "Run ticks then pause"). If the world is paused, stop: none of steps 1 to 4 runs, nothing publishes, the tick index and the tick base stay (`Zenith2/Docs/Design/ECS/WorldAPI.md` → "Zenith_WorldSimulation header"). Otherwise let t be the tick about to start (the current tick index + 1), advance the tick base to B_t = B_(t−1) + M_(t−1) + 1 (`Zenith2/Docs/Design/ECS/Queries.md` → "Change versions and merge versions" item 2), so every version of tick t, V(t, 0) first, is defined before any step uses it, then `SetPlaybackVersion(V(t, 0))` and `BeginPlaybackWrites` with the tick arena (§ "Playback write commit").
   <!-- assumption: pending core-doc batch (X123) -->
   The paused-world rule (a paused world's tick-boundary node performs none of steps 1 to 4 and publishes nothing; results that complete while it is paused publish in completion-ID order at the first tick boundary after it resumes) is the reading `Zenith2/Docs/Architecture/FramePhases.md` [Z2-N-ARCH-505] and `Zenith2/Docs/Scripting.md` [Z2-N-SCRIPT-335] take in the core-doc batch that applies X123; until then a worker follows this step.
   <!-- /assumption -->
1. `OnTickBoundary(TICK_BOUNDARY_STEP_PUBLISH_ASYNC, t, xAccess)` of every registered handler in registration order (SCRIPT and the runtime publish asynchronous results in completion-ID order and log them), where `xAccess` is the `Zenith_TickBoundaryAccess` the node constructs over the world's playback access: property reads and writes only, no structural member (`Zenith2/Docs/Design/ECS/WorldAPI.md` → "Zenith_ECSObservers header").
2. The same for `TICK_BOUNDARY_STEP_RESTORE_CINEMATICS` (CINE writes the pre-cinematic property values back through `SetProperty`, and container values through `Zenith_TickBoundaryAccess::SetContainer`, which reserves each new block itself, `Zenith2/Docs/Design/ECS/WorldAPI.md` → "Tick-boundary container writes").
3. `TICK_BOUNDARY_STEP_APPLY_DISABLES`: handlers mark their failed modules; then ECS applies pending `SetEnabled` requests and script disables into the disabled snapshot the trampolines read this tick. <!-- elaboration --> Then commit the property writes of steps 1 to 3 by § "Playback write commit" with the stage-0 playback scope of tick t and the tick boundary's merge version V(t, 0) (`Zenith2/Docs/Design/ECS/Queries.md` → "Change versions and merge versions"); no epoch, query structure version or query cache changes, because the tick boundary changes no structure; then `SetPlaybackVersion(0)`. <!-- assumption: pending core-doc batch (X123) --> The commit at V(t, 0) through a property-only access is X123's second item for [Z2-N-ARCH-505]; until it lands a worker follows this step. <!-- /assumption -->
4. Increment the tick index to t (the tick base already moved in step 0); reset the per-tick capacity counts and snapshot totals; move the carried event queue to the front of the delivery queue; set phase 1's starting capacity.

`Test_ECS_TickBoundaryPropertyWriteCommitted` writes a property through `Zenith_TickBoundaryAccess` in step 2 and asserts that a phase-1 `QUERY_TERM_CHANGED` query of that property matches the chunk and that the row's version equals V(t, 0), on every backend; `Test_ECS_PausedWorldPublishesNothing` pauses a world with a queued result and asserts no handler step runs until it resumes, then that the result publishes at the resuming boundary.

The end-of-tick playback (stage 11's barrier body) — after the last phase run in the tick ([Z2-N-ARCH-570], `Zenith2/Docs/Architecture/FramePhases.md` → "End-of-tick playback"):

1. Assert phase 11 committed nothing.
2. For every pending-destroy entity, in ascending simulation ID: `STRUCTURAL_EVENT_DESTROYED` to observers, destroy its components through the lifecycle hooks, free its container blocks, remove its row (`DestroyNow`), release its slot (generation + 1, or retirement at the maximum, [Z2-N-CONTRACT-070]).
3. Run any compaction the backend scheduled; with forced compaction enabled for tests, compact every archetype ([Z2-N-ARCH-686]).
4. `Zenith_ContainerHeap::FlushDeferred`.
5. Destroy the delivered events' payloads through their hooks and clear the delivery queue.
6. Bump epochs; increment the query structure version when an archetype, table or chunk appeared or disappeared; refresh every stale query of the world, whatever its owner's stage — the next readers are phase 1 of the next tick instance or, after the frame's last tick instance, the EPILOGUE stage-12 extractors and epilogue stages (`Zenith2/Docs/Design/ECS/Queries.md` → "Matched-chunk cache and invalidation" rule 1).
7. <!-- elaboration --> When ⌊U(t, M) / 2^29⌋ exceeds the sweep floor F (the tick crossed a sweep boundary of the unwrapped merge counter), run the version wrap sweep and set F to it (`Zenith2/Docs/Design/ECS/Queries.md` → "Version wrap sweep" item 1).
8. Release the tick arena.

## Arbitration merge

For each arbitrated (property P of type T, phase p), writer S with source σ writes its value into its staging column and marks the row (the staging column's pending bits are σ's written bits); a `WeightedBlend` writer writes a weight in [0, 1] into `GetArbitrationWeights` (values outside are clamped). The arbitration node, per chunk of T's internal all-rows query (a `ParallelFor` over chunks; each row's result depends on that row alone, [Z2-N-ARCH-495]):

1. Collect the sources whose written bit is set, in the declaration's order (the default precedence when none is declared, [Z2-N-ARCH-493]).
2. `Precedence`: take the first collected source's value.
3. `WeightedBlend`: accumulate in `double` in the declared order: scalars and vectors `Σ wᵢ vᵢ / Σ wᵢ` per component; quaternions sign-aligned to the first collected source (negate `qᵢ` when `dot(qᵢ, q₀) < 0`), accumulated, then normalised; round each component once to the property's type (round to nearest even); `Σ wᵢ = 0` keeps the previous value.
4. `Custom`: call the C++ merge function with `Zenith_ArbitrationInputs` (sources in declared order, previous value).
5. No source wrote the row: keep the previous value.
6. Write the result into the property column, mark the row changed, clear the written bits; after the chunk loop, commit the property's changed rows at the arbitration node's merge version.

`Test_Arbitration_PrecedenceDefaultOrder`, `Test_Arbitration_NoWriterKeepsValue`, `Test_Arbitration_WeightedBlend` and `Test_Arbitration_IndependentOfWorkerOrder` run on every backend.

## Events and reductions

**Events.** The world keeps two queues of event records `{ type ID, merge key, payload offset }` with payloads (constructed in the event type's layout from the recorded property values, lifetime-carrying fields through hooks) in a per-world event arena: the **delivery queue** of the current tick and the **carried queue** of events emitted in phases 9 and 10. Sync points 1 … 8 append accepted `EVENT` records to the delivery queue in merge order; sync points 9 and 10 append to the carried queue ([Z2-N-ARCH-478]). Readers in phase 9 and after (`ReadEvents(type)`) see the delivery queue's records of that type in merge order, through a per-type index built when the queue is appended to. At tick-boundary step 4 the carried queue becomes the front of the next delivery queue. Delivery order is merge order ([Z2-N-SCRIPT-283]); the event queue's capacity is Z2-B-GAME-003.

**Reductions.** `FoldReductions` lists, per reduction slot, the accepted contributions in merge order (buffers in key order, records in sequence order), folds them with GRAPH's `Zenith_FixedOrderReduce` (`Zenith2/Docs/Design/GRAPH.md` → "Zenith_FixedOrderReduce.h") using the kind's combine operation ([Z2-N-ARCH-704]), and writes the result into the reduction target's property on the target component's one entity, committed at the merge's version. The tree of `Zenith_FixedOrderReduce` (pairwise levels, odd element carried) equals `Zenith2/Docs/Scripting.md` → "Fixed-order reduction tree" (leaf blocks of 1,024, pairwise within and across), because 1,024 is a power of two and pairs never straddle a block boundary. A target type carried by no entity or by more than one drops the result with `ecs.reduction_target_invalid_delta` and a log line naming the reduction.

## Out-of-tick playback

`Zenith_WorldSimulation::ApplyOutOfTick(xBuffer)` — world loading (SCENE), carry-over deserialisation at a world transition, save loading, editor edits — runs on the main thread while no frame of the world runs (`ZENITH_ASSERT(!IsInFrame())`):

1. Play the buffer's records with steps 5, 6 and 7 of § "Sync-point playback", each step over the records in sequence order (no capacity acceptance: loading is bounded by the entity capacity itself, and a full world returns `ECS_ERROR_CAPACITY` for the spawn that did not fit), with the out-of-tick playback scope and `SetPlaybackVersion(GetChangeVersion())`; the step-5 property writes commit by § "Playback write commit" at `GetChangeVersion()`, the step-6 property writes (redundant adds, ticket-addressed records) are stamped at once with it (§ "Property writes during structural playback"), and every row a spawn or an add creates takes it as its row and column versions.
2. `EVENT` records are appended to the carried queue (delivered in the next tick's phase 9); `CONTRIBUTION` records are ignored.
3. Bump epochs, increment the query structure version, refresh every stale query.
4. Return the first error; every other record has been applied.

<!-- elaboration -->
Why `GetChangeVersion()` keeps `QUERY_TERM_CHANGED` correct: out of tick, it is V(t, M_t) of the current tick index t, the highest version tick t can issue, whether or not tick t's end-of-tick playback completed (before the first tick it is 1; `Zenith2/Docs/Design/ECS/Queries.md` → "Change versions and merge versions" item 5), and every query slot's last-seen version is the read version of an invocation of tick t or earlier, which is below it — after a cancelled frame instance too, whose completed systems of the stage after the latest completed sync point hold read versions above that sync point's version but below V(t, M_t); so the next tick's first invocation of every `CHANGED` system sees each out-of-tick write as changed, and no subsequent merge of the next tick can be below it. Two out-of-tick playbacks between the same two ticks stamp one version, which is harmless because no invocation runs between them.

`RestoreSimulationState` sets the tick index and the simulation-ID counter from a save or a replay checkpoint, out of tick; the next spawn continues the restored counter ([Z2-N-SCRIPT-281]). It also moves the tick base to the restored tick and resets the change-tracking state — every stored version re-stamped to V(t, M), every query slot's (L, O) to (0, 0) and every system's previous read version to 0, the sweep floor to ⌊U(t, M) / 2^29⌋ — before the load's `ApplyOutOfTick` runs (`Zenith2/Docs/Design/ECS/Queries.md` → "Change versions and merge versions" item 2; `Test_ECS_RestoreResetsChangeTracking`). A replay resumed from the restored state reproduces the recorded tick hashes' ECS part from the next tick, under the conservative-filter rule of `Zenith2/Docs/Design/ECS/Queries.md` → "Change-filtered queries" (the same item 2, "Resumed replay"; `Test_ECS_RestoreThenContinueMatchesUninterrupted`).

## World registry algorithm

`Zenith_WorldRegistry` holds 4,095 tag records `{ live flag, highest generation reported }` and two counters: a round-robin cursor and the creation counter.

`AcquireTag`:

1. For i from 1 to 4,095: tag = ((cursor + i − 1) mod 4,095) + 1; take the first tag that is not live ([Z2-N-CONTRACT-071]). None free: return `ECS_ERROR_WORLD_TAGS_EXHAUSTED` ([Z2-N-CONTRACT-098]).
2. Initial generation: when the tag has a reported highest generation h > 0, g = h + 1, and g > `uECS_MAX_GENERATION` wraps to 1 ([Z2-N-ARCH-725]); when the tag was never issued, g = 1 + ((creation counter × 65,537) mod 2^27) ([Z2-N-CONTRACT-099]).
3. Mark live, set cursor = tag, increment the creation counter, return `{ tag, g }`.

`ReleaseTag(tag, h)` records h (the world's `Zenith_EntityTable::GetHighestGenerationIssued`) and clears the live flag. The world's creator (the runtime) calls it after destroying the world simulation. The two formulas of steps 2 answer two different core-document rules; the reconciliation item in `Zenith2/Docs/Design/ECS.md` → "Reconciliation items" asks the owners to state this combination.

## World state hash

`Zenith_WorldSimulation::HashTickState(xStrings, xHash)` contributes the ECS part of the per-tick world-state hash `Zenith2/Docs/Scripting.md` → "Determinism test matrix" defines, after the end-of-tick playback, into a `Zenith_Hash64Stream`, all integers little-endian and floats by bit pattern:

1. Tick index; next simulation ID; live entity count.
2. For each live entity in ascending stable simulation ID: simulation ID; persistent ID; parent's simulation ID (0 for a root); destroy-pending flag; component count; for each component in ascending type ID: type ID, then every property in ascending property ID in the field-set encoding (`Zenith2/Docs/Design/ECS/Reflect.md` → "Binary field-set record") except that strings are hashed by text (cross-area note C11), entity references by the target's simulation ID (0 for null or stale) and asset references by persistent ID.
3. The tick's delivered event log in delivery order: type ID, merge key, serialized fields ([Z2-N-ARCH-610]).
4. The tick's played-back command log in playback order: stage, kind, merge key, payload with entity handles replaced by simulation IDs.

SCRIPT appends every behaviour instance's committed checkpoint after this part. Nothing hashed depends on slots, chunk ordinals, rows, addresses or storage order ([Z2-N-ARCH-884]); `Test_ECS_WorldHashLayoutIndependent` hashes one world under both capacities and forced compaction and asserts equality.
