# GAME design — script integration

This file is part of the GAME design family; the entry document is `Zenith2/Docs/Design/GAME.md`. It specifies how Luau reaches the gameplay framework (inventory item Z2-I-GAME-09 script integration, with the script halves of Z2-I-GAME-06, -07, -08, -19, -20 and -21): the `Gameplay` binding namespace that `ZenithGameplay` registers, its misuse errors, and the `@gameplay` Luau library of helpers in `Zenith2/Engine/ZenithGameplay/Scripts/`. Game rules — a quest, an inventory, a dialogue — are never here; they are game Luau built on these helpers (Z2-D-077; `Zenith2/Docs/Design/GAME/OpenWorldSlice.md` → "Gameplay sample rules").

## Scope of this file

| Topic | Heading |
|---|---|
| how the bindings are registered | "Binding registration" |
| every binding | "Binding functions" |
| what a misuse raises | "Misuse errors" |
| the Luau helpers | "Library layout", "Async helpers", "State machine helper", "Effect and ability spec builders", "Message helpers", "Read helpers" |

## Binding registration

`Zenith_RegisterGameplayScriptBindings(Zenith_ScriptBindingTable& xTable, const Zenith_GameplayModuleServices& xServices)` (`Zenith2/Engine/ZenithGameplay/Internal/Zenith_GameplayScriptBindings.cpp`) is called from the module's `Start` and handed to `Zenith_ModuleStartContext::RegisterScriptBindings` (`Zenith2/Docs/Architecture/CompositionRoot.md` → "Module interfaces"). Every function takes the invocation's `ctx` first in Luau (the leading `Zenith_ScriptInvocationContext&` of the C++ member), as PLAT's `Input` bindings do, except the three pure ones. Every function is registered with `AddFunction<&Zenith_GameplayScriptApi::<Name>>("Gameplay", "<Name>", …)` (`Zenith2/Docs/Scripting/Bindings.md` → "What AddFunction generates"), so Luau reaches it as `Zenith.Gameplay.<Name>`; `Zenith_GameplayScriptApi` (`Internal/Zenith_GameplayScriptApi.h`) is a static-only type whose members take a leading `Zenith_ScriptInvocationContext&`.

| Binding class | Effect (`Zenith_ScriptFunctionEffect`) | Side effects | Contexts |
|---|---|---|---|
| request bindings (every row of § "Binding functions" marked R) | `SCRIPT_FUNCTION_EFFECT_BUFFERED` | `uSIDE_EFFECT_MASK_FIELD_COMMAND`: one record of the module's command kind (`Zenith2/Docs/Design/GAME/Framework.md` → "Command kinds and payloads"), appended with `AppendSideEffect` and a capacity demand of one command record of the payload's size | `SCRIPT_FUNCTION_CONTEXT_SYSTEM \| SCRIPT_FUNCTION_CONTEXT_BEHAVIOUR` |
| `SendMessage` | `SCRIPT_FUNCTION_EFFECT_BUFFERED` | `1u << SIDE_EFFECT_KIND_EVENT`: one `Zenith.Gameplay.Message` event record (one unit of Z2-B-GAME-003) | system, behaviour |
| `GetTick` | `SCRIPT_FUNCTION_EFFECT_READ_WORLD` | none; resource `Zenith.Runtime.WorldState` (a calling module lists it in `Uses`, [Z2-N-SCRIPT-562]) | system, behaviour |
| `TagMatches`, `SecondsToTicks`, `IsValidTag` | `SCRIPT_FUNCTION_EFFECT_PURE` | none | system, behaviour, load |

