# AI design: script surface and behaviour authoring

Section file of `Zenith2/Docs/Design/AI.md`. It specifies the AI module's Luau bindings (the `AI` namespace), the path request payload and result decoder, the AI misuse codes, and the engine-provided Luau libraries under `Zenith2/Engine/ZenithAI/Scripts/` through which games author behaviours and states: behaviour trees, utility selection, hierarchical state machines, agent helpers, blackboard helpers and decision tracing (inventory items Z2-I-AI-09 blackboards, Z2-I-AI-10 behaviour authoring, Z2-I-AI-11 state authoring, Z2-I-AI-12 debugging).

## Scope of this file

- Decisions are Luau behaviours (Z2-D-016): AI adds no second scripting language, no graph asset and no interpreter of its own. The behaviour tree, utility and state-machine libraries are ordinary Luau library modules that run inside a behaviour's invocation, keep every piece of runtime state in the behaviour's declared `State` table (`Zenith2/Docs/Scripting.md` → "Behaviour state model and recovery"), and therefore inherit the transaction, checkpoint, quarantine, hot-reload and determinism rules of every behaviour.
- Behaviour and state authoring is in code with the libraries' builders: Z2-D-079 gives visual authoring to animation, terrain, UI and cinematics, not to AI; AI's visual surfaces are the debugger and navigation tools (`Zenith2/Docs/Design/AI.md` → "Editor surfaces").
- Chunk systems may call the PURE and READ_WORLD bindings; requests and asynchronous path requests are behaviour bindings where noted.

## Binding registration

`Zenith_RegisterAIScriptBindings(Zenith_ScriptBindingTable& xTable, const Zenith_AIModuleServices& xServices)` (`Zenith2/Engine/ZenithAI/Internal/Script/Zenith_AIScriptBindings.cpp`) is called from `Zenith_Module_AI_Start` and handed to `Zenith_ModuleStartContext::RegisterScriptBindings` (`Zenith2/Docs/Architecture/CompositionRoot.md` → "Module interfaces"). Every function is registered with `AddFunction<&Zenith_AIScriptApi::<Name>>("AI", "<Name>", …)` (`Zenith2/Docs/Scripting/Bindings.md` → "What AddFunction generates"), so Luau reaches it as `Zenith.AI.<Name>`; `Zenith_AIScriptApi` (`Internal/Script/Zenith_AIScriptApi.h`) is a static-only type whose members take a leading `Zenith_ScriptInvocationContext&`, which Luau passes as `ctx` first, as GAME's bindings do (`Zenith2/Docs/Design/GAME/Scripting.md` → "Binding registration").

| Binding class | Effect (`Zenith_ScriptFunctionEffect`) | Side effects | Contexts |
|---|---|---|---|
| request bindings (R in "Binding functions") | `SCRIPT_FUNCTION_EFFECT_BUFFERED` | `uSIDE_EFFECT_MASK_FIELD_COMMAND`: one record of an AI command kind, capacity demand one command record of the payload's size | `SCRIPT_FUNCTION_CONTEXT_SYSTEM \| SCRIPT_FUNCTION_CONTEXT_BEHAVIOUR` |
| `RequestPath` | `SCRIPT_FUNCTION_EFFECT_BUFFERED` | `1u << SIDE_EFFECT_KIND_ASYNC_REQUEST`: one record of `ASYNC_SOURCE_PATHFINDING`, capacity demand one unit of Z2-B-SCRIPT-026's pathfinding value | `SCRIPT_FUNCTION_CONTEXT_BEHAVIOUR` (a chunk system raises SCRIPT's ZS1702) |
| navigation queries (Q) | `SCRIPT_FUNCTION_EFFECT_READ_WORLD` | none; resource `Zenith.AI.World` (the calling module lists it in `Uses`, [Z2-N-SCRIPT-562]) | system, behaviour |
| blackboard reads (B) | `SCRIPT_FUNCTION_EFFECT_READ_WORLD` | none; resource `Zenith.AI.Blackboards` | system, behaviour |
| `TraceDecision`, `IsTracing`, `SecondsToTicks`, `Logistic` | `SCRIPT_FUNCTION_EFFECT_PURE` | none world-visible (`TraceDecision` appends to the development trace ring, "Debug trace helpers") | system, behaviour |

