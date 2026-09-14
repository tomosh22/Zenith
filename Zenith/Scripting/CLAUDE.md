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
  (**140 nodes** — count them with
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
  It also owns the **per-instance pin state and the pin accessors** —
  `GetInput<T>` / `GetInputPackedEntityID` / `TryGetInput` / `SetOutput`, the
  test seam, and `GetDynamicDataInputCount` (the DATA sibling of
  `GetDynamicExecOutputCount`). See "Pin runtime" below.
- `Zenith_GraphNodeRegistry.{h,cpp}` — `RegisterNodeType<T>(name, eventType,
  outputCount, bFlowNode, category, bHasFailurePin = false, bPureNode = false)` derives the
  create-fn, property table (via `ZENITH_PROPERTY`), and type version from the
  node class; name-keyed; duplicate-guarded; registrar inversion keeps this
  module leaf-safe. `bHasFailurePin` opts the type into the routable
  **On Failure** exec pin (see "Routable failure" below) and is VALIDATED at
  registration — a refused flag is a `Zenith_Error` plus a forced `false`, never
  an assert, so the refusal is something a test (and the editor) can read back.
  `bPureNode` is validated the same observable way — see "Pin runtime" below.
- `Zenith_GraphPinTable.h` — the per-node-class **pin descriptor table**
  (`ZENITH_GRAPH_PINS_BEGIN(Class)` / `ZENITH_GRAPH_PIN_*` / `ZENITH_GRAPH_PINS_END`
  → `GetPinTableStatic()`, concept-detected by `RegisterNodeType` exactly like
  the property table, and inherited the same way). See "Validation" below. It
  also carries the two helpers BOTH the validator and the runtime resolve
  through, so what was checked and what was bound cannot drift:
  `Zenith_GraphPin_ReadStringProperty(table, node, property, out)` (the
  tag-checked var-name read; `NOT_BOUND` / `OK` / `INVALID`) and
  `Zenith_GraphPin_MakeZeroValue(type)` (a TYPE-STAMPED zero — a
  `Zenith_PropertyValue` default-constructs FLOAT-tagged, and `eGRAPH_PIN_TYPE_ANY`
  has no zero at all).
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
  The `PinRuntime_*` block covers the whole pin runtime: the exact var-name
  fallback, the wire winning over it, typed-zero vs UNSET slots, checked
  extraction, the dual-write and its absence, the `TryGetInput` truth table row
  by row, pure evaluation / memoisation / cycles / non-SUCCESS, the gather token
  (including a flow node's nested sub-chain), the four resolution refusals with
  their positive controls, variadic ordinals, and the two registration blocks
  (`Registry_PureFlagRefused…`, `Registry_PureForcesZeroExecOutputs`). ★ Three
  mutually distinct values run through every one of them — const `9.0f`,
  blackboard `7.0f`, producer slot `5.0f` — so an assertion can only pass for one
  reason. Fixtures are authored with `AddNode`/`AddEdge`/`AddDataEdge` directly,
  NOT `Zenith_GraphBuilder`: the EXISTING scratch types default their var-name
  properties to non-empty names (`"value"`, `"count"`, …), so a builder graph
  that merely places one reads something and `Build()` latches `UNDECLARED_READ`.
  (The B-2 scratch types deliberately default theirs to `""` — but a fixture
  mixes both.)

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

### Pin runtime (what makes a data wire carry a value)

A `Zenith_GraphDataEdge` stores pin NAMES; `InitialiseFromDefinition` resolves
them to per-instance SLOTS, and a node reads and writes them through accessors
on `Zenith_GraphNode` rather than through the blackboard.

**The accessors.** `GetInput<T>(ctx, pin)` (plus `(ctx, pin, ordinal)` for a
variadic family), `GetInputPackedEntityID(ctx, pin)` — the non-template form,
because `Zenith_PropertyTraits` has no `u_int64` specialisation — `TryGetInput`,
and `SetOutput(ctx, pin, value)` / `SetOutput<T>`. `pin` is the pin's INDEX in
the class's pin table.

**★ NOTHING ON AN ACCESSOR PATH CAN REACH `Zenith_Assert`.** Every one of them
bounds-checks itself before indexing (`Zenith_Vector::Get` and
`Zenith_GraphPinTable::GetPinAt` both assert): a pin index past the table, a role
that is not INPUT (resp. OUTPUT), an OPAQUE node or an unresolved temp instance
(empty arrays), or a context with a null graph/blackboard yields the pin DEFAULT
from `GetInput`, `false` from `TryGetInput` and a no-op from `SetOutput`, plus at
most ONE `[GraphPin] BADACCESS` line per instance. `RenderTest`'s tennis contract
already builds a context with a null graph and calls `Execute` directly.

**★ The templates name no `Zenith_BehaviourGraph` member.** `Zenith_GraphNode.h`
only forward-declares the graph and the include cannot be reversed, so
`GetInput<T>` / `SetOutput<T>` are thin inline wrappers over NON-template
out-of-line members (`ResolveInput`, `MakePinDefault`, `SetOutput`) defined in
`Zenith_BehaviourGraph.cpp`. Node TUs include nothing new.

**The pin DEFAULT has one definition and one helper** (`MakePinDefault`): the
const property's CURRENT value when the descriptor declares one, else the type's
zero. Output slots initialise through the same rule.

**Reading an input.**

| shape | `GetInput<T>` |
|---|---|
| test override set | checked-extract it (mismatch path included) |
| connected, producer's slot tag == T | the value |
| connected, tag != T | the pin default + ONE `[GraphPin] MISMATCH` per (instance, pin) — never an assert, never a `Zenith_Error`: it is a DATA problem the validator reports at author time (B-3) |
| connected, slot UNSET | the pin default |
| unconnected, var name bound | the blackboard variable, defaulting to the pin default on a missing name OR a tag mismatch — **this IS today's `bb->GetFloat(var, const)`, exactly** |
| unconnected, no var name | the pin default |

**`TryGetInput` is PRESENCE-aware**, for a wildcard (ANY) consumer that owns its
own tag check. `false` means "there is no value here", never "the wrong type":

| shape | result |
|---|---|
| connected, slot set, tag matches | true, a pointer INTO the producer's slot (valid until that producer's next `SetOutput`) |
| connected, slot set, tag MISMATCH | **true, with the raw value** — the consumer owns the check |
| connected, slot UNSET | false |
| test override | true, a pointer to the override |
| unconnected, var bound, present | true, a pointer into the blackboard (valid until the next `SetValue`) |
| unconnected, var bound, absent | false |
| unconnected, no var, const property | true — a const IS a value; the pointer is to a per-binding scratch refreshed on each call |
| unconnected, no var, no const | false |
| bad access (above) | false |

**Writing an output.** `SetOutput` latches the per-instance slot (`m_bSet`).
A tag that is not the slot's declared/resolved type leaves the slot UNCHANGED
plus ONE `[GraphPin] OUTMISMATCH` per (instance, pin) — a node writing the wrong
type is an ENGINE defect, and re-tagging a slot under a consumer's feet would be
worse than refusing. An ANY slot accepts any tag by definition.

**Slot initialisation.** A TYPED output slot starts at its stamped zero and is
already SET — a consumer that reads before the producer runs gets `0`, not an
assert. An ANY slot, and an instance-resolved slot whose node DECLINED to answer
`GetPinType`, start **UNSET**: there is no zero to stamp, and "the producer has
not written yet" is exactly what a wildcard needs to be able to say.

**★ TWO TRANSITIONAL PATHS, BOTH DELETED IN C-1**, and nothing else in this unit
is one: the var-name fallback in `ResolveInput` (row 5 above) and the
**dual-write** in `SetOutput` — while a descriptor still binds a var name and it
reads non-empty, a latched value ALSO goes to the blackboard, so an unmigrated
downstream reader still finds it exactly where it does today. The fallback logs
one `[GraphPin] FALLBACK node=… pin=… var=…` line per (instance, pin) on first
use — **from `TryGetInput` as well as `GetInput`**, or a node migrated onto the
presence-aware accessor would be invisible to the census — and only a MIGRATED
node can reach it: **a boot log with zero `FALLBACK` lines is C-1's
precondition**, which is why the line exists at all
(`GetFallbackUseCountForTest` is the unit-visible half).

**★ "Zero FALLBACK lines" means zero in a SUITE BOOT log, not in a unit run.**
The per-game census parses `zenith test <G> --headless` runs, and those pass
`--skip-unit-tests` — so the unit batch's own deliberate fallback fixtures never
reach a counted log, and a non-zero count in one is not a census regression. What
the census counts is authored content: a binding logs `FALLBACK` only when its
var-name property reads NON-EMPTY, so the figure is "how many placed node
instances still name a blackboard variable instead of carrying a wire".

### B-7.6 corpus readiness

The ordinary `PinRuntime_*` corpus uses resolved data edges, typed slot defaults,
or `SetInputForTest`; its output observations use `GetOutputForTest`. Resolution
refusal rows retain their independent value assertion through an explicit test
wire, and reload coverage seeds persistent blackboard state directly rather than
using output publication as an implicit write.

The following tests are the intentional transitional witnesses kept until C-1:
`PinRuntime_UnconnectedReadsVarNameFallbackExactly`,
`PinRuntime_SetOutputLatchesAndDualWritesWhileVarBound`,
`PinRuntime_SetOutputDoesNotWriteBlackboardWhenVarEmpty`,
`PinRuntime_OutputFallbackVarNameBinds`,
`PinRuntime_TryGetInputTrueForVarPresent`,
`PinRuntime_TryGetInputFalseForVarAbsent`, and
`PinRuntime_FallbackUseLoggedOncePerPin` (which covers both `GetInput` and
`TryGetInput`). The validator keeps
`Validator_FallbackVarNameBinds` and `Validator_InPlaceAliasingIsWarning` as
separate negative controls. Lazy self-binding remains permanent behavior and
uses an unconnected first access followed by a typed test wire; its ordinary
output assertion is slot-only.

The first seven-suite automated census passed with zero FALLBACK,
IN_PLACE_ALIASING, and validator errors. Raw Null unit registrations were Combat
2708, Zenithmon 4561, and RenderTest 2810. This inventory remains a C-1
readiness record; the final 14-step T3 (with builds), SceneGuard, three D3D
link proofs, and doc-lint all passed.

**SELF-BINDING — a directly-constructed node binds its own pins (B-6.1).**

**★ PERMANENT RUNTIME BEHAVIOUR, NOT A THIRD TRANSITIONAL PATH.** Per-instance
bindings are normally built by `Zenith_BehaviourGraph::BuildPinState` at
`InitialiseFromDefinition`. A node constructed **directly** — `MyNode xNode;`,
properties assigned, `Execute(bareContext)`, which is how ~29 standalone node unit
tests drive the library — has no graph, so before B-6.1 every accessor on one took
the bad-access path: `GetInput` returned the type's ZERO rather than the const
property, and `SetOutput` was a no-op.

`GetInput*` / `TryGetInput` / `SetOutput` now call `EnsurePinState()` first. If
nothing has built the state **and** the class declares a pin table
(`GetPinTableVirtual()`, emitted by `ZENITH_GRAPH_PINS_BEGIN` alongside
`GetPropertyTableVirtual()`), the node builds it from its own tables once. It then
behaves **exactly like an UNWIRED graph node**: the var-name fallback with the
const as the default, and the dual-write on `SetOutput`.

- **One builder, `Zenith_GraphNode::BuildPinStateFromTables`,** for both callers.
  It **CLEARS** `m_axInputs` / `m_axOutputs` / `m_axVariadicInputs` on entry — it
  used to only `Reserve` + `PushBack`, so a second build would APPEND and leave
  pins `0..N-1` addressing stale bindings.
- **The graph path always wins,** and cannot lose a race: the graph owns its
  instances from creation, so self-binding can never have run first there. The
  graph supplies the three things only it knows — the REGISTRY's property table
  (an inheriting family's virtual answers the pin-table OWNER's table), the
  DEFINITION, and the type's `m_bVariadicNameCollision`. Self-binding passes the
  virtual, **no definition** (so a from-variable OUTPUT slot stays ANY, and
  therefore UNSET) and no collision flag.
- **`EnsurePinState` runs ONLY from the three accessors an `Execute` calls.** Not
  from `SetInputForTest`, `GetOutputForTest`, `GetOutputPinType` or the counter
  getters: the latch reads property-derived state, and a `SetInputForTest` before
  an op-code assignment would stamp the wrong slot type. The const accessors answer
  from an unbuilt state (`ANY` / null) and stay const.
- **ORDERING RULE (a Don't).** Assign EVERY property before the first `Execute` on
  a directly-constructed node. Pin state is built once and never refreshed from a
  later property write, so a test that changes an instance-resolved op code needs a
  **FRESH node** — a reused one keeps its first stamp and its second write is
  REFUSED with one `OUTMISMATCH` line. `Zenith_GraphDefinition::ApplyNodeParams`
  clears the built flag on the instance it configures, which is the one sanctioned
  way to re-derive.
- **The BAD-ACCESS path survives for, and only for:** an OPAQUE node (no pin table
  at all, so the virtual answers null), an out-of-range or wrong-role pin, a
  CONNECTED pin read through a context with a null `m_pxGraph`, and a var-bound pin
  read through a context with a null `m_pxBlackboard`.
- **Temp instances still cost nothing.** The registry's dynamic-pin probe,
  `GetExecOutputCount`, the validator, the builder, `AddNode` and the editor's
  param panel never call an accessor, so the zero-capacity invariant holds for all
  of them. A future caller that DOES touch one pays exactly one build.
- **What this means for C-1.** After the two transitional paths are deleted, a
  directly-constructed node reads its const default (or the type zero) and writes
  only its slot — so the standalone node tests that today depend on the var-name
  fallback and the dual-write are rewritten onto
  `SetInputForTest`/`GetOutputForTest` in B-7.6, as planned. Self-binding is what
  keeps them meaningful in between, and is NOT deleted with them.

**Pure nodes (`m_bPureNode`).** A pure node has NO exec pins: it evaluates on
demand when a consumer gathers an input wired to one of its OUTPUT pins.

- **Memoised within ONE gather.** A gather TOKEN is minted and captured around
  every non-pure `Execute` the graph performs (`RunChainFromPin`'s loop,
  `RunSourceNode`'s gate, `RunGraphCall`'s gate) and restored afterwards, so
  "once per consumer `Execute`" is literal rather than a global counter read at
  pull time. Tokens only ever increase and a memo hits on `stamp >= current`,
  which is what lets a FLOW node re-use a source its own sub-chain already
  evaluated: pull, run the sub-chain (whose consumer pulls the same source), pull
  again = TWO evaluations, not three. One pure node feeding two pins of one
  consumer = one; two consumers = two; a new fire = a new token. ★ That is a
  STALENESS rule as well as a saving: a pure source is NOT re-read after a
  sub-chain that mutated its inputs, so a flow node whose branch changes a
  blackboard variable its pure source reads still sees the pre-branch value on
  its second pull. Both counters are 64-bit, so the `>=` comparison cannot wrap
  a node into a permanently stale memo.
- **No lifecycle.** `OnEnter`/`OnExit`/`OnAbort` are never called on a pure node,
  and **an exec edge whose DESTINATION is a pure type is DROPPED at
  instantiation** with one `[GraphPin]` line (the chain simply ends there). B-3
  makes that an author-time error. A pure evaluation does not move
  `GetExecutingNodeID()`.
- **A PULLED pure node IS in `GetRecentlyExecuted()` (B-4).** `PullSlot` pushes
  the source's id on an evaluation **and on a memo hit** — it is feeding this
  frame's consumers either way — DEDUPLICATED (one pure source commonly feeds
  many consumers in one frame, and the reader scans linearly), under the same
  cap of 64, and ungated exactly like `RunChainFromPin`'s push. The CYCLE and
  non-SUCCESS paths return before it: those pulls yield the consumer's default,
  so nothing of that node reached anybody. **A pulled producer therefore lands
  AFTER its consumer in the trace** — the pull happens from inside the consumer's
  `Execute`, which was pushed first. The trace is "what ran", never a topological
  order. This is what makes the editor light a pulled node up like an executed
  one (`Zenith_GraphEditorPanel::IsNodeHighlightedForTest`); unit
  `PinRuntime_PulledPureNodeInRecentlyExecutedOnce`.
- **A non-SUCCESS pure `Execute` means the slot is NOT read** — the consumer
  takes its default plus one `[GraphPin] STATUS` per instance. RUNNING is refused
  the same way: there is no cursor to suspend on.
- **A runtime data CYCLE** (a pure node pulled while it is already evaluating)
  yields the consumer's pin default plus ONE `[GraphPin] CYCLE` per INSTANCE — a
  node with two cyclic inputs names one of them. Never a hang, never an assert.
- **Registration REFUSES the flag observably** (`Zenith_Error` + forced `false`,
  the `m_bHasFailurePin` pattern) on a flow node, an event source, a type that
  also asked for a failure pin (which IS an exec pin), a type with no create fn,
  a type with NO pin table or no OUTPUT pin in it (nothing could ever pull it),
  and a dynamic-exec-pin type. Only a SURVIVING flag forces `m_uExecOutputCount`
  to 0 — a refusal leaves the count alone, because zeroing a flow node's branches
  would be a silent deletion. A pure type's effective exec-output count is
  therefore 0, so **an exec edge OUT of a pure node is `PIN_OUT_OF_RANGE` by
  construction.**

**Variadic INPUT families.** `ZENITH_GRAPH_PIN_INPUT_VARIADIC(Family, Type)`
declares an ordinal family; the member count comes from the PARAM-APPLIED
instance's `GetDynamicDataInputCount()`, and a wire names a member
`"<family><ordinal>"` (`in0`, `in1`, …). Resolution tries an EXACT pin name
first, then splits a trailing decimal ordinal whose prefix must exactly name a
variadic descriptor and whose ordinal must be inside the count (itself clamped to
255, like the exec-pin count). A table that
declares both a family and a literal pin named family+digits is reported at
registration (`m_bVariadicNameCollision`) and its family is never expanded — the
literal would silently win and the member would be unreachable forever.

**Resolution refusals.** A data edge naming an unresolved node (either end), an
OPAQUE endpoint, an unknown pin name, a BARE variadic family name (`"in"` rather
than `"in0"` — a family has no non-ordinal member, so the wire would sit on a
binding no accessor can address), an ordinal past the family, or a name that
resolves to the wrong ROLE is **skipped with one `[GraphPin]` line and left in
the DEFINITION** — a build that merely lacks a node version must still round-trip
the asset. `Zenith_BehaviourGraph::GetResolutionSkipCountForTest()` is the
observable, so a unit can prove the malformed wire was refused rather than merely
prove some other pin still worked.

**★ THE ASYMMETRY ACROSS A HOT RELOAD.** A blackboard variable SURVIVES
(`CopyMatchingFrom` carries it name+type-matched); an output SLOT does not. The
reload builds a NEW `Zenith_BehaviourGraph` and every instance is re-created from
the definition, so every slot is back at its stamped default. Nothing about a
slot is persistent state.

**The log line** — `Zenith_Log(LOG_CATEGORY_CORE, "[GraphPin] …")`, a prefix that
appears on no other line in the repo, so the whole census is one `Select-String`
over `<exe dir>/Logs/zenith_*.log`. Tokens: `MISMATCH`, `OUTMISMATCH`, `CYCLE`,
`STATUS`, `BADACCESS`, `FALLBACK`, plus the resolution-skip lines. Every one of
them is once-per-instance (or once per instance+pin) — a hot chain must never
spam a log.

**Pulls are legal ONLY from `Execute`.** `OnEnter`/`OnExit`/`OnAbort` run outside
any gather token, so a pull from one would evaluate a pure source with no memo
and stamp nothing.

**The test seam is a SEAM, not a transitional path** (nothing here is deleted by
C-1): `SetInputForTest(pin[, ordinal], value)` behaves exactly like a connected
wire carrying that value, mismatch path included; `GetOutputForTest(pin)` reads
the slot (null = UNSET); and `GetMismatchWarningCountForTest` /
`GetOutputMismatchWarningCountForTest` / `GetCycleWarningCountForTest` /
`GetPureStatusWarningCountForTest` / `GetBadAccessWarningCountForTest` /
`GetFallbackUseCountForTest` are COUNTERS incremented at the `Zenith_Log` call
site — so "warned exactly once" is a unit assertion rather than a log scrape.

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
form), `m_bInstanceResolved`, `m_szTypeFromVarNameProperty` (below), and
`m_uAcceptedTypeMask` for TARGET_REF.

