# Scripting — the Behaviour Graph runtime

The interpreter behind the engine's visual scripting system. Designers author
`.bgraph` node graphs in the editor (`Editor/Panels/Zenith_EditorPanel_GraphEditor`);
this module executes them. It replaced the retired C++ script-behaviour system
outright — there is no other gameplay-scripting path.

**Doctrine:** systems are ECS components (C++); gameplay logic is graphs.

## Layering

`Zenith/Scripting/` is a low-layer module: it depends on **ZenithBase + ZenithECS
only** and never names Flux, Physics, AssetHandling, or any concrete component
(enforced by the architecture gate; the module is declared in
`Tools/complexity_profiles.json`). Everything engine-facing lives elsewhere:

- `AssetHandling/Zenith_BehaviourGraphAsset` — the `.bgraph` asset wrapper
  (registered with `Zenith_AssetRegistry`; dead-strip anchored via a ForceLink
  call from the host component).
- `EntityComponent/Components/Zenith_GraphComponent` — the host component
  (slot list, blackboard overrides, lifecycle/collision dispatch, custom-event
  firing with optional payload, registered at meta order 60 as "Graph").
- `EntityComponent/Zenith_GraphNode_Registration.cpp` — the engine node library
  (**139 nodes** — count them with
  `grep -c "RegisterNodeType<" EntityComponent/Zenith_GraphNode_Registration*.cpp`
  rather than trusting this line, which has been stale before: core
  events/blackboard/flow in
  the main TU + domain sub-registrar TUs `Zenith_GraphNode_Registration_
  {Input,Flow,Entity,Math,Scene,Physics,Animation,UI,AI}.cpp` — all filled;
  see `EntityComponent/CLAUDE.md` for the per-TU node families), installed by
  `Zenith_Engine::Initialise` via `SetNodeRegistrar`. Games register custom
  nodes from their `Project_RegisterGameComponents` hook (see "Game node
  libraries" below).
- `Scripting/Zenith_GraphBuilder.{h,cpp}` — leaf-safe fluent programmatic
  authoring (Variable/Node/Param*/Edge/Chain/Build; error-latching; auto grid
  layout). The bulk boot-authoring path: games use
  `Zenith_EditorAutomation::AddStep_GraphBuild(path, pfnBuild)`; the
  `AddStep_Graph*` click-steps stay for editor-coverage tests.
- `EntityComponent/Zenith_GraphReload` — TOOLS-only hot reload (editor Save +
  FileWatcher on `game:Graphs/*.bgraph`), drained at the main loop's safe point.

## Files

- `Zenith_GraphNode.h` — `GraphNodeStatus` (SUCCESS / FAILURE / RUNNING),
  `GraphEventType` (None, OnStart, OnUpdate, OnFixedUpdate, OnEnable, OnDisable,
  OnDestroy, OnCollisionEnter/Stay/Exit, Timer, Custom, OnGraphCall — append-only,
  never serialized), `Zenith_GraphEventArg` (named multi-field payload element),
  `Zenith_GraphContext` (`m_xSelf`, `m_fDt`, `m_fTimeSeconds` wall-clock,
  graph/blackboard pointers, optional `m_pxEventPayload` + named
  `m_pxEventArgs`/count, `m_bResumeDrive` (see "Resume drives" below), and
  `ResolveTargetEntity(var)` — the entity-targeting
  seam: "" = self, else a packed-EntityID blackboard var, resolved leaf-safe via
  `Zenith_SceneSystem::Get()`), and the node base class (`Execute`,
  `GetTypeName`, `MatchesCustomEvent`, chain-lifecycle `OnEnter`/`OnExit` —
  **now invoked by `RunChainFromPin`**: one OnEnter per run of a node (a
  suspended node resuming does NOT re-enter), OnExit on SUCCESS/FAILURE never
  RUNNING — plus `OnAbort` (preemption: reset per-run state; flow nodes forward
  into their active pins via `AbortChain`) and `GetDynamicExecOutputCount`
  (variable-pin flow nodes; pin ≤ 255 by the cursor-key layout)).
- `Zenith_GraphNodeRegistry.{h,cpp}` — `RegisterNodeType<T>(name, eventType,
  outputCount, bFlowNode, category, bHasFailurePin = false)` derives the
  create-fn, property table (via `ZENITH_PROPERTY`), and type version from the
  node class; name-keyed; duplicate-guarded; registrar inversion keeps this
  module leaf-safe. `bHasFailurePin` opts the type into the routable
  **On Failure** exec pin (see "Routable failure" below) and is VALIDATED at
  registration — a refused flag is a `Zenith_Error` plus a forced `false`, never
  an assert, so the refusal is something a test (and the editor) can read back.
- `Zenith_GraphBlackboard.{h,cpp}` — name → `Zenith_PropertyValue` store with
  typed getters (`GetFloat/GetBool/GetInt32/GetVector2/3/4/GetString/
  GetPackedEntityID(name, default)` — return the default on missing OR type
  mismatch, never reinterpret), `SetValue`, `TryGetValue`, `VisitAll`, plus a
  parallel LIST store (`GetOrCreateList`/`TryGetList`/`RemoveList` — runtime
  collections like entity-query results; deliberately NOT a PropertyValue type,
  never asset-declared; serialized as a section after the values, gated by the
  container's version via `ReadFromDataStream(stream, bWithLists)`). Two copy
  semantics, chosen deliberately: `ApplyOverridesFrom` (scene-load: ad-hoc
  variables restore verbatim, type conflicts drop) vs `CopyMatchingFrom` (hot
  reload: strict name+type match only); lists carry verbatim in both (always
  ad-hoc).
- `Zenith_BehaviourGraph.{h,cpp}` — `Zenith_GraphDefinition` (variables, nodes
  with length-framed param blobs, edges, editor positions; magic `XBGR`,
  version 1; UNKNOWN node types preserved verbatim as unresolved nodes — a
  future/missing node never silently drops from the asset) and the runtime
  `Zenith_BehaviourGraph` instance (see "Execution model").
- `Zenith_Scripting.Tests.inl` — chain execution order/params, RUNNING
  suspension/resume, branch flow semantics, serialization round-trip,
  unresolved-node preservation, custom-event name matching, blackboard
  type-safe migration, corrupt-definition rejection, the flow-node family
  (Selector/Switch/StateMachine/Repeat/ForEach/**Sequence**), the
  resume-drive flag per anchor class (`ResumeDrive_FlagNeverSetOutsideTheTwoPaths`),
  and the **routable failure pin** (the `FailurePin_*` block: unwired parity,
  wired continuation under the same key, a suspending handler, abort, the
  Selector and Repeat surprises, the three registration refusals, the inert
  edge on an unflagged type, the unresolved abort, the cycle cap, and
  `Zenith_GraphBuilder::FailPin`). The editor half — the extra pin laid out and
  keyed, and `Action_Connect` accepting it only on a flagged type — is
  `Editor/Panels/Zenith_EditorPanel_GraphEditor.Tests.inl`.

## Execution model

A `Zenith_BehaviourGraph` instance is built per graph slot from its definition
(`InitialiseFromDefinition`): nodes instantiated through the registry (an
unregistered type stays a null-node `NodeInstance` and bumps
`GetUnresolvedCount()`), event sources indexed per `GraphEventType`, blackboard
seeded from the declared variables.

- **Event-driven:** only fired event sources tick their chains; idle graphs are
  ~free. `FireEvent(eType, ctx)` snapshots the source list, then runs each
  source; `FireCustomEvent(szName, ctx)` additionally filters sources by
  `MatchesCustomEvent`.
- **Sources gate themselves:** an event source's own `Execute` returns
  SUCCESS/FAILURE to decide whether its chain runs that fire (most sources
  return SUCCESS every fire; Timer accumulates `m_fDt` and only succeeds once it
  reaches its interval, then subtracts it). Downstream action/flow nodes, by
  contrast, always `Execute` when the chain reaches them.
- **Chains:** one outgoing edge per (node, pin). A plain node's SUCCESS
  auto-continues from its pin 0; FAILURE aborts the chain **unless the node's
  type carries a wired failure pin** (below); flow nodes (`bFlowNode`, e.g.
  Branch/Loop) drive their own output pins from inside `Execute` via
  `RunChainFromPin`.
- **Routable failure (`m_bHasFailurePin`):** a node type may opt into ONE extra
  exec output at index `m_uExecOutputCount` — "On Failure". The tri-state is
  unchanged; this is only where the walk goes next.
  - **Unwired = today's behaviour**, exactly: the chain aborts. The flag alone
    changes nothing.
  - **Wired:** the walk CONTINUES down the failure edge **under the same chain
    key** (the key is the anchor's, captured before the walk), so a handler that
    returns RUNNING suspends the anchor's chain and resumes at the handler —
    without re-running the failing node and without a second `OnEnter` — and
    `AbortChain(anchor, pin)` reaches the handler's `OnAbort`.
  - **The failing node has already had `OnExit`.** Failing is a completed run of
    that node whatever happens to the chain next (`-f` precedes `+h` in the
    lifecycle log).
  - **The chain's status is the continuation's terminal status** (precedent:
    `Repeat m_bUntilFailure`). ★ **This is the surprise**, and it is not a bug:
    a failure handler REPLACES the branch's answer rather than adding to it. A
    `Selector` whose pin-0 branch fails but whose handler SUCCEEDS sees SUCCESS
    and never falls through to pin 1; `Repeat(m_bUntilFailure)` never reaches
    its done pin; `Loop`/`ForEach` keep iterating; `RunGraphCall`'s
    `bAnyCompleted` flips the call to SUCCESS. Put a handler on a node inside a
    Selector branch only when you mean "this branch has now succeeded".
  - **The flag is read off the SOURCE TYPE, never inferred from the edge.** An
    edge at that index on an unflagged type is inert.
  - **Refused, observably, on four shapes** (`Zenith_Error` + forced `false`):
    any **flow node** (fixed or dynamic — a flow node's FAILURE is the status it
    propagated out of a sub-chain it ran itself, so "Branch › On Failure" would
    mean "the child failed"), any **event source** (its FAILURE is a gate that
    returns from `RunSourceNode` before any chain walk, so the pin would be dead
    by construction), any **dynamic-pin** type (the index would move with the
    branch count), and `m_uExecOutputCount >= 255` (the cursor key packs the
    pin into its low byte). A type info with no create fn cannot be probed and
    is refused too.
  - **The unresolved-node abort is NOT routable** — there is no type info there
    to carry a flag.
  - **No format change.** An edge already carries a source pin index, so a
    failure wire is an ordinary edge at pin `m_uExecOutputCount`.
  - Authoring: `Zenith_GraphBuilder::FailPin(nodeID)` names the index (and
    error-latches on an unflagged or unknown node) so no builder hard-codes it.
- **Cycle cap (`uGRAPH_MAX_CHAIN_STEPS` = 4096):** `AddEdge` rejects only
  SELF-loops, so a chain wired back into a node it already passed spins forever
  — a **pre-existing** hazard on the SUCCESS path (`a → b → a` hangs identically
  and always did), which a failure wire makes easier to author by accident.
  `RunChainFromPin` therefore counts steps per walk and, on exceeding the cap,
  reports once per graph instance (`HasHitChainStepCap()`), clears the cursor
  and returns FAILURE. A hung walk inside a headless unit batch is a watchdog
  kill with no failing test; this turns it into an ordinary failure.
- **RUNNING suspension:** a node returning RUNNING stores a chain cursor
  (`m_xChainCursors`, keyed `(anchorID << 8) | pin`) and the chain resumes AT
  that node on the next fire. One-shot anchors (OnStart, collisions, custom
  events, ...) that suspend are re-driven by the ON_UPDATE dispatch until they
  finish; periodic anchors (OnUpdate/OnFixedUpdate/Timer) resume on their own
  next fire.
- **Editor introspection:** `GetRecentlyExecuted()` (cleared per ON_UPDATE,
  capped 64) and the currently-executing node ID feed the editor's live
  execution highlighting.
- **Preemption (adoption-program Part 1):** `AbortChain(node, pin, ctx)` kills
  a suspended chain — OnAbort on the cursor node (flow nodes cascade into
  their active pins), cursor + matching one-shot anchor cleared;
  `AbortAllChains` for whole-graph teardown (CallGraph). This is what makes
  the reactive `Selector`/`StateMachine` flow nodes BT-equivalent. The
  **BT**-sequence (children in order, stop at the first FAILURE) is
  deliberately absent — a linear exec chain IS that. The **Blueprint**-sequence
  (exec fan-out) is the `Sequence` flow node, below.
- **Exec fan-out is a node, not an edge rule:** a (node, pin) has at most ONE
  outgoing edge, so one source feeding N independent chains is the `Sequence`
  node (`_Flow.cpp`), never a raw fan-out. Branches are independent: they run
  in pin order within a fire, a branch FAILURE stops only that branch and is
  swallowed (Sequence returns RUNNING while any branch is suspended, SUCCESS
  otherwise, and never FAILURE). `OnAbort` forwards `AbortChain` into every
  pin.
- **Resume drives (`Zenith_GraphContext::m_bResumeDrive`):** a suspended node
  is re-executed WITHOUT `OnEnter`, so a fan-out node cannot otherwise tell a
  fresh fire from a resume. The flag means *this drive reached the chain
  through a cursor AND the anchor is not OnUpdate/OnFixedUpdate*. It is set —
  and scope-restored, because the context object is caller-owned and reused
  across sources and frames — in exactly two places, both in
  `Zenith_BehaviourGraph.cpp`: `RunSourceNode`'s cursor branch and `FireEvent`'s
  one-shot re-drive loop. `RunGraphCall`'s cursor branch does NOT decide; the
  `CallGraph` node copies the caller's context whole, so a child graph inherits
  the caller's answer. Set ⇒ `Sequence` fires only its uncompleted pins
  (per-instance u64 mask, reset in `OnEnter`, a pin completing on SUCCESS *or*
  FAILURE); clear ⇒ every pin fires. `false` is not a compatibility default:
  clear IS the OnUpdate semantics (Blueprint's Event Tick → Sequence re-fires
  every pin every tick).
- **Timer → Sequence is one occurrence at a time — NOT Blueprint parity.**
  `RunSourceNode` returns at the cursor before the Timer's own `Execute`, so
  while the chain is suspended the interval does not advance at all, and the
  chain is resumed on EVERY ON_UPDATE dispatch rather than once per interval.
  A Timer occurrence therefore never overlaps itself.
- **Sub-graphs:** `RunGraphCall(ctx)` runs every `OnGraphCall` entry anchor
  (RUNNING if any suspended, FAILURE when all anchors failed). The `CallGraph`
  node executes a child asset against the CALLER's blackboard (shared scope;
  the child's declared variables are its parameter list, defaults seeded where
  absent), caches the child per call-site, cuts recursion at depth 8, and
  re-resolves after TOOLS hot reloads.
- **Idle-graph skip:** `NeedsUpdateDispatch()` — the component skips graphs
  with no OnUpdate/Timer sources, no suspended chains, and no cursors from the
  ON_UPDATE dispatch entirely (pinned by the idle phase of the 1000-entity
  benchmark).

## Contracts worth knowing

- **Unresolved nodes are preserved, never dropped.** A `.bgraph` containing a
  node type this build doesn't register still loads, runs (chains through the
  missing node fail gracefully, warned once per node — `NodeInstance` carries an
  `m_bWarnedUnresolved` latch so a hot chain doesn't spam the log), round-trips
  on save, and renders error-red in the editor. Pinned by unit test and by the windowed DP tests
  `Test_GraphEditorLiveAuthoring` / `Test_GraphEditorScreenshotTour`.
- **Hot reload happens only at the main loop's safe point** (never
  mid-dispatch — asserted via `Zenith_GraphComponent::IsDispatchInProgress`),
  is atomic (a failed parse keeps the old graph live), and migrates blackboard
  state name+type-matched.
- **Custom events carry an optional payload.**
  `Zenith_GraphComponent::FireCustomEvent(szName, pxPayload)` reaches
  `OnCustomEvent` source nodes, which stash the payload into a blackboard
  variable (default `"payload"`) — the packed-EntityID pattern the collision
  sources also use. This is the standard C++→graph plumbing seam: input/system
  code fires a named event at exactly the point the old C++ call sat; the
  graph owns the decisions from there.
- **No std::function anywhere** — node creation, property access, and dispatch
  are all function pointers, per engine convention.
- Graphs are strictly main-thread, dispatched through `Zenith_GraphComponent`'s
  snapshot-iteration discipline.

## Porting C++ logic to graphs (the conversion playbook)

Used for every shipped conversion (DP interactables + door, Combat attack +
round flow, TilePuzzle pinball ball-lost flow):

1. **Characterization tests first.** Write automated tests against the C++
   version through real input paths; run them green; they must pass unchanged
   after the conversion.
2. **Split decisions from systems.** Decisions (state transitions, gating,
   scoring, win/lose) move into nodes whose `Execute` bodies are the old C++
   verbatim. Systems work (physics, navmesh, colliders, materials, entity
   spawning) stays on a C++ shim component exposing a small graph-facing
   surface; nodes call it synchronously (no 1-frame races).
3. **Fire the driving event at the old callsite.** The shim fires a custom
   event (dt as a float payload when the node needs it) exactly where the old
   C++ call sat, preserving same-frame ordering.
4. **Author the graph at boot** through `Zenith_EditorAutomation` AddStep_Graph*
   steps (regenerated every tools boot, like scenes), attach via
   `AddStep_AttachGraph` (authored entities) or
   `Zenith_GraphComponent::AddGraphByAssetPath` (runtime-spawned entities).
   Per-entity state lives on the graph blackboard; tunables are read live from
   config or exposed as node properties.
5. **Delete the C++ decision code** and re-run the characterization tests plus
   the full suites.

## Game node libraries

Each converted game keeps its custom nodes in one header, registered from
`Project_RegisterGameComponents()`:

| Game | Header | Registration fn |
|---|---|---|
| DevilsPlayground | `Games/DevilsPlayground/Components/DP_GraphNodes.h` | `DP_RegisterGraphNodes` |
| Combat | `Games/Combat/Components/Combat_GraphNodes.h` | `Combat_RegisterGraphNodes` |
| TilePuzzle | `Games/TilePuzzle/Components/Pinball_GraphNodes.h` | `Pinball_RegisterGraphNodes` |

See each game's CLAUDE.md for its node table and authored graphs.