AI also calls, before `Seal`: `DeclareScriptAsyncSource(ASYNC_SOURCE_PATHFINDING, uAI_PATH_RESULT_BYTES)` and `SetScriptSideEffectConsumer(SIDE_EFFECT_KIND_ASYNC_REQUEST, *xServices.m_pxPathConsumer)` on the start context, and `SetAsyncResultDecoder(ASYNC_SOURCE_PATHFINDING, &Zenith_AIDecodePathResult)` on its binding table (`Zenith2/Docs/Design/SCRIPT/PublicAPI.md` → "Zenith_ScriptBinding header"). `Zenith_AIModuleServices`, the consumer and the decoder are declared in `Zenith2/Docs/Design/AI/Internals.md` → "Module services", "Path request consumer", "Path result decoder". The value of `ASYNC_SOURCE_PATHFINDING` is SCRIPT's registry entry (`Zenith2/Docs/Scripting.md` → "Asynchronous source kind registry"); AI writes no number for it.

## Binding functions

Positions are world-local vectors; `entity` arguments are `Zenith.Entity`; time arguments are seconds converted with `max(1, ceil(seconds × sim.tickHz))`. R marks a request binding: its command plays back at the sync point after the recording phase, so a phase-2 request is seen by the same tick's phase 7 and a phase-10 request by the next tick's.

| Function | R/Q/B | Records (kind, op) or returns |
|---|---|---|
| `MoveTo(ctx, agent, target: vector \| Zenith.Entity, acceptRadius: number?, speedScale: number?)` | R | `AI.Agent` `MOVE_TO`; accept radius default 0.5, range (0, 100]; speed scale (0, 4], default keeps the current |
| `Stop(ctx, agent)` | R | `AI.Agent` `STOP` |
| `Pause(ctx, agent)`, `Resume(ctx, agent)` | R | `AI.Agent` `PAUSE`, `RESUME` |
| `SetSpeedScale(ctx, agent, scale: number)` | R | `AI.Agent` `SET_SPEED_SCALE`, [0, 4] |
| `SetMedium(ctx, agent, medium: "Ground" \| "Flying" \| "Underwater")` | R | `AI.Agent` `SET_MEDIUM` |
| `CompleteOffMeshLink(ctx, agent)` | R | `AI.OffMeshLink` `COMPLETE` |
| `SetLinkEnabled(ctx, link, enabled: boolean)` | R | `AI.OffMeshLink` `ENABLE` / `DISABLE` |
| `ReportNoise(ctx, position: vector, loudness: number, instigator: Zenith.Entity?, team: string?)` | R | `AI.Noise`; loudness (0, 1000] metres; team defaults to the instigator's `StimulusSource` team at playback |
| `ReportDamage(ctx, perceiver, instigator)`, `ReportTouch(ctx, perceiver, instigator)` | R | `AI.Stimulus` `DAMAGE`, `TOUCH` |
| `ForgetTarget(ctx, perceiver, target)`, `ForceAware(ctx, perceiver, target)` | R | `AI.Perception` `FORGET`, `FORCE_AWARE` |
| `SetBlackboard(ctx, owner, key: string, value: boolean \| number \| vector \| Zenith.Entity \| string, ttlSeconds: number?)` | R | `AI.Blackboard` `SET`; a string value is stored as an interned name (≤ 63 bytes); a `vector` is kind 3 VECTOR (a direction or offset, never shifted at an origin rebase); no ttl = never expires |
| `SetBlackboardPosition(ctx, owner, key: string, position: vector, ttlSeconds: number?)` | R | `AI.Blackboard` `SET` of kind 6 POSITION: a world-local position, shifted with the world at an origin rebase (`Zenith2/Docs/Design/AI/Navigation.md` → "Origin shift"); read back by `GetBlackboard` as a `vector` |
| `ClearBlackboard(ctx, owner, key: string)`, `ClearAllBlackboard(ctx, owner)` | R | `AI.Blackboard` `CLEAR`, `CLEAR_ALL` |
| `RequestPath(ctx, request: AI.PathRequest) -> Zenith.AsyncTicket` | | the asynchronous path request of "Path request payload and result" |
| `FindNearestPoint(ctx, position: vector, class: string, extents: vector?) -> vector?` | Q | `Zenith_NavFindNearestPoint` with profile 0; extents default (1, 2, 1); nil when none or no navmesh |
| `Raycast(ctx, from: vector, to: vector, class: string, profile: string?) -> (boolean, vector)` | Q | `Zenith_NavRaycast` from the nearest point of `from`: (true, wall point) on a hit, (false, `to` projected) otherwise |
| `RandomReachablePoint(ctx, centre: vector, radius: number, class: string, profile: string?) -> vector?` | Q | `Zenith_NavRandomReachablePoint` with a `Zenith_RandomSource(ulDraw, uAI_RANDOM_POINT_STREAM)` seeded by one draw `ulDraw = ctx.NextRandomDraw()` of the invocation's counter-based stream (`Zenith2/Docs/Scripting/Bindings.md` → "Public binding types"; `Zenith2/Docs/Scripting.md` → "Determinism rules and randomness"), so the point is a function of the transaction's draw and a re-run invocation draws the same point; `uAI_RANDOM_POINT_STREAM` = 0x4149524E44505431 ("AIRNDPT1") |
| `SampleMedium(ctx, position: vector, class: string) -> "Air" \| "Water" \| "Solid"` | Q | `Zenith_NavVolumeSet::SampleMedium` of a volume class |
| `HasNavMesh(ctx, class: string) -> boolean` | Q | `Zenith_NavMesh::HasTiles` |
| `TeamIndex(ctx, team: string) -> number?` | Q | `Zenith_AIWorldResource::GetTeamIndex`; nil when unknown |
| `GetBlackboard(ctx, owner, key: string) -> (boolean \| number \| vector \| Zenith.Entity \| string)?` | B | `Zenith_AIBlackboardStore::Find`; nil when absent, expired, or an entity value that is stale |
| `HasBlackboard(ctx, owner, key: string) -> boolean` | B | the same, as a boolean |
| `TraceDecision(ctx, code: number, a: number?, b: number?, label: string?)` | | appends a trace record for the calling entity ("Debug trace helpers"); a no-op in Shipping |
| `IsTracing(ctx) -> boolean` | | true when the calling entity is the selected agent or `ai.debug.views` has TRACE (development builds); false in Shipping |
| `SecondsToTicks(ctx, seconds: number) -> number` | | `max(1, ceil(seconds × sim.tickHz))` |
| `Logistic(ctx, x: number, k: number, x0: number) -> number` | | `1 / (1 + Zenith_Exp(−k × (x − x0)))` in binary64 with CORE's deterministic exponential (ruling X314-FMA3), so `@ai/Utility`'s logistic curve never reaches a CRT transcendental; a non-finite argument raises `<AI>+02` |

