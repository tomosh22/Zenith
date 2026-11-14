# Architecture — Frame phases and authority

This file is part of the Architecture document family. The entry document is `Zenith2/Docs/Architecture.md`; this file holds the full specification of its section "Frame phases and authority". Normative statements carry `[Z2-N-ARCH-NNN]` tags in the range 471–539. The lifecycle commands that publish at these phases' sync points are specified in `Zenith2/Docs/Architecture/LifecycleCommands.md`.

## Settled rules and ownership

Hazard analysis orders two writers but does not decide which one should own an outcome. Each tick therefore runs twelve fixed phases, parallel within a phase by access, with write ownership declared per field (Z2-D-080). This is the contract D-PHASES pre-decided; the D-PHASES refinement ticket (open, Stage 1) confirms it and may tighten the details below without changing the phase list. The contract is owned by the GAME area, with JOBS and ECS as dependencies; the phase enum and authority metadata live in `ZenithReflect`, the checks in `ZenithECS`, the phase nodes in the CPU frame graph (Architecture.md → "CPU frame graph and scheduler").

- [Z2-N-ARCH-471] Every simulation tick of every world runs the phases of the table "The twelve phases" in that order, and no simulation work of a world runs outside a phase, a sync point or the tick boundary.
- [Z2-N-ARCH-472] Nodes inside one phase run in parallel as far as their declared accesses allow; the phase boundary is the only ordering between nodes of different phases other than explicit `Before`/`After` constraints, which cannot contradict it.

## The twelve phases

| # | Phase | `Zenith_Phase` value | Owns | Publishes at its end | Typical nodes | Affinity |
|---|---|---|---|---|---|---|
| 1 | Input | `PHASE_INPUT` | action state, per-player input contexts; module world resources that advance world-clock-like state for the tick | input commands, replay-logged | action mapping, context stack, replay-log read or write; module environment advance (ENV: clock, weather, wind, tile and cell activation); cell streaming (`Streaming.Update`, ordered before `Scene.ActivateAdditive`); additive-scene activation (`Scene.ActivateAdditive`) | any worker |
| 2 | Early scripts | `PHASE_EARLY_SCRIPTS` | gameplay intent: desired velocity, requested actions | command and event buffers, merged deterministically | behaviours declaring Early; chunk systems declaring phase 2 | behaviours: main thread; chunk systems: any worker |
| 3 | Animation | `PHASE_ANIMATION` | poses; root-motion extraction (not application) | root-motion deltas | graph evaluation, pose sampling, IK solve, root-motion extraction | any worker |
| 4 | Movement arbitration | `PHASE_MOVEMENT_ARBITRATION` | the character-controller move: root motion, script intent and constraints combined under the arbitration rules | kinematic targets | controller move computation, arbitration merges, `Animation.ApplyRootMotion` (root motion applied through `SetProperty` to entities without a character controller, `Zenith2/Docs/Design/ANIM/Runtime.md` → "Animation.ApplyRootMotion") | any worker |
| 5 | Physics | `PHASE_PHYSICS` | dynamic-body transforms, contacts, triggers | collision and trigger events | physics step (with its own sub-jobs), controller sweep | any worker |
| 6 | Post-physics | `PHASE_POST_PHYSICS` | ragdoll ↔ animation blending, physical-animation sync | final poses | ragdoll blend, physics-to-transform sync | any worker |
| 7 | AI and navigation | `PHASE_AI_NAVIGATION` | agent decisions, path requests (asynchronous results arrive at tick boundaries) | AI commands | perception, decision graphs, path-request issue | any worker |
| 8 | Transform propagation | `PHASE_TRANSFORM_PROPAGATION` | world transforms from local transforms and hierarchy | transform change bits | hierarchy walk by depth level | any worker |
| 9 | Events | `PHASE_EVENTS` | delivery of the merged event queue | — | C++ event handlers | any worker |
| 10 | Late scripts | `PHASE_LATE_SCRIPTS` | reactions to events, final gameplay state | command buffers, applied at the next sync point | behaviours declaring Late; chunk systems declaring phase 10 | behaviours: main thread; chunk systems: any worker |
| 11 | Save-snapshot point | `PHASE_SAVE_SNAPSHOT` | the consistent world snapshot for saves and replays | — | snapshot serialization over chunks, replay checkpoint | any worker |
| 12 | Render extraction | `PHASE_RENDER_EXTRACTION` | the immutable `Flux_RenderWorld` | the hand-off to rendering (pipelined) | module extractors, `Render.Seal` | any worker |

<!-- informative -->
The order encodes the ownership chain of a character: scripts state intent (2), animation extracts root motion without applying it (3), arbitration decides the move (4), physics resolves it (5), post-physics reconciles ragdoll and animation into final poses (6), and only then are world transforms propagated (8), so events, late reactions, saves and rendering all observe one settled world.
<!-- /informative -->

