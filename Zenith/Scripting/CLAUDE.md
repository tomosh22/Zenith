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
- `Zenith_GraphPinTable.h` — the per-node-class **pin descriptor table**
  (`ZENITH_GRAPH_PINS_BEGIN(Class)` / `ZENITH_GRAPH_PIN_*` / `ZENITH_GRAPH_PINS_END`
  → `GetPinTableStatic()`, concept-detected by `RegisterNodeType` exactly like
  the property table, and inherited the same way). See "Validation" below.
- `Zenith_GraphDefinitionValidator.{h,cpp}` + `.Tests.inl` — the FULL-tier
  static check of a definition against those tables. An ERROR finding FAILS
  `Zenith_GraphBuilder::Build()`.
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
  with length-framed param blobs, exec edges, DATA edges, editor positions; magic
  `XBGR`, **version 2**; UNKNOWN node types preserved verbatim as unresolved
  nodes — a future/missing node never silently drops from the asset) and the
  runtime `Zenith_BehaviourGraph` instance (see "Execution model").
  - **Block order on the wire:** magic, version, variables, nodes, exec edges
    (`src`, `srcPin`, `dst` — three u_ints; a node has ONE exec input, so there
    is no destination pin and `Zenith_GraphEdge` static_asserts its own size),
    data edges (a plain count, then `src`, `srcPin` name, `dst`, `dstPin` name),
    then the length-framed editor-layout block.
  - **Data edges name their pins, never index them** (`Zenith_GraphDataEdge`
    carries two `std::string`s). Names resolve to slots at instantiation, so a
    wire naming a pin no table declares is PRESERVED by the definition rather
    than refused. Authoring: `AddDataEdge(src, outPin, dst, inPin)` /
    `RemoveDataEdge(dst, inPin)` / `FindDataEdgeInto(dst, inPin)` —
    **one incoming wire per INPUT pin, unbounded fan-out from an OUTPUT**, which
    is why every one of them is keyed by the DESTINATION.
  - **Version equality is strict and there is NO version-1 reader.** A v1 payload
    is refused whole. Any previously staged `dist/` package or installed APK
    holds v1 bytes and must be re-packaged / re-installed, as for any schema bump
    (nothing commits `.bgraph` files; every tools boot regenerates them).
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

## Validation (pin descriptor tables + `Zenith_GraphDefinitionValidator`)

**The problem.** Values pass between nodes by NAMED blackboard variables: a node
declares `ZENITH_PROPERTY(std::string, m_str…Var, "…")` and reads or writes
`GetBlackboard()` under that name at runtime. A mistyped name silently yields
the type's default, forever, and `SetValue` creates an undeclared variable by
design — so nothing in the engine could ever see the mistake.

**The mechanism.** A node class declares, once, what each of those name
properties MEANS:

```cpp
ZENITH_GRAPH_PINS_BEGIN(MyNode)
ZENITH_GRAPH_PIN_INPUT(Value,  "m_strValueVar",  PROPERTY_TYPE_FLOAT)
ZENITH_GRAPH_PIN_OUTPUT(Result, "m_strResultVar", PROPERTY_TYPE_FLOAT)
ZENITH_GRAPH_PINS_END
```

`Zenith_GraphPinDesc` carries `m_szName`, `m_eRole`, `m_eType` (a
`Zenith_PropertyType`, or the sentinel `eGRAPH_PIN_TYPE_ANY` ==
`PROPERTY_TYPE_COUNT`), `m_szVarNameProperty` / `m_szConstProperty` (either may
be `""`), `m_szFallbackVarNameProperty` (when the bound var-name property reads
EMPTY the pin binds to THIS property's value instead — the in-place maths-node
form), `m_bInstanceResolved`, and `m_uAcceptedTypeMask` for TARGET_REF.

**Roles.** Only INPUT and OUTPUT ever become drawn wires (Epic B); every role
participates in validation.

| Role | Means |
|---|---|
| `INPUT` | reads a value (var name, inline const, or both) |
| `OUTPUT` | the node's own computed result |
| `SELECTOR_READ` / `_WRITE` / `_READWRITE` | a named-variable REFERENCE that configures the node and stays a validated string forever — never a wire |
| `TARGET_REF` | an entity/position reference, checked against an accepted-type MASK |
| `LIST` | a name in the blackboard's parallel LIST store (not a `Zenith_PropertyValue` at all) |

**Role rulings** (binding on the annotation sweep that follows):

- `SetBlackboard*.m_strVariable` = **SELECTOR_WRITE**; its inline value =
  **INPUT** (const-only).
- `AddBlackboard*` / `LerpBlackboard*` / `ClampBlackboardFloat` /
  `WaitForCondition` variable = **SELECTOR_READWRITE**.
- A node's computed result var = **OUTPUT** — `MathBlackboard*.m_strResultVar`
  with fallback `m_strVar`, `Raycast`'s hit vars, `FindEntity*`'s result.
- Collision sources' `m_strStoreEntityVar` = **SELECTOR_WRITE `ENTITY_ID`**, not
  ANY: the component always supplies a packed EntityID. Only
  `OnCustomEvent.m_strStorePayloadVar` is **SELECTOR_WRITE ANY**.