Agent state, perception state and memory are read through views (`self:Get(T.AI.Agent)`, `self:GetContainer(T.AI.PerceptionState, "Memory")`, `ctx:View(entity, T.AI.PerceptionState)`), never through bindings; the generated `components.d.luau` types them (`Zenith2/Docs/Scripting.md` → "Modules, bindings and generated types").

## Path request payload and result

`AI.RequestPath` is the realisation of the `ctx:RequestPath` request kind of `Zenith2/Docs/Scripting.md` → "Merge-time capacity and asynchronous results"; its Luau argument type, generated into `zenith.d.luau`:

```luau
export type PathRequest = {
	from: vector,
	to: vector,
	class: string,                 -- a ground class, or a volume class with medium "Flying"/"Underwater"
	medium: ("Ground" | "Flying" | "Underwater")?,   -- default "Ground"
	profile: string?,              -- a cost profile name; default profile 0
	extents: vector?,              -- nearest-polygon half extents; default (1, 2, 1)
	maxCorners: number?,           -- 1 … 32; default 32
}
export type PathResult = {
	status: "Complete" | "Partial" | "NoStart" | "NoEnd" | "NoNavMesh" | "InvalidRequest" | "Cancelled" | "Truncated",
	limit: "None" | "NodePool" | "TickCap" | "Corridor",   -- what ended the search; not "None": the end may still be reachable
	cornerCount: number,           -- 0 … 32; 0 unless Complete, Partial or Truncated
	corners: AI.PathCorners,       -- the reflected value struct Zenith.AI.PathCorners; corners.Points[i], 1 ≤ i ≤ cornerCount, start first
	offMeshMask: number,           -- bit (i − 1) set when corner i starts an off-mesh link
	length: number,                -- metres through the corners
	cost: number,
	finalPoint: vector,            -- the corridor's end point (for Partial, the closest reachable point)
}
```