### The phase enum

Declared in `Engine/ZenithReflect/Zenith_Phase.h` (ZenithReflect, because field authority is reflected metadata):

```cpp
enum Zenith_Phase : uint8
{
	PHASE_NONE                  = 0,     // invalid in any declaration
	PHASE_INPUT                 = 1,
	PHASE_EARLY_SCRIPTS         = 2,
	PHASE_ANIMATION             = 3,
	PHASE_MOVEMENT_ARBITRATION  = 4,
	PHASE_PHYSICS               = 5,
	PHASE_POST_PHYSICS          = 6,
	PHASE_AI_NAVIGATION         = 7,
	PHASE_TRANSFORM_PROPAGATION = 8,
	PHASE_EVENTS                = 9,
	PHASE_LATE_SCRIPTS          = 10,
	PHASE_SAVE_SNAPSHOT         = 11,
	PHASE_RENDER_EXTRACTION     = 12,
	PHASE_COMMANDS_ONLY         = 254,   // authority value: no phase writes directly; every write is a command
};

inline constexpr uint8 uPHASE_COUNT = 12;

constexpr const char* Zenith_GetPhaseName(Zenith_Phase ePhase);   // "Input", "EarlyScripts", …
constexpr Zenith_Phase Zenith_ParsePhaseName(const char* szName);  // PHASE_NONE if unknown
```

The schema and diagnostic spelling of each phase is its `Zenith_GetPhaseName` string: `Input`, `EarlyScripts`, `Animation`, `MovementArbitration`, `Physics`, `PostPhysics`, `AINavigation`, `TransformPropagation`, `Events`, `LateScripts`, `SaveSnapshot`, `RenderExtraction`, and `CommandsOnly` for the authority value.

### Phase-specific rules

- [Z2-N-ARCH-473] Behaviours declare exactly one of `PHASE_EARLY_SCRIPTS` or `PHASE_LATE_SCRIPTS` and run on the world's main Luau VM with main-thread affinity (Zenith2/Docs/Scripting.md → "Behaviours and chunk systems").
- [Z2-N-ARCH-501] Luau chunk systems declare `Early` (phase 2) or `Late` (phase 10) in v1 and run on per-worker VMs; admitting them to further phases is part of the D-PHASES refinement (open, Stage 1).
- [Z2-N-ARCH-502] C++ systems declare any phase from 1 to 10 as their authority requires, and engine stages outside the phase schema (the platform pump, the audio update, the render subgraph) are frame-graph stage nodes, not phase systems (Architecture/CPUFrameGraph.md → "Frame structure").
- [Z2-N-ARCH-474] Phase 1 reads the platform's input events and the per-player input contexts and writes action state; during replay it reads the replay log's input commands for the tick instead of platform events, and in both cases it appends the tick's input commands to the replay log (Architecture.md → "Multiplayer foundations").
- [Z2-N-ARCH-475] Phase 3 extracts root motion into root-motion deltas and never applies it to a transform; application happens in phase 4 under the arbitration rules.
- [Z2-N-ARCH-476] Phases 11 and 12 are read-only for component properties and for every world resource except a phase-11 snapshot buffer ([Z2-N-ARCH-731]): no property may declare authority 11 or 12.
- [Z2-N-ARCH-503] No system of phases 11 and 12 declares a write other than a snapshot buffer of [Z2-N-ARCH-731] (phase 11) or its render-world writer partition (phase 12).
- [Z2-N-ARCH-504] Neither phase 11 nor phase 12 records structural, property or lifecycle commands.
- [Z2-N-ARCH-477] A save requested by a command is serialized in the phase 11 of the tick in which the request is played back, from the state as it stands after phase 10's sync point; the serialized bytes are handed to an asynchronous writer, and the write's completion publishes at a tick boundary (Zenith2/Docs/Scripting.md → "Merge-time capacity and asynchronous results").
- [Z2-N-ARCH-478] Phase 9 delivers the event queue merged at the sync points of phases 1–8 of the same tick; events emitted in phases 9 and 10 are merged at their sync points and delivered in phase 9 of the next tick, and delivery order is merge order.

## Sync points and the tick boundary

A sync point sits between every pair of consecutive phases, and it is where commands are applied. The tick boundary precedes phase 1; the end-of-tick playback follows the last phase run in the tick.

### The tick boundary

A pending world-transition swap is performed in the frame prologue, which precedes the first tick boundary of the frame for every world (Architecture/LifecycleCommands.md → "World transition"). The tick boundary node first applies a pending `Zenith_WorldSimulation::SetPaused`; a paused world then stops and performs none of the steps below. Otherwise, at the tick boundary of world w, in this order:

1. Publish asynchronous results that completed since the previous tick boundary, in completion-ID order, and record their publication tick in the replay log (Zenith2/Docs/Scripting.md → "Merge-time capacity and asynchronous results").
2. Restore the pre-cinematic snapshots of cinematics whose end, interrupt or skip completed in the previous tick, and release those snapshots (Architecture/LifecycleCommands.md → "Cinematic start, interrupt and skip").
3. Apply disables of systems and behaviours that failed in the previous tick (Zenith2/Docs/Scripting.md → "Commit and failure semantics"); then commit the property writes the handlers of steps 1 to 3 made through `Zenith_TickBoundaryAccess` at the tick boundary's merge version V(t, 0) (Architecture/ECSModel.md bullet 729).
4. Increment the world's tick index and reset the per-tick budgets (snapshot memory totals, merge capacities).

- [Z2-N-ARCH-505] The tick boundary node of each world that is not paused performs steps 1 to 4 above in that order, before any node of that world's phase 1 runs.
- [Z2-N-ARCH-507] A paused world's tick boundary performs none of steps 1 to 4: no asynchronous result is published into it, its tick index does not advance, and results that complete while it is paused stay queued and publish in completion-ID order at the first tick boundary after it resumes, so a publication's recorded tick is always a tick that ran.
- [Z2-N-ARCH-508] Tick-boundary handlers receive the property-only `Zenith_TickBoundaryAccess` and no structural member; their property writes change no structural epoch, query structure version or query cache.

### Playback at a sync point

At the sync point after phase p of world w, in this order:

1. Collect every command buffer and event buffer committed by the invocations of phase p.
2. Sort them by the merge key (system order, logical invocation ID, sequence) of Architecture.md → "ECS model".
3. Verify shared capacity: every buffer on the list was accepted, in chain order, by its system's merge node (Architecture/ECSModel.md bullet 692), where a rejected entity was rolled back as an entity-level failure with a diagnostic (Zenith2/Docs/Scripting.md → "Merge-time capacity and asynchronous results"); this step is a checked no-op that asserts on an unaccepted buffer.
4. Run the arbitration merge of every arbitrated property written in phase p that was not already merged by its generated arbitration node (none remain when the schedule inserted them; this step is a checked no-op).
5. Apply property commands (`SetProperty`, container and string replacements) in merge order, then commit the rows they marked with `CommitRowChanges` at the sync point's precomputed merge version, one call per written (chunk, column).
6. Apply structural commands in merge order: spawns (entity handles and stable simulation IDs assigned in merge order), component additions and removals, prefab instantiations; mark destroyed entities `pending destroy`.
7. Dispatch lifecycle commands to their listeners in merge order (Architecture/LifecycleCommands.md).
8. Append merged events to the world's event queue for delivery in phase 9.
9. Bump the structural epoch of every chunk whose membership or layout changed. Change-tracking versions advance through `Zenith_ChunkView::CommitRowChanges` and the playback-version stamps of bullet 728 alone: each invocation's merge point commits the columns it wrote at its precomputed merge version, the property commands of step 5, which marked their rows through `Zenith_WorldPlaybackAccess::SetProperty`, are committed by the sync-point node directly after step 5 with the playback's own invocation scope, and the rows step 6 creates, the property writes it makes outside the write set, the columns a move adds and the destination column of a moved or compacted row are stamped with the sync point's version through `Zenith_WorldPlaybackAccess::SetPlaybackVersion` (Architecture/ECSModel.md → "Views").

### End-of-tick playback

After the last phase run in the tick: remove every entity marked `pending destroy` (its generation increments, so its handles are stale from the next tick), run any chunk compaction the backend scheduled, and release per-tick arenas.

- [Z2-N-ARCH-479] A sync point is a `FRAME_NODE_KIND_SYNC_POINT` node that runs after every node of its phase and before every node of the next phase of the same world, and it performs the playback steps above in the order listed.
- [Z2-N-ARCH-480] Chunk compaction and any other change of chunk layout happen only at sync points and at the end-of-tick playback, never while a phase's nodes run.
- [Z2-N-ARCH-481] A tick's playback is identical for identical inputs regardless of worker count and chunk layout, because every playback step iterates in merge order and never in completion or storage order (Z2-D-019).

## Per-field authority

### Rules

