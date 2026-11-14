# GAME design — abilities, attributes and effects

This file is part of the GAME design family; the entry document is `Zenith2/Docs/Design/GAME.md`. It specifies the reusable ability primitive (inventory item Z2-I-GAME-07, Stage 3) and the attribute and effect primitives (Z2-I-GAME-20 and Z2-I-GAME-21, Stage 4) of `ZenithGameplay`: their components, the specification structs a request carries, and the algorithms the rules system runs for them (`Zenith2/Docs/Design/GAME/Framework.md` → "Rules algorithm" steps 2, 4, 5 and 7). Like every primitive of the module they carry no game's rules: names, magnitudes, durations and costs are a game's data, and an ability's behaviour (what a dash does) is a game's Luau, run by a behaviour subscribed to `Zenith.Gameplay.AbilityActivated`.

## Scope of this file

| Topic | Heading |
|---|---|
| the ability component, activation, end, cooldown | "Abilities" |
| the attribute component and the evaluation formula | "Attributes" |
| effect specifications, records, application, stacking, expiry, periods | "Effects" |
| the order of the three within a tick | "Ordering within a tick" |

## Abilities

### Ability component

```cpp
// Zenith2/Engine/ZenithGameplay/Zenith_GameplayAbilities.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_StringID.h"
#include "ZenithECS/Zenith_EntityHandle.h"
#include "ZenithReflect/Zenith_ReflectIDs.h"
#include "ZenithGameplay/Zenith_GameplayTypes.h"

/// @contract value Zenith_GameplayAbilitySpec
/// Ownership: Plain value: the payload of a Gameplay.GrantAbility request and the definition half of an ability record.
/// Thread affinity: IMMUTABLE; members non-const: a registered payload struct that command recording and playback write
///   field by field.
/// Invalidation: None.
/// Units: Ticks for the two durations; the cost in the cost attribute's units.
/// Errors: None of its own; Zenith_GameplayValidateAbilitySpec reports the first invalid field.
/// Version: since Z2 Stage 3; serialized inside the ability record, so a layout change is a component version step.
/// @endcontract
struct Zenith_GameplayAbilitySpec                      // 48 bytes
{
	Zenith_StringID m_xName;                           // non-empty, unique per entity
	uint32 m_uCooldownTicks = 0;                      // 0 = no cooldown
	uint32 m_uMaxDurationTicks = 0;                   // 0 = active until ended
	Zenith_StringID m_xCostAttribute;                  // empty = no cost
	float m_fCostAmount = 0.0f;                        // finite, ≥ 0
	Zenith_StringID m_axRequiredTags[uGAMEPLAY_MAX_ABILITY_TAGS];   // every non-empty entry must match on the owner
	Zenith_StringID m_axBlockedTags[uGAMEPLAY_MAX_ABILITY_TAGS];    // no non-empty entry may match on the owner
	Zenith_StringID m_axActiveTags[uGAMEPLAY_MAX_ABILITY_TAGS];     // granted (one reference each) while active
	uint8 m_uLevel = 1;                               // 1 … 255, reported in events
	uint8 m_uFlags = ABILITY_FLAG_NONE;               // Zenith_GameplayAbilityFlags
	uint8 m_auReserved[2] = {};
};
static_assert(sizeof(Zenith_GameplayAbilitySpec) == 48);

/// @contract value Zenith_GameplayAbilityRecord
/// Ownership: Stored inside Zenith_GameplayAbilitiesComponent.
/// Thread affinity: IMMUTABLE as a value; members non-const: a registered struct its containing component, payload or
///   result writes field by field; the component's rows follow the AINavigation authority.
/// Invalidation: None.
/// Units: Gameplay ticks (Framework.md "Gameplay time") for the two end ticks.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_GameplayAbilityRecord                    // 80 bytes
{
	Zenith_GameplayAbilitySpec m_xSpec;                // 48
	Zenith_EntityHandle m_xInstigator;                 // the current or latest activation's instigator
	uint64 m_ulCooldownEndTick = 0;                   // READY when ≤ the current tick
	uint64 m_ulActiveEndTick = 0;                     // 0 = no expiry
	uint32 m_uActivationCount = 0;                    // the latest activation ID; 0 before the first
	uint8 m_eState = ABILITY_STATE_READY;             // Zenith_GameplayAbilityState
	uint8 m_auReserved[3] = {};
};
static_assert(sizeof(Zenith_GameplayAbilityRecord) == 80);

/// @contract value Zenith_GameplayAbilitiesComponent
/// Ownership: Stored as component "Zenith.Gameplay.Abilities".
/// Thread affinity: IMMUTABLE as a value; members non-const: an ECS component the store relocates and systems write
///   property by property; rows follow the AINavigation authority.
/// Invalidation: None.
/// Units: As Zenith_GameplayAbilityRecord.
/// Errors: None; a ninth grant is rejected and counted (gameplay.abilities.full).
/// Version: since Z2 Stage 3; type version 1; property IDs 1 and 2 are permanent.
/// @endcontract
struct Zenith_GameplayAbilitiesComponent               // 648 bytes; "Zenith.Gameplay.Abilities"
{
	Zenith_GameplayAbilityRecord m_axAbilities[uGAMEPLAY_MAX_ABILITIES];   // 1 Abilities
	uint8 m_uCount = 0;                                                   // 2 Count
	uint8 m_auReserved[7] = {};
	static const Zenith_TypeID xTYPE_ID;
};
static_assert(sizeof(Zenith_GameplayAbilitiesComponent) == 648);

uint8 Zenith_GameplayValidateAbilitySpec(const Zenith_GameplayAbilitySpec& xSpec);   // 0 = valid, else the 1-based field number
```