**★ A pin's type has THREE sources, and only one of them is the graph.** The
CLASS answers statically (`m_eType`); the INSTANCE answers per configured node
(`m_bInstanceResolved` → `GetPinType`); and the GRAPH answers through
`ZENITH_GRAPH_PIN_OUTPUT_FROM_VARIABLE(Pin, "m_strSomeVar")`, which types the pin
as the **DECLARED** type of the variable that property's value names —
`GetVariable`'s output, and the only pin form whose type is a fact about the
graph rather than about the code. An **undeclared** variable leaves it ANY and
earns no finding of its own: the `SELECTOR_READ` naming the same property is
already reported by declare-or-error, and one mistake deserves one finding. A
writer-only variable is not a type source either — **the declaration is the
contract**.

**★ The from-variable form is a TYPE source and NOTHING else** — never a
binding, never a writer, never dual-written. Its descriptor leaves
`m_szVarNameProperty`, `m_szConstProperty` and `m_szFallbackVarNameProperty` all
`""` deliberately: re-using the var-name binding would make a node that merely
READS a variable an annotated **WRITER** of it, which would silently satisfy
every other reader's declare-or-error and make the node dual-write the variable
to itself. `Zenith_BehaviourGraph::BuildPinState` resolves the runtime slot off
the **declaration** for the same reason an `ApplyOverridesFrom` override could
otherwise retype a slot the validator already checked. The totality harness
treats `m_szTypeFromVarNameProperty` as a fourth named-property slot, because a
typo in it resolves to ANY **silently**.

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
- **WIRES** (pass 1b — see the next section).
- **DOMINANCE** — a consumer that can run before its producer (warning).
- **Informational** — a declared-but-unreferenced variable, a LIST name, and an
  in-place OUTPUT alias.