The decoder's ABI fills at most `uSCRIPT_ASYNC_RESULT_MAX_FIELDS` (8) named scalar fields — `PathResult` uses all eight — (`Zenith2/Docs/Scripting/Bindings.md` → "Public binding types"), so the corner list travels as one `SCRIPT_VALUE_KIND_STRUCT` field: `Zenith.AI.PathCorners` (registered by `Zenith_Module_AI_RegisterTypes`, 384 bytes: property 1 `Points`, a fixed array of 32 `Zenith_Vector3`), whose value pointer is the payload's own bytes at offset 32 (the corners of "Script result payloads" in `Zenith2/Docs/Design/AI/Formats.md`, laid out exactly as the struct), so the decoder copies nothing. `@ai/Agent`'s `Agent.Corners(result) -> { vector }` turns it into a fresh array for code that wants one.

<!-- assumption: pending SCRIPT (row AI4 (e)) -->
A `SCRIPT_VALUE_KIND_STRUCT` field filled by a ticket-result decoder may point into the payload span the decoder was given (the result table keeps the payload until the result is released, after `Result()` returns), and reaches Luau as a read-only value view of the reflected struct whose fixed-array property is indexed 1-based (`corners.Points[i]`), as component views are (`Zenith2/Docs/Scripting.md` → "Modules, bindings and generated types").
<!-- /assumption -->

1. At the call the thunk validates the table (misuse codes below), resolves `class` and `profile` against the world's settings (a world with no settings yet records class 0xFF and the search answers NO_NAVMESH), and appends a `SIDE_EFFECT_KIND_ASYNC_REQUEST` record whose 64-byte payload is `Zenith2/Docs/Design/AI/Formats.md` → "Script request payload", byte 0 being `ASYNC_SOURCE_PATHFINDING`; it returns a provisional `Zenith.AsyncTicket` ([Z2-N-SCRIPT-339]).
2. At commit, in merge order, SCRIPT assigns the completion ID and the multiplexer calls `Zenith_AIPathRequestConsumer::Consume`, which appends the request to the world's path service inbox (its capacity was accepted at the merge, so `Consume` never fails).
3. The path service runs it (`Zenith2/Docs/Design/AI/Navigation.md` → "Per-tick processing") and parks the 416-byte result (`Zenith2/Docs/Design/AI/Formats.md` → "Script result payloads") with `PUBLICATION_KIND_GENERIC_RECOMPUTED`.
4. At the next tick boundary the result is published in completion-ID order; `ticket:Result()` calls `Zenith_AIDecodePathResult(uPublicationKind, xPayload, xOut)`, which writes eight fields in this order: `status` (a static string of the table above), `limit` (a static string, from payload byte 2, `Zenith2/Docs/Design/AI/Navigation.md` → "Partial results"), `cornerCount` (NUMBER), `corners` (STRUCT `Zenith.AI.PathCorners`, the first C points meaningful and the rest zero), `offMeshMask` (NUMBER), `length`, `cost` (NUMBER) and `finalPoint` (VECTOR); a publication kind other than `PUBLICATION_KIND_GENERIC_RECOMPUTED`, a payload of another size than 416 bytes, a status byte outside `Zenith_NavPathStatus`, a limit byte above 3 or a corner count above 32 makes the decoder return false (a world halt, SCRIPT's rule: the payload came from AI's own producer).

## Misuse codes