The command-kind IDs come from the module's cached lookup (`Zenith2/Docs/Design/GAME/Framework.md` → "Command kinds and payloads", the `FindCommandKind` assumption). Names that are tags, attributes, abilities, effects, timers, machines, states and actions are passed as strings and interned through the world's `Zenith_StringTable` by the thunk (a string constant is interned once per module load by the dispatcher's constant cache; `Zenith2/Docs/Scripting/Bindings.md` → "Value marshalling"). The two spec structs are registered as value types (`AddValueType<Zenith_GameplayEffectSpec>("EffectSpec")`, `AddValueType<Zenith_GameplayAbilitySpec>("AbilitySpec")`), so a Luau table with their field names marshals into the struct; the builders of § "Effect and ability spec builders" produce such tables.

## Binding functions

Time arguments are seconds and are converted to ticks by `ceil(seconds × sim.tickHz)` (0 seconds → 1 tick, `Zenith2/Docs/Design/GAME/Framework.md` → "Timers"); `entity` is a `Zenith.Entity`. R marks a request binding: it records one command applied by `Gameplay.Rules` in phase 7 of the same tick (requests recorded in phases 1–6) or of the next tick (phases 7–10), so its effect is visible to a script only in a subsequent segment.

| Function | R | Records (kind, op) or returns |
|---|---|---|
| `AddTag(ctx, entity, tag: string, count: number?)` | R | `Gameplay.Tags` `ADD`, count 1 … 255 (default 1) |
| `RemoveTag(ctx, entity, tag: string, count: number?)` | R | `Gameplay.Tags` `REMOVE` (default 1) |
| `RemoveAllTags(ctx, entity, tag: string)` | R | `Gameplay.Tags` `REMOVE_ALL` |
| `TagMatches(tag: string, query: string) -> boolean` | | `Zenith_GameplayTagMatches` over the interned texts |
| `IsValidTag(text: string) -> boolean` | | `Zenith_GameplayIsValidTagText` |
| `DefineAttribute(ctx, entity, name: string, base: number, min: number?, max: number?)` | R | `Gameplay.Attribute` `DEFINE` (min −∞, max +∞ by default) |
| `SetAttributeBase(ctx, entity, name: string, value: number)` | R | `Gameplay.Attribute` `SET_BASE` |
| `AddAttributeBase(ctx, entity, name: string, delta: number)` | R | `Gameplay.Attribute` `ADD_BASE` |
| `SetAttributeRange(ctx, entity, name: string, min: number, max: number)` | R | `Gameplay.Attribute` `SET_RANGE` |
| `ApplyEffect(ctx, entity, source: Zenith.Entity?, spec: EffectSpec)` | R | `Gameplay.ApplyEffect` |
| `RemoveEffect(ctx, entity, name: string)` | R | `Gameplay.RemoveEffect` `BY_NAME` |
| `RemoveEffectInstance(ctx, entity, instanceId: number)` | R | `Gameplay.RemoveEffect` `BY_INSTANCE` |
| `RemoveEffectsGrantingTag(ctx, entity, tag: string)` | R | `Gameplay.RemoveEffect` `BY_GRANTED_TAG` |
| `GrantAbility(ctx, entity, spec: AbilitySpec)` | R | `Gameplay.GrantAbility` |
| `ActivateAbility(ctx, entity, name: string, instigator: Zenith.Entity?)` | R | `Gameplay.Ability` `ACTIVATE` |
| `EndAbility(ctx, entity, name: string)` | R | `Gameplay.Ability` `END` |
| `CancelAbility(ctx, entity, name: string)` | R | `Gameplay.Ability` `CANCEL` |
| `RevokeAbility(ctx, entity, name: string)` | R | `Gameplay.Ability` `REVOKE` |
| `StartTimer(ctx, entity, name: string, delaySeconds: number, intervalSeconds: number?, repeats: number?)` | R | `Gameplay.Timer` `START` (interval 0 = one shot; repeats `math.huge` = forever, mapped to `uGAMEPLAY_INFINITE_TICKS`) |
| `StopTimer(ctx, entity, name)`, `PauseTimer(ctx, entity, name)`, `ResumeTimer(ctx, entity, name)` | R | `Gameplay.Timer` `STOP`, `PAUSE`, `RESUME` |
| `SetState(ctx, entity, machine: string, state: string)` | R | `Gameplay.State` `SET` |
| `ResetState(ctx, entity, machine)`, `RemoveStateMachine(ctx, entity, machine)` | R | `Gameplay.State` `RESET`, `REMOVE` |
| `StartAction(ctx, entity, name: string, kind: ActionKind, params: ActionParams?)` | R | `Gameplay.Action` `START`; `ActionKind` is the string union `"Manual" \| "Delay" \| "WaitTagAdded" \| "WaitTagRemoved" \| "WaitState" \| "WaitAttribute" \| "WaitAbilityEnd" \| "WaitTimer"`; `ActionParams` = `{ subject: Zenith.Entity?, param: string?, param2: string?, value: number?, compare: ("<" \| "<=" \| ">" \| ">=" \| "==" \| "~=")?, timeoutSeconds: number? }` |
| `CompleteAction(ctx, entity, name)`, `FailAction(ctx, entity, name)`, `CancelAction(ctx, entity, name)`, `AckAction(ctx, entity, name)` | R | `Gameplay.Action` `COMPLETE`, `FAIL`, `CANCEL`, `ACK` |
| `Possess(ctx, player: Zenith.Entity, pawn: Zenith.Entity)`, `Unpossess(ctx, player)` | R | `Gameplay.Possess` `POSSESS`, `UNPOSSESS` |
| `SendMessage(ctx, channel: string, instigator: Zenith.Entity?, target: Zenith.Entity?, magnitude: number?, payload: string?)` | | appends `Zenith.Gameplay.Message` on the calling entity's simulation ID, delivered in phase 9 ([Z2-N-ARCH-478]) |
| `GetTick(ctx) -> number` | | the gameplay tick (`Zenith2/Docs/Design/GAME/Framework.md` → "Gameplay time") |
| `SecondsToTicks(seconds: number) -> number` | | `max(1, ceil(seconds × sim.tickHz))` |

Reads need no binding: a script reads gameplay components through ordinary views (`ctx:View(entity, T.Gameplay.Tags)`, `self:Get(T.Gameplay.Attributes)`), which the schedule sees through its `Reads` key, and the read helpers of § "Read helpers" interpret them. That keeps every read declared and keeps the bindings to the two classes the schedule compiler can account for.

## Misuse errors

Every misuse raises the catalogued runtime error through `Zenith_ScriptFailThunk` (kind `SCRIPT_ERROR_KIND_RUNTIME`), never an assert ([Z2-N-CONTRACT-472]); the dispatcher has already checked argument kinds. The `Gameplay` namespace's codes are `ZS4101`–`ZS4112` (the runtime's `Game` namespace continues the block with ZS4113–ZS4117, `Zenith2/Docs/Design/GAME/WorldHost.md` → "Game script bindings"). The rows below are the codes' only catalogue, kept in GAME's own design as [Z2-N-SCRIPT-926] requires of a block owner; `Zenith2/Docs/Scripting/MisuseCatalogue.md` → "Code ranges" lists the block ZS4100–ZS4199 with `ZS4101–ZS4117` in use (the GAME row, widened by core-doc patch e; cross-area row GA10 (c)):

