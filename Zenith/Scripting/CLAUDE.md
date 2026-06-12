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
  (26 nodes: 11 event sources, 10 actions, 5 flow), installed by
  `Zenith_Engine::Initialise` via `SetNodeRegistrar`. Games register custom
  nodes from their `Project_RegisterGameComponents` hook (see "Game node
  libraries" below).
- `EntityComponent/Zenith_GraphReload` — TOOLS-only hot reload (editor Save +
  FileWatcher on `game:Graphs/*.bgraph`), drained at the main loop's safe point.

## Files

- `Zenith_GraphNode.h` — `GraphNodeStatus` (SUCCESS / FAILURE / RUNNING),
  `GraphEventType` (None, OnStart, OnUpdate, OnFixedUpdate, OnEnable, OnDisable,
  OnDestroy, OnCollisionEnter/Stay/Exit, Timer, Custom), `Zenith_GraphContext`
  (`m_xSelf`, `m_fDt`, graph/blackboard pointers, optional
  `m_pxEventPayload`), and the node base class (`Execute`, `GetTypeName`,
  `MatchesCustomEvent`, optional `OnEnter`/`OnExit` — OnEnter fires when a
  chain first reaches the node, not on RUNNING-resume).
- `Zenith_GraphNodeRegistry.{h,cpp}` — `RegisterNodeType<T>(name, eventType,
  outputCount, bFlowNode, category)` derives the create-fn, property table
  (via `ZENITH_PROPERTY`), and type version from the node class; name-keyed;
  duplicate-guarded; registrar inversion keeps this module leaf-safe.
- `Zenith_GraphBlackboard.{h,cpp}` — name → `Zenith_PropertyValue` store with
  typed getters (`GetFloat/GetBool/GetInt32(name, default)` — return the
  default on missing OR type mismatch, never reinterpret), `SetValue`,
  `TryGetValue`, `VisitAll`. Two copy semantics, chosen deliberately:
  `ApplyOverridesFrom` (scene-load: ad-hoc variables restore verbatim, type
  conflicts drop) vs `CopyMatchingFrom` (hot reload: strict name+type match
  only).
- `Zenith_BehaviourGraph.{h,cpp}` — `Zenith_GraphDefinition` (variables, nodes
  with length-framed param blobs, edges, editor positions; magic `XBGR`,
  version 1; UNKNOWN node types preserved verbatim as unresolved nodes — a
  future/missing node never silently drops from the asset) and the runtime
  `Zenith_BehaviourGraph` instance (see "Execution model").
- `Zenith_Scripting.Tests.inl` — execution order, RUNNING suspension/resume,
  flow semantics, blackboard, serialization round-trip, unresolved-node
  preservation, custom events, and the 1000-entity OnUpdate benchmark.

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
- **Chains:** one outgoing edge per (node, pin). A plain node's SUCCESS
  auto-continues from its pin 0; FAILURE aborts the chain; flow nodes
  (`bFlowNode`, e.g. Branch/Loop) drive their own output pins from inside
  `Execute` via `RunChainFromPin`.
- **RUNNING suspension:** a node returning RUNNING stores a chain cursor
  (`m_xChainCursors`, keyed `(anchorID << 8) | pin`) and the chain resumes AT
  that node on the next fire. One-shot anchors (OnStart, collisions, custom
  events, ...) that suspend are re-driven by the ON_UPDATE dispatch until they
  finish; periodic anchors (OnUpdate/OnFixedUpdate/Timer) resume on their own
  next fire.
- **Editor introspection:** `GetRecentlyExecuted()` (cleared per ON_UPDATE,
  capped 64) and the currently-executing node ID feed the editor's live
  execution highlighting.

## Contracts worth knowing

- **Unresolved nodes are preserved, never dropped.** A `.bgraph` containing a
  node type this build doesn't register still loads, runs (chains through the
  missing node fail gracefully, warned once), round-trips on save, and renders
  error-red in the editor. Pinned by unit test and by the windowed DP tests
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
round flow, Marble level flow, Runner run flow, Test shoot, TilePuzzle pinball
ball-lost flow):

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
| Marble | `Games/Marble/Components/Marble_GraphNodes.h` | `Marble_RegisterGraphNodes` |
| Runner | `Games/Runner/Components/Runner_GraphNodes.h` | `Runner_RegisterGraphNodes` |
| Test | `Games/Test/Components/Test_GraphNodes.h` | `Test_RegisterGraphNodes` |
| TilePuzzle | `Games/TilePuzzle/Components/Pinball_GraphNodes.h` | `Pinball_RegisterGraphNodes` |

See each game's CLAUDE.md for its node table and authored graphs.
