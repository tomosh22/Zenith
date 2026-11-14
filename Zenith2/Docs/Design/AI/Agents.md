# AI design: agents, crowds, perception and blackboards

Section file of `Zenith2/Docs/Design/AI.md`. It specifies the AI module's frame-graph nodes, the agent runtime table and state machine, path following, steering, local avoidance and crowds, the locomotion output, simulation LOD, perception, the blackboard store, the determinism rules every AI node keeps, and the save and restore algorithm. Navigation data and the path service are `Zenith2/Docs/Design/AI/Navigation.md`; volumes `Zenith2/Docs/Design/AI/Volumes.md`; the script surface `Zenith2/Docs/Design/AI/Scripting.md`.

## Scope of this file

<!-- informative -->
The old engine's agent component pointed at a navigation agent object by raw pointer, its perception system was a static registry keyed by entity ID, and its squad code kept formations in C++ (`Zenith/AI/CLAUDE.md`, `Zenith/AI/Perception/CLAUDE.md`, `Zenith/AI/Squad/CLAUDE.md`). Zenith2 keeps agents, perception and memory as components and module world state reached only through declared frame-graph access, keeps no static registry, and leaves squads and tactics to Luau gameplay over blackboards: decisions are Luau behaviours (`Zenith2/Docs/Design/AI/Scripting.md` → "Behaviour tree library"), not a C++ subsystem.
<!-- /informative -->

## Frame-graph placement

Every AI system runs in phase 7, `PHASE_AI_NAVIGATION` (`Zenith2/Docs/Architecture/FramePhases.md` → "The twelve phases"), registered by `AttachWorld` in this order (the order is their registration index and so the merge-key system order, [Z2-N-ARCH-405]):