AI's bindings raise catalogued errors from one block of `Zenith2/Docs/Scripting/MisuseCatalogue.md` → "Code ranges": ZS2500–ZS2599, the block the orchestrator's ruling MISUSE-BLOCKS-2 assigns to AI (cross-document item X288, which amends X277 (5); cross-area row AI4; [Z2-N-SCRIPT-926]; the first reservation applied wins). This design names the codes by offset within the block (`<AI>+01` is the block's first code, ZS2501 while the block is ZS2500–ZS2599), so the table below is unchanged if the reservation lands elsewhere; the bindings ticket writes the codes into the catalogue row and its tests read them from the row.

| Offset | Kind | Trigger | Text |
|---|---|---|---|
| +01 | MISUSE | a vector argument with a NaN or infinite component | `<Function>: argument <Index> must be a finite vector` |
| +02 | MISUSE | a numeric argument outside its documented range | `<Function>: argument <Index> must be in <Range>, got <Value>` |
| +03 | MISUSE | a class name the world's navigation settings do not declare (raised only once settings exist) | `<Function>: unknown navigation class '<Name>'` |
| +04 | MISUSE | a cost profile name the settings do not declare | `<Function>: unknown cost profile '<Name>'` |
| +05 | MISUSE | a medium string other than Ground, Flying, Underwater | `<Function>: medium must be "Ground", "Flying" or "Underwater"` |
| +06 | MISUSE | a team name outside `ai.perception.teamNames` | `<Function>: unknown team '<Name>'` |
| +07 | MISUSE | a blackboard key that is empty, longer than 63 bytes or not `^[A-Za-z_][A-Za-z0-9_.]*$` | `<Function>: invalid blackboard key '<Key>'` |
| +08 | MISUSE | a blackboard value of an unsupported type, or a string value longer than 63 bytes | `<Function>: blackboard values must be boolean, number, vector, Entity or a string of at most 63 bytes` |
| +09 | MISUSE | a `PathRequest` table with a missing required field, an unknown key, or a medium that does not match the class's medium | `RequestPath: <Reason>` |

Errors the libraries raise (definition errors detected when a module builds a tree or machine at load) are ordinary Luau `error()` calls with the texts of each library's section; they fail the module's load (`SCRIPT_ERROR_KIND_LOAD`, BR-LOAD) because they are raised at module scope.

## Luau library layout

```
Zenith2/Engine/ZenithAI/Scripts/
	BehaviourTree.luau     @ai/BehaviourTree
	Utility.luau           @ai/Utility
	StateMachine.luau      @ai/StateMachine
	Agent.luau             @ai/Agent
	Blackboard.luau        @ai/Blackboard
	Trace.luau             @ai/Trace
```

- Each file begins `--!strict`, is a library module (its return value is a table of functions and constants, `Zenith2/Docs/Scripting.md` → "Modules, bindings and generated types"), holds no module-scope mutable state and passes the D-LUAU-STATELESS (open, Stage 1) load check whatever mechanism the ruling selects, because a behaviour or system that requires it makes the check transitive.
- The alias `@ai/` exists only in games that select the `AI` module; ZenithCook type-checks the libraries with the game's definition files and fails the cook on a type error ([Z2-N-SCRIPT-115]).
- Trees and machines are built at module scope of the requiring behaviour module, as deep-frozen constant tables ([Z2-N-SCRIPT-106]); every value that changes at run time lives in the behaviour's `State`.

## Behaviour tree library

```luau
local BT = require("@ai/BehaviourTree")

local tree = BT.Build("Guard", BT.ReactiveSelector({
	BT.Sequence({ BT.Condition("Threatened", isThreatened), BT.Action("Flee", flee) }),
	BT.Sequence({ BT.Condition("HasTarget", hasTarget), BT.Action("Chase", chase, abortChase) }),
	BT.Action("Patrol", patrol),
}))

return Zenith.Behaviour({
	Phase = "Early",
	Reads = { T.AI.Agent, T.AI.PerceptionState },
	Uses = { "Zenith.AI.World", "Zenith.AI.Blackboards" },
	State = { brain = BT.StateField(tree) },
	OnUpdate = function(self, state, ctx)
		BT.Tick(tree, state.brain, self, ctx)
	end,
})
```

| Node | Children | Status rule | Slot use (s: i32, t: i64) |
|---|---|---|---|
| `Sequence` | 1 … 16 | runs children in order from the running one; Failure → Failure; all Success → Success; Running → Running | s = running child index |
| `Selector` | 1 … 16 | runs children in order from the running one; Success → Success; all Failure → Failure | s = running child index |
| `ReactiveSelector` | 1 … 16 | every tick from child 1; a higher-priority child that does not fail aborts the running lower one | s = running child index |
| `Parallel(policy)` | 1 … 16 | ticks every non-finished child each tick; `"RequireAll"`: Success when all succeed, Failure on the first failure; `"RequireOne"`: Success on the first success, Failure when all fail | s = bitmask of finished children (16 bits) and their statuses (16 bits) |
| `RandomSelector` | 1 … 16 | on entry, orders children by a permutation drawn from `ctx.Random`, then behaves as `Selector` | s = running position; t = the drawn permutation packed 4 bits per child |
| `Inverter`, `Succeeder`, `Failer` | 1 | maps the child's status | — |
| `Repeat(n)` | 1 | re-runs the child after Success n times (0 = forever); Failure → Failure | s = completed count |
| `RetryUntilSuccess(n)` | 1 | re-runs after Failure up to n times | s = attempts |
| `Cooldown(seconds)` | 1 | Failure while the cooldown runs; after the child finishes the cooldown starts | t = end tick |
| `TimeLimit(seconds)` | 1 | aborts the child and fails at the deadline | t = deadline tick |
| `Wait(seconds)` | 0 | Running until the end tick, then Success | t = end tick |
| `Condition(name, fn)` | 0 | `fn(self, ctx) -> boolean`: Success or Failure, never Running | — |
| `Action(name, fn, abort?)` | 0 | `fn(self, ctx, node) -> "Success" \| "Failure" \| "Running"`; `node.slot` (an i64 scratch slot) and `node.entered` (true on the first tick of a run); `abort(self, ctx, node)` when an ancestor aborts it | t = the action's scratch |
| `SubTree(tree)` | 0 | ticks another built tree inline; its nodes occupy slots after the parent's | its own |

- `BT.Build(name, root)` numbers nodes in pre-order (root 1), validates (at most 256 nodes, depth ≤ 32, child counts above, every `Condition`/`Action` function a function, names unique within the tree) and freezes the result; a violation is `error("BehaviourTree '<name>': <reason>")` at module load.
- `BT.StateField(tree)` returns `Zenith.Field.Table({ s = Zenith.Field.Array(Zenith.Field.I32(0), N), t = Zenith.Field.Array(Zenith.Field.I64(0), N) })` with N the node count, so the tree's whole runtime state is serializable behaviour state (at most 3,076 bytes at N = 256, inside Z2-B-SCRIPT-015's checkpoint reservation, pending).
- `BT.Tick(tree, brain, self, ctx) -> status` walks from the root resuming running nodes through `brain.s`; time is `ctx` tick time; it allocates no table per tick (node contexts are reused per call through a constant per-tree array of method tables).
- Aborting: when a composite moves away from a running child, the library resets the child subtree's slots in pre-order and calls `abort` of each Action in it that was running, innermost first.
- A behaviour that yields (a `Run` coroutine) may call `BT.Tick` each segment; because the tree's state is in `State`, a restore through `OnRecover` resumes the tree exactly where its last checkpoint left it.
- Tracing: when `AI.IsTracing(ctx)` is true, `BT.Tick` calls `AI.TraceDecision(ctx, nodeIndex, statusCode)` for every node whose status changed this tick, so the debugger shows the tree's path without per-node cost when not tracing.