Validation (`Zenith_GameplayValidateAbilitySpec`; an invalid grant is dropped with `gameplay.abilities.invalid_spec` + 1 and the log line `gameplay: ability grant on entity <simID> rejected: invalid spec field <k>`): field 1 the name non-empty and valid tag text (`Zenith_GameplayIsValidTagText`); field 4 the cost attribute empty or valid tag text; field 5 the cost finite and ≥ 0; fields 6–8 each tag empty or valid; field 9 the level ≥ 1; field 10 no flag bit above bit 1.

Requests: `Gameplay.GrantAbility(target, spec)` — an existing record of the name gets the new spec and keeps its state, activation count and end ticks; otherwise the spec is appended in `ABILITY_STATE_READY` (a ninth ability is dropped, `gameplay.abilities.full`). `Gameplay.Ability` with op `ACTIVATE`, `END`, `CANCEL` or `REVOKE`.

### Ability activation algorithm

`ACTIVATE(owner O, ability a, instigator I)` at tick t, the checks in this order, the first that fails emitting `AbilityFailed { Ability = a, Failure }` and changing nothing:

| Check | Failure |
|---|---|
| O has a record named a | `ABILITY_FAILURE_NOT_GRANTED` |
| the record is not `ABILITY_STATE_ACTIVE` | `ABILITY_FAILURE_ALREADY_ACTIVE` |
| the record is not `ABILITY_STATE_COOLDOWN` with `m_ulCooldownEndTick > t` | `ABILITY_FAILURE_ON_COOLDOWN` |
| every non-empty required tag matches on O (`Zenith_GameplayTagsHasMatch`) | `ABILITY_FAILURE_MISSING_TAG` |
| no non-empty blocked tag matches on O | `ABILITY_FAILURE_BLOCKED_TAG` |
| the cost attribute, when named, exists on O's `Zenith.Gameplay.Attributes` | `ABILITY_FAILURE_NO_COST_ATTRIBUTE` |
| the cost attribute's evaluated value on O ≥ the cost (below) | `ABILITY_FAILURE_INSUFFICIENT_COST` |

The evaluated value is `Zenith_GameplayEvaluateAttribute(cost attribute, Base, Min, Max, O's effects row or null, bNonFinite)` (§ "Current value evaluation") over O's attributes row as it stands at this request — O is the current entity, so both rows are read on O's own row — never the stored `Current`, which step 7 of `Zenith2/Docs/Design/GAME/Framework.md` → "Rules algorithm" refreshes only at the end of the pass. Two activations in one drain therefore see each other's payment: the second is checked against the base the first lowered, so an attribute that covers one cost never pays two. The evaluation counts nothing (step 7 alone counts `gameplay.attributes.non_finite`).