**★ OPAQUE NODES.** A node type with NO pin table contributes nothing and reads
nothing as far as the validator can tell. That is the deliberate migration
shape: annotating the node library is a later unit, and an un-annotated node
must never produce a false finding. **While ANY node in a graph is opaque the
declared-but-unreferenced warning is SUPPRESSED for that graph** — otherwise
every `Variable(...)` declaration in every shipped graph would warn.

**★ …EXCEPT ON A WIRE, AND THAT SUPERSEDES THE DOCTRINE FOR WIRES ONLY.** A DATA
edge whose endpoint type is REGISTERED but declares no pin table is an **ERROR**
(`WIRE_PIN_UNKNOWN`). The doctrine above protects the annotation MIGRATION from
*false* findings, and this is not one: `ResolveDataEdges` SKIPS exactly that
wire at instantiation, so it can never carry a value. An **UNREGISTERED**
endpoint stays silent (a per-game node library this exe does not carry — the
instantiation warning covers it), and exec chains through opaque nodes are
unreported exactly as before. The consequence is real and deliberate: a
deliberately-opaque node type cannot be WIRED until it gets pins.

### The wire pass (pass 1b)

Every data edge is resolved to a real pin, of the right role, on a real node,
with agreeing endpoint types. **At most ONE finding per edge** — the first
defect is reported and the edge abandoned — so "one finding per defect" holds
and a test can assert an exact count.