## Utility library

```luau
local U = require("@ai/Utility")
local choose = U.Build("Combat", {
	U.Option("Attack", { U.Consider(targetDistance, U.Curve.Inverse(2.0)), U.Consider(health, U.Curve.Linear(1, 0)) }),
	U.Option("Retreat", { U.Consider(health, U.Curve.Linear(-1, 1)) }),
	U.Option("Idle", {}, 0.05),     -- a constant base score
})
-- State = { combat = U.StateField(choose) }
local index, name = U.Select(choose, state.combat, self, ctx)
```

- A consideration's input `fn(self, ctx) -> number` is clamped to [0, 1] after its curve; curves: `Linear(m, b)` = m x + b, `Quadratic(k)` = x^k (integer k ≥ 1 by repeated multiplication; a non-integer k is a build error at load), `Logistic(k, x0)` = `AI.Logistic(ctx, x, k, x0)` (CORE's deterministic `Zenith_Exp` behind a PURE binding; like every AI binding it takes the invocation's `ctx` first, which the library holds because `U.Select(choose, state, self, ctx)` passes it to every consideration and curve, a curve being evaluated as `curve(x, ctx)`; x = x₀ gives exactly 0.5 because `Zenith_Exp(0) == 1` exactly, row AI22; the library calls no `math.exp`, `math.sin`, `math.cos`, `math.atan2` or `math.pow`, ruling X314-FMA3), `Inverse(s)` = s / (s + x), `Step(x0)` = x ≥ x0 and 1 or 0.
- Score of an option with n considerations: the product P of curve values, compensated: P + (1 − P) × (1 − 1/n) × P when n > 1; an option without considerations scores its base score.
- Selection: the highest score; the currently selected option's score is multiplied by 1.1 (momentum) before comparison; ties by option order. The state field is `Zenith.Field.Table({ current = Zenith.Field.I32(0), since = Zenith.Field.I64(0) })`.
- At most 32 options and 8 considerations per option; `U.Build` errors at load beyond that.