- `FireCustomEventWithArgs` arg names are written by the FIRER (C++), so no pin
  can name them. They are satisfied by the READING graph **declaring** them
  (`Variable(...)`) — there is no "open writer" escape hatch.
- ★ **TARGET_REF accepts ENTITY_ID only for an ENTITY target.**
  `Zenith_GraphContext::ResolveTargetEntity` accepts a packed ENTITY_ID and
  nothing else — a STRING entity name is never legal at runtime. A polymorphic
  POSITION reference accepts ENTITY_ID **or** VECTOR3. Hence the two macros
  `ZENITH_GRAPH_PIN_TARGET_ENTITY` and `ZENITH_GRAPH_PIN_TARGET_POSITION`.

**★ `ZENITH_GRAPH_PINS_BEGIN` emits `public:` and `..._END` restores `private:`.**
A private `GetPinTableStatic()` makes the concept silently FALSE — the node would
be carefully annotated and completely unvalidated, and nothing would say so. The
paired tag `bZENITH_HAS_PIN_TABLE` makes that a **compile error**: `RegisterNodeType`
static_asserts that a class carrying the tag also exposes a detectable table.
Tables INHERIT like property tables (the collision-source family shares its
base's).

**Instance-resolved types.** A pin declared `_INSTANCE` asks the node via
`Zenith_GraphNode::GetPinType(pinIndex, out)`. A node that DECLINES leaves the
pin ANY plus one `INSTANCE_TYPE_UNRESOLVED` warning — the validator never
fabricates a type.

**What the validator checks.**

- **Structural** — an edge to a node that is not in the graph; an edge from a
  pin at or past the node's effective exec-output count. That count has ONE home,
  `Zenith_GraphNodeRegistry::GetExecOutputCount(definition, nodeID)`: unknown
  type → 1, dynamic-pin type → a param-applied instance's
  `GetDynamicExecOutputCount()` clamped to 255, static type →
  `m_uExecOutputCount + (m_bHasFailurePin ? 1 : 0)`. The editor panel draws and
  validates connections through the same function.
- **Declare-or-error** — every variable a graph READS must be declared or
  written by an annotated writer in the same graph, where "writer" **never
  means the reading node itself**: a READWRITE reference cannot satisfy its own
  read (`SELF_READWRITE`).
- **Type agreement** between a reader and the declaration and every writer. ANY
  unifies with everything; a TARGET_REF checks its mask.
- **Informational** — a declared-but-unreferenced variable, and a LIST name.

**★ OPAQUE NODES.** A node type with NO pin table contributes nothing and reads
nothing as far as the validator can tell. That is the deliberate migration
shape: annotating the node library is a later unit, and an un-annotated node
must never produce a false finding. **While ANY node in a graph is opaque the
declared-but-unreferenced warning is SUPPRESSED for that graph** — otherwise
every `Variable(...)` declaration in every shipped graph would warn.

**★ CAUGHT AT `Build()`, LITERALLY.** `Zenith_GraphBuilder::Build()` runs the
validator after the commit loop (before it the blobs still hold `AddNode`
defaults, not `Param*` values), and **any `GRAPH_VALIDATION_SEVERITY_ERROR`
finding latches `m_bErrors` and makes `Build()` return `false`.** Validation is
the LAST thing `Build()` does and nothing is rolled back, so a false return
still leaves a COMPLETE definition with the findings readable off the builder
(`GetValidationFindingCount()` / `GetValidationFindingAt(i)`) — every negative
test fixture depends on that. There is no way to turn the latch off; the
severity is the rule's, full stop.

Errors: `ORPHAN_EDGE`, `PIN_OUT_OF_RANGE`, `SELF_READWRITE`, `UNDECLARED_READ`,
`TYPE_MISMATCH` (including the TARGET masks). Warnings: `DECLARED_UNUSED`,
`LIST_NAME`, `INSTANCE_TYPE_UNRESOLVED`, `PIN_BINDING_INVALID` — the last stays
a warning deliberately, because a mis-declared pin table is an ENGINE defect the
graph author cannot fix (the per-TU totality tests guard it instead).

*History, one line:* A-5..A-7 ran this report-only (a `bLatchErrors` argument
and an `m_bWouldBeError` flag) while the engine and game node libraries were
annotated and every game's graphs were swept clean; A-8 deleted that tier and
latched it.

**★ A TEST FIXTURE DECLARES WHAT IT READS TOO.** Every var-name property has a
NON-EMPTY default (`"value"`, `"result"`, `"target"`, …), so a builder graph that
merely places a node often reads something. A POSITIVE fixture must
`Variable(...)` it with the reader's type; a NEGATIVE one asserts
`Build() == false`. In a byte-identity fixture (`GraphDefsSerializeIdentically`)
both halves must declare the SAME names, with the SAME types, in the SAME order
— variables serialise FIRST.

**★ GAME NODES ARE ANNOTATED TOO, AND A DECLARATION IS SCENE BYTES.** The sweep
that follows the engine library covers each game's node header (DevilsPlayground
first: 37 var-name properties, `Games/DevilsPlayground/Components/DP_GraphNodes.h`)
under the same rulings, and then closes what annotation cannot: a variable a C++
BRIDGE writes — a shim's `SetValue`, a `FireCustomEventWithArgs` arg — has no
property for any pin to name, so the READING graph must `Variable(...)` it with
the type its readers expect. Two consequences worth knowing before you add one.
First, annotate BEFORE you declare: a finding whose writer turns out to be a game
node in the same graph disappears the moment that node's `OUTPUT` pin exists (the
validator's writer set is graph-wide), and a `Variable(...)` added instead would
be a declaration nothing needed. Second, `Zenith_GraphComponent::WriteToDataStream`
serialises a live slot's WHOLE blackboard, seeded from the definition's declared
variables — so a new declaration changes the `.zscen` of every committed scene
that attaches that graph, and those scenes must be re-authored in the same
commit. Three strings that are NOT blackboard names recur and are worth naming:
tuning KEYS, asset paths / entity names, and any variable a node reaches through
a hard-coded literal or a compile-time constant rather than a property — the last
group is unbindable by construction and is satisfied by declaration alone.