| Check | Rule |
|---|---|
| an endpoint node is not in the graph | `ORPHAN_EDGE` (reused) |
| an endpoint type is UNREGISTERED here | *skipped silently* |
| an endpoint type is registered and OPAQUE | `WIRE_PIN_UNKNOWN` |
| the source name is not a pin / the destination name is not a pin, is a BARE variadic family, or is an ordinal past the configured member count | `WIRE_PIN_UNKNOWN` |
| a real pin of the wrong ROLE (source must be OUTPUT, destination INPUT) | `WIRE_ROLE_MISMATCH` |
| two RESOLVED types that differ, neither ANY | `TYPE_MISMATCH` |
| an exec edge INTO a pure node | `EXEC_INTO_PURE` |
| a pure node with no outgoing wire | `PURE_UNCONSUMED` (warning) |

Destination names follow B-2's algorithm exactly: an exact pin name first, then
`"<family><ordinal>"` against a variadic family whose member count comes from the
param-applied instance.

- **There is no rule for an exec edge OUT of a pure node.** A surviving PURE flag
  forces the type's exec-output count to 0, so the existing range check already
  reports it as `PIN_OUT_OF_RANGE`. A second rule would be a second finding for
  one mistake.
- **There is deliberately NO warning for an ANY output feeding a typed input.**
  "ANY unifies" is exact. The residual value-dependent case — an ANY producer
  that happens to write the wrong tag at runtime — is B-2's `[GraphPin] MISMATCH`
  census, which is where it can actually be observed.