- [Z2-N-ARCH-482] Every component property declares an authority: one phase from 1 to 10, or `PHASE_COMMANDS_ONLY`; a property without an authority (directly or through its component's default) fails registration with `REFLECT_ERROR_MISSING_AUTHORITY`.
- [Z2-N-ARCH-483] A system may declare a direct write only of properties whose authority is the system's own phase; any other write goes through a command (`Zenith_CommandBuffer::SetProperty` or a module command), applied at the next sync point.
- [Z2-N-ARCH-484] Two systems declaring a direct write of the same property in the same phase are a schedule-compile error, unless the property's component declares an arbitration rule covering it.
- [Z2-N-ARCH-485] A property with authority `PHASE_COMMANDS_ONLY` has no direct writer in any phase; every change arrives as a command.
- [Z2-N-ARCH-486] Reads are not restricted by authority: any system of any phase may declare a read of any property, subject to the cross-entity rule for script chunk systems (Zenith2/Docs/Scripting.md → "Transaction scope and the entity boundary").
- [Z2-N-ARCH-487] Read and write sets and authority are checked per property when the schedule compiles, with one granularity; a component-level declaration expands to all its properties before any check.
- [Z2-N-ARCH-488] World resources declare an owning phase at registration the same way, one phase from 1 to 11 (Architecture/ECSModel.md bullet 731), and a direct write of a world resource from another phase is `SCHEDULE_ERROR_RESOURCE_AUTHORITY`.

### Declaration in data schemas

In a `*.schema.json` component schema (an excerpt; the full format is in Architecture.md → "Data-defined components"):

```json
{
	"component": "Stats",
	"stableName": "OpenWorldSlice.Stats",
	"authority": "LateScripts",
	"fields": [
		{ "id": 1, "name": "Stamina", "type": "f32", "default": 100.0 },
		{ "id": 2, "name": "MaxStamina", "type": "f32", "default": 100.0, "authority": "CommandsOnly" },
		{ "id": 3, "name": "RegenPerSecond", "type": "f32", "default": 5.0, "authority": "EarlyScripts" }
	]
}
```

`authority` at component level is the default for every property that does not declare its own; its value is a phase name of § "The phase enum".

### Declaration in C++ registration

In a module's or library's registration function (Architecture.md → "Reflection and runtime type registration"):

```cpp
Zenith_TypeBuilder& xType = xRegistry.BeginComponent<Zenith_CharacterMotor>("Zenith.CharacterMotor", 1);
xType.DefaultAuthority(PHASE_MOVEMENT_ARBITRATION);
xType.Property(Zenith_CharacterMotor::uPROP_DESIRED_VELOCITY, "DesiredVelocity", &Zenith_CharacterMotor::m_xDesiredVelocity)
	.Authority(PHASE_EARLY_SCRIPTS);
xType.Property(Zenith_CharacterMotor::uPROP_KINEMATIC_TARGET, "KinematicTarget", &Zenith_CharacterMotor::m_xKinematicTarget);
xType.Property(Zenith_CharacterMotor::uPROP_GROUNDED, "Grounded", &Zenith_CharacterMotor::m_bGrounded)
	.Authority(PHASE_PHYSICS);
xType.End();
```

## Commands

### Command semantics

Commands are the one way to change state outside the owning phase and the one way to make a structural change (Zenith2/Docs/Contracts.md → "Handle kinds").

| Command | Recorded by | Applied at | Conflict rule |
|---|---|---|---|
| `SetProperty(entity, type, property, value)` | any system or behaviour of any phase 1–10 | the sync point after the issuing phase (step 5) | the last in merge order wins; each overwrite increments `ecs.command_overwrites_delta` |
| `SetContainer(entity, type, property, reserved storage)` | as above | step 5 | as above; the replaced storage is released at the end of the tick |
| `AddComponent`, `RemoveComponent` | as above | step 6 | add then remove in one playback leaves the component absent; remove of an absent component is a counted no-op |
| `Spawn`, `InstantiatePrefab` | as above | step 6 | IDs assigned in merge order |
| `Destroy` | as above | step 6 marks, end-of-tick removes | a second destroy of a pending entity is a counted no-op |
| lifecycle commands | as above | step 7 | per command (Architecture/LifecycleCommands.md) |
| module commands (for example `Physics.AddImpulse`) | as above | step 7, by the module's registered playback function | defined by the module, in merge order |

- [Z2-N-ARCH-489] A command recorded in phase p is applied at the sync point after phase p, before any node of phase p+1 runs; when the property's owning phase runs after p in the same tick, the owning phase may overwrite the commanded value, and that outcome is deterministic.
- [Z2-N-ARCH-490] A command naming a stale or foreign entity handle is dropped at playback with the diagnostic counter `ecs.commands_stale_delta` incremented and a log entry naming the command and issuer; it never asserts, because scripts can produce it.
- [Z2-N-ARCH-491] `AddComponent` on an entity marked `pending destroy` is dropped with the counter `ecs.commands_on_destroyed_delta`; `SetProperty` on such an entity is applied, because the entity stays readable until the end of its tick.
- [Z2-N-ARCH-492] Module command kinds are registered with the type registry (kind ID, payload type, playback function, phase restrictions), and their playback functions run on the sync-point node, single-threaded per world, in merge order, except that the records of a kind registered with `COMMAND_KIND_FLAG_PLAY_LAST` play after every other step-7 record of the playback, in merge order among themselves.

## Arbitration rules

### Writer sources and default precedence

```cpp
enum Zenith_WriterSource : uint8
{
	WRITER_SOURCE_NONE = 0,
	WRITER_SOURCE_SCRIPT,
	WRITER_SOURCE_ANIMATION,
	WRITER_SOURCE_PHYSICS,
	WRITER_SOURCE_RAGDOLL,
	WRITER_SOURCE_CINEMATIC,
	WRITER_SOURCE_PHYSICS_2D,       // = 6: TwoD.Physics2DSyncTransforms (phase 6); schema name "Physics2D"
};

enum Zenith_ArbitrationRule : uint8
{
	ARBITRATION_RULE_PRECEDENCE,      // the highest-precedence source that wrote the row this tick wins
	ARBITRATION_RULE_WEIGHTED_BLEND,  // normalised weighted blend of the sources that wrote the row
	ARBITRATION_RULE_CUSTOM,          // a C++-registered merge function (not available to data schemas)
};

// Default precedence, highest first: cinematic > ragdoll > physics > physics2D > animation > script.
inline constexpr Zenith_WriterSource aeDEFAULT_ARBITRATION_PRECEDENCE[] =
{
	WRITER_SOURCE_CINEMATIC, WRITER_SOURCE_RAGDOLL, WRITER_SOURCE_PHYSICS, WRITER_SOURCE_PHYSICS_2D, WRITER_SOURCE_ANIMATION,
	WRITER_SOURCE_SCRIPT,
};
```

- [Z2-N-ARCH-493] The default precedence is cinematic > ragdoll > physics > physics2D > animation > script; a component's arbitration declaration uses it unless it lists its own order, which names each source at most once.
- [Z2-N-ARCH-509] `WRITER_SOURCE_PHYSICS_2D` (schema name `"Physics2D"`) is the writer source of `TwoD.Physics2DSyncTransforms`, which writes `Zenith.LocalTransform` properties 1 and 2 in phase 6, under either outcome of D-SEL-2DPHYS (open, Stage 2); existing values never move, and the two physics sources never write one entity: TWOD skips, and removes, the 2D body of an entity that carries `Zenith.Physics.RigidBody` (`Zenith2/Docs/Design/TWOD/Simulation.md` → "Physics 2D write-back and sync").
- [Z2-N-ARCH-494] Every system that writes an arbitrated property declares its `m_eWriterSource`; a writer with `WRITER_SOURCE_NONE` is `SCHEDULE_ERROR_WRITER_SOURCE_MISSING`, and two writers of the same property with the same source in the same phase are `SCHEDULE_ERROR_DUPLICATE_WRITER_SOURCE`.

### Declaration

In a schema:

```json
"arbitration": {
	"fields": [4, 5],
	"rule": "Precedence",
	"order": ["Cinematic", "Ragdoll", "Physics", "Animation", "Script"]
}
```

`fields` lists field (property) IDs, or is the string `"*"` for every field; `rule` is `Precedence` or `WeightedBlend`; `order` is optional and defaults to the default precedence. In C++, `xType.Arbitration(axPropertyIDs, ARBITRATION_RULE_PRECEDENCE, axOrder)`, or `xType.ArbitrationCustom(axPropertyIDs, &MergeFunction)` with `using Zenith_PfnArbitrationMerge = void (*)(const Zenith_ArbitrationInputs& xInputs, void* pOut);`.

```cpp
// Engine/ZenithReflect/Zenith_Arbitration.h
struct Zenith_ArbitrationSourceValue
{
	Zenith_WriterSource m_eSource;
	const void* m_pValue;          // the staged value, in the property's reflected value type
	float m_fWeight;               // WeightedBlend; 1.0 otherwise
};

// Contract: IMMUTABLE for the duration of one merge call; sources in the declared order; aggregate-initialised per row.
struct Zenith_ArbitrationInputs
{
	const Zenith_TypeID m_xType = {};
	const Zenith_PropertyID m_xProperty = {};
	const uint64 m_ulSimulationID = 0;             // the row's entity
	const void* const m_pPreviousValue = nullptr;  // the property's value before this phase
	const Zenith_Span<const Zenith_ArbitrationSourceValue> m_axSources = {};   // the sources that wrote the row, declared order
};

using Zenith_PfnArbitrationMerge = void (*)(const Zenith_ArbitrationInputs& xInputs, void* pOut);

// The registered declaration, referenced by Zenith_TypeInfo::m_pxArbitration.
struct Zenith_ArbitrationDecl
{
	Zenith_Span<const Zenith_PropertyID> m_axProperties;
	Zenith_ArbitrationRule m_eRule;
	Zenith_Span<const Zenith_WriterSource> m_axOrder;   // highest precedence first; the default order when empty
	Zenith_PfnArbitrationMerge m_pfnCustom;             // ARBITRATION_RULE_CUSTOM; null otherwise
};
```

### The arbitration merge

The schedule inserts one `FRAME_NODE_KIND_ARBITRATION` node per arbitrated (world, property) written in a phase. Each writer writes its value and, for blends, a weight in [0, 1] into its own staging column, and sets the row's written bit for its source. The arbitration node then, per row, in chunk order (the result of each row depends on that row alone):

1. Collect the sources whose written bit is set.
2. `Precedence`: take the value of the first collected source in the declared order.
3. `WeightedBlend`: fold the collected sources in the declared order: scalars and vectors as `Σ wᵢ vᵢ / Σ wᵢ`; quaternions by sign-aligned weighted accumulation against the first source followed by normalisation; a row whose weights sum to 0 keeps its previous value.
4. `Custom`: call the merge function with the collected sources in the declared order.
5. No source wrote the row: the property keeps its previous value.
6. Write the result into the property and clear the written bits.

- [Z2-N-ARCH-495] Arbitration is itself a deterministic merge step: its result for a row depends only on that row's staged values, weights and the declared order, never on which worker or in which order the writers ran.
- [Z2-N-ARCH-496] `WeightedBlend` is valid only for properties of type `f32`, `f64`, `vec2`, `vec3`, `vec4`, `quat` and `colour`; declaring it on any other type fails registration with `REFLECT_ERROR_ARBITRATION_TYPE`.
- [Z2-N-ARCH-497] Readers of an arbitrated property in the writers' phase read the arbitration node's result, never a staging column; staging columns are private to their writer and to the arbitration node; a writer of the property that also reads it runs before the arbitration node and reads the committed value (the property as the previous merge left it, through `Zenith_ChunkView::GetCommittedColumn`, `Zenith2/Docs/Design/ECS/Queries.md` → "Views and validation"), never its own or another source's staged value.

<!-- elaboration -->
Illustration (the owning designs are the SCENE, PHYS and ANIM design documents): a character's local transform can declare authority `PostPhysics` with the default precedence, written in phase 6 by the physics-to-transform sync (source Physics), the ragdoll blend (Ragdoll) and a cinematic override (Cinematic). While a cinematic is active its value wins; while a ragdoll is active without a cinematic, the ragdoll's value wins; otherwise physics' resolved controller position is used.

## Schedule-compile errors

`Zenith_WorldSimulation::EmitInto` (Architecture/ECSModel.md → "Worlds and backends") checks every declaration of the world's `Zenith_SystemRegistry` before the graph core compiles, and reports every error found in one pass as `Zenith_ScheduleDiagnostic` records held by that registry, with one `Zenith_ScheduleError` record as the result:

```cpp
enum Zenith_ScheduleErrorCode : uint8
{
	SCHEDULE_ERROR_AUTHORITY_VIOLATION = 1,
	SCHEDULE_ERROR_DUPLICATE_WRITER,
	SCHEDULE_ERROR_DUPLICATE_WRITER_SOURCE,
	SCHEDULE_ERROR_WRITER_SOURCE_MISSING,
	SCHEDULE_ERROR_COMMANDS_ONLY_WRITE,
	SCHEDULE_ERROR_RESOURCE_AUTHORITY,
	SCHEDULE_ERROR_READ_ONLY_PHASE_WRITE,
	SCHEDULE_ERROR_BEHAVIOUR_PHASE,
	SCHEDULE_ERROR_PHASE_NONE,
	SCHEDULE_ERROR_UNKNOWN_TYPE,
	SCHEDULE_ERROR_UNKNOWN_PROPERTY,
	SCHEDULE_ERROR_DUPLICATE_SYSTEM_NAME,
	SCHEDULE_ERROR_ORDERING_TARGET_MISSING,
	SCHEDULE_ERROR_ORDERING_CONTRADICTS_PHASE,
	SCHEDULE_ERROR_QUERY_ACCESS_MISMATCH,
	SCHEDULE_ERROR_GRAPH,                    // a ZenithGraph compile error; its text is the diagnostic's
	SCHEDULE_ERROR_CROSS_ENTITY_READ,        // value 17: a cross-entity read of a property written in the same phase
	SCHEDULE_ERROR_SCRIPT_PHASE,             // a chunk system outside EarlyScripts and LateScripts in v1 (bullet 501)
	SCHEDULE_ERROR_INVALID_NAME,             // an empty name, a name containing '@', or a name above 127 bytes
};

struct Zenith_ScheduleDiagnostic
{
	Zenith_ScheduleErrorCode m_eCode;
	uint32 m_uSystemA = uGRAPH_INVALID_INDEX;    // registration index of the first system named
	uint32 m_uSystemB = uGRAPH_INVALID_INDEX;
	Zenith_TypeID m_xType;                         // zero when not applicable
	Zenith_PropertyID m_xProperty;                 // zero when not applicable
	uint16 m_uWorldTag = 0;
	Zenith_StringView m_strText;                   // the formatted message of the table below
};

// The error record of [Z2-N-CONTRACT-480]; the diagnostics are Zenith_SystemRegistry::GetDiagnostics().
struct Zenith_ScheduleError
{
	Zenith_ScheduleErrorCode m_eCode = SCHEDULE_ERROR_GRAPH;   // the code of the first diagnostic
	uint16 m_uWorldTag = 0;
	uint32 m_uDiagnosticCount = 0;                            // at least 1
};
static_assert(sizeof(Zenith_ScheduleError) <= 32);
```

| Code | Message format |
|---|---|
| `SCHEDULE_ERROR_AUTHORITY_VIOLATION` | `schedule: system '<S>' (phase <P>) writes '<Type>.<prop>' owned by phase <Q>; write it through a command (Zenith_CommandBuffer::SetProperty) instead` |
| `SCHEDULE_ERROR_DUPLICATE_WRITER` | `schedule: '<Type>.<prop>' has <n> direct writers in phase <P>: '<S1>', '<S2>'[, …]; declare an arbitration rule on component '<Type>' or remove a writer` |
| `SCHEDULE_ERROR_DUPLICATE_WRITER_SOURCE` | `schedule: '<Type>.<prop>' is written by '<S1>' and '<S2>' with the same writer source <Source> in phase <P>` |
| `SCHEDULE_ERROR_WRITER_SOURCE_MISSING` | `schedule: system '<S>' writes arbitrated '<Type>.<prop>' without declaring a writer source` |
| `SCHEDULE_ERROR_COMMANDS_ONLY_WRITE` | `schedule: system '<S>' declares a direct write of '<Type>.<prop>', whose authority is CommandsOnly` |
| `SCHEDULE_ERROR_RESOURCE_AUTHORITY` | `schedule: system '<S>' (phase <P>) writes resource '<Resource>' owned by phase <Q>` |
| `SCHEDULE_ERROR_READ_ONLY_PHASE_WRITE` | `schedule: system '<S>' in read-only phase <P> declares a write of '<Type>.<prop>'` |
| `SCHEDULE_ERROR_BEHAVIOUR_PHASE` | `schedule: behaviour '<B>' declares phase <P>; behaviours run in EarlyScripts or LateScripts` |
| `SCHEDULE_ERROR_PHASE_NONE` | `schedule: system '<S>' declares no phase` |
| `SCHEDULE_ERROR_UNKNOWN_TYPE` | `schedule: system '<S>' accesses unregistered type 0x<typeId>` |
| `SCHEDULE_ERROR_UNKNOWN_PROPERTY` | `schedule: system '<S>' accesses property <id> not present in '<Type>' version <v>` |
| `SCHEDULE_ERROR_DUPLICATE_SYSTEM_NAME` | `schedule: system name '<S>' registered twice in world <w> (by <module A> and <module B>)` |
| `SCHEDULE_ERROR_ORDERING_TARGET_MISSING` | `schedule: '<S>' declares <Before|After>('<T>') but no system or stage '<T>' exists in world <w>` |
| `SCHEDULE_ERROR_ORDERING_CONTRADICTS_PHASE` | `schedule: '<A>' (phase <P>) declares Before('<B>') but '<B>' runs in preceding phase <Q>` for a `Before`; `schedule: '<A>' (phase <P>) declares After('<B>') but '<B>' runs in subsequent phase <Q>` for an `After` |
| `SCHEDULE_ERROR_QUERY_ACCESS_MISMATCH` | `schedule: system '<S>' query '<Q>' names '<Type>.<prop>' as a <read|write> but the system declares it as a <write|read> or not at all` |
| `SCHEDULE_ERROR_GRAPH` | the ZenithGraph diagnostic text (Architecture.md → "ZenithGraph shared core"), prefixed `schedule: ` |
| `SCHEDULE_ERROR_CROSS_ENTITY_READ` | `schedule: system '<S>' (phase <P>) declares a cross-entity read of '<Type>.<prop>', which '<W>' writes in the same phase; read it on the system's own row only` |
| `SCHEDULE_ERROR_SCRIPT_PHASE` | `schedule: chunk system '<S>' declares phase <P>; chunk systems run in EarlyScripts or LateScripts` |
| `SCHEDULE_ERROR_INVALID_NAME` | `schedule: system name '<S>' is invalid: names are 1 to 127 bytes and contain no '@'` |

- [Z2-N-ARCH-498] Every schedule-compile error is reported with the message format of the table above, all errors of one compile together, sorted by code and then by system registration index; a startup compile that reports any error stops the game before its first tick with exit code 3 and the full list in the log.
- [Z2-N-ARCH-506] Each schedule-compile diagnostic is logged at the point of failure and kept in the world's `Zenith_SystemRegistry`, whose `GetDiagnostics` returns the diagnostics of the latest `Freeze` or `EmitInto` until the next one starts; the `Zenith_ScheduleError` result carries the first diagnostic's code, the world tag and the count, and no text.
- [Z2-N-ARCH-499] The schedule-compile checks run in every configuration, including `Shipping`, because a data-defined schema or a script system can introduce a violation without any C++ change.

## Required tests

The table "Phase and authority tests" lists the tests of this section; they live in `Engine/ZenithECS/Tests/` and run on every ECS backend through `ZenithECS_Conformance` where the table marks them conformance.

- [Z2-N-ARCH-500] Every test in the table "Phase and authority tests" exists under the name given and asserts the case given.

Phase and authority tests:

| Test | Conformance | Asserts |
|---|---|---|
| `Test_Phases_RunInFixedOrder` | yes | recorded node order visits phases 1–12 in order every tick |
| `Test_Phases_ExtractionOncePerFrame` | yes | three catch-up ticks in one frame run phase 12 once |
| `Test_Authority_WriteOutsidePhase_IsError` | no | a phase-2 system writing a phase-5 property fails with `SCHEDULE_ERROR_AUTHORITY_VIOLATION` and the exact message |
| `Test_Authority_TwoWriters_IsError` | no | two phase-5 writers of one property fail with `SCHEDULE_ERROR_DUPLICATE_WRITER` |
| `Test_Authority_TwoWritersWithArbitration_Compiles` | no | the same with an arbitration rule compiles and inserts one arbitration node |
| `Test_Authority_CommandsOnly_DirectWriteIsError` | no | a direct write of a `CommandsOnly` property fails |
| `Test_Authority_WholeComponentExpands` | no | `Write(type)` in the wrong phase names every offending property |
| `Test_Authority_MissingAuthority_FailsRegistration` | no | a property with no authority fails with `REFLECT_ERROR_MISSING_AUTHORITY` |
| `Test_Authority_DataSchemaSameAsCpp` | no | a schema-declared component produces the same errors as its C++ twin |
| `Test_Authority_ReadOnlyPhaseWrite_IsError` | no | a phase-11 write fails with `SCHEDULE_ERROR_READ_ONLY_PHASE_WRITE` |
| `Test_Commands_AppliedAtNextSyncPoint` | yes | a `SetProperty` from phase 2 is visible to phase 3 and not to phase 2 |
| `Test_Commands_LastInMergeOrderWins` | yes | two `SetProperty` commands resolve by merge key, identical with 1, 2, 8 and all workers |
| `Test_Commands_StaleHandleDropped` | yes | a command on a destroyed entity is dropped with `ecs.commands_stale_delta` incremented and no assert |
| `Test_Commands_OwningPhaseOverwrites` | yes | a phase-2 command on a phase-5 property is overwritten by phase 5 deterministically |
| `Test_Arbitration_PrecedenceDefaultOrder` | yes | with all five sources writing, the cinematic value wins; without cinematic, ragdoll; and so on |
| `Test_Arbitration_NoWriterKeepsValue` | yes | a row no source wrote keeps its previous value |
| `Test_Arbitration_WeightedBlend` | yes | a two-source blend with weights 0.25/0.75 matches the analytic result bit for bit |
| `Test_Arbitration_WeightedBlendInvalidType_Fails` | no | blending a `u32` property fails registration |
| `Test_Arbitration_IndependentOfWorkerOrder` | yes | results are identical with 1, 2, 8 and all workers and with forced compaction |
| `Test_Events_DeliveredInPhaseNine` | yes | an event from phase 5 is delivered in phase 9 of the same tick, one from phase 10 in the next tick |
| `Test_SyncPoint_EpochBumpedOnStructuralChange` | yes | a spawn bumps the target chunk's epoch; a property command does not |
| `Test_Schedule_AllErrorsInOnePass` | no | a registry with three distinct errors reports all three, sorted |
| `Test_Schedule_ErrorsCheckedInShipping` | no | the checks run with `Zenith_IsShippingBuild()` true (the ECS tests in the `ShippingTest` configuration, test preset `win-shipping-test`, Zenith2/Docs/Architecture/BuildSystem.md [Z2-N-ARCH-231]) |