**Where it runs.** Two tiers, one validator.

- **FULL** (`Validate`) — `Build()` (the boot-authoring path) and the editor
  panel on asset open (`OpenAsset` and `OpenAssetFresh`), on a parameter edit,
  and after a connection lands. Needs the node registry.
- **LOAD_SAFETY** (`ValidateLoadSafety`) — run by
  `Zenith_GraphDefinition::ReadFromDataStream` BEFORE anything instantiates the
  definition, and **a finding makes the read return `false` with the definition
  cleared** (the optional out-param hands the findings back, empty on success and
  on a non-tier refusal such as the stream's read-failure flag). It takes NO
  registry: every check is a pure fact about the definition. Scope is
  **crash-class or silently-ambiguous** structure only — two exec edges leaving
  one (node, pin) (the ambiguous one: the walk would silently take the first),
  two data edges entering one (node, pin name), and a malformed data edge (zero
  node id, self-loop, empty pin name). An `ORPHAN_EDGE` is deliberately NOT in
  this tier: it is inert at runtime, so it stays a FULL-tier ERROR and a graph
  carrying one still loads. `Validate` runs the load-safety checks first and
  appends their findings, so one report covers both tiers.
  - Two more checks belong here and are not written yet, each blocked on
    machinery a later unit adds: **pure-data cycles** among resolved nodes (B-2)
    and **static-vs-static type mismatch across a wire** (B-3).

**The log line** — `LOG_CATEGORY_CORE`; an ERROR goes to `Zenith_Error` and a
WARNING to `Zenith_Log`, and the summary line follows the errors count. The
`<SEV>` token carries the severity (there is no separate flag any more):

```
[GraphValidator] <WARN|ERROR> graph=<name> node=<id>:<type> pin=<pin|-> var=<var|-> rule=<RULE> | <text>
[GraphValidator] graph=<name> nodes=N findings=<errors>/<warnings>
```

`RULE` is one of `UNDECLARED_READ`, `TYPE_MISMATCH`, `PIN_OUT_OF_RANGE`,
`ORPHAN_EDGE`, `DECLARED_UNUSED`, `LIST_NAME`, `SELF_READWRITE`,
`INSTANCE_TYPE_UNRESOLVED`, `PIN_BINDING_INVALID`, and the three LOAD_SAFETY
rules `DUPLICATE_EXEC_SOURCE`, `DUPLICATE_DATA_INPUT`, `DATA_EDGE_MALFORMED`
(all ERROR). `PIN_BINDING_INVALID` fires when a
pin names a property the class does not declare as a string — a mis-declared
table is a warning, never a `DebugBreak`; the tagged property getters ASSERT, so
the tag is checked before `GetString`. The `[GraphValidator]` prefix appears on
no other log line in the repo, so the whole report is one `Select-String` over
`<exe dir>/Logs/zenith_*.log`.

**Applying a param blob has one home too:**
`Zenith_GraphDefinition::ApplyNodeParams(nodeID, node, typeInfo)`. Graph
instantiation, the editor's param panel, the exec-output funnel and the
validator all call it; it used to be three inline copies of the same
`Zenith_DataStream` + `ReadProperties` pair, and the validator would have been
the fourth.

## Contracts worth knowing

- **Unresolved nodes are preserved, never dropped.** A `.bgraph` containing a
  node type this build doesn't register still loads, runs (chains through the
  missing node fail gracefully, warned once per node — `NodeInstance` carries an
  `m_bWarnedUnresolved` latch so a hot chain doesn't spam the log), round-trips
  on save, and renders error-red in the editor. Pinned by unit test and by the windowed DP tests
  `Test_GraphEditorLiveAuthoring` / `Test_GraphEditorScreenshotTour`.
- **A data wire is keyed by its DESTINATION.** One incoming wire per input pin,
  unbounded fan-out from an output pin — enforced by `AddDataEdge` at authoring
  and by the LOAD_SAFETY tier at read, so no code downstream has to cope with two
  values arriving at one input. `RemoveNode` drops every touching wire, exec and
  data alike.
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