- **A wire supersedes the fallback the C-1 sweep deletes.** When pass 1b resolved
  BOTH endpoints and BOTH pins — exactly the conditions under which the runtime
  sets `m_bConnected` — the destination input is recorded as WIRED and pass 3
  skips its var-name checks entirely: a connected input never consults its var
  name (`ResolveInput` takes the pull path), so the name is not a read. In the
  silent-skip case the record is deliberately NOT taken, because the runtime
  *will* fall back to the var name there.
- **No double-counting.** A STATIC-vs-STATIC type disagreement is owned by the
  LOAD_SAFETY body, which `Validate` runs first; pass 1b reports a type mismatch
  only when at least one endpoint needed PARAMS or a DECLARATION to resolve
  (instance-resolved or from-variable).
- **`IN_PLACE_ALIASING`** (warning) fires on an OUTPUT whose primary var-name
  property reads EMPTY and whose FALLBACK reads non-empty — the two Math nodes'
  in-place form, where the result is written back over its own source. B-6.1
  records the per-game count; C-1 requires zero.

### Data cycles and dominance

- **`DATA_CYCLE`** (error) is a DFS over data edges **restricted to edges whose
  SOURCE is a pure node**. ★ **An impure producer BREAKS a cycle by design**: its
  slot is LATCHED by its own `Execute`, so a consumer pulling it reads a previous
  run's value rather than re-entering it. One finding per run, naming the ring.