## State machine library

```luau
local SM = require("@ai/StateMachine")
local machine = SM.Build("Guard", {
	initial = "Patrol",
	states = {
		Patrol = { update = patrol, transitions = { { to = "Alert", when = heardSomething } } },
		Alert = {
			initial = "Search",
			enter = raiseAlarm,
			transitions = { { to = "Patrol", when = calmDown } },
			states = {
				Search = { update = search, transitions = { { to = "Engage", when = seesTarget } } },
				Engage = { update = engage },
			},
		},
	},
	onChanged = mirrorToGameplay,   -- optional: function(self, ctx, fromPath, toPath)
})
-- State = { sm = SM.StateField(machine) }
SM.Update(machine, state.sm, self, ctx)
```

- States nest at most 4 deep and a machine holds at most 64 states; state names are unique per machine; `SM.Build` errors at load otherwise.
- `SM.StateField(machine)` is `Zenith.Field.Table({ path = Zenith.Field.Array(Zenith.Field.I32(0), 4), entered = Zenith.Field.Array(Zenith.Field.I64(0), 4) })`: the active state index per depth and the tick each was entered.
- `SM.Update`: evaluate transitions from the outermost active state inward and take the first whose `when(self, ctx)` is true; exit states innermost-first up to the lowest common ancestor, enter outer-first down to the target's initial leaf, calling `exit` and `enter`; repeat evaluation at most 4 times in one update (a fifth transition is `error("StateMachine '<name>': more than 4 transitions in one update")`); then call `update` of every active state outer-first.
- `onChanged` is how a game mirrors an AI state into GAME's state-machine component (`Zenith2/Docs/Design/GAME/Framework.md` → "State machines") with `Gameplay.SetState`, when it wants other entities to read it; AI's library does not depend on the Gameplay module.

<!-- informative -->
GAME's state machine is a world-visible component whose transitions are commands applied in phase 7; an AI decision machine needs same-invocation enter/exit/update and nesting inside one behaviour transaction, so it lives in behaviour state. The two meet through `onChanged`.
<!-- /informative -->

## Agent library

| Function | Does |
|---|---|
| `Agent.MoveTo(ctx, self, target, opts?)` | `AI.MoveTo` for the calling entity; returns the serial the move will have (the current `MoveSerial` + the moves this invocation recorded + 1) |
| `Agent.Status(self) -> string` | the `MoveStatus` name of `self:Get(T.AI.Agent)` |
| `Agent.IsDone(self, serial) -> (boolean, string?)` | true once `CompletedSerial ≥ serial`, with the result name of the latest `MoveCompleted` the behaviour received |
| `Agent.WaitArrived(ctx, self, serial)` | in a `Run` coroutine: waits (`Zenith2/Docs/Design/SCRIPT/Runtime.md` → "Wait conditions") until `CompletedSerial ≥ serial` |
| `Agent.Stop(ctx, self)` | `AI.Stop` |
| `Agent.PrimaryTarget(self) -> (Zenith.Entity?, number, vector)` | PrimaryTarget, PrimaryAwareness and PrimaryLastKnownPosition of `self:Get(T.AI.PerceptionState)` |
| `Agent.Memory(self) -> { PerceivedTarget }` | the Memory container's entries, as a fresh array |
| `Agent.Corners(result: PathResult) -> { vector }` | the first `cornerCount` points of `result.corners`, as a fresh array |
| `Agent.LocomotionParameters(self) -> (number, vector, number)` | for a root-motion agent ("Authoring conventions"): the speed \|DesiredVelocity\| (m/s), the desired direction in the entity's frame (`Zenith_InverseRotate(rotation, normalise(DesiredVelocity))`, zero when the speed is 0) and the yaw delta `wrap(DesiredYaw − current yaw)` in (−π, π], from `self:Get(T.AI.Agent)` 43, 48 and the entity's `Zenith.LocalTransform` 2 |

## Blackboard library