| # | System | Reads | Writes | Orders | Affinity, cost hint |
|---|---|---|---|---|---|
| 1 | `AI.NavIngest` | `Zenith.AI.NavMeshRef`, `.NavMeshTiles`, `.NavVolumeRef`, `.NavObstacle`, `.OffMeshLink`; `Zenith.LocalTransform` 1, 2 and `Zenith.WorldTransform` (`ReadAnyRow`; the world-pose rule of `Zenith2/Docs/Design/AI/Navigation.md` → "Obstacle modes"); `Zenith.Streaming.RegionBound` 2 (`ReadAnyRow`; an entity without the component counts as in an active region); resource `Zenith.WorldOrigin` | resource `Zenith.AI.World`; `ConsumesMergeCapacity` (its merge function `Zenith_AINavIngestMerge` allocates the navigation load completion IDs and issues the loads, `Zenith2/Docs/Design/AI/Navigation.md` → "Navigation asset requests") | first | any, 20 |
| 2 | `AI.Significance` | `Zenith.AI.LODFocus`; `Zenith.LocalTransform` 1 and `Zenith.WorldTransform` (`ReadAnyRow`: a focus's world pose); `Zenith.AI.Significance` 2; `Zenith.AI.Agent` 11 | `Zenith.AI.Significance` 1, 3, 4 | `After("AI.NavIngest")` | any, 5 |
| 3 | `AI.CrowdGrid` | `Zenith.AI.Agent` 1 … 11, 30 (`ReadAnyRow`); `Zenith.LocalTransform` 1 (`ReadAnyRow`); `Zenith.Physics.CharacterController` 1, 2, 30 (`ReadAnyRow`; also through the second query `WITH Zenith.Physics.CharacterController WITHOUT Zenith.AI.Agent`, "Crowd grid"); `Zenith.AI.Significance` 1 | resource `Zenith.AI.CrowdGrid` | `After("AI.Significance")` | any, 5 |
| 4 | `AI.StimulusGrid` | `Zenith.AI.StimulusSource`; `Zenith.LocalTransform` 1, 2 and `Zenith.WorldTransform` (`ReadAnyRow`) | resource `Zenith.AI.StimulusGrid` | `After("AI.NavIngest")` | any, 5 |
| 5 | `AI.NavRequests` | `Zenith.AI.Agent` 1 … 25 (own rows); `Zenith.LocalTransform` 1, 2 and `Zenith.WorldTransform` (`ReadAnyRow`: a MoveTargetEntity's world pose, "Path corridor" step 4); resources `Zenith.AI.World`, `Zenith.AI.AgentRuntime` (`Before("AI.Locomotion")`: the previous version) | resource `Zenith.AI.PathService`; parks script results | `After("AI.NavIngest")`, `Before("AI.Locomotion")` | any, 40 |
| 6 | `AI.Perception` | `Zenith.AI.PerceptionState` 1 … 15; `Zenith.LocalTransform` 1, 2; `Zenith.AI.Significance` 1, 4; resources `Zenith.AI.StimulusGrid`, `Zenith.AI.NoiseBuffer`, `Zenith.Physics.World` | `Zenith.AI.PerceptionState` 20 … 24, 30; resource `Zenith.AI.PerceptionLedger` (its merge function `Zenith_AIPerceptionMerge` writes the emitted levels of accepted buffers, "Perception events"); `EmitsEvent(Zenith.AI.Stimulus)`, `EmitsEvent(Zenith.AI.TargetForgotten)` | `After("AI.NavRequests")`, `After("AI.StimulusGrid")` | any, 40 |
| 7 | `AI.Locomotion` | `Zenith.AI.Agent` 1 … 30; `Zenith.LocalTransform` 1, 2 and `Zenith.WorldTransform` (`ReadAnyRow`: a MoveTargetEntity's world pose); `Zenith.Physics.CharacterController` 1 … 23, 30, 31, 39 (11 Mode selects the root-motion rule); `Zenith.Streaming.RegionBound` 2; `Zenith.AI.Significance` 1, 4; resources `Zenith.AI.World`, `Zenith.AI.PathService`, `Zenith.AI.CrowdGrid` | `Zenith.AI.Agent` 40 … 48; resource `Zenith.AI.AgentRuntime` (its merge function `Zenith_AILocomotionMerge` writes the last-written and emitted fields of accepted buffers, "Locomotion output"); `EmitsCommands`; `EmitsEvent(Zenith.AI.MoveCompleted)`, `EmitsEvent(Zenith.AI.OffMeshLinkReached)` | `After("AI.NavRequests")`, `After("AI.CrowdGrid")` | any, 60 |

- The first two rows of `Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Declarations at the ECS level"'s example (`AI.Perception` after `AI.NavRequests`, emitting `Zenith_AIStimulusEvent`, writing `Zenith_AIPerceptionState`, reading the physics world) are exactly rows 5 and 6.
- Each world resource has exactly one writer in phase 7 ([Z2-N-ARCH-403], the rule ruling Q58 restated): `Zenith.AI.World` (`AI.NavIngest` and its merge node), `Zenith.AI.CrowdGrid` (`AI.CrowdGrid`), `Zenith.AI.StimulusGrid` (`AI.StimulusGrid`), `Zenith.AI.PathService` (`AI.NavRequests`), `Zenith.AI.AgentRuntime` (`AI.Locomotion` and its merge node), `Zenith.AI.PerceptionLedger` (`AI.Perception` and its merge node); `Zenith.AI.Blackboards` and `Zenith.AI.NoiseBuffer` have none (command playback writes them). A system and its merge node are one writer: the merge node runs after every sub-job of the node has joined and before any reader of the resource, which orders after the merge node by ECS's version threading (the precedent of `Zenith.Animation.Residency`, `Zenith2/Docs/Design/ANIM/Runtime.md` → "Asset bindings").
- **Merge functions.** `AI.Locomotion` and `AI.Perception` register their own `m_pfnMerge`, which performs the default merge's steps itself (`Zenith2/Docs/Design/ECS/Scheduling.md` → "Commit and collection": for each buffer in base-key order `ComputeDemand`, `GetCapacity().TryAccept`, then `AcceptBuffer` or `RejectBuffer`; then `CommitAllWrittenRows` and `FoldReductions`) and, per buffer, applies the outcome to the entity's ledger entry or runtime record: an accepted buffer's proposed values become the record's last-written and last-emitted values, a rejected buffer's are discarded. In-place column writes stay either way (C++ systems take no snapshots, [Z2-N-ARCH-709]), so what a rejection loses is only the commands and events, and those are re-derived from committed state the next tick ("Locomotion output", "Perception events").
- No AI system reads, in any row but its own, a property some phase-7 system writes: neighbours, obstacles, links, foci and move targets are read from `Zenith.LocalTransform` (owned by phase 6) or `Zenith.WorldTransform` (phase 8), the controller's `Velocity` (phase 4) and the agent's `AppliedVelocity` (written only by AI's own command at the sync point), so every `ReadAnyRow` above passes `SCHEDULE_ERROR_CROSS_ENTITY_READ` (`Zenith2/Docs/Design/ECS/Queries.md` → "Own-row and any-row reads").
- `AI.Perception`, `AI.Locomotion` and `AI.NavRequests` fan out with `ParallelFor` over their query's chunks (grain 1 chunk) or over the active request set; `AI.NavIngest` fans out carve rebuilds; the grids are built on one job.
- In a world whose purpose is `WORLD_PURPOSE_EDIT` or `WORLD_PURPOSE_PREVIEW`, read through `Zenith_WorldAttachContext::GetWorldPurpose()` (core-doc item X243 (b); `Zenith2/Docs/Design/AI.md` → "Assumed interfaces"), `AttachWorld` registers `AI.NavIngest` alone (the navmesh exists for the editor's views and tools) and no agent, perception or path node, and registers no save participant (no script request can arise there either: SCRIPT attaches no behaviour instance in such a world, row ED4), so an edit world moves no agent and issues no path request (cross-area row ED6; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Module behaviour in edit and preview worlds"). The world's origin is never read for this: EDIT creates edit and preview worlds with origin `WORLD_ORIGIN_STARTUP`.
- The debug epilogue stage `AI.DebugDraw` (`Zenith_WorldAttachContext::AddEpilogueStage`, every build; it returns at once unless the recorder of `Zenith_ModuleStartContext::GetDebugDraw()`, present in every build, is enabled and `ai.debug.views` is non-zero) reads `Zenith.AI.World`, `Zenith.AI.AgentRuntime`, `Zenith.AI.PathService` and the perception components and draws the views of `Zenith2/Docs/Design/AI.md` → "Debug views".
- Command playback (`AI.Agent`, `AI.Noise`, `AI.Stimulus`, `AI.Blackboard`, `AI.OffMeshLink`, `AI.Perception`) runs at the sync points after phases 1 … 10 where the commands were recorded ([Z2-N-ARCH-544]); its effects are visible to the phases after that sync point.

## Agent runtime table

- One `Zenith.AI.AgentRuntime` resource per world holds a dense array of `Zenith_AIAgentRuntime` records sorted by simulation ID, capacity `ai.agent.maxAgents` (at most `uAI_MAX_AGENTS_PER_WORLD`); lookups are binary searches.
- **Insertion and removal** happen on `AI.Locomotion`'s own job before its `ParallelFor`, in one pass: it walks the agent query's rows, collects the simulation IDs with no record (new) and marks the ones seen, sorts the new IDs, inserts their records in ascending order (a full table refuses the rest in ascending order with `AI_ERROR_AGENT_CAPACITY`, counted `ai.agent.refused`; such an entity stays IDLE), and removes every unmarked record and every record the lifecycle listener marked destroyed. Which agents run never depends on chunk layout, and the sub-jobs only write records that already exist.
- **Record layout** (4,096 bytes, alignment 16), declared field by field with offsets in `Zenith2/Docs/Design/AI/Internals.md` → "Agent runtime record": simulation ID; class, medium, plan state, link state; the status a PAUSE interrupted; flags (bit 0 ragdoll, bit 1 root-motion agent, bit 2 not-root counted, bit 3 invalid counted); stuck strikes; pending completion count; handled and last-emitted `MoveSerial`; corridor and volume corner counts; current polygon; the agent's random counter (A7); target, target polygon and the suspended-since tick; the 4-corner straight-path cache; the link traversal; the stuck window (start position, the root-motion intended displacement, start tick); the next optimise and replan ticks; the continuation state (the continuing flag and the target distance of the last continuation plan, "Path corridor"); the last accepted output (desired velocity, yaw, input flags, applied velocity, root-motion yaw, the valid mask); up to 8 pending completions; this tick's proposed output; then the corridor (256 × 8 bytes at offset 1,024) and the volume corners (64 × 12 bytes at offset 3,072).
- Records are written by `AI.Locomotion` only (the resource's one writer), each sub-job writing the records of the agents in its chunk, so no two sub-jobs write one record; its merge function then commits the accepted buffers' proposed fields ("Locomotion output").

## Agent state machine

States are `Zenith_AIMoveStatus` (property 40 MoveStatus); `StatusTick` records the tick of every change.

| State | Event | Next | Action |
|---|---|---|---|
| IDLE | `MoveSerial` changed with Request = move | PLANNING | clear the continuation state; queue an agent plan, reason NEW_MOVE (`Zenith2/Docs/Design/AI/Navigation.md` → "Request sources") |
| any but IDLE | `MoveSerial` changed with Request = move | PLANNING | the previous move ends: `MoveCompleted { serial: previous, Result STOPPED }`; clear the continuation state; queue a plan, reason NEW_MOVE (it supersedes any queued or active one) |
| any | `MoveSerial` changed with Request = stop | IDLE | cancel the plan; clear the continuation state; `MoveCompleted { STOPPED }` unless the previous serial was already answered; one zero desired-velocity command |
| PLANNING | plan PARTIAL with `uNAV_PATH_RESULT_FLAG_DEAD_LINK` (`Zenith2/Docs/Design/AI/Navigation.md` → "Search state and slicing", Finish row) | PLANNING | re-queue the plan with the same reason from the current position: the route ran through a link unregistered during the search, so the request is re-run over the current graph (`ai.path.restarted_delta` + 1); this row is tested before every other plan-result row |
| PLANNING | plan COMPLETE (or PARTIAL with limit NONE and `ALLOW_PARTIAL`) | MOVING | install the corridor; compute corners; clear the continuation state |
| PLANNING | plan TRUNCATED, or PARTIAL with limit NODE_POOL or TICK_CAP (a **limited plan**: the end may be reachable), whatever `ALLOW_PARTIAL` | MOVING, or FAILED | let d = \|target − corridor end point\|, the end point of "Partial results" (for TRUNCATED the point of the 256th polygon closest to the requested end). When the plan's reason is CONTINUATION (queued by "Path corridor" step 6) and d is not at least 1 m below the recorded continuation distance: FAILED, `MoveCompleted { UNREACHABLE }`. Otherwise install the corridor, set the continuing flag and record d. A plan of any other reason records d **without the test** ("Path corridor", continuation) |
| PLANNING | plan NO_START, NO_END, PARTIAL with limit NONE without `ALLOW_PARTIAL`, INVALID_REQUEST | FAILED | `MoveCompleted { UNREACHABLE }` (INVALID_REQUEST: `FAILED`) |
| PLANNING, MOVING | plan NO_NAVMESH, or the class unresolved | SUSPENDED | keep the request; retry every 30 ticks; after 600 ticks `MoveCompleted { FAILED }` and FAILED |
| MOVING, continuing flag clear | within AcceptRadius of the end on the last corner | ARRIVED | `MoveCompleted { ARRIVED }` (PARTIAL path: `PARTIAL_ARRIVED`); zero desired-velocity command |
| MOVING, continuing flag set | within AcceptRadius of the move's target | ARRIVED | `MoveCompleted { ARRIVED }`; zero desired-velocity command; clear the continuation state |
| MOVING, continuing flag set | within AcceptRadius of the corridor's end on the last corner, the target farther | MOVING | desired velocity zero and hold until the continuation's result installs or fails (its replan was queued at 8 m, "Path corridor" step 6); never ARRIVED or `PARTIAL_ARRIVED` at a continuing corridor's end |
| MOVING | corridor invalid ("Path corridor") | PLANNING | queue a plan from the current position, reason REPLAN; keep steering to the last valid corner meanwhile |
| MOVING | next corner is OFFMESH_START within reach | TRAVERSING_LINK or WAITING_LINK | `Zenith2/Docs/Design/AI/Navigation.md` → "Link traversal state machine" |
| TRAVERSING_LINK, WAITING_LINK | link done | MOVING | advance the corridor past the link node |
| MOVING | stuck twice ("Stuck detection") | FAILED | `MoveCompleted { FAILED }` |
| any moving state | `InActiveRegion` false | SUSPENDED | desired velocity zero; plans paused (queued plans stay queued, active ones are cancelled) |
| SUSPENDED | `InActiveRegion` true and the class resolved | PLANNING | replan from the current position, reason REPLAN |
| any moving state | Request = pause | PAUSED | the record's `m_ePausedFrom` = the current status; zero desired velocity; nothing else changes |
| PAUSED | Request = resume | `m_ePausedFrom` | `m_ePausedFrom` = 0 |
| any | `LIFECYCLE_EVENT_TELEPORTED` | PLANNING (if a move is active) or unchanged | drop the corridor and corners; clear the continuation state; the stuck window restarts; a plan, reason REPLAN, when a move is active |
| any | `Medium` changed | PLANNING (if a move is active) | drop the corridor; clear the continuation state; plan in the new structure, reason REPLAN (`Zenith2/Docs/Design/AI/Volumes.md` → "Medium transitions") |
| any moving state | `LIFECYCLE_EVENT_RAGDOLL_ENTER` | SUSPENDED (ragdoll flag set) | AI records no output while the flag is set; the controller is PHYS's during the ragdoll (`Zenith2/Docs/Architecture/LifecycleCommands.md` → "Ragdoll enter and recover") |
| SUSPENDED with the ragdoll flag | `LIFECYCLE_EVENT_RAGDOLL_RECOVER` | PLANNING (if a move is active), else IDLE | clear the flag; drop the corridor; clear the continuation state; replan from the recovered position, reason REPLAN |
| any | entity destroyed | — | cancel the plan; remove the record; no event |
| ARRIVED, FAILED | entity target moved more than `ai.agent.retargetDistance` | unchanged | (a finished move does not re-follow; a new MoveTo does) |

- **Continuation state and plan reasons.** The continuation state is the record's continuing flag `m_bContinuing` and distance `m_fContinuationDistance` (`Zenith2/Docs/Design/AI/Internals.md` → "Agent runtime record"); clearing it sets both to 0. It is cleared on every `MoveSerial` change (a new MOVE_TO or a STOP), on `LIFECYCLE_EVENT_TELEPORTED`, on a `Medium` change and on ragdoll recovery, and when a COMPLETE plan installs or the move ARRIVES. So a second long move never compares its first limited plan with the previous move's distance. Every queued plan carries its **reason** (`m_ePlanReason`): NEW_MOVE (a MOVE_TO), CONTINUATION ("Path corridor" step 6), RETARGET ("Path corridor" step 4) or REPLAN (corridor invalid, a first stuck strike, the replan timer, the end of a suspension, a teleport, a medium change, a ragdoll recovery). Only a CONTINUATION plan is held to the 1 m rule; a plan of any other reason re-records the distance. A RETARGET re-records it against the new target, so a chase across an open world never fails because the target gained ground.
- **A continuation replan runs while MOVING.** The step-6 replan is queued while the agent keeps following its corridor, so MoveStatus stays MOVING while it is queued or active. Its result is applied by the plan-result rows above (written for PLANNING) with the same install semantics, from MOVING: an install replaces the corridor in place, and a failure goes from MOVING to FAILED.
- Every serial receives exactly one `MoveCompleted`, except a serial superseded before its plan was queued (then the STOPPED event of the next MOVE_TO answers it); `CompletedSerial` records the last answered serial, so scripts can match events to requests (`Test_AIAgent_OneCompletionPerSerial`).
- **Completions are re-derivable.** An answer ("`MoveCompleted { … }`" in the table) writes `CompletedSerial` (property 47, in place) and appends `{ serial, result }` to the record's pending completions (at most 8; an agent answers at most 2 serials per tick, a STOPPED and the new move's own failure). Every tick, each pending completion is emitted as an event in the agent's buffer, in serial order, and the count emitted is proposed; the merge function removes the emitted entries from the front only when the buffer was accepted, and sets `m_uEmittedCompletedSerial`. A rejected buffer therefore re-emits the same completions next tick (`ai.agent.reemitted_delta`), so a capacity rejection never loses a completion. A ninth pending completion (four consecutive rejected ticks with two answers each) drops the oldest, counted `ai.agent.completion_dropped_delta` and logged once per agent: the one case where Z2-R-AI-082 is not met, reachable only through a sustained merge-capacity breach that `ai.locomotion.rejected_delta` reports (`Test_AIAgent_RejectedCompletionReemitted`).
- An agent whose configuration is invalid stays IDLE and answers any MOVE_TO with `FAILED`.

### Stuck detection

Every window of `ceil(ai.agent.stuckSeconds × tickHz)` ticks spent MOVING with a desired speed above 0.1 m/s, compare the position with the window's start: a displacement below 0.25 m is a strike. The first strike queues a replan from the current position (reason REPLAN); a second consecutive strike is FAILED. A window that saw a link traversal, a PAUSE or a teleport is discarded.

A **root-motion agent** ("Locomotion output") moves only as fast as its animation delivers, which may legitimately be slower than AI asks (a turn on the spot, a start transition), so its window accumulates the intended displacement `d_intended = Σ |v_chosen| × Δt` over the window's ticks (record field `m_fStuckIntended`) and is a strike only when the achieved displacement is below 0.25 m **and** `d_intended ≥ 1.0 m`; a window whose intended displacement stays below 1.0 m restarts without a strike.

## Move commands

`AI.Agent` playback (payload `Zenith2/Docs/Design/AI/Formats.md` → "Command payloads"):

| Op | Effect on `Zenith.AI.Agent` |
|---|---|
| `MOVE_TO` | MoveTarget, MoveTargetEntity, AcceptRadius, SpeedScale (when non-zero) set; Request = move; MoveSerial + 1 |
| `STOP` | Request = stop; MoveSerial + 1 |
| `PAUSE`, `RESUME` | Request = pause or resume (no serial change) |
| `SET_SPEED_SCALE` | SpeedScale, clamped to [0, 4] |
| `SET_MEDIUM` | Medium (a value outside `Zenith_NavMedium` is refused, counted) |

Several commands for one agent in one sync point apply in merge order, so the last MOVE_TO wins and each superseded serial is answered STOPPED in the next phase 7 (`Test_AIAgent_SupersededMoveAnswered`). A command for an entity without `Zenith.AI.Agent` is counted (`ai.command.no_agent_delta`) and ignored.

## Path corridor

Each tick, for a MOVING ground agent, `AI.Locomotion`:

1. Locates the agent: position p from `Zenith.LocalTransform` 1. An empty corridor or a stale first polygon → `Zenith_NavFindNearestPoint` with extents (2r, h, 2r); none → SUSPENDED.
2. Moves the corridor start: `Zenith_NavMoveAlongSurface` from the corridor's first polygon toward p (≤ 16 polygons); the visited list is merged into the corridor by finding the furthest corridor polygon that also appears in the visited list (searching the visited list from its end) and replacing the corridor's prefix up to it by the reversed visited path to it. When p is not reached (blocked), the corridor is kept and a replan is queued.
3. Validates the first 8 corridor entries (a polygon: stale reference, BLOCKED or excluded flag, carved tile whose version changed; a link node: stale link-node reference, link disabled or unregistered, which includes a dead link node and its restored form `MakeLinkNode(0, 0)`, `Zenith2/Docs/Design/AI/Navigation.md` → "Search graph"): any failure marks the corridor invalid from that entry. Entries deeper than 8 are not validated until they move into the first 8, live and after a restore alike, because a capture writes a deeper entry whose link died as the dead entry and the restore rebuilds it dead (`Zenith2/Docs/Design/AI/Formats.md` → "Save section layout").
4. Follows a moving target: for MoveTargetEntity, the target's world pose position (`Zenith2/Docs/Design/AI/Navigation.md` → "Obstacle modes": its `Zenith.LocalTransform` for a root, its previous-tick `Zenith.WorldTransform` for a child) more than `ai.agent.retargetDistance` from the planned end queues a replan, reason RETARGET, whose result re-records the continuation distance against the new target without the 1 m test; a smaller move inside the corridor's last polygon just moves the end.
5. Optimises every `ai.agent.optimizeIntervalTicks` (scaled by the LOD interval): `Zenith_NavRaycast` from p toward the second straight-path corner, at most 30 × r metres; when it reaches it, the corridor prefix up to the raycast's last polygon is replaced by the raycast's visited list.
6. **Continuation.** With the continuing flag set (the corridor came from a TRUNCATED plan or a PARTIAL plan limited by the node pool or the tick cap, "Agent state machine"), when RemainingDistance to the corridor's end is at most 8 m and the agent has no queued or active plan, a **continuation replan** (reason CONTINUATION) is queued from the current position toward the move's target. The agent keeps following the current corridor and MoveStatus stays MOVING. On reaching the corridor's end before the result, it holds with desired velocity zero, never arriving there ("Agent state machine"). Only this replan is held to the 1 m rule: it must end at least 1 m nearer the target than the recorded distance, or the move FAILS `UNREACHABLE`. Corridor-invalid, stuck, timer, retarget and suspension replans of a continuing move re-record the distance without the test (`Test_AIAgent_ContinuationOneMetreRuleScope`). So between two replans of another reason a move continues at most ⌈d⌉ times, d the distance then recorded, in metres. An agent with or without `ALLOW_PARTIAL` crosses open terrain whatever the node pool. A target behind a region of increasing distance larger than the node pool fails `UNREACHABLE`, and so does a target that is not reachable (`Test_AIAgent_LongMoveContinues`, `Test_AIAgent_ContinuationStateCleared`).
7. Recomputes the corner cache: `Zenith_NavFindStraightPath` from p to the end over the corridor into 4 corners; RemainingDistance is the length through those corners plus the straight distance from the last of them to the end (an estimate, exact when the corridor needs at most 4 corners).

A volume agent's path is its corner list (Volumes.md); it is advanced by dropping corners within `max(r, 0.25 m)` and re-planned when `HasLineOfSight` to the next corner fails (validated every 8 ticks).

## Steering

- The steering target is the first corner farther than 0.01 m; when the agent is within r of a non-final corner it aims at the next one.
- Desired speed: s = MaxSpeed × SpeedScale; when the target is the END corner or an OFFMESH_START corner, s = min(s, √(2 × MaxAcceleration × 0.9 × d)) with d the distance to it, so the agent decelerates to arrive.
- Preferred velocity: v_pref = s × normalise(target − p), with y = 0 for ground agents (their vertical motion is the controller's or the navmesh surface's).
- A PAUSED, IDLE, ARRIVED, FAILED or SUSPENDED agent has v_pref = 0 and keeps taking part in avoidance as a stationary neighbour.

## Avoidance

Local avoidance is optimal reciprocal collision avoidance (ORCA; van den Berg, Guy, Lin, Manocha, "Reciprocal n-Body Collision Avoidance", 2011, as implemented by RVO2), AI's own code in `ZenithAI/Internal/Agents/Zenith_AIOrca.cpp`.

### Neighbour selection

1. Query the crowd grid ("Crowd grid") for agents within `ai.avoid.neighbourRadius` horizontally, of the same medium, whose vertical extents overlap (|Δy| < (h_i + h_j) / 2), excluding self.
2. Sort by (squared distance, simulation ID); keep the first `maxNeighbours(tier)`: `ai.avoid.maxNeighbours` at tier 0, 4 at tier 1, 2 at tier 2, 0 at tier 3 (avoidance off).
3. Navmesh boundary segments: from the corridor's first 4 polygons, every edge with no passable neighbour within r + s × 1.0 s, sorted by distance then (tile key, polygon, edge), the first `uAI_AVOID_MAX_SEGMENTS` (16).

### ORCA constraints

For each neighbour j with relative position p = p_j − p_i, relative velocity v = v_i − v_j (v_i the agent's own velocity of the previous tick: the controller's `Velocity`, or `AppliedVelocity`; v_j from the grid), combined radius R = r_i + r_j and horizon τ = `AvoidanceHorizon`:

- No collision (|p| > R): w = v − p / τ; if w points into the cut-off circle (dot(w, p) < 0 and dot(w, p)² > R² |w|²) the half-plane normal n = normalise(w) and u = (R / τ − |w|) n; otherwise project on the cone's leg (left or right by the sign of det(p, w)) with leg length √(|p|² − R²), u = (dot(v, d) d) − v for the leg direction d, n the leg's outward normal.
- Collision (|p| ≤ R): w = v − p / Δt with Δt the tick duration; n = normalise(w), u = (R / Δt − |w|) n.
- Responsibility: agent i takes the share σ_i = (π_j + 1) / (π_i + π_j + 2) of u, with π the `AvoidancePriority` values; a neighbour without `AI_AGENT_FLAG_AVOIDANCE` (it does not avoid back) gives σ_i = 1. The half-plane is { v' : dot(v' − (v_i + σ_i u), n) ≥ 0 }.
- Each boundary segment gives a static half-plane with full responsibility and horizon 1.0 s (RVO2's obstacle lines).

### Linear program

1. Order the half-planes: segments first (in their sorted order), then neighbours (in their sorted order).
2. Two-dimensional incremental linear program: start from v_pref clamped to |v| ≤ s; for each half-plane in order, if the current v violates it, solve the one-dimensional program on its boundary line within the speed circle and all previous half-planes (RVO2 `linearProgram1`/`linearProgram2`, deterministic order, no randomisation).
3. If infeasible at half-plane k, fall back to minimising the maximum violation over neighbour half-planes k … with the segment half-planes kept (RVO2 `linearProgram3`).
4. Clamp the change from the previous velocity to MaxAcceleration × Δt.

Complexity O((neighbours + segments)²) per agent in the worst case; at 8 + 16 constraints, at most 576 line tests.

### Three-dimensional avoidance

FLYING and UNDERWATER agents use the three-dimensional form (RVO2-3D): spheres, half-spaces with plane normals, `linearProgram3` over planes within the speed sphere and `linearProgram4` as the infeasible fallback; no boundary segments (the volume path keeps clearance). Neighbour selection uses 3D distance and the medium.

## Locomotion output

After steering and avoidance an agent has its chosen velocity v and, with `AI_AGENT_FLAG_FACE_MOVEMENT` and |v_xz| > 0.05 m/s, a yaw ψ = `(float)Zenith_Atan2((double)v_x, (double)v_z)` (CORE's deterministic function, ruling X314-FMA3; yaw 0 faces +Z, positive turns +Z toward +X, `Zenith2/Docs/Design/CORE/Maths.md` → "Coordinate systems") limited to MaxTurnRate × Δt from the current yaw of its rotation, the current yaw itself read from the rotation quaternion by the same `Zenith_Atan2` of its forward vector's x and z. No AI simulation path calls a CRT transcendental: every sine, cosine, arctangent and exponential below is CORE's `Zenith_Sin`, `Zenith_Cos`, `Zenith_Atan2` or `Zenith_Exp` (`Zenith2/Docs/Design/AI.md` → "Assumed interfaces", CORE row). Then, in the agent's entity transaction (`Zenith_SystemContext::Commands(simulationID)`), recorded only when the value differs from the record's last written value:

| Agent has | Commands recorded | Applied |
|---|---|---|
| `Zenith.Physics.CharacterController` in `CHARACTER_CONTROLLER_MODE_VELOCITY`, not `KINEMATIC` | `SetProperty` CharacterController 20 DesiredVelocity = v (y = 0); 22 DesiredYawRadians = ψ; 23 InputFlags = current \| `CHARACTER_FLAG_USE_DESIRED_YAW` (and \| `CHARACTER_FLAG_SUSPENDED` during a WALK or JUMP link) | at the sync point after phase 7; the controller moves the character in phase 4 of the next tick (`Zenith2/Docs/Design/PHYS/CharacterRagdollVehicle.md` → "Character move algorithm") |
| a controller in `CHARACTER_CONTROLLER_MODE_ROOT_MOTION_WITH_GRAVITY` or `CHARACTER_CONTROLLER_MODE_ROOT_MOTION` (a **root-motion agent**), not `KINEMATIC` | no controller input: in those modes PHYS takes horizontal motion and yaw from ANIM's root-motion delta (ruling Q72; "Character move algorithm" step 3) and ignores DesiredVelocity and DesiredYawRadians. AI writes its intent to its own state in place every moving tick: `Zenith.AI.Agent` 43 DesiredVelocity = v (y = 0), 44 NextCorner, 48 DesiredYaw = ψ; the game's animation graph turns them into root motion (`Zenith2/Docs/Design/AI/Scripting.md` → "Authoring conventions"). During a WALK or JUMP link AI records InputFlags \| `CHARACTER_FLAG_SUSPENDED` and moves the transform as for a kinematic agent | the state at the end of phase 7; a Late behaviour turns it into animation parameters, applied at the sync point after phase 10; ANIM evaluates them in phase 3 and PHYS applies the delta in phase 4 of the next tick (the same one-tick latency as velocity agents) |
| no controller, or `KINEMATIC`, or a volume medium | `SetProperty` `Zenith.LocalTransform` 1 Position = p + v Δt (ground: y snapped to the polygon height under it); 2 Rotation = the yaw quaternion (0, `(float)Zenith_Sin(ψ / 2)`, 0, `(float)Zenith_Cos(ψ / 2)`) (ground), or yaw and pitch from v (pitch `Zenith_Atan2(v_y, \|v_xz\|)` limited to ±80°, composed yaw then pitch from the half-angle sines and cosines likewise) (volume) | at the sync point after phase 7; phase 8 propagates it in the same tick |
| every moving agent | `SetProperty` `Zenith.AI.Agent` 30 AppliedVelocity = v | at the same sync point; the crowd grid of the next tick reads it |

- Every agent writes Agent 43, 44 and 48 in place each moving tick, whatever its output path; only a root-motion agent's movement depends on them. The corridor of a root-motion agent tracks the position the animation achieved (step 1 of "Path corridor" reads `Zenith.LocalTransform` every tick), avoidance uses the controller's achieved `Velocity` as v_i, and arrival writes 43 = 0 (`Test_AIAgent_RootMotionControllerFollowsCorridor`).
- On the tick an agent leaves a moving state, AI records one command setting DesiredVelocity to zero (velocity-mode controller agents) and clears `USE_DESIRED_YAW`, or writes 43 = 0 (root-motion agents); afterwards AI writes nothing to the controller until the agent moves again, so the field returns to scripts.
- An agent that is not a hierarchy root is never moved (`AI_ERROR_NOT_ROOT_ENTITY`, counted once per entity in `ai.agent.not_root`), because a local position write would not be a world position.
- **Rejected buffers.** The sub-job records a command only when its value differs from the record's last **accepted** value, and writes the values it recorded into the record's proposed fields; `Zenith_AILocomotionMerge` copies the proposed fields into the last-written fields only for an accepted buffer. A rejected buffer (capacity, BR-MERGE) loses that tick's output for the agent deterministically, and the next tick re-sends it because the last accepted value still differs (`ai.locomotion.rejected_delta`, `Test_AIAgent_RejectedBufferResends`); the buffer's completions are re-emitted the same way ("Agent state machine").
- **Tier 3** ("Simulation LOD"): on a tick that is not the agent's update tick, a controller agent's held DesiredVelocity keeps it moving; a kinematic or volume agent records the position command `p + v_held Δt` every tick (ground: y from `GetPolyHeight` on the record's current polygon, which is re-located only on update ticks), so it does not stop for 7 ticks in 8 (`Test_AIAgent_KinematicTierThreeKeepsMoving`). Corridor, steering and avoidance run only on update ticks.
- The one-tick latency (decided in phase 7 of tick t, moved in phase 4 of tick t + 1) is the ownership chain of the phases: intent before movement, AI after physics.

## Crowd grid

- `AI.CrowdGrid` gathers every agent into an array of entries (simulation ID, position, velocity, radius, height, priority, medium, flags; 48 bytes), sorts them by (cell key, simulation ID) with `Zenith_Sort` (keys are unique, so the unstable sort's output is unique), and indexes each non-empty cell's range in a `Zenith_HashMap` keyed by the packed cell key (cell edge = `ai.avoid.neighbourRadius`); lookups visit the 3 × 3 cells around the query in a fixed order and never iterate the map.
- Velocity is the controller's `Velocity` for controller agents and `AppliedVelocity` otherwise; position is `Zenith.LocalTransform` 1.
- **Non-agent characters.** A second query `WITH Zenith.Physics.CharacterController, Zenith.LocalTransform WITHOUT Zenith.AI.Agent` (the player's controller, scripted pawns) adds one entry per row: velocity the controller's `Velocity` (30), radius its `Radius` (1), height its `Height` (2), medium GROUND, priority 255, and the flag NON_AVOIDING, so every agent treats it as a neighbour that does not avoid back (σ_i = 1, "ORCA constraints") and steers around the player instead of into it (`Test_AIOrca_AvoidsPlayerController`). Such entries are never agents themselves: they get no record and no output. Both queries' rows merge into the one sorted array, so the grid stays a function of committed state.
- O(n log n) per tick; at 1,024 agents under 0.1 ms on the reference desktop by construction (a sort of 1,024 48-byte entries).

## Simulation LOD

| Tier | Default boundary (`ai.lod.tierDistances`) | Interval (`ai.lod.tierIntervals`) | Perception | Avoidance neighbours | Path optimisation | Locomotion |
|---|---|---|---|---|---|---|
| 0 | d < 30 m | 1 | every tick | `ai.avoid.maxNeighbours` | every `optimizeIntervalTicks` | every tick |
| 1 | 30 ≤ d < 60 m | 2 | every 2 ticks | 4 | × 2 | every tick |
| 2 | 60 ≤ d < 120 m | 4 | every 4 ticks | 2 | × 4 | every tick; corners recomputed every 2 ticks |
| 3 | d ≥ 120 m | 8 | every 8 ticks | 0 (off) | × 8 | full update every 8 ticks; the held velocity is applied every tick in between ("Locomotion output", tier 3) |

1. `AI.Significance` collects the foci: entities with `Zenith.AI.LODFocus`, ascending simulation ID, the first `uAI_MAX_LOD_FOCI` (4).
2. For each entity with `Zenith.AI.Significance`: d = min over foci of |p − f| / weight. With no focus in the world every entity is tier 0.
3. Hysteresis: from tier k the entity moves to a farther tier only when d exceeds that boundary by 5%, and to a nearer tier only when d is 5% below the boundary.
4. Pinned (property 2) or an agent with `AI_AGENT_FLAG_IGNORE_LOD` is tier 0.
5. An entity performs its full update on tick t when (t + simulation ID) mod Interval = 0, so updates spread evenly across ticks; scaled quantities (awareness gain and decay, stuck windows) use Δt × Interval.

The tiers depend only on simulation state (focus entities' positions), never on a camera or render view, so the LOD is part of the deterministic simulation (`Test_AILod_DeterministicAcrossWorkers`). `ai.lod.*` keys affect simulation results and are therefore part of the replay's configuration record.

## Perception

### Senses

| Sense | Input | Update |
|---|---|---|
| sight | stimulus sources in the perceiver's cones, line of sight by physics ray casts | at the perceiver's LOD interval |
| hearing | noises from `AI.Noise` commands | every tick (noises are transient) |
| damage | `AI.Stimulus` DAMAGE commands | at the sync point's following phase 7 |
| touch | `AI.Stimulus` TOUCH commands | likewise |
| forced | `AI.Perception` FORCE_AWARE | likewise |

Teams: `ai.perception.teamNames` names up to 32 teams; a perceiver perceives a source whose `Team` bit is set in its `PerceiveTeamMask`, and never its own entity.

### Stimulus grid

`AI.StimulusGrid` gathers every `Zenith.AI.StimulusSource` whose flags include visible and not hidden into entries (simulation ID, target point = position + TargetHeight × +Y, team, visibility, radius; 40 bytes), sorted by (16 m cell key, simulation ID) like the crowd grid. Position is `Zenith.LocalTransform` 1 for hierarchy roots and `Zenith.WorldTransform` (the previous tick's propagation) for children, a one-tick lag accepted for perception.

### Sight algorithm

For each perceiver due this tick (parallel over chunks):

1. Eye e = position + EyeHeight × +Y; forward f = rotation × +Z (model front, `Zenith2/Docs/Design/CORE/Maths.md` → "Coordinate systems").
2. Candidates: grid entries within max(SightRange, PeripheralRange) of e, team in the mask, not self. For each, d = |t − e| and c = dot(f, (t − e) / d) (c = 1 when d = 0); primary when d ≤ SightRange and c ≥ cos(SightHalfAngle); peripheral when d ≤ PeripheralRange and c ≥ cos(PeripheralHalfAngle); otherwise not a candidate. No angle is computed per candidate: the two cosines are computed once per perceiver update with CORE's `Zenith_Cos` (ruling X314-FMA3) from the configuration, so the cone test is a dot product.
3. Order candidates by (current awareness descending, d ascending, simulation ID ascending).
4. Ray budget: before the parallel loop, the node divides B = `ai.perception.maxRaysPerTick` among the n perceivers due this tick: each gets base = min(`uAI_PERCEPTION_MAX_RAYS_PER_UPDATE` (8), ⌊B / n⌋), possibly 0; then the remainder R = B − n × base goes one ray each, in ascending simulation ID, only to perceivers whose allocation is below 8, while R lasts. So the total never exceeds B (BR-DEFER) and no perceiver exceeds 8; a perceiver left at 0 rays defers its whole sight update (its candidates keep their awareness, counted once per candidate in `ai.perception.rays_deferred_delta`).
5. For candidates in order while the perceiver's rays last: cast `Zenith_PhysicsWorldResource::RayCast(e, normalise(t − e), d, SightBlockers, hit, ignoreRoot = perceiver)` (`Zenith2/Docs/Design/PHYS/ComponentsAPI.md` → "Zenith_PhysicsWorldResource header"). Visible when nothing is hit or the hit entity is the target or one of its descendants. A blocked candidate with Radius > 0 and rays left gets up to two more rays to t ± Radius × right(e → t).
6. Gain for a visible candidate: rate = GainNear + (GainFar − GainNear) × d / SightRange, × PeripheralGainScale when only peripheral, × Visibility; awareness += rate × Δt_update, clamped to 1. Last known position = t − TargetHeight × +Y; senses |= SIGHT; last sensed tick = t.
7. A candidate that ran out of rays keeps its awareness this update (`ai.perception.rays_deferred_delta`); a candidate seen blocked and every memory entry not sensed this update decays by Decay × Δt_update, clamped to 0.

### Hearing algorithm

For every perceiver (each tick, independent of LOD) and every noise of this tick in the noise buffer (in buffer order): d = |noise position − e|; heard when d ≤ loudness × HearingScale; awareness of the instigator += 0.5 × (1 − d / (loudness × HearingScale)), clamped to 1; last known position = the noise position; senses |= HEARING. A noise without an instigator, or whose instigator's team is outside the mask, is skipped (`ai.perception.noise_ignored_delta`).

The noise buffer (`Zenith.AI.NoiseBuffer`, capacity `ai.perception.maxNoisesPerTick` per tick) is written only by `AI.Noise` and `AI.Stimulus` playback: an entry is stamped with the tick whose phase 7 consumes it (the current tick when recorded in phases 1 … 6, the next tick when recorded in 7 … 10); playback drops entries older than the current tick; entries beyond capacity in a tick are dropped in merge order (BR-DROP, `ai.perception.noise_dropped_delta`).

### Damage and touch

A DAMAGE stimulus sets the instigator's awareness to 1.0 (ALERT) with last known position the instigator's position; TOUCH adds 0.5; FORCE_AWARE sets 1.0 with senses FORCED; FORGET removes the entry (event reason 3). Each applies in the phase 7 following its playback.

### Awareness model

- Awareness a ∈ [0, 1] per (perceiver, target); level UNAWARE when a < SuspiciousThreshold, SUSPICIOUS when SuspiciousThreshold ≤ a < 1, ALERT when a = 1.
- All arithmetic is `float` in a fixed order per perceiver; the update of one perceiver reads only committed state, so results are layout- and worker-independent.

### Perception memory

- Property 30 Memory holds at most `uAI_PERCEPTION_MAX_MEMORY` (16) `Zenith.AI.PerceivedTarget` entries sorted by target simulation ID, written through the ECS container heap (`Zenith2/Docs/Design/ECS/Backends.md` → "Container heap").
- A new target is inserted when it gains awareness; with 16 entries, it replaces the entry of lowest awareness (ties: the higher simulation ID) only when its awareness exceeds that entry's (the replaced target is forgotten, reason 4); otherwise it is not remembered this update.
- An entry is removed when its awareness is 0 and it has been unsensed for MemorySeconds (reason 1), or its target no longer exists (reason 2).
- PrimaryTarget is the entry of highest awareness (ties: lower simulation ID); PrimaryAwareness, PrimaryLevel and PrimaryLastKnownPosition copy it; with no entry the target is null and the awareness 0.

### Perception events

Events are derived from committed state against a **ledger** of what was last emitted, so a rejected buffer never loses one. `Zenith.AI.PerceptionLedger` (`Zenith_AIPerceptionLedger`, `Zenith2/Docs/Design/AI/Internals.md` → "Perception ledger") holds per perceiver, in simulation-ID order, the emitted level of each target it reported (at most 16) and its pending forgets (at most 16: target, reason). Every tick, for every perceiver (independent of its LOD tier, after its sight, hearing and stimulus updates):

1. For each memory entry, in the order of the perceiver's entries: when its level differs from the ledger's emitted level for that target (UNAWARE when the ledger has none), emit `Zenith.AI.Stimulus { Perceiver, Target, From = the emitted level, To = the entry's level, Senses, Awareness }` and propose the new emitted level. Two changes whose first event was lost coalesce into one event from the last emitted level.
2. For each target removed this tick (decay reason 1, destroyed 2, forced 3, capacity 4), append a pending forget to the proposal; then emit `Zenith.AI.TargetForgotten { Perceiver, Target, Reason }` for every pending forget (the ledger's and this tick's) in (target simulation ID, reason) order, and propose clearing them and dropping the target's emitted level.
3. Events are recorded on the perceiver's simulation ID (merge order, delivered in phase 9, [Z2-N-ARCH-478]); each is one unit of Z2-B-GAME-003.
4. `Zenith_AIPerceptionMerge` applies the proposal to the ledger only for an accepted buffer; for a rejected one (counted `ai.perception.rejected_delta`) it keeps the ledger unchanged except that this tick's new removals are appended to the pending forgets, so the next tick re-emits every lost event (`ai.perception.reemitted_delta`). The state itself (PerceptionState 20 … 24 and 30) is written in place by the sub-job and stays committed either way ([Z2-N-ARCH-709]; `Test_AIPerception_RejectedEventsReemitted`).
5. A perceiver with more than 16 pending forgets (sixteen removals while its buffers keep being rejected) drops the oldest, counted `ai.perception.forget_dropped_delta`. A perceiver that loses its component or is destroyed loses its ledger entry with no event.

## Blackboard store

### Blackboard records

- `Zenith.AI.Blackboards` (`Zenith_AIBlackboardStore`) holds records (80 bytes): owner simulation ID (8), key (`Zenith_StringID`, 4, reserved 4, plus its `Zenith_Hash64` of the text, 8), value (`Zenith_AIBlackboardValue`, 48), expiry tick (8; 0 = never) — indexed by a `Zenith_HashMap` from (owner, key hash) to the record index, plus a per-owner key count and an **expiry index** (a binary min-heap of (expiry tick, owner simulation ID, key hash) over the records with an expiry, 24 bytes per entry).
- Value kinds: 1 BOOL, 2 NUMBER (`f64`), 3 VECTOR (three `f32`; a direction, an offset or any triple that is not a world position: never shifted), 4 ENTITY, 5 NAME (an interned string), 6 POSITION (three `f32`, a world-local position: shifted at an origin rebase, `Zenith2/Docs/Design/AI/Navigation.md` → "Origin shift"). Luau reads kinds 3 and 6 alike as `vector`; a script chooses POSITION by writing through `AI.SetBlackboardPosition` (`Zenith2/Docs/Design/AI/Scripting.md` → "Binding functions").
- Capacity: `uAI_BLACKBOARD_MAX_ENTRIES_PER_WORLD` (65,536) records and `uAI_BLACKBOARD_MAX_KEYS_PER_OWNER` (64) keys per owner, both counting **unexpired** records only ("Blackboard reads"); at 65,536 records the store is 5 MiB plus the index.
- A blackboard is any entity's: an agent's own facts, or a squad's shared facts on a squad entity that members reference by entity handle (cross-instance data through entity handles, never shared tables, [Z2-N-SCRIPT-381] and the behaviour-sharing rule of `Zenith2/Docs/Scripting.md` → "Behaviour state model and recovery").

### Blackboard commands

`AI.Blackboard` playback, in merge order:

| Op | Effect | Refused (counted, ignored) when |
|---|---|---|
| `SET` | insert or replace (owner, key) with the value and expiry | the owner does not exist; a new key beyond 64 unexpired keys for the owner (`ai.blackboard.owner_full_delta`); a new record beyond 65,536 unexpired records (`ai.blackboard.full_delta`); a value kind outside 1 … 6; interning the key or a NAME value into the world's string table fails because Z2-B-SCRIPT-022's capacity is exhausted (`ai.blackboard.intern_full_delta`; nothing is inserted, `Test_AIBlackboard_InternFullRefused`) |
| `CLEAR` | remove (owner, key) | — (absent is a no-op) |
| `CLEAR_ALL` | remove every key of the owner | — |

The AI module's lifecycle listener removes every record of an entity at `LIFECYCLE_EVENT_DESTROYED`.

### Blackboard reads

- Readers declare `ReadResource(Zenith.AI.Blackboards)` (scripts list it in `Uses`) and read the store as the latest sync point left it: a phase-2 `SET` is visible to phases 3 … 10 of the same tick, a phase-10 `SET` from phase 1 of the next tick.
- An expired record (expiry tick ≤ the reading tick) reads as absent. **Expiry rule** (one rule everywhere): an expired record is not simulation state. Capacity and key counts count only unexpired records; the first `AI.Blackboard` playback of each tick first **prunes**: it pops every expiry-index entry whose expiry tick ≤ the current tick, in (expiry, owner, key hash) order, and removes those records physically (decrementing the owner's key count and the world count), so a `SET`'s refusal test sees exactly the unexpired records. A capture writes only the records unexpired at its tick (expiry 0 or > t), and the canonical AI state hash covers only those, so a store holding an expired, not yet pruned record hashes and saves exactly like one that has pruned it: a `SET` refused or accepted live is refused or accepted alike after a restore (`Test_AIBlackboard_ExpiryReadsAbsent`).
- An entity value that has become stale reads as nil in Luau (`Zenith2/Docs/Design/AI/Scripting.md` → "Binding functions").

## Determinism rules

| # | Rule | Where |
|---|---|---|
| A1 | every AI node reads committed state only: other rows only in columns no phase-7 system writes; resources at the version its declared ordering gives | "Frame-graph placement" |
| A2 | every per-entity computation depends only on that entity's row, the committed resources and its own runtime record; parallel sub-jobs never share a written location | "Agent runtime table" |
| A3 | every iteration over a set uses a total order of stable keys (simulation ID, tile key and polygon index, completion ID); no order depends on a slot, a pointer, a hash-map bucket, a chunk, a worker or the order tiles were added — the external-link lists are kept sorted, and which overflow candidates exist is a ranking over the loaded tiles, for exactly this | `Zenith2/Docs/Design/AI/Navigation.md` → "Tie-breaking", "Tile connection procedure" |
| A4 | budgets (expansions, rays, rebuilds, active requests) are divided before the parallel work by a function of committed state, so the work done per tick is the same on every worker count | "Sight algorithm" step 4; `Zenith2/Docs/Design/AI/Navigation.md` → "Per-tick processing" |
| A5 | every carve and query result is a pure function of the navmesh content and the arguments; a sliced search's state is a pure function of the navmesh history it saw, its descriptor and its expansion count, and is saved whole | `Zenith2/Docs/Design/AI/Navigation.md` → "Search state and slicing" |
| A6 | every world-visible output is a command, an event or a component write on the agent's own row, recorded under the agent's simulation ID, merged in merge-key order | "Locomotion output" |
| A7 | randomness comes only from `Zenith_RandomSource` streams keyed (world seed, `AI` source ID, simulation ID) with a counter in the agent's record, or from the caller's script stream | `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_RandomSource" |
| A8 | time comes from the tick index and the tick duration, never a clock; `float` arithmetic in a fixed order under the fixed FP environment | `Zenith2/Docs/Contracts.md` → "Floating-point environment" |
| A9 | the LOD is computed from simulation state alone | "Simulation LOD" |
| A10 | script path results publish at the tick after their search finished, a deterministic tick, so live runs and replays agree | `Zenith2/Docs/Design/AI/Navigation.md` → "Per-tick processing" |
| A11 | navigation files reach simulation only through `ASYNC_SOURCE_NAVIGATION` publications at tick boundaries in completion-ID order; no phase node reads an asset's live state | `Zenith2/Docs/Design/AI/Navigation.md` → "Navigation asset requests" |
| A12 | every world-visible event is re-derived from committed state against a ledger of what an accepted buffer last emitted, so a merge rejection delays an event and never loses it | "Perception events"; "Agent state machine" |

The determinism suites replay the AI scenarios with 1, 2, 8 and all workers, forced compaction and two chunk capacities, on each ECS backend with the Jolt and PhysX backends, and compare the per-tick world hash and the canonical AI state hash (`Zenith2/Docs/Design/AI.md` → "Canonical AI state hash"; `Zenith2/Docs/Design/AI/Tests.md` → "Save and determinism tests").


## Save and restore algorithm

The AI module registers one `Zenith_SaveParticipant` per play world (`Zenith_AISaveParticipant`, `Zenith2/Docs/Design/AI/Internals.md` → "Save participant class"; `Zenith2/Docs/Design/SAVE/PublicAPI.md` → "Zenith_SaveParticipant header"; cross-area row SV17): name `Zenith.AI.Navigation`, section version 1, `GetFlags()` 0 (not REQUIRED, not COHERENT, not EXCLUDE_FROM_RESUME), `GetReissuedSourceKinds()` = `(1u << ASYNC_SOURCE_PATHFINDING) | (1u << ASYNC_SOURCE_NAVIGATION)`.

### Fidelity

Every AI capture is EXACT (the orchestrator's ruling AI-R1, finding 1; Q70 (a) and Q82 (b): COHERENT only for a suspended `Run` coroutine or an in-flight additive load, neither of which is AI's). `GetCaptureFidelity` is SAVE's default. Every piece of AI state that affects simulation is in the section: the applied rebase count and applied origin; the loaded tiles and volumes with their versions and supplier slots; the supplier records; the navigation load records; the carve queue with its first-queued ticks and every carved tile's obstacle set; the obstacles' applied records; the link registrations; the agent records; the path service's queue, inbox and active searches **with their whole search state**; the perception ledger; the unexpired blackboard records; the pending noises. The two grids are rebuilt from components every tick and are not saved.

There is exactly one non-EXACT outcome, and it is decided **at restore**, never at capture: content that differs from the content the capture ran against — a navigation file re-cooked or missing since the save, or a carved section whose rebuilt check differs from the saved one (possible only on another build or platform, `Zenith2/Docs/Design/AI/Navigation.md` → "Bake determinism") — is outside [Z2-N-ARCH-881]'s "same build, same content". The restore then drops the affected corridors and replans those agents, and reports the restore COHERENT through `Zenith_SaveRestoreContext::ReportCoherentRestore(participant key)` (the member PHYS requested, row PH22; `Zenith2/Docs/Design/AI.md` → "Assumed interfaces"), counting `ai.save.coherent_fallbacks` and logging once per asset. A save restored on the build and content that wrote it is always EXACT.

`DeclareAccess`: `ReadResource` of `Zenith.AI.World`, `Zenith.AI.PathService`, `Zenith.AI.AgentRuntime`, `Zenith.AI.PerceptionLedger`, `Zenith.AI.Blackboards` and `Zenith.AI.NoiseBuffer` on the phase-11 snapshot system.

### Capture

`CaptureSection` (phase-11 snapshot node, or the main thread out of tick) writes the lists of `Zenith2/Docs/Design/AI/Formats.md` → "Save section layout" in their order, each in its stable order (tile key, simulation ID, completion ID), never in slot order. Two invariants hold at every phase 11 and are asserted: the navigation join list is empty (it is filled at a tick boundary and consumed by the next phase 7); and every proposed field of every agent record and ledger entry has been resolved by its merge function. The completion gate's entries are not saved: in production it is empty after every phase 7, and a record a test hold keeps in it is still ISSUED, so the restore re-issues it (`Zenith2/Docs/Design/AI/Navigation.md` → "Completion gate"). A link-node reference that no longer resolves at capture (a dead link node in a search, or a corridor entry or corner deeper than the 8 validated entries whose link died) is written as the dead entry; the capture never invalidates, truncates or replans anything, so it changes no state. The path inbox may hold script requests committed on the phase-10 merge chain; they are saved as inbox entries.

### Restore

`RestoreSection` (SAVE's `WORLD_RESTORE_STATE_PARTICIPANTS`, main thread, no frame running, after every entity and world resource exists):

1. **No section.** When `xContext.m_bSectionPresent` is false (a save written before the module was selected), nothing is restored: the world requests and joins its navigation through the ordinary path from its first tick, which is consistent and not identical; AI calls `ReportCoherentRestore`, logs once (`ai: save has no navigation section`) and `IsRestoreReady` returns true at once.
2. **Decode.** Decode the section through `Zenith_SaveSectionReader` into a restore-owned decoded copy (one block of the participant's heap that step 8 of `IsRestoreReady` frees); every count is checked against "Parser limits" and the bytes remaining, every index against its list, and any failure is `SAVE_ERROR_CORRUPT`; an older section version migrates forward one step per version. The header's applied rebase count and applied origin are compared with the restored `Zenith.WorldOrigin` (SAVE's `WORLD_RESTORE_STATE_RESOURCES` restores it before `WORLD_RESTORE_STATE_PARTICIPANTS`, `Zenith2/Docs/Design/SAVE/PublicAPI.md` → "Zenith_WorldSnapshot header"): a different count, or an equal count with a different origin, is `SAVE_ERROR_CORRUPT`; otherwise both are set in `Zenith.AI.World` now, before any tile is added, so no ingest after the restore shifts anything a second time (`Zenith2/Docs/Design/AI/Navigation.md` → "Origin shift", rebase bookkeeping; `Test_AISave_RestoreAfterRebaseNoSecondShift`).
3. **Re-issue.** For every saved script path request (inbox, queued or active, state recorded in flight) and every navigation load record in state ISSUED, in ascending completion ID: when `xContext.m_pxReissue->WasInFlight(id, kind)` is false for its kind (`ASYNC_SOURCE_PATHFINDING` or `ASYNC_SOURCE_NAVIGATION`), or `MarkReissued(id)` returns false, the section and SAVE's async section disagree and `RestoreSection` fails with `SAVE_ERROR_CORRUPT` (SAVE then destroys the world, ANIM's rule, `Zenith2/Docs/Design/ANIM/Runtime.md` → "Save state"). Otherwise the path request is kept under its recorded completion ID in its saved position (it is `PUBLICATION_KIND_GENERIC_RECOMPUTED`, so with `m_bUnderPublicationSource` it is still recomputed and its result published at its tick), and the load record is re-issued on the world's ordinary completion queue `ai.loads.w<tag>` under its recorded ID. Records in PARKED_OK, PARKED_FAILED and ABANDONED are kept as saved: SAVE restores their results as completed and unpublished, and AI's handler receives them at the first tick boundary exactly as the uninterrupted run did. Every in-flight entry of either kind the section does not list stays unmarked and SAVE completes it with the abandoned payload, which AI's handler and SCRIPT drop.
4. **Acquire.** For every saved loaded tile and volume, for the settings asset, and for the asset of every PARKED_OK load record, `RequestLoad` on the world's **restore queue** `ai.restore.w<tag>`, whose completions are never parked. Its capacity is the number of acquisitions a restore can need: at most `ai.nav.maxTiles` tiles (list 1 is refused beyond the tile table), at most 256 volumes (the 8-bit volume slot), at most `ai.nav.maxLoadsInFlight` PARKED_OK records (list 3 holds only records in flight), and 1 settings asset, so `ai.nav.maxTiles` + 256 + `ai.nav.maxLoadsInFlight` + 1 = 17,665 at the defaults. When that sum exceeds the ASSET queue maximum of 65,536 (possible only near the keys' maxima), the queue is created at 65,536 and step 4 issues the acquisitions in (kind, tile key or GUID) order up to its free slots; `IsRestoreReady` step 1 issues the rest, in the same order, as drains free slots. A refusal with `ASSET_ERROR_QUEUE_FULL` therefore never occurs, because no request is made without a free slot. Whatever the batching, the restore allocates no completion ID, so the world's next completion ID, which SAVE restores from its async section, equals the uninterrupted run's.

`IsRestoreReady` (SAVE's `WORLD_RESTORE_STATE_PARTICIPANTS_READY`, polled once per restore `Step`; nothing blocks):

1. Drain the restore queue. While an acquisition is outstanding, return false; `ListRestoreWaits` then lists the outstanding restore-queue load operations in ascending asset GUID, the first `axOut.GetCount()` of them, so a synchronous restore driver (`Zenith2/Docs/Design/SAVE/RuntimeBridge.md` → "Synchronous restore driver") services exactly the loads AI waits for outside a frame (`Test_AISave_SynchronousRestoreListsNavLoads`); it lists nothing once ready.
2. **Tiles and volumes**, once every acquisition is ACTIVE or FAILED: accept the saved settings (a settings asset that failed or whose hash differs makes every tile absent: content changed); restore the supplier records (list 4) exactly as saved, slot states included; add every saved tile in tile-key order through steps 4, 5 and 7 of `Zenith2/Docs/Design/AI/Navigation.md` → "Tile add procedure" and the **connection part** of its step 6 only (the overflow ranking recomputed per add, so the lists end equal to the saved world's), with no per-tick limit, recording the saved supplier simulation ID and supplier slot and setting the saved **version** (not 1); add every saved volume likewise. Step 6's MARK re-application, carve queueing and link resolution are not run here: steps 3 and 4 below restore exactly what they would have produced. A tile whose acquisition FAILED is absent, and one whose content check differs from the saved check (re-cooked) is added with its new content and marked content-changed: either reports COHERENT (above).
3. **Carving.** Restore the obstacle applied records and the rebuild queue (keys and first-queued ticks, in saved order). For each saved carved record in tile-key order whose tile is present and not content-changed: run `Zenith_NavRebuildTileFromLayers` synchronously with the record's saved obstacle set (shapes converted from the saved applied poses exactly as at the live rebuild), swap it in, set the saved version, and compare the rebuilt section's check with the saved check; a mismatch keeps the rebuilt polygons, counts `ai.save.carve_mismatch` and reports COHERENT. A content-changed carved tile is queued for a rebuild from the obstacles' applied records instead. Then re-apply every MARK obstacle at its applied pose ("Mark procedure" step 2), which reproduces the saved overlay because mark sets are a function of the tiles and the pose. Restored obstacles are re-dirtied only by comparison with their applied records, never by change bits, so the first tick re-queues exactly what the uninterrupted run would (`Test_AISave_CarvedTwiceRestoresExact`), and the queue drains under `ai.nav.maxTileRebuildsPerTick` from that tick (`Test_AISave_QueuedCarveIsExact`).
4. **Links.** Restore every saved link registration (link entity simulation ID, class, snapped ends as (tile key, polygon, tile-local position), registered entity position) in simulation-ID order into fresh link table slots (salts start at 1; no link-node reference survives a session), and the unresolved set; a registration naming an absent or content-changed tile returns its link to the unresolved set.
5. **Agents.** Restore every agent record in simulation-ID order, resolving each corridor entry and corner reference from its tagged save form (`Zenith2/Docs/Design/AI/Formats.md` → "Save section layout", node reference entry): a polygon by (tile index, polygon index) through `Zenith_NavMesh::ResolvePoly`, a link node by (link entity simulation ID, class) to the restored link table's link-node form, a dead entry (kind 3) as `Zenith_NavPolyRef::MakeLinkNode(0, 0)`, which stays dead exactly as the live entry was (`Zenith2/Docs/Design/AI/Navigation.md` → "Search graph"); and the current and target polygons by (tile index, polygon index); an agent with a reference into an absent or content-changed tile drops its corridor and corners and queues a replan from its position (`Test_AISave_RecookedTileReplans`).
6. **Path service.** Restore the inbox, the queue and the active set in their saved order. Each active search is rebuilt from its saved state with `Zenith_NavPathSearch::RestoreSnapshot` (or `Zenith_NavVolumeSearch::RestoreSnapshot`): the node array in saved index order with each node reference re-resolved — a polygon node by (tile key, polygon index), a link node by (link entity simulation ID, class) to the restored link table index, a dead link node (record kind 4) as `MakeLinkNode(0, 0)` — the open heap in saved heap order, the best node, the expansion count, the tile stamps with the restored tiles' salts and saved versions (a volume search's stamps name volumes by their restored slots and salts, "Volume search" of `Zenith2/Docs/Design/AI/Volumes.md`), the mesh serial set to the restored mesh's current serial, and the visited-node lookup rebuilt from the node array. The restored search's next `Step` therefore pops exactly the node the uninterrupted run pops, whatever overlay or tile changes it straddled before the capture ([Z2-N-ARCH-881]; `Test_AISave_SearchStraddlingOverlayChange`). A search with a node in a content-changed tile is restarted from Begin at the first tick instead (counted `ai.path.restarted_delta`; COHERENT already reported).
7. **Perception, blackboards, noises.** Restore the perception ledger, the blackboard store (the saved unexpired records; per-owner counts, the hash index and the expiry index rebuilt) and the noise buffer.
8. Free the decoded copy and return true.

`Test_AISave_RestoreContinuesBitIdentical` records a W-Characters-shaped run with `--record` in which the origin rebased once before the capture, saves it at a tick with agents moving (two of them with a JUMP link node in their corridor, one flyer with an active volume search), one tile carved twice and a third rebuild queued, a MARK obstacle that moved between two slices of an active search, 12 active and 30 queued path requests (8 of them script requests, 2 still in the inbox from phase 10), three navigation loads in flight (one issued and held by the completion gate across the capture, one parked, one abandoned), a perceiver whose last buffer was rejected and an owner holding 64 keys of which 3 are expired, restores it through `Zenith_RuntimeReplay_ResumeFromSave` and compares 300 further ticks' world and AI hashes with the recorded run (SAVE's reference for a continuation with I/O-timed sources in flight, `Zenith2/Docs/Design/SAVE.md` → "Determinism obligations").