| Code | Raised by | Message |
|---|---|---|
| ZS4101 | any name argument | `Gameplay.<Fn>: '<text>' is not a valid gameplay name (dotted segments of [A-Za-z0-9_], 1–127 bytes)` |
| ZS4102 | `AddTag`, `RemoveTag` | `Gameplay.<Fn>: count <n> outside 1–255` |
| ZS4103 | attribute bindings | `Gameplay.<Fn>: value <v> is not finite` / `min <a> above max <b>` |
| ZS4104 | `ApplyEffect` | `Gameplay.ApplyEffect: invalid spec field <k> (<field name>)` (from `Zenith_GameplayValidateEffectSpec`) |
| ZS4105 | `GrantAbility` | `Gameplay.GrantAbility: invalid spec field <k> (<field name>)` |
| ZS4106 | time arguments | `Gameplay.<Fn>: <arg> <v> seconds is negative, not finite, or above 86,400` |
| ZS4107 | `StartAction` | `Gameplay.StartAction: unknown kind '<k>'` / `kind <k> requires param` / `kind WaitAttribute requires value and compare` |
| ZS4108 | `StartAction` | `Gameplay.StartAction: empty action name` |
| ZS4109 | request bindings | `Gameplay.<Fn>: entity belongs to another world` (the handle's world tag differs from the invocation's world) |
| ZS4110 | `SendMessage` | `Gameplay.SendMessage: payload above 127 bytes` |
| ZS4111 | request bindings | `Gameplay.<Fn>: the entity's command capacity for this transaction is exhausted` (the `AppendSideEffect` refusal, per-entity cap) |
| ZS4112 | `GetTick` | `Gameplay.GetTick: the calling module does not list Zenith.Runtime.WorldState in Uses` |