- **`DOMINANCE`** (warning, never an error) follows an impure producer's data
  edges THROUGH pure relays to every executable consumer, then asks: with the
  producer removed from the exec graph, is the consumer still reachable from an
  event source? If so the wire may read the slot's default. **Two named blind
  spots:** it models neither `Sequence`'s branch ORDER nor reactive preemption,
  either of which can make a "reachable" consumer unreachable in practice — which
  is exactly why a false positive here must never fail a `Build()`. Bounded at
  ONE finding per PRODUCER (naming the first consumer and the count), with the
  event-source roots found once per `Validate`. Two degenerate cases, both
  deliberate: a consumer that is itself an event source ALWAYS warns
  (conservative), and a producer that is an event source is SKIPPED — what it
  publishes is a dispatch payload, not a latched pin.

### `ResolvePinType` — one resolver, exported

`Zenith_GraphDefinitionValidator::ResolvePinType(def, registry, nodeID, pinIndex,
out)` is the single answer to "what type is this pin": static, the
param-applied instance's `GetPinType`, or the declared variable's type. `false`
means the node/pin does not exist or the type is unregistered; `out` may
legitimately be ANY. Pass 0 uses it, B-4's editor colours and labels pins through
it, and `BuildPinState` stamps the runtime slot with the same answer — a unit
asserts the two agree for all three forms.

