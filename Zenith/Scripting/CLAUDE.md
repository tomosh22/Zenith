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
  (magic-tagged, versioned, registered with `Zenith_AssetRegistry`).
- `EntityComponent/Components/Zenith_GraphComponent` — the host component
  (slot list, blackboard overrides, lifecycle/collision dispatch, registered
  at meta order 60).
- `EntityComponent/Zenith_GraphNode_Registration.cpp` — the engine node library
  (~24 nodes), installed by `Zenith_Engine::Initialise` via `SetNodeRegistrar`.
  Games register custom nodes from their `Project_RegisterGameComponents` hook.
- `EntityComponent/Zenith_GraphReload` — FileWatcher-driven hot reload, drained
  at the main loop's safe point.

## Files

- `Zenith_GraphNode.h` — `GraphNodeStatus` (SUCCESS/FAILURE/RUNNING),
  `GraphEventType` (OnStart/OnUpdate/OnFixedUpdate/OnEnable/OnDisable/OnDestroy/
  OnCollisionEnter/Stay/Exit/Timer/Custom), `Zenith_GraphContext`
  (`m_xSelf`, `m_fDt`, graph/blackboard pointers, optional event payload), and
  the node base class (Execute / GetTypeName / MatchesCustomEvent).
- `Zenith_GraphNodeRegistry.{h,cpp}` — `RegisterNodeType<T>` derives the
  create-fn, property table (via `ZENITH_PROPERTY`), and `uTYPE_VERSION` from
  the node class; name-keyed; registrar inversion keeps this module leaf-safe.
- `Zenith_GraphBlackboard.{h,cpp}` — name → `Zenith_PropertyValue` store.
  Two copy semantics, chosen deliberately: `ApplyOverridesFrom` (scene-load:
  ad-hoc variables restore verbatim, type conflicts drop) vs `CopyMatchingFrom`
  (hot reload: strict name+type match only — values are never reinterpreted).
- `Zenith_BehaviourGraph.{h,cpp}` — `Zenith_GraphDefinition` (nodes, edges,
  editor positions; serialization preserves UNKNOWN node types verbatim as
  unresolved nodes — a future/missing node never silently drops from the asset)
  and the runtime `Zenith_BehaviourGraph` (event-driven exec-chain interpreter:
  only fired event sources tick their chains; one outgoing edge per (node,pin);
  flow nodes run sub-chains via RunChainFromPin; `GetRecentlyExecuted` feeds the
  editor's live highlight; `GetUnresolvedCount` reports unresolved nodes).
- `Zenith_Scripting.Tests.inl` — execution order, flow semantics, blackboard,
  serialization round-trip, unresolved-node preservation, custom events, and
  the 1000-entity OnUpdate benchmark.

## Contracts worth knowing

- **Unresolved nodes are preserved, never dropped.** A `.bgraph` containing a
  node type this build doesn't register still loads, runs (chains through the
  missing node fail gracefully), round-trips on save, and renders error-red in
  the editor. Pinned by unit test and by the windowed
  `Test_GraphEditorScreenshotTour` in the DP suite.
- **Hot reload happens only at the main loop's safe point** (never mid-dispatch),
  is atomic (a failed parse keeps the old graph live), and migrates blackboard
  state name+type-matched.
- **No std::function anywhere** — node creation, property access, and dispatch
  are all function pointers, per engine convention.
- Graphs are strictly main-thread, dispatched through `Zenith_GraphComponent`'s
  snapshot-iteration discipline.