`Blackboard.Get(ctx, owner, key, default)`, `GetNumber`, `GetVector`, `GetEntity`, `GetName` (typed reads returning `default` for an absent or differently-typed value; `GetVector` returns kinds 3 and 6), `Blackboard.Set(ctx, owner, key, value, ttlSeconds?)`, `Blackboard.SetPosition(ctx, owner, key, position, ttlSeconds?)` (kind 6, over `AI.SetBlackboardPosition`; a remembered place such as a last-seen position or a rally point uses it so it survives an origin rebase), `Blackboard.Clear(ctx, owner, key)`; each is a thin wrapper over the bindings that keeps the key spelling in one constant table per game (`Blackboard.Keys(...)` freezes a list of key names and checks them against the key rule at load).

## Debug trace helpers

- `AI.TraceDecision` appends a 32-byte record (tick, entity simulation ID, per-VM sequence number, code, a, b, label's interned ID) to the **staging buffer of the calling VM**, indexed by `ctx.GetVMIndex()` (`Zenith2/Docs/Scripting/Bindings.md` → "Public binding types"): one buffer of 1,024 records per VM slot, and a VM runs one invocation at a time on one thread, so chunk systems on several workers and behaviours on the main thread never write one buffer concurrently and AI takes no lock. A full staging buffer drops the record (`ai.trace.dropped_delta`).
- Once per tick, at the start of `AI.NavIngest` (phase 7, when no chunk system or behaviour of the world runs; records staged in phases 8 … 12 wait for the next tick's merge), the module merges the non-empty staging buffers into the per-agent rings in (simulation ID, tick, VM index, sequence) order: each record goes to its entity's ring of `uAI_DEBUG_TRACE_RECORDS_PER_AGENT` (64) records, for at most `uAI_DEBUG_TRACE_AGENTS` (256) agents; a 257th traced agent reuses the ring of the agent whose last record is oldest (ties: the higher simulation ID). The merge is single-threaded and runs with no script invocation in flight.
- The trace ring is development-only diagnostic state: no simulation read observes it, it is not saved, a rolled-back transaction's records stay (they show the decision that was attempted, flagged with the invocation's outcome when the merge reports it), and Shipping compiles the binding as an immediate return.
- **Tracing is outside the reproducibility promise.** `AI.IsTracing` depends on the editor's selection and on `ai.debug.*`, which are not simulation inputs and are not in the replay log, and a behaviour that branches on it (`@ai/BehaviourTree` calls `TraceDecision` only while tracing) executes more instructions and so may reach a safepoint budget at another point. A run with tracing on is therefore a diagnostic session: its tick hashes may differ from the same run with tracing off. Every determinism suite and every recorded scenario runs with `ai.debug.views` = 0 and `ai.debug.selectedAgent` empty, which makes `IsTracing` false at every call (`Zenith2/Docs/Design/AI/Tests.md` → "Save and determinism tests").
- `@ai/Trace` adds `Trace.Note(ctx, label, value)` and `Trace.Enabled(ctx)` wrappers; the editor's AI debugger shows the records (`Zenith2/Docs/Design/AI.md` → "Editor surfaces").

## Authoring conventions

- A behaviour that moves an agent through `AI.MoveTo` never writes the agent's character-controller `DesiredVelocity` itself while the agent is moving: AI's locomotion commands would otherwise race the behaviour's writes at different sync points (the last write in a tick wins, by phase order).
- **Root-motion agents.** An agent whose character controller is in a root-motion mode moves only by its animation (ruling Q72; `Zenith2/Docs/Design/AI/Agents.md` → "Locomotion output"). Its game drives the animation from AI's intent: a `Late` behaviour (phase 10) of the agent reads `Agent.LocomotionParameters(self)` and records them as the animator's locomotion parameters with ANIM's parameter command (`Zenith2/Docs/Design/ANIM/Runtime.md` → "Luau bindings"), so phase 3 of the next tick evaluates them and PHYS applies the resulting root motion in phase 4, the same one-tick latency as a velocity agent. The graph's locomotion blend space is expected to reach the requested speed within its own acceleration; an agent whose animation delivers no motion fails its move through the root-motion stuck rule (`Zenith2/Docs/Design/AI/Agents.md` → "Stuck detection").
- A behaviour declares `Uses = { "Zenith.AI.World" }` before calling a navigation query and `"Zenith.AI.Blackboards"` before a blackboard read ([Z2-N-SCRIPT-562]).
- Cross-agent coordination (squads, formations, tactical slots) is Luau over blackboards on a shared entity and over events; AI ships no squad system.
- Every tree, utility set and machine is a module-scope constant; per-instance tuning is behaviour state or component data, never a closure captured at module scope.