★ **It ALLOCATES** (one temp instance per instance-resolved or from-variable
query) and is a QUERY, not a draw-time accessor. A caller that draws pins **must
cache the per-node answer** and invalidate on a definition edit, a param edit or
a reload.

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
`TYPE_MISMATCH` (including the TARGET masks), `WIRE_PIN_UNKNOWN`,
`WIRE_ROLE_MISMATCH`, `EXEC_INTO_PURE`, `DATA_CYCLE`. Warnings:
`DECLARED_UNUSED`, `LIST_NAME`, `INSTANCE_TYPE_UNRESOLVED`, `DOMINANCE`,
`PURE_UNCONSUMED`, `IN_PLACE_ALIASING`, `PIN_BINDING_INVALID` — the last stays
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
  on a non-tier refusal such as the stream's read-failure flag). Scope is
  **crash-class or silently-ambiguous** structure only — two exec edges leaving
  one (node, pin) (the ambiguous one: the walk would silently take the first),
  two data edges entering one (node, pin name), a malformed data edge (zero
  node id, self-loop, empty pin name), a **pure-sourced data cycle**, and a
  **static-vs-static type mismatch across a wire**. An `ORPHAN_EDGE` is
  deliberately NOT in this tier: it is inert at runtime, so it stays a FULL-tier
  ERROR and a graph carrying one still loads. `Validate` runs the load-safety
  checks first and appends their findings, so one report covers both tiers —
  and that is why pass 1b never re-reports the static wire mismatch.
  - ★ **It TAKES THE REGISTRY now, so the tier's answer depends on the REGISTERED
    NODE SET.** The last two checks cannot be pure facts about the definition —
    one needs `m_bPureNode`, the other needs a pin NAME resolved through a pin
    table. `ReadFromDataStream` therefore calls `EnsureInitialized()` and ALWAYS
    passes the registry, so the answer is **deterministic** rather than
    boot-phase dependent (an optional registry would make the same asset accepted
    or refused depending on when it was read). A build with no registrar — the
    Sentinel link proofs — gets an initialised-but-EMPTY registry: nothing
    resolves, both checks are no-ops, and the tier is exactly B-1's.
  - ★ **The two test windows that swap or empty the registry must not read a
    `.bgraph` inside the window** — `Zenith_GraphPinTotality_SwapToRegistrar` and
    `RunNodeProvenanceChecks`' `ResetForTests`. Neither does today.
  - **A refused load is `LoadedOk() == false`, i.e. "no graph" for every
    consumer** — the accepted outcome, and the same one a bad-magic payload
    produces.