Semantic failures that depend on world state (a missing component, a full tag set, a stale target) are not misuse: the request is recorded and the rules system reports them as events and counters (`Zenith2/Docs/Design/GAME/Framework.md` → "Rules algorithm"), because only the rules system sees the state the request will apply to.

## Library layout

<!-- assumption: pending SCRIPT -->
GAME assumes the module's Luau alias is the lower-cased `MODULE_NAME`, `@gameplay`, mapped by the generated `.luaurc` to `Zenith2/Engine/ZenithGameplay/Scripts/` (`Zenith2/Docs/Design/SCRIPT/Bindings.md` step 6 names one alias per selected module without fixing its case). Cross-area row GA10.
<!-- /assumption -->

| Module (`require`) | File | Exports |
|---|---|---|
| `@gameplay/Async` | `Scripts/Async.luau` | `Await`, `AwaitAny`, `Delay` (§ "Async helpers") |
| `@gameplay/StateMachine` | `Scripts/StateMachine.luau` | `Define` (§ "State machine helper") |
| `@gameplay/Effects` | `Scripts/Effects.luau` | `Spec`, `Modifier` (§ "Effect and ability spec builders") |
| `@gameplay/Abilities` | `Scripts/Abilities.luau` | `Spec`, `State` |
| `@gameplay/Messages` | `Scripts/Messages.luau` | `Send`, `OnChannel` (§ "Message helpers") |
| `@gameplay/Tags` | `Scripts/Tags.luau` | `Has`, `HasExact`, `Count`, `List` (§ "Read helpers") |
| `@gameplay/Attributes` | `Scripts/Attributes.luau` | `Get`, `GetBase`, `Has` |
| `@gameplay/Ownership` | `Scripts/Ownership.luau` | `OwningPlayer`, `SameTeam` |
| `@gameplay/Cameras` | `Scripts/Cameras.luau` | `Zenith2/Docs/Design/GAME/Cameras.md` → "Camera bindings" |

Every helper module is `--!strict`, returns a frozen table (`table.freeze`), holds no module-scope mutable state, and passes ZenithCook's stateless analysis for chunk systems (`Zenith2/Docs/Design/SCRIPT/Compiler.md`), so a chunk system may require any of them. Every helper takes the invocation's `ctx` explicitly. Unit tests for each live under `Scripts/Tests/` and run in SCRIPT's Luau test runner (`Zenith2/Docs/Design/GAME/Tests.md` → "Script library tests").

## Async helpers