On success, in this order:

1. Pay the cost: the attribute's base value −= the cost, clamped to its range (§ "Attribute requests"); mark the row's attributes dirty.
2. `m_eState = ABILITY_STATE_ACTIVE`; `m_uActivationCount += 1` (wrapping from 2^32 − 1 to 1); `m_xInstigator = I`.
3. `m_ulActiveEndTick = m_uMaxDurationTicks ? t + m_uMaxDurationTicks : 0`.
4. Unless `ABILITY_FLAG_COOLDOWN_ON_END`: `m_ulCooldownEndTick = t + m_uCooldownTicks`.
5. Add one reference of each non-empty active tag (`Zenith2/Docs/Design/GAME/Framework.md` → "Tag reference counting"), emitting their `TagChanged` events.
6. Emit `AbilityActivated { Instigator = I, Ability = a, ActivationID = m_uActivationCount, Level }`.

An activation is a state, not a behaviour: the game's Late behaviour subscribed to `AbilityActivated` performs the ability (spawns a projectile, plays an animation, applies effects) and records `END` when done, or relies on `m_uMaxDurationTicks`.

### Ability end and expiry

Ending an active record (`END` → reason `ENDED`, `CANCEL` → `CANCELLED`, `REVOKE` → `REVOKED`, the expiry sweep → `EXPIRED`, the blocked sweep → `BLOCKED`), at tick t:

1. Remove one reference of each non-empty active tag.
2. With `ABILITY_FLAG_COOLDOWN_ON_END`: `m_ulCooldownEndTick = t + m_uCooldownTicks`.
3. `m_eState = (m_ulCooldownEndTick > t) ? ABILITY_STATE_COOLDOWN : ABILITY_STATE_READY`; `m_ulActiveEndTick = 0`.
4. Emit `AbilityEnded { Ability, ActivationID, Reason }`.
5. `REVOKE` then removes the record (shift left). `REVOKE` of a record that is not active removes it with no event; `END` and `CANCEL` of a record that is not active are counted (`gameplay.abilities.end_inactive`) and change nothing.

The sweep of `Zenith2/Docs/Design/GAME/Framework.md` → "Rules algorithm" step 5, per record in index order: an active record with `m_ulActiveEndTick != 0 && m_ulActiveEndTick ≤ t` ends `EXPIRED`; otherwise an active record with `ABILITY_FLAG_CANCEL_ON_BLOCKED` and a matching blocked tag ends `BLOCKED`; a `COOLDOWN` record with `m_ulCooldownEndTick ≤ t` becomes `READY` and emits `AbilityReady`.

## Attributes

### Attribute component

```cpp
/// @contract value Zenith_GameplayAttributesComponent
/// Ownership: Stored as component "Zenith.Gameplay.Attributes".
/// Thread affinity: IMMUTABLE as a value; members non-const: an ECS component the store relocates and systems write
///   property by property; rows follow the AINavigation authority.
/// Invalidation: None.
/// Units: The game's units per attribute; Min ≤ Base ≤ Max and Min ≤ Current ≤ Max hold after every rules pass.
/// Errors: None; a seventeenth attribute is rejected and counted (gameplay.attributes.full).
/// Version: since Z2 Stage 3 (registered by the API-only slice; behaviour since Stage 4); type version 1.
/// @endcontract
struct Zenith_GameplayAttributesComponent              // 328 bytes
{
	Zenith_StringID m_axNames[uGAMEPLAY_MAX_ATTRIBUTES];   // 1 Names, unique per entity, insertion order
	float m_afBase[uGAMEPLAY_MAX_ATTRIBUTES] = {};         // 2 Base
	float m_afCurrent[uGAMEPLAY_MAX_ATTRIBUTES] = {};      // 3 Current: the evaluated value (serialized; an edit is re-evaluated)
	float m_afMin[uGAMEPLAY_MAX_ATTRIBUTES] = {};          // 4 Min
	float m_afMax[uGAMEPLAY_MAX_ATTRIBUTES] = {};          // 5 Max
	uint8 m_uCount = 0;                                   // 6 Count
	uint8 m_auReserved[7] = {};
	static const Zenith_TypeID xTYPE_ID;                   // "Zenith.Gameplay.Attributes"
};
static_assert(sizeof(Zenith_GameplayAttributesComponent) == 328);
```