**The log line** — `LOG_CATEGORY_CORE`; an ERROR goes to `Zenith_Error` and a
WARNING to `Zenith_Log`, and the summary line follows the errors count. The
`<SEV>` token carries the severity (there is no separate flag any more):

```
[GraphValidator] <WARN|ERROR> graph=<name> node=<id>:<type> pin=<pin|-> var=<var|-> rule=<RULE> | <text>
[GraphValidator] graph=<name> nodes=N findings=<errors>/<warnings>
```

`RULE` is one of `UNDECLARED_READ`, `TYPE_MISMATCH`, `PIN_OUT_OF_RANGE`,
`ORPHAN_EDGE`, `DECLARED_UNUSED`, `LIST_NAME`, `SELF_READWRITE`,
`INSTANCE_TYPE_UNRESOLVED`, `PIN_BINDING_INVALID`, the wire rules
`WIRE_PIN_UNKNOWN`, `WIRE_ROLE_MISMATCH`, `EXEC_INTO_PURE`, `PURE_UNCONSUMED`,
`DOMINANCE`, `IN_PLACE_ALIASING`, and the LOAD_SAFETY rules
`DUPLICATE_EXEC_SOURCE`, `DUPLICATE_DATA_INPUT`, `DATA_EDGE_MALFORMED`,
`DATA_CYCLE` (all ERROR). Every game's clean test counts **ERROR-severity
findings only**, so a new WARNING rule lands without reding a gate — which is
also why nobody should "fix" a warning by weakening it to nothing.
`PIN_BINDING_INVALID` fires when a
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