```luau
--!strict
-- Zenith2/Engine/ZenithGameplay/Scripts/Async.luau
local Zenith = require("@zenith")
local T = require("@types")

export type Outcome = "Succeeded" | "Failed" | "Cancelled" | "TimedOut" | "Expired"
local STATE_NAMES = table.freeze({ [2] = "Succeeded", [3] = "Failed", [4] = "Cancelled", [5] = "TimedOut" })

local function findOutcome(ctx: Zenith.BehaviourContext, owner: Zenith.Entity, name: string): Outcome?
	local actions = ctx:View(owner, T.Gameplay.Actions)
	if actions == nil then
		return "Expired"
	end
	for i = 1, actions.Count do
		local record = actions.Actions[i]
		if record.Name == name then
			if record.State == 1 then
				return nil                       -- still running
			end
			return STATE_NAMES[record.State]
		end
	end
	return "Expired"                             -- retention elapsed, or never started
end

-- Yields once per tick until the named action finishes; returns its outcome.
local function Await(ctx: Zenith.BehaviourContext, owner: Zenith.Entity, name: string): Outcome
	Zenith.WaitTicks(ctx, 1)                     -- the start request applies in phase 7 at the earliest
	while true do
		local outcome = findOutcome(ctx, owner, name)
		if outcome ~= nil then
			return outcome
		end
		Zenith.WaitTicks(ctx, 1)
	end
end
```

| Helper | Behaviour |
|---|---|
| `Await(ctx, owner, name) -> Outcome` | as above: one yield, then one view read per tick until the record of `name` leaves `RUNNING`; a record already gone reads `"Expired"` |
| `AwaitAny(ctx, owner, names: { string }) -> (string, Outcome)` | the first name, in list order, whose action finished in the same segment |
| `Delay(ctx, owner, name, seconds) -> Outcome` | `Zenith.Gameplay.StartAction(ctx, owner, name, "Delay", { timeoutSeconds = seconds })` then `Await` |

The helpers are only callable from a behaviour's `Run` coroutine (they yield; a call from a chunk system raises SCRIPT's yield-in-system error). Because an action lives in a component and its outcome stays readable for `uGAMEPLAY_ACTION_RETENTION_TICKS` (2) ticks, a behaviour of either script phase observes it, and a behaviour restored by `OnRecover` after a save simply calls `Await` again with the same name: there is no ticket to lose ([Z2-N-SCRIPT-340] forbids serialized tickets, and none is used).

## State machine helper

`StateMachine.Define(spec)` builds a frozen machine description over the engine's `Zenith.Gameplay.StateMachines` primitive (`Zenith2/Docs/Design/GAME/Framework.md` → "State machines"):

```luau
local Door = StateMachine.Define({
	Machine = "Door",
	Initial = "Closed",
	States = {
		Closed = { On = { Open = "Opening" } },
		Opening = { On = { Done = "Open", Block = "Closed" }, Parent = "Moving" },
		Open = { On = { Close = "Closing" } },
		Closing = { On = { Done = "Closed" }, Parent = "Moving" },
		Moving = {},
	},
})
-- Door:Current(ctx, entity) -> string; Door:Send(ctx, entity, "Open") -> boolean; Door:IsIn(ctx, entity, "Moving") -> boolean
```

| Member | Rule |
|---|---|
| `Define(spec)` | validates at module load: every state name valid tag text, `Initial` a leaf state, every `On` target a state, `Parent` chains acyclic and at most 8 deep; a violation raises a script error naming the state |
| `:Current(ctx, entity)` | the machine record's `State` from a view of `Zenith.Gameplay.StateMachines`; empty → `Initial` |
| `:Send(ctx, entity, input)` | looks up `input` in the current state's `On`, then its parents' in order; a hit records `Zenith.Gameplay.SetState(ctx, entity, Machine, target)` and returns true; a miss returns false and records nothing |
| `:IsIn(ctx, entity, state)` | true when the current state is `state` or has `state` as an ancestor |
| `:Start(ctx, entity)` | records `Zenith.Gameplay.SetState(ctx, entity, Machine, Initial)` when the machine has no record |

Guards are plain Luau around `:Send`; entry and exit work reacts to `Zenith.Gameplay.StateChanged` in a Late `OnEvent` handler, so every transition is a request applied in phase 7 and every reaction is in phase 10 — deterministic, saved with the component, and visible to tools.