`Current` is serialized (`PROPERTY_FLAG_SERIALIZED`, like every GAME property: `Zenith2/Docs/Design/GAME/Framework.md` → "Component and resource registration") so a loaded save, a carried-over entity and a freshly loaded scene carry their evaluated values into the first tick. The rules system re-evaluates, in step 7 of `Zenith2/Docs/Design/GAME/Framework.md` → "Rules algorithm", every row its own steps 2–5 marked dirty and every row whose `Zenith.Gameplay.Attributes` row change version is newer than its previous read version (`Zenith_SystemContext::GetPreviousReadVersion`, `Zenith2/Docs/Design/ECS/SystemAPI.md` → "Zenith_SystemContext header"), so spawned, loaded, restored and inspector-edited rows converge at the first phase 7 after the change; the rows the system itself committed in its previous run match the version clause too, re-evaluate to the same bits and emit no event. Authoring writes `Current` equal to `Base` clamped (the editor's inspector rule, EDIT; the `Attributes.Define` Luau helper, `Zenith2/Docs/Design/GAME/Scripting.md` → "Effect and ability spec builders").

### Attribute requests

`Gameplay.Attribute` ops, at the rules system (§ "Ordering within a tick"):

| Op | Effect | Rejected (counted `gameplay.attributes.rejected`, logged `gameplay: attribute request <op> on entity <simID> rejected: <reason>`, no event) when |
|---|---|---|
| `DEFINE(name, base, min, max)` | append the attribute if absent (else update it): `Min = min`, `Max = max`, `Base = clamp(base, min, max)`; mark dirty | `min > max`, a non-finite value, an empty or invalid name, a seventeenth attribute (counted `gameplay.attributes.full` instead) |
| `SET_BASE(name, value)` | `Base = clamp(value, Min, Max)`; mark dirty | the attribute absent; non-finite value |
| `ADD_BASE(name, delta)` | `Base = clamp(Base + delta, Min, Max)`; mark dirty | the attribute absent; non-finite delta |
| `SET_RANGE(name, min, max)` | `Min = min`, `Max = max`, `Base` re-clamped; mark dirty | `min > max`; non-finite |

### Current value evaluation

```cpp
// Zenith2/Engine/ZenithGameplay/Zenith_GameplayMath.h
/// @contract functions GameplayAttributeEvaluation
/// Ownership: Pure function over the caller's values.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: The attribute's units.
/// Errors: None; a non-finite result is replaced by the clamped base and reported through bNonFiniteOut.
/// Version: since Z2 Stage 4. The operation order below is part of the determinism contract (Z2-D-019).
/// @endcontract
float Zenith_GameplayEvaluateAttribute(Zenith_StringID xAttribute, float fBase, float fMin, float fMax,
	const Zenith_GameplayEffectsComponent* pxEffects, bool& bNonFiniteOut);
```

For attribute name n with base b, range [lo, hi], and the owner's active-effect records in index order:

1. `fAdd = 0.0f`, `fMul = 1.0f`, `bOverride = false`.
2. For each active record r that is not periodic (`m_uPeriodTicks == 0`), for each modifier k in `[0, r.m_uModifierCount)` with attribute n, in modifier order: `ADD`: `fAdd += magnitude × stacks` (one f32 multiply, one f32 add); `MULTIPLY`: multiply `fMul` by the magnitude `stacks` times in sequence (no `pow`, so the result is the same on every platform's math library); `OVERRIDE`: `fOverride = magnitude`, `bOverride = true`.
3. `v = bOverride ? fOverride : (b + fAdd) × fMul`.
4. If v is not finite: `v = clamp(b, lo, hi)`, `bNonFiniteOut = true` (counted `gameplay.attributes.non_finite`).
5. Return `clamp(v, lo, hi)`.

Periodic effects never enter this formula: each period executes its modifiers on the base (§ "Effect expiry and periods"). Every operation is single-precision in a fixed order on threads with the fixed floating-point environment (`Zenith2/Docs/Contracts.md` → "Threading and concurrency contract", subsection "Floating-point environment"), so the result is bit-identical across runs of one build ([Z2-N-ARCH-881]).

## Effects

### Effect specification

```cpp
/// @contract value Zenith_GameplayEffectSpec
/// Ownership: Plain value: the definition carried by a Gameplay.ApplyEffect request and copied into an active record.
/// Thread affinity: IMMUTABLE; members non-const: a registered payload struct that command recording and playback write
///   field by field.
/// Invalidation: None.
/// Units: Ticks for durations and periods; modifier magnitudes in the attribute's units (ADD, OVERRIDE) or as a factor (MULTIPLY).
/// Errors: None of its own; Zenith_GameplayValidateEffectSpec returns the first invalid field.
/// Version: since Z2 Stage 3 (registered by the API-only slice; behaviour since Stage 4).
/// @endcontract
struct Zenith_GameplayEffectSpec                       // 68 bytes
{
	Zenith_StringID m_xName;                           // 1: non-empty, valid tag text
	uint32 m_uDurationTicks = 0;                      // 2: 0 = instant; uGAMEPLAY_INFINITE_TICKS = until removed
	uint32 m_uPeriodTicks = 0;                        // 3: 0 = not periodic; requires a non-instant duration
	Zenith_StringID m_axModifierAttributes[uGAMEPLAY_MAX_MODIFIERS];   // 4
	float m_afModifierMagnitudes[uGAMEPLAY_MAX_MODIFIERS] = {};        // 5: finite; MULTIPLY ≥ 0
	uint8 m_aeModifierOps[uGAMEPLAY_MAX_MODIFIERS] = {};              // 6: Zenith_GameplayModifierOp
	Zenith_StringID m_axGrantedTags[uGAMEPLAY_MAX_EFFECT_TAGS];        // 7: empty for an instant effect
	Zenith_StringID m_xRequiredTag;                    // 8: application requires a match on the target
	Zenith_StringID m_xBlockedTag;                     // 9: application is blocked by a match on the target
	uint8 m_eStacking = STACKING_POLICY_INDEPENDENT;  // 10
	uint8 m_uMaxStacks = 1;                           // 11: 1 … 255
	uint8 m_uModifierCount = 0;                       // 12: 0 … 4
	uint8 m_uFlags = EFFECT_FLAG_NONE;                // 13: Zenith_GameplayEffectFlags
};
static_assert(sizeof(Zenith_GameplayEffectSpec) == 68);

uint8 Zenith_GameplayValidateEffectSpec(const Zenith_GameplayEffectSpec& xSpec);   // 0 = valid, else the field number
```

Validation fails at the first field (number in the comment) that breaks: the name; a period with an instant duration; a modifier among the first `m_uModifierCount` with an empty or invalid attribute, a non-finite magnitude, a negative `MULTIPLY` magnitude or an op outside 1 … 3; granted tags on an instant effect or an invalid tag text; an invalid required or blocked tag; a stacking policy above 3; `m_uMaxStacks == 0`; `m_uModifierCount > 4`; a flag bit above bit 1.

### Active effect records

```cpp
/// @contract value Zenith_GameplayActiveEffect
/// Ownership: Stored inside Zenith_GameplayEffectsComponent.
/// Thread affinity: IMMUTABLE as a value; members non-const: a registered struct its containing component, payload or
///   result writes field by field; rows follow the AINavigation authority.
/// Invalidation: None.
/// Units: Gameplay ticks (Framework.md "Gameplay time").
/// Errors: None.
/// Version: since Z2 Stage 3 (registered by the API-only slice; behaviour since Stage 4).
/// @endcontract
struct Zenith_GameplayActiveEffect                     // 112 bytes
{
	Zenith_GameplayEffectSpec m_xSpec;                 // 68
	uint8 m_uStacks = 1;                              // 1 … m_xSpec.m_uMaxStacks
	uint8 m_auReserved[3] = {};
	Zenith_EntityHandle m_xSource;                     // null when applied without a source
	uint64 m_ulAppliedTick = 0;
	uint64 m_ulEndTick = 0;                           // 0 = until removed
	uint64 m_ulNextPeriodTick = 0;                    // 0 = not periodic
	uint32 m_uInstanceID = 0;                         // per entity, never 0
	uint32 m_uReserved = 0;
};
static_assert(sizeof(Zenith_GameplayActiveEffect) == 112);

/// @contract value Zenith_GameplayEffectsComponent
/// Ownership: Stored as component "Zenith.Gameplay.Effects".
/// Thread affinity: IMMUTABLE as a value; members non-const: an ECS component the store relocates and systems write
///   property by property; rows follow the AINavigation authority.
/// Invalidation: None.
/// Units: As Zenith_GameplayActiveEffect.
/// Errors: None; a ninth active record is rejected with EFFECT_REJECT_REASON_FULL.
/// Version: since Z2 Stage 3 (registered by the API-only slice; behaviour since Stage 4); type version 1; property IDs 1 to 3 are permanent.
/// @endcontract
struct Zenith_GameplayEffectsComponent                 // 904 bytes; "Zenith.Gameplay.Effects"
{
	Zenith_GameplayActiveEffect m_axEffects[uGAMEPLAY_MAX_EFFECTS];   // 1 Effects, application order
	uint32 m_uNextInstanceID = 1;                                    // 2 NextInstanceID
	uint8 m_uCount = 0;                                              // 3 Count
	uint8 m_auReserved[3] = {};
	static const Zenith_TypeID xTYPE_ID;
};
static_assert(sizeof(Zenith_GameplayEffectsComponent) == 904);
```

### Effect application algorithm

`ApplyEffect(target T, source S, spec x)` at tick t:

1. `Zenith_GameplayValidateEffectSpec(x) != 0` → reject `EFFECT_REJECT_REASON_INVALID_SPEC`.
2. `x.m_xRequiredTag` non-empty and no tag of T matches → `MISSING_TAG`; `x.m_xBlockedTag` non-empty and a tag of T matches → `BLOCKED_TAG` (a target without the tags component matches nothing).
3. `x.m_uModifierCount > 0` and T has no `Zenith.Gameplay.Attributes` → `NO_ATTRIBUTES`.
4. **Instant** (`m_uDurationTicks == 0`): execute each modifier on the base in modifier order (`ADD`: `Base += m`; `MULTIPLY`: `Base *= m`; `OVERRIDE`: `Base = m`; each followed by the clamp to the range; a modifier whose attribute T lacks is skipped and counted `gameplay.effects.unknown_attribute`); mark dirty; emit `EffectApplied { InstanceID = 0, Stacks = 1 }`; done.
5. **Duration**: T has no `Zenith.Gameplay.Effects` → `NO_EFFECTS_COMPONENT`. Find the first active record r with `r.m_xSpec.m_xName == x.m_xName`:
   - `INDEPENDENT`: ignore r.
   - `REFRESH` with r: `r.m_ulEndTick = (x infinite) ? 0 : t + x.m_uDurationTicks`; `r.m_xSource = S`; emit `EffectApplied { InstanceID = r's, Stacks = r's }`; done.
   - `STACK` with r: `r.m_uStacks = min(r.m_xSpec.m_uMaxStacks, r.m_uStacks + 1)`; refresh the end tick as above; mark dirty; emit; done.
   - `REJECT` with r: reject `STACKING`.
6. `m_uCount == uGAMEPLAY_MAX_EFFECTS` → reject `FULL`.
7. Append: `m_xSpec = x`, `m_uStacks = 1`, `m_xSource = S`, `m_ulAppliedTick = t`, `m_ulEndTick = (x infinite) ? 0 : t + x.m_uDurationTicks`, `m_ulNextPeriodTick = x.m_uPeriodTicks ? t + x.m_uPeriodTicks : 0`, `m_uInstanceID = m_uNextInstanceID` then `m_uNextInstanceID += 1` (skipping 0 on wrap).
8. With `EFFECT_FLAG_EXECUTE_PERIOD_ON_APPLY` on a periodic effect: execute one period now (the instant rule of step 4 on the new record's modifiers).
9. Add one reference of each non-empty granted tag; mark the row's attributes dirty; emit `EffectApplied { Source = S, InstanceID, Stacks = 1 }`.

Every rejection emits `EffectRejected { Effect, Reason }` on the target and changes nothing. A target that is stale or pending destroy is skipped before step 1 like every request (`Zenith2/Docs/Design/GAME/Framework.md` → "Rules algorithm" step 2), with no event because no entity remains to carry it.

`RemoveEffect` ops: `BY_NAME(n)` removes every active record named n; `BY_INSTANCE(id)` the one record; `BY_GRANTED_TAG(q)` every record granting a tag matching q. Each removal: one reference of each granted tag released, the record removed (shift left), dirty marked, `EffectRemoved { Reason = REMOVED }` emitted, in record order.

### Effect stacking

| Policy | A second application of a name while one is active | Modifier contribution |
|---|---|---|
| `STACKING_POLICY_INDEPENDENT` | a new record (until the component holds eight) | each record contributes separately |
| `STACKING_POLICY_REFRESH` | the record's end tick and source refresh; stacks stay 1 | once |
| `STACKING_POLICY_STACK` | stacks + 1 up to the maximum; end tick refreshes | `ADD`: magnitude × stacks; `MULTIPLY`: magnitude applied stacks times; `OVERRIDE`: unaffected by stacks; a periodic execution applies each modifier stacks times |
| `STACKING_POLICY_REJECT` | rejected `EFFECT_REJECT_REASON_STACKING` | once |

### Effect expiry and periods

The sweep of `Zenith2/Docs/Design/GAME/Framework.md` → "Rules algorithm" step 4, per row, per record in index order (a removal shifts the following records left, so the index is not advanced after one), at tick t:

1. **Source.** With `EFFECT_FLAG_REMOVE_WITH_SOURCE`: if `m_xSource` is non-null and resolves stale or pending destroy, remove the record with `EFFECT_REMOVE_REASON_SOURCE_DESTROYED`.
2. **Periods.** While `m_ulNextPeriodTick != 0 && m_ulNextPeriodTick ≤ t && (m_ulEndTick == 0 || m_ulNextPeriodTick ≤ m_ulEndTick)` and fewer than `uGAMEPLAY_MAX_TIMER_FIRES_PER_TICK` executions happened for this record this tick: execute one period (each modifier on the base, stacks times, clamped), mark dirty, `m_ulNextPeriodTick += m_uPeriodTicks`. A period due exactly at the end tick executes before the expiry of step 3. Execution emits no event of its own; the resulting `AttributeChanged` of step 7 of the rules algorithm reports it.
3. **Expiry.** `m_ulEndTick != 0 && m_ulEndTick ≤ t`: remove with `EFFECT_REMOVE_REASON_EXPIRED`.

A carried-over effect (world transition) keeps its relative timing: its stored ticks are gameplay ticks, which continue across the swap (`Zenith2/Docs/Design/GAME/Framework.md` → "Gameplay time"; `Zenith2/Docs/Design/GAME/WorldHost.md` → "Carry-over"), so an effect with 30 ticks left has 30 ticks left in the new world.

## Ordering within a tick

Within one rules pass, everything that happens to abilities, attributes and effects follows the step order of `Zenith2/Docs/Design/GAME/Framework.md` → "Rules algorithm": requests in queue order (step 2) — so an `ApplyEffect` recorded after an `ACTIVATE` in merge order is applied after the activation's cost is paid — then effect expiry and periods (4), then ability expiry, blocking and cooldowns (5), then the attribute re-evaluation (7) of every dirty row, whose `AttributeChanged` events carry the value before the pass and the value after it. A Late behaviour therefore sees one coherent post-pass state in phase 10, and an Early behaviour of the next tick sees the same.