## Effect and ability spec builders

`Effects.Spec(t)` returns a frozen `EffectSpec` table from a friendlier description and validates it with the same rules as `Zenith_GameplayValidateEffectSpec` (`Zenith2/Docs/Design/GAME/Abilities.md` → "Effect specification") at the call, so an authoring mistake fails in the script that made it:

| Key | Maps to | Default |
|---|---|---|
| `Name` (required) | field 1 | — |
| `Duration` (seconds, or `"Instant"` or `"Infinite"`) | 2, by `SecondsToTicks` | `"Instant"` |
| `Period` (seconds) | 3 | none |
| `Modifiers` (`{ Effects.Modifier(attribute, "Add" \| "Multiply" \| "Override", magnitude) }`, ≤ 4) | 4, 5, 6, 12 | none |
| `GrantedTags` (≤ 2 strings) | 7 | none |
| `RequiredTag`, `BlockedTag` | 8, 9 | none |
| `Stacking` (`"Independent" \| "Refresh" \| "Stack" \| "Reject"`), `MaxStacks` | 10, 11 | `"Independent"`, 1 |
| `RemoveWithSource`, `ExecutePeriodOnApply` (booleans) | 13 | false |

`Abilities.Spec(t)` does the same for `AbilitySpec` (`Name`, `Cooldown` seconds, `MaxDuration` seconds, `Cost = { Attribute, Amount }`, `RequiredTags`, `BlockedTags`, `ActiveTags` ≤ 2 each, `Level` 1–255, `CooldownOnEnd`, `CancelOnBlocked`). `Abilities.State(ctx, entity, name) -> ("Ready" | "Active" | "Cooldown" | "NotGranted", cooldownTicksLeft: number)` reads the abilities view. Specs are typically module-scope constants (frozen at load, allowed in chunk systems).

## Message helpers

| Helper | Behaviour |
|---|---|
| `Messages.Send(ctx, channel, fields: { Instigator: Zenith.Entity?, Target: Zenith.Entity?, Magnitude: number?, Payload: string? })` | `Zenith.Gameplay.SendMessage(ctx, channel, …)` with the fields |
| `Messages.OnChannel(query: string, handler: (self, state, event, ctx) -> ())` | returns an `OnEvent` handler for `T.Events.Gameplay.Message` that calls `handler` only when `Zenith.Gameplay.TagMatches(event.Channel, query)`; a behaviour combines several with `Messages.Route({ [query] = handler, … })`, which tries queries in byte order of their text and calls every match |

Channels are gameplay tags, so `Quest.Objective` matches messages sent on `Quest.Objective.Collected` and `Quest.Objective.Reached`.

## Read helpers

| Helper | Reads | Returns |
|---|---|---|
| `Tags.Has(view, query)` | a `T.Gameplay.Tags` view | true when any live tag matches the query hierarchically (`TagMatches`) |
| `Tags.HasExact(view, tag)`, `Tags.Count(view, tag)`, `Tags.List(view)` | same | exact presence; the reference count (0 when absent); the live tags in insertion order |
| `Attributes.Get(view, name)`, `Attributes.GetBase(view, name)`, `Attributes.Has(view, name)` | a `T.Gameplay.Attributes` view | the current value (nil when absent), the base, presence |
| `Ownership.OwningPlayer(ctx, entity) -> Zenith.Entity?` | `T.Gameplay.Owner` and `T.Gameplay.Player` views along the owner chain | the first entity carrying a player component, at most `uGAMEPLAY_MAX_OWNER_HOPS` (8) links (`Zenith2/Docs/Design/GAME/Framework.md` → "Ownership") |
| `Ownership.SameTeam(ctx, a, b) -> boolean` | `T.Gameplay.Owner` views | equal non-zero `Team` |

A cross-entity view follows SCRIPT's cross-entity read rule, so the module that calls `OwningPlayer` lists both components in `Reads`.
